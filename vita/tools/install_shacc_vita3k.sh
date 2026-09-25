#!/usr/bin/env bash
# Install ShaRKBR33D + PSM pkgs into Vita3K (from Cimmerian / ShaRKBR33D guide).
# libshacccg.suprx cannot be extracted on macOS — open ShaRKBR33D inside Vita3K after this.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CACHE="${ROOT}/third_party"
VITA3K_FS="${VITA3K_FS:-$HOME/Library/Application Support/Vita3K/Vita3K/fs}"

mkdir -p "$CACHE/psm" "$VITA3K_FS/ux0/app" "$VITA3K_FS/ux0/package" \
  "$VITA3K_FS/ur0/data/external"

download() {
  local url="$1" out="$2"
  if [[ -f "$out" && -s "$out" ]]; then
    echo "have $(basename "$out")"
    return
  fi
  echo "get $url"
  curl -L --fail --progress-bar -o "$out" "$url"
}

download \
  "https://github.com/Rinnegatamante/ShaRKBR33D/releases/download/v.1.2/ShaRKBR33D.vpk" \
  "$CACHE/ShaRKBR33D.vpk"
download \
  "https://www.rinnegatamante.eu/files/vitadb/ShaRKF00D.vpk" \
  "$CACHE/ShaRKF00D.vpk"
download \
  "https://archive.org/download/psm-runtime/IP9100-PCSI00011_00-PSMRUNTIME000000.pkg" \
  "$CACHE/psm/PSM_1.00.pkg"
download \
  "https://archive.org/download/psm-runtime/IP9100-PCSI00011_00-PSMRUNTIME000000-A0201-V0100-e4708b1c1c71116c29632c23df590f68edbfc341-PE.pkg" \
  "$CACHE/psm/PSM_2.01.pkg"

rm -rf "$VITA3K_FS/ux0/app/SHRKBR33D"
mkdir -p "$VITA3K_FS/ux0/app/SHRKBR33D"
unzip -o "$CACHE/ShaRKBR33D.vpk" -d "$VITA3K_FS/ux0/app/SHRKBR33D" >/dev/null
cp -f "$CACHE/psm/"*.pkg "$VITA3K_FS/ux0/package/"
cp -f "$CACHE/ShaRKBR33D.vpk" "$CACHE/ShaRKF00D.vpk" "$VITA3K_FS/ux0/"

echo
echo "Installed ShaRKBR33D → $VITA3K_FS/ux0/app/SHRKBR33D"
echo "PSM pkgs → $VITA3K_FS/ux0/package/"
echo
echo "Next (you must do this in Vita3K UI):"
echo "  1. Open app ShaRKBR33D and let it finish (needs network)."
echo "  2. Confirm files exist:"
echo "       $VITA3K_FS/ur0/data/libshacccg.suprx"
echo "       $VITA3K_FS/ur0/data/external/libshacccg.suprx"
echo "  3. Reopen SM Strikers Vita (VSTR00001)."
echo
echo "Guide: https://cimmerian.gitbook.io/vita-troubleshooting-guide/shader-compiler/extract-libshacccg.suprx"
