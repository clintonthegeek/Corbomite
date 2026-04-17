add_library(corbomite-local-graph MODULE)
target_sources(corbomite-local-graph PRIVATE
    LocalGraphPlugin.cpp
    LocalGraphView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../graph/GraphDataBuilder.cpp
)
target_include_directories(corbomite-local-graph PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../graph
)
target_link_libraries(corbomite-local-graph PRIVATE
    Qt6::Widgets
    KF6::I18n
    KF6::CoreAddons
    Corbomite::Core
    Corbomite::Storage
    Corbomite::Vault
    forcegraph
)
set_target_properties(corbomite-local-graph PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CORBOMITE_PLUGIN_DEV_DIR}"
    PREFIX ""
)
