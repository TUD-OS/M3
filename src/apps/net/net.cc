/*
 * Copyright (C) 2017, Georg Kotheimer <georg.kotheimer@mailbox.tu-dresden.de>
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
#include <base/log/Services.h>
#include <base/util/Profile.h>
#include <base/Panic.h>

#include <m3/server/RequestHandler.h>
#include <m3/server/Server.h>
#include <m3/stream/Standard.h>
#include <m3/vfs/VFS.h>
#include <pci/Device.h>
#include <thread/ThreadManager.h>

#include "driver/e1000dev.h"

#include "lwipopts.h"
#include "lwip/sys.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"
#include "lwip/prot/ethernet.h"
#include "lwip/etharp.h"
#include "lwip/timeouts.h"
#include "lwip/udp.h"
#include "lwip/tcp.h"
#include "lwip/raw.h"

#include <assert.h>
#include <m3/session/NetworkManager.h>
#include <m3/session/ServerSession.h>
#include <queue>
#include <cstring>
#include <memory>

using namespace net;
using namespace m3;

static constexpr size_t MAX_SEND_RECEIVE_BATCH_SIZE = 5;

class NMSession : public ServerSession {
public:
    static constexpr size_t MAX_SOCKETS = 16;

    struct Socket {
        class SocketWorkItem: public WorkItem {
        public:
            explicit SocketWorkItem(Socket *socket)
                : _socket(socket) {
            }

            virtual void work() override {
                size_t maxSendCount = MAX_SEND_RECEIVE_BATCH_SIZE;
                while(maxSendCount--) {
                    uint8_t buf[MessageHeader::serialize_length()];
                    MessageHeader hdr;
                    if(_socket->send_pipe->reader()->read(buf, sizeof(buf), false) == -1)
                        return;

                    Unmarshaller um(buf, sizeof(buf));
                    hdr.unserialize(um);
                    ip_addr_t addr =  IPADDR4_INIT(lwip_htonl(hdr.addr.addr()));

                    SLOG(NET, "SocketWorkItem:work(): port " << hdr.port);
                    SLOG(NET, "SocketWorkItem:work(): size " << hdr.size);

                    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, hdr.size, PBUF_RAM);
                    if(p) {
                       ssize_t read_size = _socket->send_pipe->reader()->read(p->payload, hdr.size, false);
                       if(read_size != -1) {
                           assert(static_cast<ssize_t>(hdr.size) == read_size);
                           err_t err;
                           if(ip_addr_cmp(&addr, IP_ADDR_ANY))
                               err = udp_send(_socket->pcb.udp, p);
                           else
                               err = udp_sendto(_socket->pcb.udp, p, &addr, hdr.port);
                           if(err != ERR_OK)
                               SLOG(NET, "SocketWorkItem:work(): udp_send failed: " << err);
                       }
                       else
                           SLOG(NET, "SocketWorkItem:work(): failed to read message data");
                       pbuf_free(p);
                   }
                   else {
                       SLOG(NET, "SocketWorkItem:work(): failed to allocate pbuf, dropping udp packet");
                       _socket->send_pipe->reader()->read(nullptr, hdr.size, false);
                   }
                }
            }

        protected:
            Socket *_socket;
        };

        explicit Socket(NetworkManager::SocketType _type, struct ip_pcb *_pcb, int _sd)
            : type(_type),
              sd(_sd),
              pipe_caps(ObjCap::INVALID),
              recv_pipe(nullptr),
              send_pipe(nullptr),
              work_item(nullptr) {
            pcb.ip = _pcb;
        }

        ~Socket() {
            if(work_item) {
                env()->workloop()->remove(work_item);
                delete work_item;
                work_item = nullptr;
            }
            delete recv_pipe;
            recv_pipe = nullptr;
            delete send_pipe;
            send_pipe = nullptr;
        }

        NetworkManager::SocketType type;
        union {
            struct ip_pcb *ip;
            struct tcp_pcb *tcp;
            struct udp_pcb *udp;
            struct raw_pcb *raw;
        } pcb;
        int sd;
        capsel_t pipe_caps;
        NetDirectPipe *recv_pipe;
        NetDirectPipe *send_pipe;
        SocketWorkItem *work_item;
    };

    explicit NMSession(capsel_t srv_sel)
        : ServerSession(srv_sel),
          sgate(),
          sockets() {
    }

    ~NMSession() {
        for(size_t i = 0; i < MAX_SOCKETS; i++)
            release_sd(i);
    }

    Socket *get(int sd) {
        if(sd >= 0 && static_cast<size_t>(sd) < MAX_SOCKETS)
            return sockets[sd];
        return nullptr;
    }

    int request_sd(NetworkManager::SocketType type, struct ip_pcb *pcb) {
        for(size_t i = 0; i < MAX_SOCKETS; i++) {
            if(sockets[i] == nullptr) {
                sockets[i] = new Socket(type, pcb, static_cast<int>(i));
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    void release_sd(int sd) {
        if(sd >= 0 && static_cast<size_t>(sd) < MAX_SOCKETS && sockets[sd] != nullptr) {
            // TODO: Free lwip resources?
            delete sockets[sd];
            sockets[sd] = nullptr;
        }
    }

    SendGate *sgate;
private:
    Socket *sockets[MAX_SOCKETS];
};

class NMRequestHandler;
using net_reqh_base_t = RequestHandler<
    NMRequestHandler, NetworkManager::Operation, NetworkManager::COUNT, NMSession
>;

#define LOG_SESSION(msg)  SLOG(NET, fmt((word_t)sess, "#x") << ":" << msg)

class NMRequestHandler: public net_reqh_base_t {
public:
    static constexpr size_t MAX_SOCKET_BACKLOG = 10;
    static constexpr size_t MSG_SIZE = 128;

    explicit NMRequestHandler()
        : net_reqh_base_t(),
          _rgate(RecvGate::create(nextlog2<32 * MSG_SIZE>::val, nextlog2<MSG_SIZE>::val)) {
        add_operation(NetworkManager::CREATE, &NMRequestHandler::create);
        add_operation(NetworkManager::BIND, &NMRequestHandler::bind);
        add_operation(NetworkManager::LISTEN, &NMRequestHandler::listen);
        add_operation(NetworkManager::CONNECT, &NMRequestHandler::connect);
        add_operation(NetworkManager::CLOSE, &NMRequestHandler::close);

        using std::placeholders::_1;
        _rgate.start(std::bind(&NMRequestHandler::handle_message, this, _1));
    }

    virtual Errors::Code open(NMSession **sess, capsel_t srv_sel, word_t) override {
        *sess = new NMSession(srv_sel);
        return Errors::NONE;
    }

    virtual Errors::Code obtain(NMSession *sess, KIF::Service::ExchangeData &data) override {
        if(!sess->sgate) {
            SLOG(PAGER, fmt((word_t)sess, "#x") << ": mem::get_sgate()");

            label_t label = reinterpret_cast<label_t>(sess);
            sess->sgate = new SendGate(SendGate::create(&_rgate, label, MSG_SIZE));

            data.caps = KIF::CapRngDesc(KIF::CapRngDesc::OBJ, sess->sgate->sel()).value();
            return Errors::NONE;
        }

        if(data.args.count == 1 && data.caps == 6) {
            int sd = static_cast<int>(data.args.vals[0]);
            NMSession::Socket *socket = sess->get(sd);
            if(!socket) {
                LOG_SESSION("handle_obtain failed: invalid socket descriptor");
                return Errors::INV_ARGS;
            }

            if(socket->pipe_caps != ObjCap::INVALID) {
                LOG_SESSION("handle_obtain failed: pipe is already established");
                return Errors::INV_ARGS;
            }

            // 0 - 2: Writer
            // 3 - 5: Reader
            socket->pipe_caps = VPE::self().alloc_sels(6);
            socket->recv_pipe = new NetDirectPipe(socket->pipe_caps, NetDirectPipe::BUFFER_SIZE, true);
            socket->send_pipe = new NetDirectPipe(socket->pipe_caps + 3, NetDirectPipe::BUFFER_SIZE, true);
            socket->work_item = new NMSession::Socket::SocketWorkItem(socket);
            env()->workloop()->add(socket->work_item, false);

            // TODO: pass size as argument
            KIF::CapRngDesc crd(KIF::CapRngDesc::OBJ, socket->pipe_caps, 6);
            data.caps = crd.value();
            data.args.count = 0;
            return Errors::NONE;
        }
        else
            return Errors::INV_ARGS;
    }

    virtual Errors::Code close(NMSession *sess) override {
        delete sess;
        return Errors::NONE;
    }

    virtual void shutdown() override {
        _rgate.stop();
    }

    void create(GateIStream &is) {
        NMSession *sess = is.label<NMSession*>();
        NetworkManager::SocketType type;
        uint8_t protocol;
        is >> type >> protocol;
        LOG_SESSION("net::create(type=" << type << ", protocol=" << protocol << ")");

        void *pcb = nullptr;
        switch(type) {
            case NetworkManager::SOCK_STREAM:
                if(protocol == 0 || protocol != IP_PROTO_TCP) {
                    LOG_SESSION("create failed: invalid protocol");
                    reply_error(is, Errors::INV_ARGS);
                    return;
                }
                pcb = tcp_new();
                break;
            case NetworkManager::SOCK_DGRAM:
                if(protocol != 0 && protocol != IP_PROTO_UDP) {
                    LOG_SESSION("create failed: invalid protocol");
                    reply_error(is, Errors::INV_ARGS);
                    return;
                }
                pcb = udp_new();
                break;
            case NetworkManager::SOCK_RAW:
                // TODO: Validate protocol
                pcb = raw_new(protocol);
                break;
            default:
                LOG_SESSION("create failed: invalid socket type");
                reply_error(is, Errors::INV_ARGS);
                return;
        }

        if(!pcb) {
            LOG_SESSION("create failed: allocation of pcb failed");
            reply_error(is, Errors::NO_SPACE);
            return;
        }

        // allocate new socket descriptor
        int sd = sess->request_sd(type, static_cast<struct ip_pcb*>(pcb));
        if(sd == -1) {
            LOG_SESSION("create failed: maximum number of sockets reached");
            reply_error(is, Errors::NO_SPACE);
            return;
        }

        // set argument for callback functions
        NMSession::Socket *socket = sess->get(sd);
        if(socket->type == NetworkManager::SOCK_STREAM) {
            // TODO: implement tcp_error, ... callback
            tcp_arg(socket->pcb.tcp, socket);
        }
        else if(socket->type == NetworkManager::SOCK_DGRAM)
            udp_recv(socket->pcb.udp, udp_recv_cb, socket);

        LOG_SESSION("-> sd=" << sd);
        reply_vmsg(is, Errors::NONE, sd);
    }

    void bind(GateIStream &is) {
        NMSession *sess = is.label<NMSession*>();
        int sd;
        uint32_t addr;
        uint16_t port;
        is >> sd >> addr >> port;
        ip4_addr ip_addr = IPADDR4_INIT(lwip_htonl(addr));
        LOG_SESSION("net::bind(sd=" << sd << ", addr=" << ip4addr_ntoa(&ip_addr) << ", port=" << port << ")");

        NMSession::Socket *socket = sess->get(sd);
        if(!socket) {
            LOG_SESSION("bind failed: invalid socket descriptor");
            reply_error(is, Errors::INV_ARGS);
            return;
        }

        err_t err;
        switch(socket->type) {
            case NetworkManager::SOCK_STREAM:
                err = tcp_bind(socket->pcb.tcp, &ip_addr, port);
                break;
            case NetworkManager::SOCK_DGRAM:
                err = udp_bind(socket->pcb.udp, &ip_addr, port);
                break;
            case NetworkManager::SOCK_RAW:
                LOG_SESSION("bind failed: you can not bind a raw socket");
                reply_error(is, Errors::INV_ARGS);
                return;
        }

        if(err != ERR_OK)
            LOG_SESSION("bind failed: " << errToStr(err));

        reply_error(is, mapError(err));
    }

    void listen(GateIStream &is) {
        NMSession *sess = is.label<NMSession*>();
        int sd;
        is >> sd;
        LOG_SESSION("net::listen(sd=" << sd << ")");

        NMSession::Socket *socket = sess->get(sd);
        if(!socket) {
            LOG_SESSION("listen failed: invalid socket descriptor");
            reply_error(is, Errors::INV_ARGS);
            return;
        }

        if(socket->type == NetworkManager::SOCK_STREAM) {
            err_t err = ERR_OK;
            struct tcp_pcb *lpcb = tcp_listen_with_backlog_and_err(
                socket->pcb.tcp, MAX_SOCKET_BACKLOG, &err);
            if(lpcb)
                socket->pcb.tcp = lpcb;

            if(err != ERR_OK)
                LOG_SESSION("listen failed: " << errToStr(err));

            reply_error(is, mapError(err));
        }
        else {
            LOG_SESSION("listen failed: not a stream socket");
            reply_error(is, Errors::INV_ARGS);
        }
    }

    void connect(GateIStream &is) {
        NMSession *sess = is.label<NMSession*>();
        int sd;
        uint32_t addr;
        uint16_t port;
        is >> sd >> addr >> port;
        ip4_addr ip_addr = IPADDR4_INIT(lwip_htonl(addr));
        LOG_SESSION("net::connect(sd=" << sd << ", addr=" << ip4addr_ntoa(&ip_addr)
            << ", port=" << port << ")");

        NMSession::Socket *socket = sess->get(sd);
        if(!socket) {
            LOG_SESSION("connect failed: invalid socket descriptor");
            reply_error(is, Errors::INV_ARGS);
            return;
        }

        err_t err;
        switch(socket->type) {
            case NetworkManager::SOCK_STREAM:
                // TODO: implement tcp_connect callback
                // err = tcp_connect(socket->pcb.tcp, &ip_addr, port, );
                reply_error(is, Errors::NOT_SUP);
                return;
            case NetworkManager::SOCK_DGRAM:
                err = udp_connect(socket->pcb.udp, &ip_addr, port);
                break;
            case NetworkManager::SOCK_RAW:
                LOG_SESSION("connect failed: you can not connect a raw socket");
                reply_error(is, Errors::INV_ARGS);
                return;
        }

        if(err != ERR_OK)
            LOG_SESSION("connect failed: " << errToStr(err));

        reply_error(is, mapError(err));
    }

    void close(GateIStream &is) {
        NMSession *sess = is.label<NMSession*>();
        int sd;
        is >> sd;
        LOG_SESSION("net::close(sd=" << sd << ")");

        NMSession::Socket *socket = sess->get(sd);
        if(!socket) {
            LOG_SESSION("close failed: invalid socket descriptor");
            reply_error(is, Errors::INV_ARGS);
            return;
        }

        err_t err = ERR_OK;
        switch(socket->type) {
            case NetworkManager::SOCK_STREAM:
                err = tcp_close(socket->pcb.tcp);
                break;
            case NetworkManager::SOCK_DGRAM:
                udp_remove(socket->pcb.udp);
                break;
            case NetworkManager::SOCK_RAW:
                raw_remove(socket->pcb.raw);
                break;
        }

        sess->release_sd(sd);

        if(err != ERR_OK)
            LOG_SESSION("close failed: " << errToStr(err));

        reply_error(is, mapError(err));
    }

private:
    static void udp_recv_cb(void *arg, struct udp_pcb*, struct pbuf *p, const ip_addr_t *addr, u16_t port) {
        // TODO: avoid unnecessary copy operation
        auto socket = static_cast<NMSession::Socket *>(arg);

        size_t size = p->tot_len + MessageHeader::serialize_length();
        SLOG(NET, "udp_recv_cb: size " << size);
        SLOG(NET, "udp_recv_cb: offset " << MessageHeader::serialize_length());
        u8_t *buf = static_cast<u8_t*>(Heap::alloc(size));
        Marshaller m(buf, MessageHeader::serialize_length());
        MessageHeader hdr(IpAddr(lwip_ntohl(addr->addr)), port, p->tot_len);
        hdr.serialize(m);
        pbuf_copy_partial(p, buf + MessageHeader::serialize_length(), p->tot_len, 0);
        SLOG(NET, "udp_recv_cb: forwarding data to user (" << p->tot_len << ")");
        if(socket->recv_pipe->writer()->write(buf, size, false) == -1)
            SLOG(NET, "udp_recv_cb: recv_pipe is full, dropping datagram");
        Heap::free(buf);
        pbuf_free(p);
    }

    static err_t errToStr(err_t err) {
        return err;
    }

    static Errors::Code mapError(err_t err) {
        switch(err) {
            case ERR_OK:
                return Errors::NONE;
            case ERR_MEM:
            case ERR_BUF:
                return Errors::OUT_OF_MEM;
            case ERR_VAL:
                return Errors::INV_ARGS;
            case ERR_USE:
                return Errors::IN_USE;
            default:
                return Errors::INV_STATE;
        }
    }

    RecvGate _rgate;
};

static std::queue<struct pbuf*> recvQueue;

static void eth_recv_callback(uint8_t *pkt, size_t size) {
    /* Allocate pbuf from pool */
    struct pbuf *p = pbuf_alloc(PBUF_RAW, size, PBUF_POOL);

    if(p != NULL) {
        /* Copy ethernet frame into pbuf */
        pbuf_take(p, pkt, size);

        /* Put in a queue which is processed in main loop */
        recvQueue.push(p);
        //if(!recvQueue.push(p)) {
        //  /* queue is full -> packet loss */
        //  pbuf_free(p);
        //}
    }
}

static err_t netif_output(struct netif *netif, struct pbuf *p) {
    LINK_STATS_INC(link.xmit);

    SLOG(NET, "netif_output with size " << p->len);

    uint8_t *pkt = (uint8_t*)malloc(p->tot_len);
    if(!pkt) {
        SLOG(NET, "Not enough memory to read packet");
        return ERR_MEM;
    }

    pbuf_copy_partial(p, pkt, p->tot_len, 0);
    /* Start MAC transmit here */
    E1000 *e1000 = static_cast<E1000*>(netif->state);
    if(!e1000->send(pkt, p->tot_len)) {
        free(pkt);
        SLOG(NET, "netif_output failed!");
        return ERR_IF;
    }

    free(pkt);
    return ERR_OK;
}

static void netif_status_callback(struct netif *netif) {
    SLOG(NET, "netif status changed " << ipaddr_ntoa(netif_ip4_addr(netif))
        << " to " << ((netif->flags & NETIF_FLAG_UP) ? "up" : "down"));
}

static err_t netif_init(struct netif *netif) {
    E1000 *e1000 = static_cast<E1000*>(netif->state);

    netif->linkoutput = netif_output;
    netif->output = etharp_output;
    // netif->output_ip6 = ethip6_output;
    netif->mtu = 1500;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET |
                   NETIF_FLAG_IGMP | NETIF_FLAG_MLD6;

    m3::net::MAC mac = e1000->readMAC();
    static_assert(m3::net::MAC::LEN == sizeof(netif->hwaddr), "mac address size mismatch");
    SMEMCPY(netif->hwaddr, mac.bytes(), m3::net::MAC::LEN);
    netif->hwaddr_len = sizeof(netif->hwaddr);

    return ERR_OK;
}

static bool link_state_changed(struct netif *netif) {
    return static_cast<E1000 *>(netif->state)->linkStateChanged();
}

static bool link_is_up(struct netif *netif) {
    return static_cast<E1000 *>(netif->state)->linkIsUp();
}

int main(int argc, char **argv) {
    if(argc != 4)
        exitmsg("Usage: " << argv[0] << " <name> <ip address> <netmask>");

    ip_addr_t ip;
    if(!ipaddr_aton(argv[2], &ip))
        exitmsg(argv[2] << " is not a well formed ip address.");
    else
        SLOG(NET, "ip: " << ipaddr_ntoa(&ip));

    ip_addr_t netmask;
    if(!ipaddr_aton(argv[3], &netmask))
        exitmsg(argv[3] << " is not a well formed netmask.");
    else SLOG(NET, "netmask: " << ipaddr_ntoa(&netmask));

    struct netif netif;

    pci::ProxiedPciDevice nic("nic", m3::PEISA::NIC);
    E1000 e1000(nic);
    e1000.setReceiveCallback(&eth_recv_callback);

    lwip_init();

    netif_add(&netif, &ip, &netmask, IP4_ADDR_ANY, &e1000, netif_init, netif_input);
    netif.name[0] = 'e';
    netif.name[1] = '0';
    // netif_create_ip6_linklocal_address(&netif, 1);
    // netif.ip6_autoconfig_enabled = 1;
    netif_set_status_callback(&netif, netif_status_callback);
    netif_set_default(&netif);
    netif_set_up(&netif);

    /* Start DHCP */
    // dhcp_start(&netif );
    Server<NMRequestHandler> srv(argv[1], new NMRequestHandler());

    while(1) {
        /* Check link state, e.g. via MDIO communication with PHY */
        if(link_state_changed(&netif)) {
            if(link_is_up(&netif))
                netif_set_link_up(&netif);
            else
                netif_set_link_down(&netif);
        }

        /* Check for received frames, feed them to lwIP */
        size_t maxReceiveCount = MAX_SEND_RECEIVE_BATCH_SIZE;
        while(!recvQueue.empty() && maxReceiveCount--) {
            struct pbuf *p = recvQueue.front();
            recvQueue.pop();
            LINK_STATS_INC(link.recv);

            if(netif.input(p, &netif) != ERR_OK)
                pbuf_free(p);
        }

        /* Cyclic lwIP timers check */
        sys_check_timeouts();
        // TODO: Sleep according to sys_timeouts_sleeptime() when !recvQueue.empty() and
        // _socket->send_pipe->reader()->read() has no incoming messages

        // Hack: run the workloop manually
        // - interrupt receive gate
        env()->workloop()->tick();
    }

    return 0;
}
