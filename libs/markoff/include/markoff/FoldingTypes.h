// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef MARKOFF_FOLDINGTYPES_H
#define MARKOFF_FOLDINGTYPES_H

#include <QList>
#include <QStringList>
#include <markoff-parser/Document.h>

namespace Markoff {

/// A fold region's identity. For headings, this is the hierarchy path
/// ["Intro", "Goals", "Non-goals"]. Later block types (lists, code
/// blocks) will use their own shape but reuse this type.
using FoldRegionKey = QStringList;

/// Compute the hierarchy path for each heading in document order.
/// Stripping rules: inline markdown removed (`**bold**` -> `bold`).
/// Duplicate siblings disambiguated with `#N` suffix starting at `#2`.
QList<FoldRegionKey> computeHeadingPaths(const QList<HeadingInfo> &headings);

/// Strip inline markdown delimiters from a heading's raw text. Public
/// so tests and host code can compute paths equivalently.
QString normalizeHeadingText(const QString &raw);

/// A unified fold region: either a heading or a fenced code block.
struct FoldableRegion {
    enum Type { Heading, CodeBlock };
    Type type = Heading;
    FoldRegionKey path;
    int sourceOffset = 0;
    int level = 0;       // 1..6 for Heading; 0 for CodeBlock

    // Heading-specific (valid only when type == Heading):
    HeadingInfo info;

    // Code-block-specific (valid only when type == CodeBlock):
    QString language;
    int lineCount = 0;
};

/// Compute `code:N` path segments by walking regions in document
/// order, resetting the ordinal whenever a heading boundary is crossed.
/// Called after `computeHeadingPaths` has populated heading paths in
/// the input list; code-block entries have their `path` initialized
/// to just the enclosing heading path (no "code:N" yet) and this
/// function appends the ordinal segment.
void assignCodeBlockOrdinals(QList<FoldableRegion> &regions);

} // namespace Markoff

#endif // MARKOFF_FOLDINGTYPES_H
