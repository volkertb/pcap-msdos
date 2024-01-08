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
* Module Name:  ans_hw.h                                              *
*                                                                     *
* Abstract: these are the hardware specific (not OS specific)         *
*       routines needed by the bd_ans module                          *
*                                                                     *
* Environment:                                                        *
*                                                                     *
**********************************************************************/

#define BASE_10T        1
#define BASE_100T4      2
#define BASE_100TX      4
#define BASE_100FX      8

#define I82559_REV_ID                    8
#define I82558_REV_ID                                    4
#define EEPROM_COMPATIBILITY_FLAGS      3
#define EEPROM_COMPATIBILITY_BIT_4          0x0004
#define IPCB_INSERT_VLAN_ENABLE         0x0020
#define CB_CFIG_LONG_RX_OK              0x0008
//#define CB_CFIG_VLAN_DROP_ENABLE        0x0002
#define GAMLA_REV_ID                    12
#define D101B_REV_ID                    5

/* bit definitions of 8255x header */
typedef struct _cb_header_status_word {
    volatile unsigned status:12;
    volatile unsigned underrun:1;
    volatile unsigned ok:1;
    volatile unsigned pad:1;
    volatile unsigned c:1;
} cb_header_status_word;

/* definition of an IPCB - only the parts that I care about */
typedef struct _ipcb_bits {
    volatile unsigned ipcb_scheduling:20;
    volatile unsigned ipcb_activation:12;
    volatile UINT16 vlanid;
    volatile UINT8 ip_header_offset;
    volatile UINT8 tcp_header_offset;
} ipcb_bits;

#define HIGH_BYTE(word) ((UINT8)(word >> 8))
#define BD_ANS_HW_FLAGS(bps) \
            bd_ans_hw_flags(bps, BD_ANS_DRV_REVID(bps));
#define BD_ANS_HW_AVAILABLE_SPEEDS(bps) bd_ans_hw_AvailableSpeeds(BD_ANS_DRV_PHY_ID(bps))



/* function prototypes */
extern UINT32 bd_ans_hw_AvailableSpeeds(UINT32 phy);
extern UINT32 bd_ans_hw_flags(BOARD_PRIVATE_STRUCT *bps, UINT16 revid);
void bd_ans_hw_ConfigEnableTagging(UINT8 *ConfigBytes, UINT16 rev_id);
