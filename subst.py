#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys

def main():
    if len(sys.argv) != 2:
        print "Usage: %s template" % sys.argv[0]
        sys.exit(1)

    template = eval(open(sys.argv[1], "r").read())
    input = sys.stdin.read()

    for name, stat in template.iteritems():
        input = input.replace("%%(%s)" % name, "%0.1f~нс & %0.2f\\%% \\" % (stat[0], stat[1]/stat[0]))

    print input

if __name__ == '__main__':
    main()
