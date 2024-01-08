/*
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

*/

/**********************************************************************
*                                                                     *
* INTEL CORPORATION                                                   *
*                                                                     *
* This software is supplied under the terms of the license included   *
* above.  All use of this driver must be in accordance with the terms *
* of that license.                                                    *
*                                                                     *
* Module Name:  e100.c                                                *
*                                                                     *
* Abstract:     Functions for the driver entry points like load,      *
*               unload, open and close. All board specific calls made *
*               by the network interface section of the driver.       *
*                                                                     *
* Environment:  This file is intended to be specific to the Linux     *
*               operating system.                                     *
*                                                                     *
**********************************************************************/


/* set the debug level */
#define DEBUG_STATS     0
#define DEBUG           0
#define DEBUG_TX        0
#define DEBUG_RX        0
#define DEBUG_LINK      0
#define DEBUG_SKB       0

#define COND_FREE(X)            if (X) k_free(X)
#define ALLOC_SKBS(bddp, nskb)  for(;(bddp)->skb_req>0; (bddp)->skb_req--) { \
                                  (nskb) = e100_alloc_skb(bddp);\
                                  if ((nskb) == NULL) \
                                     break;\
                                  e100_add_skb_to_end( (bddp), (nskb));\
                                }

#define _IANS_MAIN_MODULE_C_

#define VLAN_SIZE      40
#define CSUM_559_SIZE  2

#undef __NO_VERSION__
#include "pmdrvr.h"
#include "pmdrvr.h"
#include "bios32.h"
#include "pci.h"
#include "e100.h"

int e100_debug = 0;


/****************************************************************************
 * Parameter checking for compile time variables
 */
char *e100_ifname = IFNAME;
int   e100_adaptive_ifs = ADAPTIVE_IFS;
BYTE  e100_cfg_parm6 = CFG_BYTE_PARM6;

BYTE  e100_rx_fifo_lmt = (RX_FIFO_LMT < 0) ? DEFAULT_RX_FIFO_LIMIT :
                          ((RX_FIFO_LMT > 15) ? DEFAULT_RX_FIFO_LIMIT :
                          RX_FIFO_LMT);

BYTE  e100_tx_fifo_lmt = (TX_FIFO_LMT < 0) ? DEFAULT_TX_FIFO_LIMIT :
                          ((TX_FIFO_LMT > 7) ? DEFAULT_TX_FIFO_LIMIT :
                          TX_FIFO_LMT);

BYTE  e100_rx_dma_cnt  = (RX_DMA_CNT < 0) ? CB_557_CFIG_DEFAULT_PARM4 :
                          ((RX_DMA_CNT > 63) ? CB_557_CFIG_DEFAULT_PARM4 :
                          RX_DMA_CNT);

BYTE  e100_tx_dma_cnt  = (TX_DMA_CNT < 0) ? CB_557_CFIG_DEFAULT_PARM5 :
                          ((TX_DMA_CNT > 63) ? CB_557_CFIG_DEFAULT_PARM5 :
                          TX_DMA_CNT);

BYTE  e100_urun_retry  = (TX_UR_RETRY < 0) ? CB_557_CFIG_DEFAULT_PARM5 :
                          ((TX_UR_RETRY > 3) ? CB_557_CFIG_DEFAULT_PARM5 :
                          TX_UR_RETRY);

BYTE  e100_tx_thld     = (TX_THRSHLD < 0) ? DEFAULT_TRANSMIT_THRESHOLD :
                          ((TX_THRSHLD > 200) ? DEFAULT_TRANSMIT_THRESHOLD :
                          TX_THRSHLD);

/* Microcode tunable for number of receive interrupts
 */
WORD e100_cpu_saver = (CPU_CYCLE_SAVER < 0) ? 0x0600 :
                       ((CPU_CYCLE_SAVER > 0xc000) ? 0x0600 :
                       CPU_CYCLE_SAVER);

/* Microcode tunable for maximum number of frames that will be bundled
 */
WORD e100_cpusaver_bundle_max = (CPUSAVER_BUNDLE_MAX < 1) ? 0x0006 :
                                 ((CPUSAVER_BUNDLE_MAX > 0xffff) ? 0x0006 :
                                 CPUSAVER_BUNDLE_MAX);

/* Congestion enable flag for National Phy
 */
DWORD e100_cong_enbl = TX_CONG_DFLT;

/* 
 * Auto-polarity enable/disable
 * e100_autopolarity = 0 => disable auto-polarity
 * e100_autopolarity = 1 => enable auto-polarity
 * e100_autopolarity = 2 => let software determine
 */
int e100_autopolarity = 2;

/* 
 * Number of consecutive transmit frames without the CU getting suspended or
 * going idle before generating an interrupt
 */
DWORD e100_batch_tx_frames = TX_FRAME_CNT;

/* 
 * ################### 82558 and 82559 feature set ##########################
 *
 * These should be enabled only for 82558 or later based (PRO/100+) boards .
 * These should be changed only by someone with an intimate knowledge
 * of the hardware of the system that the driver is loaded.
 */

/* e100_enhanced_tx_enable
 * 
 * Enable/Disable 82558 enhanced transmit features.
 * 0 - Disable
 * 1 - Enable
 */
int e100_enhanced_tx_enable = 1;

/* e100_MWI_enable
 * 
 * Enable/Disable use of Memory Write and Invalidate
 * 0 - Disable
 * 1 - Enable
 *
 * Note: This should be enabled only for an 82558 based adapter (PRO/100+)
 * and only on systems in which the PCI bus supports MWI. If enabled on a 
 * system that does not support MWI, performance might be affected.
 */
int e100_MWI_enable = 1;

/* 
 * e100_read_align_enable
 * 
 * Enable/Disable use of Cache Line Read/Write Termination
 * 0 - Disable
 * 1 - Enable
 *
 * Note: This should be enabled only for an 82558 based adapter (PRO/100+)
 * and only on cache line oriented systems. If enabled on a
 * system that is not cache line oriented, performance might be affected.
 */
int e100_read_align_enable = 0;

/* 
 * e100_flow_control_enable
 * 
 * Enable/Disable Full Duplex Flow Control
 * 0 - Disable
 * 1 - Enable
 *
 * Note: This should be enabled only for an 82558 based adapter (PRO/100+)
 * and only if the Link Partner (hub/switch) supports Full Duplex Flow
 * Control.
 */
int e100_flow_control_enable = 0;

/* 
 * e100_cna_backoff_enable
 * 
 * Enable/Disable delaying of CNA (Control Unit Not Active) Interrupts
 * 0 - Disable
 * 1 - Enable
 *
 * Note: This should be enabled only for an 82558 based adapter (PRO/100+).
 * If enabled, then the value of e100_current_CNA_backoff should be set to 
 * a value between 0 and 31.
 */
int e100_cna_backoff_enable = 0;

/* e100_current_cna_backoff
 * 
 * Delay in assertion of CNA (Control Unit Not Active) Interrupts
 * 
 * Permitted values - 0 through 31
 *
 * Note: This should be enabled only for an 82558 based adapter (PRO/100+).
 * Larger values increase the delay in assertion of CNA interrupts and
 * possibly improve performance. 
 * If this is set to any value other than 0, make sure that 
 * e100_cna_backoff_enable is set to 1. 
 */
int e100_current_cna_backoff = 0; /* 0-31 only */

/* e100_max_rx
 * number of packets that are recieved in single isr
 */
int e100_max_rx = 24;

/* 
 * ################## End of 82558 and 82559  feature set ###################
 */

/* Global Data structures and variables
 */
char e100id_string[128] = "e100 - Intel(R) PRO/100 Fast Ethernet Adapter, ";
char e100_copyright[] = "Copyright (c) 2001 Intel Corporation";

static const char *e100_version = "1.4.21";
static const char *e100_full_driver_name = "Intel(R) PRO/100 Fast Ethernet Adapter - driver, ver 1.4.21";
static const char *e100_short_driver_name = "e100";


/* PCI Vendor ID,PCI Device ID,PCI Sub Vendor ID,PCI Sub system ID,PCI Rev,Branding String, */
e100_vendor_info_t e100_vendor_info_array[] = {
{0x8086, 0x1229, 0x8086, 0x0001, 1,   "Intel(R) PRO/100B PCI Adapter (TX)"},
{0x8086, 0x1229, 0x8086, 0x0002, 1,   "Intel(R) PRO/100B PCI Adapter (T4)"},
{0x8086, 0x1229, 0x8086, 0x0003, 1,   "Intel(R) PRO/10+ PCI Adapter"},
{0x8086, 0x1229, 0x8086, 0x0004, 1,   "Intel(R) PRO/100 WfM PCI Adapter"},
{0x8086, 0x1229, 0x8086, 0x0005, 1,   "Intel 82557-based Integrated Ethernet PCI (10/100)"},
{0x8086, 0x1229, 0x8086, 0x0006, 1,   "Intel 82557-based Integrated Ethernet with Wake on LAN*"},
{0x8086, 0x1229, 0x8086, 0x0002, 2,   "Intel(R) PRO/100B PCI Adapter (T4)"},
{0x8086, 0x1229, 0x8086, 0x0003, 2,   "Intel(R) PRO/10+ PCI Adapter"},
{0x8086, 0x1229, 0x8086, 0x0004, 2,   "Intel(R) PRO/100 WfM PCI Adapter"},
{0x8086, 0x1229, 0x8086, 0x0005, 2,   "Intel 82557-based Integrated Ethernet PCI (10/100)"},
{0x8086, 0x1229, 0x8086, 0x0006, 2,   "Intel 82557-based Integrated Ethernet with Wake on LAN*"},
{0x8086, 0x1229, 0x8086, 0x0007, 4,   "Intel 82558-based Integrated Ethernet"},
{0x8086, 0x1229, 0x8086, 0x0008, 4,   "Intel 82558-based Integrated Ethernet with Wake on LAN*"},
{0x8086, 0x1229, 0x8086, 0x0009, 5,   "Intel(R) PRO/100+ PCI Adapter"},
{0x8086, 0x1229, 0x8086, 0x000A, 5,   "Intel(R) PRO/100+ Management Adapter"},
{0x8086, 0x1229, 0x8086, 0x000A, 5,   "Intel(R) PRO/100+ Management Adapter"},
{0x8086, 0x1229, 0x8086, 0x000B, 8,   "Intel(R) PRO/100+ Adapter"},
{0x8086, 0x1229, 0x8086, 0x000C, 8,   "Intel(R) PRO/100+ Management Adapter"},
{0x8086, 0x1229, 0x8086, 0x000D, 8,   "Intel(R) PRO/100+ Alert on LAN* 2 Management Adapter"},
{0x8086, 0x1229, 0x8086, 0x000E, 8,   "Intel(R) PRO/100+ Alert on LAN* Management Adapter"},
{0x8086, 0x1229, 0x8086, 0x0010, 9,   "Intel(R) PRO/100 S Management Adapter"},
{0x8086, 0x1229, 0x8086, 0x0011, 9,   "Intel(R) PRO/100 S Management Adapter"},
{0x8086, 0x1229, 0x8086, 0x0012, 9,   "Intel(R) PRO/100 S Advanced Management Adapter"},
{0x8086, 0x1229, 0x8086, 0x0013, 9,   "Intel(R) PRO/100 S Advanced Management Adapter"},
{0x8086, 0x1229, 0x8086, 0x0030, 8,   "Intel(R) PRO/100+ Management Adapter with Alert On LAN* GC"},
{0x8086, 0x1229, 0x8086, 0x0040, 0xC, "Intel(R) PRO/100 S Desktop Adapter"},
{0x8086, 0x1229, 0x8086, 0x0041, 0xC, "Intel(R) PRO/100 S Desktop Adapter"},
{0x8086, 0x1229, 0x8086, 0x0042, 0xC, "Intel(R) PRO/100 Desktop Adapter"},
{0x8086, 0x1229, 0x8086, 0x0050, 0xD, "Intel(R) PRO/100 S Desktop Adapter"},
{0x8086, 0x1229, 0x8086, 0x1009, 4,   "Intel(R) PRO/100+ Server Adapter"},
{0x8086, 0x1229, 0x8086, 0x1009, 5,   "Intel(R) PRO/100+ Server Adapter"},
{0x8086, 0x1229, 0x8086, 0x100C, 8,   "Intel(R) PRO/100+ Server Adapter (PILA8470B)"},
{0x8086, 0x1229, 0x8086, 0x1012, 9,   "Intel(R) PRO/100 S Server Adapter"},
{0x8086, 0x1229, 0x8086, 0x1013, 9,   "Intel(R) PRO/100 S Server Adapter"},
{0x8086, 0x1229, 0x8086, 0x1014, 0xD, "Intel(R) PRO/100 Dual Port Server Adapter"},
{0x8086, 0x1229, 0x8086, 0x1015, 0xD, "Intel(R) PRO/100 S Dual Port Server Adapter"},
{0x8086, 0x1229, 0x8086, 0x1016, 0xD, "Intel(R) PRO/100 S Dual Port Server Adapter"},
{0x8086, 0x1229, 0x8086, 0x1017, 8,   "Intel(R) PRO/100+ Dual Port Server Adapter"},
{0x8086, 0x1229, 0x8086, 0x1030, 8,   "Intel(R) PRO/100+ Management Adapter with Alert On LAN* G Server"},
{0x8086, 0x1229, 0x8086, 0x1040, 0xC, "Intel(R) PRO/100 S Server Adapter"},
{0x8086, 0x1229, 0x8086, 0x1041, 0xC, "Intel(R) PRO/100 S Server Adapter"},
{0x8086, 0x1229, 0x8086, 0x1042, 0xC, "Intel(R) PRO/100 Server Adapter"},
{0x8086, 0x1229, 0x8086, 0x1050, 0xD, "Intel(R) PRO/100 S Server Adapter"},
{0x8086, 0x1229, 0x8086, 0x10F0, 4,   "Intel(R) PRO/100+ Dual Port Server Adapter "},
{0x8086, 0x1229, 0x8086, 0x10F0, 5,   "Intel(R) PRO/100+ Dual Port Server Adapter "},
{0x8086, 0x1229, 0x8086, 0x2009, 0xC, "Intel(R) PRO/100 S Mobile Adapter"},
{0x8086, 0x1229, 0x8086, 0x200D, 8,   "Intel(R) PRO/100 CardBus II"},
{0x8086, 0x1229, 0x8086, 0x200D, 8,   "Intel(R) PRO/100 CardBus II"},
{0x8086, 0x1229, 0x8086, 0x200E, 8,   "Intel(R) PRO/100 LAN+Modem56 CardBus II"},
{0x8086, 0x1229, 0x8086, 0x200E, 8,   "Intel(R) PRO/100 LAN+Modem56 CardBus II"},
{0x8086, 0x1229, 0x8086, 0x200F, 0xC, "Intel(R) PRO/100 SR Mobile Adapter"},
{0x8086, 0x1229, 0x8086, 0x2010, 0xC, "Intel(R) PRO/100 S Mobile Combo Adapter"},
{0x8086, 0x1229, 0x8086, 0x2013, 0xC, "Intel(R) PRO/100 SR Mobile Combo Adapter"},
{0x8086, 0x1229, 0x8086, 0x2016, 0xD, "Intel(R) PRO/100 S Mobile Adapter"},
{0x8086, 0x1229, 0x8086, 0x2017, 0xD, "Intel(R) PRO/100 S Combo Mobile Adapter"},
{0x8086, 0x1229, 0x8086, 0x2018, 0xD, "Intel(R) PRO/100 SR Mobile Adapter"},
{0x8086, 0x1229, 0x8086, 0x2019, 0xD, "Intel(R) PRO/100 SR Combo Mobile Adapter"},
{0x8086, 0x1229, 0x8086, 0x2101, 0xC, "Intel(R) PRO/100 P Mobile Adapter"},
{0x8086, 0x1229, 0x8086, 0x2102, 0xC, "Intel(R) PRO/100 SP Mobile Adapter"},
{0x8086, 0x1229, 0x8086, 0x2103, 0xC, "Intel(R) PRO/100 SP Mobile Adapter"},
{0x8086, 0x1229, 0x8086, 0x2104, 0xC, "Intel(R) PRO/100 SP Mobile Adapter"},
{0x8086, 0x1229, 0x8086, 0x2105, 0xC, "Intel PRO/100 SP Mobile Adapter"},
{0x8086, 0x1229, 0x8086, 0x2106, 0xA, "Intel PRO/100 P Mobile Adapter"},
{0x8086, 0x1229, 0x8086, 0x2107, 0xA, "Intel (R) PRO/100 Network Connection"},
{0x8086, 0x1229, 0x8086, 0x2108, 0xC, "Intel (R) PRO/100 Network Connection"},
{0x8086, 0x1229, 0x8086, 0x2200, 0xC, "Intel(R) PRO/100 P Mobile Combo Adapter"},
{0x8086, 0x1229, 0x8086, 0x2201, 0xC, "Intel(R) PRO/100 P Mobile Combo Adapter"},
{0x8086, 0x1229, 0x8086, 0x2202, 0xC, "Intel(R) PRO/100 SP Mobile Combo Adapter"},
{0x8086, 0x1229, 0x8086, 0x2203, CATCHALL, "Intel(R) PRO/100+ Mini PCI"},
{0x8086, 0x1229, 0x8086, 0x2204, CATCHALL, "Intel(R) PRO/100+ Mini PCI"},
{0x8086, 0x1229, 0x8086, 0x2205, 0xC, "Intel(R) PRO/100 SP Mobile Combo Adapter"},
{0x8086, 0x1229, 0x8086, 0x2206, 0xC, "Intel(R) PRO/100 SP Mobile Combo Adapter"},
{0x8086, 0x1229, 0x8086, 0x2207, 0xC, "Intel(R) PRO/100 SP Mobile Combo Adapter"},
{0x8086, 0x1229, 0x8086, 0x2208, 0xC, "Intel(R) PRO/100 P Mobile Combo Adapter"},
{0x8086, 0x1229, 0x8086, 0x2408, 9,   "Intel(R) PRO/100+ Mini PCI"},
{0x8086, 0x1229, 0x8086, 0x240F, 9,   "Intel(R) PRO/100+ Mini PCI"},
{0x8086, 0x1229, 0x8086, 0x2411, 9,   "Intel(R) PRO/100+ Mini PCI"},
{0x8086, 0x1229, 0x8086, 0x3000, 8,   "Intel(R) 82559 Fast Ethernet LAN on Motherboard"},
{0x8086, 0x1229, 0x8086, 0x3001, 8,   "Intel(R) 82559 Fast Ethernet LOM with Alert on LAN*"},
{0x8086, 0x1229, 0x8086, 0x3002, 8,   "Intel(R) 82559 Fast Ethernet LOM with Alert on LAN* 2"},
{0x8086, 0x1229, 0x8086, 0x3006, 0xC, "Intel(R) PRO/100 S Network Connection"},
{0x8086, 0x1229, 0x8086, 0x3007, 0xC, "Intel(R) PRO/100 S Network Connection"},
{0x8086, 0x1229, 0x8086, 0x3008, 0xC, "Intel(R) PRO/100 Network Connection"},
{0x8086, 0x1229, 0x8086, 0x3010, CATCHALL, "Intel(R) PRO/100 S Network Connection"},
{0x8086, 0x1229, 0x8086, 0x3011, CATCHALL, "Intel(R) PRO/100 S Network Connection"},
{0x8086, 0x1229, 0x8086, 0x3012, CATCHALL, "Intel(R) PRO/100 Network Connection"},
{0x8086, 0x1229, 0x1014, 0x005C, CATCHALL, "IBM Mobile, Desktop & Server Adapters"},
{0x8086, 0x1229, 0x1014, 0x305C, CATCHALL, "IBM Mobile, Desktop & Server Adapters"},
{0x8086, 0x1229, 0x1014, 0x405C, CATCHALL, "IBM Mobile, Desktop & Server Adapters"},
{0x8086, 0x1229, 0x1014, 0x605C, CATCHALL, "IBM Mobile, Desktop & Server Adapters"},
{0x8086, 0x1229, 0x1014, 0x505C, CATCHALL, "IBM Mobile, Desktop & Server Adapters"},
{0x8086, 0x1229, 0x1014, 0x105C, CATCHALL, "IBM Mobile, Desktop & Server Adapters"},
{0x8086, 0x1229, 0x1014, 0x805C, CATCHALL, "IBM Mobile, Desktop & Server Adapters"},
{0x8086, 0x1229, 0x1014, 0x705C, CATCHALL, "IBM Mobile, Desktop & Server Adapters"},
{0x8086, 0x1229, 0x1014, 0x01F1, CATCHALL, "IBM Mobile, Desktop & Server Adapters"},
{0x8086, 0x1229, 0x1014, 0x0232, CATCHALL, "IBM Mobile, Desktop & Server Adapters"},
{0x8086, 0x1229, 0x1014, 0x0207, CATCHALL, "Intel(R) PRO/100 Network Connection"},
{0x8086, 0x1229, 0x1014, 0x01BC, CATCHALL, "Intel(R) PRO/100 Network Connection"},
{0x8086, 0x1229, 0x1014, 0x01CE, CATCHALL, "Intel(R) PRO/100 Network Connection"},
{0x8086, 0x1229, 0x1014, 0x01DC, CATCHALL, "Intel(R) PRO/100 Network Connection"},
{0x8086, 0x1229, 0x1014, 0x01EB, CATCHALL, "Intel(R) PRO/100 Network Connection"},
{0x8086, 0x1229, 0x1014, 0x01EC, CATCHALL, "Intel(R) PRO/100 Network Connection"},
{0x8086, 0x1229, 0x1014, 0x0202, CATCHALL, "Intel(R) PRO/100 Network Connection"},
{0x8086, 0x1229, 0x1014, 0x0205, CATCHALL, "Intel(R) PRO/100 Network Connection"},
{0x8086, 0x1229, 0x1014, 0x0217, CATCHALL, "Intel(R) PRO/100 Network Connection"},
{0x8086, 0x1229, 0x0E11, 0xB01E, CATCHALL, "Compaq Fast Ethernet Server Adapter"},
{0x8086, 0x1229, 0x0E11, 0xB01F, CATCHALL, "Compaq Fast Ethernet Server Adapter"},
{0x8086, 0x1229, 0x0E11, 0xB02F, CATCHALL, "Compaq Fast Ethernet Server Adapter"},
{0x8086, 0x1229, 0x0E11, 0xB04A, CATCHALL, "Compaq Fast Ethernet Server Adapter"},
{0x8086, 0x1229, 0x0E11, 0xB0C6, CATCHALL, "Compaq Fast Ethernet Server Adapter"},
{0x8086, 0x1229, 0x0E11, 0xB0C7, CATCHALL, "Compaq Fast Ethernet Server Adapter"},
{0x8086, 0x1229, 0x0E11, 0xB0D7, CATCHALL, "Compaq Fast Ethernet Server Adapter"},
{0x8086, 0x1229, 0x0E11, 0xB0DD, CATCHALL, "Compaq Fast Ethernet Server Adapter"},
{0x8086, 0x1229, 0x0E11, 0xB0DE, CATCHALL, "Compaq Fast Ethernet Server Adapter"},
{0x8086, 0x1229, 0x0E11, 0xB0E1, CATCHALL, "Compaq Fast Ethernet Server Adapter"},
{0x8086, 0x1229, 0x0E11, 0xB134, CATCHALL, "Compaq Fast Ethernet Server Adapter"},
{0x8086, 0x1229, 0x0E11, 0xB13C, CATCHALL, "Compaq Fast Ethernet Server Adapter"},
{0x8086, 0x1229, 0x0E11, 0xB144, CATCHALL, "Compaq Fast Ethernet Server Adapter"},
{0x8086, 0x1229, 0x0E11, 0xB163, CATCHALL, "Compaq Fast Ethernet Server Adapter"},
{0x8086, 0x1229, 0x0E11, 0xB164, CATCHALL, "Compaq Fast Ethernet Server Adapter"},
{0x8086, 0x2449, 0x8086, 0x3010, 1, "Intel(R) PRO/100 VE Desktop Adapter"},
{0x8086, 0x2449, 0x8086, 0x3010, 3, "Intel(R) PRO/100 VE Desktop Adapter"},
{0x8086, 0x2449, 0x8086, 0x3011, 1, "Intel(R) PRO/100 VM Desktop Adapter"},
{0x8086, 0x2449, 0x8086, 0x3011, 3, "Intel(R) PRO/100 VM Desktop Adapter"},
{0x8086, 0x2449, 0x8086, 0x3012, 1, "82562EH based Phoneline Network Connection"},
{0x8086, 0x2449, 0x8086, 0x3012, 3, "82562EH based Phoneline Network Connection"},
{0x8086, 0x2449, 0x8086, 0x3013, 1, "Intel(R) PRO/100 VE Network ConnectionPLC LOM"},
{0x8086, 0x2449, 0x8086, 0x3013, 3, "Intel(R) PRO/100 VE Network Connection"},
{0x8086, 0x2449, 0x8086, 0x3014, 1, "Intel(R) PRO/100 VM Network Connection"},
{0x8086, 0x2449, 0x8086, 0x3014, 3, "Intel(R) PRO/100 VM Network Connection"},
{0x8086, 0x2449, 0x8086, 0x3015, 1, "82562EH based Phoneline Network Connection"},
{0x8086, 0x2449, 0x8086, 0x3015, 3, "82562EH based Phoneline Network Connection"},
{0x8086, 0x2449, 0x8086, 0x3016, 1, "Intel(R) PRO/100 P Mobile Combo Adapter"},
{0x8086, 0x2449, 0x8086, 0x3016, 3, "Intel(R) PRO/100 P Mobile Combo Adapter"},
{0x8086, 0x2449, 0x8086, 0x3017, 1, "Intel(R) PRO/100 P Mobile Adapter"},
{0x8086, 0x2449, 0x8086, 0x3017, 3, "Intel(R) PRO/100 P Mobile Adapter"},
{0x8086, 0x2449, 0x8086, 0x3018, 1, "Intel (R) PRO/100 Network Connection"},
{0x8086, 0x2449, 0x8086, 0x3018, 3, "Intel (R) PRO/100 Network Connection"},
{0x8086, 0x1031, CATCHALL, CATCHALL, CATCHALL, "Intel(R) PRO/100 VE Network Connection"},
{0x8086, 0x1032, CATCHALL, CATCHALL, CATCHALL, "Intel(R) PRO/100 VE Network Connection"},
{0x8086, 0x1033, CATCHALL, CATCHALL, CATCHALL, "Intel(R) PRO/100 VM Network Connection"},
{0x8086, 0x1034, CATCHALL, CATCHALL, CATCHALL, "Intel(R) PRO/100 VM Network Connection"},
{0x8086, 0x1035, CATCHALL, CATCHALL, CATCHALL, "82562EH based Phoneline Network Connection"},
{0x8086, 0x1036, CATCHALL, CATCHALL, CATCHALL, "82562EH based Phoneline Network Connection"},
{0x8086, 0x1037, CATCHALL, CATCHALL, CATCHALL, "82562EH based Phoneline Network Connection"},
{0x8086, 0x1038, CATCHALL, CATCHALL, CATCHALL, "Intel(R) PRO/100 VM Network Connection"},
{CATCHALL, CATCHALL, CATCHALL, CATCHALL, CATCHALL, "Intel(R) 8255x-based Ethernet Adapter"},
{0x0, 0x0, 0x0, 0x0, 0x0, NULL} // This has to be the last entry
};

bd_config_t *e100_first = NULL;
int          e100nics = 0;

#define MAX_NIC 16


/* Start Command Line Options
 *
 * Set the line speed and duplex mode of the controller.
 *  0 indicates autodetection for both speed and duplex mode
 *  1 indicates a speed of 10MBS and a duplex mode of half 
 *  2 indicates a speed of 10MBS and a duplex mode of full 
 *  3 indicates a speed of 100MBS and a duplex mode of half 
 *  4 indicates a speed of 100MBS and a duplex mode of full 
 *
 *    NOTE: The PRO/10+ can't autodetect so if the setting is left at
 *    0 the driver will force it to 10/HALF.  If full duplex is desired
 *    a setting of 2 is required.  Setting 3 and 4 are invalid for the
 *    PRO/10+ hardware.
 *
 * We support up to 16 nics with this structure.  If you need more, 
 * add new members to the structure.
 */
static int  e100_speed_duplex[MAX_NIC] = { 0, 0, 0, 0, 0, 0, 0, 0,
                                           0, 0, 0, 0, 0, 0, 0, 0 };
static int  RxDescriptors[MAX_NIC]  = { -1, -1, -1, -1, -1, -1, -1, -1,
                                        -1, -1, -1, -1, -1, -1, -1, -1 };
static int  TxDescriptors[MAX_NIC]  = { -1, -1, -1, -1, -1, -1, -1, -1,
                                        -1, -1, -1, -1, -1, -1, -1, -1 };
static int  XsumRX[MAX_NIC]         = { 1, 1, 1, 1, 1, 1, 1, 1,
                                        1, 1, 1, 1, 1, 1, 1, 1 };
static WORD PhoneLinePower[MAX_NIC] = { 0, 0, 0, 0, 0, 0, 0, 0,
                                        0, 0, 0, 0, 0, 0, 0, 0 };
static WORD PhoneLineSpeed[MAX_NIC] = { 1, 1, 1, 1, 1, 1, 1, 1,
                                        1, 1, 1, 1, 1, 1, 1, 1 };
static int  ucode[MAX_NIC] = { 1, 1, 1, 1, 1, 1, 1, 1,
                               1, 1, 1, 1, 1, 1, 1, 1 };
static DWORD ber[MAX_NIC] = { ZLOCK_MAX_ERRORS, ZLOCK_MAX_ERRORS,
                              ZLOCK_MAX_ERRORS, ZLOCK_MAX_ERRORS,
                              ZLOCK_MAX_ERRORS, ZLOCK_MAX_ERRORS,
                              ZLOCK_MAX_ERRORS, ZLOCK_MAX_ERRORS,
                              ZLOCK_MAX_ERRORS, ZLOCK_MAX_ERRORS,
                              ZLOCK_MAX_ERRORS, ZLOCK_MAX_ERRORS,
                              ZLOCK_MAX_ERRORS, ZLOCK_MAX_ERRORS,
                              ZLOCK_MAX_ERRORS, ZLOCK_MAX_ERRORS
                            };

/* global (per driver) statistic */

static long e100_total_intrs = 0;
static long e100_valid_intrs = 0;
static long e100_fr_intrs = 0;
static long e100_cna_intrs = 0;
static long e100_cx_intrs = 0;
static long e100_rnr_intrs = 0;
static long e100_er_intrs = 0;
static long e100_mdi_intrs = 0;
static long e100_swi_intrs = 0;
static long e100_fcp_intrs = 0;

static int e100_rx_not_comp = 0;

/* ====================================================================== */

static void e100_D102_check_checksum (bd_config_t *, struct sk_buff *);
static void e100_free_rfd_pool (bdd_t * bddp);
static void e100_free_tbds (bdd_t * bddp);

static void remove_e100_proc_subdir (device_t * dev);

static int e100_open (device_t *);
static int e100_close (device_t *);
static int e100_xmit_frame (struct sk_buff *, device_t *);
static int e100init (bd_config_t *);
static int e100_set_mac (device_t *, void *);

static void *e100_get_stats (device_t *);

static void e100intr (int);
static void e100_print_brd_conf (bd_config_t *);
static void e100_set_multi (device_t *);
static void drv_usecwait (DWORD);

static char *e100_GetBrandingMesg (bd_config_t *);
static void e100_get_pci_info (pci_dev_t *, bd_config_t *);

static bd_config_t *e100_alloc_space (void);
static int e100_alloc_tcbs (bd_config_t *);

static void e100_dealloc_space (bd_config_t *);
static void e100_setup_tcb_pool (ptcb_t, DWORD, bd_config_t *);

static sk_buff_t *e100_alloc_rfd_pool (bdd_t *);

/* EEPROM access functions */
static void e100_rd_eaddr (bd_config_t *);
static void e100_rd_pwa_no (bd_config_t * bdp);
static void e100_wrEEPROMcsum (bd_config_t *);
static void e100_ShiftOutBits (bd_config_t *, WORD, WORD);
static void e100_RaiseClock (bd_config_t *, WORD *);
static void e100_LowerClock (bd_config_t *, WORD *);
static void e100_EEpromCleanup (bd_config_t *);
static void e100_WriteEEprom (bd_config_t *, WORD, WORD);
static void e100_StandBy (bd_config_t *);
static void e100_rd_vendor_info (bd_config_t *, WORD *, WORD *);

static WORD e100_ReadEEprom (bd_config_t *, WORD);
static WORD e100_ShiftInBits (bd_config_t *);
static WORD e100_WaitEEPROMCmdDone (bd_config_t *);
static WORD e100_GetEEpromSize (bd_config_t *);
static WORD e100_EEpromAddressSize (WORD);

/* Functions for accessing the adapter hardware */
static boolean_t e100_clr_cntrs (bd_config_t *);
static boolean_t e100_exec_poll_cmd (bd_config_t *);
static boolean_t e100_load_microcode (bd_config_t *, BYTE);
static boolean_t e100_selftest (bd_config_t *);
static boolean_t e100_hw_init (bd_config_t *, DWORD);
static boolean_t e100_sw_init (bd_config_t *);
static boolean_t e100_configure (bd_config_t *);
static boolean_t e100_setup_iaaddr (bd_config_t *, e100_eaddr_t *);
static boolean_t e100_wait_scb (bd_config_t *);

static void e100_start_ru (bd_config_t *);
static void e100_ru_abort (bd_config_t *);
static void e100_sw_reset (bd_config_t *, DWORD);
static void e100_dis_intr (bd_config_t *);
static void e100_enbl_intr (bd_config_t *);
static void e100_trigger_SWI (bd_config_t * bdp);
static void e100_dump_stats_cntrs (bd_config_t *);

void e100_check_options (int board);

/* all inline func must be declared with body, here they're */

/* 
 * Description: allocates skb with enough room for rfd, ans and data,
 *              and reserve non-data space
 * Arguments:
 *      bddp - Ptr to this card's e100_bdconfig structure
 *
 * Returns:
 *      sk_buff_t * - the new sk_buff or NULL if we failed to allocate one.
 */
inline sk_buff_t *e100_alloc_skb (bdd_t * bddp)
{
  sk_buff_t *new_skb = (sk_buff_t *) dev_alloc_skb (sizeof (rfd_t)
#ifdef IANS
                                         + BD_ANS_INFO_SIZE
#endif
                                         + CSUM_559_SIZE + VLAN_SIZE);
  if (new_skb)
  {
#ifdef IANS
    /* we need to allocate extra room for the ans stuff */
    bd_ans_os_ReserveSpaceForANS (new_skb);
#endif
    skb_reserve (new_skb, bddp->rfd_size);
    return (new_skb);
  }

  if (e100_debug > 2)
     printk ("e100_alloc_skb, b[%d]: skb_alloc failed!!!\n",
             bddp->bdp->bd_number);
  return (NULL);
}
  
/* 
 * Procedure:   e100_add_skb_to_end
 *
 * Description: Adds an skb to the end of our rfd list.
 *
 * Arguments:
 *      bddp - Ptr to this card's e100_bdconfig structure
 *      skb_buff_t *new_skb - Ptr to the new skb
 *
 * Returns:
 *    -     NONE
 */
inline void e100_add_skb_to_end (bdd_t * bddp, sk_buff_t * new_skb)
{
  rfd_t *rfdn;                  /* The new rfd */
  rfd_t *rfdp;                  /* The old rfd */

  /* set who this is from */
  new_skb->dev = bddp->bdp->device;

  rfdn = RFD_POINTER (new_skb, bddp);

  /* init all fields in rfd */
  rfdn->rfd_header.cb_status = 0;
  rfdn->rfd_header.cb_cmd = RFD_EL_BIT;
  rfdn->rfd_rbd_ptr = E100_NULL; /* not really necessary */
  rfdn->rfd_act_cnt = 0;

  rfdn->rfd_sz = sizeof (eth_rx_buffer_t) + CSUM_559_SIZE + VLAN_SIZE;

  /* append new_skb to the end of the rx skb queue */
  rfdn->prev = bddp->rfd_tail;
  rfdn->next = NULL;
  if (bddp->rfd_tail)
  {
    rfdp = RFD_POINTER (bddp->rfd_tail, bddp);
    rfdp->next = new_skb;
    rfdp->rfd_header.cb_lnk_ptr = (DWORD) virt_to_bus (rfdn);
    /* Clear the EL bit on the previous rfd */
    rfdp->rfd_header.cb_cmd &= ~RFD_EL_BIT;
  }
  bddp->rfd_tail = new_skb;     /* reset the tail pointer */
  if (bddp->rfd_head == NULL)
     bddp->rfd_head = new_skb;

  if (e100_debug > 2)
  {
    printk ("add_to_end, board_num = 0x%x\n", bddp->bdp->bd_number);
    printk ("     rfd_head = 0x%x, rfd_tail = 0x%x\n", bddp->rfd_head, bddp->rfd_tail);
    printk ("     rfdn->next = 0x%x, rfdn->prev = 0x%x\n", rfdn->next, rfdn->prev);
    printk ("     rfdp->next = 0x%x, rfdp->prev = 0x%x\n", rfdp->next, rfdp->prev);
  }
}


/* 
 * Procedure:   e100_exec_cmd
 *
 * Description: This general routine will issue a command to the e100.
 *
 * Arguments:
 *      bdp -          Ptr to this card's e100_bdconfig structure
 *      scb_cmd_low - The command that is to be issued.
 * 
 */
#define e100_exec_cmd(bdp, cmd_low) (bdp)->bddp->scbp->scb_cmd_low = (BYTE)(cmd_low)

/* 
 * Procedure:   e100_wait_exec_cmd
 *
 * Description: This general routine will issue a command to the e100.
 *      bdp -          Ptr to this card's e100_bdconfig structure
 *      scb_cmd_low - The command that is to be issued.
 * Returns:
 *      B_TRUE if the command was issued to the chip successfully
 *      B_FALSE if the command was not issued to the chip
 */
inline boolean_t e100_wait_exec_cmd (bd_config_t * bdp, BYTE scb_cmd_low)
{
  bdd_t *bddp;                  /* stores all adapter specific info */

  bddp = (pbdd_t) bdp->bddp;    /* get the bddp for this board */

  if (DEBUG)
    printk ("e100_wait_exec_cmd: Called with cmd=0x%x\n", scb_cmd_low);

  if (e100_wait_scb (bdp) != B_TRUE)
  {
    printk ("e100_wait_exec_cmd: Wait failed. scb cmd=0x%x\n", bddp->scbp->scb_cmd_low);
    return (B_FALSE);
  }

  e100_exec_cmd (bdp, scb_cmd_low);
  return (B_TRUE);
}

void e100_tx_srv (bd_config_t *);
void e100_rx_srv (bd_config_t *);
void e100_watchdog (device_t *);
void e100_refresh_txthld (bd_config_t *);
void e100_adjust_cid (bd_config_t *);
void e100_cu_start (bd_config_t *, tcb_t *);

static ptcb_t e100_prepare_xmit_buff (bd_config_t *, sk_buff_t *);
static ptcb_t e100_prepare_ext_xmit_buff (bd_config_t *, sk_buff_t *);

/* Functions for accessing the physical layer (PHY) hardware
 */
static boolean_t e100_SetupPhy (bd_config_t *);
static boolean_t e100_PhyLinkState (bd_config_t *);
static boolean_t e100_GetPhyLinkState (bd_config_t *);
static boolean_t e100_phydetect (bd_config_t *);

void e100_FindPhySpeedAndDpx (bd_config_t *, DWORD);
void e100_ForceSpeedAndDuplex (bd_config_t *);
void e100_auto_neg (bd_config_t *);
void e100_fix_polarity (bd_config_t * bdp);
void e100_ResetPhy (bd_config_t *);
void e100_MdiWrite (bd_config_t *, DWORD, DWORD, WORD);
void e100_MdiRead (bd_config_t *, DWORD, DWORD, WORD *);
void e100_phy_check (bd_config_t *);
void e100_ResetPhy (bd_config_t *);

/* Other useful functions
 */
static char *e100_numtostr (DWORD, BYTE, BYTE);

/* 82562E (Gilad) Function Prototypes */
void Phy82562EHNoiseFloorWrite (bdd_t * adapter, BYTE Value);
void Phy82562EHNoiseCounterClear (bdd_t * adapter);
BYTE Phy82562EHNoiseEventsRead (bdd_t * adapter);
void Phy82562EHDelayMilliseconds (int time);
BYTE Phy82562EHNoiseEventsWithDelayCount (bdd_t * adapter, BYTE NoiseFloor, long Delay);
BYTE Phy82562EHMedianFind (bdd_t * adapter);
void Phy82562EHAddressSpaceSwitch (bdd_t * adapter, long Page);
void Phy82562EHRegisterInit (bdd_t * adapter);
void Phy82562EHNoiseFloorCalculate (bdd_t * adapter);
void Phy82562EHSpeedPowerSetup (bdd_t * bddp);
void PhyProcessing82562EHInit (bdd_t * bddp);

static void e100_handle_zero_lock_state (bd_config_t * bdp);


/***************************************************************************/
/***************************************************************************/
/*     Module Install/Uninstall Stuff                                      */
/***************************************************************************/


/*
 * Description: This routine is called when the dynamic driver module
 *              "e100" is loaded using the command "insmod".
 *              It calls the initialization routine e100init.
 */
static int e100_probe (void)
{
  static int e100_ohio_flag = 0; /* will be set if ohio found */
  device_t    *dev;
  bd_config_t *bdp;
  bdd_t       *bddp;
  pci_dev_t   *pcid = NULL;
  int         first_time = 0;

  if (e100_debug)
     printk ("e100_probe1\n");

  if (!pci_present())
     return (0);

  /* loop through all of the ethernet PCI devices looking for ours.
   * if we encounter device that we can't "pick up" we pass to the next,
   * while after memory error we stop looking further for NICs
   */
  while ((pcid = pci_find_class (PCI_CLASS_NETWORK_ETHERNET << 8, pcid)))
  {
    dev = NULL;

    if (e100_debug)
       printk ("e100_probe: vendor = 0x%x, device = 0x%x \n",
               pcid->vendor, pcid->device);

    if ((pcid->vendor != 0x8086) ||
        ((pcid->device != 0x1229) && (pcid->device != 0x1029)  &&
         (pcid->device != 0x1209) && (pcid->device != 0x2449)) &&
        (!(pcid->device > 0x1030) && (pcid->device < 0x1039)))
       continue;

    dev = init_etherdev (dev, 0);
    if (dev == NULL)
    {
      printk ("Not able to alloc etherdev struct\n");
      return (e100nics);
    }

    if (!first_time)
    {
      /* print out the version */
      first_time = 1;
      printk ("%s\n", e100_full_driver_name);
      printk ("%s\n", e100_copyright);
    }

    /* Allocate all the memory that the driver will need
     * Tx and Rx descriptors will be allocated later in this function
     */
    if ((bdp = e100_alloc_space()) == NULL)
    {
      printk ("%s - Failed to allocate memory\n", e100_short_driver_name);
      e100_dealloc_space (bdp);
      unregister_netdev (dev);
      return (e100nics);
    }
    bdp->flags = 0;
    bddp = bdp->bddp;
    bddp->ven_id = pcid->vendor;
    bddp->dev_id = pcid->device;
    bddp->pci_dev = pcid;
    bdp->vendor = pcid->vendor;

    /* Obtain the PCI specific information about the driver.
     */
    e100_get_pci_info (pcid, bdp);

    /* Decide whether to load or not based on the * sub-device ID file.
     * This also sets the id string.
     */
    if (!e100_GetBrandingMesg (bdp))
    {
      e100_dealloc_space (bdp);
      unregister_netdev (dev);
      continue;
    }

    if (e100_debug)
       printk ("ven_id = 0x%x\n", bddp->ven_id);

    /* init the timer */
    bdp->timer_val = -1;

    /* save off the needed information */
    bdp->device    = dev;
    dev->priv      = bdp;
    dev->base_addr = bdp->mem_start; /* set in e100_get_pci_info */
    dev->irq       = pcid->irq;
    bdp->irq_level = pcid->irq;

    if ((bddp->dev_id > 0x1030) && (bddp->dev_id < 0x1039))
       bddp->rev_id = 0xff;      /* workaround for ICH3 */

    /* set up other board info based on PCI info */
    if (bddp->rev_id == 0xff)
       bddp->rev_id = 1;  /* If rev_id is invalid, treat this as a 82557 */

    if ((BYTE)bddp->rev_id >= D101A4_REV_ID)
       bddp->flags |= IS_BACHELOR;

    if ((BYTE)bddp->rev_id >= D102_REV_ID)
    {
      bddp->flags |= USE_IPCB;
      bddp->rfd_size = 32;
    }
    else
      bddp->rfd_size = 16;

    if ((bddp->dev_id == 0x2449) || (bddp->dev_id > 0x1030) &&
        (bddp->dev_id < 0x1039))
      bddp->flags |= IS_ICH;

    /* Identify Ohio's Port number */
    if (bddp->sub_dev_id == PCI_OHIO_BOARD)
    {
      /* identify Port 1 or Port 2 based on static ohio flag */
      if (!e100_ohio_flag)
      {
        strcat (e100id_string, " (Port 1)");
        e100_ohio_flag = 1;     /* so that the next port shows as port 2 */
      }
      else
      {
        strcat (e100id_string, " (Port 2)");
        e100_ohio_flag = 0;     /* in case there is > 1 ohio board */
        bddp->port_num = 2;
      }
    }

    e100_check_options (e100nics);

    /* init all the data structure and find the rest of the pci info
     */
    if (!e100init (bdp))
    {
      printk ("Failed to initialize e100, instance #%d\n", e100nics);
      e100_dealloc_space (bdp);
      unregister_netdev (dev);
      continue;
    }

    /* Printout the board configuration */
    e100_print_brd_conf (bdp);

    /* assign driver methods */
    dev->open      = e100_open;
    dev->xmit      = e100_xmit_frame;
    dev->close     = e100_close;
    dev->get_stats = e100_get_stats;
    dev->set_multicast_list = e100_set_multi;
    dev->set_mac_address    = e100_set_mac;
    e100nics++;

    if (e100_debug)
    {
      printk ("dev = 0x%p ", dev);
      printk ("  priv = 0x%p\n", dev->priv);
      printk ("  irq = 0x%x ", dev->irq);
      printk ("  next = 0x%p ", dev->next);
      printk ("  flags = 0x%x\n", dev->flags);
      printk ("  bdp = 0x%p\n", bdp);
      printk ("  irq_level = 0x%x\n", bdp->irq_level);
    }
    if (e100nics == MAX_NIC)
    {
      printk ("e100: found %d NICs, stop searching further\n", MAX_NIC);
      break;
    }
  }
  return (e100nics);
}

/*
 * This routine does range checking on command-line options
 */
void e100_check_options (int board)
{
  /* Transmit Descriptor Count */
  if (TxDescriptors[board] == -1)
  {
    TxDescriptors[board] = E100_DEFAULT_TCB;
  }
  else if (TxDescriptors[board] > E100_MAX_TCB)
  {
    printk ("Invalid TxDescriptor count specified (%i),"
            " using maximum value of %i\n", TxDescriptors[board], E100_MAX_TCB);
    TxDescriptors[board] = E100_MAX_TCB;
  }
  else if (TxDescriptors[board] < E100_MIN_TCB)
  {
    printk ("Invalid TxDescriptor count specified (%i),"
            " using minimum value of %i\n", TxDescriptors[board], E100_MIN_TCB);
    TxDescriptors[board] = E100_MIN_TCB;
  }
  else
  {
    printk ("Using specified value of %i TxDescriptors\n", TxDescriptors[board]);
  }

  /* Receive Descriptor Count */
  if (RxDescriptors[board] == -1)
  {
    RxDescriptors[board] = E100_DEFAULT_RFD;
  }
  else if ((RxDescriptors[board] > E100_MAX_RFD) || (RxDescriptors[board] < E100_MIN_RFD))
  {
    printk ("Invalid RxDescriptor count specified (%i),"
            " using default of %i\n", RxDescriptors[board], E100_DEFAULT_RFD);
    RxDescriptors[board] = E100_DEFAULT_RFD;
  }
  else
  {
    printk ("Using specified value of %i RxDescriptors\n", RxDescriptors[board]);
  }

  /* XsumRX (initialy set to TRUE) */
  if ((XsumRX[board] != TRUE) && (XsumRX[board] != FALSE))
  {
    printk ("Invalid XsumRX value specified (%i)," " using default of %i\n", XsumRX[board], E100_DEFAULT_XSUM);
    XsumRX[board] = E100_DEFAULT_XSUM;
  }

  if (ber[board] > ZLOCK_MAX_ERRORS)
  {
    ber[board] = ZLOCK_MAX_ERRORS;
    printk ("Invalid Bit Error Rate count specified (%i)," " using default of %i\n", ber[board], ZLOCK_MAX_ERRORS);
  }
  else if (ber[board] != ZLOCK_MAX_ERRORS)
  {
    printk ("Using specified BER value of %i \n", ber[board]);
  }
}

/*
 * This routine is the open call for the interface.
 */
static int e100_open (device_t * dev)
{
  bd_config_t *bdp;
  int          ret_val;
  bdd_t       *bddp;
  pbuf_pool_t  ptcb_pool;

  bdp = dev->priv;
  bddp = bdp->bddp;

  if (e100_debug)
     printk ("open: SOR, bdp = 0x%p\n", bdp);

  if (bdp->flags & DF_OPENED)
  {
    if (e100_debug)
       printk ("open: Device is already open. Undo any changes you have made\n");
    return (0);
  }

  /* MUST set irq2dev_map first, because IRQ may come
   * before request_irq() returns.
   */
  irq2dev_map[dev->irq] = dev;
  if (request_irq (dev->irq, &e100intr))
  {
    irq2dev_map[dev->irq] = NULL;
    if (e100_debug)
       printk ("open: request_irq failed\n");
    return (0);
  }

  /* setup the tcb pool */
  if (!e100_alloc_tcbs (bdp))
  {
    if (e100_debug)
       printk ("%s - Failed to allocate descriptors\n", e100_short_driver_name);

    e100_free_tbds (bddp);
    free_irq (dev->irq, dev);
    irq2dev_map[dev->irq] = NULL;
    return (0);
  }
  ptcb_pool        = &bddp->tcb_pool;
  ptcb_pool->head  = 0;
  ptcb_pool->tail  = 0;
  ptcb_pool->count = TxDescriptors[bdp->bd_number];
  e100_setup_tcb_pool ((ptcb_t) ptcb_pool->data, TxDescriptors[bdp->bd_number], bdp);

  /* Arrange dynamic RFD's in a circular queue & setup buffer pool */
  if (e100_alloc_rfd_pool (bddp) == NULL)
  {
    e100_free_rfd_pool (bddp);
    e100_free_tbds (bddp);
    free_irq (dev->irq, dev);
    irq2dev_map[dev->irq] = NULL;
    return (0);
  }

  /* We have to reload CU and RU base because they could have been
   * corrupted from a TCO packet
   * Load the CU BASE (set to 0, because we use linear mode)
   */
  bddp->prev_cu_cmd = SCB_CUC_LOAD_BASE;
  bddp->scbp->scb_gen_ptr = 0;
  e100_exec_cmd (bdp, SCB_CUC_LOAD_BASE);

  /* Wait for the SCB command word to clear before we set the general pointer
   */
  if (e100_wait_scb (bdp) != B_TRUE)
  {
    e100_free_rfd_pool (bddp);
    e100_free_tbds (bddp);
    free_irq (dev->irq, dev);
    irq2dev_map[dev->irq] = NULL;
    return (0);
  }

  /* Load the RU BASE (set to 0, because we use linear mode) */
  bddp->scbp->scb_gen_ptr = 0;
  bddp->prev_cu_cmd = SCB_RUC_LOAD_BASE;
  e100_exec_cmd (bdp, SCB_RUC_LOAD_BASE);

  /* launch the watchdog timer */
  init_timer (&bdp->timer_id);
  bdp->timer_id.expires = bdp->timer_val = jiffies + (2 * HZ);
  bdp->timer_id.data = (DWORD) dev;
  bdp->timer_id.function = (void *) &e100_watchdog;
  add_timer (&bdp->timer_id);

  netif_start_queue (dev);

  e100_start_ru (bdp);
  e100_enbl_intr (bdp);

  bdp->flags |= DF_OPENED;
  return (1);
}

/*
 *
 */
static void e100_close (device_t *dev)
{
  bd_config_t *bdp;
  bdd_t *bddp;

  bdp = dev->priv;
  bddp = bdp->bddp;

  /* set the device to not started */
  netif_stop_queue (dev);

  /* stop the hardware */
  e100_ru_abort (bdp);

  /* Disable all possible interrupts including the sequence error  interrupts.   */
  e100_dis_intr (bdp);
  free_irq (dev->irq, dev);
  irq2dev_map[dev->irq] = NULL;

  /* kill the timer */
  del_timer (&bdp->timer_id);

  /* free tx and rx memory structures */
  e100_free_tbds (bddp);
  e100_free_rfd_pool (bddp);

  /* reset the phy */
  e100_ResetPhy (bdp);

  /* lets reset the chip */
  e100_sw_reset (bdp, PORT_SELECTIVE_RESET);

  bdp->flags &= ~DF_OPENED;
}

/*
 * This routine is called to transmit a frame.
 */
static int e100_xmit_frame (sk_buff_t * skb, device_t * dev)
{
  bd_config_t *bdp = dev->priv;
  bdd_t       *bddp = bdp->bddp;
  int          lock_flag;
  net_device_stats_t *stats = &bddp->net_stats;

  /* if there are no tcbs, tell stack to stop sending frames to us for now */
  if (!TCBS_AVAIL (&(bddp->tcb_pool)))
  {
    if (e100_debug > 2)
       printk ("e100_tx_frame no TCBs left\n");
#ifdef IANS
    if (ANS_PRIVATE_DATA_FIELD (bdp)->iANS_status == IANS_COMMUNICATION_UP)
    {
      if (ans_notify)
         ans_notify (dev, IANS_IND_XMIT_QUEUE_FULL);
    }
#endif
    netif_stop_queue (dev);
    return (0);
  }

  if (bddp->flags & USE_IPCB)
       e100_prepare_xmit_buff (bdp, skb);
  else if ((bddp->flags & IS_BACHELOR) && e100_enhanced_tx_enable)
       e100_prepare_ext_xmit_buff (bdp, skb);
  else e100_prepare_xmit_buff (bdp, skb);
  stats->tx_bytes += skb->len;

  dev->tx_start = jiffies;
  return (1);
}
   
/*
 * This routine is called when the OS wants the nic stats returned
 */
static void *e100_get_stats (device_t * dev)
{
  bd_config_t *bdp  = dev->priv;
  bdd_t       *bddp = bdp->bddp;
  err_stats_t *bd_stats = bddp->perr_stats;

  struct net_device_stats *stats = &bddp->net_stats;

  /* rx_bytes is updated in e100_rx_srv
   */
  stats->rx_packets = bd_stats->gd_recvs;
  stats->tx_packets = bd_stats->gd_xmits;
  stats->rx_errors  = bd_stats->rcv_crc_err + bd_stats->rcv_align_err +
                      bd_stats->rcv_runts + bd_stats->rcv_cdt_frames;

  /* add up all the tx errors for the total number of tx frames dropped
   */
  stats->rx_dropped        = bd_stats->rcv_rsrc_err;
  stats->rx_length_errors  = bd_stats->rcv_runts;
  stats->rx_over_errors    = bd_stats->rcv_rsrc_err;
  stats->rx_crc_errors     = bd_stats->rcv_crc_err;
  stats->rx_frame_errors   = bd_stats->rcv_align_err;
  stats->rx_fifo_errors    = bd_stats->rcv_dma_orun;
  stats->tx_errors         = bd_stats->tx_lost_csrs + bd_stats->tx_abrt_xs_col;
  stats->tx_collisions     = bd_stats->tx_tot_retries;
  stats->tx_aborted_errors = bd_stats->tx_abrt_xs_col;
  stats->tx_carrier_errors = bd_stats->tx_lost_csrs;
  stats->tx_fifo_errors    = bd_stats->tx_dma_urun;

  return (struct net_device_stats*)stats;
}  

#ifdef NOT_USED
/*
 * This routine sets the ethernet address of the board
 */
static int e100_set_mac (device_t *dev, void *addr)
{
  bd_config_t     *bdp;
  bdd_t           *bddp;
  e100_eaddr_t    *eaddrp;
  struct sockaddr *p_sockaddr = (struct sockaddr *) addr;

  if (e100_debug > 2)
     printk ("e100_set_mac()\n");

  bdp  = dev->priv;
  bddp = bdp->bddp;
  eaddrp = (e100_eaddr_t*) p_sockaddr->sa_data;

  /* copy over the new mac address */
  memcpy (&bdp->eaddr, eaddrp, ETHERNET_ADDRESS_LENGTH);

  /* copy the new address into our dev structure */
  memcpy (&dev->dev_addr, eaddrp, ETHERNET_ADDRESS_LENGTH);

  /* Now call setup_iaaddr to set the mac address on the hardware */
  return e100_setup_iaaddr (bdp, eaddrp) == B_TRUE ? 0 : -1;
}
#endif

/*
 * This routine is called to add multicast addresses
 */
static void e100_set_multi (device_t * dev)
{
  struct dev_mc_list *mc_list;

  bd_config_t *bdp = dev->priv;
  bdd_t       *bddp = bdp->bddp;
  mltcst_cb_t *mcast_buff;
  cb_header_t *cb_hdr;
  DWORD        i;
  int          lock_flag, e100_retry;
  BYTE         rx_mode;

  bddp->promisc = (dev->flags & IFF_PROMISC);
  bddp->mulcst_enbl = ((dev->flags & IFF_ALLMULTI) || (dev->mc_count > MAX_MULTICAST_ADDRS));

  if (bddp->promisc)
       rx_mode = 3;
  else if (bddp->mulcst_enbl)
       rx_mode = 1;
  else rx_mode = 0;

  /* if rx mode has changed - reconfigure the chip */
  if (bddp->prev_rx_mode != rx_mode)
  {
    bddp->prev_rx_mode = rx_mode;
    e100_configure (bdp);
  }

  if (rx_mode)
     return;                     /* no need for Multicast Cmd */

  /* get the multicast CB */
  mcast_buff = &(bddp->pntcb->ntcb.multicast);
  cb_hdr = &(bddp->pntcb->ntcb.multicast.mc_cbhdr);

  /* initialize the multi cast command */
  cb_hdr->cb_status = 0;
  cb_hdr->cb_cmd = CB_MULTICAST;
  cb_hdr->cb_lnk_ptr = 0;

  /* now fill in the rest of the multicast command */
  *(WORD *) (&(mcast_buff->mc_count)) = cpu_to_le16 (dev->mc_count * 6);
  for (i = 0, mc_list = dev->mc_list; (i < dev->mc_count) && (i < MAX_MULTICAST_ADDRS); i++, mc_list = mc_list->next)
  {
    /* copy into the command */
    memcpy (&mcast_buff->mc_addr[i * ETHERNET_ADDRESS_LENGTH],
            (BYTE*)&mc_list->dmi_addr, ETHERNET_ADDRESS_LENGTH);
  }

  /* Wait for the SCB command word to clear before we set the general
   * pointer
   */
  if (e100_wait_scb (bdp) != B_TRUE)
  {
    printk ("Multicast setup failed\n");
    return;
  }

  /* If we have issued any transmits, then the CU will either be active,
   * or in the suspended state.  If the CU is active, then we wait for
   * it to be suspended.
   */
  if ((bddp->prev_cu_cmd == CB_TRANSMIT) || (bddp->prev_cu_cmd == CB_TRANSMIT_FIRST))
  {
    /* Wait for suspended state */
    e100_retry = E100_CMD_WAIT;
    while ((bddp->scbp->scb_status & SCB_CUS_MASK) == SCB_CUS_ACTIVE && e100_retry)
    {
      mdelay (20);
      e100_retry--;
    }
  }

  /* Update the command list pointer.  */
  bddp->scbp->scb_gen_ptr = bddp->nontx_paddr;

  /* store the command */
  bddp->prev_cu_cmd = CB_MULTICAST;

  /* Submit the Multicast Cmd to the chip, and wait for it to complete. */
  if (!e100_exec_poll_cmd (bdp))
     printk ("Multicast setup failed\n");
}


/***************************************************************************/
/***************************************************************************/
/*       Initialization Routines                                           */
/***************************************************************************/


/*
 * This routine is called when this driver is loaded. This is
 * the initialization routine which allocates memory
 * configures the adapter & determines the system
 * resources.
 */
int e100init (bd_config_t * bdp)
{
  device_t *dev = bdp->device;
  bdd_t *bddp = bdp->bddp;

  if (e100_debug > 2)
     printk ("e100init()\n");

  /* init private vars: never fails */
  e100_sw_init (bdp);

  /* Do a self test of the adapter */
  if (!e100_selftest (bdp))
  {
    printk ("selftest failed\n");
    return (0);
  }

  /* read the MAC address from the eprom */
  e100_rd_eaddr (bdp);

  /* read NIC's part number */
  e100_rd_pwa_no (bdp);

  /* Do the adapter initialization */
  if (!e100_hw_init (bdp, PORT_SOFTWARE_RESET))
  {
    printk ("hw init failed\n");
    return (0);
  }
  /* Disable interrupts on the PRO/100 card */
  e100_dis_intr (bdp);

  dev->base_addr = bdp->io_start;
  dev->mem_start = (DWORD) bddp->scbp;
  dev->mem_end = (DWORD) (bddp->scbp + sizeof (scb_t));

  if (e100_debug > 2)
     printk ("e100_init: end\n");
  return (1);
}


/*
 * This routine initializes all software structures. Sets up
 * the circular structures for the RFD's & TCB's. Allocates the per board
 * structure for storing adapter information. The CSR is also memory
 * mapped in this routine.
 */
boolean_t e100_sw_init (bd_config_t * bdp)
{
  bdd_t *bddp = bdp->bddp;      /* stores all adapter specific info */

  if (e100_debug > 2)
     printk ("e100_sw_init: [brd %d] begin.\n", bdp->bd_number);

#ifdef IANS
  bd_ans_drv_InitANS (bdp, bdp->iANSdata);
#endif

  bddp->bd_number    = bdp->bd_number;
  bddp->prev_cu_cmd  = CB_NULL;
  bddp->old_xmits    = 0;
  bddp->brdcst_dsbl  = 0;
  bddp->promisc      = 0;
  bddp->prev_rx_mode = 0;
  bddp->mulcst_enbl  = 0;

  /* Change for 82558 enhancement set num interrupts to 0 */
  bddp->num_cna_interrupts = ((e100_current_cna_backoff >= 0) &&
                              (e100_current_cna_backoff <= 0x1f)) ?
                              e100_current_cna_backoff : 0;

  /* Set the value for # of good xmits per underrun. the value assigned
   * here is an intelligent  suggested default. Nothing magical about it.
   */
  bddp->tx_per_underrun = DEFAULT_TX_PER_UNDERRUN;

  /* Initialize the checksum flag. If it is supported by the hardware It
   * will be truned on in the configure command.
   */
  bddp->checksum_offload_enabled = 0;

  /* get the default transmit threshold value */
  bddp->tx_thld = e100_tx_thld;

  /* get the EPROM size */
  bddp->EEpromSize = e100_GetEEpromSize (bdp);

  /* Initialize the phone line phy values */
  bddp->Phy82562EHSampleCount = 8;
  bddp->Phy82562EHSampleDelay = 35;
  bddp->Phy82562EHSampleFilter = 1;

  bdp->ZeroLockState = ZLOCK_INITIAL;

  return (1);
}


/*
 * This routine performs a reset on the adapter, and configures
 * the adapter.  This includes configuring the 82557 LAN
 * controller, validating and setting the node address, detecting
 * and configuring the Phy chip on the adapter, and initializing
 * all of the on chip counters.
 */
boolean_t e100_hw_init (bd_config_t *bdp, DWORD reset_cmd)
{
  bdd_t *bddp = (bdd_t *) bdp->bddp;

  if (e100_debug > 2)
     printk ("e100_hw_init: begin\n");

  /* Detect the serial component, and set up the Phy if necessary */
  if (!e100_phydetect (bdp))
     return (B_FALSE);

  e100_fix_polarity (bdp);

  /* Issue a software reset to the e100 */
  e100_sw_reset (bdp, reset_cmd);

  /* Load the CU BASE (set to 0, because we use linear mode) */
  bddp->prev_cu_cmd = SCB_CUC_LOAD_BASE;
  bddp->scbp->scb_gen_ptr = 0;
  e100_exec_cmd (bdp, SCB_CUC_LOAD_BASE);

  /* Wait for the SCB command word to clear before we set the general
   * pointer
   */
  if (e100_wait_scb (bdp) != B_TRUE)
    return (B_FALSE);

  /* Load the RU BASE (set to 0, because we use linear mode) */
  bddp->scbp->scb_gen_ptr = 0;
  bddp->prev_cu_cmd = SCB_RUC_LOAD_BASE;
  e100_exec_cmd (bdp, SCB_RUC_LOAD_BASE);

  /* Configure the adapter in non promiscious mode */
  if (!e100_configure (bdp))
     return (B_FALSE);

  /* Load interrupt microcode  */
  if (e100_load_microcode (bdp, bddp->rev_id) == B_TRUE)
  {
    bdp->flags |= DF_UCODE_LOADED;
    mdelay (1000);
  }

  if (!e100_setup_iaaddr (bdp, &bdp->eaddr))
     return (B_FALSE);

  /* Clear the internal counters */
  if (!e100_clr_cntrs (bdp))
     return (B_FALSE);

  /* Change for 82558 enhancement
   * If 82558/9 and if the user has enabled flow control, set up the
   * Flow Control Reg. in the CSR
   */
  if ((bddp->flags & IS_BACHELOR) && (e100_flow_control_enable))
  {
    bddp->scbp->scb_ext.d101_scb.scb_fc_thld = DFLT_FC_THLD;
    bddp->scbp->scb_ext.d101_scb.scb_fc_xon_xoff = DFLT_FC_CMD;
  }
  return (B_TRUE);
}


/*
 * This routine arranges the contigiously allocated TCB's
 * in a circular list . Also does the one time initialization of the TCBs.
 *
 * Input : 
 *    head - Pointer to head of the allocated TCBs'.
 *    qlen - Number of elements in the queue.
 *    bdp    - Ptr to this card's e100_bdconfig structure
 */
static void e100_setup_tcb_pool (ptcb_t head, DWORD qlen, bd_config_t *bdp)
{
  int    ele_no;
  ptcb_t pcurr_tcb;             /* point to current tcb */
  ptcb_t pnext_tcb;             /* point to next tcb */
  DWORD  next_paddr;            /* the next phys addr */
  bdd_t *bddp = bdp->bddp;

  pcurr_tcb = head;
  pnext_tcb = head;

  for (ele_no = 0, next_paddr = (DWORD)bddp->tcb_paddr; ele_no < qlen;
       ele_no++, pcurr_tcb++)
  {
    /* set the phys addr for this TCB, next_paddr has not incr. yet */
    pcurr_tcb->tcb_paddr = next_paddr;
    pnext_tcb++;                       /* point to next tcb */
    next_paddr += sizeof (tcb_t);

    /* set the link to next tcb */
    if (ele_no == (qlen - 1))
         pcurr_tcb->tcb_hdr.cb_lnk_ptr = (DWORD) bddp->tcb_paddr;
    else pcurr_tcb->tcb_hdr.cb_lnk_ptr = next_paddr;

    /* initialize TCB status to zero */
    pcurr_tcb->tcb_hdr.cb_status = 0;

    /* initialize TCB skb pointer to NULL */
    pcurr_tcb->tcb_skb = NULL;

    /* initialize TCB command */
    pcurr_tcb->tcb_hdr.cb_cmd = CB_EL_BIT |    /* Last in CBL */
                                CB_TRANSMIT |  /* Xmit cmd */
                                CB_TX_SF_BIT;  /* set flexible mod  */

    /* initialize the early xmit threshold */
    pcurr_tcb->tcb_thrshld = bddp->tx_thld;
  }
}


/***************************************************************************/
/***************************************************************************/
/*       Memory Management Routines                                        */
/***************************************************************************/

/*
 * This routine allocates memory for the driver. Memory allocated is
 * for the following structures
 *  - bdp, bddp
 *  - error count structure for adapter statistics
 */
bd_config_t *e100_alloc_space (void)
{
  bd_config_t *bdp, *temp_bd;
  bdd_t       *bddp;

  if (e100_debug)
     printk ("e100_alloc_space: begin\n");

  /* allocate space for the private structures */
  if ((bdp = (bd_config_t *) k_calloc (sizeof(bd_config_t),1)) == NULL)
     return (NULL);

  /* link the bdp's, if needed */
  if (!e100_first)
  {                             /* do we have at least 1 already alloc'd? */
    e100_first = bdp;
    bdp->bd_number = 0;
    bdp->bd_next = NULL;
    bdp->bd_prev = NULL;
  }
  else
  {
    /* No, so find last in list and link the new one in */
    temp_bd = e100_first;
    bdp->bd_number = 1;         /* it is at least 1 */
    while (temp_bd->bd_next != NULL)
    {
      temp_bd = (bd_config_t *) temp_bd->bd_next;
      bdp->bd_number++;         /* set the board number */
    }
    temp_bd->bd_next = bdp;
    bdp->bd_next = NULL;
    bdp->bd_prev = temp_bd;
  }

#ifdef IANS
  if (!(bdp->iANSdata = k_calloc (sizeof(iANSsupport_t), 1)))
     return (NULL);
#endif

  /* Allocate the bdd_t structure */
  if (!(bddp = (bdd_t *) k_calloc (sizeof(bdd_t), 1)))
     return (NULL);

  bdp->bddp = bddp;             /* Hang it of the bdp */
  bddp->bdp = bdp;              /* point back to the bdp */

  /* allocate space for selt test results */
  if (!(bddp->pselftest = k_malloc (sizeof(self_test_t))))
     return (NULL);

  bddp->selftest_paddr = virt_to_bus (bddp->pselftest);

  /* allocate space for 82557 register dump area */
  if (!(bddp->pdump_area = k_malloc (sizeof(dump_area_t))))
     return (NULL);

  bddp->dump_paddr = virt_to_bus (bddp->pdump_area);

  /* allocate space for 82557 adapter statistics area */
  if (!(bddp->pstats_counters = k_malloc (sizeof (err_cntr_t))))
     return (NULL);

  bddp->stat_cnt_paddr = virt_to_bus (bddp->pstats_counters);

  /* allocate space for non transmit cb commands */
  if (!(bddp->pntcb = kmalloc (sizeof (nxmit_cb_t))))
     return (NULL);

  bddp->nontx_paddr = virt_to_bus (bddp->pntcb);

  /* allocate space for stats results */
  if (!(bddp->perr_stats = k_calloc (sizeof (err_stats_t),1)))
     return (NULL);

  bddp->estat_paddr = virt_to_bus (bddp->perr_stats);
  if (e100_debug)
     printk ("e100_alloc_space: end\n");
  return (bdp);
}


/*
 * This routine allocates memory for the transmit descriptors.
 */
int e100_alloc_tcbs (bd_config_t * bdp)
{
  bdd_t *bddp = bdp->bddp;
  int    stcb = sizeof (tcb_t) * TxDescriptors[bdp->bd_number];
  int    stbd = sizeof (tbd_t) * TxDescriptors[bdp->bd_number];

  /* allocate space for the TCBs */
  if (!(bddp->tcb_pool.data = k_calloc (stcb, 1)))
     return (0);

  bddp->tcb_paddr = virt_to_bus (bddp->tcb_pool.data);

  /* there is ALWAYS only going to 1 phys frag
   * tbd_paddr is a phys_addr but stored as an unsigned long
   */
  if (!(bddp->tbd_pool.data = k_calloc (stbd, 1)))
     return (0);

  bddp->tbd_paddr = virt_to_bus (bddp->tbd_pool.data);
  return (1);
}

void e100_free_tbds (bdd_t * bddp)
{
  COND_FREE (bddp->tcb_pool.data);
  COND_FREE (bddp->tbd_pool.data);
}

/*
 * This routine frees all the memory allocated by "alloc_space".
 * and e100_alloc_tbds.
 */
static void e100_dealloc_space (bd_config_t * bdp)
{
  bdd_t *bddp = bdp->bddp;

  if (e100_debug > 2)
     printk ("e100_dealloc_space, bdp = 0x%p\n", bdp);

  /* do we have valid board structure? */
  if (!bdp)
     return;

#ifdef IANS
  COND_FREE (bdp->iANSdata);
#endif

  /* is bddp valid? */
  if (bdp->bddp)
  {
    COND_FREE (bddp->pselftest);
    COND_FREE (bddp->pdump_area);
    COND_FREE (bddp->pstats_counters);
    COND_FREE (bddp->pntcb);
    COND_FREE (bddp->perr_stats);

    bddp->tcb_paddr = 0;
    bddp->tbd_paddr = 0;
    bddp->selftest_paddr = 0;
    bddp->dump_paddr = 0;
    bddp->stat_cnt_paddr = 0;
    bddp->nontx_paddr = 0;
    bddp->estat_paddr = 0;

    k_free (bdp->bddp);
  }
  /* un-link the bdp from the linked list */
  if (bdp == e100_first)
  {
    e100_first = (bd_config_t *) bdp->bd_next;
    if (bdp->bd_next)
      ((bd_config_t *) bdp->bd_next)->bd_prev = NULL;
  }
  else
  {
    if (bdp->bd_next)
      ((bd_config_t *) bdp->bd_next)->bd_prev = bdp->bd_prev;
    if (bdp->bd_prev)
      ((bd_config_t *) bdp->bd_prev)->bd_next = bdp->bd_next;
  }
  k_free (bdp);
}

void e100_free_rfd_pool (bdd_t * bddp)
{
  sk_buff_t *skb, *nskb;
  int       j;

  while (bddp->rfd_head)
  {
    skb  = bddp->rfd_head;
    nskb = (sk_buff_t*) RFD_POINTER (skb, bddp)->next;
    dev_kfree_skb_irq (skb);
    bddp->rfd_head = nskb;
  }
  bddp->rfd_head = NULL;
  bddp->rfd_tail = NULL;
}

/* 
 * allocates initial pool of skb which holds both rfd and data
 */
static sk_buff_t *e100_alloc_rfd_pool (bdd_t * bddp)
{
  sk_buff_t *nskb;

  bddp->rfd_tail = NULL;
  bddp->rfd_head = NULL;
  bddp->skb_req = RxDescriptors[bddp->bd_number];

  ALLOC_SKBS (bddp, nskb);
  return bddp->rfd_head;
}  


/*****************************************************************************/
/*****************************************************************************/
/*      Run Time Functions                                                   */
/*****************************************************************************/

/*****************************************************************************
 * Name:        e100_watchdog
 *
 * Description: This routine updates our statitics and refreshs the txthld
 *                  value.
 *
 * Born on Date:    1/5/00
 *
 * Arguments:
 *            dev - device structure
 *
 * Returns:
 *      (none)
 *
 * Modification log:
 * Date      Who  Description
 * --------  ---  --------------------------------------------------------
 *
 *****************************************************************************/
void e100_watchdog (device_t * dev)
{
  bd_config_t *bdp = dev->priv;
  bdd_t *bddp = bdp->bddp;

  /* Update the statistics needed by the upper interface */
  e100_dump_stats_cntrs (bdp);

  /* Now adjust our dynamic tx threshold value */
  e100_refresh_txthld (bdp);

  /* Now if we are on a 557 and we havn't received any frames then we
   * should issue a multicast command to reset the RU */
  if ((bddp->rev_id < D101A4_REV_ID) && (!(bddp->flags & IS_ICH)))
  {
    /* if we haven't received any frames then issue the multicast
     * command */
    if (bddp->pstats_counters->rcv_gd_frames == 0)
    {
      e100_set_multi (dev);
    }
  }

  e100_phy_check (bdp);


#ifdef IANS
  /* Now do the ANS stuff */
  if ((ANS_PRIVATE_DATA_FIELD (bdp)->iANS_status == IANS_COMMUNICATION_UP) &&
      (ANS_PRIVATE_DATA_FIELD (bdp)->reporting_mode == IANS_STATUS_REPORTING_ON))
  {
    bd_ans_os_Watchdog (bddp->bdp->device, bddp->bdp);
  }
#endif

  e100_handle_zero_lock_state (bdp);

  /* relaunch watchdog timer in 2 sec */
  init_timer (&bdp->timer_id);
  bdp->timer_id.expires = bdp->timer_val = jiffies + (2 * HZ);
  bdp->timer_id.data = (DWORD) dev;
  bdp->timer_id.function = (void *) &e100_watchdog;
  add_timer (&bdp->timer_id);

  if (bddp->rfd_head == NULL)
    e100_trigger_SWI (bdp);

  return;
}


/* 
 * Procedure:   e100intr
 *
 * Description: This routine is the ISR for the e100 board. It services
 *        the RX & TX queues & starts the RU if it has stopped due
 *        to no resources.
 *
 * Returns:
 *      NONE
 *
 */
void e100intr (int irq)
{
  device_t *dev = irq2dev_map[irq];
  bd_config_t *bdp;
  bdd_t *bddp;
  WORD intr_status;

  bdp = dev->priv;
  bddp = bdp->bddp;

  intr_status = bddp->scbp->scb_status & SCB_STATUS_ACK_MASK;
  if (!intr_status)
    return;                     /* not our intr - return */
  bddp->scbp->scb_status = intr_status; /* ack intrs */
  e100_dis_intr (bdp);          /* prevent intr from happen on another CPU */

  /* increment intr counter for CNA delay adjustment (82558/9 only) */
  if ((bddp->flags & IS_BACHELOR) && (intr_status & SCB_STATUS_ACK_CNA))
    bddp->num_cna_interrupts++;

  /* SWI intr (triggered by watchdog) is signal to allocate new skb buffers */
  if (intr_status & SCB_STATUS_ACK_SWI)
  {
    sk_buff_t *nskb;

    ALLOC_SKBS (bddp, nskb);
  }
  /* do recv work if any */
  if (intr_status & (SCB_STATUS_ACK_FR | SCB_STATUS_ACK_RNR | SCB_STATUS_ACK_SWI))
  {
    e100_rx_srv (bdp);
    /* restart the RU if it has stopped */
    if ((bddp->scbp->scb_status & SCB_RUS_MASK) != SCB_RUS_READY)
      e100_start_ru (bdp);
  }

  /* clean up after tx'ed packets */
  if (intr_status & (SCB_STATUS_ACK_CNA | SCB_STATUS_ACK_CX))
  {
    bdp->tx_count = 0;          /* restart tx interrupt batch count */
    e100_tx_srv (bdp);
  }

  e100_enbl_intr (bdp);
}


/* 
 * Procedure:   e100_tx_srv
 *
 * Description: This routine services the TX queues. It reclaims the
 *        TCB's & TBD's & other resources used during the transmit
 *        of this buffer. It is called from the ISR.
 *
 * Arguments:
 *      bdp - Ptr to this card's e100_bdconfig structure
 *
 * Returns:
 *    NONE
 */
void e100_tx_srv (bd_config_t * bdp)
{
  bdd_t *bddp = bdp->bddp;
  device_t *dev = bdp->device;
  buf_pool_t *tcb_poolp;
  DWORD tcb_head = 0;
  tcb_t *tcbp;
  int loop_cnt = 0;

  boolean_t last_tcb_served = 0;

  tcb_poolp = &bddp->tcb_pool;
  tcb_head  = tcb_poolp->head;
  tcbp  = tcb_poolp->data;
  tcbp += tcb_head;

  while ((tcbp->tcb_hdr.cb_status & CB_STATUS_COMPLETE) && (loop_cnt < TxDescriptors[bdp->bd_number]))
  {


    if (IS_IT_GAP (tcb_poolp))
    {
      if (tcbp->tcb_skb)
      {
        dev_kfree_skb_irq (tcbp->tcb_skb);
        tcbp->tcb_skb = NULL;
      }

      /* move head to next buffer & service it */
      if ((tcb_head + 1) >= TxDescriptors[bdp->bd_number])
        tcbp = tcb_poolp->data;
      else
        tcbp++;

      if (tcbp->tcb_hdr.cb_status & CB_STATUS_COMPLETE)
      {
        last_tcb_served = 1;
        if (tcbp->tcb_skb)
        {
          dev_kfree_skb_irq (tcbp->tcb_skb);
          tcbp->tcb_skb = NULL;
        }
      }
      /* Make sure to clear the out of resource condition */
      if (netif_queue_stopped (dev))
      {
#ifdef IANS
        if (ANS_PRIVATE_DATA_FIELD (bdp)->iANS_status == IANS_COMMUNICATION_UP)
        {
          if (ans_notify)
            ans_notify (dev, IANS_IND_XMIT_QUEUE_READY);
        }

#endif
        netif_wake_queue (dev);
      }

      break;
    }

    /* update statistics & free this msg */
    /* xmits is a counter for update CID. */
    bddp->xmits++;

    if (tcbp->tcb_skb)
    {
      dev_kfree_skb_irq (tcbp->tcb_skb);
      tcbp->tcb_skb = NULL;
    }

    tcbp->tcb_hdr.cb_status = 0;

    if (bddp->flags & USE_IPCB)
    {
      /* clean out the ipcb fields */
      (tcbp->tcbu).ipcb.scheduling = 0;
      (tcbp->tcbu).ipcb.ip_activation = IPCB_IP_ACTIVATION_DEFAULT;
      (tcbp->tcbu).ipcb.vlan = 0;
      (tcbp->tcbu).ipcb.ip_header_offset = 0;
      (tcbp->tcbu).ipcb.tcp_header_offset = 0;
      (tcbp->tcbu).ipcb.tbd_sec_addr.tbd_zero_address = 0;
      (tcbp->tcbu).ipcb.tbd_sec_size.tbd_zero_size = 0;
      (tcbp->tcbu).ipcb.total_tcp_payload = 0;
    }

    /* 
     * If 82558/9 and if enhanced_tx_enabled, clean up the extended
     * TCB fields
     */
    else if ((bddp->flags & IS_BACHELOR) && (e100_enhanced_tx_enable))
    {
      /* Note: the tbd0 and tbd1 buf_addrs are physical addresses
       * stored * in an unsigned long. */
      (tcbp->tcbu).tcb_ext.tbd0_buf_addr = 0;
      (tcbp->tcbu).tcb_ext.tbd0_buf_cnt = 0;
      (tcbp->tcbu).tcb_ext.tbd1_buf_addr = 0;
      (tcbp->tcbu).tcb_ext.tbd1_buf_cnt = 0;
    }

    /* clear the out of resource condition */
    if (netif_queue_stopped (dev))
    {
#ifdef IANS
      if (ANS_PRIVATE_DATA_FIELD (bdp)->iANS_status == IANS_COMMUNICATION_UP)
      {
        if (ans_notify)
          ans_notify (dev, IANS_IND_XMIT_QUEUE_READY);
      }

#endif
      netif_wake_queue (dev);
    }

    /* move head to next buffer & service it */
    tcb_head++;
    /* check for wrap condition */
    if (tcb_head >= TxDescriptors[bdp->bd_number])
    {
      tcb_head = 0;
      tcbp = tcb_poolp->data;
    }
    else
      tcbp++;
    tcb_poolp->head = tcb_head;
    tcb_poolp->count++;

    loop_cnt++;


    tcb_poolp->head = tcb_head;

  }                             /* end of while */


  tcb_poolp->head = tcb_head;

  if ((tcb_head != tcb_poolp->tail) && !last_tcb_served)
  {
    if ((bddp->scbp->scb_status & SCB_CUS_MASK) == SCB_CUS_IDLE)
    {
      printk ("CU idle while %d tcbs to tx\n", tcb_poolp->head - tcb_poolp->tail);
    }
  }

#if (DEBUG_TX)
  printk ("e100_tx_srv: exiting\n");
#endif
  return;
}


/* 
 * Procedure:   e100_rx_srv
 *
 * Description: This routine processes the RX interrupt & services the 
 *        RX queues. For each successful RFD, it allocates a new msg
 *        block, links that into the RFD list, & sends the old msg upstream.
 *        The new RFD is then put at the end of the free list of RFD's.
 *
 *
 * Arguments:
 *      bdp - Ptr to this card's e100_bdconfig structure
 *
 * Returns:
 *    NONE
 */
void e100_rx_srv (bd_config_t * bdp)
{
  bdd_t *bddp = bdp->bddp;
  rfd_t *rfdp;                  /* new rfd, received rfd */
  int i;
  WORD rfd_status;
  sk_buff_t *skb, *nskb;
  device_t *dev;
  net_device_stats_t *stats;
  ip_v4_header *IPPacket;
  int HeaderOffset = 0;


  dev = bdp->device;
  stats = &(bddp->net_stats);

  if (e100_debug > 2)
  {
    printk ("e100_rx_srv - B. b %d  scb_status=0x%x\n", bdp->bd_number, bddp->scbp->scb_status);
    printk ("     rfd_head = 0x%p\n", bddp->rfd_head);
  }

  /* current design of rx is as following:
   * 1. socket buffer (skb) used to pass network packet to upper layer
   * 2. all HW host memory structures (like RFDs, RBDs and data buffers)
   *    are placed in a skb's data room
   * 3. when rx process is complete, we change skb internal pointers to exclude
   *    from data area all unrelated things (RFD, RDB) and to leave
   *    just rx'ed packet netto
   * 4. for each skb passed to upper layer, new one is allocated instead.
   * 5. if no skb left, in 2 sec another atempt to allocate skbs will be made
   *    (watchdog trigger SWI intr and isr should allocate new skbs)
   */

  for (i = 0; i < RxDescriptors[bdp->bd_number]; i++)
  {
    skb = bddp->rfd_head;
    if (skb == NULL)            /* no buffers left - exit and watchdog take care later */
      return;

    rfdp = RFD_POINTER (skb, bddp); /* locate RFD within skb */
    rfd_status = rfdp->rfd_header.cb_status; /* get RFD's status */
    if (!(rfd_status & RFD_STATUS_COMPLETE)) /* does not contains data yet - exit */
      return;

    /* to allow manipulation with current skb we need to advance rfd head */
    bddp->rfd_head = rfdp->next;

    /* do not free badly recieved packet - move it to the end of skb list for reuse */
    if (!(rfd_status & RFD_STATUS_OK))
    {
      e100_add_skb_to_end (bddp, skb);
      continue;
    }
    bddp->skb_req++;            /* incr number of requested skbs */
    ALLOC_SKBS (bddp, nskb);    /* and get them */

    /* set packet size, excluding checksum (2 last bytes) if it is present */
    if (bddp->checksum_offload_enabled && (bddp->rev_id < D102_REV_ID))
      skb_put (skb, (int) (rfdp->rfd_act_cnt & 0x3fff) - 2);
    else
      skb_put (skb, (int) (rfdp->rfd_act_cnt & 0x3fff));

    /* set the checksum info */
    skb->ip_summed = CHECKSUM_NONE;
    if (bddp->checksum_offload_enabled)
    {
      if (bddp->rev_id >= D102_REV_ID)
      {
        e100_D102_check_checksum (bdp, skb);
      }
      else if ((IPPacket = e100_check_for_ip (&HeaderOffset, skb)))
      {
        e100_calculate_checksum (IPPacket, HeaderOffset, skb, rfdp->rfd_act_cnt & 0x3FFF);
      }
    }  

#ifdef IANS
    /* Before we give it to the stack lets let ANS process it */
    if (ANS_PRIVATE_DATA_FIELD (bdp)->iANS_status == IANS_COMMUNICATION_UP)
    {
      if (bd_ans_os_Receive (bdp, RFD_POINTER (skb, bddp), skb) == BD_ANS_FAILURE)
      { 
        dev_kfree_skb_irq (skb);
        continue;
      }
    }
    else
#endif
    {
      /* set the protocol */
      skb->protocol = eth_type_trans (skb, dev);
    }

    stats->rx_bytes += skb->len;
    netif_rx (skb);
  }
}


/* 
 * Procedure:   e100_refresh_txthld
 *
 * Arguments:
 *      bdp - Ptr to this card's e100_bdconfig structure
 *
 * Returns:
 *    -     NONE
 */
void e100_refresh_txthld (bd_config_t * bdp)
{
  bdd_t *bddp = bdp->bddp;

  /* as long as tx_per_underrun is not 0, we can go about dynamically *
   * adjusting the xmit threshold. we stop doing that & resort to defaults
   * * once the adjustments become meaningless. the value is adjusted by *
   * dumping the error counters & checking the # of xmit underrun errors *
   * we've had. */
  if (bddp->tx_per_underrun)
  {
    /* We are going to last values dumped from the dump statistics
     * command */
    if (bddp->pstats_counters->xmt_gd_frames)
    {
      if (bddp->pstats_counters->xmt_uruns)
      {
        /* 
         * if we have had more than one underrun per "DEFAULT #
         * OF XMITS ALLOWED PER UNDERRUN" good xmits, raise the
         * THRESHOLD.
         */
        if ((bddp->pstats_counters->xmt_gd_frames / bddp->pstats_counters->xmt_uruns) < bddp->tx_per_underrun)
        {
          bddp->tx_thld += 3;
          if (DEBUG_TX)
            printk ("e100_refresh_txthld: B %d THOLD + %d\n", bdp->bd_number, bddp->tx_thld);
        }
      }

      /* 
       * if we've had less than one underrun per the DEFAULT number of
       * of good xmits allowed, lower the THOLD but not less than 0 
       */
      if (bddp->pstats_counters->xmt_gd_frames > bddp->tx_per_underrun)
      {
        bddp->tx_thld--;

        if (bddp->tx_thld < 6)
          bddp->tx_thld = 6;

        if (DEBUG_TX)
          printk ("e100_refresh_txthld: b %d THOLD -- %d\n", bdp->bd_number, bddp->tx_thld);

      }
    }

    /* end good xmits */
    /* 
     * * if our adjustments are becoming unresonable, stop adjusting &
     * resort * to defaults & pray. A THOLD value > 190 means that the
     * adapter will * wait for 190*8=1520 bytes in TX FIFO before it
     * starts xmit. Since * MTU is 1514, it doesn't make any sense for
     * further increase. */
    if (bddp->tx_thld >= 190)
    {
      bddp->tx_per_underrun = 0;
      bddp->tx_thld = 189;
      if (DEBUG_TX)
        printk ("e100_refresh_txthld: b %d stop at 189\n", bdp->bd_number);
    }
  }                             /* end underrun check */
}


/* 
 * This routine prepare a buffer for transmission. It checks
 * the message length for the appropiate size. It picks
 * up a free tcb from the TCB pool & sets up the corresponding
 * TBD's. If number of fragments are more the the # of TBD/TCB
 * it copies all the fragments in a coalesce buffer.
 *
 * Arguments:
 *      bdp    - Ptr to this card's e100_bdconfig structure
 *      ksb    - Ptr to the skb to send
 *
 * Returns:
 *     ptcb   - Ptr to the prepared TCB
 */
static ptcb_t e100_prepare_xmit_buff (bd_config_t * bdp, sk_buff_t * skb)
{
  bdd_t      *bddp = bdp->bddp;
  buf_pool_t *tcb_poolp;
  buf_pool_t *tbd_poolp;
  tcb_t      *tcbp, *prev_tcbp;
  tbd_t      *tbdp;
  WORD        txcommand;

  tcb_poolp = &bddp->tcb_pool;
  tbd_poolp = &bddp->tbd_pool;
  tcbp      = tcb_poolp->data;
  tbdp      = tbd_poolp->data;

  /* get the TCB & the TBD we may be using for this MSG */
  tcbp += TCB_TO_USE (tcb_poolp);
  tbdp += (TCB_TO_USE (tcb_poolp));

  if (bddp->flags & USE_IPCB)
  {
    (tcbp->tcbu).ipcb.ip_activation = IPCB_IP_ACTIVATION_DEFAULT;
    txcommand = CB_IPCB_TRANSMIT | CB_S_BIT | CB_TX_SF_BIT;
  }
  else
    txcommand = CB_TRANSMIT | CB_S_BIT | CB_TX_SF_BIT;

#ifdef IANS
  if (ANS_PRIVATE_DATA_FIELD (bdp)->iANS_status == IANS_COMMUNICATION_UP)
     bd_ans_os_Transmit (bdp, tcbp, &skb);
#endif

  tcbp->tcb_cnt = 0;
  tcbp->tcb_tbd_num = 1;
  tcbp->tcb_thrshld = bddp->tx_thld;
  tcbp->tcb_tbd_ptr = bddp->tbd_paddr + (TCB_TO_USE (tcb_poolp) * sizeof (tbd_t));
  tcbp->tcb_hdr.cb_cmd = txcommand;


  /* set the I bit on the modulo tcbs, so we will get an interrupt to
   * clean things up
   */
  if (!(++bdp->tx_count % e100_batch_tx_frames))
     tcbp->tcb_hdr.cb_cmd |= CB_I_BIT;

  /* save a pointer to the skb to free it later */
  tcbp->tcb_skb = skb;

  /* set the data size of the SKB */
  tcbp->tcb_msgsz = skb->len;
  if (bddp->flags & USE_IPCB)
  {
    /* setup the ipcb fields */
    (tcbp->tcbu).ipcb.tbd_sec_addr.tbd_zero_address = virt_to_bus (skb->data);
    (tcbp->tcbu).ipcb.tbd_sec_size.tbd_zero_size = skb->len;
  }
  else
  {
    /* copy the phys addrs and len to the tbds */
    tbdp->tbd_buf_addr = virt_to_bus (skb->data);
    tbdp->tbd_buf_cnt = skb->len;
  }

  /* Clear the S-Bit of the Previous Command */
  prev_tcbp = tcb_poolp->data;
  prev_tcbp += PREV_TCB_USED (tcb_poolp);
  prev_tcbp->tcb_hdr.cb_cmd &= ~CB_S_BIT;

  /* update the tail */
  tcb_poolp->tail = NEXT_TCB_TOUSE (tcb_poolp->tail);

#if (DEBUG_TX)
  printk ("prepare_xmit_buff: Frame Data:\n");
  for (i = 0; i < skb->len; i++)
      printk (" 0x%x", skb->data[i]);
  printk ("\n");
#endif

  /* start the CU if needed */
  e100_cu_start (bdp, tcbp);

  if (tcb_poolp->count)
    tcb_poolp->count--;

  return (tcbp);
}


/* 
 * This is a 82558/9 specific routine.
 * This sets up the extended TCBs and dynamically chains TBDs.
 * This routine prepare a buffer for transmission. It checks
 * the message length for the appropiate size. It picks
 * up a free tcb from the TCB pool & sets up the corresponding
 * TBD's. If number of fragments are more the the # of TBD/TCB
 * it copies all the fragments in a coalesce buffer.
 *
 * Arguments:
 *      bdp    - Ptr to this card's e100_bdconfig structure
 *      skb    - skb to send.
 *
 * Returns:
 *  ptcb   - Ptr to the prepared TCB
 */
static ptcb_t e100_prepare_ext_xmit_buff (bd_config_t * bdp, sk_buff_t * skb)
{
  bdd_t      *bddp;
  buf_pool_t *tcb_poolp;
  tcb_t      *tcbp;
  tcb_t      *prev_tcbp;

  bddp      = (bdd_t*) bdp->bddp;
  tcb_poolp = &bddp->tcb_pool;
  tcbp      = tcb_poolp->data;

  /* get the TCB & the TBD we may be using for this MSG
   * since Linux only use 1 physical address the number of tdbs use is 1
   * which will fit into the extended tcb
   */
  tcbp += TCB_TO_USE (tcb_poolp);

#ifdef IANS
  if (ANS_PRIVATE_DATA_FIELD (bdp)->iANS_status == IANS_COMMUNICATION_UP)
     bd_ans_os_Transmit (bdp, tcbp, &skb);
#endif

#if (DEBUG_TX)
  printk ("e100_prepare_ext_xmit_buff:  num tcbs avail = 0x%x\n",
          tcb_poolp->count);
  printk ("   - B. b %d tcb[h=0x%x t=0x%x c=0x%x ] scb_status 0x%x\n",
          bdp->bd_number, tcb_poolp->head, tcb_poolp->tail, tcb_poolp->count,
          bddp->scbp->scb_status);
  printk ("   - TCB_TO_USE = 0x%x\n", TCB_TO_USE (tcb_poolp));
#endif

  tcbp->tcb_hdr.cb_status = 0;
  tcbp->tcb_cnt           = 0;
  tcbp->tcb_tbd_num       = 0xff;
  tcbp->tcb_thrshld       = bddp->tx_thld;
  tcbp->tcb_tbd_ptr       = bddp->tbd_paddr + (TCB_TO_USE(tcb_poolp) * sizeof(tbd_t));
  tcbp->tcb_hdr.cb_cmd    = CB_S_BIT | CB_TRANSMIT | CB_TX_SF_BIT;

#if (DEBUG_TX)
  printk ("     tcbp = 0x%x, skb = 0x%x\n", tcbp, skb);
  printk ("     skb_data = 0x%x, skb_len = 0x%x\n", skb->data, skb->len);
#endif

  /* set the I bit on the modulo tcbs, so we will get an interrupt to
   * clean things up
   */
  if (!(++bdp->tx_count % e100_batch_tx_frames))
  {
    tcbp->tcb_hdr.cb_cmd |= CB_I_BIT;
#if (DEBUG_TX)
    printk ("     setting the I_bit\n");
#endif
  }

  /* set the CNA backoff */
  tcbp->tcb_hdr.cb_cmd |= bddp->current_cna_backoff << 8;

  (tcbp->tcbu).tcb_ext.tbd0_buf_cnt  = 0 | CB_EL_BIT;
  (tcbp->tcbu).tcb_ext.tbd1_buf_cnt  = 0 | CB_EL_BIT;
  (tcbp->tcbu).tcb_ext.tbd0_buf_addr = 0;
  (tcbp->tcbu).tcb_ext.tbd1_buf_addr = 0;

  /* save a pointer to the skb to free it later */
  tcbp->tcb_skb = skb;

  /* set the data size of the SKB */
  tcbp->tcb_msgsz = skb->len;

  /* set the ext tbd to the skb */
  (tcbp->tcbu).tcb_ext.tbd0_buf_addr = virt_to_bus (skb->data);
  (tcbp->tcbu).tcb_ext.tbd0_buf_cnt = skb->len;

#if (DEBUG_TX)
  printk ("Data: cmd = 0x%x\n", tcbp->tcb_hdr.cb_cmd);
  for (i = 0; i < skb->len; i++)
      printk (" 0x%x", skb->data[i]);
  printk ("\n");
#endif

  /* set the 2nd tbd to end the chain */
  (tcbp->tcbu).tcb_ext.tbd1_buf_addr = 0xFFFFFFFF;
  (tcbp->tcbu).tcb_ext.tbd1_buf_cnt = 0 | CB_EL_BIT;

#if (DEBUG_TX)
  printk ("     status = 0x%x, tcb_cnt = 0x%x, tcb_tbd_num = 0x%x\n",
          tcbp->tcb_hdr.cb_status, tcbp->tcb_cnt, tcbp->tcb_tbd_num);
  printk ("     tcb_thrshld = 0x%x, tcb_tbd_ptr = 0x%x, cb_cmd= 0x%x\n",
          tcbp->tcb_thrshld, tcbp->tcb_tbd_ptr, tcbp->tcb_hdr.cb_cmd);
  printk ("     tbd0_buf_addr = 0x%x, tbd0_buf_cnt = 0x%x\n",
          (tcbp->tcbu).tcb_ext.tbd0_buf_addr, (tcbp->tcbu).tcb_ext.tbd0_buf_cnt);
  printk ("     tbd1_buf_addr = 0x%x, tbd1_buf_cnt = 0x%x\n",
          (tcbp->tcbu).tcb_ext.tbd1_buf_addr, (tcbp->tcbu).tcb_ext.tbd1_buf_cnt);
  printk ("   - PREV_TCB_USED = 0x%x\n", PREV_TCB_USED (tcb_poolp));
  printk ("   - tcb_paddr = 0x%x\n", tcbp->tcb_paddr);

  printk ("TCB:\n");
  for (i = 0, temp_val = (DWORD*)tcbp; i < sizeof(tcb_t) / 4; i++)
      printk (" 0x%x", temp_val[i]);
  printk ("\n");
#endif

  /* clear the S-BIT on the previous tcb */
  prev_tcbp = tcb_poolp->data;
  prev_tcbp += PREV_TCB_USED (tcb_poolp);
  prev_tcbp->tcb_hdr.cb_cmd &= ~CB_S_BIT;

  /* start the CU if needed */
  e100_cu_start (bdp, tcbp);

  /* update the tail */
  tcb_poolp->tail = NEXT_TCB_TOUSE (tcb_poolp->tail);

  if (tcb_poolp->count)
    tcb_poolp->count--;

#if (DEBUG_TX)
  printk ("     tail = 0x%x\n", tcb_poolp->tail);
#endif
  return (tcbp);
}
  
/* 
 * This routine issues a CU Start or CU Resume command
 * to the 82558/9. This routine was added because the prepare_ext_xmit_buff
 * takes advantage of the 82558/9's Dynamic TBD chaining feature and has to
 * start the CU as soon as the first TBD is ready. 
 *
 * Arguments:
 *      bdp    - Ptr to this card's e100_bdconfig structure
 *      tcbp  - Ptr to the TCB to be transmitted
 */
void e100_cu_start (bd_config_t * bdp, tcb_t * tcbp)
{
  bdd_t    *bddp = (bdd_t *) bdp->bddp;
  int       loop_cnt, lock_flag;
  boolean_t status;

  /* If CU is suspended, do a resume */
  if ((bddp->prev_cu_cmd == CB_TRANSMIT) ||
      (bddp->prev_cu_cmd == CB_TRANSMIT_FIRST) ||
      (bddp->prev_cu_cmd == CB_DUMP_RST_STAT))
  {
    if (DEBUG_TX)               /* e100_debug */
       printk ("cu_start: doing RESUME\n");

    /* On ICH2 at 10/H we must issue a NOOP before each CU_RESUME */
    if ((bddp->flags & IS_ICH) && (bddp->cur_line_speed == 10) &&
        (bddp->cur_dplx_mode == HALF_DUPLEX))
    {
      e100_wait_exec_cmd (bdp, SCB_CUC_NOOP);
      drv_usecwait (1);
    }
    e100_wait_exec_cmd (bdp, SCB_CUC_RESUME);

    bddp->prev_cu_cmd = CB_TRANSMIT;
    return;
  }

  /* If it is idle, do a start */
  if ((bddp->scbp->scb_status & SCB_CUS_MASK) == SCB_CUS_IDLE)
  {
    bddp->scbp->scb_gen_ptr = tcbp->tcb_paddr;
    e100_exec_cmd (bdp, SCB_CUC_START);
    bddp->prev_cu_cmd = CB_TRANSMIT_FIRST;
#if (DEBUG_TX)
    printk ("cu_start: did START, cmd = 0x%x\n", SCB_CUC_START);
    printk ("     tcb_paddr = 0x%x\n", tcbp->tcb_paddr);
    printk ("     bddp = 0x%x\n", bddp);
    printk ("     scbp = 0x%x\n", bddp->scbp);
    printk ("     scb_gen_ptr = 0x%x\n", bddp->scbp->scb_gen_ptr);
#endif
    return;
  }

  /* If it is active, but if the previous command was not a transmit then
   * wait for the command to finish, and then do a start
   */
  loop_cnt = 0;
  while ((bddp->scbp->scb_status & SCB_CUS_MASK) != SCB_CUS_IDLE && loop_cnt < 5)
  {
    loop_cnt++;
    drv_usecwait (5);
  }

  bddp->scbp->scb_gen_ptr = tcbp->tcb_paddr;
  e100_exec_cmd (bdp, SCB_CUC_START);
  bddp->prev_cu_cmd = CB_TRANSMIT_FIRST;
#if (DEBUG_TX)
  /* e100_debug */
  printk ("cu_start: did START 2, cmd = 0x%x\n", SCB_CUC_START);
  printk ("     tcb_paddr = 0x%x\n", tcbp->tcb_paddr);
  printk ("     scb_gen_ptr = 0x%x\n", bddp->scbp->scb_gen_ptr);
#endif
}


/* 
 * This routine adjusts the CNA Interrupt Delay for 82558/9.
 * The routine adjusts the value of bddp->current_cna_backoff based
 * on the number of good transmits per interrupt. This adjusted value
 * of current_cna_backoff will be used while setting up TCBs in the
 * e100_prepare_ext_xmit_buff routine.
 *
 * Arguments:
 *      bdp    - Ptr to this card's e100_bdconfig structure
 */
void e100_adjust_cid (bd_config_t * bdp)
{
  bdd_t *bddp = (bdd_t *) bdp->bddp;
  DWORD  good_xmits;

  /* How many good xmits happened since the last time we got here?
   */
  good_xmits = bddp->xmits - bddp->old_xmits;

  /* Increase CID if the number of good xmits is less than or equal to
   * the num of xmit interrupts. This will reduce the num of xmit intrs.
   */
  if ((bddp->num_cna_interrupts) && (good_xmits >= 10))
  {
    if ((good_xmits * XMITS_PER_INTR / bddp->num_cna_interrupts) <= XMITS_PER_INTR)
    {
      if ((bddp->current_cna_backoff + 4) <= 0x1f)
         bddp->current_cna_backoff += 4;
    }
  }
  else
  {
    if ((e100_current_cna_backoff >= 0) && (e100_current_cna_backoff <= 0x1f))
         bddp->current_cna_backoff = e100_current_cna_backoff;
    else bddp->current_cna_backoff = 0;
  }

  bddp->old_xmits = bddp->xmits;
  bddp->num_cna_interrupts = 0;
}


/* ====================================================================== */
/* hw                                                                     */
/* ====================================================================== */

/* 
 * This routine will issue PORT Self-test command to test the
 * e100.  The self-test will fail if the adapter's master-enable
 * bit is not set in the PCI Command Register, or if the adapter
 * is not seated in a PCI master-enabled slot.
 */
boolean_t e100_selftest (bd_config_t * bdp)
{
  bdd_t *bddp = bdp->bddp;      /* stores all adapter specific info */
  DWORD  SelfTestCommandCode;

#if (e100_debug > 2)
  printk ("e100_selftest: begin, selftest_paddr = 0x%x\n", bddp->selftest_paddr);
#endif

  /* Setup the address of the self_test area
   */
  SelfTestCommandCode = (DWORD) bddp->selftest_paddr;

  /* Setup SELF TEST Command Code in D3 - D0
   */
  SelfTestCommandCode |= PORT_SELFTEST;

  /* Initialize the self-test signature and results DWORDS
   */
  bddp->pselftest->st_sign = 0;
  bddp->pselftest->st_result = 0xffffffff;

#if (e100_debug > 2)
  printk ("Port Cmd 0x%x ST_Sign 0x%x ST_Result 0x%x\n",
          SelfTestCommandCode, bddp->pselftest->st_sign,
          bddp->pselftest->st_result);
#endif

  /* Do the port command */
  bddp->scbp->scb_port = SelfTestCommandCode;

  /* Wait 5 milliseconds for the self-test to complete */
  mdelay (50);

  /* if The First Self Test DWORD Still Zero, We've timed out. If the
   * second DWORD is not zero then we have an error.
   */
  if ((bddp->pselftest->st_sign == 0) || (bddp->pselftest->st_result != 0))
  {
#if (e100_debug > 2)
    printk ("e100: Selftest failed Sig = 0x%x Result = 0x%x brd_number=%d\n",
            bddp->pselftest->st_sign, bddp->pselftest->st_result,
            bdp->bd_number);
#endif
    return B_TRUE;
  }

#if (e100_debug > 2)
  printk ("e100self_test: end. ST_Sign 0x%x ST_Result 0x%x\n", bddp->pselftest->st_sign, bddp->pselftest->st_result);
#endif
  return B_TRUE;
}

/* 
 * This routine will issue a configure command to the 82557.
 * This command will be executed in polled mode as interrupts
 * are disabled at this time.  The configuration parameters
 * that are user configurable will have been set in "e100.c".
 */
boolean_t e100_configure (bd_config_t * bdp)
{
  bdd_t       *bddp = bdp->bddp;      /* stores all adapter specific info */
  pcb_header_t pntcb_hdr;
  int          e100_retry;

  pntcb_hdr = (pcb_header_t) bddp->pntcb; /* get hdr of non tcb cmd */

  /* Setup the non-transmit command block header for the configure command.
   */
  pntcb_hdr->cb_status = 0;
  pntcb_hdr->cb_cmd = CB_CONFIGURE;

  /* Note: cb_lnk_ptr is a physical address stored in an unsigned long
   */
  pntcb_hdr->cb_lnk_ptr = 0;

  /* Fill in the configure command data.
   *
   * First fill in the static (end user can't change) config bytes
   */
  bddp->pntcb->ntcb.config.cfg_byte[0] = CB_557_CFIG_DEFAULT_PARM0;
  bddp->pntcb->ntcb.config.cfg_byte[2] = CB_557_CFIG_DEFAULT_PARM2;
  bddp->pntcb->ntcb.config.cfg_byte[3] = CB_557_CFIG_DEFAULT_PARM3;

  /* Commented out byte 6 for CNA intr fix.
   */
//bddp->pntcb->ntcb.config.cfg_byte[6]  = CB_557_CFIG_DEFAULT_PARM6;
  bddp->pntcb->ntcb.config.cfg_byte[9]  = CB_557_CFIG_DEFAULT_PARM9;
  bddp->pntcb->ntcb.config.cfg_byte[10] = CB_557_CFIG_DEFAULT_PARM10;
  bddp->pntcb->ntcb.config.cfg_byte[11] = CB_557_CFIG_DEFAULT_PARM11;
  bddp->pntcb->ntcb.config.cfg_byte[12] = CB_557_CFIG_DEFAULT_PARM12;
  bddp->pntcb->ntcb.config.cfg_byte[13] = CB_557_CFIG_DEFAULT_PARM13;
  bddp->pntcb->ntcb.config.cfg_byte[14] = CB_557_CFIG_DEFAULT_PARM14;
  bddp->pntcb->ntcb.config.cfg_byte[18] = CB_557_CFIG_DEFAULT_PARM18;
  bddp->pntcb->ntcb.config.cfg_byte[20] = CB_557_CFIG_DEFAULT_PARM20;
  bddp->pntcb->ntcb.config.cfg_byte[21] = CB_557_CFIG_DEFAULT_PARM21;

  /* Now fill in the rest of the configuration bytes (the bytes that
   * contain user configurable parameters).
   */

  /* Change for 82558 enhancement */
  /* Set the Tx and Rx Fifo limits */
  if ((bddp->flags & IS_BACHELOR) && (e100_tx_fifo_lmt < 8))
     e100_tx_fifo_lmt = 8;       /* set 8 as the minimum */

  bddp->pntcb->ntcb.config.cfg_byte[1] = (BYTE) (BIT_7 | (e100_tx_fifo_lmt << 4) | e100_rx_fifo_lmt);

  /* added for min. support of IFS */
  if (bddp->cur_line_speed == 100) /* if running at 100Mbs use this */
    bddp->pntcb->ntcb.config.cfg_byte[2] = (BYTE) e100_adaptive_ifs;
  else                          /* we are at 10Mbs so use a greater value */
    bddp->pntcb->ntcb.config.cfg_byte[2] = (BYTE) (5 * e100_adaptive_ifs);

  /* Change for 82558 enhancement */
  /* MWI enable. This should be turned on only if enabled in e100.c and
   * if the adapter is a 82558/9 and if the PCI command reg. has enabled
   * the MWI bit.
   */
  if ((bddp->flags & IS_BACHELOR) && (e100_MWI_enable))
    bddp->pntcb->ntcb.config.cfg_byte[3] |= CB_CFIG_MWI_EN;

  /* Read Align/Write Terminate on cache line. This should be turned on
   * only if enabled in e100.c and if the adapter is a 82558/9 and if the
   * system is cache line oriented.
   */
  if ((bddp->flags & IS_BACHELOR) && (e100_read_align_enable))
     bddp->pntcb->ntcb.config.cfg_byte[3] |= CB_CFIG_READAL_EN | CB_CFIG_TERMCL_EN;

  /* Set the Tx and Rx DMA maximum byte count fields.
   */
  bddp->pntcb->ntcb.config.cfg_byte[4] = e100_rx_dma_cnt;
  bddp->pntcb->ntcb.config.cfg_byte[5] = e100_tx_dma_cnt;
  if ((e100_rx_dma_cnt) || (e100_tx_dma_cnt))
     bddp->pntcb->ntcb.config.cfg_byte[5] |= CB_CFIG_DMBC_EN;

  /* Change for 82558 enhancement */
  /* Extended TCB. Should be turned on only if enabled in e100.c and if
   * the adapter is a 82558/9.
   */
  if ((e100_cfg_parm6 == 0x32) || (e100_cfg_parm6 == 0x3a))
       bddp->pntcb->ntcb.config.cfg_byte[6] = e100_cfg_parm6;
  else bddp->pntcb->ntcb.config.cfg_byte[6] = CB_557_CFIG_DEFAULT_PARM6;

  if ((bddp->flags & IS_BACHELOR) && (e100_enhanced_tx_enable))
     bddp->pntcb->ntcb.config.cfg_byte[6] &= ~CB_CFIG_EXT_TCB_DIS;

  /* Set up number of retries after under run
   */
  bddp->pntcb->ntcb.config.cfg_byte[7] = ((CB_557_CFIG_DEFAULT_PARM7 &
                                          (~CB_CFIG_URUN_RETRY)) |
                                          (e100_urun_retry << 1));

  /* Change for 82558 enhancement */
  /* Dynamic TBD. Should be turned on only if enabled in e100.c and if
   * the adapter is a 82558/9.
   */
  if ((bddp->flags & IS_BACHELOR) && (e100_enhanced_tx_enable))
    bddp->pntcb->ntcb.config.cfg_byte[7] |= CB_CFIG_DYNTBD_EN;

  /* Setup for MII or 503 operation. The CRS+CDT bit should only be set
   * when operating in 503 mode.
   */
  if (bddp->phy_addr == 32)
  {
    bddp->pntcb->ntcb.config.cfg_byte[8] = (CB_557_CFIG_DEFAULT_PARM8 & (~CB_CFIG_503_MII));
    bddp->pntcb->ntcb.config.cfg_byte[15] = (CB_557_CFIG_DEFAULT_PARM15 | CB_CFIG_CRS_OR_CDT);
  }
  else
  {
    bddp->pntcb->ntcb.config.cfg_byte[8] = (CB_557_CFIG_DEFAULT_PARM8 | CB_CFIG_503_MII);
    bddp->pntcb->ntcb.config.cfg_byte[15] = (CB_557_CFIG_DEFAULT_PARM15 & (~CB_CFIG_CRS_OR_CDT));
  }

  /* Change for 82558 enhancement */
  /* enable flowcontrol only if 82558/9
   */
  if ((bddp->flags & IS_BACHELOR) && (e100_flow_control_enable))
  {
    bddp->pntcb->ntcb.config.cfg_byte[16] = DFLT_FC_DELAY_LSB;
    bddp->pntcb->ntcb.config.cfg_byte[17] = DFLT_FC_DELAY_MSB;
    /* Removed CB_CFIG_TX_FC_EN frm line below. This bit has to be 0 to
     * enable flow control.
     */
    bddp->pntcb->ntcb.config.cfg_byte[19] = (CB_557_CFIG_DEFAULT_PARM19 |
                                             CB_CFIG_FC_RESTOP |
                                             CB_CFIG_FC_RESTART |
                                             CB_CFIG_REJECT_FC);
  }
  else
  {
    bddp->pntcb->ntcb.config.cfg_byte[16] = CB_557_CFIG_DEFAULT_PARM16;
    bddp->pntcb->ntcb.config.cfg_byte[17] = CB_557_CFIG_DEFAULT_PARM17;
    /* Bit 2 has to be 'OR'd to disable flow control.
     */
    bddp->pntcb->ntcb.config.cfg_byte[19] = CB_557_CFIG_DEFAULT_PARM19 |
                                            CB_CFIG_TX_FC_DIS;
  }

  /* We must force full duplex on if we are using PHY 0, and we are
   * supposed to run in FDX mode.  We do this because the e100 has only
   * one FDX# input pin, and that pin will be connected to PHY 1.
   * Changed the 'if' condition below to fix performance problem at 10
   * full. The Phy was getting forced to full duplex while the MAC was
   * not, because the cur_dplx_mode was not being set to 2 by SetupPhy.
   * This is how the condition was, initially. This has been changed so
   * that the MAC gets forced to full duplex simply if the user has
   * forced full duplex.
   */
#if 0
  if (bddp->phy_addr == 0 && bddp->cur_dplx_mode == 2)
#endif
  if ((e100_speed_duplex[bdp->bd_number] == 2) ||
      (e100_speed_duplex[bdp->bd_number] == 4) ||
      (bddp->cur_dplx_mode == FULL_DUPLEX))
     bddp->pntcb->ntcb.config.cfg_byte[19] |= CB_CFIG_FORCE_FDX;

  /* The rest of the fix is in the PhyDetect code.
   */
  if ((bddp->phy_addr == 32) && (bddp->cur_dplx_mode == FULL_DUPLEX))
     bddp->pntcb->ntcb.config.cfg_byte[19] |= CB_CFIG_FORCE_FDX;

  /* if in promiscious mode, save bad frames */
  if (bddp->promisc)
  {
    bddp->pntcb->ntcb.config.cfg_byte[6] |= CB_CFIG_SAVE_BAD_FRAMES;
    bddp->pntcb->ntcb.config.cfg_byte[7] &= (BYTE) (~BIT_0);
    bddp->pntcb->ntcb.config.cfg_byte[15] |= CB_CFIG_PROMISCUOUS;
  }

  /* disable broadcast if so desired
   */
  if (bddp->brdcst_dsbl)
     bddp->pntcb->ntcb.config.cfg_byte[15] |= CB_CFIG_BROADCAST_DIS;

  /* this flag is used to enable receiving all multicast packet
   */
  if (bddp->mulcst_enbl)
     bddp->pntcb->ntcb.config.cfg_byte[21] |= CB_CFIG_MULTICAST_ALL;

  /* Enable checksum offloading if we are on a supported adapter.
   */
  if ((bddp->rev_id >= D101MA_REV_ID) && (bddp->rev_id < D102_REV_ID) &&
      (XsumRX[bddp->bd_number] == TRUE) && (bddp->dev_id != 0x1209))
  {
    bddp->checksum_offload_enabled = 1;
    bddp->pntcb->ntcb.config.cfg_byte[9] |= 1;
  }
  else if (bddp->rev_id >= D102_REV_ID)
  {
    /* The D102 chip allows for 32 config bytes.  This value is
     * supposed to be in Byte 0.  Just add the extra bytes to
     * what was already setup in the block.
     */
    bddp->pntcb->ntcb.config.cfg_byte[0] += CB_CFIG_D102_BYTE_COUNT;

    /* now we need to enable the extended RFD.  When this is
     * enabled, the immediated receive data buffer starts at offset
     * 32 from the RFD base address, instead of at offset 16.
     */
    bddp->pntcb->ntcb.config.cfg_byte[7] = CB_CFIG_EXTENDED_RFD;

    /* put the chip into D102 receive mode.  This is neccessary
     * for any parsing and offloading features.
     */
    bddp->pntcb->ntcb.config.cfg_byte[22] = CB_CFIG_RECEIVE_GAMLA_MODE;

    /* set the flag if checksum offloading was enabled
    */
    if (XsumRX[bddp->bd_number] == TRUE)
       bddp->checksum_offload_enabled = 1;
  }

#ifdef IANS
  if (ANS_PRIVATE_DATA_FIELD (bdp)->iANS_status == IANS_COMMUNICATION_UP)
  {
#ifdef IANS_BASE_VLAN_TAGGING
    if (ANS_PRIVATE_DATA_FIELD (bdp)->vlan_mode == IANS_VLAN_MODE_ON)
       bd_ans_hw_ConfigEnableTagging (bddp->pntcb->ntcb.config.cfg_byte, bddp->rev_id);
#endif
  }
#endif

  /* Wait for the SCB command word to clear before we set the general
   * pointer
   */
  if (e100_wait_scb (bdp) != B_TRUE)
     return (B_FALSE);

#if (e100_debug > 2)
  {
    int i;

    printk ("Configure: paddr = 0x%x\n", bddp->nontx_paddr);
    for (i = 0; i < 21; i++)
        printk (" 0x%x", bddp->pntcb->ntcb.config.cfg_byte[i]);
    printk ("\n");
  }
#endif

  /* If we have issued any transmits, then the CU will either be active,
   * or in the suspended state.  If the CU is active, then we wait for
   * it to be suspended.
   */
  if (bddp->prev_cu_cmd == CB_TRANSMIT || bddp->prev_cu_cmd == CB_TRANSMIT_FIRST)
  {
    /* Wait for suspended state */
    e100_retry = E100_CMD_WAIT;
    while ((bddp->scbp->scb_status & SCB_CUS_MASK) == SCB_CUS_ACTIVE && e100_retry)
    {
      mdelay (20);
      e100_retry--;
    }
  }

  /* write the config buffer in the scb gen pointer */
  bddp->scbp->scb_gen_ptr = bddp->nontx_paddr;

  /* store the command */
  bddp->prev_cu_cmd = CB_CONFIGURE;

  /* Submit the configure command to the chip, and wait for it to complete. 
   */
  if (!e100_exec_poll_cmd (bdp))
     return (B_FALSE);
  return (B_TRUE);
}


/* 
 * This routine will issue the IA setup command.  This command
 * will notify the 82557 (e100) of what its individual (node)
 * address is.  This command will be executed in polled mode.
 */
boolean_t e100_setup_iaaddr (bd_config_t * bdp, e100_eaddr_t * peaddr)
{
  bdd_t       *bddp = bdp->bddp;      /* stores all adapter specific info */
  pcb_header_t pntcb_hdr;
  DWORD        i;
  int          lock_flag;

  pntcb_hdr = (pcb_header_t) bddp->pntcb; /* get hdr of non tcb cmd */

  /* Setup the non-transmit command block header for the configure command. 
   */
  pntcb_hdr->cb_status = 0;
  pntcb_hdr->cb_cmd = CB_IA_ADDRESS;

  /* Note: cb_lnk_ptr is a physical address stored in an unsigned long
   */
  pntcb_hdr->cb_lnk_ptr = 0;

  /* Copy in the station's individual address
   */
  for (i = 0; i < ETHERNET_ADDRESS_LENGTH; i++)
      bddp->pntcb->ntcb.setup.ia_addr[i] = peaddr->bytes[i];

  /* Wait for the SCB command word to clear before we set the general
   * pointer
   */
  if (e100_wait_scb (bdp) != B_TRUE)
     return (B_FALSE);

  /* If we have issued any transmits, then the CU will either be active,
   * or in the suspended state.  If the CU is active, then we wait for
   * it to be suspended.
   */
  if ((bddp->prev_cu_cmd == CB_TRANSMIT) || (bddp->prev_cu_cmd == CB_TRANSMIT_FIRST))
  {
    /* Wait for suspended state. Set timeout to 100 ms
     */
    i = 0;
    while (((bddp->scbp->scb_status & SCB_CUS_MASK) == SCB_CUS_ACTIVE) &&
           (i < 5))
    {
      mdelay (20);
      i++;
    }
  }

  /* Update the command list pointer.
   */
  bddp->scbp->scb_gen_ptr = bddp->nontx_paddr;

  /* store the command
   */
  bddp->prev_cu_cmd = CB_IA_ADDRESS;

  /* Submit the IA configure command to the chip, and wait for it to
   * complete.
   */
  if (!e100_exec_poll_cmd (bdp))
  {
    printk ("IA setup failed\n");
    return (B_FALSE);
  }
  return (B_TRUE);
}


/* 
 * This routine checks the status of the 82557's receive unit(RU),
 * and starts the RU if it was not already active.  However,
 * before restarting the RU, the driver gives the RU the buffers
 * it freed up during the servicing of the ISR. If there are
 * no free buffers to give to the RU, ( i.e. we have reached a
 * no resource condition ) the RU will not be started till the
 * next ISR.
 */
void e100_start_ru (bd_config_t * bdp)
{
  bdd_t *bddp = bdp->bddp;      /* get the bddp for this board */
  int    lock_flag;

  /* If the receiver is ready, then don't try to restart. */
  if ((bddp->scbp->scb_status & SCB_RUS_MASK) == SCB_RUS_READY)
     return;

  /* No available buffers */
  if (bddp->rfd_head == NULL)
     return;

  e100_wait_scb (bdp);
  bddp->scbp->scb_gen_ptr = (DWORD) virt_to_bus (RFD_POINTER (bddp->rfd_head, bddp));
  e100_exec_cmd (bdp, SCB_RUC_START);
}

/* 
 * Procedure:   e100_ru_abort
 *
 * Description: This routine issues a RU_ABORT to the  receive unit(RU),
 *                  The RU will go to IDLE.
 *
 * Arguments:
 *      bdp - Ptr to this card's e100_bdconfig structure
 *      lock - whether or not we should lock the board when doing this
 *
 * Returns:
 *      NONE
 */
void e100_ru_abort (bd_config_t * bdp)
{
  bdd_t *bddp = bdp->bddp;

  e100_wait_scb (bdp);
  e100_exec_cmd (bdp, SCB_RUC_ABORT);
}

/* 
 * Procedure:   e100_clr_cntrs
 *
 * Description: This routine will clear the adapter error statistic 
 *              counters.
 *
 * Arguments:
 *      bdp - Ptr to this card's e100_bdconfig structure
 *
 * Returns:
 *      B_TRUE  - If successfully cleared stat counters
 *      B_FALSE - If command failed to complete properly
 *
 */
static boolean_t e100_clr_cntrs (bd_config_t * bdp)
{

  bdd_t *bddp = bdp->bddp;      /* stores all adapter specific info */
  int e100_retry;

  /* Load the dump counters pointer.  Since this command is generated only
   * * after the IA setup has complete, we don't need to wait for the SCB * 
   * command word to clear */
  bddp->scbp->scb_gen_ptr = bddp->stat_cnt_paddr;

  /* Issue the load dump counters address command */
  bddp->prev_cu_cmd = SCB_CUC_DUMP_ADDR;
  e100_exec_cmd (bdp, SCB_CUC_DUMP_ADDR);


  /* wait 10 microseconds for the command to complete */
  drv_usecwait (10);

  /* Now dump and reset all of the statistics */
  bddp->prev_cu_cmd = SCB_CUC_DUMP_RST_STAT;
  e100_exec_cmd (bdp, SCB_CUC_DUMP_RST_STAT);

  /* Now wait for the dump/reset to complete */
  e100_retry = E100_CMD_WAIT;
  while (((WORD) bddp->pstats_counters->cmd_complete != 0xA007) && (e100_retry))
  {
    mdelay (20);
    e100_retry--;
  }

  return (B_TRUE);
}


/* 
 * Procedure:   e100_dump_stat_cntrs
 *
 * Description: This routine will dump the board error counters & then reset 
 *        them.
 *
 * Arguments:
 *      bdp -          Ptr to this card's e100_bdconfig structure
 *
 * Returns:
 *      NONE
 */
void e100_dump_stats_cntrs (bd_config_t * bdp)
{
  bdd_t *bddp = bdp->bddp;      /* stores all adapter specific info */
  int delay_cnt = 10;
  int retry, lock_flag_tx, lock_flag_bd;
  boolean_t status;

  /* If the CU seems to be hung, get outta here. */
  if (bddp->flags & CU_ACTIVE_TOOLONG)
    return;

  /* clear the dump counter complete word */
  bddp->pstats_counters->cmd_complete = 0;

  retry = DUMP_STATS_TIMEOUT;

  while (((bddp->scbp->scb_status & SCB_CUS_MASK) == SCB_CUS_ACTIVE) && (retry))
  {
    mdelay (20);
    retry--;
  }
  if (!retry)
  {
    /* Mark board to indicate that the CU is hung. This flag * will be
     * cleared by the intr routine. This is to fix the OS * hang that was 
     * encounterd when there was a speed mismatch * between the board and 
     * the switch. */
    if (e100_debug > 2)
      printk ("e100_dump_stats_counters: CU active too long\n");
    bddp->flags |= CU_ACTIVE_TOOLONG;

    if (e100_debug > 2)
       printk ("e100: Board[%d] has been disabled\n", bdp->bd_number);
    return;
  }

  /* 
   * we need to do a wait before issuing the next send, so set previous
   * command to CB_DUMP_RST_STAT
   */
  bddp->prev_cu_cmd = CB_DUMP_RST_STAT;

  /* dump h/w stats counters */
  e100_exec_cmd (bdp, SCB_CUC_DUMP_RST_STAT);

  /* check the status of the CU,  */
  if ((bddp->scbp->scb_status & SCB_CUS_MASK) == 0) /* is idle */
     bddp->prev_cu_cmd = CB_NULL;

  /* now await command completion */
  while (((WORD) bddp->pstats_counters->cmd_complete != 0xA007) && delay_cnt)
  {
    delay_cnt--;
    drv_usecwait (25);
  }

  /* increment the statistics */
  bddp->perr_stats->gd_xmits        += bddp->pstats_counters->xmt_gd_frames;
  bddp->perr_stats->gd_recvs        += bddp->pstats_counters->rcv_gd_frames;
  bddp->perr_stats->tx_abrt_xs_col  += bddp->pstats_counters->xmt_max_coll;
  bddp->perr_stats->tx_late_col     += bddp->pstats_counters->xmt_late_coll;
  bddp->perr_stats->tx_dma_urun     += bddp->pstats_counters->xmt_uruns;
  bddp->perr_stats->tx_lost_csrs    += bddp->pstats_counters->xmt_lost_crs;
  bddp->perr_stats->tx_ok_defrd     += bddp->pstats_counters->xmt_deferred;
  bddp->perr_stats->tx_one_retry    += bddp->pstats_counters->xmt_sngl_coll;
  bddp->perr_stats->tx_mt_one_retry += bddp->pstats_counters->xmt_mlt_coll;
  bddp->perr_stats->tx_tot_retries  += bddp->pstats_counters->xmt_ttl_coll;
  bddp->perr_stats->rcv_crc_err     += bddp->pstats_counters->rcv_crc_errs;
  bddp->perr_stats->rcv_align_err   += bddp->pstats_counters->rcv_algn_errs;
  bddp->perr_stats->rcv_rsrc_err    += bddp->pstats_counters->rcv_rsrc_err;
  bddp->perr_stats->rcv_dma_orun    += bddp->pstats_counters->rcv_oruns;
  bddp->perr_stats->rcv_cdt_frames  += bddp->pstats_counters->rcv_err_coll;
  bddp->perr_stats->rcv_runts       += bddp->pstats_counters->rcv_shrt_frames;
}


/*
 * This routine will submit a command block to be executed, &
 * then it will wait for that command block to be executed.
 */
static boolean_t e100_exec_poll_cmd (bd_config_t * bdp)
{
  DWORD        delay;
  boolean_t    status = 0;
  bdd_t       *bddp;
  pcb_header_t pntcb_hdr;

  bddp = (pbdd_t) bdp->bddp;
  pntcb_hdr = (pcb_header_t) bddp->pntcb;

  /* Set the Command Block to be the last command block
   */
  pntcb_hdr->cb_cmd |= CB_EL_BIT;

  /* Clear the status of the command block
   */
  pntcb_hdr->cb_status = 0;

  /* Start the command unit.
   */
  e100_exec_cmd (bdp, SCB_CUC_START);

  /* Wait for the SCB to clear, indicating the completion of the command.
   */
  if (e100_wait_scb (bdp) != B_TRUE)
     return (B_FALSE);

  /* Wait 100ms for some status
   */
  for (delay = 0; delay < 100; delay++)
  {
    mdelay (1);

    /* need to check the pmc_buff status in case it's from the *
     *    set_multicast cmd
     */
    if (pntcb_hdr->cb_status & CB_STATUS_COMPLETE)
    {
      status = TRUE;
      break;
    }
    status = FALSE;
  }

  if (delay == 100)
     return (B_FALSE);
  return (status);
}


/* 
 * This routine checks to see if the e100 has accepted a command.
 * It does so by checking the command field in the SCB, which will
 * be zeroed by the e100 upon accepting a command.  The loop waits
 * for up to 30 milliseconds for command acceptance.
 */
boolean_t e100_wait_scb (bd_config_t * bdp)
{
  int    wait_count = 300000;
  bdd_t *bddp       = bdp->bddp;

  do
  {
    if (!bddp->scbp->scb_cmd_low)
       return B_TRUE;

    drv_usecwait (1);
  }
  while (wait_count--)
        ;
  return B_FALSE;
}


/* 
 * This routine will issue a software reset to the adapter.
 *
 * Arguments:
 *      bdp - Ptr to this card's e100_bdconfig structure
 *      reset_cmd - s/w reset or selective reset.
 */
void e100_sw_reset (bd_config_t * bdp, DWORD reset_cmd)
{
  bdd_t *bddp = bdp->bddp;      /* stores all adapter specific info */

#if (e100_debug > 2)
  printk ("e100_sw_reset, cmd = 0x%x\n", reset_cmd);
#endif

  /* Issue a PORT command with a data word of 0
   */
  bddp->scbp->scb_port = reset_cmd;

  /* wait 5 milliseconds for the reset to take effect
   */
  mdelay (5);

  /* Mask off our interrupt line -- its unmasked after reset
   */
  e100_dis_intr (bdp);
}


/* 
 * This routine downloads microcode on to the controller. This
 * microcode is available for the 82558/9. The microcode
 * reduces the number of receive interrupts by "bundling" them. The amount
 * of reduction in interrupts is configurable thru a e100.c parameter
 * called CPU_CYCLE_SAVER.
 */
static boolean_t e100_load_microcode (bd_config_t * bdp, BYTE rev_id)
{
  DWORD        i, microcode_length;
  bdd_t       *bddp = bdp->bddp;   /* stores all adapter specific info */
  pcb_header_t pntcb_hdr;

  static DWORD d101a_ucode[]  = D101_A_RCVBUNDLE_UCODE;
  static DWORD d101b0_ucode[] = D101_B0_RCVBUNDLE_UCODE;
  static DWORD d101ma_ucode[] = D101M_B_RCVBUNDLE_UCODE;
  static DWORD d101s_ucode[]  = D101S_RCVBUNDLE_UCODE;
  static DWORD d102_ucode[]   = D102_B_RCVBUNDLE_UCODE;
  static DWORD d102c_ucode[]  = D102_C_RCVBUNDLE_UCODE;
  DWORD *mlong;
  WORD  *mshort;
  int    cpusaver_dword;
  DWORD  cpusaver_dword_val;

  if (e100_debug > 2)
     printk ("e100_load_microcode: rev_id=%d\n", rev_id);

  /* user turned ucode loading off */
  if (!ucode[bdp->bd_number])
     return B_FALSE;

  /* this chips do not need ucode */
  if ((bddp->flags & IS_ICH) || (bddp->rev_id < D101A4_REV_ID))
     return B_FALSE;

  pntcb_hdr = (pcb_header_t) bddp->pntcb; /* get hdr of non tcb cmd */

  /* Decide which ucode to use by looking at the board's rev_id
   */
  if (rev_id == D101A4_REV_ID)
  {
    mlong  = d101a_ucode;
    mshort = (WORD*) d101a_ucode;
    microcode_length   = D101_NUM_MICROCODE_DWORDS;
    cpusaver_dword     = D101_CPUSAVER_DWORD;
    cpusaver_dword_val = 0x00080600;
  }
  else if (rev_id == D101B0_REV_ID)
  {
    mlong  = d101b0_ucode;
    mshort = (WORD*) d101b0_ucode;
    microcode_length   = D101_NUM_MICROCODE_DWORDS;
    cpusaver_dword     = D101_CPUSAVER_DWORD;
    cpusaver_dword_val = 0x00080600;
  }
  else if (rev_id == D101MA_REV_ID)
  {
    mlong  = d101ma_ucode;
    mshort = (WORD*) d101ma_ucode;
    microcode_length   = D101M_NUM_MICROCODE_DWORDS;
    cpusaver_dword     = D101M_CPUSAVER_DWORD;
    cpusaver_dword_val = 0x00080800;
  }
  else if (rev_id == D101S_REV_ID) /* Added microcode support for 82559S */
  {
    mlong  = d101s_ucode;
    mshort = (WORD*) d101s_ucode;
    microcode_length   = D101S_NUM_MICROCODE_DWORDS;
    cpusaver_dword     = D101S_CPUSAVER_DWORD;
    cpusaver_dword_val = 0x00080600;
  }
  else if (rev_id == D102_REV_ID)
  {
    mlong  = d102_ucode;
    mshort = (WORD*) d102_ucode;
    microcode_length   = D102_NUM_MICROCODE_DWORDS;
    cpusaver_dword     = D102_B_CPUSAVER_DWORD;
    cpusaver_dword_val = 0x00080600;
  }
  else if (rev_id == D102C_REV_ID)
  {
    mlong  = d102c_ucode;
    mshort = (WORD*) d102c_ucode;
    microcode_length   = D102C_NUM_MICROCODE_DWORDS;
    cpusaver_dword     = D102_C_CPUSAVER_DWORD;
    cpusaver_dword_val = 0x00080600;
  }
  else
    return B_FALSE;             /* we don't have ucode for this board */

  /* Get the tunable value from e100.c and stick in in the right spot */
  if (!e100_cpu_saver)
     return B_FALSE;            /* User has disabled it */

  mshort[cpusaver_dword * 2] = (WORD) e100_cpu_saver;

  /* Get tunable parameter for maximum number of frames that will be
   * bundled. Only applicable for 559's.
   */
  if (rev_id == D101MA_REV_ID)
     mshort [D101M_CPUSAVER_BUNDLE_MAX_DWORD*2] = (WORD)e100_cpusaver_bundle_max;
  else if (rev_id == D101S_REV_ID)
     mshort [D101S_CPUSAVER_BUNDLE_MAX_DWORD*2] = (WORD)e100_cpusaver_bundle_max;

  /* Setup the non-transmit command block header for the command.
   */
  pntcb_hdr->cb_status = 0;
  pntcb_hdr->cb_cmd = CB_LOAD_MICROCODE;

  /* Note: cb_lnk_ptr is a physical address stored in an unsigned long
   */
  pntcb_hdr->cb_lnk_ptr = 0;

  /* Copy in the microcode
   */
  for (i = 0; i < microcode_length; i++)
      bddp->pntcb->ntcb.load_ucode.ucode_dword[i] = mlong[i];

  /* Wait for the SCB command word to clear before we set the general
   * pointer
   */
  if (e100_wait_scb (bdp) != B_TRUE)
     return (B_FALSE);

  /* If we have issued any transmits, then the CU will either be active,
   * or in the suspended state.  If the CU is active, then we wait for
   * it to be suspended.
   */
  if (bddp->prev_cu_cmd == CB_TRANSMIT || bddp->prev_cu_cmd == CB_TRANSMIT_FIRST)
  {
    /* Wait for suspended state
     * Set timeout to 100 ms
     */
    i = 0;
    while ((bddp->scbp->scb_status & SCB_CUS_MASK) == SCB_CUS_ACTIVE && i < 5)
    {
      mdelay (20);
      i++;
    }
    if (i == 5)
       return (B_FALSE);
  }

  /* Update the command list pointer.
   */
  bddp->scbp->scb_gen_ptr = bddp->nontx_paddr;

  /* store the command
   */
  bddp->prev_cu_cmd = CB_LOAD_MICROCODE;

  /* Submit the Load microcode command to the chip, and wait for it to
   * complete.
   */
  if (!e100_exec_poll_cmd (bdp))
     return (B_FALSE);
  return (B_TRUE);
}

/***************************************************************************/
/*       EEPROM  Functions                                                 */
/***************************************************************************/

/* 
 * read pwa (printed wired assembly) number
 */
void e100_rd_pwa_no (bd_config_t * bdp)
{
  int    i;
  bdd_t *bddp = (bdd_t *) bdp->bddp;

  bddp->pwa_no = e100_ReadEEprom (bdp, EEPROM_PWA_NO);
  bddp->pwa_no <<= 16;
  bddp->pwa_no |= e100_ReadEEprom (bdp, EEPROM_PWA_NO + 1);
}
   
/* 
 * Reads the permanent ethernet address from the eprom.
 */
void e100_rd_eaddr (bd_config_t * bdp)
{
  int     i;
  WORD   EepromWordValue;
  bdd_t *bddp = (bdd_t *) bdp->bddp;
  BYTE   data = 0;

  if (e100_debug > 2)
     printk ("e100_rd_eaddr: begin\n");

  /* Read SCB reg General Control 2 */
  data = bddp->scbp->scb_ext.d102_scb.scb_gen_ctrl2;

  if (bddp->rev_id >= D102_REV_ID)
  {
    while (!(data & SCB_GCR2_EEPROM_ACCESS_SEMAPHORE))
    {
      /* or in the apropriate bit. After we write this if it is still clear
       * that means that the hardware is accessing the eeprom. In this case 
       * we will just loop until we get it.
       */
      data |= SCB_GCR2_EEPROM_ACCESS_SEMAPHORE;
      bddp->scbp->scb_ext.d102_scb.scb_gen_ctrl2 = data;
      data = bddp->scbp->scb_ext.d102_scb.scb_gen_ctrl2;
    }
  }

  for (i = 0; i < 6; i += 2)
  {
    EepromWordValue = e100_ReadEEprom (bdp, EEPROM_NODE_ADDRESS_BYTE_0 + (i / 2));

    bdp->eaddr.bytes[i]   = bddp->perm_node_address[i]   = (BYTE) EepromWordValue;
    bdp->eaddr.bytes[i+1] = bddp->perm_node_address[i+1] = (BYTE) (EepromWordValue >> 8);
  }

  /* Now reset the eeprom semaphore bit
   * Read SCB reg General Control 2
   */
  if (bddp->rev_id >= D102_REV_ID)
  {
    data = bddp->scbp->scb_ext.d102_scb.scb_gen_ctrl2;
    data &= ~SCB_GCR2_EEPROM_ACCESS_SEMAPHORE;
    bddp->scbp->scb_ext.d102_scb.scb_gen_ctrl2 = data;
  }

  /* fill in the device structure...
   */
  memcpy (&bdp->device->dev_addr[0], &bddp->perm_node_address[0], ETHERNET_ADDRESS_LENGTH);

  if (e100_debug)
     printk ("Node addr is: %x:%x:%x:%x:%x:%x\n",
             bddp->perm_node_address[0], bddp->perm_node_address[1],
             bddp->perm_node_address[2], bddp->perm_node_address[3],
             bddp->perm_node_address[4], bddp->perm_node_address[5]);
}


/* 
 * Calculates the checksum and writes it to the EEProm.  This
 * routine assumes that the checksum word is the last word in
 * a 64 word EEPROM.  It calculates the checksum accroding to
 * the formula: Checksum = 0xBABA - (sum of first 63 words).
 */
void e100_wrEEPROMcsum (bd_config_t * bdp)
{
  WORD Checksum = 0;
  WORD Iteration;

  for (Iteration = 0; Iteration < EEPROM_CHECKSUM_REG; Iteration++)
      Checksum += e100_ReadEEprom (bdp, Iteration);
  Checksum = (WORD) EEPROM_SUM - Checksum;
  e100_WriteEEprom (bdp, EEPROM_CHECKSUM_REG, Checksum);
}


/* 
 * This routine serially reads one word out of the EEPROM.
 */
static WORD e100_ReadEEprom (bd_config_t * bdp, WORD Reg)
{
  WORD   Data;
  bdd_t *bddp = (bdd_t *) bdp->bddp;
  WORD   bits = e100_EEpromAddressSize (bddp->EEpromSize);

  E100_WRITE_REG (EEPROM_CTRL, EECS);

  /* write the read opcode and register number in that order
   * The opcode is 3bits in length, reg is 'bits' bits long
   */
  e100_ShiftOutBits (bdp, EEPROM_READ_OPCODE, 3);
  e100_ShiftOutBits (bdp, Reg, bits);

  /* Now read the data (16 bits) in from the selected EEPROM word
   */
  Data = e100_ShiftInBits (bdp);

  e100_EEpromCleanup (bdp);
  return (Data);
}

/* 
 * This routine shifts data bits out to the EEPROM.
 *
 * Arguments:
 *      bdp   - Ptr to this card's e100_bdconfig structure
 *      data  - data to send to the EEPROM.
 *      count - number of data bits to shift out.
 */
static void e100_ShiftOutBits (bd_config_t * bdp, WORD data, WORD count)
{
  bdd_t *bddp = (bdd_t *) bdp->bddp;
  WORD   x, mask;

  mask = 0x01 << (count - 1);
  x    = E100_READ_REG (EEPROM_CTRL);
  x   &= ~(EEDO | EEDI);

  do
  {
    x &= ~EEDI;
    if (data & mask)
       x |= EEDI;

    E100_WRITE_REG (EEPROM_CTRL, x);

    drv_usecwait (100);

    e100_RaiseClock (bdp, &x);
    e100_LowerClock (bdp, &x);
    mask = mask >> 1;
  }
  while (mask);

  x &= ~EEDI;
  E100_WRITE_REG (EEPROM_CTRL, x);
}


/* 
 * This routine raises the EEPOM's clock input (EESK)
 */
static void e100_RaiseClock (bd_config_t * bdp, WORD * x)
{
  bdd_t *bddp = (bdd_t *) bdp->bddp;

  *x = *x | EESK;
  E100_WRITE_REG (EEPROM_CTRL, *x);
  drv_usecwait (100);
}


/* 
 * This routine lower's the EEPOM's clock input (EESK)
 */
static void e100_LowerClock (bd_config_t * bdp, WORD * x)
{
  bdd_t *bddp = (bdd_t *) bdp->bddp;

  *x = *x & ~EESK;
  E100_WRITE_REG (EEPROM_CTRL, *x);
  drv_usecwait (100);
}

/*
 * This routine shifts data bits in from the EEPROM.
 */
static WORD e100_ShiftInBits (bd_config_t * bdp)
{
  bdd_t *bddp = (bdd_t *) bdp->bddp;
  WORD   x, data, i;

  x = E100_READ_REG (EEPROM_CTRL);
  x &= ~(EEDO | EEDI);
  data = 0;

  for (i = 0; i < 16; i++)
  {
    data = data << 1;
    e100_RaiseClock (bdp, &x);

    x = E100_READ_REG (EEPROM_CTRL);

    x &= ~(EEDI);
    if (x & EEDO)
      data |= 1;

    e100_LowerClock (bdp, &x);
  }
  return (data);
}


/* 
 * This routine returns the EEPROM to an idle state
 */
static void e100_EEpromCleanup (bd_config_t * bdp)
{
  bdd_t *bddp = (bdd_t *) bdp->bddp;
  WORD   x    = E100_READ_REG (EEPROM_CTRL);

  x &= ~(EECS | EEDI);
  E100_WRITE_REG (EEPROM_CTRL, x);

  e100_RaiseClock (bdp, &x);
  e100_LowerClock (bdp, &x);
}


/* 
 * This routine writes a word to a specific EEPROM location.
 */
static void e100_WriteEEprom (bd_config_t * bdp, WORD reg, WORD data)
{
  bdd_t *bddp = (bdd_t *) bdp->bddp;
  WORD   x, bits;

  bits = e100_EEpromAddressSize (bddp->EEpromSize);

  /* select EEPROM, mask off ASIC and reset bits, set EECS
   */
  x = E100_READ_REG (EEPROM_CTRL);

  x &= ~(EEDI | EEDO | EESK);
  x |= EECS;
  E100_WRITE_REG (EEPROM_CTRL, x);

  e100_ShiftOutBits (bdp, EEPROM_EWEN_OPCODE, 5);
  e100_ShiftOutBits (bdp, reg, (bits - 2)); /* Changed 4 to bits - 2 */

  e100_StandBy (bdp);

  /* Erase this particular word.  Write the erase opcode and register
   * number in that order. The opcode is 3bits in length; reg is 'bits'
   * bits long.
   */
  e100_ShiftOutBits (bdp, EEPROM_ERASE_OPCODE, 3);
  e100_ShiftOutBits (bdp, reg, bits); /* Changed 6 to bits */

  if (e100_WaitEEPROMCmdDone (bdp) == B_FALSE)
     return;

  e100_StandBy (bdp);

  /* write the new word to the EEPROM 
   * send the write opcode the EEPORM
   */
  e100_ShiftOutBits (bdp, EEPROM_WRITE_OPCODE, 3);

  /* select which word in the EEPROM that we are writing to.
   */
  e100_ShiftOutBits (bdp, reg, bits); /* Changed 6 to bits */

  /* write the data to the selected EEPROM word.
   */
  e100_ShiftOutBits (bdp, data, 16);

  if (e100_WaitEEPROMCmdDone (bdp) == B_FALSE)
     return;

  e100_StandBy (bdp);

  e100_ShiftOutBits (bdp, EEPROM_EWDS_OPCODE, 5);
  e100_ShiftOutBits (bdp, reg, (bits - 2)); /* Changed 4 to bits - 2 */

  e100_EEpromCleanup (bdp);
}

/* 
 * This routine waits for the the EEPROM to finish its command.
 * Specifically, it waits for EEDO (data out) to go high.
 */
static WORD e100_WaitEEPROMCmdDone (bd_config_t * bdp)
{
  bdd_t *bddp = (bdd_t *) bdp->bddp;
  WORD   x, i;

  e100_StandBy (bdp);

  for (i = 0; i < 200; i++)
  {
    x = E100_READ_REG (EEPROM_CTRL);
    if (x & EEDO)
       return (B_TRUE);

    drv_usecwait (50);
  }
  return B_FALSE;
}


/* 
 * This routine lowers the EEPROM chip select (EECS) for a few
 * microseconds.
 */
static void e100_StandBy (bd_config_t * bdp)
{
  bdd_t *bddp = (bdd_t *) bdp->bddp;
  WORD   x    = E100_READ_REG (EEPROM_CTRL);

  x &= ~(EECS | EESK);
  E100_WRITE_REG (EEPROM_CTRL, x);

  drv_usecwait (50);

  x |= EECS;
  E100_WRITE_REG (EEPROM_CTRL, x);
}


/* 
 * Reads the subsystem_id and subsystem_vendor words from the eprom.
 * 
 * Arguments: 
 *          bdp - Ptr to this card's e100_bdconfig structure
 *          ven_info - Ptr to a eprom_vendor_info_t structure
 *          sub_dev - Ptr to a eeprom_sub_device_info_t structure
 * 
 * Returns:  filled in e100_vendor_info_t structure; void return
 */
static void e100_rd_vendor_info (bd_config_t *bdp, WORD *sub_ven, WORD *sub_dev)
{
  WORD EepromWordValue;

  if (e100_debug > 2)
    printk ("e100_rd_vendor_info: begin.\n");

  /* Read the Subsystem_ID and Subsystem_Vendor words from the EEPROM.
   * We Must use I/O to do this, because have not yet memory mapped the CSR
   *
   * read the subsystem ID
   */
  EepromWordValue = e100_ReadEEprom (bdp, EEPROM_SUBSYSTEM_ID_WORD);
  *sub_dev = EepromWordValue;

  /* read the subsystem vendor
   */
  EepromWordValue = e100_ReadEEprom (bdp, EEPROM_SUBSYSTEM_VENDOR_WORD);
  *sub_ven = EepromWordValue;
}

/* 
 * Returns the size of the EEPROM
 */
WORD e100_GetEEpromSize (bd_config_t * bdp)
{
  bdd_t *bddp = (bdd_t *) bdp->bddp;
  WORD   x, size = 1;          /* must be one to accumulate a product */
  WORD   data;

  /* enable the eeprom by setting EECS.
   */
  x = E100_READ_REG (EEPROM_CTRL);
  x &= ~(EEDI | EEDO | EESK);
  x |= EECS;
  E100_WRITE_REG (EEPROM_CTRL, x);

  /* write the read opcode
   */
  e100_ShiftOutBits (bdp, EEPROM_READ_OPCODE, 3);

  /* experiment to discover the size of the eeprom.  request register zero
   * and wait for the eeprom to tell us it has accepted the entire address.
   */
  x = E100_READ_REG (EEPROM_CTRL);
  do
  {
    size *= 2;       /* each bit of address doubles eeprom size */
    x |= EEDO;       /* set bit to detect "dummy zero" */
    x &= ~EEDI;      /* address consists of all zeros */

    E100_WRITE_REG (EEPROM_CTRL, x);
    drv_usecwait (100);
    e100_RaiseClock (bdp, &x);
    e100_LowerClock (bdp, &x);

    /* check for "dummy zero"
     */
    x = E100_READ_REG (EEPROM_CTRL);
    if (size > 256)
    {
      size = 0;
      break;
    }
  }
  while (x & EEDO);

  /* read in the value requested
   */
  data = e100_ShiftInBits (bdp);
  e100_EEpromCleanup (bdp);
  return (size);
}

/* 
 * Returns the number of address bits required
 */
static WORD e100_EEpromAddressSize (WORD size)
{
  WORD address_size = 0;

  switch (size)
  {
    case 64:
         return 6;
    case 128:
         return 7;
    case 256:
         return 8;
  }

  /* catchall and return statement. */
  while (size >>= 1)
    address_size++;
  return (address_size);
}


/***************************************************************************/
/*       PHY Functions                                                     */
/***************************************************************************/


/* 
 * Procedure:   PhyDetect
 *
 * Description: This routine will detect what phy we are using, set the line
 *              speed, FDX or HDX, and configure the phy if necessary.
 *
 *              The following combinations are supported:
 *              - TX or T4 PHY alone at PHY address 1
 *              - T4 or TX PHY at address 1 and MII PHY at address 0
 *              - 82503 alone (10Base-T mode, no full duplex support)
 *              - 82503 and MII PHY (TX or T4) at address 0
 *
 *              The sequence / priority of detection is as follows:
 *              - PHY 1 with cable termination
 *              - PHY 0 with cable termination
 *              - PHY 1 (if found) without cable termination
 *              - 503 interface
 *
 *              Additionally auto-negotiation capable (NWAY) and parallel
 *              detection PHYs are supported. The flow-chart is described*/
boolean_t e100_phydetect (bd_config_t * bdp)
{
  DWORD CurrPhy;
  DWORD Phy1;
  WORD MdiControlReg, MdiStatusReg;
  BYTE ReNegotiateTime = 35;
  bdd_t *bddp;
  int old_speed_duplex;
  int board_speed_duplex;

  board_speed_duplex = e100_speed_duplex[bdp->bd_number];

  bddp = (pbdd_t) bdp->bddp;    /* get the bddp for this board */
  Phy1 = 0;

#ifdef PHY_DEBUG
  printk ("e100_phydetect: \n");
#endif

  /* Check for a phy from address 1 - 31 */
  for (CurrPhy = 1; CurrPhy <= MAX_PHY_ADDR; CurrPhy++)
  {
    /* Read the MDI control register at CurrPhy. */
    e100_MdiRead (bdp, MDI_CONTROL_REG, CurrPhy, &MdiControlReg);

    /* Read the status register at phy 1 */
    e100_MdiRead (bdp, MDI_STATUS_REG, CurrPhy, &MdiStatusReg);

#ifdef PHY_DEBUG
    printk ("(%d, 0x%x, 0x%x) \n", CurrPhy, MdiControlReg, MdiStatusReg);
#endif

    /* check if we found a valid phy */
    if (!((MdiControlReg == 0xffff) || ((MdiStatusReg == 0) && (MdiControlReg == 0))))
    {
      /* we have a valid phy1 */
#ifdef PHY_DEBUG
      printk ("Found Phy 1 at address %d CReg 0x%x SReg 0x%x\n", CurrPhy, MdiControlReg, MdiStatusReg);
#endif

      Phy1 = CurrPhy;

      /* Read the status register again because of sticky bits */
      e100_MdiRead (bdp, MDI_STATUS_REG, CurrPhy, &MdiStatusReg);

      /* If there is a valid link then use this Phy. */
      if (MdiStatusReg & MDI_SR_LINK_STATUS)
      {
        /* Mark link up */
        bdp->flags |= DF_LINK_UP;
#ifdef IANS
        bddp->cur_link_status = IANS_STATUS_LINK_OK;
#endif

#ifdef PHY_DEBUG
        printk ("Setup Phy 1 at address %d with link\n", CurrPhy);
#endif

        bddp->phy_addr = Phy1;

        return (e100_SetupPhy (bdp));
      }
#ifdef PHY_DEBUG
      printk ("Phy 1 at address %d WITHOUT link\n", CurrPhy);
#endif


      /* found a valid phy without a link, so break */
      break;
    }
  }

  /* 
   * Next try to detect a PHY at address 0x00 because there was no Phy 1, 
   * or Phy 1 didn't have link
   */

  /* Read the MDI control register at phy 0 */
  e100_MdiRead (bdp, MDI_CONTROL_REG, 0, &MdiControlReg);

  /* Read the status register at phy 0 */
  e100_MdiRead (bdp, MDI_STATUS_REG, 0, &MdiStatusReg);

  /* check if we found a valid phy 0 */
  if ((MdiControlReg == 0xffff) || ((MdiStatusReg == 0) && (MdiControlReg == 0)))
  {
    /* we don't have a valid phy at address 0 */

    if (Phy1)
    {
      /* no phy 0, so use phy 1 */
      bddp->phy_addr = Phy1;

#ifdef PHY_DEBUG
      printk ("Using Phy1 without link -- Phy 0 not found\n");
#endif

      /* no phy 0, but there is a phy 1, so use phy 1 */
      return (e100_SetupPhy (bdp));
    }
    else
    {
      /* didn't find phy 0 or phy 1, so assume a 503 interface */
      bddp->phy_addr = 32;

      /* 
       *    This fix is for the board which has the 503 interface
       *    and need to have the speed and duplex mode set here.  The only
       *    way for a user to get a FULL duplex setting is to have it forced
       *    in the e100.c file.
       *
       *    The second test is for an invalid setting for the hardware.
       */
      /* The adapter can't autoneg. so set to 10/HALF */
      if (board_speed_duplex == 0)
      {
        old_speed_duplex = board_speed_duplex;
        board_speed_duplex = e100_speed_duplex[bdp->bd_number] = 1;
        e100_ForceSpeedAndDuplex (bdp);
        /* Restore speed_duplex value. */
        board_speed_duplex = e100_speed_duplex[bdp->bd_number] = old_speed_duplex;
      }
      else if ((board_speed_duplex == 1) || (board_speed_duplex == 2))
      {
        e100_ForceSpeedAndDuplex (bdp);
      }
      else if (board_speed_duplex > 2)
      {
        printk ("503 serial component detected which does not " "support 100MBS.\n");
        printk ("Change the forced speed/duplex " "to a supported setting.\n");
        return (B_FALSE);
      }
#ifdef PHY_DEBUG
      printk ("503 serial component detected and " "set speed and duplex mode\n");
#endif
      return (B_TRUE);
    }
  }
  else
  {
    /* We have a valid phy at address 0.  If phy 0 has a link then * we
     * use phy 0.  If Phy 0 doesn't have a link then we use Phy 1 * (
     * which also doesn't have a link ) if phy 1 is present, else use *
     * phy 0 if phy 1 is not present */

#ifdef PHY_DEBUG
    printk ("Phy 0 detected\n");
#endif

    /* If phy 1 was present, then we must isolate phy 1 before we *
     * enable phy 0 to see if Phy 0 has a link. */
    if (Phy1)
    {
      /* isolate phy 1 */
      e100_MdiWrite (bdp, MDI_CONTROL_REG, Phy1, MDI_CR_ISOLATE);

      /* wait 100 milliseconds for the phy to isolate. */
      mdelay (100);
    }

    /* Since this Phy is at address 0, we must enable it.  So clear */
    /* the isolate bit, and set the auto-speed select bit */
    e100_MdiWrite (bdp, MDI_CONTROL_REG, 0, MDI_CR_AUTO_SELECT);

    /* wait 100 milliseconds for the phy to be enabled. */
    mdelay (100);

    /* restart the auto-negotion process */
    e100_MdiWrite (bdp, MDI_CONTROL_REG, 0, MDI_CR_RESTART_AUTO_NEG | MDI_CR_AUTO_SELECT);

    /* wait no more than 3.5 seconds for auto-negotiation to complete */
    while (ReNegotiateTime)
    {
      /* Read the status register twice because of sticky bits */
      e100_MdiRead (bdp, MDI_STATUS_REG, 0, &MdiStatusReg);
      e100_MdiRead (bdp, MDI_STATUS_REG, 0, &MdiStatusReg);

      if (MdiStatusReg & MDI_SR_AUTO_NEG_COMPLETE)
        break;

      mdelay (10);
      ReNegotiateTime--;
    }

    /* Read the status register again because of sticky bits */
    e100_MdiRead (bdp, MDI_STATUS_REG, 0, &MdiStatusReg);

    /* Mark link up */
    if (MdiStatusReg & MDI_SR_LINK_STATUS)
    {
#ifdef IANS
      bddp->cur_link_status = IANS_STATUS_LINK_OK;
#endif
      bdp->flags |= DF_LINK_UP;
    }

    /* If the link was not set */
    if (!(MdiStatusReg & MDI_SR_LINK_STATUS))
    {
      /* the link wasn't set, so use phy 1 if phy 1 was present */
      if (Phy1)
      {
#ifdef PHY_DEBUG
        printk ("Using Phy1 without link -- Phy 0 has no link\n");
#endif

        /* isolate phy 0 */
        e100_MdiWrite (bdp, MDI_CONTROL_REG, 0, MDI_CR_ISOLATE);

        /* wait 100 milliseconds for the phy to isolate. */
        mdelay (100);

        /* Now re-enable PHY 1 */
        e100_MdiWrite (bdp, MDI_CONTROL_REG, Phy1, MDI_CR_AUTO_SELECT);

        /* wait 100 milliseconds for the phy to be enabled. */
        mdelay (100);

        /* restart the auto-negotion process */
        e100_MdiWrite (bdp, MDI_CONTROL_REG, bddp->phy_addr, MDI_CR_RESTART_AUTO_NEG | MDI_CR_AUTO_SELECT);

        bddp->phy_addr = Phy1;

        /* Don't wait for it 2 complete (no link from earlier) */
        return (e100_SetupPhy (bdp));
      }

    }

    /* Definitely using Phy 0 */
    bddp->phy_addr = 0;

    return (e100_SetupPhy (bdp));
  }
}


/* 
 * Procedure:   e100_SetupPhy
 *
 * Description: This routine will setup phy 1 or phy 0 so that it is configured
 *              to match line speed.  This driver assumes assume the adapter 
 *        is automatically setting the line speed, and the duplex mode. 
 *        At the end of this routine, any truly Phy specific code will 
 *        be executed (each Phy has its own quirks, and some require 
 *        that certain special bits are set).
 *
 *   NOTE:  The driver assumes that SPEED and FORCEFDX are specified at the
 *          same time. If FORCEDPX is set without speed being set, the driver
 *          will encouter a fatal error.
 *
 * Arguments:
 *      bdp - Ptr to this card's e100_bdconfig structure
 *
 * Result:
 * Returns:
 *      B_TRUE  - If the phy could be configured correctly
 *      B_FALSE - If the phy couldn't be configured correctly, because an 
 *           */
static boolean_t e100_SetupPhy (bd_config_t * bdp)
{
  WORD MdiIdLowReg, MdiIdHighReg;
  WORD MdiMiscReg;
  DWORD PhyId;
  bdd_t *bddp;
  int board_speed_duplex;

  board_speed_duplex = e100_speed_duplex[bdp->bd_number];

  bddp = (pbdd_t) bdp->bddp;    /* get the bddp for this board */


#ifdef PHY_DEBUG
  printk ("e100_SetupPhy Phy %d\n", bddp->phy_addr);
#endif

  /* Find out specifically what Phy this is.  We do this because for *
   * certain phys there are specific bits that must be set so that the *
   * phy and the 82557 work together properly. */

  e100_MdiRead (bdp, PHY_ID_REG_1, bddp->phy_addr, &MdiIdLowReg);
  e100_MdiRead (bdp, PHY_ID_REG_2, bddp->phy_addr, &MdiIdHighReg);

  PhyId = ((DWORD) MdiIdLowReg | ((DWORD) MdiIdHighReg << 16));

  bddp->PhyState = 0;
  bddp->PhyDelay = 0;

  /* get the revsion field of the Phy ID so that we'll be able to detect */
  /* future revs of the same Phy. */
  PhyId &= PHY_MODEL_REV_ID_MASK;
#ifdef PHY_DEBUG
  printk ("Phy ID is 0x%x \n", PhyId);
#endif

  bddp->PhyId = PhyId;

  if (PhyId == PHY_82562EH)
  {
    bddp->flags |= IS_82562EH;
    PhyProcessing82562EHInit (bddp);
  }

  /* If Intel Phy, set flag to indicate this */
  if (PhyId == PHY_82555_TX)
  {
    bdp->flags |= DF_PHY_82555;
  }

  /* Handle the National TX */
  if (PhyId == PHY_NSC_TX)
  {
    e100_MdiRead (bdp, NSC_CONG_CONTROL_REG, bddp->phy_addr, &MdiMiscReg);

    MdiMiscReg |= NSC_TX_CONG_TXREADY;

    /* If we are configured to do congestion control, then enable the */
    /* congestion control bit in the National Phy */
    if (e100_cong_enbl)
      MdiMiscReg |= NSC_TX_CONG_ENABLE;
    else
      MdiMiscReg &= ~NSC_TX_CONG_ENABLE;

    e100_MdiWrite (bdp, NSC_CONG_CONTROL_REG, bddp->phy_addr, MdiMiscReg);
  }

  if ((board_speed_duplex >= 1) && (board_speed_duplex <= 4))
    e100_ForceSpeedAndDuplex (bdp);

  /* Set bddp values for speed and duplex modes only if autonegotiation *
   * happened. If it was forced, the bddp values would have been set by *
   * the ForceSpeedAndUplex routine. If it isn't then we need to check * to 
   * see if autoneg has completed and if we need to restart it. */
  if (!((board_speed_duplex >= 1) && (board_speed_duplex <= 4)))
  {
    e100_auto_neg (bdp);
    e100_FindPhySpeedAndDpx (bdp, PhyId);
  }
#ifdef PHY_DEBUG
  printk ("Current speed=%d, Current Duplex=%d\n", bddp->cur_line_speed, bddp->cur_dplx_mode);
#endif

  return (B_TRUE);
}



/* 
 * Procedure:   e100_fix_polarity
 *
 * Description:
 *      Fix for 82555 auto-polarity toggle problem. With a short cable 
 *      connecting an 82555 with an 840A link partner, if the medium is noisy,
 *      the 82555 sometime thinks that the polarity might be wrong and so 
 *      toggles polarity. This happens repeatedly and results in a high bit 
 *      error rate.
 *      NOTE: This happens only at 10 Mbps
 *
 * Arguments:
 *      bdp - Ptr to this card's e100_bdconfig structure
 *
 * Returns:
 *      NOTHING
 */
void e100_fix_polarity (bd_config_t * bdp)
{
  bdd_t *bddp = (bdd_t *) bdp->bddp;
  DWORD PhyAdd;
  WORD  Status;
  WORD  errors;
  int   speed;

  if (e100_debug > 2)
    printk ("e100_set_polarity: begin\n");


  PhyAdd = bddp->phy_addr;

  /* If the user wants auto-polarity disabled, do only that and nothing *
   * else. * e100_autopolarity == 0 means disable --- we do just the
   * disabling * e100_autopolarity == 1 means enable  --- we do nothing at
   * all * e100_autopolarity >= 2 means we do the workaround code. */
  /* Change for 82558 enhancement */
  if (!(bddp->flags & IS_BACHELOR))
  {
    if (e100_autopolarity == 0)
    {
      e100_MdiWrite (bdp, PHY_82555_SPECIAL_CONTROL, PhyAdd, DISABLE_AUTO_POLARITY);
    }
    else if (e100_autopolarity >= 2)
    {
      /* we do this only if we have an 82555 and if link is up */
      if ((bdp->flags & DF_PHY_82555) && (bdp->flags & DF_LINK_UP))
      {




#ifdef DEBUG
        printk ("messing with autopolarity\n");
#endif
        e100_MdiRead (bdp, PHY_82555_CSR, PhyAdd, &Status);
        speed = (Status & PHY_82555_SPEED_BIT) ? 100 : 10;

        /* we need to do this only if speed is 10 */
        if (speed == 10)
        {
          /* see if we have any end of frame errors */
          e100_MdiRead (bdp, PHY_82555_EOF_COUNTER, PhyAdd, &errors);

          /* if non-zero, wait for 100 ms before reading again */
          if (errors)
          {
            mdelay (100);
            e100_MdiRead (bdp, PHY_82555_EOF_COUNTER, PhyAdd, &errors);

            /* if non-zero again, we disable polarity */
            if (errors)
            {
              e100_MdiWrite (bdp, PHY_82555_SPECIAL_CONTROL, PhyAdd, DISABLE_AUTO_POLARITY);
            }
          }
          else
          {
            /* it is safe to read the polarity now */
            e100_MdiRead (bdp, PHY_82555_CSR, PhyAdd, &Status);

            /* if polarity is normal, disable polarity */
            if (!(Status & PHY_82555_POLARITY_BIT))
            {
              e100_MdiWrite (bdp, PHY_82555_SPECIAL_CONTROL, PhyAdd, DISABLE_AUTO_POLARITY);
            }
          }
        }                       /* end if speed == 10 */
      }                         /* end if PHY 82555 */
    }                           /* end of polarity fix */
  }


}


/* 
 * Procedure:   e100_auto_neg
 *
 * Description: This routine will start autonegotiation and wait
 *                   for it to complete
 *
 * Arguments:
 *      bdp - Ptr to this card's e100_bdconfig structure
 *
 * Returns:
 *      NOTHING
 */
void e100_auto_neg (bd_config_t * bdp)
{
  WORD mdi_status_reg;
  bdd_t *bddp = bdp->bddp;
  DWORD i;

  /* If we are on 82562EH then no need */
  if (bddp->flags & IS_82562EH)
  {
    return;
  }

  /* first we need to check to see if autoneg has already completed. Sticky 
   * so read twice */
  e100_MdiRead (bdp, MDI_STATUS_REG, bddp->phy_addr, &(mdi_status_reg));
  e100_MdiRead (bdp, MDI_STATUS_REG, bddp->phy_addr, &(mdi_status_reg));
  /* if it is finished then leave */
  if (mdi_status_reg & MDI_SR_AUTO_NEG_COMPLETE)
  {
    return;
  }

  /* if we are capable of performing autoneg then we restart */
  if (mdi_status_reg & MDI_SR_AUTO_SELECT_CAPABLE)
  {
    e100_MdiWrite (bdp, MDI_CONTROL_REG, bddp->phy_addr, MDI_CR_AUTO_SELECT | MDI_CR_RESTART_AUTO_NEG);

    /* now wait for autoneg to complete this could be up to 3 seconds
     * which is 300 times 10 milliseconds */
    for (i = 0; (!(mdi_status_reg & MDI_SR_AUTO_NEG_COMPLETE)) && (i < 300); i++)
    {
      /* delay 10 milliseconds */
      mdelay (10);

      /* now re-read the value. Sticky so read twice */
      e100_MdiRead (bdp, MDI_STATUS_REG, bddp->phy_addr, &(mdi_status_reg));
      e100_MdiRead (bdp, MDI_STATUS_REG, bddp->phy_addr, &(mdi_status_reg));
    }
    if (i == 300)
      bddp->flags |= INVALID_SPEED_DPLX;
  }
  return;
}



/* 
 * Procedure:   e100_FindPhySpeedAndDpx
 *
 * Description: This routine will figure out what line speed and duplex mode
 *              the PHY is currently using.
 *
 * Arguments:
 *      bdp - Ptr to this card's e100_bdconfig structure
 *      PhyId - The ID of the PHY in question.
 *
 * Returns:
 *      NOTHING
 */
void e100_FindPhySpeedAndDpx (bd_config_t * bdp, DWORD PhyId)
{
  WORD MdiStatusReg;
  WORD MdiMiscReg;
  WORD MdiOwnAdReg;
  WORD MdiLinkPartnerAdReg;
  bdd_t *bddp = bdp->bddp;

  /* First check if we are on 82562EH */
  /* If we are then we are always 1Mpbs half duplex */
  if (bddp->flags & IS_82562EH)
  {
    bddp->cur_line_speed = 1;
    bddp->cur_dplx_mode = HALF_DUPLEX;
    return;
  }

  /* First we should check to see if we have link */
  /* If we don't have link no reason to print a speed and duplex */
  if ((bddp->rev_id >= 8) || (bddp->flags & IS_ICH))
  {
    if (!(bddp->scbp->scb_ext.d101m_scb.scb_gen_stat & BIT_0))
    {
      bddp->cur_line_speed = 0;
      bddp->cur_dplx_mode = NO_DUPLEX;
      return;
    }
  }
  else
  {
    /* No GSR so lets get it from the MDI status reg */
    /* have to read it twice because it is a sticky bit */
    e100_MdiRead (bdp, MDI_STATUS_REG, bddp->phy_addr, &(MdiStatusReg));
    e100_MdiRead (bdp, MDI_STATUS_REG, bddp->phy_addr, &(MdiStatusReg));
    if (!(MdiStatusReg & BIT_2))
    {
      bddp->cur_line_speed = 0;
      bddp->cur_dplx_mode = NO_DUPLEX;
      return;
    }
  }


  /* On the 82559 and later controllers, speed/duplex is part of the *
   * SCB. So, we save an MdiRead and get these from the SCB. * */
  if ((bddp->rev_id >= 8) || (bddp->flags & IS_ICH))
  {
    /* Read speed */
    if (bddp->scbp->scb_ext.d101m_scb.scb_gen_stat & BIT_1)
      bddp->cur_line_speed = 100;
    else
      bddp->cur_line_speed = 10;

    /* Read duplex */
    if (bddp->scbp->scb_ext.d101m_scb.scb_gen_stat & BIT_2)
      bddp->cur_dplx_mode = FULL_DUPLEX;
    else
      bddp->cur_dplx_mode = HALF_DUPLEX;
  }

  /* If this is a Phy 100, then read bits 1 and 0 of extended register 0,
   * * to get the current speed and duplex settings. */
  if ((PhyId == PHY_100_A) || (PhyId == PHY_100_C) || (PhyId == PHY_82555_TX))
  {
    /* Read Phy 100 extended register 0 */
    e100_MdiRead (bdp, EXTENDED_REG_0, bddp->phy_addr, &MdiMiscReg);

    /* Get current speed setting */
    if (MdiMiscReg & PHY_100_ER0_SPEED_INDIC)
      bddp->cur_line_speed = 100;
    else
      bddp->cur_line_speed = 10;

    /* Get current duplex setting -- if bit is set then FDX is enabled */
    if (MdiMiscReg & PHY_100_ER0_FDX_INDIC)
      bddp->cur_dplx_mode = FULL_DUPLEX;
    else
      bddp->cur_dplx_mode = HALF_DUPLEX;

#ifdef PHY_DEBUG
    printk ("Detecting Speed/Dpx from PHY_100 REG_0 0x%x\n", MdiMiscReg);
#endif

    return;
  }

  /* See if link partner was capable of Auto-Negotiation (bit 0, reg 6) */
  e100_MdiRead (bdp, AUTO_NEG_EXPANSION_REG, bddp->phy_addr, &MdiMiscReg);

  /* See if Auto-Negotiation was complete (bit 5, reg 1) */
  e100_MdiRead (bdp, MDI_STATUS_REG, bddp->phy_addr, &MdiStatusReg);

#ifdef PHY_DEBUG
  printk ("Auto_neg_expn_reg 0x%x Mdi_status_reg 0x%x\n", MdiMiscReg, MdiStatusReg);
#endif

  /* If a True NWAY connection was made, then we can detect speed/dplx *
   * by ANDing our adapter's advertised abilities with our link partner's * 
   * advertised ablilities, and then assuming that the highest common *
   * denominator was chosed by NWAY. */
  if ((MdiMiscReg & NWAY_EX_LP_NWAY) && (MdiStatusReg & MDI_SR_AUTO_NEG_COMPLETE))
  {
#ifdef PHY_DEBUG
    printk ("Detecting Speed/Dpx from NWAY connection\n");
#endif

    /* Read our advertisement register */
    e100_MdiRead (bdp, AUTO_NEG_ADVERTISE_REG, bddp->phy_addr, &MdiOwnAdReg);

    /* Read our link partner's advertisement register */
    e100_MdiRead (bdp, AUTO_NEG_LINK_PARTNER_REG, bddp->phy_addr, &MdiLinkPartnerAdReg);

#ifdef PHY_DEBUG
    printk ("Auto_neg_ad_reg 0x%x Auto_neg_link_partner 0x%x\n", MdiOwnAdReg, MdiLinkPartnerAdReg);
#endif

    /* AND the two advertisement registers together, and get rid of any
     * * extraneous bits. */
    MdiOwnAdReg &= (MdiLinkPartnerAdReg & NWAY_LP_ABILITY);

    /* Get speed setting */
    if (MdiOwnAdReg & (NWAY_AD_TX_HALF_DPX | NWAY_AD_TX_FULL_DPX | NWAY_AD_T4_CAPABLE))
      bddp->cur_line_speed = 100;
    else
      bddp->cur_line_speed = 10;

    /* Get duplex setting -- use priority resolution algorithm */
    if (MdiOwnAdReg & (NWAY_AD_T4_CAPABLE))
    {
      bddp->cur_dplx_mode = HALF_DUPLEX;
      return;
    }
    else if (MdiOwnAdReg & (NWAY_AD_TX_FULL_DPX))
    {
      bddp->cur_dplx_mode = FULL_DUPLEX;
      return;
    }
    else if (MdiOwnAdReg & (NWAY_AD_TX_HALF_DPX))
    {
      bddp->cur_dplx_mode = HALF_DUPLEX;
      return;
    }
    else if (MdiOwnAdReg & (NWAY_AD_10T_FULL_DPX))
    {
      bddp->cur_dplx_mode = FULL_DUPLEX;
      return;
    }
    else
    {
      bddp->cur_dplx_mode = HALF_DUPLEX;
      return;
    }
  }

  /* If we are connected to a dumb (non-NWAY) repeater or hub, and the line 
   * speed was determined automatically by parallel detection, then we have
   * no way of knowing exactly what speed the PHY is set to unless that PHY
   * has a propietary register which indicates speed in this situation.  The
   * NSC TX PHY does have such a register.  Also, since NWAY didn't establish
   * the connection, the duplex setting should HALF duplex. 
   */

  bddp->cur_dplx_mode = HALF_DUPLEX;

  if (PhyId == PHY_NSC_TX)
  {
    /* Read register 25 to get the SPEED_10 bit */
    e100_MdiRead (bdp, NSC_SPEED_IND_REG, bddp->phy_addr, &MdiMiscReg);

    /* If bit 6 was set then we're at 10mb */
    if (MdiMiscReg & NSC_TX_SPD_INDC_SPEED)
      bddp->cur_line_speed = 10;
    else
      bddp->cur_line_speed = 100;

    if (e100_debug > 2)
      printk ("Detecting Speed/Dpx from non-NWAY NSC NSC_SPEED 0x%x\n", MdiMiscReg);
  }
  else
  {
    /* If we don't know what line speed we are set at, then we'll default 
     * to 10mbs */
#ifdef PHY_DEBUG
    printk ("Line speed unknown set at 10Mb\n");
#endif
    bddp->cur_line_speed = 10;
  }
}


/* 
 * Procedure: e100_ForceSpeedAndDuplex
 *
 * Description: This routine forces line speed and duplex mode of the
 * adapter based on the values the user has set in e100.c.
 *
 * Arguments:  bdp - Pointer to the bd_config_t structure for the board
 *
 * Returns: void
 *
 */
void e100_ForceSpeedAndDuplex (bd_config_t * bdp)
{
  bdd_t *bddp = (bdd_t *) bdp->bddp;
  WORD control;
  int board_speed_duplex;

  /* If we are on 82562EH this doesn't apply */
  if (bddp->flags & IS_82562EH)
  {
    return;
  }

  board_speed_duplex = e100_speed_duplex[bdp->bd_number];

  bdp->flags |= DF_SPEED_FORCED;
  e100_MdiRead (bdp, MDI_CONTROL_REG, bddp->phy_addr, &control);
  control &= ~MDI_CR_AUTO_SELECT;

  /* Check e100.c values */
  switch (board_speed_duplex)
  {
         /* 10 half */
       case 1:
         control &= ~MDI_CR_10_100;
         control &= ~MDI_CR_FULL_HALF;
         bddp->cur_line_speed = 10;
         bddp->cur_dplx_mode = HALF_DUPLEX;
         break;

         /* 10 full */
       case 2:
         control &= ~MDI_CR_10_100;
         control |= MDI_CR_FULL_HALF;
         bddp->cur_line_speed = 10;
         bddp->cur_dplx_mode = FULL_DUPLEX;
         break;

         /* 100 half */
       case 3:
         control |= MDI_CR_10_100;
         control &= ~MDI_CR_FULL_HALF;
         bddp->cur_line_speed = 100;
         bddp->cur_dplx_mode = HALF_DUPLEX;
         break;

         /* 100 full */
       case 4:
         control |= MDI_CR_10_100;
         control |= MDI_CR_FULL_HALF;
         bddp->cur_line_speed = 100;
         bddp->cur_dplx_mode = FULL_DUPLEX;
         break;
  }

  e100_MdiWrite (bdp, MDI_CONTROL_REG, bddp->phy_addr, control);

  mdelay (2000);
}


/* 
 * Procedure: e100_phy_check
 * 
 * Arguments:  bdp - Pointer to the bd_config_t structure for the board
 *                  
 *
 * Returns: void
 *
 */
void e100_phy_check (bd_config_t * bdp)
{
  bdd_t *bddp = (bdd_t *) bdp->bddp;
  long LinkState;

  if (bddp->flags & PRINT_SPEED_DPLX)
  {
    e100_FindPhySpeedAndDpx (bdp, bddp->PhyId);
    bddp->flags &= ~PRINT_SPEED_DPLX;

    if (bddp->cur_dplx_mode == HALF_DUPLEX)
      printk ("e100: %s NIC Link is Up %d Mbps Half duplex\n", bdp->device->name, bddp->cur_line_speed);
    else
      printk ("e100: %s NIC Link is Up %d Mbps Full duplex\n", bdp->device->name, bddp->cur_line_speed);
  }

  LinkState = e100_PhyLinkState (bdp);
  if (LinkState && (bddp->flags & INVALID_SPEED_DPLX))
  {
    bddp->flags &= ~INVALID_SPEED_DPLX;
    bddp->flags |= PRINT_SPEED_DPLX;
  }

  if ((!(bdp->flags & DF_PHY_82555)) || (bdp->flags & DF_SPEED_FORCED))
    return;

  if (LinkState)
  {
    switch (bddp->PhyState)
    {
         case 0:
           break;
         case 1:
           e100_MdiWrite (bdp, PHY_82555_SPECIAL_CONTROL, bddp->phy_addr, 0x0000);
           break;
         case 2:
           e100_MdiWrite (bdp, PHY_82555_MDI_EQUALIZER_CSR, bddp->phy_addr, 0x3000);
           break;
    }
    bddp->PhyState = 0;
    bddp->PhyDelay = 0;
  }
  else if (!bddp->PhyDelay--)
  {
    switch (bddp->PhyState)
    {
         case 0:
           e100_MdiWrite (bdp, PHY_82555_SPECIAL_CONTROL, bddp->phy_addr, EXTENDED_SQUELCH_BIT);
           bddp->PhyState = 1;
           break;
         case 1:
           e100_MdiWrite (bdp, PHY_82555_SPECIAL_CONTROL, bddp->phy_addr, 0x0000);
           e100_MdiWrite (bdp, PHY_82555_MDI_EQUALIZER_CSR, bddp->phy_addr, 0x2010);
           bddp->PhyState = 2;
           break;
         case 2:
           e100_MdiWrite (bdp, PHY_82555_MDI_EQUALIZER_CSR, bddp->phy_addr, 0x3000);
           bddp->PhyState = 0;
           break;
    }

    e100_MdiWrite (bdp, MDI_CONTROL_REG, bddp->phy_addr, MDI_CR_AUTO_SELECT | MDI_CR_RESTART_AUTO_NEG);
    bddp->PhyDelay = 3;
  }
}


void e100_ResetPhy (bd_config_t * bdp)
{
  WORD mdi_control_reg;
  bdd_t *bddp = bdp->bddp;


  mdi_control_reg = (MDI_CR_AUTO_SELECT | MDI_CR_RESTART_AUTO_NEG | MDI_CR_RESET);

  e100_MdiWrite (bdp, MDI_CONTROL_REG, bddp->phy_addr, mdi_control_reg);

  udelay (100);
}


/* 
 * Procedure: e100_PhyLinkState
 * 
 * Description: This routine updates the link status of the adapter
 *
 * Arguments:  bdp - Pointer to the bd_config_t structure for the board
 *                  
 *
 * Returns: B_TRUE - If a link is found
 *              B_FALSE - If there is no link
 *
 */
boolean_t e100_PhyLinkState (bd_config_t * bdp)
{

  bdd_t *bddp = (bdd_t *) bdp->bddp;
  WORD Status;

  /* Check link status */
  if (e100_GetPhyLinkState (bdp) == B_TRUE)
  {
    /* Link is up */
    if (!(bdp->flags & DF_LINK_UP))
    {
#ifdef IANS
      bddp->cur_link_status = IANS_STATUS_LINK_OK;
#endif
      bdp->flags |= DF_LINK_UP;
    }
    return (B_TRUE);
  }
  /* Link is down */
  if (bdp->flags & DF_LINK_UP)
  {
#ifdef IANS
    bddp->cur_link_status = IANS_STATUS_LINK_FAIL;
#endif

    bdp->flags &= ~DF_LINK_UP;

    /* Invalidate the speed and duplex values */
    bddp->flags |= INVALID_SPEED_DPLX;

    /* reset the zerol lock state */
    bdp->ZeroLockState = ZLOCK_INITIAL;

    // set auto lock for phy auto-negotiation on link up
    e100_MdiWrite (bdp, PHY_82555_MDI_EQUALIZER_CSR, bddp->phy_addr, 0);

    printk ("e100: %s NIC Link is Down\n", bdp->device->name);
  }

  return (B_FALSE);
}

/* 
 * Procedure: e100_GetPhyLinkState
 * 
 * Description: This routine checks the link status of the adapter
 *
 * Arguments:  bdp - Pointer to the bd_config_t structure for the board
 *                  
 *
 * Returns: B_TRUE - If a link is found
 *              B_FALSE - If there is no link
 *
 */
boolean_t e100_GetPhyLinkState (bd_config_t * bdp)
{

  bdd_t *bddp = (bdd_t *) bdp->bddp;
  WORD Status;

  /* Check link status */
  /* If the controller is a 82559 or later one, link status is available
   * from the CSR. This avoids the MdiRead. */
  if ((bddp->rev_id >= 8) || (bddp->flags & IS_ICH))
  {
    if (bddp->scbp->scb_ext.d101m_scb.scb_gen_stat & BIT_0)
    {
      /* Link is up */
      return (B_TRUE);
    }
    else
      /* Link is down */
      return (B_FALSE);
  }
  else
  {
    /* Read the status register */
    e100_MdiRead (bdp, MDI_STATUS_REG, bddp->phy_addr, &Status);

    /* Read the status register again because of sticky bits */
    e100_MdiRead (bdp, MDI_STATUS_REG, bddp->phy_addr, &Status);

    if (Status & MDI_SR_LINK_STATUS)
    {
      return (B_TRUE);
    }
    else
    {
      return (B_FALSE);
    }
  }
}

/* 
 * Procedure:   e100_MdiWrite
 *
 * Description: This routine will write a value to the specified MII register
 *              of an external MDI compliant device (e.g. PHY 100).  The
 *              command will execute in polled mode.
 *
 * Arguments:
 *      bdp - Ptr to this card's e100_bdconfig structure
 *      RegAddress - The MII register that we are writing to
 *      PhyAddress - The MDI address of the Phy component.
 *      DataValue - The value that we are writing to the MII register.
 *
 * Returns:
 *      NOTHING
 */
void e100_MdiWrite (bd_config_t * bdp, DWORD RegAddress, DWORD PhyAddress, WORD DataValue)
{
  bdd_t *bddp;
  int e100_retry;
  DWORD temp_val;

  bddp = (pbdd_t) bdp->bddp;    /* get the bddp for this board */

  temp_val = (((DWORD) DataValue) | (RegAddress << 16) | (PhyAddress << 21) | (MDI_WRITE << 26));
  bddp->scbp->scb_mdi_cntrl = temp_val;

  /* wait 20usec before checking status */
  drv_usecwait (20);            /* Removed mstous. */

  /* poll for the mdi write to complete */
  e100_retry = E100_CMD_WAIT;
  while ((!(bddp->scbp->scb_mdi_cntrl & MDI_PHY_READY)) && (e100_retry))
  {
    drv_usecwait (20);          /* Removed mstous. */
    e100_retry--;
  }
}


/* 
 * Procedure:   e100_MdiRead
 *
 * Description: This routine will read a value from the specified MII register
 *              of an external MDI compliant device (e.g. PHY 100), and return
 *              it to the calling routine.  The command will execute in polled
 *              mode.
 *
 * Arguments:
 *      bdp - Ptr to this card's e100_bdconfig structure
 *      RegAddress - The MII register that we are reading from
 *      PhyAddress - The MDI address of the Phy component.
 *
 * Results:
 *      DataValue - The value that we read from the MII register.
 *
 * Returns:
 *      NOTHING
 */
void e100_MdiRead (bd_config_t * bdp, DWORD RegAddress, DWORD PhyAddress, WORD * DataValue)
{
  bdd_t *bddp;
  int e100_retry;
  DWORD temp_val;

  bddp = (pbdd_t) bdp->bddp;    /* get the bddp for this board */

  /* Issue the read command to the MDI control register. */
  temp_val = ((RegAddress << 16) | (PhyAddress << 21) | (MDI_READ << 26));
  bddp->scbp->scb_mdi_cntrl = temp_val;

  /* wait 20usec before checking status */
  drv_usecwait (20);            /* Removed mstous. */

  /* poll for the mdi read to complete */
  e100_retry = E100_CMD_WAIT;
  while ((!(bddp->scbp->scb_mdi_cntrl & MDI_PHY_READY)) && (e100_retry))
  {
    drv_usecwait (20);
    e100_retry--;
  }

  /* return (0) blindly as datavalue if we have timed out * and hope that
   * someone above smells a rat */

  if (!e100_retry)
    *DataValue = 0;


  *DataValue = (WORD) bddp->scbp->scb_mdi_cntrl;
}


/***************************************************************************/
/***************************************************************************/
/*       Checksum Functions                                                */
/***************************************************************************/


/* 
 * Looks for an TCP/IP or UDP/IP packet.
 *
 * Arguments:  int *HeaderOffset. Where does the ethernet header stop.
 *             struct sk_buff *. Pointer to the packet data
 *
 * Returns: IP Packet if we can checksum this packet.
 *          NULL if we can not checksum this packet.
 *
 */
ip_v4_header *e100_check_for_ip (int *HeaderOffset, struct sk_buff *skb)
{
  ip_v4_header         *IPPacket = NULL;
  ethernet_snap_header *snapHeader;
  ethernet_ii_header   *eIIHeader;

  snapHeader = (ethernet_snap_header *) skb->data;
  eIIHeader = (ethernet_ii_header *) skb->data;

  /* Check for Ethernet_II IP encapsulation */
  if (eIIHeader->TypeLength == IP_PROTOCOL)
  {
    /* We need to point the IPPacket past the Ethernet_II header. */
    IPPacket = (ip_v4_header *) ((BYTE *) eIIHeader + sizeof (ethernet_ii_header));

    if (IPPacket->ProtocolCarried == TCP_PROTOCOL)
    {
      *HeaderOffset = sizeof (ethernet_ii_header);
      return IPPacket;
    }

    if (IPPacket->ProtocolCarried == UDP_PROTOCOL)
    {
      *HeaderOffset = sizeof (ethernet_ii_header);
      return IPPacket;
    }
  }
  /* To qualify for IP encapsulated SNAP the type/length field must contain 
   * a length value i.e. less than 1500  */
  else if (snapHeader->TypeLength <= MAX_PKT_LEN_FIELD)
  {
    /* Check for SNAP IP encapsulation  */
    if (snapHeader->DSAP == 0xAA &&
        snapHeader->SSAP == 0xAA &&
        snapHeader->Ctrl == 0x03)
    {
      if (*(WORD*)&snapHeader->ProtocolId == IP_PROTOCOL)
      {
        IPPacket = (ip_v4_header*) ((BYTE*)snapHeader + sizeof(ethernet_snap_header));

        if (IPPacket->ProtocolCarried == UDP_PROTOCOL)
        {
          *HeaderOffset = sizeof (ethernet_snap_header);
          return IPPacket;
        }
        if (IPPacket->ProtocolCarried == TCP_PROTOCOL)
        {
          *HeaderOffset = sizeof (ethernet_snap_header);
          return IPPacket;
        }
      }
    }
  }

  /* If we didn't find IP encapsulation */
  return (NULL);
}



/* 
 * Calculates the checksum value from the hardware value.
 */
boolean_t e100_calculate_checksum (ip_v4_header *IPPacket, DWORD HeaderOffset,
                                   struct sk_buff *skb, DWORD packetSize)
{
  DWORD i, addLength, adjustValue = 0;
  WORD  totalPacketLength, hwCheckSum;
  WORD  IPTotalLength = 0;
  BYTE *dataFragment;

  udp_header *udpPacket;

  union {
    BYTE byteValue[2];
    WORD wordValue;
  } protocolCarriedSwapped = { { 0 } };

  union {
    BYTE byteValue[2];
    WORD wordValue;
  } padWord = { { 0 } };

  if (skb->len <= MINIMUM_ETHERNET_PACKET_SIZE)
  {
    skb->ip_summed = CHECKSUM_NONE;
    return B_TRUE;
  }

  /* Initialize the total packet length. This could include the CRC if we
   * are DMAing CRC's.
   */
  totalPacketLength = packetSize;

  /* Initialize the dataFragment pointer to point at the begining of the
   * data area.
   */
  dataFragment = (BYTE *) skb->data;

  /* If we have a UDP packet we are now pointing to it.
  */
  udpPacket = (udp_header *) ((BYTE *) IPPacket + (IPPacket->HdrLength * 4));

  /* If we have a UDP packet with a zero checksum, do not perform checksum
   * validation and return ZERO
   */
  if ((IPPacket->ProtocolCarried == UDP_PROTOCOL) && (udpPacket->Checksum == 0))
     return (0);

  /* Add up the IP Header and whats left of a SNAP header if it is a SNAP
   * frame. Have to convert HeaderOffset to words from bytes
   */
  addLength = (IPPacket->HdrLength * 2) + ((HeaderOffset - HARDWARE_CHECKSUM_START_OFFSET) / 2);
  for (i = 0; i < addLength; i++)
     adjustValue += *((WORD *) IPPacket + i);

  /* Now add back in the pseudo header parts.
   */
  adjustValue -= IPPacket->SrcAddr.Fields.SrcAddrLo;
  adjustValue -= IPPacket->SrcAddr.Fields.SrcAddrHi;
  adjustValue -= IPPacket->DestAddr.Fields.DestAddrLo;
  adjustValue -= IPPacket->DestAddr.Fields.DestAddrHi;

  /* Now take out the protocolID field
   */
  protocolCarriedSwapped.byteValue[0] = 0;
  protocolCarriedSwapped.byteValue[1] = IPPacket->ProtocolCarried;
  adjustValue -= protocolCarriedSwapped.wordValue;

  IPTotalLength = BYTE_SWAP_WORD (IPPacket->Length);

  /*
   * Layer4Length is part of TCP or UDP checksum
   */
  adjustValue -= BYTE_SWAP_WORD (IPTotalLength - (IPPacket->HdrLength * sizeof (DWORD)));

  /* adjustValue = BYTE_SWAP_WORD((WORD)adjustValue); */

  /* This is to take out any padding bytes. If we are posting CRC's this
   * code will need to change.
   */
  if ((totalPacketLength - CHKSUM_SIZE - HeaderOffset) != IPTotalLength)
  {
    padWord.byteValue[1] = *(BYTE *) (dataFragment + totalPacketLength - CHKSUM_SIZE - 1);

    padWord.byteValue[0] = 0;

    adjustValue += padWord.wordValue;
  }

  adjustValue = CHECK_FOR_CARRY (adjustValue);

  /* Get the Checksum value from the end of the packet */
  hwCheckSum = *(WORD *) (dataFragment + (totalPacketLength - CHKSUM_SIZE));

  if ((((adjustValue + ~hwCheckSum) & 0x0000FFFF) == 0xFFFF) || (adjustValue + ~hwCheckSum == 0))
  {

        /****************************************************************
         * Set the appropriate status bit to indicate that we performed
         * the transport layer protocol checksum and it is valid.
         **************************************************************/

    skb->ip_summed = CHECKSUM_HW;
    skb->csum = hwCheckSum;
    return B_TRUE;
  }
  else
  {
        /****************************************************************
         * Set the appropriate status bit to indicate that we performed
         * the transport layer protocol checksum and it is NOT valid.
         **************************************************************/
    skb->ip_summed = CHECKSUM_NONE;
    return B_FALSE;
  }
}


/*
 * Procedure: e100_D102_check_checksum
 * 
 * Description: Checks the D102 RFD flags to see if the checksum passed
 *
 * Arguments:  struct sk_buff *skb. Pointer to the packet data
 *               
 */
void e100_D102_check_checksum (bd_config_t * bdp, struct sk_buff *skb)
{
  rfd_t *rfd = RFD_POINTER (skb, bdp->bddp);
  DWORD status = rfd->rfd_header.cb_status;

  if (                          /* frame parsing succeed */
       (status & RFD_PARSE_BIT)
       /* frame recognized as a TCP or UDP frame */
       && (((rfd->rcvparserstatus & CHECKSUM_PROTOCOL_MASK) == RFD_TCP_PACKET) ||
           ((rfd->rcvparserstatus & CHECKSUM_PROTOCOL_MASK) == RFD_UDP_PACKET))
       /* checksum was calculated */
       && (rfd->checksumstatus & TCPUDP_CHECKSUM_BIT_VALID)
       /* and it happens to be valid */
       && (rfd->checksumstatus & TCPUDP_CHECKSUM_VALID))

    skb->ip_summed = CHECKSUM_UNNECESSARY;

}

/***************************************************************************/
/***************************************************************************/
/*       Functions for the 82562EH Home Phoneline Network Connection       */
/***************************************************************************/


/****************************************************************************
 * Name:          Phy82562EHNoiseFloorWrite
 * 
 * Description:   Writes input parameter values to 82562EH's NSE_FLOOR/
 *                CEILING register.
 *
 * Arguments:     bddp - pointer to the bddp object.
 *                Value - the value to be written.
 *
 * Returns:       Nothing
 *
 ***************************************************************************/
void Phy82562EHNoiseFloorWrite (bdd_t * bddp, BYTE Value)
{
  e100_MdiWrite (bddp->bdp, PHY_82562EH_NSE_FLOOR_CEILING_REG, bddp->phy_addr,
                 (WORD) (Value | 0xd000));
}


/****************************************************************************
 * Name:          Phy82562EHNoiseCounterClear
 *
 * Description:   Clears NSE_EVENTS bit of CONTROL register.
 *
 * Arguments:     bddp - pointer to the bddp object.
 *
 * Returns:       Nothing
 *
 ***************************************************************************/
void Phy82562EHNoiseCounterClear (bdd_t * bddp)
{
  WORD RegVal;

  e100_MdiRead (bddp->bdp, PHY_82562EH_CONTROL_REG, bddp->phy_addr, &RegVal);
  RegVal |= 0x0040;
  e100_MdiWrite (bddp->bdp, PHY_82562EH_CONTROL_REG, bddp->phy_addr, RegVal);
}

/****************************************************************************
 * Name:          Phy82562EHNoiseEventsRead
 *
 * Description:   This routine will read 82562EH NSE_ATTACK/EVENT register.
 *
 * Arguments:     bddp - pointer to the bddp object.
 *
 * Returns:       BYTE value from NSE_ATTACK/EVENT register
 *
 ***************************************************************************/
BYTE Phy82562EHNoiseEventsRead (bdd_t * bddp)
{
  WORD RegVal;

  e100_MdiRead (bddp->bdp, PHY_82562EH_NSE_ATTACK_EVENT_REG, bddp->phy_addr,
                &RegVal);
  return (BYTE) (RegVal >> 8);
}


/****************************************************************************
 * Name:          Phy82562EHDelayMilliseconds
 *
 * Description:   Stalls execution for a specified number of milliseconds. 
 *
 * Arguments:     Time - milliseconds to delay
 *
 * Returns:       Nothing
 *
 ***************************************************************************/
void Phy82562EHDelayMilliseconds (int Time)
{
  udelay (Time);
}


/****************************************************************************
 * Name:          Phy82562EHNoiseEventsWithDelayCount
 *
 * Description:
 *
 * Arguments:     bddp - pointer to the bddp object.
 *                NoiseFloor -
 *                Delay - 
 *
 * Returns:       Nothing
 *
 ***************************************************************************/
BYTE Phy82562EHNoiseEventsWithDelayCount (bdd_t * bddp, BYTE NoiseFloor, long Delay)
{
  Phy82562EHNoiseFloorWrite (bddp, NoiseFloor);
  Phy82562EHNoiseCounterClear (bddp);
  Phy82562EHDelayMilliseconds (Delay);
  return Phy82562EHNoiseEventsRead (bddp);
}


/****************************************************************************
 * Name:          Phy82562EHMedianFind
 *
 * Description:
 *
 * Arguments:     bddp - pointer to the bddp object.
 *
 * Returns:       BYTE value of
 *
 ***************************************************************************/
BYTE Phy82562EHMedianFind (bdd_t * bddp)
{

  if (Phy82562EHNoiseEventsWithDelayCount (bddp, 0x10, 90) > 0)
  {
    if (Phy82562EHNoiseEventsWithDelayCount (bddp, 0x18, 60) > 0)
    {
      if (Phy82562EHNoiseEventsWithDelayCount (bddp, 0x1c, 30) > 0)
      {
        return (0)x1d;
      }
      else
      {
        return (0)x19;
      }
    }
    else
    {
      if (Phy82562EHNoiseEventsWithDelayCount (bddp, 0x14, 30) > 0)
      {
        return (0)x15;
      }
      else
      {
        return (0)x11;
      }
    }
  }
  else
  {
    if (Phy82562EHNoiseEventsWithDelayCount (bddp, 0x08, 60) > 0)
    {
      if (Phy82562EHNoiseEventsWithDelayCount (bddp, 0x0c, 30) > 0)
      {
        return (0)x0d;
      }
      else
      {
        return (0)x09;
      }
    }
    else
    {
      if (Phy82562EHNoiseEventsWithDelayCount (bddp, 0x04, 30) > 0)
      {
        return (0)x05;
      }
      else
      {
        return (0)x02;
      }
    }
  }
}


/****************************************************************************
 * Name:          Phy82562EHAddressSpaceSwitch
 *
 * Description:   Switches 82562EH address space page 1/0 based on Page parameter.
 *
 * Arguments:     bddp - pointer to the bddp object.
 *
 * Returns:       Nothing
 *
 ***************************************************************************/
void Phy82562EHAddressSpaceSwitch (bdd_t * bddp, long Page)
{
  WORD RegVal;

  /* Switch to 82562EH address space page 1/0 by writing to RAP bit (Bit 0 )
   * in Control register (Address 10 Hex)  1/0.
   */
  e100_MdiRead (bddp->bdp, PHY_82562EH_CONTROL_REG, bddp->phy_addr, &RegVal);
  RegVal = (Page ? (RegVal | PHY_82562EH_CR_RAP) : (RegVal & ~(PHY_82562EH_CR_RAP)));
  e100_MdiWrite (bddp->bdp, PHY_82562EH_CONTROL_REG, bddp->phy_addr, RegVal);
}


/****************************************************************************
 * Name:          Phy82562EHRegisterInit
 *
 * Description:   Performs 82562EH PHY register initialization.
 *
 * Arguments:     bddp - pointer to the bddp object.
 *
 * Returns:       Nothing
 *
 ***************************************************************************/
void Phy82562EHRegisterInit (bdd_t * bddp)
{
  /* 82562EH CSR initialization value.
   *
   * This are the csrs value recommended for 82562EH regardless the W/A.
   *
   *                                 A0 value   A1 value
   * 0    0x1B - RX_CONTROL          0x0009     0x0008
   * 0    0x11 - STATUS_AND_MODE     not set    0x0100
   * 0    0x19 - NSE_FLOOR/CEILING   0xD010     not set
   * 1    0x12 - AFE_CONTROL1        0xA6A0     0x6600
   * 1    0x13 - AFE_CONTROL2        0x0100     0x0100
   */

  Phy82562EHAddressSpaceSwitch (bddp, PHY_82562EH_ADDR_SPACE_PAGE_0);
  e100_MdiWrite (bddp->bdp, PHY_82562EH_STATUS_MODE_REG, bddp->phy_addr, 0x0100);
  e100_MdiWrite (bddp->bdp, PHY_82562EH_RX_CONTROL_REG, bddp->phy_addr, 0x0008);

  Phy82562EHAddressSpaceSwitch (bddp, PHY_82562EH_ADDR_SPACE_PAGE_1);
  e100_MdiWrite (bddp->bdp, PHY_82562EH_AFE_CONTROL1_REG, bddp->phy_addr, 0x6600);
  e100_MdiWrite (bddp->bdp, PHY_82562EH_AFE_CONTROL2_REG, bddp->phy_addr, 0x0100);
}


/****************************************************************************
 * Name:          Phy82562EHNoiseFloorCalculate
 *
 * Description:   Performs 82562EH PHY noise calculations
 *
 * Arguments:     bddp - pointer to the bddp object.
 *
 * Returns:       Nothing
 *
 ***************************************************************************/
void Phy82562EHNoiseFloorCalculate (bdd_t * bddp)
{
  WORD Samples[PHY_82562EH_MAX_TABLE];
  WORD RegVal;
  WORD MaxVal;
  BYTE NoiseFloor;
  boolean_t Stable;
  int Index;

  /* 
   * 1. Set the mode for initialization:
   * a. Move to page 1 by setting RAP bit (bit 0) in the Control Register 
   * (address 10H) to value of 1.
   * b. Write    AFE_CONTROL1 register       (address 12H)  to value     A600
   * c. Write    AFE_CONTROL2 register       (address 13H)  to value     0900
   * d. Move to page 0 by resetting RAP bit (bit 0) in the Control Register 
   * (address 10H) to value of 0
   * e. Write    RX_CONTROL register         (address 1BH)  to value     0008
   * f. Write    STATUS_MODE register        (address 11H)  to value     0100
   * g. Write    NSE_EVENTS/ATTACK register  (address 0x1A) to value     0022
   * h. Write    NOISE/PEACK register        (address 0x18) to value     7F00
   */

  Phy82562EHAddressSpaceSwitch (bddp, PHY_82562EH_ADDR_SPACE_PAGE_1);

  e100_MdiWrite (bddp->bdp, PHY_82562EH_AFE_CONTROL1_REG, bddp->phy_addr, PHY_82562EH_AFE_CR1_WA_INIT);
  e100_MdiWrite (bddp->bdp, PHY_82562EH_AFE_CONTROL2_REG, bddp->phy_addr, PHY_82562EH_AFE_CR2_WA_INIT);

  Phy82562EHAddressSpaceSwitch (bddp, PHY_82562EH_ADDR_SPACE_PAGE_0);

  e100_MdiWrite (bddp->bdp, PHY_82562EH_RX_CONTROL_REG, bddp->phy_addr, PHY_82562EH_RX_CR_WA_INIT);
  e100_MdiWrite (bddp->bdp, PHY_82562EH_STATUS_MODE_REG, bddp->phy_addr, PHY_82562EH_STATUS_MODE_WA_INIT);
  e100_MdiWrite (bddp->bdp, PHY_82562EH_NSE_ATTACK_EVENT_REG, bddp->phy_addr, PHY_82562EH_NSE_ATTACK_EVENT_WA_INIT);
  e100_MdiWrite (bddp->bdp, PHY_82562EH_NOISE_PEAK_LVL_REG, bddp->phy_addr, PHY_82562EH_NOISE_PEAK_LVL_WA_INIT);

  Phy82562EHDelayMilliseconds (1);

  /* Find and write an appropriate noise floor value */
  NoiseFloor = Phy82562EHMedianFind (bddp);
  Phy82562EHNoiseFloorWrite (bddp, NoiseFloor);

  /* Perform X readings of noise register with Y msec delay between */
  for (Index = 0; Index < bddp->Phy82562EHSampleCount; Index++)
  {
    e100_MdiRead (bddp->bdp, PHY_82562EH_NOISE_PEAK_LVL_REG, bddp->phy_addr, &Samples[Index]);
    Phy82562EHDelayMilliseconds (bddp->Phy82562EHSampleDelay);
  }

  /* Find maximum and check for stability */
  MaxVal = 0;
  Stable = TRUE;
  for (Index = 0; Index < bddp->Phy82562EHSampleCount; Index++)
  {
    if (Samples[Index] > MaxVal)
    {
      MaxVal = Samples[Index];
    }
    if (Samples[Index] != Samples[0])
    {
      Stable = FALSE;
    }
  }

  RegVal = MaxVal;

  if (!Stable || (NoiseFloor == 0x02))
  {
    RegVal += bddp->Phy82562EHSampleFilter;
  }

  /* Set the normal operation mode:
   *
   * a. Write    NSE_EVENTS/ATTACK register  (address 0x1A) to value     FFF4
   * b. Move to page 1 by setting RAP bit (bit 0) in the CONTROL register
   *      (address 10H).
   * c. Write    AFE_CONTROL1 register       (address 12H)  to value     6600
   * d. Write    AFE_CONTROL2 register       (address 13H)  to value     0100
   * e. Move to page 0 by resetting RAP bit (bit 0) in the CONTROL register
   *      (address 10H) to value of 0
   */

  /* Write final noise floor */
  NoiseFloor = (BYTE) (RegVal & 0x00FF);
  Phy82562EHNoiseFloorWrite (bddp, NoiseFloor);

  /* Restore to normal working values */
  e100_MdiWrite (bddp->bdp, PHY_82562EH_NSE_ATTACK_EVENT_REG, bddp->phy_addr, PHY_82562EH_NSE_ATTACK_WORK_VALUE);

  Phy82562EHAddressSpaceSwitch (bddp, PHY_82562EH_ADDR_SPACE_PAGE_1);

  e100_MdiWrite (bddp->bdp, PHY_82562EH_AFE_CONTROL1_REG, bddp->phy_addr, PHY_82562EH_AFE_CR1_WORK_VALUE);
  e100_MdiWrite (bddp->bdp, PHY_82562EH_AFE_CONTROL2_REG, bddp->phy_addr, PHY_82562EH_AFE_CR2_WORK_VALUE);

  Phy82562EHAddressSpaceSwitch (bddp, PHY_82562EH_ADDR_SPACE_PAGE_0);
}


/****************************************************************************
 * Name:          PhyProcessing82562EHInit
 *
 * Description: This routine will do all of initializatioin necessary for
 *              the 82562EH phy.
 *
 * Arguments:     bddp - ptr to bddp object instance
 *
 * Returns:       Nothing
 *
 ***************************************************************************/
void PhyProcessing82562EHInit (bdd_t * bddp)
{
  Phy82562EHSpeedPowerSetup (bddp);
  Phy82562EHRegisterInit (bddp);

  if (bddp->Phy82562EHSampleCount > PHY_82562EH_MAX_TABLE)
  {
    bddp->Phy82562EHSampleCount = PHY_82562EH_MAX_TABLE;
  }

  Phy82562EHNoiseFloorCalculate (bddp);
}


/****************************************************************************
 * Name:          Phy82562EHSpeedPowerSetup
 *
 * Description:   This routine will setup correct speed/power for 82562EH PHY.
 *
 * Arguments:     bddp - ptr to bddp object instance
 *
 * Returns:       Nothing
 *
 ***************************************************************************/
void Phy82562EHSpeedPowerSetup (bdd_t * bddp)
{
  WORD MdiControlReg, controlReg;

  MdiControlReg = MDI_CR_RESET;

  /* Write the MDI control register with our new Phy configuration */
  e100_MdiWrite (bddp->bdp, MDI_CONTROL_REG, bddp->phy_addr, MdiControlReg);

  /* Setting up right 82562EH power/speed. */
  e100_MdiRead (bddp->bdp, PHY_82562EH_CONTROL_REG, bddp->phy_addr, &controlReg);

  /* Check if any user override. Default value after reset is low power. */
  if (PhoneLinePower[bddp->bd_number])
    controlReg |= PHY_82562EH_CR_HP;
  else
    controlReg &= ~(PHY_82562EH_CR_HP);

  /* Check if any user override. Default value after reset is high speed. */
  if (PhoneLineSpeed[bddp->bd_number])
    controlReg |= PHY_82562EH_CR_HS;
  else
    controlReg &= ~(PHY_82562EH_CR_HS);

  /* Ignore incoming command from master HomeRun node. */
  controlReg &= ~(PHY_82562EH_CR_IRC);

  e100_MdiWrite (bddp->bdp, PHY_82562EH_CONTROL_REG, bddp->phy_addr, controlReg);
}













/***************************************************************************/
/***************************************************************************/
/*       Auxilary Functions                                                */
/***************************************************************************/


/* 
 * Procedure:   e100_print_brd_conf
 *
 * Description: This routine printd the board configuration.
 *
 * Arguments:
 *      bdp    - Ptr to this card's e100_bdconfig structure
 *
 * Returns:
 *      NONE
 *
 */
void e100_print_brd_conf (bd_config_t * bdp)
{
  bdd_t *bddp = (bdd_t *) bdp->bddp;
  int offset;
  char e100_name[16];

  printk ("\n%s: %s\n", bdp->device->name, e100id_string);

  offset = bdp->io_end - bdp->io_start;
  //strcpy(e100_name, e100_short_driver_name);
  //strcat(e100_name, e100_numtostr(bdp->bd_number, 10, 1));

  printk ("  Mem:0x%p  IRQ:%d  Speed:%d Mbps  Dx:%s\n",
          (void *) bdp->mem_start,
          bdp->irq_level, bddp->cur_line_speed,
          bddp->cur_dplx_mode == FULL_DUPLEX ? "Full" : ((bddp->cur_dplx_mode == NO_DUPLEX) ? "N/A" : "Half"));
  if (bddp->flags & INVALID_SPEED_DPLX)
  {
    /* Auto negotiation failed so we should display an error */
    printk ("  Failed to detect cable link.\n");
    printk ("  Speed and duplex will be determined at time of connection.\n");
  }

  /* Print the string if checksum Offloading was enabled */
  if (bddp->checksum_offload_enabled)
    printk ("  Hardware receive checksums enabled\n");
  else
    printk ("  Hardware receive checksums disabled\n");
  if (!(bdp->flags & DF_UCODE_LOADED))
    printk ("  ucode was not loaded\n", bdp->bd_number);



}


/* New routine for checking sub-dev & sub-ven IDs. */

/* 
 * Procedure:   e100_GetBrandingMesg
 *
 * Description: This routine checks if the sub_ven and sub_dev found
 * on the board is a valid one for the driver to load. If it is, the
 * routine determines the right string to be printed. The routine knows
 * what IDs to load on by scanning a predefined array.
 *
 * Arguments:
 *      sub_ven - Subsystem vendor ID on the board
 *      sub_dev - Subsystem device ID on the board
 *
 * Returns:
 *      1 - if it is valid to load on this board
 *      0 - if it is not valid to load on this board
 *
 */
static char *e100_GetBrandingMesg (bd_config_t * bdp)
{
  WORD sub_ven, sub_dev;
  char *mesg = NULL;
  int i = 0;

  sub_ven = bdp->bddp->sub_ven_id;
  sub_dev = bdp->bddp->sub_dev_id;

  /* Go through the list of all valid device, vendor and subsystem IDs */
  while (e100_vendor_info_array[i].idstr != NULL)
  {
    /* Look for a match on device id */
    if ((e100_vendor_info_array[i].dev_id == bdp->bddp->pci_dev->device)
        || (e100_vendor_info_array[i].dev_id == CATCHALL))
    {
      /* Look for a match on sub_ven */
      if ((e100_vendor_info_array[i].sub_ven == sub_ven)
          /* Is this driver to load on all adapters from this vendor? */
          || (e100_vendor_info_array[i].sub_ven == CATCHALL))
      {
        /* Look for a match on sub_dev */
        if ((e100_vendor_info_array[i].sub_dev == sub_dev)
            /* Is this driver to load on all adapters with this subsytem dev id? */
            || (e100_vendor_info_array[i].sub_dev == CATCHALL))
        {
          mesg = e100_vendor_info_array[i].idstr;
          break;
        }
      }
    }
    i++;
  }

  if (mesg)
    strcpy (e100id_string, mesg);
  else
    printk ("e100: Vendor ID Mismatch\n");
  return mesg;

}




/* 
 * Procedure:   e100_dis_intr
 *
 * Description: This routine disables interrupts at the hardware, by setting
 *              the M (mask) bit in the adapter's CSR SCB command word.
 *
 * Arguments:
 *      bdp - Ptr to this card's e100_bdconfig structure
 *
 * Returns:
 *      NOTHING
 */
void e100_dis_intr (bd_config_t * bdp)
{
  bdd_t *bddp;                  /* stores all adapter specific info */

  if (e100_debug > 2)
    printk ("e100_dis_intr\n");

  bddp = (pbdd_t) bdp->bddp;    /* get the bddp for this board */

  /* Disable interrupts on our PCI board by setting the mask bit */
  bddp->scbp->scb_cmd_hi = (BYTE) SCB_INT_MASK;
}


/* 
 * Procedure:   e100_enbl_intr
 *
 * Description: This routine enables interrupts at the hardware, by resetting
 *              the M (mask) bit in the adapter's CSR SCB command word
 *
 * Arguments:
 *      bdp - Ptr to this card's e100_bdconfig structure
 *
 * Returns:
 *      NOTHING
 */
void e100_enbl_intr (bd_config_t * bdp)
{
  bdd_t *bddp;                  /* stores all adapter specific info */

  if (e100_debug > 2)
    printk ("e100_enbl_intr\n");

  bddp = (pbdd_t) bdp->bddp;    /* get the bddp for this board */

  /* Enable interrupts on our PCI board by clearing the mask bit */
  bddp->scbp->scb_cmd_hi = (BYTE) 0;
}


void e100_trigger_SWI (bd_config_t * bdp)
{
  bdd_t *bddp;                  /* stores all adapter specific info */

  bddp = (pbdd_t) bdp->bddp;
  /* Trigger interrupt on our PCI board by asserting SWI bit */
  bddp->scbp->scb_cmd_hi = (BYTE) SCB_SOFT_INT;
}


/************************************************************************
 * Procedure:   e100_numtostr
 *
 * Description: This routine will convert a number to an ASCII string.
 *
 * Arguments:
 *      val    - The number to be converted.
 *    base   - The base to which it will be converted
 *    slen   - The length of the string
 *
 * Returns:
 *    str    - Pointer to sting containing text.
 ************************************************************************/
static char *e100_numtostr (DWORD val, BYTE base, BYTE slen)
{
  static char str[11];          /* Holds a 32bit integer */
  char *sp = &str[9];           /* points to string to be returned */
  BYTE rem = 0;
  BYTE dpads;

  str[10] = 0;                  /* null terminated the string */

  do
  {
    /* start filling up digits backwards */

    /* get the last digit [0-9] or [A-F] */

    *sp = rem = val % base;

    /* move to the next unit's place */
    val /= base;

    /* convert last digit to a char by adding the value of '0' ie 0x30 * 
     * in case rem <= 9 or a suitable adjustment other wise */
    *sp += (rem > 9) ? 'A' - 10 : '0';

    /* move ptr for next digit */
    sp--;
  }
  while (val);

  /* Add preceeding Zeros if string is less than required length */
  dpads = &str[9] - sp;
  if (dpads < slen)
  {
    dpads = slen - dpads;
    while (dpads--)
      *sp-- = '0';
  }
  return (sp + 1);
}


/*****************************************************************************
 * Name:        drv_usecwait
 *
 * Description: This routine causes a delay for the number of useconds
 *              that it is passed.
 *
 * Arguments:
 *      MsecDelay - How many useconds to delay for.
 *
 * Returns:
 *      (none)
 *
 * Modification log:
 * Date      Who  Description
 * --------  ---  --------------------------------------------------------
 *
 *****************************************************************************/

void drv_usecwait (DWORD MsecDelay)
{
  udelay (MsecDelay);
}

/*****************************************************************************
 * Name:        e100_get_pci_info
 *
 * Description: This routine finds all PCI adapters that have the given Device
 *              ID and Vendor ID.  It will store the IRQ, I/O, mem address,
 *              amd node address for each adapter in the
 *              CardsFound structure.  This routine will also enable the bus
 *              master bit on any of our adapters (some BIOS don't do this).
 *
 *
 * Arguments:
 *      vendor_id - Vendor ID of our adapter.
 *      device_id - Device ID of our adapter.
 *
 * Returns:
 *      B_TRUE     - if found pci devices successfully
 *      B_FALSE     - if no pci devices found
 *
 * Modification log:
 * Date      Who  Description
 * --------  ---  --------------------------------------------------------
 *
 *****************************************************************************/
static void e100_get_pci_info (pci_dev_t * pcid, bd_config_t * bdp)
{
  bdd_t *bddp = bdp->bddp;
  DWORD tmp_base_addr;
  WORD pci_cmd = 0;

  if (e100_debug > 2)
    printk ("e100_get_pci_info\n");

  /* dev and ven ID have already been check so it is our device */
  pci_read_config_byte (pcid, PCI_REVISION_ID, (BYTE *) & (bddp->rev_id));
  pci_read_config_word (pcid, PCI_SUBSYSTEM_ID, (WORD *) & (bddp->sub_dev_id));
  pci_read_config_word (pcid, PCI_SUBSYSTEM_VENDOR_ID, (WORD *) & (bddp->sub_ven_id));
  pci_read_config_word (pcid, PCI_COMMAND, (WORD *) & (pci_cmd));

  /* if Bus Mastering is off, turn it on! */
  pci_set_master (pcid);

  /* address #0 is a memory mapping */
  tmp_base_addr = (DWORD) pcid->resource[0].start;
  bddp->scbp = (pscb_t) ioremap ((tmp_base_addr & ~0xf), sizeof (scb_t));
  bddp->mem_size = sizeof (scb_t);
  bdp->mem_start = tmp_base_addr;

  /* address #1 is a IO region */
  tmp_base_addr = (DWORD)  pcid->resource[1].start;
  bdp->io_start = (tmp_base_addr & ~0x3UL);

#if (e100_debug)
  printk ("RevID = 0x%x\n", (BYTE) bddp->rev_id);
  printk ("SUBSYSTEM_ID = 0x%x\n", bddp->sub_dev_id);
  printk ("SUBSYSTEM_VENDOR_ID = 0x%x\n", bddp->sub_ven_id);
  printk ("DeviceNum = 0x%x\n", bddp->dev_num);
  printk ("mem_start = 0x%x\n", bdp->mem_start);
  printk ("io_start = 0x%x\n", bdp->io_start);
  printk ("pci_cmd = 0x%x\n", pci_cmd);
  printk ("i = 0x%x\n", i);
#endif

  bddp->dev_num = pcid->devfn;

}



/*****************************************************************************
 * Name:        e100_pci_write_config
 *
 * Description: This routine allows writing individual bytes/words/dwords
 *        from the PCI Configuration Space of a specific device.
 *
 *
 * Arguments:
 *      pci_dev_t       - Device number of the write target
 *      reg_num         - Register Number to write (0-0xFF)
 *      val             - value to write
 *    wr_type           - whether byte, word or dword
 *
 * Returns:
 *
 * Modification log:
 * Date      Who  Description
 * --------  ---  --------------------------------------------------------
 *
 *****************************************************************************/
void e100_pci_write_config (pci_dev_t * pcid, BYTE reg_num, DWORD val, int wr_type)
{
  switch (wr_type)
  {
       case PCI_BYTE:
         pci_write_config_byte (pcid, reg_num, (BYTE) val);
         break;

       case PCI_WORD:
         pci_write_config_word (pcid, reg_num, (WORD) val);
         break;

       case PCI_DWORD:
         pci_write_config_dword (pcid, reg_num, val);
         break;
  }
  return;
}

/**************************************************************************\
 **
 ** PROC NAME:     e100_handle_zero_lock_state
 **    This function manages a state machine that controls
 **    the driver's zero locking algorithm.
 **    This function is called by e100_watchdog() every ~2 second.
 ** States:
 **    The current link handling state is stored in 
 **    bdp->ZeroLockState, and is one of:
 **    ZLOCK_INITIAL, ZLOCK_READING, ZLOCK_SLEEPING
 **    Detailed description of the states and the transitions
 **    between states is found below.
 **    Note that any time the link is down / there is a reset
 **    state will be changed outside this function to ZLOCK_INITIAL
 ** Algorithm:
 **    1. If link is up & 100 Mbps continue else stay in #1:
 **    2. Set 'auto lock'
 **    3. Read & Store 100 times 'Zero' locked in 1 sec interval
 **    4. If max zero read >= 0xB continue else goto 1
 **    5. Set most popular 'Zero' read in #3
 **    6. Sleep 5 minutes
 **    7. Read number of errors, if it is > 300 goto 2 else goto 6
 ** Data Structures (in DRIVER_DATA):
 **    ZeroLockState           - current state of the algorithm
 **    ZeroLockReadCounter     - counts number of reads (up to 100)
 **    ZeroLockReadData[i] - counts number of times 'Zero' read was i, 0 <= i <= 15
 **    ZeroLockSleepCounter - keeps track of "sleep" time (up to 300 secs = 5 minutes)
 **                                
 ** Parameters:    DRIVER_DATA    *bdp
 **
 **                bdp  - Pointer to HSM's adapter data space
 **
 ** Return Value:  NONE
 **
 ** See Also:      e100_watchdog()
 **
 \**************************************************************************/
static void e100_handle_zero_lock_state (bd_config_t * bdp)
{
  WORD   ArrayPosition;
  WORD   EqRegValue;
  WORD   ErrorCounter;
  BYTE   MostPopularZero;
  bdd_t *bddp = (bdd_t *) bdp->bddp;

  switch (bdp->ZeroLockState)
  {
    case ZLOCK_INITIAL:
#if (DEBUG_LINK > 0)
         printk ("ZeroLockState: INITIAL\n");
#endif
         if (((BYTE)bddp->rev_id <= D102_REV_ID) ||
             !(bddp->cur_line_speed == 100) || !(bdp->flags & DF_LINK_UP))
            break;

         /* initialize hw and sw and start reading
          */
         e100_MdiWrite (bdp, PHY_82555_MDI_EQUALIZER_CSR, bddp->phy_addr, 0);

         /* reset read counters:
          */
         bdp->ZeroLockReadCounter = 0;
         for (ArrayPosition = 0; ArrayPosition < 16; ArrayPosition++)
             bdp->ZeroLockReadData[ArrayPosition] = 0;

         /* start reading in the next call back:
          */
         bdp->ZeroLockState = ZLOCK_READING;

         /* fall through !! */

    case ZLOCK_READING:
         /* state: reading (100 times) zero locked in 1 sec interval
          * prev states: ZLOCK_INITIAL
          * next states: ZLOCK_INITIAL, ZLOCK_SLEEPING
          */
         e100_MdiRead (bdp, PHY_82555_MDI_EQUALIZER_CSR, bddp->phy_addr, &EqRegValue);

         ArrayPosition = (EqRegValue & ZLOCK_ZERO_MASK) >> 4;

         bdp->ZeroLockReadData[ArrayPosition]++;
         bdp->ZeroLockReadCounter++;

#if (DEBUG_LINK > 1)
         printk ("ZeroLockState: Reading %d 'Zero' %04X\n",
                 bdp->ZeroLockReadCounter, (EqRegValue & ZLOCK_ZERO_MASK));
#endif
         if (bdp->ZeroLockReadCounter == ZLOCK_MAX_READS)
         {
           /* check if we have read a 'Zero' value of 0xB or greater:
            */
           if ((bdp->ZeroLockReadData[0xB]) || (bdp->ZeroLockReadData[0xC]) ||
               (bdp->ZeroLockReadData[0xD]) || (bdp->ZeroLockReadData[0xE]) ||
               (bdp->ZeroLockReadData[0xF]))
           {
             // we have read a 'Zero' value of 0xB or greater, find the most popular 'Zero'
             // value and lock it:
             MostPopularZero = 0;
             // this loop finds the most popular 'Zero':
             for (ArrayPosition = 1; ArrayPosition < 16; ArrayPosition++)
             {
               if (bdp->ZeroLockReadData[ArrayPosition] > bdp->ZeroLockReadData[MostPopularZero])
                 MostPopularZero = ArrayPosition;
             }
             // now lock the most popular 'Zero': 
             EqRegValue = (ZLOCK_SET_ZERO | MostPopularZero);
             e100_MdiWrite (bdp, PHY_82555_MDI_EQUALIZER_CSR, bddp->phy_addr, EqRegValue);

#if (DEBUG_LINK > 0)
             printk ("ZeroLockState: Wrote most popular 'Zero' - %d. Going to sleep\n", MostPopularZero);
#endif
             // sleep for 5 minutes:
             bdp->ZeroLockSleepCounter = jiffies;
             bdp->ZeroLockState = ZLOCK_SLEEPING;
             // we will be reading the # of errors after 5 minutes, so we need to reset the
             // error counters - these registers are self clearing on read , so just read them:

             e100_MdiRead (bdp, PHY_82555_SYMBOL_ERR, bddp->phy_addr, &ErrorCounter);
           }
           else
           {
             // we did not read a 'Zero' value of 0xB or above. go back to the start:
             bdp->ZeroLockState = ZLOCK_INITIAL;
           }
         }
         break;

    case ZLOCK_SLEEPING:
         // state: sleeping for 5 minutes
         // prev states: ZLOCK_READING
         // next states: ZLOCK_READING, ZLOCK_SLEEPING

#if (DEBUG_LINK > 1)
         printk ("ZeroLockState: SLEEPING %d SEC \n", (jiffies - bdp->ZeroLockSleepCounter) / HZ);
#endif

         // if 5 minutes have passed:
         if ((jiffies - bdp->ZeroLockSleepCounter) >= ZLOCK_MAX_SLEEP)
         {
           // read and sum up the number of errors:
           e100_MdiRead (bdp, PHY_82555_SYMBOL_ERR, bddp->phy_addr, &ErrorCounter);

#if (DEBUG_LINK > 0)
           printk ("ZeroLockState: Sleep Time expired. Error counter %d\n", ErrorCounter);
           bdp->ErrorCounter = ErrorCounter;
#endif

           // if we have more than 300 errors (this number was calculated according
           //to the spec max allowed errors (80 errors per 1 million frames)
           //for 5 minutes in 100 Mbps (or the user specified max BER number)
           if (ErrorCounter > ber[bdp->bd_number])
           {
             // start again in the next callback:
             bdp->ZeroLockState = ZLOCK_INITIAL;
           }
           else
           {
             // we don't have more errors than allowed, sleep for another 5 minutes:
             bdp->ZeroLockSleepCounter = jiffies;
           }
         }
         break;
  }
}
