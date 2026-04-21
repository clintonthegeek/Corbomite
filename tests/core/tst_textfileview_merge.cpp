// tests/core/tst_textfileview_merge.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include "corbomite/core/TextFileView.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/storage/DataAdapter.h"

using namespace Corbomite;

class MemoryAdapter2 : public DataAdapter
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
    bool write(const QString &p, const QString &c, const WriteHints & = {}) override { files[p] = c; return true; }
    bool writeBinary(const QString &, const QByteArray &, const WriteHints & = {}) override { return false; }
    bool rename(const QString &, const QString &) override { return false; }
    bool remove(const QString &) override { return false; }
    bool rmdir(const QString &) override { return false; }
    bool mkpath(const QString &) override { return true; }
    bool moveToTrash(const QString &) override { return false; }
};

class MergeTestView : public TextFileView
{
    Q_OBJECT
public:
    using TextFileView::TextFileView;
    QString content;
    QString getViewData() const override { return content; }
    void setViewData(const QString &data, bool) override { content = data; }
    void clear() override { content.clear(); }
    QString getViewType() const override { return QStringLiteral("merge-test"); }
    QString getDisplayText() const override { return QStringLiteral("Merge Test"); }
};

class TestTextFileViewMerge : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void externalModifyNoOpWhenSameAsLastSaved()
    {
        MemoryAdapter2 adapter;
        adapter.files[QStringLiteral("/vault/test.md")] = QStringLiteral("original");

        MergeTestView view(nullptr);
        view.setDataAdapter(&adapter);
        view.setVaultRoot(QStringLiteral("/vault"));

        auto *doc = new NoteDocument(QStringLiteral("/vault"), QStringLiteral("test.md"), nullptr, &view);
        view.loadFile(doc);

        view.onExternalModify(QStringLiteral("test.md"));
        QCOMPARE(view.content, QStringLiteral("original"));
    }

    void externalModifyNoLocalChanges()
    {
        MemoryAdapter2 adapter;
        adapter.files[QStringLiteral("/vault/test.md")] = QStringLiteral("original");

        MergeTestView view(nullptr);
        view.setDataAdapter(&adapter);
        view.setVaultRoot(QStringLiteral("/vault"));

        auto *doc = new NoteDocument(QStringLiteral("/vault"), QStringLiteral("test.md"), nullptr, &view);
        view.loadFile(doc);

        adapter.files[QStringLiteral("/vault/test.md")] = QStringLiteral("remote change");
        view.onExternalModify(QStringLiteral("test.md"));
        QCOMPARE(view.content, QStringLiteral("remote change"));
    }

    void externalModifyMergesLocalAndRemote()
    {
        MemoryAdapter2 adapter;
        adapter.files[QStringLiteral("/vault/test.md")] = QStringLiteral("line1\nline2\nline3");

        MergeTestView view(nullptr);
        view.setDataAdapter(&adapter);
        view.setVaultRoot(QStringLiteral("/vault"));

        auto *doc = new NoteDocument(QStringLiteral("/vault"), QStringLiteral("test.md"), nullptr, &view);
        view.loadFile(doc);

        view.content = QStringLiteral("line1\nline2-local\nline3");
        adapter.files[QStringLiteral("/vault/test.md")] = QStringLiteral("line1\nline2\nline3-remote");
        view.onExternalModify(QStringLiteral("test.md"));

        QVERIFY(view.content.contains(QStringLiteral("line2-local")));
        QVERIFY(view.content.contains(QStringLiteral("line3-remote")));
    }

    void wrongFileIgnored()
    {
        MemoryAdapter2 adapter;
        adapter.files[QStringLiteral("/vault/test.md")] = QStringLiteral("original");

        MergeTestView view(nullptr);
        view.setDataAdapter(&adapter);
        view.setVaultRoot(QStringLiteral("/vault"));

        auto *doc = new NoteDocument(QStringLiteral("/vault"), QStringLiteral("test.md"), nullptr, &view);
        view.loadFile(doc);

        view.onExternalModify(QStringLiteral("other.md"));
        QCOMPARE(view.content, QStringLiteral("original"));
    }
};

QTEST_MAIN(TestTextFileViewMerge)
#include "tst_textfileview_merge.moc"
