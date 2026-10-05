# Cluster P — STATIC → SHARED internal library refactor

**Opened:** 2026-08-20 (stub). **Expanded to full plan:** 2026-08-20.
**Track:** strategic cluster. **Priority:** dispatch next, ahead of Cluster O
Phase O4 and Cluster L5 soak resumption — user-directed 2026-08-20.

**Research report:**
[`docs/audit-2026-08-20-shared-libraries-refactor.md`](../../audit-2026-08-20-shared-libraries-refactor.md).
Read it for the disk-usage evidence, library inventory, feasibility findings
and tradeoffs — all of which verified correct. **But read §1 below first:** a
verification pass during plan expansion found two of the report's central
claims wrong and one blocking structural problem it missed. Where §1 and the
report disagree, **§1 is normative.**

---

## §1 Corrections to the audit (normative)

All three findings below were established empirically against the current
`build-dev/` tree on 2026-08-20 (`nm`, `nm -D`, `readelf`, `ldd`), not by
reading CMake.

### C1 — The report's explanation of *why the bug hasn't bitten* is wrong

Report §3 says plugins are safe because they "only `qobject_cast` to their
own plugin-local view classes." That is true but not the reason.

The real mechanism is **ELF symbol interposition**. `libs/vault` is `SHARED`
and PUBLIC-links `Corbomite::Core` + `Corbomite::Storage` statically, so
`libvault.so` re-exports **37 `Corbomite::*` `staticMetaObject` symbols**
(plus ~20 `Markoff::*` ones) into the process's global scope. Verified:

```
$ nm -D --defined-only libvault.so | grep staticMetaObject | grep -c Corbomite
37
$ nm Corbomite | grep _ZN9Corbomite4View16staticMetaObjectE
                 U _ZN9Corbomite4View16staticMetaObjectE      # exe IMPORTS it
```

The executable does not define these at all — it imports them from
`libvault.so`. Plugins carry private duplicates but resolve to the global
scope first, so they land on `libvault.so`'s copy too. **`libvault.so` is
accidentally acting as the shared core library for everything vault's link
graph happens to pull in.**

Why this matters for the plan: the safety is an accident of link topology,
not of plugin behaviour, and it **does not generalise** — see C2.

### C2 — The divergence is not latent everywhere; it is already live

The accidental unification in C1 covers only object files `libvault.so`
transitively pulls in. `Corbomite::Models` is not in vault's link graph, and
host and plugin genuinely hold **two different `QMetaObject`s today**:

```
$ nm Corbomite | grep NotesTreeModel16staticMetaObject
0000000000f76060 D _ZN9Corbomite14NotesTreeModel16staticMetaObjectE   # exe, private
$ nm -D Corbomite | grep -c NotesTreeModel16staticMetaObject
0                                                                      # NOT exported
$ nm -D --defined-only .../corbomite-file-explorer.so | grep NotesTreeModel16staticMetaObject
0000000000245a40 D _ZN9Corbomite14NotesTreeModel16staticMetaObjectE   # plugin's own copy
```

The executable exports **no strong symbols at all** — its 5204 `.dynsym`
entries are 5077 `W` (vague/inline), 103 `u`, 24 `V`. Nothing the exe
defines can ever be interposed onto by a plugin.

So the correct framing is **live divergence, not yet observed** — for
`Models`, `Search`, `Bases`, `canvas`, and `forcegraph`. It produces no
visible failure only because nothing currently casts one of those types
across the boundary. `Core` and `Storage` are the ones that are genuinely
safe, and only by the C1 accident.

**Consequence for the acceptance gate:** the P4 correctness test must target
a **Models/Search/Bases/canvas/forcegraph** type. A test written against a
`Core` type would pass today (pre-refactor) and prove nothing.

### C3 — BLOCKER the report missed: `Corbomite::Core` ↔ `Corbomite::Storage` is a dependency cycle

```
libs/storage/CMakeLists.txt   target_link_libraries(corbomite-storage PUBLIC Corbomite::Core)   # when not top-level
libs/core/CMakeLists.txt      target_link_libraries(corbomite-core PRIVATE $<BUILD_INTERFACE:Corbomite::Storage>)
```

**CMake tolerates a dependency cycle only when every target in the strongly
connected component is `STATIC`.** Introducing a `SHARED` target into that
SCC is a configure-time error ("The inter-target dependency graph contains
the following strongly connected component"). Neither `corbomite-core` nor
`corbomite-storage` can flip until the cycle is broken — and `core` is the
single largest Corbomite `.a` after `bases`, and a transitive dep of almost
everything else.

The report's §7 "cheapest this will ever be to make" framing understates
this. It is not a one-line `add_library` edit per library.

**Mitigating finding:** the cycle is *thin* in the core→storage direction
and can be cut cleanly. Only two files in `libs/core` reach into storage:

| File | Storage headers used | Nature |
|---|---|---|
| `libs/core/src/TextFileView.cpp` | `storage/DataAdapter.h` | **Pure-abstract interface, header-only** — no `DataAdapter.cpp` exists, so this contributes *zero* link symbols |
| `libs/core/src/MarkoffAdapters.cpp` | `CachedMetadata.h`, `LinkResolver.h`, `MetadataCache.h`, `MetadataParser.h` | Real symbol dependency — the only one |

`MarkoffAdapters` (public header
`libs/core/include/corbomite/markoff_adapters/Adapters.h`) has exactly **one
consumer in the whole tree**: `src/app/MainWindow.cpp:51`. The
storage→core direction is a real dependency (`Events.h`/`Events.cpp` in
`MetadataCache.h`) and stays. See Phase P2 for the cut.

### C4 — Scope correction: the biggest disk win is Corbomite-only and risk-free

Report §5 concludes the full win requires the cross-repo change. True for
the *total*, but it obscures the ordering. `CorbomiteApp` is a 47 MB `.a`
that **no plugin links** — it has no plugin-boundary correctness dimension
at all — and it is what makes the 33 app-level test binaries 131–147 MB
each. 43 link sites reference it (tests/editor 21, tests/app 15, dialogs 6,
e2e 4, graph 1, storage 1).

Flipping `CorbomiteApp` alone is one line, is independent of C3's cycle and
of the cross-repo question, and captures the largest single share of the
disk win. **It should be Phase 1, not bundled with the risky work.**

### Context: disk pressure is live

`/home` is at **95% — 13 GB free** on a 222 GB volume, with `build-dev/` at
11 GB. `ccache`/`sccache` confirmed absent (report §2 correct).

---

## §2 Doctrine (normative for this cluster)

- **D1 — Incremental, green at every commit.** No big-bang "flip everything
  and pray" commit. Every phase ends with the full offscreen suite green and
  a launchable app. The report deliberately left this open; this plan closes
  it as *incremental, leaf-first*.
- **D2 — No export macros.** Confirmed: `-fvisibility=hidden` is set nowhere
  and `libs/vault` shipped `SHARED` with zero header annotation. Do **not**
  introduce `*_EXPORT` macros as part of this cluster. If Windows support is
  ever wanted that decision gets revisited on its own merits.
- **D3 — No ABI-stability commitment.** Host and all plugins rebuild
  together from one tree. Do not introduce pimpl, versioned SONAMEs beyond
  CMake's defaults, or a pure-virtual factory boundary. The KDE/Kate
  convention — "recompile your plugin against the Corbomite version you
  target" — is the contract. Revisit only when a real third-party plugin
  author asks.
- **D4 — Correctness gate must target a *diverged* library.** Per C2. A test
  against `Core` or `Storage` is worthless as a gate.
- **D5 — Cross-repo work is Markoff-first.** Land changes in the Markoff /
  Graffodil repos under their own CLAUDE.md discipline, then re-pin the
  submodule. Never edit the submodule checkout in place from a Corbomite
  session.
- **D6 — `libs/mmdr` is out of scope.** A prebuilt Rust `staticlib`
  (`crate-type`), not a CMake target; converting means a Cargo `cdylib`
  rebuild in a different toolchain. Small contributor. Punch-list it
  separately; do not block this cluster on it.
- **D7 — Layering fixes are allowed, feature changes are not.** P2 moves
  files between libraries to break a cycle. That is in scope. Changing what
  any of those types *do* is not.

---

## §3 Scope decision

**Recommended and assumed: Phases P0–P5 (Corbomite-only) as the committed
cluster; P6 (cross-repo) planned but separately gated on user go-ahead
after P5's measurements land.**

Rationale: P1 alone captures the largest single disk contributor with zero
plugin-boundary risk; P2–P3 close the real correctness gap (C2) for every
Corbomite library a plugin can link; P6's markoff-family/graffodil work is
where the remaining disk win lives but it is the only part that touches
foreign repos, needs their build/test discipline, and carries the PIC risk
noted in P6.T1. Splitting the gate there means the correctness fix is not
held hostage to cross-repo sequencing.

This differs from the report's §7 recommendation (full cross-repo up front)
because C4 shows the ordering assumption behind it was wrong. **Flag to the
user at dispatch; proceed on this assumption if unanswered.**

---

## §4 Phases

### Phase P0 — Baseline, corrections, companion fix

- [x] **P0.T1** Append §1 of this plan as a corrections addendum to
      `docs/audit-2026-08-20-shared-libraries-refactor.md` (do not edit its
      body — add a dated `## Corrections (2026-08-20, plan expansion)`
      section, matching the obsidian-audit addenda convention).
- [x] **P0.T2** Record the baseline into this file's §5 table, from a clean
      full `dev`-preset build: `du -sh build-dev build-dev/bin build-dev/lib`,
      the 18 largest binaries, wall-clock of the clean build, and wall-clock
      of an incremental rebuild after touching
      `libs/core/include/corbomite/core/View.h` (the relink-storm case).
- [x] **P0.T3** Capture the symbol-duplication baseline: for each of
      `Corbomite::NotesTreeModel`, one `Corbomite::Search` type, one
      `Corbomite::Bases` type, record which modules define
      `staticMetaObject`. This is the "before" side of P4's gate.
- [x] **P0.T4** *(companion, independent)* Install `ccache` and wire it via
      `CMAKE_CXX_COMPILER_LAUNCHER` in `CMakePresets.json`. Complementary to
      this cluster, not a substitute. If it turns out to interact badly with
      the preset layout, punch-list it and move on — do not let it block P1.

**P0.T3 findings (2026-08-20, against pre-refactor `build-dev/`):**

| Type | Library | Defined in exe | Defined in plugin `.so`s |
|---|---|---|---|
| `Corbomite::NotesTreeModel` | Models | yes (private, `D`) | `corbomite-search.so`, `corbomite-file-explorer.so` — **diverged today** |
| `Corbomite::SearchResultsModel` | Search | yes (private, `D`) | `corbomite-search.so`, `corbomite-file-explorer.so` — **diverged today** |
| `Corbomite::Bases::BasesView` | Bases | yes (private, `D`) | none of the 9 built plugins define it — no plugin currently links `Bases` at all, so this type has no live divergence to observe (consistent with C2: "not yet triggered" is still literally true for Bases specifically, unlike Models/Search) |

Confirms C2 exactly for Models/Search — these are real, already-existing
`nm`-visible divergences, not a hypothetical. Per D4, **P4.T1's
`tst_plugin_type_identity` should use `NotesTreeModel` or
`SearchResultsModel`, not a `Bases` type** — Bases has no current plugin
consumer to prove the fix against.

**P0.T4 note:** `ccache` install needed interactive `sudo` (unavailable in
the assistant's session); user installed it directly. Wired into all three
presets' (`dev`/`release`/`appimage`) `cacheVariables` via
`CMAKE_C_COMPILER_LAUNCHER`/`CMAKE_CXX_COMPILER_LAUNCHER`, added *after*
P0.T2's baseline was captured so it wouldn't contaminate those numbers.

**Gate:** baseline numbers recorded in §5. No code change. ✅ Phase P0 complete.

### Phase P1 — `CorbomiteApp` → SHARED

The largest single win, zero plugin-boundary risk. One-line change plus
fallout.

- [x] **P1.T1** `src/CMakeLists.txt:11` — `add_library(CorbomiteApp STATIC` →
      `SHARED`.
- [x] **P1.T2** **Risk: the KXMLGUI resource.** `CMakeLists.txt:142` does
      `qt_add_resources(CorbomiteApp "xmlgui" ...)` embedding
      `corbomite-devui.rc`. Static-library Qt resources need explicit
      initialisation; shared-library resources self-initialise at load. This
      should get *easier*, but it is the one thing that can silently
      half-work. Verify the `.rc` actually resolves at runtime — not by
      inspection, by launching.
- [x] **P1.T3** Confirm `install(TARGETS Corbomite CorbomiteApp ...)`
      (`CMakeLists.txt:168`) already carries `LIBRARY DESTINATION` — it does;
      confirm the installed layout still runs (`sudo cmake --install
      build-release`, launch from `/usr/local/bin`). RPATH for an installed
      `CorbomiteApp.so` in `${CMAKE_INSTALL_LIBDIR}` is the thing to watch.
- [x] **P1.T4** Full offscreen suite green.
- [x] **P1.T5** Re-measure `du -sh build-dev`; record delta in §5.

**Named test:** `tst_xmlgui_resource_present` — asserts the compiled-in
`:/kxmlgui5/<component>/<component>ui.rc` resource is readable at runtime.
Guards P1.T2's failure mode, which the existing suite would not catch (the
menus would just be empty, which is exactly the symptom the Cluster O3
stale-cache incident already trained us to misdiagnose).

**Gate:** suite green **and** a live launch with menus/toolbars intact.
Per project memory (`feedback_verify_ui_fixes_live`), an offscreen-green
run is not sufficient evidence for anything chrome-related.

**P1 resolution (2026-08-20):** landed as expected — no surprises. New test
`tests/app/tst_xmlgui_resource_present.cpp` asserts the compiled-in
`.rc` resource is readable at runtime (321 → 322 offscreen tests, all
green). `build-dev/` dropped **11 GB → 6.5 GB (-41%)** in one commit; every
former 131-147 MB test binary is now 55-56 MB, since all 43 former link
sites share one `libCorbomiteApp.so` instead of each embedding a private
47 MB copy — full before/after in §5. Live-verified twice: the dev build
(user confirmed menus/toolbars render and update correctly across tabs and
document types) and the installed release build (`sudo cmake --install
build-release`, launched from `/usr/local/bin`, user confirmed working).
RPATH needed no new configuration — `readelf -d /usr/local/bin/Corbomite`
already carries `RUNPATH: [/usr/local/lib]` (inherited from the same
KDECMakeSettings/ECM machinery that already made `libvault.so`'s install
work), confirming the "first RPATH this tree has ever required" concern
flagged for P5 does not apply here.

### Phase P2 — Break the `Core` ↔ `Storage` cycle

Prerequisite for P3. **No linkage type changes in this phase** — it lands
and ships as a pure layering fix with everything still `STATIC`, so a
regression here is isolated from a regression in P3.

- [x] **P2.T1** Move `libs/core/include/corbomite/markoff_adapters/Adapters.h`
      and `libs/core/src/MarkoffAdapters.cpp` → `libs/storage`
      (`include/corbomite/storage/markoff_adapters/Adapters.h`,
      `src/MarkoffAdapters.cpp`). Storage already links `Markoff::Parser` and
      already owns `MetadataCache`/`MetadataParser`/`LinkResolver`, so this
      is the natural home. Update the one consumer,
      `src/app/MainWindow.cpp:51`.
- [x] **P2.T2** Resolve `TextFileView.cpp`'s `storage/DataAdapter.h` include.
      `DataAdapter` is a pure-abstract, header-only interface that **core
      consumes and storage implements** (`FileSystemAdapter`) — i.e. the
      dependency is inverted today. Move `DataAdapter.h` to
      `libs/core/include/corbomite/core/DataAdapter.h` and have storage
      include it from there. Update all includers.
      *(Fallback if that fans out further than expected: keep the header in
      storage and give core storage's include dirs without the link
      dependency. Uglier; prefer the move.)*
- [x] **P2.T3** Delete `PRIVATE $<BUILD_INTERFACE:Corbomite::Storage>` from
      `libs/core/CMakeLists.txt`'s link block. Core must now build with no
      storage dependency at all.
- [x] **P2.T4** Sweep for any other core→storage edge this analysis missed:
      `grep -rn "corbomite/storage/" libs/core/` must return nothing.
- [x] **P2.T5** Full offscreen suite green; commit as a standalone layering
      change.

**P2 resolution (2026-08-20):** landed with one discovery not anticipated
by the plan text: `MarkoffAdapters.{h,cpp}` (the file C3 identified as
`core`'s one "real symbol dependency" on storage) turned out to be
**entirely `#if 0`-disabled** end to end — its own storage `#include`
lines, every class body, and its one call site in `MainWindow.cpp` are all
inside the disabled block (a TODO says it's parked pending
`Markoff::Vault::*` restoration on the Markoff side; see the port banner in
`CLAUDE.md`). So today it compiles to an empty translation unit and
contributes zero actual symbols — C3's "real dependency" framing describes
the *textual* include, not a currently-live compiled one. This made the
move lower-risk than planned (no behavioural surface to preserve, purely
relocating dead-but-parked code) but the CMake-level cycle it created was
real regardless — target_link_libraries edges are structural, not
conditional on `#if 0` content — so P2 was still necessary and the fix is
unchanged. `TextFileView.cpp`'s `DataAdapter.h` move went exactly as
planned: pure-abstract header-only interface, ~11 includers (`libs/core`,
`libs/storage`, `libs/vault`, and 3 test files) all repointed to
`corbomite/core/DataAdapter.h` with no link-graph changes needed since
every includer already linked `Corbomite::Core` transitively. A leftover
manual `target_include_directories(corbomite-core PRIVATE
.../storage/include)` workaround (added at some point specifically to make
the old `DataAdapter.h` include resolve without a real link dependency)
was also dead once the header moved and was removed alongside the
`Corbomite::Storage` link line. 323/323 offscreen (322 + the new
`tst_no_library_cycles`), **demonstrated red pre-fix** (ran the test
against the still-cyclic tree before touching any code — failed exactly as
designed, citing the reappeared edge) **then green post-fix**.

**Named test:** `tst_no_library_cycles` — implemented as
`cmake/CheckNoLibraryCycles.cmake`, run via `add_test(... COMMAND
${CMAKE_COMMAND} -DBINARY_DIR=... -P cmake/CheckNoLibraryCycles.cmake)`
(registered at the top-level `CMakeLists.txt`, not tied to any one
library's test directory since it inspects the whole build's dependency
graph). It shells out to `cmake --graphviz=<file> .` against the already-
configured build tree and greps the `.dot` output for a `corbomite-core ->
corbomite-storage` edge comment line, failing loudly with a pointer back
to this section if found; it also asserts the retained `corbomite-storage
-> corbomite-core` edge is still present, as a sanity check that the
graphviz mechanism itself is actually reading the real graph rather than
silently matching nothing. Chose the graphviz approach over grepping
generated link lines (the plan's other suggested form) because `STATIC`
targets don't produce a literal link-line artifact naming their private
dependencies — the CMake dependency-graph export was the more direct
signal. Do not skip this — nothing else prevents a future session from
reintroducing the include and quietly restoring the cycle, and the failure
only surfaces much later as a confusing configure
error.

**Gate:** suite green; `grep` in P2.T4 clean. ✅ Phase P2 complete.

### Phase P3 — Corbomite libraries → SHARED

Leaf-first, one library per commit, suite green after each. Order chosen so
every step's dependencies are already shared:

| Step | Library | Deps | Notes |
|---|---|---|---|
| P3.T1 ✅ | `corbomite-search` | *(none internal)* | True leaf — safest first flip |
| P3.T2 ✅ | `forcegraph` | *(none internal)* | True leaf |
| P3.T3 ✅ | `corbomite-core` | *(none, after P2)* | Now a leaf. Largest blast radius |
| P3.T4 ✅ | `corbomite-storage` | Core | |
| P3.T5 ✅ | `corbomite-models` | Core, Storage, Vault | |
| P3.T6 ✅ | `corbomite-bases` | Core, Storage, Vault | Largest `.a` (51 MB) |
| P3.T7 ✅ | `canvas` | Core, Search, Graffodil::Core | Graffodil stays static (P6) |
| P3.T8 ✅ | `jkqtmathtext` | *(vendored, none)* | 41 MB. Vendored LGPL — flipping to shared is also the *licence-friendlier* form. Low risk |

Per library:

- [x] Change `add_library(<name> STATIC` → `SHARED`.
- [x] Confirm an `install(TARGETS ... LIBRARY DESTINATION ${KDE_INSTALL_LIBDIR})`
      rule exists (core/storage/models/bases/vault already have one;
      **`canvas`, `forcegraph`, `search`, `jkqtmathtext` must be checked** —
      several have no install block at all today, which is fine while static
      and is not fine once the app has a runtime `DT_NEEDED` on them).
- [x] Rebuild, full offscreen suite green.
- [x] Note any symbol that moved from link-time to load-time failure —
      per report §6 this is the expected new failure mode, and per the vault
      precedent the suite catches it immediately.

**Watch item:** `libs/core`'s link block wraps every submodule dep in
`$<BUILD_INTERFACE:...>` (`jkqtmathtext`, `mmdr`, `markoff-parser`,
`markoff_core`, `markoff_styled`) with a comment saying the installed
package does not ship them. Once `corbomite-core` is a real `.so`, it
acquires genuine runtime dependencies on those still-static libraries —
their code gets baked into `libcorbomite-core.so`. That is correct and
works, but it means the `BUILD_INTERFACE` comment's claim ("plugins must
resolve these symbols themselves") becomes *less* true, not more. Re-read
and correct that comment rather than leaving it stale.

**Gate:** suite green after each individual flip. Do not batch. ✅ Phase P3
complete.

**P3 resolution (2026-08-20):** all 8 flips landed exactly per the table's
ordering, one commit each, suite green after every single one (no
batching) — no library needed the link-time→load-time-failure fallback
path the plan flagged as the expected new failure mode. `search`,
`forcegraph`, `canvas`, and `jkqtmathtext` all needed a fresh
`install(TARGETS ...)` block added (none had one while `STATIC`, per the
plan's warning); `search` got the full `EXPORT CorbomiteTargets` +
`EXPORT_NAME` treatment matching `core`/`storage`/`models`/`bases`/`vault`
since it has a `Corbomite::Search` alias, while `forcegraph`/`canvas`/
`jkqtmathtext` got a plain install rule (no `Corbomite::` alias, not part
of that export set). The `libs/core` `BUILD_INTERFACE` comment was
corrected in the same commit as P3.T3's flip, as the watch item asked.
One flake (`tst_quadtree`, on P3.T6's run — passed standalone and on a
clean full re-run, unrelated to `bases`) was the only test-suite noise
across all 8 flips. **Disk result, full before/after in §5:** `build-dev/`
went from the P1 baseline of 6.5 GB down to **3.6 GB** — **11 GB → 3.6 GB
(-67%) end to end** across P1+P3 combined. `libcorbomite-core.so` (62 MB)
and `libCorbomiteApp.so` (47 MB) are now the two largest binaries in the
tree, ahead of every test executable — the inverse of the pre-refactor
shape, where the largest items were 33 near-identical test binaries each
embedding a private copy of everything.

### Phase P4 — Plugin-boundary correctness gate

This is the phase that justifies the cluster. Per D4/C2 it must target a
library that is *actually* diverged today.

- [x] **P4.T1** Write `tst_plugin_type_identity` **before** finishing P3 (or
      on a branch stashed from P0) and confirm it **FAILS** against
      pre-P3 `master`. A gate that was never seen red proves nothing.
      Shape: host constructs a `Corbomite::NotesTreeModel` (Models — the
      confirmed-diverged case from C2), hands it to a real `dlopen`-ed
      plugin `.so` through `PluginContext`, plugin `qobject_cast`s it back
      and asserts non-null. Use the file-explorer plugin or a minimal
      purpose-built test plugin.
- [x] **P4.T2** Confirm it passes post-P3.
- [x] **P4.T3** Add `tst_no_duplicate_metaobjects` — a symbol-level guard
      that walks the built `Corbomite` exe, `build-dev/bin/lib*.so` and
      `build-dev/lib/plugins/corbomite/*.so`, and asserts no
      `*staticMetaObject` symbol is *defined* in more than one module. This
      is the structural guard; P4.T1 is the behavioural one. Expect to have
      to allowlist markoff-family/graffodil symbols until P6 lands — encode
      the allowlist explicitly so P6 can delete entries from it as proof of
      progress.
- [x] **P4.T4** Add a note to `cmake/CorbomitePlugin.cmake` documenting that
      plugin-linked Corbomite libraries must be `SHARED` and why (one
      paragraph, pointing at this plan). Future plugin-facing libraries are
      the obvious way to reintroduce the bug.

**P4 resolution (2026-08-20):** all four tasks landed in one commit
(`b8f492d1`). **P4.T1/T2:** built the real scenario rather than a
hypothetical one — `tst_plugin_type_identity` constructs a `Vault`/
`FileManager`/`MetadataCache` in the host test process, drives the actual
production `PluginManager`/`KPluginFactory` dlopen path (not the
`setFactoryOverride` test bypass, which would construct the plugin
in-process and prove nothing about the DSO boundary) to load the real
built `corbomite-file-explorer.so`, calls its `createView()` — which
internally constructs a `Corbomite::NotesTreeModel` inside the plugin's
own compiled code, independent of anything this test added — and
`qobject_cast`s the resulting `QAbstractItemModel*` back to
`NotesTreeModel*` from the host. **Red demonstrated empirically, not
theoretically**: created a scratch `git worktree` at `7f7dd3b5` (the
commit right after P2 closed, last commit before any P3 flip), copied the
test file + its CMake wiring in, ran `git submodule update --init` there,
built the full dependency chain from scratch, and ran it — failed with
exactly the predicted symptom (`qobject_cast<NotesTreeModel*>` → nullptr,
no crash, no error). Confirmed green back on `master`. Worktree discarded
after.

**P4.T3:** the guard's first real run found a genuine live divergence
**outside P3's scope entirely** — not a markoff-family/graffodil case, a
pure Corbomite-only bug P1-P3 never touched because it was never a
`STATIC`-library problem to begin with: `src/sidebar/
PropertyEditorWidget.cpp` and `PropertyRow.cpp` were being compiled once
into `CorbomiteApp`'s `SOURCES` and a *second* time directly into
`corbomite-properties`'s `SOURCES` (`src/plugins/properties/CMakeLists.txt`),
producing two fully independent `staticMetaObject` copies for
`PropertyRow`, `PropertyEditorWidget`, and the six `PropertyEditor`
subclasses nested inside `PropertyEditorWidget.cpp` — 8 duplicated symbols,
zero markoff-family/graffodil ones (neither is embedded into more than one
module today, so **the allowlist is genuinely empty**, not just
unpopulated). Fixed by linking `corbomite-properties` against
`CorbomiteApp` (`SHARED` since P1) instead of recompiling the same two
files; `INCLUDE_DIRECTORIES` stayed so the plugin's own sources can still
see the headers. **Verified the guard actually catches this class of bug**
(not just vacuously passes): reverted the fix via `git stash`, rebuilt
just the plugin, confirmed the test failed listing all 8 symbols by name
and both defining modules, then restored the fix and confirmed green
again. Chose the `cmake --graphviz`-sibling script-test pattern from
`tst_no_library_cycles` (`cmake -P` + `add_test`) over a compiled C++ test
since the check is fundamentally a build-artifact `nm` sweep, not
something exercising runtime behaviour.

**P4.T4:** the note lives right at `LINK_LIBRARIES` in
`cmake/CorbomitePlugin.cmake` — where a future plugin author would
actually make the mistake — explaining the pointer-identity mechanics of
`QMetaObject::cast()`, pointing at both the behavioural test and the
structural guard, and explicitly calling out the "compiled the same
source twice" variant of the mistake since P4.T3 just caught exactly that.

325/325 offscreen throughout (322 baseline + `tst_plugin_type_identity` +
`tst_no_duplicate_metaobjects` + `tst_no_library_cycles` from P2 = 325).

**Gate:** P4.T1 demonstrated red-then-green. P4.T3 green with a documented
allowlist. ✅ Phase P4 complete.

### Phase P5 — Packaging verification

Report §4 assumed packaging is generic but explicitly flagged it as **not
verified by an actual build**. Close that.

- [x] **P5.T1** AppImage: run `packaging/appimage/build-appimage.sh`.
      `linuxdeploy` must auto-bundle the new `.so`s the way it already
      bundles `libvault.so`, `libryml`, `c4core`, `fmt`, `spdlog`. Launch
      the resulting AppImage and open a vault. Most likely place for a
      missed-bundle failure.
- [x] **P5.T2** Installed release build: `cmake --preset release`,
      `sudo cmake --install build-release`, run from `/usr/local/bin`.
      **The specific risk is plugin RPATH**: plugins install to
      `${KDE_INSTALL_PLUGINDIR}/corbomite` and must find
      `libcorbomite-core.so` etc. in `${KDE_INSTALL_LIBDIR}` at `dlopen`
      time. Today only `libvault.so` is in that position and it works;
      verify it still does with 8 more. If not, an `INSTALL_RPATH` is
      needed — that would be the first RPATH configuration this tree has
      ever required, so record it prominently.
- [x] **P5.T3** Arch `PKGBUILD` build.
- [x] **P5.T4** Ubuntu `.deb` — CI-only (`v*` tag triggered). Either dry-run
      the script locally or explicitly note it as deferred to the next tag.
- [x] **P5.T5** Record final measurements in §5.

**Gate:** AppImage and installed-release both launch and open a vault. ✅
Phase P5 complete.

**P5 resolution (2026-08-20/21):** the risk this phase was written to catch
was real, just not where §4 predicted — the AppImage launched clean on the
first try (linuxdeploy auto-bundled all 9 plugins + the newly-SHARED
Corbomite libraries with zero `qWarning` plugin-load noise), but the Arch
`PKGBUILD` build (P5.T3) surfaced it: `corbomite_add_plugin()` had never set
`INSTALL_RPATH` at all, so every plugin inherited ECM's global
`CMAKE_INSTALL_RPATH` default (`$ORIGIN/../lib`), which is only correct for
a target one level below the prefix root — wrong for a plugin nested under
`lib/plugins/corbomite` (2 levels) or `lib/qt6/plugins/corbomite` (3
levels, the plain-`/usr`-prefix case `PKGBUILD` uses). It had only ever
"worked" because libdir is also a default `ldconfig` search path on a real
system install — the exact accidental-correctness shape (§1 C1) this
cluster exists to eliminate, and the literal reason
`packaging/appimage/build-appimage.sh` already has to `patchelf`-correct
plugin RPATH by hand (a workaround, not a fix, that predates this cluster).
Fixed in `cmake/CorbomitePlugin.cmake`: computes the real relative offset
via `file(RELATIVE_PATH)` between `KDE_INSTALL_FULL_PLUGINDIR/corbomite`
and `KDE_INSTALL_FULL_LIBDIR` and sets `INSTALL_RPATH` explicitly, so the
value is correct regardless of install depth — verified both algebraically
(isolated `file(RELATIVE_PATH)` check for both depths) and empirically
(`ldd` against the real `/usr/local` install: `libcorbomite-core.so`
resolves through `$ORIGIN/../../`, not "not found"). Also fixed two
unrelated `packaging/arch/PKGBUILD` bugs found getting the Arch build to
run at all: the `depends=()` array used openSUSE-style `kf6-*` package
names that don't exist on Arch (real names are unprefixed — `kcoreaddons`,
`ki18n`, etc. — and `syntax-highlighting` rather than
`ksyntaxhighlighting`), and the `git+file://` source used an invalid
`#HEAD` fragment (makepkg's git-source handling only recognizes
`key=value` fragments — `tag=`/`branch=`/`commit=` — so a bare `HEAD`
always errors; fixed by dropping the fragment, which defaults to
`origin/HEAD` anyway). Arch package built successfully
(`corbomite-0.1.0-1-x86_64.pkg.tar.zst`, 8.2 MB compressed / 21.7 MB
installed) with all dependencies resolving and all 9 plugins + 5 exported
Corbomite libraries present at the expected paths. Ubuntu `.deb` (P5.T4)
deferred to the next `v*` tag per plan — Docker unavailable in this
environment for a local dry-run; CI (`.github/workflows/release.yml`)
remains the only path and is unchanged by this phase. Installed release
build (P5.T2) confirmed both mechanically (all 9 plugins present at
`/usr/local/lib/plugins/corbomite/`, `ldd` resolves cleanly) and by live
human use — the user ran `sudo cmake --install build-release` themselves
and had it running from `/usr/local/bin` against a real vault during this
session, satisfying the project's standing live-eyeball gate for
chrome-adjacent changes without a separate offscreen-only claim.

### Phase P6 — Cross-repo: markoff-family + graffodil *(separately gated — see §3)*

Do not start without the user's go-ahead on the §3 scope call.

- [x] **P6.T1** **Markoff repo.** Flipped `markoff_core`, `markoff_canvas`,
      `markoff_source`, `markoff_styled`, `markoff-parser`,
      `ts-markdown-parser`, `collabtext` to `SHARED`. The predicted PIC
      risk was real: `markoff-parser` links vendored rapidyaml (STATIC,
      no PIC standalone) and failed to link until a global
      `CMAKE_POSITION_INDEPENDENT_CODE ON` fallback was added to
      Markoff's own top-level `CMakeLists.txt` (each flipped target also
      got explicit per-target PIC, belt and suspenders). Two more
      standalone-vs-embedded issues surfaced only once these libraries
      joined Corbomite's `CorbomiteTargets` export set: raw (non-
      `BUILD_INTERFACE`-wrapped) `PUBLIC` include dirs on
      markoff_canvas/source/styled, and `corbomite-vault`'s private
      `Markoff::Parser` link needing the same treatment vault's siblings
      already used — both fixed. `libs/jkqtmathtext` (Corbomite-owned,
      privately linked by `markoff_canvas`) needed `EXPORT
      CorbomiteTargets` added back, reversing a P3-era comment that
      assumed it would never need to join.
- [x] **P6.T2** Markoff's suite green (213/213 offscreen), committed
      there, re-pinned from Corbomite. **Caught a real regression along
      the way**: the first re-pin (straight to Markoff's `master` HEAD)
      silently dropped Cluster N's clipboard API, because Corbomite's
      old pin actually sat on Markoff's `feature/rich-clipboard` branch
      — never merged to `master` even after Corbomite's Cluster N closed
      2026-08-20. Corbomite's own incremental rebuilds (327/327, twice)
      missed it; the Arch PKGBUILD's fresh clone caught it on the first
      try. Fixed by merging `feature/rich-clipboard` into Markoff's
      `master` (clean, no conflicts) and re-verifying with genuinely
      clean rebuilds throughout. Full story: `decisions-archive.md`,
      2026-08-21.
- [x] **P6.T3** **Graffodil repo.** All six libraries
      (`graffodil-{core,batch,circular,force,spatial,sugiyama}`) flipped
      SHARED, re-pinned `dd7667de` → `011b342` (one commit ahead — no
      drift risk like Markoff's, master was already the right base).
      Simpler than Markoff as predicted: every target already had
      per-target PIC and `BUILD_INTERFACE`-wrapped include dirs, so no
      vendored-PIC work was needed. `canvas` (the sole in-tree consumer)
      isn't itself in `CorbomiteTargets`, so no export-set cascade
      either — just needed its own `install(TARGETS)` rule. 16/16
      offscreen.
- [x] **P6.T4** Allowlist was already empty (confirmed empty at P0.T3,
      before any markoff-family/graffodil duplication could even be
      observed — those libraries were STATIC and invisible to this scan
      until P6 made them SHARED). Stayed empty after P6 landed: the scan
      now inspects their `.so`s directly for the first time and still
      finds zero duplicated `staticMetaObject` symbols — the strongest
      form of the proof this task asked for. Comment updated to record
      the outcome; no code change.
- [x] **P6.T5** Re-verified with genuinely clean builds (`rm -rf`, not
      `--fresh`/incremental — the P6.T2 regression was invisible to
      incremental rebuilds): AppImage (compile+stage+launch+open-a-vault,
      all 9 plugins + every new SHARED lib mapped, zero stderr; final
      squashfs bundling hit one transient GitHub 504, retried clean),
      Arch PKGBUILD (fresh `git+file://` clone build — the check that
      caught the regression — extract+launch+open-a-vault, same clean
      result; one cosmetic `$srcdir`-in-binary warning on
      `libmarkoff-parser.so`/`libcollabtext.so`/vendored rapidyaml, from
      `__FILE__`/`assert()` string literals surviving strip, not a
      functional defect — punch-listed, not blocking). Installed release
      (`sudo cmake --install`) skipped this pass — no interactive sudo in
      this session; user explicitly deferred it rather than block on it.
      §5 measurements table filled in.

**Gate:** P4.T3 allowlist reduced to (at most) `mmdr` — **met, allowlist
is empty**; full suite green — **met** (Markoff 213/213, Corbomite
327/328 with one confirmed -j10 flake); packaging re-verified — **met**
for AppImage + Arch; installed-release deferred by user request.

---

## §5 Measurements

Filled in by P0.T2, P1.T5, P3, P5.T5, P6.T5. Same flags throughout: `dev`
preset, `Debug`, `CORBOMITE_PORT_BUILD_TESTS=ON`, clean build.

| Metric | Baseline (P0) | After P1 | After P3 | After P6 |
|---|---|---|---|---|
| `build-dev/` total | 11 GB | **6.5 GB** (-41%) | **3.6 GB** (-67% vs. baseline) | **1.8 GB** (-84% vs. baseline) |
| `build-dev/bin/` | 8.9 GB | **4.9 GB** | **2.4 GB** | **701 MB** |
| `build-dev/lib/` | 613 MB | 566 MB | 165 MB | **26 MB** |
| Largest test binary | 147 MB (`tst_completion_controller`) | 56 MB (`tst_canvas_view_contract`, tied with a dozen+ other canvas tests) | 38 MB (`tst_canvas_view_contract`, still the canvas cluster) — but no longer the single largest binary in `bin/` at all | **5.9 MB** (`tst_graphview_plugin`) — markoff-family/graffodil test binaries no longer embed a private copy of the whole D2/CRDT/canvas stack each |
| Clean build wall time | 1475s (~24.6 min), `-j 10`, no ccache | | *(not re-measured — P3 was 8 incremental flips, not a clean rebuild)* | *(not re-measured — P6 needed several from-scratch rebuilds for the clean-rebuild lesson below, none timed as a controlled baseline run)* |
| Incremental relink after `View.h` touch | 161s | | *(not re-measured — same reason; `core` is now a leaf so a `View.h` touch relinks a very different, likely smaller, dependent set post-P3)* | *(not re-measured)* |
| `/home` free | 34 GB (85% full) — improved since the audit's 13 GB/95%; no longer under acute pressure, refactor still worth doing | | | |
| Largest binaries overall (post-P3) | — | — | `libcorbomite-core.so` 62 MB, `libCorbomiteApp.so` 47 MB — both ahead of every remaining test binary | `libcorbomite-core.so` 33 MB, `libcorbomite-bases.so` 27 MB, `libCorbomiteApp.so` 26 MB, `libjkqtmathtext.so` 24 MB, `libmarkoff_core.so` 21 MB — the shape holds: real libraries dominate, no test binary comes close |

**After P6 (2026-08-21):** markoff-family (markoff_core/canvas/source/
styled, markoff-parser, ts-markdown-parser, collabtext) and graffodil
(all 6 libraries) flipped STATIC→SHARED, closing the disk-cost gap the
Corbomite-only phases (P1–P5) couldn't reach — every test binary in
those two families had been embedding a private copy of the whole
D2/CRDT engine or the whole graph-layout stack. `build-dev/` dropped a
further 3.6 GB → 1.8 GB (-50% on top of P3, **-84% end-to-end** from
the 11 GB baseline). Verification caught a real regression, not just
measured a size: the Arch PKGBUILD's fresh `git+file://` clone build
failed to compile (`Markoff::MarkdownView::copy`/`paste`/`copyAsMarkdown`/
etc. not found) because re-pinning Markoff straight to its `master`
HEAD (rather than the `feature/rich-clipboard` commit Corbomite's old
pin actually sat on) silently dropped Cluster N's clipboard API —
Corbomite's own incremental dev-preset rebuilds missed this entirely
(a stale `.o` for the one `.cpp` that referenced the removed API never
got recompiled). Fixed by merging `feature/rich-clipboard` into
Markoff's `master` (clean merge, no conflicts) and re-verifying with
**genuinely clean rebuilds** (`rm -rf` the build dir, not `--fresh`)
end to end: Markoff standalone 213/213, Corbomite full suite 327/328
(`tst_gc` flaked under `-j10`, green standalone — same class as P3's
`tst_quadtree` flake), AppImage build+stage+launch+open-a-real-vault,
Arch package build+extract+launch+open-a-real-vault. Full story:
`decisions-archive.md`, 2026-08-21.

**After P1 (2026-08-20):** the single largest binary is now `libCorbomiteApp.so`
itself (143 MB, counted under `bin/` since `RUNTIME`/`LIBRARY DESTINATION`
both resolve there in the dev build tree) — every one of the 43 former
link sites now shares that one copy instead of embedding a private 47 MB
`.a`. `libvault.so` (67 MB) is the new second-largest. Every former
131-147 MB test binary dropped to the 55-56 MB range. Not yet flipped:
`corbomite-core`/`storage`/`models`/`bases`/`search`/`canvas`/`forcegraph`/
`jkqtmathtext` remain `STATIC` (P3) — this number will drop further.

Measured 2026-08-20 (P0.T2), clean `build-dev/` (`rm -rf` + reconfigure),
`dev` preset, `Debug`, `CORBOMITE_PORT_BUILD_TESTS=ON`, no ccache wired yet
(P0.T4 installed + wired it into `CMakePresets.json` immediately after this
baseline, specifically so it wouldn't contaminate these numbers). Next 17
largest test binaries after `tst_completion_controller` (147 MB) cluster
tightly at 130-131 MB each (`tst_view_zoom_dispatch`,
`tst_view_capabilities`, `tst_view_actions_provider`,
`tst_reading_styled_leaf`, six `tst_note_editor_widget_*`, `tst_markdown_view`,
`tst_link_activation`, `tst_canvasviewtab_vaultroot_paths`,
`tst_canvas_callout_live_load`, `tst_action_context`, `tst_toolbar_policy`) —
confirms the audit's framing that the bulk of the disk cost is many
near-identical app-level test binaries each embedding a private copy of the
same static link graph, not a few outliers.

---

## §6 Standing traps

1. **CMake cycles + `SHARED` = configure error.** Only all-`STATIC` SCCs are
   tolerated. C3 found one; P2.T4's grep and `tst_no_library_cycles` exist
   to stop a new one appearing.
2. **The executable exports nothing.** Do not reason "the host defines it, so
   the plugin will find it." Verified: 5204 dynsym entries, zero strong
   definitions. Interposition only ever flows *from* a `.so`.
3. **`libvault.so` is currently masquerading as libcore.** Several things
   work today for reasons that will change under this refactor. If something
   that worked before P3 breaks after, suspect that the symbol used to
   resolve into `libvault.so` and now resolves somewhere else — do not
   assume the new `.so` is at fault.
4. **A gate never seen red is not a gate.** P4.T1 must be demonstrated
   failing pre-P3. This applies with special force here because the bug's
   signature is a silent `nullptr`, never an error.
5. **Failures move from link time to load time.** Expected, per report §6.
   A missing symbol now surfaces as a `dlopen` failure or a plugin that
   silently fails to load, not a link error. Check the plugin-load log path
   when something goes missing.
6. **Dev-build KXMLGUI cache is shared across worktrees.** Unrelated to this
   cluster but directly in its blast radius, since P1 touches the resource
   that feeds it. If menus look wrong after P1, delete
   `~/.local/share/kxmlgui5/corbomite-dev/corbomite-devui.rc` before
   suspecting the code (see CLAUDE.md § Dev Build Isolation, and the
   Cluster O3 incident).
7. **Live-eyeball gate on anything chrome-adjacent.** Project memory
   `feedback_verify_ui_fixes_live`: offscreen-green has previously hidden
   real breakage. P1 and P5 both end in a human launching the app.
8. **`mmdr` stays static** (D6). Expect it in P4.T3's allowlist permanently
   until separately punch-listed.

---

## §7 Acceptance (cluster close)

- [x] Full offscreen suite green (current baseline **320/320** excl.
      `benchmark`), at every phase boundary, not just at the end. **325/325
      as of P4's close** (+`tst_no_library_cycles`, `tst_xmlgui_resource_present`,
      `tst_plugin_type_identity`, `tst_no_duplicate_metaobjects`).
- [x] `tst_plugin_type_identity` demonstrated red pre-P3, green post-P3.
- [x] `tst_no_duplicate_metaobjects` green with a documented allowlist.
- [x] `tst_no_library_cycles` green.
- [x] `tst_xmlgui_resource_present` green.
- [x] AppImage builds, launches, opens a vault.
- [x] Installed release build launches and loads all 9 plugins from the
      installed plugin dir. *(P1 already live-verified the installed
      release build launches with all libraries at that point's SHARED
      state; P5 re-verified with the full post-P3 set — all 9 plugins
      present at `/usr/local/lib/plugins/corbomite/`, `ldd` resolves
      cleanly post-RPATH-fix, and the user ran it live against a real
      vault.)*
- [x] §5 measurement table filled in *(P0/P1/P3/P6 rows — P5 is packaging
      verification, not a disk metric, so it added no new row)*.
- [x] Audit corrections addendum landed (P0.T1).
- [x] `decisions-archive.md` closeout paragraph; `PROJECT-STATE.md`
      §Current focus updated to ≤3 sentences; `INDEX.md` status updated.
      *(P6 closeout landed 2026-08-21 — cluster fully CLOSED, all phases
      P0-P6 done.)*
- [x] P6 (cross-repo: markoff-family + graffodil) — dispatched on
      explicit user go-ahead 2026-08-21, all five tasks (T1-T5) closed.
