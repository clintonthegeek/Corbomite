// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/core/PluginContext.h"
#include "corbomite/core/proxies/VaultReader.h"
#include "corbomite/core/proxies/VaultWriter.h"

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
    void grantedAccessorsReturnProxyAfterServicesInstalled();
    void grantedPermissionsRetrievable();
    void hasPermissionMatchesGranted();
    void networkAccessorReturnsInstalledManager();
};

static Corbomite::PluginMetaData emptyMeta()
{
    return Corbomite::PluginMetaData(KPluginMetaData{});
}

void TestPluginContext::ungrantedAccessorsReturnNull()
{
    Corbomite::PluginContext ctx(emptyMeta(), {});
    // Even after installing services, ungranted permissions block access.
    auto *fakeVault    = reinterpret_cast<Corbomite::Vault *>(0x1);
    auto *fakeMetadata = reinterpret_cast<Corbomite::MetadataCache *>(0x1);
    auto *fakeWs       = reinterpret_cast<Corbomite::Workspace *>(0x1);
    auto *fakeCmds     = reinterpret_cast<Corbomite::CommandRegistry *>(0x1);
    auto *fakeViews    = reinterpret_cast<Corbomite::ViewRegistry *>(0x1);
    auto *fakeMenus    = reinterpret_cast<Corbomite::MenuEventEmitter *>(0x1);
    auto *fakeNet      = reinterpret_cast<QNetworkAccessManager *>(0x1);
    ctx.setCoreServices(fakeVault, fakeMetadata, fakeWs, fakeCmds, fakeViews,
                        fakeMenus, fakeNet);

    QCOMPARE(ctx.vaultReader(), nullptr);
    QCOMPARE(ctx.vaultWriter(), nullptr);
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
    // Permissions granted, but no services installed → still nullptr
    // (except secrets/process which don't need a core service).
    Corbomite::PluginContext ctx(emptyMeta(),
        {QStringLiteral("vault.read"), QStringLiteral("vault.write"),
         QStringLiteral("metadata.read"), QStringLiteral("workspace"),
         QStringLiteral("ui.commands"), QStringLiteral("ui.views"),
         QStringLiteral("ui.menus"), QStringLiteral("network")});

    QCOMPARE(ctx.vaultReader(), nullptr);
    QCOMPARE(ctx.vaultWriter(), nullptr);
    QCOMPARE(ctx.metadataCache(), nullptr);
    QCOMPARE(ctx.workspace(), nullptr);
    QCOMPARE(ctx.commands(), nullptr);
    QCOMPARE(ctx.views(), nullptr);
    QCOMPARE(ctx.menus(), nullptr);
    QCOMPARE(ctx.network(), nullptr);
}

void TestPluginContext::grantedAccessorsReturnProxyAfterServicesInstalled()
{
    Corbomite::PluginContext ctx(emptyMeta(),
        {QStringLiteral("vault.read"), QStringLiteral("vault.write")});

    auto *fakeVault = reinterpret_cast<Corbomite::Vault *>(0x1);
    ctx.setCoreServices(fakeVault, nullptr, nullptr, nullptr, nullptr,
                        nullptr, nullptr);

    QVERIFY(ctx.vaultReader() != nullptr);
    QVERIFY(ctx.vaultWriter() != nullptr);
    QCOMPARE(ctx.metadataCache(), nullptr); // service not installed
    QCOMPARE(ctx.network(), nullptr);       // permission not granted
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
    // Network proxy is the special case: returns the underlying QNAM directly,
    // not a wrapping proxy. Verify identity.
    Corbomite::PluginContext ctx(emptyMeta(), {QStringLiteral("network")});
    auto *fakeNet = reinterpret_cast<QNetworkAccessManager *>(0x42);
    ctx.setCoreServices(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                        fakeNet);
    QCOMPARE(ctx.network(), fakeNet);
}

QTEST_MAIN(TestPluginContext)
#include "tst_plugin_context.moc"
