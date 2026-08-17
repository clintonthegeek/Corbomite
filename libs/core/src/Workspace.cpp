// libs/core/src/Workspace.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/Workspace.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/WorkspaceActiveLeafRouter.h"
#include "corbomite/core/WorkspaceFloating.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/WorkspaceRoot.h"
#include "corbomite/core/WorkspaceWindow.h"
#include "WorkspaceSerializer.h"

#include <kddockwidgets/core/DockRegistry.h>
#include <kddockwidgets/core/Group.h>
#include <kddockwidgets/core/Layout.h>
#include <kddockwidgets/core/MainWindow.h>

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
#include <QPointer>
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

// Live tab-group enumeration via KDDW. Source of truth for "which leaves
// share a tab group right now" — tolerant of user drag-tab-to-other-group
// because it queries DockRegistry directly rather than the cached
// m_tabGroupOf which is only updated on programmatic create.
//
// m_tabGroupOf still exists, but it is now an opaque identifier used by
// the serializer to key per-group stacked-state — it is no longer
// authoritative for membership.
QVector<WorkspaceLeaf *> liveTabSiblings(const QVector<WorkspaceLeaf *> &leaves,
                                          WorkspaceLeaf *leaf)
{
    QVector<WorkspaceLeaf *> result;
    if (!leaf)
        return result;
    auto *qd = leaf->dockWidget();
    if (!qd)
        return result;
    auto *targetCore = qd->dockWidget();
    if (!targetCore)
        return result;

    QHash<KDDockWidgets::Core::DockWidget *, WorkspaceLeaf *> byCore;
    byCore.reserve(leaves.size());
    for (auto *l : leaves) {
        if (auto *q = l->dockWidget())
            if (auto *c = q->dockWidget())
                byCore.insert(c, l);
    }

    for (auto *group : KDDockWidgets::DockRegistry::self()->groups()) {
        const auto dws = group->dockWidgets();
        bool contains = false;
        for (auto *dw : dws) {
            if (dw == targetCore) { contains = true; break; }
        }
        if (!contains)
            continue;
        for (auto *dw : dws) {
            if (auto *l = byCore.value(dw, nullptr))
                result.append(l);
        }
        return result;
    }
    return result;
}

} // namespace

QString Workspace::freshTabGroupId() { return generateTabGroupId(); }

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
    // destroyLeaves unregisters each leaf immediately before deleting it
    // (Phase L1 / A1-A2), so a KDDW DockWidget destructor signal firing
    // re-entrantly (isOpenChanged etc., which used to run lambdas that read
    // freed memory out of a stale m_leaves/m_leavesById) always sees
    // already-consistent Workspace state rather than a snapshot cleared all
    // at once up front.
    destroyLeaves(m_leaves, TeardownMode::Immediate);

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
        // Realize deferred leaves on activation. Session restore marks every
        // non-currently-visible-in-its-group leaf as deferred (Workspace.cpp
        // §loadWorkspaceJson) so we don't construct a view for every tab on
        // startup; activation is when the user pays the cost. Without this
        // call, clicking a deferred tab raised the tab in the UI but left
        // it with no central widget (surfaced 2026-05-21 by Corbomite
        // session-restore dogfood). The two existing loadIfDeferred call
        // sites (MainWindow::openFileInWorkspace, WorkspaceController::
        // openFile) cover the "open file by path" entry points but missed
        // the "click on already-open tab" path.
        leaf->loadIfDeferred();
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

void Workspace::destroyLeaves(QVector<WorkspaceLeaf *> leaves, TeardownMode mode)
{
    // Iterate a value-copy of the caller's vector: unregisterLeaf() mutates
    // m_leaves via removeOne(), so iterating m_leaves itself here would be
    // iterator-invalidation. Passing by value (rather than const&) forces
    // every call site to hand over a snapshot rather than accidentally
    // aliasing m_leaves.
    for (auto *leaf : leaves) {
        if (!leaf)
            continue;

        // Unregister first. This is the actual A1 fix: previously
        // deserialize/resetToDefaultLayout deleted every leaf via
        // qDeleteAll(m_leaves) and only cleared m_leaves/m_leavesById
        // afterwards, so a KDDW signal firing re-entrantly off leaf N's
        // destruction (isCurrentTabChanged/isOpenChanged cascades) could
        // still find leaf N+1..end "registered" while they were mid-batch-
        // delete. Unregistering each leaf immediately before it is touched
        // makes "is this leaf still alive" match "is it still registered"
        // at every point in the loop, not just at the start and end.
        unregisterLeaf(leaf);
        if (m_activeLeaf == leaf)
            m_activeLeaf = nullptr;

        // Defense in depth on top of A3 (wireLeafKddwSignals now uses `leaf`
        // itself as the connect context, so those connections auto-
        // disconnect structurally when the leaf dies). Cut them explicitly
        // here too so this primitive doesn't depend on every future signal
        // wired against a leaf remembering to use the right context object.
        if (auto *dw = leaf->dockWidget())
            dw->disconnect(leaf);

        if (mode == TeardownMode::Immediate)
            delete leaf;
        else
            leaf->deleteLater();
    }
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
    //
    // A3 fix: `leaf` (not `this`) is the connect context object, so Qt
    // auto-disconnects this connection when leaf dies, regardless of what
    // order Workspace's own bookkeeping gets updated in. The
    // m_leavesById.contains() check is kept as belt-and-suspenders for the
    // narrow window in closeLeaf's Deferred teardown where a leaf is
    // unregistered but not yet actually destroyed.
    connect(dw, &KDDockWidgets::QtWidgets::DockWidget::isCurrentTabChanged,
            leaf, [this, leaf](bool isCurrent) {
        if (isCurrent && m_leavesById.contains(leaf->id()))
            Q_EMIT tabSelectRequested(leaf);
    });

    // Re-emit "user closed this tab" as Workspace::tabCloseRequested(leaf).
    // Hosts (MainWindow) call closeLeaf in response. See A3 note above.
    connect(dw, &KDDockWidgets::QtWidgets::DockWidget::isOpenChanged,
            leaf, [this, leaf](bool isOpen) {
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

WorkspaceLeaf *Workspace::createLeafUnplaced(const QString &leafId)
{
    auto *leaf = new WorkspaceLeaf(m_registry, this);
    if (!leafId.isEmpty())
        leaf->setId(leafId);
    registerLeaf(leaf);
    // No docking, no tab-group assignment — the serializer drives those.
    return leaf;
}

void Workspace::setTabGroupOf(WorkspaceLeaf *leaf, const QString &tabGroupId)
{
    if (!leaf) return;
    m_tabGroupOf.insert(leaf, tabGroupId.isEmpty() ? generateTabGroupId() : tabGroupId);
}

QString Workspace::tabGroupIdOf(WorkspaceLeaf *leaf) const
{
    return m_tabGroupOf.value(leaf);
}

bool Workspace::isTabGroupStacked(const QString &tabGroupId) const
{
    return m_stackedGroups.value(tabGroupId, false);
}

void Workspace::setTabGroupStacked(const QString &tabGroupId, bool stacked)
{
    if (stacked) m_stackedGroups.insert(tabGroupId, true);
    else m_stackedGroups.remove(tabGroupId);
}

void Workspace::detachLeavesOfType(const QString &type)
{
    if (type.isEmpty()) return;
    // Snapshot first — closeLeaf mutates m_leaves while we iterate.
    QVector<WorkspaceLeaf *> toClose;
    for (auto *leaf : m_leaves) {
        const QString leafType = leaf->getViewState()
            .value(QStringLiteral("type")).toString();
        if (leafType == type) toClose.append(leaf);
    }
    for (auto *leaf : toClose) closeLeaf(leaf);
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
    // Capture a still-live sibling in the closing leaf's KDDW tab group so
    // undoCloseLeaf can restore the leaf into its original container rather
    // than the active one. Mirrors Obsidian's "restored to original
    // container + tab group if live" invariant (workspace.md §415).
    for (auto *sib : liveTabSiblings(m_leaves, leaf)) {
        if (sib != leaf) {
            entry.parentId = sib->id();
            break;
        }
    }
    m_undoHistory.prepend(entry);
    if (m_undoHistory.size() > UndoCap)
        m_undoHistory.removeLast();

    if (m_activeLeaf == leaf)
        m_activeLeaf = nullptr;

    Q_EMIT leafClosed(leaf);
    destroyLeaves({leaf}, TeardownMode::Deferred);   // ~WorkspaceLeaf disposes its DockWidget

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

    // Place the new leaf in the original tab group when the captured sibling
    // is still live. Otherwise fall back to the active group (legacy
    // behaviour). Audit: workspace.md §"Top gaps" — undoCloseLeaf does not
    // restore container/parent.
    WorkspaceLeaf *originalSibling = entry.parentId.isEmpty()
        ? nullptr
        : m_leavesById.value(entry.parentId);
    auto *leaf = originalSibling
        ? createLeafInGroupOf(originalSibling)
        : createLeafInActiveGroup();
    if (!leaf)
        return;
    // m_leavesById is keyed on the auto-generated id of the freshly created
    // leaf; rekey to the closed leaf's id before any consumer (including
    // setActiveLeaf below) reads it. Also re-namespace the KDDW dock
    // widget's uniqueName so workspace.json round-trip uses the restored
    // id rather than the throwaway one.
    const QString freshId = leaf->id();
    if (!entry.leafId.isEmpty() && entry.leafId != freshId) {
        m_leavesById.remove(freshId);
        leaf->setId(entry.leafId);
        m_leavesById.insert(entry.leafId, leaf);
        if (auto *qtDw = leaf->dockWidget()) {
            if (auto *core = qtDw->dockWidget())
                core->setUniqueName(uniqueNameFor(m_vaultId, entry.leafId));
        }
    }
    leaf->setPinned(entry.pinned);
    leaf->setGroup(entry.group);
    if (!entry.state.isEmpty())
        leaf->setViewState(entry.state);
    if (!entry.eState.isEmpty())
        leaf->setEphemeralState(entry.eState);
    // Restore back/forward history wholesale. setViewState above doesn't
    // push to the history stack (only navigate() does), so overwriting
    // here is safe.
    leaf->history() = entry.leafHistory;

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

    auto *win = new WorkspaceWindow(this);
    m_windows.append(win);
    if (m_floating) m_floating->addWindow(win);

    // Phase 6.3: re-emit floating-window topology changes as
    // windowFrameChange so plugins can react. Hook destroy on the
    // FloatingWindow KDDW just spawned, then signal the create.
    // The lambda also reaps the WorkspaceWindow shell — without this,
    // X-closing the popout leaves the shell stranded in m_windows /
    // m_floating until workspace teardown. QPointer guards against the
    // case where the shell was already deleted via reparentToMain.
    if (auto *fw = leaf->dockWidget()->dockWidget()->floatingWindow()) {
        connect(fw, &QObject::destroyed, this,
            [this, winPtr = QPointer<WorkspaceWindow>(win)]() {
                if (winPtr) {
                    m_windows.removeOne(winPtr.data());
                    if (m_floating) m_floating->removeWindow(winPtr.data());
                    winPtr->deleteLater();
                }
                Q_EMIT windowFrameChange();
            });
    }

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

WorkspaceLeaf *Workspace::nextLeafInActiveGroup() const
{
    auto group = liveTabSiblings(m_leaves, m_activeLeaf);
    if (group.size() < 2)
        return nullptr;
    int idx = group.indexOf(m_activeLeaf);
    if (idx < 0)
        return nullptr;
    return group[(idx + 1) % group.size()];
}

WorkspaceLeaf *Workspace::previousLeafInActiveGroup() const
{
    auto group = liveTabSiblings(m_leaves, m_activeLeaf);
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
    auto group = liveTabSiblings(m_leaves, leaf);
    return group.indexOf(leaf);
}

int Workspace::leafCountInGroup(WorkspaceLeaf *leaf) const
{
    return liveTabSiblings(m_leaves, leaf).size();
}

void Workspace::closeOtherLeavesInGroupOf(WorkspaceLeaf *leaf)
{
    auto group = liveTabSiblings(m_leaves, leaf);
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
    auto group = liveTabSiblings(m_leaves, leaf);
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
    auto group = liveTabSiblings(m_leaves, sibling ? sibling : m_activeLeaf);
    for (auto *leaf : group) {
        if (!leaf->pinned())
            return leaf;
    }
    return createLeafInGroupOf(sibling);
}

QJsonObject Workspace::serialize() const
{
    // Delegates to WorkspaceSerializer (which walks LayoutSaver JSON for
    // tree topology + per-leaf state from this Workspace via findLeafById).
    // Active-leaf id and lastOpenFiles are appended here since they live
    // on Workspace, not in the layout substrate.
    QJsonObject json = WorkspaceSerializer::toJson(
        m_kddwMain, const_cast<Workspace *>(this));
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

void Workspace::deserialize(const QJsonObject &json)
{
    // Suppress activeLeafChanged emissions while we materialize the layout.
    setLayoutReady(false);

    // A1 fix: route through destroyLeaves so every leaf is unregistered
    // immediately before its dock widget is deleted, instead of the old
    // qDeleteAll(m_leaves) that deleted the whole batch first and cleared
    // the bookkeeping hashes only afterwards.
    destroyLeaves(m_leaves, TeardownMode::Immediate);
    // m_stackedGroups is keyed by tab-group id, not by leaf, so
    // destroyLeaves (which unregisters per-leaf) doesn't touch it.
    m_stackedGroups.clear();
    m_activeLeaf = nullptr;
    m_undoHistory.clear();

    // Parse + materialize the tree (main + floating). All Workspace-side
    // bookkeeping (m_leaves / m_leavesById / m_tabGroupOf / m_stackedGroups)
    // is populated by the serializer's createLeafUnplaced+setTabGroupOf
    // path.
    WorkspaceSerializer::fromJson(json, m_kddwMain, this);

    // Resolve active leaf.
    QString activeId = json[QStringLiteral("active")].toString();
    if (!activeId.isEmpty())
        m_activeLeaf = findLeafById(activeId);
    if (!m_activeLeaf && !m_leaves.isEmpty())
        m_activeLeaf = m_leaves.first();

    // Hydrate lastOpenFiles.
    m_lastOpenFiles.clear();
    for (const auto &v : json[QStringLiteral("lastOpenFiles")].toArray())
        m_lastOpenFiles.append(v.toString());

    // Defer non-active, non-currentTab leaves so they don't materialize
    // their View until the user focuses them. Per-group currentTab is
    // now read from the live KDDW Group rather than synthesized from
    // "first leaf in group", so the deferred set is more accurate than
    // pre-consolidation.
    QHash<QString, WorkspaceLeaf *> currentInGroup;
    auto *coreMain = m_kddwMain ? m_kddwMain->mainWindow() : nullptr;
    if (auto *layout = coreMain ? coreMain->layout() : nullptr) {
        for (auto *grp : layout->groups()) {
            if (grp->dockWidgetCount() == 0) continue;
            int idx = grp->currentTabIndex();
            if (idx < 0 || idx >= grp->dockWidgetCount()) idx = 0;
            const QString uniqueName = grp->dockWidgetAt(idx)->uniqueName();
            const QString stripped = m_vaultId.isEmpty()
                ? uniqueName
                : (uniqueName.startsWith(m_vaultId + QChar(':'))
                    ? uniqueName.mid(m_vaultId.size() + 1)
                    : uniqueName);
            if (auto *leaf = findLeafById(stripped))
                currentInGroup.insert(m_tabGroupOf.value(leaf), leaf);
        }
    }
    for (auto *leaf : m_leaves) {
        if (leaf == m_activeLeaf) continue;
        const QString gid = m_tabGroupOf.value(leaf);
        if (currentInGroup.value(gid) == leaf) continue;

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

    // Ping-through-null forces setActiveLeaf past its identity gate so
    // exactly one activeLeafChanged fires for the post-load state.
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
    // A1's twin: same qDeleteAll-before-clear hazard as deserialize, same
    // fix. A4 fix: also clear m_stackedGroups, which this function used to
    // leak across resets (deserialize already cleared it).
    destroyLeaves(m_leaves, TeardownMode::Immediate);
    m_stackedGroups.clear();
    m_activeLeaf = nullptr;
    m_undoHistory.clear();
    Q_EMIT layoutChanged();
}

} // namespace Corbomite
