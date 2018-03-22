/*
 * Copyright (C) 2015, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel-based SysteM for Heterogeneous Manycores).
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

#include <base/Common.h>

#include <fs/internal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

FILE *file;
m3::SuperBlock sb;

static void print_sb() {
    printf("Superblock:\n");
    printf("  blocksize: %u\n", sb.blocksize);
    printf("  total_inodes: %u\n", sb.total_inodes);
    printf("  total_blocks: %u\n", sb.total_blocks);
    printf("  free_inodes: %u\n", sb.free_inodes);
    printf("  free_blocks: %u\n", sb.free_blocks);
    printf("  first_free_inode: %u\n", sb.first_free_inode);
    printf("  first_free_block: %u\n", sb.first_free_block);
}

static void print_bitmap(uint32_t total, const m3::Bitmap &bitmap) {
    printf(" %5d..%5d: ", 0, 0 + 63);
    for(uint32_t i = 0; i < total; ++i) {
        if(i > 0 && i % 64 == 0)
            printf("\n %5d..%5d: ", i, i + 63);
        else if(i > 0 && i % 8 == 0)
            printf(" ");
        printf("%d", bitmap.is_set(i) ? 1 : 0);
    }
    printf("\n");
}

static void print_inodebm() {
    m3::Bitmap bitmap(sb.total_inodes);
    read_from_block(bitmap.bytes(), (sb.total_inodes + 7) / 8, sb.first_inodebm_block());

    printf("INode bitmap:\n");
    print_bitmap(sb.total_inodes, bitmap);
}

static void print_blockbm() {
    m3::Bitmap bitmap(sb.total_blocks);
    read_from_block(bitmap.bytes(), (sb.total_blocks + 7) / 8, sb.first_blockbm_block());

    printf("Block bitmap:\n");
    print_bitmap(sb.total_blocks, bitmap);
}

static void print_time(m3::time_t time, const char *name) {
    char timebuf[64];
    time_t unixtime = time;
    struct tm *tm = gmtime(&unixtime);
    strftime(timebuf, sizeof(timebuf), "%F %T", tm);
    printf("  %s: %u (%s)\n", name, time, timebuf);
}

static void print_extents(m3::blockno_t bno, int indent, int depth) {
    m3::Extent *extents = new m3::Extent[sb.extents_per_block()];
    read_from_block(extents, sb.blocksize, bno);

    for(uint i = 0; extents[i].length && i < sb.extents_per_block(); ++i) {
        printf("%*s%3u: %4u .. %4u (%u)\n", indent * 2, "", i, extents[i].start,
            extents[i].length ? extents[i].start + extents[i].length - 1 : 0,
            extents[i].length);
        if(extents[i].start != 0 && depth > 0)
            print_extents(extents[i].start, indent + 1, depth - 1);
    }

    delete[] extents;
}

static void print_inode(m3::inodeno_t ino, bool all) {
    m3::INode inode = read_inode(ino);
    printf("INode %u:\n", ino);
    printf("  devno: %u\n", inode.devno);
    printf("  inode: %u\n", inode.inode);
    printf("  mode: %#04o\n", inode.mode);
    printf("  links: %u\n", inode.links);
    printf("  size: %lu\n", inode.size);
    print_time(inode.lastaccess, "lastaccess");
    print_time(inode.lastmod, "lastmod");
    printf("  extents: %u\n", inode.extents);
    for(int i = 0; i < m3::INODE_DIR_COUNT; ++i) {
        printf("  direct[%d]: %4u .. %4u (%u)\n", i, inode.direct[i].start,
            inode.direct[i].length ? inode.direct[i].start + inode.direct[i].length - 1 : 0,
            inode.direct[i].length);
    }
    printf("  indirect: %u\n", inode.indirect);
    if(all && inode.indirect != 0)
        print_extents(inode.indirect, 2, 0);
    printf("  dindirect: %u\n", inode.dindirect);
    if(all && inode.dindirect != 0)
        print_extents(inode.dindirect, 2, 1);
}

static void print_inodes() {
    m3::Bitmap bitmap(sb.total_inodes);
    read_from_block(bitmap.bytes(), (sb.total_inodes + 7) / 8, sb.first_inodebm_block());

    for(uint32_t i = 0; i < sb.total_inodes; ++i) {
        if(bitmap.is_set(i))
            print_inode(i, false);
    }
}

static void print_as_dir(m3::blockno_t block) {
    char *buffer = new char[sb.blocksize];
    read_from_block(buffer, sb.blocksize, block);

    m3::DirEntry *e = reinterpret_cast<m3::DirEntry*>(buffer);
    m3::DirEntry *end = reinterpret_cast<m3::DirEntry*>(buffer + sb.blocksize);
    printf("Showing block %u as directory:\n", block);
    // actually next is not allowed to be 0. but to prevent endless looping here...
    while(e->next > 0 && e < end) {
        printf("  ino=%u len=%u next=%u name=%.*s\n",
               e->nodeno, e->namelen, e->next, e->namelen, e->name);
        e = reinterpret_cast<m3::DirEntry*>(reinterpret_cast<char*>(e) + e->next);
    }
    delete[] buffer;
}

static void print_as_extents(m3::blockno_t block) {
    m3::Extent *extents = new m3::Extent[sb.extents_per_block()];
    read_from_block(extents, sb.blocksize, block);

    printf("Showing block %u as extents:\n", block);
    for(uint i = 0; i < sb.extents_per_block(); ++i) {
        printf("  %3u: %4u .. %4u (%u)\n", i, extents[i].start,
               extents[i].length ? extents[i].start + extents[i].length - 1 : 0,
               extents[i].length);
    }

    delete[] extents;
}

static void print_block_bytes(size_t off, m3::blockno_t block) {
    printf("Block %u:\n", block);
    uint8_t *buffer = new uint8_t[sb.blocksize];
    read_from_block(buffer, sb.blocksize, block);
    for(size_t i = 0; i < sb.blocksize / 16; ++i) {
        printf("%08zx: ", off + i * 16);
        for(size_t j = 0; j < 8; ++j)
            printf("%02x ", buffer[i * 16 + j]);
        printf(" ");
        for(size_t j = 0; j < 8; ++j)
            printf("%02x ", buffer[i * 16 + j + 8]);
        printf("\n");
    }
    delete[] buffer;
}

static void print_block_text(m3::blockno_t block, size_t count) {
    uint8_t *buffer = new uint8_t[sb.blocksize];
    read_from_block(buffer, sb.blocksize, block);
    for(size_t i = 0; i < count; ++i)
        putchar(buffer[i]);
    delete[] buffer;
}

static void print_ino_bytes(m3::inodeno_t ino) {
    printf("Printing bytes of inode %d:\n", ino);
    m3::INode inode = read_inode(ino);
    size_t blockcount = (inode.size + sb.blocksize - 1) / sb.blocksize;
    for(uint32_t i = 0; i < blockcount; ++i)
        print_block_bytes(i * sb.blocksize, get_block_no(inode, i));
}

static void print_ino_text(m3::inodeno_t ino) {
    m3::INode inode = read_inode(ino);
    size_t blockcount = (inode.size + sb.blocksize - 1) / sb.blocksize;
    size_t count = 0;
    for(uint32_t i = 0; i < blockcount; ++i) {
        size_t amount = i < blockcount - 1 ? sb.blocksize : inode.size - count;
        print_block_text(get_block_no(inode, i), amount);
        count += sb.blocksize;
    }
    putchar('\n');
}

static void print_tree(m3::inodeno_t dirno, const char *path, int level) {
    m3::INode inode = read_inode(dirno);

    if(S_ISDIR(inode.mode)) {
        printf("%*sListing of directory '%s' (%u)\n", level * 2, "", path, dirno);
        char *buffer = new char[sb.blocksize];
        size_t blockcount = (inode.size + sb.blocksize - 1) / sb.blocksize;
        for(uint32_t i = 0; i < blockcount; ++i) {
            read_from_block(buffer, sb.blocksize, get_block_no(inode, i));

            m3::DirEntry *e = reinterpret_cast<m3::DirEntry*>(buffer);
            m3::DirEntry *end = reinterpret_cast<m3::DirEntry*>(buffer + sb.blocksize);
            while(e->next > 0 && e < end) {
                printf("%*sino=%u len=%u next=%u name=%.*s\n",
                    (level + 1) * 2, "", e->nodeno, e->namelen, e->next, e->namelen, e->name);

                if((e->namelen != 1 || strncmp(e->name, ".", 1) != 0) &&
                    (e->namelen != 2 || strncmp(e->name, "..", 2) != 0)) {
                    char epath[128];
                    snprintf(epath, sizeof(epath), "%s/%.*s", path, e->namelen, e->name);
                    print_tree(e->nodeno, epath, level + 1);
                }

                e = reinterpret_cast<m3::DirEntry*>(reinterpret_cast<char*>(e) + e->next);
            }
        }
        delete[] buffer;
    }
}

static void usage(const char *name) {
    fprintf(stderr, "Usage: %s <image> <cmd>\n", name);
    fprintf(stderr, "  Available commands:\n");
    fprintf(stderr, "  sb             - show superblock\n");
    fprintf(stderr, "  ibm            - show inode bitmap\n");
    fprintf(stderr, "  bbm            - show block bitmap\n");
    fprintf(stderr, "  inodes         - show all inodes\n");
    fprintf(stderr, "  tree           - show directory tree\n");
    fprintf(stderr, "  ino <n>        - show inode <n>\n");
    fprintf(stderr, "  inoextents <n> - show inode <n> including extents\n");
    fprintf(stderr, "  inobytes <n>   - show the bytes in inode <n>\n");
    fprintf(stderr, "  inotext <n>    - show inode <n> as text\n");
    fprintf(stderr, "  dir <n>        - show block <n> as directory\n");
    fprintf(stderr, "  extents <n>    - show block <n> as extents\n");
    fprintf(stderr, "  bytes <n>      - show block <n> as bytes\n");
    fprintf(stderr, "  text <n>       - show block <n> as text\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
    if(argc < 3)
        usage(argv[0]);
    if(argc < 4 && (strcmp(argv[2], "ino") == 0 || strcmp(argv[2], "dir") == 0 ||
            strcmp(argv[2], "extents") == 0 || strcmp(argv[2], "inoextents") == 0 ||
            strcmp(argv[2], "inobytes") == 0 || strcmp(argv[2], "inotext") == 0 ||
            strcmp(argv[2], "bytes") == 0 || strcmp(argv[2], "text") == 0))
        usage(argv[0]);

    file = fopen(argv[1], "r");
    if(!file)
        err(1, "Unable to open %s for reading", argv[1]);

    fread(&sb, sizeof(sb), 1, file);

    if(strcmp(argv[2], "sb") == 0)
        print_sb();
    else if(strcmp(argv[2], "ibm") == 0)
        print_inodebm();
    else if(strcmp(argv[2], "bbm") == 0)
        print_blockbm();
    else if(strcmp(argv[2], "inodes") == 0)
        print_inodes();
    else if(strcmp(argv[2], "tree") == 0)
        print_tree(0, "/", 0);
    else if(strcmp(argv[2], "ino") == 0) {
        m3::inodeno_t ino = strtoul(argv[3], nullptr, 0);
        print_inode(ino, false);
    }
    else if(strcmp(argv[2], "inoextents") == 0) {
        m3::inodeno_t ino = strtoul(argv[3], nullptr, 0);
        print_inode(ino, true);
    }
    else if(strcmp(argv[2], "inobytes") == 0) {
        m3::inodeno_t ino = strtoul(argv[3], nullptr, 0);
        print_ino_bytes(ino);
    }
    else if(strcmp(argv[2], "inotext") == 0) {
        m3::inodeno_t ino = strtoul(argv[3], nullptr, 0);
        print_ino_text(ino);
    }
    else if(strcmp(argv[2], "dir") == 0) {
        m3::blockno_t bno = strtoul(argv[3], nullptr, 0);
        print_as_dir(bno);
    }
    else if(strcmp(argv[2], "extents") == 0) {
        m3::blockno_t bno = strtoul(argv[3], nullptr, 0);
        print_as_extents(bno);
    }
    else if(strcmp(argv[2], "bytes") == 0) {
        m3::blockno_t bno = strtoul(argv[3], nullptr, 0);
        print_block_bytes(0, bno);
    }
    else if(strcmp(argv[2], "text") == 0) {
        m3::blockno_t bno = strtoul(argv[3], nullptr, 0);
        print_block_text(bno, sb.blocksize);
    }
    else
        usage(argv[0]);

    fclose(file);
    return 0;
}
