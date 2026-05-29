cmake_minimum_required(VERSION 3.19)
project(Corbomite_BasesTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test Widgets)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_executable(tst_bases_value_null tst_value_null.cpp)
add_test(NAME tst_bases_value_null COMMAND tst_bases_value_null)
target_link_libraries(tst_bases_value_null PRIVATE Qt6::Test Corbomite::Bases)

add_executable(tst_bases_value_primitive tst_value_primitive.cpp)
add_test(NAME tst_bases_value_primitive COMMAND tst_bases_value_primitive)
target_link_libraries(tst_bases_value_primitive PRIVATE Qt6::Test Corbomite::Bases)

add_executable(tst_bases_value_list tst_value_list.cpp)
add_test(NAME tst_bases_value_list COMMAND tst_bases_value_list)
target_link_libraries(tst_bases_value_list PRIVATE Qt6::Test Corbomite::Bases)

add_executable(tst_bases_value_date tst_value_date.cpp)
add_test(NAME tst_bases_value_date COMMAND tst_bases_value_date)
target_link_libraries(tst_bases_value_date PRIVATE Qt6::Test Corbomite::Bases)

add_executable(tst_bases_value_duration tst_value_duration.cpp)
add_test(NAME tst_bases_value_duration COMMAND tst_bases_value_duration)
target_link_libraries(tst_bases_value_duration PRIVATE Qt6::Test Corbomite::Bases)

add_executable(tst_bases_value_string_subclasses tst_value_string_subclasses.cpp)
add_test(NAME tst_bases_value_string_subclasses COMMAND tst_bases_value_string_subclasses)
target_link_libraries(tst_bases_value_string_subclasses PRIVATE Qt6::Test Corbomite::Bases)

add_executable(tst_bases_value_object_regex tst_value_object_regex.cpp)
add_test(NAME tst_bases_value_object_regex COMMAND tst_bases_value_object_regex)
target_link_libraries(tst_bases_value_object_regex PRIVATE Qt6::Test Corbomite::Bases)

add_executable(tst_bases_lexer tst_lexer.cpp)
add_test(NAME tst_bases_lexer COMMAND tst_bases_lexer)
target_link_libraries(tst_bases_lexer PRIVATE Qt6::Test Corbomite::Bases)

add_executable(tst_bases_parser tst_parser.cpp)
add_test(NAME tst_bases_parser COMMAND tst_bases_parser)
target_link_libraries(tst_bases_parser PRIVATE Qt6::Test Corbomite::Bases)

add_executable(tst_bases_evaluator tst_evaluator.cpp)
add_test(NAME tst_bases_evaluator COMMAND tst_bases_evaluator)
target_link_libraries(tst_bases_evaluator PRIVATE Qt6::Test Corbomite::Bases)

add_executable(tst_bases_builtins tst_builtins.cpp)
add_test(NAME tst_bases_builtins COMMAND tst_bases_builtins)
target_link_libraries(tst_bases_builtins PRIVATE Qt6::Test Corbomite::Bases)

add_executable(tst_bases_yaml_schema tst_yaml_schema.cpp)
add_test(NAME tst_bases_yaml_schema COMMAND tst_bases_yaml_schema)
target_link_libraries(tst_bases_yaml_schema PRIVATE Qt6::Test Corbomite::Bases)

add_executable(tst_bases_entry tst_basesentry.cpp)
add_test(NAME tst_bases_entry COMMAND tst_bases_entry)
target_link_libraries(tst_bases_entry PRIVATE Qt6::Test Corbomite::Bases)

add_executable(tst_bases_sortcycle tst_sortcycle.cpp)
add_test(NAME tst_bases_sortcycle COMMAND tst_bases_sortcycle)
target_link_libraries(tst_bases_sortcycle PRIVATE Qt6::Test Corbomite::Bases)

add_executable(tst_bases_tree_model tst_bases_tree_model.cpp)
add_test(NAME tst_bases_tree_model COMMAND tst_bases_tree_model)
target_link_libraries(tst_bases_tree_model PRIVATE Qt6::Test Qt6::Widgets Corbomite::Bases Corbomite::Vault Corbomite::Storage)

add_executable(tst_bases_cell_delegate tst_bases_cell_delegate.cpp)
add_test(NAME tst_bases_cell_delegate COMMAND tst_bases_cell_delegate)
target_link_libraries(tst_bases_cell_delegate PRIVATE Qt6::Test Qt6::Widgets Corbomite::Bases)

add_executable(tst_bases_view_config_ops tst_view_config_ops.cpp)
add_test(NAME tst_bases_view_config_ops COMMAND tst_bases_view_config_ops)
target_link_libraries(tst_bases_view_config_ops PRIVATE Qt6::Test Corbomite::Bases)

add_executable(tst_bases_view_wiring tst_bases_view_wiring.cpp)
add_test(NAME tst_bases_view_wiring COMMAND tst_bases_view_wiring)
target_link_libraries(tst_bases_view_wiring PRIVATE Qt6::Test Qt6::Widgets Corbomite::Bases Corbomite::Vault Corbomite::Storage)

add_executable(tst_bases_cell_hittest tst_bases_cell_hittest.cpp)
add_test(NAME tst_bases_cell_hittest COMMAND tst_bases_cell_hittest)
target_link_libraries(tst_bases_cell_hittest PRIVATE Qt6::Test Qt6::Gui Corbomite::Bases)

add_executable(tst_table_exporter tst_table_exporter.cpp)
add_test(NAME tst_table_exporter COMMAND tst_table_exporter)
target_link_libraries(tst_table_exporter PRIVATE Qt6::Test Corbomite::Bases)

add_executable(tst_new_item_seed tst_new_item_seed.cpp)
add_test(NAME tst_new_item_seed COMMAND tst_new_item_seed)
target_link_libraries(tst_new_item_seed PRIVATE Qt6::Test Corbomite::Bases)

add_executable(tst_bases_commands tst_bases_commands.cpp)
add_test(NAME tst_bases_commands COMMAND tst_bases_commands)
target_link_libraries(tst_bases_commands PRIVATE
    Qt6::Test Corbomite::Bases Corbomite::Vault Corbomite::Storage)
set_tests_properties(tst_bases_commands PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_bases_summary tst_bases_summary.cpp)
add_test(NAME tst_bases_summary COMMAND tst_bases_summary)
target_link_libraries(tst_bases_summary PRIVATE Qt6::Test Corbomite::Bases)

add_executable(tst_formula_ops tst_formula_ops.cpp)
add_test(NAME tst_formula_ops COMMAND tst_formula_ops)
target_link_libraries(tst_formula_ops PRIVATE Qt6::Test Corbomite::Bases)

add_executable(tst_formula_candidates tst_formula_candidates.cpp)
add_test(NAME tst_formula_candidates COMMAND tst_formula_candidates)
target_link_libraries(tst_formula_candidates PRIVATE Qt6::Test Corbomite::Bases)

add_executable(tst_formula_input tst_formula_input.cpp)
add_test(NAME tst_formula_input COMMAND tst_formula_input)
target_link_libraries(tst_formula_input PRIVATE Qt6::Test Qt6::Widgets Corbomite::Bases)
set_tests_properties(tst_formula_input PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_formula_edit_dialog tst_formula_edit_dialog.cpp)
add_test(NAME tst_formula_edit_dialog COMMAND tst_formula_edit_dialog)
target_link_libraries(tst_formula_edit_dialog PRIVATE Qt6::Test Qt6::Widgets Corbomite::Bases)
set_tests_properties(tst_formula_edit_dialog PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_properties_menu_panel tst_properties_menu_panel.cpp)
add_test(NAME tst_properties_menu_panel COMMAND tst_properties_menu_panel)
target_link_libraries(tst_properties_menu_panel PRIVATE Qt6::Test Qt6::Widgets Corbomite::Bases)
set_tests_properties(tst_properties_menu_panel PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_filter_spec tst_filter_spec.cpp)
add_test(NAME tst_filter_spec COMMAND tst_filter_spec)
target_link_libraries(tst_filter_spec PRIVATE Qt6::Test Corbomite::Bases)

add_executable(tst_filter_builder_widget tst_filter_builder_widget.cpp)
add_test(NAME tst_filter_builder_widget COMMAND tst_filter_builder_widget)
target_link_libraries(tst_filter_builder_widget PRIVATE Qt6::Test Qt6::Widgets Corbomite::Bases)
set_tests_properties(tst_filter_builder_widget PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_filter_builder_dialog tst_filter_builder_dialog.cpp)
add_test(NAME tst_filter_builder_dialog COMMAND tst_filter_builder_dialog)
target_link_libraries(tst_filter_builder_dialog PRIVATE Qt6::Test Qt6::Widgets Corbomite::Bases)
set_tests_properties(tst_filter_builder_dialog PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
