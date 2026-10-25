#!/usr/bin/env python3
"""Recover constant JNI lookup arguments from ARM startup disassembly.

This is a narrow constant-propagation pass, not an ARM emulator or execution
trace. Unknown values stay unknown; reports are cross-checked against smali.
"""
import json
from pathlib import Path
import re
import struct

ROOT = Path(__file__).resolve().parents[1]
LIB = ROOT / 'work/analysis/libs3e_android.so'
b = LIB.read_bytes()
shoff = struct.unpack_from('<I', b, 32)[0]
shsize, shnum = struct.unpack_from('<HH', b, 46)
sections = [struct.unpack_from('<10I', b, shoff + i * shsize) for i in range(shnum)]


def offset(addr):
    for s in sections:
        if s[1] != 8 and s[3] <= addr < s[3] + s[5]:
            return s[4] + addr - s[3]
    return None


def word(addr):
    p = offset(addr)
    return struct.unpack_from('<I', b, p)[0] if p is not None else None


def string(addr):
    if not isinstance(addr, int):
        return None
    p = offset(addr)
    if p is None:
        return None
    end = b.find(b'\0', p, p + 256)
    if end < 0:
        return None
    value = b[p:end]
    return value.decode('ascii') if value and all(32 <= v < 127 for v in value) else None


def recover(start, end):
    registers = [None] * 16
    registers[0] = 'ENV'
    callbacks = []
    classes = []
    stack = {}
    # Cached class/method IDs do not need to be emulated for constant arguments.
    for address in range(start, end, 4):
        instruction = word(address)
        if instruction is None:
            break
        registers[15] = address + 8
        rn, rd, rm = (instruction >> 16) & 15, (instruction >> 12) & 15, instruction & 15
        if instruction & 0x0ffffff0 == 0x012fff30:  # BLX register
            call = registers[rm]
            returned_class = None
            if call == 'JNI:24':  # FindClass
                name = string(registers[1])
                if name:
                    classes.append(name)
                    returned_class = 'CLASS:' + name
            if call in ('JNI:132', 'JNI:452', 'JNI:376', 'JNI:576'):
                name, signature = string(registers[2]), string(registers[3])
                if name and signature:
                    kind = {'JNI:132': 'method', 'JNI:452': 'static_method',
                            'JNI:376': 'field', 'JNI:576': 'static_field'}[call]
                    owner = registers[1]
                    callbacks.append(dict(address=hex(address), kind=kind, name=name, signature=signature,
                                          class_name=owner[6:] if isinstance(owner, str) and owner.startswith('CLASS:') else None))
            for r in (0, 1, 2, 3, 12):
                registers[r] = None
            registers[0] = returned_class
        elif instruction & 0x0f000000 == 0x0b000000:  # BL
            for r in (0, 1, 2, 3, 12):
                registers[r] = None
        elif instruction & 0x0e500000 == 0x04100000:  # LDR immediate word, pre-index
            displacement = instruction & 0xfff
            if not instruction & (1 << 23):
                displacement = -displacement
            if registers[rn] == 'ENV' and displacement == 0:
                registers[rd] = 'VTABLE'
            elif registers[rn] == 'VTABLE':
                registers[rd] = f'JNI:{displacement}'
            elif rn == 15:
                registers[rd] = word(address + 8 + displacement)
            elif rn == 13:
                registers[rd] = stack.get(displacement)
            else:
                registers[rd] = None
        elif instruction & 0x0e500000 == 0x04000000 and rn == 13:  # STR stack immediate
            displacement = instruction & 0xfff
            if not instruction & (1 << 23):
                displacement = -displacement
            stack[displacement] = registers[rd]
        elif instruction & 0x0fe00000 == 0x00800000 and instruction & 0xff0 == 0:  # ADD register
            a, c = registers[rn], registers[rm]
            registers[rd] = (a + c) & 0xffffffff if isinstance(a, int) and isinstance(c, int) else None
        elif instruction & 0x0fe00ff0 == 0x01a00000:  # MOV register
            registers[rd] = registers[rm]
        elif instruction & 0x0fe00000 == 0x03a00000:  # MOV immediate
            imm, rotation = instruction & 255, ((instruction >> 8) & 15) * 2
            registers[rd] = ((imm >> rotation) | (imm << ((32 - rotation) % 32))) & 0xffffffff
    return dict(classes=classes, lookups=callbacks)


report = {'initNative': recover(0x3ad84, 0x3baf0)}
# Check full descriptors against original smali, not just decompiler guesses.
smali = '\n'.join(p.read_text() for p in (ROOT / 'work/apkdecoded/smali').rglob('*.smali'))
for function in report.values():
    for entry in function['lookups']:
        field = 'field' in entry['kind']
        pattern = r'^\.' + ('field' if field else 'method') + r'[^\n]*\s'
        pattern += re.escape(entry['name'] + (':' if field else '') + entry['signature']) + r'$'
        owner = entry['class_name']
        source = ROOT / 'work/apkdecoded/smali' / ((owner or '') + '.smali')
        entry['smali_declaration_found'] = bool(re.search(pattern, source.read_text() if source.is_file() else smali, re.M))
out = ROOT / 'work/analysis/java_callbacks.json'
out.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
