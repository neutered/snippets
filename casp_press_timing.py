#!/usr/bin/env python
#
# hopefully the files are split into like kinds to run.
# the tests are regular polarity and presses or press-and-hold.
#
# <status> <lpf> <offset> <status * 1000>

import csv
import numpy
import sys

# noise in 8bit w/ the stupid dropping three bits thing
noise_rms = 5 * 8

# fraction of max to count as outside of 'early release'
fraction = 3

class phase:
    def __init__(self):
        self.vals = []
        self.min = 4096
        self.max = -4096
        self.area = 0

    def add(self, v):
        self.vals.append(v)
        self.area += v
        if v < self.min:
            self.min = v
        if v > self.max:
            self.max = v

    def drop(self, n):
        assert n >= 0
        if n == 0:
            return
        self.vals = self.vals[0:len(self.vals) - n]

class transaction:
    def __init__(self, p, i, r):
        self.press = p
        self.tpress = len(p.vals)
        self.inter = i
        self.tinter = len(i.vals)
        self.release = r
        self.trelease = len(r.vals)
transactions = []

assert len(sys.argv) > 1
for p in sys.argv[1:]:
    try:
        f = open(p, 'r')
    except IOError as e:
        print >> sys.stderr, 'open(%s) : %s' % (p, str(e))
        continue

    reader = csv.reader(f)
    cols = reader.next()
    print str(cols)
    i_zsum = cols.index('z sum')
    i_zoffset = cols.index('z offset')
    adcs = list()
    for l in reader:
        z = int(l[i_zsum])
        o = int(l[i_zoffset])
        adcs.append(z - o)
    print 'adcs:%d' % len(adcs)

    i = 0
    while i < len(adcs):
        # skip idle
        while i < len(adcs):
            d = adcs[i]
            if d > noise_rms:
                break
            i += 1

        # rising peak
        press = phase()
        while i < len(adcs):
            d = adcs[i]
            if d < noise_rms:
                break
            press.add(d)
            i += 1
        print 'press:%s' % str(press.vals)

        # we track the between rising/falling peaks to check for both
        # the early release statistics.
        inter = phase()
        inter_end = i
        while i < len(adcs):
            d = adcs[i]
            if d > -noise_rms:
                inter_end = i
            if d < -press.max / fraction:
                break
            inter.add(d)
            i += 1
        # back up for the release
        assert i >= inter_end
        inter.drop(i - inter_end)
        i = inter_end
        print 'inter:%s' % str(inter.vals)

        # falling peak
        release = phase()
        releasing = False
        while i < len(adcs):
            d = adcs[i]
            if releasing and d > -noise_rms:
                break
            releasing |= d < -press.max / fraction
            release.add(d)
            i += 1
        print 'release:%s' % str(release.vals)

        print '%d:%d:%d' % (len(press.vals), len(inter.vals), len(release.vals))
        if len(release.vals) > 0:
            transactions.append(transaction(press, inter, release))

    presses = []
    inters = []
    releases = []
    for t in transactions:
        presses.append(t.tpress)
        inters.append(t.tinter)
        releases.append(t.trelease)
    print 'press:%f:%f' % (numpy.median(presses), numpy.std(presses))
    print '\t%s' % str(numpy.histogram(presses, 10, density=True))
    print 'inters:%f:%f' % (numpy.median(inters), numpy.std(inters))
    print '\t%s' % str(numpy.histogram(inters, 10, density=True))
    print 'release:%f:%f' % (numpy.median(releases), numpy.std(releases))
    print '\t%s' % str(numpy.histogram(releases, 10, density=True))

    f.close()
