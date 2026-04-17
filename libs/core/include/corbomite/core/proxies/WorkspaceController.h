// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <Qt>
#include <QObject>
#include <QString>

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

Q_SIGNALS:
    /// Emitted when the active leaf changes — `relativePath` is the
    /// vault-relative path of the file in the new active leaf, or
    /// empty if the new active leaf is fileless.
    void activeFileChanged(const QString &relativePath);

private:
    Workspace *m_workspace;
};

} // namespace Corbomite
