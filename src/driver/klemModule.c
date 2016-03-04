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
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>

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
  int rvalue = 1;
  KLEMData *pData = NULL;

  pData = klemDataInit();

  if (NULL != pData) {
    /* Lets create a class reference */
    pData->pClass = class_create(THIS_MODULE, "Wireless KLEM Driver");
    if (IS_ERR(pData->pClass)) {
      KLEM_MSG("Failed to get some class\n");
      return 0;
    }

    /* Let the world know we loaded */
    klemCtrlCreate(pData);
    klemProcInit(pData);
    rvalue = 0;
  } else {
    rvalue = 1;
  }

  if (0 == rvalue) {
    KLEM_MSG("Module sucessfully installed\n");
  } else {
    KLEM_MSG("Module failed to install\n");
  }
  return rvalue;
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
MODULE_LICENSE("GPL");

MODULE_AUTHOR("Stuart Wells swells@stuartwells.net");
MODULE_DESCRIPTION("Wireless 802.11 Link Emulator");
