#!/usr/bin/env bash
# Refresh the bundled Mozilla CA root list from curl.se.
# Run this whenever you want to pick up new/removed CAs (roughly every few months).
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DATA_DIR="${HERE}/../data"
OUT="${DATA_DIR}/cacert.pem"

mkdir -p "${DATA_DIR}"
echo "Fetching latest cacert.pem from curl.se..."
curl -fsSL -o "${OUT}.new" "https://curl.se/ca/cacert.pem"

# Basic sanity check: must contain a recognizable BEGIN CERTIFICATE line
if ! grep -q "BEGIN CERTIFICATE" "${OUT}.new"; then
    echo "ERROR: downloaded file does not look like a PEM bundle" >&2
    rm -f "${OUT}.new"
    exit 1
fi

mv "${OUT}.new" "${OUT}"

DATE_LINE="$(grep -m1 "Certificate data from Mozilla" "${OUT}" || true)"
echo "Updated: ${OUT}"
echo "  ${DATE_LINE}"
