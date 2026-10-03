# Handoff → jkqtmathtext / rapidyaml / rapidoaml maintainers: vendoring hygiene, not code defects

**From:** Corbomite
**Date:** 2026-10-02
**Corbomite HEAD:** `57e79fad`
**Pinned copies:** `libs/jkqtmathtext/` (in-tree, **no** upstream version
recorded); `libs/markoff-family/libs/rapidyaml/` (ryml 0.11.1)
**Severity (our view):** P3 for rapidyaml (one line). **P2** for jkqtmathtext,
and the pinning gap is **P1** for us in a release sense — see §1.

**Scope note.** We are *not* reporting algorithmic or correctness defects in
these libraries, and we have not done a code audit of them beyond what the
vendoring issue required. We found no reason to doubt the code. This document is
about **how we vendor you**, which is entirely our fault and therefore entirely
ours to describe precisely.

We are aware this is upstream-for-upstream's-sake: jkqtmathtext and rapidoaml
do not know or care how Corbomite vendors them. We are sending it anyway
because one item (§1) is a real provenance hole in a GPL project, and the rest
is cheap.

---

## 1. (P1, about us) We have no record of which jkqtmathtext version we vendored

`Corbomite/libs/jkqtmathtext/README.md` records the upstream URL, the copyright
holder and the licence. It does **not** record the upstream commit or release
tag. Nothing else in our tree records it either.

**Why that matters, and it is our problem rather than yours:** we cannot say what
we are shipping, cannot refresh it, and cannot audit a future CVE against a known
baseline. We have written the gap down explicitly in our README rather than
leaving it implicit.

### The two files in that directory disagree about provenance

| Source | Claim |
|---|---|
| `libs/jkqtmathtext/README.md` (`## Original Author`) | 2008–2024 |
| `libs/jkqtmathtext/jkqtmathtext.readme` (your own file) | 2010–2015 |

We have **left both as-is**. Your own file is the more likely authority for the
vendored *code*; our wider range probably reflects later edits to the extraction.
We did not want to "fix" this by guessing.

**What would help:** a statement of which release or commit the extraction
targets, ideally in `jkqtmathtext.readme` itself. Then our extraction's header
range can be reconciled against it rather than left as a contradiction.

---

## 2. (P2) We vendor jkqtmathtext twice, and ~19 MB of each copy is unread

The tree exists in two places:

- `Corbomite/libs/jkqtmathtext/` (12 MB)
- `Corbomite/libs/markoff-family/libs/jkqtmathtext/` (12 MB)

They are **byte-identical except for `CMakeLists.txt` and `README.md`**
(verified with `diff -rq`).

Only 6 `.otf` files — 1,452,876 bytes total — are referenced by `xits.qrc`. The
rest of each copy is unread by any build rule: `sources/` alone is **8.6 MB** of
`.fea` feature sources, plus `documentation/`, `test-suite/`, `tex/`, `tools/`,
`Makefile`, `requirements.txt`. So roughly **19 MB of the 24 MB** we carry is
dead weight.

### Why the duplication exists, in case it informs your advice

Our root `CMakeLists.txt:73-80` adds our copy **first**, deliberately:

```cmake
# jkqtmathtext and mmdr stay in Corbomite (shared by core + readingview +
# markoff). They must be added BEFORE libs/markoff-family so that markoff's
# defensive `if(NOT TARGET jkqtmathtext)` fallback inside the submodule
# short-circuits (otherwise it tries add_subdirectory(../jkqtmathtext),
# which resolves to the wrong path inside the submodule).
add_subdirectory(libs/jkqtmathtext)
```

So only one copy is compiled. But the mechanism is a **build-order dependency**
relying on a defensive `if(NOT TARGET)` guard — which is fragile, and it means
the second copy is invisible dead weight rather than an intentional fallback.

**What we are going to do (not an ask):** prune both copies to the `.qrc`
closure, and resolve the duplication — either drop the root copy and link
Markoff's target from `libs/core`, or document why both exist. Combined saving
≈19 MB. We are recording it here so the next person to look at our tree can see
it was considered and not overlooked.

---

## 3. (P3) jkqtmathtext installs the `.so` but not its headers

`libs/jkqtmathtext/CMakeLists.txt` does `install(TARGETS …)` with no
`install(FILES …)` for headers.

Our CMake comment notes that "`SHARED` is also the licence-friendlier form for
vendored LGPL code" — which we agree with, and which makes the missing headers
slightly more than a packaging nit: **LGPL §4's relinkability affordance is
incomplete** if the object code ships without the corresponding source-availability
story. Not a licence violation on our side (we vendor the sources, as §2
describes), but the install rule and the rationale disagree.

Also in that file: `target_include_directories(jkqtmathtext PUBLIC
$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>)`. Putting the **parent**
directory on the public include path means consumers resolve `<jkqtmathtext/…>`
by having `libs/` on their path — which also makes `<core/…>`, `<canvas/…>` and
`<vault/…>` includable from any target linking jkqtmathtext. Our
`libs/core/CMakeLists.txt:178` depends on this, so it is load-bearing for us
today. Staging headers under `include/jkqtmathtext/` would be the clean shape,
but that is a change to your layout and we are not asking for it — flagging it
because it is the kind of thing that surprises consumers.

---

## 4. (P3) rapidyaml: our tree force-sets one of your cache variables

`Corbomite/libs/markoff-family/CMakeLists.txt:50`

```cmake
set(RYML_DEFAULT_CALLBACK_USES_EXCEPTIONS ON CACHE BOOL "" FORCE)
```

This is a legitimate rapidoaml option (`rapidyaml/CMakeLists.txt:20`, default
OFF) and you already handle it correctly internally
(`rapidyaml/CMakeLists.txt:99-100` translates it into
`target_compile_definitions(ryml PUBLIC …)`). So the cache variable need never be
forced at all.

Three problems with the line:

1. it is set in **Markoff's** top-level, not rapidoaml's or `markoff-parser`'s,
   so it lands in whatever cache the ultimate consumer uses;
2. `FORCE` silently overrides an explicit `-D` from the command line;
3. it mutates the cache of a project you have never heard of.

**We are fixing this on our side** (moving it into `markoff-parser`'s
CMakeLists, dropping `FORCE`, guarding with `if(NOT DEFINED …)`). Recording it
here only so you know the variable is being set on you and why.

---

## 5. (P3) rapidoaml: a changelog gap in our vendored copy

`libs/rapidyaml/changelog/` ends at `0.9.0.md` plus `current.md`, but the
vendored version is **0.11.1**. Two releases of notes are missing from the tree,
so the copy cannot be audited against its own changelog.

This is squarely our vendoring omission, not an upstream defect. We will note the
gap in `libs/rapidyaml/README.md`. If a single-file changelog covering 0.10 and
0.11 is cheap for you to produce, it would close the gap for every consumer at
once; if not, we will carry the note.

---

## 6. One class of defect we specifically did *not* report

While looking at jkqtmathtext's node tree, we noticed
`jkqtmathtexthorizontallistnode.cpp:553-570` uses a raw owning `QList<node *>`
with manual `delete` — the classic double-free shape.

**We are not filing it.** We did not trace every call site, jkqtmathtext is
vendored-and-pinned code we cannot patch upstream, and an untraced
double-free report is worse than none. We are mentioning it only so that if you
happen to be touching that file, you know we looked and backed off.

---

## 7. Summary

| # | Item | Who acts |
|---|---|---|
| 1 | No pinned version; two files disagree on the copyright range | **You**, if a release statement exists. We will pin it and reconcile. |
| 2 | Vendored twice; ~19 MB of each copy unread | **Us** — no action needed |
| 3 | Installs `.so` without headers; PUBLIC include dir is its parent | **Us** (packaging), flagged for awareness |
| 4 | We force-set your cache variable | **Us** |
| 5 | Changelog stops at 0.9.0, vendored version is 0.11.1 | **Us**, unless a consolidated changelog is cheap |
| 6 | Untraced double-free shape | **Nobody** — deliberately not filed |

Nothing here is a code defect and nothing is urgent. If §1 is actionable we
would be grateful; the rest we are simply reporting so it is on the record
rather than discovered later.