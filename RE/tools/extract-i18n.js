#!/usr/bin/env node
// Extract Obsidian's i18n key tree from i18n.js into readable form.
//
//   node extract-i18n.js <i18n.js> <out.js>
//
// i18n.js is a single IIFE assigning `window.OBSIDIAN_DEFAULT_I18N = { … }`.
// Every user-visible string in the app hangs off that object, so it is both a
// feature taxonomy and a completeness check: a feature with no strings here is
// either untranslated or not implemented. It is also one of the few parts of the
// bundle that needs no deminification at all to be useful.
const fs = require("fs");
const parser = require("@babel/parser");

const [inp, outp] = process.argv.slice(2);
if (!inp || !outp) {
  console.error("usage: extract-i18n.js <i18n.js> <out.js>");
  process.exit(2);
}

const src = fs.readFileSync(inp, "utf8");
const ast = parser.parse(src, { sourceType: "script", errorRecovery: true, ranges: true });

let root = null;
(function walk(n) {
  if (!n || typeof n !== "object") return;
  if (Array.isArray(n)) return n.forEach(walk);
  if (
    n.type === "AssignmentExpression" &&
    n.left.type === "MemberExpression" &&
    n.left.object.type === "Identifier" &&
    n.left.object.name === "window" &&
    n.left.property.name === "OBSIDIAN_DEFAULT_I18N"
  )
    root = n.right;
  for (const k of Object.keys(n)) {
    if (k === "loc") continue;
    const v = n[k];
    if (v && typeof v === "object" && (v.type || Array.isArray(v))) walk(v);
  }
})(ast);

if (!root || root.type !== "ObjectExpression") {
  console.error("OBSIDIAN_DEFAULT_I18N object literal not found");
  process.exit(1);
}

const render = (node, indent) => {
  if (node.type !== "ObjectExpression") return `<${node.type}>`;
  const inner = indent + "  ";
  const body = node.properties
    .map((p) => {
      const key = JSON.stringify(p.key.name ?? p.key.value);
      if (p.value.type === "ObjectExpression") return `${inner}${key}: ${render(p.value, inner)}`;
      if (p.value.type === "StringLiteral") return `${inner}${key}: ${JSON.stringify(p.value.value)}`;
      // Non-literal leaves are locale-parameterised callables; keep the call text
      // so the key is still documented rather than dropped.
      return `${inner}${key}: <${src.slice(p.value.start, Math.min(p.value.start + 70, p.value.end)).replace(/\s+/g, " ")}…>`;
    })
    .join(",\n");
  return `{\n${body}\n${indent}}`;
};

fs.mkdirSync(require("path").dirname(outp), { recursive: true });
fs.writeFileSync(outp, render(root, "") + "\n");

let leaves = 0;
(function count(n) {
  if (!n || typeof n !== "object") return;
  if (Array.isArray(n)) return n.forEach(count);
  if (n.type === "ObjectExpression") {
    for (const p of n.properties) {
      if (p.value.type === "ObjectExpression") count(p.value);
      else leaves++;
    }
  }
})(root);

const subs = root.properties.map((p) => `${p.key.name ?? p.key.value}(${p.value.properties ? p.value.properties.length : 0})`);
console.log(`i18n: ${subs.length} top-level subtrees, ${leaves} leaf keys -> ${outp}`);
console.log(`  ${subs.join(" ")}`);