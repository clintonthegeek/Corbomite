cmake_minimum_required(VERSION 3.19)
project(forcegraph VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets)

add_library(forcegraph STATIC
    src/QuadTree.cpp
    src/ForceLayoutEngine.cpp
    src/ForceGraphView.cpp
    src/ForceGraphScene.cpp
    src/ForceGraphNode.cpp
    src/ForceGraphEdge.cpp
    include/forcegraph/ForceLayoutEngine.h
    include/forcegraph/QuadTree.h
    include/forcegraph/ForceGraphView.h
    include/forcegraph/ForceGraphScene.h
    include/forcegraph/ForceGraphNode.h
    include/forcegraph/ForceGraphEdge.h
)
set_target_properties(forcegraph PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(forcegraph
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(forcegraph PUBLIC Qt6::Core Qt6::Widgets)

if(NOT PROJECT_IS_TOP_LEVEL)
    # In-tree build — tests managed by parent
else()
    enable_testing()
    add_subdirectory(tests)
endif()
