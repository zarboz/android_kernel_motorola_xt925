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

/*==========================================================================
 *
 *  @file:     wlan_hal_msg.h
 *
 *  @brief:    Exports and types for messages sent to HAL from WDI
 *
 *  @author:   Kumar Anand
 *
 *             Copyright (C) 2010, Qualcomm, Inc. 
 *             All rights reserved.
 *
 *=========================================================================*/

#ifndef _WLAN_HAL_MSG_H_
#define _WLAN_HAL_MSG_H_

#include "halLegacyPalTypes.h"
#include "halCompiler.h"
#include "wlan_qct_dev_defs.h"
#include "wlan_nv.h"

/*---------------------------------------------------------------------------
  API VERSIONING INFORMATION

  The RIVA API is versioned as MAJOR.MINOR.VERSION.REVISION
  The MAJOR is incremented for major product/architecture changes
      (and then MINOR/VERSION/REVISION are zeroed)
  The MINOR is incremented for minor product/architecture changes
      (and then VERSION/REVISION are zeroed)
  The VERSION is incremented if a significant API change occurs
      (and then REVISION is zeroed)
  The REVISION is incremented if an insignificant API change occurs
      or if a new API is added
  All values are in the range 0..255 (ie they are 8-bit values)
 ---------------------------------------------------------------------------*/
#define WLAN_HAL_VER_MAJOR 0
#define WLAN_HAL_VER_MINOR 0
#define WLAN_HAL_VER_VERSION 2
#define WLAN_HAL_VER_REVISION 0

/*---------------------------------------------------------------------------
  Commom Type definitons
 ---------------------------------------------------------------------------*/

//This is to force compiler to use the maximum of an int ( 4 bytes )
#define WLAN_HAL_MAX_ENUM_SIZE    0x7FFFFFFF

//Max no. of transmit categories
#define STACFG_MAX_TC    8

//The maximum value of access category
#define WLAN_HAL_MAX_AC  4

typedef tANI_U8 tSirMacAddr[6];
typedef tANI_U8 tHalIpv4Addr[4];

#define HAL_MAC_ADDR_LEN        6
#define HAL_IPV4_ADDR_LEN       4

#define WALN_HAL_STA_INVALID_IDX 0xFF
#define WLAN_HAL_BSS_INVALID_IDX 0xFF

//Default Beacon template size
#define BEACON_TEMPLATE_SIZE 0x180 

//Param Change Bitmap sent to HAL 
#define PARAM_BCN_INTERVAL_CHANGED                      (1 << 0)
#define PARAM_SHORT_PREAMBLE_CHANGED                 (1 << 1)
#define PARAM_SHORT_SLOT_TIME_CHANGED                 (1 << 2)
#define PARAM_llACOEXIST_CHANGED                            (1 << 3)
#define PARAM_llBCOEXIST_CHANGED                            (1 << 4)
#define PARAM_llGCOEXIST_CHANGED                            (1 << 5)
#define PARAM_HT20MHZCOEXIST_CHANGED                  (1<<6)
#define PARAM_NON_GF_DEVICES_PRESENT_CHANGED (1<<7)
#define PARAM_RIFS_MODE_CHANGED                            (1<<8)
#define PARAM_LSIG_TXOP_FULL_SUPPORT_CHANGED   (1<<9)
#define PARAM_OBSS_MODE_CHANGED                               (1<<10)
#define PARAM_BEACON_UPDATE_MASK                (PARAM_BCN_INTERVAL_CHANGED|PARAM_SHORT_PREAMBLE_CHANGED|PARAM_SHORT_SLOT_TIME_CHANGED|PARAM_llACOEXIST_CHANGED |PARAM_llBCOEXIST_CHANGED|\
    PARAM_llGCOEXIST_CHANGED|PARAM_HT20MHZCOEXIST_CHANGED|PARAM_NON_GF_DEVICES_PRESENT_CHANGED|PARAM_RIFS_MODE_CHANGED|PARAM_LSIG_TXOP_FULL_SUPPORT_CHANGED| PARAM_OBSS_MODE_CHANGED)

/*Dump command response Buffer size*/
#define DUMPCMD_RSP_BUFFER 100

/*Version string max length (including NUL) */
#define WLAN_HAL_VERSION_LENGTH  64

/*Max Num Of BSSIDS in INNAV_MEAS_REQ*/
#define MAX_BSSIDS_IN_INNAV_MEAS_REQ 1

/* Message types for messages exchanged between WDI and HAL */
typedef enum 
{
   //Init/De-Init
   WLAN_HAL_START_REQ = 0,
   WLAN_HAL_START_RSP = 1,
   WLAN_HAL_STOP_REQ  = 2,
   WLAN_HAL_STOP_RSP  = 3,

   //Scan
   WLAN_HAL_INIT_SCAN_REQ    = 4,
   WLAN_HAL_INIT_SCAN_RSP    = 5,
   WLAN_HAL_START_SCAN_REQ   = 6,
   WLAN_HAL_START_SCAN_RSP   = 7 ,
   WLAN_HAL_END_SCAN_REQ     = 8,
   WLAN_HAL_END_SCAN_RSP     = 9,
   WLAN_HAL_FINISH_SCAN_REQ  = 10,
   WLAN_HAL_FINISH_SCAN_RSP  = 11,

   // HW STA configuration/deconfiguration
   WLAN_HAL_CONFIG_STA_REQ   = 12,
   WLAN_HAL_CONFIG_STA_RSP   = 13,
   WLAN_HAL_DELETE_STA_REQ   = 14,
   WLAN_HAL_DELETE_STA_RSP   = 15,
   WLAN_HAL_CONFIG_BSS_REQ   = 16,
   WLAN_HAL_CONFIG_BSS_RSP   = 17,
   WLAN_HAL_DELETE_BSS_REQ   = 18,
   WLAN_HAL_DELETE_BSS_RSP   = 19,

   //Infra STA asscoiation
   WLAN_HAL_JOIN_REQ         = 20,
   WLAN_HAL_JOIN_RSP         = 21,
   WLAN_HAL_POST_ASSOC_REQ   = 22,
   WLAN_HAL_POST_ASSOC_RSP   = 23,

   //Security
   WLAN_HAL_SET_BSSKEY_REQ   = 24,
   WLAN_HAL_SET_BSSKEY_RSP   = 25,
   WLAN_HAL_SET_STAKEY_REQ   = 26,
   WLAN_HAL_SET_STAKEY_RSP   = 27,
   WLAN_HAL_RMV_BSSKEY_REQ   = 28,
   WLAN_HAL_RMV_BSSKEY_RSP   = 29,
   WLAN_HAL_RMV_STAKEY_REQ   = 30,
   WLAN_HAL_RMV_STAKEY_RSP   = 31,

   //Qos Related
   WLAN_HAL_ADD_TS_REQ          = 32,
   WLAN_HAL_ADD_TS_RSP          = 33,
   WLAN_HAL_DEL_TS_REQ          = 34,
   WLAN_HAL_DEL_TS_RSP          = 35,
   WLAN_HAL_UPD_EDCA_PARAMS_REQ = 36,
   WLAN_HAL_UPD_EDCA_PARAMS_RSP = 37,
   WLAN_HAL_ADD_BA_REQ          = 38,
   WLAN_HAL_ADD_BA_RSP          = 39,
   WLAN_HAL_DEL_BA_REQ          = 40,
   WLAN_HAL_DEL_BA_RSP          = 41,

   WLAN_HAL_CH_SWITCH_REQ       = 42,
   WLAN_HAL_CH_SWITCH_RSP       = 43,
   WLAN_HAL_SET_LINK_ST_REQ     = 44,
   WLAN_HAL_SET_LINK_ST_RSP     = 45,
   WLAN_HAL_GET_STATS_REQ       = 46,
   WLAN_HAL_GET_STATS_RSP       = 47,
   WLAN_HAL_UPDATE_CFG_REQ      = 48,
   WLAN_HAL_UPDATE_CFG_RSP      = 49,

   WLAN_HAL_MISSED_BEACON_IND           = 50,
   WLAN_HAL_UNKNOWN_ADDR2_FRAME_RX_IND  = 51,
   WLAN_HAL_MIC_FAILURE_IND             = 52,
   WLAN_HAL_FATAL_ERROR_IND             = 53,
   WLAN_HAL_SET_KEYDONE_MSG             = 54,
   
   //NV Interface
   WLAN_HAL_DOWNLOAD_NV_REQ             = 55,
   WLAN_HAL_DOWNLOAD_NV_RSP             = 56,

   WLAN_HAL_ADD_BA_SESSION_REQ          = 57,
   WLAN_HAL_ADD_BA_SESSION_RSP          = 58,
   WLAN_HAL_TRIGGER_BA_REQ              = 59,
   WLAN_HAL_TRIGGER_BA_RSP              = 60,
   WLAN_HAL_UPDATE_BEACON_REQ           = 61,
   WLAN_HAL_UPDATE_BEACON_RSP           = 62,
   WLAN_HAL_SEND_BEACON_REQ             = 63,
   WLAN_HAL_SEND_BEACON_RSP             = 64,

   WLAN_HAL_SET_BCASTKEY_REQ               = 65,
   WLAN_HAL_SET_BCASTKEY_RSP               = 66,
   WLAN_HAL_DELETE_STA_CONTEXT_IND         = 67,
   WLAN_HAL_UPDATE_PROBE_RSP_TEMPLATE_REQ  = 68,
   WLAN_HAL_UPDATE_PROBE_RSP_TEMPLATE_RSP  = 69,
   
  // PTT interface support
   WLAN_HAL_PROCESS_PTT_REQ   = 70,
   WLAN_HAL_PROCESS_PTT_RSP   = 71,
   
   // BTAMP related events
   WLAN_HAL_SIGNAL_BTAMP_EVENT_REQ  = 72,
   WLAN_HAL_SIGNAL_BTAMP_EVENT_RSP  = 73,
   WLAN_HAL_TL_HAL_FLUSH_AC_REQ     = 74,
   WLAN_HAL_TL_HAL_FLUSH_AC_RSP     = 75,

   WLAN_HAL_ENTER_IMPS_REQ           = 76,
   WLAN_HAL_EXIT_IMPS_REQ            = 77,
   WLAN_HAL_ENTER_BMPS_REQ           = 78,
   WLAN_HAL_EXIT_BMPS_REQ            = 79,
   WLAN_HAL_ENTER_UAPSD_REQ          = 80,
   WLAN_HAL_EXIT_UAPSD_REQ           = 81,
   WLAN_HAL_UPDATE_UAPSD_PARAM_REQ   = 82,
   WLAN_HAL_CONFIGURE_RXP_FILTER_REQ = 83,
   WLAN_HAL_ADD_BCN_FILTER_REQ       = 84,
   WLAN_HAL_REM_BCN_FILTER_REQ       = 85,
   WLAN_HAL_ADD_WOWL_BCAST_PTRN      = 86,
   WLAN_HAL_DEL_WOWL_BCAST_PTRN      = 87,
   WLAN_HAL_ENTER_WOWL_REQ           = 88,
   WLAN_HAL_EXIT_WOWL_REQ            = 89,
   WLAN_HAL_HOST_OFFLOAD_REQ         = 90,
   WLAN_HAL_SET_RSSI_THRESH_REQ      = 91,
   WLAN_HAL_GET_RSSI_REQ             = 92,
   WLAN_HAL_SET_UAPSD_AC_PARAMS_REQ  = 93,
   WLAN_HAL_CONFIGURE_APPS_CPU_WAKEUP_STATE_REQ = 94,

   WLAN_HAL_ENTER_IMPS_RSP           = 95,
   WLAN_HAL_EXIT_IMPS_RSP            = 96,
   WLAN_HAL_ENTER_BMPS_RSP           = 97,
   WLAN_HAL_EXIT_BMPS_RSP            = 98,
   WLAN_HAL_ENTER_UAPSD_RSP          = 99,
   WLAN_HAL_EXIT_UAPSD_RSP           = 100,
   WLAN_HAL_SET_UAPSD_AC_PARAMS_RSP  = 101,
   WLAN_HAL_UPDATE_UAPSD_PARAM_RSP   = 102,
   WLAN_HAL_CONFIGURE_RXP_FILTER_RSP = 103,
   WLAN_HAL_ADD_BCN_FILTER_RSP       = 104,
   WLAN_HAL_REM_BCN_FILTER_RSP       = 105,
   WLAN_HAL_SET_RSSI_THRESH_RSP      = 106,
   WLAN_HAL_HOST_OFFLOAD_RSP         = 107,
   WLAN_HAL_ADD_WOWL_BCAST_PTRN_RSP  = 108,
   WLAN_HAL_DEL_WOWL_BCAST_PTRN_RSP  = 109,
   WLAN_HAL_ENTER_WOWL_RSP           = 110,
   WLAN_HAL_EXIT_WOWL_RSP            = 111,
   WLAN_HAL_RSSI_NOTIFICATION_IND    = 112,
   WLAN_HAL_GET_RSSI_RSP             = 113,
   WLAN_HAL_CONFIGURE_APPS_CPU_WAKEUP_STATE_RSP = 114,

   //11k related events
   WLAN_HAL_SET_MAX_TX_POWER_REQ   = 115,
   WLAN_HAL_SET_MAX_TX_POWER_RSP   = 116,

   //11R related msgs
   WLAN_HAL_AGGR_ADD_TS_REQ        = 117,
   WLAN_HAL_AGGR_ADD_TS_RSP        = 118,

   //P2P  WLAN_FEATURE_P2P
   WLAN_HAL_SET_P2P_GONOA_REQ      = 119,
   WLAN_HAL_SET_P2P_GONOA_RSP      = 120,
   
   //WLAN Dump commands
   WLAN_HAL_DUMP_COMMAND_REQ       = 121,
   WLAN_HAL_DUMP_COMMAND_RSP       = 122,

   //INNAV FEATURE SUPPORT
   WLAN_HAL_START_INNAV_MEAS_REQ   = 123,
   WLAN_HAL_START_INNAV_MEAS_RSP   = 124,

   //ADD SELF STA REQ and RSP
   WLAN_HAL_ADD_STA_SELF_REQ       = 125,
   WLAN_HAL_ADD_STA_SELF_RSP       = 126,

   //DEL SELF STA SUPPORT
   WLAN_HAL_DEL_STA_SELF_REQ       = 127,
   WLAN_HAL_DEL_STA_SELF_RSP       = 128,

   // Coex Indication
   WLAN_HAL_COEX_IND               = 129,

   // Tx Complete Indication 
   WLAN_HAL_OTA_TX_COMPL_IND       = 130,

   //Host Suspend/resume messages
   WLAN_HAL_HOST_SUSPEND_IND       = 131,
   WLAN_HAL_HOST_RESUME_REQ        = 132,
   WLAN_HAL_HOST_RESUME_RSP        = 133,

   WLAN_HAL_SET_TX_POWER_REQ       = 134,
   WLAN_HAL_SET_TX_POWER_RSP       = 135,
   WLAN_HAL_GET_TX_POWER_REQ       = 136,
   WLAN_HAL_GET_TX_POWER_RSP       = 137,

   WLAN_HAL_P2P_NOA_ATTR_IND       = 138,
   
   WLAN_HAL_ENABLE_RADAR_DETECT_REQ  = 139,
   WLAN_HAL_ENABLE_RADAR_DETECT_RSP  = 140,
   WLAN_HAL_GET_TPC_REPORT_REQ       = 141,
   WLAN_HAL_GET_TPC_REPORT_RSP       = 142,
   WLAN_HAL_RADAR_DETECT_IND         = 143,
   WLAN_HAL_RADAR_DETECT_INTR_IND    = 144,
   WLAN_HAL_KEEP_ALIVE_REQ           = 145,
   WLAN_HAL_KEEP_ALIVE_RSP           = 146,      
   

   /*PNO messages*/
   WLAN_HAL_SET_PREF_NETWORK_REQ     = 147,
   WLAN_HAL_SET_PREF_NETWORK_RSP     = 148,
   WLAN_HAL_SET_RSSI_FILTER_REQ      = 149,
   WLAN_HAL_SET_RSSI_FILTER_RSP      = 150,
   WLAN_HAL_UPDATE_SCAN_PARAM_REQ    = 151,
   WLAN_HAL_UPDATE_SCAN_PARAM_RSP    = 152,
   WLAN_HAL_PREF_NETW_FOUND_IND      = 153, 

   WLAN_HAL_SET_TX_PER_TRACKING_REQ  = 154,
   WLAN_HAL_SET_TX_PER_TRACKING_RSP  = 155,
   WLAN_HAL_TX_PER_HIT_IND           = 156,
   

   WLAN_HAL_8023_MULTICAST_LIST_REQ   = 157,
   WLAN_HAL_8023_MULTICAST_LIST_RSP   = 158,   

   WLAN_HAL_SET_PACKET_FILTER_REQ     = 159,
   WLAN_HAL_SET_PACKET_FILTER_RSP     = 160,   
   WLAN_HAL_PACKET_FILTER_MATCH_COUNT_REQ   = 161,
   WLAN_HAL_PACKET_FILTER_MATCH_COUNT_RSP   = 162,   
   WLAN_HAL_CLEAR_PACKET_FILTER_REQ         = 163,
   WLAN_HAL_CLEAR_PACKET_FILTER_RSP         = 164,  
   /*This is temp fix. Should be removed once 
    * Host and Riva code is in sync*/
   WLAN_HAL_INIT_SCAN_CON_REQ               = 165,
    
   WLAN_HAL_SET_POWER_PARAMS_REQ            = 166,
   WLAN_HAL_SET_POWER_PARAMS_RSP            = 167,
    
   WLAN_HAL_TSM_STATS_REQ                   = 168,
   WLAN_HAL_TSM_STATS_RSP                   = 169,

   WLAN_HAL_MSG_MAX = WLAN_HAL_MAX_ENUM_SIZE
}tHalHostMsgType;

/* Enumeration for Boolean - False/True, On/Off */
typedef enum tagAniBoolean 
{
    eANI_BOOLEAN_FALSE = 0,
    eANI_BOOLEAN_TRUE,
    eANI_BOOLEAN_OFF = 0,
    eANI_BOOLEAN_ON = 1,
    eANI_BOOLEAN_MAX_FIELD = 0x7FFFFFFF  /* define as 4 bytes data */
} eAniBoolean;

typedef enum
{
   eDRIVER_TYPE_PRODUCTION  = 0,
   eDRIVER_TYPE_MFG         = 1,
   eDRIVER_TYPE_DVT         = 2,
   eDRIVER_TYPE_MAX         = WLAN_HAL_MAX_ENUM_SIZE
} tDriverType;

typedef enum
{
   HAL_STOP_TYPE_SYS_RESET,
   HAL_STOP_TYPE_SYS_DEEP_SLEEP,
   HAL_STOP_TYPE_RF_KILL,
   HAL_STOP_TYPE_MAX = WLAN_HAL_MAX_ENUM_SIZE
}tHalStopType;

typedef enum
{
   eHAL_SYS_MODE_NORMAL,
   eHAL_SYS_MODE_LEARN,
   eHAL_SYS_MODE_SCAN,
   eHAL_SYS_MODE_PROMISC,
   eHAL_SYS_MODE_MAX = WLAN_HAL_MAX_ENUM_SIZE
} eHalSysMode;

typedef enum
{
    PHY_SINGLE_CHANNEL_CENTERED = 0,     // 20MHz IF bandwidth centered on IF carrier
    PHY_DOUBLE_CHANNEL_LOW_PRIMARY = 1,  // 40MHz IF bandwidth with lower 20MHz supporting the primary channel
    PHY_DOUBLE_CHANNEL_CENTERED = 2,     // 40MHz IF bandwidth centered on IF carrier
    PHY_DOUBLE_CHANNEL_HIGH_PRIMARY = 3, // 40MHz IF bandwidth with higher 20MHz supporting the primary channel
    PHY_CHANNEL_BONDING_STATE_MAX = WLAN_HAL_MAX_ENUM_SIZE
}ePhyChanBondState;

// Spatial Multiplexing(SM) Power Save mode
typedef enum eSirMacHTMIMOPowerSaveState
{
  eSIR_HT_MIMO_PS_STATIC = 0,    // Static SM Power Save mode
  eSIR_HT_MIMO_PS_DYNAMIC = 1,   // Dynamic SM Power Save mode
  eSIR_HT_MIMO_PS_NA = 2,        // reserved
  eSIR_HT_MIMO_PS_NO_LIMIT = 3,  // SM Power Save disabled
  eSIR_HT_MIMO_PS_MAX = WLAN_HAL_MAX_ENUM_SIZE
} tSirMacHTMIMOPowerSaveState;

/* each station added has a rate mode which specifies the sta attributes */
typedef enum eStaRateMode {
    eSTA_TAURUS = 0,
    eSTA_TITAN,
    eSTA_POLARIS,
    eSTA_11b,
    eSTA_11bg,
    eSTA_11a,
    eSTA_11n,
    eSTA_INVALID_RATE_MODE = WLAN_HAL_MAX_ENUM_SIZE
} tStaRateMode, *tpStaRateMode;

#define SIR_NUM_11B_RATES           4  //1,2,5.5,11
#define SIR_NUM_11A_RATES           8  //6,9,12,18,24,36,48,54
#define SIR_NUM_POLARIS_RATES       3  //72,96,108

#define SIR_MAC_MAX_SUPPORTED_MCS_SET    16


typedef enum eSirBssType
{
    eSIR_INFRASTRUCTURE_MODE,
    eSIR_INFRA_AP_MODE,                    //Added for softAP support
    eSIR_IBSS_MODE,
    eSIR_BTAMP_STA_MODE,                   //Added for BT-AMP support
    eSIR_BTAMP_AP_MODE,                    //Added for BT-AMP support
    eSIR_AUTO_MODE,
    eSIR_DONOT_USE_BSS_TYPE = WLAN_HAL_MAX_ENUM_SIZE
} tSirBssType;

typedef enum eSirNwType
{
    eSIR_11A_NW_TYPE,
    eSIR_11B_NW_TYPE,
    eSIR_11G_NW_TYPE,
    eSIR_11N_NW_TYPE,
    eSIR_DONOT_USE_NW_TYPE = WLAN_HAL_MAX_ENUM_SIZE
} tSirNwType;

typedef tANI_U16 tSirMacBeaconInterval;

#define SIR_MAC_RATESET_EID_MAX            12

typedef enum eSirMacHTOperatingMode
{
  eSIR_HT_OP_MODE_PURE,                // No Protection
  eSIR_HT_OP_MODE_OVERLAP_LEGACY,      // Overlap Legacy device present, protection is optional
  eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT,  // No legacy device, but 20 MHz HT present
  eSIR_HT_OP_MODE_MIXED,               // Protection is required
  eSIR_HT_OP_MODE_MAX = WLAN_HAL_MAX_ENUM_SIZE
} tSirMacHTOperatingMode;

typedef enum eSirMacHTSecondaryChannelOffset
{
    eHT_SECONDARY_CHANNEL_OFFSET_NONE = 0,
    eHT_SECONDARY_CHANNEL_OFFSET_UP = 1,
    eHT_SECONDARY_CHANNEL_OFFSET_DOWN = 3,
    eHT_SECONDARY_CHANNEL_OFFSET_MAX = WLAN_HAL_MAX_ENUM_SIZE
} tSirMacHTSecondaryChannelOffset;

/// Encryption type enum used with peer
typedef enum eAniEdType
{
    eSIR_ED_NONE,
    eSIR_ED_WEP40,
    eSIR_ED_WEP104,
    eSIR_ED_TKIP,
    eSIR_ED_CCMP,
    eSIR_ED_WPI,
    eSIR_ED_AES_128_CMAC,
    eSIR_ED_NOT_IMPLEMENTED = WLAN_HAL_MAX_ENUM_SIZE
} tAniEdType;

#define WLAN_MAX_KEY_RSC_LEN                16
#define WLAN_WAPI_KEY_RSC_LEN               16

/// MAX key length when ULA is used
#define SIR_MAC_MAX_KEY_LENGTH              32
#define SIR_MAC_MAX_NUM_OF_DEFAULT_KEYS     4

/// Enum to specify whether key is used
/// for TX only, RX only or both
typedef enum eAniKeyDirection
{
    eSIR_TX_ONLY,
    eSIR_RX_ONLY,
    eSIR_TX_RX,
#ifdef WLAN_SOFTAP_FEATURE
    eSIR_TX_DEFAULT,
#endif
    eSIR_DONOT_USE_KEY_DIRECTION = WLAN_HAL_MAX_ENUM_SIZE
} tAniKeyDirection;

typedef enum eAniWepType
{
    eSIR_WEP_STATIC,
    eSIR_WEP_DYNAMIC,
    eSIR_WEP_MAX = WLAN_HAL_MAX_ENUM_SIZE
} tAniWepType;

typedef enum eSriLinkState {

    eSIR_LINK_IDLE_STATE        = 0,
    eSIR_LINK_PREASSOC_STATE    = 1,
    eSIR_LINK_POSTASSOC_STATE   = 2,
    eSIR_LINK_AP_STATE          = 3,
    eSIR_LINK_IBSS_STATE        = 4,

    /* BT-AMP Case */
    eSIR_LINK_BTAMP_PREASSOC_STATE  = 5,
    eSIR_LINK_BTAMP_POSTASSOC_STATE  = 6,
    eSIR_LINK_BTAMP_AP_STATE  = 7,
    eSIR_LINK_BTAMP_STA_STATE  = 8,
    
    /* Reserved for HAL Internal Use */
    eSIR_LINK_LEARN_STATE       = 9,
    eSIR_LINK_SCAN_STATE        = 10,
    eSIR_LINK_FINISH_SCAN_STATE = 11,
    eSIR_LINK_INIT_CAL_STATE    = 12,
    eSIR_LINK_FINISH_CAL_STATE  = 13,
#ifdef WLAN_FEATURE_P2P
    eSIR_LINK_LISTEN_STATE      = 14,
#endif
    eSIR_LINK_MAX = WLAN_HAL_MAX_ENUM_SIZE
} tSirLinkState;

typedef enum
{
    HAL_SUMMARY_STATS_INFO           = 0x00000001,
    HAL_GLOBAL_CLASS_A_STATS_INFO    = 0x00000002,
    HAL_GLOBAL_CLASS_B_STATS_INFO    = 0x00000004,
    HAL_GLOBAL_CLASS_C_STATS_INFO    = 0x00000008,
    HAL_GLOBAL_CLASS_D_STATS_INFO    = 0x00000010,
    HAL_PER_STA_STATS_INFO           = 0x00000020
}eHalStatsMask;

/* BT-AMP events type */
typedef enum 
{
    BTAMP_EVENT_CONNECTION_START,
    BTAMP_EVENT_CONNECTION_STOP,
    BTAMP_EVENT_CONNECTION_TERMINATED,
    BTAMP_EVENT_TYPE_MAX = WLAN_HAL_MAX_ENUM_SIZE, //This and beyond are invalid values
} tBtAmpEventType;

//***************************************************************


/*******************PE Statistics*************************/
typedef enum
{
    PE_SUMMARY_STATS_INFO           = 0x00000001,
    PE_GLOBAL_CLASS_A_STATS_INFO    = 0x00000002,
    PE_GLOBAL_CLASS_B_STATS_INFO    = 0x00000004,
    PE_GLOBAL_CLASS_C_STATS_INFO    = 0x00000008,
    PE_GLOBAL_CLASS_D_STATS_INFO    = 0x00000010,
    PE_PER_STA_STATS_INFO           = 0x00000020,
    PE_STATS_TYPE_MAX = WLAN_HAL_MAX_ENUM_SIZE //This and beyond are invalid values
}ePEStatsMask;

/*---------------------------------------------------------------------------
  Message definitons - All the messages below need to be packed
 ---------------------------------------------------------------------------*/

#if defined(__ANI_COMPILER_PRAGMA_PACK_STACK)
#pragma pack(push, 1)
#elif defined(__ANI_COMPILER_PRAGMA_PACK)
#pragma pack(1)
#else
#endif

/// Definition for HAL API Version.
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8                  revision;
    tANI_U8                  version;
    tANI_U8                  minor;
    tANI_U8                  major;
} tWcnssWlanVersion, *tpWcnssWlanVersion;

/// Definition for Encryption Keys
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8                  keyId;
    tANI_U8                  unicast;  // 0 for multicast
    tAniKeyDirection         keyDirection;
    tANI_U8                  keyRsc[WLAN_MAX_KEY_RSC_LEN];  // Usage is unknown
    tANI_U8                  paeRole;  // =1 for authenticator,=0 for supplicant
    tANI_U16                 keyLength;
    tANI_U8                  key[SIR_MAC_MAX_KEY_LENGTH];
} tSirKeys, *tpSirKeys;


//SetStaKeyParams Moving here since it is shared by configbss/setstakey msgs
typedef PACKED_PRE struct PACKED_POST
{
    /*STA Index*/
    tANI_U16        staIdx;

    /*Encryption Type used with peer*/
    tAniEdType      encType;

    /*STATIC/DYNAMIC - valid only for WEP*/
    tAniWepType     wepType; 

    /*Default WEP key, valid only for static WEP, must between 0 and 3.*/
    tANI_U8         defWEPIdx;

#ifdef WLAN_SOFTAP_FEATURE
    /* valid only for non-static WEP encyrptions */
    tSirKeys        key[SIR_MAC_MAX_NUM_OF_DEFAULT_KEYS];            
#else
    tSirKeys        key;
#endif
  
    /*Control for Replay Count, 1= Single TID based replay count on Tx
      0 = Per TID based replay count on TX */
    tANI_U8         singleTidRc;

} tSetStaKeyParams, *tpSetStaKeyParams;



/* 4-byte control message header used by HAL*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalHostMsgType  msgType;
   tANI_U32         msgLen;
} tHalMsgHeader, *tpHalMsgHeader;

/* Config format required by HAL for each CFG item*/
typedef PACKED_PRE struct PACKED_POST
{
   /* Cfg Id. The Id required by HAL is exported by HAL
    * in shared header file between UMAC and HAL.*/
   tANI_U16   uCfgId;

   /* Length of the Cfg. This parameter is used to go to next cfg 
    * in the TLV format.*/
   tANI_U16   uCfgLen;

   /* Padding bytes for unaligned address's */
   tANI_U16   uCfgPadBytes;

   /* Reserve bytes for making cfgVal to align address */
   tANI_U16   uCfgReserve;

   /* Following the uCfgLen field there should be a 'uCfgLen' bytes
    * containing the uCfgValue ; tANI_U8 uCfgValue[uCfgLen] */
} tHalCfg, *tpHalCfg;

/*---------------------------------------------------------------------------
  WLAN_HAL_START_REQ
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST sHalMacStartParameters
{
    /* Drive Type - Production or FTM etc */
    tDriverType  driverType;

    /*Length of the config buffer*/
    tANI_U32  uConfigBufferLen;

    /* Following this there is a TLV formatted buffer of length 
     * "uConfigBufferLen" bytes containing all config values. 
     * The TLV is expected to be formatted like this:
     * 0           15            31           31+CFG_LEN-1        length-1
     * |   CFG_ID   |   CFG_LEN   |   CFG_BODY    |  CFG_ID  |......|
     */
} tHalMacStartParameters, *tpHalMacStartParameters;

typedef PACKED_PRE struct PACKED_POST
{
   /* Note: The length specified in tHalMacStartReqMsg messages should be
    * header.msgLen = sizeof(tHalMacStartReqMsg) + uConfigBufferLen */
   tHalMsgHeader header;
   tHalMacStartParameters startReqParams;
}  tHalMacStartReqMsg, *tpHalMacStartReqMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_START_RSP
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST sHalMacStartRspParameters
{
   /*success or failure */
   tANI_U16  status;

   /*Max number of STA supported by the device*/
   tANI_U8     ucMaxStations;

   /*Max number of BSS supported by the device*/
   tANI_U8     ucMaxBssids;

   /*API Version */
   tWcnssWlanVersion wcnssWlanVersion;

   /*CRM build information */
   tANI_U8     wcnssCrmVersionString[WLAN_HAL_VERSION_LENGTH];

   /*hardware/chipset/misc version information */
   tANI_U8     wcnssWlanVersionString[WLAN_HAL_VERSION_LENGTH];

} tHalMacStartRspParams, *tpHalMacStartRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalMacStartRspParams startRspParams;
}  tHalMacStartRspMsg, *tpHalMacStartRspMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_STOP_REQ
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
  /*The reason for which the device is being stopped*/
  tHalStopType   reason;

}tHalMacStopReqParams, *tpHalMacStopReqParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalMacStopReqParams stopReqParams;
}  tHalMacStopReqMsg, *tpHalMacStopReqMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_STOP_RSP
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
  /*success or failure */
  tANI_U32   status;

}tHalMacStopRspParams, *tpHalMacStopRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalMacStopRspParams stopRspParams;
}  tHalMacStopRspMsg, *tpHalMacStopRspMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_UPDATE_CFG_REQ
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* Length of the config buffer. Allows UMAC to update multiple CFGs */
    tANI_U32  uConfigBufferLen;

    /* Following this there is a TLV formatted buffer of length 
     * "uConfigBufferLen" bytes containing all config values. 
     * The TLV is expected to be formatted like this:
     * 0           15            31           31+CFG_LEN-1        length-1
     * |   CFG_ID   |   CFG_LEN   |   CFG_BODY    |  CFG_ID  |......|
     */
} tHalUpdateCfgReqParams, *tpHalUpdateCfgReqParams;

typedef PACKED_PRE struct PACKED_POST
{
   /* Note: The length specified in tHalUpdateCfgReqMsg messages should be
    * header.msgLen = sizeof(tHalUpdateCfgReqMsg) + uConfigBufferLen */
   tHalMsgHeader header;
   tHalUpdateCfgReqParams updateCfgReqParams;
}  tHalUpdateCfgReqMsg, *tpHalUpdateCfgReqMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_UPDATE_CFG_RSP
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
  /* success or failure */
  tANI_U32   status;

}tHalUpdateCfgRspParams, *tpHalUpdateCfgRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalUpdateCfgRspParams updateCfgRspParams;
}  tHalUpdateCfgRspMsg, *tpHalUpdateCfgRspMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_INIT_SCAN_REQ
---------------------------------------------------------------------------*/

/// Frame control field format (2 bytes)
typedef  __ani_attr_pre_packed struct sSirMacFrameCtl
{

#ifndef ANI_LITTLE_BIT_ENDIAN

    tANI_U8 subType :4;
    tANI_U8 type :2;
    tANI_U8 protVer :2;

    tANI_U8 order :1;
    tANI_U8 wep :1;
    tANI_U8 moreData :1;
    tANI_U8 powerMgmt :1;
    tANI_U8 retry :1;
    tANI_U8 moreFrag :1;
    tANI_U8 fromDS :1;
    tANI_U8 toDS :1;

#else

    tANI_U8 protVer :2;
    tANI_U8 type :2;
    tANI_U8 subType :4;

    tANI_U8 toDS :1;
    tANI_U8 fromDS :1;
    tANI_U8 moreFrag :1;
    tANI_U8 retry :1;
    tANI_U8 powerMgmt :1;
    tANI_U8 moreData :1;
    tANI_U8 wep :1;
    tANI_U8 order :1;

#endif

} __ani_attr_packed  tSirMacFrameCtl, *tpSirMacFrameCtl;

/// Sequence control field
typedef __ani_attr_pre_packed struct sSirMacSeqCtl
{
    tANI_U8 fragNum : 4;
    tANI_U8 seqNumLo : 4;
    tANI_U8 seqNumHi : 8;
} __ani_attr_packed tSirMacSeqCtl, *tpSirMacSeqCtl;

/// Management header format
typedef __ani_attr_pre_packed struct sSirMacMgmtHdr
{
    tSirMacFrameCtl fc;
    tANI_U8           durationLo;
    tANI_U8           durationHi;
    tANI_U8              da[6];
    tANI_U8              sa[6];
    tANI_U8              bssId[6];
    tSirMacSeqCtl   seqControl;
} __ani_attr_packed tSirMacMgmtHdr, *tpSirMacMgmtHdr;

/// Scan Entry to hold active BSS idx's
typedef __ani_attr_pre_packed struct sSirScanEntry
{
    tANI_U8 bssIdx[HAL_NUM_BSSID];
    tANI_U8 activeBSScnt;
}__ani_attr_packed tSirScanEntry, *ptSirScanEntry;

typedef PACKED_PRE struct PACKED_POST {

    /*LEARN - AP Role
      SCAN - STA Role*/
    eHalSysMode scanMode;

    /*BSSID of the BSS*/
    tSirMacAddr bssid;

    /*Whether BSS needs to be notified*/
    tANI_U8 notifyBss;

    /*Kind of frame to be used for notifying the BSS (Data Null, QoS Null, or
      CTS to Self). Must always be a valid frame type.*/
    tANI_U8 frameType;

    /*UMAC has the option of passing the MAC frame to be used for notifying
      the BSS. If non-zero, HAL will use the MAC frame buffer pointed to by
      macMgmtHdr. If zero, HAL will generate the appropriate MAC frame based on
      frameType.*/
    tANI_U8 frameLength;

    /* Following the framelength there is a MAC frame buffer if frameLength 
       is non-zero. */
    tSirMacMgmtHdr macMgmtHdr;

    /*Entry to hold number of active BSS idx's*/
    tSirScanEntry scanEntry;

} tInitScanParams, * tpInitScanParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tInitScanParams initScanParams;
}  tHalInitScanReqMsg, *tpHalInitScanReqMsg;

typedef PACKED_PRE struct PACKED_POST {

    /*LEARN - AP Role
      SCAN - STA Role*/
    eHalSysMode scanMode;

    /*BSSID of the BSS*/
    tSirMacAddr bssid;

    /*Whether BSS needs to be notified*/
    tANI_U8 notifyBss;

    /*Kind of frame to be used for notifying the BSS (Data Null, QoS Null, or
      CTS to Self). Must always be a valid frame type.*/
    tANI_U8 frameType;

    /*UMAC has the option of passing the MAC frame to be used for notifying
      the BSS. If non-zero, HAL will use the MAC frame buffer pointed to by
      macMgmtHdr. If zero, HAL will generate the appropriate MAC frame based on
      frameType.*/
    tANI_U8 frameLength;

    /* Following the framelength there is a MAC frame buffer if frameLength 
       is non-zero. */
    tSirMacMgmtHdr macMgmtHdr;

    /*Entry to hold number of active BSS idx's*/
    tSirScanEntry scanEntry;

    /* Single NoA usage in Scanning */
    tANI_U8 useNoA;

    /* Indicates the scan duration (in ms) */
    tANI_U16 scanDuration;

} tInitScanConParams, * tpInitScanConParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tInitScanConParams initScanParams;
}  tHalInitScanConReqMsg, *tpHalInitScanConReqMsg;


/*---------------------------------------------------------------------------
  WLAN_HAL_INIT_SCAN_RSP
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
  /*success or failure */
  tANI_U32   status;

}tHalInitScanRspParams, *tpHalInitScanRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalInitScanRspParams initScanRspParams;
}  tHalInitScanRspMsg, *tpHalInitScanRspMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_START_SCAN_REQ
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST 
{
   /*Indicates the channel to scan*/
   tANI_U8 scanChannel;

 } tStartScanParams, * tpStartScanParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tStartScanParams startScanParams;
}  tHalStartScanReqMsg, *tpHalStartScanReqMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_START_SCAN_RSP
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
  /*success or failure */
  tANI_U32   status;

#if defined WLAN_FEATURE_VOWIFI
    tANI_U32 startTSF[2];
    tPowerdBm txMgmtPower;
#endif

}tHalStartScanRspParams, *tpHalStartScanRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalStartScanRspParams startScanRspParams;
}  tHalStartScanRspMsg, *tpHalStartScanRspMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_END_SCAN_REQ
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   /*Indicates the channel to stop scanning.  Not used really. But retained
    for symmetry with "start Scan" message. It can also help in error
    check if needed.*/
    tANI_U8 scanChannel;

} tEndScanParams, *tpEndScanParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tEndScanParams endScanParams;
}  tHalEndScanReqMsg, *tpHalEndScanReqMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_END_SCAN_RSP
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
  /*success or failure */
  tANI_U32   status;

}tHalEndScanRspParams, *tpHalEndScanRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalEndScanRspParams endScanRspParams;
}  tHalEndScanRspMsg, *tpHalEndScanRspMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_FINISH_SCAN_REQ
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* Identifies the operational state of the AP/STA
     * LEARN - AP Role SCAN - STA Role */
    eHalSysMode scanMode;

    /*Operating channel to tune to.*/
    tANI_U8 currentOperChannel;

    /*Channel Bonding state If 20/40 MHz is operational, this will indicate the
      40 MHz extension channel in combination with the control channel*/
    ePhyChanBondState cbState;

    /*BSSID of the BSS*/
    tSirMacAddr bssid;

    /*Whether BSS needs to be notified*/
    tANI_U8 notifyBss;

    /*Kind of frame to be used for notifying the BSS (Data Null, QoS Null, or
     CTS to Self). Must always be a valid frame type.*/
    tANI_U8 frameType;

    /*UMAC has the option of passing the MAC frame to be used for notifying
      the BSS. If non-zero, HAL will use the MAC frame buffer pointed to by
      macMgmtHdr. If zero, HAL will generate the appropriate MAC frame based on
      frameType.*/
    tANI_U8 frameLength;
    
    /*Following the framelength there is a MAC frame buffer if frameLength 
      is non-zero.*/    
    tSirMacMgmtHdr macMgmtHdr;

    /*Entry to hold number of active BSS idx's*/
    tSirScanEntry scanEntry;

} tFinishScanParams, *tpFinishScanParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tFinishScanParams finishScanParams;
}  tHalFinishScanReqMsg, *tpHalFinishScanReqMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_FINISH_SCAN_RSP
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
  /*success or failure */
  tANI_U32   status;

}tHalFinishScanRspParams, *tpHalFinishScanRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalFinishScanRspParams finishScanRspParams;
}  tHalFinishScanRspMsg, *tpHalFinishScanRspMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_CONFIG_STA_REQ
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST {
    /*
    * For Self STA Entry: this represents Self Mode.
    * For Peer Stations, this represents the mode of the peer.
    * On Station:
    * --this mode is updated when PE adds the Self Entry.
    * -- OR when PE sends 'ADD_BSS' message and station context in BSS is used to indicate the mode of the AP.
    * ON AP:
    * -- this mode is updated when PE sends 'ADD_BSS' and Sta entry for that BSS is used
    *     to indicate the self mode of the AP.
    * -- OR when a station is associated, PE sends 'ADD_STA' message with this mode updated.
    */

    tStaRateMode        opRateMode;
    // 11b, 11a and aniLegacyRates are IE rates which gives rate in unit of 500Kbps
    tANI_U16             llbRates[SIR_NUM_11B_RATES];
    tANI_U16             llaRates[SIR_NUM_11A_RATES];
    tANI_U16             aniLegacyRates[SIR_NUM_POLARIS_RATES];
    tANI_U16             reserved;

    //Taurus only supports 26 Titan Rates(no ESF/concat Rates will be supported)
    //First 26 bits are reserved for those Titan rates and
    //the last 4 bits(bit28-31) for Taurus, 2(bit26-27) bits are reserved.
    tANI_U32             aniEnhancedRateBitmap; //Titan and Taurus Rates

    /*
    * 0-76 bits used, remaining reserved
    * bits 0-15 and 32 should be set.
    */
    tANI_U8 supportedMCSSet[SIR_MAC_MAX_SUPPORTED_MCS_SET];

    /*
     * RX Highest Supported Data Rate defines the highest data
     * rate that the STA is able to receive, in unites of 1Mbps.
     * This value is derived from "Supported MCS Set field" inside
     * the HT capability element.
     */
    tANI_U16 rxHighestDataRate;

} tSirSupportedRates, *tpSirSupportedRates;

typedef PACKED_PRE struct PACKED_POST
{
    /*BSSID of STA*/
    tSirMacAddr bssId;

    /*ASSOC ID, as assigned by UMAC*/
    tANI_U16 assocId;

    /* STA entry Type: 0 - Self, 1 - Other/Peer, 2 - BSSID, 3 - BCAST */
    tANI_U8 staType;

    /*Short Preamble Supported.*/
    tANI_U8 shortPreambleSupported;

    /*MAC Address of STA*/
    tSirMacAddr staMac;

    /*Listen interval of the STA*/
    tANI_U16 listenInterval;

    /*Support for 11e/WMM*/
    tANI_U8 wmmEnabled;

    /*11n HT capable STA*/
    tANI_U8 htCapable;

    /*TX Width Set: 0 - 20 MHz only, 1 - 20/40 MHz*/
    tANI_U8 txChannelWidthSet;

    /*RIFS mode 0 - NA, 1 - Allowed */
    tANI_U8 rifsMode;

    /*L-SIG TXOP Protection mechanism 
      0 - No Support, 1 - Supported
      SG - there is global field */
    tANI_U8 lsigTxopProtection;

    /*Max Ampdu Size supported by STA. TPE programming.
      0 : 8k , 1 : 16k, 2 : 32k, 3 : 64k */
    tANI_U8 maxAmpduSize;

    /*Max Ampdu density. Used by RA.  3 : 0~7 : 2^(11nAMPDUdensity -4)*/
    tANI_U8 maxAmpduDensity;

    /*Max AMSDU size 1 : 3839 bytes, 0 : 7935 bytes*/
    tANI_U8 maxAmsduSize;

    /*Short GI support for 40Mhz packets*/
    tANI_U8 fShortGI40Mhz;

    /*Short GI support for 20Mhz packets*/
    tANI_U8 fShortGI20Mhz;

    /*These rates are the intersection of peer and self capabilities.*/
    tSirSupportedRates supportedRates;

    /*Robust Management Frame (RMF) enabled/disabled*/
    tANI_U8 rmfEnabled;

    /* The unicast encryption type in the association */
    tANI_U32 encryptType;
    
    /*HAL should update the existing STA entry, if this flag is set. UMAC 
      will set this flag in case of RE-ASSOC, where we want to reuse the old
      STA ID. 0 = Add, 1 = Update*/
    tANI_U8 action;

    /*U-APSD Flags: 1b per AC.  Encoded as follows:
       b7 b6 b5 b4 b3 b2 b1 b0 =
       X  X  X  X  BE BK VI VO */
    tANI_U8 uAPSD;

    /*Max SP Length*/
    tANI_U8 maxSPLen;

    /*11n Green Field preamble support
      0 - Not supported, 1 - Supported */
    tANI_U8 greenFieldCapable;

    /*MIMO Power Save mode*/
    tSirMacHTMIMOPowerSaveState mimoPS;

    /*Delayed BA Support*/
    tANI_U8 delayedBASupport;
    
    /*Max AMPDU duration in 32us*/
    tANI_U8 us32MaxAmpduDuration;
    
    /*HT STA should set it to 1 if it is enabled in BSS. HT STA should set
      it to 0 if AP does not support it. This indication is sent to HAL and
      HAL uses this flag to pickup up appropriate 40Mhz rates.*/
    tANI_U8 fDsssCckMode40Mhz;

    /* Valid STA Idx when action=Update. Set to 0xFF when invalid!
       Retained for backward compalibity with existing HAL code*/
    tANI_U8 staIdx;

    /* BSSID of BSS to which station is associated. Set to 0xFF when invalid.
       Retained for backward compalibity with existing HAL code*/
    tANI_U8 bssIdx;

    tANI_U8  p2pCapableSta;

} tConfigStaParams, *tpConfigStaParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tConfigStaParams configStaParams;
}  tConfigStaReqMsg, *tpConfigStaReqMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_CONFIG_STA_RSP
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
  /*success or failure */
  tANI_U32   status;

  /* Station index; valid only when 'status' field value SUCCESS */
  tANI_U8 staIdx;

  /* BSSID Index of BSS to which the station is associated */
  tANI_U8 bssIdx;

  /* DPU Index for PTK */
  tANI_U8 dpuIndex;

  /* DPU Index for GTK */  
  tANI_U8 bcastDpuIndex;

  /*DPU Index for IGTK  */
  tANI_U8 bcastMgmtDpuIdx;

  /*PTK DPU signature*/
  tANI_U8 ucUcastSig;

  /*GTK DPU isignature*/
  tANI_U8 ucBcastSig;

  /* IGTK DPU signature*/
  tANI_U8 ucMgmtSig;

  tANI_U8  p2pCapableSta;

}tConfigStaRspParams, *tpConfigStaRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tConfigStaRspParams configStaRspParams;
}tConfigStaRspMsg, *tpConfigStaRspMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_DELETE_STA_REQ
---------------------------------------------------------------------------*/

/* Delete STA Request params */
typedef PACKED_PRE struct PACKED_POST 
{
   /* Index of STA to delete */
   tANI_U8    staIdx;
} tDeleteStaParams, *tpDeleteStaParams;

/* Delete STA Request message*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tDeleteStaParams delStaParams;
}  tDeleteStaReqMsg, *tpDeleteStaReqMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_DELETE_STA_RSP
---------------------------------------------------------------------------*/

/* Delete STA Response Params */
typedef PACKED_PRE struct PACKED_POST 
{
   /*success or failure */
   tANI_U32   status;

   /* Index of STA deleted */
   tANI_U8    staId;
} tDeleteStaRspParams, *tpDeleteStaRspParams;

/* Delete STA Response message*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tDeleteStaRspParams delStaRspParams;
}  tDeleteStaRspMsg, *tpDeleteStaRspMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_CONFIG_BSS_REQ
---------------------------------------------------------------------------*/

//12 Bytes long because this structure can be used to represent rate
//and extended rate set IEs. The parser assume this to be at least 12
typedef __ani_attr_pre_packed struct sSirMacRateSet
{
    tANI_U8  numRates;
    tANI_U8  rate[SIR_MAC_RATESET_EID_MAX];
} __ani_attr_packed tSirMacRateSet;

// access category record
typedef __ani_attr_pre_packed struct sSirMacAciAifsn
{
#ifndef ANI_LITTLE_BIT_ENDIAN
    tANI_U8  rsvd  : 1;
    tANI_U8  aci   : 2;
    tANI_U8  acm   : 1;
    tANI_U8  aifsn : 4;
#else
    tANI_U8  aifsn : 4;
    tANI_U8  acm   : 1;
    tANI_U8  aci   : 2;
    tANI_U8  rsvd  : 1;
#endif
} __ani_attr_packed tSirMacAciAifsn;

// contention window size
typedef __ani_attr_pre_packed struct sSirMacCW
{
#ifndef ANI_LITTLE_BIT_ENDIAN
    tANI_U8  max : 4;
    tANI_U8  min : 4;
#else
    tANI_U8  min : 4;
    tANI_U8  max : 4;
#endif
} __ani_attr_packed tSirMacCW;

typedef __ani_attr_pre_packed struct sSirMacEdcaParamRecord
{
    tSirMacAciAifsn  aci;
    tSirMacCW        cw;
    tANI_U16         txoplimit;
} __ani_attr_packed tSirMacEdcaParamRecord;

typedef __ani_attr_pre_packed struct sSirMacSSid
{
    tANI_U8        length;
    tANI_U8        ssId[32];
} __ani_attr_packed tSirMacSSid;

// Concurrency role.  These are generic IDs that identify the various roles
// in the software system.
typedef enum {
    HAL_STA_MODE=0, 
    HAL_STA_SAP_MODE=1, // to support softAp mode . This is misleading. It means AP MODE only. 
    HAL_P2P_CLIENT_MODE,
    HAL_P2P_GO_MODE,
    HAL_MONITOR_MODE,
} tHalConMode;

//This is a bit pattern to be set for each mode
//bit 0 - sta mode
//bit 1 - ap mode
//bit 2 - p2p client mode
//bit 3 - p2p go mode
typedef enum
{
    HAL_STA=1, 
    HAL_SAP=2,
    HAL_STA_SAP=3, //to support sta, softAp  mode . This means STA+AP mode
    HAL_P2P_CLIENT=4,
    HAL_P2P_GO=8,
    HAL_MAX_CONCURRENCY_PERSONA=4
} tHalConcurrencyMode;

typedef PACKED_PRE struct PACKED_POST
{
    /* BSSID */
    tSirMacAddr bssId;

#ifdef HAL_SELF_STA_PER_BSS
    /* Self Mac Address */
    tSirMacAddr  selfMacAddr;
#endif

    /* BSS type */
    tSirBssType bssType;

    /*Operational Mode: AP =0, STA = 1*/
    tANI_U8 operMode;

    /*Network Type*/
    tSirNwType nwType;

    /*Used to classify PURE_11G/11G_MIXED to program MTU*/
    tANI_U8 shortSlotTimeSupported;

    /*Co-exist with 11a STA*/
    tANI_U8 llaCoexist;

    /*Co-exist with 11b STA*/
    tANI_U8 llbCoexist;

    /*Co-exist with 11g STA*/
    tANI_U8 llgCoexist;

    /*Coexistence with 11n STA*/
    tANI_U8 ht20Coexist;

    /*Non GF coexist flag*/
    tANI_U8 llnNonGFCoexist;

    /*TXOP protection support*/
    tANI_U8 fLsigTXOPProtectionFullSupport;

    /*RIFS mode*/
    tANI_U8 fRIFSMode;

    /*Beacon Interval in TU*/
    tSirMacBeaconInterval beaconInterval;

    /*DTIM period*/
    tANI_U8 dtimPeriod;

    /*TX Width Set: 0 - 20 MHz only, 1 - 20/40 MHz*/
    tANI_U8 txChannelWidthSet;

    /*Operating channel*/
    tANI_U8 currentOperChannel;

    /*Extension channel for channel bonding*/
    tANI_U8 currentExtChannel;

    /*Reserved to align next field on a dword boundary*/
    tANI_U8 reserved;

    /*Context of the station being added in HW
      Add a STA entry for "itself" -
      On AP  - Add the AP itself in an "STA context"
      On STA - Add the AP to which this STA is joining in an "STA context" */
    tConfigStaParams staContext;

    /*SSID of the BSS*/
    tSirMacSSid ssId;

    /*HAL should update the existing BSS entry, if this flag is set.
      UMAC will set this flag in case of reassoc, where we want to resue the
      the old BSSID and still return success 0 = Add, 1 = Update*/
    tANI_U8 action;

    /* MAC Rate Set */
    tSirMacRateSet rateSet;

    /*Enable/Disable HT capabilities of the BSS*/
    tANI_U8 htCapable;

    // Enable/Disable OBSS protection
    tANI_U8 obssProtEnabled;

    /*RMF enabled/disabled*/
    tANI_U8 rmfEnabled;

    /*HT Operating Mode operating mode of the 802.11n STA*/
    tSirMacHTOperatingMode htOperMode;

    /*Dual CTS Protection: 0 - Unused, 1 - Used*/
    tANI_U8 dualCTSProtection;

    /* Probe Response Max retries */
    tANI_U8   ucMaxProbeRespRetryLimit;

    /* To Enable Hidden ssid */
    tANI_U8   bHiddenSSIDEn;

    /* To Enable Disable FW Proxy Probe Resp */
    tANI_U8   bProxyProbeRespEn;

    /* Boolean to indicate if EDCA params are valid. UMAC might not have valid 
       EDCA params or might not desire to apply EDCA params during config BSS. 
       0 implies Not Valid ; Non-Zero implies valid*/
    tANI_U8   edcaParamsValid;

    /*EDCA Parameters for Best Effort Access Category*/
    tSirMacEdcaParamRecord acbe;
    
    /*EDCA Parameters forBackground Access Category*/
    tSirMacEdcaParamRecord acbk;

    /*EDCA Parameters for Video Access Category*/
    tSirMacEdcaParamRecord acvi;

    /*EDCA Parameters for Voice Access Category*/
    tSirMacEdcaParamRecord acvo;

#if defined WLAN_FEATURE_VOWIFI
    /*HAL fills in the tx power used for mgmt frames in txMgmtPower*/
    tANI_S8     txMgmtPower;
    /*maxTxPower has max power to be used after applying the power constraint if any */
    tANI_S8     maxTxPower;
#endif    

#ifdef WLAN_FEATURE_VOWIFI_11R
    tANI_U8 extSetStaKeyParamValid; //Ext Bss Config Msg if set
    tSetStaKeyParams extSetStaKeyParam;  //SetStaKeyParams for ext bss msg
#endif

    /* Persona for the BSS can be STA,AP,GO,CLIENT value same as tHalConMode */    
    tANI_U8   halPersona;
   
    tANI_U8 bSpectrumMgtEnable;
   
} tConfigBssParams, * tpConfigBssParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tConfigBssParams configBssParams;
}  tConfigBssReqMsg, *tpConfigBssReqMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_CONFIG_BSS_RSP
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* Success or Failure */
    tANI_U32   status;

    /* BSS index allocated by HAL */
    tANI_U8    bssIdx;

    /* DPU descriptor index for PTK */
    tANI_U8    dpuDescIndx;

    /* PTK DPU signature */
    tANI_U8    ucastDpuSignature;

    /* DPU descriptor index for GTK*/
    tANI_U8    bcastDpuDescIndx;

    /* GTK DPU signature */
    tANI_U8    bcastDpuSignature;

    /*DPU descriptor for IGTK*/
    tANI_U8    mgmtDpuDescIndx;

    /* IGTK DPU signature */
    tANI_U8    mgmtDpuSignature;

    /* Station Index for BSS entry*/
    tANI_U8     bssStaIdx;

    /* Self station index for this BSS */
    tANI_U8     bssSelfStaIdx;

    /* Bcast station for buffering bcast frames in AP role */
    tANI_U8     bssBcastStaIdx;

    /*MAC Address of STA(PEER/SELF) in staContext of configBSSReq*/
    tSirMacAddr   staMac;

#ifdef WLAN_FEATURE_VOWIFI
    /*HAL fills in the tx power used for mgmt frames in this field. */
    tANI_S8     txMgmtPower;
#endif

} tConfigBssRspParams, * tpConfigBssRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tConfigBssRspParams configBssRspParams;
}  tConfigBssRspMsg, *tpConfigBssRspMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_DELETE_BSS_REQ
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* BSS index to be deleted */
    tANI_U8 bssIdx;

} tDeleteBssParams, *tpDeleteBssParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tDeleteBssParams deleteBssParams;
}  tDeleteBssReqMsg, *tpDeleteBssReqMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_DELETE_BSS_RSP
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* Success or Failure */
    tANI_U32   status;

    /* BSS index that has been deleted */
    tANI_U8 bssIdx;

} tDeleteBssRspParams, *tpDeleteBssRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tDeleteBssRspParams deleteBssRspParams;
}  tDeleteBssRspMsg, *tpDeleteBssRspMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_JOIN_REQ
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
  /*Indicates the BSSID to which STA is going to associate*/
  tSirMacAddr     bssId; 

  /*Indicates the channel to switch to.*/
  tANI_U8         ucChannel;

  /* Self STA MAC */
  tSirMacAddr selfStaMacAddr;
     
#ifndef WLAN_FEATURE_VOWIFI   
  /*Local power constraint*/
  tANI_U8         ucLocalPowerConstraint;
#endif

  /*Secondary channel offset */
  tSirMacHTSecondaryChannelOffset  secondaryChannelOffset;

#ifdef WLAN_FEATURE_VOWIFI
  /* Max TX power */
  tANI_S8 maxTxPower;
#endif

  /*link State*/
  tSirLinkState   linkState;

} tHalJoinReqParams, *tpHalJoinReqParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalJoinReqParams joinReqParams;
}  tHalJoinReqMsg, *tpHalJoinReqMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_JOIN_RSP
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
  /*success or failure */
  tANI_U32   status;

#ifdef WLAN_FEATURE_VOWIFI
  /* HAL fills in the tx power used for mgmt frames in this field */
  tPowerdBm txMgmtPower;
#endif

}tHalJoinRspParams, *tpHalJoinRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalJoinRspParams joinRspParams;
}tHalJoinRspMsg, *tpHalJoinRspMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_POST_ASSOC_REQ
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tConfigStaParams configStaParams;
   tConfigBssParams configBssParams;
} tPostAssocReqParams, *tpPostAssocReqParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tPostAssocReqParams postAssocReqParams;
}  tPostAssocReqMsg, *tpPostAssocReqMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_POST_ASSOC_RSP
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tConfigStaRspParams configStaRspParams;
   tConfigBssRspParams configBssRspParams;
} tPostAssocRspParams, *tpPostAssocRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tPostAssocRspParams postAssocRspParams;
}  tPostAssocRspMsg, *tpPostAssocRspMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_SET_BSSKEY_REQ
---------------------------------------------------------------------------*/

/*
 * This is used by PE to create a set of WEP keys for a given BSS.
 */
typedef PACKED_PRE struct PACKED_POST
{
    /*BSS Index of the BSS*/
    tANI_U8         bssIdx;

    /*Encryption Type used with peer*/
    tAniEdType      encType;

    /*Number of keys*/
    tANI_U8         numKeys;

    /*Array of keys.*/
    tSirKeys        key[SIR_MAC_MAX_NUM_OF_DEFAULT_KEYS];
  
    /*Control for Replay Count, 1= Single TID based replay count on Tx
    0 = Per TID based replay count on TX */
    tANI_U8         singleTidRc;
} tSetBssKeyParams, *tpSetBssKeyParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tSetBssKeyParams setBssKeyParams;
} tSetBssKeyReqMsg, *tpSetBssKeyReqMsg;

/*---------------------------------------------------------------------------
  WLAN_HAL_SET_BSSKEY_RSP
---------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
  /*success or failure */
  tANI_U32   status;

} tSetBssKeyRspParams, *tpSetBssKeyRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tSetBssKeyRspParams setBssKeyRspParams;
}  tSetBssKeyRspMsg, *tpSetBssKeyRspMsg;

/*---------------------------------------------------------------------------
   WLAN_HAL_SET_STAKEY_REQ,
---------------------------------------------------------------------------*/

/*
 * This is used by PE to configure the key information on a given station.
 * When the secType is WEP40 or WEP104, the defWEPIdx is used to locate
 * a preconfigured key from a BSS the station assoicated with; otherwise
 * a new key descriptor is created based on the key field.
 */

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tSetStaKeyParams setStaKeyParams;
} tSetStaKeyReqMsg, *tpSetStaKeyReqMsg;

/*---------------------------------------------------------------------------
   WLAN_HAL_SET_STAKEY_RSP,
---------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
  /*success or failure */
  tANI_U32   status;

} tSetStaKeyRspParams, *tpSetStaKeyRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tSetStaKeyRspParams setStaKeyRspParams;
} tSetStaKeyRspMsg, *tpSetStaKeyRspMsg;

/*---------------------------------------------------------------------------
   WLAN_HAL_RMV_BSSKEY_REQ,
---------------------------------------------------------------------------*/
/*
 * This is used by PE to remove keys for a given BSS.
 */
typedef PACKED_PRE struct PACKED_POST

{
    /*BSS Index of the BSS*/
    tANI_U8         bssIdx;
    
    /*Encryption Type used with peer*/
    tAniEdType      encType;

    /*Key Id*/
    tANI_U8         keyId;

    /*STATIC/DYNAMIC. Used in Nullifying in Key Descriptors for Static/Dynamic keys*/
    tAniWepType    wepType;

} tRemoveBssKeyParams, *tpRemoveBssKeyParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tRemoveBssKeyParams removeBssKeyParams;
}  tRemoveBssKeyReqMsg, *tpRemoveBssKeyReqMsg;

/*---------------------------------------------------------------------------
   WLAN_HAL_RMV_BSSKEY_RSP,
---------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
  /*success or failure */
  tANI_U32   status;

} tRemoveBssKeyRspParams, *tpRemoveBssKeyRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tRemoveBssKeyRspParams removeBssKeyRspParams;
}  tRemoveBssKeyRspMsg, *tpRemoveBssKeyRspMsg;

/*---------------------------------------------------------------------------
   WLAN_HAL_RMV_STAKEY_REQ,
---------------------------------------------------------------------------*/
/*
 * This is used by PE to Remove the key information on a given station.
 */
typedef PACKED_PRE struct PACKED_POST
{
    /*STA Index*/
    tANI_U16         staIdx;

    /*Encryption Type used with peer*/
    tAniEdType      encType;

    /*Key Id*/
    tANI_U8           keyId;

    /*Whether to invalidate the Broadcast key or Unicast key. In case of WEP,
      the same key is used for both broadcast and unicast.*/
    tANI_BOOLEAN    unicast;

} tRemoveStaKeyParams, *tpRemoveStaKeyParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tRemoveStaKeyParams removeStaKeyParams;
}  tRemoveStaKeyReqMsg, *tpRemoveStaKeyReqMsg;

/*---------------------------------------------------------------------------
   WLAN_HAL_RMV_STAKEY_RSP,
---------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
  /*success or failure */
  tANI_U32   status;
} tRemoveStaKeyRspParams, *tpRemoveStaKeyRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tRemoveStaKeyRspParams removeStaKeyRspParams;
}  tRemoveStaKeyRspMsg, *tpRemoveStaKeyRspMsg;





/*---------------------------------------------------------------------------
WLAN_HAL_CH_SWITCH_REQ
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* Channel number */
    tANI_U8 channelNumber;

#ifndef WLAN_FEATURE_VOWIFI
    /* Local power constraint */
    tANI_U8 localPowerConstraint;
#endif

    /*Secondary channel offset */
    tSirMacHTSecondaryChannelOffset secondaryChannelOffset;

#ifdef WLAN_FEATURE_VOWIFI

    //HAL fills in the tx power used for mgmt frames in this field.
    tPowerdBm txMgmtPower;

    /* Max TX power */
    tPowerdBm maxTxPower;
    
    /* Self STA MAC */
    tSirMacAddr selfStaMacAddr;

    /*BSSID needed to identify session. As the request has power constraints,
      this should be applied only to that session*/
    tSirMacAddr bssId;
   
#endif
    
}tSwitchChannelParams, *tpSwitchChannelParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tSwitchChannelParams switchChannelParams;
}  tSwitchChannelReqMsg, *tpSwitchChannelReqMsg;

/*---------------------------------------------------------------------------
WLAN_HAL_CH_SWITCH_RSP
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* Status */
    tANI_U32 status;

    /* Channel number - same as in request*/
    tANI_U8 channelNumber;

#ifdef WLAN_FEATURE_VOWIFI
    /* HAL fills in the tx power used for mgmt frames in this field */
    tPowerdBm txMgmtPower;

    /* BSSID needed to identify session - same as in request*/
    tSirMacAddr bssId;
#endif
    
}tSwitchChannelRspParams, *tpSwitchChannelRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tSwitchChannelRspParams switchChannelRspParams;
}  tSwitchChannelRspMsg, *tpSwitchChannelRspMsg;

/*---------------------------------------------------------------------------
WLAN_HAL_UPD_EDCA_PARAMS_REQ
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   /*BSS Index*/
   tANI_U16 bssIdx;

   /* Best Effort */
   tSirMacEdcaParamRecord acbe; 

   /* Background */
   tSirMacEdcaParamRecord acbk;
   
   /* Video */
   tSirMacEdcaParamRecord acvi;

   /* Voice */
   tSirMacEdcaParamRecord acvo;

} tEdcaParams, *tpEdcaParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tEdcaParams edcaParams;
}  tUpdateEdcaParamsReqMsg, *tpUpdateEdcaParamsReqMsg;

/*---------------------------------------------------------------------------
WLAN_HAL_UPD_EDCA_PARAMS_RSP
---------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
  /*success or failure */
  tANI_U32   status;
} tEdcaRspParams, *tpEdcaRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tEdcaRspParams edcaRspParams;
}  tUpdateEdcaParamsRspMsg, *tpUpdateEdcaParamsRspMsg;



/*---------------------------------------------------------------------------
 * WLAN_HAL_GET_STATS_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST

{
    /* Index of STA to which the statistics */
    tANI_U16 staIdx;

    /* Encryption mode */
    tANI_U8 encMode;
    
    /* status */
    tANI_U32  status;
    
    /* Statistics */
    tANI_U32  sendBlocks;
    tANI_U32  recvBlocks;
    tANI_U32  replays;
    tANI_U8   micErrorCnt;
    tANI_U32  protExclCnt;
    tANI_U16  formatErrCnt;
    tANI_U16  unDecryptableCnt;
    tANI_U32  decryptErrCnt;
    tANI_U32  decryptOkCnt;
} tDpuStatsParams, * tpDpuStatsParams;

typedef PACKED_PRE struct PACKED_POST
{
   /* Valid STA Idx for per STA stats request */
   tANI_U32    staId;

   /* Categories of stats requested as specified in eHalStatsMask*/
   tANI_U32    statsMask;
}tHalStatsReqParams, *tpHalStatsReqParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader        header;
   tHalStatsReqParams   statsReqParams;
} tHalStatsReqMsg, *tpHalStatsReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_GET_STATS_RSP
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32 retry_cnt[4];          //Total number of packets(per AC) that were successfully transmitted with retries
    tANI_U32 multiple_retry_cnt[4]; //The number of MSDU packets and MMPDU frames per AC that the 802.11
                                    // station successfully transmitted after more than one retransmission attempt

    tANI_U32 tx_frm_cnt[4];         //Total number of packets(per AC) that were successfully transmitted 
                                    //(with and without retries, including multi-cast, broadcast)     
    tANI_U32 rx_frm_cnt;            //Total number of packets that were successfully received 
                                    //(after appropriate filter rules including multi-cast, broadcast)    
    tANI_U32 frm_dup_cnt;           //Total number of duplicate frames received successfully
    tANI_U32 fail_cnt[4];           //Total number packets(per AC) failed to transmit
    tANI_U32 rts_fail_cnt;          //Total number of RTS/CTS sequence failures for transmission of a packet
    tANI_U32 ack_fail_cnt;          //Total number packets failed transmit because of no ACK from the remote entity
    tANI_U32 rts_succ_cnt;          //Total number of RTS/CTS sequence success for transmission of a packet 
    tANI_U32 rx_discard_cnt;        //The sum of the receive error count and dropped-receive-buffer error count. 
                                    //HAL will provide this as a sum of (FCS error) + (Fail get BD/PDU in HW)
    tANI_U32 rx_error_cnt;          //The receive error count. HAL will provide the RxP FCS error global counter.
    tANI_U32 tx_byte_cnt;           //The sum of the transmit-directed byte count, transmit-multicast byte count 
                                    //and transmit-broadcast byte count. HAL will sum TPE UC/MC/BCAST global counters 
                                    //to provide this.
}tAniSummaryStatsInfo, *tpAniSummaryStatsInfo;


// defines tx_rate_flags
typedef enum eTxRateInfo
{
   eHAL_TX_RATE_LEGACY = 0x1,    /* Legacy rates */
   eHAL_TX_RATE_HT20   = 0x2,    /* HT20 rates */
   eHAL_TX_RATE_HT40   = 0x4,    /* HT40 rates */
   eHAL_TX_RATE_SGI    = 0x8,    /* Rate with Short guard interval */
   eHAL_TX_RATE_LGI    = 0x10    /* Rate with Long guard interval */
} tTxrateinfoflags;


typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32 rx_frag_cnt;              //The number of MPDU frames received by the 802.11 station for MSDU packets 
                                       //or MMPDU frames
    tANI_U32 promiscuous_rx_frag_cnt;  //The number of MPDU frames received by the 802.11 station for MSDU packets 
                                       //or MMPDU frames when a promiscuous packet filter was enabled
    tANI_U32 rx_input_sensitivity;     //The receiver input sensitivity referenced to a FER of 8% at an MPDU length 
                                       //of 1024 bytes at the antenna connector. Each element of the array shall correspond 
                                       //to a supported rate and the order shall be the same as the supporteRates parameter.
    tANI_U32 max_pwr;                  //The maximum transmit power in dBm upto one decimal. 
                                       //for eg: if it is 10.5dBm, the value would be 105 
    tANI_U32 sync_fail_cnt;            //Number of times the receiver failed to synchronize with the incoming signal 
                                       //after detecting the sync in the preamble of the transmitted PLCP protocol data unit. 

    tANI_U32 tx_rate;                  //Legacy transmit rate, in units of 500 kbit/sec, for the most 
                                       //recently transmitted frame    
    tANI_U32  mcs_index;               //mcs index for HT20 and HT40 rates
    tANI_U32  tx_rate_flags;           //to differentiate between HT20 and 
                                       //HT40 rates;  short and long guard interval  
}tAniGlobalClassAStatsInfo, *tpAniGlobalClassAStatsInfo;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32 rx_wep_unencrypted_frm_cnt;  //The number of unencrypted received MPDU frames that the MAC layer discarded when 
                                          //the IEEE 802.11 dot11ExcludeUnencrypted management information base (MIB) object 
                                          //is enabled
    tANI_U32 rx_mic_fail_cnt;             //The number of received MSDU packets that that the 802.11 station discarded 
                                          //because of MIC failures
    tANI_U32 tkip_icv_err;                //The number of encrypted MPDU frames that the 802.11 station failed to decrypt 
                                          //because of a TKIP ICV error
    tANI_U32 aes_ccmp_format_err;         //The number of received MPDU frames that the 802.11 discarded because of an 
                                          //invalid AES-CCMP format
    tANI_U32 aes_ccmp_replay_cnt;         //The number of received MPDU frames that the 802.11 station discarded because of 
                                          //the AES-CCMP replay protection procedure
    tANI_U32 aes_ccmp_decrpt_err;         //The number of received MPDU frames that the 802.11 station discarded because of 
                                          //errors detected by the AES-CCMP decryption algorithm
    tANI_U32 wep_undecryptable_cnt;       //The number of encrypted MPDU frames received for which a WEP decryption key was 
                                          //not available on the 802.11 station
    tANI_U32 wep_icv_err;                 //The number of encrypted MPDU frames that the 802.11 station failed to decrypt 
                                          //because of a WEP ICV error
    tANI_U32 rx_decrypt_succ_cnt;         //The number of received encrypted packets that the 802.11 station successfully 
                                          //decrypted
    tANI_U32 rx_decrypt_fail_cnt;         //The number of encrypted packets that the 802.11 station failed to decrypt

}tAniGlobalSecurityStats, *tpAniGlobalSecurityStats;
   
typedef PACKED_PRE struct PACKED_POST
{
    tAniGlobalSecurityStats ucStats;
    tAniGlobalSecurityStats mcbcStats;
}tAniGlobalClassBStatsInfo, *tpAniGlobalClassBStatsInfo;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32 rx_amsdu_cnt;             //This counter shall be incremented for a received A-MSDU frame with the stations 
                                       //MAC address in the address 1 field or an A-MSDU frame with a group address in the 
                                       //address 1 field
    tANI_U32 rx_ampdu_cnt;             //This counter shall be incremented when the MAC receives an AMPDU from the PHY
    tANI_U32 tx_20_frm_cnt;            //This counter shall be incremented when a Frame is transmitted only on the 
                                       //primary channel
    tANI_U32 rx_20_frm_cnt;            //This counter shall be incremented when a Frame is received only on the primary channel
    tANI_U32 rx_mpdu_in_ampdu_cnt;     //This counter shall be incremented by the number of MPDUs received in the A-MPDU 
                                       //when an A-MPDU is received
    tANI_U32 ampdu_delimiter_crc_err;  //This counter shall be incremented when an MPDU delimiter has a CRC error when this 
                                       //is the first CRC error in the received AMPDU or when the previous delimiter has been 
                                       //decoded correctly
}tAniGlobalClassCStatsInfo, *tpAniGlobalClassCStatsInfo;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32 tx_frag_cnt[4];        //The number of MPDU frames that the 802.11 station transmitted and acknowledged 
                                    //through a received 802.11 ACK frame
    tANI_U32 tx_ampdu_cnt;          //This counter shall be incremented when an A-MPDU is transmitted 
    tANI_U32 tx_mpdu_in_ampdu_cnt;  //This counter shall increment by the number of MPDUs in the AMPDU when an A-MPDU 
                                    //is transmitted
}tAniPerStaStatsInfo, *tpAniPerStaStatsInfo;

typedef PACKED_PRE struct PACKED_POST
{
   /* Success or Failure */
   tANI_U32 status;

   /* STA Idx */
   tANI_U32 staId;

   /* Categories of STATS being returned as per eHalStatsMask*/
   tANI_U32 statsMask;

   /* message type is same as the request type */
   tANI_U16 msgType;

   /* length of the entire request, includes the pStatsBuf length too */
   tANI_U16 msgLen;  

} tHalStatsRspParams, *tpHalStatsRspParams;



typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader  header;
   tHalStatsRspParams statsRspParams;
} tHalStatsRspMsg, *tpHalStatsRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_SET_LINK_ST_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tSirMacAddr bssid;
    tSirLinkState state;
    tSirMacAddr selfMacAddr;
} tLinkStateParams, *tpLinkStateParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tLinkStateParams linkStateParams;
}  tSetLinkStateReqMsg, *tpSetLinkStateReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_SET_LINK_ST_RSP
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
  /*success or failure */
  tANI_U32   status;
} tLinkStateRspParams, *tpLinkStateRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tLinkStateRspParams linkStateRspParams;
}  tSetLinkStateRspMsg, *tpSetLinkStateRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ADD_TS_REQ
 *--------------------------------------------------------------------------*/

/* TSPEC Params */
typedef __ani_attr_pre_packed struct sSirMacTSInfoTfc
{
#ifndef ANI_LITTLE_BIT_ENDIAN
    tANI_U16       ackPolicy : 2;
    tANI_U16       userPrio : 3;
    tANI_U16       psb : 1;
    tANI_U16       aggregation : 1;
    tANI_U16       accessPolicy : 2;
    tANI_U16       direction : 2;
    tANI_U16       tsid : 4;
    tANI_U16       trafficType : 1;
#else
    tANI_U16       trafficType : 1;
    tANI_U16       tsid : 4;
    tANI_U16       direction : 2;
    tANI_U16       accessPolicy : 2;
    tANI_U16       aggregation : 1;
    tANI_U16       psb : 1;
    tANI_U16       userPrio : 3;
    tANI_U16       ackPolicy : 2;
#endif
} __ani_attr_packed tSirMacTSInfoTfc;

/* Flag to schedule the traffic type */
typedef __ani_attr_pre_packed struct sSirMacTSInfoSch
{
#ifndef ANI_LITTLE_BIT_ENDIAN
    tANI_U8        rsvd : 7;
    tANI_U8        schedule : 1;
#else
    tANI_U8        schedule : 1;
    tANI_U8        rsvd : 7;
#endif
} __ani_attr_packed tSirMacTSInfoSch;

/* Traffic and scheduling info */
typedef __ani_attr_pre_packed struct sSirMacTSInfo
{
    tSirMacTSInfoTfc traffic;
    tSirMacTSInfoSch schedule;
} __ani_attr_packed tSirMacTSInfo;

/* Information elements */
typedef __ani_attr_pre_packed struct sSirMacTspecIE
{
    tANI_U8             type;
    tANI_U8             length;
    tSirMacTSInfo       tsinfo;
    tANI_U16            nomMsduSz;
    tANI_U16            maxMsduSz;
    tANI_U32            minSvcInterval;
    tANI_U32            maxSvcInterval;
    tANI_U32            inactInterval;
    tANI_U32            suspendInterval;
    tANI_U32            svcStartTime;
    tANI_U32            minDataRate;
    tANI_U32            meanDataRate;
    tANI_U32            peakDataRate;
    tANI_U32            maxBurstSz;
    tANI_U32            delayBound;
    tANI_U32            minPhyRate;
    tANI_U16            surplusBw;
    tANI_U16            mediumTime;
}__ani_attr_packed tSirMacTspecIE;

typedef PACKED_PRE struct PACKED_POST
{
    /* Station Index */
    tANI_U16 staIdx;

    /* TSPEC handler uniquely identifying a TSPEC for a STA in a BSS */
    tANI_U16 tspecIdx;

    /* To program TPE with required parameters */
    tSirMacTspecIE   tspec;

    /* U-APSD Flags: 1b per AC.  Encoded as follows:
     b7 b6 b5 b4 b3 b2 b1 b0 =
     X  X  X  X  BE BK VI VO */
    tANI_U8 uAPSD;

    /* These parameters are for all the access categories */
    tANI_U32 srvInterval[WLAN_HAL_MAX_AC];   // Service Interval
    tANI_U32 susInterval[WLAN_HAL_MAX_AC];   // Suspend Interval
    tANI_U32 delayInterval[WLAN_HAL_MAX_AC]; // Delay Interval
           
} tAddTsParams, *tpAddTsParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tAddTsParams  addTsParams;
}  tAddTsReqMsg, *tpAddTsReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ADD_TS_RSP
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /*success or failure */
    tANI_U32   status;
} tAddTsRspParams, *tpAddTsRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tAddTsRspParams addTsRspParams;
}  tAddTsRspMsg, *tpAddTsRspMsg;


/*---------------------------------------------------------------------------
 * WLAN_HAL_DEL_TS_REQ
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* Station Index */
    tANI_U16 staIdx;

    /* TSPEC identifier uniquely identifying a TSPEC for a STA in a BSS */
    tANI_U16 tspecIdx;

    /* To lookup station id using the mac address */
    tSirMacAddr bssId; 

} tDelTsParams, *tpDelTsParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tDelTsParams  delTsParams; 
}  tDelTsReqMsg, *tpDelTsReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_DEL_TS_RSP
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /*success or failure */
    tANI_U32   status;
} tDelTsRspParams, *tpDelTsRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tDelTsRspParams delTsRspParams;
}  tDelTsRspMsg, *tpDelTsRspMsg;

/* End of TSpec Parameters */

/* Start of BLOCK ACK related Parameters */

/*---------------------------------------------------------------------------
 * WLAN_HAL_ADD_BA_SESSION_REQ
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* Station Index */
    tANI_U16 staIdx;

    /* Peer MAC Address */
    tSirMacAddr peerMacAddr;

    /* ADDBA Action Frame dialog token
       HAL will not interpret this object */
    tANI_U8 baDialogToken;

    /* TID for which the BA is being setup
       This identifies the TC or TS of interest */
    tANI_U8 baTID;

    /* 0 - Delayed BA (Not supported)
       1 - Immediate BA */
    tANI_U8 baPolicy;

    /* Indicates the number of buffers for this TID (baTID)
       NOTE - This is the requested buffer size. When this
       is processed by HAL and subsequently by HDD, it is
       possible that HDD may change this buffer size. Any
       change in the buffer size should be noted by PE and
       advertized appropriately in the ADDBA response */
    tANI_U16 baBufferSize;

    /* BA timeout in TU's 0 means no timeout will occur */
    tANI_U16 baTimeout;

    /* b0..b3 - Fragment Number - Always set to 0
       b4..b15 - Starting Sequence Number of first MSDU
       for which this BA is setup */
    tANI_U16 baSSN;

    /* ADDBA direction
       1 - Originator
       0 - Recipient */
    tANI_U8 baDirection;
} tAddBASessionParams, *tpAddBASessionParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tAddBASessionParams  addBASessionParams; 
}tAddBASessionReqMsg, *tpAddBASessionReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ADD_BA_SESSION_RSP
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /*success or failure */
    tANI_U32   status;

    /* Dialog token */
    tANI_U8 baDialogToken;

    /* TID for which the BA session has been setup */
    tANI_U8 baTID;

    /* BA Buffer Size allocated for the current BA session */
    tANI_U8 baBufferSize;

    tANI_U8 baSessionID;

    /* Reordering Window buffer */
    tANI_U8 winSize;
    
    /*Station Index to id the sta */
    tANI_U8 STAID;
    
    /* Starting Sequence Number */
    tANI_U16 SSN;
} tAddBASessionRspParams, *tpAddBASessionRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tAddBASessionRspParams addBASessionRspParams;
}  tAddBASessionRspMsg, *tpAddBASessionRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ADD_BA_REQ
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* Session Id */
    tANI_U8 baSessionID;

    /* Reorder Window Size */
    tANI_U8 winSize;

#ifdef FEATURE_ON_CHIP_REORDERING
    tANI_BOOLEAN isReorderingDoneOnChip;
#endif
} tAddBAParams, *tpAddBAParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tAddBAParams  addBAParams; 
}  tAddBAReqMsg, *tpAddBAReqMsg;


/*---------------------------------------------------------------------------
 * WLAN_HAL_ADD_BA_RSP
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /*success or failure */
    tANI_U32   status;

    /* Dialog token */
    tANI_U8 baDialogToken;
 
} tAddBARspParams, *tpAddBARspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tAddBARspParams addBARspParams;
}  tAddBARspMsg, *tpAddBARspMsg;


/*---------------------------------------------------------------------------
 * WLAN_HAL_TRIGGER_BA_REQ
 *--------------------------------------------------------------------------*/


typedef struct sAddBaInfo
{
    tANI_U16 fBaEnable : 1;
    tANI_U16 startingSeqNum: 12;
    tANI_U16 reserved : 3;
}tAddBaInfo, *tpAddBaInfo;

typedef struct sTriggerBaRspCandidate
{
    tSirMacAddr staAddr;
    tAddBaInfo baInfo[STACFG_MAX_TC];
}tTriggerBaRspCandidate, *tpTriggerBaRspCandidate;

typedef struct sTriggerBaCandidate
{
    tANI_U8  staIdx;
    tANI_U8 tidBitmap;
}tTriggerBaReqCandidate, *tptTriggerBaReqCandidate;

typedef PACKED_PRE struct PACKED_POST
{
    /* Session Id */
    tANI_U8 baSessionID;

    /* baCandidateCnt is followed by trigger BA 
     * Candidate List(tTriggerBaCandidate)
     */
    tANI_U16 baCandidateCnt;
    
} tTriggerBAParams, *tpTriggerBAParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tTriggerBAParams  triggerBAParams; 
}  tTriggerBAReqMsg, *tpTriggerBAReqMsg;


/*---------------------------------------------------------------------------
 * WLAN_HAL_TRIGGER_BA_RSP
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   
    /* TO SUPPORT BT-AMP */
    tSirMacAddr  bssId; 

    /* success or failure */
    tANI_U32   status;

    /* baCandidateCnt is followed by trigger BA 
     * Rsp Candidate List(tTriggerRspBaCandidate)
     */
    tANI_U16 baCandidateCnt;
    

} tTriggerBARspParams, *tpTriggerBARspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tTriggerBARspParams triggerBARspParams;
}  tTriggerBARspMsg, *tpTriggerBARspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_DEL_BA_REQ
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* Station Index */
    tANI_U16 staIdx;

    /* TID for which the BA session is being deleted */
    tANI_U8 baTID;

    /* DELBA direction
       1 - Originator
       0 - Recipient */
    tANI_U8 baDirection;  
} tDelBAParams, *tpDelBAParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tDelBAParams  delBAParams; 
}  tDelBAReqMsg, *tpDelBAReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_DEL_BA_RSP
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tDelBARspParams, *tpDelBARspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tDelBARspParams delBARspParams;
}  tDelBARspMsg, *tpDelBARspMsg;



/*---------------------------------------------------------------------------
 * WLAN_HAL_SET_KEYDONE_MSG
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
  /*bssid of the keys */
  tANI_U8   bssidx;
} tSetKeyDoneParams, *tpSetKeyDoneParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tSetKeyDoneParams setKeyDoneParams;
}  tSetKeyDoneMsg, *tpSetKeyDoneMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_DOWNLOAD_NV_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* Fragment sequence number of the NV Image. Note that NV Image might not
     * fit into one message due to size limitation of the SMD channel FIFO. UMAC
     * can hence choose to chop the NV blob into multiple fragments starting with 
     * seqeunce number 0, 1, 2 etc. The last fragment MUST be indicated by 
     * marking the isLastFragment field to 1. Note that all the NV blobs would be
     * concatenated together by HAL without any padding bytes in between.*/
    tANI_U16 fragNumber;

    /* Is this the last fragment? When set to 1 it indicates that no more fragments
     * will be sent by UMAC and HAL can concatenate all the NV blobs rcvd & proceed 
     * with the parsing. HAL would generate a WLAN_HAL_DOWNLOAD_NV_RSP to the
     * WLAN_HAL_DOWNLOAD_NV_REQ after it receives each fragment */
    tANI_U16 isLastFragment;

    /* NV Image size (number of bytes) */
    tANI_U32 nvImgBufferSize;

    /* Following the 'nvImageBufferSize', there should be nvImageBufferSize
     * bytes of NV Image i.e. uint8[nvImageBufferSize] */
} tHalNvImgDownloadReqParams, *tpHalNvImgDownloadReqParams;

typedef PACKED_PRE struct PACKED_POST
{
    /* Note: The length specified in tHalNvImgDownloadReqMsg messages should be
     * header.msgLen = sizeof(tHalNvImgDownloadReqMsg) + nvImgBufferSize */
    tHalMsgHeader header;
    tHalNvImgDownloadReqParams nvImageReqParams;
} tHalNvImgDownloadReqMsg, *tpHalNvImgDownloadReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_DOWNLOAD_NV_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* Success or Failure. HAL would generate a WLAN_HAL_DOWNLOAD_NV_RSP
     * after each fragment */
    tANI_U32   status;
} tHalNvImgDownloadRspParams, *tpHalNvImgDownloadRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tHalNvImgDownloadRspParams nvImageRspParams;
}  tHalNvImgDownloadRspMsg, *tpHalNvImgDownloadRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_STORE_NV_IND
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* NV Item */
    eNvTable tableID;

    /* Size of NV Blob */
    tANI_U32 nvBlobSize;

    /* Following the 'nvBlobSize', there should be nvBlobSize bytes of 
     * NV blob i.e. uint8[nvBlobSize] */
} tHalNvStoreParams, *tpHalNvStoreParams;

typedef PACKED_PRE struct PACKED_POST
{
    /* Note: The length specified in tHalNvStoreInd messages should be
     * header.msgLen = sizeof(tHalNvStoreInd) + nvBlobSize */
    tHalMsgHeader header;
    tHalNvStoreParams nvStoreParams;
}  tHalNvStoreInd, *tpHalNvStoreInd;

/* End of Block Ack Related Parameters */

/*---------------------------------------------------------------------------
 * WLAN_HAL_MIC_FAILURE_IND
 *--------------------------------------------------------------------------*/

#define SIR_CIPHER_SEQ_CTR_SIZE 6

typedef PACKED_PRE struct PACKED_POST
{
    tSirMacAddr  srcMacAddr;     //address used to compute MIC 
    tSirMacAddr  taMacAddr;      //transmitter address
    tSirMacAddr  dstMacAddr;
    tANI_U8      multicast;             
    tANI_U8      IV1;            // first byte of IV
    tANI_U8      keyId;          // second byte of IV
    tANI_U8      TSC[SIR_CIPHER_SEQ_CTR_SIZE]; // sequence number
    tSirMacAddr  rxMacAddr;      // receive address
} tSirMicFailureInfo, *tpSirMicFailureInfo;

/* Definition for MIC failure indication
   MAC reports this each time a MIC failure occures on Rx TKIP packet
 */
typedef PACKED_PRE struct PACKED_POST
{
    tSirMacAddr         bssId;   // BSSID
    tSirMicFailureInfo  info;
} tSirMicFailureInd, *tpSirMicFailureInd;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tSirMicFailureInd micFailureInd;
}  tMicFailureIndMsg, *tpMicFailureIndMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_UPDATE_BEACON_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{

    tANI_U8  bssIdx;

    //shortPreamble mode. HAL should update all the STA rates when it
    //receives this message
    tANI_U8 fShortPreamble;
    //short Slot time.
    tANI_U8 fShortSlotTime;
    //Beacon Interval
    tANI_U16 beaconInterval;
    //Protection related
    tANI_U8 llaCoexist;
    tANI_U8 llbCoexist;
    tANI_U8 llgCoexist;
    tANI_U8 ht20MhzCoexist;
    tANI_U8 llnNonGFCoexist;
    tANI_U8 fLsigTXOPProtectionFullSupport;
    tANI_U8 fRIFSMode;

    tANI_U16 paramChangeBitmap;
}tUpdateBeaconParams, *tpUpdateBeaconParams;


typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tUpdateBeaconParams updateBeaconParam;
}  tUpdateBeaconReqMsg, *tpUpdateBeaconReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_UPDATE_BEACON_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32   status;
} tUpdateBeaconRspParams, *tpUpdateBeaconRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tUpdateBeaconRspParams updateBeaconRspParam;
}  tUpdateBeaconRspMsg, *tpUpdateBeaconRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_SEND_BEACON_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32 beaconLength; //length of the template.
    tANI_U8 beacon[BEACON_TEMPLATE_SIZE];     // Beacon data.
    tSirMacAddr bssId;
#ifdef WLAN_SOFTAP_FEATURE
    tANI_U32 timIeOffset; //TIM IE offset from the beginning of the template.
#endif
    tANI_U16 p2pIeOffset; //P2P IE offset from the begining of the template
}tSendBeaconParams, *tpSendBeaconParams;


typedef PACKED_PRE struct PACKED_POST
{
  tHalMsgHeader header;
  tSendBeaconParams sendBeaconParam;
}tSendBeaconReqMsg, *tpSendBeaconReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_SEND_BEACON_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32   status;
} tSendBeaconRspParams, *tpSendBeaconRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tSendBeaconRspParams sendBeaconRspParam;
}  tSendBeaconRspMsg, *tpSendBeaconRspMsg;

#ifdef FEATURE_5GHZ_BAND

/*---------------------------------------------------------------------------
 * WLAN_HAL_ENABLE_RADAR_DETECT_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tSirMacAddr BSSID;
    tANI_U8   channel; 
}tSirEnableRadarInfoType, *tptSirEnableRadarInfoType;


typedef PACKED_PRE struct PACKED_POST
{
    /* Link Parameters */
    tSirEnableRadarInfoType EnableRadarInfo;
}tEnableRadarReqParams, *tpEnableRadarReqParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tEnableRadarReqParams  enableRadarReqParams; 
}tEnableRadarReqMsg, *tpEnableRadarReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ENABLE_RADAR_DETECT_RSP
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* Link Parameters */
    tSirMacAddr BSSID;
    /* success or failure */
    tANI_U32   status;
}tEnableRadarRspParams, *tpEnableRadarRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tEnableRadarRspParams  enableRadarRspParams; 
}tEnableRadarRspMsg, *tpEnableRadarRspMsg;

/*---------------------------------------------------------------------------
 *WLAN_HAL_RADAR_DETECT_INTR_IND
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8 radarDetChannel;
}tRadarDetectIntrIndParams, *tpRadarDetectIntrIndParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tRadarDetectIntrIndParams  radarDetectIntrIndParams; 
}tRadarDetectIntrIndMsg, *tptRadarDetectIntrIndMsg;

/*---------------------------------------------------------------------------
 *WLAN_HAL_RADAR_DETECT_IND
 *-------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /*channel number in which the RADAR detected*/
    tANI_U8          channelNumber;

    /*RADAR pulse width*/
    tANI_U16         radarPulseWidth; // in usecond

    /*Number of RADAR pulses */
    tANI_U16         numRadarPulse;
}tRadarDetectIndParams,*tpRadarDetectIndParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tRadarDetectIndParams  radarDetectIndParams; 
}tRadarDetectIndMsg, *tptRadarDetectIndMsg;


/*---------------------------------------------------------------------------
 *WLAN_HAL_GET_TPC_REPORT_REQ
 *-------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tSirMacAddr sta;
   tANI_U8     dialogToken;
   tANI_U8     txpower;
}tSirGetTpcReportReqParams, *tpSirGetTpcReportReqParams;


typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tSirGetTpcReportReqParams  getTpcReportReqParams; 
}tSirGetTpcReportReqMsg, *tpSirGetTpcReportReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_GET_TPC_REPORT_RSP
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
}tSirGetTpcReportRspParams, *tpSirGetTpcReportRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tSirGetTpcReportRspParams  getTpcReportRspParams; 
}tSirGetTpcReportRspMsg, *tpSirGetTpcReportRspMsg;

#endif

#ifdef WLAN_SOFTAP_FEATURE
/*---------------------------------------------------------------------------
 *WLAN_HAL_UPDATE_PROBE_RSP_TEMPLATE_REQ
 *-------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8      pProbeRespTemplate[BEACON_TEMPLATE_SIZE];
    tANI_U32     probeRespTemplateLen;
    tANI_U32     ucProxyProbeReqValidIEBmap[8];
    tSirMacAddr  bssId;

}tSendProbeRespReqParams, *tpSendProbeRespReqParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tSendProbeRespReqParams sendProbeRespReqParams ;
}tSendProbeRespReqMsg, *tpSendProbeRespReqMsg;

/*---------------------------------------------------------------------------
 *WLAN_HAL_UPDATE_PROBE_RSP_TEMPLATE_RSP 
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
}tSendProbeRespRspParams, *tpSendProbeRespRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tSendProbeRespRspParams sendProbeRespRspParams;
}tSendProbeRespRspMsg, *tpSendProbeRespRspMsg;


/*---------------------------------------------------------------------------
 *WLAN_HAL_UNKNOWN_ADDR2_FRAME_RX_IND 
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
}tSendUnkownFrameRxIndParams, *tpSendUnkownFrameRxIndParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tSendUnkownFrameRxIndParams sendUnkownFrameRxIndParams;
}tSendUnkownFrameRxIndMsg, *tpSendUnkownFrameRxIndMsg;

/*---------------------------------------------------------------------------
 *WLAN_HAL_DELETE_STA_CONTEXT_IND
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U16    assocId;
    tANI_U16    staId;
    tSirMacAddr bssId; // TO SUPPORT BT-AMP
                       // HAL copies bssid from the sta table.
#ifdef WLAN_SOFTAP_FEATURE
    tSirMacAddr addr2;        //
    tANI_U16    reasonCode;   // To unify the keepalive / unknown A2 / tim-based disa                                                                        
#endif

}tDeleteStaContextParams, *tpDeleteStaContextParams;


typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tDeleteStaContextParams deleteStaContextParams;
}tDeleteStaContextIndMsg, *tpDeleteStaContextIndMsg;

#endif

/*---------------------------------------------------------------------------
 *WLAN_HAL_SIGNAL_BTAMP_EVENT_REQ
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    tBtAmpEventType btAmpEventType;

}tBtAmpEventParams, *tpBtAmpEventParams;



typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tBtAmpEventParams btAmpEventParams;
}tBtAmpEventMsg, *tpBtAmpEventMsg;

/*---------------------------------------------------------------------------
*WLAN_HAL_SIGNAL_BTAMP_EVENT_RSP
*--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
}tBtAmpEventRspParams, *tpBtAmpEventRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tBtAmpEventRspParams btAmpEventRspParams;
}tBtAmpEventRsp, *tpBtAmpEventRsp;


/*---------------------------------------------------------------------------
 *WLAN_HAL_TL_HAL_FLUSH_AC_REQ
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   // Station Index. originates from HAL
    tANI_U8  ucSTAId;

    // TID for which the transmit queue is being flushed
    tANI_U8   ucTid;

}tTlHalFlushAcParams, *tpTlHalFlushAcParams;


typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tTlHalFlushAcParams tlHalFlushAcParam;
}tTlHalFlushAcReq, *tpTlHalFlushAcReq;

/*---------------------------------------------------------------------------
*WLAN_HAL_TL_HAL_FLUSH_AC_RSP
*--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    // Station Index. originates from HAL
    tANI_U8  ucSTAId;

    // TID for which the transmit queue is being flushed
    tANI_U8   ucTid;

    /* success or failure */
    tANI_U32   status;
}tTlHalFlushAcRspParams, *tpTlHalFlushAcRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tTlHalFlushAcRspParams tlHalFlushAcRspParam;
}tTlHalFlushAcRspMsg, *tpTlHalFlushAcRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ENTER_IMPS_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
}  tHalEnterImpsReqMsg, *tpHalEnterImpsReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_EXIT_IMPS_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
}  tHalExitImpsReqMsg, *tpHalExitImpsReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ENTER_BMPS_REQ
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U8         bssIdx;
   //TBTT value derived from the last beacon
#ifndef BUILD_QWPTTSTATIC
   tANI_U64 tbtt;
#endif
   tANI_U8 dtimCount;
   //DTIM period given to HAL during association may not be valid,
   //if association is based on ProbeRsp instead of beacon.
   tANI_U8 dtimPeriod;

   // For CCX and 11R Roaming
   tANI_U32 rssiFilterPeriod;
   tANI_U32 numBeaconPerRssiAverage;
   tANI_U8  bRssiFilterEnable;

} tHalEnterBmpsReqParams, *tpHalEnterBmpsReqParams;


typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalEnterBmpsReqParams enterBmpsReq;
}  tHalEnterBmpsReqMsg, *tpHalEnterBmpsReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_EXIT_BMPS_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tANI_U8     sendDataNull;
} tHalExitBmpsReqParams, *tpHalExitBmpsReqParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalExitBmpsReqParams exitBmpsReqParams;
}  tHalExitBmpsReqMsg, *tpHalExitBmpsReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ADD_BCN_FILTER_REQ
 *--------------------------------------------------------------------------*/
/* Beacon Filtering data structures */
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8     offset;
    tANI_U8     value;
    tANI_U8     bitMask;
    tANI_U8     ref;
} tEidByteInfo, *tpEidByteInfo;

typedef PACKED_PRE struct PACKED_POST 
{
    tANI_U16    capabilityInfo;
    tANI_U16    capabilityMask;
    tANI_U16    beaconInterval;
    tANI_U16    ieNum;
} tBeaconFilterMsg, *tpBeaconFilterMsg;

/* The above structure would be followed by multiple of below mentioned structure */
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8         elementId;
    tANI_U8         checkIePresence;
    tEidByteInfo    byte;
} tBeaconFilterIe, *tpBeaconFilterIe;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tBeaconFilterMsg addBcnFilterParams;
}  tHalAddBcnFilterReqMsg, *tpHalAddBcnFilterReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_REM_BCN_FILTER_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST 
{
    tANI_U8  ucIeCount;
    tANI_U8  ucRemIeId[1];
} tRemBeaconFilterMsg, *tpRemBeaconFilterMsg;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tRemBeaconFilterMsg remBcnFilterParams;
}  tHalRemBcnFilterReqMsg, *tpHalRemBcnFilterReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_HOST_OFFLOAD_REQ
 *--------------------------------------------------------------------------*/
#define HAL_IPV4_ARP_REPLY_OFFLOAD                  0
#define HAL_IPV6_NEIGHBOR_DISCOVERY_OFFLOAD         1
#define HAL_OFFLOAD_DISABLE                         0
#define HAL_OFFLOAD_ENABLE                          1
#define HAL_OFFLOAD_BCAST_FILTER_ENABLE             0x2
#define HAL_OFFLOAD_ARP_AND_BCAST_FILTER_ENABLE     (HAL_OFFLOAD_ENABLE|HAL_OFFLOAD_BCAST_FILTER_ENABLE)

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8 offloadType;
    tANI_U8 enableOrDisable;
    PACKED_PRE union PACKED_POST
    {
        tANI_U8 hostIpv4Addr [4];
        tANI_U8 hostIpv6Addr [16];
    } params;
} tHalHostOffloadReq, *tpHalHostOffloadReq;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalHostOffloadReq hostOffloadParams;
}  tHalHostOffloadReqMsg, *tpHalHostOffloadReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_KEEP_ALIVE_REQ
 *--------------------------------------------------------------------------*/
/* Packet Types. */
#define HAL_KEEP_ALIVE_NULL_PKT              1
#define HAL_KEEP_ALIVE_UNSOLICIT_ARP_RSP     2

/* Enable or disable keep alive */
#define HAL_KEEP_ALIVE_DISABLE   0
#define HAL_KEEP_ALIVE_ENABLE    1

/* Keep Alive request. */
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8          packetType;
    tANI_U32         timePeriod;
    tHalIpv4Addr     hostIpv4Addr; 
    tHalIpv4Addr     destIpv4Addr;
    tSirMacAddr      destMacAddr;
} tHalKeepAliveReq, *tpHalKeepAliveReq;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalKeepAliveReq KeepAliveParams;
}  tHalKeepAliveReqMsg, *tpHalKeepAliveReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_SET_RSSI_THRESH_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tANI_S8   ucRssiThreshold1     : 8;
    tANI_S8   ucRssiThreshold2     : 8;
    tANI_S8   ucRssiThreshold3     : 8;
    tANI_U8   bRssiThres1PosNotify : 1;
    tANI_U8   bRssiThres1NegNotify : 1;
    tANI_U8   bRssiThres2PosNotify : 1;
    tANI_U8   bRssiThres2NegNotify : 1;
    tANI_U8   bRssiThres3PosNotify : 1;
    tANI_U8   bRssiThres3NegNotify : 1;
    tANI_U8   bReserved10          : 2;
} tHalRSSIThresholds, *tpHalRSSIThresholds;
    
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalRSSIThresholds rssiThreshParams;
}  tHalRSSIThresholdsReqMsg, *tpHalRSSIThresholdReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ENTER_UAPSD_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8     bkDeliveryEnabled:1;
    tANI_U8     beDeliveryEnabled:1;
    tANI_U8     viDeliveryEnabled:1;
    tANI_U8     voDeliveryEnabled:1;
    tANI_U8     bkTriggerEnabled:1;
    tANI_U8     beTriggerEnabled:1;
    tANI_U8     viTriggerEnabled:1;
    tANI_U8     voTriggerEnabled:1;
} tUapsdReqParams, *tpUapsdReqParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tUapsdReqParams enterUapsdParams;
}  tHalEnterUapsdReqMsg, *tpHalEnterUapsdReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_EXIT_UAPSD_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
}  tHalExitUapsdReqMsg, *tpHalExitUapsdReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ADD_WOWL_BCAST_PTRN
 *--------------------------------------------------------------------------*/
#define HAL_WOWL_BCAST_PATTERN_MAX_SIZE 128
#define HAL_WOWL_BCAST_MAX_NUM_PATTERNS 8

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8  ucPatternId;           // Pattern ID
    // Pattern byte offset from beginning of the 802.11 packet to start of the
    // wake-up pattern
    tANI_U8  ucPatternByteOffset;   
    tANI_U8  ucPatternSize;         // Non-Zero Pattern size
    tANI_U8  ucPattern[HAL_WOWL_BCAST_PATTERN_MAX_SIZE]; // Pattern
    tANI_U8  ucPatternMaskSize;     // Non-zero pattern mask size
    tANI_U8  ucPatternMask[HAL_WOWL_BCAST_PATTERN_MAX_SIZE]; // Pattern mask
} tHalWowlAddBcastPtrn, *tpHalWowlAddBcastPtrn;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalWowlAddBcastPtrn ptrnParams;
}  tHalWowlAddBcastPtrnReqMsg, *tpHalWowlAddBcastPtrnReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_DEL_WOWL_BCAST_PTRN
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* Pattern ID of the wakeup pattern to be deleted */
    tANI_U8  ucPatternId;
} tHalWowlDelBcastPtrn, *tpHalWowlDelBcastPtrn;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalWowlDelBcastPtrn ptrnParams;
}  tHalWowlDelBcastPtrnReqMsg, *tpHalWowlDelBcastPtrnReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ENTER_WOWL_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* Enables/disables magic packet filtering */
    tANI_U8   ucMagicPktEnable; 

    /* Magic pattern */
    tSirMacAddr magicPtrn;

    /* Enables/disables packet pattern filtering in firmware. 
       Enabling this flag enables broadcast pattern matching 
       in Firmware. If unicast pattern matching is also desired,  
       ucUcastPatternFilteringEnable flag must be set tot true 
       as well 
    */
    tANI_U8   ucPatternFilteringEnable;

    /* Enables/disables unicast packet pattern filtering. 
       This flag specifies whether we want to do pattern match 
       on unicast packets as well and not just broadcast packets. 
       This flag has no effect if the ucPatternFilteringEnable 
       (main controlling flag) is set to false
    */
    tANI_U8   ucUcastPatternFilteringEnable;                     

    /* This configuration is valid only when magicPktEnable=1. 
     * It requests hardware to wake up when it receives the 
     * Channel Switch Action Frame.
     */
    tANI_U8   ucWowChnlSwitchRcv;

    /* This configuration is valid only when magicPktEnable=1. 
     * It requests hardware to wake up when it receives the 
     * Deauthentication Frame. 
     */
    tANI_U8   ucWowDeauthRcv;

    /* This configuration is valid only when magicPktEnable=1. 
     * It requests hardware to wake up when it receives the 
     * Disassociation Frame. 
     */
    tANI_U8   ucWowDisassocRcv;

    /* This configuration is valid only when magicPktEnable=1. 
     * It requests hardware to wake up when it has missed
     * consecutive beacons. This is a hardware register
     * configuration (NOT a firmware configuration). 
     */
    tANI_U8   ucWowMaxMissedBeacons;

    /* This configuration is valid only when magicPktEnable=1. 
     * This is a timeout value in units of microsec. It requests
     * hardware to unconditionally wake up after it has stayed
     * in WoWLAN mode for some time. Set 0 to disable this feature.      
     */
    tANI_U8   ucWowMaxSleepUsec;
} tHalWowlEnterParams, *tpHalWowlEnterParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalWowlEnterParams enterWowlParams;
}  tHalWowlEnterReqMsg, *tpHalWowlEnterReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_EXIT_WOWL_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
}  tHalWowlExitReqMsg, *tpHalWowlExitReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_GET_RSSI_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
}  tHalGetRssiReqMsg, *tpHalGetRssiReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_SET_UAPSD_AC_PARAMS_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST {
    tANI_U8  staidx;        // STA index
    tANI_U8  ac;            // Access Category
    tANI_U8  up;            // User Priority
    tANI_U32 srvInterval;   // Service Interval
    tANI_U32 susInterval;   // Suspend Interval
    tANI_U32 delayInterval; // Delay Interval
} tUapsdInfo, tpUapsdInfo;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tUapsdInfo    enableUapsdAcParams;
}  tHalSetUapsdAcParamsReqMsg, *tpHalSetUapsdAcParamsReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_CONFIGURE_RXP_FILTER_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST {
    tANI_U8 setMcstBcstFilterSetting;
    tANI_U8 setMcstBcstFilter;
} tHalConfigureRxpFilterReqParams, tpHalConfigureRxpFilterReqParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalConfigureRxpFilterReqParams    configureRxpFilterReqParams;
}  tHalConfigureRxpFilterReqMsg, *tpHalConfigureRxpFilterReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ENTER_IMPS_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tHalEnterImpsRspParams, *tpHalEnterImpsRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalEnterImpsRspParams enterImpsRspParams;
}  tHalEnterImpsRspMsg, *tpHalEnterImpsRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_EXIT_IMPS_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tHalExitImpsRspParams, *tpHalExitImpsRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalExitImpsRspParams exitImpsRspParams;
}  tHalExitImpsRspMsg, *tpHalExitImpsRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ENTER_BMPS_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tHalEnterBmpsRspParams, *tpHalEnterBmpsRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalEnterBmpsRspParams enterBmpsRspParams;
}  tHalEnterBmpsRspMsg, *tpHalEnterBmpsRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_EXIT_BMPS_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tHalExitBmpsRspParams, *tpHalExitBmpsRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalExitBmpsRspParams exitBmpsRspParams;
}  tHalExitBmpsRspMsg, *tpHalExitBmpsRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ENTER_UAPSD_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32    status;
}tUapsdRspParams, *tpUapsdRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tUapsdRspParams enterUapsdRspParams;
}  tHalEnterUapsdRspMsg, *tpHalEnterUapsdRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_EXIT_UAPSD_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tHalExitUapsdRspParams, *tpHalExitUapsdRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalExitUapsdRspParams exitUapsdRspParams;
}  tHalExitUapsdRspMsg, *tpHalExitUapsdRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_RSSI_NOTIFICATION_IND
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32             bRssiThres1PosCross : 1;
    tANI_U32             bRssiThres1NegCross : 1;
    tANI_U32             bRssiThres2PosCross : 1;
    tANI_U32             bRssiThres2NegCross : 1;
    tANI_U32             bRssiThres3PosCross : 1;
    tANI_U32             bRssiThres3NegCross : 1;
    tANI_U32             bReserved           : 26;
} tHalRSSINotification, *tpHalRSSINotification;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalRSSINotification rssiNotificationParams;
}  tHalRSSINotificationIndMsg, *tpHalRSSINotificationIndMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_GET_RSSI_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
    tANI_S8    rssi;
} tHalGetRssiParams, *tpHalGetRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalGetRssiParams rssiRspParams;
}  tHalGetRssiRspMsg, *tpHalGetRssiRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ENTER_WOWL_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tHalEnterWowlRspParams, *tpHalEnterWowlRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalEnterWowlRspParams enterWowlRspParams;
}  tHalWowlEnterRspMsg, *tpHalWowlEnterRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_EXIT_WOWL_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tHalExitWowlRspParams, *tpHalExitWowlRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalExitWowlRspParams exitWowlRspParams;
}  tHalWowlExitRspMsg, *tpHalWowlExitRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ADD_BCN_FILTER_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tHalAddBcnFilterRspParams, *tpHalAddBcnFilterRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalAddBcnFilterRspParams addBcnFilterRspParams;
}  tHalAddBcnFilterRspMsg, *tpHalAddBcnFilterRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_REM_BCN_FILTER_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tHalRemBcnFilterRspParams, *tpHalRemBcnFilterRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalRemBcnFilterRspParams remBcnFilterRspParams;
}  tHalRemBcnFilterRspMsg, *tpHalRemBcnFilterRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_ADD_WOWL_BCAST_PTRN_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tHalAddWowlBcastPtrnRspParams, *tpHalAddWowlBcastPtrnRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalAddWowlBcastPtrnRspParams addWowlBcastPtrnRspParams;
}  tHalAddWowlBcastPtrnRspMsg, *tpHalAddWowlBcastPtrnRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_DEL_WOWL_BCAST_PTRN_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tHalDelWowlBcastPtrnRspParams, *tpHalDelWowlBcastPtrnRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalDelWowlBcastPtrnRspParams delWowlBcastRspParams;
}  tHalDelWowlBcastPtrnRspMsg, *tpHalDelWowlBcastPtrnRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_HOST_OFFLOAD_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tHalHostOffloadRspParams, *tpHalHostOffloadRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalHostOffloadRspParams hostOffloadRspParams;
}  tHalHostOffloadRspMsg, *tpHalHostOffloadRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_KEEP_ALIVE_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tHalKeepAliveRspParams, *tpHalKeepAliveRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalKeepAliveRspParams keepAliveRspParams;
}  tHalKeepAliveRspMsg, *tpHalKeepAliveRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_SET_RSSI_THRESH_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tHalSetRssiThreshRspParams, *tpHalSetRssiThreshRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalSetRssiThreshRspParams setRssiThreshRspParams;
}  tHalSetRssiThreshRspMsg, *tpHalSetRssiThreshRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_SET_UAPSD_AC_PARAMS_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tHalSetUapsdAcParamsRspParams, *tpHalSetUapsdAcParamsRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalSetUapsdAcParamsRspParams setUapsdAcParamsRspParams;
}  tHalSetUapsdAcParamsRspMsg, *tpHalSetUapsdAcParamsRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_CONFIGURE_RXP_FILTER_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tHalConfigureRxpFilterRspParams, *tpHalConfigureRxpFilterRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalConfigureRxpFilterRspParams configureRxpFilterRspParams;
}  tHalConfigureRxpFilterRspMsg, *tpHalConfigureRxpFilterRspMsg;

/*---------------------------------------------------------------------------
 *WLAN_HAL_SET_MAX_TX_POWER_REQ
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    tSirMacAddr bssId;  // BSSID is needed to identify which session issued this request. As
                        //the request has power constraints, this should be applied only to that session
    tSirMacAddr selfStaMacAddr;
    //In request,
    //power == MaxTx power to be used.
    tPowerdBm  power;

}tSetMaxTxPwrParams, *tpSetMaxTxPwrParams;


typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tSetMaxTxPwrParams setMaxTxPwrParams;
}tSetMaxTxPwrReq, *tpSetMaxTxPwrReq;

/*---------------------------------------------------------------------------
*WLAN_HAL_SET_MAX_TX_POWER_RSP
*--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    //power == tx power used for management frames.
    tPowerdBm  power;

    /* success or failure */
    tANI_U32   status;
}tSetMaxTxPwrRspParams, *tpSetMaxTxPwrRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tSetMaxTxPwrRspParams setMaxTxPwrRspParams;
}tSetMaxTxPwrRspMsg, *tpSetMaxTxPwrRspMsg;

/*---------------------------------------------------------------------------
 *WLAN_HAL_SET_TX_POWER_REQ
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* TX Power in milli watts */
    tANI_U32  txPower;
}tSetTxPwrReqParams, *tpSetTxPwrReqParams;


typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tSetTxPwrReqParams setTxPwrReqParams;
}tSetTxPwrReqMsg, *tpSetTxPwrReqMsg;

/*---------------------------------------------------------------------------
*WLAN_HAL_SET_TX_POWER_RSP
*--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
}tSetTxPwrRspParams, *tpSetTxPwrRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tSetTxPwrRspParams setTxPwrRspParams;
}tSetTxPwrRspMsg, *tpSetTxPwrRspMsg;

/*---------------------------------------------------------------------------
 *WLAN_HAL_GET_TX_POWER_REQ
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8  staId;
}tGetTxPwrReqParams, *tpGetTxPwrReqParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tGetTxPwrReqParams getTxPwrReqParams;
}tGetTxPwrReqMsg, *tpGetTxPwrReqMsg;

/*---------------------------------------------------------------------------
*WLAN_HAL_GET_TX_POWER_RSP
*--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;

    /* TX Power in milli watts */
    tANI_U32   txPower;
}tGetTxPwrRspParams, *tpGetTxPwrRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tGetTxPwrRspParams getTxPwrRspParams;
}tGetTxPwrRspMsg, *tpGetTxPwrRspMsg;

#ifdef WLAN_FEATURE_P2P
/*---------------------------------------------------------------------------
 *WLAN_HAL_SET_P2P_GONOA_REQ
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
  tANI_U8   opp_ps;
  tANI_U32  ctWindow;
  tANI_U8   count; 
  tANI_U32  duration;
  tANI_U32  interval;
  tANI_U32  single_noa_duration;
  tANI_U8   psSelection;
}tSetP2PGONOAParams, *tpSetP2PGONOAParams;


typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tSetP2PGONOAParams setP2PGONOAParams;
}tSetP2PGONOAReq, *tpSetP2PGONOAReq;

/*---------------------------------------------------------------------------
*WLAN_HAL_SET_P2P_GONOA_RSP
*--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
}tSetP2PGONOARspParams, *tpSetP2PGONOARspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tSetP2PGONOARspParams setP2PGONOARspParams;
}tSetP2PGONOARspMsg, *tpSetP2PGONOARspMsg;
#endif

/*---------------------------------------------------------------------------
 *WLAN_HAL_ADD_SELF_STA_REQ
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
  tSirMacAddr selfMacAddr;
  tANI_U32    status;
}tAddStaSelfParams, *tpAddStaSelfParams;


typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tAddStaSelfParams addStaSelfParams;
}tAddStaSelfReq, *tpAddStaSelfReq;

/*---------------------------------------------------------------------------
*WLAN_HAL_ADD_SELF_STA_RSP
*--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;

   /*Self STA Index */
   tANI_U8    selfStaIdx;

   /* DPU Index (IGTK, PTK, GTK all same) */
   tANI_U8 dpuIdx;

   /* DPU Signature */
   tANI_U8 dpuSignature;
   
}tAddStaSelfRspParams, *tpAddStaSelfRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tAddStaSelfRspParams addStaSelfRspParams;
}tAddStaSelfRspMsg, *tpAddStaSelfRspMsg;


/*---------------------------------------------------------------------------
  WLAN_HAL_DEL_STA_SELF_REQ
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tSirMacAddr selfMacAddr;

}tDelStaSelfParams, *tpDelStaSelfParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tDelStaSelfParams delStaSelfParams;
}  tDelStaSelfReqMsg, *tpDelStaSelfReqMsg;


/*---------------------------------------------------------------------------
  WLAN_HAL_DEL_STA_SELF_RSP
---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
  /*success or failure */
  tANI_U32   status;

  tSirMacAddr selfMacAddr;
}tDelStaSelfRspParams, *tpDelStaSelfRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tDelStaSelfRspParams delStaSelfRspParams;
}  tDelStaSelfRspMsg, *tpDelStaSelfRspMsg;


#ifdef WLAN_FEATURE_VOWIFI_11R

/*---------------------------------------------------------------------------
 *WLAN_HAL_AGGR_ADD_TS_REQ
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* Station Index */
    tANI_U16 staIdx;

    /* TSPEC handler uniquely identifying a TSPEC for a STA in a BSS */
    /* This will carry the bitmap with the bit positions representing different AC.s*/
    tANI_U16 tspecIdx;

    /*  Tspec info per AC To program TPE with required parameters */
    tSirMacTspecIE   tspec[WLAN_HAL_MAX_AC];

    /* U-APSD Flags: 1b per AC.  Encoded as follows:
     b7 b6 b5 b4 b3 b2 b1 b0 =
     X  X  X  X  BE BK VI VO */
    tANI_U8 uAPSD;

    /* These parameters are for all the access categories */
    tANI_U32 srvInterval[WLAN_HAL_MAX_AC];   // Service Interval
    tANI_U32 susInterval[WLAN_HAL_MAX_AC];   // Suspend Interval
    tANI_U32 delayInterval[WLAN_HAL_MAX_AC]; // Delay Interval

}tAggrAddTsParams, *tpAggrAddTsParams;


typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tAggrAddTsParams aggrAddTsParam;
}tAggrAddTsReq, *tpAggrAddTsReq;

/*---------------------------------------------------------------------------
*WLAN_HAL_AGGR_ADD_TS_RSP
*--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status0;
    /* FIXME PRIMA for future use for 11R */
    tANI_U32   status1;
}tAggrAddTsRspParams, *tpAggrAddTsRspParams;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader header;
    tAggrAddTsRspParams aggrAddTsRspParam;
}tAggrAddTsRspMsg, *tpAggrAddTsRspMsg;

#endif

/*---------------------------------------------------------------------------
 * WLAN_HAL_CONFIGURE_APPS_CPU_WAKEUP_STATE_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tANI_U8   isAppsCpuAwake;
} tHalConfigureAppsCpuWakeupStateReqParams, *tpHalConfigureAppsCpuWakeupStatReqParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalConfigureAppsCpuWakeupStateReqParams appsStateReqParams;
}  tHalConfigureAppsCpuWakeupStateReqMsg, *tpHalConfigureAppsCpuWakeupStateReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_CONFIGURE_APPS_CPU_WAKEUP_STATE_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tHalConfigureAppsCpuWakeupStateRspParams, *tpHalConfigureAppsCpuWakeupStateRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalConfigureAppsCpuWakeupStateRspParams appsStateRspParams;
}  tHalConfigureAppsCpuWakeupStateRspMsg, *tpHalConfigureAppsCpuWakeupStateRspMsg;
/*---------------------------------------------------------------------------
 * WLAN_HAL_DUMP_COMMAND_REQ
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U32    argument1;
   tANI_U32    argument2;
   tANI_U32    argument3;
   tANI_U32    argument4;
   tANI_U32    argument5;

}tHalDumpCmdReqParams,*tpHalDumpCmdReqParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader          header;
   tHalDumpCmdReqParams   dumpCmdReqParams;
} tHalDumpCmdReqMsg, *tpHalDumpCmdReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_DUMP_COMMAND_RSP
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
    /*Length of the responce message*/
    tANI_U32   rspLength;
    /*FiXME: Currently considering the  the responce will be less than 100bytes */
    tANI_U8    rspBuffer[DUMPCMD_RSP_BUFFER];
    
} tHalDumpCmdRspParams, *tpHalDumpCmdRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalDumpCmdRspParams dumpCmdRspParams;
}  tHalDumpCmdRspMsg, *tpHalDumpCmdRspMsg;

/*---------------------------------------------------------------------------
 *WLAN_HAL_COEX_IND
 *-------------------------------------------------------------------------*/
#define WLAN_COEX_IND_DATA_SIZE (4)
#define WLAN_COEX_IND_TYPE_DISABLE_HB_MONITOR (0)
#define WLAN_COEX_IND_TYPE_ENABLE_HB_MONITOR (1)

typedef PACKED_PRE struct PACKED_POST
{
    /*Coex Indication Type*/
    tANI_U32   coexIndType;

    /*Coex Indication Data*/
    tANI_U32   coexIndData[WLAN_COEX_IND_DATA_SIZE];
}tCoexIndParams,*tpCoexIndParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader   header;
   tCoexIndParams  coexIndParams; 
}tCoexIndMsg, *tpCoexIndMsg;

/*---------------------------------------------------------------------------
 *WLAN_HAL_OTA_TX_COMPL_IND
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   /*Tx Complete Indication Success or Failure*/
   tANI_U32   status;
}tTxComplParams,*tpTxComplParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader   header;
   tTxComplParams  txComplParams; 
}tTxComplIndMsg, *tpTxComplIndMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_HOST_SUSPEND_IND
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32 configuredMcstBcstFilterSetting;
    tANI_U32 activeSessionCount; 
}tHalWlanHostSuspendIndParam,*tpHalWlanHostSuspendIndParam;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalWlanHostSuspendIndParam suspendIndParams;
}tHalWlanHostSuspendIndMsg, *tpHalWlanHostSuspendIndMsg;

#ifdef WLAN_FEATURE_P2P
/*---------------------------------------------------------------------------
 *WLAN_HAL_NOA_ATTR_IND
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U8      index ;
   tANI_U8      oppPsFlag ;
   tANI_U16     ctWin  ;

   tANI_U16      uNoa1IntervalCnt;
   tANI_U16      rsvd1 ;
   tANI_U32      uNoa1Duration;
   tANI_U32      uNoa1Interval;
   tANI_U32      uNoa1StartTime;

   tANI_U16      uNoa2IntervalCnt;
   tANI_U16      rsvd2;
   tANI_U32      uNoa2Duration;
   tANI_U32      uNoa2Interval;
   tANI_U32      uNoa2StartTime;

   tANI_U32   status;
}tNoaAttrIndParams, *tpNoaAttrIndParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader      header;
   tNoaAttrIndParams  noaAttrIndParams; 
}tNoaAttrIndMsg, *tpNoaAttrIndMsg;
#endif

/*---------------------------------------------------------------------------
 * WLAN_HAL_HOST_RESUME_REQ
 *-------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8 configuredMcstBcstFilterSetting;
}tHalWlanHostResumeReqParam,*tpHalWlanHostResumeReqParam;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalWlanHostResumeReqParam resumeReqParams;
}tHalWlanHostResumeReqMsg, *tpHalWlanHostResumeReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_HOST_RESUME_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tHalHostResumeRspParams, *tpHalHostResumeRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalHostResumeRspParams hostResumeRspParams;
}  tHalHostResumeRspMsg, *tpHalHostResumeRspMsg;

/*---------------------------------------------------------------------------
 *PNO Messages
 *-------------------------------------------------------------------------*/
/*Max number of channels that a network can be found on*/
#define WLAN_HAL_PNO_MAX_NETW_CHANNELS  26

/*Maximum numbers of networks supported by PNO*/
#define WLAN_HAL_PNO_MAX_SUPP_NETWORKS  16

/*The number of scan time intervals that can be programmed into PNO*/
#define WLAN_HAL_PNO_MAX_SCAN_TIMERS    10

/*Maximum size of the probe template*/
#define WLAN_HAL_PNO_MAX_PROBE_SIZE     450

/*Type of PNO enabling 
  Immediate - scanning will start immediately and PNO procedure will
  be repeated based on timer
  Suspend - scanning will start at suspend
  Resume - scanning will start on system resume*/
typedef enum
{
   ePNO_MODE_IMMEDIATE,
   ePNO_MODE_ON_SUSPEND,
   ePNO_MODE_ON_RESUME,
   ePNO_MODE_MAX = WLAN_HAL_MAX_ENUM_SIZE
} ePNOMode;

/*Authentication type*/
typedef enum 
{
    eAUTH_TYPE_ANY                   = 0,    
    eAUTH_TYPE_OPEN_SYSTEM           = 1,
    
    // Upper layer authentication types
    eAUTH_TYPE_WPA                   = 2,
    eAUTH_TYPE_WPA_PSK               = 3,
    
    eAUTH_TYPE_RSN                   = 4,
    eAUTH_TYPE_RSN_PSK               = 5,
    eAUTH_TYPE_FT_RSN                = 6,
    eAUTH_TYPE_FT_RSN_PSK            = 7,
    eAUTH_TYPE_WAPI_WAI_CERTIFICATE  = 8,
    eAUTH_TYPE_WAPI_WAI_PSK          = 9,
    
    eAUTH_TYPE_MAX = WLAN_HAL_MAX_ENUM_SIZE

}tAuthType;

/* Encryption type */
typedef enum eEdType
{
    eED_ANY           = 0,
    eED_NONE          = 1,
    eED_WEP           = 2,
    eED_TKIP          = 3,
    eED_CCMP          = 4,
    eED_WPI           = 5,
        
    eED_TYPE_MAX = WLAN_HAL_MAX_ENUM_SIZE
} tEdType;

/* SSID broadcast  type */
typedef enum eSSIDBcastType
{
  eBCAST_UNKNOWN      = 0,
  eBCAST_NORMAL       = 1,
  eBCAST_HIDDEN       = 2,

  eBCAST_TYPE_MAX     = WLAN_HAL_MAX_ENUM_SIZE
} tSSIDBcastType;

/* 
  The network description for which PNO will have to look for
*/
typedef PACKED_PRE struct PACKED_POST
{
  /*SSID of the BSS*/
  tSirMacSSid ssId;

  /*Authentication type for the network*/
  tAuthType   authentication; 

  /*Encryption type for the network*/
  tEdType     encryption; 

  /*Indicate the channel on which the Network can be found 
    0 - if all channels */
  tANI_U8     ucChannelCount;
  tANI_U8     aChannels[WLAN_HAL_PNO_MAX_NETW_CHANNELS];

  /*Indicates the RSSI threshold for the network to be considered*/
  tANI_U8     rssiThreshold;
}tNetworkType; 

typedef PACKED_PRE struct PACKED_POST
{
  /*How much it should wait */
  tANI_U32    uTimerValue; 

  /*How many times it should repeat that wait value 
    0 - keep using this timer until PNO is disabled*/
  tANI_U32    uTimerRepeat; 

  /*e.g:   2 3 
           4 0 
    - it will wait 2s between consecutive scans for 3 times
    - after that it will wait 4s between consecutive scans until disabled*/
}tScanTimer; 

/* 
  The network parameters to be sent to the PNO algorithm
*/
typedef PACKED_PRE struct PACKED_POST
{
  /*set to 0 if you wish for PNO to use its default telescopic timer*/
  tANI_U8     ucScanTimersCount; 

  /*A set value represents the amount of time that PNO will wait between 
    two consecutive scan procedures
    If the desired is for a uniform timer that fires always at the exact same
    interval - one single value is to be set
    If there is a desire for a more complex - telescopic like timer multiple
    values can be set - once PNO reaches the end of the array it will
    continue scanning at intervals presented by the last value*/
  tScanTimer  aTimerValues[WLAN_HAL_PNO_MAX_SCAN_TIMERS]; 

}tScanTimersType;

typedef PACKED_PRE struct PACKED_POST {

    /*Enable PNO*/
    tANI_U32          enable;

    /*Immediate,  On Suspend,   On Resume*/
    ePNOMode         modePNO;
    
    /*Number of networks sent for PNO*/
    tANI_U32          ucNetworksCount; 

    /*The networks that PNO needs to look for*/
    tNetworkType     aNetworks[WLAN_HAL_PNO_MAX_SUPP_NETWORKS];

    /*The scan timers required for PNO*/
    tScanTimersType  scanTimers; 

    /*Probe template for 2.4GHz band*/
    tANI_U16         us24GProbeSize; 
    tANI_U8          a24GProbeTemplate[WLAN_HAL_PNO_MAX_PROBE_SIZE];

    /*Probe template for 5GHz band*/
    tANI_U16         us5GProbeSize; 
    tANI_U8          a5GProbeTemplate[WLAN_HAL_PNO_MAX_PROBE_SIZE];

} tPrefNetwListParams, * tpPrefNetwListParams;

/*
  Preferred network list request 
*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tPrefNetwListParams   prefNetwListParams;
}  tSetPrefNetwListReq, *tpSetPrefNetwListReq;


/* 
  The network description for which PNO will have to look for
*/
typedef PACKED_PRE struct PACKED_POST
{
  /*SSID of the BSS*/
  tSirMacSSid ssId;

  /*Authentication type for the network*/
  tAuthType   authentication; 

  /*Encryption type for the network*/
  tEdType     encryption; 

  /*SSID broadcast type, normal, hidden or unknown*/
  tSSIDBcastType bcastNetworkType;

  /*Indicate the channel on which the Network can be found 
    0 - if all channels */
  tANI_U8     ucChannelCount;
  tANI_U8     aChannels[WLAN_HAL_PNO_MAX_NETW_CHANNELS];

  /*Indicates the RSSI threshold for the network to be considered*/
  tANI_U8     rssiThreshold;
}tNetworkTypeNew; 

typedef PACKED_PRE struct PACKED_POST {

    /*Enable PNO*/
    tANI_U32          enable;

    /*Immediate,  On Suspend,   On Resume*/
    ePNOMode         modePNO;
    
    /*Number of networks sent for PNO*/
    tANI_U32         ucNetworksCount; 

    /*The networks that PNO needs to look for*/
    tNetworkTypeNew  aNetworks[WLAN_HAL_PNO_MAX_SUPP_NETWORKS];

    /*The scan timers required for PNO*/
    tScanTimersType  scanTimers; 

    /*Probe template for 2.4GHz band*/
    tANI_U16         us24GProbeSize; 
    tANI_U8          a24GProbeTemplate[WLAN_HAL_PNO_MAX_PROBE_SIZE];

    /*Probe template for 5GHz band*/
    tANI_U16         us5GProbeSize; 
    tANI_U8          a5GProbeTemplate[WLAN_HAL_PNO_MAX_PROBE_SIZE];

} tPrefNetwListParamsNew, * tpPrefNetwListParamsNew;

/*
  Preferred network list request new
*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tPrefNetwListParamsNew   prefNetwListParams;
}  tSetPrefNetwListReqNew, *tpSetPrefNetwListReqNew;

/*
  Preferred network list response 
*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;

   /*status of the request - just to indicate that PNO has acknowledged 
     the request and will start scanning*/
   tANI_U32   status;
}  tSetPrefNetwListResp, *tpSetPrefNetwListResp;

/*
  Preferred network indication parameters 
*/
typedef PACKED_PRE struct PACKED_POST {

  /*Network that was found with the highest RSSI*/
  tSirMacSSid ssId;
  
  /*Indicates the RSSI */
  tANI_U8     rssi;

} tPrefNetwFoundParams, * tpPrefNetwFoundParams;

/*
  Preferred network found indication
*/
typedef PACKED_PRE struct PACKED_POST {

   tHalMsgHeader header;
   tPrefNetwFoundParams   prefNetwFoundParams;
}  tPrefNetwFoundInd, *tpPrefNetwFoundInd;


typedef PACKED_PRE struct PACKED_POST {

  /*RSSI Threshold*/
  tANI_U8          ucRssiThreshold;

} tRssiFilterParams, * tpRssiFilterParams;

/*
  RSSI Filter request 
*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tRssiFilterParams   prefRSSIFilterParams;
}  tSetRssiFilterReq, *tpSetRssiFilterReq;

/*
 Set RSSI filter resp
*/
typedef PACKED_PRE struct PACKED_POST{
   tHalMsgHeader header;
   /*status of the request */
   tANI_U32   status;
}  tSetRssiFilterResp, *tpSetRssiFilterResp;
/*
  Update scan params 
*/
typedef PACKED_PRE struct PACKED_POST  
{

  /*Host setting for 11d*/
  tANI_U8   b11dEnabled; 

  /*Lets PNO know that host has determined the regulatory domain*/
  tANI_U8   b11dResolved;

  /*Channels on which PNO is allowed to scan*/
  tANI_U8   ucChannelCount; 
  tANI_U8   aChannels[WLAN_HAL_PNO_MAX_NETW_CHANNELS]; 

  /*Minimum channel time*/
  tANI_U16  usActiveMinChTime; 

  /*Maximum channel time*/
  tANI_U16  usActiveMaxChTime; 

  /*Minimum channel time*/
  tANI_U16  usPassiveMinChTime; 

  /*Maximum channel time*/
  tANI_U16  usPassiveMaxChTime; 

  /*Cb State*/
  ePhyChanBondState cbState;

} tUpdateScanParams, * tpUpdateScanParams;

/*
  Update scan params - sent from host to PNO
  to be used during PNO scanning 
*/
typedef PACKED_PRE struct PACKED_POST{

   tHalMsgHeader header;
   tUpdateScanParams   scanParams;
}  tUpdateScanParamsReq, *tpUpdateScanParamsReq;

/*
  Update scan params - sent from host to PNO
  to be used during PNO scanning 
*/
typedef PACKED_PRE struct PACKED_POST{

   tHalMsgHeader header;

   /*status of the request */
   tANI_U32   status;

}  tUpdateScanParamsResp, *tpUpdateScanParamsResp;

/*---------------------------------------------------------------------------
 * WLAN_HAL_SET_TX_PER_TRACKING_REQ
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8  ucTxPerTrackingEnable;           /* 0: disable, 1:enable */
    tANI_U8  ucTxPerTrackingPeriod;           /* Check period, unit is sec. */
    tANI_U8  ucTxPerTrackingRatio;            /* (Fail TX packet)/(Total TX packet) ratio, the unit is 10%. */
    tANI_U32 uTxPerTrackingWatermark;         /* A watermark of check number, once the tx packet exceed this number, we do the check, default is 5 */
} tHalTxPerTrackingReqParam, *tpHalTxPerTrackingReqParam;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalTxPerTrackingReqParam txPerTrackingParams;
}  tHalSetTxPerTrackingReqMsg, *tpHalSetTxPerTrackingReqMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_SET_TX_PER_TRACKING_RSP
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tHalTxPerTrackingRspParams, *tpHalTxPerTrackingRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader header;
   tHalTxPerTrackingRspParams txPerTrackingRspParams;
}  tHalSetTxPerTrackingRspMsg, *tpHalSetTxPerTrackingRspMsg;

/*---------------------------------------------------------------------------
 * WLAN_HAL_TX_PER_HIT_IND
 *--------------------------------------------------------------------------*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader   header;
}tTxPerHitIndMsg, *tpTxPerHitIndMsg;

/*---------------------------------------------------------------------------
 *******************Packet Filtering Definitions Begin*******************
 *--------------------------------------------------------------------------*/
#define    HAL_PROTOCOL_DATA_LEN                  8
#define    HAL_MAX_NUM_MULTICAST_ADDRESS        240
#define    HAL_MAX_NUM_FILTERS                   20
#define    HAL_MAX_CMP_PER_FILTER                10

typedef enum
{
  HAL_RCV_FILTER_TYPE_INVALID,
  HAL_RCV_FILTER_TYPE_FILTER_PKT,
  HAL_RCV_FILTER_TYPE_BUFFER_PKT,
  HAL_RCV_FILTER_TYPE_MAX_ENUM_SIZE
}tHalReceivePacketFilterType;

typedef enum 
{
  HAL_FILTER_PROTO_TYPE_INVALID,
  HAL_FILTER_PROTO_TYPE_MAC,
  HAL_FILTER_PROTO_TYPE_ARP,
  HAL_FILTER_PROTO_TYPE_IPV4,
  HAL_FILTER_PROTO_TYPE_IPV6,
  HAL_FILTER_PROTO_TYPE_UDP,
  HAL_FILTER_PROTO_TYPE_MAX
}tHalRcvPktFltProtocolType;

typedef enum 
{
  HAL_FILTER_CMP_TYPE_INVALID,
  HAL_FILTER_CMP_TYPE_EQUAL,
  HAL_FILTER_CMP_TYPE_MASK_EQUAL,
  HAL_FILTER_CMP_TYPE_NOT_EQUAL,
  HAL_FILTER_CMP_TYPE_MAX
}tHalRcvPktFltCmpFlagType;

typedef PACKED_PRE struct PACKED_POST 
{
    tANI_U8                          protocolLayer;
    tANI_U8                          cmpFlag;
    tANI_U16                         dataLength; /* Length of the data to compare */
    tANI_U8                          dataOffset; /* from start of the respective frame header */
    tANI_U8                          reserved; /* Reserved field */
    tANI_U8                          compareData[HAL_PROTOCOL_DATA_LEN];  /* Data to compare */
    tANI_U8                          dataMask[HAL_PROTOCOL_DATA_LEN];   /* Mask to be applied on the received packet data before compare */
}tHalRcvPktFilterParams, *tpHalRcvPktFilterParams;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U8                         filterId;
    tANI_U8                         filterType;
    tANI_U8                         numParams; 
    tANI_U32                        coleasceTime; 
    tHalRcvPktFilterParams          paramsData[1];
}tHalRcvPktFilterCfgType, *tpHalRcvPktFilterCfgType;

typedef PACKED_PRE struct PACKED_POST
{
  tHalMsgHeader                 header;
  tHalRcvPktFilterCfgType       pktFilterCfg;
} tHalSetRcvPktFilterReqMsg, *tpHalSetRcvPktFilterReqMsg;


typedef PACKED_PRE struct PACKED_POST 
{
    tANI_U8         dataOffset; /* from start of the respective frame header */
    tANI_U32       cMulticastAddr;
    tSirMacAddr    multicastAddr[HAL_MAX_NUM_MULTICAST_ADDRESS];
} tHalRcvFltMcAddrListType, *tpHalRcvFltMcAddrListType;

typedef PACKED_PRE struct PACKED_POST
{
    /* success or failure */
    tANI_U32   status;
} tHalSetPktFilterRspParams, *tpHalSetPktFilterRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader               header;
   tHalSetPktFilterRspParams   pktFilterRspParams;
}  tHalSetPktFilterRspMsg, *tpHalSetPktFilterRspMsg;


typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader        header;
} tHalRcvFltPktMatchCntReqMsg, *tpHalRcvFltPktMatchCntReqMsg;

typedef PACKED_PRE struct PACKED_POST
{
   tANI_U8    filterId;
   tANI_U32   matchCnt;
} tHalRcvFltPktMatchCnt;
typedef PACKED_PRE struct PACKED_POST
{
   /* Success or Failure */
   tANI_U32                 status;
   tANI_U32                 matchCnt;   
   tHalRcvFltPktMatchCnt    filterMatchCnt[HAL_MAX_NUM_FILTERS]; 
} tHalRcvFltPktMatchRspParams, *tptHalRcvFltPktMatchRspParams;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader  header;
   tHalRcvFltPktMatchRspParams fltPktMatchRspParams;
} tHalRcvFltPktMatchCntRspMsg, *tpHalRcvFltPktMatchCntRspMsg;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32   status;  /* only valid for response message */
    tANI_U8    filterId;
}tHalRcvFltPktClearParam, *tpHalRcvFltPktClearParam;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader               header;
    tHalRcvFltPktClearParam     filterClearParam;
} tHalRcvFltPktClearReqMsg, *tpHalRcvFltPktClearReqMsg;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader              header;
   tHalRcvFltPktClearParam    filterClearParam;
} tHalRcvFltPktClearRspMsg, *tpHalRcvFltPktClearRspMsg;

typedef PACKED_PRE struct PACKED_POST
{
    tANI_U32   status;  
}tHalRcvFltPktSetMcListRspType, *tpHalRcvFltPktSetMcListRspType;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader                    header;
    tHalRcvFltMcAddrListType         mcAddrList;
} tHalRcvFltPktSetMcListReqMsg, *tpHalRcvFltPktSetMcListReqMsg;

typedef PACKED_PRE struct PACKED_POST
{
   tHalMsgHeader                    header;
   tHalRcvFltPktSetMcListRspType    rspParam;
} tHalRcvFltPktSetMcListRspMsg, *tpHalRcvFltPktSetMcListRspMsg;


/*---------------------------------------------------------------------------
 *******************Packet Filtering Definitions End*******************
 *--------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST
{
   /*  Ignore DTIM */
  tANI_U32 uIgnoreDTIM;

  /*DTIM Period*/
  tANI_U32 uDTIMPeriod; 

  /* Listen Interval */
  tANI_U32 uListenInterval;

  /* Broadcast Multicast Filter  */
  tANI_U32 uBcastMcastFilter;

  /* Beacon Early Termination */
  tANI_U32 uEnableBET;

  /* Beacon Early Termination Interval */
  tANI_U32 uBETInterval; 
}tSetPowerParamsType, *tpSetPowerParamsType;

typedef PACKED_PRE struct PACKED_POST
{
    tHalMsgHeader               header;
    tSetPowerParamsType         powerParams;
} tSetPowerParamsReqMsg, *tpSetPowerParamsReqMsg;

typedef PACKED_PRE struct PACKED_POST{

   tHalMsgHeader header;

   /*status of the request */
   tANI_U32   status;

}  tSetPowerParamsResp, *tpSetPowerParamsResp;

#if defined(__ANI_COMPILER_PRAGMA_PACK_STACK)
#pragma pack(pop)
#elif defined(__ANI_COMPILER_PRAGMA_PACK)
#else
#endif

#endif /* _WLAN_HAL_MSG_H_ */
