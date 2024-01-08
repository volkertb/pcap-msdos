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

Statistics theStats;

BYTE *buildBuf (BYTE *buf, DWORD val)
{
  if (val == 0xFFFFFFFF)
       sprintf (buf, "       N/A");
  else sprintf (buf, "%10lu", val);
  return buf;
}

void printStats (void)
{
  char  buf1[15];
  char  buf2[15];
  DWORD upTime;
  DWORD upDays;
  DWORD upHours;
  DWORD upMinutes;
  DWORD upSeconds;

  fprintf (stdout, "\n--- DRAWBRIDGE STATS ---\n");

  fprintf (stdout, "                             Inside       Outside\n");
  fprintf (stdout, "Packets filtered         %10lu    %10lu\n",
	   theStats.insideFiltered, theStats.outsideFiltered);
  fprintf (stdout, "Packets received         %10lu    %10lu\n", theStats.insideRx, theStats.outsideRx);
  fprintf (stdout, "Packets transmitted      %10lu    %10lu\n", theStats.insideTx, theStats.outsideTx);

  upDays = upHours = upMinutes = upSeconds = 0;

  upTime = ((days * 0x1800B0UL + *(DWORD *) MK_FP (0x0040, 0x006C) - startTime) * 10) / 182;

  //fprintf(stderr,"uptime = %ld\n",upTime);

  if (upTime)
  {
    upDays = upTime / (24UL * 60UL * 60UL);
    upTime -= upDays * 24UL * 60UL * 60UL;
    upHours = upTime / (60 * 60);
    upTime -= upHours * 60 * 60;
    upMinutes = upTime / 60;
    upSeconds = upTime - upMinutes * 60;
  }

  fprintf (stdout, "Up for %lu days %lu hours %lu minutes %lu seconds.\n", upDays, upHours, upMinutes, upSeconds);
  fprintf (stdout, "Cache Accesses: %10lu  Cache Misses: %10lu  Hit Ratio: ",
	   theStats.cacheAccesses, theStats.cacheMisses);

  if (theStats.cacheAccesses)
       fprintf (stdout, "%3ld%%\n", (theStats.cacheAccesses - theStats.cacheMisses) * 100 / theStats.cacheAccesses);
  else fprintf (stdout, "100%%\n");

  fprintf (stdout, "Dropped packets due to lack of packet buffers: %10lu\n", theStats.droppedPackets);

  fprintf (stdout, "\n--- CARD STATS ---\n");
  fprintf (stdout, "                             Inside       Outside\n");

  MAC_DISPATCH (campus)->request (common.moduleId,
				  0,
				  0,
				  0,
				  NDIS_GENERAL_REQUEST_UPDATE_STATISTICS,
				  campus->common->moduleDS);

  //fprintf(stderr,"result = %d\n",result);

  MAC_DISPATCH (internet)->request (common.moduleId,
				    0,
				    0,
				    0,
				    NDIS_GENERAL_REQUEST_UPDATE_STATISTICS,
				    internet->common->moduleDS);

  //fprintf(stderr,"result = %d\n",result);

  fprintf (stdout, "Rx Frames                %s    %s\n",
	   buildBuf (buf1, MAC_STATUS (campus)->totalFramesRx),
	   buildBuf (buf2, MAC_STATUS (internet)->totalFramesRx));

  fprintf (stdout, "Rx Bytes                 %s    %s\n",
	   buildBuf (buf1, MAC_STATUS (campus)->totalBytesRx),
	   buildBuf (buf2, MAC_STATUS (internet)->totalBytesRx));

  fprintf (stdout, "Rx Multicast             %s    %s\n",
	   buildBuf (buf1, MAC_STATUS (campus)->totalMulticastRx),
	   buildBuf (buf2, MAC_STATUS (internet)->totalMulticastRx));

  fprintf (stdout, "Rx Broadcast             %s    %s\n",
	   buildBuf (buf1, MAC_STATUS (campus)->totalBroadcastRx),
	   buildBuf (buf2, MAC_STATUS (internet)->totalBroadcastRx));

  fprintf (stdout, "Rx CRC Errors            %s    %s\n",
	   buildBuf (buf1, MAC_STATUS (campus)->totalFramesCrc),
	   buildBuf (buf2, MAC_STATUS (internet)->totalFramesCrc));

  fprintf (stdout, "Rx Buffer Drops          %s    %s\n",
	   buildBuf (buf1, MAC_STATUS (campus)->totalFramesDiscardedBufferSpaceRx),
	   buildBuf (buf2, MAC_STATUS (internet)->totalFramesDiscardedBufferSpaceRx));

  fprintf (stdout, "Rx Hardware Error Drops  %s    %s\n",
	   buildBuf (buf1, MAC_STATUS (campus)->totalFramesDiscardedHardwareErrorRx),
	   buildBuf (buf2, MAC_STATUS (internet)->totalFramesDiscardedHardwareErrorRx));

  fprintf (stdout, "Tx Frames                %s    %s\n",
	   buildBuf (buf1, MAC_STATUS (campus)->totalFramesTx),
	   buildBuf (buf2, MAC_STATUS (internet)->totalFramesTx));

  fprintf (stdout, "Tx Bytes                 %s    %s\n",
	   buildBuf (buf1, MAC_STATUS (campus)->totalBytesTx),
	   buildBuf (buf2, MAC_STATUS (internet)->totalBytesTx));

  fprintf (stdout, "Tx Multicast             %s    %s\n",
	   buildBuf (buf1, MAC_STATUS (campus)->totalMulticastTx),
	   buildBuf (buf2, MAC_STATUS (internet)->totalMulticastTx));

  fprintf (stdout, "Tx Broadcast             %s    %s\n",
	   buildBuf (buf1, MAC_STATUS (campus)->totalBroadcastTx),
	   buildBuf (buf2, MAC_STATUS (internet)->totalBroadcastTx));

  fprintf (stdout, "Tx Timeout Drops         %s    %s\n",
	   buildBuf (buf1, MAC_STATUS (campus)->totalFramesDiscardedTimeoutTx),
	   buildBuf (buf2, MAC_STATUS (internet)->totalFramesDiscardedTimeoutTx));

  fprintf (stdout, "Tx Hardware Error Drops  %s    %s\n",
	   buildBuf (buf1, MAC_STATUS (campus)->totalFramesDiscardedHardwareErrorTx),
	   buildBuf (buf2, MAC_STATUS (internet)->totalFramesDiscardedHardwareErrorTx));
}

void clearStats (void)
{
  int result;

  memset (&theStats, 0, sizeof (theStats));

  MAC_DISPATCH (campus)->request (common.moduleId,
				  0,
				  0,
				  0,
				  NDIS_GENERAL_REQUEST_CLEAR_STATISTICS,
				  campus->common->moduleDS);

  MAC_DISPATCH (internet)->request (common.moduleId,
				    0,
				    0,
				    0,
				    NDIS_GENERAL_REQUEST_CLEAR_STATISTICS,
				    internet->common->moduleDS);
}

void initStats (void)
{
  memset (&theStats, 0, sizeof (theStats));
}
