#!/usr/bin/env python3
"""lzpack.py — LZSS + range-coder packer for morphdemo-4k (see src/stub.c).

Output stream (payload.bin):
  [0..3]  little-endian uint32: decompressed length
  [4..]   range-coded LZSS item stream:
            item = literal byte, or match (offset, length).
            The range coder codes, per item: one flag bit (1 = literal),
            8 literal bits (MSB first), OFFB offset bits (MSB first,
            offset-1) and LENB length bits (MSB first, length-3).

The LZSS token is always 2 bytes: w = ((offset-1) << LENB) | (length-3),
so OFFB + LENB = 16; the window is 1<<OFFB and lengths run 3..2+2^LENB.
The stub's decoder size is independent of the split (it always uses 25
probability models: 1 flag + 8 literal + OFFB offset + LENB length).

The range coder is an LZMA-style binary arithmetic coder: 11-bit adaptive
probabilities, one model per bit position (MSB first).  The encoder emits
one leading dummy byte (the initial carry cache); the decoder skips it.

The LZSS parse is optimal: a dynamic program over positions chooses the
item sequence with the least total cost.  Costs are per item, in bytes:
a literal costs litc + flagc, a match matchc + flagc.  For the raw stream
(litc=1, matchc=2, flagc=1/8) these are the true byte costs; for the
range-coded stream they are the measured bit costs (litc~1.03, matchc~1.62,
flagc~0.10), which steer the parse towards the item mix the coder likes.
Match-length candidates per position are 3..maxlen.

usage: lzpack.py [options] <input> <output>
  --offb=N    match offset bits (default 12 -> window 4096)
  --lenb=N    match length bits (default 4 -> lengths 3..18)
  --litc=F    literal cost in bytes (default 1.03, range-coded model)
  --matchc=F  match cost in bytes (default 1.62, range-coded model)
  --flagc=F   flag cost per item in bytes (default 0.10)
  --no-rc     emit the raw LZSS stream instead (no header, no range
              coder; costs default to the true byte costs)
  --check     after writing, decode the stream back and verify
  --grid      try a grid of (offb,lenb,litc,matchc,flagc), print the
              resulting sizes, and write the best combination
"""
import sys
from collections import defaultdict

INF = 1 << 60


# ---------------- LZSS ----------------

class Lz:
    def __init__(self, offb=12, lenb=4):
        assert offb + lenb == 16, "2-byte token: offb + lenb must be 16"
        self.offb, self.lenb = offb, lenb
        self.window, self.maxlen = 1 << offb, 2 + (1 << lenb)

    def longest_matches(self, data):
        n = len(data)
        W, ML = self.window, self.maxlen
        maxL = [0] * n
        maxO = [0] * n
        table = defaultdict(list)
        for i in range(n - 2):
            key = data[i:i + 3]
            best_l = 0
            best_o = 0
            for j in reversed(table[key][-48:]):
                off = i - j
                if off > W:
                    break
                l = 0
                while l < ML and i + l < n and data[j + l] == data[i + l]:
                    l += 1
                if l > best_l:
                    best_l, best_o = l, off
                    if l >= ML:
                        break
            table[key].append(i)
            maxL[i] = best_l
            maxO[i] = best_o
        return maxL, maxO

    def parse_items(self, data, litc=1.03, matchc=1.62, flagc=0.10):
        """Optimal DP parse -> (items, maxO); items = [(pos, length)],
        length 1 = literal.  Costs are per item in bytes."""
        n = len(data)
        maxL, maxO = self.longest_matches(data)
        fl = litc + flagc
        fm = matchc + flagc
        cost = [INF] * (n + 1)
        par = [None] * (n + 1)
        cost[0] = 0.0
        for i in range(n):
            c = cost[i]
            if c >= INF:
                continue
            nc = c + fl
            if nc < cost[i + 1]:
                cost[i + 1] = nc
                par[i + 1] = (i, 1)
            ml = maxL[i]
            if ml >= 3:
                for L in range(3, min(ml, self.maxlen) + 1):
                    nc = c + fm
                    if nc < cost[i + L]:
                        cost[i + L] = nc
                        par[i + L] = (i, L)
        items = []
        k = n
        while k > 0:
            pi, L = par[k]
            items.append((pi, L))
            k = pi
        items.reverse()
        return items, maxO

    def stream(self, data):
        """Raw LZSS byte stream: groups of 8 items, each group preceded by
        one flag byte (LSB first; bit set -> one literal byte follows,
        bit clear -> 2-byte match token w = ((off-1)<<lenb)|(len-3))."""
        items, maxO = self.parse_items(data, litc=1.0, matchc=2.0,
                                       flagc=0.125)
        out = bytearray()
        gi = 0
        while gi < len(items):
            flags = 0
            group = bytearray()
            for bit in range(8):
                if gi >= len(items):
                    break
                pos, L = items[gi]
                if L == 1:
                    flags |= 1 << bit
                    group.append(data[pos])
                else:
                    w = ((maxO[pos] - 1) << self.lenb) | (L - 3)
                    group.append(w & 0xFF)
                    group.append((w >> 8) & 0xFF)
                gi += 1
            out.append(flags)
            out += group
        return bytes(out)


# ---------------- LZMA-style binary range coder ----------------

class RangeEnc:
    def __init__(self):
        self.low = 0
        self.rng = 0xFFFFFFFF
        self.cache = 0
        self.cache_size = 1
        self.out = bytearray()

    def _shift_low(self):
        if self.low < 0xFF000000 or self.low > 0xFFFFFFFF:
            carry = self.low >> 32
            self.out.append((self.cache + carry) & 0xFF)
            self.cache_size -= 1
            while self.cache_size > 0:
                self.out.append((0xFF + carry) & 0xFF)
                self.cache_size -= 1
            self.cache = (self.low >> 24) & 0xFF
        self.cache_size += 1
        self.low = (self.low << 8) & 0xFFFFFFFF

    def finish(self):
        for _ in range(5):
            self._shift_low()


class RangeDec:
    """Mirror of the stub's decoder, for round-trip checking."""

    def __init__(self, nprob):
        self.nprob = nprob
        self.pr = [1024] * nprob

    def decode(self, src, outlen):
        p = 1                      # skip the leading dummy byte
        rng = 0xFFFFFFFF
        code = 0
        for _ in range(4):
            code = ((code << 8) | src[p]) & 0xFFFFFFFF
            p += 1
        out = bytearray()
        while len(out) < outlen:
            def bit(ctx):
                nonlocal rng, code, p
                bound = (rng >> 11) * self.pr[ctx]
                if code < bound:
                    rng = bound
                    self.pr[ctx] += (2048 - self.pr[ctx]) >> 5
                    b = 0
                else:
                    code -= bound
                    rng -= bound
                    self.pr[ctx] -= self.pr[ctx] >> 5
                    b = 1
                if rng < (1 << 24):
                    rng = (rng << 8) & 0xFFFFFFFF
                    code = ((code << 8) | src[p]) & 0xFFFFFFFF
                    p += 1
                return b
            def bits(base, n):
                v = 0
                for i in range(n):
                    v = (v << 1) | bit(base + i)
                return v
            if bit(0):
                out.append(bits(1, 8))
            else:
                off = bits(9, self.offb) + 1
                ln = bits(9 + self.offb, self.lenb) + 3
                for _ in range(ln):
                    out.append(out[-off])
        return bytes(out)


class ItemRC:
    """Range coder over the LZSS item list: one flag model, 8 literal-bit
    models, OFFB offset-bit models, LENB length-bit models."""

    def __init__(self, offb=12, lenb=4):
        self.offb, self.lenb = offb, lenb
        self.e = RangeEnc()
        self.pr = [1024] * (1 + 8 + offb + lenb)

    def _bit(self, ctx, b):
        e = self.e
        bound = (e.rng >> 11) * self.pr[ctx]
        if b:
            e.low += bound
            e.rng -= bound
            self.pr[ctx] -= self.pr[ctx] >> 5
        else:
            e.rng = bound
            self.pr[ctx] += (2048 - self.pr[ctx]) >> 5
        if e.rng < (1 << 24):
            e.rng <<= 8
            e._shift_low()

    def encode(self, data, litc=1.03, matchc=1.62, flagc=0.10):
        lz = Lz(self.offb, self.lenb)
        items, maxO = lz.parse_items(data, litc, matchc, flagc)
        for pos, L in items:
            if L == 1:
                self._bit(0, 1)
                v = data[pos]
                for i in range(8):
                    self._bit(1 + i, (v >> (7 - i)) & 1)
            else:
                self._bit(0, 0)
                w = maxO[pos] - 1
                for i in range(self.offb):
                    self._bit(1 + 8 + i, (w >> (self.offb - 1 - i)) & 1)
                for i in range(self.lenb):
                    self._bit(1 + 8 + self.offb + i,
                              ((L - 3) >> (self.lenb - 1 - i)) & 1)
        self.e.finish()
        return bytes(self.e.out)


def pack(data, offb, lenb, litc, matchc, flagc):
    out = ItemRC(offb, lenb).encode(data, litc, matchc, flagc)
    return len(data).to_bytes(4, 'little') + out


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    opts = dict(a[2:].split('=', 1) if '=' in a else (a[2:], '1')
                for a in sys.argv[1:] if a.startswith('--'))
    src, dst = args[0], args[1]
    data = open(src, "rb").read()
    n = len(data)

    def fopt(name, default):
        return float(opts[name]) if name in opts else default

    if 'grid' in opts:
        best = None
        for offb, lenb in ((13, 3), (12, 4), (11, 5), (10, 6), (9, 7)):
            for litc in (1.0, 1.03, 1.06):
                for matchc in (1.5, 1.62, 1.75, 2.0):
                    out = pack(data, offb, lenb, litc, matchc, 0.10)
                    print(f"  offb={offb:2d} lenb={lenb} litc={litc:.2f} "
                          f"matchc={matchc:.2f}: {len(out)} bytes "
                          f"({100.0*len(out)/n:.2f}%)", file=sys.stderr)
                    if best is None or len(out) < len(best[0]):
                        best = (out, offb, lenb, litc, matchc)
        out, offb, lenb, litc, matchc = best
        print(f"grid best: offb={offb} lenb={lenb} litc={litc} "
              f"matchc={matchc}: {len(out)} bytes", file=sys.stderr)
        with open(dst, "wb") as f:
            f.write(out)
        return

    offb = int(fopt('offb', 12))
    lenb = int(fopt('lenb', 4))
    no_rc = 'no-rc' in opts
    if no_rc:
        litc, matchc, flagc = 1.0, 2.0, 0.125
    else:
        litc = fopt('litc', 1.03)
        matchc = fopt('matchc', 1.62)
        flagc = fopt('flagc', 0.10)

    lz = Lz(offb, lenb)
    if no_rc:
        out = lz.stream(data)
        print(f"lzss raw: {n} -> {len(out)} bytes "
              f"({100.0*len(out)/n:.1f}%)", file=sys.stderr)
    else:
        out = pack(data, offb, lenb, litc, matchc, flagc)
        print(f"itemrc:   {n} -> {len(out)} bytes "
              f"({100.0*len(out)/n:.1f}%) "
              f"(offb={offb} lenb={lenb} litc={litc} matchc={matchc})",
              file=sys.stderr)

    with open(dst, "wb") as f:
        f.write(out)
    print(f"lzpack: {n} -> {len(out)} bytes "
          f"({100.0*len(out)/max(1,n):.1f}%)", file=sys.stderr)

    if 'check' in opts and not no_rc:
        dec = RangeDec(1 + 8 + offb + lenb)
        dec.offb, dec.lenb = offb, lenb
        back = dec.decode(out[4:], n)
        if back == data:
            print("check: round-trip OK", file=sys.stderr)
        else:
            print("check: ROUND-TRIP FAILED", file=sys.stderr)
            sys.exit(1)


if __name__ == "__main__":
    main()
