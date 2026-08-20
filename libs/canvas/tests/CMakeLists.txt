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

# Cluster M Task M3.1 — Obsidian-exact bezier strategy.
add_executable(tst_canvasbezierstrategy tst_canvasbezierstrategy.cpp)
add_test(NAME tst_canvasbezierstrategy COMMAND tst_canvasbezierstrategy)
target_link_libraries(tst_canvasbezierstrategy PRIVATE Qt6::Test canvas)
set_tests_properties(tst_canvasbezierstrategy PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Cluster M Task M4.1 — grid + object snap alignment strategy, plus
# M4.2's Shift axis-lock (same align() callback).
add_executable(tst_canvas_alignment tst_canvas_alignment.cpp)
add_test(NAME tst_canvas_alignment COMMAND tst_canvas_alignment)
target_link_libraries(tst_canvas_alignment PRIVATE Qt6::Test Qt6::Widgets canvas)
set_tests_properties(tst_canvas_alignment PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Cluster R Task 3.5 — CanvasScene export pipeline (image + SVG).
add_executable(tst_canvas_export tst_canvas_export.cpp)
add_test(NAME tst_canvas_export COMMAND tst_canvas_export)
target_link_libraries(tst_canvas_export PRIVATE Qt6::Test Qt6::Widgets canvas)
set_tests_properties(tst_canvas_export PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
