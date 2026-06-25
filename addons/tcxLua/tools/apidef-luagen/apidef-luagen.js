#!/usr/bin/env node
// apidef-luagen.js — generate tcxLua (Sol2) Lua bindings from api-definition.yaml.
//
// PoC. Phase 1 = FREE FUNCTIONS only (the slice the libclang bindgen covers today).
// Reads the canonical YAML and emits a drop-in trussc_generated.cpp that defines
// tcxLua::setTrussCGeneratedBindings(). Type members (usertypes/operators) = Phase 2.
//
//   node apidef-luagen.js <api-definition.yaml> > trussc_generated.cpp
//   (coverage + skip report go to stderr)
//
// Design notes:
//  - Free function = a `categories[].functions[]` entry WITHOUT a `self:` field
//    (entries with `self:` are type members -> Phase 2).
//  - Lambda params are built by pairing the typed list (`params`) with the names
//    (`params_simple`); if a typed token lacks the name, the name is appended.
//    This needs the two to agree in arity — mismatches are reported & skipped
//    (they'd be a YAML data bug; the generated .cpp also wouldn't compile).
//  - Correctness is ultimately guaranteed by COMPILING this output against the
//    real headers (witness principle): a wrong name/type fails the build.

const fs = require('fs');
const yaml = require('js-yaml');

const yamlPath = process.argv[2];
if (!yamlPath) { console.error('usage: node apidef-luagen.js <api-definition.yaml>'); process.exit(1); }
const api = yaml.load(fs.readFileSync(yamlPath, 'utf8'));

const NS = 'trussc';
const warnings = [];

// Functions that are documented but NOT Lua-bindable as a plain lambda:
//  - template entry points (runApp<App>), etc.
// (Long-term this should be a `bindable: false` / lua-exclude flag in the YAML.)
const DENYLIST = new Set(['runApp', 'runHeadlessApp']);

// Split a C++ parameter list on top-level commas (ignore commas inside <> or ()).
function splitParams(s) {
    if (!s || !s.trim()) return [];
    const out = []; let depth = 0, cur = '';
    for (const ch of s) {
        if (ch === '<' || ch === '(') depth++;
        else if (ch === '>' || ch === ')') depth--;
        if (ch === ',' && depth === 0) { out.push(cur.trim()); cur = ''; }
        else cur += ch;
    }
    if (cur.trim()) out.push(cur.trim());
    return out;
}

// Pair typed tokens (params) with names (params_simple), expanding trailing
// default args into multiple arities. Returns an array of {decl, call} variants
// (one per callable arity), or {error}.
function buildLambdaVariants(paramsTyped, paramsSimple) {
    const types = splitParams(paramsTyped);
    const names = splitParams(paramsSimple);
    // Template free functions (T& / const T& / T&&) can't be bound as a plain lambda.
    if (/\bT\b/.test(paramsTyped)) return { error: `template signature (bare T): "${paramsTyped}"` };
    if (types.length === 0) return { variants: [{ decl: '', call: '' }] };
    if (types.length !== names.length) {
        return { error: `arity mismatch: "${paramsTyped}" (${types.length}) vs simple "${paramsSimple}" (${names.length})` };
    }
    const decls = [];           // typed param without default, e.g. "bool close"
    const cleanNames = [];      // arg names with optional-brackets stripped
    let firstDefault = types.length;
    for (let i = 0; i < types.length; i++) {
        let t = types[i];
        // params_simple marks optional args as "[name]" — strip the brackets.
        const n = names[i].replace(/^\[|\]$/g, '');
        if (!/^[A-Za-z_]\w*$/.test(n)) return { error: `non-identifier arg name "${n}"` };
        cleanNames.push(n);
        if (t.includes('=')) {                      // strip default value
            if (firstDefault === types.length) firstDefault = i;
            t = t.slice(0, t.indexOf('=')).trim();
        }
        const endsWithName = new RegExp(`(^|[^A-Za-z0-9_])${n}$`).test(t);
        decls.push(endsWithName ? t : `${t} ${n}`);
    }
    // Emit one variant per arity from firstDefault..all (so optional args become overloads).
    const variants = [];
    for (let k = firstDefault; k <= types.length; k++) {
        variants.push({ decl: decls.slice(0, k).join(', '), call: cleanNames.slice(0, k).join(', ') });
    }
    return { variants };
}

function emitFn(name, fn) {
    const ret = (fn.return && fn.return !== 'void') ? 'return ' : '';
    const lambdas = [];
    const seen = new Set();
    for (const sig of (fn.signatures || [])) {
        const r = buildLambdaVariants(sig.params || '', sig.params_simple || '');
        if (r.error) { warnings.push(`skip ${name}(${sig.params}): ${r.error}`); continue; }
        for (const v of r.variants) {
            // Unqualified call: resolves via `using namespace trussc; using namespace std;`
            // so both TrussC funcs (drawRect) and std math (sin) work. Genuine
            // trussc/std name clashes surface as compile ambiguities (witness catches).
            const lam = `[](${v.decl}){ ${ret}${name}(${v.call}); }`;
            if (seen.has(lam)) continue;            // dedup identical arities across sigs
            seen.add(lam);
            lambdas.push(lam);
        }
    }
    if (lambdas.length === 0) { warnings.push(`skip ${name}: no usable signatures`); return ''; }
    if (lambdas.length === 1) return `    lua->set_function("${name}", ${lambdas[0]});\n`;
    return `    lua->set_function("${name}", sol::overload(\n        ${lambdas.join(',\n        ')}\n    ));\n`;
}

// Collect free functions (no `self`), dedup by name.
const byName = new Map();
let typeMembers = 0;
for (const cat of (api.categories || [])) {
    for (const fn of (cat.functions || [])) {
        if (fn.self) { typeMembers++; continue; }   // -> Phase 2
        if (!fn.signatures) continue;
        if (DENYLIST.has(fn.name)) { warnings.push(`exclude ${fn.name}: denylist (non-bindable)`); continue; }
        if (fn.name.includes('::')) { warnings.push(`exclude ${fn.name}: namespaced (not a plain Lua global)`); continue; }
        if (!byName.has(fn.name)) byName.set(fn.name, fn);
        else warnings.push(`duplicate free-function name: ${fn.name} (keeping first)`);
    }
}

let body = '';
let emitted = 0;
for (const [name, fn] of byName) {
    const s = emitFn(name, fn);
    if (s) { body += s; emitted++; }
}

const out = `// AUTO-GENERATED from api-definition.yaml by apidef-luagen.js — DO NOT EDIT.
#include "tcxLua.h"
#include "TrussC.h"

using namespace trussc;
using namespace std;   // YAML param types use unqualified string/vector (canonical style)

#ifndef _MSC_VER
#pragma GCC diagnostic push
#pragma clang diagnostic push
#endif

void tcxLua::setTrussCGeneratedBindings(const std::shared_ptr<sol::state>& lua) {
${body}}

#ifndef _MSC_VER
#pragma GCC diagnostic pop
#pragma clang diagnostic pop
#endif
`;
process.stdout.write(out);

console.error(`[apidef-luagen] free-function names: ${byName.size}, emitted: ${emitted}, type-members skipped (Phase 2): ${typeMembers}, warnings: ${warnings.length}`);
for (const w of warnings) console.error('  WARN ' + w);
