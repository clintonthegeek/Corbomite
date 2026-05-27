// SPDX-License-Identifier: GPL-3.0-or-later
//
// Regression coverage for NoteDocument's modified-flag watermark.
//
// MarkoffDocument schedules its `d2DocumentChanged` notification via
// QTimer::singleShot(0) (one event-loop iteration later). A document that is
// edited and then saved synchronously (e.g. Ctrl+S right after typing) clears
// its modified flag in the same turn, but the *stale* deferred change signal
// is still queued. Before the watermark gate in NoteDocument::setModified, that
// signal fired during the next event loop and re-dirtied already-saved content
// — surfaced as tst_e2e_gui::testSaveShortcut ("Document still modified after
// save"). This test pins the unit-level behavior so it can't regress.

#include <QTest>
#include <QSignalSpy>

#include "corbomite/core/NoteDocument.h"

class TestNoteDocumentDirty : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void cleanAfterImmediateSaveDespiteDeferredChange()
    {
        Corbomite::NoteDocument doc(QStringLiteral("/vault"), QStringLiteral("note.md"));

        // Edit: resetContent emits documentReloaded synchronously (→ dirty) and
        // schedules a deferred d2DocumentChanged.
        doc.setMarkdown(QStringLiteral("hello"));
        QVERIFY(doc.isModified());

        // Save: mark clean. Records the edit-sequence watermark.
        doc.setModified(false);
        QVERIFY(!doc.isModified());

        // Pump the event loop so the deferred d2DocumentChanged from the edit
        // above fires. It must NOT re-dirty content that is already saved.
        QTest::qWait(20);
        QVERIFY2(!doc.isModified(),
                 "deferred d2DocumentChanged re-dirtied an already-saved document");
    }

    void genuineEditAfterSaveStillDirties()
    {
        Corbomite::NoteDocument doc(QStringLiteral("/vault"), QStringLiteral("note.md"));
        doc.setMarkdown(QStringLiteral("hello"));
        doc.setModified(false);
        QTest::qWait(20);
        QVERIFY(!doc.isModified());

        QSignalSpy spy(&doc, &Corbomite::NoteDocument::modificationChanged);
        doc.setMarkdown(QStringLiteral("hello world"));   // a real edit past the watermark
        QVERIFY(doc.isModified());
        QVERIFY(spy.count() >= 1);
        QCOMPARE(spy.last().at(0).toBool(), true);
    }
};

QTEST_MAIN(TestNoteDocumentDirty)
#include "tst_notedocument_dirty.moc"
