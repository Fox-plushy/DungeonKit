#pragma once
// Minimal Unreal Engine 4 reflection reader: finds GEngine and the name table, then looks up
// property offsets *by name* while walking from the engine to the player character.
// This is what Cheat Engine's UE4 template does for the original table (GameEngine.GameInstance,
// LocalPlayer.PlayerController, ...). Nothing is hard-coded per game build.
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include "Process.h"

namespace dk {

// Tiny Win32 lock (avoids depending on the MinGW threading model for std::mutex).
class CsLock {
public:
    CsLock() { InitializeCriticalSection(&cs_); }
    ~CsLock() { DeleteCriticalSection(&cs_); }
    CsLock(const CsLock&) = delete;
    CsLock& operator=(const CsLock&) = delete;
    void lock() { EnterCriticalSection(&cs_); }
    void unlock() { LeaveCriticalSection(&cs_); }
private:
    CRITICAL_SECTION cs_;
};
struct CsGuard {
    explicit CsGuard(CsLock& l) : l_(l) { l_.lock(); }
    ~CsGuard() { l_.unlock(); }
    CsLock& l_;
};

class UE4 {
public:
    struct Prop {
        uint32_t offset = 0;     // Offset_Internal
        uint8_t  mask = 0;       // bool properties: bit mask inside the byte at offset+byteOffset
        uint8_t  byteOffset = 0;
        bool     isBool = false;
    };

    // Locate GEngine + the name table (cached per game process). Returns false with a reason.
    bool init(const Process& p, std::string& err);
    bool ready() const { return ready_; }
    void reset();
    std::string diagnostics() const;

    // Player character, resolved fresh each call (it changes between levels).
    bool engine(uint64_t& eng) { eng = ready_ ? rd64(engineVar_) : 0; return plausible(eng); }
    bool character(uint64_t& ch);

    bool find(uint64_t obj, const char* name, Prop& out);         // property of obj's class (or any super class)
    bool readPtr(uint64_t obj, const char* prop, uint64_t& out);  // pointer-typed property
    std::string className(uint64_t obj);
    // TArray property of pointers: data pointer + element count
    bool readArray(uint64_t obj, const char* prop, uint64_t& data, int32_t& num);

    // ---- extra reflection helpers (property-object layout of UE 4.22-4.24 only; "classic")
    struct Field { std::string name, type; uint64_t prop = 0; uint32_t offset = 0, elemSize = 0, dim = 1; };
    bool classic() const { return ready_ && !lay_.fField; }
    bool modernNames() const { return modernNames_; }
    uint64_t namesVar() const { return namesVar_; }
    std::string nameAt(uint32_t index);                    // text of an FName index ("" if unreadable)
    std::string objectName(uint64_t obj);                  // FName of any UObject
    std::string propClassOf(uint64_t prop);                // e.g. "FloatProperty"
    bool listFields(uint64_t structPtr, std::vector<Field>& out, bool ownOnly = false);   // struct/class + supers (or just itself), most specific first
    std::string enumeratorName(uint64_t enumObj, int64_t value);    // "EFoo::Bar" -> "Bar"

    // GUObjectArray (found on demand, used by the dump tools)
    struct ObjArray { uint64_t chunks = 0; int32_t num = 0, perChunk = 0, numChunks = 0; };
    bool objectArray(ObjArray& out, std::string& err);
    uint64_t objectAt(const ObjArray& a, int32_t index);

    static bool plausible(uint64_t ptr) { return ptr > 0x10000 && ptr < 0x7FFFFFFFFFFFull; }
    template <class T> bool rd(uint64_t a, T& v) const { return p_ && p_->read(a, &v, sizeof(T)); }
    uint64_t rd64(uint64_t a) const { uint64_t v = 0; rd(a, v); return v; }
    const Process* proc() const { return p_; }

private:
    struct Layout { uint32_t member, next, offset, mask, nameOff; bool fField; const char* label; };

    bool readNameEntry(uint32_t index, std::string& out);
    std::string nameOf(uint32_t index);
    std::string objName(uint64_t obj);                    // FName of a UObject (class names)
    std::string propName(uint64_t field);
    std::string propClassName(uint64_t field);
    bool readText(uint64_t addr, bool wide, size_t maxChars, std::string& out) const;
    bool findLocked(uint64_t cls, const std::string& name, Prop& out);
    bool validateObjArray(uint64_t cand, ObjArray& out);
    bool scanForObjArray(ObjArray& out);                 // pattern-free search of the module's data for the object table
    bool discoverLayout(uint64_t eng, uint64_t cls, std::string& log);   // finds the property-table layout of an unknown engine build

    const Process* p_ = nullptr;
    DWORD pid_ = 0;
    bool ready_ = false;
    bool modernNames_ = false;      // UE 4.23+ FNamePool, else the older chunked GNames
    uint64_t namesVar_ = 0, engineVar_ = 0;
    uint32_t strOff_ = 0x10;        // old style: offset of the characters inside an FNameEntry
    Layout lay_{};
    std::string diag_;
    ObjArray objArray_;
    bool haveObjArray_ = false;

    mutable CsLock mu_;
    std::unordered_map<uint32_t, std::string> names_;
    std::map<std::pair<uint64_t, std::string>, std::pair<bool, Prop>> propCache_;
};

} // namespace dk
