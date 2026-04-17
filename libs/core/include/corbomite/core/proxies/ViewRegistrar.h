// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>
#include <functional>

namespace Corbomite {

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

private:
    ViewRegistry *m_registry;
    QStringList m_registeredTypes;
    QStringList m_registeredExtensions;
};

} // namespace Corbomite
