// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include "corbomite/storage/FileSystemAdapter.h"

class TestFileSystemAdapter : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testWriteAndReadRoundTrip()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        Corbomite::FileSystemAdapter fs;

        QString path = tmp.path() + "/test.md";
        QVERIFY(fs.writeFile(path, QStringLiteral("# Hello\n\nWorld")));

        auto result = fs.readFile(path);
        QVERIFY(result.has_value());
        QCOMPARE(result.value(), QStringLiteral("# Hello\n\nWorld"));
    }

    void testReadNonexistent()
    {
        Corbomite::FileSystemAdapter fs;
        auto result = fs.readFile(QStringLiteral("/nonexistent/path/file.md"));
        QVERIFY(!result.has_value());
    }

    void testWriteCreatesIntermediateDirs()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        Corbomite::FileSystemAdapter fs;

        QString path = tmp.path() + "/a/b/c/deep.md";
        QVERIFY(fs.writeFile(path, QStringLiteral("deep content")));

        auto result = fs.readFile(path);
        QVERIFY(result.has_value());
        QCOMPARE(result.value(), QStringLiteral("deep content"));
    }

    void testUtf8RoundTrip()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        Corbomite::FileSystemAdapter fs;

        QString content = QString::fromUtf8(u8"日本語テスト 🎉 café résumé");
        QString path = tmp.path() + "/utf8.md";
        QVERIFY(fs.writeFile(path, content));

        auto result = fs.readFile(path);
        QVERIFY(result.has_value());
        QCOMPARE(result.value(), content);
    }

    void testRename()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        Corbomite::FileSystemAdapter fs;

        QString oldPath = tmp.path() + "/old.md";
        QString newPath = tmp.path() + "/new.md";
        fs.writeFile(oldPath, QStringLiteral("content"));

        QVERIFY(fs.rename(oldPath, newPath));
        QVERIFY(!fs.exists(oldPath));
        QVERIFY(fs.exists(newPath));

        auto result = fs.readFile(newPath);
        QVERIFY(result.has_value());
        QCOMPARE(result.value(), QStringLiteral("content"));
    }

    void testRemove()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        Corbomite::FileSystemAdapter fs;

        QString path = tmp.path() + "/doomed.md";
        fs.writeFile(path, QStringLiteral("bye"));

        QVERIFY(fs.exists(path));
        QVERIFY(fs.remove(path));
        QVERIFY(!fs.exists(path));
    }

    void testExistsReturnsFalseForMissing()
    {
        Corbomite::FileSystemAdapter fs;
        QVERIFY(!fs.exists(QStringLiteral("/surely/not/here.md")));
    }

    void testWriteEmptyFile()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        Corbomite::FileSystemAdapter fs;

        QString path = tmp.path() + "/empty.md";
        QVERIFY(fs.writeFile(path, QString()));

        auto result = fs.readFile(path);
        QVERIFY(result.has_value());
        QCOMPARE(result.value(), QString());
    }
};

QTEST_MAIN(TestFileSystemAdapter)
#include "tst_filesystemadapter.moc"
