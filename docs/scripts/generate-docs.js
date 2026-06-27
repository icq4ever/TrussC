#!/usr/bin/env node
/**
 * Generate the WEB reference data from api-definition.yaml.
 *
 * NOTE: this generator is being superseded by the reference redesign in
 * docs/reference/ (structure from the C++ AST + prose from api-reference.toml,
 * joined into reference-data.json). The FOR_AI index, of-mapping.json and the
 * oF comparison markdown are now produced by docs/reference/emit-forai.js and
 * emit-of.js from reference-data.json — they were REMOVED from here so the two
 * can't fight over the same files. What remains are the two web-player JS files,
 * pending the website's migration to consume reference-data.json directly.
 *
 * Usage:
 *   node generate-docs.js                  # both web outputs
 *   node generate-docs.js --sketch         # trusssketch-api.js only
 *   node generate-docs.js --trussc-api     # trussc-api.js only
 *
 * Outputs:
 *   --sketch:     ../trussc.org/generated/trusssketch-api.js
 *   --trussc-api: ../trussc.org/generated/trussc-api.js (all languages inline)
 */

const fs = require('fs');
const path = require('path');
const yaml = require('js-yaml');
const { execSync } = require('child_process');
const { buildReverseIndex } = require('./scan-examples.js');

// Paths
const API_YAML = path.join(__dirname, '../api-definition.yaml');
const SKETCH_API_JS = path.join(__dirname, '../../../trussc.org/generated/trusssketch-api.js');
const TRUSSC_API_JS = path.join(__dirname, '../../../trussc.org/generated/trussc-api.js');
const EXAMPLES_JSON = path.join(__dirname, '../../../trussc.org/examples/examples.json');

// Parse command line args
const args = process.argv.slice(2);
const generateAll = args.length === 0;
const generateSketch = generateAll || args.includes('--sketch');
const generateTrussCApi = generateAll || args.includes('--trussc-api');

// Load YAML
function loadAPI() {
    const content = fs.readFileSync(API_YAML, 'utf8');
    return yaml.load(content);
}

// --- migration helpers --------------------------------------------------------
// `params_simple` is retired: call-arg names are derived from the typed `params`.
// The `sketch` flag is retired: TrussSketch availability = web-compatible && Lua-bindable.
function splitParams(s) {
    const o = []; let d = 0, c = '';
    for (const ch of String(s || '')) {
        if ('<(['.includes(ch)) d++; else if (')]>'.includes(ch)) d--;
        if (ch === ',' && d === 0) { o.push(c); c = ''; } else c += ch;
    }
    if (c.trim()) o.push(c);
    return o.map(x => x.trim()).filter(Boolean);
}
// "float x, const Vec2& v, int n = 4" -> "x, v, n"
function simpleParams(params) {
    return splitParams(params).map(p => {
        p = p.replace(/=.*$/, '').trim();
        if (p === 'void' || p === '') return null;
        let m = p.match(/([A-Za-z_]\w*)\s*\[[^\]]*\]\s*$/);   // Type name[N]
        if (m) return m[1];
        m = p.match(/([A-Za-z_]\w*)\s*$/);                    // trailing identifier = the name
        return m ? m[1] : null;
    }).filter(Boolean).join(', ');
}
// web-compatible: no platform restriction (universal), or wasm explicitly listed.
function isWeb(entry) {
    const p = entry && entry.platforms;
    if (!Array.isArray(p) || p.length === 0) return true;
    return p.includes('wasm');
}
// TrussSketch availability = web-compatible AND Lua-bindable (`sketch` flag retired).
function isSketchable(entry) {
    return isWeb(entry) && (entry.lua !== false);
}

// Generate trusssketch-api.js
function generateSketchAPI(api) {
    const categories = [];

    for (const cat of api.categories) {
        const sketchFunctions = cat.functions.filter(fn => isSketchable(fn));
        if (sketchFunctions.length === 0) continue;

        const functions = [];

        for (const fn of sketchFunctions) {
                                    // Create entry for each signature
                                    for (const sig of fn.signatures) {
                                        functions.push({
                                            name: fn.name,
                                            params: simpleParams(sig.params),
                                            params_typed: sig.params,
                                            return_type: fn.return !== undefined ? fn.return : null,
                                            desc: fn.description,
                                            snippet: fn.snippet
                                        });
                                    }                    }
            
                    categories.push({
                        name: cat.name,
                        functions: functions
                    });
                }
            
                const constants = api.constants
                    .filter(c => isSketchable(c))
                    .map(c => ({
                        name: c.name,
                        value: c.value,
                        desc: c.description
                    }));

                // Process types (class definitions with properties and methods)
                const types = [];
                if (api.types) {
                    for (const type of api.types) {
                        if (!isSketchable(type)) continue;

                        const typeData = {
                            name: type.name,
                            desc: type.description
                        };

                        // Constructor
                        if (type.constructor && type.constructor.signatures) {
                            typeData.constructor = {
                                signatures: type.constructor.signatures.map(s => s.params || ''),
                                snippet: type.constructor.snippet
                            };
                        }

                        // Properties
                        if (type.properties) {
                            typeData.properties = type.properties.map(p => ({
                                name: p.name,
                                type: p.type,
                                desc: p.description
                            }));
                        }

                        // Methods (instance)
                        if (type.methods) {
                            typeData.methods = type.methods.map(m => ({
                                name: m.name,
                                return: m.return,
                                signatures: m.signatures.map(s => s.params || ''),
                                desc: m.description,
                                snippet: m.snippet
                            }));
                        }

                        // Static methods
                        if (type.static_methods) {
                            typeData.static_methods = type.static_methods.map(m => ({
                                name: m.name,
                                return: m.return,
                                signatures: m.signatures.map(s => s.params || ''),
                                desc: m.description,
                                snippet: m.snippet
                            }));
                        }

                        types.push(typeData);
                    }
                }

                const output = {
                    categories: categories,
                    constants: constants,
                    keywords: api.keywords,
                    types: types
                };
            
                // Generate JavaScript source
                let js = `// TrussSketch API Definition
            // This is the single source of truth for all TrussSketch functions.
            // Used by: autocomplete, reference page, REFERENCE.md generation
            //
            // AUTO-GENERATED from api-definition.yaml
            // Do not edit directly - edit api-definition.yaml instead
            
            const TrussSketchAPI = ${JSON.stringify(output, null, 4)};
            
            // Export for different environments
            if (typeof module !== 'undefined' && module.exports) {
                module.exports = TrussSketchAPI;
            }
            `;
            
                return js;
            }

// Get version from latest git tag
function getVersion() {
    try {
        return execSync('git describe --tags --abbrev=0', { cwd: path.join(__dirname, '../..') })
            .toString().trim();
    } catch {
        return 'unknown';
    }
}

// Pick a localized string for the given language, falling back to the base (English) value.
// lang === null => combined output (keeps base + *_ja/*_ko variants, used by legacy trussc-api.js)
function pickLang(base, ja, ko, lang) {
    if (lang === 'ja') return ja || base || '';
    if (lang === 'ko') return ko || base || '';
    return base || '';
}

// --- Operator rendering helpers (operators: / free_operators: schema) -------
// Operators carry symbol / rhs / result (+ lhs for free, unary flag). const-ness
// is the C++ convention, not stored: compound-assignment and [] mutate, the rest
// (value-returning binary/unary/comparison) are const.
function opIsConst(sym) { return !(/=$/.test(sym) && !/^(==|!=|<=|>=)$/.test(sym)) && sym !== '[]'; }
const _bare = (t) => String(t || '').replace(/^const\s+/, '').replace(/\s*&$/, '').trim();
// Human-readable form, e.g. "Vec2 + Vec2 → Vec2", "-Vec2 → Vec2", "float * Vec2 → Vec2".
function opDisplay(op, owner) {
    const s = op.symbol, res = op.result;
    if (op.lhs !== undefined) return `${_bare(op.lhs)} ${s} ${_bare(op.rhs)} → ${res}`;
    if (op.unary) return `${s}${owner} → ${res}`;
    if (s === '[]') return `${owner}[${_bare(op.rhs)}] → ${res}`;
    return `${owner} ${s} ${_bare(op.rhs)} → ${res}`;
}
// C++ declaration form, e.g. "Vec2 operator+(const Vec2&) const".
function opCpp(op, owner) {
    const res = op.result || owner;
    if (op.lhs !== undefined) return `${res} operator${op.symbol}(${op.lhs}, ${op.rhs})`;
    if (op.unary) return `${res} operator${op.symbol}()${opIsConst(op.symbol) ? ' const' : ''}`;
    return `${res} operator${op.symbol}(${op.rhs || ''})${opIsConst(op.symbol) ? ' const' : ''}`;
}
// Reference-data objects for one owner's operators (members + free), language-picked.
function mapOperators(src, owner, lang) {
    const all = [...(src.operators || []).map(o => ({ ...o })), ...(src.free_operators || []).map(o => ({ ...o, free: true }))];
    return all.map(o => ({
        symbol: o.symbol,
        signature: opDisplay(o, owner),
        cpp: opCpp(o, owner),
        free: !!o.free,
        desc: lang ? pickLang(o.description, o.description_ja, o.description_ko, lang) : o.description,
        ...(lang ? {} : { desc_ja: o.description_ja || '', desc_ko: o.description_ko || '' }),
    }));
}

// Optional platform-support annotation. Absent `platforms` = universal (all platforms);
// only restricted symbols carry it. Tokens: linux/windows/macos/ios/android/wasm.
// Mutates `entry` in place; no-op when `src.platforms` is absent/empty.
function attachPlatforms(entry, src, lang) {
    if (!Array.isArray(src.platforms) || !src.platforms.length) return entry;
    entry.platforms = src.platforms;
    if (src.platformNote) {
        entry.platformNote = lang
            ? pickLang(src.platformNote, src.platformNote_ja, src.platformNote_ko, lang)
            : src.platformNote;
        if (!lang) {
            if (src.platformNote_ja) entry.platformNote_ja = src.platformNote_ja;
            if (src.platformNote_ko) entry.platformNote_ko = src.platformNote_ko;
        }
    }
    return entry;
}

// Build symbol -> [{name, group}] example links.
// Manual `examples:` in the yaml wins; otherwise the auto reverse-index (top 3).
// Only web-playable examples present in examples.json are linked.
const EXAMPLE_CAP = 3;
function buildExamplesMap(api) {
    let exLookup = new Map();   // exampleName -> { group, web }
    try {
        const exJson = JSON.parse(fs.readFileSync(EXAMPLES_JSON, 'utf8'));
        for (const [group, cat] of Object.entries(exJson.examples || {})) {
            for (const it of (cat.items || [])) exLookup.set(it.name, { group, web: it.webSupported !== false });
        }
    } catch (e) {
        console.log(`  Warning: examples.json unreadable (${e.message}); skipping example links`);
        return {};
    }

    // Symbol sets straight from the yaml (avoids depending on generated output).
    const funcs = new Set(), types = new Set(), excluded = new Set();
    for (const c of api.categories || []) {
        for (const f of c.functions || []) {
            if (c.name === 'Lifecycle' || c.name === 'Events') excluded.add(f.name);
            else funcs.add(f.name);
        }
    }
    for (const n of excluded) funcs.delete(n);
    for (const t of api.types || []) types.add(t.name);

    let rev = { index: {} };
    try { rev = buildReverseIndex({ funcs, types, excluded }); }
    catch (e) { console.log(`  Warning: example scan failed (${e.message}); manual links only`); }

    const resolve = (symName, manual) => {
        const names = (Array.isArray(manual) && manual.length)
            ? manual                                            // manual override
            : (rev.index[symName] || []).map(e => e.name);      // auto fallback
        const out = [];
        for (const n of names.slice(0, EXAMPLE_CAP)) {
            const info = exLookup.get(n);
            if (info && info.web) out.push({ name: n, group: info.group });
        }
        return out;
    };

    const map = {};
    for (const c of api.categories || []) for (const f of c.functions || []) {
        const ex = resolve(f.name, f.examples);
        if (ex.length) map[f.name] = ex;
    }
    for (const t of api.types || []) {
        const ex = resolve(t.name, t.examples);
        if (ex.length) map['type:' + t.name] = ex;
    }
    return map;
}

// Generate trussc-api.js (all APIs, no sketch filter).
// lang: null = combined (legacy, retains desc_ja/desc_ko); 'en'|'ja'|'ko' = single-language,
//       localized fields collapsed to the base name so the frontend stays language-agnostic.
// examplesMap: symbol -> [{name, group}] (see buildExamplesMap).
function generateTrussCApiJS(api, lang, examplesMap = {}) {
    const version = getVersion();

    const categories = [];
    for (const cat of api.categories) {
        const functions = [];
        for (const fn of cat.functions) {
            for (const sig of fn.signatures) {
                const entry = {
                    name: fn.name,
                    params: simpleParams(sig.params),
                    params_typed: sig.params || '',
                    return_type: fn.return !== undefined ? fn.return : null,
                    desc: lang ? pickLang(fn.description, fn.description_ja, fn.description_ko, lang) : fn.description,
                    snippet: fn.snippet,
                    keywords: fn.keywords || []
                };
                if (!lang) {
                    entry.desc_ja = fn.description_ja || '';
                    entry.desc_ko = fn.description_ko || '';
                }
                // Optional per-signature description (only when an overload differs).
                if (sig.description) {
                    entry.sigDesc = lang ? pickLang(sig.description, sig.description_ja, sig.description_ko, lang) : sig.description;
                    if (!lang) {
                        if (sig.description_ja) entry.sigDesc_ja = sig.description_ja;
                        if (sig.description_ko) entry.sigDesc_ko = sig.description_ko;
                    }
                }
                // Optional related symbols (language-independent list of names).
                if (Array.isArray(fn.related) && fn.related.length) entry.related = fn.related;
                // Optional deprecation marker ({reason, replacement?, url?}).
                if (fn.deprecated) entry.deprecated = fn.deprecated;
                // Optional symbol-level long-form details (en-first; ja/ko fall back to en).
                if (fn.details) {
                    entry.details = lang ? pickLang(fn.details, fn.details_ja, fn.details_ko, lang) : fn.details;
                    if (!lang) {
                        if (fn.details_ja) entry.details_ja = fn.details_ja;
                        if (fn.details_ko) entry.details_ko = fn.details_ko;
                    }
                }
                // Optional example links (language-independent).
                if (examplesMap[fn.name]) entry.examples = examplesMap[fn.name];
                // Optional platform-support annotation (restricted symbols only).
                attachPlatforms(entry, fn, lang);
                functions.push(entry);
            }
        }
        if (functions.length === 0) continue;
        const catEntry = { name: lang ? pickLang(cat.name, cat.name_ja, cat.name_ko, lang) : cat.name, functions };
        if (!lang) { catEntry.name_ja = cat.name_ja || ''; catEntry.name_ko = cat.name_ko || ''; }
        categories.push(catEntry);
    }

    const constants = (api.constants || []).map(c => ({
        name: c.name,
        value: c.value,
        desc: lang ? pickLang(c.description, c.description_ja, c.description_ko, lang) : c.description,
        keywords: c.keywords || []
    }));

    const types = [];
    if (api.types) {
        for (const type of api.types) {
            const typeData = {
                name: type.name,
                desc: lang ? pickLang(type.description, type.description_ja, type.description_ko, lang) : type.description,
                keywords: type.keywords || []
            };
            if (!lang) {
                typeData.desc_ja = type.description_ja || '';
                typeData.desc_ko = type.description_ko || '';
            }
            if (examplesMap['type:' + type.name]) typeData.examples = examplesMap['type:' + type.name];
            if (Array.isArray(type.related) && type.related.length) typeData.related = type.related;
            attachPlatforms(typeData, type, lang);

            if (type.constructor && type.constructor.signatures) {
                typeData.constructor = {
                    signatures: type.constructor.signatures.map(s => s.params || ''),
                    snippet: type.constructor.snippet
                };
            }

            if (type.properties) {
                typeData.properties = type.properties.map(p => ({
                    name: p.name,
                    type: p.type,
                    desc: lang ? pickLang(p.description, p.description_ja, p.description_ko, lang) : p.description
                }));
            }

            if (type.methods) {
                typeData.methods = type.methods.map(m => attachPlatforms({
                    name: m.name,
                    return: m.return,
                    signatures: m.signatures.map(s => s.params || ''),
                    desc: lang ? pickLang(m.description, m.description_ja, m.description_ko, lang) : m.description,
                    snippet: m.snippet,
                    ...(m.deprecated ? { deprecated: m.deprecated } : {})
                }, m, lang));
            }

            if (type.static_methods) {
                typeData.static_methods = type.static_methods.map(m => attachPlatforms({
                    name: m.name,
                    return: m.return,
                    signatures: m.signatures.map(s => s.params || ''),
                    desc: lang ? pickLang(m.description, m.description_ja, m.description_ko, lang) : m.description,
                    snippet: m.snippet
                }, m, lang));
            }

            if (type.operators || type.free_operators) {
                typeData.operators = mapOperators(type, type.name, lang);
            }

            types.push(typeData);
        }
    }

    const enums = (api.enums || []).map(e => {
        const out = {
            name: e.name,
            desc: lang ? pickLang(e.description, e.description_ja, e.description_ko, lang) : e.description,
            keywords: e.keywords || [],
            values: (e.values || []).map(v => ({
                name: v.name,
                value: v.value,
                desc: lang ? pickLang(v.description, v.description_ja, v.description_ko, lang) : v.description,
            })),
        };
        if (!lang) { out.desc_ja = e.description_ja || ''; out.desc_ko = e.description_ko || ''; }
        if (Array.isArray(e.related) && e.related.length) out.related = e.related;
        if (e.operators || e.free_operators) out.operators = mapOperators(e, e.name, lang);
        return out;
    });

    const macros = (api.macros || []).map(m => {
        const out = {
            name: m.name,
            signature: m.signature || m.name,
            desc: lang ? pickLang(m.description, m.description_ja, m.description_ko, lang) : m.description,
        };
        if (!lang) { out.desc_ja = m.description_ja || ''; out.desc_ko = m.description_ko || ''; }
        return out;
    });

    const output = {
        version: version,
        lang: lang || 'all',
        categories: categories,
        constants: constants,
        keywords: api.keywords,
        types: types,
        enums: enums,
        macros: macros,
        // Named color palette (grouped). Language-independent; rendered as
        // swatches by reference.js (not a flat constant list).
        ...(api.colors ? { colors: api.colors } : {})
    };

    let js = `// TrussC API Definition${lang ? ` (${lang})` : ''}
// Complete C++ API reference (all functions, types, constants)
//
// AUTO-GENERATED from api-definition.yaml
// Do not edit directly - edit api-definition.yaml instead

const TrussCAPI = ${JSON.stringify(output, null, 4)};

// Export for different environments
if (typeof module !== 'undefined' && module.exports) {
    module.exports = TrussCAPI;
}
`;

    return js;
}

// Generate oF mapping JSON for website
// Main
function main() {
    console.log('Loading api-definition.yaml...');
    const api = loadAPI();
    console.log(`  Found ${api.categories.length} categories`);

    if (generateSketch) {
        console.log('\nGenerating trusssketch-api.js...');
        const js = generateSketchAPI(api);
        fs.writeFileSync(SKETCH_API_JS, js);
        console.log(`  Written: ${SKETCH_API_JS}`);
    }

    // Generate TrussC API JS (single combined file, all languages inline).
    // The reference page selects desc/desc_ja/desc_ko by REF_LANG at render time,
    // so one cached file serves en/ja/ko (no per-locale re-download on switch).
    if (generateTrussCApi) {
        console.log('\nGenerating trussc-api.js...');
        const examplesMap = buildExamplesMap(api);
        console.log(`  Example links resolved for ${Object.keys(examplesMap).length} symbols`);
        fs.writeFileSync(TRUSSC_API_JS, generateTrussCApiJS(api, null, examplesMap));
        console.log(`  Written: ${TRUSSC_API_JS}`);
    }


    console.log('\nDone!');
}

main();
