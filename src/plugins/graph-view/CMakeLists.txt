add_library(corbomite-graph-view MODULE)
target_sources(corbomite-graph-view PRIVATE
    GraphViewPlugin.cpp
    GraphView.cpp
    GraphViewTab.cpp
    GraphControlsPanel.cpp
    CollapsibleSection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../graph/GraphDataBuilder.cpp
)
target_include_directories(corbomite-graph-view PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../graph
)
target_link_libraries(corbomite-graph-view PRIVATE
    Qt6::Widgets
    KF6::I18n
    KF6::CoreAddons
    KF6::WidgetsAddons
    Corbomite::Core
    Corbomite::Storage
    Corbomite::Vault
    forcegraph
)
set_target_properties(corbomite-graph-view PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CORBOMITE_PLUGIN_DEV_DIR}"
    PREFIX ""
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
