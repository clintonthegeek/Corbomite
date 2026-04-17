add_library(corbomite-search-plugin MODULE)
target_sources(corbomite-search-plugin PRIVATE
    SearchPlugin.cpp
    SearchView.cpp
)
target_link_libraries(corbomite-search-plugin PRIVATE
    Qt6::Widgets
    KF6::I18n
    KF6::CoreAddons
    Corbomite::Core
    Corbomite::Search
    Corbomite::Storage
    Corbomite::Vault
    Corbomite::Models
)
set_target_properties(corbomite-search-plugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CORBOMITE_PLUGIN_DEV_DIR}"
    PREFIX ""
    OUTPUT_NAME "corbomite-search"
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
