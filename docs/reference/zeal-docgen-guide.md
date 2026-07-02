# Zeal / Dash Offline Docset — Build & Usage Guide

`emit-dash.js` turns the canonical `reference-data.json` into a self-contained
**Dash/Zeal docset** you can browse and search entirely offline.

- **What it produces:** a `TrussC.docset` bundle — static HTML (no JavaScript),
  a `docSet.dsidx` SQLite search index, and an `Info.plist`.
- **What's inside:** every public C++ symbol (types, methods, fields, operators,
  free functions, enums + values, constants, macros, type aliases) plus the
  `docs/*.md` guides converted to HTML. Descriptions render in **English by
  default**; pass `--lang all` (or e.g. `--lang en,ko`) for the other languages.
- **Where it fits:** a downstream consumer of the reference pipeline, alongside
  `emit-web.js` / `emit-of.js` / `emit-forai.js`. See [README.md](README.md).

---

## 1. Prerequisites

| Need | For |
|------|-----|
| Node.js ≥ 18 | running the scripts |
| `npm install` (in `docs/`) | pulls `sql.js` (WASM SQLite, no native build) + `marked` |
| **clang** (`clang++`) | ONLY to (re)generate `reference-data.json` — the AST step |

`emit-dash.js` itself is pure JS and runs anywhere. Clang is needed **only** by
`generate.js`, which derives the API structure from the real C++ AST.

```bash
cd docs
npm install
```

---

## 2. Build the docset

### 2a. Generate `reference-data.json` (needs clang)

`reference-data.json` is a gitignored, auto-generated artifact. Produce it first:

**macOS / Linux** (clang is the default toolchain here):
```bash
cd docs/reference
node --max-old-space-size=8192 generate.js
```

**Windows** — install LLVM and run from a VS developer shell so clang finds the
Windows SDK / MSVC STL headers (`<windows.h>`, the C++ standard library):
```powershell
winget install LLVM.LLVM            # or the VS "C++ Clang tools" component
# open "Developer PowerShell for VS 2022", then:
cd C:\TrussC\docs\reference
$env:CXX = "clang++"                # generate.js defaults CXX to "c++"
node --max-old-space-size=8192 generate.js
```
If clang can't find headers, run inside the developer shell (it sets `INCLUDE`),
or pass paths explicitly via `$env:TC_COVERAGE_CXXFLAGS` (see [README.md](README.md)).

> No clang handy? Generate `reference-data.json` on a machine that has it (or in
> CI) and copy the file over — then skip straight to 2b with `--data`.

### 2b. Emit the docset (pure JS, no clang)

```bash
cd docs/reference
node emit-dash.js
# or, from docs/:  npm run docset
```

Output: `docs/reference/build/TrussC.docset` (gitignored).

**Languages.** By default only English descriptions are emitted (the Dash
convention — one language, no per-language chips). To bundle **all languages**
(English + 日本語 + 한국어) into a single docset, pass `--lang all`:

```bash
node emit-dash.js --lang all        # en + ja + ko, each shown as a labelled block
node emit-dash.js --lang en,ko      # or pick a subset explicitly
```

`reference-data.json` always carries all three languages, so this is purely a
render-time choice — no need to regenerate the data to switch languages.

---

## 3. Load it in Zeal / Dash

Zeal has no "add local docset" dialog — you place the bundle in its docsets
folder and restart:

| OS | Docsets folder |
|----|----------------|
| Windows | `%LOCALAPPDATA%\Zeal\Zeal\docsets\` |
| Linux | `~/.local/share/Zeal/Zeal/docsets/` |
| macOS (Dash) | Dash → Preferences → Docsets → drag the `.docset` in |

Copy `TrussC.docset` there, restart Zeal, and "TrussC" appears in the sidebar.
Search `Color::fromHSB`, browse guides, and use the per-page table of contents
(driven by the embedded `dashAnchor` markers).

---

## 4. Options

```
node emit-dash.js [--data <reference-data.json>]  # source data (default: ./reference-data.json)
                  [--out  <TrussC.docset>]        # output bundle (default: ./build/TrussC.docset)
                  [--docs <dir>]                  # dir holding the *.md guides (default: ../)
                  [--lang en|ko|ja|all|en,ko]     # description language(s) (default: en)
                  [--dry]                         # print the plan, write nothing
                  [--strict]                      # treat schema warnings as errors (exit 1)
```

---

## 5. Keeping in sync with `main`

This lives on a feature branch. When `main` moves:

```bash
git checkout feat/zeal-document-generator
git merge main            # (or: git rebase main)
node reference/emit-dash.js   # rebuild
```

Conflicts are unlikely — `emit-dash.js` is a new file and the other changes are
additive (`package.json`, `.gitignore`, `README.md`).

**The one thing to watch: `reference-data.json` schema drift.** `emit-dash.js`
reads fields by name (`kind`, `name`, `owner`, `ns`, `signatures[].ret/params`,
`description.{en,ja,ko}`, `deprecated.reason`, …). If the pipeline **renames or
restructures** a field, the emitter silently reads `undefined` and renders empty
sections — the build still "succeeds."

To catch this, `emit-dash.js` runs sanity checks and prints warnings such as:

```
emit-dash: 1 schema warning(s) — reference-data.json may have drifted:
  ! unknown kind value(s): class — these are not rendered; the 'kind' vocabulary may have changed.
```

Run with **`--strict`** in CI (or after a big merge) to make any such warning a
hard failure instead of a quietly incomplete docset:

```bash
node reference/emit-dash.js --strict
```

If a warning fires, reconcile the affected field in `emit-dash.js` against the
schema documented in [README.md](README.md) §"`reference-data.json` — the
canonical format", then rebuild.

---

## 6. Troubleshooting

| Symptom | Cause / fix |
|---------|-------------|
| `reference-data.json not found` | Run `generate.js` first (needs clang), or pass `--data <path>` to a prebuilt copy. |
| `'windows.h' file not found` (Windows) | Run `generate.js` from "Developer PowerShell for VS 2022"; set `$env:CXX="clang++"`. |
| schema warnings printed | See §5 — a `reference-data.json` field likely changed; update `emit-dash.js`. |
| empty / tiny docset | Same as above; run with `--strict` to fail loudly and see which check tripped. |
| TrussC not in Zeal | Copy the `.docset` into the docsets folder (§3) and **restart** Zeal. |
