#!/usr/bin/env python

import os
import struct
import sys

assert len(sys.argv) > 1
for p in sys.argv[1:]:
    try:
        f = open(p, 'rb')
    except IOError as e:
        print 'failed open(%s):%s' % (p, str(e))
        continue

    t = f.read(4)
    if t != 'RIFF':
        print 'incorrect tag:%s' % t
        continue

    f.seek(0, os.SEEK_END)
    eof = f.tell()
    f.seek(4, os.SEEK_SET)

    # wav header doesn't include tag + file length
    flen = 8 + struct.unpack('<I', f.read(4))[0]
    if flen != eof:
        print 'file length mismatch:%d != %d' % (eof, flen)
        continue

    t = f.read(4)
    if t != 'WAVE':
        print 'incorrect tag:%s' % t
        continue

    t = f.read(4)
    if t != 'fmt ':
        print 'incorrect tag:%s' % t
        continue

    # format legnth
    t = f.read(4)

    # we only handle signed pcm
    t = struct.unpack('<H', f.read(2))[0]
    if t != 1:
        print 'unhandled data format:%u' % t
        continue

    # anything >stereo is unhandled
    n = struct.unpack('<H', f.read(2))[0]
    if n > 2:
        print 'unhandled channel count:%u' % n
        continue

    sr = struct.unpack('<I', f.read(4))[0]

    # skip the multiplied out bits
    f.seek(6, os.SEEK_CUR)

    # we're too lazy for dealing w/ weird sample sizes
    bps = struct.unpack('<H', f.read(2))[0]
    if bps != 8 and bps != 16:
        print 'unhandled sample size:%u' % bps
        continue

    t = f.read(4)
    if t != 'data':
        print 'unhandled tag:%s' % t
        continue

    # FixMe: get use slop for a reasonable size check
    dlen = struct.unpack('<I', f.read(4))[0]
    if dlen >= flen:
        print 'invald data length:%u:%u' % (dlen, flen)
        continue
    if bps == 8:
        fmt = 'b'
        typ = 'uint8_t'
        offset = 2**7
        max = 2**8
    elif bps == 16:
        fmt = '<h'
        typ = 'uint16_t'
        offset = 2**15
        max = 2**16
    else:
        assert False
    bps /= 8

    bs = f.read(dlen)
    ss = ''
    for i in xrange(dlen / bps):
        v = struct.unpack(fmt, bs[0:bps])[0] + offset
        assert v >= 0
        assert v < max
        ss += '%u, ' % v
        bs = bs[2:]

    o = open(p + '.h', 'w')
    print >> o, 'const %s samples[] = { %s };' % (typ, ss)
    print >> o, 'const struct {\n' \
        'uint8_t n;\n' \
        'uint8_t bps;\n' \
        'uint32_t ns;\n' \
        'const %s* ss;\n' \
        '} sound = {\n' \
        '%u, %u, %u, samples,\n' \
        '};\n' % (typ, n, bps, dlen / bps)

    f.close()
