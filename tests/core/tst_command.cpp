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

    // ---- Aliases ---------------------------------------------------------
    //
    // `.obsidian/hotkeys.json` is keyed by Obsidian command id, so a vault can
    // only bind an id Corbomite registers. Corbomite's built-ins predate that
    // and are named `corbomite-<plugin>:open`. An alias closes the gap without
    // a second callback and without a second palette row.

    void testAliasExecutesTargetCallback()
    {
        CommandRegistry reg;
        int runs = 0;

        Command c;
        c.id = QStringLiteral("corbomite-outline:open");
        c.name = QStringLiteral("Open outline");
        c.callback = [&runs] { ++runs; };
        reg.addCommand(c);

        QVERIFY(reg.addAlias(QStringLiteral("outline:open"),
                             QStringLiteral("corbomite-outline:open")));
        QVERIFY(reg.isAlias(QStringLiteral("outline:open")));
        QVERIFY(!reg.isAlias(QStringLiteral("corbomite-outline:open")));

        // Both ids are dispatchable.
        QVERIFY(reg.executeById(QStringLiteral("outline:open")));
        QCOMPARE(runs, 1);
        QVERIFY(reg.executeById(QStringLiteral("corbomite-outline:open")));
        QCOMPARE(runs, 2);
    }

    void testAliasInheritsTargetAvailability()
    {
        CommandRegistry reg;
        bool available = false;

        Command c;
        c.id = QStringLiteral("corbomite-x:go");
        c.checkCallback = [&available](bool checking) {
            if (!checking) return true;
            return available;
        };
        reg.addCommand(c);
        reg.addAlias(QStringLiteral("x:go"), QStringLiteral("corbomite-x:go"));

        QVERIFY(!reg.isAvailable(QStringLiteral("x:go")));
        QVERIFY(!reg.executeById(QStringLiteral("x:go")));

        available = true;
        QVERIFY(reg.isAvailable(QStringLiteral("x:go")));
        QVERIFY(reg.executeById(QStringLiteral("x:go")));
    }

    void testAliasSharesEditorGateWithTarget()
    {
        CommandRegistry reg;
        int runs = 0;
        DummyEditor editor;

        Command c;
        c.id = QStringLiteral("corbomite-x:bold");
        // The gate a real editor command carries: no active editor, no command.
        c.editorCheckCallback = [&runs](bool checking, EditorLike e) {
            if (checking) return e != nullptr;
            ++runs;
            return true;
        };
        reg.addCommand(c);
        reg.addAlias(QStringLiteral("editor:toggle-bold"),
                     QStringLiteral("corbomite-x:bold"));

        // No active editor → unavailable under *either* id.
        QVERIFY(!reg.isAvailable(QStringLiteral("editor:toggle-bold")));
        QVERIFY(!reg.executeById(QStringLiteral("editor:toggle-bold")));
        QCOMPARE(runs, 0);

        reg.setActiveEditor(&editor);
        QVERIFY(reg.isAvailable(QStringLiteral("editor:toggle-bold")));
        QVERIFY(reg.executeById(QStringLiteral("editor:toggle-bold")));
        QCOMPARE(runs, 1);
    }

    void testAliasIsNotListedInPalette()
    {
        CommandRegistry reg;

        Command c;
        c.id = QStringLiteral("corbomite-bookmarks:open");
        c.callback = []() {};
        reg.addCommand(c);
        reg.addAlias(QStringLiteral("bookmarks:open"),
                     QStringLiteral("corbomite-bookmarks:open"));

        // Both ids resolve...
        QVERIFY(reg.findCommand(QStringLiteral("bookmarks:open")));
        QVERIFY(reg.findCommand(QStringLiteral("corbomite-bookmarks:open")));
        QCOMPARE(reg.listCommands().size(), 2);
        // ...but the palette shows one row, under the Obsidian id.
        const auto avail = reg.listAvailable();
        QCOMPARE(avail.size(), 1);
        QCOMPARE(avail.first()->id, QStringLiteral("corbomite-bookmarks:open"));
    }

    void testAliasCarriesTargetLabelAndIcon()
    {
        CommandRegistry reg;

        Command c;
        c.id = QStringLiteral("corbomite-outline:open");
        c.name = QStringLiteral("Open outline");
        c.icon = QStringLiteral("view-list-tree");
        reg.addCommand(c);
        reg.addAlias(QStringLiteral("outline:open"),
                     QStringLiteral("corbomite-outline:open"));

        const auto *a = reg.findCommand(QStringLiteral("outline:open"));
        QVERIFY(a);
        QCOMPARE(a->name, QStringLiteral("Open outline"));
        QCOMPARE(a->icon, QStringLiteral("view-list-tree"));
        // An alias carries no callbacks of its own — it resolves at dispatch.
        QVERIFY(!a->callback);
    }

    void testAddAliasRejections()
    {
        CommandRegistry reg;

        Command c;
        c.id = QStringLiteral("corbomite-x:open");
        c.callback = []() {};
        reg.addCommand(c);

        // Unknown target.
        QVERIFY(!reg.addAlias(QStringLiteral("a:open"), QStringLiteral("nope")));
        // Empty or self-referential alias id.
        QVERIFY(!reg.addAlias(QString(), QStringLiteral("corbomite-x:open")));
        QVERIFY(!reg.addAlias(QStringLiteral("corbomite-x:open"),
                              QStringLiteral("corbomite-x:open")));
        // Alias id already occupied by a real command.
        Command other;
        other.id = QStringLiteral("taken");
        other.callback = []() {};
        reg.addCommand(other);
        QVERIFY(!reg.addAlias(QStringLiteral("taken"),
                              QStringLiteral("corbomite-x:open")));
        QVERIFY(!reg.isAlias(QStringLiteral("taken")));
        // Nor can a second alias silently re-target the first.
        reg.addAlias(QStringLiteral("x:open"), QStringLiteral("corbomite-x:open"));
        QVERIFY(!reg.addAlias(QStringLiteral("x:open"),
                              QStringLiteral("corbomite-x:open")));
        QCOMPARE(reg.findCommand(QStringLiteral("x:open"))->aliasOf,
                 QStringLiteral("corbomite-x:open"));
        // Chained aliases are refused: one indirection is the whole contract.
        QVERIFY(!reg.addAlias(QStringLiteral("y:open"), QStringLiteral("x:open")));

        QCOMPARE(reg.listCommands().size(), 3);
    }

    void testHandBuiltAliasCycleDegradesToUnavailable()
    {
        // `aliasOf` is public, so a hand-built Command can name itself. It must
        // not spin.
        CommandRegistry reg;
        Command self;
        self.id = QStringLiteral("a");
        self.aliasOf = QStringLiteral("a");
        reg.addCommand(self);

        QVERIFY(!reg.isAvailable(QStringLiteral("a")));
        QVERIFY(!reg.executeById(QStringLiteral("a")));
        QCOMPARE(reg.listAvailable().size(), 0);
    }

    void testDanglingAliasIsUnavailable()
    {
        // A target removed after the alias was added leaves the alias inert,
        // not crashing and not dispatching.
        CommandRegistry reg;
        int runs = 0;
        Command c;
        c.id = QStringLiteral("corbomite-x:open");
        c.callback = [&runs] { ++runs; };
        reg.addCommand(c);
        reg.addAlias(QStringLiteral("x:open"), QStringLiteral("corbomite-x:open"));

        QVERIFY(reg.removeCommand(QStringLiteral("corbomite-x:open")));
        QVERIFY(!reg.executeById(QStringLiteral("x:open")));
        QCOMPARE(runs, 0);
    }
};

QTEST_MAIN(TestCommand)
#include "tst_command.moc"
