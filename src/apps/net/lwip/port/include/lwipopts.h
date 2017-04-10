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

#ifndef LWIP_LWIPOPTS_H
#define LWIP_LWIPOPTS_H

#define NO_SYS 1
#define LWIP_CALLBACK_API 1
#define LWIP_PROVIDE_ERRNO 1

#define PBUF_POOL_SIZE 128

// There is no preemption in m3, so we can disable preemption protection.
#define SYS_LIGHTWEIGHT_PROT 0

#define LWIP_IPV4 1
#define LWIP_IPV6 0
#define LWIP_NETCONN 0
#define LWIP_SOCKET 0
#define LWIP_UDP 1
#define LWIP_TCP 1
#define LWIP_RAW 1

#define TCP_LISTEN_BACKLOG 1
#define LWIP_NETIF_STATUS_CALLBACK 1

// HACK: DirectPipe needs a DTU_PKG_SIZE aligned read buffer...
#define MEM_ALIGNMENT 8u

//#define LWIP_DEBUG
#ifdef LWIP_DEBUG

#define TAPIF_DEBUG      LWIP_DBG_ON
#define TUNIF_DEBUG      LWIP_DBG_ON
#define UNIXIF_DEBUG     LWIP_DBG_ON
#define DELIF_DEBUG      LWIP_DBG_ON
#define SIO_FIFO_DEBUG   LWIP_DBG_ON
#define TCPDUMP_DEBUG    LWIP_DBG_ON
#define API_LIB_DEBUG    LWIP_DBG_ON
#define API_MSG_DEBUG    LWIP_DBG_ON
#define TCPIP_DEBUG      LWIP_DBG_ON
#define NETIF_DEBUG      LWIP_DBG_ON
#define SOCKETS_DEBUG    LWIP_DBG_ON
#define DEMO_DEBUG       LWIP_DBG_ON
#define IP_DEBUG         LWIP_DBG_ON
#define IP_REASS_DEBUG   LWIP_DBG_ON
#define RAW_DEBUG        LWIP_DBG_ON
#define ICMP_DEBUG       LWIP_DBG_ON
#define UDP_DEBUG        LWIP_DBG_ON
#define TCP_DEBUG        LWIP_DBG_ON
#define TCP_INPUT_DEBUG  LWIP_DBG_ON
#define TCP_OUTPUT_DEBUG LWIP_DBG_ON
#define TCP_RTO_DEBUG    LWIP_DBG_ON
#define TCP_CWND_DEBUG   LWIP_DBG_ON
#define TCP_WND_DEBUG    LWIP_DBG_ON
#define TCP_FR_DEBUG     LWIP_DBG_ON
#define TCP_QLEN_DEBUG   LWIP_DBG_ON
#define TCP_RST_DEBUG    LWIP_DBG_ON
#define PBUF_DEBUG       LWIP_DBG_ON

#define LWIP_DBG_TYPES_ON (LWIP_DBG_ON|LWIP_DBG_TRACE|LWIP_DBG_STATE|LWIP_DBG_FRESH|LWIP_DBG_HALT)

#endif


#endif /* LWIP_LWIPOPTS_H */
