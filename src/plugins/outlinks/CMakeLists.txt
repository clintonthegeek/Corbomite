add_library(corbomite-outlinks MODULE)
target_sources(corbomite-outlinks PRIVATE
    OutlinksPlugin.cpp
    OutlinksView.cpp
)
target_link_libraries(corbomite-outlinks PRIVATE
    Qt6::Widgets
    KF6::I18n
    KF6::CoreAddons
    Corbomite::Core
    Corbomite::Storage
    Corbomite::Vault
)
set_target_properties(corbomite-outlinks PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CORBOMITE_PLUGIN_DEV_DIR}"
    PREFIX ""
)
