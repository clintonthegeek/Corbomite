// tests/core/tst_workspace_factory.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster Y Phase 7: Obsidian-shape Workspace::getLeaf(mode, dir)
// factory and Workspace::openLinkText(...) dispatcher.

#include <QJsonObject>
#include <QPointer>
#include <QTest>

#include <kddockwidgets/core/DockRegistry.h>
#include <kddockwidgets/qtwidgets/DockWidget.h>
#include <kddockwidgets/qtwidgets/MainWindow.h>

#include "corbomite/core/View.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceLeaf.h"

using namespace Corbomite;

// Stub View that simply round-trips state and ephemeral state. Tests that
// inspect viewState/ephemeralState after `setViewState` need this — the
// `View` base returns empty for both. Registered as "markdown" so the tests
// can call `Workspace::openLinkText` (which hard-codes that view-type).
class StubMarkdownView : public View
{
    Q_OBJECT
public:
    using View::View;
    QString getViewType() const override { return QStringLiteral("markdown"); }
    QString getDisplayText() const override { return QStringLiteral("Stub"); }
    QJsonObject getState() const override { return m_state; }
    void setState(const QJsonObject &s) override { m_state = s; }
    QJsonObject getEphemeralState() const override { return m_eState; }
    void setEphemeralState(const QJsonObject &s) override { m_eState = s; }
private:
    QJsonObject m_state;
    QJsonObject m_eState;
};

namespace {
void registerStubMarkdown(ViewRegistry &registry)
{
    registry.registerView(QStringLiteral("markdown"),
        [](WorkspaceLeaf *leaf) -> View * { return new StubMarkdownView(leaf); });
}
} // namespace

class TestWorkspaceFactory : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void getLeaf_sameMode_returnsActive();
    void getLeaf_sameMode_withNoActive_createsLeaf();
    void getLeaf_tabMode_createsSiblingTab();
    void getLeaf_splitMode_createsSibling();
    void getLeaf_windowMode_createsFloating();

    void leafCountInGroup_reflectsLiveKddwAfterDrag();
    void nextLeafInActiveGroup_followsLiveKddwAfterDrag();

    void openLinkText_simpleLink_setsViewState();
    void openLinkText_withHeading_capturesSubpathInEphemeralState();
    void openLinkText_withEStateOpts_overridesDerivedSubpath();
    void openLinkText_withResolver_resolvesPathBeforeOpen();
};

void TestWorkspaceFactory::getLeaf_sameMode_returnsActive()
{
    ViewRegistry registry;
    Workspace ws(QStringLiteral("test-vault-getleaf-same"), &registry);

    auto *seed = ws.createLeafInActiveGroup();
    QVERIFY(seed);
    ws.setActiveLeaf(seed);

    auto *got = ws.getLeaf(Workspace::LeafMode::Same);
    QCOMPARE(got, seed);
    QCOMPARE(ws.allLeaves().size(), 1);
}

void TestWorkspaceFactory::getLeaf_sameMode_withNoActive_createsLeaf()
{
    ViewRegistry registry;
    Workspace ws(QStringLiteral("test-vault-getleaf-same-empty"), &registry);

    QVERIFY(ws.activeLeaf() == nullptr);
    auto *got = ws.getLeaf(Workspace::LeafMode::Same);
    QVERIFY(got != nullptr);
    QCOMPARE(ws.allLeaves().size(), 1);
}

void TestWorkspaceFactory::getLeaf_tabMode_createsSiblingTab()
{
    ViewRegistry registry;
    Workspace ws(QStringLiteral("test-vault-getleaf-tab"), &registry);

    auto *seed = ws.createLeafInActiveGroup();
    QVERIFY(seed);
    ws.setActiveLeaf(seed);

    auto *got = ws.getLeaf(Workspace::LeafMode::Tab);
    QVERIFY(got != nullptr);
    QVERIFY(got != seed);
    // Tab mode = same group as the active leaf.
    QCOMPARE(ws.leafCountInGroup(seed), 2);
    QCOMPARE(ws.leafCountInGroup(got), 2);
}

void TestWorkspaceFactory::getLeaf_splitMode_createsSibling()
{
    ViewRegistry registry;
    Workspace ws(QStringLiteral("test-vault-getleaf-split"), &registry);

    auto *seed = ws.createLeafInActiveGroup();
    QVERIFY(seed);
    ws.setActiveLeaf(seed);

    auto *got = ws.getLeaf(Workspace::LeafMode::Split,
                            Workspace::LeafDirection::Horizontal);
    QVERIFY(got != nullptr);
    QVERIFY(got != seed);
    // Split mode = new tab group.
    QCOMPARE(ws.leafCountInGroup(seed), 1);
    QCOMPARE(ws.leafCountInGroup(got), 1);
    QCOMPARE(ws.allLeaves().size(), 2);
}

void TestWorkspaceFactory::getLeaf_windowMode_createsFloating()
{
    ViewRegistry registry;
    Workspace ws(QStringLiteral("test-vault-getleaf-window"), &registry);

    // KDDW only spawns FloatingWindows once the host MainWindow is realised;
    // mirror tst_workspace_popout.cpp setup.
    ws.kddwMainWindow()->show();

    const int floatsBefore =
        KDDockWidgets::DockRegistry::self()->floatingWindows().size();

    auto *got = ws.getLeaf(Workspace::LeafMode::Window);
    QVERIFY(got != nullptr);

    const int floatsAfter =
        KDDockWidgets::DockRegistry::self()->floatingWindows().size();
    QCOMPARE(floatsAfter, floatsBefore + 1);
}

void TestWorkspaceFactory::leafCountInGroup_reflectsLiveKddwAfterDrag()
{
    // Regression for P1 #5/#7: tab-group navigation primitives used to
    // read m_tabGroupOf, a cache that lagged user drag-tab-to-other-group.
    // After the rework they consult KDDW (DockRegistry::groups() +
    // Group::dockWidgets()) directly. Simulate a "drag" by reparenting one
    // leaf's KDDW dock widget into another leaf's group via the public
    // KDDW API; the count must reflect the new grouping immediately.
    ViewRegistry registry;
    Workspace ws(QStringLiteral("test-vault-livegroup-count"), &registry);

    auto *a = ws.createLeafInActiveGroup();
    QVERIFY(a);
    auto *b = ws.splitLeaf(a, Qt::Horizontal);
    QVERIFY(b);
    QCOMPARE(ws.leafCountInGroup(a), 1);
    QCOMPARE(ws.leafCountInGroup(b), 1);

    // Move A into B's group via KDDW directly — bypasses Workspace's
    // create-time m_tabGroupOf bookkeeping (this is the same path KDDW
    // takes when the user drags a tab onto another tab group).
    b->dockWidget()->dockWidget()->addDockWidgetAsTab(
        a->dockWidget()->dockWidget());

    QCOMPARE(ws.leafCountInGroup(a), 2);
    QCOMPARE(ws.leafCountInGroup(b), 2);
}

void TestWorkspaceFactory::nextLeafInActiveGroup_followsLiveKddwAfterDrag()
{
    // Same regression as above, but exercises the cycle-through-tabs
    // primitive. Pre-fix, nextLeafInActiveGroup would return nullptr (or
    // the wrong sibling) after a drag because the cached gid still placed
    // the moved leaf in its old (now empty) group.
    ViewRegistry registry;
    Workspace ws(QStringLiteral("test-vault-livegroup-next"), &registry);

    auto *a = ws.createLeafInActiveGroup();
    QVERIFY(a);
    auto *b = ws.splitLeaf(a, Qt::Horizontal);
    QVERIFY(b);

    // Initially A is alone in its group → no sibling to cycle to.
    ws.setActiveLeaf(a);
    QVERIFY(ws.nextLeafInActiveGroup() == nullptr);

    // After "drag" A into B's group, cycling from A must return B.
    b->dockWidget()->dockWidget()->addDockWidgetAsTab(
        a->dockWidget()->dockWidget());
    ws.setActiveLeaf(a);
    QCOMPARE(ws.nextLeafInActiveGroup(), b);
    ws.setActiveLeaf(b);
    QCOMPARE(ws.nextLeafInActiveGroup(), a);
}

void TestWorkspaceFactory::openLinkText_simpleLink_setsViewState()
{
    ViewRegistry registry;
    registerStubMarkdown(registry);
    Workspace ws(QStringLiteral("test-vault-openlink-simple"), &registry);

    QVERIFY(ws.openLinkText(QStringLiteral("MyNote"), QString{},
                             Workspace::LeafMode::Tab));

    auto *leaf = ws.activeLeaf();
    QVERIFY(leaf);
    const auto vs = leaf->getViewState();
    QCOMPARE(vs.value(QStringLiteral("type")).toString(),
              QStringLiteral("markdown"));
    QCOMPARE(vs.value(QStringLiteral("state")).toObject()
                 .value(QStringLiteral("file")).toString(),
              QStringLiteral("MyNote"));
    QVERIFY(leaf->getEphemeralState().isEmpty());
}

void TestWorkspaceFactory::openLinkText_withHeading_capturesSubpathInEphemeralState()
{
    ViewRegistry registry;
    registerStubMarkdown(registry);
    Workspace ws(QStringLiteral("test-vault-openlink-heading"), &registry);

    QVERIFY(ws.openLinkText(QStringLiteral("Note#Section"), QString{},
                             Workspace::LeafMode::Tab));

    auto *leaf = ws.activeLeaf();
    QVERIFY(leaf);
    const auto vs = leaf->getViewState();
    QCOMPARE(vs.value(QStringLiteral("state")).toObject()
                 .value(QStringLiteral("file")).toString(),
              QStringLiteral("Note"));
    const auto eState = leaf->getEphemeralState();
    QCOMPARE(eState.value(QStringLiteral("subpath")).toString(),
              QStringLiteral("#Section"));
}

void TestWorkspaceFactory::openLinkText_withEStateOpts_overridesDerivedSubpath()
{
    ViewRegistry registry;
    registerStubMarkdown(registry);
    Workspace ws(QStringLiteral("test-vault-openlink-estate"), &registry);

    QJsonObject opts;
    QJsonObject eState;
    eState[QStringLiteral("scroll")] = 42;
    opts[QStringLiteral("eState")] = eState;

    QVERIFY(ws.openLinkText(QStringLiteral("Note#Heading"), QString{},
                             Workspace::LeafMode::Tab, opts));

    auto *leaf = ws.activeLeaf();
    QVERIFY(leaf);
    const auto leafEState = leaf->getEphemeralState();
    // opts.eState wins over the heading-derived subpath.
    QVERIFY(!leafEState.contains(QStringLiteral("subpath")));
    QCOMPARE(leafEState.value(QStringLiteral("scroll")).toInt(), 42);
}

void TestWorkspaceFactory::openLinkText_withResolver_resolvesPathBeforeOpen()
{
    ViewRegistry registry;
    registerStubMarkdown(registry);
    Workspace ws(QStringLiteral("test-vault-openlink-resolver"), &registry);

    ws.setLinkResolver([](const QString &path, const QString &source) {
        Q_UNUSED(source);
        return path == QStringLiteral("Note")
            ? QStringLiteral("folder/Note.md")
            : path;
    });

    QVERIFY(ws.openLinkText(QStringLiteral("Note"), QString{},
                             Workspace::LeafMode::Tab));

    auto *leaf = ws.activeLeaf();
    QVERIFY(leaf);
    QCOMPARE(leaf->getViewState().value(QStringLiteral("state")).toObject()
                  .value(QStringLiteral("file")).toString(),
              QStringLiteral("folder/Note.md"));
}

QTEST_MAIN(TestWorkspaceFactory)
#include "tst_workspace_factory.moc"
