/* 
 * Copyright (c) 1993,1994
 *      Texas A&M University.  All rights reserved.
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
 *      This product includes software developed by Texas A&M University
 *      and its contributors.
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

// Heart of the filter program.
#include "db.h"

// Filter data structures.
AddrTableEntry        addrTable   [MAX_NUM_NETWORKS];
RejectTableEntry      rejectTable [MAX_NUM_REJECT_ENTRIES];
AllowTableEntry       allowTable  [MAX_NUM_ALLOW_ENTRIES];

AccessListTableEntry *in     = (AccessListTableEntry *) NULL;
AccessListTableEntry *out    = (AccessListTableEntry *) NULL;
AccessListTableEntry *source = (AccessListTableEntry *) NULL;
AccessListTableEntry *udp    = (AccessListTableEntry *) NULL;


// Temp data structures for things that are loaded in more
//   than one packet.
AddrTableEntry newAddrTable[MAX_NUM_NEW_NETWORKS];
WORD newIn     = 0;
WORD newOut    = 0;
WORD newSource = 0;
WORD newUdp    = 0;

// Boolean variables to tell if the data structures are dirty
//   and need to be written to disk.
int accessTableDirty = NO;
int rejectTableDirty = NO;
int allowTableDirty  = NO;

static NetworkCacheEntry *networkCache = NULL;

BYTE *networkTransferBuffer = NULL;

BYTE networkCacheLookup (in_addr host)
{
  WORD tagIndex;
  NetworkCacheEntry *entry;
  NetworkCacheEntry *replace;
  DWORD tag;
  BYTE result;

  ++theStats.cacheAccesses;

  // Note that since we are 2-way set associative, we shift up by 1. But the
  //   necessary shift down by one to get the tag first cancels it.
  tagIndex = host.S_addr & NETWORK_CACHE_TAG_MASK;
  tag = host.S_addr & 0xFFFFFFFEUL;

  entry = networkCache + tagIndex;

  // Do a two-way associative lookup.
  if (entry->tag == tag)
  {
    entry->timestamp = *(DWORD *) MK_FP (0x0040, 0x006C);
    return entry->indicies[host.S_addr & 0x01];
  }

  if ((entry + 1)->tag == tag)
  {
    (entry + 1)->timestamp = *(DWORD *) MK_FP (0x0040, 0x006C);
    return (entry + 1)->indicies[host.S_addr & 0x01];
  }

  // Choose a block to replace based on the timestamp.
  if (entry->timestamp > (entry + 1)->timestamp)
    ++entry;

  return networkCacheFetch (host, entry);
}

BYTE networkCacheFetch (in_addr host, NetworkCacheEntry * entry)
{
  DWORD offset;
  WORD hash;
  WORD curr;
  in_addr hostPart;
  in_addr networkPart;

  //fprintf(stderr,"cache miss: looking up %08lX\n",host.S_addr);

  // Get the network and host from the address.
  if (IN_CLASSB (host.S_addr))
  {

    // Class B address.
    networkPart.S_addr = host.S_addr & CLASSB_NETWORK;
    hostPart.S_addr = host.S_addr & CLASSB_HOST;
  }
  else if (IN_CLASSC (host.S_addr))
  {

    // Class C address.
    networkPart.S_addr = host.S_addr & CLASSC_NETWORK;
    hostPart.S_addr = host.S_addr & CLASSC_HOST;
  }
  else
  {
    //fprintf(stderr,"class A address in cache fetch\n");

    // We don't handle class A's yet. They get the default index.
    return 0;
  }

  // Hash into the table and see if the network has been defined.
  hash = (WORD) ((networkPart.S_addr & NETWORK_HASH_MASK) >> 19);
  curr = hash;

  while (addrTable[curr].network.S_addr != networkPart.S_addr)
  {
    if (addrTable[curr].network.S_addr == 0)
    {
      curr = 0xFFFF;
      break;
    }

    curr = (curr + 1) % MAX_NUM_NETWORKS;

    // Check if we have wrapped around.
    if (curr == hash)
    {
      curr = 0xFFFF;
      break;
    }
  }

  // If not defined then default to access list 0.
  if (curr == 0xFFFF)
  {
    //fprintf(stderr,"network not loaded in cache lookup\n");
    return 0;
  }

  ++theStats.cacheMisses;

  // Get the offset for the block in the network table.
  offset = hostPart.S_addr & 0xFFFFFFFEUL;

  //fprintf(stderr,"fetching the block\n");

  // Transfer the "block" down.
  xmsCopy (0, (DWORD) entry->indicies, addrTable[curr].hostTable, offset, 1);

  // Set the timestamp from the system timer.
  entry->timestamp = *(DWORD *) MK_FP (0x0040, 0x006C);
  entry->tag = host.S_addr & 0xFFFFFFFEUL;

  // Return back the index from here to avoid looking up again.
  return entry->indicies[host.S_addr & 0x01];
}

void networkCacheFlush (void)
{
  //fprintf(stderr,"flushing the network cache\n");

  memset ((void *) networkCache, 0, sizeof (NetworkCacheEntry) * NUM_NETWORK_CACHE_ENTRIES);
}

int checkIncomingTcp (in_addr srcAddr, in_addr dstAddr,
		      WORD srcPort, WORD dstPort)
{
  int i;
  int result;
  int curr;
  int accessIndex;
  DWORD hash;
  DWORD host;
  in_addr network;
  AccessListTableEntry *accessList;

  // fprintf(stdout,"incoming SYN\n");

  // Pass all IP multicast traffic.
  if (IN_CLASSD (dstAddr.S_addr))
#ifdef DENY_MULTICAST
  {
    syslogMessage (SYSL_IN_CLASSD, TCP_PROT, srcAddr, dstAddr, srcPort, dstPort);
    return NO;
  }
#else
    return YES;
#endif

  result = NO;

  // fprintf(stdout,"%08lX ",srcAddr.S_addr);
  // fprintf(stdout,"%08lX\n",dstAddr.S_addr);

  // Do the lookup to get the index.
  accessIndex = networkCacheLookup (dstAddr);

  accessList = in + accessIndex * MAX_NUM_ACCESS_RANGES;

  // See if the destination port is allowed. Search the in access list.
  i = 0;
  while (dstPort > accessList[i].end)
  {

    if (accessList[i].begin == 0)
    {
      i = -1;
      break;
    }
    ++i;
  }

  if (i != -1 && dstPort >= accessList[i].begin)
  {
    // fprintf(stdout,"permission allowed\n");
    result = YES;
  }
  else if (dstPort > 900)
  {

    // fprintf(stdout,"checking source\n");

    // If the destination port is not allowed then check the
    // source
    // port.
    accessList = source + accessIndex * MAX_NUM_ACCESS_RANGES;

    i = 0;
    while (srcPort > accessList[i].end)
    {
      if (accessList[i].begin == 0)
      {
	i = -1;
	break;
      }
      ++i;
    }

    if (i != -1 && srcPort >= accessList[i].begin)
    {
      // fprintf(stdout,"allowed\n");
      result = YES;
    }
  }

  if (result == NO)
    syslogMessage (SYSL_IN_PORT, TCP_PROT, srcAddr, dstAddr, srcPort, dstPort);

  // fprintf(stdout,">");
  // fflush(stdout);
  // fprintf(stdout,"result = %d\n",result);

  return result;
}

int checkOutgoingTcp (in_addr srcAddr, in_addr dstAddr,
		      WORD srcPort, WORD dstPort)
{
  int i;
  int j;
  int result;
  int curr;
  int accessIndex;
  DWORD host;
  DWORD hash;
  in_addr network;
  AccessListTableEntry *accessList;

  // Pass all IP multicast traffic.
  if (IN_CLASSD (dstAddr.S_addr))
#ifdef DENY_MULTICAST
  {
    syslogMessage (SYSL_OUT_CLASSD, TCP_PROT, srcAddr, dstAddr, srcPort, dstPort);
    return NO;
  }
#else
    return YES;
#endif

  // printf("Checking Outgoing\n");
  result = NO;

  // printf("%08lX ", srcAddr.S_addr);

  // fprintf(stdout,"dstPort = %d\n",dstPort);

  accessIndex = networkCacheLookup (srcAddr);

  // See if the destination port is allowed. Search the in access list.
  accessList = out + accessIndex * MAX_NUM_ACCESS_RANGES;
  i = 0;
  while (dstPort > accessList[i].end)
  {
    // fprintf(stdout,"end = %d\n",accessList[i].end);
    if (accessList[i].begin == 0)
    {
      i = -1;
      break;
    }
    ++i;
  }

  if (i != -1 && dstPort >= accessList[i].begin)
  {
    // fprintf(stdout,"Attempt is permitted\n");
    result = YES;
  }
  else
  {
    // fprintf(stdout,"Attempt is not permitted\n");
  }

  // Now if still not allowed check the allow list.
  if (result == NO)
  {
    // fprintf(stdout,"Checking allow\n");

    // Check the outgoing packet to see if the destination is on the
    // allow list.
    i = 0;

    while (allowTable[i].network.S_addr != 0 &&
	   i < MAX_NUM_ALLOW_ENTRIES)
    {

      if ((allowTable[i].network.S_addr & allowTable[i].mask) ==
	  (dstAddr.S_addr & allowTable[i].mask))
	break;

      ++i;
    }

    // fprintf(stdout,"finished while i = %d\n",i);

    if (i < MAX_NUM_ALLOW_ENTRIES && allowTable[i].network.S_addr != 0)
    {
      // fprintf(stdout,"going in if\n");

      // Now check to see if the destination port is allowed
      // in the access list.
      j = 0;

      while (dstPort > allowTable[i].access[j].end)
      {

	// fprintf(stdout,"end = %d\n",allowTable[i].access[j].end);

	if (allowTable[i].access[j].begin == 0)
	{
	  j = -1;
	  break;
	}
	++j;
      }

      if (j != -1 && dstPort >= allowTable[i].access[j].begin)
      {
	// fprintf(stdout,"Attempt permitted via allow\n");
	result = YES;
      }
      else
      {
	// fprintf(stdout,"Attempt not permitted via allow\n");
	syslogMessage (SYSL_OUT_ALLOW, TCP_PROT, srcAddr, dstAddr, srcPort, dstPort);
      }
    }
    else
    {
      syslogMessage (SYSL_OUT_PORT, TCP_PROT, srcAddr, dstAddr, srcPort, dstPort);
    }
  }
  // fprintf(stdout,"<");
  // fflush(stdout);
  // fprintf(stdout,"result = %d\n",result);

  return result;
}

int checkIncomingUdp (in_addr srcAddr, in_addr dstAddr,
		      WORD srcPort, WORD dstPort)
{
  int i;
  int result;
  int curr;
  int accessIndex;
  DWORD hash;
  DWORD host;
  in_addr network;
  AccessListTableEntry *accessList;

  // Pass all IP multicast traffic.
  if (IN_CLASSD (dstAddr.S_addr))
#ifdef DENY_MULTICAST
  {
    syslogMessage (SYSL_IN_CLASSD, UDP_PROT, srcAddr, dstAddr, srcPort, dstPort);
    return NO;
  }
#else
    return YES;
#endif

  result = NO;

  //fprintf(stderr,"src udp in = %d dest udp in = %d\n",srcPort,dstPort);

  accessIndex = networkCacheLookup (dstAddr);

  accessList = udp + accessIndex * MAX_NUM_ACCESS_RANGES;

  // See if the destination port is allowed. Search the in access
  // list.
  i = 0;
  while (dstPort > accessList[i].end)
  {

    if (accessList[i].begin == 0)
    {
      i = -1;
      break;
    }
    ++i;
  }

  if (i != -1 && dstPort >= accessList[i].begin)
  {
    // fprintf(stdout,"permission allowed\n");
    result = YES;
  }

  if (result == NO)
    syslogMessage (SYSL_IN_PORT, UDP_PROT, srcAddr, dstAddr, srcPort, dstPort);

  return result;
}

int checkOutgoingUdp (in_addr srcAddr, in_addr dstAddr,
		      WORD srcPort, WORD dstPort)
{
  int result;

  if (IN_CLASSD (dstAddr.S_addr))
#ifdef DENY_MULTICAST
  {
    syslogMessage (SYSL_OUT_CLASSD, UDP_PROT, srcAddr, dstAddr, srcPort, dstPort);
    return NO;
  }
#else
    return YES;
#endif

  // Allow all UDP out for now.
  result = YES;

  return result;
}

// Note that protocol here is not necessarily the protocol as found on the wire. It is a tag that we use
//   internally. (It happens to be by default the Ethernet II protocol. Others are translated to Ethernet II).
int checkIncomingPacket (WORD protocol, BYTE * packet, int length)
{
  int i;
  int result;
  IpHeader *ipHeader;
  TcpHeader *tcpHeader;
  UdpHeader *udpHeader;
  in_addr srcAddr;
  UINT ip_len;

  //fprintf(stdout,"check in\n");

  result = YES;

  switch (protocol)
     {
       case FILTER_IP_PROTOCOL:
	 ipHeader = (IpHeader *) packet;


/* 
 * for (i = 0;i < 20;++i) { fprintf(stdout," %02X",packet[i]); } fprintf(stdout,"\n");
 * 
 * fprintf(stdout,"size = %d\n",sizeof(IpHeader)); fprintf(stdout,"off = %d\n",ipHeader->ip_off); fprintf(stdout,"hl =
 * %d\n",ipHeader->ip_hl); fprintf(stdout,"version = %d\n",ipHeader->ip_v); fprintf(stdout,"proto = %d\n",(int)
 * ipHeader->ip_p); fprintf(stdout,"ttl = %d\n",(int) ipHeader->ip_ttl); fprintf(stdout,"len = %04X\n",(int)
 * ipHeader->ip_len); */

	 // Pass all IP fragments that do not have offset 0 (beginning
	 // of the packet) without checking since the TCP/UDP
	 // headers are not in this packet.
	 if ((ipHeader->ip_off & IP_OFF_MASK) != 0)
	 {
	   // fprintf(stdout,"offset fragment\n");

	   if (filterConfig.discardSuspectOffset &&
	       swapWord (ipHeader->ip_off & IP_OFF_MASK) == 1 &&
	       ipHeader->ip_p == TCP_PROT)
	   {

	     syslogMessage (SYSL_IN_OFFSET, TCP_PROT,
			    swapAddr (ipHeader->ip_src),
			    swapAddr (ipHeader->ip_dst));

	     result = NO;

	     break;
	   }

	   return YES;
	 }

	 srcAddr.S_addr = swapLong (ipHeader->ip_src.S_addr);

	 // Check the incoming packet to see if the src is on the reject list.
	 i = 0;

	 while (rejectTable[i].network.S_addr != 0 &&
		i < MAX_NUM_REJECT_ENTRIES)
	 {

	   if ((rejectTable[i].network.S_addr & rejectTable[i].mask) ==
	       (srcAddr.S_addr & rejectTable[i].mask))
	     break;

	   ++i;
	 }

	 if (i < MAX_NUM_REJECT_ENTRIES && rejectTable[i].network.S_addr != 0)
	 {
	   syslogMessage (SYSL_IN_REJECT, ipHeader->ip_p,
			  swapAddr (ipHeader->ip_src), swapAddr (ipHeader->ip_dst));

	   result = NO;

	   break;
	 }
	 switch (ipHeader->ip_p)
	    {
	      case TCP_PROT:
		// Make sure this packet (fragment) includes enough of the TCP header 
		//   before making the other checks. Otherwise a SYN can sneak through 
		//   since we might be trying to test something which is actually in 
		//   the next fragment.
		// NOTE: we drop all of these since we do not keep any state between
		//       fragments.
		//
		// Many thanks to Uwe Ellermann at DFN-CERT for reporting this problem.
		//
		ip_len = swapWord (ipHeader->ip_len);
		if ((ip_len > length) || (ip_len - (ipHeader->ip_hl << 2) < 14))
		{
		  syslogMessage (SYSL_IN_LENGTH, TCP_PROT,
				 swapAddr (ipHeader->ip_src), swapAddr (ipHeader->ip_dst),
				 swapWord (tcpHeader->th_sport), swapWord (tcpHeader->th_dport));

		  result = NO;

		  break;
		}

		tcpHeader = (TcpHeader *) (((BYTE *) ipHeader) +
					   (ipHeader->ip_hl << 2));

		// fprintf(stdout,"ip = %08lX tcp = %08lX ",ipHeader,tcpHeader);

		// for (i = 0;i < 20;++i) {
		// fprintf(stdout,"%02X ",((BYTE *) tcpHeader)[i]);
		// }
		// fprintf(stdout,"\n");

		// Check for "ACKless SYN".
		if ((tcpHeader->th_flags & (TH_SYN | TH_ACK)) == TH_SYN)
		  result = checkIncomingTcp (swapAddr (ipHeader->ip_src),
					     swapAddr (ipHeader->ip_dst),
					     swapWord (tcpHeader->th_sport),
					     swapWord (tcpHeader->th_dport));

		break;
	      case UDP_PROT:
		// Make sure this packet (fragment) includes enough of the UDP header 
		//   before making the other checks.
		ip_len = swapWord (ipHeader->ip_len);
		if ((ip_len > length) || (ip_len - (ipHeader->ip_hl << 2) < 4))
		{
		  syslogMessage (SYSL_IN_LENGTH, UDP_PROT,
				 swapAddr (ipHeader->ip_src), swapAddr (ipHeader->ip_dst),
				 swapWord (udpHeader->uh_sport), swapWord (udpHeader->uh_dport));

		  result = NO;

		  break;
		}

		// Check UDP protocols.
		udpHeader = (UdpHeader *) (((BYTE *) ipHeader) +
					   (ipHeader->ip_hl << 2));

		result = checkIncomingUdp (swapAddr (ipHeader->ip_src),
					   swapAddr (ipHeader->ip_dst),
					   swapWord (udpHeader->uh_sport),
					   swapWord (udpHeader->uh_dport));

		break;
	      case ICMP_PROT:
		// Always pass ICMP.
		break;
	      default:
		if (filterConfig.discardOtherIp)
		{

		  syslogMessage (SYSL_IN_PROT, ipHeader->ip_p,
				 swapAddr (ipHeader->ip_src),
				 swapAddr (ipHeader->ip_dst));

		  result = NO;

		  break;
		}

		// Everything else is allowed so far.
		break;
	    }

	 break;

       case FILTER_ARP_PROTOCOL:
       case FILTER_RARP_PROTOCOL:
	 // Pass these guys through no matter what.
	 break;

       default:
	 // Only pass the rest if told to do so.
	 result = !(filterConfig.discardOther);

	 // Boy will this get ugly if the syslog mask is not configured properly.....
	 if (result == NO)
	 {
	   syslogMessage (SYSL_IN_FILTER, protocol);
	 }

	 break;
     }

  //fprintf(stderr,"result = %d\n",result);

  theStats.outsideFiltered += (result == NO ? 1 : 0);

  return result;
}

int checkOutgoingPacket (WORD protocol, BYTE * packet, int length)
{
  int result;
  IpHeader *ipHeader;
  TcpHeader *tcpHeader;
  UdpHeader *udpHeader;
  UINT ip_len;

  //fprintf(stdout,"check out\n");

  // exit(0);

  result = YES;

  switch (protocol)
     {
       case FILTER_IP_PROTOCOL:
	 ipHeader = (IpHeader *) packet;

/* 
 * fprintf(stdout,"sizeof ipheader = %d\n",sizeof(IpHeader));
 * 
 * for (i = 0;i < 32;++i) { fprintf(stdout," %02X",packet[i]); } fprintf(stdout,"\n");
 * 
 * fprintf(stdout,"offset (going out) = %d\n",ipHeader->ip_off);
 * 
 * fprintf(stdout,"IP - 0x%08lX 0x%08lX ", ipHeader->ip_src.S_addr, ipHeader->ip_dst.S_addr); */

	 // Pass all IP fragments that do not have offset 0 (beginning
	 // of the packet) without checking since the TCP/UDP
	 // headers are not in this packet. An area for improvement
	 // would be to cache session info so we could drop all
	 // disallowed fragments also instead of just the
	 // first one.
	 if ((ipHeader->ip_off & IP_OFF_MASK) != 0)
	 {

	   if (filterConfig.discardSuspectOffset &&
	       swapWord (ipHeader->ip_off & IP_OFF_MASK) == 1 &&
	       ipHeader->ip_p == TCP_PROT)
	   {

	     syslogMessage (SYSL_OUT_OFFSET, TCP_PROT,
			    swapAddr (ipHeader->ip_src),
			    swapAddr (ipHeader->ip_dst));

	     result = NO;

	     break;
	   }

	   return YES;
	 }

	 // for (i = 0;i < 34;++i) {
	 // fprintf(stdout,"%02X ",packet[i]);
	 // }
	 // fprintf(stdout,"\n");

	 switch (ipHeader->ip_p)
	    {
	      case TCP_PROT:

		// Make sure this packet (fragment) includes enough of the TCP header 
		//   before making the other checks.
		ip_len = swapWord (ipHeader->ip_len);
		if ((ip_len > length) || (ip_len - (ipHeader->ip_hl << 2) < 14))
		{
		  syslogMessage (SYSL_OUT_LENGTH, TCP_PROT,
				 swapAddr (ipHeader->ip_src), swapAddr (ipHeader->ip_dst),
				 swapWord (tcpHeader->th_sport), swapWord (tcpHeader->th_dport));

		  result = NO;

		  break;
		}

		// fprintf(stdout,"size = %d\n",sizeof(TcpHeader));
		// fprintf(stdout,"TCP\n");
		// fprintf(stdout,"hl = %d\n",ipHeader->ip_hl);

		tcpHeader = (TcpHeader *) (((BYTE *) ipHeader) +
					   (ipHeader->ip_hl << 2));

		// fprintf(stdout," flags = %02X",tcpHeader->th_flags);

		if ((tcpHeader->th_flags & (TH_SYN | TH_ACK)) == TH_SYN)
		{
		  // fprintf(stdout,"Outgoing SYN\n");

		  result = checkOutgoingTcp (swapAddr (ipHeader->ip_src),
					     swapAddr (ipHeader->ip_dst),
					     swapWord (tcpHeader->th_sport),
					     swapWord (tcpHeader->th_dport));
		}
		break;
	      case UDP_PROT:
		// Make sure this packet (fragment) includes enough of the UDP header 
		//   before making the other checks.
		ip_len = swapWord (ipHeader->ip_len);
		if ((ip_len > length) || (ip_len - (ipHeader->ip_hl << 2) < 4))
		{
		  syslogMessage (SYSL_OUT_LENGTH, UDP_PROT,
				 swapAddr (ipHeader->ip_src), swapAddr (ipHeader->ip_dst),
				 swapWord (udpHeader->uh_sport), swapWord (udpHeader->uh_dport));

		  result = NO;

		  break;
		}

		// fprintf(stdout," UDP -");
		// Check UDP packets also.
		udpHeader = (UdpHeader *) (((BYTE *) ipHeader) +
					   (ipHeader->ip_hl << 2));

		result = checkOutgoingUdp (swapAddr (ipHeader->ip_src),
					   swapAddr (ipHeader->ip_dst),
					   swapWord (udpHeader->uh_sport),
					   swapWord (udpHeader->uh_dport));

		break;
	      case ICMP_PROT:
		// always pass ICMP.
		break;
	      default:
		if (filterConfig.discardOtherIp)
		{

		  syslogMessage (SYSL_OUT_PROT, ipHeader->ip_p,
				 swapAddr (ipHeader->ip_src),
				 swapAddr (ipHeader->ip_dst));

		  result = NO;
		}

		// Everything else is allowed so far.
		break;
	    }

	 break;

       case FILTER_ARP_PROTOCOL:
       case FILTER_RARP_PROTOCOL:
	 // Pass these guys through no matter what.
	 break;

       default:
	 // Only pass the rest if told to do so.
	 result = !(filterConfig.discardOther);

	 if (result == NO)
	   syslogMessage (SYSL_OUT_FILTER, protocol);

	 break;
     }

  // fprintf(stdout,"\n");

  //fprintf(stdout,"result = %d\n",result);

  theStats.insideFiltered += (result == NO ? 1 : 0);

  return result;
}

void checkCard (CardHandle * fromCard, CardHandle * toCard, CheckFunction checkFunction, WORD listen,
		DWORD * received, DWORD * transmitted)
{
  int result;
  BYTE *packet;
  WORD protocol;
  int length;
  GenericHeader *headerAddrs;
  PktBuf *pktBuf;
  int i;

  // Check if there are any management packets waiting on toCard. If so and 
  //   while there is room to, deliver them.
  while (toCard->mgmtQueue.head && toCard->sendsPending < toCard->maxSends)
  {
    // printf("found packet!\n");
    pktBuf = dequeuePktBuf (&toCard->mgmtQueue, toCard->mgmtQueue.head);
    sendPacket (pktBuf, toCard->common->moduleId);
  }

  // Loop until time to start working on the other card.
  for (;;)
  {

    // Check if there are no more packets waiting on this card.
    if (fromCard->queue.head == NULL)
      break;

    //fprintf(stderr,"packet waiting\n");

    // If the next packet has a sequence number less than equal to the packet
    //   on the other card and the other card has room to transmit packets to 
    //   this card or the other card has too many packets pending on it
    //   then quit.
    if (toCard->queue.head != NULL &&
	toCard->queue.head->sequence >=
	fromCard->queue.head->sequence &&
	fromCard->sendsPending < fromCard->maxSends ||
	toCard->sendsPending >= toCard->maxSends)
    {

      //fprintf(stderr,"breaking\n");

      break;
    }

    pktBuf = dequeuePktBuf (&fromCard->queue, fromCard->queue.head);

    ++*received;

    //fprintf(stderr,"Got a packet length = %d in from board number %d\n",ecb->dataLength,ecb->boardNumber);
    //for (i = 0;i < ecb->dataLength + headerSize;++i) {
    //      if (i % 20 == 0)
    //              fprintf(stderr,"\n");
    //      fprintf(stderr,"%02X ",ecb->fragments[0].fragmentAddress[i]);
    //}
    //fprintf(stderr,"\n");

    // Get the beginning of the protocol portion of the packet and pull out the protocol
    //   and translate it to an internal one.
    switch (mediaType)
       {
	 case MEDIA_FDDI:
	   protocol = swapWord (*(WORD *) & ((Ieee802Dot2SnapHeader *)
					     (pktBuf->buffer + sizeof (FddiHeader)))->protocolId[3]);
	   headerAddrs = (GenericHeader *) (pktBuf->buffer + 1);
	   break;
	 case MEDIA_ETHERNET:
	   // Easy mapping. Note we need to swap it for internal use.
	   protocol = swapWord (((EthernetIIHeader *) pktBuf->buffer)->etherType);
	   headerAddrs = (GenericHeader *) pktBuf->buffer;
	   break;
	 case MEDIA_TOKEN:
	   PERROR ("unsupported media")
	     break;
       }

    //fprintf(stderr,"protocol = %04X\n",protocol);

    length = pktBuf->packetLength - headerSize;
    packet = pktBuf->buffer + headerSize;

    // Check if the packet was destined to us.
    if (listen && checkLocal (fromCard->common->moduleId, &headerAddrs->destHost, protocol, packet) == YES)
    {
      // Move the actual protocol packet to the beginning of the buffer
      //   since we don't need to see the header any more.
      memcpy (pktBuf->buffer, &protocol, sizeof (WORD));
      memmove (pktBuf->buffer + sizeof (WORD), packet, length);

      pktBuf->packetLength = length;

      // Enqueue the packet on the protocol stack. Note that we don't actually
      //   handle the packet now.
      enqueuePktBuf (&protocolQueue, pktBuf);
    }
    else
    {
      // Check if it should be allowed out. 
      if (checkFunction (protocol, packet, length) == YES)
      {

	//fprintf(stderr,"dest address = %02X:%02X:%02X:%02X:%02X:%02X\n",
	//      ecb->immediateAddress.bytes[0],
	//      ecb->immediateAddress.bytes[1],
	//      ecb->immediateAddress.bytes[2],
	//      ecb->immediateAddress.bytes[3],
	//      ecb->immediateAddress.bytes[4],
	//      ecb->immediateAddress.bytes[5]);

	++*transmitted;

	// Give the packet to NDIS to be delivered.
	sendPacket (pktBuf, toCard->common->moduleId);

	//freePktBuf(pktBuf);
      }
      else
      {
	// The packet has been filtered. Throw it away.
	freePktBuf (pktBuf);
      }
    }
  }
}

void checkCards (void)
{
  // Check for packets to forward from the Internet to campus.
  checkCard (internet, campus, checkIncomingPacket, filterConfig.listenMode & OUTSIDE_MASK,
	     &theStats.outsideRx, &theStats.insideTx);

  // Check for packets to forward from campus to the Internet.
  checkCard (campus, internet, checkOutgoingPacket, filterConfig.listenMode & INSIDE_MASK,
	     &theStats.insideRx, &theStats.outsideTx);
}

void initMemory (void)
{
  DWORD size;

  // Allocate the network transfer buffer (it was taking up too much room in the DGROUP segment; stupid DOS).
  networkTransferBuffer = (BYTE *) farmalloc (NETWORK_TRANSFER_BUFFER_SIZE);

  // Allocate the access list tables.
  in = (AccessListTableEntry *) farmalloc (MAX_NUM_ACCESS_LISTS *
					   MAX_NUM_ACCESS_RANGES *
					   sizeof (AccessListTableEntry));

  // fprintf(stdout,"in = %04X:%04X\n",FP_SEG(in),FP_OFF(in));

  out = (AccessListTableEntry *) farmalloc (MAX_NUM_ACCESS_LISTS *
					    MAX_NUM_ACCESS_RANGES *
					    sizeof (AccessListTableEntry));

  // fprintf(stdout,"out = %04X:%04X\n",FP_SEG(out),FP_OFF(out));

  source = (AccessListTableEntry *) farmalloc (MAX_NUM_ACCESS_LISTS *
					       MAX_NUM_ACCESS_RANGES *
					       sizeof (AccessListTableEntry));

  udp = (AccessListTableEntry *) farmalloc (MAX_NUM_ACCESS_LISTS *
					    MAX_NUM_ACCESS_RANGES *
					    sizeof (AccessListTableEntry));

  // Clean out the address table.
  memset ((void *) addrTable, 0, sizeof (addrTable));

  // Clear out and initialize the reject table.
  memset ((void *) rejectTable, 0, sizeof (rejectTable));

  // Clear out and initialize the allow table.
  memset ((void *) allowTable, 0, sizeof (allowTable));

  memset ((BYTE *) newAddrTable, 0, sizeof (newAddrTable));

  // Clean out the access lists.
  memset ((void *) in, 0, MAX_NUM_ACCESS_LISTS *
	  MAX_NUM_ACCESS_RANGES *
	  sizeof (AccessListTableEntry));

  memset ((void *) out, 0, MAX_NUM_ACCESS_LISTS *
	  MAX_NUM_ACCESS_RANGES *
	  sizeof (AccessListTableEntry));

  memset ((void *) source, 0, MAX_NUM_ACCESS_LISTS *
	  MAX_NUM_ACCESS_RANGES *
	  sizeof (AccessListTableEntry));

  memset ((void *) udp, 0, MAX_NUM_ACCESS_LISTS *
	  MAX_NUM_ACCESS_RANGES *
	  sizeof (AccessListTableEntry));

  // Set up the default tables these are all allow lists.
  in[0].begin = 25;		// Mail

  in[0].end = 25;
  in[1].begin = 53;		// Name service

  in[1].end = 53;
  out[0].begin = 1;		// Everything

  out[0].end = 0xFFFF;
  source[0].begin = 20;		// FTP data connections

  source[0].end = 20;
  udp[0].begin = 1;		// Disallow TFTP (69) and Portmapper (111).

  udp[0].end = 68;
  udp[1].begin = 70;
  udp[1].end = 110;
  udp[2].begin = 112;
  udp[2].end = 0xFFFF;
}

void initTables (void)
{
  int fd;

  // Load in the reject table.
  fd = open (REJECT_LIST_FILE, O_RDONLY | O_BINARY);

  if (fd != -1)
  {

    if (read (fd, (char *) rejectTable, sizeof (RejectTableEntry) *
	      MAX_NUM_REJECT_ENTRIES) == -1)
    {
      fprintf (stdout, "Error reading in reject table\n");
      exit (1);
    }
    fprintf (stdout, "Loaded reject table\n");

    close (fd);
  }
  else
  {
    fprintf (stdout, "No reject table found\n");
  }

  // Load in the allow table.
  fd = open (ALLOW_LIST_FILE, O_RDONLY | O_BINARY);

  if (fd != -1)
  {

    if (read (fd, (char *) allowTable, sizeof (AllowTableEntry) *
	      MAX_NUM_ALLOW_ENTRIES) == -1)
    {
      fprintf (stdout, "Error reading in allow table\n");
      exit (1);
    }
    fprintf (stdout, "Loaded allow table\n");

    close (fd);
  }
  else
  {
    fprintf (stdout, "No allow table found\n");
  }

  // fprintf(stdout,"source =
  // %04X:%04X\n",FP_SEG(source),FP_OFF(source));
  // fprintf(stdout,"size = %ld etherHashTale = %04X:%04X\n",
  // 8192UL * sizeof(EtherHashEntry),
  // FP_SEG(etherHashTable),
  // FP_OFF(etherHashTable));

  // Load the default configuration. Begin with the access lists.
  fd = open (ACCESS_LIST_FILE, O_RDONLY | O_BINARY);

  if (fd == -1)
  {

    // If the access tables don't exist then they may end up
    // being
    // loaded from the net.
    fprintf (stdout, "No class table found\n");
  }
  else
  {
    if (read (fd, (char *) in, sizeof (AccessListTableEntry) *
	      MAX_NUM_ACCESS_LISTS * MAX_NUM_ACCESS_RANGES) == -1)
    {
      fprintf (stdout, "Error reading in access table\n");
      exit (1);
    }
    // fprintf(stdout,"result = %u\n",result);

    if (read (fd, (char *) out, sizeof (AccessListTableEntry) *
	      MAX_NUM_ACCESS_LISTS * MAX_NUM_ACCESS_RANGES) == -1)
    {
      fprintf (stdout, "Error reading out access table\n");
      exit (1);
    }
    // fprintf(stdout,"result = %u\n",result);

    if (read (fd, (char *) source, sizeof (AccessListTableEntry) *
	      MAX_NUM_ACCESS_LISTS * MAX_NUM_ACCESS_RANGES) == -1)
    {
      fprintf (stdout, "Error reading source access table\n");
      exit (1);
    }
    if (read (fd, (char *) udp, sizeof (AccessListTableEntry) *
	      MAX_NUM_ACCESS_LISTS * MAX_NUM_ACCESS_RANGES) == -1)
    {
      fprintf (stdout, "Error reading udp access table\n");
      exit (1);
    }
    // fprintf(stdout,"result = %u\n",result);

    fprintf (stdout, "Loaded class table\n");

    close (fd);
  }
}

void initNetworks (void)
{
  char wildcard[14];
  BYTE inetBuffer[32];
  int done;
  int fd;
  int curr;
  DWORD size;
  DWORD currSize;
  DWORD hash;
  in_addr network;
  struct ffblk ffblk;

  // Allocate the network cache.
  networkCache = farmalloc (sizeof (NetworkCacheEntry) * NUM_NETWORK_CACHE_ENTRIES);

  if (networkCache == NULL)
  {
    fprintf (stderr, "could not allocate the network cache\n");
    exit (1);
  }

  // Flush the network cache.
  networkCacheFlush ();

  // Loop and read in every network. Each network is in a file named
  // <network>.net where <network> is the IP network in hexadecimal.
  strcpy (wildcard, "*.");
  strcat (wildcard, NETWORK_EXTENSION);

  done = findfirst (wildcard, &ffblk, 0);

  while (!done)
  {

    // WATCH OUT! File must be opened in binary mode or else
    // text translation of carriage returns will occur.
    fd = open (ffblk.ff_name, O_RDONLY | O_BINARY);

    if (fd == -1)
    {
      fprintf (stdout, "Can't open %s\n", ffblk.ff_name);
      exit (1);
    }
    if (read (fd, (char *) &network, sizeof (network)) == -1)
    {
      fprintf (stdout, "Error reading network for %s\n", ffblk.ff_name);
      exit (1);
    }
    // Do the heavy duty checking.
    if (IN_CLASSB (network.S_addr))
    {
      // Class B address.
      network.S_addr = network.S_addr & CLASSB_NETWORK;
      size = 0x10000UL;
    }
    else if (IN_CLASSC (network.S_addr))
    {
      // Class C address.
      network.S_addr = network.S_addr & CLASSC_NETWORK;
      size = 0x100UL;
    }
    else
    {
      // We don't handle class A addresses.
      fprintf (stdout,
	       "Version %s does not handle class A or D addresses - %s skipped\n",
	       VERSION,
	       ffblk.ff_name);
      close (fd);
      done = findnext (&ffblk);

      continue;
    }

    // Insert the network into the hash table.
    hash = (network.S_addr & NETWORK_HASH_MASK) >> 19;
    // fprintf(stdout,"hash = %d\n",hash);
    curr = (int) hash;

    while (addrTable[curr].network.S_addr != network.S_addr &&
	   addrTable[curr].network.S_addr != 0UL)
    {

      curr = (curr + 1) & (MAX_NUM_NETWORKS - 1);

      if (curr == hash)
      {
	fprintf (stdout, "Network hash table is full\n");
	exit (1);
      }
    }

    // If not defined then insert it and allocate the memory.
    if (addrTable[curr].network.S_addr == 0UL)
    {

      // fprintf(stdout,"inserted network at %d\n",curr);

      // Set the address for the table.
      addrTable[curr].network.S_addr = network.S_addr;

      // Allocate the host table.
      addrTable[curr].hostTable = xmsAllocMem (size);

      if (addrTable[curr].hostTable == 0)
      {
	fprintf (stderr, "couldn't allocate a network table\n");
	exit (1);
      }

      // fprintf(stdout,"hostTable = %04X:%04X\n",
      // FP_SEG(addrTable[curr].hostTable),
      // FP_OFF(addrTable[curr].hostTable));
    }
    else
    {
      // This case should not happen.
      fprintf (stdout, "Overriding previous network entry\n");
    }

    // Since read() won't take longs.....
    currSize = size;
    while (currSize)
    {

      // Read the host table in. Be careful with the pointer
      // math in calculating the read address.
      if (read (fd, networkTransferBuffer,
		(WORD) (currSize < NETWORK_TRANSFER_BUFFER_SIZE ? currSize :
			NETWORK_TRANSFER_BUFFER_SIZE)) == -1)
      {

	fprintf (stdout, "Error reading in host table for %s\n", ffblk.ff_name);
	exit (1);
      }

      // Transfer the block to XMS memory.
      xmsCopy (addrTable[curr].hostTable, size - currSize,
	       0, (DWORD) networkTransferBuffer,
	       (currSize < NETWORK_TRANSFER_BUFFER_SIZE ? currSize :
		NETWORK_TRANSFER_BUFFER_SIZE) >> 1);

      currSize -= currSize < NETWORK_TRANSFER_BUFFER_SIZE ? currSize : NETWORK_TRANSFER_BUFFER_SIZE;
    }

    // Initialize the other parameters.
    addrTable[curr].dirty = NO;

    fprintf (stdout, "Loaded network %s\n", inet_ntoa (inetBuffer, &network));

    close (fd);

    done = findnext (&ffblk);
  }
}
