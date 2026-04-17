cmake_minimum_required(VERSION 3.19)
project(corbomite-bases VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets)
find_package(KF6I18n REQUIRED)

add_library(corbomite-bases STATIC
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
        $<BUILD_INTERFACE:MarkoffParser::MarkoffParser>
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
