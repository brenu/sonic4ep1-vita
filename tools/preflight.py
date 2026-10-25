#!/usr/bin/env python3
"""Check actual loader dependencies and link a softfp probe without installing tools."""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'work' / 'preflight'
OUT.mkdir(parents=True, exist_ok=True)
sdk = Path(os.environ.get('VITASDK', '/usr/local/vitasdk'))
env = dict(os.environ, VITASDK=str(sdk), PATH=f'{sdk}/bin:' + os.environ.get('PATH', ''))
missing = []
for name in ('arm-vita-eabi-gcc', 'arm-vita-eabi-g++', 'vita-libs-gen', 'vita-elf-create',
             'vita-make-fself', 'vita-mksfoex', 'vita-pack-vpk', 'cmake', 'make'):
    if not shutil.which(name, path=env['PATH']):
        missing.append('program: ' + name)
cmake = (ROOT / 'CMakeLists.txt').read_text()
linkblock = cmake.split('target_link_libraries(${CMAKE_PROJECT_NAME}', 2)[-1].split(')', 1)[0]
libraries = re.findall(r'^\s+([A-Za-z][A-Za-z0-9_+]+)\s*$', linkblock, re.M)
# vitaGL and SceLibcBridge are built from the pinned source checkout.
for name in sorted(set(libraries) - {'vitaGL'}):
    if not (sdk / f'arm-vita-eabi/lib/lib{name}.a').is_file():
        missing.append('library: lib' + name + '.a')
for name in ('vitashark.h', 'kubridge.h', 'SLES/OpenSLES.h', 'SLES/OpenSLES_Android.h'):
    if not (sdk / 'arm-vita-eabi/include' / name).is_file():
        missing.append('header: ' + name)
probe = OUT / 'softfp.c'
probe.write_text('#include <stdio.h>\n#include <math.h>\nvolatile float x=0.25f;\n'
                 'int main(void){printf("%f\\n",(double)sinf(x));return 0;}\n')
command = [str(sdk / 'bin/arm-vita-eabi-gcc'), '-mfloat-abi=softfp', '-fno-builtin',
           '-Wl,-q', str(probe), '-lm', '-o', str(OUT / 'softfp.elf')]
try:
    result = subprocess.run(command, env=env, text=True, capture_output=True)
    probe_ok = result.returncode == 0
    (OUT / 'softfp.log').write_text(result.stdout + result.stderr)
except FileNotFoundError as exc:
    probe_ok = False
    (OUT / 'softfp.log').write_text(str(exc))
report = dict(sdk=str(sdk), softfp_link_probe_passed=probe_ok, missing=missing,
              note='Presence checks do not prove dependency ABI compatibility or a working game.')
(OUT / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
sys.exit(0 if probe_ok and not missing else 1)
