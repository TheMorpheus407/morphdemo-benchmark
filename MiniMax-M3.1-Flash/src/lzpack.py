#!/usr/bin/env python3
"""lzpack.py -- the tiny LZ codec used by the self extracting 4 KiB binary.

The format is a sequence of items.  Every item starts with a token byte:

    high nibble   number of literal bytes that follow (15 means "read more")
    low nibble    length of the match that follows the literals (minus four)

The literals follow the token, then the distance is stored and the match is
copied from that distance back in the output.  A distance below 255 takes a
single byte; 255 is the escape and means that two more bytes follow.  A
length of 15 in either nibble means that extra length bytes follow, every
one of them adding 255, the last one adding the remainder.  The last item of
the stream has literals only and no distance.

The decoder is about fifteen lines of C, the encoder is this script.  The
window is 64 KiB, which covers the whole payload.
"""
import sys


def compress(src: bytes) -> bytes:
    """Hash chain LZ77 with lazy matching, packed into the token format."""
    n = len(src)
    out = bytearray()
    HBITS = 17
    HSIZE = 1 << HBITS
    head = [-1] * HSIZE
    prev = [-1] * (n + 1)

    def hash_at(p):
        v = (src[p] | (src[p + 1] << 8) | (src[p + 2] << 16) | (src[p + 3] << 24))
        v = (v * 2654435761) & 0xFFFFFFFF
        return v >> (32 - HBITS)

    def insert(p):
        if p + 4 <= n:
            h = hash_at(p)
            prev[p] = head[h]
            head[h] = p

    def find(p):
        """longest match at p, returns (length, distance) or (0, 0)"""
        if p + 4 > n:
            return 0, 0
        limit = min(258, n - p)
        best_len = 0
        best_off = 0
        cand = head[hash_at(p)]
        chain = 512
        while cand >= 0 and chain:
            chain -= 1
            off = p - cand
            if off <= 0 or off > 0xFFFF:
                break
            l = 0
            while l < limit and src[cand + l] == src[p + l]:
                l += 1
            if l > best_len:
                best_len = l
                best_off = off
                if l >= limit:
                    break
            cand = prev[cand]
        if best_len < 4:
            return 0, 0
        return best_len, best_off

    def emit_literals(chunk):
        """write one item that carries literals only"""
        ll = len(chunk)
        token = 0xF0 if ll >= 15 else ll << 4
        out.append(token)
        if ll >= 15:
            r = ll - 15
            while r >= 255:
                out.append(255)
                r -= 255
            out.append(r)
        out.extend(chunk)

    def emit(lit_len, match_len, dist):
        """one item: token, optional literal length, literals, distance,
        optional match length"""
        token = (0xF0 if lit_len >= 15 else lit_len << 4) | \
                (0x0F if match_len >= 19 else match_len - 4)
        out.append(token)
        if lit_len >= 15:
            r = lit_len - 15
            while r >= 255:
                out.append(255)
                r -= 255
            out.append(r)
        out.extend(src[lit_start:lit_start + lit_len])
        if dist < 255:
            out.append(dist)
        else:
            out.append(255)
            out.append(dist & 0xFF)
            out.append((dist >> 8) & 0xFF)
        if match_len >= 19:
            r = match_len - 19
            while r >= 255:
                out.append(255)
                r -= 255
            out.append(r)

    i = 0
    lit_start = 0
    while i < n:
        bl, bo = find(i)
        if bl >= 4 and i + 1 < n:
            # lazy matching: a longer match one byte later wins
            nl, no = find(i + 1)
            if nl > bl:
                bl = 0
        if bl >= 4:
            emit(i - lit_start, bl, bo)
            for k in range(i, i + bl):
                insert(k)
            i += bl
            lit_start = i
        else:
            insert(i)
            i += 1
    tail = src[lit_start:]
    if tail:
        emit_literals(tail)
    return bytes(out)


def decompress(src: bytes) -> bytes:
    out = bytearray()
    i = 0
    n = len(src)
    while i < n:
        token = src[i]
        i += 1
        ll = token >> 4
        if ll == 15:
            while True:
                s = src[i]
                i += 1
                ll += s
                if s != 255:
                    break
        out.extend(src[i:i + ll])
        i += ll
        if i >= n:
            break
        dist = src[i]
        i += 1
        if dist == 255:
            dist = src[i] | (src[i + 1] << 8)
            i += 2
        ml = (token & 15) + 4
        if (token & 15) == 15:
            while True:
                s = src[i]
                i += 1
                ml += s
                if s != 255:
                    break
        start = len(out) - dist
        for k in range(ml):
            out.append(out[start + k])
    return bytes(out)


def main():
    mode = sys.argv[1]
    if mode == "c":
        with open(sys.argv[2], "rb") as f:
            data = f.read()
        packed = compress(data)
        with open(sys.argv[3], "wb") as f:
            f.write(packed)
        print("%s: %d -> %d bytes (%.1f%%)" % (sys.argv[3], len(data), len(packed),
                                              100.0 * len(packed) / max(1, len(data))))
    elif mode == "d":
        with open(sys.argv[2], "rb") as f:
            data = f.read()
        out = decompress(data)
        with open(sys.argv[3], "wb") as f:
            f.write(out)
    elif mode == "t":
        with open(sys.argv[2], "rb") as f:
            data = f.read()
        packed = compress(data)
        assert decompress(packed) == data, "round trip failed"
        print("%s: %d -> %d bytes (%.1f%%) round trip ok" % (sys.argv[2], len(data),
              len(packed), 100.0 * len(packed) / max(1, len(data))))
    else:
        raise SystemExit("usage: lzpack.py c|d|t <in> [out]")


if __name__ == "__main__":
    main()
