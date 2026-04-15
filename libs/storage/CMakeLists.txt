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
    src/VaultScanner.cpp
    src/SQLiteIndex.cpp
    include/corbomite/storage/SQLiteIndex.h
    src/LinkResolver.cpp
    include/corbomite/storage/LinkResolver.h
    src/VaultConfig.cpp
    include/corbomite/storage/VaultConfig.h
    src/WorkspaceState.cpp
    include/corbomite/storage/WorkspaceState.h
    src/CaseSensitivityProbe.cpp
    include/corbomite/storage/CaseSensitivityProbe.h
    src/VaultTrash.cpp
    include/corbomite/storage/VaultTrash.h
    src/IgnoreFilter.cpp
    include/corbomite/storage/IgnoreFilter.h
    src/VaultProcess.cpp
    include/corbomite/storage/VaultProcess.h
)
set_target_properties(corbomite-storage PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(Corbomite::Storage ALIAS corbomite-storage)

target_include_directories(corbomite-storage
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(corbomite-storage PUBLIC Qt6::Core Qt6::Sql MarkoffParser::MarkoffParser)

if(NOT PROJECT_IS_TOP_LEVEL)
    target_link_libraries(corbomite-storage PUBLIC Corbomite::Core)
endif()
