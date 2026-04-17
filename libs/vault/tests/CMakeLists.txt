find_package(Qt6 6.8 REQUIRED COMPONENTS Test)

# Tests registered by subsequent tasks via:
#   add_executable(<name> <name>.cpp)
#   target_link_libraries(<name> PRIVATE Qt6::Test Corbomite::Vault)
#   add_test(NAME <name> COMMAND <name>)
#   set_tests_properties(<name> PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
