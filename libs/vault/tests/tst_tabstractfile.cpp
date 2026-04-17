// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/vault/TAbstractFile.h"

class TestTAbstractFile : public QObject
{
    Q_OBJECT
private slots:
    void pathAndNameDerivedFromCtor();
    void setPathUpdatesName();
    void getNewPathAfterRenameWithParent();
    void getNewPathAfterRenameDetachedReturnsEmpty();
    void stripsControlChars();
    void tombstoneDefaultsFalse();
};

namespace {
// Concrete subclass for testing the abstract base.
struct TestFile : public Corbomite::TAbstractFile {
    TestFile(Corbomite::Vault *v, QString p) : TAbstractFile(v, std::move(p)) {}
};
}

void TestTAbstractFile::pathAndNameDerivedFromCtor()
{
    TestFile f(nullptr, QStringLiteral("folder/sub/Note.md"));
    QCOMPARE(f.path, QStringLiteral("folder/sub/Note.md"));
    QCOMPARE(f.name, QStringLiteral("Note.md"));
}

void TestTAbstractFile::setPathUpdatesName()
{
    TestFile f(nullptr, QStringLiteral("a.md"));
    f.setPath(QStringLiteral("folder/b.md"));
    QCOMPARE(f.path, QStringLiteral("folder/b.md"));
    QCOMPARE(f.name, QStringLiteral("b.md"));
}

void TestTAbstractFile::getNewPathAfterRenameWithParent()
{
    // No parent set yet, so detached semantics apply. Parent-backed case is
    // exercised once TFolder exists (tst_vault_tree).
    TestFile f(nullptr, QStringLiteral("folder/old.md"));
    QCOMPARE(f.getNewPathAfterRename(QStringLiteral("new.md")), QString());
}

void TestTAbstractFile::getNewPathAfterRenameDetachedReturnsEmpty()
{
    TestFile f(nullptr, QStringLiteral("orphan.md"));
    QCOMPARE(f.getNewPathAfterRename(QStringLiteral("x")), QString());
}

void TestTAbstractFile::stripsControlChars()
{
    TestFile f(nullptr, QStringLiteral("a.md"));
    QString result = f.getNewPathAfterRename(QStringLiteral("b\x01c.md"));
    QCOMPARE(result, QString());
}

void TestTAbstractFile::tombstoneDefaultsFalse()
{
    TestFile f(nullptr, QStringLiteral("a.md"));
    QCOMPARE(f.deleted, false);
}

QTEST_MAIN(TestTAbstractFile)
#include "tst_tabstractfile.moc"
