// Generated from the original cheat table (Map Teleports). Coordinates are X, Y, Z world positions.
#include "MapTeleports.h"
namespace dk {
namespace {
const TeleportSpot kGroup0[] = {
    {"Defeat the Zombie", 14935.75586f, 11451.10742f, 6112.149902f},
    {"Defeat the Invaders", 18822.47266f, 9261.900391f, 6512.149902f},
    {"Shoot the Skeleton Guard", 20446.4707f, 13460.7373f, 6711.950684f},
    {"Secure the Inner Gate", 24688.10742f, 10383.68945f, 6910.828613f},
    {"Survive the Ambush", 31698.5957f, 9448.771484f, 6212.149902f},
    {"Set Up Camp Outside Town (EXIT)", 52103.89844f, 25900.31055f, 8912.150391f},
};
const TeleportSpot kGroup1[] = {
    {"Staff Secret", 48955.86719f, 4012.025146f, 2212.149902f},
    {"Ruined Housing", 27946.39258f, 13307.25684f, 4977.149902f},
    {"Secret Chest", 9405.916016f, 14402.91406f, 12977.15039f},
    {"Pig Pen", 108411.9453f, 4084.102295f, 2077.150146f},
    {"Top of Tower", 24015.57617f, 8739.961914f, 14911.10938f},
    {"Lonely Island", 35139.87891f, 14043.71191f, 5412.149902f},
    {"Secret Mission", 94194.27344f, 6150.63916f, 5362.149902f},
};
const TeleportSpot kGroup2[] = {
    {"Find the Brews", 33598.26172f, 45655.17969f, 5812.149902f},
    {"Find the Cauldron", 33091.84766f, 16118.2207f, 5526.158203f},
    {"Smash the Cauldron", 42640.24609f, 9047.385742f, 5676.90332f},
    {"Escape the Swamp (EXIT)", 49439.63672f, 7643.958984f, 5512.149902f},
};
const TeleportSpot kGroup3[] = {
    {"Find the Lost Ruins", 26265.60547f, 23636.20117f, 6212.149902f},
    {"Explore the Ruins", 46806.375f, 9636.798828f, 3762.150146f},
    {"Ruin Chest Room", 60204.49219f, 6885.800293f, 2677.149902f},
    {"Return to the Boat (EXIT)", 10896.60547f, 33578.98438f, 5912.149902f},
};
const TeleportSpot kGroup4[] = {
    {"Free the Villager", 20679.60938f, 14350.33301f, 5812.149902f},
    {"Find the Currivan", 69600.21875f, 35226.73047f, 5512.149902f},
    {"Escape Creeper Woods (EXIT)", 120653.1094f, 35368.30469f, 6212.149902f},
};
const TeleportSpot kGroup5[] = {
    {"Find the Lost Tomb", 103079.3203f, 19792.45312f, 5290.149902f},
    {"Find the Lost Tomb - Button Room", 98416.17969f, 26164.73242f, 5305.899902f},
    {"Acquire the Scroll", 98401.85938f, 33412.36719f, 6112.149902f},
    {"Leave the Crypts (EXIT)", 163092.1875f, 20085.07031f, 6212.149902f},
};
const TeleportSpot kGroup6[] = {
    {"Survive the Attack", 70333.05469f, 14780.10547f, 5612.140137f},
    {"Lower the Drawbridge", 75954.21094f, 14942.26172f, 6112.149902f},
    {"Find the Village", 100399.75f, 48966.68359f, 6612.149902f},
    {"Ring the Warning Bell", 109961.1406f, 50605.61328f, 6312.149902f},
    {"Enter the Town Hall (EXIT)", 110473.1328f, 54166.68359f, 6477.148438f},
};
const TeleportSpot kGroup7[] = {
    {"Crash the Gates", 12417.89844f, 23359.74609f, 3312.150146f},
    {"Reach the Village", 24059.04102f, 24265.6582f, 4112.149902f},
    {"Find all the Spells", 30626.85742f, 30684.2168f, 5112.149902f},
    {"Explore the Cave", 35841.25391f, 24500.50586f, 4212.149902f},
    {"Find Another Exit", 55150.93359f, 6017.367676f, 3312.149902f},
    {"Defeat the Guards", 82015.86719f, 8907.59375f, 3312.149902f},
    {"Sail Away (EXIT)", 81807.0f, 6183.864746f, 3411.914062f},
};
const TeleportSpot kGroup8[] = {
    {"Power the Beacons", 41842.96484f, 10747.20996f, 6812.149902f},
    {"Find the Golden Key", 76706.22656f, 17685.95703f, 5912.149902f},
    {"Find the Temple", 96231.97656f, 14160.1582f, 5912.149902f},
    {"Enter the Temple (EXIT)", 106393.6953f, 14441.03516f, 4912.411621f},
};
const TeleportSpot kGroup9[] = {
    {"Free the Villagers", 24318.95703f, 16763.39844f, 5312.149902f},
    {"Free the Villagers P2", 13000.68262f, 31559.8418f, 6012.149902f},
    {"Leave the Mines (EXIT)", 28229.26367f, 22921.42969f, 5812.149902f},
};
const TeleportSpot kGroup10[] = {
    {"Enter the Forge", 15245.16211f, 5525.875f, 4505.899902f},
    {"Overload the Cores", 33293.69922f, 25192.0625f, 5412.149902f},
    {"Escape the Forge", 58080.66016f, 17854.44141f, 5955.07373f},
    {"Defeat the Monstrosity", 65755.78906f, 16885.57617f, 5812.149902f},
    {"Use the Mining Lift (EXIT)", 84733.25f, 16834.44727f, 5977.147949f},
};
const TeleportSpot kGroup11[] = {
    {"Find the Golden Key", 15762.88965f, 23919.44922f, 5699.12207f},
    {"Survive the Ambush", 27223.60547f, 39612.32422f, 6026.648438f},
    {"Fetch the Staff", 33619.94141f, 13079.54395f, 6812.149902f},
    {"Escape the Temple (EXIT)", 44139.03125f, 12363.56836f, 7612.140625f},
};
const TeleportSpot kGroup12[] = {
    {"Find the Golden Key", 9507.6875f, 14304.69043f, 5077.149902f},
    {"Unlock the Gate", 11456.37305f, 19814.37109f, 5512.149902f},
    {"Leave the Temple (EXIT)", 27058.14453f, 21681.61328f, 5718.209473f},
};
const TeleportSpot kGroup13[] = {
    {"Survive the Ambush", 23204.32617f, 9665.698242f, 6112.140137f},
    {"Find the Golden Key", 36423.36328f, 9577.998047f, 6718.399902f},
    {"Destroy the Buffet", 54995.95703f, 8991.925781f, 7118.399902f},
    {"Confront the Arch-Illager", 59265.10156f, 57917.5625f, 7118.399902f},
    {"Pursue the Arch-Illager (EXIT)", 59331.32422f, 64786.19922f, 8412.150391f},
};
const TeleportSpot kGroup14[] = {
    {"Enter the Cellars", 15230.12402f, 22760.63867f, 5618.399902f},
    {"Explore...", 77196.59375f, 13466.43652f, 3607.325439f},
    {"Leave the Forgotten Halls (EXIT)", 27221.73242f, 6487.466797f, 5318.399902f},
};
const TeleportSpot kGroup15[] = {
    {"Survive the Ambush", 23300.21289f, 27427.05469f, 6312.041992f},
    {"Power the Beacons", 23364.71484f, 20315.12891f, 6312.149902f},
    {"Pursue the Arch-Illager", 70341.0625f, 29823.35352f, 9977.150391f},
    {"Follow the Trail (EXIT)", 81968.55469f, 30070.97852f, 11212.15039f},
};
const TeleportSpot kGroup16[] = {
    {"Moo?", 23401.86328f, 29591.66602f, 5560.65918f},
    {"Leave the Island (EXIT)", 17993.45312f, 50726.43359f, 4512.149902f},
};
const TeleportGroup kGroups[] = {
    {"Squid Coast:", kGroup0, 6},
    {"Camp Lobby:", kGroup1, 7},
    {"Soggy Swamp:", kGroup2, 4},
    {"Soggy Cave:", kGroup3, 4},
    {"Creeper Woods:", kGroup4, 3},
    {"Creepy Crpyt:", kGroup5, 4},
    {"Pumpkin Pastures:", kGroup6, 5},
    {"Arch Haven:", kGroup7, 7},
    {"Cacti Canyon:", kGroup8, 4},
    {"Restone Mnes:", kGroup9, 3},
    {"Fiery Forge:", kGroup10, 5},
    {"Desert Temple:", kGroup11, 4},
    {"Lower Temple:", kGroup12, 3},
    {"Highblock Halls:", kGroup13, 5},
    {"Under Halls:", kGroup14, 3},
    {"Obsidian Pinnacle:", kGroup15, 4},
    {"???:", kGroup16, 2},
};
} // namespace
const TeleportGroup* teleportGroups(size_t& count) { count = sizeof(kGroups) / sizeof(kGroups[0]); return kGroups; }
} // namespace dk
