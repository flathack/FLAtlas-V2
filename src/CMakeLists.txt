# ==============================================================================
# FLAtlas V2 – Source Targets
# ==============================================================================

# --- flatlas_core (statische Bibliothek: Config, I18n, Theme, Logger, PathUtils, Undo) ---
add_subdirectory(core)

# --- flatlas_domain (statische Bibliothek: Datenmodelle, UI-frei) ---
add_subdirectory(domain)

# --- flatlas_infrastructure (statische Bibliothek: Parser, IO, Freelancer, Updater) ---
add_subdirectory(infrastructure)

# --- flatlas_rendering (statische Bibliothek: 2D, 3D, Flight, Preview) ---
add_subdirectory(rendering)

# --- flatlas_editors (statische Bibliothek: Alle Editor-Pages) ---
add_subdirectory(editors)

# --- flatlas_ui (statische Bibliothek: MainWindow, Panels, Dialoge) ---
add_subdirectory(ui)

# --- flatlas_tools (statische Bibliothek: Patcher, Starter, Help) ---
add_subdirectory(tools)

# --- Hauptanwendung ---
add_executable(FLAtlas WIN32
    main.cpp
    app/Application.h
    app/Application.cpp
    app/SplashScreen.h
    app/SplashScreen.cpp
    app/AppConfig.h
    app/AppConfig.cpp
    ${CMAKE_SOURCE_DIR}/resources/resources.qrc
)

target_link_libraries(FLAtlas PRIVATE
    flatlas_ui
    flatlas_editors
    flatlas_rendering
    flatlas_infrastructure
    flatlas_domain
    flatlas_core
    flatlas_tools
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
)

target_include_directories(FLAtlas PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
