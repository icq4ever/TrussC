#!/usr/bin/env node
// ============================================================================
// mine.js — one-time migration: mine PROSE out of the legacy api-definition.yaml
// and re-key it by canonical symbol-id into api-reference.toml.
//
// The structure (from structure.js) is the id ORACLE. Every legacy prose entry
// is mapped to a real symbol-id; entries that cannot be mapped are reported as
// ORPHANS (never silently dropped — that would lose the ja/ko asset).
//
//   node mine.js --structure <structure.json> [--out api-reference.toml] [--write]
// ============================================================================
const fs = require('fs');
const path = require('path');
const yaml = require(path.join(__dirname, '../node_modules/js-yaml'));

const argv = process.argv.slice(2);
const argVal = (f, d) => { const i = argv.indexOf(f); return i >= 0 ? argv[i + 1] : d; };
const YAML = path.join(__dirname, '../api-definition.yaml');
const STRUCT = argVal('--structure');
const OUT = argVal('--out', path.join(__dirname, 'api-reference.toml'));
if (!STRUCT) { console.error('need --structure <structure.json>'); process.exit(1); }

const api = yaml.load(fs.readFileSync(YAML, 'utf8'));
const structure = JSON.parse(fs.readFileSync(STRUCT, 'utf8'));
const ids = new Set(Object.keys(structure));

// reconcile a candidate id against the structure oracle.
// returns { id, how } or { orphan:true }
const bySuffix = new Map();   // "::name" / "name" -> [ids]
for (const id of ids) {
    const tail = id.includes('::') ? id.slice(id.lastIndexOf('::') + 2) : id;
    if (!bySuffix.has(tail)) bySuffix.set(tail, []);
    bySuffix.get(tail).push(id);
    const base = id.replace(/<.*>$/, '');           // template base
    if (base !== id) { if (!bySuffix.has(base)) bySuffix.set(base, []); bySuffix.get(base).push(id); }
}
function reconcile(cand, bareName) {
    if (ids.has(cand)) return { id: cand, how: 'exact' };
    // template: "Tween" -> "Tween<T>"
    const tmpl = [...ids].filter(i => i === cand + '<' || i.startsWith(cand + '<'));
    if (tmpl.length === 1) return { id: tmpl[0], how: 'template' };
    // bare name -> unique qualified id ending in ::bareName
    const cands = bySuffix.get(bareName) || [];
    if (cands.length === 1) return { id: cands[0], how: 'suffix' };
    return { orphan: true, ambiguous: cands.length > 1 ? cands : null };
}

// prose extraction from a legacy entry
function prose(e) {
    const p = {};
    if (e.keywords && e.keywords.length) p.keywords = e.keywords;
    const of = e.of_equivalent ? String(e.of_equivalent).split(/[\/,]/).map(s => s.trim()).filter(Boolean) : null;
    if (of && of.length) p.of = of;
    const d = {};
    if (e.description) d.en = e.description;
    if (e.description_ja) d.ja = e.description_ja;
    if (e.description_ko) d.ko = e.description_ko;
    if (Object.keys(d).length) p.description = d;
    if (e.details) p.details = e.details;
    if (e.__category) p.category = e.__category;    // legacy yaml category id (free fns)
    const n = {};                                   // oF migration notes (per oF mapping)
    if (e.of_notes) n.en = e.of_notes;
    if (e.of_notes_ja) n.ja = e.of_notes_ja;
    if (e.of_notes_ko) n.ko = e.of_notes_ko;
    if (Object.keys(n).length) p.of_notes = n;
    if (e.of_type_category) p.of_category = e.of_type_category;   // oF mapping type group
    return Object.keys(p).length ? p : null;
}

const out = new Map();          // id -> prose  (Map keeps first; dup id reported)
const orphans = [];
const reconciled = [];
const dupKeys = [];
function place(id, p, srcLabel) {
    if (!p) return;
    if (out.has(id)) {                              // dual representation (cat fn + types[] entry)
        const ex = out.get(id);                     // keep first on conflict, fill gaps from the other
        for (const k of Object.keys(p)) if (ex[k] === undefined) ex[k] = p[k];
        dupKeys.push(`${id}  (merged from ${srcLabel})`);
        return;
    }
    out.set(id, p);
}
function consider(cand, bareName, e, srcLabel) {
    const p = prose(e);
    if (!p) return;                                 // nothing to carry
    const r = reconcile(cand, bareName);
    if (r.orphan) { orphans.push({ cand, srcLabel, ambiguous: r.ambiguous, hasJa: !!(e.description_ja) }); return; }
    if (r.how !== 'exact') reconciled.push(`${cand} -> ${r.id} (${r.how})`);
    place(r.id, p, srcLabel);
}

// --- walk every prose-bearing legacy shape ---
for (const c of api.categories || []) for (const f of c.functions || []) {
    const real = f.cppName || f.name;
    const cand = f.self ? `${f.self}::${real}` : real;
    f.__category = c.id;                            // carry the category id into prose
    consider(cand, f.name.replace(/^.*::/, ''), f, `cat:${c.id}/${f.name}`);
}
for (const t of api.types || []) {
    consider(t.name, t.name, t, `type:${t.name}`);
    for (const m of t.methods || []) consider(`${t.name}::${m.cppName || m.name}`, m.name, m, `type:${t.name}.method`);
    for (const m of t.static_methods || []) consider(`${t.name}::${m.cppName || m.name}`, m.name, m, `type:${t.name}.static`);
    for (const m of t.properties || []) consider(`${t.name}::${m.cppName || m.name}`, m.name, m, `type:${t.name}.prop`);
}
for (const en of api.enums || []) { const real = en.cppName || en.name; consider(real, en.name, en, `enum:${en.name}`); }
for (const k of api.constants || []) { const real = k.cppName || k.name; consider(real, k.name, k, `const:${k.name}`); }

// --- emit TOML ---
function tstr(s) { s = String(s); return /[\n"]/.test(s) ? `'''\n${s}\n'''` : `"${s.replace(/\\/g, '\\\\').replace(/"/g, '\\"')}"`; }
function tarr(a) { return '[' + a.map(x => `"${String(x).replace(/"/g, '\\"')}"`).join(', ') + ']'; }
let toml = '# Generated by mine.js — prose mined from the legacy api-definition.yaml,\n# re-keyed by canonical symbol-id. Structure lives in the C++ AST, not here.\n';
for (const id of [...out.keys()].sort()) {
    const p = out.get(id);
    toml += `\n[${tstr(id)}]\n`;
    if (p.category) toml += `category = ${tstr(p.category)}\n`;
    if (p.keywords) toml += `keywords = ${tarr(p.keywords)}\n`;
    if (p.of) toml += `of = ${tarr(p.of)}\n`;
    if (p.description) for (const lang of ['en', 'ja', 'ko']) if (p.description[lang]) toml += `description.${lang} = ${tstr(p.description[lang])}\n`;
    if (p.details) toml += `details = ${tstr(p.details)}\n`;
    if (p.of_category) toml += `of_category = ${tstr(p.of_category)}\n`;
    if (p.of_notes) for (const lang of ['en', 'ja', 'ko']) if (p.of_notes[lang]) toml += `of_notes.${lang} = ${tstr(p.of_notes[lang])}\n`;
}

// --- verify it round-trips through a strict TOML parser (dup-key guard) ---
const TOML = require(path.join(__dirname, '../node_modules/smol-toml'));
let parsed; try { parsed = TOML.parse(toml); } catch (e) { console.error('EMITTED TOML FAILED TO PARSE:', e.message); process.exit(1); }

if (argv.includes('--write')) { fs.writeFileSync(OUT, toml); console.error(`wrote ${OUT}`); }

// --- report ---
// classify orphans: addon (own reference later) / mcp (hidden, drop) /
// std-math (curated list TODO, provider:std) / true (needs attention)
const classify = (o) => /cat:addon/.test(o.srcLabel) ? 'addon'
    : /^mcp::/.test(o.cand) ? 'mcp'
    : /cat:math_/.test(o.srcLabel) ? 'std-math'
    : 'true';
const bucket = { addon: 0, mcp: 0, 'std-math': 0, true: [] };
for (const o of orphans) { const c = classify(o); if (c === 'true') bucket.true.push(o); else bucket[c]++; }

const undoc = [...ids].filter(id => !out.has(id));
console.log('=== mine.js report ===');
console.log(`legacy prose entries placed : ${out.size}`);
console.log(`reconciled (non-exact id)   : ${reconciled.length}`);
console.log(`emitted TOML parses (smol-toml): OK (${Object.keys(parsed).length} tables)`);
console.log(`undocumented (symbol, no prose): ${undoc.length} / ${ids.size}`);
console.log(`orphans: addon=${bucket.addon} (own ref later)  mcp=${bucket.mcp} (drop)  std-math=${bucket['std-math']} (curated list TODO)  TRUE=${bucket.true.length}`);
if (bucket.true.length) {
    console.log('\n--- TRUE orphans (need attention) ---');
    for (const o of bucket.true) console.log(`  ${o.cand}${o.ambiguous ? '  [ambiguous: ' + o.ambiguous.slice(0, 4).join(', ') + ']' : ''}  <- ${o.srcLabel}`);
}
if (dupKeys.length) { console.log(`\n--- dup ids (${dupKeys.length}, first 15) ---`); dupKeys.slice(0, 15).forEach(d => console.log('  ' + d)); }
