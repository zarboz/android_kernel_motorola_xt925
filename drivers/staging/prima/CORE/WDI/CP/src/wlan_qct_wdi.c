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

                       W L A N _ Q C T _ W D I. C

  OVERVIEW:

  This software unit holds the implementation of the WLAN Device Abstraction     
  Layer Interface.

  The functions externalized by this module are to be called by any upper     
  MAC implementation that wishes to use the WLAN Device.

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
10/05/11      hap     Adding support for Keep Alive
2010-08-09    lti     Created module

===========================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include "wlan_qct_wdi.h" 
#include "wlan_qct_wdi_i.h" 
#include "wlan_qct_wdi_sta.h" 
#include "wlan_qct_wdi_dp.h" 

#include "wlan_qct_wdi_cts.h" 

#include "wlan_qct_pal_api.h"
#include "wlan_qct_pal_type.h"
#include "wlan_qct_pal_status.h"
#include "wlan_qct_pal_sync.h"
#include "wlan_qct_pal_msg.h"
#include "wlan_qct_pal_trace.h"
#include "wlan_qct_pal_packet.h"

#include "wlan_qct_wdi_dts.h" 

#include "wlan_hal_msg.h"

#ifdef ANI_MANF_DIAG
#include "pttMsgApi.h"
#endif /* ANI_MANF_DIAG */

/*===========================================================================
   WLAN DAL Control Path Internal Data Definitions and Declarations 
 ===========================================================================*/
#define WDI_SET_POWER_STATE_TIMEOUT  10000 /* in msec a very high upper limit */
#define WDI_WCTS_ACTION_TIMEOUT       2000 /* in msec a very high upper limit */


#ifdef FEATURE_WLAN_SCAN_PNO
#define WDI_PNO_VERSION_MASK 0x8000
#endif

/*-------------------------------------------------------------------------- 
   WLAN DAL  State Machine
 --------------------------------------------------------------------------*/
WPT_STATIC const WDI_MainFsmEntryType wdiMainFSM[WDI_MAX_ST] = 
{
  /*WDI_INIT_ST*/
  {{
    WDI_MainStart,              /*WDI_START_EVENT*/
    NULL,                       /*WDI_STOP_EVENT*/
    WDI_MainReqBusy,            /*WDI_REQUEST_EVENT*/
    WDI_MainRspInit,            /*WDI_RESPONSE_EVENT*/
    WDI_MainClose,              /*WDI_CLOSE_EVENT*/
    WDI_MainShutdown            /*WDI_SHUTDOWN_EVENT*/
  }},

  /*WDI_STARTED_ST*/
  {{
    WDI_MainStartStarted,       /*WDI_START_EVENT*/
    WDI_MainStopStarted,        /*WDI_STOP_EVENT*/
    WDI_MainReqStarted,         /*WDI_REQUEST_EVENT*/
    WDI_MainRsp,                /*WDI_RESPONSE_EVENT*/
    NULL,                       /*WDI_CLOSE_EVENT*/
    WDI_MainShutdown            /*WDI_SHUTDOWN_EVENT*/
  }},

  /*WDI_STOPPED_ST*/
  {{
    WDI_MainStart,              /*WDI_START_EVENT*/
    WDI_MainStopStopped,        /*WDI_STOP_EVENT*/
    NULL,                       /*WDI_REQUEST_EVENT*/
    WDI_MainRsp,                /*WDI_RESPONSE_EVENT*/
    WDI_MainClose,              /*WDI_CLOSE_EVENT*/
    NULL                        /*WDI_SHUTDOWN_EVENT*/
  }},

  /*WDI_BUSY_ST*/
  {{
    WDI_MainStartBusy,          /*WDI_START_EVENT*/
    WDI_MainStopBusy,           /*WDI_STOP_EVENT*/
    WDI_MainReqBusy,            /*WDI_REQUEST_EVENT*/
    WDI_MainRsp,                /*WDI_RESPONSE_EVENT*/
    WDI_MainCloseBusy,          /*WDI_CLOSE_EVENT*/
    WDI_MainShutdownBusy        /*WDI_SHUTDOWN_EVENT*/
  }}
};

/*--------------------------------------------------------------------------- 
  DAL Request Processing Array  - the functions in this table will only be
  called when the processing of the specific request is allowed by the
  Main FSM 
 ---------------------------------------------------------------------------*/
WDI_ReqProcFuncType  pfnReqProcTbl[WDI_MAX_UMAC_IND] = 
{
  /*INIT*/
  WDI_ProcessStartReq,      /* WDI_START_REQ  */
  WDI_ProcessStopReq,       /* WDI_STOP_REQ  */
  WDI_ProcessCloseReq,      /* WDI_CLOSE_REQ  */

  /*SCAN*/
  WDI_ProcessInitScanReq,   /* WDI_INIT_SCAN_REQ  */
  WDI_ProcessStartScanReq,  /* WDI_START_SCAN_REQ  */
  WDI_ProcessEndScanReq,    /* WDI_END_SCAN_REQ  */
  WDI_ProcessFinishScanReq, /* WDI_FINISH_SCAN_REQ  */

  /*ASSOCIATION*/
  WDI_ProcessJoinReq,       /* WDI_JOIN_REQ  */
  WDI_ProcessConfigBSSReq,  /* WDI_CONFIG_BSS_REQ  */
  WDI_ProcessDelBSSReq,     /* WDI_DEL_BSS_REQ  */
  WDI_ProcessPostAssocReq,  /* WDI_POST_ASSOC_REQ  */
  WDI_ProcessDelSTAReq,     /* WDI_DEL_STA_REQ  */

  /* Security */
  WDI_ProcessSetBssKeyReq,        /* WDI_SET_BSS_KEY_REQ  */
  WDI_ProcessRemoveBssKeyReq,     /* WDI_RMV_BSS_KEY_REQ  */
  WDI_ProcessSetStaKeyReq,        /* WDI_SET_STA_KEY_REQ  */
  WDI_ProcessRemoveStaKeyReq,     /* WDI_RMV_BSS_KEY_REQ  */

  /* QoS and BA APIs */
  WDI_ProcessAddTSpecReq,          /* WDI_ADD_TS_REQ  */
  WDI_ProcessDelTSpecReq,          /* WDI_DEL_TS_REQ  */
  WDI_ProcessUpdateEDCAParamsReq,  /* WDI_UPD_EDCA_PRMS_REQ  */
  WDI_ProcessAddBASessionReq,      /* WDI_ADD_BA_SESSION_REQ  */
  WDI_ProcessDelBAReq,             /* WDI_DEL_BA_REQ  */

  /* Miscellaneous Control APIs */
  WDI_ProcessChannelSwitchReq,     /* WDI_CH_SWITCH_REQ  */
  WDI_ProcessConfigStaReq,         /* WDI_CONFIG_STA_REQ  */
  WDI_ProcessSetLinkStateReq,      /* WDI_SET_LINK_ST_REQ  */
  WDI_ProcessGetStatsReq,          /* WDI_GET_STATS_REQ  */
  WDI_ProcessUpdateCfgReq,         /* WDI_UPDATE_CFG_REQ  */

  /*BA APIs*/
  WDI_ProcessAddBAReq,             /* WDI_ADD_BA_REQ  */
  WDI_ProcessTriggerBAReq,         /* WDI_TRIGGER_BA_REQ  */

  /*Beacon processing APIs*/
  WDI_ProcessUpdateBeaconParamsReq, /* WDI_UPD_BCON_PRMS_REQ */
  WDI_ProcessSendBeaconParamsReq, /* WDI_SND_BCON_REQ */

  WDI_ProcessUpdateProbeRspTemplateReq, /* WDI_UPD_PROBE_RSP_TEMPLATE_REQ */
  WDI_ProcessSetStaBcastKeyReq,        /* WDI_SET_STA_BCAST_KEY_REQ  */
  WDI_ProcessRemoveStaBcastKeyReq,     /* WDI_RMV_STA_BCAST_KEY_REQ  */
  WDI_ProcessSetMaxTxPowerReq,         /*WDI_SET_MAX_TX_POWER_REQ*/
#ifdef WLAN_FEATURE_P2P
  WDI_ProcessP2PGONOAReq,              /* WDI_P2P_GO_NOTICE_OF_ABSENCE_REQ */
#else
  NULL,
#endif
  /* PowerSave APIs */
  WDI_ProcessEnterImpsReq,         /* WDI_ENTER_IMPS_REQ  */
  WDI_ProcessExitImpsReq,          /* WDI_EXIT_IMPS_REQ  */
  WDI_ProcessEnterBmpsReq,         /* WDI_ENTER_BMPS_REQ  */
  WDI_ProcessExitBmpsReq,          /* WDI_EXIT_BMPS_REQ  */
  WDI_ProcessEnterUapsdReq,        /* WDI_ENTER_UAPSD_REQ  */
  WDI_ProcessExitUapsdReq,         /* WDI_EXIT_UAPSD_REQ  */
  WDI_ProcessSetUapsdAcParamsReq,  /* WDI_SET_UAPSD_PARAM_REQ  */
  WDI_ProcessUpdateUapsdParamsReq, /* WDI_UPDATE_UAPSD_PARAM_REQ  */
  WDI_ProcessConfigureRxpFilterReq, /* WDI_CONFIGURE_RXP_FILTER_REQ  */
  WDI_ProcessSetBeaconFilterReq,   /* WDI_SET_BEACON_FILTER_REQ  */
  WDI_ProcessRemBeaconFilterReq,   /* WDI_REM_BEACON_FILTER_REQ  */
  WDI_ProcessSetRSSIThresholdsReq, /* WDI_SET_RSSI_THRESHOLDS_REQ  */
  WDI_ProcessHostOffloadReq,       /* WDI_HOST_OFFLOAD_REQ  */
  WDI_ProcessWowlAddBcPtrnReq,     /* WDI_WOWL_ADD_BC_PTRN_REQ  */
  WDI_ProcessWowlDelBcPtrnReq,     /* WDI_WOWL_DEL_BC_PTRN_REQ  */
  WDI_ProcessWowlEnterReq,         /* WDI_WOWL_ENTER_REQ  */
  WDI_ProcessWowlExitReq,          /* WDI_WOWL_EXIT_REQ  */
  WDI_ProcessConfigureAppsCpuWakeupStateReq, /* WDI_CONFIGURE_APPS_CPU_WAKEUP_STATE_REQ  */
  /*NV Download APIs*/
  WDI_ProcessNvDownloadReq,      /* WDI_NV_DOWNLOAD_REQ*/
  WDI_ProcessFlushAcReq,           /* WDI_FLUSH_AC_REQ  */
  WDI_ProcessBtAmpEventReq,        /* WDI_BTAMP_EVENT_REQ  */
#ifdef WLAN_FEATURE_VOWIFI_11R
  WDI_ProcessAggrAddTSpecReq,      /* WDI_AGGR_ADD_TS_REQ */
#else
  NULL,
#endif /* WLAN_FEATURE_VOWIFI_11R */
  WDI_ProcessAddSTASelfReq,         /* WDI_ADD_STA_SELF_REQ */
  WDI_ProcessDelSTASelfReq,          /* WDI DEL STA SELF REQ */
#ifdef ANI_MANF_DIAG
  WDI_ProcessFTMCommandReq,            /* WDI_FTM_CMD_REQ */
#else
  NULL,
#endif /* ANI_MANF_DIAG */
  
  NULL,
  WDI_ProcessHostResumeReq,            /*WDI_HOST_RESUME_REQ*/
  
  WDI_ProcessKeepAliveReq,       /* WDI_KEEP_ALIVE_REQ */    

#ifdef FEATURE_WLAN_SCAN_PNO
  WDI_ProcessSetPreferredNetworkReq,  /* WDI_SET_PREF_NETWORK_REQ */
  WDI_ProcessSetRssiFilterReq,        /* WDI_SET_RSSI_FILTER_REQ */
  WDI_ProcessUpdateScanParamsReq,     /* WDI_UPDATE_SCAN_PARAMS_REQ */
#else
  NULL,
  NULL,
  NULL,
#endif /* FEATURE_WLAN_SCAN_PNO */

  WDI_ProcessSetTxPerTrackingReq,     /* WDI_SET_TX_PER_TRACKING_REQ  */
  
#ifdef WLAN_FEATURE_PACKET_FILTERING
  /* WDI_8023_MULTICAST_LIST_REQ */
  WDI_Process8023MulticastListReq,          
  /* WDI_RECEIVE_FILTER_SET_FILTER_REQ */
  WDI_ProcessReceiveFilterSetFilterReq,     
  /* WDI_PACKET_COALESCING_FILTER_MATCH_COUNT_REQ */
  WDI_ProcessFilterMatchCountReq,    
  /* WDI_RECEIVE_FILTER_CLEAR_FILTER_REQ */
  WDI_ProcessReceiveFilterClearFilterReq,   
#else
  NULL,
  NULL,
  NULL,
  NULL,
#endif // WLAN_FEATURE_PACKET_FILTERING
  WDI_ProcessInitScanReq,               /* WDI_INIT_SCAN_CON_REQ */ 
  WDI_ProcessHALDumpCmdReq,             /*WDI_HAL_DUMP_CMD_REQ */
  WDI_ProcessShutdownReq,               /* WDI_SHUTDOWN_REQ  */

  WDI_ProcessSetPowerParamsReq,         /*WDI_SET_POWER_PARAMS_REQ*/
  /*-------------------------------------------------------------------------
    Indications
  -------------------------------------------------------------------------*/
  WDI_ProcessHostSuspendInd,            /* WDI_HOST_SUSPEND_IND*/

};


/*--------------------------------------------------------------------------- 
  DAL Request Processing Array  - the functions in this table will only be
  called when the processing of the specific request is allowed by the
  Main FSM 
 ---------------------------------------------------------------------------*/
WDI_RspProcFuncType  pfnRspProcTbl[WDI_MAX_RESP] = 
{
  /*INIT*/
  WDI_ProcessStartRsp,            /* WDI_START_RESP  */
  WDI_ProcessStopRsp,             /* WDI_STOP_RESP  */
  WDI_ProcessCloseRsp,            /* WDI_CLOSE_RESP  */

  /*SCAN*/
  WDI_ProcessInitScanRsp,         /* WDI_INIT_SCAN_RESP  */
  WDI_ProcessStartScanRsp,        /* WDI_START_SCAN_RESP  */
  WDI_ProcessEndScanRsp,          /* WDI_END_SCAN_RESP  */
  WDI_ProcessFinishScanRsp,       /* WDI_FINISH_SCAN_RESP  */

  /* ASSOCIATION*/
  WDI_ProcessJoinRsp,             /* WDI_JOIN_RESP  */
  WDI_ProcessConfigBSSRsp,        /* WDI_CONFIG_BSS_RESP  */
  WDI_ProcessDelBSSRsp,           /* WDI_DEL_BSS_RESP  */
  WDI_ProcessPostAssocRsp,        /* WDI_POST_ASSOC_RESP  */
  WDI_ProcessDelSTARsp,           /* WDI_DEL_STA_RESP  */

  /* Security */
  WDI_ProcessSetBssKeyRsp,        /* WDI_SET_BSS_KEY_RESP  */
  WDI_ProcessRemoveBssKeyRsp,     /* WDI_RMV_BSS_KEY_RESP  */
  WDI_ProcessSetStaKeyRsp,        /* WDI_SET_STA_KEY_RESP  */
  WDI_ProcessRemoveStaKeyRsp,     /* WDI_RMV_BSS_KEY_RESP  */

  /* QoS and BA APIs */
  WDI_ProcessAddTSpecRsp,          /* WDI_ADD_TS_RESP  */
  WDI_ProcessDelTSpecRsp,          /* WDI_DEL_TS_RESP  */
  WDI_ProcessUpdateEDCAParamsRsp,  /* WDI_UPD_EDCA_PRMS_RESP  */
  WDI_ProcessAddBASessionRsp,      /* WDI_ADD_BA_SESSION_RESP  */
  WDI_ProcessDelBARsp,             /* WDI_DEL_BA_RESP  */

  /* Miscellaneous Control APIs */
  WDI_ProcessChannelSwitchRsp,     /* WDI_CH_SWITCH_RESP  */
  WDI_ProcessConfigStaRsp,         /* WDI_CONFIG_STA_RESP  */
  WDI_ProcessSetLinkStateRsp,      /* WDI_SET_LINK_ST_RESP  */
  WDI_ProcessGetStatsRsp,          /* WDI_GET_STATS_RESP  */
  WDI_ProcessUpdateCfgRsp,         /* WDI_UPDATE_CFG_RESP  */

  /* BA APIs*/
  WDI_ProcessAddBARsp,             /* WDI_ADD_BA_RESP  */
  WDI_ProcessTriggerBARsp,         /* WDI_TRIGGER_BA_RESP  */
  
  /* IBSS APIs*/
  WDI_ProcessUpdateBeaconParamsRsp, /* WDI_UPD_BCON_PRMS_RSP */
  WDI_ProcessSendBeaconParamsRsp,   /* WDI_SND_BCON_RSP */

  /*Soft AP APIs*/
  WDI_ProcessUpdateProbeRspTemplateRsp,/*WDI_UPD_PROBE_RSP_TEMPLATE_RESP */
  WDI_ProcessSetStaBcastKeyRsp,        /*WDI_SET_STA_BCAST_KEY_RESP */
  WDI_ProcessRemoveStaBcastKeyRsp,     /*WDI_RMV_STA_BCAST_KEY_RESP */
  WDI_ProcessSetMaxTxPowerRsp,         /*WDI_SET_MAX_TX_POWER_RESP */

  /* PowerSave APIs */
  WDI_ProcessEnterImpsRsp,         /* WDI_ENTER_IMPS_RESP  */
  WDI_ProcessExitImpsRsp,          /* WDI_EXIT_IMPS_RESP  */
  WDI_ProcessEnterBmpsRsp,         /* WDI_ENTER_BMPS_RESP  */
  WDI_ProcessExitBmpsRsp,          /* WDI_EXIT_BMPS_RESP  */
  WDI_ProcessEnterUapsdRsp,        /* WDI_ENTER_UAPSD_RESP  */
  WDI_ProcessExitUapsdRsp,         /* WDI_EXIT_UAPSD_RESP  */
  WDI_ProcessSetUapsdAcParamsRsp,  /* WDI_SET_UAPSD_PARAM_RESP  */
  WDI_ProcessUpdateUapsdParamsRsp, /* WDI_UPDATE_UAPSD_PARAM_RESP  */
  WDI_ProcessConfigureRxpFilterRsp,/* WDI_CONFIGURE_RXP_FILTER_RESP  */
  WDI_ProcessSetBeaconFilterRsp,   /* WDI_SET_BEACON_FILTER_RESP  */
  WDI_ProcessRemBeaconFilterRsp,   /* WDI_REM_BEACON_FILTER_RESP  */
  WDI_ProcessSetRSSIThresoldsRsp,  /* WDI_SET_RSSI_THRESHOLDS_RESP  */
  WDI_ProcessHostOffloadRsp,       /* WDI_HOST_OFFLOAD_RESP  */
  WDI_ProcessWowlAddBcPtrnRsp,     /* WDI_WOWL_ADD_BC_PTRN_RESP  */
  WDI_ProcessWowlDelBcPtrnRsp,     /* WDI_WOWL_DEL_BC_PTRN_RESP  */
  WDI_ProcessWowlEnterRsp,         /* WDI_WOWL_ENTER_RESP  */
  WDI_ProcessWowlExitRsp,          /* WDI_WOWL_EXIT_RESP  */
  WDI_ProcessConfigureAppsCpuWakeupStateRsp, /* WDI_CONFIGURE_APPS_CPU_WAKEUP_STATE_RESP  */
  

  WDI_ProcessNvDownloadRsp, /* WDI_NV_DOWNLOAD_RESP*/

  WDI_ProcessFlushAcRsp,           /* WDI_FLUSH_AC_RESP  */
  WDI_ProcessBtAmpEventRsp,        /* WDI_BTAMP_EVENT_RESP  */
#ifdef WLAN_FEATURE_VOWIFI_11R
  WDI_ProcessAggrAddTSpecRsp,       /* WDI_ADD_TS_RESP  */
#else
  NULL,
#endif /* WLAN_FEATURE_VOWIFI_11R */
  WDI_ProcessAddSTASelfRsp,          /* WDI_ADD_STA_SELF_RESP */
  WDI_ProcessDelSTASelfRsp,          /* WDI_DEL_STA_SELF_RESP */
  NULL,
  WDI_ProcessHostResumeRsp,        /*WDI_HOST_RESUME_RESP*/

#ifdef WLAN_FEATURE_P2P
  WDI_ProcessP2PGONOARsp,           /*WDI_P2P_GO_NOTICE_OF_ABSENCE_RESP */
#else
  NULL,
#endif

#ifdef ANI_MANF_DIAG
  WDI_ProcessFTMCommandRsp,         /* WDI_FTM_CMD_RESP */
#else
  NULL,
#endif /* ANI_MANF_DIAG */

  WDI_ProcessKeepAliveRsp,       /* WDI_KEEP_ALIVE_RESP  */  
  
#ifdef FEATURE_WLAN_SCAN_PNO
  WDI_ProcessSetPreferredNetworkRsp,     /* WDI_SET_PREF_NETWORK_RESP */
  WDI_ProcessSetRssiFilterRsp,           /* WDI_SET_RSSI_FILTER_RESP */
  WDI_ProcessUpdateScanParamsRsp,        /* WDI_UPDATE_SCAN_PARAMS_RESP */
#else
  NULL,
  NULL,
  NULL,
#endif // FEATURE_WLAN_SCAN_PNO

  WDI_ProcessSetTxPerTrackingRsp,      /* WDI_SET_TX_PER_TRACKING_RESP  */
  
#ifdef WLAN_FEATURE_PACKET_FILTERING
  /* WDI_8023_MULTICAST_LIST_RESP */
  WDI_Process8023MulticastListRsp,          
  /* WDI_RECEIVE_FILTER_SET_FILTER_RESP */
  WDI_ProcessReceiveFilterSetFilterRsp,     
  /* WDI_PACKET_COALESCING_FILTER_MATCH_COUNT_RESP */
  WDI_ProcessFilterMatchCountRsp,   
  /* WDI_RECEIVE_FILTER_CLEAR_FILTER_RESP */
  WDI_ProcessReceiveFilterClearFilterRsp,   
#else
  NULL,
  NULL,
  NULL,
  NULL,
#endif // WLAN_FEATURE_PACKET_FILTERING

  WDI_ProcessHALDumpCmdRsp,       /* WDI_HAL_DUMP_CMD_RESP */
  WDI_ProcessShutdownRsp,         /* WDI_SHUTDOWN_RESP */
  WDI_ProcessSetPowerParamsRsp,         /*WDI_SET_POWER_PARAMS_RESP*/
  /*---------------------------------------------------------------------
    Indications
  ---------------------------------------------------------------------*/
  WDI_ProcessLowRSSIInd,            /* WDI_HAL_RSSI_NOTIFICATION_IND  */
  WDI_ProcessMissedBeaconInd,       /* WDI_HAL_MISSED_BEACON_IND  */
  WDI_ProcessUnkAddrFrameInd,       /* WDI_HAL_UNKNOWN_ADDR2_FRAME_RX_IND  */
  WDI_ProcessMicFailureInd,         /* WDI_HAL_MIC_FAILURE_IND  */
  WDI_ProcessFatalErrorInd,         /* WDI_HAL_FATAL_ERROR_IND  */
  WDI_ProcessDelSTAInd,             /* WDI_HAL_DEL_STA_IND  */

  WDI_ProcessCoexInd,               /* WDI_HAL_COEX_IND  */

  WDI_ProcessTxCompleteInd,         /* WDI_HAL_TX_COMPLETE_IND  */

#ifdef WLAN_FEATURE_P2P
  WDI_ProcessP2pNoaAttrInd,         /*WDI_HOST_NOA_ATTR_IND*/
#else
  NULL,
#endif

#ifdef FEATURE_WLAN_SCAN_PNO
  WDI_ProcessPrefNetworkFoundInd,   /* WDI_HAL_PREF_NETWORK_FOUND_IND */
#else
  NULL,
#endif // FEATURE_WLAN_SCAN_PNO
  
  WDI_ProcessTxPerHitInd,               /* WDI_HAL_TX_PER_HIT_IND  */
};


/*--------------------------------------------------------------------------- 
  WLAN DAL Global Control Block
 ---------------------------------------------------------------------------*/
WDI_ControlBlockType  gWDICb; 
static wpt_uint8      gWDIInitialized = eWLAN_PAL_FALSE;

const wpt_uint8 szTransportChName[] = "WLAN_CTRL"; 

/*Helper routine for retrieving the PAL Context from WDI*/
WPT_INLINE 
void* WDI_GET_PAL_CTX( void )
{
  return gWDICb.pPALContext; 
}/*WDI_GET_PAL_CTX*/

/*============================================================================ 
  Helper inline converters
 ============================================================================*/
/*Convert WDI driver type into HAL driver type*/
WPT_STATIC WPT_INLINE WDI_Status
WDI_HAL_2_WDI_STATUS
(
  eHalStatus halStatus
);

/*Convert WDI request type into HAL request type*/
WPT_STATIC WPT_INLINE tHalHostMsgType
WDI_2_HAL_REQ_TYPE
(
  WDI_RequestEnumType    wdiReqType
);

/*Convert WDI response type into HAL response type*/
WPT_STATIC WPT_INLINE WDI_ResponseEnumType
HAL_2_WDI_RSP_TYPE
(
  tHalHostMsgType halMsg
);

/*Convert WDI driver type into HAL driver type*/
WPT_STATIC WPT_INLINE tDriverType
WDI_2_HAL_DRV_TYPE
(
  WDI_DriverType wdiDriverType
);

/*Convert WDI stop reason into HAL stop reason*/
WPT_STATIC WPT_INLINE tHalStopType
WDI_2_HAL_STOP_REASON
(
  WDI_StopType wdiStopType
);

/*Convert WDI scan mode type into HAL scan mode type*/
WPT_STATIC WPT_INLINE eHalSysMode
WDI_2_HAL_SCAN_MODE
(
  WDI_ScanMode wdiScanMode
);

/*Convert WDI sec ch offset into HAL sec ch offset type*/
WPT_STATIC WPT_INLINE tSirMacHTSecondaryChannelOffset
WDI_2_HAL_SEC_CH_OFFSET
(
  WDI_HTSecondaryChannelOffset wdiSecChOffset
);

/*Convert WDI BSS type into HAL BSS type*/
WPT_STATIC WPT_INLINE tSirBssType
WDI_2_HAL_BSS_TYPE
(
  WDI_BssType wdiBSSType
);

/*Convert WDI NW type into HAL NW type*/
WPT_STATIC WPT_INLINE tSirNwType
WDI_2_HAL_NW_TYPE
(
  WDI_NwType wdiNWType
);

/*Convert WDI chanel bonding type into HAL cb type*/
WPT_STATIC WPT_INLINE ePhyChanBondState
WDI_2_HAL_CB_STATE
(
  WDI_PhyChanBondState wdiCbState
);

/*Convert WDI chanel bonding type into HAL cb type*/
WPT_STATIC WPT_INLINE tSirMacHTOperatingMode
WDI_2_HAL_HT_OPER_MODE
(
  WDI_HTOperatingMode wdiHTOperMode
);

/*Convert WDI mimo PS type into HAL mimo PS type*/
WPT_STATIC WPT_INLINE tSirMacHTMIMOPowerSaveState
WDI_2_HAL_MIMO_PS
(
  WDI_HTMIMOPowerSaveState wdiHTOperMode
);

/*Convert WDI ENC type into HAL ENC type*/
WPT_STATIC WPT_INLINE tAniEdType
WDI_2_HAL_ENC_TYPE
(
  WDI_EncryptType wdiEncType
);

/*Convert WDI WEP type into HAL WEP type*/
WPT_STATIC WPT_INLINE tAniWepType
WDI_2_HAL_WEP_TYPE
(
  WDI_WepType  wdiWEPType
);

/*Convert WDI Link State into HAL Link State*/
WPT_STATIC WPT_INLINE tSirLinkState
WDI_2_HAL_LINK_STATE
(
  WDI_LinkStateType  wdiLinkState
);

/*Translate a STA Context from WDI into HAL*/ 
WPT_STATIC WPT_INLINE 
void
WDI_CopyWDIStaCtxToHALStaCtx
( 
  tConfigStaParams*          phalConfigSta,
  WDI_ConfigStaReqInfoType*  pwdiConfigSta
);
 
/*Translate a Rate set info from WDI into HAL*/ 
WPT_STATIC WPT_INLINE void 
WDI_CopyWDIRateSetToHALRateSet
( 
  tSirMacRateSet* pHalRateSet,
  WDI_RateSet*    pwdiRateSet
);

/*Translate an EDCA Parameter Record from WDI into HAL*/
WPT_STATIC WPT_INLINE void
WDI_CopyWDIEDCAParamsToHALEDCAParams
( 
  tSirMacEdcaParamRecord* phalEdcaParam,
  WDI_EdcaParamRecord*    pWDIEdcaParam
);

/*Copy a management frame header from WDI fmt into HAL fmt*/
WPT_STATIC WPT_INLINE void
WDI_CopyWDIMgmFrameHdrToHALMgmFrameHdr
(
  tSirMacMgmtHdr* pmacMgmtHdr,
  WDI_MacMgmtHdr* pwdiMacMgmtHdr
);

/*Copy config bss parameters from WDI fmt into HAL fmt*/
WPT_STATIC WPT_INLINE void
WDI_CopyWDIConfigBSSToHALConfigBSS
(
  tConfigBssParams*         phalConfigBSS,
  WDI_ConfigBSSReqInfoType* pwdiConfigBSS
);

/*Extract the request CB function and user data from a request structure 
  pointed to by user data */
WPT_STATIC WPT_INLINE void
WDI_ExtractRequestCBFromEvent
(
  WDI_EventInfoType* pEvent,
  WDI_ReqStatusCb*   ppfnReqCB, 
  void**             ppUserData
);

wpt_uint8
WDI_FindEmptySession
( 
  WDI_ControlBlockType*   pWDICtx,
  WDI_BSSSessionType**    ppSession
);

void
WDI_AddBcastSTAtoSTATable
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_AddStaParams *     staParams,
  wpt_uint16             usBcastStaIdx
);

WDI_Status WDI_SendNvBlobReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
);

void
WDI_SetPowerStateCb
(
   wpt_status status,
   unsigned int dxePhyAddr,
   void      *pContext
);

#define CASE_RETURN_STRING( str )           \
    case ( ( str ) ): return( #str ); break \

/**
 @brief WDI_getReqMsgString prints the WDI request message in string.
  
 @param wdiReqMsgId: WDI Message request Id 
  
 @see 
 @return Result of the function call
*/
static char *WDI_getReqMsgString(wpt_uint16 wdiReqMsgId)
{
  switch (wdiReqMsgId)
  {
    CASE_RETURN_STRING( WDI_START_REQ );
    CASE_RETURN_STRING( WDI_STOP_REQ );
    CASE_RETURN_STRING( WDI_CLOSE_REQ );
    CASE_RETURN_STRING( WDI_INIT_SCAN_REQ );
    CASE_RETURN_STRING( WDI_START_SCAN_REQ );
    CASE_RETURN_STRING( WDI_END_SCAN_REQ );
    CASE_RETURN_STRING( WDI_FINISH_SCAN_REQ );
    CASE_RETURN_STRING( WDI_JOIN_REQ );
    CASE_RETURN_STRING( WDI_CONFIG_BSS_REQ );
    CASE_RETURN_STRING( WDI_DEL_BSS_REQ );
    CASE_RETURN_STRING( WDI_POST_ASSOC_REQ );
    CASE_RETURN_STRING( WDI_DEL_STA_REQ );
    CASE_RETURN_STRING( WDI_SET_BSS_KEY_REQ );
    CASE_RETURN_STRING( WDI_RMV_BSS_KEY_REQ );
    CASE_RETURN_STRING( WDI_SET_STA_KEY_REQ );
    CASE_RETURN_STRING( WDI_RMV_STA_KEY_REQ );
    CASE_RETURN_STRING( WDI_ADD_TS_REQ );
    CASE_RETURN_STRING( WDI_DEL_TS_REQ );
    CASE_RETURN_STRING( WDI_UPD_EDCA_PRMS_REQ );
    CASE_RETURN_STRING( WDI_ADD_BA_SESSION_REQ );
    CASE_RETURN_STRING( WDI_DEL_BA_REQ );
    CASE_RETURN_STRING( WDI_CH_SWITCH_REQ );
    CASE_RETURN_STRING( WDI_CONFIG_STA_REQ );
    CASE_RETURN_STRING( WDI_SET_LINK_ST_REQ );
    CASE_RETURN_STRING( WDI_GET_STATS_REQ );
    CASE_RETURN_STRING( WDI_UPDATE_CFG_REQ );
    CASE_RETURN_STRING( WDI_ADD_BA_REQ );
    CASE_RETURN_STRING( WDI_TRIGGER_BA_REQ );
    CASE_RETURN_STRING( WDI_UPD_BCON_PRMS_REQ );
    CASE_RETURN_STRING( WDI_SND_BCON_REQ );
    CASE_RETURN_STRING( WDI_UPD_PROBE_RSP_TEMPLATE_REQ );
    CASE_RETURN_STRING( WDI_SET_STA_BCAST_KEY_REQ );
    CASE_RETURN_STRING( WDI_RMV_STA_BCAST_KEY_REQ );
    CASE_RETURN_STRING( WDI_SET_MAX_TX_POWER_REQ );
    CASE_RETURN_STRING( WDI_P2P_GO_NOTICE_OF_ABSENCE_REQ );
    CASE_RETURN_STRING( WDI_ENTER_IMPS_REQ );
    CASE_RETURN_STRING( WDI_EXIT_IMPS_REQ );
    CASE_RETURN_STRING( WDI_ENTER_BMPS_REQ );
    CASE_RETURN_STRING( WDI_EXIT_BMPS_REQ );
    CASE_RETURN_STRING( WDI_ENTER_UAPSD_REQ );
    CASE_RETURN_STRING( WDI_EXIT_UAPSD_REQ );
    CASE_RETURN_STRING( WDI_SET_UAPSD_PARAM_REQ );
    CASE_RETURN_STRING( WDI_UPDATE_UAPSD_PARAM_REQ );
    CASE_RETURN_STRING( WDI_CONFIGURE_RXP_FILTER_REQ );
    CASE_RETURN_STRING( WDI_SET_BEACON_FILTER_REQ);
    CASE_RETURN_STRING( WDI_REM_BEACON_FILTER_REQ );
    CASE_RETURN_STRING( WDI_SET_RSSI_THRESHOLDS_REQ );
    CASE_RETURN_STRING( WDI_HOST_OFFLOAD_REQ );
    CASE_RETURN_STRING( WDI_WOWL_ADD_BC_PTRN_REQ );
    CASE_RETURN_STRING( WDI_WOWL_DEL_BC_PTRN_REQ );
    CASE_RETURN_STRING( WDI_WOWL_ENTER_REQ );
    CASE_RETURN_STRING( WDI_WOWL_EXIT_REQ );
    CASE_RETURN_STRING( WDI_CONFIGURE_APPS_CPU_WAKEUP_STATE_REQ );
    CASE_RETURN_STRING( WDI_NV_DOWNLOAD_REQ );
    CASE_RETURN_STRING( WDI_FLUSH_AC_REQ );
    CASE_RETURN_STRING( WDI_BTAMP_EVENT_REQ );
    CASE_RETURN_STRING( WDI_AGGR_ADD_TS_REQ );
    CASE_RETURN_STRING( WDI_ADD_STA_SELF_REQ );
    CASE_RETURN_STRING( WDI_DEL_STA_SELF_REQ );
    CASE_RETURN_STRING( WDI_FTM_CMD_REQ );
    CASE_RETURN_STRING( WDI_START_INNAV_MEAS_REQ );
    CASE_RETURN_STRING( WDI_HOST_RESUME_REQ );
    CASE_RETURN_STRING( WDI_KEEP_ALIVE_REQ);
  #ifdef FEATURE_WLAN_SCAN_PNO
    CASE_RETURN_STRING( WDI_SET_PREF_NETWORK_REQ );
    CASE_RETURN_STRING( WDI_SET_RSSI_FILTER_REQ );
    CASE_RETURN_STRING( WDI_UPDATE_SCAN_PARAMS_REQ );
  #endif
    CASE_RETURN_STRING( WDI_SET_TX_PER_TRACKING_REQ );
    CASE_RETURN_STRING( WDI_8023_MULTICAST_LIST_REQ );
    CASE_RETURN_STRING( WDI_RECEIVE_FILTER_SET_FILTER_REQ );
    CASE_RETURN_STRING( WDI_PACKET_COALESCING_FILTER_MATCH_COUNT_REQ );
    CASE_RETURN_STRING( WDI_RECEIVE_FILTER_CLEAR_FILTER_REQ );
    CASE_RETURN_STRING( WDI_INIT_SCAN_CON_REQ );
    CASE_RETURN_STRING( WDI_HAL_DUMP_CMD_REQ );
    CASE_RETURN_STRING( WDI_SHUTDOWN_REQ );
    CASE_RETURN_STRING( WDI_SET_POWER_PARAMS_REQ );
    default:
        return "Unknown WDI MessageId";
  }
}



/**
 @brief WDI_getRespMsgString prints the WDI resonse message in string.
  
 @param wdiRespMsgId: WDI Message response Id 
  
 @see 
 @return Result of the function call
*/
static char *WDI_getRespMsgString(wpt_uint16 wdiRespMsgId)
{
  switch (wdiRespMsgId)
  {
    CASE_RETURN_STRING( WDI_START_RESP );
    CASE_RETURN_STRING( WDI_STOP_RESP );
    CASE_RETURN_STRING( WDI_CLOSE_RESP );
    CASE_RETURN_STRING( WDI_INIT_SCAN_RESP );
    CASE_RETURN_STRING( WDI_START_SCAN_RESP );
    CASE_RETURN_STRING( WDI_END_SCAN_RESP );
    CASE_RETURN_STRING( WDI_FINISH_SCAN_RESP );
    CASE_RETURN_STRING( WDI_JOIN_RESP );
    CASE_RETURN_STRING( WDI_CONFIG_BSS_RESP );
    CASE_RETURN_STRING( WDI_DEL_BSS_RESP );
    CASE_RETURN_STRING( WDI_POST_ASSOC_RESP );
    CASE_RETURN_STRING( WDI_DEL_STA_RESP );
    CASE_RETURN_STRING( WDI_SET_BSS_KEY_RESP );
    CASE_RETURN_STRING( WDI_RMV_BSS_KEY_RESP );
    CASE_RETURN_STRING( WDI_SET_STA_KEY_RESP );
    CASE_RETURN_STRING( WDI_RMV_STA_KEY_RESP );
    CASE_RETURN_STRING( WDI_ADD_TS_RESP );
    CASE_RETURN_STRING( WDI_DEL_TS_RESP );
    CASE_RETURN_STRING( WDI_UPD_EDCA_PRMS_RESP );
    CASE_RETURN_STRING( WDI_ADD_BA_SESSION_RESP );
    CASE_RETURN_STRING( WDI_DEL_BA_RESP );
    CASE_RETURN_STRING( WDI_CH_SWITCH_RESP );
    CASE_RETURN_STRING( WDI_CONFIG_STA_RESP );
    CASE_RETURN_STRING( WDI_SET_LINK_ST_RESP );
    CASE_RETURN_STRING( WDI_GET_STATS_RESP );
    CASE_RETURN_STRING( WDI_UPDATE_CFG_RESP );
    CASE_RETURN_STRING( WDI_ADD_BA_RESP );
    CASE_RETURN_STRING( WDI_TRIGGER_BA_RESP );
    CASE_RETURN_STRING( WDI_UPD_BCON_PRMS_RESP );
    CASE_RETURN_STRING( WDI_SND_BCON_RESP );
    CASE_RETURN_STRING( WDI_UPD_PROBE_RSP_TEMPLATE_RESP );
    CASE_RETURN_STRING( WDI_SET_STA_BCAST_KEY_RESP );
    CASE_RETURN_STRING( WDI_RMV_STA_BCAST_KEY_RESP );
    CASE_RETURN_STRING( WDI_SET_MAX_TX_POWER_RESP );
    CASE_RETURN_STRING( WDI_P2P_GO_NOTICE_OF_ABSENCE_RESP );
    CASE_RETURN_STRING( WDI_ENTER_IMPS_RESP );
    CASE_RETURN_STRING( WDI_EXIT_IMPS_RESP );
    CASE_RETURN_STRING( WDI_ENTER_BMPS_RESP );
    CASE_RETURN_STRING( WDI_EXIT_BMPS_RESP );
    CASE_RETURN_STRING( WDI_ENTER_UAPSD_RESP );
    CASE_RETURN_STRING( WDI_EXIT_UAPSD_RESP );
    CASE_RETURN_STRING( WDI_SET_UAPSD_PARAM_RESP );
    CASE_RETURN_STRING( WDI_UPDATE_UAPSD_PARAM_RESP );
    CASE_RETURN_STRING( WDI_CONFIGURE_RXP_FILTER_RESP );
    CASE_RETURN_STRING( WDI_SET_BEACON_FILTER_RESP);
    CASE_RETURN_STRING( WDI_REM_BEACON_FILTER_RESP );
    CASE_RETURN_STRING( WDI_SET_RSSI_THRESHOLDS_RESP );
    CASE_RETURN_STRING( WDI_HOST_OFFLOAD_RESP );
    CASE_RETURN_STRING( WDI_WOWL_ADD_BC_PTRN_RESP );
    CASE_RETURN_STRING( WDI_WOWL_DEL_BC_PTRN_RESP );
    CASE_RETURN_STRING( WDI_WOWL_ENTER_RESP );
    CASE_RETURN_STRING( WDI_WOWL_EXIT_RESP );
    CASE_RETURN_STRING( WDI_CONFIGURE_APPS_CPU_WAKEUP_STATE_RESP );
    CASE_RETURN_STRING( WDI_NV_DOWNLOAD_RESP );
    CASE_RETURN_STRING( WDI_FLUSH_AC_RESP );
    CASE_RETURN_STRING( WDI_BTAMP_EVENT_RESP );
    CASE_RETURN_STRING( WDI_AGGR_ADD_TS_RESP );
    CASE_RETURN_STRING( WDI_ADD_STA_SELF_RESP );
    CASE_RETURN_STRING( WDI_DEL_STA_SELF_RESP );
    CASE_RETURN_STRING( WDI_FTM_CMD_RESP );
    CASE_RETURN_STRING( WDI_START_INNAV_MEAS_RESP );
    CASE_RETURN_STRING( WDI_HOST_RESUME_RESP );
    CASE_RETURN_STRING( WDI_KEEP_ALIVE_RESP);
  #ifdef FEATURE_WLAN_SCAN_PNO
    CASE_RETURN_STRING( WDI_SET_PREF_NETWORK_RESP );
    CASE_RETURN_STRING( WDI_SET_RSSI_FILTER_RESP );
    CASE_RETURN_STRING( WDI_UPDATE_SCAN_PARAMS_RESP );
  #endif
    CASE_RETURN_STRING( WDI_SET_TX_PER_TRACKING_RESP );
    CASE_RETURN_STRING( WDI_8023_MULTICAST_LIST_RESP );
    CASE_RETURN_STRING( WDI_RECEIVE_FILTER_SET_FILTER_RESP );
    CASE_RETURN_STRING( WDI_PACKET_COALESCING_FILTER_MATCH_COUNT_RESP );
    CASE_RETURN_STRING( WDI_RECEIVE_FILTER_CLEAR_FILTER_RESP );
    CASE_RETURN_STRING( WDI_HAL_DUMP_CMD_RESP );
    CASE_RETURN_STRING( WDI_SHUTDOWN_RESP );
    CASE_RETURN_STRING( WDI_SET_POWER_PARAMS_RESP );
    default:
        return "Unknown WDI MessageId";
  }
}

/**
 @brief WDI_getHALStatusMsgString prints the HAL status in string.
  
 @param halStatusId: HAL status Id 
  
 @see 
 @return Result of the function call
*/
static char *WDI_getHALStatusMsgString(wpt_uint16 halStatusId)
{
  switch (halStatusId)
  {
    CASE_RETURN_STRING( eHAL_STATUS_SUCCESS );
    CASE_RETURN_STRING( PAL_STATUS_INVAL );
    CASE_RETURN_STRING( PAL_STATUS_ALREADY );
    CASE_RETURN_STRING( PAL_STATUS_EMPTY );
    CASE_RETURN_STRING( PAL_STATUS_FAILURE );
    CASE_RETURN_STRING( eHAL_STATUS_FAILURE );
    CASE_RETURN_STRING( eHAL_STATUS_INVALID_PARAMETER );
    CASE_RETURN_STRING( eHAL_STATUS_INVALID_STAIDX );
    CASE_RETURN_STRING( eHAL_STATUS_DPU_DESCRIPTOR_TABLE_FULL );
    CASE_RETURN_STRING( eHAL_STATUS_NO_INTERRUPTS );
    CASE_RETURN_STRING( eHAL_STATUS_INTERRUPT_PRESENT );
    CASE_RETURN_STRING( eHAL_STATUS_STA_TABLE_FULL );
    CASE_RETURN_STRING( eHAL_STATUS_DUPLICATE_STA );
    CASE_RETURN_STRING( eHAL_STATUS_BSSID_INVALID );
    CASE_RETURN_STRING( eHAL_STATUS_STA_INVALID );
    CASE_RETURN_STRING( eHAL_STATUS_DUPLICATE_BSSID );
    CASE_RETURN_STRING( eHAL_STATUS_INVALID_BSSIDX );
    CASE_RETURN_STRING( eHAL_STATUS_BSSID_TABLE_FULL );
    CASE_RETURN_STRING( eHAL_STATUS_INVALID_SIGNATURE );
    CASE_RETURN_STRING( eHAL_STATUS_INVALID_KEYID );
    CASE_RETURN_STRING( eHAL_STATUS_SET_CHAN_ALREADY_ON_REQUESTED_CHAN );
    CASE_RETURN_STRING( eHAL_STATUS_UMA_DESCRIPTOR_TABLE_FULL );
    CASE_RETURN_STRING( eHAL_STATUS_DPU_MICKEY_TABLE_FULL );
    CASE_RETURN_STRING( eHAL_STATUS_BA_RX_BUFFERS_FULL );
    CASE_RETURN_STRING( eHAL_STATUS_BA_RX_MAX_SESSIONS_REACHED );
    CASE_RETURN_STRING( eHAL_STATUS_BA_RX_INVALID_SESSION_ID );
    CASE_RETURN_STRING( eHAL_STATUS_TIMER_START_FAILED );
    CASE_RETURN_STRING( eHAL_STATUS_TIMER_STOP_FAILED );
    CASE_RETURN_STRING( eHAL_STATUS_FAILED_ALLOC );
    CASE_RETURN_STRING( eHAL_STATUS_NOTIFY_BSS_FAIL );
    CASE_RETURN_STRING( eHAL_STATUS_DEL_STA_SELF_IGNORED_REF_COUNT_NOT_ZERO );
    CASE_RETURN_STRING( eHAL_STATUS_ADD_STA_SELF_IGNORED_REF_COUNT_NOT_ZERO );
    CASE_RETURN_STRING( eHAL_STATUS_FW_SEND_MSG_FAILED );
    default:
        return "Unknown HAL status";
  }
}

/*======================================================================== 
 
                             INITIALIZATION APIs
 
==========================================================================*/

/**
 @brief WDI_Init is used to initialize the DAL.
 
 DAL will allocate all the resources it needs. It will open PAL, it will also
 open both the data and the control transport which in their turn will open
 DXE/SMD or any other drivers that they need. 
 
 @param pOSContext: pointer to the OS context provided by the UMAC
                    will be passed on to PAL on Open
        ppWDIGlobalCtx: output pointer of Global Context
        pWdiDevCapability: output pointer of device capability

 @return Result of the function call
*/
WDI_Status 
WDI_Init
( 
  void*                      pOSContext,
  void**                     ppWDIGlobalCtx,
  WDI_DeviceCapabilityType*  pWdiDevCapability,
  unsigned int               driverType
)
{
  wpt_uint8               i;
  wpt_status              wptStatus; 
  WDI_Status              wdiStatus;
  WCTS_TransportCBsType   wctsCBs; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*---------------------------------------------------------------------
    Sanity check
  ---------------------------------------------------------------------*/
  if (( NULL == ppWDIGlobalCtx ) || ( NULL == pWdiDevCapability ))
  {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Invalid input parameters in WDI_Init");

    return WDI_STATUS_E_FAILURE; 
  }

  /*---------------------------------------------------------------------
    Check to see if the module has already been initialized or not 
  ---------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE != gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
              "WDI module already initialized - return");

    return WDI_STATUS_SUCCESS; 
  }

  /*Module is now initialized - this flag is to ensure the fact that multiple
   init will not happen on WDI
   !! - potential race does exist because read and set are not atomic,
   however an atomic operation would be closely here - reanalyze if necessary*/
  gWDIInitialized = eWLAN_PAL_TRUE; 

  /*Setup the control block */
  WDI_CleanCB(&gWDICb);
  gWDICb.pOSContext = pOSContext; 

  /*Setup the STA Table*/
  wdiStatus = WDI_STATableInit(&gWDICb);
  if ( WDI_STATUS_SUCCESS != wdiStatus )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_FATAL,
               "%s: Failure while initializing STA Table, status %d",
               __FUNCTION__, wdiStatus);
    goto fail_STATableInit;
  }

  /*------------------------------------------------------------------------
    Open the PAL
   ------------------------------------------------------------------------*/
  wptStatus =  wpalOpen(&gWDICb.pPALContext, pOSContext);
  if ( eWLAN_PAL_STATUS_SUCCESS != wptStatus )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_FATAL,
               "%s: Failed to open PAL, status %d",
               __FUNCTION__, wptStatus);
    goto fail_wpalOpen;
  }

  /*Initialize main synchro mutex - it will be used to ensure integrity of
   the main WDI Control Block*/
  wptStatus =  wpalMutexInit(&gWDICb.wptMutex);
  if ( eWLAN_PAL_STATUS_SUCCESS !=  wptStatus )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_FATAL,
               "%s: Failed to init mutex, status %d",
               __FUNCTION__, wptStatus);
    goto fail_mutex;
  }

  /*Initialize the response timer - it will be used to time all messages
    expected as response from device*/
  wptStatus = wpalTimerInit( &gWDICb.wptResponseTimer, 
                             WDI_ResponseTimerCB, 
                             &gWDICb);
  if ( eWLAN_PAL_STATUS_SUCCESS != wptStatus )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_FATAL,
              "%s: Failed to init response timer, status %d",
               __FUNCTION__, wptStatus);
    goto fail_timer;
  }

  /* Initialize the  WDI Pending Request Queue*/
  wptStatus = wpal_list_init(&(gWDICb.wptPendingQueue));
  if ( eWLAN_PAL_STATUS_SUCCESS != wptStatus )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_FATAL,
              "%s: Failed to init pending request queue, status %d",
               __FUNCTION__, wptStatus);
    goto fail_pend_queue;
  }

  /*Init WDI Pending Assoc Id Queue */
  wptStatus = wpal_list_init(&(gWDICb.wptPendingAssocSessionIdQueue));
  if ( eWLAN_PAL_STATUS_SUCCESS !=  wptStatus )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_FATAL,
              "%s: Failed to init assoc session queue, status %d",
               __FUNCTION__, wptStatus);
    goto fail_assoc_queue;
  }

  /*Initialize the BSS sessions pending Queue */
  for ( i = 0; i < WDI_MAX_BSS_SESSIONS; i++ )
  {
    wptStatus = wpal_list_init(&(gWDICb.aBSSSessions[i].wptPendingQueue));
    if ( eWLAN_PAL_STATUS_SUCCESS !=  wptStatus )
    {
      WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_FATAL,
                 "%s: Failed to init BSS %d pending queue, status %d",
                 __FUNCTION__, i, wptStatus);
      goto fail_bss_queue;
    }
  }

  /*Indicate the control block is sufficiently initialized for callbacks*/
  gWDICb.magic = WDI_CONTROL_BLOCK_MAGIC;

  /*------------------------------------------------------------------------
    Initialize the Data Path Utility Module
   ------------------------------------------------------------------------*/
  wdiStatus = WDI_DP_UtilsInit(&gWDICb);
  if ( WDI_STATUS_SUCCESS != wdiStatus )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_FATAL,
               "%s: Failed to initialize the DP Util Module, status %d",
               __FUNCTION__, wdiStatus);
    goto fail_dp_util_init;
  }

  /* Init Set power state event */
  wptStatus = wpalEventInit(&gWDICb.setPowerStateEvent);
  if ( eWLAN_PAL_STATUS_SUCCESS != wptStatus ) 
  {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_FATAL,
                "%s: Failed to initialize power state event, status %d",
                __FUNCTION__, wptStatus);
     goto fail_power_event;
  }

  /* Init WCTS action event */
  wptStatus = wpalEventInit(&gWDICb.wctsActionEvent);
  if ( eWLAN_PAL_STATUS_SUCCESS !=  wptStatus ) 
  {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_FATAL,
                "%s: Failed to initialize WCTS action event, status %d",
                __FUNCTION__, wptStatus);
     goto fail_wcts_event;
  }

  /*------------------------------------------------------------------------
    Open the Transport Services for Control and Data 
   ------------------------------------------------------------------------*/
  wctsCBs.wctsNotifyCB      = WDI_NotifyMsgCTSCB;
  wctsCBs.wctsNotifyCBData  = &gWDICb;
  wctsCBs.wctsRxMsgCB       = WDI_RXMsgCTSCB; 
  wctsCBs.wctsRxMsgCBData   = &gWDICb;

  gWDICb.bCTOpened          = eWLAN_PAL_FALSE; 
  gWDICb.wctsHandle = WCTS_OpenTransport( szTransportChName ,
                                          WDI_CT_CHANNEL_SIZE, 
                                          &wctsCBs ); 

  if ( NULL == gWDICb.wctsHandle )
  {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_FATAL,
                "%s: Failed to open WCTS transport", __FUNCTION__);
     goto fail_wcts_open;
  }

  gWDICb.driverMode = (tDriverType)driverType;
  /* FTM mode not need to open Transport Driver */
  if(eDRIVER_TYPE_MFG != (tDriverType)driverType)
  {  
    /*------------------------------------------------------------------------
     Open the Data Transport
     ------------------------------------------------------------------------*/
    if(eWLAN_PAL_STATUS_SUCCESS != WDTS_openTransport(&gWDICb))
    {
      WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_FATAL,
                 "%s: Failed to open the DT Transport", __FUNCTION__);
      goto fail_wdts_open;
    }
  }

  /*The WDI is initialized - set state to init */
  gWDICb.uGlobalState = WDI_INIT_ST; 

  /*Send the context as a ptr to the global WDI Control Block*/
  *ppWDIGlobalCtx = &gWDICb;

  /*Fill in the device capabilities*/
  pWdiDevCapability->bFrameXtlSupported = eWLAN_PAL_FALSE; 
  pWdiDevCapability->ucMaxSTASupported  = gWDICb.ucMaxStations;
  pWdiDevCapability->ucMaxBSSSupported  = gWDICb.ucMaxBssids;
  return WDI_STATUS_SUCCESS;

  /* ERROR handlers
     Undo everything that completed successfully */

 fail_wdts_open:
  {
     wpt_status             eventStatus;

     /* Closing WCTS in this scenario is tricky since it has to close
        the SMD channel and then we get notified asynchronously when
        the channel has been closed. So we take some of the logic from
        the "normal" close procedure in WDI_Close()
     */

     eventStatus = wpalEventReset(&gWDICb.wctsActionEvent);
     if ( eWLAN_PAL_STATUS_SUCCESS != eventStatus ) 
     {
        WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                   "%s: Failed to reset WCTS action event", __FUNCTION__);
     }

     WCTS_CloseTransport(gWDICb.wctsHandle);

     /* Wait for WCTS to close the control transport.  If we were able
        to reset the event flag, then we'll wait for the event,
        otherwise we'll wait for a maximum amount of time required for
        the channel to be closed */
     if ( eWLAN_PAL_STATUS_SUCCESS == eventStatus )
     {
        eventStatus = wpalEventWait(&gWDICb.wctsActionEvent, 
                                    WDI_WCTS_ACTION_TIMEOUT);
        if ( eWLAN_PAL_STATUS_SUCCESS != eventStatus )
        {
           WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                      "%s: Failed to wait on WCTS action event", __FUNCTION__);
        }
     }
     else
     {
        wpalSleep(WDI_WCTS_ACTION_TIMEOUT);
     }
  }
 fail_wcts_open:
  wpalEventDelete(&gWDICb.wctsActionEvent);
 fail_wcts_event:
  wpalEventDelete(&gWDICb.setPowerStateEvent);
 fail_power_event:
  WDI_DP_UtilsExit(&gWDICb);
 fail_dp_util_init:
  gWDICb.magic = 0;
 fail_bss_queue:
  /* entries 0 thru i-1 were successfully initialized */
  while (0 < i)
  {
     i--;
     wpal_list_destroy(&(gWDICb.aBSSSessions[i].wptPendingQueue));
  }
  wpal_list_destroy(&(gWDICb.wptPendingAssocSessionIdQueue));
 fail_assoc_queue:
  wpal_list_destroy(&(gWDICb.wptPendingQueue));
 fail_pend_queue:
  wpalTimerDelete(&gWDICb.wptResponseTimer);
 fail_timer:
  wpalMutexDelete(&gWDICb.wptMutex);
 fail_mutex:
  wpalClose(gWDICb.pPALContext);
 fail_wpalOpen:
  WDI_STATableClose(&gWDICb);
 fail_STATableInit:
  return WDI_STATUS_E_FAILURE; 

}/*WDI_Init*/;

/**
 @brief WDI_Start will be called when the upper MAC is ready to
        commence operation with the WLAN Device. Upon the call
        of this API the WLAN DAL will pack and send a HAL Start
        message to the lower RIVA sub-system if the SMD channel
        has been fully opened and the RIVA subsystem is up.

         If the RIVA sub-system is not yet up and running DAL
         will queue the request for Open and will wait for the
         SMD notification before attempting to send down the
         message to HAL. 

 WDI_Init must have been called.

 @param wdiStartParams: the start parameters as specified by 
                      the Device Interface
  
        wdiStartRspCb: callback for passing back the response of
        the start operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_Start
 @return Result of the function call
*/
WDI_Status 
WDI_Start
(
  WDI_StartReqParamsType*  pwdiStartParams,
  WDI_StartRspCb           wdiStartRspCb,
  void*                    pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_START_REQ;
  wdiEventData.pEventData      = pwdiStartParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiStartParams); 
  wdiEventData.pCBfnc          = wdiStartRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_Start*/

/**
 @brief WDI_Stop will be called when the upper MAC is ready to
        stop any operation with the WLAN Device. Upon the call
        of this API the WLAN DAL will pack and send a HAL Stop
        message to the lower RIVA sub-system if the DAL Core is
        in started state.

         In state BUSY this request will be queued.
  
         Request will not be accepted in any other state. 

 WDI_Start must have been called.

 @param wdiStopParams: the stop parameters as specified by 
                      the Device Interface
  
        wdiStopRspCb: callback for passing back the response of
        the stop operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_Start
 @return Result of the function call
*/
WDI_Status 
WDI_Stop
(
  WDI_StopReqParamsType*  pwdiStopParams,
  WDI_StopRspCb           wdiStopRspCb,
  void*                   pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_STOP_REQ;
  wdiEventData.pEventData      = pwdiStopParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiStopParams); 
  wdiEventData.pCBfnc          = wdiStopRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_STOP_EVENT, &wdiEventData);

}/*WDI_Stop*/



/**
 @brief WDI_Close will be called when the upper MAC no longer 
        needs to interact with DAL. DAL will free its control
        block.
  
        It is only accepted in state STOPPED.  

 WDI_Stop must have been called.

 @param none
  
 @see WDI_Stop
 @return Result of the function call
*/
WDI_Status 
WDI_Close
(
  void
)
{
  wpt_uint8              i;
  WDI_EventInfoType      wdiEventData;
  wpt_status             wptStatus;
  wpt_status             eventStatus;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*Reset WCTS action event prior to posting the WDI_CLOSE_REQ
   (the control transport will be closed by the FSM and we'll want
   to wait until that completes)*/
  eventStatus = wpalEventReset(&gWDICb.wctsActionEvent);
  if ( eWLAN_PAL_STATUS_SUCCESS != eventStatus ) 
  {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                "%s: Failed to reset WCTS action event", __FUNCTION__);
     /* fall through and try to finish closing via the FSM */
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_CLOSE_REQ;
  wdiEventData.pEventData      = NULL; 
  wdiEventData.uEventDataSize  = 0; 
  wdiEventData.pCBfnc          = NULL; 
  wdiEventData.pUserData       = NULL;

  gWDIInitialized = eWLAN_PAL_FALSE;

  wptStatus = WDI_PostMainEvent(&gWDICb, WDI_CLOSE_EVENT, &wdiEventData);

  /*Wait for WCTS to close the control transport
    (but only if we were able to reset the event flag*/
  if ( eWLAN_PAL_STATUS_SUCCESS == eventStatus )
  {
     eventStatus = wpalEventWait(&gWDICb.wctsActionEvent, 
                                 WDI_WCTS_ACTION_TIMEOUT);
     if ( eWLAN_PAL_STATUS_SUCCESS != eventStatus )
     {
        WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                   "%s: Failed to wait on WCTS action event", __FUNCTION__);
     }
  }

  /* Destroy the WCTS action event */
  wptStatus = wpalEventDelete(&gWDICb.wctsActionEvent);
  if ( eWLAN_PAL_STATUS_SUCCESS !=  wptStatus )
  {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                "WDI Close failed to destroy an event");
     WDI_ASSERT(0); 
  }

   /* Destroy the Set Power State event */
   wptStatus = wpalEventDelete(&gWDICb.setPowerStateEvent);
   if ( eWLAN_PAL_STATUS_SUCCESS !=  wptStatus )
   {
      WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                "WDI Close failed to destroy an event");

      WDI_ASSERT(0); 
   }

  /*------------------------------------------------------------------------
    Closes the Data Path Utility Module
   ------------------------------------------------------------------------*/
  if ( WDI_STATUS_SUCCESS != WDI_DP_UtilsExit(&gWDICb))
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
              "WDI Init failed to close the DP Util Module");

    WDI_ASSERT(0); 
  }

  /*destroy the BSS sessions pending Queue */
  for ( i = 0; i < WDI_MAX_BSS_SESSIONS; i++ )
  {
    wpal_list_destroy(&(gWDICb.aBSSSessions[i].wptPendingQueue));
  }

  /* destroy the WDI Pending Assoc Id Request Queue*/
  wpal_list_destroy(&(gWDICb.wptPendingAssocSessionIdQueue));

  /* destroy the WDI Pending Request Queue*/
  wpal_list_destroy(&(gWDICb.wptPendingQueue));
  
  /*destroy the response timer */
  wptStatus = wpalTimerDelete( &gWDICb.wptResponseTimer);

  /*invalidate the main synchro mutex */
  wptStatus = wpalMutexDelete(&gWDICb.wptMutex);
  if ( eWLAN_PAL_STATUS_SUCCESS !=  wptStatus )
  {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                "Failed to delete mutex %d", wptStatus);
     WDI_ASSERT(0);
  }

  /*Clear control block.  note that this will clear the "magic"
    which will inhibit all asynchronous callbacks*/
  WDI_CleanCB(&gWDICb);

  return wptStatus;

}/*WDI_Close*/

/**
 @brief  WDI_Shutdown will be called during 'SSR shutdown' operation.
         This will do most of the WDI stop & close
         operations without doing any handshake with Riva

         This will also make sure that the control transport
         will NOT be closed.

         This request will not be queued.


 WDI_Start must have been called.

 @param  closeTransport:  Close control channel if this is set

 @return Result of the function call
*/
WDI_Status
WDI_Shutdown
(
 wpt_boolean closeTransport
)
{
   WDI_EventInfoType      wdiEventData;
   wpt_status             wptStatus;
   int                    i = 0;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*------------------------------------------------------------------------
     Sanity Check
     ------------------------------------------------------------------------*/
   if ( eWLAN_PAL_FALSE == gWDIInitialized )
   {
      WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
            "WDI API call before module is initialized - Fail request");

      return WDI_STATUS_E_NOT_ALLOWED;
   }

   /*------------------------------------------------------------------------
     Fill in Event data and post to the Main FSM
     ------------------------------------------------------------------------*/
   wdiEventData.wdiRequest      = WDI_SHUTDOWN_REQ;
   wdiEventData.pEventData      = NULL;
   wdiEventData.uEventDataSize  = 0;

   /* Shutdown will not be queued, if the state is busy timer will be
    * stopped & this message will be processed.*/
   wptStatus = WDI_PostMainEvent(&gWDICb, WDI_SHUTDOWN_EVENT, &wdiEventData);
   if ( eWLAN_PAL_STATUS_SUCCESS != wptStatus )
   {
      WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_FATAL,
            "%s: Failed to process shutdown event", __FUNCTION__);
   }
   /* Destroy the Set Power State event */
   wptStatus = wpalEventDelete(&gWDICb.setPowerStateEvent);
   if ( eWLAN_PAL_STATUS_SUCCESS !=  wptStatus )
   {
      WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
            "WDI Close failed to destroy an event");

      WDI_ASSERT(0);
   }
   /*------------------------------------------------------------------------
     Closes the Data Path Utility Module
     ------------------------------------------------------------------------*/
   if ( WDI_STATUS_SUCCESS != WDI_DP_UtilsExit(&gWDICb))
   {
      WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
            "WDI Init failed to close the DP Util Module");

      WDI_ASSERT(0);
   }
   if ( closeTransport )
   {
      /* Close control transport, called from module unload */
      WCTS_CloseTransport(gWDICb.wctsHandle);
   }

   /*destroy the BSS sessions pending Queue */
   for ( i = 0; i < WDI_MAX_BSS_SESSIONS; i++ )
   {
      wpal_list_destroy(&(gWDICb.aBSSSessions[i].wptPendingQueue));
   }

   /* destroy the WDI Pending Assoc Id Request Queue*/
   wpal_list_destroy(&(gWDICb.wptPendingAssocSessionIdQueue));
   /* destroy the WDI Pending Request Queue*/
   wpal_list_destroy(&(gWDICb.wptPendingQueue));
   /*destroy the response timer */
   wptStatus = wpalTimerDelete( &gWDICb.wptResponseTimer);

   /*invalidate the main synchro mutex */
   wptStatus = wpalMutexDelete(&gWDICb.wptMutex);
   if ( eWLAN_PAL_STATUS_SUCCESS !=  wptStatus )
   {
      WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
            "%s: Failed to delete mutex %d",  __FUNCTION__, wptStatus);
      WDI_ASSERT(0);
   }

   /*Clear control block.  note that this will clear the "magic"
     which will inhibit all asynchronous callbacks*/
   WDI_CleanCB(&gWDICb);
   return wptStatus;

}/*WDI_Shutdown*/


/*======================================================================== 
 
                             SCAN APIs
 
==========================================================================*/

/**
 @brief WDI_InitScanReq will be called when the upper MAC wants 
        the WLAN Device to get ready for a scan procedure. Upon
        the call of this API the WLAN DAL will pack and send a
        HAL Init Scan request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_Start must have been called.

 @param wdiInitScanParams: the init scan parameters as specified
                      by the Device Interface
  
        wdiInitScanRspCb: callback for passing back the response
        of the init scan operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_Start
 @return Result of the function call
*/
WDI_Status 
WDI_InitScanReq
(
  WDI_InitScanReqParamsType*  pwdiInitScanParams,
  WDI_InitScanRspCb           wdiInitScanRspCb,
  void*                       pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_INIT_SCAN_REQ;
  wdiEventData.pEventData      = pwdiInitScanParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiInitScanParams); 
  wdiEventData.pCBfnc          = wdiInitScanRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_InitScanReq*/

/**
 @brief WDI_StartScanReq will be called when the upper MAC 
        wishes to change the Scan channel on the WLAN Device.
        Upon the call of this API the WLAN DAL will pack and
        send a HAL Start Scan request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_InitScanReq must have been called.

 @param wdiStartScanParams: the start scan parameters as 
                      specified by the Device Interface
  
        wdiStartScanRspCb: callback for passing back the
        response of the start scan operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_InitScanReq
 @return Result of the function call
*/
WDI_Status 
WDI_StartScanReq
(
  WDI_StartScanReqParamsType*  pwdiStartScanParams,
  WDI_StartScanRspCb           wdiStartScanRspCb,
  void*                        pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_START_SCAN_REQ;
  wdiEventData.pEventData      = pwdiStartScanParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiStartScanParams); 
  wdiEventData.pCBfnc          = wdiStartScanRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_StartScanReq*/


/**
 @brief WDI_EndScanReq will be called when the upper MAC is 
        wants to end scanning for a particular channel that it
        had set before by calling Scan Start on the WLAN Device.
        Upon the call of this API the WLAN DAL will pack and
        send a HAL End Scan request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_StartScanReq must have been called.

 @param wdiEndScanParams: the end scan parameters as specified 
                      by the Device Interface
  
        wdiEndScanRspCb: callback for passing back the response
        of the end scan operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_StartScanReq
 @return Result of the function call
*/
WDI_Status 
WDI_EndScanReq
(
  WDI_EndScanReqParamsType* pwdiEndScanParams,
  WDI_EndScanRspCb          wdiEndScanRspCb,
  void*                     pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_END_SCAN_REQ;
  wdiEventData.pEventData      = pwdiEndScanParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiEndScanParams); 
  wdiEventData.pCBfnc          = wdiEndScanRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_EndScanReq*/


/**
 @brief WDI_FinishScanReq will be called when the upper MAC has 
        completed the scan process on the WLAN Device. Upon the
        call of this API the WLAN DAL will pack and send a HAL
        Finish Scan Request request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_InitScanReq must have been called.

 @param wdiFinishScanParams: the finish scan  parameters as 
                      specified by the Device Interface
  
        wdiFinishScanRspCb: callback for passing back the
        response of the finish scan operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_InitScanReq
 @return Result of the function call
*/
WDI_Status 
WDI_FinishScanReq
(
  WDI_FinishScanReqParamsType* pwdiFinishScanParams,
  WDI_FinishScanRspCb          wdiFinishScanRspCb,
  void*                        pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_FINISH_SCAN_REQ;
  wdiEventData.pEventData      = pwdiFinishScanParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiFinishScanParams); 
  wdiEventData.pCBfnc          = wdiFinishScanRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_FinishScanReq*/

/*======================================================================== 
 
                          ASSOCIATION APIs
 
==========================================================================*/

/**
 @brief WDI_JoinReq will be called when the upper MAC is ready 
        to start an association procedure to a BSS. Upon the
        call of this API the WLAN DAL will pack and send a HAL
        Join request message to the lower RIVA sub-system if
        DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_Start must have been called.

 @param wdiJoinParams: the join parameters as specified by 
                      the Device Interface
  
        wdiJoinRspCb: callback for passing back the response of
        the join operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_Start
 @return Result of the function call
*/
WDI_Status 
WDI_JoinReq
(
  WDI_JoinReqParamsType* pwdiJoinParams,
  WDI_JoinRspCb          wdiJoinRspCb,
  void*                  pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_JOIN_REQ;
  wdiEventData.pEventData      = pwdiJoinParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiJoinParams); 
  wdiEventData.pCBfnc          = wdiJoinRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_JoinReq*/

/**
 @brief WDI_ConfigBSSReq will be called when the upper MAC 
        wishes to configure the newly acquired or in process of
        being acquired BSS to the HW . Upon the call of this API
        the WLAN DAL will pack and send a HAL Config BSS request
        message to the lower RIVA sub-system if DAL is in state
        STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_JoinReq must have been called.

 @param wdiConfigBSSParams: the config BSS parameters as 
                      specified by the Device Interface
  
        wdiConfigBSSRspCb: callback for passing back the
        response of the config BSS operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_JoinReq
 @return Result of the function call
*/
WDI_Status 
WDI_ConfigBSSReq
(
  WDI_ConfigBSSReqParamsType* pwdiConfigBSSParams,
  WDI_ConfigBSSRspCb          wdiConfigBSSRspCb,
  void*                       pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_CONFIG_BSS_REQ;
  wdiEventData.pEventData      = pwdiConfigBSSParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiConfigBSSParams); 
  wdiEventData.pCBfnc          = wdiConfigBSSRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_ConfigBSSReq*/

/**
 @brief WDI_DelBSSReq will be called when the upper MAC is 
        disassociating from the BSS and wishes to notify HW.
        Upon the call of this API the WLAN DAL will pack and
        send a HAL Del BSS request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_ConfigBSSReq or WDI_PostAssocReq must have been called.

 @param wdiDelBSSParams: the del BSS parameters as specified by 
                      the Device Interface
  
        wdiDelBSSRspCb: callback for passing back the response
        of the del bss operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_ConfigBSSReq, WDI_PostAssocReq 
 @return Result of the function call
*/
WDI_Status 
WDI_DelBSSReq
(
  WDI_DelBSSReqParamsType* pwdiDelBSSParams,
  WDI_DelBSSRspCb          wdiDelBSSRspCb,
  void*                    pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_DEL_BSS_REQ;
  wdiEventData.pEventData      = pwdiDelBSSParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiDelBSSParams); 
  wdiEventData.pCBfnc          = wdiDelBSSRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_DelBSSReq*/

/**
 @brief WDI_PostAssocReq will be called when the upper MAC has 
        associated to a BSS and wishes to configure HW for
        associated state. Upon the call of this API the WLAN DAL
        will pack and send a HAL Post Assoc request message to
        the lower RIVA sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_JoinReq must have been called.

 @param wdiPostAssocReqParams: the assoc parameters as specified
                      by the Device Interface
  
        wdiPostAssocRspCb: callback for passing back the
        response of the post assoc operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_JoinReq
 @return Result of the function call
*/
WDI_Status 
WDI_PostAssocReq
(
  WDI_PostAssocReqParamsType* pwdiPostAssocReqParams,
  WDI_PostAssocRspCb          wdiPostAssocRspCb,
  void*                       pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_POST_ASSOC_REQ;
  wdiEventData.pEventData      = pwdiPostAssocReqParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiPostAssocReqParams); 
  wdiEventData.pCBfnc          = wdiPostAssocRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_PostAssocReq*/

/**
 @brief WDI_DelSTAReq will be called when the upper MAC when an 
        association with another STA has ended and the station
        must be deleted from HW. Upon the call of this API the
        WLAN DAL will pack and send a HAL Del STA request
        message to the lower RIVA sub-system if DAL is in state
        STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param wdiDelSTAParams: the Del STA parameters as specified by 
                      the Device Interface
  
        wdiDelSTARspCb: callback for passing back the response
        of the del STA operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_DelSTAReq
(
  WDI_DelSTAReqParamsType* pwdiDelSTAParams,
  WDI_DelSTARspCb          wdiDelSTARspCb,
  void*                    pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_DEL_STA_REQ;
  wdiEventData.pEventData      = pwdiDelSTAParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiDelSTAParams); 
  wdiEventData.pCBfnc          = wdiDelSTARspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_DelSTAReq*/

/*======================================================================== 
 
                             SECURITY APIs
 
==========================================================================*/

/**
 @brief WDI_SetBSSKeyReq will be called when the upper MAC wants to
        install a BSS encryption key on the HW. Upon the call of this
        API the WLAN DAL will pack and send a Set BSS Key request
        message to the lower RIVA sub-system if DAL is in state
        STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param wdiSetBSSKeyParams: the BSS Key set parameters as 
                      specified by the Device Interface
  
        wdiSetBSSKeyRspCb: callback for passing back the
        response of the set BSS Key operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_SetBSSKeyReq
(
  WDI_SetBSSKeyReqParamsType* pwdiSetBSSKeyParams,
  WDI_SetBSSKeyRspCb          wdiSetBSSKeyRspCb,
  void*                       pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_SET_BSS_KEY_REQ;
  wdiEventData.pEventData      = pwdiSetBSSKeyParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiSetBSSKeyParams); 
  wdiEventData.pCBfnc          = wdiSetBSSKeyRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_SetBSSKeyReq*/

/**
 @brief WDI_RemoveBSSKeyReq will be called when the upper MAC wants to
        uninstall a BSS key from HW. Upon the call of this API the
        WLAN DAL will pack and send a HAL Remove BSS Key request
        message to the lower RIVA sub-system if DAL is in state
        STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_SetBSSKeyReq must have been called.

 @param wdiRemoveBSSKeyParams: the remove BSS key parameters as 
                      specified by the Device Interface
  
        wdiRemoveBSSKeyRspCb: callback for passing back the
        response of the remove BSS key operation received from
        the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_SetBSSKeyReq
 @return Result of the function call
*/
WDI_Status 
WDI_RemoveBSSKeyReq
(
  WDI_RemoveBSSKeyReqParamsType* pwdiRemoveBSSKeyParams,
  WDI_RemoveBSSKeyRspCb          wdiRemoveBSSKeyRspCb,
  void*                          pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_RMV_BSS_KEY_REQ;
  wdiEventData.pEventData      = pwdiRemoveBSSKeyParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiRemoveBSSKeyParams); 
  wdiEventData.pCBfnc          = wdiRemoveBSSKeyRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_RemoveBSSKeyReq*/


/**
 @brief WDI_SetSTAKeyReq will be called when the upper MAC is 
        ready to install a STA(ast) encryption key in HW. Upon
        the call of this API the WLAN DAL will pack and send a
        HAL Set STA Key request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param wdiSetSTAKeyParams: the set STA key parameters as 
                      specified by the Device Interface
  
        wdiSetSTAKeyRspCb: callback for passing back the
        response of the set STA key operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_SetSTAKeyReq
(
  WDI_SetSTAKeyReqParamsType* pwdiSetSTAKeyParams,
  WDI_SetSTAKeyRspCb          wdiSetSTAKeyRspCb,
  void*                       pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_SET_STA_KEY_REQ;
  wdiEventData.pEventData      = pwdiSetSTAKeyParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiSetSTAKeyParams); 
  wdiEventData.pCBfnc          = wdiSetSTAKeyRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_SetSTAKeyReq*/


/**
 @brief WDI_RemoveSTAKeyReq will be called when the upper MAC 
        wants to uninstall a previously set STA key in HW. Upon
        the call of this API the WLAN DAL will pack and send a
        HAL Remove STA Key request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_SetSTAKeyReq must have been called.

 @param wdiRemoveSTAKeyParams: the remove STA key parameters as 
                      specified by the Device Interface
  
        wdiRemoveSTAKeyRspCb: callback for passing back the
        response of the remove STA key operation received from
        the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_SetSTAKeyReq
 @return Result of the function call
*/
WDI_Status 
WDI_RemoveSTAKeyReq
(
  WDI_RemoveSTAKeyReqParamsType* pwdiRemoveSTAKeyParams,
  WDI_RemoveSTAKeyRspCb          wdiRemoveSTAKeyRspCb,
  void*                          pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_RMV_STA_KEY_REQ;
  wdiEventData.pEventData      = pwdiRemoveSTAKeyParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiRemoveSTAKeyParams); 
  wdiEventData.pCBfnc          = wdiRemoveSTAKeyRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_RemoveSTAKeyReq*/


/**
 @brief WDI_SetSTABcastKeyReq will be called when the upper MAC 
        wants to install a STA Bcast encryption key on the HW.
        Upon the call of this API the WLAN DAL will pack and
        send a HAL Start request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param pwdiSetSTABcastKeyParams: the BSS Key set parameters as 
                      specified by the Device Interface
  
        wdiSetSTABcastKeyRspCb: callback for passing back the
        response of the set BSS Key operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_SetSTABcastKeyReq
(
  WDI_SetSTAKeyReqParamsType* pwdiSetSTABcastKeyParams,
  WDI_SetSTAKeyRspCb          wdiSetSTABcastKeyRspCb,
  void*                       pUserData
)

{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_SET_STA_BCAST_KEY_REQ;
  wdiEventData.pEventData      = pwdiSetSTABcastKeyParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiSetSTABcastKeyParams); 
  wdiEventData.pCBfnc          = wdiSetSTABcastKeyRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_SetSTABcastKeyReq*/

/**
 @brief WDI_RemoveSTABcastKeyReq will be called when the upper 
        MAC wants to uninstall a STA Bcast key from HW. Upon the
        call of this API the WLAN DAL will pack and send a HAL
        Remove STA Bcast Key request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_SetSTABcastKeyReq must have been called.

 @param pwdiRemoveSTABcastKeyParams: the remove BSS key 
                      parameters as specified by the Device
                      Interface
  
        wdiRemoveSTABcastKeyRspCb: callback for passing back the
        response of the remove STA Bcast key operation received
        from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_SetSTABcastKeyReq
 @return Result of the function call
*/
WDI_Status 
WDI_RemoveSTABcastKeyReq
(
  WDI_RemoveSTAKeyReqParamsType* pwdiRemoveSTABcastKeyParams,
  WDI_RemoveSTAKeyRspCb          wdiRemoveSTABcastKeyRspCb,
  void*                          pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_RMV_STA_BCAST_KEY_REQ;
  wdiEventData.pEventData      = pwdiRemoveSTABcastKeyParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiRemoveSTABcastKeyParams); 
  wdiEventData.pCBfnc          = wdiRemoveSTABcastKeyRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_RemoveSTABcastKeyReq*/

/**
 @brief WDI_SetMaxTxPowerReq will be called when the upper 
        MAC wants to set Max Tx Power to HW. Upon the
        call of this API the WLAN DAL will pack and send a HAL
        Remove STA Bcast Key request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_SetSTABcastKeyReq must have been called.

 @param pwdiRemoveSTABcastKeyParams: the remove BSS key 
                      parameters as specified by the Device
                      Interface
  
        wdiRemoveSTABcastKeyRspCb: callback for passing back the
        response of the remove STA Bcast key operation received
        from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_SetMaxTxPowerReq
 @return Result of the function call
*/
WDI_Status 
WDI_SetMaxTxPowerReq
(
  WDI_SetMaxTxPowerParamsType*   pwdiSetMaxTxPowerParams,
  WDA_SetMaxTxPowerRspCb         wdiReqStatusCb,
  void*                          pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_SET_MAX_TX_POWER_REQ;
  wdiEventData.pEventData      = pwdiSetMaxTxPowerParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiSetMaxTxPowerParams); 
  wdiEventData.pCBfnc          = wdiReqStatusCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);
}

/*======================================================================== 
 
                            QoS and BA APIs
 
==========================================================================*/

/**
 @brief WDI_AddTSReq will be called when the upper MAC to inform
        the device of a successful add TSpec negotiation. HW
        needs to receive the TSpec Info from the UMAC in order
        to configure properly the QoS data traffic. Upon the
        call of this API the WLAN DAL will pack and send a HAL
        Add TS request message to the lower RIVA sub-system if
        DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param wdiAddTsReqParams: the add TS parameters as specified by
                      the Device Interface
  
        wdiAddTsRspCb: callback for passing back the response of
        the add TS operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_AddTSReq
(
  WDI_AddTSReqParamsType* pwdiAddTsReqParams,
  WDI_AddTsRspCb          wdiAddTsRspCb,
  void*                   pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_ADD_TS_REQ;
  wdiEventData.pEventData      = pwdiAddTsReqParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiAddTsReqParams); 
  wdiEventData.pCBfnc          = wdiAddTsRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_AddTSReq*/



/**
 @brief WDI_DelTSReq will be called when the upper MAC has ended
        admission on a specific AC. This is to inform HW that
        QoS traffic parameters must be rest. Upon the call of
        this API the WLAN DAL will pack and send a HAL Del TS
        request message to the lower RIVA sub-system if DAL is
        in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_AddTSReq must have been called.

 @param wdiDelTsReqParams: the del TS parameters as specified by
                      the Device Interface
  
        wdiDelTsRspCb: callback for passing back the response of
        the del TS operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_AddTSReq
 @return Result of the function call
*/
WDI_Status 
WDI_DelTSReq
(
  WDI_DelTSReqParamsType* pwdiDelTsReqParams,
  WDI_DelTsRspCb          wdiDelTsRspCb,
  void*                   pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_DEL_TS_REQ;
  wdiEventData.pEventData      = pwdiDelTsReqParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiDelTsReqParams); 
  wdiEventData.pCBfnc          = wdiDelTsRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_DelTSReq*/



/**
 @brief WDI_UpdateEDCAParams will be called when the upper MAC 
        wishes to update the EDCA parameters used by HW for QoS
        data traffic. Upon the call of this API the WLAN DAL
        will pack and send a HAL Update EDCA Params request
        message to the lower RIVA sub-system if DAL is in state
        STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param wdiUpdateEDCAParams: the start parameters as specified 
                      by the Device Interface
  
        wdiUpdateEDCAParamsRspCb: callback for passing back the
        response of the start operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_UpdateEDCAParams
(
  WDI_UpdateEDCAParamsType*    pwdiUpdateEDCAParams,
  WDI_UpdateEDCAParamsRspCb    wdiUpdateEDCAParamsRspCb,
  void*                        pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_UPD_EDCA_PRMS_REQ;
  wdiEventData.pEventData      = pwdiUpdateEDCAParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiUpdateEDCAParams); 
  wdiEventData.pCBfnc          = wdiUpdateEDCAParamsRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_UpdateEDCAParams*/


/**
 @brief WDI_AddBASessionReq will be called when the upper MAC has setup
        successfully a BA session and needs to notify the HW for
        the appropriate settings to take place. Upon the call of
        this API the WLAN DAL will pack and send a HAL Add BA
        request message to the lower RIVA sub-system if DAL is
        in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param wdiAddBAReqParams: the add BA parameters as specified by
                      the Device Interface
  
        wdiAddBARspCb: callback for passing back the response of
        the add BA operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_AddBASessionReq
(
  WDI_AddBASessionReqParamsType* pwdiAddBASessionReqParams,
  WDI_AddBASessionRspCb          wdiAddBASessionRspCb,
  void*                          pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_ADD_BA_SESSION_REQ;
  wdiEventData.pEventData      = pwdiAddBASessionReqParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiAddBASessionReqParams); 
  wdiEventData.pCBfnc          = wdiAddBASessionRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_AddBASessionReq*/

/**
 @brief WDI_DelBAReq will be called when the upper MAC wants to 
        inform HW that it has deleted a previously created BA
        session. Upon the call of this API the WLAN DAL will
        pack and send a HAL Del BA request message to the lower
        RIVA sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_AddBAReq must have been called.

 @param wdiDelBAReqParams: the del BA parameters as specified by
                      the Device Interface
  
        wdiDelBARspCb: callback for passing back the response of
        the del BA operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_AddBAReq
 @return Result of the function call
*/
WDI_Status 
WDI_DelBAReq
(
  WDI_DelBAReqParamsType* pwdiDelBAReqParams,
  WDI_DelBARspCb          wdiDelBARspCb,
  void*                   pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_DEL_BA_REQ;
  wdiEventData.pEventData      = pwdiDelBAReqParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiDelBAReqParams); 
  wdiEventData.pCBfnc          = wdiDelBARspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_DelBAReq*/

/*======================================================================== 
 
                            Power Save APIs
 
==========================================================================*/

/**
 @brief WDI_SetPwrSaveCfgReq will be called when the upper MAC 
        wants to set the power save related configurations of
        the WLAN Device. Upon the call of this API the WLAN DAL
        will pack and send a HAL Update CFG request message to
        the lower RIVA sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_Start must have been called.

 @param pwdiPowerSaveCfg: the power save cfg parameters as 
                      specified by the Device Interface
  
        wdiSetPwrSaveCfgCb: callback for passing back the
        response of the set power save cfg operation received
        from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_Start
 @return Result of the function call  
*/ 
WDI_Status 
WDI_SetPwrSaveCfgReq
(
  WDI_UpdateCfgReqParamsType*   pwdiPowerSaveCfg,
  WDI_SetPwrSaveCfgCb     wdiSetPwrSaveCfgCb,
  void*                   pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_UPDATE_CFG_REQ;
  wdiEventData.pEventData      = pwdiPowerSaveCfg; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiPowerSaveCfg); 
  wdiEventData.pCBfnc          = wdiSetPwrSaveCfgCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_SetPwrSaveCfgReq*/

/**
 @brief WDI_EnterImpsReq will be called when the upper MAC to 
        request the device to get into IMPS power state. Upon
        the call of this API the WLAN DAL will send a HAL Enter
        IMPS request message to the lower RIVA sub-system if DAL
        is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

  
 @param wdiEnterImpsRspCb: callback for passing back the 
        response of the Enter IMPS operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_Start
 @return Result of the function call
*/
WDI_Status 
WDI_EnterImpsReq
(
   WDI_EnterImpsRspCb  wdiEnterImpsRspCb,
   void*                   pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_ENTER_IMPS_REQ;
  wdiEventData.pEventData      = NULL; 
  wdiEventData.uEventDataSize  = 0; 
  wdiEventData.pCBfnc          = wdiEnterImpsRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_EnterImpsReq*/

/**
 @brief WDI_ExitImpsReq will be called when the upper MAC to 
        request the device to get out of IMPS power state. Upon
        the call of this API the WLAN DAL will send a HAL Exit
        IMPS request message to the lower RIVA sub-system if DAL
        is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 

 @param wdiExitImpsRspCb: callback for passing back the response 
        of the Exit IMPS operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_Start
 @return Result of the function call
*/
WDI_Status 
WDI_ExitImpsReq
(
   WDI_ExitImpsRspCb  wdiExitImpsRspCb,
   void*                   pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_EXIT_IMPS_REQ;
  wdiEventData.pEventData      = NULL; 
  wdiEventData.uEventDataSize  = 0; 
  wdiEventData.pCBfnc          = wdiExitImpsRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_ExitImpsReq*/

/**
 @brief WDI_EnterBmpsReq will be called when the upper MAC to 
        request the device to get into BMPS power state. Upon
        the call of this API the WLAN DAL will pack and send a
        HAL Enter BMPS request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param pwdiEnterBmpsReqParams: the Enter BMPS parameters as 
                      specified by the Device Interface
  
        wdiEnterBmpsRspCb: callback for passing back the
        response of the Enter BMPS operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_EnterBmpsReq
(
   WDI_EnterBmpsReqParamsType *pwdiEnterBmpsReqParams,
   WDI_EnterBmpsRspCb  wdiEnterBmpsRspCb,
   void*                   pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_ENTER_BMPS_REQ;
  wdiEventData.pEventData      = pwdiEnterBmpsReqParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiEnterBmpsReqParams); 
  wdiEventData.pCBfnc          = wdiEnterBmpsRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_EnterBmpsReq*/

/**
 @brief WDI_ExitBmpsReq will be called when the upper MAC to 
        request the device to get out of BMPS power state. Upon
        the call of this API the WLAN DAL will pack and send a
        HAL Exit BMPS request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param pwdiExitBmpsReqParams: the Exit BMPS parameters as 
                      specified by the Device Interface
  
        wdiExitBmpsRspCb: callback for passing back the response
        of the Exit BMPS operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_ExitBmpsReq
(
   WDI_ExitBmpsReqParamsType *pwdiExitBmpsReqParams,
   WDI_ExitBmpsRspCb  wdiExitBmpsRspCb,
   void*                   pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_EXIT_BMPS_REQ;
  wdiEventData.pEventData      = pwdiExitBmpsReqParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiExitBmpsReqParams); 
  wdiEventData.pCBfnc          = wdiExitBmpsRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_ExitBmpsReq*/

/**
 @brief WDI_EnterUapsdReq will be called when the upper MAC to 
        request the device to get into UAPSD power state. Upon
        the call of this API the WLAN DAL will pack and send a
        HAL Enter UAPSD request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.
 WDI_SetUapsdAcParamsReq must have been called.
  
 @param pwdiEnterUapsdReqParams: the Enter UAPSD parameters as 
                      specified by the Device Interface
  
        wdiEnterUapsdRspCb: callback for passing back the
        response of the Enter UAPSD operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq, WDI_SetUapsdAcParamsReq
 @return Result of the function call
*/
WDI_Status 
WDI_EnterUapsdReq
(
   WDI_EnterUapsdReqParamsType *pwdiEnterUapsdReqParams,
   WDI_EnterUapsdRspCb  wdiEnterUapsdRspCb,
   void*                   pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_ENTER_UAPSD_REQ;
  wdiEventData.pEventData      = pwdiEnterUapsdReqParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiEnterUapsdReqParams); 
  wdiEventData.pCBfnc          = wdiEnterUapsdRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_EnterUapsdReq*/

/**
 @brief WDI_ExitUapsdReq will be called when the upper MAC to 
        request the device to get out of UAPSD power state. Upon
        the call of this API the WLAN DAL will send a HAL Exit
        UAPSD request message to the lower RIVA sub-system if
        DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param wdiExitUapsdRspCb: callback for passing back the 
        response of the Exit UAPSD operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_ExitUapsdReq
(
   WDI_ExitUapsdRspCb  wdiExitUapsdRspCb,
   void*                   pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_EXIT_UAPSD_REQ;
  wdiEventData.pEventData      = NULL; 
  wdiEventData.uEventDataSize  = 0; 
  wdiEventData.pCBfnc          = wdiExitUapsdRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_ExitUapsdReq*/

/**
 @brief WDI_UpdateUapsdParamsReq will be called when the upper 
        MAC wants to set the UAPSD related configurations
        of an associated STA (while acting as an AP) to the WLAN
        Device. Upon the call of this API the WLAN DAL will pack
        and send a HAL Update UAPSD params request message to
        the lower RIVA sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_ConfigBSSReq must have been called.

 @param pwdiUpdateUapsdReqParams: the UAPSD parameters 
                      as specified by the Device Interface
  
        wdiUpdateUapsdParamsCb: callback for passing back the
        response of the update UAPSD params operation received
        from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_ConfigBSSReq
 @return Result of the function call
*/
WDI_Status 
WDI_UpdateUapsdParamsReq
(
   WDI_UpdateUapsdReqParamsType *pwdiUpdateUapsdReqParams,
   WDI_UpdateUapsdParamsCb  wdiUpdateUapsdParamsCb,
   void*                   pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_UPDATE_UAPSD_PARAM_REQ;
  wdiEventData.pEventData      = pwdiUpdateUapsdReqParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiUpdateUapsdReqParams);; 
  wdiEventData.pCBfnc          = wdiUpdateUapsdParamsCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_UpdateUapsdParamsReq*/

/**
 @brief WDI_SetUapsdAcParamsReq will be called when the upper 
        MAC wants to set the UAPSD related configurations before
        requesting for enter UAPSD power state to the WLAN
        Device. Upon the call of this API the WLAN DAL will pack
        and send a HAL Set UAPSD params request message to
        the lower RIVA sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param pwdiUapsdInfo: the UAPSD parameters as specified by
                      the Device Interface
  
        wdiSetUapsdAcParamsCb: callback for passing back the
        response of the set UAPSD params operation received from
        the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_SetUapsdAcParamsReq
(
  WDI_SetUapsdAcParamsReqParamsType*      pwdiUapsdInfo,
  WDI_SetUapsdAcParamsCb  wdiSetUapsdAcParamsCb,
  void*                   pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_SET_UAPSD_PARAM_REQ;
  wdiEventData.pEventData      = pwdiUapsdInfo; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiUapsdInfo); 
  wdiEventData.pCBfnc          = wdiSetUapsdAcParamsCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_SetUapsdAcParamsReq*/

/**
 @brief WDI_ConfigureRxpFilterReq will be called when the upper 
        MAC wants to set/reset the RXP filters for received pkts
        (MC, BC etc.). Upon the call of this API the WLAN DAL will pack
        and send a HAL configure RXP filter request message to
        the lower RIVA sub-system.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

  
 @param pwdiConfigureRxpFilterReqParams: the RXP 
                      filter as specified by the Device
                      Interface
  
        wdiConfigureRxpFilterCb: callback for passing back the
        response of the configure RXP filter operation received
        from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @return Result of the function call
*/
WDI_Status 
WDI_ConfigureRxpFilterReq
(
   WDI_ConfigureRxpFilterReqParamsType *pwdiConfigureRxpFilterReqParams,
   WDI_ConfigureRxpFilterCb             wdiConfigureRxpFilterCb,
   void*                                pUserData
)
{
   WDI_EventInfoType      wdiEventData;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*------------------------------------------------------------------------
     Sanity Check 
   ------------------------------------------------------------------------*/
   if ( eWLAN_PAL_FALSE == gWDIInitialized )
   {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WDI API call before module is initialized - Fail request");

     return WDI_STATUS_E_NOT_ALLOWED; 
   }

   /*------------------------------------------------------------------------
     Fill in Event data and post to the Main FSM
   ------------------------------------------------------------------------*/
   wdiEventData.wdiRequest      = WDI_CONFIGURE_RXP_FILTER_REQ;
   wdiEventData.pEventData      = pwdiConfigureRxpFilterReqParams; 
   wdiEventData.uEventDataSize  = sizeof(*pwdiConfigureRxpFilterReqParams); 
   wdiEventData.pCBfnc          = wdiConfigureRxpFilterCb; 
   wdiEventData.pUserData       = pUserData;

   return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);
}/*WDI_ConfigureRxpFilterReq*/

/**
 @brief WDI_SetBeaconFilterReq will be called when the upper MAC
        wants to set the beacon filters while in power save.
        Upon the call of this API the WLAN DAL will pack and
        send a Beacon filter request message to the
        lower RIVA sub-system.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

  
 @param pwdiBeaconFilterReqParams: the beacon 
                      filter as specified by the Device
                      Interface
  
        wdiBeaconFilterCb: callback for passing back the
        response of the set beacon filter operation received
        from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @return Result of the function call
*/
WDI_Status 
WDI_SetBeaconFilterReq
(
   WDI_BeaconFilterReqParamsType   *pwdiBeaconFilterReqParams,
   WDI_SetBeaconFilterCb            wdiBeaconFilterCb,
   void*                            pUserData
)
{
   WDI_EventInfoType      wdiEventData;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*------------------------------------------------------------------------
     Sanity Check 
   ------------------------------------------------------------------------*/
   if ( eWLAN_PAL_FALSE == gWDIInitialized )
   {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WDI API call before module is initialized - Fail request");

     return WDI_STATUS_E_NOT_ALLOWED; 
   }

   /*------------------------------------------------------------------------
     Fill in Event data and post to the Main FSM
   ------------------------------------------------------------------------*/
   wdiEventData.wdiRequest      = WDI_SET_BEACON_FILTER_REQ;
   wdiEventData.pEventData      = pwdiBeaconFilterReqParams; 
   wdiEventData.uEventDataSize  = sizeof(*pwdiBeaconFilterReqParams);; 
   wdiEventData.pCBfnc          = wdiBeaconFilterCb; 
   wdiEventData.pUserData       = pUserData;

   return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);
}/*WDI_SetBeaconFilterReq*/

/**
 @brief WDI_RemBeaconFilterReq will be called when the upper MAC
        wants to remove the beacon filter for particular IE
        while in power save. Upon the call of this API the WLAN
        DAL will pack and send a remove Beacon filter request
        message to the lower RIVA sub-system.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

  
 @param pwdiBeaconFilterReqParams: the beacon 
                      filter as specified by the Device
                      Interface
  
        wdiBeaconFilterCb: callback for passing back the
        response of the remove beacon filter operation received
        from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @return Result of the function call
*/
WDI_Status 
WDI_RemBeaconFilterReq
(
   WDI_RemBeaconFilterReqParamsType *pwdiBeaconFilterReqParams,
   WDI_RemBeaconFilterCb             wdiBeaconFilterCb,
   void*                             pUserData
)
{
   WDI_EventInfoType      wdiEventData;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*------------------------------------------------------------------------
     Sanity Check 
   ------------------------------------------------------------------------*/
   if ( eWLAN_PAL_FALSE == gWDIInitialized )
   {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WDI API call before module is initialized - Fail request");

     return WDI_STATUS_E_NOT_ALLOWED; 
   }

   /*------------------------------------------------------------------------
     Fill in Event data and post to the Main FSM
   ------------------------------------------------------------------------*/
   wdiEventData.wdiRequest      = WDI_REM_BEACON_FILTER_REQ;
   wdiEventData.pEventData      = pwdiBeaconFilterReqParams; 
   wdiEventData.uEventDataSize  = sizeof(*pwdiBeaconFilterReqParams);; 
   wdiEventData.pCBfnc          = wdiBeaconFilterCb; 
   wdiEventData.pUserData       = pUserData;

   return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);
}/*WDI_RemBeaconFilterReq*/

/**
 @brief WDI_SetRSSIThresholdsReq will be called when the upper 
        MAC wants to set the RSSI thresholds related
        configurations while in power save. Upon the call of
        this API the WLAN DAL will pack and send a HAL Set RSSI
        thresholds request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param pwdiUapsdInfo: the UAPSD parameters as specified by
                      the Device Interface
  
        wdiSetUapsdAcParamsCb: callback for passing back the
        response of the set UAPSD params operation received from
        the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_SetRSSIThresholdsReq
(
  WDI_SetRSSIThresholdsReqParamsType*      pwdiRSSIThresholdsParams,
  WDI_SetRSSIThresholdsCb                  wdiSetRSSIThresholdsCb,
  void*                                    pUserData
)
{
   WDI_EventInfoType      wdiEventData;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*------------------------------------------------------------------------
     Sanity Check 
   ------------------------------------------------------------------------*/
   if ( eWLAN_PAL_FALSE == gWDIInitialized )
   {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WDI API call before module is initialized - Fail request");

     return WDI_STATUS_E_NOT_ALLOWED; 
   }

   /*------------------------------------------------------------------------
     Fill in Event data and post to the Main FSM
   ------------------------------------------------------------------------*/
   wdiEventData.wdiRequest      = WDI_SET_RSSI_THRESHOLDS_REQ;
   wdiEventData.pEventData      = pwdiRSSIThresholdsParams; 
   wdiEventData.uEventDataSize  = sizeof(*pwdiRSSIThresholdsParams);; 
   wdiEventData.pCBfnc          = wdiSetRSSIThresholdsCb; 
   wdiEventData.pUserData       = pUserData;

   return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);
}/* WDI_SetRSSIThresholdsReq*/

/**
 @brief WDI_HostOffloadReq will be called when the upper MAC 
        wants to set the filter to minimize unnecessary host
        wakeup due to broadcast traffic while in power save.
        Upon the call of this API the WLAN DAL will pack and
        send a HAL host offload request message to the
        lower RIVA sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param pwdiHostOffloadParams: the host offload as specified 
                      by the Device Interface
  
        wdiHostOffloadCb: callback for passing back the response
        of the host offload operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_HostOffloadReq
(
  WDI_HostOffloadReqParamsType*      pwdiHostOffloadParams,
  WDI_HostOffloadCb                  wdiHostOffloadCb,
  void*                              pUserData
)
{
   WDI_EventInfoType      wdiEventData;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*------------------------------------------------------------------------
     Sanity Check 
   ------------------------------------------------------------------------*/
   if ( eWLAN_PAL_FALSE == gWDIInitialized )
   {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WDI API call before module is initialized - Fail request");

     return WDI_STATUS_E_NOT_ALLOWED; 
   }

   /*------------------------------------------------------------------------
     Fill in Event data and post to the Main FSM
   ------------------------------------------------------------------------*/
   wdiEventData.wdiRequest      = WDI_HOST_OFFLOAD_REQ;
   wdiEventData.pEventData      = pwdiHostOffloadParams; 
   wdiEventData.uEventDataSize  = sizeof(*pwdiHostOffloadParams);; 
   wdiEventData.pCBfnc          = wdiHostOffloadCb; 
   wdiEventData.pUserData       = pUserData;

   return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);
}/*WDI_HostOffloadReq*/

/**
 @brief WDI_KeepAliveReq will be called when the upper MAC 
        wants to set the filter to send NULL or unsolicited ARP responses 
        and minimize unnecessary host wakeups due to while in power save.
        Upon the call of this API the WLAN DAL will pack and
        send a HAL Keep Alive request message to the
        lower RIVA sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param pwdiKeepAliveParams: the Keep Alive as specified 
                      by the Device Interface
  
        wdiKeepAliveCb: callback for passing back the response
        of the Keep Alive operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_KeepAliveReq
(
  WDI_KeepAliveReqParamsType*        pwdiKeepAliveParams,
  WDI_KeepAliveCb                    wdiKeepAliveCb,
  void*                              pUserData
)
{
    WDI_EventInfoType      wdiEventData;
    /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

    /*------------------------------------------------------------------------
     Sanity Check 
    ------------------------------------------------------------------------*/
    if ( eWLAN_PAL_FALSE == gWDIInitialized )
    {
         WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                    "WDI_KeepAliveReq: WDI API call before module "
                    "is initialized - Fail request");

     return WDI_STATUS_E_NOT_ALLOWED; 
    }

    /*------------------------------------------------------------------------
     Fill in Event data and post to the Main FSM
    ------------------------------------------------------------------------*/
    wdiEventData.wdiRequest      = WDI_KEEP_ALIVE_REQ;
    wdiEventData.pEventData      = pwdiKeepAliveParams; 
    wdiEventData.uEventDataSize  = sizeof(*pwdiKeepAliveParams); 
    wdiEventData.pCBfnc          = wdiKeepAliveCb; 
    wdiEventData.pUserData       = pUserData;

    return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);
}/*WDI_KeepAliveReq*/

/**
 @brief WDI_WowlAddBcPtrnReq will be called when the upper MAC 
        wants to set the Wowl Bcast ptrn to minimize unnecessary
        host wakeup due to broadcast traffic while in power
        save. Upon the call of this API the WLAN DAL will pack
        and send a HAL Wowl Bcast ptrn request message to the
        lower RIVA sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param pwdiWowlAddBcPtrnParams: the Wowl bcast ptrn as 
                      specified by the Device Interface
  
        wdiWowlAddBcPtrnCb: callback for passing back the
        response of the add Wowl bcast ptrn operation received
        from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_WowlAddBcPtrnReq
(
  WDI_WowlAddBcPtrnReqParamsType*    pwdiWowlAddBcPtrnParams,
  WDI_WowlAddBcPtrnCb                wdiWowlAddBcPtrnCb,
  void*                              pUserData
)
{
   WDI_EventInfoType      wdiEventData;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*------------------------------------------------------------------------
     Sanity Check 
   ------------------------------------------------------------------------*/
   if ( eWLAN_PAL_FALSE == gWDIInitialized )
   {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WDI API call before module is initialized - Fail request");

     return WDI_STATUS_E_NOT_ALLOWED; 
   }

   /*------------------------------------------------------------------------
     Fill in Event data and post to the Main FSM
   ------------------------------------------------------------------------*/
   wdiEventData.wdiRequest      = WDI_WOWL_ADD_BC_PTRN_REQ;
   wdiEventData.pEventData      = pwdiWowlAddBcPtrnParams; 
   wdiEventData.uEventDataSize  = sizeof(*pwdiWowlAddBcPtrnParams);; 
   wdiEventData.pCBfnc          = wdiWowlAddBcPtrnCb; 
   wdiEventData.pUserData       = pUserData;

   return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);
}/*WDI_WowlAddBcPtrnReq*/

/**
 @brief WDI_WowlDelBcPtrnReq will be called when the upper MAC 
        wants to clear the Wowl Bcast ptrn. Upon the call of
        this API the WLAN DAL will pack and send a HAL delete
        Wowl Bcast ptrn request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_WowlAddBcPtrnReq must have been called.

 @param pwdiWowlDelBcPtrnParams: the Wowl bcast ptrn as 
                      specified by the Device Interface
  
        wdiWowlDelBcPtrnCb: callback for passing back the
        response of the del Wowl bcast ptrn operation received
        from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_WowlAddBcPtrnReq
 @return Result of the function call
*/
WDI_Status 
WDI_WowlDelBcPtrnReq
(
  WDI_WowlDelBcPtrnReqParamsType*    pwdiWowlDelBcPtrnParams,
  WDI_WowlDelBcPtrnCb                wdiWowlDelBcPtrnCb,
  void*                              pUserData
)
{
   WDI_EventInfoType      wdiEventData;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*------------------------------------------------------------------------
     Sanity Check 
   ------------------------------------------------------------------------*/
   if ( eWLAN_PAL_FALSE == gWDIInitialized )
   {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WDI API call before module is initialized - Fail request");

     return WDI_STATUS_E_NOT_ALLOWED; 
   }

   /*------------------------------------------------------------------------
     Fill in Event data and post to the Main FSM
   ------------------------------------------------------------------------*/
   wdiEventData.wdiRequest      = WDI_WOWL_DEL_BC_PTRN_REQ;
   wdiEventData.pEventData      = pwdiWowlDelBcPtrnParams; 
   wdiEventData.uEventDataSize  = sizeof(*pwdiWowlDelBcPtrnParams);; 
   wdiEventData.pCBfnc          = wdiWowlDelBcPtrnCb; 
   wdiEventData.pUserData       = pUserData;

   return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);
}/*WDI_WowlDelBcPtrnReq*/

/**
 @brief WDI_WowlEnterReq will be called when the upper MAC 
        wants to enter the Wowl state to minimize unnecessary
        host wakeup while in power save. Upon the call of this
        API the WLAN DAL will pack and send a HAL Wowl enter
        request message to the lower RIVA sub-system if DAL is
        in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param pwdiWowlEnterReqParams: the Wowl enter info as 
                      specified by the Device Interface
  
        wdiWowlEnterReqCb: callback for passing back the
        response of the enter Wowl operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_WowlEnterReq
(
  WDI_WowlEnterReqParamsType*    pwdiWowlEnterParams,
  WDI_WowlEnterReqCb             wdiWowlEnterCb,
  void*                          pUserData
)
{
   WDI_EventInfoType      wdiEventData;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*------------------------------------------------------------------------
     Sanity Check 
   ------------------------------------------------------------------------*/
   if ( eWLAN_PAL_FALSE == gWDIInitialized )
   {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WDI API call before module is initialized - Fail request");

     return WDI_STATUS_E_NOT_ALLOWED; 
   }

   /*------------------------------------------------------------------------
     Fill in Event data and post to the Main FSM
   ------------------------------------------------------------------------*/
   wdiEventData.wdiRequest      = WDI_WOWL_ENTER_REQ;
   wdiEventData.pEventData      = pwdiWowlEnterParams; 
   wdiEventData.uEventDataSize  = sizeof(*pwdiWowlEnterParams);; 
   wdiEventData.pCBfnc          = wdiWowlEnterCb; 
   wdiEventData.pUserData       = pUserData;

   return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);
}/*WDI_WowlEnterReq*/

/**
 @brief WDI_WowlExitReq will be called when the upper MAC 
        wants to exit the Wowl state. Upon the call of this API
        the WLAN DAL will pack and send a HAL Wowl exit request
        message to the lower RIVA sub-system if DAL is in state
        STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_WowlEnterReq must have been called.

 @param pwdiWowlExitReqParams: the Wowl exit info as 
                      specified by the Device Interface
  
        wdiWowlExitReqCb: callback for passing back the response
        of the exit Wowl operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_WowlEnterReq
 @return Result of the function call
*/
WDI_Status 
WDI_WowlExitReq
(
  WDI_WowlExitReqCb              wdiWowlExitCb,
  void*                          pUserData
)
{
   WDI_EventInfoType      wdiEventData;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*------------------------------------------------------------------------
     Sanity Check 
   ------------------------------------------------------------------------*/
   if ( eWLAN_PAL_FALSE == gWDIInitialized )
   {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WDI API call before module is initialized - Fail request");

     return WDI_STATUS_E_NOT_ALLOWED; 
   }

   /*------------------------------------------------------------------------
     Fill in Event data and post to the Main FSM
   ------------------------------------------------------------------------*/
   wdiEventData.wdiRequest      = WDI_WOWL_EXIT_REQ;
   wdiEventData.pEventData      = NULL; 
   wdiEventData.uEventDataSize  = 0; 
   wdiEventData.pCBfnc          = wdiWowlExitCb; 
   wdiEventData.pUserData       = pUserData;

   return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);
}/*WDI_WowlExitReq*/

/**
 @brief WDI_ConfigureAppsCpuWakeupStateReq will be called when 
        the upper MAC wants to dynamically adjusts the listen
        interval based on the WLAN/MSM activity. Upon the call
        of this API the WLAN DAL will pack and send a HAL
        configure Apps Cpu Wakeup State request message to the
        lower RIVA sub-system.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

  
 @param pwdiConfigureAppsCpuWakeupStateReqParams: the 
                      Apps Cpu Wakeup State as specified by the
                      Device Interface
  
        wdiConfigureAppsCpuWakeupStateCb: callback for passing
        back the response of the configure Apps Cpu Wakeup State
        operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @return Result of the function call
*/
WDI_Status 
WDI_ConfigureAppsCpuWakeupStateReq
(
   WDI_ConfigureAppsCpuWakeupStateReqParamsType *pwdiConfigureAppsCpuWakeupStateReqParams,
   WDI_ConfigureAppsCpuWakeupStateCb             wdiConfigureAppsCpuWakeupStateCb,
   void*                                         pUserData
)
{
   WDI_EventInfoType      wdiEventData;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*------------------------------------------------------------------------
     Sanity Check 
   ------------------------------------------------------------------------*/
   if ( eWLAN_PAL_FALSE == gWDIInitialized )
   {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WDI API call before module is initialized - Fail request");

     return WDI_STATUS_E_NOT_ALLOWED; 
   }

   /*------------------------------------------------------------------------
     Fill in Event data and post to the Main FSM
   ------------------------------------------------------------------------*/
   wdiEventData.wdiRequest      = WDI_CONFIGURE_APPS_CPU_WAKEUP_STATE_REQ;
   wdiEventData.pEventData      = pwdiConfigureAppsCpuWakeupStateReqParams; 
   wdiEventData.uEventDataSize  = sizeof(*pwdiConfigureAppsCpuWakeupStateReqParams); 
   wdiEventData.pCBfnc          = wdiConfigureAppsCpuWakeupStateCb; 
   wdiEventData.pUserData       = pUserData;

   return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);
}/*WDI_ConfigureAppsCpuWakeupStateReq*/
/**
 @brief WDI_FlushAcReq will be called when the upper MAC wants 
        to to perform a flush operation on a given AC. Upon the
        call of this API the WLAN DAL will pack and send a HAL
        Flush AC request message to the lower RIVA sub-system if
        DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_AddBAReq must have been called.

 @param pwdiFlushAcReqParams: the Flush AC parameters as 
                      specified by the Device Interface
  
        wdiFlushAcRspCb: callback for passing back the response
        of the Flush AC operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_AddBAReq
 @return Result of the function call
*/
WDI_Status 
WDI_FlushAcReq
(
  WDI_FlushAcReqParamsType* pwdiFlushAcReqParams,
  WDI_FlushAcRspCb          wdiFlushAcRspCb,
  void*                     pUserData
)
{
   WDI_EventInfoType      wdiEventData;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*------------------------------------------------------------------------
     Sanity Check 
   ------------------------------------------------------------------------*/
   if ( eWLAN_PAL_FALSE == gWDIInitialized )
   {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WDI API call before module is initialized - Fail request");

     return WDI_STATUS_E_NOT_ALLOWED; 
   }

   /*------------------------------------------------------------------------
     Fill in Event data and post to the Main FSM
   ------------------------------------------------------------------------*/
   wdiEventData.wdiRequest      = WDI_FLUSH_AC_REQ;
   wdiEventData.pEventData      = pwdiFlushAcReqParams; 
   wdiEventData.uEventDataSize  = sizeof(*pwdiFlushAcReqParams); 
   wdiEventData.pCBfnc          = wdiFlushAcRspCb; 
   wdiEventData.pUserData       = pUserData;

   return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_FlushAcReq*/

/**
 @brief WDI_BtAmpEventReq will be called when the upper MAC 
        wants to notify the lower mac on a BT AMP event. This is
        to inform BTC-SLM that some BT AMP event occurred. Upon
        the call of this API the WLAN DAL will pack and send a
        HAL BT AMP event request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

  
 @param wdiBtAmpEventReqParams: the BT AMP event parameters as 
                      specified by the Device Interface
  
        wdiBtAmpEventRspCb: callback for passing back the
        response of the BT AMP event operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
 
 @return Result of the function call
*/
WDI_Status 
WDI_BtAmpEventReq
(
  WDI_BtAmpEventParamsType* pwdiBtAmpEventReqParams,
  WDI_BtAmpEventRspCb       wdiBtAmpEventRspCb,
  void*                     pUserData
)
{
   WDI_EventInfoType      wdiEventData;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*------------------------------------------------------------------------
     Sanity Check 
   ------------------------------------------------------------------------*/
   if ( eWLAN_PAL_FALSE == gWDIInitialized )
   {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WDI API call before module is initialized - Fail request");

     return WDI_STATUS_E_NOT_ALLOWED; 
   }

   /*------------------------------------------------------------------------
     Fill in Event data and post to the Main FSM
   ------------------------------------------------------------------------*/
   wdiEventData.wdiRequest      = WDI_BTAMP_EVENT_REQ;
   wdiEventData.pEventData      = pwdiBtAmpEventReqParams; 
   wdiEventData.uEventDataSize  = sizeof(*pwdiBtAmpEventReqParams); 
   wdiEventData.pCBfnc          = wdiBtAmpEventRspCb; 
   wdiEventData.pUserData       = pUserData;

   return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_BtAmpEventReq*/



/*======================================================================== 
 
                             CONTROL APIs
 
==========================================================================*/
/**
 @brief WDI_SwitchChReq will be called when the upper MAC wants 
        the WLAN HW to change the current channel of operation.
        Upon the call of this API the WLAN DAL will pack and
        send a HAL Start request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_Start must have been called.

 @param wdiSwitchChReqParams: the switch ch parameters as 
                      specified by the Device Interface
  
        wdiSwitchChRspCb: callback for passing back the response
        of the switch ch operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_Start
 @return Result of the function call
*/
WDI_Status 
WDI_SwitchChReq
(
  WDI_SwitchChReqParamsType* pwdiSwitchChReqParams,
  WDI_SwitchChRspCb          wdiSwitchChRspCb,
  void*                      pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_CH_SWITCH_REQ;
  wdiEventData.pEventData      = pwdiSwitchChReqParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiSwitchChReqParams); 
  wdiEventData.pCBfnc          = wdiSwitchChRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_SwitchChReq*/


/**
 @brief WDI_ConfigSTAReq will be called when the upper MAC 
        wishes to add or update a STA in HW. Upon the call of
        this API the WLAN DAL will pack and send a HAL Start
        message request message to the lower RIVA sub-system if
        DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_Start must have been called.

 @param wdiConfigSTAReqParams: the config STA parameters as 
                      specified by the Device Interface
  
        wdiConfigSTARspCb: callback for passing back the
        response of the config STA operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_Start
 @return Result of the function call
*/
WDI_Status 
WDI_ConfigSTAReq
(
  WDI_ConfigSTAReqParamsType* pwdiConfigSTAReqParams,
  WDI_ConfigSTARspCb          wdiConfigSTARspCb,
  void*                       pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_CONFIG_STA_REQ;
  wdiEventData.pEventData      = pwdiConfigSTAReqParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiConfigSTAReqParams); 
  wdiEventData.pCBfnc          = wdiConfigSTARspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_ConfigSTAReq*/

/**
 @brief WDI_SetLinkStateReq will be called when the upper MAC 
        wants to change the state of an ongoing link. Upon the
        call of this API the WLAN DAL will pack and send a HAL
        Start message request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_JoinStartReq must have been called.

 @param wdiSetLinkStateReqParams: the set link state parameters 
                      as specified by the Device Interface
  
        wdiSetLinkStateRspCb: callback for passing back the
        response of the set link state operation received from
        the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_JoinStartReq
 @return Result of the function call
*/
WDI_Status 
WDI_SetLinkStateReq
(
  WDI_SetLinkReqParamsType* pwdiSetLinkStateReqParams,
  WDI_SetLinkStateRspCb     wdiSetLinkStateRspCb,
  void*                     pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_SET_LINK_ST_REQ;
  wdiEventData.pEventData      = pwdiSetLinkStateReqParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiSetLinkStateReqParams); 
  wdiEventData.pCBfnc          = wdiSetLinkStateRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_SetLinkStateReq*/


/**
 @brief WDI_GetStatsReq will be called when the upper MAC wants 
        to get statistics (MIB counters) from the device. Upon
        the call of this API the WLAN DAL will pack and send a
        HAL Start request message to the lower RIVA sub-system
        if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_Start must have been called.

 @param wdiGetStatsReqParams: the stats parameters to get as 
                      specified by the Device Interface
  
        wdiGetStatsRspCb: callback for passing back the response
        of the get stats operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_Start
 @return Result of the function call
*/
WDI_Status 
WDI_GetStatsReq
(
  WDI_GetStatsReqParamsType* pwdiGetStatsReqParams,
  WDI_GetStatsRspCb          wdiGetStatsRspCb,
  void*                      pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_GET_STATS_REQ;
  wdiEventData.pEventData      = pwdiGetStatsReqParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiGetStatsReqParams); 
  wdiEventData.pCBfnc          = wdiGetStatsRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_GetStatsReq*/


/**
 @brief WDI_UpdateCfgReq will be called when the upper MAC when 
        it wishes to change the configuration of the WLAN
        Device. Upon the call of this API the WLAN DAL will pack
        and send a HAL Update CFG request message to the lower
        RIVA sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_Start must have been called.

 @param wdiUpdateCfgReqParams: the update cfg parameters as 
                      specified by the Device Interface
  
        wdiUpdateCfgsRspCb: callback for passing back the
        response of the update cfg operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_Start
 @return Result of the function call
*/
WDI_Status 
WDI_UpdateCfgReq
(
  WDI_UpdateCfgReqParamsType* pwdiUpdateCfgReqParams,
  WDI_UpdateCfgRspCb          wdiUpdateCfgsRspCb,
  void*                       pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_UPDATE_CFG_REQ;
  wdiEventData.pEventData      = pwdiUpdateCfgReqParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiUpdateCfgReqParams); 
  wdiEventData.pCBfnc          = wdiUpdateCfgsRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_UpdateCfgReq*/



/**
 @brief WDI_AddBAReq will be called when the upper MAC has setup
        successfully a BA session and needs to notify the HW for
        the appropriate settings to take place. Upon the call of
        this API the WLAN DAL will pack and send a HAL Add BA
        request message to the lower RIVA sub-system if DAL is
        in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param wdiAddBAReqParams: the add BA parameters as specified by
                      the Device Interface
  
        wdiAddBARspCb: callback for passing back the response of
        the add BA operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_AddBAReq
(
  WDI_AddBAReqParamsType* pwdiAddBAReqParams,
  WDI_AddBARspCb          wdiAddBARspCb,
  void*                   pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_ADD_BA_REQ;
  wdiEventData.pEventData      = pwdiAddBAReqParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiAddBAReqParams); 
  wdiEventData.pCBfnc          = wdiAddBARspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_AddBAReq*/


/**
 @brief WDI_TriggerBAReq will be called when the upper MAC has setup
        successfully a BA session and needs to notify the HW for
        the appropriate settings to take place. Upon the call of
        this API the WLAN DAL will pack and send a HAL Add BA
        request message to the lower RIVA sub-system if DAL is
        in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param wdiAddBAReqParams: the add BA parameters as specified by
                      the Device Interface
  
        wdiAddBARspCb: callback for passing back the response of
        the add BA operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_TriggerBAReq
(
  WDI_TriggerBAReqParamsType* pwdiTriggerBAReqParams,
  WDI_TriggerBARspCb          wdiTriggerBARspCb,
  void*                       pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_TRIGGER_BA_REQ;
  wdiEventData.pEventData      = pwdiTriggerBAReqParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiTriggerBAReqParams); 
  wdiEventData.pCBfnc          = wdiTriggerBARspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_AddBAReq*/

/**
 @brief WDI_UpdateBeaconParamsReq will be called when the upper MAC 
        wishes to update any of the Beacon parameters used by HW.
        Upon the call of this API the WLAN DAL will pack and send a HAL Update Beacon Params request
        message to the lower RIVA sub-system if DAL is in state
        STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param wdiUpdateBeaconParams: the Beacon parameters as specified 
                      by the Device Interface
  
        wdiUpdateBeaconParamsRspCb: callback for passing back the
        response of the start operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_UpdateBeaconParamsReq
(
  WDI_UpdateBeaconParamsType*    pwdiUpdateBeaconParams,
  WDI_UpdateBeaconParamsRspCb    wdiUpdateBeaconParamsRspCb,
  void*                        pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_UPD_BCON_PRMS_REQ;
  wdiEventData.pEventData      = pwdiUpdateBeaconParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiUpdateBeaconParams); 
  wdiEventData.pCBfnc          = wdiUpdateBeaconParamsRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_UpdateBeaconParamsReq*/

/**
 @brief WDI_SendBeaconParamsReq will be called when the upper MAC 
        wishes to update  the Beacon template used by HW.
        Upon the call of this API the WLAN DAL will pack and send a HAL Update Beacon template request
        message to the lower RIVA sub-system if DAL is in state
        STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param wdiSendBeaconParams: the Beacon parameters as specified 
                      by the Device Interface
  
        wdiSendBeaconParamsRspCb: callback for passing back the
        response of the start operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_SendBeaconParamsReq
(
  WDI_SendBeaconParamsType*    pwdiSendBeaconParams,
  WDI_SendBeaconParamsRspCb    wdiSendBeaconParamsRspCb,
  void*                        pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_SND_BCON_REQ;
  wdiEventData.pEventData      = pwdiSendBeaconParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiSendBeaconParams); 
  wdiEventData.pCBfnc          = wdiSendBeaconParamsRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_SendBeaconParamsReq*/

/**
 @brief WDI_UpdateProbeRspTemplateReq will be called when the 
        upper MAC wants to update the probe response template to
        be transmitted as Soft AP
         Upon the call of this API the WLAN DAL will
        pack and send the probe rsp template  message to the
        lower RIVA sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 


 @param pwdiUpdateProbeRspParams: the Update Beacon parameters as 
                      specified by the Device Interface
  
        wdiSendBeaconParamsRspCb: callback for passing back the
        response of the Send Beacon Params operation received
        from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_AddBAReq
 @return Result of the function call
*/

WDI_Status 
WDI_UpdateProbeRspTemplateReq
(
  WDI_UpdateProbeRspTemplateParamsType*    pwdiUpdateProbeRspParams,
  WDI_UpdateProbeRspTemplateRspCb          wdiUpdateProbeRspParamsRspCb,
  void*                                    pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_UPD_PROBE_RSP_TEMPLATE_REQ;
  wdiEventData.pEventData      = pwdiUpdateProbeRspParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiUpdateProbeRspParams); 
  wdiEventData.pCBfnc          = wdiUpdateProbeRspParamsRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_UpdateProbeRspTemplateReq*/

/**
 @brief WDI_NvDownloadReq will be called by the UMAC to download the NV blob
        to the NV memory.


 @param wdiNvDownloadReqParams: the NV Download parameters as specified by
                      the Device Interface
  
        wdiNvDownloadRspCb: callback for passing back the response of
        the NV Download operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_NvDownloadReq
(
  WDI_NvDownloadReqParamsType* pwdiNvDownloadReqParams,
  WDI_NvDownloadRspCb        wdiNvDownloadRspCb,
  void*                   pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  
  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_NV_DOWNLOAD_REQ;            
  wdiEventData.pEventData      = (void *)pwdiNvDownloadReqParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiNvDownloadReqParams); 
  wdiEventData.pCBfnc          = wdiNvDownloadRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_START_EVENT, &wdiEventData);

}/*WDI_NVDownloadReq*/

#ifdef WLAN_FEATURE_P2P
/**
 @brief WDI_SetP2PGONOAReq will be called when the 
        upper MAC wants to send Notice of Absence
         Upon the call of this API the WLAN DAL will
        pack and send the probe rsp template  message to the
        lower RIVA sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 


 @param pwdiUpdateProbeRspParams: the Update Beacon parameters as 
                      specified by the Device Interface
  
        wdiSendBeaconParamsRspCb: callback for passing back the
        response of the Send Beacon Params operation received
        from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_AddBAReq
 @return Result of the function call
*/
WDI_Status
WDI_SetP2PGONOAReq
(
  WDI_SetP2PGONOAReqParamsType*    pwdiP2PGONOAReqParams,
  WDI_SetP2PGONOAReqParamsRspCb    wdiP2PGONOAReqParamsRspCb,
  void*                            pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_P2P_GO_NOTICE_OF_ABSENCE_REQ;
  wdiEventData.pEventData      = pwdiP2PGONOAReqParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiP2PGONOAReqParams); 
  wdiEventData.pCBfnc          = wdiP2PGONOAReqParamsRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_SetP2PGONOAReq*/
#endif

/**
 @brief WDI_AddSTASelfReq will be called when the 
        UMAC wanted to add STA self while opening any new session
        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 


 @param pwdiAddSTASelfParams: the add sta self parameters as 
                      specified by the Device Interface
  
        pUserData: user data will be passed back with the
        callback 
  
 @see 
 @return Result of the function call
*/
WDI_Status
WDI_AddSTASelfReq
(
  WDI_AddSTASelfReqParamsType* pwdiAddSTASelfReqParams,
  WDI_AddSTASelfParamsRspCb    wdiAddSTASelfReqParamsRspCb,
  void*                        pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_ADD_STA_SELF_REQ;
  wdiEventData.pEventData      = pwdiAddSTASelfReqParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiAddSTASelfReqParams); 
  wdiEventData.pCBfnc          = wdiAddSTASelfReqParamsRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_AddSTASelfReq*/


#ifdef WLAN_FEATURE_VOWIFI_11R 
/**
 @brief WDI_AggrAddTSReq will be called when the upper MAC to inform
        the device of a successful add TSpec negotiation. HW
        needs to receive the TSpec Info from the UMAC in order
        to configure properly the QoS data traffic. Upon the
        call of this API the WLAN DAL will pack and send a HAL
        Add TS request message to the lower RIVA sub-system if
        DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param wdiAddTsReqParams: the add TS parameters as specified by
                      the Device Interface
  
        wdiAddTsRspCb: callback for passing back the response of
        the add TS operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_AggrAddTSReq
(
  WDI_AggrAddTSReqParamsType* pwdiAggrAddTsReqParams,
  WDI_AggrAddTsRspCb          wdiAggrAddTsRspCb,
  void*                   pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_AGGR_ADD_TS_REQ;
  wdiEventData.pEventData      = pwdiAggrAddTsReqParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiAggrAddTsReqParams); 
  wdiEventData.pCBfnc          = wdiAggrAddTsRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_AggrAddTSReq*/

#endif /* WLAN_FEATURE_VOWIFI_11R */

#ifdef ANI_MANF_DIAG
/**
 @brief WDI_FTMCommandReq
        Post FTM Command Event
 
 @param  ftmCommandReq:   FTM Command Body 
 @param  ftmCommandRspCb: FTM Response from HAL CB 
 @param  pUserData:       Client Data
  
 @see
 @return Result of the function call
*/
WDI_Status 
WDI_FTMCommandReq
(
  WDI_FTMCommandReqType *ftmCommandReq,
  WDI_FTMCommandRspCb    ftmCommandRspCb,
  void                  *pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest     = WDI_FTM_CMD_REQ;
  wdiEventData.pEventData     = (void *)ftmCommandReq;
  wdiEventData.uEventDataSize = ftmCommandReq->bodyLength + sizeof(wpt_uint32);
  wdiEventData.pCBfnc         = ftmCommandRspCb;
  wdiEventData.pUserData      = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);
}
#endif /* ANI_MANF_DIAG */ 
/**
 @brief WDI_HostResumeReq will be called 

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 


 @param pwdiResumeReqParams:  as specified by
                      the Device Interface
  
        wdiResumeReqRspCb: callback for passing back the response of
        the  Resume Req received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see 
 @return Result of the function call
*/
WDI_Status 
WDI_HostResumeReq
(
  WDI_ResumeParamsType*            pwdiResumeReqParams,
  WDI_HostResumeEventRspCb         wdiResumeReqRspCb,
  void*                            pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_HOST_RESUME_REQ;
  wdiEventData.pEventData      = pwdiResumeReqParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiResumeReqParams); 
  wdiEventData.pCBfnc          = wdiResumeReqRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_HostResumeReq*/

/**
 @brief WDI_DelSTASelfReq will be called 

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 


 @param pwdiDelStaSelfReqParams:  as specified by
                      the Device Interface
  
        wdiDelStaSelfRspCb: callback for passing back the response of
        the add TS operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_DelSTASelfReq
(
  WDI_DelSTASelfReqParamsType*      pwdiDelStaSelfReqParams,
  WDI_DelSTASelfRspCb               wdiDelStaSelfRspCb,
  void*                             pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_DEL_STA_SELF_REQ;
  wdiEventData.pEventData      = pwdiDelStaSelfReqParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiDelStaSelfReqParams); 
  wdiEventData.pCBfnc          = wdiDelStaSelfRspCb; 
  wdiEventData.pUserData       = pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_AggrAddTSReq*/

/**
 @brief WDI_SetTxPerTrackingReq will be called when the upper MAC 
        wants to set the Tx Per Tracking configurations. 
        Upon the call of this API the WLAN DAL will pack
        and send a HAL Set Tx Per Tracking request message to the
        lower RIVA sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 @param pwdiSetTxPerTrackingReqParams: the Set Tx PER Tracking configurations as 
                      specified by the Device Interface
  
        pwdiSetTxPerTrackingRspCb: callback for passing back the
        response of the set Tx PER Tracking configurations operation received
        from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @return Result of the function call
*/
WDI_Status 
WDI_SetTxPerTrackingReq
(
  WDI_SetTxPerTrackingReqParamsType*      pwdiSetTxPerTrackingReqParams,
  WDI_SetTxPerTrackingRspCb               pwdiSetTxPerTrackingRspCb,
  void*                                   pUserData
)
{
   WDI_EventInfoType      wdiEventData;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*------------------------------------------------------------------------
     Sanity Check 
   ------------------------------------------------------------------------*/
   if ( eWLAN_PAL_FALSE == gWDIInitialized )
   {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WDI API call before module is initialized - Fail request");

     return WDI_STATUS_E_NOT_ALLOWED; 
   }

   /*------------------------------------------------------------------------
     Fill in Event data and post to the Main FSM
   ------------------------------------------------------------------------*/
   wdiEventData.wdiRequest      = WDI_SET_TX_PER_TRACKING_REQ;
   wdiEventData.pEventData      = pwdiSetTxPerTrackingReqParams; 
   wdiEventData.uEventDataSize  = sizeof(*pwdiSetTxPerTrackingReqParams);
   wdiEventData.pCBfnc          = pwdiSetTxPerTrackingRspCb; 
   wdiEventData.pUserData       = pUserData;

   return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_SetTxPerTrackingReq*/

/**
 @brief WDI_HostSuspendInd 
  
        Suspend Indication from the upper layer will be sent
        down to HAL
  
 @param WDI_SuspendResumeIndParamsType
 
 @see 
  
 @return Status of the request
*/
WDI_Status 
WDI_HostSuspendInd
(
  WDI_SuspendParamsType*    pwdiSuspendIndParams
)
{

  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest      = WDI_HOST_SUSPEND_IND;
  wdiEventData.pEventData      = pwdiSuspendIndParams; 
  wdiEventData.uEventDataSize  = sizeof(*pwdiSuspendIndParams); 
  wdiEventData.pCBfnc          = NULL; 
  wdiEventData.pUserData       = NULL;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);

}/*WDI_HostSuspendInd*/

/**
 @brief WDI_HALDumpCmdReq
        Post HAL DUMP Command Event
 
 @param  halDumpCmdReqParams:   Hal Dump Command Body 
 @param  halDumpCmdRspCb: HAL DUMP Response from HAL CB 
 @param  pUserData:       Client Data
  
 @see
 @return Result of the function call
*/
WDI_Status WDI_HALDumpCmdReq
(
  WDI_HALDumpCmdReqParamsType *halDumpCmdReqParams,
  WDI_HALDumpCmdRspCb    halDumpCmdRspCb,
  void                  *pUserData
)
{
  WDI_EventInfoType      wdiEventData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Fill in Event data and post to the Main FSM
  ------------------------------------------------------------------------*/
  wdiEventData.wdiRequest     =   WDI_HAL_DUMP_CMD_REQ;
  wdiEventData.pEventData     =   (void *)halDumpCmdReqParams;
  wdiEventData.uEventDataSize =   sizeof(WDI_HALDumpCmdReqParamsType);
  wdiEventData.pCBfnc         =   halDumpCmdRspCb;
  wdiEventData.pUserData      =   pUserData;

  return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);
}

/*============================================================================ 
 
            DAL Control Path Main FSM Function Implementation
 
 ============================================================================*/

/**
 @brief Main FSM Start function for all states except BUSY

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         wdiEV:           event posted to the main DAL FSM
         pEventData:      pointer to the event information
         structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_PostMainEvent
(
  WDI_ControlBlockType*  pWDICtx, 
  WDI_MainEventType      wdiEV, 
  WDI_EventInfoType*     pEventData
  
)
{
  WDI_Status         wdiStatus; 
  WDI_MainFuncType   pfnWDIMainEvHdlr; 
  WDI_MainStateType  wdiOldState; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
     Sanity check
   -------------------------------------------------------------------------*/
  if (( pWDICtx->uGlobalState >= WDI_MAX_ST ) ||
      ( wdiEV >= WDI_MAX_EVENT ))
  {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "Invalid state or event in Post Main Ev function ST: %d EV: %d",
               pWDICtx->uGlobalState, wdiEV);
     return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*Access to the global state must be locked */
  wpalMutexAcquire(&pWDICtx->wptMutex);

  /*Fetch event handler for state*/
  pfnWDIMainEvHdlr = wdiMainFSM[pWDICtx->uGlobalState].pfnMainTbl[wdiEV]; 

  wdiOldState = pWDICtx->uGlobalState;

  /*
  --Incase of WDI event is WDI_RESPONSE_EVENT and this is called when a 
  response comes from CCPU for the request sent by host: 
  the WDI global state will be in WDI_BUSY_ST already, so do not set it to BUSY again. 
  This state will be set to WDI_STARTED_ST in WDI_MainRsp, if it is a expected response.
  --Incase of WDI event is WDI_RESPONSE_EVENT and it is an indication from the 
  CCPU:
  don't change the state */
  if ( WDI_RESPONSE_EVENT != wdiEV)
  {
    /*Transition to BUSY State - the request is now being processed by the FSM,
     if the request fails we shall transition back to the old state, if not
     the request will manage its own state transition*/
    WDI_STATE_TRANSITION( pWDICtx, WDI_BUSY_ST);
  }
  /* If the state function associated with the EV is NULL it means that this
     event is not allowed in this state*/
  if ( NULL != pfnWDIMainEvHdlr ) 
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
              "Posting event %d in state: %d to the Main FSM", 
              wdiEV, wdiOldState);
    wdiStatus = pfnWDIMainEvHdlr( pWDICtx, pEventData); 
  }
  else
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Unexpected event %d in state: %d", 
              wdiEV, wdiOldState);
    wdiStatus = WDI_STATUS_E_NOT_ALLOWED; 
  }

  /* If a request handles itself well it will end up in a success or in a
     pending
     Success - means that the request was processed and the proper state
     transition already occurred or will occur when the resp is received
     - NO other state transition or dequeueing is required
 
     Pending - means the request could not be processed at this moment in time
     because the FSM was already busy so no state transition or dequeueing
     is necessary anymore
 
     Success for synchronous case means that the transition may occur and
     processing of pending requests may continue - so it should go through
     and restores the state and continue processing queued requests*/
  if (( WDI_STATUS_SUCCESS != wdiStatus )&&
      ( WDI_STATUS_PENDING != wdiStatus ))
  {
    if ( WDI_RESPONSE_EVENT != wdiEV)
    {
      /*The request has failed or could not be processed - transition back to
        the old state - check to see if anything was queued and try to execute
        The dequeue logic should post a message to a thread and return - no
        actual processing can occur */
      WDI_STATE_TRANSITION( pWDICtx, wdiOldState);
    }
    WDI_DequeuePendingReq(pWDICtx);
        
  }

  /* we have completed processing the event */
  wpalMutexRelease(&pWDICtx->wptMutex);

  return wdiStatus; 

}/*WDI_PostMainEvent*/


/*--------------------------------------------------------------------------
  INIT State Functions 
--------------------------------------------------------------------------*/
/**
 @brief Main FSM Start function for all states except BUSY

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_MainStart
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{

  /*--------------------------------------------------------------------
     Sanity Check 
  ----------------------------------------------------------------------*/
  if (( NULL ==  pWDICtx ) || ( NULL == pEventData ))
  {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
               "Invalid parameters on Main Start %x %x", 
               pWDICtx, pEventData);
     return WDI_STATUS_E_FAILURE;
  }

  wpalMutexAcquire(&pWDICtx->wptMutex);

  /*--------------------------------------------------------------------
     Check if the Control Transport has been opened 
  ----------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == pWDICtx->bCTOpened )
  {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
               "Control Transport not yet Open - queueing the request");

     WDI_STATE_TRANSITION( pWDICtx, WDI_INIT_ST);
     WDI_QueuePendingReq( pWDICtx, pEventData); 

     wpalMutexRelease(&pWDICtx->wptMutex);
     return WDI_STATUS_PENDING;
  }
 
  wpalMutexRelease(&pWDICtx->wptMutex);

  /*Return Success*/
  return WDI_ProcessRequest( pWDICtx, pEventData );

}/*WDI_MainStart*/

/**
 @brief Main FSM Response function for state INIT

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_MainRspInit
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  /*------------------------------------------------------------------------
    Not expecting a response from the device before it is started 
  ------------------------------------------------------------------------*/
  WDI_ASSERT(0); 

  /*Return Success*/
  return WDI_STATUS_E_NOT_ALLOWED;
}/* WDI_MainRspInit */

/**
 @brief Main FSM Close function for all states except BUSY

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_MainClose
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{

  /*--------------------------------------------------------------------
     Sanity Check 
  ----------------------------------------------------------------------*/
  if (( NULL ==  pWDICtx ) || ( NULL == pEventData ))
  {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
               "Invalid parameters on Main Close %x %x", 
               pWDICtx, pEventData);
     return WDI_STATUS_E_FAILURE;
  }

  /*Return Success*/
  return WDI_ProcessRequest( pWDICtx, pEventData );

}/*WDI_MainClose*/
/*--------------------------------------------------------------------------
  STARTED State Functions 
--------------------------------------------------------------------------*/
/**
 @brief Main FSM Start function for state STARTED

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_MainStartStarted
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_StartRspCb           wdiStartRspCb = NULL;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*--------------------------------------------------------------------
     Sanity Check 
  ----------------------------------------------------------------------*/
  if (( NULL ==  pWDICtx ) || ( NULL == pEventData ))
  {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
               "Invalid parameters on Main Start %x %x", 
               pWDICtx, pEventData);
     return WDI_STATUS_E_FAILURE;
  }

  /*--------------------------------------------------------------------
     Nothing to do transport was already started 
  ----------------------------------------------------------------------*/
  WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
    "Received start while transport was already started - nothing to do"); 

  wpalMutexAcquire(&pWDICtx->wptMutex);

  /*Transition back to started because the post function transitioned us to
    busy*/
  WDI_STATE_TRANSITION( pWDICtx, WDI_STARTED_ST);

  /*Check to see if any request is pending*/
  WDI_DequeuePendingReq(pWDICtx);
  
  wpalMutexRelease(&pWDICtx->wptMutex);

  /*Tell UMAC Success*/
  wdiStartRspCb = (WDI_StartRspCb)pEventData->pCBfnc; 
  
   /*Notify UMAC*/
  wdiStartRspCb( &pWDICtx->wdiCachedStartRspParams, pWDICtx->pRspCBUserData);

  /*Return Success*/
  return WDI_STATUS_SUCCESS;

}/*WDI_MainStartStarted*/

/**
 @brief Main FSM Stop function for state STARTED

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_MainStopStarted
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  /*--------------------------------------------------------------------
     Sanity Check 
  ----------------------------------------------------------------------*/
  if (( NULL ==  pWDICtx ) || ( NULL == pEventData ))
  {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "Invalid parameters on Main Start %x %x", 
               pWDICtx, pEventData);
     return WDI_STATUS_E_FAILURE;
  }

  /*State at this point is BUSY - because we enter this state before posting
    an event to the FSM in order to prevent potential race conditions*/

  WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
            "Processing stop request in FSM");

  /*Return Success*/
  return WDI_ProcessRequest( pWDICtx, pEventData );

}/*WDI_MainStopStarted*/
/**
 @brief Main FSM Request function for state started

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_MainReqStarted
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{

  /*--------------------------------------------------------------------
     Sanity Check 
  ----------------------------------------------------------------------*/
  if (( NULL ==  pWDICtx ) || ( NULL == pEventData ))
  {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
               "Invalid parameters on Main Req Started %x %x", 
               pWDICtx, pEventData);
     return WDI_STATUS_E_FAILURE;
  }

  /*State at this point is BUSY - because we enter this state before posting
    an event to the FSM in order to prevent potential race conditions*/

  /*Return Success*/
  return WDI_ProcessRequest( pWDICtx, pEventData );

}/*WDI_MainReqStarted*/

/**
 @brief Main FSM Response function for all states except INIT

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_MainRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status  wdiStatus; 
  wpt_boolean expectedResponse;

  /*--------------------------------------------------------------------
     Sanity Check 
  ----------------------------------------------------------------------*/
  if (( NULL ==  pWDICtx ) || ( NULL == pEventData ))
  {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
               "Invalid parameters on Main Response %x %x", 
               pWDICtx, pEventData);
     return WDI_STATUS_E_FAILURE;
  }

  if ( pEventData->wdiResponse ==  pWDICtx->wdiExpectedResponse )
  {
    /* we received an expected response */
    expectedResponse = eWLAN_PAL_TRUE;

    /*We expect that we will transition to started after this processing*/
    pWDICtx->ucExpectedStateTransition = WDI_STARTED_ST;

    /* we are no longer expecting a response */
     pWDICtx->wdiExpectedResponse = WDI_MAX_RESP;
  }
  else
  {
    /* we received an indication or unexpected response */
    expectedResponse = eWLAN_PAL_FALSE;

    /* for indications no need to update state from what it is right
       now, unless it explicitly does it in the indication handler (say
       for device failure ind) */
    pWDICtx->ucExpectedStateTransition = pWDICtx->uGlobalState;
  }

  /*Process the response */
  wdiStatus = WDI_ProcessResponse( pWDICtx, pEventData );

  /*Lock the CB as we are about to do a state transition*/
  wpalMutexAcquire(&pWDICtx->wptMutex);

  /*Transition to the expected state after the response processing
  - this should always be started state with the following exceptions:
  1. processing of a failed start response
  2. device failure detected while processing response
  3. stop response received*/
  WDI_STATE_TRANSITION( pWDICtx, pWDICtx->ucExpectedStateTransition);
 
  /*Dequeue request that may have been queued while we were waiting for the
    response */
  if ( expectedResponse )
  {
     WDI_DequeuePendingReq(pWDICtx); 
  }

  wpalMutexRelease(&pWDICtx->wptMutex);

  /*Return Success - always */
  return WDI_STATUS_SUCCESS; 

}/*WDI_MainRsp*/

/*--------------------------------------------------------------------------
  STOPPED State Functions 
--------------------------------------------------------------------------*/
/**
 @brief Main FSM Stop function for state STOPPED

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_MainStopStopped
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  /*--------------------------------------------------------------------
     Sanity Check 
  ----------------------------------------------------------------------*/
  if (( NULL ==  pWDICtx ) || ( NULL == pEventData ))
  {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "Invalid parameters on Main Stop Stopped %x %x", 
               pWDICtx, pEventData);
     return WDI_STATUS_E_FAILURE;
  }

  /*We should normally not get a STOP request if we are already stopped
    since we should normally be stopped by the UMAC.  However in some
    error situations we put ourselves in the stopped state without the
    UMAC knowing, so when we get a STOP request in this state we still
    process it since we need to clean up the underlying state */
  WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
            "Processing stop request while stopped in FSM");

  /*Return Success*/
  return WDI_ProcessRequest( pWDICtx, pEventData );

}/*WDI_MainStopStopped*/

/*--------------------------------------------------------------------------
  BUSY State Functions 
--------------------------------------------------------------------------*/
/**
 @brief Main FSM Start function for state BUSY

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_MainStartBusy
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  /*--------------------------------------------------------------------
     Sanity Check 
  ----------------------------------------------------------------------*/
  if (( NULL ==  pWDICtx ) || ( NULL == pEventData ))
  {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
               "Invalid parameters on Main Start in BUSY %x %x", 
               pWDICtx, pEventData);
     return WDI_STATUS_E_FAILURE;
  }

  /*--------------------------------------------------------------------
     Check if the Control Transport has been opened 
  ----------------------------------------------------------------------*/
  WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
           "WDI Busy state - queue start request");

  /*Queue the start request*/
  WDI_QueuePendingReq( pWDICtx, pEventData); 

  /*Return Success*/
  return WDI_STATUS_PENDING;
}/*WDI_MainStartBusy*/

/**
 @brief Main FSM Stop function for state BUSY

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_MainStopBusy
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  /*--------------------------------------------------------------------
     Sanity Check 
  ----------------------------------------------------------------------*/
  if (( NULL ==  pWDICtx ) || ( NULL == pEventData ))
  {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
               "Invalid parameters on Main Stop in BUSY %x %x", 
               pWDICtx, pEventData);
     return WDI_STATUS_E_FAILURE;
  }

  /*--------------------------------------------------------------------
     Check if the Control Transport has been opened 
  ----------------------------------------------------------------------*/
  WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
           "WDI Busy state - queue stop request");

  WDI_QueuePendingReq( pWDICtx, pEventData); 
  return WDI_STATUS_PENDING;
  
}/*WDI_MainStopBusy*/

/**
 @brief Main FSM Request function for state BUSY

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_MainReqBusy
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  /*--------------------------------------------------------------------
     Sanity Check 
  ----------------------------------------------------------------------*/
  if (( NULL ==  pWDICtx ) || ( NULL == pEventData ))
  {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
               "Invalid parameters on Main Request in BUSY %x %x", 
               pWDICtx, pEventData);
     return WDI_STATUS_E_FAILURE;
  }

  /*--------------------------------------------------------------------
     Check if the Control Transport has been opened 
  ----------------------------------------------------------------------*/
  WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
           "WDI Busy state - queue request %d because waiting for response %d",
             pEventData->wdiRequest, pWDICtx->wdiExpectedResponse);

  WDI_QueuePendingReq( pWDICtx, pEventData); 
  return WDI_STATUS_PENDING;
  
}/*WDI_MainReqBusy*/
/**
 @brief Main FSM Close function for state BUSY

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_MainCloseBusy
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  /*--------------------------------------------------------------------
     Sanity Check 
  ----------------------------------------------------------------------*/
  if (( NULL ==  pWDICtx ) || ( NULL == pEventData ))
  {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
               "Invalid parameters on Main Close in BUSY %x %x", 
               pWDICtx, pEventData);
     return WDI_STATUS_E_FAILURE;
  }

  /*--------------------------------------------------------------------
     Check if the Control Transport has been opened 
  ----------------------------------------------------------------------*/
  WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
           "WDI Busy state - queue close request");

  WDI_QueuePendingReq( pWDICtx, pEventData); 
  return WDI_STATUS_PENDING;
  
}/*WDI_MainCloseBusy*/

/**
 @brief Main FSM Shutdown function for INIT & STARTED states


 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_MainShutdown
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  /*--------------------------------------------------------------------
     Sanity Check
  ----------------------------------------------------------------------*/
  if (( NULL ==  pWDICtx ) || ( NULL == pEventData ))
  {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "Invalid parameters on Main Start %x %x",
               pWDICtx, pEventData);
     return WDI_STATUS_E_FAILURE;
  }

  /*State at this point is BUSY - because we enter this state before posting
    an event to the FSM in order to prevent potential race conditions*/

  WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_FATAL,
            "Processing shutdown request in FSM");

  /*Return Success*/
  return WDI_ProcessRequest( pWDICtx, pEventData );

}/*WDI_MainShutdown*/

/**
 @brief Main FSM Shutdown function for BUSY state


 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_MainShutdownBusy
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  /*--------------------------------------------------------------------
     Sanity Check
  ----------------------------------------------------------------------*/
  if (( NULL ==  pWDICtx ) || ( NULL == pEventData ))
  {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "Invalid parameters on Main Start %x %x",
               pWDICtx, pEventData);
     return WDI_STATUS_E_FAILURE;
  }

  /* If you are waiting for a HAL response at this stage, you are not
   * going to get it. Riva is already shutdown/crashed.
   */
  wpalTimerStop(&gWDICb.wptResponseTimer);

  WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_FATAL,
            "Processing shutdown request in FSM: Busy state ");

  return WDI_ProcessRequest( pWDICtx, pEventData );

}/*WDI_MainShutdownBusy*/


/*======================================================================= 
 
           WLAN DAL Control Path Main Processing Functions
 
*=======================================================================*/

/*========================================================================
          Main DAL Control Path Request Processing API 
========================================================================*/
/**
 @brief Process Start Request function (called when Main FSM 
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessStartReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_StartReqParamsType* pwdiStartParams    = NULL;
  WDI_StartRspCb          wdiStartRspCb      = NULL;
  wpt_uint8*              pSendBuffer        = NULL; 
  wpt_uint16              usDataOffset       = 0;
  wpt_uint16              usSendSize         = 0;

  tHalMacStartReqMsg      halStartReq; 
  wpt_uint16              usLen              = 0; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) ||
      ( NULL == (pwdiStartParams = (WDI_StartReqParamsType*)pEventData->pEventData)) ||
      ( NULL == (wdiStartRspCb   = (WDI_StartRspCb)pEventData->pCBfnc)))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_FATAL,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  /*-----------------------------------------------------------------------
    Get message buffer
  -----------------------------------------------------------------------*/
  usLen = sizeof(halStartReq.startReqParams) + 
          pwdiStartParams->usConfigBufferLen;

  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_START_REQ, 
                        usLen,
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset + usLen )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_FATAL,
              "Unable to get send buffer in start req %x %x %x",
                pEventData, pwdiStartParams, wdiStartRspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  /*-----------------------------------------------------------------------
    Fill in the message
  -----------------------------------------------------------------------*/
  halStartReq.startReqParams.driverType = 
     WDI_2_HAL_DRV_TYPE(pwdiStartParams->wdiDriverType); 

  halStartReq.startReqParams.uConfigBufferLen = 
                  pwdiStartParams->usConfigBufferLen; 
  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                  &halStartReq.startReqParams, 
                  sizeof(halStartReq.startReqParams)); 

  usDataOffset  += sizeof(halStartReq.startReqParams); 
  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                  pwdiStartParams->pConfigBuffer, 
                  pwdiStartParams->usConfigBufferLen); 

  pWDICtx->wdiReqStatusCB     = pwdiStartParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiStartParams->pUserData; 

  /*Save Low Level Ind CB and associated user data - it will be used further
    on when an indication is coming from the lower MAC*/
  pWDICtx->wdiLowLevelIndCB   = pwdiStartParams->wdiLowLevelIndCB;
  pWDICtx->pIndUserData       = pwdiStartParams->pIndUserData; 

  pWDICtx->bFrameTransEnabled = pwdiStartParams->bFrameTransEnabled; 
  /*-------------------------------------------------------------------------
    Send Start Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiStartRspCb, pEventData->pUserData, WDI_START_RESP);

  
}/*WDI_ProcessStartReq*/

/**
 @brief Process Stop Request function (called when Main FSM 
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessStopReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_StopReqParamsType* pwdiStopParams      = NULL;
  WDI_StopRspCb          wdiStopRspCb        = NULL;
  wpt_uint8*             pSendBuffer         = NULL; 
  wpt_uint16             usDataOffset        = 0;
  wpt_uint16             usSendSize          = 0;
  wpt_status             status;
  tHalMacStopReqMsg      halStopReq; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

 /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) ||
      ( NULL == (pwdiStopParams = (WDI_StopReqParamsType*)pEventData->pEventData)) ||
      ( NULL == (wdiStopRspCb   = (WDI_StopRspCb)pEventData->pCBfnc)))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  /*-----------------------------------------------------------------------
    Get message buffer
  -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_STOP_REQ, 
                        sizeof(halStopReq.stopReqParams),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset + sizeof(halStopReq.stopReqParams) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in stop req %x %x %x",
                pEventData, pwdiStopParams, wdiStopRspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  /*-----------------------------------------------------------------------
    Fill in the message
  -----------------------------------------------------------------------*/
  halStopReq.stopReqParams.reason = WDI_2_HAL_STOP_REASON(
                                          pwdiStopParams->wdiStopReason);

  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                  &halStopReq.stopReqParams, 
                  sizeof(halStopReq.stopReqParams)); 

  pWDICtx->wdiReqStatusCB     = pwdiStopParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiStopParams->pUserData; 

  /*! TO DO: stop the data services */
  if ( eDRIVER_TYPE_MFG != pWDICtx->driverMode )
  {
     /*Stop the STA Table !UT- check this logic again
      It is safer to do it here than on the response - because a stop is imminent*/
     WDI_STATableStop(pWDICtx);

     /* Reset the event to be not signalled */
     status = wpalEventReset(&pWDICtx->setPowerStateEvent);
     if (eWLAN_PAL_STATUS_SUCCESS != status)
     {
        WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "WDI Init failed to reset power state event");

        WDI_ASSERT(0); 
        return VOS_STATUS_E_FAILURE;
     }
     /* Stop Transport Driver, DXE */
     WDTS_SetPowerState(pWDICtx, WDTS_POWER_STATE_DOWN, WDI_SetPowerStateCb);
     /*
      * Wait for the event to be set once the ACK comes back from DXE 
      */
     status = wpalEventWait(&pWDICtx->setPowerStateEvent, 
                            WDI_SET_POWER_STATE_TIMEOUT);
     if (eWLAN_PAL_STATUS_SUCCESS != status)
     {
        WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "WDI Init failed to wait on an event");

        WDI_ASSERT(0); 
        return VOS_STATUS_E_FAILURE;
      }
  }

  /*-------------------------------------------------------------------------
    Send Stop Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiStopRspCb, pEventData->pUserData, WDI_STOP_RESP);

}/*WDI_ProcessStopReq*/

/**
 @brief Process Close Request function (called when Main FSM 
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessCloseReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   wpt_status              wptStatus; 
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*Lock control block for cleanup*/
   wpalMutexAcquire(&pWDICtx->wptMutex);
       
   /*Clear all pending request*/
   WDI_ClearPendingRequests(pWDICtx);

   /* Close Control transport*/
   WCTS_CloseTransport(pWDICtx->wctsHandle); 

   /* Close Data transport*/
   /* FTM mode does not open Data Path */
   if ( eDRIVER_TYPE_MFG != pWDICtx->driverMode )
   {
      WDTS_Close(pWDICtx);
   }

   /*Close the STA Table !UT- check this logic again*/
   WDI_STATableClose(pWDICtx);

   /*close the PAL */
   wptStatus = wpalClose(pWDICtx->pPALContext);
   if ( eWLAN_PAL_STATUS_SUCCESS !=  wptStatus )
   {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "Failed to wpal Close %d", wptStatus);
     WDI_ASSERT(0);
   }

   /*Transition back to init state*/
   WDI_STATE_TRANSITION( pWDICtx, WDI_INIT_ST);

   wpalMutexRelease(&pWDICtx->wptMutex);

   /*Make sure the expected state is properly defaulted to Init*/
   pWDICtx->ucExpectedStateTransition = WDI_INIT_ST; 

   return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessCloseReq*/


/*===========================================================================
                  SCANING REQUEST PROCESSING API 
===========================================================================*/

/**
 @brief Process Init Scan Request function (called when Main FSM
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessInitScanReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_InitScanReqParamsType*  pwdiInitScanParams    = NULL;
  WDI_InitScanRspCb           wdiInitScanRspCb      = NULL;
  wpt_uint8*                  pSendBuffer           = NULL; 
  wpt_uint16                  usDataOffset          = 0;
  wpt_uint16                  usSendSize            = 0;
  wpt_uint8                   i = 0;

  tHalInitScanReqMsg          halInitScanReqMsg;

  /*This is temporary fix. 
   * It shold be removed once host and riva changes are in sync*/
  tHalInitScanConReqMsg       halInitScanConReqMsg;

  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
    -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) ||
      ( NULL == (pwdiInitScanParams = (WDI_InitScanReqParamsType*)pEventData->pEventData)) ||
      ( NULL == (wdiInitScanRspCb   = (WDI_InitScanRspCb)pEventData->pCBfnc)))
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
        "%s: Invalid parameters", __FUNCTION__);
    WDI_ASSERT(0);
    return WDI_STATUS_E_FAILURE; 
  }

#if 0
  wpalMutexAcquire(&pWDICtx->wptMutex);
  /*-----------------------------------------------------------------------
    Check to see if SCAN is already in progress - if so reject the req
    We only allow one scan at a time
    ! TO DO: - revisit this constraint 
    -----------------------------------------------------------------------*/
  if ( pWDICtx->bScanInProgress )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_ERROR,
        "Scan is already in progress - subsequent scan is not allowed"
        " until the first scan completes");

    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  pWDICtx->bScanInProgress = eWLAN_PAL_TRUE; 
  pWDICtx->uScanState      = WDI_SCAN_INITIALIZED_ST; 

  wpalMutexRelease(&pWDICtx->wptMutex);
#endif

  if (pwdiInitScanParams->wdiReqInfo.bUseNOA)
  {

    /*This is temporary fix. 
     * It shold be removed once host and riva changes are in sync*/
    /*-----------------------------------------------------------------------
      Get message buffer
      -----------------------------------------------------------------------*/
    if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_INIT_SCAN_CON_REQ, 
            sizeof(halInitScanConReqMsg.initScanParams),
            &pSendBuffer, &usDataOffset, &usSendSize))||
        ( usSendSize < (usDataOffset + sizeof(halInitScanConReqMsg.initScanParams) )))
    {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
          "Unable to get send buffer in init scan req %x %x %x",
          pEventData, pwdiInitScanParams, wdiInitScanRspCb);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
    }


    /*-----------------------------------------------------------------------
      Fill in the message
      -----------------------------------------------------------------------*/
    halInitScanConReqMsg.initScanParams.scanMode = 
      WDI_2_HAL_SCAN_MODE(pwdiInitScanParams->wdiReqInfo.wdiScanMode);

    wpalMemoryCopy(halInitScanConReqMsg.initScanParams.bssid,
        pwdiInitScanParams->wdiReqInfo.macBSSID, WDI_MAC_ADDR_LEN);

    halInitScanConReqMsg.initScanParams.notifyBss = 
      pwdiInitScanParams->wdiReqInfo.bNotifyBSS;
    halInitScanConReqMsg.initScanParams.frameType = 
      pwdiInitScanParams->wdiReqInfo.ucFrameType;
    halInitScanConReqMsg.initScanParams.frameLength = 
      pwdiInitScanParams->wdiReqInfo.ucFrameLength;

    WDI_CopyWDIMgmFrameHdrToHALMgmFrameHdr( &halInitScanConReqMsg.initScanParams.macMgmtHdr,
        &pwdiInitScanParams->wdiReqInfo.wdiMACMgmtHdr);

#ifdef WLAN_FEATURE_P2P
    halInitScanConReqMsg.initScanParams.useNoA = pwdiInitScanParams->wdiReqInfo.bUseNOA;
    halInitScanConReqMsg.initScanParams.scanDuration = pwdiInitScanParams->wdiReqInfo.scanDuration;
#endif

    halInitScanConReqMsg.initScanParams.scanEntry.activeBSScnt = 
      pwdiInitScanParams->wdiReqInfo.wdiScanEntry.activeBSScnt;

    for (i=0; i < pwdiInitScanParams->wdiReqInfo.wdiScanEntry.activeBSScnt; i++)
    {
      halInitScanConReqMsg.initScanParams.scanEntry.bssIdx[i] = 
        pwdiInitScanParams->wdiReqInfo.wdiScanEntry.bssIdx[i];
    }

    wpalMemoryCopy( pSendBuffer+usDataOffset, 
        &halInitScanConReqMsg.initScanParams, 
        sizeof(halInitScanConReqMsg.initScanParams)); 
  }
  else
  {
    /*-----------------------------------------------------------------------
      Get message buffer
      -----------------------------------------------------------------------*/
    if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_INIT_SCAN_REQ, 
            sizeof(halInitScanReqMsg.initScanParams),
            &pSendBuffer, &usDataOffset, &usSendSize))||
        ( usSendSize < (usDataOffset + sizeof(halInitScanReqMsg.initScanParams) )))
    {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
          "Unable to get send buffer in init scan req %x %x %x",
          pEventData, pwdiInitScanParams, wdiInitScanRspCb);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
    }


    /*-----------------------------------------------------------------------
      Fill in the message
      -----------------------------------------------------------------------*/
    halInitScanReqMsg.initScanParams.scanMode = 
      WDI_2_HAL_SCAN_MODE(pwdiInitScanParams->wdiReqInfo.wdiScanMode);

    wpalMemoryCopy(halInitScanReqMsg.initScanParams.bssid,
        pwdiInitScanParams->wdiReqInfo.macBSSID, WDI_MAC_ADDR_LEN);

    halInitScanReqMsg.initScanParams.notifyBss = 
      pwdiInitScanParams->wdiReqInfo.bNotifyBSS;
    halInitScanReqMsg.initScanParams.frameType = 
      pwdiInitScanParams->wdiReqInfo.ucFrameType;
    halInitScanReqMsg.initScanParams.frameLength = 
      pwdiInitScanParams->wdiReqInfo.ucFrameLength;

    WDI_CopyWDIMgmFrameHdrToHALMgmFrameHdr( &halInitScanReqMsg.initScanParams.macMgmtHdr,
        &pwdiInitScanParams->wdiReqInfo.wdiMACMgmtHdr);

    halInitScanReqMsg.initScanParams.scanEntry.activeBSScnt = 
      pwdiInitScanParams->wdiReqInfo.wdiScanEntry.activeBSScnt;

    for (i=0; i < pwdiInitScanParams->wdiReqInfo.wdiScanEntry.activeBSScnt; i++)
    {
      halInitScanReqMsg.initScanParams.scanEntry.bssIdx[i] = 
        pwdiInitScanParams->wdiReqInfo.wdiScanEntry.bssIdx[i];
    }

    wpalMemoryCopy( pSendBuffer+usDataOffset, 
        &halInitScanReqMsg.initScanParams, 
        sizeof(halInitScanReqMsg.initScanParams)); 
  }

  pWDICtx->wdiReqStatusCB     = pwdiInitScanParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiInitScanParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Init Scan Request to HAL 
    -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
      wdiInitScanRspCb, pEventData->pUserData, WDI_INIT_SCAN_RESP);

}/*WDI_ProcessInitScanReq*/

/**
 @brief Process Start Scan Request function (called when Main 
        FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessStartScanReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_StartScanReqParamsType*  pwdiStartScanParams    = NULL;
  WDI_StartScanRspCb           wdiStartScanRspCb      = NULL;
  wpt_uint8*                   pSendBuffer            = NULL; 
  wpt_uint16                   usDataOffset           = 0;
  wpt_uint16                   usSendSize             = 0;

  tHalStartScanReqMsg          halStartScanReqMsg; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) ||
      ( NULL == (pwdiStartScanParams = (WDI_StartScanReqParamsType*)pEventData->pEventData)) ||
      ( NULL == (wdiStartScanRspCb   = (WDI_StartScanRspCb)pEventData->pCBfnc)))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

#if 0
  wpalMutexAcquire(&pWDICtx->wptMutex);
  /*-----------------------------------------------------------------------
    Check to see if SCAN is already in progress - start scan is only
    allowed when a scan is ongoing and the state of the scan procedure
    is either init or end 
  -----------------------------------------------------------------------*/
  if (( !pWDICtx->bScanInProgress ) || 
      (( WDI_SCAN_INITIALIZED_ST != pWDICtx->uScanState ) &&
       ( WDI_SCAN_ENDED_ST != pWDICtx->uScanState )))
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Scan start not allowed in this state %d %d",
               pWDICtx->bScanInProgress, pWDICtx->uScanState);
    
    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  pWDICtx->uScanState      = WDI_SCAN_STARTED_ST; 

  wpalMutexRelease(&pWDICtx->wptMutex);
#endif

  /*-----------------------------------------------------------------------
    Get message buffer
  -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_START_SCAN_REQ, 
                        sizeof(halStartScanReqMsg.startScanParams),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset + sizeof(halStartScanReqMsg.startScanParams) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in start scan req %x %x %x",
                pEventData, pwdiStartScanParams, wdiStartScanRspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  halStartScanReqMsg.startScanParams.scanChannel = 
                              pwdiStartScanParams->ucChannel;
  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                  &halStartScanReqMsg.startScanParams, 
                  sizeof(halStartScanReqMsg.startScanParams)); 

  pWDICtx->wdiReqStatusCB     = pwdiStartScanParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiStartScanParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Start Scan Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiStartScanRspCb, pEventData->pUserData, WDI_START_SCAN_RESP);
}/*WDI_ProcessStartScanReq*/


/**
 @brief Process End Scan Request function (called when Main FSM 
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessEndScanReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_EndScanReqParamsType*  pwdiEndScanParams    = NULL;
  WDI_EndScanRspCb           wdiEndScanRspCb      = NULL;
  wpt_uint8*                 pSendBuffer          = NULL; 
  wpt_uint16                 usDataOffset         = 0;
  wpt_uint16                 usSendSize           = 0;

  tHalEndScanReqMsg          halEndScanReqMsg;           
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) ||
      ( NULL == (pwdiEndScanParams = (WDI_EndScanReqParamsType*)pEventData->pEventData)) ||
      ( NULL == (wdiEndScanRspCb   = (WDI_EndScanRspCb)pEventData->pCBfnc)))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  /* commenting this check as UMAC is sending END_SCAN_REQ after FINISH_SCAN 
  * sometimes  because of this check the scan request is not being 
  * forwarded to HAL and result in hang*/
#if 0
  wpalMutexAcquire(&pWDICtx->wptMutex);
  /*-----------------------------------------------------------------------
    Check to see if SCAN is already in progress - end scan is only
    allowed when a scan is ongoing and the state of the scan procedure
    is started
  -----------------------------------------------------------------------*/
  if (( !pWDICtx->bScanInProgress ) || 
      ( WDI_SCAN_STARTED_ST != pWDICtx->uScanState ))
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "End start not allowed in this state %d %d",
               pWDICtx->bScanInProgress, pWDICtx->uScanState);
    
    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  pWDICtx->uScanState      = WDI_SCAN_ENDED_ST; 

  wpalMutexRelease(&pWDICtx->wptMutex);
#endif

  /*-----------------------------------------------------------------------
    Get message buffer
  -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_END_SCAN_REQ, 
                        sizeof(halEndScanReqMsg.endScanParams),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset + sizeof(halEndScanReqMsg.endScanParams) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in start scan req %x %x %x",
                pEventData, pwdiEndScanParams, wdiEndScanRspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  halEndScanReqMsg.endScanParams.scanChannel = pwdiEndScanParams->ucChannel;

  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                  &halEndScanReqMsg.endScanParams, 
                  sizeof(halEndScanReqMsg.endScanParams)); 

  pWDICtx->wdiReqStatusCB     = pwdiEndScanParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiEndScanParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send End Scan Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiEndScanRspCb, pEventData->pUserData, WDI_END_SCAN_RESP);
}/*WDI_ProcessEndScanReq*/


/**
 @brief Process Finish Scan Request function (called when Main 
        FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessFinishScanReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_FinishScanReqParamsType*  pwdiFinishScanParams;
  WDI_FinishScanRspCb           wdiFinishScanRspCb;
  wpt_uint8*                    pSendBuffer          = NULL; 
  wpt_uint16                    usDataOffset         = 0;
  wpt_uint16                    usSendSize           = 0;
  wpt_uint8                     i                    = 0;

  tHalFinishScanReqMsg          halFinishScanReqMsg; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData) ||
      ( NULL == pEventData->pCBfnc))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  pwdiFinishScanParams = (WDI_FinishScanReqParamsType*)pEventData->pEventData;
  wdiFinishScanRspCb   = (WDI_FinishScanRspCb)pEventData->pCBfnc;
  /* commenting this check as UMAC is sending END_SCAN_REQ after FINISH_SCAN 
  * sometimes  because of this check the scan request is not being 
  * forwarded to HAL and result in hang*/
#if 0
  wpalMutexAcquire(&pWDICtx->wptMutex);
   /*-----------------------------------------------------------------------
    Check to see if SCAN is already in progress
    Finish scan gets invoked any scan states. ie. abort scan
    It should be allowed in any states.
  -----------------------------------------------------------------------*/
  if ( !pWDICtx->bScanInProgress )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Finish start not allowed in this state %d",
               pWDICtx->bScanInProgress );

    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*-----------------------------------------------------------------------
    It is safe to reset the scan flags here because until the response comes
    back all subsequent requests will be blocked at BUSY state 
  -----------------------------------------------------------------------*/
  pWDICtx->uScanState      = WDI_SCAN_FINISHED_ST; 
  pWDICtx->bScanInProgress = eWLAN_PAL_FALSE; 
  wpalMutexRelease(&pWDICtx->wptMutex);
#endif

  if ( pWDICtx->bInBmps )
  {
     // notify DTS that we are entering BMPS
     WDTS_SetPowerState(pWDICtx, WDTS_POWER_STATE_BMPS, NULL);
  }

  /*-----------------------------------------------------------------------
    Get message buffer
  -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_FINISH_SCAN_REQ, 
                        sizeof(halFinishScanReqMsg.finishScanParams),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset + sizeof(halFinishScanReqMsg.finishScanParams) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in start scan req %x %x %x",
                pEventData, pwdiFinishScanParams, wdiFinishScanRspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  halFinishScanReqMsg.finishScanParams.scanMode = 
    WDI_2_HAL_SCAN_MODE(pwdiFinishScanParams->wdiReqInfo.wdiScanMode);

  halFinishScanReqMsg.finishScanParams.currentOperChannel = 
    pwdiFinishScanParams->wdiReqInfo.ucCurrentOperatingChannel;

  halFinishScanReqMsg.finishScanParams.cbState = 
    WDI_2_HAL_CB_STATE(pwdiFinishScanParams->wdiReqInfo.wdiCBState);

  wpalMemoryCopy(halFinishScanReqMsg.finishScanParams.bssid,
                 pwdiFinishScanParams->wdiReqInfo.macBSSID, WDI_MAC_ADDR_LEN);

  halFinishScanReqMsg.finishScanParams.notifyBss   = 
                              pwdiFinishScanParams->wdiReqInfo.bNotifyBSS ;
  halFinishScanReqMsg.finishScanParams.frameType   = 
                              pwdiFinishScanParams->wdiReqInfo.ucFrameType ;
  halFinishScanReqMsg.finishScanParams.frameLength = 
                              pwdiFinishScanParams->wdiReqInfo.ucFrameLength ;

  halFinishScanReqMsg.finishScanParams.scanEntry.activeBSScnt = 
                   pwdiFinishScanParams->wdiReqInfo.wdiScanEntry.activeBSScnt ;

  for (i = 0; i < pwdiFinishScanParams->wdiReqInfo.wdiScanEntry.activeBSScnt; i++)
  {
    halFinishScanReqMsg.finishScanParams.scanEntry.bssIdx[i] = 
               pwdiFinishScanParams->wdiReqInfo.wdiScanEntry.bssIdx[i] ;
  }

  WDI_CopyWDIMgmFrameHdrToHALMgmFrameHdr( &halFinishScanReqMsg.finishScanParams.macMgmtHdr,
                              &pwdiFinishScanParams->wdiReqInfo.wdiMACMgmtHdr);

  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                  &halFinishScanReqMsg.finishScanParams, 
                  sizeof(halFinishScanReqMsg.finishScanParams)); 

  pWDICtx->wdiReqStatusCB     = pwdiFinishScanParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiFinishScanParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Finish Scan Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiFinishScanRspCb, pEventData->pUserData, WDI_FINISH_SCAN_RESP);
}/*WDI_ProcessFinishScanReq*/


/*==========================================================================
                    ASSOCIATION REQUEST API 
==========================================================================*/
/**
 @brief Process BSS Join for a given Session 
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessBSSSessionJoinReq
( 
  WDI_ControlBlockType*   pWDICtx,
  WDI_JoinReqParamsType*  pwdiJoinParams,
  WDI_JoinRspCb           wdiJoinRspCb,
  void*                   pUserData
)
{
  WDI_BSSSessionType*     pBSSSes             = NULL;
  wpt_uint8*              pSendBuffer         = NULL; 
  wpt_uint16              usDataOffset        = 0;
  wpt_uint16              usSendSize          = 0;
  wpt_uint8               ucCurrentBSSSesIdx  = 0; 

  tHalJoinReqMsg          halJoinReqMsg; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*------------------------------------------------------------------------
    Check to see if we have any session with this BSSID already stored, we
    should not
  ------------------------------------------------------------------------*/
  ucCurrentBSSSesIdx = WDI_FindAssocSession( pWDICtx, 
                                   pwdiJoinParams->wdiReqInfo.macBSSID, 
                                  &pBSSSes);  

  if ( NULL != pBSSSes )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association for this BSSID is already in place");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  wpalMutexAcquire(&pWDICtx->wptMutex);
  /*------------------------------------------------------------------------
    Fetch an empty session block 
  ------------------------------------------------------------------------*/
  ucCurrentBSSSesIdx = WDI_FindEmptySession( pWDICtx, &pBSSSes); 
  if ( NULL == pBSSSes )
  {

    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "DAL has no free sessions - cannot run another join");

    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_RES_FAILURE; 
  }

  /*Save BSS Session Info*/
  pBSSSes->bInUse = eWLAN_PAL_TRUE; 
  wpalMemoryCopy( pBSSSes->macBSSID, pwdiJoinParams->wdiReqInfo.macBSSID, 
                  WDI_MAC_ADDR_LEN);

  /*Transition to state Joining*/
  pBSSSes->wdiAssocState      = WDI_ASSOC_JOINING_ST; 
  pWDICtx->ucCurrentBSSSesIdx = ucCurrentBSSSesIdx;
  
  wpalMutexRelease(&pWDICtx->wptMutex);

  /*-----------------------------------------------------------------------
    Get message buffer
  -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_JOIN_REQ, 
                        sizeof(halJoinReqMsg.joinReqParams),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset + sizeof(halJoinReqMsg.joinReqParams) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in join req %x %x %x",
                pUserData, pwdiJoinParams, wdiJoinRspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wpalMemoryCopy(halJoinReqMsg.joinReqParams.bssId,
                 pwdiJoinParams->wdiReqInfo.macBSSID, WDI_MAC_ADDR_LEN); 

  wpalMemoryCopy(halJoinReqMsg.joinReqParams.selfStaMacAddr,
                 pwdiJoinParams->wdiReqInfo.macSTASelf, 
                 WDI_MAC_ADDR_LEN); 

  halJoinReqMsg.joinReqParams.ucChannel = 
    pwdiJoinParams->wdiReqInfo.wdiChannelInfo.ucChannel;

  halJoinReqMsg.joinReqParams.linkState = pwdiJoinParams->wdiReqInfo.linkState;

#ifndef WLAN_FEATURE_VOWIFI
  halJoinReqMsg.joinReqParams.ucLocalPowerConstraint = 
    pwdiJoinParams->wdiReqInfo.wdiChannelInfo.ucLocalPowerConstraint;
#endif

  halJoinReqMsg.joinReqParams.secondaryChannelOffset =     
     WDI_2_HAL_SEC_CH_OFFSET(pwdiJoinParams->wdiReqInfo.wdiChannelInfo.
                             wdiSecondaryChannelOffset);

  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                  &halJoinReqMsg.joinReqParams, 
                  sizeof(halJoinReqMsg.joinReqParams)); 

  pWDICtx->wdiReqStatusCB     = pwdiJoinParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiJoinParams->pUserData;  

  /*-------------------------------------------------------------------------
    Send Join Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiJoinRspCb, pUserData, WDI_JOIN_RESP); 

}/*WDI_ProcessBSSSessionJoinReq*/

/**
 @brief Process Join Request function (called when Main FSM 
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessJoinReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status              wdiStatus          = WDI_STATUS_SUCCESS;
  WDI_JoinReqParamsType*  pwdiJoinParams     = NULL;
  WDI_JoinRspCb           wdiJoinRspCb       = NULL;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) ||
      ( NULL == (pwdiJoinParams = (WDI_JoinReqParamsType*)pEventData->pEventData)) ||
      ( NULL == (wdiJoinRspCb   = (WDI_JoinRspCb)pEventData->pCBfnc)))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }
  
  /*-------------------------------------------------------------------------
    Check to see if we are in the middle of an association, if so queue, if
    not it means it is free to process request 
  -------------------------------------------------------------------------*/
  wpalMutexAcquire(&pWDICtx->wptMutex);

  if ( eWLAN_PAL_FALSE != pWDICtx->bAssociationInProgress )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
              "Association is currently in progress, queueing new join req");

    /*Association is in progress - queue current one*/
    wdiStatus = WDI_QueueNewAssocRequest(pWDICtx, pEventData, 
                             pwdiJoinParams->wdiReqInfo.macBSSID);

    wpalMutexRelease(&pWDICtx->wptMutex);

    return wdiStatus; 
  }

  /*Starting a new association */
  pWDICtx->bAssociationInProgress = eWLAN_PAL_TRUE;
  wpalMutexRelease(&pWDICtx->wptMutex);

  /*Process the Join Request*/
  return WDI_ProcessBSSSessionJoinReq( pWDICtx, pwdiJoinParams,
                                       wdiJoinRspCb,pEventData->pUserData);

}/*WDI_ProcessJoinReq*/


/**
 @brief Process Config BSS Request function (called when Main 
        FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessConfigBSSReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_ConfigBSSReqParamsType*  pwdiConfigBSSParams;
  WDI_ConfigBSSRspCb           wdiConfigBSSRspCb;
  wpt_uint8                    ucCurrentBSSSesIdx  = 0; 
  WDI_BSSSessionType*          pBSSSes             = NULL;
  wpt_uint16                   uMsgSize            = 0; 
  wpt_uint8*                   pSendBuffer         = NULL; 
  wpt_uint16                   usDataOffset        = 0;
  wpt_uint16                   usSendSize          = 0;
  WDI_Status                   wdiStatus           = WDI_STATUS_SUCCESS; 

  tConfigBssReqMsg             halConfigBssReqMsg; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData ) ||
      ( NULL == pEventData->pCBfnc ))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  pwdiConfigBSSParams = (WDI_ConfigBSSReqParamsType*)pEventData->pEventData;
  wdiConfigBSSRspCb   = (WDI_ConfigBSSRspCb)pEventData->pCBfnc;
  /*-------------------------------------------------------------------------
    Check to see if we are in the middle of an association, if so queue, if
    not it means it is free to process request 
  -------------------------------------------------------------------------*/
  wpalMutexAcquire(&pWDICtx->wptMutex);

  /*------------------------------------------------------------------------
    Find the BSS for which the request is made 
  ------------------------------------------------------------------------*/
  ucCurrentBSSSesIdx = WDI_FindAssocSession( pWDICtx, 
                                 pwdiConfigBSSParams->wdiReqInfo.macBSSID, 
                                 &pBSSSes); 

  if ( NULL == pBSSSes ) 
  {
#ifdef WLAN_FEATURE_VOWIFI_11R
      /*------------------------------------------------------------------------
        Fetch an empty session block 
      ------------------------------------------------------------------------*/
      ucCurrentBSSSesIdx = WDI_FindEmptySession( pWDICtx, &pBSSSes); 
      if ( NULL == pBSSSes )
      {
    
        WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "DAL has no free sessions - cannot run another join");
    
        wpalMutexRelease(&pWDICtx->wptMutex);
        return WDI_STATUS_RES_FAILURE; 
      }
    
      /*Save BSS Session Info*/
      pBSSSes->bInUse = eWLAN_PAL_TRUE; 
      wpalMemoryCopy( pBSSSes->macBSSID, pwdiConfigBSSParams->wdiReqInfo.macBSSID, 
                      WDI_MAC_ADDR_LEN);
    
      /*Transition to state Joining*/
      pBSSSes->wdiAssocState      = WDI_ASSOC_JOINING_ST; 
      pWDICtx->ucCurrentBSSSesIdx = ucCurrentBSSSesIdx;
#else
    /* If the BSS type is IBSS create the session here as there is no Join 
     * Request in case of IBSS*/
    if((pwdiConfigBSSParams->wdiReqInfo.wdiBSSType == WDI_IBSS_MODE) ||
       (pwdiConfigBSSParams->wdiReqInfo.wdiBSSType == WDI_INFRA_AP_MODE) ||
       (pwdiConfigBSSParams->wdiReqInfo.wdiBSSType == WDI_BTAMP_AP_MODE) ||
       (pwdiConfigBSSParams->wdiReqInfo.wdiBSSType == WDI_BTAMP_STA_MODE))
    {
      /*------------------------------------------------------------------------
        Fetch an empty session block 
      ------------------------------------------------------------------------*/
      ucCurrentBSSSesIdx = WDI_FindEmptySession( pWDICtx, &pBSSSes); 
      if ( NULL == pBSSSes )
      {
    
        WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "DAL has no free sessions - cannot run another join");
    
        wpalMutexRelease(&pWDICtx->wptMutex);
        return WDI_STATUS_RES_FAILURE; 
      }
    
      /*Save BSS Session Info*/
      pBSSSes->bInUse = eWLAN_PAL_TRUE; 
      wpalMemoryCopy( pBSSSes->macBSSID, pwdiConfigBSSParams->wdiReqInfo.macBSSID, 
                      WDI_MAC_ADDR_LEN);
    
      /*Transition to state Joining*/
      pBSSSes->wdiAssocState      = WDI_ASSOC_JOINING_ST; 
      pWDICtx->ucCurrentBSSSesIdx = ucCurrentBSSSesIdx;
    }
    else
    {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                "Association sequence for this BSS does not yet exist");
      /* for IBSS testing */
      wpalMutexRelease(&pWDICtx->wptMutex);
      return WDI_STATUS_E_NOT_ALLOWED; 
    }
#endif
  }

  /*------------------------------------------------------------------------
    Check if this BSS is being currently processed or queued,
    if queued - queue the new request as well 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_TRUE == pBSSSes->bAssocReqQueued )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS exists but currently queued");

    wdiStatus = WDI_QueueAssocRequest( pWDICtx, pBSSSes, pEventData); 

    wpalMutexRelease(&pWDICtx->wptMutex);

    return wdiStatus; 
  }

  /* Cache the request for response processing */
  wpalMemoryCopy(&pWDICtx->wdiCachedConfigBssReq, 
                 pwdiConfigBSSParams, 
                 sizeof(pWDICtx->wdiCachedConfigBssReq));

  wpalMutexRelease(&pWDICtx->wptMutex);

  uMsgSize = sizeof(halConfigBssReqMsg.configBssParams); 

  /*-----------------------------------------------------------------------
    Get message buffer
  -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_CONFIG_BSS_REQ, 
                    uMsgSize, &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset + uMsgSize )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in config bss req %x %x %x",
                pEventData, pwdiConfigBSSParams, wdiConfigBSSRspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  /*Copy the BSS request */
  WDI_CopyWDIConfigBSSToHALConfigBSS( &halConfigBssReqMsg.configBssParams,
                                      &pwdiConfigBSSParams->wdiReqInfo);

  /* Need to fill in the STA Index to invalid, since at this point we have not
     yet received it from HAL */
  halConfigBssReqMsg.configBssParams.staContext.staIdx = WDI_STA_INVALID_IDX;

  /* Need to fill in the BSS index */
  halConfigBssReqMsg.configBssParams.staContext.bssIdx = pBSSSes->ucBSSIdx;
  
  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                  &halConfigBssReqMsg.configBssParams, 
                  sizeof(halConfigBssReqMsg.configBssParams)); 

  pWDICtx->wdiReqStatusCB     = pwdiConfigBSSParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiConfigBSSParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Config BSS Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiConfigBSSRspCb, pEventData->pUserData, 
                       WDI_CONFIG_BSS_RESP);

}/*WDI_ProcessConfigBSSReq*/


/**
 @brief Process Del BSS Request function (called when Main FSM 
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessDelBSSReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_DelBSSReqParamsType*  pwdiDelBSSParams    = NULL;
  WDI_DelBSSRspCb           wdiDelBSSRspCb      = NULL;
  wpt_uint8                 ucCurrentBSSSesIdx  = 0; 
  WDI_BSSSessionType*       pBSSSes             = NULL;
  wpt_uint8*                pSendBuffer         = NULL; 
  wpt_uint16                usDataOffset        = 0;
  wpt_uint16                usSendSize          = 0;
  WDI_Status                wdiStatus           = WDI_STATUS_SUCCESS; 

  tDeleteBssReqMsg          halBssReqMsg; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) ||
      ( NULL == (pwdiDelBSSParams = (WDI_DelBSSReqParamsType*)pEventData->pEventData)) ||
      ( NULL == (wdiDelBSSRspCb   = (WDI_DelBSSRspCb)pEventData->pCBfnc)))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  /*-------------------------------------------------------------------------
    Check to see if we are in the middle of an association, if so queue, if
    not it means it is free to process request 
  -------------------------------------------------------------------------*/
  wpalMutexAcquire(&pWDICtx->wptMutex);

  /*------------------------------------------------------------------------
    Find the BSS for which the request is made 
  ------------------------------------------------------------------------*/
  ucCurrentBSSSesIdx = WDI_FindAssocSessionByBSSIdx( pWDICtx, 
                                             pwdiDelBSSParams->ucBssIdx, 
                                            &pBSSSes); 

  if ( NULL == pBSSSes ) 
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS does not yet exist");

    wpalMutexRelease(&pWDICtx->wptMutex);

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Check if this BSS is being currently processed or queued,
    if queued - queue the new request as well 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_TRUE == pBSSSes->bAssocReqQueued )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS exists but currently queued");

    wdiStatus = WDI_QueueAssocRequest( pWDICtx, pBSSSes, pEventData); 

    wpalMutexRelease(&pWDICtx->wptMutex);

    return wdiStatus; 
  }

  /*-----------------------------------------------------------------------
    If we receive a Del BSS request for an association that is already in
    progress, it indicates that the assoc has failed => we no longer have
    an association in progress => we must check for pending associations
    that were queued and start as soon as the Del BSS response is received 
  -----------------------------------------------------------------------*/
  if ( ucCurrentBSSSesIdx == pWDICtx->ucCurrentBSSSesIdx )
  {
    /*We can switch to false here because even if a subsequent Join comes in
      it will only be processed when DAL transitions out of BUSY state which
      happens when the Del BSS request comes */
     pWDICtx->bAssociationInProgress = eWLAN_PAL_FALSE;

     /*Former association is complete - prepare next pending assoc for
       processing */
     WDI_DequeueAssocRequest(pWDICtx);
  }

  wpalMutexRelease(&pWDICtx->wptMutex);
  /*-----------------------------------------------------------------------
    Get message buffer
  -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_DEL_BSS_REQ, 
                        sizeof(halBssReqMsg.deleteBssParams),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset + sizeof(halBssReqMsg.deleteBssParams) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in start req %x %x %x",
                pEventData, pwdiDelBSSParams, wdiDelBSSRspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  /*Fill in the message request structure*/

  /*BSS Index is saved on config BSS response and Post Assoc Response */
  halBssReqMsg.deleteBssParams.bssIdx = pBSSSes->ucBSSIdx; 

  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                  &halBssReqMsg.deleteBssParams, 
                  sizeof(halBssReqMsg.deleteBssParams)); 

  pWDICtx->wdiReqStatusCB     = pwdiDelBSSParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiDelBSSParams->pUserData; 

 
  /*-------------------------------------------------------------------------
    Send Del BSS Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiDelBSSRspCb, pEventData->pUserData, WDI_DEL_BSS_RESP);

  
}/*WDI_ProcessDelBSSReq*/

/**
 @brief Process Post Assoc Request function (called when Main 
        FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessPostAssocReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_PostAssocReqParamsType* pwdiPostAssocParams   = NULL;
  WDI_PostAssocRspCb          wdiPostAssocRspCb     = NULL;
  wpt_uint8                   ucCurrentBSSSesIdx    = 0; 
  WDI_BSSSessionType*         pBSSSes               = NULL;
  wpt_uint8*                  pSendBuffer           = NULL; 
  wpt_uint16                  usDataOffset          = 0;
  wpt_uint16                  usSendSize            = 0;
  wpt_uint16                  uMsgSize              = 0;
  wpt_uint16                  uOffset               = 0;
  WDI_Status                  wdiStatus             = WDI_STATUS_SUCCESS; 

  tPostAssocReqMsg            halPostAssocReqMsg; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) ||
      ( NULL == (pwdiPostAssocParams = (WDI_PostAssocReqParamsType*)pEventData->pEventData)) ||
      ( NULL == (wdiPostAssocRspCb   = (WDI_PostAssocRspCb)pEventData->pCBfnc)))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  /*-------------------------------------------------------------------------
    Check to see if we are in the middle of an association, if so queue, if
    not it means it is free to process request 
  -------------------------------------------------------------------------*/
  wpalMutexAcquire(&pWDICtx->wptMutex);

  /*------------------------------------------------------------------------
    Find the BSS for which the request is made 
  ------------------------------------------------------------------------*/
  ucCurrentBSSSesIdx = WDI_FindAssocSession( pWDICtx, 
                              pwdiPostAssocParams->wdiBSSParams.macBSSID, 
                              &pBSSSes); 

  if ( NULL == pBSSSes )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS does not yet exist - "
              "operation not allowed");

    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Check if this BSS is being currently processed or queued,
    if queued - queue the new request as well 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_TRUE == pBSSSes->bAssocReqQueued )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS exists but currently queued");

    wdiStatus = WDI_QueueAssocRequest( pWDICtx, pBSSSes, pEventData); 

    wpalMutexRelease(&pWDICtx->wptMutex);

    return wdiStatus; 
  }

  /*-----------------------------------------------------------------------
    If Post Assoc was not yet received - the current association must
    be in progress
    -----------------------------------------------------------------------*/
  if (( ucCurrentBSSSesIdx != pWDICtx->ucCurrentBSSSesIdx ) || 
      ( eWLAN_PAL_FALSE == pWDICtx->bAssociationInProgress ))
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS association no longer in "
              "progress - not allowed");

    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*-----------------------------------------------------------------------
    Post Assoc Request is only allowed in Joining state 
  -----------------------------------------------------------------------*/
  if ( WDI_ASSOC_JOINING_ST != pBSSSes->wdiAssocState)
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Post Assoc not allowed before JOIN - failing request");

    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  wpalMutexRelease(&pWDICtx->wptMutex);

  uMsgSize = sizeof(halPostAssocReqMsg.postAssocReqParams.configStaParams) +
             sizeof(halPostAssocReqMsg.postAssocReqParams.configBssParams) ;
  /*-----------------------------------------------------------------------
    Fill message for tx over the bus 
  -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_POST_ASSOC_REQ, 
                        uMsgSize,&pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset + uMsgSize )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in start req %x %x %x",
                pEventData, pwdiPostAssocParams, wdiPostAssocRspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  /*Copy the STA parameters */
  WDI_CopyWDIStaCtxToHALStaCtx(&halPostAssocReqMsg.postAssocReqParams.configStaParams,
                               &pwdiPostAssocParams->wdiSTAParams );

  /* Need to fill in the self STA Index */
  if ( WDI_STATUS_SUCCESS != 
       WDI_STATableFindStaidByAddr(pWDICtx,
                                   pwdiPostAssocParams->wdiSTAParams.macSTA,
                                   (wpt_uint8*)&halPostAssocReqMsg.postAssocReqParams.configStaParams.staIdx ))
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
             "This station does not exist in the WDI Station Table %d");
    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_FAILURE; 
  }

  /* Need to fill in the BSS index */
  halPostAssocReqMsg.postAssocReqParams.configStaParams.bssIdx = 
     pBSSSes->ucBSSIdx;

  /*Copy the BSS parameters */
  WDI_CopyWDIConfigBSSToHALConfigBSS( &halPostAssocReqMsg.postAssocReqParams.configBssParams,
                                      &pwdiPostAssocParams->wdiBSSParams);

  /* Need to fill in the STA index of the peer */
  if ( WDI_STATUS_SUCCESS != 
       WDI_STATableFindStaidByAddr(pWDICtx,
                                   pwdiPostAssocParams->wdiBSSParams.wdiSTAContext.macSTA,
                                   (wpt_uint8*)&halPostAssocReqMsg.postAssocReqParams.configBssParams.staContext.staIdx)) 
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
             "This station does not exist in the WDI Station Table %d");
    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_FAILURE; 
  }

  /* Need to fill in the BSS index */
  halPostAssocReqMsg.postAssocReqParams.configStaParams.bssIdx = 
     pBSSSes->ucBSSIdx;

  
  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                  &halPostAssocReqMsg.postAssocReqParams.configStaParams, 
                  sizeof(halPostAssocReqMsg.postAssocReqParams.configStaParams)); 

  uOffset = sizeof(halPostAssocReqMsg.postAssocReqParams.configStaParams);

  wpalMemoryCopy( pSendBuffer+usDataOffset + uOffset, 
                  &halPostAssocReqMsg.postAssocReqParams.configBssParams, 
                  sizeof(halPostAssocReqMsg.postAssocReqParams.configBssParams)); 

 
  pWDICtx->wdiReqStatusCB     = pwdiPostAssocParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiPostAssocParams->pUserData; 

 
  wpalMemoryCopy( &pWDICtx->wdiCachedPostAssocReq, 
                  pwdiPostAssocParams,
                  sizeof(pWDICtx->wdiCachedPostAssocReq));  

  /*-------------------------------------------------------------------------
    Send Post Assoc Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiPostAssocRspCb, pEventData->pUserData, WDI_POST_ASSOC_RESP);

  
}/*WDI_ProcessPostAssocReq*/

/**
 @brief Process Del STA Request function (called when Main FSM 
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessDelSTAReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_DelSTAReqParamsType*  pwdiDelSTAParams;
  WDI_DelSTARspCb           wdiDelSTARspCb;
  wpt_uint8                 ucCurrentBSSSesIdx  = 0; 
  WDI_BSSSessionType*       pBSSSes             = NULL;
  wpt_uint8*                pSendBuffer         = NULL; 
  wpt_uint16                usDataOffset        = 0;
  wpt_uint16                usSendSize          = 0;
  wpt_macAddr               macBSSID; 
  WDI_Status                wdiStatus           = WDI_STATUS_SUCCESS;

  tDeleteStaReqMsg          halDelStaReqMsg; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) || ( NULL == pEventData->pEventData ) ||
      ( NULL == pEventData->pCBfnc ))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  pwdiDelSTAParams = (WDI_DelSTAReqParamsType*)pEventData->pEventData;
  wdiDelSTARspCb   = (WDI_DelSTARspCb)pEventData->pCBfnc;
  /*-------------------------------------------------------------------------
    Check to see if we are in the middle of an association, if so queue, if
    not it means it is free to process request 
  -------------------------------------------------------------------------*/
  wpalMutexAcquire(&pWDICtx->wptMutex);

  /*------------------------------------------------------------------------
    Find the BSS for which the request is made and identify WDI session
  ------------------------------------------------------------------------*/
  if ( WDI_STATUS_SUCCESS != WDI_STATableGetStaBSSIDAddr(pWDICtx, 
                                                         pwdiDelSTAParams->ucSTAIdx, 
                                                         &macBSSID))
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
             "This station does not exist in the WDI Station Table %d");
    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_FAILURE; 
  }

  ucCurrentBSSSesIdx = WDI_FindAssocSession( pWDICtx, macBSSID, &pBSSSes); 
  if ( NULL == pBSSSes ) 
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS does not yet exist");

    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Check if this BSS is being currently processed or queued,
    if queued - queue the new request as well 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_TRUE == pBSSSes->bAssocReqQueued )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS exists but currently queued");

    wdiStatus = WDI_QueueAssocRequest( pWDICtx, pBSSSes, pEventData); 
    wpalMutexRelease(&pWDICtx->wptMutex);
    return wdiStatus; 
  }

  wpalMutexRelease(&pWDICtx->wptMutex);
  /*-----------------------------------------------------------------------
    Get message buffer
  -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_DEL_STA_REQ, 
                        sizeof(halDelStaReqMsg.delStaParams),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset + sizeof(halDelStaReqMsg.delStaParams) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in start req %x %x %x",
                pEventData, pwdiDelSTAParams, wdiDelSTARspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  halDelStaReqMsg.delStaParams.staIdx = pwdiDelSTAParams->ucSTAIdx; 
  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                  &halDelStaReqMsg.delStaParams, 
                  sizeof(halDelStaReqMsg.delStaParams)); 

  pWDICtx->wdiReqStatusCB     = pwdiDelSTAParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiDelSTAParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Del STA Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiDelSTARspCb, pEventData->pUserData, WDI_DEL_STA_RESP);

}/*WDI_ProcessDelSTAReq*/


/*==========================================================================
                 SECURITY REQUEST PROCESSING API 
==========================================================================*/
/**
 @brief Process Set BSS Key Request function (called when Main FSM
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetBssKeyReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_SetBSSKeyReqParamsType*  pwdiSetBSSKeyParams;
  WDI_SetBSSKeyRspCb           wdiSetBSSKeyRspCb;
  wpt_uint8                    ucCurrentBSSSesIdx  = 0; 
  WDI_BSSSessionType*          pBSSSes             = NULL;
  wpt_uint8*                   pSendBuffer         = NULL; 
  wpt_uint16                   usDataOffset        = 0;
  wpt_uint16                   usSendSize          = 0;
  WDI_Status                   wdiStatus           = WDI_STATUS_SUCCESS; 
  tSetBssKeyReqMsg             halSetBssKeyReqMsg  = {{0}};
  wpt_uint8                    keyIndex            = 0;

  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) || ( NULL == pEventData->pEventData ) ||
      ( NULL == pEventData->pCBfnc ))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  pwdiSetBSSKeyParams = (WDI_SetBSSKeyReqParamsType*)pEventData->pEventData;
  wdiSetBSSKeyRspCb   = (WDI_SetBSSKeyRspCb)pEventData->pCBfnc;
  /*-------------------------------------------------------------------------
    Check to see if we are in the middle of an association, if so queue, if
    not it means it is free to process request 
  -------------------------------------------------------------------------*/
  wpalMutexAcquire(&pWDICtx->wptMutex);

  /*------------------------------------------------------------------------
    Find the BSS for which the request is made 
  ------------------------------------------------------------------------*/
  ucCurrentBSSSesIdx = WDI_FindAssocSessionByBSSIdx( pWDICtx, 
                           pwdiSetBSSKeyParams->wdiBSSKeyInfo.ucBssIdx, 
                          &pBSSSes); 

  if ( NULL == pBSSSes ) 
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS does not yet exist");

    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Check if this BSS is being currently processed or queued,
    if queued - queue the new request as well 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_TRUE == pBSSSes->bAssocReqQueued )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS exists but currently queued");

    wdiStatus = WDI_QueueAssocRequest( pWDICtx, pBSSSes, pEventData); 
    wpalMutexRelease(&pWDICtx->wptMutex);
    return wdiStatus; 
  }


  wpalMutexRelease(&pWDICtx->wptMutex);
  /*-----------------------------------------------------------------------
    Get message buffer
  -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_SET_BSS_KEY_REQ, 
                        sizeof(halSetBssKeyReqMsg.setBssKeyParams),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset + sizeof(halSetBssKeyReqMsg.setBssKeyParams) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in set bss key req %x %x %x",
                pEventData, pwdiSetBSSKeyParams, wdiSetBSSKeyRspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  /*-----------------------------------------------------------------------
    Copy the Key parameters into the HAL message
  -----------------------------------------------------------------------*/

  halSetBssKeyReqMsg.setBssKeyParams.bssIdx = ucCurrentBSSSesIdx; 

  halSetBssKeyReqMsg.setBssKeyParams.encType = 
             WDI_2_HAL_ENC_TYPE (pwdiSetBSSKeyParams->wdiBSSKeyInfo.wdiEncType);

  halSetBssKeyReqMsg.setBssKeyParams.numKeys = 
                                  pwdiSetBSSKeyParams->wdiBSSKeyInfo.ucNumKeys;

  for(keyIndex = 0; keyIndex < pwdiSetBSSKeyParams->wdiBSSKeyInfo.ucNumKeys ;
                                                                 keyIndex++)
  {
    halSetBssKeyReqMsg.setBssKeyParams.key[keyIndex].keyId = 
                      pwdiSetBSSKeyParams->wdiBSSKeyInfo.aKeys[keyIndex].keyId;
    halSetBssKeyReqMsg.setBssKeyParams.key[keyIndex].unicast =
                     pwdiSetBSSKeyParams->wdiBSSKeyInfo.aKeys[keyIndex].unicast;
    halSetBssKeyReqMsg.setBssKeyParams.key[keyIndex].keyDirection =
                pwdiSetBSSKeyParams->wdiBSSKeyInfo.aKeys[keyIndex].keyDirection;
    wpalMemoryCopy(halSetBssKeyReqMsg.setBssKeyParams.key[keyIndex].keyRsc,
                     pwdiSetBSSKeyParams->wdiBSSKeyInfo.aKeys[keyIndex].keyRsc, 
                     WDI_MAX_KEY_RSC_LEN);
    halSetBssKeyReqMsg.setBssKeyParams.key[keyIndex].paeRole = 
                     pwdiSetBSSKeyParams->wdiBSSKeyInfo.aKeys[keyIndex].paeRole;
    halSetBssKeyReqMsg.setBssKeyParams.key[keyIndex].keyLength = 
                   pwdiSetBSSKeyParams->wdiBSSKeyInfo.aKeys[keyIndex].keyLength;
    wpalMemoryCopy(halSetBssKeyReqMsg.setBssKeyParams.key[keyIndex].key,
                         pwdiSetBSSKeyParams->wdiBSSKeyInfo.aKeys[keyIndex].key, 
                        WDI_MAX_KEY_LENGTH);
   }
                                                                  
  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                    &halSetBssKeyReqMsg.setBssKeyParams, 
                    sizeof(halSetBssKeyReqMsg.setBssKeyParams)); 

  pWDICtx->wdiReqStatusCB     = pwdiSetBSSKeyParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiSetBSSKeyParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Set BSS Key Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiSetBSSKeyRspCb, pEventData->pUserData, 
                       WDI_SET_BSS_KEY_RESP); 

}/*WDI_ProcessSetBssKeyReq*/

/**
 @brief Process Remove BSS Key Request function (called when Main    
        FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessRemoveBssKeyReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_RemoveBSSKeyReqParamsType*  pwdiRemoveBSSKeyParams;
  WDI_RemoveBSSKeyRspCb           wdiRemoveBSSKeyRspCb;
  wpt_uint8                       ucCurrentBSSSesIdx     = 0; 
  WDI_BSSSessionType*             pBSSSes                = NULL;
  wpt_uint8*                      pSendBuffer            = NULL; 
  wpt_uint16                      usDataOffset           = 0;
  wpt_uint16                      usSendSize             = 0;
  WDI_Status                      wdiStatus              = WDI_STATUS_SUCCESS; 
  tRemoveBssKeyReqMsg             halRemoveBssKeyReqMsg  = {{0}};
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) || ( NULL == pEventData->pEventData ) ||
      ( NULL == pEventData->pCBfnc ))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  pwdiRemoveBSSKeyParams = (WDI_RemoveBSSKeyReqParamsType*)pEventData->pEventData;
  wdiRemoveBSSKeyRspCb   = (WDI_RemoveBSSKeyRspCb)pEventData->pCBfnc;
  /*-------------------------------------------------------------------------
    Check to see if we are in the middle of an association, if so queue, if
    not it means it is free to process request 
  -------------------------------------------------------------------------*/
  wpalMutexAcquire(&pWDICtx->wptMutex);

  /*------------------------------------------------------------------------
    Find the BSS for which the request is made 
  ------------------------------------------------------------------------*/
  ucCurrentBSSSesIdx = WDI_FindAssocSessionByBSSIdx( pWDICtx, 
                           pwdiRemoveBSSKeyParams->wdiKeyInfo.ucBssIdx, 
                          &pBSSSes); 

  if ( NULL == pBSSSes ) 
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS does not yet exist");

    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Check if this BSS is being currently processed or queued,
    if queued - queue the new request as well 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_TRUE == pBSSSes->bAssocReqQueued )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS exists but currently queued");

    wdiStatus = WDI_QueueAssocRequest( pWDICtx, pBSSSes, pEventData); 
    wpalMutexRelease(&pWDICtx->wptMutex);
    return wdiStatus; 
  }


  wpalMutexRelease(&pWDICtx->wptMutex);

  /*-----------------------------------------------------------------------
    Get message buffer
  -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_RMV_BSS_KEY_REQ, 
                        sizeof(halRemoveBssKeyReqMsg.removeBssKeyParams),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset + sizeof(halRemoveBssKeyReqMsg.removeBssKeyParams) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in set bss key req %x %x %x",
                pEventData, pwdiRemoveBSSKeyParams, wdiRemoveBSSKeyRspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }
  /*-----------------------------------------------------------------------
    Copy the Key parameters into the HAL message
  -----------------------------------------------------------------------*/
  halRemoveBssKeyReqMsg.removeBssKeyParams.bssIdx = ucCurrentBSSSesIdx;

  halRemoveBssKeyReqMsg.removeBssKeyParams.encType = 
      WDI_2_HAL_ENC_TYPE (pwdiRemoveBSSKeyParams->wdiKeyInfo.wdiEncType);

  halRemoveBssKeyReqMsg.removeBssKeyParams.keyId = pwdiRemoveBSSKeyParams->wdiKeyInfo.ucKeyId;

  halRemoveBssKeyReqMsg.removeBssKeyParams.wepType = 
      WDI_2_HAL_WEP_TYPE(pwdiRemoveBSSKeyParams->wdiKeyInfo.wdiWEPType);

  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                    &halRemoveBssKeyReqMsg.removeBssKeyParams, 
                    sizeof(halRemoveBssKeyReqMsg.removeBssKeyParams)); 

  pWDICtx->wdiReqStatusCB     = pwdiRemoveBSSKeyParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiRemoveBSSKeyParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Remove BSS Key Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiRemoveBSSKeyRspCb, pEventData->pUserData,
                       WDI_RMV_BSS_KEY_RESP); 
}/*WDI_ProcessRemoveBssKeyReq*/

/**
 @brief Process Set STA KeyRequest function (called when Main FSM 
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetStaKeyReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_SetSTAKeyReqParamsType*  pwdiSetSTAKeyParams;
  WDI_SetSTAKeyRspCb           wdiSetSTAKeyRspCb;
  WDI_BSSSessionType*          pBSSSes             = NULL;
  wpt_uint8*                   pSendBuffer         = NULL; 
  wpt_uint16                   usDataOffset        = 0;
  wpt_uint16                   usSendSize          = 0;
  WDI_Status                   wdiStatus           = WDI_STATUS_SUCCESS; 
  wpt_macAddr                  macBSSID;
  wpt_uint8                    ucCurrentBSSSesIdx; 
  tSetStaKeyReqMsg             halSetStaKeyReqMsg  = {{0}};
  wpt_uint8                    keyIndex            = 0;

  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) || ( NULL == pEventData->pEventData ) ||
      ( NULL == pEventData->pCBfnc ))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

   pwdiSetSTAKeyParams = (WDI_SetSTAKeyReqParamsType*)pEventData->pEventData;
   wdiSetSTAKeyRspCb   = (WDI_SetSTAKeyRspCb)pEventData->pCBfnc;
  /*-------------------------------------------------------------------------
    Check to see if we are in the middle of an association, if so queue, if
    not it means it is free to process request 
  -------------------------------------------------------------------------*/
  wpalMutexAcquire(&pWDICtx->wptMutex);

  /*------------------------------------------------------------------------
    Find the BSS for which the request is made and identify WDI session
  ------------------------------------------------------------------------*/
  if ( WDI_STATUS_SUCCESS != WDI_STATableGetStaBSSIDAddr(pWDICtx, 
                                  pwdiSetSTAKeyParams->wdiKeyInfo.ucSTAIdx, 
                                  &macBSSID))
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
             "This station does not exist in the WDI Station Table %d");
    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_FAILURE; 
  }

  ucCurrentBSSSesIdx = WDI_FindAssocSession( pWDICtx, macBSSID, &pBSSSes); 
  if ( NULL == pBSSSes ) 
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS does not yet exist");

    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_NOT_ALLOWED; 
  }
 
  /*------------------------------------------------------------------------
    Check if this BSS is being currently processed or queued,
    if queued - queue the new request as well 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_TRUE == pBSSSes->bAssocReqQueued )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS exists but currently queued");

    wdiStatus = WDI_QueueAssocRequest( pWDICtx, pBSSSes, pEventData); 
    wpalMutexRelease(&pWDICtx->wptMutex);
    return wdiStatus; 
  }


  wpalMutexRelease(&pWDICtx->wptMutex);
  /*-----------------------------------------------------------------------
    Get message buffer
  -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_SET_STA_KEY_REQ, 
                        sizeof(halSetStaKeyReqMsg.setStaKeyParams),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset + sizeof(halSetStaKeyReqMsg.setStaKeyParams) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in set bss key req %x %x %x",
                pEventData, pwdiSetSTAKeyParams, wdiSetSTAKeyRspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }
  /*-----------------------------------------------------------------------
    Copy the STA Key parameters into the HAL message
  -----------------------------------------------------------------------*/
  halSetStaKeyReqMsg.setStaKeyParams.encType = 
      WDI_2_HAL_ENC_TYPE (pwdiSetSTAKeyParams->wdiKeyInfo.wdiEncType);

  halSetStaKeyReqMsg.setStaKeyParams.wepType = 
      WDI_2_HAL_WEP_TYPE (pwdiSetSTAKeyParams->wdiKeyInfo.wdiWEPType );

  halSetStaKeyReqMsg.setStaKeyParams.staIdx = pwdiSetSTAKeyParams->wdiKeyInfo.ucSTAIdx;

  halSetStaKeyReqMsg.setStaKeyParams.defWEPIdx = pwdiSetSTAKeyParams->wdiKeyInfo.ucDefWEPIdx;

  halSetStaKeyReqMsg.setStaKeyParams.singleTidRc = pwdiSetSTAKeyParams->wdiKeyInfo.ucSingleTidRc;

#ifdef WLAN_SOFTAP_FEATURE
  for(keyIndex = 0; keyIndex < pwdiSetSTAKeyParams->wdiKeyInfo.ucNumKeys ;
                                                                 keyIndex++)
  {
    halSetStaKeyReqMsg.setStaKeyParams.key[keyIndex].keyId = 
                      pwdiSetSTAKeyParams->wdiKeyInfo.wdiKey[keyIndex].keyId;
    halSetStaKeyReqMsg.setStaKeyParams.key[keyIndex].unicast =
                     pwdiSetSTAKeyParams->wdiKeyInfo.wdiKey[keyIndex].unicast;
    halSetStaKeyReqMsg.setStaKeyParams.key[keyIndex].keyDirection =
                pwdiSetSTAKeyParams->wdiKeyInfo.wdiKey[keyIndex].keyDirection;
    wpalMemoryCopy(halSetStaKeyReqMsg.setStaKeyParams.key[keyIndex].keyRsc,
                     pwdiSetSTAKeyParams->wdiKeyInfo.wdiKey[keyIndex].keyRsc, 
                     WDI_MAX_KEY_RSC_LEN);
    halSetStaKeyReqMsg.setStaKeyParams.key[keyIndex].paeRole = 
                     pwdiSetSTAKeyParams->wdiKeyInfo.wdiKey[keyIndex].paeRole;
    halSetStaKeyReqMsg.setStaKeyParams.key[keyIndex].keyLength = 
                   pwdiSetSTAKeyParams->wdiKeyInfo.wdiKey[keyIndex].keyLength;
    wpalMemoryCopy(halSetStaKeyReqMsg.setStaKeyParams.key[keyIndex].key,
                         pwdiSetSTAKeyParams->wdiKeyInfo.wdiKey[keyIndex].key, 
                        WDI_MAX_KEY_LENGTH);
   }
#else
  halSetStaKeyReqMsg.setStaKeyParams.key.keyId = 
                      pwdiSetSTAKeyParams->wdiKeyInfo.wdiKey[0].keyId;
  halSetStaKeyReqMsg.setStaKeyParams.key.unicast =
                     pwdiSetSTAKeyParams->wdiKeyInfo.wdiKey[0].unicast;
  halSetStaKeyReqMsg.setStaKeyParams.key.keyDirection =
                pwdiSetSTAKeyParams->wdiKeyInfo.wdiKey[0].keyDirection;
  wpalMemoryCopy(halSetStaKeyReqMsg.setStaKeyParams.key.keyRsc,
                     pwdiSetSTAKeyParams->wdiKeyInfo.wdiKey[0].keyRsc, 
                     WDI_MAX_KEY_RSC_LEN);
  halSetStaKeyReqMsg.setStaKeyParams.key.paeRole = 
                     pwdiSetSTAKeyParams->wdiKeyInfo.wdiKey[0].paeRole;
  halSetStaKeyReqMsg.setStaKeyParams.key.keyLength = 
                   pwdiSetSTAKeyParams->wdiKeyInfo.wdiKey[0].keyLength;
  wpalMemoryCopy(halSetStaKeyReqMsg.setStaKeyParams.key.key,
                         pwdiSetSTAKeyParams->wdiKeyInfo.wdiKey[0].key, 
                        WDI_MAX_KEY_LENGTH);
#endif

  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                    &halSetStaKeyReqMsg.setStaKeyParams, 
                    sizeof(halSetStaKeyReqMsg.setStaKeyParams)); 

  pWDICtx->wdiReqStatusCB     = pwdiSetSTAKeyParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiSetSTAKeyParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Set STA Key Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiSetSTAKeyRspCb, pEventData->pUserData, 
                       WDI_SET_STA_KEY_RESP); 

}/*WDI_ProcessSetSTAKeyReq*/

/**
 @brief Process Remove STA Key Request function (called when 
        Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessRemoveStaKeyReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_RemoveSTAKeyReqParamsType*  pwdiRemoveSTAKeyParams;
  WDI_RemoveSTAKeyRspCb           wdiRemoveSTAKeyRspCb;
  WDI_BSSSessionType*             pBSSSes                = NULL;
  wpt_uint8*                      pSendBuffer            = NULL; 
  wpt_uint16                      usDataOffset           = 0;
  wpt_uint16                      usSendSize             = 0;
  WDI_Status                      wdiStatus              = WDI_STATUS_SUCCESS; 
  wpt_macAddr                     macBSSID;
  wpt_uint8                       ucCurrentBSSSesIdx;
  tRemoveStaKeyReqMsg             halRemoveStaKeyReqMsg  = {{0}};
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
 if (( NULL == pEventData ) || ( NULL == pEventData->pEventData ) ||
      ( NULL == pEventData->pCBfnc ))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  pwdiRemoveSTAKeyParams = (WDI_RemoveSTAKeyReqParamsType*)pEventData->pEventData;
  wdiRemoveSTAKeyRspCb   = (WDI_RemoveSTAKeyRspCb)pEventData->pCBfnc;
  /*-------------------------------------------------------------------------
    Check to see if we are in the middle of an association, if so queue, if
    not it means it is free to process request 
  -------------------------------------------------------------------------*/
  wpalMutexAcquire(&pWDICtx->wptMutex);

  /*------------------------------------------------------------------------
    Find the BSS for which the request is made and identify WDI session
  ------------------------------------------------------------------------*/
  if ( WDI_STATUS_SUCCESS != WDI_STATableGetStaBSSIDAddr(pWDICtx, 
                             pwdiRemoveSTAKeyParams->wdiKeyInfo.ucSTAIdx, 
                             &macBSSID))
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
             "This station does not exist in the WDI Station Table %d");
    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_FAILURE; 
  }

  ucCurrentBSSSesIdx = WDI_FindAssocSession( pWDICtx, macBSSID, &pBSSSes); 
  if ( NULL == pBSSSes ) 
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS does not yet exist");

    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_NOT_ALLOWED; 
  }
 
  /*------------------------------------------------------------------------
    Check if this BSS is being currently processed or queued,
    if queued - queue the new request as well 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_TRUE == pBSSSes->bAssocReqQueued )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS exists but currently queued");

    wdiStatus = WDI_QueueAssocRequest( pWDICtx, pBSSSes, pEventData); 
    wpalMutexRelease(&pWDICtx->wptMutex);
    return wdiStatus; 
  }



  wpalMutexRelease(&pWDICtx->wptMutex);
  /*-----------------------------------------------------------------------
    Get message buffer
  -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_RMV_STA_KEY_REQ, 
                        sizeof(halRemoveStaKeyReqMsg.removeStaKeyParams),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset + sizeof(halRemoveStaKeyReqMsg.removeStaKeyParams) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in set bss key req %x %x %x",
                pEventData, pwdiRemoveSTAKeyParams, wdiRemoveSTAKeyRspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  /*-----------------------------------------------------------------------
    Copy the Key parameters into the HAL message
  -----------------------------------------------------------------------*/

  halRemoveStaKeyReqMsg.removeStaKeyParams.staIdx = 
      pwdiRemoveSTAKeyParams->wdiKeyInfo.ucSTAIdx;

  halRemoveStaKeyReqMsg.removeStaKeyParams.encType = 
      WDI_2_HAL_ENC_TYPE (pwdiRemoveSTAKeyParams->wdiKeyInfo.wdiEncType);

  halRemoveStaKeyReqMsg.removeStaKeyParams.keyId = 
      pwdiRemoveSTAKeyParams->wdiKeyInfo.ucKeyId;

  halRemoveStaKeyReqMsg.removeStaKeyParams.unicast = 
      pwdiRemoveSTAKeyParams->wdiKeyInfo.ucUnicast;

  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                    &halRemoveStaKeyReqMsg.removeStaKeyParams, 
                    sizeof(halRemoveStaKeyReqMsg.removeStaKeyParams)); 

  pWDICtx->wdiReqStatusCB     = pwdiRemoveSTAKeyParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiRemoveSTAKeyParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Remove STA Key Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiRemoveSTAKeyRspCb, pEventData->pUserData,
                       WDI_RMV_STA_KEY_RESP); 

}/*WDI_ProcessRemoveSTAKeyReq*/

/**
 @brief Process Set STA KeyRequest function (called when Main FSM 
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetStaBcastKeyReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_SetSTAKeyReqParamsType*  pwdiSetSTAKeyParams;
  WDI_SetSTAKeyRspCb           wdiSetSTAKeyRspCb;
  WDI_BSSSessionType*          pBSSSes             = NULL;
  wpt_uint8*                   pSendBuffer         = NULL; 
  wpt_uint16                   usDataOffset        = 0;
  wpt_uint16                   usSendSize          = 0;
  WDI_Status                   wdiStatus           = WDI_STATUS_SUCCESS; 
  wpt_macAddr                  macBSSID;
  wpt_uint8                    ucCurrentBSSSesIdx; 
  tSetStaKeyReqMsg             halSetStaKeyReqMsg  = {{0}};
  wpt_uint8                    keyIndex            = 0;

  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) || ( NULL == pEventData->pEventData ) ||
      ( NULL == pEventData->pCBfnc ))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

   pwdiSetSTAKeyParams = (WDI_SetSTAKeyReqParamsType*)pEventData->pEventData;
   wdiSetSTAKeyRspCb   = (WDI_SetSTAKeyRspCb)pEventData->pCBfnc;
  /*-------------------------------------------------------------------------
    Check to see if we are in the middle of an association, if so queue, if
    not it means it is free to process request 
  -------------------------------------------------------------------------*/
  wpalMutexAcquire(&pWDICtx->wptMutex);

  /*------------------------------------------------------------------------
    Find the BSS for which the request is made and identify WDI session
  ------------------------------------------------------------------------*/
  if ( WDI_STATUS_SUCCESS != WDI_STATableGetStaBSSIDAddr(pWDICtx, 
                                  pwdiSetSTAKeyParams->wdiKeyInfo.ucSTAIdx, 
                                  &macBSSID))
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
             "This station does not exist in the WDI Station Table %d");
    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_FAILURE; 
  }

  ucCurrentBSSSesIdx = WDI_FindAssocSession( pWDICtx, macBSSID, &pBSSSes); 
  if ( NULL == pBSSSes ) 
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS does not yet exist");

    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_NOT_ALLOWED; 
  }
 
  /*------------------------------------------------------------------------
    Check if this BSS is being currently processed or queued,
    if queued - queue the new request as well 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_TRUE == pBSSSes->bAssocReqQueued )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS exists but currently queued");

    wdiStatus = WDI_QueueAssocRequest( pWDICtx, pBSSSes, pEventData); 
    wpalMutexRelease(&pWDICtx->wptMutex);
    return wdiStatus; 
  }


  wpalMutexRelease(&pWDICtx->wptMutex);
  /*-----------------------------------------------------------------------
    Get message buffer
  -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_SET_STA_KEY_REQ, 
                        sizeof(halSetStaKeyReqMsg.setStaKeyParams),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset + sizeof(halSetStaKeyReqMsg.setStaKeyParams) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in set bss key req %x %x %x",
                pEventData, pwdiSetSTAKeyParams, wdiSetSTAKeyRspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }
  /*-----------------------------------------------------------------------
    Copy the STA Key parameters into the HAL message
  -----------------------------------------------------------------------*/
  halSetStaKeyReqMsg.setStaKeyParams.encType = 
      WDI_2_HAL_ENC_TYPE (pwdiSetSTAKeyParams->wdiKeyInfo.wdiEncType);

  halSetStaKeyReqMsg.setStaKeyParams.wepType = 
      WDI_2_HAL_WEP_TYPE (pwdiSetSTAKeyParams->wdiKeyInfo.wdiWEPType );

  halSetStaKeyReqMsg.setStaKeyParams.staIdx = pwdiSetSTAKeyParams->wdiKeyInfo.ucSTAIdx;

  halSetStaKeyReqMsg.setStaKeyParams.defWEPIdx = pwdiSetSTAKeyParams->wdiKeyInfo.ucDefWEPIdx;

  halSetStaKeyReqMsg.setStaKeyParams.singleTidRc = pwdiSetSTAKeyParams->wdiKeyInfo.ucSingleTidRc;

#ifdef WLAN_SOFTAP_FEATURE
  for(keyIndex = 0; keyIndex < pwdiSetSTAKeyParams->wdiKeyInfo.ucNumKeys ;
                                                                 keyIndex++)
  {
    halSetStaKeyReqMsg.setStaKeyParams.key[keyIndex].keyId = 
                      pwdiSetSTAKeyParams->wdiKeyInfo.wdiKey[keyIndex].keyId;
    halSetStaKeyReqMsg.setStaKeyParams.key[keyIndex].unicast =
                     pwdiSetSTAKeyParams->wdiKeyInfo.wdiKey[keyIndex].unicast;
    halSetStaKeyReqMsg.setStaKeyParams.key[keyIndex].keyDirection =
                pwdiSetSTAKeyParams->wdiKeyInfo.wdiKey[keyIndex].keyDirection;
    wpalMemoryCopy(halSetStaKeyReqMsg.setStaKeyParams.key[keyIndex].keyRsc,
                     pwdiSetSTAKeyParams->wdiKeyInfo.wdiKey[keyIndex].keyRsc, 
                     WDI_MAX_KEY_RSC_LEN);
    halSetStaKeyReqMsg.setStaKeyParams.key[keyIndex].paeRole = 
                     pwdiSetSTAKeyParams->wdiKeyInfo.wdiKey[keyIndex].paeRole;
    halSetStaKeyReqMsg.setStaKeyParams.key[keyIndex].keyLength = 
                   pwdiSetSTAKeyParams->wdiKeyInfo.wdiKey[keyIndex].keyLength;
    wpalMemoryCopy(halSetStaKeyReqMsg.setStaKeyParams.key[keyIndex].key,
                         pwdiSetSTAKeyParams->wdiKeyInfo.wdiKey[keyIndex].key, 
                        WDI_MAX_KEY_LENGTH);
   }
#else
  halSetStaKeyReqMsg.setStaKeyParams.key.keyId = 
                      pwdiSetSTAKeyParams->wdiKeyInfo.wdiKey[0].keyId;
  halSetStaKeyReqMsg.setStaKeyParams.key.unicast =
                     pwdiSetSTAKeyParams->wdiKeyInfo.wdiKey[0].unicast;
  halSetStaKeyReqMsg.setStaKeyParams.key.keyDirection =
                pwdiSetSTAKeyParams->wdiKeyInfo.wdiKey[0].keyDirection;
  wpalMemoryCopy(halSetStaKeyReqMsg.setStaKeyParams.key.keyRsc,
                     pwdiSetSTAKeyParams->wdiKeyInfo.wdiKey[0].keyRsc, 
                     WDI_MAX_KEY_RSC_LEN);
  halSetStaKeyReqMsg.setStaKeyParams.key.paeRole = 
                     pwdiSetSTAKeyParams->wdiKeyInfo.wdiKey[0].paeRole;
  halSetStaKeyReqMsg.setStaKeyParams.key.keyLength = 
                   pwdiSetSTAKeyParams->wdiKeyInfo.wdiKey[0].keyLength;
  wpalMemoryCopy(halSetStaKeyReqMsg.setStaKeyParams.key.key,
                         pwdiSetSTAKeyParams->wdiKeyInfo.wdiKey[0].key, 
                        WDI_MAX_KEY_LENGTH);
#endif

  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                    &halSetStaKeyReqMsg.setStaKeyParams, 
                    sizeof(halSetStaKeyReqMsg.setStaKeyParams)); 

  pWDICtx->wdiReqStatusCB     = pwdiSetSTAKeyParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiSetSTAKeyParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Set STA Key Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiSetSTAKeyRspCb, pEventData->pUserData, 
                       WDI_SET_STA_KEY_RESP); 

}/*WDI_ProcessSetSTABcastKeyReq*/

/**
 @brief Process Remove STA Key Request function (called when 
        Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessRemoveStaBcastKeyReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_RemoveSTAKeyReqParamsType*  pwdiRemoveSTABcastKeyParams;
  WDI_RemoveSTAKeyRspCb           wdiRemoveSTAKeyRspCb;
  WDI_BSSSessionType*             pBSSSes                = NULL;
  wpt_uint8*                      pSendBuffer            = NULL; 
  wpt_uint16                      usDataOffset           = 0;
  wpt_uint16                      usSendSize             = 0;
  WDI_Status                      wdiStatus              = WDI_STATUS_SUCCESS; 
  wpt_macAddr                     macBSSID;
  wpt_uint8                       ucCurrentBSSSesIdx;
  tRemoveStaKeyReqMsg             halRemoveStaBcastKeyReqMsg = {{0}};
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
 if (( NULL == pEventData ) || ( NULL == pEventData->pEventData ) ||
      ( NULL == pEventData->pCBfnc ))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  pwdiRemoveSTABcastKeyParams = (WDI_RemoveSTAKeyReqParamsType*)pEventData->pEventData;
  wdiRemoveSTAKeyRspCb   = (WDI_RemoveSTAKeyRspCb)pEventData->pCBfnc;
  /*-------------------------------------------------------------------------
    Check to see if we are in the middle of an association, if so queue, if
    not it means it is free to process request 
  -------------------------------------------------------------------------*/
  wpalMutexAcquire(&pWDICtx->wptMutex);

  /*------------------------------------------------------------------------
    Find the BSS for which the request is made and identify WDI session
  ------------------------------------------------------------------------*/
  if ( WDI_STATUS_SUCCESS != WDI_STATableGetStaBSSIDAddr(pWDICtx, 
                             pwdiRemoveSTABcastKeyParams->wdiKeyInfo.ucSTAIdx, 
                             &macBSSID))
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
             "This station does not exist in the WDI Station Table %d");
    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_FAILURE; 
  }

  ucCurrentBSSSesIdx = WDI_FindAssocSession( pWDICtx, macBSSID, &pBSSSes); 
  if ( NULL == pBSSSes ) 
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS does not yet exist");

    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_NOT_ALLOWED; 
  }
 
  /*------------------------------------------------------------------------
    Check if this BSS is being currently processed or queued,
    if queued - queue the new request as well 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_TRUE == pBSSSes->bAssocReqQueued )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS exists but currently queued");

    wdiStatus = WDI_QueueAssocRequest( pWDICtx, pBSSSes, pEventData); 
    wpalMutexRelease(&pWDICtx->wptMutex);
    return wdiStatus; 
  }



  wpalMutexRelease(&pWDICtx->wptMutex);
  /*-----------------------------------------------------------------------
    Get message buffer
  -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_RMV_STA_BCAST_KEY_REQ, 
                        sizeof(halRemoveStaBcastKeyReqMsg.removeStaKeyParams),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset + sizeof(halRemoveStaBcastKeyReqMsg.removeStaKeyParams) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in set bss key req %x %x %x",
                pEventData, pwdiRemoveSTABcastKeyParams, wdiRemoveSTAKeyRspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  /*-----------------------------------------------------------------------
    Copy the Key parameters into the HAL message
  -----------------------------------------------------------------------*/

  halRemoveStaBcastKeyReqMsg.removeStaKeyParams.staIdx = 
      pwdiRemoveSTABcastKeyParams->wdiKeyInfo.ucSTAIdx;

  halRemoveStaBcastKeyReqMsg.removeStaKeyParams.encType = 
      WDI_2_HAL_ENC_TYPE (pwdiRemoveSTABcastKeyParams->wdiKeyInfo.wdiEncType);

  halRemoveStaBcastKeyReqMsg.removeStaKeyParams.keyId = 
      pwdiRemoveSTABcastKeyParams->wdiKeyInfo.ucKeyId;

  halRemoveStaBcastKeyReqMsg.removeStaKeyParams.unicast = 
      pwdiRemoveSTABcastKeyParams->wdiKeyInfo.ucUnicast;

  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                    &halRemoveStaBcastKeyReqMsg.removeStaKeyParams, 
                    sizeof(halRemoveStaBcastKeyReqMsg.removeStaKeyParams)); 

  pWDICtx->wdiReqStatusCB     = pwdiRemoveSTABcastKeyParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiRemoveSTABcastKeyParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Remove STA Key Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiRemoveSTAKeyRspCb, pEventData->pUserData,
                       WDI_RMV_STA_KEY_RESP); 

}/*WDI_ProcessRemoveSTABcastKeyReq*/

/*==========================================================================
                   QOS and BA PROCESSING REQUEST API 
==========================================================================*/
/**
 @brief Process Add TSpec Request function (called when Main FSM
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessAddTSpecReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_AddTSReqParamsType*  pwdiAddTSParams;
  WDI_AddTsRspCb           wdiAddTSRspCb;
  wpt_uint8                ucCurrentBSSSesIdx  = 0; 
  WDI_BSSSessionType*      pBSSSes             = NULL;
  wpt_uint8*               pSendBuffer         = NULL; 
  wpt_uint16               usDataOffset        = 0;
  wpt_uint16               usSendSize          = 0;
  WDI_Status               wdiStatus           = WDI_STATUS_SUCCESS; 
  wpt_macAddr              macBSSID;
  tAddTsParams             halAddTsParams      = {0};
  
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) || ( NULL == pEventData->pEventData ) ||
      ( NULL == pEventData->pCBfnc ))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  pwdiAddTSParams = (WDI_AddTSReqParamsType*)pEventData->pEventData;
  wdiAddTSRspCb   = (WDI_AddTsRspCb)pEventData->pCBfnc;
  /*-------------------------------------------------------------------------
    Check to see if we are in the middle of an association, if so queue, if
    not it means it is free to process request 
  -------------------------------------------------------------------------*/
  wpalMutexAcquire(&pWDICtx->wptMutex);

  /*------------------------------------------------------------------------
    Find the BSS for which the request is made and identify WDI session
  ------------------------------------------------------------------------*/
  if ( WDI_STATUS_SUCCESS != WDI_STATableGetStaBSSIDAddr(pWDICtx, 
                                        pwdiAddTSParams->wdiTsInfo.ucSTAIdx, 
                                        &macBSSID))
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
             "This station does not exist in the WDI Station Table %d");
    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_FAILURE; 
  }

  ucCurrentBSSSesIdx = WDI_FindAssocSession( pWDICtx, macBSSID, &pBSSSes); 
  if ( NULL == pBSSSes ) 
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS does not yet exist");

    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_NOT_ALLOWED; 
  }
 
  /*------------------------------------------------------------------------
    Check if this BSS is being currently processed or queued,
    if queued - queue the new request as well 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_TRUE == pBSSSes->bAssocReqQueued )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS exists but currently queued");

    wdiStatus = WDI_QueueAssocRequest( pWDICtx, pBSSSes, pEventData); 
    wpalMutexRelease(&pWDICtx->wptMutex);
    return wdiStatus; 
  }

  wpalMutexRelease(&pWDICtx->wptMutex);
  /*-----------------------------------------------------------------------
    Get message buffer
    ! TO DO : proper conversion into the HAL Message Request Format 
  -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_ADD_TS_REQ, 
                                                    sizeof(halAddTsParams), 
                                                    &pSendBuffer, &usDataOffset, 
                                                    &usSendSize))||
      ( usSendSize < (usDataOffset + sizeof(halAddTsParams) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in set bss key req %x %x %x",
                pEventData, pwdiAddTSParams, wdiAddTSRspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  halAddTsParams.staIdx = pwdiAddTSParams->wdiTsInfo.ucSTAIdx;
  halAddTsParams.tspecIdx = pwdiAddTSParams->wdiTsInfo.ucTspecIdx;

  //TSPEC IE
  halAddTsParams.tspec.type = pwdiAddTSParams->wdiTsInfo.wdiTspecIE.ucType;
  halAddTsParams.tspec.length = pwdiAddTSParams->wdiTsInfo.wdiTspecIE.ucLength;
  halAddTsParams.tspec.nomMsduSz = 
     pwdiAddTSParams->wdiTsInfo.wdiTspecIE.usNomMsduSz;
  halAddTsParams.tspec.maxMsduSz = 
     pwdiAddTSParams->wdiTsInfo.wdiTspecIE.usMaxMsduSz;
  halAddTsParams.tspec.minSvcInterval = 
     pwdiAddTSParams->wdiTsInfo.wdiTspecIE.uMinSvcInterval;
  halAddTsParams.tspec.maxSvcInterval = 
     pwdiAddTSParams->wdiTsInfo.wdiTspecIE.uMaxSvcInterval;
  halAddTsParams.tspec.inactInterval = 
     pwdiAddTSParams->wdiTsInfo.wdiTspecIE.uInactInterval;
  halAddTsParams.tspec.suspendInterval = 
     pwdiAddTSParams->wdiTsInfo.wdiTspecIE.uSuspendInterval;
  halAddTsParams.tspec.svcStartTime = 
     pwdiAddTSParams->wdiTsInfo.wdiTspecIE.uSvcStartTime;
  halAddTsParams.tspec.minDataRate = 
     pwdiAddTSParams->wdiTsInfo.wdiTspecIE.uMinDataRate;
  halAddTsParams.tspec.meanDataRate = 
     pwdiAddTSParams->wdiTsInfo.wdiTspecIE.uMeanDataRate;
  halAddTsParams.tspec.peakDataRate = 
     pwdiAddTSParams->wdiTsInfo.wdiTspecIE.uPeakDataRate;
  halAddTsParams.tspec.maxBurstSz = 
     pwdiAddTSParams->wdiTsInfo.wdiTspecIE.uMaxBurstSz;
  halAddTsParams.tspec.delayBound = 
     pwdiAddTSParams->wdiTsInfo.wdiTspecIE.uDelayBound;
  halAddTsParams.tspec.minPhyRate = 
     pwdiAddTSParams->wdiTsInfo.wdiTspecIE.uMinPhyRate;
  halAddTsParams.tspec.surplusBw = 
     pwdiAddTSParams->wdiTsInfo.wdiTspecIE.usSurplusBw;
  halAddTsParams.tspec.mediumTime = 
     pwdiAddTSParams->wdiTsInfo.wdiTspecIE.usMediumTime;

  //TSPEC IE : TS INFO : TRAFFIC
  halAddTsParams.tspec.tsinfo.traffic.ackPolicy = 
     pwdiAddTSParams->wdiTsInfo.wdiTspecIE.wdiTSinfo.wdiTraffic.accessPolicy;
  halAddTsParams.tspec.tsinfo.traffic.userPrio = 
     pwdiAddTSParams->wdiTsInfo.wdiTspecIE.wdiTSinfo.wdiTraffic.userPrio;
  halAddTsParams.tspec.tsinfo.traffic.psb = 
     pwdiAddTSParams->wdiTsInfo.wdiTspecIE.wdiTSinfo.wdiTraffic.psb;
  halAddTsParams.tspec.tsinfo.traffic.aggregation = 
     pwdiAddTSParams->wdiTsInfo.wdiTspecIE.wdiTSinfo.wdiTraffic.aggregation;
  halAddTsParams.tspec.tsinfo.traffic.direction = 
     pwdiAddTSParams->wdiTsInfo.wdiTspecIE.wdiTSinfo.wdiTraffic.direction;
  halAddTsParams.tspec.tsinfo.traffic.tsid = 
     pwdiAddTSParams->wdiTsInfo.wdiTspecIE.wdiTSinfo.wdiTraffic.tsid;
  halAddTsParams.tspec.tsinfo.traffic.trafficType = 
     pwdiAddTSParams->wdiTsInfo.wdiTspecIE.wdiTSinfo.wdiTraffic.trafficType;

  //TSPEC IE : TS INFO : SCHEDULE
  halAddTsParams.tspec.tsinfo.schedule.rsvd = 
     pwdiAddTSParams->wdiTsInfo.wdiTspecIE.wdiTSinfo.wdiSchedule.rsvd;
  halAddTsParams.tspec.tsinfo.schedule.schedule = 
     pwdiAddTSParams->wdiTsInfo.wdiTspecIE.wdiTSinfo.wdiSchedule.schedule;

  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                  &halAddTsParams, 
                  sizeof(halAddTsParams)); 

  pWDICtx->wdiReqStatusCB     = pwdiAddTSParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiAddTSParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Add TS Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiAddTSRspCb, pEventData->pUserData,
                       WDI_ADD_TS_RESP); 
}/*WDI_ProcessAddTSpecReq*/


/**
 @brief Process Del TSpec Request function (called when Main FSM
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessDelTSpecReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_DelTSReqParamsType*      pwdiDelTSParams;
  WDI_DelTsRspCb               wdiDelTSRspCb;
  wpt_uint8                    ucCurrentBSSSesIdx  = 0; 
  WDI_BSSSessionType*          pBSSSes             = NULL;
  wpt_uint8*                   pSendBuffer         = NULL; 
  wpt_uint16                   usDataOffset        = 0;
  wpt_uint16                   usSendSize          = 0;
  WDI_Status                   wdiStatus           = WDI_STATUS_SUCCESS; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) || ( NULL == pEventData->pEventData ) ||
      ( NULL == pEventData->pCBfnc ))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  pwdiDelTSParams = (WDI_DelTSReqParamsType*)pEventData->pEventData;
  wdiDelTSRspCb   = (WDI_DelTsRspCb)pEventData->pCBfnc;

  /*-------------------------------------------------------------------------
    Check to see if we are in the middle of an association, if so queue, if
    not it means it is free to process request 
  -------------------------------------------------------------------------*/
  wpalMutexAcquire(&pWDICtx->wptMutex);

  /*------------------------------------------------------------------------
    Find the BSS for which the request is made 
  ------------------------------------------------------------------------*/
  ucCurrentBSSSesIdx = WDI_FindAssocSession( pWDICtx, 
                           pwdiDelTSParams->wdiDelTSInfo.macBSSID, 
                          &pBSSSes); 

  if ( NULL == pBSSSes ) 
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS does not yet exist");

    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Check if this BSS is being currently processed or queued,
    if queued - queue the new request as well 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_TRUE == pBSSSes->bAssocReqQueued )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS exists but currently queued");

    wdiStatus = WDI_QueueAssocRequest( pWDICtx, pBSSSes, pEventData); 
    wpalMutexRelease(&pWDICtx->wptMutex);
    return wdiStatus; 
  }


  wpalMutexRelease(&pWDICtx->wptMutex);
  /*-----------------------------------------------------------------------
    Get message buffer
    ! TO DO : proper conversion into the HAL Message Request Format 
  -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_DEL_TS_REQ, 
                        sizeof(pwdiDelTSParams->wdiDelTSInfo),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset + sizeof(pwdiDelTSParams->wdiDelTSInfo) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in set bss key req %x %x %x",
                pEventData, pwdiDelTSParams, wdiDelTSRspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                  &pwdiDelTSParams->wdiDelTSInfo, 
                  sizeof(pwdiDelTSParams->wdiDelTSInfo)); 

  pWDICtx->wdiReqStatusCB     = pwdiDelTSParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiDelTSParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Del TS Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiDelTSRspCb, pEventData->pUserData, WDI_DEL_TS_RESP); 
}/*WDI_ProcessDelTSpecReq*/

/**
 @brief Process Update EDCA Params Request function (called when
        Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessUpdateEDCAParamsReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_UpdateEDCAParamsType*    pwdiUpdateEDCAParams;
  WDI_UpdateEDCAParamsRspCb    wdiUpdateEDCARspCb;
  wpt_uint8                    ucCurrentBSSSesIdx  = 0; 
  WDI_BSSSessionType*          pBSSSes             = NULL;
  wpt_uint8*                   pSendBuffer         = NULL; 
  wpt_uint16                   usDataOffset         = 0;
  wpt_uint16                   usSendSize           = 0;
  WDI_Status                   wdiStatus; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) || ( NULL == pEventData->pEventData ) ||
      ( NULL == pEventData->pCBfnc ))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  pwdiUpdateEDCAParams = (WDI_UpdateEDCAParamsType*)pEventData->pEventData;
  wdiUpdateEDCARspCb   = (WDI_UpdateEDCAParamsRspCb)pEventData->pCBfnc;
  /*-------------------------------------------------------------------------
    Check to see if we are in the middle of an association, if so queue, if
    not it means it is free to process request 
  -------------------------------------------------------------------------*/
  wpalMutexAcquire(&pWDICtx->wptMutex);

  /*------------------------------------------------------------------------
    Find the BSS for which the request is made 
  ------------------------------------------------------------------------*/
  ucCurrentBSSSesIdx = WDI_FindAssocSessionByBSSIdx( pWDICtx, 
                           pwdiUpdateEDCAParams->wdiEDCAInfo.ucBssIdx, 
                          &pBSSSes); 

  if ( NULL == pBSSSes ) 
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS does not yet exist");

    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Check if this BSS is being currently processed or queued,
    if queued - queue the new request as well 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_TRUE == pBSSSes->bAssocReqQueued )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS exists but currently queued");

    wdiStatus = WDI_QueueAssocRequest( pWDICtx, pBSSSes, pEventData); 
    wpalMutexRelease(&pWDICtx->wptMutex);
    return wdiStatus; 
  }


  wpalMutexRelease(&pWDICtx->wptMutex);
  /*-----------------------------------------------------------------------
    Get message buffer
    ! TO DO : proper conversion into the HAL Message Request Format 
  -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_UPD_EDCA_PRMS_REQ, 
                        sizeof(pwdiUpdateEDCAParams->wdiEDCAInfo),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset + sizeof(pwdiUpdateEDCAParams->wdiEDCAInfo) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in set bss key req %x %x %x",
                pEventData, pwdiUpdateEDCAParams, wdiUpdateEDCARspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                  &pwdiUpdateEDCAParams->wdiEDCAInfo, 
                  sizeof(pwdiUpdateEDCAParams->wdiEDCAInfo)); 

  pWDICtx->wdiReqStatusCB     = pwdiUpdateEDCAParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiUpdateEDCAParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Update EDCA Params Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiUpdateEDCARspCb, pEventData->pUserData, 
                       WDI_UPD_EDCA_PRMS_RESP); 
}/*WDI_ProcessUpdateEDCAParamsReq*/

/**
 @brief Process Add BA Request function (called when Main FSM 
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessAddBASessionReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_AddBASessionReqParamsType*  pwdiAddBASessionParams;
  WDI_AddBASessionRspCb           wdiAddBASessionRspCb;
  wpt_uint8                       ucCurrentBSSSesIdx  = 0; 
  WDI_BSSSessionType*             pBSSSes             = NULL;
  wpt_uint8*                      pSendBuffer         = NULL; 
  wpt_uint16                      usDataOffset        = 0;
  wpt_uint16                      usSendSize          = 0;
  WDI_Status                      wdiStatus           = WDI_STATUS_SUCCESS; 
  wpt_macAddr                     macBSSID;

  tAddBASessionReqMsg             halAddBASessionReq;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) || ( NULL == pEventData->pEventData ) ||
      ( NULL == pEventData->pCBfnc ))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  pwdiAddBASessionParams = 
                  (WDI_AddBASessionReqParamsType*)pEventData->pEventData;
  wdiAddBASessionRspCb = 
                  (WDI_AddBASessionRspCb)pEventData->pCBfnc;
  /*-------------------------------------------------------------------------
    Check to see if we are in the middle of an association, if so queue, if
    not it means it is free to process request 
  -------------------------------------------------------------------------*/
  wpalMutexAcquire(&pWDICtx->wptMutex);

  /*------------------------------------------------------------------------
    Find the BSS for which the request is made 
  ------------------------------------------------------------------------*/
  if ( WDI_STATUS_SUCCESS != WDI_STATableGetStaBSSIDAddr(pWDICtx, 
                   pwdiAddBASessionParams->wdiBASessionInfoType.ucSTAIdx, 
                   &macBSSID))
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
             "This station does not exist in the WDI Station Table %d");
    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_FAILURE; 
  }


  ucCurrentBSSSesIdx = WDI_FindAssocSession( pWDICtx, macBSSID, &pBSSSes); 

  if ( NULL == pBSSSes ) 
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS does not yet exist");

    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Check if this BSS is being currently processed or queued,
    if queued - queue the new request as well 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_TRUE == pBSSSes->bAssocReqQueued )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS exists but currently queued");

    wdiStatus = WDI_QueueAssocRequest( pWDICtx, pBSSSes, pEventData); 
    wpalMutexRelease(&pWDICtx->wptMutex);
    return wdiStatus; 
  }


  wpalMutexRelease(&pWDICtx->wptMutex);
  /*-----------------------------------------------------------------------
    Get message buffer
  -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, 
                        WDI_ADD_BA_SESSION_REQ, 
                        sizeof(halAddBASessionReq.addBASessionParams),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < 
            (usDataOffset + sizeof(halAddBASessionReq.addBASessionParams) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in Add BA session req %x %x %x",
                pEventData, pwdiAddBASessionParams, wdiAddBASessionRspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  halAddBASessionReq.addBASessionParams.staIdx =
                  pwdiAddBASessionParams->wdiBASessionInfoType.ucSTAIdx;
  wpalMemoryCopy(halAddBASessionReq.addBASessionParams.peerMacAddr,
                  pwdiAddBASessionParams->wdiBASessionInfoType.macPeerAddr,
                  WDI_MAC_ADDR_LEN);
  halAddBASessionReq.addBASessionParams.baTID =
                  pwdiAddBASessionParams->wdiBASessionInfoType.ucBaTID;
  halAddBASessionReq.addBASessionParams.baPolicy =
                  pwdiAddBASessionParams->wdiBASessionInfoType.ucBaPolicy;
  halAddBASessionReq.addBASessionParams.baBufferSize =
                  pwdiAddBASessionParams->wdiBASessionInfoType.usBaBufferSize;
  halAddBASessionReq.addBASessionParams.baTimeout =
                  pwdiAddBASessionParams->wdiBASessionInfoType.usBaTimeout;
  halAddBASessionReq.addBASessionParams.baSSN =
                  pwdiAddBASessionParams->wdiBASessionInfoType.usBaSSN;
  halAddBASessionReq.addBASessionParams.baDirection =
                  pwdiAddBASessionParams->wdiBASessionInfoType.ucBaDirection;

  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                  &halAddBASessionReq.addBASessionParams, 
                  sizeof(halAddBASessionReq.addBASessionParams)); 

  pWDICtx->wdiReqStatusCB     = pwdiAddBASessionParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiAddBASessionParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Start Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        wdiAddBASessionRspCb, pEventData->pUserData, 
                        WDI_ADD_BA_SESSION_RESP); 
}/*WDI_ProcessAddBASessionReq*/

/**
 @brief Process Del BA Request function (called when Main FSM 
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessDelBAReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_DelBAReqParamsType*      pwdiDelBAParams;
  WDI_DelBARspCb               wdiDelBARspCb;
  wpt_uint8                    ucCurrentBSSSesIdx  = 0; 
  WDI_BSSSessionType*          pBSSSes             = NULL;
  wpt_uint8*                   pSendBuffer         = NULL; 
  wpt_uint16                   usDataOffset        = 0;
  wpt_uint16                   usSendSize          = 0;
  WDI_Status                   wdiStatus           = WDI_STATUS_SUCCESS; 
  wpt_macAddr                  macBSSID;
  tDelBAParams                 halDelBAparam;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) || ( NULL == pEventData->pEventData ) ||
      ( NULL == pEventData->pCBfnc ))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  pwdiDelBAParams = (WDI_DelBAReqParamsType*)pEventData->pEventData;
  wdiDelBARspCb   = (WDI_DelBARspCb)pEventData->pCBfnc;
  /*-------------------------------------------------------------------------
    Check to see if we are in the middle of an association, if so queue, if
    not it means it is free to process request 
  -------------------------------------------------------------------------*/
  wpalMutexAcquire(&pWDICtx->wptMutex);

  /*------------------------------------------------------------------------
    Find the BSS for which the request is made 
  ------------------------------------------------------------------------*/
  if ( WDI_STATUS_SUCCESS != WDI_STATableGetStaBSSIDAddr(pWDICtx, 
                                     pwdiDelBAParams->wdiBAInfo.ucSTAIdx, 
                                     &macBSSID))
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
             "This station does not exist in the WDI Station Table %d");
    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_FAILURE; 
  }

  ucCurrentBSSSesIdx = WDI_FindAssocSession( pWDICtx, macBSSID, &pBSSSes); 

  if ( NULL == pBSSSes ) 
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS does not yet exist");

    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Check if this BSS is being currently processed or queued,
    if queued - queue the new request as well 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_TRUE == pBSSSes->bAssocReqQueued )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS exists but currently queued");

    wdiStatus = WDI_QueueAssocRequest( pWDICtx, pBSSSes, pEventData); 
    wpalMutexRelease(&pWDICtx->wptMutex);
    return wdiStatus; 
  }

  wpalMutexRelease(&pWDICtx->wptMutex);
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_DEL_BA_REQ, 
                        sizeof(halDelBAparam),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset + sizeof(halDelBAparam) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer for DEL BA req %x %x %x",
                pEventData, pwdiDelBAParams, wdiDelBARspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  halDelBAparam.staIdx = pwdiDelBAParams->wdiBAInfo.ucSTAIdx;
  halDelBAparam.baTID = pwdiDelBAParams->wdiBAInfo.ucBaTID;
  halDelBAparam.baDirection = pwdiDelBAParams->wdiBAInfo.ucBaDirection;

  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                  &halDelBAparam, 
                  sizeof(halDelBAparam)); 

  pWDICtx->wdiReqStatusCB     = pwdiDelBAParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiDelBAParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Start Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiDelBARspCb, pEventData->pUserData, WDI_DEL_BA_RESP); 
}/*WDI_ProcessDelBAReq*/

/**
 @brief Process Flush AC Request function (called when Main FSM 
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessFlushAcReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_FlushAcReqParamsType*    pwdiFlushAcParams = NULL;
   WDI_FlushAcRspCb             wdiFlushAcRspCb;
   wpt_uint8*                   pSendBuffer         = NULL; 
   wpt_uint16                   usDataOffset        = 0;
   wpt_uint16                   usSendSize          = 0;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pEventData ) || ( NULL == pEventData->pEventData ) ||
       ( NULL == pEventData->pCBfnc ))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   pwdiFlushAcParams = (WDI_FlushAcReqParamsType*)pEventData->pEventData;
   wdiFlushAcRspCb   = (WDI_FlushAcRspCb)pEventData->pCBfnc;
   /*-----------------------------------------------------------------------
     Get message buffer
     ! TO DO : proper conversion into the HAL Message Request Format 
   -----------------------------------------------------------------------*/
   if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_FLUSH_AC_REQ, 
                         sizeof(pwdiFlushAcParams->wdiFlushAcInfo),
                         &pSendBuffer, &usDataOffset, &usSendSize))||
       ( usSendSize < (usDataOffset + sizeof(pwdiFlushAcParams->wdiFlushAcInfo) )))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
               "Unable to get send buffer in set bss key req %x %x %x",
                 pEventData, pwdiFlushAcParams, wdiFlushAcRspCb);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   wpalMemoryCopy( pSendBuffer+usDataOffset, 
                   &pwdiFlushAcParams->wdiFlushAcInfo, 
                   sizeof(pwdiFlushAcParams->wdiFlushAcInfo)); 

   pWDICtx->wdiReqStatusCB     = pwdiFlushAcParams->wdiReqStatusCB;
   pWDICtx->pReqStatusUserData = pwdiFlushAcParams->pUserData; 

   /*-------------------------------------------------------------------------
     Send Start Request to HAL 
   -------------------------------------------------------------------------*/
   return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        wdiFlushAcRspCb, pEventData->pUserData, WDI_FLUSH_AC_RESP); 
}/*WDI_ProcessFlushAcReq*/

/**
 @brief Process BT AMP event Request function (called when Main 
        FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessBtAmpEventReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_BtAmpEventParamsType*    pwdiBtAmpEventParams = NULL;
   WDI_BtAmpEventRspCb          wdiBtAmpEventRspCb;
   wpt_uint8*                   pSendBuffer         = NULL; 
   wpt_uint16                   usDataOffset        = 0;
   wpt_uint16                   usSendSize          = 0;

   tBtAmpEventMsg               haltBtAmpEventMsg;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pEventData ) || ( NULL == pEventData->pEventData ) ||
       ( NULL == pEventData->pCBfnc ))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   pwdiBtAmpEventParams = (WDI_BtAmpEventParamsType*)pEventData->pEventData;
   wdiBtAmpEventRspCb   = (WDI_BtAmpEventRspCb)pEventData->pCBfnc;
   /*-----------------------------------------------------------------------
     Get message buffer
     ! TO DO : proper conversion into the HAL Message Request Format 
   -----------------------------------------------------------------------*/
   if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_BTAMP_EVENT_REQ, 
                         sizeof(haltBtAmpEventMsg.btAmpEventParams),
                         &pSendBuffer, &usDataOffset, &usSendSize))||
       ( usSendSize < (usDataOffset + sizeof(haltBtAmpEventMsg.btAmpEventParams) )))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
               "Unable to get send buffer in BT AMP event req %x %x %x",
                 pEventData, pwdiBtAmpEventParams, wdiBtAmpEventRspCb);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   haltBtAmpEventMsg.btAmpEventParams.btAmpEventType = 
      pwdiBtAmpEventParams->wdiBtAmpEventInfo.ucBtAmpEventType;
   wpalMemoryCopy( pSendBuffer+usDataOffset, 
                   &haltBtAmpEventMsg.btAmpEventParams, 
                   sizeof(haltBtAmpEventMsg.btAmpEventParams)); 

   pWDICtx->wdiReqStatusCB     = pwdiBtAmpEventParams->wdiReqStatusCB;
   pWDICtx->pReqStatusUserData = pwdiBtAmpEventParams->pUserData; 

   /*-------------------------------------------------------------------------
     Send Start Request to HAL 
   -------------------------------------------------------------------------*/
   return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        wdiBtAmpEventRspCb, pEventData->pUserData, WDI_BTAMP_EVENT_RESP); 
}/*WDI_ProcessBtAmpEventReq*/

/**
 @brief Process Add STA self Request function (called when Main FSM
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessAddSTASelfReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_AddSTASelfReqParamsType*          pwdiAddSTASelfReqParams;
  WDI_AddSTASelfParamsRspCb             wdiAddSTASelfReqRspCb;
  wpt_uint8*                            pSendBuffer         = NULL; 
  wpt_uint16                            usDataOffset        = 0;
  wpt_uint16                            usSendSize          = 0;
  tAddStaSelfParams                     halAddSTASelfParams; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData) ||
      ( NULL == pEventData->pCBfnc))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  pwdiAddSTASelfReqParams = 
    (WDI_AddSTASelfReqParamsType*)pEventData->pEventData;
  wdiAddSTASelfReqRspCb = 
    (WDI_AddSTASelfParamsRspCb)pEventData->pCBfnc;
  /*-----------------------------------------------------------------------
    Get message buffer
  -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, 
                        WDI_ADD_STA_SELF_REQ, 
                        sizeof(tAddStaSelfParams),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset + sizeof(tAddStaSelfParams) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in ADD STA SELF REQ %x %x %x",
     pEventData, pwdiAddSTASelfReqParams, wdiAddSTASelfReqRspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  /* Cache the request for response processing */
  wpalMemoryCopy(&pWDICtx->wdiCacheAddSTASelfReq, pwdiAddSTASelfReqParams, 
                 sizeof(pWDICtx->wdiCacheAddSTASelfReq));

  wpalMemoryCopy(halAddSTASelfParams.selfMacAddr, 
                   pwdiAddSTASelfReqParams->wdiAddSTASelfInfo.selfMacAddr, 6) ;

  wpalMemoryCopy( pSendBuffer+usDataOffset, &halAddSTASelfParams, 
                                         sizeof(tAddStaSelfParams)); 

  pWDICtx->wdiReqStatusCB     = pwdiAddSTASelfReqParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiAddSTASelfReqParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Update Probe Resp Template Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiAddSTASelfReqRspCb, pEventData->pUserData, 
                       WDI_ADD_STA_SELF_RESP); 
}/*WDI_ProcessAddSTASelfReq*/



/**
 @brief Process Del Sta Self Request function (called when Main 
        FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessDelSTASelfReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_DelSTASelfReqParamsType*      pwdiDelStaSelfReqParams;
  WDI_DelSTASelfRspCb               wdiDelStaSelfRspCb;
  wpt_uint8*                   pSendBuffer         = NULL; 
  wpt_uint16                   usDataOffset        = 0;
  wpt_uint16                   usSendSize          = 0;
  tDelStaSelfParams            halSetDelSelfSTAParams;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) || ( NULL == pEventData->pEventData ) ||
      ( NULL == pEventData->pCBfnc ))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  pwdiDelStaSelfReqParams = 
                 (WDI_DelSTASelfReqParamsType*)pEventData->pEventData;
  wdiDelStaSelfRspCb      = (WDI_DelSTASelfRspCb)pEventData->pCBfnc;

   /*-----------------------------------------------------------------------
     Get message buffer
     ! TO DO : proper conversion into the HAL Message Request Format 
   -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_DEL_STA_SELF_REQ, 
                         sizeof(pwdiDelStaSelfReqParams->wdiDelStaSelfInfo),
                         &pSendBuffer, &usDataOffset, &usSendSize))||
    ( usSendSize < 
         (usDataOffset + sizeof(pwdiDelStaSelfReqParams->wdiDelStaSelfInfo) )))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
               "Unable to get send buffer in Del Sta Self req %x %x %x",
                 pEventData, pwdiDelStaSelfReqParams, wdiDelStaSelfRspCb);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
  }

  wpalMemoryCopy(halSetDelSelfSTAParams.selfMacAddr, 
                   pwdiDelStaSelfReqParams->wdiDelStaSelfInfo.selfMacAddr, 6) ;

  wpalMemoryCopy( pSendBuffer+usDataOffset, &halSetDelSelfSTAParams, 
                                         sizeof(tDelStaSelfParams)); 

  pWDICtx->wdiReqStatusCB     = pwdiDelStaSelfReqParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiDelStaSelfReqParams->pUserData; 

  /*-------------------------------------------------------------------------
     Send Start Request to HAL 
   -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        wdiDelStaSelfRspCb, pEventData->pUserData, 
                                                     WDI_DEL_STA_SELF_RESP);

}


/**
 @brief Process Host Resume Request function (called when Main 
        FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessHostResumeReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_ResumeParamsType*          pwdiHostResumeParams = NULL;
  WDI_HostResumeEventRspCb       wdiHostResumeRspCb;
  wpt_uint8*                     pSendBuffer         = NULL; 
  wpt_uint16                     usDataOffset        = 0;
  wpt_uint16                     usSendSize          = 0;
  tHalWlanHostResumeReqParam     halResumeReqParams;

  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
  Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) || ( NULL == pEventData->pEventData ) ||
      ( NULL == pEventData->pCBfnc ))
  {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
               "%s: Invalid parameters ",__FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
  }

   pwdiHostResumeParams = (WDI_ResumeParamsType*)pEventData->pEventData;
   wdiHostResumeRspCb   = (WDI_HostResumeEventRspCb)pEventData->pCBfnc;

  /*-----------------------------------------------------------------------
     Get message buffer
   -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, 
                         WDI_HOST_RESUME_REQ, sizeof(halResumeReqParams),
                              &pSendBuffer, &usDataOffset, &usSendSize))||
        (usSendSize < (usDataOffset + sizeof(halResumeReqParams))))
  {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
               "Unable to get send buffer in Start In Nav Meas req %x %x %x",
                 pEventData, pwdiHostResumeParams, wdiHostResumeRspCb);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
  }

  halResumeReqParams.configuredMcstBcstFilterSetting = 
     pwdiHostResumeParams->wdiResumeParams.ucConfiguredMcstBcstFilterSetting;
       
  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                  &halResumeReqParams, 
                  sizeof(halResumeReqParams)); 

  pWDICtx->wdiReqStatusCB     = pwdiHostResumeParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiHostResumeParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Start Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        wdiHostResumeRspCb, pEventData->pUserData, 
                                            WDI_HOST_RESUME_RESP); 
}/*WDI_ProcessHostResumeReq*/

/**
 @brief Process set Tx Per Tracking Parameters Request function (called 
        when Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetTxPerTrackingReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_SetTxPerTrackingReqParamsType* pwdiSetTxPerTrackingReqParams = NULL;
   WDI_SetTxPerTrackingRspCb          pwdiSetTxPerTrackingRspCb = NULL;
   wpt_uint8*               pSendBuffer         = NULL; 
   wpt_uint16               usDataOffset        = 0;
   wpt_uint16               usSendSize          = 0;
   tHalTxPerTrackingReqParam     halTxPerTrackingReqParam;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pEventData ) || ( NULL == pEventData->pEventData ) ||
       ( NULL == pEventData->pCBfnc ))
   {
       WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
                "%s: Invalid parameters ",__FUNCTION__);
       WDI_ASSERT(0);
       return WDI_STATUS_E_FAILURE; 
   }

   pwdiSetTxPerTrackingReqParams = (WDI_SetTxPerTrackingReqParamsType*)pEventData->pEventData;
   pwdiSetTxPerTrackingRspCb   = (WDI_SetTxPerTrackingRspCb)pEventData->pCBfnc;
   
   /*-----------------------------------------------------------------------
     Get message buffer
     ! TO DO : proper conversion into the HAL Message Request Format 
   -----------------------------------------------------------------------*/
   if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_SET_TX_PER_TRACKING_REQ, 
                         sizeof(halTxPerTrackingReqParam),
                         &pSendBuffer, &usDataOffset, &usSendSize))||
       ( usSendSize < (usDataOffset + sizeof(halTxPerTrackingReqParam) )))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
                  "Unable to get send buffer in set tx per tracking req %x %x %x",
                  pEventData, pwdiSetTxPerTrackingReqParams, pwdiSetTxPerTrackingRspCb);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }
   
   halTxPerTrackingReqParam.ucTxPerTrackingEnable = pwdiSetTxPerTrackingReqParams->wdiTxPerTrackingParam.ucTxPerTrackingEnable;
   halTxPerTrackingReqParam.ucTxPerTrackingPeriod = pwdiSetTxPerTrackingReqParams->wdiTxPerTrackingParam.ucTxPerTrackingPeriod;
   halTxPerTrackingReqParam.ucTxPerTrackingRatio = pwdiSetTxPerTrackingReqParams->wdiTxPerTrackingParam.ucTxPerTrackingRatio;
   halTxPerTrackingReqParam.uTxPerTrackingWatermark = pwdiSetTxPerTrackingReqParams->wdiTxPerTrackingParam.uTxPerTrackingWatermark;
      
   wpalMemoryCopy( pSendBuffer+usDataOffset, 
                   &halTxPerTrackingReqParam, 
                   sizeof(halTxPerTrackingReqParam)); 

   pWDICtx->wdiReqStatusCB     = pwdiSetTxPerTrackingReqParams->wdiReqStatusCB;
   pWDICtx->pReqStatusUserData = pwdiSetTxPerTrackingReqParams->pUserData; 

   /*-------------------------------------------------------------------------
     Send Get STA Request to HAL 
   -------------------------------------------------------------------------*/
   return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        pwdiSetTxPerTrackingRspCb, pEventData->pUserData, WDI_SET_TX_PER_TRACKING_RESP); 
}/*WDI_ProcessSetTxPerTrackingReq*/

/*=========================================================================
                             Indications
=========================================================================*/

/**
 @brief Process Suspend Indications function (called when Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessHostSuspendInd
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_SuspendParamsType          *pSuspendIndParams;
  wpt_uint8*                     pSendBuffer         = NULL; 
  wpt_uint16                     usDataOffset        = 0;
  wpt_uint16                     usSendSize          = 0;
  WDI_Status                     wdiStatus;
  tHalWlanHostSuspendIndParam    halWlanSuspendIndparams;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
     Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) || ( NULL == pEventData->pEventData ))
  {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
               "%s: Invalid parameters in Suspend ind",__FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
  }

  pSuspendIndParams = (WDI_SuspendParamsType *)pEventData->pEventData;

   /*-----------------------------------------------------------------------
     Get message buffer
   -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, 
                         WDI_HOST_SUSPEND_IND, 
                     sizeof(halWlanSuspendIndparams),
                     &pSendBuffer, &usDataOffset, &usSendSize))||
        (usSendSize < (usDataOffset + sizeof(halWlanSuspendIndparams))))
  {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
                  "Unable to get send buffer in Suspend Ind ");
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
  }

  halWlanSuspendIndparams.configuredMcstBcstFilterSetting =
       pSuspendIndParams->wdiSuspendParams.ucConfiguredMcstBcstFilterSetting;

  halWlanSuspendIndparams.activeSessionCount = 
       WDI_GetActiveSessionsCount(pWDICtx);

  wpalMemoryCopy( pSendBuffer+usDataOffset, &halWlanSuspendIndparams, 
                                         sizeof(tHalWlanHostSuspendIndParam)); 

  /*-------------------------------------------------------------------------
    Send Suspend Request to HAL 
  -------------------------------------------------------------------------*/
  pWDICtx->wdiReqStatusCB     = pSuspendIndParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pSuspendIndParams->pUserData; 

  wdiStatus = WDI_SendIndication( pWDICtx, pSendBuffer, usSendSize); 
  return  ( wdiStatus != WDI_STATUS_SUCCESS )?wdiStatus:WDI_STATUS_SUCCESS_SYNC;
}/*WDI_ProcessHostSuspendInd*/

/*==========================================================================
                  MISC CONTROL PROCESSING REQUEST API 
==========================================================================*/
/**
 @brief Process Channel Switch Request function (called when 
        Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessChannelSwitchReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_SwitchChReqParamsType*   pwdiSwitchChParams;
  WDI_SwitchChRspCb            wdiSwitchChRspCb;
  wpt_uint8*                   pSendBuffer         = NULL; 
  wpt_uint16                   usDataOffset        = 0;
  wpt_uint16                   usSendSize          = 0;
  tSwitchChannelReqMsg         halSwitchChannelReq = {{0}};
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) || ( NULL == pEventData->pEventData ) ||
      ( NULL == pEventData->pCBfnc ))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  pwdiSwitchChParams = (WDI_SwitchChReqParamsType*)pEventData->pEventData;
  wdiSwitchChRspCb   = (WDI_SwitchChRspCb)pEventData->pCBfnc;
  /*-----------------------------------------------------------------------
    Get message buffer
    ! TO DO : proper conversion into the HAL Message Request Format 
  -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_CH_SWITCH_REQ, 
                        sizeof(halSwitchChannelReq.switchChannelParams),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset + sizeof(halSwitchChannelReq.switchChannelParams) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in channel switch req %x %x %x",
                pEventData, pwdiSwitchChParams, wdiSwitchChRspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  halSwitchChannelReq.switchChannelParams.channelNumber = 
                       pwdiSwitchChParams->wdiChInfo.ucChannel;
#ifndef WLAN_FEATURE_VOWIFI    
  halSwitchChannelReq.switchChannelParams.localPowerConstraint = 
                       pwdiSwitchChParams->wdiChInfo.ucLocalPowerConstraint;
#endif
  halSwitchChannelReq.switchChannelParams.secondaryChannelOffset = 
                       pwdiSwitchChParams->wdiChInfo.wdiSecondaryChannelOffset;

#ifdef WLAN_FEATURE_VOWIFI
  halSwitchChannelReq.switchChannelParams.maxTxPower
                            = pwdiSwitchChParams->wdiChInfo.cMaxTxPower; 
  wpalMemoryCopy(halSwitchChannelReq.switchChannelParams.selfStaMacAddr,
                  pwdiSwitchChParams->wdiChInfo.macSelfStaMacAddr,
                  WDI_MAC_ADDR_LEN);
  wpalMemoryCopy(halSwitchChannelReq.switchChannelParams.bssId,
                  pwdiSwitchChParams->wdiChInfo.macBSSId,
                  WDI_MAC_ADDR_LEN);
#endif
  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                  &halSwitchChannelReq.switchChannelParams, 
                  sizeof(halSwitchChannelReq.switchChannelParams)); 

  pWDICtx->wdiReqStatusCB     = pwdiSwitchChParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiSwitchChParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Switch Channel Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiSwitchChRspCb, pEventData->pUserData, WDI_CH_SWITCH_RESP); 
}/*WDI_ProcessChannelSwitchReq*/

/**
 @brief Process Config STA Request function (called when Main FSM 
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessConfigStaReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_ConfigSTAReqParamsType*  pwdiConfigSTAParams;
  WDI_ConfigSTARspCb           wdiConfigSTARspCb;
  wpt_uint8                    ucCurrentBSSSesIdx  = 0; 
  WDI_BSSSessionType*          pBSSSes             = NULL;
  wpt_uint8*                   pSendBuffer         = NULL; 
  wpt_uint16                   usDataOffset        = 0;
  wpt_uint16                   usSendSize          = 0;
  WDI_Status                   wdiStatus           = WDI_STATUS_SUCCESS; 

  tConfigStaReqMsg             halConfigStaReqMsg; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) || ( NULL == pEventData->pEventData ) ||
      ( NULL == pEventData->pCBfnc ))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  pwdiConfigSTAParams = (WDI_ConfigSTAReqParamsType*)pEventData->pEventData;
  wdiConfigSTARspCb   = (WDI_ConfigSTARspCb)pEventData->pCBfnc;
  /*-------------------------------------------------------------------------
    Check to see if we are in the middle of an association, if so queue, if
    not it means it is free to process request 
  -------------------------------------------------------------------------*/
  wpalMutexAcquire(&pWDICtx->wptMutex);

  /*------------------------------------------------------------------------
    Find the BSS for which the request is made 
  ------------------------------------------------------------------------*/
  ucCurrentBSSSesIdx = WDI_FindAssocSession( pWDICtx, 
                           pwdiConfigSTAParams->wdiReqInfo.macBSSID, 
                          &pBSSSes); 

  if ( NULL == pBSSSes ) 
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS does not yet exist");

    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Check if this BSS is being currently processed or queued,
    if queued - queue the new request as well 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_TRUE == pBSSSes->bAssocReqQueued )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS exists but currently queued");

    wdiStatus = WDI_QueueAssocRequest( pWDICtx, pBSSSes, pEventData); 
    wpalMutexRelease(&pWDICtx->wptMutex);
    return wdiStatus; 
  }

  wpalMutexRelease(&pWDICtx->wptMutex);
  /*-----------------------------------------------------------------------
    Get message buffer
  -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_CONFIG_STA_REQ, 
                        sizeof(halConfigStaReqMsg.configStaParams),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset + sizeof(halConfigStaReqMsg.configStaParams) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in config sta req %x %x %x",
                pEventData, pwdiConfigSTAParams, wdiConfigSTARspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  /*Copy the station context*/
  WDI_CopyWDIStaCtxToHALStaCtx( &halConfigStaReqMsg.configStaParams,
                                &pwdiConfigSTAParams->wdiReqInfo);

  if(pwdiConfigSTAParams->wdiReqInfo.wdiSTAType == WDI_STA_ENTRY_SELF)
  {
    /* Need to fill in the self STA Index */
    if ( WDI_STATUS_SUCCESS != 
         WDI_STATableFindStaidByAddr(pWDICtx,
                                     pwdiConfigSTAParams->wdiReqInfo.macSTA,
                                     (wpt_uint8*)&halConfigStaReqMsg.configStaParams.staIdx ))
    {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
               "This station does not exist in the WDI Station Table %d");
      wpalMutexRelease(&pWDICtx->wptMutex);
      return WDI_STATUS_E_FAILURE; 
    }
  }
  else
  {
  /* Need to fill in the STA Index to invalid, since at this point we have not
     yet received it from HAL */
    halConfigStaReqMsg.configStaParams.staIdx = WDI_STA_INVALID_IDX;
  }

  /* Need to fill in the BSS index */
  halConfigStaReqMsg.configStaParams.bssIdx = pBSSSes->ucBSSIdx;

  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                  &halConfigStaReqMsg.configStaParams, 
                  sizeof(halConfigStaReqMsg.configStaParams)); 

  pWDICtx->wdiReqStatusCB     = pwdiConfigSTAParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiConfigSTAParams->pUserData; 

  wpalMemoryCopy( &pWDICtx->wdiCachedConfigStaReq, 
                  pwdiConfigSTAParams, 
                  sizeof(pWDICtx->wdiCachedConfigStaReq));

  /*-------------------------------------------------------------------------
    Send Config STA Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiConfigSTARspCb, pEventData->pUserData, WDI_CONFIG_STA_RESP); 
}/*WDI_ProcessConfigStaReq*/


/**
 @brief Process Set Link State Request function (called when 
        Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetLinkStateReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_SetLinkReqParamsType*    pwdiSetLinkParams;
  WDI_SetLinkStateRspCb        wdiSetLinkRspCb;
  wpt_uint8                    ucCurrentBSSSesIdx  = 0; 
  WDI_BSSSessionType*          pBSSSes             = NULL;
  wpt_uint8*                   pSendBuffer         = NULL; 
  wpt_uint16                   usDataOffset        = 0;
  wpt_uint16                   usSendSize          = 0;
  WDI_Status                   wdiStatus           = WDI_STATUS_SUCCESS;
  tLinkStateParams             halLinkStateReqMsg; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) || ( NULL == pEventData->pEventData ) ||
      ( NULL == pEventData->pCBfnc ))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  pwdiSetLinkParams = (WDI_SetLinkReqParamsType*)pEventData->pEventData;
  wdiSetLinkRspCb   = (WDI_SetLinkStateRspCb)pEventData->pCBfnc;
  /*-------------------------------------------------------------------------
    Check to see if we are in the middle of an association, if so queue, if
    not it means it is free to process request 
  -------------------------------------------------------------------------*/
  wpalMutexAcquire(&pWDICtx->wptMutex);

  /*------------------------------------------------------------------------
    Find the BSS for which the request is made 
  ------------------------------------------------------------------------*/
  ucCurrentBSSSesIdx = WDI_FindAssocSession( pWDICtx, 
                           pwdiSetLinkParams->wdiLinkInfo.macBSSID, 
                          &pBSSSes); 

  if ( NULL == pBSSSes ) 
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
              "Set link request received outside association session");
  }
  else
  {
    /*------------------------------------------------------------------------
      Check if this BSS is being currently processed or queued,
      if queued - queue the new request as well 
    ------------------------------------------------------------------------*/
    if ( eWLAN_PAL_TRUE == pBSSSes->bAssocReqQueued )
    {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                "Association sequence for this BSS exists but currently queued");
  
      wdiStatus = WDI_QueueAssocRequest( pWDICtx, pBSSSes, pEventData); 
      wpalMutexRelease(&pWDICtx->wptMutex);
      return wdiStatus; 
    }
  }
  /* If the link is set to enter IDLE - the Session allocated for this BSS
     will be deleted on the Set Link State response coming from HAL
   - cache the request for response processing */
  wpalMemoryCopy(&pWDICtx->wdiCacheSetLinkStReq, pwdiSetLinkParams, 
                 sizeof(pWDICtx->wdiCacheSetLinkStReq));

  wpalMutexRelease(&pWDICtx->wptMutex);
  /*-----------------------------------------------------------------------
    Get message buffer
    ! TO DO : proper conversion into the HAL Message Request Format 
  -----------------------------------------------------------------------*/
  
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_SET_LINK_ST_REQ, 
                        sizeof(halLinkStateReqMsg),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset + sizeof(halLinkStateReqMsg) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in set bss key req %x %x %x",
                pEventData, pwdiSetLinkParams, wdiSetLinkRspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wpalMemoryCopy(halLinkStateReqMsg.bssid,
                 pwdiSetLinkParams->wdiLinkInfo.macBSSID, WDI_MAC_ADDR_LEN);

  wpalMemoryCopy(halLinkStateReqMsg.selfMacAddr,
                 pwdiSetLinkParams->wdiLinkInfo.macSelfStaMacAddr, WDI_MAC_ADDR_LEN);

  halLinkStateReqMsg.state = 
     WDI_2_HAL_LINK_STATE(pwdiSetLinkParams->wdiLinkInfo.wdiLinkState);

  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                  &halLinkStateReqMsg, 
                  sizeof(halLinkStateReqMsg)); 

  pWDICtx->wdiReqStatusCB     = pwdiSetLinkParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiSetLinkParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Set Link State Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiSetLinkRspCb, pEventData->pUserData, WDI_SET_LINK_ST_RESP); 
}/*WDI_ProcessSetLinkStateReq*/


/**
 @brief Process Get Stats Request function (called when Main FSM
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessGetStatsReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_GetStatsReqParamsType*   pwdiGetStatsParams;
  WDI_GetStatsRspCb            wdiGetStatsRspCb;
  wpt_uint8*                   pSendBuffer         = NULL; 
  wpt_uint16                   usDataOffset        = 0;
  wpt_uint16                   usSendSize          = 0;
  wpt_uint8                    ucCurrentBSSSesIdx  = 0; 
  WDI_BSSSessionType*          pBSSSes             = NULL;
  wpt_macAddr                  macBSSID;
  WDI_Status                   wdiStatus           = WDI_STATUS_SUCCESS; 
  tHalStatsReqMsg              halStatsReqMsg;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) || ( NULL == pEventData->pEventData) ||
      ( NULL == pEventData->pCBfnc ) )
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  pwdiGetStatsParams = (WDI_GetStatsReqParamsType*)pEventData->pEventData;
  wdiGetStatsRspCb   = (WDI_GetStatsRspCb)pEventData->pCBfnc;

  /*-------------------------------------------------------------------------
    Check to see if we are in the middle of an association, if so queue, if
    not it means it is free to process request 
  -------------------------------------------------------------------------*/
  wpalMutexAcquire(&pWDICtx->wptMutex);

  /*------------------------------------------------------------------------
    Find the BSS for which the request is made 
  ------------------------------------------------------------------------*/
  if ( WDI_STATUS_SUCCESS != WDI_STATableGetStaBSSIDAddr(pWDICtx, 
                        pwdiGetStatsParams->wdiGetStatsParamsInfo.ucSTAIdx, 
                        &macBSSID))
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
             "This station does not exist in the WDI Station Table %d");
    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_FAILURE; 
  }

  ucCurrentBSSSesIdx = WDI_FindAssocSession( pWDICtx, macBSSID, &pBSSSes); 
  if ( NULL == pBSSSes ) 
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS does not yet exist");

    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Check if this BSS is being currently processed or queued,
    if queued - queue the new request as well 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_TRUE == pBSSSes->bAssocReqQueued )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS exists but currently queued");

    wdiStatus = WDI_QueueAssocRequest( pWDICtx, pBSSSes, pEventData); 
    wpalMutexRelease(&pWDICtx->wptMutex);
    return wdiStatus; 
  }


  wpalMutexRelease(&pWDICtx->wptMutex);

  /*-----------------------------------------------------------------------
    Get message buffer
  -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_GET_STATS_REQ, 
                        sizeof(halStatsReqMsg.statsReqParams),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset + sizeof(halStatsReqMsg.statsReqParams) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in set bss key req %x %x %x",
                pEventData, pwdiGetStatsParams, wdiGetStatsRspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  halStatsReqMsg.statsReqParams.staId = 
                  pwdiGetStatsParams->wdiGetStatsParamsInfo.ucSTAIdx;
  halStatsReqMsg.statsReqParams.statsMask = 
                  pwdiGetStatsParams->wdiGetStatsParamsInfo.uStatsMask;
  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                  &halStatsReqMsg.statsReqParams, 
                  sizeof(halStatsReqMsg.statsReqParams)); 

  pWDICtx->wdiReqStatusCB     = pwdiGetStatsParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiGetStatsParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Get STA Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiGetStatsRspCb, pEventData->pUserData, WDI_GET_STATS_RESP); 
}/*WDI_ProcessGetStatsReq*/

/**
 @brief Process Update Cfg Request function (called when Main 
        FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessUpdateCfgReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_UpdateCfgReqParamsType*  pwdiUpdateCfgParams = NULL;
  WDI_UpdateCfgRspCb           wdiUpdateCfgRspCb = NULL;

  wpt_uint8*                   pSendBuffer         = NULL; 
  wpt_uint16                   usDataOffset         = 0;
  wpt_uint16                   usSendSize          = 0;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) || ( NULL == pEventData->pEventData ) ||
      ( NULL == pEventData->pCBfnc))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  pwdiUpdateCfgParams = (WDI_UpdateCfgReqParamsType*)pEventData->pEventData;
  wdiUpdateCfgRspCb   = (WDI_UpdateCfgRspCb)pEventData->pCBfnc;

  /*-----------------------------------------------------------------------
    Get message buffer
    ! TO DO : proper conversion into the HAL Message Request Format 
  -----------------------------------------------------------------------*/

  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_UPDATE_CFG_REQ, 
                        pwdiUpdateCfgParams->uConfigBufferLen + sizeof(wpt_uint32),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset +  pwdiUpdateCfgParams->uConfigBufferLen)))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in set bss key req %x %x %x",
                pEventData, pwdiUpdateCfgParams, wdiUpdateCfgRspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                  &pwdiUpdateCfgParams->uConfigBufferLen, 
                  sizeof(wpt_uint32)); 
  wpalMemoryCopy( pSendBuffer+usDataOffset+sizeof(wpt_uint32), 
                  pwdiUpdateCfgParams->pConfigBuffer, 
                  pwdiUpdateCfgParams->uConfigBufferLen); 

  pWDICtx->wdiReqStatusCB     = pwdiUpdateCfgParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiUpdateCfgParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Update Cfg Request to HAL 
  -------------------------------------------------------------------------*/

  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiUpdateCfgRspCb, pEventData->pUserData, WDI_UPDATE_CFG_RESP); 

}/*WDI_ProcessUpdateCfgReq*/


/**
 @brief Process Add BA Request function (called when Main FSM 
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessAddBAReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_AddBAReqParamsType*  pwdiAddBAParams;
  WDI_AddBARspCb           wdiAddBARspCb;
  wpt_uint8                ucCurrentBSSSesIdx  = 0; 
  WDI_BSSSessionType*      pBSSSes             = NULL;
  wpt_uint8*               pSendBuffer         = NULL; 
  wpt_uint16               usDataOffset        = 0;
  wpt_uint16               usSendSize          = 0;
  WDI_Status               wdiStatus           = WDI_STATUS_SUCCESS; 
  wpt_macAddr              macBSSID;

  tAddBAReqMsg             halAddBAReq;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData) ||
      ( NULL == pEventData->pCBfnc ))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  pwdiAddBAParams = (WDI_AddBAReqParamsType*)pEventData->pEventData;
  wdiAddBARspCb   = (WDI_AddBARspCb)pEventData->pCBfnc;

  /*-------------------------------------------------------------------------
    Check to see if we are in the middle of an association, if so queue, if
    not it means it is free to process request 
  -------------------------------------------------------------------------*/
  wpalMutexAcquire(&pWDICtx->wptMutex);

  /*------------------------------------------------------------------------
    Find the BSS for which the request is made 
  ------------------------------------------------------------------------*/
  if ( WDI_STATUS_SUCCESS != WDI_STATableGetStaBSSIDAddr(pWDICtx, 
                                  pwdiAddBAParams->wdiBAInfoType.ucSTAIdx, 
                                  &macBSSID))
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
             "This station does not exist in the WDI Station Table %d");
    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_FAILURE; 
  }

  ucCurrentBSSSesIdx = WDI_FindAssocSession( pWDICtx, macBSSID, &pBSSSes); 
  if ( NULL == pBSSSes ) 
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS does not yet exist");

    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Check if this BSS is being currently processed or queued,
    if queued - queue the new request as well 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_TRUE == pBSSSes->bAssocReqQueued )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS exists but currently queued");

    wdiStatus = WDI_QueueAssocRequest( pWDICtx, pBSSSes, pEventData); 
    wpalMutexRelease(&pWDICtx->wptMutex);
    return wdiStatus; 
  }


  wpalMutexRelease(&pWDICtx->wptMutex);
  /*-----------------------------------------------------------------------
    Get message buffer
  -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_ADD_BA_REQ, 
                        sizeof(halAddBAReq.addBAParams),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < 
            (usDataOffset + sizeof(halAddBAReq.addBAParams) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in Add BA req %x %x %x",
                pEventData, pwdiAddBAParams, wdiAddBARspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  halAddBAReq.addBAParams.baSessionID = 
                             pwdiAddBAParams->wdiBAInfoType.ucBaSessionID;
  halAddBAReq.addBAParams.winSize = pwdiAddBAParams->wdiBAInfoType.ucWinSize;
#ifdef FEATURE_ON_CHIP_REORDERING
  halAddBAReq.addBAParams.isReorderingDoneOnChip = 
                       pwdiAddBAParams->wdiBAInfoType.bIsReorderingDoneOnChip;
#endif

  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                  &halAddBAReq.addBAParams, 
                  sizeof(halAddBAReq.addBAParams)); 

  pWDICtx->wdiReqStatusCB     = pwdiAddBAParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiAddBAParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Start Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        wdiAddBARspCb, pEventData->pUserData, 
                        WDI_ADD_BA_RESP); 
}/*WDI_ProcessAddBAReq*/



/**
 @brief Process Trigger BA Request function (called when Main FSM 
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessTriggerBAReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_TriggerBAReqParamsType*  pwdiTriggerBAParams;
  WDI_TriggerBARspCb           wdiTriggerBARspCb;
  wpt_uint8                    ucCurrentBSSSesIdx  = 0; 
  WDI_BSSSessionType*          pBSSSes             = NULL;
  wpt_uint8*                   pSendBuffer         = NULL; 
  wpt_uint16                   usDataOffset        = 0;
  wpt_uint16                   usSendSize          = 0;
  WDI_Status                   wdiStatus           = WDI_STATUS_SUCCESS; 
  wpt_uint16                   index;
  wpt_macAddr                  macBSSID;
  
  tTriggerBAReqMsg               halTriggerBAReq;
  tTriggerBaReqCandidate*        halTriggerBACandidate;
  WDI_TriggerBAReqCandidateType* wdiTriggerBACandidate;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData ) ||
      ( NULL == pEventData->pCBfnc ))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  pwdiTriggerBAParams = (WDI_TriggerBAReqParamsType*)pEventData->pEventData;
  wdiTriggerBARspCb = (WDI_TriggerBARspCb)pEventData->pCBfnc;
  /*-------------------------------------------------------------------------
    Check to see if we are in the middle of an association, if so queue, if
    not it means it is free to process request 
  -------------------------------------------------------------------------*/
  wpalMutexAcquire(&pWDICtx->wptMutex);

  /*------------------------------------------------------------------------
    Find the BSS for which the request is made 
  ------------------------------------------------------------------------*/
  if ( WDI_STATUS_SUCCESS != WDI_STATableGetStaBSSIDAddr(pWDICtx, 
                                  pwdiTriggerBAParams->wdiTriggerBAInfoType.ucSTAIdx, 
                                  &macBSSID))
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
             "This station does not exist in the WDI Station Table %d");
    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_FAILURE; 
  }

  ucCurrentBSSSesIdx = WDI_FindAssocSession( pWDICtx, macBSSID, &pBSSSes); 
  if ( NULL == pBSSSes ) 
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS does not yet exist");

    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*------------------------------------------------------------------------
    Check if this BSS is being currently processed or queued,
    if queued - queue the new request as well 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_TRUE == pBSSSes->bAssocReqQueued )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS exists but currently queued");

    wdiStatus = WDI_QueueAssocRequest( pWDICtx, pBSSSes, pEventData); 
    wpalMutexRelease(&pWDICtx->wptMutex);
    return wdiStatus; 
  }


  wpalMutexRelease(&pWDICtx->wptMutex);
  /*-----------------------------------------------------------------------
    Get message buffer
  -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, 
                  WDI_TRIGGER_BA_REQ, 
                  sizeof(halTriggerBAReq.triggerBAParams) +
                  (sizeof(tTriggerBaReqCandidate) * 
                  pwdiTriggerBAParams->wdiTriggerBAInfoType.usBACandidateCnt),
                  &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < 
            (usDataOffset + sizeof(halTriggerBAReq.triggerBAParams)+
               (sizeof(tTriggerBaReqCandidate) * 
               pwdiTriggerBAParams->wdiTriggerBAInfoType.usBACandidateCnt) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in Trigger BA req %x %x %x",
                pEventData, pwdiTriggerBAParams, wdiTriggerBARspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  halTriggerBAReq.triggerBAParams.baSessionID = 
                  pwdiTriggerBAParams->wdiTriggerBAInfoType.ucBASessionID;
  halTriggerBAReq.triggerBAParams.baCandidateCnt = 
                  pwdiTriggerBAParams->wdiTriggerBAInfoType.usBACandidateCnt;

  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                  &halTriggerBAReq.triggerBAParams, 
                  sizeof(halTriggerBAReq.triggerBAParams)); 

  wdiTriggerBACandidate = 
    (WDI_TriggerBAReqCandidateType*)(pwdiTriggerBAParams + 1);
  halTriggerBACandidate = (tTriggerBaReqCandidate*)(pSendBuffer+usDataOffset+
                                 sizeof(halTriggerBAReq.triggerBAParams));
  
  for(index = 0 ; index < halTriggerBAReq.triggerBAParams.baCandidateCnt ; 
                                                                     index++)
  {
    halTriggerBACandidate->staIdx = wdiTriggerBACandidate->ucSTAIdx;
    halTriggerBACandidate->tidBitmap = wdiTriggerBACandidate->ucTidBitmap;
    halTriggerBACandidate++;
    wdiTriggerBACandidate++;
  }

  pWDICtx->wdiReqStatusCB     = pwdiTriggerBAParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiTriggerBAParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Start Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        wdiTriggerBARspCb, pEventData->pUserData, 
                        WDI_TRIGGER_BA_RESP); 
}/*WDI_ProcessTriggerBAReq*/



/**
 @brief Process Update Beacon Params  Request function (called when Main FSM
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessUpdateBeaconParamsReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_UpdateBeaconParamsType*  pwdiUpdateBeaconParams;
  WDI_UpdateBeaconParamsRspCb  wdiUpdateBeaconParamsRspCb;
  wpt_uint8*                   pSendBuffer         = NULL; 
  wpt_uint16                   usDataOffset        = 0;
  wpt_uint16                   usSendSize          = 0;
  tUpdateBeaconParams          halUpdateBeaconParams; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData) ||
      ( NULL == pEventData->pCBfnc))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  pwdiUpdateBeaconParams = (WDI_UpdateBeaconParamsType*)pEventData->pEventData;
  wdiUpdateBeaconParamsRspCb = (WDI_UpdateBeaconParamsRspCb)pEventData->pCBfnc;
  /*-----------------------------------------------------------------------
    Get message buffer
  -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_UPD_BCON_PRMS_REQ, 
                        sizeof(halUpdateBeaconParams),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset + sizeof(halUpdateBeaconParams) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in set bss key req %x %x %x",
                pEventData, pwdiUpdateBeaconParams, wdiUpdateBeaconParamsRspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  /*BSS Index of the BSS*/
  halUpdateBeaconParams.bssIdx =
    pwdiUpdateBeaconParams->wdiUpdateBeaconParamsInfo.ucBssIdx;
  /*shortPreamble mode. HAL should update all the STA rates when it
    receives this message*/
  halUpdateBeaconParams.fShortPreamble = 
    pwdiUpdateBeaconParams->wdiUpdateBeaconParamsInfo.ucfShortPreamble;
  /* short Slot time.*/
  halUpdateBeaconParams.fShortSlotTime = 
    pwdiUpdateBeaconParams->wdiUpdateBeaconParamsInfo.ucfShortSlotTime;
  /* Beacon Interval */
  halUpdateBeaconParams.beaconInterval = 
    pwdiUpdateBeaconParams->wdiUpdateBeaconParamsInfo.usBeaconInterval;

  /*Protection related */
  halUpdateBeaconParams.llaCoexist = 
    pwdiUpdateBeaconParams->wdiUpdateBeaconParamsInfo.ucllaCoexist;
  halUpdateBeaconParams.llbCoexist = 
    pwdiUpdateBeaconParams->wdiUpdateBeaconParamsInfo.ucllbCoexist;
  halUpdateBeaconParams.llgCoexist = 
    pwdiUpdateBeaconParams->wdiUpdateBeaconParamsInfo.ucllgCoexist;
  halUpdateBeaconParams.ht20MhzCoexist  = 
    pwdiUpdateBeaconParams->wdiUpdateBeaconParamsInfo.ucHt20MhzCoexist;
  halUpdateBeaconParams.llnNonGFCoexist = 
    pwdiUpdateBeaconParams->wdiUpdateBeaconParamsInfo.ucllnNonGFCoexist;
  halUpdateBeaconParams.fLsigTXOPProtectionFullSupport = 
    pwdiUpdateBeaconParams->wdiUpdateBeaconParamsInfo.ucfLsigTXOPProtectionFullSupport;
  halUpdateBeaconParams.fRIFSMode =
    pwdiUpdateBeaconParams->wdiUpdateBeaconParamsInfo.ucfRIFSMode;
  halUpdateBeaconParams.paramChangeBitmap = 
    pwdiUpdateBeaconParams->wdiUpdateBeaconParamsInfo.usChangeBitmap;

  wpalMemoryCopy( pSendBuffer+usDataOffset, &halUpdateBeaconParams, 
                  sizeof(halUpdateBeaconParams)); 

  pWDICtx->wdiReqStatusCB     = pwdiUpdateBeaconParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiUpdateBeaconParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Del TS Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiUpdateBeaconParamsRspCb, pEventData->pUserData, WDI_UPD_BCON_PRMS_RESP); 
}/*WDI_ProcessUpdateBeaconParamsReq*/



/**
 @brief Process Send Beacon template  Request function (called when Main FSM
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSendBeaconParamsReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_SendBeaconParamsType*    pwdiSendBeaconParams;
  WDI_SendBeaconParamsRspCb    wdiSendBeaconParamsRspCb;
  wpt_uint8*                   pSendBuffer         = NULL; 
  wpt_uint16                   usDataOffset        = 0;
  wpt_uint16                   usSendSize          = 0;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  tSendBeaconReqMsg            halSendBeaconReq;
  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData ) ||
      ( NULL == pEventData->pCBfnc ))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  pwdiSendBeaconParams = (WDI_SendBeaconParamsType*)pEventData->pEventData;
  wdiSendBeaconParamsRspCb   = (WDI_SendBeaconParamsRspCb)pEventData->pCBfnc;
  /*-----------------------------------------------------------------------
    Get message buffer
  -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_SND_BCON_REQ, 
                        sizeof(halSendBeaconReq.sendBeaconParam),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset + sizeof(halSendBeaconReq.sendBeaconParam) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in send beacon req %x %x %x",
                pEventData, pwdiSendBeaconParams, wdiSendBeaconParamsRspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wpalMemoryCopy(halSendBeaconReq.sendBeaconParam.bssId,
                  pwdiSendBeaconParams->wdiSendBeaconParamsInfo.macBSSID,
                  WDI_MAC_ADDR_LEN);
  halSendBeaconReq.sendBeaconParam.beaconLength = 
                  pwdiSendBeaconParams->wdiSendBeaconParamsInfo.beaconLength;
  wpalMemoryCopy(halSendBeaconReq.sendBeaconParam.beacon,
                  pwdiSendBeaconParams->wdiSendBeaconParamsInfo.beacon,
                  pwdiSendBeaconParams->wdiSendBeaconParamsInfo.beaconLength);
#ifdef WLAN_SOFTAP_FEATURE
  halSendBeaconReq.sendBeaconParam.timIeOffset = 
                  pwdiSendBeaconParams->wdiSendBeaconParamsInfo.timIeOffset;
#endif
#ifdef WLAN_FEATURE_P2P
  halSendBeaconReq.sendBeaconParam.p2pIeOffset = 
                  pwdiSendBeaconParams->wdiSendBeaconParamsInfo.usP2PIeOffset;
#endif

  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                  &halSendBeaconReq.sendBeaconParam, 
                  sizeof(halSendBeaconReq.sendBeaconParam)); 

  pWDICtx->wdiReqStatusCB     = pwdiSendBeaconParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiSendBeaconParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Del TS Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiSendBeaconParamsRspCb, pEventData->pUserData, WDI_SND_BCON_RESP); 
}/*WDI_ProcessSendBeaconParamsReq*/

/**
 @brief Process Update Beacon Params  Request function (called when Main FSM
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessUpdateProbeRspTemplateReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_UpdateProbeRspTemplateParamsType*  pwdiUpdateProbeRespTmplParams;
  WDI_UpdateProbeRspTemplateRspCb        wdiUpdateProbeRespTmplRspCb;
  wpt_uint8*                             pSendBuffer         = NULL; 
  wpt_uint16                             usDataOffset        = 0;
  wpt_uint16                             usSendSize          = 0;
  tSendProbeRespReqParams                halUpdateProbeRspTmplParams; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData) ||
      ( NULL == pEventData->pCBfnc))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  pwdiUpdateProbeRespTmplParams = 
    (WDI_UpdateProbeRspTemplateParamsType*)pEventData->pEventData;
  wdiUpdateProbeRespTmplRspCb = 
    (WDI_UpdateProbeRspTemplateRspCb)pEventData->pCBfnc;
  /*-----------------------------------------------------------------------
    Get message buffer
  -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_UPD_PROBE_RSP_TEMPLATE_REQ, 
                        sizeof(halUpdateProbeRspTmplParams),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset + sizeof(halUpdateProbeRspTmplParams) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in set bss key req %x %x %x",
     pEventData, pwdiUpdateProbeRespTmplParams, wdiUpdateProbeRespTmplRspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wpalMemoryCopy(halUpdateProbeRspTmplParams.bssId,
                 pwdiUpdateProbeRespTmplParams->wdiProbeRspTemplateInfo.macBSSID, 
                 WDI_MAC_ADDR_LEN);

  halUpdateProbeRspTmplParams.probeRespTemplateLen = 
    pwdiUpdateProbeRespTmplParams->wdiProbeRspTemplateInfo.uProbeRespTemplateLen;

  wpalMemoryCopy(halUpdateProbeRspTmplParams.pProbeRespTemplate,
    pwdiUpdateProbeRespTmplParams->wdiProbeRspTemplateInfo.pProbeRespTemplate,
                 BEACON_TEMPLATE_SIZE);     


  wpalMemoryCopy(halUpdateProbeRspTmplParams.ucProxyProbeReqValidIEBmap,
           pwdiUpdateProbeRespTmplParams->wdiProbeRspTemplateInfo.uaProxyProbeReqValidIEBmap,
                 WDI_PROBE_REQ_BITMAP_IE_LEN);

  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                  &halUpdateProbeRspTmplParams, 
                  sizeof(halUpdateProbeRspTmplParams)); 

  pWDICtx->wdiReqStatusCB     = pwdiUpdateProbeRespTmplParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiUpdateProbeRespTmplParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Update Probe Resp Template Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiUpdateProbeRespTmplRspCb, pEventData->pUserData, 
                       WDI_UPD_PROBE_RSP_TEMPLATE_RESP); 
}/*WDI_ProcessUpdateProbeRspTemplateReq*/

/**
 @brief Process NV blob download function (called when Main FSM 
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessNvDownloadReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{

  WDI_NvDownloadReqParamsType*  pwdiNvDownloadReqParams = NULL;
  WDI_NvDownloadRspCb      wdiNvDownloadRspCb = NULL;
  
  /*-------------------------------------------------------------------------
     Sanity check       
   -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) ||
      ( NULL == (pwdiNvDownloadReqParams = 
                 (WDI_NvDownloadReqParamsType*)pEventData->pEventData)) ||
      ( NULL == (wdiNvDownloadRspCb = 
                (WDI_NvDownloadRspCb)pEventData->pCBfnc)))
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                "%s: Invalid parameters", __FUNCTION__);
    WDI_ASSERT(0);
    return WDI_STATUS_E_FAILURE; 
  }

  /*Intialize the Nv Blob Info */
  pWDICtx->wdiNvBlobInfo.usTotalFragment = 
                TOTALFRAGMENTS(pwdiNvDownloadReqParams->wdiBlobInfo.uBlobSize);
  /*cache the wdi nv request message here if the the first fragment
   * To issue the request to HAL for the next fragment */
  if( 0 == pWDICtx->wdiNvBlobInfo.usCurrentFragment)
  {
    wpalMemoryCopy(&pWDICtx->wdiCachedNvDownloadReq, 
                 pwdiNvDownloadReqParams, 
                 sizeof(pWDICtx->wdiCachedNvDownloadReq));
  }

  return WDI_SendNvBlobReq(pWDICtx,pEventData);
}

/**
 @brief Process Set Max Tx Power Request function (called when Main    
        FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status WDI_ProcessSetMaxTxPowerReq
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_SetMaxTxPowerParamsType*      pwdiSetMaxTxPowerParams;
  WDA_SetMaxTxPowerRspCb            wdiSetMaxTxPowerRspCb;
  wpt_uint8*                        pSendBuffer         = NULL; 
  wpt_uint16                        usDataOffset        = 0;
  wpt_uint16                        usSendSize          = 0;
  tSetMaxTxPwrReq                   halSetMaxTxPower;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData ) ||
      ( NULL == pEventData->pCBfnc ))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  pwdiSetMaxTxPowerParams = 
    (WDI_SetMaxTxPowerParamsType*)pEventData->pEventData;
  wdiSetMaxTxPowerRspCb = 
    (WDA_SetMaxTxPowerRspCb)pEventData->pCBfnc;

  /*-----------------------------------------------------------------------
    Get message buffer
  -----------------------------------------------------------------------*/
if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_SET_MAX_TX_POWER_REQ, 
                        sizeof(halSetMaxTxPower.setMaxTxPwrParams),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset + sizeof(halSetMaxTxPower.setMaxTxPwrParams) 
)))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get Set Max Tx Power req %x %x %x",
                pEventData, pwdiSetMaxTxPowerParams, wdiSetMaxTxPowerRspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }


  wpalMemoryCopy(halSetMaxTxPower.setMaxTxPwrParams.bssId,
                  pwdiSetMaxTxPowerParams->wdiMaxTxPowerInfo.macBSSId,
                  WDI_MAC_ADDR_LEN);

  wpalMemoryCopy(halSetMaxTxPower.setMaxTxPwrParams.selfStaMacAddr,
                  pwdiSetMaxTxPowerParams->wdiMaxTxPowerInfo.macSelfStaMacAddr,
                  WDI_MAC_ADDR_LEN);
  halSetMaxTxPower.setMaxTxPwrParams.power = 
                  pwdiSetMaxTxPowerParams->wdiMaxTxPowerInfo.ucPower;
  
  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                  &halSetMaxTxPower.setMaxTxPwrParams, 
                  sizeof(halSetMaxTxPower.setMaxTxPwrParams)); 

  pWDICtx->wdiReqStatusCB     = pwdiSetMaxTxPowerParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiSetMaxTxPowerParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Del TS Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiSetMaxTxPowerRspCb, pEventData->pUserData, 
                                                      WDI_SET_MAX_TX_POWER_RESP); 
  
}

#ifdef WLAN_FEATURE_P2P

/**
 @brief Process P2P Notice Of Absence Request function (called when Main FSM
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessP2PGONOAReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_SetP2PGONOAReqParamsType*          pwdiP2PGONOAReqParams;
  WDI_SetP2PGONOAReqParamsRspCb          wdiP2PGONOAReqRspCb;
  wpt_uint8*                             pSendBuffer         = NULL; 
  wpt_uint16                             usDataOffset        = 0;
  wpt_uint16                             usSendSize          = 0;
  tSetP2PGONOAParams                     halSetP2PGONOAParams; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData) ||
      ( NULL == pEventData->pCBfnc))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  pwdiP2PGONOAReqParams = 
    (WDI_SetP2PGONOAReqParamsType*)pEventData->pEventData;
  wdiP2PGONOAReqRspCb = 
    (WDI_SetP2PGONOAReqParamsRspCb)pEventData->pCBfnc;
  /*-----------------------------------------------------------------------
    Get message buffer
  -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, 
                        WDI_P2P_GO_NOTICE_OF_ABSENCE_REQ, 
                        sizeof(halSetP2PGONOAParams),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset + sizeof(halSetP2PGONOAParams) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in set P2P GO NOA REQ %x %x %x",
     pEventData, pwdiP2PGONOAReqParams, wdiP2PGONOAReqRspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  halSetP2PGONOAParams.opp_ps = 
                           pwdiP2PGONOAReqParams->wdiP2PGONOAInfo.ucOpp_ps;
  halSetP2PGONOAParams.ctWindow = 
                           pwdiP2PGONOAReqParams->wdiP2PGONOAInfo.uCtWindow;
  halSetP2PGONOAParams.count = pwdiP2PGONOAReqParams->wdiP2PGONOAInfo.ucCount;
  halSetP2PGONOAParams.duration = 
                           pwdiP2PGONOAReqParams->wdiP2PGONOAInfo.uDuration;
  halSetP2PGONOAParams.interval = 
                           pwdiP2PGONOAReqParams->wdiP2PGONOAInfo.uInterval;
  halSetP2PGONOAParams.single_noa_duration = 
                 pwdiP2PGONOAReqParams->wdiP2PGONOAInfo.uSingle_noa_duration;
  halSetP2PGONOAParams.psSelection = 
                   pwdiP2PGONOAReqParams->wdiP2PGONOAInfo.ucPsSelection;

  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                  &halSetP2PGONOAParams, 
                  sizeof(halSetP2PGONOAParams)); 

  pWDICtx->wdiReqStatusCB     = pwdiP2PGONOAReqParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiP2PGONOAReqParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Update Probe Resp Template Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiP2PGONOAReqRspCb, pEventData->pUserData, 
                       WDI_P2P_GO_NOTICE_OF_ABSENCE_RESP); 
}/*WDI_ProcessP2PGONOAReq*/

#endif


/**
 @brief    Function to handle the ack from DXE once the power 
           state is set.
 @param    None 
    
 @see 
 @return void 
*/
void
WDI_SetPowerStateCb
(
   wpt_status status,
   unsigned int dxePhyAddr,
   void      *pContext
)
{
   wpt_status              wptStatus;
   WDI_ControlBlockType *pCB = NULL;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
   if(eWLAN_PAL_STATUS_E_FAILURE == status )
   {
      //it shouldn't happen, put an error msg
   }
   /* 
    * Trigger the event to bring the Enter BMPS req function to come 
    * out of wait 
*/
   if( NULL != pContext )
   {
      pCB = (WDI_ControlBlockType *)pContext; 
   }
   else
   {
      //put an error msg 
      pCB = &gWDICb;
   }
   pCB->dxePhyAddr = dxePhyAddr;
   wptStatus  = wpalEventSet(&pCB->setPowerStateEvent);
   if ( eWLAN_PAL_STATUS_SUCCESS !=  wptStatus )
   {
      WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                "Failed to set an event");

      WDI_ASSERT(0); 
   }
   return;
}


/**
 @brief Process Enter IMPS Request function (called when 
        Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessEnterImpsReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   wpt_status               wptStatus; 
   WDI_EnterImpsRspCb       wdiEnterImpsRspCb = NULL;
   wpt_uint8*               pSendBuffer         = NULL; 
   wpt_uint16               usDataOffset        = 0;
   wpt_uint16               usSendSize          = 0;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pEventData ) ||
       ( NULL == (wdiEnterImpsRspCb   = (WDI_EnterImpsRspCb)pEventData->pCBfnc)))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   /*-----------------------------------------------------------------------
     Get message buffer
     ! TO DO : proper conversion into the HAL Message Request Format 
   -----------------------------------------------------------------------*/
   if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_ENTER_IMPS_REQ, 
                                                     0,
                                                     &pSendBuffer, &usDataOffset, &usSendSize))||
       ( usSendSize < (usDataOffset )))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
               "Unable to get send buffer in Enter IMPS req %x %x",
                 pEventData, wdiEnterImpsRspCb);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   /* Reset the event to be not signalled */
   wptStatus = wpalEventReset(&pWDICtx->setPowerStateEvent);
   if ( eWLAN_PAL_STATUS_SUCCESS != wptStatus ) 
   {
      WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                "WDI Init failed to reset an event");

      WDI_ASSERT(0); 
      return VOS_STATUS_E_FAILURE;
   }

   // notify DTS that we are entering IMPS
   WDTS_SetPowerState(pWDICtx, WDTS_POWER_STATE_IMPS, WDI_SetPowerStateCb);

   /*
    * Wait for the event to be set once the ACK comes back from DXE 
    */
   wptStatus = wpalEventWait(&pWDICtx->setPowerStateEvent, 
                             WDI_SET_POWER_STATE_TIMEOUT);
   if ( eWLAN_PAL_STATUS_SUCCESS != wptStatus ) 
   {
      WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                "WDI Init failed to wait on an event");

      WDI_ASSERT(0); 
      return VOS_STATUS_E_FAILURE;
   }

   /*-------------------------------------------------------------------------
     Send Get STA Request to HAL 
   -------------------------------------------------------------------------*/
   return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        wdiEnterImpsRspCb, pEventData->pUserData, WDI_ENTER_IMPS_RESP); 
}/*WDI_ProcessEnterImpsReq*/

/**
 @brief Process Exit IMPS Request function (called when 
        Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessExitImpsReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_ExitImpsRspCb        wdiExitImpsRspCb = NULL;
   wpt_uint8*               pSendBuffer         = NULL; 
   wpt_uint16               usDataOffset        = 0;
   wpt_uint16               usSendSize          = 0;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pEventData ) ||
       ( NULL == (wdiExitImpsRspCb   = (WDI_ExitImpsRspCb)pEventData->pCBfnc)))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   /*-----------------------------------------------------------------------
     Get message buffer
     ! TO DO : proper conversion into the HAL Message Request Format 
   -----------------------------------------------------------------------*/
   if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_EXIT_IMPS_REQ, 
                                                     0,
                                                     &pSendBuffer, &usDataOffset, &usSendSize))||
       ( usSendSize < (usDataOffset )))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
               "Unable to get send buffer in Exit IMPS req %x %x",
                 pEventData, wdiExitImpsRspCb);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   /*-------------------------------------------------------------------------
     Send Get STA Request to HAL 
   -------------------------------------------------------------------------*/
   return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        wdiExitImpsRspCb, pEventData->pUserData, WDI_EXIT_IMPS_RESP); 
}/*WDI_ProcessExitImpsReq*/

/**
 @brief Process Enter BMPS Request function (called when Main 
        FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessEnterBmpsReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_EnterBmpsReqParamsType*  pwdiEnterBmpsReqParams = NULL;
   WDI_EnterBmpsRspCb           wdiEnterBmpsRspCb = NULL;
   wpt_uint8*               pSendBuffer         = NULL; 
   wpt_uint16               usDataOffset        = 0;
   wpt_uint16               usSendSize          = 0;
   tHalEnterBmpsReqParams   enterBmpsReq;
   wpt_status               wptStatus; 

   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
     Sanity check 
  -------------------------------------------------------------------------*/
   if (( NULL == pEventData ) ||
       ( NULL == (pwdiEnterBmpsReqParams = (WDI_EnterBmpsReqParamsType*)pEventData->pEventData)) ||
       ( NULL == (wdiEnterBmpsRspCb   = (WDI_EnterBmpsRspCb)pEventData->pCBfnc)))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   /*-----------------------------------------------------------------------
     Get message buffer
     ! TO DO : proper conversion into the HAL Message Request Format 
   -----------------------------------------------------------------------*/
   if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_ENTER_BMPS_REQ, 
                         sizeof(enterBmpsReq),
                         &pSendBuffer, &usDataOffset, &usSendSize))||
       ( usSendSize < (usDataOffset + sizeof(enterBmpsReq) )))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
               "Unable to get send buffer in Enter BMPS req %x %x %x",
                 pEventData, pwdiEnterBmpsReqParams, wdiEnterBmpsRspCb);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   /* Reset the event to be not signalled */
   wptStatus = wpalEventReset(&pWDICtx->setPowerStateEvent);
   if ( eWLAN_PAL_STATUS_SUCCESS != wptStatus )
   {
      WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                "WDI Init failed to reset an event");

      WDI_ASSERT(0); 
      return VOS_STATUS_E_FAILURE;
   }

   // notify DTS that we are entering BMPS
   WDTS_SetPowerState(pWDICtx, WDTS_POWER_STATE_BMPS, WDI_SetPowerStateCb);

/*
    * Wait for the event to be set once the ACK comes back from DXE 
    */
   wptStatus = wpalEventWait(&pWDICtx->setPowerStateEvent, 
                             WDI_SET_POWER_STATE_TIMEOUT);
   if ( eWLAN_PAL_STATUS_SUCCESS != wptStatus )
   {
      WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                "WDI Init failed to wait on an event");

      WDI_ASSERT(0); 
      return VOS_STATUS_E_FAILURE;
   }

   pWDICtx->bInBmps = eWLAN_PAL_TRUE;

   enterBmpsReq.bssIdx = pwdiEnterBmpsReqParams->wdiEnterBmpsInfo.ucBssIdx;
   enterBmpsReq.tbtt = pwdiEnterBmpsReqParams->wdiEnterBmpsInfo.uTbtt;
   enterBmpsReq.dtimCount = pwdiEnterBmpsReqParams->wdiEnterBmpsInfo.ucDtimCount;
   enterBmpsReq.dtimPeriod = pwdiEnterBmpsReqParams->wdiEnterBmpsInfo.ucDtimPeriod;

   wpalMemoryCopy( pSendBuffer+usDataOffset, 
                   &enterBmpsReq, 
                   sizeof(enterBmpsReq)); 

   pWDICtx->wdiReqStatusCB     = pwdiEnterBmpsReqParams->wdiReqStatusCB;
   pWDICtx->pReqStatusUserData = pwdiEnterBmpsReqParams->pUserData; 

   /*-------------------------------------------------------------------------
     Send Get STA Request to HAL 
   -------------------------------------------------------------------------*/
   return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        wdiEnterBmpsRspCb, pEventData->pUserData, WDI_ENTER_BMPS_RESP); 
}/*WDI_ProcessEnterBmpsReq*/

/**
 @brief Process Exit BMPS Request function (called when Main FSM
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessExitBmpsReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_ExitBmpsReqParamsType*  pwdiExitBmpsReqParams = NULL;
   WDI_ExitBmpsRspCb           wdiExitBmpsRspCb = NULL;
   wpt_uint8*               pSendBuffer         = NULL; 
   wpt_uint16               usDataOffset        = 0;
   wpt_uint16               usSendSize          = 0;
   tHalExitBmpsReqParams    exitBmpsReq;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pEventData ) ||
       ( NULL == (pwdiExitBmpsReqParams = (WDI_ExitBmpsReqParamsType*)pEventData->pEventData)) ||
       ( NULL == (wdiExitBmpsRspCb   = (WDI_ExitBmpsRspCb)pEventData->pCBfnc)))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   /*-----------------------------------------------------------------------
     Get message buffer
     ! TO DO : proper conversion into the HAL Message Request Format 
   -----------------------------------------------------------------------*/
   if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_EXIT_BMPS_REQ, 
                         sizeof(exitBmpsReq),
                         &pSendBuffer, &usDataOffset, &usSendSize))||
       ( usSendSize < (usDataOffset + sizeof(exitBmpsReq) )))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
               "Unable to get send buffer in Exit BMPS req %x %x %x",
                 pEventData, pwdiExitBmpsReqParams, wdiExitBmpsRspCb);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }
   exitBmpsReq.sendDataNull = pwdiExitBmpsReqParams->wdiExitBmpsInfo.ucSendDataNull;

   wpalMemoryCopy( pSendBuffer+usDataOffset, 
                   &exitBmpsReq, 
                   sizeof(exitBmpsReq)); 

   pWDICtx->wdiReqStatusCB     = pwdiExitBmpsReqParams->wdiReqStatusCB;
   pWDICtx->pReqStatusUserData = pwdiExitBmpsReqParams->pUserData; 

   /*-------------------------------------------------------------------------
     Send Get STA Request to HAL 
   -------------------------------------------------------------------------*/
   return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        wdiExitBmpsRspCb, pEventData->pUserData, WDI_EXIT_BMPS_RESP); 
}/*WDI_ProcessExitBmpsReq*/

/**
 @brief Process Enter UAPSD Request function (called when Main 
        FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessEnterUapsdReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_EnterUapsdReqParamsType*  pwdiEnterUapsdReqParams = NULL;
   WDI_EnterUapsdRspCb           wdiEnterUapsdRspCb = NULL;
   wpt_uint8*               pSendBuffer         = NULL; 
   wpt_uint16               usDataOffset        = 0;
   wpt_uint16               usSendSize          = 0;
   tUapsdReqParams          enterUapsdReq;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pEventData ) ||
       ( NULL == (pwdiEnterUapsdReqParams = (WDI_EnterUapsdReqParamsType*)pEventData->pEventData)) ||
       ( NULL == (wdiEnterUapsdRspCb   = (WDI_EnterUapsdRspCb)pEventData->pCBfnc)))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   /*-----------------------------------------------------------------------
     Get message buffer
     ! TO DO : proper conversion into the HAL Message Request Format 
   -----------------------------------------------------------------------*/
   if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_ENTER_UAPSD_REQ, 
                         sizeof(enterUapsdReq),
                         &pSendBuffer, &usDataOffset, &usSendSize))||
       ( usSendSize < (usDataOffset + sizeof(enterUapsdReq) )))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
               "Unable to get send buffer in Enter UAPSD req %x %x %x",
                 pEventData, pwdiEnterUapsdReqParams, wdiEnterUapsdRspCb);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   enterUapsdReq.beDeliveryEnabled  = pwdiEnterUapsdReqParams->wdiEnterUapsdInfo.ucBeDeliveryEnabled;
   enterUapsdReq.beTriggerEnabled   = pwdiEnterUapsdReqParams->wdiEnterUapsdInfo.ucBeTriggerEnabled;
   enterUapsdReq.bkDeliveryEnabled  = pwdiEnterUapsdReqParams->wdiEnterUapsdInfo.ucBkDeliveryEnabled;
   enterUapsdReq.bkTriggerEnabled   = pwdiEnterUapsdReqParams->wdiEnterUapsdInfo.ucBkTriggerEnabled;
   enterUapsdReq.viDeliveryEnabled  = pwdiEnterUapsdReqParams->wdiEnterUapsdInfo.ucViDeliveryEnabled;
   enterUapsdReq.viTriggerEnabled   = pwdiEnterUapsdReqParams->wdiEnterUapsdInfo.ucViTriggerEnabled;
   enterUapsdReq.voDeliveryEnabled  = pwdiEnterUapsdReqParams->wdiEnterUapsdInfo.ucVoDeliveryEnabled;
   enterUapsdReq.voTriggerEnabled   = pwdiEnterUapsdReqParams->wdiEnterUapsdInfo.ucVoTriggerEnabled;

   wpalMemoryCopy( pSendBuffer+usDataOffset, 
                   &enterUapsdReq, 
                   sizeof(enterUapsdReq)); 

   pWDICtx->wdiReqStatusCB     = pwdiEnterUapsdReqParams->wdiReqStatusCB;
   pWDICtx->pReqStatusUserData = pwdiEnterUapsdReqParams->pUserData; 

   /*-------------------------------------------------------------------------
     Send Get STA Request to HAL 
   -------------------------------------------------------------------------*/
   return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        wdiEnterUapsdRspCb, pEventData->pUserData, WDI_ENTER_UAPSD_RESP); 
}/*WDI_ProcessEnterUapsdReq*/

/**
 @brief Process Exit UAPSD Request function (called when 
        Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessExitUapsdReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_ExitUapsdRspCb       wdiExitUapsdRspCb = NULL;
   wpt_uint8*               pSendBuffer         = NULL; 
   wpt_uint16               usDataOffset        = 0;
   wpt_uint16               usSendSize          = 0;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pEventData ) ||
       ( NULL == (wdiExitUapsdRspCb   = (WDI_ExitUapsdRspCb)pEventData->pCBfnc)))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   /*-----------------------------------------------------------------------
     Get message buffer
     ! TO DO : proper conversion into the HAL Message Request Format 
   -----------------------------------------------------------------------*/
   if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_EXIT_UAPSD_REQ, 
                                                     0,
                                                     &pSendBuffer, &usDataOffset, &usSendSize))||
       ( usSendSize < (usDataOffset )))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
               "Unable to get send buffer in Exit UAPSD req %x %x",
                 pEventData, wdiExitUapsdRspCb);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   /*-------------------------------------------------------------------------
     Send Get STA Request to HAL 
   -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        wdiExitUapsdRspCb, pEventData->pUserData, WDI_EXIT_UAPSD_RESP); 
}/*WDI_ProcessExitUapsdReq*/

/**
 @brief Process Set UAPSD params Request function (called when 
        Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetUapsdAcParamsReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_SetUapsdAcParamsReqParamsType*  pwdiSetUapsdAcParams = NULL;
  WDI_SetUapsdAcParamsCb              wdiSetUapsdAcParamsCb = NULL;
  wpt_uint8*               pSendBuffer         = NULL; 
  wpt_uint16               usDataOffset        = 0;
  wpt_uint16               usSendSize          = 0;
  tUapsdInfo               uapsdAcParamsReq;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) ||
      ( NULL == (pwdiSetUapsdAcParams = (WDI_SetUapsdAcParamsReqParamsType*)pEventData->pEventData)) ||
      ( NULL == (wdiSetUapsdAcParamsCb   = (WDI_SetUapsdAcParamsCb)pEventData->pCBfnc)))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  /*-----------------------------------------------------------------------
    Get message buffer
    ! TO DO : proper conversion into the HAL Message Request Format 
  -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_SET_UAPSD_PARAM_REQ, 
                        sizeof(uapsdAcParamsReq),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset + sizeof(uapsdAcParamsReq) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in Set UAPSD params req %x %x %x",
                pEventData, pwdiSetUapsdAcParams, wdiSetUapsdAcParamsCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  uapsdAcParamsReq.ac = pwdiSetUapsdAcParams->wdiUapsdInfo.ucAc;
  uapsdAcParamsReq.staidx = pwdiSetUapsdAcParams->wdiUapsdInfo.ucSTAIdx;
  uapsdAcParamsReq.up = pwdiSetUapsdAcParams->wdiUapsdInfo.ucUp;
  uapsdAcParamsReq.delayInterval = pwdiSetUapsdAcParams->wdiUapsdInfo.uDelayInterval;
  uapsdAcParamsReq.srvInterval = pwdiSetUapsdAcParams->wdiUapsdInfo.uSrvInterval;
  uapsdAcParamsReq.susInterval = pwdiSetUapsdAcParams->wdiUapsdInfo.uSusInterval;

  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                  &uapsdAcParamsReq, 
                  sizeof(uapsdAcParamsReq)); 

  pWDICtx->wdiReqStatusCB     = pwdiSetUapsdAcParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiSetUapsdAcParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Get STA Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiSetUapsdAcParamsCb, pEventData->pUserData, WDI_SET_UAPSD_PARAM_RESP); 
}/*WDI_ProcessSetUapsdAcParamsReq*/

/**
 @brief Process update UAPSD params Request function (called 
        when Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessUpdateUapsdParamsReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_UpdateUapsdReqParamsType*  pwdiUpdateUapsdReqParams = NULL;
   WDI_UpdateUapsdParamsCb        wdiUpdateUapsdParamsCb = NULL;
   wpt_uint8*               pSendBuffer         = NULL; 
   wpt_uint16               usDataOffset        = 0;
   wpt_uint16               usSendSize          = 0;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pEventData ) ||
       ( NULL == (pwdiUpdateUapsdReqParams = (WDI_UpdateUapsdReqParamsType*)pEventData->pEventData)) ||
       ( NULL == (wdiUpdateUapsdParamsCb   = (WDI_UpdateUapsdParamsCb)pEventData->pCBfnc)))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   /*-----------------------------------------------------------------------
     Get message buffer
     ! TO DO : proper conversion into the HAL Message Request Format 
   -----------------------------------------------------------------------*/
   if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_UPDATE_UAPSD_PARAM_REQ, 
                         sizeof(pwdiUpdateUapsdReqParams->wdiUpdateUapsdInfo),
                         &pSendBuffer, &usDataOffset, &usSendSize))||
       ( usSendSize < (usDataOffset + sizeof(pwdiUpdateUapsdReqParams->wdiUpdateUapsdInfo) )))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
               "Unable to get send buffer in Update UAPSD params req %x %x %x",
                 pEventData, pwdiUpdateUapsdReqParams, wdiUpdateUapsdParamsCb);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   wpalMemoryCopy( pSendBuffer+usDataOffset, 
                   &pwdiUpdateUapsdReqParams->wdiUpdateUapsdInfo, 
                   sizeof(pwdiUpdateUapsdReqParams->wdiUpdateUapsdInfo)); 

   pWDICtx->wdiReqStatusCB     = pwdiUpdateUapsdReqParams->wdiReqStatusCB;
   pWDICtx->pReqStatusUserData = pwdiUpdateUapsdReqParams->pUserData; 

   /*-------------------------------------------------------------------------
     Send Get STA Request to HAL 
   -------------------------------------------------------------------------*/
   return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        wdiUpdateUapsdParamsCb, pEventData->pUserData, WDI_UPDATE_UAPSD_PARAM_RESP); 
}/*WDI_ProcessUpdateUapsdParamsReq*/

/**
 @brief Process Configure RXP filter Request function (called 
        when Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessConfigureRxpFilterReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_ConfigureRxpFilterReqParamsType*  pwdiRxpFilterParams = NULL;
  WDI_ConfigureRxpFilterCb              wdiConfigureRxpFilterCb = NULL;
  wpt_uint8*               pSendBuffer         = NULL; 
  wpt_uint16               usDataOffset        = 0;
  wpt_uint16               usSendSize          = 0;
  tHalConfigureRxpFilterReqParams     halRxpFilterParams;

  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) ||
      ( NULL == (pwdiRxpFilterParams = (WDI_ConfigureRxpFilterReqParamsType*)pEventData->pEventData)) ||
      ( NULL == (wdiConfigureRxpFilterCb   = (WDI_ConfigureRxpFilterCb)pEventData->pCBfnc)))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

   /*-----------------------------------------------------------------------
     Get message buffer
   -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_CONFIGURE_RXP_FILTER_REQ, 
                        sizeof(halRxpFilterParams),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset + sizeof(halRxpFilterParams) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in Set UAPSD params req %x %x %x",
                pEventData, pwdiRxpFilterParams, wdiConfigureRxpFilterCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  halRxpFilterParams.setMcstBcstFilterSetting = 
      pwdiRxpFilterParams->wdiRxpFilterParam.ucSetMcstBcstFilterSetting;
  halRxpFilterParams.setMcstBcstFilter = 
      pwdiRxpFilterParams->wdiRxpFilterParam.ucSetMcstBcstFilter;

  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                  &halRxpFilterParams, 
                  sizeof(halRxpFilterParams)); 

  pWDICtx->wdiReqStatusCB     = pwdiRxpFilterParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiRxpFilterParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Get STA Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiConfigureRxpFilterCb, pEventData->pUserData, WDI_CONFIGURE_RXP_FILTER_RESP); 
}/*WDI_ProcessConfigureRxpFilterReq*/

/**
 @brief Process set beacon filter Request function (called 
        when Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetBeaconFilterReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_BeaconFilterReqParamsType*  pwdiBeaconFilterParams = NULL;
   WDI_SetBeaconFilterCb           wdiBeaconFilterCb = NULL;
   wpt_uint8*               pSendBuffer         = NULL; 
   wpt_uint16               usDataOffset        = 0;
   wpt_uint16               usSendSize          = 0;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pEventData ) ||
       ( NULL == (pwdiBeaconFilterParams = (WDI_BeaconFilterReqParamsType*)pEventData->pEventData)) ||
       ( NULL == (wdiBeaconFilterCb   = (WDI_SetBeaconFilterCb)pEventData->pCBfnc)))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   /*-----------------------------------------------------------------------
     Get message buffer
     ! TO DO : proper conversion into the HAL Message Request Format 
   -----------------------------------------------------------------------*/
   if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_SET_BEACON_FILTER_REQ, 
                         sizeof(pwdiBeaconFilterParams->wdiBeaconFilterInfo) + pwdiBeaconFilterParams->wdiBeaconFilterInfo.usIeNum * sizeof(tBeaconFilterIe),
                         &pSendBuffer, &usDataOffset, &usSendSize))||
       ( usSendSize < (usDataOffset + sizeof(pwdiBeaconFilterParams->wdiBeaconFilterInfo) )))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
               "Unable to get send buffer in Set beacon filter req %x %x %x",
                 pEventData, pwdiBeaconFilterParams, wdiBeaconFilterCb);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   wpalMemoryCopy( pSendBuffer+usDataOffset, 
                   &pwdiBeaconFilterParams->wdiBeaconFilterInfo, 
                   sizeof(pwdiBeaconFilterParams->wdiBeaconFilterInfo)); 
   wpalMemoryCopy( pSendBuffer+usDataOffset+sizeof(pwdiBeaconFilterParams->wdiBeaconFilterInfo), 
                   &pwdiBeaconFilterParams->aFilters[0], 
                   pwdiBeaconFilterParams->wdiBeaconFilterInfo.usIeNum * sizeof(tBeaconFilterIe)); 

   pWDICtx->wdiReqStatusCB     = pwdiBeaconFilterParams->wdiReqStatusCB;
   pWDICtx->pReqStatusUserData = pwdiBeaconFilterParams->pUserData; 

   /*-------------------------------------------------------------------------
     Send Get STA Request to HAL 
   -------------------------------------------------------------------------*/
   return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        wdiBeaconFilterCb, pEventData->pUserData, WDI_SET_BEACON_FILTER_RESP); 
}/*WDI_ProcessSetBeaconFilterReq*/

/**
 @brief Process remove beacon filter Request function (called 
        when Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessRemBeaconFilterReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_RemBeaconFilterReqParamsType*  pwdiBeaconFilterParams = NULL;
   WDI_RemBeaconFilterCb              wdiBeaconFilterCb = NULL;
   wpt_uint8*               pSendBuffer         = NULL; 
   wpt_uint16               usDataOffset        = 0;
   wpt_uint16               usSendSize          = 0;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pEventData ) ||
       ( NULL == (pwdiBeaconFilterParams = (WDI_RemBeaconFilterReqParamsType*)pEventData->pEventData)) ||
       ( NULL == (wdiBeaconFilterCb   = (WDI_RemBeaconFilterCb)pEventData->pCBfnc)))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   /*-----------------------------------------------------------------------
     Get message buffer
     ! TO DO : proper conversion into the HAL Message Request Format 
   -----------------------------------------------------------------------*/
   if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_REM_BEACON_FILTER_REQ, 
                         sizeof(pwdiBeaconFilterParams->wdiBeaconFilterInfo),
                         &pSendBuffer, &usDataOffset, &usSendSize))||
       ( usSendSize < (usDataOffset + sizeof(pwdiBeaconFilterParams->wdiBeaconFilterInfo) )))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
                  "Unable to get send buffer in remove beacon filter req %x %x %x",
                  pEventData, pwdiBeaconFilterParams, wdiBeaconFilterCb);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   wpalMemoryCopy( pSendBuffer+usDataOffset, 
                   &pwdiBeaconFilterParams->wdiBeaconFilterInfo, 
                   sizeof(pwdiBeaconFilterParams->wdiBeaconFilterInfo)); 

   pWDICtx->wdiReqStatusCB     = pwdiBeaconFilterParams->wdiReqStatusCB;
   pWDICtx->pReqStatusUserData = pwdiBeaconFilterParams->pUserData; 

   /*-------------------------------------------------------------------------
     Send Get STA Request to HAL 
   -------------------------------------------------------------------------*/
   return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        wdiBeaconFilterCb, pEventData->pUserData, WDI_REM_BEACON_FILTER_RESP); 
}

/**
 @brief Process set RSSI thresholds Request function (called 
        when Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetRSSIThresholdsReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_SetRSSIThresholdsReqParamsType*  pwdiRSSIThresholdsParams = NULL;
   WDI_SetRSSIThresholdsCb              wdiRSSIThresholdsCb = NULL;
   wpt_uint8*               pSendBuffer         = NULL; 
   wpt_uint16               usDataOffset        = 0;
   wpt_uint16               usSendSize          = 0;
   tHalRSSIThresholds       rssiThresholdsReq;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pEventData ) ||
       ( NULL == (pwdiRSSIThresholdsParams = (WDI_SetRSSIThresholdsReqParamsType*)pEventData->pEventData)) ||
       ( NULL == (wdiRSSIThresholdsCb   = (WDI_SetRSSIThresholdsCb)pEventData->pCBfnc)))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   /*-----------------------------------------------------------------------
     Get message buffer
     ! TO DO : proper conversion into the HAL Message Request Format 
   -----------------------------------------------------------------------*/
   if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_SET_RSSI_THRESHOLDS_REQ, 
                         sizeof(rssiThresholdsReq),
                         &pSendBuffer, &usDataOffset, &usSendSize))||
       ( usSendSize < (usDataOffset + sizeof(rssiThresholdsReq) )))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
                  "Unable to get send buffer in remove beacon filter req %x %x %x",
                  pEventData, pwdiRSSIThresholdsParams, wdiRSSIThresholdsCb);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   rssiThresholdsReq.bReserved10 = 
      pwdiRSSIThresholdsParams->wdiRSSIThresholdsInfo.bReserved10;
   rssiThresholdsReq.bRssiThres1NegNotify = 
      pwdiRSSIThresholdsParams->wdiRSSIThresholdsInfo.bRssiThres1NegNotify;
   rssiThresholdsReq.bRssiThres1PosNotify = 
      pwdiRSSIThresholdsParams->wdiRSSIThresholdsInfo.bRssiThres1PosNotify;
   rssiThresholdsReq.bRssiThres2NegNotify = 
      pwdiRSSIThresholdsParams->wdiRSSIThresholdsInfo.bRssiThres2NegNotify;
   rssiThresholdsReq.bRssiThres2PosNotify = 
      pwdiRSSIThresholdsParams->wdiRSSIThresholdsInfo.bRssiThres2PosNotify;
   rssiThresholdsReq.bRssiThres3NegNotify = 
      pwdiRSSIThresholdsParams->wdiRSSIThresholdsInfo.bRssiThres3NegNotify;
   rssiThresholdsReq.bRssiThres3PosNotify = 
      pwdiRSSIThresholdsParams->wdiRSSIThresholdsInfo.bRssiThres3PosNotify;
   rssiThresholdsReq.ucRssiThreshold1 = 
      pwdiRSSIThresholdsParams->wdiRSSIThresholdsInfo.ucRssiThreshold1;
   rssiThresholdsReq.ucRssiThreshold2 = 
      pwdiRSSIThresholdsParams->wdiRSSIThresholdsInfo.ucRssiThreshold2;
   rssiThresholdsReq.ucRssiThreshold3 = 
      pwdiRSSIThresholdsParams->wdiRSSIThresholdsInfo.ucRssiThreshold3;

   wpalMemoryCopy( pSendBuffer+usDataOffset, 
                   &rssiThresholdsReq, 
                   sizeof(rssiThresholdsReq)); 

   pWDICtx->wdiReqStatusCB     = pwdiRSSIThresholdsParams->wdiReqStatusCB;
   pWDICtx->pReqStatusUserData = pwdiRSSIThresholdsParams->pUserData; 

   /*-------------------------------------------------------------------------
     Send Get STA Request to HAL 
   -------------------------------------------------------------------------*/
   return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        wdiRSSIThresholdsCb, pEventData->pUserData, WDI_SET_RSSI_THRESHOLDS_RESP); 
}

/**
 @brief Process set RSSI thresholds Request function (called 
        when Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessHostOffloadReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_HostOffloadReqParamsType*  pwdiHostOffloadParams = NULL;
   WDI_HostOffloadCb              wdiHostOffloadCb = NULL;
   wpt_uint8*               pSendBuffer         = NULL; 
   wpt_uint16               usDataOffset        = 0;
   wpt_uint16               usSendSize          = 0;
   tHalHostOffloadReq       hostOffloadReq;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pEventData ) ||
       ( NULL == (pwdiHostOffloadParams = (WDI_HostOffloadReqParamsType*)pEventData->pEventData)) ||
       ( NULL == (wdiHostOffloadCb   = (WDI_HostOffloadCb)pEventData->pCBfnc)))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   /*-----------------------------------------------------------------------
     Get message buffer
     ! TO DO : proper conversion into the HAL Message Request Format 
   -----------------------------------------------------------------------*/
   if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_HOST_OFFLOAD_REQ, 
                         sizeof(hostOffloadReq),
                         &pSendBuffer, &usDataOffset, &usSendSize))||
       ( usSendSize < (usDataOffset + sizeof(hostOffloadReq) )))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
                  "Unable to get send buffer in host offload req %x %x %x",
                  pEventData, pwdiHostOffloadParams, wdiHostOffloadCb);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   hostOffloadReq.offloadType = pwdiHostOffloadParams->wdiHostOffloadInfo.ucOffloadType;
   hostOffloadReq.enableOrDisable = pwdiHostOffloadParams->wdiHostOffloadInfo.ucEnableOrDisable;
   if( 0 == hostOffloadReq.offloadType )
   {
      wpalMemoryCopy(hostOffloadReq.params.hostIpv4Addr,
                     pwdiHostOffloadParams->wdiHostOffloadInfo.params.aHostIpv4Addr,
                     4);
   }
   else
   {
      wpalMemoryCopy(hostOffloadReq.params.hostIpv6Addr,
                     pwdiHostOffloadParams->wdiHostOffloadInfo.params.aHostIpv6Addr,
                     16);
   }
   wpalMemoryCopy( pSendBuffer+usDataOffset, 
                   &hostOffloadReq, 
                   sizeof(hostOffloadReq)); 

   pWDICtx->wdiReqStatusCB     = pwdiHostOffloadParams->wdiReqStatusCB;
   pWDICtx->pReqStatusUserData = pwdiHostOffloadParams->pUserData; 

   /*-------------------------------------------------------------------------
     Send Get STA Request to HAL 
   -------------------------------------------------------------------------*/
   return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        wdiHostOffloadCb, pEventData->pUserData, WDI_HOST_OFFLOAD_RESP); 
}/*WDI_ProcessHostOffloadReq*/

/**
 @brief Process Keep Alive Request function (called 
        when Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessKeepAliveReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_KeepAliveReqParamsType*  pwdiKeepAliveParams = NULL;
   WDI_KeepAliveCb              wdiKeepAliveCb = NULL;
   wpt_uint8*               pSendBuffer         = NULL; 
   wpt_uint16               usDataOffset        = 0;
   wpt_uint16               usSendSize          = 0;
   tHalKeepAliveReq         keepAliveReq;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pEventData ) ||
       ( NULL == (pwdiKeepAliveParams = (WDI_KeepAliveReqParamsType*)pEventData->pEventData)) ||
       ( NULL == (wdiKeepAliveCb   = (WDI_KeepAliveCb)pEventData->pCBfnc)))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_ERROR,
               "Invalid parameters in Keep Alive req");
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   /*-----------------------------------------------------------------------
     Get message buffer
     ! TO DO : proper conversion into the HAL Message Request Format 
   -----------------------------------------------------------------------*/
   if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_KEEP_ALIVE_REQ, 
                         sizeof(keepAliveReq),
                         &pSendBuffer, &usDataOffset, &usSendSize))||
       ( usSendSize < (usDataOffset + sizeof(keepAliveReq) )))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "Unable to get send buffer in keep alive req %x %x %x",
                  pEventData, pwdiKeepAliveParams, wdiKeepAliveCb);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   keepAliveReq.packetType = pwdiKeepAliveParams->wdiKeepAliveInfo.ucPacketType;
   keepAliveReq.timePeriod = pwdiKeepAliveParams->wdiKeepAliveInfo.ucTimePeriod;

   if(pwdiKeepAliveParams->wdiKeepAliveInfo.ucPacketType == 2)
   {
   wpalMemoryCopy(keepAliveReq.hostIpv4Addr,
                     pwdiKeepAliveParams->wdiKeepAliveInfo.aHostIpv4Addr,
                     HAL_IPV4_ADDR_LEN);
   wpalMemoryCopy(keepAliveReq.destIpv4Addr,
                     pwdiKeepAliveParams->wdiKeepAliveInfo.aDestIpv4Addr,
                     HAL_IPV4_ADDR_LEN);   
   wpalMemoryCopy(keepAliveReq.destMacAddr,
                     pwdiKeepAliveParams->wdiKeepAliveInfo.aDestMacAddr,
                     HAL_MAC_ADDR_LEN);
   }
      
   wpalMemoryCopy( pSendBuffer+usDataOffset, 
                   &keepAliveReq, 
                   sizeof(keepAliveReq)); 

   WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_INFO,
               "Process keep alive req %d",sizeof(keepAliveReq));

   WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_INFO,
               "Process keep alive req time period %d",keepAliveReq.timePeriod);

   pWDICtx->wdiReqStatusCB     = pwdiKeepAliveParams->wdiReqStatusCB;
   pWDICtx->pReqStatusUserData = pwdiKeepAliveParams->pUserData; 

  WPAL_TRACE(eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_INFO,
                  "Sending keep alive req to HAL");

   /*-------------------------------------------------------------------------
     Send Get STA Request to HAL 
   -------------------------------------------------------------------------*/
   return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        wdiKeepAliveCb, pEventData->pUserData, WDI_KEEP_ALIVE_RESP); 
}/*WDI_ProcessKeepAliveReq*/


/**
 @brief Process Wowl add bc ptrn Request function (called 
        when Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessWowlAddBcPtrnReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_WowlAddBcPtrnReqParamsType*  pwdiWowlAddBcPtrnParams = NULL;
   WDI_WowlAddBcPtrnCb              wdiWowlAddBcPtrnCb = NULL;
   wpt_uint8*               pSendBuffer         = NULL; 
   wpt_uint16               usDataOffset        = 0;
   wpt_uint16               usSendSize          = 0;
   tHalWowlAddBcastPtrn     wowlAddBcPtrnReq;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pEventData ) ||
       ( NULL == (pwdiWowlAddBcPtrnParams = (WDI_WowlAddBcPtrnReqParamsType*)pEventData->pEventData)) ||
       ( NULL == (wdiWowlAddBcPtrnCb   = (WDI_WowlAddBcPtrnCb)pEventData->pCBfnc)))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   /*-----------------------------------------------------------------------
     Get message buffer
     ! TO DO : proper conversion into the HAL Message Request Format 
   -----------------------------------------------------------------------*/
   if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_WOWL_ADD_BC_PTRN_REQ, 
                         sizeof(wowlAddBcPtrnReq),
                         &pSendBuffer, &usDataOffset, &usSendSize))||
       ( usSendSize < (usDataOffset + sizeof(wowlAddBcPtrnReq) )))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
                  "Unable to get send buffer in Wowl add bc ptrn req %x %x %x",
                  pEventData, pwdiWowlAddBcPtrnParams, wdiWowlAddBcPtrnCb);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   wowlAddBcPtrnReq.ucPatternId = 
      pwdiWowlAddBcPtrnParams->wdiWowlAddBcPtrnInfo.ucPatternId;
   wowlAddBcPtrnReq.ucPatternByteOffset = 
      pwdiWowlAddBcPtrnParams->wdiWowlAddBcPtrnInfo.ucPatternByteOffset;
   wowlAddBcPtrnReq.ucPatternMaskSize = 
      pwdiWowlAddBcPtrnParams->wdiWowlAddBcPtrnInfo.ucPatternMaskSize;
   wowlAddBcPtrnReq.ucPatternSize = 
      pwdiWowlAddBcPtrnParams->wdiWowlAddBcPtrnInfo.ucPatternSize;
   wpalMemoryCopy(wowlAddBcPtrnReq.ucPattern,
                  pwdiWowlAddBcPtrnParams->wdiWowlAddBcPtrnInfo.ucPattern,
                  pwdiWowlAddBcPtrnParams->wdiWowlAddBcPtrnInfo.ucPatternSize);
   wpalMemoryCopy(wowlAddBcPtrnReq.ucPatternMask,
                  pwdiWowlAddBcPtrnParams->wdiWowlAddBcPtrnInfo.ucPatternMask,
                  pwdiWowlAddBcPtrnParams->wdiWowlAddBcPtrnInfo.ucPatternMaskSize);

   wpalMemoryCopy( pSendBuffer+usDataOffset, 
                   &wowlAddBcPtrnReq, 
                   sizeof(wowlAddBcPtrnReq)); 

   pWDICtx->wdiReqStatusCB     = pwdiWowlAddBcPtrnParams->wdiReqStatusCB;
   pWDICtx->pReqStatusUserData = pwdiWowlAddBcPtrnParams->pUserData; 

   /*-------------------------------------------------------------------------
     Send Get STA Request to HAL 
   -------------------------------------------------------------------------*/
   return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        wdiWowlAddBcPtrnCb, pEventData->pUserData, WDI_WOWL_ADD_BC_PTRN_RESP); 
}/*WDI_ProcessWowlAddBcPtrnReq*/

/**
 @brief Process Wowl delete bc ptrn Request function (called 
        when Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessWowlDelBcPtrnReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_WowlDelBcPtrnReqParamsType*  pwdiWowlDelBcPtrnParams = NULL;
   WDI_WowlDelBcPtrnCb              wdiWowlDelBcPtrnCb = NULL;
   wpt_uint8*               pSendBuffer         = NULL; 
   wpt_uint16               usDataOffset        = 0;
   wpt_uint16               usSendSize          = 0;
   tHalWowlDelBcastPtrn     wowlDelBcPtrnReq;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pEventData ) ||
       ( NULL == (pwdiWowlDelBcPtrnParams = (WDI_WowlDelBcPtrnReqParamsType*)pEventData->pEventData)) ||
       ( NULL == (wdiWowlDelBcPtrnCb   = (WDI_WowlDelBcPtrnCb)pEventData->pCBfnc)))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   /*-----------------------------------------------------------------------
     Get message buffer
     ! TO DO : proper conversion into the HAL Message Request Format 
   -----------------------------------------------------------------------*/
   if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_WOWL_DEL_BC_PTRN_REQ, 
                         sizeof(wowlDelBcPtrnReq),
                         &pSendBuffer, &usDataOffset, &usSendSize))||
       ( usSendSize < (usDataOffset + sizeof(wowlDelBcPtrnReq) )))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
                  "Unable to get send buffer in Wowl del bc ptrn req %x %x %x",
                  pEventData, pwdiWowlDelBcPtrnParams, wdiWowlDelBcPtrnCb);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   wowlDelBcPtrnReq.ucPatternId = 
      pwdiWowlDelBcPtrnParams->wdiWowlDelBcPtrnInfo.ucPatternId;
   wpalMemoryCopy( pSendBuffer+usDataOffset, 
                   &wowlDelBcPtrnReq, 
                   sizeof(wowlDelBcPtrnReq)); 

   pWDICtx->wdiReqStatusCB     = pwdiWowlDelBcPtrnParams->wdiReqStatusCB;
   pWDICtx->pReqStatusUserData = pwdiWowlDelBcPtrnParams->pUserData; 

   /*-------------------------------------------------------------------------
     Send Get STA Request to HAL 
   -------------------------------------------------------------------------*/
   return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        wdiWowlDelBcPtrnCb, pEventData->pUserData, WDI_WOWL_DEL_BC_PTRN_RESP); 
}/*WDI_ProcessWowlDelBcPtrnReq*/

/**
 @brief Process Wowl enter Request function (called 
        when Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessWowlEnterReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_WowlEnterReqParamsType*  pwdiWowlEnterParams = NULL;
   WDI_WowlEnterReqCb           wdiWowlEnterCb = NULL;
   wpt_uint8*               pSendBuffer         = NULL; 
   wpt_uint16               usDataOffset        = 0;
   wpt_uint16               usSendSize          = 0;
   tHalWowlEnterParams      wowlEnterReq;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pEventData ) ||
       ( NULL == (pwdiWowlEnterParams = (WDI_WowlEnterReqParamsType*)pEventData->pEventData)) ||
       ( NULL == (wdiWowlEnterCb   = (WDI_WowlEnterReqCb)pEventData->pCBfnc)))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   /*-----------------------------------------------------------------------
     Get message buffer
     ! TO DO : proper conversion into the HAL Message Request Format 
   -----------------------------------------------------------------------*/
   if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_WOWL_ENTER_REQ, 
                         sizeof(wowlEnterReq),
                         &pSendBuffer, &usDataOffset, &usSendSize))||
       ( usSendSize < (usDataOffset + sizeof(wowlEnterReq) )))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
                  "Unable to get send buffer in Wowl enter req %x %x %x",
                  pEventData, pwdiWowlEnterParams, wdiWowlEnterCb);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   wowlEnterReq.ucMagicPktEnable = 
      pwdiWowlEnterParams->wdiWowlEnterInfo.ucMagicPktEnable;
   wowlEnterReq.ucPatternFilteringEnable = 
      pwdiWowlEnterParams->wdiWowlEnterInfo.ucPatternFilteringEnable;
   wowlEnterReq.ucUcastPatternFilteringEnable = 
      pwdiWowlEnterParams->wdiWowlEnterInfo.ucUcastPatternFilteringEnable;
   wowlEnterReq.ucWowChnlSwitchRcv = 
      pwdiWowlEnterParams->wdiWowlEnterInfo.ucWowChnlSwitchRcv;
   wowlEnterReq.ucWowDeauthRcv = 
      pwdiWowlEnterParams->wdiWowlEnterInfo.ucWowDeauthRcv;
   wowlEnterReq.ucWowDisassocRcv = 
      pwdiWowlEnterParams->wdiWowlEnterInfo.ucWowDisassocRcv;
   wowlEnterReq.ucWowMaxMissedBeacons = 
      pwdiWowlEnterParams->wdiWowlEnterInfo.ucWowMaxMissedBeacons;
   wowlEnterReq.ucWowMaxSleepUsec = 
      pwdiWowlEnterParams->wdiWowlEnterInfo.ucWowMaxSleepUsec;
   wpalMemoryCopy(wowlEnterReq.magicPtrn,
                  pwdiWowlEnterParams->wdiWowlEnterInfo.magicPtrn,
                  sizeof(tSirMacAddr));

   wpalMemoryCopy( pSendBuffer+usDataOffset, 
                   &wowlEnterReq, 
                   sizeof(wowlEnterReq)); 

   pWDICtx->wdiReqStatusCB     = pwdiWowlEnterParams->wdiReqStatusCB;
   pWDICtx->pReqStatusUserData = pwdiWowlEnterParams->pUserData; 

   /*-------------------------------------------------------------------------
     Send Get STA Request to HAL 
   -------------------------------------------------------------------------*/
   return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        wdiWowlEnterCb, pEventData->pUserData, WDI_WOWL_ENTER_RESP); 
}/*WDI_ProcessWowlEnterReq*/

/**
 @brief Process Wowl exit Request function (called when Main FSM
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessWowlExitReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_WowlExitReqCb           wdiWowlExitCb = NULL;
   wpt_uint8*               pSendBuffer         = NULL; 
   wpt_uint16               usDataOffset        = 0;
   wpt_uint16               usSendSize          = 0;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pEventData ) ||
       ( NULL == (wdiWowlExitCb   = (WDI_WowlExitReqCb)pEventData->pCBfnc)))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   /*-----------------------------------------------------------------------
     Get message buffer
     ! TO DO : proper conversion into the HAL Message Request Format 
   -----------------------------------------------------------------------*/
   if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_WOWL_EXIT_REQ, 
                                                     0,
                                                     &pSendBuffer, &usDataOffset, &usSendSize))||
       ( usSendSize < (usDataOffset )))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
               "Unable to get send buffer in Wowl Exit req %x %x",
                 pEventData, wdiWowlExitCb);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   /*-------------------------------------------------------------------------
     Send Get STA Request to HAL 
   -------------------------------------------------------------------------*/
   return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        wdiWowlExitCb, pEventData->pUserData, WDI_WOWL_EXIT_RESP); 
}/*WDI_ProcessWowlExitReq*/

/**
 @brief Process Configure Apps Cpu Wakeup State Request function
        (called when Main FSM allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessConfigureAppsCpuWakeupStateReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_ConfigureAppsCpuWakeupStateReqParamsType*  pwdiAppsCpuWakeupStateParams = NULL;
   WDI_ConfigureAppsCpuWakeupStateCb              wdiConfigureAppsCpuWakeupStateCb = NULL;
   wpt_uint8*               pSendBuffer         = NULL; 
   wpt_uint16               usDataOffset        = 0;
   wpt_uint16               usSendSize          = 0;
   tHalConfigureAppsCpuWakeupStateReqParams  halCfgAppsCpuWakeupStateReqParams;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pEventData ) ||
       ( NULL == (pwdiAppsCpuWakeupStateParams = (WDI_ConfigureAppsCpuWakeupStateReqParamsType*)pEventData->pEventData)) ||
       ( NULL == (wdiConfigureAppsCpuWakeupStateCb   = (WDI_ConfigureAppsCpuWakeupStateCb)pEventData->pCBfnc)))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   /*-----------------------------------------------------------------------
     Get message buffer
   -----------------------------------------------------------------------*/
   if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_CONFIGURE_APPS_CPU_WAKEUP_STATE_REQ, 
                         sizeof(halCfgAppsCpuWakeupStateReqParams),
                         &pSendBuffer, &usDataOffset, &usSendSize))||
       ( usSendSize < (usDataOffset + sizeof(pwdiAppsCpuWakeupStateParams->bIsAppsAwake) )))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
               "Unable to get send buffer in Apps CPU Wakeup State req %x %x %x",
                 pEventData, pwdiAppsCpuWakeupStateParams, wdiConfigureAppsCpuWakeupStateCb);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   halCfgAppsCpuWakeupStateReqParams.isAppsCpuAwake = 
                           pwdiAppsCpuWakeupStateParams->bIsAppsAwake;

   wpalMemoryCopy( pSendBuffer+usDataOffset, 
                   &halCfgAppsCpuWakeupStateReqParams, 
                   sizeof(halCfgAppsCpuWakeupStateReqParams)); 

   pWDICtx->wdiReqStatusCB     = pwdiAppsCpuWakeupStateParams->wdiReqStatusCB;
   pWDICtx->pReqStatusUserData = pwdiAppsCpuWakeupStateParams->pUserData; 

   /*-------------------------------------------------------------------------
     Send Get STA Request to HAL 
   -------------------------------------------------------------------------*/
   return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        wdiConfigureAppsCpuWakeupStateCb, pEventData->pUserData, 
                        WDI_CONFIGURE_APPS_CPU_WAKEUP_STATE_RESP); 
}/*WDI_ProcessConfigureAppsCpuWakeupStateReq*/

#ifdef WLAN_FEATURE_VOWIFI_11R
/**
 @brief Process Aggregated Add TSpec Request function (called when Main FSM
        allows it)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessAggrAddTSpecReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_AggrAddTSReqParamsType*  pwdiAggrAddTSParams;
  WDI_AggrAddTsRspCb           wdiAggrAddTSRspCb;
  wpt_uint8                ucCurrentBSSSesIdx  = 0; 
  WDI_BSSSessionType*      pBSSSes             = NULL;
  wpt_uint8*               pSendBuffer         = NULL; 
  wpt_uint16               usDataOffset        = 0;
  wpt_uint16               usSendSize          = 0;
  WDI_Status               wdiStatus           = WDI_STATUS_SUCCESS; 
  wpt_macAddr              macBSSID;
  tAggrAddTsReq            halAggrAddTsReq;
  int i;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) || ( NULL == pEventData->pEventData ) ||
      ( NULL == pEventData->pCBfnc ))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }
  wpalMemoryFill( &halAggrAddTsReq, sizeof(tAggrAddTsReq), 0 );
  pwdiAggrAddTSParams = (WDI_AggrAddTSReqParamsType*)pEventData->pEventData;
  wdiAggrAddTSRspCb   = (WDI_AggrAddTsRspCb)pEventData->pCBfnc;
  /*-------------------------------------------------------------------------
    Check to see if we are in the middle of an association, if so queue, if
    not it means it is free to process request 
  -------------------------------------------------------------------------*/
  wpalMutexAcquire(&pWDICtx->wptMutex);

  /*------------------------------------------------------------------------
    Find the BSS for which the request is made and identify WDI session
  ------------------------------------------------------------------------*/
  if ( WDI_STATUS_SUCCESS != WDI_STATableGetStaBSSIDAddr(pWDICtx, 
                                        pwdiAggrAddTSParams->wdiAggrTsInfo.ucSTAIdx, 
                                        &macBSSID))
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
             "This station does not exist in the WDI Station Table %d");
    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_FAILURE; 
  }

  ucCurrentBSSSesIdx = WDI_FindAssocSession( pWDICtx, macBSSID, &pBSSSes); 
  if ( NULL == pBSSSes ) 
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS does not yet exist");

    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_NOT_ALLOWED; 
  }
 
  /*------------------------------------------------------------------------
    Check if this BSS is being currently processed or queued,
    if queued - queue the new request as well 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_TRUE == pBSSSes->bAssocReqQueued )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS exists but currently queued");

    wdiStatus = WDI_QueueAssocRequest( pWDICtx, pBSSSes, pEventData); 
    wpalMutexRelease(&pWDICtx->wptMutex);
    return wdiStatus; 
  }

  wpalMutexRelease(&pWDICtx->wptMutex);
  /*-----------------------------------------------------------------------
    Get message buffer
    ! TO DO : proper conversion into the HAL Message Request Format 
  -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_AGGR_ADD_TS_REQ, 
                        sizeof(tAggrAddTsParams),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < (usDataOffset + sizeof(tAggrAddTsParams) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in set bss key req %x %x %x",
                pEventData, pwdiAggrAddTSParams, wdiAggrAddTSRspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  halAggrAddTsReq.aggrAddTsParam.staIdx = 
     pwdiAggrAddTSParams->wdiAggrTsInfo.ucSTAIdx;
  halAggrAddTsReq.aggrAddTsParam.tspecIdx = 
     pwdiAggrAddTSParams->wdiAggrTsInfo.ucTspecIdx;

  for( i = 0; i < WLAN_HAL_MAX_AC; i++ )
  {
     halAggrAddTsReq.aggrAddTsParam.tspec[i].type = 
        pwdiAggrAddTSParams->wdiAggrTsInfo.wdiTspecIE[i].ucType;
     halAggrAddTsReq.aggrAddTsParam.tspec[i].length = 
        pwdiAggrAddTSParams->wdiAggrTsInfo.wdiTspecIE[i].ucLength;
     halAggrAddTsReq.aggrAddTsParam.tspec[i].tsinfo.traffic.ackPolicy = 
        pwdiAggrAddTSParams->wdiAggrTsInfo.wdiTspecIE[i].wdiTSinfo.wdiTraffic.
        ackPolicy;
     halAggrAddTsReq.aggrAddTsParam.tspec[i].tsinfo.traffic.accessPolicy = 
        pwdiAggrAddTSParams->wdiAggrTsInfo.wdiTspecIE[i].wdiTSinfo.wdiTraffic.
        accessPolicy;
     halAggrAddTsReq.aggrAddTsParam.tspec[i].tsinfo.traffic.userPrio = 
        pwdiAggrAddTSParams->wdiAggrTsInfo.wdiTspecIE[i].wdiTSinfo.wdiTraffic.
        userPrio;
     halAggrAddTsReq.aggrAddTsParam.tspec[i].tsinfo.traffic.psb = 
        pwdiAggrAddTSParams->wdiAggrTsInfo.wdiTspecIE[i].wdiTSinfo.wdiTraffic.
        psb;
     halAggrAddTsReq.aggrAddTsParam.tspec[i].tsinfo.traffic.aggregation = 
        pwdiAggrAddTSParams->wdiAggrTsInfo.wdiTspecIE[i].wdiTSinfo.wdiTraffic.
        aggregation;
     halAggrAddTsReq.aggrAddTsParam.tspec[i].tsinfo.traffic.direction = 
        pwdiAggrAddTSParams->wdiAggrTsInfo.wdiTspecIE[i].wdiTSinfo.wdiTraffic.
        direction;
     halAggrAddTsReq.aggrAddTsParam.tspec[i].tsinfo.traffic.tsid = 
        pwdiAggrAddTSParams->wdiAggrTsInfo.wdiTspecIE[i].wdiTSinfo.wdiTraffic.
        trafficType;
     halAggrAddTsReq.aggrAddTsParam.tspec[i].tsinfo.traffic.tsid = 
        pwdiAggrAddTSParams->wdiAggrTsInfo.wdiTspecIE[i].wdiTSinfo.wdiTraffic.
        trafficType;
     halAggrAddTsReq.aggrAddTsParam.tspec[i].tsinfo.schedule.rsvd = 
        pwdiAggrAddTSParams->wdiAggrTsInfo.wdiTspecIE[i].wdiTSinfo.wdiSchedule.rsvd;
     halAggrAddTsReq.aggrAddTsParam.tspec[i].tsinfo.schedule.schedule = 
        pwdiAggrAddTSParams->wdiAggrTsInfo.wdiTspecIE[i].wdiTSinfo.wdiSchedule.schedule;
        
                  
     halAggrAddTsReq.aggrAddTsParam.tspec[i].nomMsduSz = 
        pwdiAggrAddTSParams->wdiAggrTsInfo.wdiTspecIE[i].usNomMsduSz;
     halAggrAddTsReq.aggrAddTsParam.tspec[i].maxMsduSz = 
        pwdiAggrAddTSParams->wdiAggrTsInfo.wdiTspecIE[i].usMaxMsduSz;
     halAggrAddTsReq.aggrAddTsParam.tspec[i].minSvcInterval = 
        pwdiAggrAddTSParams->wdiAggrTsInfo.wdiTspecIE[i].uMinSvcInterval;
     halAggrAddTsReq.aggrAddTsParam.tspec[i].maxSvcInterval = 
        pwdiAggrAddTSParams->wdiAggrTsInfo.wdiTspecIE[i].uMaxSvcInterval;
     halAggrAddTsReq.aggrAddTsParam.tspec[i].inactInterval = 
        pwdiAggrAddTSParams->wdiAggrTsInfo.wdiTspecIE[i].uInactInterval;
     halAggrAddTsReq.aggrAddTsParam.tspec[i].suspendInterval = 
        pwdiAggrAddTSParams->wdiAggrTsInfo.wdiTspecIE[i].uSuspendInterval;
     halAggrAddTsReq.aggrAddTsParam.tspec[i].svcStartTime = 
        pwdiAggrAddTSParams->wdiAggrTsInfo.wdiTspecIE[i].uSvcStartTime;
     halAggrAddTsReq.aggrAddTsParam.tspec[i].minDataRate = 
        pwdiAggrAddTSParams->wdiAggrTsInfo.wdiTspecIE[i].uMinDataRate;
     halAggrAddTsReq.aggrAddTsParam.tspec[i].meanDataRate = 
        pwdiAggrAddTSParams->wdiAggrTsInfo.wdiTspecIE[i].uMeanDataRate;
     halAggrAddTsReq.aggrAddTsParam.tspec[i].peakDataRate = 
        pwdiAggrAddTSParams->wdiAggrTsInfo.wdiTspecIE[i].uPeakDataRate;
     halAggrAddTsReq.aggrAddTsParam.tspec[i].maxBurstSz = 
        pwdiAggrAddTSParams->wdiAggrTsInfo.wdiTspecIE[i].uMaxBurstSz;
     halAggrAddTsReq.aggrAddTsParam.tspec[i].delayBound = 
        pwdiAggrAddTSParams->wdiAggrTsInfo.wdiTspecIE[i].uDelayBound;
     halAggrAddTsReq.aggrAddTsParam.tspec[i].minPhyRate = 
        pwdiAggrAddTSParams->wdiAggrTsInfo.wdiTspecIE[i].uMinPhyRate;
     halAggrAddTsReq.aggrAddTsParam.tspec[i].surplusBw = 
        pwdiAggrAddTSParams->wdiAggrTsInfo.wdiTspecIE[i].usSurplusBw;
     halAggrAddTsReq.aggrAddTsParam.tspec[i].mediumTime = 
        pwdiAggrAddTSParams->wdiAggrTsInfo.wdiTspecIE[i].usMediumTime;
  }

  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                  &halAggrAddTsReq, 
                  sizeof(halAggrAddTsReq)); 

  pWDICtx->wdiReqStatusCB     = pwdiAggrAddTSParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiAggrAddTSParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Add TS Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                       wdiAggrAddTSRspCb, pEventData->pUserData,
                       WDI_AGGR_ADD_TS_RESP); 
}/*WDI_ProcessAggrAddTSpecReq*/
#endif /* WLAN_FEATURE_VOWIFI_11R */

/**
 @brief Process Shutdown Request function (called when Main FSM
        allows it)

 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessShutdownReq
(
 WDI_ControlBlockType*  pWDICtx,
 WDI_EventInfoType*     pEventData
 )
{
   wpt_status              wptStatus;


   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
     -------------------------------------------------------------------------*/
   if ( NULL == pEventData )
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
            "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE;
   }

   wpalMutexAcquire(&pWDICtx->wptMutex);


   gWDIInitialized = eWLAN_PAL_FALSE;
   /*! TO DO: stop the data services */
   if ( eDRIVER_TYPE_MFG != pWDICtx->driverMode )
   {
      /*Stop the STA Table !UT- check this logic again
        It is safer to do it here than on the response - because a stop is imminent*/
      WDI_STATableStop(pWDICtx);

      /* Stop Transport Driver, DXE */
      WDTS_Stop(pWDICtx);
   }

   /*Clear all pending request*/
   WDI_ClearPendingRequests(pWDICtx);
   /* Close Data transport*/
   /* FTM mode does not open Data Path */
   if ( eDRIVER_TYPE_MFG != pWDICtx->driverMode )
   {
      WDTS_Close(pWDICtx);
   }
   /*Close the STA Table !UT- check this logic again*/
   WDI_STATableClose(pWDICtx);
   /*close the PAL */
   wptStatus = wpalClose(pWDICtx->pPALContext);
   if ( eWLAN_PAL_STATUS_SUCCESS !=  wptStatus )
   {
      WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
            "Failed to wpal Close %d", wptStatus);
      WDI_ASSERT(0);
   }

   /*Transition back to init state*/
   WDI_STATE_TRANSITION( pWDICtx, WDI_INIT_ST);

   wpalMutexRelease(&pWDICtx->wptMutex);

   /*Make sure the expected state is properly defaulted to Init*/
   pWDICtx->ucExpectedStateTransition = WDI_INIT_ST; 


   return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessShutdownReq*/

/*========================================================================
          Main DAL Control Path Response Processing API 
========================================================================*/

/**
 @brief Process Start Response function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessStartRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_StartRspParamsType   wdiRspParams;
  WDI_StartRspCb           wdiStartRspCb = NULL;

  tHalMacStartRspParams*   startRspParams;

#ifndef HAL_SELF_STA_PER_BSS
  WDI_AddStaParams         wdiAddSTAParam = {0};
#endif
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  wdiStartRspCb = (WDI_StartRspCb)pWDICtx->pfncRspCB; 
  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData) ||
      ( NULL == wdiStartRspCb ))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  if ( sizeof(tHalMacStartRspParams) > pEventData->uEventDataSize )
  {
     // not enough data was received
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_FATAL,
                 "Invalid response length in Start Resp Expect %x Rcvd %x",
                 sizeof(tHalMacStartRspParams), pEventData->uEventDataSize);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  /*-------------------------------------------------------------------------
    Unpack HAL Response Message - the header was already extracted by the
    main Response Handling procedure 
  -------------------------------------------------------------------------*/
  startRspParams = (tHalMacStartRspParams *) pEventData->pEventData;

  wdiRspParams.ucMaxBssids   = startRspParams->ucMaxBssids;
  wdiRspParams.ucMaxStations = startRspParams->ucMaxStations;
  wdiRspParams.wlanCompiledVersion.major = WLAN_HAL_VER_MAJOR;
  wdiRspParams.wlanCompiledVersion.minor = WLAN_HAL_VER_MINOR;
  wdiRspParams.wlanCompiledVersion.version = WLAN_HAL_VER_VERSION;
  wdiRspParams.wlanCompiledVersion.revision = WLAN_HAL_VER_REVISION;
  wdiRspParams.wlanReportedVersion.major =
                               startRspParams->wcnssWlanVersion.major;
  wdiRspParams.wlanReportedVersion.minor =
                               startRspParams->wcnssWlanVersion.minor;
  wdiRspParams.wlanReportedVersion.version =
                               startRspParams->wcnssWlanVersion.version;
  wdiRspParams.wlanReportedVersion.revision =
                               startRspParams->wcnssWlanVersion.revision;
  strlcpy(wdiRspParams.wcnssSoftwareVersion,
          startRspParams->wcnssCrmVersionString,
          sizeof(wdiRspParams.wcnssSoftwareVersion));
  strlcpy(wdiRspParams.wcnssHardwareVersion,
          startRspParams->wcnssWlanVersionString,
          sizeof(wdiRspParams.wcnssHardwareVersion));
  wdiRspParams.wdiStatus     = WDI_HAL_2_WDI_STATUS(startRspParams->status);

  wpalMutexAcquire(&pWDICtx->wptMutex);
  if ( WDI_STATUS_SUCCESS == wdiRspParams.wdiStatus  )
  {
    pWDICtx->ucExpectedStateTransition =  WDI_STARTED_ST;

    /*Cache the start response for further use*/
    wpalMemoryCopy( &pWDICtx->wdiCachedStartRspParams ,
                  &wdiRspParams, 
                  sizeof(pWDICtx->wdiCachedStartRspParams));

  }
  else
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_ERROR,
               "Failed to start device with status %s(%d)",
               WDI_getHALStatusMsgString(startRspParams->status),
               startRspParams->status);

    /*Set the expected state transition to stopped - because the start has
      failed*/
    pWDICtx->ucExpectedStateTransition =  WDI_STOPPED_ST;

    wpalMutexRelease(&pWDICtx->wptMutex);

     /*Notify UMAC*/
    wdiStartRspCb( &wdiRspParams, pWDICtx->pRspCBUserData);
    
    WDI_DetectedDeviceError(pWDICtx, wdiRspParams.wdiStatus);

    /*Although the response is an error - it was processed by our function
    so as far as the caller is concerned this is a succesful reponse processing*/
    return WDI_STATUS_SUCCESS;
  }
  
  wpalMutexRelease(&pWDICtx->wptMutex);

  if(eDRIVER_TYPE_MFG == pWDICtx->driverMode)
  {
    /* FTM mode does not need to execute below */
    /* Notify UMAC */
    wdiStartRspCb( &wdiRspParams, pWDICtx->pRspCBUserData);
    return WDI_STATUS_SUCCESS;
  }

  /* START the Data transport */
  WDTS_startTransport(pWDICtx);

  /*Start the STA Table !- check this logic again*/
  WDI_STATableStart(pWDICtx);

#ifndef HAL_SELF_STA_PER_BSS
  /* Store the Self STA Index */
  pWDICtx->ucSelfStaId = halStartRspMsg.startRspParams.selfStaIdx;

  pWDICtx->usSelfStaDpuId = wdiRspParams.usSelfStaDpuId;
  wpalMemoryCopy(pWDICtx->macSelfSta, wdiRspParams.macSelfSta,
                 WDI_MAC_ADDR_LEN);

  /* At this point add the self-STA */

  /*! TO DO: wdiAddSTAParam.bcastMgmtDpuSignature */
  /* !TO DO: wdiAddSTAParam.bcastDpuSignature */
  /*! TO DO: wdiAddSTAParam.dpuSig */
  /*! TO DO: wdiAddSTAParam.ucWmmEnabled */
  /*! TO DO: wdiAddSTAParam.ucHTCapable */
  /*! TO DO: wdiAddSTAParam.ucRmfEnabled */

  //all DPU indices are the same for self STA
  wdiAddSTAParam.bcastDpuIndex = wdiRspParams.usSelfStaDpuId;
  wdiAddSTAParam.bcastMgmtDpuIndex = wdiRspParams.usSelfStaDpuId;
  wdiAddSTAParam.dpuIndex = wdiRspParams.usSelfStaDpuId;;
  wpalMemoryCopy(wdiAddSTAParam.staMacAddr, wdiRspParams.macSelfSta,
                 WDI_MAC_ADDR_LEN);
  wdiAddSTAParam.ucStaType = WDI_STA_ENTRY_SELF; /* 0 - self */
  wdiAddSTAParam.ucSTAIdx = halStartRspMsg.startRspParams.selfStaIdx;

  /* Note: Since we don't get an explicit config STA request for self STA, we
     add the self STA upon receiving the Start response message. But the
     self STA entry in the table is deleted when WDI gets an explicit delete STA
     request */
  (void)WDI_STATableAddSta(pWDICtx,&wdiAddSTAParam);
#endif

  /*Notify UMAC*/
  wdiStartRspCb( &wdiRspParams, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessStartRsp*/


/**
 @brief Process Stop Response function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessStopRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status          wdiStatus;
  WDI_StopRspCb       wdiStopRspCb = NULL;

  tHalMacStopRspMsg   halMacStopRspMsg; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  wdiStopRspCb = (WDI_StopRspCb)pWDICtx->pfncRspCB; 
  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData) ||
      ( NULL == wdiStopRspCb ))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  if ( sizeof(halMacStopRspMsg) < pEventData->uEventDataSize )
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Invalid response length in Stop Resp %x %x",
                pEventData->uEventDataSize);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  /*-------------------------------------------------------------------------
    Unpack HAL Response Message - the header was already extracted by the
    main Response Handling procedure 
  -------------------------------------------------------------------------*/
  wpalMemoryCopy( &halMacStopRspMsg.stopRspParams,  
                  pEventData->pEventData, 
                  sizeof(halMacStopRspMsg.stopRspParams));

  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halMacStopRspMsg.stopRspParams.status); 

  wpalMutexAcquire(&pWDICtx->wptMutex);

  /*--------------------------------------------------------------------------
    Check to see if the stop went OK 
  --------------------------------------------------------------------------*/
  if ( WDI_STATUS_SUCCESS != wdiStatus  )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_ERROR,
               "Failed to stop the device with status %s (%d)",
               WDI_getHALStatusMsgString(halMacStopRspMsg.stopRspParams.status),
               halMacStopRspMsg.stopRspParams.status);

    WDI_DetectedDeviceError( pWDICtx, WDI_ERR_BASIC_OP_FAILURE); 
    
    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_FAILURE; 
  }
  
  pWDICtx->ucExpectedStateTransition = WDI_STOPPED_ST;

  /*Transition now as WDI may get preempted imediately after it sends
  up the Stop Response and it will not get to process the state transition
  from Main Rsp function*/
  WDI_STATE_TRANSITION( pWDICtx, pWDICtx->ucExpectedStateTransition);
  wpalMutexRelease(&pWDICtx->wptMutex);

  /*! TO DO: - STOP the Data transport */

  /*Notify UMAC*/
  wdiStopRspCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessStopRsp*/

/**
 @brief Process Close Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessCloseRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  /*There is no close response comming from HAL - function just kept for
  simmetry */
  WDI_ASSERT(0);
  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessCloseRsp*/


/*============================================================================
                      SCAN RESPONSE PROCESSING API 
============================================================================*/

/**
 @brief Process Init Scan Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessInitScanRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status            wdiStatus;
  WDI_InitScanRspCb     wdiInitScanRspCb;
  tHalInitScanRspMsg    halInitScanRspMsg; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiInitScanRspCb = (WDI_InitScanRspCb)pWDICtx->pfncRspCB;
  if( NULL == wdiInitScanRspCb)
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_FATAL,
                 "%s: call back function is NULL", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  /*-------------------------------------------------------------------------
    Unpack HAL Response Message - the header was already extracted by the
    main Response Handling procedure 
  -------------------------------------------------------------------------*/
  wpalMemoryCopy( &halInitScanRspMsg.initScanRspParams, 
                  pEventData->pEventData, 
                  sizeof(halInitScanRspMsg.initScanRspParams));

  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halInitScanRspMsg.initScanRspParams.status); 

  if ( pWDICtx->bInBmps )
  {
     // notify DTS that we are entering Full power
     WDTS_SetPowerState(pWDICtx, WDTS_POWER_STATE_FULL, NULL);
  }

  /*Notify UMAC*/
  wdiInitScanRspCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessInitScanRsp*/


/**
 @brief Process Start Scan Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessStartScanRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_StartScanRspParamsType   wdiStartScanParams;
  WDI_StartScanRspCb           wdiStartScanRspCb;
  
  tHalStartScanRspMsg          halStartScanRspMsg; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiStartScanRspCb = (WDI_StartScanRspCb)pWDICtx->pfncRspCB;
  if( NULL == wdiStartScanRspCb)
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_FATAL,
                 "%s: call back function is NULL", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  wpalMemoryCopy( &halStartScanRspMsg.startScanRspParams, 
                  pEventData->pEventData, 
                  sizeof(halStartScanRspMsg.startScanRspParams));

  wdiStartScanParams.wdiStatus   =   WDI_HAL_2_WDI_STATUS(
                             halStartScanRspMsg.startScanRspParams.status);
#ifdef WLAN_FEATURE_VOWIFI
  wdiStartScanParams.ucTxMgmtPower = 
                             halStartScanRspMsg.startScanRspParams.txMgmtPower;
  wpalMemoryCopy( wdiStartScanParams.aStartTSF, 
                  halStartScanRspMsg.startScanRspParams.startTSF,
                  2);
#endif 

  if ( eHAL_STATUS_SUCCESS != halStartScanRspMsg.startScanRspParams.status )
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Start scan failed with status %s (%d)",
              WDI_getHALStatusMsgString(halStartScanRspMsg.startScanRspParams.status),
              halStartScanRspMsg.startScanRspParams.status);
     /* send the status to UMAC, don't return from here*/
  }

  /*Notify UMAC*/
  wdiStartScanRspCb( &wdiStartScanParams, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 

}/*WDI_ProcessStartScanRsp*/


/**
 @brief Process End Scan Response function (called when a 
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessEndScanRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status            wdiStatus;
  tHalEndScanRspMsg     halEndScanRspMsg;
  WDI_EndScanRspCb      wdiEndScanRspCb;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiEndScanRspCb = (WDI_EndScanRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  wpalMemoryCopy( &halEndScanRspMsg.endScanRspParams, 
                  pEventData->pEventData, 
                  sizeof(halEndScanRspMsg.endScanRspParams));

  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halEndScanRspMsg.endScanRspParams.status); 

  if ( eHAL_STATUS_SUCCESS != halEndScanRspMsg.endScanRspParams.status )
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_ERROR,
              "End Scan failed with status %s (%d )",
              WDI_getHALStatusMsgString(halEndScanRspMsg.endScanRspParams.status),
              halEndScanRspMsg.endScanRspParams.status);
     /* send the status to UMAC, don't return from here*/
  }

  /*Notify UMAC*/
  wdiEndScanRspCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessEndScanRsp*/


/**
 @brief Process Finish Scan Response function (called when a 
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessFinishScanRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)  
{
  WDI_Status            wdiStatus;
  WDI_FinishScanRspCb   wdiFinishScanRspCb;
  
  tHalFinishScanRspMsg  halFinishScanRspMsg; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiFinishScanRspCb = (WDI_FinishScanRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  wpalMemoryCopy( (void *)&halFinishScanRspMsg.finishScanRspParams.status, 
                  pEventData->pEventData, 
                  sizeof(halFinishScanRspMsg.finishScanRspParams.status));

  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halFinishScanRspMsg.finishScanRspParams.status); 

  WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_INFO_LOW,
              "Finish scan response reported status: %d", 
              halFinishScanRspMsg.finishScanRspParams.status);

  if (( eHAL_STATUS_SUCCESS != halFinishScanRspMsg.finishScanRspParams.status )&&
      ( eHAL_STATUS_NOTIFY_BSS_FAIL  != halFinishScanRspMsg.finishScanRspParams.status ))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Finish Scan failed with status %s (%d)",
              WDI_getHALStatusMsgString(halFinishScanRspMsg.finishScanRspParams.status),
              halFinishScanRspMsg.finishScanRspParams.status);
     /* send the status to UMAC, don't return from here*/
  }

  /*Notify UMAC*/
  wdiFinishScanRspCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessFinishScanRsp*/

/**
 @brief Process Join Response function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessJoinRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status                    wdiStatus;
  WDI_JoinRspCb                 wdiJoinRspCb;
  WDI_BSSSessionType*           pBSSSes             = NULL;
  
  tHalJoinRspMsg                halJoinRspMsg; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) ||
      ( NULL == pWDICtx->pfncRspCB ) ||
      ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiJoinRspCb = (WDI_JoinRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  wpalMemoryCopy( &halJoinRspMsg.joinRspParams, 
                  pEventData->pEventData, 
                  sizeof(halJoinRspMsg.joinRspParams));

  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halJoinRspMsg.joinRspParams.status); 

  wpalMutexAcquire(&pWDICtx->wptMutex);

  /*-----------------------------------------------------------------------
    Join response can only be received for an existing assoc that
    is current and in progress 
    -----------------------------------------------------------------------*/
  if (( !WDI_VALID_SESSION_IDX(pWDICtx->ucCurrentBSSSesIdx )) || 
      ( eWLAN_PAL_FALSE == pWDICtx->bAssociationInProgress ))
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS does not yet exist or "
              "association no longer in progress - mysterious HAL response");

    WDI_DetectedDeviceError( pWDICtx, WDI_ERR_BASIC_OP_FAILURE); 
    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  pBSSSes = &pWDICtx->aBSSSessions[pWDICtx->ucCurrentBSSSesIdx];

  /*-----------------------------------------------------------------------
    Join Response is only allowed in init state 
  -----------------------------------------------------------------------*/
  if ( WDI_ASSOC_JOINING_ST != pBSSSes->wdiAssocState)
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Join only allowed in Joining state - failure state is %d "
              "strange HAL response", pBSSSes->wdiAssocState);

    WDI_DetectedDeviceError( pWDICtx, WDI_ERR_BASIC_OP_FAILURE); 
    
    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_NOT_ALLOWED; 
  }


  /*-----------------------------------------------------------------------
    If assoc has failed the current session will be deleted 
  -----------------------------------------------------------------------*/
  if ( WDI_STATUS_SUCCESS != wdiStatus )
  {
    /*Association was failed by HAL - remove session*/
    WDI_DeleteSession(pWDICtx, pBSSSes);

    /*Association no longer in progress  */
    pWDICtx->bAssociationInProgress = eWLAN_PAL_FALSE;

    /*Association no longer in progress - prepare pending assoc for processing*/
    WDI_DequeueAssocRequest(pWDICtx);
  
  }
  else
  {
    /*Transition to state Joining - this may be redundant as we are supposed
      to be in this state already - but just to be safe*/
    pBSSSes->wdiAssocState = WDI_ASSOC_JOINING_ST; 
  }

  wpalMutexRelease(&pWDICtx->wptMutex);

  /*Notify UMAC*/
  wdiJoinRspCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessJoinRsp*/


/**
 @brief Process Config BSS Response function (called when a 
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessConfigBSSRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_ConfigBSSRspParamsType    wdiConfigBSSParams;
  WDI_ConfigBSSRspCb            wdiConfigBSSRspCb;
  wpt_uint8                     ucCurrentBSSSesIdx  = 0; 
  WDI_BSSSessionType*           pBSSSes             = NULL;

  tConfigBssRspMsg              halConfigBssRspMsg; 
  WDI_AddStaParams              wdiBcastAddSTAParam = {0};
  WDI_AddStaParams              wdiAddSTAParam = {0};
  
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiConfigBSSRspCb = (WDI_ConfigBSSRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC 
  -------------------------------------------------------------------------*/
  wpalMemoryCopy( &halConfigBssRspMsg.configBssRspParams, 
                   pEventData->pEventData, 
                   sizeof(halConfigBssRspMsg.configBssRspParams));

  wdiConfigBSSParams.wdiStatus = WDI_HAL_2_WDI_STATUS(
                            halConfigBssRspMsg.configBssRspParams.status);
  if(WDI_STATUS_SUCCESS == wdiConfigBSSParams.wdiStatus)
  {
    wpalMemoryCopy( wdiConfigBSSParams.macBSSID, 
                    pWDICtx->wdiCachedConfigBssReq.wdiReqInfo.macBSSID,
                    WDI_MAC_ADDR_LEN);
  
    wdiConfigBSSParams.ucBSSIdx = halConfigBssRspMsg.configBssRspParams.bssIdx;
  
    wdiConfigBSSParams.ucBcastSig = 
       halConfigBssRspMsg.configBssRspParams.bcastDpuSignature;
  
    wdiConfigBSSParams.ucUcastSig = 
       halConfigBssRspMsg.configBssRspParams.ucastDpuSignature;
  
    wdiConfigBSSParams.ucSTAIdx = halConfigBssRspMsg.configBssRspParams.bssStaIdx;
  
  #ifdef WLAN_FEATURE_VOWIFI
    wdiConfigBSSParams.ucTxMgmtPower = 
                               halConfigBssRspMsg.configBssRspParams.txMgmtPower;
  #endif
     wpalMemoryCopy( wdiConfigBSSParams.macSTA,
                     halConfigBssRspMsg.configBssRspParams.staMac,
                     WDI_MAC_ADDR_LEN );
  
    wpalMutexAcquire(&pWDICtx->wptMutex);
    /*------------------------------------------------------------------------
      Find the BSS for which the request is made 
    ------------------------------------------------------------------------*/
    ucCurrentBSSSesIdx = WDI_FindAssocSession( pWDICtx, 
                                               wdiConfigBSSParams.macBSSID, 
                                              &pBSSSes); 
  
    /*-----------------------------------------------------------------------
      Config BSS response can only be received for an existing assoc that
      is current and in progress 
      -----------------------------------------------------------------------*/
    if ( NULL == pBSSSes )
    {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                "Association sequence for this BSS does not yet exist "
                "- mysterious HAL response");
  
      WDI_DetectedDeviceError( pWDICtx, WDI_ERR_BASIC_OP_FAILURE); 
      
      wpalMutexRelease(&pWDICtx->wptMutex);
      return WDI_STATUS_E_NOT_ALLOWED; 
    }
  
    /*Save data for this BSS*/
    pBSSSes->wdiBssType = pWDICtx->wdiCachedConfigBssReq.wdiReqInfo.wdiBSSType;
    pBSSSes->ucBSSIdx = halConfigBssRspMsg.configBssRspParams.bssIdx;
    pBSSSes->bcastDpuIndex     = 
      halConfigBssRspMsg.configBssRspParams.bcastDpuDescIndx;
    pBSSSes->bcastDpuSignature = 
      halConfigBssRspMsg.configBssRspParams.bcastDpuSignature;
    pBSSSes->bcastMgmtDpuIndex = 
      halConfigBssRspMsg.configBssRspParams.mgmtDpuDescIndx;
    pBSSSes->bcastMgmtDpuSignature = 
      halConfigBssRspMsg.configBssRspParams.mgmtDpuSignature;
    pBSSSes->ucRmfEnabled      = 
      pWDICtx->wdiCachedConfigBssReq.wdiReqInfo.ucRMFEnabled;
    pBSSSes->bcastStaIdx =
       halConfigBssRspMsg.configBssRspParams.bssBcastStaIdx;
  
    /* !TO DO: Shuould we be updating the RMF Capability of self STA here? */
  
    /*-------------------------------------------------------------------------
        Add Peer STA
      -------------------------------------------------------------------------*/
    wdiAddSTAParam.ucSTAIdx = halConfigBssRspMsg.configBssRspParams.bssStaIdx; 
    wdiAddSTAParam.dpuIndex = halConfigBssRspMsg.configBssRspParams.dpuDescIndx;
    wdiAddSTAParam.dpuSig   = halConfigBssRspMsg.configBssRspParams.ucastDpuSignature;
     
     /*This info can be retrieved from the cached initial request*/
    wdiAddSTAParam.ucWmmEnabled = 
        pWDICtx->wdiCachedConfigBssReq.wdiReqInfo.wdiSTAContext.ucWMMEnabled;
    wdiAddSTAParam.ucHTCapable  = 
        pWDICtx->wdiCachedConfigBssReq.wdiReqInfo.wdiSTAContext.ucHTCapable; 
    wdiAddSTAParam.ucStaType    = 
        pWDICtx->wdiCachedConfigBssReq.wdiReqInfo.wdiSTAContext.wdiSTAType;  
  
     /* MAC Address of STA */
    wpalMemoryCopy(wdiAddSTAParam.staMacAddr, 
                   halConfigBssRspMsg.configBssRspParams.staMac, 
                   WDI_MAC_ADDR_LEN);
  
    wpalMemoryCopy(wdiAddSTAParam.macBSSID, 
                   pWDICtx->wdiCachedConfigBssReq.wdiReqInfo.wdiSTAContext.macBSSID , 
                   WDI_MAC_ADDR_LEN); 
     
    /*Add BSS specific parameters*/
    wdiAddSTAParam.bcastMgmtDpuIndex     = 
        halConfigBssRspMsg.configBssRspParams.mgmtDpuDescIndx;
    wdiAddSTAParam.bcastMgmtDpuSignature = 
        halConfigBssRspMsg.configBssRspParams.mgmtDpuSignature;
    wdiAddSTAParam.bcastDpuIndex         = 
        halConfigBssRspMsg.configBssRspParams.bcastDpuDescIndx;
    wdiAddSTAParam.bcastDpuSignature     = 
        halConfigBssRspMsg.configBssRspParams.bcastDpuSignature;
    wdiAddSTAParam.ucRmfEnabled          =  
        pWDICtx->wdiCachedConfigBssReq.wdiReqInfo.ucRMFEnabled;
    wdiAddSTAParam.ucBSSIdx = 
       halConfigBssRspMsg.configBssRspParams.bssIdx;
  
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
                "Add STA to the table index: %d", wdiAddSTAParam.ucSTAIdx );
  
    WDI_STATableAddSta(pWDICtx,&wdiAddSTAParam);
    /*-------------------------------------------------------------------------
        Add Broadcast STA only in AP mode
      -------------------------------------------------------------------------*/
    if( pWDICtx->wdiCachedConfigBssReq.wdiReqInfo.ucOperMode == 
        WDI_BSS_OPERATIONAL_MODE_AP )
    {
       WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
                  "Add BCAST STA to table for index: %d",
                  halConfigBssRspMsg.configBssRspParams.bssBcastStaIdx );
  
       wpalMemoryCopy( &wdiBcastAddSTAParam, &wdiAddSTAParam, 
                       sizeof(WDI_AddStaParams) );
  
       WDI_AddBcastSTAtoSTATable( pWDICtx, &wdiBcastAddSTAParam,
                                  halConfigBssRspMsg.configBssRspParams.bssBcastStaIdx );
    }
    wpalMutexRelease(&pWDICtx->wptMutex);
  }
  else
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "Config BSS RSP failed with status : %s(%d)",
                  WDI_getHALStatusMsgString(
                  halConfigBssRspMsg.configBssRspParams.status), 
                  halConfigBssRspMsg.configBssRspParams.status);

    
    /*Association was failed by HAL - remove session*/
    WDI_DeleteSession(pWDICtx, pBSSSes);

    /*Association no longer in progress  */
    pWDICtx->bAssociationInProgress = eWLAN_PAL_FALSE;

    /*Association no longer in progress - prepare pending assoc for processing*/
    WDI_DequeueAssocRequest(pWDICtx);

  }

  /*Notify UMAC*/
  wdiConfigBSSRspCb( &wdiConfigBSSParams, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessConfigBSSRsp*/


/**
 @brief Process Del BSS Response function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessDelBSSRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_DelBSSRspParamsType       wdiDelBSSParams;
  WDI_DelBSSRspCb               wdiDelBSSRspCb;
  wpt_uint8                     ucCurrentBSSSesIdx  = 0; 
  WDI_BSSSessionType*           pBSSSes             = NULL;

  tDeleteBssRspMsg              halDelBssRspMsg; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiDelBSSRspCb = (WDI_DelBSSRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC 
  -------------------------------------------------------------------------*/
  wpalMemoryCopy( &halDelBssRspMsg.deleteBssRspParams, 
                  pEventData->pEventData, 
                  sizeof(halDelBssRspMsg.deleteBssRspParams));


  wdiDelBSSParams.wdiStatus   =   WDI_HAL_2_WDI_STATUS(
                                 halDelBssRspMsg.deleteBssRspParams.status); 

  wpalMutexAcquire(&pWDICtx->wptMutex);

  /*------------------------------------------------------------------------
    Find the BSS for which the request is made 
  ------------------------------------------------------------------------*/
  ucCurrentBSSSesIdx = WDI_FindAssocSessionByBSSIdx( pWDICtx, 
                             halDelBssRspMsg.deleteBssRspParams.bssIdx, 
                             &pBSSSes); 

  /*-----------------------------------------------------------------------
    Del BSS response can only be received for an existing assoc that
    is current and in progress 
    -----------------------------------------------------------------------*/
  if ( NULL == pBSSSes )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS does not yet exist or "
              "association no longer in progress - mysterious HAL response");

    WDI_DetectedDeviceError( pWDICtx, WDI_ERR_BASIC_OP_FAILURE); 
    
    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*Extract BSSID for the response to UMAC*/
  wpalMemoryCopy(wdiDelBSSParams.macBSSID, 
                 pBSSSes->macBSSID, WDI_MAC_ADDR_LEN);

  /*-----------------------------------------------------------------------
    The current session will be deleted 
  -----------------------------------------------------------------------*/
  WDI_DeleteSession(pWDICtx, pBSSSes);
 
  /* Delete the BCAST STA entry from the STA table */
  (void)WDI_STATableDelSta( pWDICtx, pBSSSes->bcastStaIdx );

  /* Delete the STA's in this BSS */
  WDI_STATableBSSDelSta(pWDICtx, halDelBssRspMsg.deleteBssRspParams.bssIdx);
  
  wpalMutexRelease(&pWDICtx->wptMutex);

  /*Notify UMAC*/
  wdiDelBSSRspCb( &wdiDelBSSParams, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessDelBSSRsp*/

/**
 @brief Process Post Assoc Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessPostAssocRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_PostAssocRspParamsType    wdiPostAssocParams;
  WDI_PostAssocRspCb            wdiPostAssocRspCb;
  wpt_uint8                     ucCurrentBSSSesIdx     = 0; 
  WDI_BSSSessionType*           pBSSSes                = NULL;
  tPostAssocRspMsg              halPostAssocRspMsg; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiPostAssocRspCb = (WDI_PostAssocRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  wpalMemoryCopy( &halPostAssocRspMsg.postAssocRspParams, 
                   pEventData->pEventData, 
                   sizeof(halPostAssocRspMsg.postAssocRspParams));

  /*Extract the Post Assoc STA Params */

  wdiPostAssocParams.staParams.ucSTAIdx   = 
    halPostAssocRspMsg.postAssocRspParams.configStaRspParams.staIdx;
  wdiPostAssocParams.staParams.ucUcastSig = 
    halPostAssocRspMsg.postAssocRspParams.configStaRspParams.ucUcastSig;
  wdiPostAssocParams.staParams.ucBcastSig = 
    halPostAssocRspMsg.postAssocRspParams.configStaRspParams.ucBcastSig;

 wdiPostAssocParams.wdiStatus = 
    WDI_HAL_2_WDI_STATUS(halPostAssocRspMsg.postAssocRspParams.configStaRspParams.status); 

 /*Copy the MAC addresses from the cached storage in the WDI CB as they are not
   included in the response */
  wpalMemoryCopy( wdiPostAssocParams.staParams.macSTA, 
                  pWDICtx->wdiCachedPostAssocReq.wdiSTAParams.macSTA, 
                  WDI_MAC_ADDR_LEN);

  /* Extract Post Assoc BSS Params */

  wpalMemoryCopy( wdiPostAssocParams.bssParams.macBSSID, 
                  pWDICtx->wdiCachedPostAssocReq.wdiBSSParams.macBSSID, 
                  WDI_MAC_ADDR_LEN); 

  /*Copy the MAC addresses from the cached storage in the WDI CB as they are not
   included in the response */
  wpalMemoryCopy( wdiPostAssocParams.bssParams.macSTA, 
                  pWDICtx->wdiCachedPostAssocReq.wdiBSSParams.wdiSTAContext
                  .macSTA, WDI_MAC_ADDR_LEN);

  wdiPostAssocParams.bssParams.ucBcastSig = 
     halPostAssocRspMsg.postAssocRspParams.configStaRspParams.ucBcastSig;

  wdiPostAssocParams.bssParams.ucUcastSig = 
     halPostAssocRspMsg.postAssocRspParams.configStaRspParams.ucUcastSig;

  wdiPostAssocParams.bssParams.ucBSSIdx =
     halPostAssocRspMsg.postAssocRspParams.configBssRspParams.bssIdx;

  wdiPostAssocParams.bssParams.ucSTAIdx = 
     halPostAssocRspMsg.postAssocRspParams.configBssRspParams.bssStaIdx;

  wpalMutexAcquire(&pWDICtx->wptMutex);

  /*------------------------------------------------------------------------
    Find the BSS for which the request is made 
  ------------------------------------------------------------------------*/
  ucCurrentBSSSesIdx = WDI_FindAssocSession( pWDICtx, 
                                             wdiPostAssocParams.bssParams.
                                             macBSSID, &pBSSSes); 

  /*-----------------------------------------------------------------------
    Post assoc response can only be received for an existing assoc that
    is current and in progress 
    -----------------------------------------------------------------------*/
  if (( NULL == pBSSSes ) ||
      ( ucCurrentBSSSesIdx != pWDICtx->ucCurrentBSSSesIdx ) || 
      ( eWLAN_PAL_FALSE == pWDICtx->bAssociationInProgress ))
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Association sequence for this BSS does not yet exist or "
              "association no longer in progress - mysterious HAL response");

    WDI_DetectedDeviceError( pWDICtx, WDI_ERR_BASIC_OP_FAILURE); 
    
    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*-----------------------------------------------------------------------
    Post Assoc Request is only allowed in Joining state 
  -----------------------------------------------------------------------*/
  if ( WDI_ASSOC_JOINING_ST != pBSSSes->wdiAssocState)
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Post Assoc not allowed before JOIN - failing request "
              "strange HAL response");

    WDI_DetectedDeviceError( pWDICtx, WDI_ERR_BASIC_OP_FAILURE); 
    
    wpalMutexRelease(&pWDICtx->wptMutex);
    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  /*-----------------------------------------------------------------------
    If assoc has failed the current session will be deleted 
  -----------------------------------------------------------------------*/
  if ( WDI_STATUS_SUCCESS != wdiPostAssocParams.wdiStatus )
  {
    /*Association was failed by HAL - remove session*/
    WDI_DeleteSession(pWDICtx, pBSSSes);
  }
  else
  {
    /*Transition to state POST Assoc*/
    pBSSSes->wdiAssocState = WDI_ASSOC_POST_ST; 

    /*Save DPU Info*/
    pBSSSes->bcastMgmtDpuIndex     = 
      halPostAssocRspMsg.postAssocRspParams.configBssRspParams.mgmtDpuDescIndx;
    pBSSSes->bcastMgmtDpuSignature = 
      halPostAssocRspMsg.postAssocRspParams.configBssRspParams.mgmtDpuSignature;
    pBSSSes->bcastDpuIndex         = 
      halPostAssocRspMsg.postAssocRspParams.configBssRspParams.bcastDpuDescIndx;
    pBSSSes->bcastDpuSignature     = 
      halPostAssocRspMsg.postAssocRspParams.configBssRspParams.bcastDpuSignature;

    pBSSSes->ucBSSIdx              = 
      halPostAssocRspMsg.postAssocRspParams.configBssRspParams.bssIdx;
  }

  /*Association no longer in progress  */
  pWDICtx->bAssociationInProgress = eWLAN_PAL_FALSE;

  /*Association no longer in progress - prepare pending assoc for processing*/
  WDI_DequeueAssocRequest(pWDICtx);

  wpalMutexRelease(&pWDICtx->wptMutex);

  /*Notify UMAC*/
  wdiPostAssocRspCb( &wdiPostAssocParams, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessPostAssocRsp*/

/**
 @brief Process Del STA Rsp function (called when a response is 
        being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessDelSTARsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_DelSTARspParamsType   wdiDelSTARsp;
  WDI_DelSTARspCb           wdiDelSTARspCb;
  wpt_uint8                 staType;
  tDeleteStaRspMsg          halDelStaRspMsg; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiDelSTARspCb = (WDI_DelSTARspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  wpalMemoryCopy( &halDelStaRspMsg.delStaRspParams,
                  pEventData->pEventData, 
                  sizeof(halDelStaRspMsg.delStaRspParams));

  wdiDelSTARsp.ucSTAIdx    = halDelStaRspMsg.delStaRspParams.staId;
  wdiDelSTARsp.wdiStatus   =   
    WDI_HAL_2_WDI_STATUS(halDelStaRspMsg.delStaRspParams.status); 

  WDI_STATableGetStaType(pWDICtx, wdiDelSTARsp.ucSTAIdx, &staType);

  /* If the DEL STA request is for self STA do not delete it - Really weird!!What happens in concurrency */
  if(staType == WDI_STA_ENTRY_SELF)
  {
    WDI_StaStruct* pSTATable = (WDI_StaStruct*) pWDICtx->staTable;

    /* At this point add the self-STA */

    /*! TO DO: wdiAddSTAParam.ucWmmEnabled */
    /*! TO DO: wdiAddSTAParam.ucHTCapable */
    /*! TO DO: wdiAddSTAParam.ucRmfEnabled */

#define WDI_DPU_SELF_STA_DEFAULT_IDX 0
#define WDI_DPU_SELF_STA_DEFAULT_SIG 0

    //all DPU indices are the same for self STA
    pSTATable[wdiDelSTARsp.ucSTAIdx].dpuIndex = WDI_DPU_SELF_STA_DEFAULT_IDX;
    pSTATable[wdiDelSTARsp.ucSTAIdx].bcastDpuIndex = WDI_DPU_SELF_STA_DEFAULT_IDX;
    pSTATable[wdiDelSTARsp.ucSTAIdx].bcastMgmtDpuIndex = WDI_DPU_SELF_STA_DEFAULT_IDX;
    pSTATable[wdiDelSTARsp.ucSTAIdx].bcastDpuSignature = WDI_DPU_SELF_STA_DEFAULT_SIG;
    pSTATable[wdiDelSTARsp.ucSTAIdx].bcastMgmtDpuSignature = WDI_DPU_SELF_STA_DEFAULT_SIG;
    pSTATable[wdiDelSTARsp.ucSTAIdx].dpuSig = WDI_DPU_SELF_STA_DEFAULT_SIG;
  }
  else
  {
    //Delete the station in the table
    WDI_STATableDelSta( pWDICtx, wdiDelSTARsp.ucSTAIdx);
  }

  /*Notify UMAC*/
  wdiDelSTARspCb( &wdiDelSTARsp, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessDelSTARsp*/


/*==========================================================================
                   Security Response Processing Functions 
==========================================================================*/

/**
 @brief Process Set BSS Key Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetBssKeyRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status            wdiStatus;
  eHalStatus            halStatus;
  WDI_SetBSSKeyRspCb    wdiSetBSSKeyRspCb;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiSetBSSKeyRspCb = (WDI_SetBSSKeyRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  halStatus = *((eHalStatus*)pEventData->pEventData);
  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

  if ( eHAL_STATUS_SUCCESS != halStatus )
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Set BSS Key failed with status %s (%d)",
              WDI_getHALStatusMsgString(halStatus),
              halStatus);
     /* send the status to UMAC, don't return from here*/
  }

  /*Notify UMAC*/
  wdiSetBSSKeyRspCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessSetBssKeyRsp*/

/**
 @brief Process Remove BSS Key Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessRemoveBssKeyRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status              wdiStatus;
  eHalStatus              halStatus;
  WDI_RemoveBSSKeyRspCb   wdiRemoveBSSKeyRspCb;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiRemoveBSSKeyRspCb = (WDI_RemoveBSSKeyRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  halStatus = *((eHalStatus*)pEventData->pEventData);
  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

  if ( eHAL_STATUS_SUCCESS != halStatus )
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Remove BSS Key failed with status %s (%d )",
              WDI_getHALStatusMsgString(halStatus),
              halStatus);
     /* send the status to UMAC, don't return from here*/
  }

  /*Notify UMAC*/
  wdiRemoveBSSKeyRspCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessSetBssKeyRsp*/


/**
 @brief Process Set STA Key Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetStaKeyRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status            wdiStatus;
  eHalStatus            halStatus;
  WDI_SetSTAKeyRspCb    wdiSetSTAKeyRspCb;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiSetSTAKeyRspCb = (WDI_SetSTAKeyRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  halStatus = *((eHalStatus*)pEventData->pEventData);
  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

  if ( eHAL_STATUS_SUCCESS != halStatus )
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Set STA Key failed with status %s (%d)",
              WDI_getHALStatusMsgString(halStatus),
              halStatus);
     /* send the status to UMAC, don't return from here*/
  }

  /*Notify UMAC*/
  wdiSetSTAKeyRspCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessSetSTAKeyRsp*/

/**
 @brief Process Remove STA Key Rsp function (called when a 
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessRemoveStaKeyRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status              wdiStatus;
  eHalStatus              halStatus;
  WDI_RemoveSTAKeyRspCb   wdiRemoveSTAKeyRspCb;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiRemoveSTAKeyRspCb = (WDI_RemoveSTAKeyRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  halStatus = *((eHalStatus*)pEventData->pEventData);
  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

  if ( eHAL_STATUS_SUCCESS != halStatus )
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Remove STA Key failed with status %s (%d)",
              WDI_getHALStatusMsgString(halStatus),
              halStatus);
     /* send the status to UMAC, don't return from here*/
  }

  /*Notify UMAC*/
  wdiRemoveSTAKeyRspCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessRemoveStaKeyRsp*/

/**
 @brief Process Set STA Bcast Key Rsp function (called when a 
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetStaBcastKeyRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status            wdiStatus;
  eHalStatus            halStatus;
  WDI_SetSTAKeyRspCb    wdiSetSTABcastKeyRspCb;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiSetSTABcastKeyRspCb = (WDI_SetSTAKeyRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  wpalMemoryCopy( &halStatus, 
                  pEventData->pEventData, 
                  sizeof(halStatus));

  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

  if ( eHAL_STATUS_SUCCESS != halStatus )
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Set STA Key failed with status %s (%d)",
              WDI_getHALStatusMsgString(halStatus),
              halStatus);
     /* send the status to UMAC, don't return from here*/
  }

  /*Notify UMAC*/
  wdiSetSTABcastKeyRspCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessSetSTABcastKeyRsp*/

/**
 @brief Process Remove STA Bcast Key Rsp function (called when a
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessRemoveStaBcastKeyRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status              wdiStatus;
  eHalStatus              halStatus;
  WDI_RemoveSTAKeyRspCb   wdiRemoveSTABcastKeyRspCb;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiRemoveSTABcastKeyRspCb = (WDI_RemoveSTAKeyRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  wpalMemoryCopy( &halStatus, 
                  pEventData->pEventData, 
                  sizeof(halStatus));

  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

  if ( eHAL_STATUS_SUCCESS != halStatus )
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Remove STA Key failed with status %s (%d)",
              WDI_getHALStatusMsgString(halStatus),
              halStatus);
     /* send the status to UMAC, don't return from here*/
  }

  /*Notify UMAC*/
  wdiRemoveSTABcastKeyRspCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessRemoveStaBcastKeyRsp*/


/*==========================================================================
                   QoS and BA Response Processing Functions 
==========================================================================*/

/**
 @brief Process Add TSpec Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessAddTSpecRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status       wdiStatus;
  eHalStatus       halStatus;
  WDI_AddTsRspCb   wdiAddTsRspCb;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiAddTsRspCb = (WDI_AddTsRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  halStatus = *((eHalStatus*)pEventData->pEventData);
  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

  /*Notify UMAC*/
  wdiAddTsRspCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessAddTSpecRsp*/


/**
 @brief Process Del TSpec Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessDelTSpecRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status       wdiStatus;
  eHalStatus       halStatus;
  WDI_DelTsRspCb   wdiDelTsRspCb;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiDelTsRspCb = (WDI_DelTsRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  halStatus = *((eHalStatus*)pEventData->pEventData);
  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

  /*Notify UMAC*/
  wdiDelTsRspCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessDelTSpecRsp*/

/**
 @brief Process Update EDCA Parameters Rsp function (called when a  
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessUpdateEDCAParamsRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status       wdiStatus;
  eHalStatus       halStatus;
  WDI_UpdateEDCAParamsRspCb   wdiUpdateEDCAParamsRspCb;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiUpdateEDCAParamsRspCb = (WDI_UpdateEDCAParamsRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  halStatus = *((eHalStatus*)pEventData->pEventData);
  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

  /*Notify UMAC*/
  wdiUpdateEDCAParamsRspCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessUpdateEDCAParamsRsp*/


/**
 @brief Process Add BA Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessAddBASessionRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_AddBASessionRspCb   wdiAddBASessionRspCb;

  tAddBASessionRspParams        halBASessionRsp;
  WDI_AddBASessionRspParamsType wdiBASessionRsp;

  
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiAddBASessionRspCb = (WDI_AddBASessionRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  wpalMemoryCopy( &halBASessionRsp, 
                  pEventData->pEventData, 
                  sizeof(halBASessionRsp));

  wdiBASessionRsp.wdiStatus = WDI_HAL_2_WDI_STATUS(halBASessionRsp.status);

  if ( WDI_STATUS_SUCCESS == wdiBASessionRsp.wdiStatus )
  {
    wdiBASessionRsp.ucBaDialogToken = halBASessionRsp.baDialogToken;
    wdiBASessionRsp.ucBaTID = halBASessionRsp.baTID;
    wdiBASessionRsp.ucBaBufferSize = halBASessionRsp.baBufferSize;
    wdiBASessionRsp.usBaSessionID = halBASessionRsp.baSessionID;
    wdiBASessionRsp.ucWinSize = halBASessionRsp.winSize;
    wdiBASessionRsp.ucSTAIdx = halBASessionRsp.STAID;
    wdiBASessionRsp.usBaSSN = halBASessionRsp.SSN;
  }

  /*Notify UMAC*/
  wdiAddBASessionRspCb( &wdiBASessionRsp, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessAddSessionBARsp*/


/**
 @brief Process Del BA Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessDelBARsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status       wdiStatus;
  eHalStatus       halStatus;
  WDI_DelBARspCb   wdiDelBARspCb;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiDelBARspCb = (WDI_DelBARspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  halStatus = *((eHalStatus*)pEventData->pEventData);
  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

  if ( eHAL_STATUS_SUCCESS == halStatus )
  {
    /*! TO DO: I should notify the DAL Data Path that the BA was deleted*/
  }

  /*Notify UMAC*/
  wdiDelBARspCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessDelBARsp*/

/**
 @brief Process Flush AC Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessFlushAcRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status       wdiStatus;
  eHalStatus       halStatus;
  WDI_FlushAcRspCb wdiFlushAcRspCb;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiFlushAcRspCb = (WDI_FlushAcRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  wpalMemoryCopy( &halStatus, 
                  pEventData->pEventData, 
                  sizeof(halStatus));

  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

  /*Notify UMAC*/
  wdiFlushAcRspCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessFlushAcRsp*/

/**
 @brief Process BT AMP event Rsp function (called when a 
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessBtAmpEventRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_Status       wdiStatus;
   eHalStatus       halStatus;
   WDI_BtAmpEventRspCb wdiBtAmpEventRspCb;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
       ( NULL == pEventData->pEventData))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   wdiBtAmpEventRspCb = (WDI_BtAmpEventRspCb)pWDICtx->pfncRspCB;

   /*-------------------------------------------------------------------------
     Extract response and send it to UMAC
   -------------------------------------------------------------------------*/
   wpalMemoryCopy( &halStatus, 
                   pEventData->pEventData, 
                   sizeof(halStatus));

   wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

   /*Notify UMAC*/
   wdiBtAmpEventRspCb( wdiStatus, pWDICtx->pRspCBUserData);

   return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessBtAmpEventRsp*/


/**
 @brief Process ADD STA SELF Rsp function (called 
        when a response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessAddSTASelfRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_AddSTASelfRspParamsType  wdiAddSTASelfParams;
  WDI_AddSTASelfParamsRspCb    wdiAddSTASelfReqParamsRspCb;
  tAddStaSelfRspMsg            halAddStaSelfRsp;
  WDI_AddStaParams             wdiAddSTAParam = {0};
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiAddSTASelfReqParamsRspCb = 
                         (WDI_AddSTASelfParamsRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  wpalMemoryCopy( &halAddStaSelfRsp.addStaSelfRspParams, 
                  pEventData->pEventData, 
                  sizeof(halAddStaSelfRsp.addStaSelfRspParams));


  wdiAddSTASelfParams.wdiStatus   =   
    WDI_HAL_2_WDI_STATUS(halAddStaSelfRsp.addStaSelfRspParams.status); 

  wdiAddSTASelfParams.ucSTASelfIdx   = 
    halAddStaSelfRsp.addStaSelfRspParams.selfStaIdx;
  wdiAddSTASelfParams.dpuIdx = 
    halAddStaSelfRsp.addStaSelfRspParams.dpuIdx;
  wdiAddSTASelfParams.dpuSignature = 
    halAddStaSelfRsp.addStaSelfRspParams.dpuSignature;

  wpalMemoryCopy(wdiAddSTASelfParams.macSelfSta,
                 pWDICtx->wdiCacheAddSTASelfReq.wdiAddSTASelfInfo.selfMacAddr,
                 WDI_MAC_ADDR_LEN);


#ifdef HAL_SELF_STA_PER_BSS

  /* At this point add the self-STA */

  /*! TO DO: wdiAddSTAParam.ucWmmEnabled */
  /*! TO DO: wdiAddSTAParam.ucHTCapable */
  /*! TO DO: wdiAddSTAParam.ucRmfEnabled */

  //all DPU indices are the same for self STA

  /*DPU Information*/
  wdiAddSTAParam.dpuIndex              = wdiAddSTASelfParams.dpuIdx; 
  wdiAddSTAParam.dpuSig                = wdiAddSTASelfParams.dpuSignature;
  wdiAddSTAParam.bcastDpuSignature     = wdiAddSTASelfParams.dpuSignature;
  wdiAddSTAParam.bcastMgmtDpuSignature = wdiAddSTASelfParams.dpuSignature;
  wdiAddSTAParam.bcastDpuIndex         = wdiAddSTASelfParams.dpuIdx;
  wdiAddSTAParam.bcastMgmtDpuIndex     = wdiAddSTASelfParams.dpuIdx;

  wpalMemoryCopy(wdiAddSTAParam.staMacAddr, wdiAddSTASelfParams.macSelfSta,
                 WDI_MAC_ADDR_LEN);

  wdiAddSTAParam.ucStaType = WDI_STA_ENTRY_SELF; /* 0 - self */
  wdiAddSTAParam.ucSTAIdx = wdiAddSTASelfParams.ucSTASelfIdx;

  if(halAddStaSelfRsp.addStaSelfRspParams.status 
     != eHAL_STATUS_ADD_STA_SELF_IGNORED_REF_COUNT_NOT_ZERO)
  {
     (void)WDI_STATableAddSta(pWDICtx,&wdiAddSTAParam);
  }
#endif

  /*Notify UMAC*/
  wdiAddSTASelfReqParamsRspCb( &wdiAddSTASelfParams, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessAddSTASelfRsp*/



/**
 @brief WDI_ProcessDelSTASelfRsp function (called when a 
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessDelSTASelfRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_DelSTASelfRspParamsType     wdiDelStaSelfRspParams;
  WDI_DelSTASelfRspCb             wdiDelStaSelfRspCb;
  tDelStaSelfRspParams            delStaSelfRspParams;
  wpt_uint8                       ucStaIdx;

  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
    -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                "%s: Invalid parameters", __FUNCTION__);
    WDI_ASSERT(0);
    return WDI_STATUS_E_FAILURE; 
  }

  wdiDelStaSelfRspCb = (WDI_DelSTASelfRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
    -------------------------------------------------------------------------*/

  wpalMemoryCopy( &delStaSelfRspParams, 
                        (wpt_uint8*)pEventData->pEventData,
                              sizeof(tDelStaSelfRspParams));

  wdiDelStaSelfRspParams.wdiStatus   =   
    WDI_HAL_2_WDI_STATUS(delStaSelfRspParams.status); 

  /* delStaSelfRspParams.status is not 
   eHAL_STATUS_DEL_STA_SELF_IGNORED_REF_COUNT_NOT_ZERO*/
  if( eHAL_STATUS_SUCCESS == delStaSelfRspParams.status )
  {
    WDI_Status wdiStatus;
    wdiStatus = WDI_STATableFindStaidByAddr(pWDICtx, 
                               delStaSelfRspParams.selfMacAddr,
                               &ucStaIdx);
    if(WDI_STATUS_E_FAILURE == wdiStatus)
    {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "%s: Unable to extract the STA Idx ", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
    }
    WDI_STATableDelSta(pWDICtx, ucStaIdx);
  }

  /*Notify UMAC*/
  wdiDelStaSelfRspCb(&wdiDelStaSelfRspParams, (void*) pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS;
}



/*===========================================================================
           Miscellaneous Control Response Processing API 
===========================================================================*/

/**
 @brief Process Channel Switch Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessChannelSwitchRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_SwitchCHRspParamsType   wdiSwitchChRsp;
  WDI_SwitchChRspCb           wdiChSwitchRspCb;
  tSwitchChannelRspParams     halSwitchChannelRsp;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiChSwitchRspCb = (WDI_SwitchChRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  wpalMemoryCopy( &halSwitchChannelRsp, 
                  (wpt_uint8*)pEventData->pEventData,
                  sizeof(halSwitchChannelRsp));

  wdiSwitchChRsp.wdiStatus   =  
               WDI_HAL_2_WDI_STATUS(halSwitchChannelRsp.status); 
  wdiSwitchChRsp.ucChannel = halSwitchChannelRsp.channelNumber;

#ifdef WLAN_FEATURE_VOWIFI
  wdiSwitchChRsp.ucTxMgmtPower =  halSwitchChannelRsp.txMgmtPower; 
#endif

  /*Notify UMAC*/
  wdiChSwitchRspCb( &wdiSwitchChRsp, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessChannelSwitchRsp*/


/**
 @brief Process Config STA Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessConfigStaRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_ConfigSTARspParamsType    wdiCfgSTAParams;
  WDI_ConfigSTARspCb            wdiConfigSTARspCb;
  WDI_AddStaParams              wdiAddSTAParam;

  WDI_BSSSessionType*           pBSSSes             = NULL;
  wpt_uint8                     ucCurrentBSSSesIdx  = 0; 

  tConfigStaRspMsg              halConfigStaRsp; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiConfigSTARspCb = (WDI_ConfigSTARspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  wpalMemoryCopy( &halConfigStaRsp.configStaRspParams, 
                  pEventData->pEventData, 
                  sizeof(halConfigStaRsp.configStaRspParams));


  wdiCfgSTAParams.ucSTAIdx    = halConfigStaRsp.configStaRspParams.staIdx;
  wdiCfgSTAParams.ucBssIdx    = halConfigStaRsp.configStaRspParams.bssIdx;
  wdiCfgSTAParams.ucUcastSig  = halConfigStaRsp.configStaRspParams.ucUcastSig;
  wdiCfgSTAParams.ucBcastSig  = halConfigStaRsp.configStaRspParams.ucBcastSig;
  wdiCfgSTAParams.ucMgmtSig   = halConfigStaRsp.configStaRspParams.ucMgmtSig;

   /* MAC Address of STA - take from cache as it does not come back in the
   response*/
   wpalMemoryCopy( wdiCfgSTAParams.macSTA,
          pWDICtx->wdiCachedConfigStaReq.wdiReqInfo.macSTA, 
          WDI_MAC_ADDR_LEN);
  
  wdiCfgSTAParams.wdiStatus   =   
    WDI_HAL_2_WDI_STATUS(halConfigStaRsp.configStaRspParams.status); 

  wdiCfgSTAParams.ucDpuIndex = halConfigStaRsp.configStaRspParams.dpuIndex;
  wdiCfgSTAParams.ucBcastDpuIndex = halConfigStaRsp.configStaRspParams.bcastDpuIndex;
  wdiCfgSTAParams.ucBcastMgmtDpuIdx = halConfigStaRsp.configStaRspParams.bcastMgmtDpuIdx;

  if ( WDI_STATUS_SUCCESS == wdiCfgSTAParams.wdiStatus )
  {
    if ( WDI_ADD_STA == pWDICtx->wdiCachedConfigStaReq.wdiReqInfo.wdiAction )
    {
      /* ADD STA to table */
      wdiAddSTAParam.ucSTAIdx = halConfigStaRsp.configStaRspParams.staIdx; 
      wdiAddSTAParam.dpuSig   = halConfigStaRsp.configStaRspParams.ucUcastSig;
      wdiAddSTAParam.dpuIndex = halConfigStaRsp.configStaRspParams.dpuIndex;
       
      /*This info can be retrieved from the cached initial request*/
      wdiAddSTAParam.ucWmmEnabled = 
        pWDICtx->wdiCachedConfigStaReq.wdiReqInfo.ucWMMEnabled;
      wdiAddSTAParam.ucHTCapable  = 
        pWDICtx->wdiCachedConfigStaReq.wdiReqInfo.ucHTCapable; 
      wdiAddSTAParam.ucStaType    = 
        pWDICtx->wdiCachedConfigStaReq.wdiReqInfo.wdiSTAType;  
   
      /* MAC Address of STA */
      wpalMemoryCopy(wdiAddSTAParam.staMacAddr, 
                     pWDICtx->wdiCachedConfigStaReq.wdiReqInfo.macSTA, 
                     WDI_MAC_ADDR_LEN);
  
      wpalMemoryCopy(wdiAddSTAParam.macBSSID, 
                     pWDICtx->wdiCachedConfigStaReq.wdiReqInfo.macBSSID , 
                     WDI_MAC_ADDR_LEN); 
  
      ucCurrentBSSSesIdx = WDI_FindAssocSession( pWDICtx, 
                    pWDICtx->wdiCachedConfigStaReq.wdiReqInfo.macBSSID, 
                    &pBSSSes);  

      if ( NULL == pBSSSes )
      {
        WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                  "Association for this BSSID is not in place");
    
        WDI_ASSERT(0);
        return WDI_STATUS_E_NOT_ALLOWED; 
      }

      /*Add BSS specific parameters*/
      wdiAddSTAParam.bcastMgmtDpuIndex = 
         halConfigStaRsp.configStaRspParams.bcastMgmtDpuIdx;
      wdiAddSTAParam.bcastMgmtDpuSignature = 
         halConfigStaRsp.configStaRspParams.ucMgmtSig;
      wdiAddSTAParam.bcastDpuIndex  = 
         halConfigStaRsp.configStaRspParams.bcastDpuIndex;
      wdiAddSTAParam.bcastDpuSignature = 
         halConfigStaRsp.configStaRspParams.ucBcastSig;
      wdiAddSTAParam.ucRmfEnabled          = pBSSSes->ucRmfEnabled;
      wdiAddSTAParam.ucBSSIdx              = ucCurrentBSSSesIdx;
      
      WDI_STATableAddSta(pWDICtx,&wdiAddSTAParam);
    }
    if( WDI_UPDATE_STA == pWDICtx->wdiCachedConfigStaReq.wdiReqInfo.wdiAction )
    {
       WDI_StaStruct* pSTATable = (WDI_StaStruct*) pWDICtx->staTable;

       pSTATable[halConfigStaRsp.configStaRspParams.staIdx].bcastDpuIndex = 
          halConfigStaRsp.configStaRspParams.bcastDpuIndex;
       pSTATable[halConfigStaRsp.configStaRspParams.staIdx].bcastDpuSignature = 
          halConfigStaRsp.configStaRspParams.ucBcastSig;
       pSTATable[halConfigStaRsp.configStaRspParams.staIdx].bcastMgmtDpuIndex = 
          halConfigStaRsp.configStaRspParams.bcastMgmtDpuIdx;
       pSTATable[halConfigStaRsp.configStaRspParams.staIdx].bcastMgmtDpuSignature = 
          halConfigStaRsp.configStaRspParams.ucMgmtSig;
       pSTATable[halConfigStaRsp.configStaRspParams.staIdx].bssIdx = 
          halConfigStaRsp.configStaRspParams.bssIdx;
       pSTATable[halConfigStaRsp.configStaRspParams.staIdx].dpuIndex = 
          halConfigStaRsp.configStaRspParams.dpuIndex;
       pSTATable[halConfigStaRsp.configStaRspParams.staIdx].dpuSig = 
          halConfigStaRsp.configStaRspParams.ucUcastSig;
    }
  }

  /*Notify UMAC*/
  wdiConfigSTARspCb( &wdiCfgSTAParams, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessConfigStaRsp*/


/**
 @brief Process Set Link State Rsp function (called when a 
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetLinkStateRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status              wdiStatus;
  eHalStatus              halStatus;
  WDI_SetLinkStateRspCb   wdiSetLinkStateRspCb;

  WDI_BSSSessionType*     pBSSSes              = NULL;
  wpt_uint8               ucCurrentBSSSesIdx   = 0; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiSetLinkStateRspCb = (WDI_SetLinkStateRspCb)pWDICtx->pfncRspCB;

  wpalMutexAcquire(&pWDICtx->wptMutex);

  /*If the link is being transitioned to idle - the BSS is to be deleted
  - this type of ending a session is possible when UMAC has failed an
  - association session during Join*/
  if ( WDI_LINK_IDLE_STATE == 
       pWDICtx->wdiCacheSetLinkStReq.wdiLinkInfo.wdiLinkState )
  {
    /*------------------------------------------------------------------------
      Find the BSS for which the request is made 
    ------------------------------------------------------------------------*/
    ucCurrentBSSSesIdx = WDI_FindAssocSession( pWDICtx, 
                        pWDICtx->wdiCacheSetLinkStReq.wdiLinkInfo.macBSSID, 
                        &pBSSSes); 
  
    /*-----------------------------------------------------------------------
      Del BSS response can only be received for an existing assoc that
      is current and in progress 
      -----------------------------------------------------------------------*/
    if ( NULL == pBSSSes )
    {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
                "Set link response received outside association session");
    }
    else
    {
      /* For BT AMP roles no need to delete the sessions if assoc fails. There
      will be del BSS coming after this to stop the beaconing & cleaning up the
      sessions*/
      if(( WDI_BTAMP_STA_MODE != pBSSSes->wdiBssType )&&
         ( WDI_BTAMP_AP_MODE != pBSSSes->wdiBssType ))
      {
         /*-----------------------------------------------------------------------
           The current session will be deleted 
         -----------------------------------------------------------------------*/
         WDI_DeleteSession(pWDICtx, pBSSSes);

         /*-----------------------------------------------------------------------
           Check to see if this association is in progress - if so disable the
           flag as this has ended
         -----------------------------------------------------------------------*/
         if ( ucCurrentBSSSesIdx == pWDICtx->ucCurrentBSSSesIdx )
         {  
           /*Association no longer in progress  */
           pWDICtx->bAssociationInProgress = eWLAN_PAL_FALSE;
           /*Association no longer in progress - prepare pending assoc for processing*/
           WDI_DequeueAssocRequest(pWDICtx);
         }
      }
    }
  }
  /* If the link state has been set to POST ASSOC, reset the "association in
     progress" flag */
  if ( WDI_LINK_POSTASSOC_STATE == 
       pWDICtx->wdiCacheSetLinkStReq.wdiLinkInfo.wdiLinkState )
  {
     pWDICtx->bAssociationInProgress = eWLAN_PAL_FALSE;
     WDI_DequeueAssocRequest(pWDICtx);
  }

  wpalMutexRelease(&pWDICtx->wptMutex);

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  wpalMemoryCopy( &halStatus, 
                  pEventData->pEventData, 
                  sizeof(halStatus));

  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

  /*Notify UMAC*/
  wdiSetLinkStateRspCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessSetLinkStateRsp*/

/**
 @brief Process Get Stats Rsp function (called when a response is   
        being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessGetStatsRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_GetStatsRspParamsType   *wdiGetStatsRsp;
  WDI_GetStatsRspCb           wdiGetStatsRspCb;
  tHalStatsRspParams*         pHalStatsRspParams;
  
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  pHalStatsRspParams = (tHalStatsRspParams *)pEventData->pEventData;

  /*allocate the stats response buffer */
  wdiGetStatsRsp = (WDI_GetStatsRspParamsType *)wpalMemoryAllocate(
              pHalStatsRspParams->msgLen - sizeof(tHalStatsRspParams)
              + sizeof(WDI_GetStatsRspParamsType));

  if(NULL == wdiGetStatsRsp)
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_ERROR,
                "Failed to allocate memory in Get Stats Response %x %x %x ",
                 pWDICtx, pEventData, pEventData->pEventData);
    WDI_ASSERT(0);
    return WDI_STATUS_E_FAILURE; 
  }

  wdiGetStatsRspCb = (WDI_GetStatsRspCb)pWDICtx->pfncRspCB;

  wpalMemoryZero(wdiGetStatsRsp, pHalStatsRspParams->msgLen);
  wdiGetStatsRsp->usMsgType  = pHalStatsRspParams->msgType;
  wdiGetStatsRsp->usMsgLen   = pHalStatsRspParams->msgLen;
  wdiGetStatsRsp->wdiStatus  = WDI_HAL_2_WDI_STATUS(pHalStatsRspParams->status);
  wdiGetStatsRsp->ucSTAIdx   = pHalStatsRspParams->staId;
  wdiGetStatsRsp->uStatsMask = pHalStatsRspParams->statsMask;

  /* copy the stats from buffer at the end of the tHalStatsRspParams message */
  wpalMemoryCopy(wdiGetStatsRsp + 1,
   (wpt_uint8*)pEventData->pEventData + sizeof(tHalStatsRspParams),
   pHalStatsRspParams->msgLen - sizeof(tHalStatsRspParams));

  /*Notify UMAC*/
  wdiGetStatsRspCb( wdiGetStatsRsp, pWDICtx->pRspCBUserData);

  wpalMemoryFree(wdiGetStatsRsp);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessGetStatsRsp*/


/**
 @brief Process Update Cfg Rsp function (called when a response is  
        being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessUpdateCfgRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status           wdiStatus;
  eHalStatus           halStatus;
  WDI_UpdateCfgRspCb   wdiUpdateCfgRspCb;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiUpdateCfgRspCb = (WDI_UpdateCfgRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  halStatus = *((eHalStatus*)pEventData->pEventData);
  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

  /*Notify UMAC*/
  wdiUpdateCfgRspCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessUpdateCfgRsp*/



/**
 @brief Process Add BA Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessAddBARsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_AddBARspCb          wdiAddBARspCb;

  tAddBARspParams         halAddBARsp;
  WDI_AddBARspinfoType    wdiAddBARsp;

  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiAddBARspCb = (WDI_AddBARspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  wpalMemoryCopy( &halAddBARsp, 
                  pEventData->pEventData, 
                  sizeof(halAddBARsp));

  wdiAddBARsp.wdiStatus = WDI_HAL_2_WDI_STATUS(halAddBARsp.status);

  if ( WDI_STATUS_SUCCESS == wdiAddBARsp.wdiStatus )
  {
    wdiAddBARsp.ucBaDialogToken = halAddBARsp.baDialogToken;
  }

  /*Notify UMAC*/
  wdiAddBARspCb( &wdiAddBARsp, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessAddSessionBARsp*/

/**
 @brief Process Add BA Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessTriggerBARsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_TriggerBARspCb      wdiTriggerBARspCb;

  tTriggerBARspParams*           halTriggerBARsp;
  tTriggerBaRspCandidate*        halBaCandidate;
  WDI_TriggerBARspParamsType*    wdiTriggerBARsp;
  WDI_TriggerBARspCandidateType* wdiTriggerBARspCandidate;
  wpt_uint16                     index;
  wpt_uint16                     TidIndex;
  
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiTriggerBARspCb = (WDI_TriggerBARspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  halTriggerBARsp = (tTriggerBARspParams *)pEventData->pEventData;

  wdiTriggerBARsp = wpalMemoryAllocate(sizeof(WDI_TriggerBARspParamsType) + 
                      halTriggerBARsp->baCandidateCnt * 
                      sizeof(WDI_TriggerBARspCandidateType));
  if(NULL == wdiTriggerBARsp)
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
                "Failed to allocate memory in Trigger BA Response %x %x %x ",
                 pWDICtx, pEventData, pEventData->pEventData);
    wpalMemoryFree(halTriggerBARsp);
    WDI_ASSERT(0);
    return WDI_STATUS_E_FAILURE; 
  }

  wdiTriggerBARsp->wdiStatus = WDI_HAL_2_WDI_STATUS(halTriggerBARsp->status);

  if ( WDI_STATUS_SUCCESS == wdiTriggerBARsp->wdiStatus)
  {
    wdiTriggerBARsp->usBaCandidateCnt = halTriggerBARsp->baCandidateCnt;
    wpalMemoryCopy(wdiTriggerBARsp->macBSSID, 
                                 halTriggerBARsp->bssId , WDI_MAC_ADDR_LEN);

    wdiTriggerBARspCandidate = (WDI_TriggerBARspCandidateType*)(wdiTriggerBARsp + 1);
    halBaCandidate = (tTriggerBaRspCandidate*)(halTriggerBARsp + 1);

    for(index = 0; index < wdiTriggerBARsp->usBaCandidateCnt; index++)
    {
      wpalMemoryCopy(wdiTriggerBARspCandidate->macSTA, 
                                  halBaCandidate->staAddr, WDI_MAC_ADDR_LEN);
      for(TidIndex = 0; TidIndex < STA_MAX_TC; TidIndex++)
      {
        wdiTriggerBARspCandidate->wdiBAInfo[TidIndex].fBaEnable = 
                            halBaCandidate->baInfo[TidIndex].fBaEnable;
        wdiTriggerBARspCandidate->wdiBAInfo[TidIndex].startingSeqNum = 
                            halBaCandidate->baInfo[TidIndex].startingSeqNum;
      }
      wdiTriggerBARspCandidate++;
      halBaCandidate++;
    }
  }

  /*Notify UMAC*/
  wdiTriggerBARspCb( wdiTriggerBARsp, pWDICtx->pRspCBUserData);

  wpalMemoryFree(wdiTriggerBARsp);
  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessAddSessionBARsp*/

/**
 @brief Process Update Beacon Params Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessUpdateBeaconParamsRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status       wdiStatus;
  eHalStatus       halStatus;
  WDI_UpdateBeaconParamsRspCb   wdiUpdateBeaconParamsRspCb;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiUpdateBeaconParamsRspCb = (WDI_UpdateBeaconParamsRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  wpalMemoryCopy( &halStatus, 
                  pEventData->pEventData, 
                  sizeof(halStatus));

  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

  /*Notify UMAC*/
  wdiUpdateBeaconParamsRspCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessUpdateBeaconParamsRsp*/

/**
 @brief Process Send Beacon template Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSendBeaconParamsRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status       wdiStatus;
  eHalStatus       halStatus;
  WDI_SendBeaconParamsRspCb   wdiSendBeaconParamsRspCb;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiSendBeaconParamsRspCb = (WDI_SendBeaconParamsRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  wpalMemoryCopy( &halStatus, 
                  pEventData->pEventData, 
                  sizeof(halStatus));

  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

  /*Notify UMAC*/
  wdiSendBeaconParamsRspCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessSendBeaconParamsRsp*/

  
/**
 @brief Process Update Probe Resp Template Rsp function (called 
        when a response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessUpdateProbeRspTemplateRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status       wdiStatus;
  eHalStatus       halStatus;
  WDI_UpdateProbeRspTemplateRspCb   wdiUpdProbeRspTemplRspCb;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiUpdProbeRspTemplRspCb = (WDI_UpdateProbeRspTemplateRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  wpalMemoryCopy( &halStatus, 
                  pEventData->pEventData, 
                  sizeof(halStatus));

  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

  /*Notify UMAC*/
  wdiUpdProbeRspTemplRspCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessUpdateProbeRspTemplateRsp*/

  /**
 @brief Process Set Max Tx Power Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetMaxTxPowerRsp
( 
  WDI_ControlBlockType*          pWDICtx,
  WDI_EventInfoType*             pEventData
)
{
  tSetMaxTxPwrRspMsg             halTxpowerrsp;
  
  WDI_SetMaxTxPowerRspMsg        wdiSetMaxTxPowerRspMsg;
  
  WDA_SetMaxTxPowerRspCb         wdiReqStatusCb;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiReqStatusCb = (WDA_SetMaxTxPowerRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  wpalMemoryCopy( &halTxpowerrsp.setMaxTxPwrRspParams, 
                           pEventData->pEventData, 
                           sizeof(halTxpowerrsp.setMaxTxPwrRspParams)); 

  if ( eHAL_STATUS_SUCCESS != halTxpowerrsp.setMaxTxPwrRspParams.status )
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Error status returned in Set Max Tx Power Response ");
     WDI_DetectedDeviceError( pWDICtx, WDI_ERR_BASIC_OP_FAILURE); 
     return WDI_STATUS_E_FAILURE; 
  }

  wdiSetMaxTxPowerRspMsg.wdiStatus = 
         WDI_HAL_2_WDI_STATUS(halTxpowerrsp.setMaxTxPwrRspParams.status);
  wdiSetMaxTxPowerRspMsg.ucPower  = halTxpowerrsp.setMaxTxPwrRspParams.power; 

  /*Notify UMAC*/
  wdiReqStatusCb( &wdiSetMaxTxPowerRspMsg, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}

#ifdef WLAN_FEATURE_P2P
/**
 @brief Process P2P Group Owner Notice Of Absense Rsp function (called 
        when a response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessP2PGONOARsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status       wdiStatus;
  eHalStatus       halStatus;
  WDI_SetP2PGONOAReqParamsRspCb   wdiP2PGONOAReqParamsRspCb;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiP2PGONOAReqParamsRspCb = (WDI_SetP2PGONOAReqParamsRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  wpalMemoryCopy( &halStatus, 
                  pEventData->pEventData, 
                  sizeof(halStatus));

  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

  /*Notify UMAC*/
  wdiP2PGONOAReqParamsRspCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessP2PGONOARsp*/
#endif
/**
 @brief Process Enter IMPS Rsp function (called when a response 
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessEnterImpsRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status           wdiStatus;
  eHalStatus           halStatus;
  WDI_EnterImpsRspCb   wdiEnterImpsRspCb;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiEnterImpsRspCb = (WDI_EnterImpsRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  halStatus = *((eHalStatus*)pEventData->pEventData);

  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

  /*Notify UMAC*/
  wdiEnterImpsRspCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessEnterImpsRsp*/

/**
 @brief Process Exit IMPS Rsp function (called when a response 
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessExitImpsRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status           wdiStatus;
  eHalStatus           halStatus;
  WDI_ExitImpsRspCb    wdiExitImpsRspCb;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiExitImpsRspCb = (WDI_ExitImpsRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  halStatus = *((eHalStatus*)pEventData->pEventData);
  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

  // notify DTS that we are entering Full power
  WDTS_SetPowerState(pWDICtx, WDTS_POWER_STATE_FULL, NULL);

  /*Notify UMAC*/
  wdiExitImpsRspCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessExitImpsRsp*/

/**
 @brief Process Enter BMPS Rsp function (called when a response 
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessEnterBmpsRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status           wdiStatus;
  eHalStatus           halStatus;
  WDI_EnterBmpsRspCb   wdiEnterBmpsRspCb;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiEnterBmpsRspCb = (WDI_EnterBmpsRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  halStatus = *((eHalStatus*)pEventData->pEventData);
  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

  /*Notify UMAC*/
  wdiEnterBmpsRspCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessEnterBmpsRsp*/

/**
 @brief Process Exit BMPS Rsp function (called when a response 
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessExitBmpsRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status           wdiStatus;
  eHalStatus           halStatus;
  WDI_ExitBmpsRspCb   wdiExitBmpsRspCb;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiExitBmpsRspCb = (WDI_ExitBmpsRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  halStatus = *((eHalStatus*)pEventData->pEventData);
  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

  // notify DTS that we are entering Full power
  WDTS_SetPowerState(pWDICtx, WDTS_POWER_STATE_FULL, NULL);

  pWDICtx->bInBmps = eWLAN_PAL_FALSE;

  /*Notify UMAC*/
  wdiExitBmpsRspCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessExitBmpsRsp*/

/**
 @brief Process Enter UAPSD Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessEnterUapsdRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status           wdiStatus;
  eHalStatus           halStatus;
  WDI_EnterUapsdRspCb   wdiEnterUapsdRspCb;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiEnterUapsdRspCb = (WDI_EnterUapsdRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  halStatus = *((eHalStatus*)pEventData->pEventData);
  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

  if(WDI_STATUS_SUCCESS == wdiStatus)
  {
   // Set the DPU routing flag to the FW WQ, all the TX frames would be now pushed
   // from DPU to the FW-WQ (5) in UAPSD. FW would be in data path, monitoring
   // the traffic to decide when to suspend the trigger frames when there is no traffic
   // activity on the trigger enabled ACs
   pWDICtx->ucDpuRF = BMUWQ_FW_DPU_TX;

#ifdef WLAN_PERF
   // Increment the BD signature to refresh the fast path BD utilization
   pWDICtx->uBdSigSerialNum++;
#endif
  }

  /*Notify UMAC*/
  wdiEnterUapsdRspCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessEnterUapsdRsp*/

/**
 @brief Process Exit UAPSD Rsp function (called when a response 
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessExitUapsdRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status           wdiStatus;
  eHalStatus           halStatus;
  WDI_ExitUapsdRspCb   wdiExitUapsdRspCb;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiExitUapsdRspCb = (WDI_ExitUapsdRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  halStatus = *((eHalStatus*)pEventData->pEventData);
  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

   // Restore back the DPU routing flag in the TxBD, for DPU to push the TxBDs to BTQM
   // directly instead of the FW WQ.
   pWDICtx->ucDpuRF = BMUWQ_BTQM_TX_MGMT;

#ifdef WLAN_PERF
   // Increment the BD signature to refresh the fast path BD utilization
   pWDICtx->uBdSigSerialNum++;
#endif

  /*Notify UMAC*/
  wdiExitUapsdRspCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessExitUapsdRsp*/

/**
 @brief Process set UAPSD params Rsp function (called when a 
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetUapsdAcParamsRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status           wdiStatus;
  eHalStatus           halStatus;
  WDI_SetUapsdAcParamsCb   wdiSetUapsdAcParamsCb;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiSetUapsdAcParamsCb = (WDI_SetUapsdAcParamsCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  halStatus = *((eHalStatus*)pEventData->pEventData);
  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

  /*Notify UMAC*/
  wdiSetUapsdAcParamsCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessSetUapsdAcParamsRsp*/

/**
 @brief Process update UAPSD params Rsp function (called when a 
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessUpdateUapsdParamsRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status           wdiStatus;
  eHalStatus           halStatus;
  WDI_UpdateUapsdParamsCb   wdiUpdateUapsdParamsCb;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiUpdateUapsdParamsCb = (WDI_UpdateUapsdParamsCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  halStatus = *((eHalStatus*)pEventData->pEventData);
  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

  /*Notify UMAC*/
  wdiUpdateUapsdParamsCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessUpdateUapsdParamsRsp*/

/**
 @brief Process Configure RXP filter Rsp function (called when a
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessConfigureRxpFilterRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status           wdiStatus;
  eHalStatus           halStatus;
  WDI_ConfigureRxpFilterCb   wdiConfigureRxpFilterCb;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiConfigureRxpFilterCb = (WDI_ConfigureRxpFilterCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  halStatus = *((eHalStatus*)pEventData->pEventData);
  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

  /*Notify UMAC*/
  wdiConfigureRxpFilterCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessConfigureRxpFilterRsp*/

/**
 @brief Process Set beacon filter Rsp function (called when a
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetBeaconFilterRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_Status           wdiStatus;
   eHalStatus           halStatus;
   WDI_SetBeaconFilterCb   wdiBeaconFilterCb;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
       ( NULL == pEventData->pEventData))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   wdiBeaconFilterCb = (WDI_SetBeaconFilterCb)pWDICtx->pfncRspCB;

   /*-------------------------------------------------------------------------
     Extract response and send it to UMAC
   -------------------------------------------------------------------------*/
   halStatus = *((eHalStatus*)pEventData->pEventData);
   wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

   /*Notify UMAC*/
   wdiBeaconFilterCb( wdiStatus, pWDICtx->pRspCBUserData);

   return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessSetBeaconFilterRsp*/

/**
 @brief Process remove beacon filter Rsp function (called when a
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessRemBeaconFilterRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_Status           wdiStatus;
   eHalStatus           halStatus;
   WDI_RemBeaconFilterCb   wdiBeaconFilterCb;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
       ( NULL == pEventData->pEventData))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   wdiBeaconFilterCb = (WDI_RemBeaconFilterCb)pWDICtx->pfncRspCB;

   /*-------------------------------------------------------------------------
     Extract response and send it to UMAC
   -------------------------------------------------------------------------*/
   halStatus = *((eHalStatus*)pEventData->pEventData);
   wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

   /*Notify UMAC*/
   wdiBeaconFilterCb( wdiStatus, pWDICtx->pRspCBUserData);

   return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessRemBeaconFilterRsp*/

/**
 @brief Process set RSSI thresholds Rsp function (called when a
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetRSSIThresoldsRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_Status           wdiStatus;
   eHalStatus           halStatus;
   WDI_SetRSSIThresholdsCb   wdiRSSIThresholdsCb;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
       ( NULL == pEventData->pEventData))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   wdiRSSIThresholdsCb = (WDI_SetRSSIThresholdsCb)pWDICtx->pfncRspCB;

   /*-------------------------------------------------------------------------
     Extract response and send it to UMAC
   -------------------------------------------------------------------------*/
   halStatus = *((eHalStatus*)pEventData->pEventData);
   wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

   /*Notify UMAC*/
   wdiRSSIThresholdsCb( wdiStatus, pWDICtx->pRspCBUserData);

   return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessSetRSSIThresoldsRsp*/

/**
 @brief Process host offload Rsp function (called when a
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessHostOffloadRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_Status           wdiStatus;
   eHalStatus           halStatus;
   WDI_HostOffloadCb    wdiHostOffloadCb;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
       ( NULL == pEventData->pEventData))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   wdiHostOffloadCb = (WDI_HostOffloadCb)pWDICtx->pfncRspCB;

   /*-------------------------------------------------------------------------
     Extract response and send it to UMAC
   -------------------------------------------------------------------------*/
   halStatus = *((eHalStatus*)pEventData->pEventData);
   wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

   /*Notify UMAC*/
   wdiHostOffloadCb( wdiStatus, pWDICtx->pRspCBUserData);

   return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessHostOffloadRsp*/

/**
 @brief Process keep alive Rsp function (called when a
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessKeepAliveRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_Status           wdiStatus;
   eHalStatus           halStatus;
   WDI_KeepAliveCb      wdiKeepAliveCb;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
   WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_ERROR,
               "Received WDI_ProcessKeepAliveRsp Callback from HAL");


   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
       ( NULL == pEventData->pEventData))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   wdiKeepAliveCb = (WDI_KeepAliveCb)pWDICtx->pfncRspCB; 
   
   /*-------------------------------------------------------------------------
     Extract response and send it to UMAC
   -------------------------------------------------------------------------*/
   halStatus = *((eHalStatus*)pEventData->pEventData);
   wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

   /*Notify UMAC*/
   wdiKeepAliveCb( wdiStatus, pWDICtx->pRspCBUserData);

   return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessKeepAliveRsp*/

/**
 @brief Process wowl add ptrn Rsp function (called when a
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessWowlAddBcPtrnRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_Status           wdiStatus;
   eHalStatus           halStatus;
   WDI_WowlAddBcPtrnCb    wdiWowlAddBcPtrnCb;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
       ( NULL == pEventData->pEventData))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   wdiWowlAddBcPtrnCb = (WDI_WowlAddBcPtrnCb)pWDICtx->pfncRspCB;

   /*-------------------------------------------------------------------------
     Extract response and send it to UMAC
   -------------------------------------------------------------------------*/
   halStatus = *((eHalStatus*)pEventData->pEventData);
   wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

   /*Notify UMAC*/
   wdiWowlAddBcPtrnCb( wdiStatus, pWDICtx->pRspCBUserData);

   return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessWowlAddBcPtrnRsp*/

/**
 @brief Process wowl delete ptrn Rsp function (called when a 
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessWowlDelBcPtrnRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_Status           wdiStatus;
   eHalStatus           halStatus;
   WDI_WowlDelBcPtrnCb    wdiWowlDelBcPtrnCb;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
       ( NULL == pEventData->pEventData))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   wdiWowlDelBcPtrnCb = (WDI_WowlDelBcPtrnCb)pWDICtx->pfncRspCB;

   /*-------------------------------------------------------------------------
     Extract response and send it to UMAC
   -------------------------------------------------------------------------*/
   halStatus = *((eHalStatus*)pEventData->pEventData);
   wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

   /*Notify UMAC*/
   wdiWowlDelBcPtrnCb( wdiStatus, pWDICtx->pRspCBUserData);

   return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessWowlDelBcPtrnRsp*/

/**
 @brief Process wowl enter Rsp function (called when a response 
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessWowlEnterRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_Status           wdiStatus;
   eHalStatus           halStatus;
   WDI_WowlEnterReqCb   wdiWowlEnterCb;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
       ( NULL == pEventData->pEventData))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   wdiWowlEnterCb = (WDI_WowlEnterReqCb)pWDICtx->pfncRspCB;

   /*-------------------------------------------------------------------------
     Extract response and send it to UMAC
   -------------------------------------------------------------------------*/
   halStatus = *((eHalStatus*)pEventData->pEventData);
   wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

   /*Notify UMAC*/
   wdiWowlEnterCb( wdiStatus, pWDICtx->pRspCBUserData);

   return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessWowlEnterRsp*/

/**
 @brief Process wowl exit Rsp function (called when a response 
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessWowlExitRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_Status           wdiStatus;
   eHalStatus           halStatus;
   WDI_WowlExitReqCb   wdiWowlExitCb;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
       ( NULL == pEventData->pEventData))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   wdiWowlExitCb = (WDI_WowlExitReqCb)pWDICtx->pfncRspCB;

   /*-------------------------------------------------------------------------
     Extract response and send it to UMAC
   -------------------------------------------------------------------------*/
   halStatus = *((eHalStatus*)pEventData->pEventData);
   wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

   /*Notify UMAC*/
   wdiWowlExitCb( wdiStatus, pWDICtx->pRspCBUserData);

   return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessWowlExitRsp*/

/**
 @brief Process Configure Apps CPU wakeup State Rsp function 
        (called when a response is being received over the bus
        from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessConfigureAppsCpuWakeupStateRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_Status           wdiStatus;
   eHalStatus           halStatus;
   WDI_ConfigureAppsCpuWakeupStateCb   wdiConfigureAppsCpuWakeupStateCb;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
       ( NULL == pEventData->pEventData))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   wdiConfigureAppsCpuWakeupStateCb = (WDI_ConfigureAppsCpuWakeupStateCb)pWDICtx->pfncRspCB;

   /*-------------------------------------------------------------------------
     Extract response and send it to UMAC
   -------------------------------------------------------------------------*/
   halStatus = *((eHalStatus*)pEventData->pEventData);
   wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

   /*Notify UMAC*/
   wdiConfigureAppsCpuWakeupStateCb( wdiStatus, pWDICtx->pRspCBUserData);

   return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessConfigureAppsCpuWakeupStateRsp*/


/**
 @brief Process Nv download(called when a response
        is being received over the bus from HAL,will check if the responce is )
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessNvDownloadRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{

  WDI_NvDownloadRspCb    wdiNvDownloadRspCb;
  tHalNvImgDownloadRspParams halNvDownloadRsp;
  WDI_NvDownloadRspInfoType wdiNvDownloadRsp;


  /*-------------------------------------------------------------------------
   Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
    ( NULL == pEventData->pEventData))
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                "%s: Invalid parameters", __FUNCTION__);
    WDI_ASSERT(0);
    return WDI_STATUS_E_FAILURE; 
  }

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
   -------------------------------------------------------------------------*/
  wpalMemoryCopy( &halNvDownloadRsp, 
                  pEventData->pEventData, 
                  sizeof(halNvDownloadRsp));

  wdiNvDownloadRsp.wdiStatus = WDI_HAL_2_WDI_STATUS(halNvDownloadRsp.status);

  if((wdiNvDownloadRsp.wdiStatus == WDI_STATUS_SUCCESS) &&
    (pWDICtx->wdiNvBlobInfo.usCurrentFragment != 
         pWDICtx->wdiNvBlobInfo.usTotalFragment )) 
  {
    WDI_NvDownloadReq(&pWDICtx->wdiCachedNvDownloadReq,
       (WDI_NvDownloadRspCb)pWDICtx->pfncRspCB, pWDICtx->pRspCBUserData); 
  }
  else
  {
    /*Reset the Nv related global information in WDI context information */
    pWDICtx->wdiNvBlobInfo.usTotalFragment = 0;
    pWDICtx->wdiNvBlobInfo.usFragmentSize = 0;
    pWDICtx->wdiNvBlobInfo.usCurrentFragment = 0;
    /*call WDA callback function for last fragment */
    wdiNvDownloadRspCb = (WDI_NvDownloadRspCb)pWDICtx->pfncRspCB;
    wdiNvDownloadRspCb( &wdiNvDownloadRsp, pWDICtx->pRspCBUserData);
  }

  return WDI_STATUS_SUCCESS; 
}
#ifdef WLAN_FEATURE_VOWIFI_11R
/**
 @brief Process Add TSpec Rsp function (called when a response
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessAggrAddTSpecRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status            wdiStatus;
  tAggrAddTsRspParams   aggrAddTsRsp;
  WDI_AggrAddTsRspCb    wdiAggrAddTsRspCb;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiAggrAddTsRspCb = (WDI_AddTsRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  wpalMemoryCopy( &aggrAddTsRsp, 
                  pEventData->pEventData, 
                  sizeof(aggrAddTsRsp));

  /* What is the difference between status0 and status1? */
  wdiStatus   =   WDI_HAL_2_WDI_STATUS(aggrAddTsRsp.status0); 

  /*Notify UMAC*/
  wdiAggrAddTsRspCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessAddTSpecRsp*/
#endif /* WLAN_FEATURE_VOWIFI_11R */

/**
 @brief WDI_ProcessHostResumeRsp function (called when a 
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessHostResumeRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_SuspendResumeRspParamsType     wdiResumeRspParams;
  WDI_HostResumeEventRspCb           wdiHostResumeRspCb;
  tHalHostResumeRspParams            hostResumeRspMsg;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
    -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
                "%s: Invalid parameters", __FUNCTION__);
    WDI_ASSERT(0);
    return WDI_STATUS_E_FAILURE; 
  }

  wdiHostResumeRspCb = (WDI_HostResumeEventRspCb)pWDICtx->pfncRspCB;

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
    -------------------------------------------------------------------------*/

  wpalMemoryCopy( &hostResumeRspMsg, 
      (wpt_uint8*)pEventData->pEventData,
      sizeof(hostResumeRspMsg));

  wdiResumeRspParams.wdiStatus   =   
    WDI_HAL_2_WDI_STATUS(hostResumeRspMsg.status); 

  /*Notify UMAC*/
  wdiHostResumeRspCb(&wdiResumeRspParams, (void*) pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS;
}

/**
 @brief Process Set Tx PER Rsp function (called when a response 
        is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetTxPerTrackingRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status           wdiStatus;
  eHalStatus           halStatus;
  WDI_SetTxPerTrackingRspCb   pwdiSetTxPerTrackingRspCb;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }
  
  pwdiSetTxPerTrackingRspCb = (WDI_SetTxPerTrackingRspCb)pWDICtx->pfncRspCB; 

  /*-------------------------------------------------------------------------
    Extract response and send it to UMAC
  -------------------------------------------------------------------------*/
  halStatus = *((eHalStatus*)pEventData->pEventData);
  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

  /*Notify UMAC*/
  pwdiSetTxPerTrackingRspCb( wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessSetTxPerTrackingRsp*/

/*==========================================================================
                        Indications from HAL
 ==========================================================================*/
/**
 @brief Process Low RSSI Indication function (called when an 
        indication of this kind is being received over the bus
        from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessLowRSSIInd
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_LowLevelIndType  wdiInd;
  tHalRSSINotificationIndMsg   halRSSINotificationIndMsg;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  /*-------------------------------------------------------------------------
    Extract indication and send it to UMAC
  -------------------------------------------------------------------------*/
  wpalMemoryCopy( (void *)&halRSSINotificationIndMsg.rssiNotificationParams, 
                  pEventData->pEventData, 
                  sizeof(tHalRSSINotification));

  /*Fill in the indication parameters*/
  wdiInd.wdiIndicationType = WDI_HAL_RSSI_NOTIFICATION_IND; 
  wdiInd.wdiIndicationData.wdiLowRSSIInfo.bRssiThres1PosCross =
     halRSSINotificationIndMsg.rssiNotificationParams.bRssiThres1PosCross;
  wdiInd.wdiIndicationData.wdiLowRSSIInfo.bRssiThres1NegCross =
     halRSSINotificationIndMsg.rssiNotificationParams.bRssiThres1NegCross;
  wdiInd.wdiIndicationData.wdiLowRSSIInfo.bRssiThres2PosCross =
     halRSSINotificationIndMsg.rssiNotificationParams.bRssiThres2PosCross;
  wdiInd.wdiIndicationData.wdiLowRSSIInfo.bRssiThres2NegCross =
     halRSSINotificationIndMsg.rssiNotificationParams.bRssiThres2NegCross;
  wdiInd.wdiIndicationData.wdiLowRSSIInfo.bRssiThres3PosCross =
     halRSSINotificationIndMsg.rssiNotificationParams.bRssiThres3PosCross;
  wdiInd.wdiIndicationData.wdiLowRSSIInfo.bRssiThres3NegCross =
     halRSSINotificationIndMsg.rssiNotificationParams.bRssiThres3NegCross;

  /*Notify UMAC*/
  pWDICtx->wdiLowLevelIndCB( &wdiInd, pWDICtx->pIndUserData );
  
  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessLowRSSIInd*/


/**
 @brief Process Missed Beacon Indication function (called when 
        an indication of this kind is being received over the
        bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessMissedBeaconInd
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status           wdiStatus;
  eHalStatus           halStatus;
  WDI_LowLevelIndType  wdiInd;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  /*-------------------------------------------------------------------------
    Extract indication and send it to UMAC
  -------------------------------------------------------------------------*/
  /*! TO DO: Parameters need to be unpacked according to HAL struct*/
  halStatus = *((eHalStatus*)pEventData->pEventData);
  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

  /*Fill in the indication parameters*/
  wdiInd.wdiIndicationType = WDI_MISSED_BEACON_IND; 
  
  /*Notify UMAC*/
  pWDICtx->wdiLowLevelIndCB( &wdiInd, pWDICtx->pIndUserData );
  
  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessMissedBeaconInd*/


/**
 @brief Process Unk Addr Frame Indication function (called when 
        an indication of this kind is being received over the
        bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessUnkAddrFrameInd
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status           wdiStatus;
  eHalStatus           halStatus;
  WDI_LowLevelIndType  wdiInd;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  /*-------------------------------------------------------------------------
    Extract indication and send it to UMAC
  -------------------------------------------------------------------------*/
  /*! TO DO: Parameters need to be unpacked according to HAL struct*/
  halStatus = *((eHalStatus*)pEventData->pEventData);
  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

  /*Fill in the indication parameters*/
  wdiInd.wdiIndicationType = WDI_UNKNOWN_ADDR2_FRAME_RX_IND; 
  /* ! TO DO - fill in from HAL struct:
    wdiInd.wdiIndicationData.wdiUnkAddr2FrmInfo*/

  /*Notify UMAC*/
  pWDICtx->wdiLowLevelIndCB( &wdiInd, pWDICtx->pIndUserData );
  
  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessUnkAddrFrameInd*/


/**
 @brief Process MIC Failure Indication function (called when an 
        indication of this kind is being received over the bus
        from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessMicFailureInd
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_LowLevelIndType  wdiInd;
  tpSirMicFailureInd   pHalMicFailureInd;

  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }
  
  pHalMicFailureInd = (tpSirMicFailureInd)pEventData->pEventData;
  /*-------------------------------------------------------------------------
    Extract indication and send it to UMAC
  -------------------------------------------------------------------------*/

  /*Fill in the indication parameters*/
  wdiInd.wdiIndicationType = WDI_MIC_FAILURE_IND; 
  wpalMemoryCopy(wdiInd.wdiIndicationData.wdiMICFailureInfo.bssId,
                 pHalMicFailureInd->bssId, WDI_MAC_ADDR_LEN);
  wpalMemoryCopy(wdiInd.wdiIndicationData.wdiMICFailureInfo.macSrcAddr,
                 pHalMicFailureInd->info.srcMacAddr, WDI_MAC_ADDR_LEN);
  wpalMemoryCopy(wdiInd.wdiIndicationData.wdiMICFailureInfo.macTaAddr,
                 pHalMicFailureInd->info.taMacAddr, WDI_MAC_ADDR_LEN);
  wpalMemoryCopy(wdiInd.wdiIndicationData.wdiMICFailureInfo.macDstAddr,
                 pHalMicFailureInd->info.dstMacAddr, WDI_MAC_ADDR_LEN);
  wdiInd.wdiIndicationData.wdiMICFailureInfo.ucMulticast = 
                 pHalMicFailureInd->info.multicast;
  wdiInd.wdiIndicationData.wdiMICFailureInfo.ucIV1 = 
                 pHalMicFailureInd->info.IV1;
  wdiInd.wdiIndicationData.wdiMICFailureInfo.keyId= 
                 pHalMicFailureInd->info.keyId;
  wpalMemoryCopy(wdiInd.wdiIndicationData.wdiMICFailureInfo.TSC,
                 pHalMicFailureInd->info.TSC,WDI_CIPHER_SEQ_CTR_SIZE);
  wpalMemoryCopy(wdiInd.wdiIndicationData.wdiMICFailureInfo.macRxAddr,
                 pHalMicFailureInd->info.rxMacAddr, WDI_MAC_ADDR_LEN);
  /*Notify UMAC*/
  pWDICtx->wdiLowLevelIndCB( &wdiInd, pWDICtx->pIndUserData );
  
  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessMicFailureInd*/


/**
 @brief Process Fatal Failure Indication function (called when 
        an indication of this kind is being received over the
        bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessFatalErrorInd
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_Status           wdiStatus;
  eHalStatus           halStatus;
  WDI_LowLevelIndType  wdiInd;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  /*-------------------------------------------------------------------------
    Extract indication and send it to UMAC
  -------------------------------------------------------------------------*/

  /*! TO DO: Parameters need to be unpacked according to HAL struct*/
  halStatus = *((eHalStatus*)pEventData->pEventData);
  wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

  WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Fatal failure received from device %d ", halStatus );
  
  /*Fill in the indication parameters*/
  wdiInd.wdiIndicationType             = WDI_FATAL_ERROR_IND; 
  wdiInd.wdiIndicationData.usErrorCode = WDI_ERR_DEV_INTERNAL_FAILURE; 

  /*Notify UMAC*/
  pWDICtx->wdiLowLevelIndCB( &wdiInd, pWDICtx->pIndUserData );
  
  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessFatalErrorInd*/

/**
 @brief Process Delete STA Indication function (called when 
        an indication of this kind is being received over the
        bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessDelSTAInd
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  tDeleteStaContextParams    halDelSTACtx;
  WDI_LowLevelIndType        wdiInd;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  /*-------------------------------------------------------------------------
    Extract indication and send it to UMAC
  -------------------------------------------------------------------------*/

  /* Parameters need to be unpacked according to HAL struct*/
  wpalMemoryCopy( &halDelSTACtx, 
                  pEventData->pEventData, 
                  sizeof(halDelSTACtx));

  /*Fill in the indication parameters*/
  wdiInd.wdiIndicationType             = WDI_DEL_STA_IND; 

  wpalMemoryCopy(wdiInd.wdiIndicationData.wdiDeleteSTAIndType.macADDR2,
                 halDelSTACtx.addr2, WDI_MAC_ADDR_LEN);
  wpalMemoryCopy(wdiInd.wdiIndicationData.wdiDeleteSTAIndType.macBSSID,
                 halDelSTACtx.bssId, WDI_MAC_ADDR_LEN);

  wdiInd.wdiIndicationData.wdiDeleteSTAIndType.usAssocId = 
    halDelSTACtx.assocId;
  wdiInd.wdiIndicationData.wdiDeleteSTAIndType.ucSTAIdx  = 
    halDelSTACtx.staId;
  wdiInd.wdiIndicationData.wdiDeleteSTAIndType.wptReasonCode = 
    halDelSTACtx.reasonCode; 

  /*Notify UMAC*/
  pWDICtx->wdiLowLevelIndCB( &wdiInd, pWDICtx->pIndUserData );
  
  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessDelSTAInd*/

/**
*@brief Process Coex Indication function (called when
        an indication of this kind is being received over the
        bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessCoexInd
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_LowLevelIndType  wdiInd;
  tCoexIndMsg          halCoexIndMsg;
  wpt_uint32           index;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData ))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT( 0 );
     return WDI_STATUS_E_FAILURE; 
  }

  /*-------------------------------------------------------------------------
    Extract indication and send it to UMAC
  -------------------------------------------------------------------------*/
  wpalMemoryCopy( &halCoexIndMsg.coexIndParams, 
                  pEventData->pEventData, 
                  sizeof(halCoexIndMsg.coexIndParams) );

  /*Fill in the indication parameters*/
  wdiInd.wdiIndicationType = WDI_COEX_IND; 
  wdiInd.wdiIndicationData.wdiCoexInfo.coexIndType = halCoexIndMsg.coexIndParams.coexIndType; 
  for (index = 0; index < WDI_COEX_IND_DATA_SIZE; index++)
  {
    wdiInd.wdiIndicationData.wdiCoexInfo.coexIndData[index] = halCoexIndMsg.coexIndParams.coexIndData[index]; 
  }

  // DEBUG
  WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_INFO,
              "[COEX WDI] Coex Ind Type (%x) data (%x %x %x %x)",
              wdiInd.wdiIndicationData.wdiCoexInfo.coexIndType, 
              wdiInd.wdiIndicationData.wdiCoexInfo.coexIndData[0], 
              wdiInd.wdiIndicationData.wdiCoexInfo.coexIndData[1], 
              wdiInd.wdiIndicationData.wdiCoexInfo.coexIndData[2], 
              wdiInd.wdiIndicationData.wdiCoexInfo.coexIndData[3] ); 

  /*Notify UMAC*/
  pWDICtx->wdiLowLevelIndCB( &wdiInd, pWDICtx->pIndUserData );
  
  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessCoexInd*/

/**
*@brief Process Tx Complete Indication function (called when
        an indication of this kind is being received over the
        bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessTxCompleteInd
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_LowLevelIndType  wdiInd;
  tTxComplIndMsg       halTxComplIndMsg;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData ))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT( 0 );
     return WDI_STATUS_E_FAILURE;
  }

  /*-------------------------------------------------------------------------
    Extract indication and send it to UMAC
  -------------------------------------------------------------------------*/
  wpalMemoryCopy( &halTxComplIndMsg.txComplParams, 
                  pEventData->pEventData, 
                  sizeof(halTxComplIndMsg.txComplParams) );

  /*Fill in the indication parameters*/
  wdiInd.wdiIndicationType = WDI_TX_COMPLETE_IND; 
  wdiInd.wdiIndicationData.tx_complete_status 
                               = halTxComplIndMsg.txComplParams.status; 

  /*Notify UMAC*/
  pWDICtx->wdiLowLevelIndCB( &wdiInd, pWDICtx->pIndUserData );
  
  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessTxCompleteInd*/

#ifdef WLAN_FEATURE_P2P
/**
*@brief Process Noa Attr Indication function (called when
        an indication of this kind is being received over the
        bus from HAL)

 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessP2pNoaAttrInd
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_LowLevelIndType  wdiInd;
  tNoaAttrIndMsg       halNoaAttrIndMsg;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData ))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT( 0 );
     return WDI_STATUS_E_FAILURE;
  }

  /*-------------------------------------------------------------------------
    Extract indication and send it to UMAC
  -------------------------------------------------------------------------*/
  wpalMemoryCopy( &halNoaAttrIndMsg.noaAttrIndParams,
                  pEventData->pEventData,
                  sizeof(halNoaAttrIndMsg.noaAttrIndParams) );

  /*Fill in the indication parameters*/
  wdiInd.wdiIndicationType = WDI_P2P_NOA_ATTR_IND;
  
  wdiInd.wdiIndicationData.wdiP2pNoaAttrInfo.status
                          = halNoaAttrIndMsg.noaAttrIndParams.status;
  
  wdiInd.wdiIndicationData.wdiP2pNoaAttrInfo.ucIndex
                          = halNoaAttrIndMsg.noaAttrIndParams.index;
  wdiInd.wdiIndicationData.wdiP2pNoaAttrInfo.ucOppPsFlag
                          = halNoaAttrIndMsg.noaAttrIndParams.oppPsFlag;
  wdiInd.wdiIndicationData.wdiP2pNoaAttrInfo.usCtWin
                          = halNoaAttrIndMsg.noaAttrIndParams.ctWin;
  
  wdiInd.wdiIndicationData.wdiP2pNoaAttrInfo.usNoa1IntervalCnt
                          = halNoaAttrIndMsg.noaAttrIndParams.uNoa1IntervalCnt;
  wdiInd.wdiIndicationData.wdiP2pNoaAttrInfo.uslNoa1Duration
                          = halNoaAttrIndMsg.noaAttrIndParams.uNoa1Duration;
  wdiInd.wdiIndicationData.wdiP2pNoaAttrInfo.uslNoa1Interval
                             = halNoaAttrIndMsg.noaAttrIndParams.uNoa1Interval;
  wdiInd.wdiIndicationData.wdiP2pNoaAttrInfo.uslNoa1StartTime
                          = halNoaAttrIndMsg.noaAttrIndParams.uNoa1StartTime;
  
  wdiInd.wdiIndicationData.wdiP2pNoaAttrInfo.usNoa2IntervalCnt
                          = halNoaAttrIndMsg.noaAttrIndParams.uNoa2IntervalCnt;
  wdiInd.wdiIndicationData.wdiP2pNoaAttrInfo.uslNoa2Duration
                          = halNoaAttrIndMsg.noaAttrIndParams.uNoa2Duration;
  wdiInd.wdiIndicationData.wdiP2pNoaAttrInfo.uslNoa2Interval
                          = halNoaAttrIndMsg.noaAttrIndParams.uNoa2Interval;
  wdiInd.wdiIndicationData.wdiP2pNoaAttrInfo.uslNoa2StartTime
                          = halNoaAttrIndMsg.noaAttrIndParams.uNoa2StartTime;

  /*Notify UMAC*/
  pWDICtx->wdiLowLevelIndCB( &wdiInd, pWDICtx->pIndUserData );

  return WDI_STATUS_SUCCESS;
}/*WDI_ProcessNoaAttrInd*/
#endif

/**
 @brief Process Tx PER Hit Indication function (called when 
        an indication of this kind is being received over the
        bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessTxPerHitInd
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_LowLevelIndType  wdiInd;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  
  /*-------------------------------------------------------------------------
    Extract indication and send it to UMAC
  -------------------------------------------------------------------------*/
  /*Fill in the indication parameters*/
  wdiInd.wdiIndicationType = WDI_TX_PER_HIT_IND; 
  
  /*Notify UMAC*/
  pWDICtx->wdiLowLevelIndCB( &wdiInd, pWDICtx->pIndUserData );

  return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessTxPerHitInd*/

#ifdef ANI_MANF_DIAG
/**
 @brief WDI_ProcessFTMCommandReq
        Process FTM Command, simply route to HAL
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessFTMCommandReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_FTMCommandReqType  *ftmCommandReq = NULL;
  wpt_uint8              *ftmCommandBuffer = NULL;
  wpt_uint16              dataOffset;
  wpt_uint16              bufferSize;
  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))

  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  ftmCommandReq = (WDI_FTMCommandReqType *)pEventData->pEventData;

  /* Get MSG Buffer */
  WDI_GetMessageBuffer(pWDICtx,
                       WDI_FTM_CMD_REQ,
                       ftmCommandReq->bodyLength,
                       &ftmCommandBuffer,
                       &dataOffset,
                       &bufferSize);

  wpalMemoryCopy(ftmCommandBuffer + dataOffset,
                 ftmCommandReq->FTMCommandBody,
                 ftmCommandReq->bodyLength);

  /* Send MSG */
  return WDI_SendMsg(pWDICtx,
                     ftmCommandBuffer,
                     bufferSize,
                     pEventData->pCBfnc,
                     pEventData->pUserData,
                     WDI_FTM_CMD_RESP);
}

/**
 @brief WDI_ProcessFTMCommandRsp
        Process FTM Command Response from HAL, simply route to HDD FTM
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessFTMCommandRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_FTMCommandRspCb   ftmCMDRspCb;
  tProcessPttRspParams *ftmCMDRspData = NULL;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  ftmCMDRspCb = (WDI_FTMCommandRspCb)pWDICtx->pfncRspCB;

  ftmCMDRspData = (tProcessPttRspParams *)pEventData->pEventData;

  wpalMemoryCopy((void *)pWDICtx->ucFTMCommandRspBuffer, 
                 (void *)&ftmCMDRspData->pttMsgBuffer, 
                 ftmCMDRspData->pttMsgBuffer.msgBodyLength);

  /*Notify UMAC*/
  ftmCMDRspCb((void *)pWDICtx->ucFTMCommandRspBuffer, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}
#endif /* ANI_MANF_DIAG */
/**
 @brief WDI_ProcessHalDumpCmdReq
        Process hal dump Command, simply route to HAL
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessHALDumpCmdReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_HALDumpCmdReqParamsType*  pwdiHALDumpCmdParams = NULL;
  WDI_HALDumpCmdRspCb           wdiHALDumpCmdRspCb = NULL;
  wpt_uint16               usDataOffset        = 0;
  wpt_uint16               usSendSize          = 0;
  tHalDumpCmdReqMsg        halDumpCmdReqMsg;
  wpt_uint8*               pSendBuffer         = NULL; 

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData) ||
      ( NULL == pEventData->pCBfnc ))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  pwdiHALDumpCmdParams = (WDI_HALDumpCmdReqParamsType*)pEventData->pEventData;
  wdiHALDumpCmdRspCb   = (WDI_HALDumpCmdRspCb)pEventData->pCBfnc;

  /* Copying the HAL DUMP Command Information HAL Structure*/
  halDumpCmdReqMsg.dumpCmdReqParams.argument1 = 
                pwdiHALDumpCmdParams->wdiHALDumpCmdInfoType.command;
  halDumpCmdReqMsg.dumpCmdReqParams.argument2 = 
                pwdiHALDumpCmdParams->wdiHALDumpCmdInfoType.argument1;
  halDumpCmdReqMsg.dumpCmdReqParams.argument3 = 
                pwdiHALDumpCmdParams->wdiHALDumpCmdInfoType.argument2;
  halDumpCmdReqMsg.dumpCmdReqParams.argument4 = 
                pwdiHALDumpCmdParams->wdiHALDumpCmdInfoType.argument3;
  halDumpCmdReqMsg.dumpCmdReqParams.argument5 = 
                pwdiHALDumpCmdParams->wdiHALDumpCmdInfoType.argument4;
  
  /*-----------------------------------------------------------------------
    Get message buffer
  -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_HAL_DUMP_CMD_REQ, 
                        sizeof(halDumpCmdReqMsg.dumpCmdReqParams),
                        &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < 
            (usDataOffset + sizeof(halDumpCmdReqMsg.dumpCmdReqParams) )))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
              "Unable to get send buffer in HAL Dump Command req %x %x %x",
                pEventData, pwdiHALDumpCmdParams, wdiHALDumpCmdRspCb);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wpalMemoryCopy( pSendBuffer+usDataOffset, 
                  &halDumpCmdReqMsg.dumpCmdReqParams, 
                  sizeof(halDumpCmdReqMsg.dumpCmdReqParams)); 

  pWDICtx->wdiReqStatusCB     = pwdiHALDumpCmdParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiHALDumpCmdParams->pUserData; 

  /*-------------------------------------------------------------------------
    Send Start Request to HAL 
  -------------------------------------------------------------------------*/
  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        wdiHALDumpCmdRspCb, pEventData->pUserData, 
                        WDI_HAL_DUMP_CMD_RESP); 
}

/**
 @brief WDI_ProcessHalDumpCmdRsp
        Process hal Dump Command Response from HAL, simply route to HDD 
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessHALDumpCmdRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_HALDumpCmdRspCb     wdiHALDumpCmdRspCb;
  tpHalDumpCmdRspParams   halDumpCmdRspParams;
  WDI_HALDumpCmdRspParamsType wdiHALDumpCmdRsp;

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT(0);
     return WDI_STATUS_E_FAILURE; 
  }

  wdiHALDumpCmdRspCb = (WDI_HALDumpCmdRspCb)pWDICtx->pfncRspCB; 

  /*Initialize the WDI Response structure */
  wdiHALDumpCmdRsp.usBufferLen = 0;
  wdiHALDumpCmdRsp.pBuffer = NULL;

  halDumpCmdRspParams = (tHalDumpCmdRspParams *)pEventData->pEventData;
  
  wdiHALDumpCmdRsp.wdiStatus   = 
              WDI_HAL_2_WDI_STATUS(halDumpCmdRspParams->status); 

  if (( wdiHALDumpCmdRsp.wdiStatus  ==  WDI_STATUS_SUCCESS) &&
      (halDumpCmdRspParams->rspLength != 0))
  {
      /* Copy the response data */
      wdiHALDumpCmdRsp.usBufferLen = halDumpCmdRspParams->rspLength;
      wdiHALDumpCmdRsp.pBuffer = wpalMemoryAllocate(halDumpCmdRspParams->rspLength);
      wpalMemoryCopy( &halDumpCmdRspParams->rspBuffer, 
                  wdiHALDumpCmdRsp.pBuffer, 
                  sizeof(wdiHALDumpCmdRsp.usBufferLen));
  }
  
  /*Notify UMAC*/
  wdiHALDumpCmdRspCb(&wdiHALDumpCmdRsp, pWDICtx->pRspCBUserData);

  if(wdiHALDumpCmdRsp.pBuffer != NULL)
  {
    /* Free the allocated buffer */
    wpalMemoryFree(wdiHALDumpCmdRsp.pBuffer);
  }
  return WDI_STATUS_SUCCESS;
}

/*==========================================================================
                     CONTRL TRANSPORT INTERACTION
 
    Callback function registered with the control transport - for receiving
    notifications and packets 
==========================================================================*/
/**
 @brief    This callback is invoked by the control transport 
   when it wishes to send up a notification like the ones
   mentioned above.
 
 @param
    
    wctsHandle:       handle to the control transport service 
    wctsEvent:        the event being notified
    wctsNotifyCBData: the callback data of the user 
    
 @see  WCTS_OpenTransport
  
 @return None 
*/
void 
WDI_NotifyMsgCTSCB
(
  WCTS_HandleType        wctsHandle, 
  WCTS_NotifyEventType   wctsEvent,
  void*                  wctsNotifyCBData
)
{
  WDI_ControlBlockType*  pWDICtx = (WDI_ControlBlockType*)wctsNotifyCBData; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  if (NULL == pWDICtx )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                "%s: Invalid parameters", __FUNCTION__);
    WDI_ASSERT(0);
    return; 
  }

  if (WDI_CONTROL_BLOCK_MAGIC != pWDICtx->magic)
  {
    /* callback presumably occurred after close */
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                "%s: Invalid control block", __FUNCTION__);
    return; 
  }

  if ( WCTS_EVENT_OPEN == wctsEvent )
  {
    /*Flag must be set atomically as it is checked from incoming request
      functions*/
    wpalMutexAcquire(&pWDICtx->wptMutex);
    pWDICtx->bCTOpened   = eWLAN_PAL_TRUE; 

    /*Nothing to do - so try to dequeue any pending request that may have
     occurred while we were trying to establish this*/
    WDI_DequeuePendingReq(pWDICtx);
    wpalMutexRelease(&pWDICtx->wptMutex);   
  }
  else if  ( WCTS_EVENT_CLOSE == wctsEvent ) 
  {
    /*Flag must be set atomically as it is checked from incoming request
      functions*/
    wpalMutexAcquire(&pWDICtx->wptMutex);
    pWDICtx->bCTOpened   = eWLAN_PAL_FALSE; 

    /*No other request will be processed from now on - fail all*/
    WDI_ClearPendingRequests(pWDICtx); 
    wpalMutexRelease(&pWDICtx->wptMutex);

    /*Notify that the Control Channel is closed */
    wpalEventSet(&pWDICtx->wctsActionEvent);
  }

}/*WDI_NotifyMsgCTSCB*/


/**
 @brief    This callback is invoked by the control transport 
           when it wishes to send up a packet received over the
           bus.
 
 @param
    
    wctsHandle:  handle to the control transport service 
    pMsg:        the packet
    uLen:        the packet length
    wctsRxMsgCBData: the callback data of the user 
    
 @see  WCTS_OpenTransport
  
 @return None 
*/
void 
WDI_RXMsgCTSCB 
(
  WCTS_HandleType       wctsHandle, 
  void*                 pMsg,
  wpt_uint32            uLen,
  void*                 wctsRxMsgCBData
)
{
  tHalMsgHeader          *pHalMsgHeader; 
  WDI_EventInfoType      wdiEventData; 
  WDI_ControlBlockType*  pWDICtx = (WDI_ControlBlockType*)wctsRxMsgCBData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Sanity check 
  ------------------------------------------------------------------------*/
  if ((NULL == pWDICtx ) || ( NULL == pMsg ) || 
      ( uLen < sizeof(tHalMsgHeader)))
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                "%s: Invalid parameters", __FUNCTION__);
    WDI_ASSERT(0);
    return; 
  }

  if (WDI_CONTROL_BLOCK_MAGIC != pWDICtx->magic)
  {
    /* callback presumably occurred after close */
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                "%s: Invalid control block", __FUNCTION__);
    return; 
  }

  /*The RX Callback is expected to be serialized in the proper control thread 
    context - so no serialization is necessary here
    ! - revisit this assumption */

  pHalMsgHeader = (tHalMsgHeader *)pMsg;

  if ( uLen != pHalMsgHeader->msgLen )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Invalid packet received from HAL - catastrophic failure");
    WDI_DetectedDeviceError( pWDICtx, WDI_ERR_INVALID_RSP_FMT); 
    return; 
  }

  wdiEventData.wdiResponse = HAL_2_WDI_RSP_TYPE( pHalMsgHeader->msgType );

  /*The message itself starts after the header*/
  wdiEventData.pEventData     = (wpt_uint8*)pMsg + sizeof(tHalMsgHeader);
  wdiEventData.uEventDataSize = pHalMsgHeader->msgLen - sizeof(tHalMsgHeader);
  wdiEventData.pCBfnc         = gWDICb.pfncRspCB;
  wdiEventData.pUserData      = gWDICb.pRspCBUserData;


  if ( wdiEventData.wdiResponse ==  pWDICtx->wdiExpectedResponse )
  {
    /*Stop the timer as the response was received */
    /*!UT - check for potential race conditions between stop and response */
    wpalTimerStop(&pWDICtx->wptResponseTimer);
  }
  /* Check if we receive a response message which is not expected */
  else if ( wdiEventData.wdiResponse < WDI_HAL_IND_MIN )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_FATAL,
               "Received response %s (%d) when expecting %s (%d) - catastrophic failure",
               WDI_getRespMsgString(wdiEventData.wdiResponse),
               wdiEventData.wdiResponse, 
               WDI_getRespMsgString(pWDICtx->wdiExpectedResponse),
               pWDICtx->wdiExpectedResponse);
    /* WDI_DetectedDeviceError( pWDICtx, WDI_ERR_INVALID_RSP_FMT); */
    return;
  }

  /*Post response event to the state machine*/
  WDI_PostMainEvent(pWDICtx, WDI_RESPONSE_EVENT, &wdiEventData);

}/*WDI_RXMsgCTSCB*/


/*========================================================================
         Internal Helper Routines 
========================================================================*/

/**
 @brief WDI_CleanCB - internal helper routine used to clean the 
        WDI Main Control Block
 
 @param pWDICtx - pointer to the control block

 @return Result of the function call
*/
WPT_INLINE WDI_Status
WDI_CleanCB
(
  WDI_ControlBlockType*  pWDICtx
)
{
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*Clean the WDI Control Block*/
  wpalMemoryZero( pWDICtx, sizeof(*pWDICtx)); 

  pWDICtx->uGlobalState  = WDI_MAX_ST; 
  pWDICtx->ucMaxBssids   = WDI_MAX_SUPPORTED_BSS;
  pWDICtx->ucMaxStations = WDI_MAX_SUPPORTED_STAS;

  WDI_ResetAssocSessions( pWDICtx );

  return WDI_STATUS_SUCCESS;
}/*WDI_CleanCB*/


/**
 @brief Process request helper function 

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WPT_INLINE WDI_Status
WDI_ProcessRequest
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*!! Skip sanity check as this is called from the FSM functionss which 
    already checked these pointers*/

  if (( pEventData->wdiRequest < WDI_MAX_UMAC_IND ) &&
      ( NULL != pfnReqProcTbl[pEventData->wdiRequest] ))
  {  
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
              "Calling request processing function for req %s (%d) %x",
              WDI_getReqMsgString(pEventData->wdiRequest),
              pEventData->wdiRequest, pfnReqProcTbl[pEventData->wdiRequest]);
    return pfnReqProcTbl[pEventData->wdiRequest](pWDICtx, pEventData);
  }
  else
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Operation %d is not yet implemented ", 
               pEventData->wdiRequest);
    return WDI_STATUS_E_NOT_IMPLEMENT;
  }
}/*WDI_ProcessRequest*/


/**
 @brief Get message helper function - it allocates memory for a 
        message that is to be sent to HAL accross the bus and
        prefixes it with a send message header 
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         wdiReqType:      type of the request being sent
         uBufferLen:      message buffer len
         pMsgBuffer:      resulting allocated buffer
         pusDataOffset:    offset in the buffer where the caller
         can start copying its message data
         puBufferSize:    the resulting buffer size (offset+buff
         len)
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_GetMessageBuffer
( 
  WDI_ControlBlockType*  pWDICtx, 
  WDI_RequestEnumType    wdiReqType, 
  wpt_uint16             usBufferLen,
  wpt_uint8**            pMsgBuffer, 
  wpt_uint16*            pusDataOffset, 
  wpt_uint16*            pusBufferSize
)
{
  tHalMsgHeader  halMsgHeader;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /*!! No sanity check here as we trust the called - ! check this assumption 
    again*/

  /*-------------------------------------------------------------------------
     Try to allocate message buffer from PAL 
  -------------------------------------------------------------------------*/
  *pusBufferSize = sizeof(halMsgHeader) + usBufferLen; 
  *pMsgBuffer   = (wpt_uint8*)wpalMemoryAllocate(*pusBufferSize);
  if ( NULL ==  *pMsgBuffer )
  {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "Unable to allocate message buffer for req %s (%d)",
               WDI_getReqMsgString(wdiReqType),
               wdiReqType); 
     WDI_ASSERT(0);
     return WDI_STATUS_MEM_FAILURE; 
  }

  /*-------------------------------------------------------------------------
     Fill in the message header
  -------------------------------------------------------------------------*/
  halMsgHeader.msgType = WDI_2_HAL_REQ_TYPE(wdiReqType); 
  halMsgHeader.msgLen  = sizeof(halMsgHeader) + usBufferLen; 
  *pusDataOffset       = sizeof(halMsgHeader); 
  wpalMemoryCopy(*pMsgBuffer, &halMsgHeader, sizeof(halMsgHeader)); 

  return WDI_STATUS_SUCCESS; 
}/*WDI_GetMessageBuffer*/


/**
 @brief Send message helper function - sends a message over the 
        bus using the control tranport and saves some info in
        the CB 
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pSendBuffer:     buffer to be sent
  
         usSendSize          size of the buffer to be sent
         pRspCb:            response callback - save in the WDI
         CB
         pUserData:         user data associated with the
         callback
         wdiExpectedResponse: the code of the response that is
         expected to be rx-ed for this request
  
 @see
 @return Result of the function call
*/
WDI_Status 
WDI_SendMsg
( 
  WDI_ControlBlockType*  pWDICtx,  
  wpt_uint8*             pSendBuffer, 
  wpt_uint32             usSendSize, 
  void*                  pRspCb, 
  void*                  pUserData,
  WDI_ResponseEnumType   wdiExpectedResponse
)
{
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*------------------------------------------------------------------------
    Save needed info in the CB 
  ------------------------------------------------------------------------*/
  pWDICtx->pRspCBUserData      = pUserData;
  pWDICtx->pfncRspCB           = pRspCb; 
  pWDICtx->wdiExpectedResponse = wdiExpectedResponse; 

   /*-----------------------------------------------------------------------
     Call the CTS to send this message over - free message afterwards
     - notify transport failure
     Note: CTS is reponsible for freeing the message buffer.
   -----------------------------------------------------------------------*/
   if ( 0 != WCTS_SendMessage( pWDICtx->wctsHandle, (void*)pSendBuffer, usSendSize ))
   {
      WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                "Failed to send message over the bus - catastrophic failure");

      /*Inform Upper MAC that the request could not go through*/
      if ( NULL != pWDICtx->wdiReqStatusCB )
      {
        pWDICtx->wdiReqStatusCB( WDI_STATUS_E_FAILURE, 
                                 pWDICtx->pReqStatusUserData); 
      }

      /*Free the buffer to prevent memory leak*/
      /*wpalMemoryFree( pSendBuffer);
        WCTS API takes ownership of this pointer and it is therefore in charge
        of freeing it
      **/
      WDI_DetectedDeviceError( pWDICtx, WDI_ERR_TRANSPORT_FAILURE);
      return WDI_STATUS_E_FAILURE;
   }

   /*Inform Upper MAC that the request went through*/
   if ( NULL != pWDICtx->wdiReqStatusCB )
   {
     pWDICtx->wdiReqStatusCB( WDI_STATUS_SUCCESS, pWDICtx->pReqStatusUserData); 
   }

   /*Start timer for the expected response */
   wpalTimerStart(&pWDICtx->wptResponseTimer, WDI_RESPONSE_TIMEOUT);

   return WDI_STATUS_SUCCESS; 
}/*WDI_SendMsg*/



/**
 @brief Send indication helper function - sends a message over 
        the bus using the control transport and saves some info
        in the CB
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pSendBuffer:     buffer to be sent
         usSendSize: size of the buffer to be sent
  
 @see
 @return Result of the function call
*/
WDI_Status 
WDI_SendIndication
( 
  WDI_ControlBlockType*  pWDICtx,  
  wpt_uint8*             pSendBuffer, 
  wpt_uint32             usSendSize
)
{
   wpt_uint32 uStatus ;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

   /*-----------------------------------------------------------------------
     Call the CTS to send this message over 
     Note: CTS is reponsible for freeing the message buffer.
   -----------------------------------------------------------------------*/
   uStatus = WCTS_SendMessage( pWDICtx->wctsHandle, 
                               (void*)pSendBuffer, usSendSize );

   /*Inform Upper MAC about the outcome of the request*/
   if ( NULL != pWDICtx->wdiReqStatusCB )
   {
      WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
                "Send indication status : %d", uStatus);

      pWDICtx->wdiReqStatusCB( (uStatus != 0 ) ? WDI_STATUS_E_FAILURE:
                                                 WDI_STATUS_SUCCESS, 
                               pWDICtx->pReqStatusUserData); 
   }

   /*If sending of the message failed - it is considered catastrophic and
     indicates an error with the device*/
   if ( 0 != uStatus)
   {
      WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_FATAL,
                "Failed to send indication over the bus - catastrophic failure"); 

      WDI_DetectedDeviceError( pWDICtx, WDI_ERR_TRANSPORT_FAILURE);
      return WDI_STATUS_E_FAILURE;
   }

   return WDI_STATUS_SUCCESS; 
}/*WDI_SendIndication*/


/**
 @brief WDI_DetectedDeviceError - called internally by DAL when 
        it has detected a failure in the device 
 
 @param  pWDICtx:        pointer to the WLAN DAL context 
         usErrorCode:    error code detected by WDI or received
                         from HAL
  
 @see
 @return None 
*/
void
WDI_DetectedDeviceError
(
  WDI_ControlBlockType*  pWDICtx,
  wpt_uint16             usErrorCode
)
{
  WDI_LowLevelIndType  wdiInd;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
            "Device Error detected code: %d - transitioning to stopped state",
            usErrorCode);

  wpalMutexAcquire(&pWDICtx->wptMutex);

  WDI_STATableStop(pWDICtx);

  WDI_ResetAssocSessions(pWDICtx);

  /*Set the expected state transition to stopped - because the device
  experienced a failure*/
  pWDICtx->ucExpectedStateTransition =  WDI_STOPPED_ST;

  /*Transition to stopped to fail all incomming requests from this point on*/
  WDI_STATE_TRANSITION( pWDICtx, WDI_STOPPED_ST); 

  WDI_ClearPendingRequests(pWDICtx); 

  /*TO DO: -  there should be an attempt to reset the device here*/

  wpalMutexRelease(&pWDICtx->wptMutex);

  /*------------------------------------------------------------------------
    Notify UMAC if a handler is registered
  ------------------------------------------------------------------------*/
  if (pWDICtx->wdiLowLevelIndCB)
  {
     wdiInd.wdiIndicationType             = WDI_FATAL_ERROR_IND; 
     wdiInd.wdiIndicationData.usErrorCode = usErrorCode; 

     pWDICtx->wdiLowLevelIndCB( &wdiInd,  pWDICtx->pIndUserData);
  }
}/*WDI_DetectedDeviceError*/

/**
 @brief    This callback is invoked by the wpt when a timer that 
           we started on send message has expire - this should
           never happen - it means device is stuck and cannot
           reply - trigger catastrophic failure 
 @param 
    
    pUserData: the callback data of the user (ptr to WDI CB)
    
 @see 
 @return None 
*/
void 
WDI_ResponseTimerCB
(
  void *pUserData
)
{
  WDI_ControlBlockType*  pWDICtx = (WDI_ControlBlockType*)pUserData; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  if (NULL == pWDICtx )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                "%s: Invalid parameters", __FUNCTION__);
    WDI_ASSERT(0);
    return; 
  }

  if ( WDI_MAX_RESP != pWDICtx->wdiExpectedResponse )
  {

  WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
            "Timeout occurred while waiting for %s (%d) message from device "
            " - catastrophic failure", 
            WDI_getRespMsgString(pWDICtx->wdiExpectedResponse),
            pWDICtx->wdiExpectedResponse);
  /* WDI timeout means Riva is not responding or SMD communication to Riva
   * is not happening. The only possible way to recover from this error
   * is to initiate SSR from APPS */
  wpalRivaSubystemRestart();
  }
  else
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "Timeout occurred but not waiting for any response %d", 
                 pWDICtx->wdiExpectedResponse);
  }

  return; 

}/*WDI_ResponseTimerCB*/


/**
 @brief Process response helper function 

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WPT_INLINE WDI_Status
WDI_ProcessResponse
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  /* Skip sanity check as this is called from the FSM functions which 
    already checked these pointers
    ! - revisit this assumption */
  if (( pEventData->wdiResponse < WDI_MAX_RESP ) &&
      ( NULL != pfnRspProcTbl[pEventData->wdiResponse] ))
  {  
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
              "Calling response processing function for resp %s (%d) %x", 
              WDI_getRespMsgString(pEventData->wdiResponse),
              pEventData->wdiResponse, pfnRspProcTbl[pEventData->wdiResponse]);
    return pfnRspProcTbl[pEventData->wdiResponse](pWDICtx, pEventData);
  }
  else
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Operation %d is not yet implemented ", 
              pEventData->wdiResponse);
    return WDI_STATUS_E_NOT_IMPLEMENT;
  }
}/*WDI_ProcessResponse*/


/*=========================================================================
                   QUEUE SUPPORT UTILITY FUNCTIONS 
=========================================================================*/

/**
 @brief    Utility function used by the DAL Core to help queue a 
           request that cannot be processed right away. 
 @param 
    
    pWDICtx: - pointer to the WDI control block
    pEventData: - pointer to the evnt info that needs to be
    queued 
    
 @see 
 @return Result of the operation  
*/
WDI_Status
WDI_QueuePendingReq
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  wpt_list_node*      pNode; 
  WDI_EventInfoType*  pEventDataQueue = wpalMemoryAllocate(sizeof(*pEventData));
  void*               pEventInfo = NULL; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  if ( NULL ==  pEventDataQueue )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Cannot allocate memory for queueing"); 
    WDI_ASSERT(0);
    return WDI_STATUS_MEM_FAILURE;
  }

  pEventDataQueue->pCBfnc          = pEventData->pCBfnc;
  pEventDataQueue->pUserData       = pEventData->pUserData;
  pEventDataQueue->uEventDataSize  = pEventData->uEventDataSize;
  pEventDataQueue->wdiRequest      = pEventData->wdiRequest;
  pEventDataQueue->wdiResponse     = pEventData->wdiResponse; 

  if( pEventData->uEventDataSize != 0 && pEventData->pEventData != NULL )
  {
     pEventInfo = wpalMemoryAllocate(pEventData->uEventDataSize);
   
     if ( NULL ==  pEventInfo )
     {
       WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
                 "Cannot allocate memory for queueing event data info"); 
       WDI_ASSERT(0);
       wpalMemoryFree(pEventDataQueue);
       return WDI_STATUS_MEM_FAILURE;
     }
   
     wpalMemoryCopy(pEventInfo, pEventData->pEventData, pEventData->uEventDataSize);

  }
  pEventDataQueue->pEventData = pEventInfo;

  /*Send wpt a pointer to the node (this is the 1st element in the event data)*/
  pNode = (wpt_list_node*)pEventDataQueue; 

  wpal_list_insert_back(&(pWDICtx->wptPendingQueue), pNode); 

  return WDI_STATUS_SUCCESS;
}/*WDI_QueuePendingReq*/

/**
 @brief    Callback function for serializing queued message 
           processing in the control context
 @param 
    
    pMsg - pointer to the message 
    
 @see 
 @return Result of the operation  
*/
void 
WDI_PALCtrlMsgCB
(
 wpt_msg *pMsg
)
{
  WDI_EventInfoType*     pEventData = NULL;
  WDI_ControlBlockType*  pWDICtx    = NULL; 
  WDI_Status             wdiStatus; 
  WDI_ReqStatusCb        pfnReqStatusCB; 
  void*                  pUserData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  if (( NULL == pMsg )||
      ( NULL == (pEventData = (WDI_EventInfoType*)pMsg->ptr)) ||
      ( NULL == (pWDICtx  = (WDI_ControlBlockType*)pMsg->pContext )))
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "Invalid message received on serialize ctrl context API"); 
    WDI_ASSERT(0);
    return; 
  }

  /*Transition back to the state that we had before serialization
  - serialization transitions us to BUSY to stop any incomming requests
  ! TO DO L: possible race condition here if a request comes in between the
   state transition and the post function*/

  WDI_STATE_TRANSITION( pWDICtx, pMsg->val); 

  /*-----------------------------------------------------------------------
     Check to see what type of event we are serializing
     - responses are never expected to come through here 
  -----------------------------------------------------------------------*/
  switch ( pEventData->wdiRequest )
  {
     
  case WDI_STOP_REQ:
    wdiStatus = WDI_PostMainEvent(&gWDICb, WDI_STOP_EVENT, pEventData);
    break;
  
  default: 
    wdiStatus = WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, pEventData);
    break;
  }/*switch ( pEventData->wdiRequest )*/

  if ( WDI_STATUS_SUCCESS != wdiStatus  )
  {
    WDI_ExtractRequestCBFromEvent(pEventData, &pfnReqStatusCB, &pUserData);

    if ( NULL != pfnReqStatusCB )
    {
      /*Fail the request*/
      pfnReqStatusCB( wdiStatus, pUserData);
    }
  }

  /* Free data - that was allocated when queueing*/
  if( pEventData != NULL )
  {
     if( pEventData->pEventData != NULL )
     {
        wpalMemoryFree(pEventData->pEventData);
     }
     wpalMemoryFree(pEventData);
  }

  if( pMsg != NULL )
  {
     wpalMemoryFree(pMsg);
  }
  
}/*WDI_PALCtrlMsgCB*/

/**
 @brief    Utility function used by the DAL Core to help dequeue
           and schedule for execution a pending request 
 @param 
    
    pWDICtx: - pointer to the WDI control block
    pEventData: - pointer to the evnt info that needs to be
    queued 
    
 @see 
 @return Result of the operation  
*/
WDI_Status
WDI_DequeuePendingReq
(
  WDI_ControlBlockType*  pWDICtx
)
{
  wpt_list_node*      pNode      = NULL; 
  WDI_EventInfoType*  pEventData;
  wpt_msg*            palMsg; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  wpal_list_remove_front(&(pWDICtx->wptPendingQueue), &pNode); 

  if ( NULL ==  pNode )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
              "List is empty - return"); 
    return WDI_STATUS_SUCCESS;
  }

  /*The node actually points to the 1st element inside the Event Data struct -
    just cast it back to the struct*/
  pEventData = (WDI_EventInfoType*)pNode; 

  /*Serialize processing in the control thread
     !TO DO: - check to see if these are all the messages params that need
     to be filled in*/
  palMsg = wpalMemoryAllocate(sizeof(wpt_msg));

  if ( NULL ==  palMsg )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WDI_DequeuePendingReq: Cannot allocate memory for palMsg."); 
    WDI_ASSERT(0);
    return WDI_STATUS_MEM_FAILURE; 
  }
  palMsg->pContext = pWDICtx; 
  palMsg->callback = WDI_PALCtrlMsgCB;
  palMsg->ptr      = pEventData;

  /*Save the global state as we need it on the other side*/
  palMsg->val      = pWDICtx->uGlobalState; 
    
  /*Transition back to BUSY as we need to handle a queued request*/
  WDI_STATE_TRANSITION( pWDICtx, WDI_BUSY_ST);
  
  wpalPostCtrlMsg(pWDICtx->pPALContext, palMsg);

  return WDI_STATUS_PENDING;
}/*WDI_DequeuePendingReq*/


/**
 @brief    Utility function used by the DAL Core to help queue 
           an association request that cannot be processed right
           away.- The assoc requests will be queued by BSSID 
 @param 
    
    pWDICtx: - pointer to the WDI control block
    pEventData: pointer to the evnt info that needs to be queued
    macBSSID: bssid
    
 @see 
 @return Result of the operation  
*/
WDI_Status
WDI_QueueNewAssocRequest
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData,
  wpt_macAddr            macBSSID
)
{
  wpt_uint8 i; 
  WDI_BSSSessionType*     pSession = NULL; 
  wpt_list_node*          pNode; 
  WDI_EventInfoType*      pEventDataQueue;
  void*                   pEventInfo; 
  WDI_NextSessionIdType*  pSessionIdElement; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  

  /*------------------------------------------------------------------------ 
      Search for a session that matches the BSSID 
    ------------------------------------------------------------------------*/
  for ( i = 0; i < WDI_MAX_BSS_SESSIONS; i++ )
  {
     if ( eWLAN_PAL_FALSE == pWDICtx->aBSSSessions[i].bInUse )
     {
       /*Found an empty session*/
       pSession = &pWDICtx->aBSSSessions[i]; 
       break; 
     }
  }

  if ( i >=  WDI_MAX_BSS_SESSIONS )
  {
    /*Cannot find any empty sessions*/
    return WDI_STATUS_E_FAILURE; 
  }
  
  /*------------------------------------------------------------------------
    Fill in the BSSID for this session and set the usage flag
  ------------------------------------------------------------------------*/
  wpalMemoryCopy(pWDICtx->aBSSSessions[i].macBSSID, macBSSID, WDI_MAC_ADDR_LEN);
  pWDICtx->aBSSSessions[i].bInUse = eWLAN_PAL_TRUE; 

  /*------------------------------------------------------------------------
    Allocate memory for this and place it in the queue 
  ------------------------------------------------------------------------*/
  pEventDataQueue = (WDI_EventInfoType*)wpalMemoryAllocate(sizeof(WDI_EventInfoType));
  if ( NULL == pEventDataQueue )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "%s: Cannot allocate memory for queue node", __FUNCTION__);
    WDI_ASSERT(0);
    return WDI_STATUS_MEM_FAILURE;
  }

  pSessionIdElement = (WDI_NextSessionIdType*)wpalMemoryAllocate(sizeof(WDI_NextSessionIdType));
  if ( NULL == pSessionIdElement )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "%s: Cannot allocate memory for session ID", __FUNCTION__);
    WDI_ASSERT(0);
    wpalMemoryFree(pEventDataQueue);
    return WDI_STATUS_MEM_FAILURE;
  }

  pEventInfo = wpalMemoryAllocate(pEventData->uEventDataSize);
  if ( NULL == pEventInfo )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "%s: Cannot allocate memory for event data info", __FUNCTION__);
    WDI_ASSERT(0);
    wpalMemoryFree(pSessionIdElement);
    wpalMemoryFree(pEventDataQueue);
    return WDI_STATUS_MEM_FAILURE;
  }

  pEventDataQueue->pCBfnc          = pEventData->pCBfnc;
  pEventDataQueue->pUserData       = pEventData->pUserData;
  pEventDataQueue->uEventDataSize  = pEventData->uEventDataSize;
  pEventDataQueue->wdiRequest      = pEventData->wdiRequest;
  pEventDataQueue->wdiResponse     = pEventData->wdiResponse; 

  wpalMemoryCopy(pEventInfo, pEventData->pEventData, pEventData->uEventDataSize);
  pEventDataQueue->pEventData = pEventInfo;

  /*Send wpt a pointer to the node (this is the 1st element in the event data)*/
  pNode = (wpt_list_node*)pEventDataQueue; 

  /*This association is currently being queued*/
  pSession->bAssocReqQueued = eWLAN_PAL_TRUE; 

  wpal_list_insert_back(&(pSession->wptPendingQueue), pNode); 

  /*We need to maintain a separate list that keeps track of the order in which
  the new assoc requests are being queued such that we can start processing
  them in the order that they had arrived*/
  pSessionIdElement->ucIndex = i; 
  pNode = (wpt_list_node*)pSessionIdElement; 

  WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
       "Queueing up new assoc session : %d ", pSessionIdElement->ucIndex);
  wpal_list_insert_back(&pWDICtx->wptPendingAssocSessionIdQueue, pNode); 

  /*Return pending as this is what the status of the request is since it has
    been queued*/
  return WDI_STATUS_PENDING;
}/*WDI_QueueNewAssocRequest*/

/**
 @brief    Utility function used by the DAL Core to help queue 
           an association request that cannot be processed right
           away.- The assoc requests will be queued by BSSID 
 @param 
    
    pWDICtx: - pointer to the WDI control block
    pSession: - session in which to queue
    pEventData: pointer to the event info that needs to be
    queued
    
 @see 
 @return Result of the operation  
*/
WDI_Status
WDI_QueueAssocRequest
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_BSSSessionType*    pSession,
  WDI_EventInfoType*     pEventData
)
{
  wpt_list_node*      pNode; 
  WDI_EventInfoType*  pEventDataQueue;
  void*               pEventInfo; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  
  /*------------------------------------------------------------------------ 
      Sanity check
    ------------------------------------------------------------------------*/
  if (( NULL == pSession ) || ( NULL == pWDICtx ))
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                "%s: Invalid parameters", __FUNCTION__);

    return WDI_STATUS_E_FAILURE; 
  }

  /*------------------------------------------------------------------------
    Allocate memory for this and place it in the queue 
  ------------------------------------------------------------------------*/
  pEventDataQueue = (WDI_EventInfoType*)wpalMemoryAllocate(sizeof(WDI_EventInfoType));
  if ( NULL ==  pEventDataQueue )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "%s: Cannot allocate memory for queueing", __FUNCTION__); 
    WDI_ASSERT(0);
    return WDI_STATUS_MEM_FAILURE;
  }

  pEventInfo = wpalMemoryAllocate(pEventData->uEventDataSize);
  if ( NULL ==  pEventInfo )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "%s: Cannot allocate memory for queueing event data info",
               __FUNCTION__);
    WDI_ASSERT(0);
    wpalMemoryFree(pEventDataQueue);
    return WDI_STATUS_MEM_FAILURE;
  }

  pEventDataQueue->pCBfnc          = pEventData->pCBfnc;
  pEventDataQueue->pUserData       = pEventData->pUserData;
  pEventDataQueue->uEventDataSize  = pEventData->uEventDataSize;
  pEventDataQueue->wdiRequest      = pEventData->wdiRequest;
  pEventDataQueue->wdiResponse     = pEventData->wdiResponse; 
  pEventDataQueue->pEventData      = pEventInfo;

  wpalMemoryCopy(pEventInfo, pEventData->pEventData, pEventData->uEventDataSize);

  /*Send wpt a pointer to the node (this is the 1st element in the event data)*/
  pNode = (wpt_list_node*)pEventDataQueue; 

  /*This association is currently being queued*/
  pSession->bAssocReqQueued = eWLAN_PAL_TRUE; 

  wpal_list_insert_back(&(pSession->wptPendingQueue), pNode); 

  /*The result of this operation is pending because the request has been
    queued and it will be processed at a later moment in time */
  return WDI_STATUS_PENDING;
}/*WDI_QueueAssocRequest*/

/**
 @brief    Utility function used by the DAL Core to help dequeue
           an association request that was pending
           The request will be queued up in front of the main
           pending queue for imediate processing
 @param 
    
    pWDICtx: - pointer to the WDI control block
  
    
 @see 
 @return Result of the operation  
*/
WDI_Status
WDI_DequeueAssocRequest
(
  WDI_ControlBlockType*  pWDICtx
)
{
  wpt_list_node*          pNode = NULL; 
  WDI_NextSessionIdType*  pSessionIdElement; 
  WDI_BSSSessionType*     pSession;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  
  /*------------------------------------------------------------------------ 
      Sanity check
    ------------------------------------------------------------------------*/
  if ( NULL == pWDICtx )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                "%s: Invalid parameters", __FUNCTION__);

    return WDI_STATUS_E_FAILURE; 
  }

  /*------------------------------------------------------------------------
    An association has been completed => a new association can occur
    Check to see if there are any pending associations ->
    If so , transfer all the pending requests into the busy queue for
    processing
    These requests have arrived prior to the requests in the busy queue
    (bc they needed to be processed in order to be placed in this queue)
    => they will be placed at the front of the busy queue
  ------------------------------------------------------------------------*/
  wpal_list_remove_front(&(pWDICtx->wptPendingAssocSessionIdQueue), &pNode); 

  if ( NULL ==  pNode )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
              "List is empty - return"); 
    return WDI_STATUS_SUCCESS;
  }

  /*The node actually points to the 1st element inside the Session Id struct -
    just cast it back to the struct*/
  pSessionIdElement = (WDI_NextSessionIdType*)pNode; 

  WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
       "Dequeueing new assoc session : %d ", pSessionIdElement->ucIndex);

  if ( pSessionIdElement->ucIndex < WDI_MAX_BSS_SESSIONS )
  {
      pSession = &pWDICtx->aBSSSessions[pSessionIdElement->ucIndex];
      
      /*Transfer all the pending requests in this assoc queue to
      the front of the main waiting queue for subsequent execution*/      
      wpal_list_remove_back(&(pSession->wptPendingQueue), &pNode); 
      while ( NULL !=  pNode )
      {
        /*Place it in front of the main pending list*/
        wpal_list_insert_front( &(pWDICtx->wptPendingQueue), &pNode); 
        wpal_list_remove_back(&(pSession->wptPendingQueue), &pNode); 
      }
  }
  else
  {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_FATAL,
              "Invalid session id queued up for assoc"); 
     WPAL_ASSERT(0);
     wpalMemoryFree(pSessionIdElement);
     return WDI_STATUS_E_FAILURE; 
  }
  
  /*Clean this up as it is no longer needed in order to prevent memory leak*/
  wpalMemoryFree(pSessionIdElement);
  return WDI_STATUS_SUCCESS;
}/*WDI_DequeueAssocRequest*/

/**
 @brief    Utility function used by the DAL Core to clear any 
           pending requests - all req cb will be called with
           failure and the queue will be emptied.
 @param 
    
    pWDICtx: - pointer to the WDI control block
    
 @see 
 @return Result of the operation  
*/
WDI_Status
WDI_ClearPendingRequests
( 
  WDI_ControlBlockType*  pWDICtx
)
{
  wpt_list_node*      pNode = NULL; 
  WDI_EventInfoType*  pEventDataQueue = NULL;
  WDI_ReqStatusCb     pfnReqStatusCB; 
  void*               pUserData;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  wpal_list_remove_front(&(pWDICtx->wptPendingQueue), &pNode); 

  /*------------------------------------------------------------------------
    Go through all the requests and fail them - this will only be called
    when device is being stopped or an error was detected - either case the
    pending requests can no longer be sent down to HAL 
  ------------------------------------------------------------------------*/
  while( pNode )
  {
      /*The node actually points to the 1st element inside the Event Data struct -
    just cast it back to the struct*/
    pEventDataQueue = (WDI_EventInfoType*)pNode; 
  
    WDI_ExtractRequestCBFromEvent(pEventDataQueue, &pfnReqStatusCB, &pUserData);
    if ( NULL != pfnReqStatusCB )
    {
      /*Fail the request*/
      pfnReqStatusCB( WDI_STATUS_E_FAILURE, pUserData);
    }
    /* Free data - that was allocated when queueing */
    if ( pEventDataQueue->pEventData != NULL )
    {
      wpalMemoryFree(pEventDataQueue->pEventData);
    }
    wpalMemoryFree(pEventDataQueue);

    if (wpal_list_remove_front(&(pWDICtx->wptPendingQueue), &pNode) !=  eWLAN_PAL_STATUS_SUCCESS)
    {
        break;
    }
  } 
 
  return WDI_STATUS_SUCCESS;
}/*WDI_ClearPendingRequests*/

/**
 @brief Helper routine used to init the BSS Sessions in the WDI control block 
  
 
 @param  pWDICtx:       pointer to the WLAN DAL context 
  
 @see
*/
void
WDI_ResetAssocSessions
( 
  WDI_ControlBlockType*   pWDICtx
)
{
  wpt_uint8 i; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*-------------------------------------------------------------------------
    No Sanity check
  -------------------------------------------------------------------------*/
  for ( i = 0; i < WDI_MAX_BSS_SESSIONS; i++ )
  {
    wpalMemoryZero( &pWDICtx->aBSSSessions[i], sizeof(WDI_BSSSessionType) ); 
    pWDICtx->aBSSSessions[i].wdiAssocState = WDI_ASSOC_INIT_ST;
    pWDICtx->aBSSSessions[i].bcastStaIdx = WDI_STA_INVALID_IDX;
    pWDICtx->aBSSSessions[i].ucBSSIdx = WDI_BSS_INVALID_IDX;
  }
}/*WDI_ResetAssocSessions*/

/**
 @brief Helper routine used to find a session based on the BSSID 
  
 
 @param  pWDICtx:       pointer to the WLAN DAL context 
         macBSSID:      BSSID of the session
         pSession:      pointer to the session (if found) 
  
 @see
 @return Index of the session in the array 
*/
wpt_uint8
WDI_FindAssocSession
( 
  WDI_ControlBlockType*   pWDICtx,
  wpt_macAddr             macBSSID,
  WDI_BSSSessionType**    ppSession
)
{
  wpt_uint8 i; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if ( NULL == ppSession )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                "%s: Invalid parameters", __FUNCTION__);
     return WDI_MAX_BSS_SESSIONS; 
  }

  *ppSession = NULL; 

  /*------------------------------------------------------------------------ 
      Search for a session that matches the BSSID 
    ------------------------------------------------------------------------*/
  for ( i = 0; i < WDI_MAX_BSS_SESSIONS; i++ )
  {
     if ( eWLAN_PAL_TRUE == 
          wpalMemoryCompare(pWDICtx->aBSSSessions[i].macBSSID, macBSSID, WDI_MAC_ADDR_LEN) )
     {
       /*Found the session*/
       *ppSession = &pWDICtx->aBSSSessions[i]; 
       return i;
     }
  }

  return i; 
}/*WDI_FindAssocSession*/

/**
 @brief Helper routine used to find a session based on the BSSID 
  
 
 @param  pWDICtx:   pointer to the WLAN DAL context 
         ucBSSIdx:  BSS Index of the session
         ppSession: out pointer to the session (if found)
  
 @see
 @return Index of the session in the array 
*/
wpt_uint8
WDI_FindAssocSessionByBSSIdx
( 
  WDI_ControlBlockType*   pWDICtx,
  wpt_uint16              ucBSSIdx,
  WDI_BSSSessionType**    ppSession
)
{
  wpt_uint8 i; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if ( NULL == ppSession )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                "%s: Invalid parameters", __FUNCTION__);
     return WDI_MAX_BSS_SESSIONS; 
  }

  *ppSession = NULL; 

  /*------------------------------------------------------------------------ 
      Search for a session that matches the BSSID 
    ------------------------------------------------------------------------*/
  for ( i = 0; i < WDI_MAX_BSS_SESSIONS; i++ )
  {
     if ( ucBSSIdx == pWDICtx->aBSSSessions[i].ucBSSIdx )
     {
       /*Found the session*/
       *ppSession = &pWDICtx->aBSSSessions[i]; 
       return i;
     }
  }

  return i; 
}/*WDI_FindAssocSessionByBSSIdx*/

/**
 @brief Helper routine used to find a session based on the BSSID 
  
 
 @param  pWDICtx:   pointer to the WLAN DAL context 
         ucBSSIdx:  BSS Index of the session
         ppSession: out pointer to the session (if found)
  
 @see
 @return Index of the session in the array 
*/
wpt_uint8
WDI_FindAssocSessionByIdx
( 
  WDI_ControlBlockType*   pWDICtx,
  wpt_uint16              usIdx,
  WDI_BSSSessionType**    ppSession
)
{
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if ( NULL == ppSession || usIdx >= WDI_MAX_BSS_SESSIONS )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                "%s: Invalid parameters", __FUNCTION__);
     return WDI_MAX_BSS_SESSIONS; 
  }

  /*Found the session*/
  *ppSession = &pWDICtx->aBSSSessions[usIdx]; 

  return usIdx;
  
}/*WDI_FindAssocSessionByBSSIdx*/

/**
 @brief Helper routine used to find an empty session in the WDI 
        CB
  
 
 @param  pWDICtx:       pointer to the WLAN DAL context 
         pSession:      pointer to the session (if found) 
  
 @see
 @return Index of the session in the array 
*/
wpt_uint8
WDI_FindEmptySession
( 
  WDI_ControlBlockType*   pWDICtx,
  WDI_BSSSessionType**    ppSession
)
{
  wpt_uint8 i; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
   /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if ( NULL == ppSession )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                "%s: Invalid parameters", __FUNCTION__);
     return WDI_MAX_BSS_SESSIONS; 
  }

  *ppSession = NULL; 

  /*------------------------------------------------------------------------ 
      Search for a session that it is not in use 
    ------------------------------------------------------------------------*/
  for ( i = 0; i < WDI_MAX_BSS_SESSIONS; i++ )
  {
     if ( ! pWDICtx->aBSSSessions[i].bInUse )
     {
       /*Found a session*/
       *ppSession = &pWDICtx->aBSSSessions[i]; 
       return i;
     }
  }

  return i; 
}/*WDI_FindEmptySession*/


/**
 @brief Helper routine used to get the total count of active 
        sessions
  
 
 @param  pWDICtx:       pointer to the WLAN DAL context 
  
 @see
 @return Number of sessions in use
*/
wpt_uint8
WDI_GetActiveSessionsCount
( 
  WDI_ControlBlockType*   pWDICtx
)
{
  wpt_uint8 i, ucCount = 0; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/
  
  /*------------------------------------------------------------------------ 
      Count all sessions in use
    ------------------------------------------------------------------------*/
  for ( i = 0; i < WDI_MAX_BSS_SESSIONS; i++ )
  {
     if ( pWDICtx->aBSSSessions[i].bInUse )
     {
       ucCount++;
     }
  }

  return ucCount; 
}/*WDI_GetActiveSessionsCount*/

/**
 @brief Helper routine used to delete session in the WDI 
        CB
  
 
 @param  pWDICtx:       pointer to the WLAN DAL context 
         pSession:      pointer to the session (if found) 
  
 @see
 @return Index of the session in the array 
*/
void 
WDI_DeleteSession
( 
  WDI_ControlBlockType*   pWDICtx,
  WDI_BSSSessionType*     ppSession
)
{
   /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if ( NULL == ppSession )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                "%s: Invalid parameters", __FUNCTION__);
     return ; 
  }

  /*------------------------------------------------------------------------ 
      Reset the entries int session 
    ------------------------------------------------------------------------*/
  wpal_list_destroy(&ppSession->wptPendingQueue);
  wpalMemoryZero(ppSession,  sizeof(*ppSession));
  ppSession->wdiAssocState = WDI_ASSOC_INIT_ST; 
  ppSession->bInUse        = eWLAN_PAL_FALSE; 
  ppSession->wdiBssType    = WDI_INFRASTRUCTURE_MODE;
  wpal_list_init(&ppSession->wptPendingQueue);

}/*WDI_DeleteSession*/

/**
 @brief    Utility function to add the broadcast STA to the the STA table. 
 The bcast STA ID is assigned by HAL and must be valid.
 @param 
    
    WDI_AddStaParams: - pointer to the WDI Add STA params
    usBcastStaIdx: - Broadcast STA index passed by HAL
    
 @see 
 @return void 
*/
void
WDI_AddBcastSTAtoSTATable
(
  WDI_ControlBlockType*  pWDICtx,
  WDI_AddStaParams *     staParams,
  wpt_uint16             usBcastStaIdx
)
{
  WDI_AddStaParams              wdiAddSTAParam = {0};
  wpt_macAddr  bcastMacAddr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

  /*---------------------------------------------------------------------
    Sanity check
  ---------------------------------------------------------------------*/
  if ( NULL == staParams )
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                "%s: Invalid parameters", __FUNCTION__);

    return; 
  }

  wdiAddSTAParam.bcastDpuIndex = staParams->bcastDpuIndex;
  wdiAddSTAParam.bcastDpuSignature = staParams->bcastDpuSignature;
  wdiAddSTAParam.bcastMgmtDpuIndex = staParams->bcastMgmtDpuIndex;
  wdiAddSTAParam.bcastMgmtDpuSignature = staParams->bcastMgmtDpuSignature;
  wdiAddSTAParam.dpuIndex = staParams->dpuIndex;
  wdiAddSTAParam.dpuSig = staParams->dpuSig;
  wpalMemoryCopy( wdiAddSTAParam.macBSSID, staParams->macBSSID,
                  WDI_MAC_ADDR_LEN );
  wpalMemoryCopy( wdiAddSTAParam.staMacAddr, bcastMacAddr, WDI_MAC_ADDR_LEN );
  wdiAddSTAParam.ucBSSIdx = staParams->ucBSSIdx;
  wdiAddSTAParam.ucHTCapable = staParams->ucHTCapable;
  wdiAddSTAParam.ucRmfEnabled = staParams->ucRmfEnabled;
  wdiAddSTAParam.ucStaType = WDI_STA_ENTRY_BCAST;
  wdiAddSTAParam.ucWmmEnabled = staParams->ucWmmEnabled;
  wdiAddSTAParam.ucSTAIdx = usBcastStaIdx;
    
  (void)WDI_STATableAddSta(pWDICtx,&wdiAddSTAParam);
}

/**
 @brief NV blob will be divided into fragments of size 4kb and 
 Sent to HAL 
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
 */

WDI_Status WDI_SendNvBlobReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{

  tHalNvImgDownloadReqMsg    halNvImgDownloadParam;
  wpt_uint8*                 pSendBuffer   = NULL;
  wpt_uint16                 usDataOffset  = 0;
  wpt_uint16                 usSendSize    = 0;
  wpt_uint16                 usCurrentFragmentSize =0;
  wpt_uint8*                 pSrcBuffer = NULL;
  WDI_NvDownloadReqParamsType*  pwdiNvDownloadReqParams =NULL ;
  WDI_NvDownloadRspCb      wdiNvDownloadRspCb;


  wdiNvDownloadRspCb = (WDI_NvDownloadRspCb)pEventData->pCBfnc;
  WDI_ASSERT(NULL != wdiNvDownloadRspCb);
  pwdiNvDownloadReqParams = (WDI_NvDownloadReqParamsType*)pEventData->pEventData;

  /* Sanity Check is done by the caller */ 
  pSrcBuffer =(wpt_uint8 *) pwdiNvDownloadReqParams->wdiBlobInfo.pBlobAddress;

  /* Update the current  Fragment Number */
  pWDICtx->wdiNvBlobInfo.usCurrentFragment += 1;

  /*Update the HAL REQ structure */
  /*HAL maintaining the fragment count as 0,1,2...n where at WDI it is represented as 1,2,3.. n*/
  halNvImgDownloadParam.nvImageReqParams.fragNumber =
                                     pWDICtx->wdiNvBlobInfo.usCurrentFragment-1;

  /*    Divide the NV Image to size of 'FRAGMENT_SIZE' fragments and send it to HAL.
       If the size of the Image is less than 'FRAGMENT_SIZE' then in one iteration total 
       image will be sent to HAL*/

  if(pWDICtx->wdiNvBlobInfo.usTotalFragment 
                         == pWDICtx->wdiNvBlobInfo.usCurrentFragment)
  { 
    /*     Taking care of boundry condition */
    if( !(usCurrentFragmentSize = 
                 pwdiNvDownloadReqParams->wdiBlobInfo.uBlobSize%FRAGMENT_SIZE ))
      usCurrentFragmentSize = FRAGMENT_SIZE;

    /*Update the HAL REQ structure */
    halNvImgDownloadParam.nvImageReqParams.isLastFragment = 1;
    halNvImgDownloadParam.nvImageReqParams.nvImgBufferSize= usCurrentFragmentSize;

  }
  else
  { 
    usCurrentFragmentSize = FRAGMENT_SIZE;

    /*Update the HAL REQ structure */
    halNvImgDownloadParam.nvImageReqParams.isLastFragment =0;
    halNvImgDownloadParam.nvImageReqParams.nvImgBufferSize = usCurrentFragmentSize;
  }


  /*-----------------------------------------------------------------------
   Get message buffer
   -----------------------------------------------------------------------*/
  if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx,WDI_NV_DOWNLOAD_REQ,
         sizeof(halNvImgDownloadParam.nvImageReqParams)+ usCurrentFragmentSize,
                    &pSendBuffer, &usDataOffset, &usSendSize))||
      ( usSendSize < 
           (usDataOffset + sizeof(halNvImgDownloadParam.nvImageReqParams) + usCurrentFragmentSize )))
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
         "Unable to get send buffer in NV Download req %x %x ",
         pEventData, pwdiNvDownloadReqParams);
    WDI_ASSERT(0);
    return WDI_STATUS_E_FAILURE; 
  }

  /* Copying the Hal NV download REQ structure */
  wpalMemoryCopy(pSendBuffer + usDataOffset , 
    &halNvImgDownloadParam.nvImageReqParams ,sizeof(tHalNvImgDownloadReqParams));

  /* Appending the NV image fragment */
  wpalMemoryCopy(pSendBuffer + usDataOffset + sizeof(tHalNvImgDownloadReqParams),
        (void *)(pSrcBuffer + halNvImgDownloadParam.nvImageReqParams.fragNumber * FRAGMENT_SIZE),
                  usCurrentFragmentSize);

  pWDICtx->wdiReqStatusCB     = pwdiNvDownloadReqParams->wdiReqStatusCB;
  pWDICtx->pReqStatusUserData = pwdiNvDownloadReqParams->pUserData; 

  return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                           wdiNvDownloadRspCb, pEventData->pUserData, 
                           WDI_NV_DOWNLOAD_RESP);

}
/*============================================================================ 
  Helper inline functions for 
 ============================================================================*/
/**
 @brief Helper routine used to find a session based on the BSSID 
 @param  pContext:   pointer to the WLAN DAL context 
 @param  pDPContext:   pointer to the Datapath context 
  
 @see
 @return 
*/
WPT_INLINE void 
WDI_DS_AssignDatapathContext (void *pContext, void *pDPContext)
{
   WDI_ControlBlockType *pCB = (WDI_ControlBlockType *)pContext;

   pCB->pDPContext = pDPContext;
   return;
}

/**
 @brief Helper routine used to find a session based on the BSSID 
  
 
 @param  pContext:   pointer to the WLAN DAL context 
  
 @see
 @return pointer to Datapath context
*/
WPT_INLINE void * 
WDI_DS_GetDatapathContext (void *pContext)
{
   WDI_ControlBlockType *pCB = (WDI_ControlBlockType *)pContext;
   return pCB->pDPContext;
}
/**
 @brief Helper routine used to find a session based on the BSSID 
  
 
 @param  pContext:   pointer to the WLAN DAL context 
 @param  pDTDriverContext:   pointer to the Transport Driver context 
  
 @see
 @return void
*/
WPT_INLINE void  
WDT_AssignTransportDriverContext (void *pContext, void *pDTDriverContext)
{
   WDI_ControlBlockType *pCB = (WDI_ControlBlockType *)pContext;

   pCB->pDTDriverContext = pDTDriverContext;
   return; 
}

/**
 @brief Helper routine used to find a session based on the BSSID 
  
 
 @param  pWDICtx:   pointer to the WLAN DAL context 
  
 @see
 @return pointer to datapath context 
*/
WPT_INLINE void * 
WDT_GetTransportDriverContext (void *pContext)
{
   WDI_ControlBlockType *pCB = (WDI_ControlBlockType *)pContext;
   return(pCB->pDTDriverContext); 
}

/*============================================================================ 
  Helper inline converters
 ============================================================================*/
/*Convert WDI driver type into HAL driver type*/
WPT_STATIC WPT_INLINE WDI_Status
WDI_HAL_2_WDI_STATUS
(
  eHalStatus halStatus
)
{
  /*Lightweight function - no sanity checks and no unecessary code to increase 
    the chances of getting inlined*/
  switch(  halStatus )
  {
  case eHAL_STATUS_SUCCESS:
  case eHAL_STATUS_ADD_STA_SELF_IGNORED_REF_COUNT_NOT_ZERO:
  case eHAL_STATUS_DEL_STA_SELF_IGNORED_REF_COUNT_NOT_ZERO:
    return WDI_STATUS_SUCCESS;
  case eHAL_STATUS_FAILURE:
    return WDI_STATUS_E_FAILURE;
  case eHAL_STATUS_FAILED_ALLOC:
    return WDI_STATUS_MEM_FAILURE;    
   /*The rest of the HAL error codes must be kept hidden from the UMAC as 
     they refer to specific internal modules of our device*/
  default: 
    return WDI_STATUS_DEV_INTERNAL_FAILURE; 
  } 

  return WDI_STATUS_E_FAILURE; 
}/*WDI_HAL_2_WDI_STATUS*/

/*Convert WDI request type into HAL request type*/
WPT_STATIC WPT_INLINE tHalHostMsgType
WDI_2_HAL_REQ_TYPE
(
  WDI_RequestEnumType    wdiReqType
)
{
   /*Lightweight function - no sanity checks and no unecessary code to increase 
    the chances of getting inlined*/
  switch(  wdiReqType )
  {    
  case WDI_START_REQ:
    return WLAN_HAL_START_REQ; 
  case WDI_STOP_REQ:
    return WLAN_HAL_STOP_REQ; 
  case WDI_INIT_SCAN_REQ:
    return WLAN_HAL_INIT_SCAN_REQ; 
  case WDI_START_SCAN_REQ:
    return WLAN_HAL_START_SCAN_REQ; 
  case WDI_END_SCAN_REQ:
    return WLAN_HAL_END_SCAN_REQ; 
  case WDI_FINISH_SCAN_REQ:
    return WLAN_HAL_FINISH_SCAN_REQ; 
  case WDI_JOIN_REQ:
    return WLAN_HAL_JOIN_REQ; 
  case WDI_CONFIG_BSS_REQ:
    return WLAN_HAL_CONFIG_BSS_REQ; 
  case WDI_DEL_BSS_REQ:
    return WLAN_HAL_DELETE_BSS_REQ; 
  case WDI_POST_ASSOC_REQ:
    return WLAN_HAL_POST_ASSOC_REQ; 
  case WDI_DEL_STA_REQ:
    return WLAN_HAL_DELETE_STA_REQ; 
  case WDI_SET_BSS_KEY_REQ:
    return WLAN_HAL_SET_BSSKEY_REQ; 
  case WDI_RMV_BSS_KEY_REQ:
    return WLAN_HAL_RMV_BSSKEY_REQ; 
  case WDI_SET_STA_KEY_REQ:
    return WLAN_HAL_SET_STAKEY_REQ; 
  case WDI_RMV_STA_KEY_REQ:
    return WLAN_HAL_RMV_STAKEY_REQ; 
  case WDI_SET_STA_BCAST_KEY_REQ:
    return WLAN_HAL_SET_BCASTKEY_REQ; 
  case WDI_RMV_STA_BCAST_KEY_REQ:
    //Some conflict in the old code - check this: return WLAN_HAL_RMV_BCASTKEY_REQ; 
    return WLAN_HAL_RMV_STAKEY_REQ;
  case WDI_ADD_TS_REQ:
    return WLAN_HAL_ADD_TS_REQ; 
  case WDI_DEL_TS_REQ:
    return WLAN_HAL_DEL_TS_REQ; 
  case WDI_UPD_EDCA_PRMS_REQ:
    return WLAN_HAL_UPD_EDCA_PARAMS_REQ; 
  case WDI_ADD_BA_REQ:
    return WLAN_HAL_ADD_BA_REQ; 
  case WDI_DEL_BA_REQ:
    return WLAN_HAL_DEL_BA_REQ; 
  case WDI_CH_SWITCH_REQ:
    return WLAN_HAL_CH_SWITCH_REQ; 
  case WDI_CONFIG_STA_REQ:
    return WLAN_HAL_CONFIG_STA_REQ; 
  case WDI_SET_LINK_ST_REQ:
    return WLAN_HAL_SET_LINK_ST_REQ; 
  case WDI_GET_STATS_REQ:
    return WLAN_HAL_GET_STATS_REQ; 
  case WDI_UPDATE_CFG_REQ:
    return WLAN_HAL_UPDATE_CFG_REQ; 
  case WDI_ADD_BA_SESSION_REQ:
    return WLAN_HAL_ADD_BA_SESSION_REQ;
  case WDI_TRIGGER_BA_REQ:
    return WLAN_HAL_TRIGGER_BA_REQ;
  case WDI_UPD_BCON_PRMS_REQ:
    return WLAN_HAL_UPDATE_BEACON_REQ; 
  case WDI_SND_BCON_REQ:
    return WLAN_HAL_SEND_BEACON_REQ; 
  case WDI_UPD_PROBE_RSP_TEMPLATE_REQ:
    return WLAN_HAL_UPDATE_PROBE_RSP_TEMPLATE_REQ;
   case WDI_SET_MAX_TX_POWER_REQ:
    return WLAN_HAL_SET_MAX_TX_POWER_REQ;
#ifdef WLAN_FEATURE_P2P
  case WDI_P2P_GO_NOTICE_OF_ABSENCE_REQ:
    return WLAN_HAL_SET_P2P_GONOA_REQ;
#endif
  case WDI_ENTER_IMPS_REQ:
    return WLAN_HAL_ENTER_IMPS_REQ; 
  case WDI_EXIT_IMPS_REQ:
    return WLAN_HAL_EXIT_IMPS_REQ; 
  case WDI_ENTER_BMPS_REQ:
    return WLAN_HAL_ENTER_BMPS_REQ; 
  case WDI_EXIT_BMPS_REQ:
    return WLAN_HAL_EXIT_BMPS_REQ; 
  case WDI_ENTER_UAPSD_REQ:
    return WLAN_HAL_ENTER_UAPSD_REQ; 
  case WDI_EXIT_UAPSD_REQ:
    return WLAN_HAL_EXIT_UAPSD_REQ; 
  case WDI_SET_UAPSD_PARAM_REQ:
    return WLAN_HAL_SET_UAPSD_AC_PARAMS_REQ; 
  case WDI_UPDATE_UAPSD_PARAM_REQ:
    return WLAN_HAL_UPDATE_UAPSD_PARAM_REQ; 
  case WDI_CONFIGURE_RXP_FILTER_REQ:
    return WLAN_HAL_CONFIGURE_RXP_FILTER_REQ; 
  case WDI_SET_BEACON_FILTER_REQ:
    return WLAN_HAL_ADD_BCN_FILTER_REQ; 
  case WDI_REM_BEACON_FILTER_REQ:
    return WLAN_HAL_REM_BCN_FILTER_REQ;
  case WDI_SET_RSSI_THRESHOLDS_REQ:
    return WLAN_HAL_SET_RSSI_THRESH_REQ;
  case WDI_HOST_OFFLOAD_REQ:
    return WLAN_HAL_HOST_OFFLOAD_REQ;
  case WDI_WOWL_ADD_BC_PTRN_REQ:
    return WLAN_HAL_ADD_WOWL_BCAST_PTRN;
  case WDI_WOWL_DEL_BC_PTRN_REQ:
    return WLAN_HAL_DEL_WOWL_BCAST_PTRN;
  case WDI_WOWL_ENTER_REQ:
    return WLAN_HAL_ENTER_WOWL_REQ;
  case WDI_WOWL_EXIT_REQ:
    return WLAN_HAL_EXIT_WOWL_REQ;
  case WDI_CONFIGURE_APPS_CPU_WAKEUP_STATE_REQ:
    return WLAN_HAL_CONFIGURE_APPS_CPU_WAKEUP_STATE_REQ;
   case WDI_NV_DOWNLOAD_REQ:
    return WLAN_HAL_DOWNLOAD_NV_REQ;
  case WDI_FLUSH_AC_REQ:
    return WLAN_HAL_TL_HAL_FLUSH_AC_REQ;
  case WDI_BTAMP_EVENT_REQ:
    return WLAN_HAL_SIGNAL_BTAMP_EVENT_REQ;
#ifdef WLAN_FEATURE_VOWIFI_11R
  case WDI_AGGR_ADD_TS_REQ:
     return WLAN_HAL_AGGR_ADD_TS_REQ;
#endif /* WLAN_FEATURE_VOWIFI_11R */
#ifdef ANI_MANF_DIAG
  case WDI_FTM_CMD_REQ:
    return WLAN_HAL_PROCESS_PTT_REQ;
#endif /* ANI_MANF_DIAG */
  case WDI_ADD_STA_SELF_REQ:
    return WLAN_HAL_ADD_STA_SELF_REQ;
  case WDI_DEL_STA_SELF_REQ:
    return WLAN_HAL_DEL_STA_SELF_REQ;
  case WDI_HOST_RESUME_REQ:
    return WLAN_HAL_HOST_RESUME_REQ;
  case WDI_HOST_SUSPEND_IND:
    return WLAN_HAL_HOST_SUSPEND_IND;
  case WDI_KEEP_ALIVE_REQ:
    return WLAN_HAL_KEEP_ALIVE_REQ;

#ifdef FEATURE_WLAN_SCAN_PNO
  case WDI_SET_PREF_NETWORK_REQ:
    return WLAN_HAL_SET_PREF_NETWORK_REQ;
  case WDI_SET_RSSI_FILTER_REQ:
    return WLAN_HAL_SET_RSSI_FILTER_REQ;
  case WDI_UPDATE_SCAN_PARAMS_REQ:
    return WLAN_HAL_UPDATE_SCAN_PARAM_REQ;
#endif // FEATURE_WLAN_SCAN_PNO
  case WDI_SET_TX_PER_TRACKING_REQ:
    return WLAN_HAL_SET_TX_PER_TRACKING_REQ;
#ifdef WLAN_FEATURE_PACKET_FILTERING
  case WDI_8023_MULTICAST_LIST_REQ:
    return WLAN_HAL_8023_MULTICAST_LIST_REQ;
  case WDI_RECEIVE_FILTER_SET_FILTER_REQ:
    return WLAN_HAL_SET_PACKET_FILTER_REQ; 
  case WDI_PACKET_COALESCING_FILTER_MATCH_COUNT_REQ:
    return WLAN_HAL_PACKET_FILTER_MATCH_COUNT_REQ;
  case WDI_RECEIVE_FILTER_CLEAR_FILTER_REQ:
    return WLAN_HAL_CLEAR_PACKET_FILTER_REQ;
#endif // WLAN_FEATURE_PACKET_FILTERING
  case WDI_HAL_DUMP_CMD_REQ:
    return WLAN_HAL_DUMP_COMMAND_REQ;
  /*This is temporary change and should be 
   * removed once host and riva changes are in sync*/
  case WDI_INIT_SCAN_CON_REQ:
    return WLAN_HAL_INIT_SCAN_CON_REQ; 

  case WDI_SET_POWER_PARAMS_REQ:
    return WLAN_HAL_SET_POWER_PARAMS_REQ; 
  default:
    return WLAN_HAL_MSG_MAX; 
  }
  
}/*WDI_2_HAL_REQ_TYPE*/

/*Convert WDI response type into HAL response type*/
WPT_STATIC WPT_INLINE WDI_ResponseEnumType
HAL_2_WDI_RSP_TYPE
(
  tHalHostMsgType halMsg
)
{
  /*Lightweight function - no sanity checks and no unecessary code to increase 
    the chances of getting inlined*/
  switch(  halMsg )
  {
  case WLAN_HAL_START_RSP:
    return WDI_START_RESP;
  case WLAN_HAL_STOP_RSP:
    return WDI_STOP_RESP;
  case WLAN_HAL_INIT_SCAN_RSP:
    return WDI_INIT_SCAN_RESP;
  case WLAN_HAL_START_SCAN_RSP:
    return WDI_START_SCAN_RESP;
  case WLAN_HAL_END_SCAN_RSP:
    return WDI_END_SCAN_RESP;
  case WLAN_HAL_FINISH_SCAN_RSP:
    return WDI_FINISH_SCAN_RESP;
  case WLAN_HAL_CONFIG_STA_RSP:
    return WDI_CONFIG_STA_RESP;
  case WLAN_HAL_DELETE_STA_RSP:
    return WDI_DEL_STA_RESP;
  case WLAN_HAL_CONFIG_BSS_RSP:
    return WDI_CONFIG_BSS_RESP;
  case WLAN_HAL_DELETE_BSS_RSP:
    return WDI_DEL_BSS_RESP;
  case WLAN_HAL_JOIN_RSP:
    return WDI_JOIN_RESP;
  case WLAN_HAL_POST_ASSOC_RSP:
    return WDI_POST_ASSOC_RESP;
  case WLAN_HAL_SET_BSSKEY_RSP:
    return WDI_SET_BSS_KEY_RESP;
  case WLAN_HAL_SET_STAKEY_RSP:
    return WDI_SET_STA_KEY_RESP;
  case WLAN_HAL_RMV_BSSKEY_RSP:
    return WDI_RMV_BSS_KEY_RESP;
  case WLAN_HAL_RMV_STAKEY_RSP:
    return WDI_RMV_STA_KEY_RESP;
  case WLAN_HAL_SET_BCASTKEY_RSP:
    return WDI_SET_STA_BCAST_KEY_RESP;
  //Some conflict in the old code - check this: case WLAN_HAL_RMV_BCASTKEY_RSP:
  //  return WDI_RMV_STA_BCAST_KEY_RESP;
  case WLAN_HAL_ADD_TS_RSP:
    return WDI_ADD_TS_RESP;
  case WLAN_HAL_DEL_TS_RSP:
    return WDI_DEL_TS_RESP;
  case WLAN_HAL_UPD_EDCA_PARAMS_RSP:
    return WDI_UPD_EDCA_PRMS_RESP;
  case WLAN_HAL_ADD_BA_RSP:
    return WDI_ADD_BA_RESP;
  case WLAN_HAL_DEL_BA_RSP:
    return WDI_DEL_BA_RESP;
  case WLAN_HAL_CH_SWITCH_RSP:
    return WDI_CH_SWITCH_RESP;
  case WLAN_HAL_SET_LINK_ST_RSP:
    return WDI_SET_LINK_ST_RESP;
  case WLAN_HAL_GET_STATS_RSP:
    return WDI_GET_STATS_RESP;
  case WLAN_HAL_UPDATE_CFG_RSP:
    return WDI_UPDATE_CFG_RESP;
  case WLAN_HAL_ADD_BA_SESSION_RSP:
    return WDI_ADD_BA_SESSION_RESP;
  case WLAN_HAL_TRIGGER_BA_RSP:
    return WDI_TRIGGER_BA_RESP;
  case WLAN_HAL_UPDATE_BEACON_RSP:
    return WDI_UPD_BCON_PRMS_RESP;
  case WLAN_HAL_SEND_BEACON_RSP:
    return WDI_SND_BCON_RESP;
  case WLAN_HAL_UPDATE_PROBE_RSP_TEMPLATE_RSP:
    return WDI_UPD_PROBE_RSP_TEMPLATE_RESP;
  /*Indications*/
  case WLAN_HAL_RSSI_NOTIFICATION_IND:
    return WDI_HAL_RSSI_NOTIFICATION_IND;
  case WLAN_HAL_MISSED_BEACON_IND:
    return WDI_HAL_MISSED_BEACON_IND;
  case WLAN_HAL_UNKNOWN_ADDR2_FRAME_RX_IND:
    return WDI_HAL_UNKNOWN_ADDR2_FRAME_RX_IND;
  case WLAN_HAL_MIC_FAILURE_IND:
    return WDI_HAL_MIC_FAILURE_IND;
  case WLAN_HAL_FATAL_ERROR_IND:
    return WDI_HAL_FATAL_ERROR_IND;
  case WLAN_HAL_DELETE_STA_CONTEXT_IND:
    return WDI_HAL_DEL_STA_IND;
  case WLAN_HAL_COEX_IND:
    return WDI_HAL_COEX_IND;
  case WLAN_HAL_OTA_TX_COMPL_IND:
    return WDI_HAL_TX_COMPLETE_IND;
#ifdef WLAN_FEATURE_P2P
  case WLAN_HAL_P2P_NOA_ATTR_IND:
    return WDI_HAL_P2P_NOA_ATTR_IND;
#endif
  case WLAN_HAL_TX_PER_HIT_IND:
    return WDI_HAL_TX_PER_HIT_IND;
  case WLAN_HAL_SET_MAX_TX_POWER_RSP:
    return WDI_SET_MAX_TX_POWER_RESP;
#ifdef WLAN_FEATURE_P2P
  case WLAN_HAL_SET_P2P_GONOA_RSP:
    return WDI_P2P_GO_NOTICE_OF_ABSENCE_RESP;
#endif
  case WLAN_HAL_ENTER_IMPS_RSP:
    return WDI_ENTER_IMPS_RESP; 
  case WLAN_HAL_EXIT_IMPS_RSP:
    return WDI_EXIT_IMPS_RESP; 
  case WLAN_HAL_ENTER_BMPS_RSP:
    return WDI_ENTER_BMPS_RESP; 
  case WLAN_HAL_EXIT_BMPS_RSP:
    return WDI_EXIT_BMPS_RESP; 
  case WLAN_HAL_ENTER_UAPSD_RSP:
    return WDI_ENTER_UAPSD_RESP; 
  case WLAN_HAL_EXIT_UAPSD_RSP:
    return WDI_EXIT_UAPSD_RESP; 
  case WLAN_HAL_SET_UAPSD_AC_PARAMS_RSP:
    return WDI_SET_UAPSD_PARAM_RESP; 
  case WLAN_HAL_UPDATE_UAPSD_PARAM_RSP:
    return WDI_UPDATE_UAPSD_PARAM_RESP; 
  case WLAN_HAL_CONFIGURE_RXP_FILTER_RSP:
    return WDI_CONFIGURE_RXP_FILTER_RESP; 
  case WLAN_HAL_ADD_BCN_FILTER_RSP:
    return WDI_SET_BEACON_FILTER_RESP;
  case WLAN_HAL_REM_BCN_FILTER_RSP:
    return WDI_REM_BEACON_FILTER_RESP;
  case WLAN_HAL_SET_RSSI_THRESH_RSP:
    return WDI_SET_RSSI_THRESHOLDS_RESP;
  case WLAN_HAL_HOST_OFFLOAD_RSP:
    return WDI_HOST_OFFLOAD_RESP;
  case WLAN_HAL_ADD_WOWL_BCAST_PTRN_RSP:
    return WDI_WOWL_ADD_BC_PTRN_RESP;
  case WLAN_HAL_DEL_WOWL_BCAST_PTRN_RSP:
    return WDI_WOWL_DEL_BC_PTRN_RESP;
  case WLAN_HAL_ENTER_WOWL_RSP:
    return WDI_WOWL_ENTER_RESP;
  case WLAN_HAL_EXIT_WOWL_RSP:
    return WDI_WOWL_EXIT_RESP;
  case WLAN_HAL_CONFIGURE_APPS_CPU_WAKEUP_STATE_RSP:
    return WDI_CONFIGURE_APPS_CPU_WAKEUP_STATE_RESP;
  case WLAN_HAL_DOWNLOAD_NV_RSP:
    return WDI_NV_DOWNLOAD_RESP;
  case WLAN_HAL_TL_HAL_FLUSH_AC_RSP:
    return WDI_FLUSH_AC_RESP;
  case WLAN_HAL_SIGNAL_BTAMP_EVENT_RSP:
    return WDI_BTAMP_EVENT_RESP;
#ifdef ANI_MANF_DIAG
  case WLAN_HAL_PROCESS_PTT_RSP:
    return  WDI_FTM_CMD_RESP;
#endif /* ANI_MANF_DIAG */
  case WLAN_HAL_ADD_STA_SELF_RSP:
    return WDI_ADD_STA_SELF_RESP;
case WLAN_HAL_DEL_STA_SELF_RSP:
    return WDI_DEL_STA_SELF_RESP;
  case WLAN_HAL_HOST_RESUME_RSP:
    return WDI_HOST_RESUME_RESP;
  case WLAN_HAL_KEEP_ALIVE_RSP:
    return WDI_KEEP_ALIVE_RESP;
#ifdef FEATURE_WLAN_SCAN_PNO
  case WLAN_HAL_SET_PREF_NETWORK_RSP:
    return WDI_SET_PREF_NETWORK_RESP;
  case WLAN_HAL_SET_RSSI_FILTER_RSP:
    return WDI_SET_RSSI_FILTER_RESP; 
  case WLAN_HAL_UPDATE_SCAN_PARAM_RSP:
    return WDI_UPDATE_SCAN_PARAMS_RESP;
  case WLAN_HAL_PREF_NETW_FOUND_IND:
    return WDI_HAL_PREF_NETWORK_FOUND_IND;
#endif // FEATURE_WLAN_SCAN_PNO
  case WLAN_HAL_SET_TX_PER_TRACKING_RSP:
    return WDI_SET_TX_PER_TRACKING_RESP;
#ifdef WLAN_FEATURE_PACKET_FILTERING
  case WLAN_HAL_8023_MULTICAST_LIST_RSP:
    return WDI_8023_MULTICAST_LIST_RESP;
  case WLAN_HAL_SET_PACKET_FILTER_RSP:
    return WDI_RECEIVE_FILTER_SET_FILTER_RESP;
  case WLAN_HAL_PACKET_FILTER_MATCH_COUNT_RSP:
    return WDI_PACKET_COALESCING_FILTER_MATCH_COUNT_RESP;
  case WLAN_HAL_CLEAR_PACKET_FILTER_RSP:
    return WDI_RECEIVE_FILTER_CLEAR_FILTER_RESP;
#endif // WLAN_FEATURE_PACKET_FILTERING
  case WLAN_HAL_DUMP_COMMAND_RSP:
    return WDI_HAL_DUMP_CMD_RESP;

  case WLAN_HAL_SET_POWER_PARAMS_RSP:
    return WDI_SET_POWER_PARAMS_RESP;
  default:
    return eDRIVER_TYPE_MAX; 
  }

}/*HAL_2_WDI_RSP_TYPE*/


/*Convert WDI driver type into HAL driver type*/
WPT_STATIC WPT_INLINE tDriverType
WDI_2_HAL_DRV_TYPE
(
  WDI_DriverType wdiDriverType
)
{
  /*Lightweight function - no sanity checks and no unecessary code to increase 
    the chances of getting inlined*/
  switch(  wdiDriverType )
  {
  case WDI_DRIVER_TYPE_PRODUCTION:
    return eDRIVER_TYPE_PRODUCTION;
  case WDI_DRIVER_TYPE_MFG:
    return eDRIVER_TYPE_MFG;
  case WDI_DRIVER_TYPE_DVT:
    return eDRIVER_TYPE_DVT;
  }

  return eDRIVER_TYPE_MAX; 
}/*WDI_2_HAL_DRV_TYPE*/


/*Convert WDI stop reason into HAL stop reason*/
WPT_STATIC WPT_INLINE tHalStopType
WDI_2_HAL_STOP_REASON
(
  WDI_StopType wdiDriverType
)
{
  /*Lightweight function - no sanity checks and no unecessary code to increase 
    the chances of getting inlined*/
  switch(  wdiDriverType )
  {
  case WDI_STOP_TYPE_SYS_RESET:
    return HAL_STOP_TYPE_SYS_RESET;
  case WDI_DRIVER_TYPE_MFG:
    return WDI_STOP_TYPE_SYS_DEEP_SLEEP;
  case WDI_STOP_TYPE_RF_KILL:
    return HAL_STOP_TYPE_RF_KILL;
  }

  return HAL_STOP_TYPE_MAX; 
}/*WDI_2_HAL_STOP_REASON*/


/*Convert WDI scan mode type into HAL scan mode type*/
WPT_STATIC WPT_INLINE eHalSysMode
WDI_2_HAL_SCAN_MODE
(
  WDI_ScanMode wdiScanMode
)
{
  /*Lightweight function - no sanity checks and no unecessary code to increase 
    the chances of getting inlined*/
  switch(  wdiScanMode )
  {
  case WDI_SCAN_MODE_NORMAL:
    return eHAL_SYS_MODE_NORMAL;
  case WDI_SCAN_MODE_LEARN:
    return eHAL_SYS_MODE_LEARN;
  case WDI_SCAN_MODE_SCAN:
    return eHAL_SYS_MODE_SCAN;
  case WDI_SCAN_MODE_PROMISC:
    return eHAL_SYS_MODE_PROMISC; 
  }

  return eHAL_SYS_MODE_MAX; 
}/*WDI_2_HAL_SCAN_MODE*/

/*Convert WDI sec ch offset into HAL sec ch offset type*/
WPT_STATIC WPT_INLINE tSirMacHTSecondaryChannelOffset
WDI_2_HAL_SEC_CH_OFFSET
(
  WDI_HTSecondaryChannelOffset wdiSecChOffset
)
{
  /*Lightweight function - no sanity checks and no unecessary code to increase 
    the chances of getting inlined*/
  switch(  wdiSecChOffset )
  {
  case WDI_SECONDARY_CHANNEL_OFFSET_NONE:
    return eHT_SECONDARY_CHANNEL_OFFSET_NONE;
  case WDI_SECONDARY_CHANNEL_OFFSET_UP:
    return eHT_SECONDARY_CHANNEL_OFFSET_UP;
  case WDI_SECONDARY_CHANNEL_OFFSET_DOWN:
    return eHT_SECONDARY_CHANNEL_OFFSET_DOWN;
  }

  return eHT_SECONDARY_CHANNEL_OFFSET_MAX; 
}/*WDI_2_HAL_SEC_CH_OFFSET*/

/*Convert WDI BSS type into HAL BSS type*/
WPT_STATIC WPT_INLINE tSirBssType
WDI_2_HAL_BSS_TYPE
(
  WDI_BssType wdiBSSType
)
{
  /*Lightweight function - no sanity checks and no unecessary code to increase 
    the chances of getting inlined*/
  switch(  wdiBSSType )
  {
  case WDI_INFRASTRUCTURE_MODE:
    return eSIR_INFRASTRUCTURE_MODE;
  case WDI_INFRA_AP_MODE:
    return eSIR_INFRA_AP_MODE;
  case WDI_IBSS_MODE:
    return eSIR_IBSS_MODE;
  case WDI_BTAMP_STA_MODE:
    return eSIR_BTAMP_STA_MODE;
  case WDI_BTAMP_AP_MODE:
    return eSIR_BTAMP_AP_MODE; 
  case WDI_BSS_AUTO_MODE:
    return eSIR_AUTO_MODE;
  }

  return eSIR_DONOT_USE_BSS_TYPE; 
}/*WDI_2_HAL_BSS_TYPE*/

/*Convert WDI NW type into HAL NW type*/
WPT_STATIC WPT_INLINE tSirNwType
WDI_2_HAL_NW_TYPE
(
  WDI_NwType wdiNWType
)
{
  /*Lightweight function - no sanity checks and no unecessary code to increase 
    the chances of getting inlined*/
  switch(  wdiNWType )
  {
  case WDI_11A_NW_TYPE:
    return eSIR_11A_NW_TYPE;
  case WDI_11B_NW_TYPE:
    return eSIR_11B_NW_TYPE;
  case WDI_11G_NW_TYPE:
    return eSIR_11G_NW_TYPE;
  case WDI_11N_NW_TYPE:
    return eSIR_11N_NW_TYPE;
  }

  return eSIR_DONOT_USE_NW_TYPE; 
}/*WDI_2_HAL_NW_TYPE*/

/*Convert WDI chanel bonding type into HAL cb type*/
WPT_STATIC WPT_INLINE ePhyChanBondState
WDI_2_HAL_CB_STATE
(
  WDI_PhyChanBondState wdiCbState
)
{
  /*Lightweight function - no sanity checks and no unecessary code to increase 
    the chances of getting inlined*/
  switch ( wdiCbState )
  {
  case WDI_PHY_SINGLE_CHANNEL_CENTERED:
    return PHY_SINGLE_CHANNEL_CENTERED;
  case WDI_PHY_DOUBLE_CHANNEL_LOW_PRIMARY:
    return PHY_DOUBLE_CHANNEL_LOW_PRIMARY;
  case WDI_PHY_DOUBLE_CHANNEL_CENTERED:
    return PHY_DOUBLE_CHANNEL_CENTERED;
  case WDI_PHY_DOUBLE_CHANNEL_HIGH_PRIMARY:
    return PHY_DOUBLE_CHANNEL_HIGH_PRIMARY;
  }
  
  return PHY_CHANNEL_BONDING_STATE_MAX;
}/*WDI_2_HAL_CB_STATE*/

/*Convert WDI chanel bonding type into HAL cb type*/
WPT_STATIC WPT_INLINE tSirMacHTOperatingMode
WDI_2_HAL_HT_OPER_MODE
(
  WDI_HTOperatingMode wdiHTOperMode
)
{
  /*Lightweight function - no sanity checks and no unecessary code to increase 
    the chances of getting inlined*/
  switch ( wdiHTOperMode )
  {
  case WDI_HT_OP_MODE_PURE:
    return eSIR_HT_OP_MODE_PURE;
  case WDI_HT_OP_MODE_OVERLAP_LEGACY:
    return eSIR_HT_OP_MODE_OVERLAP_LEGACY;
  case WDI_HT_OP_MODE_NO_LEGACY_20MHZ_HT:
    return eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT;
  case WDI_HT_OP_MODE_MIXED:
    return eSIR_HT_OP_MODE_MIXED;
  }
  
  return eSIR_HT_OP_MODE_MAX;
}/*WDI_2_HAL_HT_OPER_MODE*/

/*Convert WDI mimo PS type into HAL mimo PS type*/
WPT_STATIC WPT_INLINE tSirMacHTMIMOPowerSaveState
WDI_2_HAL_MIMO_PS
(
  WDI_HTMIMOPowerSaveState wdiHTOperMode
)
{
  /*Lightweight function - no sanity checks and no unecessary code to increase 
    the chances of getting inlined*/
  switch ( wdiHTOperMode )
  {
  case WDI_HT_MIMO_PS_STATIC:
    return eSIR_HT_MIMO_PS_STATIC;
  case WDI_HT_MIMO_PS_DYNAMIC:
    return eSIR_HT_MIMO_PS_DYNAMIC;
  case WDI_HT_MIMO_PS_NA:
    return eSIR_HT_MIMO_PS_NA;
  case WDI_HT_MIMO_PS_NO_LIMIT:
    return eSIR_HT_MIMO_PS_NO_LIMIT;
  }
  
  return eSIR_HT_MIMO_PS_MAX;
}/*WDI_2_HAL_MIMO_PS*/

/*Convert WDI ENC type into HAL ENC type*/
WPT_STATIC WPT_INLINE tAniEdType
WDI_2_HAL_ENC_TYPE
(
  WDI_EncryptType wdiEncType
)
{
  /*Lightweight function - no sanity checks and no unecessary code to increase 
    the chances of getting inlined*/
  switch ( wdiEncType )
  {
  case WDI_ENCR_NONE:
    return eSIR_ED_NONE;

  case WDI_ENCR_WEP40:
    return eSIR_ED_WEP40;

  case WDI_ENCR_WEP104:
    return eSIR_ED_WEP104;

  case WDI_ENCR_TKIP:
    return eSIR_ED_TKIP;

  case WDI_ENCR_CCMP:
    return eSIR_ED_CCMP;

  case WDI_ENCR_AES_128_CMAC:
    return eSIR_ED_AES_128_CMAC;
#if defined(FEATURE_WLAN_WAPI)
  case WDI_ENCR_WPI:
    return eSIR_ED_WPI;
#endif
  default:
    return eSIR_ED_NOT_IMPLEMENTED;
  }

}/*WDI_2_HAL_ENC_TYPE*/

/*Convert WDI WEP type into HAL WEP type*/
WPT_STATIC WPT_INLINE tAniWepType
WDI_2_HAL_WEP_TYPE
(
  WDI_WepType  wdiWEPType
)
{
  /*Lightweight function - no sanity checks and no unecessary code to increase 
    the chances of getting inlined*/
  switch ( wdiWEPType )
  {
  case WDI_WEP_STATIC:
    return eSIR_WEP_STATIC;

  case WDI_WEP_DYNAMIC:
    return eSIR_WEP_DYNAMIC;
  }
  
  return eSIR_WEP_MAX;
}/*WDI_2_HAL_WEP_TYPE*/

WPT_STATIC WPT_INLINE tSirLinkState
WDI_2_HAL_LINK_STATE
(
  WDI_LinkStateType  wdiLinkState
)
{
  /*Lightweight function - no sanity checks and no unecessary code to increase 
    the chances of getting inlined*/
  switch ( wdiLinkState )
  {
  case WDI_LINK_IDLE_STATE:
    return eSIR_LINK_IDLE_STATE;

  case WDI_LINK_PREASSOC_STATE:
    return eSIR_LINK_PREASSOC_STATE;

  case WDI_LINK_POSTASSOC_STATE:
    return eSIR_LINK_POSTASSOC_STATE;

  case WDI_LINK_AP_STATE:
    return eSIR_LINK_AP_STATE;

  case WDI_LINK_IBSS_STATE:
    return eSIR_LINK_IBSS_STATE;

  case WDI_LINK_BTAMP_PREASSOC_STATE:
    return eSIR_LINK_BTAMP_PREASSOC_STATE;

  case WDI_LINK_BTAMP_POSTASSOC_STATE:
    return eSIR_LINK_BTAMP_POSTASSOC_STATE;

  case WDI_LINK_BTAMP_AP_STATE:
    return eSIR_LINK_BTAMP_AP_STATE;

  case WDI_LINK_BTAMP_STA_STATE:
    return eSIR_LINK_BTAMP_STA_STATE;

  case WDI_LINK_LEARN_STATE:
    return eSIR_LINK_LEARN_STATE;

  case WDI_LINK_SCAN_STATE:
    return eSIR_LINK_SCAN_STATE;

  case WDI_LINK_FINISH_SCAN_STATE:
    return eSIR_LINK_FINISH_SCAN_STATE;

  case WDI_LINK_INIT_CAL_STATE:
    return eSIR_LINK_INIT_CAL_STATE;

  case WDI_LINK_FINISH_CAL_STATE:
    return eSIR_LINK_FINISH_CAL_STATE;

#ifdef WLAN_FEATURE_P2P
  case WDI_LINK_LISTEN_STATE:
    return eSIR_LINK_LISTEN_STATE;
#endif

  default:
    return eSIR_LINK_MAX;
  }  
}

/*Translate a STA Context from WDI into HAL*/ 
WPT_STATIC WPT_INLINE 
void
WDI_CopyWDIStaCtxToHALStaCtx
( 
  tConfigStaParams*          phalConfigSta,
  WDI_ConfigStaReqInfoType*  pwdiConfigSta
)
{
   wpt_uint8 i;
   /*Lightweight function - no sanity checks and no unecessary code to increase 
    the chances of getting inlined*/

  wpalMemoryCopy(phalConfigSta->bssId, 
                  pwdiConfigSta->macBSSID, WDI_MAC_ADDR_LEN); 
  
  wpalMemoryCopy(phalConfigSta->staMac, 
                  pwdiConfigSta->macSTA, WDI_MAC_ADDR_LEN); 

  phalConfigSta->assocId                 = pwdiConfigSta->usAssocId;
  phalConfigSta->staType                 = pwdiConfigSta->wdiSTAType;
  phalConfigSta->shortPreambleSupported  = pwdiConfigSta->ucShortPreambleSupported;
  phalConfigSta->listenInterval          = pwdiConfigSta->usListenInterval;
  phalConfigSta->wmmEnabled              = pwdiConfigSta->ucWMMEnabled;
  phalConfigSta->htCapable               = pwdiConfigSta->ucHTCapable;
  phalConfigSta->txChannelWidthSet       = pwdiConfigSta->ucTXChannelWidthSet;
  phalConfigSta->rifsMode                = pwdiConfigSta->ucRIFSMode;
  phalConfigSta->lsigTxopProtection      = pwdiConfigSta->ucLSIGTxopProtection;
  phalConfigSta->maxAmpduSize            = pwdiConfigSta->ucMaxAmpduSize;
  phalConfigSta->maxAmpduDensity         = pwdiConfigSta->ucMaxAmpduDensity;
  phalConfigSta->maxAmsduSize            = pwdiConfigSta->ucMaxAmsduSize;
  phalConfigSta->fShortGI40Mhz           = pwdiConfigSta->ucShortGI40Mhz;
  phalConfigSta->fShortGI20Mhz           = pwdiConfigSta->ucShortGI20Mhz;
  phalConfigSta->rmfEnabled              = pwdiConfigSta->ucRMFEnabled;
  phalConfigSta->action                  = pwdiConfigSta->wdiAction;
  phalConfigSta->uAPSD                   = pwdiConfigSta->ucAPSD;
  phalConfigSta->maxSPLen                = pwdiConfigSta->ucMaxSPLen;
  phalConfigSta->greenFieldCapable       = pwdiConfigSta->ucGreenFieldCapable;
  phalConfigSta->delayedBASupport        = pwdiConfigSta->ucDelayedBASupport;
  phalConfigSta->us32MaxAmpduDuration    = pwdiConfigSta->us32MaxAmpduDuratio;
  phalConfigSta->fDsssCckMode40Mhz       = pwdiConfigSta->ucDsssCckMode40Mhz;
  phalConfigSta->encryptType             = pwdiConfigSta->ucEncryptType;
  
  phalConfigSta->mimoPS = WDI_2_HAL_MIMO_PS(pwdiConfigSta->wdiMIMOPS);

  phalConfigSta->supportedRates.opRateMode = 
                          pwdiConfigSta->wdiSupportedRates.opRateMode;
  for(i = 0; i < SIR_NUM_11B_RATES; i ++)
  {
     phalConfigSta->supportedRates.llbRates[i] = 
                          pwdiConfigSta->wdiSupportedRates.llbRates[i];
  }
  for(i = 0; i < SIR_NUM_11A_RATES; i ++)
  {
     phalConfigSta->supportedRates.llaRates[i] = 
                          pwdiConfigSta->wdiSupportedRates.llaRates[i];
  }
  for(i = 0; i < SIR_NUM_POLARIS_RATES; i ++)
  {
     phalConfigSta->supportedRates.aniLegacyRates[i] =
                          pwdiConfigSta->wdiSupportedRates.aLegacyRates[i];
  }
  phalConfigSta->supportedRates.aniEnhancedRateBitmap = 
                          pwdiConfigSta->wdiSupportedRates.uEnhancedRateBitmap;
  for(i = 0; i < SIR_MAC_MAX_SUPPORTED_MCS_SET; i ++)
  {
     phalConfigSta->supportedRates.supportedMCSSet[i] = 
                          pwdiConfigSta->wdiSupportedRates.aSupportedMCSSet[i];
  }
  phalConfigSta->supportedRates.rxHighestDataRate =
                          pwdiConfigSta->wdiSupportedRates.aRxHighestDataRate;

#ifdef WLAN_FEATURE_P2P
  phalConfigSta->p2pCapableSta = pwdiConfigSta->ucP2pCapableSta ;
#endif

}/*WDI_CopyWDIStaCtxToHALStaCtx*/;
 
/*Translate a Rate set info from WDI into HAL*/ 
WPT_STATIC WPT_INLINE void 
WDI_CopyWDIRateSetToHALRateSet
( 
  tSirMacRateSet* pHalRateSet,
  WDI_RateSet*    pwdiRateSet
)
{
  wpt_uint8 i; 
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  pHalRateSet->numRates = ( pwdiRateSet->ucNumRates <= SIR_MAC_RATESET_EID_MAX )?
                            pwdiRateSet->ucNumRates:SIR_MAC_RATESET_EID_MAX;

  for ( i = 0; i < pHalRateSet->numRates; i++ )
  {
    pHalRateSet->rate[i] = pwdiRateSet->aRates[i];
  }
  
}/*WDI_CopyWDIRateSetToHALRateSet*/


/*Translate an EDCA Parameter Record from WDI into HAL*/
WPT_STATIC WPT_INLINE void
WDI_CopyWDIEDCAParamsToHALEDCAParams
( 
  tSirMacEdcaParamRecord* phalEdcaParam,
  WDI_EdcaParamRecord*    pWDIEdcaParam
)
{
  /*Lightweight function - no sanity checks and no unecessary code to increase 
    the chances of getting inlined*/

  phalEdcaParam->aci.rsvd   = pWDIEdcaParam->wdiACI.rsvd;
  phalEdcaParam->aci.aci    = pWDIEdcaParam->wdiACI.aci;
  phalEdcaParam->aci.acm    = pWDIEdcaParam->wdiACI.acm;
  phalEdcaParam->aci.aifsn  = pWDIEdcaParam->wdiACI.aifsn;

  phalEdcaParam->cw.max     = pWDIEdcaParam->wdiCW.max;
  phalEdcaParam->cw.min     = pWDIEdcaParam->wdiCW.min;
  phalEdcaParam->txoplimit  = pWDIEdcaParam->usTXOPLimit;
}/*WDI_CopyWDIEDCAParamsToHALEDCAParams*/


/*Copy a management frame header from WDI fmt into HAL fmt*/
WPT_STATIC WPT_INLINE void
WDI_CopyWDIMgmFrameHdrToHALMgmFrameHdr
(
  tSirMacMgmtHdr* pmacMgmtHdr,
  WDI_MacMgmtHdr* pwdiMacMgmtHdr
)
{
  pmacMgmtHdr->fc.protVer    =  pwdiMacMgmtHdr->fc.protVer;
  pmacMgmtHdr->fc.type       =  pwdiMacMgmtHdr->fc.type;
  pmacMgmtHdr->fc.subType    =  pwdiMacMgmtHdr->fc.subType;
  pmacMgmtHdr->fc.toDS       =  pwdiMacMgmtHdr->fc.toDS;
  pmacMgmtHdr->fc.fromDS     =  pwdiMacMgmtHdr->fc.fromDS;
  pmacMgmtHdr->fc.moreFrag   =  pwdiMacMgmtHdr->fc.moreFrag;
  pmacMgmtHdr->fc.retry      =  pwdiMacMgmtHdr->fc.retry;
  pmacMgmtHdr->fc.powerMgmt  =  pwdiMacMgmtHdr->fc.powerMgmt;
  pmacMgmtHdr->fc.moreData   =  pwdiMacMgmtHdr->fc.moreData;
  pmacMgmtHdr->fc.wep        =  pwdiMacMgmtHdr->fc.wep;
  pmacMgmtHdr->fc.order      =  pwdiMacMgmtHdr->fc.order;

  pmacMgmtHdr->durationLo =  pwdiMacMgmtHdr->durationLo;
  pmacMgmtHdr->durationHi =  pwdiMacMgmtHdr->durationHi;

  wpalMemoryCopy(pmacMgmtHdr->da, 
                 pwdiMacMgmtHdr->da, 6);
  wpalMemoryCopy(pmacMgmtHdr->sa, 
                 pwdiMacMgmtHdr->sa, 6);
  wpalMemoryCopy(pmacMgmtHdr->bssId, 
                 pwdiMacMgmtHdr->bssId, 6);

  pmacMgmtHdr->seqControl.fragNum  =  pwdiMacMgmtHdr->seqControl.fragNum;
  pmacMgmtHdr->seqControl.seqNumLo =  pwdiMacMgmtHdr->seqControl.seqNumLo;
  pmacMgmtHdr->seqControl.seqNumHi =  pwdiMacMgmtHdr->seqControl.seqNumHi;

}/*WDI_CopyWDIMgmFrameHdrToHALMgmFrameHdr*/


/*Copy config bss parameters from WDI fmt into HAL fmt*/
WPT_STATIC WPT_INLINE void
WDI_CopyWDIConfigBSSToHALConfigBSS
(
  tConfigBssParams*         phalConfigBSS,
  WDI_ConfigBSSReqInfoType* pwdiConfigBSS
)
{

  wpt_uint8 keyIndex = 0;
  wpalMemoryCopy( phalConfigBSS->bssId,
                  pwdiConfigBSS->macBSSID,
                  WDI_MAC_ADDR_LEN);

#ifdef HAL_SELF_STA_PER_BSS
  wpalMemoryCopy( phalConfigBSS->selfMacAddr,
                  pwdiConfigBSS->macSelfAddr,
                  WDI_MAC_ADDR_LEN);
#endif

  phalConfigBSS->bssType  = WDI_2_HAL_BSS_TYPE(pwdiConfigBSS->wdiBSSType);

  phalConfigBSS->operMode = pwdiConfigBSS->ucOperMode;
  phalConfigBSS->nwType   = WDI_2_HAL_NW_TYPE(pwdiConfigBSS->wdiNWType);

  phalConfigBSS->shortSlotTimeSupported = 
     pwdiConfigBSS->ucShortSlotTimeSupported;
  phalConfigBSS->llaCoexist         = pwdiConfigBSS->ucllaCoexist;
  phalConfigBSS->llbCoexist         = pwdiConfigBSS->ucllbCoexist;
  phalConfigBSS->llgCoexist         = pwdiConfigBSS->ucllgCoexist;
  phalConfigBSS->ht20Coexist        = pwdiConfigBSS->ucHT20Coexist;
  phalConfigBSS->llnNonGFCoexist    = pwdiConfigBSS->ucllnNonGFCoexist;
  phalConfigBSS->fLsigTXOPProtectionFullSupport = 
    pwdiConfigBSS->ucTXOPProtectionFullSupport;
  phalConfigBSS->fRIFSMode          = pwdiConfigBSS->ucRIFSMode;
  phalConfigBSS->beaconInterval     = pwdiConfigBSS->usBeaconInterval;
  phalConfigBSS->dtimPeriod         = pwdiConfigBSS->ucDTIMPeriod;
  phalConfigBSS->txChannelWidthSet  = pwdiConfigBSS->ucTXChannelWidthSet;
  phalConfigBSS->currentOperChannel = pwdiConfigBSS->ucCurrentOperChannel;
  phalConfigBSS->currentExtChannel  = pwdiConfigBSS->ucCurrentExtChannel;
  phalConfigBSS->action             = pwdiConfigBSS->wdiAction;
  phalConfigBSS->htCapable          = pwdiConfigBSS->ucHTCapable;
  phalConfigBSS->rmfEnabled         = pwdiConfigBSS->ucRMFEnabled;

  phalConfigBSS->htOperMode = 
    WDI_2_HAL_HT_OPER_MODE(pwdiConfigBSS->wdiHTOperMod); 

  phalConfigBSS->dualCTSProtection        = pwdiConfigBSS->ucDualCTSProtection;
  phalConfigBSS->ucMaxProbeRespRetryLimit = pwdiConfigBSS->ucMaxProbeRespRetryLimit;
  phalConfigBSS->bHiddenSSIDEn            = pwdiConfigBSS->bHiddenSSIDEn;
  phalConfigBSS->bProxyProbeRespEn        = pwdiConfigBSS->bProxyProbeRespEn;

#ifdef WLAN_FEATURE_VOWIFI
  phalConfigBSS->maxTxPower               = pwdiConfigBSS->cMaxTxPower;
#endif

  /*! Used 32 as magic number because that is how the ssid is declared inside the
   hal header - hal needs a macro for it */
  phalConfigBSS->ssId.length = 
    (pwdiConfigBSS->wdiSSID.ucLength <= 32)?
    pwdiConfigBSS->wdiSSID.ucLength : 32;
  wpalMemoryCopy(phalConfigBSS->ssId.ssId,
                 pwdiConfigBSS->wdiSSID.sSSID, 
                 phalConfigBSS->ssId.length); 

  WDI_CopyWDIStaCtxToHALStaCtx( &phalConfigBSS->staContext,
                                &pwdiConfigBSS->wdiSTAContext);
  
  WDI_CopyWDIRateSetToHALRateSet( &phalConfigBSS->rateSet,
                                  &pwdiConfigBSS->wdiRateSet);

  phalConfigBSS->edcaParamsValid = pwdiConfigBSS->ucEDCAParamsValid;

  if(phalConfigBSS->edcaParamsValid)
  {
     WDI_CopyWDIEDCAParamsToHALEDCAParams( &phalConfigBSS->acbe,
                                        &pwdiConfigBSS->wdiBEEDCAParams);
     WDI_CopyWDIEDCAParamsToHALEDCAParams( &phalConfigBSS->acbk,
                                           &pwdiConfigBSS->wdiBKEDCAParams);
     WDI_CopyWDIEDCAParamsToHALEDCAParams( &phalConfigBSS->acvi,
                                           &pwdiConfigBSS->wdiVIEDCAParams);
     WDI_CopyWDIEDCAParamsToHALEDCAParams( &phalConfigBSS->acvo,
                                           &pwdiConfigBSS->wdiVOEDCAParams);
  }

  phalConfigBSS->halPersona = pwdiConfigBSS->ucPersona; 

  phalConfigBSS->bSpectrumMgtEnable = pwdiConfigBSS->bSpectrumMgtEn;

#ifdef WLAN_FEATURE_VOWIFI_11R

  phalConfigBSS->extSetStaKeyParamValid = 
     pwdiConfigBSS->bExtSetStaKeyParamValid;
  
  if( phalConfigBSS->extSetStaKeyParamValid )
  {
     /*-----------------------------------------------------------------------
       Copy the STA Key parameters into the HAL message
     -----------------------------------------------------------------------*/
     phalConfigBSS->extSetStaKeyParam.encType = 
        WDI_2_HAL_ENC_TYPE (pwdiConfigBSS->wdiExtSetKeyParam.wdiEncType);

     phalConfigBSS->extSetStaKeyParam.wepType = 
        WDI_2_HAL_WEP_TYPE (pwdiConfigBSS->wdiExtSetKeyParam.wdiWEPType );

     phalConfigBSS->extSetStaKeyParam.staIdx = pwdiConfigBSS->wdiExtSetKeyParam.ucSTAIdx;

     phalConfigBSS->extSetStaKeyParam.defWEPIdx = pwdiConfigBSS->wdiExtSetKeyParam.ucDefWEPIdx;

     phalConfigBSS->extSetStaKeyParam.singleTidRc = pwdiConfigBSS->wdiExtSetKeyParam.ucSingleTidRc;

#ifdef WLAN_SOFTAP_FEATURE
     for(keyIndex = 0; keyIndex < pwdiConfigBSS->wdiExtSetKeyParam.ucNumKeys ;
          keyIndex++)
     {
        phalConfigBSS->extSetStaKeyParam.key[keyIndex].keyId = 
           pwdiConfigBSS->wdiExtSetKeyParam.wdiKey[keyIndex].keyId;
        phalConfigBSS->extSetStaKeyParam.key[keyIndex].unicast =
           pwdiConfigBSS->wdiExtSetKeyParam.wdiKey[keyIndex].unicast;
        phalConfigBSS->extSetStaKeyParam.key[keyIndex].keyDirection =
           pwdiConfigBSS->wdiExtSetKeyParam.wdiKey[keyIndex].keyDirection;
        wpalMemoryCopy(phalConfigBSS->extSetStaKeyParam.key[keyIndex].keyRsc,
                       pwdiConfigBSS->wdiExtSetKeyParam.wdiKey[keyIndex].keyRsc, 
                       WDI_MAX_KEY_RSC_LEN);
        phalConfigBSS->extSetStaKeyParam.key[keyIndex].paeRole = 
           pwdiConfigBSS->wdiExtSetKeyParam.wdiKey[keyIndex].paeRole;
        phalConfigBSS->extSetStaKeyParam.key[keyIndex].keyLength = 
           pwdiConfigBSS->wdiExtSetKeyParam.wdiKey[keyIndex].keyLength;
        wpalMemoryCopy(phalConfigBSS->extSetStaKeyParam.key[keyIndex].key,
                       pwdiConfigBSS->wdiExtSetKeyParam.wdiKey[keyIndex].key, 
                       WDI_MAX_KEY_LENGTH);
     }
#else
     phalConfigBSS->extSetStaKeyParam.key.keyId = 
        pwdiConfigBSS->wdiExtSetKeyParam.wdiKey[0].keyId;
     phalConfigBSS->extSetStaKeyParam.key.unicast =
        pwdiConfigBSS->wdiExtSetKeyParam.wdiKey[0].unicast;
     phalConfigBSS->extSetStaKeyParam.key.keyDirection =
        pwdiConfigBSS->wdiExtSetKeyParam.wdiKey[0].keyDirection;
     wpalMemoryCopy(phalConfigBSS->extSetStaKeyParam.key.keyRsc,
                    pwdiConfigBSS->wdiExtSetKeyParam.wdiKey[0].keyRsc, 
                    WDI_MAX_KEY_RSC_LEN);
     phalConfigBSS->extSetStaKeyParam.key.paeRole = 
        pwdiConfigBSS->wdiExtSetKeyParam.wdiKey[0].paeRole;
     phalConfigBSS->extSetStaKeyParam.key.keyLength = 
        pwdiConfigBSS->wdiExtSetKeyParam.wdiKey[0].keyLength;
     wpalMemoryCopy(phalConfigBSS->extSetStaKeyParam.key.key,
                    pwdiConfigBSS->wdiExtSetKeyParam.wdiKey[0].key, 
                    WDI_MAX_KEY_LENGTH);
#endif
  }
  else/* phalConfigBSS->extSetStaKeyParamValid is not set */
  {
     wpalMemoryZero( &phalConfigBSS->extSetStaKeyParam, 
                     sizeof(phalConfigBSS->extSetStaKeyParam) );
  }

#endif /*WLAN_FEATURE_VOWIFI_11R*/

}/*WDI_CopyWDIConfigBSSToHALConfigBSS*/


/*Extract the request CB function and user data from a request structure 
  pointed to by user data */
WPT_STATIC WPT_INLINE void
WDI_ExtractRequestCBFromEvent
(
  WDI_EventInfoType* pEvent,
  WDI_ReqStatusCb*   ppfnReqCB, 
  void**             ppUserData
)
{
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  switch ( pEvent->wdiRequest )
  {
  case WDI_START_REQ:
    *ppfnReqCB   =  ((WDI_StartReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_StartReqParamsType*)pEvent->pEventData)->pUserData;
    break;
  case WDI_STOP_REQ:
    *ppfnReqCB   =  ((WDI_StopReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_StopReqParamsType*)pEvent->pEventData)->pUserData;
    break;
  case WDI_INIT_SCAN_REQ:
    *ppfnReqCB   =  ((WDI_InitScanReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_InitScanReqParamsType*)pEvent->pEventData)->pUserData;
    break;
  case WDI_START_SCAN_REQ:
    *ppfnReqCB   =  ((WDI_StartScanReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_StartScanReqParamsType*)pEvent->pEventData)->pUserData;
    break;
  case WDI_END_SCAN_REQ:
    *ppfnReqCB   =  ((WDI_EndScanReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_EndScanReqParamsType*)pEvent->pEventData)->pUserData;
    break;
  case WDI_FINISH_SCAN_REQ:
    *ppfnReqCB   =  ((WDI_FinishScanReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_FinishScanReqParamsType*)pEvent->pEventData)->pUserData;
    break;
  case WDI_JOIN_REQ:
    *ppfnReqCB   =  ((WDI_JoinReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_JoinReqParamsType*)pEvent->pEventData)->pUserData;
    break;
  case WDI_CONFIG_BSS_REQ:
    *ppfnReqCB   =  ((WDI_ConfigBSSReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_ConfigBSSReqParamsType*)pEvent->pEventData)->pUserData;
    break;
  case WDI_DEL_BSS_REQ:
    *ppfnReqCB   =  ((WDI_DelBSSReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_DelBSSReqParamsType*)pEvent->pEventData)->pUserData;
    break;
  case WDI_POST_ASSOC_REQ:
    *ppfnReqCB   =  ((WDI_PostAssocReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_PostAssocReqParamsType*)pEvent->pEventData)->pUserData;
    break;
  case WDI_DEL_STA_REQ:
    *ppfnReqCB   =  ((WDI_DelSTAReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_DelSTAReqParamsType*)pEvent->pEventData)->pUserData;
    break;
  case WDI_SET_BSS_KEY_REQ:
    *ppfnReqCB   =  ((WDI_SetBSSKeyReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_SetBSSKeyReqParamsType*)pEvent->pEventData)->pUserData;
    break;
  case WDI_RMV_BSS_KEY_REQ:
    *ppfnReqCB   =  ((WDI_RemoveBSSKeyReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_RemoveBSSKeyReqParamsType*)pEvent->pEventData)->pUserData;
    break;
  case WDI_SET_STA_KEY_REQ:
    *ppfnReqCB   =  ((WDI_SetSTAKeyReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_SetSTAKeyReqParamsType*)pEvent->pEventData)->pUserData;
    break;
  case WDI_RMV_STA_KEY_REQ:
    *ppfnReqCB   =  ((WDI_RemoveSTAKeyReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_RemoveSTAKeyReqParamsType*)pEvent->pEventData)->pUserData;
    break;
  case WDI_ADD_TS_REQ:
    *ppfnReqCB   =  ((WDI_AddTSReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_AddTSReqParamsType*)pEvent->pEventData)->pUserData;
    break;
  case WDI_DEL_TS_REQ:
    *ppfnReqCB   =  ((WDI_DelTSReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_DelTSReqParamsType*)pEvent->pEventData)->pUserData;
    break;
  case WDI_UPD_EDCA_PRMS_REQ:
    *ppfnReqCB   =  ((WDI_UpdateEDCAParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_UpdateEDCAParamsType*)pEvent->pEventData)->pUserData;
    break;
  case WDI_ADD_BA_SESSION_REQ:
    *ppfnReqCB   =  ((WDI_AddBASessionReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_AddBASessionReqParamsType*)pEvent->pEventData)->pUserData;
    break;
  case WDI_DEL_BA_REQ:
    *ppfnReqCB   =  ((WDI_DelBAReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_DelBAReqParamsType*)pEvent->pEventData)->pUserData;
    break;
  case WDI_CH_SWITCH_REQ:
    *ppfnReqCB   =  ((WDI_SwitchChReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_SwitchChReqParamsType*)pEvent->pEventData)->pUserData;
    break;
  case WDI_CONFIG_STA_REQ:
    *ppfnReqCB   =  ((WDI_ConfigSTAReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_ConfigSTAReqParamsType*)pEvent->pEventData)->pUserData;
    break;
  case WDI_SET_LINK_ST_REQ:
    *ppfnReqCB   =  ((WDI_SetLinkReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_SetLinkReqParamsType*)pEvent->pEventData)->pUserData;
    break;
  case WDI_GET_STATS_REQ:
    *ppfnReqCB   =  ((WDI_GetStatsReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_GetStatsReqParamsType*)pEvent->pEventData)->pUserData;
    break;
  case WDI_UPDATE_CFG_REQ:
    *ppfnReqCB   =  ((WDI_UpdateCfgReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_UpdateCfgReqParamsType*)pEvent->pEventData)->pUserData;
    break;
  case WDI_ADD_BA_REQ:
    *ppfnReqCB   =  ((WDI_AddBAReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_AddBAReqParamsType*)pEvent->pEventData)->pUserData;
    break;
  case WDI_TRIGGER_BA_REQ:
    *ppfnReqCB   =  ((WDI_TriggerBAReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_TriggerBAReqParamsType*)pEvent->pEventData)->pUserData;
    break;
  case  WDI_UPD_BCON_PRMS_REQ:
    *ppfnReqCB   =  ((WDI_UpdateBeaconParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_UpdateBeaconParamsType*)pEvent->pEventData)->pUserData;
     break;
  case  WDI_SND_BCON_REQ:
    *ppfnReqCB   =  ((WDI_SendBeaconParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_SendBeaconParamsType*)pEvent->pEventData)->pUserData;
     break;
  case  WDI_ENTER_BMPS_REQ:
    *ppfnReqCB   =  ((WDI_EnterBmpsReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_EnterBmpsReqParamsType*)pEvent->pEventData)->pUserData;
     break;
  case  WDI_EXIT_BMPS_REQ:
    *ppfnReqCB   =  ((WDI_ExitBmpsReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_ExitBmpsReqParamsType*)pEvent->pEventData)->pUserData;
     break;
  case  WDI_ENTER_UAPSD_REQ:
    *ppfnReqCB   =  ((WDI_EnterUapsdReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_EnterUapsdReqParamsType*)pEvent->pEventData)->pUserData;
     break;
  case  WDI_UPDATE_UAPSD_PARAM_REQ:
    *ppfnReqCB   =  ((WDI_UpdateUapsdReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_UpdateUapsdReqParamsType*)pEvent->pEventData)->pUserData;
     break;
  case  WDI_CONFIGURE_RXP_FILTER_REQ:
    *ppfnReqCB   =  ((WDI_ConfigureRxpFilterReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_ConfigureRxpFilterReqParamsType*)pEvent->pEventData)->pUserData;
     break;
  case  WDI_SET_BEACON_FILTER_REQ:
    *ppfnReqCB   =  ((WDI_BeaconFilterReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_BeaconFilterReqParamsType*)pEvent->pEventData)->pUserData;
     break;
  case  WDI_REM_BEACON_FILTER_REQ:
    *ppfnReqCB   =  ((WDI_RemBeaconFilterReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_RemBeaconFilterReqParamsType*)pEvent->pEventData)->pUserData;
     break; 
  case  WDI_SET_RSSI_THRESHOLDS_REQ:
    *ppfnReqCB   =  ((WDI_SetRSSIThresholdsReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_SetRSSIThresholdsReqParamsType*)pEvent->pEventData)->pUserData;
     break;
  case  WDI_HOST_OFFLOAD_REQ:
    *ppfnReqCB   =  ((WDI_HostOffloadReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_HostOffloadReqParamsType*)pEvent->pEventData)->pUserData;
     break;
  case  WDI_WOWL_ADD_BC_PTRN_REQ:
    *ppfnReqCB   =  ((WDI_WowlAddBcPtrnReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_WowlAddBcPtrnReqParamsType*)pEvent->pEventData)->pUserData;
     break;
  case  WDI_WOWL_DEL_BC_PTRN_REQ:
    *ppfnReqCB   =  ((WDI_WowlDelBcPtrnReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_WowlDelBcPtrnReqParamsType*)pEvent->pEventData)->pUserData;
     break;
  case  WDI_WOWL_ENTER_REQ:
    *ppfnReqCB   =  ((WDI_WowlEnterReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_WowlEnterReqParamsType*)pEvent->pEventData)->pUserData;
     break;
  case  WDI_CONFIGURE_APPS_CPU_WAKEUP_STATE_REQ:
    *ppfnReqCB   =  ((WDI_ConfigureAppsCpuWakeupStateReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_ConfigureAppsCpuWakeupStateReqParamsType*)pEvent->pEventData)->pUserData;
     break;
  case WDI_FLUSH_AC_REQ:
    *ppfnReqCB   =  ((WDI_FlushAcReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_FlushAcReqParamsType*)pEvent->pEventData)->pUserData;
    break;
  case WDI_BTAMP_EVENT_REQ:
    *ppfnReqCB   =  ((WDI_BtAmpEventParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_BtAmpEventParamsType*)pEvent->pEventData)->pUserData;
    break;
  case WDI_KEEP_ALIVE_REQ:
    *ppfnReqCB   =  ((WDI_KeepAliveReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_KeepAliveReqParamsType*)pEvent->pEventData)->pUserData;
    break;
  case WDI_SET_TX_PER_TRACKING_REQ:
    *ppfnReqCB   =  ((WDI_SetTxPerTrackingReqParamsType*)pEvent->pEventData)->wdiReqStatusCB;
    *ppUserData  =  ((WDI_SetTxPerTrackingReqParamsType*)pEvent->pEventData)->pUserData;
  default:
    *ppfnReqCB   =  NULL;
    *ppUserData  =  NULL;
      break; 
  }
}/*WDI_ExtractRequestCBFromEvent*/


/**
 @brief WDI_IsHwFrameTxTranslationCapable checks to see if HW 
        frame xtl is enabled for a particular STA.

 WDI_PostAssocReq must have been called.

 @param uSTAIdx: STA index 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
wpt_boolean 
WDI_IsHwFrameTxTranslationCapable
(
  wpt_uint8 uSTAIdx
)
{
  /*!! FIX ME - this must eventually be per station - for now just feedback 
    uma value*/
  /*------------------------------------------------------------------------
    Sanity Check 
  ------------------------------------------------------------------------*/
  if ( eWLAN_PAL_FALSE == gWDIInitialized )
  {
    WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
              "WDI API call before module is initialized - Fail request");

    return WDI_STATUS_E_NOT_ALLOWED; 
  }

  
  return gWDICb.bFrameTransEnabled;
}/*WDI_IsHwFrameTxTranslationCapable*/

#ifdef FEATURE_WLAN_SCAN_PNO
/**
 @brief WDI_SetPreferredNetworkList

 @param pwdiPNOScanReqParams: the Set PNO as specified 
                      by the Device Interface
  
        wdiPNOScanCb: callback for passing back the response
        of the Set PNO operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @return Result of the function call
*/
WDI_Status 
WDI_SetPreferredNetworkReq
(
  WDI_PNOScanReqParamsType* pwdiPNOScanReqParams,
  WDI_PNOScanCb             wdiPNOScanCb,
  void*                      pUserData
)
{
   WDI_EventInfoType      wdiEventData;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*------------------------------------------------------------------------
     Sanity Check 
   ------------------------------------------------------------------------*/
   if ( eWLAN_PAL_FALSE == gWDIInitialized )
   {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WDI API call before module is initialized - Fail request");

     return WDI_STATUS_E_NOT_ALLOWED; 
   }

   /*------------------------------------------------------------------------
     Fill in Event data and post to the Main FSM
   ------------------------------------------------------------------------*/
   wdiEventData.wdiRequest      = WDI_SET_PREF_NETWORK_REQ;
   wdiEventData.pEventData      = pwdiPNOScanReqParams; 
   wdiEventData.uEventDataSize  = sizeof(*pwdiPNOScanReqParams);
   wdiEventData.pCBfnc          = wdiPNOScanCb; 
   wdiEventData.pUserData       = pUserData;

   return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);
}


/**
 @brief WDI_SetRssiFilterReq

 @param pwdiRssiFilterReqParams: the Set RSSI Filter as 
                      specified by the Device Interface
  
        wdiRssiFilterCb: callback for passing back the response
        of the Set RSSI Filter operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @return Result of the function call
*/
WDI_Status 
WDI_SetRssiFilterReq
(
  WDI_SetRssiFilterReqParamsType* pwdiRssiFilterReqParams,
  WDI_RssiFilterCb                wdiRssiFilterCb,
  void*                           pUserData
)
{
   WDI_EventInfoType      wdiEventData;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*------------------------------------------------------------------------
     Sanity Check 
   ------------------------------------------------------------------------*/
   if ( eWLAN_PAL_FALSE == gWDIInitialized )
   {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WDI API call before module is initialized - Fail request");

     return WDI_STATUS_E_NOT_ALLOWED; 
   }

   /*------------------------------------------------------------------------
     Fill in Event data and post to the Main FSM
   ------------------------------------------------------------------------*/
   wdiEventData.wdiRequest      = WDI_SET_RSSI_FILTER_REQ;
   wdiEventData.pEventData      = pwdiRssiFilterReqParams; 
   wdiEventData.uEventDataSize  = sizeof(*pwdiRssiFilterReqParams);
   wdiEventData.pCBfnc          = wdiRssiFilterCb; 
   wdiEventData.pUserData       = pUserData;

   return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);
}/*WDI_SetRssiFilterReq*/

/**
 @brief WDI_UpdateScanParamsReq

 @param pwdiUpdateScanParamsInfoType: the Update Scan Params as specified 
                      by the Device Interface
  
        wdiUpdateScanParamsCb: callback for passing back the response
        of the Set PNO operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @return Result of the function call
*/
WDI_Status 
WDI_UpdateScanParamsReq
(
  WDI_UpdateScanParamsInfoType* pwdiUpdateScanParamsInfoType,
  WDI_UpdateScanParamsCb        wdiUpdateScanParamsCb,
  void*                         pUserData
)
{
   WDI_EventInfoType      wdiEventData;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*------------------------------------------------------------------------
     Sanity Check 
   ------------------------------------------------------------------------*/
   if ( eWLAN_PAL_FALSE == gWDIInitialized )
   {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WDI API call before module is initialized - Fail request");

     return WDI_STATUS_E_NOT_ALLOWED; 
   }

   /*------------------------------------------------------------------------
     Fill in Event data and post to the Main FSM
   ------------------------------------------------------------------------*/
   wdiEventData.wdiRequest      = WDI_UPDATE_SCAN_PARAMS_REQ;
   wdiEventData.pEventData      = pwdiUpdateScanParamsInfoType; 
   wdiEventData.uEventDataSize  = sizeof(*pwdiUpdateScanParamsInfoType);
   wdiEventData.pCBfnc          = wdiUpdateScanParamsCb; 
   wdiEventData.pUserData       = pUserData;

   return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);
}

/**
 @brief Helper function to pack Set Preferred Network List 
        Request parameters
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pwdiPNOScanReqParams:      pointer to the info received
         from upper layers
         ppSendBuffer, pSize - out pointers of the packed buffer
         and its size 
  
 @return Result of the function call
*/

WDI_Status
WDI_PackPreferredNetworkList
(
  WDI_ControlBlockType*      pWDICtx,
  WDI_PNOScanReqParamsType*  pwdiPNOScanReqParams,
  wpt_uint8**                ppSendBuffer,
  wpt_uint16*                pSize
)
{
   wpt_uint8*                 pSendBuffer           = NULL; 
   wpt_uint16                 usDataOffset          = 0;
   wpt_uint16                 usSendSize            = 0;
   tPrefNetwListParams        pPrefNetwListParams;
   wpt_uint8 i;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

   /*-----------------------------------------------------------------------
     Get message buffer
   -----------------------------------------------------------------------*/
   if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_SET_PREF_NETWORK_REQ, 
                         sizeof(pPrefNetwListParams),
                         &pSendBuffer, &usDataOffset, &usSendSize))||
       ( usSendSize < (usDataOffset + sizeof(pPrefNetwListParams) )))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
                  "Unable to get send buffer in Set PNO req %x ",
                   pwdiPNOScanReqParams);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   /*-------------------------------------------------------------------------
     Fill prefNetwListParams from pwdiPNOScanReqParams->wdiPNOScanInfo
   -------------------------------------------------------------------------*/
   pPrefNetwListParams.enable  = 
     pwdiPNOScanReqParams->wdiPNOScanInfo.bEnable;
   pPrefNetwListParams.modePNO = 
     pwdiPNOScanReqParams->wdiPNOScanInfo.wdiModePNO;

   pPrefNetwListParams.ucNetworksCount = 
     (pwdiPNOScanReqParams->wdiPNOScanInfo.ucNetworksCount < 
      WLAN_HAL_PNO_MAX_SUPP_NETWORKS)?
     pwdiPNOScanReqParams->wdiPNOScanInfo.ucNetworksCount : 
      WLAN_HAL_PNO_MAX_SUPP_NETWORKS;

   WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
               "WDI SET PNO: Enable %d, Mode %d, Netw Count %d", 
               pwdiPNOScanReqParams->wdiPNOScanInfo.bEnable,
               pwdiPNOScanReqParams->wdiPNOScanInfo.wdiModePNO,
               pwdiPNOScanReqParams->wdiPNOScanInfo.ucNetworksCount);

   for ( i = 0; i < pPrefNetwListParams.ucNetworksCount; i++ )
   {
     /*SSID of the BSS*/
     pPrefNetwListParams.aNetworks[i].ssId.length
        = pwdiPNOScanReqParams->wdiPNOScanInfo.aNetworks[i].ssId.ucLength;

     wpalMemoryCopy( pPrefNetwListParams.aNetworks[i].ssId.ssId,
          pwdiPNOScanReqParams->wdiPNOScanInfo.aNetworks[i].ssId.sSSID,
          pPrefNetwListParams.aNetworks[i].ssId.length);

     /*Authentication type for the network*/
     pPrefNetwListParams.aNetworks[i].authentication = 
       (tAuthType)pwdiPNOScanReqParams->wdiPNOScanInfo.aNetworks[i].wdiAuth; 

     /*Encryption type for the network*/
     pPrefNetwListParams.aNetworks[i].encryption = 
       (tEdType)pwdiPNOScanReqParams->wdiPNOScanInfo.aNetworks[i].wdiEncryption; 

     /*Indicate the channel on which the Network can be found 
       0 - if all channels */
     pPrefNetwListParams.aNetworks[i].ucChannelCount = 
       pwdiPNOScanReqParams->wdiPNOScanInfo.aNetworks[i].ucChannelCount;

     wpalMemoryCopy(pPrefNetwListParams.aNetworks[i].aChannels,
                    pwdiPNOScanReqParams->wdiPNOScanInfo.aNetworks[i].aChannels,
                    pPrefNetwListParams.aNetworks[i].ucChannelCount);

     /*Indicates the RSSI threshold for the network to be considered*/
     pPrefNetwListParams.aNetworks[i].rssiThreshold =
       pwdiPNOScanReqParams->wdiPNOScanInfo.aNetworks[i].rssiThreshold;

     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WDI SET PNO: SSID %d %s", 
               pPrefNetwListParams.aNetworks[i].ssId.length,
               pPrefNetwListParams.aNetworks[i].ssId.ssId);
   }

   pPrefNetwListParams.scanTimers.ucScanTimersCount = 
     (pwdiPNOScanReqParams->wdiPNOScanInfo.scanTimers.ucScanTimersCount < 
      WLAN_HAL_PNO_MAX_SCAN_TIMERS)?
     pwdiPNOScanReqParams->wdiPNOScanInfo.scanTimers.ucScanTimersCount :
      WLAN_HAL_PNO_MAX_SCAN_TIMERS;

   WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WDI SET PNO: Scan timers count %d 24G P %d 5G Probe %d", 
               pPrefNetwListParams.scanTimers.ucScanTimersCount,
               pwdiPNOScanReqParams->wdiPNOScanInfo.us24GProbeSize,
               pwdiPNOScanReqParams->wdiPNOScanInfo.us5GProbeSize);

   for ( i = 0; i < pPrefNetwListParams.scanTimers.ucScanTimersCount; i++   )
   {
     pPrefNetwListParams.scanTimers.aTimerValues[i].uTimerValue  = 
       pwdiPNOScanReqParams->wdiPNOScanInfo.scanTimers.aTimerValues[i].uTimerValue;
     pPrefNetwListParams.scanTimers.aTimerValues[i].uTimerRepeat = 
       pwdiPNOScanReqParams->wdiPNOScanInfo.scanTimers.aTimerValues[i].uTimerRepeat;
   }

   /*Copy the probe template*/
   pPrefNetwListParams.us24GProbeSize = 
     (pwdiPNOScanReqParams->wdiPNOScanInfo.us24GProbeSize<
     WLAN_HAL_PNO_MAX_PROBE_SIZE)?
     pwdiPNOScanReqParams->wdiPNOScanInfo.us24GProbeSize:
     WLAN_HAL_PNO_MAX_PROBE_SIZE; 

   wpalMemoryCopy(pPrefNetwListParams.a24GProbeTemplate, 
                  pwdiPNOScanReqParams->wdiPNOScanInfo.a24GProbeTemplate, 
                  pPrefNetwListParams.us24GProbeSize); 

   pPrefNetwListParams.us5GProbeSize = 
     (pwdiPNOScanReqParams->wdiPNOScanInfo.us5GProbeSize <
     WLAN_HAL_PNO_MAX_PROBE_SIZE)?
     pwdiPNOScanReqParams->wdiPNOScanInfo.us5GProbeSize:
     WLAN_HAL_PNO_MAX_PROBE_SIZE; 

   wpalMemoryCopy(pPrefNetwListParams.a5GProbeTemplate, 
                  pwdiPNOScanReqParams->wdiPNOScanInfo.a5GProbeTemplate, 
                  pPrefNetwListParams.us5GProbeSize); 

   /*Pack the buffer*/
   wpalMemoryCopy( pSendBuffer+usDataOffset, &pPrefNetwListParams, 
                   sizeof(pPrefNetwListParams)); 

   /*Set the output values*/
   *ppSendBuffer = pSendBuffer;
   *pSize        = usSendSize; 

   return WDI_STATUS_SUCCESS;
}/*WDI_PackPreferredNetworkList*/

/**
 @brief Helper function to pack Set Preferred Network List 
        Request parameters
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pwdiPNOScanReqParams:      pointer to the info received
         from upper layers
         ppSendBuffer, pSize - out pointers of the packed buffer
         and its size 
  
 @return Result of the function call
*/

WDI_Status
WDI_PackPreferredNetworkListNew
(
  WDI_ControlBlockType*      pWDICtx,
  WDI_PNOScanReqParamsType*  pwdiPNOScanReqParams,
  wpt_uint8**                ppSendBuffer,
  wpt_uint16*                pSize
)
{
   wpt_uint8*                 pSendBuffer           = NULL; 
   wpt_uint16                 usDataOffset          = 0;
   wpt_uint16                 usSendSize            = 0;
   tPrefNetwListParamsNew     pPrefNetwListParams;
   wpt_uint8 i;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -*/

   /*-----------------------------------------------------------------------
     Get message buffer
   -----------------------------------------------------------------------*/
   if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_SET_PREF_NETWORK_REQ, 
                         sizeof(pPrefNetwListParams),
                         &pSendBuffer, &usDataOffset, &usSendSize))||
       ( usSendSize < (usDataOffset + sizeof(pPrefNetwListParams) )))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
                  "Unable to get send buffer in Set PNO req %x  ",
                   pwdiPNOScanReqParams);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   /*-------------------------------------------------------------------------
     Fill prefNetwListParams from pwdiPNOScanReqParams->wdiPNOScanInfo
   -------------------------------------------------------------------------*/
   pPrefNetwListParams.enable  = 
     pwdiPNOScanReqParams->wdiPNOScanInfo.bEnable;
   pPrefNetwListParams.modePNO = 
     pwdiPNOScanReqParams->wdiPNOScanInfo.wdiModePNO;

   pPrefNetwListParams.ucNetworksCount = 
     (pwdiPNOScanReqParams->wdiPNOScanInfo.ucNetworksCount < 
      WLAN_HAL_PNO_MAX_SUPP_NETWORKS)?
     pwdiPNOScanReqParams->wdiPNOScanInfo.ucNetworksCount : 
      WLAN_HAL_PNO_MAX_SUPP_NETWORKS;

   WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
               "WDI SET PNO: Enable %d, Mode %d, Netw Count %d", 
               pwdiPNOScanReqParams->wdiPNOScanInfo.bEnable,
               pwdiPNOScanReqParams->wdiPNOScanInfo.wdiModePNO,
               pwdiPNOScanReqParams->wdiPNOScanInfo.ucNetworksCount);

   for ( i = 0; i < pPrefNetwListParams.ucNetworksCount; i++ )
   {
     /*SSID of the BSS*/
     pPrefNetwListParams.aNetworks[i].ssId.length
        = pwdiPNOScanReqParams->wdiPNOScanInfo.aNetworks[i].ssId.ucLength;

     wpalMemoryCopy( pPrefNetwListParams.aNetworks[i].ssId.ssId,
          pwdiPNOScanReqParams->wdiPNOScanInfo.aNetworks[i].ssId.sSSID,
          pPrefNetwListParams.aNetworks[i].ssId.length);

     /*Authentication type for the network*/
     pPrefNetwListParams.aNetworks[i].authentication = 
       (tAuthType)pwdiPNOScanReqParams->wdiPNOScanInfo.aNetworks[i].wdiAuth; 

     /*Encryption type for the network*/
     pPrefNetwListParams.aNetworks[i].encryption = 
       (tEdType)pwdiPNOScanReqParams->wdiPNOScanInfo.aNetworks[i].wdiEncryption; 

     /*SSID bcast type for the network*/
     pPrefNetwListParams.aNetworks[i].bcastNetworkType = 
       (tSSIDBcastType)pwdiPNOScanReqParams->wdiPNOScanInfo.aNetworks[i].wdiBcastNetworkType; 

     /*Indicate the channel on which the Network can be found 
       0 - if all channels */
     pPrefNetwListParams.aNetworks[i].ucChannelCount = 
       pwdiPNOScanReqParams->wdiPNOScanInfo.aNetworks[i].ucChannelCount;

     wpalMemoryCopy(pPrefNetwListParams.aNetworks[i].aChannels,
                    pwdiPNOScanReqParams->wdiPNOScanInfo.aNetworks[i].aChannels,
                    pPrefNetwListParams.aNetworks[i].ucChannelCount);

     /*Indicates the RSSI threshold for the network to be considered*/
     pPrefNetwListParams.aNetworks[i].rssiThreshold =
       pwdiPNOScanReqParams->wdiPNOScanInfo.aNetworks[i].rssiThreshold;

     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WDI SET PNO: SSID %d %s", 
               pPrefNetwListParams.aNetworks[i].ssId.length,
               pPrefNetwListParams.aNetworks[i].ssId.ssId);
   }

   pPrefNetwListParams.scanTimers.ucScanTimersCount = 
     (pwdiPNOScanReqParams->wdiPNOScanInfo.scanTimers.ucScanTimersCount < 
      WLAN_HAL_PNO_MAX_SCAN_TIMERS)?
     pwdiPNOScanReqParams->wdiPNOScanInfo.scanTimers.ucScanTimersCount :
      WLAN_HAL_PNO_MAX_SCAN_TIMERS;

   WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WDI SET PNO: Scan timers count %d 24G P %d 5G Probe %d", 
               pPrefNetwListParams.scanTimers.ucScanTimersCount,
               pwdiPNOScanReqParams->wdiPNOScanInfo.us24GProbeSize,
               pwdiPNOScanReqParams->wdiPNOScanInfo.us5GProbeSize);

   for ( i = 0; i < pPrefNetwListParams.scanTimers.ucScanTimersCount; i++   )
   {
     pPrefNetwListParams.scanTimers.aTimerValues[i].uTimerValue  = 
       pwdiPNOScanReqParams->wdiPNOScanInfo.scanTimers.aTimerValues[i].uTimerValue;
     pPrefNetwListParams.scanTimers.aTimerValues[i].uTimerRepeat = 
       pwdiPNOScanReqParams->wdiPNOScanInfo.scanTimers.aTimerValues[i].uTimerRepeat;
   }

   /*Copy the probe template*/
   pPrefNetwListParams.us24GProbeSize = 
     (pwdiPNOScanReqParams->wdiPNOScanInfo.us24GProbeSize<
     WLAN_HAL_PNO_MAX_PROBE_SIZE)?
     pwdiPNOScanReqParams->wdiPNOScanInfo.us24GProbeSize:
     WLAN_HAL_PNO_MAX_PROBE_SIZE; 

   wpalMemoryCopy(pPrefNetwListParams.a24GProbeTemplate, 
                  pwdiPNOScanReqParams->wdiPNOScanInfo.a24GProbeTemplate, 
                  pPrefNetwListParams.us24GProbeSize); 

   pPrefNetwListParams.us5GProbeSize = 
     (pwdiPNOScanReqParams->wdiPNOScanInfo.us5GProbeSize <
     WLAN_HAL_PNO_MAX_PROBE_SIZE)?
     pwdiPNOScanReqParams->wdiPNOScanInfo.us5GProbeSize:
     WLAN_HAL_PNO_MAX_PROBE_SIZE; 

   wpalMemoryCopy(pPrefNetwListParams.a5GProbeTemplate, 
                  pwdiPNOScanReqParams->wdiPNOScanInfo.a5GProbeTemplate, 
                  pPrefNetwListParams.us5GProbeSize); 

   /*Pack the buffer*/
   wpalMemoryCopy( pSendBuffer+usDataOffset, &pPrefNetwListParams, 
                   sizeof(pPrefNetwListParams)); 

   /*Set the output values*/
   *ppSendBuffer = pSendBuffer;
   *pSize        = usSendSize; 

   return WDI_STATUS_SUCCESS;
}/*WDI_PackPreferredNetworkListNew*/

/**
 @brief Process Set Preferred Network List Request function
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetPreferredNetworkReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_PNOScanReqParamsType*  pwdiPNOScanReqParams  = NULL;
   WDI_PNOScanCb              wdiPNOScanCb          = NULL;
   wpt_uint8*                 pSendBuffer           = NULL; 
   wpt_uint16                 usSendSize            = 0;
   WDI_Status                 wdiStatus; 

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pEventData ) ||
       ( NULL == (pwdiPNOScanReqParams = (WDI_PNOScanReqParamsType*)pEventData->pEventData)) ||
       ( NULL == (wdiPNOScanCb   = (WDI_PNOScanCb)pEventData->pCBfnc)))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   /*-------------------------------------------------------------------------
     Pack the PNO request structure based on version
   -------------------------------------------------------------------------*/
   if ( pWDICtx->wdiPNOVersion > 0 )
   {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
                  "%s: PNO new version %d ", __FUNCTION__, 
                  pWDICtx->wdiPNOVersion);

     wdiStatus = WDI_PackPreferredNetworkListNew( pWDICtx, pwdiPNOScanReqParams,
                                      &pSendBuffer, &usSendSize);
   }
   else
   {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
                  "%s: PNO old version %d ", __FUNCTION__, 
                  pWDICtx->wdiPNOVersion);

     wdiStatus = WDI_PackPreferredNetworkList( pWDICtx, pwdiPNOScanReqParams,
                                      &pSendBuffer, &usSendSize);
   }

   if (( WDI_STATUS_SUCCESS != wdiStatus )||
       ( NULL == pSendBuffer )||( 0 == usSendSize ))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: failed to pack request parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return wdiStatus; 
   }

   pWDICtx->wdiReqStatusCB     = pwdiPNOScanReqParams->wdiReqStatusCB;
   pWDICtx->pReqStatusUserData = pwdiPNOScanReqParams->pUserData; 

   /*-------------------------------------------------------------------------
     Send Get STA Request to HAL 
   -------------------------------------------------------------------------*/
   return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
               wdiPNOScanCb, pEventData->pUserData, WDI_SET_PREF_NETWORK_RESP); 
}

/**
 @brief Process Set RSSI Filter Request function
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetRssiFilterReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_SetRssiFilterReqParamsType* pwdiRssiFilterReqParams = NULL;
   WDI_RssiFilterCb                wdiRssiFilterCb         = NULL;
   wpt_uint8*                      pSendBuffer             = NULL; 
   wpt_uint16                      usDataOffset            = 0;
   wpt_uint16                      usSendSize              = 0;
   wpt_uint8                       ucRssiThreshold;

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pEventData ) ||
       ( NULL == (pwdiRssiFilterReqParams = (WDI_SetRssiFilterReqParamsType*)pEventData->pEventData)) ||
       ( NULL == (wdiRssiFilterCb   = (WDI_RssiFilterCb)pEventData->pCBfnc)))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   /*-----------------------------------------------------------------------
     Get message buffer
   -----------------------------------------------------------------------*/
   if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_SET_PREF_NETWORK_REQ, 
                         sizeof(ucRssiThreshold),
                         &pSendBuffer, &usDataOffset, &usSendSize))||
       ( usSendSize < (usDataOffset + sizeof(ucRssiThreshold) )))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
                  "Unable to get send buffer in Set PNO req %x %x %x",
                  pEventData, pwdiRssiFilterReqParams, wdiRssiFilterCb);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   ucRssiThreshold = pwdiRssiFilterReqParams->rssiThreshold;

   wpalMemoryCopy( pSendBuffer+usDataOffset, 
                   &ucRssiThreshold, 
                   sizeof(ucRssiThreshold)); 

   pWDICtx->wdiReqStatusCB     = pwdiRssiFilterReqParams->wdiReqStatusCB;
   pWDICtx->pReqStatusUserData = pwdiRssiFilterReqParams->pUserData; 

   /*-------------------------------------------------------------------------
     Send Get STA Request to HAL 
   -------------------------------------------------------------------------*/
   return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        wdiRssiFilterCb, pEventData->pUserData, WDI_SET_RSSI_FILTER_RESP); 
}


/**
 @brief Process Update Scan Params function
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessUpdateScanParamsReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_UpdateScanParamsInfoType* pwdiUpdateScanParams  = NULL;
   WDI_UpdateScanParamsCb        wdiUpdateScanParamsCb = NULL;
   wpt_uint8*                    pSendBuffer           = NULL; 
   wpt_uint16                    usDataOffset          = 0;
   wpt_uint16                    usSendSize            = 0;
   tUpdateScanParams             updateScanParams;

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pEventData ) ||
       ( NULL == (pwdiUpdateScanParams = (WDI_UpdateScanParamsInfoType*)pEventData->pEventData)) ||
       ( NULL == (wdiUpdateScanParamsCb   = (WDI_UpdateScanParamsCb)pEventData->pCBfnc)))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_INFO,
               "Begin WDI Update Scan Parameters");
   /*-----------------------------------------------------------------------
     Get message buffer
   -----------------------------------------------------------------------*/
   if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_UPDATE_SCAN_PARAMS_REQ, 
                         sizeof(updateScanParams),
                         &pSendBuffer, &usDataOffset, &usSendSize))||
       ( usSendSize < (usDataOffset + sizeof(updateScanParams) )))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
                  "Unable to get send buffer in Update Scan Params req %x %x %x",
                  pEventData, pwdiUpdateScanParams, wdiUpdateScanParamsCb);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   //
   // Fill updateScanParams from pwdiUpdateScanParams->wdiUpdateScanParamsInfo
   //

   updateScanParams.b11dEnabled    = pwdiUpdateScanParams->wdiUpdateScanParamsInfo.b11dEnabled;
   updateScanParams.b11dResolved   = pwdiUpdateScanParams->wdiUpdateScanParamsInfo.b11dResolved;
   updateScanParams.ucChannelCount = 
     (pwdiUpdateScanParams->wdiUpdateScanParamsInfo.ucChannelCount <
     WLAN_HAL_PNO_MAX_NETW_CHANNELS)?
     pwdiUpdateScanParams->wdiUpdateScanParamsInfo.ucChannelCount :
     WLAN_HAL_PNO_MAX_NETW_CHANNELS;

   wpalMemoryCopy( updateScanParams.aChannels, 
                   pwdiUpdateScanParams->wdiUpdateScanParamsInfo.aChannels,
                   updateScanParams.ucChannelCount);

   updateScanParams.usActiveMinChTime  = pwdiUpdateScanParams->wdiUpdateScanParamsInfo.usActiveMinChTime;
   updateScanParams.usActiveMaxChTime  = pwdiUpdateScanParams->wdiUpdateScanParamsInfo.usActiveMaxChTime;
   updateScanParams.usPassiveMinChTime = pwdiUpdateScanParams->wdiUpdateScanParamsInfo.usPassiveMinChTime;
   updateScanParams.usPassiveMaxChTime = pwdiUpdateScanParams->wdiUpdateScanParamsInfo.usPassiveMaxChTime;
   updateScanParams.cbState            = pwdiUpdateScanParams->wdiUpdateScanParamsInfo.cbState;

   wpalMemoryCopy( pSendBuffer+usDataOffset, 
                   &updateScanParams, 
                   sizeof(updateScanParams)); 

   pWDICtx->wdiReqStatusCB     = pwdiUpdateScanParams->wdiReqStatusCB;
   pWDICtx->pReqStatusUserData = pwdiUpdateScanParams->pUserData; 

   WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_INFO,
               "End Update Scan Parameters");
   /*-------------------------------------------------------------------------
     Send Get STA Request to HAL 
   -------------------------------------------------------------------------*/
   return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        wdiUpdateScanParamsCb, pEventData->pUserData, WDI_UPDATE_SCAN_PARAMS_RESP); 
}

/**
 @brief Process Preferred Network Found Indication function
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessPrefNetworkFoundInd
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  WDI_LowLevelIndType  wdiInd;
  tPrefNetwFoundInd    prefNetwFoundInd;

  /*-------------------------------------------------------------------------
    Sanity check 
  -------------------------------------------------------------------------*/
  if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
      ( NULL == pEventData->pEventData ))
  {
     WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                 "%s: Invalid parameters", __FUNCTION__);
     WDI_ASSERT( 0 );
     return WDI_STATUS_E_FAILURE; 
  }

  /*-------------------------------------------------------------------------
    Extract indication and send it to UMAC
  -------------------------------------------------------------------------*/
  wpalMemoryCopy( (void *)&prefNetwFoundInd.prefNetwFoundParams, 
                  pEventData->pEventData, 
                  sizeof(tPrefNetwFoundParams));

  /*Fill in the indication parameters*/
  wdiInd.wdiIndicationType = WDI_PREF_NETWORK_FOUND_IND; 

  wpalMemoryZero(wdiInd.wdiIndicationData.wdiPrefNetworkFoundInd.ssId.sSSID,32);

  wdiInd.wdiIndicationData.wdiPrefNetworkFoundInd.ssId.ucLength = 
     (prefNetwFoundInd.prefNetwFoundParams.ssId.length < 31 )?
      prefNetwFoundInd.prefNetwFoundParams.ssId.length : 31; 

  wpalMemoryCopy( wdiInd.wdiIndicationData.wdiPrefNetworkFoundInd.ssId.sSSID, 
                  prefNetwFoundInd.prefNetwFoundParams.ssId.ssId, 
                  wdiInd.wdiIndicationData.wdiPrefNetworkFoundInd.ssId.ucLength);

  wdiInd.wdiIndicationData.wdiPrefNetworkFoundInd.rssi =
     prefNetwFoundInd.prefNetwFoundParams.rssi;

  // DEBUG
  WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_FATAL,
              "[PNO WDI] PREF_NETWORK_FOUND_IND Type (%x) data (SSID=%s, RSSI=%d)",
              wdiInd.wdiIndicationType,
              wdiInd.wdiIndicationData.wdiPrefNetworkFoundInd.ssId.sSSID,
              wdiInd.wdiIndicationData.wdiPrefNetworkFoundInd.rssi );

  /*Notify UMAC*/
  pWDICtx->wdiLowLevelIndCB( &wdiInd, pWDICtx->pIndUserData );
  
  return WDI_STATUS_SUCCESS; 
}

/**
 @brief Process PNO Rsp function (called when a
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetPreferredNetworkRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_Status           wdiStatus;
   eHalStatus           halStatus;
   WDI_PNOScanCb       wdiPNOScanCb   = NULL;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
       ( NULL == pEventData->pEventData ))
   {
      WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }


   wdiPNOScanCb = (WDI_PNOScanCb)pWDICtx->pfncRspCB; 

   /*-------------------------------------------------------------------------
     Extract response and send it to UMAC
   -------------------------------------------------------------------------*/
   halStatus = *((eHalStatus*)pEventData->pEventData);
   wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

   /*Notify UMAC*/
   wdiPNOScanCb(wdiStatus, pWDICtx->pRspCBUserData);

   return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessSetPreferredNetworkRsp*/

/**
 @brief Process RSSI Filter Rsp function (called when a
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetRssiFilterRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_Status           wdiStatus;
   eHalStatus           halStatus;
   WDI_RssiFilterCb     wdiRssiFilterCb;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
       ( NULL == pEventData->pEventData ))
   {
      WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   wdiRssiFilterCb = (WDI_RssiFilterCb)pWDICtx->pfncRspCB; 

   /*-------------------------------------------------------------------------
     Extract response and send it to UMAC
   -------------------------------------------------------------------------*/
   halStatus = *((eHalStatus*)pEventData->pEventData);
   wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

   /*Notify UMAC*/
   wdiRssiFilterCb(wdiStatus, pWDICtx->pRspCBUserData);

   return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessSetRssiFilterRsp*/

/**
 @brief Process Update Scan Params Rsp function (called when a
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessUpdateScanParamsRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_Status             wdiStatus;
   tUpdateScanParamsResp  halUpdScanParams; 
   WDI_UpdateScanParamsCb wdiUpdateScanParamsCb   = NULL;
   wpt_uint32             uStatus; 
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
       ( NULL == pEventData->pEventData ))
   {
      WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
                  "Process UPD scan params ptr : %x", __FUNCTION__);

  wdiUpdateScanParamsCb = (WDI_UpdateScanParamsCb)pWDICtx->pfncRspCB; 

  /*-------------------------------------------------------------------------
     Extract response and send it to UMAC
   -------------------------------------------------------------------------*/
  wpalMemoryCopy( (void *)&halUpdScanParams.status, 
                  pEventData->pEventData, 
                  sizeof(halUpdScanParams.status));

  uStatus  = halUpdScanParams.status;

  /*Extract PNO version - 1st bit of the status */
  pWDICtx->wdiPNOVersion = (uStatus & WDI_PNO_VERSION_MASK)? 1:0; 

  /*Remove version bit*/
  uStatus = uStatus & ( ~(WDI_PNO_VERSION_MASK)); 

  wdiStatus   =   WDI_HAL_2_WDI_STATUS(uStatus); 

  WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_INFO,
              "UPD Scan Parameters rsp with status: %d", 
              halUpdScanParams.status);

  /*Notify UMAC*/
  wdiUpdateScanParamsCb(wdiStatus, pWDICtx->pRspCBUserData);

  return WDI_STATUS_SUCCESS; 
}
#endif // FEATURE_WLAN_SCAN_PNO

#ifdef WLAN_FEATURE_PACKET_FILTERING
WDI_Status 
WDI_8023MulticastListReq
(
  WDI_RcvFltPktSetMcListReqParamsType*  pwdiRcvFltPktSetMcListReqInfo,
  WDI_8023MulticastListCb               wdi8023MulticastListCallback,
  void*                                 pUserData
)
{
   WDI_EventInfoType      wdiEventData;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
             "%s", __FUNCTION__);

   /*------------------------------------------------------------------------
     Sanity Check 
   ------------------------------------------------------------------------*/
   if ( eWLAN_PAL_FALSE == gWDIInitialized )
   {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WDI API call before module is initialized - Fail request");

     return WDI_STATUS_E_NOT_ALLOWED; 
   }

   /*------------------------------------------------------------------------
     Fill in Event data and post to the Main FSM
   ------------------------------------------------------------------------*/
   wdiEventData.wdiRequest      = WDI_8023_MULTICAST_LIST_REQ;
   wdiEventData.pEventData      = pwdiRcvFltPktSetMcListReqInfo; 
   wdiEventData.uEventDataSize  = sizeof(*pwdiRcvFltPktSetMcListReqInfo);
   wdiEventData.pCBfnc          = wdi8023MulticastListCallback; 
   wdiEventData.pUserData       = pUserData;

   return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);
}

WDI_Status 
WDI_ReceiveFilterSetFilterReq
(
  WDI_SetRcvPktFilterReqParamsType* pwdiSetRcvPktFilterReqInfo,
  WDI_ReceiveFilterSetFilterCb      wdiReceiveFilterSetFilterCallback,
  void*                             pUserData
)
{
   WDI_EventInfoType      wdiEventData;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
             "%s",__FUNCTION__);

   /*------------------------------------------------------------------------
     Sanity Check 
   ------------------------------------------------------------------------*/
   if ( eWLAN_PAL_FALSE == gWDIInitialized )
   {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WDI API call before module is initialized - Fail request");

     return WDI_STATUS_E_NOT_ALLOWED; 
   }

   /*------------------------------------------------------------------------
     Fill in Event data and post to the Main FSM
   ------------------------------------------------------------------------*/
   wdiEventData.wdiRequest      = WDI_RECEIVE_FILTER_SET_FILTER_REQ;
   wdiEventData.pEventData      = pwdiSetRcvPktFilterReqInfo; 
   wdiEventData.uEventDataSize  = sizeof(*pwdiSetRcvPktFilterReqInfo) + 
                                  (pwdiSetRcvPktFilterReqInfo->wdiPktFilterCfg.numFieldParams 
                                  * sizeof(WDI_RcvPktFilterFieldParams) - 1);
   wdiEventData.pCBfnc          = wdiReceiveFilterSetFilterCallback; 
   wdiEventData.pUserData       = pUserData;


   return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);
}

WDI_Status 
WDI_FilterMatchCountReq
(
  WDI_RcvFltPktMatchCntReqParamsType* pwdiRcvFltPktMatchCntReqInfo,
  WDI_FilterMatchCountCb              wdiFilterMatchCountCallback,
  void*                               pUserData
)
{
   WDI_EventInfoType      wdiEventData;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
             "%s",__FUNCTION__);

   /*------------------------------------------------------------------------
     Sanity Check 
   ------------------------------------------------------------------------*/
   if ( eWLAN_PAL_FALSE == gWDIInitialized )
   {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WDI API call before module is initialized - Fail request");

     return WDI_STATUS_E_NOT_ALLOWED; 
   }

   /*------------------------------------------------------------------------
     Fill in Event data and post to the Main FSM
   ------------------------------------------------------------------------*/
   wdiEventData.wdiRequest      = WDI_PACKET_COALESCING_FILTER_MATCH_COUNT_REQ;
   wdiEventData.pEventData      = pwdiRcvFltPktMatchCntReqInfo; 
   wdiEventData.uEventDataSize  = sizeof(*pwdiRcvFltPktMatchCntReqInfo);
   wdiEventData.pCBfnc          = wdiFilterMatchCountCallback; 
   wdiEventData.pUserData       = pUserData;


   return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);
}

WDI_Status 
WDI_ReceiveFilterClearFilterReq
(
  WDI_RcvFltPktClearReqParamsType*  pwdiRcvFltPktClearReqInfo,
  WDI_ReceiveFilterClearFilterCb    wdiReceiveFilterClearFilterCallback,
  void*                             pUserData
)
{
   WDI_EventInfoType      wdiEventData;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
             "%s",__FUNCTION__);

   /*------------------------------------------------------------------------
     Sanity Check 
   ------------------------------------------------------------------------*/
   if ( eWLAN_PAL_FALSE == gWDIInitialized )
   {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WDI API call before module is initialized - Fail request");

     return WDI_STATUS_E_NOT_ALLOWED; 
   }

   /*------------------------------------------------------------------------
     Fill in Event data and post to the Main FSM
   ------------------------------------------------------------------------*/
   wdiEventData.wdiRequest      = WDI_RECEIVE_FILTER_CLEAR_FILTER_REQ;
   wdiEventData.pEventData      = pwdiRcvFltPktClearReqInfo; 
   wdiEventData.uEventDataSize  = sizeof(*pwdiRcvFltPktClearReqInfo);
   wdiEventData.pCBfnc          = wdiReceiveFilterClearFilterCallback; 
   wdiEventData.pUserData       = pUserData;


   return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);
}

/**
 @brief Process 8023 Multicast List Request function
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_Process8023MulticastListReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_RcvFltPktSetMcListReqParamsType* pwdiFltPktSetMcListReqParamsType  = NULL;
   WDI_8023MulticastListCb    wdi8023MulticastListCb = NULL;
   wpt_uint8*                 pSendBuffer           = NULL; 
   wpt_uint16                 usDataOffset          = 0;
   wpt_uint16                 usSendSize            = 0;
   tHalRcvFltMcAddrListType   rcvFltMcAddrListType;
   wpt_uint8                  i;

   WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
             "%s",__FUNCTION__);

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pEventData ) ||
       ( NULL == (pwdiFltPktSetMcListReqParamsType = 
       (WDI_RcvFltPktSetMcListReqParamsType*)pEventData->pEventData)) ||
       ( NULL == (wdi8023MulticastListCb = 
       (WDI_8023MulticastListCb)pEventData->pCBfnc)))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   /*-----------------------------------------------------------------------
     Get message buffer
     ! TO DO : proper conversion into the HAL Message Request Format 
   -----------------------------------------------------------------------*/
   if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, 
                         WDI_8023_MULTICAST_LIST_REQ, 
                         sizeof(tHalRcvFltMcAddrListType),
                         &pSendBuffer, &usDataOffset, &usSendSize))||
       ( usSendSize < (usDataOffset + sizeof(tHalRcvFltMcAddrListType))))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
                  "Unable to get send buffer in "
                  "WDI_Process8023MulticastListReq() %x %x %x",
                  pEventData, pwdiFltPktSetMcListReqParamsType,
                  wdi8023MulticastListCb);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   rcvFltMcAddrListType.cMulticastAddr = 
       pwdiFltPktSetMcListReqParamsType->mcAddrList.ulMulticastAddrCnt; 
   for( i = 0; i < rcvFltMcAddrListType.cMulticastAddr; i++ )
   {
      wpalMemoryCopy(rcvFltMcAddrListType.multicastAddr[i],
                 pwdiFltPktSetMcListReqParamsType->mcAddrList.multicastAddr[i],
                 sizeof(tSirMacAddr));
   }

   wpalMemoryCopy( pSendBuffer+usDataOffset, 
                   &rcvFltMcAddrListType, 
                   sizeof(rcvFltMcAddrListType)); 

   pWDICtx->wdiReqStatusCB     = pwdiFltPktSetMcListReqParamsType->wdiReqStatusCB;
   pWDICtx->pReqStatusUserData = pwdiFltPktSetMcListReqParamsType->pUserData; 


   /*-------------------------------------------------------------------------
     Send Get STA Request to HAL 
   -------------------------------------------------------------------------*/
   return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        wdi8023MulticastListCb, pEventData->pUserData,
                        WDI_8023_MULTICAST_LIST_RESP); 
}

/**
 @brief Process Receive Filter Set Filter Request function
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessReceiveFilterSetFilterReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_SetRcvPktFilterReqParamsType* pwdiSetRcvPktFilterReqInfo  = NULL;
   WDI_ReceiveFilterSetFilterCb      wdiReceiveFilterSetFilterCb = NULL;
   wpt_uint8*                 pSendBuffer           = NULL; 
   wpt_uint16                 usDataOffset          = 0;
   wpt_uint16                 usSendSize            = 0;
   wpt_uint32                 usRcvPktFilterCfgSize;
   tHalRcvPktFilterCfgType    *pRcvPktFilterCfg;
   wpt_uint8                  i;

   WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
             "%s",__FUNCTION__);

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pEventData ) ||
       ( NULL == (pwdiSetRcvPktFilterReqInfo = 
       (WDI_SetRcvPktFilterReqParamsType*)pEventData->pEventData)) ||
       ( NULL == (wdiReceiveFilterSetFilterCb = 
       (WDI_ReceiveFilterSetFilterCb)pEventData->pCBfnc)))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   usRcvPktFilterCfgSize = sizeof(tHalRcvPktFilterCfgType) + 
       ((pwdiSetRcvPktFilterReqInfo->wdiPktFilterCfg.numFieldParams - 1)
        * sizeof(tHalRcvPktFilterParams));

  pRcvPktFilterCfg = (tHalRcvPktFilterCfgType *)wpalMemoryAllocate(
                                              usRcvPktFilterCfgSize);

  if(NULL == pRcvPktFilterCfg)
  {
    WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
            "%s: Failed to allocate memory for "
            "tHalRcvPktFilterCfgType: %x %x %x ",
            __FUNCTION__, pWDICtx, pEventData, pEventData->pEventData);
    WDI_ASSERT(0);
    return WDI_STATUS_E_FAILURE; 
  }

  wpalMemoryZero(pRcvPktFilterCfg, usRcvPktFilterCfgSize);

   /*-----------------------------------------------------------------------
     Get message buffer
     ! TO DO : proper conversion into the HAL Message Request Format 
   -----------------------------------------------------------------------*/
   if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_RECEIVE_FILTER_SET_FILTER_REQ, 
                         usRcvPktFilterCfgSize,
                         &pSendBuffer, &usDataOffset, &usSendSize))||
       ( usSendSize < (usDataOffset + usRcvPktFilterCfgSize)))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
                  "Unable to get send buffer in "
                  "WDI_ProcessReceiveFilterSetFilterReq() %x %x %x",
                  pEventData, pwdiSetRcvPktFilterReqInfo,
                  wdiReceiveFilterSetFilterCb);
      WDI_ASSERT(0);
      wpalMemoryFree(pRcvPktFilterCfg);
      return WDI_STATUS_E_FAILURE; 
   }

   WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
              "UsData Off %d UsSend %d cfg %d",usDataOffset, 
              usSendSize,usRcvPktFilterCfgSize);

   pRcvPktFilterCfg->filterId = pwdiSetRcvPktFilterReqInfo->wdiPktFilterCfg.filterId;
   pRcvPktFilterCfg->filterType = pwdiSetRcvPktFilterReqInfo->wdiPktFilterCfg.filterType;   
   pRcvPktFilterCfg->numParams = pwdiSetRcvPktFilterReqInfo->wdiPktFilterCfg.numFieldParams;
   pRcvPktFilterCfg->coleasceTime = pwdiSetRcvPktFilterReqInfo->wdiPktFilterCfg.coalesceTime;


   WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
              "Out: FID %d FT %d",pRcvPktFilterCfg->filterId, 
              pRcvPktFilterCfg->filterType);
   WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
              "NParams %d CT %d",pRcvPktFilterCfg->numParams, 
              pRcvPktFilterCfg->coleasceTime);

   for ( i = 0; i < pRcvPktFilterCfg->numParams; i++ )
   {
       pRcvPktFilterCfg->paramsData[i].protocolLayer = 
           pwdiSetRcvPktFilterReqInfo->wdiPktFilterCfg.paramsData[i].protocolLayer;
       pRcvPktFilterCfg->paramsData[i].cmpFlag = 
           pwdiSetRcvPktFilterReqInfo->wdiPktFilterCfg.paramsData[i].cmpFlag;
       pRcvPktFilterCfg->paramsData[i].dataOffset = 
           pwdiSetRcvPktFilterReqInfo->wdiPktFilterCfg.paramsData[i].dataOffset;
        pRcvPktFilterCfg->paramsData[i].dataLength = 
            pwdiSetRcvPktFilterReqInfo->wdiPktFilterCfg.paramsData[i].dataLength;

       wpalMemoryCopy(&pRcvPktFilterCfg->paramsData[i].compareData,
                    &pwdiSetRcvPktFilterReqInfo->wdiPktFilterCfg.paramsData[i].compareData,
                    8);
       wpalMemoryCopy(&pRcvPktFilterCfg->paramsData[i].dataMask,
                    &pwdiSetRcvPktFilterReqInfo->wdiPktFilterCfg.paramsData[i].dataMask,
                    8);

      WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
           "Out:Proto %d Comp Flag %d \n",
           pRcvPktFilterCfg->paramsData[i].protocolLayer, 
           pRcvPktFilterCfg->paramsData[i].cmpFlag);

      WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
           "Data Offset %d Data Len %d\n",
           pRcvPktFilterCfg->paramsData[i].dataOffset, 
           pRcvPktFilterCfg->paramsData[i].dataLength);

      WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
           "CData: %d:%d:%d:%d:%d:%d\n",
           pRcvPktFilterCfg->paramsData[i].compareData[0], 
           pRcvPktFilterCfg->paramsData[i].compareData[1], 
           pRcvPktFilterCfg->paramsData[i].compareData[2], 
           pRcvPktFilterCfg->paramsData[i].compareData[3],
           pRcvPktFilterCfg->paramsData[i].compareData[4], 
           pRcvPktFilterCfg->paramsData[i].compareData[5]);

      WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
           "MData: %d:%d:%d:%d:%d:%d\n",
           pRcvPktFilterCfg->paramsData[i].dataMask[0], 
           pRcvPktFilterCfg->paramsData[i].dataMask[1], 
           pRcvPktFilterCfg->paramsData[i].dataMask[2], 
           pRcvPktFilterCfg->paramsData[i].dataMask[3],
           pRcvPktFilterCfg->paramsData[i].dataMask[4], 
           pRcvPktFilterCfg->paramsData[i].dataMask[5]);
   }

   wpalMemoryCopy( pSendBuffer+usDataOffset, 
                   pRcvPktFilterCfg, 
                   usRcvPktFilterCfgSize); 


   pWDICtx->wdiReqStatusCB     = pwdiSetRcvPktFilterReqInfo->wdiReqStatusCB;
   pWDICtx->pReqStatusUserData = pwdiSetRcvPktFilterReqInfo->pUserData; 

   WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
             "%s",__FUNCTION__);
   wpalMemoryFree(pRcvPktFilterCfg);

   /*-------------------------------------------------------------------------
     Send Get STA Request to HAL 
   -------------------------------------------------------------------------*/
   return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        wdiReceiveFilterSetFilterCb, pEventData->pUserData,
                        WDI_RECEIVE_FILTER_SET_FILTER_RESP); 
}

/**
 @brief Process Packet Filter Match Count Request function
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessFilterMatchCountReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_RcvFltPktMatchCntReqParamsType*    pwdiRcvFltPktMatchCntReqParamsType =
                                                                         NULL;
   WDI_FilterMatchCountCb               wdiFilterMatchCountCb            =
                                                                         NULL;
   wpt_uint8*                             pSendBuffer           = NULL; 
   wpt_uint16                             usDataOffset          = 0;
   wpt_uint16                             usSendSize            = 0;

   WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
             "%s",__FUNCTION__);

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pEventData ) ||
       ( NULL == (pwdiRcvFltPktMatchCntReqParamsType = 
       (WDI_RcvFltPktMatchCntReqParamsType*)pEventData->pEventData)) ||
       ( NULL == (wdiFilterMatchCountCb = 
       (WDI_FilterMatchCountCb)pEventData->pCBfnc)))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   /*-----------------------------------------------------------------------
     Get message buffer
     ! TO DO : proper conversion into the HAL Message Request Format 
   -----------------------------------------------------------------------*/
   if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, 
                         WDI_PACKET_COALESCING_FILTER_MATCH_COUNT_REQ, 
                         0,
                         &pSendBuffer, &usDataOffset, &usSendSize))||
       ( usSendSize < usDataOffset))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
                  "Unable to get send buffer in "
                  "WDI_ProcessFilterMatchCountReq() %x %x %x",
                  pEventData, pwdiRcvFltPktMatchCntReqParamsType,
                  wdiFilterMatchCountCb);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   //
   // Don't need to fill send buffer other than header
   //
   pWDICtx->wdiReqStatusCB     = pwdiRcvFltPktMatchCntReqParamsType->wdiReqStatusCB;
   pWDICtx->pReqStatusUserData = pwdiRcvFltPktMatchCntReqParamsType->pUserData; 


   /*-------------------------------------------------------------------------
     Send Get STA Request to HAL 
   -------------------------------------------------------------------------*/
   return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        wdiFilterMatchCountCb, 
                        pEventData->pUserData, 
                        WDI_PACKET_COALESCING_FILTER_MATCH_COUNT_RESP); 
}

/**
 @brief Process Receive Filter Clear Filter Request function
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessReceiveFilterClearFilterReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{ 
   WDI_RcvFltPktClearReqParamsType* pwdiRcvFltPktClearReqParamsType = NULL;
   WDI_ReceiveFilterClearFilterCb   wdiRcvFltPktClearFilterCb       = NULL;
   wpt_uint8*                       pSendBuffer           = NULL; 
   wpt_uint16                       usDataOffset          = 0;
   wpt_uint16                       usSendSize            = 0;
   tHalRcvFltPktClearParam          rcvFltPktClearParam;

   WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
             "%s",__FUNCTION__);

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pEventData ) ||
       ( NULL == (pwdiRcvFltPktClearReqParamsType =
       (WDI_RcvFltPktClearReqParamsType*)pEventData->pEventData)) ||
       ( NULL == (wdiRcvFltPktClearFilterCb = 
       (WDI_ReceiveFilterClearFilterCb)pEventData->pCBfnc)))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   /*-----------------------------------------------------------------------
     Get message buffer
     ! TO DO : proper conversion into the HAL Message Request Format 
   -----------------------------------------------------------------------*/
   if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx,
                         WDI_RECEIVE_FILTER_CLEAR_FILTER_REQ, 
                         sizeof(tHalRcvFltPktClearParam),
                         &pSendBuffer, &usDataOffset, &usSendSize))||
       ( usSendSize < (usDataOffset + sizeof(tHalRcvFltPktClearParam))))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
                  "Unable to get send buffer in "
                  "WDI_ProcessReceiveFilterClearFilterReq() %x %x %x",
                  pEventData, pwdiRcvFltPktClearReqParamsType,
                  wdiRcvFltPktClearFilterCb);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }


   rcvFltPktClearParam.status = pwdiRcvFltPktClearReqParamsType->
                                                    filterClearParam.status; 
   rcvFltPktClearParam.filterId = pwdiRcvFltPktClearReqParamsType->
                                                    filterClearParam.filterId; 

   wpalMemoryCopy( pSendBuffer+usDataOffset, 
                   &rcvFltPktClearParam, 
                   sizeof(rcvFltPktClearParam)); 

   pWDICtx->wdiReqStatusCB     = pwdiRcvFltPktClearReqParamsType->wdiReqStatusCB;
   pWDICtx->pReqStatusUserData = pwdiRcvFltPktClearReqParamsType->pUserData; 


   /*-------------------------------------------------------------------------
     Send Get STA Request to HAL 
   -------------------------------------------------------------------------*/
   return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        wdiRcvFltPktClearFilterCb, pEventData->pUserData,
                        WDI_RECEIVE_FILTER_CLEAR_FILTER_RESP); 
}

/**
 @brief Process 8023 Multicast List Response function
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_Process8023MulticastListRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_Status           wdiStatus;
   eHalStatus           halStatus;
   WDI_8023MulticastListCb wdi8023MulticastListCb;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
             "%s",__FUNCTION__);

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
       ( NULL == pEventData->pEventData ))
   {
      WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   wdi8023MulticastListCb = (WDI_8023MulticastListCb)pWDICtx->pfncRspCB; 

   /*-------------------------------------------------------------------------
     Extract response and send it to UMAC
   -------------------------------------------------------------------------*/
   halStatus = *((eHalStatus*)pEventData->pEventData);
   wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

   /*Notify UMAC*/
   wdi8023MulticastListCb(wdiStatus, pWDICtx->pRspCBUserData);

   return WDI_STATUS_SUCCESS; 
}

/**
 @brief Process Set Rsp function (called when a
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessReceiveFilterSetFilterRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_Status           wdiStatus;
   eHalStatus           halStatus;
   WDI_ReceiveFilterSetFilterCb wdiReceiveFilterSetFilterCb;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
          "%s",__FUNCTION__);

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
       ( NULL == pEventData->pEventData ))
   {
      WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   wdiReceiveFilterSetFilterCb = (WDI_ReceiveFilterSetFilterCb)pWDICtx->
                                                                   pfncRspCB; 

   /*-------------------------------------------------------------------------
     Extract response and send it to UMAC
   -------------------------------------------------------------------------*/
   halStatus = *((eHalStatus*)pEventData->pEventData);
   wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

   /*Notify UMAC*/
   wdiReceiveFilterSetFilterCb(wdiStatus, pWDICtx->pRspCBUserData);

   return WDI_STATUS_SUCCESS; 
}

/**
 @brief Process Packet Filter Match Count Response function
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessFilterMatchCountRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_Status           wdiStatus;
   eHalStatus           halStatus;

   WDI_FilterMatchCountCb   wdiFilterMatchCountCb;

   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
             "%s",__FUNCTION__);

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
       ( NULL == pEventData->pEventData ))
   {
      WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   wdiFilterMatchCountCb = (WDI_FilterMatchCountCb)pWDICtx->pfncRspCB; 

   /*-------------------------------------------------------------------------
     Extract response and send it to UMAC
   -------------------------------------------------------------------------*/
   halStatus = *((eHalStatus*)pEventData->pEventData);
   wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

   /*Notify UMAC*/
   wdiFilterMatchCountCb(wdiStatus, pWDICtx->pRspCBUserData);

   return WDI_STATUS_SUCCESS; 
}

/**
 @brief Process Receive Filter Clear Filter Response function
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessReceiveFilterClearFilterRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_Status           wdiStatus;
   eHalStatus           halStatus;
   WDI_ReceiveFilterClearFilterCb wdiReceiveFilterClearFilterCb;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_INFO,
             "%s",__FUNCTION__);

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
       ( NULL == pEventData->pEventData ))
   {
      WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   wdiReceiveFilterClearFilterCb = (WDI_ReceiveFilterClearFilterCb)pWDICtx->
                                                                 pfncRspCB; 

   /*-------------------------------------------------------------------------
     Extract response and send it to UMAC
   -------------------------------------------------------------------------*/
   halStatus = *((eHalStatus*)pEventData->pEventData);
   wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

   /*Notify UMAC*/
   wdiReceiveFilterClearFilterCb(wdiStatus, pWDICtx->pRspCBUserData);

   return WDI_STATUS_SUCCESS; 
}
#endif // WLAN_FEATURE_PACKET_FILTERING

/**
 @brief Process Shutdown Rsp function
        There is no shutdown response comming from HAL
        - function just kept for simmetry
 
 @param  pWDICtx:         pointer to the WLAN DAL context
         pEventData:      pointer to the event information structure 

 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessShutdownRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
  /*There is no shutdown response comming from HAL - function just kept for
  simmetry */
  WDI_ASSERT(0);
  return WDI_STATUS_SUCCESS;
}/*WDI_ProcessShutdownRsp*/

/**
 @brief WDI_SetPowerParamsReq

 @param pwdiPowerParamsReqParams: the Set Power Params as 
                      specified by the Device Interface
  
        wdiPowerParamsCb: callback for passing back the response
        of the Set Power Params operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @return Result of the function call
*/
WDI_Status 
WDI_SetPowerParamsReq
(
  WDI_SetPowerParamsReqParamsType* pwdiPowerParamsReqParams,
  WDI_SetPowerParamsCb             wdiPowerParamsCb,
  void*                            pUserData
)
{
   WDI_EventInfoType      wdiEventData;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*------------------------------------------------------------------------
     Sanity Check 
   ------------------------------------------------------------------------*/
   if ( eWLAN_PAL_FALSE == gWDIInitialized )
   {
     WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_ERROR,
               "WDI API call before module is initialized - Fail request");

     return WDI_STATUS_E_NOT_ALLOWED; 
   }

   /*------------------------------------------------------------------------
     Fill in Event data and post to the Main FSM
   ------------------------------------------------------------------------*/
   wdiEventData.wdiRequest      = WDI_SET_POWER_PARAMS_REQ;
   wdiEventData.pEventData      = pwdiPowerParamsReqParams; 
   wdiEventData.uEventDataSize  = sizeof(*pwdiPowerParamsReqParams);
   wdiEventData.pCBfnc          = wdiPowerParamsCb; 
   wdiEventData.pUserData       = pUserData;

   return WDI_PostMainEvent(&gWDICb, WDI_REQUEST_EVENT, &wdiEventData);
}/*WDI_SetPowerParamsReq*/

/**
 @brief Process Set Power Params Request function
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetPowerParamsReq
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_SetPowerParamsReqParamsType* pwdiPowerParamsReqParams = NULL;
   WDI_SetPowerParamsCb             wdiPowerParamsCb         = NULL;
   wpt_uint8*                       pSendBuffer              = NULL; 
   wpt_uint16                       usDataOffset             = 0;
   wpt_uint16                       usSendSize               = 0;
   tSetPowerParamsType              powerParams;

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pEventData ) ||
       ( NULL == (pwdiPowerParamsReqParams = (WDI_SetPowerParamsReqParamsType*)pEventData->pEventData)) ||
       ( NULL == (wdiPowerParamsCb   = (WDI_SetPowerParamsCb)pEventData->pCBfnc)))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   /*-----------------------------------------------------------------------
     Get message buffer
   -----------------------------------------------------------------------*/
   if (( WDI_STATUS_SUCCESS != WDI_GetMessageBuffer( pWDICtx, WDI_SET_POWER_PARAMS_REQ, 
                         sizeof(powerParams),
                         &pSendBuffer, &usDataOffset, &usSendSize))||
       ( usSendSize < (usDataOffset + sizeof(powerParams) )))
   {
      WPAL_TRACE( eWLAN_MODULE_DAL_CTRL,  eWLAN_PAL_TRACE_LEVEL_WARN,
                  "Unable to get send buffer in Set PNO req %x %x %x",
                  pEventData, pwdiPowerParamsReqParams, wdiPowerParamsCb);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

  /*  Ignore DTIM */
  powerParams.uIgnoreDTIM = 
    pwdiPowerParamsReqParams->wdiSetPowerParamsInfo.uIgnoreDTIM;

  /*DTIM Period*/
  powerParams.uDTIMPeriod = 
    pwdiPowerParamsReqParams->wdiSetPowerParamsInfo.uDTIMPeriod;

  /* Listen Interval */
  powerParams.uListenInterval= 
    pwdiPowerParamsReqParams->wdiSetPowerParamsInfo.uListenInterval;

  /* Broadcast Multicas Filter  */
  powerParams.uBcastMcastFilter = 
    pwdiPowerParamsReqParams->wdiSetPowerParamsInfo.uBcastMcastFilter;

  /* Beacon Early Termination */
  powerParams.uEnableBET = 
    pwdiPowerParamsReqParams->wdiSetPowerParamsInfo.uEnableBET;

  /* Beacon Early Termination Interval */
  powerParams.uBETInterval = 
    pwdiPowerParamsReqParams->wdiSetPowerParamsInfo.uBETInterval; 
    

   wpalMemoryCopy( pSendBuffer+usDataOffset, 
                   &powerParams, 
                   sizeof(powerParams)); 

   pWDICtx->wdiReqStatusCB     = pwdiPowerParamsReqParams->wdiReqStatusCB;
   pWDICtx->pReqStatusUserData = pwdiPowerParamsReqParams->pUserData; 

   /*-------------------------------------------------------------------------
     Send Get STA Request to HAL 
   -------------------------------------------------------------------------*/
   return  WDI_SendMsg( pWDICtx, pSendBuffer, usSendSize, 
                        wdiPowerParamsCb, pEventData->pUserData, WDI_SET_POWER_PARAMS_RESP); 
}

/**
 @brief Process Power Params Rsp function (called when a
        response is being received over the bus from HAL)
 
 @param  pWDICtx:         pointer to the WLAN DAL context 
         pEventData:      pointer to the event information structure 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_ProcessSetPowerParamsRsp
( 
  WDI_ControlBlockType*  pWDICtx,
  WDI_EventInfoType*     pEventData
)
{
   WDI_Status           wdiStatus;
   eHalStatus           halStatus;
   WDI_SetPowerParamsCb             wdiPowerParamsCb;
   /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

   /*-------------------------------------------------------------------------
     Sanity check 
   -------------------------------------------------------------------------*/
   if (( NULL == pWDICtx ) || ( NULL == pEventData ) ||
       ( NULL == pEventData->pEventData ))
   {
      WPAL_TRACE(eWLAN_MODULE_DAL_CTRL, eWLAN_PAL_TRACE_LEVEL_WARN,
                  "%s: Invalid parameters", __FUNCTION__);
      WDI_ASSERT(0);
      return WDI_STATUS_E_FAILURE; 
   }

   wdiPowerParamsCb = (WDI_SetPowerParamsCb)pWDICtx->pfncRspCB; 

   /*-------------------------------------------------------------------------
     Extract response and send it to UMAC
   -------------------------------------------------------------------------*/
   halStatus = *((eHalStatus*)pEventData->pEventData);
   wdiStatus   =   WDI_HAL_2_WDI_STATUS(halStatus); 

   /*Notify UMAC*/
   wdiPowerParamsCb(wdiStatus, pWDICtx->pRspCBUserData);

   return WDI_STATUS_SUCCESS; 
}/*WDI_ProcessSetPowerParamsRsp*/

