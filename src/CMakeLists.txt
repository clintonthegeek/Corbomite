include(KDECMakeSettings)
kconfig_add_kcfg_files(CORBOMITE_KCFG_SRCS app/corbomitesettings.kcfgc GENERATE_MOC)

add_library(CorbomiteApp STATIC
    ${CORBOMITE_KCFG_SRCS}
    app/CorbomiteApp.cpp
    app/MainWindow.cpp
    app/VaultService.cpp
    app/SessionManager.cpp
    mdi/CorbomiteMDI.cpp
    sidebar/FileExplorerPanel.cpp
    sidebar/SearchPanel.cpp
    sidebar/BacklinksPanel.cpp
    sidebar/OutlinksPanel.cpp
    sidebar/OutlinePanel.cpp
    editor/NoteEditorWidget.cpp
    editor/EditorViewSpace.cpp
    editor/EditorViewManager.cpp
    editor/CompletionPopup.cpp
    editor/CompletionDelegate.cpp
    editor/NotePreviewWidget.cpp
    reactors/AutosaveReactor.cpp
    reactors/FileWatchReactor.cpp
    dialogs/SettingsDialog.cpp
    dialogs/QuickSwitcherModel.cpp
    dialogs/QuickSwitcherDelegate.cpp
    dialogs/QuickSwitcher.cpp
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
        Corbomite::Storage
        Corbomite::Models
        qmarkdowntextedit
)

target_include_directories(CorbomiteApp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/app
    ${CMAKE_CURRENT_SOURCE_DIR}/mdi
    ${CMAKE_CURRENT_SOURCE_DIR}/editor
    ${CMAKE_CURRENT_SOURCE_DIR}/sidebar
    ${CMAKE_CURRENT_SOURCE_DIR}/reactors
    ${CMAKE_CURRENT_SOURCE_DIR}/dialogs
    ${CMAKE_CURRENT_BINARY_DIR}
)
