add_library(corbomite-outline MODULE)
target_sources(corbomite-outline PRIVATE
    OutlinePlugin.cpp
    OutlineView.cpp
)
target_link_libraries(corbomite-outline PRIVATE
    Qt6::Widgets
    KF6::I18n
    KF6::CoreAddons
    Corbomite::Core
    Corbomite::Storage
    Corbomite::Vault
)
set_target_properties(corbomite-outline PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CORBOMITE_PLUGIN_DEV_DIR}"
    PREFIX ""
)
