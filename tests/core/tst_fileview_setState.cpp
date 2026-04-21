// tests/core/tst_fileview_setState.cpp
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Spec-driven tests for FileView::setState / loadFile contract.
// Ref: docs/superpowers/specs/2026-04-15-cluster-g-views-hierarchy-design.md §4.3
//
// CRITICAL: Do NOT read .cpp implementation files before writing this test.
// Tests are written from spec claims only.

#include <QTest>
#include <QWidget>
#include <QJsonObject>
#include "corbomite/core/FileView.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/WorkspaceLeaf.h"
#include "corbomite/core/NoteDocument.h"

using namespace Corbomite;

// ---------------------------------------------------------------------------
// Minimal concrete FileView subclass for testing.
// FileView is abstract (inherits pure-virtual getViewType() / getDisplayText()
// from View). We override those plus track onLoadFile / onUnloadFile calls.
// ---------------------------------------------------------------------------
class StubFileView : public FileView
{
    Q_OBJECT
public:
    using FileView::FileView;

    QString getViewType() const override { return QStringLiteral("stub-file"); }

    // FileView::getDisplayText() is not pure-virtual — it has a default
    // implementation that returns file()->name() or i18n("No file").
    // We do NOT override it here so the spec behaviour is tested directly.

    int loadFileCalls = 0;
    int unloadFileCalls = 0;
    NoteDocument *lastLoaded = nullptr;

protected:
    void onLoadFile(NoteDocument *file) override
    {
        ++loadFileCalls;
        lastLoaded = file;
    }

    void onUnloadFile(NoteDocument *) override
    {
        ++unloadFileCalls;
    }
};

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------
class TestFileViewSetState : public QObject
{
    Q_OBJECT

private:
    // Convenience: build a NoteDocument on the heap.
    static NoteDocument *makeDoc(const QString &vaultRoot,
                                  const QString &relativePath,
                                  QObject *parent = nullptr)
    {
        return new NoteDocument(vaultRoot, relativePath, nullptr, parent);
    }

private Q_SLOTS:
    // ------------------------------------------------------------------
    // 1. file() returns nullptr before any loadFile call.
    // ------------------------------------------------------------------
    void fileIsNullBeforeLoad()
    {
        StubFileView view(nullptr);
        QVERIFY(view.file() == nullptr);
    }

    // ------------------------------------------------------------------
    // 2. loadFile() sets m_file; file() returns the document after load.
    // ------------------------------------------------------------------
    void loadFileSetsFile()
    {
        StubFileView view(nullptr);
        NoteDocument doc(QStringLiteral("/vault"), QStringLiteral("notes/hello.md"));

        bool ok = view.loadFile(&doc);

        QVERIFY(ok);
        QCOMPARE(view.file(), &doc);
    }

    // ------------------------------------------------------------------
    // 3. onLoadFile() is called after m_file is set.
    //    The spec says "called after m_file is set" — so lastLoaded must
    //    equal the argument and file() must already be set when the hook runs.
    //    We verify onLoadFile was invoked with the correct document.
    // ------------------------------------------------------------------
    void onLoadFileIsCalledAfterFileIsSet()
    {
        StubFileView view(nullptr);
        NoteDocument doc(QStringLiteral("/vault"), QStringLiteral("notes/hello.md"));

        view.loadFile(&doc);

        QCOMPARE(view.loadFileCalls, 1);
        QCOMPARE(view.lastLoaded, &doc);
    }

    // ------------------------------------------------------------------
    // 4. getDisplayText() returns the file name (NoteDocument::name())
    //    after a file is loaded.
    //    Spec: "defaults to file->baseName() or i18n("No file")"
    //    NoteDocument::name() is the method exposed on the header.
    // ------------------------------------------------------------------
    void getDisplayTextReturnsFileName()
    {
        StubFileView view(nullptr);
        NoteDocument doc(QStringLiteral("/vault"), QStringLiteral("notes/hello.md"));

        view.loadFile(&doc);

        QCOMPARE(view.getDisplayText(), doc.name());
    }

    // ------------------------------------------------------------------
    // 5. getDisplayText() without a loaded file should not crash and
    //    should return a non-empty fallback string.
    //    Spec: "or i18n("No file")"
    // ------------------------------------------------------------------
    void getDisplayTextWithNoFileDoesNotCrash()
    {
        StubFileView view(nullptr);
        // Should not crash; result must be non-empty (fallback string).
        QString text;
        QVERIFY_THROWS_NO_EXCEPTION(text = view.getDisplayText());
        QVERIFY(!text.isEmpty());
    }

    // ------------------------------------------------------------------
    // 6. getState() serialises as {file: relativePath} after load.
    //    Spec: "serializes {file: relativePath}"
    // ------------------------------------------------------------------
    void getStateSerializesRelativePath()
    {
        StubFileView view(nullptr);
        NoteDocument doc(QStringLiteral("/vault"), QStringLiteral("notes/hello.md"));

        view.loadFile(&doc);

        QJsonObject state = view.getState();
        QVERIFY(state.contains(QStringLiteral("file")));
        QCOMPARE(state[QStringLiteral("file")].toString(), doc.relativePath());
    }

    // ------------------------------------------------------------------
    // 7. setState({file: path}) resolves via ViewRegistry::FileResolver
    //    and calls loadFile().  After setState, file() == resolved doc.
    // ------------------------------------------------------------------
    void setStateResolvesFileAndCallsLoadFile()
    {
        ViewRegistry registry;
        NoteDocument doc(QStringLiteral("/vault"), QStringLiteral("notes/hello.md"));

        registry.setFileResolver([&](const QString &path) -> NoteDocument * {
            if (path == QStringLiteral("notes/hello.md"))
                return &doc;
            return nullptr;
        });

        WorkspaceLeaf leaf(&registry);
        StubFileView view(&leaf);

        QJsonObject state;
        state[QStringLiteral("file")] = QStringLiteral("notes/hello.md");
        view.setState(state);

        QCOMPARE(view.file(), &doc);
        QCOMPARE(view.loadFileCalls, 1);
    }

    // ------------------------------------------------------------------
    // 8. setState() with no "file" key does not crash.
    //    Spec: "setState with no 'file' key ... does not crash"
    // ------------------------------------------------------------------
    void setStateWithNoFileKeyDoesNotCrash()
    {
        ViewRegistry registry;
        WorkspaceLeaf leaf(&registry);
        StubFileView view(&leaf);

        QJsonObject emptyState;
        QVERIFY_THROWS_NO_EXCEPTION(view.setState(emptyState));

        // File should remain nullptr.
        QVERIFY(view.file() == nullptr);
    }

    // ------------------------------------------------------------------
    // 9. setState() with an unresolvable path does not crash.
    //    Spec: "setState with ... unresolvable path does not crash"
    // ------------------------------------------------------------------
    void setStateWithUnresolvablePathDoesNotCrash()
    {
        ViewRegistry registry;
        // Resolver always returns nullptr.
        registry.setFileResolver([](const QString &) -> NoteDocument * { return nullptr; });

        WorkspaceLeaf leaf(&registry);
        StubFileView view(&leaf);

        QJsonObject state;
        state[QStringLiteral("file")] = QStringLiteral("nonexistent/file.md");
        QVERIFY_THROWS_NO_EXCEPTION(view.setState(state));

        // File must still be nullptr — no successful load happened.
        QVERIFY(view.file() == nullptr);
    }

    // ------------------------------------------------------------------
    // 10. setState() when no FileResolver is set does not crash.
    //     Spec: "setState with no FileResolver set does not crash"
    // ------------------------------------------------------------------
    void setStateWithNoFileResolverDoesNotCrash()
    {
        ViewRegistry registry;
        // No setFileResolver() call — resolver is empty/null.
        WorkspaceLeaf leaf(&registry);
        StubFileView view(&leaf);

        QJsonObject state;
        state[QStringLiteral("file")] = QStringLiteral("notes/hello.md");
        QVERIFY_THROWS_NO_EXCEPTION(view.setState(state));

        QVERIFY(view.file() == nullptr);
    }

    // ------------------------------------------------------------------
    // 11. setState() with no leaf (leaf == nullptr) does not crash.
    //     ViewRegistry access via leaf — if leaf is null setState should
    //     be defensive.
    // ------------------------------------------------------------------
    void setStateWithNullLeafDoesNotCrash()
    {
        StubFileView view(nullptr);

        QJsonObject state;
        state[QStringLiteral("file")] = QStringLiteral("notes/hello.md");
        QVERIFY_THROWS_NO_EXCEPTION(view.setState(state));

        QVERIFY(view.file() == nullptr);
    }

    // ------------------------------------------------------------------
    // 12. Loading a second file unloads the first (onUnloadFile called).
    //     Spec: "orchestrate unload-current → set file → onLoadFile"
    // ------------------------------------------------------------------
    void loadFileUnloadsPreviousFile()
    {
        StubFileView view(nullptr);
        NoteDocument doc1(QStringLiteral("/vault"), QStringLiteral("notes/first.md"));
        NoteDocument doc2(QStringLiteral("/vault"), QStringLiteral("notes/second.md"));

        view.loadFile(&doc1);
        QCOMPARE(view.unloadFileCalls, 0);

        view.loadFile(&doc2);
        QCOMPARE(view.unloadFileCalls, 1);
        QCOMPARE(view.file(), &doc2);
    }

    // ------------------------------------------------------------------
    // 13. setState round-trip: getState() after setState() returns the
    //     same relative path that was set.
    // ------------------------------------------------------------------
    void setStateRoundTrip()
    {
        ViewRegistry registry;
        NoteDocument doc(QStringLiteral("/vault"), QStringLiteral("notes/hello.md"));

        registry.setFileResolver([&](const QString &path) -> NoteDocument * {
            if (path == QStringLiteral("notes/hello.md"))
                return &doc;
            return nullptr;
        });

        WorkspaceLeaf leaf(&registry);
        StubFileView view(&leaf);

        QJsonObject state;
        state[QStringLiteral("file")] = QStringLiteral("notes/hello.md");
        view.setState(state);

        QJsonObject out = view.getState();
        QCOMPARE(out[QStringLiteral("file")].toString(), QStringLiteral("notes/hello.md"));
    }
};

QTEST_MAIN(TestFileViewSetState)
#include "tst_fileview_setState.moc"
