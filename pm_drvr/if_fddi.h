/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Global definitions for the ANSI FDDI interface.
 *
 * Version:	@(#)if_fddi.h	1.0.1	09/16/96
 *
 * Author:	Lawrence V. Stefani, <stefani@lkg.dec.com>
 *
 *		if_fddi.h is based on previous if_ether.h and if_tr.h work by
 *			Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *			Donald Becker, <becker@super.org>
 *			Alan Cox, <alan@cymru.net>
 *			Steve Whitehouse, <gw7rrm@eeshack3.swan.ac.uk>
 *			Peter De Schrijver, <stud11@cc4.kuleuven.ac.be>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */
#ifndef _LINUX_IF_FDDI_H
#define _LINUX_IF_FDDI_H

/*
 *  Define max and min legal sizes.  The frame sizes do not include
 *  4 byte FCS/CRC (frame check sequence).
 */
#define FDDI_K_ALEN             6               /* Octets in one FDDI address */
#define FDDI_K_8022_HLEN	16		/* Total octets in 802.2 header */
#define FDDI_K_SNAP_HLEN	21		/* Total octets in 802.2 SNAP header */
#define FDDI_K_8022_ZLEN	16		/* Min octets in 802.2 frame sans FCS */
#define FDDI_K_SNAP_ZLEN	21		/* Min octets in 802.2 SNAP frame sans FCS */
#define FDDI_K_8022_DLEN	4475	/* Max octets in 802.2 payload */
#define FDDI_K_SNAP_DLEN	4470	/* Max octets in 802.2 SNAP payload */
#define FDDI_K_LLC_ZLEN		13		/* Min octets in LLC frame sans FCS */
#define FDDI_K_LLC_LEN		4491	/* Max octets in LLC frame sans FCS */

/* Define FDDI Frame Control (FC) Byte values */
#define FDDI_FC_K_VOID					0x00	
#define FDDI_FC_K_NON_RESTRICTED_TOKEN	0x80	
#define FDDI_FC_K_RESTRICTED_TOKEN		0xC0	
#define FDDI_FC_K_SMT_MIN				0x41
#define FDDI_FC_K_SMT_MAX		   		0x4F
#define FDDI_FC_K_MAC_MIN				0xC1
#define FDDI_FC_K_MAC_MAX		  		0xCF	
#define FDDI_FC_K_ASYNC_LLC_MIN			0x50
#define FDDI_FC_K_ASYNC_LLC_DEF			0x54
#define FDDI_FC_K_ASYNC_LLC_MAX			0x5F
#define FDDI_FC_K_SYNC_LLC_MIN			0xD0
#define FDDI_FC_K_SYNC_LLC_MAX			0xD7
#define FDDI_FC_K_IMPLEMENTOR_MIN		0x60
#define FDDI_FC_K_IMPLEMENTOR_MAX  		0x6F
#define FDDI_FC_K_RESERVED_MIN			0x70
#define FDDI_FC_K_RESERVED_MAX			0x7F

/* Define LLC and SNAP constants */
#define FDDI_EXTENDED_SAP	0xAA
#define FDDI_UI_CMD			0x03

/* Define 802.2 Type 1 header */
struct fddi_8022_1_hdr
	{
        BYTE    dsap;                                   /* destination service access point */
        BYTE    ssap;                                   /* source service access point */
        BYTE    ctrl;                                   /* control byte #1 */
	} __attribute__ ((packed));

/* Define 802.2 Type 2 header */
struct fddi_8022_2_hdr
	{
        BYTE    dsap;                                   /* destination service access point */
        BYTE    ssap;                                   /* source service access point */
        BYTE    ctrl_1;                                 /* control byte #1 */
        BYTE    ctrl_2;                                 /* control byte #2 */
	} __attribute__ ((packed));

/* Define 802.2 SNAP header */
#define FDDI_K_OUI_LEN	3
struct fddi_snap_hdr
	{
        BYTE    dsap;                                   /* always 0xAA */
        BYTE    ssap;                                   /* always 0xAA */
        BYTE    ctrl;                                   /* always 0x03 */
        BYTE    oui[FDDI_K_OUI_LEN];    /* organizational universal id */
        WORD   ethertype;                              /* packet type ID field */
	} __attribute__ ((packed));

/* Define FDDI LLC frame header */
struct fddihdr
	{
        BYTE    fc;                                             /* frame control */
        BYTE    daddr[FDDI_K_ALEN];             /* destination address */
        BYTE    saddr[FDDI_K_ALEN];             /* source address */
	union
		{
		struct fddi_8022_1_hdr		llc_8022_1;
		struct fddi_8022_2_hdr		llc_8022_2;
		struct fddi_snap_hdr		llc_snap;
		} hdr;
	} __attribute__ ((packed));

/* Define FDDI statistics structure */
struct fddi_statistics
	{
        DWORD   rx_packets;                             /* total packets received */
        DWORD   tx_packets;                             /* total packets transmitted */
        DWORD   rx_errors;                              /* bad packets received */
        DWORD   tx_errors;                              /* packet transmit problems     */
        DWORD   rx_dropped;                             /* no space in linux buffers */
        DWORD   tx_dropped;                             /* no space available in linux */
        DWORD   multicast;                              /* multicast packets received */
        DWORD   transmit_collision;             /* always 0 for FDDI */

	/* Detailed FDDI statistics.  Adopted from RFC 1512 */

        BYTE    smt_station_id[8];
        DWORD   smt_op_version_id;
        DWORD   smt_hi_version_id;
        DWORD   smt_lo_version_id;
        BYTE    smt_user_data[32];
        DWORD   smt_mib_version_id;
        DWORD   smt_mac_cts;
        DWORD   smt_non_master_cts;
        DWORD   smt_master_cts;
        DWORD   smt_available_paths;
        DWORD   smt_config_capabilities;
        DWORD   smt_config_policy;
        DWORD   smt_connection_policy;
        DWORD   smt_t_notify;
        DWORD   smt_stat_rpt_policy;
        DWORD   smt_trace_max_expiration;
        DWORD   smt_bypass_present;
        DWORD   smt_ecm_state;
        DWORD   smt_cf_state;
        DWORD   smt_remote_disconnect_flag;
        DWORD   smt_station_status;
        DWORD   smt_peer_wrap_flag;
        DWORD   smt_time_stamp;
        DWORD   smt_transition_time_stamp;
        DWORD   mac_frame_status_functions;
        DWORD   mac_t_max_capability;
        DWORD   mac_tvx_capability;
        DWORD   mac_available_paths;
        DWORD   mac_current_path;
        BYTE    mac_upstream_nbr[FDDI_K_ALEN];
        BYTE    mac_downstream_nbr[FDDI_K_ALEN];
        BYTE    mac_old_upstream_nbr[FDDI_K_ALEN];
        BYTE    mac_old_downstream_nbr[FDDI_K_ALEN];
        DWORD   mac_dup_address_test;
        DWORD   mac_requested_paths;
        DWORD   mac_downstream_port_type;
        BYTE    mac_smt_address[FDDI_K_ALEN];
        DWORD   mac_t_req;
        DWORD   mac_t_neg;
        DWORD   mac_t_max;
        DWORD   mac_tvx_value;
        DWORD   mac_frame_cts;
        DWORD   mac_copied_cts;
        DWORD   mac_transmit_cts;
        DWORD   mac_error_cts;
        DWORD   mac_lost_cts;
        DWORD   mac_frame_error_threshold;
        DWORD   mac_frame_error_ratio;
        DWORD   mac_rmt_state;
        DWORD   mac_da_flag;
        DWORD   mac_una_da_flag;
        DWORD   mac_frame_error_flag;
        DWORD   mac_ma_unitdata_available;
        DWORD   mac_hardware_present;
        DWORD   mac_ma_unitdata_enable;
        DWORD   path_tvx_lower_bound;
        DWORD   path_t_max_lower_bound;
        DWORD   path_max_t_req;
        DWORD   path_configuration[8];
        DWORD   port_my_type[2];
        DWORD   port_neighbor_type[2];
        DWORD   port_connection_policies[2];
        DWORD   port_mac_indicated[2];
        DWORD   port_current_path[2];
        BYTE    port_requested_paths[3*2];
        DWORD   port_mac_placement[2];
        DWORD   port_available_paths[2];
        DWORD   port_pmd_class[2];
        DWORD   port_connection_capabilities[2];
        DWORD   port_bs_flag[2];
        DWORD   port_lct_fail_cts[2];
        DWORD   port_ler_estimate[2];
        DWORD   port_lem_reject_cts[2];
        DWORD   port_lem_cts[2];
        DWORD   port_ler_cutoff[2];
        DWORD   port_ler_alarm[2];
        DWORD   port_connect_state[2];
        DWORD   port_pcm_state[2];
        DWORD   port_pc_withhold[2];
        DWORD   port_ler_flag[2];
        DWORD   port_hardware_present[2];
	};

#endif	/* _LINUX_IF_FDDI_H */
