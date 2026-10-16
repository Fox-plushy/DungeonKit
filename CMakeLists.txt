cmake_minimum_required(VERSION 3.16)
project(DungeonKit VERSION 1.121.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


option(DK_BUILD_TESTS "Build platform-independent unit tests" OFF)

# ---- platform-independent core pieces + tests (build anywhere) ----
add_library(dk_pattern STATIC src/core/Pattern.cpp)
if(DK_BUILD_TESTS)
    add_executable(dk_tests tests/test_core.cpp)
    add_library(dk_ids STATIC src/data/IdTables.cpp)
    target_link_libraries(dk_tests dk_pattern dk_ids)
endif()

# ---- the trainer itself (Windows only) ----
if(WIN32)
    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)       # Qt 5.15+ or Qt 6
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

    add_executable(DungeonKit WIN32
        src/main.cpp
        src/core/Process.cpp
        src/core/Hook.cpp
        src/core/GameVersions.cpp
        src/core/UE4.cpp
        src/core/UE4Dump.cpp
        src/core/Injector.cpp
        src/mods/Mods.cpp
        src/mods/PlayerTools.cpp
        src/mods/ItemViewer.cpp
        src/mods/Mcd2.cpp
        src/ui/MainWindow.cpp
        src/ui/Theme.cpp
        src/ui/IdPicker.cpp
        src/ui/ItemsPage.cpp
        src/ui/NameDialog.cpp
        src/ui/Identity.cpp
        src/ui/Icons.cpp
        src/ui/TeleportsPage.cpp
        src/ui/Logo.cpp
        src/ui/Splash.cpp
        src/ui/GameFolders.cpp
        src/ui/SaveBackup.cpp
        src/ui/I18n.cpp
        src/ui/I18nData.cpp
        src/ui/ItemViewerPage.cpp
        src/ui/Mcd2Pages.cpp
        src/ui/Mcd1Pages.cpp
        src/ui/DiscordPresence.cpp
        src/ui/TitleBar.cpp
        src/ui/ClickSound.cpp
        src/ui/AppSound.cpp
        src/ui/ShopPricesCard.cpp
        src/ui/ModMaker.cpp
        src/ui/ProbeCard.cpp
        src/ui/NameHideCard.cpp
        src/ui/WidgetHideCard.cpp
        src/ui/Overlay.cpp
        src/data/MapTeleports.cpp
        src/data/IdTables.cpp
        src/data/Mcd2Catalog.cpp
        resources/app.rc
        resources/resources.qrc
    )
    set_target_properties(DungeonKit PROPERTIES AUTOMOC ON AUTORCC ON)
    target_include_directories(DungeonKit PRIVATE src)
    target_compile_definitions(DungeonKit PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    target_link_libraries(DungeonKit PRIVATE dk_pattern Qt${QT_VERSION_MAJOR}::Widgets winmm)

    # DLLs that are loaded into the game (Minecraft Dungeons > Tools > Console). They are plain DLLs with no Qt and with the C++ runtime
    # linked in (the game has no libstdc++ next to it). They are copied beside DungeonKit.exe in a folder named Probe.
    function(dk_add_game_dll name source)
        add_library(${name} SHARED ${source} src/core/Process.cpp src/core/UE4.cpp src/core/GameVersions.cpp)
        target_include_directories(${name} PRIVATE src)
        target_compile_definitions(${name} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
        target_link_libraries(${name} PRIVATE dk_pattern)
        set_target_properties(${name} PROPERTIES PREFIX "")
        if(MSVC)
            target_compile_options(${name} PRIVATE /MT /utf-8)
        else()
            target_link_options(${name} PRIVATE -static -static-libgcc -static-libstdc++)
        endif()
        add_dependencies(DungeonKit ${name})
        add_custom_command(TARGET DungeonKit POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:DungeonKit>/Probe"
            COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${name}>" "$<TARGET_FILE_DIR:DungeonKit>/Probe/${name}.dll")
    endfunction()
    dk_add_game_dll(DungeonKitConsole src/console/Console.cpp)
    dk_add_game_dll(DungeonKitConsole2 src/console/Console2.cpp)

    if(MSVC)
        target_compile_options(DungeonKit PRIVATE /W3 /utf-8)
        # The manifest (administrator elevation) is embedded through app.rc instead.
        set_target_properties(DungeonKit PROPERTIES LINK_FLAGS "/MANIFEST:NO")
    endif()

    # The logo is built into the exe (resources/logo.png). To override it without rebuilding,
    # put a logo.png next to DungeonKit.exe (it is then used as-is, without theme recoloring).
else()
    message(STATUS "DungeonKit targets Windows; only the core tests are built on this platform.")
endif()
