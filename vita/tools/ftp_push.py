#!/usr/bin/env python3
"""Push a directory tree to a PS Vita over vitacompanion's FTP.

One persistent connection for the whole tree: vitacompanion is slow to accept
new connections, so curl-per-file spends most of its time in handshakes.
Resumable — files already on the device with a matching size are skipped.

  python3 vita/tools/ftp_push.py <local_dir> <remote_dir> [host] [port]
"""
import ftplib
import os
import sys
import time


def main():
    local = sys.argv[1].rstrip("/")
    remote = sys.argv[2].rstrip("/")
    host = sys.argv[3] if len(sys.argv) > 3 else "192.168.0.108"
    port = int(sys.argv[4]) if len(sys.argv) > 4 else 1337

    # macOS sprinkles these through any folder it has browsed; they are dead
    # weight on the device and the game never reads them.
    junk = {".DS_Store", "._.DS_Store", "Thumbs.db"}

    files = []
    for root, _dirs, names in os.walk(local):
        for n in names:
            if n in junk or n.startswith("._"):
                continue
            p = os.path.join(root, n)
            files.append((p, os.path.relpath(p, local)))
    files.sort(key=lambda x: x[1])
    total = sum(os.path.getsize(p) for p, _ in files)
    print(f"{len(files)} files, {total / 1048576:.0f} MB -> {host}:{port}{remote}", flush=True)

    # vitacompanion's FTP server runs out of memory if a connection is held for
    # too long or a transfer dies mid-flight ("550 Could not allocate memory").
    # Reconnect on failure, and proactively every so often.
    state = {"ftp": None, "since_reconnect": 0}

    def connect():
        if state["ftp"] is not None:
            try:
                state["ftp"].close()
            except Exception:                              # noqa: BLE001
                pass
        time.sleep(1)
        ftp = ftplib.FTP()
        ftp.connect(host, port, timeout=60)
        ftp.login()
        ftp.set_pasv(True)
        state["ftp"] = ftp
        state["since_reconnect"] = 0
        made.clear()
        return ftp

    made = set()
    ftp = connect()

    def ensure_dir(path):
        if path in made or not path:
            return
        ensure_dir(os.path.dirname(path))
        try:
            state["ftp"].mkd(path)
        except ftplib.error_perm:
            pass  # already there
        made.add(path)

    def remote_size(path):
        try:
            return state["ftp"].size(path)
        except Exception:
            return None

    sent = skipped = 0
    sent_bytes = 0
    started = time.time()

    for i, (localpath, rel) in enumerate(files, 1):
        rpath = f"{remote}/{rel}"
        size = os.path.getsize(localpath)
        if remote_size(rpath) == size:
            skipped += 1
            continue
        ok = False
        for attempt in range(3):
            try:
                ensure_dir(os.path.dirname(rpath))
                with open(localpath, "rb") as fh:
                    ftp.storbinary(f"STOR {rpath}", fh, blocksize=32768)
                ok = True
                break
            except Exception as exc:                      # noqa: BLE001
                print(f"  retry {attempt + 1} {rel}: {exc}", flush=True)
                try:
                    ftp = connect()
                except Exception as exc2:                 # noqa: BLE001
                    print(f"  reconnect failed: {exc2}", flush=True)
                    time.sleep(5)
        if not ok:
            print(f"  FAIL {rel}", flush=True)
            continue
        state["since_reconnect"] += 1
        if state["since_reconnect"] >= 100:
            ftp = connect()
        sent += 1
        sent_bytes += size
        elapsed = time.time() - started
        rate = sent_bytes / elapsed / 1048576 if elapsed > 0 else 0
        if sent % 25 == 0 or size > 8 * 1048576:
            print(f"  [{i}/{len(files)}] {rate:.2f} MB/s  {rel}", flush=True)

    try:
        state["ftp"].quit()
    except Exception:                                  # noqa: BLE001
        pass
    print(f"done: {sent} uploaded, {skipped} already present, "
          f"{sent_bytes / 1048576:.0f} MB in {(time.time() - started) / 60:.1f} min", flush=True)


if __name__ == "__main__":
    main()
