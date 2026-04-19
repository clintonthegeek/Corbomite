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
    ~Workspace() override;

    ViewRegistry *viewRegistry() const;
    WorkspaceSplit *mainRoot() const;

    WorkspaceLeaf *activeLeaf() const;
    void setActiveLeaf(WorkspaceLeaf *leaf);

    QStringList lastOpenFiles() const;
    void setLastOpenFiles(const QStringList &files);
    void pushLastOpenFile(const QString &path);

    // Tree operations
    WorkspaceLeaf *createLeafInTabs(WorkspaceTabs *parent);
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

    // Linked-pane group propagation
    void propagatePinToGroup(WorkspaceLeaf *leaf);
    QVector<WorkspaceLeaf *> groupMembers(const QString &groupId) const;
    WorkspaceLeaf *findOrCreateUnpinnedLeaf(WorkspaceTabs *tabs);

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

Q_SIGNALS:
    void activeLeafChanged(WorkspaceLeaf *leaf);
    void layoutChanged();
    void leafClosed(WorkspaceLeaf *leaf);
    void revealDockViewRequested(const QString &slug);

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

    ViewRegistry *m_registry;
    WorkspaceSplit *m_mainRoot = nullptr;
    WorkspaceLeaf *m_activeLeaf = nullptr;
    QVector<WorkspaceWindow *> m_windows;
    QVector<UndoEntry> m_undoHistory;
    QStringList m_lastOpenFiles;
};

} // namespace Corbomite
