#!/usr/bin/env node
// Build the RE manifest for one extracted app version.
//
//   node build-manifest.js <version-dir> <source.deb>
//
// ## What this deliberately does not do
//
// An earlier attempt assigned every statement to one of the audit's semantic
// domains (vault/workspace/editor/...) by keyword matching. That was unsound and
// the numbers said so: only ~10% of statements could be attributed, because
// Obsidian's own code reaches its internals through per-module mangled locals
// (`bd` appears 1379 times, `rd` 575, while minifier locals `e`/`t`/`n` appear
// 46945/36031/24380 times). There is no single alias table to route on.
//
// So this tool reports only what the binary states about itself, and builds an
// inverted index so a reader can ask "which statements touch X" instead:
//
//   * the core-plugin id registry       - from `.id = "..."` initialisers
//   * the i18n key tree                 - Obsidian's own feature taxonomy
//   * an `app.*` / `bd.*` census        - sparse but precise
//   * `this.<method>` surfaces per class- the real navigational index
//
// Semantic domain assignment stays where the previous audit put it: with the
// reader. This is an index, not a judgement.
const fs = require("fs");
const path = require("path");
const crypto = require("crypto");
const parser = require("@babel/parser");

const [versionDir, debPath] = process.argv.slice(2);
if (!versionDir) {
  console.error("usage: build-manifest.js <version-dir> [source.deb]");
  process.exit(2);
}
const BUNDLES = ["app", "worker", "starter", "help", "sim", "enhance", "i18n", "main"];

const sha = (p) => {
  try {
    return crypto.createHash("sha256").update(fs.readFileSync(p)).digest("hex");
  } catch {
    return null;
  }
};

// `app.*` and `bd.*` are first-party and stable across the whole bundle, unlike
// the mangled locals. Sparse but precise, so they index rather than classify.
const API_RE = /\b(app|bd)\.([A-Za-z0-9_$]+(?:\.[A-Za-z0-9_$]+)?)/g;
const THIS_METHOD_RE = /\bthis\.([A-Za-z_$][\w$]{2,})\s*=/g;

// A regex for `<something>.id = "core-plugin"` is wrong in both directions: it
// misses plugins that assign `id` off `this`/a prototype, and it matches inside
// string literals (an earlier attempt confidently reported "logo-top-left" and
// nine friends as core plugins - they are SVG gradient ids in the window-frame
// code). Only an AST walk is exact, and the difference was 43 vs the true 40.
const BABEL_OPTS = {
  sourceType: "script",
  allowReturnOutsideFunction: true,
  allowAwaitOutsideFunction: true,
  errorRecovery: true,
  ranges: true,
};
function idAssignments(src) {
  let ast;
  try {
    ast = parser.parse(src, BABEL_OPTS);
  } catch {
    return [];
  }
  const out = [];
  const visit = (n) => {
    if (!n || typeof n !== "object") return;
    if (Array.isArray(n)) return n.forEach(visit);
    if (
      n.type === "AssignmentExpression" &&
      n.left.type === "MemberExpression" &&
      !n.left.computed &&
      n.left.property.name === "id" &&
      n.right.type === "StringLiteral" &&
      /^[a-z][a-z0-9-]{2,}$/.test(n.right.value)
    ) {
      out.push(n.right.value);
    }
    for (const k of Object.keys(n)) {
      if (k === "loc") continue;
      const v = n[k];
      if (v && typeof v === "object" && (v.type || Array.isArray(v))) visit(v);
    }
  };
  visit(ast);
  return out;
}

const out = { generated: new Date().toISOString().slice(0, 10), version: path.basename(versionDir) };
out.provenance = {
  deb: debPath ? path.basename(debPath) : null,
  debBytes: debPath ? fs.statSync(debPath).size : null,
  debSha256: debPath ? sha(debPath) : null,
  obsidianAsarSha256: sha(path.join(versionDir, "pkg/opt/Obsidian/resources/obsidian.asar")),
  appAsarSha256: sha(path.join(versionDir, "pkg/opt/Obsidian/resources/app.asar")),
  installPrefix: "/opt/Obsidian",
};

const apiIndex = new Map();   // "app.workspace" -> [ "app/own/0754-wy.js", ... ]
const methodIndex = new Map(); // "onLayoutChange" -> [ "app/own/2040-Vee.js", ... ]
const corePluginHits = new Map();
const registry = { statements: [], api: {}, methods: {} };
let totalStatements = 0;

for (const b of BUNDLES) {
  const idxPath = path.join(versionDir, "src", b, "index.json");
  if (!fs.existsSync(idxPath)) continue;
  const idx = JSON.parse(fs.readFileSync(idxPath, "utf8"));
  const rep = fs.existsSync(path.join(versionDir, "src", b, "report.json"))
    ? JSON.parse(fs.readFileSync(path.join(versionDir, "src", b, "report.json"), "utf8"))
    : {};
  const entry = {
    bundleBytes: rep.bytes || null,
    vendorModules: rep.vendor ? rep.vendor.moduleCount : 0,
    vendorMaps: rep.vendor ? rep.vendor.maps : 0,
    ownStatements: idx.statements.length,
    ownBytes: idx.statements.reduce((a, s) => a + s.bytes, 0),
  };
  for (const s of idx.statements) {
    totalStatements++;
    let text = "";
    try { text = fs.readFileSync(path.join(versionDir, "src", b, "own", s.file), "utf8"); } catch {}
    const where = `${b}/own/${s.file}`;
    const api = new Set();
    for (const m of text.matchAll(API_RE)) {
      api.add(m[0]);
      if (!apiIndex.has(m[0])) apiIndex.set(m[0], []);
      apiIndex.get(m[0]).push(where);
    }
    for (const m of text.matchAll(THIS_METHOD_RE)) {
      if (!methodIndex.has(m[1])) methodIndex.set(m[1], []);
      methodIndex.get(m[1]).push(where);
    }
    // Core plugins assign their registry id off `this` or a prototype.
    for (const id of idAssignments(text)) {
      if (!corePluginHits.has(id)) corePluginHits.set(id, new Map());
      corePluginHits.get(id).set(where, true);
    }
    registry.statements.push({
      at: where, bytes: s.bytes, type: s.type, names: s.names,
      api: [...api].slice(0, 12), strings: s.strings,
    });
  }
  out.bundles = out.bundles || {};
  out.bundles[b] = entry;
}

// ---- first-party registries
out.corePlugins = [...corePluginHits.entries()]
  .map(([id, w]) => ({ id, statements: [...w.keys()].sort() }))
  .sort((a, b) => a.id.localeCompare(b.id));
out.corePluginIds = out.corePlugins.map((p) => p.id);

out.apiIndex = Object.fromEntries(
  [...apiIndex.entries()].sort((a, b) => a[0].localeCompare(b[0])).map(([k, v]) => [k, [...new Set(v)]])
);
out.methodIndex = Object.fromEntries(
  [...methodIndex.entries()].sort((a, b) => a[0].localeCompare(b[0])).map(([k, v]) => [k, [...new Set(v)]])
);
out.coverage = {
  totalStatements,
  statementsTouchingAppOrBd: registry.statements.filter((s) => s.api.length).length,
  distinctApiPaths: apiIndex.size,
  distinctThisMethods: methodIndex.size,
};
out.bundles = out.bundles || {};

// ---- bundled third-party libraries
const libDir = path.join(versionDir, "asar", "obsidian", "lib");
if (fs.existsSync(libDir)) {
  const libs = [];
  const walk = (d, rel) => {
    for (const e of fs.readdirSync(d, { withFileTypes: true })) {
      const p = path.join(d, e.name);
      if (e.isDirectory()) walk(p, path.join(rel, e.name));
      else if (/\.(m?js)$/.test(e.name)) libs.push({ path: path.join(rel, e.name), bytes: fs.statSync(p).size });
    }
  };
  walk(libDir, "lib");
  libs.sort((a, b) => b.bytes - a.bytes);
  out.vendorLibs = libs;
}

// ---- i18n subtrees (Obsidian's own feature taxonomy)
try {
  const tree = fs.readFileSync(path.join(versionDir, "src/i18n/tree.js"), "utf8");
  const subs = [...tree.matchAll(/^ {2}"([^"]+)": \{/gm)].map((m) => m[1]);
  out.i18nSubtrees = subs;
} catch {}

// --------------------------------------------------------------------- render
let md = `# Obsidian RE manifest — ${out.version}\n\n`;
md += `Generated ${out.generated} by \`RE/tools/build-manifest.js\`. **Scratch output, untracked.**\n`;
md += `The durable product of this work is \`docs/obsidian-audit/\` (specs) and\n`;
md += `\`docs/obsidian-audit/addenda/\` (corrections) — never this file.\n\n`;

md += `## Provenance\n\n| | |\n|---|---|\n`;
if (out.provenance.deb) md += `| Source | \`${out.provenance.deb}\`, ${(out.provenance.debBytes / 1048576).toFixed(1)} MB |\n`;
if (out.provenance.debSha256) md += `| deb sha256 | \`${out.provenance.debSha256}\` |\n`;
if (out.provenance.obsidianAsarSha256) md += `| \`obsidian.asar\` sha256 | \`${out.provenance.obsidianAsarSha256}\` |\n`;
if (out.provenance.appAsarSha256) md += `| \`app.asar\` sha256 | \`${out.provenance.appAsarSha256}\` |\n`;
md += `| Install prefix | \`${out.provenance.installPrefix}\` |\n`;
md += `| Previous audit baseline | **1.12.7** (\`docs/obsidian-audit/VAULT-FORMAT.md:735\`) |\n\n`;

md += `## What the package contains\n\n`;
md += `The installer ships **two** asars. \`app.asar\` (480 KB) is the Electron\n`;
md += `bootstrap — main process plus native \`btime\` / \`get-fonts\` addons.\n`;
md += `\`obsidian.asar\` (25.8 MB) is the application.\n\n`;
md += `Inside \`obsidian.asar\`, each \`*.js\` entry point is one of two shapes:\n\n`;
md += `1. **Webpack bundle** — an object literal keyed by numeric ids whose values are\n`;
md += `   factories. That is bundled third-party code. Extracted to \`src/*/vendor/\`.\n`;
md += `2. **Flattened rollup bundle** — one function body holding thousands of\n`;
md += `   top-level statements. This is Obsidian's own code. Extracted one statement\n`;
md += `   per file to \`src/*/own/\`, plus a combined \`all.js\`.\n\n`;
md += `The own-code bundle is **minified, not obfuscated**. No string-array encoding,\n`;
md += `no control-flow flattening. Identifiers are mangled, but every string literal\n`;
md += `survives, and rollup preserves ESM named exports via \`__export\` maps, so\n`;
md += `third-party packages stay identifiable. That is what makes this auditable.\n\n`;

md += `## Bundle inventory\n\n| Entry point | Bytes | Vendor modules | Own-code statements | Own-code bytes |\n|---|---:|---:|---:|---:|\n`;
for (const [b, v] of Object.entries(out.bundles)) {
  md += `| \`${b}.js\` | ${(v.bundleBytes || 0).toLocaleString()} | ${v.vendorModules} | ${v.ownStatements} | ${v.ownBytes.toLocaleString()} |\n`;
}

md += `\n## Index coverage\n\n`;
md += `| | |\n|---|---:|\n`;
md += `| Own-code statements indexed | ${out.coverage.totalStatements} |\n`;
md += `| Statements referencing \`app.*\` or \`bd.*\` | ${out.coverage.statementsTouchingAppOrBd} |\n`;
md += `| Distinct \`app.*\`/\`bd.*\` paths | ${out.coverage.distinctApiPaths} |\n`;
md += `| Distinct \`this.<method>\` definitions | ${out.coverage.distinctThisMethods} |\n`;
md += `| Core plugins (AST-exact) | ${out.corePluginIds.length} |\n\n`;

if (out.i18nSubtrees) {
  md += `## i18n key subtrees\n\nObsidian's own feature taxonomy, straight from\n`;
  md += `\`window.OBSIDIAN_DEFAULT_I18N\` (full tree in \`src/i18n/tree.js\`). Every user-visible\n`;
  md += `string in the app hangs off one of these, so this doubles as a completeness\n`;
  md += `check on the audit: a feature with no strings here is either untranslated or\n`;
  md += `not implemented.\n\n${out.i18nSubtrees.map((s) => `\`${s}\``).join(" · ")}\n\n`;
}

md += `## Core plugin registry\n\n**${out.corePluginIds.length} plugins**, recovered by AST-walking every\n`;
md += `\`.id = "…"\` assignment rather than by regex, and each mapped to the statement\n`;
md += `that implements it. This is the authoritative list of what Obsidian ships by\n`;
md += `default, as opposed to what a given vault has actually enabled.\n\n`;
md += `| Core plugin | Implementing statement |\n|---|---|\n`;
for (const p of out.corePlugins) md += `| \`${p.id}\` | ${p.statements.map((x) => `\`${x}\``).join(", ")} |\n`;
md += `\nA regex-based pass reports 43 ids and includes \`logo-*\` and \`clip\`; those are\n`;
md += `SVG gradient ids matched inside string literals. 40 is the real count.\n\n`;

md += `## \`app.*\` / \`bd.*\` census\n\nSparse but precise. This is the navigational index: pick a path, get the\n`;
md += `statements that touch it.\n\n| Path | Statements |\n|---|---:|\n`;
for (const [k, v] of Object.entries(out.apiIndex).sort((a, b) => b[1].length - a[1].length).slice(0, 40)) {
  md += `| \`${k}\` | ${v.length} |\n`;
}
md += `\n${out.coverage.distinctApiPaths} paths total — see \`manifest.json\` for the full inverted index.\n\n`;

md += `## Bundled third-party libraries (\`lib/\`)\n\nOut of audit scope. Recorded so nobody re-derives it, and because several are the\n`;
md += `basis for our own replacements.\n\n| Library | Bytes |\n|---|---:|\n`;
for (const l of (out.vendorLibs || []).slice(0, 30)) md += `| \`${l.path}\` | ${l.bytes.toLocaleString()} |\n`;
if ((out.vendorLibs || []).length > 30) md += `| … ${out.vendorLibs.length - 30} more | |\n`;

fs.writeFileSync(path.join(versionDir, "MANIFEST.md"), md);
fs.writeFileSync(path.join(versionDir, "manifest.json"), JSON.stringify({ ...out, registry }, null, 2));

console.log(`MANIFEST.md: ${out.coverage.totalStatements} statements, ${out.corePluginIds.length} core plugins, ${out.coverage.distinctApiPaths} api paths, ${out.coverage.distinctThisMethods} methods`);
console.log(`  app/bd coverage: ${out.coverage.statementsTouchingAppOrBd}/${out.coverage.totalStatements} (${((100 * out.coverage.statementsTouchingAppOrBd) / out.coverage.totalStatements).toFixed(1)}%) — see the header comment for why this is the ceiling`);