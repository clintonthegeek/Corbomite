# Vault domain audit

Audit date: 2026-04-26. Scope: `/home/clinton/dev/Corbomite/libs/vault/`,
`/home/clinton/dev/Corbomite/libs/storage/`, `libs/core/include/corbomite/core/`
NoteMeta/NoteDocument/View, `libs/models/`. Spec:
`/home/clinton/dev/Corbomite/docs/obsidian-audit/domains/vault.md`.

## Architecture fit

Corbomite ships a single canonical `Corbomite::Vault` aggregate in
`libs/vault/include/corbomite/vault/Vault.h:59` that mirrors Obsidian's
`Vault` JS class shape with reasonable parity: a `TFile`/`TFolder`
hierarchy (`TAbstractFile.h:18`, `TFile.h:13`, `TFolder.h:11`) with NFC-
normalised `/`-separated paths keyed in a `std::unordered_map<QString,
unique_ptr<TAbstractFile>>` (`Vault.h:183`). The "tree of nodes" idea
maps cleanly; ownership lives on the Vault, callers hold raw pointers,
and a tombstone (`TAbstractFile::deleted`) plus a one-tick deferred-
deletion queue (`Vault.cpp:398`, `Vault.cpp:400`) gives synchronous
subscribers the same window-of-validity Obsidian's WeakMap-cached
delete semantics provide.

The split between *vault* (in-memory tree + low-level write API) and
*FileManager* (link-aware refactoring) matches Obsidian. `FileManager` lives at
`libs/vault/include/corbomite/vault/FileManager.h:23`. A separate
`DataAdapter` interface (`libs/storage/include/corbomite/storage/DataAdapter.h:37`)
is the abstract IO surface — production impl `FileSystemAdapter`
(`libs/storage/.../FileSystemAdapter.h:12`, `libs/storage/src/FileSystemAdapter.cpp`).
Plugin-facing facades (`VaultProxy`, `FileManagerProxy`) under
`libs/vault/include/corbomite/vault/proxies/` enforce permission gates.
`VaultConfig` (`libs/storage/include/corbomite/storage/VaultConfig.h:26`)
covers `.obsidian/*.json` schema-aware reads and writes.

The translation principle (Qt widgets / KDE primitives, Obsidian wire
format) is broadly observed: paths are NFC-normalised on the wire, JSON
config is written via Obsidian's exact "2-space indent, no trailing
newline" format (see post-processing in `VaultConfig.cpp:32`). Echo-
suppression for self-writes is implemented via mtime ledger
(`Vault.cpp:779`) — the moral equivalent of Obsidian's `file.saving`
flag, but framed differently.

That said, several Obsidian-side surface details have **not** been
ported: the `raw` event, the `config-changed` event, the
`Vault.process()` adapter-queued no-op-when-text-equal contract, the
Wiki/Markdown-link renderer, Obsidian's wiki-link rewrite during folder
rename, and `getResourcePath()`. All called out below.

## Implemented (parity-equivalent)

- **Tree model.** `TAbstractFile` (`libs/vault/src/TAbstractFile.cpp`),
  `TFile` (`libs/vault/src/TFile.cpp:22-32`) and `TFolder`
  (`libs/vault/src/TFolder.cpp`) implement the Obsidian shape: root
  `TFolder` at `"/"`, `getParentPrefix()` empty for root, `isRoot()`
  test, `setPath` updates `name` from basename, `TFile::setPath` re-
  derives `basename`/`extension` (lowercase, no dot —
  `TFile.cpp:9-19`). `getNewPathAfterRename` strips `<0x20` chars and
  trims, returns "" when detached (`TAbstractFile.cpp:22-35`). Matches
  the spec invariants in §8 of `vault.md`.

- **Path normalization.** `Corbomite::VaultPaths::normalize` at
  `libs/vault/src/PathNormalization.cpp:6-18` does backslash-to-slash,
  `//+` collapse, trailing-slash trim (preserving root), `./` strip,
  then `QString::NormalizationForm_C` (NFC). This is the correct
  Obsidian sequence and is applied at every Vault entry point
  (`Vault.cpp:105`, `:167`, `:255`, `:341`, `:407`, `:592`, `:610`,
  `:616`, `:673`, `:711`, `:798`, `:820`).

- **Tree queries.** `getAbstractFileByPath` / `getFileByPath` /
  `getFolderByPath` / `getMarkdownFiles` / `getFiles` /
  `getAllLoadedFiles` / `isEmpty` / `getRoot`, all in `Vault.cpp:101-149`.

- **Read API.** `read`, `readBinary`, `readRaw`, `cachedRead`
  (`Vault.cpp:151-179`). `cachedRead` populates a per-Vault
  `QHash<QString, QByteArray>` — reasonable spiritual port of
  Obsidian's `WeakMap<TFile, string>` cache.

- **Write API.** `modify`, `modifyBinary`, `append`, `process`, `create`,
  `createBinary`, `createFolder`, `rename`, `remove`, `copy`, `trash`
  (`Vault.cpp:181-530`). `process` serialises through a process-wide
  per-path mutex (`Vault.cpp:225-237`) — narrower than Obsidian's per-
  adapter queue but does prevent same-file RMW races.

- **`.trash/` writer.** `Vault::trash` with `useSystem=false`
  (`Vault.cpp:495-528`) writes to `<vault>/.trash/<basename>.<ext>`
  with ` 2`, ` 3`, …  collision suffix using `completeBaseName()` +
  `suffix()` from `QFileInfo`. Matches Obsidian's local-trash naming
  convention.

- **System-trash routing.** `Vault::trash` with `useSystem=true` calls
  `m_adapter->moveToTrash` (`FileSystemAdapter.cpp:124` calls
  `QFile::moveToTrash`, which lands in the XDG trash on Linux — the
  desktop behaviour Obsidian's IPC trash provides). Falls through to
  `.trash/` on system-trash failure (`Vault.cpp:492`), matching the
  Obsidian fallback exactly.

- **Echo suppression.** `Vault::stampSelfWrite` /
  `consumeSelfWrite` (`Vault.cpp:779-794`) gives a 1-second mtime
  ledger so the watcher's own re-fire on a self-write is dropped.
  Defense-in-depth byte-equal disk-vs-doc compare in
  `onExternalModified` (`Vault.cpp:721-730`) so even a leaked ledger
  entry doesn't cause spurious reload.

- **External-edit conflict UX.** `externalReloadConflict` signal
  (`Vault.h:154`) for the dirty-doc case, automatic clean-doc reload
  via `Markoff::Origin::ExternalReloadClean` (`Vault.cpp:759`),
  `resolveExternalReload` callback (`Vault.cpp:770`). This is the right
  shape for the "external edit while editing" problem Obsidian solves
  via `TextFileView.onExternal*`.

- **VaultConfig (`.obsidian/*.json`).** Schema-aware reads/writes for
  `app.json`, `appearance.json`, `community-plugins.json`,
  `hotkeys.json`, `daily-notes.json`, `templates.json`, `core-plugins.json`
  (`VaultConfig.h`, `VaultConfig.cpp`). The `mergeJson` helper
  (`VaultConfig.cpp:102-113`) preserves unknown keys at top level.
  The `2-space indent, no trailing newline` format is correctly
  emulated by squeezing Qt's 4-space output (`VaultConfig.cpp:32-47`).
  Legacy `core-plugins.json` array → object migration matches
  Obsidian's behaviour (`VaultConfig.cpp:209-256`).

- **`parseLinktext`.** `Markoff::parseLinktext` at
  `libs/markoff-family/libs/markoff-parser/src/LinkTextParser.cpp`
  splits at the first `#` and keeps the `#` in `subpath`; no `#`
  → `subpath = ""`. Matches the Obsidian contract precisely.
  Tests at `libs/markoff-family/libs/markoff-parser/tests/tst_linktext.cpp`
  cover headings, blockids, and missing-hash cases.

- **`generateMarkdownLink`.** Implemented at `FileManager.cpp:306-324`
  for the wikilink (Phase-5 slice). Delivers `[[basename]]`,
  `[[basename#sub]]`, `[[basename|alias]]` for `.md` targets;
  uses full relative path otherwise.

- **Filename validity.** `validateFileName` at
  `libs/vault/src/FileNameValidator.cpp:34-81` rejects `\\`, `/`, `:`,
  `*`, `?`, `"`, `<`, `>`, `|` and Windows reserved basenames
  (CON/PRN/AUX/NUL/COM1-9/LPT1-9). Stronger than the spec's loose
  description of "`checkPath` throws on illegal chars" — this is the
  right cross-platform set.

- **Plugin facade.** `VaultProxy` and `FileManagerProxy` enforce per-
  plugin tokens (`vault.read`, `vault.write`, `vault.events`,
  `metadata.read`). Forwarded signals fire only when `vault.events`
  is granted, checked once at ctor time (proxies/VaultProxy.h:86-99,
  proxies/FileManagerProxy.h:21-69). Aligns well with the
  "plugins cannot bypass the host's permissions" intent of an
  Obsidian-API-style port.

## Partial / divergent (with gap description)

- **Vault events.** Spec event quad is
  `create`/`modify`/`delete`/`rename`/`closed`/`raw`/`config-changed`.
  Corbomite has `created` / `modified` / `deletedFile` / `renamed` /
  `closed` (`Vault.h:140-149`). Two are missing or renamed:
  - `deleted` is named `deletedFile` to "avoid collision with
    `QObject::destroyed`-adjacent confusion" (`Vault.h:92` comment).
    Plugins porting from Obsidian will need a renaming shim.
  - `raw` is **absent**. Obsidian's `raw` event fires on every adapter
    notification, including paths inside `.obsidian/` (which never
    enter `fileMap`). Corbomite has no equivalent — `Watcher` actively
    excludes `.obsidian/`, `.corbomite/`, `.trash/`, `.git/`
    (`Watcher.cpp:28-39`), so even if a `raw` signal were added it
    would not fire for plugin-data files. This is a real gap for
    plugins that want to observe their own `data.json` for external
    edits.
  - `config-changed` is **absent**. Obsidian fires this both on local
    `setConfig` and on detected external `.obsidian/app.json` edits
    (debounced 500ms). Corbomite's `Vault` has no `getConfig`/
    `setConfig` API at all — `VaultConfig` reads/writes are direct
    file ops with no observer signal. Settings UIs that expect to
    refresh on git-pull-induced config changes won't.

- **`getConfig` / `setConfig` / `requestSaveConfig`.** Spec contract is
  in-memory `config` object with key allow-list partition between
  `app.json` and `appearance.json`, debounced 1-second leading-edge
  save. Corbomite has no in-memory config object on the `Vault`. Each
  caller goes through `VaultConfig` directly (synchronous read on every
  query, synchronous write on every set, no debounce, no
  partitioning). A plugin wishing to write `accentColor` would need to
  know it lives in `appearance.json`, not `app.json`. The spec's `AC`
  allow-list (Section 2 of vault.md) is **not modelled** in code.

- **`FileManager::renameFile` link rewriting.** `FileManager.cpp:148-216`
  walks `MetadataCache::allPaths()`, snapshots references to the old
  basename, then after the rename does a per-source
  `[[oldBase]]` → `[[newBase]]` / `[[oldBase|` / `[[oldBase#`
  string replacement (`FileManager.cpp:203-208`). Gaps vs. Obsidian:
  - Markdown links (`[text](Note.md)`) are **not** rewritten — only
    wiki-link forms.
  - The `[[full/path/Note]]` style (when `newLinkFormat == 'absolute'`
    or `'relative'`) is **not** rewritten — only basename matches.
  - Display-text aliases (`[[Old Note|Custom Display]]`) are
    rewritten target-side but the alias is preserved verbatim, so
    `[[New Note|Old Note]]` is plausible after a rename — Obsidian
    auto-elides equal alias.
  - No "always update / just once / don't update" prompt — Obsidian
    routes through `MetadataCache.updateInternalLinks` which honours
    `alwaysUpdateLinks`. Corbomite always updates silently.
  - No batched `fileManager.updateQueue` — concurrent `renameFile`
    calls can interleave with each other. (Per-path mutex in
    `Vault::process` only serialises one source's body update, not
    the cross-vault snapshot.)
  - **Folder rename does not propagate** — `FileManager::renameFile`
    treats the file by name, but a `TFolder` rename neither renames
    children in `m_fileMap` nor rewrites their references. The
    underlying `Vault::rename` (`Vault.cpp:337-373`) just moves the
    folder node without recursing. Renaming a folder will desync
    `m_fileMap` and lose every child node.

- **`processFrontMatter`.** Implemented (`FileManager.cpp:124-146`) as
  parse → variant-map → mutator → re-serialise. Two divergences:
  - **Key order is not preserved.** `FileManager.cpp:140-142` rebuilds
    `Markoff::YamlValue::emptyMap()` and re-applies the QVariantMap.
    `QVariantMap` is sorted alphabetically. Obsidian's `yI` ordered-
    assign preserves original frontmatter key order — Corbomite re-
    sorts on every edit. Spec invariant §8 explicitly calls this out.
  - **Nested maps are stringified.** Comment at `FileManager.cpp:65-68`
    acknowledges this — nested-map keys round-trip as YAML *strings*,
    not structured maps. A plugin that does `frontmatter.nested.key
    = …` cannot, by design, work today.

- **`getAvailablePathForAttachment`.** `FileManager.cpp:276-294` only
  honours "same folder as source" (Obsidian's
  `attachmentFolderPath = "."`). The spec's `./sub` (relative
  subfolder), absolute (vault-root anchored) and 250-byte truncation
  are not implemented. Comment at `:280-283` flags this explicitly.

- **`insertIntoFile` frontmatter merge.** `FileManager.cpp:296-304`
  does a plain `cur + content` / `content + cur` concat. Obsidian's
  semantics merge frontmatter blocks from both docs, right-biased by
  mode, then concatenate the bodies. A note inserted into another
  with conflicting frontmatter will end up with two YAML blocks today.

- **`Vault::copy`.** `Vault.cpp:404-416` only copies single `TFile`s by
  reading + creating; declines folder copies entirely. Obsidian's
  `Vault.copy` is recursive. Comment at `:414` admits scope-deferral.

- **Watcher coverage / `raw` events on `.obsidian/`.** Watcher excludes
  `.obsidian/` and `.corbomite/` from snapshot AND from event
  emission (`Watcher.cpp:28-39`). Spec §1 / §3 explicitly require the
  ability to observe `.obsidian/*.json` external edits (Obsidian's
  `watchHiddenRecursive` on Linux). Without this, plugin `data.json`
  external-edit detection and live `app.json` reload (the
  `config-changed` flow) cannot fire.

- **External rename detection.** Watcher emits `renamed` only when
  delete + create within one drain have an exact mtime match
  (`Watcher.cpp:139-156`). Reasonable best-effort, but on a real
  rename the mtime is preserved and Obsidian's adapter sees a single
  `renamed` event; Corbomite's drains may de-duplicate to either
  `delete` or `create` if the bursts cross drain boundaries.

- **`copy` path collision on case-insensitive FS.** `Vault::create`
  and `rename` use exact-match collision check (`Vault.cpp:256, :343`),
  not the case-insensitive equivalent of Obsidian's `getAbstractFileByPathInsensitive`.
  Creating `Note.md` when `note.md` exists on macOS will silently
  succeed in Corbomite's `m_fileMap` but the underlying FS will
  overwrite. Spec invariant §8: "`Vault.getAvailablePath(base, ext)`
  returns a path whose case-insensitive form is free."

- **`CaseSensitivityProbe`.** Class exists at
  `libs/storage/include/corbomite/storage/CaseSensitivityProbe.h:19`
  with a sound implementation. **No callers** of `isCaseSensitive`
  appear anywhere outside the probe itself
  (`grep -rn CaseSensitivityProbe::` returns only the .cpp). The
  probe is dead code — `Vault` makes no case-insensitive lookup.

- **System-trash on Wayland.** `QFile::moveToTrash` on KDE Plasma 6
  works; on minimal X11/Wayland sessions without a trash daemon it
  silently fails. The `useSystem` arg's spec-mandated "fall through to
  `.trash/`" path (`Vault.cpp:492`) handles the failure correctly.

## Missing

- **`raw` signal.** No equivalent exists. Plugin-data external-edit
  observation impossible.

- **`config-changed` signal.** No equivalent. Settings UI can't refresh
  reactively on external `.obsidian/app.json` edits.

- **`Vault::getConfig` / `Vault::setConfig`.** Vault has no in-memory
  config object. The split-by-AC-allow-list partition between
  `app.json` and `appearance.json` is not modelled — callers must know
  which file each key lives in.

- **`Vault::getAvailablePath` / `getAvailablePathForAttachments`.**
  Only `getAvailablePathForAttachment` exists (lowercased single-source
  variant in `FileManager`). The general-purpose Obsidian helper is
  not in `Vault`.

- **`Vault::getAbstractFileByPathInsensitive`.** No case-insensitive
  lookup helper. The `CaseSensitivityProbe` is dead code without a
  consumer.

- **`Vault::getResourcePath`.** No `app://local/<path>?<mtime>` URL
  generator. The image renderer path goes through
  `VaultResourceProvider` (`libs/core/include/corbomite/core/VaultResourceProvider.h:24`)
  which is shape-only — concrete impls live elsewhere. Plugins porting
  Obsidian code that calls `vault.getResourcePath(file)` won't have
  an equivalent.

- **`FileManager::registerFileParentCreator` / `unregisterFileCreator`.**
  No extension-keyed new-file-parent factory map. Parent selection in
  `getNewFileParent` (`FileManager.cpp:236-245`) is fixed to "use
  hint's parent or root."

- **`FileManager::deleteProperty` / `renameProperty`.** Declared at
  `FileManager.h:40-41`; bodies stub-return `false`
  (`FileManager.cpp:217-218`).

- **`FileManager::createNewMarkdownFile` localisation.** Falls back to
  `"Untitled"` (`FileManager.cpp:223`) — not `i18n("Untitled")`. Spec
  doesn't require localisation per se, but Corbomite's CLAUDE.md
  ("use `i18n()` for all user-visible strings") is violated here.

- **`FileManager::storeTextFileBackup` and `notifyForBulkUndo`.**
  Obsidian's 30-second "Undo bulk operation" toast that restores both
  content and mtime — no equivalent in Corbomite. Folder rename
  recovery, mass-property edits, etc. cannot be undone.

- **`FileManager::downloadAttachmentsForNote`.** Remote
  `http(s)://` / `data:` image localisation — not present.

- **BOM handling.** `Vault::read` (`Vault.cpp:151-156`) returns the raw
  bytes; no leading-`U+FEFF` strip. `cachedRead` and the watcher cache
  store the bytes verbatim. Spec §3 ("`Vault.read` strips leading
  U+FEFF before returning"): violated. A vault file authored on
  Windows with a UTF-8 BOM will display the BOM in the editor.

- **`Vault.process` "no-op when text equal" optimisation.**
  Obsidian's adapter `process` skips the write entirely when
  `mutator(text) === text`. Corbomite's `Vault::process`
  (`Vault.cpp:240-250`) always writes, which means a no-op
  `processFrontMatter` mutation costs an mtime bump and a watcher
  re-fire (suppressed by ledger, but wasteful).

- **`createNewMarkdownFileFromLinktext` parent inference.**
  `FileManager.cpp:258-263` ignores the linkText path component; if
  `linkText = "subfolder/Note"`, Obsidian creates
  `subfolder/Note.md`; Corbomite creates a flat
  `subfolder/Note.md` only when the existing parent is the right
  folder — and the slash inside the filename will trigger the bad-
  character rejection if validated.

- **`renameFile` for `TFolder`.** Per the Partial section above, no
  recursive renaming of children. The `m_fileMap` keys for descendants
  remain stale until the watcher catches up; observers iterating the
  tree mid-rename will see a malformed state.

## Notable translation successes

- **Per-vault NoteDocument cache + save echo-suppression** (`Vault.cpp:589-669`,
  `:779-794`). The mtime-ledger + byte-equal-disk-compare pair gives
  defence-in-depth against the "self-write fires watcher fires reload"
  loop. Better than Obsidian's `file.saving` flag because it survives
  cross-process editor races (the editor may not be the writer).

- **U+FFFC terminal guard in `saveDocument`** (`Vault.cpp:627-634`).
  Refuses to write the canonical buffer if it contains
  `QChar::ObjectReplacementCharacter`. This catches a class of
  presentation-layer-leaking-into-source bugs that Obsidian doesn't
  have to worry about because its editor is browser-text. Smart
  Corbomite-specific addition.

- **Deferred-deletion queue** (`Vault.cpp:398-401`). Single-frame-
  delayed cleanup of removed nodes so synchronous subscribers can
  still read the tombstone safely. Cleaner than Obsidian's "delete
  later" + WeakMap pattern, and matches Qt's natural one-tick
  rhythm.

- **`FileNameValidator`** (`FileNameValidator.cpp`). Cross-platform
  reserved-name handling that's friendlier than Obsidian's regex
  (`UT`/`WT`) — adds an `isFinal` flag for typing-in-progress
  validation that doesn't yell at the user mid-keystroke.

- **`VaultConfig` JSON formatting** (`VaultConfig.cpp:19-48`).
  Pragmatic post-process of Qt's 4-space output to Obsidian's 2-space
  no-trailing-newline format. The work-around comment is honest about
  why.

- **Markoff parse pool ownership** (`Vault.h:193`, `Vault.cpp:50, :595, :69-70`).
  `m_parsePool` is a Vault-lifetime resource; explicit `qDeleteAll(m_docs)`
  in dtor before the pool is destroyed (`Vault.cpp:64-71`) prevents the
  UAF that Qt parent-child ordering would otherwise produce. Well-
  reasoned, well-commented.

- **PathNormalization at every Vault entry point.** Search shows it's
  consistently applied — that's the kind of discipline that pays back
  during cross-tool interop.

## Notable concerns / suspected bugs

1. **Folder rename loses descendants.** `Vault::rename`
   (`Vault.cpp:337-373`) takes the renamed node out of `m_fileMap`,
   updates its single key, and re-inserts. For a `TFolder` with
   children, the descendants' `m_fileMap` keys still point to the
   old prefix; their `path` strings are stale; their `parent` link is
   intact but their string identity is wrong. A subsequent
   `getFileByPath("oldFolder/child.md")` returns `nullptr` while
   `getFileByPath("newFolder/child.md")` returns `nullptr` too. The
   only recovery is a watcher re-snapshot. **Recommend a recursive
   rekey** with cache invalidation per descendant, plus emitting a
   `renamed` per descendant (Obsidian's adapter does this — see spec
   §13 Q6).

2. **`processFrontMatter` re-sorts keys alphabetically.**
   `FileManager.cpp:140-142` builds `next = YamlValue::emptyMap()`
   and applies a `QVariantMap` (sorted by key). Spec invariant §8
   explicitly requires preservation. Open vault, open in Obsidian,
   re-edit a YAML key in Corbomite → key order changes silently. This
   will produce noisy git diffs even when no semantic change happened.

3. **Read does not strip BOM.** Corbomite reads `.md` files verbatim;
   no `if (body.startsWith(QByteArray("\xef\xbb\xbf"))) body.remove(0, 3);`
   anywhere. A vault containing a BOM-prefixed note will display
   `﻿` as the first character in the editor. Likely cosmetic but
   user-visible. Round-tripping (read → edit → save) will preserve the
   BOM, which is at least consistent — but if the user removes the
   BOM in the editor, the file will be byte-different on save in a
   way the user can't see.

4. **`Vault::create` does not collision-check case-insensitively.**
   `Vault.cpp:256` does `if (m_fileMap.count(rel))` — but on macOS/
   Windows, `Note.md` and `note.md` collide on disk while remaining
   distinct keys. Either a case-insensitive scan or case-folding the
   key on insert is required.

5. **`Vault::onExternalCreated` parent-folder inference races.**
   `Vault.cpp:679-683` walks the path tail for a parent; if the parent
   doesn't yet exist in `m_fileMap` (because the watcher saw the
   child's create before the parent's), the new node is parented at
   root. Obsidian builds intermediate folders eagerly. The `create`
   path (`Vault.cpp:268-291`) does this correctly; the watcher path
   does not.

6. **Watcher excludes `.obsidian/` from observation.**
   `Watcher.cpp:28-39` filters out `.obsidian/`, `.corbomite/`,
   `.trash/`, `.git/`. This prevents the spec-mandated
   `config-changed` reload, the spec-mandated `raw` event for plugin
   data, and any future feature that wants to react to vault config
   changes from a git pull. The exclusion appears to be a perf
   shortcut; recommend at minimum watching `.obsidian/`.

7. **`FileManager::renameFile` reference-detection misses many forms.**
   The string-replace at `FileManager.cpp:203-208` only catches
   `[[oldBase]]`, `[[oldBase|`, `[[oldBase#`. Misses:
   - Markdown-style `[label](Note.md)` and percent-encoded variants
   - `[[folder/Note]]` (full path forms)
   - `[[oldBase \| alias]]` with whitespace
   - `[[oldBase ]]` with trailing space
   - References inside frontmatter (`note: "[[oldBase]]"`)
   The MetadataCache snapshot identifies *which sources* to walk, but
   the rewrite logic itself is shallower than the cache knows. This
   is a soft data-loss bug: links break, no notice given.

8. **`FileManager::trashFile` ignores user `[Files]/TrashOption`.**
   `FileManager.cpp:325` always calls `m_vault->trash(f, false)`
   (i.e. local trash, never system). Yet `promptForDeletion`
   (`:407-434`) reads the same setting via KConfig. So programmatic
   `trashFile()` and dialog-routed `promptForDeletion()` disagree.
   The plugin-facing `FileManagerProxy::trashFile` therefore *also*
   diverges from user intent.

9. **`writeConfigJson` doesn't accept JSON values that aren't object/array.**
   `Vault.cpp:454-457` returns `false` for primitive top-level JSON
   (`true`, `42`, `"string"`). Obsidian's `writeJson` accepts any
   JSON-stringifiable value. Edge case but technically a divergence.

10. **`Watcher::stop()` clears `m_basePath`** (`Watcher.cpp:71`) **after**
    starting watching but `start()` calls `stop()` first
    (`Watcher.cpp:43`). On `Vault::unload`, the watcher's `m_basePath` is
    cleared while the `drainPending` queue may still have a pending
    50ms QTimer; on fire it short-circuits via `m_basePath.isEmpty()`
    (`Watcher.cpp:104`). OK in current flow but fragile — a
    `m_drainTimer.stop()` before clearing `m_basePath` (already
    present at line 70) is what saves it.

11. **`Vault::trash` system path doesn't preserve `.trash/` collision logic.**
    On `useSystem=true` success it just deletes; only the `useSystem=false`
    branch does the ` 2`, ` 3` suffixing. That matches spec — system
    trash is the OS's responsibility — but worth noting that
    `QFile::moveToTrash` on Linux handles its own collision suffixing
    via the FreeDesktop trash spec.

12. **`FileManager::renameFile` does not honour `Q_EMIT renameStarted`
    cancellation.** Signals are fire-and-forget; subscribers can't
    veto. Obsidian doesn't expose veto either, but the `signalize-
    intent + commit` separation here suggests one was planned —
    currently inert.

## Vault-format compatibility risks (on-disk contract)

The audit's stated principle is "the on-disk format must match Obsidian
exactly." Risks ranked by user-visible impact:

1. **Frontmatter key order churn (high impact).** `processFrontMatter`
   sorts keys alphabetically. Concrete failure: user with a vault used
   from both apps will see noisy git diffs every time Corbomite edits
   any metadata, plus Obsidian-side templates that depend on key
   order (rare but possible) will break. Concern §2 above.

2. **No BOM strip on read; round-trip preserves bytes (medium impact).**
   Cosmetic in editor, but a Corbomite save followed by an Obsidian
   open will show the same BOM, so cross-tool churn is zero — only
   editor display is wrong. A user removing the BOM in Corbomite will
   produce a byte-divergent save. Concern §3.

3. **Wikilink-only rename rewrite (high impact for markdown-link
   users).** Vaults with `useMarkdownLinks=true` get **no** rename
   refactoring at all, because the `[[…]]`-only rewrite skips the
   `[label](Note.md)` form. Spec invariant §8: `generateMarkdownLink`
   honours `useMarkdownLinks`; Corbomite's link-rewrite does not.

4. **`m_fileMap` key staleness on folder rename (high impact).** See
   concern §1. Until the watcher catches up, the in-memory tree is
   inconsistent. If the user moves a folder and immediately edits a
   contained file via the file-tree, the tree will not surface the
   file by its new path.

5. **`.obsidian/app.json` 2-space format, key partition, unknown-key
   preservation (medium-low impact, mostly correct).** `VaultConfig`
   handles format and unknown-key preservation cleanly. The miss is
   the *partition*: a Corbomite write that puts e.g. `accentColor`
   into `app.json` instead of `appearance.json` will be tolerated by
   Obsidian on read (it merges both files) but on next Obsidian save,
   the key gets duplicated to both files or relocated. Recommend
   modelling the `AC` allow-list and routing writes accordingly.

6. **`.trash/` naming convention (low impact, correct desktop side).**
   Corbomite uses `<basename> <N>.<ext>` matching Obsidian desktop.
   Mobile (`<basename> N` no separator) is not relevant —
   Corbomite is desktop-only.

7. **Alias auto-elision in link rewrite (low impact).** Concern §7
   point 3. Vaults edited cross-tool will gain redundant aliases over
   time on rename; Obsidian doesn't normalise these on read.

8. **`processFrontMatter` nested-map stringification (low impact).**
   Documented design limitation. A vault with deeply nested YAML
   (rare in practice) will see structured maps collapsed to YAML
   string values on edit.

9. **`.OBSIDIANTEST` probe absence (no impact).** Corbomite's
   `CaseSensitivityProbe` writes `.case-probe-<uuid>` instead, so
   doesn't pollute the vault root with the Obsidian probe name. Spec
   §3 explicitly says the name is not load-bearing.

10. **No `getResourcePath` (low impact for vault format; high impact
    for embed correctness).** Plugins that compute image hot-reload
    URLs via `app://local/...?<mtime>` won't have a Corbomite
    equivalent to call. But the on-disk format is unaffected.

---

**Bottom line.** The vault domain is the most thoroughly built-out
parity surface in Corbomite's tree — `Vault` + `FileManager` +
`VaultConfig` + `DataAdapter` + the `TFile`/`TFolder` hierarchy + the
plugin proxies cover ~70% of the Obsidian surface area with sensible
shape. The biggest gaps are *event-surface completeness* (`raw`,
`config-changed`), *folder-rename correctness* (descendant rekeying +
event fan-out), *frontmatter key-order preservation*, and *link-
rewrite fidelity* (markdown links, full-path wiki forms,
`MetadataCache`-driven detection used for the *what* but not the
*how*). None of those are conceptually hard to fix; all are tractable
without architectural changes. The case-insensitivity probe being
dead code is the most "shipped but not wired" item.
