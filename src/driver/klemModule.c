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
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>

#include "klemData.h"
#include "klemHdr.h"
#include "klemProc.h"
#include "klemCtrl.h"

/*
  The linux kernel module insmod entry point.
*/
static int __init privKLEMInit(void)
{
  /*
   * This is where the driver will call functions to install
   * its functions.  Keep this area simple.
   */
  int iRvalue = -ENOMEM;
  KLEMData *pData = NULL;

  pData = klemDataInit();

  if (NULL != pData) {
    /* Lets create a class reference */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6,4,0))
    pData->pClass = class_create("Wireless KLEM Driver");
#else
    pData->pClass = class_create(THIS_MODULE, "Wireless KLEM Driver");
#endif
    if (IS_ERR(pData->pClass)) {
      KLEM_MSG("Failed to get some class\n");
      iRvalue = PTR_ERR(pData->pClass);
      pData->pClass = NULL;
      klemDataDeInit();
      return iRvalue;
    }

    /* Let the world know we loaded */
    klemCtrlCreate(pData);
    klemProcInit(pData);
    iRvalue = 0;
  } else {
    iRvalue = -ENOMEM;
  }

  if (0 == iRvalue) {
    KLEM_MSG("Module sucessfully installed\n");
  } else {
    KLEM_MSG("Module failed to install\n");
  }
  return iRvalue;
}

/*
   Inform the linux kernel of the entry point.
*/
module_init(privKLEMInit);

/*
  The linux kernel module insmod exit point.
*/
static void __exit privKLEMExit(void)
{
  KLEMData *pData = NULL;

  /*
   * This is where the driver will call functions to un-install
   * itself.  Keep this area simple and remember all device drivers
   * must rmmod properly, and without memory leaks.
   */
  pData = klemGetData();
  if (NULL != pData) {
    klemProcDeinit(pData);
    klemCtrlDestroy(pData);
    if (NULL != pData->pClass) {
      class_destroy(pData->pClass);
    }
    klemDataDeInit();
    pData = NULL;
  }

  KLEM_MSG("Module has been removed\n");
}

/*
   Inform the linux kernel of the exit point.
*/
module_exit(privKLEMExit);

/* Module Information for modinfo */
MODULE_VERSION(KLEM_STR_VERSION);

/*
 * This driver uses GPL only facilities of the linux kernel, so
 * it may not load if you try and change the license.
 */
MODULE_LICENSE("GPL and additional rights");

MODULE_AUTHOR("Stuart Wells swells@stuartwells.net");
MODULE_DESCRIPTION("Wireless 802.11 Link Emulator");
