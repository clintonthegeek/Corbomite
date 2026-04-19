// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster R Task 3.2 — MarkdownView::insertFrontmatterProperty creates or
// extends the YAML frontmatter block of the active NoteDocument. Flips the
// view out of Reading mode into LivePreview so the new row is visible.

#include "MarkdownView.h"
#include "NoteEditorWidget.h"

#include "corbomite/core/NoteDocument.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/WorkspaceLeaf.h"

#include <QTest>

using Corbomite::MarkdownView;
using Corbomite::NoteDocument;
using Corbomite::NoteEditorWidget;
using Corbomite::ViewRegistry;
using Corbomite::WorkspaceLeaf;

class TestMarkdownView : public QObject
{
    Q_OBJECT

private slots:
    void insertFrontmatterPropertyCreatesBlockIfMissing();
    void insertFrontmatterPropertyAppendsWhenBlockExists();
};

void TestMarkdownView::insertFrontmatterPropertyCreatesBlockIfMissing()
{
    // NoteDocument must outlive MarkdownView — NoteEditorWidget holds a raw
    // pointer to it and touches it during widget destruction.
    NoteDocument doc(QStringLiteral("/tmp/vault"), QStringLiteral("note.md"));
    doc.setMarkdown(QStringLiteral("# Title\n\nBody text.\n"));

    ViewRegistry registry;
    WorkspaceLeaf leaf(&registry);
    MarkdownView view(&leaf);
    view.editorWidget()->setNoteDocument(&doc);

    view.insertFrontmatterProperty();

    const QString out = doc.markdown();
    QVERIFY2(out.startsWith(QStringLiteral("---\n")),
             qPrintable(QStringLiteral("got: %1").arg(out)));
    // Original body preserved after the new frontmatter block.
    QVERIFY(out.contains(QStringLiteral("# Title")));
    QVERIFY(out.contains(QStringLiteral("Body text.")));
}

void TestMarkdownView::insertFrontmatterPropertyAppendsWhenBlockExists()
{
    NoteDocument doc(QStringLiteral("/tmp/vault"), QStringLiteral("note.md"));
    doc.setMarkdown(QStringLiteral("---\ntitle: Foo\n---\nBody\n"));

    ViewRegistry registry;
    WorkspaceLeaf leaf(&registry);
    MarkdownView view(&leaf);
    view.editorWidget()->setNoteDocument(&doc);

    const int beforeLines =
        doc.markdown().count(QLatin1Char('\n'));

    view.insertFrontmatterProperty();

    const QString out = doc.markdown();
    // Body preserved.
    QVERIFY(out.contains(QStringLiteral("title: Foo")));
    QVERIFY(out.contains(QStringLiteral("Body")));
    // New row added to the existing block (exactly one extra \n).
    const int afterLines = out.count(QLatin1Char('\n'));
    QCOMPARE(afterLines, beforeLines + 1);
    // The closing fence still follows the appended row (no second --- block).
    QCOMPARE(out.count(QStringLiteral("\n---")), 1);
}

QTEST_MAIN(TestMarkdownView)
#include "tst_markdown_view.moc"
