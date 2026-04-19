# flatlas_core – Grundlegende Dienste (Config, I18n, Theme, Logger, PathUtils, Undo)
add_library(flatlas_core STATIC
    Config.h Config.cpp
    I18n.h I18n.cpp
    Theme.h Theme.cpp
    Logger.h Logger.cpp
    PathUtils.h PathUtils.cpp
    UndoManager.h UndoManager.cpp
)

target_include_directories(flatlas_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(flatlas_core PUBLIC Qt6::Core Qt6::Widgets)
