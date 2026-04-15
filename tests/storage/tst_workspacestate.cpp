// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/WorkspaceState.h"

using namespace Corbomite;

namespace {

void writeRaw(const QString &path, const QByteArray &bytes)
{
    QFile f(path);
    QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(path));
    f.write(bytes);
}

QByteArray readRaw(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return f.readAll();
}

} // namespace

class TestWorkspaceState : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void loadMissingReturnsNullopt()
    {
        FileSystemAdapter fs;
        const auto ws = WorkspaceState::load(&fs, QStringLiteral("/nope.json"));
        QVERIFY(!ws.has_value());
    }

    // --- Round-trip unknown keys: load → save → load preserves nested
    //     plugin/unknown data at every tree level. ---

    void roundTripPreservesUnknownKeysAtRootAndNodes()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        const QString path = tmp.path() + QStringLiteral("/workspace.json");

        const QByteArray original = R"({
  "main": {
    "id": "root-main",
    "type": "split",
    "direction": "vertical",
    "_pluginExtraField": { "foo": 1 },
    "children": [
      {
        "id": "tabs-a",
        "type": "tabs",
        "_unknownTab": "data",
        "children": [
          {
            "id": "leaf-1",
            "type": "leaf",
            "state": {
              "type": "markdown",
              "state": { "file": "note.md", "mode": "source" }
            }
          }
        ],
        "currentTab": 0
      }
    ]
  },
  "left": {
    "id": "left-root",
    "type": "split",
    "direction": "horizontal",
    "children": []
  },
  "active": "leaf-1",
  "lastOpenFiles": ["note.md", "other.md"],
  "_futureTopLevelKey": 42
})";
        writeRaw(path, original);

        auto ws = WorkspaceState::load(&fs, path);
        QVERIFY(ws.has_value());

        QVERIFY(ws->save(&fs, path));

        auto ws2 = WorkspaceState::load(&fs, path);
        QVERIFY(ws2.has_value());

        // Top-level unknown survived.
        QCOMPARE(ws2->raw().value(QStringLiteral("_futureTopLevelKey")).toInt(), 42);

        // Node-level unknown survived.
        const auto mainNode = ws2->main();
        QVERIFY(mainNode.contains(QStringLiteral("_pluginExtraField")));
        const auto firstChild = WorkspaceState::children(mainNode).at(0).toObject();
        QVERIFY(firstChild.contains(QStringLiteral("_unknownTab")));

        // Leaf ViewState intact.
        const auto leaf = WorkspaceState::children(firstChild).at(0).toObject();
        const auto state = leaf.value(QStringLiteral("state")).toObject();
        QCOMPARE(state.value(QStringLiteral("type")).toString(),
                 QStringLiteral("markdown"));
    }

    // --- Typed accessors ---

    void mainLeftRightAccessors()
    {
        WorkspaceState ws;
        QJsonObject mainNode;
        mainNode.insert(QStringLiteral("id"), QStringLiteral("m"));
        mainNode.insert(QStringLiteral("type"), QStringLiteral("split"));
        ws.setMain(mainNode);
        ws.setLeft(QJsonObject{{QStringLiteral("id"), QStringLiteral("L")}});
        ws.setRight(QJsonObject{{QStringLiteral("id"), QStringLiteral("R")}});

        QCOMPARE(ws.main().value(QStringLiteral("id")).toString(), QStringLiteral("m"));
        QCOMPARE(ws.left().value(QStringLiteral("id")).toString(), QStringLiteral("L"));
        QCOMPARE(ws.right().value(QStringLiteral("id")).toString(), QStringLiteral("R"));
    }

    void lastOpenFilesAndActive()
    {
        WorkspaceState ws;
        ws.setLastOpenFiles({QStringLiteral("a.md"), QStringLiteral("b/c.md")});
        ws.setActiveLeafId(QStringLiteral("leaf-xyz"));

        QCOMPARE(ws.lastOpenFiles().size(), 2);
        QCOMPARE(ws.lastOpenFiles().first(), QStringLiteral("a.md"));
        QCOMPARE(ws.activeLeafId(), QStringLiteral("leaf-xyz"));
    }

    // --- NodeType discriminator ---

    void typeOfAllVariants()
    {
        using NT = WorkspaceState::NodeType;
        auto make = [](const QString &t) {
            QJsonObject o;
            o.insert(QStringLiteral("type"), t);
            return o;
        };
        QCOMPARE(WorkspaceState::typeOf(make(QStringLiteral("split"))), NT::Split);
        QCOMPARE(WorkspaceState::typeOf(make(QStringLiteral("tabs"))), NT::Tabs);
        QCOMPARE(WorkspaceState::typeOf(make(QStringLiteral("leaf"))), NT::Leaf);
        QCOMPARE(WorkspaceState::typeOf(make(QStringLiteral("floating"))), NT::Floating);
        QCOMPARE(WorkspaceState::typeOf(make(QStringLiteral("window"))), NT::Window);
        QCOMPARE(WorkspaceState::typeOf(make(QStringLiteral("mobile-drawer"))), NT::MobileDrawer);
        QCOMPARE(WorkspaceState::typeOf(make(QStringLiteral("future-type"))), NT::Unknown);
        QCOMPARE(WorkspaceState::typeOf(QJsonObject{}), NT::Unknown);
    }

    void typeStringMatchesInput()
    {
        using NT = WorkspaceState::NodeType;
        QCOMPARE(WorkspaceState::typeString(NT::Split), QStringLiteral("split"));
        QCOMPARE(WorkspaceState::typeString(NT::Tabs), QStringLiteral("tabs"));
        QCOMPARE(WorkspaceState::typeString(NT::Leaf), QStringLiteral("leaf"));
        QCOMPARE(WorkspaceState::typeString(NT::Floating), QStringLiteral("floating"));
        QCOMPARE(WorkspaceState::typeString(NT::Window), QStringLiteral("window"));
        QCOMPARE(WorkspaceState::typeString(NT::MobileDrawer), QStringLiteral("mobile-drawer"));
    }

    // --- walk() visits the tree pre-order ---

    void walkVisitsNodesInPreOrder()
    {
        QJsonObject leaf1;
        leaf1.insert(QStringLiteral("id"), QStringLiteral("leaf1"));
        leaf1.insert(QStringLiteral("type"), QStringLiteral("leaf"));
        QJsonObject leaf2;
        leaf2.insert(QStringLiteral("id"), QStringLiteral("leaf2"));
        leaf2.insert(QStringLiteral("type"), QStringLiteral("leaf"));

        QJsonObject tabs;
        tabs.insert(QStringLiteral("id"), QStringLiteral("tabs"));
        tabs.insert(QStringLiteral("type"), QStringLiteral("tabs"));
        tabs.insert(QStringLiteral("children"), QJsonArray{leaf1, leaf2});

        QJsonObject split;
        split.insert(QStringLiteral("id"), QStringLiteral("split"));
        split.insert(QStringLiteral("type"), QStringLiteral("split"));
        split.insert(QStringLiteral("children"), QJsonArray{tabs});

        QStringList ids;
        WorkspaceState::walk(split, [&](const QJsonObject &n) {
            ids.append(n.value(QStringLiteral("id")).toString());
            return true;
        });
        QCOMPARE(ids, (QStringList{QStringLiteral("split"), QStringLiteral("tabs"),
                                   QStringLiteral("leaf1"), QStringLiteral("leaf2")}));
    }

    void walkStopsWhenVisitorReturnsFalse()
    {
        QJsonObject root;
        root.insert(QStringLiteral("id"), QStringLiteral("root"));
        root.insert(QStringLiteral("type"), QStringLiteral("split"));
        root.insert(QStringLiteral("children"),
                    QJsonArray{QJsonObject{{QStringLiteral("id"), QStringLiteral("c1")}}});

        int count = 0;
        WorkspaceState::walk(root, [&](const QJsonObject &) {
            ++count;
            return false; // stop after first
        });
        QCOMPARE(count, 1);
    }

    // --- Serialisation format (match Obsidian) ---

    void saveFormatIs2SpaceIndentNoTrailingNewline()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        const QString path = tmp.path() + QStringLiteral("/workspace.json");

        WorkspaceState ws;
        QJsonObject m;
        m.insert(QStringLiteral("id"), QStringLiteral("root"));
        m.insert(QStringLiteral("type"), QStringLiteral("split"));
        ws.setMain(m);
        QVERIFY(ws.save(&fs, path));

        const QByteArray bytes = readRaw(path);
        QVERIFY(!bytes.endsWith('\n'));
        // main's nested fields are at depth 2 → 4 spaces.
        QVERIFY(bytes.contains("\n    \"id\":"));
    }
};

QTEST_APPLESS_MAIN(TestWorkspaceState)
#include "tst_workspacestate.moc"
