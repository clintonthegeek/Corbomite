# Phase 1 — Markoff Re-pin + MarkdownView Contract-v2 Adoption — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Re-pin `libs/markoff-family` to Markoff master past the contract-v2 arc (Task 13, `1f9ebdc9`) plus a new upstream parser fix, then execute the adoption brief — unstubbing find-in-Reading, undo unification, theme propagation, format-verb dispatch, contextChanged toolbar state, ephemeral state, goToLine, and the Ln/Col statusbar — all through the polymorphic `Markoff::MarkdownView` base.

**Architecture:** Two repos, ordered: first a small TDD fix in `~/dev/Markoff` (the `![[…]]` image-node embed normalization steered 2026-06-04 — it never landed, and `tst_metadataparser` green depends on it), pushed to Codeberg so the new pin is fetchable. Then in `~/dev/Corbomite`: submodule re-pin, followed by mechanical adoption edits that replace every leaf-typed dispatch (`qobject_cast` switches, `LiveActionController` walks, `plainTextEdit()->undo()`) with calls on the `Markoff::MarkdownView*` base pointer.

**Tech Stack:** C++20, Qt 6.8, KDE Frameworks 6, CMake presets (`dev` → `build-dev/`), QtTest offscreen.

**Source spec:** `/home/clinton/dev/Markoff/docs/handoff/2026-06-09-corbomite-api-adoption-brief.md` (the adoption brief — READ IT FIRST; §2 is the migration table this plan executes, §3 the behavior notes). Roadmap context: `docs/superpowers/plans/2026-06-10-road-to-dogfood.md` Phase 1. Upstream fix spec: `docs/handoff/2026-06-04-to-markoff-embed-image-node-target.md`.

---

## Design principle — leaf-agnostic consumption (user directive 2026-06-10)

Markoff's canonical live-render view is **deliberately undecided long-term**: today Corbomite's `ViewMode::LivePreview` hosts the QML `Markoff::Live::EditorWidget`, but if the QWidget `Styled` editor catches up, the leaf behind that mode may be swapped. Therefore:

1. **All consumer operations dispatch through `Markoff::MarkdownView*`** (the base). After this phase, `MainWindow.cpp` must contain **zero** `markoff/live`, `markoff/source`, or `markoff/styled` includes and zero `Markoff::Live::`/`Markoff::Source::`/`Markoff::Styled::` mentions (Task 10 has the verifying grep).
2. **Leaf-typed code is permitted only in `NoteEditorWidget`** at construction/wiring sites, each marked with a `// leaf-specific:` comment so a future canonical-view swap is one grep away. The allowed list after this phase: the three `new` expressions, `m_editor->binding()->setLinkService(...)`, `m_styledReadingView->setLinkService(...)`, mermaid-renderer injection, and the `editor()`/`sourceEditor()` accessors (kept for tests).
3. **Plugin APIs that reach into the text widget stay blocked/deferred.** This is accepted; do not design plugin-facing editor surfaces in this phase.

## Hard rules for the implementer

- **NEVER `git add -A` in Corbomite.** The `testvaults/` working-tree changes (6 modified files + `films-vault/Untitled.md`) are deliberately uncommitted triage evidence — do not commit, revert, or clean them. Stage files explicitly by path in every commit.
- **Never re-pin into `8c13c5d..079ac1f`** (styled-table SIGSEGV window). The target commit is past `b1b238f` (the fix), so this is satisfied — but verify with the ancestry check in Task 2.
- **Corbomite tests:** `cd build-dev && QT_QPA_PLATFORM=offscreen ctest --output-on-failure -j 10`. Baseline before this phase: all green except `tst_metadataparser` (2 slots, the embed bug) and `tst_benchmark_layout` (times out by design — exclude with `-E benchmark`). Builds: `cmake --build --preset dev -j 10`.
- **Markoff tests:** `scripts/run-tests.sh -E 'tst_realistic|tst_benchmark'`. Baseline 266/269 — the 3 failing binaries (`tst_live_render_e2_nav_shift_extend`, `tst_live_render_focus_chokepoint_invariant`, `tst_live_render_cursor_typing_invariant`) are deterministic, documented (Markoff queue #10). Any OTHER failure is a regression you introduced.
- **When a Corbomite test breaks after the re-pin, classify before fixing**: styled tables now render as real `QTextTable` grids, so text-extraction/rendering assertions in styled-related tests may be *contract drift* (update the test) rather than bugs (fix the code).
- **Naming trap:** Corbomite has its own `class MarkdownView` (a workspace view wrapper; `MainWindow::activeMarkdownView()` returns it — it is NOT `Markoff::MarkdownView`). The path to the active Markoff leaf is `activeEditor()->activeLeaf()`. Never confuse the two.
- The brief's ordering constraint: `attachFindController` must be called **after** `setDocument`; detach before swapping documents. The existing call sites already obey this — preserve the order in every edit.

---

## Task 0: Preflight — verify the world matches this plan

**Files:** none (read-only checks)

- [ ] **Step 1: Verify Corbomite state**

```bash
cd /home/clinton/dev/Corbomite && git branch --show-current && git log --oneline -1 && git submodule status libs/markoff-family
```

Expected: branch `master`, HEAD at/past `0e84a6a7`, submodule at `ddf5e9a8` (prefixed `-` or space). `git status` will show the deliberate `testvaults/` modifications — leave them alone.

- [ ] **Step 2: Verify Markoff state**

```bash
cd /home/clinton/dev/Markoff && git branch --show-current && git log --oneline -1
```

Expected: branch `master`, HEAD `1f9ebdc9` (the Task-13 commit containing the adoption brief). If master has advanced past `1f9ebdc9`, that's fine — read `git log 1f9ebdc9..HEAD --oneline` and confirm nothing touches the contract headers before proceeding.

- [ ] **Step 3: Verify the Corbomite baseline build is green**

```bash
cd /home/clinton/dev/Corbomite && cmake --build --preset dev -j 10 && cd build-dev && QT_QPA_PLATFORM=offscreen ctest -E benchmark --output-on-failure -j 10 2>&1 | tail -5
```

Expected: build succeeds; exactly one failing test, `tst_metadataparser`.

---

## Task 1: Markoff — normalize `![[…]]` wiki-embeds parsed as `image` nodes

This executes the 2026-06-04 Corbomite steer (`docs/handoff/2026-06-04-to-markoff-embed-image-node-target.md` in Corbomite — read it; it is the spec, with acceptance criteria). Work happens in **`/home/clinton/dev/Markoff`** on `master`. The parser is outside Markoff's INVARIANTS seam scope; normal engineering judgment applies, but TDD is still mandatory.

**Files:**
- Create: `/home/clinton/dev/Markoff/libs/markoff-parser/tests/tst_embed_image_node.cpp`
- Modify: `/home/clinton/dev/Markoff/libs/markoff-parser/tests/CMakeLists.txt` (append registration)
- Modify: `/home/clinton/dev/Markoff/libs/markoff-parser/src/TreeSitterParser.cpp` (`extractLinkFromNode`, image branch at ~line 975)

- [ ] **Step 1: Write the failing test**

Create `libs/markoff-parser/tests/tst_embed_image_node.cpp`:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
//
// `![[…]]` wiki-embeds that tree-sitter parses as `image` nodes (rather
// than `wiki_link`) must yield the same LinkInfo as the wiki_link path:
// type=Embed, bracket-free target, consistent displayText convention.
// Spec: Corbomite steer 2026-06-04 (embed-image-node-target).
#include <QTest>
#include <markoff/parser/Document.h>

using namespace Markoff;

class TestEmbedImageNode : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void embedWithExtension()
    {
        Document doc = Document::fromMarkdown(QStringLiteral("![[Image.png]]\n"));
        QCOMPARE(doc.links().size(), 1);
        const LinkInfo &l = doc.links().at(0);
        QCOMPARE(l.type, LinkInfo::Embed);
        QCOMPARE(l.target, QStringLiteral("Image.png"));
        QVERIFY(!l.displayText.startsWith(QLatin1Char('[')));
    }

    void embedWithoutExtension()
    {
        Document doc = Document::fromMarkdown(QStringLiteral("![[Note]]\n"));
        QCOMPARE(doc.links().size(), 1);
        const LinkInfo &l = doc.links().at(0);
        QCOMPARE(l.type, LinkInfo::Embed);
        QCOMPARE(l.target, QStringLiteral("Note"));
        QVERIFY(!l.displayText.startsWith(QLatin1Char('[')));
    }

    void embedWithAlias()
    {
        Document doc = Document::fromMarkdown(QStringLiteral("![[Target|Alias]]\n"));
        QCOMPARE(doc.links().size(), 1);
        const LinkInfo &l = doc.links().at(0);
        QCOMPARE(l.type, LinkInfo::Embed);
        QCOMPARE(l.target, QStringLiteral("Target"));
        QCOMPARE(l.displayText, QStringLiteral("Alias"));
    }

    void standardImageUnchanged()
    {
        Document doc = Document::fromMarkdown(QStringLiteral("![alt](path.png)\n"));
        QCOMPARE(doc.links().size(), 1);
        const LinkInfo &l = doc.links().at(0);
        QCOMPARE(l.type, LinkInfo::Image);
        QCOMPARE(l.target, QStringLiteral("path.png"));
        QCOMPARE(l.displayText, QStringLiteral("alt"));
    }
};

QTEST_MAIN(TestEmbedImageNode)
#include "tst_embed_image_node.moc"
```

Append to `libs/markoff-parser/tests/CMakeLists.txt` (same pattern as the existing entries at the bottom of the file):

```cmake
add_executable(tst_embed_image_node tst_embed_image_node.cpp)
add_test(NAME tst_embed_image_node COMMAND tst_embed_image_node)
target_link_libraries(tst_embed_image_node PRIVATE Qt6::Test markoff-parser)
set_tests_properties(tst_embed_image_node PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
```

- [ ] **Step 2: Run the test, verify it fails the right way**

```bash
cd /home/clinton/dev/Markoff && cmake --build build-dev -j 8 --target tst_embed_image_node && QT_QPA_PLATFORM=offscreen ./build-dev/bin/tst_embed_image_node
```

Expected: `embedWithExtension` and/or `embedWithoutExtension` FAIL with `target == ""` and `displayText == "[…]"` (whichever shapes the grammar routes through `image`); `standardImageUnchanged` PASSES. If ALL embed slots pass, stop — the grammar is matching everything via `wiki_link` on this machine, the steer's premise needs re-verification; report to the user before touching production code.

- [ ] **Step 3: Implement the normalization**

In `libs/markoff-parser/src/TreeSitterParser.cpp`, replace the image branch of `extractLinkFromNode` (currently lines 975–995):

```cpp
    if (strcmp(type, "image") == 0) {
        int sb = static_cast<int>(ts_node_start_byte(node));
        int eb = static_cast<int>(ts_node_end_byte(node));
        out.sourceOffset = sb;
        out.sourceLength = eb - sb;

        // Obsidian wiki-embed matched by the `image` grammar rule (happens
        // for e.g. extension-less ![[Note]]): normalize exactly like the
        // wiki_link path above so the same `![[…]]` source yields the same
        // LinkInfo regardless of which rule matched. Corbomite steer
        // 2026-06-04 (embed-image-node-target).
        const QString raw = QString::fromUtf8(utf8.mid(sb, eb - sb));
        if (raw.startsWith(QStringLiteral("![["))) {
            out.type = LinkInfo::Embed;
            QString inner = raw.mid(3, raw.size() - 5);   // strip "![[" … "]]"
            const int pipeIdx = inner.indexOf(QLatin1Char('|'));
            if (pipeIdx >= 0) {
                out.target = inner.left(pipeIdx);
                out.displayText = inner.mid(pipeIdx + 1);
            } else {
                out.target = inner;
                out.displayText = inner;   // wiki_link convention: alias-less ⇒ displayText == target
            }
            out.structured = Markoff::Detail::decomposeWikilinkInner(QStringView{inner});
            return true;
        }

        out.type = LinkInfo::Image;
        out.target.clear();
        out.displayText.clear();
        uint32_t count = ts_node_child_count(node);
        for (uint32_t i = 0; i < count; ++i) {
            TSNode child = ts_node_child(node, i);
            const char *ct = ts_node_type(child);
            int cs = static_cast<int>(ts_node_start_byte(child));
            int ce = static_cast<int>(ts_node_end_byte(child));
            if (strcmp(ct, "image_description") == 0 || strcmp(ct, "link_text") == 0) {
                out.displayText = QString::fromUtf8(utf8.mid(cs, ce - cs));
            } else if (strcmp(ct, "link_destination") == 0) {
                out.target = QString::fromUtf8(utf8.mid(cs, ce - cs));
            }
        }
        out.structured.url = out.target;
        return true;
    }
```

(Note: the only changes to the standard-image fall-through are that `sourceOffset`/`sourceLength` are now set before the wiki-embed check — same values — and `out.type = LinkInfo::Image` moves below it. The child loop is byte-for-byte the existing code.)

- [ ] **Step 4: Run the new test, verify green**

```bash
cmake --build build-dev -j 8 --target tst_embed_image_node && QT_QPA_PLATFORM=offscreen ./build-dev/bin/tst_embed_image_node
```

Expected: all 4 slots PASS.

- [ ] **Step 5: Run the Markoff fast suite, verify no regressions**

```bash
scripts/run-tests.sh -E 'tst_realistic|tst_benchmark'
```

Expected: same 3 known failures (queue #10), nothing new. Total passes = old 266 + 1 new binary.

- [ ] **Step 6: Commit and push to Codeberg, record the SHA**

```bash
cd /home/clinton/dev/Markoff && git add libs/markoff-parser/src/TreeSitterParser.cpp libs/markoff-parser/tests/tst_embed_image_node.cpp libs/markoff-parser/tests/CMakeLists.txt
git commit -m "fix(parser): normalize ![[…]] wiki-embeds matched by the image grammar rule

The image branch of extractLinkFromNode now detects the wiki-embed shape
from the raw node bytes and extracts it identically to the wiki_link
branch (type=Embed, bracket-free target, displayText==target when
alias-less), instead of returning target=\"\" / displayText=\"[X]\".

Executes the Corbomite steer
2026-06-04-to-markoff-embed-image-node-target; the two Corbomite-side
oracles (tst_metadataparser) go green on the next re-pin."
git push origin master
git log --oneline -1
```

The printed SHA is **`<REPIN_SHA>`** — substitute it everywhere below.

---

## Task 2: Corbomite — re-pin `libs/markoff-family` to `<REPIN_SHA>`

**Files:**
- Modify: gitlink `libs/markoff-family` (submodule pointer only)

- [ ] **Step 1: Fetch and checkout the target in the submodule**

```bash
cd /home/clinton/dev/Corbomite/libs/markoff-family && git fetch origin && git checkout <REPIN_SHA> && git submodule update --init --recursive
```

(The nested `libs/collabtext` pin is unchanged in this range — the update is belt-and-braces.)

- [ ] **Step 2: Safety checks — forbidden window and Task-13 inclusion**

```bash
git merge-base --is-ancestor 079ac1f2 HEAD && echo "PAST forbidden window: OK"
git merge-base --is-ancestor b1b238fd HEAD && echo "SIGSEGV fix included: OK"
git merge-base --is-ancestor 1f9ebdc9 HEAD && echo "Task 13 included: OK"
```

All three must print OK. If any fails, STOP — wrong target.

- [ ] **Step 3: Reconfigure and rebuild Corbomite**

```bash
cd /home/clinton/dev/Corbomite && cmake --preset dev && cmake --build --preset dev -j 10
```

Expected: clean build. Pre-verified: Corbomite has no references to the APIs this range removed (`Styled::Editor::fontScale()` getter, leaf-level `themeChanged`/`fontScaleChanged` signals). If a compile error appears anyway, consult the brief §1–2 and the breaking-changes summary: leaf-level theme/fontScale signals moved to the `MarkdownView` base; format verbs became base virtuals.

- [ ] **Step 4: Run the full suite — the embed oracles must flip green**

```bash
cd build-dev && QT_QPA_PLATFORM=offscreen ctest -E benchmark --output-on-failure -j 10 2>&1 | tail -8
```

Expected: `tst_metadataparser` now PASSES (both `testParseEmbedVsLink` and `testParseEmbedAsImageNode`). If other tests newly fail, classify: styled-table rendering changes (tables are now `QTextTable` grids) are expected contract drift in styled/render tests — update those tests to the new rendering, citing brief §2 "Reading-mode read-only" / styled tables. Anything not explainable by the brief is a regression: stop and investigate before proceeding.

- [ ] **Step 5: Commit the gitlink (explicit staging — never `git add -A`)**

```bash
cd /home/clinton/dev/Corbomite && git add libs/markoff-family
# plus any test files you updated in Step 4, by explicit path
git commit -m "build: re-pin markoff-family ddf5e9a8 → <REPIN_SHA> (contract v2 + embed image-node fix)

Past Task 13 (1f9ebdc9), outside the 8c13c5d..079ac1f styled-table
SIGSEGV window. tst_metadataparser green: the 2026-06-04 embed steer
landed upstream. Styled tables + find + format verbs now available on
the MarkdownView base contract; adoption edits follow."
```

---

## Task 3: Find-attach via the base virtual (find arrives in Reading mode)

**Files:**
- Modify: `src/editor/NoteEditorWidget.cpp` (three sites: `setViewMode` ~280–312, `showFindBar` ~521–539, `hideFindBar` ~541–554)
- Test: `tests/editor/tst_reading_styled_leaf.cpp` (add one slot)

- [ ] **Step 1: Write the failing test — find navigation scrolls the Reading leaf**

Add to `tests/editor/tst_reading_styled_leaf.cpp` (follow the file's existing fixture pattern for constructing `NoteEditorWidget` + `NoteDocument`; the helpers already there for building a widget in Reading mode can be reused):

```cpp
    // Phase 1 (contract v2): showFindBar in Reading mode must attach the
    // FindController to the styled leaf via the MarkdownView base virtual.
    // Falsifiable: with the old Live/Source qobject_cast switch the styled
    // leaf is never attached, navigation does not scroll, and this fails.
    void findAttachesInReadingMode()
    {
        NoteEditorWidget widget;
        widget.resize(600, 240);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));

        NoteDocument doc(QStringLiteral("/tmp/vault"), QStringLiteral("note.md"));
        // 80 paragraphs; the needle only matches in the last one, far below
        // the initial viewport.
        QStringList blocks;
        for (int i = 0; i < 79; ++i)
            blocks << QStringLiteral("Filler paragraph %1.").arg(i);
        blocks << QStringLiteral("ZZUNIQUEZZ at the bottom.");
        doc.setMarkdown(blocks.join(QStringLiteral("\n\n")));
        widget.setNoteDocument(&doc);
        widget.setViewMode(NoteEditorWidget::ViewMode::Reading);

        auto *leaf = widget.activeLeaf();
        QVERIFY(leaf);
        QCOMPARE(leaf->scrollPositionVisualLine(), 0.0f);

        widget.showFindBar();
        auto *fc = doc.findController();
        fc->setNeedle(QStringLiteral("ZZUNIQUEZZ"));
        QVERIFY(fc->matchCount() >= 1);
        fc->findNext();
        QTest::qWait(50);

        // The attached StyledFindAdapter must have scrolled toward the match.
        QVERIFY2(leaf->scrollPositionVisualLine() > 0.0f,
                 "find navigation did not scroll the Reading leaf — "
                 "attachFindController not reaching the styled leaf");
    }
```

Add the needed includes at the top of the file if absent: `#include <markoff/core/FindController.h>`, `#include <markoff/core/MarkdownView.h>`, `#include "corbomite/core/NoteDocument.h"`.

- [ ] **Step 2: Build and run — verify it fails**

```bash
cd /home/clinton/dev/Corbomite && cmake --build --preset dev -j 10 && cd build-dev && QT_QPA_PLATFORM=offscreen ctest -R tst_reading_styled_leaf --output-on-failure
```

Expected: FAIL at the final QVERIFY2 (styled leaf never attached under the old switch).

- [ ] **Step 3: Replace the three qobject_cast switches with base calls**

In `src/editor/NoteEditorWidget.cpp`:

`showFindBar()` — replace the cast switch:

```cpp
void NoteEditorWidget::showFindBar()
{
    if (!m_doc) return;
    auto *fc = m_doc->findController();
    m_findBar->setController(fc);
    // Polymorphic attach on the MarkdownView base (contract v2) — works for
    // all three leaves; Reading (styled) gains find with this call. Must be
    // called after setDocument (brief §2 find-attach behavioral note).
    if (auto *leaf = activeLeaf())
        leaf->attachFindController(fc);
    fc->activate();
    m_findBar->show();
    m_findBar->focusLineEdit();
}
```

`hideFindBar()` — same collapse:

```cpp
void NoteEditorWidget::hideFindBar()
{
    if (m_doc) {
        if (auto *leaf = activeLeaf())
            leaf->detachFindController();
        m_doc->findController()->deactivate();
    }
    m_findBar->hide();
    if (auto *leaf = activeLeaf()) leaf->setFocus();
}
```

`setViewMode()` — the detach site (step 2 of the transition, ~lines 280–287) becomes:

```cpp
    if (isFindBarVisible() && m_doc) {
        if (auto *leaf = activeLeaf())
            leaf->detachFindController();
    }
```

and the attach site (step 4 of the transition, ~lines 304–312) becomes:

```cpp
    if (isFindBarVisible() && m_doc) {
        auto *fc = m_doc->findController();
        if (auto *leaf = activeLeaf())
            leaf->attachFindController(fc);   // after setDocument, per contract
    }
```

Also remove the now-stale comment block ("the call site needs a downcast since MarkdownView itself doesn't expose attachFindController") — it is no longer true.

- [ ] **Step 4: Build, run the test — verify green; run the editor test dir**

```bash
cmake --build --preset dev -j 10 && cd build-dev && QT_QPA_PLATFORM=offscreen ctest -R 'tst_reading_styled_leaf|tst_note_editor|tst_findbar' --output-on-failure
```

Expected: all PASS.

- [ ] **Step 5: Commit**

```bash
git add src/editor/NoteEditorWidget.cpp tests/editor/tst_reading_styled_leaf.cpp
git commit -m "feat(editor): find-attach via MarkdownView base — Reading mode gains find

Collapses the Live/Source qobject_cast switches in showFindBar/
hideFindBar/setViewMode to the contract-v2 base virtual. Adoption
brief §2 (find-attach). attach stays ordered after setDocument."
```

---

## Task 4: Undo/redo via the base (fixes the Source dual-stack divergence)

**Files:**
- Modify: `src/app/MainWindow.cpp` (`KStandardAction::undo`/`redo` lambdas, ~lines 1299–1325)

- [ ] **Step 1: Replace the per-mode switch with base dispatch**

The current lambdas branch on view mode: Source calls `src->plainTextEdit()->undo()` (the INVARIANTS §3 dual-authority anti-pattern — Qt's widget stack diverges from the D2 document stack), Live triggers `LiveActionController::undoAction()`. Replace both lambdas:

```cpp
    KStandardAction::undo(this, [this]() {
        if (auto *bv = activeBasesView()) {
            bv->undo();
            return;
        }
        if (auto *editor = activeEditor())
            if (auto *leaf = editor->activeLeaf())
                leaf->undo();   // base-implemented: doc->undoD2(); no-op while read-only
    }, ac);

    KStandardAction::redo(this, [this]() {
        if (auto *bv = activeBasesView()) {
            bv->redo();
            return;
        }
        if (auto *editor = activeEditor())
            if (auto *leaf = editor->activeLeaf())
                leaf->redo();
    }, ac);
```

Keep the `activeBasesView()` branch exactly as-is — Bases tables have their own undo.

- [ ] **Step 2: Build and run the suite**

```bash
cmake --build --preset dev -j 10 && cd build-dev && QT_QPA_PLATFORM=offscreen ctest -E benchmark --output-on-failure -j 10 2>&1 | tail -3
```

Expected: green (the base `undo()`→`undoD2` path is covered upstream by the three `tst_view_contract_*` suites).

- [ ] **Step 3: Commit**

```bash
git add src/app/MainWindow.cpp
git commit -m "fix(app): undo/redo through MarkdownView base — retires plainTextEdit() Qt-stack undo

Source mode previously invoked QPlainTextEdit's own undo stack, which
diverges from MarkoffDocument::undoD2 (Markoff INVARIANTS §3 dual-
authority anti-pattern). All three leaves now dispatch via the base,
which routes to undoD2/redoD2. Adoption brief §2 (undo/redo)."
```

---

## Task 5: Theme propagation to all leaves

**Files:**
- Modify: `src/editor/NoteEditorWidget.cpp` (`applyThemeToAllLeaves` ~139–146, `ensureWidgetConstructed` ~180–211)
- Modify: `src/editor/NoteEditorWidget.h` (add private `wireLeaf` declaration)

- [ ] **Step 1: Add the `wireLeaf` helper (extended by Tasks 7–8)**

In `NoteEditorWidget.h`, after the `restoreEphemeralStateFor` declaration in the private section:

```cpp
    // Phase 1 (contract v2) — one-time wiring applied to each leaf at
    // construction: theme application now; contextChanged + cursor
    // forwarding are added by the toolbar-state and statusbar tasks.
    // Takes the base pointer: wiring must stay leaf-agnostic.
    void wireLeaf(Markoff::MarkdownView *leaf);
```

In `NoteEditorWidget.cpp`:

```cpp
void NoteEditorWidget::wireLeaf(Markoff::MarkdownView *leaf)
{
    if (m_themeService)
        leaf->setTheme(m_themeService->currentTheme());
}
```

Call it at the end of the constructor for the eager leaf (after `m_editor->installEventFilter(this);`):

```cpp
    wireLeaf(m_editor);
```

And in `ensureWidgetConstructed`, after each lazy construction completes — in the `ViewMode::Source` branch after `m_sourceIndex = m_stack->addWidget(m_sourceEditor);` add `wireLeaf(m_sourceEditor);`, and in the `ViewMode::Reading` branch after `m_readingIndex = m_stack->addWidget(m_styledReadingView);` add `wireLeaf(m_styledReadingView);`.

- [ ] **Step 2: Implement `applyThemeToAllLeaves`**

Replace the no-op body (the TODO about `binding()->setTheme` is obsolete — Live now overrides the base `setTheme` and forwards to its binding itself, brief §2 theme):

```cpp
void NoteEditorWidget::applyThemeToAllLeaves()
{
    if (!m_themeService) return;
    const Markoff::Theme t = m_themeService->currentTheme();
    const std::initializer_list<Markoff::MarkdownView *> leaves{
        m_editor, m_sourceEditor, m_styledReadingView};
    for (Markoff::MarkdownView *view : leaves)
        if (view) view->setTheme(t);
}
```

(`setThemeService` already connects `ThemeService::themeChanged` → `applyThemeToAllLeaves` and calls it once — no change needed there.)

- [ ] **Step 3: Build + suite**

```bash
cmake --build --preset dev -j 10 && cd build-dev && QT_QPA_PLATFORM=offscreen ctest -E benchmark --output-on-failure -j 10 2>&1 | tail -3
```

Expected: green. (No new automated test: constructing `Core::ThemeService` requires a `KColorSchemeManager`; the application of a `Theme` to each leaf is upstream-contract-tested. Theme propagation is on the Task 11 manual checklist.)

- [ ] **Step 4: Commit**

```bash
git add src/editor/NoteEditorWidget.cpp src/editor/NoteEditorWidget.h
git commit -m "feat(editor): theme propagation via MarkdownView::setTheme on all leaves

applyThemeToAllLeaves was a no-op TODO since the foundation port. Live
overrides base setTheme and forwards to its binding (no binding()
escape hatch needed). Lazily-constructed leaves get the current theme
at construction via the new wireLeaf hook. Adoption brief §2 (theme)."
```

---

## Task 6: Format verbs on the base pointer; delete `addEditorActionForwarded`

**Files:**
- Modify: `src/app/MainWindow.cpp` (`addEditorActionForwarded` lambda + its 5 call sites, ~1516–1593; `onSetHeading`, ~516–542)

- [ ] **Step 1: Replace the helper and its call sites**

Delete the `addEditorActionForwarded` lambda (~lines 1516–1536: the one taking a `LiveActionController` accessor member-pointer and a `Source::Editor` method pointer). In its place add a base-dispatch helper:

```cpp
    // Contract v2: format verbs are virtuals on Markoff::MarkdownView; one
    // base call covers all three leaves (no-op on leaves without editing —
    // hasEditing() drives the enabled state, wired in onEditorContextChanged).
    auto addEditorActionBase = [this, ac](
        const QString &objName, const QString &icon, const QString &label,
        const QKeySequence &shortcut,
        void (Markoff::MarkdownView::*verb)()) -> QAction* {
        auto *act = ac->addAction(objName);
        act->setText(label);
        if (!icon.isEmpty()) act->setIcon(QIcon::fromTheme(icon));
        if (!shortcut.isEmpty()) ac->setDefaultShortcut(act, shortcut);
        connect(act, &QAction::triggered, this, [this, verb]() {
            if (auto *editor = activeEditor())
                if (auto *leaf = editor->activeLeaf())
                    (leaf->*verb)();
        });
        return act;
    };
```

Rewrite the five call sites — these object names, icons, labels, and shortcuts are verbatim from the current code (lines 1561–1593); the four format actions stay **checkable** (toolbar/menubar parity — the new `EditorContext` carries no inline-span state, so their checked state is not yet driven; that is a known v2 contract gap, not a regression):

```cpp
    // Format: Bold / Italic / Strikethrough / Inline code — checkable for
    // toolbar/menubar parity. Contract-v2 EditorContext has no inline-span
    // fields, so checked state is not yet synced (same as before this change).
    if (auto *a = addEditorActionBase(
            QStringLiteral("format_bold"),
            QStringLiteral("format-text-bold"), i18n("Bold"), QKeySequence::Bold,
            &Markoff::MarkdownView::toggleBold))
        a->setCheckable(true);
    if (auto *a = addEditorActionBase(
            QStringLiteral("format_italic"),
            QStringLiteral("format-text-italic"), i18n("Italic"), QKeySequence::Italic,
            &Markoff::MarkdownView::toggleItalic))
        a->setCheckable(true);
    if (auto *a = addEditorActionBase(
            QStringLiteral("format_strikethrough"),
            QStringLiteral("format-text-strikethrough"), i18n("Strikethrough"),
            QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_X),
            &Markoff::MarkdownView::toggleStrikethrough))
        a->setCheckable(true);
    if (auto *a = addEditorActionBase(
            QStringLiteral("format_inline_code"),
            QStringLiteral("code-context"), i18n("Inline Code"),
            QKeySequence(Qt::CTRL | Qt::Key_E),
            &Markoff::MarkdownView::toggleInlineCode))
        a->setCheckable(true);

    addEditorActionBase(QStringLiteral("insert_link"),
                        QStringLiteral("insert-link"), i18n("Insert Link"),
                        QKeySequence(Qt::CTRL | Qt::Key_K),
                        &Markoff::MarkdownView::insertLink);
```

The `addEditorActionBase` helper's `QKeySequence` parameter must accept both `QKeySequence::StandardKey` and explicit sequences — taking `const QKeySequence &shortcut` (as written above) covers both since `StandardKey` converts implicitly.

- [ ] **Step 2: Rewrite `onSetHeading` (~lines 516–542)**

```cpp
void MainWindow::onSetHeading(int level)
{
    auto *editor = activeEditor();
    if (!editor || level < 0 || level > 6) return;
    if (auto *leaf = editor->activeLeaf())
        leaf->setHeadingLevel(level);   // 0 strips ATX markers, 1..6 sets
}
```

- [ ] **Step 3: Build + suite, commit**

```bash
cmake --build --preset dev -j 10 && cd build-dev && QT_QPA_PLATFORM=offscreen ctest -E benchmark --output-on-failure -j 10 2>&1 | tail -3
cd .. && git add src/app/MainWindow.cpp
git commit -m "refactor(app): format verbs + heading on MarkdownView base; delete addEditorActionForwarded

One polymorphic call per verb replaces the dual LiveActionController/
Source::Editor dispatch. Styled (Reading) inherits the verbs but is
read-only, so they correctly no-op there. Adoption brief §2 (format)."
```

---

## Task 7: `contextChanged` → toolbar enable-state + heading radio

**Files:**
- Modify: `src/editor/NoteEditorWidget.h` (new signal), `src/editor/NoteEditorWidget.cpp` (`wireLeaf`)
- Modify: `src/app/MainWindow.cpp` (`connectEditorContext` ~624–630, `onEditorContextChanged` ~631–639, heading group creation ~1615–1616, viewMode lambda ~1157–1163, service-propagation site ~1942), `src/app/MainWindow.h` (declare `updateEditorActionStates`)

- [ ] **Step 1: Forward leaf contexts through NoteEditorWidget**

`NoteEditorWidget.h`: add `#include <markoff/core/EditorContext.h>` to the includes, and in `Q_SIGNALS`:

```cpp
    // Contract v2: re-emitted from whichever leaf is active. Inactive leaves
    // are detached from the document and silent, but the leaf == activeLeaf()
    // guard in wireLeaf makes that explicit.
    void editorContextChanged(const Markoff::EditorContext &ctx);
```

`NoteEditorWidget.cpp`: extend `wireLeaf`:

```cpp
void NoteEditorWidget::wireLeaf(Markoff::MarkdownView *leaf)
{
    if (m_themeService)
        leaf->setTheme(m_themeService->currentTheme());
    connect(leaf, &Markoff::MarkdownView::contextChanged, this,
            [this, leaf](const Markoff::EditorContext &ctx) {
                if (leaf == activeLeaf())
                    Q_EMIT editorContextChanged(ctx);
            });
}
```

- [ ] **Step 2: Wire MainWindow — connectEditorContext + onEditorContextChanged**

Replace the `connectEditorContext` stub:

```cpp
void MainWindow::connectEditorContext(NoteEditorWidget *editor)
{
    connect(editor, &NoteEditorWidget::editorContextChanged,
            this, &MainWindow::onEditorContextChanged, Qt::UniqueConnection);
}
```

Replace the `onEditorContextChanged` stub:

```cpp
void MainWindow::onEditorContextChanged(const Markoff::EditorContext &ctx)
{
    // Heading radio: check H<n> while the caret sits in a heading block,
    // clear otherwise (group policy is ExclusiveOptional, set at creation).
    auto *ac = actionCollection();
    const bool isHeading =
        ctx.blockKind == QLatin1String(Markoff::BlockKindNames::Heading);
    for (int level = 1; level <= 6; ++level) {
        if (auto *a = ac->action(QStringLiteral("heading_%1").arg(level)))
            a->setChecked(isHeading && ctx.headingLevel == level);
    }
    updateEditorActionStates();
}
```

Note: `Markoff::BlockKindNames` lives in `<markoff/core/EditorContext.h>`, already included by `MainWindow.h`. EditorContext carries no `isReadOnly` — `hasEditing()` on the leaf gates the verbs (next step).

- [ ] **Step 3: Heading group must allow "no heading checked"**

At the heading-group creation (~line 1615–1616), replace `headingGroup->setExclusive(true);` with:

```cpp
    headingGroup->setExclusionPolicy(QActionGroup::ExclusionPolicy::ExclusiveOptional);
```

- [ ] **Step 4: Enable-state from `hasEditing()`**

`MainWindow.h` (private section, near `connectEditorContext`):

```cpp
    // Contract v2: format verbs + heading actions enabled iff the active
    // Markoff leaf advertises editing (hasEditing() — false in Reading,
    // false while read-only).
    void updateEditorActionStates();
```

`MainWindow.cpp`:

```cpp
void MainWindow::updateEditorActionStates()
{
    auto *editor = activeEditor();
    Markoff::MarkdownView *leaf = editor ? editor->activeLeaf() : nullptr;
    const bool canEdit = leaf && leaf->hasEditing();

    auto *ac = actionCollection();
    const QStringList verbActions{
        QStringLiteral("format_bold"),       QStringLiteral("format_italic"),
        QStringLiteral("format_strikethrough"),
        QStringLiteral("format_inline_code"), QStringLiteral("insert_link")};
    for (const QString &name : verbActions)
        if (auto *a = ac->action(name)) a->setEnabled(canEdit);
    for (int level = 1; level <= 6; ++level)
        if (auto *a = ac->action(QStringLiteral("heading_%1").arg(level)))
            a->setEnabled(canEdit);
}
```

Call it from the two leaf-switch sites:
1. The property-guarded `viewModeChanged` lambda (~1157–1163) — replace the Reading-label body (see Task 8 Step 3, which rewrites this lambda fully).
2. After `connectEditorContext(editor); connectEditorContextMenu(editor);` (~1942), add `updateEditorActionStates();`.

- [ ] **Step 5: Build + suite, commit**

```bash
cmake --build --preset dev -j 10 && cd build-dev && QT_QPA_PLATFORM=offscreen ctest -E benchmark --output-on-failure -j 10 2>&1 | tail -3
cd .. && git add src/editor/NoteEditorWidget.h src/editor/NoteEditorWidget.cpp src/app/MainWindow.h src/app/MainWindow.cpp
git commit -m "feat(app): contextChanged drives heading radio + format enable-state

All three leaves emit contextChanged(EditorContext) under contract v2.
NoteEditorWidget re-emits from the active leaf; MainWindow checks the
H1-H6 radio from blockKind/headingLevel and gates the verbs on
hasEditing(). Known upstream staleness window (Markoff queue #15) —
kind-changes that don't move the caret update on next caret move; do
not chase. Adoption brief §2 (contextChanged)."
```

---

## Task 8: Cursor info → Ln/Col statusbar (all modes, including Reading)

**Files:**
- Modify: `src/editor/NoteEditorWidget.cpp` (`wireLeaf`, `setViewMode`, `currentLine`/`currentColumn` ~335–345)
- Modify: `src/app/MainWindow.cpp` (viewMode lambda ~1157–1163)
- Test: `tests/editor/tst_note_editor_widget_mode_transition.cpp` (add one slot)

- [ ] **Step 1: Write the failing test**

Add a slot to `tests/editor/tst_note_editor_widget_mode_transition.cpp` (reuse the file's existing widget/doc fixture pattern):

```cpp
    // Phase 1 (contract v2): leaf cursor movement must surface as
    // cursorInfoChanged(line, column, wordCount) for the statusbar.
    void cursorMovesEmitCursorInfo()
    {
        NoteEditorWidget widget;
        widget.resize(600, 240);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));

        NoteDocument doc(QStringLiteral("/tmp/vault"), QStringLiteral("note.md"));
        doc.setMarkdown(QStringLiteral("alpha\n\nbravo\n\ncharlie"));
        widget.setNoteDocument(&doc);
        widget.setViewMode(NoteEditorWidget::ViewMode::Source);

        QSignalSpy spy(&widget, &NoteEditorWidget::cursorInfoChanged);
        widget.activeLeaf()->setCursorPosition({3, 1});
        QTest::qWait(20);

        QVERIFY2(!spy.isEmpty(), "no cursorInfoChanged after cursor move");
        const auto args = spy.last();
        QCOMPARE(args.at(0).toInt(), 3);   // 1-based flat visual line
    }
```

(Add `#include <QSignalSpy>` and `#include <markoff/core/MarkdownView.h>` if the file lacks them.)

- [ ] **Step 2: Run it — expect FAIL** (no connection exists from leaf cursor signals to `onCursorPositionChanged`):

```bash
cmake --build --preset dev -j 10 && cd build-dev && QT_QPA_PLATFORM=offscreen ctest -R tst_note_editor_widget_mode_transition --output-on-failure
```

- [ ] **Step 3: Implement**

(a) Extend `wireLeaf` in `NoteEditorWidget.cpp` with the cursor connection:

```cpp
    connect(leaf, &Markoff::MarkdownView::cursorPositionChanged, this,
            [this, leaf](int line, int column) {
                if (leaf == activeLeaf())
                    onCursorPositionChanged(line, column);
            });
```

(`onCursorPositionChanged` already exists and emits `cursorInfoChanged(line, column, m_cachedWordCount)`; MainWindow is already connected to that at `propagateServicesToView`. Word count stays 0 — that's Phase 2 scope.)

(b) Refresh the label on mode switch — at the end of `setViewMode`, just before `Q_EMIT viewModeChanged(newMode);`:

```cpp
    // Refresh statusbar cursor info for the incoming leaf.
    if (auto *leaf = activeLeaf()) {
        const Markoff::CursorPos pos = leaf->cursorPosition();
        onCursorPositionChanged(pos.line, pos.column);
    }
```

(c) Implement `currentLine()`/`currentColumn()` (~335–345), replacing the TODO stubs:

```cpp
int NoteEditorWidget::currentLine() const
{
    auto *leaf = activeLeaf();
    return leaf ? leaf->cursorPosition().line : 0;
}

int NoteEditorWidget::currentColumn() const
{
    auto *leaf = activeLeaf();
    return leaf ? leaf->cursorPosition().column : 0;
}
```

(d) In `MainWindow.cpp`, rewrite the property-guarded `viewModeChanged` lambda (~1157–1163). Reading now has a live caret (styled keeps caret/selection while read-only), so the `"Reading"` label override is retired:

```cpp
            if (!editor->property("_mw_viewmode").toBool()) {
                editor->setProperty("_mw_viewmode", true);
                connect(editor, &NoteEditorWidget::viewModeChanged,
                        this, [this](NoteEditorWidget::ViewMode) {
                    updateEditorActionStates();
                });
            }
```

- [ ] **Step 4: Run tests — green; commit**

```bash
cmake --build --preset dev -j 10 && cd build-dev && QT_QPA_PLATFORM=offscreen ctest -E benchmark --output-on-failure -j 10 2>&1 | tail -3
cd .. && git add src/editor/NoteEditorWidget.cpp src/app/MainWindow.cpp tests/editor/tst_note_editor_widget_mode_transition.cpp
git commit -m "feat(editor): Ln/Col statusbar live in all modes via base cursorPositionChanged

CursorPos is 1-based flat visual lines (contract v2). Reading mode now
reports a real caret position, so the 'Reading' label override is
retired. Word count remains Phase 2. Adoption brief §2 (line/col)."
```

---

## Task 9: Ephemeral state capture/restore + goToLine in all modes

**Files:**
- Modify: `src/editor/NoteEditorWidget.cpp` (`captureEphemeralStateFor` ~248–261, `restoreEphemeralStateFor` ~263–270, `goToLine` ~226–246, add `leafFor`), `src/editor/NoteEditorWidget.h` (declare `leafFor`, fix `goToLine` doc comment)
- Modify: `src/app/MainWindow.cpp` (template cursor-marker site ~2688–2692)
- Modify: `tests/editor/CMakeLists.txt` (re-enable `tst_note_editor_widget_ephemeral` — remove the `if(FALSE)`/`endif()` wrapper around it)
- Rewrite: `tests/editor/tst_note_editor_widget_ephemeral.cpp` (contract-v2 semantics)

**Semantics decision (write it in code comments too):** `EphemeralState.cursor` stores the contract's `CursorPos` verbatim — **1-based flat visual lines**, `0` = never captured (skip on restore). `EphemeralState.scroll` stores the contract's **scroll fraction 0.0–1.0** (`scrollPositionVisualLine()` despite its legacy name returns a fraction in v2). This deviates from the field's old "visual-line float" comment; the JSON shape (`cursor:{line,column}`, flat `scroll`) is Corbomite-internal, not Obsidian's `from/ch` shape, so interop fidelity is unaffected now and workspace.json fidelity is Phase 3's item. Update the comment in `libs/storage/include/corbomite/storage/EphemeralState.h:38` from `// visual-line float (±0.5 precision)` to `// scroll fraction 0.0–1.0 (MarkdownView::scrollPositionVisualLine, contract v2)`.

- [ ] **Step 1: Rewrite the ephemeral test file to contract-v2 semantics**

Replace the body of `tests/editor/tst_note_editor_widget_ephemeral.cpp` with:

```cpp
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Phase 1 (contract v2) — NoteEditorWidget ephemeral state round-trips
// through the MarkdownView base: cursor as 1-based flat-visual-line
// CursorPos, scroll as 0.0–1.0 fraction. Replaces the pre-port tests that
// drove the retired leaf-specific scrollPosition/cursorLine APIs.
// Runs under QT_QPA_PLATFORM=offscreen.

#include "NoteEditorWidget.h"
#include "corbomite/storage/EphemeralState.h"
#include "corbomite/core/NoteDocument.h"

#include <markoff/core/MarkdownView.h>

#include <QObject>
#include <QStringList>
#include <QTest>

using Corbomite::EphemeralState;
using Corbomite::NoteDocument;
using Corbomite::NoteEditorWidget;

namespace {
QString makeParagraphs(int count)
{
    QStringList blocks;
    blocks.reserve(count);
    for (int i = 0; i < count; ++i)
        blocks.append(QStringLiteral("Paragraph %1 line A.").arg(i));
    return blocks.join(QStringLiteral("\n\n"));
}
} // namespace

class NoteEditorWidgetEphemeralTest : public QObject {
    Q_OBJECT

    void roundTripInMode(NoteEditorWidget::ViewMode mode)
    {
        NoteEditorWidget widget;
        widget.resize(600, 240);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));
        widget.setViewMode(mode);

        NoteDocument doc(QStringLiteral("/tmp/vault"), QStringLiteral("note.md"));
        doc.setMarkdown(makeParagraphs(60));
        widget.setNoteDocument(&doc);

        auto *leaf = widget.activeLeaf();
        QVERIFY(leaf);
        leaf->setCursorPosition({7, 3});
        leaf->setScrollPositionVisualLine(0.5f);
        QTest::qWait(20);

        const EphemeralState saved = widget.saveEphemeralState();
        QCOMPARE(saved.cursor.line, 7);
        QVERIFY2(std::abs(saved.scroll - 0.5f) <= 0.1f,
                 qPrintable(QStringLiteral("scroll drift: %1").arg(saved.scroll)));

        leaf->setCursorPosition({1, 1});
        leaf->setScrollPositionVisualLine(0.0f);
        QTest::qWait(20);
        widget.restoreEphemeralState(saved);
        QTest::qWait(20);

        QCOMPARE(leaf->cursorPosition().line, 7);
        QVERIFY(leaf->scrollPositionVisualLine() > 0.3f);
    }

private Q_SLOTS:
    void sourceModeRoundTrip()  { roundTripInMode(NoteEditorWidget::ViewMode::Source); }
    void liveModeRoundTrip()    { roundTripInMode(NoteEditorWidget::ViewMode::LivePreview); }
    void readingModeRoundTrip() { roundTripInMode(NoteEditorWidget::ViewMode::Reading); }

    void cursorSurvivesModeSwitch()
    {
        NoteEditorWidget widget;
        widget.resize(600, 240);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));
        widget.setViewMode(NoteEditorWidget::ViewMode::Source);

        NoteDocument doc(QStringLiteral("/tmp/vault"), QStringLiteral("note.md"));
        doc.setMarkdown(makeParagraphs(60));
        widget.setNoteDocument(&doc);

        widget.activeLeaf()->setCursorPosition({9, 1});
        QTest::qWait(20);
        widget.setViewMode(NoteEditorWidget::ViewMode::Reading);
        QTest::qWait(20);

        // setViewMode captures the outgoing leaf's state and restores it on
        // the incoming leaf — line position carries across the mode switch.
        QCOMPARE(widget.activeLeaf()->cursorPosition().line, 9);
    }

    void goToLineAllModes()
    {
        const NoteEditorWidget::ViewMode modes[] = {
            NoteEditorWidget::ViewMode::Source,
            NoteEditorWidget::ViewMode::LivePreview,
            NoteEditorWidget::ViewMode::Reading};
        for (auto mode : modes) {
            NoteEditorWidget widget;
            widget.resize(600, 240);
            widget.show();
            QVERIFY(QTest::qWaitForWindowExposed(&widget));
            widget.setViewMode(mode);

            NoteDocument doc(QStringLiteral("/tmp/vault"), QStringLiteral("note.md"));
            doc.setMarkdown(makeParagraphs(60));
            widget.setNoteDocument(&doc);

            QVERIFY2(widget.goToLine(5),
                     "goToLine must succeed in every mode under contract v2");
            QCOMPARE(widget.activeLeaf()->cursorPosition().line, 5);
            QTest::qWait(10);
        }
    }
};

QTEST_MAIN(NoteEditorWidgetEphemeralTest)
#include "tst_note_editor_widget_ephemeral.moc"
```

In `tests/editor/CMakeLists.txt`, delete the `if(FALSE)` line above the `add_executable(tst_note_editor_widget_ephemeral …)` block and its matching `endif()` so the target builds again. Keep the existing `target_link_libraries`/`add_test`/`set_tests_properties` lines; drop any now-unneeded `Markoff::Source` link if it errors (the rewritten test only needs `CorbomiteApp` + `Qt6::Test` + `Qt6::Widgets` + `Markoff::Core` for `MarkdownView.h`).

- [ ] **Step 2: Run — expect FAIL** (capture/restore/goToLine are still stubs):

```bash
cmake --preset dev && cmake --build --preset dev -j 10 && cd build-dev && QT_QPA_PLATFORM=offscreen ctest -R tst_note_editor_widget_ephemeral --output-on-failure
```

- [ ] **Step 3: Implement `leafFor`, capture, restore, goToLine**

`NoteEditorWidget.h`, private section (above `activeLeaf` note):

```cpp
    // Leaf for a given mode (nullptr until lazily constructed). activeLeaf()
    // == leafFor(m_viewMode).
    Markoff::MarkdownView *leafFor(ViewMode mode) const;
```

`NoteEditorWidget.cpp` — implement and refactor `activeLeaf` through it:

```cpp
Markoff::MarkdownView *NoteEditorWidget::leafFor(ViewMode mode) const
{
    switch (mode) {
    case ViewMode::Source:      return m_sourceEditor;
    case ViewMode::LivePreview: return m_editor;
    case ViewMode::Reading:     return m_styledReadingView;
    }
    return nullptr;
}

Markoff::MarkdownView *NoteEditorWidget::activeLeaf() const
{
    return leafFor(m_viewMode);
}
```

Capture (replaces the stub; keep the `ViewModeSerializer` lines):

```cpp
EphemeralState NoteEditorWidget::captureEphemeralStateFor(ViewMode mode) const
{
    EphemeralState s;
    const auto compound = ViewModeSerializer::toCompound(mode);
    s.modeRaw = compound.mode;
    s.sourceFlag = compound.source;

    // Contract v2: CursorPos is 1-based flat visual lines (0 = unset);
    // scroll is the 0.0–1.0 fraction from scrollPositionVisualLine().
    if (Markoff::MarkdownView *leaf = leafFor(mode)) {
        const Markoff::CursorPos pos = leaf->cursorPosition();
        s.cursor.line = pos.line;
        s.cursor.column = pos.column;
        s.scroll = leaf->scrollPositionVisualLine();
    }
    return s;
}
```

Restore:

```cpp
void NoteEditorWidget::restoreEphemeralStateFor(ViewMode mode,
                                                 const EphemeralState &s)
{
    Markoff::MarkdownView *leaf = leafFor(mode);
    if (!leaf) return;
    if (s.cursor.line >= 1)
        leaf->setCursorPosition({s.cursor.line, std::max(1, s.cursor.column)});
    leaf->setScrollPositionVisualLine(std::clamp(s.scroll, 0.0f, 1.0f));
}
```

(Add `#include <algorithm>` if not present.)

goToLine (replaces the per-mode switch and its Live/Reading TODOs):

```cpp
bool NoteEditorWidget::goToLine(int line)
{
    if (line < 1) return false;
    Markoff::MarkdownView *leaf = activeLeaf();
    if (!leaf) return false;
    leaf->setCursorPosition({line, 1});
    return true;
}
```

Update the stale doc comment on `goToLine` in the header (~110–114) to:

```cpp
    /// Move the cursor to `line` (1-based flat visual line). Dispatches via
    /// the MarkdownView base; works in all three modes (Reading keeps a
    /// caret while read-only). Returns false only when no leaf exists yet.
    bool goToLine(int line);
```

- [ ] **Step 4: Wire the template cursor-marker site**

`src/app/MainWindow.cpp` ~2688–2692 — replace:

```cpp
    if (cursorIdx >= 0) {
        // TODO(port-foundation-exploration): goToLine retired on EditorWidget.
        const int line = finalBody.left(cursorIdx).count(QLatin1Char('\n'));
        (void)line;
    }
```

with:

```cpp
    if (cursorIdx >= 0) {
        const int line = finalBody.left(cursorIdx).count(QLatin1Char('\n'));
        editor->goToLine(line + 1);   // count('\n') is 0-based; CursorPos is 1-based
    }
```

- [ ] **Step 5: Update the `EphemeralState.h:38` scroll comment** (exact text in the semantics note above).

- [ ] **Step 6: Run all tests — green; commit**

```bash
cmake --build --preset dev -j 10 && cd build-dev && QT_QPA_PLATFORM=offscreen ctest -E benchmark --output-on-failure -j 10 2>&1 | tail -3
cd .. && git add src/editor/NoteEditorWidget.cpp src/editor/NoteEditorWidget.h src/app/MainWindow.cpp tests/editor/tst_note_editor_widget_ephemeral.cpp tests/editor/CMakeLists.txt libs/storage/include/corbomite/storage/EphemeralState.h
git commit -m "feat(editor): ephemeral cursor/scroll capture-restore + goToLine, all modes

Implemented over the contract-v2 base: CursorPos (1-based flat visual
lines) and the 0.0-1.0 scroll fraction. EphemeralState.scroll now
stores the fraction (comment updated; Corbomite-internal JSON shape,
Phase 3 owns workspace.json fidelity). Template cursor-marker goToLine
re-wired. tst_note_editor_widget_ephemeral re-enabled, rewritten to v2
semantics. Adoption brief §2 (ephemeral/goToLine)."
```

---

## Task 10: Zoom via the base + purge leaf-typed code from MainWindow

**Files:**
- Modify: `src/app/MainWindow.cpp` (`onZoomIn`/`onZoomOut`/`onZoomReset` ~705–724; delete `liveActionControllerFor` ~209–217; includes ~8, 65–67)

- [ ] **Step 1: Re-route zoom through `setFontScale`**

Replace the three handlers (keep the explanatory comment block above them about Markoff's own window-level shortcuts — it is still true for the Live leaf):

```cpp
namespace { // alongside the other file-local helpers
// Matches Markoff::Live::kFontScaleStep so menu zoom and the Live leaf's
// own Ctrl+=/Ctrl+- shortcuts step identically.
constexpr qreal kZoomStep = 1.10;
} // namespace

void MainWindow::onZoomIn()
{
    auto *editor = activeEditor();
    if (!editor) return;
    if (auto *leaf = editor->activeLeaf())
        leaf->setFontScale(leaf->fontScale() * kZoomStep);   // base clamps to [0.25, 4.0]
}

void MainWindow::onZoomOut()
{
    auto *editor = activeEditor();
    if (!editor) return;
    if (auto *leaf = editor->activeLeaf())
        leaf->setFontScale(leaf->fontScale() / kZoomStep);
}

void MainWindow::onZoomReset()
{
    auto *editor = activeEditor();
    if (!editor) return;
    if (auto *leaf = editor->activeLeaf())
        leaf->setFontScale(1.0);
}
```

Also update the comment at ~698–704: zoom is no longer "No-op in Source/Reading" — all three leaves honor `setFontScale` under contract v2.

- [ ] **Step 2: Delete `liveActionControllerFor`**

Remove the helper (anonymous namespace, ~lines 209–217). All seven former call sites were replaced in Tasks 4, 6, and this task. Compile errors here mean a call site was missed — replace it with base dispatch, do not resurrect the helper.

- [ ] **Step 3: Purge leaf includes and verify leaf-agnosticism**

Remove from `MainWindow.cpp`: `#include <markoff/live/EditorWidget.h>`, `#include <markoff/live/LiveActionController.h>`, `#include <markoff/live/LiveListModelBinding.h>`, and `#include <markoff/source/Editor.h>`. Then verify:

```bash
grep -n "markoff/live\|markoff/source\|markoff/styled\|Markoff::Live\|Markoff::Source\|Markoff::Styled" src/app/MainWindow.cpp src/app/MainWindow.h
```

Expected: **zero hits** (comment-only mentions: rewrite or delete the comment). Any code hit must be migrated to the `Markoff::MarkdownView` base — this is the leaf-agnosticism gate (user directive: the canonical live view may change leaf class later).

- [ ] **Step 4: Build + suite, commit**

```bash
cmake --build --preset dev -j 10 && cd build-dev && QT_QPA_PLATFORM=offscreen ctest -E benchmark --output-on-failure -j 10 2>&1 | tail -3
cd .. && git add src/app/MainWindow.cpp
git commit -m "refactor(app): zoom via MarkdownView::setFontScale; MainWindow now leaf-agnostic

Zoom works in Source and Reading for the first time (was Live-only via
LiveActionController). liveActionControllerFor deleted — MainWindow no
longer includes or names any concrete Markoff leaf type, so the
canonical live view can change leaf class without touching this file."
```

- [ ] **Step 5: Mark the remaining leaf-specific sites in NoteEditorWidget**

In `src/editor/NoteEditorWidget.cpp`, add a `// leaf-specific:` comment marker at each permitted leaf-typed touchpoint (per the design principle): the `m_editor` construction + `binding()->setLinkService` in the ctor, the `m_sourceEditor` and `m_styledReadingView` constructions + `setLinkService` in `ensureWidgetConstructed`, and the mermaid-renderer injection site(s) in `setMermaidRenderer`. One-line comments, e.g.:

```cpp
    // leaf-specific: Live QML binding wiring — revisit if the canonical
    // live view changes leaf class.
```

```bash
git add src/editor/NoteEditorWidget.cpp
git commit -m "docs(editor): mark permitted leaf-specific touchpoints for future canonical-view swap"
```

---

## Task 11: Free-rider verification + full gate

**Files:** none (verification only)

- [ ] **Step 1: Full offscreen suite**

```bash
cd /home/clinton/dev/Corbomite/build-dev && QT_QPA_PLATFORM=offscreen ctest -E benchmark --output-on-failure -j 10 2>&1 | tail -6
```

Expected: **100% pass** (excluding the by-design `benchmark` label). Specifically `tst_metadataparser` green. Record the new pass/total count for the docs task.

- [ ] **Step 2: Launch + manual smoke (best-effort; anything not verifiable headlessly goes to the user's dogfood list)**

```bash
./bin/Corbomite
```

Checklist (free riders + adopted surfaces) — verify in a **copy** of a test vault, never the live `testvaults/` evidence files:

1. **Styled tables in Reading** — open a note with a pipe table, switch to Reading: renders as a real grid (was raw text at the old pin).
2. **Find in Reading** — Ctrl+F finds and navigates; matches inside tables are *counted but not highlighted* (expected v1 limitation, brief §3 — not a bug).
3. **Undo in Source** — type, undo via Edit menu: content reverts (now through D2, not the Qt widget stack).
4. **Theme switch** — change theme in settings: all three modes follow.
5. **Ln/Col** — moves in all three modes, including Reading.
6. **Heading radio** — caret in `## heading` checks H2; caret in a paragraph clears all.
7. **Format verbs disabled in Reading** (greyed out), enabled in Live/Source.
8. **Zoom in Source/Reading** — View menu zoom now works there (was Live-only); **source find-highlight drift** — find in Source, verify highlights sit on the matches (upstream fix free-rider).

---

## Task 12: Docs closeout, registry updates, push

Follow the "phase done" ritual in `docs/CONTRIBUTING-OPS.md`. All edits below cite what Tasks 1–11 actually delivered — adjust statuses to match reality if anything was deferred.

**Files:**
- Modify: `docs/PARITY-MATRIX.md` (rows at current lines 69, 74, 75, 76, 79, 80, 81, 85)
- Modify: `docs/punch-list.md` (mark the matching items `[x]` — find them by topic: Reading find, undo divergence, theme propagation, format-verb dual dispatch, Ln/Col, goToLine/ephemeral, styled tables, zoom/font scale)
- Modify: `docs/superpowers/plans/2026-06-10-road-to-dogfood.md` (Phase 1 status banner)
- Modify: `docs/PROJECT-STATE.md` (§Current focus — ≤3 sentences, no "Previously:" cascade)
- Modify: `docs/decisions-archive.md` (append dated H2 closeout paragraph)
- Modify: `CLAUDE.md` (state-of-world banner: next workfront → Phase 2; test baseline count; the "lone red tst_metadataparser" note is obsolete)

- [ ] **Step 1: PARITY-MATRIX rows** — update the eight editor rows to their new truth, with evidence pointers (file:line of the new code), e.g.: Find in note → `✅ all modes (in-table matches counted, not painted — Markoff brief §3)`; Undo/redo → `✅ via MarkdownView base → undoD2`; Theme propagation → `✅ applyThemeToAllLeaves + wireLeaf`; Format verbs → `✅ base dispatch; enabled-state via hasEditing()`; Ln/Col → `✅ all modes (word count still ⭕ — Phase 2)`; goToLine/ephemeral → `✅ contract-v2 CursorPos/scroll-fraction`; Reading tables → `✅ post-pin`; Zoom → `✅ all leaves via setFontScale`.

- [ ] **Step 2: punch-list** — mark the corresponding items `[x]` (do not delete). Add one new tracked item: *"workspace.json eState scroll stores a 0.0–1.0 fraction (contract v2), not a visual line — revisit in Phase 3 workspace-fidelity if Obsidian interop of this field matters"* (P4 or wherever Phase 3 items live).

- [ ] **Step 3: road-to-dogfood** — add a status line under the Phase 1 heading mirroring Phase 0's pattern: `**Status: COMPLETE (date, master)** — re-pin at <REPIN_SHA>; brief §2 fully consumed; <N>/<N> offscreen.` Note the one roadmap deviation: the `![[…]]` embed fix was NOT in Markoff master at planning time — it was implemented and landed upstream as part of this phase (Task 1).

- [ ] **Step 4: PROJECT-STATE + decisions-archive + CLAUDE.md** — per the rituals: 3 sentences max in PROJECT-STATE pointing the current focus at Phase 2 (completion revival first); the full closeout paragraph (what landed, the scroll-fraction decision, the leaf-agnosticism gate now enforced in MainWindow) goes in `decisions-archive.md` under a new dated H2. Update CLAUDE.md's banner ("Next major workfront") and the Testing section's baseline counts.

- [ ] **Step 5: Commit docs and push both repos**

```bash
cd /home/clinton/dev/Corbomite
git add docs/PARITY-MATRIX.md docs/punch-list.md docs/superpowers/plans/2026-06-10-road-to-dogfood.md docs/PROJECT-STATE.md docs/decisions-archive.md CLAUDE.md
git commit -m "docs: Phase 1 closeout — re-pin + contract-v2 adoption complete; matrix/punch-list/state updated"
git push origin master
# Markoff was already pushed in Task 1 — verify:
cd /home/clinton/dev/Markoff && git status -sb | head -1
```

Expected: Corbomite push succeeds; Markoff shows `## master...origin/master` (no ahead/behind). **Reminder: at no point in this phase should `git add -A` have been used in Corbomite — final check that `testvaults/` modifications are still uncommitted:** `git -C /home/clinton/dev/Corbomite status --short testvaults/ | head -3` must still show the `M`/`??` entries.

---

## Self-review notes (already applied)

- **Roadmap deviation handled:** road-to-dogfood Phase 1 step 1 says "verify the embed fix is in the pin" — verified NOT in `ddf5e9a8..1f9ebdc9` (zero parser commits in range); Task 1 lands it upstream first, so the re-pin target supersedes `1f9ebdc9`.
- **Brief's §2 items mapped to tasks:** find-attach → T3; undo/redo → T4; theme → T5; format verbs + helper deletion → T6; contextChanged → T7; ephemeral + goToLine + Ln/Col → T8/T9; Reading read-only honesty → free (upstream) + verified T11. §4 non-goals respected (no session unification, no word count, no cursor-restore-after-undo chasing, no in-table highlight).
- **Known-and-accepted upstream limitations (do not file as bugs):** contextChanged staleness window (Markoff queue #15); find-highlight color hardcoded yellow (queue #14); in-table matches counted-not-painted.
- **`Markoff::CursorPos`** is used in Tasks 8–9; it is the `{int line; int column;}` 1-based struct from `<markoff/core/MarkdownView.h>` (already included where used).
