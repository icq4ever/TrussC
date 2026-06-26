#!/usr/bin/env node
/**
 * Generate documentation from api-definition.yaml
 *
 * Usage:
 *   node generate-docs.js                  # Generate all outputs
 *   node generate-docs.js --sketch         # Generate TrussSketch-related files only
 *   node generate-docs.js --trussc-api     # Generate TrussC API JS (all APIs + version)
 *   node generate-docs.js --reference      # Inject API index + addon list into FOR_AI_ASSISTANT.md
 *   node generate-docs.js --of-mapping     # Generate oF mapping JSON for website
 *   node generate-docs.js --of-markdown    # Generate oF comparison markdown
 *
 * Outputs:
 *   --sketch:
 *     - ../trussc.org/generated/trusssketch-api.js
 *   --trussc-api:
 *     - ../trussc.org/generated/trussc-api.js (combined, all languages inline)
 *   --reference:
 *     - ../FOR_AI_ASSISTANT.md (API-INDEX + ADDON-LIST marker sections injected)
 *   --of-mapping:
 *     - ../trussc.org/generated/of-mapping.json
 *   --of-markdown:
 *     - ../TrussC_vs_openFrameworks.md (Section 5)
 */

const fs = require('fs');
const path = require('path');
const yaml = require('js-yaml');
const { execSync } = require('child_process');
const { categoryMapping, typeCategoryMapping, ofOnlyEntries } = require('./of-category-mapping.js');
const { buildReverseIndex } = require('./scan-examples.js');

// Paths
const API_YAML = path.join(__dirname, '../api-definition.yaml');
const SKETCH_API_JS = path.join(__dirname, '../../../trussc.org/generated/trusssketch-api.js');
const TRUSSC_API_JS = path.join(__dirname, '../../../trussc.org/generated/trussc-api.js');
const GENERATED_DIR = path.join(__dirname, '../../../trussc.org/generated');
const FOR_AI_MD = path.join(__dirname, '../FOR_AI_ASSISTANT.md');
const ADDONS_REPO = path.join(__dirname, '../../../trussc-addons');
const EXAMPLES_JSON = path.join(__dirname, '../../../trussc.org/examples/examples.json');
const OF_MAPPING_JSON = path.join(__dirname, '../../../trussc.org/generated/of-mapping.json');
const OF_COMPARISON_MD = path.join(__dirname, '../TrussC_vs_openFrameworks.md');

// Parse command line args
const args = process.argv.slice(2);
const generateAll = args.length === 0;
const generateSketch = generateAll || args.includes('--sketch');
const generateTrussCApi = generateAll || args.includes('--trussc-api');
const generateReference = generateAll || args.includes('--reference');
const generateOfMapping = generateAll || args.includes('--of-mapping');
const generateOfMarkdown = generateAll || args.includes('--of-markdown');

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
function generateOfMappingJson(api) {
    // Group functions by display category
    const categoryGroups = {};

    for (const cat of api.categories) {
        const mapping = categoryMapping[cat.id];
        if (!mapping) continue;

        const displayId = mapping.id;
        if (!categoryGroups[displayId]) {
            categoryGroups[displayId] = {
                id: displayId,
                name: mapping.name,
                name_ja: mapping.name_ja,
                name_ko: mapping.name_ko || '',
                order: mapping.order,
                mappings: []
            };
        }

        // Add functions that have of_equivalent
        for (const fn of cat.functions) {
            if (fn.of_equivalent) {
                // Use of_signature_index to pick which overload to display (default: 0)
                const sigIdx = fn.of_signature_index || 0;
                const sig = fn.signatures[sigIdx] || fn.signatures[0];
                const params = simpleParams(sig.params);
                categoryGroups[displayId].mappings.push({
                    of: fn.of_equivalent,
                    tc: fn.name + (params ? `(${params})` : '()'),
                    notes: fn.of_notes || '',
                    notes_ja: fn.of_notes_ja || fn.of_notes || '',
                    notes_ko: fn.of_notes_ko || ''
                });
            }
        }
    }

    // Add ofOnlyEntries (categories/functions not yet in YAML)
    for (const entry of ofOnlyEntries) {
        if (!categoryGroups[entry.category]) {
            categoryGroups[entry.category] = {
                id: entry.category,
                name: entry.name,
                name_ja: entry.name_ja,
                name_ko: entry.name_ko || '',
                order: entry.order,
                mappings: []
            };
        }
        for (const e of entry.entries) {
            categoryGroups[entry.category].mappings.push({
                of: e.of,
                tc: e.tc,
                notes: e.notes || '',
                notes_ja: e.notes_ja || e.notes || '',
                notes_ko: e.notes_ko || ''
            });
        }
    }

    // Convert to sorted array
    const functions = Object.values(categoryGroups)
        .filter(cat => cat.mappings.length > 0)
        .sort((a, b) => a.order - b.order);

    // Group types by of_type_category
    const typeGroups = {};

    if (api.types) {
        for (const type of api.types) {
            if (!type.of_type_category) continue;

            const catDef = typeCategoryMapping[type.of_type_category];
            if (!catDef) continue;

            const catId = catDef.id;
            if (!typeGroups[catId]) {
                typeGroups[catId] = {
                    id: catDef.id,
                    name: catDef.name,
                    name_ja: catDef.name_ja,
                    name_ko: catDef.name_ko || '',
                    order: catDef.order,
                    mappings: []
                };
            }

            typeGroups[catId].mappings.push({
                of: type.of_equivalent || '-',
                tc: type.name,
                notes: type.description || '',
                notes_ja: type.description_ja || type.description || '',
                notes_ko: type.description_ko || ''
            });
        }
    }

    const types = Object.values(typeGroups)
        .sort((a, b) => a.order - b.order);

    return JSON.stringify({ functions, types }, null, 2);
}

// Generate oF comparison markdown (Section 5)
function generateOfMarkdownSection(api) {
    // Same grouping logic as JSON
    const categoryGroups = {};

    for (const cat of api.categories) {
        const mapping = categoryMapping[cat.id];
        if (!mapping) continue;

        const displayId = mapping.id;
        if (!categoryGroups[displayId]) {
            categoryGroups[displayId] = {
                id: displayId,
                name: mapping.name,
                order: mapping.order,
                mappings: []
            };
        }

        for (const fn of cat.functions) {
            if (fn.of_equivalent) {
                const params = simpleParams(fn.signatures[0] && fn.signatures[0].params);
                categoryGroups[displayId].mappings.push({
                    of: fn.of_equivalent,
                    tc: fn.name + (params ? `(${params})` : '()'),
                    notes: fn.of_notes || ''
                });
            }
        }
    }

    // Add ofOnlyEntries
    for (const entry of ofOnlyEntries) {
        if (!categoryGroups[entry.category]) {
            categoryGroups[entry.category] = {
                id: entry.category,
                name: entry.name,
                order: entry.order,
                mappings: []
            };
        }
        for (const e of entry.entries) {
            categoryGroups[entry.category].mappings.push({
                of: e.of,
                tc: e.tc,
                notes: e.notes || ''
            });
        }
    }

    // Convert to sorted array
    const categories = Object.values(categoryGroups)
        .filter(cat => cat.mappings.length > 0)
        .sort((a, b) => a.order - b.order);

    // Generate markdown
    let md = '';
    for (const cat of categories) {
        md += `### **${cat.name}**\n\n`;
        md += '| openFrameworks | TrussC | Notes |\n';
        md += '|:---|:---|:---|\n';
        for (const m of cat.mappings) {
            md += `| \`${m.of}\` | \`${m.tc}\` | ${m.notes} |\n`;
        }
        md += '\n';
    }

    return md;
}

// Update TrussC_vs_openFrameworks.md with auto-generated section
function updateOfComparisonMarkdown(api) {
    const START_MARKER = '<!-- AUTO-GENERATED-START -->';
    const END_MARKER = '<!-- AUTO-GENERATED-END -->';

    let content = fs.readFileSync(OF_COMPARISON_MD, 'utf8');
    const generatedSection = generateOfMarkdownSection(api);

    const startIdx = content.indexOf(START_MARKER);
    const endIdx = content.indexOf(END_MARKER);

    if (startIdx === -1 || endIdx === -1) {
        console.log('  Warning: Markers not found in TrussC_vs_openFrameworks.md');
        console.log('  Add <!-- AUTO-GENERATED-START --> and <!-- AUTO-GENERATED-END --> markers');
        return null;
    }

    const newContent = content.substring(0, startIdx + START_MARKER.length) +
        '\n\n' + generatedSection +
        content.substring(endIdx);

    return newContent;
}

// --- FOR_AI_ASSISTANT.md injection -----------------------------------------

// True if a signature/return string uses AngelScript-only syntax (script factories, not C++).
function isScriptOnly(s) {
    return /@|&in\b|&out\b/.test(s || '');
}

// Build the curated C++ API index (one line per signature) for FOR_AI_ASSISTANT.md.
function buildApiIndexSection(api) {
    let md = '_Auto-generated C++ API index from `api-definition.yaml`. Core symbols only — addons are listed in the next section._\n';
    md += '_Curated subset: some low-level / internal APIs may be omitted. For the interactive reference see https://trussc.org/reference/._\n\n';

    for (const cat of api.categories) {
        if (cat.index === false) continue;
        const lines = [];
        for (const fn of cat.functions) {
            if (fn.index === false) continue;
            const ret = (fn.return !== undefined && fn.return !== null) ? fn.return : 'void';
            if (isScriptOnly(ret)) continue;
            for (const sig of fn.signatures) {
                const params = sig.params || '';
                if (isScriptOnly(params)) continue;
                const desc = fn.description ? `  // ${fn.description}` : '';
                lines.push(`${ret} ${fn.name}(${params})${desc}`);
            }
        }
        if (!lines.length) continue;
        md += `### ${cat.name}\n\n\`\`\`cpp\n${lines.join('\n')}\n\`\`\`\n\n`;
    }

    if (api.types) {
        const typeBlocks = [];
        for (const type of api.types) {
            if (type.index === false) continue;
            const lines = [];
            const addMethod = (m) => {
                const ret = m.return || 'void';
                if (isScriptOnly(ret)) return;
                for (const s of m.signatures || []) {
                    const p = s.params || '';
                    if (isScriptOnly(p)) continue;
                    lines.push(`${ret} ${m.name}(${p})${m.description ? `  // ${m.description}` : ''}`);
                }
            };
            if (type.constructor && type.constructor.signatures) {
                for (const s of type.constructor.signatures) {
                    const p = s.params || '';
                    if (isScriptOnly(p)) continue;
                    lines.push(`${type.name}(${p})`);
                }
            }
            if (type.properties) {
                for (const p of type.properties) {
                    if (isScriptOnly(p.type)) continue;
                    lines.push(`${p.type || ''} ${p.name}${p.description ? `  // ${p.description}` : ''}`.trim());
                }
            }
            if (type.methods) type.methods.forEach(addMethod);
            if (type.static_methods) type.static_methods.forEach(addMethod);
            for (const o of (type.operators || [])) lines.push(`${opCpp(o, type.name)}${o.description ? `  // ${o.description}` : ''}`);
            for (const o of (type.free_operators || [])) lines.push(`friend ${opCpp(o, type.name)}${o.description ? `  // ${o.description}` : ''}`);
            if (!lines.length) continue;
            typeBlocks.push(`#### ${type.name}${type.description ? ` — ${type.description}` : ''}\n\n\`\`\`cpp\n${lines.join('\n')}\n\`\`\``);
        }
        if (typeBlocks.length) md += `### Types\n\n${typeBlocks.join('\n\n')}\n\n`;
    }

    if (api.enums && api.enums.length) {
        const blocks = api.enums.filter(e => e.index !== false).map(e => {
            const vals = (e.values || []).map(v => {
                const val = (v.value !== undefined && v.value !== null) ? ` = ${v.value}` : '';
                return `  ${v.name}${val},${v.description ? `  // ${v.description}` : ''}`;
            }).join('\n');
            const ops = [...(e.operators || []), ...(e.free_operators || [])]
                .map(o => `\nfriend ${opCpp(o, e.name)};${o.description ? `  // ${o.description}` : ''}`).join('');
            return `enum class ${e.name} {${e.description ? `  // ${e.description}` : ''}\n${vals}\n}${ops}`;
        });
        if (blocks.length) md += `### Enums\n\n\`\`\`cpp\n${blocks.join('\n\n')}\n\`\`\`\n\n`;
    }

    if (api.macros && api.macros.length) {
        const lines = api.macros.filter(m => m.index !== false)
            .map(m => `${m.signature || m.name}${m.description ? `  // ${m.description}` : ''}`);
        if (lines.length) md += `### Macros\n\n\`\`\`cpp\n${lines.join('\n')}\n\`\`\`\n\n`;
    }

    if (api.constants) {
        const lines = api.constants
            .filter(c => c.index !== false)
            .map(c => `${c.name} = ${c.value}${c.description ? `  // ${c.description}` : ''}`);
        if (lines.length) md += `### Constants\n\n\`\`\`cpp\n${lines.join('\n')}\n\`\`\`\n`;
    }

    return md.trimEnd();
}

// Build the addon list from the TrussC-org/trussc-addons gh-pages registry.
function buildAddonListSection() {
    let raw;
    try {
        raw = execSync(`git -C "${ADDONS_REPO}" show origin/gh-pages:registry.json`, { encoding: 'utf8' });
    } catch (e) {
        console.log('  Warning: could not read addon registry (origin/gh-pages:registry.json): ' + e.message);
        return null;
    }
    let reg;
    try { reg = JSON.parse(raw); } catch (e) {
        console.log('  Warning: addon registry.json parse failed: ' + e.message);
        return null;
    }
    const addons = (reg.addons || []).slice().sort((a, b) => a.name.localeCompare(b.name));
    const fmt = a => `- **${a.name}** — ${a.description || ''}${a.category ? ` [${a.category}]` : ''}`;
    const bundled = addons.filter(a => a.bundled);
    const community = addons.filter(a => !a.bundled);

    let md = '_Auto-generated from the TrussC-org/trussc-addons registry. Add one with `trusscli addon add <name>`._\n\n';
    if (bundled.length) md += `**Bundled (ship with TrussC):**\n\n${bundled.map(fmt).join('\n')}\n\n`;
    if (community.length) md += `**Community:**\n\n${community.map(fmt).join('\n')}\n`;
    return md.trimEnd();
}

// Splice content between a marker pair; returns updated string or null if markers missing.
function spliceMarkers(content, startMarker, endMarker, section) {
    const s = content.indexOf(startMarker);
    const e = content.indexOf(endMarker);
    if (s === -1 || e === -1) {
        console.log(`  Warning: markers ${startMarker} / ${endMarker} not found in FOR_AI_ASSISTANT.md`);
        return null;
    }
    return content.substring(0, s + startMarker.length) + '\n\n' + section + '\n\n' + content.substring(e);
}

// Inject the API index + addon list into FOR_AI_ASSISTANT.md marker sections.
function updateForAiAssistant(api) {
    let content = fs.readFileSync(FOR_AI_MD, 'utf8');
    let changed = false;

    const apiSection = buildApiIndexSection(api);
    const afterApi = spliceMarkers(content, '<!-- API-INDEX-START -->', '<!-- API-INDEX-END -->', apiSection);
    if (afterApi) { content = afterApi; changed = true; }

    const addonSection = buildAddonListSection();
    if (addonSection) {
        const afterAddons = spliceMarkers(content, '<!-- ADDON-LIST-START -->', '<!-- ADDON-LIST-END -->', addonSection);
        if (afterAddons) { content = afterAddons; changed = true; }
    }

    return changed ? content : null;
}

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

    // Inject the curated C++ API index + addon list into FOR_AI_ASSISTANT.md
    if (generateReference) {
        console.log('\nInjecting API index + addon list into FOR_AI_ASSISTANT.md...');
        const updated = updateForAiAssistant(api);
        if (updated) {
            fs.writeFileSync(FOR_AI_MD, updated);
            console.log(`  Updated: ${FOR_AI_MD}`);
        } else {
            console.log('  Skipped FOR_AI_ASSISTANT.md (no markers present yet).');
        }
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

    // Generate oF mapping JSON
    if (generateOfMapping) {
        console.log('\nGenerating of-mapping.json...');
        const json = generateOfMappingJson(api);
        fs.writeFileSync(OF_MAPPING_JSON, json);
        console.log(`  Written: ${OF_MAPPING_JSON}`);
    }

    // Update oF comparison markdown
    if (generateOfMarkdown) {
        console.log('\nUpdating TrussC_vs_openFrameworks.md...');
        const updatedMd = updateOfComparisonMarkdown(api);
        if (updatedMd) {
            fs.writeFileSync(OF_COMPARISON_MD, updatedMd);
            console.log(`  Updated: ${OF_COMPARISON_MD}`);
        }
    }

    console.log('\nDone!');
}

main();
