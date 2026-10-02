// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QClipboard>
#include <QMenu>
#include <QTemporaryDir>
#include <QTest>

#include "corbomite/core/EditableFileView.h"
#include "corbomite/core/MenuSectionHelper.h"
#include "corbomite/core/NoteDocument.h"

using namespace Corbomite;

// Concrete leaf-less subclass (View has pure virtuals for
// getViewType / getDisplayText). Null `WorkspaceLeaf` is fine — the
// hamburger-menu wiring under test doesn't deref the leaf.
class TestEditableView : public EditableFileView
{
public:
    TestEditableView() : EditableFileView(nullptr, nullptr) {}
    QString getViewType() const override { return QStringLiteral("test-editable"); }
    QString getDisplayText() const override { return QStringLiteral("Test"); }
};

class TestEditableFileViewMenu : public QObject
{
    Q_OBJECT
private slots:
    void testNoMenuWhenFileMissing();
    void testAllSectionsPopulatedWhenFilePresent();
    void testRenameCallbackFiresWithFileAndParent();
    void testCopyObsidianUrlPutsUrlOnClipboard();
    void testCopyVaultRelativePutsRelPathOnClipboard();
    void testCommandDispatcherReceivesRevealCommand();
    void testDisabledPlaceholdersHaveTooltips();
    void testBookmarkCallbackEnablesMenuEntry();
};

namespace {

NoteDocument *makeDoc(QObject *parent, const QString &vaultRoot,
                      const QString &relPath)
{
    return new NoteDocument(vaultRoot, relPath, parent);
}

}  // namespace

void TestEditableFileViewMenu::testNoMenuWhenFileMissing()
{
    TestEditableView view;
    QMenu menu;
    MenuSectionHelper helper(&menu);
    view.onMoreOptionsMenu(helper);
    helper.finalize();
    // No document loaded — view only contributes whatever View::onMoreOptionsMenu
    // does (nothing today), so menu is empty.
    QCOMPARE(menu.actions().size(), 0);
}

void TestEditableFileViewMenu::testAllSectionsPopulatedWhenFilePresent()
{
    QTemporaryDir tmp;
    TestEditableView view;
    QScopedPointer<NoteDocument> doc(
        makeDoc(nullptr, tmp.path(), QStringLiteral("foo.md")));
    QVERIFY(view.loadFile(doc.data()));

    QMenu menu;
    MenuSectionHelper helper(&menu);
    view.onMoreOptionsMenu(helper);
    helper.finalize();

    // Collect all action texts (flatten submenu children).
    QStringList texts;
    for (QAction *a : menu.actions()) {
        if (a->menu()) {
            for (QAction *child : a->menu()->actions())
                texts << child->text();
            texts << a->text();
        } else {
            texts << a->text();
        }
    }

    // Every expected item from the Cluster R canonical list is present.
    QVERIFY(texts.contains(QStringLiteral("Rename...")));
    QVERIFY(texts.contains(QStringLiteral("Move file to...")));
    QVERIFY(texts.contains(QStringLiteral("Version history")));
    QVERIFY(texts.contains(QStringLiteral("Copy path")));
    QVERIFY(texts.contains(QStringLiteral("Copy Obsidian URL")));
    QVERIFY(texts.contains(QStringLiteral("Copy Corbomite URL")));
    QVERIFY(texts.contains(QStringLiteral("Copy vault-relative path")));
    QVERIFY(texts.contains(QStringLiteral("Open in default app")));
    QVERIFY(texts.contains(QStringLiteral("Show in folder")));
    QVERIFY(texts.contains(QStringLiteral("Reveal file in navigation")));
    QVERIFY(texts.contains(QStringLiteral("Delete")));
    QVERIFY(texts.contains(QStringLiteral("Open in new window")));
    QVERIFY(texts.contains(QStringLiteral("Bookmark…")));
}

void TestEditableFileViewMenu::testRenameCallbackFiresWithFileAndParent()
{
    QTemporaryDir tmp;
    TestEditableView view;
    QScopedPointer<NoteDocument> doc(
        makeDoc(nullptr, tmp.path(), QStringLiteral("foo.md")));
    QVERIFY(view.loadFile(doc.data()));

    bool called = false;
    NoteDocument *seenDoc = nullptr;
    QWidget *seenParent = nullptr;
    view.setRenameCallback([&](NoteDocument *d, QWidget *parent) {
        called = true;
        seenDoc = d;
        seenParent = parent;
    });

    QMenu menu;
    MenuSectionHelper helper(&menu);
    view.onMoreOptionsMenu(helper);
    helper.finalize();

    QAction *rename = nullptr;
    for (QAction *a : menu.actions()) {
        if (a->text() == QStringLiteral("Rename...")) { rename = a; break; }
    }
    QVERIFY(rename);
    rename->trigger();

    QVERIFY(called);
    QCOMPARE(seenDoc, doc.data());
    QCOMPARE(seenParent, &view);
}

void TestEditableFileViewMenu::testCopyObsidianUrlPutsUrlOnClipboard()
{
    QTemporaryDir tmp;
    TestEditableView view;
    QScopedPointer<NoteDocument> doc(
        makeDoc(nullptr, tmp.path(), QStringLiteral("notes/foo.md")));
    QVERIFY(view.loadFile(doc.data()));

    view.setVaultNameResolver([]() { return QStringLiteral("my-vault"); });

    QMenu menu;
    MenuSectionHelper helper(&menu);
    view.onMoreOptionsMenu(helper);
    helper.finalize();

    // Walk into the "Copy path" submenu and trigger "Copy Obsidian URL".
    QAction *target = nullptr;
    for (QAction *a : menu.actions()) {
        if (a->menu()) {
            for (QAction *c : a->menu()->actions()) {
                if (c->text() == QStringLiteral("Copy Obsidian URL")) {
                    target = c;
                    break;
                }
            }
        }
    }
    QVERIFY(target);
    target->trigger();

    QCOMPARE(QApplication::clipboard()->text(),
             QStringLiteral(
                 "obsidian://open?vault=my-vault&file=notes%2Ffoo.md"));
}

void TestEditableFileViewMenu::testCopyVaultRelativePutsRelPathOnClipboard()
{
    QTemporaryDir tmp;
    TestEditableView view;
    QScopedPointer<NoteDocument> doc(
        makeDoc(nullptr, tmp.path(), QStringLiteral("deep/nested/bar.md")));
    QVERIFY(view.loadFile(doc.data()));

    QMenu menu;
    MenuSectionHelper helper(&menu);
    view.onMoreOptionsMenu(helper);
    helper.finalize();

    QAction *target = nullptr;
    for (QAction *a : menu.actions()) {
        if (a->menu()) {
            for (QAction *c : a->menu()->actions()) {
                if (c->text() == QStringLiteral("Copy vault-relative path")) {
                    target = c;
                    break;
                }
            }
        }
    }
    QVERIFY(target);
    target->trigger();
    QCOMPARE(QApplication::clipboard()->text(),
             QStringLiteral("deep/nested/bar.md"));
}

void TestEditableFileViewMenu::testCommandDispatcherReceivesRevealCommand()
{
    QTemporaryDir tmp;
    TestEditableView view;
    QScopedPointer<NoteDocument> doc(
        makeDoc(nullptr, tmp.path(), QStringLiteral("foo.md")));
    QVERIFY(view.loadFile(doc.data()));

    QString seenCommand;
    view.setCommandDispatcher([&](const QString &cmd) { seenCommand = cmd; });

    QMenu menu;
    MenuSectionHelper helper(&menu);
    view.onMoreOptionsMenu(helper);
    helper.finalize();

    QAction *reveal = nullptr;
    for (QAction *a : menu.actions()) {
        if (a->text() == QStringLiteral("Reveal file in navigation")) {
            reveal = a;
            break;
        }
    }
    QVERIFY(reveal);
    reveal->trigger();
    QCOMPARE(seenCommand, QStringLiteral("file-explorer:reveal-file"));
}

void TestEditableFileViewMenu::testDisabledPlaceholdersHaveTooltips()
{
    QTemporaryDir tmp;
    TestEditableView view;
    QScopedPointer<NoteDocument> doc(
        makeDoc(nullptr, tmp.path(), QStringLiteral("foo.md")));
    QVERIFY(view.loadFile(doc.data()));

    QMenu menu;
    MenuSectionHelper helper(&menu);
    view.onMoreOptionsMenu(helper);
    helper.finalize();

    QAction *newWin = nullptr;
    QAction *versionHistory = nullptr;
    QAction *bookmark = nullptr;
    auto walk = [&](QList<QAction *> acts) {
        for (QAction *a : acts) {
            if (a->text() == QStringLiteral("Open in new window")) newWin = a;
            else if (a->text() == QStringLiteral("Version history")) versionHistory = a;
            else if (a->text() == QStringLiteral("Bookmark…")) bookmark = a;
        }
    };
    walk(menu.actions());
    for (QAction *a : menu.actions()) {
        if (a->menu()) walk(a->menu()->actions());
    }

    QVERIFY(newWin);
    QVERIFY(versionHistory);
    QVERIFY(bookmark);
    QVERIFY(!newWin->isEnabled());
    QVERIFY(!versionHistory->isEnabled());
    QVERIFY(!bookmark->isEnabled());
    QVERIFY(!newWin->toolTip().isEmpty());
    QVERIFY(!versionHistory->toolTip().isEmpty());
    QVERIFY(!bookmark->toolTip().isEmpty());
}

void TestEditableFileViewMenu::testBookmarkCallbackEnablesMenuEntry()
{
    QTemporaryDir tmp;
    TestEditableView view;
    QScopedPointer<NoteDocument> doc(
        makeDoc(nullptr, tmp.path(), QStringLiteral("foo.md")));
    QVERIFY(view.loadFile(doc.data()));

    bool called = false;
    QString seen;
    view.setBookmarkCallback([&](NoteDocument *d, QWidget *) {
        called = true;
        if (d) seen = d->relativePath();
    });

    QMenu menu;
    MenuSectionHelper helper(&menu);
    view.onMoreOptionsMenu(helper);
    helper.finalize();

    QAction *bookmark = nullptr;
    auto walk = [&](QList<QAction *> acts) {
        for (QAction *a : acts) {
            if (a->text() == QStringLiteral("Bookmark…")) bookmark = a;
        }
    };
    walk(menu.actions());
    for (QAction *a : menu.actions())
        if (a->menu()) walk(a->menu()->actions());

    QVERIFY(bookmark);
    QVERIFY(bookmark->isEnabled());
    bookmark->trigger();
    QVERIFY(called);
    QCOMPARE(seen, QStringLiteral("foo.md"));
}

QTEST_MAIN(TestEditableFileViewMenu)
#include "tst_editable_file_view_menu.moc"
