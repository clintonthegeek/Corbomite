// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPointer>
#include <QString>
#include <QStringList>
#include <functional>

namespace Corbomite {

class ViewActions;
class ViewRegistry;
class View;
class WorkspaceLeaf;

using ViewFactory = std::function<View *(WorkspaceLeaf *)>;

/// View-registration facade for plugins with the "ui.views" permission.
///
/// Tracks every type registered + every extension mapped, and unregisters
/// them all on destruction. Permission gating happens upstream in
/// PluginContext::views() — the proxy itself is unconditional.
class ViewRegistrar
{
public:
    explicit ViewRegistrar(ViewRegistry *registry);
    ~ViewRegistrar();

    ViewRegistrar(const ViewRegistrar &) = delete;
    ViewRegistrar &operator=(const ViewRegistrar &) = delete;

    void registerView(const QString &type, ViewFactory factory);
    void registerExtensions(const QStringList &extensions, const QString &type);
    void unregisterView(const QString &type);

    /// Cluster O Phase O5.T4 — let a plugin ship a `ViewActions` provider for
    /// its own view type, with no new plugin-API surface. Deliberately on the
    /// SAME proxy as `registerView()` (both gated by "ui.views"): a view type
    /// and its actions are one registration concern, and splitting them would
    /// mean inventing a second permission for the same relationship.
    ///
    /// LIFETIME (see ViewRegistry::registerViewActions): the provider's code
    /// lives in the plugin .so, so it must be unregistered BEFORE the library
    /// unloads. Call `unregisterViewActions()` from your `onUnload()` — that
    /// hook provably runs while the .so is still mapped. This class's
    /// destructor also unregisters, but that is the fallback for an
    /// abnormal teardown, not something to rely on.
    void registerViewActions(const QString &type, ViewActions *provider);
    void unregisterViewActions(const QString &type);

    /// View types still registered through this proxy. Used by the host on
    /// plugin disable to drive `Workspace::detachLeavesOfType` for each
    /// type the plugin owned, before unregister-on-destruction kicks in.
    QStringList registeredTypes() const { return m_registeredTypes; }

private:
    // QPointer so destructor can safely skip cleanup when the host
    // ViewRegistry has already been destroyed (e.g. under the Cluster Q
    // recreated-MainWindow teardown path in tst_e2e_gui testCleanShutdown).
    QPointer<ViewRegistry> m_registry;
    QStringList m_registeredTypes;
    /// Types whose ViewActions provider this proxy installed, so the
    /// destructor can take them back out in reverse order.
    QStringList m_registeredActionTypes;
    QStringList m_registeredExtensions;
};

} // namespace Corbomite
