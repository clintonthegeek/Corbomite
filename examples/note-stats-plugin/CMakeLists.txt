# SPDX-License-Identifier: GPL-3.0-or-later
#
# note-stats reference plugin. Exercises the complete plugin ABI:
#   - VaultProxy (vault.read + vault.events)
#   - SearchProxy (metadata.read — tags + links)
#   - MetadataCacheReader (metadata.read — indexFinished signal)
#   - ViewRegistrar (ui.views — right sidebar mount via DockArea metadata)
#
# Subject of docs/plugin-development/TUTORIAL.md.

corbomite_add_plugin(note-stats
    METADATA_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/metadata.json.in"
    SOURCES
        NoteStatsPlugin.cpp
        NoteStatsView.cpp
    LINK_LIBRARIES
        Corbomite::Core
        Corbomite::Vault
        Corbomite::Storage
        KF6::I18n
        Qt6::Widgets
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
