#pragma once
// Read-only view of everything the game reports about your equipped items.
// Idea and original viewer: Hybrid. This is a re-implementation inside DungeonKit that
// walks the items' reflected properties, so nothing here can be changed from the app.
#include <memory>
#include <string>
#include <vector>
#include "../core/UE4.h"
#include "Mods.h"

namespace dk {

struct ViewNode {
    std::string label, value;
    std::vector<ViewNode> kids;
};

class ItemViewerMod : public Mod {
public:
    explicit ItemViewerMod(std::shared_ptr<UE4> ue) : ue_(std::move(ue)) {}
    std::string id() const override { return "item_viewer"; }
    std::string name() const override { return "Item Stat Viewer"; }
    std::string description() const override { return "Shows the stats of your equipped items. View only."; }

    // Builds the tree of equipped items. Returns false (with a reason) if the player isn't reachable.
    bool snapshot(std::vector<ViewNode>& out, std::string& err);

    std::string technique() const override {
        return "Read-only reflection walk: no code is patched and nothing is written. DungeonKit follows the "
               "game's own object tables from your character to its equipped items, then to each item's stat "
               "properties, matching property names to their byte offset the same way the engine itself does, "
               "and just reads the values.";
    }

protected:
    bool onEnable(const Process& p, std::string& err) override;
    void onDisable(const Process&) override {}

private:
    void addObject(ViewNode& into, uint64_t obj);
    ViewNode value(uint64_t addr, const std::string& type, uint64_t prop, const std::string& label, int depth);
    void addStruct(ViewNode& into, uint64_t structPtr, uint64_t base, int depth);
    std::string summary(uint64_t obj);

    std::shared_ptr<UE4> ue_;
    int budget_ = 0;
};

} // namespace dk
