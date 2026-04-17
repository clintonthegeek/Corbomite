add_library(corbomite-properties MODULE)
target_sources(corbomite-properties PRIVATE
    PropertiesPlugin.cpp
    PropertiesView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../sidebar/PropertyEditorWidget.cpp
)
target_include_directories(corbomite-properties PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../sidebar
)
target_link_libraries(corbomite-properties PRIVATE
    Qt6::Widgets
    KF6::I18n
    KF6::CoreAddons
    Corbomite::Core
    Corbomite::Storage
    Corbomite::Vault
    Corbomite::Models
)
set_target_properties(corbomite-properties PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CORBOMITE_PLUGIN_DEV_DIR}"
    PREFIX ""
)
