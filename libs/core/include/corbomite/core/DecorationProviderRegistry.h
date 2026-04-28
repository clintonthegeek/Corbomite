// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QString>

namespace Corbomite {

class DecorationProvider;

/// Host-side registry of decoration providers, keyed by id. Plugin
/// proxies (`DecorationProviderRegistrar`) register against the
/// singleton instance.
///
/// **Note: dispatch wiring is deferred.** Today the registry stores
/// registrations but Markoff's render path does not yet consult it.
/// A follow-up in the markoff-family submodule will add a hook that
/// queries `instance().providers()` from the editor build pipeline,
/// at which point plugin-supplied decorations will appear without
/// any plugin-API change.
class DecorationProviderRegistry
{
public:
    static DecorationProviderRegistry &instance();

    /// Register `provider` under `id`. Returns false if `id` is empty
    /// or already taken; the caller retains ownership of the provider.
    bool registerProvider(const QString &id, DecorationProvider *provider);

    /// Remove the registration under `id`. Safe to call on unregistered
    /// ids.
    void unregisterProvider(const QString &id);

    bool hasProvider(const QString &id) const;

    /// Snapshot of the registered providers in registration order. The
    /// returned list does not transfer ownership; callers must not
    /// outlive any provider.
    QList<DecorationProvider *> providers() const;

    int providerCount() const { return m_providers.size(); }

    /// Test-only: clear the singleton between test runs.
    void clearForTesting();

private:
    DecorationProviderRegistry() = default;
    QHash<QString, DecorationProvider *> m_providers;
    QStringList m_order;
};

} // namespace Corbomite
