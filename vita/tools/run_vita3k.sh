#!/usr/bin/env bash
# Build SM Strikers Vita port, install to Vita3K, launch emulator, and verify logs.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VITA_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# 1. Environment & paths
export VITASDK="${VITASDK:-/usr/local/vitasdk}"
export PATH="$VITASDK/bin:$PATH"

if [[ ! -d "$VITASDK" ]]; then
  echo "[ERROR] VITASDK directory not found at: $VITASDK" >&2
  echo "Please set VITASDK to your VitaSDK installation path." >&2
  exit 1
fi

BUILD_DIR="${BUILD_DIR:-$VITA_DIR/build}"
TITLE_ID="VSTR00002"
VPK_PATH="$BUILD_DIR/smstrikers-vita.vpk"
VITA3K_HOME="${VITA3K_HOME:-$HOME/Library/Application Support/Vita3K/Vita3K}"
APP_DIR="${VITA3K_APP_DIR:-$VITA3K_HOME/fs/ux0/app/$TITLE_ID}"
MAIN_LOG="$VITA3K_HOME/vita3k.log"
TITLE_LOG="$VITA3K_HOME/logs/$TITLE_ID - [SM Strikers Vita].log"

# Parse optional arguments
SKIP_LAUNCH=0
TIMEOUT_SECS="${VITA3K_TIMEOUT:-0}"
VITA3K_PID=""

cleanup_vita3k() {
  if [[ -n "$VITA3K_PID" ]] && kill -0 "$VITA3K_PID" 2>/dev/null; then
    echo "==> Closing the Vita3K instance started by this script (PID $VITA3K_PID)..."
    kill -TERM "$VITA3K_PID" 2>/dev/null || true
    wait "$VITA3K_PID" 2>/dev/null || true
  fi
  if [[ -n "${VITA3K_BIN:-}" ]]; then
    pkill -f "^${VITA3K_BIN}( |$)" 2>/dev/null || true
  fi
}
trap cleanup_vita3k EXIT INT TERM

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-launch|--skip-launch|--install-only)
      SKIP_LAUNCH=1
      shift
      ;;
    --timeout)
      TIMEOUT_SECS="$2"
      shift 2
      ;;
    -h|--help)
      echo "Usage: $0 [options]"
      echo "Options:"
      echo "  --timeout <seconds>   Run Vita3K for specified seconds, then stop and verify"
      echo "  --skip-launch         Build and install VPK without launching Vita3K"
      echo "  --install-only        Alias for --skip-launch"
      echo "  -h, --help            Show this help message"
      exit 0
      ;;
    *)
      echo "[WARNING] Unknown option: $1" >&2
      shift
      ;;
  esac
done

# 2. CMake build
echo "==> Building Vita port target in $BUILD_DIR..."
mkdir -p "$BUILD_DIR"
if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
  cmake -S "$VITA_DIR" -B "$BUILD_DIR" -DCMAKE_TOOLCHAIN_FILE="$VITASDK/share/vita.toolchain.cmake"
fi
cmake --build "$BUILD_DIR"

if [[ ! -f "$VPK_PATH" ]]; then
  echo "[ERROR] VPK not found at: $VPK_PATH" >&2
  exit 1
fi
echo "==> Build succeeded: $VPK_PATH"

# 3. Install VPK into Vita3K ux0:app/VSTR00002
echo "==> Installing VPK into $APP_DIR..."
mkdir -p "$APP_DIR"
unzip -o "$VPK_PATH" -d "$APP_DIR" >/dev/null
echo "==> Installed $TITLE_ID to Vita3K filesystem."

# Helper function to grep Vita3K logs
grep_logs() {
  local target="$1"
  if [[ -f "$target" ]]; then
    echo "--- Log: $target ---"
    echo "['Game started' matches]"
    grep -i "Game started" "$target" || echo "  (not found)"
    echo "['title:' matches]"
    grep -i "title:" "$target" || echo "  (not found)"
    local mem_count
    mem_count=$(grep -ci "MemoryWrite" "$target" || true)
    if [[ -z "$mem_count" ]]; then
      mem_count=0
    fi
    echo "MemoryWrite count: $mem_count"
    echo
  fi
}

# 5. Launch Vita3K with -w -r VSTR00001
if [[ $SKIP_LAUNCH -eq 1 ]]; then
  echo "==> Skipping Vita3K launch as requested."
else
  # Locate Vita3K binary
  VITA3K_BIN="${VITA3K_BIN:-}"
  if [[ -z "$VITA3K_BIN" ]]; then
    if command -v Vita3K >/dev/null 2>&1; then
      VITA3K_BIN="$(command -v Vita3K)"
    elif command -v vita3k >/dev/null 2>&1; then
      VITA3K_BIN="$(command -v vita3k)"
    elif [[ -x "/Applications/Vita3K.app/Contents/MacOS/Vita3K" ]]; then
      VITA3K_BIN="/Applications/Vita3K.app/Contents/MacOS/Vita3K"
    elif [[ -x "$HOME/Applications/Vita3K.app/Contents/MacOS/Vita3K" ]]; then
      VITA3K_BIN="$HOME/Applications/Vita3K.app/Contents/MacOS/Vita3K"
    fi
  fi

  if [[ -z "$VITA3K_BIN" || ! -x "$VITA3K_BIN" ]]; then
    echo "[ERROR] Vita3K executable not found." >&2
    echo "Searched standard paths:" >&2
    echo "  - PATH (Vita3K, vita3k)" >&2
    echo "  - /Applications/Vita3K.app/Contents/MacOS/Vita3K" >&2
    echo "  - $HOME/Applications/Vita3K.app/Contents/MacOS/Vita3K" >&2
    echo "Please set VITA3K_BIN environment variable or install Vita3K." >&2
    exit 1
  fi

  if pgrep -f "^${VITA3K_BIN}( |$)" >/dev/null 2>&1; then
    echo "==> Closing existing Vita3K instance before launch..."
    pkill -f "^${VITA3K_BIN}( |$)" || true
    sleep 1
  fi

  echo "==> Launching one instance: $VITA3K_BIN -w -r $TITLE_ID"
  if [[ "$TIMEOUT_SECS" -gt 0 ]]; then
    echo "==> Running with timeout of ${TIMEOUT_SECS}s..."
    set +e
    "$VITA3K_BIN" -w -r "$TITLE_ID" &
    VITA3K_PID=$!
    sleep "$TIMEOUT_SECS"
    if kill -0 "$VITA3K_PID" 2>/dev/null; then
      echo "==> Timeout reached; stopping Vita3K (PID $VITA3K_PID)..."
      kill -TERM "$VITA3K_PID" 2>/dev/null || true
      wait "$VITA3K_PID" 2>/dev/null || true
      VITA3K_PID=""
    fi
    set -e
  else
    "$VITA3K_BIN" -w -r "$TITLE_ID" &
    VITA3K_PID=$!
    wait "$VITA3K_PID" || true
    VITA3K_PID=""
  fi
fi

# 6. Grep logs for 'Game started', 'title:', and MemoryWrite count
echo "==> Verifying Vita3K logs for $TITLE_ID..."
grep_logs "$MAIN_LOG"
grep_logs "$TITLE_LOG"

echo "==> Done."
