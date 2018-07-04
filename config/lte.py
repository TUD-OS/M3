import os, sys

sys.path.append(os.path.realpath('hw/gem5/configs/example'))
from dtu_fs import *

options = getOptions()
root = createRoot(options)

cmd_list = options.cmd.split(",")

num_mem = 1
num_pes = int(os.environ.get('M3_GEM5_PES'))
num_accels = int(os.environ.get('M3_GEM5_ACCELS'))
fsimg = os.environ.get('M3_GEM5_FS')
fsimgnum = os.environ.get('M3_GEM5_FSNUM', '1')
dtupos = int(os.environ.get('M3_GEM5_DTUPOS', 0))
mmu = int(os.environ.get('M3_GEM5_MMU', 0))
mem_pe = num_pes

pes = []

# create the core PEs
for i in range(0, num_pes):
    pe = createCorePE(noc=root.noc,
                      options=options,
                      no=i,
                      cmdline=cmd_list[i],
                      memPE=mem_pe,
                      l1size='32kB',
                      l2size='256kB',
                      dtupos=dtupos,
                      mmu=mmu == 1)
    pes.append(pe)

# create the memory PEs
for i in range(0, num_mem):
    pe = createMemPE(noc=root.noc,
                     options=options,
                     no=num_pes + i,
                     size='3072MB',
                     image=fsimg if i == 0 else None,
                     imageNum=int(fsimgnum))
    pes.append(pe)

# create accelerator PEs
for i in range(0, num_accels):
    options.cpu_clock = '1000MHz' if i % 2 == 0 else '568MHz'
    pe = createAccelPE(noc=root.noc,
                       options=options,
                       no=num_pes + num_mem + i,
                       accel='fft',
                       memPE=mem_pe,
                       spmsize='64kB')
                       #l1size='32kB')
    pes.append(pe)

runSimulation(root, options, pes)
