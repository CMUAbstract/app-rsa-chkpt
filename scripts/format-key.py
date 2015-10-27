#!/usr/bin/python

import sys
from key_parser import  *

def extract_digits(n):
    digits = []
    while n > 0:
        digits += [n & 0xff]
        n >>= 8
    return digits

def format_array(n):
    digits = extract_digits(n)
    return ",".join(map(lambda d: "0x%02x" % d, digits))

key_file = sys.argv[1]
code_file = sys.argv[2]

key = parse_key(key_file)
fout = open(code_file, "w")
fout.write("// modulus: byte order: LSB to MSB, constraint MSB>=0x80\n");
fout.write(".n = { " + format_array(key['N']) + " },\n")
fout.write(".e = " + "0x%x" % key['E'] + "\n")
