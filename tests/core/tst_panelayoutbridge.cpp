// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QApplication>
#include <QSplitter>
#include <QWidget>

#include "corbomite/core/PaneLayout.h"
#include "corbomite/core/PaneLayoutBridge.h"

using namespace Corbomite;

namespace {

// A minimal stand-in for EditorViewSpace — just a QWidget that carries a
// list of "tabs" as PaneLeaf records. Used to drive the bridge from pure
// test code without dragging in the real editor stack.
class FakePane : public QWidget
{
public:
    QList<PaneLeaf> tabs;
};

PaneLeaf makeLeaf(const QString &file)
{
    PaneLeaf l;
    l.id = PaneLayout::newId();
    l.viewType = QStringLiteral("markdown");
    l.filePath = file;
    l.mode = QStringLiteral("source");
    return l;
}

} // namespace

class TestPaneLayoutBridge : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void singlePaneRoundTrip()
    {
        QSplitter root(Qt::Horizontal);
        auto *pane = new FakePane;
        pane->tabs = {makeLeaf(QStringLiteral("a.md"))};
        root.addWidget(pane);

        auto layout = PaneLayoutBridge::serializeFromSplitter(
            &root,
            [](QWidget *w) {
                return static_cast<FakePane *>(w)->tabs;
            });

        QVERIFY(!layout.root()->isSplit());
        QCOMPARE(layout.root()->viewCount(), 1);
        QCOMPARE(layout.root()->viewAt(0)->filePath, QStringLiteral("a.md"));
    }

    void horizontalSplitTwoPanes()
    {
        QSplitter root(Qt::Horizontal);
        auto *left = new FakePane;
        left->tabs = {makeLeaf(QStringLiteral("L.md"))};
        auto *right = new FakePane;
        right->tabs = {makeLeaf(QStringLiteral("R.md"))};
        root.addWidget(left);
        root.addWidget(right);

        auto layout = PaneLayoutBridge::serializeFromSplitter(
            &root,
            [](QWidget *w) {
                return static_cast<FakePane *>(w)->tabs;
            });

        QVERIFY(layout.root()->isSplit());
        QCOMPARE(layout.root()->orientation(), Qt::Horizontal);
        QCOMPARE(layout.root()->first()->viewAt(0)->filePath,
                 QStringLiteral("L.md"));
        QCOMPARE(layout.root()->second()->viewAt(0)->filePath,
                 QStringLiteral("R.md"));
    }

    void nestedSplits()
    {
        // Horizontal root: [leftPane, verticalSubSplitter(topPane, botPane)]
        QSplitter root(Qt::Horizontal);
        auto *left = new FakePane;
        left->tabs = {makeLeaf(QStringLiteral("left.md"))};
        root.addWidget(left);

        auto *sub = new QSplitter(Qt::Vertical);
        auto *top = new FakePane;
        top->tabs = {makeLeaf(QStringLiteral("top.md"))};
        auto *bot = new FakePane;
        bot->tabs = {makeLeaf(QStringLiteral("bot.md"))};
        sub->addWidget(top);
        sub->addWidget(bot);
        root.addWidget(sub);

        auto layout = PaneLayoutBridge::serializeFromSplitter(
            &root,
            [](QWidget *w) {
                return static_cast<FakePane *>(w)->tabs;
            });

        QVERIFY(layout.root()->isSplit());
        QCOMPARE(layout.root()->orientation(), Qt::Horizontal);
        QCOMPARE(layout.root()->first()->viewAt(0)->filePath,
                 QStringLiteral("left.md"));
        const auto *second = layout.root()->second();
        QVERIFY(second->isSplit());
        QCOMPARE(second->orientation(), Qt::Vertical);
        QCOMPARE(second->first()->viewAt(0)->filePath, QStringLiteral("top.md"));
        QCOMPARE(second->second()->viewAt(0)->filePath, QStringLiteral("bot.md"));
    }

    void tabsInPaneSerialize()
    {
        QSplitter root(Qt::Horizontal);
        auto *pane = new FakePane;
        pane->tabs = {
            makeLeaf(QStringLiteral("tab1.md")),
            makeLeaf(QStringLiteral("tab2.md")),
            makeLeaf(QStringLiteral("tab3.md")),
        };
        root.addWidget(pane);

        auto layout = PaneLayoutBridge::serializeFromSplitter(
            &root,
            [](QWidget *w) {
                return static_cast<FakePane *>(w)->tabs;
            });

        QVERIFY(!layout.root()->isSplit());
        QCOMPARE(layout.root()->viewCount(), 3);
        const auto j = layout.toJson();
        // Multi-view leaf index → tabs wrapper node.
        QCOMPARE(j.value(QStringLiteral("type")).toString(),
                 QStringLiteral("tabs"));
    }

    void activeSpaceSetsActiveLeafId()
    {
        QSplitter root(Qt::Horizontal);
        auto *paneA = new FakePane;
        paneA->tabs = {makeLeaf(QStringLiteral("a.md"))};
        auto *paneB = new FakePane;
        paneB->tabs = {makeLeaf(QStringLiteral("b.md"))};
        root.addWidget(paneA);
        root.addWidget(paneB);

        const QString expectedId = paneB->tabs.first().id;
        auto layout = PaneLayoutBridge::serializeFromSplitter(
            &root,
            [](QWidget *w) { return static_cast<FakePane *>(w)->tabs; },
            /*activeSpace=*/paneB);

        QCOMPARE(layout.activeLeafId(), expectedId);
    }

    // --- Deserialisation ---

    void deserialiseSinglePane()
    {
        PaneLayout layout;
        layout.root()->addView(makeLeaf(QStringLiteral("a.md")));

        QSplitter root(Qt::Horizontal);
        QList<QString> opened;

        auto created = PaneLayoutBridge::deserializeIntoSplitter(
            layout,
            &root,
            []() -> QWidget * { return new FakePane; },
            [&opened](QWidget *, const PaneLeaf &l) { opened.append(l.filePath); });

        QCOMPARE(created.size(), 1);
        QCOMPARE(opened, QList<QString>{QStringLiteral("a.md")});
        QCOMPARE(root.count(), 1);
    }

    void deserialiseSplitRebuildsWidgets()
    {
        PaneLayout layout;
        layout.root()->addView(makeLeaf(QStringLiteral("A.md")));
        layout.root()->splitWithNewLeaf(makeLeaf(QStringLiteral("B.md")),
                                        Qt::Horizontal);

        QSplitter root(Qt::Horizontal);
        QList<QString> opened;
        auto created = PaneLayoutBridge::deserializeIntoSplitter(
            layout,
            &root,
            []() -> QWidget * { return new FakePane; },
            [&opened](QWidget *, const PaneLeaf &l) { opened.append(l.filePath); });

        QCOMPARE(created.size(), 2);
        QCOMPARE(opened.size(), 2);
        QVERIFY(opened.contains(QStringLiteral("A.md")));
        QVERIFY(opened.contains(QStringLiteral("B.md")));
    }

    void roundTripWidgetsThenJson()
    {
        // Build widgets, serialise, toJson, fromJson, deserialise, serialise
        // again → stable.
        QSplitter root(Qt::Horizontal);
        auto *a = new FakePane;
        a->tabs = {makeLeaf(QStringLiteral("a.md"))};
        auto *b = new FakePane;
        b->tabs = {makeLeaf(QStringLiteral("b.md"))};
        root.addWidget(a);
        root.addWidget(b);

        auto layoutFromWidgets = PaneLayoutBridge::serializeFromSplitter(
            &root,
            [](QWidget *w) { return static_cast<FakePane *>(w)->tabs; });

        const auto json = layoutFromWidgets.toJson();
        auto reloaded = PaneLayout::fromJson(json);

        QSplitter rebuiltRoot(Qt::Horizontal);
        QList<QString> opened;
        PaneLayoutBridge::deserializeIntoSplitter(
            reloaded,
            &rebuiltRoot,
            []() -> QWidget * { return new FakePane; },
            [&opened](QWidget *, const PaneLeaf &l) { opened.append(l.filePath); });

        QCOMPARE(opened.size(), 2);
        QVERIFY(opened.contains(QStringLiteral("a.md")));
        QVERIFY(opened.contains(QStringLiteral("b.md")));
    }
};

QTEST_MAIN(TestPaneLayoutBridge)
#include "tst_panelayoutbridge.moc"
