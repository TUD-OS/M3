#!/usr/bin/env python2

import os
import argparse
import subprocess
import sys

# disk geometry (512 * 31 * 63 ~= 1 mb)
secsize = 512
hdheads = 31
hdtracksecs = 63

def create_disk(image, fs, parts, offset):
    if len(parts) == 0:
        exit("Please provide at least one partition")
    if len(parts) > 4:
        exit("Sorry, the maximum number of partitions is currently 4")

    # determine size of disk
    totalmb = 0
    for p in parts:
        totalmb += int(p)
    hdcyl = totalmb
    totalsecs = offset + hdheads * hdtracksecs * hdcyl

    # create image and copy file system into partition
    subprocess.call(["dd", "if=" + fs, "of=" + str(image),
                     "bs=512", "seek=" + str(offset)])

    # zero beginning
    subprocess.call(["dd", "if=/dev/zero" , "of=" + str(image),
                     "bs=512", "conv=notrunc", "count=" + str(offset)])

    tmpfile = subprocess.check_output("mktemp").rstrip()
    lodev = create_loop(image)
    # build command file for fdisk
    with open(tmpfile, "w") as f:
        i = 1
        for p in parts:
            # n = new partition, p = primary, partition number, default offset
            f.write('n\np\n' + str(i) + '\n\n')
            # the last partition gets the remaining sectors
            if i == len(parts):
                f.write('\n')
            # all others get all sectors up to the following partition
            else:
                f.write(str(block_offset(parts, offset, i) * 2 - 1) + '\n')
            # make first partition bootable
            if i == 1:
                f.write('\na\n')
            i += 1
        # write partitions to disk
        f.write('w\n')

    # create partitions with fdisk
    with open(tmpfile, "r") as fin:
        p = subprocess.Popen(
            ["sudo", "fdisk", "-u", "-C", str(hdcyl), "-S", str(hdheads), lodev], stdin=fin
        )
        p.wait()
    free_loop(lodev)

    # remove temp file
    subprocess.call(["rm", "-Rf", tmpfile])

# determines the number of blocks for <mb> MB
def mb_to_blocks(mb):
    return (mb * hdheads * hdtracksecs) / 2

# determines the block offset for partition <no> in <parts>
def block_offset(parts, secoffset, no):
    i = 0
    off = secoffset / 2
    for p in parts:
        if i == no:
            return off
        off += mb_to_blocks(int(p))
        i += 1

# creates a free loop device for <image>, starting at <offset>
def create_loop(image, offset = 0):
    lodev = subprocess.check_output(["sudo", "losetup" , "-f"]).rstrip()
    subprocess.call(["sudo", "losetup", "-o", str(offset), lodev, image])
    return lodev

# frees loop device <lodev>
def free_loop(lodev):
    # sometimes the resource is still busy, so try it a few times
    i = 0
    while i < 10 and subprocess.call(["sudo", "losetup", "-d", lodev]) != 0:
        i += 1

# runs fdisk for <image>
def run_fdisk(image):
    lodev = create_loop(image)
    hdcyl = os.path.getsize(image) / (1024 * 1024)
    subprocess.call(["sudo", "fdisk", "-u", "-C", str(hdcyl), "-S", str(hdheads), lodev])
    free_loop(lodev)

# runs parted for <image>
def run_parted(image):
    lodev = create_loop(image)
    subprocess.call(["sudo", "parted", lodev, "print"])
    free_loop(lodev)

# subcommand functions
def create(args):
    size = subprocess.check_output(["stat", "--format=%s", args.fs]).rstrip()
    create_disk(args.disk, args.fs, [int(size) / (1024 * 1024)], 2048)
def fdisk(args):
    run_fdisk(args.disk)
def parted(args):
    run_parted(args.disk)

# argument handling
parser = argparse.ArgumentParser(description='This is a tool for creating disk images with'
    + ' specified partitions. Additionally, you can mount partitions and analyze the disk'
    + ' with fdisk and parted.')
subparsers = parser.add_subparsers(
    title='subcommands',description='valid subcommands',help='additional help'
)

parser_create = subparsers.add_parser('create', description='Writes a new disk image to <diskimage>'
    + ' with the file system <fs>.')
parser_create.add_argument('disk', metavar='<diskimage>')
parser_create.add_argument('fs', metavar='<fs>')
parser_create.set_defaults(func=create)

parser_fdisk = subparsers.add_parser('fdisk', description='Runs fdisk for <diskimage>.')
parser_fdisk.add_argument('disk', metavar='<diskimage>')
parser_fdisk.set_defaults(func=fdisk)

parser_parted = subparsers.add_parser('parted', description='Runs parted for <diskimage>.')
parser_parted.add_argument('disk', metavar='<diskimage>')
parser_parted.set_defaults(func=parted)

args = parser.parse_args()
args.func(args)
