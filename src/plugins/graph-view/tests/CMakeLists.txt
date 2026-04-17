cmake_minimum_required(VERSION 3.19)
project(Corbomite_GraphViewPluginTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test Widgets)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)

add_executable(tst_graphview_plugin tst_graphview_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../GraphViewPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../GraphView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../GraphViewTab.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../GraphControlsPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../CollapsibleSection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../graph/GraphDataBuilder.cpp)
target_include_directories(tst_graphview_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../graph)
add_test(NAME tst_graphview_plugin COMMAND tst_graphview_plugin)
target_link_libraries(tst_graphview_plugin PRIVATE
    Qt6::Test Qt6::Widgets
    Corbomite::Core Corbomite::Storage Corbomite::Vault
    forcegraph
    KF6::I18n KF6::CoreAddons KF6::WidgetsAddons)
set_tests_properties(tst_graphview_plugin PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
