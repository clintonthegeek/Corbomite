corbomite_add_plugin(corbomite-properties
    METADATA_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json.in"
    SOURCES
        PropertiesPlugin.cpp
        PropertiesView.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../../sidebar/PropertyEditorWidget.cpp
    TRUSTED
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}/../../sidebar
    LINK_LIBRARIES
        Qt6::Widgets
        KF6::I18n
        Corbomite::Core
        Corbomite::Storage
        Corbomite::Vault
        Corbomite::Models)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
