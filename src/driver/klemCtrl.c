/*
 * Wireless Kernel Link Emulator
 *
 * Copyright (C) 2013 - 2016 Stuart Wells <swells@stuartwells.net>
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

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/wait.h>
#include <linux/semaphore.h>
#include <linux/interrupt.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include "klemData.h"
#include "klemNet.h"
#include "klemHdr.h"
#include "klem80211.h"
#include "klemCtrl.h"

#include <linux/workqueue.h>

typedef struct {
  struct work_struct ctrl_work_hdr;
  KLEMData *pData;
  enum {
    CONNECTION_START,
    CONNECTION_STOP,
  } eCommand;
} ctrl_data;

/* Tasklet for disconnect to be outside of interrupt context */
static void privCtrlProcess(struct work_struct *pPtr)
{
  ctrl_data *pWork = container_of(pPtr, ctrl_data, ctrl_work_hdr);
  KLEMData *pData = NULL;

  KLEM_MSG("Entering work queue\n");

  if (NULL != pPtr) {
    pData = pWork->pData;

    /* Lock the data structure */
    mutex_lock(&pData->sLock);

    switch(pWork->eCommand)
      {
      case CONNECTION_START:
        if (NULL == pData->pRawSocket) {
          if (strlen(pData->pDevName) > 0) {
            KLEM_MSG("Start socket\n");
            pData->pRawSocket = klemNetConnect(pData, pData->pDevName);
            KLEM_MSG("Start socket Completed\n");
          } else {
            KLEM_MSG("Network Device not specified.");
          }
        }

        klem80211Start(pData);
        break;
      case CONNECTION_STOP:
        klem80211Stop(pData);
        if (NULL != pData->pRawSocket) {
          klemNetDisconnect(pData->pRawSocket);
          pData->pRawSocket = NULL;
        } else {
          KLEM_MSG(" Raw Socket never allocated.");
        }
        break;
      }

    /* Unlock the data structure */
    mutex_unlock(&pData->sLock);

    /* Free the work structure */
    kfree(pWork);
  }

  KLEM_MSG("Leaving work queue\n");

  return;
}

void klemCtrlCreate(void *pPtr)
{
  KLEMData *pData = (KLEMData *)pPtr;

  pData->pCtrlQueue = (void *)create_workqueue(KLEM_CONTROL);

}

/*
 * Queue a start/stop command onto the control work queue so the
 * (sleeping) socket and radio setup happens outside the caller's context.
 */
static void privCtrlQueue(KLEMData *pData, int eCommand)
{
  ctrl_data *pWork = NULL;
  int iRvalue;

  if (NULL == pData->pCtrlQueue) {
    KLEM_MSG("work queue not active\n");
    return;
  }

  pWork = (ctrl_data *)kmalloc(sizeof(ctrl_data), GFP_KERNEL);

  if (NULL != pWork) {
    INIT_WORK(&pWork->ctrl_work_hdr, privCtrlProcess);
    pWork->eCommand = eCommand;
    pWork->pData = pData;
    iRvalue = queue_work(pData->pCtrlQueue, &pWork->ctrl_work_hdr);
    KLEM_LOG("work queued response %d\n", iRvalue);
  }
}

void klemCtrlStart(void *pPtr)
{
  privCtrlQueue((KLEMData *)pPtr, CONNECTION_START);
}

void klemCtrlStop(void *pPtr)
{
  privCtrlQueue((KLEMData *)pPtr, CONNECTION_STOP);
}

void klemCtrlDestroy(void *pPtr)
{
  KLEMData *pData = (KLEMData *)pPtr;

  if (NULL != pData->pCtrlQueue) {
    flush_workqueue((struct workqueue_struct *)pData->pCtrlQueue);
    destroy_workqueue((struct workqueue_struct *)pData->pCtrlQueue);
    pData->pCtrlQueue = NULL;
  }

  /* Destroy the socket, if its still exists */
  if (NULL != pData->pRawSocket) {
    klem80211Stop(pData);
    klemNetDisconnect(pData->pRawSocket);
    pData->pRawSocket = NULL;
  }
}
