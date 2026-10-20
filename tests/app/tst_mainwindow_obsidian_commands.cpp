// SPDX-License-Identifier: GPL-3.0-or-later
//
// MainWindow's half of the command-id compatibility contract. The plugin half
// lives in tests/integration/tst_obsidian_command_ids.cpp.
//
// `.obsidian/hotkeys.json` is keyed by Obsidian command id, so an id Corbomite
// does not register can only ever be an inert override. MainWindow owns the
// commands that already exist behind a KAction with a different name — back,
// forward, save, split, the tab verbs — and registers each under its Obsidian
// id as well. The full id inventory is generated into
// docs/obsidian-command-ids.md by RE/tools/extract-command-ids.js.
//
// Each row is asserted three ways, so a mapping that silently stops working
// fails here rather than in a user's vault: the id resolves, its checkCallback
// gates on the KAction being enabled, and triggering it actually fires the
// KAction (spied, not assumed).
#include <QAction>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <KActionCollection>

#include <corbomite/core/Command.h>
#include <corbomite/core/HotkeyDispatcher.h>

#include "app/CorbomiteApp.h"
#include "app/MainWindow.h"
#include "corbomite/vault/Vault.h"

using Corbomite::CorbomiteApp;
using Corbomite::MainWindow;

namespace {

struct Bridge {
    const char *obsidianId;
    const char *actionId;
};

// Mirrors kObsidianIds in MainWindow.cpp's constructor. A row added there and
// not here fails the last test, which walks the live registry for ids matching
// the Obsidian namespaces this file owns.
const Bridge kBridges[] = {
    {"app:go-back",                "go_back"},
    {"app:go-forward",             "go_forward"},
    {"app:toggle-left-sidebar",    "view_toggle_left_sidebar"},
    {"command-palette:open",       "command_palette"},
    {"editor:save-file",           "file_save"},
    {"global-search:open",         "search_vault"},
    {"graph:open",                 "graph_view"},
    {"workspace:split-vertical",   "split_right"},
    {"workspace:split-horizontal", "split_down"},
    {"workspace:close",            "tab_close"},
    {"workspace:next-tab",         "tab_next"},
    {"workspace:previous-tab",     "tab_prev"},
    {"workspace:goto-last-tab",    "tab_jump_last"},
    {"workspace:undo-close-pane",  "tab_undo_close"},
    {"workspace:toggle-pin",       "tab_pin_toggle"},
    {"workspace:move-to-new-window", "tab_move_to_new_window"},
    {"workspace:toggle-stacked-tabs", "tab_toggle_stacked"},
};

// The Obsidian command namespaces MainWindow owns. Used to catch a bridge
// added to MainWindow.cpp without a matching row here.
const QStringList kOwnedPrefixes = {
    QStringLiteral("app:"), QStringLiteral("editor:"),
    QStringLiteral("workspace:"), QStringLiteral("global-search:"),
    QStringLiteral("graph:"), QStringLiteral("command-palette:"),
    QStringLiteral("markdown:"),
};

} // namespace

class TstMainWindowObsidianCommands : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void everyObsidianIdIsRegistered();
    void everyObsidianIdGatesOnItsKAction();
    void everyObsidianIdTriggersItsKAction();
    void bridgeIdsAreRegisteredButNotADuplicatePaletteRow();
    void labelsComeFromTheKAction();
    void noBridgeIsMissingFromThisTable();
    void splitIdsStillDispatchForViewHeaderMenus();
    void markdownAddMetadataPropertyStaysRegistered();
    void vaultHotkeysJsonRebindsTheKAction();
    void vaultHotkeysJsonUnbindClearsTheKAction();
    void closingTheVaultRestoresTheKAction();
    void editingHotkeysJsonReloadsIt();
};

void TstMainWindowObsidianCommands::everyObsidianIdIsRegistered()
{
    CorbomiteApp app;
    MainWindow w(&app);
    auto *registry = w.commandRegistry();
    QVERIFY(registry);

    for (const auto &b : kBridges) {
        const QString id = QString::fromLatin1(b.obsidianId);
        const auto *cmd = registry->findCommand(id);
        QVERIFY2(cmd, qPrintable(QStringLiteral("Obsidian id %1 not registered").arg(id)));
        // A real command, not an alias — MainWindow's are the primary name.
        QVERIFY2(!registry->isAlias(id),
                 qPrintable(QStringLiteral("%1 is an alias").arg(id)));
    }
}

void TstMainWindowObsidianCommands::everyObsidianIdGatesOnItsKAction()
{
    CorbomiteApp app;
    MainWindow w(&app);
    auto *registry = w.commandRegistry();
    auto *ac = w.actionCollection();

    for (const auto &b : kBridges) {
        const QString id = QString::fromLatin1(b.obsidianId);
        QAction *act = ac->action(QString::fromLatin1(b.actionId));
        QVERIFY2(act, qPrintable(QStringLiteral("no KAction %1").arg(b.actionId)));

        // No vault is open, so `updateVaultActions()` has disabled the
        // vault-scoped actions. A command whose checkCallback reported true
        // anyway would let a hotkey fire into a dead workspace.
        const bool expected = act->isEnabled();
        QCOMPARE(registry->isAvailable(id), expected);
    }
}

void TstMainWindowObsidianCommands::everyObsidianIdTriggersItsKAction()
{
    for (const auto &b : kBridges) {
        CorbomiteApp app;
        MainWindow w(&app);
        auto *registry = w.commandRegistry();
        QAction *act = w.actionCollection()->action(QString::fromLatin1(b.actionId));
        QVERIFY(act);
        act->setEnabled(true);

        QSignalSpy spy(act, &QAction::triggered);
        QVERIFY2(registry->executeById(QString::fromLatin1(b.obsidianId)),
                 qPrintable(QStringLiteral("executeById(%1) refused — is its KAction "
                                           "disabled?")
                                .arg(QString::fromLatin1(b.obsidianId))));
        QCOMPARE(spy.count(), 1);
    }
}

void TstMainWindowObsidianCommands::bridgeIdsAreRegisteredButNotADuplicatePaletteRow()
{
    CorbomiteApp app;
    MainWindow w(&app);
    auto *registry = w.commandRegistry();

    QStringList palette;
    for (const auto *cmd : registry->listPaletteCommands())
        palette.append(cmd->id);

    // A bridge has two jobs: make the Obsidian id addressable (hotkeys.json,
    // executeById, view hamburger menus) and NOT become a second row in the
    // palette. The verb is already there as its own KAction, and the palette
    // builds a group from the actions and another from the registry — so a
    // bridge that is not paletteHidden lists the same label twice
    // (`workspace:move-to-new-window` under both Tabs and Commands; found live
    // 2026-10-18). This case used to assert the opposite, which is how the
    // duplicate shipped.
    for (const auto &b : kBridges) {
        const QString id = QString::fromLatin1(b.obsidianId);
        QVERIFY2(registry->findCommand(id),
                 qPrintable(QStringLiteral("%1 is not registered at all").arg(id)));
        QVERIFY2(!palette.contains(id),
                 qPrintable(QStringLiteral(
                     "%1 would duplicate its own KAction in the palette").arg(id)));
    }
}

void TstMainWindowObsidianCommands::labelsComeFromTheKAction()
{
    CorbomiteApp app;
    MainWindow w(&app);
    auto *registry = w.commandRegistry();

    // If the label were a literal in the bridge table it could go stale against
    // setupActions(); pin that it is copied from the KAction instead.
    for (const auto &b : kBridges) {
        const QString id = QString::fromLatin1(b.obsidianId);
        QAction *act = w.actionCollection()->action(QString::fromLatin1(b.actionId));
        QVERIFY(act);
        QVERIFY(!act->text().isEmpty());
        QCOMPARE(registry->findCommand(id)->name, act->text());
    }
}

void TstMainWindowObsidianCommands::noBridgeIsMissingFromThisTable()
{
    CorbomiteApp app;
    MainWindow w(&app);
    auto *registry = w.commandRegistry();
    auto *ac = w.actionCollection();

    QStringList listed;
    for (const auto &b : kBridges) listed.append(QString::fromLatin1(b.obsidianId));

    for (const auto *cmd : registry->listCommands()) {
        if (registry->isAlias(cmd->id)) continue;
        if (cmd->id.startsWith(QLatin1String("split_"))) continue;
        if (cmd->id == QLatin1String("markdown:add-metadata-property")) continue;
        bool owned = false;
        for (const auto &p : kOwnedPrefixes)
            if (cmd->id.startsWith(p)) { owned = true; break; }
        if (!owned) continue;
        QVERIFY2(listed.contains(cmd->id),
                 qPrintable(QStringLiteral("%1 is registered under an Obsidian namespace "
                                           "but has no row in this test — add it to kBridges")
                                .arg(cmd->id)));
    }
    Q_UNUSED(ac);
}

void TstMainWindowObsidianCommands::splitIdsStillDispatchForViewHeaderMenus()
{
    // MarkdownFileView's view-header hamburger dispatches `split_right`/
    // `split_down` (Bug #1, commit 9fb2fe47). The Obsidian ids are additional
    // names for the same two commands, not replacements, so the Corbomite ids
    // must still resolve.
    CorbomiteApp app;
    MainWindow w(&app);
    auto *registry = w.commandRegistry();

    for (const char *id : {"split_right", "split_down"}) {
        const QString actionId = QString::fromLatin1(id);
        QVERIFY(registry->findCommand(actionId));
        QVERIFY(!registry->isAlias(actionId));

        QAction *act = w.actionCollection()->action(actionId);
        QVERIFY(act);
        act->setEnabled(true);
        QSignalSpy spy(act, &QAction::triggered);
        QVERIFY(registry->executeById(actionId));
        QCOMPARE(spy.count(), 1);
    }
}

void TstMainWindowObsidianCommands::markdownAddMetadataPropertyStaysRegistered()
{
    // The one command MainWindow registered under its Obsidian id before this
    // work (Cluster R Task 3.2). It must survive the reorganisation, and it is
    // the row in docs/obsidian-command-ids.corbomite.json that is not a bridge.
    CorbomiteApp app;
    MainWindow w(&app);
    auto *registry = w.commandRegistry();
    const auto *cmd = registry->findCommand(QStringLiteral("markdown:add-metadata-property"));
    QVERIFY(cmd);
    QVERIFY(!registry->isAlias(cmd->id));
    QVERIFY(cmd->callback);
}

// The other half of the contract: registering the ids is necessary but not
// sufficient. Without a dispatcher a vault's `hotkeys.json` is inert, so these
// three go through a real vault open with a real file in `.obsidian/`.

namespace {

/// A throwaway vault carrying `hotkeys.json`. QTemporaryDir, so nothing outside
/// the test's own directory is touched.
bool makeVaultWithHotkeys(QTemporaryDir &dir, const QByteArray &body)
{
    if (!dir.isValid()) return false;
    QDir vault(dir.path());
    if (!vault.mkpath(QStringLiteral(".obsidian"))) return false;
    QFile f(vault.filePath(QStringLiteral(".obsidian/hotkeys.json")));
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(body);
    f.close();
    QFile note(vault.filePath(QStringLiteral("note.md")));
    if (!note.open(QIODevice::WriteOnly)) return false;
    note.write("# Note\n");
    return true;
}

/// Write `body` over an existing vault's `.obsidian/hotkeys.json`.
bool rewriteHotkeys(const QString &vaultPath, const QByteArray &body)
{
    QFile f(QDir(vaultPath).filePath(QStringLiteral(".obsidian/hotkeys.json")));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    return f.write(body) == body.size();
}

} // namespace

void TstMainWindowObsidianCommands::vaultHotkeysJsonRebindsTheKAction()
{
    QTemporaryDir vaultDir;
    // `app:go-back` is bridged to the `go_back` KAction, whose stock chord is
    // Ctrl+Alt+Left (MainWindow.cpp setupActions).
    QVERIFY(makeVaultWithHotkeys(
        vaultDir,
        R"({"app:go-back":[{"modifiers":["Mod","Shift"],"key":"Left"}]})"));

    CorbomiteApp app;
    MainWindow w(&app);
    QVERIFY(w.hotkeyDispatcher());
    QVERIFY(w.hotkeyDispatcher()->unresolved().isEmpty());
    QAction *act = w.actionCollection()->action(QStringLiteral("go_back"));
    QVERIFY(act);
    QCOMPARE(act->shortcut(), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Left));

    QVERIFY(app.openVault(vaultDir.path()));
    QCOMPARE(act->shortcut(),
             QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Left));
    // And the vault's own table, not just the action:
    QCOMPARE(w.hotkeyDispatcher()->hotkeys().customHotkeys(
                 QStringLiteral("app:go-back")).value().first().key,
             QStringLiteral("Left"));
}

void TstMainWindowObsidianCommands::vaultHotkeysJsonUnbindClearsTheKAction()
{
    QTemporaryDir vaultDir;
    QVERIFY(makeVaultWithHotkeys(vaultDir, R"({"app:go-back":[]})"));

    CorbomiteApp app;
    MainWindow w(&app);
    QAction *act = w.actionCollection()->action(QStringLiteral("go_back"));
    QVERIFY(act);
    QVERIFY(app.openVault(vaultDir.path()));

    // `[]` is an unbind, not "no opinion": the action must end up with no
    // shortcut at all. Leaving Ctrl+Alt+Left in place would resurrect exactly
    // the default the vault removed.
    QVERIFY(act->shortcut().isEmpty());
    QVERIFY(!w.hotkeyDispatcher()->hotkeys().isBound(
        QStringLiteral("app:go-back")));
}

void TstMainWindowObsidianCommands::closingTheVaultRestoresTheKAction()
{
    QTemporaryDir vaultDir;
    QVERIFY(makeVaultWithHotkeys(
        vaultDir, R"({"app:go-back":[{"modifiers":["Mod"],"key":"g"}]})"));

    CorbomiteApp app;
    MainWindow w(&app);
    QAction *act = w.actionCollection()->action(QStringLiteral("go_back"));
    QVERIFY(act);

    QVERIFY(app.openVault(vaultDir.path()));
    QCOMPARE(act->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_G));

    app.closeVault();
    // Overrides are vault state; the next vault must not inherit them.
    QCOMPARE(act->shortcut(), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Left));
    QVERIFY(w.hotkeyDispatcher()->hotkeys().overriddenIds().isEmpty());
}

void TstMainWindowObsidianCommands::editingHotkeysJsonReloadsIt()
{
    QTemporaryDir vaultDir;
    QVERIFY(makeVaultWithHotkeys(vaultDir, R"({})"));

    CorbomiteApp app;
    MainWindow w(&app);
    QVERIFY(app.openVault(vaultDir.path()));
    QVERIFY(w.vaultObj());
    QAction *act = w.actionCollection()->action(QStringLiteral("go_back"));
    QVERIFY(act);
    QCOMPARE(act->shortcut(), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Left));

    // Obsidian watches its own config file (`vault.on("raw")` filtered to
    // `configDir + "/hotkeys.json"`, 0833-$w.js:15-18). Vault's own slots are
    // public so the watcher need not be timed out.
    QVERIFY(rewriteHotkeys(vaultDir.path(),
                           R"({"app:go-back":[{"modifiers":["Mod"],"key":"g"}]})"));
    w.vaultObj()->onExternalRaw(QStringLiteral(".obsidian/hotkeys.json"));
    QVERIFY2(QTest::qWaitFor(
                 [&] { return act->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_G); },
                 2000),
             "an external hotkeys.json edit did not re-arm the shortcut");

    QVERIFY(rewriteHotkeys(vaultDir.path(), R"({"app:go-back":[]})"));
    w.vaultObj()->onExternalRaw(QStringLiteral(".obsidian/hotkeys.json"));
    QVERIFY2(QTest::qWaitFor([&] { return act->shortcut().isEmpty(); }, 2000),
             "an external unbind did not clear the shortcut");
}

QTEST_MAIN(TstMainWindowObsidianCommands)
#include "tst_mainwindow_obsidian_commands.moc"
