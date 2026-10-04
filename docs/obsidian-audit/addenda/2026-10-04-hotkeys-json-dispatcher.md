# The `hotkeys.json` dispatcher exists now — and the `code` field it has to carry is a *name*, not a position

**Date:** 2026-10-04
**Discovered during:** implementation of the `hotkeys.json` → shortcut dispatch path (the gap the
`2026-10-03` addendum's §D "Action taken" item 1 predicted: registering the command ids is
necessary but not sufficient)
**Companion to:** [`2026-10-03-core-command-and-hotkey-contracts.md`](2026-10-03-core-command-and-hotkey-contracts.md) §D, §E, §M.17–M.20
**Code:** `libs/core/include/corbomite/core/HotkeyDispatcher.h`, `libs/core/src/HotkeyDispatcher.cpp`,
`libs/core/src/Hotkey.cpp` (`hotkeyCodeToQtKey` / `hotkeyToKeySequence` / `hotkeyMatches`),
`libs/core/src/ScopeManager.cpp` (root slot), `libs/core/src/Scope.cpp` (`registerCatchAll`),
`src/app/MainWindow.cpp` (`reloadVaultHotkeys`, the bridge loop's `bindAction`)

> **Doctrine reminder.** Corbomite does not mirror the JS API. What follows is the
> compat contract it now implements and the two places where Obsidian's own behaviour is
> *not* what the addendum describes.

---

## Finding

### §A. CORRECTION to the 2026-10-03 addendum §D: the `["Mod"]` + `code:"KeyI"` example does not match, in Obsidian either

The addendum states that a mac-authored override

```json
{"modifiers":["Mod"],"key":"^","code":"KeyI"}
```

"can … still match Ctrl+Option+I". Read against the source, it cannot, and the blocker is not
the `code` field but the modifier list. `isMatch` (`app/own/0747-ly.js:130-133`) is

```js
(null === n || n === t.modifiers) && (!i || i === t.vkey || !(!t.key || i.toLowerCase() !== t.key.toLowerCase()))
```

and `bake()` compares `n === t.modifiers` — **exact string equality of sorted, comma-joined
modifier sets** (`compileModifiers`, `:112-115`). On macOS Ctrl+Option+I reports
`ctrlKey && altKey`, so `getModifiers` yields `["Ctrl","Meta","Alt"]` → `"Alt,Ctrl,Meta"`, while
the binding compiles `["Mod"]` → `"Meta"`. `"Meta" !== "Alt,Ctrl,Meta"`, the conjunction is
false, and `onTrigger` (`app/own/0833-$w.js:78-86`) never reaches `executeCommand`. The `code`
field is real and `Xw` really does fold it into the `key` slot — the addendum's *mechanism* is
right — but the worked example needs `["Mod","Alt"]` to fire. Corbomite's tests use the
corrected spelling (`tst_hotkeydispatcher.cpp::testCodeBindingRoundTripsThroughTheFileAndFires`).

The same rewrite the addendum describes, for completeness:

```js
54 === t.which && "^" == t.key && "KeyI" === t.code && (o = "KeyI")
```

(`:83`) rewrites `vkey` to `"KeyI"`, while `bake` stored `Xw("KeyI") === "I"`. So even with the
modifiers right, the first arm of `isMatch`'s key test (`i === ctx.vkey` → `"I" === "KeyI"`)
fails, and the match has to come from the **third arm** — `ctx.key` (`"^"`) compared
case-insensitively against the binding key (`"I"`) — which fails too. In other words the `code`
field buys the mac-authored binding nothing on 1.13.7 as shipped; what actually makes
`["Mod","Alt"] + "KeyI"` work is the modifier set plus the fact that `vkey` was rewritten, and
the *binding* is matched by name, not by position.

### §B. CONSEQUENCE — a `code` binding is matched as a name, so Corbomite matches it the same way

`bake()` stores one string per binding: `key: a.code ? Xw(a.code) : a.key`. There is no separate
physical-key comparison anywhere in `onTrigger`; the `code` only chooses *which string* lands in
that slot. So a faithful matcher does **not** need a physical-position lookup — it needs the
`Xw` collapse plus a name comparison, which is what
`Corbomite::hotkeyMatches` (`libs/core/src/Hotkey.cpp`) implements:

- modifiers: exact set equality, `Mod` → `Meta` on macOS else `Ctrl`, `Qt::KeypadModifier`
  stripped from the event side (Obsidian's Keymap has no keypad state to compare against);
- key: `hotkey.code.isEmpty() ? hotkey.key : Xw(hotkey.code)`, compared against
  `QKeyEvent::key()`, and failing that against `QKeyEvent::text()` case-insensitively
  (`isMatch`'s third arm).

The corollary is worth stating because it looks like a bug otherwise: on a non-QWERTY layout a
`code` binding does **not** track the physical key, in Corbomite or in Obsidian.

What a `QKeySequence` genuinely cannot express is the *spelling*, not the position: `NumpadAdd`,
`IntlBackslash`, `MediaPlayPause` have no Qt key name. Those are refused and reported
(`HotkeyDispatcher::unresolved()` + a `qWarning`), never coerced onto a different key.

### §C. Qt specifics a Qt-based reader has to know (all measured, Qt 6.11)

| Input | `QKeySequence::fromString` |
|---|---|
| `"ArrowLeft"`, `"ArrowRight"`, `"ArrowUp"`, `"ArrowDown"` | `Qt::Key_unknown` — Qt spells these `Left`/`Right`/`Up`/`Down` |
| `"PageUp"` / `"PageDown"` | `Qt::Key_unknown` — Qt spells these `PgUp`/`PgDown` |
| `"Enter"` | `Qt::Key_Enter` (numpad), **not** `Qt::Key_Return`; Obsidian's `Enter` means Return |
| `"Minus"`, `"Equal"`, `"BracketLeft"`, … `"Backquote"` | `Qt::Key_unknown` — Qt names these `-`, `=`, `[`, … `` ` `` |
| unknown name generally | **a one-step sequence holding `Qt::Key_unknown`**, not an empty sequence — so `isEmpty()` is the wrong validity test |
| `"Ctrl+Return+Shift"` | `Qt::Key_unknown` — modifiers must precede the key |
| `"Ctrl+Ctrl+J"` | collapses to `Ctrl+J`, where Obsidian's `compileModifiers` would produce the unmatchable `"Ctrl,Ctrl"` |

`Corbomite::hotkeyToKeySequence` therefore renames those eighteen DOM spellings and then defers
to Qt's own parser — the same table `KKeySequenceWidget` writes — so a user-edited shortcut and
a vault-authored one speak the same language. It refuses a duplicated modifier set rather than
letting Qt collapse it, because Obsidian never de-duplicates and the binding is dead there.

### §D. What the dispatcher does, and the scope shape it needed

`HotkeyManager` merged the file over the defaults (`HotkeyManager.cpp`, already landed) and
nothing consumed the result: a vault's override was inert. `HotkeyDispatcher` is the delivery
half, with one binding travelling exactly one path:

| Binding | Path | Why |
|---|---|---|
| first binding an id can express as a `QKeySequence`, for an id with a bound `QAction` | `QAction::setShortcut` | the shortcut stays visible in the menu, stays editable in `Settings` → Shortcuts, and Qt's own matcher runs it |
| any `code` binding, and every binding past the first | the event-level table, via `ScopeManager`'s new root slot | a `code` has no `QKeySequence` spelling |
| an id whose list is `[]` | nowhere — the action's shortcut is **cleared** | `[]` is an unbind; leaving the action's chord in place would resurrect the exact default the vault removed, and a `QAction` shortcut fires independently of everything above |

**The shape decision.** `ScopeManager`'s stack was flat, and `ScopeManager.cpp` left the choice
open: either a parentless pushed scope cuts the scopes below it (Obsidian-faithful, and an Obsidian
bug per §E of the 2026-10-03 addendum) or it does not. Corbomite keeps the flat walk and adds an
explicit **root slot** that is always consulted last, gated on `QApplication::activeModalWidget`
being null. The reasoning: the flat walk reproduces the *ordering* Obsidian gets from its
per-window stacks plus `Scope.parent` (modal first, command table last) without reproducing the
*bug* that kills every `hotkeys.json` binding while any modal is up; and the cut that genuinely
matters — a `QDialog` must not fire the main window's commands — is expressed where it actually
lives (Qt modality) rather than as a side effect of `Scope.parent` being null. Two consequences
are pinned by tests: a pushed scope that declines a key lets the table see it (Obsidian would
stop the walk), and the focus gate that protects typed input covers the pushed scopes only
(Obsidian's catch-all has no focus condition at all, and gating it would make every global
shortcut dead inside the search box).

`Scope::registerCatchAll` is Obsidian's `register(null, null, …)` (`0747-ly.js:6-13`), with one
divergence: a catch-all is a fallback layer, so it cannot preempt a concrete binding in the same
scope regardless of registration order. Obsidian keeps both in one array matched in registration
order, which would let the command table shadow a modal's Esc.

### §E. Gaps that remain, by design

1. **Only registered ids dispatch.** 26 of the 232 Obsidian ids exist in Corbomite
   (`docs/obsidian-command-ids.md`), 17 of them bridged to a `QAction`. `editor:toggle-bold` in a
   vault is still inert — not because of the dispatcher, but because no command is registered
   under that id. The table holds the binding; `executeById` has nothing to run.
2. **`repeatable` is unimplemented.** `onTrigger` fires a command once per physical press unless
   it carries `repeatable: true` (`0833-$w.js:84`). `Corbomite::Command` has no such field, so a
   held key re-fires. That needs a field on `Command`, not dispatcher work.
3. **`Qw`, the display formatter, is unused.** It renders only the first binding, in Obsidian's
   own modifier order and title-cased key names (`0832-Qw.js:2-13`). Corbomite shows whatever
   the `QAction` holds, so a code-only or second binding has no user-visible form yet; nothing
   renders a `Hotkey` list.
4. **The root gate is Qt modality, not window identity.** Obsidian gives the vault-chooser and
   help windows their own root `Keymap` (addendum §A). Here a *non-modal* auxiliary window — a
   popped-out leaf, say — still reaches the command table, which is arguably right for a
   workspace window and wrong for a chooser dialog.
5. **No write path.** `save()` exists on `HotkeyManager` and is not wired to anything: Corbomite
   never rewrites `hotkeys.json`, and `saveShortcuts()` is called nowhere in the tree, so the
   effective shortcut cannot leak into the user's global `KConfig` shortcuts.
6. **Live reload is coalesced, not leading-edge.** Obsidian's `Ml(this.load, 50)` is a
   leading-edge throttle, which can drop an edit arriving inside the window; Corbomite restarts a
   50 ms timer instead, which converges on the last edit.

## Why noticed now

The 2026-10-03 addendum flagged the dispatcher as the next implementation surface and warned that
"A Corbomite `QKeySequence`-based reader **cannot** express this; it needs the raw binding stored
and matched". That warning is right about the storage and about `QKeySequence`, and slightly off
about the *reason*: the binding cannot be expressed because Qt lacks a name for some of the DOM
codes, not because the field carries position information Obsidian's matcher ever used. §A's
correction is what turned "store the raw binding and match it" from a guess into a specification.

## Action taken

Code, not documentation: `HotkeyDispatcher` (new), `Scope::registerCatchAll`, `ScopeManager`'s root
slot + root gate + focus-gate scoping, `MainWindow::reloadVaultHotkeys` wired to `Vault::configChanged`,
and 35 new test cases across `tst_hotkeydispatcher` (new, 22), `tst_scope` (+4),
`tst_scopemanager` (+5) and `tst_mainwindow_obsidian_commands` (+4). Each of the four
load-bearing behaviours is separately falsifiable: reverting the `[]` handling in
`HotkeyDispatcher::reconcile` reds `testEmptyArrayClearsTheShortcutAndDoesNotFallBack`; dropping the
root-slot walk reds six; removing the vault-open `load()` reds all three vault cases.
