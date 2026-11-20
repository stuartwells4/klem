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
        pData->pStatusReason = NULL;
        if (NULL == pData->pRawSocket) {
          if (strlen(pData->pDevName) > 0) {
            KLEM_MSG("Start socket\n");
            pData->pRawSocket = klemNetConnect(pData, pData->pDevName);
            KLEM_MSG("Start socket Completed\n");
            if (NULL == pData->pRawSocket) {
              pData->pStatusReason = "cannot open raw socket on device";
            }
          } else {
            KLEM_MSG("Network Device not specified.");
            pData->pStatusReason = "no device set";
          }
        }

        klem80211Start(pData);
        if ((NULL == pData->pMacData) && (NULL == pData->pStatusReason)) {
          pData->pStatusReason = "cannot register radio";
        }

        if (NULL == pData->pStatusReason) {
          pData->eStatus = KLEM_STATUS_RUNNING;
        } else {
          pData->eStatus = KLEM_STATUS_FAILED;
        }
        break;
      case CONNECTION_STOP:
        /*
         * The send thread uses the raw socket and the recv thread
         * uses the radio, so stop sending, then disconnect (which
         * stops recv), and only then free the radio.
         */
        klem80211StopSend(pData);
        if (NULL != pData->pRawSocket) {
          klemNetDisconnect(pData->pRawSocket);
          pData->pRawSocket = NULL;
        } else {
          KLEM_MSG(" Raw Socket never allocated.");
        }
        klem80211Stop(pData);
        pData->eStatus = KLEM_STATUS_STOPPED;
        pData->pStatusReason = NULL;
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
 * Called with pData->sLock held, which protects eStatus.
 */
static void privCtrlQueue(KLEMData *pData, int eCommand)
{
  ctrl_data *pWork = NULL;
  int iRvalue;

  if (NULL == pData->pCtrlQueue) {
    KLEM_MSG("work queue not active\n");
    pData->eStatus = KLEM_STATUS_FAILED;
    pData->pStatusReason = "work queue not active";
    return;
  }

  pWork = (ctrl_data *)kmalloc(sizeof(ctrl_data), GFP_KERNEL);

  if (NULL != pWork) {
    INIT_WORK(&pWork->ctrl_work_hdr, privCtrlProcess);
    pWork->eCommand = eCommand;
    pWork->pData = pData;
    if (CONNECTION_START == eCommand) {
      pData->eStatus = KLEM_STATUS_STARTING;
    } else {
      pData->eStatus = KLEM_STATUS_STOPPING;
    }
    pData->pStatusReason = NULL;
    iRvalue = queue_work(pData->pCtrlQueue, &pWork->ctrl_work_hdr);
    KLEM_LOG("work queued response %d\n", iRvalue);
  } else {
    pData->eStatus = KLEM_STATUS_FAILED;
    pData->pStatusReason = "out of memory queueing command";
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

  /*
   * Same order as CONNECTION_STOP.  The radio is stopped even without a
   * socket, since start brings it up whether or not a device was set.
   */
  klem80211StopSend(pData);
  if (NULL != pData->pRawSocket) {
    klemNetDisconnect(pData->pRawSocket);
    pData->pRawSocket = NULL;
  }
  klem80211Stop(pData);
}
