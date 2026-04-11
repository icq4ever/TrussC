#!/bin/bash
# =============================================================================
# TrussC Linux Dependency Installer
# =============================================================================
# Checks for required packages and installs missing ones.
# Supports Debian/Ubuntu (apt) and Arch Linux (pacman).
# Usage:
#   ./install_dependencies_linux.sh       # Interactive mode (asks before install)
#   ./install_dependencies_linux.sh -y    # Auto-install without asking
# =============================================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Detect distribution
DISTRO=""
if [ -r /etc/os-release ]; then
    . /etc/os-release
    DISTRO="$ID"
    DISTRO_LIKE="$ID_LIKE"
fi

# Required packages per distribution (keep in sync — these are the single source of truth)
REQUIRED_PACKAGES_DEBIAN=(
    libx11-dev
    libxcursor-dev
    libxi-dev
    libxrandr-dev
    libgl1-mesa-dev
    libgles2-mesa-dev
    libegl1-mesa-dev
    libasound2-dev
    libgtk-3-dev
    libavcodec-dev
    libavformat-dev
    libswscale-dev
    libavutil-dev
    libgstreamer1.0-dev
    libgstreamer-plugins-base1.0-dev
    gstreamer1.0-plugins-good
    gstreamer1.0-plugins-bad
    pkg-config
    libcurl4-openssl-dev
    cmake
)

REQUIRED_PACKAGES_ARCH=(
    libx11
    libxcursor
    libxi
    libxrandr
    mesa
    alsa-lib
    gtk3
    ffmpeg
    gstreamer
    gst-plugins-base
    gst-plugins-good
    gst-plugins-bad
    pkgconf
    curl
    cmake
)

AUTO_YES=false
if [ "$1" = "-y" ]; then
    AUTO_YES=true
fi

# Pick package manager based on distro
PKG_MANAGER=""
case "$DISTRO" in
    ubuntu|debian|linuxmint|pop|elementary|kali|raspbian)
        PKG_MANAGER="apt"
        ;;
    arch|manjaro|endeavouros|artix|cachyos)
        PKG_MANAGER="pacman"
        ;;
    *)
        # Fallback: check ID_LIKE
        case "$DISTRO_LIKE" in
            *debian*|*ubuntu*) PKG_MANAGER="apt" ;;
            *arch*)            PKG_MANAGER="pacman" ;;
        esac
        ;;
esac

if [ -z "$PKG_MANAGER" ]; then
    echo "ERROR: Unsupported Linux distribution: ${DISTRO:-unknown}"
    echo "Supported: Debian/Ubuntu (apt), Arch (pacman)"
    echo "Please install the required dependencies manually."
    exit 1
fi

# Build the missing-package list and prepare install commands
MISSING=()
case "$PKG_MANAGER" in
    apt)
        REQUIRED_PACKAGES=("${REQUIRED_PACKAGES_DEBIAN[@]}")

        # Raspberry Pi OS Lite: needs a Wayland session (labwc + Xwayland) since
        # the Lite image ships without any display server. Detect by hardware
        # (device-tree model) rather than distro ID, because 64-bit Raspberry Pi OS
        # reports ID=debian, not raspbian.
        IS_RASPBERRY_PI=false
        if [ -r /proc/device-tree/model ] && grep -qi "raspberry pi" /proc/device-tree/model; then
            IS_RASPBERRY_PI=true
        fi
        if [ "$IS_RASPBERRY_PI" = true ] && ! dpkg -s raspberrypi-ui-mods &>/dev/null; then
            echo "Detected Raspberry Pi (Lite — no desktop meta-package) — adding labwc + xwayland for runtime"
            REQUIRED_PACKAGES+=(labwc xwayland)
        fi

        for pkg in "${REQUIRED_PACKAGES[@]}"; do
            if ! dpkg -s "$pkg" &>/dev/null; then
                MISSING+=("$pkg")
            fi
        done
        MANUAL_INSTALL_CMD="sudo apt-get install"
        ;;
    pacman)
        REQUIRED_PACKAGES=("${REQUIRED_PACKAGES_ARCH[@]}")
        for pkg in "${REQUIRED_PACKAGES[@]}"; do
            if ! pacman -Qi "$pkg" &>/dev/null; then
                MISSING+=("$pkg")
            fi
        done
        MANUAL_INSTALL_CMD="sudo pacman -S"
        ;;
esac

if [ ${#MISSING[@]} -eq 0 ]; then
    echo "All required packages are already installed."
else
    echo "Detected distribution: $DISTRO ($PKG_MANAGER)"
    echo "The following packages are missing:"
    echo ""
    for pkg in "${MISSING[@]}"; do
        echo "  - $pkg"
    done
    echo ""

    if [ "$AUTO_YES" = true ]; then
        echo "Installing (auto-yes mode)..."
    else
        read -p "Install now? [Y/n] " answer
        case "$answer" in
            [nN]*)
                echo "Skipped. You can install manually with:"
                echo "  $MANUAL_INSTALL_CMD ${MISSING[*]}"
                exit 1
                ;;
        esac
    fi

    case "$PKG_MANAGER" in
        apt)
            sudo apt-get update
            sudo apt-get install -y "${MISSING[@]}"
            ;;
        pacman)
            sudo pacman -Sy --needed --noconfirm "${MISSING[@]}"
            ;;
    esac

    if [ $? -ne 0 ]; then
        echo "ERROR: Failed to install some packages."
        exit 1
    fi

    echo "All dependencies installed successfully."
fi

# On Raspberry Pi, ensure the user is in the groups labwc / Wayland needs
# (GPU + input + DRM seat). Skipped on non-Pi systems.
if [ "$IS_RASPBERRY_PI" = true ]; then
    REQUIRED_GROUPS=(video input render)
    MISSING_GROUPS=()
    CURRENT_GROUPS=" $(id -nG "$USER") "
    for grp in "${REQUIRED_GROUPS[@]}"; do
        if [[ "$CURRENT_GROUPS" != *" $grp "* ]]; then
            MISSING_GROUPS+=("$grp")
        fi
    done

    if [ ${#MISSING_GROUPS[@]} -gt 0 ]; then
        GROUPS_CSV=$(IFS=,; echo "${MISSING_GROUPS[*]}")
        echo ""
        echo "labwc/Wayland needs $USER to be in these groups (currently missing): ${MISSING_GROUPS[*]}"

        DO_GROUPS=true
        if [ "$AUTO_YES" != true ]; then
            read -p "Add $USER to ${GROUPS_CSV}? [Y/n] " group_answer
            case "$group_answer" in
                [nN]*) DO_GROUPS=false ;;
            esac
        fi

        if [ "$DO_GROUPS" = true ]; then
            if sudo usermod -aG "$GROUPS_CSV" "$USER"; then
                echo "  Done. You must log out and back in for the new groups to take effect."
            else
                echo "  Failed to add groups. Run manually:"
                echo "    sudo usermod -aG $GROUPS_CSV $USER"
            fi
        else
            echo "Skipped. Run manually when ready:"
            echo "  sudo usermod -aG $GROUPS_CSV $USER"
        fi
    fi

    # Offer to install the tc-run launcher into /usr/local/bin so users can
    # call `tc-run ./yourApp` from anywhere instead of typing the full
    # `labwc -s 'sh -c "..."'` incantation.
    if [ ! -e /usr/local/bin/tc-run ]; then
        echo ""
        echo "tc-run launcher: a one-line command that launches a TrussC app under labwc"
        echo "and exits the Wayland session when the app terminates."

        DO_TCRUN=true
        if [ "$AUTO_YES" != true ]; then
            read -p "Install tc-run to /usr/local/bin/tc-run? [Y/n] " tcrun_answer
            case "$tcrun_answer" in
                [nN]*) DO_TCRUN=false ;;
            esac
        fi

        if [ "$DO_TCRUN" = true ]; then
            if sudo tee /usr/local/bin/tc-run >/dev/null <<'TCRUN_EOF'
#!/bin/sh
# tc-run — launch a TrussC app inside a labwc Wayland session.
#
# Note: labwc is a full compositor and does NOT exit when the app terminates.
# After the app closes, right-click on the empty desktop and choose Exit.
# For unattended kiosk use, run via systemd with Restart=always instead.

if [ -z "$1" ] || [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    echo "usage: tc-run <app-binary>"
    exit 1
fi

APP="$(realpath "$1")"

if [ ! -x "$APP" ]; then
    echo "tc-run: not executable: $APP" >&2
    exit 1
fi

if ! command -v labwc >/dev/null 2>&1; then
    echo "tc-run: labwc not found. Install with: sudo apt install labwc xwayland" >&2
    exit 1
fi

exec labwc -s "$APP"
TCRUN_EOF
            then
                sudo chmod +x /usr/local/bin/tc-run
                echo "  Installed: /usr/local/bin/tc-run"
                echo "  Usage: tc-run <app-binary>"
            else
                echo "  Failed to install tc-run."
            fi
        else
            echo "Skipped tc-run installation."
        fi
    fi
fi
