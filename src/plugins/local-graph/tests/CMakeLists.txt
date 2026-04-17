cmake_minimum_required(VERSION 3.19)
project(Corbomite_LocalGraphPluginTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test Widgets)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)

add_executable(tst_localgraph_plugin tst_localgraph_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../LocalGraphPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../LocalGraphView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../graph/GraphDataBuilder.cpp)
target_include_directories(tst_localgraph_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../graph)
add_test(NAME tst_localgraph_plugin COMMAND tst_localgraph_plugin)
target_link_libraries(tst_localgraph_plugin PRIVATE
    Qt6::Test Qt6::Widgets
    Corbomite::Core Corbomite::Storage Corbomite::Vault
    forcegraph
    KF6::I18n KF6::CoreAddons)
set_tests_properties(tst_localgraph_plugin PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
