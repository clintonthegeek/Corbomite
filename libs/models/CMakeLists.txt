cmake_minimum_required(VERSION 3.19)
project(corbomite-models VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets)

add_library(corbomite-models STATIC
    src/TabModel.cpp
    include/corbomite/models/TabModel.h
    src/VaultModel.cpp
    include/corbomite/models/VaultModel.h
)
set_target_properties(corbomite-models PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(Corbomite::Models ALIAS corbomite-models)

target_include_directories(corbomite-models
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(corbomite-models PUBLIC Qt6::Core Qt6::Widgets)

if(NOT PROJECT_IS_TOP_LEVEL)
    target_link_libraries(corbomite-models PUBLIC Corbomite::Core Corbomite::Storage)
endif()
