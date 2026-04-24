// SPDX-FileCopyrightText: 2026 Clinton Molyneux <clinton@concernednetizen.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// Cluster Y Phase 1 risk reducer: verify that KDDockWidgets::QtWidgets::MainWindow
// can be embedded as the central widget of a KXmlGuiWindow, and that
// KActionCollection on the outer window still fires.  Both are required for
// Corbomite's existing menu/toolbar plumbing to survive the Cluster Y substrate
// swap.

#include <QtTest>
#include <QAction>
#include <KActionCollection>
#include <KXmlGuiWindow>
#include <kddockwidgets/KDDockWidgets.h>
#include <kddockwidgets/qtwidgets/MainWindow.h>
#include <kddockwidgets/qtwidgets/DockWidget.h>

class TestWorkspaceEmbedKXmlGui : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void embedsKddwMainWindowInKXmlGuiWindow();
    void actionsOnOuterWindowStillReachable();
};

void TestWorkspaceEmbedKXmlGui::initTestCase()
{
    // KDDW 2.x requires explicit frontend initialization before any KDDW
    // objects are constructed.  Must be called once per process.
    KDDockWidgets::initFrontend(KDDockWidgets::FrontendType::QtWidgets);
}

void TestWorkspaceEmbedKXmlGui::embedsKddwMainWindowInKXmlGuiWindow()
{
    auto outer = std::make_unique<KXmlGuiWindow>();
    // KDDW 2.4.0: KDDockWidgets::QtWidgets::MainWindow extends QMainWindow.
    // It can be parented to the KXmlGuiWindow and set as its central widget.
    auto *inner = new KDDockWidgets::QtWidgets::MainWindow(
        QStringLiteral("test-main"),
        KDDockWidgets::MainWindowOption_None,
        outer.get());
    outer->setCentralWidget(inner);
    outer->resize(800, 600);
    outer->show();
    // KXmlGuiWindow::show() does not recursively show embedded QMainWindow
    // children in offscreen mode; call inner->show() explicitly to propagate.
    inner->show();
    QVERIFY(QTest::qWaitForWindowExposed(outer.get()));
    QCOMPARE(outer->centralWidget(), inner);
    QVERIFY(inner->isVisible());

    // DockWidget: KDDW 2.4.0 QtWidgets API uses setWidget(QWidget*) — not setGuestView.
    auto *dw = new KDDockWidgets::QtWidgets::DockWidget(QStringLiteral("test-dock"));
    auto *guest = new QWidget;
    dw->setWidget(guest);
    inner->addDockWidget(dw, KDDockWidgets::Location_OnLeft);
    QVERIFY(dw->isVisible());
}

void TestWorkspaceEmbedKXmlGui::actionsOnOuterWindowStillReachable()
{
    auto outer = std::make_unique<KXmlGuiWindow>();
    auto *inner = new KDDockWidgets::QtWidgets::MainWindow(
        QStringLiteral("test-main-2"),
        KDDockWidgets::MainWindowOption_None,
        outer.get());
    outer->setCentralWidget(inner);

    auto *act = new QAction(QStringLiteral("TestAction"), outer.get());
    outer->actionCollection()->addAction(QStringLiteral("test_action"), act);

    QSignalSpy spy(act, &QAction::triggered);
    act->trigger();
    QCOMPARE(spy.count(), 1);
}

QTEST_MAIN(TestWorkspaceEmbedKXmlGui)
#include "tst_workspace_embed_kxmlgui.moc"
