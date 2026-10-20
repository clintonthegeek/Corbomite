corbomite_add_plugin(corbomite-graph-view
    METADATA_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json.in"
    SOURCES
        GraphViewPlugin.cpp
        GraphViewActions.cpp
        GraphViewActions.h
        GraphView.cpp
        GraphViewTab.cpp
        GraphControlsPanel.cpp
        CollapsibleSection.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../../graph/GraphDataBuilder.cpp
    TRUSTED
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}/../../graph
    LINK_LIBRARIES
        Qt6::Widgets
        KF6::I18n
        KF6::WidgetsAddons
        Corbomite::Core
        Corbomite::Storage
        Corbomite::Vault
        forcegraph)

if(CORBOMITE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
