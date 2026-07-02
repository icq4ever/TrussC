#!/usr/bin/env node
// ============================================================================
// emit-dash.js — emit a Dash/Zeal offline docset from the canonical
// reference-data.json (a downstream consumer; see README.md).
//
// Produces a self-contained `.docset` bundle:
//   <out>/Contents/Info.plist
//   <out>/Contents/Resources/docSet.dsidx           (SQLite search index)
//   <out>/Contents/Resources/Documents/             (static HTML + style.css)
//
// The HTML is fully static (no JavaScript needed) so Zeal renders it offline.
// Search + per-page table-of-contents come from the SQLite index and the
// `dashAnchor` markers embedded in each page (DashDocSetFamily = dashtoc).
//
//   node emit-dash.js [--data <reference-data.json>] [--out <TrussC.docset>]
//                     [--docs <docs dir with the *.md guides>]
//                     [--lang en|ko|ja|all|en,ko] [--dry] [--strict]
//
// --lang: which description language(s) to render. Default 'en' (Dash-style,
//         one language, no per-language chips). 'all' = en,ja,ko; or a comma
//         list like 'en,ko'. reference-data.json always carries all three.
// --strict: treat schema-drift warnings as errors (exit 1, write nothing).
//           Use in CI so a renamed/restructured reference-data.json field is
//           caught loudly instead of silently producing an empty docset.
//
// Prereq: reference-data.json must exist — generate it first with
//   node --max-old-space-size=8192 generate.js      (needs clang; see README)
//
// Deps (docs/package.json): sql.js (WASM SQLite, no native build), marked.
// ============================================================================
const fs = require('fs');
const path = require('path');

const argv = process.argv.slice(2);
const argVal = (f, d) => { const i = argv.indexOf(f); return i >= 0 ? argv[i + 1] : d; };
const DRY = argv.includes('--dry');
const STRICT = argv.includes('--strict');
// description language(s) to render; default English-only (Dash convention)
const LANG_LABELS = { en: 'EN', ja: '日本語', ko: '한국어' };
const _langArg = String(argVal('--lang', 'en')).toLowerCase();
const LANGS = (_langArg === 'all' ? ['en', 'ja', 'ko'] : _langArg.split(',').map(s => s.trim()))
    .filter(k => LANG_LABELS[k]);
if (!LANGS.length) LANGS.push('en');

const REF_DIR = __dirname;
const DATA = argVal('--data', path.join(REF_DIR, 'reference-data.json'));
const OUT = argVal('--out', path.join(REF_DIR, 'build', 'TrussC.docset'));
const DOCS_DIR = argVal('--docs', path.join(REF_DIR, '..'));
const NODE_MODULES = path.join(REF_DIR, '..', 'node_modules');

// --- guides to include (whitelist; order = display order) -------------------
const GUIDES = [
    ['GET_STARTED.md', 'Get Started'],
    ['GET_STARTED_CONSOLE_MODE.md', 'Get Started — Console Mode'],
    ['ARCHITECTURE.md', 'Architecture'],
    ['BUILD_SYSTEM.md', 'Build System'],
    ['ADDONS.md', 'Addons'],
    ['AI_AUTOMATION.md', 'AI Automation'],
    ['TrussC_vs_openFrameworks.md', 'TrussC vs openFrameworks'],
    ['ROADMAP.md', 'Roadmap'],
    ['SECURITY.md', 'Security'],
];

// --- load canonical + sidecars ----------------------------------------------
if (!fs.existsSync(DATA)) {
    console.error(`emit-dash: ${DATA} not found.\n` +
        `Generate it first:  node --max-old-space-size=8192 generate.js  (needs clang)\n` +
        `or point at a prebuilt copy:  node emit-dash.js --data <path>`);
    process.exit(1);
}
const REF = JSON.parse(fs.readFileSync(DATA, 'utf8'));
const readJson = (p, fallback) => { try { return JSON.parse(fs.readFileSync(p, 'utf8')); } catch { return fallback; } };
const CATS = readJson(path.join(REF_DIR, 'categories.json'), []);
const EXTRAS = readJson(path.join(REF_DIR, 'extras.json'), { macros: [], keywords: [], constants: {} });
const COLORS = readJson(path.join(REF_DIR, 'colors.json'), []);
const CAT_BY_ID = new Map(CATS.map(c => [c.id, c]));

// --- helpers ----------------------------------------------------------------
const esc = (s) => String(s == null ? '' : s)
    .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
const _t = (s) => String(s == null ? '' : s).trim();
// DISPLAY-only: drop a single trailing "_" from param names (member-shadow
// convention, e.g. Vec2(float x_, float y_)). reference-data keeps the real name.
const stripU = (s) => String(s || '').replace(/_(?=\s*(?:,|=|$))/g, '');
// filesystem-safe page name for a type (Tween<T> -> Tween_T)
const safe = (s) => String(s).replace(/[^A-Za-z0-9_]+/g, '_').replace(/^_+|_+$/g, '') || 'x';
const DASH_LANG = 'cpp';
const anchor = (type, name) => `//apple_ref/${DASH_LANG}/${type}/${encodeURIComponent(name)}`;
// relative path from a page at <depth> dirs deep back to Documents/ root
const up = (depth) => '../'.repeat(depth);

// details may be {en,ja,ko} or a bare en-only string
function detailsTrio(det) {
    if (!det) return {};
    if (typeof det === 'object') return { en: _t(det.en), ja: _t(det.ja), ko: _t(det.ko) };
    return { en: _t(det) };
}

// description + details block, in the selected LANGS. Single language → plain
// (no per-language chip); multiple → one labelled block per language.
function descBlock(e) {
    const d = e.description || {};
    const det = detailsTrio(e.details);
    const single = LANGS.length === 1;
    let out = '';
    let any = false;
    for (const k of LANGS) {
        const desc = _t(d[k]);
        const detail = _t(det[k]);
        if (!desc && !detail) continue;
        any = true;
        if (!single) out += `<div class="lang"><span class="lang-tag">${LANG_LABELS[k]}</span>`;
        if (desc) out += `<p class="desc">${esc(desc)}</p>`;
        if (detail) out += `<p class="detail">${esc(detail)}</p>`;
        if (!single) out += `</div>`;
    }
    // fallback: any available language (prefer en), else undocumented
    if (!any) {
        const fb = _t(d.en) || _t(d[LANGS[0]]) || _t(d.ja) || _t(d.ko);
        out = fb ? `<p class="desc">${esc(fb)}</p>` : `<p class="desc undocumented">No description yet.</p>`;
    }
    return out;
}
// first available description among the selected LANGS (for compact tables)
const pickDesc = (e) => {
    const d = e.description || {};
    for (const k of LANGS) if (_t(d[k])) return _t(d[k]);
    return _t(d.en) || '';
};

function sigLines(e) {
    const sigs = e.signatures || [];
    if (!sigs.length) return '';
    const stat = e.static ? 'static ' : '';
    return sigs.map(s => {
        const ret = s.ret ? esc(s.ret) + ' ' : '';
        const owner = e.owner ? esc(e.owner) + '::' : '';
        const params = esc(stripU(s.params || ''));
        const cst = s.const ? ' const' : '';
        return `<code class="sig">${stat}${ret}${owner}${esc(e.name)}(${params})${cst}</code>`;
    }).join('');
}

function depBadge(e) {
    if (!e.deprecated) return '';
    const r = e.deprecated.reason ? ` — ${esc(e.deprecated.reason)}` : '';
    return `<span class="deprecated">deprecated${r}</span>`;
}

// a single documented symbol block, with its dash TOC anchor
function symbolBlock(e, dashType) {
    const a = anchor(dashType, e.name);
    let h = `<a name="${a}" class="dashAnchor"></a>\n`;
    h += `<div class="sym" id="${safe(e.id)}">`;
    h += `<h3 class="sym-name">${esc(e.name)} ${depBadge(e)}</h3>`;
    const sig = sigLines(e);
    if (sig) h += `<div class="sigs">${sig}</div>`;
    h += descBlock(e);
    if (e.of && e.of.length) h += `<p class="of">openFrameworks: <code>${e.of.map(esc).join('</code>, <code>')}</code></p>`;
    if (_t(e.snippet)) h += `<pre class="snippet"><code>${esc(_t(e.snippet))}</code></pre>`;
    h += `</div>`;
    return h;
}

// --- page shell -------------------------------------------------------------
function page(depth, title, bodyHtml) {
    const css = up(depth) + 'style.css';
    const home = up(depth) + 'index.html';
    return `<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>${esc(title)} — TrussC</title>
<link rel="stylesheet" href="${css}">
</head><body>
<nav class="top"><a href="${home}">TrussC Reference</a> &rsaquo; <span>${esc(title)}</span></nav>
<main>
${bodyHtml}
</main>
<footer>TrussC offline docset — generated by emit-dash.js</footer>
</body></html>`;
}

// ============================================================================
// build the model from reference-data.json
// ============================================================================
const all = Object.values(REF).filter(e => !e.hidden);
const enumNames = new Set(all.filter(e => e.kind === 'enum').map(e => e.name));
const isEnumMember = (e) => e.kind === 'var' && e.ns && enumNames.has(e.ns);
const isOperator = (e) => /^operator/.test(e.name || '') && !/^operator[A-Za-z\s(]/.test(e.name || '');

// owners -> members
const types = new Map();        // typeName -> type entry (or synthesized)
for (const e of all) if (e.kind === 'type') types.set(e.name, e);
const ownerMembers = new Map(); // ownerName -> { methods, statics, fields, operators }
function bucket(owner) {
    if (!ownerMembers.has(owner)) ownerMembers.set(owner, { methods: [], statics: [], fields: [], operators: [] });
    return ownerMembers.get(owner);
}
for (const e of all) {
    if (!e.owner) continue;
    if (e.kind === 'method') {
        if (isOperator(e)) bucket(e.owner).operators.push(e);
        else (e.static ? bucket(e.owner).statics : bucket(e.owner).methods).push(e);
    } else if (e.kind === 'field') {
        bucket(e.owner).fields.push(e);
    }
}
// synthesize a type entry for any owner that has members but no type entry
for (const owner of ownerMembers.keys())
    if (!types.has(owner)) types.set(owner, { id: owner, kind: 'type', name: owner, signatures: [] });

// free functions grouped by category
const freeFns = all.filter(e => e.kind === 'func');
const enums = all.filter(e => e.kind === 'enum');
const enumMembers = new Map(); // enumName -> [members]
for (const e of all) if (isEnumMember(e)) {
    if (!enumMembers.has(e.ns)) enumMembers.set(e.ns, []);
    enumMembers.get(e.ns).push(e);
}
const typedefs = all.filter(e => e.kind === 'typedef');
// namespaced constants: kind var/field, not an enum member, not owned by a type
const constants = all.filter(e => (e.kind === 'var' || e.kind === 'field') && !isEnumMember(e) && !e.owner);
const macros = EXTRAS.macros || [];
const keywords = EXTRAS.keywords || [];

// --- schema sanity checks — catch SILENT reference-data.json drift ----------
// A renamed/restructured field usually doesn't crash: JS just reads `undefined`
// and renders empty sections, so an incomplete docset looks "successful". These
// checks turn that into a visible warning (or, with --strict, a hard failure).
// If one fires, reconcile emit-dash.js with the schema in README.md §"the
// canonical format" after a `git merge main`.
const warnings = [];
const warn = (m) => warnings.push(m);
{
    const KNOWN_KINDS = new Set(['type', 'enum', 'typedef', 'method', 'func', 'field', 'var']);
    const kindCount = {};
    let noName = 0, strDesc = 0, sigNoRet = 0, callable = 0, hasDesc = 0;
    for (const e of all) {
        kindCount[e.kind] = (kindCount[e.kind] || 0) + 1;
        if (!e.name) noName++;
        if (typeof e.description === 'string') strDesc++;
        if (e.description && typeof e.description === 'object' && (e.description.en || e.description.ja || e.description.ko)) hasDesc++;
        if (e.kind === 'method' || e.kind === 'func') {
            const s = (e.signatures || [])[0];
            if (s) { callable++; if (!('ret' in s)) sigNoRet++; }
        }
    }
    const unknown = Object.keys(kindCount).filter(k => !KNOWN_KINDS.has(k));
    const enumMemberCount = [...enumMembers.values()].reduce((n, a) => n + a.length, 0);
    if (unknown.length) warn(`unknown kind value(s): ${unknown.join(', ')} — these are not rendered; the 'kind' vocabulary may have changed.`);
    if (types.size === 0) warn(`0 types found — expected classes/structs. 'kind'/'owner' field renamed?`);
    if (freeFns.length === 0) warn(`0 free functions found (kind:'func').`);
    if (enums.length > 0 && enumMemberCount === 0) warn(`${enums.length} enum(s) but 0 members grouped — enum-member layout (kind:'var' + ns=enum) may have changed.`);
    if (noName) warn(`${noName} entr(y/ies) with no 'name' field.`);
    if (strDesc) warn(`${strDesc} entr(y/ies) have a STRING 'description' (expected {en,ja,ko}) — they will render as undocumented.`);
    if (all.length > 20 && hasDesc === 0) warn(`no entry has a {en,ja,ko} 'description' — the 'description' field may be renamed; everything will render as undocumented.`);
    if (callable && sigNoRet === callable) warn(`no signature carries a 'ret' field — signature shape may have changed.`);
    if (!CATS.length) warn(`categories.json empty/unreadable — free functions won't be grouped by category.`);
}

const catName = (id, lang) => {
    const c = CAT_BY_ID.get(id);
    if (!c) return id || 'Other';
    if (lang === 'ko') return c.name_ko || c.name;
    return c.name;
};
const byName = (a, b) => (a.name || '').localeCompare(b.name || '');
function groupByCategory(list) {
    const g = new Map();
    for (const e of list) {
        const cat = e.category || e.ns || e.owner || 'other';
        if (!g.has(cat)) g.set(cat, []);
        g.get(cat).push(e);
    }
    // order: categories.json order first, then unknowns alphabetically
    const order = (id) => (CAT_BY_ID.get(id) ? CAT_BY_ID.get(id).order : 1e6);
    return [...g.entries()].sort((a, b) => order(a[0]) - order(b[0]) || a[0].localeCompare(b[0]));
}

// ============================================================================
// render pages + collect index rows  { name, type, path }
// ============================================================================
const files = new Map();  // relative path under Documents/ -> html string
const index = [];         // dash search rows
const addIndex = (name, type, p) => index.push({ name, type, path: p });

// ---- type pages ----
const typeNames = [...types.keys()].sort((a, b) => a.localeCompare(b));
for (const name of typeNames) {
    const t = types.get(name);
    const isEnumType = t.kind === 'enum';
    const dashType = isEnumType ? 'Enum' : 'Class';
    const rel = `types/${safe(name)}.html`;
    const mem = ownerMembers.get(name) || { methods: [], statics: [], fields: [], operators: [] };

    let body = `<h1>${esc(name)}</h1>`;
    body += `<a name="${anchor(dashType, name)}" class="dashAnchor"></a>`;
    body += `<div class="kindtag">${isEnumType ? 'enum' : 'type'}</div>`;
    body += descBlock(t);

    // constructors
    if (t.constructors && t.constructors.length) {
        body += `<section><h2>Constructors</h2>`;
        for (const c of t.constructors) {
            body += `<a name="${anchor('Constructor', name)}" class="dashAnchor"></a>`;
            body += `<div class="sym"><code class="sig">${esc(name)}(${esc(stripU(c.params || ''))})</code></div>`;
        }
        body += `</section>`;
        addIndex(name, 'Constructor', rel + '#' + anchor('Constructor', name));
    }
    const section = (title, items, dtype) => {
        if (!items.length) return;
        body += `<section><h2>${title}</h2>`;
        for (const e of items.sort(byName)) {
            body += symbolBlock(e, dtype);
            addIndex(`${name}::${e.name}`, dtype, rel + '#' + anchor(dtype, e.name));
        }
        body += `</section>`;
    };
    section('Static methods', mem.statics, 'Method');
    section('Methods', mem.methods, 'Method');
    section('Fields', mem.fields, 'Field');
    section('Operators', mem.operators, 'Operator');

    files.set(rel, page(1, name, body));
    addIndex(name, dashType, rel + '#' + anchor(dashType, name));
}

// ---- free functions (functions.html, grouped by category) ----
{
    let body = `<h1>Free Functions</h1>`;
    for (const [cat, list] of groupByCategory(freeFns)) {
        const cn = catName(cat, 'en');
        body += `<a name="${anchor('Section', cn)}" class="dashAnchor"></a>`;
        body += `<section><h2>${esc(cn)}</h2>`;
        for (const e of list.sort(byName)) {
            body += symbolBlock(e, 'Function');
            addIndex(e.name, 'Function', 'functions.html#' + anchor('Function', e.name));
        }
        body += `</section>`;
    }
    files.set('functions.html', page(0, 'Free Functions', body));
}

// ---- enums (enums.html) ----
{
    let body = `<h1>Enums</h1>`;
    for (const en of enums.sort(byName)) {
        body += `<a name="${anchor('Enum', en.name)}" class="dashAnchor"></a>`;
        body += `<section><h2>${esc(en.name)}</h2>`;
        body += descBlock(en);
        const members = (enumMembers.get(en.name) || []).sort(byName);
        if (members.length) {
            body += `<table class="members"><thead><tr><th>Value</th><th>Description</th></tr></thead><tbody>`;
            for (const m of members) {
                body += `<a name="${anchor('Value', en.name + '.' + m.name)}" class="dashAnchor"></a>`;
                body += `<tr><td><code>${esc(m.name)}</code></td><td>${esc(pickDesc(m))}</td></tr>`;
                addIndex(`${en.name}::${m.name}`, 'Value', 'enums.html#' + anchor('Value', en.name + '.' + m.name));
            }
            body += `</tbody></table>`;
        }
        body += `</section>`;
        addIndex(en.name, 'Enum', 'enums.html#' + anchor('Enum', en.name));
    }
    files.set('enums.html', page(0, 'Enums', body));
}

// ---- constants / macros / keywords / colors / type aliases (constants.html) ----
{
    const constVal = (name) => (EXTRAS.constants && EXTRAS.constants[name]) || '';
    const colorHex = new Map();
    for (const grp of COLORS) for (const it of (grp.items || [])) colorHex.set(it.name, it.hex);

    let body = `<h1>Constants &amp; Macros</h1>`;

    if (constants.length) {
        body += `<section><h2>Constants</h2>`;
        for (const e of constants.sort(byName)) {
            body += `<a name="${anchor('Constant', e.name)}" class="dashAnchor"></a>`;
            body += `<div class="sym"><h3 class="sym-name">${esc(e.ns ? e.ns + '::' + e.name : e.name)}`;
            const hex = colorHex.get(e.name);
            if (hex) body += ` <span class="swatch" style="background:${esc(hex)}"></span>`;
            const v = constVal(e.name);
            if (v) body += ` <code class="val">= ${esc(v)}</code>`;
            body += `</h3>`;
            body += descBlock(e);
            body += `</div>`;
            addIndex(e.ns ? `${e.ns}::${e.name}` : e.name, 'Constant', 'constants.html#' + anchor('Constant', e.name));
        }
        body += `</section>`;
    }

    if (macros.length) {
        body += `<section><h2>Macros</h2>`;
        for (const m of macros) {
            body += `<a name="${anchor('Macro', m.name)}" class="dashAnchor"></a>`;
            body += `<div class="sym"><h3 class="sym-name">${esc(m.name)}</h3>`;
            if (m.signature) body += `<div class="sigs"><code class="sig">${esc(m.signature)}</code></div>`;
            body += descBlock({ description: { en: m.description, ja: m.description_ja, ko: m.description_ko },
                                details: { en: m.details, ja: m.details_ja, ko: m.details_ko } });
            body += `</div>`;
            addIndex(m.name, 'Macro', 'constants.html#' + anchor('Macro', m.name));
        }
        body += `</section>`;
    }

    if (typedefs.length) {
        body += `<section><h2>Type aliases</h2>`;
        for (const e of typedefs.sort(byName)) {
            body += `<a name="${anchor('Type', e.name)}" class="dashAnchor"></a>`;
            body += `<div class="sym"><h3 class="sym-name">${esc(e.name)}`;
            if (e.type) body += ` <code class="val">= ${esc(e.type)}</code>`;
            body += `</h3>`;
            body += descBlock(e);
            body += `</div>`;
            addIndex(e.name, 'Type', 'constants.html#' + anchor('Type', e.name));
        }
        body += `</section>`;
    }

    if (keywords.length) {
        body += `<section><h2>Language keywords</h2><p class="kwlist">`;
        for (const k of keywords) {
            body += `<a name="${anchor('Keyword', k)}" class="dashAnchor"></a><code>${esc(k)}</code> `;
            addIndex(k, 'Keyword', 'constants.html#' + anchor('Keyword', k));
        }
        body += `</p></section>`;
    }

    files.set('constants.html', page(0, 'Constants & Macros', body));
}

// ---- guides (markdown -> html) ----
const marked = require('marked');
const guidePages = [];
for (const [file, title] of GUIDES) {
    const src = path.join(DOCS_DIR, file);
    if (!fs.existsSync(src)) continue;
    const md = fs.readFileSync(src, 'utf8');
    let html = marked.parse(md);
    // rewrite intra-doc .md links to the generated .html (basename match)
    html = html.replace(/href="([^"]+?)\.md(#[^"]*)?"/g, (m, base, frag) => {
        const b = base.split('/').pop();
        return `href="${b}.html${frag || ''}"`;
    });
    const rel = `guides/${file.replace(/\.md$/, '')}.html`;  // keep source basename so intra-doc .md links resolve
    let body = `<a name="${anchor('Guide', title)}" class="dashAnchor"></a>` + html;
    files.set(rel, page(1, title, body));
    addIndex(title, 'Guide', rel + '#' + anchor('Guide', title));
    guidePages.push([title, rel]);
}

// ---- index.html (landing) ----
{
    const li = (href, label, extra = '') => `<li><a href="${href}">${esc(label)}</a>${extra}</li>`;
    let body = `<h1>TrussC Reference</h1><p class="lede">Offline API reference &amp; guides. Use the search field, or browse below.</p>`;

    if (guidePages.length) {
        body += `<section><h2>Guides</h2><ul class="cols">`;
        for (const [title, rel] of guidePages) body += li(rel, title);
        body += `</ul></section>`;
    }

    body += `<section><h2>Types</h2><ul class="cols">`;
    for (const name of typeNames) body += li(`types/${safe(name)}.html`, name);
    body += `</ul></section>`;

    body += `<section><h2>Free Functions</h2>`;
    for (const [cat, list] of groupByCategory(freeFns)) {
        body += `<h3>${esc(catName(cat, 'en'))}</h3><ul class="cols">`;
        for (const e of list.sort(byName)) body += li(`functions.html#${anchor('Function', e.name)}`, e.name);
        body += `</ul>`;
    }
    body += `</section>`;

    body += `<section><h2>More</h2><ul>`;
    body += li('enums.html', `Enums (${enums.length})`);
    body += li('constants.html', `Constants, Macros & Type aliases`);
    body += `</ul></section>`;

    files.set('index.html', page(0, 'Home', body));
}

// ---- style.css ----
const CSS = `:root{--fg:#1c1c1e;--muted:#6b6b70;--bg:#fff;--card:#f6f6f7;--accent:#0b6cff;--code:#e8eef7;--border:#e2e2e6}
@media(prefers-color-scheme:dark){:root{--fg:#e6e6ea;--muted:#9a9aa2;--bg:#1a1a1c;--card:#242427;--accent:#5aa0ff;--code:#20293a;--border:#33333a}}
*{box-sizing:border-box}
body{margin:0;font:15px/1.6 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,"Noto Sans CJK KR",sans-serif;color:var(--fg);background:var(--bg)}
main{max-width:900px;margin:0 auto;padding:0 24px 64px}
nav.top{padding:12px 24px;border-bottom:1px solid var(--border);color:var(--muted);font-size:13px}
nav.top a{color:var(--accent);text-decoration:none}
footer{max-width:900px;margin:0 auto;padding:24px;color:var(--muted);font-size:12px;border-top:1px solid var(--border)}
h1{font-size:28px;margin:24px 0 8px}
h2{font-size:20px;margin:32px 0 8px;padding-bottom:4px;border-bottom:1px solid var(--border)}
h3.sym-name{font-size:16px;margin:20px 0 6px;font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace}
.kindtag{display:inline-block;font-size:11px;text-transform:uppercase;letter-spacing:.05em;color:var(--muted);border:1px solid var(--border);border-radius:4px;padding:1px 6px;margin-bottom:8px}
code{font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;background:var(--code);padding:1px 5px;border-radius:4px;font-size:.9em}
pre{background:var(--card);padding:12px 14px;border-radius:8px;overflow:auto;border:1px solid var(--border)}
pre code{background:none;padding:0}
code.sig{display:block;background:var(--card);padding:8px 12px;border-radius:6px;margin:3px 0;border:1px solid var(--border);white-space:pre-wrap}
.sym{padding:4px 0 8px}
.lang{margin:6px 0}
.lang-tag{display:inline-block;min-width:44px;font-size:11px;color:var(--muted);border:1px solid var(--border);border-radius:4px;padding:0 5px;margin-right:8px;vertical-align:top}
p.desc{margin:2px 0}
p.detail{margin:4px 0;color:var(--muted)}
p.desc.undocumented{color:var(--muted);font-style:italic}
p.of{font-size:13px;color:var(--muted);margin:4px 0}
.deprecated{color:#c1121f;font-size:12px;font-weight:600;border:1px solid #c1121f;border-radius:4px;padding:0 5px}
.swatch{display:inline-block;width:14px;height:14px;border-radius:3px;border:1px solid var(--border);vertical-align:middle}
table.members{border-collapse:collapse;width:100%;margin:8px 0}
table.members th,table.members td{text-align:left;padding:6px 10px;border-bottom:1px solid var(--border);vertical-align:top}
ul.cols{columns:3;-webkit-columns:3;list-style:none;padding:0}
ul.cols li{break-inside:avoid}
ul.cols a{color:var(--accent);text-decoration:none}
.lede{color:var(--muted)}
.kwlist code{margin:2px}
`;
files.set('style.css', CSS);

// ============================================================================
// write everything
// ============================================================================
async function main() {
    console.log(`emit-dash: ${all.length} public symbols → ${index.length} index entries, ${files.size} files (lang: ${LANGS.join(',')})`);
    const byType = {};
    for (const r of index) byType[r.type] = (byType[r.type] || 0) + 1;
    console.log('  by type:', Object.entries(byType).map(([k, v]) => `${k}:${v}`).join(' '));

    if (index.length === 0) warn('0 index entries — nothing to search; likely a schema mismatch.');
    if (warnings.length) {
        console.warn(`emit-dash: ${warnings.length} schema warning(s) — reference-data.json may have drifted:`);
        for (const w of warnings) console.warn('  ! ' + w);
        if (STRICT) { console.error('emit-dash: --strict set → aborting, wrote nothing.'); process.exit(1); }
    }

    if (DRY) { console.log('  --dry: not writing'); return; }

    const DOCS = path.join(OUT, 'Contents', 'Resources', 'Documents');
    fs.rmSync(OUT, { recursive: true, force: true });
    fs.mkdirSync(DOCS, { recursive: true });

    for (const [rel, html] of files) {
        const p = path.join(DOCS, rel);
        fs.mkdirSync(path.dirname(p), { recursive: true });
        fs.writeFileSync(p, html);
    }

    // copy guide images (docs/images -> Documents/guides/images) if any
    const imgSrc = path.join(DOCS_DIR, 'images');
    if (fs.existsSync(imgSrc)) fs.cpSync(imgSrc, path.join(DOCS, 'guides', 'images'), { recursive: true });

    // Info.plist
    const plist = `<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
    <key>CFBundleIdentifier</key><string>trussc</string>
    <key>CFBundleName</key><string>TrussC</string>
    <key>DocSetPlatformFamily</key><string>trussc</string>
    <key>isDashDocset</key><true/>
    <key>dashIndexFilePath</key><string>index.html</string>
    <key>DashDocSetFamily</key><string>dashtoc</string>
    <key>isJavaScriptEnabled</key><false/>
</dict></plist>
`;
    fs.writeFileSync(path.join(OUT, 'Contents', 'Info.plist'), plist);

    // SQLite index via sql.js (WASM — no native build)
    const initSqlJs = require('sql.js');
    const SQL = await initSqlJs({ locateFile: (f) => path.join(NODE_MODULES, 'sql.js', 'dist', f) });
    const db = new SQL.Database();
    db.run('CREATE TABLE searchIndex(id INTEGER PRIMARY KEY, name TEXT, type TEXT, path TEXT);' +
        'CREATE UNIQUE INDEX anchor ON searchIndex (name, type, path);');
    const stmt = db.prepare('INSERT OR IGNORE INTO searchIndex(name, type, path) VALUES (?, ?, ?)');
    for (const r of index) stmt.run([r.name, r.type, r.path]);
    stmt.free();
    const bytes = Buffer.from(db.export());
    db.close();
    fs.writeFileSync(path.join(OUT, 'Contents', 'Resources', 'docSet.dsidx'), bytes);

    console.log(`emit-dash: wrote ${OUT}`);
    console.log(`  docSet.dsidx: ${bytes.length} bytes`);
    console.log(`Load in Zeal: copy the .docset into Zeal's docsets dir and restart Zeal.`);
}
main().catch(e => { console.error(e); process.exit(1); });
