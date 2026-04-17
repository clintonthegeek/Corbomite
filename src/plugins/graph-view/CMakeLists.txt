add_library(corbomite-graph-view MODULE)
target_sources(corbomite-graph-view PRIVATE
    GraphViewPlugin.cpp
)
target_link_libraries(corbomite-graph-view PRIVATE
    Qt6::Widgets
    KF6::I18n
    KF6::CoreAddons
    Corbomite::Core
    Corbomite::Vault
)
set_target_properties(corbomite-graph-view PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CORBOMITE_PLUGIN_DEV_DIR}"
    PREFIX ""
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
