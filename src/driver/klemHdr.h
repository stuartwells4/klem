/*
 * Wireless Kernel Link Emulator
 *
 * Copyright (C) 2013 - 2026 Stuart Wells <swells@stuartwells.net>
 *
 * Licensed under the GNU Lesser General Public License, version 2.1 (LGPLv2.1)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 */
#ifndef KLEM_HDR_INCLUDE
#define KLEM_HDR_INCLUDE

#include <linux/if_ether.h>

#define KLEM_INT_VERSION 1
#define KLEM_STR_VERSION "1"
#define KLEM_NAME "klem"
#define KLEM_CONTROL "klemControl"
#define KLEM_PROTOCOL 0xdead

/*
 * Magic in KLEM_RAW_HEADER.uHeader, sent with htonl.  The value is
 * "klem" read as a little-endian u32, which is what older x86 builds
 * put on the wire, so they still interoperate.
 */
#define KLEM_HEADER_MAGIC 0x6d656c6bU

typedef struct KLEM_RAW_HEADER_DEF {
  u8 pDstMac [ETH_ALEN];
  u8 pSrcMac [ETH_ALEN];
  u16 uProtocol;
  u32 uHeader;
  u32 uVersion;
} __attribute__((packed)) KLEM_RAW_HEADER;

typedef struct KLEM_TAP_HEADER_DEF {
  u32 uBand;
  u32 uFrequency;
  u32 uPower;
  u32 uId;
} __attribute__((packed)) KLEM_TAP_HEADER;


#endif
