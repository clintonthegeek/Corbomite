// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace Corbomite {

class DataAdapter;

/// One-shot filesystem case-sensitivity probe.
///
/// Writes a temporary file with an uppercase suffix into the given directory
/// and then tries to stat its lowercased twin. If the lowercased form is
/// found, the filesystem is case-insensitive; if not, it's case-sensitive.
///
/// Mirrors Obsidian's probe pattern (`docs/obsidian-audit/domains/vault.md §1`).
/// The result informs all path comparisons that might otherwise mis-resolve
/// basenames on case-insensitive filesystems (HFS+, APFS default, NTFS).
class CaseSensitivityProbe
{
public:
    /// Run the probe once. `probeDir` should be writable (typically the
    /// vault root or `.obsidian/`). The probe file is removed on success.
    ///
    /// On I/O failure, returns `true` (case-sensitive — conservative: we'd
    /// rather over-check equality than miss a collision).
    static bool isCaseSensitive(DataAdapter *fs, const QString &probeDir);
};

} // namespace Corbomite
