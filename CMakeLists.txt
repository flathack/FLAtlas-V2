cmake_minimum_required(VERSION 3.21)
project(FLAtlas VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt6 finden
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Gui
    Network
    Xml
)

# Optional: Qt3D (wird erst in Phase 7 benötigt)
find_package(Qt6 COMPONENTS
    3DCore
    3DRender
    3DInput
    3DExtras
    QUIET
)

# Optional: Linguist Tools
find_package(Qt6 COMPONENTS LinguistTools QUIET)

# Compiler-Warnungen
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Windows-spezifisch
if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE)
endif()

add_subdirectory(src)

enable_testing()
add_subdirectory(tests)

# Third-Party (noch leer)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/CMakeLists.txt")
    add_subdirectory(third_party)
endif()
