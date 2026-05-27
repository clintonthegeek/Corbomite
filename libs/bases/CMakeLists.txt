cmake_minimum_required(VERSION 3.19)
project(corbomite-bases VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets)
find_package(KF6I18n REQUIRED)

add_library(corbomite-bases STATIC
    src/BasesCommands.cpp
    include/corbomite/bases/BasesCommands.h
    src/Value.cpp
    src/NullValue.cpp
    src/BooleanValue.cpp
    src/NumberValue.cpp
    src/StringValue.cpp
    src/ListValue.cpp
    src/DateValue.cpp
    src/DurationValue.cpp
    src/StringSubclasses.cpp
    src/RegExpValue.cpp
    src/ObjectValue.cpp
    src/FileValue.cpp
    src/Lexer.cpp
    include/corbomite/bases/Lexer.h
    include/corbomite/bases/Ast.h
    src/Parser.cpp
    include/corbomite/bases/Parser.h
    include/corbomite/bases/EvalContext.h
    src/Evaluator.cpp
    include/corbomite/bases/Evaluator.h
    src/FunctionRegistry.cpp
    include/corbomite/bases/FunctionRegistry.h
    src/Builtins.cpp
    src/Formula.cpp
    include/corbomite/bases/Formula.h
    src/PropertyId.cpp
    include/corbomite/bases/PropertyId.h
    src/FilterTree.cpp
    include/corbomite/bases/FilterTree.h
    src/BasesViewConfig.cpp
    include/corbomite/bases/BasesViewConfig.h
    src/SortCycle.cpp
    include/corbomite/bases/SortCycle.h
    src/TableExporter.cpp
    include/corbomite/bases/TableExporter.h
    src/CellHitTest.cpp
    include/corbomite/bases/CellHitTest.h
    src/ViewConfigOps.cpp
    include/corbomite/bases/ViewConfigOps.h
    src/NewItemSeed.cpp
    include/corbomite/bases/NewItemSeed.h
    src/FormulaOps.cpp
    include/corbomite/bases/FormulaOps.h
    src/FormulaCandidates.cpp
    include/corbomite/bases/FormulaCandidates.h
    src/BasesQuery.cpp
    include/corbomite/bases/BasesQuery.h
    src/BasesEntry.cpp
    include/corbomite/bases/BasesEntry.h
    src/BasesVaultResolver.cpp
    include/corbomite/bases/BasesVaultResolver.h
    src/BasesQueryResult.cpp
    include/corbomite/bases/BasesQueryResult.h
    src/QueryController.cpp
    include/corbomite/bases/QueryController.h
    src/BasesTableModel.cpp
    include/corbomite/bases/BasesTableModel.h
    src/BasesTreeModel.cpp
    include/corbomite/bases/BasesTreeModel.h
    src/BasesCellDelegate.cpp
    include/corbomite/bases/BasesCellDelegate.h
    src/BasesHeaderView.cpp
    include/corbomite/bases/BasesHeaderView.h
    src/BasesView.cpp
    include/corbomite/bases/BasesView.h
    src/PropertiesMenuPanel.cpp
    include/corbomite/bases/PropertiesMenuPanel.h
    src/PropertiesDrawer.cpp
    include/corbomite/bases/PropertiesDrawer.h
    src/SortGroupMenuPanel.cpp
    include/corbomite/bases/SortGroupMenuPanel.h
    src/ViewsMenuPanel.cpp
    include/corbomite/bases/ViewsMenuPanel.h
    src/FormulaInput.cpp
    include/corbomite/bases/FormulaInput.h
    include/corbomite/bases/ValuePtr.h
    include/corbomite/bases/Value.h
    include/corbomite/bases/Values.h
)
set_target_properties(corbomite-bases PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    EXPORT_NAME Bases)
add_library(Corbomite::Bases ALIAS corbomite-bases)

target_include_directories(corbomite-bases
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${KDE_INSTALL_INCLUDEDIR}>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(corbomite-bases
    PUBLIC
        Qt6::Core
        Qt6::Widgets
        KF6::I18n
        $<BUILD_INTERFACE:Corbomite::Core>
        $<BUILD_INTERFACE:Corbomite::Storage>
        $<BUILD_INTERFACE:Corbomite::Vault>
        $<BUILD_INTERFACE:Markoff::Parser>
)

# Install rules for find_package(Corbomite) third-party consumers.
if(DEFINED KDE_INSTALL_INCLUDEDIR)
    install(TARGETS corbomite-bases
        EXPORT CorbomiteTargets
        LIBRARY DESTINATION ${KDE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${KDE_INSTALL_LIBDIR})
    install(DIRECTORY include/corbomite/bases
            DESTINATION ${KDE_INSTALL_INCLUDEDIR}/corbomite)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
