// SPDX-FileCopyrightText: 2026 Clinton Molyneux <clinton@concernednetizen.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// Cluster Y Phase 3: WorkspaceSerializer round-trip tests against synthetic
// KDDockWidgets trees driven by Obsidian-shape workspace.json fixtures.
// Phase 3 builds the serializer in isolation; Phase 4 wires it into the real
// Workspace + WorkspaceLeaf tree.

#include <QtTest>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <kddockwidgets/KDDockWidgets.h>
#include <kddockwidgets/core/DockRegistry.h>
#include <kddockwidgets/qtwidgets/DockWidget.h>
#include <kddockwidgets/qtwidgets/MainWindow.h>

// Included via private path — serializer is not in the public API
#include "WorkspaceSerializer.h"

class TestWorkspaceSerializer : public QObject
{
    Q_OBJECT

private:
    QJsonObject readFixture(const QString &name);

private slots:
    void initTestCase();
    void cleanup();

    void fixture01_singleLeaf_fromJson_createsOneDockWidget();
    void fixture01_singleLeaf_roundTrip_isShapeEquivalent();
    void fixture02_horizontalSplit_twoDockWidgetsSideBySide();
    void fixture03_nestedSplits_threeDockWidgetsInCorrectGroups();
    void fixture04_stackedTabs_preservesStackedFlag();
    void fixture05_floatingWindow_createsFloatingWindow();
    void fixture06_pinnedWithGroup_preservesBoth();
    void fixture07_emptyJson_producesDefaultTree();
    void fixture08_unknownKeys_preservedVerbatim();
    void malformedJson_fallsBackToDefaultTree();
    void fixture09_orphanLeaf_reHomedToRoot();
};

void TestWorkspaceSerializer::initTestCase()
{
    KDDockWidgets::initFrontend(KDDockWidgets::FrontendType::QtWidgets);
}

void TestWorkspaceSerializer::cleanup()
{
    // Each test creates a MainWindow with its own affinity; clear the global
    // DockRegistry between tests so dock widget unique-name assertions stay
    // isolated.
    KDDockWidgets::DockRegistry::self()->clear();
}

QJsonObject TestWorkspaceSerializer::readFixture(const QString &name)
{
    QFile f(QStringLiteral(CORBOMITE_TEST_FIXTURE_DIR "/workspace-obsidian/") + name);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open fixture" << name << ":" << f.errorString();
        return {};
    }
    return QJsonDocument::fromJson(f.readAll()).object();
}

void TestWorkspaceSerializer::fixture01_singleLeaf_fromJson_createsOneDockWidget()
{
    auto json = readFixture(QStringLiteral("01-single-leaf.json"));
    QVERIFY(!json.isEmpty());
    auto mainWindow = std::make_unique<KDDockWidgets::QtWidgets::MainWindow>(
        QStringLiteral("test-f01"), KDDockWidgets::MainWindowOption_None);

    Corbomite::WorkspaceSerializer::fromJson(json, mainWindow.get(), /*workspace*/ nullptr);

    auto *registry = KDDockWidgets::DockRegistry::self();
    QCOMPARE(registry->dockwidgets().size(), 1);
    QVERIFY(registry->dockByName(QStringLiteral("cccccccccccccccc")) != nullptr);
}

void TestWorkspaceSerializer::fixture01_singleLeaf_roundTrip_isShapeEquivalent()
{
    // Shape-only comparison; cached-field round-trip covered later
    // after Workspace* integration lands (Task 3.7+).
    auto jsonIn = readFixture(QStringLiteral("01-single-leaf.json"));
    auto mainWindow = std::make_unique<KDDockWidgets::QtWidgets::MainWindow>(
        QStringLiteral("test-f01-rt"), KDDockWidgets::MainWindowOption_None);

    Corbomite::WorkspaceSerializer::fromJson(jsonIn, mainWindow.get(), nullptr);
    QJsonObject jsonOut = Corbomite::WorkspaceSerializer::toJson(mainWindow.get(), nullptr);

    auto mainIn = jsonIn.value(QStringLiteral("main")).toObject();
    auto mainOut = jsonOut.value(QStringLiteral("main")).toObject();
    QCOMPARE(mainOut.value(QStringLiteral("type")).toString(),
             mainIn.value(QStringLiteral("type")).toString());
    QCOMPARE(mainOut.value(QStringLiteral("children")).toArray().size(),
             mainIn.value(QStringLiteral("children")).toArray().size());
}

void TestWorkspaceSerializer::fixture02_horizontalSplit_twoDockWidgetsSideBySide()
{
    auto json = readFixture(QStringLiteral("02-two-leaf-split-horizontal.json"));
    QVERIFY(!json.isEmpty());
    auto mainWindow = std::make_unique<KDDockWidgets::QtWidgets::MainWindow>(
        QStringLiteral("test-f02"), KDDockWidgets::MainWindowOption_None);

    Corbomite::WorkspaceSerializer::fromJson(json, mainWindow.get(), nullptr);

    auto *registry = KDDockWidgets::DockRegistry::self();
    QCOMPARE(registry->dockwidgets().size(), 2);

    auto *dw1 = registry->dockByName(QStringLiteral("cccccccccccccccc"));
    auto *dw2 = registry->dockByName(QStringLiteral("eeeeeeeeeeeeeeee"));
    QVERIFY(dw1);
    QVERIFY(dw2);

    // Side-by-side splits create two separate KDDW Groups (one per leaf).
    // Tabbed leaves would share a Group and report isTabbed() == true.
    QCOMPARE(dw1->isTabbed(), false);
    QCOMPARE(dw2->isTabbed(), false);
    QCOMPARE(registry->groups().size(), 2);
}

void TestWorkspaceSerializer::fixture03_nestedSplits_threeDockWidgetsInCorrectGroups()
{
    auto json = readFixture(QStringLiteral("03-nested-splits.json"));
    QVERIFY(!json.isEmpty());
    auto mainWindow = std::make_unique<KDDockWidgets::QtWidgets::MainWindow>(
        QStringLiteral("test-f03"), KDDockWidgets::MainWindowOption_None);

    Corbomite::WorkspaceSerializer::fromJson(json, mainWindow.get(), nullptr);

    auto *registry = KDDockWidgets::DockRegistry::self();
    QCOMPARE(registry->dockwidgets().size(), 3);
    QVERIFY(registry->dockByName(QStringLiteral("leaf01aaaaaaaaaa")));
    QVERIFY(registry->dockByName(QStringLiteral("leaf02aaaaaaaaaa")));
    QVERIFY(registry->dockByName(QStringLiteral("leaf03aaaaaaaaaa")));

    // Three side-by-side leaves => three KDDW Groups (none tabbed together).
    QCOMPARE(registry->groups().size(), 3);
}

void TestWorkspaceSerializer::fixture04_stackedTabs_preservesStackedFlag()
{
    auto jsonIn = readFixture(QStringLiteral("04-stacked-tabs.json"));
    QVERIFY(!jsonIn.isEmpty());
    auto mainWindow = std::make_unique<KDDockWidgets::QtWidgets::MainWindow>(
        QStringLiteral("test-f04"), KDDockWidgets::MainWindowOption_None);

    Corbomite::WorkspaceSerializer::fromJson(jsonIn, mainWindow.get(), nullptr);

    auto *registry = KDDockWidgets::DockRegistry::self();
    QCOMPARE(registry->dockwidgets().size(), 3);
    // All three leaves tab into a single Group.
    QCOMPARE(registry->groups().size(), 1);

    auto jsonOut = Corbomite::WorkspaceSerializer::toJson(mainWindow.get(), nullptr);
    auto tabsOut = jsonOut.value(QStringLiteral("main"))
                       .toObject()
                       .value(QStringLiteral("children"))
                       .toArray()
                       .first()
                       .toObject();
    QCOMPARE(tabsOut.value(QStringLiteral("type")).toString(), QStringLiteral("tabs"));
    QCOMPARE(tabsOut.value(QStringLiteral("stacked")).toBool(), true);
}

void TestWorkspaceSerializer::fixture05_floatingWindow_createsFloatingWindow()
{
    auto jsonIn = readFixture(QStringLiteral("05-floating-window.json"));
    QVERIFY(!jsonIn.isEmpty());
    auto mainWindow = std::make_unique<KDDockWidgets::QtWidgets::MainWindow>(
        QStringLiteral("test-f05"), KDDockWidgets::MainWindowOption_None);
    mainWindow->show();

    Corbomite::WorkspaceSerializer::fromJson(jsonIn, mainWindow.get(), nullptr);

    auto *registry = KDDockWidgets::DockRegistry::self();
    QCOMPARE(registry->dockwidgets().size(), 2);
    auto floats = registry->floatingWindows();
    QCOMPARE(floats.size(), 1);

    // Round-trip: the output JSON should carry a "floating" object whose
    // children array has one entry (the single floating window).
    auto jsonOut = Corbomite::WorkspaceSerializer::toJson(mainWindow.get(), nullptr);
    auto floatingOut = jsonOut.value(QStringLiteral("floating")).toObject();
    QVERIFY(!floatingOut.isEmpty());
    QCOMPARE(floatingOut.value(QStringLiteral("children")).toArray().size(), 1);
}

void TestWorkspaceSerializer::fixture06_pinnedWithGroup_preservesBoth()
{
    auto jsonIn = readFixture(QStringLiteral("06-pinned-with-group.json"));
    QVERIFY(!jsonIn.isEmpty());
    auto mainWindow = std::make_unique<KDDockWidgets::QtWidgets::MainWindow>(
        QStringLiteral("test-f06"), KDDockWidgets::MainWindowOption_None);

    Corbomite::WorkspaceSerializer::fromJson(jsonIn, mainWindow.get(), nullptr);
    auto jsonOut = Corbomite::WorkspaceSerializer::toJson(mainWindow.get(), nullptr);

    auto leaf = jsonOut.value(QStringLiteral("main"))
                    .toObject()
                    .value(QStringLiteral("children"))
                    .toArray()
                    .first()
                    .toObject()
                    .value(QStringLiteral("children"))
                    .toArray()
                    .first()
                    .toObject();
    QCOMPARE(leaf.value(QStringLiteral("pinned")).toBool(), true);
    QCOMPARE(leaf.value(QStringLiteral("group")).toString(),
             QStringLiteral("pinned-group-id"));
}

void TestWorkspaceSerializer::fixture07_emptyJson_producesDefaultTree()
{
    auto json = readFixture(QStringLiteral("07-empty.json"));
    auto mainWindow = std::make_unique<KDDockWidgets::QtWidgets::MainWindow>(
        QStringLiteral("test-f07"), KDDockWidgets::MainWindowOption_None);

    Corbomite::WorkspaceSerializer::fromJson(json, mainWindow.get(), nullptr);

    auto *registry = KDDockWidgets::DockRegistry::self();
    QCOMPARE(registry->dockwidgets().size(), 1);
}

void TestWorkspaceSerializer::fixture08_unknownKeys_preservedVerbatim()
{
    auto jsonIn = readFixture(QStringLiteral("08-unknown-keys.json"));
    QVERIFY(!jsonIn.isEmpty());
    auto mainWindow = std::make_unique<KDDockWidgets::QtWidgets::MainWindow>(
        QStringLiteral("test-f08"), KDDockWidgets::MainWindowOption_None);

    Corbomite::WorkspaceSerializer::fromJson(jsonIn, mainWindow.get(), nullptr);
    auto jsonOut = Corbomite::WorkspaceSerializer::toJson(mainWindow.get(), nullptr);

    auto leaf = jsonOut.value(QStringLiteral("main"))
                    .toObject()
                    .value(QStringLiteral("children"))
                    .toArray()
                    .first()
                    .toObject()
                    .value(QStringLiteral("children"))
                    .toArray()
                    .first()
                    .toObject();
    auto obsidianInternal = leaf.value(QStringLiteral("obsidianInternal")).toObject();
    QCOMPARE(obsidianInternal.value(QStringLiteral("someField")).toInt(), 42);
}

void TestWorkspaceSerializer::malformedJson_fallsBackToDefaultTree()
{
    // 'main' present but the wrong type (an integer rather than an object).
    // Should not crash; serializer falls back to the default empty tree.
    QJsonObject broken;
    broken[QStringLiteral("main")] = 42;
    broken[QStringLiteral("garbage")] = true;

    auto mainWindow = std::make_unique<KDDockWidgets::QtWidgets::MainWindow>(
        QStringLiteral("test-broken"), KDDockWidgets::MainWindowOption_None);

    Corbomite::WorkspaceSerializer::fromJson(broken, mainWindow.get(), nullptr);

    auto *registry = KDDockWidgets::DockRegistry::self();
    QCOMPARE(registry->dockwidgets().size(), 1);
}

void TestWorkspaceSerializer::fixture09_orphanLeaf_reHomedToRoot()
{
    auto json = readFixture(QStringLiteral("09-orphan-leaf.json"));
    QVERIFY(!json.isEmpty());
    auto mainWindow = std::make_unique<KDDockWidgets::QtWidgets::MainWindow>(
        QStringLiteral("test-f09"), KDDockWidgets::MainWindowOption_None);

    // First sibling is an empty tabs node; the orphan leaf in the second
    // sibling should be re-homed to the MainWindow root rather than crash.
    Corbomite::WorkspaceSerializer::fromJson(json, mainWindow.get(), nullptr);

    auto *registry = KDDockWidgets::DockRegistry::self();
    QCOMPARE(registry->dockwidgets().size(), 1);
    QVERIFY(registry->dockByName(QStringLiteral("orphan1aaaaaaaaa")) != nullptr);
}

QTEST_MAIN(TestWorkspaceSerializer)
#include "tst_workspace_serializer.moc"
