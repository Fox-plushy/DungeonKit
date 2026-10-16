#include "UE4Dump.h"

#include <cstdio>
#include <ctime>
#include <vector>

namespace dk {

namespace {
struct File {
    FILE* f = nullptr;
    explicit File(const std::wstring& p) { f = _wfopen(p.c_str(), L"w"); }
    ~File() { if (f) std::fclose(f); }
};

void header(FILE* f, UE4& ue, const char* what) {
    const std::time_t t = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    std::fprintf(f, "DungeonKit %s\nProcess id: %lu\nTimeStamp: %s\n\n", what,
                 ue.proc() ? (unsigned long)ue.proc()->pid() : 0ul, buf);
}
}

bool dumpNames(UE4& ue, const std::wstring& path, std::atomic<int>& count, std::string& err) {
    if (!ue.ready()) { err = "Engine data not found yet."; return false; }
    File out(path);
    if (!out.f) { err = "Could not create the file."; return false; }
    header(out.f, ue, "names dump");
    count = 0;
    const uint64_t var = ue.namesVar();
    if (ue.modernNames()) {
        for (uint32_t block = 0; block < 512; ++block) {
            const uint64_t base = ue.rd64(var + 0x10 + (uint64_t)block * 8);
            if (!UE4::plausible(base)) break;
            uint32_t off = 0;
            while (off < 0x20000) {
                uint16_t hd = 0;
                if (!ue.rd(base + off, hd)) break;
                const uint32_t len = hd >> 6;
                if (len == 0) break;
                const uint32_t idx = (block << 16) | (off / 2);
                const std::string s = ue.nameAt(idx);
                if (!s.empty()) { std::fprintf(out.f, "[%08X] %s\n", idx, s.c_str()); ++count; }
                off += (2 + len * ((hd & 1) ? 2 : 1) + 1) & ~1u;
            }
        }
    } else {
        const uint64_t arr = ue.rd64(var);
        for (uint32_t chunk = 0; chunk < 128 && UE4::plausible(arr); ++chunk) {
            const uint64_t cp = ue.rd64(arr + (uint64_t)chunk * 8);
            if (!UE4::plausible(cp)) break;
            for (uint32_t slot = 0; slot < 0x4000; ++slot) {
                const uint32_t idx = chunk * 0x4000 + slot;
                const std::string s = ue.nameAt(idx);
                if (!s.empty()) { std::fprintf(out.f, "[%08X] %s\n", idx, s.c_str()); ++count; }
            }
        }
    }
    return true;
}

bool scanSummonable(UE4& ue, std::vector<std::string>& out, std::atomic<int>& progress, std::string& err) {
    UE4::ObjArray oa;
    if (!ue.objectArray(oa, err)) return false;
    progress = 0;
    auto starts = [](const std::string& s, const char* p) { return s.rfind(p, 0) == 0; };
    for (int32_t i = 0; i < oa.num; ++i) {
        if ((i & 0x3FFF) == 0) progress = (int)((int64_t)i * 100 / (oa.num ? oa.num : 1));
        const uint64_t o = ue.objectAt(oa, i);
        if (!UE4::plausible(o)) continue;
        if (ue.className(o) != "BlueprintGeneratedClass") continue;
        const std::string name = ue.objectName(o);
        if (name.size() < 3 || name.compare(name.size() - 2, 2, "_C") != 0) continue;
        if (starts(name, "SKEL_") || starts(name, "REINST_") || starts(name, "HOTRELOADED_") || starts(name, "Default__")) continue;
        bool isActor = false;
        uint64_t c = ue.rd64(o + 0x40);                       // SuperStruct, walked up to the base class
        for (int d = 0; d < 32 && UE4::plausible(c); ++d, c = ue.rd64(c + 0x40))
            if (ue.objectName(c) == "Actor") { isActor = true; break; }
        if (isActor) out.push_back(name);
    }
    progress = 100;
    return true;
}

bool dumpObjects(UE4& ue, const std::wstring& path, std::atomic<int>& count, std::string& err) {
    UE4::ObjArray oa;
    if (!ue.objectArray(oa, err)) return false;
    File out(path);
    if (!out.f) { err = "Could not create the file."; return false; }
    header(out.f, ue, "object dump");
    count = 0;
    for (int32_t i = 0; i < oa.num; ++i) {
        const uint64_t o = ue.objectAt(oa, i);
        if (!UE4::plausible(o)) continue;
        const std::string cls = ue.className(o);
        if (cls.empty()) continue;
        std::string name = ue.objectName(o);
        if (name.empty()) continue;
        // owner chain: Outer.Outer.Name
        uint64_t outer = ue.rd64(o + 0x20);
        for (int d = 0; d < 8 && UE4::plausible(outer); ++d, outer = ue.rd64(outer + 0x20)) {
            const std::string on = ue.objectName(outer);
            if (on.empty()) break;
            name = on + "." + name;
        }
        const bool isProp = cls.size() > 8 && cls.compare(cls.size() - 8, 8, "Property") == 0;
        if (isProp) {                                              // older engines only: properties are objects there
            uint32_t off = 0;
            ue.rd(o + 0x44, off);
            std::fprintf(out.f, "[%8X] %016llX %s %s\n", off, (unsigned long long)o, cls.c_str(), name.c_str());
        } else {
            std::fprintf(out.f, "[%08d] %016llX %s %s\n", i, (unsigned long long)o, cls.c_str(), name.c_str());
        }
        if (cls == "Enum" || cls == "UserDefinedEnum") {
            for (uint32_t eo : {0x40u, 0x48u, 0x50u}) {              // where the name/value list sits depends on the engine version
                const uint64_t data = ue.rd64(o + eo);
                int32_t num = 0;
                ue.rd(o + eo + 8, num);
                uint32_t first = 0;
                if (!UE4::plausible(data) || num < 1 || num > 4096 || !ue.rd(data, first) || ue.nameAt(first).find("::") == std::string::npos) continue;
                for (int32_t k = 0; k < num; ++k) {
                    uint32_t idx = 0;
                    int64_t v = 0;
                    if (!ue.rd(data + (uint64_t)k * 16, idx) || !ue.rd(data + (uint64_t)k * 16 + 8, v)) break;
                    std::fprintf(out.f, "[%8llX] %016llX %s\n", (unsigned long long)v,
                                 (unsigned long long)(data + (uint64_t)k * 16), ue.nameAt(idx).c_str());
                }
                break;
            }
        }
        ++count;
    }
    return true;
}

bool dumpClasses(UE4& ue, const std::wstring& path, std::atomic<int>& count, std::string& err) {
    UE4::ObjArray oa;
    if (!ue.objectArray(oa, err)) return false;
    File out(path);
    if (!out.f) { err = "Could not create the file."; return false; }
    header(out.f, ue, "class and field dump");
    count = 0;
    for (int32_t i = 0; i < oa.num; ++i) {
        const uint64_t o = ue.objectAt(oa, i);
        if (!UE4::plausible(o)) continue;
        const std::string cls = ue.className(o);
        const bool isStruct = cls == "ScriptStruct" || cls == "UserDefinedStruct" ||
                              (cls.size() >= 5 && cls.compare(cls.size() - 5, 5, "Class") == 0);
        if (!isStruct) continue;
        std::string name = ue.objectName(o);
        if (name.empty()) continue;
        uint64_t outer = ue.rd64(o + 0x20);
        for (int d = 0; d < 8 && UE4::plausible(outer); ++d, outer = ue.rd64(outer + 0x20)) {
            const std::string on = ue.objectName(outer);
            if (on.empty()) break;
            name = on + "." + name;
        }
        std::vector<UE4::Field> fields;
        ue.listFields(o, fields, true);
        const uint64_t sup = ue.rd64(o + 0x40);
        std::fprintf(out.f, "\n%s %s%s%s  (%d fields)\n", cls.c_str(), name.c_str(),
                     UE4::plausible(sup) ? " : " : "", UE4::plausible(sup) ? ue.objectName(sup).c_str() : "", (int)fields.size());
        for (const auto& f : fields) {
            if (f.dim > 1) std::fprintf(out.f, "  +%04X %s %s[%u]\n", f.offset, f.type.c_str(), f.name.c_str(), f.dim);
            else           std::fprintf(out.f, "  +%04X %s %s\n", f.offset, f.type.c_str(), f.name.c_str());
        }
        ++count;
    }
    return true;
}

} // namespace dk
