import os, sys
from subprocess import call

sys.path.append(os.path.realpath('hw/gem5/configs/example'))
from dtu_fs import *

options = getOptions()
root = createRoot(options)

cmd_list = options.cmd.split(",")

num_mem = 1
num_sto = 1 # Number of PEs for IDE storage
num_pes = int(os.environ.get('M3_GEM5_PES'))
num_spm = 4 if num_pes >= 4 else 4 - num_pes
mem_pe = num_pes

fsimg = os.environ.get('M3_GEM5_FS')
fsimgnum = os.environ.get('M3_GEM5_FSNUM', '1')

# create disk image
hard_disk0 = os.environ.get('M3_GEM5_IDE_DRIVE')
call(['dd', 'if=/dev/zero', 'of=' + hard_disk0, 'bs=1024', 'count=1024'])

dtupos = int(os.environ.get('M3_GEM5_DTUPOS', 0))
mmu = int(os.environ.get('M3_GEM5_MMU', 0))

pes = []

# create the core PEs
for i in range(0, num_pes - num_spm):
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

for i in range(num_pes - num_spm, num_pes):
    pe = createCorePE(noc=root.noc,
                      options=options,
                      no=i,
                      cmdline=cmd_list[i],
                      memPE=mem_pe,
                      spmsize='16MB')
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

# create the persistent storage PEs
for i in range(0, num_sto):
    pe = createStoragePE(noc=root.noc,
                         options=options,
                         no=num_pes + num_mem + i,
                         memPE=mem_pe,
                         img0=hard_disk0)
    pes.append(pe)

# create ether PEs
ether0 = createEtherPE(noc=root.noc,
                       options=options,
                       no=num_pes + num_mem + num_sto + 0,
                       memPE=mem_pe)
pes.append(ether0)

ether1 = createEtherPE(noc=root.noc,
                       options=options,
                       no=num_pes + num_mem + num_sto + 1,
                       memPE=mem_pe)
pes.append(ether1)

linkEtherPEs(ether0, ether1)

runSimulation(root, options, pes)
