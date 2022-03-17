#!/usr/bin/env python

import csv
import matplotlib.pyplot
import numpy
import scipy
import sys

signals = {}

assert len(sys.argv) > 1
for p in sys.argv[1:]:
    try:
        f = open(p, 'r')
    except IOError as e:
        print >> sys.stderr, 'input open(%s) : %s' % (p, str(e))
        continue
    l = p.split('/')[-1]
    title_prefix = l.split('.')[0]

    reader = csv.reader(f)
    cols = reader.next()
    print str(cols)
    for c in cols:
        signals[c] = list()

    for l in reader:
        i = 0
        for c in cols:
            signals[c].append(int(l[i]))
            i = i + 1
    xs = [x*0.01 for x in xrange(len(signals[cols[0]]))]

    for c in cols:
        yf = numpy.fft.fft(signals[c], norm=None)
        xf = numpy.fft.fftfreq(len(signals[c]), 0.01)
        matplotlib.pyplot.plot(xf, numpy.abs(yf))
        matplotlib.pyplot.title(title_prefix + '-' + c)
        matplotlib.pyplot.show()
