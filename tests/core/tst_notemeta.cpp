// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/core/NoteMeta.h"

class TestNoteMeta : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testFromFileInfo()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());

        // Create a test file
        QString vaultRoot = tmpDir.path();
        QString filePath = vaultRoot + "/subfolder/my-note.md";
        QDir().mkpath(vaultRoot + "/subfolder");
        QFile f(filePath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("# Hello\n\nSome content.");
        f.close();

        QFileInfo fi(filePath);
        auto meta = Corbomite::NoteMeta::fromFileInfo(fi, vaultRoot);

        QCOMPARE(meta.relativePath, QStringLiteral("subfolder/my-note.md"));
        QCOMPARE(meta.nameFromPath(), QStringLiteral("my-note"));
        QVERIFY(meta.modified.isValid());
        QVERIFY(meta.sizeBytes > 0);
    }

    void testNameStripsExtension()
    {
        Corbomite::NoteMeta meta;
        meta.relativePath = QStringLiteral("folder/My Note.md");

        QCOMPARE(meta.nameFromPath(), QStringLiteral("My Note"));
    }

    void testNameStripsCanvasExtension()
    {
        Corbomite::NoteMeta meta;
        meta.relativePath = QStringLiteral("canvas/Brainstorm.canvas");

        QCOMPARE(meta.nameFromPath(), QStringLiteral("Brainstorm"));
    }

    void testAbsolutePath()
    {
        Corbomite::NoteMeta meta;
        meta.relativePath = QStringLiteral("subfolder/note.md");

        QString abs = meta.absolutePath(QStringLiteral("/home/user/vault"));
        QCOMPARE(abs, QStringLiteral("/home/user/vault/subfolder/note.md"));
    }

    void testPathNormalization()
    {
        // Backslashes should be normalized to forward slashes
        auto meta = Corbomite::NoteMeta::fromRelativePath(QStringLiteral("folder\\note.md"));
        QCOMPARE(meta.relativePath, QStringLiteral("folder/note.md"));
    }

    void testNoLeadingSlash()
    {
        auto meta = Corbomite::NoteMeta::fromRelativePath(QStringLiteral("/folder/note.md"));
        QCOMPARE(meta.relativePath, QStringLiteral("folder/note.md"));
    }
};

QTEST_MAIN(TestNoteMeta)
#include "tst_notemeta.moc"
