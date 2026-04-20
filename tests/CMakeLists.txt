# Tests for FLAtlas V2
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test)

function(flatlas_add_test name source)
    add_executable(${name} ${source})
    target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(${name} PRIVATE Qt6::Test ${ARGN})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

flatlas_add_test(test_IniParser      test_IniParser.cpp      flatlas_infrastructure)
flatlas_add_test(test_BiniDecoder    test_BiniDecoder.cpp    flatlas_infrastructure)
flatlas_add_test(test_BiniConverter  test_BiniConverter.cpp  flatlas_infrastructure)
flatlas_add_test(test_PathUtils      test_PathUtils.cpp      flatlas_core)

# Phase 2 domain model tests
flatlas_add_test(test_DomainModels   test_DomainModels.cpp   flatlas_domain)

# Phase 1 core tests
flatlas_add_test(test_Config      test_Config.cpp      flatlas_core)
flatlas_add_test(test_Theme       test_Theme.cpp       flatlas_core)
flatlas_add_test(test_I18n        test_I18n.cpp        flatlas_core)
flatlas_add_test(test_UndoManager test_UndoManager.cpp flatlas_core)
flatlas_add_test(test_Logger      test_Logger.cpp      flatlas_core)

# Phase 5 system editor tests
flatlas_add_test(test_SystemPersistence test_SystemPersistence.cpp flatlas_editors)

# Phase 6 INI editor tests
flatlas_add_test(test_IniSyntaxHighlighter test_IniSyntaxHighlighter.cpp flatlas_editors Qt6::Widgets)

# Phase 7 3D view tests
flatlas_add_test(test_SceneView3D test_SceneView3D.cpp flatlas_rendering Qt6::Widgets)

# Phase 8 CMP/3DB/Texture tests
flatlas_add_test(test_VmeshDecoder    test_VmeshDecoder.cpp    flatlas_infrastructure)
flatlas_add_test(test_CmpLoader       test_CmpLoader.cpp       flatlas_infrastructure)
flatlas_add_test(test_TextureLoader   test_TextureLoader.cpp   flatlas_infrastructure)
flatlas_add_test(test_ModelCache      test_ModelCache.cpp       flatlas_rendering)

# Phase 9 Universe editor tests
flatlas_add_test(test_UniverseSerializer   test_UniverseSerializer.cpp   flatlas_editors)
flatlas_add_test(test_UniverseEditorPage   test_UniverseEditorPage.cpp   flatlas_editors Qt6::Widgets)

# Phase 10 Base editor tests
flatlas_add_test(test_BaseEditor   test_BaseEditor.cpp   flatlas_editors Qt6::Widgets)

# Phase 11 Trade route tests
flatlas_add_test(test_MarketScanner   test_MarketScanner.cpp   flatlas_editors)
flatlas_add_test(test_TradeScoring    test_TradeScoring.cpp    flatlas_editors flatlas_domain)

# Phase 12 IDS/String tests
flatlas_add_test(test_IdsStringTable  test_IdsStringTable.cpp  flatlas_infrastructure)
flatlas_add_test(test_CsvImporter     test_CsvImporter.cpp     flatlas_infrastructure)
flatlas_add_test(test_IdsEditorPage   test_IdsEditorPage.cpp   flatlas_editors Qt6::Widgets)

# Phase 13 Mod manager tests
flatlas_add_test(test_ModManager      test_ModManager.cpp      flatlas_editors flatlas_domain)

# Phase 14 NPC editor tests
flatlas_add_test(test_NpcEditor       test_NpcEditor.cpp       flatlas_editors Qt6::Widgets)

# Phase 15 Flight mode tests
flatlas_add_test(test_FlightMode      test_FlightMode.cpp      flatlas_rendering Qt6::Widgets)

# Phase 16 Model/Character preview tests
flatlas_add_test(test_ModelPreview    test_ModelPreview.cpp    flatlas_rendering Qt6::Widgets)

# Phase 17 Infocard editor tests
flatlas_add_test(test_InfocardEditor  test_InfocardEditor.cpp  flatlas_editors Qt6::Widgets)

# Phase 18 News/Rumor editor tests
flatlas_add_test(test_NewsRumorEditor test_NewsRumorEditor.cpp flatlas_editors Qt6::Widgets)

# Phase 19 Jump Connection dialog tests
flatlas_add_test(test_JumpConnection test_JumpConnection.cpp flatlas_editors Qt6::Widgets)

# Phase 20 EXE/DLL handling tests
flatlas_add_test(test_ExeHandling test_ExeHandling.cpp flatlas_infrastructure)

# Phase 21 Scripting/Patching tests
# SpStarter is compiled directly to avoid linking ScriptPatcher (triggers AV)
add_executable(test_SpStarter test_SpStarter.cpp ${CMAKE_SOURCE_DIR}/src/tools/SpStarter.cpp)
target_include_directories(test_SpStarter PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_SpStarter PRIVATE Qt6::Test Qt6::Core Qt6::Widgets flatlas_core)
add_test(NAME test_SpStarter COMMAND test_SpStarter)

# Phase 22 Auto-Updater tests
find_package(Qt6 REQUIRED COMPONENTS Network)
add_executable(test_VersionCheck test_VersionCheck.cpp
    ${CMAKE_SOURCE_DIR}/src/tools/UpdateChecker.cpp
    ${CMAKE_SOURCE_DIR}/src/tools/UpdateDownloader.cpp
)
target_include_directories(test_VersionCheck PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_VersionCheck PRIVATE Qt6::Test Qt6::Core Qt6::Widgets Qt6::Network flatlas_core)
add_test(NAME test_VersionCheck COMMAND test_VersionCheck)

# Phase 23 Help-System tests
add_executable(test_HelpBrowser test_HelpBrowser.cpp
    ${CMAKE_SOURCE_DIR}/src/tools/HelpBrowser.cpp
)
target_include_directories(test_HelpBrowser PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_HelpBrowser PRIVATE Qt6::Test Qt6::Core Qt6::Widgets)
add_test(NAME test_HelpBrowser COMMAND test_HelpBrowser)

# Phase 24 Shortest-Path-Generator tests
add_executable(test_ShortestPath test_ShortestPath.cpp
    ${CMAKE_SOURCE_DIR}/src/tools/ShortestPathGenerator.cpp
    ${CMAKE_SOURCE_DIR}/src/domain/UniverseData.cpp
)
target_include_directories(test_ShortestPath PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_ShortestPath PRIVATE Qt6::Test Qt6::Core Qt6::Gui)
add_test(NAME test_ShortestPath COMMAND test_ShortestPath)

# AppConfig is compiled into the FLAtlas executable, not a library.
# Compile the source directly into the test.
add_executable(test_AppConfig test_AppConfig.cpp ${CMAKE_SOURCE_DIR}/src/app/AppConfig.cpp)
target_include_directories(test_AppConfig PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_AppConfig PRIVATE Qt6::Test Qt6::Core)
add_test(NAME test_AppConfig COMMAND test_AppConfig)
