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

void (*xmsEntry) (void);

// Allocate len number of kilobytes. Returns a EMB handle.
WORD xmsAllocMem (DWORD length)
{
  XmsRegs regs;

  //fprintf(stderr,"specified length = %ld\n",length);

  regs.ax = 0x0900;
  regs.dx = length / 1024 + ((length % 1024 == 0) ? 0 : 1);

  //fprintf(stderr,"allocing %d KB\n",regs.dx);
  xmsCall (&regs);

  return regs.dx;
}

// Free an EMB.
void xmsFreeMem (WORD handle)
{
  XmsRegs regs;

  regs.ax = 0x0A00;
  regs.dx = handle;
  xmsCall (&regs);
}

void xmsCopy (WORD toHandle, DWORD toOffset, WORD fromHandle,
	      DWORD fromOffset, DWORD numWords)
{
  XmsRegs regs;
  XmsMove move;

  move.length = numWords << 1;
  move.srcHandle = fromHandle;
  move.srcOffset = fromOffset;
  move.destHandle = toHandle;
  move.destOffset = toOffset;

  regs.ax = 0x0B00;
  regs.si = FP_OFF (&move);
  regs.ds = FP_SEG (&move);
  xmsCall (&regs);

  if (regs.ax != 1)
  {
    fprintf (stderr, "the XMS copy failed (%d:%d)\n", regs.ax, regs.bx & 0xFF);
    exit (1);
  }
}

WORD xmsQueryFree (void)
{
  XmsRegs regs;

  regs.ax = 0x0800;
  xmsCall (&regs);

  return regs.dx;
}

int getXmsEntry (void)
{
  union REGS regs;
  struct SREGS segRegs;

  regs.x.ax = 0x4300;

  int86 (0x2F, &regs, &regs);

  if (regs.h.al != 0x80)
  {
    // XMS not installed.
    return -1;
  }

  regs.x.ax = 0x4310;
  int86x (0x2F, &regs, &regs, &segRegs);
  xmsEntry = (void (*)(void)) MK_FP (segRegs.es, regs.x.bx);

  return 0;
}

void initXms (void)
{
  // Check for XMS and get the entry point.
  if (getXmsEntry ())
  {
    fprintf (stderr, "XMS not installed\n");
    exit (1);
  }
  fprintf (stderr, "%dKB of XMS memory free\n", xmsQueryFree ());
}
