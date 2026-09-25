#!/usr/bin/env bash
# Fast dev loop against a REAL PS Vita, via vitacompanion.
#
#   export VITA_IP=192.168.1.42
#   bash vita/tools/deploy_vita.sh
#
# Uploads only eboot.bin over FTP and relaunches the title. No VPK repack, no
# reinstall, no touching the console. Roughly a 5-second cycle.
#
# On the Vita, once:
#   - HENkaku/h-encore, with "Enable unsafe homebrew" ON (we build UNSAFE selfs)
#   - vitacompanion: it is a taiHEN PLUGIN, not an app. Copy both modules to
#     ur0:/tai/ and add to ur0:/tai/config.txt:
#         *KERNEL
#         ur0:tai/vitacompanion_kernel.skprx
#         *main
#         ur0:tai/vitacompanion.suprx
#     Then reboot. It runs in the background from then on (FTP 1337, cmd 1338).
#   - install our VPK once, so ux0:app/<TITLEID>/ exists
#   - copy the game dump to   ux0:data/smstrikers/
#   - copy libshacccg.suprx to ur0:data/

set -uo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
export VITASDK="${VITASDK:-/usr/local/vitasdk}"
export PATH="$VITASDK/bin:$PATH"

BUILD="$REPO/build/vita"
TITLEID="${TITLEID:-VSTR00003}"
FTP_PORT="${FTP_PORT:-1337}"
CMD_PORT="${CMD_PORT:-1338}"

if [ -z "${VITA_IP:-}" ]; then
    echo "Set VITA_IP first, e.g.  export VITA_IP=192.168.1.42"
    echo "(vitacompanion shows the address on the Vita's screen)"
    exit 1
fi

send_cmd() { printf '%s\n' "$1" | nc -w 2 "$VITA_IP" "$CMD_PORT" >/dev/null 2>&1; }

echo "==> build"
if ! make -C "$BUILD" sms_entry.bin-self -j"$(sysctl -n hw.ncpu)" > /tmp/sms_build.log 2>&1; then
    echo "BUILD FAILED:"
    grep -E ' error:|undefined reference|multiple definition' /tmp/sms_build.log | head -15
    exit 1
fi

# Best effort: "destroy" is not in vitacompanion's documented command set, so
# ignore it failing. Launching again over a running title works anyway.
echo "==> stop whatever is running (best effort)"
send_cmd "destroy"
sleep 1

echo "==> upload eboot.bin -> $VITA_IP:$FTP_PORT"
if ! curl -s --fail -T "$BUILD/sms_entry.bin" \
        "ftp://$VITA_IP:$FTP_PORT/ux0:/app/$TITLEID/eboot.bin"; then
    echo "upload failed — is vitacompanion running, and is VITA_IP right?"
    exit 1
fi

echo "==> launch $TITLEID"
send_cmd "launch $TITLEID"

cat <<EOF

Running on the Vita.

If it crashes, the console writes a coredump to ux0:data/. Pull it and resolve
it with the unstripped ELF (we already build with -Wl,-q, which is what makes
this work):

  curl -s "ftp://$VITA_IP:$FTP_PORT/ux0:/data/" | grep -i psp2core
  curl -s -O "ftp://$VITA_IP:$FTP_PORT/ux0:/data/<psp2core-file>"
  vita-parse-core <psp2core-file> $BUILD/sms_entry

For live stdout instead of after-the-fact dumps, install psp2shell on the Vita
and connect with psp2shell_cli.
EOF
