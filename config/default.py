import os, sys

sys.path.append(os.path.realpath('hw/gem5/configs/example'))
from dtu_fs import *

options = getOptions()
root = createRoot(options)

cmd_list = options.cmd.split(",")

num_mem = 1
num_pes = int(os.environ.get('M3_GEM5_PES'))
fsimg = os.environ.get('M3_GEM5_FS')
num_spm = 4 if num_pes >= 4 else 4 - num_pes
mem_pe = num_pes

pes = []

# create the core PEs
for i in range(0, num_pes - num_spm):
    pe = createCorePE(root=root,
                      options=options,
                      no=i,
                      cmdline=cmd_list[i],
                      memPE=mem_pe,
                      l1size='64kB',
                      l2size=None)
    pes.append(pe)
for i in range(num_pes - num_spm, num_pes):
    pe = createCorePE(root=root,
                      options=options,
                      no=i,
                      cmdline=cmd_list[i],
                      memPE=mem_pe,
                      spmsize='8MB')
    pes.append(pe)

# create the memory PEs
for i in range(0, num_mem):
    pe = createMemPE(root=root,
                     options=options,
                     no=num_pes + i,
                     size='1024MB',
                     content=fsimg if i == 0 else None)
    pes.append(pe)

pe = createHashAccelPE(root=root,
                   options=options,
                   no=num_pes + num_mem,
                   memPE=mem_pe,
                   spmsize='40kB')
pe.accelhash.buf_size = '8kB'
pes.append(pe)

# pes[1].dtu.watch_range_start  = 0x43d2ff0
# pes[1].dtu.watch_range_end    = 0x43d2fff

runSimulation(options, pes)
