cmake_minimum_required(VERSION 3.19)
project(corbomite-core VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets)

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
    src/FrontMatterWriter.cpp
    include/corbomite/core/FrontMatterWriter.h
    src/PaneLayout.cpp
    include/corbomite/core/PaneLayout.h
    src/PaneLayoutBridge.cpp
    include/corbomite/core/PaneLayoutBridge.h
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
)
set_target_properties(corbomite-core PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(Corbomite::Core ALIAS corbomite-core)

target_include_directories(corbomite-core
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(corbomite-core PUBLIC Qt6::Core Qt6::Gui Qt6::Svg Qt6::Widgets KF6::SyntaxHighlighting jkqtmathtext mmdr markoff)
