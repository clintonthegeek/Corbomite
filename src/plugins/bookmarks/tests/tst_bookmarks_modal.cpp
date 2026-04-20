// SPDX-License-Identifier: GPL-3.0-or-later
#include "../BookmarkItem.h"
#include "../BookmarkModal.h"
#include "../BookmarksStore.h"

#include <QComboBox>
#include <QLineEdit>
#include <QTest>

using namespace Corbomite::Bookmarks;

class TstBookmarkModal : public QObject
{
    Q_OBJECT
private slots:
    void titlePrefilledFromFilePath();
    void titlePrefilledFromSearchQuery();
    void titlePrefilledFromGraph();
    void titleRespectsExistingOverride();
    void groupComboIncludesRootPlusGroupsRecursively();
    void groupComboSkipsWhenStoreHasNoGroups();
    void commitAddsAtRootByDefault();
    void commitTargetsSelectedGroup();
    void commitPreservesInferredTitleWhenUnchanged();
    void commitStoresEditedTitle();
};

static BookmarkItem fileItem(const QString &path)
{
    BookmarkItem it;
    it.type = QStringLiteral("file");
    it.path = path;
    it.ctime = 42;
    return it;
}

void TstBookmarkModal::titlePrefilledFromFilePath()
{
    BookmarksStore store;
    BookmarkModal dlg(fileItem(QStringLiteral("notes/Daily/2026-04-20.md")), &store);
    QCOMPARE(dlg.nameEdit()->text(), QStringLiteral("2026-04-20"));
}

void TstBookmarkModal::titlePrefilledFromSearchQuery()
{
    BookmarksStore store;
    BookmarkItem it;
    it.type = QStringLiteral("search");
    it.query = QStringLiteral("tag:#todo");
    BookmarkModal dlg(it, &store);
    QCOMPARE(dlg.nameEdit()->text(), QStringLiteral("tag:#todo"));
}

void TstBookmarkModal::titlePrefilledFromGraph()
{
    BookmarksStore store;
    BookmarkItem it;
    it.type = QStringLiteral("graph");
    BookmarkModal dlg(it, &store);
    QVERIFY(!dlg.nameEdit()->text().isEmpty());
}

void TstBookmarkModal::titleRespectsExistingOverride()
{
    BookmarksStore store;
    BookmarkItem it = fileItem(QStringLiteral("a.md"));
    it.title = QStringLiteral("Already named");
    BookmarkModal dlg(it, &store);
    QCOMPARE(dlg.nameEdit()->text(), QStringLiteral("Already named"));
}

void TstBookmarkModal::groupComboIncludesRootPlusGroupsRecursively()
{
    BookmarksStore store;
    BookmarkItem g1; g1.type = QStringLiteral("group"); g1.title = QStringLiteral("Reading");
    BookmarkItem g2; g2.type = QStringLiteral("group"); g2.title = QStringLiteral("Later");
    g1.children.append(g2);
    store.addBookmark(g1, {});
    BookmarkModal dlg(fileItem(QStringLiteral("a.md")), &store);
    QCOMPARE(dlg.groupCombo()->count(), 3);
    QCOMPARE(dlg.groupCombo()->itemText(1), QStringLiteral("Reading"));
    QCOMPARE(dlg.groupCombo()->itemText(2), QStringLiteral("Reading / Later"));
}

void TstBookmarkModal::groupComboSkipsWhenStoreHasNoGroups()
{
    BookmarksStore store;
    BookmarkModal dlg(fileItem(QStringLiteral("a.md")), &store);
    QCOMPARE(dlg.groupCombo()->count(), 1);
}

void TstBookmarkModal::commitAddsAtRootByDefault()
{
    BookmarksStore store;
    BookmarkModal dlg(fileItem(QStringLiteral("a.md")), &store);
    dlg.commit();
    QCOMPARE(store.rootItems().size(), 1);
    QCOMPARE(store.rootItems().at(0).path, QStringLiteral("a.md"));
}

void TstBookmarkModal::commitTargetsSelectedGroup()
{
    BookmarksStore store;
    BookmarkItem g; g.type = QStringLiteral("group"); g.title = QStringLiteral("Reading");
    store.addBookmark(g, {});
    BookmarkModal dlg(fileItem(QStringLiteral("a.md")), &store);
    QCOMPARE(dlg.groupCombo()->count(), 2);
    dlg.groupCombo()->setCurrentIndex(1);  // Reading
    dlg.commit();
    QCOMPARE(store.rootItems().at(0).children.size(), 1);
    QCOMPARE(store.rootItems().at(0).children.at(0).path, QStringLiteral("a.md"));
}

void TstBookmarkModal::commitPreservesInferredTitleWhenUnchanged()
{
    BookmarksStore store;
    BookmarkModal dlg(fileItem(QStringLiteral("notes/foo.md")), &store);
    dlg.commit();
    // Title stays empty on the item — display inference continues to work.
    QVERIFY(store.rootItems().at(0).title.isEmpty());
}

void TstBookmarkModal::commitStoresEditedTitle()
{
    BookmarksStore store;
    BookmarkModal dlg(fileItem(QStringLiteral("notes/foo.md")), &store);
    dlg.nameEdit()->setText(QStringLiteral("Important note"));
    dlg.commit();
    QCOMPARE(store.rootItems().at(0).title, QStringLiteral("Important note"));
}

QTEST_MAIN(TstBookmarkModal)
#include "tst_bookmarks_modal.moc"
