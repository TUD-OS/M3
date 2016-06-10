import os, sys

sys.path.append(os.path.realpath('hw/gem5/configs/example'))
from dtu_fs import *

options = getOptions()
root = createRoot(options)

cmd_list = options.cmd.split(",")

num_mem = 1
num_pes = int(os.environ.get('M3_GEM5_PES'))
fsimg = os.environ.get('M3_GEM5_FS')
num_spm = 4
mem_pe = 0

pes = []

# create the memory PEs (have to come first)
for i in range(0, num_mem):
    pes.append(createMemPE(root=root,
                           options=options,
                           no=i,
                           size='1024MB',
                           content=fsimg if i == 0 else None))

# create the core PEs
for i in range(0, num_pes - num_spm):
    pe = createCorePE(root=root,
                      options=options,
                      no=i + num_mem,
                      cmdline=cmd_list[i],
                      memPE=mem_pe,
                      l1size='64kB',
                      l2size='0')
    pes.append(pe)
for i in range(num_pes - num_spm, num_pes):
    pe = createCorePE(root=root,
                      options=options,
                      no=i + num_mem,
                      cmdline=cmd_list[i],
                      memPE=mem_pe,
                      spmsize='8MB')
    pes.append(pe)

# pes[1].dtu.watch_range_start  = 0x43d2ff0
# pes[1].dtu.watch_range_end    = 0x43d2fff

runSimulation(options, pes)
