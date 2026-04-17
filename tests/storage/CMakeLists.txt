cmake_minimum_required(VERSION 3.19)
project(Corbomite_StorageTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test Concurrent Sql)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_executable(tst_filesystemadapter tst_filesystemadapter.cpp)
add_test(NAME tst_filesystemadapter COMMAND tst_filesystemadapter)
target_link_libraries(tst_filesystemadapter PRIVATE Qt6::Test Corbomite::Storage)

add_executable(tst_vaultscanner tst_vaultscanner.cpp)
add_test(NAME tst_vaultscanner COMMAND tst_vaultscanner)
target_link_libraries(tst_vaultscanner PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage Corbomite::Core)

add_executable(tst_sqliteindex tst_sqliteindex.cpp)
add_test(NAME tst_sqliteindex COMMAND tst_sqliteindex)
target_link_libraries(tst_sqliteindex PRIVATE Qt6::Test Qt6::Sql Corbomite::Storage Corbomite::Core)
set_tests_properties(tst_sqliteindex PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_linkresolver tst_linkresolver.cpp)
add_test(NAME tst_linkresolver COMMAND tst_linkresolver)
target_link_libraries(tst_linkresolver PRIVATE Qt6::Test Corbomite::Storage)

add_executable(tst_vaultconfig tst_vaultconfig.cpp)
add_test(NAME tst_vaultconfig COMMAND tst_vaultconfig)
target_link_libraries(tst_vaultconfig PRIVATE Qt6::Test Corbomite::Storage)

add_executable(tst_workspacestate tst_workspacestate.cpp)
add_test(NAME tst_workspacestate COMMAND tst_workspacestate)
target_link_libraries(tst_workspacestate PRIVATE Qt6::Test Corbomite::Storage)

add_executable(tst_probe_trash_ignore tst_probe_trash_ignore.cpp)
add_test(NAME tst_probe_trash_ignore COMMAND tst_probe_trash_ignore)
target_link_libraries(tst_probe_trash_ignore PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage Corbomite::Core)

add_executable(tst_vaultprocess tst_vaultprocess.cpp)
add_test(NAME tst_vaultprocess COMMAND tst_vaultprocess)
target_link_libraries(tst_vaultprocess PRIVATE Qt6::Test Qt6::Concurrent Corbomite::Storage)

add_executable(tst_obsidian_vault_roundtrip tst_obsidian_vault_roundtrip.cpp)
add_test(NAME tst_obsidian_vault_roundtrip COMMAND tst_obsidian_vault_roundtrip)
target_link_libraries(tst_obsidian_vault_roundtrip PRIVATE Qt6::Test Corbomite::Storage)

add_executable(tst_cachedmetadata tst_cachedmetadata.cpp)
add_test(NAME tst_cachedmetadata COMMAND tst_cachedmetadata)
target_link_libraries(tst_cachedmetadata PRIVATE Qt6::Test Corbomite::Storage)

add_executable(tst_metadataparser tst_metadataparser.cpp)
add_test(NAME tst_metadataparser COMMAND tst_metadataparser)
target_link_libraries(tst_metadataparser PRIVATE Qt6::Test Corbomite::Storage MarkoffParser::MarkoffParser)

add_executable(tst_metadatacache_core tst_metadatacache_core.cpp)
add_test(NAME tst_metadatacache_core COMMAND tst_metadatacache_core)
target_link_libraries(tst_metadatacache_core PRIVATE Qt6::Test Qt6::Core Corbomite::Storage)

add_executable(tst_metadatacache_events tst_metadatacache_events.cpp)
add_test(NAME tst_metadatacache_events COMMAND tst_metadatacache_events)
target_link_libraries(tst_metadatacache_events PRIVATE Qt6::Test Qt6::Core Corbomite::Storage Corbomite::Core)
set_tests_properties(tst_metadatacache_events PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_metadataworker tst_metadataworker.cpp)
add_test(NAME tst_metadataworker COMMAND tst_metadataworker)
target_link_libraries(tst_metadataworker PRIVATE Qt6::Test Qt6::Core Corbomite::Storage)
set_tests_properties(tst_metadataworker PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_metadatacache_worker_integration tst_metadatacache_worker_integration.cpp)
add_test(NAME tst_metadatacache_worker_integration COMMAND tst_metadatacache_worker_integration)
target_link_libraries(tst_metadatacache_worker_integration PRIVATE Qt6::Test Qt6::Core Qt6::Sql Corbomite::Storage Corbomite::Core)
set_tests_properties(tst_metadatacache_worker_integration PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_cachedmetadatastore tst_cachedmetadatastore.cpp)
add_test(NAME tst_cachedmetadatastore COMMAND tst_cachedmetadatastore)
target_link_libraries(tst_cachedmetadatastore PRIVATE Qt6::Test Qt6::Core Qt6::Sql Corbomite::Storage Corbomite::Core)
set_tests_properties(tst_cachedmetadatastore PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_metadatacache_signals tst_metadatacache_signals.cpp)
add_test(NAME tst_metadatacache_signals COMMAND tst_metadatacache_signals)
target_link_libraries(tst_metadatacache_signals PRIVATE Qt6::Test Qt6::Core Corbomite::Storage Corbomite::Core)
set_tests_properties(tst_metadatacache_signals PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Cluster E Phase 1 — EphemeralState JSON round-trip.
add_executable(tst_ephemeral_state tst_ephemeral_state.cpp)
add_test(NAME tst_ephemeral_state COMMAND tst_ephemeral_state)
target_link_libraries(tst_ephemeral_state PRIVATE Qt6::Test Corbomite::Storage)

# SessionManager round-trip tests — compile SessionManager.cpp directly to
# avoid the heavy KDE/Qt-Widgets dependencies of the CorbomiteApp static lib.
add_executable(tst_session_manager_roundtrip
    tst_session_manager_roundtrip.cpp
    ${CMAKE_SOURCE_DIR}/src/app/SessionManager.cpp
)
add_test(NAME tst_session_manager_roundtrip COMMAND tst_session_manager_roundtrip)
target_include_directories(tst_session_manager_roundtrip PRIVATE
    ${CMAKE_SOURCE_DIR}/src/app
)
target_link_libraries(tst_session_manager_roundtrip PRIVATE Qt6::Test Qt6::Core)
