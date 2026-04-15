cmake_minimum_required(VERSION 3.19)
project(Corbomite_StorageTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_executable(tst_filesystemadapter tst_filesystemadapter.cpp)
add_test(NAME tst_filesystemadapter COMMAND tst_filesystemadapter)
target_link_libraries(tst_filesystemadapter PRIVATE Qt6::Test Corbomite::Storage)

add_executable(tst_vaultscanner tst_vaultscanner.cpp)
add_test(NAME tst_vaultscanner COMMAND tst_vaultscanner)
target_link_libraries(tst_vaultscanner PRIVATE Qt6::Test Corbomite::Storage Corbomite::Core)

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
