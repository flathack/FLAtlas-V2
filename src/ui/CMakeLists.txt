# flatlas_ui – Hauptfenster, Panels, Dialoge
add_library(flatlas_ui STATIC
    MainWindow.h MainWindow.cpp
    WelcomePage.h WelcomePage.cpp
    BrowserPanel.h BrowserPanel.cpp
    PropertiesPanel.h PropertiesPanel.cpp
    CenterTabWidget.h CenterTabWidget.cpp
    SettingsDialog.h SettingsDialog.cpp
    StatusBarManager.h StatusBarManager.cpp
)

target_include_directories(flatlas_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(flatlas_ui PUBLIC
    flatlas_editors
    flatlas_rendering
    flatlas_core
    Qt6::Core
    Qt6::Widgets
)
