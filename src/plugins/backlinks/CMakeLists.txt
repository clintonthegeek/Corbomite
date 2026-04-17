corbomite_add_plugin(corbomite-backlinks
    METADATA_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json.in"
    SOURCES
        BacklinksPlugin.cpp
        BacklinksView.cpp
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
