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

// Thanks to Klaus-Peter Kossakowski and Uwe Ellermann at DFN-CERT for contributing much of this implementation.

static SyslogMessageEntry syslogMessages[] =
{
  {"unknown event", SYSL_PRIORITY_WARNING},
  {"incoming class D", SYSL_PRIORITY_WARNING},
  {"outgoing class D", SYSL_PRIORITY_WARNING},
  {"incoming port", SYSL_PRIORITY_WARNING},
  {"outgoing port", SYSL_PRIORITY_WARNING},
  {"outgoing via allow", SYSL_PRIORITY_WARNING},
  {"incoming header too short", SYSL_PRIORITY_WARNING},
  {"outgoing header too short", SYSL_PRIORITY_WARNING},
  {"incoming via reject", SYSL_PRIORITY_WARNING},

	// priority NOTICE to allow easy separation for incoming IP protocols
  {"incoming IP", SYSL_PRIORITY_NOTICE},
  {"outgoing IP", SYSL_PRIORITY_WARNING},

	// priority INFO to allow easy separation for incoming MAC layer protocols
  {"incoming MAC layer protocol", SYSL_PRIORITY_INFO},
  {"outgoing MAC layer protocol", SYSL_PRIORITY_WARNING},

  {"beginning filtering", SYSL_PRIORITY_INFO},
  {"heartbeat", SYSL_PRIORITY_INFO},

	// Offset attack alerts.
  {"incoming fragment with IP offset == 1", SYSL_PRIORITY_WARNING},
  {"outgoing fragment with IP offset == 1", SYSL_PRIORITY_WARNING},

	// Marks the end of the messages.
  {NULL}
};

static IoVec udpData;
static BYTE udpBuffer[MAX_SYSL_DATA_LEN];
static Socket *logHost;

void initSyslog (void)
{
  SyslogMessageEntry *message;

  if (filterConfig.logHost.S_addr != 0UL)
  {
    // initialize IoVector for future use
    udpData.buffer = udpBuffer;

    // initialize Socket for future use
    logHost = socket (SOCKET_UDP,
		      SYSL_SRC_PORT,
		      &filterConfig.logHost,
		      SYSL_DEST_PORT,
		      0,
		      NULL);

    // Initialize the encoded priorities.
    for (message = syslogMessages; message->message; ++message)
    {
      sprintf (message->encodedPriority, "<%d>",
	       ((filterConfig.logFacility + 16) << 3) | message->priority);

      // printf("%s\n",message->encodedPriority);
    }
  }

  return;
}


// in_addr long2Addr(DWORD number)
// {
//      in_addr     address;
// 
//      address.S_addr = number;
// 
//      return(address);
// }

void syslogMessage (DWORD eventNo,...)
{
  va_list ap;

  //fprintf(stderr,"event mask == 0x%08lX logmask == 0x%08lX\n",syslogMessages[eventNo].mask,filterConfig.logMask);

  // If syslogging is not enabled or this message is disabled then just return.
  if (filterConfig.logHost.S_addr == 0UL || !((1UL << eventNo) & filterConfig.logMask))
     return;

  // we need no strncpy/strncat because we know the size of the strings
  strcpy (udpBuffer, syslogMessages[eventNo].encodedPriority);
  strcat (udpBuffer, "drawbridge: ");

  switch (eventNo)
  {
    case SYSL_IN_OFFSET:
    case SYSL_OUT_OFFSET:
    case SYSL_IN_REJECT:
    case SYSL_IN_PROT:
    case SYSL_OUT_PROT:
	 {
	   BYTE protocolNo;
	   in_addr srcAddr;
	   in_addr dstAddr;

	   // BYTE protocolNo, in_addr srcAddr, in_addr dstAddr,
	   //
	   // only protocolNo, srcAddr and dstAddr - maximum of 74 chars

	   va_start (ap, eventNo);

	   protocolNo = va_arg (ap, BYTE);
	   srcAddr = va_arg (ap, in_addr);
	   dstAddr = va_arg (ap, in_addr);

	   va_end (ap);

	   sprintf (udpBuffer + strlen (udpBuffer), "%s protocol %d from: %u.%u.%u.%u to: %u.%u.%u.%u",
		    syslogMessages[eventNo].message, protocolNo,
		    srcAddr.S_un_b.s_b4, srcAddr.S_un_b.s_b3, srcAddr.S_un_b.s_b2, srcAddr.S_un_b.s_b1,
		    dstAddr.S_un_b.s_b4, dstAddr.S_un_b.s_b3, dstAddr.S_un_b.s_b2, dstAddr.S_un_b.s_b1);
	   break;
	 }

    case SYSL_IN_CLASSD:
    case SYSL_OUT_CLASSD:
    case SYSL_IN_PORT:
    case SYSL_OUT_PORT:
    case SYSL_OUT_ALLOW:
    case SYSL_IN_LENGTH:
    case SYSL_OUT_LENGTH:
	 {
	   //protocol, addresses and ports - maximum of 84 chars

	   BYTE protocolNo;
	   in_addr srcAddr;
	   in_addr dstAddr;
	   WORD srcPort;
	   WORD dstPort;

	   va_start (ap, eventNo);

	   protocolNo = va_arg (ap, BYTE);
	   srcAddr = va_arg (ap, in_addr);
	   dstAddr = va_arg (ap, in_addr);
	   srcPort = va_arg (ap, WORD);
	   dstPort = va_arg (ap, WORD);

	   va_end (ap);

	   if (protocolNo == UDP_PROT)
	   {
	     strcat (udpBuffer, "UDP ");
	   }
	   else
	   {
	     strcat (udpBuffer, "TCP ");
	   }
	   sprintf (udpBuffer + strlen (udpBuffer), "%s from: %u.%u.%u.%u port %u to: %u.%u.%u.%u port %u",
		    syslogMessages[eventNo].message,
		    srcAddr.S_un_b.s_b4, srcAddr.S_un_b.s_b3, srcAddr.S_un_b.s_b2, srcAddr.S_un_b.s_b1, srcPort,
		    dstAddr.S_un_b.s_b4, dstAddr.S_un_b.s_b3, dstAddr.S_un_b.s_b2, dstAddr.S_un_b.s_b1, dstPort);
	   break;
	 }

    case SYSL_IN_FILTER:
    case SYSL_OUT_FILTER:
	 {
	   // MAC layer protocol

	   WORD protocol;

	   va_start (ap, eventNo);

	   protocol = va_arg (ap, WORD);

	   va_end (ap);

	   sprintf (udpBuffer + strlen (udpBuffer), "%s 0x%04X",
		    syslogMessages[eventNo].message,
		    protocol);

	   break;
	 }

    default:
	 strcat (udpBuffer, syslogMessages[eventNo].message);
	 break;
  }

  //printf("'%s' going to syslog\n",udpBuffer);

  // actual length of udp data
  udpData.length = strlen (udpBuffer);

  // send udp data to loghost
  sendvUdp (&udpData, 1, logHost);
}
