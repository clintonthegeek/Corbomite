// tests/core/tst_view_lifecycle.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QWidget>
#include "corbomite/core/View.h"
#include "corbomite/core/Component.h"

using namespace Corbomite;

class StubView : public View
{
    Q_OBJECT
public:
    using View::View;
    QString getViewType() const override { return QStringLiteral("stub"); }
    QString getDisplayText() const override { return QStringLiteral("Stub View"); }

    bool openCalled = false;
    bool closeCalled = false;

protected:
    void onOpen() override { openCalled = true; }
    void onClose() override { closeCalled = true; }
};

class TestViewLifecycle : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void constructionCreatesComponent()
    {
        StubView view(nullptr);
        QVERIFY(view.component() != nullptr);
        QVERIFY(!view.component()->isLoaded());
    }

    void openLoadsComponentAndCallsOnOpen()
    {
        QWidget parent;
        StubView view(nullptr);
        view.open(&parent);
        QVERIFY(view.component()->isLoaded());
        QVERIFY(view.openCalled);
    }

    void closeUnloadsComponentAndCallsOnClose()
    {
        QWidget parent;
        StubView view(nullptr);
        view.open(&parent);
        view.close();
        QVERIFY(!view.component()->isLoaded());
        QVERIFY(view.closeCalled);
    }

    void doubleOpenIsIdempotent()
    {
        QWidget parent;
        StubView view(nullptr);
        view.open(&parent);
        view.open(&parent);
        QVERIFY(view.component()->isLoaded());
    }

    void gettersReturnDefaults()
    {
        StubView view(nullptr);
        QCOMPARE(view.getViewType(), QStringLiteral("stub"));
        QCOMPARE(view.getDisplayText(), QStringLiteral("Stub View"));
        QCOMPARE(view.getIcon(), QStringLiteral("document"));
    }

    void stateRoundTrips()
    {
        StubView view(nullptr);
        QJsonObject state;
        state[QStringLiteral("key")] = QStringLiteral("value");
        view.setState(state);
        // Default impl is no-op, so getState returns {}
        QVERIFY(view.getState().isEmpty());
    }

    void registerQObjectConnectionDelegates()
    {
        QWidget parent;
        StubView view(nullptr);
        view.open(&parent);
        QObject sender;
        bool called = false;
        auto conn = QObject::connect(&sender, &QObject::destroyed, [&] { called = true; });
        view.registerQObjectConnection(conn);
        QVERIFY(!called);
        view.close();
    }

    void containerWidgetExists()
    {
        StubView view(nullptr);
        QVERIFY(view.containerWidget() != nullptr);
    }
};

QTEST_MAIN(TestViewLifecycle)
#include "tst_view_lifecycle.moc"
