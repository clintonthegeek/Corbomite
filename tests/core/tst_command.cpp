// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for Corbomite::Command + CommandRegistry (Cluster C Phase 3).
// Spec: docs/obsidian-audit/domains/plugin.md §10 + domains/core.md §7
//
// Obsidian contract:
//   Command {
//     id, name, icon?, mobileOnly?, hotkeys?,
//     callback?            ()=>void
//     checkCallback?       (checking:bool)=>bool         // dual-mode
//     editorCallback?      (editor,view)=>void
//     editorCheckCallback? (checking:bool,editor,view)=>bool
//   }
//
// check variants allow the command palette to grey out unavailable
// commands (call with checking=true — return value drives enabled state,
// side effects forbidden; call with checking=false to actually execute).

#include <QTest>
#include <QObject>

#include "corbomite/core/Command.h"

using Corbomite::Command;
using Corbomite::CommandRegistry;
using Corbomite::EditorLike;

namespace {

// A stand-in for the Obsidian `editor` parameter. Cluster G will make
// this a real type; for now any opaque pointer works.
struct DummyEditor { int data = 0; };

} // namespace

class TestCommand : public QObject {
    Q_OBJECT

private Q_SLOTS:
    // ---- Registry basics --------------------------------------------------

    void testAddAndFind()
    {
        CommandRegistry reg;
        Command cmd;
        cmd.id = QStringLiteral("app:hello");
        cmd.name = QStringLiteral("Hello");
        cmd.callback = []() {};
        reg.addCommand(cmd);

        auto *found = reg.findCommand(QStringLiteral("app:hello"));
        QVERIFY(found);
        QCOMPARE(found->name, QStringLiteral("Hello"));
    }

    void testAddReplacesExistingWithSameId()
    {
        CommandRegistry reg;
        Command a;
        a.id = QStringLiteral("app:same");
        a.name = QStringLiteral("First");
        a.callback = []() {};
        reg.addCommand(a);

        Command b;
        b.id = QStringLiteral("app:same");
        b.name = QStringLiteral("Second");
        b.callback = []() {};
        reg.addCommand(b);

        QCOMPARE(reg.listCommands().size(), 1);
        QCOMPARE(reg.findCommand(QStringLiteral("app:same"))->name,
                 QStringLiteral("Second"));
    }

    void testRemove()
    {
        CommandRegistry reg;
        Command cmd;
        cmd.id = QStringLiteral("app:x");
        cmd.callback = []() {};
        reg.addCommand(cmd);
        QVERIFY(reg.removeCommand(QStringLiteral("app:x")));
        QCOMPARE(reg.listCommands().size(), 0);
        QVERIFY(!reg.removeCommand(QStringLiteral("app:x"))); // idempotent
    }

    void testListCommandsReturnsAll()
    {
        CommandRegistry reg;
        for (int i = 0; i < 3; ++i) {
            Command c;
            c.id = QStringLiteral("c%1").arg(i);
            c.callback = []() {};
            reg.addCommand(c);
        }
        QCOMPARE(reg.listCommands().size(), 3);
    }

    // ---- Execute: SimpleCallback -----------------------------------------

    void testExecuteSimpleCallback()
    {
        CommandRegistry reg;
        int fired = 0;
        Command cmd;
        cmd.id = QStringLiteral("app:go");
        cmd.callback = [&]() { ++fired; };
        reg.addCommand(cmd);

        QVERIFY(reg.executeById(QStringLiteral("app:go")));
        QCOMPARE(fired, 1);
    }

    void testExecuteUnknownIdReturnsFalse()
    {
        CommandRegistry reg;
        QVERIFY(!reg.executeById(QStringLiteral("nope")));
    }

    // ---- Execute: CheckCallback ------------------------------------------

    void testCheckCallbackAvailability()
    {
        CommandRegistry reg;
        bool available = false;
        int runs = 0;
        Command cmd;
        cmd.id = QStringLiteral("app:maybe");
        cmd.checkCallback = [&](bool checking) {
            if (checking) return available;
            ++runs;
            return true;
        };
        reg.addCommand(cmd);

        // Not available → executeById returns false, runs not incremented.
        QVERIFY(!reg.executeById(QStringLiteral("app:maybe")));
        QCOMPARE(runs, 0);

        available = true;
        QVERIFY(reg.executeById(QStringLiteral("app:maybe")));
        QCOMPARE(runs, 1);
    }

    void testIsAvailableReflectsCheckCallback()
    {
        CommandRegistry reg;
        bool available = true;
        Command cmd;
        cmd.id = QStringLiteral("app:maybe");
        cmd.checkCallback = [&](bool checking) {
            if (checking) return available;
            return true;
        };
        reg.addCommand(cmd);

        QVERIFY(reg.isAvailable(QStringLiteral("app:maybe")));
        available = false;
        QVERIFY(!reg.isAvailable(QStringLiteral("app:maybe")));
    }

    // ---- Execute: EditorCallback / EditorCheckCallback ------------------

    void testEditorCallbackReceivesEditor()
    {
        CommandRegistry reg;
        DummyEditor editor;
        editor.data = 42;

        void *seen = nullptr;
        Command cmd;
        cmd.id = QStringLiteral("editor:act");
        cmd.editorCallback = [&](EditorLike ed) {
            seen = ed;
        };
        reg.addCommand(cmd);

        // Without an editor set, editor-only command can't run.
        QVERIFY(!reg.executeById(QStringLiteral("editor:act")));
        QVERIFY(!seen);

        reg.setActiveEditor(&editor);
        QVERIFY(reg.executeById(QStringLiteral("editor:act")));
        QCOMPARE(seen, &editor);
    }

    void testEditorCheckCallbackGatesOnEditor()
    {
        CommandRegistry reg;
        DummyEditor editor;
        int runs = 0;
        Command cmd;
        cmd.id = QStringLiteral("editor:chk");
        cmd.editorCheckCallback = [&](bool checking, EditorLike ed) {
            if (!ed) return false;
            if (checking) return true;
            ++runs;
            return true;
        };
        reg.addCommand(cmd);

        // No active editor.
        QVERIFY(!reg.isAvailable(QStringLiteral("editor:chk")));
        QVERIFY(!reg.executeById(QStringLiteral("editor:chk")));
        QCOMPARE(runs, 0);

        reg.setActiveEditor(&editor);
        QVERIFY(reg.isAvailable(QStringLiteral("editor:chk")));
        QVERIFY(reg.executeById(QStringLiteral("editor:chk")));
        QCOMPARE(runs, 1);
    }

    // ---- listAvailable filters by isAvailable ---------------------------

    void testListAvailableFiltersUnavailable()
    {
        CommandRegistry reg;

        Command a;
        a.id = QStringLiteral("a");
        a.callback = []() {};
        reg.addCommand(a);

        Command b;
        b.id = QStringLiteral("b");
        b.checkCallback = [](bool checking) { return checking ? false : true; };
        reg.addCommand(b);

        auto avail = reg.listAvailable();
        QCOMPARE(avail.size(), 1);
        QCOMPARE(avail.first()->id, QStringLiteral("a"));
    }
};

QTEST_MAIN(TestCommand)
#include "tst_command.moc"
