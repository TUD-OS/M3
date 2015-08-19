/*
 * Copyright (C) 2013, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel for Minimalist Manycores).
 *
 * M3 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * M3 is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#define FS_TOOLS

#include <m3/Common.h>
#include <m3/BitField.h>
#include <fs/internal.h>
#include <cstdarg>
#include <err.h>

FILE *file;
m3::SuperBlock sb;
static int exitcode = 0;

static void set_inode(m3::Bitmap &inodes, m3::inodeno_t ino) {
    inodes.set(ino);
}

static void set_block(m3::Bitmap &blocks, m3::blockno_t no) {
    if(blocks.is_set(no))
        errx(1, "Block number %u is used (at least) twice\n", no);
    blocks.set(no);
}

static void collect_blocks_and_inodes(m3::inodeno_t ino, m3::Bitmap &blocks, m3::Bitmap &inodes) {
    if(inodes.is_set(ino))
        return;

    m3::INode inode = read_inode(ino);
    set_inode(inodes, ino);

    if(inode.inode != ino)
        errx(1, "Inode %u says that its inode-number is %u\n", ino, inode.inode);

    uint32_t block_count = (inode.size + sb.blocksize - 1) / sb.blocksize;
    if(S_ISDIR(inode.mode)) {
        char *buffer = new char[sb.blocksize];
        for(uint32_t i = 0; i < block_count; ++i) {
            m3::blockno_t block = get_block_no(inode, i);
            if(block == 0) {
                errx(1, "Inode %u has %u blocks, but %u can't be found in extents\n",
                    ino, block_count, i);
                break;
            }

            read_from_block(buffer, sb.blocksize, block);
            set_block(blocks, block);

            m3::DirEntry *e = reinterpret_cast<m3::DirEntry*>(buffer);
            m3::DirEntry *end = reinterpret_cast<m3::DirEntry*>(buffer + sb.blocksize);
            // actually next is not allowed to be 0. but to prevent endless looping here...
            while(e->next > 0 && e < end) {
                if(!(e->namelen == 1 && strncmp(e->name, ".", 1) == 0) &&
                    !(e->namelen == 2 && strncmp(e->name, "..", 2) == 0))
                    collect_blocks_and_inodes(e->nodeno, blocks, inodes);
                e = reinterpret_cast<m3::DirEntry*>(reinterpret_cast<char*>(e) + e->next);
            }
        }
        delete[] buffer;
    }
    else {
        for(uint32_t i = 0; i < block_count; ++i) {
            m3::blockno_t block = get_block_no(inode, i);
            if(block == 0) {
                errx(1, "Inode %u has %u blocks, but %u can't be found in extents\n",
                    ino, block_count, i);
                break;
            }
            set_block(blocks, block);
        }
    }

    if(inode.extents > m3::INODE_DIR_COUNT) {
        if(inode.indirect == 0)
            errx(1, "Inode %u has %u extents, but indirect pointer is 0\n", ino, inode.extents);
        else
            set_block(blocks, inode.indirect);
    }
    else if(inode.indirect != 0)
        errx(1, "Inode %u has %u extents, but indirect pointer is NOT 0\n", ino, inode.extents);

    if(inode.extents > m3::INODE_DIR_COUNT + sb.extents_per_block()) {
        if(inode.dindirect == 0)
            errx(1, "Inode %u has %u extents, but double-indirect pointer is 0\n", ino, inode.extents);
        else {
            uint32_t count = inode.extents - (m3::INODE_DIR_COUNT + sb.extents_per_block());
            count = (count + sb.extents_per_block() - 1) / sb.extents_per_block();
            set_block(blocks, inode.dindirect);

            m3::Extent *extents = new m3::Extent[sb.extents_per_block()];
            read_from_block(extents, sb.blocksize, inode.dindirect);

            for(uint i = 0; i < sb.extents_per_block(); ++i) {
                if(i < count && (extents[i].length == 0 || extents[i].start == 0)) {
                    errx(1, "Inode %u has %u extents, but extent %u is empty\n", ino, inode.extents,
                        i + m3::INODE_DIR_COUNT + sb.extents_per_block());
                }
                if(i >= count && (extents[i].length != 0 || extents[i].start != 0)) {
                    errx(1, "Inode %u has %u extents, but extent %u is NOT empty\n", ino, inode.extents,
                        i + m3::INODE_DIR_COUNT + sb.extents_per_block());
                }

                if(extents[i].start && extents[i].length) {
                    if(extents[i].length != 1) {
                        errx(1, "Double-indirect entry %u of inode %u has a length of %u instead of 1\n",
                            i, ino, extents[i].length);
                    }
                    set_block(blocks, extents[i].start);
                }
            }
            delete[] extents;
        }
    }
    else if(inode.dindirect != 0)
        errx(1, "Inode %u has %u extents, but double-indirect pointer is NOT 0\n", ino, inode.extents);
}

static void compare_bitmaps(const char *name, const m3::Bitmap &used, const m3::Bitmap &marked, uint bits) {
    for(uint i = 0; i < bits; ++i) {
        if(used.is_set(i) != marked.is_set(i)) {
            if(used.is_set(i))
                errx(1, "%s %u is in use, but NOT marked used\n", name, i);
            else
                errx(1, "%s %u is NOT in use, but marked used\n", name, i);
        }
    }
}

static void check_bitmap(const char *name, const m3::Bitmap &used, uint32_t total, uint32_t free,
        m3::blockno_t first) {
    m3::Bitmap bm(total);
    read_from_block(bm.bytes(), (total + 7) / 8, first);

    uint32_t count = 0;
    for(uint32_t i = 0; i < total; ++i) {
        if(!bm.is_set(i))
            count++;
    }
    if(count != free)
        errx(1, "Superblock says %u free blocks, but block bitmap has %u free blocks\n", free, count);

    compare_bitmaps(name, used, bm, total);
}

static void usage(const char *name) {
    fprintf(stderr, "Usage: %s <image>\n", name);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
    if(argc != 2)
        usage(argv[0]);

    file = fopen(argv[1], "r");
    if(!file)
        err(1, "Unable to open %s for reading", argv[1]);

    fread(&sb, sizeof(sb), 1, file);

    if(sb.checksum != sb.get_checksum()) {
        errx(1, "Superblock checksum is invalid (is %#010x, should be %#010x)\n",
            sb.checksum, sb.get_checksum());
    }
    if(sb.total_blocks == 0 || sb.total_inodes == 0)
        errx(1, "Superblock is invalid (no blocks or inodes)\n");
    if(sb.blocksize == 0 || (sb.blocksize & (sb.blocksize - 1)) != 0)
        errx(1, "Blocksize is no power of 2\n");
    // don't continue if the superblock is bogus
    if(exitcode != 0)
        return exitcode;

    if(sb.free_blocks > sb.total_blocks)
        errx(1, "Free blocks is larger than total blocks\n");
    if(sb.free_inodes > sb.total_inodes)
        errx(1, "Free inodes is larger than total inodes\n");

    m3::Bitmap blocks(sb.total_blocks);
    m3::Bitmap inodes(sb.total_inodes);

    // mark superblock, inode-bitmap and block-bitmap used
    for(m3::blockno_t bno = 0; bno < sb.first_data_block(); ++bno)
        blocks.set(bno);

    // collect all inode and block numbers from the directory tree
    collect_blocks_and_inodes(0, blocks, inodes);

    // now check if the bitmaps match
    check_bitmap("INode", inodes, sb.total_inodes, sb.free_inodes, sb.first_inodebm_block());
    check_bitmap("Block", blocks, sb.total_blocks, sb.free_blocks, sb.first_blockbm_block());

    uint32_t first;
    if(sb.first_free_inode > (first = first_free(inodes, sb.total_inodes))) {
        errx(1, "First free inode number in superblock is wrong (is %u, should be at most %u)",
            sb.first_free_inode, first);
    }
    if(sb.first_free_block > (first = first_free(blocks, sb.total_blocks))) {
        errx(1, "First free block number in superblock is wrong (is %u, should be at most %u)",
            sb.first_free_block, first);
    }

    fclose(file);
    return exitcode;
}
