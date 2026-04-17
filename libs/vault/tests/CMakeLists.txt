find_package(Qt6 6.8 REQUIRED COMPONENTS Test)

add_executable(tst_tabstractfile tst_tabstractfile.cpp)
target_link_libraries(tst_tabstractfile PRIVATE Qt6::Test Corbomite::Vault)
add_test(NAME tst_tabstractfile COMMAND tst_tabstractfile)
set_tests_properties(tst_tabstractfile PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_tfile tst_tfile.cpp)
target_link_libraries(tst_tfile PRIVATE Qt6::Test Corbomite::Vault)
add_test(NAME tst_tfile COMMAND tst_tfile)
set_tests_properties(tst_tfile PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_tfolder tst_tfolder.cpp)
target_link_libraries(tst_tfolder PRIVATE Qt6::Test Corbomite::Vault)
add_test(NAME tst_tfolder COMMAND tst_tfolder)
set_tests_properties(tst_tfolder PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_vault_skeleton tst_vault_skeleton.cpp)
target_link_libraries(tst_vault_skeleton
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_vault_skeleton COMMAND tst_vault_skeleton)
set_tests_properties(tst_vault_skeleton PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
