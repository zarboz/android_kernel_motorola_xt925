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

/*
 * Airgo Networks, Inc proprietary. All rights reserved.
 * This file limLinkMonitoringAlgo.cc contains the code for
 * Link monitoring algorithm on AP and heart beat failure
 * handling on STA.
 * Author:        Chandra Modumudi
 * Date:          03/01/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#include "aniGlobal.h"
#include "wniCfgAp.h"
#include "cfgApi.h"

#ifdef FEATURE_WLAN_NON_INTEGRATED_SOC
#include "halDataStruct.h"
#include "halCommonApi.h"
#endif

#include "schApi.h"
#include "pmmApi.h"
#include "utilsApi.h"
#include "limAssocUtils.h"
#include "limTypes.h"
#include "limUtils.h"
#include "limPropExtsUtils.h"

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT
#include "vos_diag_core_log.h"
#endif //FEATURE_WLAN_DIAG_SUPPORT
#include "limSession.h"
#include "limSerDesUtils.h"


/**
 * limSendKeepAliveToPeer()
 *
 *FUNCTION:
 * This function is called to send Keep alive message to peer
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 * NA
 *
 * @param  pMac        - Pointer to Global MAC structure
 * @return None
 */

void
limSendKeepAliveToPeer(tpAniSirGlobal pMac)
{

#ifdef ANI_PRODUCT_TYPE_AP   //oct 3rd review

    tpDphHashNode   pStaDs;
    //fetch the sessionEntry based on the sessionId
    tpPESession psessionEntry;

    if((psessionEntry = peFindSessionBySessionId(pMac, pMac->lim.limTimers.gLimKeepaliveTimer.sessionId))== NULL)
    {
        limLog(pMac, LOGP,FL("Session Does not exist for given sessionID\n"));
        return;
    }

    // If keep live has been disabled, exit
    if (pMac->sch.keepAlive == 0)
        return;

    if ( (limIsSystemInScanState(pMac) == false) &&
          (psessionEntry->limSystemRole == eLIM_AP_ROLE))
    {
        tANI_U16 i;
        tANI_U32        len = SIR_MAC_MAX_SSID_LENGTH;
        tAniSSID   ssId;

        /*
        ** send keepalive NULL data frame for each
        ** associated STA;
        */

        for (i=2; i<pMac->lim.maxStation; i++)
        {
            pStaDs = dphGetHashEntry(pMac, i, &psessionEntry->dph.dphHashTable);

            if (pStaDs && pStaDs->added &&
                (pStaDs->mlmStaContext.mlmState == eLIM_MLM_LINK_ESTABLISHED_STATE))
            {
                // SP-Tx hangs at times when a zero-lenght packet is transmitted
                // To avoid any interoperability issue with third party clinet
                // instead of sending a non-zero data-null packet, AP sends a
                // probe response as a keep alive packet.
                if (wlan_cfgGetStr(pMac, WNI_CFG_SSID,
                                (tANI_U8 *) &ssId.ssId,
                                (tANI_U32 *) &len) != eSIR_SUCCESS)
                {
                        /// Could not get SSID from CFG. Log error.
                    limLog(pMac, LOGP, FL("could not retrieve SSID\n"));
                }
                ssId.length = (tANI_U8) len;

                PELOG2(limLog(pMac, LOG2,  FL("Sending keepalive Probe Rsp Msg to "));
                limPrintMacAddr(pMac, pStaDs->staAddr, LOG2);)
                limSendProbeRspMgmtFrame(pMac,
                                         pStaDs->staAddr,
                                         &ssId,
                                         i,
                                         DPH_KEEPALIVE_FRAME, 0);
            }
        }
    }
    #endif
} /*** limSendKeepAliveToPeer() ***/


/** ---------------------------------------------------------
\fn      limDeleteStaContext
\brief   This function handles the message from HAL:
\        WDA_DELETE_STA_CONTEXT_IND. This function
\        validates that the given station id exist, and if so,
\        deletes the station by calling limTriggerSTAdeletion.
\param   tpAniSirGlobal pMac
\param   tpSirMsgQ      limMsg
\return  none
  -----------------------------------------------------------*/
void
limDeleteStaContext(tpAniSirGlobal pMac, tpSirMsgQ limMsg)
{
    tpDeleteStaContext  pMsg = (tpDeleteStaContext)limMsg->bodyptr;
    tpDphHashNode       pStaDs;
    tpPESession psessionEntry ;
    tANI_U8     sessionId;

    if((psessionEntry = peFindSessionByBssid(pMac,pMsg->bssId,&sessionId))== NULL)
    {
         PELOGE(limLog(pMac, LOGE,FL("session does not exist for given BSSId\n"));)
         palFreeMemory(pMac->hHdd, pMsg);
         return;
    }

    if (NULL != pMsg)
    {
#ifdef WLAN_SOFTAP_FEATURE
        switch(pMsg->reasonCode)
        {
            case HAL_DEL_STA_REASON_CODE_KEEP_ALIVE:
            case HAL_DEL_STA_REASON_CODE_TIM_BASED:
                PELOGE(limLog(pMac, LOGE, FL(" Deleting station: staId = %d, reasonCode = %d\n"), pMsg->staId, pMsg->reasonCode);)
#endif        
                pStaDs = dphGetHashEntry(pMac, pMsg->assocId, &psessionEntry->dph.dphHashTable);
                if (! pStaDs)
                {
                   PELOGW(limLog(pMac, LOGW, FL("Skip STA deletion (invalid STA)\n"));)
                   palFreeMemory(pMac->hHdd, pMsg);
                   return;
                }

                /* check and see if same staId. This is to avoid the scenario
                * where we're trying to delete a staId we just added.
                */
                if (pStaDs->staIndex != pMsg->staId)
                {
                    PELOGW(limLog(pMac, LOGW, FL("staid mismatch: %d vs %d \n"), pStaDs->staIndex, pMsg->staId);)
                    palFreeMemory(pMac->hHdd, pMsg);
                    return;
                }

                if((eLIM_BT_AMP_AP_ROLE == psessionEntry->limSystemRole) 
					            || (eLIM_AP_ROLE == psessionEntry->limSystemRole))
       	        {
                    PELOG1(limLog(pMac, LOG1, FL("SAP:lim Delete Station Context (staId: %d, assocId: %d) \n"),
                                                               pMsg->staId, pMsg->assocId);)
                    limTriggerSTAdeletion(pMac, pStaDs, psessionEntry);
                }
                else
                {
                    //TearDownLink with AP
                    tLimMlmDeauthInd  mlmDeauthInd;
                    PELOG1(limLog(pMac, LOG1, FL("lim Delete Station Context (staId: %d, assocId: %d) \n"),
                                                               pMsg->staId, pMsg->assocId);)

                    pStaDs->mlmStaContext.disassocReason = eSIR_MAC_UNSPEC_FAILURE_REASON;
                    pStaDs->mlmStaContext.cleanupTrigger = eLIM_LINK_MONITORING_DEAUTH;

                    /// Issue Deauth Indication to SME.
                    palCopyMemory( pMac->hHdd, (tANI_U8 *) &mlmDeauthInd.peerMacAddr,
                                     pStaDs->staAddr, sizeof(tSirMacAddr));
                    mlmDeauthInd.reasonCode    = (tANI_U8) pStaDs->mlmStaContext.disassocReason;
                    mlmDeauthInd.deauthTrigger =  pStaDs->mlmStaContext.cleanupTrigger;

                    limPostSmeMessage(pMac, LIM_MLM_DEAUTH_IND, (tANI_U32 *) &mlmDeauthInd);
 
                    limSendSmeDeauthInd(pMac, pStaDs, psessionEntry);
                }
#ifdef WLAN_SOFTAP_FEATURE
                break;        
            
            case HAL_DEL_STA_REASON_CODE_UNKNOWN_A2:
                PELOGE(limLog(pMac, LOGE, FL(" Deleting Unknown station \n"));)
                limPrintMacAddr(pMac, pMsg->addr2, LOGE);
               
                limSendDeauthMgmtFrame( pMac, eSIR_MAC_CLASS3_FRAME_FROM_NON_ASSOC_STA_REASON, pMsg->addr2, psessionEntry);
                break;

            default:
                PELOGE(limLog(pMac, LOGE, FL(" Unknown reason code \n"));)
                break;

        }
#endif
    }

    palFreeMemory(pMac->hHdd, pMsg);
    return;
}


/**
 * limTriggerSTAdeletion()
 *
 *FUNCTION:
 * This function is called to trigger STA context deletion
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 * NA
 *
 * @param  pMac   - Pointer to global MAC structure
 * @param  pStaDs - Pointer to internal STA Datastructure
 * @return None
 */
void
limTriggerSTAdeletion(tpAniSirGlobal pMac, tpDphHashNode pStaDs, tpPESession psessionEntry)
{
    tSirSmeDeauthReq    *pSmeDeauthReq;
    tANI_U8             *pBuf;
    tANI_U8             *pLen;
    tANI_U16            msgLength = 0;

    if (! pStaDs)
    {
        PELOGW(limLog(pMac, LOGW, FL("Skip STA deletion (invalid STA)\n"));)
        return;
    }
    /**
     * MAC based Authentication was used. Trigger
     * Deauthentication frame to peer since it will
     * take care of disassociation as well.
     */

    if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd, (void **)&pSmeDeauthReq, sizeof(tSirSmeDeauthReq)))
    {
        limLog(pMac, LOGP, FL("palAllocateMemory failed for eWNI_SME_DEAUTH_REQ \n"));
        return;
    }

    pBuf = (tANI_U8 *) &pSmeDeauthReq->messageType;

    //messageType
#ifdef WLAN_SOFTAP_FEATURE
    limCopyU16((tANI_U8*)pBuf, eWNI_SME_DISASSOC_REQ);
#else
    limCopyU16((tANI_U8*)pBuf, eWNI_SME_DEAUTH_REQ);
#endif
    pBuf += sizeof(tANI_U16);
    msgLength += sizeof(tANI_U16);

    //length
    pLen = pBuf;
    pBuf += sizeof(tANI_U16);
    msgLength += sizeof(tANI_U16);
    
    //sessionId
    *pBuf = psessionEntry->peSessionId;
    pBuf++;
    msgLength++;
  
    //transactionId
    limCopyU16((tANI_U8*)pBuf, psessionEntry->transactionId);
    pBuf += sizeof(tANI_U16);
    msgLength += sizeof(tANI_U16);

    //bssId
    palCopyMemory( pMac->hHdd, pBuf, psessionEntry->bssId, sizeof(tSirMacAddr) );
    pBuf += sizeof(tSirMacAddr);
    msgLength += sizeof(tSirMacAddr);

    //peerMacAddr
    palCopyMemory( pMac->hHdd, pBuf, pStaDs->staAddr, sizeof(tSirMacAddr) );
    pBuf += sizeof(tSirMacAddr);
    msgLength += sizeof(tSirMacAddr);

    //reasonCode 
#ifdef WLAN_SOFTAP_FEATURE
    limCopyU16((tANI_U8*)pBuf, (tANI_U16)eLIM_LINK_MONITORING_DISASSOC);
#else
    limCopyU16((tANI_U8*)pBuf, (tANI_U16)eLIM_LINK_MONITORING_DEAUTH);
#endif
    pBuf += sizeof(tANI_U16);
    msgLength += sizeof(tANI_U16);

    //send disassoc OTA
    pBuf[0]= 1;
    pBuf += sizeof(tANI_U8);
    msgLength += sizeof(tANI_U8);


#if (WNI_POLARIS_FW_PRODUCT == AP)
    //aid
    limCopyU16((tANI_U8*)pBuf, pStaDs->assocId);
    pBuf += sizeof(tANI_U16);
    msgLength += sizeof(tANI_U16);
#endif
  
    //Fill in length
    limCopyU16((tANI_U8*)pLen, msgLength);

#ifdef WLAN_SOFTAP_FEATURE
    limPostSmeMessage(pMac, eWNI_SME_DISASSOC_REQ, (tANI_U32 *) pSmeDeauthReq);
#else
    limPostSmeMessage(pMac, eWNI_SME_DEAUTH_REQ, (tANI_U32 *) pSmeDeauthReq);
#endif
    palFreeMemory( pMac->hHdd, pSmeDeauthReq );

} /*** end limTriggerSTAdeletion() ***/



/**
 * limTearDownLinkWithAp()
 *
 *FUNCTION:
 * This function is called when heartbeat (beacon reception)
 * fails on STA
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @return None
 */

void
limTearDownLinkWithAp(tpAniSirGlobal pMac)
{
    tpDphHashNode pStaDs = NULL;

    //tear down the following sessionEntry
    tpPESession psessionEntry;

    if((psessionEntry = peFindSessionBySessionId(pMac, pMac->lim.limTimers.gLimProbeAfterHBTimer.sessionId))== NULL)
    {
        limLog(pMac, LOGP,FL("Session Does not exist for given sessionID\n"));
        return;
    }
    /**
     * Heart beat failed for upto threshold value
     * and AP did not respond for Probe request.
     * Trigger link tear down.
     */

    pMac->pmm.inMissedBeaconScenario = FALSE;
    limLog(pMac, LOGW,
       FL("No ProbeRsp from AP after HB failure. Tearing down link\n"));

    // Deactivate heartbeat timer
    limHeartBeatDeactivateAndChangeTimer(pMac, psessionEntry);

    // Announce loss of link to Roaming algorithm
    // and cleanup by sending SME_DISASSOC_REQ to SME

    pStaDs = dphGetHashEntry(pMac, DPH_STA_HASH_INDEX_PEER, &psessionEntry->dph.dphHashTable);

    
    if (pStaDs != NULL)
    {
        tLimMlmDeauthInd  mlmDeauthInd;

        pStaDs->mlmStaContext.disassocReason = eSIR_MAC_UNSPEC_FAILURE_REASON;
        pStaDs->mlmStaContext.cleanupTrigger = eLIM_LINK_MONITORING_DEAUTH;

        /// Issue Deauth Indication to SME.
        palCopyMemory( pMac->hHdd, (tANI_U8 *) &mlmDeauthInd.peerMacAddr,
                      pStaDs->staAddr,
                      sizeof(tSirMacAddr));
        mlmDeauthInd.reasonCode    = (tANI_U8) pStaDs->mlmStaContext.disassocReason;
        mlmDeauthInd.deauthTrigger =  pStaDs->mlmStaContext.cleanupTrigger;

        limPostSmeMessage(pMac, LIM_MLM_DEAUTH_IND, (tANI_U32 *) &mlmDeauthInd);

        limSendSmeDeauthInd(pMac, pStaDs, psessionEntry);
    }    
} /*** limTearDownLinkWithAp() ***/




/**
 * limHandleHeartBeatFailure()
 *
 *FUNCTION:
 * This function is called when heartbeat (beacon reception)
 * fails on STA
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @return None
 */

void limHandleHeartBeatFailure(tpAniSirGlobal pMac,tpPESession psessionEntry)
{

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT
    vos_log_beacon_update_pkt_type *log_ptr = NULL;
#endif //FEATURE_WLAN_DIAG_SUPPORT 

    /* If gLimHeartBeatTimer fires between the interval of sending WDA_ENTER_BMPS_REQUEST 
     * to the HAL and receiving WDA_ENTER_BMPS_RSP from the HAL, then LIM (PE) tries to Process the
     * SIR_LIM_HEAR_BEAT_TIMEOUT message but The PE state is ePMM_STATE_BMPS_SLEEP so PE dont
     * want to handle heartbeat timeout in the BMPS, because Firmware handles it in BMPS.
     * So just return from heartbeatfailure handler
     */

    if(!limIsSystemInActiveState(pMac))
        return;

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT
    WLAN_VOS_DIAG_LOG_ALLOC(log_ptr, vos_log_beacon_update_pkt_type, LOG_WLAN_BEACON_UPDATE_C);
    if(log_ptr)
        log_ptr->bcn_rx_cnt = psessionEntry->LimRxedBeaconCntDuringHB;
    WLAN_VOS_DIAG_LOG_REPORT(log_ptr);
#endif //FEATURE_WLAN_DIAG_SUPPORT

    /** Re Activate Timer if the system is Waiting for ReAssoc Response*/
    if(((psessionEntry->limSystemRole == eLIM_STA_IN_IBSS_ROLE) || 
        (psessionEntry->limSystemRole == eLIM_STA_ROLE) ||
        (psessionEntry->limSystemRole == eLIM_BT_AMP_STA_ROLE)) && 
       (LIM_IS_CONNECTION_ACTIVE(psessionEntry) ||
        (limIsReassocInProgress(pMac, psessionEntry))))
    {
        if(psessionEntry->LimRxedBeaconCntDuringHB < MAX_NO_BEACONS_PER_HEART_BEAT_INTERVAL)
            pMac->lim.gLimHeartBeatBeaconStats[psessionEntry->LimRxedBeaconCntDuringHB]++;
        else
            pMac->lim.gLimHeartBeatBeaconStats[0]++;

        /****** 
         * Note: Use this code once you have converted all  
         * limReactivateHeartBeatTimer() calls to 
         * limReactivateTimer() calls.
         * 
         ******/
        //limReactivateTimer(pMac, eLIM_HEART_BEAT_TIMER, psessionEntry);
        limReactivateHeartBeatTimer(pMac, psessionEntry);

        // Reset number of beacons received
        limResetHBPktCount(psessionEntry);
        return;
    }
    if (((psessionEntry->limSystemRole == eLIM_STA_ROLE)||(psessionEntry->limSystemRole == eLIM_BT_AMP_STA_ROLE))&&
         (psessionEntry->limMlmState == eLIM_MLM_LINK_ESTABLISHED_STATE))
    {
        if (!pMac->sys.gSysEnableLinkMonitorMode)
            return;

        /**
         * Beacon frame not received within heartbeat timeout.
         */
        PELOGW(limLog(pMac, LOGW, FL("Heartbeat Failure\n"));)
        pMac->lim.gLimHBfailureCntInLinkEstState++;

        /**
         * Send Probe Request frame to AP to see if
         * it is still around. Wait until certain
         * timeout for Probe Response from AP.
         */
        PELOGW(limLog(pMac, LOGW, FL("Heart Beat missed from AP. Sending Probe Req\n"));)
        /* for searching AP, we don't include any additional IE */
        limSendProbeReqMgmtFrame(pMac, &psessionEntry->ssId, psessionEntry->bssId,
                                  psessionEntry->currentOperChannel,psessionEntry->selfMacAddr,
                                  psessionEntry->dot11mode, 0, NULL);

        //assign the sessionId to the timer object

        limDeactivateAndChangeTimer(pMac, eLIM_PROBE_AFTER_HB_TIMER);
        MTRACE(macTrace(pMac, TRACE_CODE_TIMER_ACTIVATE, 0, eLIM_PROBE_AFTER_HB_TIMER));
        pMac->lim.limTimers.gLimProbeAfterHBTimer.sessionId = psessionEntry->peSessionId;
        if (tx_timer_activate(&pMac->lim.limTimers.gLimProbeAfterHBTimer) != TX_SUCCESS)
        {
            limLog(pMac, LOGP, FL("Fail to re-activate Probe-after-heartbeat timer\n"));
            limReactivateHeartBeatTimer(pMac, psessionEntry);
        }
    }
    else
    {
        /**
             * Heartbeat timer may have timed out
            * while we're doing background scanning/learning
            * or in states other than link-established state.
            * Log error.
            */
        PELOG1(limLog(pMac, LOG1, FL("received heartbeat timeout in state %X\n"),
               psessionEntry->limMlmState);)
        limPrintMlmState(pMac, LOG1, psessionEntry->limMlmState);
        pMac->lim.gLimHBfailureCntInOtherStates++;
        limReactivateHeartBeatTimer(pMac, psessionEntry);
    }
} /*** limHandleHeartBeatFailure() ***/
