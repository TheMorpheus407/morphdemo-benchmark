#!/usr/bin/env python3
import argparse
import bisect
from pathlib import Path

MASK = 0xFFFFFFFF
LITERAL_BASE = 256
LENGTH_BASE = LITERAL_BASE + 65536
DISTANCE_BASE = LENGTH_BASE + 4128
MODEL_SIZE = DISTANCE_BASE + 4128
MAX_MATCH = 1026


class Models:
    def __init__(self, shift=4):
        self.p = [2048] * MODEL_SIZE
        self.shift = shift

    def update(self, context, bit):
        p = self.p[context]
        if bit:
            self.p[context] = p - (p >> self.shift)
        else:
            self.p[context] = p + ((4096 - p) >> self.shift)


class Encoder(Models):
    def __init__(self, shift=4):
        super().__init__(shift)
        self.low = 0
        self.range = MASK
        self.cache = 0
        self.cache_size = 1
        self.output = bytearray()

    def shift_low(self):
        if self.low < 0xFF000000 or self.low > MASK:
            carry = self.low >> 32
            temp = self.cache
            while self.cache_size:
                self.output.append((temp + carry) & 255)
                temp = 255
                self.cache_size -= 1
            self.cache = (self.low >> 24) & 255
        self.cache_size += 1
        self.low = (self.low << 8) & MASK

    def bit(self, context, bit):
        p = self.p[context]
        bound = (self.range >> 12) * p
        if bit:
            self.low += bound
            self.range -= bound
        else:
            self.range = bound
        self.update(context, bit)
        while self.range < (1 << 24):
            self.range <<= 8
            self.shift_low()

    def gamma(self, base, value):
        n = value.bit_length() - 1
        for level in range(n):
            self.bit(base + level, 0)
        self.bit(base + n, 1)
        prefix = 1
        for shift in range(n - 1, -1, -1):
            bit = (value >> shift) & 1
            self.bit(base + 32 + (n << 8) + (prefix & 255), bit)
            prefix = (prefix << 1) | bit

    def finish(self):
        for _ in range(5):
            self.shift_low()
        return bytes(self.output)


class Decoder(Models):
    def __init__(self, data, shift=4):
        super().__init__(shift)
        self.data = data
        self.position = 5
        self.range = MASK
        self.code = int.from_bytes(data[:5].ljust(5, b"\0"), "big")

    def next_byte(self):
        if self.position < len(self.data):
            value = self.data[self.position]
        else:
            value = 0
        self.position += 1
        return value

    def bit(self, context):
        p = self.p[context]
        bound = (self.range >> 12) * p
        if self.code < bound:
            bit = 0
            self.range = bound
        else:
            bit = 1
            self.code -= bound
            self.range -= bound
        self.update(context, bit)
        while self.range < (1 << 24):
            self.range <<= 8
            self.code = ((self.code << 8) & MASK) | self.next_byte()
        return bit

    def gamma(self, base):
        n = 0
        while not self.bit(base + n):
            n += 1
        prefix = 1
        for _ in range(n):
            bit = self.bit(base + 32 + (n << 8) + (prefix & 255))
            prefix = (prefix << 1) | bit
        return prefix


def tokens(data):
    positions = {}
    for i in range(len(data) - 2):
        positions.setdefault(data[i:i + 3], []).append(i)
    matches = [()] * len(data)
    thresholds = (3, 4, 5, 9, 17, 33, 65, 129, 257, 513, 1025)
    for pos in range(len(data) - 2):
        candidates = positions[data[pos:pos + 3]]
        end = bisect.bisect_left(candidates, pos)
        options = set()
        limit = min(MAX_MATCH, len(data) - pos)
        for previous in reversed(candidates[max(0, end - 256):end]):
            offset = pos - previous
            if offset > 65535:
                break
            length = 3
            while length < limit and data[previous + length] == data[pos + length]:
                length += 1
            for candidate in thresholds:
                if candidate <= length:
                    options.add((candidate, offset))
            if length > 3:
                options.add((length, offset))
        matches[pos] = tuple(options)
    costs = [0] * (len(data) + 1)
    choices = [None] * len(data)
    for pos in range(len(data) - 1, -1, -1):
        best_cost = 9 + costs[pos + 1]
        best_token = (0, data[pos])
        for length, offset in matches[pos]:
            cost = (3 + 2 * ((length - 1).bit_length() - 1)
                    + 2 * (offset.bit_length() - 1) + costs[pos + length])
            if cost < best_cost:
                best_cost = cost
                best_token = (length, offset)
        costs[pos] = best_cost
        choices[pos] = best_token
    result = []
    pos = 0
    while pos < len(data):
        token = choices[pos]
        result.append(token)
        pos += token[0] if token[0] else 1
    return result


def pack(data, shift=4):
    encoder = Encoder(shift)
    previous = 0
    consumed = 0
    for length, value in tokens(data):
        if length:
            encoder.bit(previous, 1)
            encoder.gamma(LENGTH_BASE, length - 1)
            encoder.gamma(DISTANCE_BASE, value)
            consumed += length
            previous = data[consumed - 1]
        else:
            encoder.bit(previous, 0)
            prefix = 1
            for shift_bit in range(7, -1, -1):
                bit = (value >> shift_bit) & 1
                encoder.bit(LITERAL_BASE + (previous << 8) + prefix, bit)
                prefix = (prefix << 1) | bit
            previous = value
            consumed += 1
    return encoder.finish()


def decompress(data, size, shift=4):
    decoder = Decoder(data, shift)
    output = bytearray()
    previous = 0
    while len(output) < size:
        if decoder.bit(previous):
            length = decoder.gamma(LENGTH_BASE) + 1
            offset = decoder.gamma(DISTANCE_BASE)
            for _ in range(length):
                value = output[-offset]
                output.append(value)
                previous = value
        else:
            prefix = 1
            for _ in range(8):
                bit = decoder.bit(LITERAL_BASE + (previous << 8) + prefix)
                prefix = (prefix << 1) | bit
            value = prefix & 255
            output.append(value)
            previous = value
    return bytes(output)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input")
    parser.add_argument("output")
    parser.add_argument("--decompress", action="store_true")
    parser.add_argument("--size", type=int)
    parser.add_argument("--shift", type=int, default=4)
    args = parser.parse_args()
    data = Path(args.input).read_bytes()
    if args.decompress:
        result = decompress(data, args.size, args.shift)
    else:
        result = pack(data, args.shift)
        assert decompress(result, len(data), args.shift) == data
        print(f"LZR: {len(data)} -> {len(result)} bytes")
    Path(args.output).write_bytes(result)


if __name__ == "__main__":
    main()
