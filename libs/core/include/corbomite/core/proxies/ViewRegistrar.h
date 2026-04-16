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
/// Stub — wire-up lands in Cluster Q Task 9.
class ViewRegistrar
{
public:
    explicit ViewRegistrar(ViewRegistry *registry) : m_registry(registry) {}

    void registerView(const QString &type, ViewFactory factory);
    void registerExtensions(const QStringList &extensions, const QString &type);
    void unregisterView(const QString &type);

private:
    ViewRegistry *m_registry;
};

} // namespace Corbomite
