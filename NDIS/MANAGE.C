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

// MANAGE.C
//
// The routines in this file implement the protocol that fm uses
//   to communicate with the filter.
//
// NOTE:
//
// When we allocate network tables, we always allocate the size
//   of the table + 16 and then adjust the pointer to the first
//   paragraph in the table. The reason we do this is that Class
//   B tables are 64K large. Using malloc causes an extra 4 bytes
//   to be added on to the front of the block. Thus we are trying
//   to work with a 64K + 4B structure which you can't do with
//   far pointers (the last four bytes in the host table would
//   wrap around to the beginning and try to use malloc's information.)
//   We don't want the overhead of huge pointers so we overallocate
//   by a paragraph and then shift to the first paragraph boundary
//   in the block. We then end up with a pure pointer to a 64K block
//   which will not wrap. (Don't you just love DOS and Intel?)
//
#include "db.h"

BYTE password[PASSWORD_LENGTH];
int passwordLoaded = NO;

QueryPacket queryPacket;
StatisticsPacket statisticsPacket;

int sessionKeyValid = NO;
Key sessionKey;

// If a reboot command comes in, this variable will be set and
//   a reboot will occur 2 seconds later.
BYTE rebootRequested = NO;

// Big enough to hande the biggest packet we might ever see.
BYTE recvPacketBuffer[5120];
BYTE sendPacketBuffer[5120];

IoVec bufVector[10];

// Note this check sum adds a last odd byte as a short with the
//    byte high. This is a checksum usable for either an IP or UDP
//    header checksum.
//
// We have support in here for chaining. We will take a previous checksum,
//    undo the complement and then continue the sum with the specified
//    buffer.
// WARNING: This checksum is byte swapped!
WORD chkSum (BYTE * buf, WORD length, WORD * prevSum)
{
  int i;
  WORD *curr;
  DWORD sum;

  // If a previous sum was specified then use it to keep chaining.
  if (prevSum == NULL)
    sum = 0;
  else
    sum = ~*prevSum;

  curr = (WORD *) buf;

  for (i = 0; i < length / 2; ++i)
    sum += *curr++;

  if (length % 2 == 1)
    sum += ((WORD) * (BYTE *) curr) << 8;

  sum = (sum & 0xFFFF) + ((sum & 0xFFFF0000) >> 16);
  sum = (sum & 0xFFFF) + ((sum & 0xFFFF0000) >> 16);

  return (WORD) ~ sum;
}

// Save the password off to disk.
static int writePassword (BYTE * thePassword)
{
  FILE *passwordFile;

  //fprintf(stdout,"installing password\n");

  // Open the password file.
  passwordFile = fopen (PASSWORD_FILE, "w");

  if (passwordFile == NULL)
  {
    fprintf (stdout, "Could not open file errno = %d\n", errno);

    return -1;
  }

  if (fprintf (passwordFile, "%s\n", thePassword) == EOF)
  {
    fprintf (stdout, "Error writing out password\n");

    return -2;
  }
  // fprintf(stdout,"saved password\n");

  fclose (passwordFile);

  return 0;
}

static int readPassword (void)
{
  FILE *passwordFile;

  //fprintf(stdout,"reading password\n");

  // Open the password file.
  passwordFile = fopen (PASSWORD_FILE, "r");

  if (passwordFile == NULL)
  {
    //fprintf(stdout,"Could not open file errno = %d\n",errno);
    fprintf (stdout, "PASSWORD file does not exist; starting in insecure mode\n");

    return -1;
  }

  if (fscanf (passwordFile, "%s\n", password) <= 0)
  {
    fprintf (stdout, "Error reading in password\n");

    return -2;
  }

  //fprintf(stdout,"read password = %s\n",password);

  fclose (passwordFile);

  passwordLoaded = YES;

  return 0;
}

void deliverPacket (Socket * to, BYTE type, void *fmPacket, int length)
{
  FilterHeader header;

  //fprintf(stderr,"sending back packet\n");

  // Build the header.
  header.type = type;
  header.flags = passwordLoaded;
  header.randomInject = rand ();
  header.chkSum = 0;
  header.chkSum = chkSum ((BYTE *) & header.chkSum, sizeof (header.chkSum) + sizeof (header.randomInject), NULL);
  header.chkSum = chkSum (fmPacket, length, &header.chkSum);

  //fprintf(stderr,"random inject = %04X\n",header.randomInject);

  bufVector[0].buffer = (BYTE *) & header;
  bufVector[0].length = sizeof (FilterHeader);

  // If the password is loaded and a session key valid then encrypt the fm packet.
  //
  // Note that ERROR messages are never encrypted.
  if (sessionKeyValid == YES && type != FM_M_ERROR)
  {
    // Note that eventually a CRC calculation should be used here instead of a
    //   checksum.

    // Encrypt the check sum and random injection in place.
    encrypt ((BYTE *) & header.chkSum,
	     (BYTE *) & header.chkSum,
	     &sessionKey,
	     sizeof (header.chkSum) + sizeof (header.randomInject));

    //fprintf(stderr,"encrypting size %d\n",
    //      length + sizeof(header.chkSum) + sizeof(header.randomInject));

    // Note that at this point we could probably speed things up by doing an in place
    //   encryption but then this routine now has a nasty side effect. I don't want
    //   code above this routine to have to worry about it.
    encrypt (fmPacket, sendPacketBuffer, &sessionKey, length);

    bufVector[1].buffer = sendPacketBuffer;
    bufVector[1].length = length;
  }
  else
  {
    // If we don't need to encrypt then don't do the copy.
    bufVector[1].buffer = fmPacket;
    bufVector[1].length = length;
  }

  // Deliver the packet.
  sendvUdp (bufVector, 2, to);
}

// Handle a SYNC message.
//
// Note that if we got to here then we are for sure in encyption mode.
void handleSync (SyncPacket * inSync, int length, Socket * from)
{
  SyncPacket outSync;
  Key myKey;
  Key theirKey;

  //fprintf(stderr,"received SYNC message\n");

  // First off, get a key and return it.
  clientInit (password, &myKey, &outSync);

  deliverPacket (from, FM_M_SYNCACK, (void *) &outSync, sizeof (SyncPacket));

  // Ok, get the other guy's key.
  serverInit (password, &theirKey, inSync);

  // Ok, now we have a session key.
  buildNewSessionKey (&myKey, &theirKey, &sessionKey);

  sessionKeyValid = YES;
}

static void reboot (ScheduledEvent * event)
{
  // Jump to the warm start vector.
  ((void (*)(void)) MK_FP (0xF000, 0xFFF0)) ();
}

void handleReboot (void *packet, int length, Socket * from)
{
  // fprintf(stdout,"reboot requested\n");

  // Send back a REBOOTACK packet.
  deliverPacket (from, FM_M_REBOOTACK, (void *) NULL, 0);

  // Set the reboot flag. This prevents us from responding to any more management packets.
  rebootRequested = YES;

  // Register a callback two seconds from now. This should allow enough time to send out the ack. If
  //    not, tough.
  addScheduledEvent (2, 0, reboot);
}

void handleNewkey (BYTE * newKey, int length, Socket * from)
{
  ErrorPacket error;
  BYTE tempPassword[PASSWORD_LENGTH];
  int result;

  // Get the key and save it. We assume that the key may not be NULL terminated.
  strncpy (tempPassword, newKey, PASSWORD_LENGTH < length ? PASSWORD_LENGTH : length);

  // Save the password to disk.
  result = writePassword (tempPassword);

  switch (result)
     {
       case -1:
	 // Couldn't open the file. Set the appropriate error code.
	 error.errorCode = FM_ERROR_PASSFILE;

	 // Send back an error packet.
	 deliverPacket (from, FM_M_ERROR, (void *) &error, sizeof (ErrorPacket));
	 break;

       case -2:
	 // Couldn't write to the file. Set the appropriate error code.
	 error.errorCode = FM_ERROR_PASSWRITE;

	 // Send back an error packet.
	 deliverPacket (from, FM_M_ERROR, (void *) &error, sizeof (ErrorPacket));
	 break;

       case 0:
	 // The install succeeded. Send back an ack and enable
	 // the key.
	 deliverPacket (from, FM_M_NEWKEYACK, (void *) NULL, 0);

	 strncpy (password, tempPassword, PASSWORD_LENGTH < length ? PASSWORD_LENGTH : length);
	 passwordLoaded = YES;

	 break;
       default:
	 // fprintf(stdout,"oops, bad return code from installDesKey\n");
	 break;
     }
}

void handleQuery (QueryPacket * query, int length, Socket * from)
{
  int i;
  int j;
  int index;
  in_addr network;
  ErrorPacket error;

  //fprintf(stdout,"received QUERY message\n");

  // fprintf(stdout,"type = %d\n",query->type);

  // Zero out the query reply packet.
  memset ((BYTE *) & queryPacket, 0, sizeof (QueryPacket));

  switch (query->type)
     {

       case FM_QUERY_NETWORK:
	 // Send back a list of all the currently loaded network tables.

	 // fprintf(stdout,"network query\n");

	 // Build the table of networks for the manager.
	 queryPacket.type = FM_QUERY_NETWORK;

	 for (i = 0, j = 0; i < MAX_NUM_NETWORKS; ++i)
	 {
	   if (addrTable[i].network.S_addr != 0L)
	   {
	     queryPacket.queryResult.networks[j] =
	       addrTable[i].network;
	     ++j;
	   }
	 }

	 // Send back the answer.
	 deliverPacket (from, FM_M_QUERYACK, (void *) &queryPacket, sizeof (QueryPacket));
	 break;

       case FM_QUERY_HOST:
	 // Get the network and host from the address.
	 if (IN_CLASSB (query->queryValue.addr.S_addr))
	 {
	   // Class B address.
	   network.S_addr = query->queryValue.addr.S_addr & CLASSB_NETWORK;
	 }
	 else if (IN_CLASSC (query->queryValue.addr.S_addr))
	 {
	   // Class C address.
	   network.S_addr = query->queryValue.addr.S_addr & CLASSC_NETWORK;
	 }
	 else
	 {
	   // We don't handle class A or D addresses. Send
	   // back an error packet.
	   error.errorCode = FM_ERROR_NONETWORK;

	   // Send back an error packet.
	   deliverPacket (from, FM_M_ERROR, (void *) &error, sizeof (ErrorPacket));
	   break;
	 }

	 for (i = 0; i < MAX_NUM_NETWORKS; ++i)
	   if (addrTable[i].network.S_addr == network.S_addr)
	     break;

	 if (i == MAX_NUM_NETWORKS)
	 {
	   // Couldn't find the proper network. Send
	   // back an error packet.
	   error.errorCode = FM_ERROR_NONETWORK;

	   // Send back an error packet.
	   deliverPacket (from, FM_M_ERROR, (void *) &error, sizeof (ErrorPacket));
	 }
	 else
	 {
	   // Build the answer to the query.
	   queryPacket.type = FM_QUERY_HOST;
	   queryPacket.queryResult.index = networkCacheLookup (query->queryValue.addr);

	   // Send back the answer.
	   deliverPacket (from, FM_M_QUERYACK, (void *) &queryPacket, sizeof (QueryPacket));
	 }

	 break;

       case FM_QUERY_REJECT:
	 // fprintf(stdout,"reject query\n");

	 // Build the class tables for the manager.
	 queryPacket.type = FM_QUERY_REJECT;

	 memcpy ((BYTE *) queryPacket.queryResult.reject,
		 (BYTE *) rejectTable,
		 sizeof (RejectTableEntry) * MAX_NUM_REJECT_ENTRIES);

	 // Send back the answer.
	 deliverPacket (from, FM_M_QUERYACK, (void *) &queryPacket, sizeof (QueryPacket));
	 break;

       case FM_QUERY_ALLOW:
	 // fprintf(stdout,"allow query\n");

	 // Build the class tables for the manager.
	 queryPacket.type = FM_QUERY_ALLOW;

	 memcpy ((BYTE *) queryPacket.queryResult.allow,
		 (BYTE *) allowTable,
		 sizeof (AllowTableEntry) * MAX_NUM_ALLOW_ENTRIES);

	 // Send back the answer.
	 deliverPacket (from, FM_M_QUERYACK, (void *) &queryPacket, sizeof (QueryPacket));
	 break;

       case FM_QUERY_CLASS:
	 // fprintf(stdout,"class query\n");

	 // Get the index of the class being asked for.
	 index = query->queryValue.index;

	 // fprintf(stdout,"index = %d\n",index);

	 // Build the class tables for the manager.
	 queryPacket.type = FM_QUERY_CLASS;

	 // Copy the class to the packet.
	 memcpy ((BYTE *) queryPacket.queryResult.accessList.in,
		 (BYTE *) (in + index * MAX_NUM_ACCESS_RANGES),
		 sizeof (AccessListTableEntry) * MAX_NUM_ACCESS_RANGES);
	 memcpy ((BYTE *) queryPacket.queryResult.accessList.out,
		 (BYTE *) (out + index * MAX_NUM_ACCESS_RANGES),
		 sizeof (AccessListTableEntry) * MAX_NUM_ACCESS_RANGES);
	 memcpy ((BYTE *) queryPacket.queryResult.accessList.src,
		 (BYTE *) (source + index * MAX_NUM_ACCESS_RANGES),
		 sizeof (AccessListTableEntry) * MAX_NUM_ACCESS_RANGES);
	 memcpy ((BYTE *) queryPacket.queryResult.accessList.udp,
		 (BYTE *) (udp + index * MAX_NUM_ACCESS_RANGES),
		 sizeof (AccessListTableEntry) * MAX_NUM_ACCESS_RANGES);

	 // Send back the answer.
	 deliverPacket (from, FM_M_QUERYACK, (void *) &queryPacket, sizeof (QueryPacket));

	 break;

       default:
	 // Bad query. Send back a gripe.
	 error.errorCode = FM_ERROR_COMMAND;

	 // Send back an error packet.
	 deliverPacket (from, FM_M_ERROR, (void *) &error, sizeof (ErrorPacket));
	 break;
     }
}

void handleLoad (LoadPacket * load, int length, Socket * from)
{
  int i;
  int index;
  UINT curr;
  DWORD size;
  DWORD offset;
  DWORD hash;
  in_addr network;
  ErrorPacket error;

  // fprintf(stdout,"received load command\n");

  // XXX Make sure the packet is of a reasonable length.

  switch (load->type)
     {

       case FM_LOAD_NETWORK:
	 // Get the network and offset.
	 network = load->loadValue.networkBlock.network;
	 offset = load->loadValue.networkBlock.offset;

	 // Determine the size of the host table.
	 if (IN_CLASSB (network.S_addr))
	   size = 0x10000UL;
	 else if (IN_CLASSC (network.S_addr))
	   size = 0x100UL;
	 else
	 {
	   // We do not support A or D networks. Gripe.
	   error.errorCode = FM_ERROR_NONETWORK;

	   // Send back an error packet.
	   deliverPacket (from, FM_M_ERROR, (void *) &error, sizeof (ErrorPacket));

	   break;
	 }

	 // fprintf(stdout,"network = %08lX offset = %ld\n",
	 // network,offset);

	 // Find the network in the new networks array.
	 for (i = 0; i < MAX_NUM_NEW_NETWORKS; ++i)
	   if (newAddrTable[i].network.S_addr ==
	       network.S_addr)
	     break;

	 if (load->flags & FM_LOAD_FLAGS_BEGIN)
	 {

	   // fprintf(stdout,"BEGIN\n");

	   if (i == MAX_NUM_NEW_NETWORKS)
	   {
	     // We didn't find the network in the new network
	     // list so find an empty slot to insert it in.
	     for (i = 0; i < MAX_NUM_NEW_NETWORKS; ++i)
	       if (newAddrTable[i].network.S_addr ==
		   0L)
		 break;

	     // If there is no room in the table for a
	     // new entry then send back an error message.
	     if (i == MAX_NUM_NEW_NETWORKS)
	     {
	       error.errorCode = FM_ERROR_NOMEMORY;

	       // Send back an error packet.
	       deliverPacket (from, FM_M_ERROR, (void *) &error, sizeof (ErrorPacket));
	       break;
	     }
	     // Install the network into the new network array.
	     newAddrTable[i].network = network;
	   }

	   // If a buffer has not been allocated for the entry
	   // then allocate one and clear it.
	   if (newAddrTable[i].hostTable != 0)
	   {

	     // fprintf(stdout,"deleting old table\n");
	     xmsFreeMem (newAddrTable[i].hostTable);
	   }

	   newAddrTable[i].hostTable = xmsAllocMem (size);

	   if (newAddrTable[i].hostTable == 0)
	   {

	     error.errorCode = FM_ERROR_NOMEMORY;

	     // Return an out of memory error.
	     deliverPacket (from, FM_M_ERROR, (void *) &error, sizeof (ErrorPacket));

	     // Uninstall the network in the new network array.
	     newAddrTable[i].network.S_addr = 0L;
	     break;
	   }
	 }
	 // Transfer the data to the new network buffer.
	 if (i == MAX_NUM_NEW_NETWORKS)
	 {
	   // The network is not defined!
	   error.errorCode = FM_ERROR_NONETWORK;

	   // Send back an error packet.
	   deliverPacket (from, FM_M_ERROR, (void *) &error, sizeof (ErrorPacket));
	   break;
	 }

	 // Copy the data to the table.
	 xmsCopy (newAddrTable[i].hostTable, offset, 0, (DWORD) load->loadData.networkBlock,
		  (size == 0x100UL ? 0x100UL : 1024UL) >> 1);

	 if (load->flags & FM_LOAD_FLAGS_END)
	 {
	   // Get the network and host from the address.
	   // Install the network into the address hash
	   // table and delete the entry in the new networks
	   // array.

	   // Hash into the table and see if the network has been defined.
	   hash = (network.S_addr & NETWORK_HASH_MASK) >> 19;
	   curr = (UINT) hash;

	   while (addrTable[curr].network.S_addr != network.S_addr &&
		  addrTable[curr].network.S_addr != 0)
	   {

	     curr = (curr + 1) & (MAX_NUM_NETWORKS - 1);

	     if (curr == hash)
	     {
	       // Network hash table is full.
	       error.errorCode = FM_ERROR_NONETWORK;

	       // Send back an error packet.
	       deliverPacket (from, FM_M_ERROR, (void *) &error, sizeof (ErrorPacket));

	       // Release the memory that was allocated during the
	       // load. Probably should check for this case before
	       // we allow the load to begin.
	       xmsFreeMem (newAddrTable[i].hostTable);
	       newAddrTable[i].hostTable = 0;
	       newAddrTable[i].network.S_addr = 0UL;

	       break;
	     }
	   }

	   // fprintf(stdout,"inserted network at %d\n",curr);

	   // Set the address for the table.
	   addrTable[curr].network.S_addr = network.S_addr;

	   // Install the network and mark it as dirty (not saved).
	   addrTable[curr].dirty = YES;

	   // Free up the previous table if it existed. (This should not
	   // happen since if we are inserting here the entry should
	   // already be free()'d.)
	   if (addrTable[curr].hostTable)
	     xmsFreeMem (addrTable[curr].hostTable);

	   addrTable[curr].hostTable = newAddrTable[i].hostTable;
	   newAddrTable[i].hostTable = 0;
	   newAddrTable[i].network.S_addr = 0UL;

	   // Flush the network cache.
	   networkCacheFlush ();
	 }
	 // Send back a LOADACK packet.
	 deliverPacket (from, FM_M_LOADACK, (void *) NULL, 0);

	 break;
       case FM_LOAD_REJECT:
	 // Load in the new reject table.
	 memcpy (rejectTable,
		 load->loadData.reject,
		 sizeof (rejectTable));

	 // Mark the table as dirty.
	 rejectTableDirty = YES;

	 // Send back a LOADACK packet.
	 deliverPacket (from, FM_M_LOADACK, (void *) NULL, 0);

	 break;

       case FM_LOAD_ALLOW:
	 // Load in the new allow table.
	 memcpy (allowTable,
		 load->loadData.allow,
		 sizeof (allowTable));

	 // Mark the table as dirty.
	 allowTableDirty = YES;

	 // Send back a LOADACK packet.
	 deliverPacket (from, FM_M_LOADACK, (void *) NULL, 0);
	 break;

       case FM_LOAD_CLASS:
	 // fprintf(stdout,"loading class\n");

	 // Get the network and offset.
	 index = load->loadValue.index;

	 // fprintf(stdout,"index = %d\n",index);
	 // fprintf(stdout,"load = %08X\n",load);
	 // fprintf(stdout,"index = %08X\n",&load->loadValue.index);

	 if (load->flags & FM_LOAD_FLAGS_BEGIN)
	 {
	   // fprintf(stdout,"begin\n");

	   if (newIn == 0)
	     newIn = xmsAllocMem (MAX_NUM_ACCESS_LISTS *
				  MAX_NUM_ACCESS_RANGES *
				  sizeof (AccessListTableEntry));

	   if (newOut == 0)
	     newOut = xmsAllocMem (MAX_NUM_ACCESS_LISTS *
				   MAX_NUM_ACCESS_RANGES *
				   sizeof (AccessListTableEntry));

	   if (newSource == 0)
	     newSource = xmsAllocMem (MAX_NUM_ACCESS_LISTS *
				      MAX_NUM_ACCESS_RANGES *
				      sizeof (AccessListTableEntry));

	   if (newUdp == 0)
	     newUdp = xmsAllocMem (MAX_NUM_ACCESS_LISTS *
				   MAX_NUM_ACCESS_RANGES *
				   sizeof (AccessListTableEntry));

	   if (newIn == 0 || newOut == 0 || newSource == 0 || newUdp == 0)
	   {
	     // The network is not defined!
	     error.errorCode = FM_ERROR_NOMEMORY;

	     // Send back an error packet.
	     deliverPacket (from, FM_M_ERROR, (void *) &error, sizeof (ErrorPacket));
	     break;
	   }
	 }

	 xmsCopy (newIn, sizeof (AccessListTableEntry) * index * MAX_NUM_ACCESS_RANGES,
		  0, (DWORD) load->loadData.accessList.in,
		  (sizeof (AccessListTableEntry) * MAX_NUM_ACCESS_RANGES) >> 1);

	 xmsCopy (newOut, sizeof (AccessListTableEntry) * index * MAX_NUM_ACCESS_RANGES,
		  0, (DWORD) load->loadData.accessList.out,
		  (sizeof (AccessListTableEntry) * MAX_NUM_ACCESS_RANGES) >> 1);

	 xmsCopy (newSource, sizeof (AccessListTableEntry) * index * MAX_NUM_ACCESS_RANGES,
		  0, (DWORD) load->loadData.accessList.src,
		  (sizeof (AccessListTableEntry) * MAX_NUM_ACCESS_RANGES) >> 1);

	 xmsCopy (newUdp, sizeof (AccessListTableEntry) * index * MAX_NUM_ACCESS_RANGES,
		  0, (DWORD) load->loadData.accessList.udp,
		  (sizeof (AccessListTableEntry) * MAX_NUM_ACCESS_RANGES) >> 1);

	 // Copy the access list.
	 /*
	  * memcpy(newIn + index * MAX_NUM_ACCESS_RANGES,
	  * load->loadData.accessList.in,
	  * sizeof(AccessListTableEntry) * MAX_NUM_ACCESS_RANGES);
	  * 
	  * memcpy(newOut + index * MAX_NUM_ACCESS_RANGES,
	  * load->loadData.accessList.out,
	  * sizeof(AccessListTableEntry) * MAX_NUM_ACCESS_RANGES);
	  * 
	  * memcpy(newSource + index * MAX_NUM_ACCESS_RANGES,
	  * load->loadData.accessList.src,
	  * sizeof(AccessListTableEntry) * MAX_NUM_ACCESS_RANGES);
	  * 
	  * memcpy(newUdp + index * MAX_NUM_ACCESS_RANGES,
	  * load->loadData.accessList.udp,
	  * sizeof(AccessListTableEntry) * MAX_NUM_ACCESS_RANGES);
	  */

	 // Install the loaded table.
	 if (load->flags & FM_LOAD_FLAGS_END)
	 {
	   // fprintf(stdout,"end\n");

	   // The tables will have always been allocated so we don't
	   // need to check the pointers.
	   xmsCopy (0, (DWORD) in,
		    newIn, 0,
		    (sizeof (AccessListTableEntry) * MAX_NUM_ACCESS_LISTS * MAX_NUM_ACCESS_RANGES) >> 1);
	   xmsFreeMem (newIn);
	   newIn = 0;

	   xmsCopy (0, (DWORD) out,
		    newOut, 0,
		    (sizeof (AccessListTableEntry) * MAX_NUM_ACCESS_LISTS * MAX_NUM_ACCESS_RANGES) >> 1);
	   xmsFreeMem (newOut);
	   newOut = 0;

	   xmsCopy (0, (DWORD) source,
		    newSource, 0,
		    (sizeof (AccessListTableEntry) * MAX_NUM_ACCESS_LISTS * MAX_NUM_ACCESS_RANGES) >> 1);
	   xmsFreeMem (newSource);
	   newSource = 0;

	   xmsCopy (0, (DWORD) udp,
		    newUdp, 0,
		    (sizeof (AccessListTableEntry) * MAX_NUM_ACCESS_LISTS * MAX_NUM_ACCESS_RANGES) >> 1);
	   xmsFreeMem (newUdp);
	   newUdp = 0;

	   accessTableDirty = YES;
	 }
	 // Send back a LOADACK packet.
	 deliverPacket (from, FM_M_LOADACK, (void *) NULL, 0);
	 break;

       default:
	 // fprintf(stdout,"unknown load type %d\n",load->type);

	 // Bad load. Send back a gripe.
	 error.errorCode = FM_ERROR_COMMAND;

	 // Send back an error packet.
	 deliverPacket (from, FM_M_ERROR, (void *) &error, sizeof (ErrorPacket));
	 break;
     }
}

void handleWrite (void *packet, int length, Socket * from)
{
  int i;
  int fd;
  UINT writeAmount;
  DWORD size;
  DWORD currSize;
  char filename[15];
  ErrorPacket error;

  // fprintf(stdout,"write requested\n");

  if (rejectTableDirty == YES)
  {
    fd = open (REJECT_LIST_FILE,
	       O_WRONLY | O_BINARY | O_CREAT,
	       S_IREAD | S_IWRITE);

    if (fd != -1)
    {

      writeAmount = sizeof (RejectTableEntry) *
	MAX_NUM_REJECT_ENTRIES;

      if (write (fd, (char *) rejectTable, writeAmount) != writeAmount)
      {
	// Send back an error message.
	error.errorCode = FM_ERROR_DATAWRITE;

	// Send back an error packet.
	deliverPacket (from, FM_M_ERROR, (void *) &error, sizeof (ErrorPacket));

	close (fd);

	return;
      }
      rejectTableDirty = NO;
      close (fd);
    }
    else
    {
      // Could not open the data file. Gripe.
      error.errorCode = FM_ERROR_DATAFILE;

      // Send back an error packet.
      deliverPacket (from, FM_M_ERROR, (void *) &error, sizeof (ErrorPacket));
      return;
    }
  }
  if (allowTableDirty == YES)
  {

    fd = open (ALLOW_LIST_FILE,
	       O_WRONLY | O_BINARY | O_CREAT,
	       S_IREAD | S_IWRITE);

    if (fd != -1)
    {

      writeAmount = sizeof (AllowTableEntry) *
	MAX_NUM_ALLOW_ENTRIES;

      if (write (fd, (char *) allowTable, writeAmount) != writeAmount)
      {
	// Send back an error message.
	error.errorCode = FM_ERROR_DATAWRITE;

	// Send back an error packet.
	deliverPacket (from, FM_M_ERROR, (void *) &error, sizeof (ErrorPacket));

	close (fd);

	return;
      }
      allowTableDirty = NO;
      close (fd);
    }
    else
    {
      // Could not open the data file. Gripe.
      error.errorCode = FM_ERROR_DATAFILE;

      // Send back an error packet.
      deliverPacket (from, FM_M_ERROR, (void *) &error, sizeof (ErrorPacket));
      return;
    }
  }
  // fprintf(stdout,"working on access table\n");

  if (accessTableDirty == YES)
  {
    // Write out the access lists.
    fd = open (ACCESS_LIST_FILE,
	       O_WRONLY | O_BINARY | O_CREAT,
	       S_IREAD | S_IWRITE);

    if (fd != -1)
    {

      // fprintf(stdout,"wrote in part\n");

      writeAmount = sizeof (AccessListTableEntry) *
	MAX_NUM_ACCESS_LISTS * MAX_NUM_ACCESS_RANGES;

      if (write (fd, (char *) in, writeAmount) != writeAmount)
      {

	// Send back an error message.
	error.errorCode = FM_ERROR_DATAWRITE;

	// Send back an error packet.
	deliverPacket (from, FM_M_ERROR, (void *) &error, sizeof (ErrorPacket));

	close (fd);

	// fprintf(stdout,"wrote in\n");

	return;
      }
      if (write (fd, (char *) out, writeAmount) != writeAmount)
      {
	// Send back an error message.
	error.errorCode = FM_ERROR_DATAWRITE;

	// Send back an error packet.
	deliverPacket (from, FM_M_ERROR, (void *) &error, sizeof (ErrorPacket));

	close (fd);

	return;
      }
      if (write (fd, (char *) source, writeAmount) != writeAmount)
      {
	// Send back an error message.
	error.errorCode = FM_ERROR_DATAWRITE;

	// Send back an error packet.
	deliverPacket (from, FM_M_ERROR, (void *) &error, sizeof (ErrorPacket));

	close (fd);

	// fprintf(stdout,"write failed\n");

	return;
      }
      if (write (fd, (char *) udp, writeAmount) != writeAmount)
      {
	// Send back an error message.
	error.errorCode = FM_ERROR_DATAWRITE;

	// Send back an error packet.
	deliverPacket (from, FM_M_ERROR, (void *) &error, sizeof (ErrorPacket));

	close (fd);

	// fprintf(stdout,"write failed\n");

	return;
      }
      accessTableDirty = NO;
      close (fd);
    }
    else
    {
      // Could not open the data file. Gripe.
      error.errorCode = FM_ERROR_DATAFILE;

      // Send back an error packet.
      deliverPacket (from, FM_M_ERROR, (void *) &error, sizeof (ErrorPacket));
      return;
    }
  }
  // fprintf(stdout,"wrote access table\n");

  // Write out all of the dirty network tables.
  for (i = 0; i < MAX_NUM_NETWORKS; ++i)
  {

    if (addrTable[i].dirty == YES)
    {

      // Create the file name for the network.
      sprintf (filename, "%08lx.%s", addrTable[i].network.S_addr,
	       NETWORK_EXTENSION);

      // fprintf(stdout,"creating new network file %s\n",filename);

      fd = open (filename,
		 O_WRONLY | O_BINARY | O_CREAT,
		 S_IREAD | S_IWRITE);

      if (fd != -1)
      {

	// Get the right size.
	if (IN_CLASSB (addrTable[i].network.S_addr))
	  size = 0x10000UL;
	else if (IN_CLASSC (addrTable[i].network.S_addr))
	  size = 0x100UL;

	// Write the network first.
	if (write (fd, (void *) &addrTable[i].network,
		   sizeof (in_addr)) != sizeof (in_addr))
	{
	  // Send back an error message.
	  error.errorCode = FM_ERROR_DATAWRITE;

	  // Send back an error packet.
	  deliverPacket (from, FM_M_ERROR, (void *) &error, sizeof (ErrorPacket));

	  close (fd);

	  return;
	}
	// Since write() won't take longs.....
	currSize = size;
	while (currSize)
	{

	  writeAmount = (WORD) (currSize < NETWORK_TRANSFER_BUFFER_SIZE ?
				currSize : NETWORK_TRANSFER_BUFFER_SIZE);

	  // fprintf(stdout,"writeAmount = %d\n",writeAmount);

	  // Transfer the block down from XMS memory.
	  xmsCopy (0, (DWORD) networkTransferBuffer,
		   addrTable[i].hostTable, size - currSize,
		   writeAmount >> 1);

	  // Write the host table out. Be careful with the pointer
	  // math in calculating the read address.
	  if (write (fd, networkTransferBuffer, writeAmount) != writeAmount)
	  {

	    error.errorCode = FM_ERROR_DATAWRITE;

	    // Send back an error packet.
	    deliverPacket (from, FM_M_ERROR, (void *) &error, sizeof (ErrorPacket));

	    close (fd);

	    return;
	  }
	  currSize -= writeAmount;
	}

	addrTable[i].dirty = NO;
	close (fd);

	// fprintf(stdout,"closed file\n");
      }
      else
      {
	// Could not open the data file. Gripe.
	error.errorCode = FM_ERROR_DATAFILE;

	// Send back an error packet.
	deliverPacket (from, FM_M_ERROR, (void *) &error, sizeof (ErrorPacket));
	return;
      }
    }
  }

  // Send back a WRITEACK packet.
  deliverPacket (from, FM_M_WRITEACK, (void *) NULL, 0);

}

void handleRelease (ReleasePacket * release, int length, Socket * from)
{
  char filename[15];
  int i;
  UINT curr;
  DWORD hash;
  in_addr network;
  ErrorPacket error;

  // fprintf(stdout,"release requested\n");

  switch (release->type)
     {
       case FM_RELEASE_NETWORK:
	 network = release->network;

	 for (i = 0; i < MAX_NUM_NETWORKS; ++i)
	   if (addrTable[i].network.S_addr ==
	       network.S_addr)
	     break;

	 if (i != MAX_NUM_NETWORKS)
	 {

	   // First create the file name.
	   sprintf (filename, "%08lx.%s", addrTable[i].network.S_addr,
		    NETWORK_EXTENSION);

	   // Delete the file. Don't worry if it succeeded since the
	   // the network may still be dirty and the file not
	   // exist yet.
	   unlink (filename);

	   // Free the table.
	   xmsFreeMem (addrTable[i].hostTable);

	   // Delete the entry out of the hash table.
	   hash = (addrTable[i].network.S_addr & NETWORK_HASH_MASK) >> 19;
	   curr = (UINT) ((hash + 1) & (MAX_NUM_NETWORKS - 1));

	   // Shuffle up the hash table entries.
	   while (addrTable[curr].network.S_addr != 0)
	   {

	     if (((addrTable[curr].network.S_addr & NETWORK_HASH_MASK) >> 19) == hash)
	     {

	       // Shift it up.
	       addrTable[i] = addrTable[curr];
	       i = curr;
	     }
	     curr = (curr + 1) & (MAX_NUM_NETWORKS - 1);
	   }

	   // Clean out this last entry.
	   addrTable[i].network.S_addr = 0UL;
	   addrTable[i].hostTable = 0;
	   addrTable[i].dirty = NO;

	   // Flush the network cache.
	   networkCacheFlush ();
	 }
	 else
	 {
	   // Network could not be found.
	   error.errorCode = FM_ERROR_NONETWORK;

	   // Send back an error packet.
	   deliverPacket (from, FM_M_ERROR, (void *) &error, sizeof (ErrorPacket));
	   break;
	 }

	 // Send back an ack.
	 deliverPacket (from, FM_M_RELEASEACK, (void *) NULL, 0);

	 break;
       case FM_RELEASE_ALLOW:
	 // Clear out the allow table.
	 memset ((void *) allowTable, 0, sizeof (allowTable));

	 // Delete the file.
	 unlink (ALLOW_LIST_FILE);

	 allowTableDirty = NO;

	 deliverPacket (from, FM_M_RELEASEACK, (void *) NULL, 0);

	 break;
       case FM_RELEASE_REJECT:
	 // Clear out the reject table.
	 memset ((void *) rejectTable, 0, sizeof (rejectTable));

	 // Delete the file.
	 unlink (REJECT_LIST_FILE);

	 rejectTableDirty = NO;

	 deliverPacket (from, FM_M_RELEASE, (void *) NULL, 0);

	 break;
       case FM_RELEASE_CLASSES:

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

	 // Set up the default tables. These are all allow lists.
	 in[0].begin = 25;	// Mail

	 in[0].end = 25;
	 in[1].begin = 53;	// Name service

	 in[1].end = 53;
	 out[0].begin = 1;	// Everything

	 out[0].end = 0xFFFF;
	 source[0].begin = 20;	// FTP data connections

	 source[0].end = 20;
	 udp[0].begin = 0;	// Disallow TFTP (69) and Portmapper (111).

	 udp[0].end = 68;
	 udp[1].begin = 70;
	 udp[1].end = 110;
	 udp[2].begin = 112;
	 udp[2].end = 0xFFFF;

	 // Delete the file. (May fail if there was not anything
	 // loaded in the first place.)
	 unlink (ACCESS_LIST_FILE);

	 accessTableDirty = NO;

	 deliverPacket (from, FM_M_RELEASEACK, (void *) NULL, 0);
	 break;
       default:
	 // fprintf(stdout,"unknown message type\n");
	 // Bad release. Send back a gripe.
	 error.errorCode = FM_ERROR_COMMAND;

	 // Send back an error packet.
	 deliverPacket (from, FM_M_ERROR, (void *) &error, sizeof (ErrorPacket));
	 break;
     }

}

void handleStatistics (StatisticsPacket * packet, int length, Socket * from)
{
  ErrorPacket error;

  switch (packet->type)
     {
       case FM_STATISTICS_QUERY:
	 statisticsPacket.type = FM_STATISTICS_QUERY;
	 statisticsPacket.statistics[FM_STAT_DB_PACKETS_FILTERED_INSIDE] = theStats.insideFiltered;
	 statisticsPacket.statistics[FM_STAT_DB_PACKETS_FILTERED_OUTSIDE] = theStats.outsideFiltered;
	 statisticsPacket.statistics[FM_STAT_DB_PACKETS_RX_INSIDE] = theStats.insideRx;
	 statisticsPacket.statistics[FM_STAT_DB_PACKETS_RX_OUTSIDE] = theStats.outsideRx;
	 statisticsPacket.statistics[FM_STAT_DB_PACKETS_TX_INSIDE] = theStats.insideTx;
	 statisticsPacket.statistics[FM_STAT_DB_PACKETS_TX_OUTSIDE] = theStats.outsideTx;
	 statisticsPacket.statistics[FM_STAT_DB_CACHE_ACCESSES] = theStats.cacheAccesses;
	 statisticsPacket.statistics[FM_STAT_DB_CACHE_MISSES] = theStats.cacheMisses;
	 statisticsPacket.statistics[FM_STAT_DB_DROPPED_PACKETS] = theStats.droppedPackets;

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

	 statisticsPacket.statistics[FM_STAT_CARD_FRAMES_RX_INSIDE] = MAC_STATUS (campus)->totalFramesRx;
	 statisticsPacket.statistics[FM_STAT_CARD_FRAMES_RX_OUTSIDE] = MAC_STATUS (internet)->totalFramesRx;
	 statisticsPacket.statistics[FM_STAT_CARD_BYTES_RX_INSIDE] = MAC_STATUS (campus)->totalBytesRx;
	 statisticsPacket.statistics[FM_STAT_CARD_BYTES_RX_OUTSIDE] = MAC_STATUS (internet)->totalBytesRx;
	 statisticsPacket.statistics[FM_STAT_CARD_MULTICAST_RX_INSIDE] = MAC_STATUS (campus)->totalMulticastRx;
	 statisticsPacket.statistics[FM_STAT_CARD_MULTICAST_RX_OUTSIDE] = MAC_STATUS (internet)->totalMulticastRx;
	 statisticsPacket.statistics[FM_STAT_CARD_BROADCAST_RX_INSIDE] = MAC_STATUS (campus)->totalBroadcastRx;
	 statisticsPacket.statistics[FM_STAT_CARD_BROADCAST_RX_OUTSIDE] = MAC_STATUS (internet)->totalBroadcastRx;
	 statisticsPacket.statistics[FM_STAT_CARD_CRC_RX_INSIDE] = MAC_STATUS (campus)->totalFramesCrc;
	 statisticsPacket.statistics[FM_STAT_CARD_CRC_RX_OUTSIDE] = MAC_STATUS (internet)->totalFramesCrc;
	 statisticsPacket.statistics[FM_STAT_CARD_BUFFER_DROPS_RX_INSIDE] = MAC_STATUS (campus)->totalFramesDiscardedBufferSpaceRx;
	 statisticsPacket.statistics[FM_STAT_CARD_BUFFER_DROPS_RX_OUTSIDE] = MAC_STATUS (internet)->totalFramesDiscardedBufferSpaceRx;
	 statisticsPacket.statistics[FM_STAT_CARD_HARDWARE_DROPS_RX_INSIDE] = MAC_STATUS (campus)->totalFramesDiscardedHardwareErrorRx;
	 statisticsPacket.statistics[FM_STAT_CARD_HARDWARE_DROPS_RX_OUTSIDE] = MAC_STATUS (internet)->totalFramesDiscardedHardwareErrorRx;
	 statisticsPacket.statistics[FM_STAT_CARD_FRAMES_TX_INSIDE] = MAC_STATUS (campus)->totalFramesTx;
	 statisticsPacket.statistics[FM_STAT_CARD_FRAMES_TX_OUTSIDE] = MAC_STATUS (internet)->totalFramesTx;
	 statisticsPacket.statistics[FM_STAT_CARD_BYTES_TX_INSIDE] = MAC_STATUS (campus)->totalBytesTx;
	 statisticsPacket.statistics[FM_STAT_CARD_BYTES_TX_OUTSIDE] = MAC_STATUS (internet)->totalBytesTx;
	 statisticsPacket.statistics[FM_STAT_CARD_MULTICAST_TX_INSIDE] = MAC_STATUS (campus)->totalMulticastTx;
	 statisticsPacket.statistics[FM_STAT_CARD_MULTICAST_TX_OUTSIDE] = MAC_STATUS (internet)->totalMulticastTx;
	 statisticsPacket.statistics[FM_STAT_CARD_BROADCAST_TX_INSIDE] = MAC_STATUS (campus)->totalBroadcastTx;
	 statisticsPacket.statistics[FM_STAT_CARD_BROADCAST_TX_OUTSIDE] = MAC_STATUS (internet)->totalBroadcastTx;
	 statisticsPacket.statistics[FM_STAT_CARD_TIMEOUT_DROPS_TX_INSIDE] = MAC_STATUS (campus)->totalFramesDiscardedTimeoutTx;
	 statisticsPacket.statistics[FM_STAT_CARD_TIMEOUT_DROPS_TX_OUTSIDE] = MAC_STATUS (internet)->totalFramesDiscardedTimeoutTx;
	 statisticsPacket.statistics[FM_STAT_CARD_HARDWARE_DROPS_TX_INSIDE] = MAC_STATUS (campus)->totalFramesDiscardedHardwareErrorTx;
	 statisticsPacket.statistics[FM_STAT_CARD_HARDWARE_DROPS_TX_OUTSIDE] = MAC_STATUS (internet)->totalFramesDiscardedHardwareErrorTx;

	 // This is the up time in seconds.
	 statisticsPacket.statistics[FM_STAT_UPTIME] = ((days * 0x1800B0UL + *(DWORD *) MK_FP (0x0040, 0x006C) - startTime) * 10) / 182;

	 break;
       case FM_STATISTICS_CLEAR:
	 statisticsPacket.type = FM_STATISTICS_CLEAR;
	 clearStats ();
	 break;
       default:
	 error.errorCode = FM_ERROR_COMMAND;

	 // Send back an error packet.
	 deliverPacket (from, FM_M_ERROR, (void *) &error, sizeof (ErrorPacket));
	 break;
     }

  deliverPacket (from, FM_M_STATISTICSACK, (void *) &statisticsPacket, sizeof (StatisticsPacket));
}

// Note that all of the code in this routine ignores byte ordering problems.
//   It assumes that the manager side will handle all of that for it.
//
// We have changed philosophies on the encryption in 2.0 since we are now
//   dealing with a stream cipher. We no longer use sequence numbers nor
//   do we handle retries. We also only do the sync once instead of at the
//   beginning of each command. The entire packet from the chkSum on is
//   encrypted now.
//
// There is also a check sum now included. This is used to detect an invalid
//   packet in the case of a dropped or spoofed packet. Note that if 
//   encryption is off, the chkSum is ignored.
//
void filtMessage (BYTE * packet, int length, Socket * from)
{
  FilterHeader *filtHead;
  ErrorPacket error;
  BYTE *fmPacket;
  WORD fmLength;

  //fprintf(stderr,"got a filter packet\n");

  // If a reboot has been requested then ignore everything.
  if (rebootRequested == YES)
    return;

  filtHead = (FilterHeader *) packet;

  //fprintf(stderr,"type = %d\n",filtHead->type);
  //fprintf(stderr,"flags = %d\n",filtHead->flags);
  //fprintf(stderr,"chksum = %d\n",filtHead->chkSum);
  //fprintf(stderr,"randomInject = %d\n",filtHead->randomInject);

  // Check the encryption mode of the manager. If it is not the
  //   same as ours, then return an error.
  if (filtHead->flags != passwordLoaded)
  {
    //fprintf(stderr,"wrong mode\n");

    // Set the appropriate error code.
    error.errorCode = passwordLoaded ? FM_ERROR_SECURE : FM_ERROR_INSECURE;

    // Send back an error packet.
    deliverPacket (from, FM_M_ERROR, (void *) &error, sizeof (ErrorPacket));
    return;
  }

  fmPacket = (((BYTE *) filtHead) + sizeof (FilterHeader));
  fmLength = length - sizeof (FilterHeader);

  // Decrypt the rest of the packet if we have loaded a password, the message is not
  //   a sync message and we have established a sessionKey.
  if (passwordLoaded == YES)
  {
    if (filtHead->type != FM_M_SYNC)
    {
      if (sessionKeyValid == YES)
      {
	/*
	 * fprintf(stderr,"decrypting size = %d\n",
	 * fmLength + sizeof(filtHead->chkSum) +
	 * sizeof(filtHead->randomInject));
	 * 
	 * {
	 * int k;
	 * for (k = 0;k < 20;++k) {
	 * fprintf(stderr,"%02X ",((BYTE *) &filtHead->chkSum)[k]);
	 * }
	 * fprintf(stderr,"\n");
	 * }
	 */

	// Decrypt the packet. Note that the chkSum and randomInject are included in
	//   this.
	decrypt ((BYTE *) & filtHead->chkSum,
		 (BYTE *) recvPacketBuffer,
		 &sessionKey,
		 fmLength + sizeof (filtHead->chkSum) + sizeof (filtHead->randomInject));

	/*
	 * {
	 * int k;
	 * for (k = 0;k < 20;++k) {
	 * fprintf(stderr,"%02X ",recvPacketBuffer[k]);
	 * }
	 * fprintf(stderr,"\n");
	 * }
	 * 
	 * fprintf(stderr,"chksum = %04X\n",*(WORD *) recvPacketBuffer);
	 * 
	 * fprintf(stderr,"calculated chkSum = %04X\n",
	 * chkSum(recvPacketBuffer,fmLength,NULL));
	 */

	if (chkSum (recvPacketBuffer,
		    fmLength + sizeof (filtHead->chkSum) + sizeof (filtHead->randomInject),
		    NULL) != 0)
	{
	  // Oops! The check sum didn't come out right. Either packets got out of
	  //   sequence, were dropped or were injected by the bad guys. Tell the manager to
	  //   start over.

	  //fprintf(stderr,"lost sync\n");
	  error.errorCode = FM_ERROR_LOSTSYNC;

	  sessionKeyValid = NO;

	  // Send back an error packet.
	  deliverPacket (from, FM_M_ERROR, (void *) &error, sizeof (ErrorPacket));
	  return;
	}

	// Else drop through and handle message.
	fmPacket = recvPacketBuffer + sizeof (filtHead->chkSum) + sizeof (filtHead->randomInject);
      }
      else
      {
	//fprintf(stderr,"session key not valid\n");

	// Send back an error indicating that we are operating in secure mode.
	error.errorCode = FM_ERROR_LOSTSYNC;

	// Send back an error packet.
	deliverPacket (from, FM_M_ERROR, (void *) &error, sizeof (ErrorPacket));
	return;
      }
    }
    // Else drop through and handle the SYNC message.
  }
  else
  {
    if (filtHead->type == FM_M_SYNC)
    {
      // Send back an error indicating that we are operating in insecure mode.
      error.errorCode = FM_ERROR_INSECURE;

      // Send back an error packet.
      deliverPacket (from, FM_M_ERROR, (void *) &error, sizeof (ErrorPacket));
      return;
    }
    // Else drop through and handle message.
  }

  //fprintf(stderr,"dispatching request\n");

  switch (filtHead->type)
     {
       case FM_M_SYNC:
	 handleSync ((void *) fmPacket, fmLength, from);
	 break;
       case FM_M_REBOOT:
	 handleReboot ((void *) fmPacket, fmLength, from);
	 break;
       case FM_M_NEWKEY:
	 handleNewkey ((void *) fmPacket, fmLength, from);
	 break;
       case FM_M_QUERY:
	 handleQuery ((void *) fmPacket, fmLength, from);
	 break;
       case FM_M_LOAD:
	 handleLoad ((void *) fmPacket, fmLength, from);
	 break;
       case FM_M_WRITE:
	 handleWrite ((void *) fmPacket, fmLength, from);
	 break;
       case FM_M_RELEASE:
	 handleRelease ((void *) fmPacket, fmLength, from);
	 break;
       case FM_M_STATISTICS:
	 handleStatistics ((void *) fmPacket, fmLength, from);
	 break;
       default:
	 // fprintf(stdout,"unknown message type\n");
	 // Silently eat anything else.
	 break;
     }
}

void initManage (void)
{
  int i;

  // If the management stuff has been configured then open the sockets for it.
  if (filterConfig.numManagers)
  {
    for (i = 0; i < filterConfig.numManagers; ++i)
    {
      // This will define UDP sockets that are not fixed on the remote port and callback
      //   to filtMessage().
      (void) socket (SOCKET_UDP,
		     filterConfig.listenPort,
		     &filterConfig.managers[i],
		     0,
		     0,
		     filtMessage);
    }
  }

  // Read 
  (void) readPassword ();
}
