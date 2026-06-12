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

# Phase 1 (contract v2) — NoteEditorWidget saveEphemeralState /
# restoreEphemeralState + goToLine round-trip through Source / LivePreview /
# Reading via the MarkdownView base (CursorPos + 0.0–1.0 scroll fraction).
# Rewritten against the contract-v2 base (re-enabled 2026-06-10).
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
    markoff_styled
    Markoff::Source
    Corbomite::Core
)
add_test(NAME tst_note_editor_widget_ephemeral COMMAND tst_note_editor_widget_ephemeral)
set_tests_properties(tst_note_editor_widget_ephemeral PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# Completion revival A1 exit — CompletionController against the REAL Live leaf
# (real QML caretRect, real document propagation): trigger -> popup -> accept
# -> single-undo, end-to-end.
add_executable(tst_note_editor_widget_completion
    tst_note_editor_widget_completion.cpp
)
target_include_directories(tst_note_editor_widget_completion PRIVATE
    ${CMAKE_SOURCE_DIR}/src/editor
)
target_link_libraries(tst_note_editor_widget_completion PRIVATE
    Qt6::Test
    Qt6::Widgets
    CorbomiteApp
    markoff_live
    markoff_styled
    Markoff::Source
    Corbomite::Core
)
add_test(NAME tst_note_editor_widget_completion COMMAND tst_note_editor_widget_completion)
set_tests_properties(tst_note_editor_widget_completion PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# Regression: NoteDocument freed under a live NoteEditorWidget must not crash
# (2026-06-10 first-run SIGSEGV; vault teardown frees the doc while the live
# leaf's deferred QML seed is pending).
add_executable(tst_note_editor_widget_doc_lifetime
    tst_note_editor_widget_doc_lifetime.cpp
)
target_include_directories(tst_note_editor_widget_doc_lifetime PRIVATE
    ${CMAKE_SOURCE_DIR}/src/editor
)
target_link_libraries(tst_note_editor_widget_doc_lifetime PRIVATE
    Qt6::Test
    Qt6::Widgets
    CorbomiteApp
    markoff_live
    markoff_styled
    Markoff::Source
    Corbomite::Core
)
add_test(NAME tst_note_editor_widget_doc_lifetime COMMAND tst_note_editor_widget_doc_lifetime)
set_tests_properties(tst_note_editor_widget_doc_lifetime PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# Phase 2 (road-to-dogfood) — status-bar word count honesty: attaching a
# document seeds the count and every edit re-emits a fresh count on
# cursorInfoChanged.
add_executable(tst_note_editor_widget_word_count
    tst_note_editor_widget_word_count.cpp
)
target_include_directories(tst_note_editor_widget_word_count PRIVATE
    ${CMAKE_SOURCE_DIR}/src/editor
)
target_link_libraries(tst_note_editor_widget_word_count PRIVATE
    Qt6::Test
    Qt6::Widgets
    CorbomiteApp
    markoff_live
    markoff_styled
    Markoff::Source
    Corbomite::Core
)
add_test(NAME tst_note_editor_widget_word_count COMMAND tst_note_editor_widget_word_count)
set_tests_properties(tst_note_editor_widget_word_count PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# Phase 2 (road-to-dogfood) — template-at-cursor: insertAtCursor lands the
# text at the caret (one D2 edit) instead of appending at end-of-document.
add_executable(tst_note_editor_widget_insert_at_cursor
    tst_note_editor_widget_insert_at_cursor.cpp
)
target_include_directories(tst_note_editor_widget_insert_at_cursor PRIVATE
    ${CMAKE_SOURCE_DIR}/src/editor
)
target_link_libraries(tst_note_editor_widget_insert_at_cursor PRIVATE
    Qt6::Test
    Qt6::Widgets
    CorbomiteApp
    markoff_live
    markoff_styled
    Markoff::Source
    Corbomite::Core
)
add_test(NAME tst_note_editor_widget_insert_at_cursor COMMAND tst_note_editor_widget_insert_at_cursor)
set_tests_properties(tst_note_editor_widget_insert_at_cursor PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# Phase 1 (contract v2) — NoteEditorWidget signal-driven mode-transition
# glue: cursorInfoChanged on leaf cursor move, canonical-bytes preservation
# across swap, leaves reflecting canonical on attach, viewModeChanged shape.
# Rewritten against the MarkdownView base contract (re-enabled 2026-06-10).
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
# TODO(port-foundation-exploration): Markoff::MermaidRenderer abstract +
# setMermaidRenderer() retired with Reading leaf (E5 work). Re-enable when
# the renderer-unification port lands.
if(FALSE)
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
    Markoff::Core
    Corbomite::Core
)
add_test(NAME tst_mermaid_injection COMMAND tst_mermaid_injection)
set_tests_properties(tst_mermaid_injection PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
endif()

# Hover preview re-light (2026-06-11) — HoverPopover renders via
# StyledRenderEngine into a QTextBrowser. Both tests re-enabled.

# tst_hover_popover_render: 3 slots verifying resolve → render → display.
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
    markoff_styled
    markoff_core
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
    markoff_styled
    markoff_core
)
add_test(NAME tst_hover_popover_pinning COMMAND tst_hover_popover_pinning)
set_tests_properties(tst_hover_popover_pinning PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# Find UI port (2026-05-20) — host-owned FindBar consumes Markoff::FindController.
add_executable(tst_findbar
    tst_findbar.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/FindBar.cpp
    ${CMAKE_SOURCE_DIR}/src/editor/FindBar.h
)
target_include_directories(tst_findbar PRIVATE
    ${CMAKE_SOURCE_DIR}/src/editor
)
target_link_libraries(tst_findbar PRIVATE
    Qt6::Test
    Qt6::Widgets
    Markoff::Core
)
add_test(NAME tst_findbar COMMAND tst_findbar)
set_tests_properties(tst_findbar PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# 2026-05-29 — Reading mode is a read-only Markoff::Styled::Editor leaf.
add_executable(tst_reading_styled_leaf
    tst_reading_styled_leaf.cpp
)
target_include_directories(tst_reading_styled_leaf PRIVATE
    ${CMAKE_SOURCE_DIR}/src/editor
)
target_link_libraries(tst_reading_styled_leaf PRIVATE
    Qt6::Test
    Qt6::Widgets
    CorbomiteApp
    markoff_styled
    Corbomite::Core
)
add_test(NAME tst_reading_styled_leaf COMMAND tst_reading_styled_leaf)
set_tests_properties(tst_reading_styled_leaf PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# Task 0.2 — NoteEditorWidget::linkActivated is emitted when the Live
# binding's LinkService fires, covering both Live and Reading modes plus
# external-link suppression and shared-service no-double-emit invariants.
add_executable(tst_link_activation
    tst_link_activation.cpp
)
target_include_directories(tst_link_activation PRIVATE
    ${CMAKE_SOURCE_DIR}/src/editor
)
target_link_libraries(tst_link_activation PRIVATE
    Qt6::Test
    Qt6::Widgets
    CorbomiteApp
    markoff_live
    markoff_styled
    Corbomite::Core
)
add_test(NAME tst_link_activation COMMAND tst_link_activation)
set_tests_properties(tst_link_activation PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

# Completion revival A1 — WikiLinkSuggest names mode: trigger detection,
# replaceEnd (consuming a trailing "]]"), names universe with basename vs
# path disambiguation via LinkResolver::candidateCount.
add_executable(tst_wikilink_suggest tst_wikilink_suggest.cpp)
target_include_directories(tst_wikilink_suggest PRIVATE ${CMAKE_SOURCE_DIR}/src/editor)
target_link_libraries(tst_wikilink_suggest PRIVATE
    Qt6::Test Qt6::Widgets CorbomiteApp Corbomite::Core Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_wikilink_suggest COMMAND tst_wikilink_suggest)
set_tests_properties(tst_wikilink_suggest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Completion revival Task 8 — LineResolve: contract-v2 flat visual line ⟷
# (block, offsets, lineText) against a MarkoffDocument. Pure coordinate
# bridge; no widget deps. Links CorbomiteApp (for the LineResolve TU) +
# markoff_core (for MarkoffDocument).
add_executable(tst_line_resolve tst_line_resolve.cpp)
target_include_directories(tst_line_resolve PRIVATE ${CMAKE_SOURCE_DIR}/src/editor)
target_link_libraries(tst_line_resolve PRIVATE Qt6::Test Qt6::Widgets CorbomiteApp markoff_core)
add_test(NAME tst_line_resolve COMMAND tst_line_resolve)
set_tests_properties(tst_line_resolve PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Completion revival A1 — TagSuggest trigger detection + null-index empty
# universe. (Real-index allTags() path is exercised in storage tests.)
add_executable(tst_tag_suggest tst_tag_suggest.cpp)
target_include_directories(tst_tag_suggest PRIVATE ${CMAKE_SOURCE_DIR}/src/editor)
target_link_libraries(tst_tag_suggest PRIVATE
    Qt6::Test Qt6::Widgets CorbomiteApp Corbomite::Core Corbomite::Storage)
add_test(NAME tst_tag_suggest COMMAND tst_tag_suggest)
set_tests_properties(tst_tag_suggest PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Completion revival Task 9 — CompletionController: reactive trigger/dismiss
# over the MarkdownView base. A FakeLeaf supplies caretRect/cursorPosition; a
# real NoteDocument supplies the edit path; a stub '@' suggester isolates
# controller logic. The 2 accept-path slots are committed RED (Task 10 fills).
add_executable(tst_completion_controller tst_completion_controller.cpp)
target_include_directories(tst_completion_controller PRIVATE ${CMAKE_SOURCE_DIR}/src/editor)
target_link_libraries(tst_completion_controller PRIVATE
    Qt6::Test Qt6::Widgets CorbomiteApp Corbomite::Core markoff_core)
add_test(NAME tst_completion_controller COMMAND tst_completion_controller)
set_tests_properties(tst_completion_controller PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
