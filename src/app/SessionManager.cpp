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
constexpr auto kLeftRibbon = "left-ribbon";

constexpr auto kWindowGeometry = "windowGeometry";
constexpr auto kWindowState = "windowState";
constexpr auto kSidebar = "sidebar";
constexpr auto kSidebarLeftVisible = "leftVisible";
constexpr auto kSidebarLeftWidth = "leftWidth";
constexpr auto kSidebarRightVisible = "rightVisible";
constexpr auto kSidebarRightWidth = "rightWidth";
constexpr auto kSidebarActivePanel = "activePanel";
constexpr auto kExpandedFolders = "expandedFolders";
constexpr auto kPlugins = "plugins";

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
    m_leftRibbon = {};
    m_activeLeafId.clear();
    m_sidebarDirty = false;

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

    if (root.contains(QLatin1String(kLeftRibbon))
            && root.value(QLatin1String(kLeftRibbon)).isObject()) {
        m_leftRibbon = root.value(QLatin1String(kLeftRibbon)).toObject();
    }

    // Everything else (Obsidian's left/right/floating/lastOpenFiles/ribbon/
    // etc.) goes into m_unknownRoot to round-trip unchanged on save.
    for (auto it = root.begin(); it != root.end(); ++it) {
        if (it.key() == QLatin1String(kMain)
                || it.key() == QLatin1String(kActive)
                || it.key() == QLatin1String(kCorbomite)
                || it.key() == QLatin1String(kLeftRibbon)) continue;
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
    // Detect user-driven divergence from the on-disk sidebar so doSave
    // knows whether to drop the passed-through Obsidian `left`/`right`
    // sub-trees. MainWindow::saveSessionState calls this on every flush
    // (including session-restore replays), so identity must be the
    // gating signal — not call count.
    const auto previous = m_corbomiteTail.value(QLatin1String(kSidebar)).toObject();
    if (sidebar != previous)
        m_sidebarDirty = true;
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

void SessionManager::setPluginSessionState(const QString &pluginId,
                                           const QJsonObject &state)
{
    if (pluginId.isEmpty()) return;
    QJsonObject plugins = m_corbomiteTail.value(QLatin1String(kPlugins)).toObject();
    if (state.isEmpty()) {
        plugins.remove(pluginId);
    } else {
        plugins.insert(pluginId, state);
    }
    if (plugins.isEmpty()) {
        m_corbomiteTail.remove(QLatin1String(kPlugins));
    } else {
        m_corbomiteTail.insert(QLatin1String(kPlugins), plugins);
    }
    scheduleSave();
}

void SessionManager::setLeftRibbonState(const QJsonObject &state)
{
    m_leftRibbon = state;
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

QJsonObject SessionManager::pluginSessionState(const QString &pluginId) const
{
    if (pluginId.isEmpty()) return {};
    return m_corbomiteTail.value(QLatin1String(kPlugins)).toObject()
        .value(pluginId).toObject();
}

QJsonObject SessionManager::leftRibbonState() const { return m_leftRibbon; }
QJsonObject SessionManager::workspaceLayout() const { return m_mainJson; }
QString SessionManager::activeLeafId() const { return m_activeLeafId; }

QStringList SessionManager::lastOpenFiles() const
{
    QStringList files;
    const auto arr = m_unknownRoot.value(QStringLiteral("lastOpenFiles")).toArray();
    files.reserve(arr.size());
    for (const auto &v : arr)
        files.append(v.toString());
    return files;
}

// --- Save ---

void SessionManager::doSave()
{
    if (m_sessionPath.isEmpty()) return;

    // Compose: unknownRoot (Obsidian keys we pass through) + main + active
    // + _corbomite (our namespaced state).
    QJsonObject root = m_unknownRoot;
    // Obsidian's `left`/`right` sub-trees encode the sidedock split tree.
    // Corbomite has no live model of that tree (the `WorkspaceSidedock`
    // shells return nullptr; the real sidebar is `CorbomiteMDI::Sidebar`,
    // owned outside the Workspace). While the user hasn't touched
    // Corbomite's sidebar, we round-trip whatever Obsidian last wrote.
    // Once they have, that subtree is stale and would freeze Obsidian's
    // sidedock at an arbitrary past state on the next Obsidian-side
    // session — drop it so Obsidian rebuilds from defaults.
    if (m_sidebarDirty) {
        root.remove(QStringLiteral("left"));
        root.remove(QStringLiteral("right"));
    }
    root.insert(QLatin1String(kMain), m_mainJson);
    if (!m_activeLeafId.isEmpty()) {
        root.insert(QLatin1String(kActive), m_activeLeafId);
    }
    if (!m_corbomiteTail.isEmpty()) {
        root.insert(QLatin1String(kCorbomite), m_corbomiteTail);
    }
    if (!m_leftRibbon.isEmpty()) {
        root.insert(QLatin1String(kLeftRibbon), m_leftRibbon);
    }

    QDir().mkpath(QFileInfo(m_sessionPath).absolutePath());
    QSaveFile file(m_sessionPath);
    if (!file.open(QIODevice::WriteOnly)) return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.commit();
}

} // namespace Corbomite
