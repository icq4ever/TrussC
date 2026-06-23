# oracle-screen-alpha — DISPOSABLE render check (delete before merge)

A throwaway deterministic render app for `fix/screen-alpha-premult`. It is **not**
a permanent test and is **not** wired into CI. It exists so the Screen/Multiply
alpha fix can be eyeballed on a real GPU on each backend (Metal / D3D11 / GL),
since the change is about pixel output and the headless core tests can't see
pixels. Delete this directory before merging — the deletion commit keeps it
salvageable from history.

## Build & run

```bash
# from the repo root (uses the worktree's own trusscli; --tc-root points here)
trusscli run -p oracle-screen-alpha
# or manually:
trusscli update -p oracle-screen-alpha --tc-root .
cd oracle-screen-alpha
cmake --preset macos     # or windows / linux
cmake --build --preset macos
ORACLE_OUT=out.png ./bin/oracle-screen-alpha.app/Contents/MacOS/oracle-screen-alpha   # Win: bin\...\oracle-screen-alpha.exe
```

It renders one frame, writes a screenshot to `$ORACLE_OUT` (default `oracle.png`),
and quits. `setHighDpi(false)` keeps the framebuffer size deterministic.

## What CORRECT looks like (after the fix)

Orange source bars, alpha 1.0 (top) → 0.0 (bottom), over 4 backdrops:

| section | black bg | gray bg | white bg | red bg |
|---|---|---|---|---|
| **SCREEN** | orange → **black** (smooth) | orange → gray | **all white** | orange → red |
| **MULTIPLY** | **all black** | orange×gray → gray | orange → **white** (smooth) | all red |

The key proof is the **smooth fade to the backdrop** as alpha decreases:
- SCREEN over black and MULTIPLY over white must grade smoothly.
- **Before the fix every bar was full orange** (alpha ignored) — if you still see
  solid orange bars with no fade, the fix isn't active on this backend.
- No dark "halo" should appear around low-alpha Screen bars.

Multiply over a *partially-transparent* or *fully-transparent* backdrop is an
accepted approximation (see the blend-mode notes); this oracle only uses opaque
backdrops, where Multiply is exact.

## Cross-backend

Run it on macOS (Metal), Windows (D3D11) and Linux (GL). The table above must
hold on all three. macOS/Metal is already confirmed.
