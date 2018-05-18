#!/usr/bin/env python3

import sys
import collections
import matplotlib.pyplot as plt

CT_LARGE_OFFSET = 0
CT_LARGE_LENGTH = 4

CT_LARGE_SIZE = int('0x20000', 16)
CT_LARGE_HASH = 8339


def hash_large_ct(val):
    return (to_int(val) * CT_LARGE_HASH) & (CT_LARGE_SIZE - 1)


def to_int(val):
    return ord(val[3]) << 24 | ord(val[2]) << 16 | ord(val[1]) << 8 | ord(val[0]) << 0


def main():
    if len(sys.argv) != 2:
        executable_name = sys.argv[0]
        print(f"Usage: {executable_name} FILE")
        sys.exit(1)

    filename = sys.argv[1]
    offset = CT_LARGE_OFFSET
    length = CT_LARGE_LENGTH

    subpatterns = []
    with open(filename) as f:
        for pattern in f:
            pattern = pattern.rstrip()
            hex_pattern = parse_hex(pattern)
            if len(hex_pattern) >= length:
                start = offset
                end = offset + length if offset + \
                    length > 0 else len(hex_pattern)
                sub = hex_pattern[start:end]
                subpatterns.append(sub)

    print("Unique values: {}".format(len(set(subpatterns))))
    plot_equal_character_count(subpatterns)
    plot_hash_frequency(subpatterns)


def plot_equal_character_count(subpatterns):
    counter = collections.Counter(subpatterns)
    print("10 max subpatterns and their counts")
    print(sorted(counter.items(), key=lambda x: x[1], reverse=True)[0:10])

    counts = counter.values()
    plt.hist(counts, bins=[x for x in range(min(counts), max(counts))])
    plt.title('Equal character distribution')
    plt.show()


def plot_hash_frequency(subpatterns):
    hashes = [hash_large_ct(x) for x in subpatterns if len(x) == 4]

    print("10 max hashes and their counts")
    print(sorted(collections.Counter(hashes).items(),
                 key=lambda x: x[1], reverse=True)[0:10])
    hash_count = len(set(hashes))
    print(f"Unique hashes: {hash_count} / {CT_LARGE_SIZE}")

    plt.hist(hashes, bins=[x for x in range(min(hashes), max(hashes))])
    plt.title('Large CT hash distribution')
    plt.show()


def parse_hex(pattern):
    resulting_pattern = ''

    hex_pattern = ''
    in_hex = False
    for character in pattern:
        if character == '|' and in_hex:
            resulting_pattern += to_hex(hex_pattern)
            hex_pattern = ''
            in_hex = False
        elif character == '|':
            in_hex = True
        elif in_hex:
            hex_pattern += character
        else:
            resulting_pattern += character

    return resulting_pattern


def to_hex(hex_pattern):
    resulting_pattern = ''
    x = ''
    y = ''
    for i, character in enumerate(hex_pattern):
        if i % 3 == 0:
            x = character
        elif i % 3 == 1:
            y = character
            resulting_pattern += from_hex(x, y)

    return resulting_pattern


def from_hex(x, y):
    x = to_binary(x)
    y = to_binary(y)

    return chr(x * 16 + y)


def to_binary(x):
    val = ord(x)
    if val >= ord('0') and val <= ord('9'):
        return val - ord('0')
    elif val >= ord('A') and val <= ord('F'):
        return val - ord('A') + 10
    else:
        print('Invalid hex')
        sys.exit(1)


if __name__ == '__main__':
    main()
