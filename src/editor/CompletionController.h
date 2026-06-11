// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPointer>
#include <QObject>
#include <QRect>

#include <markoff/core/MarkdownView.h>

class QStandardItemModel;

namespace Corbomite {

class CompletionPopup;
class EditorSuggestManager;
class NoteDocument;

/// Leaf-agnostic completion driver (spec
/// docs/superpowers/specs/2026-06-11-completion-revival-design.md §6).
/// Owns the popup + trigger session; reads the active leaf ONLY through
/// the Markoff::MarkdownView base (cursorPosition / caretRect /
/// hasEditing) and mutates ONLY the shared MarkoffDocument
/// (applyBlockEdit), so it works identically for every leaf.
///
/// Reactive model: refresh() recomputes the entire trigger state from the
/// current snapshot on every (coalesced) document/cursor change — no
/// stored trigger position, no per-key state machine.
class CompletionController : public QObject {
    Q_OBJECT
public:
    explicit CompletionController(QObject *parent = nullptr);
    ~CompletionController() override;

    void setManager(EditorSuggestManager *manager);
    void setLeaf(Markoff::MarkdownView *leaf);      // every mode switch; dismisses
    void setNoteDocument(NoteDocument *doc);        // every document swap; dismisses

    bool isActive() const;                          // popup visible
    CompletionPopup *popup() const { return m_popup; }   // tests

public Q_SLOTS:
    void dismiss();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;  // scoped app filter

private:
    void scheduleRefresh();
    void refresh();
    void ensurePopup();
    void positionPopup(const QRect &caretRect);
    void accept(const QString &display, const QString &insertText);

    EditorSuggestManager *m_manager = nullptr;
    Markoff::MarkdownView *m_leaf = nullptr;
    NoteDocument *m_doc = nullptr;                  // nulled via destroyed()
    CompletionPopup *m_popup = nullptr;
    QStandardItemModel *m_model = nullptr;
    bool m_refreshPending = false;
    QMetaObject::Connection m_docChangedCon;
    QMetaObject::Connection m_docDestroyedCon;
    QMetaObject::Connection m_leafCursorCon;
};

} // namespace Corbomite
