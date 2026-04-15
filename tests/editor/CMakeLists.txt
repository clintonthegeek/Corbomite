cmake_minimum_required(VERSION 3.19)
project(Corbomite_EditorTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test Widgets)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Phase 2 shim: Corbomite::SourceEditor wraps Qutepart::Qutepart and exposes
# the Corbomite-facing API (cursor, visual-line float scroll, fold scaffold,
# read-only). The test builds the two shim source files directly so it does
# not need to pull in CorbomiteApp (which has many unrelated deps).
add_executable(tst_source_editor
    tst_source_editor.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/SourceEditor.cpp
)
target_include_directories(tst_source_editor PRIVATE
    ${CMAKE_SOURCE_DIR}/src/editor
)
target_link_libraries(tst_source_editor PRIVATE
    Qt6::Test
    Qt6::Widgets
    Corbomite::QutepartSource
)
add_test(NAME tst_source_editor COMMAND tst_source_editor)
set_tests_properties(tst_source_editor PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
