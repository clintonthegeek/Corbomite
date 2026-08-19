cmake_minimum_required(VERSION 3.19)
project(canvas VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets Svg)

add_library(canvas STATIC
    src/CanvasDocument.cpp
    src/CanvasView.cpp
    src/CanvasScene.cpp
    src/CanvasCommands.cpp
    src/CanvasNodeItem.cpp
    src/CanvasResizeTool.cpp
    src/TextCardItem.cpp
    src/GroupItem.cpp
    src/EdgeItem.cpp
    src/FileCardItem.cpp
    src/CanvasFilePickerDialog.cpp
    include/canvas/CanvasDocument.h
    include/canvas/CanvasView.h
    include/canvas/CanvasScene.h
    include/canvas/CanvasCommands.h
    include/canvas/CanvasNodeItem.h
    include/canvas/CanvasResizeTool.h
    include/canvas/TextCardItem.h
    include/canvas/GroupItem.h
    include/canvas/EdgeItem.h
    include/canvas/FileCardItem.h
    include/canvas/CanvasFilePickerDialog.h
)
set_target_properties(canvas PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(canvas
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(canvas PUBLIC Qt6::Core Qt6::Widgets Qt6::Svg Corbomite::Core Graffodil::Core KF6::I18n Corbomite::Search)

if(NOT PROJECT_IS_TOP_LEVEL)
    # In-tree build
else()
    enable_testing()
    add_subdirectory(tests)
endif()
