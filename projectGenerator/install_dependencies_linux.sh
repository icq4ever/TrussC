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
    exit 0
fi

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
        echo "Adding $USER to groups required by labwc/Wayland: ${MISSING_GROUPS[*]}"
        if sudo usermod -aG "$GROUPS_CSV" "$USER"; then
            echo "  Done. You must log out and back in for the new groups to take effect."
        else
            echo "  Failed to add groups. Run manually:"
            echo "    sudo usermod -aG $GROUPS_CSV $USER"
        fi
    fi
fi
