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

#include "btampHCI.h"
#include "btampFsm.h"

/////////////////////////////////////////////////////////////////////////////
//                                                                         //
//                        HCI Command Opcode type values                   //
//                                                                         //
//   Note: These are constants which are TWO BYTE (16 bit) values          //
//   These values consist of:                                              //
//    1. A 10 bit Opcode Command Field (OCF) and                           //
//    2. A 6 bit Opcode Group Field (OGF)                                  //
//                                                                         //
/////////////////////////////////////////////////////////////////////////////



/** BT v3.0 Link Control commands */

#define WLANBAP_HCI_CREATE_PHYSICAL_LINK_CMD BTAMP_TLV_HCI_CREATE_PHYSICAL_LINK_CMD //( 1077 )
#define WLANBAP_HCI_ACCEPT_PHYSICAL_LINK_CMD  BTAMP_TLV_HCI_ACCEPT_PHYSICAL_LINK_CMD //( 1078 )
#define WLANBAP_HCI_DISCONNECT_PHYSICAL_LINK_CMD  BTAMP_TLV_HCI_DISCONNECT_PHYSICAL_LINK_CMD //( 1079 )
#define WLANBAP_HCI_CREATE_LOGICAL_LINK_CMD  BTAMP_TLV_HCI_CREATE_LOGICAL_LINK_CMD //( 1080 )
#define WLANBAP_HCI_ACCEPT_LOGICAL_LINK_CMD  BTAMP_TLV_HCI_ACCEPT_LOGICAL_LINK_CMD //( 1081 )
#define WLANBAP_HCI_DISCONNECT_LOGICAL_LINK_CMD  BTAMP_TLV_HCI_DISCONNECT_LOGICAL_LINK_CMD //( 1082 )
#define WLANBAP_HCI_LOGICAL_LINK_CANCEL_CMD  BTAMP_TLV_HCI_LOGICAL_LINK_CANCEL_CMD //( 1083 )
#define WLANBAP_HCI_FLOW_SPEC_MODIFY_CMD  BTAMP_TLV_HCI_FLOW_SPEC_MODIFY_CMD //( 1084 )

/*
Host Controller and Baseband Commands
*/
#define WLANBAP_HCI_RESET_CMD  BTAMP_TLV_HCI_RESET_CMD //( 3075 )
#define WLANBAP_HCI_SET_EVENT_MASK_CMD  BTAMP_TLV_HCI_SET_EVENT_MASK_CMD //( 3077 )
#define WLANBAP_HCI_FLUSH_CMD  BTAMP_TLV_HCI_FLUSH_CMD //( 3080 )
#define WLANBAP_HCI_READ_CONNECTION_ACCEPT_TIMEOUT_CMD  BTAMP_TLV_HCI_READ_CONNECTION_ACCEPT_TIMEOUT_CMD //( 3093 )
#define WLANBAP_HCI_WRITE_CONNECTION_ACCEPT_TIMEOUT_CMD  BTAMP_TLV_HCI_WRITE_CONNECTION_ACCEPT_TIMEOUT_CMD //( 3094 )
#define WLANBAP_HCI_READ_LINK_SUPERVISION_TIMEOUT_CMD  BTAMP_TLV_HCI_READ_LINK_SUPERVISION_TIMEOUT_CMD //( 3126 )
#define WLANBAP_HCI_WRITE_LINK_SUPERVISION_TIMEOUT_CMD  BTAMP_TLV_HCI_WRITE_LINK_SUPERVISION_TIMEOUT_CMD //( 3127 )
/* v3.0 Host Controller and Baseband Commands */
#define WLANBAP_HCI_READ_LOGICAL_LINK_ACCEPT_TIMEOUT_CMD  BTAMP_TLV_HCI_READ_LOGICAL_LINK_ACCEPT_TIMEOUT_CMD //( 3169 )
#define WLANBAP_HCI_WRITE_LOGICAL_LINK_ACCEPT_TIMEOUT_CMD  BTAMP_TLV_HCI_WRITE_LOGICAL_LINK_ACCEPT_TIMEOUT_CMD //( 3170 )
#define WLANBAP_HCI_SET_EVENT_MASK_PAGE_2_CMD  BTAMP_TLV_HCI_SET_EVENT_MASK_PAGE_2_CMD //( 3171 )
#define WLANBAP_HCI_READ_LOCATION_DATA_CMD  BTAMP_TLV_HCI_READ_LOCATION_DATA_CMD //( 3172 )
#define WLANBAP_HCI_WRITE_LOCATION_DATA_CMD  BTAMP_TLV_HCI_WRITE_LOCATION_DATA_CMD //( 3173 )
#define WLANBAP_HCI_READ_FLOW_CONTROL_MODE_CMD  BTAMP_TLV_HCI_READ_FLOW_CONTROL_MODE_CMD //( 3174 )
#define WLANBAP_HCI_WRITE_FLOW_CONTROL_MODE_CMD  BTAMP_TLV_HCI_WRITE_FLOW_CONTROL_MODE_CMD //( 3175 )
#define WLANBAP_HCI_READ_BEST_EFFORT_FLUSH_TIMEOUT_CMD  BTAMP_TLV_HCI_READ_BEST_EFFORT_FLUSH_TIMEOUT_CMD //( 3177 )
#define WLANBAP_HCI_WRITE_BEST_EFFORT_FLUSH_TIMEOUT_CMD  BTAMP_TLV_HCI_WRITE_BEST_EFFORT_FLUSH_TIMEOUT_CMD //( 3178 )
/** opcode definition for this command from AMP HCI CR D9r4 markup */
#define WLANBAP_HCI_SET_SHORT_RANGE_MODE_CMD  BTAMP_TLV_HCI_SET_SHORT_RANGE_MODE_CMD //( 3179 )
/* End of v3.0 Host Controller and Baseband Commands */
/*
Informational Parameters
*/
#define WLANBAP_HCI_READ_LOCAL_VERSION_INFO_CMD  BTAMP_TLV_HCI_READ_LOCAL_VERSION_INFO_CMD //( 4097 )
#define WLANBAP_HCI_READ_LOCAL_SUPPORTED_CMDS_CMD  BTAMP_TLV_HCI_READ_LOCAL_SUPPORTED_CMDS_CMD //( 4098 )
#define WLANBAP_HCI_READ_BUFFER_SIZE_CMD  BTAMP_TLV_HCI_READ_BUFFER_SIZE_CMD //( 4101 )
/* v3.0 Informational commands */
#define WLANBAP_HCI_READ_DATA_BLOCK_SIZE_CMD  BTAMP_TLV_HCI_READ_DATA_BLOCK_SIZE_CMD //( 4106 )

/*
Status Parameters
*/
#define WLANBAP_HCI_READ_FAILED_CONTACT_COUNTER_CMD  BTAMP_TLV_HCI_READ_FAILED_CONTACT_COUNTER_CMD //( 5121 )
#define WLANBAP_HCI_RESET_FAILED_CONTACT_COUNTER_CMD  BTAMP_TLV_HCI_RESET_FAILED_CONTACT_COUNTER_CMD //( 5122 )
#define WLANBAP_HCI_READ_LINK_QUALITY_CMD  BTAMP_TLV_HCI_READ_LINK_QUALITY_CMD //( 5123 )
#define WLANBAP_HCI_READ_RSSI_CMD  BTAMP_TLV_HCI_READ_RSSI_CMD //( 5125 )
#define WLANBAP_HCI_READ_LOCAL_AMP_INFORMATION_CMD  BTAMP_TLV_HCI_READ_LOCAL_AMP_INFORMATION_CMD //( 5129 )
#define WLANBAP_HCI_READ_LOCAL_AMP_ASSOC_CMD  BTAMP_TLV_HCI_READ_LOCAL_AMP_ASSOC_CMD //( 5130 )
#define WLANBAP_HCI_WRITE_REMOTE_AMP_ASSOC_CMD  BTAMP_TLV_HCI_WRITE_REMOTE_AMP_ASSOC_CMD //( 5131 )

/*
Debug Commands
*/
#define WLANBAP_HCI_READ_LOOPBACK_MODE_CMD  BTAMP_TLV_HCI_READ_LOOPBACK_MODE_CMD //( 6145 )
#define WLANBAP_HCI_WRITE_LOOPBACK_MODE_CMD  BTAMP_TLV_HCI_WRITE_LOOPBACK_MODE_CMD //( 6146 )

/////////////////////////////////////////////////////////////////////////////
//                                                                         //
//                        HCI Event type values                            //
//                                                                         //
/////////////////////////////////////////////////////////////////////////////


#define WLANBAP_HCI_COMMAND_COMPLETE_EVENT BTAMP_TLV_HCI_COMMAND_COMPLETE_EVENT //( 14 )
#define WLANBAP_HCI_COMMAND_STATUS_EVENT BTAMP_TLV_HCI_COMMAND_STATUS_EVENT //( 15 )
#define WLANBAP_HCI_HARDWARE_ERROR_EVENT BTAMP_TLV_HCI_HARDWARE_ERROR_EVENT //( 16 )
#define WLANBAP_HCI_FLUSH_OCCURRED_EVENT BTAMP_TLV_HCI_FLUSH_OCCURRED_EVENT //( 17 )
#define WLANBAP_HCI_NUM_OF_COMPLETED_PKTS_EVENT BTAMP_TLV_HCI_NUM_OF_COMPLETED_PKTS_EVENT //( 19 )
#define WLANBAP_HCI_LOOPBACK_COMMAND_EVENT BTAMP_TLV_HCI_LOOPBACK_COMMAND_EVENT //( 25 )
#define WLANBAP_HCI_DATA_BUFFER_OVERFLOW_EVENT BTAMP_TLV_HCI_DATA_BUFFER_OVERFLOW_EVENT //( 26 )
#define WLANBAP_HCI_QOS_VIOLATION_EVENT BTAMP_TLV_HCI_QOS_VIOLATION_EVENT //( 30 )
/** BT v3.0 events */
#define WLANBAP_HCI_GENERIC_AMP_LINK_KEY_NOTIFICATION_EVENT BTAMP_TLV_HCI_GENERIC_AMP_LINK_KEY_NOTIFICATION_EVENT //( 62 )
#define WLANBAP_HCI_PHYSICAL_LINK_COMPLETE_EVENT BTAMP_TLV_HCI_PHYSICAL_LINK_COMPLETE_EVENT //( 64 )
#define WLANBAP_HCI_CHANNEL_SELECTED_EVENT BTAMP_TLV_HCI_CHANNEL_SELECTED_EVENT //( 65 )
#define WLANBAP_HCI_DISCONNECT_PHYSICAL_LINK_COMPLETE_EVENT BTAMP_TLV_HCI_DISCONNECT_PHYSICAL_LINK_COMPLETE_EVENT //( 66 )
#define WLANBAP_HCI_PHYSICAL_LINK_LOSS_WARNING_EVENT BTAMP_TLV_HCI_PHYSICAL_LINK_LOSS_WARNING_EVENT //( 67 )
#define WLANBAP_HCI_PHYSICAL_LINK_RECOVERY_EVENT BTAMP_TLV_HCI_PHYSICAL_LINK_RECOVERY_EVENT //( 68 )
#define WLANBAP_HCI_LOGICAL_LINK_COMPLETE_EVENT BTAMP_TLV_HCI_LOGICAL_LINK_COMPLETE_EVENT //( 69 )
#define WLANBAP_HCI_DISCONNECT_LOGICAL_LINK_COMPLETE_EVENT BTAMP_TLV_HCI_DISCONNECT_LOGICAL_LINK_COMPLETE_EVENT //( 70 ) 
#define WLANBAP_HCI_FLOW_SPEC_MODIFY_COMPLETE_EVENT BTAMP_TLV_HCI_FLOW_SPEC_MODIFY_COMPLETE_EVENT //( 71 )
#define WLANBAP_HCI_NUM_OF_COMPLETED_DATA_BLOCKS_EVENT BTAMP_TLV_HCI_NUM_OF_COMPLETED_DATA_BLOCKS_EVENT //( 72 )
#define WLANBAP_HCI_SHORT_RANGE_MODE_CHANGE_COMPLETE_EVENT BTAMP_TLV_HCI_SHORT_RANGE_MODE_CHANGE_COMPLETE_EVENT //( 76 )

/////////////////////////////////////////////////////////////////////////////
//                                                                         //
//                        BT-AMP Link establishment states                 //
//                                                                         //
/////////////////////////////////////////////////////////////////////////////

#define WLANBAP_AUTHENTICATING AUTHENTICATING //( 0 )
#define WLANBAP_DISCONNECTED DISCONNECTED //( 1 )
#define WLANBAP_CONNECTING CONNECTING //( 2 )
#define WLANBAP_DISCONNECTING DISCONNECTING //( 3 )
#define WLANBAP_SCANNING SCANNING //( 4 )
#define WLANBAP_CONNECTED CONNECTED //( 5 )
#define WLANBAP_S1 S1 //( 6 )
#define WLANBAP_KEYING KEYING //( 7 )
#define WLANBAP_VALIDATED VALIDATED //( 8 )
#define WLANBAP_STARTING STARTING //( 9 )

