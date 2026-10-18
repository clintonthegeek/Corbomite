// tests/core/tst_recovery_snapshot.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster F-a Phase P0 — periodic File Recovery snapshot.
// Covers: dirty-leaf walk, dedup against the last snapshot, no-write-when-clean,
// the out-of-vault path invariant (P0.6), and the failure/periodic suffix.
#include <QTest>
#include "corbomite/core/TextFileView.h"
#include "corbomite/core/RecoveryService.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/core/DataAdapter.h"

using namespace Corbomite;

namespace {

class MemoryAdapter : public DataAdapter
{
public:
    QHash<QString, QString> files;
    bool exists(const QString &p) const override { return files.contains(p); }
    std::optional<QString> read(const QString &p) const override {
        if (files.contains(p)) return files[p];
        return std::nullopt;
    }
    std::optional<QByteArray> readBinary(const QString &) const override { return std::nullopt; }
    FileStat stat(const QString &) const override { return {}; }
    QStringList list(const QString &) const override { return {}; }
    bool write(const QString &p, const QString &c, const WriteHints & = {}) override {
        files[p] = c;
        return true;
    }
    bool writeBinary(const QString &, const QByteArray &, const WriteHints & = {}) override { return false; }
    bool rename(const QString &, const QString &) override { return false; }
    bool remove(const QString &) override { return false; }
    bool rmdir(const QString &) override { return false; }
    bool mkpath(const QString &) override { return true; }
    bool moveToTrash(const QString &) override { return false; }
};

class ConcreteTextFileView : public TextFileView
{
    Q_OBJECT
public:
    using TextFileView::TextFileView;
    QString content;
    QString getViewData() const override { return content; }
    void setViewData(const QString &data, bool) override { content = data; }
    void clear() override { content.clear(); }
    QString getViewType() const override { return QStringLiteral("test-text"); }
    QString getDisplayText() const override { return QStringLiteral("Test"); }
};

QString recoveryKey(const MemoryAdapter &a)
{
    for (auto it = a.files.cbegin(); it != a.files.cend(); ++it) {
        if (it.key().contains(QStringLiteral("file-recovery")))
            return it.key();
    }
    return {};
}

}  // namespace

class TestRecoverySnapshot : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void writesSnapshotForDirtyViewAndPathIsOutsideVault()
    {
        MemoryAdapter adapter;
        adapter.files[QStringLiteral("/vault/test.md")] = QStringLiteral("hello");

        ConcreteTextFileView view(nullptr);
        view.setDataAdapter(&adapter);
        view.setVaultRoot(QStringLiteral("/vault"));
        auto *doc = new NoteDocument(QStringLiteral("/vault"), QStringLiteral("test.md"), &view);
        view.loadFile(doc);

        view.content = QStringLiteral("edited");
        view.requestSave();  // marks dirty (timer never fires without an event loop)
        QVERIFY(view.isDirty());

        RecoveryService service([&view]() { return QList<TextFileView *>{&view}; });
        QCOMPARE(service.snapshotNow(), 1);

        const QString key = recoveryKey(adapter);
        QVERIFY2(!key.isEmpty(), "a recovery snapshot must have been written");
        QVERIFY(key.endsWith(QStringLiteral(".snapshot.md")));
        QCOMPARE(adapter.files.value(key), QStringLiteral("edited"));
        // P0.6: the store must be OUTSIDE the vault root (the bug the
        // 2026-04-27 sweep fixed).
        QVERIFY2(!key.startsWith(QStringLiteral("/vault")),
                 qPrintable(QStringLiteral("recovery path inside the vault: ") + key));
    }

    void skipsCleanView()
    {
        MemoryAdapter adapter;
        adapter.files[QStringLiteral("/vault/test.md")] = QStringLiteral("hello");

        ConcreteTextFileView view(nullptr);
        view.setDataAdapter(&adapter);
        view.setVaultRoot(QStringLiteral("/vault"));
        auto *doc = new NoteDocument(QStringLiteral("/vault"), QStringLiteral("test.md"), &view);
        view.loadFile(doc);
        QVERIFY(!view.isDirty());

        RecoveryService service([&view]() { return QList<TextFileView *>{&view}; });
        QCOMPARE(service.snapshotNow(), 0);
        QVERIFY(recoveryKey(adapter).isEmpty());
    }

    void dedupsUnchangedContentBetweenPasses()
    {
        MemoryAdapter adapter;
        adapter.files[QStringLiteral("/vault/test.md")] = QStringLiteral("hello");

        ConcreteTextFileView view(nullptr);
        view.setDataAdapter(&adapter);
        view.setVaultRoot(QStringLiteral("/vault"));
        auto *doc = new NoteDocument(QStringLiteral("/vault"), QStringLiteral("test.md"), &view);
        view.loadFile(doc);

        view.content = QStringLiteral("edited");
        view.requestSave();

        RecoveryService service([&view]() { return QList<TextFileView *>{&view}; });
        QCOMPARE(service.snapshotNow(), 1);
        QCOMPARE(service.snapshotNow(), 0);  // dirty, but unchanged since last snapshot

        view.content = QStringLiteral("edited again");
        view.requestSave();
        QCOMPARE(service.snapshotNow(), 1);
    }

    void failureBackupUsesDistinctSuffix()
    {
        MemoryAdapter adapter;
        adapter.files[QStringLiteral("/vault/test.md")] = QStringLiteral("hello");

        ConcreteTextFileView view(nullptr);
        view.setDataAdapter(&adapter);
        view.setVaultRoot(QStringLiteral("/vault"));
        auto *doc = new NoteDocument(QStringLiteral("/vault"), QStringLiteral("test.md"), &view);
        view.loadFile(doc);

        view.writeRecoverySnapshot(QStringLiteral("boom"), /*failure=*/true);
        QVERIFY(recoveryKey(adapter).endsWith(QStringLiteral(".failure.md")));
    }

    void disabledIntervalDoesNotStart()
    {
        RecoveryService service([]() { return QList<TextFileView *>{}; });
        service.setIntervalMinutes(0);
        service.start();
        QVERIFY(!service.isRunning());
    }
};

QTEST_MAIN(TestRecoverySnapshot)
#include "tst_recovery_snapshot.moc"
