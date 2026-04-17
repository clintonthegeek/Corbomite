// libs/core/src/Workspace.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/Workspace.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/WorkspaceSplit.h"
#include "corbomite/core/WorkspaceTabs.h"
#include "corbomite/core/WorkspaceWindow.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QWidget>

namespace Corbomite {

Workspace::Workspace(ViewRegistry *registry, QObject *parent)
    : QObject(parent)
    , m_registry(registry)
{
    setupDefaultLayout();
}

Workspace::~Workspace()
{
    destroyTree();
}

ViewRegistry *Workspace::viewRegistry() const { return m_registry; }

WorkspaceSplit *Workspace::mainRoot() const { return m_mainRoot; }

WorkspaceLeaf *Workspace::activeLeaf() const { return m_activeLeaf; }

void Workspace::setActiveLeaf(WorkspaceLeaf *leaf)
{
    if (m_activeLeaf == leaf)
        return;
    m_activeLeaf = leaf;
    if (leaf)
        leaf->updateActiveTime();
    Q_EMIT activeLeafChanged(leaf);
}

QStringList Workspace::lastOpenFiles() const { return m_lastOpenFiles; }

void Workspace::setLastOpenFiles(const QStringList &files) { m_lastOpenFiles = files; }

void Workspace::pushLastOpenFile(const QString &path)
{
    m_lastOpenFiles.removeAll(path);
    m_lastOpenFiles.prepend(path);
    if (m_lastOpenFiles.size() > 50)
        m_lastOpenFiles.removeLast();
}

WorkspaceLeaf *Workspace::createLeafInTabs(WorkspaceTabs *parent)
{
    auto *leaf = new WorkspaceLeaf(m_registry);
    parent->addChild(leaf);
    connect(leaf, &WorkspaceLeaf::pinnedChanged, this, [this, leaf](bool) {
        propagatePinToGroup(leaf);
    });
    Q_EMIT layoutChanged();
    return leaf;
}

void Workspace::closeLeaf(WorkspaceLeaf *leaf)
{
    if (!leaf)
        return;

    UndoEntry entry;
    entry.leafId = leaf->id();
    entry.state = leaf->getViewState();
    entry.eState = leaf->getEphemeralState();
    entry.leafHistory = leaf->history();
    entry.pinned = leaf->pinned();
    entry.group = leaf->group();

    if (auto *parent = leaf->parentItem()) {
        entry.parentId = parent->id();
        if (auto *grandparent = parent->parentItem())
            entry.rootId = grandparent->id();
    }

    m_undoHistory.prepend(entry);
    if (m_undoHistory.size() > UndoCap)
        m_undoHistory.removeLast();

    auto *parentTabs = qobject_cast<WorkspaceTabs *>(leaf->parentItem());

    if (m_activeLeaf == leaf)
        m_activeLeaf = nullptr;

    Q_EMIT leafClosed(leaf);

    if (parentTabs) {
        parentTabs->removeChild(leaf, true);
        if (parentTabs->childCount() > 0 && !m_activeLeaf)
            setActiveLeaf(parentTabs->currentLeaf());
    } else {
        delete leaf;
    }

    Q_EMIT layoutChanged();
}

bool Workspace::canUndoCloseLeaf() const { return !m_undoHistory.isEmpty(); }

void Workspace::undoCloseLeaf()
{
    if (m_undoHistory.isEmpty())
        return;

    UndoEntry entry = m_undoHistory.takeFirst();

    WorkspaceTabs *targetTabs = nullptr;
    if (!entry.parentId.isEmpty())
        targetTabs = findTabsById(entry.parentId);
    if (!targetTabs)
        targetTabs = activeTabs();
    if (!targetTabs)
        targetTabs = findFirstTabs(m_mainRoot);
    if (!targetTabs)
        return;

    auto *leaf = new WorkspaceLeaf(m_registry);
    leaf->setId(entry.leafId);
    leaf->setPinned(entry.pinned);
    leaf->setGroup(entry.group);
    targetTabs->addChild(leaf);

    if (!entry.state.isEmpty())
        leaf->setViewState(entry.state);

    setActiveLeaf(leaf);
    Q_EMIT layoutChanged();
}

WorkspaceSplit *Workspace::splitLeaf(WorkspaceLeaf *leaf, Qt::Orientation direction)
{
    if (!leaf || !leaf->parentItem())
        return nullptr;

    auto *parentTabs = qobject_cast<WorkspaceTabs *>(leaf->parentItem());
    auto *grandparent = parentTabs ? parentTabs->parentItem() : nullptr;
    if (!grandparent)
        return nullptr;

    auto *split = new WorkspaceSplit(this);
    split->setDirection(direction);

    int parentIndex = grandparent->indexOf(parentTabs);
    grandparent->removeChild(parentTabs);
    grandparent->addChild(split, parentIndex);

    split->addChild(parentTabs);

    auto *newTabs = new WorkspaceTabs(this);
    split->addChild(newTabs);

    Q_EMIT layoutChanged();
    return split;
}

WorkspaceWindow *Workspace::popoutLeaf(WorkspaceLeaf *leaf)
{
    if (!leaf)
        return nullptr;

    auto *oldParent = qobject_cast<WorkspaceTabs *>(leaf->parentItem());
    if (oldParent)
        oldParent->removeChild(leaf);

    auto *win = new WorkspaceWindow(this);
    auto *tabs = new WorkspaceTabs(this);
    win->addChild(tabs);
    tabs->addChild(leaf);

    m_windows.append(win);
    win->showWindow();
    Q_EMIT layoutChanged();
    return win;
}

void Workspace::reparentToMain(WorkspaceWindow *window)
{
    if (!window)
        return;

    // Find target in main tree before touching the window tree.
    // activeTabs() follows m_activeLeaf which may be inside the window —
    // always anchor to the main root to avoid reparenting back into the
    // window's inner tabs right before they are destroyed.
    auto *targetTabs = findFirstTabs(m_mainRoot);
    if (!targetTabs)
        return;

    QVector<WorkspaceLeaf *> leaves;
    collectLeaves(window, leaves);

    // Clear active leaf if it lives in this window.
    if (m_activeLeaf && leaves.contains(m_activeLeaf))
        m_activeLeaf = nullptr;

    for (auto *leaf : leaves) {
        if (auto *parent = qobject_cast<WorkspaceParent *>(leaf->parentItem()))
            parent->removeChild(leaf);
        targetTabs->addChild(leaf);
    }

    window->closeWindow();
    m_windows.removeOne(window);
    delete window;
    Q_EMIT layoutChanged();
}

QVector<WorkspaceWindow *> Workspace::windows() const { return m_windows; }

WorkspaceTabs *Workspace::activeTabs() const
{
    if (m_activeLeaf)
        return qobject_cast<WorkspaceTabs *>(m_activeLeaf->parentItem());
    return findFirstTabs(m_mainRoot);
}

WorkspaceLeaf *Workspace::findLeafById(const QString &id) const
{
    return findLeafInTree(m_mainRoot, id);
}

WorkspaceTabs *Workspace::findTabsById(const QString &id) const
{
    return findTabsInTree(m_mainRoot, id);
}

QVector<WorkspaceLeaf *> Workspace::allLeaves() const
{
    QVector<WorkspaceLeaf *> result;
    collectLeaves(m_mainRoot, result);
    return result;
}

QVector<WorkspaceLeaf *> Workspace::groupMembers(const QString &groupId) const
{
    QVector<WorkspaceLeaf *> result;
    if (groupId.isEmpty())
        return result;
    for (auto *leaf : allLeaves()) {
        if (leaf->group() == groupId)
            result.append(leaf);
    }
    return result;
}

void Workspace::propagatePinToGroup(WorkspaceLeaf *leaf)
{
    if (!leaf || leaf->group().isEmpty())
        return;
    bool pinned = leaf->pinned();
    for (auto *member : groupMembers(leaf->group())) {
        if (member != leaf)
            member->setPinned(pinned);
    }
}

WorkspaceLeaf *Workspace::findOrCreateUnpinnedLeaf(WorkspaceTabs *tabs)
{
    for (int i = 0; i < tabs->childCount(); ++i) {
        auto *leaf = tabs->leafAt(i);
        if (leaf && !leaf->pinned())
            return leaf;
    }
    return createLeafInTabs(tabs);
}

QJsonObject Workspace::serialize() const
{
    QJsonObject json;

    if (m_mainRoot)
        json[QStringLiteral("main")] = m_mainRoot->serialize();

    if (m_activeLeaf)
        json[QStringLiteral("active")] = m_activeLeaf->id();
    else
        json[QStringLiteral("active")] = QString{};

    if (!m_lastOpenFiles.isEmpty()) {
        QJsonArray files;
        for (const auto &f : m_lastOpenFiles)
            files.append(f);
        json[QStringLiteral("lastOpenFiles")] = files;
    }

    return json;
}

void Workspace::deserialize(const QJsonObject &json)
{
    destroyTree();

    if (json.contains(QStringLiteral("main"))) {
        auto *node = deserializeNode(json[QStringLiteral("main")].toObject());
        m_mainRoot = qobject_cast<WorkspaceSplit *>(node);
    }

    if (!m_mainRoot)
        setupDefaultLayout();

    QString activeId = json[QStringLiteral("active")].toString();
    if (!activeId.isEmpty())
        m_activeLeaf = findLeafById(activeId);
    if (!m_activeLeaf) {
        auto leaves = allLeaves();
        if (!leaves.isEmpty())
            m_activeLeaf = leaves.first();
    }

    m_lastOpenFiles.clear();
    for (const auto &v : json[QStringLiteral("lastOpenFiles")].toArray())
        m_lastOpenFiles.append(v.toString());

    // Mark all non-active leaves as deferred so they don't construct a View
    // until the user focuses them. The active leaf and each WorkspaceTabs'
    // currentTab leaf are loaded eagerly (§3.5).
    for (auto *leaf : allLeaves()) {
        if (leaf == m_activeLeaf)
            continue;

        // Don't defer the currentTab leaf of its parent WorkspaceTabs.
        if (auto *parentTabs = qobject_cast<WorkspaceTabs *>(leaf->parentItem())) {
            if (parentTabs->currentLeaf() == leaf)
                continue;
        }

        auto state = leaf->getViewState();
        QString icon = state[QStringLiteral("icon")].toString();
        QString title = state[QStringLiteral("title")].toString();
        if (icon.isEmpty())
            icon = QStringLiteral("document");
        if (title.isEmpty())
            title = QStringLiteral("Untitled");
        leaf->setDeferred(true, icon, title);
    }

    // Ensure the active leaf is not stuck in deferred state.
    if (m_activeLeaf && m_activeLeaf->isDeferred())
        m_activeLeaf->loadIfDeferred();

    Q_EMIT layoutChanged();
}

void Workspace::readWorkspaceJson(const QString &vaultPath)
{
    QString path = vaultPath + QStringLiteral("/.obsidian/workspace.json");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        setupDefaultLayout();
        // Spec: default layout must have at least one leaf with an active leaf set.
        auto *tabs = findFirstTabs(m_mainRoot);
        if (tabs) {
            auto *leaf = createLeafInTabs(tabs);
            setActiveLeaf(leaf);
        }
        return;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError) {
        setupDefaultLayout();
        auto *tabs = findFirstTabs(m_mainRoot);
        if (tabs) {
            auto *leaf = createLeafInTabs(tabs);
            setActiveLeaf(leaf);
        }
        return;
    }

    deserialize(doc.object());
}

void Workspace::writeWorkspaceJson(const QString &vaultPath)
{
    QString dirPath = vaultPath + QStringLiteral("/.obsidian");
    QDir().mkpath(dirPath);

    QString path = dirPath + QStringLiteral("/workspace.json");
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return;

    QJsonDocument doc(serialize());
    f.write(doc.toJson(QJsonDocument::Indented));
}

WorkspaceItem *Workspace::deserializeNode(const QJsonObject &json)
{
    QString type = json[QStringLiteral("type")].toString();

    if (type == QStringLiteral("split")) {
        auto *split = new WorkspaceSplit(this);
        if (json.contains(QStringLiteral("id")))
            split->setId(json[QStringLiteral("id")].toString());
        QString dir = json[QStringLiteral("direction")].toString();
        split->setDirection(dir == QStringLiteral("vertical") ? Qt::Vertical : Qt::Horizontal);
        if (json.contains(QStringLiteral("dimension")))
            split->setDimension(json[QStringLiteral("dimension")].toInt());

        for (const auto &child : json[QStringLiteral("children")].toArray()) {
            if (auto *node = deserializeNode(child.toObject()))
                split->addChild(node);
        }
        return split;
    }

    if (type == QStringLiteral("tabs")) {
        auto *tabs = new WorkspaceTabs(this);
        if (json.contains(QStringLiteral("id")))
            tabs->setId(json[QStringLiteral("id")].toString());
        if (json.contains(QStringLiteral("dimension")))
            tabs->setDimension(json[QStringLiteral("dimension")].toInt());
        if (json[QStringLiteral("stacked")].toBool())
            tabs->setStacked(true);

        for (const auto &child : json[QStringLiteral("children")].toArray()) {
            if (auto *node = deserializeNode(child.toObject()))
                tabs->addChild(node);
        }

        int currentTab = json[QStringLiteral("currentTab")].toInt(0);
        tabs->setCurrentTab(currentTab);
        return tabs;
    }

    if (type == QStringLiteral("leaf")) {
        auto *leaf = WorkspaceLeaf::deserialize(json, m_registry, nullptr);
        return leaf;
    }

    return nullptr;
}

WorkspaceLeaf *Workspace::findLeafInTree(WorkspaceItem *root, const QString &id) const
{
    if (!root)
        return nullptr;
    if (auto *leaf = qobject_cast<WorkspaceLeaf *>(root)) {
        if (leaf->id() == id)
            return leaf;
        return nullptr;
    }
    if (auto *parent = qobject_cast<WorkspaceParent *>(root)) {
        for (auto *child : parent->children()) {
            if (auto *found = findLeafInTree(child, id))
                return found;
        }
    }
    return nullptr;
}

WorkspaceTabs *Workspace::findTabsInTree(WorkspaceItem *root, const QString &id) const
{
    if (!root)
        return nullptr;
    if (auto *tabs = qobject_cast<WorkspaceTabs *>(root)) {
        if (tabs->id() == id)
            return tabs;
    }
    if (auto *parent = qobject_cast<WorkspaceParent *>(root)) {
        for (auto *child : parent->children()) {
            if (auto *found = findTabsInTree(child, id))
                return found;
        }
    }
    return nullptr;
}

void Workspace::collectLeaves(WorkspaceItem *root, QVector<WorkspaceLeaf *> &out) const
{
    if (!root)
        return;
    if (auto *leaf = qobject_cast<WorkspaceLeaf *>(root)) {
        out.append(leaf);
        return;
    }
    if (auto *parent = qobject_cast<WorkspaceParent *>(root)) {
        for (auto *child : parent->children())
            collectLeaves(child, out);
    }
}

WorkspaceTabs *Workspace::findFirstTabs(WorkspaceItem *root) const
{
    if (!root)
        return nullptr;
    if (auto *tabs = qobject_cast<WorkspaceTabs *>(root))
        return tabs;
    if (auto *parent = qobject_cast<WorkspaceParent *>(root)) {
        for (auto *child : parent->children()) {
            if (auto *tabs = findFirstTabs(child))
                return tabs;
        }
    }
    return nullptr;
}

void Workspace::setupDefaultLayout()
{
    destroyTree();
    m_mainRoot = new WorkspaceSplit(this);
    m_mainRoot->setDirection(Qt::Vertical);

    auto *tabs = new WorkspaceTabs(this);
    m_mainRoot->addChild(tabs);
}

void Workspace::resetToDefaultLayout()
{
    destroyTree();
    setupDefaultLayout();
    Q_EMIT layoutChanged();
}

void Workspace::destroyTree()
{
    if (!m_mainRoot)
        return;

    // 1. Close all views to release NoteDocument / file references
    //    before the widget tree is torn down.
    for (auto *leaf : allLeaves())
        leaf->closeCurrentView();

    // 2. Collect every item in the tree (depth-first).
    QVector<WorkspaceItem *> items;
    collectAllItems(m_mainRoot, items);

    // 3. Detach each item's widget from its Qt parent so that
    //    cascade deletion doesn't reach sibling widgets.  After this
    //    each widget is an orphan that its own destructor can safely
    //    delete without double-free.  QPointer guards mean widget()
    //    returns nullptr if the widget was already cascade-deleted
    //    (e.g. during MainWindow destruction).
    for (auto *item : items) {
        if (auto *w = item->widget())
            w->setParent(nullptr);
    }

    // 4. Clear internal pointers before deleting objects.
    m_mainRoot = nullptr;
    m_activeLeaf = nullptr;
    m_undoHistory.clear();

    // 5. Delete every item — their destructors delete their own widget.
    qDeleteAll(items);
}

void Workspace::collectAllItems(WorkspaceItem *root,
                                QVector<WorkspaceItem *> &out) const
{
    if (!root)
        return;
    out.append(root);
    if (auto *p = qobject_cast<WorkspaceParent *>(root)) {
        for (auto *child : p->children())
            collectAllItems(child, out);
    }
}

} // namespace Corbomite
