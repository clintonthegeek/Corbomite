// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QDate>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTime>
#include "corbomite/core/MomentFormatter.h"
#include "corbomite/models/TemplateService.h"
#include "corbomite/models/VaultModel.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/VaultConfig.h"

class TestTemplateService : public QObject {
    Q_OBJECT

    void createFile(const QString &path, const QString &content)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write(content.toUtf8());
        f.close();
    }

private Q_SLOTS:
    void testExpandTitle()
    {
        Corbomite::TemplateService service(nullptr);
        QString result = service.expandTemplate(
            QStringLiteral("# {{title}}\n\nContent"), QStringLiteral("My Note"));
        QCOMPARE(result, QStringLiteral("# My Note\n\nContent"));
    }

    void testExpandDate()
    {
        Corbomite::TemplateService service(nullptr);
        QString result = service.expandTemplate(
            QStringLiteral("Date: {{date}}"), QStringLiteral("Note"));
        // Default date format is Moment tokens "YYYY-MM-DD".
        QString expected = QStringLiteral("Date: ") +
            Corbomite::MomentFormatter::format(QDateTime::currentDateTime(),
                                               QStringLiteral("YYYY-MM-DD"));
        QCOMPARE(result, expected);
    }

    void testExpandTime()
    {
        Corbomite::TemplateService service(nullptr);
        QString result = service.expandTemplate(
            QStringLiteral("Time: {{time}}"), QStringLiteral("Note"));
        // Time changes — just verify it doesn't contain {{time}} anymore
        QVERIFY(!result.contains(QStringLiteral("{{time}}")));
        QVERIFY(result.startsWith(QStringLiteral("Time: ")));
    }

    void testExpandCustomDateFormat()
    {
        Corbomite::TemplateService service(nullptr);
        QString result = service.expandTemplate(
            QStringLiteral("{{date:DD/MM/YYYY}}"), QStringLiteral("Note"));
        QString expected = Corbomite::MomentFormatter::format(
            QDateTime::currentDateTime(), QStringLiteral("DD/MM/YYYY"));
        QCOMPARE(result, expected);
    }

    void testExpandCustomTimeFormat()
    {
        Corbomite::TemplateService service(nullptr);
        QString result = service.expandTemplate(
            QStringLiteral("{{time:HH:mm:ss}}"), QStringLiteral("Note"));
        QVERIFY(!result.contains(QStringLiteral("{{time:")));
        QVERIFY(result.contains(QStringLiteral(":")));
    }

    void testExpandMultipleVariables()
    {
        Corbomite::TemplateService service(nullptr);
        QString result = service.expandTemplate(
            QStringLiteral("# {{title}}\nDate: {{date}}\nTime: {{time}}"),
            QStringLiteral("Test"));
        QVERIFY(result.startsWith(QStringLiteral("# Test\n")));
        QVERIFY(!result.contains(QStringLiteral("{{")));
    }

    void testNoVariables()
    {
        Corbomite::TemplateService service(nullptr);
        QString input = QStringLiteral("Just plain text\nNo variables here");
        QString result = service.expandTemplate(input, QStringLiteral("Note"));
        QCOMPARE(result, input);
    }

    void testAvailableTemplates()
    {
        QTemporaryDir tmp;
        createFile(tmp.path() + "/vault/Templates/Daily.md", "# {{title}}");
        createFile(tmp.path() + "/vault/Templates/Meeting.md", "## Meeting Notes");
        createFile(tmp.path() + "/vault/Templates/notamd.txt", "ignored");

        Corbomite::VaultModel vault;
        vault.open(tmp.path() + "/vault");

        Corbomite::TemplateService service(&vault);
        service.setTemplateFolder(QStringLiteral("Templates"));

        auto templates = service.availableTemplates();
        QCOMPARE(templates.size(), 2);
        QVERIFY(templates.contains(QStringLiteral("Daily")));
        QVERIFY(templates.contains(QStringLiteral("Meeting")));
    }

    void testEmptyTemplateFolder()
    {
        QTemporaryDir tmp;
        QDir().mkpath(tmp.path() + "/vault");

        Corbomite::VaultModel vault;
        vault.open(tmp.path() + "/vault");

        Corbomite::TemplateService service(&vault);
        service.setTemplateFolder(QStringLiteral("Templates"));

        QCOMPARE(service.availableTemplates().size(), 0);
    }

    void testLoadAndExpand()
    {
        QTemporaryDir tmp;
        createFile(tmp.path() + "/vault/Templates/Test.md",
                   "# {{title}}\nCreated: {{date}}");

        Corbomite::VaultModel vault;
        vault.open(tmp.path() + "/vault");

        Corbomite::TemplateService service(&vault);
        service.setTemplateFolder(QStringLiteral("Templates"));

        QString result = service.loadAndExpand(QStringLiteral("Test"), QStringLiteral("My Note"));
        QVERIFY(result.startsWith(QStringLiteral("# My Note\n")));
        QVERIFY(!result.contains(QStringLiteral("{{")));
    }

    // --- Phase 3: Moment tokens, {{folder}}, {{cursor}}, vault config ---

    void testDateFormatUsesMomentTokens()
    {
        Corbomite::TemplateService service(nullptr);
        const QString result = service.expandTemplate(
            QStringLiteral("{{date:YYYY-MM-DD}}"), QStringLiteral("t"));
        // Compare to MomentFormatter directly to avoid race-around-midnight.
        const QString expected = Corbomite::MomentFormatter::format(
            QDateTime::currentDateTime(), QStringLiteral("YYYY-MM-DD"));
        QCOMPARE(result, expected);
    }

    void testTimeFormatUsesMomentTokens()
    {
        Corbomite::TemplateService service(nullptr);
        const QString result = service.expandTemplate(
            QStringLiteral("{{time:HH:mm}}"), QStringLiteral("t"));
        const QString expected = Corbomite::MomentFormatter::format(
            QDateTime::currentDateTime(), QStringLiteral("HH:mm"));
        QCOMPARE(result, expected);
    }

    void testFolderPlaceholder()
    {
        Corbomite::TemplateService service(nullptr);
        const QString result = service.expandTemplate(
            QStringLiteral("{{folder}}/X"),
            QStringLiteral("note"),
            QStringLiteral("Daily"));
        QCOMPARE(result, QStringLiteral("Daily/X"));
    }

    void testFolderPlaceholderEmptyWithTwoArgOverload()
    {
        Corbomite::TemplateService service(nullptr);
        const QString result = service.expandTemplate(
            QStringLiteral("{{folder}}/X"), QStringLiteral("note"));
        QCOMPARE(result, QStringLiteral("/X"));
    }

    void testCursorPlaceholderPreserved()
    {
        Corbomite::TemplateService service(nullptr);
        const QString result = service.expandTemplate(
            QStringLiteral("# H\n{{cursor}}\nBody"), QStringLiteral("t"));
        QVERIFY(result.contains(QStringLiteral("{{cursor}}")));
    }

    void testInitFromVaultConfigReadsTemplatesJson()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString vaultRoot = tmp.path();

        // Write .obsidian/templates.json
        QDir().mkpath(vaultRoot + QStringLiteral("/.obsidian"));
        QJsonObject obj;
        obj.insert(QStringLiteral("folder"), QStringLiteral("tpl"));
        obj.insert(QStringLiteral("date_format"), QStringLiteral("YYYY"));
        obj.insert(QStringLiteral("time_format"), QStringLiteral("HH"));
        QFile f(vaultRoot + QStringLiteral("/.obsidian/templates.json"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
        f.close();

        Corbomite::FileSystemAdapter fs;
        Corbomite::VaultConfig vc(&fs, vaultRoot);

        Corbomite::TemplateService service(nullptr);
        service.initFromVaultConfig(vc);

        QCOMPARE(service.templateFolder(), QStringLiteral("tpl"));
        QCOMPARE(service.defaultDateFormat(), QStringLiteral("YYYY"));
        QCOMPARE(service.defaultTimeFormat(), QStringLiteral("HH"));
    }

    void testInitFromVaultConfigFallsBackWhenJsonMissing()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString vaultRoot = tmp.path();
        QDir().mkpath(vaultRoot + QStringLiteral("/.obsidian"));
        // No templates.json file.

        Corbomite::FileSystemAdapter fs;
        Corbomite::VaultConfig vc(&fs, vaultRoot);

        Corbomite::TemplateService service(nullptr);
        service.setTemplateFolder(QStringLiteral("KEEP_FOLDER"));
        service.setDefaultDateFormat(QStringLiteral("KEEP_DATE"));
        service.setDefaultTimeFormat(QStringLiteral("KEEP_TIME"));

        service.initFromVaultConfig(vc);

        QCOMPARE(service.templateFolder(), QStringLiteral("KEEP_FOLDER"));
        QCOMPARE(service.defaultDateFormat(), QStringLiteral("KEEP_DATE"));
        QCOMPARE(service.defaultTimeFormat(), QStringLiteral("KEEP_TIME"));
    }

    void testInitFromVaultConfigPartialKeysOnlyUpdatePresent()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString vaultRoot = tmp.path();
        QDir().mkpath(vaultRoot + QStringLiteral("/.obsidian"));

        QJsonObject obj;
        obj.insert(QStringLiteral("folder"), QStringLiteral("tpl"));
        // No date_format, no time_format.
        QFile f(vaultRoot + QStringLiteral("/.obsidian/templates.json"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
        f.close();

        Corbomite::FileSystemAdapter fs;
        Corbomite::VaultConfig vc(&fs, vaultRoot);

        Corbomite::TemplateService service(nullptr);
        service.setDefaultDateFormat(QStringLiteral("OLD"));
        service.setDefaultTimeFormat(QStringLiteral("OLD_T"));

        service.initFromVaultConfig(vc);

        QCOMPARE(service.templateFolder(), QStringLiteral("tpl"));
        QCOMPARE(service.defaultDateFormat(), QStringLiteral("OLD"));
        QCOMPARE(service.defaultTimeFormat(), QStringLiteral("OLD_T"));
    }
};

QTEST_MAIN(TestTemplateService)
#include "tst_templateservice.moc"
