// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace Corbomite {

/// Obsidian's canonical property types for frontmatter values.
/// Matches docs/obsidian-audit/domains/metadata.md §2 PropertyInfo.widget.
enum class PropertyType {
    Text,      // default; also for null / unrecognised
    Number,    // int or double
    Checkbox,  // bool
    Date,      // YYYY-MM-DD string
    DateTime,  // ISO 8601 string
    List       // YAML sequence (typed as List — each element separately editable as Text)
};

}  // namespace Corbomite
