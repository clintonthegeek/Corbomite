// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/VaultConfig.h"

using namespace Corbomite;

namespace {

void writeRaw(const QString &path, const QByteArray &bytes)
{
    QFile f(path);
    QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(path));
    f.write(bytes);
}

/// Every key present in the on-disk object at `name`, or an assertion
/// failure. Reading the files back (rather than trusting what was written
/// through the API) is what makes a partition bug visible.
QJsonObject onDisk(VaultConfig &cfg, const QString &name)
{
    const auto obj = cfg.readJson(name);
    if (!obj) return {};
    return *obj;
}

} // namespace

/// The `app.json` / `appearance.json` partition, the value encodings, and
/// the `.obsidian/<corePluginId>.json` file class. Spec source:
/// `docs/obsidian-audit/addenda/2026-10-03-vault-config-split-and-defaults.md`
/// and `.../2026-10-03-settings-config-file-ownership-and-definitions.md`.
class TestVaultConfigPartition : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    // --- Task 1: the 16-key allow-list is the whole partition -------------

    void allowListIsExactlyTheSixteenObsidianKeys()
    {
        const QStringList expected{
            QStringLiteral("accentColor"),
            QStringLiteral("theme"),
            QStringLiteral("cssTheme"),
            QStringLiteral("enabledCssSnippets"),
            QStringLiteral("showViewHeader"),
            QStringLiteral("showRibbon"),
            QStringLiteral("nativeMenus"),
            QStringLiteral("translucency"),
            QStringLiteral("textFontFamily"),
            QStringLiteral("interfaceFontFamily"),
            QStringLiteral("monospaceFontFamily"),
            QStringLiteral("baseFontSize"),
            QStringLiteral("baseFontSizeAction"),
            QStringLiteral("slidingSidebar"),
            QStringLiteral("floatingNavigation"),
            QStringLiteral("autoFullScreen"),
        };
        QCOMPARE(VaultConfig::appearanceKeys(), expected);
        QCOMPARE(VaultConfig::appearanceKeys().size(), 16);
    }

    void theTenMisfiledKeysAreRecognisedAsAppearanceKeys()
    {
        // `docs/obsidian-audit/domains/vault.md §2` files all ten of these
        // under AppConfig. Each would be silently reverted by Obsidian.
        for (const QString &key : {QStringLiteral("enabledCssSnippets"),
                                   QStringLiteral("showViewHeader"),
                                   QStringLiteral("showRibbon"),
                                   QStringLiteral("nativeMenus"),
                                   QStringLiteral("translucency"),
                                   QStringLiteral("interfaceFontFamily"),
                                   QStringLiteral("monospaceFontFamily"),
                                   QStringLiteral("slidingSidebar"),
                                   QStringLiteral("floatingNavigation"),
                                   QStringLiteral("autoFullScreen")}) {
            QVERIFY2(VaultConfig::isAppearanceKey(key), qPrintable(key));
        }
    }

    void keysOutsideTheAllowListAreAppJsonKeys()
    {
        for (const QString &key : {QStringLiteral("openBehavior"),
                                   QStringLiteral("attachmentFolderPath"),
                                   QStringLiteral("userIgnoreFilters"),
                                   QStringLiteral("trashOption"),
                                   QStringLiteral("promptDelete"),
                                   QStringLiteral("hotkeys"),
                                   QStringLiteral("types"),
                                   QStringLiteral("settingsPopoutWindow"),
                                   QStringLiteral("pdfExportSettings"),
                                   // Unknown keys must fall through to
                                   // app.json, never be dropped or diverted.
                                   QStringLiteral("_futurePluginKey"),
                                   QStringLiteral("theme2")}) {
            QVERIFY2(!VaultConfig::isAppearanceKey(key), qPrintable(key));
        }
    }

    void saveConfigRoutesEachKeyToItsOwnFile()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        QVERIFY(cfg.ensureConfigDir());

        QJsonObject merged;
        merged.insert(QStringLiteral("theme"), QStringLiteral("obsidian"));
        merged.insert(QStringLiteral("showRibbon"), false);
        merged.insert(QStringLiteral("openBehavior"),
                      QStringLiteral("file:Notes/Home.md"));
        merged.insert(QStringLiteral("trashOption"), QStringLiteral("vault"));
        merged.insert(QStringLiteral("userIgnoreFilters"),
                      QJsonArray{QStringLiteral("/^tmp/")});
        QVERIFY(cfg.saveConfig(merged));

        const QJsonObject app = onDisk(cfg, QStringLiteral("app.json"));
        const QJsonObject appearance =
            onDisk(cfg, QStringLiteral("appearance.json"));

        QCOMPARE(app.keys(), (QStringList{
                                QStringLiteral("openBehavior"),
                                QStringLiteral("trashOption"),
                                QStringLiteral("userIgnoreFilters")}));
        // QJsonObject::keys() is sorted, so `showRibbon` precedes `theme`
        // regardless of insertion order.
        QCOMPARE(appearance.keys(), (QStringList{
                                        QStringLiteral("showRibbon"),
                                        QStringLiteral("theme")}));
        QCOMPARE(app.value(QStringLiteral("openBehavior")).toString(),
                 QStringLiteral("file:Notes/Home.md"));
        QCOMPARE(appearance.value(QStringLiteral("showRibbon")).toBool(), false);
    }

    void saveConfigRewritesBothFilesAndClearsTheStaleCopy()
    {
        // The failure mode that motivates the whole partition: a key left in
        // the other file is overwritten by Obsidian's next save from its own
        // merged map, silently reverting the Corbomite-side change.
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        QVERIFY(cfg.ensureConfigDir());

        writeRaw(cfg.configFilePath(QStringLiteral("app.json")),
                 R"({"showRibbon": true, "trashOption": "vault"})");
        QVERIFY(!cfg.readAppearanceJson().has_value());

        QVERIFY(cfg.setConfigValue(QStringLiteral("showRibbon"), false));

        const QJsonObject app = onDisk(cfg, QStringLiteral("app.json"));
        QVERIFY2(!app.contains(QStringLiteral("showRibbon")),
                 "showRibbon is an appearance.json key and must not survive "
                 "in app.json");
        QCOMPARE(app.value(QStringLiteral("trashOption")).toString(),
                 QStringLiteral("vault"));
        QCOMPARE(onDisk(cfg, QStringLiteral("appearance.json"))
                     .value(QStringLiteral("showRibbon")).toBool(),
                 false);
    }

    void saveConfigMaterialisesBothFiles()
    {
        // Obsidian writes app.json and appearance.json unconditionally
        // (app/own/1129-UD.js:145-147), so appearance.json exists as `{}`
        // at minimum.
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        QVERIFY(cfg.ensureConfigDir());

        QVERIFY(cfg.setConfigValue(QStringLiteral("trashOption"),
                                   QStringLiteral("system")));
        QVERIFY(fs.exists(cfg.configFilePath(QStringLiteral("app.json"))));
        QVERIFY(fs.exists(cfg.configFilePath(QStringLiteral("appearance.json"))));
        QCOMPARE(onDisk(cfg, QStringLiteral("appearance.json")).size(), 0);
    }

    void readConfigMergesBothFilesWithAppJsonWinning()
    {
        // Object.assign({}, appearance, app) — app/own/1129-UD.js:87.
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        QVERIFY(cfg.ensureConfigDir());

        writeRaw(cfg.configFilePath(QStringLiteral("app.json")),
                 R"({"trashOption": "vault", "theme": "from-app"})");
        writeRaw(cfg.configFilePath(QStringLiteral("appearance.json")),
                 R"({"theme": "from-appearance", "translucency": true})");

        const QJsonObject merged = cfg.readConfig();
        QCOMPARE(merged.value(QStringLiteral("theme")).toString(),
                 QStringLiteral("from-app"));
        QCOMPARE(merged.value(QStringLiteral("trashOption")).toString(),
                 QStringLiteral("vault"));
        QCOMPARE(merged.value(QStringLiteral("translucency")).toBool(), true);
    }

    void setConfigValuePreservesUnknownKeysInBothFiles()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        QVERIFY(cfg.ensureConfigDir());

        writeRaw(cfg.configFilePath(QStringLiteral("app.json")),
                 R"({"_pluginA": {"x": 7}, "newLinkFormat": "shortest"})");
        writeRaw(cfg.configFilePath(QStringLiteral("appearance.json")),
                 R"({"_snippetHelper": 1, "enabledCssSnippets": ["wide"]})");

        QVERIFY(cfg.setConfigValue(QStringLiteral("theme"),
                                   QStringLiteral("moonstone")));

        const QJsonObject app = onDisk(cfg, QStringLiteral("app.json"));
        QCOMPARE(app.value(QStringLiteral("_pluginA")).toObject()
                     .value(QStringLiteral("x")).toInt(), 7);
        QCOMPARE(app.value(QStringLiteral("newLinkFormat")).toString(),
                 QStringLiteral("shortest"));
        // `_snippetHelper` is not one of the 16 allow-list keys, so Obsidian
        // relocates it from appearance.json into app.json on the next save.
        // Preserved — not dropped — but not left behind.
        QCOMPARE(app.value(QStringLiteral("_snippetHelper")).toInt(), 1);
        QVERIFY(!onDisk(cfg, QStringLiteral("appearance.json"))
                     .contains(QStringLiteral("_snippetHelper")));

        const QJsonObject appearance =
            onDisk(cfg, QStringLiteral("appearance.json"));
        const QJsonArray snippets =
            appearance.value(QStringLiteral("enabledCssSnippets")).toArray();
        QCOMPARE(snippets.size(), 1);
        QCOMPARE(snippets[0].toString(), QStringLiteral("wide"));
        QCOMPARE(appearance.value(QStringLiteral("theme")).toString(),
                 QStringLiteral("moonstone"));
    }

    void removeConfigKeyDeletesFromWhicheverFileHeldIt()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        QVERIFY(cfg.ensureConfigDir());

        QVERIFY(cfg.setConfigValue(QStringLiteral("trashOption"),
                                   QStringLiteral("vault")));
        QVERIFY(cfg.setConfigValue(QStringLiteral("cssTheme"),
                                   QStringLiteral("Catppuccin")));
        QCOMPARE(onDisk(cfg, QStringLiteral("app.json"))
                     .value(QStringLiteral("trashOption")).toString(),
                 QStringLiteral("vault"));
        QCOMPARE(onDisk(cfg, QStringLiteral("appearance.json"))
                     .value(QStringLiteral("cssTheme")).toString(),
                 QStringLiteral("Catppuccin"));

        QVERIFY(cfg.removeConfigKey(QStringLiteral("trashOption")));
        QVERIFY(!onDisk(cfg, QStringLiteral("app.json"))
                     .contains(QStringLiteral("trashOption")));
        QVERIFY(cfg.removeConfigKey(QStringLiteral("cssTheme")));
        QVERIFY(!onDisk(cfg, QStringLiteral("appearance.json"))
                     .contains(QStringLiteral("cssTheme")));
    }

    void defaultConstructedQJsonValueWritesNullNotDeletion()
    {
        // Guards the trap that makes `setConfigValue(k, QJsonValue())` mean
        // something other than what it reads like: QJsonValue's default ctor
        // is Null (Null == 0x0), and Undefined == 0x80.
        QCOMPARE(int(QJsonValue().type()), int(QJsonValue::Null));
        QCOMPARE(int(QJsonValue(QJsonValue::Undefined).type()),
                 int(QJsonValue::Undefined));

        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        QVERIFY(cfg.ensureConfigDir());

        QVERIFY(cfg.setConfigValue(QStringLiteral("trashOption"),
                                   QStringLiteral("vault")));
        QVERIFY(cfg.setConfigValue(QStringLiteral("trashOption"), QJsonValue()));
        QVERIFY2(cfg.readAppJson()->value(QStringLiteral("trashOption")).isNull(),
                 "a default-constructed QJsonValue writes null");
        QVERIFY(cfg.removeConfigKey(QStringLiteral("trashOption")));
        QVERIFY(!cfg.readAppJson()->contains(QStringLiteral("trashOption")));
    }

    // --- Task 2: value encodings ----------------------------------------

    void openBehaviorFileFormRoundTrips()
    {
        // app/own/2062-uie.js:534 writes "file:" + path; :542 reads it back
        // with startsWith("file:").
        const QString stored = VaultConfig::encodeOpenBehaviorFile(
            QStringLiteral("Daily/2026-10-03.md"));
        QCOMPARE(stored, QStringLiteral("file:Daily/2026-10-03.md"));
        QCOMPARE(VaultConfig::openBehaviorPath(stored),
                 QStringLiteral("Daily/2026-10-03.md"));
        QCOMPARE(VaultConfig::openBehaviorModes(),
                 (QStringList{QString(), QStringLiteral("new"),
                              QStringLiteral("file"), QStringLiteral("daily")}));
    }

    void openBehaviorPathIsEmptyForEveryOtherMode()
    {
        for (const QString &mode : VaultConfig::openBehaviorModes()) {
            if (mode == QLatin1String("file")) continue;
            QVERIFY2(VaultConfig::openBehaviorPath(mode).isEmpty(),
                     qPrintable(mode));
        }
        // Including the audit's four fabricated enum values, none of which is
        // reachable in 1.13.7.
        for (const QString &bogus : {QStringLiteral("default"),
                                     QStringLiteral("tab"),
                                     QStringLiteral("split"),
                                     QStringLiteral("window")}) {
            QVERIFY(VaultConfig::openBehaviorPath(bogus).isEmpty());
        }
        // A bare "file" with no path is not the `file:` form either.
        QVERIFY(VaultConfig::openBehaviorPath(QStringLiteral("file")).isEmpty());
    }

    void openBehaviorAccessorReadsTheMergedMap()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        QVERIFY(cfg.ensureConfigDir());
        QVERIFY(cfg.setConfigValue(QStringLiteral("openBehavior"),
                                   VaultConfig::encodeOpenBehaviorFile(
                                       QStringLiteral("Home.md"))));
        QCOMPARE(cfg.openBehavior(), QStringLiteral("file:Home.md"));
        QCOMPARE(VaultConfig::openBehaviorPath(cfg.openBehavior()),
                 QStringLiteral("Home.md"));
    }

    void openBehaviorAbsentIsEmptyNotDefault()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        QVERIFY(cfg.openBehavior().isEmpty());
    }

    void attachmentFolderPathIsAFourWayDiscriminatedString_data()
    {
        QTest::addColumn<QString>("stored");
        QTest::addColumn<int>("location");
        QTest::addColumn<QString>("value");

        QTest::newRow("root")        << "/"            << int(VaultConfig::AttachmentLocation::Root)          << "";
        QTest::newRow("dot")         << "."            << int(VaultConfig::AttachmentLocation::CurrentFolder) << "";
        QTest::newRow("dotslash")    << "./"           << int(VaultConfig::AttachmentLocation::CurrentFolder) << "";
        QTest::newRow("subfolder")   << "./assets/img" << int(VaultConfig::AttachmentLocation::Subfolder)     << "assets/img";
        QTest::newRow("folder")      << "Attachments"  << int(VaultConfig::AttachmentLocation::Folder)        << "Attachments";
        QTest::newRow("nested-folder") << "a/b/c"      << int(VaultConfig::AttachmentLocation::Folder)        << "a/b/c";
        // The "anything else" arm — Obsidian does not normalise it.
        QTest::newRow("unknown")     << "wat"          << int(VaultConfig::AttachmentLocation::Folder)        << "wat";
    }

    void attachmentFolderPathIsAFourWayDiscriminatedString()
    {
        QFETCH(QString, stored);
        QFETCH(int, location);
        QFETCH(QString, value);

        const auto decoded = VaultConfig::decodeAttachmentFolderPath(stored);
        QCOMPARE(int(decoded.location), location);
        QCOMPARE(decoded.value, value);
    }

    void attachmentFolderPathEncodesBackToTheWireForm_data()
    {
        QTest::addColumn<int>("location");
        QTest::addColumn<QString>("value");
        QTest::addColumn<QString>("expected");

        QTest::newRow("root")      << int(VaultConfig::AttachmentLocation::Root)          << ""            << "/";
        QTest::newRow("current")   << int(VaultConfig::AttachmentLocation::CurrentFolder) << ""            << "./";
        QTest::newRow("subfolder") << int(VaultConfig::AttachmentLocation::Subfolder)     << "assets/img"  << "./assets/img";
        QTest::newRow("folder")    << int(VaultConfig::AttachmentLocation::Folder)        << "Attachments" << "Attachments";
        // Obsidian substitutes a localised placeholder for an empty field
        // (`o.getValue() || c`, app/own/2062-uie.js:598).
        QTest::newRow("folder-empty-uses-fallback")
            << int(VaultConfig::AttachmentLocation::Folder) << ""
            << "attachments";
        QTest::newRow("subfolder-empty-uses-fallback")
            << int(VaultConfig::AttachmentLocation::Subfolder) << ""
            << "./attachments";
    }

    void attachmentFolderPathEncodesBackToTheWireForm()
    {
        QFETCH(int, location);
        QFETCH(QString, value);
        QFETCH(QString, expected);

        VaultConfig::AttachmentPath in;
        in.location = VaultConfig::AttachmentLocation(location);
        in.value = value;
        QCOMPARE(VaultConfig::encodeAttachmentFolderPath(in,
                                                          QStringLiteral("attachments")),
                 expected);
    }

    void attachmentFolderPathAccessorRoundTripsThroughDisk()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        QVERIFY(cfg.ensureConfigDir());

        VaultConfig::AttachmentPath in;
        in.location = VaultConfig::AttachmentLocation::Subfolder;
        in.value = QStringLiteral("images");
        QVERIFY(cfg.setConfigValue(QStringLiteral("attachmentFolderPath"),
                                   VaultConfig::encodeAttachmentFolderPath(in)));

        // app.json is the right file for this key.
        QCOMPARE(onDisk(cfg, QStringLiteral("app.json"))
                     .value(QStringLiteral("attachmentFolderPath")).toString(),
                 QStringLiteral("./images"));
        const auto back = cfg.attachmentFolderPath();
        QCOMPARE(back.location, VaultConfig::AttachmentLocation::Subfolder);
        QCOMPARE(back.value, QStringLiteral("images"));
    }

    void absentAttachmentFolderPathFallsBackToTheVaultRoot()
    {
        // Obsidian's getConfig falls back to ME.attachmentFolderPath === "/",
        // i.e. the vault root — not "." (app/own/0920-eE.js:1204).
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        QCOMPARE(cfg.attachmentFolderPath().location,
                 VaultConfig::AttachmentLocation::Root);
    }

    void fontKeysAreCommaJoinedStringsNotArrays_data()
    {
        QTest::addColumn<QString>("stored");
        QTest::addColumn<QStringList>("expected");

        QTest::newRow("empty")      << ""                       << QStringList{};
        QTest::newRow("single")     << "Inter"                  << QStringList{QStringLiteral("Inter")};
        QTest::newRow("two")        << "Inter,Roboto Mono"      << (QStringList{QStringLiteral("Inter"),
                                                                          QStringLiteral("Roboto Mono")});
        QTest::newRow("spaces")     << " Inter ,  Roboto Mono " << (QStringList{QStringLiteral("Inter"),
                                                                          QStringLiteral("Roboto Mono")});
        // Every `"` is stripped on read (app/own/2049-zne.js:3), including
        // the space that the quote was protecting.
        QTest::newRow("quoted")     << "\"Fira Sans\", monospace"
                                    << (QStringList{QStringLiteral("Fira Sans"),
                                                    QStringLiteral("monospace")});
        QTest::newRow("doubles")    << "\"\"\"A\"\""            << QStringList{QStringLiteral("A")};
        QTest::newRow("empty-parts") << "A,,  ,B"              << (QStringList{QStringLiteral("A"),
                                                                          QStringLiteral("B")});
        // Quoted family containing a comma cannot round-trip — Obsidian's
        // read side strips the quotes, splits on the comma, then trims each
        // part (app/own/2049-zne.js:3-5), so the space is lost too.
        QTest::newRow("comma-in-name")
            << "\"Noto Sans CJK JP, Extra\"" << QStringList{QStringLiteral("Noto Sans CJK JP"),
                                                            QStringLiteral("Extra")};
    }

    void fontKeysAreCommaJoinedStringsNotArrays()
    {
        QFETCH(QString, stored);
        QFETCH(QStringList, expected);
        QCOMPARE(VaultConfig::decodeFontFamilyStack(stored), expected);
    }

    void fontStackEncodesWithABareJoin()
    {
        QCOMPARE(VaultConfig::encodeFontFamilyStack(
                     {QStringLiteral("Inter"), QStringLiteral("Roboto Mono")}),
                 QStringLiteral("Inter,Roboto Mono"));
        QCOMPARE(VaultConfig::encodeFontFamilyStack({}), QString());
    }

    void fontStackRoundTripsThroughAppearanceJson()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        QVERIFY(cfg.ensureConfigDir());

        const QStringList families{QStringLiteral("Inter"),
                                   QStringLiteral("JetBrains Mono")};
        QVERIFY(cfg.setConfigValue(
            QStringLiteral("monospaceFontFamily"),
            VaultConfig::encodeFontFamilyStack(families)));

        const QJsonObject appearance =
            onDisk(cfg, QStringLiteral("appearance.json"));
        QCOMPARE(appearance.value(QStringLiteral("monospaceFontFamily")).toString(),
                 QStringLiteral("Inter,JetBrains Mono"));
        QVERIFY(!appearance.value(QStringLiteral("monospaceFontFamily")).isArray());
        QVERIFY(!onDisk(cfg, QStringLiteral("app.json"))
                     .contains(QStringLiteral("monospaceFontFamily")));
        QCOMPARE(cfg.fontFamilyStack(QStringLiteral("monospaceFontFamily")),
                 families);
    }

    void nativeMenusIsTriState()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        QVERIFY(cfg.ensureConfigDir());

        QVERIFY(!cfg.nativeMenus().has_value()); // absent = follow the OS

        QVERIFY(cfg.setNativeMenus(true));
        QCOMPARE(cfg.nativeMenus().value_or(false), true);
        QCOMPARE(onDisk(cfg, QStringLiteral("appearance.json"))
                     .value(QStringLiteral("nativeMenus")).toBool(), true);

        QVERIFY(cfg.setNativeMenus(false));
        QCOMPARE(cfg.nativeMenus().value_or(true), false);

        // nullopt persists an explicit JSON `null`, matching the Obsidian
        // default (app/own/0920-eE.js:1203) rather than deleting the key.
        QVERIFY(cfg.setNativeMenus(std::nullopt));
        QVERIFY(!cfg.nativeMenus().has_value());
        const QJsonValue v = onDisk(cfg, QStringLiteral("appearance.json"))
                                 .value(QStringLiteral("nativeMenus"));
        QVERIFY(v.isNull());
    }

    void nativeMenusNonBooleanDecodesAsFollowOs()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        QVERIFY(cfg.ensureConfigDir());
        writeRaw(cfg.configFilePath(QStringLiteral("appearance.json")),
                 R"({"nativeMenus": "yes"})");
        QVERIFY(!cfg.nativeMenus().has_value());
    }

    // --- Task 3: `.obsidian/<corePluginId>.json` -------------------------

    void corePluginDataLandsBesideCorePluginsJson()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        QVERIFY(cfg.ensureConfigDir());

        QJsonObject graph;
        graph.insert(QStringLiteral("collapse-filter"), false);
        graph.insert(QStringLiteral("search"), QStringLiteral("-line:(toc)"));
        graph.insert(QStringLiteral("showTags"), true);
        QVERIFY(cfg.writeCorePluginConfig(QStringLiteral("graph"), graph));

        const QString path =
            cfg.configFilePath(QStringLiteral("graph.json"));
        QVERIFY2(fs.exists(path), qPrintable(path));
        QVERIFY2(!fs.exists(cfg.configFilePath(QStringLiteral("plugins/graph/data.json"))),
                 "core-plugin data must not use the community-plugin path");

        const auto loaded = cfg.readCorePluginConfig(QStringLiteral("graph"));
        QVERIFY(loaded.has_value());
        QCOMPARE(loaded->value(QStringLiteral("collapse-filter")).toBool(), false);
        QCOMPARE(loaded->value(QStringLiteral("search")).toString(),
                 QStringLiteral("-line:(toc)"));
        QCOMPARE(loaded->value(QStringLiteral("showTags")).toBool(), true);
    }

    void corePluginDataIsDistinctPerPluginAndFromCorePluginsJson()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        QVERIFY(cfg.ensureConfigDir());

        QVERIFY(cfg.writeCorePluginConfig(QStringLiteral("workspaces"),
                                          QJsonObject{{QStringLiteral("last"), true}}));
        QVERIFY(cfg.writeCorePluginConfig(QStringLiteral("backlink"),
                                          QJsonObject{{QStringLiteral("last"), false}}));
        VaultConfig::CorePlugins enabled;
        enabled.raw.insert(QStringLiteral("graph"), true);
        QVERIFY(cfg.writeCorePlugins(enabled));

        QCOMPARE(cfg.readCorePluginConfig(QStringLiteral("workspaces"))
                     ->value(QStringLiteral("last")).toBool(), true);
        QCOMPARE(cfg.readCorePluginConfig(QStringLiteral("backlink"))
                     ->value(QStringLiteral("last")).toBool(), false);
        QCOMPARE(cfg.readCorePlugins()->raw.value(QStringLiteral("graph")).toBool(),
                 true);
    }

    void corePluginConfigMissingIsNullopt()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        QVERIFY(!cfg.readCorePluginConfig(QStringLiteral("outline"))
                     .has_value());
    }

    void corePluginConfigRejectsPathTraversal()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        QVERIFY(cfg.ensureConfigDir());

        for (const QString &bad : {QString(), QStringLiteral(".."),
                                    QStringLiteral("../../evil"),
                                    QStringLiteral("plugins/x"),
                                    QStringLiteral("a\\b")}) {
            QVERIFY2(!cfg.readCorePluginConfig(bad).has_value(), qPrintable(bad));
            QVERIFY2(!cfg.writeCorePluginConfig(bad, QJsonObject{}),
                     qPrintable(bad));
            QVERIFY2(!cfg.deleteCorePluginConfig(bad), qPrintable(bad));
        }
    }

    void corePluginConfigDeletes()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        QVERIFY(cfg.ensureConfigDir());
        QVERIFY(cfg.writeCorePluginConfig(QStringLiteral("random-note"),
                                          QJsonObject{{QStringLiteral("x"), 1}}));
        QVERIFY(fs.exists(cfg.configFilePath(QStringLiteral("random-note.json"))));
        QVERIFY(cfg.deleteCorePluginConfig(QStringLiteral("random-note")));
        QVERIFY(!fs.exists(cfg.configFilePath(QStringLiteral("random-note.json"))));
    }

    void corePluginConfigIsWrittenInObsidianWireFormat()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        QVERIFY(cfg.ensureConfigDir());
        QVERIFY(cfg.writeCorePluginConfig(
            QStringLiteral("daily-notes"),
            QJsonObject{{QStringLiteral("format"), QStringLiteral("YYYY-MM-DD")}}));

        QFile f(cfg.configFilePath(QStringLiteral("daily-notes.json")));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray bytes = f.readAll();
        QCOMPARE(bytes,
                 QByteArray("{\n  \"format\": \"YYYY-MM-DD\"\n}"));
    }

    // --- Task 4: userIgnoreFilters encoding ------------------------------

    void userIgnoreFiltersWriteNormalisesEmptyToNull()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        QVERIFY(cfg.ensureConfigDir());

        QVERIFY(cfg.setUserIgnoreFilters({QStringLiteral("/^tmp/")}));
        QVERIFY(cfg.readAppJson()->value(QStringLiteral("userIgnoreFilters"))
                     .isArray());

        QVERIFY(cfg.setUserIgnoreFilters({}));
        QVERIFY(cfg.userIgnoreFilters().isEmpty());
        // `null`, not `[]` — app/own/2062-uie.js:738. `[]` would be a
        // distinguishable on-disk value Obsidian never writes.
        QVERIFY2(cfg.readAppJson()->value(QStringLiteral("userIgnoreFilters"))
                     .isNull(),
                 "an emptied ignore-filter list must persist as null, not []");
    }

    void userIgnoreFiltersLiveInAppJson()
    {
        QTemporaryDir tmp;
        FileSystemAdapter fs;
        VaultConfig cfg(&fs, tmp.path());
        QVERIFY(cfg.ensureConfigDir());
        QVERIFY(cfg.setUserIgnoreFilters({QStringLiteral("/^tmp/")}));
        QVERIFY(cfg.readAppJson()->contains(QStringLiteral("userIgnoreFilters")));
        QVERIFY(!onDisk(cfg, QStringLiteral("appearance.json"))
                     .contains(QStringLiteral("userIgnoreFilters")));
    }
};

QTEST_APPLESS_MAIN(TestVaultConfigPartition)
#include "tst_vaultconfig_partition.moc"