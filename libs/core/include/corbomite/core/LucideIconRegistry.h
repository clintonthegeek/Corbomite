// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QHash>
#include <QIcon>
#include <QString>

namespace Corbomite {

/// Singleton registry for icons addressable by `lucide-*`-style names.
/// Today the registry is **not pre-populated** with the bundled Lucide
/// SVG set — that's a Cluster B follow-up. Plugins (and the host) can
/// `addIcon(name, svg)` ad-hoc to register icons; `get(name)` returns an
/// empty `QIcon` for unregistered names.
class LucideIconRegistry
{
public:
    static LucideIconRegistry &instance();

    /// Register an SVG-encoded icon under `name`. The registry copies
    /// the SVG bytes; subsequent calls with the same `name` overwrite
    /// the previous registration.
    void addIcon(const QString &name, const QByteArray &svg);

    /// Remove an icon registration. Safe to call on names that were
    /// never registered.
    void removeIcon(const QString &name);

    /// Returns a `QIcon` for `name`, or a null `QIcon` if `name` is
    /// not registered.
    QIcon get(const QString &name) const;

    /// True if a non-null icon is registered under `name`.
    bool hasIcon(const QString &name) const;

    int iconCount() const { return m_icons.size(); }

    /// Test-only: clear the singleton between test runs.
    void clearForTesting();

private:
    LucideIconRegistry() = default;
    QHash<QString, QIcon> m_icons;
};

} // namespace Corbomite
