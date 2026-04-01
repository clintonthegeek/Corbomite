// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QDate>
#include <QDir>
#include <QFile>
#include "corbomite/models/DailyNoteService.h"
#include "corbomite/models/VaultModel.h"
#include "corbomite/models/NoteService.h"
#include "corbomite/models/TemplateService.h"

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

private Q_SLOTS:
    void testTodayNotePath()
    {
        QTemporaryDir tmp;
        QDir().mkpath(tmp.path() + "/vault");
        Corbomite::VaultModel vault;
        vault.open(tmp.path() + "/vault");
        Corbomite::NoteService noteService(&vault);
        Corbomite::TemplateService templateService(&vault);

        Corbomite::DailyNoteService daily(&vault, &noteService, &templateService);
        daily.setDateFormat(QStringLiteral("yyyy-MM-dd"));
        daily.setFolder(QStringLiteral("Daily Notes"));

        QString expected = QStringLiteral("Daily Notes/")
            + QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"))
            + QStringLiteral(".md");
        QCOMPARE(daily.todayNotePath(), expected);
    }

    void testTodayNoteNotExists()
    {
        QTemporaryDir tmp;
        QDir().mkpath(tmp.path() + "/vault");
        Corbomite::VaultModel vault;
        vault.open(tmp.path() + "/vault");
        Corbomite::NoteService noteService(&vault);
        Corbomite::TemplateService templateService(&vault);

        Corbomite::DailyNoteService daily(&vault, &noteService, &templateService);

        QVERIFY(!daily.todayNoteExists());
    }

    void testOpenOrCreateToday()
    {
        QTemporaryDir tmp;
        QDir().mkpath(tmp.path() + "/vault");
        Corbomite::VaultModel vault;
        vault.open(tmp.path() + "/vault");
        Corbomite::NoteService noteService(&vault);
        Corbomite::TemplateService templateService(&vault);

        Corbomite::DailyNoteService daily(&vault, &noteService, &templateService);
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

        Corbomite::VaultModel vault;
        vault.open(tmp.path() + "/vault");
        Corbomite::NoteService noteService(&vault);
        Corbomite::TemplateService templateService(&vault);
        templateService.setTemplateFolder(QStringLiteral("Templates"));

        Corbomite::DailyNoteService daily(&vault, &noteService, &templateService);
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
        Corbomite::VaultModel vault;
        vault.open(tmp.path() + "/vault");
        Corbomite::NoteService noteService(&vault);
        Corbomite::TemplateService templateService(&vault);

        Corbomite::DailyNoteService daily(&vault, &noteService, &templateService);
        daily.setFolder(QStringLiteral("Daily Notes"));

        // Create today's note first
        auto *doc1 = daily.openOrCreateToday();
        QVERIFY(doc1);
        doc1->setMarkdown(QStringLiteral("My custom content"));
        noteService.saveNote(doc1);

        // Open again — should return same doc, not overwrite
        auto *doc2 = daily.openOrCreateToday();
        QCOMPARE(doc2, doc1);
        QCOMPARE(doc2->markdown(), QStringLiteral("My custom content"));
    }
};

QTEST_MAIN(TestDailyNoteService)
#include "tst_dailynoteservice.moc"
