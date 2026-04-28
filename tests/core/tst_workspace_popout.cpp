// tests/core/tst_workspace_popout.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster Y Phase 5: popout windows atop KDDW FloatingWindow.
// Phase 5.1 lights only popoutLeaf_createsFloatingWindow; the remaining
// slots are populated in 5.3 (close-window propagation) and 5.4
// (geometry + maximize round-trip).

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QSignalSpy>
#include <QTest>

#include <kddockwidgets/core/DockRegistry.h>
#include <kddockwidgets/core/FloatingWindow.h>
#include <kddockwidgets/qtwidgets/DockWidget.h>
#include <kddockwidgets/qtwidgets/MainWindow.h>

#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceFloating.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/WorkspaceWindow.h"
#include "WorkspaceSerializer.h"

using namespace Corbomite;

class TestWorkspacePopout : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void popoutLeaf_createsFloatingWindow();
    void closeFloatingWindow_closesChildrenLeaves();
    void closeFloatingWindow_reapsWorkspaceWindowShell();
    void restoreFloatingWindow_preservesGeometry();
    void restoreFloatingWindow_preservesMaximize();
};

void TestWorkspacePopout::popoutLeaf_createsFloatingWindow()
{
    ViewRegistry registry;
    Workspace ws(QStringLiteral("test-vault-popout-create"), &registry);

    // KDDW only spawns FloatingWindows once the host MainWindow is realised;
    // without show() setFloating(true) silently no-ops. See the comment on
    // materializeFloatingWindow in WorkspaceSerializer.cpp.
    ws.kddwMainWindow()->show();

    auto *leaf = ws.createLeafInActiveGroup();
    QVERIFY(leaf);
    QVERIFY(leaf->dockWidget());

    const int floatsBefore =
        KDDockWidgets::DockRegistry::self()->floatingWindows().size();

    auto *win = ws.popoutLeaf(leaf);
    QVERIFY(win != nullptr);

    const int floatsAfter =
        KDDockWidgets::DockRegistry::self()->floatingWindows().size();
    QCOMPARE(floatsAfter, floatsBefore + 1);
    QVERIFY(leaf->dockWidget()->isFloating());
}

void TestWorkspacePopout::closeFloatingWindow_closesChildrenLeaves()
{
    ViewRegistry registry;
    Workspace ws(QStringLiteral("test-vault-popout-close"), &registry);
    ws.kddwMainWindow()->show();

    // Wire the host-side handshake: in production MainWindow connects
    // Workspace::tabCloseRequested -> Workspace::closeLeaf so the close-flow
    // can hook save-prompts on dirty files. The test simulates that here.
    QObject::connect(&ws, &Workspace::tabCloseRequested, &ws,
                     [&ws](WorkspaceLeaf *l) { ws.closeLeaf(l); });

    auto *leaf = ws.createLeafInActiveGroup();
    QVERIFY(leaf);
    ws.popoutLeaf(leaf);
    QVERIFY(leaf->dockWidget()->isFloating());

    auto floats = KDDockWidgets::DockRegistry::self()->floatingWindows();
    QVERIFY(!floats.isEmpty());

    QPointer<WorkspaceLeaf> leafPtr(leaf);
    QSignalSpy leafClosedSpy(&ws, &Workspace::leafClosed);
    floats.first()->view()->close();

    // closeLeaf emits leafClosed and then schedules deleteLater on the leaf;
    // QTRY_COMPARE runs the event loop so the delete actually happens. The
    // QPointer-null check verifies the leaf was reaped, not just signalled.
    QTRY_COMPARE(leafClosedSpy.count(), 1);
    QTRY_VERIFY(leafPtr.isNull());
    QVERIFY(ws.allLeaves().isEmpty());
}

void TestWorkspacePopout::closeFloatingWindow_reapsWorkspaceWindowShell()
{
    // Regression for P1 #6: X-closing a popout used to leave the
    // WorkspaceWindow shell stranded in m_windows / m_floating until
    // workspace teardown. The fw->destroyed handler should now clean
    // both lists and delete the shell.
    ViewRegistry registry;
    Workspace ws(QStringLiteral("test-vault-popout-reap"), &registry);
    ws.kddwMainWindow()->show();

    QObject::connect(&ws, &Workspace::tabCloseRequested, &ws,
                     [&ws](WorkspaceLeaf *l) { ws.closeLeaf(l); });

    auto *leaf = ws.createLeafInActiveGroup();
    QVERIFY(leaf);
    auto *win = ws.popoutLeaf(leaf);
    QVERIFY(win != nullptr);
    QPointer<WorkspaceWindow> winPtr(win);

    QCOMPARE(ws.windows().size(), 1);
    QVERIFY(ws.floatingSplit() != nullptr);
    QCOMPARE(ws.floatingSplit()->windows().size(), 1);

    auto floats = KDDockWidgets::DockRegistry::self()->floatingWindows();
    QVERIFY(!floats.isEmpty());
    floats.first()->view()->close();

    QTRY_VERIFY(winPtr.isNull());
    QCOMPARE(ws.windows().size(), 0);
    QCOMPARE(ws.floatingSplit()->windows().size(), 0);
}

void TestWorkspacePopout::restoreFloatingWindow_preservesGeometry()
{
    KDDockWidgets::DockRegistry::self()->clear();

    // Serialize side: spin a Workspace, popout a leaf, set the floating
    // geometry to a known rect, then capture the JSON via WorkspaceSerializer.
    QJsonObject json;
    {
        ViewRegistry registry;
        Workspace ws(QStringLiteral("test-vault-popout-geom-a"), &registry);
        ws.kddwMainWindow()->show();

        auto *leaf = ws.createLeafInActiveGroup();
        QVERIFY(leaf);
        ws.popoutLeaf(leaf);
        leaf->dockWidget()->dockWidget()->setFloatingGeometry(
            QRect(123, 456, 789, 234));

        json = WorkspaceSerializer::toJson(ws.kddwMainWindow(), &ws);
    }
    KDDockWidgets::DockRegistry::self()->clear();

    // Inspect the captured JSON before round-tripping.
    auto floatingObj = json.value(QStringLiteral("floating")).toObject();
    QVERIFY(!floatingObj.isEmpty());
    auto floatingChildren = floatingObj.value(QStringLiteral("children")).toArray();
    QCOMPARE(floatingChildren.size(), 1);
    auto windowObj = floatingChildren.first().toObject();
    QCOMPARE(windowObj.value(QStringLiteral("x")).toInt(), 123);
    QCOMPARE(windowObj.value(QStringLiteral("y")).toInt(), 456);
    QCOMPARE(windowObj.value(QStringLiteral("width")).toInt(), 789);
    QCOMPARE(windowObj.value(QStringLiteral("height")).toInt(), 234);

    // Deserialize side: a fresh KDDW MainWindow + WorkspaceSerializer::fromJson
    // should resurrect the floating window with the same geometry.
    auto *fresh = new KDDockWidgets::QtWidgets::MainWindow(
        QStringLiteral("test-vault-popout-geom-b"),
        KDDockWidgets::MainWindowOption_None);
    fresh->show();

    WorkspaceSerializer::fromJson(json, fresh, /*workspace=*/nullptr);

    auto fws = KDDockWidgets::DockRegistry::self()->floatingWindows();
    QCOMPARE(fws.size(), 1);
    const auto rect = fws.first()->geometry();
    QCOMPARE(rect.width(), 789);
    QCOMPARE(rect.height(), 234);
    // x/y may be tweaked by the WM "ensure-on-screen" pass; assert nominal
    // round-trip when the value survived.
    QCOMPARE(rect.x(), 123);
    QCOMPARE(rect.y(), 456);

    delete fresh;
}

void TestWorkspacePopout::restoreFloatingWindow_preservesMaximize()
{
    KDDockWidgets::DockRegistry::self()->clear();

    // Build a JSON payload directly that asks for a maximized floating
    // window. (Driving real maximize through the offscreen platform is
    // unreliable; the contract being tested is that the materializer
    // honours the maximize flag, not that QWindow round-trips it.)
    const QJsonObject json = QJsonDocument::fromJson(R"({
        "main": {
            "id": "main", "type": "split", "direction": "vertical",
            "children": [
                { "id": "g1", "type": "tabs", "children": [
                    { "id": "anchor", "type": "leaf",
                      "state": {"type": "empty", "state": {}} } ] }
            ]
        },
        "floating": {
            "id": "f", "type": "floating", "children": [
                { "id": "win", "type": "window", "direction": "vertical",
                  "x": 50, "y": 60, "width": 800, "height": 600,
                  "maximize": true,
                  "children": [
                      { "id": "ftabs", "type": "tabs", "children": [
                          { "id": "fleaf", "type": "leaf",
                            "state": {"type": "empty", "state": {}} } ] }
                  ] }
            ]
        }
    })").object();

    auto *fresh = new KDDockWidgets::QtWidgets::MainWindow(
        QStringLiteral("test-vault-popout-max"),
        KDDockWidgets::MainWindowOption_None);
    fresh->show();

    WorkspaceSerializer::fromJson(json, fresh, /*workspace=*/nullptr);

    auto fws = KDDockWidgets::DockRegistry::self()->floatingWindows();
    QCOMPARE(fws.size(), 1);
    QVERIFY(fws.first()->view());
    QTRY_VERIFY(fws.first()->view()->isMaximized());

    delete fresh;
}

QTEST_MAIN(TestWorkspacePopout)
#include "tst_workspace_popout.moc"
