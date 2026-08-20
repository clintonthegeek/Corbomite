// src/app/ActionContextController.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ActionContextController.h"

#include "CorbomiteApp.h"
#include "canvas/CanvasFileView.h"
#include "canvas/CanvasViewTab.h"
#include "corbomite/bases/BasesView.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "editor/MarkdownView.h"
#include "editor/NoteEditorWidget.h"

#include <canvas/CanvasScene.h>
#include <markoff/core/MarkdownView.h>

#include <KActionCollection>

#include <QAction>
#include <QUndoStack>

namespace Corbomite {

namespace {
// View-type string constants — match View::getViewType() across the four
// concrete view classes (MarkdownView/CanvasFileView/BasesView/GraphView).
const QString kMarkdown = QStringLiteral("markdown");
const QString kCanvas = QStringLiteral("canvas");
const QString kBases = QStringLiteral("bases");
const QString kGraph = QStringLiteral("graph");
const QString kUniversal = QStringLiteral("*");
} // namespace

ActionContextController::ActionContextController(KActionCollection *actionCollection,
                                                   QObject *parent)
    : QObject(parent)
    , m_actionCollection(actionCollection)
{
    registerHandlers();
}

void ActionContextController::setWorkspace(Workspace *workspace)
{
    m_workspace = workspace;
}

void ActionContextController::setApp(CorbomiteApp *app)
{
    m_app = app;
}

// ---------------------------------------------------------------------
// Active-view accessors
// ---------------------------------------------------------------------

MarkdownView *ActionContextController::activeMarkdownView() const
{
    if (!m_workspace || !m_workspace->activeLeaf())
        return nullptr;
    return qobject_cast<MarkdownView *>(m_workspace->activeLeaf()->view());
}

Bases::BasesView *ActionContextController::activeBasesView() const
{
    if (!m_workspace || !m_workspace->activeLeaf())
        return nullptr;
    return qobject_cast<Bases::BasesView *>(m_workspace->activeLeaf()->view());
}

CanvasFileView *ActionContextController::activeCanvasView() const
{
    if (!m_workspace || !m_workspace->activeLeaf())
        return nullptr;
    return qobject_cast<CanvasFileView *>(m_workspace->activeLeaf()->view());
}

NoteEditorWidget *ActionContextController::activeEditor() const
{
    auto *mv = activeMarkdownView();
    return mv ? mv->editorWidget() : nullptr;
}

QString ActionContextController::currentViewType() const
{
    if (!m_workspace || !m_workspace->activeLeaf() || !m_workspace->activeLeaf()->view())
        return {};
    return m_workspace->activeLeaf()->view()->getViewType();
}

// ---------------------------------------------------------------------
// Binding
// ---------------------------------------------------------------------

void ActionContextController::bindActiveLeaf(WorkspaceLeaf *leaf)
{
    disconnect(m_activeLeafViewChangedConnection);
    disconnect(m_activeLeafPinnedConnection);

    m_activeLeaf = leaf;

    if (leaf) {
        // O1.T2: an in-place view-type swap (WorkspaceLeaf::navigate()
        // swapping the leaf's View without the active-leaf pointer
        // itself changing) fires viewChanged but NOT
        // Workspace::activeLeafChanged. Rebinding the active view's
        // signals and re-running a full refresh here — the same pattern
        // already used for back/forward history — is what makes that
        // case pick up fresh action state instead of leaving stale
        // Format/Heading enablement and a stuck editor-mode radio behind
        // (report §4.2).
        m_activeLeafViewChangedConnection =
            connect(leaf, &WorkspaceLeaf::viewChanged, this, [this]() {
                rebindActiveView();
                refresh();
            });
        m_activeLeafPinnedConnection =
            connect(leaf, &WorkspaceLeaf::pinnedChanged, this,
                    &ActionContextController::updateTabStateActions);
    }

    rebindActiveView();
    refresh();
}

void ActionContextController::rebindActiveView()
{
    disconnect(m_activeEditorContextConnection);
    disconnect(m_activeViewModeConnection);
    disconnect(m_activeViewContextChangedConnection);

    if (auto *editor = activeEditor()) {
        m_activeEditorContextConnection =
            connect(editor, &NoteEditorWidget::editorContextChanged, this,
                    &ActionContextController::onEditorContextChanged,
                    Qt::UniqueConnection);
        m_activeViewModeConnection =
            connect(editor, &NoteEditorWidget::viewModeChanged, this,
                    [this](NoteEditorWidget::ViewMode mode) {
                        syncEditorModeCheckState(static_cast<int>(mode));
                    });
        syncEditorModeCheckState(static_cast<int>(editor->viewMode()));
    }

    // O2.T3 — generic Tier-B refresh trigger: every view type forwards
    // its own capability-relevant signals onto View::contextChanged()
    // (markdown's editor context/view-mode, canvas's selection/undo,
    // bases's selection/undo; graph never — constant capabilities), so
    // this one connection covers all of them without the controller
    // knowing any type-specific signal names. Same rebind-per-leaf
    // discipline as the connections above.
    if (auto *view = m_activeLeaf ? m_activeLeaf->view() : nullptr) {
        m_activeViewContextChangedConnection =
            connect(view, &View::contextChanged, this,
                    &ActionContextController::refresh);
    }
}

// ---------------------------------------------------------------------
// Master refresh
// ---------------------------------------------------------------------

void ActionContextController::refresh()
{
    updateMarkdownActionStates();
    updateEditorModeActions();
    updateVaultActions();
    updateSaveAction();
    updateFindAndTemplateActions();
    updateZoomActions();
    updateBackForwardActions();
    updateTabStateActions();
    updateUndoRedoActions();
}

void ActionContextController::setEnabled(const QString &actionId, bool enabled) const
{
    if (auto *act = m_actionCollection->action(actionId))
        act->setEnabled(enabled);
}

// ---------------------------------------------------------------------
// O1.T6 — merged refreshEditorActions() + updateEditorActionStates().
// The two functions used to disagree (isMarkdown vs hasEditing()); call
// ordering made the stricter one win by accident, and Insert > Table was
// gated on isMarkdown alone — enabled in read-only Reading mode. One
// function, one predicate.
// ---------------------------------------------------------------------

void ActionContextController::updateMarkdownActionStates()
{
    auto *mv = activeMarkdownView();
    const bool isMarkdown = mv != nullptr;

    // O2.T4: routed through the Tier-B capability virtual instead of a
    // bespoke activeEditor()->activeLeaf()->hasEditing() read.
    const bool canEdit = isMarkdown && mv->canEdit();

    // Live format verbs — Tier A (markdown only) AND Tier B (canEdit,
    // false in Reading mode / while read-only).
    static const QStringList verbActionIds = {
        QStringLiteral("format_bold"), QStringLiteral("format_italic"),
        QStringLiteral("format_strikethrough"), QStringLiteral("format_inline_code"),
        QStringLiteral("insert_link"),
        QStringLiteral("heading_1"), QStringLiteral("heading_2"),
        QStringLiteral("heading_3"), QStringLiteral("heading_4"),
        QStringLiteral("heading_5"), QStringLiteral("heading_6"),
    };
    for (const auto &id : verbActionIds)
        setEnabled(id, canEdit);

    // Dialog-wrapped actions (Insert Table.../Insert Callout...): Tier A
    // (markdown) AND Tier B (canEdit) — opening an insert dialog in
    // read-only Reading mode makes no sense even though the eventual
    // insert is still a placebo (O7 disposes of that separately, per
    // D7). O2.T1 test tst_action_context::readingMode_disablesFormatVerbs
    // caught this still gating on isMarkdown alone despite O1.T6's own
    // comment claiming the fix.
    setEnabled(QStringLiteral("insert_table"), canEdit);
    setEnabled(QStringLiteral("insert_callout"), canEdit);

    // Stubs — no Markoff-side implementation yet. Always disabled
    // regardless of leaf state; O7 disposes of these (registerPlannedAction).
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
        setEnabled(id, false);

    // Clear the heading radio on every refresh; onEditorContextChanged
    // re-checks the right one once the (possibly new) editor reports its
    // block context.
    for (int i = 1; i <= 6; ++i)
        if (auto *act = m_actionCollection->action(QStringLiteral("heading_%1").arg(i)))
            act->setChecked(false);
}

void ActionContextController::onEditorContextChanged(const Markoff::EditorContext &ctx)
{
    // O2.T3: Tier-B re-enablement (updateMarkdownActionStates(), which
    // reads canEdit()) now happens via the generic View::contextChanged()
    // -> refresh() path — MarkdownView forwards this same
    // editorContextChanged signal onto contextChanged(). This slot is
    // left owning only the heading-radio Tier-C check state, which needs
    // the EditorContext payload contextChanged() doesn't carry.
    const bool isHeading =
        ctx.blockKind == QLatin1String(Markoff::BlockKindNames::Heading);
    for (int level = 1; level <= 6; ++level) {
        if (auto *a = m_actionCollection->action(QStringLiteral("heading_%1").arg(level)))
            a->setChecked(isHeading && ctx.headingLevel == level);
    }
}

void ActionContextController::syncEditorModeCheckState(int viewMode)
{
    using VM = NoteEditorWidget::ViewMode;
    QString id;
    switch (static_cast<VM>(viewMode)) {
    case VM::Source:      id = QStringLiteral("view_source_mode");  break;
    case VM::LivePreview: id = QStringLiteral("view_editing_mode"); break;
    case VM::Reading:     id = QStringLiteral("view_reading_mode"); break;
    }
    if (auto *act = m_actionCollection->action(id))
        act->setChecked(true);
}

// ---------------------------------------------------------------------
// O1.T7 — editor_toggle_mode + the three View > Editor Mode radios.
// Previously view_source_mode was never enable-gated at all (report
// §4.5); its two siblings and editor_toggle_mode were vault-gated only,
// so all four stayed enabled on a canvas/bases/graph tab too — a silent
// no-op class of the same shape (cycleEditorMode()/setViewMode() both
// early-return with no active MarkdownView).
// ---------------------------------------------------------------------

void ActionContextController::updateEditorModeActions()
{
    const bool open = m_app && m_app->isOpen();
    const bool isMarkdown = currentViewType() == kMarkdown;
    const bool enabled = open && isMarkdown;
    setEnabled(QStringLiteral("editor_toggle_mode"), enabled);
    setEnabled(QStringLiteral("view_source_mode"), enabled);
    setEnabled(QStringLiteral("view_editing_mode"), enabled);
    setEnabled(QStringLiteral("view_reading_mode"), enabled);

    // O1.T2 regression guard: syncEditorModeCheckState() only ever CHECKS
    // the current mode's radio when a markdown editor is active — nothing
    // previously cleared it when the focused tab stopped being markdown
    // (the old mega-lambda's sync block was itself gated on `if (editor)`,
    // so it silently skipped this case too). Without this, an in-place
    // swap to canvas/bases left the last markdown mode's radio checked
    // forever. Clear all three explicitly whenever markdown isn't active.
    if (!isMarkdown) {
        if (auto *a = m_actionCollection->action(QStringLiteral("view_source_mode")))
            a->setChecked(false);
        if (auto *a = m_actionCollection->action(QStringLiteral("view_editing_mode")))
            a->setChecked(false);
        if (auto *a = m_actionCollection->action(QStringLiteral("view_reading_mode")))
            a->setChecked(false);
    }
}

// ---------------------------------------------------------------------
// O1.T3 (enablement half) — view_zoom_in/out/reset are only ever a real
// handler for markdown/canvas/graph (each overrides View::zoomIn/Out/
// Reset onto a real viewport, per registerHandlers() above); bases/empty
// fall back to the no-op View base. Previously these three actions had
// NO enablement gate at all (report §3.4's ☠ class) and stayed
// permanently enabled regardless of what was focused.
// ---------------------------------------------------------------------

void ActionContextController::updateZoomActions()
{
    const QString type = currentViewType();
    const bool zoomable = type == kMarkdown || type == kCanvas || type == kGraph;
    setEnabled(QStringLiteral("view_zoom_in"), zoomable);
    setEnabled(QStringLiteral("view_zoom_out"), zoomable);
    setEnabled(QStringLiteral("view_zoom_reset"), zoomable);
}

// ---------------------------------------------------------------------
// Vault-open gating for workspace/window-level actions with no view-type
// dependency (D1: vault-open is a Tier-B capability of the window, not a
// fourth tier).
// ---------------------------------------------------------------------

void ActionContextController::updateVaultActions()
{
    const bool open = m_app && m_app->isOpen();

    setEnabled(QStringLiteral("file_close_vault"), open);
    setEnabled(QStringLiteral("file_new_note"), open);
    setEnabled(QStringLiteral("file_new_canvas"), open);
    setEnabled(QStringLiteral("quick_switcher"), open);
    setEnabled(QStringLiteral("search_vault"), open);
    setEnabled(QStringLiteral("graph_view"), open);
    setEnabled(QStringLiteral("tab_close"), open);
    setEnabled(QStringLiteral("tab_next"), open);
    setEnabled(QStringLiteral("tab_prev"), open);
    setEnabled(QStringLiteral("open_daily_note"), open);
    setEnabled(QStringLiteral("split_right"), open);
    setEnabled(QStringLiteral("split_down"), open);
    // file_save -> updateSaveAction() (O1.T4, type-aware).
    // view_editing_mode/reading_mode/source_mode/editor_toggle_mode ->
    //   updateEditorModeActions() (O1.T7, type-aware).
    // insert_template -> updateFindAndTemplateActions() (O1.T5, type-aware).
}

// ---------------------------------------------------------------------
// O1.T4 — file_save. saveCurrentNote() (MainWindow) already handles
// markdown (via activeEditor()) and any other TextFileView subclass
// (which covers bases for free — BasesView : TextFileView). Canvas is a
// bare FileView, so Ctrl+S was a real no-op there (report §3.1); the
// canvas save path is now wired in saveCurrentNote() directly. This
// enablement gate additionally catches graph/empty tabs, which have no
// save target at all and were previously enabled (and silently did
// nothing) whenever a vault was merely open.
// ---------------------------------------------------------------------

void ActionContextController::updateSaveAction()
{
    const bool open = m_app && m_app->isOpen();
    const QString type = currentViewType();
    const bool hasSaveTarget =
        type == kMarkdown || type == kCanvas || type == kBases;
    setEnabled(QStringLiteral("file_save"), open && hasSaveTarget);
}

// ---------------------------------------------------------------------
// O1.T5 — find/replace/insert_template honesty. edit_find routes to
// Bases' own search box (BasesView::focusSearch(), wired in
// MainWindow::onFind()) where a real target exists; replace/find-next/
// find-prev and insert_template have no bases-side equivalent and are
// disabled off-markdown rather than faked.
// ---------------------------------------------------------------------

void ActionContextController::updateFindAndTemplateActions()
{
    const bool open = m_app && m_app->isOpen();
    const QString type = currentViewType();
    const bool isMarkdown = type == kMarkdown;
    const bool isBases = type == kBases;

    setEnabled(QStringLiteral("edit_find"), open && (isMarkdown || isBases));
    setEnabled(QStringLiteral("edit_replace"), open && isMarkdown);
    setEnabled(QStringLiteral("edit_find_next"), open && isMarkdown);
    setEnabled(QStringLiteral("edit_find_prev"), open && isMarkdown);
    setEnabled(QStringLiteral("insert_template"), open && isMarkdown);
}

// ---------------------------------------------------------------------
// Back/forward + pin/stacked — unchanged logic, moved verbatim out of
// MainWindow.
// ---------------------------------------------------------------------

void ActionContextController::updateBackForwardActions()
{
    auto *leaf = m_workspace ? m_workspace->activeLeaf() : nullptr;
    const bool canBack = leaf && leaf->history().canGoBack();
    const bool canForward = leaf && leaf->history().canGoForward();
    setEnabled(QStringLiteral("go_back"), canBack);
    setEnabled(QStringLiteral("go_forward"), canForward);
}

void ActionContextController::updateTabStateActions()
{
    auto *leaf = m_workspace ? m_workspace->activeLeaf() : nullptr;
    if (auto *act = m_actionCollection->action(QStringLiteral("tab_pin_toggle"))) {
        act->setEnabled(leaf != nullptr);
        act->setChecked(leaf && leaf->pinned());
    }
    if (auto *act = m_actionCollection->action(QStringLiteral("tab_toggle_stacked"))) {
        const QString groupId = (m_workspace && leaf) ? m_workspace->tabGroupIdOf(leaf) : QString();
        act->setEnabled(!groupId.isEmpty());
        act->setChecked(!groupId.isEmpty() && m_workspace->isTabGroupStacked(groupId));
    }
}

// ---------------------------------------------------------------------
// O1.T8 — edit_undo/edit_redo from the active view's real undo-stack
// depth instead of always-enabled. Bases and canvas each own a real
// QUndoStack and answer canUndo()/canRedo() precisely. Markoff's
// MarkdownView base has no global canUndoD2()/canRedoD2() query yet
// (only the per-block canUndoForBlock) — approximated with hasEditing()
// until that lands upstream (punch-listed: exposing a real depth query
// would tighten this from "editable" to "actually has undo history").
// ---------------------------------------------------------------------

void ActionContextController::updateUndoRedoActions()
{
    // O2.T1: routed through the generic Tier-B virtuals instead of a
    // per-type branch — each View subclass already knows how to answer
    // canUndo()/canRedo() for itself (bases' real QUndoStack, canvas' real
    // QUndoStack, markdown's hasEditing() approximation — see
    // MarkdownView::canUndo()'s comment for why that one isn't a real
    // depth query).
    auto *leaf = m_workspace ? m_workspace->activeLeaf() : nullptr;
    auto *view = leaf ? leaf->view() : nullptr;
    const bool canUndo = view && view->canUndo();
    const bool canRedo = view && view->canRedo();

    setEnabled(QStringLiteral("edit_undo"), canUndo);
    setEnabled(QStringLiteral("edit_redo"), canRedo);
}

// ---------------------------------------------------------------------
// tst_action_context_no_silent_noop support table.
// ---------------------------------------------------------------------

void ActionContextController::registerHandlers()
{
    auto reg = [this](const QString &id, std::initializer_list<QString> types) {
        m_handlerViewTypes[id] = QSet<QString>(types.begin(), types.end());
    };

    // Zoom — polymorphic (O1.T3): markdown/canvas/graph each override
    // View::zoomIn/Out/Reset onto a real viewport; bases/empty do not, so
    // these are disabled there rather than dispatching to the no-op base.
    reg(QStringLiteral("view_zoom_in"), {kMarkdown, kCanvas, kGraph});
    reg(QStringLiteral("view_zoom_out"), {kMarkdown, kCanvas, kGraph});
    reg(QStringLiteral("view_zoom_reset"), {kMarkdown, kCanvas, kGraph});

    // Editor-mode group (O1.T7).
    reg(QStringLiteral("editor_toggle_mode"), {kMarkdown});
    reg(QStringLiteral("view_source_mode"), {kMarkdown});
    reg(QStringLiteral("view_editing_mode"), {kMarkdown});
    reg(QStringLiteral("view_reading_mode"), {kMarkdown});

    // Live format verbs + headings (O1.T6).
    reg(QStringLiteral("format_bold"), {kMarkdown});
    reg(QStringLiteral("format_italic"), {kMarkdown});
    reg(QStringLiteral("format_strikethrough"), {kMarkdown});
    reg(QStringLiteral("format_inline_code"), {kMarkdown});
    reg(QStringLiteral("insert_link"), {kMarkdown});
    for (int i = 1; i <= 6; ++i)
        reg(QStringLiteral("heading_%1").arg(i), {kMarkdown});

    // Dialog-wrapped placebo actions — opening the dialog IS the visible
    // effect (O7 disposes of the placebo itself; out of O1 scope).
    reg(QStringLiteral("insert_table"), {kMarkdown});
    reg(QStringLiteral("insert_callout"), {kMarkdown});

    // Stubs — always disabled; if any of these is ever enabled without a
    // real implementation behind it, the introspection gate must fail.
    static const QStringList stubIds = {
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
    for (const auto &id : stubIds)
        m_handlerViewTypes[id] = QSet<QString>{};

    // Find/replace/template (O1.T5).
    reg(QStringLiteral("edit_find"), {kMarkdown, kBases});
    reg(QStringLiteral("edit_replace"), {kMarkdown});
    reg(QStringLiteral("edit_find_next"), {kMarkdown});
    reg(QStringLiteral("edit_find_prev"), {kMarkdown});
    reg(QStringLiteral("insert_template"), {kMarkdown});

    // Save (O1.T4).
    reg(QStringLiteral("file_save"), {kMarkdown, kCanvas, kBases});

    // Undo/redo (O1.T8).
    reg(QStringLiteral("edit_undo"), {kMarkdown, kCanvas, kBases});
    reg(QStringLiteral("edit_redo"), {kMarkdown, kCanvas, kBases});

    // Everything else this controller does not explicitly track (File/
    // Quit/Preferences, quick switcher, tab/split/navigation plumbing,
    // sidebar toggle, ...) has no view-type dependency at all — leaving
    // them untracked here makes hasHandlerForCurrentContext() default to
    // "universal," which is correct for them and keeps this table scoped
    // to O1's actual subject matter.
}

bool ActionContextController::hasHandlerForCurrentContext(const QString &actionId) const
{
    auto it = m_handlerViewTypes.constFind(actionId);
    if (it == m_handlerViewTypes.constEnd())
        return true; // untracked -> assumed universal (see registerHandlers()).
    const QSet<QString> &types = it.value();
    if (types.contains(kUniversal))
        return true;
    return types.contains(currentViewType());
}

} // namespace Corbomite
