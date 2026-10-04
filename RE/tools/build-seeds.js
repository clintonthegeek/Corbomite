#!/usr/bin/env node
// Generate a per-domain "starting map" for the audit-the-audit pass.
//
//   node build-seeds.js <version-dir> <out-dir>
//
// The previous audit's failure was partly navigational: 15 domain docs covering
// ~660KB of prose with no way to get from a claim to the code. A reader who has
// to grep 7MB of source to find the one function a doc section describes will
// either give up or guess.
//
// So this produces, per domain, the evidence a reader should start from:
//   * named classes whose names or fingerprints belong to the domain
//   * method definitions whose names match the domain's vocabulary
//   * string literals that are the domain's on-disk / on-wire contract
//   * the `app.*`/`bd.*` API paths in that area
//
// Everything here is derived from the index, so it cannot assert anything the
// source does not contain. It is a table of contents for a reader, not a finding.
const fs = require("fs");
const path = require("path");
const Database = require("better-sqlite3");

const [versionDir, outDir] = process.argv.slice(2);
if (!versionDir || !outDir) {
  console.error("usage: build-seeds.js <version-dir> <out-dir>");
  process.exit(2);
}

const dbPath = path.join(versionDir, "renamed", "index.sqlite");
const db = new Database(dbPath, { readonly: true });
const renames = JSON.parse(fs.readFileSync(path.join(versionDir, "renames.json"), "utf8"));

// domain -> { doc, keywords (method/class names), literals (exact strings that
//             are the on-disk contract), apiPaths, hint }
const DOMAINS = {
  vault: {
    doc: "vault.md",
    topic: /vault|adapter|file|folder|path|trash|attachment|rename|ignore/,
    hint: "Vault/TFile/TFolder model, path naming, adapter, trash, file manager, config dir, attachment paths, sync/ignore rules.",
    methods: /^(get|create|createBinary|createFolder|append|appendBinary|cachedRead|cachedReadBinary|read|write|delete|trash|rename|copy|modify|stat|exists|list|process|adapter|normalizePath|sanitize|getAvailablePath|getAvailablePathForAttachments|getAllLoadedFiles|getAllFolders|getConfig|setConfig|recycleName|recycle|getAbstractFileByPath|getFileByPath|getFolderByPath|getRoot|getName|getAbstractFileByRelPath|createBinary|open|getAvailablePathForAttachments|copyImage|link|generateMarkdownLink|resolveRelativePath|parseLinktext|getNewFileParent|recycle|trashSystem|getResourcePath|openLinkText)$/,
    literals: ["workspace.json", "app.json", "hotkeys.json", "graph.json", ".obsidian", "trash", "/.trash", ".obsidian/config", ".obsidian/plugins", ".obsidian/themes", ".obsidian/snippets", "attachmentFolderPath", "newFileLocation", "newFileFolderPath", "alwaysUpdateLinks", "useMarkdownLinks", "showUnsupportedFiles", "trashOption", "userIgnoreFilters"],
    api: ["app.vault", "app.fileManager", "app.adapter"],
  },
  workspace: {
    doc: "workspace.md",
    topic: /workspace|leaf|split|tab|layout|window|sidebar|ribbon|drawer/,
    hint: "Workspace tree, leaves, splits, tabs, serialization to workspace.json, layout migration, active-leaf tracking, window nodes.",
    methods: /^(serialize|deserialize|getLeavesOfType|getLeaf|createLeafBySplit|createLeafByTab|getActiveViewOfType|getActiveFile|iterateAllLeaves|iterateRootLeaves|getMostRecentLeaf|setActiveLeaf|getLastOpenFiles|setLastOpenFiles|detachLeavesOfType|getLeftLeaf|getRightLeaf|getRoot|revealLeaf|duplicateLeaf|splitActiveLeaf|collapseLeftSidebar|toggleLeftSidebar|onLayoutReady|requestSaveLayout|updateOptions|getGroup|addToHistory|back|forward|canGoBack|canGoForward)$/,
    literals: ["workspace.json", "left-ribbon", "right-ribbon", "left-sidebar", "right-sidebar", "main", "split", "tab", "floating", "pinned", "currentTab", "lastOpenFiles", "main-1", "main-2", "window", "collapse", "expand"],
    api: ["app.workspace"],
  },
  metadata: {
    doc: "metadata.md",
    topic: /metadata|cache|link|resolved|heading|section|tag|block|footnote/,
    hint: "MetadataCache, CachedMetadata, resolvedLinks/unresolvedLinks, link resolution, heading/tag/section caches, IndexedDB persistence.",
    methods: /^(getFileCache|getCache|getFirstLinkpathDest|getLinkpath|fileToLinktext|getEphemeralState|setEphemeralState)$/,
    literals: ["-cache", "frontmatterPos", "frontmatterPosition", "unresolvedLinks", "resolvedLinks", "getFileCache", "duration", "mtime", "ctime", "size"],
    api: ["app.metadataCache", "app.metadataCache.resolvedLinks", "app.metadataCache.unresolvedLinks"],
  },
  parsing: {
    doc: "parsing.md",
    topic: /parse|linktext|frontmatter|yaml|footnote|blockquote|listitem|tag|embed/,
    hint: "Markdown/frontmatter/YAML parsing, wikilink + embed linktext parsing, block IDs, footnotes, tags, headings, position/offset semantics (UTF-16 units).",
    methods: /^(parse|parseFrontMatter|parseFrontmatter|parseYaml|parseLinktext|parseLinktextTo|resolveRelativePath|expandText|findLink|findHeading|findTag|getFrontMatterInfo|iterateCodeLines|parseEmbedded|getLinkpath)/,
    literals: ["frontmatter", "frontmatterPos", "^\\d+", "^[a-f0-9]{6}", "callout", "> [!", "```", "[[", "![[", "](", "footnote", "^\\[\\^", "^\\[\\*\\]", "\\t", "  ", "- [ ]", "- [x]"],
    api: ["app.metadataCache.getFileCache"],
  },
  rendering: {
    doc: "rendering.md",
    topic: /render|markdown|embed|mermaid|math|highlight|sanitiz|purify/,
    hint: "Markdown rendering pipeline, live preview vs reading view, embeds, math, mermaid, syntax highlighting, export.",
    methods: /^(render|renderMarkdown|rerender|rerenderFull|setViewData|getViewData|clear|getMode|setMode|update|onloadMarkdown|onunloadMarkdown|sanitize|DOMPurify)/,
    literals: ["--font-mermaid", "mermaid.initialize", "math", "application/x-markdown", "text/markdown", "mod-reading", "is-live-preview", "mathjax", "MathJax"],
    api: ["app.renderer"],
  },
  editor: {
    doc: "editor.md",
    topic: /editor|cursor|selection|history|vim|find|replace|indent|doc/,
    hint: "CodeMirror 6 editing, expandText/live-preview, selection, history/undo, Vim, table editor, find/replace, drag-and-drop, footnote autocomplete.",
    methods: /^(expandText|getValue|setValue|getSelection|replaceSelection|getCursor|listSelections|somethingSelected|transaction|dispatch|open|close|redo|undo|canUndo|canRedo|history|addSelectionListener|setBookmark|toggleComment|openSearch|query|replaceAll)/,
    literals: ["expandText", "onFileMenu", "onEditorMenu", "cm-editor", "is-live-preview", "selectall", "copyAsMarkdown", "toggleFold", "foldGutter"],
    api: ["app.editor"],
  },
  "editor-markdown": {
    doc: "editor-markdown.md",
    topic: /markdown|indent|wrap|inline|list|quote|pairing|newline/,
    hint: "Markdown-syntax semantics in the editor: inline styles, list continuation, block transforms, indentation, trailing whitespace, typing handlers.",
    methods: /^(handleDOMEvents|handleKeyDown|handlePaste|handleDrop|insertNewlineAndIndent|deleteCharBackward|indentSelection|indentLine|smartSelect|toMarkdown|setValue|applyMarkdown|markdown)/,
    literals: ["**", "__", "~~", "indentWithTabs", "strictLineBreaks", "smartIndent", "autoPairBrackets", "autoFormat", "readableLineLength"],
    api: [],
  },
  views: {
    doc: "views.md",
    topic: /view|leaf|pane|state|ephemeral|register|defer/,
    hint: "View registry, ItemView/FileView/ItemView subclasses, leaf-to-view binding, state serialization per view, deferred leaves.",
    methods: /^(getViewType|getDisplayText|getIcon|getState|setState|getViewState|setViewState|onOpen|onClose|getEphemeralState|setEphemeralState|registerView|registerExtensions|getViewByType|addAction|onPaneMenu|getParent|navigate|loadIfDeferred|requiresDefer|onResize|getData)/,
    literals: ["getViewState", "setViewState", "eState", "pinned", "group", "state", "active", "preview", "file", "currentTab"],
    api: ["app.workspace.getLeaf", "app.workspace.getLeavesOfType"],
  },
  search: {
    doc: "search.md",
    topic: /search|query|suggest|filter|sort|score|fuzzy/,
    hint: "Search query DSL: operators, filters (path/tag/file/property), sort, contextual search, quick switcher scoring.",
    methods: /^(search|prepareQuery|executeQuery|getSuggestions|suggest|setQuery|clear|parseSearchQuery|parseDQL|buildSearchQuery)/,
    literals: ["path:", "tag:", "file:", "line:", "section:", "block:", "task:", "property:", "-", "OR", "AND", "sort", "match-case", "explain"],
    api: ["app.metadataCache.resolvedLinks"],
  },
  settings: {
    doc: "settings.md",
    topic: /setting|toggle|dropdown|slider|component|search/,
    hint: "Settings UI framework: Setting/SettingGroup/Setting components, setting tabs, conditional visibility, search over settings, per-scope settings.",
    methods: /^(addSettingTab|removeSettingTab|getSettings|load|save|setValue|getValue|then|setDisabled|setHeading|setTooltip|addSearch|addExtraButton|setClass|clear|registerAndGetSettingTabForScope|open|hide|search|applying)/,
    literals: ["addSearch", "addExtraButton", "setting-item", "setHeading", "setDisabled", "getSettings", "addSettingTab", "setValue", "getValue"],
    api: ["bd.setting"],
  },
  bases: {
    doc: "bases.md",
    topic: /base|formula|propert|column|filter|sort|summary|card/,
    hint: "Bases: .base file schema, formula DSL, table/cards views, properties, summararies, filter/sort expressions.",
    methods: /^(bases|evaluateFormula|computeColumns|getViews|getCards|getTable|getRows|getProperties|setValue|onSelect|importFromMarkdown|exportToMarkdown|normalizeProperty|applySummaries|unrecognizedData)/,
    literals: ["filters", "formulas", "properties", "views", "summaries", "cards", "table", "unrecognizedData", "views:", "display:"],
    api: ["bd.plugins.bases"],
  },
  canvas: {
    doc: "canvas.md",
    topic: /canvas|node|edge|group|zoom|bbox|resize|bezier/,
    hint: "Canvas: .canvas JSON round-trip, nodes/edges/groups, text nodes, file/link nodes, export, editing, connection semantics.",
    methods: /^(getData|setData|addNode|addEdge|addGroup|delete|select|zoomTo|zoomToSelection|zoomToFit|export|create|update|getBounds|nudge|convertToMarkdown|createData|readNode|writeNode|getEdgesForNode|patchNode)/,
    literals: ["nodes", "edges", "canvas", "color", "background", "backgroundStyle", "label", "file", "url", "subpath", "export", "fit-content"],
    api: ["bd.plugins.canvas"],
  },
  plugin: {
    doc: "plugin.md",
    topic: /plugin|manifest|command|ribbon|statusbar|protocol|scope/,
    hint: "Extension surfaces. NOTE: Corbomite deliberately does NOT mirror this — see the doctrine note below. Useful only for the manifest/permissions shape and for understanding what Obsidian's own plugins rely on.",
    methods: /^(onload|onunload|loadData|saveData|addCommand|addRibbonIcon|addStatusBarItem|addSettingTab|registerView|registerEvent|registerDomEvent|registerInterval|registerEditorExtension|registerMarkdownPostProcessor|registerObsidianProtocolHandler|addChild|removeChild|register)/,
    literals: ["id", "name", "version", "minAppVersion", "description", "author", "isDesktopOnly", "manifest.json", "main.js", "styles.css", "data.json"],
    api: ["app.plugins", "app.internalPlugins", "app.plugins.getPlugin"],
  },
  core: {
    doc: "core.md",
    topic: /app|layout|menu|window|hotkey|keymap|theme|notice|modal|lifecycle/,
    hint: "App lifecycle, commands, hotkeys, themes/CSS, dialogs, notices, window/menu, keymap handling, i18n, mobile gating.",
    methods: /^(executeCommand|addCommand|register|onload|hotkey|keymap|save|load|apply|registerTheme|setTheme|customCss|addStyle|notice|open|close|onOpen|onClose|trigger|off|on)/,
    literals: ["hotkeys.json", "app.css", "themes", "snippets", "window", "workspace", "app-container", "notice"],
    api: ["app.commands", "app.keymap", "app.scope", "app.customCss", "app.loadLocalStorage", "app.saveLocalStorage"],
  },
  "ui-bundle": {
    doc: "ui-bundle.md",
    topic: /modal|menu|popover|suggest|tooltip|drag|createDiv|createEl|clickable/,
    hint: "Widget/DOM helpers, modals, menus, popovers, suggesters, tooltips, icons, drag-and-drop.",
    methods: /^(createDiv|createEl|createSpan|addClass|removeClass|toggleClass|hasClass|setText|setAttr|empty|addChild|remove|detach|show|hide|toggle|open|close|setPosition|onClick|onChange|registerDomEvent|registerInterval)/,
    literals: ["modal", "menu", "suggestion", "tooltip", "prompt", "popover", "modal-container", "suggestion-container", "clickable-icon", "view-header"],
    api: [],
  },
  "leaf-utilities": {
    doc: "leaf-utilities.md",
    topic: /linkpath|relative|hover|subpath|markdownlink|navigate/,
    hint: "Link resolution + navigation helpers used across leaves: parseLinktext, resolveRelativePath, openLinkText, hover popovers, backlinks/outgoing-link resolution.",
    methods: /^(parseLinktext|resolveRelativePath|resolveSubpath|generateMarkdownLink|openLinkText|getLinkpath|hoverPopover|showLinkHover|getFirstLinkpathDest|toMarkdownLink|modify|applying)/,
    literals: ["^\\[\\^", "^#", "^\\\\[", "subpath", "|", "#^", "alias", "embed", "![[", "[["],
    api: ["app.workspace.openLinkText"],
  },
};

// ---------------------------------------------------------------- queries
const qMethdef = db.prepare("SELECT name, bundle, rel, line FROM facts WHERE kind='methdef' AND name = ? LIMIT 1");
const qStr = db.prepare(
  "SELECT value, bundle, rel, line FROM facts WHERE kind='str' AND value = ? LIMIT 6"
);
const qApi = db.prepare("SELECT name, rel FROM search WHERE search MATCH ? AND kind='prop' LIMIT 12");
// The semantic vocabulary is the *method* names, not the declaration names.
// In Obsidian's own code essentially every declaration is a 1-3 char terser
// temp, so matching on `bindef` (filtered to length>5) returned almost nothing;
// the 20k `methdef` names are the part terser did not touch and are exactly
// what a reader would search for.
const qMethNames = db.prepare("SELECT DISTINCT name FROM facts WHERE kind='methdef' ORDER BY name");
const allBindings = qMethNames.all().map((r) => r.name);

fs.mkdirSync(outDir, { recursive: true });
const summary = [];

for (const [domain, cfg] of Object.entries(DOMAINS)) {
  const lines = [];
  lines.push(`# Seed map — \`domains/${cfg.doc}\` (Obsidian 1.13.7)`);
  lines.push("");
  lines.push(`**Scope:** ${cfg.hint}`);
  lines.push("");
  lines.push("Derived from `renamed/index.sqlite`. Every entry is a location in the");
  lines.push("readable corpus. Nothing here is a finding — it is where to start reading.");
  lines.push("");
  lines.push("```bash");
  lines.push(`Q="node RE/tools/q.js --db RE/obsidian/1.13.7/renamed/index.sqlite"`);
  lines.push("```");
  lines.push("");

  // Two match modes, because the stored patterns are $-anchored alternations:
  // `^(get|create|...)$` is an EXACT set, so `getAvailablePath` does not match it.
  // Testing those as a bare regex silently produced 0 bindings for every domain.
  const alts = new Set(
    cfg.methods.source
      .replace(/^\^/, "")
      .replace(/\$$/, "")
      .replace(/^\(/, "")
      .replace(/\)$/, "")
      .split("|")
  );
  const exact = allBindings.filter((n) => alts.has(n));
  const topicSrc = cfg.topic instanceof RegExp ? cfg.topic.source : cfg.topic;
  const topicRe = new RegExp(topicSrc, "i");
  const topical = allBindings.filter((n) => topicRe.test(n) && !alts.has(n));

  const section = (title, names, cap) => {
    if (!names.length) return;
    lines.push(`## ${title} (${names.length})`);
    lines.push("");
    for (const n of names.slice(0, cap)) {
      const r = qMethdef.get(n);
      lines.push(r ? `- \`${n}\` — \`${r.bundle}/${r.rel}:${r.line}\`` : `- \`${n}\``);
    }
    if (names.length > cap) lines.push(`- … ${names.length - cap} more`);
    lines.push("");
  };
  section("Methods named exactly in this domain's vocabulary", exact, 30);
  section("Methods whose name is topical to this domain", topical, 60);

  const renamedClasses = Object.values(renames.names)
    .filter((r) => r.new)
    .map((r) => r.new);
  const renamedHits = renamedClasses.filter((n) => alts.has(n) || topicRe.test(n));
  if (renamedHits.length) {
    lines.push(`## Renamed classes in this area (${renamedHits.length})`);
    lines.push("");
    for (const n of renamedHits) lines.push(`- \`${n}\``);
    lines.push("");
  }

  // On-disk / on-wire literals: the compat contract.
  const lit = [];
  for (const lit2 of cfg.literals) {
    for (const r of qStr.all(lit2)) lit.push({ lit: lit2, ...r });
  }
  if (lit.length) {
    lines.push(`## Contract literals found in source (${lit.length})`);
    lines.push("");
    lines.push("These are the strings a claim will usually quote. If a domain doc");
    lines.push("describes behaviour around one of these, check it here first.");
    lines.push("");
    for (const l of lit.slice(0, 30)) lines.push(`- \`${JSON.stringify(l.lit)}\` — \`${l.bundle}/${l.rel}:${l.line}\``);
    lines.push("");
  }

  if (cfg.api.length) {
    lines.push(`## API paths`);
    lines.push("");
    for (const a of cfg.api) {
      const rows = qApi.all(`"${a.split(".").slice(0, 2).join(".")}"`);
      lines.push(`- \`${a}\` (${rows.length} refs)`);
    }
    lines.push("");
  }

  lines.push("## Live queries for this domain");
  lines.push("");
  lines.push("```bash");
  const glob = topicSrc.replace(/^\^/, "").replace(/\$$/, "").split("|").map((x) => x.replace(/[.*+?^${}()|[\]\\]/g, "\\$&") + "*").join("|");
  lines.push(`$Q sql "SELECT name, bundle||'/'||rel AS at, line FROM facts WHERE kind='methdef' AND name GLOB '${glob}' ORDER BY name LIMIT 60"`);
  lines.push(`$Q sql "SELECT bundle||'/'||rel AS at, line, substr(value,1,90) FROM facts WHERE kind='str' AND value LIKE '%${cfg.literals[0]}%' LIMIT 20"`);
  lines.push("```");
  lines.push("");

  const file = path.join(outDir, `${domain}.md`);
  fs.writeFileSync(file, lines.join("\n"));
  summary.push(`${domain.padEnd(17)} ${String(exact.length).padStart(3)} exact + ${String(topical.length).padStart(3)} topical  ${String(lit.length).padStart(2)} literals  ${renamedHits.length} renamed`);
}

console.log(`seed maps -> ${outDir}`);
for (const s of summary) console.log("  " + s);