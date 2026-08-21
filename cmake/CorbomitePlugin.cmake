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
#
# LINK_LIBRARIES must only name SHARED libraries (Cluster P, closed
# 2026-08-20 — see docs/superpowers/plans/2026-08-20-cluster-p-shared-
# libraries.md). A plugin built via this MODULE target is dlopen()'d into
# the host process at runtime; if it links a STATIC library, it gets its
# own private compiled copy of every class in that library, including a
# private staticMetaObject for each QObject subclass. Qt6's
# QMetaObject::cast() (what qobject_cast<T*> compiles down to) is a
# pointer-identity check on the metaobject chain, not a string
# comparison — so a QObject the host constructed against its own
# staticMetaObject copy silently fails qobject_cast<T*>() inside the
# plugin, and vice versa, with no error, ever. This exact bug was fixed
# for every Corbomite-owned library plugins can link in Cluster P; the
# test-time proof is tests/integration/tst_plugin_type_identity.cpp and
# the structural regression guard is
# cmake/CheckNoDuplicateMetaObjects.cmake (registered as ctest
# tst_no_duplicate_metaobjects). Do not reintroduce a STATIC dependency
# here — and don't compile the same .cpp a second time directly into a
# plugin's SOURCES either (functionally identical mistake; caught once
# already in src/plugins/properties, see that commit).

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
        # ECM's global CMAKE_INSTALL_RPATH assumes every installed target
        # sits one level below the prefix root (bin/ or lib/) and stamps
        # $ORIGIN/../lib on all of them regardless of actual depth. Plugins
        # land two-to-three levels below libdir (lib/plugins/corbomite or
        # lib/qt6/plugins/corbomite, depending on KDE_INSTALL_QTPLUGINDIR),
        # so that default resolves to a directory that doesn't exist. It
        # has only ever "worked" because libdir is also a default ldconfig
        # search path on a real system install — the AppImage build script
        # already has to patchelf-correct this by hand for the very same
        # reason. Compute the real relative offset instead of relying on
        # that accident (Cluster P doctrine: see plan §1 C1 on ELF
        # interposition accidents — this is the same failure shape).
        file(RELATIVE_PATH _corbomite_plugin_libdir_rel
            "${KDE_INSTALL_FULL_PLUGINDIR}/corbomite"
            "${KDE_INSTALL_FULL_LIBDIR}")
        set_target_properties(${TARGET} PROPERTIES
            INSTALL_RPATH "$ORIGIN/${_corbomite_plugin_libdir_rel}")

        install(TARGETS ${TARGET}
            DESTINATION ${KDE_INSTALL_PLUGINDIR}/corbomite)
    endif()
endfunction()
