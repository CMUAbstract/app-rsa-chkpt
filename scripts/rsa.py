#!/usr/bin/python

import sys

from key_parser import *

def modexp(m, e, n):
    result = 1
    base = m % n
    while e > 0:
        if e & 0x1:
            result = (result * base) % n
        e >>= 1
        base = (base * base) % n
    return result

def count_bytes(n):
    return len("%x" % n) / 2 # no leading zeros allowed

def encrypt(cyphertext_file, plaintext_file, e, n):

    print "n=", "%x" % n
    print "e=", "%x" % e

    fin = open(plaintext_file, "r")
    fout = open(cyphertext_file, "w")

    block_size = count_bytes(n)
    padding = [0x01]


    while True:
        plaintext_bytes = fin.read(block_size - len(padding))
        if len(plaintext_bytes) == 0:
            break

        in_block = list(bytearray(plaintext_bytes))

        # Replace EOF with null-byte
        in_block = filter(lambda b: b != 0x0a, in_block)

        while len(in_block) < block_size - len(padding):
            in_block += [0xFF];
        in_block = padding + in_block[::-1]
        m = int("0x" + "".join(map(lambda d: "%02x" % d, in_block)), 16)

        c = modexp(m, e, n)

        for i in range(block_size):
            fout.write(bytearray([c & 0xff]))
            c >>= 8

key_file = sys.argv[1]
plaintext_file = sys.argv[2]
cyphertext_file = sys.argv[3]

key = parse_key(key_file)
encrypt(cyphertext_file, plaintext_file, key['E'], key['N'])
