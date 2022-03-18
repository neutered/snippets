#!/usr/bin/env python

import csv
import matplotlib.pyplot
import numpy
import scipy
import sys

raws = {}

assert len(sys.argv) > 1
for p in sys.argv[1:]:
    try:
        f = open(p, 'r')
    except IOError as e:
        print >> sys.stderr, 'input open(%s) : %s' % (p, str(e))
        continue
    l = p.split('/')[-1]
    prefix = l.split('.')[0]

    reader = csv.reader(f)
    cols = reader.next()

    signals = dict()
    for c in cols:
        signals[c] = list()

    for l in reader:
        i = 0
        for c in cols:
            signals[c].append(int(l[i]))
            i = i + 1
    raws[prefix] = signals

height = 20000
width = -1
for prefix,signals in raws.items():
    for c in signals.keys():
        n = len(signals[c])
        print 'prefix:%s width:%d n:%d' % (prefix, width, n)
        if width == -1 or n < width:
            width = n
        break
print 'width:%d' % width

origs = dict()
signals = raws['orig']
for c in signals.keys():
    origs[c] = numpy.fft.fft(signals[c][:width], norm=None)

for prefix,signals in raws.items():
    for c in signals.keys():
        yf = numpy.fft.fft(signals[c][:width], norm=None)
        xf = numpy.fft.fftfreq(width, 0.01)
        matplotlib.pyplot.plot(xf, numpy.abs(origs[c]), 'r')
        if prefix != 'orig':
            matplotlib.pyplot.plot(xf, numpy.abs(yf), 'b', alpha=0.5)
        matplotlib.pyplot.title(prefix + '-' + c)
        matplotlib.pyplot.ylim(0, height)
        matplotlib.pyplot.show()
