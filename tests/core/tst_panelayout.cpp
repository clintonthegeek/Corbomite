// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "corbomite/core/PaneLayout.h"

using namespace Corbomite;

class TestPaneLayout : public QObject
{
    Q_OBJECT

private:
    static PaneLeaf makeLeaf(const QString &file, const QString &mode = QStringLiteral("source"))
    {
        PaneLeaf l;
        l.id = PaneLayout::newId();
        l.viewType = QStringLiteral("markdown");
        l.filePath = file;
        l.mode = mode;
        return l;
    }

private Q_SLOTS:

    // --- AreaIndex-pattern invariants ---

    void emptyRootIsLeaf()
    {
        PaneLayout layout;
        QVERIFY(layout.root() != nullptr);
        QVERIFY(!layout.root()->isSplit());
        QCOMPARE(layout.root()->viewCount(), 0);
    }

    void addViewAppendsToLeaf()
    {
        PaneLayout layout;
        layout.root()->addView(makeLeaf(QStringLiteral("a.md")));
        layout.root()->addView(makeLeaf(QStringLiteral("b.md")));
        QCOMPARE(layout.root()->viewCount(), 2);
        QCOMPARE(layout.root()->viewAt(0)->filePath, QStringLiteral("a.md"));
        QCOMPARE(layout.root()->viewAt(1)->filePath, QStringLiteral("b.md"));
    }

    void addViewOnSplitNodeIsNoOp()
    {
        PaneLayout layout;
        layout.root()->addView(makeLeaf(QStringLiteral("a.md")));
        layout.root()->split(Qt::Horizontal);
        layout.root()->addView(makeLeaf(QStringLiteral("c.md")));
        // Root is split: add should be ignored.
        QCOMPARE(layout.root()->viewCount(), 0);
        QCOMPARE(layout.root()->first()->viewCount(), 1);
    }

    void splitMovesExistingViewsToFirstChild()
    {
        PaneLayout layout;
        layout.root()->addView(makeLeaf(QStringLiteral("a.md")));
        layout.root()->addView(makeLeaf(QStringLiteral("b.md")));
        layout.root()->split(Qt::Horizontal);
        QVERIFY(layout.root()->isSplit());
        QCOMPARE(layout.root()->viewCount(), 0);
        QCOMPARE(layout.root()->first()->viewCount(), 2);
        QCOMPARE(layout.root()->second()->viewCount(), 0);
        QCOMPARE(layout.root()->orientation(), Qt::Horizontal);
    }

    void splitMoveToSecondVariant()
    {
        PaneLayout layout;
        layout.root()->addView(makeLeaf(QStringLiteral("a.md")));
        layout.root()->split(Qt::Vertical, /*moveViewsToSecond=*/true);
        QCOMPARE(layout.root()->first()->viewCount(), 0);
        QCOMPARE(layout.root()->second()->viewCount(), 1);
        QCOMPARE(layout.root()->orientation(), Qt::Vertical);
    }

    void splitWithNewLeafPlacesInSecond()
    {
        PaneLayout layout;
        layout.root()->addView(makeLeaf(QStringLiteral("existing.md")));
        layout.root()->splitWithNewLeaf(makeLeaf(QStringLiteral("new.md")),
                                        Qt::Horizontal);
        QCOMPARE(layout.root()->first()->viewAt(0)->filePath,
                 QStringLiteral("existing.md"));
        QCOMPARE(layout.root()->second()->viewAt(0)->filePath,
                 QStringLiteral("new.md"));
    }

    // --- The critical auto-unsplit-on-empty cascade ---

    void removeLastViewUnsplitsParent()
    {
        PaneLayout layout;
        layout.root()->addView(makeLeaf(QStringLiteral("a.md")));
        layout.root()->splitWithNewLeaf(makeLeaf(QStringLiteral("b.md")),
                                        Qt::Horizontal);
        // Now root has first={a}, second={b}. Remove 'b' and the parent
        // should auto-unsplit: root becomes leaf with just 'a'.
        const QString bId = layout.root()->second()->viewAt(0)->id;
        layout.root()->second()->removeView(bId);

        QVERIFY(!layout.root()->isSplit());
        QCOMPARE(layout.root()->viewCount(), 1);
        QCOMPARE(layout.root()->viewAt(0)->filePath, QStringLiteral("a.md"));
    }

    void nestedUnsplitCollapsesChildrenUp()
    {
        // Build: root={split H: left={a}, right={split V: {b}, {c}}}
        // Remove c → right becomes leaf {b}
        // Remove b → right becomes empty, parent unsplits → root becomes leaf {a}
        PaneLayout layout;
        layout.root()->addView(makeLeaf(QStringLiteral("a.md")));
        layout.root()->splitWithNewLeaf(makeLeaf(QStringLiteral("b.md")),
                                        Qt::Horizontal);
        layout.root()->second()->splitWithNewLeaf(
            makeLeaf(QStringLiteral("c.md")), Qt::Vertical);

        // Verify structure before
        QVERIFY(layout.root()->isSplit());
        QVERIFY(layout.root()->second()->isSplit());

        // Remove 'c' — second becomes leaf {b}
        const QString cId = layout.root()->second()->second()->viewAt(0)->id;
        layout.root()->second()->second()->removeView(cId);
        QVERIFY(!layout.root()->second()->isSplit());
        QCOMPARE(layout.root()->second()->viewCount(), 1);
        QCOMPARE(layout.root()->second()->viewAt(0)->filePath,
                 QStringLiteral("b.md"));

        // Remove 'b' — second becomes empty, root unsplits
        const QString bId = layout.root()->second()->viewAt(0)->id;
        layout.root()->second()->removeView(bId);
        QVERIFY(!layout.root()->isSplit());
        QCOMPARE(layout.root()->viewAt(0)->filePath, QStringLiteral("a.md"));
    }

    void moveViewPositionWithinLeaf()
    {
        PaneLayout layout;
        layout.root()->addView(makeLeaf(QStringLiteral("a.md")));
        layout.root()->addView(makeLeaf(QStringLiteral("b.md")));
        layout.root()->addView(makeLeaf(QStringLiteral("c.md")));
        const QString aId = layout.root()->viewAt(0)->id;
        layout.root()->moveViewPosition(aId, 2);
        QCOMPARE(layout.root()->viewAt(0)->filePath, QStringLiteral("b.md"));
        QCOMPARE(layout.root()->viewAt(1)->filePath, QStringLiteral("c.md"));
        QCOMPARE(layout.root()->viewAt(2)->filePath, QStringLiteral("a.md"));
    }

    void walkVisitsPreOrder()
    {
        PaneLayout layout;
        layout.root()->addView(makeLeaf(QStringLiteral("a.md")));
        layout.root()->splitWithNewLeaf(makeLeaf(QStringLiteral("b.md")),
                                        Qt::Horizontal);

        QStringList visited;
        layout.root()->walk([&](const PaneLayoutIndex *n) {
            if (n->isSplit()) visited.append(QStringLiteral("split"));
            else if (n->viewCount() > 0)
                visited.append(n->viewAt(0)->filePath);
            else
                visited.append(QStringLiteral("empty"));
            return true;
        });
        QCOMPARE(visited, (QStringList{QStringLiteral("split"),
                                       QStringLiteral("a.md"),
                                       QStringLiteral("b.md")}));
    }

    void findLeafByIdAcrossTree()
    {
        PaneLayout layout;
        PaneLeaf deep = makeLeaf(QStringLiteral("deep.md"));
        const QString targetId = deep.id;
        layout.root()->addView(makeLeaf(QStringLiteral("surface.md")));
        layout.root()->splitWithNewLeaf(std::move(deep), Qt::Horizontal);

        auto *found = layout.findLeaf(targetId);
        QVERIFY(found);
        QCOMPARE(found->filePath, QStringLiteral("deep.md"));

        QVERIFY(layout.findLeaf(QStringLiteral("nope-missing")) == nullptr);
    }

    // --- Obsidian SplitNode JSON round-trip ---

    void fromJsonLeafOnly()
    {
        QJsonObject leaf;
        leaf.insert(QStringLiteral("id"), QStringLiteral("leaf-xyz"));
        leaf.insert(QStringLiteral("type"), QStringLiteral("leaf"));
        QJsonObject state;
        state.insert(QStringLiteral("type"), QStringLiteral("markdown"));
        QJsonObject inner;
        inner.insert(QStringLiteral("file"), QStringLiteral("note.md"));
        inner.insert(QStringLiteral("mode"), QStringLiteral("source"));
        state.insert(QStringLiteral("state"), inner);
        leaf.insert(QStringLiteral("state"), state);

        auto layout = PaneLayout::fromJson(leaf);
        QVERIFY(!layout.root()->isSplit());
        QCOMPARE(layout.root()->viewCount(), 1);
        const auto *l = layout.root()->viewAt(0);
        QCOMPARE(l->id, QStringLiteral("leaf-xyz"));
        QCOMPARE(l->viewType, QStringLiteral("markdown"));
        QCOMPARE(l->filePath, QStringLiteral("note.md"));
        QCOMPARE(l->mode, QStringLiteral("source"));
    }

    void fromJsonSplitTree()
    {
        // split(H) { leaf(a), tabs(currentTab=1) { leaf(b), leaf(c) } }
        QJsonObject leafA;
        leafA.insert(QStringLiteral("type"), QStringLiteral("leaf"));
        leafA.insert(QStringLiteral("state"),
            QJsonObject{{QStringLiteral("type"), QStringLiteral("markdown")},
                        {QStringLiteral("state"),
                         QJsonObject{{QStringLiteral("file"),
                                      QStringLiteral("a.md")}}}});

        QJsonObject leafB = leafA;
        leafB.insert(QStringLiteral("state"),
            QJsonObject{{QStringLiteral("type"), QStringLiteral("markdown")},
                        {QStringLiteral("state"),
                         QJsonObject{{QStringLiteral("file"),
                                      QStringLiteral("b.md")}}}});
        QJsonObject leafC = leafA;
        leafC.insert(QStringLiteral("state"),
            QJsonObject{{QStringLiteral("type"), QStringLiteral("markdown")},
                        {QStringLiteral("state"),
                         QJsonObject{{QStringLiteral("file"),
                                      QStringLiteral("c.md")}}}});

        QJsonObject tabs;
        tabs.insert(QStringLiteral("id"), QStringLiteral("tabs-1"));
        tabs.insert(QStringLiteral("type"), QStringLiteral("tabs"));
        tabs.insert(QStringLiteral("currentTab"), 1);
        tabs.insert(QStringLiteral("children"), QJsonArray{leafB, leafC});

        QJsonObject split;
        split.insert(QStringLiteral("id"), QStringLiteral("root"));
        split.insert(QStringLiteral("type"), QStringLiteral("split"));
        split.insert(QStringLiteral("direction"), QStringLiteral("horizontal"));
        split.insert(QStringLiteral("children"), QJsonArray{leafA, tabs});

        auto layout = PaneLayout::fromJson(split);
        QVERIFY(layout.root()->isSplit());
        QCOMPARE(layout.root()->orientation(), Qt::Horizontal);
        QCOMPARE(layout.root()->first()->viewAt(0)->filePath,
                 QStringLiteral("a.md"));
        QCOMPARE(layout.root()->second()->viewCount(), 2);
        QCOMPARE(layout.root()->second()->currentTab(), 1);
        QCOMPARE(layout.root()->second()->viewAt(0)->filePath,
                 QStringLiteral("b.md"));
    }

    void unknownNodeTypeSkipped()
    {
        // future "floating" type under a split should be silently dropped
        QJsonObject split;
        split.insert(QStringLiteral("type"), QStringLiteral("split"));
        split.insert(QStringLiteral("direction"), QStringLiteral("vertical"));
        QJsonObject leafA;
        leafA.insert(QStringLiteral("type"), QStringLiteral("leaf"));
        leafA.insert(QStringLiteral("state"),
            QJsonObject{{QStringLiteral("type"), QStringLiteral("markdown")}});
        QJsonObject unknown;
        unknown.insert(QStringLiteral("type"), QStringLiteral("future-type"));
        split.insert(QStringLiteral("children"),
                     QJsonArray{leafA, unknown});

        auto layout = PaneLayout::fromJson(split);
        // Only one child valid — it collapses up into the root (no split).
        QVERIFY(!layout.root()->isSplit());
        QCOMPARE(layout.root()->viewCount(), 1);
    }

    void unknownKeysOnNodesPreserved()
    {
        QJsonObject split;
        split.insert(QStringLiteral("type"), QStringLiteral("split"));
        split.insert(QStringLiteral("direction"), QStringLiteral("horizontal"));
        split.insert(QStringLiteral("_futurePluginField"),
                     QStringLiteral("keep-me"));
        QJsonObject leafA;
        leafA.insert(QStringLiteral("type"), QStringLiteral("leaf"));
        leafA.insert(QStringLiteral("_leafExtra"), 42);
        leafA.insert(QStringLiteral("state"),
            QJsonObject{{QStringLiteral("type"), QStringLiteral("markdown")}});
        QJsonObject leafB;
        leafB.insert(QStringLiteral("type"), QStringLiteral("leaf"));
        leafB.insert(QStringLiteral("state"),
            QJsonObject{{QStringLiteral("type"), QStringLiteral("markdown")}});
        split.insert(QStringLiteral("children"),
                     QJsonArray{leafA, leafB});

        auto layout = PaneLayout::fromJson(split);
        const auto emitted = layout.toJson();
        QCOMPARE(emitted.value(QStringLiteral("_futurePluginField")).toString(),
                 QStringLiteral("keep-me"));
        // Leaf-level unknown preserved too.
        const auto kids = emitted.value(QStringLiteral("children")).toArray();
        QCOMPARE(kids[0].toObject().value(QStringLiteral("_leafExtra")).toInt(),
                 42);
    }

    void roundTripDeepTree()
    {
        // Build a layout programmatically, toJson, fromJson, toJson again →
        // stable across cycles.
        PaneLayout a;
        a.root()->addView(makeLeaf(QStringLiteral("a.md")));
        a.root()->splitWithNewLeaf(makeLeaf(QStringLiteral("b.md")),
                                   Qt::Vertical);
        a.root()->second()->splitWithNewLeaf(makeLeaf(QStringLiteral("c.md")),
                                             Qt::Horizontal);

        const QJsonObject j1 = a.toJson();
        auto b = PaneLayout::fromJson(j1);
        const QJsonObject j2 = b.toJson();

        QCOMPARE(QJsonDocument(j1).toJson(QJsonDocument::Compact),
                 QJsonDocument(j2).toJson(QJsonDocument::Compact));
    }

    void leafWithSingleViewEmitsBareLeaf()
    {
        PaneLayout a;
        a.root()->addView(makeLeaf(QStringLiteral("solo.md")));
        const auto j = a.toJson();
        // Single-view leaf-index → bare leaf node (not tabs wrapper).
        QCOMPARE(j.value(QStringLiteral("type")).toString(),
                 QStringLiteral("leaf"));
    }

    void leafWithMultipleViewsEmitsTabs()
    {
        PaneLayout a;
        a.root()->addView(makeLeaf(QStringLiteral("x.md")));
        a.root()->addView(makeLeaf(QStringLiteral("y.md")));
        const auto j = a.toJson();
        QCOMPARE(j.value(QStringLiteral("type")).toString(),
                 QStringLiteral("tabs"));
        const auto kids = j.value(QStringLiteral("children")).toArray();
        QCOMPARE(kids.size(), 2);
        QCOMPARE(kids[0].toObject().value(QStringLiteral("type")).toString(),
                 QStringLiteral("leaf"));
    }

    void activeLeafIdTracked()
    {
        PaneLayout a;
        auto leafA = makeLeaf(QStringLiteral("a.md"));
        const QString aId = leafA.id;
        a.root()->addView(std::move(leafA));
        a.setActiveLeafId(aId);
        QCOMPARE(a.activeLeafId(), aId);
    }

    // --- PaneLayout::newId yields distinct 16-char tokens ---

    void newIdsAre16CharsAndUnique()
    {
        QSet<QString> seen;
        for (int i = 0; i < 100; ++i) {
            const QString id = PaneLayout::newId();
            QCOMPARE(id.size(), 16);
            QVERIFY(!seen.contains(id));
            seen.insert(id);
        }
    }
};

QTEST_APPLESS_MAIN(TestPaneLayout)
#include "tst_panelayout.moc"
