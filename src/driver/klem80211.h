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

#ifndef KLEM_80211_INCLUDE
#define KLEM_80211_INCLUDE
#include <linux/version.h>

struct seq_file;
struct sk_buff;

void klem80211Proc(void *pPtr, struct seq_file *pOutput);

void klem80211Recv(void *pPtr, struct sk_buff *pSkb);
void klem80211Start(void *pPtr);
void klem80211StopSend(void *pPtr);
void klem80211Stop(void *pPtr);
#endif
