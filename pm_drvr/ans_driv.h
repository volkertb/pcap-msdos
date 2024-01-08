/*****************************************************************************
 *****************************************************************************
 Copyright (c) 1999-2001, Intel Corporation 

 All rights reserved.

 Redistribution and use in source and binary forms, with or without 
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, 
 this list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation 
 and/or other materials provided with the distribution.

 3. Neither the name of Intel Corporation nor the names of its contributors 
 may be used to endorse or promote products derived from this software 
 without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
 LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

 *****************************************************************************
****************************************************************************/

/**********************************************************************
*                                                                     *
* INTEL CORPORATION                                                   *
*                                                                     *
* This software is supplied under the terms of the license included   *
* above.  All use of this driver must be in accordance with the terms *
* of that license.                                                    *
*                                                                     *
* Module Name:  ans_driver.h                                          *
*                                                                     *
* Abstract: driver defines specific to the linux HVA driver           *
*                                                                     *
* Environment:  This file is intended to be specific to the Linux     *
*               operating system.                                     *
*                                                                     *
**********************************************************************/

#ifndef _ANS_DRIVER_H
#define _ANS_DRIVER_H

/* Forward declerations */
#ifndef _RFD_T_
#define _RFD_T_
typedef struct _rfd_t rfd_t, *prfd_t;
#endif

#ifndef _TCB_T_
#define _TCB_T_
typedef struct _tcb_t tcb_t, *ptcb_t;
#endif

#ifndef _BD_CONFIG_T_
#define _BD_CONFIG_T_
typedef struct bdconfig bd_config_t;
#endif


#define BOARD_PRIVATE_STRUCT void 

/* hardware specfic defines */
/* you may need to include your driver's header file which
** defines your descriptors right here 
*/

#define HW_RX_DESCRIPTOR rfd_t
#define HW_TX_DESCRIPTOR tcb_t
#define FRAME_DATA unsigned char


/* you must include this after you define above stuff */
#include "ans.h"

/* e100.h has to come after ans.h is included */
#include "e100.h"


#define ANS_PRIVATE_DATA_FIELD(bps) (((bd_config_t *)(bps))->iANSdata)
#define DRIVER_DEV_FIELD(bps) (((bd_config_t *)(bps))->device)

/* how we will be defining the duplex field values for this driver */
#define BD_ANS_DUPLEX_FULL              2
#define BD_ANS_DUPLEX_HALF              1

/* macros for accessing some driver structures */
#define BD_ANS_DRV_PHY_ID(bps) (((bd_config_t *)(bps))->bddp->PhyId) 
#define BD_ANS_DRV_REVID(bps) (((bd_config_t *)(bps))->bddp->rev_id)
#define BD_ANS_DRV_SUBSYSTEMID(bps) (((bd_config_t *)(bps))->bddp->sub_dev_id)
#define BD_ANS_DRV_SUBVENDORID(bps) (((bd_config_t *)(bps))->bddp->sub_dev_id)
#define RXD_T_BIT(rxd) \
        (((cb_header_status_word)((rxd)->rfd_header.cb_status)).underrun)
#define RXD_VLANID(rxd) ((rxd)->vlanid)
#define IPCB_IP_ACTIVATION(txd) \
        (((ipcb_bits)((txd)->tcbu.tcb_ext)).ipcb_activation)
#define IPCB_VLANID(txd) \
        (((ipcb_bits)((txd)->tcbu.tcb_ext)).vlanid)
#define RXD_STATUS(rxd) ((rxd)->rfd_header.cb_status)
#define EXT_TCB_START(txd) ((txd)->tcbu.tcb_ext)
#define READ_EEPROM(bps, reg) e100_ReadEEprom(bps, reg)

#define BD_ANS_DRV_STATUS_SUPPORT_FLAGS (BD_ANS_LINK_STATUS_SUPPORTED | BD_ANS_SPEED_STATUS_SUPPORTED |BD_ANS_DUPLEX_STATUS_SUPPORTED )
#define BD_ANS_DRV_MAX_VLAN_ID(bps) 4096 
#define BD_ANS_DRV_MAX_VLAN_TABLE_SIZE(bps) 0
#define BD_ANS_DRV_ISL_TAG_SUPPORT(bps) BD_ANS_FALSE
#define BD_ANS_DRV_IEEE_TAG_SUPPORT(bps) \
        ((BD_ANS_DRV_REVID(bps) >= I82558_REV_ID)?BD_ANS_TRUE:BD_ANS_FALSE)

#define BD_ANS_DRV_VLAN_SUPPORT(bps) (BD_ANS_DRV_IEEE_TAG_SUPPORT(bps) | BD_ANS_DRV_ISL_TAG_SUPPORT(bps))
#define BD_ANS_DRV_VLAN_FILTER_SUPPORT(bps) BD_ANS_FALSE

#define BD_ANS_DRV_VLAN_OFFLOAD_SUPPORT(bps) \
        (bd_ans_hw_SupportsVlanOffload(bps, BD_ANS_DRV_REVID(bps)))
#ifndef MAX_ETHERNET_PACKET_SIZE
#define MAX_ETHERNET_PACKET_SIZE 1514
#endif

#ifndef BYTE_SWAP_WORD
#define BYTE_SWAP_WORD(word) ((((word) & 0x00ff) << 8) \
                            | (((word) & 0xff00) >> 8))
#endif

/* function prototypes */
extern void bd_ans_drv_InitANS(BOARD_PRIVATE_STRUCT *bps, iANSsupport_t *iANSdata);
extern void bd_ans_drv_UpdateStatus(BOARD_PRIVATE_STRUCT *bps);
extern BD_ANS_STATUS bd_ans_drv_ConfigureTagging(BOARD_PRIVATE_STRUCT *bdp);
extern BD_ANS_STATUS bd_ans_drv_ConfigureVlanTable(BOARD_PRIVATE_STRUCT *bps);
extern BD_ANS_STATUS bd_ans_drv_ConfigureVlan(BOARD_PRIVATE_STRUCT *bps);
extern VOID bd_ans_drv_StopWatchdog(BOARD_PRIVATE_STRUCT *bps);
extern BD_ANS_STATUS bd_ans_drv_StopPromiscuousMode(BOARD_PRIVATE_STRUCT *bps);
extern UINT32 bd_ans_drv_StartWatchdog(BOARD_PRIVATE_STRUCT *bps);

/* you may want to include some other driver include file here, if it will be
 * needed by any of the other ans modules.  The ans_driver.h is included by
 * all the .c files, so if you want this include in all the ans .c files, 
 * stick it right here.
 */

#endif
