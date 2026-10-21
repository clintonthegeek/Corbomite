cmake_minimum_required(VERSION 3.19)
project(Corbomite_ForceGraphTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test Widgets)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_executable(tst_forcegraph_quadtree tst_forcegraph_quadtree.cpp)
add_test(NAME tst_forcegraph_quadtree COMMAND tst_forcegraph_quadtree)
target_link_libraries(tst_forcegraph_quadtree PRIVATE Qt6::Test Corbomite::ForceGraph)
set_tests_properties(tst_forcegraph_quadtree PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_forcegraph_layout tst_forcegraph_layout.cpp)
add_test(NAME tst_forcegraph_layout COMMAND tst_forcegraph_layout)
target_link_libraries(tst_forcegraph_layout PRIVATE Qt6::Test Corbomite::ForceGraph)
set_tests_properties(tst_forcegraph_layout PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_forcegraph_benchmark tst_forcegraph_benchmark.cpp)
add_test(NAME tst_forcegraph_benchmark COMMAND tst_forcegraph_benchmark)
target_link_libraries(tst_forcegraph_benchmark PRIVATE Qt6::Core Corbomite::ForceGraph)
set_tests_properties(tst_forcegraph_benchmark PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 120
    LABELS "benchmark")
