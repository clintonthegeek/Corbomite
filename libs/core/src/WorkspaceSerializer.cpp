// SPDX-FileCopyrightText: 2026 Clinton Molyneux <clinton@concernednetizen.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "WorkspaceSerializer.h"

#include <QJsonArray>
#include <QJsonObject>

#include <kddockwidgets/KDDockWidgets.h>
#include <kddockwidgets/core/DockRegistry.h>
#include <kddockwidgets/qtwidgets/DockWidget.h>
#include <kddockwidgets/qtwidgets/MainWindow.h>

namespace Corbomite::WorkspaceSerializer {

namespace {

// Internal types — not exported.  Mirror Obsidian's node types (split, tabs,
// leaf, window) for serialization only.  They do NOT own widgets.
struct LeafNode {
    QString id;
    QString viewType;
    QString icon;
    QString title;
    QJsonObject state;
    bool pinned = false;
    QString group;
};

struct TabsNode {
    QString id;
    int currentTab = 0;
    bool stacked = false;
    QList<LeafNode> children;
};

struct SplitNode {
    QString id;
    QString direction; // "horizontal" or "vertical"
    QList<SplitNode> splitChildren;
    QList<TabsNode> tabsChildren;
    // Variant-like: a node has EITHER splitChildren OR tabsChildren, not both.
};

LeafNode parseLeaf(const QJsonObject &o)
{
    LeafNode n;
    n.id = o.value(QStringLiteral("id")).toString();
    auto stateObj = o.value(QStringLiteral("state")).toObject();
    n.viewType = stateObj.value(QStringLiteral("type")).toString();
    n.icon = stateObj.value(QStringLiteral("icon")).toString();
    n.title = stateObj.value(QStringLiteral("title")).toString();
    n.state = stateObj.value(QStringLiteral("state")).toObject();
    n.pinned = o.value(QStringLiteral("pinned")).toBool(false);
    n.group = o.value(QStringLiteral("group")).toString();
    return n;
}

TabsNode parseTabs(const QJsonObject &o)
{
    TabsNode n;
    n.id = o.value(QStringLiteral("id")).toString();
    n.currentTab = o.value(QStringLiteral("currentTab")).toInt(0);
    n.stacked = o.value(QStringLiteral("stacked")).toBool(false);
    for (auto v : o.value(QStringLiteral("children")).toArray()) {
        n.children.append(parseLeaf(v.toObject()));
    }
    return n;
}

SplitNode parseSplit(const QJsonObject &o)
{
    SplitNode n;
    n.id = o.value(QStringLiteral("id")).toString();
    n.direction =
        o.value(QStringLiteral("direction")).toString(QStringLiteral("vertical"));
    for (auto v : o.value(QStringLiteral("children")).toArray()) {
        auto childObj = v.toObject();
        auto type = childObj.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("tabs")) {
            n.tabsChildren.append(parseTabs(childObj));
        } else if (type == QStringLiteral("split")) {
            n.splitChildren.append(parseSplit(childObj));
        }
    }
    return n;
}

QJsonObject renderLeaf(const LeafNode &n)
{
    QJsonObject o;
    o[QStringLiteral("id")] = n.id;
    o[QStringLiteral("type")] = QStringLiteral("leaf");
    QJsonObject state;
    state[QStringLiteral("type")] = n.viewType;
    if (!n.state.isEmpty()) state[QStringLiteral("state")] = n.state;
    if (!n.icon.isEmpty()) state[QStringLiteral("icon")] = n.icon;
    if (!n.title.isEmpty()) state[QStringLiteral("title")] = n.title;
    o[QStringLiteral("state")] = state;
    if (n.pinned) o[QStringLiteral("pinned")] = true;
    if (!n.group.isEmpty()) o[QStringLiteral("group")] = n.group;
    return o;
}

QJsonObject renderTabs(const TabsNode &n)
{
    QJsonObject o;
    o[QStringLiteral("id")] = n.id;
    o[QStringLiteral("type")] = QStringLiteral("tabs");
    QJsonArray kids;
    for (const auto &c : n.children) kids.append(renderLeaf(c));
    o[QStringLiteral("children")] = kids;
    if (n.currentTab != 0) o[QStringLiteral("currentTab")] = n.currentTab;
    if (n.stacked) o[QStringLiteral("stacked")] = true;
    return o;
}

QJsonObject renderSplit(const SplitNode &n)
{
    QJsonObject o;
    o[QStringLiteral("id")] = n.id;
    o[QStringLiteral("type")] = QStringLiteral("split");
    o[QStringLiteral("direction")] = n.direction;
    QJsonArray kids;
    for (const auto &c : n.splitChildren) kids.append(renderSplit(c));
    for (const auto &c : n.tabsChildren) kids.append(renderTabs(c));
    o[QStringLiteral("children")] = kids;
    return o;
}

// Walk the KDDW MainWindow tree to produce a SplitNode.  Phase 3 Task 3.3
// only needs the simple case: one split, one tabs, N leaves.  Workspace*
// integration (which would supply real ids + viewType + icon + title) lands
// in later phases; for now we emit placeholders.
SplitNode walkKddwTreeSimple(KDDockWidgets::QtWidgets::MainWindow *main)
{
    SplitNode root;
    root.id = QStringLiteral("aaaaaaaaaaaaaaaa");
    root.direction = QStringLiteral("vertical");

    TabsNode onlyTabs;
    onlyTabs.id = QStringLiteral("bbbbbbbbbbbbbbbb");

    // Use DockRegistry as the source of truth for live dock widgets in this
    // process.  Phase 3 Task 3.3 walks the registry directly; tests isolate
    // by clearing the registry between cases.  Phase 4 will walk the live
    // KDDW layout tree on the supplied MainWindow instead.
    Q_UNUSED(main);
    auto *registry = KDDockWidgets::DockRegistry::self();
    for (auto *dw : registry->dockwidgets()) {
        LeafNode l;
        l.id = dw->uniqueName();
        l.viewType = QStringLiteral("empty");
        l.icon = QStringLiteral("lucide-file");
        l.title = QStringLiteral("New tab");
        onlyTabs.children.append(l);
    }

    root.tabsChildren.append(onlyTabs);
    return root;
}

} // namespace

void fromJson(const QJsonObject &json,
              KDDockWidgets::QtWidgets::MainWindow *main,
              Workspace * /*workspace*/)
{
    auto mainObj = json.value(QStringLiteral("main")).toObject();
    auto rootSplit = parseSplit(mainObj);

    // Phase 3 Task 3.3: simplest case — one split, one tabs, N leaves.
    // Later tasks generalize to nested splits + tabs.
    for (const auto &tabs : rootSplit.tabsChildren) {
        for (const auto &leaf : tabs.children) {
            auto *dw = new KDDockWidgets::QtWidgets::DockWidget(leaf.id);
            main->addDockWidget(dw, KDDockWidgets::Location_OnRight);
        }
    }
}

QJsonObject toJson(KDDockWidgets::QtWidgets::MainWindow *main, Workspace * /*workspace*/)
{
    QJsonObject out;
    out[QStringLiteral("main")] = renderSplit(walkKddwTreeSimple(main));
    // "active" and "lastOpenFiles" added in later tasks.
    return out;
}

} // namespace Corbomite::WorkspaceSerializer
