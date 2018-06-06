import os, sys

sys.path.append(os.path.realpath('hw/gem5/configs/example'))
from dtu_fs import *

options = getOptions()
root = createRoot(options)

num_pes = 1
mem_pe = num_pes
pes = []

for i in range(0, num_pes):
    pe = createAbortTestPE(noc=root.noc,
                           options=options,
                           no=i,
                           memPE=mem_pe,
                           l1size='32kB',
                           l2size='256kB')
    pes.append(pe)

pe = createMemPE(noc=root.noc,
                 options=options,
                 no=num_pes,
                 size='3072MB')

pes.append(pe)

# this is required in order to not occupy the noc xbar for a
# longer amount of time as we need to handle the request on the remote side
root.noc.width = 64

runSimulation(root, options, pes)
