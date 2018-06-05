import os, sys

sys.path.append(os.path.realpath('hw/gem5/configs/example'))
from dtu_fs import *

options = getOptions()
root = createRoot(options)

cmd_list = options.cmd.split(",")

num_mem = 1
num_pes = int(os.environ.get('M3_GEM5_PES'))
fsimg = os.environ.get('M3_GEM5_FS')
num_fft = 2
fsimgnum = os.environ.get('M3_GEM5_FSNUM', '1')
mem_pe = num_pes + 1 + num_fft

pes = []

# create the core PEs
options.cpu_clock = '3GHz'
for i in range(0, num_pes):
    pe = createCorePE(noc=root.noc,
                      options=options,
                      no=i,
                      cmdline=cmd_list[i],
                      memPE=mem_pe,
                      l1size='32kB',
                      l2size='256kB',
                      dtupos=0,
                      mmu=0)
    pes.append(pe)

# create accelerator PEs
options.cpu_clock = '800MHz'
pe = createCorePE(noc=root.noc,
                  options=options,
                  no=num_pes,
                  cmdline=cmd_list[num_pes - 1],
                  memPE=mem_pe,
                  spmsize='16MB',
                  dtupos=0,
                  mmu=0)
pes.append(pe)

options.cpu_clock = '200MHz'
for i in range(0, num_fft):
    pe = createAccelPE(noc=root.noc,
                       options=options,
                       no=num_pes + 1 + i,
                       accel='fft',
                       memPE=mem_pe,
                       spmsize='128kB')
                       #l1size='32kB')
    pes.append(pe)

# create the memory PEs
for i in range(0, num_mem):
    pe = createMemPE(noc=root.noc,
                     options=options,
                     no=num_pes + 1 + num_fft + i,
                     size='2048MB',
                     image=fsimg if i == 0 else None,
                     imageNum=int(fsimgnum))
    pes.append(pe)

runSimulation(root, options, pes)
