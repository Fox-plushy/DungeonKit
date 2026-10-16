#include "UE4.h"

#include <algorithm>
#include <set>
#include <cstdio>
#include <cstring>

namespace dk {

namespace {

// Where the instruction that references the global sits relative to the pattern match, and its opcode.
struct RipRef { const char* pattern; int insOffset; int insLen; uint8_t op[3]; };

const RipRef kEngineRefs[] = {
    {"41 B8 01 00 00 00 ** ** ** 48 8B 0D ** ** ** ** E8 ** ** ** ** 48 85 C0", 9, 7, {0x48, 0x8B, 0x0D}},
    {"48 8B 1D ** ** ** ** 48 85 DB 74 ** 48 8D",                               0, 7, {0x48, 0x8B, 0x1D}},
};
const RipRef kModernNameRefs[] = {
    {"48 8D 0D ** ** ** ** E8 ** ** ** ** 4C 8B C0 C6", 0, 7, {0x48, 0x8D, 0x0D}},
};
const RipRef kOldNameRefs[] = {
    {"48 83 EC 28 48 8B 05 ** ** ** ** 48 85 C0 75 ** B9 ** ** 00 00 48 89 5C 24 20 E8", 4, 7, {0x48, 0x8B, 0x05}},
    {"C3 ** DB 48 89 1D ** ** ** ** ** ** 48 8B 5C 24 20",                               3, 7, {0x48, 0x89, 0x1D}},
};

bool ripTargets(const Process& p, const RipRef& r, std::vector<uint64_t>& out) {
    Pattern pat = parsePattern(r.pattern);
    if (!pat.valid) return false;
    for (uint64_t m : p.scan(pat, 24)) {
        uint8_t b[16];
        const uint64_t at = m + r.insOffset;
        if (!p.read(at, b, r.insLen)) continue;
        if (b[0] != r.op[0] || b[1] != r.op[1] || b[2] != r.op[2]) continue;
        int32_t disp;
        std::memcpy(&disp, b + r.insLen - 4, 4);
        out.push_back(at + r.insLen + (int64_t)disp);
    }
    return !out.empty();
}

std::string hex(uint64_t v) {
    char b[24];
    std::snprintf(b, sizeof b, "0x%llX", (unsigned long long)v);
    return b;
}

bool endsWith(const std::string& s, const char* suf) {
    const size_t n = std::strlen(suf);
    return s.size() >= n && s.compare(s.size() - n, n, suf) == 0;
}

} // namespace

void UE4::reset() {
    CsGuard g(mu_);
    ready_ = false;
    haveObjArray_ = false;
    names_.clear();
    propCache_.clear();
    namesVar_ = engineVar_ = 0;
    diag_.clear();
    pid_ = 0;
}

std::string UE4::diagnostics() const {
    CsGuard g(mu_);
    return diag_;
}

bool UE4::readText(uint64_t addr, bool wide, size_t maxChars, std::string& out) const {
    out.clear();
    for (size_t chunk : {size_t(128), size_t(64), size_t(32), size_t(16), size_t(8), size_t(4)}) {
        const size_t n = std::min(chunk, maxChars);
        std::vector<uint8_t> buf(n * (wide ? 2 : 1));
        if (!p_->read(addr, buf.data(), buf.size())) continue;
        for (size_t i = 0; i < n; ++i) {
            unsigned c = wide ? (buf[i * 2] | (buf[i * 2 + 1] << 8)) : buf[i];
            if (c == 0) return !out.empty();
            if (c < 0x20 || c > 0x7E) return false;      // names are plain ASCII; anything else means a wrong address
            out.push_back((char)c);
        }
        return true;
    }
    return false;
}

bool UE4::readNameEntry(uint32_t index, std::string& out) {
    if (modernNames_) {
        // FNamePool: Blocks[] at +0x10, each entry 2-byte aligned: u16 header (len<<6 | wide), then characters.
        const uint64_t block = rd64(namesVar_ + 0x10 + (uint64_t)(index >> 16) * 8);
        if (!plausible(block)) return false;
        uint16_t header = 0;
        const uint64_t entry = block + (uint64_t)(index & 0xFFFF) * 2;
        if (!rd(entry, header)) return false;
        const size_t len = header >> 6;
        if (len == 0 || len > 1024) return false;
        return readText(entry + 2, header & 1, std::min<size_t>(len, 200), out);
    }
    const uint64_t arr = rd64(namesVar_);
    if (!plausible(arr)) return false;
    const uint64_t chunk = rd64(arr + (uint64_t)(index >> 14) * 8);
    if (!plausible(chunk)) return false;
    const uint64_t entry = rd64(chunk + (uint64_t)(index & 0x3FFF) * 8);
    if (!plausible(entry)) return false;
    uint32_t idField = 0;
    if (!rd(entry, idField)) return false;
    return readText(entry + strOff_, idField & 1, 200, out);
}

std::string UE4::nameOf(uint32_t index) {
    auto it = names_.find(index);
    if (it != names_.end()) return it->second;
    std::string s;
    if (!readNameEntry(index, s)) s.clear();
    names_[index] = s;
    return s;
}

std::string UE4::objName(uint64_t obj) {
    uint32_t idx = 0;
    if (!rd(obj + 0x18, idx)) return {};
    return nameOf(idx);
}

std::string UE4::propName(uint64_t field) {
    uint32_t idx = 0;
    if (!rd(field + lay_.nameOff, idx)) return {};
    return nameOf(idx);
}

std::string UE4::propClassName(uint64_t field) {
    if (!lay_.fField) return objName(rd64(field + 0x10));
    const uint64_t fc = rd64(field + 8);                 // FFieldClass: FName Name comes first
    uint32_t idx = 0;
    if (!plausible(fc) || !rd(fc, idx)) return {};
    return nameOf(idx);
}

std::string UE4::className(uint64_t obj) {
    CsGuard g(mu_);
    const uint64_t cls = rd64(obj + 0x10);
    return plausible(cls) ? objName(cls) : std::string();
}

bool UE4::findLocked(uint64_t cls, const std::string& name, Prop& out) {
    auto key = std::make_pair(cls, name);
    auto it = propCache_.find(key);
    if (it != propCache_.end()) { out = it->second.second; return it->second.first; }

    bool found = false;
    Prop pr;
    uint64_t c = cls;
    for (int depth = 0; depth < 24 && plausible(c) && !found; ++depth) {
        uint64_t f = rd64(c + lay_.member);
        for (int n = 0; n < 4096 && plausible(f); ++n, f = rd64(f + lay_.next)) {
            if (propName(f) != name) continue;
            if (!endsWith(propClassName(f), "Property")) continue;     // skip functions with the same name
            uint32_t off = 0;
            if (!rd(f + lay_.offset, off) || off > 0x100000) continue;
            pr = Prop();
            pr.offset = off;
            if (propClassName(f) == "BoolProperty") {
                uint8_t byteOff = 0, mask = 0;
                rd(f + lay_.mask - 1, byteOff);
                rd(f + lay_.mask, mask);
                pr.isBool = true;
                pr.byteOffset = byteOff < 8 ? byteOff : 0;
                pr.mask = mask;
            }
            found = true;
            break;
        }
        c = rd64(c + 0x40);                                              // SuperStruct
    }
    propCache_[key] = {found, pr};
    out = pr;
    return found;
}

std::string UE4::nameAt(uint32_t index) { CsGuard g(mu_); return nameOf(index); }
std::string UE4::objectName(uint64_t obj) { CsGuard g(mu_); return objName(obj); }
std::string UE4::propClassOf(uint64_t prop) { CsGuard g(mu_); return propClassName(prop); }

bool UE4::listFields(uint64_t st, std::vector<Field>& out, bool ownOnly) {
    out.clear();
    if (!ready_) return false;
    CsGuard g(mu_);
    uint64_t c = st;
    for (int depth = 0; depth < (ownOnly ? 1 : 24) && plausible(c); ++depth, c = rd64(c + 0x40)) {
        uint64_t f = rd64(c + lay_.member);
        for (int n = 0; n < 2048 && plausible(f); ++n, f = rd64(f + lay_.next)) {
            Field fl;
            fl.type = propClassName(f);
            if (!endsWith(fl.type, "Property")) continue;
            fl.name = propName(f);
            fl.prop = f;
            rd(f + lay_.offset, fl.offset);
            rd(f + lay_.offset - 0x10, fl.elemSize);          // ElementSize / ArrayDim sit just before Offset_Internal in every layout
            rd(f + lay_.offset - 0x14, fl.dim);
            if (fl.offset > 0x100000 || fl.dim == 0 || fl.dim > 4096) continue;
            out.push_back(fl);
            if (out.size() >= 600) return true;
        }
    }
    return true;
}

std::string UE4::enumeratorName(uint64_t en, int64_t value) {
    if (!plausible(en)) return {};
    CsGuard g(mu_);
    const uint64_t data = rd64(en + 0x40);
    int32_t num = 0;
    rd(en + 0x48, num);
    if (!plausible(data) || num < 1 || num > 4096) return {};
    for (int32_t i = 0; i < num; ++i) {
        int64_t v = 0;
        uint32_t idx = 0;
        if (!rd(data + (uint64_t)i * 16, idx) || !rd(data + (uint64_t)i * 16 + 8, v)) return {};
        if (v == value) {
            std::string s = nameOf(idx);
            const size_t p = s.rfind("::");
            return p == std::string::npos ? s : s.substr(p + 2);
        }
    }
    return {};
}

bool UE4::validateObjArray(uint64_t cand, ObjArray& out) {
    // UE 4.22 FUObjectArray: ObjObjects (FChunkedFixedUObjectArray) at +0x10: Objects**, PreAllocated, MaxElements, NumElements, MaxChunks, NumChunks
    uint64_t chunks = rd64(cand + 0x10);
    int32_t maxEl = 0, numEl = 0, maxCh = 0, numCh = 0;
    rd(cand + 0x20, maxEl); rd(cand + 0x24, numEl); rd(cand + 0x28, maxCh); rd(cand + 0x2C, numCh);
    if (!plausible(chunks) || numEl < 1000 || numEl > 10000000 || maxEl < numEl || maxEl > 12000000) return false;
    if (numCh < 1 || maxCh < numCh || maxCh > 512 || maxEl % maxCh != 0) return false;
    const int32_t per = maxEl / maxCh;
    if (per < 0x1000 || per > 0x20000 || (int64_t)numCh * per < numEl) return false;
    const uint64_t first = rd64(chunks), last = rd64(chunks + (uint64_t)(numCh - 1) * 8);
    if (!plausible(first) || !plausible(last)) return false;
    int valid = 0;
    for (int i = 0; i < 128 && i < numEl && i < per; ++i) {
        const uint64_t o = rd64(first + (uint64_t)i * 0x18);
        if (plausible(o) && plausible(rd64(o + 0x10))) ++valid;
    }
    if (valid < 8) return false;
    out.chunks = chunks; out.num = numEl; out.perChunk = per; out.numChunks = numCh;
    return true;
}

bool UE4::scanForObjArray(ObjArray& out) {
    // Reads the module in chunks and tests every 8-byte aligned spot against the shape of the object table header.
    const uint64_t base = p_->base(), size = p_->moduleSize();
    const size_t chunk = 1 << 20, overlap = 0x40;
    std::vector<uint8_t> buf(chunk + overlap);
    for (uint64_t off = 0; off < size; off += chunk) {
        const size_t n = (size_t)std::min<uint64_t>(chunk + overlap, size - off);
        if (!p_->read(base + off, buf.data(), n)) continue;
        for (size_t i = 0; i + 0x30 <= n && i < chunk; i += 8) {
            uint64_t chunks; int32_t maxEl, numEl, maxCh, numCh;
            std::memcpy(&chunks, &buf[i + 0x10], 8);
            std::memcpy(&maxEl, &buf[i + 0x20], 4);
            std::memcpy(&numEl, &buf[i + 0x24], 4);
            std::memcpy(&maxCh, &buf[i + 0x28], 4);
            std::memcpy(&numCh, &buf[i + 0x2C], 4);
            if (numEl < 1000 || numEl > 10000000 || maxEl < numEl || maxEl > 12000000) continue;
            if (numCh < 1 || maxCh < numCh || maxCh > 512 || maxEl % maxCh != 0) continue;
            if (!plausible(chunks)) continue;
            ObjArray oa;
            if (validateObjArray(base + off + i, oa)) { out = oa; return true; }
        }
    }
    return false;
}

bool UE4::objectArray(ObjArray& out, std::string& err) {
    if (!ready_) { err = "Engine data not found yet."; return false; }
    {
        CsGuard g(mu_);
        if (haveObjArray_) { out = objArray_; return true; }
    }
    static const char* kPatterns[] = {
        "44 8B ** ** ** 48 8D 05 ** ** ** ** ** ** ** ** ** 48 89 71 10",
        "40 53 48 83 EC 20 48 8B D9 48 85 D2 74 ** 8B",
        "4C 8B 05 ** ** ** ** 45 3B 88",
    };
    CsGuard g(mu_);
    for (const char* ps : kPatterns) {
        Pattern pat = parsePattern(ps);
        if (!pat.valid) continue;
        for (uint64_t m : p_->scan(pat, 16)) {
            uint8_t b[0x48];
            if (!p_->read(m, b, sizeof b)) continue;
            // Any RIP-relative operand near the match is a candidate; the strict layout check weeds out the wrong ones.
            for (int k = 1; k < 0x38; ++k) {
                if ((b[k] & 0xC7) != 0x05) continue;
                int32_t disp;
                std::memcpy(&disp, b + k + 1, 4);
                for (int imm : {0, 1, 4}) {
                    const uint64_t cand = m + k + 5 + imm + (int64_t)disp;
                    ObjArray oa;
                    if (validateObjArray(cand, oa)) { objArray_ = oa; haveObjArray_ = true; out = oa; return true; }
                }
            }
        }
    }
    if (scanForObjArray(objArray_)) { haveObjArray_ = true; out = objArray_; return true; }
    err = "Could not find the object table in this game build.";
    return false;
}

uint64_t UE4::objectAt(const ObjArray& a, int32_t index) {
    if (index < 0 || index >= a.num) return 0;
    const uint64_t chunk = rd64(a.chunks + (uint64_t)(index / a.perChunk) * 8);
    if (!plausible(chunk)) return 0;
    return rd64(chunk + (uint64_t)(index % a.perChunk) * 0x18);
}

bool UE4::find(uint64_t obj, const char* name, Prop& out) {
    if (!ready_ || !plausible(obj)) return false;
    CsGuard g(mu_);
    const uint64_t cls = rd64(obj + 0x10);
    if (!plausible(cls)) return false;
    return findLocked(cls, name, out);
}

bool UE4::readPtr(uint64_t obj, const char* prop, uint64_t& out) {
    Prop pr;
    if (!find(obj, prop, pr)) return false;
    out = rd64(obj + pr.offset);
    return plausible(out);
}

bool UE4::readArray(uint64_t obj, const char* prop, uint64_t& data, int32_t& num) {
    Prop pr;
    if (!find(obj, prop, pr)) return false;
    data = rd64(obj + pr.offset);
    num = 0;
    rd(obj + pr.offset + 8, num);
    return plausible(data) && num > 0 && num < 4096;
}

bool UE4::character(uint64_t& ch) {
    if (!ready_) return false;
    uint64_t eng = rd64(engineVar_), gi = 0, players = 0, lp = 0, pc = 0;
    int32_t num = 0;
    if (!plausible(eng)) return false;
    if (!readPtr(eng, "GameInstance", gi)) return false;
    if (!readArray(gi, "LocalPlayers", players, num)) return false;
    lp = rd64(players);
    if (!plausible(lp)) return false;
    if (!readPtr(lp, "PlayerController", pc)) return false;
    if (readPtr(pc, "Character", ch)) return true;
    if (readPtr(pc, "AcknowledgedPawn", ch)) return true;
    return readPtr(pc, "Pawn", ch);
}

// Works out the property-table layout of an engine build we have no preset for. The engine object is the test case:
// UEngine has a pointer property "GameInstance", so the right layout is the one where walking the class's
// property list finds that name AND the offset it reports points at an object whose class looks like a GameInstance.
bool UE4::discoverLayout(uint64_t eng, uint64_t cls, std::string& log) {
    const uint32_t nexts[] = {0x18, 0x20, 0x28, 0x30, 0x38};
    const uint32_t nameOffs[] = {0x18, 0x20, 0x28, 0x30, 0x38};
    int tried = 0, sawName = 0;
    for (int fField = 1; fField >= 0; --fField)
    for (uint32_t member = 0x38; member <= 0x90; member += 8)
    for (uint32_t next : nexts)
    for (uint32_t nameOff : nameOffs) {
        if (next == nameOff) continue;
        Layout l{member, next, 0x4C, 0x7A, nameOff, fField != 0, "auto"};
        lay_ = l;
        ++tried;
        uint64_t c = cls;
        for (int depth = 0; depth < 10 && plausible(c); ++depth, c = rd64(c + 0x40)) {
            uint64_t f = rd64(c + member);
            for (int n = 0; n < 1024 && plausible(f); ++n, f = rd64(f + next)) {
                if (propName(f) != "GameInstance") continue;
                ++sawName;
                for (uint32_t po = 0x28; po <= 0xA0; po += 4) {                    // where does the property keep its Offset_Internal?
                    uint32_t off = 0;
                    if (!rd(f + po, off) || off < 0x8 || off > 0x10000) continue;
                    const uint64_t target = rd64(eng + off);
                    if (!plausible(target)) continue;
                    if (className(target).find("Instance") == std::string::npos) continue;      // GameInstance, or the game's own subclass of it
                    lay_ = Layout{member, next, po, po + 0x2E, nameOff, fField != 0, "auto-detected"};
                    char b[160];
                    std::snprintf(b, sizeof b, "Layout found automatically: members +0x%X, next +0x%X, name +0x%X, offset +0x%X, %s\n",
                                  member, next, nameOff, po, fField ? "FField" : "UProperty");
                    log += b;
                    return true;
                }
            }
        }
    }
    log += "Automatic layout search: " + std::to_string(tried) + " layouts tried, property name seen " + std::to_string(sawName) + " time(s), no usable offset.\n";
    return false;
}

bool UE4::init(const Process& p, std::string& err) {
    if (ready_ && pid_ == p.pid()) return true;
    reset();
    p_ = &p;
    pid_ = p.pid();

    std::string log;
    std::vector<uint64_t> engines, modern, old;
    for (auto& r : kEngineRefs) ripTargets(p, r, engines);
    for (auto& r : kModernNameRefs) ripTargets(p, r, modern);
    for (auto& r : kOldNameRefs) ripTargets(p, r, old);
    log += "GEngine candidates: " + std::to_string(engines.size()) + ", name table candidates: " +
           std::to_string(modern.size()) + " modern / " + std::to_string(old.size()) + " classic\n";
    if (engines.empty() || (modern.empty() && old.empty())) {
        err = "Could not find the engine globals in this game build.";
        CsGuard g(mu_);
        diag_ = log + err;
        return false;
    }

    struct NameCand { uint64_t var; bool modern; };
    std::vector<NameCand> names;
    for (uint64_t v : modern) names.push_back({v, true});
    for (uint64_t v : old) names.push_back({v, false});

    static const Layout layouts[] = {
        {0x48, 0x28, 0x44, 0x72, 0x18, false, "UE 4.22-4.24"},
        {0x50, 0x20, 0x4C, 0x7A, 0x28, true,  "UE 4.25+"},
    };

    CsGuard g(mu_);
    std::set<std::string> seenClasses;
    std::set<uint64_t> doneNames;
    bool triedAuto = false;
    for (const NameCand& nc : names) {
        if (!doneNames.insert(nc.var).second) continue;      // same table reached from several places
        modernNames_ = nc.modern;
        namesVar_ = nc.var;
        names_.clear();
        bool nameOk = false;
        if (modernNames_) {
            std::string s;
            nameOk = readNameEntry(0, s) && s == "None";
        } else {
            const uint64_t arr = rd64(namesVar_);
            const uint64_t chunk = plausible(arr) ? rd64(arr) : 0;
            const uint64_t entry = plausible(chunk) ? rd64(chunk) : 0;
            if (plausible(entry)) {
                for (uint32_t off : {0x10u, 0xCu, 0x8u}) {
                    strOff_ = off;
                    std::string s;
                    if (readNameEntry(0, s) && s == "None") { nameOk = true; break; }
                }
            }
        }
        if (!nameOk) continue;
        log += "Names: " + hex(namesVar_) + (modernNames_ ? " (FNamePool)" : " (GNames)") + "\n";

        for (uint64_t ev : engines) {
            const uint64_t eng = rd64(ev);
            if (!plausible(eng)) continue;
            const uint64_t cls = rd64(eng + 0x10);
            if (!plausible(cls)) continue;
            const std::string cn = objName(cls);
            if (seenClasses.insert(cn).second && seenClasses.size() <= 8) log += "Engine candidate class: " + cn + "\n";
            if (cn.find("Engine") == std::string::npos) continue;
            for (const Layout& l : layouts) {
                lay_ = l;
                propCache_.clear();
                Prop pr;
                if (findLocked(cls, "GameInstance", pr)) {
                    engineVar_ = ev;
                    ready_ = true;
                    log += "GEngine: " + hex(ev) + " -> " + cn + ", layout " + l.label + "\n";
                    diag_ = log;
                    return true;
                }
            }
            if (!triedAuto) {
                triedAuto = true;
                if (discoverLayout(eng, cls, log)) {
                    propCache_.clear();
                    Prop pr;
                    if (findLocked(cls, "GameInstance", pr)) {
                        engineVar_ = ev;
                        ready_ = true;
                        log += "GEngine: " + hex(ev) + " -> " + cn + ", layout auto-detected\n";
                        diag_ = log;
                        return true;
                    }
                }
            }
        }
    }
    err = "Found the engine data but could not read its property tables (unsupported engine layout).";
    diag_ = log + err;
    propCache_.clear();
    return false;
}

} // namespace dk
