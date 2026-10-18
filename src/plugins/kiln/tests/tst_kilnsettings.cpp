// SPDX-License-Identifier: GPL-3.0-or-later
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include "../KilnSettings.h"

using namespace Corbomite;

class TestKilnSettings : public QObject
{
    Q_OBJECT

private slots:
    void defaultsAreNotRunnableAndSayWhy();
    void roundTripsThroughJson();
    void omitsLinkBaseWhenEmpty();
    void omitsSelectKeysTheEngineWouldReadAsUnset();
    void omitsAnEmptySectionAndKeepsANamedOne();
    void carriesTheLiveVaultPath();
    void rejectsADuplicateName();
    void rejectsAProjectionThatSelectsNothing();
    void acceptsARunningOrderInsteadOfFolders();
    void describesAReportWithFindings();
    void describesACleanReport();
    void describesAnEngineThatNeverStarted();
    void describesARunThatDidNotHappen();
    void theSiteShellKeysRoundTrip();
    void tagParentsIsWrittenOnlyWhenOff();
    void describesTheShellAndMovedUrls();
};

static KilnProjection siteProjection()
{
    KilnProjection projection;
    projection.name = QStringLiteral("vibekoder.ca");
    projection.folders = {QStringLiteral("software")};
    projection.deny = {QStringLiteral("_schema.md")};
    projection.fieldOrder = {QStringLiteral("title"), QStringLiteral("related")};
    return projection;
}

void TestKilnSettings::defaultsAreNotRunnableAndSayWhy()
{
    KilnSettings settings;
    QVERIFY(settings.problem().contains(QStringLiteral("site directory")));

    settings.output = QStringLiteral("/tmp/site");
    QVERIFY(settings.problem().contains(QStringLiteral("projections")));

    settings.projections.append(siteProjection());
    QCOMPARE(settings.problem(), QString());
}

void TestKilnSettings::roundTripsThroughJson()
{
    KilnSettings settings;
    settings.enginePath = QStringLiteral("/usr/local/bin/kiln");
    settings.output = QStringLiteral("/home/clinton/dev/VibeKoderSite");
    settings.projections.append(siteProjection());

    KilnProjection book;
    book.name = QStringLiteral("the book");
    book.orderFrom = QStringLiteral("maps of content/Chapter One - The Harness.md");
    book.section = QStringLiteral("book");
    book.shape = QStringLiteral("leaf");
    book.frontMatter = QStringLiteral("toml");
    book.linkBase = QStringLiteral("/book/");
    settings.projections.append(book);

    const QJsonObject json = settings.toJson(QStringLiteral("/vaults/VK"));
    const KilnSettings again = KilnSettings::fromJson(json);

    QCOMPARE(again.enginePath, settings.enginePath);
    QCOMPARE(again.output, settings.output);
    QCOMPARE(again.projections.size(), 2);
    QCOMPARE(again.projections.at(0).name, QStringLiteral("vibekoder.ca"));
    QCOMPARE(again.projections.at(0).folders, QStringList{QStringLiteral("software")});
    QCOMPARE(again.projections.at(0).deny, QStringList{QStringLiteral("_schema.md")});
    QCOMPARE(again.projections.at(1).orderFrom, book.orderFrom);
    QCOMPARE(again.projections.at(1).shape, QStringLiteral("leaf"));
    QCOMPARE(again.projections.at(1).frontMatter, QStringLiteral("toml"));
    QCOMPARE(again.projections.at(1).linkBase, QStringLiteral("/book/"));
    QCOMPARE(again.problem(), QString());
}

void TestKilnSettings::omitsLinkBaseWhenEmpty()
{
    // Empty means "derive it from the section". Writing "" would be a value the
    // engine would have to treat as unset anyway — two spellings of one instruction.
    KilnSettings settings;
    settings.output = QStringLiteral("/tmp/site");
    settings.projections.append(siteProjection());
    const QJsonObject projection = settings.toJson(QString()).value(QStringLiteral("projections"))
                                       .toArray().at(0).toObject();
    QVERIFY(!projection.contains(QStringLiteral("linkBase")));

    KilnProjection withBase = siteProjection();
    withBase.linkBase = QStringLiteral("/software/");
    settings.projections[0] = withBase;
    const QJsonObject explicitBase = settings.toJson(QString()).value(QStringLiteral("projections"))
                                         .toArray().at(0).toObject();
    QCOMPARE(explicitBase.value(QStringLiteral("linkBase")).toString(), QStringLiteral("/software/"));
}

void TestKilnSettings::omitsSelectKeysTheEngineWouldReadAsUnset()
{
    KilnSettings settings;
    settings.output = QStringLiteral("/tmp/site");
    settings.projections.append(siteProjection());
    const QJsonObject select = settings.toJson(QString()).value(QStringLiteral("projections"))
                                   .toArray().at(0).toObject()
                                   .value(QStringLiteral("select")).toObject();
    QVERIFY(select.contains(QStringLiteral("folders")));
    QVERIFY(select.contains(QStringLiteral("deny")));
    QVERIFY(!select.contains(QStringLiteral("orderFrom")));
}

void TestKilnSettings::omitsAnEmptySectionAndKeepsANamedOne()
{
    // Empty is the instruction — each note's own folder is its section — and it is the
    // engine's own default, so writing `""` would say nothing. A named one is kept,
    // because it is what moves a folder's notes somewhere else.
    KilnSettings settings;
    settings.output = QStringLiteral("/tmp/site");
    settings.projections.append(siteProjection());

    const QJsonObject empty = settings.toJson(QStringLiteral("/vault"));
    QVERIFY(!empty.value(QStringLiteral("projections")).toArray().at(0).toObject()
                 .contains(QStringLiteral("section")));
    QCOMPARE(KilnSettings::fromJson(empty).projections.first().section, QString());
    QCOMPARE(KilnSettings::fromJson(empty).problem(), QString());

    settings.projections.first().section = QStringLiteral("docs");
    const QJsonObject named = settings.toJson(QStringLiteral("/vault"));
    QCOMPARE(named.value(QStringLiteral("projections")).toArray().at(0).toObject()
                 .value(QStringLiteral("section")).toString(),
             QStringLiteral("docs"));
    QCOMPARE(KilnSettings::fromJson(named).projections.first().section, QStringLiteral("docs"));
}

void TestKilnSettings::carriesTheLiveVaultPath()
{
    // The vault's location is a fact the host knows and a saved file can only
    // remember wrongly: a vault that moved would keep publishing to the old path.
    KilnSettings settings;
    settings.output = QStringLiteral("/tmp/site");
    settings.projections.append(siteProjection());
    QCOMPARE(settings.toJson(QStringLiteral("/now/here")).value(QStringLiteral("vault")).toString(),
             QStringLiteral("/now/here"));
}

void TestKilnSettings::rejectsADuplicateName()
{
    KilnSettings settings;
    settings.output = QStringLiteral("/tmp/site");
    settings.projections.append(siteProjection());
    KilnProjection sameName = siteProjection();
    sameName.section = QStringLiteral("other");
    settings.projections.append(sameName);
    QVERIFY(settings.problem().contains(QStringLiteral("Two projections")));
}

void TestKilnSettings::rejectsAProjectionThatSelectsNothing()
{
    KilnProjection projection;
    projection.name = QStringLiteral("empty");
    QVERIFY(projection.problem().contains(QStringLiteral("selects nothing")));

    projection.folders = {QStringLiteral("software")};
    QCOMPARE(projection.problem(), QString());

    projection.folders.clear();
    projection.orderFrom = QStringLiteral("maps/Chapter One.md");
    QCOMPARE(projection.problem(), QString());
}

void TestKilnSettings::acceptsARunningOrderInsteadOfFolders()
{
    // A book is selected by order, not by folder: the map-of-content note is the
    // table of contents, and reading it is one list instead of two.
    KilnSettings settings;
    settings.output = QStringLiteral("/tmp/site");
    KilnProjection book;
    book.name = QStringLiteral("the book");
    book.orderFrom = QStringLiteral("maps of content/Chapter One - The Harness.md");
    book.shape = QStringLiteral("leaf");
    settings.projections.append(book);
    QCOMPARE(settings.problem(), QString());
}

void TestKilnSettings::describesAReportWithFindings()
{
    const QJsonObject report = QJsonDocument::fromJson(R"({
        "ok": true, "dryRun": true, "targetRoot": "/tmp/site",
        "selected": 37, "linksRewritten": 46,
        "published": [], "denied": [{"note": "software/_schema.md", "detail": "select.deny"}],
        "unresolvedLinks": [{"note": "software/Penelope.md", "detail": "techniques/Gone"}],
        "unselectedLinks": [], "brokenProperties": [], "removed": [],
        "warnings": [{"note": "techniques/Know the Command Line.md", "detail": "no 'title' property"}]
    })").object();

    const QString text = describeReport(report, 2, QString());
    QVERIFY(text.contains(QStringLiteral("37")));
    QVERIFY(text.contains(QStringLiteral("46")));
    QVERIFY(text.contains(QStringLiteral("dry run")));
    QVERIFY(text.contains(QStringLiteral("held back")));
    QVERIFY(text.contains(QStringLiteral("software/_schema.md")));
    QVERIFY(text.contains(QStringLiteral("links to no such note")));
    QVERIFY(text.contains(QStringLiteral("warnings")));
    QVERIFY(text.contains(QStringLiteral("something to fix")));
}

void TestKilnSettings::describesACleanReport()
{
    const QJsonObject report = QJsonDocument::fromJson(R"({
        "ok": true, "dryRun": false, "targetRoot": "/tmp/site",
        "selected": 3, "linksRewritten": 0,
        "published": [], "denied": [], "unresolvedLinks": [], "unselectedLinks": [],
        "brokenProperties": [], "removed": [], "warnings": []
    })").object();

    const QString text = describeReport(report, 0, QString());
    QVERIFY(text.contains(QStringLiteral("Clean")));
    QVERIFY(!text.contains(QStringLiteral("dry run")));
}

void TestKilnSettings::describesAnEngineThatNeverStarted()
{
    // The only place this is learnable: `finished()` never fires for a process that
    // failed to start, so a panel that only listened for it would sit there silently.
    const QString text = describeReport(QJsonObject(), -1, QStringLiteral("No such file or directory"));
    QVERIFY(text.contains(QStringLiteral("did not start")));
    QVERIFY(text.contains(QStringLiteral("No such file")));
}

void TestKilnSettings::describesARunThatDidNotHappen()
{
    const QJsonObject report = QJsonDocument::fromJson(
        R"({"ok": false, "error": "output overlaps the vault"})").object();
    const QString text = describeReport(report, 1, QString());
    QVERIFY(text.contains(QStringLiteral("did not happen")));
    QVERIFY(text.contains(QStringLiteral("overlaps the vault")));
}

void TestKilnSettings::theSiteShellKeysRoundTrip()
{
    KilnSettings settings;
    settings.output = QStringLiteral("/tmp/site");
    KilnProjection projection = siteProjection();
    projection.taxonomies.insert(QStringLiteral("series"), QStringLiteral("series"));
    projection.taxonomies.insert(QStringLiteral("author"), QStringLiteral("authors"));
    projection.related = false;
    projection.fragments = false;
    projection.scaffold = true;
    settings.projections.append(projection);

    const QJsonObject json = settings.toJson(QStringLiteral("/vault"));
    const QJsonObject stored = json.value(QStringLiteral("projections")).toArray().at(0).toObject();
    QCOMPARE(stored.value(QStringLiteral("taxonomies")).toObject()
                 .value(QStringLiteral("author")).toString(),
             QStringLiteral("authors"));
    QCOMPARE(stored.value(QStringLiteral("related")).toBool(true), false);
    QCOMPARE(stored.value(QStringLiteral("fragments")).toBool(true), false);
    QCOMPARE(stored.value(QStringLiteral("scaffold")).toBool(false), true);

    const KilnSettings again = KilnSettings::fromJson(json);
    QCOMPARE(again.projections.first().taxonomies.size(), 2);
    QCOMPARE(again.projections.first().related, false);
    QCOMPARE(again.projections.first().fragments, false);
    QCOMPARE(again.projections.first().scaffold, true);
    QCOMPARE(again.problem(), QString());
}

void TestKilnSettings::tagParentsIsWrittenOnlyWhenOff()
{
    // The engine defaults it on, so the stored config says nothing when it is on — the
    // same rule `related` and `fragments` follow.
    KilnSettings settings;
    settings.output = QStringLiteral("/tmp/site");
    settings.projections.append(siteProjection());

    const QJsonObject on = settings.toJson(QStringLiteral("/vault"));
    QVERIFY(!on.value(QStringLiteral("projections")).toArray().at(0).toObject()
                 .contains(QStringLiteral("tagParents")));
    QCOMPARE(KilnSettings::fromJson(on).projections.first().tagParents, true);

    settings.projections.first().tagParents = false;
    const QJsonObject off = settings.toJson(QStringLiteral("/vault"));
    QCOMPARE(off.value(QStringLiteral("projections")).toArray().at(0).toObject()
                 .value(QStringLiteral("tagParents")).toBool(true),
             false);
    QCOMPARE(KilnSettings::fromJson(off).projections.first().tagParents, false);
}

void TestKilnSettings::describesTheShellAndMovedUrls()
{
    const QJsonObject report = QJsonDocument::fromJson(R"({
        "ok": true, "dryRun": false, "targetRoot": "/tmp/site", "selected": 3, "linksRewritten": 1,
        "published": [], "denied": [], "unresolvedLinks": [], "unselectedLinks": [],
        "brokenProperties": [], "removed": [],
        "moved": [{"note": "software/Markoff.md", "detail": "/software/markoff/ -> /software/markoff-editor/"}],
        "gone": [{"note": "software/Wayfarer.md", "detail": "was a page last time, is not now"}],
        "shell": [{"note": "config/_default/markup.toml", "detail": "regenerated"}],
        "warnings": []
    })").object();

    const QString text = describeReport(report, 0, QString());
    QVERIFY(text.contains(QStringLiteral("URLs that moved")));
    QVERIFY(text.contains(QStringLiteral("/software/markoff-editor/")));
    QVERIFY(text.contains(QStringLiteral("not this time")));
    QVERIFY(text.contains(QStringLiteral("the site shell")));
    QVERIFY(text.contains(QStringLiteral("markup.toml")));
}

QTEST_MAIN(TestKilnSettings)
#include "tst_kilnsettings.moc"
