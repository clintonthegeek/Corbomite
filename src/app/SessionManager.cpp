// SPDX-License-Identifier: GPL-3.0-or-later
#include "SessionManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

namespace Corbomite {

namespace {

constexpr auto kVaultId = "vaultId";
constexpr auto kExpandedFolders = "expandedFolders";
constexpr auto kLeftRibbon = "leftRibbon";
constexpr auto kSidebar = "sidebar";
constexpr auto kSidebarLeftVisible = "leftVisible";
constexpr auto kSidebarRightVisible = "rightVisible";
constexpr auto kSidebarActivePanel = "activePanel";

constexpr auto kWindowGeometry = "windowGeometry";
constexpr auto kWindowState = "windowState";
constexpr auto kSidebarLeftWidth = "leftWidth";
constexpr auto kSidebarRightWidth = "rightWidth";
constexpr auto kPlugins = "plugins";

bool writeJsonAtomically(const QString &path, const QJsonObject &obj)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    return file.commit();
}

QJsonObject readJson(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    const auto doc = QJsonDocument::fromJson(f.readAll());
    return doc.isObject() ? doc.object() : QJsonObject{};
}

} // namespace

SessionManager::SessionManager(QObject *parent)
    : QObject(parent)
{
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(2000);
    connect(&m_saveTimer, &QTimer::timeout, this, &SessionManager::doSave);
}

SessionManager::~SessionManager() = default;

void SessionManager::setVaultPath(const QString &vaultRootPath)
{
    m_vaultRootPath = vaultRootPath;
    m_tier2Path = vaultRootPath + QStringLiteral("/.obsidian/corbomite/state.json");
    // Tier 3 path depends on vaultId, resolved in load()/ensureVaultId().
    m_tier3Path.clear();
}

QString SessionManager::tier3Dir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
         + QStringLiteral("/vaults/") + m_vaultId;
}

QJsonObject SessionManager::buildTier2Json() const
{
    QJsonObject o;
    o.insert(QLatin1String(kVaultId), m_vaultId);
    if (!m_expandedFolders.isEmpty()) {
        QJsonArray arr;
        for (const auto &f : m_expandedFolders) arr.append(f);
        o.insert(QLatin1String(kExpandedFolders), arr);
    }
    if (!m_leftRibbon.isEmpty())
        o.insert(QLatin1String(kLeftRibbon), m_leftRibbon);
    QJsonObject sidebar;
    sidebar.insert(QLatin1String(kSidebarLeftVisible), m_sidebarLeftVisible);
    sidebar.insert(QLatin1String(kSidebarRightVisible), m_sidebarRightVisible);
    if (!m_sidebarActivePanel.isEmpty())
        sidebar.insert(QLatin1String(kSidebarActivePanel), m_sidebarActivePanel);
    o.insert(QLatin1String(kSidebar), sidebar);
    return o;
}

void SessionManager::ensureVaultId()
{
    if (m_vaultId.isEmpty()) {
        m_vaultId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        // Persist immediately (not debounced) — tier 3's directory name
        // depends on this, and other in-process consumers may read
        // vaultId() right after load() returns.
        writeJsonAtomically(m_tier2Path, buildTier2Json());
    }
    m_tier3Path = tier3Dir() + QStringLiteral("/session.json");
}

bool SessionManager::load()
{
    m_loaded = false;
    m_vaultId.clear();
    m_expandedFolders.clear();
    m_leftRibbon = {};
    m_sidebarLeftVisible = true;
    m_sidebarRightVisible = false;
    m_sidebarActivePanel.clear();
    m_windowGeometry.clear();
    m_windowState.clear();
    m_sidebarLeftWidth = 200;
    m_sidebarRightWidth = 200;
    m_pluginSessionStates = {};

    if (m_tier2Path.isEmpty()) return false;

    bool foundAny = false;

    // --- Tier 2 ---
    const QJsonObject tier2 = readJson(m_tier2Path);
    if (!tier2.isEmpty()) {
        foundAny = true;
        m_vaultId = tier2.value(QLatin1String(kVaultId)).toString();
        for (const auto &v : tier2.value(QLatin1String(kExpandedFolders)).toArray())
            if (v.isString()) m_expandedFolders.append(v.toString());
        if (tier2.value(QLatin1String(kLeftRibbon)).isObject())
            m_leftRibbon = tier2.value(QLatin1String(kLeftRibbon)).toObject();
        const auto sidebar = tier2.value(QLatin1String(kSidebar)).toObject();
        if (!sidebar.isEmpty()) {
            m_sidebarLeftVisible = sidebar.value(QLatin1String(kSidebarLeftVisible)).toBool(true);
            m_sidebarRightVisible = sidebar.value(QLatin1String(kSidebarRightVisible)).toBool(false);
            m_sidebarActivePanel = sidebar.value(QLatin1String(kSidebarActivePanel)).toString();
        }
    }

    // Mint vaultId if this is a first-ever open (persists tier 2
    // immediately) and derive the tier-3 path from it.
    ensureVaultId();

    // --- Tier 3 ---
    const QJsonObject tier3 = readJson(m_tier3Path);
    if (!tier3.isEmpty()) {
        foundAny = true;
        m_windowGeometry = QByteArray::fromBase64(
            tier3.value(QLatin1String(kWindowGeometry)).toString().toLatin1());
        m_windowState = QByteArray::fromBase64(
            tier3.value(QLatin1String(kWindowState)).toString().toLatin1());
        const auto sidebar = tier3.value(QLatin1String(kSidebar)).toObject();
        if (!sidebar.isEmpty()) {
            m_sidebarLeftWidth = sidebar.value(QLatin1String(kSidebarLeftWidth)).toInt(200);
            m_sidebarRightWidth = sidebar.value(QLatin1String(kSidebarRightWidth)).toInt(200);
        }
        if (tier3.value(QLatin1String(kPlugins)).isObject())
            m_pluginSessionStates = tier3.value(QLatin1String(kPlugins)).toObject();
    }

    m_loaded = true;
    return foundAny;
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

void SessionManager::saveWindowGeometry(const QByteArray &geometry, const QByteArray &state)
{
    m_windowGeometry = geometry;
    m_windowState = state;
    scheduleSave();
}

void SessionManager::saveSidebarState(bool leftVisible, int leftWidth,
                                      bool rightVisible, int rightWidth,
                                      const QString &activePanel)
{
    m_sidebarLeftVisible = leftVisible;
    m_sidebarRightVisible = rightVisible;
    if (!activePanel.isEmpty()) m_sidebarActivePanel = activePanel;
    m_sidebarLeftWidth = leftWidth;
    m_sidebarRightWidth = rightWidth;
    scheduleSave();
}

void SessionManager::saveExpandedFolders(const QStringList &folders)
{
    m_expandedFolders = folders;
    scheduleSave();
}

void SessionManager::setPluginSessionState(const QString &pluginId, const QJsonObject &state)
{
    if (pluginId.isEmpty()) return;
    if (state.isEmpty()) {
        m_pluginSessionStates.remove(pluginId);
    } else {
        m_pluginSessionStates.insert(pluginId, state);
    }
    scheduleSave();
}

void SessionManager::setLeftRibbonState(const QJsonObject &state)
{
    m_leftRibbon = state;
    scheduleSave();
}

// --- Accessors ---

QByteArray SessionManager::windowGeometry() const { return m_windowGeometry; }
QByteArray SessionManager::windowState() const { return m_windowState; }

QJsonObject SessionManager::sidebarState() const
{
    QJsonObject sidebar;
    sidebar.insert(QLatin1String(kSidebarLeftVisible), m_sidebarLeftVisible);
    sidebar.insert(QLatin1String(kSidebarLeftWidth), m_sidebarLeftWidth);
    sidebar.insert(QLatin1String(kSidebarRightVisible), m_sidebarRightVisible);
    sidebar.insert(QLatin1String(kSidebarRightWidth), m_sidebarRightWidth);
    if (!m_sidebarActivePanel.isEmpty())
        sidebar.insert(QLatin1String(kSidebarActivePanel), m_sidebarActivePanel);
    return sidebar;
}

QStringList SessionManager::expandedFolders() const { return m_expandedFolders; }

QJsonObject SessionManager::pluginSessionState(const QString &pluginId) const
{
    if (pluginId.isEmpty()) return {};
    return m_pluginSessionStates.value(pluginId).toObject();
}

QJsonObject SessionManager::leftRibbonState() const { return m_leftRibbon; }

QString SessionManager::vaultId() const { return m_vaultId; }

// --- Save ---

void SessionManager::doSave()
{
    if (m_tier2Path.isEmpty()) return;
    ensureVaultId(); // no-op if already minted; guarantees m_tier3Path is set

    writeJsonAtomically(m_tier2Path, buildTier2Json());

    QJsonObject tier3;
    if (!m_windowGeometry.isEmpty())
        tier3.insert(QLatin1String(kWindowGeometry),
                     QString::fromLatin1(m_windowGeometry.toBase64()));
    if (!m_windowState.isEmpty())
        tier3.insert(QLatin1String(kWindowState),
                     QString::fromLatin1(m_windowState.toBase64()));
    QJsonObject sidebar3;
    sidebar3.insert(QLatin1String(kSidebarLeftWidth), m_sidebarLeftWidth);
    sidebar3.insert(QLatin1String(kSidebarRightWidth), m_sidebarRightWidth);
    tier3.insert(QLatin1String(kSidebar), sidebar3);
    if (!m_pluginSessionStates.isEmpty())
        tier3.insert(QLatin1String(kPlugins), m_pluginSessionStates);
    writeJsonAtomically(m_tier3Path, tier3);
}

} // namespace Corbomite
