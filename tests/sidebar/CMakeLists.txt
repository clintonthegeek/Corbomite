cmake_minimum_required(VERSION 3.19)
project(Corbomite_SidebarTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test Widgets)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_executable(tst_propertiespanel tst_propertiespanel.cpp)
add_test(NAME tst_propertiespanel COMMAND tst_propertiespanel)
target_link_libraries(tst_propertiespanel PRIVATE
    Qt6::Test
    Qt6::Widgets
    CorbomiteApp
    Corbomite::Core
    Corbomite::Models
    Corbomite::Storage
    MarkoffParser::MarkoffParser
)
set_tests_properties(tst_propertiespanel PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
