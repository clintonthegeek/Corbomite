cmake_minimum_required(VERSION 3.19)
project(forcegraph VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets)

add_library(forcegraph SHARED
    src/QuadTree.cpp
    src/ForceLayoutEngine.cpp
    src/ForceGraphView.cpp
    src/ForceGraphScene.cpp
    src/ForceGraphNode.cpp
    src/ForceGraphEdge.cpp
    src/BatchNodeItem.cpp
    src/BatchEdgeItem.cpp
    src/MultilevelLayout.cpp
    include/forcegraph/ForceLayoutEngine.h
    include/forcegraph/QuadTree.h
    include/forcegraph/ForceGraphView.h
    include/forcegraph/ForceGraphScene.h
    include/forcegraph/ForceGraphNode.h
    include/forcegraph/ForceGraphEdge.h
    include/forcegraph/BatchNodeItem.h
    include/forcegraph/BatchEdgeItem.h
    include/forcegraph/MultilevelLayout.h
    include/forcegraph/SpatialGrid.h
)
set_target_properties(forcegraph PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(forcegraph
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(forcegraph PUBLIC Qt6::Core Qt6::Widgets)

# Install rule — needed now that the app carries a runtime DT_NEEDED on
# this library instead of embedding it statically. Not part of the
# CorbomiteTargets export set (forcegraph has zero Corbomite:: deps and no
# Corbomite:: alias; see the CLAUDE.md library table), so this is a plain
# install rather than the EXPORT CorbomiteTargets pattern used by
# libs/core, libs/storage, etc.
if(DEFINED KDE_INSTALL_LIBDIR)
    install(TARGETS forcegraph
        RUNTIME DESTINATION ${KDE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${KDE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${KDE_INSTALL_LIBDIR}
    )
endif()

if(NOT PROJECT_IS_TOP_LEVEL)
    # In-tree build — tests managed by parent
else()
    enable_testing()
    add_subdirectory(tests)
endif()
