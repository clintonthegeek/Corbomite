find_package(Qt6 6.8 REQUIRED COMPONENTS Test)

add_executable(tst_tabstractfile tst_tabstractfile.cpp)
target_link_libraries(tst_tabstractfile PRIVATE Qt6::Test Corbomite::Vault)
add_test(NAME tst_tabstractfile COMMAND tst_tabstractfile)
set_tests_properties(tst_tabstractfile PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
