// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

namespace Corbomite {

class DecorationProviderRegistry;
class DecorationProvider;

/// Decoration-provider registration facade for plugins with the
/// "ui.editor" permission. Auto-namespaces ids as `<pluginId>:<localId>`.
/// Tracks every full id and unregisters them all on destruction.
///
/// Caller retains ownership of the registered DecorationProvider
/// instances; the registrar holds non-owning pointers.
class DecorationProviderRegistrar
{
public:
    DecorationProviderRegistrar(DecorationProviderRegistry *registry,
                                  QString pluginId);
    ~DecorationProviderRegistrar();

    DecorationProviderRegistrar(const DecorationProviderRegistrar &) = delete;
    DecorationProviderRegistrar &operator=(
        const DecorationProviderRegistrar &) = delete;

    /// Register a decoration provider under `<pluginId>:<localId>`.
    /// Returns the namespaced full id on success, empty string on
    /// failure (collision or null inputs).
    QString registerProvider(const QString &localId,
                                DecorationProvider *provider);

    /// Remove the provider under `<pluginId>:<localId>`.
    void unregisterProvider(const QString &localId);

    const QString &pluginId() const { return m_pluginId; }

private:
    DecorationProviderRegistry *m_registry;
    QString m_pluginId;
    QStringList m_registeredIds;
};

} // namespace Corbomite
