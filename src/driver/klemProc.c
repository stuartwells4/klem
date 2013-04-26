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
#include <linux/proc_fs.h>
#include "klemHdr.h"
#include "klemData.h"
#include "klemCtrl.h"
#include "klem80211.h"

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
static int privProcSend(char *pKernBuf,
			char **ppIgnore,
			off_t iKernOffset,
			int iKernLen,
			int *pEOF,
			void *pPtr)
{
  KLEMData *pData = (KLEMData *)pPtr;
  char *pOutput = pData->proc.pBuffer + iKernOffset;
  int iOutLen = pData->proc.iSize - iKernOffset;
  int rvalue = 0;
  int loop;
  int ufnum = 0;

  spin_lock(&pData->sLock);

  if (0 == iKernOffset) {

    sprintf(pOutput, "KLEM Proc Interface\n\n");
    pOutput += strlen(pOutput);

    sprintf(pOutput, "Version:              %d\n", pData->uiVersion);
    pOutput += strlen(pOutput);

    sprintf(pOutput, "raw-device:           %s\n", pData->pDevName);
    pOutput += strlen(pOutput);

    if (NULL != pData->pRawSocket) {
      sprintf(pOutput, "raw-socket:           %p\n", pData->pRawSocket);
    } else {
      sprintf(pOutput, "raw-socket:           null\n");
    }
    pOutput += strlen(pOutput);

    if (NULL != pData->pNetLink) {
      sprintf(pOutput, "netlink:              %p\n", pData->pNetLink);
    } else {
      sprintf(pOutput, "netlink:              null\n");
    }
    pOutput += strlen(pOutput);

    sprintf(pOutput, "device-id:            %d\n", pData->uDeviceId);
    pOutput += strlen(pOutput);

    if (LEMU == pData->eMode) {
      sprintf(pOutput, "device-id:            lemu\n");
    } else {
      sprintf(pOutput, "device-id:            bridge\n");
    }
    pOutput += strlen(pOutput);

    ufnum = 0;
    sprintf(pOutput, "filter:               ");
    pOutput += strlen(pOutput);
    for (loop = 0; loop < MAX_WIRELESS_NODE; loop++) {
      if (true == pData->bFilterNode [loop]) {
	sprintf(pOutput, "%03d ", loop);
	pOutput += strlen(pOutput);
	ufnum += 1;
	if ((0 != ufnum) && (0 == ufnum % 8)) {
	  sprintf(pOutput, "\nfilter:               ");
	  pOutput += strlen(pOutput);
	}
      }
    }

    sprintf(pOutput, "\n");
    pOutput += strlen(pOutput);
    
    /* Printout wireless emulation data */
    pOutput += klem80211Proc(pData, pOutput);

    pOutput = pData->proc.pBuffer;
    pData->proc.iSize = strlen(pOutput);
    iOutLen = pData->proc.iSize;
  }

  if (iOutLen < iKernLen) {
    memcpy(pKernBuf, pOutput, iOutLen);
    rvalue = iOutLen;
    *pEOF = 0;
  } else {
    memcpy(pKernBuf, pOutput, iKernLen);
    rvalue = iOutLen - iKernLen;
    *pEOF = 0;
  }    

  spin_unlock(&pData->sLock);

  return rvalue;
}

/*
 * Interface to recv information from the proc file system.
 * A way to use script files to configure experiments without
 * use netlink.
 *
 * This routine is not secure.  Remove if you care about security.
 */
static int privProcRecv(struct file *pFile,
			const char __user *pUserBuf,
			unsigned long uiCount,
			void *pMyData)
{
  KLEMData *pData = (KLEMData *)pMyData;
  const char *pCommand = NULL;
  unsigned int iCommandLen = 0;
  const char *pValue = NULL;
  unsigned int iValueLen = 0;
  bool bCommand = true;
  bool bParse = false;
  unsigned int loop = 0;
  unsigned int utmp;

  spin_lock(&pData->sLock);

  /* Look for command = values, and act upon it. */
  loop = 0;
  while (loop < strlen(pUserBuf)) {
    switch(pUserBuf [loop]) 
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
	  pCommand = &pUserBuf [loop];
	}

	/* Did we find a value */
	if ((NULL == pValue) && (false == bCommand)) {
	  pValue = &pUserBuf [loop];
	}

	/* Determine the length of this string run. */
	if (true == bCommand) {
	  iCommandLen += 1;
	} else {
	  iValueLen += 1;
	}
	break;
      }

    /* Should parsing those command = values */
    if ((NULL != pCommand) && (NULL != pValue) && (true == bParse)) {
      /* Do we have a command */
      if (strncmp(pCommand, COMMAND_STR, iCommandLen) == 0) {
	if (strncmp(pValue, COMMAND_START_STR, iValueLen) == 0) {
	  klemCtrlStart(pData);
	} else if (strncmp(pValue, COMMAND_STOP_STR, iValueLen) == 0) { 
	  klemCtrlStop(pData);
	}
      } else if (strncmp(pCommand, DEVICE_STR, iCommandLen) == 0) {
	if (iValueLen < sizeof(pData->pDevName)) {
	  memset(pData->pDevName, 0, sizeof(pData->pDevName));
	  strncpy(pData->pDevName, pValue, iValueLen);
	} 
      } else if (strncmp(pCommand, ID_STR, iCommandLen) == 0) {
	utmp = (unsigned int)simple_strtol(pValue, NULL, 10);
	if (utmp >= MAX_WIRELESS_NODE) {
	  pData->uDeviceId = 0;
	  KLEM_LOG("Error, device id %s must be beteen 0-255\n",
		   pValue);
	} else {
	  pData->uDeviceId = utmp;
	}
      } else if (strncmp(pCommand, FILTER_STR, iCommandLen) == 0) {
	utmp = (unsigned int)simple_strtol(pValue, NULL, 10);
	if (utmp >= MAX_WIRELESS_NODE) {
	  KLEM_LOG("Error, filter id %s must be beteen 0-255\n",
		   pValue);
	} else {
	  pData->bFilterNode [utmp] = true;
	}
      } else if (strncmp(pCommand, ACCEPT_STR, iCommandLen) == 0) {
	utmp = (unsigned int)simple_strtol(pValue, NULL, 10);
	if (utmp >= MAX_WIRELESS_NODE) {
	  KLEM_LOG("Error, accept id %s must be beteen 0-255\n",
		   pValue);
	} else {
	  pData->bFilterNode [utmp] = false;
	}
      } else if (strncmp(pCommand, MODE_STR, iCommandLen) == 0) {
	if (strncmp(pValue, MODE_LEMU_STR, iValueLen) == 0) {
	  pData->eMode = LEMU;
	} else if (strncmp(pValue, MODE_BRIDGE_STR, iValueLen) == 0) {
	  pData->eMode = BRIDGE;
	}
      }
      pCommand = NULL;
      pValue = NULL;
      iCommandLen = 0;
      iValueLen = 0;
      bCommand = false;
      bParse = false;
    }

    bParse = false;
    loop += 1;
  }

  spin_unlock(&pData->sLock);

  return uiCount;
}
  
/* Proc entry point */
void klemProcInit(void *pPtr)
{
  KLEMData *pData = (KLEMData *)pPtr;

  if (NULL != pData) {
    
    pData->proc.pEntry = create_proc_read_entry(KLEM_NAME,
						0644,
						NULL,
						privProcSend,
						pPtr);
    
    if (NULL == pData->proc.pEntry) {
      KLEM_LOG("Failed to create proc read entry %s\n", KLEM_NAME);
    } else {
      pData->proc.pEntry->write_proc = privProcRecv;
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
