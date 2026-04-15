cmake_minimum_required(VERSION 3.19)
project(corbomite-search VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui)

add_library(corbomite-search STATIC
    src/PreparedQuery.cpp
    include/corbomite/search/PreparedQuery.h
    src/FuzzyMatch.cpp
    include/corbomite/search/FuzzyMatch.h
    src/FuzzyMatcher.cpp
    include/corbomite/search/FuzzyMatcher.h
    src/ResultHighlighter.cpp
    include/corbomite/search/ResultHighlighter.h
)
set_target_properties(corbomite-search PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(Corbomite::Search ALIAS corbomite-search)

target_include_directories(corbomite-search
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(corbomite-search PUBLIC Qt6::Core Qt6::Gui)
