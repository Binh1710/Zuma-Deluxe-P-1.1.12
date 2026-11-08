"""Add a 'LoadLibraryA("zdslots.dll")' stub at the entry point of ZDPlus.exe / ZDPlusO.exe.

usage: python patch_slots.py <in.exe> <out.exe>

The stub lives in the last 128 bytes of .text slack (0x4fdf80..0x4fe000), after the
caves used by the older bonus/timer patches. If the DLL is missing the game runs unchanged.
"""
import struct, sys

STUB_VA = 0x4FDF80
NAME_VA = 0x4FDFA0
ORIG_ENTRY = 0x4E2948
IAT_LOADLIBRARYA = 0x4FE0CC
DLL = b"zdslots.dll\0"

src, dst = sys.argv[1], sys.argv[2]
d = bytearray(open(src, "rb").read())
pe = struct.unpack_from("<I", d, 0x3C)[0]
base = struct.unpack_from("<I", d, pe + 24 + 28)[0]
entry_rva_off = pe + 24 + 16
optsz = struct.unpack_from("<H", d, pe + 20)[0]
sec = pe + 24 + optsz                      # first section header = .text
assert d[sec:sec + 5] == b".text"
vsize, va, rsize, roff = struct.unpack_from("<IIII", d, sec + 8)


def off(v):
    return roff + (v - base - va)


entry = struct.unpack_from("<I", d, entry_rva_off)[0] + base
if entry == STUB_VA:
    sys.exit("already patched")
assert entry == ORIG_ENTRY, hex(entry)
assert all(b == 0 for b in d[off(STUB_VA):off(0x4FE000)]), "slack not empty"
assert va + rsize >= 0x4FE000 - base

stub = bytearray()
stub += b"\x68" + struct.pack("<I", NAME_VA)             # push "zdslots.dll"
stub += b"\xFF\x15" + struct.pack("<I", IAT_LOADLIBRARYA)  # call [LoadLibraryA]
rel = ORIG_ENTRY - (STUB_VA + len(stub) + 5)
stub += b"\xE9" + struct.pack("<i", rel)                 # jmp original entry
assert len(stub) <= NAME_VA - STUB_VA
d[off(STUB_VA):off(STUB_VA) + len(stub)] = stub
d[off(NAME_VA):off(NAME_VA) + len(DLL)] = DLL

struct.pack_into("<I", d, entry_rva_off, STUB_VA - base)
struct.pack_into("<I", d, sec + 8, max(vsize, 0x4FE000 - base - va))
open(dst, "wb").write(d)
print("patched")
