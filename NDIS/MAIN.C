
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

// Handles for the two defined cards.
CardHandle *campus   = NULL;
CardHandle *internet = NULL;

ScheduledEvent events[MAX_NUM_EVENTS];

DWORD nextEvent  = 0;
DWORD startTime  = 0;
DWORD days       = 0;
DWORD lastMyTime = 0;

void init (void)
{
  // Get our boot time.
  startTime = lastMyTime = *(DWORD *) MK_FP (0x0040, 0x006C);

  // Zero out the events.
  memset (events, 0, sizeof (events));

  // Add in the handler for the keyboard.
  addScheduledEvent (1, 0, keyCheckCallBack);

  initXms ();

  initStats ();

  // Initialize the encryption routines. This needs to come first since this is where
  //   the random number generator is initialized.
  initPotp ();

  // Initialize the bridging code.
  initBridge ();

  // Initialize the IP stack.
  initIp ();

  // Initialize the filter stuff.
  initMemory ();
  initTables ();
  initNetworks ();

  // Initialize the NDIS stuff. Packets start arriving after this call. Note that the configuration
  //   for IP is read at this point.
  initNdis (&campus, &internet);
}

// Note this kind of stuff only works on Intel.
void fastCopy (BYTE * dest, BYTE * src, int length)
{
  int temp;

  moveLongs (dest, src, length >> 2);
  temp = length & ~0x0003;
  moveBytes (dest + temp, src + temp, length & 0x0003);
}

void checkMisc (void)
{
  DWORD myTime;
  int i;

  // Output any debugging strings accumulated. This mechanism is used to print debugging
  //   from interrupt threads.
  while (stopDebug != startDebug)
  {
    if (++startDebug == MAX_NUM_DEBUG_STRINGS)
       startDebug = 0;

    fprintf (stderr, ">%s", debugStrings[startDebug]);
  }

  // Get the time of day timer.
  myTime = *(DWORD *) MK_FP (0x0040, 0x006C);

  // This is another kludge due to braindead PCs. The stupid timer in the BIOS counts 
  //   ticks since midnight, and so of course is set to zero every 24 hours. Note that
  //   0x1800B0L is the largest value the clock can have.
  if (myTime < lastMyTime)
  {
    // Joy. Midnight just went by. Retime the event queue.
    for (i = 0; i < MAX_NUM_EVENTS; ++i)
    {
      if (events[i].valid == YES)
      {
	// Be careful. .time is an DWORD so we must set to 0
	//   if it is less than 0x1800B0L.
	if (events[i].time < 0x1800B0UL)
             events[i].time = 0L;
        else events[i].time -= 0x1800B0UL;
      }
    }

    // Reset nextEvent if need be also. Be careful not to set nextEvent to zero since this
    //   means no event is pending.
    if (nextEvent)
    {
      if (nextEvent <= 0x1800B0UL)
           nextEvent = 1;
      else nextEvent -= 0x1800B0UL;
    }

    // Increment the days.
    ++days;
  }

  // Save off the current sample of time so we can figure out when midnight rolls around.
  lastMyTime = myTime;

  // Check if an event has expired.
  if (nextEvent && nextEvent < myTime)
  {
    // If we don't put this kind of wrapper around the check stuff we
    //   could end up with a race condition that causes us to lose events.
    for (i = 0; i < MAX_NUM_EVENTS; ++i)
      events[i].new = NO;

    for (i = 0; i < MAX_NUM_EVENTS; ++i)
    {
      if (events[i].valid == YES && events[i].new == NO && events[i].time < myTime)
      {
	events[i].callBack (&events[i]);
	events[i].valid = NO;
      }
    }

    // Note we cannot mix the following step with the loop above. If the callBack()
    //   tries to register another event it may not work correctly.
    nextEvent = 0xFFFFFFFF;

    for (i = 0; i < MAX_NUM_EVENTS; ++i)
    {
      if (events[i].valid && events[i].time < nextEvent)
         nextEvent = events[i].time;
    }
    if (nextEvent == 0xFFFFFFFF)
       nextEvent = 0;
  }

}

ScheduledEvent *addScheduledEvent (DWORD expire, DWORD opaque, EventCallBack callBack)
{
  int i,j;

  for (i = 0; i < MAX_NUM_EVENTS; ++i)
    if (events[i].valid == NO)
      break;

  if (i == MAX_NUM_EVENTS)
    return NULL;

  // So we don't have to do floating point math.
  events[i].time = (*(DWORD *) MK_FP (0x0040, 0x006C)) + ((expire * 182) / 10);

  //fprintf(stderr,"event will be at %ld\n",events[i].time);

  events[i].opaque = opaque;
  events[i].callBack = callBack;
  events[i].valid = YES;
  events[i].new = YES;

  nextEvent = 0xFFFFFFFF;
  for (j = 0; j < MAX_NUM_EVENTS; ++j)
  {
    if (events[j].valid && events[j].time < nextEvent)
      nextEvent = events[j].time;
  }

  if (nextEvent == 0xFFFFFFFF)
    nextEvent = 0;

  return &events[i];
}

void deleteScheduledEvent (ScheduledEvent * event)
{
  int i;

  event->valid = NO;

  nextEvent = 0xFFFFFFFF;
  for (i = 0; i < MAX_NUM_EVENTS; ++i)
  {
    if (events[i].valid && events[i].time < nextEvent)
      nextEvent = events[i].time;
  }

  if (nextEvent == 0xFFFFFFFF)
    nextEvent = 0;
}

void usage (void)
{
  fprintf (stderr, "usage: bridge\n");
  exit (1);
}

void keyCheckCallBack (ScheduledEvent * event)
{
  char c;

  //DWORD myTime;

  // myTime = *(DWORD *) MK_FP(0x0040,0x006C);

  //syslogMessage(SYSL_HEARTBEAT);

  // Check to see if any key strokes are waiting.
  if (*(WORD *) MK_FP (0x0040, 0x001A) != *(WORD *) MK_FP (0x0040, 0x001C))
  {

    c = getch ();

    // Take the requested action.
    switch (c)
    {
      case '$':
	   exit (0);
	   break;
      case 'S':
	   printStats ();
	   break;
      case 'C':
	   clearStats ();
	   fprintf (stderr, "Cleared the stats.\n");
	   break;
      default:
	   break;
    }
  }
  addScheduledEvent (1, 0, keyCheckCallBack);
}

int main (int argc, char *argv[])
{

/* 
 * int i; Key testKey;
 * 
 * initPotp();
 * 
 * initFromPass("Hello",&testKey);
 * 
 * for (i = 0;i < 6947;++i) { fprintf(stdout,"0x%02X, ",randByte(&testKey)); if (i % 20 == 0) fprintf(stdout,"\n"); }
 * fprintf(stdout,"\n");
 * 
 * fprintf(stderr,"three rand longs %08lX %08lX %08lX\n", randLong(&testKey), randLong(&testKey), randLong(&testKey));
 * 
 * exit(1);
 * 
 */

  if (argc != 1)
  {
    // This does not return.
    usage();
  }

  // Print out a start up message.
  fprintf (stdout, "Filter Version %s starting...\n", VERSION);

  // Initialize everything.
  //fprintf(stdout, "Initializing...\n");

  init();

  syslogMessage (SYSL_COLD_START);

  fprintf (stdout, "Beginning filtering");

  // fprintf(stdout,"farcore %lu\n",farcoreleft());

  // Do forever!
  for (;;)
  {
    //fprintf(stderr,"%d\n",argh);

    //fprintf(stderr,"spin");

    // Forward packets.
    checkCards();

    // Check for management packets.
    checkIp();

    // Check miscellaneous things.
    checkMisc();
  }
}
