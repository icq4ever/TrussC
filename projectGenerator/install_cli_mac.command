#!/bin/bash
# =============================================================================
# TrussC projectGenerator CLI installer (macOS)
# =============================================================================
# Installs a wrapper script so you can run projectGenerator from anywhere.
# No sudo required - installs to ~/.local/bin/
# =============================================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TRUSSC_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BIN_PATH="$SCRIPT_DIR/tools/projectGenerator/bin/projectGenerator.app/Contents/MacOS/projectGenerator"

# Verify binary exists and is a real binary (not a shell script)
if [ ! -f "$BIN_PATH" ]; then
    echo "ERROR: projectGenerator binary not found."
    echo "Please build it first by running buildProjectGenerator_mac.command"
    echo ""
    echo "Expected path: $BIN_PATH"
    read -p "Press Enter to close..."
    exit 1
fi

if file "$BIN_PATH" | grep -q "shell script"; then
    echo "ERROR: Binary is corrupted (it's a shell script, not a Mach-O binary)."
    echo "Please clean and rebuild:"
    echo "  rm -rf tools/projectGenerator/build"
    echo "  rm tools/projectGenerator/bin/projectGenerator.app/Contents/MacOS/projectGenerator"
    echo "  ./buildProjectGenerator_mac.command"
    read -p "Press Enter to close..."
    exit 1
fi

INSTALL_DIR="$HOME/.local/bin"
INSTALL_PATH="$INSTALL_DIR/projectGenerator"

mkdir -p "$INSTALL_DIR"

echo "This will install a CLI wrapper at $INSTALL_PATH"
echo ""
echo "  TRUSSC_DIR = $TRUSSC_ROOT"
echo "  Binary     = $BIN_PATH"
echo ""
read -r -p "Proceed? [y/N]: " CONFIRM
case "$CONFIRM" in
    [yY]|[yY][eE][sS])
        # Write wrapper script to a temp file first, then move
        TMPFILE=$(mktemp)
        cat > "$TMPFILE" <<EOF
#!/bin/bash
export TRUSSC_DIR="$TRUSSC_ROOT"
exec "$BIN_PATH" "\$@"
EOF
        mv "$TMPFILE" "$INSTALL_PATH"
        chmod +x "$INSTALL_PATH"

        echo ""
        echo "Installed: $INSTALL_PATH"

        # Check if ~/.local/bin is in PATH
        if ! echo "$PATH" | tr ':' '\n' | grep -q "$INSTALL_DIR"; then
            echo ""
            echo "NOTE: $INSTALL_DIR is not in your PATH."
            echo "Add this to your ~/.zshrc:"
            echo ""
            echo "  export PATH=\"\$HOME/.local/bin:\$PATH\""
            echo ""
            echo "Then run: source ~/.zshrc"
        else
            echo "You can now run 'projectGenerator --help' from anywhere."
``        fi
        ;;
    *)
        echo "Cancelled."
        ;;
esac
