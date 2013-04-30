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
#include <linux/in.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/net.h>
#include <linux/skbuff.h>
#include <linux/socket.h>
#include <linux/tcp.h>
#include <linux/delay.h>
#include <net/sock.h>
#include <asm-generic/errno.h>
#include <asm-generic/errno-base.h>
#include <linux/wait.h>
#include <linux/semaphore.h>
#include <linux/if.h>
#include <linux/inetdevice.h>
#include <linux/if_ether.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/ieee80211.h>

#include "klemData.h"
#include "klemHdr.h"
#include "klem80211.h"

#define MAX_RETRIES 256

/*
 * Private information about a connection we need to maintain.
 */
typedef struct raw_socket_def {
  struct socket *pSocket;
  KLEMData *pData;
  bool bConnected;
  wait_queue_head_t recvQueue;
  void (*recvReady)(struct sock *, int);

  /* mac address we will need for the raw ethernet packet */
  char pDevMac [ETH_ALEN];
  char pLemuMac [ETH_ALEN];

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

static void privRecvReady(struct sock *pSocket, int iBytes)
{
  raw_socket *pRaw = (raw_socket *)pSocket->sk_user_data;

  if (NULL != pRaw) {
    if (NULL != pRaw->recvReady) {
      pRaw->recvReady(pSocket, iBytes);
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
    pRaw = kmalloc(sizeof(raw_socket), GFP_ATOMIC);

    if (NULL != pRaw) {
      pRaw->pSocket = NULL;
      pRaw->bConnected = false;

      /* Create our socket */
      rvalue = sock_create_kern(PF_PACKET,
                                SOCK_RAW,
                                htons(ETH_P_ALL),
                                &pRaw->pSocket);
      if ((rvalue >= 0) && (NULL != pRaw->pSocket)) {
        /* Need to reference our data structure */
        pRaw->pSocket->sk->sk_user_data = (void *)pRaw;

        /* Set 4 second timeout. Easy calculation. */
        pRaw->pSocket->sk->sk_sndtimeo = HZ << 2;
        pRaw->pSocket->sk->sk_rcvtimeo = HZ << 2;

        /* Yea, we can reuse this socket.  Needed? */
        pRaw->pSocket->sk->sk_reuse = 1;

        /* atomic allocation */
        pRaw->pSocket->sk->sk_allocation = GFP_ATOMIC;

        /* Create recv queue, before setting callbacks */
        init_waitqueue_head(&pRaw->recvQueue);

        /* We need a semaphore. */
        sema_init(&pRaw->sendWait, 1);

        /* Halt any callback */
        write_lock_bh(&pRaw->pSocket->sk->sk_callback_lock);

        /* Perhaps not needed, but remember our orignal data ready function. */
        pRaw->recvReady = pRaw->pSocket->sk->sk_data_ready;

        /* Our callback recv function. */
        pRaw->pSocket->sk->sk_data_ready = privRecvReady;

        /* Copy the header information */
        strncpy(pRaw->hdr.str, KLEM_NAME, 4);

        /* Set the protocol type */
        pRaw->uProtocol = KLEM_PROTOCOL;

        /* Set the protcol version */
        pRaw->uVersion = KLEM_INT_VERSION;

        /* Resume any callbacks */
        write_unlock_bh(&pRaw->pSocket->sk->sk_callback_lock);

        /* Find the device were will transmit the raw packet. */
        pDev = __dev_get_by_name(pRaw->pSocket->sk->__sk_common.skc_net,
                                 pDevLabel);
        if (NULL != pDev) {
          memcpy(pRaw->pDevMac, (char *)pDev->perm_addr, ETH_ALEN);
        } else {
          /* We failed, broadcasting it might work, lets try that. */
          KLEM_MSG("Didn't find network device, we will broadcast it");
          memset(pRaw->pDevMac, 0xff, ETH_ALEN);
        }

        /* Default the lemu to broadcast. */
        memset(pRaw->pLemuMac, 0xff, ETH_ALEN);

        /* Set the send/recv threads to null */
        pRaw->pRecvThread = NULL;

        /* Lets say we have a conenction now. */
        pRaw->bConnected = true;
      } else {
        KLEM_LOG("Error creating socket %s %d\n", pDevLabel, rvalue);
        kfree(pRaw);
        pRaw = NULL;
      }
    }
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

      /* Perhaps not needed, set to orignal callback */
      pRaw->pSocket->sk->sk_data_ready = pRaw->recvReady;

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
 * Fairly generic implementation for sending a packet
 * should work for tcp, udp and raw connections.
 */
static unsigned int privSocketSend(void *pPtr,
                                   struct iovec *pVec,
                                   unsigned int uVecLength,
                                   unsigned int uDataLength,
                                   void *pName,
                                   unsigned int uNameLength)
{
  raw_socket *pRaw = (raw_socket *)pPtr;
  unsigned long ulFlags;
  sigset_t sigSet;
  siginfo_t sigInfo;
  struct msghdr mHdr;
  unsigned int uSendLength = uDataLength;
  unsigned int uVecCurrent = 0;
  int iRetries;
  int iError;
  unsigned int rvalue = 0;

  if (NULL != pRaw) {
    /* allow sigkill */
    spin_lock_irqsave(&current->sighand->siglock, ulFlags);
    sigSet = current->blocked;
    sigfillset(&current->blocked);
    sigdelsetmask(&current->blocked, sigmask(SIGKILL));
    recalc_sigpending();
    spin_unlock_irqrestore(&current->sighand->siglock, ulFlags);

    mHdr.msg_control = NULL;
    mHdr.msg_controllen = 0;
    mHdr.msg_flags = MSG_NOSIGNAL | MSG_DONTWAIT;

    /* sockaddr stuff */
    mHdr.msg_name = pName;
    mHdr.msg_namelen = uNameLength;

    while ((true == pRaw->bConnected) && (uSendLength > 0)) {
      mHdr.msg_iov = &pVec [uVecCurrent];
      mHdr.msg_iovlen = uVecLength - uVecCurrent;

      iRetries = 0;
      iError = 0;
      while ((iRetries++ < MAX_RETRIES) && (true == pRaw->bConnected)) {
        /* Finally the kernel does it magic, */
        iError = sock_sendmsg(pRaw->pSocket,
                              &mHdr,
                              uSendLength);

        if (signal_pending(current)) {
          /* dequeue a sigkill and quiet. */
          spin_lock_irqsave(&current->sighand->siglock, ulFlags);
          dequeue_signal(current, &current->blocked, &sigInfo);
          spin_unlock_irqrestore(&current->sighand->siglock, ulFlags);
          break;
        }

        switch(iError)
          {
          case -EAGAIN:
          case -ENOSPC:
            /* For these errors, les sleep then try again. */
            msleep_interruptible(32 << (iRetries % 4));
            break;
          case 0:
            /* Generic TCP has issues, copied from cifs */
            KLEM_MSG("Recv TCP size issue\n");
            msleep_interruptible(500);
            break;
          default:
            /* must have gotten something more interesting, don't try again */
            iRetries = MAX_RETRIES;
            break;
        }
      }

      /* Did we send any data? consider simplification*/
      if (iError > 0) {
        if (iError >= uSendLength) {
          /* All sent, full write */
          uSendLength -= iError;
          rvalue += iError;
        } else {
          /* fix that partial write */
          while ((uVecCurrent < uVecLength) && (iError > 0)) {
            if (iError >= pVec [uVecCurrent].iov_len) {
              /* We have consumed an entire iov */
              uSendLength -= pVec [uVecCurrent].iov_len;
              iError -= pVec [uVecCurrent].iov_len;
              rvalue += pVec [uVecCurrent].iov_len;
              uVecCurrent += 1;
            } else {
              /* Partial iov consumed case */
              pVec [uVecCurrent].iov_len -= iError;
              pVec [uVecCurrent].iov_base += iError;
              uSendLength -= iError;
              rvalue += iError;
              iError = 0;
            }
          }
        }
      }        else {
        /* No data was sent, this is an error. */
        KLEM_LOG("send error %d\n", iError);
        rvalue = 0;
        uSendLength = 0;
      }
    }

    /* no more sigkill. */
    spin_lock_irqsave(&current->sighand->siglock, ulFlags);
    current->blocked = sigSet;
    recalc_sigpending();
    spin_unlock_irqrestore(&current->sighand->siglock, ulFlags);
  }

  return rvalue;
}

/*
 * Poll to see if anything is ready to recv.
*/
static int privRecvPoll(void *pPtr)
{
  raw_socket *pRaw = (raw_socket *)pPtr;
  int rvalue = 0;

  if (NULL != pRaw) {
    if (true == pRaw->bConnected) {
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

  int rvalue = 0;

  if (NULL != pRaw)  {
    wait_event_interruptible(pRaw->recvQueue,
                             ((rvalue = privRecvPoll(pPtr)) > 0));
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
        pHdr = (KLEM_RAW_HEADER *)pSkb->data;

        if (LEMU == pRaw->pData->eMode) {
          if (pSkb->len >= uHdrLen) {
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
          dev_kfree_skb(pSkb);
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

/* Add a recv callback !!!! */

void *klemNetConnect(void *pPtr, char *pDevLabel)
{
  KLEMData *pData = (KLEMData *)pPtr;
  raw_socket *pRaw = NULL;

  KLEM_LOG("Create Raw Socket %s\n", pDevLabel);
  pRaw = privCreateRaw(pDevLabel);

  if (NULL != pRaw) {
    sprintf(pRaw->pRecvName, "klem-%s-recv", pDevLabel);
    pRaw->pData = pData;
    pRaw->pRecvThread = (void *)kthread_run(privateRecvRawThread,
                                            (void *)pRaw,
                                            pRaw->pRecvName);
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
  struct iovec sioVec [4];
  unsigned int rvalue = 0;
  unsigned int uError;
  unsigned int uvloc = 0;
  unsigned int uvsize = 0;

  if (NULL != pRaw) {
    if (true == pRaw->bConnected) {
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
      llAddr.sll_family = htons(PF_PACKET);
      llAddr.sll_protocol = htons(pRaw->uProtocol);
      llAddr.sll_halen = 6;
      llAddr.sll_ifindex = 2;

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

