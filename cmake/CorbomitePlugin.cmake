# SPDX-License-Identifier: GPL-3.0-or-later
#
# corbomite_add_plugin(<target>
#   METADATA_TEMPLATE <path/to/metadata.json.in>
#   SOURCES <sources...>
#   [TRUSTED]
#   [INCLUDE_DIRECTORIES <dirs...>]
#   [LINK_LIBRARIES <libs...>]
# )
#
# Builds a KPluginFactory .so plugin and configures its metadata.json
# from the template, substituting the X_CORBOMITE_TRUSTED token.
#
# Only in-tree (src/plugins/*) CMakeLists should pass TRUSTED. Third-
# party plugins using this helper (e.g. via find_package(Corbomite))
# get the "false" default; passing TRUSTED from an out-of-tree CMake is
# discouraged — there is no enforcement, but the social convention is
# that only src/plugins/ sets it. At runtime, PluginManager further
# demotes any User-origin plugin's trusted() claim to false regardless
# of what its metadata declares (see PluginMetaData::trusted()).
#
# The configured metadata.json is emitted next to the .so so that
# KPluginFactory::loadFactory() picks it up via the loader's search.
# In dev builds the .so is dropped into CORBOMITE_PLUGIN_DEV_DIR (if
# defined) so CorbomiteApp can find it without an `install` step;
# otherwise it goes to the target's default output directory.

function(corbomite_add_plugin TARGET)
    cmake_parse_arguments(ARG
        "TRUSTED"
        "METADATA_TEMPLATE"
        "SOURCES;INCLUDE_DIRECTORIES;LINK_LIBRARIES"
        ${ARGN})

    if(NOT ARG_METADATA_TEMPLATE)
        message(FATAL_ERROR
            "corbomite_add_plugin(${TARGET}): METADATA_TEMPLATE required")
    endif()
    if(NOT ARG_SOURCES)
        message(FATAL_ERROR
            "corbomite_add_plugin(${TARGET}): SOURCES required")
    endif()

    if(ARG_TRUSTED)
        set(X_CORBOMITE_TRUSTED "true")
    else()
        set(X_CORBOMITE_TRUSTED "false")
    endif()

    # Configure metadata.json into the build tree. KPluginFactory reads
    # this embedded JSON via KPluginMetaData::findPlugins(), which scans
    # the directory alongside the .so.
    configure_file(
        "${ARG_METADATA_TEMPLATE}"
        "${CMAKE_CURRENT_BINARY_DIR}/metadata.json"
        @ONLY)

    add_library(${TARGET} MODULE ${ARG_SOURCES})

    # moc needs to find the configured metadata.json when expanding
    # K_PLUGIN_FACTORY_WITH_JSON("metadata.json", ...). By default moc
    # only searches the source file's directory, so we append the
    # binary dir to AUTOMOC_MOC_OPTIONS with `-I`.
    set_property(TARGET ${TARGET} APPEND PROPERTY
        AUTOMOC_MOC_OPTIONS "-I${CMAKE_CURRENT_BINARY_DIR}")

    if(ARG_INCLUDE_DIRECTORIES)
        target_include_directories(${TARGET} PRIVATE ${ARG_INCLUDE_DIRECTORIES})
    endif()

    # KF6::CoreAddons is the minimum for KPluginFactory; downstream
    # additions are caller-specified. Kept explicit so the helper
    # doesn't silently pull in extra KF6 components.
    target_link_libraries(${TARGET} PRIVATE
        KF6::CoreAddons
        ${ARG_LINK_LIBRARIES})

    # Dev-build output: CorbomiteApp adds CORBOMITE_PLUGIN_DEV_DIR to
    # its search path at startup (see src/CMakeLists.txt). If unset
    # (e.g. an out-of-tree consumer building their own plugin), falls
    # through to the target's default output location.
    if(DEFINED CORBOMITE_PLUGIN_DEV_DIR)
        set_target_properties(${TARGET} PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY "${CORBOMITE_PLUGIN_DEV_DIR}"
            PREFIX "")
    else()
        set_target_properties(${TARGET} PROPERTIES PREFIX "")
    endif()

    # Install alongside the configured metadata.json so both land in
    # the distro plugin dir. KDE_INSTALL_PLUGINDIR is provided by
    # KDEInstallDirs; the `corbomite` subdir mirrors PluginManager's
    # default system search subdirectory.
    if(DEFINED KDE_INSTALL_PLUGINDIR)
        install(TARGETS ${TARGET}
            DESTINATION ${KDE_INSTALL_PLUGINDIR}/corbomite)
    endif()
endfunction()
