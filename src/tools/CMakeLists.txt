# flatlas_tools – Patcher, Starter, Help, Updater
add_library(flatlas_tools STATIC
    ScriptPatcher.h ScriptPatcher.cpp
    SpStarter.h SpStarter.cpp
    HelpBrowser.h HelpBrowser.cpp
    UpdateChecker.h UpdateChecker.cpp
    UpdateDownloader.h UpdateDownloader.cpp
    UpdateInstaller.h UpdateInstaller.cpp
)

target_include_directories(flatlas_tools PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(flatlas_tools PUBLIC flatlas_core Qt6::Core Qt6::Widgets Qt6::Network)
