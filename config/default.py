import os, sys

sys.path.append(os.path.realpath('hw/gem5/configs/example'))
from dtu_fs import *

options = getOptions()
root = createRoot(options)

cmd_list = options.cmd.split(",")

num_pes = int(os.environ.get('M3_GEM5_PES'))
fsimg = os.environ.get('M3_GEM5_FS')
num_spm = 4

# create the core PEs
pes = []
for i in range(0, num_pes - num_spm):
    pe = createCorePE(root=root,
                      options=options,
                      no=i,
                      cmdline=cmd_list[i],
                      memPE=num_pes,
                      l1size='64kB',
                      l2size='0')
    pes.append(pe)
for i in range(num_pes - num_spm, num_pes):
    pe = createCorePE(root=root,
                      options=options,
                      no=i,
                      cmdline=cmd_list[i],
                      memPE=num_pes,
                      spmsize='8MB')
    pes.append(pe)

# create the memory PEs
pes.append(createMemPE(root=root,
                       options=options,
                       no=num_pes,
                       size='512MB',
                       content=fsimg))

runSimulation(options, pes)
