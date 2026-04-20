// SPDX-License-Identifier: GPL-3.0-or-later
#include "../BookmarkItem.h"
#include "../BookmarksStore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

using namespace Corbomite::Bookmarks;

class TstBookmarksStore : public QObject
{
    Q_OBJECT
private slots:
    void emptyJsonLoadsAsEmptyStore();
    void fileItemRoundTrips();
    void groupWithChildrenRoundTrips();
    void unknownTypeIsPreserved();
    void unknownKeysOnKnownTypeArePreserved();
    void addBookmarkAppendsAtRoot();
    void removeBookmarkByPath();
};

void TstBookmarksStore::emptyJsonLoadsAsEmptyStore()
{
    BookmarksStore store;
    QVERIFY(store.loadFromJson(QJsonDocument::fromJson("{\"items\":[]}").object()));
    QCOMPARE(store.rootItems().size(), 0);
}

void TstBookmarksStore::fileItemRoundTrips()
{
    const QByteArray raw = R"({"items":[{"type":"file","ctime":1713544200000,"path":"notes/foo.md","title":"Foo"}]})";
    BookmarksStore store;
    QVERIFY(store.loadFromJson(QJsonDocument::fromJson(raw).object()));
    QCOMPARE(store.rootItems().size(), 1);
    const auto &item = store.rootItems().at(0);
    QCOMPARE(item.type, QStringLiteral("file"));
    QCOMPARE(item.path, QStringLiteral("notes/foo.md"));
    QCOMPARE(item.title, QStringLiteral("Foo"));
    QCOMPARE(item.ctime, 1713544200000LL);

    const QJsonObject out = store.toJson();
    // Note: QJsonDocument reserializes QJsonObject with keys in sorted order.
    // If the raw bytes don't match due to key ordering, parse both sides back
    // to QJsonObject and compare structurally instead of byte-exact.
    const QByteArray roundTripped = QJsonDocument(out).toJson(QJsonDocument::Compact);
    if (roundTripped != raw) {
        QCOMPARE(QJsonDocument::fromJson(roundTripped).object(),
                 QJsonDocument::fromJson(raw).object());
    } else {
        QCOMPARE(roundTripped, raw);
    }
}

void TstBookmarksStore::groupWithChildrenRoundTrips()
{
    const QByteArray raw = R"({"items":[{"type":"group","ctime":1,"title":"Reading","items":[{"type":"file","ctime":2,"path":"a.md"}]}]})";
    BookmarksStore store;
    QVERIFY(store.loadFromJson(QJsonDocument::fromJson(raw).object()));
    QCOMPARE(store.rootItems().size(), 1);
    QCOMPARE(store.rootItems().at(0).children.size(), 1);
    QCOMPARE(store.rootItems().at(0).children.at(0).path, QStringLiteral("a.md"));

    const QByteArray roundTripped = QJsonDocument(store.toJson()).toJson(QJsonDocument::Compact);
    if (roundTripped != raw) {
        QCOMPARE(QJsonDocument::fromJson(roundTripped).object(),
                 QJsonDocument::fromJson(raw).object());
    } else {
        QCOMPARE(roundTripped, raw);
    }
}

void TstBookmarksStore::unknownTypeIsPreserved()
{
    const QByteArray raw = R"({"items":[{"type":"future-kind","ctime":1,"customField":"hello"}]})";
    BookmarksStore store;
    QVERIFY(store.loadFromJson(QJsonDocument::fromJson(raw).object()));
    QCOMPARE(store.rootItems().size(), 1);
    QCOMPARE(store.rootItems().at(0).type, QStringLiteral("future-kind"));

    const QByteArray roundTripped = QJsonDocument(store.toJson()).toJson(QJsonDocument::Compact);
    if (roundTripped != raw) {
        QCOMPARE(QJsonDocument::fromJson(roundTripped).object(),
                 QJsonDocument::fromJson(raw).object());
    } else {
        QCOMPARE(roundTripped, raw);
    }
}

void TstBookmarksStore::unknownKeysOnKnownTypeArePreserved()
{
    const QByteArray raw = R"({"items":[{"type":"file","ctime":1,"path":"x.md","futureField":42}]})";
    BookmarksStore store;
    QVERIFY(store.loadFromJson(QJsonDocument::fromJson(raw).object()));

    const QByteArray roundTripped = QJsonDocument(store.toJson()).toJson(QJsonDocument::Compact);
    if (roundTripped != raw) {
        QCOMPARE(QJsonDocument::fromJson(roundTripped).object(),
                 QJsonDocument::fromJson(raw).object());
    } else {
        QCOMPARE(roundTripped, raw);
    }
}

void TstBookmarksStore::addBookmarkAppendsAtRoot()
{
    BookmarksStore store;
    BookmarkItem item;
    item.type = QStringLiteral("file");
    item.path = QStringLiteral("a.md");
    item.ctime = 100;
    store.addBookmark(item, {});
    QCOMPARE(store.rootItems().size(), 1);
    QCOMPARE(store.rootItems().at(0).path, QStringLiteral("a.md"));
}

void TstBookmarksStore::removeBookmarkByPath()
{
    BookmarksStore store;
    BookmarkItem a; a.type = "file"; a.path = "a.md"; a.ctime = 1;
    BookmarkItem b; b.type = "file"; b.path = "b.md"; b.ctime = 2;
    store.addBookmark(a, {});
    store.addBookmark(b, {});
    QCOMPARE(store.rootItems().size(), 2);
    store.removeBookmark({QStringLiteral("0")});
    QCOMPARE(store.rootItems().size(), 1);
    QCOMPARE(store.rootItems().at(0).path, QStringLiteral("b.md"));
}

QTEST_MAIN(TstBookmarksStore)
#include "tst_bookmarks_store.moc"
