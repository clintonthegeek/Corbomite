// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

namespace Corbomite {

/// Persists Corbomite-native (non-Obsidian-schema) UI/session state for a
/// vault, per the workspace compat-boundary doctrine
/// (`docs/superpowers/specs/2026-08-17-workspace-compat-boundary.md`).
///
/// Tier-1 (Obsidian-schema `main`/`active`/`left`/`right`/`floating`/
/// `lastOpenFiles`/etc.) is NOT this class's concern any more — that lives
/// entirely in `.obsidian/workspace.json`, written/read by
/// `Workspace::writeWorkspaceJson`/`readWorkspaceJson`, which round-trips
/// the full payload including unknown-key passthrough. SessionManager owns
/// the other two tiers:
///
///   - **Tier 2 — vault-portable, Corbomite-native**: expanded folders,
///     left-ribbon layout, sidebar visibility (a boolean, not a pixel
///     width), and the vault's `vaultId` (minted once on first open).
///     Lives at `<vault>/.obsidian/corbomite/state.json` — inside the
///     vault, in a subfolder Obsidian never reads or writes, so it rides
///     whatever sync channel the vault already uses without contending
///     with Obsidian's own `workspace.json` churn.
///
///   - **Tier 3 — machine-local**: window geometry/state, sidebar pixel
///     widths, and `pluginSessionState` (ephemeral per-toolview UI restore
///     state — NOT the same thing as real plugin data, which
///     `PluginDataStore` already persists vault-portably under
///     `.obsidian/plugins/<id>/data.json`). Lives at
///     `<AppDataLocation>/vaults/<vaultId>/session.json` — never synced,
///     keyed by the vaultId minted into tier 2.
///
/// `<AppDataLocation>` resolves via `QStandardPaths::AppDataLocation`,
/// which is already `~/.local/share/corbomite[-dev]` because
/// `KAboutData::setApplicationData` stamps the process's
/// `QCoreApplication::applicationName` to `corbomite` or `corbomite-dev`
/// at startup (see `src/app/main.cpp`) — dev/release isolation falls out
/// for free, no `#ifdef CORBOMITE_DEV_BUILD` needed here.
///
/// Save is debounced (leading-edge timer) and flushes both tier files
/// together each time.
class SessionManager : public QObject
{
    Q_OBJECT

public:
    explicit SessionManager(QObject *parent = nullptr);
    ~SessionManager() override;

    /// Absolute path to the vault's root directory. Derives the tier-2 path
    /// (`<vault>/.obsidian/corbomite/state.json`) immediately; the tier-3
    /// path is derived lazily once the vaultId is known (minted on load()
    /// if not already present in tier 2).
    void setVaultPath(const QString &vaultRootPath);

    /// Load tier-2 and tier-3 state if present. Mints a `vaultId` into
    /// tier 2 (persisted immediately) if one wasn't already there — this is
    /// "first open" of this vault under this doctrine. Returns true if
    /// either tier file was found on disk (false only means "nothing to
    /// restore", not an error — a brand-new vault is expected to miss).
    bool load();

    /// Flush any pending changes right now (cancels the debounce).
    void saveNow();
    void scheduleSave();

    /// Suspend save-on-change while vault transitions are underway.
    void blockSaving();
    void unblockSaving();

    // --- Granular setters (each triggers scheduleSave) ---

    /// Tier 3 (machine-local).
    void saveWindowGeometry(const QByteArray &geometry, const QByteArray &state);
    /// `leftVisible`/`rightVisible` land in tier 2 (vault-portable —
    /// matches Obsidian's own precedent for "what workspace state means");
    /// `leftWidth`/`rightWidth` land in tier 3 (screen-relative, wrong to
    /// sync). `activePanel` is a content preference and lands in tier 2.
    void saveSidebarState(bool leftVisible, int leftWidth,
                          bool rightVisible, int rightWidth,
                          const QString &activePanel = QString());
    /// Tier 2.
    void saveExpandedFolders(const QStringList &folders);

    /// Tier 3. Store per-plugin *ephemeral UI* session state keyed by
    /// plugin id (tree-expand on a panel, scroll position — not real
    /// plugin settings/data, which belongs to `PluginDataStore`).
    void setPluginSessionState(const QString &pluginId, const QJsonObject &state);

    /// Tier 3. Per-file local data, keyed by Obsidian's own key format.
    ///
    /// Obsidian keeps this class of state in a flat localStorage map keyed
    /// `<type>-<vault-relative-path>`; canvas is the only current user
    /// (`localStorage["canvas-<path>"]`, `app/own/2028-n9.js:560-579`), whose
    /// single payload key is `readonly`. Keeping the key format identical
    /// means a Corbomite-written tier-3 file can be read against Obsidian's
    /// behaviour by inspection, and vice versa.
    ///
    /// An empty `data` REMOVES the key, mirroring Obsidian's
    /// `saveLocalStorage(key, null)` (`:569` and `:576`).
    void setFileLocalData(const QString &key, const QJsonObject &data);

    /// Tier 2. Replace the left-ribbon layout state. Pass `{}` to clear.
    void setLeftRibbonState(const QJsonObject &state);

    // --- Accessors (reflect loaded or in-memory state) ---

    QByteArray windowGeometry() const;
    QByteArray windowState() const;
    /// Merged view: `leftVisible`/`rightVisible`/`activePanel` from tier 2,
    /// `leftWidth`/`rightWidth` from tier 3 — same shape callers relied on
    /// pre-split.
    QJsonObject sidebarState() const;
    QStringList expandedFolders() const;
    QJsonObject pluginSessionState(const QString &pluginId) const;
    QJsonObject leftRibbonState() const;
    /// Tier 3. Empty object when absent.
    QJsonObject fileLocalData(const QString &key) const;

    /// The durable per-vault id minted into tier 2 on first open. Empty
    /// until `load()` has run at least once with a vault path set.
    QString vaultId() const;

    /// True once `load()` has been called with a vault path set (even if
    /// nothing was found on disk — a fresh vault still gets a minted
    /// vaultId and is considered "loaded").
    bool hasLoadedSession() const { return m_loaded; }

private:
    void doSave();
    QString tier3Dir() const;
    void ensureVaultId();
    QJsonObject buildTier2Json() const;

    QString m_vaultRootPath;
    QString m_tier2Path; // <vault>/.obsidian/corbomite/state.json
    QString m_tier3Path; // <AppData>/vaults/<vaultId>/session.json

    // --- Tier 2 (vault-portable) state ---
    QString m_vaultId;
    QStringList m_expandedFolders;
    QJsonObject m_leftRibbon;
    bool m_sidebarLeftVisible = true;
    bool m_sidebarRightVisible = false;
    QString m_sidebarActivePanel;

    // --- Tier 3 (machine-local) state ---
    QByteArray m_windowGeometry;
    QByteArray m_windowState;
    int m_sidebarLeftWidth = 200;
    int m_sidebarRightWidth = 200;
    QJsonObject m_pluginSessionStates; // pluginId -> state
    QJsonObject m_fileLocalData; // "canvas-<path>" -> state

    QTimer m_saveTimer;
    int m_saveBlockCount = 0;
    bool m_loaded = false;
};

} // namespace Corbomite
