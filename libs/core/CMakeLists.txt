cmake_minimum_required(VERSION 3.19)
project(corbomite-core VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets Network)
find_package(KF6I18n REQUIRED)
find_package(KF6CoreAddons REQUIRED)
find_package(KF6Config REQUIRED)
find_package(KF6ColorScheme REQUIRED)
# Cluster O Phase O3 (O3.T1) — ViewActions is a KXMLGUIClient (the Tier-A
# provider mechanism), so libs/core now needs KF6::XmlGui.
find_package(KF6XmlGui REQUIRED)

add_library(corbomite-core SHARED
    src/NoteMeta.cpp
    src/NoteDocument.cpp
    include/corbomite/core/NoteDocument.h
    src/MarkdownRenderer.cpp
    src/RenderedDocument.cpp
    src/MarkdownRenderEngine.cpp
    src/SubpathExtract.cpp
    include/corbomite/core/SubpathExtract.h
    src/RegexRenderEngine.cpp
    src/MarkoffRenderEngine.cpp
    src/StyledRenderEngine.cpp
    include/corbomite/core/StyledRenderEngine.h
    src/LinkUtils.cpp
    include/corbomite/core/LinkUtils.h
    src/Component.cpp
    include/corbomite/core/Component.h
    src/Events.cpp
    include/corbomite/core/Events.h
    src/Scope.cpp
    include/corbomite/core/Scope.h
    src/ScopeManager.cpp
    include/corbomite/core/ScopeManager.h
    src/Command.cpp
    include/corbomite/core/Command.h
    src/Hotkey.cpp
    include/corbomite/core/Hotkey.h
    src/PluginInstance.cpp
    include/corbomite/core/PluginInstance.h
    src/MenuSectionHelper.cpp
    include/corbomite/core/MenuSectionHelper.h
    src/Platform.cpp
    include/corbomite/core/Platform.h
    src/PathUtils.cpp
    include/corbomite/core/PathUtils.h
    src/MenuEventEmitter.cpp
    include/corbomite/core/MenuEventEmitter.h
    src/HoverLinkSourceRegistry.cpp
    include/corbomite/core/HoverLinkSourceRegistry.h
    include/corbomite/core/HoverLinkSource.h
    src/EditorSuggestManager.cpp
    include/corbomite/core/EditorSuggestManager.h
    include/corbomite/core/EditorSuggest.h
    src/MomentFormatter.cpp
    include/corbomite/core/MomentFormatter.h
    include/corbomite/core/VaultResourceProvider.h
    src/MarkdownRenderChild.cpp
    include/corbomite/core/MarkdownRenderChild.h
    src/EmbedDepthGuard.cpp
    include/corbomite/core/EmbedDepthGuard.h
    src/PostProcessorRegistry.cpp
    include/corbomite/core/PostProcessorRegistry.h
    src/MermaidRenderer.cpp
    include/corbomite/core/MermaidRenderer.h
    src/DiffMatchPatch.cpp
    include/corbomite/core/DiffMatchPatch.h
    src/View.cpp
    include/corbomite/core/View.h
    src/ViewActions.cpp
    include/corbomite/core/ViewActions.h
    include/corbomite/core/ToolBarPolicy.h
    src/EmptyView.cpp
    include/corbomite/core/EmptyView.h
    src/ItemView.cpp
    include/corbomite/core/ItemView.h
    src/FileView.cpp
    include/corbomite/core/FileView.h
    src/EditableFileView.cpp
    include/corbomite/core/EditableFileView.h
    src/TextFileView.cpp
    include/corbomite/core/TextFileView.h
    include/corbomite/core/DataAdapter.h
    src/ViewRegistry.cpp
    include/corbomite/core/ViewRegistry.h
    src/WorkspaceLeaf.cpp
    include/corbomite/core/WorkspaceLeaf.h
    src/WorkspaceKddwInit.cpp
    src/WorkspaceKddwInit.h
    src/LeafHistory.cpp
    include/corbomite/core/LeafHistory.h
    src/Workspace.cpp
    include/corbomite/core/Workspace.h
    src/WorkspaceActiveLeafRouter.cpp
    include/corbomite/core/WorkspaceActiveLeafRouter.h
    src/WorkspaceWindow.cpp
    include/corbomite/core/WorkspaceWindow.h
    src/WorkspaceFloating.cpp
    include/corbomite/core/WorkspaceFloating.h
    src/PluginMetaData.cpp
    include/corbomite/core/PluginMetaData.h
    include/corbomite/core/PluginApi.h
    src/proxies/WorkspaceController.cpp
    include/corbomite/core/proxies/WorkspaceController.h
    src/proxies/CommandRegistrar.cpp
    include/corbomite/core/proxies/CommandRegistrar.h
    src/proxies/ViewRegistrar.cpp
    include/corbomite/core/proxies/ViewRegistrar.h
    src/proxies/MenuInjector.cpp
    include/corbomite/core/proxies/MenuInjector.h
    src/proxies/SecretStorage.cpp
    include/corbomite/core/proxies/SecretStorage.h
    src/proxies/ProcessSpawner.cpp
    include/corbomite/core/proxies/ProcessSpawner.h
    src/proxies/HoverLinkSourceRegistrar.cpp
    include/corbomite/core/proxies/HoverLinkSourceRegistrar.h
    src/proxies/EditorSuggestRegistrar.cpp
    include/corbomite/core/proxies/EditorSuggestRegistrar.h
    src/proxies/PostProcessorRegistrar.cpp
    include/corbomite/core/proxies/PostProcessorRegistrar.h
    src/proxies/RibbonRegistrar.cpp
    include/corbomite/core/proxies/RibbonRegistrar.h
    include/corbomite/core/RibbonHandle.h
    src/proxies/EmbedRegistrar.cpp
    include/corbomite/core/proxies/EmbedRegistrar.h
    src/proxies/CodeBlockRegistrar.cpp
    include/corbomite/core/proxies/CodeBlockRegistrar.h
    src/StatusBarRegistry.cpp
    include/corbomite/core/StatusBarRegistry.h
    src/proxies/StatusBarRegistrar.cpp
    include/corbomite/core/proxies/StatusBarRegistrar.h
    src/LucideIconRegistry.cpp
    include/corbomite/core/LucideIconRegistry.h
    src/proxies/LucideIconRegistrar.cpp
    include/corbomite/core/proxies/LucideIconRegistrar.h
    src/MarkdownRenderer.cpp
    include/corbomite/core/MarkdownRenderer.h
    include/corbomite/core/Decoration.h
    src/DecorationProviderRegistry.cpp
    include/corbomite/core/DecorationProviderRegistry.h
    src/proxies/DecorationProviderRegistrar.cpp
    include/corbomite/core/proxies/DecorationProviderRegistrar.h
    src/ProtocolHandlerRegistry.cpp
    include/corbomite/core/ProtocolHandlerRegistry.h
    src/proxies/ProtocolHandlerRegistrar.cpp
    include/corbomite/core/proxies/ProtocolHandlerRegistrar.h
    include/corbomite/core/PluginPermissions.h
    src/SystemThemeBuilder.cpp
    include/corbomite/core/SystemThemeBuilder.h
    src/ThemeService.cpp
    include/corbomite/core/ThemeService.h
    src/WorkspaceSerializer.cpp
    src/WorkspaceSerializer.h
)
set_target_properties(corbomite-core PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    EXPORT_NAME Core)
add_library(Corbomite::Core ALIAS corbomite-core)

target_include_directories(corbomite-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(corbomite-core
    PUBLIC
        Qt6::Core Qt6::Gui Qt6::Svg Qt6::Widgets Qt6::Network
        KF6::SyntaxHighlighting KF6::I18n KF6::CoreAddons KF6::ConfigCore KF6::ColorScheme
        KF6::XmlGui
        KDAB::kddockwidgets
    # Submodule / bundled deps are linked only in the in-tree BUILD
    # interface — their .cmake config files aren't installed, so an
    # out-of-tree consumer can't find_package() them. This no longer means
    # "plugins must resolve these symbols themselves": since corbomite-core
    # is SHARED (Cluster P Phase P3), each submodule's object code is baked
    # into libcorbomite-core.so itself and plugins get it for free by
    # linking Corbomite::Core, exactly like any other symbol core exports.
    # BUILD_INTERFACE here is purely about satisfying CMake's target-lookup
    # at configure time in an installed tree, not about runtime symbol
    # availability.
    PUBLIC
        $<BUILD_INTERFACE:jkqtmathtext>
        $<BUILD_INTERFACE:mmdr>
        $<BUILD_INTERFACE:markoff-parser>
        $<BUILD_INTERFACE:markoff_core>
        $<BUILD_INTERFACE:markoff_styled>
        # TODO(port-foundation-exploration): markoff_reading retired.
        # $<BUILD_INTERFACE:markoff_reading>
    )

if(CORBOMITE_USE_KEYRING)
    target_link_libraries(corbomite-core PRIVATE Qt6Keychain::Qt6Keychain)
endif()

# DBus FileManager1 (Linux-only, used by Platform::showInFolder).
if(UNIX AND NOT APPLE)
    find_package(Qt6 REQUIRED COMPONENTS DBus)
    target_link_libraries(corbomite-core PRIVATE Qt6::DBus)
endif()

# Install rules (public consumption via find_package(Corbomite)).
# KDE_INSTALL_* is provided by the parent Corbomite tree's
# include(KDEInstallDirs); when this lib is built standalone the
# install step is skipped.
if(DEFINED KDE_INSTALL_INCLUDEDIR)
    install(TARGETS corbomite-core
        EXPORT CorbomiteTargets
        RUNTIME DESTINATION ${KDE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${KDE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${KDE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${KDE_INSTALL_INCLUDEDIR}
    )
    install(DIRECTORY include/corbomite/core/
        DESTINATION ${KDE_INSTALL_INCLUDEDIR}/corbomite/core
    )
endif()
