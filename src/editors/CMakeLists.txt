# flatlas_editors – Alle Editor-Pages
add_library(flatlas_editors STATIC
    system/SystemEditorPage.h system/SystemEditorPage.cpp
    system/SystemPersistence.h system/SystemPersistence.cpp
    system/SystemCreationWizard.h system/SystemCreationWizard.cpp
    system/SystemUndoCommands.h
    universe/UniverseEditorPage.h universe/UniverseEditorPage.cpp
    universe/UniverseSerializer.h universe/UniverseSerializer.cpp
    base/BaseEditorPage.h base/BaseEditorPage.cpp
    base/BasePersistence.h base/BasePersistence.cpp
    base/BaseBuilder.h base/BaseBuilder.cpp
    base/RoomEditor.h base/RoomEditor.cpp
    npc/NpcEditorPage.h npc/NpcEditorPage.cpp
    ini/IniEditorPage.h ini/IniEditorPage.cpp
    ini/IniCodeEditor.h ini/IniCodeEditor.cpp
    ini/IniSyntaxHighlighter.h ini/IniSyntaxHighlighter.cpp
    infocard/InfocardEditor.h infocard/InfocardEditor.cpp
    trade/TradeRoutePage.h trade/TradeRoutePage.cpp
    trade/MarketScanner.h trade/MarketScanner.cpp
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
    Qt6::Core
    Qt6::Widgets
)
