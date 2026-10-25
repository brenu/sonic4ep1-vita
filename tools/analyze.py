#!/usr/bin/env python3
"""Reproducible private APK analysis, using Python's stdlib and readelf only."""
import hashlib
import json
import lzma
from pathlib import Path
import re
import struct
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'work' / 'analysis'
OUT.mkdir(parents=True, exist_ok=True)


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def elf_info(path):
    b = path.read_bytes()
    if b[:6] != b'\x7fELF\x01\x01':
        raise ValueError(f'Expected little-endian ELF32: {path}')
    shoff = struct.unpack_from('<I', b, 32)[0]
    shsize, shnum = struct.unpack_from('<HH', b, 46)
    sections = [struct.unpack_from('<10I', b, shoff + i * shsize) for i in range(shnum)]

    def cstring(addr):
        for s in sections:
            if s[1] != 8 and s[3] <= addr < s[3] + s[5]:
                offset = s[4] + addr - s[3]
                end = b.find(b'\0', offset, min(offset + 512, s[4] + s[5]))
                if end >= 0:
                    try:
                        return b[offset:end].decode('ascii')
                    except UnicodeDecodeError:
                        pass
        return None

    candidates = []
    for s in sections:
        if not s[2] & 2 or s[1] == 8:
            continue
        for offset in range(s[4], s[4] + s[5] - 11, 4):
            nameptr, sigptr, fn = struct.unpack_from('<III', b, offset)
            name, sig = cstring(nameptr), cstring(sigptr)
            if (name and re.fullmatch(r'[A-Za-z_$][\w$]*', name)
                    and sig and re.fullmatch(r'\([^\s]*\)[^\s]+', sig)
                    and any(t[2] & 4 and t[3] <= (fn & ~1) < t[3] + t[5] for t in sections)):
                candidates.append(dict(table_address=hex(s[3] + offset - s[4]),
                                       name=name, signature=sig, function=hex(fn)))
    reports = {}
    for label, flags in [('headers', ['-h', '-A', '-d', '-l']), ('symbols', ['-Ws'])]:
        r = subprocess.run(['readelf', *flags, str(path)], text=True, capture_output=True, check=True)
        (OUT / (path.name + '.' + label + '.txt')).write_text(r.stdout + r.stderr)
        reports[label] = r.stdout
    imports, exports = [], []
    for line in reports['symbols'].splitlines():
        parts = line.split()
        if len(parts) >= 8 and parts[0].rstrip(':').isdigit():
            (imports if parts[6] == 'UND' else exports).append(parts[7])
    return dict(sha256=digest(path), imports=sorted(set(imports)),
                exports=sorted(set(exports)), native_method_candidates=candidates)


def main():
    apks = list(ROOT.glob('*.apk'))
    if len(apks) != 1:
        raise SystemExit('Expected exactly one APK in sonic/')
    apk = apks[0]
    inventory = {'apk': dict(name=apk.name, sha256=digest(apk)), 'libraries': {}}
    with zipfile.ZipFile(apk) as z:
        inventory['apk']['entries'] = [dict(name=i.filename, size=i.file_size) for i in z.infolist()]
        for name in z.namelist():
            if name.startswith('lib/armeabi/') and name.endswith('.so'):
                target = OUT / Path(name).name
                target.write_bytes(z.read(name))
                inventory['libraries'][target.name] = elf_info(target)
        data = lzma.decompress(z.read('assets/Sonic4epI.s3e'), memlimit=128 * 1024 * 1024)
        (OUT / 'Sonic4epI.unpacked').write_bytes(data)
        inventory['payload'] = dict(size=len(data), magic=data[:4].decode('ascii'),
                                    sha256=hashlib.sha256(data).hexdigest())
    inventory['obb'] = []
    for path in ROOT.rglob('*.obb'):
        if 'staging' in path.parts or 'work' in path.parts:
            continue
        with path.open('rb') as f:
            magic = f.read(8).hex()
        inventory['obb'].append(dict(name=str(path.relative_to(ROOT)), size=path.stat().st_size,
                                     sha256=digest(path), first_bytes=magic, zip=zipfile.is_zipfile(path)))
    dynlib = ROOT / 'source/dynlib.c'
    if dynlib.exists():
        mapped = set(re.findall(r'\{\s*"([^"]+)"\s*,', dynlib.read_text()))
        core_exports = set(inventory['libraries']['libs3e_android.so']['exports'])
        for lib in inventory['libraries'].values():
            lib['imports_without_literal_boilerplate_mapping'] = sorted(set(lib['imports']) - mapped)
            lib['imports_without_core_or_boilerplate_mapping'] = sorted(set(lib['imports']) - mapped - core_exports)
    (OUT / 'inventory.json').write_text(json.dumps(inventory, indent=2) + '\n')
    for name, lib in inventory['libraries'].items():
        print(f'{name}: {len(lib["imports"])} imports, {len(lib["native_method_candidates"])} JNI table candidates')
    print(f'Payload: {inventory["payload"]}')
    print(f'Report: {OUT / "inventory.json"}')


if __name__ == '__main__':
    main()
