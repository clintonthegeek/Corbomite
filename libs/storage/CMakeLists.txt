cmake_minimum_required(VERSION 3.19)
project(corbomite-storage VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Sql)

add_library(corbomite-storage STATIC
    src/FileSystemAdapter.cpp
    include/corbomite/storage/FileSystemAdapter.h
    include/corbomite/storage/DataAdapter.h
    src/SQLiteIndex.cpp
    include/corbomite/storage/SQLiteIndex.h
    src/LinkResolver.cpp
    include/corbomite/storage/LinkResolver.h
    src/VaultConfig.cpp
    include/corbomite/storage/VaultConfig.h
    src/WorkspaceState.cpp
    include/corbomite/storage/WorkspaceState.h
    src/EphemeralState.cpp
    include/corbomite/storage/EphemeralState.h
    src/CaseSensitivityProbe.cpp
    include/corbomite/storage/CaseSensitivityProbe.h
    src/IgnoreFilter.cpp
    include/corbomite/storage/IgnoreFilter.h
    src/CachedMetadata.cpp
    include/corbomite/storage/CachedMetadata.h
    src/MetadataParser.cpp
    include/corbomite/storage/MetadataParser.h
    src/MetadataCache.cpp
    include/corbomite/storage/MetadataCache.h
    src/MetadataWorker.cpp
    src/MetadataWorkerLoop_p.h
    include/corbomite/storage/MetadataWorker.h
    src/CachedMetadataStore.cpp
    include/corbomite/storage/CachedMetadataStore.h
    src/proxies/MetadataCacheReader.cpp
    include/corbomite/storage/proxies/MetadataCacheReader.h
    src/proxies/SearchProxy.cpp
    include/corbomite/storage/proxies/SearchProxy.h
)
set_target_properties(corbomite-storage PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    EXPORT_NAME Storage)
add_library(Corbomite::Storage ALIAS corbomite-storage)

target_include_directories(corbomite-storage
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(corbomite-storage
    PUBLIC
        Qt6::Core Qt6::Sql
    # MarkoffParser is a submodule dep — used only in .cpp today, kept on
    # the PUBLIC interface for header-visibility convenience. Wrapped with
    # BUILD_INTERFACE so it doesn't leak into the installed package, which
    # does not ship submodule targets. See libs/core/CMakeLists.txt for
    # the full rationale.
    PUBLIC
        $<BUILD_INTERFACE:MarkoffParser::MarkoffParser>)

if(NOT PROJECT_IS_TOP_LEVEL)
    target_link_libraries(corbomite-storage PUBLIC Corbomite::Core)
endif()

# Install rules (public consumption via find_package(Corbomite)).
if(DEFINED KDE_INSTALL_INCLUDEDIR)
    install(TARGETS corbomite-storage
        EXPORT CorbomiteTargets
        RUNTIME DESTINATION ${KDE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${KDE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${KDE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${KDE_INSTALL_INCLUDEDIR}
    )
    install(DIRECTORY include/corbomite/storage/
        DESTINATION ${KDE_INSTALL_INCLUDEDIR}/corbomite/storage
    )
endif()
