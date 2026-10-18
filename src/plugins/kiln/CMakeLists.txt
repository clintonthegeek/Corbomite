corbomite_add_plugin(corbomite-kiln
    METADATA_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json.in"
    SOURCES
        KilnPlugin.cpp
        KilnRunner.cpp
        KilnSettings.cpp
        KilnView.cpp
    TRUSTED
    LINK_LIBRARIES
        Qt6::Widgets
        KF6::I18n
        Corbomite::Core
        Corbomite::Vault)

if(CORBOMITE_PORT_BUILD_TESTS)
    add_subdirectory(tests)
endif()
