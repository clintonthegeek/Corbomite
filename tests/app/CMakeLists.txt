cmake_minimum_required(VERSION 3.19)
project(Corbomite_AppTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test Widgets)
find_package(KF6ColorScheme REQUIRED)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_executable(tst_recent_vaults tst_recent_vaults.cpp)
add_test(NAME tst_recent_vaults COMMAND tst_recent_vaults)
target_link_libraries(tst_recent_vaults PRIVATE Qt6::Test CorbomiteApp)
set_tests_properties(tst_recent_vaults PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Cluster R Task 3.3 — ExportToPdf wraps QPrinter+QTextDocument to write
# PDFs for a TFile via Vault::read. The test uses the no-dialog seam.
add_executable(tst_export_to_pdf tst_export_to_pdf.cpp)
add_test(NAME tst_export_to_pdf COMMAND tst_export_to_pdf)
target_link_libraries(tst_export_to_pdf PRIVATE
    Qt6::Test CorbomiteApp Corbomite::Vault Corbomite::Storage)
set_tests_properties(tst_export_to_pdf PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Cluster V Task 1.7 — smoke test for MainWindow::applyTheme()'s
# KColorScheme preconditions (manager + BreezeLight/BreezeDark schemes).
add_executable(tst_theme_applier tst_theme_applier.cpp)
add_test(NAME tst_theme_applier COMMAND tst_theme_applier)
target_link_libraries(tst_theme_applier PRIVATE
    Qt6::Test Qt6::Widgets KF6::ColorScheme)
set_tests_properties(tst_theme_applier PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
