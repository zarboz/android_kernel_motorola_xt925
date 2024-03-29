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
 * This file halHddApi.h contains the prototypes for DVT Apis
 * that are used by Hdd
 *
 * Author:      Sam Hsieh
 * Date:        03/10/2006
 * History:-
 * Date         Modified by    Modification Information
 * --------------------------------------------------------------------
 */

#ifndef _DVTHDDAPI_H_
#define _DVTHDDAPI_H_

#if (WNI_POLARIS_FW_OS == SIR_WINDOWS)
#include <ndis.h>
#endif

typedef struct sAniSirGlobal *tpAniSirGlobal;

#include "sirParams.h"
#include "dvtMsgApi.h"

eANI_DVT_STATUS dvtProcessMsg(tpAniSirGlobal pMac, tDvtMsgbuffer *pDvtMsg);
tSirRetStatus dvtMmhForwardMBmsg(void* pSirGlobal, tSirMbMsg* pMb);

#endif // _DVTHDDAPI_H_
