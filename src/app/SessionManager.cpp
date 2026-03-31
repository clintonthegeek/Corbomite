// SPDX-License-Identifier: GPL-3.0-or-later
#include "SessionManager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDir>

namespace Corbomite {

SessionManager::SessionManager(QObject *parent)
    : QObject(parent)
{
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(2000);
    connect(&m_saveTimer, &QTimer::timeout, this, &SessionManager::doSave);
}

void SessionManager::setSessionPath(const QString &path)
{
    m_sessionPath = path;
}

void SessionManager::saveWindowGeometry(const QByteArray &geometry, const QByteArray &state)
{
    m_data[QStringLiteral("windowGeometry")] = QString::fromLatin1(geometry.toBase64());
    m_data[QStringLiteral("windowState")] = QString::fromLatin1(state.toBase64());
    scheduleSave();
}

void SessionManager::saveSidebarState(bool leftVisible, int leftWidth, bool rightVisible, int rightWidth)
{
    QJsonObject sidebar;
    sidebar[QStringLiteral("leftVisible")] = leftVisible;
    sidebar[QStringLiteral("leftWidth")] = leftWidth;
    sidebar[QStringLiteral("rightVisible")] = rightVisible;
    sidebar[QStringLiteral("rightWidth")] = rightWidth;
    m_data[QStringLiteral("sidebar")] = sidebar;
    scheduleSave();
}

void SessionManager::saveOpenTabs(const QJsonArray &tabs, int activeIndex)
{
    m_data[QStringLiteral("tabs")] = tabs;
    m_data[QStringLiteral("activeTabIndex")] = activeIndex;
    scheduleSave();
}

void SessionManager::saveExpandedFolders(const QStringList &folders)
{
    m_data[QStringLiteral("expandedFolders")] = QJsonArray::fromStringList(folders);
    scheduleSave();
}

void SessionManager::scheduleSave()
{
    m_saveTimer.start();
}

void SessionManager::saveNow()
{
    m_saveTimer.stop();
    doSave();
}

QJsonObject SessionManager::load() const
{
    QFile file(m_sessionPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    auto doc = QJsonDocument::fromJson(file.readAll());
    return doc.isObject() ? doc.object() : QJsonObject();
}

void SessionManager::doSave()
{
    if (m_sessionPath.isEmpty()) return;

    QDir().mkpath(QFileInfo(m_sessionPath).absolutePath());
    QFile file(m_sessionPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(m_data).toJson(QJsonDocument::Indented));
    }
}

} // namespace Corbomite
