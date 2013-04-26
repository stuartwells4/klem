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

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/wait.h>
#include <linux/semaphore.h>
#include <linux/interrupt.h>
#include <linux/skbuff.h>
#include "klemData.h"
#include "klemNet.h"
#include "klemHdr.h"
#include "klem80211.h"

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
  ctrl_data *pWork = (ctrl_data *)pPtr;
  KLEMData *pData = NULL;

  KLEM_MSG("Entering work queue\n");

  if (NULL != pWork) {
    pData = pWork->pData;

    /* Lock the data structure */
    spin_lock(&pData->sLock);
    
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

    /* Free the work structure */
    kfree(pWork);

    /* Unlock the data structure */
    spin_unlock(&pData->sLock);
  }

  KLEM_MSG("Leaving work queue\n");

  return;
}

void klemCtrlCreate(void *pPtr)
{
  KLEMData *pData = (KLEMData *)pPtr;
  
  pData->pCtrlQueue = (void *)create_workqueue(KELM_CONTROL);

}

void klemCtrlStart(void *pPtr)
{
  KLEMData *pData = (KLEMData *)pPtr;
  ctrl_data *pWork = NULL;
  mm_segment_t fsSet;
  int rvalue;

  /* Set memory segment to kernel. */
  fsSet = get_fs();
  set_fs(KERNEL_DS);

  pWork = (ctrl_data *)kmalloc(sizeof(ctrl_data), GFP_NOFS); 

  if (NULL != pWork) {
    if (NULL != pData->pCtrlQueue)  {
      
      INIT_WORK((struct work_struct *)pWork,
		privCtrlProcess);
      pWork->eCommand = CONNECTION_START;
      pWork->pData = pData;
      rvalue = queue_work(pData->pCtrlQueue,
			  (struct work_struct *)pWork);
      KLEM_LOG("work queued response %d\n", rvalue);
    } else {
      KLEM_MSG("work queue not active\n");
      kfree(pWork);
    }
  }

  /* Goto the orignal filesystem/memory */
  set_fs(fsSet);
}

void klemCtrlStop(void *pPtr)
{
  KLEMData *pData = (KLEMData *)pPtr;
  ctrl_data *pWork = NULL;
  mm_segment_t fsSet;
  int rvalue;

  /* Set memory segment to kernel. */
  fsSet = get_fs();
  set_fs(KERNEL_DS);

  pWork = (ctrl_data *)kmalloc(sizeof(ctrl_data), GFP_NOFS); 

  if (NULL != pWork) {
    if (NULL != pData->pCtrlQueue)  {
      
      INIT_WORK((struct work_struct *)pWork, 
		privCtrlProcess);
      pWork->eCommand = CONNECTION_STOP;
      pWork->pData = pData;
      rvalue = queue_work(pData->pCtrlQueue,
			  (struct work_struct *)pWork);
      KLEM_LOG("work queued response %d\n", rvalue);
    } else {
      KLEM_MSG("work queue not active\n");
      kfree(pWork);
    }
  }

  set_fs(fsSet);
}

void klemCtrlDestroy(void *pPtr)
{
  KLEMData *pData = (KLEMData *)pPtr;

  flush_workqueue((struct workqueue_struct *)pData->pCtrlQueue);
  destroy_workqueue((struct workqueue_struct *)pData->pCtrlQueue);

  /* Destroy the socket, if its still exists */
  if (NULL != pData->pRawSocket) {
    klem80211Stop(pData);
    klemNetDisconnect(pData->pRawSocket);
    pData->pRawSocket = NULL;
  }
}
