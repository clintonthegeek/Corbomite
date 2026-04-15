cmake_minimum_required(VERSION 3.19)
project(readingview VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets)
find_package(KF6SyntaxHighlighting REQUIRED)

# ReadingView library (Obsidian-compatible Reading-mode widget).
# Peer to libs/markoff — DOES NOT depend on Markoff.
add_library(readingview STATIC
    include/corbomite/readingview/CodeBlockHighlighter.h
    src/CodeBlockHighlighter.cpp
    include/corbomite/readingview/ReadingView.h
    src/ReadingView.cpp
)
set_target_properties(readingview PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(Corbomite::ReadingView ALIAS readingview)

target_include_directories(readingview
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(readingview
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        KF6::SyntaxHighlighting
)
# NOTE: KF6::SyntaxHighlighting is PUBLIC (not PRIVATE) because
# CodeBlockHighlighter.h currently exposes KSyntaxHighlighting types
# (AbstractHighlighter base class, Repository member). If later phases
# refactor the public header to hide these behind a pimpl / pure-virtual
# interface, this link can be downgraded to PRIVATE.

enable_testing()
add_subdirectory(tests)

# TODO(Phase 0c+): wire in the remaining external deps as the transplant
# progresses:
#   - MarkoffParser::MarkoffParser  (tree-sitter AST; Phase 1+)
#   - JKQTMathText                  (inline/block math; Phase ~3)
#   - mmdr                          (Mermaid rendering; Phase ~4)
#   - Corbomite::Core               (NoteMeta / NoteDocument shared types)
# Phase 0b is bootstrap-only — only KF6::SyntaxHighlighting + Qt are needed
# for the transplanted CodeBlockHighlighter.
