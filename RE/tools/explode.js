#!/usr/bin/env node
// Explode a minified Obsidian bundle into an auditable corpus.
//
//   node explode.js <bundle.js> <out-dir> [--min-stmts N] [--mode auto|statements|modules]
//
// Two independent shapes matter here, and a bundle can contain both:
//
//   * A webpack module map - an object literal keyed by numeric ids whose values
//     are factory functions. Obsidian keeps its bundled third-party code this
//     way. Those become vendor/<id>.js.
//
//   * A flattened rollup bundle - one function body with thousands of top-level
//     statements. This is Obsidian's own code. Rollup mangles the identifiers but
//     preserves string literals, and it preserves ESM named exports via
//     `__export(e,{RealName:()=>x})`, which is the only reliable naming anchor.
//
// For the flattened part we emit one file per top-level statement plus an index,
// because the earlier three-pass audit worked statement-by-statement and a
// 3.6MB single file is not something a reader can navigate. We also derive a
// routing table from the `bd.plugins.*` / `bd.setting.*` / `app.*` namespaces,
// which is what the audit's taxonomy pass was built on.
const fs = require("fs");
const path = require("path");
const parser = require("@babel/parser");
const beautify = require("js-beautify").js;

const argv = process.argv.slice(2);
const positional = argv.filter((a) => !a.startsWith("--"));
const flag = (n, d) => {
  const i = argv.indexOf(`--${n}`);
  return i === -1 ? d : argv[i + 1];
};
const [input, outDir] = positional;
if (!input || !outDir) {
  console.error("usage: explode.js <bundle.js> <out-dir>");
  process.exit(2);
}
const MIN_STMTS = Number(flag("min-stmts", 60));
const MODE = flag("mode", "auto");

const name = path.basename(input);
const src = fs.readFileSync(input, "utf8");

let ast;
try {
  ast = parser.parse(src, {
    sourceType: "script",
    allowReturnOutsideFunction: true,
    allowAwaitOutsideFunction: true,
    allowSuperOutsideMethod: true,
    allowUndeclaredExports: true,
    errorRecovery: true,
    ranges: true,
  });
} catch (e) {
  console.error(`${name}: parse failed: ${e.message}`);
  process.exit(1);
}

const beautifyOpts = { indent_size: 2, wrap_line_length: 0, preserve_newlines: false };

// ---------------------------------------------------------------- vendor maps
function factoryOf(p) {
  const v = p.value;
  if (v.type === "ObjectMethod") return v;
  if (v.type === "ArrowFunctionExpression" || v.type === "FunctionExpression") return v;
  return null;
}
function findModuleMaps() {
  const found = [];
  const visit = (n) => {
    if (!n || typeof n !== "object") return;
    if (Array.isArray(n)) return n.forEach(visit);
    if (n.type === "ObjectExpression" && n.properties.length >= 8) {
      const numeric = n.properties.filter((p) => p.type === "ObjectProperty" && /^\d+$/.test(String(p.key.value ?? p.key.name ?? "")) && factoryOf(p));
      if (numeric.length >= 8) found.push({ node: n, numeric });
    }
    for (const k of Object.keys(n)) {
      if (k === "loc") continue;
      const v = n[k];
      if (v && typeof v === "object" && (v.type || Array.isArray(v))) visit(v);
    }
  };
  visit(ast);
  return found;
}

function exportNamesOf(node) {
  const out = new Set();
  const walk = (n) => {
    if (!n || typeof n !== "object" || out.size > 400) return;
    if (Array.isArray(n)) return n.forEach(walk);
    if (n.type === "ObjectExpression" && n.properties.length && n.properties.length <= 60) {
      const keys = n.properties.map((p) => p.key && (p.key.name ?? p.key.value));
      if (keys.every((k) => typeof k === "string" && /^[A-Za-z_$][\w$]*$/.test(k))) keys.forEach((k) => out.add(k));
    }
    for (const k of Object.keys(n)) {
      if (k === "loc") continue;
      const v = n[k];
      if (v && typeof v === "object" && (v.type || Array.isArray(v))) walk(v);
    }
  };
  walk(node);
  return [...out];
}

function leadingStrings(node, cap) {
  const out = [];
  const walk = (n) => {
    if (out.length >= cap) return;
    if (!n || typeof n !== "object") return;
    if (Array.isArray(n)) return n.forEach(walk);
    if (n.type === "StringLiteral" && n.value.length > 3 && n.value.length < 120) out.push(n.value);
    for (const k of Object.keys(n)) {
      if (k === "loc") continue;
      const v = n[k];
      if (v && typeof v === "object" && (v.type || Array.isArray(v))) walk(v);
    }
  };
  walk(node);
  return [...new Set(out)];
}

// Routing table: which subsystem does this statement belong to? Mirrors the
// namespace discipline Obsidian itself uses, which is what the audit's domains
// were keyed on.
const NS_PATTERNS = [
  [/\bbd\.plugins\.([A-Za-z0-9_]+)/g, (m) => `plugins.${m[1]}`],
  [/\bbd\.setting\.([A-Za-z0-9_]+)/g, (m) => `setting.${m[1]}`],
  [/\bbd\.callout\b/g, () => "callout"],
  [/\bapp\.workspace\b/g, () => "app.workspace"],
  [/\bapp\.vault\b/g, () => "app.vault"],
  [/\bapp\.metadataCache\b/g, () => "app.metadataCache"],
  [/\bapp\.editor\b/g, () => "app.editor"],
  [/\bapp\.fileManager\b/g, () => "app.fileManager"],
  [/\bapp\.keymap\b/g, () => "app.keymap"],
  [/\bapp\.scope\b/g, () => "app.scope"],
  [/\bapp\.commands\b/g, () => "app.commands"],
  [/\bapp\.internalPlugins\b/g, () => "app.internalPlugins"],
  [/\bapp\.plugins\b/g, () => "app.plugins"],
  [/\bapp\.customCss\b/g, () => "app.customCss"],
  [/\bapp\.loadLocalStorage\b/g, () => "app.storage"],
  [/\bapp\.saveLocalStorage\b/g, () => "app.storage"],
];
function namespaces(text) {
  const hits = {};
  for (const [re, key] of NS_PATTERNS) {
    re.lastIndex = 0;
    let m;
    while ((m = re.exec(text))) {
      const k = key(m);
      hits[k] = (hits[k] || 0) + 1;
    }
  }
  return Object.entries(hits)
    .sort((a, b) => b[1] - a[1])
    .map(([k, c]) => `${k}×${c}`);
}

// ------------------------------------------------------- flattened statement root
function findStatementRoot() {
  let best = null;
  const consider = (stmts, where) => {
    if (stmts.length < MIN_STMTS) return;
    const size = stmts.reduce((a, s) => a + (s.end - s.start), 0);
    if (!best || size > best.size) best = { stmts, size, where };
  };
  const visit = (n) => {
    if (!n || typeof n !== "object") return;
    if (Array.isArray(n)) return n.forEach(visit);
    if (n.type === "Program") consider(n.body, "Program");
    if ((n.type === "FunctionDeclaration" || n.type === "FunctionExpression" || n.type === "ArrowFunctionExpression") && n.body && n.body.type === "BlockStatement") {
      consider(n.body.body, `${n.type}${n.id ? " " + n.id.name : ""}`);
    }
    for (const k of Object.keys(n)) {
      if (k === "loc") continue;
      const v = n[k];
      if (v && typeof v === "object" && (v.type || Array.isArray(v))) visit(v);
    }
  };
  visit(ast);
  return best;
}

fs.mkdirSync(outDir, { recursive: true });
const report = { bundle: name, bytes: src.length, vendor: null, statements: null };
const maps = MODE === "statements" ? [] : findModuleMaps();
const root = MODE === "modules" ? null : findStatementRoot();

// Vendor modules first.
if (maps.length) {
  const vdir = path.join(outDir, "vendor");
  fs.mkdirSync(vdir, { recursive: true });
  const perMap = [];
  for (const m of maps) {
    const entries = {};
    for (const p of m.numeric) {
      const id = String(p.key.value ?? p.key.name);
      const f = factoryOf(p);
      const body = src.slice(f.body.start, f.body.end);
      const req = f.params[2];
      const deps = req && req.name ? [...new Set([...body.matchAll(new RegExp(`\\b${req.name}\\((\\d+)\\)`, "g"))].map((x) => +x[1]))] : [];
      const names = exportNamesOf(f);
      fs.writeFileSync(path.join(vdir, `${id}.js`), `// ${name} vendor module ${id}\n// deps: ${deps.join(", ") || "none"}\n${beautify(body, beautifyOpts)}`);
      entries[id] = { bytes: body.length, deps, exportNames: names.slice(0, 15), strings: leadingStrings(f, 6) };
    }
    perMap.push({ offset: m.node.start, count: m.numeric.length, modules: entries });
  }
  report.vendor = { maps: perMap.length, moduleCount: perMap.reduce((a, m) => a + m.count, 0), detail: perMap };
  const bytes = perMap.reduce((a, m) => a + Object.values(m.modules).reduce((x, y) => x + y.bytes, 0), 0);
  console.log(`${name}: vendor ${report.vendor.moduleCount} modules in ${perMap.length} map(s), ${(bytes / 1024).toFixed(0)}KB -> ${vdir}`);
}

// Obsidian's own code: one file per top-level statement.
if (root) {
  const sdir = path.join(outDir, "own");
  fs.mkdirSync(sdir, { recursive: true });
  const parts = [];
  const index = [];
  root.stmts.forEach((s, i) => {
    const text = src.slice(s.start, s.end);
    const ids = [];
    if (s.type === "VariableDeclaration") s.declarations.forEach((d) => d.id && d.id.name && ids.push(d.id.name));
    else if ((s.type === "FunctionDeclaration" || s.type === "ClassDeclaration") && s.id) ids.push(s.id.name);
    else if (s.type === "ExpressionStatement") {
      const ex = s.expression;
      const inner = ex.type === "SequenceExpression" ? ex.expressions[ex.expressions.length - 1] : ex;
      if (inner && inner.type === "AssignmentExpression" && inner.left.type === "Identifier") ids.push(inner.left.name);
    }
    const ns = namespaces(text);
    const label = ids[0] || s.type.toLowerCase();
    const file = `${String(i).padStart(4, "0")}-${label}.js`;
    const pretty = beautify(text, beautifyOpts);
    fs.writeFileSync(path.join(sdir, file), `// ${name} stmt #${i} (${s.start}-${s.end}, ${text.length}B)\n${pretty}\n`);
    parts.push(`\n// ${"=".repeat(70)}\n// stmt #${i}  ${file}  ${text.length}B\n// ${"=".repeat(70)}\n${pretty}`);
    index.push({ i, file, type: s.type, from: s.start, to: s.end, bytes: text.length, names: ids, namespaces: ns, strings: leadingStrings(s, 6) });
  });
  fs.writeFileSync(path.join(outDir, "all.js"), `/* ${name} - flattened statements (${root.where}) */\n${parts.join("\n")}\n`);
  report.statements = { where: root.where, count: index.length, bytes: root.size, files: sdir, combined: path.join(outDir, "all.js") };
  fs.writeFileSync(path.join(outDir, "index.json"), JSON.stringify({ bundle: name, statements: index }, null, 2));
  const withNs = index.filter((x) => x.namespaces.length).length;
  console.log(`${name}: ${index.length} own-code statements, ${(root.size / 1024 / 1024).toFixed(2)}MB, ${withNs} carry a subsystem tag -> ${sdir}`);
}

fs.writeFileSync(path.join(outDir, "report.json"), JSON.stringify(report, null, 2));
if (!maps.length && !root) console.log(`${name}: nothing recognisable (${src.length}B)`);