// SPDX-FileCopyrightText: 2026 Clinton Molyneux <clinton@concernednetizen.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// Cluster Y Phase 3: WorkspaceSerializer round-trip tests against synthetic
// KDDockWidgets trees driven by Obsidian-shape workspace.json fixtures.
// Phase 3 builds the serializer in isolation; Phase 4 wires it into the real
// Workspace + WorkspaceLeaf tree.

#include <QtTest>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <kddockwidgets/KDDockWidgets.h>
#include <kddockwidgets/core/DockRegistry.h>
#include <kddockwidgets/qtwidgets/DockWidget.h>
#include <kddockwidgets/qtwidgets/MainWindow.h>
#include <kddockwidgets/qtcommon/View.h>

// Included via private path — serializer is not in the public API
#include "WorkspaceSerializer.h"

#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceLeaf.h"

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
    void fixture11_perGroupCurrentTab_roundtrips();
    void fixture13_popoutNestedSplit_roundtrips();
    void fixture12_nestedWithState_workspaceRoundTrip();
    void fixture08b_unknownKeys_workspaceRoundTrip();
    void fixture04b_stacked_workspaceRoundTrip();
    void fixture14_deferSet_perGroupCurrentTabRespected();
    void fixture16_nestedSplitFirstChild_matchesObsidianGeometry();
    void containerIds_stableAcrossSerializations();
    void windowNode_directionAlwaysVertical();
    void rootSplit_directionForcedVerticalOnLoad();
    void windowNode_maximizeWrittenUnconditionally();
    void dimension_survivesRoundTrip();
    void dimension_clampIsInclusiveAndPoisoning();
    void jsonText_rootKeyOrderMatchesObsidian();
    void jsonText_nodeKeyOrderMatchesObsidian();
    void jsonText_reparsesToEquivalentDocument();
};

namespace {

QJsonObject leafNode(const QString &id)
{
    QJsonObject state;
    state[QStringLiteral("type")] = QStringLiteral("empty");
    state[QStringLiteral("state")] = QJsonObject();
    QJsonObject o;
    o[QStringLiteral("id")] = id;
    o[QStringLiteral("type")] = QStringLiteral("leaf");
    o[QStringLiteral("state")] = state;
    return o;
}

QJsonObject tabsNode(const QString &id, const QString &leafId,
                     const QJsonValue &dimension = QJsonValue::Undefined)
{
    QJsonObject o;
    o[QStringLiteral("id")] = id;
    o[QStringLiteral("type")] = QStringLiteral("tabs");
    if (!dimension.isUndefined())
        o[QStringLiteral("dimension")] = dimension;
    o[QStringLiteral("children")] = QJsonArray{leafNode(leafId)};
    return o;
}

QJsonObject layoutWith(const QJsonArray &children)
{
    QJsonObject o;
    o[QStringLiteral("main")] = [&] {
        QJsonObject root;
        root[QStringLiteral("id")] = QStringLiteral("rootid000000000");
        root[QStringLiteral("type")] = QStringLiteral("split");
        root[QStringLiteral("direction")] = QStringLiteral("vertical");
        root[QStringLiteral("children")] = children;
        return root;
    }();
    return o;
}

// Locate the serialized container node whose first leaf carries `leafId`.
QJsonObject nodeForLeaf(const QJsonObject &main, const QString &leafId)
{
    for (const auto &v : main.value(QStringLiteral("children")).toArray()) {
        const auto node = v.toObject();
        if (node.value(QStringLiteral("type")).toString() == QLatin1String("tabs")) {
            const auto kids = node.value(QStringLiteral("children")).toArray();
            if (!kids.isEmpty()
                && kids.first().toObject().value(QStringLiteral("id")).toString() == leafId)
                return node;
        } else if (node.value(QStringLiteral("type")).toString() == QLatin1String("split")) {
            if (const auto found = nodeForLeaf(node, leafId); !found.isEmpty())
                return found;
        }
    }
    return {};
}

// Keys emitted at exactly `indent` columns of `text`. The emitter puts one
// member per line at 2 spaces per nesting level, so a deeper line is
// filtered out by requiring the first character at `indent` to be the
// opening quote.
QStringList keysAtIndent(const QByteArray &text, int indent)
{
    QStringList keys;
    const QString prefix(indent, QLatin1Char(' '));
    const auto lines = QString::fromUtf8(text).split(QLatin1Char('\n'));
    for (const auto &line : lines) {
        if (line.size() <= indent || !line.startsWith(prefix)
            || line.at(indent) != QLatin1Char('"'))
            continue;
        const qsizetype close = line.indexOf(QLatin1Char('"'), indent + 1);
        if (close > 0)
            keys.append(line.mid(indent + 1, close - indent - 1));
    }
    return keys;
}

} // namespace

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
    mainWindow->show();

    Corbomite::WorkspaceSerializer::fromJson(json, mainWindow.get(), nullptr);

    auto *registry = KDDockWidgets::DockRegistry::self();
    QCOMPARE(registry->dockwidgets().size(), 3);
    QVERIFY(registry->dockByName(QStringLiteral("leaf01aaaaaaaaaa")));
    QVERIFY(registry->dockByName(QStringLiteral("leaf02aaaaaaaaaa")));
    QVERIFY(registry->dockByName(QStringLiteral("leaf03aaaaaaaaaa")));
    QCOMPARE(registry->groups().size(), 3);

    // Round-trip shape: outer split has two children — one tabs (with leaf01)
    // and one inner split (with two tabs holding leaf02 and leaf03).
    auto out = Corbomite::WorkspaceSerializer::toJson(mainWindow.get(), nullptr);
    auto outerChildren = out.value(QStringLiteral("main")).toObject()
                            .value(QStringLiteral("children")).toArray();
    QCOMPARE(outerChildren.size(), 2);

    bool sawOuterTabs = false;
    bool sawInnerSplit = false;
    for (const auto &v : outerChildren) {
        auto obj = v.toObject();
        auto type = obj.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("tabs"))
            sawOuterTabs = true;
        if (type == QStringLiteral("split")) {
            sawInnerSplit = true;
            auto innerChildren = obj.value(QStringLiteral("children")).toArray();
            QCOMPARE(innerChildren.size(), 2);
            for (const auto &vv : innerChildren) {
                QCOMPARE(vv.toObject().value(QStringLiteral("type")).toString(),
                         QStringLiteral("tabs"));
            }
        }
    }
    QVERIFY(sawOuterTabs);
    QVERIFY(sawInnerSplit);
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

void TestWorkspaceSerializer::fixture11_perGroupCurrentTab_roundtrips()
{
    auto jsonIn = readFixture(QStringLiteral("11-per-group-currenttab.json"));
    QVERIFY(!jsonIn.isEmpty());
    auto mainWindow = std::make_unique<KDDockWidgets::QtWidgets::MainWindow>(
        QStringLiteral("test-f11"), KDDockWidgets::MainWindowOption_None);
    mainWindow->show();

    Corbomite::WorkspaceSerializer::fromJson(jsonIn, mainWindow.get(), nullptr);
    auto jsonOut = Corbomite::WorkspaceSerializer::toJson(mainWindow.get(), nullptr);

    auto children = jsonOut.value(QStringLiteral("main")).toObject()
                          .value(QStringLiteral("children")).toArray();
    QCOMPARE(children.size(), 2);

    // Group A should report currentTab = 2; group B should report 1.
    QSet<int> seen;
    for (const auto &v : children) {
        auto tabs = v.toObject();
        QCOMPARE(tabs.value(QStringLiteral("type")).toString(),
                 QStringLiteral("tabs"));
        seen.insert(tabs.value(QStringLiteral("currentTab")).toInt());
    }
    QVERIFY(seen.contains(2));
    QVERIFY(seen.contains(1));
}

void TestWorkspaceSerializer::fixture13_popoutNestedSplit_roundtrips()
{
    auto jsonIn = readFixture(QStringLiteral("13-popout-nested-split.json"));
    QVERIFY(!jsonIn.isEmpty());
    auto mainWindow = std::make_unique<KDDockWidgets::QtWidgets::MainWindow>(
        QStringLiteral("test-f13"), KDDockWidgets::MainWindowOption_None);
    mainWindow->show();

    Corbomite::WorkspaceSerializer::fromJson(jsonIn, mainWindow.get(), nullptr);

    auto *registry = KDDockWidgets::DockRegistry::self();
    QCOMPARE(registry->dockwidgets().size(), 3);
    QCOMPARE(registry->floatingWindows().size(), 1);

    auto jsonOut = Corbomite::WorkspaceSerializer::toJson(mainWindow.get(), nullptr);
    auto floatingOut = jsonOut.value(QStringLiteral("floating")).toObject()
                              .value(QStringLiteral("children")).toArray();
    QCOMPARE(floatingOut.size(), 1);
    auto win = floatingOut.first().toObject();
    auto winChildren = win.value(QStringLiteral("children")).toArray();
    QCOMPARE(winChildren.size(), 2);
    for (const auto &v : winChildren) {
        QCOMPARE(v.toObject().value(QStringLiteral("type")).toString(),
                 QStringLiteral("tabs"));
    }
}

void TestWorkspaceSerializer::fixture12_nestedWithState_workspaceRoundTrip()
{
    auto jsonIn = readFixture(QStringLiteral("12-nested-with-state.json"));
    QVERIFY(!jsonIn.isEmpty());

    Corbomite::ViewRegistry registry;
    Corbomite::Workspace workspace(QStringLiteral("test-vault-12"), &registry);
    auto *kddwMain = qobject_cast<KDDockWidgets::QtWidgets::MainWindow *>(
        workspace.rootWidget());
    QVERIFY(kddwMain);
    kddwMain->show();

    Corbomite::WorkspaceSerializer::fromJson(jsonIn, kddwMain, &workspace);

    auto *leafA1 = workspace.findLeafById(QStringLiteral("f12leafA1aaaaaaa"));
    auto *leafB1 = workspace.findLeafById(QStringLiteral("f12leafB1aaaaaaa"));
    auto *leafB2 = workspace.findLeafById(QStringLiteral("f12leafB2aaaaaaa"));
    QVERIFY(leafA1); QVERIFY(leafB1); QVERIFY(leafB2);

    QCOMPARE(leafA1->pinned(), true);
    QCOMPARE(leafA1->group(), QStringLiteral("linked-pair-1"));
    QCOMPARE(leafB1->group(), QStringLiteral("linked-pair-1"));

    auto jsonOut = Corbomite::WorkspaceSerializer::toJson(kddwMain, &workspace);
    auto outChildren = jsonOut.value(QStringLiteral("main")).toObject()
                              .value(QStringLiteral("children")).toArray();
    QCOMPARE(outChildren.size(), 2);

    bool sawA1 = false, sawB1 = false;
    for (const auto &v : outChildren) {
        auto tabs = v.toObject();
        for (const auto &lv : tabs.value(QStringLiteral("children")).toArray()) {
            auto leaf = lv.toObject();
            const QString id = leaf.value(QStringLiteral("id")).toString();
            if (id == QStringLiteral("f12leafA1aaaaaaa")) {
                sawA1 = true;
                QCOMPARE(leaf.value(QStringLiteral("pinned")).toBool(), true);
                QCOMPARE(leaf.value(QStringLiteral("group")).toString(),
                         QStringLiteral("linked-pair-1"));
            }
            if (id == QStringLiteral("f12leafB1aaaaaaa")) {
                sawB1 = true;
                QCOMPARE(leaf.value(QStringLiteral("group")).toString(),
                         QStringLiteral("linked-pair-1"));
            }
        }
    }
    QVERIFY(sawA1);
    QVERIFY(sawB1);
}

void TestWorkspaceSerializer::fixture08b_unknownKeys_workspaceRoundTrip()
{
    auto jsonIn = readFixture(QStringLiteral("08-unknown-keys.json"));
    QVERIFY(!jsonIn.isEmpty());

    Corbomite::ViewRegistry registry;
    Corbomite::Workspace workspace(QStringLiteral("test-vault-08b"), &registry);
    auto *kddwMain = qobject_cast<KDDockWidgets::QtWidgets::MainWindow *>(
        workspace.rootWidget());
    QVERIFY(kddwMain);
    kddwMain->show();

    Corbomite::WorkspaceSerializer::fromJson(jsonIn, kddwMain, &workspace);
    auto jsonOut = Corbomite::WorkspaceSerializer::toJson(kddwMain, &workspace);

    auto leaf = jsonOut.value(QStringLiteral("main")).toObject()
                       .value(QStringLiteral("children")).toArray()
                       .first().toObject()
                       .value(QStringLiteral("children")).toArray()
                       .first().toObject();
    auto obsidianInternal = leaf.value(QStringLiteral("obsidianInternal")).toObject();
    QCOMPARE(obsidianInternal.value(QStringLiteral("someField")).toInt(), 42);
}

void TestWorkspaceSerializer::fixture04b_stacked_workspaceRoundTrip()
{
    auto jsonIn = readFixture(QStringLiteral("04-stacked-tabs.json"));
    QVERIFY(!jsonIn.isEmpty());

    Corbomite::ViewRegistry registry;
    Corbomite::Workspace workspace(QStringLiteral("test-vault-04b"), &registry);
    auto *kddwMain = qobject_cast<KDDockWidgets::QtWidgets::MainWindow *>(
        workspace.rootWidget());
    QVERIFY(kddwMain);
    kddwMain->show();

    Corbomite::WorkspaceSerializer::fromJson(jsonIn, kddwMain, &workspace);
    auto jsonOut = Corbomite::WorkspaceSerializer::toJson(kddwMain, &workspace);

    auto tabsOut = jsonOut.value(QStringLiteral("main")).toObject()
                          .value(QStringLiteral("children")).toArray()
                          .first().toObject();
    QCOMPARE(tabsOut.value(QStringLiteral("type")).toString(),
             QStringLiteral("tabs"));
    QCOMPARE(tabsOut.value(QStringLiteral("stacked")).toBool(), true);
}

void TestWorkspaceSerializer::fixture14_deferSet_perGroupCurrentTabRespected()
{
    auto jsonIn = readFixture(QStringLiteral("14-defer-set.json"));
    QVERIFY(!jsonIn.isEmpty());

    Corbomite::ViewRegistry registry;
    Corbomite::Workspace workspace(QStringLiteral("test-vault-14"), &registry);
    auto *kddwMain = qobject_cast<KDDockWidgets::QtWidgets::MainWindow *>(
        workspace.rootWidget());
    QVERIFY(kddwMain);
    kddwMain->show();

    workspace.deserialize(jsonIn);

    // Active is A2 (group A's currentTab=1). Group B's currentTab=0 = B1.
    // Expected non-deferred: A2 (active), B1 (currentTab in B).
    // Expected deferred: A1, A3, B2, B3.
    auto isDeferred = [&](const QString &id) {
        auto *l = workspace.findLeafById(id);
        return l && l->isDeferred();
    };
    QCOMPARE(isDeferred(QStringLiteral("f14A2aaaaaaaaaaa")), false);
    QCOMPARE(isDeferred(QStringLiteral("f14B1aaaaaaaaaaa")), false);
    QCOMPARE(isDeferred(QStringLiteral("f14A1aaaaaaaaaaa")), true);
    QCOMPARE(isDeferred(QStringLiteral("f14A3aaaaaaaaaaa")), true);
    QCOMPARE(isDeferred(QStringLiteral("f14B2aaaaaaaaaaa")), true);
    QCOMPARE(isDeferred(QStringLiteral("f14B3aaaaaaaaaaa")), true);
}

// Regression test for a live-dogfood bug (Cluster L Phase L5, 2026-08-18):
// a real Obsidian-authored workspace.json with the split shape
// root(direction=vertical)[ nested(direction=horizontal)[topLeft, botLeft],
// right ] loaded with topLeft/botLeft/right scrambled across quadrants
// instead of matching Obsidian's own rendering (topLeft above botLeft in a
// left column, right spanning the full right column). Root causes: (1)
// "vertical"/"horizontal" were mapped to KDDW locations backwards —
// Obsidian's "direction" names the divider line's orientation, not the
// stacking axis of its children (verified against docs/obsidian-audit/
// domains/workspace.md); (2) materializeSplit's nested nested-split
// anchor threading placed later top-level siblings relative to the wrong
// descendant. Fixture 16 reproduces the exact shape that broke; this test
// asserts on-screen geometry, not just JSON shape, because the bug
// round-tripped a structurally-plausible-looking JSON while still
// rendering in the wrong quadrants — see fixture03 above, which passed
// throughout this bug's lifetime because it only checks group/tab counts.
void TestWorkspaceSerializer::fixture16_nestedSplitFirstChild_matchesObsidianGeometry()
{
    auto json = readFixture(QStringLiteral("16-nested-split-first-child.json"));
    QVERIFY(!json.isEmpty());
    auto mainWindow = std::make_unique<KDDockWidgets::QtWidgets::MainWindow>(
        QStringLiteral("test-f16"), KDDockWidgets::MainWindowOption_None);
    mainWindow->show();

    Corbomite::WorkspaceSerializer::fromJson(json, mainWindow.get(), nullptr);

    auto *registry = KDDockWidgets::DockRegistry::self();
    QCOMPARE(registry->dockwidgets().size(), 3);
    auto *topLeft = registry->dockByName(QStringLiteral("leaf16topleftaaa"));
    auto *botLeft = registry->dockByName(QStringLiteral("leaf16botleftaaa"));
    auto *right = registry->dockByName(QStringLiteral("leaf16rightaaaaa"));
    QVERIFY(topLeft);
    QVERIFY(botLeft);
    QVERIFY(right);

    auto *topLeftW = KDDockWidgets::QtCommon::View_qt::asQWidget(topLeft);
    auto *botLeftW = KDDockWidgets::QtCommon::View_qt::asQWidget(botLeft);
    auto *rightW = KDDockWidgets::QtCommon::View_qt::asQWidget(right);
    QVERIFY(topLeftW);
    QVERIFY(botLeftW);
    QVERIFY(rightW);

    mainWindow->resize(1200, 800);
    QCoreApplication::processEvents();
    QTest::qWait(50);

    const QPoint topLeftPos = topLeftW->mapTo(mainWindow.get(), QPoint(0, 0));
    const QPoint botLeftPos = botLeftW->mapTo(mainWindow.get(), QPoint(0, 0));
    const QPoint rightPos = rightW->mapTo(mainWindow.get(), QPoint(0, 0));

    // topLeft sits directly above botLeft in the same (left) column.
    QVERIFY2(topLeftPos.y() < botLeftPos.y(),
             "topLeft must be above botLeft, not beside or below it");
    QVERIFY2(qAbs(topLeftPos.x() - botLeftPos.x()) < 5,
             "topLeft and botLeft must share the same left column");

    // right sits in its own column, to the right of both — not stacked
    // below either one (empty leaf widgets report height()==0 offscreen,
    // so height-based span assertions aren't reliable here; the x-position
    // checks below are what actually distinguish the two known-wrong
    // layouts this bug produced from the correct one).
    QVERIFY2(rightPos.x() > topLeftPos.x() + topLeftW->width() - 5,
             "right must be to the right of the left column, not stacked below it");
    QVERIFY2(rightPos.x() > botLeftPos.x() + botLeftW->width() - 5,
             "right must be to the right of the left column, not stacked below it");
    QVERIFY2(rightPos.y() <= topLeftPos.y() + 5,
             "right must start at the top of the left column");
}


// Container ids and `dimension` are properties of the Obsidian node, and the
// KDDW substrate has no concept of either, so the live walk used to rebuild
// every container from scratch on each save — minting a brand-new random id
// for the root split and for every nested split and tab group, every time.
// Obsidian mints ids in the node's constructor and keeps them stable
// (app/own/1206-OI.js:9), so a vault synced between the two apps churned on
// every single save.
void TestWorkspaceSerializer::containerIds_stableAcrossSerializations()
{
    KDDockWidgets::DockRegistry::self()->clear();
    KDDockWidgets::initFrontend(KDDockWidgets::FrontendType::QtWidgets);
    Corbomite::ViewRegistry registry;
    Corbomite::Workspace workspace(QStringLiteral("test-vault-ids"), &registry);
    workspace.kddwMainWindow()->show();

    auto *a = workspace.createLeafInActiveGroup();
    QVERIFY(a);
    QVERIFY(workspace.createLeafInGroupOf(a));
    QVERIFY(workspace.splitLeaf(a, Qt::Horizontal));

    const QJsonObject first = workspace.serialize();
    const QJsonObject second = workspace.serialize();

    QVERIFY(!first.isEmpty());
    QCOMPARE(second, first);

    const auto main1 = first.value(QStringLiteral("main")).toObject();
    QCOMPARE(main1.value(QStringLiteral("id")).toString().size(), 16);
    const auto kids1 = main1.value(QStringLiteral("children")).toArray();
    const auto kids2 = second.value(QStringLiteral("main")).toObject()
                           .value(QStringLiteral("children")).toArray();
    QCOMPARE(kids1.size(), kids2.size());
    for (int i = 0; i < kids1.size(); ++i)
        QCOMPARE(kids2.at(i).toObject().value(QStringLiteral("id")).toString(),
                 kids1.at(i).toObject().value(QStringLiteral("id")).toString());
}

// WorkspaceWindow's constructor passes a literal "vertical" and
// deserializeLayout never feeds a window node's own direction back into it
// (app/own/1928-Y6.js:317; app/own/1932-r4.js:1132), so a window node always
// round-trips as "vertical". Fixture 13 says "horizontal" on the window node.
void TestWorkspaceSerializer::windowNode_directionAlwaysVertical()
{
    auto jsonIn = readFixture(QStringLiteral("13-popout-nested-split.json"));
    QVERIFY(!jsonIn.isEmpty());
    const auto windowIn = jsonIn.value(QStringLiteral("floating")).toObject()
                              .value(QStringLiteral("children")).toArray()
                              .first().toObject();
    QCOMPARE(windowIn.value(QStringLiteral("direction")).toString(),
             QStringLiteral("horizontal"));

    auto mainWindow = std::make_unique<KDDockWidgets::QtWidgets::MainWindow>(
        QStringLiteral("test-f13-dir"), KDDockWidgets::MainWindowOption_None);
    mainWindow->show();

    Corbomite::WorkspaceSerializer::fromJson(jsonIn, mainWindow.get(), nullptr);
    const auto windowOut = Corbomite::WorkspaceSerializer::toJson(mainWindow.get(), nullptr)
                               .value(QStringLiteral("floating")).toObject()
                               .value(QStringLiteral("children")).toArray()
                               .first().toObject();
    QCOMPARE(windowOut.value(QStringLiteral("direction")).toString(),
             QStringLiteral("vertical"));
}

// The read half of the direction contract, where it is actually load-bearing:
// setLayout forces the restored root split to "vertical" whatever the file
// said (app/own/1932-r4.js:1016). "vertical" means left-to-right siblings, so
// the root's second pane must land to the right of the first, not below it.
void TestWorkspaceSerializer::rootSplit_directionForcedVerticalOnLoad()
{
    QJsonObject root = layoutWith(QJsonArray{
        tabsNode(QStringLiteral("roottabsaaaaaa"), QStringLiteral("rootleftaaaaa")),
        tabsNode(QStringLiteral("roottabsbbbbb"), QStringLiteral("rootrightaaaa")),
    })
                            .value(QStringLiteral("main")).toObject();
    root[QStringLiteral("direction")] = QStringLiteral("horizontal");

    auto mainWindow = std::make_unique<KDDockWidgets::QtWidgets::MainWindow>(
        QStringLiteral("test-root-dir"), KDDockWidgets::MainWindowOption_None);
    mainWindow->show();

    QJsonObject jsonIn;
    jsonIn[QStringLiteral("main")] = root;
    Corbomite::WorkspaceSerializer::fromJson(jsonIn, mainWindow.get(), nullptr);
    mainWindow->resize(1200, 800);
    QCoreApplication::processEvents();
    QTest::qWait(50);

    auto *left = KDDockWidgets::DockRegistry::self()->dockByName(
        QStringLiteral("rootleftaaaaa"));
    auto *right = KDDockWidgets::DockRegistry::self()->dockByName(
        QStringLiteral("rootrightaaaa"));
    QVERIFY(left);
    QVERIFY(right);
    auto *leftW = KDDockWidgets::QtCommon::View_qt::asQWidget(left);
    auto *rightW = KDDockWidgets::QtCommon::View_qt::asQWidget(right);
    QVERIFY(leftW);
    QVERIFY(rightW);
    QVERIFY2(leftW->width() > 100, "the layout must actually have been sized");

    const QPoint leftPos = leftW->mapTo(mainWindow.get(), QPoint(0, 0));
    const QPoint rightPos = rightW->mapTo(mainWindow.get(), QPoint(0, 0));
    QVERIFY2(rightPos.x() > leftPos.x() + leftW->width() - 5,
             "a restored root split is forced to \"vertical\", so its second "
             "pane sits to the right of the first, not below it");
    QVERIFY2(qAbs(rightPos.y() - leftPos.y()) < 5,
             "side-by-side panes share a row");
}

// Obsidian assigns maximize inside a try/catch, so every window node carries
// the key rather than only the maximized ones (app/own/1928-Y6.js:364-367).
// An unmangled window therefore writes `maximize: false`, not nothing.
void TestWorkspaceSerializer::windowNode_maximizeWrittenUnconditionally()
{
    auto jsonIn = readFixture(QStringLiteral("13-popout-nested-split.json"));
    QVERIFY(!jsonIn.isEmpty());
    auto mainWindow = std::make_unique<KDDockWidgets::QtWidgets::MainWindow>(
        QStringLiteral("test-f13-max"), KDDockWidgets::MainWindowOption_None);
    mainWindow->show();

    Corbomite::WorkspaceSerializer::fromJson(jsonIn, mainWindow.get(), nullptr);
    const QJsonObject jsonOut =
        Corbomite::WorkspaceSerializer::toJson(mainWindow.get(), nullptr);
    const auto windowOut = jsonOut.value(QStringLiteral("floating")).toObject()
                               .value(QStringLiteral("children")).toArray()
                               .first().toObject();
    QVERIFY2(windowOut.contains(QStringLiteral("maximize")),
             "an un-maximized window node must still carry maximize: false");
    QCOMPARE(windowOut.value(QStringLiteral("maximize")).toBool(), false);
}

// `dimension` is parsed and re-emitted verbatim across a save: an in-range
// value survives, so a pane sized 60/40 in Obsidian is still 60/40 here.
void TestWorkspaceSerializer::dimension_survivesRoundTrip()
{
    const QJsonObject jsonIn = layoutWith(QJsonArray{
        tabsNode(QStringLiteral("tabsAaaaaaaaaaa"), QStringLiteral("leafAaaaaaaaaa"), 60),
        tabsNode(QStringLiteral("tabsBbbbbbbbbb"), QStringLiteral("leafBbbbbbbbbb"), 40),
    });
    auto mainWindow = std::make_unique<KDDockWidgets::QtWidgets::MainWindow>(
        QStringLiteral("test-dim-rt"), KDDockWidgets::MainWindowOption_None);
    mainWindow->show();

    Corbomite::WorkspaceSerializer::fromJson(jsonIn, mainWindow.get(), nullptr);
    const QJsonObject jsonOut =
        Corbomite::WorkspaceSerializer::toJson(mainWindow.get(), nullptr);
    const auto main = jsonOut.value(QStringLiteral("main")).toObject();

    QCOMPARE(nodeForLeaf(main, QStringLiteral("leafAaaaaaaaaa"))
                 .value(QStringLiteral("dimension")).toInt(-1), 60);
    QCOMPARE(nodeForLeaf(main, QStringLiteral("leafBbbbbbbbbb"))
                 .value(QStringLiteral("dimension")).toInt(-1), 40);
}

// WorkspaceItem.setDimension's clamp is inclusive on both ends — `0` and
// `100` are both out of range — and WorkspaceSplit.recomputeChildrenDimensions
// is all-or-nothing, so one flexible pane resets every sibling
// (app/own/1206-OI.js:51-52, :134-146). A lone child normalises to exactly
// 100, which the clamp then drops.
void TestWorkspaceSerializer::dimension_clampIsInclusiveAndPoisoning()
{
    const QJsonValue unset = QJsonValue::Undefined;
    const QList<QJsonArray> cases = {
        // Control: both in range, sum 100, nothing changes.
        QJsonArray{tabsNode(QStringLiteral("ta"), QStringLiteral("la"), 60),
                   tabsNode(QStringLiteral("tb"), QStringLiteral("lb"), 40)},
        // `100` is out of range → null → poisons the whole sibling set.
        QJsonArray{tabsNode(QStringLiteral("ta"), QStringLiteral("la"), 60),
                   tabsNode(QStringLiteral("tb"), QStringLiteral("lb"), 100)},
        // `0` likewise.
        QJsonArray{tabsNode(QStringLiteral("ta"), QStringLiteral("la"), 60),
                   tabsNode(QStringLiteral("tb"), QStringLiteral("lb"), 0)},
        // A single flexible pane poisons its sibling.
        QJsonArray{tabsNode(QStringLiteral("ta"), QStringLiteral("la"), 60),
                   tabsNode(QStringLiteral("tb"), QStringLiteral("lb"), unset)},
        // A lone child normalises to 100 and is then dropped.
        QJsonArray{tabsNode(QStringLiteral("ta"), QStringLiteral("la"), 60)},
    };
    const QList<bool> expectKept{true, false, false, false, false};

    for (int i = 0; i < cases.size(); ++i) {
        auto mainWindow = std::make_unique<KDDockWidgets::QtWidgets::MainWindow>(
            QStringLiteral("test-dim-clamp-%1").arg(i),
            KDDockWidgets::MainWindowOption_None);
        mainWindow->show();
        Corbomite::WorkspaceSerializer::fromJson(
            layoutWith(cases.at(i)), mainWindow.get(), nullptr);
        const auto main =
            Corbomite::WorkspaceSerializer::toJson(mainWindow.get(), nullptr)
                .value(QStringLiteral("main")).toObject();

        // Every child of the root is a single-leaf tab group here. Case 0 is
        // the only survivor, and its two values are the input's, unchanged
        // (they already sum to 100).
        const auto kids = main.value(QStringLiteral("children")).toArray();
        QCOMPARE(kids.size(), cases.at(i).size());
        for (int k = 0; k < kids.size(); ++k) {
            const auto node = kids.at(k).toObject();
            const bool kept = node.contains(QStringLiteral("dimension"));
            QVERIFY2(kept == expectKept.at(i),
                     qPrintable(QStringLiteral("case %1 child %2: dimension %3")
                                    .arg(i).arg(k)
                                    .arg(kept ? QStringLiteral("kept")
                                              : QStringLiteral("dropped"))));
            if (kept)
                QCOMPARE(node.value(QStringLiteral("dimension")).toInt(-1),
                         k == 0 ? 60 : 40);
        }
    }
}

// Root key order is getLayout's assignment order plus saveLayout's
// lastOpenFiles append (app/own/1932-r4.js:1163, :1173). Keys this writer
// does not model trail in QJsonObject's own order.
void TestWorkspaceSerializer::jsonText_rootKeyOrderMatchesObsidian()
{
    QJsonObject root;
    root[QStringLiteral("main")] = layoutWith({}).value(QStringLiteral("main"));
    root[QStringLiteral("active")] = QStringLiteral("leafAaaaaaaaaa");
    root[QStringLiteral("lastOpenFiles")] = QJsonArray{QStringLiteral("a.md")};
    root[QStringLiteral("left-ribbon")] =
        QJsonObject{{QStringLiteral("hiddenItems"), QJsonObject()}};
    root[QStringLiteral("right")] = QJsonObject();
    root[QStringLiteral("floating")] = QJsonObject();
    root[QStringLiteral("left")] = QJsonObject();
    root[QStringLiteral("aFutureObsidianKey")] = 1;

    QCOMPARE(keysAtIndent(Corbomite::WorkspaceSerializer::toJsonText(root), 2),
             QStringList({QStringLiteral("main"), QStringLiteral("left"),
                          QStringLiteral("right"), QStringLiteral("left-ribbon"),
                          QStringLiteral("floating"), QStringLiteral("active"),
                          QStringLiteral("lastOpenFiles"),
                          QStringLiteral("aFutureObsidianKey")}));
}

// Per-node key order. `direction` lands *after* `children`: the split's
// serialize() calls WorkspaceContainer's first, and that is what appends
// `children` (app/own/1206-OI.js:67-71, :129-131). A leaf's `state` value is
// a ViewState, ordered by getViewState (app/own/1209-jI.js:641-648).
void TestWorkspaceSerializer::jsonText_nodeKeyOrderMatchesObsidian()
{
    QJsonObject viewState;
    viewState[QStringLiteral("type")] = QStringLiteral("markdown");
    viewState[QStringLiteral("state")] =
        QJsonObject{{QStringLiteral("file"), QStringLiteral("n.md")}};
    viewState[QStringLiteral("pinned")] = true;
    viewState[QStringLiteral("icon")] = QStringLiteral("lucide-file-text");
    viewState[QStringLiteral("title")] = QStringLiteral("n");
    QJsonObject leaf;
    leaf[QStringLiteral("id")] = QStringLiteral("leafAaaaaaaaaa");
    leaf[QStringLiteral("type")] = QStringLiteral("leaf");
    leaf[QStringLiteral("pinned")] = true;
    leaf[QStringLiteral("state")] = viewState;
    leaf[QStringLiteral("group")] = QStringLiteral("grp");
    QJsonObject tabs = tabsNode(QStringLiteral("tabsAaaaaaaaaaa"),
                                QStringLiteral("leafAaaaaaaaaa"), 60);
    tabs[QStringLiteral("children")] = QJsonArray{leaf};
    tabs[QStringLiteral("currentTab")] = 2;
    tabs[QStringLiteral("stacked")] = true;
    QJsonObject root;
    root[QStringLiteral("id")] = QStringLiteral("rootid000000000");
    root[QStringLiteral("type")] = QStringLiteral("split");
    root[QStringLiteral("direction")] = QStringLiteral("vertical");
    root[QStringLiteral("children")] = QJsonArray{tabs};
    QJsonObject doc;
    doc[QStringLiteral("main")] = root;

    const QByteArray text = Corbomite::WorkspaceSerializer::toJsonText(doc);
    QCOMPARE(keysAtIndent(text, 4),
             QStringList({QStringLiteral("id"), QStringLiteral("type"),
                          QStringLiteral("children"), QStringLiteral("direction")}));
    QCOMPARE(keysAtIndent(text, 8),
             QStringList({QStringLiteral("id"), QStringLiteral("type"),
                          QStringLiteral("dimension"), QStringLiteral("children"),
                          QStringLiteral("currentTab"), QStringLiteral("stacked")}));
    QCOMPARE(keysAtIndent(text, 12),
             QStringList({QStringLiteral("id"), QStringLiteral("type"),
                          QStringLiteral("pinned"), QStringLiteral("state"),
                          QStringLiteral("group")}));

    // The leaf's ViewState sits one level inside the leaf's own `state`, and
    // getViewState writes icon/title for any view present, not just deferred
    // ones (app/own/1209-jI.js:641-648).
    int stateIndent = -1;
    const auto lines = QString::fromUtf8(text).split(QLatin1Char('\n'));
    for (const auto &line : lines) {
        if (line.endsWith(QLatin1String("\"state\": {"))) {
            stateIndent = static_cast<int>(line.size() - line.trimmed().size());
            break;
        }
    }
    QVERIFY(stateIndent > 0);
    QCOMPARE(keysAtIndent(text, stateIndent + 2),
             QStringList({QStringLiteral("type"), QStringLiteral("state"),
                          QStringLiteral("pinned"), QStringLiteral("icon"),
                          QStringLiteral("title")}));
}

// The emitter only reorders members; it must never alter values. A re-parse
// has to come back equal to the document QJsonDocument would have written.
void TestWorkspaceSerializer::jsonText_reparsesToEquivalentDocument()
{
    QJsonObject leaf = leafNode(QStringLiteral("leaf\"quoted\\slash\nnewline"));
    leaf[QStringLiteral("pinned")] = true;
    leaf[QStringLiteral("obsidianInternal")] =
        QJsonObject{{QStringLiteral("n"), 42},
                    {QStringLiteral("deep"),
                     QJsonArray{QJsonValue(1.5), QJsonValue(true),
                                QJsonValue(QJsonValue::Null),
                                QJsonObject{{QStringLiteral("k"), QString()}}}}};
    QJsonObject tabs = tabsNode(QStringLiteral("tabsAaaaaaaaaaa"),
                                QStringLiteral("leafAaaaaaaaaa"), 33.25);
    tabs[QStringLiteral("children")] = QJsonArray{leaf, leafNode(QStringLiteral("other"))};
    QJsonObject root;
    root[QStringLiteral("id")] = QStringLiteral("rootid000000000");
    root[QStringLiteral("type")] = QStringLiteral("split");
    root[QStringLiteral("direction")] = QStringLiteral("vertical");
    root[QStringLiteral("children")] = QJsonArray{tabs};
    QJsonObject doc;
    doc[QStringLiteral("main")] = root;
    doc[QStringLiteral("lastOpenFiles")] = QJsonArray{QStringLiteral("a.md")};

    const QJsonDocument reparsed =
        QJsonDocument::fromJson(Corbomite::WorkspaceSerializer::toJsonText(doc));
    QVERIFY2(!reparsed.isNull(), "emitted text must be valid JSON");
    QCOMPARE(reparsed.object(), QJsonDocument(doc).object());
}

QTEST_MAIN(TestWorkspaceSerializer)
#include "tst_workspace_serializer.moc"
