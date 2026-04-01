cmake_minimum_required(VERSION 3.19)
project(corbomite-core VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets)

add_library(corbomite-core STATIC
    src/NoteMeta.cpp
    src/NoteDocument.cpp
    include/corbomite/core/NoteDocument.h
    src/MarkdownRenderer.cpp
    src/RenderedDocument.cpp
    src/MarkdownRenderEngine.cpp
    src/RegexRenderEngine.cpp
    src/MarkoffRenderEngine.cpp
)
set_target_properties(corbomite-core PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(Corbomite::Core ALIAS corbomite-core)

target_include_directories(corbomite-core
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(corbomite-core PUBLIC Qt6::Core Qt6::Gui Qt6::Svg Qt6::Widgets KF6::SyntaxHighlighting jkqtmathtext mmdr markoff)
