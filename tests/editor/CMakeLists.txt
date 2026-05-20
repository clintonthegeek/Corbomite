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
    Markoff::Source
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
    markoff_live
    Markoff::Source
)
add_test(NAME tst_note_editor_widget_ephemeral COMMAND tst_note_editor_widget_ephemeral)
set_tests_properties(tst_note_editor_widget_ephemeral PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# Phase C3 — NoteEditorWidget signal-driven mode-transition glue. Covers
# Source↔LivePreview↔Reading pairs + canonical-bytes preservation across swap
# + viewModeChanged signal shape.
add_executable(tst_note_editor_widget_mode_transition
    tst_note_editor_widget_mode_transition.cpp
)
target_include_directories(tst_note_editor_widget_mode_transition PRIVATE
    ${CMAKE_SOURCE_DIR}/src/editor
)
target_link_libraries(tst_note_editor_widget_mode_transition PRIVATE
    Qt6::Test
    Qt6::Widgets
    CorbomiteApp
    markoff_live
    # TODO(port-foundation-exploration): Markoff::Reading retired.
    #   Markoff::Reading
    Markoff::Source
    Corbomite::Core
)
add_test(NAME tst_note_editor_widget_mode_transition
         COMMAND tst_note_editor_widget_mode_transition)
set_tests_properties(tst_note_editor_widget_mode_transition PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# Cluster R Task 3.2 — MarkdownView::insertFrontmatterProperty ensures a
# YAML frontmatter block exists and appends a blank property row. Links
# against CorbomiteApp (for MarkdownView + NoteEditorWidget) plus the view
# hierarchy from Corbomite::Core.
add_executable(tst_markdown_view
    tst_markdown_view.cpp
)
target_include_directories(tst_markdown_view PRIVATE
    ${CMAKE_SOURCE_DIR}/src/editor
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(tst_markdown_view PRIVATE
    Qt6::Test
    Qt6::Widgets
    CorbomiteApp
    Corbomite::Core
)
add_test(NAME tst_markdown_view COMMAND tst_markdown_view)
set_tests_properties(tst_markdown_view PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# C4 Task 14 — NoteEditorWidget mermaid renderer injection. Verifies that
# setMermaidRenderer() propagates to the Live leaf (eagerly) and to the
# Reading leaf (when lazily constructed on first ViewMode::Reading switch).
# Also verifies that injecting after the Reading leaf exists updates it.
add_executable(tst_mermaid_injection
    tst_mermaid_injection.cpp
)
target_include_directories(tst_mermaid_injection PRIVATE
    ${CMAKE_SOURCE_DIR}/src/editor
)
target_link_libraries(tst_mermaid_injection PRIVATE
    Qt6::Test
    Qt6::Widgets
    CorbomiteApp
    markoff_live
    # TODO(port-foundation-exploration): Markoff::Reading retired.
    #   Markoff::Reading
    Markoff::Core
    Corbomite::Core
)
add_test(NAME tst_mermaid_injection COMMAND tst_mermaid_injection)
set_tests_properties(tst_mermaid_injection PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# TODO(port-foundation-exploration): HoverPopover currently uses
# Markoff::Reading::ReadingView, which is retired in foundation-exploration.
# Both tests below are disabled until HoverPopover is reworked against
# Live-with-editing-disabled (E1) or a markoff-reading-lite leaf is reintroduced.
if(FALSE)

# Cluster J Phase 6 — HoverPopover renders rich content via EmbedRenderer.
# The popover is part of CorbomiteApp, but the test only exercises a small
# rendering surface; it pulls the .cpp/.h directly to keep link surface
# small (HoverPopover only depends on Core, ReadingView, and Models).
add_executable(tst_hover_popover_render
    tst_hover_popover_render.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/HoverPopover.cpp
)
target_include_directories(tst_hover_popover_render PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(tst_hover_popover_render PRIVATE
    Qt6::Test
    Qt6::Widgets
    Corbomite::Core
    Corbomite::Models
    Markoff::Reading
)
add_test(NAME tst_hover_popover_render COMMAND tst_hover_popover_render)
set_tests_properties(tst_hover_popover_render PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# 2026-04-27 — HoverPopover Mod-key pinning + 500ms grace timer
# (P2 #4 sub-items 1+4). Same link surface as tst_hover_popover_render.
add_executable(tst_hover_popover_pinning
    tst_hover_popover_pinning.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/HoverPopover.cpp
)
target_include_directories(tst_hover_popover_pinning PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(tst_hover_popover_pinning PRIVATE
    Qt6::Test
    Qt6::Widgets
    Corbomite::Core
    Corbomite::Models
    Markoff::Reading
)
add_test(NAME tst_hover_popover_pinning COMMAND tst_hover_popover_pinning)
set_tests_properties(tst_hover_popover_pinning PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

endif()  # FALSE — HoverPopover tests disabled pending port
