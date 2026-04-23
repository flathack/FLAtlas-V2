# flatlas_editors – Alle Editor-Pages
add_library(flatlas_editors STATIC
    system/SystemEditorPage.h system/SystemEditorPage.cpp
    system/SystemDisplayFilterDialog.h system/SystemDisplayFilterDialog.cpp
    system/SystemSettingsDialog.h system/SystemSettingsDialog.cpp
    system/SystemPersistence.h system/SystemPersistence.cpp
    system/SystemCreationWizard.h system/SystemCreationWizard.cpp
    system/SystemUndoCommands.h
    system/CreateObjectDialog.h system/CreateObjectDialog.cpp
    system/CreateFieldZoneDialog.h system/CreateFieldZoneDialog.cpp
    system/CreateExclusionZoneDialog.h system/CreateExclusionZoneDialog.cpp
    system/ExclusionZoneUtils.h system/ExclusionZoneUtils.cpp
    universe/UniverseEditorPage.h universe/UniverseEditorPage.cpp
    universe/UniverseSerializer.h universe/UniverseSerializer.cpp
    universe/NewSystemDialog.h universe/NewSystemDialog.cpp
    universe/NewSystemService.h universe/NewSystemService.cpp
    universe/EditSystemDialog.h universe/EditSystemDialog.cpp
    universe/DeleteSystemDialog.h universe/DeleteSystemDialog.cpp
    universe/DeleteSystemService.h universe/DeleteSystemService.cpp
    base/BaseEditorPage.h base/BaseEditorPage.cpp
    base/BasePersistence.h base/BasePersistence.cpp
    base/BaseBuilder.h base/BaseBuilder.cpp
    base/RoomEditor.h base/RoomEditor.cpp
    npc/NpcEditorPage.h npc/NpcEditorPage.cpp
    npc/MbaseOperations.h npc/MbaseOperations.cpp
    ini/IniEditorPage.h ini/IniEditorPage.cpp
    ini/IniCodeEditor.h ini/IniCodeEditor.cpp
    ini/IniFindReplaceDialog.h ini/IniFindReplaceDialog.cpp
    ini/IniMiniMap.h ini/IniMiniMap.cpp
    ini/IniSyntaxHighlighter.h ini/IniSyntaxHighlighter.cpp
    infocard/InfocardEditor.h infocard/InfocardEditor.cpp
    trade/TradeRoutePage.h trade/TradeRoutePage.cpp
    trade/MarketScanner.h trade/MarketScanner.cpp
    trade/TradeRouteDataService.h trade/TradeRouteDataService.cpp
    trade/TradeScoring.h trade/TradeScoring.cpp
    news/NewsRumorEditor.h news/NewsRumorEditor.cpp
    ids/IdsEditorPage.h ids/IdsEditorPage.cpp
    modmanager/ModManagerPage.h modmanager/ModManagerPage.cpp
    modmanager/ConflictDetector.h modmanager/ConflictDetector.cpp
    modmanager/ModWorkflow.h modmanager/ModWorkflow.cpp
    jump/JumpConnectionDialog.h jump/JumpConnectionDialog.cpp
)

target_include_directories(flatlas_editors PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(flatlas_editors PUBLIC
    flatlas_domain
    flatlas_infrastructure
    flatlas_rendering
    flatlas_tools
    Qt6::Core
    Qt6::Widgets
)
