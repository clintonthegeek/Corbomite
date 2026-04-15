// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include "corbomite/core/PaneLayout.h"

namespace Corbomite {

/// Persists Corbomite's per-vault workspace state into
/// `<vault>/.obsidian/workspace.json` following Obsidian's LayoutJson
/// schema (see `docs/obsidian-audit/domains/workspace.md §2`):
///
///   { main, left?, right?, floating?, active?, lastOpenFiles?,
///     'left-ribbon'?, … , _corbomite: {...} }
///
/// The editor split/tab tree is mirrored into `main` via `PaneLayout`.
/// Corbomite-specific state that Obsidian doesn't model (Qt window
/// geometry, sidebar widths, expanded folders in the file tree) lives
/// under the `_corbomite` unknown-key namespace. Unknown-key preservation
/// guarantees Obsidian won't drop that sub-object on round-trip.
///
/// Save is debounced (leading-edge timer) and composes the full
/// workspace.json bytes each flush.
class SessionManager : public QObject
{
    Q_OBJECT

public:
    explicit SessionManager(QObject *parent = nullptr);
    ~SessionManager() override;

    /// Absolute path to the vault's `.obsidian/workspace.json`.
    void setSessionPath(const QString &path);

    /// Load the current workspace.json if it exists. Subsequent accessors
    /// reflect the parsed content. Returns true if a file was found.
    bool load();

    /// Flush any pending changes right now (cancels the debounce).
    void saveNow();
    void scheduleSave();

    /// Suspend save-on-change while vault transitions are underway.
    void blockSaving();
    void unblockSaving();

    // --- Granular setters (each triggers scheduleSave) ---

    void saveWindowGeometry(const QByteArray &geometry, const QByteArray &state);
    void saveSidebarState(bool leftVisible, int leftWidth,
                          bool rightVisible, int rightWidth,
                          const QString &activePanel = QString());
    void saveExpandedFolders(const QStringList &folders);

    /// Replace the pane layout (main SplitNode). `activeLeafId` is written
    /// to workspace.json's root `active` field.
    void setPaneLayout(const PaneLayout &layout,
                       const QString &activeLeafId = QString());

    // --- Accessors (reflect loaded or in-memory state) ---

    QByteArray windowGeometry() const;
    QByteArray windowState() const;
    QJsonObject sidebarState() const;
    QStringList expandedFolders() const;
    const PaneLayout &paneLayout() const;
    QString activeLeafId() const;

    /// True once `load()` has been called successfully on a non-empty file.
    bool hasLoadedSession() const { return m_loaded; }

private:
    void doSave();

    QString m_sessionPath;
    /// Corbomite-only sub-object written under `_corbomite`.
    QJsonObject m_corbomiteTail;
    /// All unknown root-level keys preserved between load and save
    /// (Obsidian unknown-key invariant).
    QJsonObject m_unknownRoot;
    PaneLayout m_paneLayout;
    QString m_activeLeafId;
    QTimer m_saveTimer;
    int m_saveBlockCount = 0;
    bool m_loaded = false;
};

} // namespace Corbomite
