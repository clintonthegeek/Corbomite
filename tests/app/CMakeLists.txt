cmake_minimum_required(VERSION 3.19)
project(Corbomite_AppTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test Widgets)
find_package(KF6ColorScheme REQUIRED)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

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

# C2 Task 18 — ThemeService unit tests
# TODO(port-foundation-exploration): same Theme-shape mismatch as
# tst_system_theme_builder. Re-enable with the Theme port.
if(FALSE)
add_executable(tst_theme_service tst_theme_service.cpp)
add_test(NAME tst_theme_service COMMAND tst_theme_service)
target_link_libraries(tst_theme_service PRIVATE
    Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_theme_service PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

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
