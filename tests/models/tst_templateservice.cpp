// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QDate>
#include <QTime>
#include "corbomite/models/TemplateService.h"
#include "corbomite/models/VaultModel.h"

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
        QString expected = QStringLiteral("Date: ") + QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
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
            QStringLiteral("{{date:dd/MM/yyyy}}"), QStringLiteral("Note"));
        QString expected = QDate::currentDate().toString(QStringLiteral("dd/MM/yyyy"));
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
};

QTEST_MAIN(TestTemplateService)
#include "tst_templateservice.moc"
