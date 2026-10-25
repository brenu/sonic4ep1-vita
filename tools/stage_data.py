#!/usr/bin/env python3
"""Stage this exact game revision locally; no upload, DRM changes, or OBB rewriting."""
import hashlib
from pathlib import Path
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[1]
INPUTS = {
    'Sonic the Hedgehog 4 - Episode I.apk': 'd29290bee0e18705a52c316102865fe89f97a1fe1c2574ec7d8f7276590e87ee',
    'com.sega.sonic4epi/main.6200011.com.sega.sonic4epi.obb': '853eaa91679785b3a247e7e8077ecdb278815cc5a8ad5f72164b7b715c97f9c1',
}


def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for b in iter(lambda: f.read(1024 * 1024), b''):
            h.update(b)
    return h.hexdigest()


for name, expected in INPUTS.items():
    if sha(ROOT / name) != expected:
        raise SystemExit(f'Revision mismatch: {name}; reanalyze before staging.')
target = ROOT / 'staging/ux0/data/sonic4ep1'
target.mkdir(parents=True, exist_ok=True)
apk = ROOT / next(iter(INPUTS))
copies = [(apk, target / 'game.apk'),
          (ROOT / list(INPUTS)[1], target / 'main.6200011.com.sega.sonic4epi.obb')]
for source, dest in copies:
    if not dest.exists() or sha(dest) != sha(source):
        shutil.copyfile(source, dest)
with zipfile.ZipFile(apk) as z:
    for name in z.namelist():
        if name.startswith('lib/armeabi/') and name.endswith('.so'):
            # Only a basename from the explicitly selected native-library directory.
            (target / Path(name).name).write_bytes(z.read(name))
print(f'Private game data staged at: {target}')
print('Copy the contents of this directory to ux0:data/sonic4ep1/ on the Vita.')
print('The APK is renamed to game.apk, the OBB is copied unchanged, and lib/armeabi/*.so files are extracted beside them.')
