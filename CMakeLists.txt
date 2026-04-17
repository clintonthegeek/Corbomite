cmake_minimum_required(VERSION 3.19)
project(Corbomite VERSION 0.1.0 LANGUAGES CXX)

option(CORBOMITE_DEV_BUILD "Build with dev identity (isolated config/data, [Dev] suffix)" OFF)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets DBus Sql Svg)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

qt_standard_project_setup()

# Extra CMake Modules (for KDE macros)
find_package(ECM 6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)

# KDE Frameworks dependencies
find_package(KF6CoreAddons REQUIRED)
find_package(KF6I18n REQUIRED)
find_package(KF6XmlGui REQUIRED)
find_package(KF6WidgetsAddons REQUIRED)
find_package(KF6IconThemes REQUIRED)
find_package(KF6Config REQUIRED)
find_package(KF6ConfigWidgets REQUIRED)
find_package(KF6DBusAddons REQUIRED)
find_package(KF6SyntaxHighlighting REQUIRED)

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

set(MARKOFF_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(libs/markoff-family)

add_subdirectory(libs/core)
add_subdirectory(libs/search)
add_subdirectory(libs/storage)
add_subdirectory(libs/models)
add_subdirectory(libs/vault)
add_subdirectory(libs/forcegraph)
add_subdirectory(libs/canvas)
add_subdirectory(libs/qutepart-corbomite)
add_subdirectory(libs/readingview)

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

qt_add_resources(Corbomite "xmlgui"
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

install(FILES data/org.corbomite.Corbomite.desktop DESTINATION ${KDE_INSTALL_APPDIR})

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    qt_generate_deploy_app_script(
        TARGET Corbomite
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${deploy_script})
endif()

# Tests
enable_testing()
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
add_subdirectory(libs/forcegraph/tests)
add_subdirectory(libs/canvas/tests)
