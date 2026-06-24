#!/usr/bin/env node
// Coverage audit: which PUBLIC core symbols are missing from api-definition.yaml.
//
//   node audit-coverage.js            # human-readable grouped report
//   node audit-coverage.js --json     # machine-readable candidates (for LLM triage)
//
// Mechanical only: it decides "is this name in the yaml?" deterministically.
// It does NOT judge whether a symbol *should* be documented — that triage is left
// to a human / LLM pass (the script intentionally over-reports, marking likely
// noise so the reviewer can reject false positives quickly).

const fs = require('fs');
const path = require('path');
const yaml = require('js-yaml');

const ROOT = path.join(__dirname, '../../..');
const API_YAML = path.join(__dirname, '../api-definition.yaml');
const INCLUDE = path.join(ROOT, 'trussc/core/include');

// Directories / files that are vendored or not part of the documented surface.
const SKIP_DIR = /^(impl|stb|sokol|imgui|nlohmann|earcut|pugixml|miniaudio|dr_libs|shaders)$/i;
const SKIP_FILE = /^(miniaudio|httplib|json|stb_|lz4|tcReflect)/i;

// ---- 1. known names from the yaml (authoritative) -----------------------
function loadKnown() {
    const api = yaml.load(fs.readFileSync(API_YAML, 'utf8'));
    const known = new Set();
    const add = (n) => n && known.add(n);
    for (const c of api.categories || []) for (const f of c.functions || []) add(f.name);
    for (const t of api.types || []) {
        add(t.name);
        for (const m of (t.methods || [])) add(m.name);
        for (const m of (t.static_methods || [])) add(m.name);
        for (const p of (t.properties || [])) add(p.name);
    }
    for (const c of api.constants || []) add(c.name);
    for (const e of api.enums || []) { add(e.name); for (const v of (e.values || [])) add(v.name); }
    return known;
}

// ---- 2. extract public symbols from headers ----------------------------
// Returns [{ name, kind, header, line, snippet }]
function extractSymbols() {
    const out = [];
    const walk = (d) => {
        for (const n of fs.readdirSync(d)) {
            if (SKIP_DIR.test(n)) continue;
            const p = path.join(d, n);
            const st = fs.statSync(p);
            if (st.isDirectory()) walk(p);
            else if (/\.(h|hpp)$/.test(n) && !SKIP_FILE.test(n)) scan(p);
        }
    };
    const scan = (file) => {
        const header = file.replace(INCLUDE + '/', '');
        const isColor = /tcColor\.h$/.test(file);
        const lines = fs.readFileSync(file, 'utf8').split('\n');
        let internalDepth = -1; // brace depth tracking inside `namespace internal`
        for (let i = 0; i < lines.length; i++) {
            const line = lines[i];
            if (internalDepth >= 0) {
                internalDepth += (line.match(/{/g) || []).length - (line.match(/}/g) || []).length;
                if (internalDepth <= 0) internalDepth = -1;
                continue;
            }
            if (/\bnamespace\s+internal\b/.test(line)) { internalDepth = 1; continue; }

            // enum class
            let m = line.match(/\benum\s+class\s+([A-Z]\w*)/);
            if (m) { push(out, m[1], 'enum', header, i + 1, line); continue; }
            // class / struct definition (has a body, not a forward decl ending in ;)
            m = line.match(/^\s*(?:class|struct)\s+([A-Z]\w*)\b(?!\s*;)/);
            if (m && !/;\s*$/.test(line)) { push(out, m[1], 'class', header, i + 1, line); continue; }
            // public type alias: using Name = ...  (e.g. using Json = nlohmann::json)
            m = line.match(/^\s*using\s+([A-Z]\w*)\s*=/);
            if (m && !['Self', 'Super', 'Base', 'Ptr', 'Type'].includes(m[1])) {
                push(out, m[1], 'class', header, i + 1, line.trim()); continue;
            }
            // free function: `inline <type> name(`  OR a column-0 (un-indented,
            // = namespace scope, not a class method) `<type> name(` even without
            // inline (e.g. `int runApp(...)`). Methods inside classes are indented.
            m = line.match(/^inline\s+[A-Za-z_][\w:<>,&*\s]*?\b([a-z]\w*)\s*\(/)
             || line.match(/^[A-Za-z_][\w:<>,&*]*[\s&*]+([a-z]\w*)\s*\([^;{]*[;{]/);
            if (m) {
                const nm = m[1];
                if (nm.endsWith('_')) continue;          // internal convention
                // In tcColor.h skip only the no-arg color CONSTANTS (aliceBlue(), ...),
                // but keep conversion/factory functions with parameters (colorFromHSB(...)).
                if (isColor && /\(\s*\)/.test(line)) continue;
                if (['operator', 'if', 'for', 'while', 'return', 'switch', 'else', 'static_cast', 'reinterpret_cast'].includes(nm)) continue;
                push(out, nm, 'func', header, i + 1, line.trim());
            }
        }
    };
    walk(INCLUDE);
    return out;
}

const seenKey = new Set();
function push(out, name, kind, header, line, snippet) {
    if (name.endsWith('_')) return;   // internal naming convention (any kind)
    const k = kind + ':' + name;
    if (seenKey.has(k)) return;
    seenKey.add(k);
    out.push({ name, kind, header, line, snippet: (snippet || '').trim().slice(0, 120) });
}

// yaml function + type names only (the kinds the header scan targets) — used to
// self-test the extractor: any of these the scan can't find is a blind spot.
function loadYamlTargets() {
    const api = yaml.load(fs.readFileSync(API_YAML, 'utf8'));
    const funcs = new Set(), types = new Set();
    for (const c of api.categories || []) for (const f of c.functions || []) funcs.add(f.name);
    for (const t of api.types || []) types.add(t.name);
    for (const e of api.enums || []) types.add(e.name);
    return { funcs, types };
}

// ---- 3. diff -----------------------------------------------------------
const known = loadKnown();
const symbols = extractSymbols();
const missing = symbols.filter(s => !known.has(s.name));

// ---- reverse (extractor self-test) -------------------------------------
if (process.argv.includes('--reverse')) {
    const foundNames = new Set(symbols.map(s => s.name));
    const { funcs, types } = loadYamlTargets();
    const unfoundFuncs = [...funcs].filter(n => !foundNames.has(n)).sort();
    const unfoundTypes = [...types].filter(n => !foundNames.has(n)).sort();
    console.log(`\n=== extractor self-test: yaml symbols the header scan did NOT find ===`);
    console.log(`\n--- functions in yaml but unfound (${unfoundFuncs.length}) ---`);
    console.log('  ' + unfoundFuncs.join(', '));
    console.log(`\n--- types in yaml but unfound (${unfoundTypes.length}) ---`);
    console.log('  ' + unfoundTypes.join(', '));
    console.log('\n(each = an extractor blind spot OR a phantom/sketch-only yaml entry)');
    process.exit(0);
}

if (process.argv.includes('--json')) {
    console.log(JSON.stringify(missing, null, 2));
} else {
    const byKind = { enum: [], class: [], func: [] };
    for (const s of missing) (byKind[s.kind] || (byKind[s.kind] = [])).push(s);
    console.log(`\n=== coverage audit: ${symbols.length} public symbols scanned, ${missing.length} NOT in yaml ===\n`);
    for (const kind of ['enum', 'class', 'func']) {
        const list = byKind[kind] || [];
        console.log(`--- ${kind} (${list.length}) ---`);
        const byHeader = {};
        for (const s of list) (byHeader[s.header] || (byHeader[s.header] = [])).push(s.name);
        for (const [h, ns] of Object.entries(byHeader).sort((a, b) => b[1].length - a[1].length)) {
            console.log(`  ${h}:  ${ns.join(', ')}`);
        }
        console.log('');
    }
    // known-gap sanity check
    const KNOWN_GAPS = ['logWarning', 'logError', 'fft', 'getBlendModeName', 'setBeepVolume', 'Xml', 'Json', 'StrokeMesh', 'BlendMode', 'enable3D', 'getRootNode'];
    const names = new Set(missing.map(s => s.name));
    console.log('known-gap check: ' + KNOWN_GAPS.map(g => `${g}=${names.has(g) ? '✓' : '✗'}`).join('  '));
}
