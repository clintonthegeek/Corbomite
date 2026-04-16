// libs/core/include/corbomite/core/LeafHistory.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

namespace Corbomite {

struct LeafHistoryEntry {
    QString title;
    QString icon;
    QJsonObject state;
    QJsonObject eState;

    bool isValid() const { return !title.isEmpty(); }
    QJsonObject serialize() const;
    static LeafHistoryEntry deserialize(const QJsonObject &json);
};

class LeafHistory
{
public:
    static constexpr int Cap = 20;

    void push(const LeafHistoryEntry &current);
    LeafHistoryEntry goBack(const LeafHistoryEntry &current);
    LeafHistoryEntry goForward(const LeafHistoryEntry &current);

    bool canGoBack() const;
    bool canGoForward() const;

    QJsonObject serialize() const;
    static LeafHistory deserialize(const QJsonObject &json);

private:
    QVector<LeafHistoryEntry> m_back;
    QVector<LeafHistoryEntry> m_forward;
};

} // namespace Corbomite
