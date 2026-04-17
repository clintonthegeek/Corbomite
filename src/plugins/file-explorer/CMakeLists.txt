add_library(corbomite-file-explorer MODULE)
target_sources(corbomite-file-explorer PRIVATE
    FileExplorerPlugin.cpp
    FileExplorerView.cpp
)
target_link_libraries(corbomite-file-explorer PRIVATE
    Qt6::Widgets
    KF6::I18n
    KF6::CoreAddons
    Corbomite::Core
    Corbomite::Models
    Corbomite::Vault
)
set_target_properties(corbomite-file-explorer PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CORBOMITE_PLUGIN_DEV_DIR}"
    PREFIX ""
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
