// src/editor/MarkdownViewActions.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "MarkdownViewActions.h"

#include "MarkdownView.h"
#include "NoteEditorWidget.h"
#include "corbomite/core/View.h"
#include "dialogs/CalloutPickerDialog.h"
#include "dialogs/InsertTableDialog.h"

#include <markoff/core/EditorContext.h>
#include <markoff/core/MarkdownView.h>

#include <KActionCollection>
#include <KLocalizedString>

#include <QAction>
#include <QActionGroup>
#include <QIcon>
#include <QKeySequence>

namespace Corbomite {

namespace {
const QString kMarkdownGuiXml = QStringLiteral(
    "<!DOCTYPE gui SYSTEM \"kpartgui.dtd\">"
    "<gui name=\"markdown_view_actions\" version=\"2\">"
    "  <MenuBar>"
    "    <Menu name=\"edit\">"
    "      <Action name=\"edit_cut\"/>"
    "      <Action name=\"edit_copy\"/>"
    "      <Action name=\"edit_paste\"/>"
    "      <Menu name=\"edit_copy_as\">"
    "        <text>Copy as</text>"
    "        <Action name=\"edit_copy_as_markdown\"/>"
    "        <Action name=\"edit_copy_as_plain\"/>"
    "        <Action name=\"edit_copy_as_html\"/>"
    "        <Action name=\"edit_copy_as_rtf\"/>"
    "      </Menu>"
    "      <Action name=\"edit_paste_plain\"/>"
    "    </Menu>"
    "    <Menu name=\"view\">"
    "      <Action name=\"editor_toggle_mode\"/>"
    "      <Menu name=\"editor_mode\">"
    "        <text>Editor &amp;Mode</text>"
    "        <Action name=\"view_source_mode\"/>"
    "        <Action name=\"view_editing_mode\"/>"
    "        <Action name=\"view_reading_mode\"/>"
    "      </Menu>"
    "      <Separator/>"
    "      <Action name=\"fold_all\"/>"
    "      <Action name=\"unfold_all\"/>"
    "      <Action name=\"toggle_fold\"/>"
    "    </Menu>"
    "    <Menu name=\"format\" append=\"viewtype_merge\">"
    "      <text>F&amp;ormat</text>"
    "      <Action name=\"format_bold\"/>"
    "      <Action name=\"format_italic\"/>"
    "      <Action name=\"format_strikethrough\"/>"
    "      <Action name=\"format_inline_code\"/>"
    "      <Separator/>"
    "      <Action name=\"insert_link\"/>"
    "      <Action name=\"insert_wiki_link\"/>"
    "      <Action name=\"insert_image\"/>"
    "      <Separator/>"
    "      <Action name=\"insert_code_block\"/>"
    "      <Action name=\"insert_block_quote\"/>"
    "      <Action name=\"insert_horizontal_rule\"/>"
    "    </Menu>"
    "    <Menu name=\"heading\" append=\"viewtype_merge\">"
    "      <text>&amp;Heading</text>"
    "      <Action name=\"heading_1\"/>"
    "      <Action name=\"heading_2\"/>"
    "      <Action name=\"heading_3\"/>"
    "      <Action name=\"heading_4\"/>"
    "      <Action name=\"heading_5\"/>"
    "      <Action name=\"heading_6\"/>"
    "      <Separator/>"
    "      <Action name=\"heading_increase\"/>"
    "      <Action name=\"heading_decrease\"/>"
    "    </Menu>"
    "    <Menu name=\"insert\" append=\"viewtype_merge\">"
    "      <text>&amp;Insert</text>"
    "      <Action name=\"insert_table\"/>"
    "      <Action name=\"insert_callout\"/>"
    "      <Action name=\"toggle_checkbox\"/>"
    "    </Menu>"
    "    <Menu name=\"table\" append=\"viewtype_merge\">"
    "      <text>&amp;Table</text>"
    "      <Action name=\"table_row_above\"/>"
    "      <Action name=\"table_row_below\"/>"
    "      <Separator/>"
    "      <Action name=\"table_col_left\"/>"
    "      <Action name=\"table_col_right\"/>"
    "      <Separator/>"
    "      <Action name=\"table_delete_row\"/>"
    "      <Action name=\"table_delete_col\"/>"
    "    </Menu>"
    "  </MenuBar>"
    "</gui>");
} // namespace

MarkdownViewActions::MarkdownViewActions(QWidget *dialogParent, QObject *parent)
    : ViewActions(parent)
    , m_dialogParent(dialogParent)
{
    setComponentName(QStringLiteral("corbomite_markdown_actions"), i18n("Markdown Editor"));
    setupActions();
    setXML(kMarkdownGuiXml, false /*merge*/);

    actionCollection()->setConfigGroup(QStringLiteral("Shortcuts"));
    if (m_dialogParent)
        actionCollection()->addAssociatedWidget(m_dialogParent);

    // Nothing bound yet — disable everything (Tier B "unbound" default).
    refresh();
}

QString MarkdownViewActions::viewType() const
{
    return QStringLiteral("markdown");
}

NoteEditorWidget *MarkdownViewActions::editor() const
{
    return m_boundView ? m_boundView->editorWidget() : nullptr;
}

void MarkdownViewActions::bind(View *view)
{
    disconnect(m_editorContextConnection);
    disconnect(m_viewModeConnection);

    m_boundView = qobject_cast<MarkdownView *>(view);

    if (auto *ed = editor()) {
        // Tier C (heading radio) AND Tier B (canEdit() may have changed
        // alongside the context — e.g. entering/leaving Reading mode) —
        // both halves must re-run on this signal, same discipline as
        // ActionContextController's own O2.T3 contextChanged() wiring.
        m_editorContextConnection =
            connect(ed, &NoteEditorWidget::editorContextChanged, this,
                    [this](const Markoff::EditorContext &ctx) {
                        onEditorContextChanged(ctx);
                        refresh();
                    });
        m_viewModeConnection =
            connect(ed, &NoteEditorWidget::viewModeChanged, this,
                    [this](NoteEditorWidget::ViewMode mode) {
                        syncEditorModeCheckState(static_cast<int>(mode));
                        refresh();
                    });
        syncEditorModeCheckState(static_cast<int>(ed->viewMode()));
    }

    refresh();
}

void MarkdownViewActions::unbind()
{
    disconnect(m_editorContextConnection);
    disconnect(m_viewModeConnection);
    m_boundView = nullptr;
    refresh();
}

void MarkdownViewActions::refresh()
{
    auto *ac = actionCollection();
    const bool isMarkdown = m_boundView != nullptr;
    // O2.T4: routed through the Tier-B capability virtual — a bound
    // MarkdownView always implies an open vault (you cannot have a
    // markdown leaf otherwise), so "bound" alone used to also need an
    // "open" check upstream in ActionContextController; here it collapses
    // to just canEdit().
    const bool canEdit = isMarkdown && m_boundView->canEdit();

    static const QStringList verbActionIds = {
        QStringLiteral("format_bold"), QStringLiteral("format_italic"),
        QStringLiteral("format_strikethrough"), QStringLiteral("format_inline_code"),
        QStringLiteral("insert_link"),
        QStringLiteral("heading_1"), QStringLiteral("heading_2"),
        QStringLiteral("heading_3"), QStringLiteral("heading_4"),
        QStringLiteral("heading_5"), QStringLiteral("heading_6"),
    };
    for (const auto &id : verbActionIds)
        if (auto *a = ac->action(id)) a->setEnabled(canEdit);

    if (auto *a = ac->action(QStringLiteral("insert_table"))) a->setEnabled(canEdit);
    if (auto *a = ac->action(QStringLiteral("insert_callout"))) a->setEnabled(canEdit);
    if (auto *a = ac->action(QStringLiteral("insert_template"))) a->setEnabled(isMarkdown);

    // Clipboard (Cluster N). canCopy uses hasCursor() (not a real
    // hasSelection() — Markoff::MarkdownView doesn't expose one yet) as a
    // cheap "there is an active leaf with a document" proxy, same as the
    // pre-merge MainWindow implementation this was ported from; copying
    // with nothing actually selected is a safe no-op (the leaf's
    // copy()/selectedText() early-out on an empty selection).
    const bool canCopy = editor() && editor()->activeLeaf()
                       && editor()->activeLeaf()->hasCursor();
    for (const auto &id : {QStringLiteral("edit_copy"),
                           QStringLiteral("edit_copy_as_markdown"),
                           QStringLiteral("edit_copy_as_plain"),
                           QStringLiteral("edit_copy_as_html"),
                           QStringLiteral("edit_copy_as_rtf")})
        if (auto *a = ac->action(id)) a->setEnabled(canCopy);
    for (const auto &id : {QStringLiteral("edit_cut"),
                           QStringLiteral("edit_paste"),
                           QStringLiteral("edit_paste_plain")})
        if (auto *a = ac->action(id)) a->setEnabled(canEdit);

    // Stubs — no Markoff-side implementation yet (§D7). Always disabled.
    static const QStringList stubActionIds = {
        QStringLiteral("insert_wiki_link"), QStringLiteral("insert_image"),
        QStringLiteral("insert_code_block"), QStringLiteral("insert_block_quote"),
        QStringLiteral("insert_horizontal_rule"), QStringLiteral("toggle_checkbox"),
        QStringLiteral("heading_increase"), QStringLiteral("heading_decrease"),
        QStringLiteral("table_row_above"), QStringLiteral("table_row_below"),
        QStringLiteral("table_col_left"),  QStringLiteral("table_col_right"),
        QStringLiteral("table_delete_row"), QStringLiteral("table_delete_col"),
        QStringLiteral("fold_all"), QStringLiteral("unfold_all"),
        QStringLiteral("toggle_fold"),
    };
    for (const auto &id : stubActionIds)
        if (auto *a = ac->action(id)) a->setEnabled(false);

    if (auto *a = ac->action(QStringLiteral("editor_toggle_mode"))) a->setEnabled(isMarkdown);
    if (auto *a = ac->action(QStringLiteral("view_source_mode"))) a->setEnabled(isMarkdown);
    if (auto *a = ac->action(QStringLiteral("view_editing_mode"))) a->setEnabled(isMarkdown);
    if (auto *a = ac->action(QStringLiteral("view_reading_mode"))) a->setEnabled(isMarkdown);

    // Clear the heading radio on every refresh; onEditorContextChanged
    // re-checks the right one once the (possibly new) editor reports its
    // block context.
    for (int i = 1; i <= 6; ++i)
        if (auto *a = ac->action(QStringLiteral("heading_%1").arg(i)))
            a->setChecked(false);

    if (!isMarkdown) {
        if (auto *a = ac->action(QStringLiteral("view_source_mode"))) a->setChecked(false);
        if (auto *a = ac->action(QStringLiteral("view_editing_mode"))) a->setChecked(false);
        if (auto *a = ac->action(QStringLiteral("view_reading_mode"))) a->setChecked(false);
    }
}

QList<QAction *> MarkdownViewActions::toolBarActions() const
{
    auto *ac = actionCollection();
    QList<QAction *> actions;
    for (const auto &id : {
             QStringLiteral("editor_toggle_mode"),
             QStringLiteral("format_bold"),
             QStringLiteral("format_italic"),
             QStringLiteral("format_strikethrough"),
             QStringLiteral("format_inline_code"),
             QStringLiteral("insert_link"),
         }) {
        if (auto *a = ac->action(id)) actions << a;
    }
    return actions;
}

void MarkdownViewActions::onEditorContextChanged(const Markoff::EditorContext &ctx)
{
    auto *ac = actionCollection();
    const bool isHeading =
        ctx.blockKind == QLatin1String(Markoff::BlockKindNames::Heading);
    for (int level = 1; level <= 6; ++level) {
        if (auto *a = ac->action(QStringLiteral("heading_%1").arg(level)))
            a->setChecked(isHeading && ctx.headingLevel == level);
    }
}

void MarkdownViewActions::syncEditorModeCheckState(int viewMode)
{
    using VM = NoteEditorWidget::ViewMode;
    QString id;
    switch (static_cast<VM>(viewMode)) {
    case VM::Source:      id = QStringLiteral("view_source_mode");  break;
    case VM::LivePreview: id = QStringLiteral("view_editing_mode"); break;
    case VM::Reading:     id = QStringLiteral("view_reading_mode"); break;
    }
    if (auto *act = actionCollection()->action(id))
        act->setChecked(true);
}

// ---------------------------------------------------------------------
// setupActions() — moved verbatim out of MainWindow::setupActions()
// (Cluster V Phase 2+3 / Cluster O O3.T6). Connect bodies are unchanged;
// they now call this->editor() instead of MainWindow's activeEditor().
// ---------------------------------------------------------------------

void MarkdownViewActions::setupActions()
{
    auto *ac = actionCollection();

    {
        auto *toggleMode = ac->addAction(QStringLiteral("editor_toggle_mode"));
        toggleMode->setText(i18n("Toggle Editor Mode"));
        toggleMode->setIcon(QIcon::fromTheme(QStringLiteral("view-preview")));
        KActionCollection::setDefaultShortcut(toggleMode, QKeySequence(Qt::CTRL | Qt::Key_E));
        connect(toggleMode, &QAction::triggered, this, [this]() {
            auto *ed = editor();
            if (!ed) return;
            using VM = NoteEditorWidget::ViewMode;
            switch (ed->viewMode()) {
                case VM::Source:      ed->setViewMode(VM::LivePreview); break;
                case VM::LivePreview: ed->setViewMode(VM::Reading);     break;
                case VM::Reading:     ed->setViewMode(VM::Source);      break;
            }
        });
    }

    auto *insertTpl = ac->addAction(QStringLiteral("insert_template"));
    insertTpl->setText(i18n("Insert Template"));
    insertTpl->setIcon(QIcon::fromTheme(QStringLiteral("document-new-from-template")));
    KActionCollection::setDefaultShortcut(insertTpl, QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(insertTpl, &QAction::triggered, this, &MarkdownViewActions::insertTemplateRequested);

    // View > Editor Mode submenu — three checkable radio actions selecting
    // one of the three ViewModes. Ctrl+E is owned by editor_toggle_mode
    // (3-way cycle) — no per-mode shortcut is registered here.
    auto *modeGroup = new QActionGroup(this);
    modeGroup->setExclusive(true);
    auto addModeAction = [this, ac, modeGroup](
        const QString &id, const QString &label, const QString &icon,
        NoteEditorWidget::ViewMode mode) {
        auto *act = ac->addAction(id);
        act->setText(label);
        act->setIcon(QIcon::fromTheme(icon));
        act->setCheckable(true);
        act->setActionGroup(modeGroup);
        connect(act, &QAction::triggered, this, [this, mode]() {
            if (auto *ed = editor()) ed->setViewMode(mode);
        });
        return act;
    };
    addModeAction(QStringLiteral("view_source_mode"),  i18n("Source"),
                  QStringLiteral("text-plain"),
                  NoteEditorWidget::ViewMode::Source);
    addModeAction(QStringLiteral("view_editing_mode"), i18n("Live Preview"),
                  QStringLiteral("text-x-markdown"),
                  NoteEditorWidget::ViewMode::LivePreview);
    addModeAction(QStringLiteral("view_reading_mode"), i18n("Reading"),
                  QStringLiteral("view-preview"),
                  NoteEditorWidget::ViewMode::Reading);

    // Contract v2: format verbs are virtuals on Markoff::MarkdownView; one
    // base call covers all three leaves (no-op on leaves without editing —
    // canEdit() drives the enabled state, see refresh()).
    auto addEditorActionBase = [this, ac](
        const QString &objName, const QString &icon, const QString &label,
        const QKeySequence &shortcut,
        void (Markoff::MarkdownView::*verb)()) -> QAction* {
        auto *act = ac->addAction(objName);
        act->setText(label);
        if (!icon.isEmpty()) act->setIcon(QIcon::fromTheme(icon));
        if (!shortcut.isEmpty()) KActionCollection::setDefaultShortcut(act, shortcut);
        connect(act, &QAction::triggered, this, [this, verb]() {
            if (auto *ed = editor())
                if (auto *leaf = ed->activeLeaf())
                    (leaf->*verb)();
        });
        return act;
    };
    auto addEditorActionStub = [this, ac](
        const QString &objName, const QString &icon, const QString &label,
        const QKeySequence &shortcut = {}) -> QAction* {
        // TODO(port-foundation-exploration): no Markoff-side implementation
        // yet (§D7). Registered so menus/toolbars/KCommandBar can discover
        // the action and so refresh() can grey it out. Wire when the
        // corresponding Markoff feature lands.
        auto *act = ac->addAction(objName);
        act->setText(label);
        if (!icon.isEmpty()) act->setIcon(QIcon::fromTheme(icon));
        if (!shortcut.isEmpty()) KActionCollection::setDefaultShortcut(act, shortcut);
        act->setEnabled(false);
        return act;
    };

    if (auto *a = addEditorActionBase(
            QStringLiteral("format_bold"),
            QStringLiteral("format-text-bold"), i18n("Bold"), QKeySequence::Bold,
            &Markoff::MarkdownView::toggleBold))
        a->setCheckable(true);
    if (auto *a = addEditorActionBase(
            QStringLiteral("format_italic"),
            QStringLiteral("format-text-italic"), i18n("Italic"), QKeySequence::Italic,
            &Markoff::MarkdownView::toggleItalic))
        a->setCheckable(true);
    if (auto *a = addEditorActionBase(
            QStringLiteral("format_strikethrough"),
            QStringLiteral("format-text-strikethrough"), i18n("Strikethrough"),
            QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_X),
            &Markoff::MarkdownView::toggleStrikethrough))
        a->setCheckable(true);
    if (auto *a = addEditorActionBase(
            QStringLiteral("format_inline_code"),
            QStringLiteral("code-context"), i18n("Inline Code"),
            QKeySequence(Qt::CTRL | Qt::Key_E),
            &Markoff::MarkdownView::toggleInlineCode))
        a->setCheckable(true);

    addEditorActionBase(QStringLiteral("insert_link"),
                        QStringLiteral("insert-link"), i18n("Insert Link"),
                        QKeySequence(Qt::CTRL | Qt::Key_K),
                        &Markoff::MarkdownView::insertLink);

    // Clipboard (Cluster N, ported here post-merge — see class doc
    // comment). Default Cut/Copy/Paste have no KStandardAction shortcuts:
    // the leaf already handles Ctrl+X/C/V in keyPressEvent; registering
    // the standard chords here would double-fire. Copy-as is exclusive
    // (one MIME flavor) so Word/LibreOffice cannot prefer HTML over
    // markdown.
    addEditorActionBase(QStringLiteral("edit_cut"),
                        QStringLiteral("edit-cut"), i18n("Cut"),
                        QKeySequence(),
                        &Markoff::MarkdownView::cut);
    addEditorActionBase(QStringLiteral("edit_copy"),
                        QStringLiteral("edit-copy"), i18n("Copy"),
                        QKeySequence(),
                        &Markoff::MarkdownView::copy);
    addEditorActionBase(QStringLiteral("edit_paste"),
                        QStringLiteral("edit-paste"), i18n("Paste"),
                        QKeySequence(),
                        &Markoff::MarkdownView::paste);
    addEditorActionBase(QStringLiteral("edit_copy_as_markdown"),
                        QString(), i18n("Copy as Markdown"),
                        QKeySequence(),
                        &Markoff::MarkdownView::copyAsMarkdown);
    addEditorActionBase(QStringLiteral("edit_copy_as_plain"),
                        QString(), i18n("Copy as Plain Text"),
                        QKeySequence(),
                        &Markoff::MarkdownView::copyAsPlain);
    addEditorActionBase(QStringLiteral("edit_copy_as_html"),
                        QString(), i18n("Copy as HTML"),
                        QKeySequence(),
                        &Markoff::MarkdownView::copyAsHtml);
    addEditorActionBase(QStringLiteral("edit_copy_as_rtf"),
                        QString(), i18n("Copy as RTF"),
                        QKeySequence(),
                        &Markoff::MarkdownView::copyAsRtf);
    addEditorActionBase(QStringLiteral("edit_paste_plain"),
                        QStringLiteral("edit-paste"), i18n("Paste as Plain Text"),
                        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V),
                        &Markoff::MarkdownView::pasteAsPlain);
    addEditorActionStub(QStringLiteral("insert_wiki_link"),
                        QStringLiteral("insert-link"), i18n("Insert Wiki Link"),
                        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K));
    addEditorActionStub(QStringLiteral("insert_image"),
                        QStringLiteral("insert-image"), i18n("Insert Image"));
    addEditorActionStub(QStringLiteral("insert_code_block"),
                        QStringLiteral("code-block"), i18n("Insert Code Block"));
    addEditorActionStub(QStringLiteral("insert_block_quote"),
                        QStringLiteral("format-text-blockquote"),
                        i18n("Insert Block Quote"));
    addEditorActionStub(QStringLiteral("insert_horizontal_rule"),
                        QStringLiteral("distribute-horizontal-center"),
                        i18n("Insert Horizontal Rule"));
    addEditorActionStub(QStringLiteral("toggle_checkbox"),
                        QStringLiteral("checkbox"), i18n("Toggle Checkbox"));

    // Heading: H1..H6. H0 (paragraph) is not surfaced as a discrete
    // KAction; users hit Ctrl+0 via heading0Action when it propagates, or
    // use heading_decrease repeatedly.
    auto *headingGroup = new QActionGroup(this);
    headingGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
    for (int level = 1; level <= 6; ++level) {
        auto *act = ac->addAction(QStringLiteral("heading_%1").arg(level));
        act->setText(i18n("Heading %1", level));
        act->setIcon(QIcon::fromTheme(QStringLiteral("format-text-heading")));
        act->setCheckable(true);
        act->setActionGroup(headingGroup);
        KActionCollection::setDefaultShortcut(
            act, QKeySequence(Qt::CTRL | static_cast<Qt::Key>(Qt::Key_0 + level)));
        connect(act, &QAction::triggered, this, [this, level]() {
            auto *ed = editor();
            if (!ed || level < 0 || level > 6) return;
            if (auto *leaf = ed->activeLeaf())
                leaf->setHeadingLevel(level);   // 0 strips ATX markers, 1..6 sets
        });
    }
    // Increase / Decrease still stubs — Markoff has no native
    // increase/decrease semantic (§D7 marks these "cheap" for O7, not O3).
    addEditorActionStub(QStringLiteral("heading_increase"),
                        QStringLiteral("format-header-more"),
                        i18n("Increase Heading Level"));
    addEditorActionStub(QStringLiteral("heading_decrease"),
                        QStringLiteral("format-header-less"),
                        i18n("Decrease Heading Level"));

    // Insert: Table... / Callout... (dialog-wrapped) — dialog opens; the
    // actual insert is still a no-op until Markoff lands the insert paths
    // (§D7 / punch-list P3, disposed of in O7).
    auto *insertTable = ac->addAction(QStringLiteral("insert_table"));
    insertTable->setText(i18n("Insert Table..."));
    insertTable->setIcon(QIcon::fromTheme(QStringLiteral("insert-table")));
    connect(insertTable, &QAction::triggered, this, [this]() {
        auto *ed = editor();
        if (!ed || !ed->activeLeaf()) return;
        InsertTableDialog dlg(m_dialogParent);
        if (dlg.exec() != QDialog::Accepted) return;
        // TODO(port-foundation-exploration): insertTable was on the old
        // Markoff::Editor; the contract-v2 base has no insert-table verb
        // yet. Re-steer upstream (O7.T3) when it lands.
        (void)dlg.rows();
        (void)dlg.cols();
        (void)dlg.firstRowAsHeader();
    });

    auto *insertCallout = ac->addAction(QStringLiteral("insert_callout"));
    insertCallout->setText(i18n("Insert Callout..."));
    insertCallout->setIcon(QIcon::fromTheme(QStringLiteral("dialog-information")));
    connect(insertCallout, &QAction::triggered, this, [this]() {
        auto *ed = editor();
        if (!ed || !ed->activeLeaf()) return;
        CalloutPickerDialog dlg(m_dialogParent);
        if (dlg.exec() != QDialog::Accepted) return;
        // TODO(port-foundation-exploration): ditto insert_table's note.
        (void)dlg.selectedType();
        (void)dlg.title();
    });

    // Table operations — all stubs (no per-row/col API on Markoff yet).
    addEditorActionStub(QStringLiteral("table_row_above"),
                        QStringLiteral("edit-table-insert-row-above"),
                        i18n("Insert Row Above"));
    addEditorActionStub(QStringLiteral("table_row_below"),
                        QStringLiteral("edit-table-insert-row-below"),
                        i18n("Insert Row Below"));
    addEditorActionStub(QStringLiteral("table_col_left"),
                        QStringLiteral("edit-table-insert-column-left"),
                        i18n("Insert Column Left"));
    addEditorActionStub(QStringLiteral("table_col_right"),
                        QStringLiteral("edit-table-insert-column-right"),
                        i18n("Insert Column Right"));
    addEditorActionStub(QStringLiteral("table_delete_row"),
                        QStringLiteral("edit-table-delete-row"),
                        i18n("Delete Row"));
    addEditorActionStub(QStringLiteral("table_delete_col"),
                        QStringLiteral("edit-table-delete-column"),
                        i18n("Delete Column"));

    // View > Fold All / Unfold All / Toggle Fold — folding is not in
    // foundation-exploration at all yet; all stubs.
    addEditorActionStub(QStringLiteral("fold_all"),
                        QStringLiteral("collapse-all"), i18n("Fold All"),
                        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Minus));
    addEditorActionStub(QStringLiteral("unfold_all"),
                        QStringLiteral("expand-all"), i18n("Unfold All"));
    addEditorActionStub(QStringLiteral("toggle_fold"),
                        QStringLiteral("code-function"),
                        i18n("Toggle Fold at Cursor"),
                        QKeySequence(Qt::CTRL | Qt::Key_Period));
}

} // namespace Corbomite
