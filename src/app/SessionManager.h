// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QJsonObject>
#include <QTimer>

namespace Corbomite {

class SessionManager : public QObject {
    Q_OBJECT

public:
    explicit SessionManager(QObject *parent = nullptr);

    void setSessionPath(const QString &path);

    // Save
    void saveWindowGeometry(const QByteArray &geometry, const QByteArray &state);
    void saveSidebarState(bool leftVisible, int leftWidth, bool rightVisible, int rightWidth);
    void saveOpenTabs(const QJsonArray &tabs, int activeIndex);
    void saveExpandedFolders(const QStringList &folders);
    void scheduleSave();
    void saveNow();

    // Load
    QJsonObject load() const;

private:
    void doSave();

    QString m_sessionPath;
    QJsonObject m_data;
    QTimer m_saveTimer;
};

} // namespace Corbomite
