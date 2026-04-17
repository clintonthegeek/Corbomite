include(KDECMakeSettings)
kconfig_add_kcfg_files(CORBOMITE_KCFG_SRCS app/corbomitesettings.kcfgc GENERATE_MOC)

# Built-in plugin output directory for dev builds. Each plugin's
# CMakeLists.txt drops its .so here; CorbomiteApp adds this path to
# the PluginManager's system search list at startup so dev runs
# discover plugins from the build tree without an `install` step.
set(CORBOMITE_PLUGIN_DEV_DIR "${CMAKE_BINARY_DIR}/lib/plugins/corbomite"
    CACHE INTERNAL "Build-tree plugin output dir")

add_library(CorbomiteApp STATIC
    ${CORBOMITE_KCFG_SRCS}
    app/CorbomiteApp.cpp
    app/MainWindow.cpp
    app/RecentVaults.cpp
    app/SessionManager.cpp
    app/RibbonSlot.cpp
    app/WelcomeScreen.cpp
    mdi/CorbomiteMDI.cpp
    sidebar/PropertyEditorWidget.cpp
    editor/MarkdownView.cpp
    editor/MarkdownView.h
    editor/NoteEditorWidget.cpp
    editor/SourceEditor.cpp
    editor/ViewModeSerializer.cpp
    editor/CompletionPopup.cpp
    editor/CompletionDelegate.cpp
    editor/HoverPopover.cpp
    editor/WikiLinkSuggest.cpp
    editor/TagSuggest.cpp
    editor/VaultResourceProvider.cpp
    reactors/AutosaveReactor.cpp
    dialogs/CreateVaultDialog.cpp
    dialogs/Notice.cpp
    dialogs/MomentFormatPreview.cpp
    dialogs/SettingsDialog.cpp
    dialogs/PluginsPage.cpp
    dialogs/QuickSwitcherModel.cpp
    dialogs/QuickSwitcherDelegate.cpp
    dialogs/QuickSwitcher.cpp
    dialogs/TemplatePicker.cpp
    graph/GraphDataBuilder.cpp
    graph/GraphView.cpp
    graph/GraphView.h
    graph/GraphViewTab.cpp
    graph/CollapsibleSection.cpp
    graph/GraphControlsPanel.cpp
    canvas/CanvasFileView.cpp
    canvas/CanvasFileView.h
    canvas/CanvasViewTab.cpp
)

target_link_libraries(CorbomiteApp
    PUBLIC
        Qt6::Core
        Qt6::Widgets
        Qt6::DBus
        KF6::CoreAddons
        KF6::I18n
        KF6::XmlGui
        KF6::WidgetsAddons
        KF6::IconThemes
        KF6::ConfigCore
        KF6::ConfigWidgets
        KF6::DBusAddons
        Corbomite::Core
        Corbomite::Search
        Corbomite::Storage
        Corbomite::Models
        Corbomite::Vault
        Markoff::Markoff
        Corbomite::ReadingView
        Corbomite::QutepartSource
        forcegraph
        canvas
)

target_include_directories(CorbomiteApp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/app
    ${CMAKE_CURRENT_SOURCE_DIR}/mdi
    ${CMAKE_CURRENT_SOURCE_DIR}/editor
    ${CMAKE_CURRENT_SOURCE_DIR}/sidebar
    ${CMAKE_CURRENT_SOURCE_DIR}/reactors
    ${CMAKE_CURRENT_SOURCE_DIR}/dialogs
    ${CMAKE_CURRENT_SOURCE_DIR}/graph
    ${CMAKE_CURRENT_SOURCE_DIR}/canvas
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_definitions(CorbomiteApp PRIVATE
    CORBOMITE_PLUGIN_DEV_DIR="${CORBOMITE_PLUGIN_DEV_DIR}")

# Built-in plugins (Cluster Q migrations).
add_subdirectory(plugins/backlinks)
add_subdirectory(plugins/outlinks)
add_subdirectory(plugins/outline)
add_subdirectory(plugins/properties)
add_subdirectory(plugins/search)
add_subdirectory(plugins/file-explorer)
add_subdirectory(plugins/local-graph)
add_subdirectory(plugins/graph-view)
