#pragma once
// Developer dumps of the game's name table and object table (what the original table's
// "Data Dumper" and "Object Dumper" did). Writes plain text files.
#include <atomic>
#include <string>
#include <vector>
#include "UE4.h"

namespace dk {

bool dumpNames(UE4& ue, const std::wstring& path, std::atomic<int>& count, std::string& err);
bool dumpClasses(UE4& ue, const std::wstring& path, std::atomic<int>& count, std::string& err);   // every class/struct with its fields
// Names of every loaded Blueprint class that is an Actor (what the console's "summon" can create), e.g. "BP_TargetDummy_C".
bool scanSummonable(UE4& ue, std::vector<std::string>& out, std::atomic<int>& progress, std::string& err);
bool dumpObjects(UE4& ue, const std::wstring& path, std::atomic<int>& count, std::string& err);

}
