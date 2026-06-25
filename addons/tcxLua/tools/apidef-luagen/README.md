# apidef-luagen (PoC)

Generate tcxLua (Sol2) Lua bindings from the canonical `docs/api-definition.yaml`,
instead of parsing headers with the libclang `bindgen`. The YAML carries clean
C++ types now, so generation is straightforward and — crucially — **the generated
C++ is verified by COMPILING it** against the real headers (witness principle):
a wrong name/type fails the build.

## Run
```bash
npm i js-yaml          # once
node apidef-luagen.js ../../../../docs/api-definition.yaml > ../../src/generated/trussc_generated.cpp
```

## Status
- **Phase 1 = free functions.** Generates ~396 bindings (vs ~208 from libclang
  bindgen — roughly 2×, and includes previously-missing APIs like
  `beginStroke`/`vertex`/math/sound).
- Type members (usertypes, operators) = **Phase 2** (skipped: entries with `self:`).

## How it handles the YAML
- Free function = `categories[].functions[]` entry **without** `self:`.
- Lambda params: typed list (`params`) paired with names (`params_simple`);
  `[opt]` brackets stripped; trailing default args expanded into arity overloads.
- Calls are emitted **unqualified** under `using namespace trussc; using namespace std;`
  so both TrussC funcs (`drawRect`) and std math (`sin`) resolve.

## Known tail (surfaced by the compile-witness; needs follow-up)
- **Non-copyable return types** (`events`→CoreEvents, `getLogger`→Logger,
  `getMicInput`→MicInput): generator should emit reference-returning lambdas (Phase 2).
- **Namespace ambiguity** (`random`: tc:: vs std::): `using`-directives can't
  disambiguate names present in both. Cleanest fix: a per-symbol namespace /
  qualified-call hint in the YAML (like static `cppName`).
- **Templates** (`toString`, `stringReplace`): not all caught by the `\bT\b` skip.
- **Sound ADSR builder** (`audioOut`/`wave`/`adsr`/...): `params` vs
  `params_simple` name mismatches — likely YAML data to reconcile.
- Excluded by rule: template entry points (`runApp`, `runHeadlessApp`),
  `namespace::`-qualified free functions (`bitmapfont::*`, `mcp::*`).

## Witness workflow
Point a tcxLua consumer at this worktree's core and build only the addon lib:
```bash
# in TrussSketch/lua-poc (TRUSSC_DIR repointed to this worktree's core)
cmake --build build-macos --target tcxLua
```
Compile errors name the exact functions to fix/exclude.
