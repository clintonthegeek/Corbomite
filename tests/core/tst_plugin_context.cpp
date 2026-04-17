// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTemporaryDir>
#include <QTest>

#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/LinkResolver.h"
#include "corbomite/storage/MetadataCache.h"
#include "corbomite/storage/SQLiteIndex.h"
#include "corbomite/storage/proxies/SearchProxy.h"
#include "corbomite/vault/FileManager.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/Vault.h"

// Note: granted accessors return nullptr unless the underlying core service
// has been installed via setCoreServices. We pass dummy non-null pointers
// (cast from intptr) because the proxies don't dereference in these tests
// — they're stubs.

class TestPluginContext : public QObject
{
    Q_OBJECT
private slots:
    void ungrantedAccessorsReturnNull();
    void grantedAccessorsReturnNullWithoutCoreServices();
    void grantedPermissionsRetrievable();
    void hasPermissionMatchesGranted();
    void networkAccessorReturnsInstalledManager();
    void vaultAndFileManagerProxiesLazyConstruct();
    void searchAccessorLazyConstructsWhenGranted();
    void searchAccessorReturnsNullWhenPermissionDenied();
};

static Corbomite::PluginMetaData emptyMeta()
{
    return Corbomite::PluginMetaData(KPluginMetaData{});
}

void TestPluginContext::ungrantedAccessorsReturnNull()
{
    Corbomite::PluginContext ctx(emptyMeta(), {});
    auto *fakeVault    = reinterpret_cast<Corbomite::Vault *>(0x1);
    auto *fakeFm       = reinterpret_cast<Corbomite::FileManager *>(0x1);
    auto *fakeMetadata = reinterpret_cast<Corbomite::MetadataCache *>(0x1);
    auto *fakeWs       = reinterpret_cast<Corbomite::Workspace *>(0x1);
    auto *fakeCmds     = reinterpret_cast<Corbomite::CommandRegistry *>(0x1);
    auto *fakeViews    = reinterpret_cast<Corbomite::ViewRegistry *>(0x1);
    auto *fakeMenus    = reinterpret_cast<Corbomite::MenuEventEmitter *>(0x1);
    auto *fakeNet      = reinterpret_cast<QNetworkAccessManager *>(0x1);
    ctx.setCoreServices(fakeVault, fakeFm, fakeMetadata, nullptr,
                        fakeWs, fakeCmds, fakeViews, fakeMenus, fakeNet);

    QCOMPARE(ctx.vault(), nullptr);
    QCOMPARE(ctx.fileManager(), nullptr);
    QCOMPARE(ctx.metadataCache(), nullptr);
    QCOMPARE(ctx.workspace(), nullptr);
    QCOMPARE(ctx.commands(), nullptr);
    QCOMPARE(ctx.views(), nullptr);
    QCOMPARE(ctx.menus(), nullptr);
    QCOMPARE(ctx.network(), nullptr);
    QCOMPARE(ctx.secrets(), nullptr);
    QCOMPARE(ctx.process(), nullptr);
}

void TestPluginContext::grantedAccessorsReturnNullWithoutCoreServices()
{
    Corbomite::PluginContext ctx(emptyMeta(),
        {QStringLiteral("vault.read"), QStringLiteral("vault.write"),
         QStringLiteral("metadata.read"), QStringLiteral("workspace"),
         QStringLiteral("ui.commands"), QStringLiteral("ui.views"),
         QStringLiteral("ui.menus"), QStringLiteral("network")});

    QCOMPARE(ctx.vault(), nullptr);
    QCOMPARE(ctx.fileManager(), nullptr);
    QCOMPARE(ctx.metadataCache(), nullptr);
    QCOMPARE(ctx.workspace(), nullptr);
    QCOMPARE(ctx.commands(), nullptr);
    QCOMPARE(ctx.views(), nullptr);
    QCOMPARE(ctx.menus(), nullptr);
    QCOMPARE(ctx.network(), nullptr);
}

void TestPluginContext::grantedPermissionsRetrievable()
{
    QSet<QString> granted{QStringLiteral("vault.read"), QStringLiteral("ui.views")};
    Corbomite::PluginContext ctx(emptyMeta(), granted);
    QCOMPARE(ctx.grantedPermissions(), granted);
}

void TestPluginContext::hasPermissionMatchesGranted()
{
    Corbomite::PluginContext ctx(emptyMeta(),
        {QStringLiteral("vault.read"), QStringLiteral("ui.views")});
    QVERIFY(ctx.hasPermission(QStringLiteral("vault.read")));
    QVERIFY(ctx.hasPermission(QStringLiteral("ui.views")));
    QVERIFY(!ctx.hasPermission(QStringLiteral("network")));
    QVERIFY(!ctx.hasPermission(QStringLiteral("vault.write")));
}

void TestPluginContext::networkAccessorReturnsInstalledManager()
{
    Corbomite::PluginContext ctx(emptyMeta(), {QStringLiteral("network")});
    auto *fakeNet = reinterpret_cast<QNetworkAccessManager *>(0x42);
    ctx.setCoreServices(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                        nullptr, nullptr, fakeNet);
    QCOMPARE(ctx.network(), fakeNet);
}

void TestPluginContext::vaultAndFileManagerProxiesLazyConstruct()
{
    Corbomite::FileSystemAdapter fs;
    QTemporaryDir dir;
    Corbomite::Vault vault(&fs);
    vault.load(dir.path());
    Corbomite::LinkResolver resolver;
    Corbomite::MetadataCache cache(resolver);
    Corbomite::FileManager fm(&vault, &cache);

    Corbomite::PluginContext ctx(emptyMeta(),
        {QStringLiteral("vault.read"), QStringLiteral("vault.write"),
         QStringLiteral("metadata.read")});
    ctx.setCoreServices(&vault, &fm, &cache, nullptr, nullptr, nullptr,
                        nullptr, nullptr, nullptr);

    auto *vproxy = ctx.vault();
    auto *fmproxy = ctx.fileManager();
    QVERIFY(vproxy != nullptr);
    QVERIFY(fmproxy != nullptr);
    // Lazy construction: second call returns same instance.
    QCOMPARE(ctx.vault(), vproxy);
    QCOMPARE(ctx.fileManager(), fmproxy);
}

void TestPluginContext::searchAccessorLazyConstructsWhenGranted()
{
    QSet<QString> granted = { QStringLiteral("metadata.read") };
    Corbomite::PluginContext ctx(emptyMeta(), granted);

    Corbomite::SQLiteIndex index;
    ctx.setCoreServices(nullptr, nullptr, nullptr, &index, nullptr, nullptr,
                        nullptr, nullptr, nullptr);

    auto *proxy = ctx.search();
    QVERIFY(proxy != nullptr);
    QCOMPARE(ctx.search(), proxy);  // idempotent
}

void TestPluginContext::searchAccessorReturnsNullWhenPermissionDenied()
{
    QSet<QString> granted;  // no metadata.read
    Corbomite::PluginContext ctx(emptyMeta(), granted);

    Corbomite::SQLiteIndex index;
    ctx.setCoreServices(nullptr, nullptr, nullptr, &index, nullptr, nullptr,
                        nullptr, nullptr, nullptr);

    QVERIFY(ctx.search() == nullptr);
}

QTEST_MAIN(TestPluginContext)
#include "tst_plugin_context.moc"
