corbomite_add_plugin(corbomite-local-graph
    METADATA_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json.in"
    SOURCES
        LocalGraphPlugin.cpp
        LocalGraphView.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../../graph/GraphDataBuilder.cpp
    TRUSTED
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}/../../graph
    LINK_LIBRARIES
        Qt6::Widgets
        KF6::I18n
        Corbomite::Core
        Corbomite::Storage
        Corbomite::Vault
        Corbomite::ForceGraph)

if(CORBOMITE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
