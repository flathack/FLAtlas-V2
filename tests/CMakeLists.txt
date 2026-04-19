# Tests for FLAtlas V2
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test)

function(flatlas_add_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE Qt6::Test ${ARGN})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

flatlas_add_test(test_IniParser     test_IniParser.cpp     flatlas_infrastructure)
flatlas_add_test(test_BiniDecoder   test_BiniDecoder.cpp   flatlas_infrastructure)
flatlas_add_test(test_PathUtils     test_PathUtils.cpp     flatlas_core)
flatlas_add_test(test_AppConfig     test_AppConfig.cpp     flatlas_app)
