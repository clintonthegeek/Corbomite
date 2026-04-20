cmake_minimum_required(VERSION 3.19)
project(Corbomite_BookmarksPluginTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test Widgets)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_AUTOMOC ON)

add_executable(tst_bookmarks_store tst_bookmarks_store.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../BookmarksStore.cpp)
target_include_directories(tst_bookmarks_store PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..)
add_test(NAME tst_bookmarks_store COMMAND tst_bookmarks_store)
target_link_libraries(tst_bookmarks_store PRIVATE
    Qt6::Test Qt6::Widgets
    Corbomite::Core Corbomite::Vault
    KF6::I18n)
set_tests_properties(tst_bookmarks_store PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
