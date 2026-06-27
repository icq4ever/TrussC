# luagen

Generate tcxLua (Sol2) Lua bindings from `docs/reference/reference-data.json`
(the canonical AST-derived artifact). **Fully data-driven — no heuristics, no
denylist, no npm deps.** Correctness is verified by COMPILING the output against
the real headers (witness).

## Run
```bash
node luagen.js ../../../../docs/reference/reference-data.json > ../../src/generated/trussc_generated.cpp
# reference-data.json is gitignored; regenerate it first:
#   node docs/reference/generate.js
```

## What it does (Phase 1 = free functions)
- Binds entries with `kind:"func"`, no `owner`, no `ns` (top-level `trussc::` free fns).
  Type members (`owner`) and sub-namespaced fns (`ns` → Lua tables) are Phase 2.
- Everything is derived from the structured `signatures[].args[]`
  (`{type, name, hasDefault, isRef, isConst, isPointer, isArray}`):
  - lambda decl + call come straight from `type`/`name` (synthesize `aN` if name empty)
  - **bindability** = skip a sig with a non-const lvalue-ref (out-param), raw pointer,
    or C-array arg; skip a fn with `tparams` (template)
  - `hasDefault` expands trailing defaults into arity overloads (Lua optional args)
  - reference returns (`ret` ends in `&`) use `-> decltype(auto)`
  - calls are qualified: `provider:"std"` symbols (sin/cos/min…) → `std::name`,
    everything else → `trussc::name` (unambiguous, no std clash)
  - signatures flagged `tmpl:true` (template-derived phantom overloads) are skipped

## Status
**Witness green** — 410 free functions generated from reference-data.json compile
clean against the real headers (`cmake --build … --target tcxLua`). Skipped:
type members (Phase 2), sub-namespaced fns (`ns` → Lua tables, Phase 2),
templates, and unbindable args (out-params / raw pointers / C arrays). Zero
heuristics, zero denylist — every decision is read from the data.
