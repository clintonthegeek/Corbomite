// tests/core/tst_workspace_window.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceWindow.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/ViewRegistry.h"

using namespace Corbomite;

class TestWorkspaceWindow : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void windowIsQtWindow()
    {
        WorkspaceWindow win;
        QVERIFY(win.widget()->windowFlags() & Qt::Window);
    }

    void geometryRoundTrip()
    {
        WorkspaceWindow win;
        win.setWindowGeometry(100, 200, 800, 600);
        win.setMaximized(true);

        QJsonObject json = win.serialize();
        QCOMPARE(json[QStringLiteral("type")].toString(), QStringLiteral("window"));
        QCOMPARE(json[QStringLiteral("x")].toInt(), 100);
        QCOMPARE(json[QStringLiteral("y")].toInt(), 200);
        QCOMPARE(json[QStringLiteral("width")].toInt(), 800);
        QCOMPARE(json[QStringLiteral("height")].toInt(), 600);
        QCOMPARE(json[QStringLiteral("maximize")].toBool(), true);
    }

    void popoutMovesLeafToWindow()
    {
        ViewRegistry registry;
        Workspace ws(&registry);

        auto *leaf = ws.createLeafInActiveGroup();
        QVERIFY(leaf);
        ws.setActiveLeaf(leaf);

        auto *win = ws.popoutLeaf(leaf);
        QVERIFY(win != nullptr);
        QCOMPARE(ws.windows().size(), 1);
    }

    void reparentToMainMovesLeavesBack()
    {
        ViewRegistry registry;
        Workspace ws(&registry);

        auto *leaf = ws.createLeafInActiveGroup();
        QVERIFY(leaf);
        ws.setActiveLeaf(leaf);

        auto *win = ws.popoutLeaf(leaf);
        QVERIFY(win);
        ws.reparentToMain(win);

        QCOMPARE(ws.windows().size(), 0);
        QVERIFY(!ws.allLeaves().isEmpty());
    }
};

QTEST_MAIN(TestWorkspaceWindow)
#include "tst_workspace_window.moc"
