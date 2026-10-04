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
    void windowsOnlyCharsAreLegalOnPosix_data();
    void windowsOnlyCharsAreLegalOnPosix();
    void windowsOnlyCharsAreRejectedOnWindows();
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

// Obsidian's illegal-character set is platform-conditional
// (`ND`/`HD`, app/own/1126-FD.js:52,54): `\ / :` on POSIX/macOS, the full
// `* " \ / < > : | ?` on Windows. A `?` or `|` in a note name is therefore a
// *legal* vault filename on Linux, and refusing it here would refuse to open
// or rename a file Obsidian is happy with.
void TestFileNameValidator::windowsOnlyCharsAreLegalOnPosix_data()
{
    QTest::addColumn<QString>("name");

    QTest::newRow("question mark") << QStringLiteral("What?.md");
    QTest::newRow("pipe")         << QStringLiteral("a|b.md");
    QTest::newRow("star")         << QStringLiteral("a*b.md");
    QTest::newRow("quote")        << QStringLiteral("say\"hi\".md");
    QTest::newRow("lt/gt")        << QStringLiteral("a<b>c.md");
}

void TestFileNameValidator::windowsOnlyCharsAreLegalOnPosix()
{
    QFETCH(QString, name);
#ifdef Q_OS_WIN
    QVERIFY(!Corbomite::validateFileName(name, nullptr, nullptr).isEmpty());
#else
    QVERIFY2(Corbomite::validateFileName(name, nullptr, nullptr).isEmpty(),
             qPrintable(name));
#endif
}

void TestFileNameValidator::windowsOnlyCharsAreRejectedOnWindows()
{
    const auto err = Corbomite::validateFileName(QStringLiteral("What?.md"),
                                                 nullptr, nullptr);
#ifdef Q_OS_WIN
    QVERIFY(!err.isEmpty());
#else
    QVERIFY(err.isEmpty());
#endif
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
