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

    // Popout windows (stub — implemented in Task 7)
    WorkspaceWindow *popoutLeaf(WorkspaceLeaf *leaf);
    void reparentToMain(WorkspaceWindow *window);
    QVector<WorkspaceWindow *> windows() const;

    // Find nodes
    WorkspaceTabs *activeTabs() const;
    WorkspaceLeaf *findLeafById(const QString &id) const;
    WorkspaceTabs *findTabsById(const QString &id) const;
    QVector<WorkspaceLeaf *> allLeaves() const;

    // Persistence
    QJsonObject serialize() const;
    void deserialize(const QJsonObject &json);
    void readWorkspaceJson(const QString &vaultPath);
    void writeWorkspaceJson(const QString &vaultPath);

Q_SIGNALS:
    void activeLeafChanged(WorkspaceLeaf *leaf);
    void layoutChanged();
    void leafClosed(WorkspaceLeaf *leaf);

private:
    WorkspaceItem *deserializeNode(const QJsonObject &json);
    WorkspaceLeaf *findLeafInTree(WorkspaceItem *root, const QString &id) const;
    WorkspaceTabs *findTabsInTree(WorkspaceItem *root, const QString &id) const;
    void collectLeaves(WorkspaceItem *root, QVector<WorkspaceLeaf *> &out) const;
    WorkspaceTabs *findFirstTabs(WorkspaceItem *root) const;
    void setupDefaultLayout();

    ViewRegistry *m_registry;
    WorkspaceSplit *m_mainRoot = nullptr;
    WorkspaceLeaf *m_activeLeaf = nullptr;
    QVector<WorkspaceWindow *> m_windows;
    QVector<UndoEntry> m_undoHistory;
    QStringList m_lastOpenFiles;
};

} // namespace Corbomite
