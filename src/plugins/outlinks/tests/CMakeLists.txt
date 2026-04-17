cmake_minimum_required(VERSION 3.19)
project(Corbomite_OutlinksPluginTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test Widgets)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)

add_executable(tst_outlinks_plugin tst_outlinks_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../OutlinksPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../OutlinksView.cpp)
target_include_directories(tst_outlinks_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..)
# moc resolves K_PLUGIN_FACTORY_WITH_JSON("metadata.json", ...) against
# its include path; the configured metadata.json lives in the plugin
# target's build directory (parent scope).
set_property(TARGET tst_outlinks_plugin APPEND PROPERTY
    AUTOMOC_MOC_OPTIONS "-I${CMAKE_CURRENT_BINARY_DIR}/..")
add_test(NAME tst_outlinks_plugin COMMAND tst_outlinks_plugin)
target_link_libraries(tst_outlinks_plugin PRIVATE
    Qt6::Test Qt6::Widgets
    Corbomite::Core Corbomite::Storage Corbomite::Vault
    KF6::I18n KF6::CoreAddons)
set_tests_properties(tst_outlinks_plugin PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
