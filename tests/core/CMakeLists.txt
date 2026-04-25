cmake_minimum_required(VERSION 3.19)
project(Corbomite_CoreTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test)
find_package(KDDockWidgets-qt6 2.0 REQUIRED)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_executable(tst_menusectionhelper tst_menusectionhelper.cpp)
add_test(NAME tst_menusectionhelper COMMAND tst_menusectionhelper)
target_link_libraries(tst_menusectionhelper PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_menusectionhelper PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_platform tst_platform.cpp)
add_test(NAME tst_platform COMMAND tst_platform)
target_link_libraries(tst_platform PRIVATE Qt6::Test Corbomite::Core)
set_tests_properties(tst_platform PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_path_utils tst_path_utils.cpp)
add_test(NAME tst_path_utils COMMAND tst_path_utils)
target_link_libraries(tst_path_utils PRIVATE Qt6::Test Corbomite::Core)
set_tests_properties(tst_path_utils PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_view_more_options tst_view_more_options.cpp)
add_test(NAME tst_view_more_options COMMAND tst_view_more_options)
target_link_libraries(tst_view_more_options PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_view_more_options PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_editable_file_view_menu tst_editable_file_view_menu.cpp)
add_test(NAME tst_editable_file_view_menu COMMAND tst_editable_file_view_menu)
target_link_libraries(tst_editable_file_view_menu PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_editable_file_view_menu PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

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

add_executable(tst_postprocessorregistry tst_postprocessorregistry.cpp)
add_test(NAME tst_postprocessorregistry COMMAND tst_postprocessorregistry)
target_link_libraries(tst_postprocessorregistry PRIVATE Qt6::Test Corbomite::Core)
set_tests_properties(tst_postprocessorregistry PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_diffmatchpatch tst_diffmatchpatch.cpp)
add_test(NAME tst_diffmatchpatch COMMAND tst_diffmatchpatch)
target_link_libraries(tst_diffmatchpatch PRIVATE Qt6::Test Corbomite::Core)

add_executable(tst_view_lifecycle tst_view_lifecycle.cpp)
add_test(NAME tst_view_lifecycle COMMAND tst_view_lifecycle)
target_link_libraries(tst_view_lifecycle PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_view_lifecycle PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_view_zoom tst_view_zoom.cpp)
add_test(NAME tst_view_zoom COMMAND tst_view_zoom)
target_link_libraries(tst_view_zoom PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_view_zoom PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

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

add_executable(tst_leaf_service_propagation tst_leaf_service_propagation.cpp)
add_test(NAME tst_leaf_service_propagation COMMAND tst_leaf_service_propagation)
target_link_libraries(tst_leaf_service_propagation PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_leaf_service_propagation PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_fileview_setState tst_fileview_setState.cpp)
add_test(NAME tst_fileview_setState COMMAND tst_fileview_setState)
target_link_libraries(tst_fileview_setState PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_fileview_setState PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_workspace_leaf_navigate tst_workspace_leaf_navigate.cpp)
add_test(NAME tst_workspace_leaf_navigate COMMAND tst_workspace_leaf_navigate)
target_link_libraries(tst_workspace_leaf_navigate PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_workspace_leaf_navigate PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_plugin_metadata tst_plugin_metadata.cpp)
add_test(NAME tst_plugin_metadata COMMAND tst_plugin_metadata)
target_link_libraries(tst_plugin_metadata PRIVATE Qt6::Test Corbomite::Core KF6::CoreAddons)

add_executable(tst_plugin tst_plugin.cpp)
add_test(NAME tst_plugin COMMAND tst_plugin)
target_link_libraries(tst_plugin PRIVATE Qt6::Test Corbomite::Core Corbomite::Vault KF6::CoreAddons)
set_tests_properties(tst_plugin PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_plugin_context tst_plugin_context.cpp)
add_test(NAME tst_plugin_context COMMAND tst_plugin_context)
target_link_libraries(tst_plugin_context PRIVATE Qt6::Test Corbomite::Core Corbomite::Vault Corbomite::Storage KF6::CoreAddons)
set_tests_properties(tst_plugin_context PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_plugin_permission_dialog tst_plugin_permission_dialog.cpp)
add_test(NAME tst_plugin_permission_dialog COMMAND tst_plugin_permission_dialog)
target_link_libraries(tst_plugin_permission_dialog PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core Corbomite::Vault)
set_tests_properties(tst_plugin_permission_dialog PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_plugin_manager tst_plugin_manager.cpp)
add_test(NAME tst_plugin_manager COMMAND tst_plugin_manager)
target_link_libraries(tst_plugin_manager PRIVATE Qt6::Test Corbomite::Core Corbomite::Vault KF6::CoreAddons)
set_tests_properties(tst_plugin_manager PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_plugin_manager_lifecycle tst_plugin_manager_lifecycle.cpp)
add_test(NAME tst_plugin_manager_lifecycle COMMAND tst_plugin_manager_lifecycle)
target_link_libraries(tst_plugin_manager_lifecycle PRIVATE
    Qt6::Test Qt6::Widgets Corbomite::Core Corbomite::Vault KF6::CoreAddons KF6::ConfigCore)
set_tests_properties(tst_plugin_manager_lifecycle PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_proxy_metadata tst_proxy_metadata.cpp)
add_test(NAME tst_proxy_metadata COMMAND tst_proxy_metadata)
target_link_libraries(tst_proxy_metadata PRIVATE Qt6::Test Corbomite::Core Corbomite::Storage)

add_executable(tst_proxy_workspace tst_proxy_workspace.cpp)
add_test(NAME tst_proxy_workspace COMMAND tst_proxy_workspace)
target_link_libraries(tst_proxy_workspace PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_proxy_workspace PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_proxy_ui tst_proxy_ui.cpp)
add_test(NAME tst_proxy_ui COMMAND tst_proxy_ui)
target_link_libraries(tst_proxy_ui PRIVATE Qt6::Test Qt6::Widgets Corbomite::Core)
set_tests_properties(tst_proxy_ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_proxy_secrets_process tst_proxy_secrets_process.cpp)
add_test(NAME tst_proxy_secrets_process COMMAND tst_proxy_secrets_process)
target_link_libraries(tst_proxy_secrets_process PRIVATE Qt6::Test Corbomite::Core)

# Cluster Y Phase 1: risk reducer — verify KDDW MainWindow embeds in KXmlGuiWindow
add_executable(tst_workspace_embed_kxmlgui tst_workspace_embed_kxmlgui.cpp)
add_test(NAME tst_workspace_embed_kxmlgui COMMAND tst_workspace_embed_kxmlgui)
target_link_libraries(tst_workspace_embed_kxmlgui PRIVATE
    Qt6::Test Qt6::Widgets
    Corbomite::Core
    KF6::XmlGui KF6::ConfigWidgets
    KDAB::kddockwidgets)
set_tests_properties(tst_workspace_embed_kxmlgui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Cluster Y Phase 3: WorkspaceSerializer round-trip against synthetic KDDW trees
add_executable(tst_workspace_serializer tst_workspace_serializer.cpp)
add_test(NAME tst_workspace_serializer COMMAND tst_workspace_serializer)
target_link_libraries(tst_workspace_serializer PRIVATE
    Qt6::Test Qt6::Widgets
    Corbomite::Core
    KDAB::kddockwidgets)
target_include_directories(tst_workspace_serializer PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/core/src)
target_compile_definitions(tst_workspace_serializer PRIVATE
    CORBOMITE_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
set_tests_properties(tst_workspace_serializer PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Cluster Y Phase 6: layoutReady gate + WorkspaceActiveLeafRouter
add_executable(tst_workspace_active_leaf_router tst_workspace_active_leaf_router.cpp)
add_test(NAME tst_workspace_active_leaf_router COMMAND tst_workspace_active_leaf_router)
target_link_libraries(tst_workspace_active_leaf_router PRIVATE
    Qt6::Test Qt6::Widgets
    Corbomite::Core
    KDAB::kddockwidgets)
set_tests_properties(tst_workspace_active_leaf_router PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Cluster Y Phase 5: popout leaves atop KDDW FloatingWindow
add_executable(tst_workspace_popout tst_workspace_popout.cpp)
add_test(NAME tst_workspace_popout COMMAND tst_workspace_popout)
target_link_libraries(tst_workspace_popout PRIVATE
    Qt6::Test Qt6::Widgets
    Corbomite::Core
    KDAB::kddockwidgets)
target_include_directories(tst_workspace_popout PRIVATE
    ${CMAKE_SOURCE_DIR}/libs/core/src)
set_tests_properties(tst_workspace_popout PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
