#include "ItemViewer.h"

#include <cstdio>

namespace dk {

namespace {
std::string fmt(const char* f, double v) {
    char b[48];
    std::snprintf(b, sizeof b, f, v);
    return b;
}
std::string i64(int64_t v) { return std::to_string((long long)v); }
std::string u64(uint64_t v) { return std::to_string((unsigned long long)v); }
}

bool ItemViewerMod::onEnable(const Process& p, std::string& err) {
    if (!ue_->init(p, err)) return false;
    if (!ue_->classic()) { err = "This game build uses a property layout the viewer doesn't support yet."; return false; }
    uint64_t ch = 0;
    if (!ue_->character(ch)) { err = "Can't find your character. Load into a level first (not a menu)."; return false; }
    return true;
}

std::string ItemViewerMod::summary(uint64_t obj) {
    if (!UE4::plausible(obj)) return "None";
    std::string c = ue_->className(obj), n = ue_->objectName(obj);
    return c.empty() ? n : (n.empty() ? c : c + "  " + n);
}

void ItemViewerMod::addStruct(ViewNode& into, uint64_t st, uint64_t base, int depth) {
    std::vector<UE4::Field> fields;
    if (!ue_->listFields(st, fields)) return;
    for (const auto& f : fields) {
        if (budget_ <= 0) return;
        if (f.dim > 1) {
            ViewNode arr;
            arr.label = f.name;
            arr.value = "[" + std::to_string(f.dim) + "]";
            for (uint32_t i = 0; i < f.dim && i < 16; ++i)
                arr.kids.push_back(value(base + f.offset + (uint64_t)i * f.elemSize, f.type, f.prop, "[" + std::to_string(i) + "]", depth));
            into.kids.push_back(std::move(arr));
        } else {
            into.kids.push_back(value(base + f.offset, f.type, f.prop, f.name, depth));
        }
    }
}

ViewNode ItemViewerMod::value(uint64_t a, const std::string& type, uint64_t prop, const std::string& label, int depth) {
    --budget_;
    ViewNode n;
    n.label = label;
    const UE4& ue = *ue_;
    auto bytesAt = [&](uint32_t size) -> int64_t {
        switch (size) {
        case 1: { int8_t v = 0; ue.rd(a, v); return v; }
        case 2: { int16_t v = 0; ue.rd(a, v); return v; }
        case 8: { int64_t v = 0; ue.rd(a, v); return v; }
        default: { int32_t v = 0; ue.rd(a, v); return v; }
        }
    };

    if (type == "FloatProperty") { float v = 0; n.value = ue.rd(a, v) ? fmt("%.5g", v) : "?"; }
    else if (type == "DoubleProperty") { double v = 0; n.value = ue.rd(a, v) ? fmt("%.6g", v) : "?"; }
    else if (type == "IntProperty") { int32_t v = 0; n.value = ue.rd(a, v) ? i64(v) : "?"; }
    else if (type == "Int8Property") n.value = i64((int8_t)bytesAt(1));
    else if (type == "Int16Property") n.value = i64((int16_t)bytesAt(2));
    else if (type == "UInt16Property") { uint16_t v = 0; ue.rd(a, v); n.value = u64(v); }
    else if (type == "UInt32Property") { uint32_t v = 0; ue.rd(a, v); n.value = u64(v); }
    else if (type == "Int64Property") n.value = i64(bytesAt(8));
    else if (type == "UInt64Property") { uint64_t v = 0; ue.rd(a, v); n.value = u64(v); }
    else if (type == "BoolProperty") {
        uint8_t byteOff = 0, mask = 0, b = 0;
        ue.rd(prop + 0x71, byteOff);
        ue.rd(prop + 0x72, mask);
        if (byteOff < 8 && mask && ue.rd(a + byteOff, b)) n.value = (b & mask) ? "Yes" : "No";
        else n.value = "?";
    }
    else if (type == "ByteProperty") {
        uint8_t b = 0;
        ue.rd(a, b);
        const uint64_t en = ue.rd64(prop + 0x70);
        std::string s = UE4::plausible(en) ? ue_->enumeratorName(en, b) : std::string();
        n.value = s.empty() ? u64(b) : s + "  (" + u64(b) + ")";
    }
    else if (type == "EnumProperty") {
        const uint64_t under = ue.rd64(prop + 0x70), en = ue.rd64(prop + 0x78);
        uint32_t sz = 1;
        if (UE4::plausible(under)) ue.rd(under + 0x34, sz);
        const int64_t v = bytesAt(sz == 2 || sz == 8 ? sz : (sz == 4 ? 4 : 1));
        std::string s = UE4::plausible(en) ? ue_->enumeratorName(en, v) : std::string();
        n.value = s.empty() ? i64(v) : s + "  (" + i64(v) + ")";
    }
    else if (type == "NameProperty") { uint32_t idx = 0; ue.rd(a, idx); n.value = ue_->nameAt(idx); if (n.value.empty()) n.value = "None"; }
    else if (type == "StrProperty") {
        const uint64_t data = ue.rd64(a);
        int32_t num = 0;
        ue.rd(a + 8, num);
        std::string s;
        for (int32_t i = 0; UE4::plausible(data) && i < num - 1 && i < 160; ++i) {
            uint16_t c = 0;
            if (!ue.rd(data + (uint64_t)i * 2, c)) break;
            s.push_back(c >= 0x20 && c < 0x7F ? (char)c : '?');
        }
        n.value = s.empty() ? "(empty)" : s;
    }
    else if (type == "TextProperty") n.value = "(text)";
    else if (type == "ClassProperty") { const uint64_t c = ue.rd64(a); n.value = UE4::plausible(c) ? ue_->objectName(c) : "None"; }
    else if (type == "ObjectProperty" || type == "WeakObjectProperty" || type == "LazyObjectProperty" || type == "InterfaceProperty")
        n.value = type == "ObjectProperty" ? summary(ue.rd64(a)) : "(reference)";
    else if (type == "SoftObjectProperty" || type == "SoftClassProperty") n.value = "(soft reference)";
    else if (type == "StructProperty") {
        const uint64_t st = ue.rd64(prop + 0x70);
        n.value = UE4::plausible(st) ? ue_->objectName(st) : "struct";
        if (UE4::plausible(st) && depth < 5) addStruct(n, st, a, depth + 1);
    }
    else if (type == "ArrayProperty") {
        const uint64_t inner = ue.rd64(prop + 0x70), data = ue.rd64(a);
        int32_t num = 0;
        ue.rd(a + 8, num);
        uint32_t es = 0;
        if (UE4::plausible(inner)) ue.rd(inner + 0x34, es);
        n.value = (num < 0 || num > 100000 ? std::string("?") : std::to_string(num)) + " items";
        if (UE4::plausible(inner) && UE4::plausible(data) && es > 0 && num > 0 && num <= 100000 && depth < 5) {
            const std::string it = ue_->propClassOf(inner);
            for (int32_t i = 0; i < num && i < 40 && budget_ > 0; ++i)
                n.kids.push_back(value(data + (uint64_t)i * es, it, inner, "[" + std::to_string(i) + "]", depth + 1));
            if (num > 40) { ViewNode more; more.label = "..."; more.value = std::to_string(num - 40) + " more not shown"; n.kids.push_back(more); }
        }
    }
    else if (type == "MapProperty" || type == "SetProperty") n.value = "(" + type.substr(0, type.size() - 8) + ")";
    else n.value = "(" + type + ")";
    return n;
}

void ItemViewerMod::addObject(ViewNode& into, uint64_t obj) {
    if (!UE4::plausible(obj)) return;
    const uint64_t cls = ue_->rd64(obj + 0x10);
    if (UE4::plausible(cls)) addStruct(into, cls, obj, 0);
}

bool ItemViewerMod::snapshot(std::vector<ViewNode>& out, std::string& err) {
    out.clear();
    budget_ = 8000;
    uint64_t ch = 0;
    if (!ue_->ready() || !ue_->character(ch)) { err = "Can't find your character. Load into a level first."; return false; }

    uint64_t eq = 0, slots = 0;
    int32_t num = 0;
    if (ue_->readPtr(ch, "EquipmentComponent", eq) && ue_->readArray(eq, "EquipmentSlots", slots, num)) {
        ViewNode root;
        root.label = "Equipped items";
        for (int32_t i = 0; i < num && i < 32; ++i) {
            const uint64_t slot = ue_->rd64(slots + (uint64_t)i * 8);
            uint64_t inst = 0;
            if (!UE4::plausible(slot) || !ue_->readPtr(slot, "Instance", inst)) continue;
            ViewNode item;
            std::string sn = ue_->objectName(slot);
            // "MeleeWeaponItemSlot" -> "Melee Weapon"
            std::string pretty;
            for (size_t k = 0; k < sn.size(); ++k) {
                if (k > 0 && sn[k] >= 'A' && sn[k] <= 'Z') pretty.push_back(' ');
                pretty.push_back(sn[k]);
            }
            item.label = pretty.empty() ? "Slot" : pretty;
            item.value = summary(inst);
            addObject(item, inst);
            root.kids.push_back(std::move(item));
        }
        if (!root.kids.empty()) out.push_back(std::move(root));
    }
    struct Extra { const char* prop; const char* label; };
    for (const Extra& e : {Extra{"MeleeAttack", "Melee attack component"}, Extra{"ArmorProperties", "Armor properties component"}}) {
        uint64_t o = 0;
        if (!ue_->readPtr(ch, e.prop, o)) continue;
        ViewNode n;
        n.label = e.label;
        n.value = summary(o);
        addObject(n, o);
        out.push_back(std::move(n));
    }
    if (out.empty()) { err = "No equipped items found yet. Open your inventory or start a level."; return false; }
    return true;
}

} // namespace dk
