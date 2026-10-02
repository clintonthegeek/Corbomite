// SPDX-License-Identifier: GPL-3.0-or-later
//
// Cluster R Task 3.2 — MarkdownView::insertFrontmatterProperty creates or
// extends the YAML frontmatter block of the active NoteDocument. Flips the
// view out of Reading mode into LivePreview so the new row is visible.

#include "MarkdownView.h"
#include "NoteEditorWidget.h"

#include "corbomite/core/MenuSectionHelper.h"
#include "corbomite/core/NoteDocument.h"
#include "corbomite/core/ViewRegistry.h"
#include "corbomite/core/WorkspaceLeaf.h"

#include <QMenu>
#include <QStringList>
#include <QTest>

using Corbomite::MarkdownView;
using Corbomite::findClosingFrontmatterFence;
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
    void insertFrontmatterPropertyIgnoresLongerDashRuns();
    void insertFrontmatterPropertyIgnoresDashPrefixedProse();
    void insertFrontmatterPropertyHandlesCrlf();
    void insertFrontmatterPropertyIgnoresThematicBreak();
    void insertFrontmatterPropertyFindsTrailingFenceAtEof();
    void insertFrontmatterPropertyFindsOpeningFenceUnclosed();
    void onMoreOptionsMenuDispatchesAddPropertyCommand();
    void onMoreOptionsMenuContributesViewLinkedSubmenu();
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

// ---- P1: the closing-fence scan matched non-fences --------------------
//
// MarkdownView::insertFrontmatterProperty located the closing fence with
// body.indexOf("\n---", 3), which also matched a LONGER run of dashes and a
// line merely PREFIXED with dashes. The failure mode is bad: a bogus ": " row
// gets spliced into the middle of someone's prose.
//
// These test the SCAN directly. An end-to-end test through a live
// NoteDocument would not be testing this code: Markoff's own frontmatter
// parser independently mangles a `----` line, so the round-trip result is
// already wrong before this scan runs, and the assertion fails for an
// unrelated reason. See punch-list [editor][P1] for the Markoff half.

void TestMarkdownView::insertFrontmatterPropertyIgnoresLongerDashRuns()
{
    const QString body = QStringLiteral("---\ntitle: Foo\n----\n---\nBody\n");
    // The closing fence is the standalone `---`, NOT the `----` above it.
    // (With only the `----` and no real fence, returning -1 is also correct —
    // covered separately by FindsOpeningFenceUnclosed.)
    const int idx = findClosingFrontmatterFence(body);
    QVERIFY2(idx > 0, "no closing fence found");
    QCOMPARE(body.mid(idx, 4), QStringLiteral("\n---"));
    QVERIFY2(idx > body.indexOf(QStringLiteral("----")),
             "the longer dash run was mistaken for the closing fence");

    // And the strong form: a body whose only candidate is a longer dash run
    // has NO closing fence, rather than one at the dash run.
    QCOMPARE(findClosingFrontmatterFence(QStringLiteral("---\ntitle: Foo\n----\nBody\n")), -1);
}

void TestMarkdownView::insertFrontmatterPropertyIgnoresDashPrefixedProse()
{
    const QString body = QStringLiteral("---\ntitle: Foo\n---not a fence\n---\nBody\n");
    const int idx = findClosingFrontmatterFence(body);
    QVERIFY(idx > 0);
    QCOMPARE(body.mid(idx, 4), QStringLiteral("\n---"));
    QVERIFY2(!body.mid(idx, 12).startsWith(QStringLiteral("\n---not")),
             "dash-prefixed prose was mistaken for the closing fence");
}

// A thematic break (`--- ` with trailing space) is not a fence either.
void TestMarkdownView::insertFrontmatterPropertyIgnoresThematicBreak()
{
    const QString body = QStringLiteral("---\ntitle: Foo\n--- \n---\nBody\n");
    const int idx = findClosingFrontmatterFence(body);
    QVERIFY(idx > 0);
    QVERIFY2(!body.mid(idx, 5).startsWith(QStringLiteral("\n--- ")),
             "a thematic break was mistaken for the closing fence");
}

void TestMarkdownView::insertFrontmatterPropertyHandlesCrlf()
{
    const QString body = QStringLiteral("---\r\ntitle: Foo\r\n---\r\nBody\r\n");
    const int idx = findClosingFrontmatterFence(body);
    QVERIFY2(idx > 0, "CRLF document reported no closing fence");
    QCOMPARE(body.mid(idx + 1, 3), QStringLiteral("---"));
    // Must land on a standalone fence, i.e. followed by CR, not by content.
    QCOMPARE(body.at(idx + 4), QLatin1Char('\r'));
}

// A fence at the very END of the body, with no trailing newline, is valid:
// the terminator check must accept end-of-body, not just a following newline.
void TestMarkdownView::insertFrontmatterPropertyFindsTrailingFenceAtEof()
{
    const QString body = QStringLiteral("---\ntitle: Foo\n---");
    const int idx = findClosingFrontmatterFence(body);
    QCOMPARE(idx, body.size() - 4);
}

void TestMarkdownView::insertFrontmatterPropertyFindsOpeningFenceUnclosed()
{
    // No closing fence at all -> -1, which the caller turns into "append a
    // fresh block". Must NOT report the opening fence's own position.
    QCOMPARE(findClosingFrontmatterFence(QStringLiteral("---\ntitle: Foo\n")), -1);
    QCOMPARE(findClosingFrontmatterFence(QStringLiteral("")), -1);
}

void TestMarkdownView::onMoreOptionsMenuDispatchesAddPropertyCommand()
{
    NoteDocument doc(QStringLiteral("/tmp/vault"), QStringLiteral("note.md"));
    doc.setMarkdown(QStringLiteral("body\n"));

    ViewRegistry registry;
    WorkspaceLeaf leaf(&registry);
    MarkdownView view(&leaf);
    view.editorWidget()->setNoteDocument(&doc);

    QStringList dispatched;
    view.setMarkdownCommandDispatcher([&](const QString &id) {
        dispatched << id;
    });

    QMenu menu;
    Corbomite::MenuSectionHelper helper(&menu);
    view.onMoreOptionsMenu(helper);
    helper.finalize();

    // Find the "Add file property" action and trigger it.
    QAction *target = nullptr;
    for (QAction *a : menu.actions()) {
        if (a->text().contains(QStringLiteral("Add file property"))) {
            target = a;
            break;
        }
    }
    QVERIFY2(target != nullptr, "Add file property action not found");
    target->trigger();
    QCOMPARE(dispatched,
             QStringList{QStringLiteral("markdown:add-metadata-property")});
}

void TestMarkdownView::onMoreOptionsMenuContributesViewLinkedSubmenu()
{
    NoteDocument doc(QStringLiteral("/tmp/vault"), QStringLiteral("note.md"));
    doc.setMarkdown(QStringLiteral("body\n"));

    ViewRegistry registry;
    WorkspaceLeaf leaf(&registry);
    MarkdownView view(&leaf);
    view.editorWidget()->setNoteDocument(&doc);

    QStringList dispatched;
    view.setMarkdownCommandDispatcher([&](const QString &id) {
        dispatched << id;
    });

    QMenu menu;
    Corbomite::MenuSectionHelper helper(&menu);
    view.onMoreOptionsMenu(helper);
    helper.finalize();

    // Locate the submenu by title.
    QMenu *linkedMenu = nullptr;
    for (QAction *a : menu.actions()) {
        if (a->menu() && a->text().contains(QStringLiteral("Open linked view"))) {
            linkedMenu = a->menu();
            break;
        }
    }
    QVERIFY2(linkedMenu != nullptr, "Open linked view submenu missing");

    // Trigger "Open backlinks".
    QAction *backlinksAct = nullptr;
    for (QAction *a : linkedMenu->actions()) {
        if (a->text().contains(QStringLiteral("backlinks"))) {
            backlinksAct = a;
            break;
        }
    }
    QVERIFY2(backlinksAct != nullptr, "Open backlinks action missing");
    backlinksAct->trigger();
    QVERIFY(dispatched.contains(
        QStringLiteral("corbomite-backlinks:open")));
}

QTEST_MAIN(TestMarkdownView)
#include "tst_markdown_view.moc"
