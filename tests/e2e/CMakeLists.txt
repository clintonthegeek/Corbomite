cmake_minimum_required(VERSION 3.19)
project(Corbomite_E2ETests LANGUAGES CXX)
enable_testing()
find_package(Qt6 REQUIRED COMPONENTS Test Widgets)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

add_executable(tst_e2e_gui tst_e2e_gui.cpp)
add_test(NAME tst_e2e_gui COMMAND tst_e2e_gui)
target_link_libraries(tst_e2e_gui PRIVATE
    Qt6::Test
    Qt6::Widgets
    CorbomiteApp
)

# Pass source dir so tests can find testvaults/
target_compile_definitions(tst_e2e_gui PRIVATE
    CORBOMITE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_executable(tst_vault_switch tst_vault_switch.cpp)
add_test(NAME tst_vault_switch COMMAND tst_vault_switch)
target_link_libraries(tst_vault_switch PRIVATE
    Qt6::Test
    Qt6::Widgets
    CorbomiteApp
)

add_executable(tst_completion_popup tst_completion_popup.cpp)
add_test(NAME tst_completion_popup COMMAND tst_completion_popup)
target_link_libraries(tst_completion_popup PRIVATE
    Qt6::Test
    Qt6::Widgets
    CorbomiteApp
)

add_executable(tst_panels_populated tst_panels_populated.cpp)
add_test(NAME tst_panels_populated COMMAND tst_panels_populated)
target_link_libraries(tst_panels_populated PRIVATE
    Qt6::Test
    Qt6::Widgets
    CorbomiteApp
)

# E2E tests need a real display — do NOT set QT_QPA_PLATFORM=offscreen
# They will fail in headless CI but pass on a developer machine with Wayland/X11
