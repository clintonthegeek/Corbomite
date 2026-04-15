// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "NoteEditorWidget.h"

#include <QString>

#include <optional>

namespace Corbomite {

/// Obsidian's on-wire compound encoding for editor mode — see
/// `docs/obsidian-audit/domains/editor-markdown.md §8 invariant 2`.
///
/// The mapping between `NoteEditorWidget::ViewMode` and this compound is:
///
///   Source       ↔ {mode: "source",  source: true}
///   LivePreview  ↔ {mode: "source",  source: false}
///   Reading      ↔ {mode: "preview"}  (source is don't-care)
///
/// `source` defaults to `false` when absent/null on `{mode: "source"}`; it is
/// ignored on `{mode: "preview"}`.
struct ViewModeCompound {
    QString mode;
    bool source = false;

    friend bool operator==(const ViewModeCompound &a, const ViewModeCompound &b)
    {
        return a.mode == b.mode && a.source == b.source;
    }
};

class ViewModeSerializer {
public:
    static ViewModeCompound toCompound(NoteEditorWidget::ViewMode m);
    static NoteEditorWidget::ViewMode fromCompound(const ViewModeCompound &c);

    /// Accept the raw `mode` string + optional `source` bool (absent/null
    /// permitted). Unknown `mode` strings log a warning and fall back to
    /// LivePreview — that keeps forward-compat with future Obsidian values.
    static NoteEditorWidget::ViewMode fromCompound(const QString &mode,
                                                   const std::optional<bool> &source);
};

} // namespace Corbomite
