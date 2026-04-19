// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>

#include "corbomite/vault/Vault.h"
#include "corbomite/vault/TAbstractFile.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include "FileNameValidator.h"

class TestFileNameValidator : public QObject
{
    Q_OBJECT
private slots:
    void testEmptyNameIsInvalid();
    void testBackslashIsInvalid();
    void testColonIsInvalid();
    void testReservedWindowsNameFlagged();
    void testCollisionDetection();
    void testValidNamePassesWithEmptyReturn();
    void testRenameToSameNameNotACollision();
};

namespace {
void writeFile(const QString &path, const QByteArray &body)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write(body);
}
}  // namespace

void TestFileNameValidator::testEmptyNameIsInvalid()
{
    auto err = Corbomite::validateFileName(QStringLiteral(""), nullptr, nullptr);
    QVERIFY(!err.isEmpty());
    QVERIFY(err.contains(QStringLiteral("required"), Qt::CaseInsensitive));
}

void TestFileNameValidator::testBackslashIsInvalid()
{
    auto err = Corbomite::validateFileName(QStringLiteral("foo\\bar.md"), nullptr, nullptr);
    QVERIFY(!err.isEmpty());
    QVERIFY(err.contains(QStringLiteral("cannot contain"), Qt::CaseInsensitive));
}

void TestFileNameValidator::testColonIsInvalid()
{
    auto err = Corbomite::validateFileName(QStringLiteral("foo:bar.md"), nullptr, nullptr);
    QVERIFY(!err.isEmpty());
}

void TestFileNameValidator::testReservedWindowsNameFlagged()
{
    auto err = Corbomite::validateFileName(
        QStringLiteral("CON.md"), nullptr, nullptr, /*isFinal=*/true);
#ifdef Q_OS_WIN
    QVERIFY(!err.isEmpty());
    QVERIFY(err.contains(QStringLiteral("reserved"), Qt::CaseInsensitive));
#else
    QVERIFY(!err.isEmpty());
    QVERIFY(err.contains(QStringLiteral("reserved"), Qt::CaseInsensitive));
#endif
}

void TestFileNameValidator::testCollisionDetection()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    writeFile(tmp.path() + QStringLiteral("/existing.md"), "hi");
    writeFile(tmp.path() + QStringLiteral("/other.md"), "hi");

    Corbomite::FileSystemAdapter fsa;
    Corbomite::Vault vault(&fsa);
    vault.load(tmp.path());

    auto *other = vault.getAbstractFileByPath(QStringLiteral("other.md"));
    QVERIFY(other != nullptr);

    // Try renaming `other.md` to `existing.md` → collision.
    auto err = Corbomite::validateFileName(
        QStringLiteral("existing.md"), other, &vault);
    QVERIFY(!err.isEmpty());
    QVERIFY(err.contains(QStringLiteral("already exists"), Qt::CaseInsensitive));
}

void TestFileNameValidator::testRenameToSameNameNotACollision()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    writeFile(tmp.path() + QStringLiteral("/same.md"), "hi");

    Corbomite::FileSystemAdapter fsa;
    Corbomite::Vault vault(&fsa);
    vault.load(tmp.path());

    auto *self = vault.getAbstractFileByPath(QStringLiteral("same.md"));
    QVERIFY(self != nullptr);

    // Renaming a file to its own name is not a collision.
    auto err = Corbomite::validateFileName(QStringLiteral("same.md"), self, &vault);
    QVERIFY(err.isEmpty());
}

void TestFileNameValidator::testValidNamePassesWithEmptyReturn()
{
    QTemporaryDir tmp;
    Corbomite::FileSystemAdapter fsa;
    Corbomite::Vault vault(&fsa);
    vault.load(tmp.path());
    auto err = Corbomite::validateFileName(
        QStringLiteral("valid-name.md"), nullptr, &vault);
    QVERIFY(err.isEmpty());
}

QTEST_MAIN(TestFileNameValidator)
#include "tst_file_name_validator.moc"
