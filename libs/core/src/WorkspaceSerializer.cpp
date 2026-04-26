// SPDX-FileCopyrightText: 2026 Clinton Molyneux <clinton@concernednetizen.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "WorkspaceSerializer.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

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
    // Variant-like: a node has EITHER splitChildren OR tabsChildren, not both.
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
            n.tabsChildren.append(parseTabs(childObj));
        } else if (type == QStringLiteral("split")) {
            n.splitChildren.append(parseSplit(childObj));
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
    for (const auto &c : n.splitChildren) kids.append(renderSplit(c));
    for (const auto &c : n.tabsChildren) kids.append(renderTabs(c));
    o[QStringLiteral("children")] = kids;
    return o;
}

// Build a LeafNode for a dock widget identified by uniqueName, falling back
// to the sidecar (and finally to a placeholder) when state isn't available.
// Phase 4 (workspace=non-null) wires this through Workspace::findLeafById.
LeafNode buildLeafNode(const QString &uniqueName)
{
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
//
// Returns either a SplitNode or a TabsNode wrapped in a holder; the caller
// distinguishes by checking which of `out.tabsChildren`/`out.splitChildren`
// the function appended to. We use two helpers to avoid that ambiguity.
void walkLayoutNode(const QJsonObject &layoutNode,
                    const QJsonObject &frames,
                    SplitNode &parent);

void walkLayoutContainer(const QJsonObject &containerNode,
                         const QJsonObject &frames,
                         SplitNode &out)
{
    const int orient = containerNode.value(QStringLiteral("orientation")).toInt();
    out.direction = (orient == 2)
        ? QStringLiteral("vertical")
        : QStringLiteral("horizontal");
    const auto children = containerNode.value(QStringLiteral("children")).toArray();
    for (const auto &v : children)
        walkLayoutNode(v.toObject(), frames, out);
}

void walkLayoutNode(const QJsonObject &layoutNode,
                    const QJsonObject &frames,
                    SplitNode &parent)
{
    if (layoutNode.value(QStringLiteral("isContainer")).toBool()) {
        SplitNode child;
        walkLayoutContainer(layoutNode, frames, child);
        parent.splitChildren.append(child);
        return;
    }
    // Leaf-of-tree (group reference). Resolve the frame via guestId.
    const QString guestId = layoutNode.value(QStringLiteral("guestId")).toString();
    const QJsonObject frame = frames.value(guestId).toObject();
    TabsNode tabs;
    tabs.currentTab = frame.value(QStringLiteral("currentTabIndex")).toInt(0);
    const auto frameDws = frame.value(QStringLiteral("dockWidgets")).toArray();
    for (const auto &v : frameDws)
        tabs.children.append(buildLeafNode(v.toString()));
    if (!tabs.children.isEmpty()
        && stackedSidecar().value(tabs.children.first().id, false)) {
        tabs.stacked = true;
    }
    parent.tabsChildren.append(tabs);
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

// Walk the KDDW MainWindow tree by parsing LayoutSaver::serializeLayout()
// JSON output. Replaces the Phase 3 flat walker. Schema reference:
// docs/superpowers/specs/2026-04-26-kddw-layoutsaver-shape.md.
SplitNode walkLayoutSaverTree(KDDockWidgets::QtWidgets::MainWindow *main,
                               const QJsonObject &saverRoot)
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
    const QJsonObject msl = mwEntry.value(QStringLiteral("multiSplitterLayout"))
                                    .toObject();
    const QJsonObject frames = msl.value(QStringLiteral("frames")).toObject();
    const QJsonObject rootLayout = msl.value(QStringLiteral("layout")).toObject();
    if (rootLayout.isEmpty()) return root;

    // The root is itself a container; copy its orientation+children into
    // `root` rather than nesting an additional split level.
    walkLayoutContainer(rootLayout, frames, root);
    return root;
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

// Materialize one TabsNode: create N DockWidgets; the first becomes a new
// container (placed at `location` relative to `relativeTo` if set, otherwise
// docked into `main` directly); subsequent leaves are tabbed onto the first.
// Returns the first DockWidget, which downstream callers use as the anchor
// for further sibling placements.
KDDockWidgets::QtWidgets::DockWidget *
materializeTabs(const TabsNode &tabs,
                KDDockWidgets::QtWidgets::MainWindow *main,
                KDDockWidgets::QtWidgets::DockWidget *relativeTo,
                KDDockWidgets::Location location)
{
    KDDockWidgets::QtWidgets::DockWidget *first = nullptr;
    for (const auto &leaf : tabs.children) {
        auto *dw = new KDDockWidgets::QtWidgets::DockWidget(leaf.id);
        if (!first) {
            main->addDockWidget(dw, location, relativeTo);
            first = dw;
        } else {
            first->addDockWidgetAsTab(dw);
        }
    }
    if (first && tabs.currentTab > 0 && tabs.currentTab < tabs.children.size()) {
        if (auto *current = KDDockWidgets::Core::DockWidget::byName(
                tabs.children[tabs.currentTab].id)) {
            current->setAsCurrentTab();
        }
    }
    return first;
}

// Forward declaration — materializeSplit recurses on itself.
KDDockWidgets::QtWidgets::DockWidget *
materializeSplit(const SplitNode &split,
                 KDDockWidgets::QtWidgets::MainWindow *main,
                 KDDockWidgets::QtWidgets::DockWidget *relativeTo,
                 KDDockWidgets::Location baseLocation);

KDDockWidgets::QtWidgets::DockWidget *
materializeSplit(const SplitNode &split,
                 KDDockWidgets::QtWidgets::MainWindow *main,
                 KDDockWidgets::QtWidgets::DockWidget *relativeTo,
                 KDDockWidgets::Location baseLocation)
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

    for (const auto &childTabs : split.tabsChildren) {
        placeChild([&](KDDockWidgets::Location loc,
                       KDDockWidgets::QtWidgets::DockWidget *rel) {
            return materializeTabs(childTabs, main, rel, loc);
        });
    }
    for (const auto &childSplit : split.splitChildren) {
        placeChild([&](KDDockWidgets::Location loc,
                       KDDockWidgets::QtWidgets::DockWidget *rel) {
            return materializeSplit(childSplit, main, rel, loc);
        });
    }

    return firstAnchor;
}

// Materialize a floating window: create the leaves of its first tabs child,
// float the first leaf at the given geometry, then tab the rest onto it.
// Phase 3 only handles the single-tabs case; nested splits inside floating
// windows would extend this similarly to materializeSplit.
//
// KDDW won't materialize a FloatingWindow for a freshly constructed,
// never-attached DockWidget because the dock has no prior layout.  We dock
// the first leaf into the supplied MainWindow as a momentary hold, then
// setFloating(true) detaches it into a fresh FloatingWindow.  Requires the
// MainWindow be already shown — without a realized window KDDW won't
// allocate the FloatingWindow at all.  Production callers (Workspace) will
// always satisfy this since the main window is shown at app start; tests
// must call mainWindow->show() before fromJson if they care about
// floating-window construction.
void materializeFloatingWindow(const WindowNode &w,
                               KDDockWidgets::QtWidgets::MainWindow *main)
{
    if (w.content.tabsChildren.isEmpty()) return;
    const auto &tabs = w.content.tabsChildren.first();
    if (tabs.children.isEmpty()) return;

    KDDockWidgets::QtWidgets::DockWidget *first = nullptr;
    for (const auto &leaf : tabs.children) {
        auto *dw = new KDDockWidgets::QtWidgets::DockWidget(leaf.id);
        if (!first) {
            main->addDockWidget(dw, KDDockWidgets::Location_OnRight);
            dw->dockWidget()->setFloating(true);
            if (w.width > 0 && w.height > 0) {
                dw->dockWidget()->setFloatingGeometry(
                    QRect(w.x, w.y, w.width, w.height));
            }
            if (w.maximize) {
                if (auto *fw = dw->dockWidget()->floatingWindow();
                    fw && fw->view())
                    fw->view()->showMaximized();
            }
            first = dw;
        } else {
            first->addDockWidgetAsTab(dw);
        }
    }
}

// Build a SplitNode that mirrors the layout of a single floating window.
// Phase 3: collapse the floating window's contents into one tabs child
// (matching walkKddwTreeSimple's main-area approximation).
SplitNode floatingWindowAsSplit(KDDockWidgets::Core::FloatingWindow *fw)
{
    SplitNode root;
    root.direction = QStringLiteral("vertical");
    TabsNode tabs;
    for (auto *dw : fw->dockWidgets()) {
        LeafNode l;
        l.id = dw->uniqueName();
        l.viewType = QStringLiteral("empty");
        tabs.children.append(l);
    }
    root.tabsChildren.append(tabs);
    return root;
}

} // namespace

void fromJson(const QJsonObject &json,
              KDDockWidgets::QtWidgets::MainWindow *main,
              Workspace * /*workspace*/)
{
    auto installDefault = [&]() {
        // Default tree: one empty leaf in a single tabs node inside a
        // vertical root split.
        auto *dw = new KDDockWidgets::QtWidgets::DockWidget(
            QStringLiteral("default-empty-leaf"));
        main->addDockWidget(dw, KDDockWidgets::Location_OnLeft);
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
                     KDDockWidgets::Location_OnLeft);

    auto floatingObj = json.value(QStringLiteral("floating")).toObject();
    for (auto v : floatingObj.value(QStringLiteral("children")).toArray()) {
        auto childObj = v.toObject();
        if (childObj.value(QStringLiteral("type")).toString()
            == QStringLiteral("window")) {
            materializeFloatingWindow(parseWindow(childObj), main);
        }
    }
}

QJsonObject toJson(KDDockWidgets::QtWidgets::MainWindow *main, Workspace * /*workspace*/)
{
    QJsonObject out;
    KDDockWidgets::LayoutSaver saver;
    const QJsonObject saverRoot =
        QJsonDocument::fromJson(saver.serializeLayout()).object();

    out[QStringLiteral("main")] = renderSplit(walkLayoutSaverTree(main, saverRoot));

    auto *registry = KDDockWidgets::DockRegistry::self();
    auto fws = registry->floatingWindows();
    if (!fws.isEmpty()) {
        QJsonObject floating;
        floating[QStringLiteral("type")] = QStringLiteral("floating");
        QJsonArray windows;
        for (auto *fw : fws) {
            QJsonObject windowObj = renderSplit(floatingWindowAsSplit(fw));
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
