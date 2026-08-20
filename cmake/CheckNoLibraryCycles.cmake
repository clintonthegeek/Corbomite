# SPDX-License-Identifier: GPL-3.0-or-later
#
# Cluster P Phase P2 — tst_no_library_cycles. `corbomite-core` and
# `corbomite-storage` used to form a target_link_libraries dependency cycle
# (C3 in docs/audit-2026-08-20-shared-libraries-refactor.md's corrections),
# which CMake only tolerates when every target in the cycle is STATIC —
# introducing SHARED anywhere in that strongly connected component is a
# configure-time error. P2 cut the cycle by moving MarkoffAdapters (the only
# real symbol dependency core had on storage) into storage itself. Nothing
# else in the build stops a future session from reintroducing the edge (e.g.
# a stray `#include "corbomite/storage/..."` in libs/core or a re-added
# target_link_libraries line) — this script is the guard, run as a ctest via
# `cmake -P`. It asks CMake's own --graphviz export for the dependency
# graph rather than grepping source, so it catches the edge regardless of
# how it's reintroduced.
#
# Expected variables (passed via -D on the cmake -P invocation):
#   BINARY_DIR — the configured build tree to inspect (must already exist).

if(NOT DEFINED BINARY_DIR)
    message(FATAL_ERROR "CheckNoLibraryCycles.cmake requires -DBINARY_DIR=<build dir>")
endif()

set(_dot_file "${BINARY_DIR}/cycle_check.dot")

execute_process(
    COMMAND ${CMAKE_COMMAND} --graphviz=${_dot_file} .
    WORKING_DIRECTORY ${BINARY_DIR}
    RESULT_VARIABLE _graphviz_result
    OUTPUT_QUIET
    ERROR_VARIABLE _graphviz_error
)
if(NOT _graphviz_result EQUAL 0)
    message(FATAL_ERROR "cmake --graphviz failed: ${_graphviz_error}")
endif()

if(NOT EXISTS "${_dot_file}")
    message(FATAL_ERROR "graphviz output not found at ${_dot_file}")
endif()

file(READ "${_dot_file}" _dot_contents)

# Sanity check the mechanism itself: the retained storage->core edge must
# still be visible, or this script isn't actually reading the real graph.
if(NOT _dot_contents MATCHES "// corbomite-storage -> corbomite-core")
    message(FATAL_ERROR
        "Expected edge 'corbomite-storage -> corbomite-core' not found in "
        "${_dot_file} — the graphviz mechanism this test relies on may have "
        "changed shape; investigate before trusting a green result.")
endif()

# The actual guard: core must not depend on storage.
if(_dot_contents MATCHES "// corbomite-core -> corbomite-storage")
    message(FATAL_ERROR
        "corbomite-core -> corbomite-storage dependency edge has "
        "reappeared. This reintroduces the P2-fixed dependency cycle "
        "(see C3 in the audit corrections) and will block any future "
        "STATIC->SHARED flip of either library. Remove the edge — check "
        "for a target_link_libraries(corbomite-core ... Corbomite::Storage) "
        "line or a #include \"corbomite/storage/...\" in libs/core.")
endif()

message(STATUS "tst_no_library_cycles: no corbomite-core -> corbomite-storage edge found")
