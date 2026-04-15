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

add_executable(tst_linkutils tst_linkutils.cpp)
add_test(NAME tst_linkutils COMMAND tst_linkutils)
target_link_libraries(tst_linkutils PRIVATE Qt6::Test Corbomite::Core MarkoffParser::MarkoffParser)

add_executable(tst_frontmatterwriter tst_frontmatterwriter.cpp)
add_test(NAME tst_frontmatterwriter COMMAND tst_frontmatterwriter)
target_link_libraries(tst_frontmatterwriter PRIVATE Qt6::Test Corbomite::Core MarkoffParser::MarkoffParser)

add_executable(tst_panelayout tst_panelayout.cpp)
add_test(NAME tst_panelayout COMMAND tst_panelayout)
target_link_libraries(tst_panelayout PRIVATE Qt6::Test Corbomite::Core)

add_executable(tst_panelayoutbridge tst_panelayoutbridge.cpp)
add_test(NAME tst_panelayoutbridge COMMAND tst_panelayoutbridge)
target_link_libraries(tst_panelayoutbridge PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_panelayoutbridge PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_component tst_component.cpp)
add_test(NAME tst_component COMMAND tst_component)
target_link_libraries(tst_component PRIVATE Qt6::Test Corbomite::Core)
set_tests_properties(tst_component PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_events tst_events.cpp)
add_test(NAME tst_events COMMAND tst_events)
target_link_libraries(tst_events PRIVATE Qt6::Test Corbomite::Core)
set_tests_properties(tst_events PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_scope tst_scope.cpp)
add_test(NAME tst_scope COMMAND tst_scope)
target_link_libraries(tst_scope PRIVATE Qt6::Test Qt6::Gui Corbomite::Core)
set_tests_properties(tst_scope PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_scopemanager tst_scopemanager.cpp)
add_test(NAME tst_scopemanager COMMAND tst_scopemanager)
target_link_libraries(tst_scopemanager PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_scopemanager PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_command tst_command.cpp)
add_test(NAME tst_command COMMAND tst_command)
target_link_libraries(tst_command PRIVATE Qt6::Test Corbomite::Core)
set_tests_properties(tst_command PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_hotkey tst_hotkey.cpp)
add_test(NAME tst_hotkey COMMAND tst_hotkey)
target_link_libraries(tst_hotkey PRIVATE Qt6::Test Corbomite::Core)
set_tests_properties(tst_hotkey PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_plugininstance tst_plugininstance.cpp)
add_test(NAME tst_plugininstance COMMAND tst_plugininstance)
target_link_libraries(tst_plugininstance PRIVATE Qt6::Test Corbomite::Core)
set_tests_properties(tst_plugininstance PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_renderengine tst_renderengine.cpp)
add_test(NAME tst_renderengine COMMAND tst_renderengine)
target_link_libraries(tst_renderengine PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_renderengine PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
