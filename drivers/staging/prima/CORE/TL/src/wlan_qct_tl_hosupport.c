/*
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*===========================================================================

                       W L A N _ Q C T _ T L _ HOSUPPORT. C
                                               
  OVERVIEW:
  
  DEPENDENCIES: 

  Are listed for each API below. 
  
  
  Copyright (c) 2008 QUALCOMM Incorporated.
  All Rights Reserved.
  Qualcomm Confidential and Proprietary
===========================================================================*/

/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


   $Header$$DateTime$$Author$


  when        who     what, where, why
----------    ---    --------------------------------------------------------
02/19/09      lti     Vos trace fix
02/06/09      sch     Dereg Bug fix
12/11/08      sch     Initial creation

===========================================================================*/
#include "wlan_qct_tl.h" 
#include "wlan_qct_wda.h"
#if defined WLAN_FEATURE_NEIGHBOR_ROAMING
/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include "wlan_qct_tl_hosupport.h"
#include "wlan_qct_tli.h"
#include "tlDebug.h"
/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/
//#define WLANTL_HO_DEBUG_MSG
//#define WLANTL_HO_UTEST

#define WLANTL_HO_DEFAULT_RSSI      0xFF
#define WLANTL_HO_DEFAULT_ALPHA     5
#define WLANTL_HO_INVALID_RSSI      -100
/* RSSI sampling period, usec based
 * To reduce performance overhead
 * Current default 500msec */
#define WLANTL_HO_SAMPLING_PERIOD   500000



/* Get and release lock */
#define THSGETLOCK(a, b)                                      \
        do                                                    \
        {                                                     \
           if(!VOS_IS_STATUS_SUCCESS(vos_lock_acquire(b)))    \
           {                                                  \
              TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"%s Get Lock Fail", a));      \
              return VOS_STATUS_E_FAILURE;                    \
           }                                                  \
        }while(0)

#define THSRELEASELOCK(a, b)                                  \
        do                                                    \
        {                                                     \
           if(!VOS_IS_STATUS_SUCCESS(vos_lock_release(b)))    \
           {                                                  \
              TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"%s Release Lock Fail", a));  \
              return VOS_STATUS_E_FAILURE;                    \
           }                                                  \
        }while(0)

const v_U8_t  WLANTL_HO_TID_2_AC[WLAN_MAX_TID] = {WLANTL_AC_BE, 
                                                  WLANTL_AC_BK,
                                                  WLANTL_AC_BK, 
                                                  WLANTL_AC_BE,
                                                  WLANTL_AC_VI, 
                                                  WLANTL_AC_VI,
                                                  WLANTL_AC_VO,
                                                  WLANTL_AC_VO};
/*----------------------------------------------------------------------------
 *  Type Declarations
 * -------------------------------------------------------------------------*/
/* Temporary threshold store place for BMPS */
typedef struct
{
   v_S7_t  rssi;
   v_U8_t  event;
} WLANTL_HSTempPSIndType;

#ifdef RSSI_HACK
/* This is a dummy averaged RSSI value that can be controlled using dump commands 
 * to trigger TL to issue handoff related events. We will be using dump 362 <average RSSI> 
 * value to change its value */
int  dumpCmdRSSI = -48;
#endif

#ifdef WLANTL_HO_UTEST
/*==========================================================================

   FUNCTION

   DESCRIPTION 
    
   PARAMETERS 

   RETURN VALUE

============================================================================*/
static v_S7_t   rssi;
static v_S7_t   direction;
void TLHS_UtestHandleNewRSSI(v_S7_t *newRSSI, v_PVOID_t pAdapter)
{
   if(0 == rssi)
   {
      direction = -1;
   }
   else if(-90 == rssi)
   {
      direction = 1;
   }

   *newRSSI = rssi;
   rssi += direction;

   return;
}
#endif /* WLANTL_HO_UTEST */

#ifdef WLANTL_HO_DEBUG_MSG
/*==========================================================================

   FUNCTION

   DESCRIPTION 
    
   PARAMETERS 

   RETURN VALUE

============================================================================*/
void WLANTL_StatDebugDisplay
(
   v_U8_t                    STAid,
   WLANTL_TRANSFER_STA_TYPE *statistics
)
{
   TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"================================================="));
   TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"Statistics for STA %d", STAid));
   TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"RX UC Fcnt %5d, MC Fcnt %5d, BC Fcnt %5d",
                statistics->rxUCFcnt, statistics->rxMCFcnt, statistics->rxBCFcnt));
   TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"RX UC Bcnt %5d, MC Bcnt %5d, BC Bcnt %5d",
                statistics->rxUCBcnt, statistics->rxMCBcnt, statistics->rxBCBcnt));
   TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"TX UC Fcnt %5d, MC Fcnt %5d, BC Fcnt %5d",
                statistics->txUCFcnt, statistics->txMCFcnt, statistics->txBCFcnt));
   TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"TX UC Bcnt %5d, MC Bcnt %5d, BC Bcnt %5d",
                statistics->txUCBcnt, statistics->txMCBcnt, statistics->txBCBcnt));
   TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"TRX Bcnt %5d, CRCOK Bcnt %5d, RXRate %5d",
                statistics->rxBcnt, statistics->rxBcntCRCok, statistics->rxRate));
   TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"================================================="));
   return;
}
#endif /* WLANTL_HO_DEBUG_MSG */

/*==========================================================================

   FUNCTION

   DESCRIPTION 
    
   PARAMETERS 

   RETURN VALUE

============================================================================*/
void WLANTL_HSDebugDisplay
(
   v_PVOID_t pAdapter
)
{
   WLANTL_CbType                  *tlCtxt = VOS_GET_TL_CB(pAdapter);
   v_U8_t                          idx, sIdx;
   v_BOOL_t                        regionFound = VOS_FALSE;
   WLANTL_CURRENT_HO_STATE_TYPE   *currentHO;
   WLANTL_HO_SUPPORT_TYPE         *hoSupport;

   if (NULL == tlCtxt)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid TL Context",
                       __FUNCTION__));
      return;
   }

   currentHO = &(tlCtxt->hoSupport.currentHOState);
   hoSupport = &(tlCtxt->hoSupport);


   for(idx = 0; idx < currentHO->numThreshold; idx++)
   {
      if(idx == currentHO->regionNumber)
      {
         regionFound = VOS_TRUE;
         if(VOS_TRUE == tlCtxt->isBMPS)
         {
            TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO," ----> CRegion %d, hRSSI:NA, BMPS, Alpha %d",
                         currentHO->regionNumber, currentHO->alpha));
         }
         else
         {
            TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO," ----> CRegion %d, hRSSI %d, Alpha %d",
                         currentHO->regionNumber,
                         currentHO->historyRSSI,
                         currentHO->alpha));
         }
      }
      for(sIdx = 0; sIdx < WLANTL_HS_NUM_CLIENT; sIdx++)
      {
         if(NULL != hoSupport->registeredInd[idx].crossCBFunction[sIdx])
         {
            if(VOS_MODULE_ID_HDD == hoSupport->registeredInd[idx].whoIsClient[sIdx])
            {
               TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"Client HDD pCB %p, triggerEvt %d, RSSI %d",
                   hoSupport->registeredInd[idx].crossCBFunction[sIdx],
                             hoSupport->registeredInd[idx].triggerEvent[sIdx],
                             hoSupport->registeredInd[idx].rssiValue));
            }
            else
            {
               TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"Client SME pCB %p, triggerEvt %d, RSSI %d",
                             hoSupport->registeredInd[idx].crossCBFunction[sIdx],
                             hoSupport->registeredInd[idx].triggerEvent[sIdx],
                             hoSupport->registeredInd[idx].rssiValue));
            }
         }
      }
   }

   if(VOS_FALSE == regionFound)
   {
      if(VOS_TRUE == tlCtxt->isBMPS)
      {
         TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR," ----> CRegion %d, hRSSI:NA, BMPS, Alpha %d",
                      currentHO->regionNumber, currentHO->alpha));
      }
      else
      {
         TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR," ----> CRegion %d, hRSSI %d, Alpha %d",
                      currentHO->regionNumber,
                      currentHO->historyRSSI,
                      currentHO->alpha));
      }
   }

   return;
}

/*==========================================================================

   FUNCTION

   DESCRIPTION 
    
   PARAMETERS 

   RETURN VALUE

============================================================================*/
VOS_STATUS WLANTL_SetFWRSSIThresholds
(
   v_PVOID_t                       pAdapter
)
{
   WLANTL_CbType                  *tlCtxt = VOS_GET_TL_CB(pAdapter);
   VOS_STATUS                      status = VOS_STATUS_SUCCESS;
   WLANTL_HO_SUPPORT_TYPE         *hoSupport;
   WLANTL_CURRENT_HO_STATE_TYPE   *currentHO;
   tSirRSSIThresholds              bmpsThresholds;
   WLANTL_HSTempPSIndType          tempIndSet[WLANTL_SINGLE_CLNT_THRESHOLD];
   v_U8_t                          bmpsLoop;
   v_U8_t                          bmpsInd;
   v_U8_t                          clientLoop;

   if(NULL == tlCtxt)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Invalid TL handle"));
      return VOS_STATUS_E_INVAL;
   }

   WLANTL_HSDebugDisplay(pAdapter);
   currentHO = &(tlCtxt->hoSupport.currentHOState);
   hoSupport = &(tlCtxt->hoSupport);

   memset((v_U8_t *)&tempIndSet[0], 0, WLANTL_SINGLE_CLNT_THRESHOLD * sizeof(WLANTL_HSTempPSIndType));
   memset(&bmpsThresholds, 0, sizeof(tSirRSSIThresholds));

   bmpsInd = 0;
   for(bmpsLoop = 0; bmpsLoop < WLANTL_MAX_AVAIL_THRESHOLD; bmpsLoop++)
   {
      for(clientLoop = 0; clientLoop < WLANTL_HS_NUM_CLIENT; clientLoop++)
      {
         if(0 != hoSupport->registeredInd[bmpsLoop].triggerEvent[clientLoop])
         {
            if(bmpsInd == WLANTL_SINGLE_CLNT_THRESHOLD)
            {
               TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Single Client Threshold should be less than %d", WLANTL_SINGLE_CLNT_THRESHOLD));
               break;
            }
            tempIndSet[bmpsInd].rssi  = hoSupport->registeredInd[bmpsLoop].rssiValue;
            tempIndSet[bmpsInd].event = hoSupport->registeredInd[bmpsLoop].triggerEvent[clientLoop];
            bmpsInd++;
            break;
         }
      }
   }

   bmpsThresholds.ucRssiThreshold1 = tempIndSet[0].rssi;
   if((WLANTL_HO_THRESHOLD_DOWN == tempIndSet[0].event) ||
      (WLANTL_HO_THRESHOLD_CROSS == tempIndSet[0].event))
   {
      bmpsThresholds.bRssiThres1NegNotify = 1;
   }
   if((WLANTL_HO_THRESHOLD_UP == tempIndSet[0].event) ||
      (WLANTL_HO_THRESHOLD_CROSS == tempIndSet[0].event))
   {
      bmpsThresholds.bRssiThres1PosNotify = 1;
   }

   bmpsThresholds.ucRssiThreshold2 = tempIndSet[1].rssi;
   if((WLANTL_HO_THRESHOLD_DOWN == tempIndSet[1].event) ||
      (WLANTL_HO_THRESHOLD_CROSS == tempIndSet[1].event))
   {
      bmpsThresholds.bRssiThres2NegNotify = 1;
   }
   if((WLANTL_HO_THRESHOLD_UP == tempIndSet[1].event) ||
      (WLANTL_HO_THRESHOLD_CROSS == tempIndSet[1].event))
   {
      bmpsThresholds.bRssiThres2PosNotify = 1;
   }

   bmpsThresholds.ucRssiThreshold3 = tempIndSet[2].rssi;
   if((WLANTL_HO_THRESHOLD_DOWN == tempIndSet[2].event) ||
      (WLANTL_HO_THRESHOLD_CROSS == tempIndSet[2].event))
   {
      bmpsThresholds.bRssiThres3NegNotify = 1;
   }
   if((WLANTL_HO_THRESHOLD_UP == tempIndSet[2].event) ||
      (WLANTL_HO_THRESHOLD_CROSS == tempIndSet[2].event))
   {
      bmpsThresholds.bRssiThres3PosNotify = 1;
   }

   WDA_SetRSSIThresholds(hoSupport->macCtxt, &bmpsThresholds);
   return status;
}

/*==========================================================================

   FUNCTION

   DESCRIPTION 
    
   PARAMETERS 

   RETURN VALUE

============================================================================*/
VOS_STATUS WLANTL_StatHandleRXFrame
(
   v_PVOID_t        pAdapter,
   v_PVOID_t        pBDHeader,
   v_U8_t           STAid,
   v_BOOL_t         isBroadcast,
   vos_pkt_t       *dataBuffer   
)
{
   WLANTL_CbType            *tlCtxt = VOS_GET_TL_CB(pAdapter);
   VOS_STATUS                status = VOS_STATUS_SUCCESS;
   WLANTL_TRANSFER_STA_TYPE *statistics;
   v_U16_t                   packetSize;

   if(NULL == tlCtxt)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Invalid TL handle"));
      return VOS_STATUS_E_INVAL;
   }

   if(NULL == dataBuffer)
   {
      TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"Management Frame, not need to handle with Stat"));
      return status;
   }

   if(0 == tlCtxt->atlSTAClients[STAid].ucExists)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"WLAN TL: %d STA ID is not exist", STAid));
      return VOS_STATUS_E_INVAL;
   }

   /* TODO : BC/MC/UC have to be determined by MAC address */
   statistics = &tlCtxt->atlSTAClients[STAid].trafficStatistics;
   vos_pkt_get_packet_length(dataBuffer, &packetSize);
   if(WDA_IS_RX_BCAST(pBDHeader))
   {
      TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"This is RX BC/MC frame"));
      if(VOS_FALSE == isBroadcast)
      {
         TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"This is RX BC frame"));
         statistics->rxBCFcnt++;
         statistics->rxBCBcnt += (packetSize - WLANHAL_RX_BD_HEADER_SIZE);
      }
      else
      {
         TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"This is RX MC frame"));
         statistics->rxMCFcnt++;
         statistics->rxMCBcnt += (packetSize - WLANHAL_RX_BD_HEADER_SIZE);
      }
   }
   else
   {
      TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"This is RX UC frame"));
      statistics->rxUCFcnt++;
      statistics->rxUCBcnt += (packetSize - WLANHAL_RX_BD_HEADER_SIZE);
   }

   /* TODO caculation is needed, dimension of 500kbps */
   statistics->rxRate = WDA_GET_RX_MAC_RATE_IDX(pBDHeader);
   statistics->rxBcnt += (packetSize - WLANHAL_RX_BD_HEADER_SIZE);

#ifdef WLANTL_HO_DEBUG_MSG
   WLANTL_StatDebugDisplay(STAid, statistics);
#endif /* WLANTL_HO_DEBUG_MSG */

   return status;
}

/*==========================================================================

   FUNCTION

   DESCRIPTION 
    
   PARAMETERS 

   RETURN VALUE

============================================================================*/
VOS_STATUS WLANTL_StatHandleTXFrame
(
   v_PVOID_t        pAdapter,
   v_U8_t           STAid,
   vos_pkt_t       *dataBuffer,
   v_PVOID_t        pBDHeader
)
{
   WLANTL_CbType            *tlCtxt = VOS_GET_TL_CB(pAdapter);
   VOS_STATUS                status = VOS_STATUS_SUCCESS;
   WLANTL_TRANSFER_STA_TYPE *statistics;
   v_U16_t                   packetSize;

   if((NULL == tlCtxt) || (NULL == dataBuffer))
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Invalid TL handle"));
      return VOS_STATUS_E_INVAL;
   }

   if(0 == tlCtxt->atlSTAClients[STAid].ucExists)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"WLAN TL: %d STA ID is not exist", STAid));
      return VOS_STATUS_E_INVAL;
   }

   /* TODO : BC/MC/UC have to be determined by MAC address */
   statistics = &tlCtxt->atlSTAClients[STAid].trafficStatistics;
   vos_pkt_get_packet_length(dataBuffer, &packetSize);
   if(WLANTL_STA_ID_BCAST == STAid)
   {
      TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"This TX is BC frame"));
      statistics->txBCFcnt++;
      statistics->txBCBcnt += (packetSize - WLANHAL_TX_BD_HEADER_SIZE);
   }
/*
   if(WLANHAL_TX_BD_GET_UB(pBDHeader))
   {
      TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"This TX is BC/MC frame"));
      if(WLANTL_STA_ID_BCAST == STAid)
      {
         TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"This TX is BC frame"));
         statistics->txBCFcnt++;
         statistics->txBCBcnt += (packetSize - WLANHAL_TX_BD_HEADER_SIZE);
      }
      else
      {
         TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"This TX is MC frame"));
         statistics->txMCFcnt++;
         statistics->txMCBcnt += (packetSize - WLANHAL_RX_BD_HEADER_SIZE);
      }
   }
*/
   else
   {
      TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"This is TX UC frame"));
      statistics->txUCFcnt++;
      statistics->txUCBcnt += (packetSize - WLANHAL_RX_BD_HEADER_SIZE);
   }

#ifdef WLANTL_HO_DEBUG_MSG
   WLANTL_StatDebugDisplay(STAid, statistics);
#endif /* WLANTL_HO_DEBUG_MSG */

   return status;
}

/*==========================================================================

   FUNCTION  WLANTL_HSTrafficStatusTimerExpired

   DESCRIPTION  If traffic status monitoring timer is expiered,
                Count how may frames have sent and received during 
                measure period and if traffic status is changed
                send notification to Client(SME)
    
   PARAMETERS pAdapter
              Global handle

   RETURN VALUE

============================================================================*/
v_VOID_t WLANTL_HSTrafficStatusTimerExpired
(
   v_PVOID_t pAdapter
)
{
   WLANTL_CbType                        *tlCtxt = VOS_GET_TL_CB(pAdapter);
   WLANTL_HO_TRAFFIC_STATUS_HANDLE_TYPE *trafficHandle = NULL;
   WLANTL_HO_TRAFFIC_STATUS_TYPE         newTraffic;
   v_U32_t                               rtFrameCount;
   v_U32_t                               nrtFrameCount;
   v_BOOL_t                              trafficStatusChanged = VOS_FALSE;

   if(NULL == tlCtxt)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Invalid TL handle"));
      return;
   }

   /* Get rt and nrt frame count sum */
   trafficHandle = &tlCtxt->hoSupport.currentTraffic;
   rtFrameCount  = trafficHandle->rtRXFrameCount + trafficHandle->rtTXFrameCount;
   nrtFrameCount = trafficHandle->nrtRXFrameCount + trafficHandle->nrtTXFrameCount;

   TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"Traffic status timer expired RT FC %d, NRT FC %d", rtFrameCount, nrtFrameCount));

   /* Get current traffic status */
   if(rtFrameCount > trafficHandle->idleThreshold)
   {
      newTraffic.rtTrafficStatus = WLANTL_HO_RT_TRAFFIC_STATUS_ON;
   }
   else
   {
      newTraffic.rtTrafficStatus = WLANTL_HO_RT_TRAFFIC_STATUS_OFF;
   }

   if(nrtFrameCount > trafficHandle->idleThreshold)
   {
      newTraffic.nrtTrafficStatus = WLANTL_HO_NRT_TRAFFIC_STATUS_ON;
   }
   else
   {
      newTraffic.nrtTrafficStatus = WLANTL_HO_NRT_TRAFFIC_STATUS_OFF;
   }

   /* Differentiate with old traffic status */
   if(trafficHandle->trafficStatus.rtTrafficStatus != newTraffic.rtTrafficStatus)
   {
      TLLOGW(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,"RT Traffic status changed from %d to %d",
                   trafficHandle->trafficStatus.rtTrafficStatus,
                   newTraffic.rtTrafficStatus));
      trafficStatusChanged = VOS_TRUE;
   }
   if(trafficHandle->trafficStatus.nrtTrafficStatus != newTraffic.nrtTrafficStatus)
   {
      TLLOGW(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,"NRT Traffic status changed from %d to %d",
                   trafficHandle->trafficStatus.nrtTrafficStatus,
                   newTraffic.nrtTrafficStatus));
      trafficStatusChanged = VOS_TRUE;
   }

   /* If traffic status is changed send notification to client */
   if((VOS_TRUE == trafficStatusChanged) && (NULL != trafficHandle->trafficCB))
   {
      trafficHandle->trafficCB(pAdapter, newTraffic, trafficHandle->usrCtxt);
      trafficHandle->trafficStatus.rtTrafficStatus = newTraffic.rtTrafficStatus;
      trafficHandle->trafficStatus.nrtTrafficStatus = newTraffic.nrtTrafficStatus;
   }
   else if((VOS_TRUE == trafficStatusChanged) && (NULL == trafficHandle->trafficCB))
   {
      TLLOGW(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,"Traffic status is changed but not need to report"));
   }

   /* Reset frame counters */
   trafficHandle->rtRXFrameCount = 0;
   trafficHandle->rtTXFrameCount = 0;
   trafficHandle->nrtRXFrameCount = 0;
   trafficHandle->nrtTXFrameCount = 0;

   /* restart timer */
   vos_timer_start(&trafficHandle->trafficTimer, trafficHandle->measurePeriod);

   return;
}


/*==========================================================================

   FUNCTION

   DESCRIPTION 
    
   PARAMETERS 

   RETURN VALUE

============================================================================*/
VOS_STATUS WLANTL_HSGetRSSI
(
   v_PVOID_t        pAdapter,
   v_PVOID_t        pBDHeader,
   v_U8_t           STAid,
   v_S7_t          *currentAvgRSSI
)
{
   WLANTL_CbType   *tlCtxt = VOS_GET_TL_CB(pAdapter);
   VOS_STATUS       status = VOS_STATUS_SUCCESS;
   v_S7_t           currentRSSI, currentRSSI0, currentRSSI1;
   WLANTL_CURRENT_HO_STATE_TYPE *currentHO = NULL;


   if(NULL == tlCtxt)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Invalid TL handle"));
      return VOS_STATUS_E_INVAL;
   }

   /* 
      Compute RSSI only for the last MPDU of an AMPDU.
      Only last MPDU carries the Phy Stats Values 
   */
    if (WDA_IS_RX_AN_AMPDU (pBDHeader)) {
       if (!WDA_IS_RX_LAST_MPDU(pBDHeader)) {
           return VOS_STATUS_E_FAILURE;
          }
    }

   currentHO = &tlCtxt->hoSupport.currentHOState;

   currentRSSI0 = WLANTL_GETRSSI0(pBDHeader);
   currentRSSI1 = WLANTL_GETRSSI0(pBDHeader);
   currentRSSI  = (currentRSSI0 > currentRSSI1) ? currentRSSI0 : currentRSSI1;

   if (0 == currentRSSI)
      return VOS_STATUS_E_INVAL;

#ifdef WLANTL_HO_UTEST
   TLHS_UtestHandleNewRSSI(&currentRSSI, pAdapter);
#endif /* WLANTL_HO_UTEST */

/* Commenting this part of the code as this may not be necessarity true in all cases */
#if 0
   if(WLANTL_HO_INVALID_RSSI == currentRSSI)
   {
      return status;
   }
#endif

   if(0 == currentHO->historyRSSI)
   {
      *currentAvgRSSI = currentRSSI;
   }
   else
   {
      *currentAvgRSSI = ((currentHO->historyRSSI * currentHO->alpha) +
                         (currentRSSI * (10 - currentHO->alpha))) / 10;
   }
#ifdef RSSI_HACK
   *currentAvgRSSI = (v_S7_t)dumpCmdRSSI;
#endif


   tlCtxt->atlSTAClients[STAid].rssiAvg = *currentAvgRSSI;

   TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"Current new RSSI is %d, averaged RSSI is %d", currentRSSI, *currentAvgRSSI));
   return status;
}

/*==========================================================================

   FUNCTION

   DESCRIPTION 
    
   PARAMETERS 

   RETURN VALUE

============================================================================*/
VOS_STATUS WLANTL_HSBMPSRSSIRegionChangedNotification
(
   v_PVOID_t             pAdapter,
   tpSirRSSINotification pRSSINotification
)
{
   WLANTL_CbType                  *tlCtxt = VOS_GET_TL_CB(pAdapter);
   VOS_STATUS                      status = VOS_STATUS_SUCCESS;
   WLANTL_CURRENT_HO_STATE_TYPE   *currentHO;
   WLANTL_HO_SUPPORT_TYPE         *hoSupport;
   WLANTL_RSSICrossThresholdCBType cbFunction = NULL;
   v_PVOID_t                       usrCtxt = NULL;
   v_U8_t                          evtType = WLANTL_HO_THRESHOLD_NA;
   static v_U32_t                  preFWNotification = 0;
   v_U32_t                         curFWNotification = 0;
   v_U8_t                          newRegionNumber = 0;
   v_U8_t                          pRegionNumber = 0, nRegionNumber = 0;
   v_U32_t                         isSet;
   v_U8_t                          idx, sIdx;

   if(NULL == tlCtxt)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Invalid TL handle"));
      return VOS_STATUS_E_INVAL;
   }

   if(NULL == pRSSINotification)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Invalid FW RSSI Notification"));
      VOS_ASSERT(0);
      return VOS_STATUS_E_INVAL;
   }

   currentHO = &(tlCtxt->hoSupport.currentHOState);
   hoSupport = &(tlCtxt->hoSupport);

   isSet = pRSSINotification->bRssiThres1PosCross;
   curFWNotification |= isSet << 5;
   isSet = pRSSINotification->bRssiThres2PosCross;
   curFWNotification |= isSet << 4;
   isSet = pRSSINotification->bRssiThres3PosCross;
   curFWNotification |= isSet << 3;
   isSet = pRSSINotification->bRssiThres1NegCross;
   curFWNotification |= isSet << 2;
   isSet = pRSSINotification->bRssiThres2NegCross;
   curFWNotification |= isSet << 1;
   isSet = pRSSINotification->bRssiThres3NegCross;
   curFWNotification |= isSet;
   TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"Current FW Notification is 0x%x", (v_U32_t)curFWNotification ));

   if(0 == preFWNotification)
   {
      TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"This is the first time notification from FW Value is 0x%x", curFWNotification));
      preFWNotification = curFWNotification;
   }
   else if(preFWNotification == curFWNotification)
   {
      return status;
   }

   if(1 == pRSSINotification->bRssiThres1PosCross)
   {
      TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"POS Cross to Region 0"));
      pRegionNumber = 0;
   }
   else if(1 == pRSSINotification->bRssiThres2PosCross)
   {
      TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"POS Cross to Region 1"));
      pRegionNumber = 1;
   }
   else if(1 == pRSSINotification->bRssiThres3PosCross)
   {
      TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"POS Cross to Region 2"));
      pRegionNumber = 2;
   }

   if(1 == pRSSINotification->bRssiThres3NegCross)
   {
      TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"NEG Cross to Region 3"));
      nRegionNumber = 3;
   }
   else if(1 == pRSSINotification->bRssiThres2NegCross)
   {
      TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"NEG Cross to Region 2"));
      nRegionNumber = 2;
   }
   else if(1 == pRSSINotification->bRssiThres1NegCross)
   {
      TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"NEG Cross to Region 1"));
      nRegionNumber = 1;
   }

   newRegionNumber = (nRegionNumber > pRegionNumber) ? nRegionNumber : pRegionNumber;
   if(newRegionNumber == currentHO->regionNumber)
   {
      TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"No Region Change with BMPS mode"));
      preFWNotification = curFWNotification;
      return status;
   }
   else if(newRegionNumber > currentHO->regionNumber)
   {
      TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"Region Increase Worse RSSI"));
      for(idx = currentHO->regionNumber; idx < newRegionNumber; idx++)
      {
         for(sIdx = 0; sIdx < WLANTL_HS_NUM_CLIENT; sIdx++)
         {
            if((WLANTL_HO_THRESHOLD_DOWN == hoSupport->registeredInd[idx].triggerEvent[sIdx]) ||
               (WLANTL_HO_THRESHOLD_CROSS == hoSupport->registeredInd[idx].triggerEvent[sIdx]))
            {
               if(NULL != hoSupport->registeredInd[idx].crossCBFunction[sIdx])
               {
                  cbFunction = hoSupport->registeredInd[idx].crossCBFunction[sIdx];
                  usrCtxt = hoSupport->registeredInd[idx].usrCtxt[sIdx];
                  evtType = WLANTL_HO_THRESHOLD_DOWN;
                  TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"Trigger Event %d, region index %d", hoSupport->registeredInd[idx].triggerEvent[sIdx], idx));
                  currentHO->regionNumber = newRegionNumber;
                  status = cbFunction(pAdapter, evtType, usrCtxt);
               }
            }
         }
      }
   }
   else
   {
      TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"Region Decrease Better RSSI"));
      for(idx = currentHO->regionNumber; idx > newRegionNumber; idx--)
      {
         for(sIdx = 0; sIdx < WLANTL_HS_NUM_CLIENT; sIdx++)
         {
            if((WLANTL_HO_THRESHOLD_UP & hoSupport->registeredInd[idx - 1].triggerEvent[sIdx]) ||
               (WLANTL_HO_THRESHOLD_CROSS & hoSupport->registeredInd[idx - 1].triggerEvent[sIdx]))
            {
               if(NULL != hoSupport->registeredInd[idx - 1].crossCBFunction[sIdx])
               {
                  cbFunction = hoSupport->registeredInd[idx - 1].crossCBFunction[sIdx];
                  usrCtxt = hoSupport->registeredInd[idx - 1].usrCtxt[sIdx];
                  evtType = WLANTL_HO_THRESHOLD_UP;
                  TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"Trigger Event %d, region index %d", hoSupport->registeredInd[idx - 1].triggerEvent[sIdx], idx - 1));
                  currentHO->regionNumber = newRegionNumber;
                  status = cbFunction(pAdapter, evtType, usrCtxt);
               }
            }
         }
      }
   }

   TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"BMPS State, MSG from FW, Trigger Event %d, region index %d",
                 evtType, currentHO->regionNumber));
   preFWNotification = curFWNotification;

   return VOS_STATUS_SUCCESS;
}

/*==========================================================================

   FUNCTION

   DESCRIPTION 
    
   PARAMETERS 

   RETURN VALUE

============================================================================*/
VOS_STATUS WLANTL_HSHandleRSSIChange
(
   v_PVOID_t   pAdapter,
   v_S7_t      currentRSSI
)
{
   WLANTL_CbType                  *tlCtxt = VOS_GET_TL_CB(pAdapter);
   VOS_STATUS                      status = VOS_STATUS_SUCCESS;
   v_U8_t                          currentRegion = 0;
   v_U8_t                          idx, sIdx;
   WLANTL_CURRENT_HO_STATE_TYPE   *currentHO;
   WLANTL_HO_SUPPORT_TYPE         *hoSupport;
   WLANTL_RSSICrossThresholdCBType cbFunction = NULL;
   v_PVOID_t                       usrCtxt = NULL;
   v_U8_t                          evtType = WLANTL_HO_THRESHOLD_NA;

   if(NULL == tlCtxt)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Invalid TL handle"));
      return VOS_STATUS_E_INVAL;
   }

   currentHO = &(tlCtxt->hoSupport.currentHOState);
   hoSupport = &(tlCtxt->hoSupport);
   TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"CRegion %d, NThreshold %d, HRSSI %d",
                currentHO->regionNumber,
                currentHO->numThreshold,
                currentHO->historyRSSI));

   /* Find where is current region */
   for(idx = 0; idx < currentHO->numThreshold; idx++)
   {
      if(hoSupport->registeredInd[idx].rssiValue < currentRSSI)
      {
         currentRegion = idx;
         TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"Found region %d, not bottom", currentRegion));
         break;
      }
   }

   /* If could not find then new RSSI is belong to bottom region */
   if(idx == currentHO->numThreshold)
   {
      currentRegion = idx;
      TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"Current region is bottom %d", idx));
   }

   /* This is a hack. Actual assignment was happening after the below checks. This hack is needed till TL
      posts message and nothing else in the callback indicating UP/DOWN event to the registered module */
   currentHO->historyRSSI = currentRSSI;
   
   if(currentRegion == currentHO->regionNumber)
   {
      currentHO->historyRSSI = currentRSSI;
      return status;
   }
   else if(currentRegion > currentHO->regionNumber)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Region Increase Worse RSSI"));
      for(idx = currentHO->regionNumber; idx < currentRegion; idx++)
      {
         for(sIdx = 0; sIdx < WLANTL_HS_NUM_CLIENT; sIdx++)
         {
            if((WLANTL_HO_THRESHOLD_DOWN == hoSupport->registeredInd[idx].triggerEvent[sIdx]) ||
               (WLANTL_HO_THRESHOLD_CROSS == hoSupport->registeredInd[idx].triggerEvent[sIdx]))
            {
               if(NULL != hoSupport->registeredInd[idx].crossCBFunction[sIdx])
               {
                  cbFunction = hoSupport->registeredInd[idx].crossCBFunction[sIdx];
                  usrCtxt = hoSupport->registeredInd[idx].usrCtxt[sIdx];
                  evtType = WLANTL_HO_THRESHOLD_DOWN;
                  TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Trigger Event %d, region index %d", hoSupport->registeredInd[idx].triggerEvent[sIdx], idx));
                  status = cbFunction(pAdapter, evtType, usrCtxt);
               }
            }
         }
      }
   }
   else
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Region Decrease Better RSSI"));
      for(idx = currentHO->regionNumber; idx > currentRegion; idx--)
      {
         for(sIdx = 0; sIdx < WLANTL_HS_NUM_CLIENT; sIdx++)
         {
            if((WLANTL_HO_THRESHOLD_UP & hoSupport->registeredInd[idx - 1].triggerEvent[sIdx]) ||
               (WLANTL_HO_THRESHOLD_CROSS & hoSupport->registeredInd[idx - 1].triggerEvent[sIdx]))
            {
               if(NULL != hoSupport->registeredInd[idx - 1].crossCBFunction[sIdx])
               {
                  cbFunction = hoSupport->registeredInd[idx - 1].crossCBFunction[sIdx];
                  usrCtxt = hoSupport->registeredInd[idx - 1].usrCtxt[sIdx];
                  evtType = WLANTL_HO_THRESHOLD_UP;
                  TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Trigger Event %d, region index %d", hoSupport->registeredInd[idx - 1].triggerEvent[sIdx], idx - 1));
                  status = cbFunction(pAdapter, evtType, usrCtxt);
               }
            }
         }
      }
   }

   currentHO->historyRSSI = currentRSSI;
   currentHO->regionNumber = currentRegion;
   WLANTL_HSDebugDisplay(pAdapter);

   if(!VOS_IS_STATUS_SUCCESS(status))
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Client fail to handle region change in normal mode %d", status));
   }
   return VOS_STATUS_SUCCESS;
}

/*==========================================================================

   FUNCTION

   DESCRIPTION 
    
   PARAMETERS 

   RETURN VALUE

============================================================================*/
VOS_STATUS WLANTL_HSHandleRXFrame
(
   v_PVOID_t        pAdapter,
   v_U8_t           frameType,
   v_PVOID_t        pBDHeader,
   v_U8_t           STAid,
   v_BOOL_t         isBroadcast,
   vos_pkt_t       *dataBuffer
)
{
   WLANTL_CURRENT_HO_STATE_TYPE *currentHO = NULL;
   WLANTL_CbType   *tlCtxt = VOS_GET_TL_CB(pAdapter);
   VOS_STATUS       status = VOS_STATUS_SUCCESS;
   v_S7_t           currentAvgRSSI = 0;
   v_U8_t           ac;
   v_U32_t          currentTimestamp;
   v_U8_t           tid;

   if(NULL == tlCtxt)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Invalid TL handle"));
      return VOS_STATUS_E_INVAL;
   }

   WLANTL_StatHandleRXFrame(pAdapter, pBDHeader, STAid, isBroadcast, dataBuffer);

   /* If this frame is not management frame increase frame count */
   if((0 != tlCtxt->hoSupport.currentTraffic.idleThreshold) &&
      (WLANTL_MGMT_FRAME_TYPE != frameType))
   {
      tid = WDA_GET_RX_TID( pBDHeader );
      ac = WLANTL_HO_TID_2_AC[(v_U8_t)tid];

      /* Only Voice traffic is handled as real time traffic */
      if(WLANTL_AC_VO == ac)
      {
         tlCtxt->hoSupport.currentTraffic.rtRXFrameCount++;
      }
      else
      {
         tlCtxt->hoSupport.currentTraffic.nrtRXFrameCount++;
      }
      TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"RX frame AC %d, RT Frame Count %d, NRT Frame Count %d",
                   ac,
                   tlCtxt->hoSupport.currentTraffic.rtRXFrameCount,
                   tlCtxt->hoSupport.currentTraffic.nrtRXFrameCount));
   }

   currentHO = &tlCtxt->hoSupport.currentHOState;
   if(VOS_TRUE == tlCtxt->isBMPS)
   {
      WLANTL_HSGetRSSI(pAdapter, pBDHeader, STAid, &currentAvgRSSI);
      return status;
   }

   currentTimestamp = WDA_GET_RX_TIMESTAMP(pBDHeader);
   if((currentTimestamp - currentHO->sampleTime) < WLANTL_HO_SAMPLING_PERIOD)
   {
      return status;
   }
   currentHO->sampleTime = currentTimestamp;

   /* If any threshold is not registerd, DO NOTHING! */
   if(0 == tlCtxt->hoSupport.currentHOState.numThreshold)
   {
      TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"There is no thresholds pass"));
   }
   else
   {
      /* Get Current RSSI from BD Heaser */
      status = WLANTL_HSGetRSSI(pAdapter, pBDHeader, STAid, &currentAvgRSSI);
      if(!VOS_IS_STATUS_SUCCESS(status))
      {
         TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"Get RSSI Fail"));
         return status;
      }
      /* Handle current RSSI value, region, notification, etc */
      status = WLANTL_HSHandleRSSIChange(pAdapter, currentAvgRSSI);
      if(!VOS_IS_STATUS_SUCCESS(status))
      {
         TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Handle new RSSI fail"));
         return status;
      }
   }

   return status;
}

/*==========================================================================

   FUNCTION

   DESCRIPTION 
    
   PARAMETERS 

   RETURN VALUE

============================================================================*/
VOS_STATUS WLANTL_HSHandleTXFrame
(
   v_PVOID_t        pAdapter,
   v_U8_t           ac,
   v_U8_t           STAid,
   vos_pkt_t       *dataBuffer,
   v_PVOID_t        bdHeader
)
{
   WLANTL_CbType                  *tlCtxt = VOS_GET_TL_CB(pAdapter);
   VOS_STATUS                      status = VOS_STATUS_SUCCESS;

   if(NULL == tlCtxt)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Invalid TL handle"));
      return VOS_STATUS_E_INVAL;
   }

   /* Traffic status report is not registered, JUST DO NOTHING */
   if(0 == tlCtxt->hoSupport.currentTraffic.idleThreshold)
   {
      return VOS_STATUS_SUCCESS;
   }

   WLANTL_StatHandleTXFrame(pAdapter, STAid, dataBuffer, bdHeader);

   /* Only Voice traffic is handled as real time traffic */
   if(WLANTL_AC_VO == ac)
   {
      tlCtxt->hoSupport.currentTraffic.rtTXFrameCount++;
   }
   else
   {
      tlCtxt->hoSupport.currentTraffic.nrtTXFrameCount++;
   }
   TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"TX frame AC %d, RT Frame Count %d, NRT Frame Count %d",
                ac,
                tlCtxt->hoSupport.currentTraffic.rtTXFrameCount,
                tlCtxt->hoSupport.currentTraffic.nrtTXFrameCount));

   return status;
}

/*==========================================================================

   FUNCTION

   DESCRIPTION 
    
   PARAMETERS 

   RETURN VALUE

============================================================================*/
VOS_STATUS WLANTL_HSRegRSSIIndicationCB
(
   v_PVOID_t                       pAdapter,
   v_S7_t                          rssiValue,
   v_U8_t                          triggerEvent,
   WLANTL_RSSICrossThresholdCBType crossCBFunction,
   VOS_MODULE_ID                   moduleID,
   v_PVOID_t                       usrCtxt
)
{
   WLANTL_CbType                  *tlCtxt = VOS_GET_TL_CB(pAdapter);
   VOS_STATUS                      status = VOS_STATUS_SUCCESS;
   v_U8_t                          idx, sIdx;
   WLANTL_HO_SUPPORT_TYPE         *hoSupport;
   WLANTL_CURRENT_HO_STATE_TYPE   *currentHO;
   v_U8_t                          clientOrder = 0;

   if(NULL == tlCtxt)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Invalid TL handle"));
      return VOS_STATUS_E_INVAL;
   }

   if((-1 < rssiValue) || (NULL == crossCBFunction))
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Reg Invalid Argument"));
      return VOS_STATUS_E_INVAL;
   }

   THSGETLOCK("WLANTL_HSRegRSSIIndicationCB", &tlCtxt->hoSupport.hosLock);

   currentHO = &(tlCtxt->hoSupport.currentHOState);
   hoSupport = &(tlCtxt->hoSupport);
   TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Make Registration Module %d, Event %d, RSSI %d", moduleID, triggerEvent, rssiValue));

   if((WLANTL_MAX_AVAIL_THRESHOLD < currentHO->numThreshold) ||
      (WLANTL_MAX_AVAIL_THRESHOLD == currentHO->numThreshold))
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"No more available slot, please DEL first %d",
                    currentHO->numThreshold));
      THSRELEASELOCK("WLANTL_HSRegRSSIIndicationCB", &tlCtxt->hoSupport.hosLock);
      return VOS_STATUS_E_RESOURCES;
   }

   if(0 == currentHO->numThreshold)
   {
      TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"First Registration"));
      hoSupport->registeredInd[0].rssiValue    = rssiValue;
      hoSupport->registeredInd[0].triggerEvent[0]    = triggerEvent;
      hoSupport->registeredInd[0].crossCBFunction[0] = crossCBFunction;
      hoSupport->registeredInd[0].usrCtxt[0]         = usrCtxt;
      hoSupport->registeredInd[0].whoIsClient[0]     = moduleID;
      hoSupport->registeredInd[0].numClient++;
   }
   else
   {
      for(idx = 0; idx < currentHO->numThreshold; idx++)
      {
         if(rssiValue == hoSupport->registeredInd[idx].rssiValue)
         {
            for(sIdx = 0; sIdx < WLANTL_HS_NUM_CLIENT; sIdx++)
            {
               TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Reg CB P 0x%x, registered CB P 0x%x",
                             crossCBFunction,
                             hoSupport->registeredInd[idx].crossCBFunction[sIdx]));
               if(crossCBFunction == hoSupport->registeredInd[idx].crossCBFunction[sIdx])
               {
                  TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Same RSSI %d, Same CB 0x%x already registered",
                               rssiValue, crossCBFunction));
                  WLANTL_HSDebugDisplay(pAdapter);
                  THSRELEASELOCK("WLANTL_HSRegRSSIIndicationCB", &tlCtxt->hoSupport.hosLock);
                  return status;
               }
            }

            for(sIdx = 0; sIdx < WLANTL_HS_NUM_CLIENT; sIdx++)
            {
               if(NULL == hoSupport->registeredInd[idx].crossCBFunction[sIdx])
               {
                  clientOrder = sIdx;
                  break;
               }
            }
            hoSupport->registeredInd[idx].triggerEvent[clientOrder]    = triggerEvent;
            hoSupport->registeredInd[idx].crossCBFunction[clientOrder] = crossCBFunction;
            hoSupport->registeredInd[idx].usrCtxt[clientOrder]         = usrCtxt;
            hoSupport->registeredInd[idx].whoIsClient[clientOrder]     = moduleID;
            hoSupport->registeredInd[idx].numClient++;
            WLANTL_HSDebugDisplay(pAdapter);
            THSRELEASELOCK("WLANTL_HSRegRSSIIndicationCB", &tlCtxt->hoSupport.hosLock);
            return status;
         }
      }
      for(idx = 0; idx < currentHO->numThreshold; idx++)
      {
         if(rssiValue > hoSupport->registeredInd[idx].rssiValue)
         {
            for(sIdx = (currentHO->numThreshold - 1); (sIdx > idx) || (sIdx == idx); sIdx--)
            {
               TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO, "Shift %d array to %d", sIdx, sIdx + 1));
               memcpy(&hoSupport->registeredInd[sIdx + 1], &hoSupport->registeredInd[sIdx], sizeof(WLANTL_HO_RSSI_INDICATION_TYPE));
               memset(&hoSupport->registeredInd[sIdx], 0, sizeof(WLANTL_HO_RSSI_INDICATION_TYPE));
               if(0 == sIdx)
               {
                  break;
               }
            }
            TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"Put in Here %d", idx));
            hoSupport->registeredInd[idx].rssiValue    = rssiValue;
            hoSupport->registeredInd[idx].triggerEvent[0]    = triggerEvent;
            hoSupport->registeredInd[idx].crossCBFunction[0] = crossCBFunction;
            hoSupport->registeredInd[idx].usrCtxt[0]         = usrCtxt;
            hoSupport->registeredInd[idx].whoIsClient[0]     = moduleID;
            hoSupport->registeredInd[idx].numClient++;
            break;
         }
      }
      if(currentHO->numThreshold == idx)
      {
         TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO, "New threshold put in bottom"));

         hoSupport->registeredInd[currentHO->numThreshold].rssiValue    = rssiValue;
         hoSupport->registeredInd[currentHO->numThreshold].triggerEvent[0] = triggerEvent;
         hoSupport->registeredInd[currentHO->numThreshold].crossCBFunction[0] = crossCBFunction;
         hoSupport->registeredInd[currentHO->numThreshold].usrCtxt[0] = usrCtxt;
         hoSupport->registeredInd[currentHO->numThreshold].whoIsClient[0]     = moduleID;
         hoSupport->registeredInd[currentHO->numThreshold].numClient++;
      }
   }

   currentHO->numThreshold++;
   if((VOS_FALSE == tlCtxt->isBMPS) && (rssiValue > currentHO->historyRSSI))
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Added Threshold above current RSSI level, old RN %d", currentHO->regionNumber));
      if(4 > currentHO->regionNumber)
      {
         currentHO->regionNumber++;
      }
      else
      {
         TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Current region number is max %d, cannot increase anymore", currentHO->regionNumber));
      }
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"increase region number without notification %d", currentHO->regionNumber));
   }
   else if(VOS_TRUE == tlCtxt->isBMPS)
   {
      if(0 != currentHO->regionNumber)
      {
         if(hoSupport->registeredInd[currentHO->regionNumber].rssiValue < rssiValue)
         {
            currentHO->regionNumber++;
            if((WLANTL_HO_THRESHOLD_DOWN == triggerEvent) || (WLANTL_HO_THRESHOLD_CROSS == triggerEvent))
            {
               TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Registered RSSI value larger than Current RSSI, and DOWN event, Send Notification"));
               crossCBFunction(pAdapter, WLANTL_HO_THRESHOLD_DOWN, usrCtxt);
            }
         }
         else if((currentHO->regionNumber < (currentHO->numThreshold - 1)) &&
                 (hoSupport->registeredInd[currentHO->regionNumber + 1].rssiValue > rssiValue))
         {
            if((WLANTL_HO_THRESHOLD_UP == triggerEvent) || (WLANTL_HO_THRESHOLD_CROSS == triggerEvent))
            {
               TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Registered RSSI value smaller than Current RSSI"));
            }
         }
      }
      else
      {
         if(hoSupport->registeredInd[currentHO->regionNumber].rssiValue > rssiValue)
         {
            if((WLANTL_HO_THRESHOLD_UP == triggerEvent) || (WLANTL_HO_THRESHOLD_CROSS == triggerEvent))
            {
               TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Registered RSSI value smaller than Current RSSI"));
            }
         }
      }
   }

   if((VOS_FALSE == tlCtxt->isBMPS) &&
      (rssiValue >= currentHO->historyRSSI) && (0 != currentHO->historyRSSI) &&
      ((WLANTL_HO_THRESHOLD_DOWN == triggerEvent) || (WLANTL_HO_THRESHOLD_CROSS == triggerEvent)))
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Registered RSSI value larger than Current RSSI, and DOWN event, Send Notification"));
      crossCBFunction(pAdapter, WLANTL_HO_THRESHOLD_DOWN, usrCtxt);
   }
   else if((VOS_FALSE == tlCtxt->isBMPS) &&
           (rssiValue < currentHO->historyRSSI) && (0 != currentHO->historyRSSI) &&
           ((WLANTL_HO_THRESHOLD_UP == triggerEvent) || (WLANTL_HO_THRESHOLD_CROSS == triggerEvent)))
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Registered RSSI value smaller than Current RSSI, and UP event, Send Notification"));
      crossCBFunction(pAdapter, WLANTL_HO_THRESHOLD_UP, usrCtxt);
   }

   if(VOS_TRUE == tlCtxt->isBMPS)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Register into FW, now BMPS"));
      WLANTL_SetFWRSSIThresholds(pAdapter);
   }

   WLANTL_HSDebugDisplay(pAdapter);
   THSRELEASELOCK("WLANTL_HSRegRSSIIndicationCB", &tlCtxt->hoSupport.hosLock);
   return status;
}

/*==========================================================================

   FUNCTION

   DESCRIPTION 
    
   PARAMETERS 

   RETURN VALUE

============================================================================*/
VOS_STATUS WLANTL_HSDeregRSSIIndicationCB
(
   v_PVOID_t                       pAdapter,
   v_S7_t                          rssiValue,
   v_U8_t                          triggerEvent,
   WLANTL_RSSICrossThresholdCBType crossCBFunction,
   VOS_MODULE_ID                   moduleID
)
{
   WLANTL_CbType                  *tlCtxt = VOS_GET_TL_CB(pAdapter);
   VOS_STATUS                      status = VOS_STATUS_SUCCESS;
   v_U8_t                          idx, sIdx;
   WLANTL_HO_SUPPORT_TYPE         *hoSupport;
   WLANTL_CURRENT_HO_STATE_TYPE   *currentHO;
   v_BOOL_t                        bmpsAbove = VOS_FALSE;

   if(NULL == tlCtxt)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Invalid TL handle"));
      return VOS_STATUS_E_INVAL;
   }

   if(0 == tlCtxt->hoSupport.currentHOState.numThreshold)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Empty list, can not remove"));
      return VOS_STATUS_E_EMPTY;
   }

   THSGETLOCK("WLANTL_HSDeregRSSIIndicationCB", &tlCtxt->hoSupport.hosLock);
   currentHO = &(tlCtxt->hoSupport.currentHOState);
   hoSupport = &(tlCtxt->hoSupport);

   TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"DEL target RSSI %d, event %d", rssiValue, triggerEvent));

   if((VOS_TRUE == tlCtxt->isBMPS) && (0 < currentHO->regionNumber))
   {
      if(rssiValue >= hoSupport->registeredInd[currentHO->regionNumber - 1].rssiValue)
      {
         bmpsAbove = VOS_TRUE;
         TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Remove Threshold larger than current region"));
      }
   }

   for(idx = 0; idx < currentHO->numThreshold; idx++)
   {
      if(rssiValue == hoSupport->registeredInd[idx].rssiValue)
      {
         for(sIdx = 0; sIdx < WLANTL_HS_NUM_CLIENT; sIdx++)
         {
            if(crossCBFunction == tlCtxt->hoSupport.registeredInd[idx].crossCBFunction[sIdx])
            {
               tlCtxt->hoSupport.registeredInd[idx].triggerEvent[sIdx]    = 0;
               tlCtxt->hoSupport.registeredInd[idx].crossCBFunction[sIdx] = NULL;
               tlCtxt->hoSupport.registeredInd[idx].usrCtxt[sIdx]         = NULL;
               tlCtxt->hoSupport.registeredInd[idx].whoIsClient[sIdx]     = 0;
               tlCtxt->hoSupport.registeredInd[idx].numClient--;
            }
         }
         if(0 != tlCtxt->hoSupport.registeredInd[idx].numClient)
         {
            TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"Found Multiple idx is %d", idx));
            WLANTL_HSDebugDisplay(pAdapter);
            THSRELEASELOCK("WLANTL_HSDeregRSSIIndicationCB", &tlCtxt->hoSupport.hosLock);
            return status;
         }
         else
         {
            TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"Found Single idx is %d", idx));
            break;
         }
      }
   }
   if(idx == currentHO->numThreshold)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Could not find entry, maybe invalid arg"));
      THSRELEASELOCK("WLANTL_HSDeregRSSIIndicationCB", &tlCtxt->hoSupport.hosLock);
      return VOS_STATUS_E_INVAL;
   }

   for(idx = 0; idx < currentHO->numThreshold; idx++)
   {
      if(rssiValue == hoSupport->registeredInd[idx].rssiValue)
      {
         if((currentHO->numThreshold - 1) == idx)
         {
            TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"Remove target is last one"));
            /* Does not need move any element, just remove last array entry */
         }
         else
         {
            for(sIdx = idx; sIdx < (currentHO->numThreshold - 1); sIdx++)
            {
               TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"Shift up from %d to %d", sIdx + 1, sIdx));
               memcpy(&hoSupport->registeredInd[sIdx], &hoSupport->registeredInd[sIdx + 1], sizeof(WLANTL_HO_RSSI_INDICATION_TYPE));
            }
         }
         break;
      }
   }
   /* Common remove last array entry */
   tlCtxt->hoSupport.registeredInd[currentHO->numThreshold - 1].rssiValue    = WLANTL_HO_DEFAULT_RSSI;
   for(idx = 0; idx < WLANTL_HS_NUM_CLIENT; idx++)
   {
      tlCtxt->hoSupport.registeredInd[currentHO->numThreshold - 1].triggerEvent[idx]    = WLANTL_HO_THRESHOLD_NA;
      tlCtxt->hoSupport.registeredInd[currentHO->numThreshold - 1].crossCBFunction[idx] = NULL;
      tlCtxt->hoSupport.registeredInd[currentHO->numThreshold - 1].usrCtxt[idx]         = NULL;
      tlCtxt->hoSupport.registeredInd[currentHO->numThreshold - 1].whoIsClient[idx]     = 0;
      tlCtxt->hoSupport.registeredInd[currentHO->numThreshold - 1].numClient            = 0;
   }

   if((VOS_FALSE == tlCtxt->isBMPS) && (rssiValue >= currentHO->historyRSSI))
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Removed Threshold above current RSSI level, old RN %d", currentHO->regionNumber));
      if(0 < currentHO->regionNumber)
      {
         currentHO->regionNumber--;
      }
      else
      {
         TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Current Region number is 0, cannot decrease anymore"));
      }
      TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"Decrease region number without notification %d", currentHO->regionNumber));
   }
   else if((VOS_TRUE == tlCtxt->isBMPS) && (VOS_TRUE == bmpsAbove))
   {
      currentHO->regionNumber--;
   }
   /* Decrease number of thresholds */
   tlCtxt->hoSupport.currentHOState.numThreshold--;

   if(VOS_TRUE == tlCtxt->isBMPS)
   {
      TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"Register into FW, now BMPS"));
      WLANTL_SetFWRSSIThresholds(pAdapter);
   }

   /* Based on new threshold set recalculated current RSSI status */
   if(0 < tlCtxt->hoSupport.currentHOState.numThreshold)
   {
   }
   else if(0 == tlCtxt->hoSupport.currentHOState.numThreshold)
   {
      currentHO->regionNumber = 0;
      TLLOGW(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN,"No registered Threshold"));
      /* What should do? */
   }

   WLANTL_HSDebugDisplay(pAdapter);
   THSRELEASELOCK("WLANTL_HSDeregRSSIIndicationCB", &tlCtxt->hoSupport.hosLock);
   return status;
}

/*==========================================================================

   FUNCTION

   DESCRIPTION 
    
   PARAMETERS 

   RETURN VALUE

============================================================================*/
VOS_STATUS WLANTL_HSSetAlpha
(
   v_PVOID_t pAdapter,
   int       valueAlpha
)
{
   WLANTL_CbType   *tlCtxt = VOS_GET_TL_CB(pAdapter);
   VOS_STATUS       status = VOS_STATUS_SUCCESS;

   if(NULL == tlCtxt)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Invalid TL handle"));
      return VOS_STATUS_E_INVAL;
   }

   THSGETLOCK("WLANTL_HSSetAlpha", &tlCtxt->hoSupport.hosLock);
   tlCtxt->hoSupport.currentHOState.alpha = (v_U8_t)valueAlpha;
   THSRELEASELOCK("WLANTL_HSSetAlpha", &tlCtxt->hoSupport.hosLock);
   return status;
}

/*==========================================================================

   FUNCTION

   DESCRIPTION 
    
   PARAMETERS 

   RETURN VALUE

============================================================================*/
VOS_STATUS WLANTL_HSRegGetTrafficStatus
(
   v_PVOID_t                          pAdapter,
   v_U32_t                            idleThreshold,
   v_U32_t                            period,
   WLANTL_TrafficStatusChangedCBType  trfficStatusCB,
   v_PVOID_t                          usrCtxt
)
{
   WLANTL_CbType   *tlCtxt = VOS_GET_TL_CB(pAdapter);
   VOS_STATUS       status = VOS_STATUS_SUCCESS;

   if(NULL == tlCtxt)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Invalid TL handle"));
      return VOS_STATUS_E_INVAL;
   }

   if((0 == idleThreshold) || (0 == period) || (NULL == trfficStatusCB))
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Invalid Argument Passed from SME"));
      return VOS_STATUS_E_INVAL;
   }

   tlCtxt->hoSupport.currentTraffic.idleThreshold = idleThreshold;
   tlCtxt->hoSupport.currentTraffic.measurePeriod = period;
   tlCtxt->hoSupport.currentTraffic.trafficCB     = trfficStatusCB;
   tlCtxt->hoSupport.currentTraffic.usrCtxt       = usrCtxt;

   vos_timer_start(&tlCtxt->hoSupport.currentTraffic.trafficTimer,
                   tlCtxt->hoSupport.currentTraffic.measurePeriod);

   return status;
}

/*==========================================================================

   FUNCTION

   DESCRIPTION 
    
   PARAMETERS 

   RETURN VALUE

============================================================================*/
VOS_STATUS WLANTL_HSInit
(
   v_PVOID_t   pAdapter
)
{
   WLANTL_CbType   *tlCtxt = VOS_GET_TL_CB(pAdapter);
   VOS_STATUS       status = VOS_STATUS_SUCCESS;
   v_U8_t           idx, sIdx;

   if(NULL == tlCtxt)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Invalid TL handle"));
      return VOS_STATUS_E_INVAL;
   }
#ifdef WLANTL_HO_UTEST
   rssi = 0;
   direction = -1;
#endif /* WLANTL_HO_UTEST */

   /* set default current HO status */
   tlCtxt->hoSupport.currentHOState.alpha        = WLANTL_HO_DEFAULT_ALPHA;
   tlCtxt->hoSupport.currentHOState.historyRSSI  = 0;
   tlCtxt->hoSupport.currentHOState.numThreshold = 0;
   tlCtxt->hoSupport.currentHOState.regionNumber = 0;
   tlCtxt->hoSupport.currentHOState.sampleTime   = 0;

   /* set default current traffic status */
   tlCtxt->hoSupport.currentTraffic.trafficStatus.rtTrafficStatus
                                                    = WLANTL_HO_RT_TRAFFIC_STATUS_OFF;
   tlCtxt->hoSupport.currentTraffic.trafficStatus.nrtTrafficStatus
                                                    = WLANTL_HO_NRT_TRAFFIC_STATUS_OFF;
   tlCtxt->hoSupport.currentTraffic.idleThreshold   = 0;
   tlCtxt->hoSupport.currentTraffic.measurePeriod   = 0;
   tlCtxt->hoSupport.currentTraffic.rtRXFrameCount  = 0;
   tlCtxt->hoSupport.currentTraffic.rtTXFrameCount  = 0;
   tlCtxt->hoSupport.currentTraffic.nrtRXFrameCount = 0;
   tlCtxt->hoSupport.currentTraffic.nrtTXFrameCount = 0;
   tlCtxt->hoSupport.currentTraffic.trafficCB       = NULL;

   /* Initialize indication array */
   for(idx = 0; idx < WLANTL_MAX_AVAIL_THRESHOLD; idx++)
   {
      for(sIdx = 0; sIdx < WLANTL_HS_NUM_CLIENT; sIdx++)
      {
         tlCtxt->hoSupport.registeredInd[idx].triggerEvent[sIdx]    = WLANTL_HO_THRESHOLD_NA;
         tlCtxt->hoSupport.registeredInd[idx].crossCBFunction[sIdx] = NULL;
         tlCtxt->hoSupport.registeredInd[idx].usrCtxt[sIdx]         = NULL;
         tlCtxt->hoSupport.registeredInd[idx].whoIsClient[sIdx]     = 0;
      }
      tlCtxt->hoSupport.registeredInd[idx].rssiValue          = WLANTL_HO_DEFAULT_RSSI;
      tlCtxt->hoSupport.registeredInd[idx].numClient          = 0;
   }

   vos_timer_init(&tlCtxt->hoSupport.currentTraffic.trafficTimer,
                  VOS_TIMER_TYPE_SW,
                  WLANTL_HSTrafficStatusTimerExpired,
                  pAdapter);


   vos_lock_init(&tlCtxt->hoSupport.hosLock);
   tlCtxt->hoSupport.macCtxt = vos_get_context(VOS_MODULE_ID_SME, pAdapter);

   return status;
}

/*==========================================================================

   FUNCTION

   DESCRIPTION 
    
   PARAMETERS 

   RETURN VALUE

============================================================================*/
VOS_STATUS WLANTL_HSStop
(
   v_PVOID_t   pAdapter
)
{
   WLANTL_CbType   *tlCtxt = VOS_GET_TL_CB(pAdapter);
   VOS_STATUS       status = VOS_STATUS_SUCCESS;
   VOS_TIMER_STATE  timerState;

   if(NULL == tlCtxt)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Invalid TL handle"));
      return VOS_STATUS_E_INVAL;
   }

   timerState = vos_timer_getCurrentState(&tlCtxt->hoSupport.currentTraffic.trafficTimer);
   if(VOS_TIMER_STATE_RUNNING == timerState)
   {
      TLLOG1(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO,"Stop Traffic status monitoring timer"));
      status = vos_timer_stop(&tlCtxt->hoSupport.currentTraffic.trafficTimer);
   }
   if(VOS_STATUS_SUCCESS != status)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Timer Stop Failed, Status %d", status));
   }
   status = vos_timer_destroy(&tlCtxt->hoSupport.currentTraffic.trafficTimer);
   if(VOS_STATUS_SUCCESS != status)
   {
      TLLOGE(VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR,"Timer Destroy Failed, Status %d", status));
   }
   return status;   
}
#endif //FEATURE_WLAN_GEN6_ROAMING
