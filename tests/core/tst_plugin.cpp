// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
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

QTEST_MAIN(TestPlugin)
#include "tst_plugin.moc"
