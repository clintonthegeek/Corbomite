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

QTEST_MAIN(TestWorkspaceSerializer)
#include "tst_workspace_serializer.moc"
