# Pre-consolidation `CLAUDE.md` state banner (superseded 2026-10-04)

> This is the 110-line "state of the world" banner that sat at the top of `CLAUDE.md`
> until 2026-10-04. It was accurate as written but accumulated resolved history:
> three archives, four cluster closeouts, two refuted audit claims, and a v0.1.0 release
> note, none of which a new agent needs in order to act. Kept because the *reasoning*
> behind several current decisions is only here — in particular why Reading mode is a
> `Styled::Editor` leaf, and the Cluster M↔O ownership boundary.

> **Do not read this as current state.** The current banner is at the top of
> `CLAUDE.md`; the current queue is `docs/punch-list.md`'s `Added 2026-10-04` section.

---

> **State of the world (refreshed 2026-06-10).**
>
> The 2026-05-25 foundation port is history: Markoff's QML/D2 rebuild merged at
> `v0.7.0-freeze`, the old four-leaf QGraphicsView editor is gone, and Corbomite
> hosts the **three Markoff leaves** (Canvas LivePreview, Source, Styled) via
> `NoteEditorWidget`. **Reading mode is a read-only `Markoff::Styled::Editor`
> leaf** (decided + shipped 2026-05-29; the earlier "read-only Live" steer is
> retired). `StyledRenderEngine` (2026-05-30) renders canvas cards headlessly.
>
> **Contract-v2 adoption COMPLETE (2026-06-10, road-to-dogfood Phase 1):**
> submodule pinned at `af91a936`; find-in-Reading, undo unification, theme
> propagation, Ln/Col, goToLine, ephemeral state, format-verb dispatch and
> zoom all run through the `Markoff::MarkdownView` base (MainWindow is
> grep-gated leaf-agnostic). **Next major workfront:** road-to-dogfood
> Phase 2 (completion revival first) —
> [`docs/superpowers/plans/2026-06-10-road-to-dogfood.md`](docs/superpowers/plans/2026-06-10-road-to-dogfood.md).
>
> **Dogfood loop started 2026-06-10 (Phase 6).** First real run surfaced a
> first-run SIGSEGV (use-after-free): double-clicking a recent vault on the
> welcome screen double-fired `vaultRequested`, so a second vault open tore the
> vault down while a live editor was attached, and the live binding held the
> freed `MarkoffDocument` by a never-detaching raw pointer. Fixed both ends —
> `WelcomeScreen` single-activation + markoff-family retire-on-destroy (re-pin
> `8112833f` → `af91a936`). Other first-run noise still open (see punch list):
> repeated `kf.xmlgui: Index 18 is not within range (0-16)`, plugin Id-in-
> metadata warnings, `qt.qml Invalid QML element name "Theme"`, portal app-id.
>
> **Cluster K — Markoff canvas leaf adoption, CLOSED (2026-08-18 Phase 5).**
> LivePreview is solely `Markoff::Canvas::EditorWidget`; the QML
> `markoff_live` leaf is unlinked (`MARKOFF_BUILD_LIVE=OFF`) and the
> `CanvasLivePreview` settings toggle is removed. Callouts remain frozen
> on Markoff E3 (not a canvas gap). Plan:
> [plan](docs/superpowers/plans/archive/2026-08-15-cluster-k-markoff-canvas-adoption.md).
>
> **Cluster L — workspace/KDDW stabilization & nativization, L0-L4
> landed+closed, L5 (soak & closeout) in progress (2026-08-17/18).**
> Full re-evaluation of the tab/dock layer found a teardown-UAF crash
> class, persistence split-brain (production save dropping
> `floating`/`lastOpenFiles`, Corbomite-private keys leaking into the
> Obsidian-shared `workspace.json`), and Obsidian-literalism residue. L0
> set the compat doctrine (schema-at-rest fidelity to Obsidian is the
> only contract; in-memory shape is free to be idiomatic Qt/KDE) —
> [spec](docs/superpowers/specs/2026-08-17-workspace-compat-boundary.md).
> L1 fixed the crash class (`Workspace::destroyLeaves`, ASAN-clean).
> L2 made `workspace.json` persistence
> full-fidelity and split Corbomite-native state into the doctrine's
> three tiers. L3 removed dead Obsidian-shape shells, consolidated KDDW
> init, and fixed router/resize perf. L4 (native UX polish — KDDW chrome,
> back/forward completion, tab commands, sidebar-width persistence)
> **live-verified and closed 2026-08-18**. **L5's Obsidian round-trip
> soak pass found and fixed a P0 bug**: a real Obsidian-authored vault's
> nested split layout landed scrambled on open, root-caused to an
> inverted divider-direction convention plus a materialization
> anchor-ordering bug in `WorkspaceSerializer.cpp` — fixed and
> live-verified across two round trips against the real vault (punch-list
> `[cluster-l]`, `decisions-archive.md`). **L5 soak paused, not closed** —
> superseded same-day by the v0.1.0 release push below; remaining soak
> items (drag between groups, split, popout, close-undo, vault switch)
> are still open —
> [plan](docs/superpowers/plans/2026-08-17-cluster-l-workspace-stabilization.md).
>
> **v0.1.0 tagged and released (2026-08-18), canonical host moved to
> GitHub.** Same day as the Cluster K Phase 5 closeout: `origin` retargeted
> from Codeberg to `git@github.com:clintonthegeek/Corbomite.git` (the
> `codeberg` remote still exists but is no longer canonical — **the
> `~/dev/CLAUDE.md` ownership registry has not been updated to match and
> now conflicts with reality**); `libs/markoff-family` submodule repointed
> to GitHub Markoff; app identity rebranded to
> `com.concernednetizen.Corbomite`. Packaging added: AppImage build script,
> Arch `PKGBUILD`, and a GitHub Actions Ubuntu 25.10 `.deb` CI building on
> every `v*` tag — plus post-tag fixups (`libtree-sitter-dev` for the `.deb`
> configure step, Qt6 private-dev packages for KDDockWidgets, a
> markoff-family bump for a Qt 6.9 `qHash` fix, and a `Qt >= 6.10`
> version-gate on `endFilterChange`). README rewritten with install/build
> instructions and screenshots. One stale artifact from the migration: a
> code comment in `MainWindow.cpp`'s Help-menu wiring still says "homepage
> + Codeberg" even though `KAboutData` only sets the concernednetizen.com
> homepage. No docs (`PROJECT-STATE.md`, `decisions-archive.md`) were
> updated for any of this until now — see the 2026-08-18 decisions-archive
> entry for full detail. **Next: resume Cluster L5 soak**, or continue
> hardening the release (multi-distro packaging, CI matrix).
>
> **Cluster N — rich clipboard (copy-as + smart paste), CLOSED and MERGED
> to `master` 2026-08-20.** N5 eyeball passed on the branch; a pre-merge
> cleanup pass then fixed the stalled paste-fix agent's incomplete work
> (canvas Smart paste still called `applyFlatEdit`, never the finished
> `Cmd::pasteMarkdown`) plus a BlockQuote marker asymmetry bug in canvas
> and Styled, and ported the 8 clipboard actions onto `MarkdownViewActions`
> (Cluster O's O3.T6 had removed the flat `MainWindow` mechanism N was
> built on while N sat on its own branch). Image paste/drop out of scope.
> Plan: `docs/superpowers/plans/archive/2026-08-20-cluster-n-rich-clipboard.md`.
> Post-reset N — not legacy Cluster N.
>
> **Cluster O — context-sensitive menu/toolbar/sidebar, O0-O3 CLOSED
> 2026-08-20** (see PROJECT-STATE for the current phase — this banner
> predates O3's close). Audit report:
> [`docs/audit-2026-08-20-context-sensitive-ui.md`](docs/audit-2026-08-20-context-sensitive-ui.md);
> [plan](docs/superpowers/plans/2026-08-20-cluster-o-context-sensitive-ui.md)
> (doctrine §D1-D7 is normative; all 12 open questions answered by the user).
> Headline: of ~79 KActions **zero** belong to canvas/bases/graph, 17 are
> permanently disabled, 7 silently no-op off-markdown. Closed **Cluster I**
> by absorption in the same pass
> ([retro](docs/cluster-retros/cluster-i-ui-surfacing.md) — note
> `cluster-retros/cluster-i.md` is the *pre-reset* Cluster I, a different
> cluster). Cluster M↔O boundary is recorded in the M plan's Phase M5 banner.
>
> Current parity status: [`docs/PARITY-MATRIX.md`](docs/PARITY-MATRIX.md).
> Port history: [`docs/port-foundation-exploration.md`](docs/port-foundation-exploration.md) (historical).
