cmake_minimum_required(VERSION 3.19)
project(corbomite-models VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets)

add_library(corbomite-models SHARED
    src/TabModel.cpp
    include/corbomite/models/TabModel.h
    src/NotesTreeModel.cpp
    include/corbomite/models/NotesTreeModel.h
    src/SearchResultsModel.cpp
    include/corbomite/models/SearchResultsModel.h
    src/TemplateService.cpp
    include/corbomite/models/TemplateService.h
    src/DailyNoteService.cpp
    include/corbomite/models/DailyNoteService.h
    src/PropertyTypeInference.cpp
    include/corbomite/models/PropertyType.h
    include/corbomite/models/PropertyTypeInference.h
)
set_target_properties(corbomite-models PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    EXPORT_NAME Models)
add_library(Corbomite::Models ALIAS corbomite-models)

target_include_directories(corbomite-models
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(corbomite-models
    PUBLIC
        Qt6::Core Qt6::Widgets
    # MarkoffParser is a submodule dep; wrapped in BUILD_INTERFACE so it
    # doesn't leak into the installed Corbomite package. See
    # libs/core/CMakeLists.txt for the full rationale.
    PUBLIC
        $<BUILD_INTERFACE:Markoff::Parser>)

target_link_libraries(corbomite-models PUBLIC Corbomite::Vault)

if(NOT PROJECT_IS_TOP_LEVEL)
    target_link_libraries(corbomite-models PUBLIC Corbomite::Core Corbomite::Storage)
endif()

# Install rules (public consumption via find_package(Corbomite)).
if(DEFINED KDE_INSTALL_INCLUDEDIR)
    install(TARGETS corbomite-models
        EXPORT CorbomiteTargets
        RUNTIME DESTINATION ${KDE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${KDE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${KDE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${KDE_INSTALL_INCLUDEDIR}
    )
    install(DIRECTORY include/corbomite/models/
        DESTINATION ${KDE_INSTALL_INCLUDEDIR}/corbomite/models
    )
endif()
