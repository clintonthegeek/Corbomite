#!/usr/bin/env python3
"""Central linkage for the 2026-10-03 audit-the-audit batch.

The 2026-10-03 verification pass ran one agent per domain. The first few wrote
their own linkage bullets; the later ones were told not to, because they were
running concurrently and would clobber `addenda/README.md` and
`00-taxonomy.md` (both are single shared files). This script does the linkage in
one place instead.

    python3 link-2026-10-03.py <repo-root>

Appends:
  * a `### 2026-10-03 - <domain> domain` section to addenda/README.md, inserted
    above the 2026-06-10 batch so newest stays on top
  * one bullet per addendum to the `## Addenda` section of 00-taxonomy.md

Idempotent: skips any domain already present.
"""
import os
import re
import sys

root = sys.argv[1] if len(sys.argv) > 1 else "."
addenda = os.path.join(root, "docs/obsidian-audit/addenda")

# domain -> (README section heading, [(slug, one-line summary)])
BATCH = [
    ("editor", "editor domain, verified against the restored 1.13.7 corpus", [
        ("editor-wrapper-commands-and-keymaps",
         "**`Editor` is half-documented.** The public export binds the *mixin*; the concrete "
         "`EditorView`-owning subclass (`app/own/1823-RJ.js:885`) adds **~45 undocumented methods** — the "
         "7-entry format-markup table, a ~48-binding base keymap, undo-history snapshotting, fold-restore "
         "line-count gate, clipboard hooks. §14's sizing basis is off ~2x. `exec` is a fixed **18-key** "
         "table that **throws** on unknown names. `insertCallout()` selects `NOTE` in both branches. "
         "Suggesters are link / tag / **footnote** (no headers suggester). **Audit-wide: `debounce` (`Ml`, "
         "`app/own/0355-Ml.js:22-27`) is a leading-edge throttle, not a trailing debounce** — repeat calls "
         "inside the window do not reset the timer, so every debounce/timing claim needs re-reading."),
    ]),
    ("editor-markdown", "editor-markdown domain, verified against the restored 1.13.7 corpus", [
        ("editor-markdown-view-modes-and-preview-pipeline",
         "**The async preview worker is single-flight and contention degrades to a blocking main-thread "
         "parse** (`app/own/1500-wW.js:11-13` returns `null` when in flight; `parseAsync` doesn't check, so "
         "the `catch` runs `parseSync()`). `TW` is module-level, so contention is app-wide — a Corbomite "
         "thread-pool-with-queue would diverge. **`commonmark: true` is hard-coded with no user escape "
         "hatch**; `breaks` is a process-global singleton. 8 corrections (save throttle, Ctrl+Wheel, "
         "double-tap, sortOrder, rAF-vs-timeout) + full citation remap: every `MarkdownView.js:*` style "
         "reference in the frozen doc is dead."),
    ]),
    ("views", "views domain, verified against the restored 1.13.7 corpus", [
        ("views-leaf-viewstate-and-deferred-restore",
         "**There is no three-way merge** (`app/own/1810-bZ.js:2-8`): `diff_main(base, view)` -> "
         "`patch_make` -> `patch_apply(patches, disk)[0]`, with per-patch success discarded. Demonstrated "
         "by execution against the extracted code: a mid-paragraph user edit is **discarded and its text "
         "relocated onto the preceding line**, with only a notice. Refutes §1/§12's \"does not lose "
         "in-progress edits\". Also: a **second registry `app.embedRegistry`** exists besides `viewRegistry`, "
         "and `_empty` is a per-leaf singleton, not global."),
    ]),
    ("search", "search domain, verified against the restored 1.13.7 corpus", [
        ("search-dsl-grammar-and-quick-switcher-scoring",
         "**The DSL grammar is in-tree and readable** (`app/own/1175-SP.js:148-221`, a ~60-line operator "
         "table) — refuting §13 Q1's \"out-of-tree, reverse-engineer from documentation\". Real flags are "
         "`match-case` + **`ignore-case`**; `whole-word` **does not exist**. Missing from the doc: `content:`, "
         "`task:`, `task-todo:`, `task-done:`, `[key:value]`, `TRUE`/`FALSE`/`EMPTY`, `>`/`<`. "
         "**`PARITY-MATRIX.md` §5 overclaims**: quick-switcher fuzzy constants match but the *penalty rule* "
         "does not (two worked examples)."),
    ]),
    ("settings", "settings domain, verified against the restored 1.13.7 corpus", [
        ("settings-config-file-ownership-and-definitions",
         "**Definitive tab -> config-key -> on-disk-file table.** §3's table is wrong in 3 of 7 rows: **4 keys "
         "it names do not exist** (proven exhaustively — 55 unique `getConfig`/`setConfig` keys, all keys of "
         "the defaults table `ME` at `app/own/0920-eE.js:1174-1237`), and **17 real keys are missing**. "
         "**`.obsidian/<corePluginId>.json` is a whole file class the doc omits** — core-plugin data goes "
         "through `readConfigJson(pluginId)`, *not* `plugins/<id>/data.json`. Encodings that corrupt a vault "
         "if missed: `openBehavior` is `\"file:\" + path`, not an enum; the three font keys are comma-joined "
         "**strings**. `display()` is no longer the contract (declarative `getSettingDefinitions()`), so a "
         "`display()`-only tab is **invisible to global settings search**. Safe mode is a `localStorage` flag, "
         "not absence of `community-plugins.json`. Cross-checked and **confirmed** the 16-key "
         "`app.json`/`appearance.json` partition."),
    ]),
    ("plugin", "plugin domain — capability surface only; the JS API is NOT mirrored by design", [
        ("plugin-loader-lifecycle-and-capability-surface",
         "**Corbomite deliberately does not mirror this domain's API** — a C++ plugin is not a web plugin. "
         "What *is* transferable is documented as a 15-item capability surface. Load-bearing facts: **there is "
         "no permission model at all** (zero permission hits; the only gate is `Ab.plugins`); enable-set order "
         "*is* load order; `minAppVersion` is a real install gate; a throw from `onload` **leaks a half-loaded "
         "instance that is never retried**; `loadCSS()` is not opt-in (§12 is backwards); `addCommand` "
         "**silently overwrites** on duplicate id; `saveData` **cannot report a failed write**. §10's "
         "enumeration is complete (26 members). 8 corrections + 17 catalogued defects. Web-delivery surface "
         "(`require` shim, `window.eval`, inline-sourcemap stripping, hot-reload) is explicitly marked "
         "**not applicable by design**."),
    ]),
    ("core", "core domain, verified against the restored 1.13.7 corpus", [
        ("core-command-and-hotkey-contracts",
         "**`.obsidian/hotkeys.json` is override-only** (`app/own/0833-$w.js:57-76`): `bake()` walks custom "
         "keys first and falls back to defaults only when `!hasOwnProperty` — so `\"some:command\": []` "
         "**unbinds** the command, the only way Obsidian expresses an unbind. **115 command ids, not \"30+\"**; "
         "implementing only the doc's 25 silently drops every `editor:*` override in a real vault. `Modal` "
         "allocates a **parentless** `Scope`, so the root scope and its hotkeys are unreachable while a plain "
         "modal is open. `Events.off` deletes *all* listeners on a no-match. 21 NEW + 8 corrections; §13 "
         "Q1-Q6 answered, Q7 refuted."),
    ]),
    ("ui-bundle", "ui-bundle domain, verified against the restored 1.13.7 corpus", [
        ("ui-bundle-widgets-and-overlays-source-proof",
         "**The menu section protocol is a hierarchical `a.b.c` namespace** (`app/own/0751-my.js:62-81`): "
         "separators key on the id's *first dot-segment*, and `submenuConfigs` matching is **prefix-based**, so "
         "`info` and `info.copy` render contiguously with no separator — the doc's flat list cannot produce "
         "this. **The shipped Lucide table has 1408 entries, not 125** and is *generated* by `getIconIds()`, "
         "not hand-listed. `PopoverState` is a **string enum with reversed member order** (so §2, §8's "
         "`Hidden === 0` and §13 Q1 are all wrong); `SliderComponent.setDynamicTooltip`/`showTooltip` are "
         "empty stubs; `Modal.open` listens for `pagehide`, not `beforeunload`; `openSubmenuSoon` is a "
         "leading-edge throttle. **Web→desktop UI translation is intended, so DOM-widget choices are not "
         "reported as gaps** — only observable-behaviour and data-contract differences."),
    ]),
    ("leaf-utilities", "leaf-utilities domain, verified against the restored 1.13.7 corpus", [
        ("leaf-utilities-subpath-rename-and-leaf-bundle-corrections",
         "**Heading rename matches with `stripHeading` but writes with `stripHeadingForLink`** — "
         "asymmetrically, in adjacent lines (`app/own/1312-hR.js:205-206`). The match side strips `* ! [ ] % ` "
         "+ backtick that the write side **keeps**, so matching is permanently looser than the bytes written. "
         "The ref set is `frontmatterLinks ∪ links ∪ embeds`, so **renames rewrite YAML property values**, and "
         "the applier **re-dumps the whole frontmatter block through js-yaml** — silently keeping body edits but "
         "dropping frontmatter edits if the YAML won't parse, and silently no-opping on dotted keys. "
         "Block-id rename uses a third rule entirely. `SecretStorage` **extends `Events`** and fires "
         "`changed` 3x (refuting §4); `Scope.handleKey` lets a specific binding returning `undefined` "
         "**swallow** the event; `requestUrl` **drops `opts.headers`** on the web path."),
    ]),
]

README = os.path.join(addenda, "README.md")
TAXONOMY = os.path.join(root, "docs/obsidian-audit/00-taxonomy.md")

readme = open(README, encoding="utf-8").read()
tax = open(TAXONOMY, encoding="utf-8").read()

added_domains, added_bullets = [], 0
for domain, heading, entries in BATCH:
    marker = f"### 2026-10-03 — {domain} domain"
    if marker in readme:
        print(f"  skip {domain} (already linked)")
        continue
    missing = [s for s, _ in entries if not os.path.exists(os.path.join(addenda, f"2026-10-03-{s}.md"))]
    if missing:
        print(f"  WARN {domain}: missing file(s) {missing}")
        continue

    # README section, inserted above the 2026-06-10 batch (newest stays on top)
    section = [f"{marker}, verified against the restored 1.13.7 corpus", ""]
    for slug, summary in entries:
        section.append(
            f"- [`2026-10-03-{slug}.md`](2026-10-03-{slug}.md) — {summary}"
        )
    section.append("")
    anchor = "### 2026-06-10 batch"
    assert anchor in readme, "README anchor missing"
    readme = readme.replace(anchor, "\n".join(section) + "\n" + anchor, 1)

    # taxonomy bullets, newest at the top of the append-only list
    anchor2 = "## Addenda\n"
    assert anchor2 in tax, "taxonomy Addenda section missing"
    head, sep, tail = tax.partition(anchor2)
    lines = tail.split("\n")
    # skip the blockquote preamble and the first blank line
    insert_at = 0
    for i, ln in enumerate(lines):
        if ln.startswith("- ["):
            insert_at = i
            break
    bullets = []
    for slug, summary in entries:
        label = summary.split(".")[0].replace("**", "")
        bullets.append(
            f"- [2026-10-03 — **CORRECTION + NEW ({domain}):** {summary}]"
            f"(addenda/2026-10-03-{slug}.md)"
        )
        added_bullets += 1
    lines = lines[:insert_at] + bullets + lines[insert_at:]
    tax = head + sep + "\n".join(lines)

    added_domains.append(domain)

open(README, "w", encoding="utf-8").write(readme)
open(TAXONOMY, "w", encoding="utf-8").write(tax)

print(f"\nlinked {len(added_domains)} domains ({added_bullets} taxonomy bullets): {', '.join(added_domains)}")