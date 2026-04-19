// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster R Task 3.3 — ExportToPdf::exportFileToPath writes a valid PDF
// file for a note's markdown content. Uses the test seam (no dialog).

#include "ExportToPdf.h"

#include "corbomite/storage/FileSystemAdapter.h"
#include "corbomite/vault/TFile.h"
#include "corbomite/vault/Vault.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using Corbomite::FileSystemAdapter;
using Corbomite::Vault;

class TestExportToPdf : public QObject
{
    Q_OBJECT

private slots:
    void exportProducesValidPdf();
    void exportFailsOnNullFile();
};

void TestExportToPdf::exportProducesValidPdf()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString mdPath = tmp.filePath(QStringLiteral("foo.md"));
    {
        QFile f(mdPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("# Hello\n\nBody text.\n");
    }

    FileSystemAdapter fsa;
    Vault vault(&fsa);
    vault.load(tmp.path());
    auto *file = vault.getFileByPath(QStringLiteral("foo.md"));
    QVERIFY(file != nullptr);

    const QString outPath = tmp.filePath(QStringLiteral("out.pdf"));
    QVERIFY(Corbomite::ExportToPdf::exportFileToPath(file, &vault, outPath));

    QFile outFile(outPath);
    QVERIFY(outFile.open(QIODevice::ReadOnly));
    const QByteArray header = outFile.read(4);
    QCOMPARE(header, QByteArray("%PDF"));  // PDF magic
}

void TestExportToPdf::exportFailsOnNullFile()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    FileSystemAdapter fsa;
    Vault vault(&fsa);
    vault.load(tmp.path());

    const QString outPath = tmp.filePath(QStringLiteral("nope.pdf"));
    QVERIFY(!Corbomite::ExportToPdf::exportFileToPath(nullptr, &vault, outPath));
    QVERIFY(!Corbomite::ExportToPdf::exportFileToPath(nullptr, nullptr, outPath));
}

QTEST_MAIN(TestExportToPdf)
#include "tst_export_to_pdf.moc"
