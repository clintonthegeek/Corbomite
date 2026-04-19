cmake_minimum_required(VERSION 3.19)
project(vault VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets Network)
find_package(KF6I18n REQUIRED)
find_package(KF6CoreAddons REQUIRED)
find_package(KF6Config REQUIRED)

add_library(vault SHARED
    include/corbomite/vault/TAbstractFile.h
    src/TAbstractFile.cpp
    include/corbomite/vault/TFile.h
    src/TFile.cpp
    include/corbomite/vault/TFolder.h
    src/TFolder.cpp
    include/corbomite/vault/Vault.h
    src/Vault.cpp
    src/PathNormalization.h
    src/PathNormalization.cpp
    include/corbomite/vault/VaultScanner.h
    src/VaultScanner.cpp
    src/Watcher.h
    src/Watcher.cpp
    include/corbomite/vault/FileManager.h
    src/FileManager.cpp
    src/FileNameValidator.h
    src/FileNameValidator.cpp
    src/dialogs/RenameDialog.h
    src/dialogs/RenameDialog.cpp
    src/dialogs/MoveFileDialog.h
    src/dialogs/MoveFileDialog.cpp
    include/corbomite/vault/proxies/VaultProxy.h
    src/proxies/VaultProxy.cpp
    include/corbomite/vault/proxies/FileManagerProxy.h
    src/proxies/FileManagerProxy.cpp
    include/corbomite/vault/Plugin.h
    src/Plugin.cpp
    include/corbomite/vault/PluginContext.h
    src/PluginContext.cpp
    include/corbomite/vault/PluginDataStore.h
    src/PluginDataStore.cpp
    include/corbomite/vault/PluginPermissionGrantDialog.h
    src/PluginPermissionGrantDialog.cpp
    include/corbomite/vault/PluginManager.h
    src/PluginManager.cpp
)
set_target_properties(vault PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    EXPORT_NAME Vault)
add_library(Corbomite::Vault ALIAS vault)

target_include_directories(vault
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR}>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(vault
    PUBLIC
        Qt6::Core
        Qt6::Widgets
        Qt6::Network
        Corbomite::Core
        Corbomite::Storage
        KF6::I18n
        KF6::CoreAddons
        KF6::ConfigCore
    PRIVATE
        MarkoffParser::MarkoffParser
)

# PluginManager's MinVersion gate compares plugin metadata against this
# value at runtime. Sourced from the top-level project() VERSION so
# bumps to corbomite's version automatically propagate.
if(DEFINED Corbomite_VERSION)
    target_compile_definitions(vault PRIVATE
        CORBOMITE_APP_VERSION="${Corbomite_VERSION}")
elseif(DEFINED PROJECT_VERSION)
    target_compile_definitions(vault PRIVATE
        CORBOMITE_APP_VERSION="${PROJECT_VERSION}")
endif()

# Install rules (public consumption via find_package(Corbomite)).
if(DEFINED KDE_INSTALL_INCLUDEDIR)
    install(TARGETS vault
        EXPORT CorbomiteTargets
        RUNTIME DESTINATION ${KDE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${KDE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${KDE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${KDE_INSTALL_INCLUDEDIR}
    )
    install(DIRECTORY include/corbomite/vault/
        DESTINATION ${KDE_INSTALL_INCLUDEDIR}/corbomite/vault
    )
endif()

enable_testing()
add_subdirectory(tests)
