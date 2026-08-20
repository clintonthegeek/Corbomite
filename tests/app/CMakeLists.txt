cmake_minimum_required(VERSION 3.19)
project(Corbomite_AppTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test Widgets)
find_package(KF6ColorScheme REQUIRED)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Punch-list P2 (audit-2026-06-10) — canvas file-card paths must resolve
# against the vault root, not the .canvas file's own directory.
add_executable(tst_canvasviewtab_vaultroot_paths tst_canvasviewtab_vaultroot_paths.cpp)
add_test(NAME tst_canvasviewtab_vaultroot_paths COMMAND tst_canvasviewtab_vaultroot_paths)
target_link_libraries(tst_canvasviewtab_vaultroot_paths PRIVATE
    Qt6::Test CorbomiteApp Corbomite::Core)
set_tests_properties(tst_canvasviewtab_vaultroot_paths PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_recent_vaults tst_recent_vaults.cpp)
add_test(NAME tst_recent_vaults COMMAND tst_recent_vaults)
target_link_libraries(tst_recent_vaults PRIVATE Qt6::Test CorbomiteApp)
set_tests_properties(tst_recent_vaults PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Cluster R Task 3.3 — ExportToPdf wraps QPrinter+QTextDocument to write
# PDFs for a TFile via Vault::read. The test uses the no-dialog seam.
add_executable(tst_export_to_pdf tst_export_to_pdf.cpp)
add_test(NAME tst_export_to_pdf COMMAND tst_export_to_pdf)
target_link_libraries(tst_export_to_pdf PRIVATE
    Qt6::Test CorbomiteApp Corbomite::Vault Corbomite::Storage)
set_tests_properties(tst_export_to_pdf PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Cluster V Task 1.7 — smoke test for MainWindow::applyTheme()'s
# KColorScheme preconditions (manager + BreezeLight/BreezeDark schemes).
add_executable(tst_theme_applier tst_theme_applier.cpp)
add_test(NAME tst_theme_applier COMMAND tst_theme_applier)
target_link_libraries(tst_theme_applier PRIVATE
    Qt6::Test Qt6::Widgets KF6::ColorScheme)
set_tests_properties(tst_theme_applier PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Cluster R Task 17 — SystemThemeBuilder builds Markoff::Theme from KColorScheme.
# TODO(port-foundation-exploration): references retired Markoff::Theme shape
# (name/elements fields + Markoff::Element enum, all gone in the rebuild).
# Re-enable when the Theme port lands and the test is updated to the new shape.
if(FALSE)
add_executable(tst_system_theme_builder tst_system_theme_builder.cpp)
add_test(NAME tst_system_theme_builder COMMAND tst_system_theme_builder)
target_link_libraries(tst_system_theme_builder PRIVATE
    Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_system_theme_builder PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# ThemeService — find-highlight regression guard (2026-06-14). The bulk of
# the original suite is retired (old Element/elements Theme shape); this is a
# single focused test against the current stub's currentTheme() contract.
add_executable(tst_theme_service tst_theme_service.cpp)
add_test(NAME tst_theme_service COMMAND tst_theme_service)
target_link_libraries(tst_theme_service PRIVATE
    Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_theme_service PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Cluster V Task 2.8 — introspect the Cluster V Phase 2+3 KActionCollection
# wiring (Format/Heading/Insert/Table/Fold/Edit-find actions, radio groups).
# TODO(port-foundation-exploration): references retired Markoff::ActionId
# values + EditorContext shape. Re-enable per feature port as each action
# slice comes back.
if(FALSE)
add_executable(tst_mainwindow_action_wiring tst_mainwindow_action_wiring.cpp)
add_test(NAME tst_mainwindow_action_wiring COMMAND tst_mainwindow_action_wiring)
target_link_libraries(tst_mainwindow_action_wiring PRIVATE
    Qt6::Test CorbomiteApp)
set_tests_properties(tst_mainwindow_action_wiring PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

# Cluster V.2 Phase 2 — pin the round-trip semantics of the three
# VaultConfig::mergeJson calls MainWindow::applyVaultPortableSettings()
# performs (appearance / daily-notes / templates JSON). Exercising the
# dispatcher would require a full MainWindow Qt loop; this test pins the
# wire-format and unknown-key-preservation guarantees instead.
add_executable(tst_mainwindow_settings_apply tst_mainwindow_settings_apply.cpp)
add_test(NAME tst_mainwindow_settings_apply COMMAND tst_mainwindow_settings_apply)
target_link_libraries(tst_mainwindow_settings_apply PRIVATE
    Qt6::Test Corbomite::Storage CorbomiteApp)
set_tests_properties(tst_mainwindow_settings_apply PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Task 0.2 Finding 1 — MainWindow wikilink navigation dispatch.
# Verifies the navigation lambda in propagateServicesToView (88ad1b46):
# (a) link to an existing note navigates to it, and
# (b) link to a missing note creates the .md file on disk (create-on-click).
# Runs offscreen — no display server required.
add_executable(tst_mainwindow_link_navigation
    tst_mainwindow_link_navigation.cpp)
add_test(NAME tst_mainwindow_link_navigation COMMAND tst_mainwindow_link_navigation)
target_link_libraries(tst_mainwindow_link_navigation PRIVATE
    Qt6::Test CorbomiteApp)
set_tests_properties(tst_mainwindow_link_navigation PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Cluster O Phase O1 — ActionContextController: the phase's acceptance
# gate (tst_action_context_no_silent_noop, run inside noSilentNoop()),
# plus the two named tests for O1.T2 (in-place view-type swap) and O1.T4
# (Ctrl+S must actually save a canvas tab).
add_executable(tst_action_context tst_action_context.cpp)
add_test(NAME tst_action_context COMMAND tst_action_context)
target_link_libraries(tst_action_context PRIVATE
    Qt6::Test CorbomiteApp Corbomite::Core Corbomite::Bases canvas)
set_tests_properties(tst_action_context PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Cluster O Phase O1.T3 — re-lit View::zoomIn/Out/Reset polymorphic
# dispatch (markdown + canvas; graph coverage lives in
# src/plugins/graph-view/tests/tst_graphview_plugin.cpp).
add_executable(tst_view_zoom_dispatch tst_view_zoom_dispatch.cpp)
add_test(NAME tst_view_zoom_dispatch COMMAND tst_view_zoom_dispatch)
target_link_libraries(tst_view_zoom_dispatch PRIVATE
    Qt6::Test CorbomiteApp Corbomite::Core canvas)
set_tests_properties(tst_view_zoom_dispatch PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Cluster O Phase O3 (O3.T1/T2/T3) — the ViewActions provider mechanism:
# eager construction, install/uninstall leaves the collection clean (no
# dangling shortcuts).
add_executable(tst_view_actions_provider tst_view_actions_provider.cpp)
add_test(NAME tst_view_actions_provider COMMAND tst_view_actions_provider)
target_link_libraries(tst_view_actions_provider PRIVATE
    Qt6::Test CorbomiteApp Corbomite::Core canvas)
set_tests_properties(tst_view_actions_provider PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Cluster O Phase O3.T4 (doctrine §D4) — tri-state toolbar visibility
# policy: pure Auto/AlwaysShow/AlwaysHide logic table, plus a real
# MainWindow/KToolBar integration check in and out of context.
add_executable(tst_toolbar_policy tst_toolbar_policy.cpp)
add_test(NAME tst_toolbar_policy COMMAND tst_toolbar_policy)
target_link_libraries(tst_toolbar_policy PRIVATE
    Qt6::Test CorbomiteApp Corbomite::Core canvas)
set_tests_properties(tst_toolbar_policy PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Cluster O Phase O2.T1 — the View Tier-B capability surface
# (canEdit/canSave/canZoom/canFind/hasSelection/canUndo/canRedo). Graph
# coverage lives in src/plugins/graph-view/tests/tst_graphview_plugin.cpp
# (separate plugin target this binary doesn't link against).
add_executable(tst_view_capabilities tst_view_capabilities.cpp)
add_test(NAME tst_view_capabilities COMMAND tst_view_capabilities)
target_link_libraries(tst_view_capabilities PRIVATE
    Qt6::Test CorbomiteApp Corbomite::Core Corbomite::Bases canvas)
set_tests_properties(tst_view_capabilities PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Cluster K — canvas leaf's inline title band (rename-via-header
# affordance) surfaced in Corbomite: seeded from filename on attach,
# debounced edit commits a real FileManager rename, '/' in the typed text
# is rejected rather than silently becoming a folder move.
# LivePreview is always Markoff::Canvas (Cluster K Phase 5).
add_executable(tst_mainwindow_title_rename
    tst_mainwindow_title_rename.cpp)
add_test(NAME tst_mainwindow_title_rename COMMAND tst_mainwindow_title_rename)
target_link_libraries(tst_mainwindow_title_rename PRIVATE
    Qt6::Test CorbomiteApp)
set_tests_properties(tst_mainwindow_title_rename PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Task 0.3 P1 — LinkResolver freshness: verify that create/rename/delete in
# an open vault immediately update the app-level LinkResolver (no reopen
# needed), and that non-.md attachments are resolvable targets from vault open.
# Driven through the real Vault signal path, not by calling addVaultPath directly.
add_executable(tst_mainwindow_link_resolver_freshness
    tst_mainwindow_link_resolver_freshness.cpp)
add_test(NAME tst_mainwindow_link_resolver_freshness
         COMMAND tst_mainwindow_link_resolver_freshness)
target_link_libraries(tst_mainwindow_link_resolver_freshness PRIVATE
    Qt6::Test CorbomiteApp Corbomite::Vault Corbomite::Storage)
set_tests_properties(tst_mainwindow_link_resolver_freshness PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Task 0.7 P1 — DB location: verify that index.sqlite and metadata-cache.db
# are written under AppLocalDataLocation (not inside the vault/.obsidian/),
# and that legacy in-vault DB files are removed on vault open.
add_executable(tst_mainwindow_db_paths tst_mainwindow_db_paths.cpp)
add_test(NAME tst_mainwindow_db_paths COMMAND tst_mainwindow_db_paths)
target_link_libraries(tst_mainwindow_db_paths PRIVATE
    Qt6::Test CorbomiteApp Corbomite::Core)
set_tests_properties(tst_mainwindow_db_paths PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Phase C7 Task 14 — pin the Cluster R hamburger Find/Replace dispatch
# (Task 12) and MainWindow's edit_find_next/_previous/_replace QAction
# routing into the active leaf (Task 13). Synthetic MarkdownView wrapper
# in Source mode exercises the same code paths the MainWindow shell calls.
# TODO(port-foundation-exploration): this test predates the FindController
# port. The find-dispatch contract is now Markoff::FindController, not
# MarkdownView virtuals. Replace this test with one exercising the new
# Ctrl+F → showFindBar → controller path (port follow-up).
if(FALSE)
add_executable(tst_markdownview_find_replace_dispatch
    tst_markdownview_find_replace_dispatch.cpp)
target_include_directories(tst_markdownview_find_replace_dispatch PRIVATE
    ${CMAKE_SOURCE_DIR}/src/editor)
add_test(NAME tst_markdownview_find_replace_dispatch
         COMMAND tst_markdownview_find_replace_dispatch)
target_link_libraries(tst_markdownview_find_replace_dispatch PRIVATE
    Qt6::Test CorbomiteApp Corbomite::Core Markoff::Source Markoff::Core)
set_tests_properties(tst_markdownview_find_replace_dispatch PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
