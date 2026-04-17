cmake_minimum_required(VERSION 3.19)
project(Corbomite_FileExplorerPluginTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test Widgets)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)

add_executable(tst_fileexplorer_plugin tst_fileexplorer_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../FileExplorerPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../FileExplorerView.cpp)
target_include_directories(tst_fileexplorer_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_test(NAME tst_fileexplorer_plugin COMMAND tst_fileexplorer_plugin)
target_link_libraries(tst_fileexplorer_plugin PRIVATE
    Qt6::Test Qt6::Widgets
    Corbomite::Core Corbomite::Models Corbomite::Storage Corbomite::Vault
    KF6::I18n KF6::CoreAddons)
set_tests_properties(tst_fileexplorer_plugin PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
