# SPDX-License-Identifier: GPL-3.0-or-later
#
# Minimum third-party Corbomite plugin. In an out-of-tree build a
# developer would start with:
#
#     cmake_minimum_required(VERSION 3.20)
#     project(corbomite-template-plugin VERSION 0.1.0 LANGUAGES CXX)
#     find_package(Corbomite REQUIRED)
#     find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
#     find_package(KF6 REQUIRED COMPONENTS CoreAddons)
#
# In-tree (examples/ built from the parent Corbomite CMakeLists) the
# Corbomite::* targets, Qt6::*, and KF6::* are already imported, and
# corbomite_add_plugin() is already on the module path.

corbomite_add_plugin(corbomite-template
    METADATA_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json.in"
    SOURCES
        TemplatePlugin.cpp
    LINK_LIBRARIES
        Corbomite::Core
        Corbomite::Vault
        Qt6::Widgets
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
