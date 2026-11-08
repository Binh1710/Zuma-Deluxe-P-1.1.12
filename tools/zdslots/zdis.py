import struct, sys, re
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

EXE = r"C:\Users\Acer\Desktop\Tệp\ZD+ v1.1\ZDPlus.exe"


class PE:
    def __init__(self, path=EXE):
        self.d = bytearray(open(path, 'rb').read())
        d = self.d
        pe = struct.unpack_from('<I', d, 0x3c)[0]
        self.pe = pe
        nsec = struct.unpack_from('<H', d, pe + 6)[0]
        optsz = struct.unpack_from('<H', d, pe + 20)[0]
        self.base = struct.unpack_from('<I', d, pe + 24 + 28)[0]
        self.entry = struct.unpack_from('<I', d, pe + 24 + 16)[0] + self.base
        self.secs = []
        so = pe + 24 + optsz
        for i in range(nsec):
            name = d[so:so + 8].rstrip(b'\0').decode()
            vs, va, rs, ro = struct.unpack_from('<IIII', d, so + 8)
            self.secs.append((name, va + self.base, vs, ro, rs, so))
            so += 40
        self.md = Cs(CS_ARCH_X86, CS_MODE_32)
        self.md.detail = False

    def off(self, va):
        for n, sva, vs, ro, rs, _ in self.secs:
            if sva <= va < sva + max(vs, rs):
                return ro + (va - sva)
        raise ValueError(hex(va))

    def va(self, off):
        for n, sva, vs, ro, rs, _ in self.secs:
            if ro <= off < ro + rs:
                return sva + off - ro
        return None

    def u32(self, va):
        return struct.unpack_from('<I', self.d, self.off(va))[0]

    def cstr(self, va):
        o = self.off(va); e = self.d.index(b'\0', o)
        return self.d[o:e].decode('latin-1')

    def dis(self, va, n=60, stop_ret=True):
        o = self.off(va)
        out = []
        for ins in self.md.disasm(bytes(self.d[o:o + n * 15]), va):
            s = '%08x  %-8s %s' % (ins.address, ins.mnemonic, ins.op_str)
            for m in re.findall(r'0x[0-9a-f]{6,8}', ins.op_str):
                v = int(m, 16)
                try:
                    if 0x4fe000 <= v < 0x540000:
                        cs = self.cstr(v)
                        if 2 < len(cs) < 80 and all(32 <= ord(c) < 127 for c in cs):
                            s += '   ; "%s"' % cs
                except Exception:
                    pass
            out.append(s)
            n -= 1
            if n <= 0 or (stop_ret and ins.mnemonic in ('ret',) and False):
                break
        return '\n'.join(out)

    def text(self):
        for n, sva, vs, ro, rs, _ in self.secs:
            if n == '.text':
                return sva, ro, min(vs, rs)

    def xrefs(self, target):
        """find instructions with imm/disp == target (raw byte search) and rel32 calls/jmps."""
        sva, ro, sz = self.text()
        res = []
        pat = struct.pack('<I', target)
        i = ro
        while True:
            i = self.d.find(pat, i, ro + sz)
            if i < 0: break
            res.append(('imm', sva + i - ro)); i += 1
        # rel32 calls / jmps
        b = self.d
        for k in range(ro, ro + sz - 5):
            op = b[k]
            if op in (0xE8, 0xE9):
                rel = struct.unpack_from('<i', b, k + 1)[0]
                if sva + (k - ro) + 5 + rel == target:
                    res.append(('call' if op == 0xE8 else 'jmp', sva + k - ro))
        return res

    def findstr(self, s):
        return [self.va(m.start()) for m in re.finditer(re.escape(s.encode()) + b'\0', bytes(self.d))]

    def func_start(self, va):
        """walk back to a likely function start (after CC/C3 padding)."""
        o = self.off(va)
        while o > 0:
            if self.d[o - 1] in (0xCC, 0x90) and self.d[o] not in (0xCC, 0x90):
                return self.va(o)
            o -= 1


if __name__ == '__main__':
    p = PE()
    cmd = sys.argv[1]
    if cmd == 'dis':
        print(p.dis(int(sys.argv[2], 16), int(sys.argv[3]) if len(sys.argv) > 3 else 60))
    elif cmd == 'x':
        t = int(sys.argv[2], 16)
        for k, a in p.xrefs(t):
            print(k, hex(a), 'func~', hex(p.func_start(a) or 0))
    elif cmd == 's':
        for a in p.findstr(sys.argv[2]):
            print(hex(a))
            for k, x in p.xrefs(a):
                print('  ', k, hex(x), 'func~', hex(p.func_start(x) or 0))
    elif cmd == 'secs':
        for s in p.secs: print(s[0], hex(s[1]), hex(s[2]), hex(s[3]), hex(s[4]))
        print('entry', hex(p.entry))


def vtables(p, cls):
    """return list of (vtable_va, nfuncs) for RTTI class name like PracticeScreen@Sexy"""
    import struct
    name = ('.?AV' + cls + '@@').encode()
    i = bytes(p.d).find(name)
    td = p.va(i - 8)
    res = []
    rd = [s for s in p.secs if s[0] == '.rdata'][0]
    ro, rs, sva = rd[3], rd[4], rd[1]
    for k in range(ro, ro + rs - 16, 4):
        sig, off, cdo, t = struct.unpack_from('<IIII', p.d, k)
        if t == td and sig == 0:
            col = p.va(k)
            pat = struct.pack('<I', col)
            j = bytes(p.d).find(pat, ro, ro + rs)
            while j >= 0:
                vt = p.va(j) + 4
                n = 0
                while True:
                    f = struct.unpack_from('<I', p.d, p.off(vt) + 4 * n)[0]
                    if not (0x401000 <= f < 0x4fe000): break
                    n += 1
                res.append((off, vt, n))
                j = bytes(p.d).find(pat, j + 4, ro + rs)
    return res
