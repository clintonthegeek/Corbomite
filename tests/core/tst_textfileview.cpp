// tests/core/tst_textfileview.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include <QTimer>
#include "corbomite/core/TextFileView.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/core/DataAdapter.h"

using namespace Corbomite;

class MemoryAdapter : public DataAdapter
{
public:
    QHash<QString, QString> files;
    bool failNextWrite = false;

    bool exists(const QString &p) const override { return files.contains(p); }
    std::optional<QString> read(const QString &p) const override {
        if (files.contains(p)) return files[p];
        return std::nullopt;
    }
    std::optional<QByteArray> readBinary(const QString &) const override { return std::nullopt; }
    FileStat stat(const QString &) const override { return {}; }
    QStringList list(const QString &) const override { return {}; }
    bool write(const QString &p, const QString &c, const WriteHints & = {}) override {
        if (failNextWrite) { failNextWrite = false; return false; }
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

class TestTextFileView : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void saveSkipsWhenClean()
    {
        MemoryAdapter adapter;
        adapter.files[QStringLiteral("/vault/test.md")] = QStringLiteral("hello");

        ConcreteTextFileView view(nullptr);
        view.setDataAdapter(&adapter);
        view.setVaultRoot(QStringLiteral("/vault"));

        auto *doc = new NoteDocument(QStringLiteral("/vault"), QStringLiteral("test.md"), nullptr, &view);
        view.loadFile(doc);

        view.save();
        QCOMPARE(adapter.files[QStringLiteral("/vault/test.md")], QStringLiteral("hello"));
    }

    void saveWritesDirtyContent()
    {
        MemoryAdapter adapter;
        adapter.files[QStringLiteral("/vault/test.md")] = QStringLiteral("hello");

        ConcreteTextFileView view(nullptr);
        view.setDataAdapter(&adapter);
        view.setVaultRoot(QStringLiteral("/vault"));

        auto *doc = new NoteDocument(QStringLiteral("/vault"), QStringLiteral("test.md"), nullptr, &view);
        view.loadFile(doc);

        view.content = QStringLiteral("hello world");
        view.save();
        QCOMPARE(adapter.files[QStringLiteral("/vault/test.md")], QStringLiteral("hello world"));
    }

    void saveFailureWritesBackup()
    {
        MemoryAdapter adapter;
        adapter.files[QStringLiteral("/vault/test.md")] = QStringLiteral("hello");

        ConcreteTextFileView view(nullptr);
        view.setDataAdapter(&adapter);
        view.setVaultRoot(QStringLiteral("/vault"));

        auto *doc = new NoteDocument(QStringLiteral("/vault"), QStringLiteral("test.md"), nullptr, &view);
        view.loadFile(doc);

        view.content = QStringLiteral("changed");
        adapter.failNextWrite = true;

        QSignalSpy errorSpy(&view, &TextFileView::saveError);
        view.save();

        QCOMPARE(errorSpy.count(), 1);
        bool backupExists = false;
        for (auto it = adapter.files.cbegin(); it != adapter.files.cend(); ++it) {
            if (it.key().contains(QStringLiteral("file-recovery"))) {
                backupExists = true;
                QCOMPARE(it.value(), QStringLiteral("changed"));
                break;
            }
        }
        QVERIFY(backupExists);
    }

    void saveReentryGuard()
    {
        MemoryAdapter adapter;
        adapter.files[QStringLiteral("/vault/test.md")] = QStringLiteral("v1");

        ConcreteTextFileView view(nullptr);
        view.setDataAdapter(&adapter);
        view.setVaultRoot(QStringLiteral("/vault"));

        auto *doc = new NoteDocument(QStringLiteral("/vault"), QStringLiteral("test.md"), nullptr, &view);
        view.loadFile(doc);

        view.content = QStringLiteral("v2");
        view.save();
        QCOMPARE(adapter.files[QStringLiteral("/vault/test.md")], QStringLiteral("v2"));
    }

    void immediateSaveClearsState()
    {
        MemoryAdapter adapter;
        adapter.files[QStringLiteral("/vault/test.md")] = QStringLiteral("v1");

        ConcreteTextFileView view(nullptr);
        view.setDataAdapter(&adapter);
        view.setVaultRoot(QStringLiteral("/vault"));

        auto *doc = new NoteDocument(QStringLiteral("/vault"), QStringLiteral("test.md"), nullptr, &view);
        view.loadFile(doc);

        view.content = QStringLiteral("v2");
        view.save(true);
        QCOMPARE(adapter.files[QStringLiteral("/vault/test.md")], QStringLiteral("v2"));
        QVERIFY(view.content.isEmpty());
    }
};

QTEST_MAIN(TestTextFileView)
#include "tst_textfileview.moc"
