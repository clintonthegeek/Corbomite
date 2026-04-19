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

add_executable(tst_vault_adapter tst_vault_adapter.cpp)
target_link_libraries(tst_vault_adapter
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_vault_adapter COMMAND tst_vault_adapter)
set_tests_properties(tst_vault_adapter PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_vault_watcher tst_vault_watcher.cpp)
target_link_libraries(tst_vault_watcher
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_vault_watcher COMMAND tst_vault_watcher)
set_tests_properties(tst_vault_watcher PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 30)

add_executable(tst_vault_echo_suppression tst_vault_echo_suppression.cpp)
target_link_libraries(tst_vault_echo_suppression
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_vault_echo_suppression COMMAND tst_vault_echo_suppression)
set_tests_properties(tst_vault_echo_suppression PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_vault_read tst_vault_read.cpp)
target_link_libraries(tst_vault_read
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_vault_read COMMAND tst_vault_read)
set_tests_properties(tst_vault_read PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_vault_cached_read tst_vault_cached_read.cpp)
target_link_libraries(tst_vault_cached_read
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_vault_cached_read COMMAND tst_vault_cached_read)
set_tests_properties(tst_vault_cached_read PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 30)

add_executable(tst_vault_modify tst_vault_modify.cpp)
target_link_libraries(tst_vault_modify
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_vault_modify COMMAND tst_vault_modify)
set_tests_properties(tst_vault_modify PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_vault_process tst_vault_process.cpp)
target_link_libraries(tst_vault_process
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_vault_process COMMAND tst_vault_process)
set_tests_properties(tst_vault_process PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_vault_create tst_vault_create.cpp)
target_link_libraries(tst_vault_create
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_vault_create COMMAND tst_vault_create)
set_tests_properties(tst_vault_create PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_vault_rename_remove tst_vault_rename_remove.cpp)
target_link_libraries(tst_vault_rename_remove
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_vault_rename_remove COMMAND tst_vault_rename_remove)
set_tests_properties(tst_vault_rename_remove PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_vault_trash tst_vault_trash.cpp)
target_link_libraries(tst_vault_trash
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_vault_trash COMMAND tst_vault_trash)
set_tests_properties(tst_vault_trash PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_vault_configdir tst_vault_configdir.cpp)
target_link_libraries(tst_vault_configdir
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_vault_configdir COMMAND tst_vault_configdir)
set_tests_properties(tst_vault_configdir PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_vault_config_json tst_vault_config_json.cpp)
target_link_libraries(tst_vault_config_json
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_vault_config_json COMMAND tst_vault_config_json)
set_tests_properties(tst_vault_config_json PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_file_manager tst_file_manager.cpp)
target_link_libraries(tst_file_manager
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_file_manager COMMAND tst_file_manager)
set_tests_properties(tst_file_manager PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_file_manager_frontmatter tst_file_manager_frontmatter.cpp)
target_link_libraries(tst_file_manager_frontmatter
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_file_manager_frontmatter COMMAND tst_file_manager_frontmatter)
set_tests_properties(tst_file_manager_frontmatter PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_file_manager_rename tst_file_manager_rename.cpp)
target_link_libraries(tst_file_manager_rename
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_file_manager_rename COMMAND tst_file_manager_rename)
set_tests_properties(tst_file_manager_rename PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 30)

add_executable(tst_file_manager_newfile tst_file_manager_newfile.cpp)
target_link_libraries(tst_file_manager_newfile
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_file_manager_newfile COMMAND tst_file_manager_newfile)
set_tests_properties(tst_file_manager_newfile PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_file_manager_misc tst_file_manager_misc.cpp)
target_link_libraries(tst_file_manager_misc
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_file_manager_misc COMMAND tst_file_manager_misc)
set_tests_properties(tst_file_manager_misc PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_vault_proxy tst_vault_proxy.cpp)
target_link_libraries(tst_vault_proxy
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_vault_proxy COMMAND tst_vault_proxy)
set_tests_properties(tst_vault_proxy PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_file_manager_proxy tst_file_manager_proxy.cpp)
target_link_libraries(tst_file_manager_proxy
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
add_test(NAME tst_file_manager_proxy COMMAND tst_file_manager_proxy)
set_tests_properties(tst_file_manager_proxy PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_plugin_data_store tst_plugin_data_store.cpp)
target_link_libraries(tst_plugin_data_store
    PRIVATE Qt6::Test Corbomite::Vault)
add_test(NAME tst_plugin_data_store COMMAND tst_plugin_data_store)
set_tests_properties(tst_plugin_data_store PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_file_name_validator tst_file_name_validator.cpp)
target_link_libraries(tst_file_name_validator
    PRIVATE Qt6::Test Corbomite::Vault Corbomite::Storage)
# The validator header lives under libs/vault/src/ (private), so the test
# needs explicit include access.
target_include_directories(tst_file_name_validator
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
add_test(NAME tst_file_name_validator COMMAND tst_file_name_validator)
set_tests_properties(tst_file_name_validator PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_rename_dialog tst_rename_dialog.cpp)
target_link_libraries(tst_rename_dialog
    PRIVATE Qt6::Test Qt6::Widgets Corbomite::Vault Corbomite::Storage)
target_include_directories(tst_rename_dialog
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
add_test(NAME tst_rename_dialog COMMAND tst_rename_dialog)
set_tests_properties(tst_rename_dialog PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_move_file_dialog tst_move_file_dialog.cpp)
target_link_libraries(tst_move_file_dialog
    PRIVATE Qt6::Test Qt6::Widgets Corbomite::Vault Corbomite::Storage)
target_include_directories(tst_move_file_dialog
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
add_test(NAME tst_move_file_dialog COMMAND tst_move_file_dialog)
set_tests_properties(tst_move_file_dialog PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_delete_confirm_dialog tst_delete_confirm_dialog.cpp)
target_link_libraries(tst_delete_confirm_dialog
    PRIVATE Qt6::Test Qt6::Widgets KF6::ConfigCore
            Corbomite::Vault Corbomite::Storage)
target_include_directories(tst_delete_confirm_dialog
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
add_test(NAME tst_delete_confirm_dialog COMMAND tst_delete_confirm_dialog)
set_tests_properties(tst_delete_confirm_dialog PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
