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

add_executable(tst_bases_value_object_regex tst_value_object_regex.cpp)
add_test(NAME tst_bases_value_object_regex COMMAND tst_bases_value_object_regex)
target_link_libraries(tst_bases_value_object_regex PRIVATE Qt6::Test Corbomite::Bases)

add_executable(tst_bases_lexer tst_lexer.cpp)
add_test(NAME tst_bases_lexer COMMAND tst_bases_lexer)
target_link_libraries(tst_bases_lexer PRIVATE Qt6::Test Corbomite::Bases)

add_executable(tst_bases_parser tst_parser.cpp)
add_test(NAME tst_bases_parser COMMAND tst_bases_parser)
target_link_libraries(tst_bases_parser PRIVATE Qt6::Test Corbomite::Bases)

add_executable(tst_bases_evaluator tst_evaluator.cpp)
add_test(NAME tst_bases_evaluator COMMAND tst_bases_evaluator)
target_link_libraries(tst_bases_evaluator PRIVATE Qt6::Test Corbomite::Bases)

add_executable(tst_bases_builtins tst_builtins.cpp)
add_test(NAME tst_bases_builtins COMMAND tst_bases_builtins)
target_link_libraries(tst_bases_builtins PRIVATE Qt6::Test Corbomite::Bases)

add_executable(tst_bases_yaml_schema tst_yaml_schema.cpp)
add_test(NAME tst_bases_yaml_schema COMMAND tst_bases_yaml_schema)
target_link_libraries(tst_bases_yaml_schema PRIVATE Qt6::Test Corbomite::Bases)
