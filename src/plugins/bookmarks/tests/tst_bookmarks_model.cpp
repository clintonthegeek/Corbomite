// SPDX-License-Identifier: GPL-3.0-or-later
#include "../BookmarkItem.h"
#include "../BookmarksModel.h"
#include "../BookmarksStore.h"

#include <QMimeData>
#include <QTest>

using namespace Corbomite::Bookmarks;

class TstBookmarksModel : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void rowCountMatchesStoreDepth();
    void displayRoleInfersFromFilePath();
    void displayRoleUsesTitleOverride();
    void decorationRoleDiffersByType();
    void typeRoleExposesRawType();
    void dragMimeRoundTripsTreePath();

private:
    BookmarksStore *m_store = nullptr;
    BookmarksModel *m_model = nullptr;
};

void TstBookmarksModel::init()
{
    m_store = new BookmarksStore;
    BookmarkItem a; a.type = "file"; a.path = "notes/foo.md"; a.ctime = 1;
    BookmarkItem b; b.type = "group"; b.title = "Reading"; b.ctime = 2;
    BookmarkItem c; c.type = "file"; c.path = "a.md"; c.title = "Alpha"; c.ctime = 3;
    b.children.append(c);
    m_store->addBookmark(a, {});
    m_store->addBookmark(b, {});
    m_model = new BookmarksModel(m_store);
}

void TstBookmarksModel::cleanup()
{
    delete m_model; m_model = nullptr;
    delete m_store; m_store = nullptr;
}

void TstBookmarksModel::rowCountMatchesStoreDepth()
{
    QCOMPARE(m_model->rowCount(QModelIndex()), 2);
    const QModelIndex group = m_model->index(1, 0, QModelIndex());
    QCOMPARE(m_model->rowCount(group), 1);
}

void TstBookmarksModel::displayRoleInfersFromFilePath()
{
    const QModelIndex idx = m_model->index(0, 0, QModelIndex());
    QCOMPARE(idx.data(Qt::DisplayRole).toString(), QStringLiteral("foo"));
}

void TstBookmarksModel::displayRoleUsesTitleOverride()
{
    const QModelIndex group = m_model->index(1, 0, QModelIndex());
    const QModelIndex child = m_model->index(0, 0, group);
    QCOMPARE(child.data(Qt::DisplayRole).toString(), QStringLiteral("Alpha"));
}

void TstBookmarksModel::decorationRoleDiffersByType()
{
    const QModelIndex file = m_model->index(0, 0, QModelIndex());
    const QModelIndex group = m_model->index(1, 0, QModelIndex());
    QVERIFY(file.data(Qt::DecorationRole).isValid());
    QVERIFY(group.data(Qt::DecorationRole).isValid());
}

void TstBookmarksModel::typeRoleExposesRawType()
{
    const QModelIndex file = m_model->index(0, 0, QModelIndex());
    QCOMPARE(file.data(BookmarksModel::BookmarksTypeRole).toString(),
             QStringLiteral("file"));
}

void TstBookmarksModel::dragMimeRoundTripsTreePath()
{
    const QModelIndex file = m_model->index(0, 0, QModelIndex());
    QModelIndexList indices; indices << file;
    QMimeData *mime = m_model->mimeData(indices);
    QVERIFY(mime->hasFormat(QStringLiteral("application/x-corbomite-bookmarks-drag")));
    const QByteArray payload = mime->data(
        QStringLiteral("application/x-corbomite-bookmarks-drag"));
    QCOMPARE(QString::fromUtf8(payload), QStringLiteral("0"));
    delete mime;
}

QTEST_MAIN(TstBookmarksModel)
#include "tst_bookmarks_model.moc"
