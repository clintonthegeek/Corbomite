cmake_minimum_required(VERSION 3.19)
project(Corbomite_GraphTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test Widgets Sql)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_executable(tst_graphdatabuilder tst_graphdatabuilder.cpp)
add_test(NAME tst_graphdatabuilder COMMAND tst_graphdatabuilder)
target_link_libraries(tst_graphdatabuilder PRIVATE
    Qt6::Test CorbomiteApp)
set_tests_properties(tst_graphdatabuilder PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
