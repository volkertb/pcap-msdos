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
* Module Name:  ans_driver.c                                          *
*                                                                     *
* Abstract: Driver specific routines                                  *
*                                                                     *
* Environment:  This file is intended to be specific to the Linux     *
*               operating system.                                     *
*                                                                     *
**********************************************************************/

#include "ans_driver.h"



/* bd_ans_drv_InitANS()
**
**  This function should be called at driver Init time to set the pointers
**  in the iANSsupport_t structure to the driver's current pointers.
**
**  Arguments:  BOARD_PRIVATE_STRUCT *bdp - private data struct
**              iANSsupport_t *iANSdata - iANS support structure.
**
**  Returns:  void
**
*/
void
bd_ans_drv_InitANS(BOARD_PRIVATE_STRUCT *bps, 
    iANSsupport_t *iANSdata)
{
    bd_ans_Init(iANSdata);
    
    /* set all the required status fields to this driver's 
     * status fields. */
    iANSdata->link_status = &(((bd_config_t *)bps)->bddp->cur_link_status);
    iANSdata->line_speed  = &(((bd_config_t *)bps)->bddp->ans_line_speed);
    iANSdata->duplex      = &(((bd_config_t *)bps)->bddp->ans_dplx_mode);
    iANSdata->hw_fail   = NULL;
    iANSdata->suspended = NULL;
    iANSdata->in_reset  = NULL;
}                            



/* bd_ans_drv_UpdateStatus()
**
**  This function should update the driver board status in the iANSsupport
**  structure for this adapter
**
**  Arguments: BOARD_PRIVATE_STRUCT *bps - board private structure
**
**  Returns:  void
*/
void
bd_ans_drv_UpdateStatus(BOARD_PRIVATE_STRUCT *bps)
{
    //DEBUGLOG("bd_ans_drv_UpdateStatus: enter\n");

    /* update the driver's current status if needed.  You may
     * not need to do anything here if your status fields 
     * are updated before you call the ans Watchdog routine.
     * the key is to make sure that all the fields you set in
     * InitANS have the correct value in them, because these 
     * values will be used now to determine if there has been
     * a status change.
     */
    /* this is done to convert to 32 bit values */
    ((bd_config_t *)bps)->bddp->ans_line_speed =
        ((bd_config_t *)bps)->bddp->cur_line_speed;
    ((bd_config_t *)bps)->bddp->ans_dplx_mode = 
        ((bd_config_t *)bps)->bddp->cur_dplx_mode;
    return;
}



/* bd_ans_drv_ConfigureTagging()
**
**  This function will call the HW specific functions to configure
**  the adapter to operate in tagging mode.  This function can also
**  be called to disable tagging support.  
**
**  Arguments:  BOARD_PRIVATE_STRUCT *bps - the driver's private data struct
**
**  Returns:    BD_ANS_STATUS - BD_ANS_SUCCESS if the adapter was configured
**                              BD_ANS_FAILURE if the adapter was not  
*/
BD_ANS_STATUS 
bd_ans_drv_ConfigureTagging(BOARD_PRIVATE_STRUCT *bdp)
{

    
    /* no hw action should ne taken to turn general tagging on/off */
    return BD_ANS_SUCCESS;
    
}


/* bd_ans_drv_ConfigureVlanTable()
**
**  This function will call the HW specific functions to configure the
**  adapter to do vlan filtering in hardware.  This function call also
**  be called to disable vlan filtering support
**
**  Arguments:  BOARD_PRIVATE_STRUCT *bps - the driver's private data struct
**                 
**  Returns:    BD_ANS_STATUS - BD_ANS_SUCCESS if the adapter was configured
**                              BD_ANS_FAILURE otherwise
*/ 
BD_ANS_STATUS
bd_ans_drv_ConfigureVlanTable(BOARD_PRIVATE_STRUCT *bps)
{
    
    /* this feature is unsupported by 8255x Intel NICs */
    return BD_ANS_FAILURE;
}


/* bd_ans_drv_ConfigureVlan()
**
**  This function will call the HW specific functions to configure the
**  adapter to operate in vlan mode. This function can also be called
**  to disable vlan mode.
**
**  Arguments:  BOARD_PRIVATE_STRUCT *bps - the driver's private data struct
**                 
**  Returns:    BD_ANS_STATUS - BD_ANS_SUCCESS if the adapter was configured
**                              BD_ANS_FAILURE otherwise
*/ 
BD_ANS_STATUS
bd_ans_drv_ConfigureVlan(BOARD_PRIVATE_STRUCT *bps)
{
    /* this function should call the hardware specific routines
     * to configure the adapter in vlan mode (bd_ans_hw_EnableVlan)
     * or bd_ans_hw_DisableTagging depending on how the vlan_mode
     * and tag_mode flags are set.  The driver should not modify
     * the flag
     */

    return e100_configure(bps) ? BD_ANS_SUCCESS : BD_ANS_FAILURE;
}


/* bd_ans_drv_StopWatchdog()
**
**  Since the linux driver already has a watchdog routine, we just need to
**  set a flag to change the code path in the watchdog routine to not call
**  the bd_ans_os_Watchdog() procedure.
**
**  Arguments:  BOARD_PRIVATE_STRUCT *bps - adapter private data
**
**  Returns:  void
*/
VOID
bd_ans_drv_StopWatchdog(BOARD_PRIVATE_STRUCT *bps)
{
    /* set a flag to indicate that we no longer need to call
    ** the bd_ans_os_Watchdog routine.  Do anything else you feel
    ** like doing here.
    */
    ANS_PRIVATE_DATA_FIELD(bps)->reporting_mode = IANS_STATUS_REPORTING_OFF;
}


/* bd_ans_drv_StopPromiscuousMode()
**
**  The linux driver does not support this.
*/
BD_ANS_STATUS
bd_ans_drv_StopPromiscuousMode(BOARD_PRIVATE_STRUCT *bps)
{
    return BD_ANS_FAILURE;
}


/* bd_ans_drv_StartWatchdog()
**
**  Since the linux driver already has a watchdog routine started,
**  we just need to set a flag to change the code path to call the
**  bd_ans_os_Watchdog routine from the current watchdog routine.
**
**  Arguments:  BOARD_PRIVATE_STRUCT *bps - private data structure.
** 
**  Returns:  UINT32 - non-zero indicates success.
*/
UINT32 
bd_ans_drv_StartWatchdog(BOARD_PRIVATE_STRUCT *bps)
{
    /* set your flag to indicate that the watchdog routine should
    ** call ans_bd_os_Watchdog(). Do whatever else you need to do here 
    ** if you already have a watchdog routine, there probably isn't any
    ** thing left to do except leave 
    */
    ANS_PRIVATE_DATA_FIELD(bps)->reporting_mode = IANS_STATUS_REPORTING_ON;
    
    /* return a non-zero value */
    return 1;
}




