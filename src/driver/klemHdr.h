/*
 * Wireless Kernel Link Emulator
 *
 * Copyright (C) 2013 - 2016 Stuart Wells <swells@stuartwells.net>
 * All rights reserved.
 *
 * This portion of the interface is licensed under LGPL
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef KLEM_PROTOCOL_HDR
#define KLEM_PROTOCOL_HDR

#include <linux/if_ether.h>

#define KLEM_INT_VERSION 1
#define KLEM_STR_VERSION "1"
#define KLEM_NAME "klem"
#define KELM_CONTROL "klemControl"
#define KLEM_PROTOCOL 0xdead

typedef struct KLEM_RAW_HEADER_DEF {
  u8 pDstMac [ETH_ALEN];
  u8 pSrcMac [ETH_ALEN];
  u16 uProtocol;
  u32 uHeader;
  u32 uVersion;
} __attribute__((packed)) KLEM_RAW_HEADER;

typedef struct KELM_DATA_HDR_DEF {
  u32 uBand;
  u32 uFrequency;
  u32 uPower;
  u32 uId;
} __attribute__((packed)) KLEM_TAP_HEADER;


#endif
