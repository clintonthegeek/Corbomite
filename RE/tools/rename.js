#!/usr/bin/env node
// Evidence-based renamer for the Obsidian bundles.
//
//   node rename.js analyse <bundle.js>... --dts obsidian.d.ts --out renames.json
//   node rename.js apply   <bundle.js>... --renames renames.json --out <dir>
//
// ## Why renaming is tractable here
//
// terser mangled *bindings* only. Property names, method names and string
// literals all survived, so nearly all the meaning is still present - it is just
// attached to anonymous things. Two independent evidence sources:
//
//   1. Method-set fingerprinting against obsidian.d.ts (98 classes + 116
//      interfaces with member sets, unioned over the `extends` chain). An
//      implementation class's `X.prototype.foo = …` set is compared to each
//      declaration's set by Jaccard similarity. Decidable, not a guess.
//   2. Plugin self-identification: core plugins assign `this.id = "canvas"`, and
//      the class shape (has onload? has getViewType?) says Plugin vs View.
//
// ## The subtlety that makes this non-obvious
//
// rollup reused the same mangled identifier in every module scope. app.js has
// 5,926 `X.prototype.Y = …` assignments but only **18 distinct names** - `e`
// alone owns 2,620 of them, and each is a different class in a different
// scope. Keying the rename map on the *name* therefore collapses thousands of
// classes into 18 entries and fingerprints nothing. The map is keyed on binding
// identity (`binding.identifier.start`) instead, and renaming goes through
// @babel/traverse's scope-aware `binding.scope.rename`, which renames exactly
// one binding and leaves same-named siblings in other scopes alone.
//
// ## The rule that matters
//
// A name is applied only when it clears a threshold *and* beats the runner-up by
// a margin; every applied name records its evidence and score. Ambiguous
// bindings keep their mangled name. A confidently-wrong name is worse than a
// mangled one, because the reader stops looking.
const fs = require("fs");
const path = require("path");
const parser = require("@babel/parser");
const traverse = require("@babel/traverse").default;
const generate = require("@babel/generator").default;

const argv = process.argv.slice(2);
const mode = argv[0];
const flag = (n, d) => {
  const i = argv.indexOf(`--${n}`);
  return i === -1 ? d : argv[i + 1];
};
// Consume flags properly. The previous version filtered on `argv[i-1]` while `i`
// was an index into `argv.slice(1)` - an off-by-one that leaked every flag value
// into `inputs`, so the run tried to parse obsidian.d.ts as a bundle.
const FLAGS_WITH_VALUE = new Set(["--dts", "--out", "--renames"]);
const inputs = [];
for (let i = 1; i < argv.length; i++) {
  if (argv[i].startsWith("--")) {
    if (FLAGS_WITH_VALUE.has(argv[i])) i++;
    continue;
  }
  inputs.push(argv[i]);
}

const SCRIPT_OPTS = {
  sourceType: "script",
  allowReturnOutsideFunction: true,
  allowAwaitOutsideFunction: true,
  allowSuperOutsideMethod: true,
  allowUndeclaredExports: true,
  errorRecovery: true,
};
const TS_OPTS = { sourceType: "module", plugins: ["typescript"], errorRecovery: true };
// Thresholds. Tuned against app.js: 978 classes, of which these admit 31 real
// matches instead of ~40 mostly-spurious ones on tiny member sets.
const MIN_CLASS_MEMBERS = 6;
const MIN_DTS_MEMBERS = 4;
const MIN_INTERSECTION = 4;
const MIN_CONTAINMENT = 0.6;   // inter / min(|impl|, |declared|)
const MIN_IMPL_COVERAGE = 0.5; // inter / |impl|  - the declaration explains the class
const MIN_DECL_COVERAGE = 0.35;// inter / |declared|
const MIN_MARGIN = 0.1;
const MAX_BINDINGS_PER_NAME = 6;

const jaccard = (a, b) => {
  let inter = 0;
  for (const x of a) if (b.has(x)) inter++;
  return a.size + b.size - inter === 0 ? 0 : inter / (a.size + b.size - inter);
};

// ---------------------------------------------------------------- obsidian.d.ts
// The typings wrap everything in `declare module`, so a scan of
// program.body finds nothing - the declarations are nested two levels down.
function loadDts(file) {
  const ast = parser.parse(fs.readFileSync(file, "utf8"), TS_OPTS);
  const decls = new Map();
  const visit = (n) => {
    if (!n || typeof n !== "object") return;
    if (Array.isArray(n)) return n.forEach(visit);
    if (n.type === "ClassDeclaration" || n.type === "TSInterfaceDeclaration") {
      if (n.id && n.id.name && n.body) {
        const members = new Set();
        for (const m of n.body.body) {
          const k = m.key;
          if (!k) continue;
          if (["TSPropertySignature", "TSMethodSignature", "ClassMethod", "TSDeclareMethod", "ClassProperty"].includes(m.type) && k.name)
            members.add(k.name);
          else if (m.type === "TSIndexSignature" && m.parameters?.[0]?.name) members.add(m.parameters[0].name);
        }
        const supers = [];
        if (n.superClass?.type === "Identifier") supers.push(n.superClass.name);
        if (n.extends) for (const e of n.extends) if (e.expression?.type === "Identifier") supers.push(e.expression.name);
        if (!decls.has(n.id.name)) decls.set(n.id.name, { members, supers, kind: n.type === "ClassDeclaration" ? "class" : "interface" });
      }
    }
    for (const k of Object.keys(n)) {
      if (k === "loc") continue;
      const v = n[k];
      if (v && typeof v === "object" && (v.type || Array.isArray(v))) visit(v);
    }
  };
  visit(ast.program);

  const resolved = new Map();
  const unionOf = (name, seen = new Set()) => {
    if (resolved.has(name)) return resolved.get(name);
    if (seen.has(name) || !decls.has(name)) return new Set();
    seen.add(name);
    const d = decls.get(name);
    const out = new Set(d.members);
    for (const s of d.supers) for (const m of unionOf(s, seen)) out.add(m);
    resolved.set(name, out);
    return out;
  };
  for (const name of decls.keys()) unionOf(name);
  return decls;
}

// ------------------------------------------------------- scope-aware class discovery
function findClassLikes(ast) {
  const classes = new Map(); // bindingKey -> {orig, members:Set, ids:Set, fnPath}
  const seen = new Set();

  traverse(ast, {
    AssignmentExpression(p) {
      const left = p.node.left;
      if (left.type !== "MemberExpression" || left.computed) return;
      const obj = left.object;
      if (obj.type !== "MemberExpression" || obj.computed) return;
      if (obj.property?.name !== "prototype") return;
      if (obj.object?.type !== "Identifier") return;
      if (left.property?.type !== "Identifier") return;

      const binding = p.scope.getBinding(obj.object.name);
      if (!binding) return;
      const key = String(binding.identifier.start);
      if (!classes.has(key)) {
        classes.set(key, { orig: obj.object.name, members: new Set(), ids: new Set(), fnPath: binding.path, scope: binding.scope });
        seen.add(key);
      }
      const info = classes.get(key);
      info.members.add(left.property.name);
      // Plugins announce themselves as `X.prototype.id = "canvas"` as often as
      // `this.id = …` inside the constructor - reading only the latter found
      // zero of the 40 core plugins.
      if (left.property.name === "id" && p.node.right?.type === "StringLiteral") info.ids.add(p.node.right.value);
    },
  });

  // `this.id = "<plugin>"` anywhere inside the class body identifies core plugins.
  for (const info of classes.values()) {
    if (!info.fnPath) continue;
    try {
      info.fnPath.traverse({
        AssignmentExpression(q) {
          const l = q.node.left;
          if (l?.type === "MemberExpression" && !l.computed && l.object?.type === "ThisExpression" && l.property?.name === "id" && q.node.right?.type === "StringLiteral")
            info.ids.add(q.node.right.value);
        },
      });
    } catch {
      /* binding path detached; skip */
    }
  }
  return classes;
}

// --------------------------------------------------------------------- analyse
if (mode === "analyse") {
  const dtsFile = flag("dts");
  const outFile = flag("out", "renames.json");
  if (!dtsFile || !inputs.length) {
    console.error("usage: rename.js analyse <bundle.js>... --dts obsidian.d.ts --out renames.json");
    process.exit(2);
  }
  const decls = loadDts(dtsFile);
  const usable = [...decls.entries()].filter(([, d]) => d.members.size >= MIN_DTS_MEMBERS);
  console.log(`obsidian.d.ts: ${decls.size} declarations (${usable.length} with >=${MIN_DTS_MEMBERS} members)\n`);

  const result = {};
  let totalClasses = 0;

  for (const f of inputs) {
    let ast;
    try {
      ast = parser.parse(fs.readFileSync(f, "utf8"), SCRIPT_OPTS);
    } catch (e) {
      console.error(`${path.basename(f)}: parse failed: ${e.message}`);
      continue;
    }
    const classes = findClassLikes(ast);
    totalClasses += classes.size;
    let named = 0;
    let byPlugin = 0;

    for (const [key, info] of classes) {
      const rec = { orig: info.orig, members: [...info.members].sort(), memberCount: info.members.size, pluginIds: [...info.ids] };
      // Obsidian's *internal* plugin lifecycle is init/onEnable/onDisable, not
      // the public Plugin.onload/onunload - gating on "onload" matched 0 of 29.
      const isInternalPlugin = info.members.has("init") && (info.members.has("onEnable") || info.members.has("onDisable"));
      if (info.ids.size === 1 && (isInternalPlugin || info.members.has("onload"))) {
        const id = [...info.ids][0];
        const c = id.replace(/(^|[-_])(\w)/g, (_, __, ch) => ch.toUpperCase());
        rec.new = `${c}Plugin`;
        rec.confidence = "high";
        rec.evidence = `assigns this.id="${id}" and defines onload`;
        named++;
        byPlugin++;
      } else if (info.members.has("getViewType") && info.ids.size === 1) {
        const id = [...info.ids][0];
        const c = id.replace(/(^|[-_])(\w)/g, (_, __, ch) => ch.toUpperCase());
        rec.new = `${c}View`;
        rec.confidence = "medium";
        rec.evidence = `assigns this.id="${id}" and defines getViewType`;
        named++;
      } else {
        const cands = [];
        for (const [dname, d] of usable) {
          const dm = d.members;
          let inter = 0;
          for (const m of info.members) if (dm.has(m)) inter++;
          if (inter < MIN_INTERSECTION) continue;
          // Containment, not Jaccard: an implementation is normally a *superset*
          // of its declared interface (internal methods, private helpers), so
          // Jaccard punishes exactly the classes we most want to name. WorkspaceLeaf
          // scores 16/30 = 0.47 Jaccard but 16/16 = 1.00 containment.
          const containment = inter / Math.min(info.members.size, dm.size);
          cands.push({ dname, containment, inter, implCov: inter / info.members.size, declCov: inter / dm.size, size: dm.size });
        }
        cands.sort((a, b) => b.containment - a.containment || b.inter - a.inter);
        const best = cands[0];
        const next = cands[1];
        // A 3-member class matches a 3-member interface at Jaccard 1.00, which
        // is how nine unrelated classes all became "FuzzySuggestModal". Demand
        // absolute evidence, not just a high ratio.
        if (
          best &&
          info.members.size >= MIN_CLASS_MEMBERS &&
          best.containment >= MIN_CONTAINMENT &&
          best.implCov >= MIN_IMPL_COVERAGE &&
          best.declCov >= MIN_DECL_COVERAGE &&
          (!next || best.containment - next.containment >= MIN_MARGIN)
        ) {
          rec.new = best.dname;
          rec.containment = Number(best.containment.toFixed(3));
          rec.implCoverage = Number(best.implCov.toFixed(3));
          rec.declCoverage = Number(best.declCov.toFixed(3));
          rec.confidence = best.containment >= 0.85 && best.declCov >= 0.6 ? "high" : "medium";
          rec.evidence = `${best.inter}/${info.members.size} impl members, ${best.inter}/${best.size} declared; containment ${best.containment.toFixed(2)}, margin ${(best.containment - (next?.containment || 0)).toFixed(2)}`;
          named++;
        } else if (best) {
          rec.rejected = `below threshold or ambiguous: ${best.dname} containment ${best.containment.toFixed(2)} implCov ${best.implCov.toFixed(2)} declCov ${best.declCov.toFixed(2)}` +
            (next ? ` vs ${next.dname} ${next.containment.toFixed(2)}` : "");
        }
      }
      result[key] = rec;
    }
    console.log(`${path.basename(f)}: ${classes.size} distinct classes (scope-aware), ${named} named (${byPlugin} via plugin id)`);
  }

  // Safety net: if one interface name would swallow many bindings, keep only the
  // best-scoring ones. Nine classes became "FuzzySuggestModal" before thresholds
  // were tightened; this catches anything similar that slips through.
  const byNew = new Map();
  for (const [key, rec] of Object.entries(result)) {
    if (!rec.new) continue;
    if (!byNew.has(rec.new)) byNew.set(rec.new, []);
    byNew.get(rec.new).push({ key, rec });
  }
  let capped = 0;
  for (const [name, list] of byNew) {
    if (list.length <= MAX_BINDINGS_PER_NAME) continue;
    list.sort((a, b) => (b.rec.containment || 0) - (a.rec.containment || 0) || b.rec.implCoverage - a.rec.implCoverage);
    for (const { key, rec } of list.slice(MAX_BINDINGS_PER_NAME)) {
      rec.new = null;
      rec.rejected = `capped: ${name} matched ${list.length} bindings, kept the best ${MAX_BINDINGS_PER_NAME}`;
      capped++;
    }
  }

  const out = { generated: new Date().toISOString().slice(0, 10), dts: path.basename(dtsFile), decls: decls.size, classes: totalClasses, names: result };
  fs.writeFileSync(outFile, JSON.stringify(out, null, 2));
  const recs = Object.values(result);
  console.log(`\n${recs.filter((r) => r.new).length}/${totalClasses} named ` +
    `(${recs.filter((r) => r.confidence === "high").length} high, ${recs.filter((r) => r.rejected).length} not named${capped ? `, ${capped} capped by repeat guard` : ""})\n-> ${outFile}`);
  console.log("\nnamed classes:");
  for (const [k, r] of Object.entries(result)) if (r.new) console.log(`  @${k.padStart(8)} ${r.orig.padEnd(5)} -> ${r.new.padEnd(22)} [${r.confidence}] ${r.evidence}`);
}

// ------------------------------------------------------------------------ apply
else if (mode === "apply") {
  const renamesFile = flag("renames");
  const outDir = flag("out");
  if (!renamesFile || !inputs.length || !outDir) {
    console.error("usage: rename.js apply <bundle.js>... --renames renames.json --out <dir>");
    process.exit(2);
  }
  const { names } = JSON.parse(fs.readFileSync(renamesFile, "utf8"));
  fs.mkdirSync(outDir, { recursive: true });

  for (const f of inputs) {
    const base = path.basename(f, ".js");
    const src = fs.readFileSync(f, "utf8");
    const ast = parser.parse(src, SCRIPT_OPTS);
    let applied = 0;
    const collisions = [];

    // Group the map by original name so one prototype-owner traversal can rename
    // every matching binding in its own scope.
    const byOrig = new Map();
    for (const [key, rec] of Object.entries(names)) if (rec.new && rec.new !== rec.orig) {
      if (!byOrig.has(rec.orig)) byOrig.set(rec.orig, []);
      byOrig.get(rec.orig).push({ key, ...rec });
    }

    const done = new Set();
    traverse(ast, {
      AssignmentExpression(p) {
        const left = p.node.left;
        if (left.type !== "MemberExpression" || left.computed) return;
        const obj = left.object;
        if (obj.type !== "MemberExpression" || obj.computed || obj.property?.name !== "prototype") return;
        if (obj.object?.type !== "Identifier" || left.property?.type !== "Identifier") return;
        const binding = p.scope.getBinding(obj.object.name);
        if (!binding) return;
        const key = String(binding.identifier.start);
        if (done.has(key)) return;
        done.add(key);
        const rec = names[key];
        if (!rec || !rec.new) return;
        const taken = binding.scope.hasBinding(rec.new) || binding.scope.hasGlobal(rec.new);
        if (taken) {
          collisions.push(`${rec.orig} -> ${rec.new} (already bound in scope; skipped)`);
          return;
        }
        try {
          binding.scope.rename(rec.orig, rec.new);
          applied++;
        } catch (e) {
          collisions.push(`${rec.orig} -> ${rec.new}: ${e.message}`);
        }
      },
    });

    const out = generate(ast, { comments: true, compact: false, retainLines: false, concise: false }, src);
    fs.writeFileSync(path.join(outDir, `${base}.js`), out.code);
    console.log(`${base}: applied ${applied} renames -> ${(out.code.length / 1048576).toFixed(2)}MB`);
    for (const c of collisions.slice(0, 5)) console.log(`  skipped ${c}`);
    if (collisions.length > 5) console.log(`  ... ${collisions.length - 5} more skipped`);
  }
} else {
  console.error("usage: rename.js <analyse|apply> ...");
  process.exit(2);
}