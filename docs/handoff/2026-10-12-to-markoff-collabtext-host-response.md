# Corbomite response: CollabText host boundary

**Date:** 2026-10-12  
**Status:** proposed design; no production cutover or persistence implementation implied.

We accept the ownership split in Markoff's October 12 handoff. Corbomite owns
document lifecycle, durable storage and device-private writer authority. Markoff
owns the editing protocol and its recovery state machine. Please implement against
an explicit provider boundary, rather than moving CRDT orchestration into our
Vault or requiring editor leaves to manage persistence.

## What our integration actually does

- `libs/core/src/NoteDocument.cpp` constructs `MarkoffDocument(quint16{1})` and
  exposes that document to all views. There is no durable writer authority.
- `libs/vault/src/Vault.cpp`, `openDocument()`, caches one NoteDocument per
  normalized path within a Vault instance and seeds it from Markdown on open.
  This coordinates ordinary panes, but is not a cross-process writer lease.
- `saveDocument()` writes `serializeForSave()` bytes and marks an export watermark
  clean. It does not store operations. Our `SQLiteIndex` is a rebuildable search
  projection and must not become the accepted-history store.
- `reconcileOpenDocument()`, `onExternalModified()` and
  `resolveExternalReload()` replace contents through `resetContent()`. Vault
  `modify()`/`process()` and FileManager link rewriting also reach this boundary.
  These are production mutation paths, not just editor typing.
- Editor completion and insertion use Markoff's block/flat-edit APIs; views use
  MarkdownView and ephemeral state. We found no host remote-operation pipeline.

Consequently this adoption is a persistence feature with a file-editing migration,
not a replica-ID substitution. Transport and collaboration UI can follow later.

## Shared identity and history

Proposed shared location: `<vault>/.corbomite/collaboration/v1/`, containing a
versioned manifest (path bindings to opaque document UUIDs and epoch 1) and
`documents/<uuid>/` histories. Markdown remains unchanged. These are authoritative
document artifacts, distinct from `.obsidian` settings and disposable caches;
sync and backup must explicitly include them. Missing history for a registered
document is an error, not permission to seed it again.

Store exact Markoff bundles as immutable transaction records. A recoverable local
journal commits identity registration and initial history together; publication
and recovery rules must prevent a manifest alone from making a document writable.
Do not synchronize a live SQLite database or treat ordinary file synchronization
as a multi-device transaction protocol. Later transport must merge immutable
records and binding changes and wait for complete import records before replay.

Rename/move within a vault preserves UUID and history; folder moves update all
bindings. Cross-vault movement preserves identity only through an explicit
history-aware transfer; an ordinary Markdown-only import creates a new document.
Duplicate/fork creates a new UUID and one new initial history from visible bytes,
without inheriting undo. A full document restore or synchronized continuation
preserves the UUID and accepted history; restoring only older Markdown is an
external edit, not a rollback of the history.

Copied metadata at conflicting paths requires resolution before writes: continue
the existing document or fork it. Do not infer intent from path or content hashes.
Untracked external renames need the same caution; byte equality is not proof of
identity. Retain history on deletion initially so trash/restore remains possible.

First import is serialized by a document-service lease: reserve UUID, durably
record an import transaction and source bytes, ask Markoff to produce the seed
bundle, and commit that exact bundle with registration. Restart reconciles the
original transaction. Never rerun seeding after an uncertain commit. Other devices
replay the committed seed. Two offline devices independently importing an
unregistered file cannot guarantee one history: require a designated bootstrap
owner until a shared registration protocol exists, and surface divergent imports
as a conflict rather than merging namespaces by filename.

## Private state and ownership

Add a Corbomite document service in Core, with persistence implementations in
Storage and lifecycle coordination in Vault. NoteDocument holds the session and
provider lifetime; leaves only bind to its MarkoffDocument. Coordinate sessions
by document UUID, including multiple Vault instances, and acquire an OS-backed
writer lease across processes before allocating or recovering.

Private location: `QStandardPaths::AppLocalDataLocation/collaboration/v1/`
(using the existing separate release/dev application identities). Storage owns
durable ReplicaState, reservations, pending transaction recovery and local undo
records here, outside the vault. Actors persist across normal reopen; separate
installations obtain independent authority. Markoff specifies the scope and
serialization of allocation and undo records; Corbomite stores them atomically.

Loss/corruption/restored or copied authority stops writing and returns a typed
error. An explicit recovery starts fresh actors, replays accepted history and
reconciles outstanding reservations where records are available; it never resumes
old counters from a stale backup. A copied installation must undergo that recovery
before acting as an independent writer. Portable software cannot reliably detect
every cloned private directory: do not promise automatic clone detection without
an external authority. Preserve diagnostics and offer explicit re-enrollment.

## Provider contract to agree before implementation

Markoff should publish a small Qt/C++ contract with opaque versioned records:

| Operation | Required contract |
| --- | --- |
| Open session | Explicit document UUID, epoch, provider and exclusive writer lease; fallible recovery before writable state |
| Read accepted history | Stable revision/prefix and exact ordered bundle bytes; replay outcomes stay Markoff-owned |
| Reserve allocation | Atomically persist scoped actor/counter reservations before returning authority; full intervals retained |
| Prepare transaction | Persist original transaction ID, exact bytes, reservations and recovery metadata before append |
| Append transaction | Idempotent ID bound to exact bytes; atomic whole bundle; committed / definitively absent / uncertain, plus typed diagnostic |
| Inspect transaction | Reconcile the original ID and bytes after restart; conflicting bytes for an existing ID are an error |
| Store local history | Opaque versioned undo state bound to an accepted revision; recoverable association with its action commit |
| Export | Exact Markdown bytes plus accepted revision; host acknowledges only the revision actually written |

These are semantic operations, not final signatures. Markoff must specify callback
ordering, thread affinity, provider lifetime, reentrancy and how synchronous edit
APIs expose pending/failed durability. A change notification is not a commit
acknowledgement. Start with a local serialized implementation; do not require
network I/O per keystroke.

Corbomite implements the journal, atomic append/inspection, reservation storage,
leases and user-visible error plumbing. Markoff creates bundles and scoped gaps,
tracks cross-stream action groups, rebuilds tentative engines from accepted
history, and enforces the commit barrier. For proven absence it supplies the full
reserved intervals and gap bundle; Corbomite durably commits it before Markoff
permits subsequent affected-stream writes. Uncertainty blocks reconciliation-
dependent writes; it never creates replacement operations or terminal gaps.
Crash recovery must also cover committed actions whose private undo-state write
did not finish, using the prepared transaction metadata.

## Existing APIs and file edits

Keep MarkdownView, serializer, completion and leaf-binding APIs as stable as
possible. Composite BlockId and scoped-position changes are Markoff-owned API
migrations; provide opaque comparison/hash/token APIs and a host migration list.
Corbomite updates its consumers without inventing block identities or decoding
Core wire formats. Provide an explicitly ephemeral in-memory provider for tests,
previews and standalone rendering; durable writable sessions require the host
contract. The old numeric constructor must not silently create production actors.

For registered documents, reopen replays history. `loadFromMarkdown()` becomes
first import, and wholesale `resetContent()` cannot silently discard identities.
Corbomite detects external/plugin bytes and manages conflict decisions; Markoff
must expose an action to reconcile accepted replacement bytes against the current
document through its normal bundle pipeline. Until that is implemented, divergent
file writes require explicit resolution and block further writes rather than
resetting a durable session. This includes edits to closed registered notes.
Corbomite must adapt Vault process/modify and rename link rewriting accordingly.

Markdown export remains independently fallible after a successful action commit.
Report export failure and retain dirty/export-pending state without rolling back
accepted editing history. Markoff exposes a stable export revision; Corbomite
only marks it clean if that revision was written and no later edits superseded it.
Save must neither compact history nor clear undo.

## Delivery split and acceptance

1. Agree this provider contract, formats/readers and replacement-edit semantics.
   Markoff can develop with a deterministic reference provider while Corbomite
   implements the local document service and storage.
2. Complete one durable Corbomite editing path: import, edit, commit, export,
   close/reopen, rename, duplicate and two panes sharing a lease. Include plugin
   writes and external reload resolution, plus fault injection at reservation,
   append, undo metadata and initial-import boundaries.
3. Verify same namespace/distinct actors with two independent private stores,
   seed replay and duplicate/reordered remote delivery against the same immutable
   candidate. This requires a test delivery harness, not completed network UI.
4. Enable production writers only after Markoff's actual engine/routing/undo and
   archive-reader gates and the host crash/recovery gates pass. Pin the validated
   Markoff revision and dependency candidate together. Keep pruning and epoch
   transitions disabled initially.

We accept host persistence responsibility. We ask Markoff to retain responsibility
for the complete editing transaction and recovery algorithm, and to provide the
provider API and migration surfaces before Corbomite commits to concrete C++
signatures. No full synchronization service is required to close this first design
deliverable, and no successful production acceptance is claimed by this response.
