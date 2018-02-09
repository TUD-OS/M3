#!/usr/bin/env python

import sys
import subprocess
import re
from os.path import basename

funcs = []

def usage():
    print("Usage: %s (trace|functime) <binary>..." % sys.argv[0], file=sys.stderr)
    print("  With run/gem5.log as stdin and Exec,ExecPC enabled.", file=sys.stderr)
    sys.exit(1)

def addr2func(funcs, addr):
    if addr2func.last:
        if addr >= addr2func.last['addr'] and addr < addr2func.last['addr'] + addr2func.last['size']:
            return addr2func.last

    for f in funcs:
        if addr >= f['addr'] and addr < f['addr'] + f['size']:
            addr2func.last = f
            return f
    return {}

addr2func.last = {}

def funcname(funcs, addr):
    f = addr2func(funcs, addr)
    if not f:
        return '<Unknown>: %x' % addr

    name = f['name'].decode('ASCII')
    return '\033[1m%s\033[0m @ %s+0x%x' % (basename(f['bin']), name, addr - f['addr'])

if len(sys.argv) < 2:
    usage()

# read symbols
for i in range(2, len(sys.argv)):
    proc = subprocess.Popen(['nm', '-SC', sys.argv[i]], stdout=subprocess.PIPE)
    while True:
        line = proc.stdout.readline()
        if not line:
            break

        m = re.match(b'^([a-f0-9]+) ([a-f0-9]+) (t|T|w|W) ([A-Za-z0-9_:\.\~<>, ]+)', line)
        if m:
            funcs.append({
                'addr': int(m.group(1), 16),
                'size': int(m.group(2), 16),
                'name': m.group(4),
                'bin': sys.argv[i]
            })

# sort symbols by address
funcs = sorted(funcs, key=lambda f: f['addr'])

# read log lines
if sys.argv[1] == 'trace':
    while True:
        line = sys.stdin.readline()
        if not line:
            break

        m = re.match('^\s*(\d+): (pe\d+\.cpu\d*) (\S+) : 0x([a-f0-9]+)\s*(?:@\s+)?[^@]+?\s+:\s+(.*)$', line)
        if m:
            func = funcname(funcs, int(m.group(4), 16));
            print("%7s: %s: %s : %s" % (m.group(1), m.group(2), func, m.group(5)));
        else:
            print(line.rstrip())
elif sys.argv[1] == 'functime':
    pes = {}
    while True:
        line = sys.stdin.readline()
        if not line:
            break

        m = re.match('^\s*(\d+): (pe(\d+)\.cpu\d*) (\S+) : 0x([a-f0-9]+).*', line)
        if m:
            # find function
            addr = int(m.group(5), 16)
            func = addr2func(funcs, addr)
            if not func:
                func = {
                    'addr': addr,
                    'name': b'<Unknown>: %x' % addr,
                    'bin': '<Unknown>'
                }

            pe = int(m.group(3))

            # is it a new function?
            if not pe in pes or pes[pe]['func']['name'] != func['name']:
                tick = int(m.group(1))
                if pe in pes:
                    # print ticks for last function
                    name = pes[pe]['func']['name'].decode('ASCII')
                    print("%9s: %s: [%9u ticks] \033[1m%10.10s\033[0m @ %s+0x%x" % (
                        m.group(1), m.group(2), tick - pes[pe]['tick'],
                        basename(pes[pe]['func']['bin']), name,
                        pes[pe]['addr'] - pes[pe]['func']['addr']
                    ))
                # remember function start
                pes[pe] = {'func': func, 'tick': tick, 'addr': addr}
        else:
            m = re.match('^\s*(\d+): (.*)$', line)
            if m:
                print("%9s: %s" % (m.group(1), m.group(2)))
else:
    usage()
