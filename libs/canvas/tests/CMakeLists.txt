cmake_minimum_required(VERSION 3.19)
project(canvas_tests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test Widgets)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_executable(tst_canvasdocument tst_canvasdocument.cpp)
add_test(NAME tst_canvasdocument COMMAND tst_canvasdocument)
target_link_libraries(tst_canvasdocument PRIVATE Qt6::Test canvas)
set_tests_properties(tst_canvasdocument PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_canvasscene tst_canvasscene.cpp)
add_test(NAME tst_canvasscene COMMAND tst_canvasscene)
target_link_libraries(tst_canvasscene PRIVATE Qt6::Test Qt6::Widgets canvas Corbomite::Core)
set_tests_properties(tst_canvasscene PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Cluster R Task 3.5 — CanvasScene export pipeline (image + SVG).
add_executable(tst_canvas_export tst_canvas_export.cpp)
add_test(NAME tst_canvas_export COMMAND tst_canvas_export)
target_link_libraries(tst_canvas_export PRIVATE Qt6::Test Qt6::Widgets canvas)
set_tests_properties(tst_canvas_export PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
