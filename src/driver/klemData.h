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
#ifndef KLEM_DATA_INCLUDE

#include <linux/wait.h>

#define KLEM_LOG(fmt, args...) printk("klem::%s "fmt, __func__, args)
#define KLEM_MSG(fmt) printk("klem::%s "fmt, __func__)

#define MAX_WIRELESS_NODE 256
#define MAX_DEVICE_NAME 64

typedef struct klem_data {
  /* Keep track of our version number. */
  unsigned int uiVersion;

  /* The networking device to attach. */
  char pDevName [MAX_DEVICE_NAME];

  /* The Node Number of the Link Emulation. */
  unsigned char ubNode;

  /* Need a lock for our structure data */
  spinlock_t sLock;

  /* Information for the proc interface */
  struct {
    char pBuffer [4096];
    int iSize;
    struct proc_dir_entry *pEntry;
  } proc;

  /* Netlink socket */
  void *pNetLink;

  /* Raw Socket information */
  void *pRawSocket;

  void *pCtrlQueue;

  /* Keep pointer to our only mac radio.  */
  void *pMacData;

  /* What is our id */
  unsigned int uDeviceId;

  /* 
   * keep track of our filter.  This doesn't scale, consider
   * a list/hash in future.
  */
  bool bFilterNode [MAX_WIRELESS_NODE];

  /*
   * Specificy if the driver is a virtual wireless / lemu
   * or a bridge wireless connection
   */
  enum {
    LEMU,
    BRIDGE,
  } eMode;
  /* remember the class */
  struct class *pClass;
} KLEMData;

void *klemDataInit(void);
void klemDataDeInit(void);
void *klemGetData(void);
#endif
