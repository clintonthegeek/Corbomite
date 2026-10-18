// SPDX-License-Identifier: GPL-3.0-or-later
#include <QCheckBox>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include <KPluginMetaData>

#include "../KilnView.h"
#include "fakerunner.h"

#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/vault/PluginContext.h"
#include "corbomite/vault/Vault.h"

using namespace Corbomite;

/// The panel's flow, over a runner that answers instantly.
///
/// The panel is constructed directly rather than through `KilnPlugin::createView`,
/// because the plugin builds its runner from the `process` permission and a test that
/// went through it would be spawning real subprocesses to find out what a string says.
class TestKilnView : public QObject
{
    Q_OBJECT
private slots:
    void saysWhatToDoWhenNothingIsConfigured();
    void refusesBeforeItSpawnsWhenThereIsNoSiteDirectory();
    void handsTheLiveVaultPathToTheEngine();
    void writesCheckOrProjectAsTheMode();
    void showsTheEngineReport();
    void settingsSurviveTheHostStore();
    void copylessDefaultIsNotRunnable();
    void theScaffoldActionRunsTheScaffoldMode();
    void theTagParentsBoxReflectsAndWritesTheSetting();
};

namespace {

/// A vault plus a plugin context with a data directory, and the ability to seed the
/// stored settings before the panel reads them.
struct Harness
{
    QTemporaryDir dir;
    FileSystemAdapter fs;
    Vault vault{&fs};
    PluginContext ctx;
    QString dataDir;

    Harness()
        : ctx(makeMeta(), {QStringLiteral("vault.read"), QStringLiteral("config"),
                           QStringLiteral("process")})
    {
        vault.load(dir.path());
        ctx.setCoreServices(&vault, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                            nullptr);
        dataDir = dir.path() + QStringLiteral("/.obsidian/plugins/corbomite-kiln");
        QDir().mkpath(dataDir);
        ctx.setPluginDataDir(dataDir);
    }

    static PluginMetaData makeMeta()
    {
        QJsonObject full;
        full.insert(QStringLiteral("KPlugin"),
                    QJsonObject{{QStringLiteral("Id"), QStringLiteral("corbomite-kiln")},
                                {QStringLiteral("Name"), QStringLiteral("Kiln")}});
        return PluginMetaData(KPluginMetaData(full, QStringLiteral("corbomite-kiln")));
    }

    QString dataFile() const { return dataDir + QStringLiteral("/data.json"); }

    void seed(const QJsonObject &settings) const
    {
        QFile file(dataFile());
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(QJsonDocument(settings).toJson());
    }

    QJsonObject stored() const
    {
        QFile file(dataFile());
        if (!file.open(QIODevice::ReadOnly))
            return {};
        return QJsonDocument::fromJson(file.readAll()).object();
    }

    QJsonObject usableSettings(const QString &siteDirectory) const
    {
        QJsonObject projection;
        projection.insert(QStringLiteral("name"), QStringLiteral("vibekoder.ca"));
        projection.insert(QStringLiteral("select"),
                          QJsonObject{{QStringLiteral("folders"), QJsonArray{QStringLiteral("software")}}});
        projection.insert(QStringLiteral("section"), QStringLiteral("software"));

        QJsonObject settings;
        settings.insert(QStringLiteral("output"), siteDirectory);
        settings.insert(QStringLiteral("projections"), QJsonArray{projection});
        return settings;
    }
};

} // namespace

void TestKilnView::saysWhatToDoWhenNothingIsConfigured()
{
    Harness harness;
    FakeKilnRunner runner;
    KilnView view(&harness.ctx, &runner);

    QVERIFY(view.reportText().contains(QStringLiteral("No projections yet")));
    QVERIFY(view.reportText().contains(QStringLiteral("cohort")));
}

void TestKilnView::refusesBeforeItSpawnsWhenThereIsNoSiteDirectory()
{
    // The check happens in the panel, not in the engine: a half-filled projection
    // should be a sentence here, not a process failing somewhere else.
    Harness harness;
    QJsonObject settings = harness.usableSettings(QString());
    settings.remove(QStringLiteral("output"));
    harness.seed(settings);

    FakeKilnRunner runner;
    KilnView view(&harness.ctx, &runner);
    view.runCheck();

    QCOMPARE(runner.calls, 0);
    QVERIFY(view.reportText().contains(QStringLiteral("site directory")));
}

void TestKilnView::handsTheLiveVaultPathToTheEngine()
{
    Harness harness;
    harness.seed(harness.usableSettings(harness.dir.path() + QStringLiteral("/site")));

    FakeKilnRunner runner;
    KilnView view(&harness.ctx, &runner);
    view.runCheck();

    QCOMPARE(runner.calls, 1);
    // Not a path read back out of the saved file: the vault's location is a fact the
    // host knows, and a vault that moved must not keep publishing to the old one.
    QCOMPARE(runner.lastConfig.value(QStringLiteral("vault")).toString(), harness.vault.basePath());
    QCOMPARE(runner.lastConfig.value(QStringLiteral("projections")).toArray().size(), 1);
}

void TestKilnView::writesCheckOrProjectAsTheMode()
{
    Harness harness;
    harness.seed(harness.usableSettings(harness.dir.path() + QStringLiteral("/site")));

    FakeKilnRunner runner;
    KilnView view(&harness.ctx, &runner);

    view.runCheck();
    QCOMPARE(runner.lastMode, QStringLiteral("check"));
    view.runProject();
    QCOMPARE(runner.lastMode, QStringLiteral("project"));
    QCOMPARE(runner.calls, 2);
}

void TestKilnView::showsTheEngineReport()
{
    Harness harness;
    harness.seed(harness.usableSettings(harness.dir.path() + QStringLiteral("/site")));

    FakeKilnRunner runner;
    runner.autoAnswer = true;
    runner.answerExit = 2;
    runner.answerReport = QJsonDocument::fromJson(R"({
        "ok": true, "dryRun": true, "targetRoot": "/tmp/site",
        "selected": 37, "linksRewritten": 46,
        "published": [], "denied": [{"note": "software/_schema.md", "detail": "select.deny"}],
        "unresolvedLinks": [{"note": "software/Penelope.md", "detail": "techniques/Gone"}],
        "unselectedLinks": [], "brokenProperties": [], "removed": [], "warnings": []
    })").object();

    KilnView view(&harness.ctx, &runner);
    view.runCheck();

    const QString text = view.reportText();
    QVERIFY(text.contains(QStringLiteral("37")));
    QVERIFY(text.contains(QStringLiteral("software/_schema.md")));
    QVERIFY(text.contains(QStringLiteral("techniques/Gone")));
    QVERIFY(text.contains(QStringLiteral("something to fix")));
}

void TestKilnView::settingsSurviveTheHostStore()
{
    Harness harness;
    const QString site = harness.dir.path() + QStringLiteral("/site");

    {
        FakeKilnRunner runner;
        KilnView view(&harness.ctx, &runner);
        view.runCheck(); // no site directory yet, so nothing is spawned…
        QCOMPARE(runner.calls, 0);
    }

    harness.seed(harness.usableSettings(site));
    {
        FakeKilnRunner runner;
        KilnView view(&harness.ctx, &runner);
        // …and the seeded settings come back out of the store as settings.
        QCOMPARE(view.configForRun().value(QStringLiteral("output")).toString(), site);
        view.runCheck();
        QCOMPARE(runner.calls, 1);
        QCOMPARE(runner.lastConfig.value(QStringLiteral("output")).toString(), site);
    }

    // The panel writes back what it holds, so the same file works from a shell.
    const QJsonObject written = harness.stored();
    QCOMPARE(written.value(QStringLiteral("output")).toString(), site);
    QVERIFY(written.contains(QStringLiteral("projections")));
    QVERIFY(written.value(QStringLiteral("vault")).toString().isEmpty()
            || written.value(QStringLiteral("vault")).toString() == harness.vault.basePath());
}

void TestKilnView::copylessDefaultIsNotRunnable()
{
    Harness harness;
    FakeKilnRunner runner;
    KilnView view(&harness.ctx, &runner);

    // A fresh panel has no projections, so it is not runnable and says so.
    QVERIFY(!view.settings().problem().isEmpty());
    view.runProject();
    QCOMPARE(runner.calls, 0);
}

void TestKilnView::theScaffoldActionRunsTheScaffoldMode()
{
    // The shell is a separate command, not a flag on `project`: it writes layouts and no
    // content, so a site builds before there is anything to put in it.
    Harness harness;
    harness.seed(harness.usableSettings(harness.dir.path() + QStringLiteral("/site")));

    FakeKilnRunner runner;
    KilnView view(&harness.ctx, &runner);
    view.runScaffold();

    QCOMPARE(runner.calls, 1);
    QCOMPARE(runner.lastMode, QStringLiteral("scaffold"));
}

void TestKilnView::theTagParentsBoxReflectsAndWritesTheSetting()
{
    // A checkbox that does not reach the config is the failure mode worth pinning: the
    // panel would look right and the engine would never hear about it. Found by its label,
    // because the form has several boxes and `findChild` returns whichever is first.
    const auto boxFor = [](KilnView &view, const QString &needle) -> QCheckBox * {
        for (QCheckBox *candidate : view.findChildren<QCheckBox *>()) {
            if (candidate->text().contains(needle))
                return candidate;
        }
        return nullptr;
    };
    Harness harness;
    const QString site = harness.dir.path() + QStringLiteral("/site");

    // Default: on, and absent from the config, because on is the engine's own default.
    harness.seed(harness.usableSettings(site));
    {
        FakeKilnRunner runner;
        KilnView view(&harness.ctx, &runner);
        QCheckBox *box = boxFor(view, QStringLiteral("nested tag"));
        QVERIFY(box);
        QVERIFY(box->isChecked());
        QVERIFY(!view.configForRun().value(QStringLiteral("projections")).toArray().at(0).toObject()
                     .contains(QStringLiteral("tagParents")));

        // Ticking it off must reach the config the engine is handed.
        box->setChecked(false);
        QCOMPARE(view.configForRun().value(QStringLiteral("projections")).toArray().at(0).toObject()
                     .value(QStringLiteral("tagParents")).toBool(true),
                 false);
    }

    // And the other direction: a stored `false` comes back as an unticked box.
    harness.seed(harness.usableSettings(site));
    {
        QJsonObject stored = harness.stored();
        QJsonArray projections = stored.value(QStringLiteral("projections")).toArray();
        QJsonObject projection = projections.at(0).toObject();
        projection.insert(QStringLiteral("tagParents"), false);
        stored.insert(QStringLiteral("projections"), QJsonArray{projection});
        harness.seed(stored);

        FakeKilnRunner runner;
        KilnView view(&harness.ctx, &runner);
        QCheckBox *box = boxFor(view, QStringLiteral("nested tag"));
        QVERIFY(box);
        QVERIFY(!box->isChecked());
    }
}

QTEST_MAIN(TestKilnView)
#include "tst_kilnview.moc"
