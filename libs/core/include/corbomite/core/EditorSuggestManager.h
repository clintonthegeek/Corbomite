// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <optional>

#include <QObject>
#include <QVector>

#include "corbomite/core/EditorSuggest.h"

namespace Corbomite {

// Insertion-order-first-non-null-onTrigger-wins dispatcher. Per
// domains/editor.md §3 — no priority system; the order matters.
//
// The manager does NOT own the registered suggesters' lifetimes
// (suggesters are Components owned by their caller, typically a
// PluginInstance or the app). Caller must unregister before the
// suggester is destroyed.
class EditorSuggestManager : public QObject {
    Q_OBJECT

public:
    explicit EditorSuggestManager(QObject *parent = nullptr);

    // Append `suggester` to the dispatch list. Insertion order is the
    // entire coordination mechanism — register built-ins first so they
    // shadow plugin overrides of the same trigger.
    void registerSuggest(EditorSuggest *suggester);
    void unregisterSuggest(EditorSuggest *suggester);

    // Result: which suggester (if any) wants to activate, with its trigger
    // info. Iterates registered suggesters in insertion order, returning the
    // first whose onTrigger() returns a value.
    struct DispatchResult {
        EditorSuggest *suggester = nullptr;
        EditorSuggestTriggerInfo info;
    };
    std::optional<DispatchResult> dispatch(int cursorPos,
                                             const QString &lineText,
                                             NoteDocument *file) const;

    int suggesterCount() const { return m_suggesters.size(); }

private:
    QVector<EditorSuggest *> m_suggesters;
};

} // namespace Corbomite
