# Handoff → collabtext devs: two release-only correctness bugs in `IdList`, plus a GC-safety issue

**From:** Corbomite (downstream consumer via `markoff-core`)
**Date:** 2026-10-02
**Corbomite HEAD:** `57e79fad`
**collabtext pin:** inside Markoff pin `5d8850369`
**Severity (our view):** **P0** for §1 — in a release build this is a heap
out-of-bounds *write*, and a self-declaring document divergence. §2 is **P0** but
needs a design conversation. §3 is a performance ask. None of this blocks your
roadmap; all of it is reachable from ordinary user actions.

We are collabtext's largest consumer by some margin (it sits under every editor
leaf Corbomite ships), so we are being greedy with this list. We have included
everything we found, including the parts that are only probably-urgent, so you
can triage.

---

## 0. How we verified this

We did not take these from a code skim. The two §1 findings were read directly
at the line numbers quoted, and §1's second half was additionally confirmed by
checking that `entries` is a `std::vector` (`IdList.h:187`) and that the compiler
configuration used for release builds defines `NDEBUG`. If any of this is wrong,
we would much rather hear about it than have you spend an hour on it — we have
been wrong about "dead code" and about a keyboard shortcut in the same fortnight.

---

## 1. `assert()` is the only guard on three anchor-lookup paths

**In a release build this is an out-of-bounds heap read followed by an
out-of-bounds heap write.** Not a crash — silent corruption of whatever the
allocator placed after the vector.

### 1a. `remove_at` — OOB read then write

`src/crdt/IdList.cpp`

```cpp
:88    size_t idx = entries.size();                 // "not found" sentinel
:89-95 for (size_t i = 0; i < entries.size(); ++i) { /* find matching origin */ }
:96    assert(idx < entries.size() && "remove_at: target anchor not found in entries");
:99    Lamport target_origin = entries[idx].origin; // <-- OOB READ (idx == size())
...
:113   entries[idx].deletions.push_back(del_id);    // <-- OOB WRITE
```

`entries` is `std::vector<IdListEntry>` (`IdList.h:187`). Under `NDEBUG` the
`assert` at `:96` is gone, so `idx == entries.size()` indexes one-past-the-end.
The value read at `:99` is then fed into a `Lamport`, so the corruption is not
even immediately self-evident.

### 1b. `insert_after` — silent block misordering

```cpp
:40    assert(found && "insert_after: anchor not found in entries");
:43    Locator predecessor;   // default-constructed on a miss
:48    Locator new_loc = Locator::between(predecessor, successor);
```

`Locator::between` guards its own precondition with `assert(lo <= hi)`
(`Locator.cpp:44`), also compiled out — so on a miss it produces an arbitrary
locator and the entry lands at an **arbitrary position** in the sorted entries.
This one is worse than a crash: it silently reorders the user's document.

### 1c. `apply_concrete(RemoveOp)` — the version vector records a delete that never happened

This is the one we would most like fixed.

```cpp
:469   bool found = false;
:470-476 for (auto &e : entries) { /* find target_origin */ if (match) { e.deletions.push_back(...); found = true; break; } }
:478   assert(found && "apply_concrete(remove): target_origin not in tree — causal gate should have prevented this");
:480   m_clock.observe(op.timestamp);
:481   m_version.observe(op.timestamp);
:482   m_version.join(op.version);
:483   set_entries(std::move(entries));
:484   return true;
```

On a miss, in release: the deletion is silently discarded, but its Lamport
timestamp is **written into `m_version` anyway**, the op is reported as applied,
and the deferred queue (`:430-448`) drops it so it is never retried.

The consequence is worse than a lost delete. `version()` now asserts "I have seen
delete D" for a delete that did not happen. **Any future GC driven off
`version()` may reclaim the insert that D targeted** — which makes the divergence
permanent and unrecoverable rather than a transient.

We recognise the in-tree comment's reasoning ("causal gate should have prevented
this"), and we think it holds in the happy path. But the causal gate is precisely
what fails on op replay, reordering, or a duplicate arriving after a reconnect —
i.e. exactly the cases the deferred queue exists to handle.

### Why this is reachable for us

Markoff stores block order in `IdList` and feeds it anchors derived from
persisted document state. A stale anchor — a truncated autosave, a merge of two
files, a block whose origin never made it into the local list — is an ordinary
user action, not an exotic one.

### What we would like

1. Make the miss a first-class outcome rather than an assertion failure:
   ```cpp
   std::optional<IdListOperation> remove_at(const Anchor &target);
   ```
   returning `std::nullopt` plus an **unconditional** `qCWarning`, checked
   *before any mutation*. Same shape for `insert_after`.
2. In `apply_concrete`, reorder so the guard precedes every mutation:
   ```cpp
   if (!found) { qCWarning(...) << "remove op target not present"; return false; }
   m_clock.observe(...); m_version.observe(...); ...
   ```
   Returning `false` also lets the existing deferred queue retry it, which we
   think is the behaviour you actually want.
3. Demote the `assert`s to post-guard invariant assertions.

### Tests we would like to see

- `remove_at` with an anchor not present — asserts `std::nullopt` and no
  mutation.
- A remove op delivered **before** its insert.
- Both of the above run in a **release** build in CI. All three current crash
  modes are invisible under `NDEBUG`, which is presumably why they survived —
  our suite would not have caught them either.

---

## 2. `Global::meet` is not a meet — it produces watermarks that are unsafe for GC

`src/crdt/Clock.cpp:164-178`

```cpp
void Global::meet(const Global &other) {
    ...
    uint32_t other_val = other.get(rid);
    if (other_val > 0 && other_val < self_val) {   // :174
        a[i] = pack(rid, other_val);
    }
```

A replica id **absent** from `other` leaves `self`'s value untouched, so the
result keeps the higher value. That is not the meet (pointwise minimum) — it is
"min over ids both sides happen to know about, keep everything else".

The shipped caller is `NetworkSim::compact_all()` (`NetworkSim.cpp:168-177`),
which builds a `watermark` from replica 0 and `meet`s the rest in.

**Failure shape:** with three or more replicas where r0 has seen r2 but r1 never
has, `meet` leaves r2's high value in the watermark. `Buffer::compact`
(`Buffer.cpp:192`) then GCs r2's tombstones that r1 never received. r1 never
learns about those deletes → permanent text divergence, which is the exact thing
`compact()` exists to make impossible.

**Why no test catches it — and this is the part we think is most useful to you:**
all five `compact_*` cases in `tests/tst_gc.cpp` (`:430`, `:459`, `:480`, `:500`,
`:537`) use exactly **two** replicas. With two replicas both version vectors
carry both ids, so `meet` degenerates to the correct pointwise min and the bug
is invisible. It lives *only* in the ≥3-replica case, which nothing exercises.

**Our suggestion:** two passes. First collect the union of replica ids across all
replicas; drop from the accumulator **every** id not present in *all* of them, so
`observed()` is false and nothing is GC'd for it; only then `meet`. And please add
the missing test: three replicas, r2 deletes, r0 syncs, r1 never does,
`compact_all()`, then assert r1 converges.

We recognise this is a semantics change rather than a typo fix, which is why we
have put it in its own section.

---

## 3. Performance: `set_entries` rebuilds the entire `SumTree` on every single edit

`src/crdt/IdList.cpp:394-400`

```cpp
void IdList::set_entries(std::vector<IdListEntry> &&entries) {
    for (auto &e : entries) e.visible = e.compute_visible(m_undo_map);   // O(n) lookups
    IdListTree tree;
    for (auto &e : entries) tree.push_item(std::move(e));               // O(n log n) rebuild
    m_entry_tree = std::move(tree);
}
```

Called unconditionally from `insert_after:65`, `remove_at:117`,
`apply_concrete:464/484/497`, `undo`/`redo`, and `compact`. Combined with the
linear anchor scan (`:29-38`, `:88-95`) and a sorted-vector insert, **every block
insert or remove is O(n log n)**.

For us this fires on every Enter that splits a paragraph, and on canvas-file load
where blocks arrive one at a time. Each pushed `IdListEntry` carries summaries
with `Global` version vectors (three per summary, per `Clock.h:50-53`).

**Suggestions:** incremental `SumTree::update_item`/remove along the affected
path rather than a full rebuild; an `origin → position` index to kill the linear
scan; and a batch-load entry point for file-open.

---

## 4. Build hygiene

1. **`enable_testing()` and all 50 test targets are unconditional**
   (`CMakeLists.txt:93`) — no `option()`, no `PROJECT_IS_TOP_LEVEL` gate. Any
   project that vendors collabtext builds 50 test executables whether or not it
   wants them. Both Graffodil and Markoff gate theirs; we suspect this is an
   oversight. An `option(COLLABTEXT_BUILD_TESTS ... ${PROJECT_IS_TOP_LEVEL})`
   would fit the family.

   *(This one cuts slightly in your favour — those 50 tests do run in Corbomite's
   suite and give us genuine coverage. But we'd rather they were opt-out-able than
   forced on every consumer.)*

2. **Public `Qt6::Test` and public internal include path**
   (`CMakeLists.txt:76-80`). `target_link_libraries(collabtext PUBLIC … Qt6::Test)`
   and `target_include_directories(collabtext PUBLIC …/src)` mean every downstream
   target inherits a test-framework dependency *and* collabtext's internal
   `crdt/` headers. This directly contradicts collabtext's own `CLAUDE.md`, which
   says `src/crdt/` is "internal-only after the extraction". Making `Qt6::Test`
   PRIVATE and dropping `src/` from the PUBLIC include dirs would fix it — the
   `add_crdt_test` helper at `:98` already opts back in for the internal tests.

3. **`install(TARGETS collabtext EXPORT CorbomiteTargets)`** (`:85`). collabtext
   is a submodule of Markoff, which is a submodule of Corbomite, yet it names
   *Corbomite's* private export set, with no version and no alias. That leaks a
   parent's build layout into a third-party tree. Each library defining its own
   export set (`CollabTextTargets`) and letting the parent aggregate would be
   cleaner. We will do the same on our side.

---

## 5. Smaller items

| Item | Location | Note |
|---|---|---|
| `Global::observe()` silently drops `Lamport{rid, 0}` | `Clock.cpp:116` | The sentinel rule is enforced only by a comment in two constructors (`Buffer.cpp`, `IdList.cpp:8`). A `Q_ASSERT(ts.value != 0)` plus a note on `Lamport` would make it structural. |
| `Global::ensure_capacity` has a dead-but-live landmine branch | `Clock.cpp:77` | If `m_capacity` were ever 0 with `need <= 4`, `on_heap()` stays false and the just-`malloc`'d block leaks. Unreachable today; `qMax(new_cap, SBO_CAP + 1)` before assigning would make it unreachable forever. |
| `observed()` returns `get(rid) >= 0` | `Clock.cpp:136-137` | Always true, so it disagrees with what `observe()` actually recorded. May be intentional, but it reads like a bug. |
| `apply_concrete` joins the sender's whole version vector on every op | `IdList.cpp:463,483,496` | `version()` then reports what the *sender* had seen, not what this replica applied. Worth a doc comment either way; also the input that makes §2 reachable. |
| `Buffer::split_fragment_at` split-origin collision invariant is untested | `Buffer.cpp:494` | We are **not** claiming a collision — only that the standard technique's invariant is neither documented nor pinned by a test. |
| `UndoLog::lastEntry()` is public and UB on empty | `UndoLog.h:70` | Returns `m_entries.back()` with no guard, and hands out a reference into a vector that `undo()`/`compact()` mutate. Currently only tests call it, but it is reachable from any consumer through the public `d2UndoLog()`. |
| `IdList::set_entries` is called with a `std::vector` moved in and out on every edit | `IdList.cpp:394` | Beyond the rebuild cost, this is a per-edit allocation churn. |

---

## 6. Suggested triage order

1. **§1** — highest severity, smallest diff, three file:line cites sharing one
   fix shape, and one regression test each.
2. **§2** — needs its own design conversation, but the missing 3-replica test is
   cheap and will pin whatever you decide.
3. **§4** — build hygiene, no design questions, and it makes the library easier
   for us to consume correctly.
4. **§3**, then §5.

If you want to push back on any of it, we would rather argue about it here than
carry a workaround. §1 in particular we are happy to be shown wrong about — but
we could not construct a release build where those three paths are safe, and we
would rather that were our mistake than yours.