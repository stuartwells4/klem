/*
 * Wireless Kernel Link Emulator
 *
 * Copyright (C) 2013 Stuart Wells <swells@stuartwells.net>
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
#include <linux/in.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/net.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/socket.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/if_packet.h>
#include <net/sock.h>
#include <net/net_namespace.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/semaphore.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/ieee80211.h>

#include "klemData.h"
#include "klemHdr.h"
#include "klem80211.h"
#include "klemNet.h"

#define MAX_RETRIES 256

/*
 * Private information about a connection we need to maintain.
 */
typedef struct raw_socket_def {
  struct socket *pSocket;
  KLEMData *pData;
  bool bConnected;
  wait_queue_head_t recvQueue;
  void (*recvReady)(struct sock *);

  /* mac address we will need for the raw ethernet packet */
  char pDevMac [ETH_ALEN];
  char pLemuMac [ETH_ALEN];

  /* Interface index of the wired device we transmit on. */
  int iIfIndex;

  /* make send thread safe. */
  struct semaphore sendWait;
  u16 uProtocol;

  /* Information to keep track of the sending / recieving threads */
  char pRecvName [64];
  void *pRecvThread;
  union {
    char str [4];
    u32 ui;
  } hdr;
  u32 uVersion;
} raw_socket;

static void privRecvReady(struct sock *pSocket)
{
  raw_socket *pRaw = (raw_socket *)pSocket->sk_user_data;

  if (NULL != pRaw) {
    if (NULL != pRaw->recvReady) {
      pRaw->recvReady(pSocket);
    }

    /* Wake up */
    wake_up_all(&pRaw->recvQueue);
  }
}

/*
 * Create a raw connection on a network device
 */
static void *privCreateRaw(char *pDevLabel)
{
  raw_socket *pRaw = NULL;
  int rvalue;
  struct net_device *pDev = NULL;

  if (NULL != pDevLabel) {
    /* Find the device were will transmit the raw packet. */
    pDev = dev_get_by_name(&init_net, pDevLabel);
    if (NULL == pDev) {
      KLEM_LOG("Network device %s not found\n", pDevLabel);
      return NULL;
    }

    pRaw = kzalloc(sizeof(raw_socket), GFP_KERNEL);

    if (NULL != pRaw) {
      pRaw->pSocket = NULL;
      pRaw->bConnected = false;

      memcpy(pRaw->pDevMac, pDev->dev_addr, ETH_ALEN);
      pRaw->iIfIndex = pDev->ifindex;

      /* Create our socket (kernel socket in the initial namespace) */
      rvalue = sock_create_kern(&init_net,
                                PF_PACKET,
                                SOCK_RAW,
                                htons(ETH_P_ALL),
                                &pRaw->pSocket);

      if ((rvalue >= 0) && (NULL != pRaw->pSocket)) {
        /* Need to reference our data structure */
        pRaw->pSocket->sk->sk_user_data = (void *)pRaw;

        /* Set 4 second timeout. Easy calculation. */
        pRaw->pSocket->sk->sk_sndtimeo = HZ << 2;
        pRaw->pSocket->sk->sk_rcvtimeo = HZ << 2;

        /* atomic allocation */
        pRaw->pSocket->sk->sk_allocation = GFP_ATOMIC;

        /* Create recv queue, before setting callbacks */
        init_waitqueue_head(&pRaw->recvQueue);

        /* We need a semaphore. */
        sema_init(&pRaw->sendWait, 1);

        /* Copy the header information */
        memcpy(pRaw->hdr.str, KLEM_NAME, 4);

        /* Set the protocol type */
        pRaw->uProtocol = KLEM_PROTOCOL;

        /* Set the protcol version */
        pRaw->uVersion = KLEM_INT_VERSION;

        /* Default the lemu to broadcast. */
        memset(pRaw->pLemuMac, 0xff, ETH_ALEN);

        /* Set the send/recv threads to null */
        pRaw->pRecvThread = NULL;

        /* Lets say we have a conenction now. */
        pRaw->bConnected = true;

        /* Halt any callback */
        write_lock_bh(&pRaw->pSocket->sk->sk_callback_lock);

        /* Remember our orignal data ready function. */
        pRaw->recvReady = pRaw->pSocket->sk->sk_data_ready;

        /* Our callback recv function. */
        pRaw->pSocket->sk->sk_data_ready = privRecvReady;

        /* Resume any callbacks */
        write_unlock_bh(&pRaw->pSocket->sk->sk_callback_lock);
      } else {
        KLEM_LOG("Error creating socket %s %d\n", pDevLabel, rvalue);
        kfree(pRaw);
        pRaw = NULL;
      }
    }

    dev_put(pDev);
  }

  return pRaw;
}

/*
 * Remove everything about a raw connections.
 */
static void privDestroyRaw(void *pPtr)
{
  raw_socket *pRaw = (raw_socket *)pPtr;

  if (NULL != pRaw) {
    /* We are not connected now */
    pRaw->bConnected = false;

    if (NULL != pRaw->pSocket) {
      /* Wake up everyone, so they go away. */
      wake_up_all(&pRaw->recvQueue);

      /* Halt any callback */
      write_lock_bh(&pRaw->pSocket->sk->sk_callback_lock);

      /* Set to orignal callback */
      pRaw->pSocket->sk->sk_data_ready = pRaw->recvReady;
      pRaw->pSocket->sk->sk_user_data = NULL;

      /* Resume any callbacks */
      write_unlock_bh(&pRaw->pSocket->sk->sk_callback_lock);

      /* Release that socket into the wild. */
      sock_release(pRaw->pSocket);
      pRaw->pSocket = NULL;
    }

    kfree(pRaw);
  }
}

/*
 * Send a raw datagram.  Packet sockets send the whole datagram or
 * nothing, so there is no partial write handling.
 */
static unsigned int privSocketSend(void *pPtr,
                                   struct kvec *pVec,
                                   unsigned int uVecLength,
                                   unsigned int uDataLength,
                                   void *pName,
                                   unsigned int uNameLength)
{
  raw_socket *pRaw = (raw_socket *)pPtr;
  struct msghdr mHdr;
  int iRetries = 0;
  int iError = 0;

  if (NULL == pRaw) {
    return 0;
  }

  while ((iRetries++ < MAX_RETRIES) && (true == pRaw->bConnected)) {
    memset(&mHdr, 0, sizeof(mHdr));
    mHdr.msg_flags = MSG_NOSIGNAL | MSG_DONTWAIT;
    mHdr.msg_name = pName;
    mHdr.msg_namelen = uNameLength;

    iError = kernel_sendmsg(pRaw->pSocket, &mHdr, pVec, uVecLength,
                            uDataLength);

    if ((-EAGAIN == iError) || (-ENOBUFS == iError) || (-ENOSPC == iError)) {
      /* For these errors, lets sleep then try again. */
      msleep_interruptible(32 << (iRetries % 4));
      continue;
    }
    break;
  }

  if (iError > 0) {
    return (unsigned int)iError;
  }

  KLEM_LOG("send error %d\n", iError);
  return 0;
}

/*
 * Poll to see if anything is ready to recv.
*/
static int privRecvPoll(void *pPtr)
{
  raw_socket *pRaw = (raw_socket *)pPtr;
  int rvalue = 0;

  if (NULL != pRaw) {
    if ((true == pRaw->bConnected) && (false == kthread_should_stop())) {
      rvalue = skb_queue_len(&pRaw->pSocket->sk->sk_receive_queue);
    } else {
      /* Fake out, if were trying to disconnect, say we have something. */
      rvalue = 1;
    }
  }

  return rvalue;
}

/*
 *  Wait until we have a packet.
*/
static void privRecvWait(void *pPtr)
{
  raw_socket *pRaw = (raw_socket *)pPtr;

  if (NULL != pRaw)  {
    wait_event_interruptible(pRaw->recvQueue, (privRecvPoll(pPtr) > 0));
  }
}

static int privateRecvRawThread(void *pPtr)
{
  raw_socket *pRaw = (raw_socket *)pPtr;
  struct sk_buff *pSkb = NULL;
  KLEM_RAW_HEADER *pHdr = NULL;
  unsigned int uHdrLen = sizeof(KLEM_RAW_HEADER);

  set_user_nice(current, -20);

  while ((false == kthread_should_stop()) &&
         (false != pRaw->bConnected)) {
    /* Wait until we have a packet */
    privRecvWait(pRaw);

    if (true == pRaw->bConnected) {
      /* Get the packet. */
      pSkb = skb_dequeue(&pRaw->pSocket->sk->sk_receive_queue);

      if (NULL != pSkb) {
        /*
         * Detach from the socket's receive accounting, the buffer
         * may outlive the socket once handed to mac80211.
         */
        skb_orphan(pSkb);

        if (LEMU == pRaw->pData->eMode) {
          if ((pSkb->len >= uHdrLen) && (0 == skb_linearize(pSkb))) {
            /* Lets examine the incomming header */
            pHdr = (KLEM_RAW_HEADER *)pSkb->data;

            if (pRaw->uProtocol == ntohs(pHdr->uProtocol)) {
              if (pRaw->hdr.ui == ntohl(pHdr->uHeader)) {
                if (pRaw->uVersion == ntohl(pHdr->uVersion)) {

                  /* remove the header */
                  skb_pull(pSkb, uHdrLen);

                  /* call the klem80211 side, to recv packet. */
                  klem80211Recv(pRaw->pData, pSkb);

                  /* I know nothing */
                  pSkb = NULL;
                }
              }
            }
          }
        } else {
          /* call the klem80211 side, to recv packet. */
          klem80211Recv(pRaw->pData, pSkb);

          /* I know nothing */
          pSkb = NULL;
        }

        /* IF were are here, free that buffer, its not ours. */
        if (NULL != pSkb) {
          kfree_skb(pSkb);
          pSkb = NULL;
        }
      }
    }
  }

  /* Wait to die */
  while ((false == kthread_should_stop())) {
    msleep(1000);
    KLEM_MSG("Waiting to die\n");
  }

  /* We are dead. */

  return 0;
}

void *klemNetConnect(void *pPtr, char *pDevLabel)
{
  KLEMData *pData = (KLEMData *)pPtr;
  raw_socket *pRaw = NULL;
  struct task_struct *pTask = NULL;

  KLEM_LOG("Create Raw Socket %s\n", pDevLabel);
  pRaw = privCreateRaw(pDevLabel);

  if (NULL != pRaw) {
    snprintf(pRaw->pRecvName, sizeof(pRaw->pRecvName),
             "klem-%s-recv", pDevLabel);
    pRaw->pData = pData;
    pTask = kthread_run(privateRecvRawThread, (void *)pRaw, "%s",
                        pRaw->pRecvName);
    if (IS_ERR(pTask)) {
      KLEM_LOG("Failed to start recv thread %ld\n", PTR_ERR(pTask));
      privDestroyRaw(pRaw);
      pRaw = NULL;
    } else {
      pRaw->pRecvThread = (void *)pTask;
    }
  }

  return (void *)pRaw;
}

void klemNetDisconnect(void *pPtr)
{
  raw_socket *pRaw = (raw_socket *)pPtr;

  if (NULL != pRaw) {
    /* Lets make the socket inactive. */
    pRaw->bConnected = false;

    /* Wake everyone up */
    wake_up_all(&pRaw->recvQueue);
    if (NULL != pRaw->pRecvThread) {
      kthread_stop((struct task_struct *)pRaw->pRecvThread);
      pRaw->pRecvThread = NULL;
    }

    privDestroyRaw(pRaw);
  }
}

/*
 * Send the contents of an sk_buff raw on a network device.
 */
unsigned int klemTransmit(void *pPtr,
                          struct sk_buff *pSkb,
                          char *pHdr, unsigned int uHdrSize)
{
  raw_socket *pRaw = (raw_socket *)pPtr;
  KLEM_RAW_HEADER khdr;
  struct sockaddr_ll llAddr;
  struct kvec sioVec [4];
  unsigned int rvalue = 0;
  unsigned int uError;
  unsigned int uvloc = 0;
  unsigned int uvsize = 0;

  if (NULL != pRaw) {
    if (true == pRaw->bConnected) {
      /* mac80211 may hand us fragmented buffers, flatten them. */
      if (0 != skb_linearize(pSkb)) {
        return 0;
      }

      /* Take semaphore, but allow ourselves to be blocked. */
      if (down_interruptible(&pRaw->sendWait)) {
        /* We got a bad signal, don't send anything. */
        return 0;
      }

      if (LEMU == pRaw->pData->eMode) {
        /* Let everyone know our mac. */
        memcpy(khdr.pSrcMac, pRaw->pDevMac, ETH_ALEN);

        /* Broadcast the packet */
        memcpy(khdr.pDstMac, pRaw->pLemuMac, ETH_ALEN);

        /* Setup the raw ethernet header */
        khdr.uProtocol = htons(pRaw->uProtocol);
        khdr.uHeader = htonl(pRaw->hdr.ui);
        khdr.uVersion = htonl(pRaw->uVersion);

        /* Point to the ethernet header + klem datagram stuff */
        sioVec [uvloc].iov_base = (char *)&khdr;
        sioVec [uvloc].iov_len = sizeof(KLEM_RAW_HEADER);
        uvsize += sioVec [uvloc].iov_len;
        uvloc++;
        if ((NULL != pHdr) && (0 != uHdrSize)) {
          sioVec [uvloc].iov_base = (char *)pHdr;
          sioVec [uvloc].iov_len = uHdrSize;
          uvsize += sioVec [uvloc].iov_len;
          uvloc++;
        }
      }

      /* Raw address family and protocol. */
      memset(&llAddr, 0, sizeof(llAddr));
      llAddr.sll_family = AF_PACKET;
      llAddr.sll_protocol = htons(pRaw->uProtocol);
      llAddr.sll_halen = ETH_ALEN;
      llAddr.sll_ifindex = pRaw->iIfIndex;

      /* Set the destination stuff. */
      memcpy(llAddr.sll_addr, pRaw->pLemuMac, ETH_ALEN);

      /* Now we need to point to our data in the sk_buffer */
      sioVec [uvloc].iov_base = (char *)pSkb->data;
      sioVec [uvloc].iov_len = pSkb->len;
      uvsize += sioVec [uvloc].iov_len;
      uvloc++;

      /* Do the work of sending that data. */
      uError = privSocketSend(pPtr,
                              sioVec,
                              uvloc,
                              uvsize,
                              (void *)&llAddr,
                              sizeof(struct sockaddr_ll));

      /* Set the return value, if greater then 0, to the packet size */
      if (uError > 0) {
        rvalue = pSkb->len;
      } else {
        rvalue = 0;
      }

      up(&pRaw->sendWait);
    }
  }

  return rvalue;
}
