cmake_minimum_required(VERSION 3.19)
project(Corbomite_AppTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test)
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
