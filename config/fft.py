import os, sys

sys.path.append(os.path.realpath('hw/gem5/configs/example'))
from dtu_fs import *

options = getOptions()
root = createRoot(options)

cmd_list = options.cmd.split(",")

num_mem = 1
num_pes = int(os.environ.get('M3_GEM5_PES'))
fsimg = os.environ.get('M3_GEM5_FS')
num_fft = 4
mem_pe = num_pes

pes = []

# create the core PEs
for i in range(0, num_pes):
    options.cpu_clock = '3GHz' if i < num_pes - 1 else '1GHz'
    pe = createCorePE(noc=root.noc,
                      options=options,
                      no=i,
                      cmdline=cmd_list[i],
                      memPE=mem_pe,
                      l1size='32kB',
                      l2size='256kB',
                      dtupos=2 if i < num_pes - 1 else 0,
                      mmu=1 if i < num_pes - 1 else 0)
    pes.append(pe)

# create the memory PEs
for i in range(0, num_mem):
    pe = createMemPE(noc=root.noc,
                     options=options,
                     no=num_pes + i,
                     size='1024MB',
                     content=fsimg if i == 0 else None)
    pes.append(pe)

# create accelerator PEs
for i in range(0, num_fft):
    options.cpu_clock = '500MHz'
    pe = createAccelPE(noc=root.noc,
                       options=options,
                       no=num_pes + num_mem + i,
                       accel='fft',
                       memPE=mem_pe,
                       spmsize='128kB')
                       #l1size='32kB')
    pes.append(pe)

runSimulation(root, options, pes)
