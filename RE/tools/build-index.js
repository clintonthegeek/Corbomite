#!/usr/bin/env node
// Build the searchable index over an extracted corpus.
//
//   node build-index.js <corpus-root> <out.sqlite>
//
//   corpus-root looks like  RE/obsidian/1.13.7/src   (contains <bundle>/own/*.js)
//
// ## Why this exists
//
// The previous audit's corpus is gone, so its symbol citations cannot be checked.
// But its claims are mostly *about literals* — a regex body, a JSON key, an i18n
// key, a path like "workspace.json" — and literals survive minification intact.
// So the cheapest way to make the audit falsifiable again is not renaming, it is
// making every literal and every property name locatable in one query.
//
// Facts recorded:
//   str      string literal            the highest-value kind: paths, JSON keys,
//                                       i18n keys, command ids, css selectors
//   regex    regex literal             self-contained; matches a claim by content
//   tmpl     template literal          often embedded markup or SQL
//   methdef  method/field definition   `X.prototype.foo=` / `this.foo=`
//   prop     property reference        `.foo` (non-computed)
//   decl     top-level binding         the ~2.3k names that are actually mangled
//   bindef   any binding, any depth    after renaming, the meaningful names live on
//                                       nested function declarations (WorkspaceLeaf
//                                       is `function WorkspaceLeaf()` inside an
//                                       IIFE, not a top-level var), so a
//                                       top-level-only index cannot find them
//
// `prop`/`methdef` are scope-free by nature (property names are not mangled), so
// they are exact without any scope analysis. `decl` is recorded but deliberately
// NOT resolved to uses here - that needs real scope analysis and is the
// renamer's job, not the index's.
const fs = require("fs");
const path = require("path");
const parser = require("@babel/parser");
const Database = require("better-sqlite3");

const [root, out] = process.argv.slice(2);
if (!root || !out) {
  console.error("usage: build-index.js <corpus-root> <out.sqlite>");
  process.exit(2);
}

const OPTS = {
  sourceType: "script",
  allowReturnOutsideFunction: true,
  allowAwaitOutsideFunction: true,
  allowSuperOutsideMethod: true,
  allowUndeclaredExports: true,
  errorRecovery: true,
};

fs.rmSync(out, { force: true });
const db = new Database(out);
db.pragma("journal_mode = WAL");
db.exec(`
CREATE TABLE corpus (bundle TEXT, rel TEXT, bytes INT, sha TEXT);
CREATE TABLE facts (
  bundle TEXT, rel TEXT, line INT, col INT, kind TEXT, name TEXT, value TEXT
);
CREATE VIRTUAL TABLE search USING fts5(name, value, kind UNINDEXED, bundle UNINDEXED, rel UNINDEXED, line UNINDEXED);
CREATE INDEX facts_kind_name ON facts(kind, name);
CREATE INDEX facts_rel      ON facts(rel);
`);

const insCorpus = db.prepare("INSERT INTO corpus VALUES (?,?,?,?)");
const insFact = db.prepare("INSERT INTO facts VALUES (?,?,?,?,?,?,?)");
const insSearch = db.prepare("INSERT INTO search VALUES (?,?,?,?,?,?)");

const stat = { files: 0, bytes: 0, parseFailures: 0, facts: 0 };
const kinds = {};
const bump = (k) => {
  kinds[k] = (kinds[k] || 0) + 1;
  stat.facts++;
};

// A definition is a member expression on the LHS of an assignment, or the key of
// an ObjectMethod / ClassMethod. That covers `X.prototype.foo = …`,
// `this.foo = …` and `foo() {}` alike.
//
// NB: this takes the parent explicitly. An earlier version read `node.parent`,
// which is always undefined in a manual AST recursion (only @babel/traverse
// synthesises `parent`), so every definition silently classified as a reference
// and `methdef` came out as 0.
function methodNameOf(node, parent) {
  if (!parent) return false;
  if (parent.type === "AssignmentExpression" && parent.left === node) return true;
  if ((parent.type === "ObjectMethod" || parent.type === "ClassMethod") && parent.key === node) return true;
  return false;
}

// Names bound by a top-level statement. Declarations are grandchildren of the
// statement (var X = … is Stmt -> Declarator -> Identifier), so testing
// "is this an Identifier and am I top level" never fires; the statement itself
// has to be interrogated.
function declaredNames(st) {
  const out = [];
  const fromPattern = (n) => {
    if (!n) return;
    if (n.type === "Identifier") out.push(n);
    else if (n.type === "ObjectPattern") n.properties.forEach((p) => fromPattern(p.value || p.argument));
    else if (n.type === "ArrayPattern") n.elements.forEach(fromPattern);
    else if (n.type === "AssignmentPattern") fromPattern(n.left);
    else if (n.type === "RestElement") fromPattern(n.argument);
  };
  if (st.type === "VariableDeclaration") st.declarations.forEach((d) => fromPattern(d.id));
  else if ((st.type === "FunctionDeclaration" || st.type === "ClassDeclaration") && st.id) out.push(st.id);
  return out;
}

const walkFile = db.transaction((bundle, rel, file) => {
  const text = fs.readFileSync(file, "utf8");
  let ast;
  try {
    ast = parser.parse(text, OPTS);
  } catch (e) {
    stat.parseFailures++;
    console.error(`  parse failed: ${bundle}/${rel}: ${e.message}`);
    return;
  }
  const emit = (kind, name, value, node) => {
    const line = node.loc ? node.loc.start.line : 0;
    const col = node.loc ? node.loc.start.column : 0;
    insFact.run(bundle, rel, line, col, kind, name || null, value == null ? null : String(value).slice(0, 2000));
    insSearch.run(name || "", value == null ? "" : String(value).slice(0, 2000), kind, bundle, rel, line);
    bump(kind);
  };

  const visit = (node, parent) => {
    if (!node || typeof node !== "object") return;
    if (Array.isArray(node)) return node.forEach((n) => visit(n, node));

    switch (node.type) {
      case "StringLiteral":
        emit("str", null, node.value, node);
        break;
      case "RegExpLiteral":
        emit("regex", null, node.pattern, node);
        break;
      case "TemplateLiteral":
        emit("tmpl", null, node.quasis.map((q) => q.value.cooked ?? q.value.raw).join("${}"), node);
        break;
      case "VariableDeclarator":
        if (node.id?.type === "Identifier") emit("bindef", node.id.name, null, node.id);
        break;
      case "FunctionDeclaration":
      case "ClassDeclaration":
      case "ClassExpression":
        // After renaming, the meaningful names live on *nested* function
        // declarations - `WorkspaceLeaf` is `function WorkspaceLeaf()` inside an
        // IIFE, not a top-level var - so a top-level-only index cannot find the
        // very names the renamer introduced.
        if (node.id?.name) emit("bindef", node.id.name, null, node.id);
        break;
      case "MemberExpression": {
        if (!node.computed && node.property.type === "Identifier") {
          emit(methodNameOf(node, parent) ? "methdef" : "prop", node.property.name, null, node.property);
        }
        break;
      }
      default:
        break;
    }
    for (const k of Object.keys(node)) {
      if (k === "loc" || k === "leadingComments" || k === "trailingComments") continue;
      const v = node[k];
      if (v && typeof v === "object" && (v.type || Array.isArray(v))) visit(v, node);
    }
  };

  for (const st of ast.program.body) {
    for (const id of declaredNames(st)) emit("decl", id.name, null, id);
    visit(st, null);
  }
  stat.files++;
  stat.bytes += text.length;
});

// Discover <bundle>/{own,vendor}/*.js under the root.
const bundles = fs
  .readdirSync(root, { withFileTypes: true })
  .filter((d) => d.isDirectory())
  .map((d) => d.name);

for (const b of bundles) {
  for (const sub of ["own", "vendor"]) {
    const dir = path.join(root, b, sub);
    if (!fs.existsSync(dir)) continue;
    const files = fs.readdirSync(dir).filter((f) => f.endsWith(".js")).sort();
    for (const f of files) walkFile(b, `${sub}/${f}`, path.join(dir, f));
    if (files.length) console.log(`  ${b}/${sub}: ${files.length} files`);
  }
}

db.exec("ANALYZE");
const size = fs.statSync(out).size;
console.log(`\nindex: ${stat.files} files, ${(stat.bytes / 1048576).toFixed(1)}MB source, ${stat.facts.toLocaleString()} facts, ${(size / 1048576).toFixed(1)}MB db -> ${out}`);
for (const [k, v] of Object.entries(kinds).sort((a, b) => b[1] - a[1])) console.log(`  ${k.padEnd(9)} ${v.toLocaleString()}`);
if (stat.parseFailures) console.log(`  WARNING: ${stat.parseFailures} files failed to parse`);