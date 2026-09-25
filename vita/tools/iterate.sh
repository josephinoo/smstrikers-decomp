#!/usr/bin/env bash
# One-shot boot iteration: build -> install -> run -> catch the crash -> report.
#
# Usage:  bash vita/tools/iterate.sh [seconds]
#
# Polls the Vita3K log instead of sleeping a fixed time, so it stops the moment
# the guest faults rather than always waiting the full timeout. Always kills
# Vita3K on exit, including on Ctrl-C.

set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
export VITASDK="${VITASDK:-/usr/local/vitasdk}"
export PATH="$VITASDK/bin:$PATH"

BUILD="$REPO/build/vita"
FS="$HOME/Library/Application Support/Vita3K/Vita3K/fs"
APP="$FS/ux0/app/VSTR00003"
DATA="$FS/ux0/data/smstrikers"
RUNLOG="/tmp/sms_run.log"
VITA3K="/Applications/Vita3K.app/Contents/MacOS/Vita3K"
MAX_WAIT="${1:-45}"

cleanup() { pkill -9 -f Vita3K >/dev/null 2>&1; }
trap cleanup EXIT INT TERM

echo "==> build"
if ! make -C "$BUILD" smstrikers-game.vpk-vpk -j"$(sysctl -n hw.ncpu)" > /tmp/sms_build.log 2>&1; then
    echo "BUILD FAILED:"
    grep -E ' error:|undefined reference|multiple definition' /tmp/sms_build.log | head -15
    exit 1
fi

echo "==> install"
rm -f "$DATA/_vita_fs.log" "$RUNLOG"
rm -rf "$APP"; mkdir -p "$APP"
unzip -o "$BUILD/smstrikers-game.vpk" -d "$APP" > /dev/null

echo "==> run (up to ${MAX_WAIT}s, stops as soon as it faults)"
cleanup; sleep 0.5
"$VITA3K" -w -r VSTR00003 > "$RUNLOG" 2>&1 &

for _ in $(seq 1 $((MAX_WAIT * 2))); do
    sleep 0.5
    # Guest fault only — host Unhandled EXC_BAD_ACCESS is Vita3K pad/Vulkan noise.
    grep -qE 'PC: 0x[0-9a-f]+' "$RUNLOG" 2>/dev/null && break
    pgrep -f Vita3K > /dev/null || break
done

# Give it a moment to finish writing the register dump, then grab a screenshot.
sleep 1
screencapture -x /tmp/sms_shot.png 2>/dev/null
cleanup

echo
echo "==> files the game opened"
LC_ALL=C grep -a '^OPEN\|^MISS' "$DATA/_vita_fs.log" 2>/dev/null | tail -5

echo
PC=$(grep -oE 'PC: 0x[0-9a-f]+' "$RUNLOG" 2>/dev/null | head -1 | cut -d' ' -f2)
if [ -n "$PC" ]; then
    echo "==> CRASH at $PC"
    arm-vita-eabi-addr2line -f -C -i -e "$BUILD/sms_entry" "$PC" 2>/dev/null
    echo
    echo "--- registers ---"
    grep -A 10 "PC: $PC" "$RUNLOG" | head -11
    OOM=$(LC_ALL=C grep -a '^OOM' "$DATA/_vita_fs.log" 2>/dev/null | head -1)
    [ -n "$OOM" ] && { echo; echo "--- allocation failure ---"; echo "$OOM"; }
else
    echo "==> no guest fault recorded. Either it is still alive (good) or it"
    echo "    exited cleanly. Screenshot: /tmp/sms_shot.png"
fi

echo
echo "full logs: $RUNLOG  /tmp/sms_build.log  $DATA/_vita_fs.log"
