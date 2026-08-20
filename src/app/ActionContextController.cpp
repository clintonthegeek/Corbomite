// src/app/ActionContextController.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ActionContextController.h"

#include "CorbomiteApp.h"
#include "canvas/CanvasFileView.h"
#include "canvas/CanvasViewTab.h"
#include "corbomite/bases/BasesView.h"
#include "corbomite/core/View.h"
#include "corbomite/core/ViewActions.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomitesettings.h"
#include "editor/MarkdownView.h"
#include "editor/NoteEditorWidget.h"

#include <canvas/CanvasScene.h>
#include <markoff/core/MarkdownView.h>

#include <KActionCollection>
#include <KLocalizedString>
#include <KToolBar>
#include <KXMLGUIFactory>

#include <QAction>
#include <QActionGroup>
#include <QMenu>
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

    // O3.T6: the heading-radio / editor-mode-radio sync this controller
    // used to do directly (connecting NoteEditorWidget::editorContextChanged/
    // viewModeChanged) is now MarkdownViewActions::bind()'s job — those
    // actions live in the provider's own collection, not this one.
    // onEditorContextChanged()/syncEditorModeCheckState() stay below as
    // public slots (Phase C6's documented test seam) but are no longer
    // wired to a live signal here.

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

    // O3.T3 — Tier A: install/uninstall the provider whose type matches
    // the (possibly new) active view, guarded internally so an ordinary
    // same-type switch costs one bind()+refresh(), not a client swap.
    installProviderForCurrentContext();
}

// ---------------------------------------------------------------------
// Master refresh
// ---------------------------------------------------------------------

void ActionContextController::refresh()
{
    // O3.T6: markdown's Format/Heading/Insert/Table/fold/editor-mode Tier-B
    // logic moved into MarkdownViewActions::refresh() (called from
    // installProviderForCurrentContext()/rebindActiveView(), not here) —
    // those actions no longer live in this controller's actionCollection.
    updateVaultActions();
    updateSaveAction();
    updateFindActions();
    updateZoomActions();
    updateBackForwardActions();
    updateTabStateActions();
    updateUndoRedoActions();
    applyToolBarPolicies();
}

void ActionContextController::setEnabled(const QString &actionId, bool enabled) const
{
    if (auto *act = m_actionCollection->action(actionId))
        act->setEnabled(enabled);
}

// ---------------------------------------------------------------------
// O3.T6: the Tier-B logic that used to live here (O1.T6's merged
// refreshEditorActions()+updateEditorActionStates()) moved into
// MarkdownViewActions::refresh() — those actions (format/heading/insert/
// table verbs) live in the provider's own collection now, not this one's.
// onEditorContextChanged()/syncEditorModeCheckState() remain below as
// public slots (Phase C6's documented test seam) but are effectively
// unused in the live wiring — MarkdownViewActions::bind() owns the real
// connection to NoteEditorWidget's signals.
// ---------------------------------------------------------------------

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
    // insert_template -> MarkdownViewActions::refresh() (O3.T6).
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
// O1.T5 — find/replace honesty. edit_find routes to Bases' own search box
// (BasesView::focusSearch(), wired in MainWindow::onFind()) where a real
// target exists; replace/find-next/find-prev have no bases-side
// equivalent and are disabled off-markdown rather than faked.
// insert_template moved to MarkdownViewActions (O3.T6) — its Tier-B is
// now that provider's refresh(), not this function.
// ---------------------------------------------------------------------

void ActionContextController::updateFindActions()
{
    const bool open = m_app && m_app->isOpen();
    const QString type = currentViewType();
    const bool isMarkdown = type == kMarkdown;
    const bool isBases = type == kBases;

    setEnabled(QStringLiteral("edit_find"), open && (isMarkdown || isBases));
    setEnabled(QStringLiteral("edit_replace"), open && isMarkdown);
    setEnabled(QStringLiteral("edit_find_next"), open && isMarkdown);
    setEnabled(QStringLiteral("edit_find_prev"), open && isMarkdown);
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

    // O3.T6: the editor-mode group, live format verbs + headings, the two
    // dialog-wrapped placebo actions, every markdown-only stub, and
    // insert_template all moved into MarkdownViewActions' own collection
    // — they no longer appear in mw->actionCollection()->actions() at
    // all (Tier A: hidden, not just disabled, off-markdown), so this
    // table has nothing left to say about them. hasHandlerForCurrentContext()
    // still defaults untracked ids to "universal," which is vacuously
    // correct once an id simply isn't in the walked collection any more.

    // Find/replace (O1.T5). insert_template moved to MarkdownViewActions.
    reg(QStringLiteral("edit_find"), {kMarkdown, kBases});
    reg(QStringLiteral("edit_replace"), {kMarkdown});
    reg(QStringLiteral("edit_find_next"), {kMarkdown});
    reg(QStringLiteral("edit_find_prev"), {kMarkdown});

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

// ---------------------------------------------------------------------
// Cluster O Phase O3 — the ViewActions provider mechanism.
// ---------------------------------------------------------------------

void ActionContextController::setGuiFactory(KXMLGUIFactory *factory)
{
    m_guiFactory = factory;
}

void ActionContextController::registerProvider(ViewActions *provider)
{
    if (!provider) return;
    m_providers.insert(provider->viewType(), provider);
}

void ActionContextController::installProviderForCurrentContext()
{
    const QString type = currentViewType();
    auto *view = (m_workspace && m_workspace->activeLeaf())
                     ? m_workspace->activeLeaf()->view() : nullptr;

    if (type == m_currentProviderType) {
        // O3.T3: same type (or both empty) — no XMLGUI rebuild, just
        // rebind the provider to the (possibly new) View instance and
        // let it re-sync its own Tier B/C. Cheap.
        if (m_currentProvider && view)
            m_currentProvider->bind(view);
        return;
    }

    if (m_currentProvider) {
        m_currentProvider->unbind();
        if (m_guiFactory)
            m_guiFactory->removeClient(m_currentProvider);
    }

    m_currentProvider = m_providers.value(type, nullptr);
    m_currentProviderType = type;

    if (m_currentProvider) {
        if (m_guiFactory)
            m_guiFactory->addClient(m_currentProvider);
        if (view)
            m_currentProvider->bind(view);
    }

    // A provider swap can change which toolbar should be visible under
    // Auto policy (§D4).
    applyToolBarPolicies();
}

void ActionContextController::registerToolBar(const QString &viewType, KToolBar *toolBar)
{
    if (!toolBar) return;
    m_toolBars.insert(viewType, toolBar);
    // Icon-only by default for every provider toolbar (present and future —
    // O4/O5 land here for free). Unlike mainToolBar's declarative
    // iconText="icononly" (corbomiteui.rc.in), these toolbars are created
    // programmatically with no <ToolBar> XML element for KXMLGUI to read the
    // attribute from, so the style has to be set directly.
    toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    installToolBarContextMenu(toolBar, viewType);
    applyToolBarPolicies();
}

ToolBarPolicy ActionContextController::toolBarPolicyFor(const QString &viewType) const
{
    // O3 has exactly one provider (markdown); O4/O5 add a branch + a new
    // named kcfg entry per provider they land, same pattern (kcfg has no
    // dynamic-key entries, so this stays a short explicit dispatch rather
    // than a generic map — matches how other per-feature settings in this
    // codebase are named).
    if (viewType == kMarkdown)
        return toolBarPolicyFromString(CorbomiteSettings::self()->markdownToolBarPolicy());
    return ToolBarPolicy::Auto;
}

void ActionContextController::setToolBarPolicy(const QString &viewType, ToolBarPolicy policy)
{
    if (viewType == kMarkdown) {
        CorbomiteSettings::self()->setMarkdownToolBarPolicy(toolBarPolicyToString(policy));
        CorbomiteSettings::self()->save();
    }
    applyToolBarPolicies();
}

void ActionContextController::applyToolBarPolicies()
{
    const QString active = currentViewType();
    for (auto it = m_toolBars.constBegin(); it != m_toolBars.constEnd(); ++it) {
        const ToolBarPolicy policy = toolBarPolicyFor(it.key());
        const bool inContext = (it.key() == active);
        it.value()->setVisible(toolBarShouldBeVisible(policy, inContext));
    }
}

void ActionContextController::installToolBarContextMenu(KToolBar *toolBar, const QString &viewType)
{
    // Q3 — user override, from the toolbar's own context menu. KToolBar's
    // default right-click behaviour is QMainWindow's generic
    // "show/hide toolbars" popup; CustomContextMenu on this specific
    // toolbar lets us offer the tri-state choice instead when the user
    // right-clicks IT specifically (live-eyeball item — see phase report).
    toolBar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(toolBar, &QWidget::customContextMenuRequested, this,
            [this, toolBar, viewType](const QPoint &pos) {
        QMenu menu(toolBar);
        auto *group = new QActionGroup(&menu);
        group->setExclusive(true);
        auto addChoice = [&](ToolBarPolicy p, const QString &label) {
            auto *a = menu.addAction(label);
            a->setCheckable(true);
            a->setChecked(toolBarPolicyFor(viewType) == p);
            a->setActionGroup(group);
            connect(a, &QAction::triggered, this, [this, viewType, p]() {
                setToolBarPolicy(viewType, p);
            });
        };
        addChoice(ToolBarPolicy::Auto, i18n("Automatic"));
        addChoice(ToolBarPolicy::AlwaysShow, i18n("Always Show"));
        addChoice(ToolBarPolicy::AlwaysHide, i18n("Always Hide"));
        menu.addSeparator();
        auto *resetAll = menu.addAction(i18n("Reset All Toolbars to Automatic"));
        connect(resetAll, &QAction::triggered, this, [this]() {
            for (auto it = m_toolBars.constBegin(); it != m_toolBars.constEnd(); ++it)
                setToolBarPolicy(it.key(), ToolBarPolicy::Auto);
        });
        menu.addSeparator();
        // CustomContextMenu on this toolbar (above) fully replaces KToolBar's
        // own built-in context menu, which is otherwise where its icon/text
        // style picker lives — re-offer it here so overriding this session's
        // icon-only default (registerToolBar()) isn't lost for provider
        // toolbars specifically (mainToolBar/ribbonToolBar keep the KToolBar
        // default menu and so keep this for free).
        auto *styleGroup = new QActionGroup(&menu);
        styleGroup->setExclusive(true);
        auto addStyle = [&](Qt::ToolButtonStyle style, const QString &label) {
            auto *a = menu.addAction(label);
            a->setCheckable(true);
            a->setChecked(toolBar->toolButtonStyle() == style);
            a->setActionGroup(styleGroup);
            connect(a, &QAction::triggered, this, [toolBar, style]() {
                toolBar->setToolButtonStyle(style);
            });
        };
        addStyle(Qt::ToolButtonIconOnly, i18n("Icon Only"));
        addStyle(Qt::ToolButtonTextUnderIcon, i18n("Text Under Icon"));
        addStyle(Qt::ToolButtonTextBesideIcon, i18n("Text Alongside Icon"));
        menu.exec(toolBar->mapToGlobal(pos));
    });
}

} // namespace Corbomite
