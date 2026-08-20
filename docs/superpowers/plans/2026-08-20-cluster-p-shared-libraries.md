# Cluster P — STATIC → SHARED internal library refactor

**Opened:** 2026-08-20. **Type:** Stub — brainstorm + full plan expansion
required before dispatch (see convention in `INDEX.md`). **Track:**
strategic cluster. **Priority: dispatch next**, ahead of Cluster O Phase O4
and Cluster L5 soak resumption — user-directed 2026-08-20.

**Research report (read this first, in full, before planning):**
[`docs/audit-2026-08-20-shared-libraries-refactor.md`](../../audit-2026-08-20-shared-libraries-refactor.md).
That document is the substantial pre-work for this cluster — problem
evidence, the full library inventory with file:line citations, the vault
precedent, feasibility findings, cross-repo scope analysis, and tradeoffs.
Re-deriving any of that from scratch is wasted work; start from it.

## Why this cluster exists (one paragraph — full detail in the report)

`build-dev/` runs 10-11 GB per git worktree, driven by 33 app-level test
binaries at 131-147 MB each — because every internal library across
Corbomite, the `markoff-family` submodule, and the `graffodil` submodule is
`STATIC`, so every one of those 320 test binaries embeds its own private
copy of the libraries it links. Investigating this surfaced something more
important than the disk cost: `libs/vault` was *already* flipped
`STATIC` → `SHARED` in Cluster Q (2026-04-17) specifically because
plugins (`dlopen`-loaded `.so` modules) got a private copy of its RTTI/
`QMetaObject`s when static, silently breaking `qobject_cast` across the
host/plugin boundary. Every other library plugins link against —
`corbomite-core`/`storage`/`models`/`bases`, `markoff_core` and siblings,
`graffodil-core` and siblings — has the identical latent bug today, just
not yet triggered by what current in-tree plugins happen to do.

## Scope decision needed at plan-expansion time

The report's §5 flags this explicitly: the full disk-usage win *and* the
full correctness fix require converting libraries in **three repositories**
(Corbomite, `markoff-family`, `graffodil`), not just this one. A
Corbomite-only pass is a valid smaller-scope option but leaves the single
biggest disk contributor (markoff_core-linking test binaries) mostly
unaddressed and leaves the same correctness bug live in markoff-family's
and graffodil's own classes. Whoever expands this into a full plan should
make this scope call explicitly (and check it against the user, if
unclear) rather than defaulting narrow.

## Known constraints for whoever writes the phased plan

- No export-macro work needed anywhere (`-fvisibility=hidden` is not set
  project-wide) — confirmed by the vault precedent shipping with zero
  header annotations.
- RPATH and packaging (`PKGBUILD`, `.deb` script, AppImage/`linuxdeploy`)
  are already generic/automatic per the report's §4 — but **not
  independently verified by an actual packaging build in the research
  pass**. A real packaging build (at minimum the AppImage, since it's the
  most likely to surface a missed bundled-`.so` case) should be a named
  verification step, not assumed.
- `libs/mmdr` is a prebuilt Rust staticlib (Cargo `crate-type`), not a
  CMake target — converting it means touching the Rust crate, a different
  toolchain. Small contributor; likely fine to punch-list separately
  rather than block this cluster on it.
- Cross-repo changes should follow this project's established
  Markoff-first-ordering discipline (land the Markoff/Graffodil-side
  change and re-pin the submodule, mirroring how the 2026-05-25 foundation
  port and other cross-repo changes in `decisions-archive.md` were
  sequenced) — not edited in-place inside the submodule checkout from a
  Corbomite session.
- A companion (not substitute) fix worth a brainstorm note: no
  ccache/sccache is installed on the dev machine at all, which compounds
  the "every worktree rebuilds from scratch" cost independent of static
  vs. shared linking. Cheap, complementary, probably a much smaller task
  than this cluster — could be picked up alongside or separately.

## Suggested acceptance shape (not a committed plan — starting point only)

- Disk: `build-dev/` size before/after on a clean full build, same flags.
- Correctness: at least one regression test that positively proves a
  previously-`STATIC` library's type now round-trips correctly across a
  real plugin `.so` boundary (`qobject_cast` on a `Corbomite::Core` type
  handed from host to plugin, or equivalent) — the class of bug this
  cluster exists to close, not just "it still builds."
- A real packaging build (AppImage at minimum) succeeds and the app runs
  from the packaged artifact, not just from `build-dev/`.
- Full existing test suite stays green throughout (incremental, not one
  big-bang link-everything-and-pray commit) — the report deliberately
  leaves phase ordering (leaf-first vs. hub-first, all-at-once vs.
  incremental) as an open plan-design question rather than presupposing
  an answer.
