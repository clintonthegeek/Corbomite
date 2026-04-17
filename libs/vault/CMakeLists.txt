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

add_library(vault STATIC
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
    include/corbomite/vault/proxies/VaultProxy.h
    src/proxies/VaultProxy.cpp
    include/corbomite/vault/proxies/FileManagerProxy.h
    src/proxies/FileManagerProxy.cpp
    include/corbomite/vault/Plugin.h
    src/Plugin.cpp
    include/corbomite/vault/PluginContext.h
    src/PluginContext.cpp
    include/corbomite/vault/PluginPermissionGrantDialog.h
    src/PluginPermissionGrantDialog.cpp
    include/corbomite/vault/PluginManager.h
    src/PluginManager.cpp
)
set_target_properties(vault PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(Corbomite::Vault ALIAS vault)

target_include_directories(vault
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
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

enable_testing()
add_subdirectory(tests)
