// tests/core/tst_workspace_integration.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Spec-driven integration tests for workspace tree <-> widget hierarchy claims.
// Derived from: docs/superpowers/specs/2026-04-15-cluster-g-part2-workspace-design.md
// §2 "Workspace tree" and §3 "Widget hierarchy"

#include <QtTest/QtTest>
#include <QApplication>
#include <QSignalSpy>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabBar>
#include <QWidget>

#include "corbomite/core/Workspace.h"
#include "corbomite/core/WorkspaceItem.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/WorkspaceParent.h"
#include "corbomite/core/WorkspaceSplit.h"
#include "corbomite/core/WorkspaceTabs.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/View.h"

using namespace Corbomite;

// ---------------------------------------------------------------------------
// Minimal stub View so ViewRegistry can create views without real backends
// ---------------------------------------------------------------------------
class StubView : public View
{
    Q_OBJECT
public:
    explicit StubView(WorkspaceLeaf *leaf, QWidget *parent = nullptr)
        : View(leaf, parent)
    {}

    QString getViewType() const override { return QStringLiteral("stub"); }
    QString getDisplayText() const override { return QStringLiteral("Stub"); }
};

// ---------------------------------------------------------------------------
// Test fixture — creates a fresh Workspace + ViewRegistry per test
// ---------------------------------------------------------------------------
class WorkspaceIntegrationTest : public QObject
{
    Q_OBJECT

private:
    ViewRegistry *makeRegistry(QObject *parent = nullptr)
    {
        auto *reg = new ViewRegistry(parent);
        reg->registerView(QStringLiteral("stub"), [](WorkspaceLeaf *leaf) -> View * {
            return new StubView(leaf);
        });
        return reg;
    }

private Q_SLOTS:
    // ------------------------------------------------------------------
    // 1. WorkspaceItem: unique 16-char hex id generated at construction
    // ------------------------------------------------------------------
    void test_workspaceItem_id_isUnique()
    {
        // Each item gets a distinct id
        WorkspaceSplit split1;
        WorkspaceSplit split2;

        QVERIFY(!split1.id().isEmpty());
        QVERIFY(!split2.id().isEmpty());
        QVERIFY(split1.id() != split2.id());
    }

    void test_workspaceItem_id_is16CharHex()
    {
        WorkspaceSplit split;
        const QString id = split.id();
        QCOMPARE(id.length(), 16);
        // Every character must be a hex digit
        static const QString hexChars = QStringLiteral("0123456789abcdefABCDEF");
        for (QChar c : id) {
            QVERIFY2(hexChars.contains(c),
                     qPrintable(QStringLiteral("Non-hex char '%1' in id '%2'").arg(c).arg(id)));
        }
    }

    // ------------------------------------------------------------------
    // 2. WorkspaceSplit::widget() returns a non-null QWidget that contains
    //    a QSplitter (spec §3.1: "m_splitter: QSplitter* (internal widget)")
    // ------------------------------------------------------------------
    void test_workspaceSplit_widget_isNonNull()
    {
        WorkspaceSplit split;
        QVERIFY(split.widget() != nullptr);
    }

    void test_workspaceSplit_widget_containsQSplitter()
    {
        WorkspaceSplit split;
        QWidget *w = split.widget();
        QVERIFY(w != nullptr);

        // The widget itself may be the QSplitter, or the QSplitter is a child
        QSplitter *splitter = qobject_cast<QSplitter *>(w);
        if (!splitter) {
            splitter = w->findChild<QSplitter *>();
        }
        QVERIFY2(splitter != nullptr,
                 "WorkspaceSplit::widget() must contain or be a QSplitter");
    }

    // ------------------------------------------------------------------
    // 3. WorkspaceTabs::widget() returns a non-null QWidget that contains
    //    a QTabBar and a QStackedWidget
    // ------------------------------------------------------------------
    void test_workspaceTabs_widget_isNonNull()
    {
        WorkspaceTabs tabs;
        QVERIFY(tabs.widget() != nullptr);
    }

    void test_workspaceTabs_widget_containsTabBar()
    {
        WorkspaceTabs tabs;
        QWidget *w = tabs.widget();
        QVERIFY(w != nullptr);

        // tabBar() is part of the public API
        QTabBar *bar = tabs.tabBar();
        QVERIFY2(bar != nullptr, "WorkspaceTabs::tabBar() must return a non-null QTabBar");

        // The tab bar must be a descendant of (or be) the tabs widget
        // Walk up the parent chain
        bool found = false;
        QWidget *cursor = bar;
        while (cursor) {
            if (cursor == w) { found = true; break; }
            cursor = cursor->parentWidget();
        }
        QVERIFY2(found, "QTabBar must be a descendant of WorkspaceTabs::widget()");
    }

    void test_workspaceTabs_widget_containsStackedWidget()
    {
        WorkspaceTabs tabs;
        QWidget *w = tabs.widget();
        QVERIFY(w != nullptr);

        QStackedWidget *stack = w->findChild<QStackedWidget *>();
        QVERIFY2(stack != nullptr,
                 "WorkspaceTabs::widget() must contain a QStackedWidget");
    }

    // ------------------------------------------------------------------
    // 4. After WorkspaceSplit::addChild(), child widget is reparented into
    //    the split's widget subtree (spec: "children's widgets are inserted
    //    into [the QSplitter]")
    // ------------------------------------------------------------------
    void test_workspaceSplit_addChild_widgetParenting()
    {
        WorkspaceSplit split;
        WorkspaceTabs *tabs = new WorkspaceTabs(&split);

        split.addChild(tabs);

        QWidget *splitWidget = split.widget();
        QWidget *tabsWidget = tabs->widget();

        QVERIFY(splitWidget != nullptr);
        QVERIFY(tabsWidget != nullptr);

        // tabs widget must be a descendant of the split widget
        bool found = false;
        QWidget *cursor = tabsWidget;
        while (cursor) {
            if (cursor == splitWidget) { found = true; break; }
            cursor = cursor->parentWidget();
        }
        QVERIFY2(found,
                 "After addChild to WorkspaceSplit, child widget must be inside split widget");
    }

    // ------------------------------------------------------------------
    // 5. After WorkspaceSplit::removeChild(), child widget is unparented
    //    (spec: "after removeChild, the child's widget is unparented")
    // ------------------------------------------------------------------
    void test_workspaceSplit_removeChild_widgetUnparented()
    {
        WorkspaceSplit split;
        WorkspaceTabs *tabs = new WorkspaceTabs; // no parent yet

        split.addChild(tabs);

        // Confirm widget is inside split first
        QWidget *splitWidget = split.widget();
        QWidget *tabsWidget = tabs->widget();

        {
            bool foundBefore = false;
            QWidget *cursor = tabsWidget;
            while (cursor) {
                if (cursor == splitWidget) { foundBefore = true; break; }
                cursor = cursor->parentWidget();
            }
            QVERIFY2(foundBefore, "Pre-condition: child widget should be inside split widget");
        }

        split.removeChild(tabs, /*deleteChild=*/false);

        // After removal, tabsWidget must NOT be a descendant of splitWidget
        bool foundAfter = false;
        QWidget *cursor = tabsWidget;
        while (cursor) {
            if (cursor == splitWidget) { foundAfter = true; break; }
            cursor = cursor->parentWidget();
        }
        QVERIFY2(!foundAfter,
                 "After removeChild from WorkspaceSplit, child widget must be unparented");

        delete tabs; // clean up manually since we used deleteChild=false
    }

    // ------------------------------------------------------------------
    // 6. WorkspaceSplit::childCount reflects addChild / removeChild
    // ------------------------------------------------------------------
    void test_workspaceSplit_childCount()
    {
        WorkspaceSplit split;
        QCOMPARE(split.childCount(), 0);

        WorkspaceTabs *t1 = new WorkspaceTabs(&split);
        split.addChild(t1);
        QCOMPARE(split.childCount(), 1);

        WorkspaceTabs *t2 = new WorkspaceTabs(&split);
        split.addChild(t2);
        QCOMPARE(split.childCount(), 2);

        split.removeChild(t1, /*deleteChild=*/true);
        QCOMPARE(split.childCount(), 1);

        split.removeChild(t2, /*deleteChild=*/true);
        QCOMPARE(split.childCount(), 0);
    }

    // ------------------------------------------------------------------
    // 7. After WorkspaceTabs::addChild(leaf), leaf widget is in the stack
    //    (spec: "leaf widgets go into the stack")
    // ------------------------------------------------------------------
    void test_workspaceTabs_addChild_leafWidgetInStack()
    {
        ViewRegistry reg;
        WorkspaceTabs tabs;
        WorkspaceLeaf *leaf = new WorkspaceLeaf(&reg, &tabs);

        tabs.addChild(leaf);

        QWidget *tabsWidget = tabs.widget();
        QWidget *leafWidget = leaf->widget();

        QVERIFY(tabsWidget != nullptr);
        QVERIFY(leafWidget != nullptr);

        // leafWidget must be a descendant of tabsWidget
        bool found = false;
        QWidget *cursor = leafWidget;
        while (cursor) {
            if (cursor == tabsWidget) { found = true; break; }
            cursor = cursor->parentWidget();
        }
        QVERIFY2(found,
                 "After addChild to WorkspaceTabs, leaf widget must be inside tabs widget");
    }

    // ------------------------------------------------------------------
    // 8. After WorkspaceTabs::removeChild(leaf), leaf widget is unparented
    //    (spec: "after removeChild, the child's widget is unparented")
    // ------------------------------------------------------------------
    void test_workspaceTabs_removeChild_leafWidgetUnparented()
    {
        ViewRegistry reg;
        WorkspaceTabs tabs;
        WorkspaceLeaf *leaf = new WorkspaceLeaf(&reg); // no parent

        tabs.addChild(leaf);

        QWidget *tabsWidget = tabs.widget();
        QWidget *leafWidget = leaf->widget();

        {
            bool foundBefore = false;
            QWidget *cursor = leafWidget;
            while (cursor) {
                if (cursor == tabsWidget) { foundBefore = true; break; }
                cursor = cursor->parentWidget();
            }
            QVERIFY2(foundBefore, "Pre-condition: leaf widget should be inside tabs widget");
        }

        tabs.removeChild(leaf, /*deleteChild=*/false);

        bool foundAfter = false;
        QWidget *cursor = leafWidget;
        while (cursor) {
            if (cursor == tabsWidget) { foundAfter = true; break; }
            cursor = cursor->parentWidget();
        }
        QVERIFY2(!foundAfter,
                 "After removeChild from WorkspaceTabs, leaf widget must be unparented");

        delete leaf;
    }

    // ------------------------------------------------------------------
    // 9. WorkspaceTabs::childCount reflects addChild / removeChild for leaves
    // ------------------------------------------------------------------
    void test_workspaceTabs_childCount()
    {
        ViewRegistry reg;
        WorkspaceTabs tabs;
        QCOMPARE(tabs.childCount(), 0);

        WorkspaceLeaf *l1 = new WorkspaceLeaf(&reg, &tabs);
        tabs.addChild(l1);
        QCOMPARE(tabs.childCount(), 1);

        WorkspaceLeaf *l2 = new WorkspaceLeaf(&reg, &tabs);
        tabs.addChild(l2);
        QCOMPARE(tabs.childCount(), 2);

        tabs.removeChild(l1, /*deleteChild=*/true);
        QCOMPARE(tabs.childCount(), 1);

        tabs.removeChild(l2, /*deleteChild=*/true);
        QCOMPARE(tabs.childCount(), 0);
    }

    // ------------------------------------------------------------------
    // 10. parentItem() tracks workspace object tree parent
    // ------------------------------------------------------------------
    void test_workspaceItem_parentItem_setOnAddChild()
    {
        WorkspaceSplit split;
        WorkspaceTabs *tabs = new WorkspaceTabs(&split);

        QVERIFY(tabs->parentItem() == nullptr); // not yet added

        split.addChild(tabs);

        QVERIFY2(tabs->parentItem() == &split,
                 "After addChild, child's parentItem() must be the parent");
    }

    void test_workspaceItem_parentItem_clearedOnRemoveChild()
    {
        WorkspaceSplit split;
        WorkspaceTabs *tabs = new WorkspaceTabs;

        split.addChild(tabs);
        QVERIFY(tabs->parentItem() == &split);

        split.removeChild(tabs, /*deleteChild=*/false);
        QVERIFY2(tabs->parentItem() == nullptr,
                 "After removeChild, child's parentItem() must be null");

        delete tabs;
    }

    // ------------------------------------------------------------------
    // 11. Default Workspace layout: one WorkspaceTabs with zero leaves
    //     (spec §3.2: "Missing file → default layout (single WorkspaceTabs
    //     with one empty leaf)" — but headers show createLeafInTabs is
    //     separate, so default = single tabs, no leaves until explicitly added)
    // ------------------------------------------------------------------
    void test_workspace_defaultLayout_hasOneTabs()
    {
        ViewRegistry reg;
        Workspace ws(&reg);

        WorkspaceSplit *mainRoot = ws.mainRoot();
        QVERIFY2(mainRoot != nullptr, "Default Workspace must have a non-null mainRoot()");

        // Find the WorkspaceTabs inside the main root
        WorkspaceTabs *tabs = nullptr;
        // mainRoot should contain at least one child that is WorkspaceTabs
        for (int i = 0; i < mainRoot->childCount(); ++i) {
            tabs = qobject_cast<WorkspaceTabs *>(mainRoot->childAt(i));
            if (tabs) break;
        }
        QVERIFY2(tabs != nullptr,
                 "Default Workspace layout must contain at least one WorkspaceTabs");
    }

    void test_workspace_defaultLayout_tabsHasZeroLeaves()
    {
        ViewRegistry reg;
        Workspace ws(&reg);

        WorkspaceSplit *mainRoot = ws.mainRoot();
        QVERIFY(mainRoot != nullptr);

        WorkspaceTabs *tabs = nullptr;
        for (int i = 0; i < mainRoot->childCount(); ++i) {
            tabs = qobject_cast<WorkspaceTabs *>(mainRoot->childAt(i));
            if (tabs) break;
        }
        QVERIFY(tabs != nullptr);

        QCOMPARE(tabs->childCount(), 0);
    }

    // ------------------------------------------------------------------
    // 12. Workspace::createLeafInTabs adds a leaf to the tree
    //     (spec: "createLeafInTabs adds a leaf to the tree")
    // ------------------------------------------------------------------
    void test_workspace_createLeafInTabs_addsLeaf()
    {
        ViewRegistry reg;
        Workspace ws(&reg);

        WorkspaceSplit *mainRoot = ws.mainRoot();
        QVERIFY(mainRoot != nullptr);

        WorkspaceTabs *tabs = nullptr;
        for (int i = 0; i < mainRoot->childCount(); ++i) {
            tabs = qobject_cast<WorkspaceTabs *>(mainRoot->childAt(i));
            if (tabs) break;
        }
        QVERIFY(tabs != nullptr);

        int before = tabs->childCount();
        WorkspaceLeaf *leaf = ws.createLeafInTabs(tabs);

        QVERIFY2(leaf != nullptr, "createLeafInTabs must return a non-null leaf");
        QCOMPARE(tabs->childCount(), before + 1);

        // The leaf must appear in allLeaves()
        QVector<WorkspaceLeaf *> all = ws.allLeaves();
        QVERIFY2(all.contains(leaf),
                 "Leaf returned by createLeafInTabs must appear in Workspace::allLeaves()");
    }

    // ------------------------------------------------------------------
    // 13. Workspace::closeLeaf removes the leaf from the tree
    //     (spec: "closeLeaf removes it")
    // ------------------------------------------------------------------
    void test_workspace_closeLeaf_removesLeaf()
    {
        ViewRegistry reg;
        Workspace ws(&reg);

        WorkspaceSplit *mainRoot = ws.mainRoot();
        WorkspaceTabs *tabs = nullptr;
        for (int i = 0; i < mainRoot->childCount(); ++i) {
            tabs = qobject_cast<WorkspaceTabs *>(mainRoot->childAt(i));
            if (tabs) break;
        }
        QVERIFY(tabs != nullptr);

        WorkspaceLeaf *leaf = ws.createLeafInTabs(tabs);
        QVERIFY(leaf != nullptr);

        int before = tabs->childCount();
        ws.closeLeaf(leaf);

        QCOMPARE(tabs->childCount(), before - 1);

        // leaf must no longer appear in allLeaves()
        QVector<WorkspaceLeaf *> all = ws.allLeaves();
        QVERIFY2(!all.contains(leaf),
                 "Closed leaf must not appear in Workspace::allLeaves()");
    }

    // ------------------------------------------------------------------
    // 14. closeLeaf emits leafClosed signal
    // ------------------------------------------------------------------
    void test_workspace_closeLeaf_emitsSignal()
    {
        ViewRegistry reg;
        Workspace ws(&reg);

        WorkspaceSplit *mainRoot = ws.mainRoot();
        WorkspaceTabs *tabs = nullptr;
        for (int i = 0; i < mainRoot->childCount(); ++i) {
            tabs = qobject_cast<WorkspaceTabs *>(mainRoot->childAt(i));
            if (tabs) break;
        }
        QVERIFY(tabs != nullptr);

        WorkspaceLeaf *leaf = ws.createLeafInTabs(tabs);
        QVERIFY(leaf != nullptr);

        QSignalSpy spy(&ws, &Workspace::leafClosed);
        ws.closeLeaf(leaf);

        QCOMPARE(spy.count(), 1);
    }

    // ------------------------------------------------------------------
    // 15. Workspace::splitLeaf creates a WorkspaceSplit containing the
    //     original tabs (spec: "splitting a leaf wraps its parent tabs in
    //     a new split")
    // ------------------------------------------------------------------
    void test_workspace_splitLeaf_createsWorkspaceSplit()
    {
        ViewRegistry reg;
        Workspace ws(&reg);

        WorkspaceSplit *mainRoot = ws.mainRoot();
        WorkspaceTabs *tabs = nullptr;
        for (int i = 0; i < mainRoot->childCount(); ++i) {
            tabs = qobject_cast<WorkspaceTabs *>(mainRoot->childAt(i));
            if (tabs) break;
        }
        QVERIFY(tabs != nullptr);

        WorkspaceLeaf *leaf = ws.createLeafInTabs(tabs);
        QVERIFY(leaf != nullptr);

        WorkspaceSplit *newSplit = ws.splitLeaf(leaf, Qt::Horizontal);
        QVERIFY2(newSplit != nullptr, "splitLeaf must return a non-null WorkspaceSplit");
    }

    void test_workspace_splitLeaf_splitContainsTabs()
    {
        ViewRegistry reg;
        Workspace ws(&reg);

        WorkspaceSplit *mainRoot = ws.mainRoot();
        WorkspaceTabs *tabs = nullptr;
        for (int i = 0; i < mainRoot->childCount(); ++i) {
            tabs = qobject_cast<WorkspaceTabs *>(mainRoot->childAt(i));
            if (tabs) break;
        }
        QVERIFY(tabs != nullptr);

        WorkspaceLeaf *leaf = ws.createLeafInTabs(tabs);
        QVERIFY(leaf != nullptr);

        WorkspaceSplit *newSplit = ws.splitLeaf(leaf, Qt::Horizontal);
        QVERIFY(newSplit != nullptr);

        // The new split must have at least 2 children (original tabs + new tabs)
        QVERIFY2(newSplit->childCount() >= 2,
                 "WorkspaceSplit from splitLeaf must have at least 2 children");

        // At least one child must be a WorkspaceTabs
        bool hasTabs = false;
        for (int i = 0; i < newSplit->childCount(); ++i) {
            if (qobject_cast<WorkspaceTabs *>(newSplit->childAt(i))) {
                hasTabs = true;
                break;
            }
        }
        QVERIFY2(hasTabs, "WorkspaceSplit from splitLeaf must contain at least one WorkspaceTabs");
    }

    // ------------------------------------------------------------------
    // 16. Widget tree mirrors workspace object tree: the widget hierarchy
    //     matches the WorkspaceItem tree structure.
    //     Split -> Tabs -> Leaf widget nesting must be reflected in QWidget
    //     parent chain.
    // ------------------------------------------------------------------
    void test_widgetHierarchyMirrorsObjectTree()
    {
        ViewRegistry reg;
        Workspace ws(&reg);

        WorkspaceSplit *mainRoot = ws.mainRoot();
        WorkspaceTabs *tabs = nullptr;
        for (int i = 0; i < mainRoot->childCount(); ++i) {
            tabs = qobject_cast<WorkspaceTabs *>(mainRoot->childAt(i));
            if (tabs) break;
        }
        QVERIFY(tabs != nullptr);

        WorkspaceLeaf *leaf = ws.createLeafInTabs(tabs);
        QVERIFY(leaf != nullptr);

        QWidget *rootWidget = mainRoot->widget();
        QWidget *tabsWidget = tabs->widget();
        QWidget *leafWidget = leaf->widget();

        QVERIFY(rootWidget != nullptr);
        QVERIFY(tabsWidget != nullptr);
        QVERIFY(leafWidget != nullptr);

        // tabsWidget must be inside rootWidget
        {
            bool found = false;
            QWidget *cursor = tabsWidget;
            while (cursor) {
                if (cursor == rootWidget) { found = true; break; }
                cursor = cursor->parentWidget();
            }
            QVERIFY2(found, "Tabs widget must be inside root split widget");
        }

        // leafWidget must be inside tabsWidget
        {
            bool found = false;
            QWidget *cursor = leafWidget;
            while (cursor) {
                if (cursor == tabsWidget) { found = true; break; }
                cursor = cursor->parentWidget();
            }
            QVERIFY2(found, "Leaf widget must be inside tabs widget");
        }
    }

    // ------------------------------------------------------------------
    // 17. WorkspaceTabs::leafAt(index) returns the correct leaf
    // ------------------------------------------------------------------
    void test_workspaceTabs_leafAt_returnsCorrectLeaf()
    {
        ViewRegistry reg;
        WorkspaceTabs tabs;

        WorkspaceLeaf *l1 = new WorkspaceLeaf(&reg, &tabs);
        WorkspaceLeaf *l2 = new WorkspaceLeaf(&reg, &tabs);

        tabs.addChild(l1);
        tabs.addChild(l2);

        QCOMPARE(tabs.leafAt(0), l1);
        QCOMPARE(tabs.leafAt(1), l2);
    }

    // ------------------------------------------------------------------
    // 18. WorkspaceTabs children are exclusively WorkspaceLeaf instances
    //     (spec invariant 2: "WorkspaceTabs children are exclusively
    //     WorkspaceLeaf instances")
    //     Verify: all children returned via childAt() are WorkspaceLeaf
    // ------------------------------------------------------------------
    void test_workspaceTabs_childrenAreLeaves()
    {
        ViewRegistry reg;
        WorkspaceTabs tabs;

        WorkspaceLeaf *l1 = new WorkspaceLeaf(&reg, &tabs);
        WorkspaceLeaf *l2 = new WorkspaceLeaf(&reg, &tabs);
        tabs.addChild(l1);
        tabs.addChild(l2);

        for (int i = 0; i < tabs.childCount(); ++i) {
            WorkspaceItem *child = tabs.childAt(i);
            QVERIFY2(qobject_cast<WorkspaceLeaf *>(child) != nullptr,
                     "All WorkspaceTabs children must be WorkspaceLeaf instances");
        }
    }

    // ------------------------------------------------------------------
    // 19. WorkspaceParent::childAdded signal fires on addChild
    // ------------------------------------------------------------------
    void test_workspaceParent_childAddedSignal()
    {
        ViewRegistry reg;
        WorkspaceTabs tabs;

        QSignalSpy spy(&tabs, &WorkspaceParent::childAdded);

        WorkspaceLeaf *leaf = new WorkspaceLeaf(&reg, &tabs);
        tabs.addChild(leaf);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<WorkspaceItem *>(), leaf);
    }

    // ------------------------------------------------------------------
    // 20. WorkspaceParent::childRemoved signal fires on removeChild
    // ------------------------------------------------------------------
    void test_workspaceParent_childRemovedSignal()
    {
        ViewRegistry reg;
        WorkspaceTabs tabs;

        WorkspaceLeaf *leaf = new WorkspaceLeaf(&reg, &tabs);
        tabs.addChild(leaf);

        QSignalSpy spy(&tabs, &WorkspaceParent::childRemoved);
        tabs.removeChild(leaf, /*deleteChild=*/true);

        QCOMPARE(spy.count(), 1);
    }

    // ------------------------------------------------------------------
    // 21. WorkspaceTabs::tabBar() is non-null and tracks leaf count
    //     (tab count matches child leaf count)
    // ------------------------------------------------------------------
    void test_workspaceTabs_tabBar_countMatchesChildren()
    {
        ViewRegistry reg;
        WorkspaceTabs tabs;

        QTabBar *bar = tabs.tabBar();
        QVERIFY(bar != nullptr);
        QCOMPARE(bar->count(), 0);

        WorkspaceLeaf *l1 = new WorkspaceLeaf(&reg, &tabs);
        tabs.addChild(l1);
        QCOMPARE(bar->count(), 1);

        WorkspaceLeaf *l2 = new WorkspaceLeaf(&reg, &tabs);
        tabs.addChild(l2);
        QCOMPARE(bar->count(), 2);

        tabs.removeChild(l1, /*deleteChild=*/true);
        QCOMPARE(bar->count(), 1);
    }

    // ------------------------------------------------------------------
    // 22. WorkspaceSplit::direction() defaults and can be changed
    // ------------------------------------------------------------------
    void test_workspaceSplit_direction()
    {
        WorkspaceSplit split;
        // Default direction per spec is Horizontal
        QCOMPARE(split.direction(), Qt::Horizontal);

        split.setDirection(Qt::Vertical);
        QCOMPARE(split.direction(), Qt::Vertical);
    }

    // ------------------------------------------------------------------
    // 23. WorkspaceTabs currentLeaf() matches currentTab index
    // ------------------------------------------------------------------
    void test_workspaceTabs_currentLeaf_tracksCurrentTab()
    {
        ViewRegistry reg;
        WorkspaceTabs tabs;

        WorkspaceLeaf *l1 = new WorkspaceLeaf(&reg, &tabs);
        WorkspaceLeaf *l2 = new WorkspaceLeaf(&reg, &tabs);
        tabs.addChild(l1);
        tabs.addChild(l2);

        tabs.setCurrentTab(0);
        QCOMPARE(tabs.currentLeaf(), l1);

        tabs.setCurrentTab(1);
        QCOMPARE(tabs.currentLeaf(), l2);
    }

    // ------------------------------------------------------------------
    // 24. Workspace::allLeaves() collects all leaves in the tree
    // ------------------------------------------------------------------
    void test_workspace_allLeaves_collectsAll()
    {
        ViewRegistry reg;
        Workspace ws(&reg);

        WorkspaceSplit *mainRoot = ws.mainRoot();
        WorkspaceTabs *tabs = nullptr;
        for (int i = 0; i < mainRoot->childCount(); ++i) {
            tabs = qobject_cast<WorkspaceTabs *>(mainRoot->childAt(i));
            if (tabs) break;
        }
        QVERIFY(tabs != nullptr);

        WorkspaceLeaf *l1 = ws.createLeafInTabs(tabs);
        WorkspaceLeaf *l2 = ws.createLeafInTabs(tabs);
        WorkspaceLeaf *l3 = ws.createLeafInTabs(tabs);

        QVector<WorkspaceLeaf *> all = ws.allLeaves();
        QVERIFY(all.contains(l1));
        QVERIFY(all.contains(l2));
        QVERIFY(all.contains(l3));
        QCOMPARE(all.size(), 3);
    }

    // ------------------------------------------------------------------
    // 25. Workspace::findLeafById finds a leaf by its id
    // ------------------------------------------------------------------
    void test_workspace_findLeafById()
    {
        ViewRegistry reg;
        Workspace ws(&reg);

        WorkspaceSplit *mainRoot = ws.mainRoot();
        WorkspaceTabs *tabs = nullptr;
        for (int i = 0; i < mainRoot->childCount(); ++i) {
            tabs = qobject_cast<WorkspaceTabs *>(mainRoot->childAt(i));
            if (tabs) break;
        }
        QVERIFY(tabs != nullptr);

        WorkspaceLeaf *leaf = ws.createLeafInTabs(tabs);
        QVERIFY(leaf != nullptr);

        QString leafId = leaf->id();
        WorkspaceLeaf *found = ws.findLeafById(leafId);
        QCOMPARE(found, leaf);
    }

    void test_workspace_findLeafById_unknownId_returnsNull()
    {
        ViewRegistry reg;
        Workspace ws(&reg);

        WorkspaceLeaf *found = ws.findLeafById(QStringLiteral("0000000000000000"));
        QVERIFY(found == nullptr);
    }

    // ------------------------------------------------------------------
    // 26. WorkspaceLeaf::widget() is non-null
    // ------------------------------------------------------------------
    void test_workspaceLeaf_widget_isNonNull()
    {
        ViewRegistry reg;
        WorkspaceLeaf leaf(&reg);
        QVERIFY(leaf.widget() != nullptr);
    }

    // ------------------------------------------------------------------
    // 27. WorkspaceParent::moveChild reorders children correctly
    // ------------------------------------------------------------------
    void test_workspaceParent_moveChild()
    {
        ViewRegistry reg;
        WorkspaceTabs tabs;

        WorkspaceLeaf *l1 = new WorkspaceLeaf(&reg, &tabs);
        WorkspaceLeaf *l2 = new WorkspaceLeaf(&reg, &tabs);
        WorkspaceLeaf *l3 = new WorkspaceLeaf(&reg, &tabs);
        tabs.addChild(l1);
        tabs.addChild(l2);
        tabs.addChild(l3);

        // Move l1 from index 0 to index 2 (last)
        tabs.moveChild(0, 2);

        // After move: [l2, l3, l1]
        QCOMPARE(tabs.childAt(0), l2);
        QCOMPARE(tabs.childAt(1), l3);
        QCOMPARE(tabs.childAt(2), l1);
    }

    // ------------------------------------------------------------------
    // 28. WorkspaceSplit children must not be WorkspaceLeaf (invariant 3)
    //     This is an assertion of the tree invariant — WorkspaceSplit
    //     contains WorkspaceSplit or WorkspaceTabs, not leaves.
    //     We verify the object tree contract by checking indexOf/childAt.
    // ------------------------------------------------------------------
    void test_workspaceSplit_containsNonLeafChildren()
    {
        WorkspaceSplit split;
        WorkspaceTabs *tabs = new WorkspaceTabs(&split);
        split.addChild(tabs);

        // Only WorkspaceTabs should be a child, not a leaf
        for (int i = 0; i < split.childCount(); ++i) {
            WorkspaceItem *child = split.childAt(i);
            QVERIFY2(qobject_cast<WorkspaceLeaf *>(child) == nullptr,
                     "WorkspaceSplit must not have WorkspaceLeaf as direct children");
        }
    }
};

QTEST_MAIN(WorkspaceIntegrationTest)
#include "tst_workspace_integration.moc"
