cmake_minimum_required(VERSION 3.19)
project(Corbomite_CoreTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_executable(tst_notemeta tst_notemeta.cpp)
add_test(NAME tst_notemeta COMMAND tst_notemeta)
target_link_libraries(tst_notemeta PRIVATE Qt6::Test Corbomite::Core)

add_executable(tst_notedocument tst_notedocument.cpp)
add_test(NAME tst_notedocument COMMAND tst_notedocument)
target_link_libraries(tst_notedocument PRIVATE Qt6::Test Corbomite::Core)
set_tests_properties(tst_notedocument PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_markdownrenderer tst_markdownrenderer.cpp)
add_test(NAME tst_markdownrenderer COMMAND tst_markdownrenderer)
target_link_libraries(tst_markdownrenderer PRIVATE Qt6::Test Corbomite::Core)

add_executable(tst_rendereddocument tst_rendereddocument.cpp)
add_test(NAME tst_rendereddocument COMMAND tst_rendereddocument)
target_link_libraries(tst_rendereddocument PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_rendereddocument PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_subpath tst_subpath.cpp)
add_test(NAME tst_subpath COMMAND tst_subpath)
target_link_libraries(tst_subpath PRIVATE Qt6::Test Corbomite::Core)

add_executable(tst_renderengine tst_renderengine.cpp)
add_test(NAME tst_renderengine COMMAND tst_renderengine)
target_link_libraries(tst_renderengine PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_renderengine PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
