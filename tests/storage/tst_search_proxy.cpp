// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/storage/proxies/SearchProxy.h"
#include "corbomite/storage/SQLiteIndex.h"

#include <QObject>
#include <QSet>
#include <QTemporaryDir>
#include <QTest>

using namespace Corbomite;

class tst_search_proxy : public QObject
{
    Q_OBJECT
private slots:
    void permissionGating_searchDeniedReturnsEmpty();
    void permissionGating_searchGrantedForwards();

private:
    std::unique_ptr<QTemporaryDir> m_tmp;
};

void tst_search_proxy::permissionGating_searchDeniedReturnsEmpty()
{
    SQLiteIndex index;
    m_tmp = std::make_unique<QTemporaryDir>();
    QVERIFY(m_tmp->isValid());
    QVERIFY(index.open(m_tmp->filePath(QStringLiteral("idx.sqlite"))));

    QSet<QString> granted;  // no metadata.read
    SearchProxy proxy(&index, granted, QStringLiteral("test.plugin"));

    QCOMPARE(proxy.search(QStringLiteral("anything")).size(), 0);
    QCOMPARE(proxy.allTags().size(), 0);
    QCOMPARE(proxy.backlinksFor(QStringLiteral("x.md")).size(), 0);
}

void tst_search_proxy::permissionGating_searchGrantedForwards()
{
    SQLiteIndex index;
    m_tmp = std::make_unique<QTemporaryDir>();
    QVERIFY(m_tmp->isValid());
    QVERIFY(index.open(m_tmp->filePath(QStringLiteral("idx.sqlite"))));

    QSet<QString> granted = { QStringLiteral("metadata.read") };
    SearchProxy proxy(&index, granted, QStringLiteral("test.plugin"));

    QCOMPARE(proxy.search(QStringLiteral("x")).size(), 0);
    QCOMPARE(proxy.allTags().size(), 0);
    QCOMPARE(proxy.backlinksFor(QStringLiteral("x.md")).size(), 0);
    QCOMPARE(proxy.outlinksFor(QStringLiteral("x.md")).size(), 0);
    QCOMPARE(proxy.allLinks().size(), 0);
    QCOMPARE(proxy.notesWithTag(QStringLiteral("#t")).size(), 0);
    QCOMPARE(proxy.searchCompiled(QStringLiteral("x"), {}, {}).size(), 0);
}

QTEST_GUILESS_MAIN(tst_search_proxy)
#include "tst_search_proxy.moc"
