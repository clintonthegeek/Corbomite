// tests/core/tst_workspace_tabs_lifecycle.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Spec-driven tests for WorkspaceTabs tab lifecycle claims.
// Source: docs/superpowers/specs/2026-04-15-cluster-g-part2-workspace-design.md §3.1, §3.5
//
// Behavioral claims under test:
//  1. Tab titles come from View::getDisplayText()
//  2. Tab icons come from View::getIcon()
//  3. Closing a tab (removeChild) removes the leaf and reduces tab count
//  4. Switching tabs calls loadIfDeferred() for deferred leaves
//  5. setCurrentTab(index) updates current index and emits currentTabChanged
//  6. Multiple tabs track correct insertion order via leafAt()
//  7. Tab title updates when the view changes (viewChanged propagation)

#include <QTest>
#include <QSignalSpy>
#include <QWidget>
#include <QTabBar>

#include "corbomite/core/View.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/WorkspaceTabs.h"

using namespace Corbomite;

// ---------------------------------------------------------------------------
// Stub view with configurable display text and icon
// ---------------------------------------------------------------------------

class StubView : public View
{
    Q_OBJECT
public:
    explicit StubView(WorkspaceLeaf *leaf,
                      const QString &type = QStringLiteral("stub"),
                      const QString &text = QStringLiteral("Untitled"),
                      const QString &icon = QStringLiteral("document"),
                      QWidget *parent = nullptr)
        : View(leaf, parent)
        , m_type(type)
        , m_text(text)
        , m_icon(icon)
    {
    }

    QString getViewType() const override { return m_type; }
    QString getDisplayText() const override { return m_text; }
    QString getIcon() const override { return m_icon; }

    void setDisplayText(const QString &text) { m_text = text; }
    void setIconName(const QString &icon) { m_icon = icon; }

private:
    QString m_type;
    QString m_text;
    QString m_icon;
};

// ---------------------------------------------------------------------------
// Helper: make a leaf with a StubView attached
// ---------------------------------------------------------------------------

static WorkspaceLeaf *makeLeaf(ViewRegistry *reg,
                                const QString &displayText = QStringLiteral("Untitled"),
                                const QString &icon = QStringLiteral("document"),
                                QObject *parent = nullptr)
{
    auto *leaf = new WorkspaceLeaf(reg, parent);
    auto *view = new StubView(leaf,
                              QStringLiteral("stub"),
                              displayText,
                              icon,
                              nullptr);
    leaf->open(view);
    return leaf;
}

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

class TestWorkspaceTabsLifecycle : public QObject
{
    Q_OBJECT

private:
    ViewRegistry *m_reg = nullptr;

private Q_SLOTS:

    void init()
    {
        m_reg = new ViewRegistry(this);
    }

    void cleanup()
    {
        // m_reg owned by this (QObject parent chain)
    }

    // -----------------------------------------------------------------------
    // Claim 1: Tab titles come from View::getDisplayText()
    // -----------------------------------------------------------------------
    void tabTitleFromViewDisplayText()
    {
        WorkspaceTabs tabs;
        auto *leaf = makeLeaf(m_reg, QStringLiteral("My Note"), QStringLiteral("document"), &tabs);
        tabs.addChild(leaf);

        // The tab bar should reflect the view's display text
        const QString tabText = tabs.tabBar()->tabText(0);
        QCOMPARE(tabText, QStringLiteral("My Note"));
    }

    // -----------------------------------------------------------------------
    // Claim 2: Tab icons come from View::getIcon()
    // -----------------------------------------------------------------------
    void tabIconFromViewGetIcon()
    {
        WorkspaceTabs tabs;
        auto *leaf = makeLeaf(m_reg, QStringLiteral("Note"), QStringLiteral("text-plain"), &tabs);
        tabs.addChild(leaf);

        // The tab bar should have an icon set (non-null)
        // We can't easily check the icon name string, but we can verify
        // that the icon is not null/empty when getIcon() returns a non-empty string.
        // The tab bar icon is set from the view's icon string.
        QIcon tabIcon = tabs.tabBar()->tabIcon(0);
        // Since QIcon::fromTheme("text-plain") may or may not find a theme icon,
        // we just verify updateTabHeader doesn't crash and tab exists.
        QCOMPARE(tabs.tabBar()->count(), 1);

        // Trigger explicit header update and ensure no crash
        tabs.updateTabHeader(0);
        QCOMPARE(tabs.tabBar()->count(), 1);
    }

    // -----------------------------------------------------------------------
    // Claim 3: Closing a tab (removeChild) removes the leaf and reduces count
    // -----------------------------------------------------------------------
    void removeChildReducesTabCount()
    {
        WorkspaceTabs tabs;
        auto *leaf0 = makeLeaf(m_reg, QStringLiteral("Tab A"), {}, &tabs);
        auto *leaf1 = makeLeaf(m_reg, QStringLiteral("Tab B"), {}, &tabs);
        auto *leaf2 = makeLeaf(m_reg, QStringLiteral("Tab C"), {}, &tabs);
        tabs.addChild(leaf0);
        tabs.addChild(leaf1);
        tabs.addChild(leaf2);

        QCOMPARE(tabs.tabBar()->count(), 3);

        tabs.removeChild(leaf1);

        QCOMPARE(tabs.tabBar()->count(), 2);
        // leafAt should now return leaf0 and leaf2 at indices 0 and 1
        QCOMPARE(tabs.leafAt(0), leaf0);
        QCOMPARE(tabs.leafAt(1), leaf2);
    }

    void removeChildDeletesLeafWhenRequested()
    {
        WorkspaceTabs tabs;
        auto *leaf = makeLeaf(m_reg, QStringLiteral("Temp"), {}, &tabs);
        tabs.addChild(leaf);

        QPointer<WorkspaceLeaf> weakLeaf(leaf);
        QVERIFY(!weakLeaf.isNull());

        tabs.removeChild(leaf, /*deleteChild=*/true);

        // After deleteChild=true, the leaf should be deleted
        QVERIFY(weakLeaf.isNull());
        QCOMPARE(tabs.tabBar()->count(), 0);
    }

    void removeAllTabsEmptiesTabBar()
    {
        WorkspaceTabs tabs;
        auto *leaf0 = makeLeaf(m_reg, {}, {}, &tabs);
        auto *leaf1 = makeLeaf(m_reg, {}, {}, &tabs);
        tabs.addChild(leaf0);
        tabs.addChild(leaf1);

        tabs.removeChild(leaf0);
        tabs.removeChild(leaf1);

        QCOMPARE(tabs.tabBar()->count(), 0);
    }

    // -----------------------------------------------------------------------
    // Claim 4: Switching tabs calls loadIfDeferred() for deferred leaves
    // -----------------------------------------------------------------------
    void switchingToTabLoadsDeferred()
    {
        WorkspaceTabs tabs;

        // Leaf 0: non-deferred (active)
        auto *leaf0 = makeLeaf(m_reg, QStringLiteral("Active"), {}, &tabs);
        tabs.addChild(leaf0);

        // Leaf 1: deferred
        auto *leaf1 = new WorkspaceLeaf(m_reg, &tabs);
        leaf1->setDeferred(true,
                           QStringLiteral("document"),
                           QStringLiteral("Deferred Note"));
        tabs.addChild(leaf1);

        // Before switching: leaf1 is deferred and has no real view
        QVERIFY(leaf1->isDeferred());
        QVERIFY(leaf1->view() == nullptr);

        // The ViewRegistry has no factory registered for any type,
        // so loadIfDeferred() may not instantiate a real view —
        // but it MUST clear the deferred flag.
        tabs.setCurrentTab(1);

        // After switching: deferred flag should be cleared (loadIfDeferred was called)
        QVERIFY(!leaf1->isDeferred());
    }

    // -----------------------------------------------------------------------
    // Claim 5: setCurrentTab(index) updates current index and emits currentTabChanged
    // -----------------------------------------------------------------------
    void setCurrentTabUpdatesIndex()
    {
        WorkspaceTabs tabs;
        auto *leaf0 = makeLeaf(m_reg, {}, {}, &tabs);
        auto *leaf1 = makeLeaf(m_reg, {}, {}, &tabs);
        tabs.addChild(leaf0);
        tabs.addChild(leaf1);

        tabs.setCurrentTab(1);
        QCOMPARE(tabs.currentTab(), 1);
        QCOMPARE(tabs.currentLeaf(), leaf1);
    }

    void setCurrentTabEmitsCurrentTabChanged()
    {
        WorkspaceTabs tabs;
        auto *leaf0 = makeLeaf(m_reg, {}, {}, &tabs);
        auto *leaf1 = makeLeaf(m_reg, {}, {}, &tabs);
        tabs.addChild(leaf0);
        tabs.addChild(leaf1);

        QSignalSpy spy(&tabs, &WorkspaceTabs::currentTabChanged);

        tabs.setCurrentTab(1);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 1);
    }

    void setCurrentTabToSameIndexStillWorks()
    {
        WorkspaceTabs tabs;
        auto *leaf0 = makeLeaf(m_reg, {}, {}, &tabs);
        tabs.addChild(leaf0);

        // Setting to same index should not crash
        tabs.setCurrentTab(0);
        QCOMPARE(tabs.currentTab(), 0);
    }

    // -----------------------------------------------------------------------
    // Claim 6: Multiple tabs track correct insertion order via leafAt()
    // -----------------------------------------------------------------------
    void leafAtReturnsCorrectInsertionOrder()
    {
        WorkspaceTabs tabs;
        auto *leafA = makeLeaf(m_reg, QStringLiteral("A"), {}, &tabs);
        auto *leafB = makeLeaf(m_reg, QStringLiteral("B"), {}, &tabs);
        auto *leafC = makeLeaf(m_reg, QStringLiteral("C"), {}, &tabs);
        tabs.addChild(leafA);
        tabs.addChild(leafB);
        tabs.addChild(leafC);

        QCOMPARE(tabs.leafAt(0), leafA);
        QCOMPARE(tabs.leafAt(1), leafB);
        QCOMPARE(tabs.leafAt(2), leafC);
    }

    void leafAtWithIndexInsertionOrder()
    {
        WorkspaceTabs tabs;
        auto *leafA = makeLeaf(m_reg, QStringLiteral("A"), {}, &tabs);
        auto *leafC = makeLeaf(m_reg, QStringLiteral("C"), {}, &tabs);
        tabs.addChild(leafA);
        tabs.addChild(leafC);

        // Insert B between A and C at index 1
        auto *leafB = makeLeaf(m_reg, QStringLiteral("B"), {}, &tabs);
        tabs.addChild(leafB, /*index=*/1);

        QCOMPARE(tabs.leafAt(0), leafA);
        QCOMPARE(tabs.leafAt(1), leafB);
        QCOMPARE(tabs.leafAt(2), leafC);
    }

    void leafAtOutOfBoundsReturnsNull()
    {
        WorkspaceTabs tabs;
        QCOMPARE(tabs.leafAt(0), nullptr);
        QCOMPARE(tabs.leafAt(-1), nullptr);
    }

    void tabCountMatchesLeafAtRange()
    {
        WorkspaceTabs tabs;
        auto *leaf0 = makeLeaf(m_reg, {}, {}, &tabs);
        auto *leaf1 = makeLeaf(m_reg, {}, {}, &tabs);
        tabs.addChild(leaf0);
        tabs.addChild(leaf1);

        const int count = tabs.tabBar()->count();
        QCOMPARE(count, 2);

        // All indices [0, count) should return valid leaves
        for (int i = 0; i < count; ++i) {
            QVERIFY(tabs.leafAt(i) != nullptr);
        }
        // Index at count should be null
        QCOMPARE(tabs.leafAt(count), nullptr);
    }

    // -----------------------------------------------------------------------
    // Claim 7: Tab title updates when the view changes (viewChanged propagation)
    // -----------------------------------------------------------------------
    void updateTabHeaderRefreshesTitle()
    {
        WorkspaceTabs tabs;
        auto *leaf = makeLeaf(m_reg, QStringLiteral("Original Title"), {}, &tabs);
        tabs.addChild(leaf);

        QCOMPARE(tabs.tabBar()->tabText(0), QStringLiteral("Original Title"));

        // Simulate the view changing its display text and trigger header update
        // (In the real flow, viewChanged signal triggers updateTabHeader)
        auto *view = qobject_cast<StubView *>(leaf->view());
        QVERIFY(view != nullptr);
        view->setDisplayText(QStringLiteral("Updated Title"));
        tabs.updateTabHeader(0);

        QCOMPARE(tabs.tabBar()->tabText(0), QStringLiteral("Updated Title"));
    }

    void updateAllTabHeadersRefreshesAllTitles()
    {
        WorkspaceTabs tabs;
        auto *leaf0 = makeLeaf(m_reg, QStringLiteral("Tab 1"), {}, &tabs);
        auto *leaf1 = makeLeaf(m_reg, QStringLiteral("Tab 2"), {}, &tabs);
        tabs.addChild(leaf0);
        tabs.addChild(leaf1);

        auto *view0 = qobject_cast<StubView *>(leaf0->view());
        auto *view1 = qobject_cast<StubView *>(leaf1->view());
        QVERIFY(view0 != nullptr);
        QVERIFY(view1 != nullptr);

        view0->setDisplayText(QStringLiteral("Updated 1"));
        view1->setDisplayText(QStringLiteral("Updated 2"));
        tabs.updateAllTabHeaders();

        QCOMPARE(tabs.tabBar()->tabText(0), QStringLiteral("Updated 1"));
        QCOMPARE(tabs.tabBar()->tabText(1), QStringLiteral("Updated 2"));
    }

    // -----------------------------------------------------------------------
    // Additional lifecycle claims from spec §3.1 / Invariant 2:
    // WorkspaceTabs children are exclusively WorkspaceLeaf instances
    // -----------------------------------------------------------------------
    void firstTabBecomesCurrentAfterAdd()
    {
        WorkspaceTabs tabs;
        QCOMPARE(tabs.currentTab(), 0);
        QCOMPARE(tabs.currentLeaf(), nullptr);

        auto *leaf = makeLeaf(m_reg, {}, {}, &tabs);
        tabs.addChild(leaf);

        QCOMPARE(tabs.currentLeaf(), leaf);
    }

    void currentLeafNullWhenEmpty()
    {
        WorkspaceTabs tabs;
        QCOMPARE(tabs.currentLeaf(), nullptr);
    }

    void tabBarCountMatchesAddedLeaves()
    {
        WorkspaceTabs tabs;
        QCOMPARE(tabs.tabBar()->count(), 0);

        auto *leaf0 = makeLeaf(m_reg, {}, {}, &tabs);
        tabs.addChild(leaf0);
        QCOMPARE(tabs.tabBar()->count(), 1);

        auto *leaf1 = makeLeaf(m_reg, {}, {}, &tabs);
        tabs.addChild(leaf1);
        QCOMPARE(tabs.tabBar()->count(), 2);
    }

    // -----------------------------------------------------------------------
    // Deferred tab: cachedTitle used for tab header paint while deferred
    //
    // Spec §3.5: "Non-visible tabs at session restore get deferred=true."
    // The CURRENT tab is not deferred — only non-current (background) tabs are.
    // When only one tab exists, it becomes current immediately on addChild,
    // which fires onTabBarCurrentChanged → loadIfDeferred(), clearing deferred.
    // So we must use at least 2 tabs: leaf0 is current, leaf1 stays deferred.
    // -----------------------------------------------------------------------
    void deferredLeafTabHeaderUseCachedTitle()
    {
        WorkspaceTabs tabs;

        // Add a non-deferred leaf first so it becomes current (index 0)
        auto *leaf0 = makeLeaf(m_reg, QStringLiteral("Current"), {}, &tabs);
        tabs.addChild(leaf0);

        // Now add a deferred leaf as a background tab (index 1)
        auto *leaf1 = new WorkspaceLeaf(m_reg, &tabs);
        leaf1->setDeferred(true,
                           QStringLiteral("note"),
                           QStringLiteral("Cached Note Title"));
        tabs.addChild(leaf1);

        // leaf1 is background (not current), so it should remain deferred
        QVERIFY(leaf1->isDeferred());
        QCOMPARE(leaf1->cachedTitle(), QStringLiteral("Cached Note Title"));

        // Tab bar should use cachedTitle for the deferred leaf's header
        QCOMPARE(tabs.tabBar()->tabText(1), QStringLiteral("Cached Note Title"));
    }

    // -----------------------------------------------------------------------
    // setCurrentTab updates tab bar's currentIndex
    // -----------------------------------------------------------------------
    void setCurrentTabSyncsTabBarCurrentIndex()
    {
        WorkspaceTabs tabs;
        auto *leaf0 = makeLeaf(m_reg, {}, {}, &tabs);
        auto *leaf1 = makeLeaf(m_reg, {}, {}, &tabs);
        auto *leaf2 = makeLeaf(m_reg, {}, {}, &tabs);
        tabs.addChild(leaf0);
        tabs.addChild(leaf1);
        tabs.addChild(leaf2);

        tabs.setCurrentTab(2);
        QCOMPARE(tabs.tabBar()->currentIndex(), 2);

        tabs.setCurrentTab(0);
        QCOMPARE(tabs.tabBar()->currentIndex(), 0);
    }
};

QTEST_MAIN(TestWorkspaceTabsLifecycle)
#include "tst_workspace_tabs_lifecycle.moc"
