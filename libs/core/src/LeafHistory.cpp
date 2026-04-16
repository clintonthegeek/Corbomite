// libs/core/src/LeafHistory.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/LeafHistory.h"

#include <QJsonArray>

namespace Corbomite {

QJsonObject LeafHistoryEntry::serialize() const
{
    return {{QStringLiteral("title"), title},
            {QStringLiteral("icon"), icon},
            {QStringLiteral("state"), state},
            {QStringLiteral("eState"), eState}};
}

LeafHistoryEntry LeafHistoryEntry::deserialize(const QJsonObject &json)
{
    return {json[QStringLiteral("title")].toString(),
            json[QStringLiteral("icon")].toString(),
            json[QStringLiteral("state")].toObject(),
            json[QStringLiteral("eState")].toObject()};
}

void LeafHistory::push(const LeafHistoryEntry &current)
{
    m_back.append(current);
    if (m_back.size() > Cap)
        m_back.removeFirst();
    m_forward.clear();
}

LeafHistoryEntry LeafHistory::goBack(const LeafHistoryEntry &current)
{
    if (m_back.isEmpty())
        return {};
    auto entry = m_back.takeLast();
    m_forward.append(current);
    if (m_forward.size() > Cap)
        m_forward.removeFirst();
    return entry;
}

LeafHistoryEntry LeafHistory::goForward(const LeafHistoryEntry &current)
{
    if (m_forward.isEmpty())
        return {};
    auto entry = m_forward.takeLast();
    m_back.append(current);
    if (m_back.size() > Cap)
        m_back.removeFirst();
    return entry;
}

bool LeafHistory::canGoBack() const { return !m_back.isEmpty(); }
bool LeafHistory::canGoForward() const { return !m_forward.isEmpty(); }

QJsonObject LeafHistory::serialize() const
{
    QJsonArray back;
    for (const auto &e : m_back)
        back.append(e.serialize());
    QJsonArray fwd;
    for (const auto &e : m_forward)
        fwd.append(e.serialize());
    return {{QStringLiteral("backHistory"), back},
            {QStringLiteral("forwardHistory"), fwd}};
}

LeafHistory LeafHistory::deserialize(const QJsonObject &json)
{
    LeafHistory h;
    for (const auto &v : json[QStringLiteral("backHistory")].toArray())
        h.m_back.append(LeafHistoryEntry::deserialize(v.toObject()));
    for (const auto &v : json[QStringLiteral("forwardHistory")].toArray())
        h.m_forward.append(LeafHistoryEntry::deserialize(v.toObject()));
    return h;
}

} // namespace Corbomite
