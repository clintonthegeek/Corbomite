cmake_minimum_required(VERSION 3.19)
project(Corbomite_IntegrationTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test Sql)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_executable(tst_vault_lifecycle tst_vault_lifecycle.cpp)
add_test(NAME tst_vault_lifecycle COMMAND tst_vault_lifecycle)
target_link_libraries(tst_vault_lifecycle PRIVATE
    Qt6::Test Corbomite::Models Corbomite::Storage Corbomite::Core)
set_tests_properties(tst_vault_lifecycle PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_editor_save tst_editor_save.cpp)
add_test(NAME tst_editor_save COMMAND tst_editor_save)
target_link_libraries(tst_editor_save PRIVATE
    Qt6::Test Corbomite::Models Corbomite::Storage Corbomite::Core)
set_tests_properties(tst_editor_save PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_search_dsl_pipeline tst_search_dsl_pipeline.cpp)
add_test(NAME tst_search_dsl_pipeline COMMAND tst_search_dsl_pipeline)
target_link_libraries(tst_search_dsl_pipeline PRIVATE
    Qt6::Test Corbomite::Search Corbomite::Storage Corbomite::Core)
set_tests_properties(tst_search_dsl_pipeline PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_cross_session tst_cross_session.cpp)
add_test(NAME tst_cross_session COMMAND tst_cross_session)
target_link_libraries(tst_cross_session PRIVATE
    Qt6::Test Qt6::Sql Corbomite::Models Corbomite::Storage Corbomite::Core)
set_tests_properties(tst_cross_session PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Cluster P Phase P4.T1/T2 — the plugin-boundary correctness gate. Loads
# the real corbomite-file-explorer.so via KPluginFactory (genuine dlopen,
# no test-only bypass) and qobject_casts a NotesTreeModel constructed
# inside it. Needs Qt6::Widgets (QTreeView) and Corbomite::Vault
# (PluginManager/PluginContext/Plugin/Vault/FileManager) on top of the
# other tests' deps, plus KF6::CoreAddons for KPluginFactory.
find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(KF6CoreAddons REQUIRED)
add_executable(tst_plugin_type_identity tst_plugin_type_identity.cpp)
add_test(NAME tst_plugin_type_identity COMMAND tst_plugin_type_identity)
target_link_libraries(tst_plugin_type_identity PRIVATE
    Qt6::Test Qt6::Widgets KF6::CoreAddons
    Corbomite::Models Corbomite::Storage Corbomite::Core Corbomite::Vault)
target_compile_definitions(tst_plugin_type_identity PRIVATE
    CORBOMITE_PLUGIN_DEV_DIR="${CORBOMITE_PLUGIN_DEV_DIR}")
set_tests_properties(tst_plugin_type_identity PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
# The plugin .so must exist on disk before this test can find it.
add_dependencies(tst_plugin_type_identity corbomite-file-explorer)

# Cluster M M6.2 — vault rename through FileManager with the real
# CanvasLinkRewriter (vault seam + Corbomite::Canvas rewriter composed).
add_executable(tst_canvas_rename_rewrite tst_canvas_rename_rewrite.cpp)
add_test(NAME tst_canvas_rename_rewrite COMMAND tst_canvas_rename_rewrite)
target_link_libraries(tst_canvas_rename_rewrite PRIVATE
    Qt6::Test Qt6::Widgets
    Corbomite::Storage Corbomite::Core Corbomite::Vault Corbomite::Canvas)
set_tests_properties(tst_canvas_rename_rewrite PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Canvas persistence, 2026-10-18 — debounced autosave and the external-edit
# policy (adopt when clean, keep local and warn when not). Uses the real
# CanvasViewTab against a temp-dir .canvas file.
add_executable(tst_canvas_persistence tst_canvas_persistence.cpp)
add_test(NAME tst_canvas_persistence COMMAND tst_canvas_persistence)
target_include_directories(tst_canvas_persistence PRIVATE
    ${CMAKE_SOURCE_DIR}/src/canvas
)
target_link_libraries(tst_canvas_persistence PRIVATE
    Qt6::Test Qt6::Widgets
    CorbomiteApp Corbomite::Canvas
)
set_tests_properties(tst_canvas_persistence PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Cluster F-a P3.6 — Note Composer pipeline (Vault + FileManager).
add_executable(tst_merge_file_pipeline tst_merge_file_pipeline.cpp)
add_test(NAME tst_merge_file_pipeline COMMAND tst_merge_file_pipeline)
target_link_libraries(tst_merge_file_pipeline PRIVATE
    Qt6::Test Qt6::Widgets
    Corbomite::Storage Corbomite::Core Corbomite::Vault)
set_tests_properties(tst_merge_file_pipeline PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_obsidian_command_ids tst_obsidian_command_ids.cpp)
add_test(NAME tst_obsidian_command_ids COMMAND tst_obsidian_command_ids)
target_link_libraries(tst_obsidian_command_ids PRIVATE
    Qt6::Test Qt6::Widgets KF6::CoreAddons
    Corbomite::Storage Corbomite::Core Corbomite::Vault)
target_compile_definitions(tst_obsidian_command_ids PRIVATE
    CORBOMITE_PLUGIN_DEV_DIR="${CORBOMITE_PLUGIN_DEV_DIR}"
    CORBOMITE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
set_tests_properties(tst_obsidian_command_ids PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
# The plugin .so files must exist on disk before this test can find them.
add_dependencies(tst_obsidian_command_ids
    corbomite-backlinks corbomite-outlinks corbomite-outline
    corbomite-properties corbomite-bookmarks corbomite-local-graph
    corbomite-file-explorer)
