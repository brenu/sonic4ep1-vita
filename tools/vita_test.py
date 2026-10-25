#!/usr/bin/env python3
"""Deploy this installed Sonic diagnostic and collect evidence via VitaCompanion."""
import argparse
import ftplib
import hashlib
import io
import json
from pathlib import Path
import socket
import struct
import sys
import time
import zipfile

ROOT = Path(__file__).resolve().parents[1]
TITLE = "S4EP10001"
MOUNT = "/ux0:"
APP = MOUNT + "/app/" + TITLE
DATA = MOUNT + "/data/sonic4ep1"


def command(host, port, text):
    with socket.create_connection((host, port), timeout=10) as sock:
        sock.settimeout(20 if text.startswith("promote ") else 5)
        sock.sendall((text + "\n").encode())
        chunks = []
        while True:
            try:
                part = sock.recv(8192)
            except socket.timeout:
                if chunks:
                    break
                raise
            if not part:
                break
            chunks.append(part)
            if b"\n" in part:
                break
        result = b"".join(chunks).decode(errors="replace").strip()
        if not result:
            raise RuntimeError("No reply to " + text)
        print(text + ": " + result)
        return result


def download(ftp, remote):
    output = io.BytesIO()
    last_report = time.monotonic()

    def receive(block):
        nonlocal last_report
        output.write(block)
        now = time.monotonic()
        if now - last_report >= 15:
            print("Reading:", remote, output.tell(), "bytes", flush=True)
            last_report = now

    ftp.retrbinary("RETR " + remote, receive, blocksize=65536)
    return output.getvalue()


def sfo_string(data, requested):
    magic, version, keys, values, count = struct.unpack_from("<5I", data)
    if magic != 0x46535000 or count > 256:
        raise ValueError("Invalid SFO header")
    for index in range(count):
        key, fmt, length, maximum, value = struct.unpack_from("<HHIII", data, 20 + index * 16)
        name = data[keys + key:].split(b"\0", 1)[0].decode()
        if name == requested:
            return data[values + value:values + value + length].rstrip(b"\0").decode()
    raise ValueError("Missing SFO field: " + requested)


def collect(ftp, folder):
    try:
        (folder / "port.log").write_bytes(download(ftp, DATA + "/port.log"))
    except ftplib.error_perm as exc:
        print("Log unavailable:", exc)
    # GPU faults can implicate a system module too. Keep recent complete
    # cores, then inspect their identity rather than inferring it from names.
    names = ftp.nlst(MOUNT + "/data/")
    (folder / "core-inventory.json").write_text(json.dumps(
        [name for name in names if name.rsplit("/", 1)[-1].startswith("psp2core-")],
        indent=2) + "\n")
    for name in names:
        basename = name.rsplit("/", 1)[-1]
        if basename.startswith("psp2core-") and basename.endswith(".psp2dmp"):
            try:
                created = int(basename.split("-", 2)[1])
            except (ValueError, IndexError):
                continue
            # Don't repeatedly download years of unrelated crash history.
            if created < time.time() - 7 * 86400:
                continue
            (folder / basename).write_bytes(download(ftp, MOUNT + "/data/" + basename))
    print("Evidence:", folder)


def deploy(ftp, args, folder):
    package = args.vpk.resolve()
    raw = package.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    with zipfile.ZipFile(io.BytesIO(raw)) as archive:
        if archive.testzip() is not None:
            raise RuntimeError("Corrupt VPK")
        names = archive.namelist()
        if any(name.endswith((".apk", ".obb", ".so")) for name in names):
            raise RuntimeError("Package unexpectedly contains game data")
        if any(name.startswith("/") or ".." in Path(name).parts for name in names):
            raise RuntimeError("Unsafe package path")
        # Fixed code-only deployment scope; refuse silently missing resources.
        required = {"eboot.bin", "sce_sys/param.sfo"}
        if not required.issubset(names):
            raise RuntimeError("Missing executable or metadata")
        sfo = archive.read("sce_sys/param.sfo")
        if sfo_string(sfo, "TITLE_ID") != TITLE:
            raise RuntimeError("Package is not the Sonic diagnostic")
        changed = []
        backup = folder / "installed-backup"
        for name in names:
            if name.endswith("/"):
                continue
            new = archive.read(name)
            old = download(ftp, APP + "/" + name)
            if old != new:
                target = backup / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(old)
                changed.append((name, new))
        (folder / "deployment.json").write_text(json.dumps({"vpk": str(package), "sha256": digest, "version": sfo_string(sfo, "APP_VER"), "ftp_mount": MOUNT, "changed": [name for name, _ in changed]}, indent=2) + "\n")
        # A retry must also stop an unchanged installed build: sending launch
        # alone can leave the same stalled process resident.
        if not args.app_closed:
            reply = command(args.host, args.command_port, "quit " + TITLE)
            if reply != "Quit.":
                raise RuntimeError("Cannot confirm Sonic stopped; close it manually, then use --app-closed")
            time.sleep(1)
        if changed:
            for name, data in changed:
                remote = APP + "/" + name
                temporary = remote + ".upload"
                ftp.storbinary("STOR " + temporary, io.BytesIO(data))
                if download(ftp, temporary) != data:
                    raise RuntimeError("Transfer verification failed: " + name)
                # This FTP server refuses rename over an existing file.
                # Preserve the old remote file and restore it if commit fails.
                previous = remote + ".previous-" + folder.name
                ftp.rename(remote, previous)
                try:
                    ftp.rename(temporary, remote)
                except Exception:
                    ftp.rename(previous, remote)
                    raise
                if download(ftp, remote) != data:
                    raise RuntimeError("Installed verification failed: " + name)
                print("Updated and verified:", name, len(data), "bytes")
        else:
            print("Installed files already match package")
    reply = command(args.host, args.command_port, "launch " + TITLE)
    if reply != "Launched.":
        raise RuntimeError("Launch was not confirmed")


def main():
    global MOUNT, APP, DATA
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["ping", "reboot", "status", "collect", "deploy", "mount-check"])
    parser.add_argument("host")
    parser.add_argument("--ftp-port", type=int, default=1337)
    parser.add_argument("--command-port", type=int, default=1338)
    parser.add_argument("--ftp-mount", choices=["ux0", "uma0"], help="Select only after checking the mount in VitaShell")
    parser.add_argument("--app-closed", action="store_true", help="Use only after confirming Sonic is closed on the Vita")
    parser.add_argument("--vpk", type=Path, help="Defaults to the current diagnostic manifest artifact")
    args = parser.parse_args()
    if args.action == "deploy" and args.vpk is None:
        args.vpk = Path(json.loads((ROOT / "work/build/diagnostic_manifest.json").read_text())["file"])
    if args.action == "reboot":
        command(args.host, args.command_port, "reboot")
        return
    if args.action in ("ping", "status"):
        command(args.host, args.command_port, "version")
        if args.action == "ping":
            return
    with ftplib.FTP() as ftp:
        # Wi-Fi/storage can pause during app teardown or a file transfer.
        # Commands run in a resumable terminal so this does not block updates.
        ftp.connect(args.host, args.ftp_port, timeout=60)
        ftp.login()
        found = []
        for mount in ["/ux0:", "/uma0:"]:
            try:
                sfo = download(ftp, mount + "/app/" + TITLE + "/sce_sys/param.sfo")
            except ftplib.error_perm:
                continue
            if sfo_string(sfo, "TITLE_ID") == TITLE:
                found.append(mount)
        if not found:
            raise RuntimeError("No Sonic installation found")
        if args.ftp_mount:
            MOUNT = "/" + args.ftp_mount + ":"
            if MOUNT not in found:
                raise RuntimeError("No Sonic app on selected FTP mount")
        elif len(found) == 1:
            MOUNT = found[0]
        else:
            raise RuntimeError("Multiple Sonic copies: explicitly select the verified --ftp-mount")
        APP = MOUNT + "/app/" + TITLE
        DATA = MOUNT + "/data/sonic4ep1"
        print("FTP storage view:", MOUNT)
        if args.action == "mount-check":
            marker = "/uma0:/data/sonic4ep1/VITACOMPANION_MOUNT_CHECK.txt"
            payload = b"Written through VitaCompanion FTP /uma0:/data/sonic4ep1/. Compare this file's location in VitaShell.\n"
            ftp.storbinary("STOR " + marker, io.BytesIO(payload))
            if download(ftp, marker) != payload:
                raise RuntimeError("Marker verification failed")
            print("Mount-check marker:", marker)
            return
        if args.action == "status":
            print("Installed app:", ftp.nlst(APP + "/"))
            print("Game directory entries:", len(ftp.nlst(DATA + "/")))
            print("Dump directory:", ftp.nlst(MOUNT + "/data/"))
            ftp.voidcmd("TYPE I")
            for mount in ["/ux0:", "/uma0:"]:
                for suffix in ["", ".upload"]:
                    remote = mount + "/data/sonic4ep1/main.6200011.com.sega.sonic4epi.obb" + suffix
                    try:
                        print("OBB path:", remote, "size:", ftp.size(remote))
                    except ftplib.error_perm as exc:
                        print("OBB path:", remote, str(exc))
            return
        folder = ROOT / "work/hardware" / time.strftime("companion-%Y%m%d-%H%M%S")
        folder.mkdir(parents=True, exist_ok=False)
        collect(ftp, folder)
        if args.action == "deploy":
            deploy(ftp, args, folder)


if __name__ == "__main__":
    try:
        main()
    except (OSError, EOFError, ValueError, RuntimeError, ftplib.Error) as exc:
        print("Vita test stopped:", exc, file=sys.stderr)
        sys.exit(1)
