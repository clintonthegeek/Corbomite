// src/editor/MarkdownViewActions.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/ViewActions.h"

#include <QMetaObject>

class QWidget;

namespace Markoff {
struct EditorContext;
}

namespace Corbomite {

class MarkdownFileView;
class NoteEditorWidget;
class View;

/// Markdown's `ViewActions` provider.
///
/// Owns everything Appendix A of the plan assigns to "MarkdownViewActions":
/// the Format / Heading / Insert / Table menus (new top-level menus merged
/// in the slot just after Edit (`append="view"`, §D5), the View ▸ Editor Mode
/// radio group + `editor_toggle_mode` + the three fold stubs (merged into the
/// pre-existing, always-present View menu), and `insert_template`. Moved
/// verbatim out of `MainWindow::setupActions()` / `corbomiteui.rc.in` —
/// the connect() bodies are unchanged, just re-targeted at the bound
/// `MarkdownFileView` instead of `MainWindow`'s `activeEditor()`/
/// `activeMarkdownView()` accessors.
///
/// `edit_find`/`edit_replace`/`edit_find_next`/`edit_find_prev` are NOT
/// here — those are routed universally and stay in `MainWindow`'s collection.
///
/// Also owns the clipboard verbs (Cut/Copy/Paste/Copy as
/// Markdown|Plain|HTML|RTF/Paste as Plain Text), ported here from
/// `MainWindow` — they were built before Format/Heading/etc. moved out of
/// `MainWindow`'s flat KActionCollection, onto the exact same "leaf-agnostic
/// MarkdownFileView virtual, one polymorphic call" shape this provider already
/// uses. They merge into the pre-existing
/// universal "edit" menu container (same pattern the View menu's Editor
/// Mode group already uses), not a new top-level menu.
class MarkdownViewActions : public ViewActions
{
    Q_OBJECT
public:
    /// `dialogParent` parents the two dialog-wrapped Insert actions
    /// (Insert Table.../Insert Callout...) — a provider owns no window of
    /// its own to parent transient dialogs to.
    explicit MarkdownViewActions(QWidget *dialogParent, QObject *parent = nullptr);

    QString viewType() const override;
    void bind(View *view) override;
    void unbind() override;
    void refresh() override;
    QList<QAction *> toolBarActions() const override;

Q_SIGNALS:
    /// `insert_template`'s trigger. Template expansion needs the host's
    /// `TemplateService` (folder config, `{{variable}}` expansion) — that
    /// stays MainWindow-owned; the provider only owns the QAction/menu
    /// entry and forwards the trigger. MainWindow connects this to its
    /// existing `insertTemplate()` slot.
    void insertTemplateRequested();

private Q_SLOTS:
    void onEditorContextChanged(const Markoff::EditorContext &ctx);
    void syncEditorModeCheckState(int viewMode);

private:
    void setupActions();
    NoteEditorWidget *editor() const;

    QWidget *m_dialogParent;
    MarkdownFileView *m_boundView = nullptr;

    QMetaObject::Connection m_editorContextConnection;

    /// O7.T1 — the cursor's current block context, cached from
    /// EditorContextChanged. heading_increase/decrease need the CURRENT
    /// heading level to compute the next one, and EditorContext was
    /// previously consumed and discarded, so the only way to get it was to
    /// ask the editor again — which is not exposed. `hasCursorContext` keeps
    /// the Tier-B honest before any context has ever been reported.
    int m_cursorHeadingLevel = 0;
    bool m_hasCursorContext = false;
    QMetaObject::Connection m_viewModeConnection;
};

} // namespace Corbomite
