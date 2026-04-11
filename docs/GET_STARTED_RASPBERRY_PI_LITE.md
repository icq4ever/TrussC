# Running TrussC on Raspberry Pi OS Lite

Raspberry Pi OS Lite ships without a display server. This guide walks you through the minimal setup needed to **build and run** TrussC apps on Lite.

> **Why this guide exists:** Raspberry Pi 5's `vc4` driver does not play well with raw Xorg + a stand-alone window manager. The standard Raspberry Pi OS Desktop actually runs a Wayland compositor (`labwc`) and uses Xwayland to host X11 apps — so the most reliable way to run TrussC on Lite is to install the same stack.

> **Tested on:** Raspberry Pi 5, Raspberry Pi OS Lite (Trixie / Debian 13), kernel 6.12.

---

## 1. Install Packages

Just run the dependency installer. It detects Raspberry Pi OS Lite automatically and adds `labwc` + `xwayland` on top of the normal build dependencies:

```bash
./projectGenerator/install_dependencies_linux.sh
```

You should see a line like:
```
Detected Raspberry Pi OS Lite — adding labwc + xwayland for runtime
```

That's it. After it finishes, `projectGenerator` and your examples will build and run normally.

The script installs the standard TrussC Debian build dependencies (X11 dev headers, GLES/EGL, GTK, FFmpeg, GStreamer, ALSA, curl, cmake, etc.) and then, **only on raspbian Lite**, adds the following on top:

| Package | Purpose |
|---|---|
| `labwc` | Wayland compositor (the same one Raspberry Pi OS Desktop uses) |
| `xwayland` | X11 server emulator — sokol_app is X11-only, so it runs through Xwayland |

> **How Lite detection works:** The script first checks `/proc/device-tree/model` for the string "Raspberry Pi" — this is a hardware-level signal that works regardless of distro ID. (64-bit Raspberry Pi OS reports `ID=debian` in `/etc/os-release`, not `raspbian`, so distro-ID-based detection would miss it.) If running on a Pi *and* the Desktop meta-package `raspberrypi-ui-mods` is not installed, the script treats it as Lite and adds `labwc` + `xwayland`.

### Permissions

labwc needs to access the GPU (`/dev/dri/*`), input devices (`/dev/input/*`), and DRM render nodes. On a fresh Lite install your user is not in the required groups, so the install script adds them automatically when it detects a Raspberry Pi:

```
Adding pi to groups required by labwc/Wayland: video input render
  Done. You must log out and back in for the new groups to take effect.
```

**You must log out of all sessions (including SSH) and log back in for the new groups to apply.** `groups` should then list `video`, `input`, and `render`.

If you ever need to add them by hand:
```bash
sudo usermod -aG video,input,render $USER
```

> **Note:** These groups solve GPU/input access. They do **not** let you launch labwc over SSH — Wayland compositors also need a real seat (a physical TTY login or a systemd service). See [Section 4](#4-auto-start-on-boot-kiosk-mode) for the systemd-based approach if you want to start the kiosk from SSH.

---

## 2. Why labwc / Xwayland?

TrussC apps need a graphical session. On Pi 5, **`labwc` (Wayland) is the recommended path** — it's exactly what Raspberry Pi OS Desktop uses, and it's the lightest configuration that "just works" with the `vc4` driver.

`xwayland` is required because TrussC apps are X11 clients (sokol_app's Linux backend uses X11 only), and labwc hosts them through Xwayland.

```
TrussC app (X11 client)
    ↓ libX11
Xwayland (X server emulator)
    ↓ Wayland protocol
labwc (Wayland compositor)
    ↓ DRM/KMS
GPU (vc4)
```

This is also why `libx11-dev` and friends are still in the build dependency list — TrussC links against libX11 at compile time, even though the actual display server is Wayland.

---

## 3. Run a TrussC App

### Easiest: `tc-run`

If you accepted the `tc-run` prompt during install, just call it with your app binary:

```bash
cd ~/TrussC/examples/3d/3DPrimitivesExample/bin
tc-run ./3DPrimitivesExample
```

`tc-run` starts a labwc Wayland session and runs your app through Xwayland. Works from any directory once `/usr/local/bin/tc-run` is installed.

### Exiting the session

> **Important:** Closing the app does **not** automatically close labwc. labwc is a full Wayland compositor (the same one that ships with Raspberry Pi OS Desktop), and it stays running after its child app terminates. You'll see an empty desktop with just the cursor.
>
> To exit labwc, **click anywhere on the empty desktop (any mouse button) and choose `Exit`** from the menu that appears. This is the standard labwc shutdown path.
>
> For unattended kiosk use, you don't need to exit manually at all — see [Section 4](#4-auto-start-on-boot-kiosk-mode), where systemd handles restart and shutdown for you.

### Manual equivalent

If you skipped the launcher install or want to understand what `tc-run` does, the underlying command is:

```bash
labwc -s ./3DPrimitivesExample
```

The `-s` flag tells labwc to run a startup command after starting. Same exit caveat as above — close the app, then right-click → Exit.

---

## 4. Auto-Start on Boot (Kiosk Mode)

For exhibition / signage use, you want the Pi to boot straight into your app. Two approaches are common — pick whichever fits your use case.

### Comparison

| | bash_profile hook | systemd service |
|---|---|---|
| **Setup complexity** | Very simple (2 steps) | Moderate (unit file + enable) |
| **Auto-restart on crash** | ❌ No (shell exits, you stare at a black screen) | ✅ Yes (`Restart=on-failure`) |
| **Logs** | Lost / printed to dead tty | ✅ Captured by `journalctl -u trussc-kiosk` |
| **Boot ordering** | After shell login (late) | ✅ Controlled via `After=` / `WantedBy=` |
| **Stops getty cleanly** | ❌ Coexists awkwardly with getty | ✅ `Conflicts=getty@tty1` replaces it |
| **Easy to disable temporarily** | Comment out the lines | `systemctl stop trussc-kiosk` |
| **Best for** | Quick prototyping, dev tinkering | Production exhibitions, unattended kiosks |

**TL;DR:** Use bash_profile while you're iterating, then move to systemd before deploying.

---

### Method A — Shell profile hook (simple)

1. Enable console autologin:
   ```bash
   sudo raspi-config
   ```
   → *System Options* → *Boot / Auto Login* → *Console Autologin*

2. Append the auto-launch snippet to your shell's login profile:

   **bash** — `~/.bash_profile`:
   ```bash
   if [ -z "$WAYLAND_DISPLAY" ] && [ "$(tty)" = "/dev/tty1" ]; then
       cd ~/TrussC/examples/3d/3DPrimitivesExample/bin
       labwc -s ./3DPrimitivesExample
   fi
   ```

   **zsh** — `~/.zprofile` (preferred) or `~/.zshrc`:
   ```zsh
   if [[ -z "$WAYLAND_DISPLAY" && "$(tty)" == "/dev/tty1" ]]; then
       cd ~/TrussC/examples/3d/3DPrimitivesExample/bin
       labwc -s ./3DPrimitivesExample
   fi
   ```

   > **bash_profile vs zprofile vs zshrc:** Login shells source `*_profile` files once at login, which is exactly what an autologin tty needs. `.zshrc` runs for every interactive zsh (including subshells), so the `tty == /dev/tty1` guard is what prevents it from launching labwc again inside nested shells. `.zprofile` is the cleaner choice semantically.

On reboot, the Pi logs in automatically on tty1, the shell sources the profile, and labwc launches your app. If the app crashes you're back at the login shell — fine for development, not great for unattended kiosks.

---

### Method B — systemd service (robust)

This replaces the bash_profile hook entirely. It takes over `tty1` directly, runs labwc as your user, and restarts automatically on failure.

1. Create `/etc/systemd/system/trussc-kiosk.service`:
   ```ini
   [Unit]
   Description=TrussC Kiosk
   After=systemd-user-sessions.service getty@tty1.service
   Conflicts=getty@tty1.service

   [Service]
   Type=simple
   User=<YOUR_USERNAME>                                    # whoami
   PAMName=login
   TTYPath=/dev/tty1
   TTYReset=yes
   TTYVHangup=yes
   TTYVTDisallocate=yes
   StandardInput=tty
   StandardOutput=journal
   StandardError=journal
   Environment=XDG_RUNTIME_DIR=/run/user/<YOUR_UID>        # id -u
   ExecStart=/usr/bin/labwc -s /home/<YOUR_USERNAME>/TrussC/examples/3d/3DPrimitivesExample/bin/3DPrimitivesExample
   Restart=always
   RestartSec=2

   [Install]
   WantedBy=graphical.target
   ```

   Replace the three `<...>` placeholders with your own values:

   | Placeholder | How to find it | Typical value |
   |---|---|---|
   | `<YOUR_USERNAME>` | `whoami` | e.g. `pi`, |
   | `<YOUR_UID>` | `id -u` | `1000` for the first user |
   | `<YOUR_USERNAME>` (in path) | same as above | — |

   Also adjust the `ExecStart` path if your app lives somewhere other than `~/TrussC/examples/3d/3DPrimitivesExample/bin/`.

2. Disable getty on tty1 (the kiosk will own it instead) and enable the service:
   ```bash
   sudo systemctl disable getty@tty1
   sudo systemctl enable trussc-kiosk
   sudo systemctl set-default graphical.target
   sudo reboot
   ```

3. Useful commands afterwards:
   ```bash
   systemctl status trussc-kiosk           # Is it running?
   journalctl -u trussc-kiosk -f           # Live logs
   sudo systemctl restart trussc-kiosk     # Restart the kiosk
   sudo systemctl stop trussc-kiosk        # Stop (drops back to a black tty)
   ```

**Key directives explained:**
- `Conflicts=getty@tty1` — getty is stopped before the kiosk starts, so they don't fight over tty1.
- `PAMName=login` — runs the service through the PAM `login` stack, which sets up logind seat access. Without this, labwc can't grab `/dev/dri/card1` and `/dev/input/*`.
- `TTYPath=/dev/tty1` — pins labwc to the same VT getty was on.
- `Restart=always` — labwc does not exit when the wrapped app terminates, but systemd will restart the whole unit anyway whenever the service stops (manual `systemctl stop`, crash, normal exit). For unattended kiosks this is exactly what you want: the screen never stays empty. If you'd prefer "restart only on crash, leave alone on clean exit," use `Restart=on-failure` instead.

---

## 5. Headless Mode (No Display Needed)

If your TrussC app doesn't need a window — e.g., a sensor logger, network service, or batch renderer — use `tcHeadlessApp` instead of `tcBaseApp`. Headless apps run on Lite without installing labwc, Xorg, or any display libraries.

See [trussc/include/tc/app/tcHeadlessApp.h](../trussc/include/tc/app/tcHeadlessApp.h).

---

## Troubleshooting

### labwc starts but the screen stays black with only a cursor

Normal — labwc on its own has nothing to display. Pass an app via `-s`, or set up an autostart file at `~/.config/labwc/autostart`.

### App doesn't appear when launched from another TTY / SSH

X11 apps under labwc need the right Wayland socket. From outside the labwc session:
```bash
ls /run/user/$(id -u)/ | grep wayland   # find the socket name, e.g. wayland-0
WAYLAND_DISPLAY=wayland-0 DISPLAY=:0 ./yourApp
```

### What about raw Xorg + openbox / picom?

It can be made to work on Pi 4, but on **Pi 5 the `vc4` driver lacks proper GLX support**, which breaks `picom --backend glx` and causes various rendering issues. If you must avoid Wayland entirely, consider Pi 4 hardware or installing the full `raspberrypi-ui-mods` desktop instead.

### Last-resort fallback: full desktop

If labwc gives you trouble, the full Raspberry Pi OS Desktop environment is always an option:

```bash
sudo apt install raspberrypi-ui-mods
sudo raspi-config   # Boot / Auto Login → Desktop Autologin
sudo reboot
```

This is heavier (~500 MB) but matches the standard Raspberry Pi OS Desktop image exactly.
