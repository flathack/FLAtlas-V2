# flatlas_infrastructure – Parser, IO, Freelancer-Pfade, Updater
add_library(flatlas_infrastructure STATIC
    parser/IniParser.h parser/IniParser.cpp
    parser/BiniDecoder.h parser/BiniDecoder.cpp
    parser/BiniConverter.h parser/BiniConverter.cpp
    parser/XmlInfocard.h parser/XmlInfocard.cpp
    io/CmpLoader.h io/CmpLoader.cpp
    io/VmeshDecoder.h io/VmeshDecoder.cpp
    io/TextureLoader.h io/TextureLoader.cpp
    io/DllResources.h io/DllResources.cpp
    io/PeVersionInfo.h io/PeVersionInfo.cpp
    io/DebugCapture.h io/DebugCapture.cpp
    io/CsvImporter.h io/CsvImporter.cpp
    freelancer/FreelancerPaths.h freelancer/FreelancerPaths.cpp
    freelancer/UniverseScanner.h freelancer/UniverseScanner.cpp
    freelancer/IdsStringTable.h freelancer/IdsStringTable.cpp
    updater/AutoUpdater.h updater/AutoUpdater.cpp
    net/DownloadManager.h net/DownloadManager.cpp
)

target_include_directories(flatlas_infrastructure PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(flatlas_infrastructure PUBLIC
    flatlas_core
    flatlas_domain
    Qt6::Core
    Qt6::Xml
    Qt6::Network
)

if(WIN32)
    target_link_libraries(flatlas_infrastructure PUBLIC version)
endif()
