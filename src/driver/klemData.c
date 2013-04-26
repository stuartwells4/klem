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
#include "klemData.h"
#include "klemHdr.h"

#include <linux/slab.h>
#include <linux/version.h>

static KLEMData *pLocalData = NULL;

void *klemDataInit(void)
{
  KLEMData *pData = NULL;

  pLocalData = kmalloc(sizeof(KLEMData), GFP_KERNEL); 
  pData = pLocalData;
  if (NULL != pData) {
    /* Make sure we set everything to zero */
    memset(pData->pDevName, 0, sizeof(pData->pDevName));
    pData->uiVersion = KLEM_INT_VERSION;
    pData->ubNode = 0;
    pData->proc.pEntry = NULL;
    pData->pNetLink = NULL;
    pData->pRawSocket = NULL;
    pData->pCtrlQueue = NULL;
    pData->pMacData = NULL;
    pData->uDeviceId = 0;
    memset(pData->bFilterNode, false, MAX_WIRELESS_NODE);
    pData->eMode = LEMU;
    pData->pClass = NULL;

    /* We need a spin lock for calls from interrupts to lock data structure */
    spin_lock_init(&pData->sLock);

    KLEM_LOG("malloc internal data at %p\n", pData);
  } else {
    KLEM_MSG("failed to allocate memory\n");
  }
  return (void *)pData;
}

void klemDataDeInit(void)
{
  KLEM_LOG("free memory at %p\n", pLocalData);
  if (NULL != pLocalData) {
    kfree(pLocalData);
    pLocalData = NULL;
  }
}

void *klemGetData(void)
{
  return (void *)pLocalData;
}
