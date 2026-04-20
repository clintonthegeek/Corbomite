corbomite_add_plugin(corbomite-bookmarks
    METADATA_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json.in"
    SOURCES
        BookmarksPlugin.cpp
        BookmarksCommands.cpp
        BookmarksView.cpp
        BookmarksModel.cpp
        BookmarksStore.cpp
        BookmarkModal.cpp
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
