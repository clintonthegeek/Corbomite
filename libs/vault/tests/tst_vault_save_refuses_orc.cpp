// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>

#include "corbomite/vault/Vault.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/storage/FileSystemAdapter.h"
#include <markoff/core/MarkoffDocument.h>

using namespace Corbomite;

class TstVaultSaveRefusesOrc : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void saveDocument_withOrcInCanonical_returnsFalseAndDoesNotWrite();
};

void TstVaultSaveRefusesOrc::saveDocument_withOrcInCanonical_returnsFalseAndDoesNotWrite()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    const QString vaultPath = tmp.path();
    const QString relPath = QStringLiteral("Note.md");
    const QString cleanContent = QStringLiteral("Hello, world.\n");
    const QByteArray cleanBytes = cleanContent.toUtf8();
    {
        QFile f(vaultPath + QLatin1Char('/') + relPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        QCOMPARE(f.write(cleanBytes), static_cast<qint64>(cleanBytes.size()));
    }

    FileSystemAdapter adapter;
    Vault vault(&adapter);
    vault.load(vaultPath);

    NoteDocument *doc = vault.openDocument(relPath);
    QVERIFY(doc);

    // Inject U+FFFC into the canonical buffer via the markoff handle.
    // Build the polluted string programmatically so the literal encoding
    // isn't subject to editor/shell-UTF-8 quirks.
    // resetContent() with Origin::ExternalReloadClean calls buffer->reset()
    // directly (bypassing applyCanonicalDelta), so the debug Q_ASSERTs in
    // MarkoffDocument::applyCanonicalDelta are not triggered by this seed.
    QString pollutedCanonical = QStringLiteral("Hello, ");
    pollutedCanonical.append(QChar(QChar::ObjectReplacementCharacter));
    pollutedCanonical.append(QStringLiteral("world.\n"));
    doc->markoff()->resetContent(pollutedCanonical,
                                 Markoff::Origin::ExternalReloadClean);

    QSignalSpy failedSpy(doc, &NoteDocument::saveFailed);
    const bool ok = vault.saveDocument(doc);
    QCOMPARE(ok, false);
    QCOMPARE(failedSpy.count(), 1);

    // File on disk must be unchanged.
    QFile f(vaultPath + QLatin1Char('/') + relPath);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), cleanBytes);
}

QTEST_MAIN(TstVaultSaveRefusesOrc)
#include "tst_vault_save_refuses_orc.moc"
