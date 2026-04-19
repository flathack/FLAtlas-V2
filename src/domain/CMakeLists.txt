# flatlas_domain – Fachliche Datenmodelle (UI-frei)
add_library(flatlas_domain STATIC
    SystemDocument.h SystemDocument.cpp
    SolarObject.h SolarObject.cpp
    ZoneItem.h ZoneItem.cpp
    UniverseData.h UniverseData.cpp
    BaseData.h BaseData.cpp
    TradeRoute.h TradeRoute.cpp
    NpcData.h NpcData.cpp
    ModProfile.h ModProfile.cpp
)

target_include_directories(flatlas_domain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(flatlas_domain PUBLIC flatlas_core Qt6::Core)
