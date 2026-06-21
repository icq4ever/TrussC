# render-target-refactor-oracle

**Branch-local GPU regression harness** for the `refactor/render-target-pipelines`
work (centralizing FBO/swapchain sgl pipeline selection behind `RenderTarget`).

> ⚠️ This is an **exception** to the `core/tests/` "fast & headless, no window/GPU"
> charter: it needs a real GPU/swapchain. It lives here so it travels with this
> branch to Linux/Windows machines for cross-platform verification.
> **Remove it before merging this branch into `dev`** (it is not a permanent test).

## What it is

A **deterministic** TrussC app (fixed RNG seed, no time/frame animation) that draws
one frame exercising the whole pipeline matrix, then saves the swapchain to a
lossless BMP and quits:

- 2D primitives + every `BlendMode` (Alpha/Add/Multiply/Screen/Subtract/Disabled)
- bitmap text, straight-alpha and premultiplied textures
- an FBO with 2D composited back over a non-black backdrop (exercises the alpha channel)
- an FBO containing a 3D PBR scene (3D-in-FBO)
- a 3D PBR scene drawn straight to the swapchain

On frame 4 it writes `$ORACLE_OUT` (default `/tmp/oracle.bmp`) and calls `sapp_quit()`.

`diff.py` compares two BMPs and reports: exact-match (max error 0), differing-pixel
count/%, per-channel max error, mean error, diff bbox, and an optional heatmap.

## Why it can't be a golden image

The render is byte-identical **only on the same machine + same graphics backend**.
Metal (macOS), GL (Linux), D3D11 (Windows) and WGPU (web) differ in rasterization
and blend precision, so we do **not** commit a baseline BMP. Instead the workflow is
self-contained *per platform*: capture a baseline from the **known-good `dev` core**,
then diff the **refactor core** against it on that same machine.

## Verification workflow (run on each platform)

The app/binary name is the **folder name** (`trussc_app()` derives it from the
directory, not from `project()`), so it is `render-target-refactor-oracle`. It runs
one frame, saves `$ORACLE_OUT`, and **quits itself** — run it in the foreground.

```sh
ORACLE=core/tests/render-target-refactor-oracle
APP=render-target-refactor-oracle
BIN=$ORACLE/bin/$APP.app/Contents/MacOS/$APP   # macOS
# BIN=$ORACLE/bin/$APP                         # Linux
# BIN=$ORACLE\bin\$APP.exe                     # Windows

# 1) Baseline: build the oracle against a checkout of the KNOWN-GOOD dev core,
#    then capture (the app self-quits after writing the BMP).
rm -rf /tmp/ob_dev
cmake -S "$ORACLE" -B /tmp/ob_dev -DTRUSSC_DIR=/path/to/dev/core && cmake --build /tmp/ob_dev -j4
ORACLE_OUT=/tmp/base.bmp "$BIN"        # wait for the "[oracle] saved" log line

# 2) Candidate: clean-build the oracle against THIS branch's core, re-capture.
#    (Both builds install into this tree's bin/, so do step 1 fully, then step 2.)
rm -rf /tmp/ob_cur
cmake -S "$ORACLE" -B /tmp/ob_cur -DTRUSSC_DIR=$PWD/core && cmake --build /tmp/ob_cur -j4
ORACLE_OUT=/tmp/cur.bmp "$BIN"

# 3) Compare — must be an exact match (max error 0).
python3 "$ORACLE/diff.py" /tmp/base.bmp /tmp/cur.bmp
```

### Gotchas (learned the hard way)

- **Always CLEAN-build the oracle** (`rm -rf <build-dir>` first). Incremental
  `cmake --build` does not reliably pick up core *header* changes between slices and
  gives false pass/fail.
- **Let the window settle** before trusting a capture: run the binary on its own,
  confirm the `[oracle] saved` log, and don't capture while the bin is being
  overwritten by another build. A 100%-different / all-black result is almost always
  a stale build or a too-soon capture, not a real regression.
- Both builds install the binary into `bin/` in this source tree, so run them
  **sequentially** (capture, then rebuild, then capture again), not into two -B dirs
  expecting two binaries.

## Intentional differences vs `dev`

The refactor unifies the swapchain Fill2D **alpha** policy (alpha now accumulates,
matching the FBO). This changes only the swapchain alpha channel, which is invisible
in RGB on an opaque window — so the oracle (RGB BMP) still reports an exact match.
