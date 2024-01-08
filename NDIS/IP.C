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

#include "db.h"

FilterConfig filterConfig;	// Structure that holds all of the configuration information
                                // for management.

static WORD ipSequence = 0;	// IP Packet Identification Counter

static int numSockets = 0;
static Socket sockets[MAX_NUM_SOCKETS];
static ArpEntry arpTable[ARP_TABLE_SIZE];

static Socket *icmpSocket;

// Convert an internet address to a string.
BYTE *inet_ntoa (BYTE * buffer, in_addr * addr)
{
  sprintf (buffer, "%d.%d.%d.%d", addr->S_un_b.s_b4,
	   addr->S_un_b.s_b3,
	   addr->S_un_b.s_b2,
	   addr->S_un_b.s_b1);
  return buffer;
}

// Convert a string to an internet address.
in_addr *inet_aton (BYTE * buffer, in_addr * addr)
{
  int t1;
  int t2;
  int t3;
  int t4;

  sscanf (buffer, "%d.%d.%d.%d", &t1, &t2, &t3, &t4);

  addr->S_un_b.s_b4 = t1;
  addr->S_un_b.s_b3 = t2;
  addr->S_un_b.s_b2 = t3;
  addr->S_un_b.s_b1 = t4;

  return addr;
}

ArpEntry *arpLookupIp (in_addr addr)
{
  int i;

  //fprintf(stderr,"%08lX\n",addr.S_addr);

  // Look up the ethernet address in the ARP table. For now it is a linear look but
  //   eventually it needs to be a hash look up.
  for (i = 0; i < ARP_TABLE_SIZE; ++i)
  {
    if (!arpTable[i].state)
      continue;

    //fprintf(stderr,"%08lX\n",arpTable[i].ipAddress.S_addr);
    //fprintf(stderr,"%02X:%02X:%02X:%02X:%02X:%02X\n",
    //      arpTable[i].hardwareAddress.bytes[0],
    //      arpTable[i].hardwareAddress.bytes[1],
    //      arpTable[i].hardwareAddress.bytes[2],
    //      arpTable[i].hardwareAddress.bytes[3],
    //      arpTable[i].hardwareAddress.bytes[4],
    //      arpTable[i].hardwareAddress.bytes[5]);

    if (memcmp (&arpTable[i].ipAddress, &addr, sizeof (in_addr)) == 0)
      break;
  }

  // If the entry was not found then just return. 
  if (i == ARP_TABLE_SIZE)
     return NULL;
 
  return arpTable + i;
}

ArpEntry *arpLookupHardware (HardwareAddress * addr)
{
  int i;

  // Look up the ethernet address in the ARP table to make sure it doesn't exist. For now it is a 
  //   linear look but eventually it needs to be a hash look up.
  for (i = 0; i < ARP_TABLE_SIZE; ++i)
  {
    if (!arpTable[i].state)
      continue;

    //fprintf(stderr,"%08lX\n",arpTable[i].ipAddress.S_addr);
    //fprintf(stderr,"%02X:%02X:%02X:%02X:%02X:%02X\n",
    //      arpTable[i].hardwareAddress.bytes[0],
    //      arpTable[i].hardwareAddress.bytes[1],
    //      arpTable[i].hardwareAddress.bytes[2],
    //      arpTable[i].hardwareAddress.bytes[3],
    //      arpTable[i].hardwareAddress.bytes[4],
    //      arpTable[i].hardwareAddress.bytes[5]);
    //fprintf(stderr,"%08lX\n",to->S_addr);

    if (memcmp (&arpTable[i].hardwareAddress, addr, sizeof (HardwareAddress)) == 0)
      break;
  }

  if (i == ARP_TABLE_SIZE)
    return NULL;

  return arpTable + i;
}

ArpEntry *arpAdd (void)
{
  int i;

  //  The entry does not exist. Get the next valid available one.
  for (i = 0; i < ARP_TABLE_SIZE; ++i)
    if (!arpTable[i].state)
      break;

  if (i == ARP_TABLE_SIZE)
    return NULL;

  return arpTable + i;
}

void arpCallBack (ScheduledEvent * event)
{
  int i;

  //fprintf(stderr,"arp cleanup called\n");

  // Scan the table and decrement the timers.
  for (i = 0; i < ARP_TABLE_SIZE; ++i)
  {
    // Only worry about existing ARP entries.
    if (arpTable[i].state)
    {
      // Decrement this entry's timer.
      arpTable[i].timeToLive -= ARP_TIMER_GRANULARITY;

      if (arpTable[i].timeToLive <= 0)
      {
	if (arpTable[i].state == ARP_ENTRY_VALID)
	{
	  // Delete the entry
	  arpTable[i].state = 0;
	}
	else if (arpTable[i].state == ARP_ENTRY_PENDING)
	{
	  // An unresolved entry timed out.
	  if (arpTable[i].retries > ARP_RETRIES)
	  {
	    arpTable[i].state = 0;

	    // If there was a pending packet then free it.
	    if (arpTable[i].pendingPacket)
                 free (arpTable[i].pendingPacket);
            else fprintf (stderr, "didn't find pending packet\n");
	  }
	  else
	  {
	    // Resend the ARP request.
	    ++arpTable[i].retries;
	    arpTable[i].timeToLive = ARP_ENTRY_RETRY_TIMEOUT;
	    sendArp (arpTable[i].ipAddress);
	  }
	}
	else
	{
	  fprintf (stderr, "unknown state for ARP entry\n");
	  exit (1);
	}
      }
    }
  }

  addScheduledEvent (ARP_TIMER_GRANULARITY, 0, arpCallBack);
}

// Note that this "raw" function is raw in terms of protocol, not framing. I build
//   the frame here.
void sendvRawMac (IoVec * vec, int length, HardwareAddress * to, WORD protocolId, int macId)
{
  int i;
  int packetSize;
  BYTE *currentLocation;
  PktBuf *pktBuf;
  EthernetIIHeader *ethernetIIHeader;
  FddiHeader *fddiHeader;
  Ieee802Dot2SnapHeader *ieee802Dot2SnapHeader;

  // Build a buffer that is the vector concatenated.
  pktBuf = allocPktBufMgmt ();
  if (pktBuf == NULL)
  {
    // fprintf(stderr,"no packet for the send\n");
    // Well so much for that idea.
    return;
  }

  currentLocation = pktBuf->buffer;
  packetSize = 0;

  // Build the frame.
  switch (mediaType)
  {
    case MEDIA_ETHERNET:
	 ethernetIIHeader = (EthernetIIHeader *) currentLocation;

	 // Build an ethernet frame. Remember to map the internal Filter Protocol ID to
	 //   the network protocol ID.
	 memcpy (&ethernetIIHeader->etherDestHost, to, sizeof (HardwareAddress));
	 memcpy (&ethernetIIHeader->etherSrcHost,
		 MAC_CHAR (cardHandleLookup[macId])->currentAddress,
		 sizeof (HardwareAddress));
	 ethernetIIHeader->etherType = swapWord (protocolId);

	 packetSize += sizeof (EthernetIIHeader);
	 currentLocation += sizeof (EthernetIIHeader);
         break;

    case MEDIA_TOKEN:
	 PERROR ("unsupported media type")
         break;

    case MEDIA_FDDI:
	 // Build an 802.2 SNAP frame.
	 fddiHeader = (FddiHeader *) currentLocation;

	 // Standard asynchronous LLC frame.
	 fddiHeader->frameControl = 0x50;

	 // We always use the physical address currently assigned to the card regardless
	 //   of the InternalMac address.
	 memcpy (&fddiHeader->etherDestHost, to, sizeof (HardwareAddress));
	 memcpy (&fddiHeader->etherSrcHost,
		 MAC_CHAR (cardHandleLookup[macId])->currentAddress,
		 sizeof (HardwareAddress));

	 packetSize += sizeof (FddiHeader);
	 currentLocation += sizeof (FddiHeader);

	 ieee802Dot2SnapHeader = (Ieee802Dot2SnapHeader *) currentLocation;

	 ieee802Dot2SnapHeader->dsap = 0xAA;
	 ieee802Dot2SnapHeader->ssap = 0xAA;
	 ieee802Dot2SnapHeader->control = 0x03;
	 memset (ieee802Dot2SnapHeader->protocolId, 0, 5);

	 *((WORD *) (ieee802Dot2SnapHeader->protocolId + 3)) = swapWord (protocolId);

	 packetSize += sizeof (Ieee802Dot2SnapHeader);
	 currentLocation += sizeof (Ieee802Dot2SnapHeader);
         break;

    default:
	 PERROR ("unsupported media type")
  }

  for (i = 0; i < length; ++i)
  {
    memcpy (currentLocation, vec[i].buffer, vec[i].length);
    packetSize += vec[i].length;
    currentLocation += vec[i].length;
  }

  // Put the packet length in the PktBuf.
  pktBuf->packetLength = packetSize;

  //printf("enqueueing send %08lX\n",pktBuf);

  // Enqueue the packet for sending.
  enqueuePktBuf (&cardHandleLookup[macId]->mgmtQueue, pktBuf);

  //sendPacket(pktBuf,macId);
}

void sendvRaw (IoVec * vec, int length, HardwareAddress * to, WORD protocolId,...)
{
  int macId;
  WORD mask;
  va_list ap;

  // Look up the correct interface for the ethernet address in the bridge table.
  if (IS_BROADCAST (*to))
  {
    va_start (ap, protocolId);
    mask = va_arg (ap, WORD);
    va_end (ap);

    macId = -1;

    if (mask & OUTSIDE_MASK)
      sendvRawMac (vec, length, to, protocolId, internet->common->moduleId);

    if (mask & INSIDE_MASK)
      sendvRawMac (vec, length, to, protocolId, campus->common->moduleId);
  }
  else
  {
    macId = bridgeLookUp (to);

    if (macId == -1)
       PERROR ("couldn't find hardware address in the bridge tables")

    sendvRawMac (vec, length, to, protocolId, macId);
  }
}

void sendArp (in_addr dest)
{
  ArpHeader arpHeader;
  static IoVec localVec[10];

  // We need to perform an arp request.
  arpHeader.op = swapWord (ARP_REQUEST);
  arpHeader.protType = swapWord (FILTER_IP_PROTOCOL);
  arpHeader.hardSize = 6;
  arpHeader.protSize = 4;

  arpHeader.senderIp = swapAddr (filterConfig.myIpAddr);
  arpHeader.targetIp = swapAddr (dest);
  memset (&arpHeader.target, 0xFF, sizeof (HardwareAddress));

  switch (mediaType)
  {
    case MEDIA_ETHERNET:
	 arpHeader.hardType = swapWord (1);
	 break;
    case MEDIA_TOKEN:
    case MEDIA_FDDI:
	 arpHeader.hardType = swapWord (6);
	 break;
    default:
	 PERROR ("unsupported media type")
  }

  localVec[0].buffer = (BYTE *) & arpHeader;
  localVec[0].length = sizeof (ArpHeader);

  //fprintf(stderr,"sending arp packets\n");

  if (filterConfig.internalMacSet == YES)
       memcpy (&arpHeader.sender, &filterConfig.internalMac, 6);
  else memcpy (&arpHeader.sender, MAC_CHAR (internet)->currentAddress, 6);

  // Send the request for the outside.
  sendvRaw (localVec, 1, &arpHeader.target, FILTER_ARP_PROTOCOL, OUTSIDE_MASK);

  if (filterConfig.internalMacSet == YES)
       memcpy (&arpHeader.sender, &filterConfig.internalMac, 6);
  else memcpy (&arpHeader.sender, MAC_CHAR (campus)->currentAddress, 6);

  // Send the request for the inside.
  sendvRaw (localVec, 1, &arpHeader.target, FILTER_ARP_PROTOCOL, INSIDE_MASK);
}

void sendvIp (in_addr * to, BYTE protocol, IoVec * vec, int length)
{
  int i;
  int dataSize;
  BYTE *currentLocation;
  IpHeader ipHeader;
  ArpEntry *arp;
  ArpHeader arpHeader;
  static IoVec localVec[10];
  in_addr *dest;

  // Zero out the header.
  memset (&ipHeader, 0, sizeof (IpHeader));

  // Find out the size of the data.
  dataSize = 0;
  for (i = 0; i < length; ++i)
    dataSize += vec[i].length;

  // Build an IP header.
  ipHeader.ip_hl = sizeof (IpHeader) >> 2;	// Standard header length with no options.

  ipHeader.ip_v = 4;
  ipHeader.ip_len = swapWord (sizeof (IpHeader) + dataSize);
  ipHeader.ip_id = swapWord (++ipSequence);
  ipHeader.ip_ttl = 30;
  ipHeader.ip_p = protocol;

  // Set up the destination and source IP addresses.
  ipHeader.ip_dst = swapAddr (*to);
  ipHeader.ip_src = swapAddr (filterConfig.myIpAddr);

  ipHeader.ip_sum = 0;
  ipHeader.ip_sum = chkSum ((BYTE *) & ipHeader, sizeof (IpHeader), NULL);

#if 0
  fprintf (stderr,"subnet mask = %08lX\n",filterConfig.mySubnetMask.S_addr);
  fprintf (stderr,"myipaddr = %08lX\n",filterConfig.myIpAddr.S_addr);
  fprintf (stderr,"dest = %08lX\n",to->S_addr);
  fprintf (stderr,"subnet mask & my ipaddr = %08lX\n",filterConfig.mySubnetMask.S_addr & filterConfig.myIpAddr.S_addr);
#endif

  // Check if we need to send this to the default gateway instead.
  if ((filterConfig.myIpAddr.S_addr & filterConfig.mySubnetMask.S_addr) !=
      (to->S_addr & filterConfig.mySubnetMask.S_addr))
    // The destination address is not on my network so send it to the
    //   gateway.
       dest = &filterConfig.myGateway;
  else dest = to;

  // Look up the ARP entry.
  arp = arpLookupIp (*dest);

  if (arp && arp->state == ARP_ENTRY_VALID)
  {
    //fprintf(stderr,"sending directly IoVec length = %d\n",length);

    localVec[0].buffer = (BYTE *) & ipHeader;
    localVec[0].length = sizeof (IpHeader);

    // Copy the incoming vec to the outgoing.
    for (i = 0; i < length; ++i)
      localVec[i + 1] = vec[i];

    // Do a raw send on the result.
    sendvRaw (localVec, length + 1, &arp->hardwareAddress, FILTER_IP_PROTOCOL);

    arp->timeToLive = ARP_ENTRY_TIMEOUT;
  }
  else
  {
    //fprintf(stderr,"No arp entry. Arping.\n");

    // Add an ARP entry if necessary.
    if (!arp)
    {
      arp = arpAdd ();

      if (arp == NULL)
      {
	// Ran out of ARP entries. return.
	fprintf (stderr, "ran out of arp entries\n");
	return;
      }

      arp->state = ARP_ENTRY_PENDING;
      arp->ipAddress = *dest;
    }
    else
    {
      // Since it is pending then release the queued packet.
      if (arp->pendingPacket)
           free (arp->pendingPacket);
      else fprintf (stderr, "pending packet not found\n");
    }

    // This resets the timer if it was pending.
    arp->timeToLive = ARP_ENTRY_RETRY_TIMEOUT;

    // Save off the packet.
    arp->pendingPacket = farmalloc (dataSize + sizeof (IpHeader));
    arp->pendingPacketLength = dataSize + sizeof (IpHeader);
    currentLocation = arp->pendingPacket;

    memcpy (currentLocation, &ipHeader, sizeof (IpHeader));
    currentLocation += sizeof (IpHeader);

    for (i = 0; i < length; ++i)
    {
      memcpy (currentLocation, vec[i].buffer, vec[i].length);
      currentLocation += vec[i].length;
    }

    // Send out the ARP request.
    sendArp (*dest);
  }
}

void sendvUdp (IoVec * vec, int length, Socket * to)
{
  static IoVec udpVec[10];
  UdpHeader udpHeader;
  int udpDataSize;
  int i;

  udpVec[0].buffer = (BYTE *) & udpHeader;
  udpVec[0].length = sizeof (UdpHeader);
  udpDataSize = sizeof (UdpHeader);

  for (i = 0; i < length; ++i)
  {
    udpVec[i + 1] = vec[i];
    udpDataSize += vec[i].length;
  }

  udpHeader.uh_sport = swapWord (to->localPort);
  udpHeader.uh_dport = swapWord (to->remotePort);
  udpHeader.uh_ulen = swapWord (udpDataSize);
  udpHeader.uh_sum = 0;		// Disable checksums.

  sendvIp (&to->host, UDP_PROT, udpVec, length + 1);
}

// Handle an ARP packet.
void handleArp (ArpHeader * arpHeader, int length)
{
  int macId;
  static IoVec vec[10];
  ArpEntry *arp;

  //fprintf(stderr,"Received ARP packet\n");

  // It must have been requesting us properly or we would never have gotten here.

  switch (swapWord (arpHeader->op))
  {
    case ARP_REQUEST:
	 // Update the ARP entry for the sender.
	 arp = arpLookupIp (swapAddr (arpHeader->senderIp));

	 if (!arp)
	 {
	   // No entry for this guy yet. Create one.
	   arp = arpAdd ();

	   if (arp == NULL)
	   {
	     // Ran out of ARP entries. return.
	     fprintf (stderr, "ran out of arp entries\n");
	     return;
	   }

	   arp->ipAddress = arpHeader->senderIp;
	 }

	 // Copy the hardware address to the hardware address. Set the state and timer.
	 memcpy (&arp->hardwareAddress, &arpHeader->sender, 6);

	 if (arp->state == ARP_ENTRY_PENDING)
	 {
	   if (arp->pendingPacket)
	   {
	     vec[0].buffer = arp->pendingPacket;
	     vec[0].length = arp->pendingPacketLength;

	     // Send the queued packet.
	     sendvRaw (vec, 1, &arp->hardwareAddress, FILTER_IP_PROTOCOL);

	     // Free the buffer.
	     free (arp->pendingPacket);
	   }
	   else
	   {
	     fprintf (stderr, "pending packet missing\n");
	   }
	 }

	 arp->timeToLive = ARP_ENTRY_TIMEOUT;
	 arp->state = ARP_ENTRY_VALID;

	 // Build a response packet.
	 arpHeader->op = swapWord (ARP_REPLY);

	 macId = bridgeLookUp (&arpHeader->sender);

	 if (macId == -1)
	 {
	   PERROR ("couldn't find hardware address in the bridge tables")
	 }

	 //fprintf(stderr,"request came from macId %d\n",macId);

	 // Use the buffer that the request came in to build the response.
	 memcpy (&arpHeader->target, &arpHeader->sender, 6);
	 memcpy (&arpHeader->targetIp, &arpHeader->senderIp, sizeof (in_addr));

	 if (filterConfig.internalMacSet == YES)
	 {
	   memcpy (&arpHeader->sender, &filterConfig.internalMac, 6);
	 }
	 else
	 {
	   memcpy (&arpHeader->sender, MAC_CHAR (cardHandleLookup[macId])->currentAddress, 6);
	 }
	 memcpy (&arpHeader->senderIp, &filterConfig.myIpAddr, sizeof (in_addr));
	 swapLongPtr (&arpHeader->senderIp.S_addr);

	 vec[0].buffer = (BYTE *) arpHeader;
	 vec[0].length = sizeof (ArpHeader);

	 // Send back the response.
	 sendvRaw (vec, 1, &arpHeader->target, FILTER_ARP_PROTOCOL);
	 break;

    case ARP_REPLY:
	 //fprintf(stderr,"Got an ARP reply\n");

	 macId = bridgeLookUp (&arpHeader->sender);

	 if (macId == -1)
	 {
	   PERROR ("couldn't find hardware address in the bridge tables")
	 }

	 // Check the hardware address to make sure it isn't broadcast/multicast.
	 if (IS_BROADCAST (arpHeader->sender))
	   break;

	 //fprintf(stderr,"looking for pending arp request.\n");

	 // Check for a pending ARP request.
	 arp = arpLookupIp (swapAddr (arpHeader->senderIp));

	 // If the entry does not exist then we never asked for this reply.
	 //   Chunk it.
	 if (arp == NULL || arp->state != ARP_ENTRY_PENDING)
	 {
	   //fprintf(stderr,"could not find it\n");
	   break;
	 }

	 arp->state = ARP_ENTRY_VALID;
	 memcpy (&arp->hardwareAddress, &arpHeader->sender, sizeof (HardwareAddress));

	 if (arp->pendingPacket)
	 {
	   //fprintf(stderr,"sending pending packet.\n");

	   vec[0].buffer = arp->pendingPacket;
	   vec[0].length = arp->pendingPacketLength;

	   // Send the queued packet.
	   sendvRaw (vec, 1, &arp->hardwareAddress, FILTER_IP_PROTOCOL);

	   // Free the buffer.
	   free (arp->pendingPacket);
	 }
	 else
	 {
	   fprintf (stderr, "pending packet missing\n");
	   exit (1);
	 }

	 break;

    default:
	 // We don't handle anything else.
	 break;
  }

}

// Handle an incoming ICMP.
void handleIcmp (IcmpHeader * icmpHeader, int length, in_addr * from)
{
  static IoVec vec[10];

  // We handle only redirects and echo requests.
  switch (icmpHeader->type)
  {
    case ICMP_ECHO_REQUEST:
	 // Generate a reply.
	 //fprintf(stderr,"received echo request\n");

	 // Calculate the checksum of the REQUEST.
	 if (chkSum ((BYTE *) icmpHeader, length, NULL) != 0)
	 {
	   //fprintf(stderr,"bad checksum in ICMP request body (%04X)\n",icmpHeader->chkSum);
	   return;
	 }

	 // Change it into a reply and send it back.
	 icmpHeader->type = ICMP_ECHO_REPLY;
	 icmpHeader->chkSum = 0;
	 icmpHeader->chkSum = chkSum ((BYTE *) icmpHeader, length, NULL);

	 vec[0].buffer = (BYTE *) icmpHeader;
	 vec[0].length = length;

	 // Send the packet back.
	 sendvIp (from, ICMP_PROT, vec, 1);
	 break;

    case ICMP_REDIRECT:
	 //fprintf(stderr,"received redirect\n");
	 break;

    default:
	 fprintf (stderr, "received strange ICMP\n");
	 break;
  }
}

// Handle an incoming UDP packet.
void handleUdp (UdpHeader * udpHeader, int length, in_addr * from)
{
  int  i;
  WORD dport;

  // Ignore the check sum for now.

  //fprintf(stderr,"received UDP packet\n");

  dport = swapWord (udpHeader->uh_dport);

  // Find the correct socket that this request is headed to.
  for (i = 0; i < numSockets; ++i)
  {
    if (sockets[i].kind == SOCKET_UDP &&
	sockets[i].localPort == dport &&
        !memcmp(&sockets[i].host, from, sizeof(in_addr)))
    {
      //fprintf(stderr,"found a socket for the packet\n");
      break;
    }
  }

  if (i == numSockets)
  {
    //fprintf(stderr,"packet to invalid udp socket or invalid manager; ignored\n");
    return;
  }

  // Update the socket structure with the source socket if necessary.
  if (!(sockets[i].flags & SOCKET_FIXED_DEST_PORT))
  {
    //fprintf(stderr,"updating the remote port\n");
    sockets[i].remotePort = swapWord (udpHeader->uh_sport);
  }

  // Make sure that we don't go into lala land if the socket is used for transmitting only.
  if (sockets[i].callBack)
    // Call the callback with the correct socket.
    sockets[i].callBack (((BYTE *) udpHeader) + sizeof (UdpHeader),
			 swapWord (udpHeader->uh_ulen) - sizeof (UdpHeader),
			 &sockets[i]);
}

// Handle an IP packet.
void handleIp (IpHeader * ipHeader, int length)
{
  in_addr temp;
  BYTE   *protocolHeader;
  int     protocolSize;
  int     i;

#if 0
  fprintf(stderr,"Handled IP packet!!!! length = %d\n",length);

  for (i = 0;i < length;++i)
  {
    if (i % 20 == 0)
       fprintf (stderr,"\n");
    fprintf (stderr,"%02X ",((BYTE*)ipHeader)[i]);
  }
  fprintf (stderr,"\n");
#endif

  // Check the version.
  if (ipHeader->ip_v != 4)
  {
    // fprintf(stderr,"wrong IP version (%d)\n",ipHeader->ip_v);
    return;
  }

  // Check to make sure there is no fragmentation going on. We can't
  //   handle it.
  if ((ipHeader->ip_off & IP_OFF_MASK) || (ipHeader->ip_off & IP_MORE_FRAGMENTS))
  {
    fprintf (stdout, "Received IP fragments on the management channel. Drawbridge\n");
    fprintf (stdout, "  doesn't handle IP fragmentation.\n");
    return;
  }

  // Check the checksum. Note that we do not have to byte reverse things
  //   for this check to work.
  if (chkSum ((BYTE *) ipHeader, ipHeader->ip_hl << 2, NULL) != 0)
  {
    fprintf (stderr, "bad checksum in IpHeader (header len = %d)\n", ipHeader->ip_hl << 2);
    return;
  }

  //fprintf(stderr,"unswapped = %08lX\n",ipHeader->ip_dst.S_addr);
  temp = swapAddr (ipHeader->ip_dst);
  //fprintf(stderr,"swapped = %08lX\n",temp.S_addr);

  // Check to see if it has our IP address on it.
  if (memcmp (&temp, &filterConfig.myIpAddr, sizeof (in_addr)) != 0)
  {
    fprintf (stderr, "not my IP address\n");
    return;
  }

  protocolHeader = ((BYTE *) ipHeader) + (ipHeader->ip_hl << 2);
  protocolSize = swapWord (ipHeader->ip_len) - (ipHeader->ip_hl << 2);

  temp = swapAddr (ipHeader->ip_src);

  // Check to see if UDP or ICMP and dispatch appropriately.
  switch (ipHeader->ip_p)
  {
    case UDP_PROT:
	 handleUdp ((void *) protocolHeader, protocolSize, &temp);
	 break;
    case ICMP_PROT:
	 handleIcmp ((void *) protocolHeader, protocolSize, &temp);
	 break;
    default:
	 // Just ignore it if it is not recognizable.
	 break;
  }
}

// Allocate a socket.
Socket *socket (int kind, WORD localPort, in_addr * host, WORD remotePort, int flags, CallBack callBack)
{
  if (numSockets == MAX_NUM_SOCKETS)
     return NULL;
  
  sockets[numSockets].kind = kind;
  sockets[numSockets].localPort = localPort;
  sockets[numSockets].remotePort = remotePort;
  sockets[numSockets].host = *host;
  sockets[numSockets].flags = flags;
  sockets[numSockets].callBack = callBack;

  return (&sockets[numSockets++]);
}

// Check the packet to see if we really want to receive it.
int checkLocal (WORD macId, HardwareAddress * dest, WORD protocol, BYTE * packet)
{
  ArpHeader *arpHeader;
  in_addr tempIp;
  BYTE temp[20];

  // Make sure it is IP or ARP.
  if (protocol != FILTER_IP_PROTOCOL && protocol != FILTER_ARP_PROTOCOL)
    return NO;

  // If the packet was broadcast then check if we really want to except it. 
  //   We must be careful since if we accept this packet, it will not be
  //   bridged.
  if (!IS_BROADCAST (*dest))
  {
    // Since it is not broadcast then we need to make sure it was unicast to this
    //   MAC. Also if we are in FDDI mode then we need to check against our fake
    //   mac address.
    if (filterConfig.internalMacSet == YES)
    {
      // fprintf(stderr,"checking against InternalMac\n");

      if (!memcmp(dest,&filterConfig.internalMac,sizeof(HardwareAddress)))
         return YES;
    }
    else
    {
#if 0
      fprintf (stderr,"checking against card mac: macId = %d\n",macId);

      fprintf (stderr,"dest = %02X:%02X:%02X:%02X:%02X:%02X\n",
               dest->bytes[0], dest->bytes[1], dest->bytes[2],
               dest->bytes[3], dest->bytes[4], dest->bytes[5]);

      fprintf (stderr,"current address = %02X:%02X:%02X:%02X:%02X:%02X\n",
               MAC_CHAR(cardHandleLookup[macId])->currentAddress[0],
               MAC_CHAR(cardHandleLookup[macId])->currentAddress[1],
               MAC_CHAR(cardHandleLookup[macId])->currentAddress[2],
               MAC_CHAR(cardHandleLookup[macId])->currentAddress[3],
               MAC_CHAR(cardHandleLookup[macId])->currentAddress[4],
               MAC_CHAR(cardHandleLookup[macId])->currentAddress[5]);
#endif
      if (!memcmp(dest, MAC_CHAR (cardHandleLookup[macId])->currentAddress,
                  sizeof (HardwareAddress)))
         return YES;
    }

    // fprintf(stderr,"not to me\n");
    return NO;
  }
  // Ok, we know it is broadcast at this point.
  else if (protocol == FILTER_ARP_PROTOCOL)
  {

    // It is a broadcast ARP at this point.
    arpHeader = (ArpHeader *) packet;

    //sprintf(GET_DEBUG_STRING,"looking at an ARP packet target = %02X:%02X:%02X:%02X:%02X:%02X\n",
    //      arpHeader->target.bytes[0],
    //      arpHeader->target.bytes[1],
    //      arpHeader->target.bytes[2],
    //      arpHeader->target.bytes[3],
    //      arpHeader->target.bytes[4],
    //      arpHeader->target.bytes[5]);

    tempIp = swapAddr (arpHeader->targetIp);

    //fprintf(stderr,"looking at arp target ip = %s\n",inet_ntoa(temp,&tempIp));

    if (memcmp (&tempIp, &filterConfig.myIpAddr, sizeof (in_addr)) == 0)
    {
      //sprintf(GET_DEBUG_STRING,"accepting an ARP packet\n");
      // The ARP request is looking for me.
      return YES;
    }
  }

  // All other broadcasts need to be passed.
  return NO;
}

// Check to see if any IP packets have been delivered to us.
void checkIp (void)
{
  ArpEntry *arp;
  PktBuf   *pktBuf;
  BYTE     *packet;
  WORD     i, protocol;

  if (protocolQueue.head != NULL)
  {
    // Since we may get clobbered by incoming packets. Note that we make this
    // critical region extremely small because some of the management code
    // would probably inadvertantly reenable interrupts eventually via calls
    // to DOS.
    pktBuf = dequeuePktBuf (&protocolQueue, protocolQueue.head);

    // Note we have stripped off the frame earlier and stuck just the protocol at the front.
    protocol = *(WORD *) pktBuf->buffer;

    // Find out whether it is IP or ARP and handle appropriately.
    if (protocol == FILTER_IP_PROTOCOL)
    {

      //fprintf(stderr,"Got an IP packet\n");
      //fprintf(stderr,"length = %d\n",pktBuf->packetLength);

      // Dispatch the packet to the IP layer.
      handleIp ((void *) (pktBuf->buffer + sizeof (WORD)), pktBuf->packetLength);
    }
    else if (protocol == FILTER_ARP_PROTOCOL)
    {
      //fprintf(stderr,"Got an ARP packet\n");

      // Dispatch the packet to the ARP layer.
      handleArp ((void *) (pktBuf->buffer + sizeof (WORD)), pktBuf->packetLength);
    }
    else
    {
      PERROR ("unknown protocol type in packet in protocol stack")
    }

    freePktBuf (pktBuf);
  }
}

// Init the IP module.
void initIp (void)
{
  // Initialize some things. Note that most stuff was read in by the ODI init routine since it
  //   is in the NET.CFG file.
  filterConfig.listenMode = 0;
  filterConfig.numManagers = 0;
  filterConfig.listenPort = DEFAULT_PORT;

  // Make sure the IP stuff is cleared.
  filterConfig.myGateway.S_addr = 0;
  filterConfig.mySubnetMask.S_addr = 0;
  filterConfig.myIpAddr.S_addr = 0;
  filterConfig.logHost.S_addr = 0;
  filterConfig.logFacility = DEFAULT_SYSL_FACILITY;

  // Set up a starting point for the IP sequence counter. Note that the random number generator
  //   was initialized in potp.
  ipSequence = rand ();

  // Zero out the ARP table.
  memset (arpTable, 0, sizeof (ArpEntry) * ARP_TABLE_SIZE);

  // Start the ARP timer callback.
  addScheduledEvent (ARP_TIMER_GRANULARITY, 0, arpCallBack);
}
