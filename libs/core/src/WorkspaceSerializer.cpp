// SPDX-FileCopyrightText: 2026 Clinton Molyneux <clinton@concernednetizen.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "WorkspaceSerializer.h"

#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceLeaf.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalBlocker>

#include <QLoggingCategory>

#include <kddockwidgets/KDDockWidgets.h>
#include <kddockwidgets/LayoutSaver.h>
#include <kddockwidgets/core/DockRegistry.h>
#include <kddockwidgets/core/DockWidget.h>
#include <kddockwidgets/core/FloatingWindow.h>
#include <kddockwidgets/qtwidgets/DockWidget.h>
#include <kddockwidgets/qtwidgets/MainWindow.h>

namespace Corbomite::WorkspaceSerializer {

Q_LOGGING_CATEGORY(lcWorkspaceSerializer, "corbomite.workspace.serializer")

namespace {

// Phase 3 workaround: stackedness stored as a sidecar map keyed by
// first-leaf-id of the tabs group.  Phase 4 replaces this with a
// WorkspaceLeaf-carried flag once Workspace owns the leaf model.
QHash<QString, bool> &stackedSidecar()
{
    static QHash<QString, bool> map;
    return map;
}

// Forward declaration; LeafNode is defined below.
struct LeafNode;
QHash<QString, LeafNode> &leafSidecar();

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
    QJsonObject unknownKeys;
};

QHash<QString, LeafNode> &leafSidecar()
{
    static QHash<QString, LeafNode> map;
    return map;
}

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
    // Original interleaved order of children. Each entry is (isSplit, index)
    // — index points into splitChildren or tabsChildren depending on the
    // bool. Render/materialize walk this list to preserve the on-disk order
    // of children at this split level. Earlier code assumed a node had
    // EITHER splits OR tabs (not both interleaved); Obsidian-authored
    // layouts and KDDW LayoutSaver dumps both produce mixed orderings,
    // so the assumption was wrong and silently scrambled split positions.
    QList<QPair<bool, int>> childOrder;

    void addTabs(const TabsNode &t) {
        tabsChildren.append(t);
        childOrder.append({false, tabsChildren.size() - 1});
    }
    void addSplit(const SplitNode &s) {
        splitChildren.append(s);
        childOrder.append({true, splitChildren.size() - 1});
    }
};

struct WindowNode {
    QString id;
    int x = 0, y = 0, width = 0, height = 0;
    bool maximize = false;
    SplitNode content;
};

LeafNode parseLeaf(const QJsonObject &o)
{
    static const QSet<QString> known = {
        QStringLiteral("id"), QStringLiteral("type"), QStringLiteral("state"),
        QStringLiteral("pinned"), QStringLiteral("group"),
    };
    LeafNode n;
    n.id = o.value(QStringLiteral("id")).toString();
    auto stateObj = o.value(QStringLiteral("state")).toObject();
    n.viewType = stateObj.value(QStringLiteral("type")).toString();
    n.icon = stateObj.value(QStringLiteral("icon")).toString();
    n.title = stateObj.value(QStringLiteral("title")).toString();
    n.state = stateObj.value(QStringLiteral("state")).toObject();
    n.pinned = o.value(QStringLiteral("pinned")).toBool(false);
    n.group = o.value(QStringLiteral("group")).toString();
    for (auto it = o.begin(); it != o.end(); ++it) {
        if (!known.contains(it.key())) {
            n.unknownKeys.insert(it.key(), it.value());
        }
    }
    leafSidecar().insert(n.id, n);
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
    if (n.stacked && !n.children.isEmpty()) {
        stackedSidecar().insert(n.children.first().id, true);
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
            n.addTabs(parseTabs(childObj));
        } else if (type == QStringLiteral("split")) {
            n.addSplit(parseSplit(childObj));
        }
    }
    return n;
}

WindowNode parseWindow(const QJsonObject &o)
{
    WindowNode w;
    w.id = o.value(QStringLiteral("id")).toString();
    w.x = o.value(QStringLiteral("x")).toInt(0);
    w.y = o.value(QStringLiteral("y")).toInt(0);
    w.width = o.value(QStringLiteral("width")).toInt(0);
    w.height = o.value(QStringLiteral("height")).toInt(0);
    w.maximize = o.value(QStringLiteral("maximize")).toBool(false);
    // The window node carries the same children shape as a split node:
    // either tabs or nested splits beneath it.
    w.content = parseSplit(o);
    return w;
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
    for (auto it = n.unknownKeys.begin(); it != n.unknownKeys.end(); ++it) {
        o.insert(it.key(), it.value());
    }
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
    // Walk childOrder to preserve original interleaved ordering. Falls back
    // to splits-then-tabs if childOrder wasn't populated (defensive — should
    // not happen for nodes built by parseSplit / addTabs / addSplit).
    if (!n.childOrder.isEmpty()) {
        for (const auto &slot : n.childOrder) {
            const bool isSplit = slot.first;
            const int idx = slot.second;
            if (isSplit && idx < n.splitChildren.size()) {
                kids.append(renderSplit(n.splitChildren[idx]));
            } else if (!isSplit && idx < n.tabsChildren.size()) {
                kids.append(renderTabs(n.tabsChildren[idx]));
            }
        }
    } else {
        for (const auto &c : n.splitChildren) kids.append(renderSplit(c));
        for (const auto &c : n.tabsChildren) kids.append(renderTabs(c));
    }
    o[QStringLiteral("children")] = kids;
    return o;
}

// Strip the `<vaultId>:` prefix from a DockWidget unique-name to recover
// the WorkspaceLeaf id (Workspace::registerLeaf installs it via
// uniqueNameFor at Workspace.cpp:31-36).
QString stripVaultPrefix(const QString &uniqueName, const QString &vaultId)
{
    if (vaultId.isEmpty()) return uniqueName;
    const QString prefix = vaultId + QChar(':');
    return uniqueName.startsWith(prefix)
        ? uniqueName.mid(prefix.size())
        : uniqueName;
}

// Build a LeafNode for a dock widget identified by uniqueName.
// - When `workspace` is non-null, the leaf state comes from the live
//   WorkspaceLeaf (looked up via findLeafById with vault prefix stripped).
//   The serialized leaf JSON is parsed back into LeafNode shape so the
//   render pipeline downstream is uniform.
// - When `workspace` is null (test path), falls back to the in-process
//   leafSidecar map populated during parseLeaf.
LeafNode buildLeafNode(const QString &uniqueName,
                       Corbomite::Workspace *workspace)
{
    if (workspace) {
        const QString stripped = stripVaultPrefix(uniqueName, workspace->vaultId());
        if (auto *wl = workspace->findLeafById(stripped)) {
            const QJsonObject obj = wl->serialize();
            LeafNode n;
            n.id = obj.value(QStringLiteral("id")).toString();
            const auto stateObj = obj.value(QStringLiteral("state")).toObject();
            n.viewType = stateObj.value(QStringLiteral("type")).toString();
            n.icon = stateObj.value(QStringLiteral("icon")).toString();
            n.title = stateObj.value(QStringLiteral("title")).toString();
            n.state = stateObj.value(QStringLiteral("state")).toObject();
            n.pinned = obj.value(QStringLiteral("pinned")).toBool(false);
            n.group = obj.value(QStringLiteral("group")).toString();
            // Anything else on the live serialize() output came from
            // m_unknownLeafKeys; capture for the renderLeaf merge.
            static const QSet<QString> known = {
                QStringLiteral("id"), QStringLiteral("type"),
                QStringLiteral("state"), QStringLiteral("pinned"),
                QStringLiteral("group"),
            };
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                if (!known.contains(it.key()))
                    n.unknownKeys.insert(it.key(), it.value());
            }
            return n;
        }
    }
    LeafNode l = leafSidecar().value(uniqueName, LeafNode{});
    if (l.id.isEmpty()) {
        l.id = uniqueName;
        l.viewType = QStringLiteral("empty");
        l.icon = QStringLiteral("lucide-file");
        l.title = QStringLiteral("New tab");
    }
    return l;
}

// Walk a KDDW LayoutSaver-shape `layout` node recursively. The schema is
// documented in docs/superpowers/specs/2026-04-26-kddw-layoutsaver-shape.md.
// Container nodes have isContainer=true + orientation + children;
// leaf nodes carry a guestId pointing into the sibling `frames` dict.
//
// orientation maps to Qt::Orientation values: Horizontal=1, Vertical=2.
// Obsidian's "vertical" split == top/bottom siblings == Qt::Vertical (2).
void walkLayoutNode(const QJsonObject &layoutNode,
                    const QJsonObject &frames,
                    SplitNode &parent,
                    Corbomite::Workspace *workspace);

void walkLayoutContainer(const QJsonObject &containerNode,
                         const QJsonObject &frames,
                         SplitNode &out,
                         Corbomite::Workspace *workspace)
{
    const int orient = containerNode.value(QStringLiteral("orientation")).toInt();
    out.direction = (orient == 2)
        ? QStringLiteral("vertical")
        : QStringLiteral("horizontal");
    const auto children = containerNode.value(QStringLiteral("children")).toArray();
    for (const auto &v : children)
        walkLayoutNode(v.toObject(), frames, out, workspace);
}

void walkLayoutNode(const QJsonObject &layoutNode,
                    const QJsonObject &frames,
                    SplitNode &parent,
                    Corbomite::Workspace *workspace)
{
    if (layoutNode.value(QStringLiteral("isContainer")).toBool()) {
        SplitNode child;
        walkLayoutContainer(layoutNode, frames, child, workspace);
        parent.addSplit(child);
        return;
    }
    // Leaf-of-tree (group reference). Resolve the frame via guestId.
    const QString guestId = layoutNode.value(QStringLiteral("guestId")).toString();
    const QJsonObject frame = frames.value(guestId).toObject();
    TabsNode tabs;
    tabs.currentTab = frame.value(QStringLiteral("currentTabIndex")).toInt(0);
    const auto frameDws = frame.value(QStringLiteral("dockWidgets")).toArray();
    for (const auto &v : frameDws)
        tabs.children.append(buildLeafNode(v.toString(), workspace));

    // stacked: production path reads from Workspace; tests fall back to
    // the in-process stackedSidecar populated during parseTabs.
    if (!tabs.children.isEmpty()) {
        if (workspace) {
            if (auto *wl = workspace->findLeafById(
                    stripVaultPrefix(frameDws.first().toString(),
                                      workspace->vaultId()))) {
                tabs.stacked = workspace->isTabGroupStacked(
                    workspace->tabGroupIdOf(wl));
            }
        } else if (stackedSidecar().value(tabs.children.first().id, false)) {
            tabs.stacked = true;
        }
    }
    parent.addTabs(tabs);
}

// Locate this MainWindow's entry in a LayoutSaver JSON dump.
QJsonObject findMainWindowEntry(const QJsonObject &saverRoot,
                                const QString &mainUniqueName)
{
    const auto mws = saverRoot.value(QStringLiteral("mainWindows")).toArray();
    for (const auto &v : mws) {
        auto obj = v.toObject();
        if (obj.value(QStringLiteral("uniqueName")).toString() == mainUniqueName)
            return obj;
    }
    return {};
}

// Walk a KDDW multiSplitterLayout sub-object (either main or floating
// window's) and return a SplitNode tree. Caller has already located the
// correct entry in the LayoutSaver dump.
SplitNode walkMultiSplitterLayout(const QJsonObject &msl,
                                   Corbomite::Workspace *workspace)
{
    SplitNode root;
    root.direction = QStringLiteral("vertical");
    const QJsonObject frames = msl.value(QStringLiteral("frames")).toObject();
    const QJsonObject rootLayout = msl.value(QStringLiteral("layout")).toObject();
    if (rootLayout.isEmpty()) return root;
    walkLayoutContainer(rootLayout, frames, root, workspace);
    return root;
}

// Walk the KDDW MainWindow tree by parsing LayoutSaver::serializeLayout()
// JSON output. Replaces the Phase 3 flat walker. Schema reference:
// docs/superpowers/specs/2026-04-26-kddw-layoutsaver-shape.md.
SplitNode walkLayoutSaverTree(KDDockWidgets::QtWidgets::MainWindow *main,
                               const QJsonObject &saverRoot,
                               Corbomite::Workspace *workspace)
{
    SplitNode root;
    root.direction = QStringLiteral("vertical");
    if (!main) return root;
    const QJsonObject mwEntry = findMainWindowEntry(saverRoot, main->uniqueName());
    if (mwEntry.isEmpty()) {
        qCWarning(lcWorkspaceSerializer)
            << "no LayoutSaver entry for MainWindow" << main->uniqueName();
        return root;
    }
    return walkMultiSplitterLayout(
        mwEntry.value(QStringLiteral("multiSplitterLayout")).toObject(),
        workspace);
}

// Locate `fw`'s entry in a LayoutSaver JSON dump by searching for a frame
// that contains one of `fw`'s live dockwidget unique names.
QJsonObject findFloatingWindowEntry(const QJsonObject &saverRoot,
                                    KDDockWidgets::Core::FloatingWindow *fw)
{
    QSet<QString> liveDws;
    for (auto *dw : fw->dockWidgets())
        liveDws.insert(dw->uniqueName());
    const auto fws = saverRoot.value(QStringLiteral("floatingWindows")).toArray();
    for (const auto &v : fws) {
        const auto entry = v.toObject();
        const auto frames = entry.value(QStringLiteral("multiSplitterLayout"))
                                  .toObject().value(QStringLiteral("frames"))
                                  .toObject();
        for (auto it = frames.begin(); it != frames.end(); ++it) {
            const auto fdws = it.value().toObject()
                                .value(QStringLiteral("dockWidgets")).toArray();
            for (const auto &nv : fdws) {
                if (liveDws.contains(nv.toString()))
                    return entry;
            }
        }
    }
    return {};
}

// Translate an Obsidian split direction + position to a KDDW Location.
// "horizontal" = left↔right siblings; "vertical" = top↔bottom siblings.
KDDockWidgets::Location directionToKddwLocation(const QString &direction,
                                                bool firstInParent)
{
    if (firstInParent) {
        // Anchor placement; the first child of the root split has no sibling
        // to relate to, so we place it on the left (KDDW grows the layout
        // outward from this anchor).
        return KDDockWidgets::Location_OnLeft;
    }
    return direction == QStringLiteral("horizontal")
        ? KDDockWidgets::Location_OnRight
        : KDDockWidgets::Location_OnBottom;
}

// Dock `dw` at `location` relative to `relativeTo`, routing through the
// correct containing-window API. When `relativeTo` is in a floating
// window, addDockWidgetToContainingWindow keeps the new dock in the same
// float; otherwise we fall through to the MainWindow API. This is the
// load-bearing helper for nested-split materialization inside popouts.
void dockBesideOrInto(KDDockWidgets::QtWidgets::DockWidget *dw,
                      KDDockWidgets::Location location,
                      KDDockWidgets::QtWidgets::DockWidget *relativeTo,
                      KDDockWidgets::QtWidgets::MainWindow *main)
{
    if (relativeTo && relativeTo->dockWidget()->isFloating()) {
        relativeTo->dockWidget()->addDockWidgetToContainingWindow(
            dw->dockWidget(), location, relativeTo->dockWidget());
    } else if (relativeTo) {
        main->addDockWidget(dw, location, relativeTo);
    } else {
        main->addDockWidget(dw, location);
    }
}

// Apply parsed leaf-payload state onto a Workspace-owned leaf. Centralized
// to keep the workspace-non-null path consistent across main and floating
// materialization.
void applyLeafPayload(Corbomite::WorkspaceLeaf *wl, const LeafNode &leaf)
{
    if (!wl) return;
    if (leaf.pinned) wl->setPinned(true);
    if (!leaf.group.isEmpty()) wl->setGroup(leaf.group);
    if (!leaf.viewType.isEmpty()) {
        QJsonObject viewState;
        viewState[QStringLiteral("type")] = leaf.viewType;
        if (!leaf.state.isEmpty())
            viewState[QStringLiteral("state")] = leaf.state;
        if (!leaf.icon.isEmpty())
            viewState[QStringLiteral("icon")] = leaf.icon;
        if (!leaf.title.isEmpty())
            viewState[QStringLiteral("title")] = leaf.title;
        wl->setViewState(viewState);
    }
    wl->setUnknownLeafKeys(leaf.unknownKeys);
}

// Materialize one TabsNode: create N DockWidgets; the first becomes a new
// container (placed at `location` relative to `relativeTo` if set, otherwise
// docked into `main` directly); subsequent leaves are tabbed onto the first.
// Returns the first DockWidget, which downstream callers use as the anchor
// for further sibling placements.
//
// When `workspace` is non-null, leaves are constructed via
// Workspace::createLeafUnplaced so they're registered with persistent ids
// and their state survives. Tab-group bookkeeping is updated post-dock.
KDDockWidgets::QtWidgets::DockWidget *
materializeTabs(const TabsNode &tabs,
                KDDockWidgets::QtWidgets::MainWindow *main,
                KDDockWidgets::QtWidgets::DockWidget *relativeTo,
                KDDockWidgets::Location location,
                Corbomite::Workspace *workspace)
{
    KDDockWidgets::QtWidgets::DockWidget *first = nullptr;
    QString tabGroupId;
    for (const auto &leaf : tabs.children) {
        KDDockWidgets::QtWidgets::DockWidget *dw = nullptr;
        Corbomite::WorkspaceLeaf *wl = nullptr;
        if (workspace) {
            wl = workspace->createLeafUnplaced(leaf.id);
            dw = wl->dockWidget();
        } else {
            dw = new KDDockWidgets::QtWidgets::DockWidget(leaf.id);
        }
        {
            QSignalBlocker block(dw);
            if (!first) {
                dockBesideOrInto(dw, location, relativeTo, main);
                first = dw;
                if (workspace)
                    tabGroupId = Corbomite::Workspace::freshTabGroupId();
            } else {
                first->addDockWidgetAsTab(dw);
            }
        }
        if (workspace && wl) {
            workspace->setTabGroupOf(wl, tabGroupId);
            applyLeafPayload(wl, leaf);
        }
    }
    if (workspace && !tabGroupId.isEmpty() && tabs.stacked)
        workspace->setTabGroupStacked(tabGroupId, true);

    // KDDW makes the most-recently-added tab current. We always need to
    // explicitly select tabs.currentTab — even when it's 0 — because the
    // last leaf added (the trailing one in the loop) becomes current
    // otherwise.
    if (first && !tabs.children.isEmpty()) {
        int idx = tabs.currentTab;
        if (idx < 0 || idx >= tabs.children.size()) idx = 0;
        const QString leafId = tabs.children[idx].id;
        const QString uniqueName = (workspace && !workspace->vaultId().isEmpty())
            ? QStringLiteral("%1:%2").arg(workspace->vaultId(), leafId)
            : leafId;
        if (auto *current = KDDockWidgets::Core::DockWidget::byName(uniqueName))
            current->setAsCurrentTab();
    }
    return first;
}

// Forward declaration — materializeSplit recurses on itself.
KDDockWidgets::QtWidgets::DockWidget *
materializeSplit(const SplitNode &split,
                 KDDockWidgets::QtWidgets::MainWindow *main,
                 KDDockWidgets::QtWidgets::DockWidget *relativeTo,
                 KDDockWidgets::Location baseLocation,
                 Corbomite::Workspace *workspace);

KDDockWidgets::QtWidgets::DockWidget *
materializeSplit(const SplitNode &split,
                 KDDockWidgets::QtWidgets::MainWindow *main,
                 KDDockWidgets::QtWidgets::DockWidget *relativeTo,
                 KDDockWidgets::Location baseLocation,
                 Corbomite::Workspace *workspace)
{
    bool first = true;
    KDDockWidgets::QtWidgets::DockWidget *anchorForNext = relativeTo;
    KDDockWidgets::QtWidgets::DockWidget *firstAnchor = nullptr;

    auto placeChild = [&](auto &&placeFn) {
        auto loc = first
            ? baseLocation
            : directionToKddwLocation(split.direction, /*firstInParent=*/false);
        // Orphan recovery: if a previous sibling failed to materialize
        // (e.g. an empty tabs node in the input JSON), anchorForNext is
        // null; KDDW's addDockWidget treats a null relativeTo as "dock to
        // the main window root", which is the safe fallback.  Log it so a
        // surprising layout doesn't go unnoticed.
        if (!first && !anchorForNext) {
            qCWarning(lcWorkspaceSerializer)
                << "orphaned child in split" << split.id
                << "— previous sibling produced no anchor; re-homing to root";
            loc = KDDockWidgets::Location_OnRight;
        }
        auto *placed = placeFn(loc, anchorForNext);
        if (placed) {
            if (!firstAnchor) firstAnchor = placed;
            anchorForNext = placed;
        }
        first = false;
    };

    // Walk childOrder so siblings are docked left-to-right (or top-to-bottom)
    // in their JSON order. Falls back to tabs-then-splits if childOrder is
    // empty (defensive — should not happen for parsed/programmatic nodes).
    if (!split.childOrder.isEmpty()) {
        for (const auto &slot : split.childOrder) {
            const bool isSplit = slot.first;
            const int idx = slot.second;
            if (isSplit && idx < split.splitChildren.size()) {
                const auto &childSplit = split.splitChildren[idx];
                placeChild([&](KDDockWidgets::Location loc,
                               KDDockWidgets::QtWidgets::DockWidget *rel) {
                    return materializeSplit(childSplit, main, rel, loc, workspace);
                });
            } else if (!isSplit && idx < split.tabsChildren.size()) {
                const auto &childTabs = split.tabsChildren[idx];
                placeChild([&](KDDockWidgets::Location loc,
                               KDDockWidgets::QtWidgets::DockWidget *rel) {
                    return materializeTabs(childTabs, main, rel, loc, workspace);
                });
            }
        }
    } else {
        for (const auto &childTabs : split.tabsChildren) {
            placeChild([&](KDDockWidgets::Location loc,
                           KDDockWidgets::QtWidgets::DockWidget *rel) {
                return materializeTabs(childTabs, main, rel, loc, workspace);
            });
        }
        for (const auto &childSplit : split.splitChildren) {
            placeChild([&](KDDockWidgets::Location loc,
                           KDDockWidgets::QtWidgets::DockWidget *rel) {
                return materializeSplit(childSplit, main, rel, loc, workspace);
            });
        }
    }

    return firstAnchor;
}

// DFS to find the leftmost-first leaf id in a SplitNode tree. Used to
// pick the anchor leaf when materializing a floating window.
QString findFirstLeafId(const SplitNode &node)
{
    if (!node.tabsChildren.isEmpty()
        && !node.tabsChildren.first().children.isEmpty())
        return node.tabsChildren.first().children.first().id;
    for (const auto &s : node.splitChildren) {
        QString r = findFirstLeafId(s);
        if (!r.isEmpty()) return r;
    }
    return {};
}

// Strip the leftmost-first leaf with the matching id from a SplitNode tree.
// Empties get cleaned up so subsequent materializeSplit doesn't see stale
// nodes.
void removeFirstLeaf(SplitNode &node, const QString &id)
{
    if (!node.tabsChildren.isEmpty()) {
        auto &firstTabs = node.tabsChildren.first();
        if (!firstTabs.children.isEmpty()
            && firstTabs.children.first().id == id) {
            firstTabs.children.removeFirst();
            if (firstTabs.children.isEmpty())
                node.tabsChildren.removeFirst();
            return;
        }
    }
    for (auto &s : node.splitChildren)
        removeFirstLeaf(s, id);
}

// Find the LeafNode in `node` whose id matches; returns nullptr when not
// present. Used to recover the anchor's parsed payload after trimming.
const LeafNode *findLeafNodeById(const SplitNode &node, const QString &id)
{
    for (const auto &t : node.tabsChildren)
        for (const auto &l : t.children)
            if (l.id == id) return &l;
    for (const auto &s : node.splitChildren)
        if (auto *r = findLeafNodeById(s, id)) return r;
    return nullptr;
}

// Materialize a floating window. Strategy:
//   1. DFS-locate the first leaf id in the tree.
//   2. Dock it into `main` momentarily, then setFloating(true) — that's
//      the only way KDDW will allocate a FloatingWindow; freshly-constructed
//      DockWidgets that were never attached refuse to float.
//   3. Stamp geometry + maximize on the new FloatingWindow.
//   4. Strip the first leaf from a copy of the tree, then materializeSplit
//      the remainder relative to the floating anchor — dockBesideOrInto
//      routes through addDockWidgetToContainingWindow so the rest of the
//      tree lands inside the float.
//
// Requires the MainWindow be already shown — without a realized window
// KDDW won't allocate the FloatingWindow at all. Production callers
// (Workspace) satisfy this since the main window is shown at app start;
// tests must call mainWindow->show() before fromJson if they care about
// floating-window construction.
void materializeFloatingWindow(const WindowNode &w,
                               KDDockWidgets::QtWidgets::MainWindow *main,
                               Corbomite::Workspace *workspace)
{
    const QString firstLeafId = findFirstLeafId(w.content);
    if (firstLeafId.isEmpty()) return;

    KDDockWidgets::QtWidgets::DockWidget *first = nullptr;
    Corbomite::WorkspaceLeaf *firstWl = nullptr;
    if (workspace) {
        firstWl = workspace->createLeafUnplaced(firstLeafId);
        first = firstWl->dockWidget();
    } else {
        first = new KDDockWidgets::QtWidgets::DockWidget(firstLeafId);
    }

    main->addDockWidget(first, KDDockWidgets::Location_OnRight);
    first->dockWidget()->setFloating(true);
    if (w.width > 0 && w.height > 0) {
        first->dockWidget()->setFloatingGeometry(
            QRect(w.x, w.y, w.width, w.height));
    }
    if (w.maximize) {
        if (auto *fw = first->dockWidget()->floatingWindow();
            fw && fw->view())
            fw->view()->showMaximized();
    }

    if (workspace && firstWl) {
        const QString anchorTabGroupId = Corbomite::Workspace::freshTabGroupId();
        workspace->setTabGroupOf(firstWl, anchorTabGroupId);
        if (auto *ln = findLeafNodeById(w.content, firstLeafId))
            applyLeafPayload(firstWl, *ln);
    }

    SplitNode trimmed = w.content;
    removeFirstLeaf(trimmed, firstLeafId);
    if (trimmed.tabsChildren.isEmpty() && trimmed.splitChildren.isEmpty())
        return;
    materializeSplit(trimmed, main, first, KDDockWidgets::Location_OnRight,
                     workspace);
}

// Build a SplitNode that mirrors the layout of a single floating window
// by parsing its multiSplitterLayout subtree from the LayoutSaver dump.
// Falls back to a flat tabs node when the LayoutSaver entry can't be
// located (defensive — should not happen for a live FloatingWindow).
SplitNode floatingWindowAsSplit(KDDockWidgets::Core::FloatingWindow *fw,
                                const QJsonObject &saverRoot,
                                Corbomite::Workspace *workspace)
{
    const QJsonObject entry = findFloatingWindowEntry(saverRoot, fw);
    if (!entry.isEmpty()) {
        return walkMultiSplitterLayout(
            entry.value(QStringLiteral("multiSplitterLayout")).toObject(),
            workspace);
    }
    qCWarning(lcWorkspaceSerializer)
        << "no LayoutSaver entry for FloatingWindow; falling back to flat shape";
    SplitNode root;
    root.direction = QStringLiteral("vertical");
    TabsNode tabs;
    for (auto *dw : fw->dockWidgets()) {
        LeafNode l;
        l.id = dw->uniqueName();
        l.viewType = QStringLiteral("empty");
        tabs.children.append(l);
    }
    root.addTabs(tabs);
    return root;
}

} // namespace

void fromJson(const QJsonObject &json,
              KDDockWidgets::QtWidgets::MainWindow *main,
              Workspace *workspace)
{
    auto installDefault = [&]() {
        // Default tree: one empty leaf in a single tabs node inside a
        // vertical root split.
        if (workspace) {
            auto *wl = workspace->createLeafUnplaced(
                QStringLiteral("default-empty-leaf"));
            main->addDockWidget(wl->dockWidget(),
                                 KDDockWidgets::Location_OnLeft);
            workspace->setTabGroupOf(wl, QString{});
        } else {
            auto *dw = new KDDockWidgets::QtWidgets::DockWidget(
                QStringLiteral("default-empty-leaf"));
            main->addDockWidget(dw, KDDockWidgets::Location_OnLeft);
        }
    };

    auto mainObj = json.value(QStringLiteral("main")).toObject();
    if (mainObj.isEmpty()) {
        // Missing 'main', or 'main' present but the wrong type — fall back
        // to the default tree rather than crash.
        installDefault();
        return;
    }

    auto rootSplit = parseSplit(mainObj);
    if (rootSplit.tabsChildren.isEmpty() && rootSplit.splitChildren.isEmpty()) {
        installDefault();
        return;
    }
    materializeSplit(rootSplit, main, /*relativeTo*/ nullptr,
                     KDDockWidgets::Location_OnLeft, workspace);

    auto floatingObj = json.value(QStringLiteral("floating")).toObject();
    for (auto v : floatingObj.value(QStringLiteral("children")).toArray()) {
        auto childObj = v.toObject();
        if (childObj.value(QStringLiteral("type")).toString()
            == QStringLiteral("window")) {
            materializeFloatingWindow(parseWindow(childObj), main, workspace);
        }
    }
}

QJsonObject toJson(KDDockWidgets::QtWidgets::MainWindow *main, Workspace *workspace)
{
    QJsonObject out;
    KDDockWidgets::LayoutSaver saver;
    const QJsonObject saverRoot =
        QJsonDocument::fromJson(saver.serializeLayout()).object();

    out[QStringLiteral("main")] = renderSplit(walkLayoutSaverTree(main, saverRoot, workspace));

    auto *registry = KDDockWidgets::DockRegistry::self();
    auto fws = registry->floatingWindows();
    if (!fws.isEmpty()) {
        QJsonObject floating;
        floating[QStringLiteral("type")] = QStringLiteral("floating");
        QJsonArray windows;
        for (auto *fw : fws) {
            QJsonObject windowObj =
                renderSplit(floatingWindowAsSplit(fw, saverRoot, workspace));
            // Tag the node as a window rather than a split for round-trip clarity.
            windowObj[QStringLiteral("type")] = QStringLiteral("window");
            // Stamp geometry + maximize from the live FloatingWindow so the
            // workspace.json round-trip preserves popout-window placement.
            const auto rect = fw->geometry();
            windowObj[QStringLiteral("x")] = rect.x();
            windowObj[QStringLiteral("y")] = rect.y();
            windowObj[QStringLiteral("width")] = rect.width();
            windowObj[QStringLiteral("height")] = rect.height();
            if (fw->view() && fw->view()->isMaximized())
                windowObj[QStringLiteral("maximize")] = true;
            windows.append(windowObj);
        }
        floating[QStringLiteral("children")] = windows;
        out[QStringLiteral("floating")] = floating;
    }

    // "active" and "lastOpenFiles" added in later tasks.
    return out;
}

} // namespace Corbomite::WorkspaceSerializer
