// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <KPluginMetaData>
#include <QStringList>
#include <QVersionNumber>

namespace Corbomite {

/// Thin wrapper over KPluginMetaData exposing Corbomite-specific JSON keys
/// (X-Corbomite-Permissions, X-Corbomite-Trusted, X-Corbomite-MinVersion)
/// plus an Origin field set by PluginManager during discovery.
///
/// Origin is load-bearing: a plugin loaded from the user install path can
/// not be trusted regardless of what its JSON declares — trusted() returns
/// false when origin == User. See spec §4.
class PluginMetaData
{
public:
    enum class Origin { Unknown, System, User };

    PluginMetaData() = default;
    explicit PluginMetaData(const KPluginMetaData &base) : m_base(base) {}

    const KPluginMetaData &base() const { return m_base; }

    /// X-Corbomite-Permissions — declared capability tokens. Empty if absent.
    QStringList permissions() const;

    /// X-Corbomite-Trusted, normalised by origin: a User-origin plugin is
    /// never trusted regardless of JSON claim. Defaults to false.
    bool trusted() const;

    /// X-Corbomite-MinVersion. Returns null QVersionNumber if absent or unparsable.
    QVersionNumber minAppVersion() const;

    /// X-Corbomite-ApiLevel. Integer ABI-break marker. Defaults to 1 when
    /// the key is absent — the plugin is implicitly targeting today's API.
    /// The host accepts plugins declaring a level <= CORBOMITE_PLUGIN_API_LEVEL.
    int apiLevel() const;

    void setOrigin(Origin o) { m_origin = o; }
    Origin origin() const { return m_origin; }

private:
    KPluginMetaData m_base;
    Origin m_origin = Origin::Unknown;
};

} // namespace Corbomite
