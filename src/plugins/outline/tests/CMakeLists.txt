cmake_minimum_required(VERSION 3.19)
project(Corbomite_OutlinePluginTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test Widgets)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)

add_executable(tst_outline_plugin tst_outline_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../OutlinePlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../OutlineView.cpp)
target_include_directories(tst_outline_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_test(NAME tst_outline_plugin COMMAND tst_outline_plugin)
target_link_libraries(tst_outline_plugin PRIVATE
    Qt6::Test Qt6::Widgets
    Corbomite::Core Corbomite::Storage Corbomite::Vault
    KF6::I18n KF6::CoreAddons)
set_tests_properties(tst_outline_plugin PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
