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
 * Created by Lukas Landgraf, llandgraf317@gmail.com
 */

#pragma once

// Common PCI offsets
#define PCI_VENDOR_ID 0x00       // Vendor ID                    ro
#define PCI_DEVICE_ID 0x02       // Device ID                    ro
#define PCI_COMMAND 0x04         // Command                      rw
#define PCI_STATUS 0x06          // Status                       rw
#define PCI_REVISION_ID 0x08     // Revision ID                  ro
#define PCI_CLASS_CODE 0x09      // Class Code                   ro
#define PCI_SUB_CLASS_CODE 0x0A  // Sub Class Code               ro
#define PCI_BASE_CLASS_CODE 0x0B // Base Class Code              ro
#define PCI_CACHE_LINE_SIZE 0x0C // Cache Line Size              ro+
#define PCI_LATENCY_TIMER 0x0D   // Latency Timer                ro+
#define PCI_HEADER_TYPE 0x0E     // Header Type                  ro
#define PCI_BIST 0x0F            // Built in self test           rw

// Type 0 PCI offsets
#define PCI0_BASE_ADDR0 0x10    // Base Address 0               rw
#define PCI0_BASE_ADDR1 0x14    // Base Address 1               rw
#define PCI0_BASE_ADDR2 0x18    // Base Address 2               rw
#define PCI0_BASE_ADDR3 0x1C    // Base Address 3               rw
#define PCI0_BASE_ADDR4 0x20    // Base Address 4               rw
#define PCI0_BASE_ADDR5 0x24    // Base Address 5               rw
#define PCI0_CIS 0x28           // CardBus CIS Pointer          ro
#define PCI0_SUB_VENDOR_ID 0x2C // Sub-Vendor ID                ro
#define PCI0_SUB_SYSTEM_ID 0x2E // Sub-System ID                ro
#define PCI0_ROM_BASE_ADDR 0x30 // Expansion ROM Base Address   rw
#define PCI0_CAP_PTR 0x34       // Capability list pointer      ro
#define PCI0_RESERVED 0x35
#define PCI0_INTERRUPT_LINE 0x3C  // Interrupt Line               rw
#define PCI0_INTERRUPT_PIN 0x3D   // Interrupt Pin                ro
#define PCI0_MINIMUM_GRANT 0x3E   // Maximum Grant                ro
#define PCI0_MAXIMUM_LATENCY 0x3F // Maximum Latency              ro
