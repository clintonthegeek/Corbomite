cmake_minimum_required(VERSION 3.19)
project(Corbomite VERSION 0.2.0 LANGUAGES CXX)

option(CORBOMITE_DEV_BUILD "Build with dev identity (isolated config/data, [Dev] suffix)" OFF)
option(CORBOMITE_BUILD_EXAMPLES "Build example plugins" OFF)

# Cluster K Phase 5: Corbomite no longer links markoff_live / Qt QuickWidgets.
# KDDockWidgets may still pull qt6-declarative transitively at the system
# package level; we do not need Quick/Qml for our own targets.
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets DBus Sql Svg PrintSupport)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# TODO(port-foundation-exploration): Corbomite's shared libs (KF6/Qt
# convention) link the Markoff static libs, so the submodule must build with
# PIC. Set globally so we don't have to per-target-set on each Markoff lib.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

qt_standard_project_setup()

# Extra CMake Modules (for KDE macros)
find_package(ECM 6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)

# Project-local CMake modules (helpers used by in-tree + third-party
# plugins). Must appear after ECM_MODULE_PATH is set so KDE modules
# still resolve first.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(CorbomitePlugin)

# KDE Frameworks dependencies
find_package(KF6CoreAddons REQUIRED)
find_package(KF6I18n REQUIRED)
find_package(KF6XmlGui REQUIRED)
find_package(KF6WidgetsAddons REQUIRED)
find_package(KF6IconThemes REQUIRED)
find_package(KF6Config REQUIRED)
find_package(KF6ConfigWidgets REQUIRED)
find_package(KF6ColorScheme REQUIRED)
find_package(KF6DBusAddons REQUIRED)
find_package(KF6SyntaxHighlighting REQUIRED)

find_package(KDDockWidgets-qt6 2.0 REQUIRED)
message(STATUS "Found KDDockWidgets-qt6: ${KDDockWidgets-qt6_VERSION} at ${KDDockWidgets-qt6_DIR}")

# Optional: qt6keychain for persistent SecretStorage backend. Auto-disables
# when the package isn't installed; SecretStorage then falls back to an
# in-process QHash (with a qCWarning noting secrets won't persist).
option(CORBOMITE_USE_KEYRING "Use qt6keychain for SecretStorage" ON)
if(CORBOMITE_USE_KEYRING)
    find_package(Qt6Keychain CONFIG)
    if(NOT Qt6Keychain_FOUND)
        message(STATUS "Qt6Keychain not found; SecretStorage will use in-process QHash fallback")
        set(CORBOMITE_USE_KEYRING OFF CACHE BOOL "" FORCE)
    endif()
endif()
if(CORBOMITE_USE_KEYRING)
    add_compile_definitions(CORBOMITE_HAVE_KEYRING=1)
endif()


# Libraries
#
# Markoff family (markoff + markoff-parser + rapidyaml) lives in a separate
# repo at ~/dev/Markoff, attached here as a git submodule. Its own top-level
# CMakeLists adds the three sub-libraries. Tests inside the submodule are
# disabled when built as a sub-dir of Corbomite (we run Markoff's tests from
# within the Markoff repo); Corbomite keeps its own Markoff integration
# tests at libs/markoff-family/tests/markoff.
# jkqtmathtext and mmdr stay in Corbomite (shared by core + readingview +
# markoff). They must be added BEFORE libs/markoff-family so that markoff's
# defensive `if(NOT TARGET jkqtmathtext)` fallback inside the submodule
# short-circuits (otherwise it tries add_subdirectory(../jkqtmathtext),
# which resolves to the wrong path inside the submodule).
add_subdirectory(libs/jkqtmathtext)
add_subdirectory(libs/mmdr)

# Markoff's document engine is the installed CollabText Core package. Do not
# point this at a CollabText checkout. COLLABTEXT_ROOT overrides the dev
# default, which is the alpha5 prefix next to this repo.
if(DEFINED ENV{COLLABTEXT_ROOT})
    list(PREPEND CMAKE_PREFIX_PATH "$ENV{COLLABTEXT_ROOT}")
else()
    set(_corbomite_collabtext_hint
        "${CMAKE_SOURCE_DIR}/../Markoff/build-collabtext-alpha5/install")
    if(EXISTS "${_corbomite_collabtext_hint}/lib/cmake/collabtext/collabtext-config.cmake")
        list(PREPEND CMAKE_PREFIX_PATH "${_corbomite_collabtext_hint}")
    endif()
endif()
find_package(collabtext CONFIG REQUIRED COMPONENTS core)

set(MARKOFF_BUILD_TESTS OFF CACHE BOOL "" FORCE)
# TODO(port-foundation-exploration): foundation-exploration retired the
# single top-level MARKOFF_BUILD_TESTS gate; each leaf now has its own
# (MARKOFF_CORE_BUILD_TESTS, etc.). Per-leaf gates explicitly OFF here so
# the submodule doesn't try to build tests that depend on internal
# headers Corbomite doesn't expose to its build. Clean-up: have Markoff
# make MARKOFF_BUILD_TESTS the canonical gate that propagates to all
# leaves.
#
# Only four leaves actually honour a variable of this shape — core, source,
# styled and canvas (`if(NOT DEFINED MARKOFF_<LEAF>_BUILD_TESTS)`) — and
# canvas defaults its gate to ON when undefined, so it has to be named here
# or its two tests register in every configure, release included (found
# 2026-10-18 by the USER-TESTING §11b check: a release configure reported 17
# tests where this comment claimed the only survivors would be gated ones).
# MARKOFF_LIVE_BUILD_TESTS and MARKOFF_PARSER_BUILD_TESTS below are inert:
# markoff-live has no such gate, and markoff-parser does an ungated
# `add_subdirectory(tests)`, so its 13 tests cannot be switched off from here
# at all — filed upstream (ASK-M36 in the consolidated handoff, docs/handoff/2026-10-18-to-markoff-consolidated-open-asks.md).
set(MARKOFF_CORE_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
set(MARKOFF_LIVE_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
set(MARKOFF_SOURCE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(MARKOFF_PARSER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(MARKOFF_STYLED_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(MARKOFF_CANVAS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
# Cluster K Phase 5 — do not build the QML Live leaf into Corbomite.
set(MARKOFF_BUILD_LIVE OFF CACHE BOOL "" FORCE)
# Markoff's provider-consumer tests are a Markoff-repo suite. They require
# MARKOFF_BUILD_TESTS and are not part of Corbomite's ctest run.
set(MARKOFF_BUILD_COLLABTEXT_VNEXT_TESTS OFF CACHE BOOL "" FORCE)
# Phase B bridge (MARKOFF_READING_USE_REAL_COREDEPS) retired in Markoff
# v0.3.0 (Phase C1). markoff-reading now consumes host-provided
# implementations of `Markoff::*` / `Markoff::Vault::*` interfaces via
# setter injection; the adapters live at
# libs/core/include/corbomite/markoff_adapters/.
add_subdirectory(libs/markoff-family)

# Cluster M — Graffodil graph-scene substrate (submodule pinned at v0.2.3).
# GRAFFODIL_BUILD_DEMO/TESTS default OFF when not top-level; libs/canvas
# adopts Graffodil::Core in Phase M1.
#
# NOTE: Graffodil ships 16 ctest cases and we run ZERO of them, because
# GRAFFODIL_BUILD_TESTS defaults to ${PROJECT_IS_TOP_LEVEL} (FALSE as a
# subdirectory). Fixing this is BLOCKED: Graffodil's tests/CMakeLists.txt uses
# hardcoded target names, and tst_forcelayout + tst_quadtree collide with
# Corbomite's own libs/forcegraph tests, so flipping the gate fails to
# configure. See the punch list item for the two-line fix.
add_subdirectory(libs/graffodil)

add_subdirectory(libs/core)
add_subdirectory(libs/search)
add_subdirectory(libs/storage)
add_subdirectory(libs/models)
add_subdirectory(libs/vault)
add_subdirectory(libs/bases)
add_subdirectory(libs/forcegraph)
add_subdirectory(libs/canvas)

# Application
add_subdirectory(src)

qt_add_executable(Corbomite WIN32 MACOSX_BUNDLE src/app/main.cpp)

if(CORBOMITE_DEV_BUILD)
    target_compile_definitions(Corbomite PRIVATE CORBOMITE_DEV_BUILD)
    target_compile_definitions(CorbomiteApp PRIVATE CORBOMITE_DEV_BUILD)
    set(_xmlgui_prefix "/kxmlgui5/corbomite-dev")
    set(CORBOMITE_COMPONENT_NAME "corbomite-dev")
else()
    set(_xmlgui_prefix "/kxmlgui5/corbomite")
    set(CORBOMITE_COMPONENT_NAME "corbomite")
endif()

set(_xmlgui_rcfile "${CORBOMITE_COMPONENT_NAME}ui.rc")

configure_file(src/app/corbomiteui.rc.in
               ${CMAKE_CURRENT_BINARY_DIR}/${_xmlgui_rcfile} @ONLY)

set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/${_xmlgui_rcfile}
    PROPERTIES QT_RESOURCE_ALIAS ${_xmlgui_rcfile})

qt_add_resources(CorbomiteApp "xmlgui"
    PREFIX "${_xmlgui_prefix}"
    FILES ${CMAKE_CURRENT_BINARY_DIR}/${_xmlgui_rcfile}
)

target_link_libraries(Corbomite
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::DBus
        KF6::CoreAddons
        KF6::I18n
        KF6::XmlGui
        KF6::WidgetsAddons
        KF6::IconThemes
        KF6::ConfigCore
        KF6::ConfigWidgets
        KF6::DBusAddons
        CorbomiteApp
)

target_include_directories(Corbomite PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Install
include(GNUInstallDirs)

install(TARGETS Corbomite CorbomiteApp
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${_xmlgui_rcfile}
        DESTINATION ${KDE_INSTALL_KXMLGUIDIR}/${CORBOMITE_COMPONENT_NAME})

install(FILES data/com.concernednetizen.Corbomite.desktop DESTINATION ${KDE_INSTALL_APPDIR})

# AppStream metainfo (release-hygiene E.1, 2026-10-01). Without this the .deb is
# invisible to GNOME Software / KDE Discover. This ECM version has no
# kde_install_metainfo_file() macro — its appstreamtest() works by scanning the
# install manifest for *.metainfo.xml / *.appdata.xml, so a plain install() rule
# is both sufficient and the mechanism that gets the file validated by ctest.
# Keep the component id in the file, the .desktop basename, and
# KAboutData::setDesktopFileName() in src/app/main.cpp in sync.
install(FILES data/com.concernednetizen.Corbomite.metainfo.xml
        DESTINATION ${KDE_INSTALL_DATADIR}/metainfo)

install(FILES data/icons/hicolor/scalable/apps/com.concernednetizen.Corbomite.svg
        DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps)

# CMake package-config: lets third-party plugins do find_package(Corbomite)
# to pull in Corbomite::Core / Vault / Storage / Models targets and the
# corbomite_add_plugin() helper. Must come after all add_subdirectory(libs/*)
# calls so each library's install(TARGETS ... EXPORT CorbomiteTargets) has
# already registered into the export set.
include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/cmake/CorbomiteConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/CorbomiteConfig.cmake"
    INSTALL_DESTINATION "${KDE_INSTALL_CMAKEPACKAGEDIR}/Corbomite"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/CorbomiteConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/CorbomiteConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/CorbomiteConfigVersion.cmake"
    "${CMAKE_CURRENT_LIST_DIR}/cmake/CorbomitePlugin.cmake"
    DESTINATION "${KDE_INSTALL_CMAKEPACKAGEDIR}/Corbomite"
)

install(EXPORT CorbomiteTargets
    FILE CorbomiteTargets.cmake
    NAMESPACE Corbomite::
    DESTINATION "${KDE_INSTALL_CMAKEPACKAGEDIR}/Corbomite"
)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    qt_generate_deploy_app_script(
        TARGET Corbomite
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${deploy_script})
endif()

# Tests
#
# CORBOMITE_BUILD_TESTS is the single canonical test gate for the whole
# tree. The `dev` preset sets it ON, `release`/`appimage` set it OFF.
#
# The gate was previously OFF-by-default with NO preset override, which meant a
# clean `cmake --preset dev` silently produced a tree with 109 tests instead of
# 345 - all 109 from the markoff-family submodule, zero of them Corbomite's. It
# appeared to work only because this machine's checked-in build-dev/ cache
# already had it ON. Verified and fixed 2026-10-02.
#
# NOTE: `BUILD_TESTING` is NOT used as a gate here. ECM's
# kde-modules/KDECMakeSettings.cmake declares `option(BUILD_TESTING ... ON)`,
# so 12 subdirectories that read it were reading a third-party default rather
# than our setting. Do not reintroduce it; use CORBOMITE_BUILD_TESTS.
#
# Residual, known and unfixed: with the gate OFF, markoff-family still
# registers ~109 of its own tests unconditionally. Patching it there would be
# lost on the next submodule re-pin.
option(CORBOMITE_BUILD_TESTS "Build the Corbomite test suite" OFF)
if(CORBOMITE_BUILD_TESTS)
enable_testing()

# Structural guard against the corbomite-core <-> corbomite-storage
# dependency cycle reappearing. Not tied to any one library's test
# directory since it inspects the whole build's dependency graph via
# `cmake --graphviz`; see cmake/CheckNoLibraryCycles.cmake.
add_test(NAME tst_no_library_cycles
    COMMAND ${CMAKE_COMMAND} -DBINARY_DIR=${CMAKE_BINARY_DIR}
            -P ${CMAKE_SOURCE_DIR}/cmake/CheckNoLibraryCycles.cmake)

# Cluster P Phase P4.T3 — structural guard against a
# staticMetaObject symbol being defined in more than one built module
# (the C1/C2 plugin-boundary correctness bug this cluster fixes). See
# cmake/CheckNoDuplicateMetaObjects.cmake for the allowlist mechanism.
add_test(NAME tst_no_duplicate_metaobjects
    COMMAND ${CMAKE_COMMAND} -DBINARY_DIR=${CMAKE_BINARY_DIR}
            -P ${CMAKE_SOURCE_DIR}/cmake/CheckNoDuplicateMetaObjects.cmake)

add_subdirectory(tests/app)
add_subdirectory(tests/core)
add_subdirectory(tests/storage)
add_subdirectory(tests/search)
add_subdirectory(tests/models)
add_subdirectory(tests/integration)
add_subdirectory(tests/dialogs)
add_subdirectory(tests/editor)
add_subdirectory(tests/e2e)
add_subdirectory(tests/graph)
add_subdirectory(tests/sidebar)
endif()
# libs/forcegraph and libs/canvas gate their own tests/ subdirectories on
# CORBOMITE_BUILD_TESTS inside their CMakeLists.txt.
# TODO(port-foundation-exploration): Markoff foundation-exploration retired
# the top-level tests/ directory; cross-leaf tests are now per-leaf or absent.
# Re-evaluate once the port has settled on which Markoff tests Corbomite wants
# to drive directly. Original line:
#   add_subdirectory(libs/markoff-family/tests/markoff)

# Examples (opt-in) — gated behind CORBOMITE_BUILD_EXAMPLES so default
# builds don't pay for them.
if(CORBOMITE_BUILD_EXAMPLES)
    add_subdirectory(examples/plugin-template)
    add_subdirectory(examples/note-stats-plugin)
endif()
