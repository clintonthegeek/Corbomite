// SPDX-License-Identifier: GPL-3.0-or-later
#include "SessionManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

namespace Corbomite {

namespace {

constexpr auto kMain = "main";
constexpr auto kActive = "active";
constexpr auto kCorbomite = "_corbomite";

constexpr auto kWindowGeometry = "windowGeometry";
constexpr auto kWindowState = "windowState";
constexpr auto kSidebar = "sidebar";
constexpr auto kSidebarLeftVisible = "leftVisible";
constexpr auto kSidebarLeftWidth = "leftWidth";
constexpr auto kSidebarRightVisible = "rightVisible";
constexpr auto kSidebarRightWidth = "rightWidth";
constexpr auto kSidebarActivePanel = "activePanel";
constexpr auto kExpandedFolders = "expandedFolders";

} // namespace

SessionManager::SessionManager(QObject *parent)
    : QObject(parent)
{
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(2000);
    connect(&m_saveTimer, &QTimer::timeout, this, &SessionManager::doSave);
}

SessionManager::~SessionManager() = default;

void SessionManager::setSessionPath(const QString &path)
{
    m_sessionPath = path;
}

bool SessionManager::load()
{
    m_loaded = false;
    m_corbomiteTail = {};
    m_unknownRoot = {};
    m_mainJson = {};
    m_activeLeafId.clear();

    if (m_sessionPath.isEmpty()) return false;

    QFile f(m_sessionPath);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return false;

    const QJsonObject root = doc.object();

    if (root.contains(QLatin1String(kMain))
            && root.value(QLatin1String(kMain)).isObject()) {
        m_mainJson = root.value(QLatin1String(kMain)).toObject();
    }

    m_activeLeafId = root.value(QLatin1String(kActive)).toString();

    // Corbomite-specific tail.
    if (root.contains(QLatin1String(kCorbomite))) {
        m_corbomiteTail = root.value(QLatin1String(kCorbomite)).toObject();
    }

    // Everything else (Obsidian's left/right/floating/lastOpenFiles/ribbon/
    // etc.) goes into m_unknownRoot to round-trip unchanged on save.
    for (auto it = root.begin(); it != root.end(); ++it) {
        if (it.key() == QLatin1String(kMain)
                || it.key() == QLatin1String(kActive)
                || it.key() == QLatin1String(kCorbomite)) continue;
        m_unknownRoot.insert(it.key(), it.value());
    }

    m_loaded = true;
    return true;
}

void SessionManager::saveNow()
{
    m_saveTimer.stop();
    doSave();
}

void SessionManager::scheduleSave()
{
    if (m_saveBlockCount > 0) return;
    m_saveTimer.start();
}

void SessionManager::blockSaving()
{
    ++m_saveBlockCount;
    m_saveTimer.stop();
}

void SessionManager::unblockSaving()
{
    if (m_saveBlockCount > 0) --m_saveBlockCount;
}

// --- Granular setters ---

void SessionManager::saveWindowGeometry(const QByteArray &geometry,
                                        const QByteArray &state)
{
    m_corbomiteTail.insert(QLatin1String(kWindowGeometry),
                           QString::fromLatin1(geometry.toBase64()));
    m_corbomiteTail.insert(QLatin1String(kWindowState),
                           QString::fromLatin1(state.toBase64()));
    scheduleSave();
}

void SessionManager::saveSidebarState(bool leftVisible, int leftWidth,
                                      bool rightVisible, int rightWidth,
                                      const QString &activePanel)
{
    QJsonObject sidebar;
    sidebar.insert(QLatin1String(kSidebarLeftVisible), leftVisible);
    sidebar.insert(QLatin1String(kSidebarLeftWidth), leftWidth);
    sidebar.insert(QLatin1String(kSidebarRightVisible), rightVisible);
    sidebar.insert(QLatin1String(kSidebarRightWidth), rightWidth);
    if (!activePanel.isEmpty()) {
        sidebar.insert(QLatin1String(kSidebarActivePanel), activePanel);
    }
    m_corbomiteTail.insert(QLatin1String(kSidebar), sidebar);
    scheduleSave();
}

void SessionManager::saveExpandedFolders(const QStringList &folders)
{
    QJsonArray arr;
    for (const auto &f : folders) arr.append(f);
    m_corbomiteTail.insert(QLatin1String(kExpandedFolders), arr);
    scheduleSave();
}

void SessionManager::setWorkspaceLayout(const QJsonObject &mainJson,
                                        const QString &activeLeafId)
{
    m_mainJson = mainJson;
    m_activeLeafId = activeLeafId;
    scheduleSave();
}

// --- Accessors ---

QByteArray SessionManager::windowGeometry() const
{
    return QByteArray::fromBase64(
        m_corbomiteTail.value(QLatin1String(kWindowGeometry)).toString().toLatin1());
}

QByteArray SessionManager::windowState() const
{
    return QByteArray::fromBase64(
        m_corbomiteTail.value(QLatin1String(kWindowState)).toString().toLatin1());
}

QJsonObject SessionManager::sidebarState() const
{
    return m_corbomiteTail.value(QLatin1String(kSidebar)).toObject();
}

QStringList SessionManager::expandedFolders() const
{
    QStringList out;
    const auto arr = m_corbomiteTail.value(QLatin1String(kExpandedFolders)).toArray();
    for (const auto &v : arr) if (v.isString()) out.append(v.toString());
    return out;
}

QJsonObject SessionManager::workspaceLayout() const { return m_mainJson; }
QString SessionManager::activeLeafId() const { return m_activeLeafId; }

// --- Save ---

void SessionManager::doSave()
{
    if (m_sessionPath.isEmpty()) return;

    // Compose: unknownRoot (Obsidian keys we pass through) + main + active
    // + _corbomite (our namespaced state).
    QJsonObject root = m_unknownRoot;
    root.insert(QLatin1String(kMain), m_mainJson);
    if (!m_activeLeafId.isEmpty()) {
        root.insert(QLatin1String(kActive), m_activeLeafId);
    }
    if (!m_corbomiteTail.isEmpty()) {
        root.insert(QLatin1String(kCorbomite), m_corbomiteTail);
    }

    QDir().mkpath(QFileInfo(m_sessionPath).absolutePath());
    QSaveFile file(m_sessionPath);
    if (!file.open(QIODevice::WriteOnly)) return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.commit();
}

} // namespace Corbomite
