#!/bin/bash
# =============================================================================
# TrussC Linux Dependency Installer
# =============================================================================
# Checks for required packages and installs missing ones.
# Usage:
#   ./install_dependencies_linux.sh       # Interactive mode (asks before install)
#   ./install_dependencies_linux.sh -y    # Auto-install without asking
# =============================================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Required packages (keep in sync — this is the single source of truth)
REQUIRED_PACKAGES=(
    libx11-dev
    libxcursor-dev
    libxi-dev
    libxrandr-dev
    libgl1-mesa-dev
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

AUTO_YES=false
if [ "$1" = "-y" ]; then
    AUTO_YES=true
fi

# Check which packages are missing
MISSING=()
for pkg in "${REQUIRED_PACKAGES[@]}"; do
    if ! dpkg -s "$pkg" &>/dev/null; then
        MISSING+=("$pkg")
    fi
done

if [ ${#MISSING[@]} -eq 0 ]; then
    echo "All required packages are already installed."
    exit 0
fi

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
            echo "  sudo apt-get install ${MISSING[*]}"
            exit 1
            ;;
    esac
fi

sudo apt-get update
sudo apt-get install -y "${MISSING[@]}"

if [ $? -ne 0 ]; then
    echo "ERROR: Failed to install some packages."
    exit 1
fi

echo "All dependencies installed successfully."
