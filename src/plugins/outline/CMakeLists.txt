corbomite_add_plugin(corbomite-outline
    METADATA_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json.in"
    SOURCES
        OutlinePlugin.cpp
        OutlineView.cpp
    TRUSTED
    LINK_LIBRARIES
        Qt6::Widgets
        KF6::I18n
        Corbomite::Core
        Corbomite::Storage
        Corbomite::Vault)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
