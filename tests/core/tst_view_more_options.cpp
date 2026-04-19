// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest/QtTest>
#include <QMenu>
#include <QSignalSpy>

#include "corbomite/core/ItemView.h"
#include "corbomite/core/View.h"
#include "corbomite/core/MenuSectionHelper.h"

class TestView : public Corbomite::View {
public:
    using Corbomite::View::View;
    QStringList callOrder;

    QString getViewType() const override { return QStringLiteral("test-view"); }
    QString getDisplayText() const override { return QStringLiteral("Test"); }

    void onMoreOptionsMenu(Corbomite::MenuSectionHelper &helper) override
    {
        callOrder << QStringLiteral("onMoreOptionsMenu");
        auto *action = new QAction(QStringLiteral("Custom item"), this);
        helper.addToSection(action, QStringLiteral("action"));
    }

    void onPaneMenu(QMenu * /*menu*/, const QString &source) override
    {
        callOrder << (QStringLiteral("onPaneMenu:") + source);
    }
};

class TestViewMoreOptions : public QObject {
    Q_OBJECT

private slots:
    void testDispatchOrderHamburgerPath()
    {
        TestView view(nullptr);
        QMenu menu;
        Corbomite::MenuSectionHelper helper(&menu);

        // Simulate ItemView::showMoreOptionsMenu dispatch sequence
        view.onMoreOptionsMenu(helper);
        view.onPaneMenu(&menu, QStringLiteral("more-options"));
        helper.finalize();

        QCOMPARE(view.callOrder.size(), 2);
        QCOMPARE(view.callOrder[0], QStringLiteral("onMoreOptionsMenu"));
        QCOMPARE(view.callOrder[1], QStringLiteral("onPaneMenu:more-options"));

        // Helper action landed in action section
        QCOMPARE(menu.actions().size(), 1);
        QCOMPARE(menu.actions()[0]->text(), QStringLiteral("Custom item"));
    }

    void testOnPaneMenuSourceDefaultForwarder()
    {
        // A subclass overriding only the zero-arg onPaneMenu should still be
        // called via the two-arg overload (default forwarder delegates).
        class LegacyView : public Corbomite::View {
        public:
            using Corbomite::View::View;
            using Corbomite::View::onPaneMenu;  // un-hide two-arg overload
            int paneMenuCalls = 0;
            QString getViewType() const override { return QStringLiteral("legacy-view"); }
            QString getDisplayText() const override { return QStringLiteral("Legacy"); }
            void onPaneMenu(QMenu * /*menu*/) override { ++paneMenuCalls; }
        };

        LegacyView view(nullptr);
        QMenu menu;
        view.onPaneMenu(&menu, QStringLiteral("more-options"));
        QCOMPARE(view.paneMenuCalls, 1);
    }

    void testItemViewShowMoreOptionsIntegration()
    {
        class SpyItemView : public Corbomite::ItemView {
        public:
            using Corbomite::ItemView::ItemView;
            using Corbomite::View::onPaneMenu;  // un-hide two-arg overload
            QStringList order;

            QString getViewType() const override { return QStringLiteral("spy-item-view"); }
            QString getDisplayText() const override { return QStringLiteral("Spy"); }

            void onMoreOptionsMenu(Corbomite::MenuSectionHelper &h) override
            {
                order << QStringLiteral("onMoreOptionsMenu");
                auto *a = new QAction(QStringLiteral("A1"), this);
                h.addToSection(a, QStringLiteral("action"));
            }
            void onPaneMenu(QMenu * /*m*/, const QString &src) override
            {
                order << (QStringLiteral("onPaneMenu:") + src);
            }
        };

        SpyItemView view(nullptr);
        QMenu menu;
        view.buildMoreOptionsMenu(&menu);

        QVERIFY(view.order.contains(QStringLiteral("onMoreOptionsMenu")));
        QVERIFY(view.order.contains(QStringLiteral("onPaneMenu:more-options")));
        QVERIFY(view.order.indexOf(QStringLiteral("onMoreOptionsMenu")) <
                view.order.indexOf(QStringLiteral("onPaneMenu:more-options")));
    }
};

QTEST_MAIN(TestViewMoreOptions)
#include "tst_view_more_options.moc"
