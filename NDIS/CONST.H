/*
 * Copyright (c) 1993,1994
 *	Texas A&M University.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Texas A&M University
 *	and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Developers:
 *             David K. Hess, Douglas Lee Schales, David R. Safford
 */
#define YES           1
#define NO            0

#define VERSION                "2.0"

#define INSIDE_MASK            0x1
#define OUTSIDE_MASK           0x2

// The maximum number of networks must always be a power of two due
//   to the hash function.
#define MAX_NUM_NETWORKS       32
#define NETWORK_HASH_MASK      0x00F80000UL

// The maximum number of ethernet address entries must also always
//   be a power of two.
#define MAX_NUM_ADDRESSES      8192
#define ADDRESS_HASH_MASK      0x1FFF

#define MAX_NUM_ACCESS_LISTS   256
#define MAX_NUM_ACCESS_RANGES  32
#define MAX_NUM_REJECT_ENTRIES 32
#define MAX_NUM_ALLOW_ENTRIES  8
#define MAX_NUM_NEW_NETWORKS   8

#define NETWORK_EXTENSION      "net"
#define ACCESS_LIST_FILE       "class.tbl"
#define REJECT_LIST_FILE       "reject.tbl"
#define ALLOW_LIST_FILE        "allow.tbl"
#define PASSWORD_FILE          "password"

// Our internal tags for protocols. Since it is typically two bytes (EthernetII and 802.2 SNAP) we
//   will use a short and take the Ethernet values as defaults. Everything else must be mapped to
//   these values as the packets are decoded.
#define FILTER_IP_PROTOCOL	0x0800
#define FILTER_ARP_PROTOCOL	0x0806
#define FILTER_RARP_PROTOCOL	0x8035

// Ethernet protocols. 
#define ETHERNET_IP_PROT        0x0800
#define ETHERNET_ARP_PROT       0x0806

// IP protocols.
#define ICMP_PROT      1
#define TCP_PROT       6
#define UDP_PROT       17

// IP flags. Note these are for little endian representations of the offset.
#define IP_DONT_FRAGMENT	0x0040
#define IP_MORE_FRAGMENTS	0x0020

// ARP codes.
#define ARP_REQUEST             1
#define ARP_REPLY               2
#define RARP_REQUEST            3
#define RARP_REPLY              4

// ARP table flag masks.
#define ARP_ENTRY_VALID		1
#define ARP_ENTRY_PENDING	2

// One hour timeout on arp entries.
#define ARP_ENTRY_TIMEOUT       (60 * 60)

#define ARP_TIMER_GRANULARITY	1

// ICMP codes.
#define ICMP_ECHO_REPLY		0
#define ICMP_REDIRECT		5
#define ICMP_ECHO_REQUEST	8

// Constants for TCP
#define TH_FIN  0x01
#define TH_SYN  0x02
#define TH_RST  0x04
#define TH_PUSH 0x08
#define TH_ACK  0x10
#define TH_URG  0x20

// Class macros.
#define IN_CLASSA(i)  (((DWORD)(i) & 0x80000000UL) == 0)
#define IN_CLASSB(i)  (((DWORD)(i) & 0xC0000000UL) == 0x80000000UL)
#define IN_CLASSC(i)  (((DWORD)(i) & 0xE0000000UL) == 0xC0000000UL)
#define IN_CLASSD(i)  (((DWORD)(i) & 0xF0000000UL) == 0xE0000000UL)

// Class Constants
#define CLASSB_HOST      0x0000FFFFUL
#define CLASSC_HOST      0x000000FFUL

#define CLASSB_NETWORK   0xFFFF0000UL
#define CLASSC_NETWORK   0xFFFFFF00UL

// This is a kludge. I need to look at the offset field in the ip header
//   but it is bit fielded with some flags. However since this stupid
//   architecture is byte reversed, I can't define a bit field structure
//   in C so that I can easily get to it. I always end up with two
//   pieces of it that I have to put back together. Luckily since
//   I don't need to know the value of the offset, just if it is
//   0 I can mask off the flag bits and do an int compare to 0.
#define IP_OFF_MASK      0xFF1F

// Message types.
#define FM_M_SYNC		0
#define FM_M_SYNCACK		1
#define FM_M_WRITE		2
#define FM_M_WRITEACK		3
#define FM_M_REBOOT		4
#define FM_M_REBOOTACK		5
#define FM_M_NEWKEY		6
#define FM_M_NEWKEYACK		7
#define FM_M_QUERY		8
#define FM_M_QUERYACK		9
#define FM_M_LOAD		10
#define FM_M_LOADACK		11
#define FM_M_ERROR		12
#define FM_M_RELEASE		13
#define FM_M_RELEASEACK		14
#define FM_M_STATISTICS		15
#define FM_M_STATISTICSACK	16

// Message flags.
#define FM_F_CRYPTED      0x01

// Miscellaneous constants used by the different messages.
#define FM_QUERY_NETWORK  0
#define FM_QUERY_HOST     1
#define FM_QUERY_REJECT   2
#define FM_QUERY_ALLOW    3
#define FM_QUERY_CLASS    4

#define FM_LOAD_NETWORK       0
#define FM_LOAD_REJECT        1
#define FM_LOAD_ALLOW         2
#define FM_LOAD_CLASS         3

#define FM_RELEASE_CLASSES    0
#define FM_RELEASE_REJECT     1
#define FM_RELEASE_ALLOW      2
#define FM_RELEASE_NETWORK    3

#define FM_LOAD_FLAGS_BEGIN  0x01
#define FM_LOAD_FLAGS_END    0x02

#define FM_STATISTICS_QUERY	0
#define FM_STATISTICS_CLEAR	1

#define FM_ERROR_INSECURE        0
#define FM_ERROR_SECURE          1
#define FM_ERROR_PASSFILE        2
#define FM_ERROR_PASSWRITE       3
#define FM_ERROR_NONETWORK       4
#define FM_ERROR_NOMEMORY        5
#define FM_ERROR_COMMAND         6
#define FM_ERROR_DATAFILE        7
#define FM_ERROR_DATAWRITE       8
#define FM_ERROR_INVALIDNETWORK  9
#define FM_ERROR_LOADBUFFER     10
#define FM_ERROR_LOSTSYNC       11

#define FM_STAT_DB_PACKETS_FILTERED_INSIDE      0
#define FM_STAT_DB_PACKETS_FILTERED_OUTSIDE     1
#define FM_STAT_DB_PACKETS_RX_INSIDE            2
#define FM_STAT_DB_PACKETS_RX_OUTSIDE           3
#define FM_STAT_DB_PACKETS_TX_INSIDE            4
#define FM_STAT_DB_PACKETS_TX_OUTSIDE           5
#define FM_STAT_DB_CACHE_ACCESSES               6
#define FM_STAT_DB_CACHE_MISSES                 7
#define FM_STAT_DB_DROPPED_PACKETS              8
#define FM_STAT_CARD_FRAMES_RX_INSIDE           9
#define FM_STAT_CARD_FRAMES_RX_OUTSIDE          10
#define FM_STAT_CARD_BYTES_RX_INSIDE            11
#define FM_STAT_CARD_BYTES_RX_OUTSIDE           12
#define FM_STAT_CARD_MULTICAST_RX_INSIDE        13
#define FM_STAT_CARD_MULTICAST_RX_OUTSIDE       14
#define FM_STAT_CARD_BROADCAST_RX_INSIDE        15
#define FM_STAT_CARD_BROADCAST_RX_OUTSIDE       16
#define FM_STAT_CARD_CRC_RX_INSIDE              17
#define FM_STAT_CARD_CRC_RX_OUTSIDE             18
#define FM_STAT_CARD_BUFFER_DROPS_RX_INSIDE     18
#define FM_STAT_CARD_BUFFER_DROPS_RX_OUTSIDE    19
#define FM_STAT_CARD_HARDWARE_DROPS_RX_INSIDE   20
#define FM_STAT_CARD_HARDWARE_DROPS_RX_OUTSIDE  21
#define FM_STAT_CARD_FRAMES_TX_INSIDE           22
#define FM_STAT_CARD_FRAMES_TX_OUTSIDE          23
#define FM_STAT_CARD_BYTES_TX_INSIDE            24
#define FM_STAT_CARD_BYTES_TX_OUTSIDE           25
#define FM_STAT_CARD_MULTICAST_TX_INSIDE        26
#define FM_STAT_CARD_MULTICAST_TX_OUTSIDE       27
#define FM_STAT_CARD_BROADCAST_TX_INSIDE        28
#define FM_STAT_CARD_BROADCAST_TX_OUTSIDE       29
#define FM_STAT_CARD_TIMEOUT_DROPS_TX_INSIDE    30
#define FM_STAT_CARD_TIMEOUT_DROPS_TX_OUTSIDE   31
#define FM_STAT_CARD_HARDWARE_DROPS_TX_INSIDE   32
#define FM_STAT_CARD_HARDWARE_DROPS_TX_OUTSIDE  33
#define FM_STAT_UPTIME  			34

// Syslog constants. 
#define SYSL_UNKNOWN			0 
#define SYSL_IN_CLASSD			1 
#define SYSL_OUT_CLASSD			2 
#define SYSL_IN_PORT			3 
#define SYSL_OUT_PORT			4 	// only TCP
#define SYSL_OUT_ALLOW			5 	// only TCP
#define SYSL_IN_LENGTH			6 	// TCP/UDP-Header maybe not initialized
#define SYSL_OUT_LENGTH			7 	// TCP/UDP-Header maybe not initialized
#define SYSL_IN_REJECT			8 	// only protocolNo, srcAddr and dstAddr
#define SYSL_IN_PROT			9 	// only protocolNo, srcAddr and dstAddr
#define SYSL_OUT_PROT			10 	// only protocolNo, srcAddr and dstAddr
#define SYSL_IN_FILTER			11 	// no packet information
#define SYSL_OUT_FILTER			12 	// no packet information
#define SYSL_COLD_START			13 	// cold start
#define SYSL_HEARTBEAT			14 	// heart beat message
#define SYSL_IN_OFFSET			15 	// suspect fragment offset
#define SYSL_OUT_OFFSET			16 	// suspect fragment offset

// Priorities.
#define SYSL_PRIORITY_EMERG		0
#define SYSL_PRIORITY_ALERT		1
#define SYSL_PRIORITY_CRIT		2
#define SYSL_PRIORITY_ERR		3
#define SYSL_PRIORITY_WARNING		4
#define SYSL_PRIORITY_NOTICE		5
#define SYSL_PRIORITY_INFO		6
#define SYSL_PRIORITY_DEBUG		7

#define MAX_SYSL_DATA_LEN	   256

#define SYSL_SRC_PORT		   6768
#define SYSL_DEST_PORT		   514

#define DEFAULT_SYSL_FACILITY      0

#define DEFAULT_SYSL_MASK	   0x1FFFFUL

#define MEDIA_FDDI		1
#define MEDIA_ETHERNET		2
#define MEDIA_TOKEN		3

#define MAX_NUM_CARDS           16

#define LOOKAHEAD_SIZE		30

// We need these for out protocol stack.
#define FRAME_TYPE_ETHERNET_II	2
#define FRAME_TYPE_TOKEN_SNAP	11
#define FRAME_TYPE_FDDI_SNAP	23

#define PASSWORD_LENGTH         12

// Default UDP port we will be listening on.
#define DEFAULT_PORT		6767

#define MAX_NUM_SOCKETS		5

// We don't handle a TCP socket. (Yet :-)
#define SOCKET_UDP		0
#define SOCKET_TCP		1

// Flags.
#define SOCKET_FIXED_DEST_PORT	0x01

#define ARP_TABLE_SIZE		16

#define MAX_NUM_EVENTS		10

#define MAX_NUM_MANAGERS	10

#define BUFFER_POOL_SIZE	20
#define BUFFER_POOL_OVERHEAD    5

#define MAX_NUM_DEBUG_STRINGS	90
#define DEBUG_STRING_LENGTH	80

#define MAX_NUM_EVENTS		10

// Note that this cannot be bigger than 66535
#define NETWORK_TRANSFER_BUFFER_SIZE	32768UL

// This constant is fixed unless you change the tag mask also.
#define NUM_NETWORK_CACHE_ENTRIES   1024
#define NETWORK_CACHE_TAG_MASK 0x000003FEUL

#define STACK_POOL_SIZE		6
#define STACK_SIZE		256

#define ARP_RETRIES		3

// Seconds
#define ARP_ENTRY_RETRY_TIMEOUT	2

#define MAX_NUM_STATISTICS	50
