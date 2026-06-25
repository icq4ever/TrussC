# `docs/tests/` — api-definition contract checks

These scripts verify that [`../api-definition.yaml`](../api-definition.yaml) — the
single source of truth for the documented API — stays in sync with the **real
C++ API** in `core/include/`. They are *checks*, not generators: nothing here
writes docs. (The doc generators live next door in [`../scripts/`](../scripts/).)

The contract is verified in **both directions**, with the compiler as the
oracle in each:

| Direction | Question | Tool | Status |
|-----------|----------|------|--------|
| **C++ → yaml** | Is every public symbol documented? | `coverage-audit.js` | ✅ |
| **yaml → C++** | Does every documented symbol exist with the documented signature? | `signature-probe.js` | 🚧 (productionizing from scratchpad) |

Why two tools: neither half implies the other. The probe proves the yaml isn't
*lying* (no entry describes a signature the compiler rejects); coverage proves
the yaml isn't *incomplete* (no public symbol is silently undocumented).

## coverage-audit.js  (C++ → yaml)

Enumerates the real public surface from a `clang -ast-dump=json` of
`#include <TrussC.h>`, drops anything under an excluded namespace (declared in
`api-definition.yaml` → `coverage.excluded_namespaces`), then diffs what remains
against the documented symbols.

```sh
node docs/tests/coverage-audit.js              # human-readable report
node docs/tests/coverage-audit.js --json       # machine-readable residual
node docs/tests/coverage-audit.js --ast <file> # reuse a cached AST dump
node docs/tests/coverage-audit.js --save-ast <file>
```

- **Report-only.** It never fails a build. The residual mixes genuine doc gaps
  with intentional-but-public internals; bucketing it into *document / hidden /
  ignore* is the one-time closed-world triage, not a per-run gate.
- The full dump is ~280 MB; the script re-execs itself with a larger V8 heap.
- macOS/Metal compile flags are the default. On another host set
  `TC_COVERAGE_CXXFLAGS` to your own flags — the public surface is mostly
  platform-independent, but the dump must compile.

## signature-probe.js  (yaml → C++)  — in progress

Generates a `.cpp` that `static_cast`s the address of each documented symbol to
its documented signature (`static_cast<Ret (Owner::*)(P) const>(&Owner::name)`),
so the **compiler** checks exact params / return / const-ness — no fragile
string comparison. A `-fsyntax-only` compile is enough for the type checks;
the existing example builds already cover linkage of out-of-line definitions.

Currently lives in the session scratchpad (`gen-probe.js` + `patch.js` +
`driver.sh`); productionizing it here — including support for the
`operators:` / `free_operators:` schema — is the next step.

## Dependencies

Node deps (just `js-yaml`) are declared in [`../package.json`](../package.json)
and resolve from `docs/node_modules` (shared with `../scripts/`). Run
`npm install` in `docs/` once.
