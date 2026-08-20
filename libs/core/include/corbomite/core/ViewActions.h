// libs/core/include/corbomite/core/ViewActions.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <KXMLGUIClient>

#include <QList>
#include <QObject>
#include <QString>

class QAction;

namespace Corbomite {

class View;

/// Cluster O Phase O3 (O3.T1) — the Tier-A "presence" mechanism (doctrine
/// §D1 in docs/superpowers/plans/2026-08-20-cluster-o-context-sensitive-ui.md).
///
/// One `ViewActions` subclass per view type ("markdown", "canvas", "bases",
/// "graph", ...) owns exactly that type's menu(s), its own
/// `KActionCollection` (inherited from `KXMLGUIClient` — a second/child
/// XMLGUI client merging into the shell's menubar, precedented by
/// `CorbomiteMDI::GUIClient`, `src/mdi/CorbomiteMDI.cpp`), and the QActions
/// that populate its persistent toolbar (§D3/§D4).
///
/// Providers are constructed EAGERLY at `MainWindow` startup and registered
/// with `ActionContextController` by view type (O3.T2 — forced by the
/// Hotkeys page, which needs every type's shortcuts visible even with no
/// matching tab open). Only *installation* — `guiFactory()->addClient()` /
/// `removeClient()` — is dynamic, driven by the focused leaf's view type
/// (O3.T3). A provider is therefore constructed with a DEFAULT (parent-less)
/// `KXMLGUIClient()` base so it is never auto-merged as a permanent child
/// client; `ActionContextController` is the only code that calls
/// `addClient()`/`removeClient()` on it.
class ViewActions : public QObject, public KXMLGUIClient
{
    Q_OBJECT
public:
    explicit ViewActions(QObject *parent = nullptr);
    ~ViewActions() override;

    /// Matches `View::getViewType()` ("markdown", "canvas", "bases",
    /// "graph", ...) — the key `ActionContextController` looks this
    /// provider up by.
    virtual QString viewType() const = 0;

    /// Tier C: point this provider at the newly-focused `View` of its own
    /// type — `view->getViewType() == viewType()` always holds when this is
    /// called — and sync check-state (radios, checkable toggles, ...) from
    /// it. Called both when this provider's client is freshly installed AND
    /// on an ordinary same-type tab switch (O3.T3: a same-type switch costs
    /// one `bind()` + `refresh()`, never an XMLGUI client swap).
    virtual void bind(View *view) = 0;

    /// Tier B: drop the bound view and disable every action this provider
    /// owns — there is nothing left to act on. Called when the focused leaf
    /// stops being this provider's type (right before the client is
    /// removed from the factory) and on shutdown. Implementations must
    /// disconnect every per-view connection they made in `bind()` (no
    /// dangling connections to a torn-down view — the
    /// `GraphControlsPanel` cross-talk bug this project has hit before).
    virtual void unbind() = 0;

    /// Tier B/C: recompute every owned action's enable/check state from the
    /// currently bound view (or "nothing bound" defaults). Cheap — must
    /// never touch the `KXMLGUIFactory`/rebuild any container.
    virtual void refresh() = 0;

    /// The actions shown on this provider's persistent toolbar (§D4), in
    /// display order. May be called before the first `bind()` (right after
    /// the toolbar itself is constructed, post-`setupGUI()`); the returned
    /// `QAction`s live in this provider's own `actionCollection()` for the
    /// provider's whole lifetime, independent of whether its XMLGUI client
    /// is currently installed.
    virtual QList<QAction *> toolBarActions() const = 0;
};

} // namespace Corbomite
