/*
 * Wireless Kernel Link Emulator
 *
 * Copyright (C) 2013 Stuart Wells <swells@stuartwells.net>
 * All rights reserved.
 *
 * Licensed under the GNU General Public License, version 2 (GPLv2)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef KLEM_NET_INCLUDE
void *klemNetConnect(void *pPtr, char *pDevLabel);
void klemNetDisconnect(void *pPtr);
unsigned int klemTransmit(void *pPtr, 
			  struct sk_buff *pSkb,
			  char *pHdr, unsigned int uHdrSize);
#endif
