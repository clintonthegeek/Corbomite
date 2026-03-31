cmake_minimum_required(VERSION 3.19)
project(Corbomite_EditorTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test Widgets)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_executable(tst_obsidian_highlighting tst_obsidian_highlighting.cpp)
add_test(NAME tst_obsidian_highlighting COMMAND tst_obsidian_highlighting)
target_link_libraries(tst_obsidian_highlighting PRIVATE
    Qt6::Test Qt6::Widgets qmarkdowntextedit)
set_tests_properties(tst_obsidian_highlighting PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
