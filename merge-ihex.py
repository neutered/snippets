#!/usr/bin/env python
#
# flatten hex files

import sys

verbose = False

assert len(sys.argv) == 2
f = open(sys.argv[1], 'r')
assert f is not None

class range:
    def __init__(self, addr, bs, nb):
        self.addr = addr
        self.bs = bs
        self.nb = nb

addrs = dict()

# hexfile line format
#   :<#><address><op><data><xsum>
# ops
# - 00 - data
# - 01 - eof (last line)
# - 02 - real-mode segment value
# - 03 - start segment cs:ip
# - 04 - addr [31:16] for following data ops
# - 05 - addr [31:0]
for l in f:
    if l[0] != ':':
        print 'invalid:' + l
        continue
    nb = int(l[1:3], 16)
    addr = int(l[3:7], 16)
    op = int(l[7:9], 16)
    bs = l[9:9+2*nb]
    xsum = int(l[9+nb:9+2*nb+2], 16)

    if op == 0x00:
        addrs[addr] = range(addr, bs, nb)
    elif op == 0x01:
        assert nb == 0
        assert addr == 0
        assert len(f.read()) == 0
    else:
        print 'unhandled op:%02x' % op
        continue
f.close()

print 'initial pass:'
for (k, v) in addrs.items():
    print 'addr:%04x:%u' % (k, v.nb)
    if verbose:
        print v.bs

# one more pass to fix up holes from out of order blocks
ks = addrs.keys()
ks.sort()
for addr in ks:
    for (base, value) in addrs.items():
        if base > addr:
            break
        merged = value.addr + value.nb == addr
        if merged:
            old = addrs.pop(addr)
            value.bs += old.bs
            value.nb += old.nb
            break

print 'merged pass:'
for (k, v) in addrs.items():
    print 'addr:%04x:%u' % (k, v.nb)
    if verbose:
        print v.bs
