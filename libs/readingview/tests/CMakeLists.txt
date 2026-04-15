cmake_minimum_required(VERSION 3.19)
project(readingview_tests LANGUAGES CXX)
enable_testing()

find_package(Qt6 REQUIRED COMPONENTS Test Widgets)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

function(add_rv_test NAME)
    add_executable(${NAME} ${NAME}.cpp)
    add_test(NAME ${NAME} COMMAND ${NAME})
    target_link_libraries(${NAME} PRIVATE
        Qt6::Test
        Qt6::Gui
        Qt6::Widgets
        readingview
    )
    set_tests_properties(${NAME} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

add_rv_test(tst_readingview_bootstrap)
add_rv_test(tst_readingpipeline_section_split)
add_rv_test(tst_sectionlayout_basic)
add_rv_test(tst_readingview_end_to_end)
