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
* Module Name:  ans_hw.c                                              *
*                                                                     *
* Abstract: hardware specific files for the 8255x adapters            *
*                                                                     *
* Environment:                                                        *
*                                                                     *
**********************************************************************/


#include "ans_driver.h"






UINT32
bd_ans_hw_AvailableSpeeds(UINT32 phy)
{
    UINT32 speeds;

    /* this one runs on 1 Mbps*/
    if (phy == PHY_82562EH)     
        return IANS_STATUS_LINK_SPEED_1MBPS;

    speeds =
        IANS_STATUS_LINK_SPEED_10MBPS | IANS_STATUS_LINK_SPEED_100MBPS;
    
    
    
    
       
    return (speeds);
    
}



UINT32
bd_ans_hw_flags(BOARD_PRIVATE_STRUCT *bps, UINT16 revid)
{                                        
    UINT16 CompatibilityFlagsReg;
    UINT32 flags = 0;
    
    /* check to see if we are a 559 or greater adapter */
    if (revid >= I82559_REV_ID) {
        /* this is not a 82557 or 82558, so we should keep checking */
        CompatibilityFlagsReg = READ_EEPROM(bps, EEPROM_COMPATIBILITY_FLAGS);
        /* get hw flags from the EEPROM */
        if (!(HIGH_BYTE(CompatibilityFlagsReg) & EEPROM_COMPATIBILITY_BIT_4))
            flags &= !IANS_BD_FLAG4;
        else
            flags |= IANS_BD_FLAG4;
    } 
    else
    {
        flags |= IANS_BD_FLAG4;
    }

    return flags;
}
          
BD_ANS_BOOLEAN
bd_ans_hw_SupportsIEEETagging(BOARD_PRIVATE_STRUCT *bps, UINT16 revid)
{
    /* we only support IEEE tagging if we are on a bachelor or better card. */
    DEBUGLOG1("bd_ans_hw_SupportsIEEETagging: revid = %d\n", revid);

    if (revid >= I82558_REV_ID) {
        return BD_ANS_TRUE;
    }
    return BD_ANS_FALSE;
}

#ifdef IANS_BASE_VLAN_TAGGING
BD_ANS_BOOLEAN
bd_ans_hw_IsQtagPacket(BOARD_PRIVATE_STRUCT *bps, HW_RX_DESCRIPTOR *rxd)
{
    cb_header_status_word *header;

    header = (cb_header_status_word *) &(RXD_STATUS(rxd));
    /* check the T bit */
    if (header->underrun) {
        return BD_ANS_TRUE;
    }
    return BD_ANS_FALSE;
}



BD_ANS_STATUS
bd_ans_hw_InsertQtagHW(BOARD_PRIVATE_STRUCT *bps, HW_TX_DESCRIPTOR *txd, UINT16 *vlanid)
{  

    txd->tcbu.ipcb.ip_activation |= IPCB_INSERT_VLAN_ENABLE;
    txd->tcbu.ipcb.vlan = BYTE_SWAP_WORD(*vlanid);
    return BD_ANS_SUCCESS;
}

UINT16 
bd_ans_hw_GetVlanId(BOARD_PRIVATE_STRUCT *bps,
    HW_RX_DESCRIPTOR *rxd)
{
    UINT16 VlanId = INVALID_VLAN_ID;
    cb_header_status_word *status;


    status = (cb_header_status_word *) &(RXD_STATUS(rxd));

    /* check the T bit */
    if (status->underrun) 
    {
        VlanId = BYTE_SWAP_WORD(RXD_VLANID(rxd)) & VLAN_ID_MASK;
        /* clear out the vlan id bits just to be safe */
        status->underrun = 0;
        RXD_VLANID(rxd) = 0;
    }
    DEBUGLOG1("bd_ans_hw_GetVlanId: got %d\n", VlanId);
    return VlanId;
}     


/* this function will setup the config bytes for the 8255x to enable
 * tagging.  The driver must call the this routine as part of it's normal
 * configure routine if it wants to enable 802.1q tagging.  To disable
 * tagging, the driver should just call configure without calling
 * this routine.
 */
void
bd_ans_hw_ConfigEnableTagging(UINT8 *ConfigBytes, UINT16 rev_id)
{
    DEBUGLOG("bd_ans_hw_ConfigEnableTagging: enter\n");
    /* assumption is that ConfigBytes has already been initialized
     * to default values.
     */
    
    /* first we need to enable reception of larger packets.  This
     * is config byte 18.
     */                                                 
    if (rev_id >= D101B_REV_ID)
        *(ConfigBytes + 18) |= CB_CFIG_LONG_RX_OK;
    
    /* for tagging only mode, (priority only), we would accept packets
     * only on vlanid 0.  Since 8255x does not have a way to filter this
     * in hardware, we need to go ahead and enable tag stripping/insertion
     * anyway, and assume that softwar will deal with filtering the 
     * packet correctly.  We can only enable hw vlan offloading on 82550
     * and above
     */
    if (rev_id >= GAMLA_REV_ID) 
    {
        /* assumption is that the rest of the GAMLA configuration is 
         * handled by the driver for other features and has been setup
         * already.  This includes putting the driver in Gamla receive
         * mode, enabling the extended RFD and TCB structures etc.
         * here we only enable the vlan tag stripping.
         */
        *(ConfigBytes + 22) |= CB_CFIG_VLAN_DROP_ENABLE;
    }
}

#endif

BD_ANS_BOOLEAN
bd_ans_hw_SupportsVlanOffload(BOARD_PRIVATE_STRUCT *bps, UINT16 rev_id)
{
        
    if (rev_id >= GAMLA_REV_ID) {
        DEBUGLOG("bd_ans_hw_SupportsVlanOffload: does support\n");
        return BD_ANS_TRUE;
    }
    DEBUGLOG("bd_ans_hw_SupportsVlanOffload: does NOT support\n");
    return BD_ANS_FALSE;
}

