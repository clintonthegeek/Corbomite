cmake_minimum_required(VERSION 3.19)
project(Corbomite_SearchTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_executable(tst_fuzzymatcher tst_fuzzymatcher.cpp)
add_test(NAME tst_fuzzymatcher COMMAND tst_fuzzymatcher)
target_link_libraries(tst_fuzzymatcher PRIVATE Qt6::Test Corbomite::Search)

add_executable(tst_fuzzy_penalty_rule tst_fuzzy_penalty_rule.cpp)
add_test(NAME tst_fuzzy_penalty_rule COMMAND tst_fuzzy_penalty_rule)
target_link_libraries(tst_fuzzy_penalty_rule PRIVATE Qt6::Test Corbomite::Search)

add_executable(tst_fuzzy_simple_search tst_fuzzy_simple_search.cpp)
add_test(NAME tst_fuzzy_simple_search COMMAND tst_fuzzy_simple_search)
target_link_libraries(tst_fuzzy_simple_search PRIVATE Qt6::Test Corbomite::Search)

add_executable(tst_searchdsl tst_searchdsl.cpp)
add_test(NAME tst_searchdsl COMMAND tst_searchdsl)
target_link_libraries(tst_searchdsl PRIVATE Qt6::Test Corbomite::Search)

add_executable(tst_search_dsl_grammar tst_search_dsl_grammar.cpp)
add_test(NAME tst_search_dsl_grammar COMMAND tst_search_dsl_grammar)
target_link_libraries(tst_search_dsl_grammar PRIVATE Qt6::Test Corbomite::Search)
