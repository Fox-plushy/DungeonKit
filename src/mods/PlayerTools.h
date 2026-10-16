#pragma once
// Mods that work through the game's own object tables (UE4 reflection) instead of code patches:
// No Clip & Fly, God Mode and the player attribute editor.
#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include "../core/UE4.h"
#include "Mods.h"

namespace dk {

// Base for mods that keep a background thread running while enabled.
class UEToolMod : public Mod {
public:
    std::string technique() const override;
protected:
    explicit UEToolMod(std::shared_ptr<UE4> ue) : ue_(std::move(ue)) {}
    ~UEToolMod() override { stopThread(); }

    bool onEnable(const Process& p, std::string& err) final;
    void onDisable(const Process& p) final;

    virtual bool setup(const Process&, std::string&) { return true; }
    virtual void teardown(const Process&) {}
    virtual void tick() {}
    virtual int intervalMs() const { return 0; }       // 0 = no thread

    std::shared_ptr<UE4> ue_;
    const Process* proc_ = nullptr;

private:
    void stopThread();
    static unsigned long __stdcall threadMain(void* self);
    void* th_ = nullptr;                    // Win32 thread handle
    std::atomic<bool> stop_{false};
};

class NoClipMod : public UEToolMod {
public:
    explicit NoClipMod(std::shared_ptr<UE4> ue) : UEToolMod(std::move(ue)) {}
    std::string id() const override { return "noclip"; }
    std::string name() const override { return "No Clip & Fly"; }
    std::string description() const override {
        return "Walk through walls and fly. Space = up, Left Ctrl = down, W A S D = move. Keys only count while the game is the active window.";
    }
    void setSpeed(float unitsPerSecond) { speed_ = unitsPerSecond; }
    float speed() const { return speed_; }
protected:
    bool setup(const Process& p, std::string& err) override;
    void teardown(const Process& p) override;
    void tick() override;
    int intervalMs() const override { return 8; }
private:
    std::atomic<float> speed_{2000.f};
    std::vector<uint32_t> zOffsets_;        // every copy of the capsule's Z inside the component
    UE4::Prop moveMode_, collision_;
    bool haveCollision_ = false;
    bool armed_ = false;                    // setup finished; teardown may restore
    std::chrono::steady_clock::time_point last_{};
};

// No Cooldown for builds where the code pattern is not found (Steam): raises every equipment slot's
// "CooldownReduction" so artifact / potion cooldowns finish at once. Works through the object tables only.
class SlotCooldownMod : public UEToolMod {
public:
    explicit SlotCooldownMod(std::shared_ptr<UE4> ue) : UEToolMod(std::move(ue)) {}
    std::string id() const override { return "slot_cooldown"; }
    std::string name() const override { return "No Cooldown"; }
    std::string description() const override { return "Equipment slot cooldowns finish at once."; }
    std::string note() const { return note_; }
protected:
    bool setup(const Process& p, std::string& err) override;
    void teardown(const Process& p) override;
    void tick() override;
    int intervalMs() const override { return 150; }
private:
    int apply(bool restore);
    std::map<uint64_t, float> original_;     // slot -> value before we changed it
    std::string note_;
};

// Enemies stop targeting you: clears the character's "targetable" flag. Experimental.
class NoTargetMod : public UEToolMod {
public:
    explicit NoTargetMod(std::shared_ptr<UE4> ue) : UEToolMod(std::move(ue)) {}
    std::string id() const override { return "no_target"; }
    std::string name() const override { return "Enemies Ignore You"; }
    std::string description() const override { return "Enemies stop targeting you. Experimental: the game may switch it back, and bosses may still react."; }
protected:
    bool setup(const Process& p, std::string& err) override;
    void teardown(const Process& p) override;
    void tick() override;
    int intervalMs() const override { return 150; }
private:
    UE4::Prop flag_;
    bool armed_ = false, orig_ = true;
};

// WASD movement (experimental): hold W A S D to move your character along the ground. Keys only count while the game is the active window.
class WasdMod : public UEToolMod {
public:
    explicit WasdMod(std::shared_ptr<UE4> ue) : UEToolMod(std::move(ue)) {}
    std::string id() const override { return "wasd"; }
    std::string name() const override { return "WASD Movement"; }
    std::string description() const override {
        return "Walk with W A S D by feeding the game its own movement input. Experimental. Offline only. Keys only count while the game is the active window.";
    }
    void setRotation(float degrees) { rotation_ = degrees; }
    void setExclusive(Mod* other) { exclusive_ = other; }        // does nothing while this mod is on (No Clip)
protected:
    bool setup(const Process& p, std::string& err) override;
    void teardown(const Process& p) override;
    void tick() override;
    int intervalMs() const override { return 8; }
private:
    std::atomic<float> rotation_{0.f};
    uint32_t inputOffset_ = 0;              // Pawn.ControlInputVector
    Mod* exclusive_ = nullptr;
};

// Overkill: your attacks one-shot nearly everything. Raises the damage multipliers on your attribute sets
// (melee, ranged and the "affector" DealDamageMul) and puts the old values back when switched off.
class OverkillMod : public UEToolMod {
public:
    explicit OverkillMod(std::shared_ptr<UE4> ue) : UEToolMod(std::move(ue)) {}
    std::string id() const override { return "overkill"; }
    std::string name() const override { return "Overkill"; }
    std::string description() const override { return "Your attacks one-shot nearly everything. Raises your melee and ranged damage; turning it off puts them back."; }
protected:
    bool setup(const Process& p, std::string& err) override;
    void teardown(const Process& p) override;
    void tick() override;
    int intervalMs() const override { return 150; }
private:
    bool collect(std::vector<uint64_t>& addrs);
    std::map<uint64_t, float> original_;      // attribute address -> value before Overkill
};

class GodModeMod : public UEToolMod {
public:
    explicit GodModeMod(std::shared_ptr<UE4> ue) : UEToolMod(std::move(ue)) {}
    std::string id() const override { return "god_mode"; }
    std::string name() const override { return "God Mode"; }
    std::string description() const override { return "Your character can't be damaged."; }
protected:
    bool setup(const Process& p, std::string& err) override;
    void teardown(const Process& p) override;
    void tick() override;
    int intervalMs() const override { return 200; }
private:
    UE4::Prop flag_;
    bool armed_ = false;
};

// Player attributes (melee / ranged / health / resistance / movement).
class StatsMod : public UEToolMod {
public:
    struct Def { const char* group; const char* label; const char* prop; };
    static const std::vector<Def>& defs();
    static const char* groupName(int g);              // 0..4
    static int groupCount() { return 5; }

    explicit StatsMod(std::shared_ptr<UE4> ue) : UEToolMod(std::move(ue)) {}
    std::string id() const override { return "player_stats"; }
    std::string name() const override { return "Player Stats"; }
    std::string description() const override { return "Edit your attack speed, damage, health, movement and more. Values reset when you change level."; }

    bool get(int def, float& v);
    bool set(int def, float v);
    std::string summary();                            // which attribute objects were found
protected:
    bool setup(const Process& p, std::string& err) override;
private:
    bool resolve(bool force);
    bool locate(int def, uint64_t& addr);
    uint64_t groups_[5] = {0, 0, 0, 0, 0};
    std::string groupClass_[5];
    uint64_t resolvedAt_ = 0;
};

// Game speed through the world's TimeDilation value (not the PC clock). Shared by Timer and Pause Game.
class TimeControl {
public:
    explicit TimeControl(std::shared_ptr<UE4> ue) : ue_(std::move(ue)) {}
    std::atomic<float> speed{2.5f};
    std::atomic<bool> timerOn{false}, pauseOn{false};
    bool locate(std::string& err);              // can we reach TimeDilation right now?
    void apply();                               // write the speed that the switches ask for
    bool readRaw(float& v);                     // the world's current TimeDilation
    bool writeRaw(float v);                     // set TimeDilation directly (Camera Editor photo mode)
private:
    bool worldSettings(uint64_t& ws);
    std::shared_ptr<UE4> ue_;
};

class TimerMod : public UEToolMod {
public:
    TimerMod(std::shared_ptr<UE4> ue, std::shared_ptr<TimeControl> tc) : UEToolMod(std::move(ue)), tc_(std::move(tc)) {}
    std::string id() const override { return "timer"; }
    std::string name() const override { return "Timer"; }
    std::string description() const override { return "Speeds the game up or slows it down. 1.0 is normal. Hotkeys: Ctrl + Numpad + on, Ctrl + Numpad - off."; }
    void setSpeed(float s) { tc_->speed = s; if (enabled()) tc_->apply(); }
    float speed() const { return tc_->speed; }
protected:
    bool setup(const Process&, std::string& err) override;
    void teardown(const Process&) override;
    void tick() override { tc_->apply(); }
    int intervalMs() const override { return 250; }
private:
    std::shared_ptr<TimeControl> tc_;
};

class PauseMod : public UEToolMod {
public:
    PauseMod(std::shared_ptr<UE4> ue, std::shared_ptr<TimeControl> tc) : UEToolMod(std::move(ue)), tc_(std::move(tc)) {}
    std::string id() const override { return "pause"; }
    std::string name() const override { return "Pause Game"; }
    std::string description() const override { return "Freezes the action in the level. Hotkey: Pause / Break."; }
protected:
    bool setup(const Process&, std::string& err) override;
    void teardown(const Process&) override;
    void tick() override { tc_->apply(); }
    int intervalMs() const override { return 250; }
private:
    std::shared_ptr<TimeControl> tc_;
};

} // namespace dk
