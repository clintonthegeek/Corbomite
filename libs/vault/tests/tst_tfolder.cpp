// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/vault/TFolder.h"
#include "corbomite/vault/TFile.h"

class TestTFolder : public QObject
{
    Q_OBJECT
private slots:
    void rootRecognition();
    void parentPrefixRoot();
    void parentPrefixNonRoot();
    void getNewPathAfterRenameInFolder();
    void stripsControlCharsAndProducesPath();
    void getFileCountRecursive();
    void getFolderCountRecursive();
};

void TestTFolder::rootRecognition()
{
    Corbomite::TFolder root(nullptr, QStringLiteral("/"));
    QCOMPARE(root.isRoot(), true);

    Corbomite::TFolder sub(nullptr, QStringLiteral("folder"));
    QCOMPARE(sub.isRoot(), false);
}

void TestTFolder::parentPrefixRoot()
{
    Corbomite::TFolder root(nullptr, QStringLiteral("/"));
    QCOMPARE(root.getParentPrefix(), QString());
}

void TestTFolder::parentPrefixNonRoot()
{
    Corbomite::TFolder sub(nullptr, QStringLiteral("a/b"));
    QCOMPARE(sub.getParentPrefix(), QStringLiteral("a/b/"));
}

void TestTFolder::getNewPathAfterRenameInFolder()
{
    Corbomite::TFolder parent(nullptr, QStringLiteral("folder"));
    Corbomite::TFile child(nullptr, QStringLiteral("folder/old.md"));
    child.parent = &parent;
    QCOMPARE(child.getNewPathAfterRename(QStringLiteral("new.md")),
             QStringLiteral("folder/new.md"));
}

void TestTFolder::stripsControlCharsAndProducesPath()
{
    Corbomite::TFolder parent(nullptr, QStringLiteral("x"));
    Corbomite::TFile child(nullptr, QStringLiteral("x/a.md"));
    child.parent = &parent;
    QCOMPARE(child.getNewPathAfterRename(QStringLiteral(" b\x01" "c.md ")),
             QStringLiteral("x/bc.md"));
}

void TestTFolder::getFileCountRecursive()
{
    Corbomite::TFolder root(nullptr, QStringLiteral("/"));
    Corbomite::TFolder sub(nullptr, QStringLiteral("sub"));
    sub.parent = &root;
    root.children.append(&sub);

    Corbomite::TFile a(nullptr, QStringLiteral("a.md"));
    a.parent = &root;
    root.children.append(&a);

    Corbomite::TFile b(nullptr, QStringLiteral("sub/b.md"));
    b.parent = &sub;
    sub.children.append(&b);

    QCOMPARE(root.getFileCount(), 2);
    QCOMPARE(sub.getFileCount(), 1);
}

void TestTFolder::getFolderCountRecursive()
{
    Corbomite::TFolder root(nullptr, QStringLiteral("/"));
    Corbomite::TFolder s1(nullptr, QStringLiteral("s1"));
    Corbomite::TFolder s2(nullptr, QStringLiteral("s1/s2"));
    s1.parent = &root; root.children.append(&s1);
    s2.parent = &s1;   s1.children.append(&s2);

    QCOMPARE(root.getFolderCount(), 2);
    QCOMPARE(s1.getFolderCount(), 1);
}

QTEST_MAIN(TestTFolder)
#include "tst_tfolder.moc"
