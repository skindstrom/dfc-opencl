#! /usr/bin/env python3

import sys
import matplotlib.pyplot as plt

def main():
    if len(sys.argv) != 2:
        executable_name = sys.argv[0]
        print(f"Usage: {executable_name} FILE")
        sys.exit(1)

    filename = sys.argv[1]

    patterns = []
    with open(filename) as f:
        for pattern in f:
            patterns.append(pattern.rstrip())

    lengths = [len(x) for x in patterns]
    plt.hist(lengths, bins=[x for x in range(min(lengths), max(lengths))])
    plt.title('Distribution of pattern lengths')
    plt.show()

if __name__ == '__main__':
    main()