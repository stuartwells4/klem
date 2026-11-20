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
#include <linux/proc_fs.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>

#include "klemHdr.h"
#include "klemData.h"
#include "klemCtrl.h"
#include "klem80211.h"
#include "klemProc.h"

/* String information for starting/stopping the system. */
#define COMMAND_STR "command"
#define COMMAND_START_STR "start"
#define COMMAND_STOP_STR "stop"

/* String information for setting the device */
#define DEVICE_STR "device"

/* string information for setting the id */
#define ID_STR "id"

/* string to filter a wireless node */
#define FILTER_STR "filter"

/* string to unfilter/accept node inforamation */
#define ACCEPT_STR "accept"

/* string to set brige/lemu */
#define MODE_STR "mode"
#define MODE_LEMU_STR "lemu"
#define MODE_BRIDGE_STR "bridge"

/*
 * Interface to send information to the proc file system.
 */
/* String information for starting/stopping the system. */
static int privProcOutputSeq(struct seq_file *pOutput, void *pBuffer)
{
  KLEMData *pData;
  int iLoop;
  int iFilterNum = 0;

  if (NULL != pOutput) {
    pData = (KLEMData *)pOutput->private;

    if (NULL != pData) {
      mutex_lock(&pData->sLock);
      seq_printf(pOutput, "KLEM Proc Interface\n\n");
      seq_printf(pOutput, "Version:              %d\n", pData->uiVersion);
      seq_printf(pOutput, "raw-device:           %s\n", pData->pDevName);

      if (NULL != pData->pRawSocket) {
	seq_printf(pOutput, "raw-socket:           %p\n", pData->pRawSocket);
      } else {
	seq_printf(pOutput, "raw-socket:           null\n");
      }

      if (NULL != pData->pNetLink) {
	seq_printf(pOutput, "netlink:              %p\n", pData->pNetLink);
      } else {
	seq_printf(pOutput, "netlink:              null\n");
      }

      seq_printf(pOutput, "device-id:            %d\n", pData->uiDeviceId);

      if (LEMU == pData->eMode) {
	seq_printf(pOutput, "mode:                 lemu\n");
      } else {
	seq_printf(pOutput, "mode:                 bridge\n");
      }

      iFilterNum = 0;
      seq_printf(pOutput, "filter:               ");
      for (iLoop = 0; iLoop < MAX_WIRELESS_NODE; iLoop++) {
	if (true == pData->bFilterNode [iLoop]) {
	  seq_printf(pOutput, "%03d ", iLoop);
	  iFilterNum += 1;
	  if ((0 != iFilterNum) && (0 == iFilterNum % 8)) {
	    seq_printf(pOutput, "\nfilter:               ");
	  }
	}
      }

      seq_printf(pOutput, "\n");

      /* Printout wireless emulation data */
      klem80211Proc(pData, pOutput);
      mutex_unlock(&pData->sLock);
    }
  }

  return 0;
}

/* True only if the run pWord [0..uiLen) is exactly pKeyword, not a prefix. */
static bool privProcMatch(const char *pWord, unsigned int uiLen,
                          const char *pKeyword)
{
  return ((strlen(pKeyword) == uiLen) &&
          (0 == strncmp(pWord, pKeyword, uiLen)));
}

/*
 * Interface to recv information from the proc file system.
 * A way to use script files to configure experiments without
 * use netlink.
 *
 * This routine is not secure.  Remove if you care about security.
 */
static int privProcInput(struct file *pFile,
			 const char *pUserBuf,
			 unsigned long uCount,
			 void *pMyData)
{
  KLEMData *pData = (KLEMData *)pMyData;
  const char *pCommand = NULL;
  unsigned int uiCommandLen = 0;
  const char *pValue = NULL;
  unsigned int uiValueLen = 0;
  bool bCommand = true;
  bool bParse = false;
  unsigned int uiLoop = 0;
  unsigned int uiTmp;
  char cByte;

  mutex_lock(&pData->sLock);

  /* Look for command = values, and act upon it. */
  uiLoop = 0;
  while (uiLoop <= uCount) {
    /* Treat the end of the buffer as a newline so the last pair is parsed. */
    if (uiLoop < uCount) {
      cByte = pUserBuf [uiLoop];
    } else {
      cByte = '\n';
    }

    switch(cByte)
      {
      case ' ':
      case '\r':
      case '\n':
        /* If we have a pvalue, we found command, value, now parse. */
        if ((NULL != pValue) && (NULL != pCommand)) {
          bParse = true;
          bCommand = true;
        }
        break;
      case '=':
        /* Possible value next */
        bCommand = false;
        break;
      default:
        /* did we find a command */
        if ((NULL == pCommand) && (true == bCommand)) {
          pCommand = &pUserBuf [uiLoop];
        }

        /* Did we find a value */
        if ((NULL == pValue) && (false == bCommand)) {
          pValue = &pUserBuf [uiLoop];
        }

        /* Determine the length of this string run. */
        if (true == bCommand) {
          uiCommandLen += 1;
        } else {
          uiValueLen += 1;
        }
        break;
      }

    /* Should parsing those command = values */
    if ((NULL != pCommand) && (NULL != pValue) && (true == bParse)) {
      /* Do we have a command */
      if (privProcMatch(pCommand, uiCommandLen, COMMAND_STR)) {
        if (privProcMatch(pValue, uiValueLen, COMMAND_START_STR)) {
          klemCtrlStart(pData);
        } else if (privProcMatch(pValue, uiValueLen, COMMAND_STOP_STR)) {
          klemCtrlStop(pData);
        }
      } else if (privProcMatch(pCommand, uiCommandLen, DEVICE_STR)) {
        if (uiValueLen < sizeof(pData->pDevName)) {
          memset(pData->pDevName, 0, sizeof(pData->pDevName));
          strncpy(pData->pDevName, pValue, uiValueLen);
        }
      } else if (privProcMatch(pCommand, uiCommandLen, ID_STR)) {
        uiTmp = (unsigned int)simple_strtoul(pValue, NULL, 10);
        if (uiTmp >= MAX_WIRELESS_NODE) {
          pData->uiDeviceId = 0;
          KLEM_LOG("Error, device id %s must be beteen 0-255\n",
                   pValue);
        } else {
          pData->uiDeviceId = uiTmp;
        }
      } else if (privProcMatch(pCommand, uiCommandLen, FILTER_STR)) {
        uiTmp = (unsigned int)simple_strtoul(pValue, NULL, 10);
        if (uiTmp >= MAX_WIRELESS_NODE) {
          KLEM_LOG("Error, filter id %s must be beteen 0-255\n",
                   pValue);
        } else {
          pData->bFilterNode [uiTmp] = true;
        }
      } else if (privProcMatch(pCommand, uiCommandLen, ACCEPT_STR)) {
        uiTmp = (unsigned int)simple_strtoul(pValue, NULL, 10);
        if (uiTmp >= MAX_WIRELESS_NODE) {
          KLEM_LOG("Error, accept id %s must be beteen 0-255\n",
                   pValue);
        } else {
          pData->bFilterNode [uiTmp] = false;
        }
      } else if (privProcMatch(pCommand, uiCommandLen, MODE_STR)) {
        if (privProcMatch(pValue, uiValueLen, MODE_LEMU_STR)) {
          pData->eMode = LEMU;
        } else if (privProcMatch(pValue, uiValueLen, MODE_BRIDGE_STR)) {
          pData->eMode = BRIDGE;
        }
      }
      pCommand = NULL;
      pValue = NULL;
      uiCommandLen = 0;
      uiValueLen = 0;
      bCommand = true;
      bParse = false;
    }

    bParse = false;
    uiLoop += 1;
  }

  mutex_unlock(&pData->sLock);

  return uCount;
}
static ssize_t privProcInputSeq(struct file *pFile,
				const char __user *pBuffer,
				size_t uCount,
				loff_t *pPos)
{
  void *pData = NULL;
  char *pKernBuf = NULL;
  ssize_t iRvalue = 0;

  if ((NULL != pFile) && (NULL != pBuffer) && (uCount > 0)) {
    /* Commands are short, a page is plenty. */
    if (uCount > PAGE_SIZE) {
      uCount = PAGE_SIZE;
    }

    /* Copy the user data into kernel memory, nul terminated. */
    pKernBuf = memdup_user_nul(pBuffer, uCount);
    if (IS_ERR(pKernBuf)) {
      return PTR_ERR(pKernBuf);
    }

    pData = klemGetData();
    if (NULL != pData) {
      privProcInput(pFile, pKernBuf, strlen(pKernBuf), pData);
    }
    kfree(pKernBuf);
    iRvalue = uCount;
  }

  return iRvalue;
}

static int privProcOpen(struct inode *pINode, struct file *pFile)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,17,0))
  return single_open(pFile, privProcOutputSeq, pde_data(pINode));
#else
  return single_open(pFile, privProcOutputSeq, PDE_DATA(pINode));
#endif
}

static const struct proc_ops privConstProcOps = {
  .proc_open    = privProcOpen,
  .proc_read    = seq_read,
  .proc_write   = privProcInputSeq,
  .proc_lseek   = seq_lseek,
  .proc_release = single_release,
};

/* Proc entry point */
void klemProcInit(void *pPtr)
{
  KLEMData *pData = (KLEMData *)pPtr;

  if (NULL != pData) {
    pData->proc.pEntry = proc_create_data(KLEM_NAME,
					  0644,
					  NULL,
					  &privConstProcOps,
					  pPtr);

    if (NULL == pData->proc.pEntry) {
      KLEM_LOG("Failed to create proc entry %s\n", KLEM_NAME);
    }
  }
}


void klemProcDeinit(void *pPtr)
{
  KLEMData *pData = (KLEMData *)pPtr;;

  if (NULL != pData) {
    if (NULL != pData->proc.pEntry) {
      /* Don't remove it, if we didn't have it. */
      remove_proc_entry(KLEM_NAME, NULL);
      pData->proc.pEntry = NULL;
    }
  }
}
