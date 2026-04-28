// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/ProtocolHandlerRegistry.h"

namespace Corbomite {

ProtocolHandlerRegistry &ProtocolHandlerRegistry::instance()
{
    static ProtocolHandlerRegistry singleton;
    return singleton;
}

bool ProtocolHandlerRegistry::registerHandler(const QString &action,
                                                Handler handler)
{
    if (action.isEmpty() || !handler) return false;
    if (m_handlers.contains(action)) return false;
    m_handlers.insert(action, std::move(handler));
    return true;
}

void ProtocolHandlerRegistry::unregisterHandler(const QString &action)
{
    m_handlers.remove(action);
}

bool ProtocolHandlerRegistry::hasHandler(const QString &action) const
{
    return m_handlers.contains(action);
}

void ProtocolHandlerRegistry::dispatch(const QUrl &url)
{
    const QString action = url.host();
    auto it = m_handlers.constFind(action);
    if (it == m_handlers.constEnd()) return;
    it.value()(url);
}

void ProtocolHandlerRegistry::clearForTesting()
{
    m_handlers.clear();
}

} // namespace Corbomite
