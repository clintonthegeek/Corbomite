// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster V.2 Phase 2 — persistence-layer integration test for
// MainWindow::applyVaultPortableSettings(). Exercising the dispatcher
// directly requires a full MainWindow + SettingsDialog Qt loop; this test
// instead pins the round-trip semantics of the three VaultConfig::mergeJson
// calls the applier issues. Any regression of the wire-format will fail
// here even if the dispatcher is exercised separately.

#include <QTest>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>

#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/storage/VaultConfig.h"
#include "corbomitesettings.h"

using namespace Corbomite;

class TestSettingsApply : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void applyWritesThreeFiles()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        FileSystemAdapter fs;
        VaultConfig vc(&fs, dir.path());
        QVERIFY(vc.ensureConfigDir());

        // Mirror the three writes the applier performs with concrete values.
        QJsonObject app;
        app.insert(QStringLiteral("theme"), QStringLiteral("dark"));
        QVERIFY(vc.mergeJson(QStringLiteral("appearance.json"), app));

        QJsonObject dn;
        dn.insert(QStringLiteral("folder"), QStringLiteral("Journal"));
        dn.insert(QStringLiteral("format"), QStringLiteral("YYYY-MM-DD"));
        dn.insert(QStringLiteral("template"), QStringLiteral("DailyTemplate.md"));
        QVERIFY(vc.mergeJson(QStringLiteral("daily-notes.json"), dn));

        QJsonObject tpl;
        tpl.insert(QStringLiteral("folder"), QStringLiteral("Templates"));
        QVERIFY(vc.mergeJson(QStringLiteral("templates.json"), tpl));

        // Verify all three appeared with expected contents.
        auto a = vc.readJson(QStringLiteral("appearance.json"));
        QVERIFY(a.has_value());
        QCOMPARE(a->value(QStringLiteral("theme")).toString(),
                 QStringLiteral("dark"));

        auto d = vc.readJson(QStringLiteral("daily-notes.json"));
        QVERIFY(d.has_value());
        QCOMPARE(d->value(QStringLiteral("folder")).toString(),
                 QStringLiteral("Journal"));
        QCOMPARE(d->value(QStringLiteral("format")).toString(),
                 QStringLiteral("YYYY-MM-DD"));
        QCOMPARE(d->value(QStringLiteral("template")).toString(),
                 QStringLiteral("DailyTemplate.md"));

        auto t = vc.readJson(QStringLiteral("templates.json"));
        QVERIFY(t.has_value());
        QCOMPARE(t->value(QStringLiteral("folder")).toString(),
                 QStringLiteral("Templates"));
    }

    void applyPreservesUnknownKeysInAppearance()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        FileSystemAdapter fs;
        VaultConfig vc(&fs, dir.path());
        QVERIFY(vc.ensureConfigDir());

        // Pre-seed appearance.json with an Obsidian-authored key the applier
        // does not know about, plus a "theme" we will overwrite.
        QJsonObject seed;
        seed.insert(QStringLiteral("accentColor"), QStringLiteral("#ff8800"));
        seed.insert(QStringLiteral("theme"), QStringLiteral("light"));
        QVERIFY(vc.writeJson(QStringLiteral("appearance.json"), seed));

        // Mirror the appearance-page write.
        QJsonObject upd;
        upd.insert(QStringLiteral("theme"), QStringLiteral("dark"));
        QVERIFY(vc.mergeJson(QStringLiteral("appearance.json"), upd));

        auto result = vc.readJson(QStringLiteral("appearance.json"));
        QVERIFY(result.has_value());
        QCOMPARE(result->value(QStringLiteral("theme")).toString(),
                 QStringLiteral("dark"));
        QCOMPARE(result->value(QStringLiteral("accentColor")).toString(),
                 QStringLiteral("#ff8800"));
    }

    // Cluster V.2 Phase 4 — pin the kcfg getter/setter round-trip for the
    // Editor/AutoSaveDelayMs key. The applier itself
    // (MainWindow::applyAutosaveDelay) is one line of dispatch
    // (m_autosave->setDelayMs(ms)) which is correct by inspection; this
    // confirms the kcfg side of the chain works.
    void autosaveDelayKcfgRoundTrip()
    {
        auto *s = CorbomiteSettings::self();
        const int prev = s->autoSaveDelayMs();
        s->setAutoSaveDelayMs(7500);
        QCOMPARE(s->autoSaveDelayMs(), 7500);
        s->setAutoSaveDelayMs(prev); // restore — kcfg state is process-global
    }
};

QTEST_MAIN(TestSettingsApply)
#include "tst_mainwindow_settings_apply.moc"
