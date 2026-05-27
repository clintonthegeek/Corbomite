# Cluster D.4c — Bases undo/redo for value edits

> **Design spec, 2026-05-27.** Sub-project of [Cluster D — Bases UI completion](../plans/2026-04-26-cluster-d-bases-ui-completion.md), scope item #8 ("Undo stack integration — cell edits go through QUndoStack"). Follows D.4a (cell interactivity) and D.4b (export/+New). Pre-approved in brainstorm; no open questions.

## Goal

Cell edits and properties-drawer edits in a Bases view become undoable/redoable
via the application's standard Edit ▸ Undo / Edit ▸ Redo (Ctrl+Z / Ctrl+Y).
Each `.base` view owns an isolated history. Undo never clobbers a change made
to the same frontmatter field outside Bases (editor tab, external app, another
row); when such drift is detected, the offending command is skipped, the user
is told, and the rest of the history stays usable.

## Decisions (locked in brainstorm)

1. **Standalone, stale-guarded stack.** The Bases view owns its own
   `QUndoStack`. It does **not** coordinate with the editor's undo or any
   vault-wide undo abstraction (none exists). Every command verifies the
   on-disk value before reversing, and refuses to overwrite an external change.
2. **Value edits only.** Undoable mutations are (a) cell edits via
   `BasesTreeModel::setData` — including checkbox toggle — and (b)
   `PropertiesDrawer` frontmatter commits. The "+New" file creation (D.4b),
   row rename/delete (D.4a context menu), and sort/group/view-config changes
   (D.3) are **out of scope** — "+New" is a deliberate creative act with its
   own tab, and the others route through their own dialogs/persistence.
3. **Skip + notify + neutralize on drift.** When the on-disk value no longer
   matches what the command last wrote, the command does not write, notifies
   the user via a transient banner, and marks itself neutralized so it (and its
   future redo/undo) become no-ops. Because a no-op `undo()` still lets
   `QUndoStack` advance its index, the stale command is effectively skipped and
   older history remains reachable.

## Architecture

### Component: `CmdSetFrontMatter` (new)

New unit `libs/bases/include/corbomite/bases/BasesCommands.h` +
`libs/bases/src/BasesCommands.cpp`. A single `QUndoCommand` subclass:

```cpp
namespace Corbomite::Bases {

class CmdSetFrontMatter : public QUndoCommand {
public:
    // notify is called (with a user-facing i18n string) when the command
    // detects external drift and skips. Widget-free; BasesView injects a
    // sink that shows the banner.
    CmdSetFrontMatter(Corbomite::FileManager *fm,
                      Corbomite::TFile *file,
                      QString key,
                      QVariant newValue,
                      std::function<void(const QString &)> notify);

    void redo() override;   // apply newValue
    void undo() override;   // restore oldValue

private:
    Corbomite::FileManager *m_fm;
    Corbomite::TFile       *m_file;
    QString   m_key;
    QVariant  m_newValue;
    QVariant  m_oldValue;          // captured lazily on first redo()
    bool      m_oldCaptured = false;
    bool      m_neutralized = false;
    std::function<void(const QString &)> m_notify;
};

} // namespace Corbomite::Bases
```

It performs **all reads and writes inside one `processFrontMatter` mutator**, so
it operates on the frontmatter freshly parsed from disk — sidestepping any
`MetadataCache` async-update lag. The mutator receives a `QVariantMap &`, the
same seam already used by `BasesTreeModel::setData` / `PropertiesDrawer`.

`redo()`:
- Runs `m_fm->processFrontMatter(m_file, mutator)`. Inside:
  - `current = fm.value(m_key)`.
  - **First application** (`!m_oldCaptured`): record `m_oldValue = current`,
    set `m_oldCaptured = true`, write `fm.insert(m_key, m_newValue)`.
  - **Re-redo after an undo**: stale check — if `current != m_oldValue`
    (the state undo should have left), set a local `stale` flag and return
    without mutating; else write `fm.insert(m_key, m_newValue)`.
- After the call, if `stale`: set `m_neutralized = true` and call `m_notify(...)`.
- If `m_neutralized` already true on entry, return immediately (no-op).

`undo()`:
- If `m_neutralized`, no-op.
- Runs `processFrontMatter`. Inside:
  - `current = fm.value(m_key)`.
  - Stale check — if `current != m_newValue` (what redo wrote), set `stale`,
    return without mutating; else restore `fm.insert(m_key, m_oldValue)`.
    - If `m_oldValue` is an invalid `QVariant` (key was absent before the
      edit), the restore is `fm.remove(m_key)` instead of inserting an empty
      value — preserving the "key did not exist" pre-state.
- After the call, if `stale`: `m_neutralized = true`, `m_notify(...)`.

Equality uses `QVariant::operator==`. Both operands come from the same parse
path (`processFrontMatter`'s `QVariantMap`), so types align; a missing key is an
invalid `QVariant` on both sides and compares equal.

`setText(i18n("Edit \"%1\"", m_key))` so the Edit menu shows a meaningful label.

> **Note — `processFrontMatter` writes unconditionally.** When the mutator
> declines to change the map (stale or neutralized), `processFrontMatter` will
> still re-serialize and write the unchanged frontmatter. This is a harmless
> idempotent write (byte-identical content). If a future profile shows this
> causing spurious file-change churn, add an early-out, but it is **not**
> required for correctness and is out of scope here.

### Component: `BasesView` chokepoint + stack (modified)

- New member `QUndoStack m_undoStack;` on `BasesView`.
- New slot `void pushFrontMatterEdit(Corbomite::TFile *file, const QString &key, const QVariant &newValue);`
  that constructs a `CmdSetFrontMatter` (passing a notify sink bound to the
  view's banner) and calls `m_undoStack.push(cmd)`. `push()` auto-runs `redo()`,
  performing the write synchronously.
- New public `void undo();` / `void redo();` (or `QUndoStack *undoStack();`)
  for the host to drive. Recommended: expose the two slots so the host stays
  ignorant of the stack type.
- The stack is **cleared whenever the view (re)loads a `.base` file**, so a
  history never spans two different base documents or survives an external
  reload of the underlying notes.
- Notify sink: a small lambda that shows `m_errorBanner` with the message for
  ~a few seconds (reuse the existing banner show idiom from D.3; if it has no
  auto-hide, a `QTimer::singleShot` hide is acceptable).

### Wiring: model / drawer → chokepoint (modified)

Today both edit paths call `m_fm->processFrontMatter(...)` directly. They are
changed to **emit a request instead of writing**, keeping them widget- and
stack-agnostic (and unit-testable without a `QUndoStack`):

- `BasesTreeModel`: add signal
  `void frontMatterEditRequested(Corbomite::TFile *file, const QString &key, const QVariant &value);`
  `setData` emits it (instead of calling `processFrontMatter`) and returns
  `true` synchronously. Because the connected slot runs the write synchronously
  via `QUndoStack::push`→`redo`, the post-`setData` `dataChanged`/repaint still
  observes the committed value. (`setData` may emit `dataChanged` itself after
  the synchronous push, matching current behavior.)
- `PropertiesDrawer`: add the analogous signal and emit on commit instead of
  calling `processFrontMatter`.
- `BasesView` connects both signals to `pushFrontMatterEdit`.

> The `PropertyId pid` / `key` resolution presently done inside
> `BasesTreeModel::setData` stays in the model; only the final write call is
> replaced by the signal emission carrying the resolved `key`.

### Wiring: app action routing (modified)

`MainWindow`'s existing `KStandardAction::undo` / `redo` lambdas
(`src/app/MainWindow.cpp:1229` / `:1239`) gain a Bases branch, mirroring the
existing `activeEditor()` and `CanvasFileView` branches:

```cpp
KStandardAction::undo(this, [this]() {
    if (auto *bv = qobject_cast<Corbomite::Bases::BasesView *>(activeView())) {
        bv->undo();
        return;
    }
    auto *editor = activeEditor();
    ... // unchanged editor path
}, ac);
```

`activeView()` here means "the view widget of the currently-active workspace
leaf." The exact accessor is whatever `activeEditor()` already uses internally
to find the active leaf's widget — **resolve by reading `activeEditor()`'s
implementation during planning; do not guess a new accessor.**

## Data flow (happy path)

1. User toggles a checkbox / commits a cell editor / edits a drawer field.
2. `BasesTreeModel::setData` (or `PropertiesDrawer`) emits
   `frontMatterEditRequested(file, key, newValue)`.
3. `BasesView::pushFrontMatterEdit` builds `CmdSetFrontMatter` and pushes it.
4. `push()` runs `redo()` → `processFrontMatter` captures `oldValue`, writes
   `newValue` to disk. The metadata cache picks up the change via the normal
   file-watch path; the query re-evaluates and the table repaints as today.
5. Ctrl+Z on the focused Bases tab → `MainWindow` undo lambda → `BasesView::undo`
   → `m_undoStack.undo()` → `CmdSetFrontMatter::undo()` restores `oldValue`
   (after passing the stale check).

## Error / edge handling

- **External drift** (the headline case): handled by the per-command stale
  check + neutralize + notify, as specified above. No clobber, history stays
  usable.
- **File deleted while in history**: `processFrontMatter` returns false / no-ops
  for a null or non-`.md` file. `CmdSetFrontMatter` treats a failed
  `processFrontMatter` like drift (neutralize + notify). Stack clears on the
  next view (re)load anyway.
- **Key absent before edit**: `m_oldValue` is invalid `QVariant`; undo removes
  the key rather than inserting an empty value (see `undo()` above).
- **No merging / coalescing in v1.** Each edit is a discrete command (including
  repeated edits to the same cell). `mergeWith`/`id()` are intentionally not
  implemented; revisit only if rapid-edit history proves noisy.

## Out of scope

- "+New" undo, row rename/delete undo, sort/group/view-config undo (decision 2).
- Coalescing consecutive edits (above).
- A vault-wide / cross-view unified undo (decision 1).
- Eliminating the idempotent re-write on a declined mutator (note above).

## Testing

New automated tests, run with `QT_QPA_PLATFORM=offscreen`, built on the
existing bases temp-vault + `FileManager` harness (pattern from
`tst_bases_tree_model` / `tst_file_manager`):

**`tst_bases_commands` (new) — `CmdSetFrontMatter` against a real temp-vault `FileManager`:**
1. `redo` writes `newValue` to the note's frontmatter; `oldValue` captured.
2. `undo` restores the prior value on disk.
3. `redo` after `undo` re-applies `newValue`.
4. **Drift before undo:** write a different value to the key directly (via a
   second `processFrontMatter`), then `undo` → on-disk value is unchanged
   (no clobber), the notify sink fired once, and a subsequent `redo`/`undo`
   are no-ops (neutralized).
5. **Key-absent pre-state:** editing a key not present before → `undo` removes
   the key entirely (frontmatter returns to no-key state).
6. **Label:** `text()` contains the key.

**`tst_bases_tree_model` (extend):**
7. `setData` emits `frontMatterEditRequested(file, key, value)` with the
   resolved key/value and no longer writes directly (assert the signal fires;
   assert the model does not mutate disk on its own).

**Manual / pending-user-eyeball** (offscreen Qt can't drive the full app action
routing + focus): Ctrl+Z / Ctrl+Y on a focused Bases tab actually undoing a
checkbox toggle and a drawer edit; the banner appearing on a genuine external
drift. Joins the D.2–D.4b verification backlog. If `tst_mainwindow_action_wiring`
can reach a `BasesView`-active state cheaply, add a routing assertion there;
otherwise leave to manual.

Per project rule: **tests define expected behavior — when one fails, fix the
code, not the test.**

## Files touched

- **New:** `libs/bases/include/corbomite/bases/BasesCommands.h`,
  `libs/bases/src/BasesCommands.cpp`, `libs/bases/tests/tst_bases_commands.cpp`
  (+ CMake registration).
- **Modified:** `libs/bases/include/corbomite/bases/BasesView.h` +
  `libs/bases/src/BasesView.cpp` (stack, chokepoint, undo/redo, notify,
  clear-on-load); `BasesTreeModel.{h,cpp}` (signal, setData);
  `PropertiesDrawer.{h,cpp}` (signal, commit); `src/app/MainWindow.cpp` (undo/
  redo routing); `libs/bases/tests/tst_bases_tree_model.cpp` (extend);
  `libs/bases/CMakeLists.txt` / `libs/bases/tests/CMakeLists.txt`.

## Verify-against-source flags (resolve during planning, do not guess)

- `activeView()` accessor — read how `MainWindow::activeEditor()` finds the
  active leaf's widget and reuse that path.
- The `m_errorBanner` show/hide idiom in `BasesView` (introduced D.3) — reuse
  exactly; confirm whether it auto-hides.
- The `BasesEntry` / temp-vault `FileManager` construction idiom used by the
  existing bases tests, for `tst_bases_commands` setup.
- `processFrontMatter`'s return value and behavior for null/non-`.md` files,
  to confirm the "failed write = drift" handling.
- The exact key-resolution code in `BasesTreeModel::setData` that must stay put
  while only the write call is replaced by the signal.
```
