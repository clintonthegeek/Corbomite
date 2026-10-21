cmake_minimum_required(VERSION 3.19)
project(Corbomite_CanvasTests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test Widgets)
find_package(KF6I18n REQUIRED)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# The card view: a canvas card is edited in Markoff's canvas leaf in
# embedded (card) mode, over the card's own session document.
add_executable(tst_canvas_card_editor tst_canvas_card_editor.cpp)
add_test(NAME tst_canvas_card_editor COMMAND tst_canvas_card_editor)
target_link_libraries(tst_canvas_card_editor PRIVATE
    Qt6::Test Qt6::Widgets Corbomite::Canvas Corbomite::Core)
set_tests_properties(tst_canvas_card_editor PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_canvasdocument tst_canvasdocument.cpp)
add_test(NAME tst_canvasdocument COMMAND tst_canvasdocument)
target_link_libraries(tst_canvasdocument PRIVATE Qt6::Test Corbomite::Canvas)
set_tests_properties(tst_canvasdocument PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# The canvas leaf's painter-state leak, seen from the host: a table after a
# heading must not be filled with the heading's chevron brush (Markoff
# v0.7.1's `f68a37c5`). Upstream's own test is in a leaf this build does not
# compile, so the host pins the behaviour.
add_executable(tst_canvas_table_brush tst_canvas_table_brush.cpp)
add_test(NAME tst_canvas_table_brush COMMAND tst_canvas_table_brush)
target_link_libraries(tst_canvas_table_brush PRIVATE
    Qt6::Test Qt6::Widgets Corbomite::Canvas Corbomite::Core)
set_tests_properties(tst_canvas_table_brush PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Missing-file placeholder (logic up to the live check).
add_executable(tst_canvas_missing_file tst_canvas_missing_file.cpp)
add_test(NAME tst_canvas_missing_file COMMAND tst_canvas_missing_file)
target_link_libraries(tst_canvas_missing_file PRIVATE Qt6::Test Qt6::Widgets Corbomite::Canvas Corbomite::Core)
set_tests_properties(tst_canvas_missing_file PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Canvas link rewrite on vault rename.
add_executable(tst_canvas_link_rewriter tst_canvas_link_rewriter.cpp)
add_test(NAME tst_canvas_link_rewriter COMMAND tst_canvas_link_rewriter)
target_link_libraries(tst_canvas_link_rewriter PRIVATE Qt6::Test Corbomite::Canvas Corbomite::Core)
set_tests_properties(tst_canvas_link_rewriter PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_canvasscene tst_canvasscene.cpp)
add_test(NAME tst_canvasscene COMMAND tst_canvasscene)
target_link_libraries(tst_canvasscene PRIVATE Qt6::Test Qt6::Widgets Corbomite::Canvas Corbomite::Core)
set_tests_properties(tst_canvasscene PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# `.canvas` disk contract: byte parity with Obsidian's printer plus the
# round-trip-loss rules (unknown node types, unresolved edges, defaults).
add_executable(tst_canvas_diskcontract tst_canvas_diskcontract.cpp)
add_test(NAME tst_canvas_diskcontract COMMAND tst_canvas_diskcontract)
target_link_libraries(tst_canvas_diskcontract PRIVATE Qt6::Test Qt6::Widgets Corbomite::Canvas)
set_tests_properties(tst_canvas_diskcontract PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Obsidian-exact bezier strategy.
add_executable(tst_canvasbezierstrategy tst_canvasbezierstrategy.cpp)
add_test(NAME tst_canvasbezierstrategy COMMAND tst_canvasbezierstrategy)
target_link_libraries(tst_canvasbezierstrategy PRIVATE Qt6::Test Corbomite::Canvas)
set_tests_properties(tst_canvasbezierstrategy PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Grid + object snap alignment strategy, plus
# the Shift axis-lock (same align() callback).
add_executable(tst_canvas_alignment tst_canvas_alignment.cpp)
add_test(NAME tst_canvas_alignment COMMAND tst_canvas_alignment)
target_link_libraries(tst_canvas_alignment PRIVATE Qt6::Test Qt6::Widgets Corbomite::Canvas)
set_tests_properties(tst_canvas_alignment PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# CanvasScene export pipeline (image + SVG).
add_executable(tst_canvas_export tst_canvas_export.cpp)
add_test(NAME tst_canvas_export COMMAND tst_canvas_export)
target_link_libraries(tst_canvas_export PRIVATE Qt6::Test Qt6::Widgets Corbomite::Canvas)
set_tests_properties(tst_canvas_export PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Canvas dark mode.
add_executable(tst_canvas_dark_mode tst_canvas_dark_mode.cpp)
add_test(NAME tst_canvas_dark_mode COMMAND tst_canvas_dark_mode)
target_link_libraries(tst_canvas_dark_mode PRIVATE Qt6::Test Qt6::Widgets Corbomite::Canvas)
set_tests_properties(tst_canvas_dark_mode PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Per-file Corbomite::Canvas read-only lock: every mutation gate,
# plus the negative case (pan/zoom/select keep working while locked).
add_executable(tst_canvas_readonly tst_canvas_readonly.cpp)
add_test(NAME tst_canvas_readonly COMMAND tst_canvas_readonly)
target_link_libraries(tst_canvas_readonly PRIVATE Qt6::Test Qt6::Widgets Corbomite::Canvas KF6::I18n)
set_tests_properties(tst_canvas_readonly PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# "Convert to File…": the type swap, the geometry
# preservation, and the single-undo-step guarantee.
add_executable(tst_canvas_convert_to_file tst_canvas_convert_to_file.cpp)
add_test(NAME tst_canvas_convert_to_file COMMAND tst_canvas_convert_to_file)
target_link_libraries(tst_canvas_convert_to_file PRIVATE
    Qt6::Test Qt6::Widgets Corbomite::Canvas KF6::I18n)
set_tests_properties(tst_canvas_convert_to_file PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# "Jump to Group…": the group candidate list and
# Obsidian's zoomToBbox scale formula.
add_executable(tst_canvas_jump_to_group tst_canvas_jump_to_group.cpp)
add_test(NAME tst_canvas_jump_to_group COMMAND tst_canvas_jump_to_group)
target_link_libraries(tst_canvas_jump_to_group PRIVATE
    Qt6::Test Qt6::Widgets Corbomite::Canvas KF6::I18n)
set_tests_properties(tst_canvas_jump_to_group PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
