cmake_minimum_required(VERSION 3.19)
project(corbomite-forcegraph VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets)

add_library(corbomite-forcegraph SHARED
    src/QuadTree.cpp
    src/ForceLayoutEngine.cpp
    src/ForceGraphView.cpp
    src/ForceGraphScene.cpp
    src/ForceGraphNode.cpp
    src/ForceGraphEdge.cpp
    src/BatchNodeItem.cpp
    src/BatchEdgeItem.cpp
    src/MultilevelLayout.cpp
    include/corbomite/forcegraph/ForceLayoutEngine.h
    include/corbomite/forcegraph/QuadTree.h
    include/corbomite/forcegraph/ForceGraphView.h
    include/corbomite/forcegraph/ForceGraphScene.h
    include/corbomite/forcegraph/ForceGraphNode.h
    include/corbomite/forcegraph/ForceGraphEdge.h
    include/corbomite/forcegraph/BatchNodeItem.h
    include/corbomite/forcegraph/BatchEdgeItem.h
    include/corbomite/forcegraph/MultilevelLayout.h
    include/corbomite/forcegraph/SpatialGrid.h
)
set_target_properties(corbomite-forcegraph PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    EXPORT_NAME ForceGraph)
add_library(Corbomite::ForceGraph ALIAS corbomite-forcegraph)

target_include_directories(corbomite-forcegraph
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(corbomite-forcegraph PUBLIC Qt6::Core Qt6::Widgets)

# Install rules — public consumption via find_package(Corbomite), the same
# pattern as libs/core, libs/storage, etc. This library has no Corbomite::
# dependency and no third-party public header, so it exports cleanly.
if(DEFINED KDE_INSTALL_INCLUDEDIR)
    install(TARGETS corbomite-forcegraph
        EXPORT CorbomiteTargets
        RUNTIME DESTINATION ${KDE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${KDE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${KDE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${KDE_INSTALL_INCLUDEDIR}
    )
    install(DIRECTORY include/corbomite/forcegraph/
            DESTINATION ${KDE_INSTALL_INCLUDEDIR}/corbomite/forcegraph)
endif()

if(CORBOMITE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
