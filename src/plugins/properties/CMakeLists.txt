# PropertyEditorWidget/PropertyRow used to live in src/sidebar/ and be
# compiled a second time directly into this plugin's SOURCES, alongside
# CorbomiteApp compiling the same two files for its own sidebar. That
# produced two independent staticMetaObject definitions for PropertyRow,
# PropertyEditorWidget, and the six PropertyEditor subclasses nested in
# PropertyEditorWidget.cpp — a real, live plugin-boundary divergence
# caught by the tst_no_duplicate_metaobjects guard,
# not a hypothetical one. Fixed by linking CorbomiteApp (SHARED)
# instead of recompiling. The two classes moved into this plugin's own
# directory on 2026-10-20 (the src/sidebar/ orphan dir is gone), so the
# plugin no longer needs an extra include directory for them.
corbomite_add_plugin(corbomite-properties
    METADATA_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json.in"
    SOURCES
        PropertiesPlugin.cpp
        PropertiesView.cpp
    TRUSTED
    LINK_LIBRARIES
        Qt6::Widgets
        KF6::I18n
        Corbomite::Core
        Corbomite::Storage
        Corbomite::Vault
        Corbomite::Models
        CorbomiteApp)

if(CORBOMITE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
