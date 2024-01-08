
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

// ODI data structures. Note that the packet fragment fields need to be
//   filled in during initialization.

static int      freePktBufPtr;
static int     *freePktBufs;
static PktBuf **pktBufs;

PktBuf *pktBufFromHandle (int handle)
{
  return pktBufs[handle - 1];
}

void enqueuePktBuf (Queue * queue, PktBuf * pktBuf)
{
  PktBuf *last;
  BYTE atHead;

  GUARD

  last = queue->tail;

  // We are going to make the assumption that the packets are always delivered in
  //   order respective to the lookahead calls. I.E. we don't have to do any special
  //   processing to make sure the sequence numbers end up monotonically increasing
  //   in the queues. We just throw the ECB at the tail of the queue.
  if (last == NULL)
  {
    //sprintf(GET_DEBUG_STRING,"the queue was empty\n");

    // The queue is empty.
    queue->head = pktBuf;
    queue->tail = pktBuf;
    pktBuf->nextLink = NULL;
    pktBuf->prevLink = NULL;
  }
  else
  {
    // The queue is not empty.
    queue->tail = pktBuf;
    last->nextLink = pktBuf;
    pktBuf->prevLink = last;
    pktBuf->nextLink = NULL;
  }
  UNGUARD
}

// This routine is only ever called from the main thread.
PktBuf *dequeuePktBuf (Queue * queue, PktBuf * pktBuf)
{
  PktBuf *first;

  // Watch out for mutex problems from the interrupt threads.
  GUARD

  first = queue->head;

  if (queue->head != pktBuf)
     PERROR ("queue head did not match dequeue request.")

  if (first->nextLink == NULL)
  {
    //fprintf(stderr,"was only ecb on queue\n");
    // This is the only ECB on the queue.
    queue->head = NULL;
    queue->tail = NULL;
  }
  else
  {
    // There is more than one ECB on the queue.
    queue->head = pktBuf->nextLink;
    queue->head->prevLink = NULL;
  }

  // Restore interrupts.
  UNGUARD

  // No matter what, zero out these pointers to prevent misuse and detect bugs.
    pktBuf->nextLink = NULL;
  pktBuf->prevLink = NULL;

  return pktBuf;
}

// This routine is called from both threads.
void freePktBuf (PktBuf * pktBuf)
{
  GUARD

  if (freePktBufPtr == filterConfig.numBuffers - 1)
     PERROR ("tried to free too many packet buffers")
 
  freePktBufs[++freePktBufPtr] = pktBuf->handle - 1;

/* 
 * if (freePktBufPtr == 0)
 *    sprintf(GET_DEBUG_STRING,"PktBufs available again.\n");
 * putch('+');
 */

  UNGUARD
}

PktBuf *allocPktBufMgmt (void)
{
  PktBuf *pktBuf;

  GUARD

  if (freePktBufPtr < 0)
  {
    // Ran out of packet buffers. Return NULL which will start dropping packets.
    pktBuf = NULL;
  }
  else
  {
    pktBuf = pktBufs[freePktBufs[freePktBufPtr--]];
/* 
 * if (freePktBufPtr < 0)
 *    sprintf(GET_DEBUG_STRING,"PktBufs unavailable.\n");
 * putch('-');
 */
  }

  UNGUARD

  //sprintf(GET_DEBUG_STRING,"######### freePktBufPtr = %d\n",freePktBufPtr);

  return pktBuf;
}

// This routine is called from both threads.
PktBuf *allocPktBuf (void)
{
  PktBuf *pktBuf;

  GUARD

  if (freePktBufPtr < BUFFER_POOL_OVERHEAD)
  {
    // Ran out of packet buffers. Return NULL which will start dropping packets.
    pktBuf = NULL;
  }
  else
  {
    pktBuf = pktBufs[freePktBufs[freePktBufPtr--]];
/* 
 * if (freePktBufPtr < 0)
 *    sprintf(GET_DEBUG_STRING,"PktBufs unavailable.\n");
 *  putch('-');
 */
  }

  UNGUARD

  //sprintf(GET_DEBUG_STRING,"######### freePktBufPtr = %d\n",freePktBufPtr);

  return pktBuf;
}

void initQueue (void)
{
  int i;

  filterConfig.numBuffers += BUFFER_POOL_OVERHEAD;

  freePktBufs = (int *) farmalloc (sizeof (int) * filterConfig.numBuffers);

  pktBufs = (PktBuf **) farmalloc (sizeof (PktBuf *) * filterConfig.numBuffers);

  freePktBufPtr = filterConfig.numBuffers - 1;

  // Initialize the free list to contain the entire pool.
  for (i = 0; i < filterConfig.numBuffers; ++i)
  {
    freePktBufs[i] = i;

    // Allocate the packet buffer.
    pktBufs[i] = farmalloc (sizeof (PktBuf));

    if (pktBufs[i] == NULL)
    {
      fprintf (stderr, "could not allocate packet buffers\n");
      exit (1);
    }

    // Allocate the buffer itself.
    pktBufs[i]->buffer = farmalloc (frameSize);

    if (pktBufs[i]->buffer == NULL)
    {
      fprintf (stderr, "could not allocate packet buffers\n");
      exit (1);
    }

    pktBufs[i]->handle = i + 1;
    pktBufs[i]->length = frameSize;

    // Paranoia.
    memset (pktBufs[i]->buffer, 0xFF, frameSize);
  }
}
