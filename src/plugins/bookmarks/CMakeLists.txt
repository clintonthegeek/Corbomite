# Phase-in: the plugin target lands in Task 2.1. Until then, only build tests.
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
