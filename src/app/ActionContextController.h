// src/app/ActionContextController.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/core/ToolBarPolicy.h"

#include <QHash>
#include <QPointer>
#include <QMetaObject>
#include <QObject>
#include <QSet>
#include <QString>

#include <markoff/core/EditorContext.h>

class KActionCollection;
class KToolBar;
class KXMLGUIFactory;

namespace Markoff {
class MarkdownView;
}

namespace Corbomite {

class Workspace;
class WorkspaceLeaf;
class CorbomiteApp;
class MarkdownFileView;
class NoteEditorWidget;
class CanvasFileView;
class View;
class ViewActions;
class ViewRegistry;

namespace Bases {
class BasesView;
}

/// The app-shell action-state controller.
///
/// Owns the Tier-B (enablement) and Tier-C (check-state) refresh logic
/// that used to be five scattered `MainWindow` methods
/// (`refreshEditorActions`, `updateEditorActionStates`,
/// `updateVaultActions`, `updateBackForwardActions`,
/// `updateTabStateActions`) plus the action-related half of the
/// `activeLeafChanged` mega-lambda. `MainWindow` still owns
/// `actionCollection()` (a `KXmlGuiWindow` requirement) and hands the
/// pointer to this controller, which never deletes it.
///
/// Tier A (presence — which actions exist in the chrome at all) belongs to
/// the `ViewActions` provider mechanism. This controller is Tier B/C only.
class ActionContextController : public QObject
{
    Q_OBJECT
public:
    explicit ActionContextController(KActionCollection *actionCollection,
                                      QObject *parent = nullptr);

    void setWorkspace(Workspace *workspace);
    void setApp(CorbomiteApp *app);

    /// Rebinds the controller's per-leaf connections (the active leaf's
    /// `viewChanged`/`pinnedChanged`, and the active view's
    /// `editorContextChanged`/`viewModeChanged` when it is a markdown
    /// leaf) and immediately runs a full refresh(). Call on every
    /// `Workspace::activeLeafChanged`.
    ///
    /// The controller ALSO reconnects on the leaf's own
    /// `viewChanged` (not just on `Workspace::activeLeafChanged`), so an
    /// in-place view-type swap — `WorkspaceLeaf::navigate()` swapping a
    /// markdown leaf to a canvas viewState without the active leaf
    /// pointer itself changing — refreshes action state too. Before this
    /// fix, `Workspace::setActiveLeaf` early-returning on an unchanged
    /// leaf meant `activeLeafChanged` never fired for that case, and
    /// nothing but back/forward listened to `viewChanged`.
    void bindActiveLeaf(WorkspaceLeaf *leaf);

    /// Master refresh — recomputes every tracked action's Tier B
    /// enable-state and Tier C check-state for the CURRENT context
    /// (active leaf's view type + capability + vault-open). Safe to call
    /// any time; cheap (no XMLGUI rebuild — that is the ViewActions
    /// provider mechanism's job).
    void refresh();

    // --- Active-view accessors, moved out of MainWindow so this
    // controller (and the provider code) has one place to ask
    // "what's focused right now." ---
    MarkdownFileView *activeMarkdownView() const;
    Bases::BasesView *activeBasesView() const;
    CanvasFileView *activeCanvasView() const;
    NoteEditorWidget *activeEditor() const;

    /// The focused leaf's `View::getViewType()` string ("markdown",
    /// "canvas", "bases", "graph", "empty", ...), or an empty string when
    /// there is no active leaf/view.
    QString currentViewType() const;

    /// Introspection used by `tst_action_context_no_silent_noop`: does
    /// action `id` have a REAL handler for the CURRENT
    /// context? This is ground truth independent of the action's current
    /// enabled bit — the bug class this controller fixes is exactly
    /// "enabled, but triggering it silently does nothing," so for every
    /// action this controller enables, this must return true. Actions this
    /// controller does not track (universal `KStandardAction`s, workspace/tab
    /// plumbing with no view-type dependency, etc.) default to "has a
    /// handler" — the table below only needs to enumerate the
    /// view-type-restricted surface, which is exactly this controller's
    /// subject matter.
    bool hasHandlerForCurrentContext(const QString &actionId) const;

    // -------------------------------------------------------------
    // The ViewActions provider mechanism (Tier A:
    // presence). Providers are constructed eagerly by MainWindow and
    // registered here by view type; only *installation* — addClient()/
    // removeClient() on the KXMLGUIFactory — is dynamic.
    // -------------------------------------------------------------

    /// MainWindow calls this once, right after setupGUI() (the factory
    /// doesn't exist before then).
    void setGuiFactory(KXMLGUIFactory *factory);

    /// Registers `provider` under `provider->viewType()`. Must be called
    /// for every provider before the first bindActiveLeaf().
    void registerProvider(ViewActions *provider);

    /// Hook the ViewRegistry's provider signals so a
    /// PLUGIN-registered view type can ship its own ViewActions. Host types
    /// still register directly via registerProvider() in MainWindow's ctor.
    ///
    /// QPointer throughout: a plugin provider's vtable lives in the plugin
    /// .so, so if it is destroyed without unregistering first, every stored
    /// copy must become null rather than dangle. Call this once, after the
    /// registry exists.
    void observePluginProviders(ViewRegistry *registry);
    void unregisterProvider(const QString &viewType);

    /// The provider whose XMLGUI client is currently installed (its type
    /// matches the focused leaf's view type), or nullptr if none is (no
    /// leaf focused, or no provider registered for the focused type).
    ViewActions *currentProvider() const { return m_currentProvider; }

    /// Registers `toolBar` as the persistent toolbar for `viewType`'s
    /// provider — created by MainWindow AFTER setupGUI(),
    /// same as the pre-existing RibbonToolBar. Wires the toolbar's
    /// context-menu override and immediately applies the persisted
    /// policy.
    void registerToolBar(const QString &viewType, KToolBar *toolBar);

    /// Current persisted tri-state policy for `viewType`'s toolbar.
    ToolBarPolicy toolBarPolicyFor(const QString &viewType) const;

    /// Sets and persists `policy` for `viewType`'s toolbar, then
    /// re-applies visibility for every registered toolbar.
    void setToolBarPolicy(const QString &viewType, ToolBarPolicy policy);

    /// Re-applies every registered toolbar's visibility from its
    /// persisted policy + the current context. Safe to call any time;
    /// called from refresh() and once more explicitly wherever a
    /// toolbar's visibility could have been silently overwritten (the
    /// KMainWindow applyMainWindowSettings trap).
    void applyToolBarPolicies();

public Q_SLOTS:
    /// Public so tests can drive it with a synthetic `EditorContext`
    /// without a live Markoff editor (mirrors the old
    /// `MainWindow::onEditorContextChanged` contract). Syncs the
    /// heading radio's checked state from `ctx`, then re-runs the
    /// markdown Tier-B refresh (canEdit may have changed alongside the
    /// context).
    void onEditorContextChanged(const Markoff::EditorContext &ctx);

    /// Tier C: syncs the View ▸ Editor Mode radio group (Source/Live/
    /// Reading) to the active editor's current mode. Public for the same
    /// reason as onEditorContextChanged above.
    void syncEditorModeCheckState(int viewMode);

private:
    void rebindActiveView();
    /// The client-swap logic: installs/uninstalls providers'
    /// KXMLGUIClients on the factory as the focused leaf's view type
    /// changes, guarded so an ordinary same-type tab switch costs one
    /// bind()+refresh(), never a client swap.
    void installProviderForCurrentContext();
    void installToolBarContextMenu(KToolBar *toolBar, const QString &viewType);

    // updateMarkdownActionStates() and
    // updateEditorModeActions() were retired — the actions they
    // touched (format/heading/insert/table/fold/editor-mode) moved into
    // MarkdownViewActions' own collection; its refresh() is the Tier-B
    // logic's new home.
    void updateVaultActions();
    void updateSaveAction();
    void updateFindActions();            // insert_template lives in MarkdownViewActions
    void updateZoomActions();            // enablement half — dispatch itself is polymorphic
    void updateBackForwardActions();
    void updateTabStateActions();
    void updateUndoRedoActions();

    void setEnabled(const QString &actionId, bool enabled) const;
    void registerHandlers();

    KActionCollection *m_actionCollection;
    Workspace *m_workspace = nullptr;
    CorbomiteApp *m_app = nullptr;
    WorkspaceLeaf *m_activeLeaf = nullptr;

    QMetaObject::Connection m_activeLeafViewChangedConnection;
    QMetaObject::Connection m_activeLeafPinnedConnection;
    QMetaObject::Connection m_activeEditorContextConnection;
    QMetaObject::Connection m_activeViewModeConnection;
    QMetaObject::Connection m_activeViewContextChangedConnection;

    // actionId -> set of view types with a real handler ("*" == universal,
    // unaffected by focused view type). See hasHandlerForCurrentContext().
    QHash<QString, QSet<QString>> m_handlerViewTypes;

    // Provider registry + toolbar policy.
    KXMLGUIFactory *m_guiFactory = nullptr;
    QHash<QString, QPointer<ViewActions>> m_providers;
    QPointer<ViewActions> m_currentProvider;
    QMetaObject::Connection m_providerRegisteredConn;
    QMetaObject::Connection m_providerUnregisteredConn;
    QString m_currentProviderType;
    QHash<QString, KToolBar *> m_toolBars;
};

} // namespace Corbomite
