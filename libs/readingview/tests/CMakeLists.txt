cmake_minimum_required(VERSION 3.19)
project(readingview_tests LANGUAGES CXX)
enable_testing()

find_package(Qt6 REQUIRED COMPONENTS Test)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_executable(tst_readingview_bootstrap tst_readingview_bootstrap.cpp)
add_test(NAME tst_readingview_bootstrap COMMAND tst_readingview_bootstrap)
target_link_libraries(tst_readingview_bootstrap PRIVATE Qt6::Test Qt6::Gui readingview)
set_tests_properties(tst_readingview_bootstrap PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
