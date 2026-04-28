// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QUrl>

#include <functional>

namespace Corbomite {

/// Routes `corbomite://<action>` (and optionally `obsidian://<action>`)
/// URLs to handlers registered by plugins or the host. Wired into
/// `QDesktopServices::setUrlHandler` by `MainWindow` at app start.
///
/// Handler selection: the URL's *host* component is the action name.
/// `corbomite://open?vault=Foo&file=Bar` dispatches to the handler
/// registered for `"open"`, with the full URL passed through.
class ProtocolHandlerRegistry : public QObject
{
    Q_OBJECT
public:
    using Handler = std::function<void(const QUrl &)>;

    static ProtocolHandlerRegistry &instance();

    /// Register `handler` under `action`. Returns false if `action` is
    /// empty or already registered (first-wins).
    bool registerHandler(const QString &action, Handler handler);

    /// Remove the handler under `action`. Safe on unregistered actions.
    void unregisterHandler(const QString &action);

    bool hasHandler(const QString &action) const;
    int handlerCount() const { return m_handlers.size(); }

    /// Dispatch slot. Wired into `QDesktopServices::setUrlHandler`.
    /// Looks up the handler for `url.host()` and invokes it.
    Q_INVOKABLE void dispatch(const QUrl &url);

    /// Test-only: clear the singleton between test runs.
    void clearForTesting();

private:
    ProtocolHandlerRegistry() = default;
    QHash<QString, Handler> m_handlers;
};

} // namespace Corbomite
