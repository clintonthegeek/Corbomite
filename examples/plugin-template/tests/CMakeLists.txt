# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.19)
project(Corbomite_TemplatePluginTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test Widgets)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)

# Smoke test for the plugin-template skeleton. Compiles the plugin
# sources directly (rather than dlopen'ing the built .so) so we don't
# depend on KPluginFactory discovery inside the test.
add_executable(tst_template_plugin
    tst_template_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../TemplatePlugin.cpp)
target_include_directories(tst_template_plugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..)

# moc resolves K_PLUGIN_FACTORY_WITH_JSON("metadata.json", ...) against
# its include path; the configured metadata.json lives next to the
# plugin target's build output in the parent scope.
set_property(TARGET tst_template_plugin APPEND PROPERTY
    AUTOMOC_MOC_OPTIONS "-I${CMAKE_CURRENT_BINARY_DIR}/..")

target_link_libraries(tst_template_plugin PRIVATE
    Qt6::Test Qt6::Widgets
    Corbomite::Core Corbomite::Vault
    KF6::CoreAddons)

add_test(NAME tst_template_plugin COMMAND tst_template_plugin)
set_tests_properties(tst_template_plugin PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
