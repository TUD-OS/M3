/**
 * $Id$
 * Copyright (C) 2008 - 2014 Nils Asmussen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * Modifications in 2017 by Lukas Landgraf, llandgraf317@gmail.com
 * This file is copied and modified from Escape OS.
 * Currently this program only works as an example and should be further modified to utilize shared
 * memory to copy read data from disk to another userspace program.
 * For now, interrupts and PIO is implemented in M3. DMA remains to be implemented.
 */

#include <m3/com/MemGate.h>
#include <m3/stream/Standard.h>

#include <base/util/Math.h>

#include "custom_types.h"
#include "ata.h"
#include "controller.h"
#include "device.h"
#include "partition.h"

#include <m3/server/RequestHandler.h>
#include <m3/server/Server.h>

#include "Session/DiskSession.h"
#include "Session/DiskSrvSession.h"

#include <base/col/Treap.h>

using namespace m3;

static ulong handleRead(sATADevice *device,sPartition *part,uint16_t *buf,uint offset,uint count);
static ulong handleWrite(sATADevice *device,sPartition *part,uint16_t *buf,uint offset,uint count);

struct CapNode : public TreapNode<CapNode, blockno_t> {
friend class TreapNode;

        CapNode(blockno_t bno, capsel_t mem, size_t len) : TreapNode(bno),_len(len), _mem(mem) {}
        size_t _len;
        capsel_t _mem;

        bool matches(blockno_t bno) {
            return (key() <= bno) && (bno < key() + _len);
        }
};

class DiskRequestHandler;

using base_class = RequestHandler<DiskRequestHandler, DiskSession::Operation, DiskSession::COUNT, DiskSrvSession>;
static Server<DiskRequestHandler> *srv;

class DiskRequestHandler : public base_class {
public:
    explicit DiskRequestHandler(sATADevice *dev)
        : base_class(),
          _rgate(RecvGate::create(nextlog2<32 * DiskSession::MSG_SIZE>::val,
                                  nextlog2<DiskSession::MSG_SIZE>::val)),
          _dev(dev) {
        add_operation(DiskSession::READ, &DiskRequestHandler::read);
        add_operation(DiskSession::WRITE, &DiskRequestHandler::write);

        using std::placeholders::_1;
        _rgate.start(std::bind(&DiskRequestHandler::handle_message, this, _1));
    }

    virtual Errors::Code obtain(DiskSrvSession *sess, KIF::Service::ExchangeData &data) override {
        if(data.args.count != 0 || data.caps != 1)
                return Errors::INV_ARGS;

        return sess->get_sgate(data);
    }

    virtual Errors::Code open(DiskSrvSession **sess, capsel_t srv_sel, word_t) override {
        *sess = new DiskSrvSession(srv_sel, &_rgate);
	return Errors::NONE;
    }


    virtual Errors::Code delegate(DiskSrvSession *sess, KIF::Service::ExchangeData &data) override {
        if(data.args.count != 2 || data.caps != 1) {
            return Errors::NOT_SUP;
        }

        capsel_t sel = VPE::self().alloc_sel();
        data.caps = KIF::CapRngDesc(KIF::CapRngDesc::OBJ, sel, data.caps).value();

        CapNode *node = caps.find(data.args.vals[0]);
        if(node)
                caps.remove(node);
        caps.insert(new CapNode(data.args.vals[0], sel, data.args.vals[1]));
        return Errors::NONE;
    }

    virtual Errors::Code close(DiskSrvSession *sess) override {
        delete sess;
        return Errors::NONE;
    }

    void read(GateIStream &is) {
        blockno_t cap, start;
        size_t len, blocksize;
        goff_t off;
        is >> cap;
        is >> start;
        is >> len;
        is >> blocksize;
        is >> off;

        SLOG(FS, "DISK: Read blocks " << start << ":" << len << " @ " << fmt(off, "x"));
        if(len * blocksize < 512) {
            reply_error(is, Errors::INV_ARGS);
            return;
        }

        Errors::Code res;

        uint16_t *buf = new uint16_t[(30 * blocksize) / 2];
        capsel_t m_cap = caps.find(cap)->_mem;
        if(m_cap != ObjCap::INVALID) {
                MemGate m = MemGate::bind(m_cap);

                //we can only read 255 sectors (<31 blocks) at once (see ata.cc ata_setupCommand)
                size_t i = 0;
                for(; i < (len / 30); i++)  {
                    handleRead(_dev, _dev->partTable, buf, (start + i * 30) * blocksize, 30 * blocksize);
                    res = m.write(buf, 30 * blocksize, (off + i * 30) * blocksize);
                }
                //now read the rest
                if(len % 30) {
                    handleRead(_dev, _dev->partTable, buf, (start + i * 30) * blocksize, (len % 30) * blocksize);
                    res = m.write(buf, (len % 30) * blocksize, (off + i * 30) * blocksize);
                }
        }
        else {
                res = Errors::NO_PERM;
        }
        delete[] buf;

        reply_error(is, res);

    }

    void write(GateIStream &is) {
        blockno_t cap, start;
        size_t len, blocksize;
        goff_t off;
        is >> cap;
        is >> start;
        is >> len;
        is >> blocksize;
        is >> off;

        SLOG(FS, "DISK: Write blocks " << start << ":" << len << " @ " << fmt(off, "x"));

        if(len * blocksize < 512) {
            reply_error(is, Errors::INV_ARGS);
            return;
        }

        Errors::Code res;

        uint16_t *buf = new uint16_t[(30 * blocksize) / 2];

        capsel_t m_cap = caps.find(cap)->_mem;
        if(m_cap != ObjCap::INVALID) {
                MemGate m = MemGate::bind(m_cap);

                //we can only write 255 sectors (<31 blocks) at once (see ata.cc ata_setupCommand)
                size_t i = 0;
                for(; i < (len / 30); i++)  {
                    res = m.read(buf, 30 * blocksize, (off + i * 30) * blocksize);
                    handleWrite(_dev, _dev->partTable, buf, (start + i * 30) * blocksize, 30 * blocksize);
                }
                //now read the rest
                if(len % 30) {
                    res = m.read(buf, (len % 30) * blocksize, (off + i * 30) * blocksize);
                    handleWrite(_dev, _dev->partTable, buf, (start + i * 30) * blocksize, (len % 30) * blocksize);
                }
        }
        else {
                res = Errors::NO_PERM;
        }

        delete[] buf;
        reply_error(is, res);
    }

private:
    RecvGate _rgate;
    sATADevice *_dev;
    Treap<CapNode> caps;
};


/* a partition on the disk */
typedef struct {
	/* Boot indicator bit flag: 0 = no, 0x80 = bootable (or "active") */
	uint8_t bootable;
	/* start: Cylinder, Head, Sector */
	uint8_t startHead;
	uint16_t startSector : 6,
		startCylinder: 10;
	uint8_t systemId;
	/* end: Cylinder, Head, Sector */
	uint8_t endHead;
	uint16_t endSector : 6,
		endCylinder : 10;
	/* Relative Sector (to start of partition -- also equals the partition's starting LBA value) */
	uint32_t start;
	/* Total Sectors in partition */
	uint32_t size;
} PACKED sDiskPart;

static const size_t MAX_RW_SIZE		= 4096;
static const int RETRY_COUNT		= 3;


static void initDrives(void);
class ATAPartitionDevice;

static size_t drvCount = 0;
static ATAPartitionDevice * devs[PARTITION_COUNT * DEVICE_COUNT];
/* don't use dynamic memory here since this may cause trouble with swapping (which we do) */
/* because if the heap hasn't enough memory and we request more when we should swap the kernel
 * may not have more memory and can't do anything about it */
static uint16_t buffer[MAX_RW_SIZE / sizeof(uint16_t)];

class ATAPartitionDevice {
  private:
  	uint32_t id;
  	uint32_t partition;
  	char* accessId;
  	uint16_t mode;

  public:
  	ATAPartitionDevice(uint32_t id, uint32_t partition, char* name, uint16_t mode)
  		: id(id),
  		  partition(partition),
  		  accessId(new char[strlen(name) + 1]),
  		  mode(mode) {
		strcpy(this->accessId, name);
	}
	~ATAPartitionDevice() {
		delete[] accessId;
	}
};

int main(int argc,char **argv) {
	bool useDma = false; // TODO current unsupported
	bool useIRQ = true;

	for(size_t i = 2; (int)i < argc; i++) {
		if(strcmp(argv[i],"nodma") == 0)
			useDma = false;
		else if(strcmp(argv[i],"noirq") == 0)
			useIRQ = false;
	}

	/* detect and init all devices */
	ctrl_init(useDma, useIRQ);
	initDrives();
	/* flush prints */

	sATADevice * ataDev = ctrl_getDevice(0);
	device_print(ataDev, cout);

	/* Example input */
	uint16_t arg[256] = {0xC0FF, 0xEEEE};
	/* Output */
	uint16_t res[256] = {0, 0};

	uint8_t buf[0x1FF];

	part_print(ataDev->partTable);
	uint present = 0;

	for(uint i = 0; i<PARTITION_COUNT; i++) {
		present |= ataDev->partTable[i].present;
	}

	/* Just setup a example partition table, not written or read from disk */
	if(!present) {
		sDiskPart *src = (sDiskPart*) (buf + 0x1BE);
		for(uint i=0; i<PARTITION_COUNT; i++) {
			src[i].systemId = i+1;
			src[i].startSector = 0x20*i;
			src[i].endSector = 0x3F;
			src[i].startHead = i;
			src[i].endHead = i;
			src[i].start = 0x0;
            // this could be too small for bench.img
			src[i].size = 173000000;

		}

		/* write the information to the in-memory partition table */
		part_fillPartitions(ataDev->partTable, (void*) buf);
		part_print(ataDev->partTable);
	}

	// ctrl_deinit();

        const char *name = "disk";
        srv = new Server<DiskRequestHandler>(name, new DiskRequestHandler(ataDev));

        // env()->workloop()->multithreaded(8);
        env()->workloop()->run();

        delete srv;
	return 0;
}

static ulong handleRead(sATADevice *ataDev,sPartition *part,uint16_t *buf,uint offset,uint count) {
	/* we have to check whether it is at least one sector. otherwise ATA can't
	 * handle the request */
	SLOG(IDE_ALL, "" << offset << " + " << count << " <= " << part->size << " * " << ataDev->secSize);
	if(offset + count <= part->size * ataDev->secSize && offset + count > offset) {
		uint rcount = m3::Math::round_up((size_t)count,ataDev->secSize);
		if(buf != buffer || rcount <= MAX_RW_SIZE) {
			int i;
			SLOG(IDE_ALL, "Reading " << rcount << " bytes @ " << offset << " from device " << ataDev->id);
			for(i = 0; i < RETRY_COUNT; i++) {
				if(i > 0)
					SLOG(IDE, "Read failed; retry " << i);
				if(ataDev->rwHandler(ataDev,OP_READ,buf,
						offset / ataDev->secSize + part->start,
						ataDev->secSize,rcount / ataDev->secSize)) {
					return count;
				}
			}
		SLOG(IDE, "Giving up after " << i << " retries");
			return 0;
		}
	}
	SLOG(IDE, "Invalid read-request: offset=" << offset <<", count=" << count
		<< ", partSize=" << part->size * ataDev->secSize << " (device " << ataDev->id << ")");
	return 0;
}

static ulong handleWrite(sATADevice *ataDev,sPartition *part,uint16_t *buf,uint offset,uint count) {
	SLOG(IDE_ALL, "ataDev->secSize: " << ataDev->secSize << ", count: " << count);
	if(offset + count <= part->size * ataDev->secSize && offset + count > offset) {
		if(buf != buffer || count <= MAX_RW_SIZE) {
			int i;
			SLOG(IDE_ALL, "Writing " << count << " bytes @ 0x" << m3::fmt(offset,"x")
				<< " to device " << ataDev->id);
			for(i = 0; i < RETRY_COUNT; i++) {
				if(i > 0)
					SLOG(IDE, "Write failed; retry " << i);
				if(ataDev->rwHandler(ataDev,OP_WRITE,buf,
						offset / ataDev->secSize + part->start,
						ataDev->secSize,count / ataDev->secSize)) {
					return count;
				}
			}
			SLOG(IDE, "Giving up after " << i << " retries");
			return 0;
		}
	}
	SLOG(IDE, "Invalid write-request: offset=0x" << m3::fmt(offset,"x") << ", count=" \
		<< count << ", partSize=" << part->size * ataDev->secSize
		<< " (device " << ataDev->id << ")");
	return 0;
}

static void initDrives(void) {
	uint deviceIds[] = {DEVICE_PRIM_MASTER,DEVICE_PRIM_SLAVE,DEVICE_SEC_MASTER,DEVICE_SEC_SLAVE};
	char name[SSTRLEN("hda1") + 1];
	char path[MAX_PATH_LEN] = "/dev/";
	for(size_t i = 0; i < DEVICE_COUNT; i++) {
		sATADevice *ataDev = ctrl_getDevice(deviceIds[i]);
		if(ataDev->present == 0)
			continue;

		/* register device for every partition */
		for(size_t p = 0; p < PARTITION_COUNT; p++) {
			if(ataDev->partTable[p].present) {
				strcpy(path + SSTRLEN("/dev/"),name);

				devs[drvCount] = new ATAPartitionDevice(ataDev->id,p,path,0770);
				SLOG(IDE, "Registered device '"<< name << "' (device "
					<< ataDev->id << ", partition " << p + 1 << ")");

				drvCount++;
			}
		}
	}
}
