cmake_minimum_required(VERSION 3.19)
project(Corbomite_KilnPluginTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test Widgets)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)

# The settings model and the report rendering are pure logic, so they are tested
# without a widget, a vault, or a process.
add_executable(tst_kilnsettings
    tst_kilnsettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../KilnSettings.cpp)
target_include_directories(tst_kilnsettings PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_test(NAME tst_kilnsettings COMMAND tst_kilnsettings)
target_link_libraries(tst_kilnsettings PRIVATE
    Qt6::Core Qt6::Test KF6::I18n KF6::CoreAddons)
set_tests_properties(tst_kilnsettings PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_kiln_plugin tst_kiln_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../KilnPlugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../KilnRunner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../KilnSettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../KilnView.cpp)
target_include_directories(tst_kiln_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..)
# moc resolves K_PLUGIN_FACTORY_WITH_JSON("metadata.json", ...) against
# its include path; the configured metadata.json lives in the plugin
# target's build directory (parent scope).
set_property(TARGET tst_kiln_plugin APPEND PROPERTY
    AUTOMOC_MOC_OPTIONS "-I${CMAKE_CURRENT_BINARY_DIR}/..")
add_test(NAME tst_kiln_plugin COMMAND tst_kiln_plugin)
target_link_libraries(tst_kiln_plugin PRIVATE
    Qt6::Test Qt6::Widgets
    Corbomite::Core Corbomite::Storage Corbomite::Vault
    KF6::I18n KF6::CoreAddons)
set_tests_properties(tst_kiln_plugin PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# The panel's flow over a runner that answers instantly. No subprocess is started.
# `fakerunner.h` is listed as a source so AUTOMOC moc's it: a Q_OBJECT class in a
# header that no target names gets no metaobject, and the link fails on its vtable.
add_executable(tst_kilnview tst_kilnview.cpp
    fakerunner.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../KilnRunner.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../KilnSettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../KilnView.cpp)
target_include_directories(tst_kilnview PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_test(NAME tst_kilnview COMMAND tst_kilnview)
target_link_libraries(tst_kilnview PRIVATE
    Qt6::Test Qt6::Widgets
    Corbomite::Core Corbomite::Storage Corbomite::Vault
    KF6::I18n KF6::CoreAddons)
set_tests_properties(tst_kilnview PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
