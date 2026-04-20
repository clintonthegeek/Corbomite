// SPDX-License-Identifier: GPL-3.0-or-later
#include "../BookmarkItem.h"
#include "../BookmarksPlugin.h"
#include "../BookmarksStore.h"

#include <QJsonObject>
#include <QStringList>
#include <QTest>

using namespace Corbomite::Bookmarks;

class TstBookmarksCommands : public QObject
{
    Q_OBJECT
private slots:
    void bookmarkCurrentFileAppendsAtRoot();
    void bookmarkCurrentFileIgnoresEmptyPath();
    void bookmarkCurrentFileAppendsInGroup();
    void bookmarkAllTabsAppendsEachPath();
    void bookmarkAllTabsEmptyListIsNoop();
    void bookmarkCurrentHeadingUsesSubpath();
    void bookmarkCurrentHeadingNormalisesMissingHash();
    void bookmarkCurrentBlockUsesCaretPrefix();
    void bookmarkCurrentBlockNoopsWithoutId();
    void bookmarkCurrentSearchSnapshotsQuery();
    void bookmarkCurrentSearchNoopsOnEmptyQuery();
    void bookmarkCurrentGraphSnapshotsOptions();
};

void TstBookmarksCommands::bookmarkCurrentFileAppendsAtRoot()
{
    BookmarksStore store;
    BookmarksPlugin::bookmarkFile(&store, QStringLiteral("notes/foo.md"));
    QCOMPARE(store.rootItems().size(), 1);
    const auto &it = store.rootItems().at(0);
    QCOMPARE(it.type, QStringLiteral("file"));
    QCOMPARE(it.path, QStringLiteral("notes/foo.md"));
    QVERIFY(it.ctime > 0);
}

void TstBookmarksCommands::bookmarkCurrentFileIgnoresEmptyPath()
{
    BookmarksStore store;
    BookmarksPlugin::bookmarkFile(&store, QString());
    QCOMPARE(store.rootItems().size(), 0);
}

void TstBookmarksCommands::bookmarkCurrentFileAppendsInGroup()
{
    BookmarksStore store;
    BookmarkItem group;
    group.type  = QStringLiteral("group");
    group.title = QStringLiteral("Reading");
    store.addBookmark(group, {});
    BookmarksPlugin::bookmarkFile(&store, QStringLiteral("a.md"), {QStringLiteral("0")});
    QCOMPARE(store.rootItems().size(), 1);
    QCOMPARE(store.rootItems().at(0).children.size(), 1);
    QCOMPARE(store.rootItems().at(0).children.at(0).path, QStringLiteral("a.md"));
}

void TstBookmarksCommands::bookmarkAllTabsAppendsEachPath()
{
    BookmarksStore store;
    BookmarksPlugin::bookmarkAllTabs(&store,
        {QStringLiteral("a.md"), QStringLiteral("b.md"), QStringLiteral("c.md")});
    QCOMPARE(store.rootItems().size(), 3);
    QCOMPARE(store.rootItems().at(0).path, QStringLiteral("a.md"));
    QCOMPARE(store.rootItems().at(2).path, QStringLiteral("c.md"));
}

void TstBookmarksCommands::bookmarkAllTabsEmptyListIsNoop()
{
    BookmarksStore store;
    BookmarksPlugin::bookmarkAllTabs(&store, {});
    QCOMPARE(store.rootItems().size(), 0);
}

void TstBookmarksCommands::bookmarkCurrentHeadingUsesSubpath()
{
    BookmarksStore store;
    BookmarksPlugin::bookmarkHeading(&store, QStringLiteral("notes/foo.md"),
                                     QStringLiteral("#Intro"));
    QCOMPARE(store.rootItems().size(), 1);
    const auto &it = store.rootItems().at(0);
    QCOMPARE(it.path, QStringLiteral("notes/foo.md#Intro"));
    QCOMPARE(it.subpath, QStringLiteral("#Intro"));
}

void TstBookmarksCommands::bookmarkCurrentHeadingNormalisesMissingHash()
{
    BookmarksStore store;
    BookmarksPlugin::bookmarkHeading(&store, QStringLiteral("a.md"),
                                     QStringLiteral("Methods"));
    QCOMPARE(store.rootItems().at(0).subpath, QStringLiteral("#Methods"));
    QCOMPARE(store.rootItems().at(0).path, QStringLiteral("a.md#Methods"));
}

void TstBookmarksCommands::bookmarkCurrentBlockUsesCaretPrefix()
{
    BookmarksStore store;
    BookmarksPlugin::bookmarkBlock(&store, QStringLiteral("a.md"),
                                   QStringLiteral("abc123"));
    QCOMPARE(store.rootItems().at(0).subpath, QStringLiteral("#^abc123"));
    QCOMPARE(store.rootItems().at(0).path, QStringLiteral("a.md#^abc123"));

    BookmarksStore store2;
    BookmarksPlugin::bookmarkBlock(&store2, QStringLiteral("a.md"),
                                   QStringLiteral("#^xyz"));
    QCOMPARE(store2.rootItems().at(0).subpath, QStringLiteral("#^xyz"));
}

void TstBookmarksCommands::bookmarkCurrentBlockNoopsWithoutId()
{
    BookmarksStore store;
    BookmarksPlugin::bookmarkBlock(&store, QStringLiteral("a.md"), QString());
    QCOMPARE(store.rootItems().size(), 0);
}

void TstBookmarksCommands::bookmarkCurrentSearchSnapshotsQuery()
{
    BookmarksStore store;
    BookmarksPlugin::bookmarkSearch(&store, QStringLiteral("tag:#foo path:notes/"));
    QCOMPARE(store.rootItems().size(), 1);
    const auto &it = store.rootItems().at(0);
    QCOMPARE(it.type, QStringLiteral("search"));
    QCOMPARE(it.query, QStringLiteral("tag:#foo path:notes/"));
}

void TstBookmarksCommands::bookmarkCurrentSearchNoopsOnEmptyQuery()
{
    BookmarksStore store;
    BookmarksPlugin::bookmarkSearch(&store, QString());
    QCOMPARE(store.rootItems().size(), 0);
}

void TstBookmarksCommands::bookmarkCurrentGraphSnapshotsOptions()
{
    BookmarksStore store;
    QJsonObject opts;
    opts.insert(QStringLiteral("showTags"), true);
    opts.insert(QStringLiteral("depth"), 2);
    BookmarksPlugin::bookmarkGraph(&store, opts);
    QCOMPARE(store.rootItems().size(), 1);
    const auto &it = store.rootItems().at(0);
    QCOMPARE(it.type, QStringLiteral("graph"));
    QCOMPARE(it.options.value(QStringLiteral("showTags")).toBool(), true);
    QCOMPARE(it.options.value(QStringLiteral("depth")).toInt(), 2);
}

QTEST_MAIN(TstBookmarksCommands)
#include "tst_bookmarks_commands.moc"
