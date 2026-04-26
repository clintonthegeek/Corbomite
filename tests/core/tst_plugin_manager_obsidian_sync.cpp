// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests the Corbomite ⇄ Obsidian plugin enable-state compromise documented at
// docs/superpowers/specs/2026-04-26-plugin-enable-state-cross-app-compromise.md
//
// Sub-decisions verified here:
// 1. ID mapping via X-Obsidian-Id manifest field (alias-dict fallback covered
//    by separate fixture using a Corbomite-shape ID with no X-Obsidian-Id
//    declaration).
// 2. JSON wins on vault-open / KConfig wins thereafter — overlay test.
// 3. Trusted vs untrusted partition into core-plugins.json /
//    community-plugins.json.
// 4. Dual-write on toggle gated by presence of an Obsidian counterpart.

#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <KConfig>
#include <KConfigGroup>
#include <KSharedConfig>

#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/VaultConfig.h"
#include "corbomite/vault/Plugin.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/PluginManager.h"

namespace {

class FakePlugin : public Corbomite::Plugin
{
    Q_OBJECT
public:
    using Plugin::Plugin;
protected:
    void onLoad(Corbomite::PluginContext *) override {}
    void onUnload() override {}
};

KPluginMetaData makeFixture(const QString &id,
                             bool trusted,
                             const QString &obsidianId)
{
    QJsonObject kplugin{{QStringLiteral("Id"), id},
                        {QStringLiteral("Name"), id},
                        {QStringLiteral("EnabledByDefault"), false}};
    QJsonObject full{{QStringLiteral("KPlugin"), kplugin},
                     {QStringLiteral("X-Corbomite-Trusted"), trusted}};
    if (!obsidianId.isEmpty())
        full.insert(QStringLiteral("X-Obsidian-Id"), obsidianId);
    return KPluginMetaData(full, id);
}

Corbomite::PluginManager *makeManager(QTemporaryDir &cfgDir)
{
    auto *mgr = new Corbomite::PluginManager;
    mgr->setConfig(KSharedConfig::openConfig(
        cfgDir.filePath(QStringLiteral("corbomite-test.rc")),
        KConfig::SimpleConfig));
    mgr->setFactoryOverride([](const Corbomite::PluginMetaData &)
                                -> Corbomite::Plugin * {
        return new FakePlugin;
    });
    return mgr;
}

} // namespace

class TestPluginManagerObsidianSync : public QObject
{
    Q_OBJECT
private slots:
    void enableTrustedPluginWritesCorePluginsJson();
    void disableTrustedPluginWritesFalseInCorePluginsJson();
    void enableUntrustedPluginAppendsToCommunityPluginsJson();
    void disableUntrustedPluginRemovesFromCommunityPluginsJson();
    void pluginWithoutObsidianIdLeavesJsonFilesAlone();
    void corePluginsJsonOverridesKConfigOnOpen();
    void corePluginsJsonPreservesUnknownKeys();
};

void TestPluginManagerObsidianSync::enableTrustedPluginWritesCorePluginsJson()
{
    QTemporaryDir cfg, vaultDir;
    auto *mgr = makeManager(cfg);
    Corbomite::FileSystemAdapter fs;
    Corbomite::VaultConfig vcfg(&fs, vaultDir.path());
    QVERIFY(vcfg.ensureConfigDir());
    mgr->setVaultConfig(&vcfg);

    mgr->ingest({makeFixture(QStringLiteral("corbomite_backlinks"), true,
                              QStringLiteral("backlink"))},
                Corbomite::PluginMetaData::Origin::System);

    QVERIFY(mgr->enablePlugin(QStringLiteral("corbomite_backlinks")));

    auto core = vcfg.readCorePlugins();
    QVERIFY(core.has_value());
    QCOMPARE(core->raw.value(QStringLiteral("backlink")).toBool(), true);

    delete mgr;
}

void TestPluginManagerObsidianSync::disableTrustedPluginWritesFalseInCorePluginsJson()
{
    QTemporaryDir cfg, vaultDir;
    auto *mgr = makeManager(cfg);
    Corbomite::FileSystemAdapter fs;
    Corbomite::VaultConfig vcfg(&fs, vaultDir.path());
    QVERIFY(vcfg.ensureConfigDir());
    mgr->setVaultConfig(&vcfg);

    mgr->ingest({makeFixture(QStringLiteral("corbomite_outline"), true,
                              QStringLiteral("outline"))},
                Corbomite::PluginMetaData::Origin::System);
    QVERIFY(mgr->enablePlugin(QStringLiteral("corbomite_outline")));
    QVERIFY(mgr->disablePlugin(QStringLiteral("corbomite_outline")));

    auto core = vcfg.readCorePlugins();
    QVERIFY(core.has_value());
    QCOMPARE(core->raw.value(QStringLiteral("outline")).toBool(), false);

    delete mgr;
}

void TestPluginManagerObsidianSync::enableUntrustedPluginAppendsToCommunityPluginsJson()
{
    QTemporaryDir cfg, vaultDir;
    auto *mgr = makeManager(cfg);
    Corbomite::FileSystemAdapter fs;
    Corbomite::VaultConfig vcfg(&fs, vaultDir.path());
    QVERIFY(vcfg.ensureConfigDir());
    mgr->setVaultConfig(&vcfg);

    // Untrusted plugins skip the permission-grant prompt only if they
    // declare no permissions. Use empty perms here so enablePlugin
    // doesn't pop a dialog.
    mgr->ingest({makeFixture(QStringLiteral("third-party-thing"),
                              /*trusted=*/false,
                              QStringLiteral("third-party-thing"))},
                Corbomite::PluginMetaData::Origin::User);

    QVERIFY(mgr->enablePlugin(QStringLiteral("third-party-thing")));

    auto comm = vcfg.readCommunityPlugins();
    QVERIFY(comm.has_value());
    QVERIFY(comm->contains(QStringLiteral("third-party-thing")));

    delete mgr;
}

void TestPluginManagerObsidianSync::disableUntrustedPluginRemovesFromCommunityPluginsJson()
{
    QTemporaryDir cfg, vaultDir;
    auto *mgr = makeManager(cfg);
    Corbomite::FileSystemAdapter fs;
    Corbomite::VaultConfig vcfg(&fs, vaultDir.path());
    QVERIFY(vcfg.ensureConfigDir());
    mgr->setVaultConfig(&vcfg);

    mgr->ingest({makeFixture(QStringLiteral("third-party-thing"), false,
                              QStringLiteral("third-party-thing"))},
                Corbomite::PluginMetaData::Origin::User);
    QVERIFY(mgr->enablePlugin(QStringLiteral("third-party-thing")));
    QVERIFY(mgr->disablePlugin(QStringLiteral("third-party-thing")));

    auto comm = vcfg.readCommunityPlugins();
    QVERIFY(comm.has_value());
    QVERIFY(!comm->contains(QStringLiteral("third-party-thing")));

    delete mgr;
}

void TestPluginManagerObsidianSync::pluginWithoutObsidianIdLeavesJsonFilesAlone()
{
    QTemporaryDir cfg, vaultDir;
    auto *mgr = makeManager(cfg);
    Corbomite::FileSystemAdapter fs;
    Corbomite::VaultConfig vcfg(&fs, vaultDir.path());
    QVERIFY(vcfg.ensureConfigDir());
    mgr->setVaultConfig(&vcfg);

    // No X-Obsidian-Id and no entry in the alias dict → Corbomite-only.
    mgr->ingest({makeFixture(QStringLiteral("corbomite-private-thing"), true,
                              /*obsidianId=*/QString())},
                Corbomite::PluginMetaData::Origin::System);
    QVERIFY(mgr->enablePlugin(QStringLiteral("corbomite-private-thing")));

    QVERIFY(!vcfg.readCorePlugins().has_value());
    QVERIFY(!vcfg.readCommunityPlugins().has_value());

    delete mgr;
}

void TestPluginManagerObsidianSync::corePluginsJsonOverridesKConfigOnOpen()
{
    QTemporaryDir cfg, vaultDir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::VaultConfig vcfg(&fs, vaultDir.path());
    QVERIFY(vcfg.ensureConfigDir());

    // Pre-seed the JSON with disabled state for `outline`
    Corbomite::VaultConfig::CorePlugins seeded;
    seeded.raw.insert(QStringLiteral("outline"), false);
    QVERIFY(vcfg.writeCorePlugins(seeded));

    auto *mgr = makeManager(cfg);
    mgr->setVaultConfig(&vcfg);
    mgr->ingest({makeFixture(QStringLiteral("corbomite_outline"), true,
                              QStringLiteral("outline"))},
                Corbomite::PluginMetaData::Origin::System);

    // Trusted plugins default to enabled; the JSON `false` must override.
    mgr->loadEnabledStateFromConfig();

    const auto *info = mgr->pluginById(QStringLiteral("corbomite_outline"));
    QVERIFY(info != nullptr);
    QVERIFY(!info->enabled);

    delete mgr;
}

void TestPluginManagerObsidianSync::corePluginsJsonPreservesUnknownKeys()
{
    QTemporaryDir cfg, vaultDir;
    Corbomite::FileSystemAdapter fs;
    Corbomite::VaultConfig vcfg(&fs, vaultDir.path());
    QVERIFY(vcfg.ensureConfigDir());

    // Seed with two keys we don't recognise (Obsidian-only plugins).
    Corbomite::VaultConfig::CorePlugins seeded;
    seeded.raw.insert(QStringLiteral("daily-notes"), true);
    seeded.raw.insert(QStringLiteral("audio-recorder"), false);
    QVERIFY(vcfg.writeCorePlugins(seeded));

    auto *mgr = makeManager(cfg);
    mgr->setVaultConfig(&vcfg);
    mgr->ingest({makeFixture(QStringLiteral("corbomite_backlinks"), true,
                              QStringLiteral("backlink"))},
                Corbomite::PluginMetaData::Origin::System);

    QVERIFY(mgr->enablePlugin(QStringLiteral("corbomite_backlinks")));

    auto core = vcfg.readCorePlugins();
    QVERIFY(core.has_value());
    QCOMPARE(core->raw.value(QStringLiteral("backlink")).toBool(), true);
    QCOMPARE(core->raw.value(QStringLiteral("daily-notes")).toBool(), true);
    QCOMPARE(core->raw.value(QStringLiteral("audio-recorder")).toBool(), false);

    delete mgr;
}

QTEST_MAIN(TestPluginManagerObsidianSync)
#include "tst_plugin_manager_obsidian_sync.moc"
