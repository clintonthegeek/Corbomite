// SPDX-License-Identifier: GPL-3.0-or-later
//
// Corbomite::HotkeyDispatcher — the delivery half of the `hotkeys.json`
// contract. `tst_hotkey` pins the merge (presence-based, `[]` unbinds);
// this pins that the merge reaches a real shortcut.
//
// Spec: docs/obsidian-audit/addenda/2026-10-03-core-command-and-hotkey-contracts.md
// §D (override-only bake, the `code` field) and §E (modals cut the command
// table). Every assertion below is falsifiable against a dispatcher that is
// wired up but wrong:
//
//   - an override has to *replace* the action's chord, not add a second one;
//   - `[]` has to leave the action with no shortcut at all (a reader that
//     treats "[]" as "no opinion" resurrects Ctrl+B behind the user's back);
//   - a `code` binding has to round-trip and fire, or be reported — never
//     coerced onto some other key;
//   - an unspellable binding has to be reported, not dropped in silence.

#include <QAction>
#include <QDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLineEdit>
#include <QSignalSpy>
#include <QTest>

#include <corbomite/core/Command.h>
#include <corbomite/core/Hotkey.h>
#include <corbomite/core/HotkeyDispatcher.h>
#include <corbomite/core/ScopeManager.h>

using Corbomite::Command;
using Corbomite::CommandRegistry;
using Corbomite::Hotkey;
using Corbomite::HotkeyDispatcher;
using Corbomite::HotkeyModifier;
using Corbomite::hotkeyMatches;
using Corbomite::hotkeyToKeySequence;
using Corbomite::ScopeManager;

namespace {

QKeyEvent keyPress(int key, Qt::KeyboardModifiers mods = Qt::NoModifier,
                   const QString &text = {})
{
    return QKeyEvent(QEvent::KeyPress, key, mods, text);
}

} // namespace

class TestHotkeyDispatcher : public QObject
{
    Q_OBJECT

private:
    CommandRegistry m_registry;
    HotkeyDispatcher *m_dispatcher = nullptr;
    QAction *m_bold = nullptr;
    QAction *m_boldNative = nullptr;
    QAction *m_italics = nullptr;
    int m_fired = 0;

    /// A command with the shape MainWindow's Obsidian-id bridges have: an
    /// availability check that gates on a QAction, exactly like
    /// `bindToAction` in MainWindow's constructor.
    void registerBridgedCommand(const QString &id, QAction *action)
    {
        Command c;
        c.id = id;
        c.name = action->text();
        c.checkCallback = [action](bool checking) {
            if (!action->isEnabled()) return false;
            if (!checking) action->trigger();
            return true;
        };
        m_registry.addCommand(c);
        m_dispatcher->bindAction(id, action);
    }

    /// A command with no QAction behind it — the shape a plugin command has.
    /// Nothing can carry a shortcut text for it, so every binding of an
    /// overridden id goes to the event-level table.
    void registerBareCommand(const QString &id)
    {
        Command c;
        c.id = id;
        c.name = id;
        c.callback = [&] { ++m_fired; };
        m_registry.addCommand(c);
    }

    bool dispatch(int key, Qt::KeyboardModifiers mods = Qt::NoModifier,
                  const QString &text = {}, QWidget *focusOverride = nullptr)
    {
        auto e = keyPress(key, mods, text);
        return ScopeManager::instance()->dispatchKey(&e, focusOverride);
    }

private Q_SLOTS:
    void init()
    {
        m_fired = 0;
        m_bold = new QAction(QStringLiteral("Bold"), nullptr);
        m_bold->setShortcut(QKeySequence(Qt::ControlModifier | Qt::Key_B));
        m_boldNative = new QAction(QStringLiteral("Split Down"), nullptr);
        m_boldNative->setShortcut(QKeySequence(Qt::ControlModifier | Qt::ShiftModifier
                                               | Qt::Key_Down));
m_italics = new QAction(QStringLiteral("Italic"), nullptr);
        m_italics->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));

        m_dispatcher = new HotkeyDispatcher(&m_registry, this);

        registerBridgedCommand(QStringLiteral("editor:toggle-bold"), m_bold);
        registerBridgedCommand(QStringLiteral("workspace:split-horizontal"),
                               m_boldNative);
        registerBridgedCommand(QStringLiteral("editor:toggle-italics"), m_italics);

        m_dispatcher->install();
        QVERIFY(ScopeManager::instance()->rootScope());

    }

    void cleanup()
    {
        // A failing assertion returns from a test body mid-way, so the stack may
        // still hold scopes the test pushed. Drop them before the locals die.
        while (ScopeManager::instance()->stackDepth() > 0)
            ScopeManager::instance()->popScope();
        m_dispatcher->clearActions();
        delete m_dispatcher;
        m_dispatcher = nullptr;
        ScopeManager::instance()->setRootScope(nullptr);
        delete m_bold;
        delete m_boldNative;
        delete m_italics;
        m_bold = m_boldNative = m_italics = nullptr;
    }

    // --- the override actually replaces the action's chord ------------------

    void testOverrideMovesTheActionShortcut()
    {
        QSignalSpy spy(m_bold, &QAction::triggered);
        m_bold->setEnabled(true);
        QVERIFY(m_dispatcher->load(
            R"({"editor:toggle-bold":[{"modifiers":["Mod"],"key":"j"}]})"));

        QCOMPARE(m_bold->shortcut(), QKeySequence(Qt::ControlModifier | Qt::Key_J));
        QCOMPARE(spy.count(), 0);

        // The old chord is gone everywhere, not merely shadowed by the new one:
        // nothing in the chain claims Ctrl+B any more.
        QVERIFY(!dispatch(Qt::Key_B, Qt::ControlModifier, QStringLiteral("b")));
        QCOMPARE(spy.count(), 0);
    }

    void testAnAbsentIdKeepsItsOwnShortcut()
    {
        m_bold->setEnabled(true);
        QVERIFY(m_dispatcher->load(
            R"({"editor:toggle-bold":[{"modifiers":["Mod"],"key":"j"}]})"));
        // workspace:split-horizontal is untouched, so its own chord stands.
        QCOMPARE(m_boldNative->shortcut(),
                 QKeySequence(Qt::ControlModifier | Qt::ShiftModifier | Qt::Key_Down));
        QVERIFY(!m_dispatcher->hotkeys().customHotkeys(
                     QStringLiteral("workspace:split-horizontal")).has_value());
    }

    void testDismissingTheOverrideRestoresTheActionShortcut()
    {
        QVERIFY(m_dispatcher->load(
            R"({"editor:toggle-bold":[{"modifiers":["Mod"],"key":"j"}]})"));
        QCOMPARE(m_bold->shortcut(), QKeySequence(Qt::ControlModifier | Qt::Key_J));
        QVERIFY(m_dispatcher->load(R"({})"));
        QCOMPARE(m_bold->shortcut(), QKeySequence(Qt::ControlModifier | Qt::Key_B));
    }

    // --- unbind: `[]` is not "no opinion" ----------------------------------

    void testEmptyArrayClearsTheShortcutAndDoesNotFallBack()
    {
        m_bold->setEnabled(true);
        QSignalSpy spy(m_bold, &QAction::triggered);
        QVERIFY(m_dispatcher->load(R"({"editor:toggle-bold":[]})"));

        // The action must lose its chord, not merely stop matching in the
        // dispatcher: a QAction shortcut left in place fires independently of
        // everything above.
        QVERIFY(m_bold->shortcut().isEmpty());
        QVERIFY(!m_dispatcher->hotkeys().isBound(
            QStringLiteral("editor:toggle-bold")));
        QVERIFY(!dispatch(Qt::Key_B, Qt::ControlModifier, QStringLiteral("b")));
        QCOMPARE(spy.count(), 0);
    }

    void testUnbindBeatsAnObsidianDefaultOfTheSameId()
    {
        // The 41 built-in defaults are exactly the thing an unbind has to
        // suppress: `editor:toggle-bold` carries Mod+B in Obsidian
        // (app/own/2075-Vie.js:1171). Registering it makes "fall back to the
        // default" a live failure mode rather than a hypothetical one.
        Command c;
        c.id = QStringLiteral("editor:toggle-bold");
        c.name = QStringLiteral("Bold");
        c.callback = [&] { ++m_fired; };
        m_registry.addCommand(c);
        m_dispatcher->hotkeys().addDefaultHotkeys(
            QStringLiteral("editor:toggle-bold"),
            {Hotkey{{HotkeyModifier::Mod}, QStringLiteral("B")}});

        QVERIFY(m_dispatcher->load(R"({"editor:toggle-bold":[]})"));
        QCOMPARE(m_dispatcher->hotkeys().defaultHotkeys(
                     QStringLiteral("editor:toggle-bold")).size(), 1);
        QVERIFY(m_dispatcher->hotkeys().effectiveHotkeys(
                    QStringLiteral("editor:toggle-bold")).isEmpty());
        QVERIFY(!dispatch(Qt::Key_B, Qt::ControlModifier, QStringLiteral("b")));
        QCOMPARE(m_fired, 0);
    }

    // --- `code`: round-trip or refuse, never substitute ---------------------

    void testCodeBindingRoundTripsThroughTheFileAndFires()
    {
        // The macOS Ctrl+Option+I binding from the addendum §D: `key` holds the
        // caret the platform reports, `code` the physical key. QKeySequence has
        // no syntax for a physical key, so this must go through the event-level
        // matcher — and it must survive save/load unchanged.
        //
        // The addendum writes the binding as `{"modifiers":["Mod"], …}`, but its
        // own `isMatch` compares modifier *sets*: Ctrl+Option also holds Alt, so
        // "Meta" never equals "Alt,Meta" and that spelling fires in Obsidian
        // either. Option is named explicitly here for that reason.
        const QByteArray json =
            R"({"editor:toggle-italics":[{"modifiers":["Mod","Alt"],"key":"^","code":"KeyI"}]})";
        m_italics->setEnabled(true);
        QVERIFY(m_dispatcher->load(json));

        const auto bindings = m_dispatcher->hotkeys().customHotkeys(
            QStringLiteral("editor:toggle-italics"));
        QVERIFY(bindings.has_value());
        QCOMPARE(bindings->size(), 1);
        QCOMPARE(bindings->first().code, QStringLiteral("KeyI"));
        QCOMPARE(bindings->first().key, QStringLiteral("^"));
        QCOMPARE(QJsonDocument::fromJson(m_dispatcher->hotkeys().save())
                     .object().value(QStringLiteral("editor:toggle-italics"))
                     .toArray().first().toObject().value(QStringLiteral("code"))
                     .toString(),
                 QStringLiteral("KeyI"));

        QCOMPARE(m_dispatcher->dispatchEntryCount(), 1);
        QCOMPARE(m_dispatcher->unresolved(), QStringList{});
        // Ctrl+Option+I: the character is the caret, the physical key is KeyI.
        QVERIFY(dispatch(Qt::Key_I, Qt::ControlModifier | Qt::AltModifier, QStringLiteral("^")));
        QVERIFY(!dispatch(Qt::Key_I, Qt::ControlModifier, QStringLiteral("i")));
    }

    void testCodeBindingClearsTheActionShortcutInsteadOfAddingToIt()
    {
        QVERIFY(m_dispatcher->load(
            R"({"editor:toggle-italics":[{"modifiers":["Mod","Alt"],"key":"^","code":"KeyI"}]})"));
        // Ctrl+I is the action's stock chord. Leaving it would make the vault's
        // binding additive: both Ctrl+I and Ctrl+Option+I would fire italics.
        QVERIFY(m_italics->shortcut().isEmpty());
    }

    void testCodeOnlyBindingLeavesTheOtherBindingsOnTheAction()
    {
        QVERIFY(m_dispatcher->load(
            R"({"editor:toggle-italics":[{"modifiers":["Mod","Alt"],"key":"^","code":"KeyI"},)"
            R"({"modifiers":["Mod"],"key":"u"}]})"));
        // One command, one live path per binding: the code binding goes to the
        // matcher, the plain one to the action.
        QCOMPARE(m_italics->shortcut(), QKeySequence(Qt::ControlModifier | Qt::Key_U));
        QCOMPARE(m_dispatcher->dispatchEntryCount(), 1);
    }

    void testUnmappableCodeIsReportedRatherThanGuessed()
    {
        m_italics->setEnabled(true);
        QVERIFY(m_dispatcher->load(
            R"({"editor:toggle-italics":[{"modifiers":["Mod"],"key":"+","code":"NumpadNonsense"}]})"));
        QCOMPARE(m_dispatcher->dispatchEntryCount(), 0);
        QCOMPARE(m_dispatcher->unresolved().size(), 1);
        QVERIFY2(m_dispatcher->unresolved().first().startsWith(
                     QStringLiteral("editor:toggle-italics:")),
                 qPrintable(m_dispatcher->unresolved().first()));
        QVERIFY2(m_dispatcher->unresolved().first().contains(
                     QStringLiteral("NumpadNonsense")),
                 qPrintable(m_dispatcher->unresolved().first()));
        // The character it was paired with must not stand in for the code.
        QVERIFY(!dispatch(Qt::Key_Plus, Qt::ControlModifier, QStringLiteral("+")));
    }

    void testUnknownKeyNameIsReported()
    {
        QVERIFY(m_dispatcher->load(
            R"({"editor:toggle-bold":[{"modifiers":["Mod"],"key":"NotAKey"}]})"));
        QCOMPARE(m_dispatcher->unresolved().size(), 1);
        QVERIFY2(m_dispatcher->unresolved().first().contains(
                     QStringLiteral("NotAKey")),
                 qPrintable(m_dispatcher->unresolved().first()));
        // An id present in the file has spoken, so the stock chord goes even
        // though the replacement cannot be spelled as one.
        QVERIFY(m_bold->shortcut().isEmpty());
    }

    // --- matching semantics -------------------------------------------------

    void testAnExtraHeldModifierBreaksTheMatch()
    {
        m_italics->setEnabled(true);
        QVERIFY(m_dispatcher->load(
            R"({"editor:toggle-italics":[{"modifiers":["Mod"],"key":"u"}]})"));
        // isMatch compares modifier *sets* exactly (0747-ly.js:130-133).
        QVERIFY(!dispatch(Qt::Key_U, Qt::ControlModifier | Qt::ShiftModifier, QStringLiteral("U")));
    }

    void testModResolvesToCtrlOffMacAndMetaOnIt()
    {
        Hotkey h{{HotkeyModifier::Mod}, QStringLiteral("u")};
        QCOMPARE(hotkeyToKeySequence(h, false), QKeySequence(Qt::ControlModifier | Qt::Key_U));
        QCOMPARE(hotkeyToKeySequence(h, true),
                 QKeySequence(Qt::MetaModifier | Qt::Key_U));

        auto ctrlU = keyPress(Qt::Key_U, Qt::ControlModifier, QStringLiteral("u"));
        auto metaU = keyPress(Qt::Key_U, Qt::MetaModifier, QStringLiteral("u"));
        QVERIFY(hotkeyMatches(h, ctrlU, false));
        QVERIFY(!hotkeyMatches(h, ctrlU, true));
        QVERIFY(hotkeyMatches(h, metaU, true));
    }

    void testDuplicateModifiersAreRefused()
    {
        // compileModifiers sorts but never de-duplicates (0747-ly.js:112-115),
        // so ["Mod","Ctrl"] is "Ctrl,Ctrl" in Obsidian and matches nothing.
        Hotkey h{{HotkeyModifier::Mod, HotkeyModifier::Ctrl}, QStringLiteral("b")};
        QVERIFY(hotkeyToKeySequence(h, false).isEmpty());
        auto e = keyPress(Qt::Key_B, Qt::ControlModifier, QStringLiteral("b"));
        QVERIFY(!hotkeyMatches(h, e, false));
    }

    void testModlessBindingMatchesNothingHeld()
    {
        Hotkey h{{}, QStringLiteral("F1")};
        auto f1 = keyPress(Qt::Key_F1);
        QVERIFY(hotkeyMatches(h, f1));
        QVERIFY(!hotkeyMatches(h, keyPress(Qt::Key_F1, Qt::ControlModifier)));
    }

    void testDomKeySpellingsMapOntoQtOnes()
    {
        // The four DOM names Qt's tables spell differently, all of which are in
        // Obsidian's own default table (0833-$w.js + 2075-Vie.js).
        QCOMPARE(hotkeyToKeySequence(Hotkey{{HotkeyModifier::Mod},
                                            QStringLiteral("ArrowLeft")}),
                 QKeySequence(Qt::ControlModifier | Qt::Key_Left));
        QCOMPARE(hotkeyToKeySequence(Hotkey{{HotkeyModifier::Mod},
                                            QStringLiteral("PageDown")}),
                 QKeySequence(Qt::ControlModifier | Qt::Key_PageDown));
        QCOMPARE(hotkeyToKeySequence(Hotkey{{HotkeyModifier::Mod},
                                            QStringLiteral("Enter")}),
                 QKeySequence(Qt::ControlModifier | Qt::Key_Return));
        QCOMPARE(hotkeyToKeySequence(Hotkey{{HotkeyModifier::Mod},
                                            QStringLiteral(";")}),
                 QKeySequence(Qt::ControlModifier | Qt::Key_Semicolon));
    }

    // --- the scope-stack shape ---------------------------------------------

    void testAPushedScopeMasksTheTableAndDecliningFallsThrough()
    {
        m_italics->setEnabled(true);
        // First binding → the action, second → the event-level table, so the
        // masking below is observable in the table specifically.
        QVERIFY(m_dispatcher->load(
            R"({"editor:toggle-italics":[{"modifiers":["Mod"],"key":"u"},)"
            R"({"modifiers":["Mod","Alt"],"key":"u"}]})"));
        QCOMPARE(m_dispatcher->dispatchEntryCount(), 1);

        Corbomite::Scope modal;
        bool claimEscape = false;
        modal.registerBinding(Qt::NoModifier, Qt::Key_Escape,
                              [&](QKeyEvent *) { return claimEscape; });
        ScopeManager::instance()->pushScope(&modal);

        // Declines → the table still sees it. This is the deliberate divergence
        // from Obsidian, where a parentless modal scope ends the walk
        // (addendum §E): a modal that ignores a key must not silence the app.
        QVERIFY(dispatch(Qt::Key_U, Qt::ControlModifier | Qt::AltModifier,
                         QStringLiteral("u")));
        QVERIFY(!dispatch(Qt::Key_Escape));

        // Claims → the table is masked for that key.
        claimEscape = true;
        QVERIFY(dispatch(Qt::Key_Escape));
        ScopeManager::instance()->popScope();
    }

    void testTheTableIsSkippedWhileAModalOwnsInput()
    {
        registerBareCommand(QStringLiteral("plugin:demo"));
        m_dispatcher->load(
            R"({"plugin:demo":[{"modifiers":["Mod"],"key":"u"}]})");
        QCOMPARE(m_dispatcher->dispatchEntryCount(), 1);
        QVERIFY(dispatch(Qt::Key_U, Qt::ControlModifier, QStringLiteral("u")));

        // Addendum §E: a QDialog runs in its own shortcut context and must not
        // inherit the main window's commands. The gate is
        // `QApplication::activeModalWidget`, which a shown application-modal
        // dialog sets.
        auto *dialog = new QDialog;
        dialog->setWindowModality(Qt::ApplicationModal);
        dialog->show();
        QVERIFY(qApp->activeModalWidget());
        QVERIFY(!dispatch(Qt::Key_U, Qt::ControlModifier, QStringLiteral("u")));
        dialog->close();
        delete dialog;
        QVERIFY(!qApp->activeModalWidget());
        QVERIFY(dispatch(Qt::Key_U, Qt::ControlModifier, QStringLiteral("u")));
        QCOMPARE(m_fired, 2);
    }

    void testAFocusGatedTextInputDoesNotSilenceTheTable()
    {
        registerBareCommand(QStringLiteral("plugin:demo"));
        m_dispatcher->load(
            R"({"plugin:demo":[{"modifiers":["Mod"],"key":"u"}]})");
        QLineEdit input;
        // The focus gate is for pushed scopes (so a modal can still get typed
        // input); gating the command table on it would make every global
        // shortcut dead inside the search box.
        QVERIFY(dispatch(Qt::Key_U, Qt::ControlModifier, QStringLiteral("u"),
                        &input));
        QCOMPARE(m_fired, 1);
    }

    void testUnavailableCommandDoesNotConsumeTheKey()
    {
        m_bold->setEnabled(false);
        // Two bindings: the first goes to the action, the second to the table.
        QVERIFY(m_dispatcher->load(
            R"({"editor:toggle-bold":[{"modifiers":["Mod"],"key":"j"},)"
            R"({"modifiers":["Mod"],"key":"k"}]})"));
        QCOMPARE(m_dispatcher->dispatchEntryCount(), 1);
        // onTrigger consumes only on a successful dispatch (0833-$w.js:84-85),
        // so a disabled command leaves the key to the rest of the chain.
        QVERIFY(!dispatch(Qt::Key_K, Qt::ControlModifier, QStringLiteral("k")));
    }

    void testNonObjectBodyKeepsThePreviousTable()
    {
        QVERIFY(m_dispatcher->load(
            R"({"editor:toggle-bold":[{"modifiers":["Mod"],"key":"j"}]})"));
        QCOMPARE(m_bold->shortcut(), QKeySequence(Qt::ControlModifier | Qt::Key_J));
        // Obsidian's load() gates on typeof === "object" and keeps the previous
        // map otherwise (0833-$w.js:38).
        QVERIFY(!m_dispatcher->load(QByteArray("[1, 2, 3]")));
        QCOMPARE(m_bold->shortcut(), QKeySequence(Qt::ControlModifier | Qt::Key_J));
        QVERIFY(!m_dispatcher->load(QByteArray("{ not json")));
        QCOMPARE(m_bold->shortcut(), QKeySequence(Qt::ControlModifier | Qt::Key_J));
    }

    void testMultipleBindingsSplitBetweenActionAndTable()
    {
        m_italics->setEnabled(true);
        QVERIFY(m_dispatcher->load(
            R"({"editor:toggle-italics":[{"modifiers":["Mod"],"key":"u"},)"
            R"({"modifiers":["Mod","Shift"],"key":"u"}]})"));
        QCOMPARE(m_italics->shortcut(), QKeySequence(Qt::ControlModifier | Qt::Key_U));
        QCOMPARE(m_dispatcher->dispatchEntryCount(), 1);
        QVERIFY(dispatch(Qt::Key_U, Qt::ControlModifier | Qt::ShiftModifier, QStringLiteral("U")));
    }

    void testUnknownCommandIdsStillDispatch()
    {
        // A community plugin's id, or one Corbomite has not implemented: the
        // override is inert until something registers it, and registering it
        // later must need no reload of the file. It also has no QAction behind
        // it, so the binding cannot ride a shortcut text and has to be matched.
        QVERIFY(m_dispatcher->load(
            R"({"bookmarks:bookmark-current-file":[{"modifiers":["Mod","Alt"],"key":"d"}]})"));
        QVERIFY(!dispatch(Qt::Key_D, Qt::ControlModifier | Qt::AltModifier,
                         QStringLiteral("d")));

        Command c;
        c.id = QStringLiteral("bookmarks:bookmark-current-file");
        c.name = QStringLiteral("Bookmark");
        c.callback = [&] { ++m_fired; };
        m_registry.addCommand(c);
        QVERIFY(dispatch(Qt::Key_D, Qt::ControlModifier | Qt::AltModifier,
                         QStringLiteral("d")));
        QCOMPARE(m_fired, 1);
    }
};

QTEST_MAIN(TestHotkeyDispatcher)
#include "tst_hotkeydispatcher.moc"
