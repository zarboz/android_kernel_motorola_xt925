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
 * This file limProcessMlmMessages.cc contains the code
 * for processing MLM request messages.
 * Author:        Chandra Modumudi
 * Date:          02/12/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#include "palTypes.h"
#ifdef ANI_PRODUCT_TYPE_AP
#include "wniCfgAp.h"
#else
#include "wniCfgSta.h"
#endif
#include "aniGlobal.h"
#include "sirApi.h"
#include "sirParams.h"
#include "cfgApi.h"

#include "schApi.h"
#include "utilsApi.h"
#include "limUtils.h"
#include "limAssocUtils.h"
#include "limPropExtsUtils.h"
#include "limSecurityUtils.h"
#include "limSendMessages.h"
#include "pmmApi.h"
#include "limSendMessages.h"
//#include "limSessionUtils.h"
#include "limSessionUtils.h"
#ifdef WLAN_FEATURE_VOWIFI_11R
#include <limFT.h>
#endif



// MLM REQ processing function templates
static void limProcessMlmStartReq(tpAniSirGlobal, tANI_U32 *);
static void limProcessMlmScanReq(tpAniSirGlobal, tANI_U32 *);
static void limProcessMlmJoinReq(tpAniSirGlobal, tANI_U32 *);
static void limProcessMlmAuthReq(tpAniSirGlobal, tANI_U32 *);
static void limProcessMlmAssocReq(tpAniSirGlobal, tANI_U32 *);
static void limProcessMlmReassocReq(tpAniSirGlobal, tANI_U32 *);
static void limProcessMlmDisassocReq(tpAniSirGlobal, tANI_U32 *);
static void limProcessMlmDeauthReq(tpAniSirGlobal, tANI_U32 *);
static void limProcessMlmSetKeysReq(tpAniSirGlobal, tANI_U32 *);

static void limProcessMlmAddBAReq( tpAniSirGlobal, tANI_U32 * );
static void limProcessMlmAddBARsp( tpAniSirGlobal, tANI_U32 * );
static void limProcessMlmDelBAReq( tpAniSirGlobal, tANI_U32 * );

// MLM Timeout event handler templates
static void limProcessMinChannelTimeout(tpAniSirGlobal);
static void limProcessMaxChannelTimeout(tpAniSirGlobal);
static void limProcessJoinFailureTimeout(tpAniSirGlobal);
static void limProcessAuthFailureTimeout(tpAniSirGlobal);
static void limProcessAuthRspTimeout(tpAniSirGlobal, tANI_U32);
static void limProcessAssocFailureTimeout(tpAniSirGlobal, tANI_U32);

static void limProcessMlmRemoveKeyReq(tpAniSirGlobal pMac, tANI_U32 * pMsgBuf);
void 
limSetChannel(tpAniSirGlobal pMac, tANI_U32 titanHtcap, tANI_U8 channel, tPowerdBm maxTxPower, tANI_U8 peSessionId);


/*
 * determine the secondary channel state for hal
 */
static ePhyChanBondState
mlm_get_ext_chnl(
    tpAniSirGlobal      pMac,
    tAniCBSecondaryMode chnl)
{
    if (chnl == eANI_CB_SECONDARY_UP)
        return PHY_DOUBLE_CHANNEL_LOW_PRIMARY;
    if (chnl == eANI_CB_SECONDARY_DOWN)
        return PHY_DOUBLE_CHANNEL_HIGH_PRIMARY;
    return PHY_SINGLE_CHANNEL_CENTERED;

}


#define IS_MLM_SCAN_REQ_BACKGROUND_SCAN_AGGRESSIVE(pMac)    (pMac->lim.gpLimMlmScanReq->backgroundScanMode == eSIR_AGGRESSIVE_BACKGROUND_SCAN)


/**
 * limProcessMlmReqMessages()
 *
 *FUNCTION:
 * This function is called by limPostMlmMessage(). This
 * function handles MLM primitives invoked by SME.
 *
 *LOGIC:
 * Depending on the message type, corresponding function will be
 * called.
 *
 *ASSUMPTIONS:
 * 1. Upon receiving Beacon in WT_JOIN_STATE, MLM module invokes
 *    APIs exposed by Beacon Processing module for setting parameters
 *    at MAC hardware.
 * 2. If attempt to Reassociate with an AP fails, link with current
 *    AP is restored back.
 *
 *NOTE:
 *
 * @param pMac      Pointer to Global MAC structure
 * @param msgType   Indicates the MLM primitive message type
 * @param *pMsgBuf  A pointer to the MLM message buffer
 *
 * @return None
 */

void
limProcessMlmReqMessages(tpAniSirGlobal pMac, tpSirMsgQ Msg)
{
    switch (Msg->type)
    {
        case LIM_MLM_START_REQ:             limProcessMlmStartReq(pMac, Msg->bodyptr);   break;
        case LIM_MLM_SCAN_REQ:              limProcessMlmScanReq(pMac, Msg->bodyptr);    break;
        case LIM_MLM_JOIN_REQ:              limProcessMlmJoinReq(pMac, Msg->bodyptr);    break;
        case LIM_MLM_AUTH_REQ:              limProcessMlmAuthReq(pMac, Msg->bodyptr);    break;
        case LIM_MLM_ASSOC_REQ:             limProcessMlmAssocReq(pMac, Msg->bodyptr);   break;
        case LIM_MLM_REASSOC_REQ:           limProcessMlmReassocReq(pMac, Msg->bodyptr); break;
        case LIM_MLM_DISASSOC_REQ:          limProcessMlmDisassocReq(pMac, Msg->bodyptr);  break;
        case LIM_MLM_DEAUTH_REQ:            limProcessMlmDeauthReq(pMac, Msg->bodyptr);  break;
        case LIM_MLM_SETKEYS_REQ:           limProcessMlmSetKeysReq(pMac, Msg->bodyptr);  break;
        case LIM_MLM_REMOVEKEY_REQ:         limProcessMlmRemoveKeyReq(pMac, Msg->bodyptr); break;
        case SIR_LIM_MIN_CHANNEL_TIMEOUT:   limProcessMinChannelTimeout(pMac);  break;
        case SIR_LIM_MAX_CHANNEL_TIMEOUT:   limProcessMaxChannelTimeout(pMac);  break;
        case SIR_LIM_JOIN_FAIL_TIMEOUT:     limProcessJoinFailureTimeout(pMac);  break;
        case SIR_LIM_AUTH_FAIL_TIMEOUT:     limProcessAuthFailureTimeout(pMac);  break;
        case SIR_LIM_AUTH_RSP_TIMEOUT:      limProcessAuthRspTimeout(pMac, Msg->bodyval);  break;
        case SIR_LIM_ASSOC_FAIL_TIMEOUT:    limProcessAssocFailureTimeout(pMac, Msg->bodyval);  break;
#ifdef WLAN_FEATURE_VOWIFI_11R
        case SIR_LIM_FT_PREAUTH_RSP_TIMEOUT:limProcessFTPreauthRspTimeout(pMac); break;
#endif
#ifdef WLAN_FEATURE_P2P
        case SIR_LIM_REMAIN_CHN_TIMEOUT:    limProcessRemainOnChnTimeout(pMac); break;
#endif
        case LIM_MLM_ADDBA_REQ:             limProcessMlmAddBAReq( pMac, Msg->bodyptr ); break;
        case LIM_MLM_ADDBA_RSP:             limProcessMlmAddBARsp( pMac, Msg->bodyptr ); break;
        case LIM_MLM_DELBA_REQ:             limProcessMlmDelBAReq( pMac, Msg->bodyptr ); break;
        case LIM_MLM_TSPEC_REQ:                 
        default:
            break;
    } // switch (msgType)
} /*** end limProcessMlmReqMessages() ***/


/**
 * limSetScanMode()
 *
 *FUNCTION:
 * This function is called to setup system into Scan mode
 *
 *LOGIC:
 * NA
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @return None
 */

void
limSetScanMode(tpAniSirGlobal pMac)
{
    tSirLinkTrafficCheck checkTraffic;

    /// Set current scan channel id to the first in the channel list
    pMac->lim.gLimCurrentScanChannelId = 0;

#ifdef ANI_PRODUCT_TYPE_CLIENT         
       if ( IS_MLM_SCAN_REQ_BACKGROUND_SCAN_AGGRESSIVE(pMac) )
           checkTraffic = eSIR_DONT_CHECK_LINK_TRAFFIC_BEFORE_SCAN;
       else 
           checkTraffic = eSIR_CHECK_LINK_TRAFFIC_BEFORE_SCAN;
#else
            /* Currently checking the traffic before scan for Linux station. This is because MLM
             * scan request is not filled as scan is received via Measurement req in Linux. This
             * should be made as common code for Windows/Linux station once the scan requests are
             * enabled in Linux
             * TODO */
            checkTraffic = eSIR_CHECK_LINK_TRAFFIC_BEFORE_SCAN;
#endif

    PELOG1(limLog(pMac, LOG1, FL("Calling limSendHalInitScanReq\n"));)
    limSendHalInitScanReq(pMac, eLIM_HAL_INIT_SCAN_WAIT_STATE, checkTraffic);

    return ;
} /*** end limSetScanMode() ***/

//WLAN_SUSPEND_LINK Related

/**
 * limSuspendLink()
 *
 *FUNCTION:
 * This function is called to suspend traffic. Internally this function uses WDA_INIT_SCAN_REQ.
 *
 *LOGIC:
 * NA
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param trafficCheck - Takes value from enum tSirLinkTrafficCheck. 
 * @param callback - Callback function to be called after suspending the link.
 * @param data - Pointer to any buffer that will be passed to callback.
 * @return None
 */
void
limSuspendLink(tpAniSirGlobal pMac, tSirLinkTrafficCheck trafficCheck,  SUSPEND_RESUME_LINK_CALLBACK callback, tANI_U32 *data)
{
   if( NULL == callback )
   {
      limLog( pMac, LOGE, "%s:%d: Invalid parameters\n", __FUNCTION__, __LINE__ );
      return;
   }

   if( pMac->lim.gpLimSuspendCallback )
   {
      limLog( pMac, LOGE, "%s:%d: gLimSuspendLink callback is not NULL...something is wrong\n", __FUNCTION__, __LINE__ );
      callback( pMac, eHAL_STATUS_FAILURE, data ); 
      return;
   }

   pMac->lim.gLimSystemInScanLearnMode = 1;
   pMac->lim.gpLimSuspendCallback = callback;
   pMac->lim.gpLimSuspendData = data;
   limSendHalInitScanReq(pMac, eLIM_HAL_SUSPEND_LINK_WAIT_STATE, trafficCheck );
}

/**
 * limResumeLink()
 *
 *FUNCTION:
 * This function is called to Resume traffic after a suspend. Internally this function uses WDA_FINISH_SCAN_REQ.
 *
 *LOGIC:
 * NA
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param callback - Callback function to be called after Resuming the link.
 * @param data - Pointer to any buffer that will be passed to callback.
 * @return None
 */
void
limResumeLink(tpAniSirGlobal pMac, SUSPEND_RESUME_LINK_CALLBACK callback, tANI_U32 *data)
{
   if( NULL == callback )
   {
      limLog( pMac, LOGE, "%s:%d: Invalid parameters\n", __FUNCTION__, __LINE__ );
      return;
   }

   if( pMac->lim.gpLimResumeCallback )
   {
      limLog( pMac, LOGE, "%s:%d: gLimResumeLink callback is not NULL...something is wrong\n", __FUNCTION__, __LINE__ );
      callback( pMac, eHAL_STATUS_FAILURE, data ); 
      return;
   }

   pMac->lim.gpLimResumeCallback = callback;
   pMac->lim.gpLimResumeData = data;
   limSendHalFinishScanReq(pMac, eLIM_HAL_RESUME_LINK_WAIT_STATE );
}
//end WLAN_SUSPEND_LINK Related


/**
 *
 * limChangeChannelWithCallback()
 *
 * FUNCTION:
 * This function is called to change channel and perform off channel operation
 * if required. The caller registers a callback to be called at the end of the
 * channel change. 
 *
 */
void
limChangeChannelWithCallback(tpAniSirGlobal pMac, tANI_U8 newChannel, 
    CHANGE_CHANNEL_CALLBACK callback, 
    tANI_U32 *cbdata, tpPESession psessionEntry)
{
    // Sanity checks for the current and new channel
#if defined WLAN_VOWIFI_DEBUG
        PELOGE(limLog( pMac, LOGE, "Switching channel to %d\n", newChannel);)
#endif
    psessionEntry->channelChangeReasonCode=LIM_SWITCH_CHANNEL_OPERATION;

    pMac->lim.gpchangeChannelCallback = callback;
    pMac->lim.gpchangeChannelData = cbdata;

    limSendSwitchChnlParams(pMac, newChannel,
        eHT_SECONDARY_CHANNEL_OFFSET_NONE,
        psessionEntry->maxTxPower, psessionEntry->peSessionId);

    return;
}


/**
 * limContinuePostChannelScan()
 *
 *FUNCTION:
 * This function is called to scan the current channel.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac    - Pointer to Global MAC structure
 *
 * @return None
 */

void limContinuePostChannelScan(tpAniSirGlobal pMac)
{
    tANI_U8 channelNum;
    tANI_U8 handleError = 0;
    tANI_U8 i = 0;
    tSirRetStatus status = eSIR_SUCCESS;
    
    if( pMac->lim.abortScan || (NULL == pMac->lim.gpLimMlmScanReq ) )
    {
        pMac->lim.abortScan = 0;
        limSendHalFinishScanReq(pMac, eLIM_HAL_FINISH_SCAN_WAIT_STATE);
        return;
    }


    channelNum = limGetCurrentScanChannel(pMac);
    if ((pMac->lim.gpLimMlmScanReq->scanType == eSIR_ACTIVE_SCAN) &&
        (limActiveScanAllowed(pMac, channelNum)))
    {
        PELOG2(limLog(pMac, LOG2, FL("ACTIVE Scan chan %d, sending probe\n"), channelNum);)

        do
        {
            /* Prepare and send Probe Request frame for all the SSIDs present in the saved MLM 
                    */
       
            PELOGE(limLog(pMac, LOGW, FL("sending ProbeReq number %d, for SSID %s on channel: %d\n"), 
                                                i, pMac->lim.gpLimMlmScanReq->ssId[i].ssId, channelNum);)
            status = limSendProbeReqMgmtFrame( pMac, &pMac->lim.gpLimMlmScanReq->ssId[i],
               pMac->lim.gpLimMlmScanReq->bssId, channelNum, pMac->lim.gSelfMacAddr, 
               pMac->lim.gpLimMlmScanReq->dot11mode,
               pMac->lim.gpLimMlmScanReq->uIEFieldLen,
               (tANI_U8 *)(pMac->lim.gpLimMlmScanReq)+pMac->lim.gpLimMlmScanReq->uIEFieldOffset);
            
            if ( status != eSIR_SUCCESS)
            {
                PELOGE(limLog(pMac, LOGE, FL("send ProbeReq failed for SSID %s on channel: %d\n"), 
                                                pMac->lim.gpLimMlmScanReq->ssId[i].ssId, channelNum);)
                limDeactivateAndChangeTimer(pMac, eLIM_MIN_CHANNEL_TIMER);
                limSendHalEndScanReq(pMac, channelNum, eLIM_HAL_END_SCAN_WAIT_STATE);
                return;
            }
            i++;
        } while (i < pMac->lim.gpLimMlmScanReq->numSsid);

        {
#if defined WLAN_FEATURE_VOWIFI
           //If minChannelTime is set to zero, SME is requesting scan to not use min channel timer.
           //This is used in 11k to request for beacon measurement request with a fixed duration in
           //max channel time.
           if( pMac->lim.gpLimMlmScanReq->minChannelTime != 0 )
           {
#endif
            /// TXP has sent Probe Request
            /// Activate minChannelTimer
            limDeactivateAndChangeTimer(pMac, eLIM_MIN_CHANNEL_TIMER);

#ifdef GEN6_TODO
            /* revisit this piece of code to assign the appropriate sessionId below
             * priority - LOW/might not be needed
             */ 
            pMac->lim.limTimers.gLimMinChannelTimer.sessionId = sessionId;
#endif            
            
            MTRACE(macTrace(pMac, TRACE_CODE_TIMER_ACTIVATE, 0, eLIM_MIN_CHANNEL_TIMER));

            if (tx_timer_activate(&pMac->lim.limTimers.gLimMinChannelTimer) != TX_SUCCESS)
            {
                limLog(pMac, LOGP, FL("could not start min channel timer\n"));
                return;
            }

            // Initialize max timer too
            limDeactivateAndChangeTimer(pMac, eLIM_MAX_CHANNEL_TIMER);
#if defined WLAN_FEATURE_VOWIFI
        }
           else
           {
#if defined WLAN_VOWIFI_DEBUG
              PELOGE(limLog( pMac, LOGE, "Min channel time == 0, Use only max chan timer\n" );)
#endif
              //No Need to start Min channel timer. Start Max Channel timer.
              limDeactivateAndChangeTimer(pMac, eLIM_MAX_CHANNEL_TIMER);
              if (tx_timer_activate(&pMac->lim.limTimers.gLimMaxChannelTimer)
                    == TX_TIMER_ERROR)
              {
                 /// Could not activate max channel timer.
                 // Log error
                 limLog(pMac,LOGP, FL("could not start max channel timer\n"));
                 return; 
              }

    }
#endif
        }
    }
    else
    {
        tANI_U32 val;
        PELOG2(limLog(pMac, LOG2, FL("START PASSIVE Scan chan %d\n"), channelNum);)

        /// Passive Scanning. Activate maxChannelTimer
        MTRACE(macTrace(pMac, TRACE_CODE_TIMER_DEACTIVATE, 0, eLIM_MAX_CHANNEL_TIMER));
        if (tx_timer_deactivate(&pMac->lim.limTimers.gLimMaxChannelTimer)
                                      != TX_SUCCESS)
        {
            // Could not deactivate max channel timer.
            // Log error
            limLog(pMac, LOGP, FL("Unable to deactivate max channel timer\n"));
            return;
        }
        else
        {
            if (wlan_cfgGetInt(pMac, WNI_CFG_PASSIVE_MAXIMUM_CHANNEL_TIME,    
                          &val) != eSIR_SUCCESS)
            {
                /**
                 * Could not get max channel value
                 * from CFG. Log error.
                 */
                limLog(pMac, LOGP, FL("could not retrieve passive max channel value\n"));
                return;
            }
            else
            {
                tANI_U32 val1 = 0;

                val = SYS_MS_TO_TICKS(val);
#ifdef ANI_PRODUCT_TYPE_CLIENT
                // If a background was triggered via Quiet BSS,
                // then we need to adjust the MIN and MAX channel
                // timer's accordingly to the Quiet duration that
                // was specified
                if( eLIM_QUIET_RUNNING == pMac->lim.gLimSpecMgmt.quietState &&
                    pMac->lim.gLimTriggerBackgroundScanDuringQuietBss )
                {
                    // gLimQuietDuration is already cached in units of
                    // system ticks. No conversion is reqd...
                    val1 = pMac->lim.gLimSpecMgmt.quietDuration;
                }
                else
                {
                    val1 = SYS_MS_TO_TICKS(pMac->lim.gpLimMlmScanReq->maxChannelTime);
                }
#endif
                //Pick the longer stay time
                val = (val > val1) ? val : val1;
                MTRACE(macTrace(pMac, TRACE_CODE_TIMER_ACTIVATE, 0, eLIM_MAX_CHANNEL_TIMER));
                if (tx_timer_change(&pMac->lim.limTimers.gLimMaxChannelTimer,
                                val, 0) != TX_SUCCESS)
                {
                    // Could not change max channel timer.
                    // Log error
                    limLog(pMac, LOGP, FL("Unable to change max channel timer\n"));
                    return;
                }
                else if (tx_timer_activate(&pMac->lim.limTimers.gLimMaxChannelTimer) != TX_SUCCESS)
                {
                    limLog(pMac, LOGP, FL("could not start max channel timer\n"));
                    return;
                }
          
            }
        }
        // Wait for Beacons to arrive
    } // if (pMac->lim.gLimMlmScanReq->scanType == eSIR_ACTIVE_SCAN)

    if( handleError )
    {
        //
        // FIXME - With this, LIM/SoftMAC will try and recover
        // state, but eWNI_SME_SCAN_CNF maybe reporting an
        // incorrect status back to the SME. Some of the possible
        // errors are:
        // eSIR_SME_HAL_SCAN_INIT_FAILED
        // eSIR_SME_RESOURCES_UNAVAILABLE
        //
        limSendHalFinishScanReq( pMac, eLIM_HAL_FINISH_SCAN_WAIT_STATE );
        //limCompleteMlmScan(pMac, eSIR_SME_HAL_SCAN_INIT_FAILED);
    }
    else
    {
        limAddScanChannelInfo(pMac, channelNum);
    }

    return;
}




/*
* Creates a Raw frame to be sent before every Scan, if required.
* If only infra link is active (mlmState = Link Estb), then send Data Null
* If only BT-AMP-AP link is active(mlmState = BSS_STARTED), then send CTS2Self frame.
* If only BT-AMP-STA link is active(mlmState = BSS_STARTED or Link Est) then send CTS2Self
* If Only IBSS link is active, then send CTS2Self
* for concurrent scenario: Infra+BT  or Infra+IBSS, always send CTS2Self, no need to send Data Null
*
*/
static void __limCreateInitScanRawFrame(tpAniSirGlobal pMac, 
                                        tpInitScanParams pInitScanParam)
{
    tANI_U8   i;
    pInitScanParam->scanEntry.activeBSScnt = 0;
    
    /* Don't send CTS to self as we have issue with BTQM queues where BTQM can 
     * not handle transmition of CTS2self frames.  Sending CTS 2 self at this 
     * juncture also doesn't serve much purpose as probe request frames go out 
     * immediately, No need to notify BSS in IBSS case.
     * */

    for(i =0; i < pMac->lim.maxBssId; i++)
    {
        if(pMac->lim.gpSession[i].valid == TRUE)
        {
            if(pMac->lim.gpSession[i].limMlmState == eLIM_MLM_LINK_ESTABLISHED_STATE)
            {
               if ((pMac->lim.gpSession[i].limSystemRole != eLIM_BT_AMP_STA_ROLE) &&
                   (pInitScanParam->scanEntry.activeBSScnt < HAL_NUM_BSSID))
                {
                    pInitScanParam->scanEntry.bssIdx[pInitScanParam->scanEntry.activeBSScnt] 
                        = pMac->lim.gpSession[i].bssIdx;
                    pInitScanParam->scanEntry.activeBSScnt++;

                }
            }
#ifdef WLAN_FEATURE_P2P
            else if( (eLIM_AP_ROLE == pMac->lim.gpSession[i].limSystemRole ) 
                    && ( VOS_P2P_GO_MODE == pMac->lim.gpSession[i].pePersona )
                   )
            {
                pInitScanParam->useNoA = TRUE;
            }
#endif
        }
    }
    if (pInitScanParam->scanEntry.activeBSScnt)
    {
        pInitScanParam->notifyBss = TRUE;
        pInitScanParam->frameType = SIR_MAC_DATA_FRAME;
        pInitScanParam->frameLength = 0;
    }
}

/*
* Creates a Raw frame to be sent during finish scan, if required.
* Send data null frame, only when there is just one session active and that session is
* in 'link Estb' state.
* if more than one session is active, don't send any frame.
* for concurrent scenario: Infra+BT  or Infra+IBSS, no need to send Data Null
*
*/
static void __limCreateFinishScanRawFrame(tpAniSirGlobal pMac, 
                                          tpFinishScanParams pFinishScanParam)
{
    tANI_U8   i;
    pFinishScanParam->scanEntry.activeBSScnt = 0;

    for(i =0; i < pMac->lim.maxBssId; i++)
    {
        if(pMac->lim.gpSession[i].valid == TRUE)
        {
            if(pMac->lim.gpSession[i].limMlmState == eLIM_MLM_LINK_ESTABLISHED_STATE)
            {
                //BT-STA can either be in LINK-ESTB state or BSS_STARTED State
                //for BT, need to send CTS2Self
               if ((pMac->lim.gpSession[i].limSystemRole != eLIM_BT_AMP_STA_ROLE) &&
                   (pFinishScanParam->scanEntry.activeBSScnt < HAL_NUM_BSSID))
                {
                    pFinishScanParam->scanEntry.bssIdx[pFinishScanParam->scanEntry.activeBSScnt] 
                        = pMac->lim.gpSession[i].bssIdx;
                    pFinishScanParam->scanEntry.activeBSScnt++;
                }
            }
        }
    }
    
    if (pFinishScanParam->scanEntry.activeBSScnt)
    {
        pFinishScanParam->notifyBss = TRUE;
        pFinishScanParam->frameType = SIR_MAC_DATA_FRAME;
        pFinishScanParam->frameLength = 0;
    }
}

void
limSendHalInitScanReq(tpAniSirGlobal pMac, tLimLimHalScanState nextState, tSirLinkTrafficCheck trafficCheck)
{


    tSirMsgQ                msg;
    tpInitScanParams        pInitScanParam;
    tSirRetStatus           rc = eSIR_SUCCESS;

    if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd, (void **)&pInitScanParam,
                sizeof(*pInitScanParam)))
    {
        PELOGW(limLog(pMac, LOGW, FL("palAllocateMemory() failed\n"));)
        goto error;
    }
    
    /*Initialize the pInitScanParam with 0*/
    palZeroMemory( pMac->hHdd, (tANI_U8 *)pInitScanParam, sizeof(*pInitScanParam));

    msg.type = WDA_INIT_SCAN_REQ;
    msg.bodyptr = pInitScanParam;
    msg.bodyval = 0;

    palZeroMemory( pMac->hHdd, (tANI_U8 *)&pInitScanParam->macMgmtHdr, sizeof(tSirMacMgmtHdr));
    if (nextState == eLIM_HAL_INIT_LEARN_WAIT_STATE)
    {
        pInitScanParam->notifyBss = TRUE;
        pInitScanParam->notifyHost = FALSE;
        pInitScanParam->scanMode = eHAL_SYS_MODE_LEARN;

#if defined(ANI_AP_CLIENT_SDK) 
        if (GET_LIM_SYSTEM_ROLE(pMac) == eLIM_STA_ROLE)
        {
            pInitScanParam->frameType = SIR_MAC_DATA_NULL;
            // We need to inform the AP only when we are
            // in the LINK_ESTABLISHED state
            if( eLIM_SME_LINK_EST_WT_SCAN_STATE != pMac->lim.gLimSmeState )
            {
                pInitScanParam->notifyBss = FALSE;
                // FIXME - Handle this one carefully
                pInitScanParam->notifyHost = FALSE;
            }
            __limCreateInitScanRawFrame(pMac, pInitScanParam);
            pInitScanParam->checkLinkTraffic = trafficCheck;
        }
        else
#endif
        {
            pInitScanParam->frameType = SIR_MAC_CTRL_CTS;
            __limCreateInitScanRawFrame(pMac, pInitScanParam);
            pInitScanParam->checkLinkTraffic = trafficCheck;
        }

#if (defined(ANI_PRODUCT_TYPE_AP) ||defined(ANI_PRODUCT_TYPE_AP_SDK))
        /* Currently using the AP's scanDuration values for Linux station also. This should
         * be revisited if this needs to changed depending on AP or Station */
        {
            if (pMac->lim.gpLimMeasReq->measControl.longChannelScanPeriodicity &&
                    (pMac->lim.gLimMeasParams.shortDurationCount ==
                     pMac->lim.gpLimMeasReq->measControl.longChannelScanPeriodicity))
            {
#ifdef ANI_AP_SDK
                pInitScanParam->scanDuration = (tANI_U16)pMac->lim.gLimScanDurationConvert.longChannelScanDuration_tick;
#else
                pInitScanParam->scanDuration = (tANI_U16)pMac->lim.gpLimMeasReq->measDuration.longChannelScanDuration;
#endif /* ANI_AP_SDK */
            }
            else
            {
#ifdef ANI_AP_SDK
                pInitScanParam->scanDuration = pMac->lim.gLimScanDurationConvert.shortChannelScanDuration_tick;
#else
                pInitScanParam->scanDuration = (tANI_U16)pMac->lim.gpLimMeasReq->measDuration.shortChannelScanDuration;
#endif /* ANI_AP_SDK */
            }
        }
#endif       //#if (defined(ANI_PRODUCT_TYPE_AP) ||defined(ANI_PRODUCT_TYPE_AP_SDK))
    }
    else
    {
        pInitScanParam->scanMode = eHAL_SYS_MODE_SCAN;
        __limCreateInitScanRawFrame(pMac, pInitScanParam);
#ifdef WLAN_FEATURE_P2P
        if (pInitScanParam->useNoA)
        {
            pInitScanParam->scanDuration = pMac->lim.gTotalScanDuration;
        }
#endif       
        /* Inform HAL whether it should check for traffic on the link
         * prior to performing a background scan
         */
        pInitScanParam->checkLinkTraffic = trafficCheck;
    }

    pMac->lim.gLimHalScanState = nextState;
    SET_LIM_PROCESS_DEFD_MESGS(pMac, false);
    MTRACE(macTraceMsgTx(pMac, 0, msg.type));

    rc = wdaPostCtrlMsg(pMac, &msg);
    if (rc == eSIR_SUCCESS) {
        PELOG3(limLog(pMac, LOG3, FL("wdaPostCtrlMsg() return eSIR_SUCCESS pMac=%x nextState=%d\n"),
                    pMac, pMac->lim.gLimHalScanState);)
            return;
    }

    SET_LIM_PROCESS_DEFD_MESGS(pMac, true);
    palFreeMemory(pMac->hHdd, (void *)pInitScanParam);
    PELOGW(limLog(pMac, LOGW, FL("wdaPostCtrlMsg failed, error code %d\n"), rc);)

error:
    switch(nextState)
    {
        case eLIM_HAL_START_SCAN_WAIT_STATE:
            limCompleteMlmScan(pMac, eSIR_SME_HAL_SCAN_INIT_FAILED);
            break;

#if defined(ANI_PRODUCT_TYPE_AP) && (WNI_POLARIS_FW_PACKAGE == ADVANCED)
        case eLIM_HAL_START_LEARN_WAIT_STATE:
            //            if (pMac->lim.gLimSystemRole == eLIM_AP_ROLE)
            {
                limRestorePreLearnState(pMac);
                limReEnableLearnMode(pMac);
            }
            break;

#endif

            //WLAN_SUSPEND_LINK Related
        case eLIM_HAL_SUSPEND_LINK_WAIT_STATE:
            pMac->lim.gLimHalScanState = eLIM_HAL_IDLE_SCAN_STATE;
            if( pMac->lim.gpLimSuspendCallback )
            {
                pMac->lim.gpLimSuspendCallback( pMac, rc, pMac->lim.gpLimSuspendData );
                pMac->lim.gpLimSuspendCallback = NULL;
                pMac->lim.gpLimSuspendData = NULL;
            }
            pMac->lim.gLimSystemInScanLearnMode = 0;
            break;
            //end WLAN_SUSPEND_LINK Related
        default:
            break;
    }
    pMac->lim.gLimHalScanState = eLIM_HAL_IDLE_SCAN_STATE;

    return ;
}

void
limSendHalStartScanReq(tpAniSirGlobal pMac, tANI_U8 channelNum, tLimLimHalScanState nextState)
{
    tSirMsgQ            msg;
    tpStartScanParams   pStartScanParam;
    tSirRetStatus       rc = eSIR_SUCCESS;

    /**
     * The Start scan request to be sent only if Start Scan is not already requested
     */
    if(pMac->lim.gLimHalScanState != eLIM_HAL_START_SCAN_WAIT_STATE) 
    { 

        if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd, 
                    (void **)&pStartScanParam,
                    sizeof(*pStartScanParam)))
        {
            PELOGW(limLog(pMac, LOGW, FL("palAllocateMemory() failed\n"));)
                goto error;
        }

        msg.type = WDA_START_SCAN_REQ;
        msg.bodyptr = pStartScanParam;
        msg.bodyval = 0;
        pStartScanParam->status = eHAL_STATUS_SUCCESS;
        pStartScanParam->scanChannel = (tANI_U8)channelNum;

        pMac->lim.gLimHalScanState = nextState;
        SET_LIM_PROCESS_DEFD_MESGS(pMac, false);

        MTRACE(macTraceMsgTx(pMac, 0, msg.type));
        PELOGW(limLog(pMac, LOGW, FL("Channel %d\n"), channelNum);)

            rc = wdaPostCtrlMsg(pMac, &msg);
        if (rc == eSIR_SUCCESS) {
            return;
        }

        SET_LIM_PROCESS_DEFD_MESGS(pMac, true);
        palFreeMemory(pMac->hHdd, (void *)pStartScanParam);
        PELOGW(limLog(pMac, LOGW, FL("wdaPostCtrlMsg failed, error code %d\n"), rc);)

error:
        switch(nextState)
        {
            case eLIM_HAL_START_SCAN_WAIT_STATE:
                limCompleteMlmScan(pMac, eSIR_SME_HAL_SCAN_INIT_FAILED);
                break;

#if defined(ANI_PRODUCT_TYPE_AP) && (WNI_POLARIS_FW_PACKAGE == ADVANCED)
            case eLIM_HAL_START_LEARN_WAIT_STATE:
                //if (pMac->lim.gLimSystemRole == eLIM_AP_ROLE)
                {
                    limRestorePreLearnState(pMac);
                    limReEnableLearnMode(pMac);
                }
                break;

#endif

            default:
                break;
        }
        pMac->lim.gLimHalScanState = eLIM_HAL_IDLE_SCAN_STATE;

    }
    else
    {
        PELOGW(limLog(pMac, LOGW, FL("Invalid state for START_SCAN_REQ message=%d\n"), pMac->lim.gLimHalScanState);)
    }

    return;
}

void limSendHalEndScanReq(tpAniSirGlobal pMac, tANI_U8 channelNum, tLimLimHalScanState nextState)
{
    tSirMsgQ            msg;
    tpEndScanParams     pEndScanParam;
    tSirRetStatus       rc = eSIR_SUCCESS;

    /**
     * The End scan request to be sent only if End Scan is not already requested or
     * Start scan is not already requestd
     */
    if((pMac->lim.gLimHalScanState != eLIM_HAL_END_SCAN_WAIT_STATE)  &&
            (pMac->lim.gLimHalScanState != eLIM_HAL_START_SCAN_WAIT_STATE))
    { 

        if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd, (void **)&pEndScanParam,
                    sizeof(*pEndScanParam)))
        {
            PELOGW(limLog(pMac, LOGW, FL("palAllocateMemory() failed\n"));)
                goto error;
        }

        msg.type = WDA_END_SCAN_REQ;
        msg.bodyptr = pEndScanParam;
        msg.bodyval = 0;
        pEndScanParam->status = eHAL_STATUS_SUCCESS;
        pEndScanParam->scanChannel = (tANI_U8)channelNum;

        pMac->lim.gLimHalScanState = nextState;
        SET_LIM_PROCESS_DEFD_MESGS(pMac, false);
        MTRACE(macTraceMsgTx(pMac, 0, msg.type));

        rc = wdaPostCtrlMsg(pMac, &msg);
        if (rc == eSIR_SUCCESS) {
            return;
        }

        SET_LIM_PROCESS_DEFD_MESGS(pMac, true);
        palFreeMemory(pMac->hHdd, (void *)pEndScanParam);
        PELOGW(limLog(pMac, LOGW, FL("wdaPostCtrlMsg failed, error code %d\n"), rc);)

            error:
            switch(nextState)
            {
                case eLIM_HAL_END_SCAN_WAIT_STATE:
                    limCompleteMlmScan(pMac, eSIR_SME_HAL_SCAN_END_FAILED);
                    break;

#if defined(ANI_PRODUCT_TYPE_AP) && (WNI_POLARIS_FW_PACKAGE == ADVANCED)
                case eLIM_HAL_END_LEARN_WAIT_STATE:
                    //            if (pMac->lim.gLimSystemRole == eLIM_AP_ROLE)
                    {
                        limRestorePreLearnState(pMac);
                        limReEnableLearnMode(pMac);
                    }
                    break;
#endif

                default:
                    PELOGW(limLog(pMac, LOGW, FL("wdaPostCtrlMsg Rcvd invalid nextState %d\n"), nextState);)
                        break;
            }
        pMac->lim.gLimHalScanState = eLIM_HAL_IDLE_SCAN_STATE;
        PELOGW(limLog(pMac, LOGW, FL("wdaPostCtrlMsg failed, error code %d\n"), rc);)    
    }
    else
    {
        PELOGW(limLog(pMac, LOGW, FL("Invalid state for END_SCAN_REQ message=%d\n"), pMac->lim.gLimHalScanState);)
    }


    return;
}

/**
 * limSendHalFinishScanReq()
 *
 *FUNCTION:
 * This function is called to finish scan/learn request..
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac    - Pointer to Global MAC structure
 * @param  nextState - this parameters determines whether this call is for scan or learn
 *
 * @return None
 */
void limSendHalFinishScanReq(tpAniSirGlobal pMac, tLimLimHalScanState nextState)
{

    tSirMsgQ            msg;
    tpFinishScanParams  pFinishScanParam;
    tSirRetStatus       rc = eSIR_SUCCESS;

    if(pMac->lim.gLimHalScanState == nextState)
    {
        /*
         * PE may receive multiple probe responses, while waiting for HAL to send 'FINISH_SCAN_RSP' message
         * PE was sending multiple finish scan req messages to HAL
         * this check will avoid that.
         * If PE is already waiting for the 'finish_scan_rsp' message from HAL, it will ignore this request.
         */
        PELOGW(limLog(pMac, LOGW, FL("Next Scan State is same as the current state: %d \n"), nextState);)
            return;
    }


    if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd, (void **)&pFinishScanParam,
                sizeof(*pFinishScanParam)))
    {
        PELOGW(limLog(pMac, LOGW, FL("palAllocateMemory() failed\n"));)
            goto error;
    }

    msg.type = WDA_FINISH_SCAN_REQ;
    msg.bodyptr = pFinishScanParam;
    msg.bodyval = 0;
    pFinishScanParam->currentOperChannel = peGetCurrentChannel(pMac);
    pFinishScanParam->cbState = limGetPhyCBState( pMac );
    palZeroMemory( pMac->hHdd, (tANI_U8 *)&pFinishScanParam->macMgmtHdr, sizeof(tSirMacMgmtHdr));

    if (nextState == eLIM_HAL_FINISH_LEARN_WAIT_STATE)
    {
        //AP - No pkt need to be transmitted
        pFinishScanParam->scanMode = eHAL_SYS_MODE_LEARN;
        pFinishScanParam->notifyBss = FALSE;
        pFinishScanParam->notifyHost = FALSE;
        pFinishScanParam->frameType = 0;
        pFinishScanParam->currentOperChannel = peGetCurrentChannel(pMac);
        pFinishScanParam->cbState = limGetPhyCBState(pMac);
        pFinishScanParam->frameLength = 0;
        pMac->lim.gLimHalScanState = nextState;
    }
    else
    {
        /* If STA is associated with an AP (ie. STA is in
         * LINK_ESTABLISHED state), then STA need to inform
         * the AP via either DATA-NULL
         */
        pFinishScanParam->scanMode = eHAL_SYS_MODE_SCAN;
        pFinishScanParam->notifyHost = FALSE;
        __limCreateFinishScanRawFrame(pMac, pFinishScanParam);
        pFinishScanParam->currentOperChannel = peGetCurrentChannel(pMac);
        pFinishScanParam->cbState = limGetPhyCBState(pMac);
        //WLAN_SUSPEND_LINK Related
        pMac->lim.gLimHalScanState = nextState;
        //end WLAN_SUSPEND_LINK Related
    }

    SET_LIM_PROCESS_DEFD_MESGS(pMac, false);
    MTRACE(macTraceMsgTx(pMac, 0, msg.type));

    rc = wdaPostCtrlMsg(pMac, &msg);
    if (rc == eSIR_SUCCESS) {
        return;
    }
    SET_LIM_PROCESS_DEFD_MESGS(pMac, true);
    palFreeMemory(pMac->hHdd, (void *)pFinishScanParam);
    PELOGW(limLog(pMac, LOGW, FL("wdaPostCtrlMsg failed, error code %d\n"), rc);)

        error:
        if(nextState == eLIM_HAL_FINISH_SCAN_WAIT_STATE)
            limCompleteMlmScan(pMac, eSIR_SME_HAL_SCAN_FINISH_FAILED);
    //WLAN_SUSPEND_LINK Related
        else if ( nextState == eLIM_HAL_RESUME_LINK_WAIT_STATE )
        {
            if( pMac->lim.gpLimResumeCallback )
            {
                pMac->lim.gpLimResumeCallback( pMac, rc, pMac->lim.gpLimResumeData );
                pMac->lim.gpLimResumeCallback = NULL;
                pMac->lim.gpLimResumeData = NULL;
                pMac->lim.gLimSystemInScanLearnMode = 0;
            }
        }
    //end WLAN_SUSPEND_LINK Related
    pMac->lim.gLimHalScanState = eLIM_HAL_IDLE_SCAN_STATE;
    return;
}

/**
 * limContinueChannelScan()
 *
 *FUNCTION:
 * This function is called by limPerformChannelScan().
 * This function is called to continue channel scanning when
 * Beacon/Probe Response frame are received.
 *
 *LOGIC:
 * Scan criteria stored in pMac->lim.gLimMlmScanReq is used
 * to perform channel scan. In this function MLM sub module
 * makes channel switch, sends PROBE REQUEST frame in case of
 * ACTIVE SCANNING, starts min/max channel timers, programs
 * NAV to probeDelay timer and waits for Beacon/Probe Response.
 * Once all required channels are scanned, LIM_MLM_SCAN_CNF
 * primitive is used to send Scan results to SME sub module.
 *
 *ASSUMPTIONS:
 * 1. In case of Active scanning, start MAX channel time iff
 *    MIN channel timer expired and activity is observed on
 *    the channel.
 *
 *NOTE:
 * NA
 *
 * @param pMac      Pointer to Global MAC structure
 * @return None
 */
void
limContinueChannelScan(tpAniSirGlobal pMac)
{
    tANI_U8                channelNum;

    PELOG1(limLog(pMac, LOG1, FL("Continue SCAN : chan %d tot %d\n"),
           pMac->lim.gLimCurrentScanChannelId,
           pMac->lim.gpLimMlmScanReq->channelList.numChannels);)

    if (pMac->lim.gLimCurrentScanChannelId >
        (tANI_U32) (pMac->lim.gpLimMlmScanReq->channelList.numChannels - 1) 
        || pMac->lim.abortScan)
    {
#ifndef ANI_SNIFFER
        pMac->lim.abortScan = 0;

        /// Done scanning all required channels
        limSendHalFinishScanReq(pMac, eLIM_HAL_FINISH_SCAN_WAIT_STATE);
#endif
        return;
    }

    /// Atleast one more channel is to be scanned

    if ((pMac->lim.gLimReturnAfterFirstMatch & 0x40) ||
        (pMac->lim.gLimReturnAfterFirstMatch & 0x80))
    {
        while (pMac->lim.gLimCurrentScanChannelId <=
               (tANI_U32) (pMac->lim.gpLimMlmScanReq->channelList.numChannels - 1))
        {
            if (((limGetCurrentScanChannel(pMac) <= 14) &&
                  pMac->lim.gLim24Band11dScanDone) ||
                ((limGetCurrentScanChannel(pMac) > 14) &&
                  pMac->lim.gLim50Band11dScanDone))
            {
                limLog(pMac, LOGW, FL("skipping chan %d\n"),
                       limGetCurrentScanChannel(pMac));
                pMac->lim.gLimCurrentScanChannelId++;
            }
            else
                break;
        }

        if (pMac->lim.gLimCurrentScanChannelId >
            (tANI_U32) (pMac->lim.gpLimMlmScanReq->channelList.numChannels - 1))
        {
            /// Done scanning all required channels
            limSendHalFinishScanReq(pMac, eLIM_HAL_FINISH_SCAN_WAIT_STATE);
            return;
        }
    }

    channelNum = limGetCurrentScanChannel(pMac);
   PELOG2(limLog(pMac, LOG2, FL("Current Channel to be scanned is %d\n"),
           channelNum);)

    limSendHalStartScanReq(pMac, channelNum, eLIM_HAL_START_SCAN_WAIT_STATE);
    return;
} /*** end limContinueChannelScan() ***/



/**
 * limRestorePreScanState()
 *
 *FUNCTION:
 * This function is called by limContinueChannelScan()
 * to restore HW state prior to entering 'scan state'
 *
 *LOGIC
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 * NA
 *
 * @param pMac      Pointer to Global MAC structure
 * @return None
 */
void
limRestorePreScanState(tpAniSirGlobal pMac)
{
    int i;
    
    /// Deactivate MIN/MAX channel timers if running
    limDeactivateAndChangeTimer(pMac, eLIM_MIN_CHANNEL_TIMER);
    limDeactivateAndChangeTimer(pMac, eLIM_MAX_CHANNEL_TIMER);

    /* Re-activate Heartbeat timers for connected sessions as scan is done */
    for(i=0;i<pMac->lim.maxBssId;i++)
    {
       if(pMac->lim.gpSession[i].valid == FALSE)
          break;
       if(pMac->lim.gpSession[i].limMlmState == eLIM_MLM_LINK_ESTABLISHED_STATE)
       {
          limReactivateHeartBeatTimer(pMac, peFindSessionBySessionId(pMac,i));
       }  
    }
 

    /**
     * clean up message queue.
     * If SME messages, redirect to deferred queue.
     * The rest will be discarded.
     */
    //limCleanupMsgQ(pMac);

    pMac->lim.gLimSystemInScanLearnMode = 0;
    PELOG1(limLog(pMac, LOG1, FL("Scan ended, took %d tu\n"), (tx_time_get() - pMac->lim.scanStartTime));)
} /*** limRestorePreScanState() ***/


static void
mlm_add_sta(
    tpAniSirGlobal  pMac,
    tpAddStaParams  pSta,
    tANI_U8        *pBssid,
    tANI_U8         htCapable,
    tpPESession     psessionEntry)  //psessionEntry  may required in future
{
    tANI_U32 val;
    int      i;
    

    pSta->staType = STA_ENTRY_SELF; // Identifying self

    palCopyMemory( pMac->hHdd,  pSta->bssId,   pBssid,  sizeof( tSirMacAddr ));
    palCopyMemory( pMac->hHdd,  pSta->staMac, psessionEntry->selfMacAddr, sizeof(tSirMacAddr));

    /* Configuration related parameters to be changed to support BT-AMP */

    if( eSIR_SUCCESS != wlan_cfgGetInt( pMac, WNI_CFG_LISTEN_INTERVAL, &val ))
        limLog(pMac, LOGP, FL("Couldn't get LISTEN_INTERVAL\n"));
    
    pSta->listenInterval = (tANI_U16) val;
    
    if (eSIR_SUCCESS != wlan_cfgGetInt(pMac, WNI_CFG_SHORT_PREAMBLE, &val) )
        limLog(pMac, LOGP, FL("Couldn't get SHORT_PREAMBLE\n"));
    pSta->shortPreambleSupported = (tANI_U8)val;

    pSta->assocId               = 0; // Is SMAC OK with this?
    pSta->wmmEnabled            = 0;
    pSta->uAPSD                 = 0;
    pSta->maxSPLen              = 0;
    pSta->us32MaxAmpduDuration  = 0;
    pSta->maxAmpduSize          = 0; // 0: 8k, 1: 16k,2: 32k,3: 64k


    if(IS_DOT11_MODE_HT(psessionEntry->dot11mode)) 
    {
        pSta->htCapable         = htCapable;
#ifdef WLAN_SOFTAP_FEATURE
        pSta->greenFieldCapable = limGetHTCapability( pMac, eHT_GREENFIELD, psessionEntry);
        pSta->txChannelWidthSet = limGetHTCapability( pMac, eHT_SUPPORTED_CHANNEL_WIDTH_SET, psessionEntry );
        pSta->mimoPS            = (tSirMacHTMIMOPowerSaveState)limGetHTCapability( pMac, eHT_MIMO_POWER_SAVE, psessionEntry );
        pSta->rifsMode          = limGetHTCapability( pMac, eHT_RIFS_MODE, psessionEntry );
        pSta->lsigTxopProtection = limGetHTCapability( pMac, eHT_LSIG_TXOP_PROTECTION, psessionEntry );
        pSta->delBASupport      = limGetHTCapability( pMac, eHT_DELAYED_BA, psessionEntry );
        pSta->maxAmpduDensity   = limGetHTCapability( pMac, eHT_MPDU_DENSITY, psessionEntry );
        pSta->maxAmsduSize      = limGetHTCapability( pMac, eHT_MAX_AMSDU_LENGTH, psessionEntry );
        pSta->fDsssCckMode40Mhz = limGetHTCapability( pMac, eHT_DSSS_CCK_MODE_40MHZ, psessionEntry);
        pSta->fShortGI20Mhz     = limGetHTCapability( pMac, eHT_SHORT_GI_20MHZ, psessionEntry);
        pSta->fShortGI40Mhz     = limGetHTCapability( pMac, eHT_SHORT_GI_40MHZ, psessionEntry);
#else
        pSta->greenFieldCapable = limGetHTCapability( pMac, eHT_GREENFIELD );
        pSta->txChannelWidthSet = limGetHTCapability( pMac, eHT_SUPPORTED_CHANNEL_WIDTH_SET );
        pSta->mimoPS            = (tSirMacHTMIMOPowerSaveState)limGetHTCapability( pMac, eHT_MIMO_POWER_SAVE );
        pSta->rifsMode          = limGetHTCapability( pMac, eHT_RIFS_MODE );
        pSta->lsigTxopProtection = limGetHTCapability( pMac, eHT_LSIG_TXOP_PROTECTION );
        pSta->delBASupport      = limGetHTCapability( pMac, eHT_DELAYED_BA );
        pSta->maxAmpduDensity   = limGetHTCapability( pMac, eHT_MPDU_DENSITY );
        pSta->maxAmsduSize      = limGetHTCapability( pMac, eHT_MAX_AMSDU_LENGTH );
        pSta->fDsssCckMode40Mhz = limGetHTCapability( pMac, eHT_DSSS_CCK_MODE_40MHZ);
        pSta->fShortGI20Mhz     = limGetHTCapability( pMac, eHT_SHORT_GI_20MHZ);
        pSta->fShortGI40Mhz     = limGetHTCapability( pMac, eHT_SHORT_GI_40MHZ);

#endif
    }

    limPopulateOwnRateSet(pMac, &pSta->supportedRates, NULL, false,psessionEntry);
    limFillSupportedRatesInfo(pMac, NULL, &pSta->supportedRates,psessionEntry);
    
    limLog( pMac, LOGE, FL( "GF: %d, ChnlWidth: %d, MimoPS: %d, lsigTXOP: %d, dsssCCK: %d, SGI20: %d, SGI40%d\n") ,
                                          pSta->greenFieldCapable, pSta->txChannelWidthSet, pSta->mimoPS, pSta->lsigTxopProtection, 
                                          pSta->fDsssCckMode40Mhz,pSta->fShortGI20Mhz, pSta->fShortGI40Mhz);

#ifdef WLAN_FEATURE_P2P
     if (VOS_P2P_GO_MODE == psessionEntry->pePersona)
     {
         pSta->p2pCapableSta = 1;
     }
#endif

    //Disable BA. It will be set as part of ADDBA negotiation.
    for( i = 0; i < STACFG_MAX_TC; i++ )
    {
        pSta->staTCParams[i].txUseBA = eBA_DISABLE;
        pSta->staTCParams[i].rxUseBA = eBA_DISABLE;
    }
    
}

//
// New HAL interface - WDA_ADD_BSS_REQ
// Package WDA_ADD_BSS_REQ to HAL, in order to start a BSS
//
tSirResultCodes
limMlmAddBss (
    tpAniSirGlobal      pMac,
    tLimMlmStartReq    *pMlmStartReq,
    tpPESession         psessionEntry)
{
    tSirMsgQ msgQ;
    tpAddBssParams pAddBssParams = NULL;
    tANI_U32 retCode;

#ifdef WLAN_SOFTAP_FEATURE
    tANI_U32 val = 0;
#endif
    
    // Package WDA_ADD_BSS_REQ message parameters

    if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd,
                               (void **) &pAddBssParams, sizeof( tAddBssParams )))
    {
        limLog( pMac, LOGE, FL( "Unable to PAL allocate memory during ADD_BSS\n" ));
        // Respond to SME with LIM_MLM_START_CNF
        return eSIR_SME_HAL_SEND_MESSAGE_FAIL;
    }

    palZeroMemory(pMac->hHdd, pAddBssParams, sizeof(tAddBssParams));

    // Fill in tAddBssParams members
    palCopyMemory( pMac->hHdd,  pAddBssParams->bssId, pMlmStartReq->bssId,
                   sizeof( tSirMacAddr ));

    // Fill in tAddBssParams selfMacAddr
    palCopyMemory ( pMac->hHdd,  pAddBssParams->selfMacAddr, 
                    psessionEntry->selfMacAddr,
                   sizeof( tSirMacAddr ));
    
    pAddBssParams->bssType = pMlmStartReq->bssType;
    if ((pMlmStartReq->bssType == eSIR_IBSS_MODE) || 
        (pMlmStartReq->bssType == eSIR_BTAMP_AP_MODE)|| 
        (pMlmStartReq->bssType == eSIR_BTAMP_STA_MODE)) {
        pAddBssParams->operMode                 = BSS_OPERATIONAL_MODE_STA;
    }
#ifdef WLAN_SOFTAP_FEATURE
    else if (pMlmStartReq->bssType == eSIR_INFRA_AP_MODE){
#else
    else{
#endif
        pAddBssParams->operMode                 = BSS_OPERATIONAL_MODE_AP;
    }

#ifdef WLAN_SOFTAP_FEATURE  
    if( wlan_cfgGetInt ( pMac, WNI_CFG_SHORT_SLOT_TIME, &val ) != eSIR_SUCCESS)
    {
        limLog ( pMac, LOGP, FL(" Error : unable to fetch the WNI_CFG_SHORT_SLOT_TIME\n"));
        palFreeMemory(pMac->hHdd,(void *)pAddBssParams);
        return eSIR_SME_HAL_SEND_MESSAGE_FAIL;
    }
    pAddBssParams->shortSlotTimeSupported = (tANI_U8)val;
#endif


    pAddBssParams->beaconInterval               = pMlmStartReq->beaconPeriod;
    pAddBssParams->dtimPeriod                   = pMlmStartReq->dtimPeriod;
    pAddBssParams->cfParamSet.cfpCount          = pMlmStartReq->cfParamSet.cfpCount;
    pAddBssParams->cfParamSet.cfpPeriod         = pMlmStartReq->cfParamSet.cfpPeriod;
    pAddBssParams->cfParamSet.cfpMaxDuration    = pMlmStartReq->cfParamSet.cfpMaxDuration;
    pAddBssParams->cfParamSet.cfpDurRemaining   = pMlmStartReq->cfParamSet.cfpDurRemaining;

    pAddBssParams->rateSet.numRates = pMlmStartReq->rateSet.numRates;
    palCopyMemory( pMac->hHdd,  pAddBssParams->rateSet.rate,
                   pMlmStartReq->rateSet.rate, pMlmStartReq->rateSet.numRates );

    pAddBssParams->nwType = pMlmStartReq->nwType;

    pAddBssParams->htCapable            = pMlmStartReq->htCapable;
    pAddBssParams->htOperMode           = pMlmStartReq->htOperMode;
    pAddBssParams->dualCTSProtection    = pMlmStartReq->dualCTSProtection;
    pAddBssParams->txChannelWidthSet    = pMlmStartReq->txChannelWidthSet;

    pAddBssParams->currentOperChannel   = pMlmStartReq->channelNumber;
    pAddBssParams->currentExtChannel    = mlm_get_ext_chnl(pMac, pMlmStartReq->cbMode);

    /* Update PE sessionId*/
    pAddBssParams->sessionId            = pMlmStartReq->sessionId; 

    //Send the SSID to HAL to enable SSID matching for IBSS
    palCopyMemory( pMac->hHdd, &(pAddBssParams->ssId.ssId),
        pMlmStartReq->ssId.ssId,
        pMlmStartReq->ssId.length);
    pAddBssParams->ssId.length = pMlmStartReq->ssId.length;
#ifdef WLAN_SOFTAP_FEATURE
    pAddBssParams->bHiddenSSIDEn = pMlmStartReq->ssidHidden;
    limLog( pMac, LOGE, FL( "TRYING TO HIDE SSID %d\n" ),pAddBssParams->bHiddenSSIDEn);
    pAddBssParams->bProxyProbeRespEn = 1;
    pAddBssParams->obssProtEnabled = pMlmStartReq->obssProtEnabled;

#endif
#if defined WLAN_FEATURE_VOWIFI  
    pAddBssParams->maxTxPower = psessionEntry->maxTxPower;
#endif
    mlm_add_sta(pMac, &pAddBssParams->staContext,
                pAddBssParams->bssId, pAddBssParams->htCapable,psessionEntry);

    pAddBssParams->status   = eHAL_STATUS_SUCCESS;
    pAddBssParams->respReqd = 1;

    // Set a new state for MLME
    psessionEntry->limMlmState = eLIM_MLM_WT_ADD_BSS_RSP_STATE;
    MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, 0, pMac->lim.gLimMlmState));

    pAddBssParams->halPersona=psessionEntry->pePersona; //pass on the session persona to hal

    pAddBssParams->bSpectrumMgtEnabled = psessionEntry->spectrumMgtEnabled;

#if defined WLAN_FEATURE_VOWIFI_11R
    pAddBssParams->extSetStaKeyParamValid = 0;
#endif

    //
    // FIXME_GEN4
    // A global counter (dialog token) is required to keep track of
    // all PE <-> HAL communication(s)
    //
    msgQ.type       = WDA_ADD_BSS_REQ;
    msgQ.reserved   = 0;
    msgQ.bodyptr    = pAddBssParams;
    msgQ.bodyval    = 0;
    MTRACE(macTraceMsgTx(pMac, 0, msgQ.type));

    limLog( pMac, LOGW, FL( "Sending WDA_ADD_BSS_REQ...\n" ));
    if( eSIR_SUCCESS != (retCode = wdaPostCtrlMsg( pMac, &msgQ )))
    {
        limLog( pMac, LOGE, FL("Posting ADD_BSS_REQ to HAL failed, reason=%X\n"), retCode );
        palFreeMemory(pMac->hHdd,(void *)pAddBssParams);
        return eSIR_SME_HAL_SEND_MESSAGE_FAIL;
    }

    return eSIR_SME_SUCCESS;
}


/**
 * limProcessMlmStartReq()
 *
 *FUNCTION:
 * This function is called to process MLM_START_REQ message
 * from SME
 *
 *LOGIC:
 * 1) MLME receives LIM_MLM_START_REQ from LIM
 * 2) MLME sends WDA_ADD_BSS_REQ to HAL
 * 3) MLME changes state to eLIM_MLM_WT_ADD_BSS_RSP_STATE
 * MLME now waits for HAL to send WDA_ADD_BSS_RSP
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pMsgBuf  A pointer to the MLM message buffer
 * @return None
 */

static void
limProcessMlmStartReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tLimMlmStartReq          *pMlmStartReq;
    tLimMlmStartCnf          mlmStartCnf;
    tpPESession              psessionEntry = NULL;
    
    if(pMsgBuf == NULL)
    {
        PELOGE(limLog(pMac, LOGE,FL("Buffer is Pointing to NULL\n"));)
        return;
    }
    
    pMlmStartReq = (tLimMlmStartReq *) pMsgBuf;
    if((psessionEntry = peFindSessionBySessionId(pMac,pMlmStartReq->sessionId))==NULL)
    {
        limLog(pMac, LOGP,FL("Session Does not exist for given sessionID\n"));
        mlmStartCnf.resultCode = eSIR_SME_REFUSED;
        goto end;
    }

    if (psessionEntry->limMlmState != eLIM_MLM_IDLE_STATE)
    {
        /**
         * Should not have received Start req in states other than idle.
         * Return Start confirm with failure code.
         */
        PELOGE(limLog(pMac, LOGE, FL("received unexpected MLM_START_REQ in state %X\n"),psessionEntry->limMlmState);)
        limPrintMlmState(pMac, LOGE, psessionEntry->limMlmState);
        mlmStartCnf.resultCode = eSIR_SME_BSS_ALREADY_STARTED_OR_JOINED;
        goto end;
    }
    
    #if 0
     if (cfgSetInt(pMac, WNI_CFG_CURRENT_CHANNEL, pMlmStartReq->channelNumber)!= eSIR_SUCCESS)
            limLog(pMac, LOGP, FL("could not set CURRENT_CHANNEL at CFG\n"));
     
        pMac->lim.gLimCurrentChannelId = pMlmStartReq->channelNumber;
    #endif //TO SUPPORT BT-AMP


    // Update BSSID & SSID at CFG database
    #if 0 //We are not using the BSSID and SSID from the config file, instead we are reading form the session table
     if (cfgSetStr(pMac, WNI_CFG_BSSID, (tANI_U8 *) pMlmStartReq->bssId, sizeof(tSirMacAddr))
        != eSIR_SUCCESS)
        limLog(pMac, LOGP, FL("could not update BSSID at CFG\n"));

   
    
    palCopyMemory( pMac->hHdd, pMac->lim.gLimCurrentBssId,
                   pMlmStartReq->bssId,
                   sizeof(tSirMacAddr));
    #endif //TO SUPPORT BT-AMP

    #if 0
    if (cfgSetStr(pMac, WNI_CFG_SSID, (tANI_U8 *) &pMlmStartReq->ssId.ssId, pMlmStartReq->ssId.length)
        != eSIR_SUCCESS)
        limLog(pMac, LOGP, FL("could not update SSID at CFG\n"));
    #endif //To SUPPORT BT-AMP
   
         
   // pMac->lim.gLimCurrentSSID.length = pMlmStartReq->ssId.length;

    #if 0
    
    /* WNI_CFG_PHY_MODE is to made sessio specific  ???*/
    if (cfgSetInt(pMac, WNI_CFG_PHY_MODE, psessionEntry->pLimStartBssReq->nwType)!= eSIR_SUCCESS)
        limLog(pMac, LOGP, FL("could not set networkType at CFG\n"));
    #endif

        #if 0
        if (cfgSetStr(pMac, WNI_CFG_OPERATIONAL_RATE_SET,
           (tANI_U8 *) &pMac->lim.gpLimStartBssReq->operationalRateSet.rate,
           pMac->lim.gpLimStartBssReq->operationalRateSet.numRates)
        != eSIR_SUCCESS)
        limLog(pMac, LOGP, FL("could not update Operational Rateset at CFG\n"));
        #endif //TO SUPPORT BT-AMP
        

#if defined(ANI_PRODUCT_TYPE_AP) && (WNI_POLARIS_FW_PACKAGE == ADVANCED)
    if (cfgSetInt(pMac, WNI_CFG_CURRENT_TX_POWER_LEVEL, pMac->lim.gpLimStartBssReq->powerLevel)
        != eSIR_SUCCESS)
        limLog(pMac, LOGP, FL("could not set WNI_CFG_CURRENT_TX_POWER_LEVEL at CFG\n"));
#endif

#ifdef WLAN_SOFTAP_FEATURE
#if 0 // Periodic timer for remove WPS PBC proble response entry in PE is disbaled now.
    if (psessionEntry->limSystemRole == eLIM_AP_ROLE)
    {
        if(pMac->lim.limTimers.gLimWPSOverlapTimerObj.isTimerCreated == eANI_BOOLEAN_FALSE)
        {
            if (tx_timer_create(&pMac->lim.limTimers.gLimWPSOverlapTimerObj.gLimWPSOverlapTimer,
                            "PS OVERLAP Timer",
                            limWPSOverlapTimerHandler,
                            SIR_LIM_WPS_OVERLAP_TIMEOUT, // expiration_input
                            SYS_MS_TO_TICKS(LIM_WPS_OVERLAP_TIMER_MS),  // initial_ticks
                            SYS_MS_TO_TICKS(LIM_WPS_OVERLAP_TIMER_MS),                         // reschedule_ticks
                            TX_AUTO_ACTIVATE /* TX_NO_ACTIVATE*/) != TX_SUCCESS)
            {
                limLog(pMac, LOGP, FL("failed to create WPS overlap Timer\n"));
            }
            
            pMac->lim.limTimers.gLimWPSOverlapTimerObj.sessionId = pMlmStartReq->sessionId;
            pMac->lim.limTimers.gLimWPSOverlapTimerObj.isTimerCreated = eANI_BOOLEAN_TRUE;
            limLog(pMac, LOGE, FL("Create WPS overlap Timer, session=%d\n"), pMlmStartReq->sessionId);

            if (tx_timer_activate(&pMac->lim.limTimers.gLimWPSOverlapTimerObj.gLimWPSOverlapTimer) != TX_SUCCESS)
            {
                limLog(pMac, LOGP, FL("tx_timer_activate failed\n"));
            }    
       }
    }
#endif
#endif


   
    mlmStartCnf.resultCode = limMlmAddBss(pMac, pMlmStartReq,psessionEntry);

end:
    /* Update PE session Id */
     mlmStartCnf.sessionId = pMlmStartReq->sessionId;
    
    /// Free up buffer allocated for LimMlmScanReq
    palFreeMemory( pMac->hHdd, (tANI_U8 *) pMsgBuf);

    //
    // Respond immediately to LIM, only if MLME has not been
    // successfully able to send WDA_ADD_BSS_REQ to HAL.
    // Else, LIM_MLM_START_CNF will be sent after receiving
    // WDA_ADD_BSS_RSP from HAL
    //
    if( eSIR_SME_SUCCESS != mlmStartCnf.resultCode )
      limPostSmeMessage(pMac, LIM_MLM_START_CNF, (tANI_U32 *) &mlmStartCnf);
} /*** limProcessMlmStartReq() ***/


/*
* This function checks if Scan is allowed or not.
* It checks each session and if any session is not in the normal state,
* it will return false.
* Note:  BTAMP_STA can be in LINK_EST as well as BSS_STARTED State, so
* both cases are handled below.
*/

static tANI_U8 __limMlmScanAllowed(tpAniSirGlobal pMac)
{
    int i;

    if(pMac->lim.gLimMlmState != eLIM_MLM_IDLE_STATE)
    {
        return FALSE;
    }
    for(i =0; i < pMac->lim.maxBssId; i++)
    {
        if(pMac->lim.gpSession[i].valid == TRUE)
        {
            if(!( ( (  (pMac->lim.gpSession[i].bssType == eSIR_INFRASTRUCTURE_MODE) ||
                       (pMac->lim.gpSession[i].limSystemRole == eLIM_BT_AMP_STA_ROLE))&&
                       (pMac->lim.gpSession[i].limMlmState == eLIM_MLM_LINK_ESTABLISHED_STATE) )||
                  
                  (    ( (pMac->lim.gpSession[i].bssType == eSIR_IBSS_MODE)||
                           (pMac->lim.gpSession[i].limSystemRole == eLIM_BT_AMP_AP_ROLE)||
                           (pMac->lim.gpSession[i].limSystemRole == eLIM_BT_AMP_STA_ROLE) )&&
                       (pMac->lim.gpSession[i].limMlmState == eLIM_MLM_BSS_STARTED_STATE) )
#ifdef WLAN_FEATURE_P2P
               ||  ( ( ( (pMac->lim.gpSession[i].bssType == eSIR_INFRA_AP_MODE) 
                      && ( pMac->lim.gpSession[i].pePersona == VOS_P2P_GO_MODE) )
                    || (pMac->lim.gpSession[i].limSystemRole == eLIM_AP_ROLE) )
                  && (pMac->lim.gpSession[i].limMlmState == eLIM_MLM_BSS_STARTED_STATE) )
#endif
                ))
            {
                return FALSE;

            }
        }
    }

    return TRUE;
}



/**
 * limProcessMlmScanReq()
 *
 *FUNCTION:
 * This function is called to process MLM_SCAN_REQ message
 * from SME
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pMsgBuf  A pointer to the MLM message buffer
 * @return None
 */

static void
limProcessMlmScanReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tLimMlmScanCnf       mlmScanCnf;

    if (pMac->lim.gLimSystemInScanLearnMode)
    {
        PELOGE(limLog(pMac, LOGE,
               FL("Sending START_SCAN from LIM while one req is pending\n"));)
        return;
    }

 if(__limMlmScanAllowed(pMac) && 
    (((tLimMlmScanReq *) pMsgBuf)->channelList.numChannels != 0))
        
    {
        /// Hold onto SCAN REQ criteria
        pMac->lim.gpLimMlmScanReq = (tLimMlmScanReq *) pMsgBuf;

       PELOG3(limLog(pMac, LOG3, FL("Number of channels to scan are %d \n"),
               pMac->lim.gpLimMlmScanReq->channelList.numChannels);)

        pMac->lim.gLimPrevMlmState = pMac->lim.gLimMlmState;

        if (pMac->lim.gpLimMlmScanReq->scanType == eSIR_ACTIVE_SCAN)
            pMac->lim.gLimMlmState = eLIM_MLM_WT_PROBE_RESP_STATE;
        else // eSIR_PASSIVE_SCAN
            pMac->lim.gLimMlmState = eLIM_MLM_PASSIVE_SCAN_STATE;
        MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, 0, pMac->lim.gLimMlmState));

        pMac->lim.gLimSystemInScanLearnMode = 1;

#ifdef WLAN_FEATURE_P2P
        pMac->lim.gTotalScanDuration = 
                    pMac->lim.gpLimMlmScanReq->maxChannelTime*
                    pMac->lim.gpLimMlmScanReq->channelList.numChannels;
#endif
        limSetScanMode(pMac);
    }
    else
    {
        /**
         * Should not have received SCAN req in other states
         * OR should not have received LIM_MLM_SCAN_REQ with
         * zero number of channels
         * Log error
         */
        limLog(pMac, LOGW,
               FL("received unexpected MLM_SCAN_REQ in state %X OR zero number of channels: %X\n"),
               pMac->lim.gLimMlmState, ((tLimMlmScanReq *) pMsgBuf)->channelList.numChannels);
        limPrintMlmState(pMac, LOGW, pMac->lim.gLimMlmState);

        /// Free up buffer allocated for
        /// pMac->lim.gLimMlmScanReq
        palFreeMemory( pMac->hHdd, (tANI_U8 *) pMsgBuf);

        /// Return Scan confirm with INVALID_PARAMETERS

        mlmScanCnf.resultCode = eSIR_SME_INVALID_PARAMETERS;
        mlmScanCnf.scanResultLength = 0;
        limPostSmeMessage(pMac,
                         LIM_MLM_SCAN_CNF,
                         (tANI_U32 *) &mlmScanCnf);
    }
} /*** limProcessMlmScanReq() ***/







/**
 * limProcessMlmJoinReq()
 *
 *FUNCTION:
 * This function is called to process MLM_JOIN_REQ message
 * from SME
 *
 *LOGIC:
 * 1) Initialize LIM, HAL, DPH
 * 2) Configure the BSS for which the JOIN REQ was received
 *   a) Send WDA_ADD_BSS_REQ to HAL -
 *   This will identify the BSS that we are interested in
 *   --AND--
 *   Add a STA entry for the AP (in a STA context)
 *   b) Wait for WDA_ADD_BSS_RSP
 *   c) Send WDA_ADD_STA_REQ to HAL
 *   This will add the "local STA" entry to the STA table
 * 3) Continue as before, i.e,
 *   a) Send a PROBE REQ
 *   b) Wait for PROBE RSP/BEACON containing the SSID that
 *   we are interested in
 *   c) Then start an AUTH seq
 *   d) Followed by the ASSOC seq
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pMsgBuf  A pointer to the MLM message buffer
 * @return None
 */

static void
limProcessMlmJoinReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tANI_U8             chanNum;
    tLimMlmJoinCnf      mlmJoinCnf;
    tANI_U8             sessionId;
    tpPESession         psessionEntry;
    tSirLinkState       linkState;

    sessionId = ((tpLimMlmJoinReq)pMsgBuf)->sessionId;

    if((psessionEntry = peFindSessionBySessionId(pMac,sessionId))== NULL)
    {
        limLog(pMac, LOGP, FL("session does not exist for given sessionId\n"));

        goto error;
    }

    if (( (psessionEntry->limSystemRole != eLIM_AP_ROLE ) && (psessionEntry->limSystemRole != eLIM_BT_AMP_AP_ROLE )) &&
          ( (psessionEntry->limMlmState == eLIM_MLM_IDLE_STATE) ||
            (psessionEntry->limMlmState == eLIM_MLM_JOINED_STATE))  &&
        (SIR_MAC_GET_ESS( ((tpLimMlmJoinReq) pMsgBuf)->bssDescription.capabilityInfo) !=
             SIR_MAC_GET_IBSS( ((tpLimMlmJoinReq) pMsgBuf)->bssDescription.capabilityInfo)))
    {
        #if 0
        if (pMac->lim.gpLimMlmJoinReq)
            palFreeMemory( pMac->hHdd, pMac->lim.gpLimMlmJoinReq);
        #endif //TO SUPPORT BT-AMP , review 23sep

        /// Hold onto Join request parameters
        
        psessionEntry->pLimMlmJoinReq =(tpLimMlmJoinReq) pMsgBuf;
        
        psessionEntry->limPrevMlmState = psessionEntry->limMlmState;
                
        psessionEntry->limMlmState = eLIM_MLM_WT_JOIN_BEACON_STATE;
        MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, 0, pMac->lim.gLimMlmState));

        limDeactivateAndChangeTimer(pMac, eLIM_JOIN_FAIL_TIMER);

        //assign appropriate sessionId to the timer object
        pMac->lim.limTimers.gLimJoinFailureTimer.sessionId = sessionId;

        linkState = ((psessionEntry->limSystemRole == eLIM_BT_AMP_STA_ROLE) ? eSIR_LINK_BTAMP_PREASSOC_STATE : eSIR_LINK_PREASSOC_STATE);
        limLog(pMac, LOG1, FL("[limProcessMlmJoinReq]: linkState:%d\n"),linkState);

        if (limSetLinkState(pMac, linkState, 
             psessionEntry->pLimMlmJoinReq->bssDescription.bssId, 
             psessionEntry->selfMacAddr, NULL, NULL) != eSIR_SUCCESS )
        {
            limLog(pMac, LOGE, FL("limSetLinkState to eSIR_LINK_PREASSOC_STATE Failed!!\n"));
            mlmJoinCnf.resultCode = eSIR_SME_RESOURCES_UNAVAILABLE;
            pMac->lim.gLimMlmState = eLIM_MLM_IDLE_STATE;
            goto error;
        }

        /** Derive channel from BSS description and store it in the CFG */
        // chanNum = pMac->lim.gpLimMlmJoinReq->bssDescription.channelId;
       
        chanNum = psessionEntry->currentOperChannel;
        //store the channel switch sessionEntry in the lim global var
        psessionEntry->channelChangeReasonCode = LIM_SWITCH_CHANNEL_JOIN;

        limSetChannel(pMac, psessionEntry->pLimMlmJoinReq->bssDescription.titanHtCaps,
                            chanNum, psessionEntry->maxTxPower, psessionEntry->peSessionId); 
        return;
    }
    else
    {
        /**
              * Should not have received JOIN req in states other than
              * Idle state or on AP.
              * Return join confirm with invalid parameters code.
              */
        PELOGE(limLog(pMac, LOGE,
               FL("Unexpected Join request for role %d state %X\n"),
               psessionEntry->limSystemRole,
               psessionEntry->limMlmState);)
        limPrintMlmState(pMac, LOGE, psessionEntry->limMlmState);
        
        limLog(pMac, LOGE, FL("Unexpected Join request for role %d state %X\n"),
               psessionEntry->limSystemRole, pMac->lim.gLimMlmState);
    }

error: 

        
        mlmJoinCnf.resultCode = eSIR_SME_RESOURCES_UNAVAILABLE;
        mlmJoinCnf.sessionId = sessionId;
        mlmJoinCnf.protStatusCode = eSIR_MAC_UNSPEC_FAILURE_STATUS;
        limPostSmeMessage(pMac, LIM_MLM_JOIN_CNF, (tANI_U32 *) &mlmJoinCnf);


} /*** limProcessMlmJoinReq() ***/



/**
 * limProcessMlmAuthReq()
 *
 *FUNCTION:
 * This function is called to process MLM_AUTH_REQ message
 * from SME
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pMsgBuf  A pointer to the MLM message buffer
 * @return None
 */

static void
limProcessMlmAuthReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tANI_U32                numPreAuthContexts;
    tSirMacAddr             currentBssId;
    tSirMacAuthFrameBody    authFrameBody;
    tLimMlmAuthCnf          mlmAuthCnf;
    struct tLimPreAuthNode  *preAuthNode;
    tpDphHashNode           pStaDs;
    tANI_U8                 sessionId;
    tpPESession             psessionEntry;

    if(pMsgBuf == NULL)
    {
        PELOGE(limLog(pMac, LOGE,FL("Buffer is Pointing to NULL\n"));)
        return;
    }

    pMac->lim.gpLimMlmAuthReq = (tLimMlmAuthReq *) pMsgBuf;
    sessionId = pMac->lim.gpLimMlmAuthReq->sessionId;
    if((psessionEntry= peFindSessionBySessionId(pMac,sessionId) )== NULL)
    {
        limLog(pMac, LOGP, FL("Session Does not exist for given sessionId\n"));
        return;
    }

    /**
     * Expect Auth request only when:
     * 1. STA joined/associated with a BSS or
     * 2. STA is in IBSS mode
     * and STA is going to authenticate with a unicast
     * adress and requested authentication algorithm is
     * supported.
     */
     #if 0
    if (wlan_cfgGetStr(pMac, WNI_CFG_BSSID, currentBssId, &cfg) !=
                                eSIR_SUCCESS)
    {
        /// Could not get BSSID from CFG. Log error.
        limLog(pMac, LOGP, FL("could not retrieve BSSID\n"));
    }
    #endif //To SuppoRT BT-AMP

    sirCopyMacAddr(currentBssId,psessionEntry->bssId);

    if (((((psessionEntry->limSystemRole== eLIM_STA_ROLE) || (psessionEntry->limSystemRole == eLIM_BT_AMP_STA_ROLE)) &&
          ((psessionEntry->limMlmState == eLIM_MLM_JOINED_STATE) ||
           (psessionEntry->limMlmState ==
                                  eLIM_MLM_LINK_ESTABLISHED_STATE))) ||
         ((psessionEntry->limSystemRole == eLIM_STA_IN_IBSS_ROLE) &&
          (psessionEntry->limMlmState == eLIM_MLM_BSS_STARTED_STATE))) &&
        (limIsGroupAddr(pMac->lim.gpLimMlmAuthReq->peerMacAddr)
                                                   == false) &&
#ifdef WLAN_SOFTAP_FEATURE
        (limIsAuthAlgoSupported(
                        pMac,
                        pMac->lim.gpLimMlmAuthReq->authType,
                        psessionEntry) == true)
#else
        (limIsAuthAlgoSupported(
                        pMac,
                        pMac->lim.gpLimMlmAuthReq->authType) == true)
#endif
        )        
    {
        /**
         * This is a request for pre-authentication.
         * Check if there exists context already for
         * the requsted peer OR
         * if this request is for the AP we're currently
         * associated with.
         * If yes, return auth confirm immediately when
         * requested auth type is same as the one used before.
         */
        if ((((psessionEntry->limSystemRole == eLIM_STA_ROLE) ||(psessionEntry->limSystemRole == eLIM_BT_AMP_STA_ROLE) )&&
             (psessionEntry->limMlmState ==
                                  eLIM_MLM_LINK_ESTABLISHED_STATE) &&
             (((pStaDs = dphGetHashEntry(pMac, DPH_STA_HASH_INDEX_PEER, &psessionEntry->dph.dphHashTable)) != NULL) &&
              (pMac->lim.gpLimMlmAuthReq->authType ==
                                   pStaDs->mlmStaContext.authType)) &&
             (palEqualMemory( pMac->hHdd,pMac->lim.gpLimMlmAuthReq->peerMacAddr,
                        currentBssId,
                        sizeof(tSirMacAddr)) )) ||
            (((preAuthNode =
               limSearchPreAuthList(
                     pMac,
                     pMac->lim.gpLimMlmAuthReq->peerMacAddr)) != NULL) &&
             (preAuthNode->authType ==
                                   pMac->lim.gpLimMlmAuthReq->authType)))
        {
           PELOG2(limLog(pMac, LOG2,
                   FL("Already have pre-auth context with peer\n"));
            limPrintMacAddr(pMac, pMac->lim.gpLimMlmAuthReq->peerMacAddr,
                            LOG2);)

            mlmAuthCnf.resultCode = (tSirResultCodes)
                                    eSIR_MAC_SUCCESS_STATUS;
            

            goto end;
        }
        else
        {
            if (wlan_cfgGetInt(pMac, WNI_CFG_MAX_NUM_PRE_AUTH,
                          (tANI_U32 *) &numPreAuthContexts) != eSIR_SUCCESS)
            {
                limLog(pMac, LOGP,
                   FL("Could not retrieve NumPreAuthLimit from CFG\n"));
            }
#ifdef ANI_AP_SDK_OPT
            if(numPreAuthContexts > SIR_SDK_OPT_MAX_NUM_PRE_AUTH)
                numPreAuthContexts = SIR_SDK_OPT_MAX_NUM_PRE_AUTH;
#endif // ANI_AP_SDK_OPT

            if (pMac->lim.gLimNumPreAuthContexts == numPreAuthContexts)
            {
                PELOGW(limLog(pMac, LOGW,
                       FL("Number of pre-auth reached max limit\n"));)

                /// Return Auth confirm with reject code
                mlmAuthCnf.resultCode =
                               eSIR_SME_MAX_NUM_OF_PRE_AUTH_REACHED;

                goto end;
            }
        }

        // Delete pre-auth node if exists
        if (preAuthNode)
            limDeletePreAuthNode(pMac,
                                 pMac->lim.gpLimMlmAuthReq->peerMacAddr);

        psessionEntry->limPrevMlmState = psessionEntry->limMlmState;
        psessionEntry->limMlmState = eLIM_MLM_WT_AUTH_FRAME2_STATE;
        MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, 0, pMac->lim.gLimMlmState));

        /// Prepare & send Authentication frame
        authFrameBody.authAlgoNumber =
                                  (tANI_U8) pMac->lim.gpLimMlmAuthReq->authType;
        authFrameBody.authTransactionSeqNumber = SIR_MAC_AUTH_FRAME_1;
        authFrameBody.authStatusCode = 0;
        limSendAuthMgmtFrame(pMac,
                             &authFrameBody,
                             pMac->lim.gpLimMlmAuthReq->peerMacAddr,
                             LIM_NO_WEP_IN_FC,psessionEntry);

        //assign appropriate sessionId to the timer object
        pMac->lim.limTimers.gLimAuthFailureTimer.sessionId = sessionId;
 
        // Activate Auth failure timer
        MTRACE(macTrace(pMac, TRACE_CODE_TIMER_ACTIVATE, 0, eLIM_AUTH_FAIL_TIMER));
        if (tx_timer_activate(&pMac->lim.limTimers.gLimAuthFailureTimer)
                                       != TX_SUCCESS)
        {
            /// Could not start Auth failure timer.
            // Log error
            limLog(pMac, LOGP,
                   FL("could not start Auth failure timer\n"));
            // Cleanup as if auth timer expired
            limProcessAuthFailureTimeout(pMac);
        }

        return;
    }
    else
    {
        /**
         * Unexpected auth request.
         * Return Auth confirm with Invalid parameters code.
         */
        mlmAuthCnf.resultCode = eSIR_SME_INVALID_PARAMETERS;

        goto end;
    }

end:
    palCopyMemory( pMac->hHdd, (tANI_U8 *) &mlmAuthCnf.peerMacAddr,
                  (tANI_U8 *) &pMac->lim.gpLimMlmAuthReq->peerMacAddr,
                  sizeof(tSirMacAddr));

    mlmAuthCnf.authType = pMac->lim.gpLimMlmAuthReq->authType;
    mlmAuthCnf.sessionId = sessionId;

    /// Free up buffer allocated
    /// for pMac->lim.gLimMlmAuthReq
    palFreeMemory( pMac->hHdd, pMac->lim.gpLimMlmAuthReq);
    pMac->lim.gpLimMlmAuthReq = NULL;
    limPostSmeMessage(pMac, LIM_MLM_AUTH_CNF, (tANI_U32 *) &mlmAuthCnf);
} /*** limProcessMlmAuthReq() ***/



/**
 * limProcessMlmAssocReq()
 *
 *FUNCTION:
 * This function is called to process MLM_ASSOC_REQ message
 * from SME
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pMsgBuf  A pointer to the MLM message buffer
 * @return None
 */

static void
limProcessMlmAssocReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tSirMacAddr              currentBssId;
    tLimMlmAssocReq          *pMlmAssocReq;
    tLimMlmAssocCnf          mlmAssocCnf;
    tpPESession               psessionEntry;
   // tANI_U8                  sessionId;

    if(pMsgBuf == NULL)
    {
        PELOGE(limLog(pMac, LOGE,FL("Buffer is Pointing to NULL\n"));)
        return;
    }
    pMlmAssocReq = (tLimMlmAssocReq *) pMsgBuf;

    if( (psessionEntry = peFindSessionBySessionId(pMac,pMlmAssocReq->sessionId) )== NULL) 
    {
        limLog(pMac, LOGP,FL("Session Does not exist for given sessionID\n"));
        return;
    }

    #if 0
    if (wlan_cfgGetStr(pMac, WNI_CFG_BSSID, currentBssId, &cfg) !=
                                eSIR_SUCCESS)
    {
        /// Could not get BSSID from CFG. Log error.
        limLog(pMac, LOGP, FL("could not retrieve BSSID\n"));
    }
    #endif //TO SUPPORT BT-AMP
    sirCopyMacAddr(currentBssId,psessionEntry->bssId);
    
    if ( (psessionEntry->limSystemRole != eLIM_AP_ROLE && psessionEntry->limSystemRole != eLIM_BT_AMP_AP_ROLE) &&
         (psessionEntry->limMlmState == eLIM_MLM_AUTHENTICATED_STATE || psessionEntry->limMlmState == eLIM_MLM_JOINED_STATE) &&
         (palEqualMemory(pMac->hHdd,pMlmAssocReq->peerMacAddr, currentBssId, sizeof(tSirMacAddr))) )
    {


        psessionEntry->limPrevMlmState = psessionEntry->limMlmState;
        psessionEntry->limMlmState = eLIM_MLM_WT_ASSOC_RSP_STATE;
        MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, 0, pMac->lim.gLimMlmState));
 
        /// Prepare and send Association request frame
        limSendAssocReqMgmtFrame(pMac, pMlmAssocReq,psessionEntry);

  //Set the link state to postAssoc, so HW can start receiving frames from AP.
    if ((psessionEntry->bssType == eSIR_BTAMP_STA_MODE)||
        ((psessionEntry->bssType == eSIR_BTAMP_AP_MODE) && (psessionEntry->limSystemRole == eLIM_BT_AMP_STA_ROLE)))
    {
       if(limSetLinkState(pMac, eSIR_LINK_BTAMP_POSTASSOC_STATE, currentBssId, 
           psessionEntry->selfMacAddr, NULL, NULL) != eSIR_SUCCESS)
            PELOGE(limLog(pMac, LOGE,  FL("Failed to set the LinkState\n"));)
    } else {
       if(limSetLinkState(pMac, eSIR_LINK_POSTASSOC_STATE, currentBssId, 
           psessionEntry->selfMacAddr, NULL, NULL) != eSIR_SUCCESS)
            PELOGE(limLog(pMac, LOGE,  FL("Failed to set the LinkState\n"));)
    }
        /// map the session entry pointer to the AssocFailureTimer 
        pMac->lim.limTimers.gLimAssocFailureTimer.sessionId = pMlmAssocReq->sessionId;

        /// Start association failure timer
        MTRACE(macTrace(pMac, TRACE_CODE_TIMER_ACTIVATE, 0, eLIM_ASSOC_FAIL_TIMER));
        if (tx_timer_activate(&pMac->lim.limTimers.gLimAssocFailureTimer)
                                              != TX_SUCCESS)
        {
            /// Could not start Assoc failure timer.
            // Log error
            limLog(pMac, LOGP,
                   FL("could not start Association failure timer\n"));
            // Cleanup as if assoc timer expired
            limProcessAssocFailureTimeout(pMac,LIM_ASSOC );
           
        }

        return;
    }
    else
    {
        /**
         * Received Association request either in invalid state
         * or to a peer MAC entity whose address is different
         * from one that STA is currently joined with or on AP.
         * Return Assoc confirm with Invalid parameters code.
         */

        // Log error
        PELOGW(limLog(pMac, LOGW,
           FL("received unexpected MLM_ASSOC_CNF in state %X for role=%d, MAC addr= "),
           psessionEntry->limMlmState,
           psessionEntry->limSystemRole);)
        limPrintMacAddr(pMac, pMlmAssocReq->peerMacAddr, LOGW);
        limPrintMlmState(pMac, LOGW, psessionEntry->limMlmState);

        mlmAssocCnf.resultCode = eSIR_SME_INVALID_PARAMETERS;
        mlmAssocCnf.protStatusCode = eSIR_MAC_UNSPEC_FAILURE_STATUS;

        goto end;
    }

end:
    /* Update PE session Id*/
    mlmAssocCnf.sessionId = pMlmAssocReq->sessionId;

    /// Free up buffer allocated for assocReq
    palFreeMemory( pMac->hHdd, (tANI_U8 *) pMlmAssocReq);

    limPostSmeMessage(pMac, LIM_MLM_ASSOC_CNF, (tANI_U32 *) &mlmAssocCnf);
} /*** limProcessMlmAssocReq() ***/



/**
 * limProcessMlmReassocReq()
 *
 *FUNCTION:
 * This function is called to process MLM_REASSOC_REQ message
 * from SME
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pMsgBuf  A pointer to the MLM message buffer
 * @return None
 */

static void
limProcessMlmReassocReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tANI_U8                       chanNum;
    struct tLimPreAuthNode  *pAuthNode;
    tLimMlmReassocReq       *pMlmReassocReq;
    tLimMlmReassocCnf       mlmReassocCnf;
    tpPESession             psessionEntry;

    if(pMsgBuf == NULL)
    {
        PELOGE(limLog(pMac, LOGE,FL("Buffer is Pointing to NULL\n"));)
        return;
    }

    pMlmReassocReq = (tLimMlmReassocReq *) pMsgBuf;
    
    if((psessionEntry = peFindSessionBySessionId(pMac,pMlmReassocReq->sessionId)) == NULL)
    {
        PELOGE(limLog(pMac, LOGE,FL("Session Does not exist for given sessionId\n"));)
        return;
    }
    
    if (((psessionEntry->limSystemRole != eLIM_AP_ROLE) && (psessionEntry->limSystemRole != eLIM_BT_AMP_AP_ROLE)) &&
         (psessionEntry->limMlmState == eLIM_MLM_LINK_ESTABLISHED_STATE))
    {
        if (psessionEntry->pLimMlmReassocReq)
            palFreeMemory( pMac->hHdd, psessionEntry->pLimMlmReassocReq);

        /* Hold Re-Assoc request as part of Session, knock-out pMac */
        /// Hold onto Reassoc request parameters
        psessionEntry->pLimMlmReassocReq = pMlmReassocReq;
        
        // See if we have pre-auth context with new AP
        pAuthNode = limSearchPreAuthList(pMac, psessionEntry->limReAssocbssId);

        if (!pAuthNode &&
            (!palEqualMemory( pMac->hHdd,pMlmReassocReq->peerMacAddr,
                       psessionEntry->bssId,
                       sizeof(tSirMacAddr)) ))
        {
            // Either pre-auth context does not exist AND
            // we are not reassociating with currently
            // associated AP.
            // Return Reassoc confirm with not authenticated
            mlmReassocCnf.resultCode = eSIR_SME_STA_NOT_AUTHENTICATED;
            mlmReassocCnf.protStatusCode = eSIR_MAC_UNSPEC_FAILURE_STATUS;

            goto end;
        }

        //assign the sessionId to the timer object
        pMac->lim.limTimers.gLimReassocFailureTimer.sessionId = pMlmReassocReq->sessionId;

        psessionEntry->limPrevMlmState = psessionEntry->limMlmState;
        psessionEntry->limMlmState    = eLIM_MLM_WT_REASSOC_RSP_STATE;
        MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, 0, pMac->lim.gLimMlmState));

#if 0
        // Update BSSID at CFG database
        if (wlan_cfgSetStr(pMac, WNI_CFG_BSSID,
                      pMac->lim.gLimReassocBssId,
                      sizeof(tSirMacAddr)) != eSIR_SUCCESS)
        {
            /// Could not update BSSID at CFG. Log error.
            limLog(pMac, LOGP, FL("could not update BSSID at CFG\n"));
        }
#endif //TO SUPPORT BT-AMP

    /* Copy Global Reassoc ID*/
   // sirCopyMacAddr(psessionEntry->reassocbssId,pMac->lim.gLimReAssocBssId);

        /**
         * Derive channel from BSS description and
         * store it at CFG.
         */

        chanNum = psessionEntry->limReassocChannelId;


        /* To Support BT-AMP .. read channel number from psessionEntry*/
        //chanNum = psessionEntry->currentOperChannel;

        // Apply previously set configuration at HW
        limApplyConfiguration(pMac,psessionEntry);

        //store the channel switch sessionEntry in the lim global var
        /* We have already saved the ReAssocreq Pointer abobe */
        //psessionEntry->pLimReAssocReq = (void *)pMlmReassocReq;
        psessionEntry->channelChangeReasonCode = LIM_SWITCH_CHANNEL_REASSOC;

        /** Switch channell to the new Operating channel for Reassoc*/
        limSetChannel(pMac, psessionEntry->limReassocTitanHtCaps, chanNum, psessionEntry->maxTxPower, psessionEntry->peSessionId);

        return;
    }
    else
    {
        /**
         * Received Reassoc request in invalid state or
         * in AP role.Return Reassoc confirm with Invalid
         * parameters code.
         */

        // Log error
        PELOGW(limLog(pMac, LOGW,
           FL("received unexpected MLM_REASSOC_CNF in state %X for role=%d, MAC addr= "),
           psessionEntry->limMlmState,
           psessionEntry->limSystemRole);)
        limPrintMacAddr(pMac, pMlmReassocReq->peerMacAddr,
                        LOGW);
        limPrintMlmState(pMac, LOGW, psessionEntry->limMlmState);

        mlmReassocCnf.resultCode = eSIR_SME_INVALID_PARAMETERS;
        mlmReassocCnf.protStatusCode = eSIR_MAC_UNSPEC_FAILURE_STATUS;

        goto end;
    }

end:
    mlmReassocCnf.protStatusCode = eSIR_MAC_UNSPEC_FAILURE_STATUS;
    /* Update PE sessio Id*/
    mlmReassocCnf.sessionId = pMlmReassocReq->sessionId;
    /// Free up buffer allocated for reassocReq
    palFreeMemory( pMac->hHdd, (tANI_U8 *) pMlmReassocReq);
    psessionEntry->pLimReAssocReq = NULL;

    limPostSmeMessage(pMac, LIM_MLM_REASSOC_CNF, (tANI_U32 *) &mlmReassocCnf);
} /*** limProcessMlmReassocReq() ***/



/**
 * limProcessMlmDisassocReq()
 *
 *FUNCTION:
 * This function is called to process MLM_DISASSOC_REQ message
 * from SME
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pMsgBuf  A pointer to the MLM message buffer
 * @return None
 */

static void
limProcessMlmDisassocReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tANI_U16                 aid;
    tSirMacAddr              currentBssId;
    tpDphHashNode            pStaDs;
    tLimMlmDisassocReq       *pMlmDisassocReq;
    tLimMlmDisassocCnf       mlmDisassocCnf;
    tpPESession              psessionEntry;
    extern tANI_BOOLEAN     sendDisassocFrame;

    if(pMsgBuf == NULL)
    {
        PELOGE(limLog(pMac, LOGE,FL("Buffer is Pointing to NULL\n"));)
        return;
    }

    pMlmDisassocReq = (tLimMlmDisassocReq *) pMsgBuf;

    if((psessionEntry = peFindSessionBySessionId(pMac,pMlmDisassocReq->sessionId))== NULL)
    {
    
        PELOGE(limLog(pMac, LOGE,
                  FL("session does not exist for given sessionId\n"));)
        return;
    }

    #if 0
    if (wlan_cfgGetStr(pMac, WNI_CFG_BSSID, currentBssId, &cfg) !=
                                eSIR_SUCCESS)
    {
        /// Could not get BSSID from CFG. Log error.
        limLog(pMac, LOGP, FL("could not retrieve BSSID\n"));
    }
    #endif //BT-AMP Support
    sirCopyMacAddr(currentBssId,psessionEntry->bssId);

    switch (psessionEntry->limSystemRole)
    {
        case eLIM_STA_ROLE:
        case eLIM_BT_AMP_STA_ROLE:    
            if ( !palEqualMemory( pMac->hHdd,pMlmDisassocReq->peerMacAddr,
                          currentBssId,
                          sizeof(tSirMacAddr)) )
            {
                PELOGW(limLog(pMac, LOGW,
                   FL("received MLM_DISASSOC_REQ with invalid BSS id "));)
                limPrintMacAddr(pMac, pMlmDisassocReq->peerMacAddr, LOGW);

                /// Prepare and Send LIM_MLM_DISASSOC_CNF

                mlmDisassocCnf.resultCode      =
                                       eSIR_SME_INVALID_PARAMETERS;

                goto end;
            }

            break;

        case eLIM_STA_IN_IBSS_ROLE:

            break;

        default: // eLIM_AP_ROLE

            // Fall through
            break;

    } // end switch (psessionEntry->limSystemRole)

    /**
     * Check if there exists a context for the peer entity
     * to be disassociated with.
     */
    pStaDs = dphLookupHashEntry(pMac, pMlmDisassocReq->peerMacAddr, &aid, &psessionEntry->dph.dphHashTable);
    if ((pStaDs == NULL) ||
        (pStaDs &&
         ((pStaDs->mlmStaContext.mlmState !=
                             eLIM_MLM_LINK_ESTABLISHED_STATE) &&
          (pStaDs->mlmStaContext.mlmState !=
                             eLIM_MLM_WT_ASSOC_CNF_STATE) &&
          (pStaDs->mlmStaContext.mlmState !=
                             eLIM_MLM_ASSOCIATED_STATE))))
    {
        /**
         * Received LIM_MLM_DISASSOC_REQ for STA that does not
         * have context or in some transit state.
         * Log error
         */
        PELOGW(limLog(pMac, LOGW,
           FL("received MLM_DISASSOC_REQ for STA that either has no context or in some transit state, Addr= "));)
           limPrintMacAddr(pMac, pMlmDisassocReq->peerMacAddr, LOGW);

        /// Prepare and Send LIM_MLM_DISASSOC_CNF

        mlmDisassocCnf.resultCode = eSIR_SME_INVALID_PARAMETERS;

        goto end;
    }

    //pStaDs->mlmStaContext.rxPurgeReq = 1;
    pStaDs->mlmStaContext.disassocReason = (tSirMacReasonCodes)
                                           pMlmDisassocReq->reasonCode;
    pStaDs->mlmStaContext.cleanupTrigger = pMlmDisassocReq->disassocTrigger;

    /// Send Disassociate frame to peer entity
    if (sendDisassocFrame)
    {
    limSendDisassocMgmtFrame(pMac,
                             pMlmDisassocReq->reasonCode,
                             pMlmDisassocReq->peerMacAddr,psessionEntry);
    }
    else
    {
       sendDisassocFrame = 1;    
    }

    /* We received disassoc request and about to send disassoc frame
     * to AP. PE shall reset the EDCA parameters to default parameters 
     * as advertised by AP and send the update to HAL;
     */
    if  ( (psessionEntry->limSystemRole == eLIM_STA_ROLE )|| (psessionEntry->limSystemRole == eLIM_BT_AMP_STA_ROLE ) )
    {
        schSetDefaultEdcaParams(pMac);
        if (pStaDs != NULL)
        {
            if (pStaDs->aniPeer == eANI_BOOLEAN_TRUE) 
                limSendEdcaParams(pMac, psessionEntry->gLimEdcaParamsActive, pStaDs->bssId, eANI_BOOLEAN_TRUE);
            else
                limSendEdcaParams(pMac, psessionEntry->gLimEdcaParamsActive, pStaDs->bssId, eANI_BOOLEAN_FALSE);
        }
        else
        {
            PELOGE(limLog(pMac, LOGE, FL("Self entry missing in Hash Table \n"));)
        }
    }

    /// Receive path cleanup with dummy packet
    if(eSIR_SUCCESS != limCleanupRxPath(pMac, pStaDs,psessionEntry))
        {
            mlmDisassocCnf.resultCode = eSIR_SME_RESOURCES_UNAVAILABLE;
            goto end;
        }

    /// Free up buffer allocated for mlmDisassocReq
    palFreeMemory( pMac->hHdd, (tANI_U8 *) pMlmDisassocReq);

    return;

end:
    palCopyMemory( pMac->hHdd, (tANI_U8 *) &mlmDisassocCnf.peerMacAddr,
                  (tANI_U8 *) pMlmDisassocReq->peerMacAddr,
                  sizeof(tSirMacAddr));
    mlmDisassocCnf.aid = pMlmDisassocReq->aid;
    mlmDisassocCnf.disassocTrigger = pMlmDisassocReq->disassocTrigger;
    
    /* Update PE session ID*/
    mlmDisassocCnf.sessionId = pMlmDisassocReq->sessionId;

    /// Free up buffer allocated for mlmDisassocReq
    palFreeMemory( pMac->hHdd, (tANI_U8 *) pMlmDisassocReq);

    limPostSmeMessage(pMac,
                      LIM_MLM_DISASSOC_CNF,
                      (tANI_U32 *) &mlmDisassocCnf);
} /*** limProcessMlmDisassocReq() ***/


/**
 * limProcessMlmDeauthReq()
 *
 *FUNCTION:
 * This function is called to process MLM_DEAUTH_REQ message
 * from SME
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pMsgBuf  A pointer to the MLM message buffer
 * @return None
 */

static void
limProcessMlmDeauthReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tANI_U16                aid;
    tSirMacAddr             currentBssId;
    tpDphHashNode           pStaDs;
    struct tLimPreAuthNode  *pAuthNode;
    tLimMlmDeauthReq        *pMlmDeauthReq;
    tLimMlmDeauthCnf        mlmDeauthCnf;
    tpPESession             psessionEntry;

    if(pMsgBuf == NULL)
    {
        PELOGE(limLog(pMac, LOGE,FL("Buffer is Pointing to NULL\n"));)
        return;
    }

    pMlmDeauthReq = (tLimMlmDeauthReq *) pMsgBuf;

    if((psessionEntry = peFindSessionBySessionId(pMac,pMlmDeauthReq->sessionId))== NULL)
    {
    
        PELOGE(limLog(pMac, LOGE, FL("session does not exist for given sessionId\n"));)
        return;
    }

    #if 0
    if (wlan_cfgGetStr(pMac, WNI_CFG_BSSID, currentBssId, &cfg) !=
                                eSIR_SUCCESS)
    {
        /// Could not get BSSID from CFG. Log error.
        limLog(pMac, LOGP, FL("could not retrieve BSSID\n"));
    }
    #endif //SUPPORT BT-AMP
    sirCopyMacAddr(currentBssId,psessionEntry->bssId);

    switch (psessionEntry->limSystemRole)
    {
        case eLIM_STA_ROLE:
        case eLIM_BT_AMP_STA_ROLE:
            switch (psessionEntry->limMlmState)
            {
                case eLIM_MLM_IDLE_STATE:
                    // Attempting to Deauthenticate
                    // with a pre-authenticated peer.
                    // Deauthetiate with peer if there
                    // exists a pre-auth context below.
                    break;

                case eLIM_MLM_AUTHENTICATED_STATE:
                case eLIM_MLM_WT_ASSOC_RSP_STATE:
                case eLIM_MLM_LINK_ESTABLISHED_STATE:
                    if (!palEqualMemory( pMac->hHdd,pMlmDeauthReq->peerMacAddr,
                                  currentBssId,
                                  sizeof(tSirMacAddr)) )
                    {
                        PELOGW(limLog(pMac, LOGW,
                           FL("received MLM_DEAUTH_REQ with invalid BSS id "));)
                        PELOGE(limLog(pMac, LOGE, FL("Peer MAC Addr : "));)
                        limPrintMacAddr(pMac, pMlmDeauthReq->peerMacAddr,LOGE);

                        PELOGE(limLog(pMac, LOGE, FL("\n CFG BSSID Addr : "));)
                        limPrintMacAddr(pMac, currentBssId,LOGE);

                        /// Prepare and Send LIM_MLM_DEAUTH_CNF

                        mlmDeauthCnf.resultCode = eSIR_SME_INVALID_PARAMETERS;

                        goto end;
                    }

                    if ((psessionEntry->limMlmState ==
                                       eLIM_MLM_AUTHENTICATED_STATE) ||
                         (psessionEntry->limMlmState ==
                                       eLIM_MLM_WT_ASSOC_RSP_STATE))
                    {
                        // Send Deauthentication frame
                        // to peer entity
                        limSendDeauthMgmtFrame(
                                   pMac,
                                   pMlmDeauthReq->reasonCode,
                                   pMlmDeauthReq->peerMacAddr,psessionEntry);

                        /// Prepare and Send LIM_MLM_DEAUTH_CNF
                        mlmDeauthCnf.resultCode = eSIR_SME_SUCCESS;
                        psessionEntry->limMlmState = eLIM_MLM_IDLE_STATE;
                        MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, 0, pMac->lim.gLimMlmState));
                        goto end;
                    }
                    else
                    {
                        // LINK_ESTABLISED_STATE
                        // Cleanup RX & TX paths
                        // below
                    }

                    break;

                default:

                    PELOGW(limLog(pMac, LOGW,
                       FL("received MLM_DEAUTH_REQ with in state %d for peer "),
                       psessionEntry->limMlmState);)
                    limPrintMacAddr(pMac, pMlmDeauthReq->peerMacAddr, LOGW);
                    limPrintMlmState(pMac, LOGW, psessionEntry->limMlmState);

                    /// Prepare and Send LIM_MLM_DEAUTH_CNF
                    mlmDeauthCnf.resultCode =
                                    eSIR_SME_STA_NOT_AUTHENTICATED;

                    goto end;
            }

            break;

        case eLIM_STA_IN_IBSS_ROLE:

            return;

        default: // eLIM_AP_ROLE
            break;

    } // end switch (psessionEntry->limSystemRole)

    /**
     * Check if there exists a context for the peer entity
     * to be deauthenticated with.
     */
    pStaDs = dphLookupHashEntry(pMac, pMlmDeauthReq->peerMacAddr, &aid, &psessionEntry->dph.dphHashTable);

    if (pStaDs == NULL)
    {
        /// Check if there exists pre-auth context for this STA
        pAuthNode = limSearchPreAuthList(pMac,
                                pMlmDeauthReq->peerMacAddr);

        if (pAuthNode == NULL)
        {
            /**
             * Received DEAUTH REQ for a STA that is neither
             * Associated nor Pre-authenticated. Log error,
             * Prepare and Send LIM_MLM_DEAUTH_CNF
             */
            PELOGW(limLog(pMac, LOGW,
               FL("received MLM_DEAUTH_REQ for STA that does not have context, Addr="));)
            limPrintMacAddr(pMac, pMlmDeauthReq->peerMacAddr, LOGW);

            mlmDeauthCnf.resultCode =
                                    eSIR_SME_STA_NOT_AUTHENTICATED;
        }
        else
        {
            mlmDeauthCnf.resultCode = eSIR_SME_SUCCESS;

            /// Delete STA from pre-auth STA list
            limDeletePreAuthNode(pMac, pMlmDeauthReq->peerMacAddr);

            /// Send Deauthentication frame to peer entity
            limSendDeauthMgmtFrame(pMac,
                                   pMlmDeauthReq->reasonCode,
                                   pMlmDeauthReq->peerMacAddr,psessionEntry);
        }

        goto end;
    }
    else if ((pStaDs->mlmStaContext.mlmState !=
                                     eLIM_MLM_LINK_ESTABLISHED_STATE) &&
             (pStaDs->mlmStaContext.mlmState !=
                                          eLIM_MLM_WT_ASSOC_CNF_STATE))
    {
        /**
         * Received LIM_MLM_DEAUTH_REQ for STA that is n
         * some transit state. Log error.
         */
        PELOGW(limLog(pMac, LOGW,
           FL("received MLM_DEAUTH_REQ for STA that either has no context or in some transit state, Addr="));)
        limPrintMacAddr(pMac, pMlmDeauthReq->peerMacAddr, LOGW);

        /// Prepare and Send LIM_MLM_DEAUTH_CNF

        mlmDeauthCnf.resultCode    = eSIR_SME_INVALID_PARAMETERS;

        goto end;
    }

    /* We received disassoc request and about to send disassoc frame
     * to AP. PE shall reset the EDCA parameters to default parameters 
     * as advertised by AP and send the update to HAL;
     */
    if (psessionEntry->limSystemRole == eLIM_STA_ROLE )
    {
        schSetDefaultEdcaParams(pMac);
        if (pStaDs != NULL)
        {
            if (pStaDs->aniPeer == eANI_BOOLEAN_TRUE) 
                limSendEdcaParams(pMac, psessionEntry->gLimEdcaParamsActive, pStaDs->bssId, eANI_BOOLEAN_TRUE);
            else
                limSendEdcaParams(pMac, psessionEntry->gLimEdcaParamsActive, pStaDs->bssId, eANI_BOOLEAN_FALSE);
        }
        else
            PELOGE(limLog(pMac, LOGE, FL("Self entry missing in Hash Table \n"));)
    }

    //pStaDs->mlmStaContext.rxPurgeReq     = 1;
    pStaDs->mlmStaContext.disassocReason = (tSirMacReasonCodes)
                                           pMlmDeauthReq->reasonCode;
    pStaDs->mlmStaContext.cleanupTrigger = pMlmDeauthReq->deauthTrigger;

    /// Send Deauthentication frame to peer entity
    limSendDeauthMgmtFrame(pMac, pMlmDeauthReq->reasonCode,
                           pMlmDeauthReq->peerMacAddr,psessionEntry);

    if( (psessionEntry->limSystemRole == eLIM_AP_ROLE))
    {
      // Delay DEL STA for 300ms such that unicast deauth is 
      // delivered at TIM(100 for normal or 300ms for dynamic) 
      // to power save stations after setting PVB
      vos_sleep(300);
    }

    /// Receive path cleanup with dummy packet
    limCleanupRxPath(pMac, pStaDs,psessionEntry);

    /// Free up buffer allocated for mlmDeauthReq
    palFreeMemory( pMac->hHdd, (tANI_U8 *) pMlmDeauthReq);

    return;

end:
    palCopyMemory( pMac->hHdd, (tANI_U8 *) &mlmDeauthCnf.peerMacAddr,
                  (tANI_U8 *) pMlmDeauthReq->peerMacAddr,
                  sizeof(tSirMacAddr));
    mlmDeauthCnf.deauthTrigger = pMlmDeauthReq->deauthTrigger;
    mlmDeauthCnf.aid           = pMlmDeauthReq->aid;
    mlmDeauthCnf.sessionId = pMlmDeauthReq->sessionId;

    // Free up buffer allocated
    // for mlmDeauthReq
    palFreeMemory( pMac->hHdd, (tANI_U8 *) pMlmDeauthReq);

    limPostSmeMessage(pMac,
                      LIM_MLM_DEAUTH_CNF,
                      (tANI_U32 *) &mlmDeauthCnf);
} /*** limProcessMlmDeauthReq() ***/



/**
 * @function : limProcessMlmSetKeysReq()
 *
 * @brief : This function is called to process MLM_SETKEYS_REQ message
 * from SME
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pMsgBuf  A pointer to the MLM message buffer
 * @return None
 */

static void
limProcessMlmSetKeysReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
tANI_U16           aid;
tANI_U16           staIdx = 0;
tANI_U32           defaultKeyId = 0;
tSirMacAddr        currentBssId;
tpDphHashNode      pStaDs;
tLimMlmSetKeysReq  *pMlmSetKeysReq;
tLimMlmSetKeysCnf  mlmSetKeysCnf;
tpPESession        psessionEntry;

  if(pMsgBuf == NULL)
  {
         PELOGE(limLog(pMac, LOGE,FL("Buffer is Pointing to NULL\n"));)
         return;
  }


  pMlmSetKeysReq = (tLimMlmSetKeysReq *) pMsgBuf;
  // Hold onto the SetKeys request parameters
  pMac->lim.gpLimMlmSetKeysReq = (void *) pMlmSetKeysReq;

  if((psessionEntry = peFindSessionBySessionId(pMac,pMlmSetKeysReq->sessionId))== NULL)
  {
    PELOGE(limLog(pMac, LOGE,FL("session does not exist for given sessionId\n"));)
    return;
  }

  limLog( pMac, LOGW,
      FL( "Received MLM_SETKEYS_REQ with parameters:\n"
        "AID [%d], ED Type [%d], # Keys [%d] & Peer MAC Addr - "),
      pMlmSetKeysReq->aid,
      pMlmSetKeysReq->edType,
      pMlmSetKeysReq->numKeys );
  limPrintMacAddr( pMac, pMlmSetKeysReq->peerMacAddr, LOGW );

    #if 0
    if( eSIR_SUCCESS != wlan_cfgGetStr( pMac, WNI_CFG_BSSID, currentBssId, &cfg )) {
    limLog( pMac, LOGP, FL("Could not retrieve BSSID\n"));
        return;
    }
    #endif //TO SUPPORT BT-AMP
    sirCopyMacAddr(currentBssId,psessionEntry->bssId);

    switch( psessionEntry->limSystemRole ) {
    case eLIM_STA_ROLE:
    case eLIM_BT_AMP_STA_ROLE:
        if((!limIsAddrBC( pMlmSetKeysReq->peerMacAddr ) ) &&
          (!palEqualMemory( pMac->hHdd,pMlmSetKeysReq->peerMacAddr,
                         currentBssId, sizeof(tSirMacAddr))) ){
            limLog( pMac, LOGW, FL("Received MLM_SETKEYS_REQ with invalid BSSID\n"));
        limPrintMacAddr( pMac, pMlmSetKeysReq->peerMacAddr, LOGW );

        // Prepare and Send LIM_MLM_SETKEYS_CNF with error code
        mlmSetKeysCnf.resultCode = eSIR_SME_INVALID_PARAMETERS;
        goto end;
      }
      // Fall thru' & 'Plumb' keys below
      break;
    case eLIM_STA_IN_IBSS_ROLE:
    default: // others
      // Fall thru...
      break;
  }

    /**
      * Use the "unicast" parameter to determine if the "Group Keys"
      * are being set.
      * pMlmSetKeysReq->key.unicast = 0 -> Multicast/broadcast
      * pMlmSetKeysReq->key.unicast - 1 -> Unicast keys are being set
      */
    if( limIsAddrBC( pMlmSetKeysReq->peerMacAddr )) {
        limLog( pMac, LOG1, FL("Trying to set Group Keys...%d \n"), pMlmSetKeysReq->sessionId);
        /** When trying to set Group Keys for any
          * security mode other than WEP, use the
          * STA Index corresponding to the AP...
          */
        switch( pMlmSetKeysReq->edType ) {
      case eSIR_ED_CCMP:
        staIdx = psessionEntry->staId;
        break;

      default:
        break;
    }
    }else {
        limLog( pMac, LOG1, FL("Trying to set Unicast Keys...\n"));
    /**
     * Check if there exists a context for the
     * peer entity for which keys need to be set.
     */


    pStaDs = dphLookupHashEntry( pMac, pMlmSetKeysReq->peerMacAddr, &aid , &psessionEntry->dph.dphHashTable);

#ifdef WLAN_SOFTAP_FEATURE
    if ((pStaDs == NULL) ||
           ((pStaDs->mlmStaContext.mlmState != eLIM_MLM_LINK_ESTABLISHED_STATE) && (psessionEntry->limSystemRole != eLIM_AP_ROLE))) {
#else
    if ((pStaDs == NULL) ||
           ((pStaDs->mlmStaContext.mlmState != eLIM_MLM_LINK_ESTABLISHED_STATE) )) {
#endif
        /**
         * Received LIM_MLM_SETKEYS_REQ for STA
         * that does not have context or in some
         * transit state. Log error.
         */
            limLog( pMac, LOG1,
            FL("Received MLM_SETKEYS_REQ for STA that either has no context or in some transit state, Addr = "));
        limPrintMacAddr( pMac, pMlmSetKeysReq->peerMacAddr, LOGW );

        // Prepare and Send LIM_MLM_SETKEYS_CNF
        mlmSetKeysCnf.resultCode = eSIR_SME_INVALID_PARAMETERS;
        goto end;
        } else
      staIdx = pStaDs->staIndex;
  }

    if ((pMlmSetKeysReq->numKeys == 0) && (pMlmSetKeysReq->edType != eSIR_ED_NONE)) {
    //
    // Broadcast/Multicast Keys (for WEP!!) are NOT sent
    // via this interface!!
    //
    // This indicates to HAL that the WEP Keys need to be
    // extracted from the CFG and applied to hardware
    defaultKeyId = 0xff;
      }else
    defaultKeyId = 0;

    limLog( pMac, LOG1,
      FL( "Trying to set keys for STA Index [%d], using defaultKeyId [%d]\n" ),
      staIdx,
      defaultKeyId );

    if(limIsAddrBC( pMlmSetKeysReq->peerMacAddr )) {
  psessionEntry->limPrevMlmState = psessionEntry->limMlmState;
  psessionEntry->limMlmState = eLIM_MLM_WT_SET_BSS_KEY_STATE;
  MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, 0, pMac->lim.gLimMlmState));
        limLog( pMac, LOG1, FL("Trying to set Group Keys...%d \n"), 
            psessionEntry->peSessionId);

    // Package WDA_SET_BSSKEY_REQ message parameters
        limSendSetBssKeyReq(pMac, pMlmSetKeysReq,psessionEntry);
    return;
    }else {
    // Package WDA_SET_STAKEY_REQ / WDA_SET_STA_BCASTKEY_REQ message parameters
        limSendSetStaKeyReq(pMac, pMlmSetKeysReq, staIdx, (tANI_U8) defaultKeyId,psessionEntry);
    return;
  }

end:
    mlmSetKeysCnf.sessionId= pMlmSetKeysReq->sessionId;
    limPostSmeSetKeysCnf( pMac, pMlmSetKeysReq, &mlmSetKeysCnf );

} /*** limProcessMlmSetKeysReq() ***/

/**
 * limProcessMlmRemoveKeyReq()
 *
 *FUNCTION:
 * This function is called to process MLM_REMOVEKEY_REQ message
 * from SME
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pMsgBuf  A pointer to the MLM message buffer
 * @return None
 */

static void
limProcessMlmRemoveKeyReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
tANI_U16           aid;
tANI_U16           staIdx = 0;
tSirMacAddr        currentBssId;
tpDphHashNode      pStaDs;
tLimMlmRemoveKeyReq  *pMlmRemoveKeyReq;
tLimMlmRemoveKeyCnf  mlmRemoveKeyCnf;
    tpPESession         psessionEntry;

    if(pMsgBuf == NULL)
    {
           PELOGE(limLog(pMac, LOGE,FL("Buffer is Pointing to NULL\n"));)
           return;
    }

    pMlmRemoveKeyReq = (tLimMlmRemoveKeyReq *) pMsgBuf;


    if((psessionEntry = peFindSessionBySessionId(pMac,pMlmRemoveKeyReq->sessionId))== NULL)
    {
        PELOGE(limLog(pMac, LOGE,
                    FL("session does not exist for given sessionId\n"));)
        return;
    }


    if( pMac->lim.gpLimMlmRemoveKeyReq != NULL )
    {
        // Free any previous requests.
        palFreeMemory( pMac->hHdd, (tANI_U8 *) pMac->lim.gpLimMlmRemoveKeyReq);
        pMac->lim.gpLimMlmRemoveKeyReq = NULL;
    }
    // Hold onto the RemoveKeys request parameters
    pMac->lim.gpLimMlmRemoveKeyReq = (void *) pMlmRemoveKeyReq; 

    #if 0
    if( eSIR_SUCCESS != wlan_cfgGetStr( pMac,
        WNI_CFG_BSSID,
        currentBssId,
        &cfg ))
    limLog( pMac, LOGP, FL("Could not retrieve BSSID\n"));
    #endif //TO-SUPPORT BT-AMP
    sirCopyMacAddr(currentBssId,psessionEntry->bssId);

    switch( psessionEntry->limSystemRole )
    {
        case eLIM_STA_ROLE:
        case eLIM_BT_AMP_STA_ROLE:
        if(( limIsAddrBC( pMlmRemoveKeyReq->peerMacAddr ) != true ) &&
          (!palEqualMemory( pMac->hHdd,pMlmRemoveKeyReq->peerMacAddr,
                         currentBssId,
                         sizeof(tSirMacAddr))))
        {
            limLog( pMac, LOGW,
            FL("Received MLM_REMOVEKEY_REQ with invalid BSSID\n"));
            limPrintMacAddr( pMac, pMlmRemoveKeyReq->peerMacAddr, LOGW );

            // Prepare and Send LIM_MLM_REMOVEKEY_CNF with error code
            mlmRemoveKeyCnf.resultCode = eSIR_SME_INVALID_PARAMETERS;
            goto end;
        }
        break;

        case eLIM_STA_IN_IBSS_ROLE:
        default: // eLIM_AP_ROLE
                 // Fall thru...
                break;
    }


    psessionEntry->limPrevMlmState = psessionEntry->limMlmState;
    if(limIsAddrBC( pMlmRemoveKeyReq->peerMacAddr )) //Second condition for IBSS or AP role.
    {
        psessionEntry->limMlmState = eLIM_MLM_WT_REMOVE_BSS_KEY_STATE;
        MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, 0, pMac->lim.gLimMlmState));    
        // Package WDA_REMOVE_BSSKEY_REQ message parameters
        limSendRemoveBssKeyReq( pMac,pMlmRemoveKeyReq,psessionEntry);
        return;
    }

  /**
    * Check if there exists a context for the
    * peer entity for which keys need to be removed.
    */
  pStaDs = dphLookupHashEntry( pMac, pMlmRemoveKeyReq->peerMacAddr, &aid, &psessionEntry->dph.dphHashTable );
  if ((pStaDs == NULL) ||
         (pStaDs &&
         (pStaDs->mlmStaContext.mlmState !=
                       eLIM_MLM_LINK_ESTABLISHED_STATE)))
  {
     /**
       * Received LIM_MLM_REMOVEKEY_REQ for STA
       * that does not have context or in some
       * transit state. Log error.
       */
      limLog( pMac, LOGW,
          FL("Received MLM_REMOVEKEYS_REQ for STA that either has no context or in some transit state, Addr = "));
      limPrintMacAddr( pMac, pMlmRemoveKeyReq->peerMacAddr, LOGW );

      // Prepare and Send LIM_MLM_REMOVEKEY_CNF
      mlmRemoveKeyCnf.resultCode = eSIR_SME_INVALID_PARAMETERS;
      mlmRemoveKeyCnf.sessionId = pMlmRemoveKeyReq->sessionId;
      

      goto end;
  }
  else
    staIdx = pStaDs->staIndex;
  


    psessionEntry->limMlmState = eLIM_MLM_WT_REMOVE_STA_KEY_STATE;
    MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, 0, pMac->lim.gLimMlmState));

    // Package WDA_REMOVE_STAKEY_REQ message parameters
    limSendRemoveStaKeyReq( pMac,pMlmRemoveKeyReq,staIdx,psessionEntry);
    return;
 
end:
    limPostSmeRemoveKeyCnf( pMac,
      pMlmRemoveKeyReq,
      &mlmRemoveKeyCnf );

} /*** limProcessMlmRemoveKeyReq() ***/


/**
 * limProcessMinChannelTimeout()
 *
 *FUNCTION:
 * This function is called to process Min Channel Timeout
 * during channel scan.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @return None
 */

static void
limProcessMinChannelTimeout(tpAniSirGlobal pMac)
{
    tANI_U8 channelNum;
    
#ifdef GEN6_TODO
    //if the min Channel is maintained per session, then use the below seesionEntry
    //priority - LOW/might not be needed
    
    //TBD-RAJESH HOW TO GET sessionEntry?????
    tpPESession psessionEntry;

    if((psessionEntry = peFindSessionBySessionId(pMac, pMac->lim.limTimers.gLimMinChannelTimer.sessionId))== NULL) 
    {
        limLog(pMac, LOGP,FL("Session Does not exist for given sessionID\n"));
        return;
    }
#endif

    
    if (pMac->lim.gLimMlmState == eLIM_MLM_WT_PROBE_RESP_STATE)
    {
        PELOG1(limLog(pMac, LOG1, FL("Scanning : min channel timeout occurred\n"));)

        /// Min channel timer timed out
        limDeactivateAndChangeTimer(pMac, eLIM_MIN_CHANNEL_TIMER);
        channelNum = (tANI_U8)limGetCurrentScanChannel(pMac);
        limSendHalEndScanReq(pMac, channelNum, eLIM_HAL_END_SCAN_WAIT_STATE);
    }
    else
    {
    /**
         * MIN channel timer should not have timed out
         * in states other than wait_probe_response.
         * Log error.
         */
        limLog(pMac, LOGW,
           FL("received unexpected MIN channel timeout in state %X\n"),
           pMac->lim.gLimMlmState);
        limPrintMlmState(pMac, LOGE, pMac->lim.gLimMlmState);
    }
} /*** limProcessMinChannelTimeout() ***/



/**
 * limProcessMaxChannelTimeout()
 *
 *FUNCTION:
 * This function is called to process Max Channel Timeout
 * during channel scan.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @return None
 */

static void
limProcessMaxChannelTimeout(tpAniSirGlobal pMac)
{
    tANI_U8 channelNum;

    
    if (pMac->lim.gLimMlmState == eLIM_MLM_WT_PROBE_RESP_STATE ||
        pMac->lim.gLimMlmState == eLIM_MLM_PASSIVE_SCAN_STATE)
    {
        PELOG1(limLog(pMac, LOG1, FL("Scanning : Max channel timed out\n"));)
        /**
         * MAX channel timer timed out
         * Continue channel scan.
         */
        limDeactivateAndChangeTimer(pMac, eLIM_MAX_CHANNEL_TIMER);
        channelNum = limGetCurrentScanChannel(pMac);
        limSendHalEndScanReq(pMac, channelNum, eLIM_HAL_END_SCAN_WAIT_STATE);
    }
    else
    {
        /**
         * MAX channel timer should not have timed out
         * in states other than wait_scan.
         * Log error.
         */
        limLog(pMac, LOGW,
           FL("received unexpected MAX channel timeout in state %X\n"),
           pMac->lim.gLimMlmState);
        limPrintMlmState(pMac, LOGW, pMac->lim.gLimMlmState);
    }
} /*** limProcessMaxChannelTimeout() ***/



/**
 * limProcessJoinFailureTimeout()
 *
 *FUNCTION:
 * This function is called to process JoinFailureTimeout
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @return None
 */

static void
limProcessJoinFailureTimeout(tpAniSirGlobal pMac)
{
    tLimMlmJoinCnf  mlmJoinCnf;
    tSirMacAddr bssid;
    tANI_U32 len;
    
    //fetch the sessionEntry based on the sessionId
    tpPESession psessionEntry;

    if((psessionEntry = peFindSessionBySessionId(pMac, pMac->lim.limTimers.gLimJoinFailureTimer.sessionId))== NULL) 
    {
        limLog(pMac, LOGP,FL("Session Does not exist for given sessionID\n"));
        return;
    }
        
    if (psessionEntry->limMlmState == eLIM_MLM_WT_JOIN_BEACON_STATE)
    {
        len = sizeof(tSirMacAddr);

        if (wlan_cfgGetStr(pMac, WNI_CFG_BSSID, bssid, &len) !=
                            eSIR_SUCCESS)
        {
            /// Could not get BSSID from CFG. Log error.
            limLog(pMac, LOGP, FL("could not retrieve BSSID\n"));
            return;
        }

        // 'Change' timer for future activations
        limDeactivateAndChangeTimer(pMac, eLIM_JOIN_FAIL_TIMER);

        /**
         * Issue MLM join confirm with timeout reason code
         */
        PELOGE(limLog(pMac, LOGE,  FL(" Join Failure Timeout occurred.\n"));)

        mlmJoinCnf.resultCode = eSIR_SME_JOIN_TIMEOUT_RESULT_CODE;
        mlmJoinCnf.protStatusCode = eSIR_MAC_UNSPEC_FAILURE_STATUS;

        psessionEntry->limMlmState = eLIM_MLM_IDLE_STATE;
        MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, 0, pMac->lim.gLimMlmState));
        if(limSetLinkState(pMac, eSIR_LINK_IDLE_STATE, psessionEntry->bssId, 
            psessionEntry->selfMacAddr, NULL, NULL) != eSIR_SUCCESS)
            PELOGE(limLog(pMac, LOGE,  FL("Failed to set the LinkState\n"));)
        /* Update PE session Id */
        mlmJoinCnf.sessionId = psessionEntry->peSessionId;
        
       
        // Freeup buffer allocated to join request
        if (psessionEntry->pLimMlmJoinReq)
        {
            palFreeMemory( pMac->hHdd, psessionEntry->pLimMlmJoinReq);
            psessionEntry->pLimMlmJoinReq = NULL;
        }
        
        limPostSmeMessage(pMac,
                          LIM_MLM_JOIN_CNF,
                          (tANI_U32 *) &mlmJoinCnf);

        return;
    }
    else
    {
        /**
         * Join failure timer should not have timed out
         * in states other than wait_join_beacon state.
         * Log error.
         */
        limLog(pMac, LOGW,
           FL("received unexpected JOIN failure timeout in state %X\n"),psessionEntry->limMlmState);
        limPrintMlmState(pMac, LOGW, psessionEntry->limMlmState);
    }
} /*** limProcessJoinFailureTimeout() ***/



/**
 * limProcessAuthFailureTimeout()
 *
 *FUNCTION:
 * This function is called to process Min Channel Timeout
 * during channel scan.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @return None
 */

static void
limProcessAuthFailureTimeout(tpAniSirGlobal pMac)
{
    //fetch the sessionEntry based on the sessionId
    tpPESession psessionEntry;

    if((psessionEntry = peFindSessionBySessionId(pMac, pMac->lim.limTimers.gLimAuthFailureTimer.sessionId))== NULL) 
    {
        limLog(pMac, LOGP,FL("Session Does not exist for given sessionID\n"));
        return;
    }
    
    switch (psessionEntry->limMlmState)
    {
        case eLIM_MLM_WT_AUTH_FRAME2_STATE:
        case eLIM_MLM_WT_AUTH_FRAME4_STATE:
            /**
             * Requesting STA did not receive next auth frame
             * before Auth Failure timeout.
             * Issue MLM auth confirm with timeout reason code
             */

             
            limRestoreFromAuthState(pMac,eSIR_SME_AUTH_TIMEOUT_RESULT_CODE,eSIR_MAC_UNSPEC_FAILURE_REASON,psessionEntry);
            break;

        default:
            /**
             * Auth failure timer should not have timed out
             * in states other than wt_auth_frame2/4
             * Log error.
             */
            PELOGE(limLog(pMac, LOGE, FL("received unexpected AUTH failure timeout in state %X\n"), psessionEntry->limMlmState);)
            limPrintMlmState(pMac, LOGE, psessionEntry->limMlmState);

            break;
    }
} /*** limProcessAuthFailureTimeout() ***/



/**
 * limProcessAuthRspTimeout()
 *
 *FUNCTION:
 * This function is called to process Min Channel Timeout
 * during channel scan.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @return None
 */

static void
limProcessAuthRspTimeout(tpAniSirGlobal pMac, tANI_U32 authIndex)
{
    struct tLimPreAuthNode *pAuthNode;
    tpPESession        psessionEntry;
    tANI_U8            sessionId;

    pAuthNode = limGetPreAuthNodeFromIndex(pMac, &pMac->lim.gLimPreAuthTimerTable, authIndex);

    if((psessionEntry = peFindSessionByBssid(pMac, pAuthNode->peerMacAddr, &sessionId)) == NULL)
    {
        limLog(pMac, LOGW, FL("session does not exist for given BSSID \n"));
        return;
    } 

    if (psessionEntry->limSystemRole == eLIM_AP_ROLE ||
        psessionEntry->limSystemRole == eLIM_STA_IN_IBSS_ROLE)
    {
        // Check if there exists a context for the STA

        if (pAuthNode == NULL)
        {
            /**
             * Authentication response timer timedout for an STA
             * that does not have context at AP/STA in IBSS mode.
             */

            // Log error
            PELOGE(limLog(pMac, LOGE, FL("received unexpected message\n"));)
        }
        else
        {
            if (pAuthNode->mlmState != eLIM_MLM_WT_AUTH_FRAME3_STATE)
            {
                /**
                 * Authentication response timer timedout
                 * in unexpected state. Log error
                 */
                PELOGE(limLog(pMac, LOGE,
                   FL("received unexpected AUTH rsp timeout for MAC address "));
                limPrintMacAddr(pMac, pAuthNode->peerMacAddr, LOGE);)
            }
            else
            {
                // Authentication response timer
                // timedout for an STA.
                pAuthNode->mlmState = eLIM_MLM_AUTH_RSP_TIMEOUT_STATE;
                pAuthNode->fTimerStarted = 0;
               PELOG1( limLog(pMac, LOG1,
                       FL("AUTH rsp timedout for MAC address "));
                limPrintMacAddr(pMac, pAuthNode->peerMacAddr, LOG1);)

                // Change timer to reactivate it in future
                limDeactivateAndChangePerStaIdTimer(pMac,
                                                    eLIM_AUTH_RSP_TIMER,
                                                    pAuthNode->authNodeIdx);

                limDeletePreAuthNode(pMac, pAuthNode->peerMacAddr);
            }
        }
    }
} /*** limProcessAuthRspTimeout() ***/


/**
 * limProcessAssocFailureTimeout()
 *
 *FUNCTION:
 * This function is called to process Min Channel Timeout
 * during channel scan.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @return None
 */

static void
limProcessAssocFailureTimeout(tpAniSirGlobal pMac, tANI_U32 MsgType)
{

    tLimMlmAssocCnf     mlmAssocCnf;
    tpPESession         psessionEntry;
    
    //to fetch the lim/mlm state based on the sessionId, use the below sessionEntry
    tANI_U8 sessionId;
    
    if(MsgType == LIM_ASSOC)
    {
        sessionId = pMac->lim.limTimers.gLimAssocFailureTimer.sessionId;
    }
    else
    {
        sessionId = pMac->lim.limTimers.gLimReassocFailureTimer.sessionId;
    }
    
    if((psessionEntry = peFindSessionBySessionId(pMac, sessionId))== NULL) 
    {
        limLog(pMac, LOGP,FL("Session Does not exist for given sessionID\n"));
        return;
    }
    
    /**
     * Expected Re/Association Response frame
     * not received within Re/Association Failure Timeout.
     */




    /* CR: vos packet memory is leaked when assoc rsp timeouted/failed. */
    /* notify TL that association is failed so that TL can flush the cached frame  */
    WLANTL_AssocFailed (psessionEntry->staId);

    // Log error
    PELOG1(limLog(pMac, LOG1,
       FL("Re/Association Response not received before timeout \n"));)

    if (( (psessionEntry->limSystemRole == eLIM_AP_ROLE) || (psessionEntry->limSystemRole == eLIM_BT_AMP_AP_ROLE) )||
        ( (psessionEntry->limMlmState != eLIM_MLM_WT_ASSOC_RSP_STATE) &&
          (psessionEntry->limMlmState != eLIM_MLM_WT_REASSOC_RSP_STATE) ) )
    {
        /**
         * Re/Assoc failure timer should not have timedout on AP
         * or in a state other than wt_re/assoc_response.
         */

        // Log error
        limLog(pMac, LOGW,
           FL("received unexpected REASSOC failure timeout in state %X for role %d\n"),
           psessionEntry->limMlmState, psessionEntry->limSystemRole);
        limPrintMlmState(pMac, LOGW, psessionEntry->limMlmState);
    }
    else
    {

        if (MsgType == LIM_ASSOC)
        {

            PELOGE(limLog(pMac, LOGE,  FL("Assoc Failure Timeout occurred.\n"));)

            psessionEntry->limMlmState = eLIM_MLM_IDLE_STATE;
            MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, 0, psessionEntry->limMlmState));
            
            //Set the RXP mode to IDLE, so it starts filtering the frames.
            if(limSetLinkState(pMac, eSIR_LINK_IDLE_STATE,psessionEntry->bssId, 
                psessionEntry->selfMacAddr, NULL, NULL) != eSIR_SUCCESS)
                PELOGE(limLog(pMac, LOGE,  FL("Failed to set the LinkState\n"));)
         
            // 'Change' timer for future activations
            limDeactivateAndChangeTimer(pMac, eLIM_ASSOC_FAIL_TIMER);

            // Free up buffer allocated for JoinReq held by
            // MLM state machine
            if (psessionEntry->pLimMlmJoinReq)
            {
                palFreeMemory( pMac->hHdd, psessionEntry->pLimMlmJoinReq);
                psessionEntry->pLimMlmJoinReq = NULL;
            }

#if defined(ANI_PRODUCT_TYPE_CLIENT)
            //To remove the preauth node in case of fail to associate
            if (limSearchPreAuthList(pMac, psessionEntry->bssId))
            {
                PELOG1(limLog(pMac, LOG1, FL(" delete pre auth node for %02X-%02X-%02X-%02X-%02X-%02X\n"),
                    psessionEntry->bssId[0], psessionEntry->bssId[1], psessionEntry->bssId[2], 
                    psessionEntry->bssId[3], psessionEntry->bssId[4], psessionEntry->bssId[5]);)
                limDeletePreAuthNode(pMac, psessionEntry->bssId);
            }
#endif

            mlmAssocCnf.resultCode =
                            eSIR_SME_ASSOC_TIMEOUT_RESULT_CODE;
            mlmAssocCnf.protStatusCode = 
                            eSIR_MAC_UNSPEC_FAILURE_STATUS;
            
            /* Update PE session Id*/
            mlmAssocCnf.sessionId = psessionEntry->peSessionId;
            limPostSmeMessage(pMac,
                              LIM_MLM_ASSOC_CNF,
                              (tANI_U32 *) &mlmAssocCnf);
        }
        else
        {
            /**
             * Restore pre-reassoc req state.
             * Set BSSID to currently associated AP address.
             */
            psessionEntry->limMlmState = psessionEntry->limPrevMlmState;
            MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, 0, pMac->lim.gLimMlmState));

            limRestorePreReassocState(pMac, 
                eSIR_SME_REASSOC_TIMEOUT_RESULT_CODE, eSIR_MAC_UNSPEC_FAILURE_STATUS,psessionEntry);
        }
    }
} /*** limProcessAssocFailureTimeout() ***/



/**
 * limCompleteMlmScan()
 *
 *FUNCTION:
 * This function is called to send MLM_SCAN_CNF message
 * to SME state machine.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  retCode   Result code to be sent
 * @return None
 */

void
limCompleteMlmScan(tpAniSirGlobal pMac, tSirResultCodes retCode)
{
    tLimMlmScanCnf    mlmScanCnf;

    /// Restore previous MLM state
    pMac->lim.gLimMlmState = pMac->lim.gLimPrevMlmState;
    MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, 0, pMac->lim.gLimMlmState));
    limRestorePreScanState(pMac);

    // Free up pMac->lim.gLimMlmScanReq
    if( NULL != pMac->lim.gpLimMlmScanReq )
    {
        palFreeMemory( pMac->hHdd, pMac->lim.gpLimMlmScanReq);
        pMac->lim.gpLimMlmScanReq = NULL;
    }

    mlmScanCnf.resultCode       = retCode;
    mlmScanCnf.scanResultLength = pMac->lim.gLimMlmScanResultLength;

    limPostSmeMessage(pMac, LIM_MLM_SCAN_CNF, (tANI_U32 *) &mlmScanCnf);

} /*** limCompleteMlmScan() ***/

/**
 * \brief Setup an A-MPDU/BA session
 *
 * \sa limProcessMlmAddBAReq
 *
 * \param pMac The global tpAniSirGlobal object
 *
 * \param pMsgBuf The MLME ADDBA Req message buffer
 *
 * \return none
 */
void limProcessMlmAddBAReq( tpAniSirGlobal pMac,
    tANI_U32 *pMsgBuf )
{
tSirRetStatus status = eSIR_SUCCESS;
tpLimMlmAddBAReq pMlmAddBAReq;
tpLimMlmAddBACnf pMlmAddBACnf;
  tpPESession     psessionEntry;
    
  if(pMsgBuf == NULL)
  {
      PELOGE(limLog(pMac, LOGE,FL("Buffer is Pointing to NULL\n"));)
           return;
  }

  pMlmAddBAReq = (tpLimMlmAddBAReq) pMsgBuf;
  if((psessionEntry = peFindSessionBySessionId(pMac,pMlmAddBAReq->sessionId))== NULL)
  {
      PELOGE(limLog(pMac, LOGE,
               FL("session does not exist for given sessionId\n"));)
      return;
  }
  

  // Send ADDBA Req over the air
  status = limSendAddBAReq( pMac, pMlmAddBAReq,psessionEntry);

  //
  // Respond immediately to LIM, only if MLME has not been
  // successfully able to send WDA_ADDBA_REQ to HAL.
  // Else, LIM_MLM_ADDBA_CNF will be sent after receiving
  // ADDBA Rsp from peer entity
  //
  if( eSIR_SUCCESS != status )
  {
    // Allocate for LIM_MLM_ADDBA_CNF
    if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd,
                                     (void **) &pMlmAddBACnf,
                                     sizeof( tLimMlmAddBACnf )))
    {
      limLog( pMac, LOGP,
          FL("palAllocateMemory failed with error code %d\n"));
      palFreeMemory( pMac->hHdd, (tANI_U8 *) pMsgBuf );
      return;
    }
    else
    {
        palZeroMemory( pMac->hHdd, (void *) pMlmAddBACnf, sizeof( tLimMlmAddBACnf ));
        palCopyMemory( pMac->hHdd,
          (void *) pMlmAddBACnf->peerMacAddr,
          (void *) pMlmAddBAReq->peerMacAddr,
          sizeof( tSirMacAddr ));

      pMlmAddBACnf->baDialogToken = pMlmAddBAReq->baDialogToken;
      pMlmAddBACnf->baTID = pMlmAddBAReq->baTID;
      pMlmAddBACnf->baPolicy = pMlmAddBAReq->baPolicy;
      pMlmAddBACnf->baBufferSize = pMlmAddBAReq->baBufferSize;
      pMlmAddBACnf->baTimeout = pMlmAddBAReq->baTimeout;
      pMlmAddBACnf->sessionId = pMlmAddBAReq->sessionId;

      // Update the result code
      pMlmAddBACnf->addBAResultCode = eSIR_MAC_UNSPEC_FAILURE_STATUS;

      limPostSmeMessage( pMac,
          LIM_MLM_ADDBA_CNF,
          (tANI_U32 *) pMlmAddBACnf );
    }

    // Restore MLME state
    psessionEntry->limMlmState = psessionEntry->limPrevMlmState;
    MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, 0, pMac->lim.gLimMlmState));

  }

  // Free the buffer allocated for tLimMlmAddBAReq
  palFreeMemory( pMac->hHdd, (tANI_U8 *) pMsgBuf );

}

/**
 * \brief Send an ADDBA Rsp to peer STA in response
 * to an ADDBA Req received earlier
 *
 * \sa limProcessMlmAddBARsp
 *
 * \param pMac The global tpAniSirGlobal object
 *
 * \param pMsgBuf The MLME ADDBA Rsp message buffer
 *
 * \return none
 */
void limProcessMlmAddBARsp( tpAniSirGlobal pMac,
    tANI_U32 *pMsgBuf )
{
tpLimMlmAddBARsp pMlmAddBARsp;
   tANI_U16 aid;
   tpDphHashNode pSta;
   tpPESession  psessionEntry;

    
    if(pMsgBuf == NULL)
    {
           PELOGE(limLog(pMac, LOGE,FL("Buffer is Pointing to NULL\n"));)
           return;
    }

    pMlmAddBARsp = (tpLimMlmAddBARsp) pMsgBuf;

    if(( psessionEntry = peFindSessionBySessionId(pMac,pMlmAddBARsp->sessionId))== NULL)
    {
        PELOGE(limLog(pMac, LOGE,
                  FL("session does not exist for given session ID\n"));)
        return;
    }
  

  // Send ADDBA Rsp over the air
  if( eSIR_SUCCESS != limSendAddBARsp( pMac,pMlmAddBARsp,psessionEntry))
  {
    limLog( pMac, LOGE,
    FL("Failed to send ADDBA Rsp to peer \n"));
    limPrintMacAddr( pMac, pMlmAddBARsp->peerMacAddr, LOGE );
    /* Clean the BA context maintained by HAL and TL on failure */
    pSta = dphLookupHashEntry( pMac, pMlmAddBARsp->peerMacAddr, &aid, 
            &psessionEntry->dph.dphHashTable);
     if( NULL != pSta )
    {
        limPostMsgDelBAInd( pMac, pSta, pMlmAddBARsp->baTID, eBA_RECIPIENT, 
                psessionEntry);
    }
  }

  // Time to post a WDA_DELBA_IND to HAL in order
  // to cleanup the HAL and SoftMAC entries


  // Free the buffer allocated for tLimMlmAddBARsp
  palFreeMemory( pMac->hHdd, (tANI_U8 *) pMsgBuf );

}

/**
 * \brief Setup an A-MPDU/BA session
 *
 * \sa limProcessMlmDelBAReq
 *
 * \param pMac The global tpAniSirGlobal object
 *
 * \param pMsgBuf The MLME DELBA Req message buffer
 *
 * \return none
 */
void limProcessMlmDelBAReq( tpAniSirGlobal pMac,
    tANI_U32 *pMsgBuf )
{
    tSirRetStatus status = eSIR_SUCCESS;
    tpLimMlmDelBAReq pMlmDelBAReq;
    tpLimMlmDelBACnf pMlmDelBACnf;
    tpPESession  psessionEntry;
  
    
    if(pMsgBuf == NULL)
    {
           PELOGE(limLog(pMac, LOGE,FL("Buffer is Pointing to NULL\n"));)
           return;
    }

  // TODO - Need to validate MLME state
    pMlmDelBAReq = (tpLimMlmDelBAReq) pMsgBuf;

    if((psessionEntry = peFindSessionBySessionId(pMac,pMlmDelBAReq->sessionId))== NULL)
    {
        PELOGE(limLog(pMac, LOGE,FL("session does not exist for given bssId\n"));)
        return;
    }

  // Send DELBA Ind over the air
  if( eSIR_SUCCESS !=
      (status = limSendDelBAInd( pMac, pMlmDelBAReq,psessionEntry)))
    status = eSIR_SME_HAL_SEND_MESSAGE_FAIL;
  else
  {
    tANI_U16 aid;
    tpDphHashNode pSta;

    // Time to post a WDA_DELBA_IND to HAL in order
    // to cleanup the HAL and SoftMAC entries
    pSta = dphLookupHashEntry( pMac, pMlmDelBAReq->peerMacAddr, &aid , &psessionEntry->dph.dphHashTable);
    if( NULL != pSta )
    {
        status = limPostMsgDelBAInd( pMac,
         pSta,
          pMlmDelBAReq->baTID,
          pMlmDelBAReq->baDirection,psessionEntry);

    }
  }

  //
  // Respond immediately to SME with DELBA CNF using
  // LIM_MLM_DELBA_CNF with appropriate status
  //

  // Allocate for LIM_MLM_DELBA_CNF
  if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd,
                                   (void **) &pMlmDelBACnf,
                                   sizeof( tLimMlmDelBACnf )))
  {
    limLog( pMac, LOGP, FL("palAllocateMemory failed\n"));
    palFreeMemory( pMac->hHdd, (tANI_U8 *) pMsgBuf );
    return;
  }
  else
  {
    palZeroMemory( pMac->hHdd, (void *) pMlmDelBACnf, sizeof( tLimMlmDelBACnf ));

    palCopyMemory( pMac->hHdd,
        (void *) pMlmDelBACnf,
        (void *) pMlmDelBAReq,
        sizeof( tLimMlmDelBAReq ));

    // Update DELBA result code
    pMlmDelBACnf->delBAReasonCode = pMlmDelBAReq->delBAReasonCode;
    
    /* Update PE session Id*/
    pMlmDelBACnf->sessionId = pMlmDelBAReq->sessionId;

    limPostSmeMessage( pMac,
        LIM_MLM_DELBA_CNF,
        (tANI_U32 *) pMlmDelBACnf );
  }

  // Free the buffer allocated for tLimMlmDelBAReq
  palFreeMemory( pMac->hHdd, (tANI_U8 *) pMsgBuf );

}

/**
 * @function :  limSMPowerSaveStateInd( )
 *
 * @brief  : This function is called upon receiving the PMC Indication to update the STA's MimoPs State.
 *
 *      LOGIC:
 *
 *      ASSUMPTIONS:
 *          NA
 *
 *      NOTE:
 *          NA
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  limMsg - Lim Message structure object with the MimoPSparam in body
 * @return None
 */
 
tSirRetStatus
limSMPowerSaveStateInd(tpAniSirGlobal pMac, tSirMacHTMIMOPowerSaveState state)
{
#if 0
    tSirRetStatus           retStatus = eSIR_SUCCESS;  
#if 0
    tANI_U32                  cfgVal1;          
    tANI_U16                   cfgVal2;                 
    tSirMacHTCapabilityInfo *pHTCapabilityInfo;         
    tpDphHashNode pSta = NULL;

    tpPESession psessionEntry  = &pMac->lim.gpSession[0]; //TBD-RAJESH HOW TO GET sessionEntry?????
    /** Verify the Mode of operation */    
    if (pMac->lim.gLimSystemRole != eSYSTEM_STA_ROLE) {  
        PELOGE(limLog(pMac, LOGE, FL("Got PMC indication when System not in the STA Role\n"));)       
        return eSIR_FAILURE;       
    }      

    if ((pMac->lim.gHTMIMOPSState == state) || (state == eSIR_HT_MIMO_PS_NA )) { 
        PELOGE(limLog(pMac, LOGE, FL("Got Indication when already in the same mode or State passed is NA:%d \n"),  state);)      
        return eSIR_FAILURE;      
    }     

    if (!pMac->lim.htCapability){        
        PELOGW(limLog(pMac, LOGW, FL(" Not in 11n or HT capable mode\n"));)        
        return eSIR_FAILURE;   
    }        

    /** Update the CFG about the default MimoPS State */
    if (wlan_cfgGetInt(pMac, WNI_CFG_HT_CAP_INFO, &cfgVal1) != eSIR_SUCCESS) {  
            limLog(pMac, LOGP, FL("could not retrieve HT Cap CFG \n"));    
            return eSIR_FAILURE;     
    }          

    cfgVal2 = (tANI_U16)cfgVal1;            
    pHTCapabilityInfo = (tSirMacHTCapabilityInfo *) &cfgVal2;          
    pHTCapabilityInfo->mimoPowerSave = state;

    if(cfgSetInt(pMac, WNI_CFG_HT_CAP_INFO, *(tANI_U16*)pHTCapabilityInfo) != eSIR_SUCCESS) {   
        limLog(pMac, LOGP, FL("could not update HT Cap Info CFG\n"));                  
        return eSIR_FAILURE;
    }

    PELOG2(limLog(pMac, LOG2, FL(" The HT Capability for Mimo Pwr is updated to State: %u  \n"),state);)  
    if (pMac->lim.gLimSmeState != eLIM_SME_LINK_EST_STATE) { 
        PELOG2(limLog(pMac, LOG2,FL(" The STA is not in the Connected/Link Est Sme_State: %d  \n"), pMac->lim.gLimSmeState);)          
        /** Update in the LIM the MIMO PS state of the SELF */   
        pMac->lim.gHTMIMOPSState = state;          
        return eSIR_SUCCESS;    
    }              

    pSta = dphGetHashEntry(pMac, DPH_STA_HASH_INDEX_PEER, &psessionEntry->dph.dphHashTable);    
    if (!pSta->mlmStaContext.htCapability) {
        limLog( pMac, LOGE,FL( "limSendSMPowerState: Peer is not HT Capable \n" ));
        return eSIR_FAILURE;
    }
     
    if (isEnteringMimoPS(pMac->lim.gHTMIMOPSState, state)) {    
        tSirMacAddr             macAddr;      
        /** Obtain the AP's Mac Address */    
        palCopyMemory(pMac ->hHdd, (tANI_U8 *)macAddr, psessionEntry->bssId, sizeof(tSirMacAddr)); 
        /** Send Action Frame with the corresponding mode */       
        retStatus = limSendSMPowerStateFrame(pMac, macAddr, state);       
        if (retStatus != eSIR_SUCCESS) {         
            PELOGE(limLog(pMac, LOGE, "Update SM POWER: Sending Action Frame has failed\n");)        
            return retStatus;         
        }   
    }    

    /** Update MlmState about the SetMimoPS State */
    pMac->lim.gLimPrevMlmState = pMac->lim.gLimMlmState;
    pMac->lim.gLimMlmState = eLIM_MLM_WT_SET_MIMOPS_STATE;
    MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, 0, pMac->lim.gLimMlmState));

    /** Update the HAL and s/w mac about the mode to be set */    
    retStatus = limPostSMStateUpdate( pMac,psessionEntry->staId, state);     

    PELOG2(limLog(pMac, LOG2, " Updated the New SMPS State");)
    /** Update in the LIM the MIMO PS state of the SELF */   
    pMac->lim.gHTMIMOPSState = state;          
#endif
    return retStatus;
#endif
return eSIR_SUCCESS;
}

void 
limSetChannel(tpAniSirGlobal pMac, tANI_U32 titanHtcap, tANI_U8 channel, tPowerdBm maxTxPower, tANI_U8 peSessionId)
{
#if !defined WLAN_FEATURE_VOWIFI
    tANI_U32 localPwrConstraint;
#endif

    // Setup the CB State appropriately, prior to
    // issuing a dphChannelChange(). This is done
    // so that if CB is enabled, the CB Secondary
    // Channel is setup correctly
    
     /* if local CB admin state is off or join req CB admin state is off, don't bother with CB channel setup */
    PELOG1(limLog(pMac, LOG1, FL("Before : gCbState = 0x%x, gCbmode = %x\n"), pMac->lim.gCbState, pMac->lim.gCbMode);)
    if(GET_CB_ADMIN_STATE(pMac->lim.gCbState) &&
                    SME_GET_CB_ADMIN_STATE(titanHtcap)) {
        PELOG1(limLog(pMac, LOG1, FL("station doing channel bonding\n"));)
        setupCBState( pMac, (tAniCBSecondaryMode)SME_GET_CB_OPER_STATE(titanHtcap));
    }else {
        PELOG1(limLog(pMac, LOG1, FL("station not doing channel bonding\n"));)
        setupCBState(pMac, eANI_CB_SECONDARY_NONE);
    }

    PELOG1(limLog(pMac, LOG1, FL("After :gCbState = 0x%x, gCbmode = %x\n"), pMac->lim.gCbState, pMac->lim.gCbMode);)

    #if 0
    if (wlan_cfgSetInt(pMac, WNI_CFG_CURRENT_CHANNEL, channel) != eSIR_SUCCESS) {
           limLog(pMac, LOGP, FL("could not set CURRENT_CHANNEL at CFG\n"));
           return;
    }
    #endif // TO SUPPORT BT-AMP

#if defined WLAN_FEATURE_VOWIFI  
    limSendSwitchChnlParams( pMac, channel, limGetPhyCBState( pMac ), maxTxPower, peSessionId);
#else
    if (wlan_cfgGetInt(pMac, WNI_CFG_LOCAL_POWER_CONSTRAINT, &localPwrConstraint) != eSIR_SUCCESS) {
           limLog(pMac, LOGP, FL("could not read WNI_CFG_LOCAL_POWER_CONSTRAINT from CFG\n"));
           return;
    }
    // Send WDA_CHNL_SWITCH_IND to HAL
    limSendSwitchChnlParams( pMac, channel, limGetPhyCBState( pMac ), (tPowerdBm)localPwrConstraint, peSessionId);
#endif
   
 }


