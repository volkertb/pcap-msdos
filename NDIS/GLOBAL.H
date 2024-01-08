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
// Global variables defined in FILTER.C and need to be visible.
extern AddrTableEntry addrTable[MAX_NUM_NETWORKS];
extern AccessListTableEntry *in;
extern AccessListTableEntry *out;
extern AccessListTableEntry *source;
extern AccessListTableEntry *udp;
extern RejectTableEntry rejectTable[MAX_NUM_REJECT_ENTRIES];
extern AllowTableEntry allowTable[MAX_NUM_ALLOW_ENTRIES];

extern AddrTableEntry newAddrTable[MAX_NUM_NEW_NETWORKS];
extern WORD newIn;
extern WORD newOut;
extern WORD newSource;
extern WORD newUdp;

extern int accessTableDirty;
extern int rejectTableDirty;
extern int allowTableDirty;

extern BYTE rebootRequested;

extern CardHandle *cardHandleLookup[MAX_NUM_CARDS];

extern BYTE arpProtocolId[6];
extern BYTE ipProtocolId[6];

extern WORD ipStackId;
extern WORD arpStackId;

extern Queue protocolQueue;

extern CardHandle *campus;
extern CardHandle *internet;
extern HashEntry *hashTable;

extern int listenMode;

extern WORD frameSize;
extern FilterConfig filterConfig;

extern int startDebug;
extern int stopDebug;
extern BYTE debugStrings[MAX_NUM_DEBUG_STRINGS][DEBUG_STRING_LENGTH];

extern int timesAllowed;

extern WORD headerSize;

extern int mediaType;

extern BYTE *networkTransferBuffer;

extern Statistics theStats;

extern CommonCharacteristics common;

extern DWORD startTime;
extern DWORD days;
