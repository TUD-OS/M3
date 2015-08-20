#!/usr/bin/env python2

import tomahawk as th
import sys
import time
import ConfigParser
import StringIO

# read memory layout
ini_str = open('memlayout.ini', 'r').read()
config = ConfigParser.RawConfigParser()
config.readfp(StringIO.StringIO(ini_str))

ARGC_ADDR = int(config.get("memlayout", "ARGC_ADDR"))
ARGV_ADDR = int(config.get("memlayout", "ARGV_ADDR"))
ARGV_SIZE = int(config.get("memlayout", "ARGV_SIZE"))
ARGV_START = int(config.get("memlayout", "ARGV_START"))
SERIAL_ACK = int(config.get("memlayout", "SERIAL_ACK"))
SERIAL_BUF = int(config.get("memlayout", "SERIAL_BUF"))
BOOT_ENTRY = int(config.get("memlayout", "BOOT_ENTRY"))
BOOT_SP = int(config.get("memlayout", "BOOT_SP"))
BOOT_CHANS = int(config.get("memlayout", "BOOT_CHANS"))
BOOT_CAPS = int(config.get("memlayout", "BOOT_CAPS"))
BOOT_LAMBDA = int(config.get("memlayout", "BOOT_LAMBDA"))
BOOT_MOUNTS = int(config.get("memlayout", "BOOT_MOUNTS"))
STATE_SPACE = int(config.get("memlayout", "STATE_SPACE"))
BOOT_EXIT = int(config.get("memlayout", "BOOT_EXIT"))
BOOT_DATA = int(config.get("memlayout", "BOOT_DATA"))
CORE_CONF = int(config.get("memlayout", "CORE_CONF"))
CORE_CONF_SIZE = int(config.get("memlayout", "CORE_CONF_SIZE"))
DRAM_CCOUNT = int(config.get("memlayout", "DRAM_CCOUNT"))
TRACE_MEMBUF_SIZE = int(config.get("memlayout", "TRACE_MEMBUF_SIZE"))
TRACE_MEMBUF_ADDR = int(config.get("memlayout", "TRACE_MEMBUF_ADDR"))

def charToInt(c):
    if c >= ord('a'):
        return 10 + c - ord('a')
    return c - ord('0')

def strToBytes(str):
    bytes = []
    for i in range(0, 16, 2):
        i1 = charToInt(ord(str[i]))
        i2 = charToInt(ord(str[i + 1]))
        bytes += [(i1 << 4) | i2]
    return reversed(bytes)

def beginToInt64(str):
    res = 0
    i = 0
    for x in range(0,16,2):
        if i >= len(str):
            break
        res |= ord(str[i]) << x * 4
        i += 1
    return res

def stringToInt64s(str):
    vals = []
    for i in range(0,len(str),8):
        int64 = beginToInt64(str[i:i + 8])
        vals += [int64]
    return vals

def checkOffset(off):
    if off >= ARGV_START + ARGV_SIZE:
        print "Error: Argument size is too large!"
        sys.exit(1)

def initMem(core, argv):
    # init arguments in argv space
    core.mem[ARGV_START] = ARGV_START + 4
    off = ARGV_START + ((len(argv) + 1) / 2) * 8
    argptr = []
    for a in argv:
        argptr += [off]
        for v in stringToInt64s(a):
            checkOffset(off)
            core.mem[off] = v
            off += 8
        checkOffset(off)
        core.mem[off] = 0
        off += 8

    # init argv array
    for a in range(0,len(argptr),2):
        val = argptr[a]
        if a < len(argptr) - 1:
            val |= argptr[a + 1] << 32
        core.mem[ARGV_START + a * 4] = val

    # start with empty caps and chans
    core.mem[BOOT_CAPS] = 0
    core.mem[BOOT_CHANS] = 0
    core.mem[BOOT_MOUNTS] = 0
    core.mem[STATE_SPACE] = 0
    # set argc and argv
    core.mem[ARGC_ADDR] = len(argv)
    core.mem[ARGV_ADDR] = ARGV_START
    # call main
    core.mem[BOOT_LAMBDA] = 0
    core.mem[BOOT_EXIT] = 0
    # use default stack pointer
    core.mem[BOOT_SP] = 0

    # clear core config
    core.mem[SERIAL_ACK] = 0
    for a in range(0, CORE_CONF_SIZE, 8):
        core.mem[CORE_CONF + a] = 0

def fetchPrint(core, id):
    length = core.mem[SERIAL_ACK] & 0xFFFFFFFF
    if length != 0 and length < 256:
        line = ""
        t1 = time.time()
        line += "%08.4f> " % (t1 - t0)
        length = (length + 7) & ~7
        for off in range(0, length, 8):
            val = core.mem[SERIAL_BUF + off]
            for x in range(0, 64, 8):
                hexdig = (val >> x) & 0xFF
                if hexdig == 0:
                    break
                if (hexdig < 0x20 or hexdig > 0x80) and hexdig != ord('\t') and hexdig != ord('\n') and hexdig != 033:
                    hexdig = ord('?')
                line += chr(hexdig)
        sys.stdout.write(line)
        sys.stdout.flush()
        log.write(line)
        core.mem[SERIAL_ACK] = 0
        if "kernel" in line and "Shutting down..." in line:
            return 2
        return 1
    elif length != 0:
        print "Got invalid length from PE ", id, ": ", length
    return 0

def getTraceFile():
    # check TRACE_MEMBUF_ADDR for a special value that indicates tracing is off
    if int(th.ddr_ram[TRACE_MEMBUF_ADDR]) == 0xffffffffffffffff:
        return 1

    print "Reading trace events from DRAM..."
    f = open("trace.txt", "w")
    for i in range(0, 8):
        pe = i + 4
        addr = TRACE_MEMBUF_ADDR + i * TRACE_MEMBUF_SIZE
        val = th.ddr_ram[addr]
        numEvents = int(val)
        if numEvents > 0:
            print "num trace events of PE ",pe,": ", numEvents
            print >>f, "p", pe
            for word in th.ddr_ram.readWords(addr + 8, numEvents):
                print >>f, word
    f.close()
    return 0

if len(sys.argv) < 6:
    print "Usage: %s <fsimg> <cm-core> <app-core> <logfile> <program>..." % sys.argv[0]
    sys.exit(1)

# open logfile
log = open(sys.argv[4], 'w')

# parse programs and their arguments, separated by --
progs = []
args = []
for arg in sys.argv[5:]:
    # don't pass ".mem" to target
    if arg[-4:] == ".mem":
        arg = arg[0:-4]
    if arg == '++':
        args += ['--']
    elif arg == '--':
        progs += [args]
        args = []
    else:
        args += [arg]
if len(args) > 0:
    progs += args

if len(progs) - 1 > len(th.duo_pes):
    print "Too few PEs"
    sys.exit(1)

# power on routers and global memory
for r in th.routers:
    r.on()
th.ddr_ram.on()

# copy fs-image to global memory
if sys.argv[1] != "-":
    th.ddr_ram.mem.writememdata(th.memfilestream(sys.argv[1]))
th.ddr_ram.mem[DRAM_CCOUNT] = 0

# will be overwritten when an event trace is produced
th.ddr_ram[TRACE_MEMBUF_ADDR] = 0xffffffffffffffff

print progs

# init App-Core
if sys.argv[3] != "-":
    print "Initializing memory of App-Core with " + sys.argv[3]
    th.app_core.initMem(sys.argv[3])

# init PEs
i = 0
for duo_pe in th.duo_pes[0:len(progs)]:
    print "Powering on PE", i

    # power on PE
    duo_pe.on()
    duo_pe.set_pmgt_val(0);

    print "Initializing memory of PE", i

    # load program
    duo_pe.initMem(progs[i][0] + ".mem")

    # set arguments, ...
    initMem(duo_pe, progs[i])

    i += 1

# put idle on all other cores
for duo_pe in th.duo_pes[len(progs):]:
    print "Powering on PE", i

    # power on PE
    duo_pe.on()
    duo_pe.set_pmgt_val(0);

    print "Initializing memory of PE with idle", i

    # load program
    duo_pe.initMem("idle.mem")

    # set argc and argv
    duo_pe.mem[ARGC_ADDR] = 0
    duo_pe.mem[ARGV_ADDR] = 0

    # ensure that he waits
    duo_pe.mem[BOOT_ENTRY] = 0
    # ensure that we don't receive prints
    duo_pe.mem[SERIAL_ACK] = 0

    i += 1

# init and start CM
if sys.argv[2] != "-":
    print "Powering on CM"
    th.cm_core.on()
    th.cm_core.set_ptable_val(10)   # 400 MHz
    print "Initializing memory of CM with " + sys.argv[2]
    th.cm_core.initMem(sys.argv[2])
    th.cm_core.start()

# start all PEs
i = 0
for duo_pe in th.duo_pes:
    print "Starting PE", i
    duo_pe.start()
    i += 1

if sys.argv[3] != "-":
    print "Starting App-Core"
    th.app_core.set_ptable_val(10)  # 400 MHz
    th.app_core.on()

t0 = time.time()
run = True
while run:
    i = 0
    counter = 0
    for duo_pe in th.duo_pes:
        res = fetchPrint(duo_pe, i)
        if res == 2:
            run = False
        else:
            counter += res
        i += 1

    # if nobody wanted to print something, take a break
    if counter == 0:
        time.sleep(0.01)

getTraceFile()

# now, read the fs image back from DRAM into a file
if sys.argv[1] != "-":
    print "Reading filesystem image from DRAM..."
    with open(sys.argv[1] + '.out', 'wb') as f:
        for addr, data in th.memfilestream(sys.argv[1]):
            for i, w in enumerate(th.ddr_ram.mem.__getslice__(addr, addr + len(data) * 8)):
                for b in strToBytes(w):
                    f.write("%c" % b)
print "Done. Bye!"
