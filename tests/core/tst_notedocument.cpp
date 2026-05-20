// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QSignalSpy>
#include <QDir>
#include "corbomite/core/NoteDocument.h"
#include <markoff/core/MarkoffDocument.h>
// TODO(port): MarkdownDelta retired
// include <markoff/MarkdownDelta.h>

class TestNoteDocument : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testInitialState()
    {
        Corbomite::NoteDocument doc(QStringLiteral("/vault"), QStringLiteral("note.md"));
        QCOMPARE(doc.relativePath(), QStringLiteral("note.md"));
        QCOMPARE(doc.name(), QStringLiteral("note"));
        QCOMPARE(doc.markdown(), QString());
        QVERIFY(!doc.isModified());
    }

    void testSetMarkdownEmitsTextChanged()
    {
        Corbomite::NoteDocument doc(QStringLiteral("/vault"), QStringLiteral("note.md"));
        QSignalSpy spy(&doc, &Corbomite::NoteDocument::textChanged);

        doc.setMarkdown(QStringLiteral("# Hello"));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(doc.markdown(), QStringLiteral("# Hello"));
    }

    void testModifiedStateTracking()
    {
        Corbomite::NoteDocument doc(QStringLiteral("/vault"), QStringLiteral("note.md"));
        QSignalSpy spy(&doc, &Corbomite::NoteDocument::modificationChanged);

        QVERIFY(!doc.isModified());

        doc.setMarkdown(QStringLiteral("changed"));
        QVERIFY(doc.isModified());
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);

        doc.setModified(false);
        QVERIFY(!doc.isModified());
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(1).at(0).toBool(), false);
    }

    void testWordCount()
    {
        Corbomite::NoteDocument doc(QStringLiteral("/vault"), QStringLiteral("note.md"));

        QCOMPARE(doc.wordCount(), 0);

        doc.setMarkdown(QStringLiteral("hello world"));
        QCOMPARE(doc.wordCount(), 2);

        doc.setMarkdown(QStringLiteral("one two three four five"));
        QCOMPARE(doc.wordCount(), 5);
    }

    void testWordCountHandlesMarkdown()
    {
        Corbomite::NoteDocument doc(QStringLiteral("/vault"), QStringLiteral("note.md"));

        doc.setMarkdown(QStringLiteral("# Heading\n\nSome **bold** text."));
        // Words: Heading, Some, bold, text = 4
        // # and ** are not words
        QCOMPARE(doc.wordCount(), 4);
    }

    void testCharacterCount()
    {
        Corbomite::NoteDocument doc(QStringLiteral("/vault"), QStringLiteral("note.md"));

        QCOMPARE(doc.characterCount(), 0);

        doc.setMarkdown(QStringLiteral("hello"));
        QCOMPARE(doc.characterCount(), 5);
    }

    void testEmptyDocument()
    {
        Corbomite::NoteDocument doc(QStringLiteral("/vault"), QStringLiteral("note.md"));
        doc.setMarkdown(QString());

        QCOMPARE(doc.wordCount(), 0);
        QCOMPARE(doc.characterCount(), 0);
    }

    void testFilePath()
    {
        Corbomite::NoteDocument doc(QStringLiteral("/home/user/vault"), QStringLiteral("folder/note.md"));

        QCOMPARE(doc.filePath(), QStringLiteral("/home/user/vault/folder/note.md"));
        QCOMPARE(doc.relativePath(), QStringLiteral("folder/note.md"));
        QCOMPARE(doc.name(), QStringLiteral("note"));
    }

    // --- Wrapper API tests (Phase C3, Task 20) ---

    void markoffAccessor_isNonNull()
    {
        Corbomite::NoteDocument note(QDir::tempPath(), QStringLiteral("t.md"));
        QVERIFY(note.markoff() != nullptr);
    }

    void markdownDelegates_toMarkoffDocument()
    {
        Corbomite::NoteDocument note(QDir::tempPath(), QStringLiteral("t.md"));
        note.markoff()->resetContent(QStringLiteral("# hello"), Markoff::Origin::FirstOpen);
        QCOMPARE(note.markdown(), QStringLiteral("# hello"));
    }

    void textChanged_firesOnCanonicalChange()
    {
        Corbomite::NoteDocument note(QDir::tempPath(), QStringLiteral("t.md"));
        QSignalSpy spy(&note, &Corbomite::NoteDocument::textChanged);
        note.markoff()->resetContent(QStringLiteral("x"), Markoff::Origin::FirstOpen);
        QCOMPARE(spy.count(), 1);
    }

    void modifiedFlag_flipsOnEdit()
    {
        Corbomite::NoteDocument note(QDir::tempPath(), QStringLiteral("t.md"));
        note.markoff()->resetContent(QStringLiteral("base"), Markoff::Origin::FirstOpen);
        note.setModified(false);
        QSignalSpy modSpy(&note, &Corbomite::NoteDocument::modificationChanged);
        note.markoff()->undoStack()->push(
            new Markoff::MarkdownDelta(note.markoff(), 4, 0, QStringLiteral(" edited")));
        QVERIFY(note.isModified());
        QCOMPARE(modSpy.count(), 1);
    }
};

QTEST_MAIN(TestNoteDocument)
#include "tst_notedocument.moc"
