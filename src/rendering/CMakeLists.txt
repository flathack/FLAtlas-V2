# flatlas_rendering – 2D, 3D, Flight, Preview
add_library(flatlas_rendering STATIC
    view2d/SystemDisplayFilter.h view2d/SystemDisplayFilter.cpp
    view2d/MapScene.h view2d/MapScene.cpp
    view2d/SystemMapView.h view2d/SystemMapView.cpp
    view2d/items/SolarObjectItem.h view2d/items/SolarObjectItem.cpp
    view2d/items/ZoneItem2D.h view2d/items/ZoneItem2D.cpp
    view2d/items/TradelaneItem.h view2d/items/TradelaneItem.cpp
    view3d/SceneView3D.h view3d/SceneView3D.cpp
    view3d/OrbitCamera.h view3d/OrbitCamera.cpp
    view3d/ModelGeometryBuilder.h view3d/ModelGeometryBuilder.cpp
    view3d/ModelViewport3D.h view3d/ModelViewport3D.cpp
    view3d/SelectionManager.h view3d/SelectionManager.cpp
    view3d/SkyRenderer.h view3d/SkyRenderer.cpp
    view3d/MaterialFactory.h view3d/MaterialFactory.cpp
    flight/FlightController.h flight/FlightController.cpp
    flight/FlightCamera.h flight/FlightCamera.cpp
    flight/FlightHud.h flight/FlightHud.cpp
    flight/Autopilot.h flight/Autopilot.cpp
    flight/DustParticles.h flight/DustParticles.cpp
    flight/FlightNavigation.h flight/FlightNavigation.cpp
    preview/ModelPreview.h preview/ModelPreview.cpp
    preview/ModelViewerPage.h preview/ModelViewerPage.cpp
    preview/CharacterPreview.h preview/CharacterPreview.cpp
    preview/ModelCache.h preview/ModelCache.cpp
    pathgen/ShortestPath.h pathgen/ShortestPath.cpp
)

target_include_directories(flatlas_rendering PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(flatlas_rendering PUBLIC
    flatlas_domain
    flatlas_infrastructure
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
)

# Qt3D optional verlinken (Phase 7+)
if(TARGET Qt6::3DCore)
    target_link_libraries(flatlas_rendering PUBLIC
        Qt6::3DCore Qt6::3DRender Qt6::3DInput Qt6::3DExtras
    )
    target_compile_definitions(flatlas_rendering PUBLIC FLATLAS_HAS_QT3D)
endif()
