cmake_minimum_required(VERSION 3.19)
project(Corbomite_CoreTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_executable(tst_menusectionhelper tst_menusectionhelper.cpp)
add_test(NAME tst_menusectionhelper COMMAND tst_menusectionhelper)
target_link_libraries(tst_menusectionhelper PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_menusectionhelper PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_hoverlinksources tst_hoverlinksources.cpp)
add_test(NAME tst_hoverlinksources COMMAND tst_hoverlinksources)
target_link_libraries(tst_hoverlinksources PRIVATE Qt6::Test Corbomite::Core)

add_executable(tst_editorsuggest tst_editorsuggest.cpp)
add_test(NAME tst_editorsuggest COMMAND tst_editorsuggest)
target_link_libraries(tst_editorsuggest PRIVATE Qt6::Test Corbomite::Core)

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

add_executable(tst_momentformatter tst_momentformatter.cpp)
add_test(NAME tst_momentformatter COMMAND tst_momentformatter)
target_link_libraries(tst_momentformatter PRIVATE Qt6::Test Corbomite::Core)

add_executable(tst_vaultresourceprovider tst_vaultresourceprovider.cpp)
add_test(NAME tst_vaultresourceprovider COMMAND tst_vaultresourceprovider)
target_link_libraries(tst_vaultresourceprovider PRIVATE Qt6::Test Corbomite::Core)
set_tests_properties(tst_vaultresourceprovider PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_markdownrenderchild tst_markdownrenderchild.cpp)
add_test(NAME tst_markdownrenderchild COMMAND tst_markdownrenderchild)
target_link_libraries(tst_markdownrenderchild PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_markdownrenderchild PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_embeddepthguard tst_embeddepthguard.cpp)
add_test(NAME tst_embeddepthguard COMMAND tst_embeddepthguard)
target_link_libraries(tst_embeddepthguard PRIVATE Qt6::Test Corbomite::Core)
set_tests_properties(tst_embeddepthguard PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_embedregistry tst_embedregistry.cpp)
add_test(NAME tst_embedregistry COMMAND tst_embedregistry)
target_link_libraries(tst_embedregistry PRIVATE Qt6::Test Corbomite::Core)
set_tests_properties(tst_embedregistry PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_postprocessorregistry tst_postprocessorregistry.cpp)
add_test(NAME tst_postprocessorregistry COMMAND tst_postprocessorregistry)
target_link_libraries(tst_postprocessorregistry PRIVATE Qt6::Test Corbomite::Core)
set_tests_properties(tst_postprocessorregistry PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_codeblockprocessorregistry tst_codeblockprocessorregistry.cpp)
add_test(NAME tst_codeblockprocessorregistry COMMAND tst_codeblockprocessorregistry)
target_link_libraries(tst_codeblockprocessorregistry PRIVATE Qt6::Test Corbomite::Core)
set_tests_properties(tst_codeblockprocessorregistry PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_diffmatchpatch tst_diffmatchpatch.cpp)
add_test(NAME tst_diffmatchpatch COMMAND tst_diffmatchpatch)
target_link_libraries(tst_diffmatchpatch PRIVATE Qt6::Test Corbomite::Core)

add_executable(tst_view_lifecycle tst_view_lifecycle.cpp)
add_test(NAME tst_view_lifecycle COMMAND tst_view_lifecycle)
target_link_libraries(tst_view_lifecycle PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_view_lifecycle PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_textfileview tst_textfileview.cpp)
add_test(NAME tst_textfileview COMMAND tst_textfileview)
target_link_libraries(tst_textfileview PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core Corbomite::Storage)
set_tests_properties(tst_textfileview PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_textfileview_merge tst_textfileview_merge.cpp)
add_test(NAME tst_textfileview_merge COMMAND tst_textfileview_merge)
target_link_libraries(tst_textfileview_merge PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core Corbomite::Storage)
set_tests_properties(tst_textfileview_merge PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_viewregistry tst_viewregistry.cpp)
add_test(NAME tst_viewregistry COMMAND tst_viewregistry)
target_link_libraries(tst_viewregistry PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_viewregistry PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_workspaceleaf tst_workspaceleaf.cpp)
add_test(NAME tst_workspaceleaf COMMAND tst_workspaceleaf)
target_link_libraries(tst_workspaceleaf PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_workspaceleaf PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_leaf_history tst_leaf_history.cpp)
add_test(NAME tst_leaf_history COMMAND tst_leaf_history)
target_link_libraries(tst_leaf_history PRIVATE Qt6::Test Corbomite::Core)

add_executable(tst_workspace_tree tst_workspace_tree.cpp)
add_test(NAME tst_workspace_tree COMMAND tst_workspace_tree)
target_link_libraries(tst_workspace_tree PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_workspace_tree PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_workspace_tabs tst_workspace_tabs.cpp)
add_test(NAME tst_workspace_tabs COMMAND tst_workspace_tabs)
target_link_libraries(tst_workspace_tabs PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_workspace_tabs PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_workspace_serialize tst_workspace_serialize.cpp)
add_test(NAME tst_workspace_serialize COMMAND tst_workspace_serialize)
target_link_libraries(tst_workspace_serialize PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_workspace_serialize PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_leaf_undo tst_leaf_undo.cpp)
add_test(NAME tst_leaf_undo COMMAND tst_leaf_undo)
target_link_libraries(tst_leaf_undo PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_leaf_undo PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_workspace_deferred tst_workspace_deferred.cpp)
add_test(NAME tst_workspace_deferred COMMAND tst_workspace_deferred)
target_link_libraries(tst_workspace_deferred PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_workspace_deferred PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_workspace_window tst_workspace_window.cpp)
add_test(NAME tst_workspace_window COMMAND tst_workspace_window)
target_link_libraries(tst_workspace_window PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_workspace_window PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_workspace_integration tst_workspace_integration.cpp)
add_test(NAME tst_workspace_integration COMMAND tst_workspace_integration)
target_link_libraries(tst_workspace_integration PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_workspace_integration PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_workspace_session tst_workspace_session.cpp)
add_test(NAME tst_workspace_session COMMAND tst_workspace_session)
target_link_libraries(tst_workspace_session PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_workspace_session PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
