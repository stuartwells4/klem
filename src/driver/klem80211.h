/*
 * Wireless Kernel Link Emulator
 *
 * Copyright (C) 2013 Stuart Wells <swells@stuartwells.net> All rights reserved.
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

#ifndef KLEM80211_INCLUDE
#define KLEM80211_INCLUDE
#include <linux/version.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,11,0))
unsigned int klem80211Proc(void *pPtr, char *pOutput);
#else
void klem80211Proc(void *pPtr, struct seq_file *pOutput);
#endif

void klem80211Recv(void *pPtr, struct sk_buff *pSkb);
void klem80211Start(void *pPtr);
void klem80211Stop(void *pPtr);
#endif
