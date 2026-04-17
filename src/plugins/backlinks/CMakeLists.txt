add_library(corbomite-backlinks MODULE)
target_sources(corbomite-backlinks PRIVATE
    BacklinksPlugin.cpp
    BacklinksView.cpp
)
target_link_libraries(corbomite-backlinks PRIVATE
    Qt6::Widgets
    KF6::I18n
    KF6::CoreAddons
    Corbomite::Core
    Corbomite::Storage
    Corbomite::Vault
)
set_target_properties(corbomite-backlinks PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CORBOMITE_PLUGIN_DEV_DIR}"
    PREFIX ""
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
