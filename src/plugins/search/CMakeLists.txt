corbomite_add_plugin(corbomite-search-plugin
    METADATA_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json.in"
    SOURCES
        SearchPlugin.cpp
        SearchView.cpp
    TRUSTED
    LINK_LIBRARIES
        Qt6::Widgets
        KF6::I18n
        Corbomite::Core
        Corbomite::Search
        Corbomite::Storage
        Corbomite::Vault
        Corbomite::Models)

# Preserve the artifact name (corbomite-search) so PluginManager config
# keys and CorbomiteApp dev discovery don't need to change.
set_target_properties(corbomite-search-plugin PROPERTIES
    OUTPUT_NAME "corbomite-search")

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
