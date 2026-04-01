cmake_minimum_required(VERSION 3.19)
project(markoff VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets)
find_package(KF6SyntaxHighlighting REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MD4C REQUIRED IMPORTED_TARGET md4c)

add_library(markoff STATIC
    include/markoff/Editor.h
    include/markoff/ReadingView.h
    src/Document.cpp
    src/DocumentBuilder.cpp
    src/Renderer.cpp
    src/ReadingView.cpp
    src/TextControl.cpp
    src/Editor.cpp
    src/MarkdownHighlighter.cpp
    src/AtomicBlock.cpp
    src/CodeAtomicBlock.cpp
)
set_target_properties(markoff PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(Markoff::Markoff ALIAS markoff)

target_include_directories(markoff
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(markoff
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
    PRIVATE
        PkgConfig::MD4C
        KF6::SyntaxHighlighting
        jkqtmathtext
)

add_subdirectory(tests)
add_subdirectory(app)
