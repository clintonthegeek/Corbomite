// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace Corbomite::MergeFiles {

/// True when `content` opens with a YAML frontmatter block (`---` … `---`).
bool hasFrontmatter(const QString &content);

/// Return `content` with a leading YAML frontmatter block removed. Obsidian's
/// Note Composer strips the source note's frontmatter before merging (its
/// shipped behaviour, even when the target has none); this is that strip. A
/// content string with no well-formed leading block is returned unchanged.
QString stripFrontmatter(const QString &content);

/// Compose the merged body: `<target>\n\n<source>` for append, and
/// `<source>\n\n<target>` for prepend — Obsidian's `text + '\n\n' + srcBody`.
/// Empty operands skip the separator rather than producing a stray blank pair.
QString compose(const QString &targetContent,
                const QString &sourceBody,
                bool prepend);

}  // namespace Corbomite::MergeFiles
