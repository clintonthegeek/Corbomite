// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QFocusEvent>
#include <QTest>
#include <QWidget>
#include "corbomite/vault/Plugin.h"
#include "corbomite/vault/PluginContext.h"

class TrackingPlugin : public Corbomite::Plugin
{
    Q_OBJECT
public:
    int onLoadCalls = 0;
    int onUnloadCalls = 0;
    Corbomite::PluginContext *lastContext = nullptr;

    void onLoad(Corbomite::PluginContext *ctx) override
    {
        ++onLoadCalls;
        lastContext = ctx;
    }
    void onUnload() override { ++onUnloadCalls; }
};

class TestPlugin : public QObject
{
    Q_OBJECT
private slots:
    void lifecycleFiresOnLoadOnce();
    void lifecycleFiresOnUnloadOnceOnExplicitUnload();
    void componentCleanupsFire();
    void cleanupsFireLifo();
    void contextClearedAfterUnload();
    void createViewDefaultsToNullptr();
    void focusDefaultsToSetFocusOnWidget();
};

static Corbomite::PluginContext *makeCtx()
{
    return new Corbomite::PluginContext(
        Corbomite::PluginMetaData(KPluginMetaData{}), {});
}

void TestPlugin::lifecycleFiresOnLoadOnce()
{
    TrackingPlugin p;
    auto *ctx = makeCtx();
    p.load(ctx);
    QCOMPARE(p.onLoadCalls, 1);
    QCOMPARE(p.lastContext, ctx);

    // Idempotent — second load is a no-op (Component is already loaded).
    p.load(ctx);
    QCOMPARE(p.onLoadCalls, 1);

    p.unload();
    delete ctx;
}

void TestPlugin::lifecycleFiresOnUnloadOnceOnExplicitUnload()
{
    TrackingPlugin p;
    auto *ctx = makeCtx();
    p.load(ctx);
    p.unload();
    QCOMPARE(p.onUnloadCalls, 1);

    // Idempotent — second unload is a no-op.
    p.unload();
    QCOMPARE(p.onUnloadCalls, 1);
    delete ctx;
}

void TestPlugin::componentCleanupsFire()
{
    TrackingPlugin p;
    auto *ctx = makeCtx();
    p.load(ctx);
    int cleanupRuns = 0;
    p.registerCleanup([&] { ++cleanupRuns; });
    p.unload();
    QCOMPARE(cleanupRuns, 1);
    delete ctx;
}

void TestPlugin::cleanupsFireLifo()
{
    TrackingPlugin p;
    auto *ctx = makeCtx();
    p.load(ctx);
    QString order;
    p.registerCleanup([&] { order += QStringLiteral("a"); });
    p.registerCleanup([&] { order += QStringLiteral("b"); });
    p.registerCleanup([&] { order += QStringLiteral("c"); });
    p.unload();
    // Last-registered runs first.
    QCOMPARE(order, QStringLiteral("cba"));
    delete ctx;
}

void TestPlugin::contextClearedAfterUnload()
{
    TrackingPlugin p;
    auto *ctx = makeCtx();
    p.load(ctx);
    QCOMPARE(p.context(), ctx);
    p.unload();
    QCOMPARE(p.context(), nullptr);
    delete ctx;
}

void TestPlugin::createViewDefaultsToNullptr()
{
    TrackingPlugin p;
    QCOMPARE(p.createView(nullptr), nullptr);
    QCOMPARE(p.configPages(), 0);
    QCOMPARE(p.configPage(0, nullptr), nullptr);
}

void TestPlugin::focusDefaultsToSetFocusOnWidget()
{
    TrackingPlugin p;
    // Null input is a safe no-op.
    p.focus(nullptr);

    // Non-widget QObject is ignored (no crash, no state change).
    QObject nonWidget;
    p.focus(&nonWidget);

    // Widget input: default implementation dispatches setFocus. Under an
    // offscreen platform we can't meaningfully observe focus transitions
    // through the window system, so assert dispatch-by-classification via
    // a widget that records the focus-in event.
    class FocusCatcher : public QWidget
    {
    public:
        int focusInCount = 0;
    protected:
        void focusInEvent(QFocusEvent *e) override
        {
            ++focusInCount;
            QWidget::focusInEvent(e);
        }
    };
    QWidget host;
    host.resize(10, 10);
    auto *catcher = new FocusCatcher;
    catcher->setParent(&host);
    catcher->setFocusPolicy(Qt::StrongFocus);
    host.show();
    QApplication::processEvents();

    p.focus(catcher);
    QApplication::processEvents();
    QVERIFY(catcher->focusInCount >= 1);
}

QTEST_MAIN(TestPlugin)
#include "tst_plugin.moc"
