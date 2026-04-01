cmake_minimum_required(VERSION 3.19)
project(markoff VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets)
find_package(KF6SyntaxHighlighting REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(TREESITTER REQUIRED IMPORTED_TARGET tree-sitter)
pkg_check_modules(MD4C REQUIRED IMPORTED_TARGET md4c)  # kept temporarily for Renderer

# Tree-sitter markdown grammar (vendored, will be forked for Obsidian extensions)
set(TS_MD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/tree-sitter-markdown)
add_library(ts-markdown STATIC
    ${TS_MD_DIR}/tree-sitter-markdown/src/parser.c
    ${TS_MD_DIR}/tree-sitter-markdown/src/scanner.c
    ${TS_MD_DIR}/tree-sitter-markdown-inline/src/parser.c
    ${TS_MD_DIR}/tree-sitter-markdown-inline/src/scanner.c
)
target_include_directories(ts-markdown PRIVATE
    ${TS_MD_DIR}/tree-sitter-markdown/src
    ${TS_MD_DIR}/tree-sitter-markdown-inline/src
)
# Enable extensions: wiki links, tags, LaTeX math, YAML frontmatter, GFM
target_compile_definitions(ts-markdown PRIVATE
    EXTENSION_WIKI_LINK
    EXTENSION_TAGS
)
set_target_properties(ts-markdown PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Markoff library
add_library(markoff STATIC
    include/markoff/Editor.h
    include/markoff/ReadingView.h
    src/Document.cpp
    src/DocumentBuilder.cpp
    src/TreeSitterParser.cpp
    src/Renderer.cpp
    src/ReadingView.cpp
    src/TextControl.cpp
    src/Editor.cpp
    src/MarkdownHighlighter.cpp
    src/SourceSpan.cpp
    src/AtomicBlock.cpp
    src/DecoratedRange.cpp
    src/TableHandler.cpp
)
set_target_properties(markoff PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(Markoff::Markoff ALIAS markoff)

target_include_directories(markoff
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${TS_MD_DIR}/tree-sitter-markdown/bindings/c
            ${TS_MD_DIR}/tree-sitter-markdown-inline/bindings/c
)

target_link_libraries(markoff
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
    PRIVATE
        PkgConfig::TREESITTER
        PkgConfig::MD4C
        ts-markdown
        KF6::SyntaxHighlighting
        jkqtmathtext
)

add_subdirectory(tests)
add_subdirectory(app)
