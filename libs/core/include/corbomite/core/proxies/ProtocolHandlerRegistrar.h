// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>
#include <QUrl>

#include <functional>

namespace Corbomite {

class ProtocolHandlerRegistry;

/// Protocol-handler registration facade for plugins with the "protocol"
/// permission. Auto-namespaces actions as `<pluginId>.<localAction>`
/// (dot-separator: URL hosts disallow `:`). Tracks every full action
/// and removes them all on destruction.
class ProtocolHandlerRegistrar
{
public:
    using Handler = std::function<void(const QUrl &)>;

    ProtocolHandlerRegistrar(ProtocolHandlerRegistry *registry,
                              QString pluginId);
    ~ProtocolHandlerRegistrar();

    ProtocolHandlerRegistrar(const ProtocolHandlerRegistrar &) = delete;
    ProtocolHandlerRegistrar &operator=(const ProtocolHandlerRegistrar &) = delete;

    /// Register a handler for `<pluginId>.<localAction>`. Returns the
    /// full namespaced action on success, empty string on failure.
    QString registerHandler(const QString &localAction, Handler handler);

    /// Remove the handler under `<pluginId>.<localAction>`.
    void unregisterHandler(const QString &localAction);

    const QString &pluginId() const { return m_pluginId; }

private:
    ProtocolHandlerRegistry *m_registry;
    QString m_pluginId;
    QStringList m_registeredActions;
};

} // namespace Corbomite
