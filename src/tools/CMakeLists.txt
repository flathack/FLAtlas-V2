# flatlas_tools – Patcher, Starter, Help, Updater, Pathfinding
add_library(flatlas_tools STATIC
    ScriptPatcher.h ScriptPatcher.cpp
    SpStarter.h SpStarter.cpp
    HelpBrowser.h HelpBrowser.cpp
    KeyboardShortcutOverviewDialog.h KeyboardShortcutOverviewDialog.cpp
    UpdateChecker.h UpdateChecker.cpp
    UpdateDownloader.h UpdateDownloader.cpp
    UpdateInstaller.h UpdateInstaller.cpp
    ShortestPathGenerator.h ShortestPathGenerator.cpp
    PathFinderDialog.h PathFinderDialog.cpp
    ModelScreenshotExporter.h ModelScreenshotExporter.cpp
)

target_include_directories(flatlas_tools PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(flatlas_tools PUBLIC flatlas_core flatlas_infrastructure Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)
