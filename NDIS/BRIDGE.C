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

// Ethernet table for bridging.
HashEntry *hashTable = (HashEntry *) NULL;

// This only ever gets called from the main thread.
int bridgeLookUp (HardwareAddress * addr)
{
  int hash;
  int curr;

  // Check the hash table.
  hash = (addr->words[0] ^
	  addr->words[1] ^
	  addr->words[2]) & ADDRESS_HASH_MASK;

  curr = hash;

  GUARD

  while (hashTable[curr].address.words[0] != addr->words[0] ||
         hashTable[curr].address.words[1] != addr->words[1] ||
         hashTable[curr].address.words[2] != addr->words[2])
  {
    // Check to see if the entry is valid.
    if (hashTable[curr].valid == NO)
    {
      curr = -1;
      break;
    }
    curr = (curr + 1) & (MAX_NUM_ADDRESSES - 1);

    // Check if we have wrapped around.
    if (curr == hash)
    {
      curr = -1;
      break;
    }
  }

  // Note it is safe to reenable interrupts here since we never modify any existing
  //   entry in the bridge table.
  UNGUARD

  // If we found the address then we know what interface the
  //   address is on. If this packet came from that interface then
  //   don't forward the packet.
  if (curr != -1)
     return hashTable[curr].boardNumber;

  return -1;
}

// This is only called from the interrupt threads.
int bridge (WORD macId, BYTE * buffer)
{
  int hash;
  int curr;
  int pass;
  GenericHeader *headerAddrs;

  // Find the addresses in the packet based on the frametype. 
  switch (mediaType)
  {
    case MEDIA_FDDI:
	 headerAddrs = (GenericHeader *) (buffer + 1);
	 break;
    case MEDIA_ETHERNET:
	 headerAddrs = (GenericHeader *) buffer;
	 break;
    case MEDIA_TOKEN:
	 PERROR ("no support for token ring yet")
         break;
    default:
	 PERROR ("unknown media specified")
         break;
  }

  //if (timesAllowed == 0)
  //   return NO;

  //sprintf(GET_DEBUG_STRING,"dest = %02X:%02X:%02X:%02X:%02X:%02X src = %02X:%02X:%02X:%02X:%02X:%02X\n",
  //      headerAddrs->destHost.bytes[0],
  //      headerAddrs->destHost.bytes[1],
  //      headerAddrs->destHost.bytes[2],
  //      headerAddrs->destHost.bytes[3],
  //      headerAddrs->destHost.bytes[4],
  //      headerAddrs->destHost.bytes[5],
  //      headerAddrs->srcHost.bytes[0],
  //      headerAddrs->srcHost.bytes[1],
  //      headerAddrs->srcHost.bytes[2],
  //      headerAddrs->srcHost.bytes[3],
  //      headerAddrs->srcHost.bytes[4],
  //      headerAddrs->srcHost.bytes[5]);

  //sprintf(GET_DEBUG_STRING,"macId = %d\n",macId);

  pass = YES;

  // Check if the destination address is a multicast or broadcast
  //   address and always forward if so.
  if (!IS_BROADCAST (headerAddrs->destHost))
  { 
    // Check the hash table.
    hash = (headerAddrs->destHost.words[0] ^
	    headerAddrs->destHost.words[1] ^
	    headerAddrs->destHost.words[2]) & ADDRESS_HASH_MASK;

    curr = hash;

    while (hashTable[curr].address.words[0] != headerAddrs->destHost.words[0] ||
           hashTable[curr].address.words[1] != headerAddrs->destHost.words[1] ||
           hashTable[curr].address.words[2] != headerAddrs->destHost.words[2])
    {
      if (hashTable[curr].valid == NO)
      {
	curr = -1;
	break;
      }
      curr = (curr + 1) & (MAX_NUM_ADDRESSES - 1);

      // Check if we have wrapped around.
      if (curr == hash)
      {
	curr = -1;
	break;
      }
    }

    //if (curr != -1) {
    //      sprintf(GET_DEBUG_STRING,"found the destination address\n");
    //}

    // If we found the address then we know what interface the
    //   address is on. If this packet came from that interface then
    //   don't forward the packet.
    if (curr != -1 && macId == hashTable[curr].boardNumber)
      pass = NO;
  }

  // Note that this check is somewhat superfluous. But if some broken
  //   implementation sources a broadcast or multicast address we don't
  //   want to add them to the bridge table! 
  if (!IS_BROADCAST (headerAddrs->srcHost))
  {
    //sprintf(GET_DEBUG_STRING,"looking for the source address\n");

    // Check the source address. If not found then add into hash
    // table.
    hash = (headerAddrs->srcHost.words[0] ^
	    headerAddrs->srcHost.words[1] ^
	    headerAddrs->srcHost.words[2]) & ADDRESS_HASH_MASK;

    curr = hash;

    while (hashTable[curr].address.words[0] != headerAddrs->srcHost.words[0] ||
           hashTable[curr].address.words[1] != headerAddrs->srcHost.words[1] ||
           hashTable[curr].address.words[2] != headerAddrs->srcHost.words[2])
    {
      if (hashTable[curr].valid == NO)
      {
        // Wrap this due to reentrancy problems. We only suspend interrupts if
	//   they currently aren't. We don't want to mess with any assumptions
	//   made by the driver about interrupt state.
	GUARD

	//sprintf(GET_DEBUG_STRING,"Putting source address in table\n");

	// Insert the source hardware address in the table. Note that the boardNumber update
	//   is redundant. We do it here to reduce the size of the critical region.
        hashTable[curr].address.addr = headerAddrs->srcHost.addr;
        hashTable[curr].valid        = YES;
        hashTable[curr].boardNumber  = macId;

	UNGUARD
        break;
      }

      curr = (curr + 1) & (MAX_NUM_ADDRESSES - 1);

      // Check if we have wrapped around.
      if (curr == hash)
      {
	curr = -1;
	break;
      }
    }

    // Wrap this due to reentrancy problems.
    GUARD

    // If the address was entered OR found then update the interface.
    //   This is necessary if a device ever switches from being on
    //   one side of the filter to the other. Note that this ends up
    //   being redundant if the address was new but it allows us to make
    //   the critical region smaller.
    if (curr != -1)
       hashTable[curr].boardNumber = macId;

    // Note that interrupts may have been disabled in the while loop above and not just
    //   right above here.
    UNGUARD
  }
  return pass;
}

void initBridge (void)
{
  DWORD size;

  //fprintf(stderr,"table size = %ld\n",8192UL * sizeof(HashEntry) + 16);

  // Allocate an 8K entry table. Note that the way we are using this pointer we are assuming
  //   that the HashEntry structure is only 8 bytes long.
  hashTable = (HashEntry *) farmalloc (8192UL * sizeof (HashEntry) + 16);

  if (hashTable == NULL)
     PERROR ("could not allocate the bridge table")

  //fprintf(stderr,"table = %08lX\n",hashTable);

  // Shift the pointer down to the next paragraph so we have a true 64K table.
  hashTable = (HashEntry *) MK_FP (FP_SEG (hashTable) + 1, 0);

  //fprintf(stderr,"table = %08lX\n",hashTable);

  // Clean out the ethernet address table. Do it in two steps since
  //   the size argument is an unsigned and our data structure takes
  //   64K.
  size = 8192UL * sizeof (HashEntry) / 2;

  memset (hashTable, 0, (WORD) size);
  memset ((((BYTE *) hashTable) + size), 0, (WORD) size);
}
