# PropertyEditorWidget/PropertyRow (src/sidebar/) used to be compiled a
# second time directly into this plugin's SOURCES, alongside CorbomiteApp
# compiling the same two files for its own sidebar. That produced two
# independent staticMetaObject definitions for PropertyRow,
# PropertyEditorWidget, and the six PropertyEditor subclasses nested in
# PropertyEditorWidget.cpp — a real, live plugin-boundary divergence
# caught by Cluster P Phase P4.T3's tst_no_duplicate_metaobjects guard,
# not a hypothetical one. Fixed by linking CorbomiteApp (SHARED since
# Phase P1) instead of recompiling; the plugin still needs the headers,
# so INCLUDE_DIRECTORIES stays.
corbomite_add_plugin(corbomite-properties
    METADATA_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json.in"
    SOURCES
        PropertiesPlugin.cpp
        PropertiesView.cpp
    TRUSTED
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}/../../sidebar
    LINK_LIBRARIES
        Qt6::Widgets
        KF6::I18n
        Corbomite::Core
        Corbomite::Storage
        Corbomite::Vault
        Corbomite::Models
        CorbomiteApp)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
