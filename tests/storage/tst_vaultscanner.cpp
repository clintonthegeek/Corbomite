// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include "corbomite/vault/VaultScanner.h"

class TestVaultScanner : public QObject {
    Q_OBJECT

    void createFile(const QString &path, const QString &content = QStringLiteral("test"))
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        f.write(content.toUtf8());
        f.close();
    }

private Q_SLOTS:
    void testScanFindsMarkdownFiles()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        createFile(tmp.path() + "/note1.md");
        createFile(tmp.path() + "/note2.md");

        Corbomite::VaultScanner scanner;
        auto results = scanner.scan(tmp.path());

        QCOMPARE(results.size(), 2);
    }

    void testScanFindsNestedFiles()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        createFile(tmp.path() + "/top.md");
        createFile(tmp.path() + "/folder/nested.md");
        createFile(tmp.path() + "/folder/deep/deeper.md");

        Corbomite::VaultScanner scanner;
        auto results = scanner.scan(tmp.path());

        QCOMPARE(results.size(), 3);
    }

    void testScanFindsCanvasFiles()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        createFile(tmp.path() + "/note.md");
        createFile(tmp.path() + "/board.canvas");

        Corbomite::VaultScanner scanner;
        auto results = scanner.scan(tmp.path());

        QCOMPARE(results.size(), 2);
    }

    void testScanExcludesObsidianDir()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        createFile(tmp.path() + "/note.md");
        createFile(tmp.path() + "/.obsidian/app.json");
        createFile(tmp.path() + "/.obsidian/plugins/test/main.js");

        Corbomite::VaultScanner scanner;
        auto results = scanner.scan(tmp.path());

        QCOMPARE(results.size(), 1);
        QCOMPARE(results.at(0).relativePath, QStringLiteral("note.md"));
    }

    void testScanExcludesGitDir()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        createFile(tmp.path() + "/note.md");
        createFile(tmp.path() + "/.git/HEAD");

        Corbomite::VaultScanner scanner;
        auto results = scanner.scan(tmp.path());

        QCOMPARE(results.size(), 1);
    }

    void testScanExcludesCorbomiteDir()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        createFile(tmp.path() + "/note.md");
        createFile(tmp.path() + "/.corbomite/index.sqlite");

        Corbomite::VaultScanner scanner;
        auto results = scanner.scan(tmp.path());

        QCOMPARE(results.size(), 1);
    }

    void testScanExcludesNonNoteFiles()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        createFile(tmp.path() + "/note.md");
        createFile(tmp.path() + "/image.png");
        createFile(tmp.path() + "/data.json");
        createFile(tmp.path() + "/readme.txt");

        Corbomite::VaultScanner scanner;
        auto results = scanner.scan(tmp.path());

        // Only .md and .canvas are included
        QCOMPARE(results.size(), 1);
    }

    void testScanEmptyVault()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        Corbomite::VaultScanner scanner;
        auto results = scanner.scan(tmp.path());

        QCOMPARE(results.size(), 0);
    }

    void testScanRelativePathsCorrect()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());

        createFile(tmp.path() + "/folder/note.md");

        Corbomite::VaultScanner scanner;
        auto results = scanner.scan(tmp.path());

        QCOMPARE(results.size(), 1);
        QCOMPARE(results.at(0).relativePath, QStringLiteral("folder/note.md"));
    }
};

QTEST_MAIN(TestVaultScanner)
#include "tst_vaultscanner.moc"
