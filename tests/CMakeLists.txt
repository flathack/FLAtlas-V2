# Tests for FLAtlas V2
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test)

function(flatlas_add_test name source)
    add_executable(${name} ${source})
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(${name} PRIVATE Qt6::Test ${ARGN})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

flatlas_add_test(test_IniParser     test_IniParser.cpp     flatlas_infrastructure)
flatlas_add_test(test_BiniDecoder   test_BiniDecoder.cpp   flatlas_infrastructure)
flatlas_add_test(test_PathUtils     test_PathUtils.cpp     flatlas_core)

# Phase 1 core tests
flatlas_add_test(test_Config      test_Config.cpp      flatlas_core)
flatlas_add_test(test_Theme       test_Theme.cpp       flatlas_core)
flatlas_add_test(test_I18n        test_I18n.cpp        flatlas_core)
flatlas_add_test(test_UndoManager test_UndoManager.cpp flatlas_core)
flatlas_add_test(test_Logger      test_Logger.cpp      flatlas_core)

# AppConfig is compiled into the FLAtlas executable, not a library.
# Compile the source directly into the test.
add_executable(test_AppConfig test_AppConfig.cpp ${CMAKE_SOURCE_DIR}/src/app/AppConfig.cpp)
target_include_directories(test_AppConfig PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_AppConfig PRIVATE Qt6::Test Qt6::Core)
add_test(NAME test_AppConfig COMMAND test_AppConfig)
