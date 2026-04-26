// libs/core/src/Workspace.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/Workspace.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/WorkspaceActiveLeafRouter.h"
#include "corbomite/core/WorkspaceFloating.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/WorkspaceRoot.h"
#include "corbomite/core/WorkspaceWindow.h"

#include <kddockwidgets/Config.h>
#include <kddockwidgets/KDDockWidgets.h>
#include <kddockwidgets/core/DockWidget.h>
#include <kddockwidgets/core/FloatingWindow.h>
#include <kddockwidgets/qtwidgets/DockWidget.h>
#include <kddockwidgets/qtwidgets/MainWindow.h>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QSignalBlocker>
#include <QWidget>

namespace Corbomite {

namespace {

QString uniqueNameFor(const QString &vaultId, const QString &leafId)
{
    return vaultId.isEmpty()
        ? leafId
        : QStringLiteral("%1:%2").arg(vaultId, leafId);
}

void ensureKddwInit()
{
    static bool initialized = false;
    if (initialized) return;
    initialized = true;
    KDDockWidgets::initFrontend(KDDockWidgets::FrontendType::QtWidgets);
    auto &cfg = KDDockWidgets::Config::self();
    cfg.setFlags(cfg.flags()
                 | KDDockWidgets::Config::Flag_AlwaysShowTabs
                 | KDDockWidgets::Config::Flag_AllowReorderTabs
                 | KDDockWidgets::Config::Flag_TabsHaveCloseButton);
}

QString generateTabGroupId()
{
    static const char chars[] = "0123456789abcdef";
    QString result;
    result.reserve(16);
    auto *rng = QRandomGenerator::global();
    for (int i = 0; i < 16; ++i)
        result.append(QLatin1Char(chars[rng->bounded(16)]));
    return result;
}

} // namespace

Workspace::Workspace(ViewRegistry *registry, QObject *parent)
    : Workspace(QString{}, registry, parent)
{
}

Workspace::Workspace(QString vaultId, ViewRegistry *registry, QObject *parent)
    : QObject(parent)
    , m_vaultId(std::move(vaultId))
    , m_registry(registry)
{
    ensureKddwInit();

    const QString mainName = QStringLiteral("corbomite:%1")
        .arg(m_vaultId.isEmpty() ? QStringLiteral("default") : m_vaultId);
    m_kddwMain = new KDDockWidgets::QtWidgets::MainWindow(
        mainName, KDDockWidgets::MainWindowOption_None,
        /*parent=*/nullptr);
    // The host (e.g. CorbomiteMDI::MainWindow) reparents m_kddwMain into its
    // own central widget hierarchy, so Qt's parent cleanup may destroy the
    // KDDW MainWindow before Workspace's destructor runs. When that happens
    // every leaf's m_dockWidget becomes dangling — release them before
    // ~WorkspaceLeaf's `delete m_dockWidget` attempts a double-free.
    connect(m_kddwMain, &QObject::destroyed, this, [this]() {
        m_kddwMain = nullptr;
        for (auto *leaf : m_leaves)
            leaf->releaseDockWidget();
        for (auto *child : children()) {
            if (auto *leaf = qobject_cast<WorkspaceLeaf *>(child))
                leaf->releaseDockWidget();
        }
    });

    // Per-pane focus routing. Promoted from a Cluster G inline lambda to
    // the named WorkspaceActiveLeafRouter class in Cluster Y Phase 6.1
    // (no behaviour change). The router is parented to `this` so it dies
    // with the workspace.
    new WorkspaceActiveLeafRouter(this);

    // Phase 6.3: re-emit the host MainWindow's QEvent::Resize as
    // Workspace::resize() so plugins can hook layout-size changes the
    // same way Obsidian's `Workspace.on("resize")` works.
    m_kddwMain->installEventFilter(this);

    // Phase 7.5: Obsidian-shape root + floating containers. Bookkeeping
    // shells; the actual KDDW MainWindow remains the substrate.
    m_rootSplit = new WorkspaceRoot(QStringLiteral("root"), this);
    m_floating = new WorkspaceFloating(this);
}

bool Workspace::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_kddwMain && event && event->type() == QEvent::Resize)
        Q_EMIT resize();
    return QObject::eventFilter(watched, event);
}

Workspace::~Workspace()
{
    // Snapshot-then-clear before deleting: leaf destruction triggers KDDW
    // DockWidget destructors which emit signals (isOpenChanged etc.) that
    // run lambdas re-entering Workspace state (m_leaves iteration in the
    // focus router). If m_leaves still contains the half-destructed leaf
    // pointers during qDeleteAll, those lambdas read freed memory.
    QVector<WorkspaceLeaf *> leavesCopy = m_leaves;
    m_leaves.clear();
    m_leavesById.clear();
    m_tabGroupOf.clear();
    m_activeLeaf = nullptr;
    qDeleteAll(leavesCopy);

    // Closed-but-pending leaves (closeLeaf path: deleteLater scheduled but
    // not yet processed) are still QObject children of `this`. Detach
    // their dock widgets before deleting the KDDW MainWindow — otherwise
    // ~MainWindow disposes of the docked widgets and ~QObject's own child
    // cleanup later double-frees them through ~WorkspaceLeaf's
    // `delete m_dockWidget`.
    for (auto *child : children()) {
        if (auto *leaf = qobject_cast<WorkspaceLeaf *>(child))
            leaf->releaseDockWidget();
    }

    delete m_kddwMain;
    m_kddwMain = nullptr;
    qDeleteAll(m_windows);
    m_windows.clear();
}

void Workspace::revealDockView(const QString &slug)
{
    Q_EMIT revealDockViewRequested(slug);
}

void Workspace::requestCommand(const QString &commandId)
{
    Q_EMIT commandRequested(commandId);
}

QString Workspace::vaultId() const { return m_vaultId; }

ViewRegistry *Workspace::viewRegistry() const { return m_registry; }

QWidget *Workspace::rootWidget() const { return m_kddwMain; }

WorkspaceLeaf *Workspace::activeLeaf() const { return m_activeLeaf; }

void Workspace::setActiveLeaf(WorkspaceLeaf *leaf)
{
    if (m_activeLeaf == leaf)
        return;
    // Layout-ready gate: while a workspace.json load is in flight, KDDW's
    // own substrate emits cascading isCurrentTabChanged signals that route
    // back through the host (tabSelectRequested → setActiveLeaf). Suppress
    // those so consumers see exactly one activeLeafChanged once the load
    // settles. Programmatic callers during a load (e.g. tests) can still
    // force-set m_activeLeaf directly inside Workspace.
    if (!m_layoutReady)
        return;
    m_activeLeaf = leaf;
    if (leaf) {
        leaf->updateActiveTime();
        // Make the substrate raise the leaf's tab so the visible UI matches.
        // No-op if the tab is already current.
        leaf->setAsCurrentTab();
    }
    Q_EMIT activeLeafChanged(leaf);
}

void Workspace::setLayoutReady(bool ready)
{
    if (m_layoutReady == ready)
        return;
    m_layoutReady = ready;
    if (ready)
        Q_EMIT layoutReady();
}

bool Workspace::isLayoutReady() const { return m_layoutReady; }

QStringList Workspace::lastOpenFiles() const { return m_lastOpenFiles; }

void Workspace::setLastOpenFiles(const QStringList &files) { m_lastOpenFiles = files; }

void Workspace::pushLastOpenFile(const QString &path)
{
    m_lastOpenFiles.removeAll(path);
    m_lastOpenFiles.prepend(path);
    if (m_lastOpenFiles.size() > 50)
        m_lastOpenFiles.removeLast();
}

void Workspace::registerLeaf(WorkspaceLeaf *leaf)
{
    if (!leaf || !leaf->dockWidget())
        return;
    auto *qtDw = leaf->dockWidget();
    // Re-namespace the dock widget's unique name now that we know the vault.
    if (auto *core = qtDw->dockWidget())
        core->setUniqueName(uniqueNameFor(m_vaultId, leaf->id()));

    m_leaves.append(leaf);
    m_leavesById.insert(leaf->id(), leaf);

    connect(leaf, &WorkspaceLeaf::pinnedChanged, this, [this, leaf](bool) {
        propagatePinToGroup(leaf);
    });
    wireLeafKddwSignals(leaf);
}

void Workspace::unregisterLeaf(WorkspaceLeaf *leaf)
{
    if (!leaf)
        return;
    m_leaves.removeOne(leaf);
    m_leavesById.remove(leaf->id());
    m_tabGroupOf.remove(leaf);
}

void Workspace::wireLeafKddwSignals(WorkspaceLeaf *leaf)
{
    auto *dw = leaf->dockWidget();
    if (!dw)
        return;

    // Re-emit "user clicked this tab" as Workspace::tabSelectRequested(leaf).
    // Programmatic setAsCurrentTab also triggers this, but setActiveLeaf's
    // m_activeLeaf == leaf early-return guards against feedback loops.
    connect(dw, &KDDockWidgets::QtWidgets::DockWidget::isCurrentTabChanged,
            this, [this, leaf](bool isCurrent) {
        if (isCurrent && m_leavesById.contains(leaf->id()))
            Q_EMIT tabSelectRequested(leaf);
    });

    // Re-emit "user closed this tab" as Workspace::tabCloseRequested(leaf).
    // Hosts (MainWindow) call closeLeaf in response.
    connect(dw, &KDDockWidgets::QtWidgets::DockWidget::isOpenChanged,
            this, [this, leaf](bool isOpen) {
        if (!isOpen && m_leavesById.contains(leaf->id()))
            Q_EMIT tabCloseRequested(leaf);
    });
}

WorkspaceLeaf *Workspace::createLeafInGroupOf(WorkspaceLeaf *sibling)
{
    auto *leaf = new WorkspaceLeaf(m_registry, this);
    registerLeaf(leaf);
    // Block signals on the new dock widget across the addDockWidget call:
    // KDDW marks the freshly-docked widget as the current tab and emits
    // isCurrentTabChanged(true), which would cascade through
    // wireLeafKddwSignals -> tabSelectRequested -> MainWindow ->
    // setActiveLeaf BEFORE the caller (e.g. openFileInWorkspace) has set
    // viewState on the leaf. The result would be an activeFileChanged
    // emission with empty path, blanking sidebar panels (BUG-20260425).
    {
        const QSignalBlocker blocker(leaf->dockWidget());
        if (sibling && sibling->dockWidget()) {
            sibling->dockWidget()->dockWidget()->addDockWidgetAsTab(
                leaf->dockWidget()->dockWidget());
            // Inherit the sibling's tab group identity.
            m_tabGroupOf.insert(leaf, m_tabGroupOf.value(sibling, QString{}));
        } else {
            m_kddwMain->addDockWidget(leaf->dockWidget(),
                                       KDDockWidgets::Location_OnRight);
            m_tabGroupOf.insert(leaf, generateTabGroupId());
        }
    }
    Q_EMIT layoutChanged();
    return leaf;
}

WorkspaceLeaf *Workspace::createLeafInActiveGroup()
{
    return createLeafInGroupOf(m_activeLeaf);
}

void Workspace::closeLeaf(WorkspaceLeaf *leaf)
{
    if (!leaf || !m_leavesById.contains(leaf->id()))
        return;

    UndoEntry entry;
    entry.leafId = leaf->id();
    entry.state = leaf->getViewState();
    entry.eState = leaf->getEphemeralState();
    entry.leafHistory = leaf->history();
    entry.pinned = leaf->pinned();
    entry.group = leaf->group();
    m_undoHistory.prepend(entry);
    if (m_undoHistory.size() > UndoCap)
        m_undoHistory.removeLast();

    if (m_activeLeaf == leaf)
        m_activeLeaf = nullptr;

    Q_EMIT leafClosed(leaf);
    unregisterLeaf(leaf);
    leaf->deleteLater();   // ~WorkspaceLeaf disposes its DockWidget

    if (!m_activeLeaf && !m_leaves.isEmpty())
        setActiveLeaf(m_leaves.first());

    Q_EMIT layoutChanged();
}

bool Workspace::canUndoCloseLeaf() const { return !m_undoHistory.isEmpty(); }

void Workspace::undoCloseLeaf()
{
    if (m_undoHistory.isEmpty())
        return;
    UndoEntry entry = m_undoHistory.takeFirst();

    auto *leaf = createLeafInActiveGroup();
    if (!leaf)
        return;
    leaf->setId(entry.leafId);
    leaf->setPinned(entry.pinned);
    leaf->setGroup(entry.group);
    if (!entry.state.isEmpty())
        leaf->setViewState(entry.state);

    setActiveLeaf(leaf);
    Q_EMIT layoutChanged();
}

WorkspaceLeaf *Workspace::splitLeaf(WorkspaceLeaf *source, Qt::Orientation direction)
{
    if (!source || !source->dockWidget())
        return nullptr;
    auto *leaf = new WorkspaceLeaf(m_registry, this);
    registerLeaf(leaf);
    auto location = direction == Qt::Horizontal
        ? KDDockWidgets::Location_OnRight
        : KDDockWidgets::Location_OnBottom;
    {
        const QSignalBlocker blocker(leaf->dockWidget());
        source->dockWidget()->dockWidget()->addDockWidgetToContainingWindow(
            leaf->dockWidget()->dockWidget(), location,
            source->dockWidget()->dockWidget());
    }
    // Splits put the new leaf in a fresh tab group.
    m_tabGroupOf.insert(leaf, generateTabGroupId());
    Q_EMIT layoutChanged();
    return leaf;
}

WorkspaceLeaf *Workspace::duplicateLeaf(WorkspaceLeaf *leaf, Qt::Orientation direction)
{
    if (!leaf)
        return nullptr;

    QJsonObject state = leaf->getViewState();
    QJsonObject eState = leaf->getEphemeralState();
    LeafHistory hist = leaf->history();
    bool wasPinned = leaf->pinned();
    QString grp = leaf->group();

    auto *newLeaf = splitLeaf(leaf, direction);
    if (!newLeaf)
        return nullptr;

    if (!state.isEmpty())
        newLeaf->setViewState(state);
    if (!eState.isEmpty())
        newLeaf->setEphemeralState(eState);
    newLeaf->setPinned(wasPinned);
    newLeaf->setGroup(grp);
    newLeaf->history() = hist;

    setActiveLeaf(newLeaf);
    return newLeaf;
}

void Workspace::setLinkResolver(LinkResolverFn resolver)
{
    m_linkResolver = std::move(resolver);
}

bool Workspace::openLinkText(const QString &linktext,
                              const QString &source,
                              LeafMode mode,
                              const QJsonObject &opts)
{
    // Split linktext into <path>(#heading|^block)? — Obsidian-style.
    // Anchor is the first occurrence of either delimiter.
    int hashIdx = linktext.indexOf(QLatin1Char('#'));
    int caretIdx = linktext.indexOf(QLatin1Char('^'));
    int anchorIdx = -1;
    if (hashIdx >= 0 && caretIdx >= 0)
        anchorIdx = std::min(hashIdx, caretIdx);
    else if (hashIdx >= 0)
        anchorIdx = hashIdx;
    else if (caretIdx >= 0)
        anchorIdx = caretIdx;

    QString path;
    QString subpath;
    if (anchorIdx >= 0) {
        path = linktext.left(anchorIdx);
        subpath = linktext.mid(anchorIdx);
    } else {
        path = linktext;
    }

    // Run the installed resolver (if any). Empty result preserves input.
    if (m_linkResolver) {
        const QString resolved = m_linkResolver(path, source);
        if (!resolved.isEmpty())
            path = resolved;
    }

    auto *leaf = getLeaf(mode);
    if (!leaf)
        return false;

    QJsonObject viewState;
    viewState[QStringLiteral("type")] = QStringLiteral("markdown");
    QJsonObject stateObj;
    stateObj[QStringLiteral("file")] = path;
    viewState[QStringLiteral("state")] = stateObj;
    leaf->setViewState(viewState);

    if (opts.contains(QStringLiteral("eState"))) {
        leaf->setEphemeralState(
            opts.value(QStringLiteral("eState")).toObject());
    } else if (!subpath.isEmpty()) {
        QJsonObject derived;
        derived[QStringLiteral("subpath")] = subpath;
        leaf->setEphemeralState(derived);
    }

    setActiveLeaf(leaf);
    return true;
}

WorkspaceLeaf *Workspace::getLeaf(LeafMode mode, LeafDirection dir)
{
    auto *active = m_activeLeaf;
    switch (mode) {
    case LeafMode::Same:
        return active ? active : createLeafInActiveGroup();
    case LeafMode::Tab:
        return createLeafInGroupOf(active);
    case LeafMode::Split: {
        if (!active)
            return createLeafInActiveGroup();
        const auto orient = dir == LeafDirection::Horizontal
            ? Qt::Horizontal
            : Qt::Vertical;
        return splitLeaf(active, orient);
    }
    case LeafMode::Window: {
        auto *leaf = createLeafInActiveGroup();
        if (!leaf)
            return nullptr;
        popoutLeaf(leaf);
        return leaf;
    }
    }
    return nullptr;
}

WorkspaceWindow *Workspace::popoutLeaf(WorkspaceLeaf *leaf)
{
    if (!leaf || !leaf->dockWidget())
        return nullptr;

    // Detach the leaf's DockWidget into a fresh KDDW FloatingWindow. KDDW
    // requires the host MainWindow be realised (shown) before it will spawn
    // a FloatingWindow; production callers always satisfy that since the
    // host shows MainWindow at app start. Tests must show() the MainWindow
    // explicitly — see materializeFloatingWindow in WorkspaceSerializer for
    // the corresponding serializer-side note.
    leaf->dockWidget()->setFloating(true);

    // Phase 6.3: re-emit floating-window topology changes as
    // windowFrameChange so plugins can react. Hook destroy on the
    // FloatingWindow KDDW just spawned, then signal the create.
    if (auto *fw = leaf->dockWidget()->dockWidget()->floatingWindow()) {
        connect(fw, &QObject::destroyed, this, [this]() {
            Q_EMIT windowFrameChange();
        });
    }

    auto *win = new WorkspaceWindow(this);
    m_windows.append(win);
    if (m_floating) m_floating->addWindow(win);
    Q_EMIT layoutChanged();
    Q_EMIT windowFrameChange();
    return win;
}

void Workspace::reparentToMain(WorkspaceWindow *window)
{
    if (!window)
        return;
    m_windows.removeOne(window);
    if (m_floating) m_floating->removeWindow(window);
    delete window;
    Q_EMIT layoutChanged();
    Q_EMIT windowFrameChange();
}

QVector<WorkspaceWindow *> Workspace::windows() const { return m_windows; }

namespace {
QVector<WorkspaceLeaf *> tabSiblings(const QHash<WorkspaceLeaf *, QString> &tabGroupOf,
                                     const QVector<WorkspaceLeaf *> &leaves,
                                     WorkspaceLeaf *leaf)
{
    QVector<WorkspaceLeaf *> result;
    if (!leaf)
        return result;
    QString gid = tabGroupOf.value(leaf);
    if (gid.isEmpty())
        return result;
    for (auto *l : leaves) {
        if (tabGroupOf.value(l) == gid)
            result.append(l);
    }
    return result;
}
} // namespace

WorkspaceLeaf *Workspace::nextLeafInActiveGroup() const
{
    auto group = tabSiblings(m_tabGroupOf, m_leaves, m_activeLeaf);
    if (group.size() < 2)
        return nullptr;
    int idx = group.indexOf(m_activeLeaf);
    if (idx < 0)
        return nullptr;
    return group[(idx + 1) % group.size()];
}

WorkspaceLeaf *Workspace::previousLeafInActiveGroup() const
{
    auto group = tabSiblings(m_tabGroupOf, m_leaves, m_activeLeaf);
    if (group.size() < 2)
        return nullptr;
    int idx = group.indexOf(m_activeLeaf);
    if (idx < 0)
        return nullptr;
    int n = group.size();
    return group[(idx + n - 1) % n];
}

int Workspace::leafIndexInGroup(WorkspaceLeaf *leaf) const
{
    auto group = tabSiblings(m_tabGroupOf, m_leaves, leaf);
    return group.indexOf(leaf);
}

int Workspace::leafCountInGroup(WorkspaceLeaf *leaf) const
{
    return tabSiblings(m_tabGroupOf, m_leaves, leaf).size();
}

void Workspace::closeOtherLeavesInGroupOf(WorkspaceLeaf *leaf)
{
    auto group = tabSiblings(m_tabGroupOf, m_leaves, leaf);
    QVector<WorkspaceLeaf *> targets;
    for (auto *l : group) {
        if (l != leaf)
            targets.append(l);
    }
    for (auto *l : targets)
        closeLeaf(l);
}

void Workspace::closeLeavesToRightOf(WorkspaceLeaf *leaf)
{
    auto group = tabSiblings(m_tabGroupOf, m_leaves, leaf);
    int pivot = group.indexOf(leaf);
    if (pivot < 0)
        return;
    QVector<WorkspaceLeaf *> targets;
    for (int i = pivot + 1; i < group.size(); ++i)
        targets.append(group[i]);
    for (auto *l : targets)
        closeLeaf(l);
}

WorkspaceLeaf *Workspace::findLeafById(const QString &id) const
{
    return m_leavesById.value(id);
}

QVector<WorkspaceLeaf *> Workspace::allLeaves() const
{
    return m_leaves;
}

QVector<WorkspaceLeaf *> Workspace::groupMembers(const QString &groupId) const
{
    QVector<WorkspaceLeaf *> result;
    if (groupId.isEmpty())
        return result;
    for (auto *leaf : m_leaves) {
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

WorkspaceLeaf *Workspace::findOrCreateUnpinnedLeafInGroupOf(WorkspaceLeaf *sibling)
{
    auto group = tabSiblings(m_tabGroupOf, m_leaves, sibling ? sibling : m_activeLeaf);
    for (auto *leaf : group) {
        if (!leaf->pinned())
            return leaf;
    }
    return createLeafInGroupOf(sibling);
}

QJsonObject Workspace::serialize() const
{
    // Obsidian-shape JSON: {"main": {type:split, children:[{type:tabs,
    // currentTab, children:[{type:leaf, id, state}]}]}, "active", "lastOpenFiles"}.
    // We emit one tabs group per distinct tabGroupId in m_tabGroupOf,
    // wrapped in a single root split. This isn't a full tree
    // reconstruction (KDDW's nested splits are flattened), but it satisfies
    // the round-trip contract that Workspace owns: leaf identity + per-leaf
    // state survive serialize -> deserialize. Phase 5/6 introduces a
    // proper tree walker that round-trips KDDW's nested split structure.

    QJsonObject json;

    QJsonObject mainSplit;
    mainSplit[QStringLiteral("type")] = QStringLiteral("split");
    mainSplit[QStringLiteral("direction")] = QStringLiteral("vertical");

    // Bucket leaves by their tabGroupId, preserving insertion order both
    // across groups and within a group.
    QVector<QString> orderedGroupIds;
    QHash<QString, QVector<WorkspaceLeaf *>> groupBuckets;
    for (auto *leaf : m_leaves) {
        QString gid = m_tabGroupOf.value(leaf);
        if (gid.isEmpty())
            gid = leaf->id();   // singleton fallback
        if (!groupBuckets.contains(gid)) {
            orderedGroupIds.append(gid);
            groupBuckets.insert(gid, {});
        }
        groupBuckets[gid].append(leaf);
    }

    QJsonArray children;
    for (const QString &gid : orderedGroupIds) {
        QJsonObject tabs;
        tabs[QStringLiteral("type")] = QStringLiteral("tabs");
        QJsonArray leafChildren;
        int currentTab = 0;
        const auto &bucket = groupBuckets.value(gid);
        for (int i = 0; i < bucket.size(); ++i) {
            auto *leaf = bucket[i];
            leafChildren.append(leaf->serialize());
            if (leaf == m_activeLeaf)
                currentTab = i;
        }
        tabs[QStringLiteral("currentTab")] = currentTab;
        tabs[QStringLiteral("children")] = leafChildren;
        children.append(tabs);
    }
    mainSplit[QStringLiteral("children")] = children;
    json[QStringLiteral("main")] = mainSplit;

    json[QStringLiteral("active")] = m_activeLeaf
        ? m_activeLeaf->id()
        : QString{};

    if (!m_lastOpenFiles.isEmpty()) {
        QJsonArray files;
        for (const auto &f : m_lastOpenFiles)
            files.append(f);
        json[QStringLiteral("lastOpenFiles")] = files;
    }

    return json;
}

namespace {

void collectLeafObjects(const QJsonObject &node,
                        QVector<QPair<QString, QJsonObject>> &out)
{
    QString type = node[QStringLiteral("type")].toString();
    if (type == QStringLiteral("leaf")) {
        // Empty group sentinel — separates tabs containers in the flat
        // out list. The string identifies the group; first leaf with this
        // group becomes the new tab group anchor.
        out.append({QString{}, node});
        return;
    }
    if (type == QStringLiteral("tabs")) {
        // Generate a fresh group id per tabs node so leaves in the same
        // tabs JSON node map to the same Workspace tab group.
        static int counter = 0;
        QString gid = QStringLiteral("g_%1").arg(++counter);
        for (const auto &cv : node[QStringLiteral("children")].toArray()) {
            const QJsonObject co = cv.toObject();
            if (co[QStringLiteral("type")].toString() == QStringLiteral("leaf"))
                out.append({gid, co});
        }
        return;
    }
    if (type == QStringLiteral("split")) {
        for (const auto &cv : node[QStringLiteral("children")].toArray())
            collectLeafObjects(cv.toObject(), out);
    }
}

} // namespace

void Workspace::deserialize(const QJsonObject &json)
{
    // Suppress activeLeafChanged emissions while we materialize the layout;
    // KDDW's own substrate signals (isCurrentTabChanged) would otherwise
    // route back through host wiring and fire activeLeafChanged once per
    // dock-widget creation. The gate is lifted below once the active leaf
    // has been resolved, with `layoutReady()` emitted exactly once.
    setLayoutReady(false);

    // Drop existing leaves; KDDW MainWindow reused.
    qDeleteAll(m_leaves);
    m_leaves.clear();
    m_leavesById.clear();
    m_tabGroupOf.clear();
    m_activeLeaf = nullptr;
    m_undoHistory.clear();

    QVector<QPair<QString, QJsonObject>> leafEntries;
    if (json.contains(QStringLiteral("main")))
        collectLeafObjects(json[QStringLiteral("main")].toObject(), leafEntries);

    // Materialize each leaf, anchoring same-group leaves to the first leaf
    // already created in that group.
    QHash<QString, WorkspaceLeaf *> firstInGroup;
    for (const auto &entry : leafEntries) {
        const QString &gid = entry.first;
        const QJsonObject &lo = entry.second;
        WorkspaceLeaf *anchor = gid.isEmpty()
            ? nullptr
            : firstInGroup.value(gid);
        auto *leaf = createLeafInGroupOf(anchor);
        if (!leaf)
            continue;
        QString leafId = lo[QStringLiteral("id")].toString();
        if (!leafId.isEmpty()) {
            // createLeafInGroupOf registered the leaf with its
            // auto-generated id; rewire m_leavesById to the JSON-side id
            // so findLeafById(activeId) below resolves correctly.
            const QString autoId = leaf->id();
            m_leavesById.remove(autoId);
            leaf->setId(leafId);
            m_leavesById.insert(leafId, leaf);
        }
        if (lo[QStringLiteral("pinned")].toBool())
            leaf->setPinned(true);
        QString grp = lo[QStringLiteral("group")].toString();
        if (!grp.isEmpty())
            leaf->setGroup(grp);
        QJsonObject viewState = lo[QStringLiteral("state")].toObject();
        if (!viewState.isEmpty())
            leaf->setViewState(viewState);

        if (!gid.isEmpty() && !firstInGroup.contains(gid))
            firstInGroup.insert(gid, leaf);
    }

    QString activeId = json[QStringLiteral("active")].toString();
    if (!activeId.isEmpty())
        m_activeLeaf = findLeafById(activeId);
    if (!m_activeLeaf && !m_leaves.isEmpty())
        m_activeLeaf = m_leaves.first();

    m_lastOpenFiles.clear();
    for (const auto &v : json[QStringLiteral("lastOpenFiles")].toArray())
        m_lastOpenFiles.append(v.toString());

    // Defer non-active, non-currentTab leaves so they don't materialize
    // their View until the user focuses them. The active leaf and each
    // tab group's currentTab leaf load eagerly.
    QHash<QString, WorkspaceLeaf *> currentInGroup;
    for (auto *leaf : m_leaves) {
        QString gid = m_tabGroupOf.value(leaf);
        if (gid.isEmpty()) continue;
        if (!currentInGroup.contains(gid))
            currentInGroup.insert(gid, leaf);  // first as default current
    }
    for (auto *leaf : m_leaves) {
        if (leaf == m_activeLeaf)
            continue;
        QString gid = m_tabGroupOf.value(leaf);
        if (currentInGroup.value(gid) == leaf)
            continue;

        auto state = leaf->getViewState();
        QString icon = state[QStringLiteral("icon")].toString();
        QString title = state[QStringLiteral("title")].toString();
        if (icon.isEmpty()) icon = QStringLiteral("document");
        if (title.isEmpty()) title = QStringLiteral("Untitled");
        leaf->setDeferred(true, icon, title);
    }
    if (m_activeLeaf && m_activeLeaf->isDeferred())
        m_activeLeaf->loadIfDeferred();

    Q_EMIT layoutChanged();
    setLayoutReady(true);

    // Layout settled: fire one activeLeafChanged for the resolved active
    // leaf so consumers that subscribed before the load (or that wait on
    // layoutReady) see exactly one signal for the post-load state. The
    // ping-through-null hop forces setActiveLeaf past its identity gate.
    if (auto *target = m_activeLeaf) {
        m_activeLeaf = nullptr;
        setActiveLeaf(target);
    }
}

void Workspace::readWorkspaceJson(const QString &vaultPath)
{
    QString path = vaultPath + QStringLiteral("/.obsidian/workspace.json");
    QFile f(path);
    auto installDefaultAndReady = [this]() {
        // Force a false → true gate transition even on the no-vault fallback
        // so layoutReady fires exactly once for downstream consumers.
        setLayoutReady(false);
        resetToDefaultLayout();
        auto *leaf = createLeafInActiveGroup();
        setLayoutReady(true);
        if (leaf)
            setActiveLeaf(leaf);
    };
    if (!f.open(QIODevice::ReadOnly)) {
        installDefaultAndReady();
        return;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError) {
        installDefaultAndReady();
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

void Workspace::resetToDefaultLayout()
{
    qDeleteAll(m_leaves);
    m_leaves.clear();
    m_leavesById.clear();
    m_tabGroupOf.clear();
    m_activeLeaf = nullptr;
    m_undoHistory.clear();
    Q_EMIT layoutChanged();
}

} // namespace Corbomite
