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
