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

class MarkdownView;
class NoteEditorWidget;
class View;

/// Cluster O Phase O3 (O3.T6) — markdown's `ViewActions` provider.
///
/// Owns everything Appendix A of the plan assigns to "MarkdownViewActions":
/// the Format / Heading / Insert / Table menus (new top-level menus merged
/// in at the `viewtype_merge` point, §D5), the View ▸ Editor Mode radio
/// group + `editor_toggle_mode` + the three fold stubs (merged into the
/// pre-existing, always-present View menu), and `insert_template`. Moved
/// verbatim out of `MainWindow::setupActions()` / `corbomiteui.rc.in` —
/// the connect() bodies are unchanged, just re-targeted at the bound
/// `MarkdownView` instead of `MainWindow`'s `activeEditor()`/
/// `activeMarkdownView()` accessors.
///
/// `edit_find`/`edit_replace`/`edit_find_next`/`edit_find_prev` are NOT
/// here — O1.T5 already routes those universally and they stay in
/// `MainWindow`'s collection per the plan's explicit instruction.
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
    MarkdownView *m_boundView = nullptr;

    QMetaObject::Connection m_editorContextConnection;
    QMetaObject::Connection m_viewModeConnection;
};

} // namespace Corbomite
