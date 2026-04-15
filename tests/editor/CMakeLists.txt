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

# Cluster E Phase 1 — ViewModeSerializer is a pure translation between
# NoteEditorWidget::ViewMode and Obsidian's {mode, source} compound.
# NoteEditorWidget is a Q_OBJECT that lives in CorbomiteApp, so we link
# against that static library to pick up the metaobject + enum symbols
# without having to rebuild the widget's transitive-dep graph here.
add_executable(tst_view_mode_serializer
    tst_view_mode_serializer.cpp
)
target_include_directories(tst_view_mode_serializer PRIVATE
    ${CMAKE_SOURCE_DIR}/src/editor
)
target_link_libraries(tst_view_mode_serializer PRIVATE
    Qt6::Test
    Qt6::Widgets
    CorbomiteApp
)
add_test(NAME tst_view_mode_serializer COMMAND tst_view_mode_serializer)
set_tests_properties(tst_view_mode_serializer PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# Cluster E Phase 2 — NoteEditorWidget saveEphemeralState / restoreEphemeralState
# round-trips through Source / LivePreview / Reading targets. Links against
# CorbomiteApp (for NoteEditorWidget) plus Markoff (for the LivePreview editor
# accessor) — the Source + Reading widgets are reached via the widget itself.
add_executable(tst_note_editor_widget_ephemeral
    tst_note_editor_widget_ephemeral.cpp
)
target_include_directories(tst_note_editor_widget_ephemeral PRIVATE
    ${CMAKE_SOURCE_DIR}/src/editor
)
target_link_libraries(tst_note_editor_widget_ephemeral PRIVATE
    Qt6::Test
    Qt6::Widgets
    CorbomiteApp
    Markoff::Markoff
)
add_test(NAME tst_note_editor_widget_ephemeral COMMAND tst_note_editor_widget_ephemeral)
set_tests_properties(tst_note_editor_widget_ephemeral PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
