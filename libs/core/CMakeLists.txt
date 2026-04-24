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

add_library(corbomite-core STATIC
    src/NoteMeta.cpp
    src/NoteDocument.cpp
    include/corbomite/core/NoteDocument.h
    src/MarkdownRenderer.cpp
    src/RenderedDocument.cpp
    src/MarkdownRenderEngine.cpp
    src/RegexRenderEngine.cpp
    src/MarkoffRenderEngine.cpp
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
    src/MarkoffAdapters.cpp
    include/corbomite/markoff_adapters/Adapters.h
    src/MermaidRenderer.cpp
    include/corbomite/core/MermaidRenderer.h
    src/DiffMatchPatch.cpp
    include/corbomite/core/DiffMatchPatch.h
    src/View.cpp
    include/corbomite/core/View.h
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
    src/ViewRegistry.cpp
    include/corbomite/core/ViewRegistry.h
    src/WorkspaceLeaf.cpp
    include/corbomite/core/WorkspaceLeaf.h
    src/LeafHistory.cpp
    include/corbomite/core/LeafHistory.h
    src/WorkspaceItem.cpp
    include/corbomite/core/WorkspaceItem.h
    src/WorkspaceParent.cpp
    include/corbomite/core/WorkspaceParent.h
    src/WorkspaceSplit.cpp
    include/corbomite/core/WorkspaceSplit.h
    src/WorkspaceTabs.cpp
    include/corbomite/core/WorkspaceTabs.h
    src/Workspace.cpp
    include/corbomite/core/Workspace.h
    src/WorkspaceWindow.cpp
    include/corbomite/core/WorkspaceWindow.h
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
    src/SystemThemeBuilder.cpp
    include/corbomite/core/SystemThemeBuilder.h
    src/ThemeService.cpp
    include/corbomite/core/ThemeService.h
)
set_target_properties(corbomite-core PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    EXPORT_NAME Core)
add_library(Corbomite::Core ALIAS corbomite-core)

target_include_directories(corbomite-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR}>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../storage/include
)
target_link_libraries(corbomite-core
    PUBLIC
        Qt6::Core Qt6::Gui Qt6::Svg Qt6::Widgets Qt6::Network
        KF6::SyntaxHighlighting KF6::I18n KF6::CoreAddons KF6::ConfigCore KF6::ColorScheme
        KDAB::kddockwidgets
    # Submodule / bundled deps are linked only in the in-tree BUILD
    # interface. They are not shipped as part of the installed Corbomite
    # package. Third-party plugins that link against an installed
    # Corbomite::Core must still be built in a tree that resolves these
    # symbols themselves (typically via add_subdirectory of Corbomite).
    PUBLIC
        $<BUILD_INTERFACE:jkqtmathtext>
        $<BUILD_INTERFACE:mmdr>
        $<BUILD_INTERFACE:markoff-parser>
        $<BUILD_INTERFACE:markoff_core>
    PRIVATE
        $<BUILD_INTERFACE:Corbomite::Storage>)

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
