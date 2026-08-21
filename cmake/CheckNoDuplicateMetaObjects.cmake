# SPDX-License-Identifier: GPL-3.0-or-later
#
# Cluster P Phase P4.T3 — tst_no_duplicate_metaobjects. The structural
# guard for C1/C2 (docs/audit-2026-08-20-shared-libraries-refactor.md's
# corrections): walks every built module (the Corbomite executable, every
# bin/lib*.so, and every plugin .so under lib/plugins/corbomite/) and
# asserts no `*staticMetaObject` symbol is *defined* (not just referenced)
# in more than one module.
#
# A symbol defined in two modules means two independently-linked copies of
# the same Qt class exist in the process — Qt6's QMetaObject::cast() is a
# pointer-identity check (`m == this` walking the metaobject chain, not a
# string comparison), so a QObject constructed against one copy silently
# fails qobject_cast() against the other. That is exactly the bug this
# cluster exists to fix (see tst_plugin_type_identity for the behavioural
# proof) and P3's STATIC->SHARED flips fixed it for every Corbomite library
# a plugin can link — but nothing stops a *future* session from
# reintroducing it by, e.g., compiling the same .cpp into two independent
# targets instead of linking a shared library (exactly what P4.T3's first
# real run caught: src/sidebar/PropertyEditorWidget.cpp + PropertyRow.cpp
# were being compiled once into CorbomiteApp and a second time directly
# into corbomite-properties.so — fixed by linking the plugin against
# CorbomiteApp instead of recompiling; see that commit for detail).
#
# Allowlist: stayed empty through Phase P6 (cross-repo: markoff-family,
# graffodil, 2026-08-21) — the plan's P6.T4 asked to delete any entries
# added here for expected markoff-family/graffodil duplication as those
# libraries went SHARED, as proof the phase worked. None were ever
# needed: before P6, those libraries were STATIC and baked directly into
# whichever module linked them, so this scan never even saw them as
# separate modules; P6 made every one of them (markoff_core/canvas/
# source/styled, markoff-parser, ts-markdown-parser, collabtext,
# graffodil-core/batch/circular/force/spatial/sugiyama) SHARED and
# installed alongside Corbomite's own libraries, so this check now scans
# their .so's directly for the first time — and still finds zero
# duplicated staticMetaObject symbols. If a future library ends up
# statically linking one of these instead of using the shared target,
# that's exactly the class of bug this check exists to catch; add an
# allowlist entry here only if the duplication is genuinely expected,
# by symbol substring with a comment — NOT to silently weaken the check.
set(_allowlist_substrings
    # "9Markoff5Canvas"   # example: add a real mangled-name substring here
                            # with a comment explaining why, when P6 or any
                            # other future work introduces an expected one.
)

if(NOT DEFINED BINARY_DIR)
    message(FATAL_ERROR "CheckNoDuplicateMetaObjects.cmake requires -DBINARY_DIR=<build dir>")
endif()

find_program(NM_EXECUTABLE nm)
if(NOT NM_EXECUTABLE)
    message(FATAL_ERROR "tst_no_duplicate_metaobjects requires 'nm' (binutils) on PATH")
endif()

file(GLOB _shared_libs "${BINARY_DIR}/bin/lib*.so")
file(GLOB _plugin_libs "${BINARY_DIR}/lib/plugins/corbomite/*.so")
set(_exe "${BINARY_DIR}/bin/Corbomite")
set(_modules "${_exe}" ${_shared_libs} ${_plugin_libs})

list(LENGTH _modules _module_count)
if(_module_count LESS 3)
    message(FATAL_ERROR
        "Only found ${_module_count} module(s) to scan under ${BINARY_DIR} "
        "(expected the exe + several .so's) — build the full dev preset "
        "before running this test.")
endif()

# symbol name -> semicolon-joined list of defining module basenames
set(_definers_KEYS "")

foreach(_module ${_modules})
    if(NOT EXISTS "${_module}")
        continue()
    endif()
    execute_process(
        COMMAND ${NM_EXECUTABLE} -D --defined-only "${_module}"
        OUTPUT_VARIABLE _nm_out
        ERROR_QUIET
        RESULT_VARIABLE _nm_result
    )
    if(NOT _nm_result EQUAL 0)
        continue() # not a valid ELF (shouldn't happen for our globs, but be defensive)
    endif()
    get_filename_component(_modname "${_module}" NAME)
    string(REPLACE "\n" ";" _lines "${_nm_out}")
    foreach(_line ${_lines})
        if(_line MATCHES "staticMetaObjectE$")
            string(REGEX MATCH "[A-Za-z0-9_]+staticMetaObjectE$" _sym "${_line}")
            if(_sym)
                string(MAKE_C_IDENTIFIER "${_sym}" _key)
                if(NOT DEFINED _definers_${_key})
                    list(APPEND _definers_KEYS "${_key}")
                    set(_definers_${_key} "")
                    set(_symname_${_key} "${_sym}")
                endif()
                list(APPEND _definers_${_key} "${_modname}")
            endif()
        endif()
    endforeach()
endforeach()

set(_violations "")
foreach(_key ${_definers_KEYS})
    list(LENGTH _definers_${_key} _n)
    if(_n GREATER 1)
        set(_sym "${_symname_${_key}}")
        set(_allowed FALSE)
        foreach(_substr ${_allowlist_substrings})
            if(_sym MATCHES "${_substr}")
                set(_allowed TRUE)
            endif()
        endforeach()
        if(NOT _allowed)
            string(REPLACE ";" ", " _modlist "${_definers_${_key}}")
            list(APPEND _violations "  ${_sym}  defined in: ${_modlist}")
        endif()
    endif()
endforeach()

if(_violations)
    string(REPLACE ";" "\n" _report "${_violations}")
    message(FATAL_ERROR
        "tst_no_duplicate_metaobjects: found staticMetaObject symbols "
        "defined in more than one module (each represents a live "
        "qobject_cast-across-the-boundary risk per C1/C2):\n${_report}\n\n"
        "Fix by making the owning library SHARED and linking it (not "
        "recompiling its sources) from every consumer, or add an explicit, "
        "commented allowlist entry in cmake/CheckNoDuplicateMetaObjects.cmake "
        "if the duplication is expected (e.g. pending Phase P6).")
endif()

message(STATUS "tst_no_duplicate_metaobjects: no unexpected duplicate staticMetaObject symbols")
