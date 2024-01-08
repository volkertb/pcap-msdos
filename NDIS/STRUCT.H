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
// FILTER.C

typedef struct _Statistics {
	DWORD cacheAccesses;
	DWORD cacheMisses;
	DWORD droppedPackets;
	DWORD insideFiltered;
	DWORD outsideFiltered;
	DWORD insideRx;
	DWORD outsideRx;
	DWORD insideTx;
	DWORD outsideTx;
} Statistics;

typedef struct _IoVec {
        BYTE *buffer;
        WORD length;
}      IoVec;

typedef struct _PktBuf {
	struct _PktBuf *nextLink;
	struct _PktBuf *prevLink;
	int handle;
	int length;
	int packetLength;
	DWORD sequence;
	BYTE *buffer;
} PktBuf;

typedef union _HardwareAddress {
        BYTE bytes[6];
        WORD words[3];
        struct {
                BYTE bytes[6];
        }      addr;
}                HardwareAddress;

typedef struct _Queue {
        PktBuf *head;
        PktBuf *tail;
}      Queue;

typedef struct _CardHandle {
	BYTE moduleName[16];
        CommonCharacteristics *common;
        int maxSends;
        int sendsPending;
        Queue queue;
        Queue mgmtQueue;
}           CardHandle;

// Generic structure which can be mapped onto the source and destination
//   portion of all hardware frames.
typedef struct _GenericHeader {
        HardwareAddress destHost;
        HardwareAddress srcHost;
}              GenericHeader;

// Media level frame formats.

typedef struct _EthernetIIHeader {
        HardwareAddress etherDestHost;
        HardwareAddress etherSrcHost;
        WORD etherType;
}                 EthernetIIHeader;

typedef struct _Ieee802Dot3Header {
        HardwareAddress etherDestHost;
        HardwareAddress etherSrcHost;
        WORD length;
}                  Ieee802Dot3Header;

// Geez, this structure actually pads exactly as defined. Ugly.
typedef struct _FddiHeader {
	BYTE frameControl;
        HardwareAddress etherDestHost;
        HardwareAddress etherSrcHost;
}           FddiHeader;

// NOTE: Token ring is defined but not yet currently supported.
typedef struct _Ieee802Dot5Header {
        HardwareAddress etherDestHost;
        HardwareAddress etherSrcHost;
        BYTE routeInfo[30];
}                  Ieee802Dot5Header;

typedef struct _Ieee802Dot2SnapHeader {
        BYTE dsap;			// 0xAA
        BYTE ssap;			// 0xAA
        BYTE control;			// 3
        BYTE protocolId[5];
}                  Ieee802Dot2SnapHeader;

// Internet address structure.
typedef union _in_addr {
        struct {
                BYTE s_b1;
                BYTE s_b2;
                BYTE s_b3;
                BYTE s_b4;
        }      S_un_b;
        struct {
                WORD s_w1;
                WORD s_w2;
        }      S_un_w;
        DWORD S_addr;
}        in_addr;

typedef struct _IpHeader {
 BYTE ip_hl:
             4,                 /* header length */
      ip_v:
             4;                 /* version */
        BYTE ip_tos;   /* type of service */
        WORD ip_len;  /* total length */
        WORD ip_id;   /* identification */
        short ip_off;           /* fragment offset field */
        BYTE ip_ttl;   /* time to live */
        BYTE ip_p;     /* protocol */
        WORD ip_sum;  /* checksum */
        in_addr ip_src,
                ip_dst;         /* source and dest address */
}         IpHeader;

typedef struct _ArpEntry {
	BYTE *pendingPacket;
	int pendingPacketLength;
        HardwareAddress hardwareAddress;
        in_addr ipAddress;
        int timeToLive;
        int retries;
        int state;
}         ArpEntry;

typedef struct _ArpHeader {
        WORD hardType;/* Type of hardware address */
        WORD protType;/* Protocol we wish translation for */
        BYTE hardSize; /* Size of a hardware addres */
        BYTE protSize; /* Size of a protocol address */
        WORD op;      /* Operation requested */
        HardwareAddress sender; /* Hardware address of who is sending the request */
        in_addr senderIp;       /* Ditto but for IP */
        HardwareAddress target; /* Hardware address of who is targeted by the request */
        in_addr targetIp;       /* Ditto but for IP */
}          ArpHeader;

typedef struct _IcmpHeader {
        BYTE type;     /* ICMP type */
        BYTE code;     /* ICMP code */
        WORD chkSum;  /* ICMP check sum */
}           IcmpHeader;

typedef struct _IcmpEchoRequestHeader {
        WORD id;      /* ICMP Echo Request identifier */
        WORD seq;     /* ICMP Echo Request sequence identifier */
}                      IcmpEchoRequestHeader;

typedef DWORD tcp_seq;

typedef struct _TcpHeader {
        WORD th_sport;/* source port */
        WORD th_dport;/* destination port */
        tcp_seq th_seq;         /* sequence number */
        tcp_seq th_ack;         /* acknowledgement number */
 UINT th_x2:
            4,                  /* (unused) */
     th_off:
            4;                  /* data offset */
        BYTE th_flags;
        WORD th_win;  /* window */
        WORD th_sum;  /* checksum */
        WORD th_urp;  /* urgent pointer */
}          TcpHeader;

typedef struct _UdpHeader {
        WORD uh_sport;/* source port */
        WORD uh_dport;/* destination port */
        WORD uh_ulen; /* udp length */
        WORD uh_sum;  /* udp checksum */
}          UdpHeader;

//
// Structure for an address table entry.
//
typedef struct _AddrTableEntry {
        in_addr network;
        BYTE dirty;
        WORD hostTable;
}               AddrTableEntry;

typedef struct _AccessListTableEntry {
        WORD begin;
        WORD end;
}                     AccessListTableEntry;

typedef struct _RejectTableEntry {
        in_addr network;
        DWORD mask;
}                 RejectTableEntry;

// Note that it is assumed that this structure will always be 8 bytes long.
typedef struct _HashEntry {
        HardwareAddress address;
        BYTE boardNumber;
	BYTE valid;
}          HashEntry;

typedef struct _AllowTableEntry {
        in_addr network;
        DWORD mask;
        AccessListTableEntry access[MAX_NUM_ACCESS_RANGES];
}                AllowTableEntry;

//
// Structure representing a filter protocol header.
//
// The purpose for the chkSum and randomInject members is to prevent bit twiddling of a
//   management datagram when encryption is in use. The chkSum prevents anyone from twiddling
//   the bits given a known plaintext and the randomInject prevents anyone from fixing the
//   chkSum because the plaintext is no longer known. Of course a checksum is not too strong
//   since they could brute force it in at most 2^16 tries. This needs to be eventually changed
//   to a CRC calculation.
typedef struct _FilterHeader {
        BYTE type;
        BYTE flags;
        WORD chkSum;
        WORD randomInject;
}           FilterHeader;

typedef struct _Key {
        DWORD i1;
        DWORD i2;
        DWORD i3;
}    Key;

// MESSAGE.C

//
// Structure representing a filter protocol message.
//
typedef struct _SyncPacket {
        WORD spinCount;
        WORD dummy;
        Key key;
}           SyncPacket;

typedef struct _QueryPacket {
        BYTE type;
        BYTE dummy[3];
        union {
                BYTE index;
                in_addr addr;
        }     queryValue;
        union {
                in_addr networks[MAX_NUM_NETWORKS];
                BYTE index;
                RejectTableEntry reject[MAX_NUM_REJECT_ENTRIES];
                AllowTableEntry allow[MAX_NUM_ALLOW_ENTRIES];
                struct {
                        AccessListTableEntry in[MAX_NUM_ACCESS_RANGES];
                        AccessListTableEntry out[MAX_NUM_ACCESS_RANGES];
                        AccessListTableEntry src[MAX_NUM_ACCESS_RANGES];
                        AccessListTableEntry udp[MAX_NUM_ACCESS_RANGES];
                }      accessList;
        }     queryResult;
}            QueryPacket;

typedef struct _LoadPacket {
        BYTE type;
        BYTE flags;
        BYTE dummy[2];
        union {
                BYTE index;
                struct {
                        in_addr network;
                        DWORD offset;
                }      networkBlock;
        }     loadValue;
        union {
                RejectTableEntry reject[MAX_NUM_REJECT_ENTRIES];
                AllowTableEntry allow[MAX_NUM_ALLOW_ENTRIES];
                struct {
                        AccessListTableEntry in[MAX_NUM_ACCESS_RANGES];
                        AccessListTableEntry out[MAX_NUM_ACCESS_RANGES];
                        AccessListTableEntry src[MAX_NUM_ACCESS_RANGES];
                        AccessListTableEntry udp[MAX_NUM_ACCESS_RANGES];
                }      accessList;
                BYTE networkBlock[1024];
        }     loadData;
}           LoadPacket;

typedef struct _ReleasePacket {
        BYTE type;
        BYTE dummy[3];
        in_addr network;
}              ReleasePacket;

typedef struct _StatisticsPacket {
	BYTE type;
	BYTE dummy[3];
	DWORD statistics[MAX_NUM_STATISTICS];
} StatisticsPacket;

typedef struct _ErrorPacket {
        BYTE errorCode;
}            ErrorPacket;

// ODI.C

// POTP.C
// QUEUE.C
typedef void (* CallBack) (BYTE *, int, struct _Socket *);

typedef struct _Socket {
        int kind;
        int flags;
        WORD localPort;
        WORD remotePort;
        in_addr host;
        CallBack callBack;
}       Socket;

// Typedef for check functions.
typedef int (* CheckFunction) (WORD, BYTE *, int);

typedef struct _FilterConfig {
	int discardOther;
	int discardOtherIp;
	int discardSuspectOffset;
	int numBuffers;
	int listenMode;
	int numManagers;
	in_addr managers[MAX_NUM_MANAGERS];
	in_addr myIpAddr;
	in_addr mySubnetMask;
	in_addr myGateway;
	in_addr logHost;
	int logFacility;
	DWORD logMask;
	WORD listenPort;
	int internalMacSet;
	HardwareAddress internalMac;
} FilterConfig;

typedef void (* EventCallBack) (struct _ScheduledEvent *);

typedef struct _ScheduledEvent {
	DWORD time;    // Ticks.
	DWORD opaque;  // Client data.
	EventCallBack callBack;
	unsigned valid;
	unsigned new;
} ScheduledEvent;

typedef struct _NetworkCacheEntry {
	DWORD tag;
	BYTE indicies[2];   // The block.
	DWORD timestamp;
} NetworkCacheEntry;

typedef struct _SyslogMessageEntry {
	BYTE *message;
	BYTE priority;
	BYTE encodedPriority[6];
} SyslogMessageEntry;
