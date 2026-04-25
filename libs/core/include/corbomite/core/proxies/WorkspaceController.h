// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <Qt>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>

namespace Corbomite {

class Workspace;
class WorkspaceLeaf;

/// Workspace facade for plugins with the "workspace" permission.
///
/// Thin forward over the host's Workspace. Opens by vault-relative path,
/// looks up leaves by their stable string id, and returns success/failure
/// booleans (leaf/window pointers are intentionally not exposed to the
/// plugin surface — plugin authors operate on leaf ids + the open-by-path
/// flow).
///
/// Inherits QObject so plugins can react to active-leaf transitions via
/// `activeFileChanged(QString relativePath)`. The proxy resolves the
/// active leaf to a vault-relative path, so plugins never see raw
/// WorkspaceLeaf*.
class WorkspaceController : public QObject
{
    Q_OBJECT
public:
    explicit WorkspaceController(Workspace *workspace, QObject *parent = nullptr);
    ~WorkspaceController() override;

    /// Open (or activate, if already open) `relativePath` in the active
    /// tab group. Uses the Workspace's ViewRegistry to pick a view type
    /// based on the file extension. Returns true if a leaf was opened
    /// or activated, false otherwise.
    bool openFile(const QString &relativePath);

    /// Id of the currently active leaf, or an empty string if none.
    QString activeLeafId() const;

    /// Vault-relative path of the file shown in the active leaf, or an
    /// empty string if the active leaf hosts no file (or there is no
    /// active leaf).
    QString activeFilePath() const;

    /// Split the leaf with the given id. Returns true on success.
    bool splitLeaf(const QString &leafId, Qt::Orientation orientation);

    /// Close the leaf with the given id. Returns true on success.
    bool closeLeaf(const QString &leafId);

    /// Move the leaf with the given id into a popout window. Returns
    /// true on success.
    bool popoutLeaf(const QString &leafId);

    /// Move the cursor of the active leaf's view to `line` (1-based). Returns
    /// true if the active leaf hosts an editable file view that could apply
    /// the move. Used by the Outline plugin's scroll-to-heading flow and any
    /// plugin that needs to drive the editor caret.
    bool goToLine(int line);

    /// Request the host reveal the dock panel for plugin `slug`. The host
    /// resolves `slug` → tool view id `<slug>_panel` and raises it. Used
    /// by the `<plugin>:open` commands from Cluster R Task 3.1.
    void revealDockView(const QString &slug);

    // --- Cluster Y Phase 7.4 — Obsidian-shape additions ---

    /// Stable leaf ids for every leaf currently hosting a view of
    /// `viewType`. Includes deferred leaves whose cached state carries
    /// `viewType` (Obsidian counts those as "of type"). Order matches the
    /// Workspace's insertion-ordered iteration.
    QStringList getLeavesOfType(const QString &viewType) const;

    /// Invoke `cb` once per leaf in the workspace, passing the leaf id.
    /// DFS-order walk; mirrors `Workspace.iterateAllLeaves(cb)` from the
    /// Obsidian plugin API. No-op if the workspace is null or `cb` empty.
    void iterateAllLeaves(std::function<void(const QString &leafId)> cb) const;

    /// Leaf id of the active leaf if its view type matches `viewType`,
    /// else empty. Used by plugins that need to interrogate the active
    /// view only when it is one of their own (e.g. `BasesView`).
    QString getActiveViewOfType(const QString &viewType) const;

    /// Open `linktext` in a leaf chosen per `mode`. String-mode argument
    /// for plugin parity:
    ///   - "split"  → `Workspace::LeafMode::Split`
    ///   - "tab"    → `Workspace::LeafMode::Tab`
    ///   - "window" → `Workspace::LeafMode::Window`
    ///   - "same"   → `Workspace::LeafMode::Same`
    /// Direction defaults to horizontal when `mode == "split"`. Delegates
    /// to `Workspace::openLinkText`. Returns true on success.
    bool openLinkText(const QString &linktext,
                       const QString &source,
                       const QString &mode,
                       const QJsonObject &opts = {});

    /// Obsidian-shape leaf factory exposed to plugins. Same string-mode
    /// + direction encoding as `openLinkText`. Returns the new leaf's id,
    /// or empty on failure.
    QString getLeaf(const QString &mode,
                     const QString &direction = QStringLiteral("horizontal"));

Q_SIGNALS:
    /// Emitted when the active leaf changes — `relativePath` is the
    /// vault-relative path of the file in the new active leaf, or
    /// empty if the new active leaf is fileless.
    void activeFileChanged(const QString &relativePath);

private:
    Workspace *m_workspace;
};

} // namespace Corbomite
