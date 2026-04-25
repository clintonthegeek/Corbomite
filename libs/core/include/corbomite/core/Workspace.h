// libs/core/include/corbomite/core/Workspace.h
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QVector>

#include "corbomite/core/LeafHistory.h"

namespace Corbomite {

class ViewRegistry;
class WorkspaceItem;
class WorkspaceLeaf;
class WorkspaceParent;
class WorkspaceSplit;
class WorkspaceTabs;
class WorkspaceWindow;

struct UndoEntry {
    QString leafId;
    QJsonObject state;
    QJsonObject eState;
    QString parentId;
    QString rootId;
    LeafHistory leafHistory;
    bool pinned = false;
    QString group;
};

class Workspace : public QObject
{
    Q_OBJECT
public:
    static constexpr int UndoCap = 10;

    explicit Workspace(ViewRegistry *registry, QObject *parent = nullptr);

    /// Vault-scoped constructor. The `vaultId` identifies which vault this
    /// workspace belongs to; used by Phase 4b's KDDW substrate for
    /// `DockRegistry` unique-name namespacing across multiple workspaces in
    /// one process. New callers should prefer this constructor.
    explicit Workspace(QString vaultId, ViewRegistry *registry,
                       QObject *parent = nullptr);
    ~Workspace() override;

    /// Identifier of the vault this workspace belongs to. Empty if the
    /// legacy (non-vaultId) constructor was used.
    QString vaultId() const;

    ViewRegistry *viewRegistry() const;
    WorkspaceSplit *mainRoot() const;
    /// Bare central widget of the workspace; replaces `mainRoot()->widget()`
    /// for callers that just need the widget, not the substrate node.
    QWidget *rootWidget() const;

    WorkspaceLeaf *activeLeaf() const;
    void setActiveLeaf(WorkspaceLeaf *leaf);

    QStringList lastOpenFiles() const;
    void setLastOpenFiles(const QStringList &files);
    void pushLastOpenFile(const QString &path);

    // Tree operations
    WorkspaceLeaf *createLeafInTabs(WorkspaceTabs *parent);

    /// Create a new leaf in the same tab group as `sibling`. Pass `nullptr`
    /// to create a new leaf in the active group (or the first group at root
    /// if no leaf is active). Replaces `createLeafInTabs(WorkspaceTabs*)`
    /// for callers that don't have a substrate Tabs handle.
    WorkspaceLeaf *createLeafInGroupOf(WorkspaceLeaf *sibling);

    /// Atomic version of the historical idiom
    /// `createLeafInTabs(activeTabs())` — most callers want this.
    /// Returns nullptr if there is no active group (no leaves at all).
    WorkspaceLeaf *createLeafInActiveGroup();

    void closeLeaf(WorkspaceLeaf *leaf);
    bool canUndoCloseLeaf() const;
    void undoCloseLeaf();
    WorkspaceSplit *splitLeaf(WorkspaceLeaf *leaf, Qt::Orientation direction);

    /// Obsidian-shape user-facing split: clone `leaf`'s view state (+ ephemeral
    /// state, history, pinned, group) into a new leaf in a new split sibling.
    /// The new leaf becomes active. Returns the new leaf, or nullptr on failure.
    WorkspaceLeaf *duplicateLeaf(WorkspaceLeaf *leaf, Qt::Orientation direction);

    // Popout windows (stub — implemented in Task 7)
    WorkspaceWindow *popoutLeaf(WorkspaceLeaf *leaf);
    void reparentToMain(WorkspaceWindow *window);
    QVector<WorkspaceWindow *> windows() const;

    // Find nodes
    WorkspaceTabs *activeTabs() const;
    WorkspaceLeaf *findLeafById(const QString &id) const;
    WorkspaceTabs *findTabsById(const QString &id) const;
    QVector<WorkspaceLeaf *> allLeaves() const;

    /// Tab-group navigation. `nextLeafInActiveGroup` returns the leaf
    /// immediately after the active leaf in its tab group, wrapping back
    /// to the start. Returns nullptr if the active group has fewer than
    /// two leaves or there is no active leaf. Used by Ctrl+Tab.
    WorkspaceLeaf *nextLeafInActiveGroup() const;
    /// Mirror of `nextLeafInActiveGroup`. Used by Ctrl+Shift+Tab.
    WorkspaceLeaf *previousLeafInActiveGroup() const;

    /// Position of `leaf` inside its tab group (0-based), or -1 if `leaf`
    /// is not in any tab group. Used by tab context menus.
    int leafIndexInGroup(WorkspaceLeaf *leaf) const;
    /// Total number of leaves in `leaf`'s tab group, or 0 if `leaf` is
    /// not in any tab group.
    int leafCountInGroup(WorkspaceLeaf *leaf) const;

    /// Close every leaf in `leaf`'s tab group except `leaf` itself.
    /// No-op if `leaf` is not in any tab group.
    void closeOtherLeavesInGroupOf(WorkspaceLeaf *leaf);
    /// Close every leaf in `leaf`'s tab group whose tab index is greater
    /// than `leaf`'s. No-op if `leaf` is not in any tab group.
    void closeLeavesToRightOf(WorkspaceLeaf *leaf);

    // Linked-pane group propagation
    void propagatePinToGroup(WorkspaceLeaf *leaf);
    QVector<WorkspaceLeaf *> groupMembers(const QString &groupId) const;
    WorkspaceLeaf *findOrCreateUnpinnedLeaf(WorkspaceTabs *tabs);

    /// Sibling-typed rename of `findOrCreateUnpinnedLeaf(WorkspaceTabs*)`.
    /// Returns the first unpinned leaf in `sibling`'s tab group, or
    /// creates a new leaf there if all are pinned.
    WorkspaceLeaf *findOrCreateUnpinnedLeafInGroupOf(WorkspaceLeaf *sibling);

    // Persistence
    QJsonObject serialize() const;
    void deserialize(const QJsonObject &json);
    void readWorkspaceJson(const QString &vaultPath);
    void writeWorkspaceJson(const QString &vaultPath);

    /// Tear down the current tree and recreate an empty default layout.
    void resetToDefaultLayout();

    /// Request the host reveal the dock panel for the given plugin slug.
    /// Emits `revealDockViewRequested(slug)` — MainWindow connects to
    /// this to raise the plugin's tool view. Used by plugin `:open`
    /// commands (Cluster R Task 3.1).
    void revealDockView(const QString &slug);

    /// Request the host dispatch `commandId` through its CommandRegistry.
    /// Emits `commandRequested(commandId)`. Used by views (GraphView,
    /// CanvasFileView, MarkdownView) to invoke `split_right` /
    /// `<plugin>:<cmd>` etc. without holding a direct CommandRegistry ref.
    /// Cluster R Task 3.7.
    void requestCommand(const QString &commandId);

Q_SIGNALS:
    void activeLeafChanged(WorkspaceLeaf *leaf);
    void layoutChanged();
    void leafClosed(WorkspaceLeaf *leaf);
    void revealDockViewRequested(const QString &slug);
    void commandRequested(const QString &commandId);

    /// Re-emission of "the user clicked a tab to make this leaf active"
    /// from the substrate. Hosts (MainWindow) connect once at Workspace
    /// setup time and call setActiveLeaf in response, replacing the older
    /// per-Tabs subscription to `WorkspaceTabs::currentTabChanged`. In
    /// Phase 4b this fires from KDDW's `Group::currentDockWidgetChanged`
    /// instead of `WorkspaceTabs`; signature unchanged.
    void tabSelectRequested(WorkspaceLeaf *leaf);

    /// Re-emission of "the user clicked the X on this leaf's tab" from the
    /// substrate. Hosts call closeLeaf in response. Replaces the older
    /// per-Tabs subscription to `WorkspaceTabs::tabCloseRequested(int)`.
    void tabCloseRequested(WorkspaceLeaf *leaf);

private:
    /// Remove an empty WorkspaceTabs from its parent split, and collapse the
    /// split itself into its sole remaining child if that leaves the split
    /// with only one child. Matches Obsidian's "close last tab in a split
    /// pane reclaims the sibling's space" behaviour.
    void collapseEmptyTabs(WorkspaceTabs *tabs);

    WorkspaceItem *deserializeNode(const QJsonObject &json);
    WorkspaceLeaf *findLeafInTree(WorkspaceItem *root, const QString &id) const;
    WorkspaceTabs *findTabsInTree(WorkspaceItem *root, const QString &id) const;
    void collectLeaves(WorkspaceItem *root, QVector<WorkspaceLeaf *> &out) const;
    WorkspaceTabs *findFirstTabs(WorkspaceItem *root) const;
    void setupDefaultLayout();
    void destroyTree();
    void collectAllItems(WorkspaceItem *root, QVector<WorkspaceItem *> &out) const;

    /// Wires `WorkspaceTabs::currentTabChanged` and `tabCloseRequested`
    /// on `tabs` to re-emit `Workspace::tabSelectRequested(leaf)` and
    /// `tabCloseRequested(leaf)`. Idempotent — uses a dynamic property
    /// guard to avoid double-connection. Called from the leaf-add paths
    /// so each Tabs gets wired exactly once.
    void wireTabsSignalForwarding(WorkspaceTabs *tabs);

    QString m_vaultId;
    ViewRegistry *m_registry;
    WorkspaceSplit *m_mainRoot = nullptr;
    WorkspaceLeaf *m_activeLeaf = nullptr;
    QVector<WorkspaceWindow *> m_windows;
    QVector<UndoEntry> m_undoHistory;
    QStringList m_lastOpenFiles;
};

} // namespace Corbomite
