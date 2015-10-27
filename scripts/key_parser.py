#!/usr/bin/python

import sys
import re

keys_file = sys.argv[1]

def parse_digits(s):
    raw_digits = s.split(':')

    i = 0
    while raw_digits[i] == "00" or raw_digits[i] == "0":
        i = i + 1

    digits = []
    for d in raw_digits[i:]:
        digits += [d]

    return digits

def to_number(digits):
    return int("0x" + "".join(digits), 16)

def parse_key(keys_file):
    pairs = {}
    name = None
    for line in open(keys_file, "r"):
        m = re.match(r'^(?P<name>[^: \t]+):(\s*(?P<value>\d+))?', line)
        if m:
            # special case
            if m.group('name') == 'Private-Key':
                continue

            # Close the current multi-line value if any
            if name is not None:
                pairs[name] = to_number(parse_digits(value_str))
                name = None

            name = m.group('name')
            value_str = ""
            if m.group('value') is not None:
                pairs[name] = int(m.group('value'))
                name = None
        elif name is not None and re.match('^\s+[0-9A-Fa-f:]+', line):
            value_str += line.strip()

    # Close the last multi-line value if any
    if name is not None:
        pairs[name] = to_number(parse_digits(value_str))

    return dict(N=pairs['modulus'],
                E=pairs['publicExponent'],
                D=pairs['privateExponent'])
