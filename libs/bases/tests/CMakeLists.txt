cmake_minimum_required(VERSION 3.19)
project(Corbomite_BasesTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_executable(tst_bases_value_null tst_value_null.cpp)
add_test(NAME tst_bases_value_null COMMAND tst_bases_value_null)
target_link_libraries(tst_bases_value_null PRIVATE Qt6::Test Corbomite::Bases)

add_executable(tst_bases_value_primitive tst_value_primitive.cpp)
add_test(NAME tst_bases_value_primitive COMMAND tst_bases_value_primitive)
target_link_libraries(tst_bases_value_primitive PRIVATE Qt6::Test Corbomite::Bases)

add_executable(tst_bases_value_list tst_value_list.cpp)
add_test(NAME tst_bases_value_list COMMAND tst_bases_value_list)
target_link_libraries(tst_bases_value_list PRIVATE Qt6::Test Corbomite::Bases)

add_executable(tst_bases_value_date tst_value_date.cpp)
add_test(NAME tst_bases_value_date COMMAND tst_bases_value_date)
target_link_libraries(tst_bases_value_date PRIVATE Qt6::Test Corbomite::Bases)

add_executable(tst_bases_value_duration tst_value_duration.cpp)
add_test(NAME tst_bases_value_duration COMMAND tst_bases_value_duration)
target_link_libraries(tst_bases_value_duration PRIVATE Qt6::Test Corbomite::Bases)

add_executable(tst_bases_value_string_subclasses tst_value_string_subclasses.cpp)
add_test(NAME tst_bases_value_string_subclasses COMMAND tst_bases_value_string_subclasses)
target_link_libraries(tst_bases_value_string_subclasses PRIVATE Qt6::Test Corbomite::Bases)
