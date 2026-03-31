cmake_minimum_required(VERSION 3.19)
project(corbomite-storage VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core)

add_library(corbomite-storage STATIC
    src/FileSystemAdapter.cpp
)
set_target_properties(corbomite-storage PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(Corbomite::Storage ALIAS corbomite-storage)

target_include_directories(corbomite-storage
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(corbomite-storage PUBLIC Qt6::Core)

if(NOT PROJECT_IS_TOP_LEVEL)
    target_link_libraries(corbomite-storage PRIVATE Corbomite::Core)
    target_include_directories(corbomite-storage PRIVATE
        ${CMAKE_SOURCE_DIR}/libs/core/include
    )
endif()
