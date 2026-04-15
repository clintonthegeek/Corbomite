// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef CORBOMITE_CORE_FRONTMATTERWRITER_H
#define CORBOMITE_CORE_FRONTMATTERWRITER_H

#include <functional>

#include <QString>

#include <markoff-parser/YamlValue.h>

namespace Corbomite {

/// Atomic read-modify-write for a markdown file's YAML frontmatter.
///
/// Mirrors Obsidian's `FileManager.processFrontMatter` semantics:
///   1. Read the current file (UTF-8).
///   2. Parse its frontmatter into a mutable `Markoff::YamlValue` (empty map
///      if the file had no frontmatter block).
///   3. Pass the value to a caller-supplied mutator.
///   4. Stringify with the Obsidian option set (via markoff-parser's ryml
///      integration) and splice back into the file.
///   5. Write atomically via `QSaveFile` (write to temp + fsync + rename).
///
/// Compat limitation, documented and preserved: YAML comments inside
/// frontmatter are silently dropped on round-trip. This matches Obsidian's
/// behaviour — see `docs/obsidian-audit/domains/vault.md §1`.
class FrontMatterWriter
{
public:
    /// Atomically mutate `filePath`'s frontmatter. `mutator` is called once
    /// with a writable YamlValue (always a map; empty if none present).
    /// Returns true on success; on failure, leaves the original file intact.
    /// `errorOut`, if non-null, receives a diagnostic string on failure.
    static bool process(const QString &filePath,
                        const std::function<void(Markoff::YamlValue &)> &mutator,
                        QString *errorOut = nullptr);

    /// Read-only convenience: parse and return the frontmatter (empty map if
    /// the file has no frontmatter, or on parse error).
    static Markoff::YamlValue read(const QString &filePath,
                                   QString *errorOut = nullptr);

    /// Write-only convenience: replace the frontmatter block with `value`.
    /// If `value` is null / an empty map, an existing frontmatter block is
    /// stripped.
    static bool write(const QString &filePath,
                      const Markoff::YamlValue &value,
                      QString *errorOut = nullptr);
};

} // namespace Corbomite

#endif // CORBOMITE_CORE_FRONTMATTERWRITER_H
