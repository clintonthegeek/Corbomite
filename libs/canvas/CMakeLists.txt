cmake_minimum_required(VERSION 3.19)
project(corbomite-canvas VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets Svg)

add_library(corbomite-canvas SHARED
    src/CanvasDocument.cpp
    src/CanvasJsonPrinter.cpp
    src/CanvasDocumentRegistry.cpp
    src/CanvasLinkRewriter.cpp
    src/CanvasView.cpp
    src/CanvasScene.cpp
    src/CanvasCommands.cpp
    src/CanvasNodeItem.cpp
    src/CanvasNodeChromeOverlay.cpp
    src/CanvasResizeTool.cpp
    src/CanvasDuplicateDragTool.cpp
    src/CanvasEdgeGestureTool.cpp
    src/ReconnectEdgeTool.cpp
    src/TextCardItem.cpp
    src/GroupItem.cpp
    src/EdgeItem.cpp
    src/CanvasBezierStrategy.cpp
    src/CanvasAlignmentStrategy.cpp
    src/FileCardItem.cpp
    src/CanvasFilePickerDialog.cpp
    include/corbomite/canvas/CanvasDocument.h
    include/corbomite/canvas/CanvasJsonPrinter.h
    include/corbomite/canvas/CanvasDocumentRegistry.h
    include/corbomite/canvas/CanvasLinkRewriter.h
    include/corbomite/canvas/CanvasView.h
    include/corbomite/canvas/CanvasScene.h
    include/corbomite/canvas/CanvasCommands.h
    include/corbomite/canvas/CanvasNodeItem.h
    include/corbomite/canvas/CanvasNodeChromeOverlay.h
    include/corbomite/canvas/CanvasResizeTool.h
    include/corbomite/canvas/CanvasDuplicateDragTool.h
    include/corbomite/canvas/CanvasEdgeGestureTool.h
    include/corbomite/canvas/ReconnectEdgeTool.h
    include/corbomite/canvas/TextCardItem.h
    include/corbomite/canvas/GroupItem.h
    include/corbomite/canvas/EdgeItem.h
    include/corbomite/canvas/CanvasBezierStrategy.h
    include/corbomite/canvas/CanvasAlignmentStrategy.h
    include/corbomite/canvas/FileCardItem.h
    include/corbomite/canvas/CanvasFilePickerDialog.h
)
set_target_properties(corbomite-canvas PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    EXPORT_NAME Canvas)
add_library(Corbomite::Canvas ALIAS corbomite-canvas)

target_include_directories(corbomite-canvas
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(corbomite-canvas PUBLIC Qt6::Core Qt6::Widgets Qt6::Svg Corbomite::Core Graffodil::Core KF6::I18n Corbomite::Search)

# The card editor is Markoff's canvas leaf in embedded (card) mode — the
# widget the card view is built on. It is linked here (not through
# corbomite-core) because only this library embeds it.
target_link_libraries(corbomite-canvas PUBLIC Markoff::Canvas)

# Install rule — needed now that the app carries a runtime DT_NEEDED on
# this library instead of embedding it statically. Not part of the
# CorbomiteTargets export set: canvas's public headers include Graffodil's
# (<graffodil/GraphScene.h>), and Graffodil installs neither its headers
# nor an export set of its own — it carries its own runtime DT_NEEDED
# independently (libs/graffodil/src/core/CMakeLists.txt). So Corbomite::Canvas cannot
# be exported to third parties without first fixing Graffodil's install.
if(DEFINED KDE_INSTALL_LIBDIR)
    install(TARGETS corbomite-canvas
        RUNTIME DESTINATION ${KDE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${KDE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${KDE_INSTALL_LIBDIR}
    )
endif()

if(CORBOMITE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
