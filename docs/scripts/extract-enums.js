#!/usr/bin/env node
// Extract `enum class` definitions from core headers into api-definition.yaml
// `enums:` entries (name, values with value + English description from // comments).
// One-off-ish migration helper; review output before pasting.  Run:
//   node extract-enums.js            # yaml for all public enums
//   node extract-enums.js --names    # just "Name (header)" list, for triage

const fs = require('fs');
const path = require('path');
const INCLUDE = path.join(__dirname, '../../../trussc/core/include');
const SKIP_DIR = /^(impl|stb|sokol|imgui|nlohmann|earcut|pugixml|miniaudio|dr_libs|shaders)$/i;
const SKIP_FILE = /^(miniaudio|httplib|json|stb_|lz4)/i;
// clearly-internal enum types to omit (real code, but implementation-only)
const DENY = new Set(['TK', 'VTokKind', 'VertOrient', 'Width', 'Mode']);

// Drop block comments and pure-comment lines (so doc examples like
// `//   enum class WeaponType { ... }` are not mistaken for real enums).
// Inline value descriptions (`Round, // desc`) survive — those lines have code.
function stripComments(text) {
    return text.replace(/\/\*[\s\S]*?\*\//g, '').split('\n').filter(l => !/^\s*\/\//.test(l)).join('\n');
}

function* headers(d) {
    for (const n of fs.readdirSync(d)) {
        if (SKIP_DIR.test(n)) continue;
        const p = path.join(d, n);
        if (fs.statSync(p).isDirectory()) yield* headers(p);
        else if (/\.(h|hpp)$/.test(n) && !SKIP_FILE.test(n)) yield p;
    }
}

const enums = [];
for (const file of headers(INCLUDE)) {
    const header = file.replace(INCLUDE + '/', '');
    const text = stripComments(fs.readFileSync(file, 'utf8'));
    const re = /enum\s+class\s+([A-Z]\w*)[^\{]*\{([\s\S]*?)\}/g;
    let m;
    while ((m = re.exec(text))) {
        const name = m[1];
        if (DENY.has(name) || name.endsWith('_')) continue;
        const body = m[2];
        const values = [];
        let next = 0;
        for (let raw of body.split('\n')) {
            const commentMatch = raw.match(/\/\/\s*(.*)$/);
            const desc = commentMatch ? commentMatch[1].trim() : '';
            const code = raw.replace(/\/\/.*$/, '').replace(/\/\*[\s\S]*?\*\//g, '').trim();
            if (!code) continue;
            // A line may hold one value (multi-line enum, may carry a // desc) or
            // several comma-separated values (single-line enum, no per-value desc).
            const tokens = code.split(',').map(t => t.trim()).filter(Boolean);
            for (const tok of tokens) {
                const vm = tok.match(/^([A-Za-z_]\w*)\s*(?:=\s*(-?\w+))?$/);
                if (!vm) continue;
                let val = next;
                if (vm[2] !== undefined) { const parsed = parseInt(vm[2]); if (!isNaN(parsed)) val = parsed; }
                values.push({ name: vm[1], value: val, desc: tokens.length === 1 ? desc : '' });
                next = val + 1;
            }
        }
        if (values.length) enums.push({ name, header, values });
    }
}

module.exports = { enums };

if (require.main === module) {
    if (process.argv.includes('--names')) {
        for (const e of enums) console.log(`  ${e.name}  (${e.header})  [${e.values.length}]`);
        console.log(`\n${enums.length} enums`);
        process.exit(0);
    }
    const esc = (s) => s.replace(/"/g, '\\"');
    let out = '';
    for (const e of enums) {
        out += `  - name: ${e.name}\n    description: ""\n    description_ja: ""\n    description_ko: ""\n    sketch: true\n    values:\n`;
        for (const v of e.values) {
            out += `      - name: ${v.name}\n        value: ${v.value}\n`;
            if (v.desc) out += `        description: "${esc(v.desc)}"\n`;
        }
        out += '\n';
    }
    console.log(out);
}
