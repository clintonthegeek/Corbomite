corbomite_add_plugin(corbomite-file-explorer
    METADATA_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json.in"
    SOURCES
        FileExplorerPlugin.cpp
        FileExplorerView.cpp
    TRUSTED
    LINK_LIBRARIES
        Qt6::Widgets
        KF6::I18n
        Corbomite::Core
        Corbomite::Models
        Corbomite::Vault)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
