// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

namespace Corbomite {

/// Per-leaf ephemeral state persisted inside `.obsidian/workspace.json`'s
/// `leaf.eState` slot. See `docs/obsidian-audit/domains/workspace.md §3`
/// and `domains/editor-markdown.md §8 invariant 2` for the on-wire shape.
///
/// The mode is encoded as Obsidian's compound `{mode, source}` — we do NOT
/// persist the local `NoteEditorWidget::ViewMode` integer. The mapping is
/// owned by `Corbomite::ViewModeSerializer` and only the raw string + bool
/// fields live on this struct, so this header has no dependency on
/// `NoteEditorWidget.h`.
///
/// scroll is visual-line float; precision ±0.5 for Source mode until Phase 4
/// (see Cluster E plan — `Qutepart::scrollContentsBy` sub-line precision is
/// deferred until the KSyntaxHighlighting / FoldCalculator rework).
///
/// **Unknown-key preservation.** Any keys not covered by the typed accessors
/// survive a `fromJson → toJson` round-trip via `extraKeys`. This mirrors
/// the Cluster B `WorkspaceState` idiom: workspace.json tolerates future
/// Obsidian versions and plugin data evolving around us.
struct EphemeralState {
    struct Cursor {
        int line = 0;
        int column = 0;

        friend bool operator==(const Cursor &a, const Cursor &b)
        {
            return a.line == b.line && a.column == b.column;
        }
    };

    float scroll = 0.0f;             // visual-line float (±0.5 precision — see header comment)
    Cursor cursor;
    QString modeRaw = QStringLiteral("source"); // {"source","preview"}
    bool sourceFlag = false;         // when modeRaw == "source": false = live-preview, true = source
    QVector<int> foldedHeadings;

    /// Unknown keys surviving a round-trip. Preserved at the top level of the
    /// `eState` object. See WorkspaceState for the sibling pattern.
    QJsonObject extraKeys;

    QJsonObject toJson() const;
    static EphemeralState fromJson(const QJsonObject &json);

    friend bool operator==(const EphemeralState &a, const EphemeralState &b)
    {
        return a.scroll == b.scroll && a.cursor == b.cursor
            && a.modeRaw == b.modeRaw && a.sourceFlag == b.sourceFlag
            && a.foldedHeadings == b.foldedHeadings
            && a.extraKeys == b.extraKeys;
    }
};

} // namespace Corbomite
