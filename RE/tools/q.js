#!/usr/bin/env node
// Query the corpus index.
//
//   node q.js find <text> [--kind K] [--bundle B] [--limit N] [--context N]
//   node q.js at <bundle>/<rel> <line> [--context N]
//   node q.js sql "<SQL>"
//   node q.js stats
//
// `find` is the one that matters: it is how a claim in docs/obsidian-audit gets
// checked. Audit claims are usually *about literals* (a path, a JSON key, an
// i18n key, a regex body), and literals survive minification, so:
//
//   node q.js find workspace.json
//   node q.js find --kind regex 'break-spaces'
//   node q.js at app/own/1129-UD.js 12 --context 20
//
// answers "is that still true, and where" in one step.
const fs = require("fs");
const path = require("path");
const Database = require("better-sqlite3");

const argv = process.argv.slice(2);
const flag = (n, d) => {
  const i = argv.indexOf(`--${n}`);
  return i === -1 ? d : argv[i + 1];
};
const positional = argv.filter((a, i) => !a.startsWith("--") && !(i > 0 && argv[i - 1].startsWith("--") && !["find", "at", "sql", "stats"].includes(argv[i - 1])));

const dbPath = flag("db", null) || path.resolve(__dirname, "../obsidian/1.13.7/index.sqlite");
if (!fs.existsSync(dbPath)) {
  console.error(`no index at ${dbPath} — run: node build-index.js <corpus-root> <out.sqlite>`);
  process.exit(1);
}
const db = new Database(dbPath, { readonly: true });
const SRC_ROOT = path.resolve(path.dirname(dbPath), "src");

const cmd = positional[0] || "stats";

// ------------------------------------------------------------------- source
const fileCache = new Map();
function sourceOf(bundle, rel) {
  const k = `${bundle}/${rel}`;
  if (!fileCache.has(k)) {
    const p = path.join(SRC_ROOT, bundle, rel);
    fileCache.set(k, fs.existsSync(p) ? fs.readFileSync(p, "utf8").split("\n") : null);
  }
  return fileCache.get(k);
}
function snippet(lines, line, width = 150) {
  const l = lines[line - 1];
  if (l === undefined) return "";
  return l.trim().slice(0, width);
}
function context(bundle, rel, line, n) {
  const lines = sourceOf(bundle, rel);
  if (!lines) return "";
  const from = Math.max(1, line - n);
  const to = Math.min(lines.length, line + n);
  const out = [];
  for (let i = from; i <= to; i++) {
    out.push(`${i === line ? ">" : " "} ${String(i).padStart(5)} | ${lines[i - 1].slice(0, 200)}`);
  }
  return out.join("\n");
}

// -------------------------------------------------------------------- find
if (cmd === "find") {
  const term = positional[1];
  if (!term) {
    console.error('usage: q.js find <text> [--kind str|regex|tmpl|methdef|prop|decl] [--bundle app] [--limit 40] [--context 3]');
    process.exit(2);
  }
  const kind = flag("kind", null);
  const bundle = flag("bundle", null);
  const limit = Number(flag("limit", 40));
  const ctx = Number(flag("context", 0));

  // FTS5: quote the term so punctuation can't be read as query syntax. A bare
  // `foo.bar` would otherwise be parsed as a column filter and silently return
  // nothing - which looks like "not present" rather than "bad query".
  const q = `"${term.replace(/"/g, '""')}"`;
  let sql = `SELECT kind, bundle, rel, line, name, value FROM search WHERE search MATCH ?`;
  const args = [q];
  if (kind) { sql += ` AND kind = ?`; args.push(kind); }
  if (bundle) { sql += ` AND bundle = ?`; args.push(bundle); }
  sql += ` ORDER BY rank LIMIT ?`;
  args.push(limit);

  let rows;
  try {
    rows = db.prepare(sql).all(...args);
  } catch (e) {
    console.error(`query failed: ${e.message}`);
    process.exit(1);
  }
  if (!rows.length) {
    console.log(`no match for ${JSON.stringify(term)}${kind ? ` (kind=${kind})` : ""}`);
    process.exit(0);
  }
  console.log(`${rows.length}${rows.length === limit ? "+" : ""} match(es) for ${JSON.stringify(term)}${kind ? ` kind=${kind}` : ""}:\n`);
  for (const r of rows) {
    const what = r.name ? r.name : JSON.stringify(String(r.value).slice(0, 110));
    console.log(`  ${r.bundle}/${r.rel}:${r.line}  [${r.kind}]  ${what}`);
    if (ctx) console.log(context(r.bundle, r.rel, r.line, ctx).replace(/^/gm, "      "));
  }
}

// ---------------------------------------------------------------------- at
else if (cmd === "at") {
  const where = positional[1];
  const line = Number(positional[2]);
  if (!where || !line) {
    console.error("usage: q.js at <bundle>/<rel> <line> [--context 20]");
    process.exit(2);
  }
  const slash = where.indexOf("/");
  const bundle = where.slice(0, slash);
  const rel = where.slice(slash + 1);
  console.log(context(bundle, rel, line, Number(flag("context", 20))));
}

// --------------------------------------------------------------------- sql
else if (cmd === "sql") {
  const q = positional.slice(1).join(" ");
  if (!q) {
    console.error('usage: q.js sql "SELECT kind, count(*) FROM facts GROUP BY kind"');
    process.exit(2);
  }
  const rows = db.prepare(q).all();
  if (!rows.length) console.log("(no rows)");
  else {
    const cols = Object.keys(rows[0]);
    console.log(cols.join("\t"));
    for (const r of rows) console.log(cols.map((c) => String(r[c] ?? "")).join("\t"));
  }
}

// ------------------------------------------------------------------- stats
else {
  const files = db.prepare("SELECT count(*) n, sum(bytes) b FROM corpus").get();
  console.log(`index: ${dbPath}`);
  console.log(`corpus: ${files.n} files, ${((files.b || 0) / 1048576).toFixed(1)}MB source\n`);
  for (const r of db.prepare("SELECT kind, count(*) n FROM facts GROUP BY kind ORDER BY n DESC").all())
    console.log(`  ${r.kind.padEnd(9)} ${r.n.toLocaleString()}`);
  console.log(`\ntop bundles by source size:`);
  for (const r of db.prepare("SELECT bundle, sum(bytes) b, count(*) n FROM corpus GROUP BY bundle ORDER BY b DESC").all())
    console.log(`  ${r.bundle.padEnd(9)} ${(r.b / 1048576).toFixed(2)}MB in ${r.n} files`);
}