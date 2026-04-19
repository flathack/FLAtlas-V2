# flatlas_rendering – 2D, 3D, Flight, Preview
add_library(flatlas_rendering STATIC
    view2d/MapScene.h view2d/MapScene.cpp
    view2d/SystemMapView.h view2d/SystemMapView.cpp
    view2d/items/SolarObjectItem.h view2d/items/SolarObjectItem.cpp
    view2d/items/ZoneItem2D.h view2d/items/ZoneItem2D.cpp
    view2d/items/TradelaneItem.h view2d/items/TradelaneItem.cpp
    view3d/SceneView3D.h view3d/SceneView3D.cpp
    flight/FlightController.h flight/FlightController.cpp
    preview/ModelPreview.h preview/ModelPreview.cpp
    preview/ModelCache.h preview/ModelCache.cpp
    pathgen/ShortestPath.h pathgen/ShortestPath.cpp
)

target_include_directories(flatlas_rendering PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(flatlas_rendering PUBLIC
    flatlas_domain
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
