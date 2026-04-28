// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <functional>

#include <QIcon>
#include <QString>

namespace Corbomite {

/// Abstract interface for the host's ribbon toolbar. The concrete
/// implementation is `RibbonToolBar` in `src/app/`; this interface lives
/// in `libs/core` so plugin proxies (and `PluginContext`) can register
/// items without taking a hard dependency on the application binary.
class RibbonHandle
{
public:
    virtual ~RibbonHandle() = default;

    /// Add a ribbon icon. Returns `id` on success, empty string on
    /// failure (id collision; matches RibbonToolBar quirk).
    virtual QString addRibbonIcon(const QString &id,
                                    const QIcon &icon,
                                    const QString &title,
                                    std::function<void()> onActivated) = 0;

    /// Remove a ribbon icon. Returns true if the icon was registered.
    virtual bool removeRibbonIcon(const QString &id) = 0;
};

} // namespace Corbomite
