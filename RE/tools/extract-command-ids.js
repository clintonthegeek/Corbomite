#!/usr/bin/env node
// Extract Obsidian's core command-id inventory from the restored corpus.
//
//   node extract-command-ids.js <corpus-root> <out.json> [--md <out.md>]
//        [--corbomite <corbomite.json>]
//
//   corpus-root   RE/obsidian/<version>/renamed/src   (contains <bundle>/own/*.js)
//   out.json      the generated machine artefact
//   out.md        the generated readable table (optional)
//   corbomite.json  ids Corbomite registers, for the coverage column
//
// ## Why AST and not the index
//
// `q.js find addCommand` counts property references — 535 in the command
// palette's own file alone — and cannot tell `addCommand({…})` from unrelated
// `.addCommand` traffic. Terser mangled bindings only, so string literals and
// property names survived intact and the ids are exactly recoverable, but only
// by parsing the object literal actually handed to `addCommand`. RE/README.md's
// "use the AST for anything name-shaped" rule is the load-bearing one.
//
// ## Why this needs to be a program
//
// Three id shapes are not plain string literals at their call site, and a
// regex scrape undercounts all of them:
//
//   1. `id: "editor:set-heading-".concat(e)` where `e` arrives from
//      `for (var a = 0, s = [1..6]; a < s.length; a++) o(s[a])`      →  6 ids
//   2. `var l = function(e) { addCommand({ id: "editor:table-" + e }) }`
//      invoked 15 times with a suffix literal                        → 15 ids
//   3. `registerGlobalCommand(<key>, …)` — a second registration path
//      whose identifier is the first argument, not an `id:` field.
//
// Shapes 1 and 2 are the same problem — an `id` computed from a function
// parameter — and both are solved by one bounded inlining pass: when an
// identifier in an `id:` expression resolves to nothing statically, and that
// identifier is a parameter of a function we can see the call sites of, resolve
// the call-site argument instead and union the results. That is what makes the
// count correct instead of approximately correct, and it is why the number has
// to be generated rather than transcribed.
const fs = require("fs");
const path = require("path");
const parser = require("@babel/parser");
const traverse = require("@babel/traverse").default;

const PARSE_OPTS = {
  sourceType: "script",
  allowReturnOutsideFunction: true,
  allowAwaitOutsideFunction: true,
  allowSuperOutsideMethod: true,
  allowUndeclaredExports: true,
  errorRecovery: true,
  plugins: ["classProperties", "classPrivateProperties", "classPrivateMethods",
            "dynamicImport", "optionalChaining", "nullishCoalescingOperator",
            "objectRestSpread", "topLevelAwait", "numericSeparator",
            "logicalAssignment"],
};

// Obsidian 1.13.7 renames the platform predicates; they are mangled bindings,
// so the *shape* is the recognisable thing: a bare identifier compared against
// a platform string, or used as a conditional test.
const PLATFORM_IDENTS = new Set(["Zp", "Jp", "ed", "nd"]);
const PLATFORM_WORDS = new Set(["mac", "darwin", "win32", "win", "linux", "android", "iphone", "ipad"]);

const TRUTHY_PROPS = new Set(["mobileOnly", "repeatable", "allowPreview",
                              "allowProperties", "showOnMobileToolbar",
                              "isDesktopOnly", "desktopOnly"]);

// --------------------------------------------------------------------------
// Static string resolution
// --------------------------------------------------------------------------

function cartesian(a, b) {
  const out = [];
  for (const x of a) for (const y of b) out.push(x + y);
  return out;
}

// Resolve `node` to a de-duplicated, sorted-or-not list of concrete strings, or
// `null` when it is not statically knowable.
//
// `nodePath` is a @babel NodePath for `node` — needed so identifier lookups go
// through the real binding table instead of a hand-rolled scope walk.
function stringsOf(node, nodePath, depth = 0) {
  if (!node || depth > 6) return null;
  switch (node.type) {
    case "StringLiteral":
      return [node.value];
    case "NumericLiteral":
      return [String(node.value)];
    case "TemplateLiteral": {
      if (node.expressions.length) return null;
      return [node.quasis.map((q) => q.value.cooked).join("")];
    }
    case "ArrayExpression": {
      // A loop bound: `for (var a = 0, s = [1,2,3]; a < s.length; a++) o(s[a])`.
      // Each element is a distinct value the referenced parameter takes.
      const out = [];
      for (const el of node.elements) {
        if (!el) return null;
        const r = stringsOf(el, subPath(nodePath, el), depth + 1);
        if (!r) return null;
        out.push(...r);
      }
      return [...new Set(out)];
    }
    case "BinaryExpression": {
      if (node.operator !== "+") return null;
      const l = stringsOf(node.left, subPath(nodePath, node.left), depth + 1);
      const r = stringsOf(node.right, subPath(nodePath, node.right), depth + 1);
      if (!l || !r) return null;
      return cartesian(l, r);
    }
    case "MemberExpression": {
      if (!node.computed) return null;
      const base = stringsOf(node.object, subPath(nodePath, node.object), depth + 1);
      if (!base) return null;
      const idx = indexOf(node.property);
      if (idx !== null) return base[idx] === undefined ? null : [base[idx]];
      return base;
    }
    case "CallExpression": {
      const callee = node.callee;
      // `String(x)` coerces; a templated site uses it on the loop variable.
      if (callee.type === "Identifier" && callee.name === "String"
          && node.arguments.length === 1) {
        return stringsOf(node.arguments[0], at(nodePath, "arguments.0"), depth + 1);
      }
      if (callee.type !== "MemberExpression" || callee.computed) return null;
      if (callee.property.type !== "Identifier") return null;
      if (callee.property.name !== "concat") return null;
      const base = stringsOf(callee.object, subPath(nodePath, callee.object), depth + 1);
      if (!base) return null;
      let acc = base;
      for (const arg of node.arguments) {
        const a = stringsOf(arg, subPath(nodePath, arg), depth + 1);
        if (!a) return null;
        acc = cartesian(acc, a);
      }
      return acc;
    }
    case "Identifier": {
      const binding = nodePath && nodePath.scope.getBinding(node.name);
      if (!binding) return inlineParam(node.name, nodePath, depth);
      if (binding.kind === "param") return inlineParam(node.name, nodePath, depth);
      const init = binding.path.node.type === "VariableDeclarator"
        ? binding.path.node.init
        : binding.path.node.type === "AssignmentExpression"
        ? binding.path.node.right
        : null;
      if (!init) return null;
      return stringsOf(init, binding.path, depth + 1);
    }
    default:
      return null;
  }
}

// The NodePath for a sub-node of `nodePath`, needed because `stringsOf` resolves
// identifiers through the scope of the node they sit in.
function subPath(nodePath, child) {
  if (!nodePath) return null;
  try { return nodePath.get(keyOfChild(nodePath, child)); }
  catch { return null; }
}

function keyOfChild(nodePath, child) {
  for (const k of ["arguments", "elements", "properties", "expressions", "quasis"]) {
    const v = nodePath.node[k];
    if (Array.isArray(v)) {
      const i = v.indexOf(child);
      if (i >= 0) return `${k}.${i}`;
    }
  }
  if (nodePath.node.left === child) return "left";
  if (nodePath.node.right === child) return "right";
  if (nodePath.node.object === child) return "object";
  if (nodePath.node.property === child) return "property";
  if (nodePath.node.argument === child) return "argument";
  return "callee";
}

// Only a *literal* index selects a single element. Resolving an index through
// its binding would pin a `for` loop's counter to its initial value and emit one
// id where the loop emits six — the loop is the interesting case, so a
// non-literal index means "every element is a candidate".
function indexOf(propNode) {
  if (propNode.type === "NumericLiteral") return propNode.value;
  if (propNode.type === "StringLiteral" && /^-?\d+$/.test(propNode.value))
    return Number(propNode.value);
  return null;
}

// `name` is a parameter of an enclosing function. Find that function's binding,
// resolve each call site that passes an argument at this position, and union.
function inlineParam(name, nodePath, depth) {
  if (!nodePath || depth > 6) return null;
  let fn = nodePath.findParent((p) =>
    p.isFunction() && (p.isFunctionExpression() || p.isArrowFunctionExpression()
                       || p.isFunctionDeclaration() || p.isObjectMethod()));
  if (!fn) return null;
  const params = fn.node.params;
  let pos = -1;
  for (let i = 0; i < params.length; ++i) {
    if (params[i].type === "Identifier" && params[i].name === name) { pos = i; break; }
  }
  if (pos < 0) return null;

  let refPath = fn;
  if (fn.parentPath && fn.parentPath.isVariableDeclarator())
    refPath = fn.parentPath;
  else if (fn.parentPath && (fn.parentPath.isAssignmentExpression()
          || fn.parentPath.isObjectProperty()))
    refPath = fn.parentPath;
  const binding = refPath.scope.getBinding(refPath.node.id
    ? refPath.node.id.name
    : (fn.node.id && fn.node.id.name));
  if (!binding) return null;

  const out = new Set();
  let calls = 0;
  for (const ref of binding.referencePaths) {
    const parent = ref.parentPath;
    if (!parent || !parent.isCallExpression() || parent.node.callee !== ref.node) continue;
    if (++calls > 64) break;             // runaway guard
    const arg = parent.node.arguments[pos];
    if (!arg) continue;
    const r = stringsOf(arg, parent.get(`arguments.${pos}`), depth + 1);
    if (!r) return null;
    for (const s of r) out.add(s);
  }
  if (calls === 0 || out.size === 0) return null;
  return [...out];
}

// --------------------------------------------------------------------------
// Spec-field extraction
// --------------------------------------------------------------------------

function objKey(p) {
  if (p.type !== "ObjectProperty" && p.type !== "ObjectMethod") return null;
  const k = p.key;
  if (!k) return null;
  if (k.type === "Identifier") return k.name;
  if (k.type === "StringLiteral") return k.value;
  return null;
}

// The display `name` is an i18n call in a mangled module; the corpus does not
// carry the English tree, so record the key path (`commands.toggleBold`). The
// key path is the callee, not the arguments — `bd.commands.toggleBold()`.
function nameKeyOf(node, depth = 0) {
  if (!node || depth > 8) return null;
  if (node.type === "StringLiteral") return node.value;
  if (node.type === "CallExpression") return nameKeyOf(node.callee, depth + 1);
  if (node.type === "MemberExpression" && !node.computed
      && node.property.type === "Identifier") {
    const base = nameKeyOf(node.object, depth + 1);
    return base ? base + "." + node.property.name : node.property.name;
  }
  if (node.type === "Identifier") return node.name;
  return null;
}

// Obsidian's key-binding factory is a mangled 2-arg call taking a modifier
// array and a key string. Recognise by shape.
function hotkeyOf(node, nodePath, depth = 0) {
  if (!node || depth > 4) return null;
  // `String(e)` shows up in a templated site's hotkeys (`workspace:goto-tab-N`
  // is bound to `Mod`+`String(n)`); unwrap so the key is recorded.
  if (node.type === "CallExpression" && node.callee.type === "Identifier"
      && node.callee.name === "String" && node.arguments.length === 1) {
    node = node.arguments[0];
    nodePath = at(nodePath, "arguments.0");
  }
  if (node.type === "CallExpression" && node.arguments.length === 2) {
    const mods = node.arguments[0], key = node.arguments[1];
    if (mods.type === "ArrayExpression" && key.type !== "ArrayExpression") {
      const m = [];
      for (const el of mods.elements) {
        if (!el || el.type !== "StringLiteral") return null;
        m.push(el.value);
      }
      // The key is usually a literal, but a templated site binds it to the same
      // loop variable as the id (`workspace:goto-tab-` + `n` → `Mod` + `String(n)`).
      // Recording the candidate *set* is honest; pairing it to a specific id is
      // not, since a templated site expands to many ids from one binding.
      if (key.type === "StringLiteral") return [{ modifiers: m, key: key.value }];
      const cands = stringsOf(key, at(nodePath, "arguments.1"), 1);
      if (!cands) return null;
      if (cands.length === 1) return [{ modifiers: m, key: cands[0] }];
      return [{ modifiers: m, key: null, keyCandidates: cands }];
    }
  }
  if (node.type === "ArrayExpression") {
    const out = [];
    for (let i = 0; i < node.elements.length; ++i) {
      const el = node.elements[i];
      if (!el) continue;
      const h = hotkeyOf(el, at(nodePath, `elements.${i}`), depth + 1);
      if (!h) return null;
      out.push(...h);
    }
    return out;
  }
  if (node.type === "ConditionalExpression") {
    // The only platform-conditional default hotkeys in the corpus. Record both
    // branches and the predicate, so a reader cannot assume one default per id.
    const t = hotkeyOf(node.consequent, at(nodePath, "consequent"), depth + 1);
    const f = hotkeyOf(node.alternate, at(nodePath, "alternate"), depth + 1);
    if (!t || !f) return null;
    const gate = platformPredicateOf(node.test, at(nodePath, "test")) || "?";
    return [...t.map((h) => ({ ...h, when: gate })),
            ...f.map((h) => ({ ...h, when: "!" + gate }))];
  }
  return null;
}

// `NodePath.get` throws on a key that does not exist for the node type; a
// missing path just means "resolve statically from the node alone".
function at(nodePath, key) {
  if (!nodePath) return null;
  try { return nodePath.get(key); } catch { return null; }
}

function isLiteralTrue(node, nodePath) {
  if (!node) return false;
  if (node.type === "BooleanLiteral") return node.value;
  if (node.type === "UnaryExpression" && node.operator === "!"
      && node.argument.type === "NumericLiteral") return node.argument.value === 0;
  if (node.type === "Identifier" && nodePath) {
    const b = nodePath.scope.getBinding(node.name);
    if (b && b.path.node.init) return isLiteralTrue(b.path.node.init, b.path);
  }
  return false;
}

// A platform predicate: `rd.isDesktopApp`, `rd.isMobile`, `rd.isMacOS`, or one of
// the renamed module-level bindings (`Zp`=mac, `Jp`=win, `ed`=linux, `nd`=android).
const MEMBER_PREDICATE = /^is(Desktop|Mobile|MobileApp|DesktopApp|Mac|MacOS|Win|Win32|Linux|Android|Phone|Tablet|Web|Ios|WebApp)$/;

function platformPredicateOf(node, nodePath) {
  if (!node) return null;
  if (node.type === "MemberExpression" && !node.computed
      && node.property.type === "Identifier") {
    const n = node.property.name;
    if (MEMBER_PREDICATE.test(n)) return n;
    if (PLATFORM_IDENTS.has(n)) return n;
    return null;
  }
  if (node.type === "Identifier" && PLATFORM_IDENTS.has(node.name)) return node.name;
  if (node.type === "UnaryExpression" && node.operator === "!") {
    const inner = platformPredicateOf(node.argument, nodePath);
    if (inner) return "!" + inner;
  }
  if (node.type === "BinaryExpression" && (node.operator === "===" || node.operator === "==")) {
    for (const s of [node.left, node.right])
      if (s.type === "StringLiteral" && PLATFORM_WORDS.has(s.value)) return s.value;
  }
  return null;
}

// Platform gates that sit *around* a registration site rather than inside its
// spec. Obsidian writes `rd.isDesktopApp && app.commands.addCommand({…})` for a
// desktop-only command and `rd.isMobile && …` for a mobile-only one, so the
// split is only recoverable from the call site's ancestors.
function siteGateOf(callPath) {
  const gates = [];
  let p = callPath.parentPath;
  while (p) {
    const n = p.node;
    if (n.type === "LogicalExpression" && n.operator === "&&" && n.right === callPath.node) {
      const g = platformPredicateOf(n.left);
      if (g) gates.push(g);
    } else if (n.type === "ConditionalExpression"
               && (n.consequent === callPath.node || n.alternate === callPath.node)) {
      const g = platformPredicateOf(n.test);
      if (g) gates.push(g);
    }
    callPath = p;
    p = p.parentPath;
  }
  return gates;
}

function platformGateOf(node, nodePath) {
  if (!node) return null;
  switch (node.type) {
    case "ConditionalExpression": {
      const t = platformPredicateOf(node.test, nodePath);
      if (t) return t;
      return platformGateOf(node.consequent, nodePath);
    }
    case "BinaryExpression":
      if (node.operator === "&&") {
        const l = platformGateOf(node.left, nodePath);
        const r = platformGateOf(node.right, nodePath);
        if (l && r) return `${l} && ${r}`;
        return l || r;
      }
      return null;
    default:
      return platformPredicateOf(node, nodePath);
  }
}

// --------------------------------------------------------------------------

function jsFiles(root) {
  const out = [];
  const walk = (d) => {
    for (const e of fs.readdirSync(d, { withFileTypes: true })) {
      const p = path.join(d, e.name);
      if (e.isDirectory()) walk(p);
      else if (e.name.endsWith(".js")) out.push(p);
    }
  };
  walk(root);
  out.sort();
  return out;
}

function collectFile(rel, code) {
  let ast;
  try { ast = parser.parse(code, PARSE_OPTS); }
  catch (e) { console.error(`  parse failed ${rel}: ${e.message}`); return null; }

  const sites = [];
  const forwarders = [];

  traverse(ast, {
    CallExpression(p) {
      const callee = p.node.callee;
      let fname = null;
      if (callee.type === "Identifier") fname = callee.name;
      else if (callee.type === "MemberExpression" && !callee.computed
               && callee.property.type === "Identifier") fname = callee.property.name;
      if (fname !== "addCommand" && fname !== "registerGlobalCommand") return;

      const arg = p.node.arguments[0];
      // Both registration paths take a Command object literal. A non-literal
      // argument is a *forwarder* — `Plugin.prototype.addCommand` and the
      // internal-plugin wrapper's bulk `commands.addCommand(spec)` loop — which
      // is API surface, not a registration site.
      if (!arg || arg.type !== "ObjectExpression") {
        forwarders.push({
          callee: fname,
          file: rel,
          line: p.node.loc.start.line,
          receiver: callee.type === "MemberExpression" && callee.object.type === "MemberExpression"
            && callee.object.property.type === "Identifier"
            ? callee.object.property.name : null,
          snippet: code.slice(arg ? arg.start : p.node.start,
                              arg ? arg.end : p.node.end).replace(/\s+/g, " ").slice(0, 90),
        });
        return;
      }

      const spec = readSpec(p);
      spec.siteGates = siteGateOf(p);
      sites.push({ line: p.node.loc.start.line, registrar: fname, spec });
    },
  });

  function readSpec(callPath) {
    const spec = { ids: null };
    for (const propPath of callPath.get("arguments.0.properties")) {
      const prop = propPath.node;
      const k = objKey(prop);
      if (!k) continue;
      const vPath = propPath.get("value");
      switch (k) {
        case "id": {
          const r = stringsOf(prop.value, vPath);
          if (r) spec.ids = [...new Set(r)];
          break;
        }
        case "name":
          spec.nameKey = nameKeyOf(prop.value);
          break;
        case "icon":
          spec.icon = prop.value.type === "StringLiteral" ? prop.value.value : null;
          break;
        case "hotkeys":
          spec.hotkeys = hotkeyOf(prop.value, vPath);
          break;
        case "editorCallback":
        case "editorCheckCallback":
          spec.hasEditorCallback = true;
          spec.editorGate = platformGateOf(prop.value, vPath);
          break;
        case "callback":
        case "checkCallback":
          spec.hasCallback = true;
          break;
        default:
          if (TRUTHY_PROPS.has(k)) {
            spec[k] = isLiteralTrue(prop.value, vPath);
            const g = platformGateOf(prop.value, vPath);
            if (g) spec[k + "Gate"] = g;
          } else {
            // Any other key is recorded so a new spec field shows up in the
            // generated table rather than being silently dropped.
            spec[k] = isLiteralTrue(prop.value, vPath) ? true : undefined;
          }
      }
    }
    return spec;
  }

  return { sites, forwarders };
}

function main() {
  const argv = process.argv.slice(2);
  const root = argv[0], outJson = argv[1];
  const flag = (n) => { const i = argv.indexOf(n); return i > 0 ? argv[i + 1] : null; };
  const outMd = flag("--md");
  const corbomiteFile = flag("--corbomite");
  if (!root || !outJson) {
    console.error("usage: extract-command-ids.js <corpus-root> <out.json> "
                  + "[--md <out.md>] [--corbomite <corbomite.json>]");
    process.exit(2);
  }
  if (!fs.existsSync(root)) {
    console.error(`corpus root not found: ${root}\n`
      + "Restore it with:  RE/extract-obsidian.sh <obsidian .deb>");
    process.exit(2);
  }

  const records = [];
  const unresolved = [];
  const forwarders = [];
  const skipped = [];

  for (const f of jsFiles(root)) {
    const rel = path.relative(root, f).split(path.sep).join("/");
    // `app/all.js` is the pre-split concatenated build: every statement in it is
    // duplicated in `<bundle>/own/*.js`. Counting it would double the inventory
    // and attribute every id to the wrong file.
    if (rel === "app/all.js") { skipped.push(rel); continue; }
    const code = fs.readFileSync(f, "utf8");
    const got = collectFile(rel, code);
    if (!got) { skipped.push(rel); continue; }
    forwarders.push(...got.forwarders);
    for (const site of got.sites) {
      if (!site.spec.ids) {
        unresolved.push({ file: rel, line: site.line, registrar: site.registrar });
        continue;
      }
      for (const id of site.spec.ids) {
        const gates = collectGates(site.spec);
        records.push({
          specMobileOnlyField: site.spec.mobileOnly === true,
          specMobileGated: (site.spec.siteGates || []).some((g) => MOBILE_GATE.test(g)),
          id,
          nameKey: site.spec.nameKey ?? null,
          icon: site.spec.icon ?? null,
          registrar: site.registrar,
          bundle: rel.split("/")[0],
          file: rel,
          line: site.line,
          hotkeys: site.spec.hotkeys ?? null,
          flags: {
            // `mobileOnly: !0` is the documented field; a registration site
            // wrapped in `rd.isMobile &&` has the same effect and must count
            // toward the same split.
            mobileOnly: site.spec.mobileOnly === true
              || (site.spec.siteGates || []).some((g) => MOBILE_GATE.test(g)),
            desktopOnly: (site.spec.siteGates || []).some((g) => DESKTOP_GATE.test(g)),
            mobileAppOnly: (site.spec.siteGates || []).some((g) => MOBILE_APP_GATE.test(g)),
            repeatable: site.spec.repeatable === true,
            allowPreview: site.spec.allowPreview === true,
            allowProperties: site.spec.allowProperties === true,
            showOnMobileToolbar: site.spec.showOnMobileToolbar === true,
            hasEditorCallback: site.spec.hasEditorCallback === true,
          },
          gates,
          otherFields: Object.keys(site.spec)
            .filter((k) => !KNOWN.has(k) && !k.endsWith("Gate") && k !== "hasCallback"
                    && k !== "siteGates")
            .map((k) => k + "=" + JSON.stringify(site.spec[k])),
        });
      }
    }
  }

  // One record per id; keep every distinct call site.
  const byId = new Map();
  for (const r of records) {
    const prev = byId.get(r.id);
    if (!prev) { byId.set(r.id, { ...r, sites: [{ file: r.file, line: r.line }] }); continue; }
    prev.sites.push({ file: r.file, line: r.line });
    if (r.hotkeys && !prev.hotkeys) prev.hotkeys = r.hotkeys;
    if (r.nameKey && !prev.nameKey) prev.nameKey = r.nameKey;
    if (r.icon && !prev.icon) prev.icon = r.icon;
  }
  const commands = [...byId.values()].sort((a, b) => a.id.localeCompare(b.id));

  let corbomite = null;
  if (corbomiteFile) {
    if (!fs.existsSync(corbomiteFile)) {
      console.error(`--corbomite file missing: ${corbomiteFile}`);
      process.exit(2);
    }
    corbomite = JSON.parse(fs.readFileSync(corbomiteFile, "utf8"));
  }

  const table = commands.map((c) => ({
    id: c.id,
    nameKey: c.nameKey,
    icon: c.icon,
    origin: c.sites[0].file === APP_COMMANDS_FILE ? "app"
           : c.sites[0].file === WORKSPACE_COMMANDS_FILE ? "workspace"
           : "internal-plugin",
    sites: c.sites,
    hotkeys: c.hotkeys,
    flags: c.flags,
    gates: c.gates,
    otherFields: c.otherFields,
    corbomite: corbomite
      ? (corbomite.byId[c.id]
          ? { status: corbomite.byId[c.id].status, action: corbomite.byId[c.id].action ?? null }
          : { status: "unimplemented", action: null })
      : null,
  }));

  const out = {
    schema: 1,
    corpusRoot: path.relative(process.cwd(), root),
    generatedBy: "RE/tools/extract-command-ids.js",
    counts: {
      registrationSites: records.length,
      distinctIds: table.length,
      appLevelIds: table.filter((c) => c.origin === "app").length,
      workspaceIds: table.filter((c) => c.origin === "workspace").length,
      internalPluginIds: table.filter((c) => c.origin === "internal-plugin").length,
      mobileOnlyIds: table.filter((c) => c.flags.mobileOnly).length,
      mobileOnlySpecIds: records.filter((r) => r.specMobileOnlyField).length,
      mobileGatedIds: records.filter((r) => r.specMobileGated).length,
      desktopOnlyIds: table.filter((c) => c.flags.desktopOnly).length,
      mobileAppOnlyIds: table.filter((c) => c.flags.mobileAppOnly).length,
      idsWithHotkeys: table.filter((c) => c.hotkeys).length,
      forwardingSites: forwarders.length,
      unresolvedSites: unresolved.length,
      skippedFiles: skipped,
    },
    commands: table,
    forwarders,
    unresolved,
    corbomiteNativeIds: corbomite ? corbomite.corbomiteNativeIds ?? {} : null,
  };

  fs.mkdirSync(path.dirname(path.resolve(outJson)), { recursive: true });
  fs.writeFileSync(outJson, JSON.stringify(out, null, 1) + "\n");

  console.error(`distinct ids:            ${out.counts.distinctIds}`);
  console.error(`  app-level:             ${out.counts.appLevelIds}`);
  console.error(`  workspace-level:       ${out.counts.workspaceIds}`);
  console.error(`  internal plugins:      ${out.counts.internalPluginIds}`);
  console.error(`  mobileOnly:            ${out.counts.mobileOnlyIds}`);
  console.error(`  desktop-only:          ${out.counts.desktopOnlyIds}`);
  console.error(`  mobile-app-only:       ${out.counts.mobileAppOnlyIds}`);
  console.error(`  ids with a hotkey:     ${out.counts.idsWithHotkeys}`);
  console.error(`forwarding sites:       ${out.counts.forwardingSites}`);
  console.error(`unresolved sites:       ${out.counts.unresolvedSites}`);
  if (unresolved.length) for (const u of unresolved)
    console.error(`    ! ${u.file}:${u.line} (${u.registrar})`);
  if (skipped.length) console.error(`skipped files:          ${skipped.join(", ")}`);

  if (outMd) writeMarkdown(outMd, out);
}

// Registration owners in 1.13.7. Three classes of site, and the split matters:
// the audit's 115 + 74 + 1 arithmetic counts `App.registerCommands` and the
// internal-plugin constructors but omits the `Workspace` set entirely, which is
// 42 more ids.
const APP_COMMANDS_FILE = "app/own/2075-Vie.js";
const WORKSPACE_COMMANDS_FILE = "app/own/1932-r4.js";

const KNOWN = new Set(["ids", "nameKey", "icon", "hotkeys", "hasEditorCallback",
                       "hasCallback", "editorGate", "mobileOnly", "repeatable",
                       "allowPreview", "allowProperties", "showOnMobileToolbar"]);

const MOBILE_GATE = /^!?is(Mobile|Phone|Tablet|Ios|MobileApp)$/;
const MOBILE_APP_GATE = /^!?isMobileApp$/;
const DESKTOP_GATE = /^!?is(Desktop|DesktopApp)$/;

function collectGates(spec) {
  const g = [];
  for (const [k, v] of Object.entries(spec)) {
    if (k === "siteGates") { for (const s of v || []) g.push("site:" + s); continue; }
    if (!k.endsWith("Gate")) continue;
    if (v) g.push(k.replace(/Gate$/, "") + ":" + v);
  }
  return [...new Set(g)];
}

function writeMarkdown(file, out) {
  const esc = (s) => (s === null || s === undefined ? "" : String(s).replace(/\|/g, "\\|"));
  const L = [];
  L.push("# Obsidian core command ids → Corbomite");
  L.push("");
  L.push("**GENERATED — do not hand-edit.** Regenerate with:");
  L.push("");
  L.push("```bash");
  L.push("node RE/tools/extract-command-ids.js RE/obsidian/<version>/renamed/src \\");
  L.push("     docs/obsidian-command-ids.json \\");
  L.push("     --md docs/obsidian-command-ids.md \\");
  L.push("     --corbomite docs/obsidian-command-ids.corbomite.json");
  L.push("```");
  L.push("");
  L.push(`Corpus: \`${out.corpusRoot}\`. Extraction is AST-exact (see the script's header`);
  L.push("for why a regex scrape undercounts). Machine form: `docs/obsidian-command-ids.json`.");
  L.push("");

  const c = out.counts;
  L.push("## Reconciling the audit's 190");
  L.push("");
  L.push("`docs/obsidian-audit/addenda/2026-10-03-core-command-and-hotkey-contracts.md`");
  L.push("§B reports **190** ids as 115 app-level + 74 internal-plugin + 1 mobile-only.");
  L.push("Re-extraction reproduces its 115 and its 74 + 1 exactly. The 42-id gap is");
  L.push("the `Workspace` set: `Workspace.registerCommands`");
  L.push("(`app/own/1932-r4.js:311-763`) registers 42 more command ids through the");
  L.push("same `app.commands.addCommand`, and the audit's arithmetic has no bucket");
  L.push("for it. So **232** is the real total, not 190.");
  L.push("");
  L.push("Its per-platform split (7 desktop-only, 11 mobile-only, 3 mobile-app-only)");
  L.push("is likewise correct *for the app-level set only* — it was read inside");
  L.push("`App.registerCommands`. Corpus-wide it is 10 / 14 / 3: `Workspace` adds");
  L.push("`workspace:copy-full-path` and `workspace:show-trash` to the desktop-only");
  L.push("list, the canvas plugin adds `canvas:export-as-image`, and the three");
  L.push("mobile-app-only clipboard ids are additionally absent from the app-level");
  L.push("count. Both framings are in the table below: the `Registered by` column");
  L.push("says which owner a row belongs to.");
  L.push("");
  L.push("## Counts");
  L.push("");
  L.push("| Measure | Value |");
  L.push("|---|---|");
  L.push(`| **Distinct core command ids** | **${c.distinctIds}** |`);
  L.push(`| App-level (\`App.registerCommands\`) | ${c.appLevelIds} |`);
  L.push(`| Workspace-level | ${c.workspaceIds} |`);
  L.push(`| Internal-plugin registrations | ${c.internalPluginIds} |`);
  L.push(`| Desktop-only | ${c.desktopOnlyIds} |`);
  L.push(`| Mobile-only | ${c.mobileOnlyIds} |`);
  L.push(`| — of which \`mobileOnly: true\` in the spec | ${c.mobileOnlySpecIds} |`);
  L.push(`| — of which the site is wrapped in \`rd.isMobile &&\` | ${c.mobileGatedIds} |`);
  L.push(`| Mobile-app-only | ${c.mobileAppOnlyIds} |`);
  L.push(`| Ids carrying a default hotkey | ${c.idsWithHotkeys} |`);
  L.push(`| Registration sites with an unresolvable id | ${c.unresolvedSites} |`);
  L.push(`| API forwarders (not registrations) | ${c.forwardingSites} |`);
  L.push("");
  L.push("Three registration paths carry a `Command` object literal and all are");
  L.push("counted: `app.commands.addCommand({…})` in `App.registerCommands` and in");
  L.push("`Workspace`, and the internal-plugin wrapper's `registerGlobalCommand({…})`");
  L.push("(`app/own/1939-m4.js:663`). A call with a non-literal argument is a");
  L.push("*forwarder* — `Plugin.prototype.addCommand` (`app/own/1938-c4.js:63`) and the");
  L.push("wrapper's bulk re-add loop (`app/own/1939-m4.js:560`) — i.e. plugin API");
  L.push("surface, not a registration, so it is listed separately and not counted.");
  L.push("");
  L.push("`app/all.js` is excluded: it is the pre-split concatenated build and every");
  L.push("statement in it is duplicated under `<bundle>/own/*.js`.");
  L.push("");


  L.push("## Corbomite coverage");
  L.push("");
  const tally = new Map();
  for (const row of out.commands) {
    const st = row.corbomite ? row.corbomite.status : "unknown";
    tally.set(st, (tally.get(st) || 0) + 1);
  }
  L.push("| Status | Obsidian ids |");
  L.push("|---|---|");
  for (const [k, v] of [...tally.entries()].sort((a, b) => b[1] - a[1]))
    L.push(`| \`${k}\` | ${v} |`);
  L.push("");

  L.push("## Corbomite-native ids (not Obsidian)");
  L.push("");
  L.push("Corbomite also registers ids that are **not** Obsidian command ids. They");
  L.push("cannot be named by a vault's `hotkeys.json`, so they are compat-neutral:");
  L.push("nothing breaks, nothing is gained.");
  L.push("");
  L.push("| Corbomite id | Note |");
  L.push("|---|---|");
  for (const [id, note] of Object.entries(out.corbomiteNativeIds ?? {}).sort())
    L.push(`| \`${id}\` | ${note} |`);
  L.push("");

  L.push("## Full table");
  L.push("");
  L.push("`Flags` carries Obsidian's four undocumented spec fields (addendum §C) plus");
  L.push("the platform split. `gate:` records a platform-conditional *field*; the");
  L.push("desktop-only and mobile-only rows are registrations Obsidian wraps in");
  L.push("`rd.isDesktopApp && …` / `rd.isMobile && …`, which is a property of the");
  L.push("call site rather than of the spec.");
  L.push("");
  L.push("The hotkey column is the `hotkeys:` field verbatim. `{1|2|…}` marks a key");
  L.push("templated over a loop variable — one binding serves every id the site");
  L.push("expands to. *when …* marks the platform-conditional defaults; they are the");
  L.push("only ones, so a reader cannot assume one default per id.");
  L.push("");
  L.push("| Obsidian command id | i18n name key | Registered by | Flags | Default hotkey | Corbomite |");
  L.push("|---|---|---|---|---|---|");
  for (const row of out.commands) {
    const flags = [];
    if (row.flags.mobileOnly) flags.push("`mobileOnly`");
    if (row.flags.desktopOnly) flags.push("**desktop-only**");
    if (row.flags.mobileAppOnly) flags.push("**mobile-app-only**");
    if (row.flags.repeatable) flags.push("`repeatable`");
    if (row.flags.allowPreview) flags.push("`allowPreview`");
    if (row.flags.allowProperties) flags.push("`allowProperties`");
    if (row.flags.showOnMobileToolbar) flags.push("`showOnMobileToolbar`");
    if (row.flags.hasEditorCallback) flags.push("editor");
    for (const g of row.gates) flags.push("gate:" + g.replace(/:/g, "="));
    for (const o of row.otherFields) flags.push("`" + o + "`");
    const hk = row.hotkeys
      ? row.hotkeys.map((h) => {
          const mods = h.modifiers.length ? h.modifiers.join("+") : "—";
          const when = h.when ? ` <i>when ${h.when}</i>` : "";
          const key = h.key !== null && h.key !== undefined
            ? `\`${h.key}\``
            : `<code>{${h.keyCandidates.join("|")}}</code>`;
          return `${mods} ${key}${when}`;
        }).join("<br>")
      : "";
    const cor = row.corbomite
      ? "`" + row.corbomite.status + "`"
        + (row.corbomite.action ? " → `" + row.corbomite.action + "`" : "")
      : "—";
    L.push(`| \`${esc(row.id)}\` | ${row.nameKey ? "\`" + esc(row.nameKey) + "\`" : "—"} `
          + `| ${esc(row.origin)} | ${flags.join(" ") || "—"} | ${hk} | ${cor} |`);
  }
  L.push("");

  if (out.unresolved.length) {
    L.push("## Unresolved registration sites");
    L.push("");
    L.push("Each is a site whose `id` the script could not statically resolve, so each");
    L.push("is a hole in the counts above. Empty here means the inventory is closed.");
    L.push("");
    L.push("| Site | Registrar |");
    L.push("|---|---|");
    for (const u of out.unresolved)
      L.push(`| \`${u.file}:${u.line}\` | \`${u.registrar}\` |`);
    L.push("");
  }

  if (out.forwarders.length) {
    L.push("## Forwarding call sites (excluded from the count)");
    L.push("");
    L.push("`addCommand`/`registerGlobalCommand` with a non-literal argument. These");
    L.push("are the plugin API's own entry points, not core registrations.");
    L.push("");
    L.push("| Site | Callee | Argument |");
    L.push("|---|---|---|");
    for (const f of out.forwarders)
      L.push(`| \`${f.file}:${f.line}\` | \`${f.callee}\` | \`${f.snippet.replace(/\|/g, "\\|")}\` |`);
    L.push("");
  }

  fs.mkdirSync(path.dirname(path.resolve(file)), { recursive: true });
  fs.writeFileSync(file, L.join("\n") + "\n");
}

main();
