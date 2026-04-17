// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include "corbomite/models/DailyNoteService.h"
#include "corbomite/models/TemplateService.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/vault/FileManager.h"
#include "corbomite/core/MomentFormatter.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/VaultConfig.h"
#include "corbomite/vault/Vault.h"

class TestDailyNoteService : public QObject {
    Q_OBJECT

    void createFile(const QString &path, const QString &content)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write(content.toUtf8());
        f.close();
    }

    void writeJsonFile(const QString &path, const QJsonObject &obj)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
        f.close();
    }

private Q_SLOTS:
    void testTodayNotePath()
    {
        QTemporaryDir tmp;
        QDir().mkpath(tmp.path() + "/vault");
        Corbomite::FileSystemAdapter vaultFs;
        Corbomite::Vault vaultObj(&vaultFs);
        vaultObj.load(tmp.path() + "/vault");
        Corbomite::FileManager fileManager(&vaultObj, nullptr);
        Corbomite::TemplateService templateService(&vaultObj);

        Corbomite::DailyNoteService daily(&vaultObj, &fileManager, &templateService);
        daily.setDateFormat(QStringLiteral("YYYY-MM-DD"));
        daily.setFolder(QStringLiteral("Daily Notes"));

        const QString expected = QStringLiteral("Daily Notes/")
            + Corbomite::MomentFormatter::format(
                QDateTime::currentDateTime(), QStringLiteral("YYYY-MM-DD"))
            + QStringLiteral(".md");
        QCOMPARE(daily.todayNotePath(), expected);
    }

    void testTodayPathUsesMomentFormat()
    {
        QTemporaryDir tmp;
        QDir().mkpath(tmp.path() + "/vault");
        Corbomite::FileSystemAdapter vaultFs;
        Corbomite::Vault vaultObj(&vaultFs);
        vaultObj.load(tmp.path() + "/vault");
        Corbomite::FileManager fileManager(&vaultObj, nullptr);
        Corbomite::TemplateService templateService(&vaultObj);

        Corbomite::DailyNoteService daily(&vaultObj, &fileManager, &templateService);
        daily.setDateFormat(QStringLiteral("YYYY-MM-DD"));
        daily.setFolder(QString());  // bare file at vault root

        const QString today = Corbomite::MomentFormatter::format(
            QDateTime::currentDateTime(), QStringLiteral("YYYY-MM-DD"));
        QCOMPARE(daily.todayNotePath(), today + QStringLiteral(".md"));
    }

    void testNestedFolderFormatComputesCorrectPath()
    {
        QTemporaryDir tmp;
        QDir().mkpath(tmp.path() + "/vault");
        Corbomite::FileSystemAdapter vaultFs;
        Corbomite::Vault vaultObj(&vaultFs);
        vaultObj.load(tmp.path() + "/vault");
        Corbomite::FileManager fileManager(&vaultObj, nullptr);
        Corbomite::TemplateService templateService(&vaultObj);

        Corbomite::DailyNoteService daily(&vaultObj, &fileManager, &templateService);
        daily.setDateFormat(QStringLiteral("YYYY/MMMM/YYYY-MM-DD"));
        daily.setFolder(QStringLiteral("Daily"));

        const QString datePart = Corbomite::MomentFormatter::format(
            QDateTime::currentDateTime(), QStringLiteral("YYYY/MMMM/YYYY-MM-DD"));
        const QString expected = QStringLiteral("Daily/") + datePart + QStringLiteral(".md");
        QCOMPARE(daily.todayNotePath(), expected);
    }

    void testNestedFolderFormatAutoCreatesDirectories()
    {
        QTemporaryDir tmp;
        QDir().mkpath(tmp.path() + "/vault");
        Corbomite::FileSystemAdapter vaultFs;
        Corbomite::Vault vaultObj(&vaultFs);
        vaultObj.load(tmp.path() + "/vault");
        Corbomite::FileManager fileManager(&vaultObj, nullptr);
        Corbomite::TemplateService templateService(&vaultObj);

        Corbomite::DailyNoteService daily(&vaultObj, &fileManager, &templateService);
        daily.setDateFormat(QStringLiteral("YYYY/MMMM/YYYY-MM-DD"));
        daily.setFolder(QStringLiteral("Daily"));

        auto *doc = daily.openOrCreateToday();
        QVERIFY(doc != nullptr);

        // Verify the absolute path of today's file exists and all its
        // intermediate directories were created.
        const QString absPath = tmp.path() + "/vault/" + daily.todayNotePath();
        QVERIFY2(QFileInfo::exists(absPath), qPrintable(absPath));

        const QFileInfo fi(absPath);
        QVERIFY(fi.dir().exists());

        // Year / month-name directories should both be on disk.
        const QDateTime now = QDateTime::currentDateTime();
        const QString yearDir = tmp.path() + "/vault/Daily/"
            + Corbomite::MomentFormatter::format(now, QStringLiteral("YYYY"));
        const QString monthDir = yearDir + QLatin1Char('/')
            + Corbomite::MomentFormatter::format(now, QStringLiteral("MMMM"));
        QVERIFY2(QFileInfo(yearDir).isDir(), qPrintable(yearDir));
        QVERIFY2(QFileInfo(monthDir).isDir(), qPrintable(monthDir));
    }

    void testTodayNoteNotExists()
    {
        QTemporaryDir tmp;
        QDir().mkpath(tmp.path() + "/vault");
        Corbomite::FileSystemAdapter vaultFs;
        Corbomite::Vault vaultObj(&vaultFs);
        vaultObj.load(tmp.path() + "/vault");
        Corbomite::FileManager fileManager(&vaultObj, nullptr);
        Corbomite::TemplateService templateService(&vaultObj);

        Corbomite::DailyNoteService daily(&vaultObj, &fileManager, &templateService);

        QVERIFY(!daily.todayNoteExists());
    }

    void testOpenOrCreateToday()
    {
        QTemporaryDir tmp;
        QDir().mkpath(tmp.path() + "/vault");
        Corbomite::FileSystemAdapter vaultFs;
        Corbomite::Vault vaultObj(&vaultFs);
        vaultObj.load(tmp.path() + "/vault");
        Corbomite::FileManager fileManager(&vaultObj, nullptr);
        Corbomite::TemplateService templateService(&vaultObj);

        Corbomite::DailyNoteService daily(&vaultObj, &fileManager, &templateService);
        daily.setFolder(QStringLiteral("Daily Notes"));

        auto *doc = daily.openOrCreateToday();
        QVERIFY(doc != nullptr);
        QVERIFY(daily.todayNoteExists());

        // File should exist on disk
        QString absPath = tmp.path() + "/vault/" + daily.todayNotePath();
        QVERIFY(QFileInfo::exists(absPath));
    }

    void testOpenOrCreateWithTemplate()
    {
        QTemporaryDir tmp;
        createFile(tmp.path() + "/vault/Templates/Daily.md",
                   "# {{title}}\n\nDate: {{date}}\n\n## Tasks\n\n- [ ] ");

        Corbomite::FileSystemAdapter vaultFs;
        Corbomite::Vault vaultObj(&vaultFs);
        vaultObj.load(tmp.path() + "/vault");
        Corbomite::FileManager fileManager(&vaultObj, nullptr);
        Corbomite::TemplateService templateService(&vaultObj);
        templateService.setTemplateFolder(QStringLiteral("Templates"));

        Corbomite::DailyNoteService daily(&vaultObj, &fileManager, &templateService);
        daily.setFolder(QStringLiteral("Daily Notes"));
        daily.setTemplateName(QStringLiteral("Daily"));

        auto *doc = daily.openOrCreateToday();
        QVERIFY(doc != nullptr);

        // Content should have expanded template
        QString content = doc->markdown();
        QVERIFY(content.contains(QStringLiteral("## Tasks")));
        QVERIFY(!content.contains(QStringLiteral("{{title}}")));
        QVERIFY(!content.contains(QStringLiteral("{{date}}")));
    }

    void testOpenExistingDoesNotModify()
    {
        QTemporaryDir tmp;
        Corbomite::FileSystemAdapter vaultFs;
        Corbomite::Vault vaultObj(&vaultFs);
        vaultObj.load(tmp.path() + "/vault");
        Corbomite::FileManager fileManager(&vaultObj, nullptr);
        Corbomite::TemplateService templateService(&vaultObj);

        Corbomite::DailyNoteService daily(&vaultObj, &fileManager, &templateService);
        daily.setFolder(QStringLiteral("Daily Notes"));

        // Create today's note first
        auto *doc1 = daily.openOrCreateToday();
        QVERIFY(doc1);
        doc1->setMarkdown(QStringLiteral("My custom content"));
        vaultObj.saveDocument(doc1);

        // Open again — should return same doc, not overwrite
        auto *doc2 = daily.openOrCreateToday();
        QCOMPARE(doc2, doc1);
        QCOMPARE(doc2->markdown(), QStringLiteral("My custom content"));
    }

    void testInitFromVaultConfigReadsDailyNotesJson()
    {
        QTemporaryDir tmp;
        QDir().mkpath(tmp.path() + "/vault");

        QJsonObject obj;
        obj.insert(QStringLiteral("format"), QStringLiteral("YYYY"));
        obj.insert(QStringLiteral("folder"), QStringLiteral("d"));
        obj.insert(QStringLiteral("template"), QStringLiteral("t.md"));
        writeJsonFile(tmp.path() + "/vault/.obsidian/daily-notes.json", obj);

        Corbomite::FileSystemAdapter vaultFs;
        Corbomite::Vault vaultObj(&vaultFs);
        vaultObj.load(tmp.path() + "/vault");
        Corbomite::FileManager fileManager(&vaultObj, nullptr);
        Corbomite::TemplateService templateService(&vaultObj);

        Corbomite::DailyNoteService daily(&vaultObj, &fileManager, &templateService);

        Corbomite::FileSystemAdapter fs;
        Corbomite::VaultConfig cfg(&fs, tmp.path() + "/vault");
        daily.initFromVaultConfig(cfg);

        const QString datePart = Corbomite::MomentFormatter::format(
            QDateTime::currentDateTime(), QStringLiteral("YYYY"));
        QCOMPARE(daily.todayNotePath(),
                 QStringLiteral("d/") + datePart + QStringLiteral(".md"));
    }

    void testInitFromVaultConfigFallsBackWhenJsonMissing()
    {
        QTemporaryDir tmp;
        QDir().mkpath(tmp.path() + "/vault");
        // Note: no .obsidian/daily-notes.json file.

        Corbomite::FileSystemAdapter vaultFs;
        Corbomite::Vault vaultObj(&vaultFs);
        vaultObj.load(tmp.path() + "/vault");
        Corbomite::FileManager fileManager(&vaultObj, nullptr);
        Corbomite::TemplateService templateService(&vaultObj);

        Corbomite::DailyNoteService daily(&vaultObj, &fileManager, &templateService);
        daily.setDateFormat(QStringLiteral("YYYY-MM-DD"));
        daily.setFolder(QStringLiteral("Preseeded"));
        daily.setTemplateName(QStringLiteral("PreseededTpl"));

        Corbomite::FileSystemAdapter fs;
        Corbomite::VaultConfig cfg(&fs, tmp.path() + "/vault");
        daily.initFromVaultConfig(cfg);

        // Pre-seeded values still in effect.
        const QString datePart = Corbomite::MomentFormatter::format(
            QDateTime::currentDateTime(), QStringLiteral("YYYY-MM-DD"));
        QCOMPARE(daily.todayNotePath(),
                 QStringLiteral("Preseeded/") + datePart + QStringLiteral(".md"));
    }

    void testInitFromVaultConfigPartialKeysOnlyUpdatePresent()
    {
        QTemporaryDir tmp;
        QDir().mkpath(tmp.path() + "/vault");

        QJsonObject obj;
        obj.insert(QStringLiteral("folder"), QStringLiteral("d"));
        writeJsonFile(tmp.path() + "/vault/.obsidian/daily-notes.json", obj);

        Corbomite::FileSystemAdapter vaultFs;
        Corbomite::Vault vaultObj(&vaultFs);
        vaultObj.load(tmp.path() + "/vault");
        Corbomite::FileManager fileManager(&vaultObj, nullptr);
        Corbomite::TemplateService templateService(&vaultObj);

        Corbomite::DailyNoteService daily(&vaultObj, &fileManager, &templateService);
        daily.setDateFormat(QStringLiteral("YYYY-MM-DD"));
        daily.setTemplateName(QStringLiteral("MyTpl"));

        Corbomite::FileSystemAdapter fs;
        Corbomite::VaultConfig cfg(&fs, tmp.path() + "/vault");
        daily.initFromVaultConfig(cfg);

        // folder updated from JSON; format + template unchanged.
        const QString datePart = Corbomite::MomentFormatter::format(
            QDateTime::currentDateTime(), QStringLiteral("YYYY-MM-DD"));
        QCOMPARE(daily.todayNotePath(),
                 QStringLiteral("d/") + datePart + QStringLiteral(".md"));
        // Template is not directly observable via getter in the API,
        // but we can assert by watching for its effect: create a note
        // with a known template and verify expansion ran.
    }

    void testOpenOrCreateTodayAppliesTemplateFromVault()
    {
        QTemporaryDir tmp;
        createFile(tmp.path() + "/vault/Templates/Daily.md",
                   "# {{title}}\n\nContent body.");

        QJsonObject obj;
        obj.insert(QStringLiteral("format"), QStringLiteral("YYYY-MM-DD"));
        obj.insert(QStringLiteral("folder"), QStringLiteral("Daily Notes"));
        obj.insert(QStringLiteral("template"), QStringLiteral("Daily"));
        writeJsonFile(tmp.path() + "/vault/.obsidian/daily-notes.json", obj);

        Corbomite::FileSystemAdapter vaultFs;
        Corbomite::Vault vaultObj(&vaultFs);
        vaultObj.load(tmp.path() + "/vault");
        Corbomite::FileManager fileManager(&vaultObj, nullptr);
        Corbomite::TemplateService templateService(&vaultObj);
        templateService.setTemplateFolder(QStringLiteral("Templates"));

        Corbomite::DailyNoteService daily(&vaultObj, &fileManager, &templateService);

        Corbomite::FileSystemAdapter fs;
        Corbomite::VaultConfig cfg(&fs, tmp.path() + "/vault");
        daily.initFromVaultConfig(cfg);

        auto *doc = daily.openOrCreateToday();
        QVERIFY(doc != nullptr);

        const QString content = doc->markdown();
        QVERIFY(content.contains(QStringLiteral("Content body.")));
        QVERIFY(!content.contains(QStringLiteral("{{title}}")));
    }
};

QTEST_MAIN(TestDailyNoteService)
#include "tst_dailynoteservice.moc"
