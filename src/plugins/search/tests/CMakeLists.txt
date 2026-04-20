cmake_minimum_required(VERSION 3.19)
project(Corbomite_SearchPluginTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test Widgets)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)

add_executable(tst_search_plugin tst_search_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../SearchPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../SearchView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../SearchResultsDelegate.cpp)
target_include_directories(tst_search_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..)
# moc resolves K_PLUGIN_FACTORY_WITH_JSON("metadata.json", ...) against
# its include path; the configured metadata.json lives in the plugin
# target's build directory (parent scope).
set_property(TARGET tst_search_plugin APPEND PROPERTY
    AUTOMOC_MOC_OPTIONS "-I${CMAKE_CURRENT_BINARY_DIR}/..")
add_test(NAME tst_search_plugin COMMAND tst_search_plugin)
target_link_libraries(tst_search_plugin PRIVATE
    Qt6::Test Qt6::Widgets
    Corbomite::Core Corbomite::Search Corbomite::Storage
    Corbomite::Vault Corbomite::Models
    KF6::I18n KF6::CoreAddons)
set_tests_properties(tst_search_plugin PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
