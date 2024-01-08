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
// From manage.c
WORD chkSum(BYTE *,WORD,WORD *);
void deliverPacket(Socket *, BYTE , void * , int);
void handleSync(SyncPacket * , int , Socket *);
void handleReboot(void *,int , Socket *);
void handleNewkey(BYTE *, int , Socket *);
void handleQuery(QueryPacket * , int , Socket *);
void handleLoad(LoadPacket * , int , Socket *);
void handleWrite(void *,int ,Socket *);
void handleRelease(ReleasePacket *, int , Socket * );
void handleStatistics(StatisticsPacket *,int ,Socket *);
void filtMessage(BYTE *, int , Socket * );
void initManage(void);

// From filter.c
BYTE networkCacheLookup(in_addr);
BYTE networkCacheFetch(in_addr,NetworkCacheEntry *);
void networkCacheFlush(void);
int checkIncomingTcp( in_addr , in_addr , WORD , WORD );
int checkOutgoingTcp( in_addr , in_addr , WORD , WORD );
int checkIncomingUdp( in_addr , in_addr , WORD , WORD );
int checkOutgoingUdp( in_addr , in_addr , WORD , WORD );
int checkIncomingPacket(WORD, BYTE *, int );
int checkOutgoingPacket(WORD, BYTE *, int );
void checkCard(CardHandle *, CardHandle *, CheckFunction ,WORD,DWORD *,DWORD *);
void checkCards(void);
void initMemory(void);
void initTables(void);
void initNetworks(void);

// From potp.c
void clientInit(BYTE *,Key *,SyncPacket *);
void serverInit(BYTE *,Key *,SyncPacket *);
void buildNewSessionKey(Key *,Key *,Key *);
void encrypt(BYTE *,BYTE *,Key *,WORD);
void decrypt(BYTE *,BYTE *,Key *,WORD);
void initFromPass(BYTE *,Key *);
void initFromTime(Key *);
DWORD randLong(Key *);
BYTE randByte(Key *k);
BYTE randBit(Key *);
BYTE lfsr(DWORD *,DWORD);
void initPotp(void);

// From ndis.c
WORD __pascal systemRequest(DWORD,DWORD,WORD,
						WORD,WORD);
WORD __pascal requestConfirm(WORD,WORD,WORD,
						WORD,WORD,WORD);
WORD __pascal transmitConfirm(WORD,WORD,
						WORD,WORD,WORD);
WORD __pascal receiveLookahead(WORD,WORD,
						WORD,BYTE *,
						BYTE *,WORD);
WORD __pascal indicationComplete(WORD,WORD);
WORD __pascal receiveChain(WORD,WORD,WORD,
						RxBufDescr *,BYTE *,WORD);
WORD __pascal status(WORD,WORD,BYTE *,
						WORD,WORD);
void far systemRequestGlue(void);
void far requestConfirmGlue(void);
void far transmitConfirmGlue(void);
void far receiveLookaheadGlue(void);
void far indicationCompleteGlue(void);
void far receiveChainGlue(void);
void far statusGlue(void);

void renumberPacketQueues(void);
void scanQueue(Queue *,DWORD *,DWORD *,DWORD);
void sendPacket(PktBuf *,int);
void printNdisError(WORD);
void openNdis(void);
void parseNdisConfig(void);
void registerNdisAndBind(void);
void checkMacFeatures(CardHandle *);
void startMac(CardHandle *);
void shutdownNdis(void);
void initNdis(CardHandle **, CardHandle **);

// From queue.c
PktBuf *pktBufFromHandle(int);
void enqueuePktBuf(Queue *, PktBuf *);
PktBuf *dequeuePktBuf(Queue *, PktBuf *);
void freePktBuf(PktBuf *);
PktBuf *allocPktBuf(void);
PktBuf *allocPktBufMgmt(void);
void initQueue(void);

// From main.c
void init(void);
void checkMisc(void);
void fastCopy(BYTE *dest,BYTE *src,int length);
ScheduledEvent *addScheduledEvent(DWORD,DWORD,EventCallBack);
void deleteScheduledEvent(ScheduledEvent *);
void usage(void);
int main(int , char *[]);

// From ip.c
BYTE *inet_ntoa(BYTE *,in_addr *);
in_addr *inet_aton(BYTE *,in_addr *);
ArpEntry *arpLookupIp(in_addr);
ArpEntry *arpLookupHardware(HardwareAddress *);
ArpEntry *arpAdd(void);
void arpCallBack(ScheduledEvent *);
void sendvRawMac(IoVec *,int ,HardwareAddress *,WORD ,int );
void sendvRaw(IoVec *,int,HardwareAddress *,WORD,...);
void sendArp(in_addr);
void sendvIp(in_addr *,BYTE ,IoVec *,int );
void sendvUdp(IoVec *,int ,Socket *);
void handleArp(ArpHeader *,int);
void handleIp(IpHeader *,int );
void handleIcmp(IcmpHeader *,int ,in_addr *);
void handleUdp(UdpHeader *,int ,in_addr *);
int checkLocal(WORD,HardwareAddress *,WORD,BYTE *);
Socket *socket(int,WORD,in_addr *,WORD,int,CallBack);
void initIp(void);
void checkIp(void);

// From bridge.c
int bridgeLookUp(HardwareAddress *);
int bridge(WORD,BYTE *);
void initBridge(void);

// From misc.asm
DWORD swapLong(DWORD);
in_addr swapAddr(in_addr);
WORD swapWord(WORD);
void swapLongPtr(DWORD *);
void swapWordPtr(WORD *);
void moveBytes(BYTE *,BYTE *,int);
void moveWords(BYTE *,BYTE *,int);
void moveLongs(BYTE *,BYTE *,int);
void xmsCall(XmsRegs *);

BYTE networkCacheLookup(in_addr);
BYTE networkCacheFetch(in_addr host,NetworkCacheEntry *);
void networkCacheFlush(void);

WORD xmsAllocMem(DWORD);
void xmsFreeMem(WORD);
void xmsCopy(WORD,DWORD,WORD,DWORD,DWORD);
WORD xmsQueryFree(void);
void initXms(void);

void printStats(void);
void clearStats(void);
void initStats(void);
void keyCheckCallBack(ScheduledEvent *);

// From sl.c
void initSyslog(void);
// in_addr long2Addr(DWORD);
void syslogMessage(DWORD,...);
