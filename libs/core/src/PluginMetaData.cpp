// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/PluginMetaData.h"

#include <QJsonArray>
#include <QJsonValue>

namespace Corbomite {

QStringList PluginMetaData::permissions() const
{
    QStringList out;
    const auto val = m_base.rawData().value(QStringLiteral("X-Corbomite-Permissions"));
    if (val.isArray()) {
        const auto arr = val.toArray();
        out.reserve(arr.size());
        for (const auto &v : arr) {
            out << v.toString();
        }
    }
    return out;
}

bool PluginMetaData::trusted() const
{
    if (m_origin == Origin::User) return false;
    return m_base.rawData()
        .value(QStringLiteral("X-Corbomite-Trusted"))
        .toBool(false);
}

QVersionNumber PluginMetaData::minAppVersion() const
{
    const QString s = m_base.rawData()
        .value(QStringLiteral("X-Corbomite-MinVersion"))
        .toString();
    if (s.isEmpty()) return {};
    return QVersionNumber::fromString(s);
}

int PluginMetaData::apiLevel() const
{
    // Absent key => level 1 (today's API). We use toInt(1) so any
    // non-integer value also falls back cleanly rather than breaking
    // discovery.
    return m_base.rawData()
        .value(QStringLiteral("X-Corbomite-ApiLevel"))
        .toInt(1);
}

QString PluginMetaData::obsidianId() const
{
    return m_base.rawData()
        .value(QStringLiteral("X-Obsidian-Id"))
        .toString();
}

} // namespace Corbomite
