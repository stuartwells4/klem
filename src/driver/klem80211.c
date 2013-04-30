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
#include <linux/spinlock.h>
#include <net/dst.h>
#include <net/xfrm.h>
#include <net/mac80211.h>
#include <net/ieee80211_radiotap.h>
#include <linux/skbuff.h>
#include <linux/version.h>
#include <linux/etherdevice.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include "klemHdr.h"
#include "klemData.h"
#include "klemNet.h"

#define KLEM_MAX_QOS 4
#define KLEM_QUEUE_MAX 32
#define KLEM_QUEUE_HIGH 16
#define KLEM_QUEUE_LOW 8

/*
 * values obtained from

 * http://en.wikipedia.org/wiki/IEEE_802.11g-2003
 *
 * 802.11b/g channels in 2.4 Ghz Band
 */
static const struct ieee80211_channel privConstChannels_2ghz[] = {
  /* channel 1
   * frequency 2.412 GHZ
   * width 2.401 HGZ - 2.423 GHZ
   * Overlaps channel 2,3,4,5
   */
  {
    .band = IEEE80211_BAND_2GHZ, .center_freq = 2412,
    .hw_value = 2412, .max_power = 25,
  },

  /* channel 2
   * frequency 2.417 GHZ
   * width 2.406 HGZ - 2.428 GHZ
   * Overlaps channel 1,3,4,5,6
   */
  {
    .band = IEEE80211_BAND_2GHZ, .center_freq = 2417,
    .hw_value = 2417, .max_power = 25,
  },

  /* channel 3
   * frequency 2.422 GHZ
   * width 2.411 HGZ - 2.433 GHZ
   * Overlaps channel 1,2,4,5,6,7
   */
  {
    .band = IEEE80211_BAND_2GHZ, .center_freq = 2422,
    .hw_value = 2422, .max_power = 25,
  },

  /* channel 4
   * frequency 2.427 GHZ
   * width 2.416 HGZ - 2.438 GHZ
   * Overlaps channel 1,2,3,5,6,7,8
   */
  {
    .band = IEEE80211_BAND_2GHZ, .center_freq = 2427,
    .hw_value = 2427, .max_power = 25,
  },

  /* channel 5
   * frequency 2.432 GHZ
   * width 2.421 HGZ - 2.443 GHZ
   * Overlaps channel 1,2,3,4,6,7,8,9
   */
  {
    .band = IEEE80211_BAND_2GHZ, .center_freq = 2432,
    .hw_value = 2432, .max_power = 25,
  },

  /* channel 6
   * frequency 2.437 GHZ
   * width 2.426 HGZ - 2.448 GHZ
   * Overlaps channel 2,3,4,5,7,8,9,10
   */
  {
    .band = IEEE80211_BAND_2GHZ, .center_freq = 2437,
    .hw_value = 2437, .max_power = 25,
  },


  /* channel 7
   * frequency 2.442 GHZ
   * width 2.431 HGZ - 2.453 GHZ
   * Overlaps channel 3,4,5,6,8,9,10,11
   */
  {
    .band = IEEE80211_BAND_2GHZ, .center_freq = 2442,
    .hw_value = 2442, .max_power = 25,
  },

  /* channel 8,
   * frequency 2.447 GHZ
   * width 2.436 HGZ - 2.458 GHZ
   * Overlaps channel 4,5,6,7,9,10,11,12
   */
  {
    .band = IEEE80211_BAND_2GHZ, .center_freq = 2447,
    .hw_value = 2447, .max_power = 25,
  },

  /* channel 9,
   * frequency 2.452 GHZ
   * width 2.441 HGZ - 2.463 GHZ
   * Overlaps channel 5,6,7,8,10,11,12,13
   */
  {
    .band = IEEE80211_BAND_2GHZ, .center_freq = 2452,
    .hw_value = 2452, .max_power = 25,
  },

  /* channel 10,
   * frequency 2.457 GHZ
   * width 2.446 HGZ - 2.468 GHZ
   * Overlaps channel 6,7,8,9,11,12,13
   */
  {
    .band = IEEE80211_BAND_2GHZ, .center_freq = 2457,
    .hw_value = 2457, .max_power = 25,
  },

  /* channel 11,
   * frequency 2.462 GHZ
   * width 2.451 HGZ - 2.473 GHZ
   * Overlaps channel 7,8,9,10,12,13
   */
  {
    .band = IEEE80211_BAND_2GHZ, .center_freq = 2462,
    .hw_value = 2462, .max_power = 25,
  },

  /* channel 12,
   * frequency 2.467 GHZ
   * width 2.456 HGZ - 2.478 GHZ
   * Overlaps channel 8,9,10,11,13
   */
  {
    .band = IEEE80211_BAND_2GHZ, .center_freq = 2467,
    .hw_value = 2467, .max_power = 25,
  },

  /* channel 13,
   * frequency 2.472 GHZ
   * width 2.461 HGZ - 2.483 GHZ
   * Overlaps channel 9,10,11,12
   */
  {
    .band = IEEE80211_BAND_2GHZ, .center_freq = 2472,
    .hw_value = 2472, .max_power = 25,
  },

  /* channel 14,
   * frequency 2.484 GHZ
   * width 2.446 HGZ - 2.468 GHZ
   * Overlaps channel 6,7,8,9,11,12,13
   */
  {
    .band = IEEE80211_BAND_2GHZ, .center_freq = 2484,
    .hw_value = 2484, .max_power = 25,
  },
};

/*
 * IEEE 802.11a Channel Assignments.  5GHZ spectrum
 */
static const struct ieee80211_channel privConstChannels_5ghz[] = {
  /* channel 34 */
  {
    .band = IEEE80211_BAND_5GHZ, .center_freq = 5170,
    .hw_value = 5170, .max_power = 25,
  },

  /* channel 36 */
  {
    .band = IEEE80211_BAND_5GHZ, .center_freq = 5180,
    .hw_value = 5180, .max_power = 25,
  },

  /* channel 38 */
  {
    .band = IEEE80211_BAND_5GHZ, .center_freq = 5190,
    .hw_value = 5190, .max_power = 25,
  },

  /* channel 40 */
  {
    .band = IEEE80211_BAND_5GHZ, .center_freq = 5200,
    .hw_value = 5200, .max_power = 25,
  },

  /* channel 42 */
  {
    .band = IEEE80211_BAND_5GHZ, .center_freq = 5210,
    .hw_value = 5210, .max_power = 25,
  },

  /* channel 44 */
  {
    .band = IEEE80211_BAND_5GHZ, .center_freq = 5220,
    .hw_value = 5220, .max_power = 25,
  },

  /* channel 46 */
  {
    .band = IEEE80211_BAND_5GHZ, .center_freq = 5230,
    .hw_value = 5230, .max_power = 25,
  },

  /* channel 48 */
  {
    .band = IEEE80211_BAND_5GHZ, .center_freq = 5240,
    .hw_value = 5240, .max_power = 25,
  },

  /* channel 52 */
  {
    .band = IEEE80211_BAND_5GHZ, .center_freq = 5260,
    .hw_value = 5260, .max_power = 25,
  },

  /* channel 56 */
  {
    .band = IEEE80211_BAND_5GHZ, .center_freq = 5280,
    .hw_value = 5280, .max_power = 25,
  },

  /* channel 60 */
  {
    .band = IEEE80211_BAND_5GHZ, .center_freq = 5300,
    .hw_value = 5300, .max_power = 25,
  },

  /* channel 64 */
  {
    .band = IEEE80211_BAND_5GHZ, .center_freq = 5320,
    .hw_value = 5320, .max_power = 25,
  },

  /* channel 100 */
  {
    .band = IEEE80211_BAND_5GHZ, .center_freq = 5500,
    .hw_value = 5500, .max_power = 25,
  },

  /* channel 104 */
  {
    .band = IEEE80211_BAND_5GHZ, .center_freq = 5520,
    .hw_value = 5520, .max_power = 25,
  },

  /* channel 108 */
  {
    .band = IEEE80211_BAND_5GHZ, .center_freq = 5540,
    .hw_value = 5540, .max_power = 25,
  },

  /* channel 112 */
  {
    .band = IEEE80211_BAND_5GHZ, .center_freq = 5560,
    .hw_value = 5560, .max_power = 25,
  },

  /* channel 116 */
  {
    .band = IEEE80211_BAND_5GHZ, .center_freq = 5580,
    .hw_value = 5580, .max_power = 25,
  },

  /* channel 120 */
  {
    .band = IEEE80211_BAND_5GHZ, .center_freq = 5600,
    .hw_value = 5600, .max_power = 25,
  },

  /* channel 124 */
  {
    .band = IEEE80211_BAND_5GHZ, .center_freq = 5620,
    .hw_value = 5620, .max_power = 25,
  },

  /* channel 128 */
  {
    .band = IEEE80211_BAND_5GHZ, .center_freq = 5640,
    .hw_value = 5640, .max_power = 25,
  },

  /* channel 132 */
  {
    .band = IEEE80211_BAND_5GHZ, .center_freq = 5660,
    .hw_value = 5660, .max_power = 25,
  },

  /* channel 136 */
  {
    .band = IEEE80211_BAND_5GHZ, .center_freq = 5680,
    .hw_value = 5680, .max_power = 25,
  },

  /* channel 140 */
  {
    .band = IEEE80211_BAND_5GHZ, .center_freq = 5700,
    .hw_value = 5700, .max_power = 25,
  },

  /* channel 149 */
  {
    .band = IEEE80211_BAND_5GHZ, .center_freq = 5745,
    .hw_value = 5745, .max_power = 25,
  },

  /* channel 153 */
  {
    .band = IEEE80211_BAND_5GHZ, .center_freq = 5765,
    .hw_value = 5765, .max_power = 25,
  },

  /* channel 157 */
  {
    .band = IEEE80211_BAND_5GHZ, .center_freq = 5785,
    .hw_value = 5785, .max_power = 25,
  },

  /* channel 161 */
  {
    .band = IEEE80211_BAND_5GHZ, .center_freq = 5805,
    .hw_value = 5805, .max_power = 25,
  },

  /* channel 165 */
  {
    .band = IEEE80211_BAND_5GHZ, .center_freq = 5825,
    .hw_value = 5825, .max_power = 25,
  },
};


/* Just use what is a requirement for 2gb bitrate. */
static const struct ieee80211_rate privConstBitRate_2g[] = {
  { .bitrate = 10,  .flags = 0 },
  { .bitrate = 20,  .flags = IEEE80211_RATE_SHORT_PREAMBLE },
  { .bitrate = 55,  .flags = IEEE80211_RATE_SHORT_PREAMBLE },
  { .bitrate = 110, .flags = IEEE80211_RATE_SHORT_PREAMBLE },
  { .bitrate = 60,  .flags = 0 },
  { .bitrate = 120, .flags = 0 },
  { .bitrate = 240, .flags = 0 },
};

/* Just use what is a requirement for 5gb bitrate. */
static const struct ieee80211_rate privConstBitRate_5g[] = {
  { .bitrate = 60,  .flags = 0 },
  { .bitrate = 120, .flags = 0 },
  { .bitrate = 240, .flags = 0 },
};

/* define a length for above tables */
#define CHANNEL_SIZE_2G ARRAY_SIZE(privConstChannels_2ghz)
#define CHANNEL_SIZE_5G ARRAY_SIZE(privConstChannels_5ghz)
#define RATE_SIZE_2G ARRAY_SIZE(privConstBitRate_2g)
#define RATE_SIZE_5G ARRAY_SIZE(privConstBitRate_5g)

typedef struct {
  bool bActive;
} VIFData;

typedef struct {
  bool bActive;
} STAData;


/*
 * Data we need for each mac 802.11 instance.
 *
 */
typedef struct {
  /* Structures specific for mac80211 book keeping */
  struct ieee80211_hw *pHW;
  KLEMData *pData;
  struct device *pDev;
  int iPower;
  bool bIdle;
  unsigned long uBeacons;
  unsigned long uBeaconCount;
  char devName [64];
  struct mac_address  macAddress;

  /* Structures for band, channel and rates for 2GHZ */
  struct ieee80211_supported_band band_2g;
  struct ieee80211_channel channel_2g [CHANNEL_SIZE_2G];
  struct ieee80211_rate rate_2g [RATE_SIZE_2G];

  /* Structures for band, channel and rates for 5GHZ */
  struct ieee80211_supported_band band_5g;
  struct ieee80211_channel channel_5g [CHANNEL_SIZE_5G];
  struct ieee80211_rate rate_5g [RATE_SIZE_5G];

  /* Send thread info */
  char pSendName [64];
  void *pSendThread;
  spinlock_t sSpinLock;
  wait_queue_head_t sListWait;


  bool bRadioActive;
  bool bActive;
  /* qos queues */
  struct qos_info {
    /* information on how to transmit the packet.  For now keep, but ignore */
    u8 aifs;
    u16 cw_min;
    u16 cw_max;
    u16 txop;

    /* Debugging, keep track of packets sent, error*/
    unsigned long uRecvNumber;
    unsigned long uSendNumber;
    unsigned long uSendErrorNumber;
    unsigned long uSendDroppedNumber;

    /* Queue information for transitting a packet */
    struct sk_buff_head listSkb;
    unsigned int uListDepth;
    bool bListActive;
  } qos [KLEM_MAX_QOS];
} mac80211Data;

/* forward declarations of needed functions. */
static int privStart(struct ieee80211_hw *pHW);
static void privStop(struct ieee80211_hw *pHW);
static int privAddInterface(struct ieee80211_hw *pHW,
                 struct ieee80211_vif *pVIF);
static void privRemoveInterface(struct ieee80211_hw *pHW,
                struct ieee80211_vif *pVIF);
static void privBssInfoChanged(struct ieee80211_hw *pHW,
                   struct ieee80211_vif *pVIF,
                   struct ieee80211_bss_conf *pBSS,
                   u32 uChanged);
static int privStaAdd(struct ieee80211_hw *pHW,
              struct ieee80211_vif *pVIF,
              struct ieee80211_sta *pSta);
static int privStaRemove(struct ieee80211_hw *pHW,
             struct ieee80211_vif *pVIF,
             struct ieee80211_sta *pSta);
static void privStaNotify(struct ieee80211_hw *pHW,
              struct ieee80211_vif *pVIF,
              enum sta_notify_cmd eCmd,
              struct ieee80211_sta *pSta);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0))
static void privTX(struct ieee80211_hw *pHW,
           struct ieee80211_tx_control *pControl,
           struct sk_buff *pSkb);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39))
static void privTX(struct ieee80211_hw *pHW, struct sk_buff *pSkb);
#else
static int privTX(struct ieee80211_hw *pHW, struct sk_buff *pSkb);
#endif
static int privConfig(struct ieee80211_hw *pHW, u32 uChanged);
static void privConfigureFilter(struct ieee80211_hw *pHW,
                unsigned int uCchangedFlags,
                unsigned int *puTotalFlags,
                u64 uMulticast);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0))
static int privConfigureTX(struct ieee80211_hw *pHW,
               struct ieee80211_vif *pVIF,
               u16 uiQueue,
               const struct ieee80211_tx_queue_params *pQueue);
#else
static int privConfigureTX(struct ieee80211_hw *pHW,
               u16 uiQueue,
               const struct ieee80211_tx_queue_params *pQueue);
#endif
static void privCompleteTX(void *pPtr, struct sk_buff *pSkb, bool bAck);

static struct ieee80211_ops privKlem80211OPS =
{
  /* routines to add/stop mac 802.11 radio */
  .start = privStart,
  .stop = privStop,

  /* routines to add/remove interfaces */
  .add_interface = privAddInterface,
  .remove_interface = privRemoveInterface,

  .bss_info_changed = privBssInfoChanged,
  .configure_filter = privConfigureFilter,

  /* State routines, not importent, but mandatory. */
  .sta_add = privStaAdd,
  .sta_remove = privStaRemove,
  .sta_notify = privStaNotify,

  .tx = privTX,
  .config = privConfig,

  /* Added as a required function in later kernels. */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,0,0))
  .conf_tx = privConfigureTX,
#else
  .conf_tx = privConfigureTX,
#endif

  /* not required for using mac80211 */
  .prepare_multicast = NULL,
  .change_interface = NULL,
  .set_tim = NULL,
  .get_survey = NULL,
  .ampdu_action = NULL,
  .sw_scan_start = NULL,
  .sw_scan_complete = NULL,
  .flush = NULL,
};

static struct device_driver klem80211Driver = {
  .name = "klem-mac80211"
};

/*
 * Called when the wireless mac 802.11 radio is started
 */
static int privStart(struct ieee80211_hw *pHW)
{
  mac80211Data *pMacData = (mac80211Data *)pHW->priv;

  if (NULL != pMacData) {
    pMacData->bRadioActive = true;
    KLEM_LOG("Called (%p)\n", pMacData);
  }
  return 0;
}

/*
 * Called when the wireless mac 802.11 radio is stopped
*/
static void privStop(struct ieee80211_hw *pHW)
{
  mac80211Data *pMacData = (mac80211Data *)pHW->priv;

  if (NULL != pMacData) {
    pMacData->bRadioActive = false;

    KLEM_LOG("Called (%p)\n", pMacData);
  }
}

/*
 * Add wireless interface
 */
static int privAddInterface(struct ieee80211_hw *pHW,
                struct ieee80211_vif *pVIF)
{
  mac80211Data *pMacData = (mac80211Data *)pHW->priv;
  VIFData *pVIFData = (VIFData *)pVIF->drv_priv;

  KLEM_LOG("Called pMacData (%p) pVIFData(%p)\n", pMacData, pVIFData);

  if (NULL != pVIFData) {
    pVIFData->bActive = true;
  }

  return 0;
}

/*
 * Remove wireless Interface
 */
static void privRemoveInterface(struct ieee80211_hw *pHW,
                struct ieee80211_vif *pVIF)
{
  mac80211Data *pMacData = (mac80211Data *)pHW->priv;
  VIFData *pVIFData = (VIFData *)pVIF->drv_priv;

  KLEM_LOG("Called pMacData (%p) pVIFData(%p)\n", pMacData, pVIFData);

  if (NULL != pVIFData) {
    pVIFData->bActive = false;
  }

  KLEM_LOG("Remove Interface pMacData (%p) pVIIFData (%p)\n",
       pMacData, pVIFData);

}

/*
 * configurfe beacons
 *
 */
static void privBssInfoChanged(struct ieee80211_hw *pHW,
                   struct ieee80211_vif *pVIF,
                   struct ieee80211_bss_conf *pBSS,
                   u32 uChanged)
{
  VIFData *pVIFData = (VIFData *)pVIF->drv_priv;
  mac80211Data *pMacData = (mac80211Data *)pHW->priv;
  
  if ((NULL != pMacData) && (NULL != pVIFData)) {
    if (true == pVIFData->bActive) {
      /* Could do something with bssid, assoc and aid here. */
      if (uChanged & BSS_CHANGED_BEACON_INT) {
        if (NULL != pMacData) {
          pMacData->uBeacons = (pBSS->beacon_int * HZ) >> 10;
          if (0 == pMacData->uBeacons)
            pMacData->uBeacons = 1;
          KLEM_LOG("Beacons = %lud\n", pMacData->uBeacons);
        }
      }
      
      if (uChanged & BSS_CHANGED_ERP_CTS_PROT) {
        KLEM_LOG("ERP_CTS_PROT: %d\n", pBSS->use_cts_prot);
      }
      
      if (uChanged & BSS_CHANGED_ERP_PREAMBLE) {
        KLEM_LOG("ERP_PREAMBLE: %d\n", pBSS->use_short_preamble);
      }
      
      if (uChanged & BSS_CHANGED_ERP_SLOT) {
        KLEM_LOG("ERP_SLOT: %d\n", pBSS->use_short_slot);
      }
      
      if (uChanged & BSS_CHANGED_HT) {
        KLEM_LOG("HT: op_mode=0x%x, chantype=%d\n",
                 pBSS->ht_operation_mode,
                 pBSS->channel_type);
      }
      
      if (uChanged & BSS_CHANGED_BASIC_RATES) {
        KLEM_LOG("BASIC_RATES: 0x%llx\n",
                 (unsigned long long)pBSS->basic_rates);
      }
    }
  }
}

static int privStaAdd(struct ieee80211_hw *pHW,
              struct ieee80211_vif *pVIF,
              struct ieee80211_sta *pSta)
{
  VIFData *pVIFData = (VIFData *)pVIF->drv_priv;
  STAData *pSTAData = (STAData *)pSta->drv_priv;

  if ((NULL != pVIFData) && (NULL != pSTAData)) {
    if (true == pVIFData->bActive) {
      pSTAData->bActive = true;
    }
  }

  KLEM_LOG("Called pHW(%p) pVIFData(%p) pSTAData(%p)\n",
       pHW, pVIFData, pSTAData);

  return 0;
}

static int privStaRemove(struct ieee80211_hw *pHW,
             struct ieee80211_vif *pVIF,
             struct ieee80211_sta *pSta)
{
  VIFData *pVIFData = (VIFData *)pVIF->drv_priv;
  STAData *pSTAData = (STAData *)pSta->drv_priv;

  if ((NULL != pVIFData) && (NULL != pSTAData)) {
    if (true == pVIFData->bActive) {
      pSTAData->bActive = false;
    }
  }

  KLEM_LOG("Called pHW(%p) pVIFData(%p) pSTAData(%p)\n",
       pHW, pVIFData, pSTAData);

  return 0;
}

static void privStaNotify(struct ieee80211_hw *pHW,
              struct ieee80211_vif *pVIF,
              enum sta_notify_cmd eCmd,
              struct ieee80211_sta *pSta)
{
  /* Not really using this function, record it was called. */
  KLEM_LOG("Called Command(%d) pHW(%p) pVIF(%p) pSta(%p)\n",
       eCmd, pHW, pVIF, pSta);
}

/*
 * Common Transmit sk_buff through wireless device
 */
static inline void privCommonTX(struct ieee80211_hw *pHW, struct sk_buff *pSkb)
{
  mac80211Data *pMacData = (mac80211Data *)pHW->priv;
  struct ieee80211_hdr *pHdr = NULL;
  unsigned int uqos = 0;
  unsigned long uSigFlags;
  
  if ((NULL != pMacData) && (NULL != pSkb)) {
    if (true == pMacData->bRadioActive) {
      if (pSkb->len < 10) {
        KLEM_MSG(" drop this small packet\n");
        dev_kfree_skb(pSkb);
      } else {
        pHdr = (struct ieee80211_hdr *)pSkb->data;
        
        /* Determine the qos mapping */
        if (ieee80211_is_data_qos(pHdr->frame_control)) {
          uqos = skb_get_queue_mapping(pSkb);
        }
        
        /*
         * Assuming we have a valid qos, enqueue the packet onto our
         * fake hardware.
         */
        if (uqos < KLEM_MAX_QOS) {
          spin_lock_irqsave(&pMacData->sSpinLock, uSigFlags);
          
          if (pMacData->qos [uqos].uListDepth < KLEM_QUEUE_MAX) {
            skb_queue_tail(&pMacData->qos [uqos].listSkb,
                           pSkb);
            pMacData->qos [uqos].uListDepth++;
            /* Check to see if are close to high water mark for queue storage.*/
            if (pMacData->qos [uqos].uListDepth >= KLEM_QUEUE_HIGH) {
              if (true == pMacData->qos [uqos].bListActive) {
                /* Mainly due this, so a network issue don't consume all mem */
                pMacData->qos [uqos].bListActive = false;
                ieee80211_stop_queue(pMacData->pHW, uqos);
              }
            }
          } else {
            /* Our fake hardware ran out of storage space, drop packet */
            pMacData->qos [uqos].uSendDroppedNumber++;
          }
          
          spin_unlock_irqrestore(&pMacData->sSpinLock, uSigFlags);
          
          /* Added the packet, wake a potential sleeper. */
          wake_up_all(&pMacData->sListWait);
        }
      }
    } else {
      privCompleteTX(pMacData, pSkb, false);
    }
  } else {
    /* THis shouldn't be executed */
    KLEM_LOG("Packet TX called incorrectly. pMacData(%p) pSkb(%p)\n",
             pMacData, pSkb);
  }
}

/*
 * transmit an sk_buff through wireless device interface */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0))
static void privTX(struct ieee80211_hw *pHW,
           struct ieee80211_tx_control *pControl,
           struct sk_buff *pSkb)
{
  privCommonTX(pHW, pSkb);
}
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39))
static void privTX(struct ieee80211_hw *pHW, struct sk_buff *pSkb)
{
  privCommonTX(pHW, pSkb);
}
#else
static int privTX(struct ieee80211_hw *pHW, struct sk_buff *pSkb)
{
  privCommonTX(pHW, pSkb);

  return NETDEV_TX_OK;
}
#endif

static int privConfig(struct ieee80211_hw *pHW, u32 uChanged)
{
  mac80211Data *pMacData = NULL;

  if (NULL != pHW) {
    pMacData = (mac80211Data *)pHW->priv;

    if (NULL != pMacData) {
      pMacData->bIdle = !!(pHW->conf.flags & IEEE80211_CONF_IDLE);
    } else {
      KLEM_LOG("Error pMacData (%p)\n", pMacData);
    }
  } else {
      KLEM_LOG("Error pHW (%p) uChanged(%d)\n", pHW, uChanged);
  }

  return 0;
}


/*
 * Configuration Filter
 * Allow BSS to be in promiscuous mode
 * pass al multicast frames
 */
static void privConfigureFilter(struct ieee80211_hw *pHW,
                unsigned int uChFlags,
                unsigned int *pTotalFlags,
                u64 uMcast)
{
  mac80211Data *pMacData = (mac80211Data *)pHW->priv;
  
  if ((NULL != pMacData) && (NULL != pTotalFlags)) {
    *pTotalFlags &= (FIF_PROMISC_IN_BSS | FIF_ALLMULTI);
  } else {
    KLEM_LOG("Error pHW (%p) uChFlags (%d) pTotalFlags (%p) uMcast (%lld)\n",
         pHW, uChFlags, pTotalFlags, uMcast);

  }
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0))
static int privConfigureTX(struct ieee80211_hw *pHW,
               struct ieee80211_vif *pVIF,
               u16 uQueue,
               const struct ieee80211_tx_queue_params *pQueue)
{
  mac80211Data *pMacData = NULL;

  if ((NULL != pHW) && (NULL != pQueue)) {
    if (NULL != pHW->priv) {
      pMacData = (mac80211Data *)pHW->priv;

      /* Remember the qos setup */
      pMacData->qos [uQueue].aifs = pQueue->aifs;
      pMacData->qos [uQueue].cw_min = pQueue->cw_min;
      pMacData->qos [uQueue].cw_max = pQueue->cw_max;
      pMacData->qos [uQueue].txop = pQueue->txop;

      KLEM_LOG("qos = %d, aifs = %d, cw_min = %d, cw_max = %d, txop = %d\n",
           uQueue,
           pQueue->aifs,
           pQueue->cw_min,
           pQueue->cw_max,
           pQueue->txop);
    }
  }

  return 0;
}
#else
static int privConfigureTX(struct ieee80211_hw *pHW,
               u16 uQueue,
               const struct ieee80211_tx_queue_params *pQueue)
{
  mac80211Data *pMacData = NULL;

  if ((NULL != pHW) && (NULL != pQueue)) {
    if (NULL != pHW->priv) {
      pMacData = (mac80211Data *)pHW->priv;

      /* Remember the qos setup */
      pMacData->qos [uQueue].aifs = pQueue->aifs;
      pMacData->qos [uQueue].cw_min = pQueue->cw_min;
      pMacData->qos [uQueue].cw_max = pQueue->cw_max;
      pMacData->qos [uQueue].txop = pQueue->txop;

      KLEM_LOG("qos = %d, aifs = %d, cw_min = %d, cw_max = %d, txop = %d\n",
           uQueue,
           pQueue->aifs,
           pQueue->cw_min,
           pQueue->cw_max,
           pQueue->txop);
    }
  }

  return 0;
}

#endif

/* function to define if packets are ready to transmit */
static int privQueuePoll(void *pPtr)
{
  mac80211Data *pMacData = (mac80211Data *)pPtr;
  int rvalue = KLEM_MAX_QOS;
  unsigned long uSigFlags;
  unsigned int loop;

  if (NULL != pMacData) {
    if (true == pMacData->bActive) {
      spin_lock_irqsave(&pMacData->sSpinLock, uSigFlags);
      for (loop = 0; loop < KLEM_MAX_QOS; loop++) {
        if (skb_queue_len(&pMacData->qos [loop].listSkb) > 0) {
          rvalue = loop;
          break;
        }
      }
      spin_unlock_irqrestore(&pMacData->sSpinLock, uSigFlags);
    } else {
      rvalue = 0;
    }
  }
  
  return rvalue;
}

/* Quick function to transmit a beacon */
void privBeaconTX(void *pPtr, u8 *mac,
          struct ieee80211_vif *pVIF)
{
  struct ieee80211_hw *pHW = (struct ieee80211_hw *)pPtr;
  mac80211Data *pMacData = (mac80211Data *)pHW->priv;
  KLEMData *pData = (KLEMData *)pMacData->pData;
  struct sk_buff *pSkb = NULL;
  KLEM_TAP_HEADER sTapHdr;

  /* is the mac 802.11 active? */
  if (true == pMacData->bActive) {
    if (true == pMacData->bRadioActive) {
      pSkb = ieee80211_beacon_get(pHW, pVIF);
      
      if (NULL != pSkb) {
        if (LEMU == pData->eMode) {
          /* Put in the information on band, frequency, etc */
          sTapHdr.uBand = htonl((u32)pMacData->pHW->conf.channel->band);
          sTapHdr.uFrequency =
            htonl((u32)pMacData->pHW->conf.channel->center_freq);
          sTapHdr.uPower = htonl((u32)pMacData->pHW->conf.power_level);
          sTapHdr.uId = htonl((u32)pData->uDeviceId);
          
          klemTransmit(pData->pRawSocket, pSkb,
                       (char *)&sTapHdr, sizeof(KLEM_TAP_HEADER));
        } else {
          klemTransmit(pData->pRawSocket, pSkb,
                       NULL, 0);
        }
        pMacData->uBeaconCount++;
        dev_kfree_skb(pSkb);
      }
    }
  }
}

/*
 * comlpete packet transmission
 */
static void privCompleteTX(void *pPtr, struct sk_buff *pSkb, bool bAck)
{
  mac80211Data *pMacData = (mac80211Data *)pPtr;
  struct ieee80211_tx_info *pTXResp = NULL;

  /* Drop that packet offically */
  skb_orphan(pSkb);
  skb_dst_drop(pSkb);
  pSkb->mark = 0;
  secpath_reset(pSkb);
  nf_reset(pSkb);

  pTXResp = IEEE80211_SKB_CB(pSkb);
  ieee80211_tx_info_clear_status(pTXResp);
  if ((0 == (pTXResp->flags & IEEE80211_TX_CTL_NO_ACK)) && (true == bAck)) {
    pTXResp->flags |= IEEE80211_TX_STAT_ACK;
  }
  ieee80211_tx_status_irqsafe(pMacData->pHW, pSkb);
}

/*
 * Take any queued packet and transmit it.
 *
 * Future: Needs better abstraction of extracting qos packets to emulate
 * a physical device.
 *
 */
static int privSendRawThread(void *pPtr)
{
  mac80211Data *pMacData = (mac80211Data *)pPtr;
  KLEMData *pData = pMacData->pData;
  unsigned int uqos = 0;
  struct sk_buff *pSkb = NULL;
  KLEM_TAP_HEADER sTapHdr;
  unsigned long ctime;

  set_user_nice(current, -20);
  
  ctime = jiffies + pMacData->uBeacons;
  while ((false == kthread_should_stop()) &&
         (false != pMacData->bActive)) {
    
    /* Wait until we have something to do. */
    wait_event_interruptible_timeout(pMacData->sListWait,
                                     ((uqos = privQueuePoll(pMacData)) < KLEM_MAX_QOS),
                                     pMacData->uBeacons);
    
    if (true == pMacData->bActive) {
      if (false == pMacData->bIdle) {
        if (true == pMacData->bRadioActive) {
          if (jiffies < ctime) {
            if (uqos < KLEM_MAX_QOS) {
              /* remove a packet from the list. */
              spin_lock(&pMacData->sSpinLock);
              
              /* We have the qos position from the privQueuePoll above. */
              pSkb = skb_dequeue(&pMacData->qos [uqos].listSkb);
              
              /* We should always have a packet here, but check anyways. */
              if (NULL != pSkb) {
                pMacData->qos [uqos].uListDepth--;
                if (pMacData->qos [uqos].bListActive == false) {
                  if (pMacData->qos [uqos].uListDepth <= KLEM_QUEUE_LOW) {
                    ieee80211_wake_queue(pMacData->pHW, uqos);
                    pMacData->qos [uqos].bListActive = true;
                  }
                }
              }
              
              if (uqos < KLEM_QUEUE_LOW) {
                pMacData->qos [uqos].uSendNumber++;
              }
              
              /* Done removing the packet, release this portion of data */
              spin_unlock(&pMacData->sSpinLock);
              
              if (NULL != pSkb) {
                if (LEMU == pData->eMode) {
                  /* Put in the information on band, frequency, etc */
                  sTapHdr.uBand = htonl((u32)pMacData->pHW->conf.channel->band);
                  sTapHdr.uFrequency =
                    htonl((u32)pMacData->pHW->conf.channel->center_freq);
                  sTapHdr.uPower = htonl((u32)pMacData->pHW->conf.power_level);
                  sTapHdr.uId = htonl((u32)pData->uDeviceId);
                  
                  /*
                   * Transmit that packet,
                   * and encapulate a mactap header.
                   */
                  klemTransmit(pData->pRawSocket, pSkb,
                               (char *)&sTapHdr, sizeof(KLEM_TAP_HEADER));
                } else {
                  KLEM_MSG("bridge send \n");
                  klemTransmit(pData->pRawSocket, pSkb, NULL, 0);
                }
                
                /* Indicate a success transmission */
                privCompleteTX(pMacData, pSkb, true);
              } else {
                /* Indicate a success transmission */
                privCompleteTX(pMacData, pSkb, false);
              }
            }
          } else {
            ieee80211_iterate_active_interfaces_atomic(pMacData->pHW,
                                                       privBeaconTX,
                                                       pMacData->pHW);
            ctime = jiffies + pMacData->uBeacons;
            pMacData->uBeaconCount++;
          }
        }
      }
    }
  }
  
  /* Clean up any packets the might here. */
  for (uqos = 0; uqos < KLEM_MAX_QOS; uqos++) {
    pSkb = skb_dequeue(&pMacData->qos [uqos].listSkb);
    while (NULL != pSkb) {
      dev_kfree_skb(pSkb);
      pSkb = skb_dequeue(&pMacData->qos [uqos].listSkb);
    }
  }
  
  while ((false == kthread_should_stop())) {
    msleep(1000);
    KLEM_MSG("Waiting to die\n");
  }
  
  /* We are dead. */
  
  return 0;
}

void klem80211Recv(void *pPtr, struct sk_buff *pSkb)
{
  KLEMData *pData = (KLEMData *)pPtr;
  mac80211Data *pMacData = (mac80211Data *)pData->pMacData;
  KLEM_TAP_HEADER *pTapHdr = (KLEM_TAP_HEADER *)pSkb->data;
  struct ieee80211_rx_status recvStat;
  struct ieee80211_hdr *pWHdr = NULL;
  unsigned int uqos = 0;
  unsigned int utmp;
  struct sk_buff *pTmpSkb = pSkb;
  bool bRecvFlag = false;
  
  if (NULL != pMacData) {
    if (true == pMacData->bRadioActive) {
      if (LEMU == pData->eMode) {
        memset(&recvStat, 0, sizeof(recvStat));
        
        /* Get the needed recv information. */
        recvStat.band = ntohl(pTapHdr->uBand);
        recvStat.freq = ntohl(pTapHdr->uFrequency);
        recvStat.signal = ntohl(pTapHdr->uPower);
        recvStat.rate_idx = 1;
        
        /* Get the wireless header */
        pWHdr = (struct ieee80211_hdr *)skb_pull(pTmpSkb,
                                                 sizeof(KLEM_TAP_HEADER));
        
        utmp = ntohl(pTapHdr->uId);
        if (utmp < MAX_WIRELESS_NODE) {
          if (false == pData->bFilterNode [utmp]) {
            memcpy(IEEE80211_SKB_RXCB(pTmpSkb), &recvStat, sizeof(recvStat));
            bRecvFlag = true;
          }
        }
      } else {
        memset(&recvStat, 0, sizeof(recvStat));
        
        /* Put in the information on band, frequency, etc */
        recvStat.band = (u32)pMacData->pHW->conf.channel->band;
        recvStat.freq = (u32)pMacData->pHW->conf.channel->center_freq;
        recvStat.signal = (u32)pMacData->pHW->conf.power_level;
        recvStat.rate_idx = 1;
        
        /* Get the wireless header */
        pWHdr = (struct ieee80211_hdr *)pTmpSkb->data;
        memcpy(IEEE80211_SKB_RXCB(pTmpSkb), &recvStat, sizeof(recvStat));
        bRecvFlag = true;
      }
      
      if (true == bRecvFlag) {
        /* Remove the Data/tap header */
        if (ieee80211_is_data_qos(pWHdr->frame_control)) {
          switch(*((u8*)ieee80211_get_qos_ctl(pWHdr)) &
                 IEEE80211_QOS_CTL_TID_MASK)
            {
            case 6:
            case 7:
              uqos = 0;
              break;
            case 4:
            case 5:
              uqos = 1;
              break;
            case 0:
            case 3:
              uqos = 2;
              break;
            case 1:
            case 2:
              uqos = 3;
              break;
            default:
              uqos = 0;
              break;
            }
        }
        
        pMacData->qos [uqos].uRecvNumber++;
        ieee80211_rx_irqsafe(pMacData->pHW, pTmpSkb);
        pTmpSkb = NULL;
        bRecvFlag = false;
        
      }
    }
  }
  
  /* If we stillhave the sk buffer, free it.  it was rejected. */
  if (NULL != pTmpSkb) dev_kfree_skb(pTmpSkb);
}

/*
 * Called from proc, to output 802.11 specific data.
 * !Warning, be sure pOutput points to enough space
 * or you have memory overwrite.
 *
 */
unsigned int klem80211Proc(void *pPtr, char *pOutput)
{
  KLEMData *pData = (KLEMData *)pPtr;
  mac80211Data *pMacData = NULL;
  unsigned int rvalue = 0;
  unsigned int tmp = 0;
  int loop;

  if (NULL != pData) {
    pMacData = (mac80211Data *)pData->pMacData;
    if (NULL != pMacData) {
      if (NULL != pOutput) {
        sprintf(pOutput, "MAC Address:          %pM\n",
                (void *)&pMacData->macAddress);
        tmp = strlen(pOutput);
        pOutput += tmp;
        rvalue += tmp;
        
        sprintf(pOutput, "beacon count :         %ld\n",
                pMacData->uBeaconCount);
        tmp = strlen(pOutput);
        pOutput += tmp;
        rvalue += tmp;
        
        for (loop = 0; loop < KLEM_MAX_QOS; loop++) {
          sprintf(pOutput, "qos [%d] aifs:         %d\n", loop,
                  pMacData->qos [loop].aifs);
          tmp = strlen(pOutput);
          pOutput += tmp;
          rvalue += tmp;
          sprintf(pOutput, "qos [%d] cw_min:       %d\n", loop,
                  pMacData->qos [loop].cw_min);
          tmp = strlen(pOutput);
          pOutput += tmp;
          rvalue += tmp;
          sprintf(pOutput, "qos [%d] cw_max:       %d\n", loop,
                  pMacData->qos [loop].cw_max);
          tmp = strlen(pOutput);
          pOutput += tmp;
          rvalue += tmp;
          sprintf(pOutput, "qos [%d] txop:         %d\n", loop,
                  pMacData->qos [loop].txop);
          tmp = strlen(pOutput);
          pOutput += tmp;
          rvalue += tmp;
          sprintf(pOutput, "qos [%d] recv:         %ld\n", loop,
                  pMacData->qos [loop].uRecvNumber);
          tmp = strlen(pOutput);
          pOutput += tmp;
          rvalue += tmp;
          sprintf(pOutput, "qos [%d] sent:         %ld\n", loop,
                  pMacData->qos [loop].uSendNumber);
          tmp = strlen(pOutput);
          pOutput += tmp;
          rvalue += tmp;
          sprintf(pOutput, "qos [%d] sent error:   %ld\n", loop,
                  pMacData->qos [loop].uSendErrorNumber);
          tmp = strlen(pOutput);
          pOutput += tmp;
          rvalue += tmp;
          sprintf(pOutput, "qos [%d] sent drop:    %ld\n", loop,
                  pMacData->qos [loop].uSendDroppedNumber);
          tmp = strlen(pOutput);
          pOutput += tmp;
          rvalue += tmp;
        }
      }
    }
  }
  
  return rvalue;
}

void klem80211Start(void *pPtr)
{
  KLEMData *pData = (KLEMData *)pPtr;
  mac80211Data *pMacData = NULL;
  struct ieee80211_hw *pHW = NULL;
  int loop;
  int err;

  KLEM_MSG("Start the mac802.11 Simulator\n");
  if (NULL != pData) {
    pHW = ieee80211_alloc_hw(sizeof(mac80211Data), &privKlem80211OPS);
    if (NULL == pHW) {
      KLEM_MSG("Failed to allocate ieee80211_alloc_hw\n");
      return;
    }

    /* We have mac80211 hardware area */
    pMacData = (mac80211Data *)pHW->priv;
    pMacData->pHW = pHW;
    pMacData->pData = pData;

    /* save the pointer to the mac radio in the general structure. */
    pData->pMacData = (void *)pMacData;

    sprintf(pMacData->devName, "klemMac80211");

    /* This is a GPL exported only function. */
    pMacData->pDev = device_create(pData->pClass,
                                   NULL,
                                   0,
                                   pMacData->pHW,
                                   pMacData->devName,
                                   0);

    if (IS_ERR(pMacData->pDev)) {
      KLEM_MSG("Failed to get create a wireless device\n");
      ieee80211_free_hw(pHW);
      pData->pMacData = NULL;
      return;
    }

    /* Continue to setup wireless driver */
    pMacData->bActive = true;
    pMacData->bRadioActive = false;
    pMacData->bIdle = true;
    pMacData->uBeacons = (1024 * HZ) >> 10;

    spin_lock_init(&pMacData->sSpinLock);
    init_waitqueue_head(&pMacData->sListWait);
    pMacData->uBeaconCount = 0;

    for (loop = 0; loop < KLEM_MAX_QOS; loop++) {
      pMacData->qos [loop].aifs = 0;
      pMacData->qos [loop].cw_min = 0;
      pMacData->qos [loop].cw_max = 0;
      pMacData->qos [loop].txop = 0;
      pMacData->qos [loop].uRecvNumber = 0;
      pMacData->qos [loop].uSendNumber = 0;
      pMacData->qos [loop].uSendErrorNumber = 0;
      pMacData->qos [loop].uSendDroppedNumber = 0;
      pMacData->qos [loop].uListDepth = 0;

      /* We need to create a skb buffer queue */
      skb_queue_head_init(&pMacData->qos [loop].listSkb);
      pMacData->qos [loop].bListActive = true;
    }

    /* Specify the supported driver name. */
    pMacData->pDev->driver = &klem80211Driver;
    SET_IEEE80211_DEV(pMacData->pHW, pMacData->pDev);

    /* Setup hardware data */
    pMacData->pHW->channel_change_time = 1;
    pMacData->pHW->queues = KLEM_MAX_QOS;
    pMacData->pHW->wiphy->n_addresses = 1;

    /* Generate a mac address */
    random_ether_addr((u8 *)&pMacData->macAddress);
    pMacData->pHW->wiphy->addresses = &pMacData->macAddress;

    pMacData->pHW->wiphy->interface_modes = BIT(NL80211_IFTYPE_MESH_POINT) |
      BIT(NL80211_IFTYPE_ADHOC) |
      BIT(NL80211_IFTYPE_AP) |
      BIT(NL80211_IFTYPE_P2P_CLIENT) |
      BIT(NL80211_IFTYPE_P2P_GO) |
      BIT(NL80211_IFTYPE_STATION);

    pMacData->pHW->flags = IEEE80211_HW_MFP_CAPABLE |
      IEEE80211_HW_SIGNAL_DBM |
      IEEE80211_HW_SUPPORTS_STATIC_SMPS |
      IEEE80211_HW_SUPPORTS_DYNAMIC_SMPS |
      IEEE80211_HW_AMPDU_AGGREGATION;

    /* Lets not fake vif and sta for now */
    pMacData->pHW->vif_data_size = sizeof(VIFData);
    pMacData->pHW->sta_data_size = sizeof(STAData);

    /* copy memory for the channels and rate for 2g */
    memcpy(pMacData->channel_2g, privConstChannels_2ghz,
       sizeof(privConstChannels_2ghz));
    pMacData->band_2g.channels = pMacData->channel_2g;
    pMacData->band_2g.n_channels = CHANNEL_SIZE_2G;

    memcpy(pMacData->rate_2g, privConstBitRate_2g, sizeof(privConstBitRate_2g));
    pMacData->band_2g.bitrates = pMacData->rate_2g;
    pMacData->band_2g.n_bitrates = RATE_SIZE_2G;
    pMacData->band_2g.ht_cap.ht_supported = true;
    pMacData->band_2g.ht_cap.cap =
      IEEE80211_HT_CAP_SUP_WIDTH_20_40 |
      IEEE80211_HT_CAP_GRN_FLD |
      IEEE80211_HT_CAP_SGI_40 |
      IEEE80211_HT_CAP_DSSSCCK40;
    pMacData->band_2g.ht_cap.ampdu_factor = 0x3;
    pMacData->band_2g.ht_cap.ampdu_density = 0x6;
    memset(&pMacData->band_2g.ht_cap.mcs, 0,
       sizeof(pMacData->band_2g.ht_cap.mcs));
    pMacData->band_2g.ht_cap.mcs.rx_mask[0] = 0xff;
    pMacData->band_2g.ht_cap.mcs.rx_mask[1] = 0xff;
    pMacData->band_2g.ht_cap.mcs.tx_params = IEEE80211_HT_MCS_TX_DEFINED;
    pMacData->pHW->wiphy->bands[IEEE80211_BAND_2GHZ] = &pMacData->band_2g;

    /* copy memory for the channels and rate for 5g */
    memcpy(pMacData->channel_5g,
       privConstChannels_5ghz,
       sizeof(privConstChannels_5ghz));
    pMacData->band_5g.channels = pMacData->channel_5g;
    pMacData->band_5g.n_channels = CHANNEL_SIZE_5G;
    memcpy(pMacData->rate_5g, privConstBitRate_5g, sizeof(privConstBitRate_5g));
    pMacData->band_5g.bitrates = pMacData->rate_5g;
    pMacData->band_5g.n_bitrates = RATE_SIZE_5G;
    pMacData->band_5g.ht_cap.ht_supported = true;
    pMacData->band_5g.ht_cap.cap =
      IEEE80211_HT_CAP_SUP_WIDTH_20_40 |
      IEEE80211_HT_CAP_GRN_FLD |
      IEEE80211_HT_CAP_SGI_40 |
      IEEE80211_HT_CAP_DSSSCCK40;
    pMacData->band_5g.ht_cap.ampdu_factor = 0x3;
    pMacData->band_5g.ht_cap.ampdu_density = 0x6;
    memset(&pMacData->band_2g.ht_cap.mcs, 0,
       sizeof(pMacData->band_2g.ht_cap.mcs));
    pMacData->band_2g.ht_cap.mcs.rx_mask[0] = 0xff;
    pMacData->band_5g.ht_cap.mcs.rx_mask[1] = 0xff;
    pMacData->band_5g.ht_cap.mcs.tx_params = IEEE80211_HT_MCS_TX_DEFINED;
    pMacData->pHW->wiphy->bands[IEEE80211_BAND_5GHZ] = &pMacData->band_5g;

    /* Need a name for the thread. */
    sprintf(pMacData->pSendName, "klemMacSend");
    pMacData->pSendThread = (void *)kthread_run(privSendRawThread,
                        (void *)pMacData,
                        pMacData->pSendName);

    err = ieee80211_register_hw(pMacData->pHW);
    if (err < 0) {
      KLEM_LOG("Failed to get register a wireless device (%d)\n", err);
      device_unregister(pMacData->pDev);
      ieee80211_free_hw(pMacData->pHW);
      pMacData = NULL;
    }
  }
}

void klem80211Stop(void *pPtr)
{
  KLEMData *pData = (KLEMData *)pPtr;
  mac80211Data *pMacData  = NULL;

  if (NULL != pData) {
    pMacData = (mac80211Data *)pData->pMacData;
    if (NULL != pMacData) {
      pMacData->bActive = false;
      pMacData->bRadioActive = false;

      if (NULL != pMacData->pSendThread) {
        kthread_stop((struct task_struct *)pMacData->pSendThread);
        pMacData->pSendThread = NULL;
      }

      if (NULL != pMacData->pHW) {
        ieee80211_unregister_hw(pMacData->pHW);
        if (NULL != pMacData->pDev) {
          device_unregister(pMacData->pDev);
        }
        ieee80211_free_hw(pMacData->pHW);
      }
      pData->pMacData = NULL;
    }
  }

  KLEM_MSG("Removed wireless device\n");
}

