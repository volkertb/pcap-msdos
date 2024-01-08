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

/* define the three feedback shift register polynomials */
#define MASK1 0x80000057UL
#define MASK2 0x80000062UL
#define MASK3 0x20000029UL

void clientInit (BYTE * pwd, Key * sessionKey, SyncPacket * sync)
{
  Key  tempKey;
  Key  randKey;
  Key  passKey;
  WORD spinCount;
  int  i;

  //fprintf(stderr,"address = %08lX\n",&tempKey);

  // Initialize the random number generator from the clock.
  initFromTime (&randKey);

  // Get random spinCount and sessionKey
  spinCount = randLong (&randKey) & 0xFFFF;
  spinCount /= 8;

  tempKey.i1 = randLong (&randKey);
  tempKey.i2 = randLong (&randKey);
  tempKey.i3 = randLong (&randKey);

  // Return tempKey as my half of session key.
  *sessionKey = tempKey;

  // Make a working copy of pass key
  initFromPass (pwd, &passKey);

  // Get k(r) to send across to server
  for (i = 0; i < spinCount; ++i)
      randByte (&passKey);

  tempKey.i1 ^= randLong (&passKey);
  tempKey.i2 ^= randLong (&passKey);
  tempKey.i3 ^= randLong (&passKey);

  sync->spinCount = swapWord (spinCount);
  sync->dummy = 0;

  sync->key.i1 = swapLong (tempKey.i1);
  sync->key.i2 = swapLong (tempKey.i2);
  sync->key.i3 = swapLong (tempKey.i3);

  //fprintf(stdout,"my encrypted key %04X %08lX %08lX %08lX\n",sync->spinCount,sync->key.i1,sync->key.i2,sync->key.i3);
}

void serverInit (BYTE * pwd, Key * sessionKey, SyncPacket * sync)
{
  Key  passKey;
  WORD spinCount;
  int  i;

  spinCount = swapWord (sync->spinCount);

  // Get info out of init message. note: compatible char set???
  //fprintf(stdout,"their encrypted key %04X %08lX %08lX %08lX\n",
  //      spinCount,
  //      swapLong(sync->key.i1),
  //      swapLong(sync->key.i2),
  //      swapLong(sync->key.i3));

  // Decrypt random session key
  initFromPass (pwd, &passKey);


  //fprintf(stdout,"result of initFromPass %08lX %08lX %08lX\n",
  //      passKey.i1,
  //      passKey.i2,
  //      passKey.i3);

  for (i = 0; i < spinCount; ++i)
    (void) randByte (&passKey);

  // Return the clients half of session key.
  sessionKey->i1 = swapLong (sync->key.i1) ^ randLong (&passKey);
  sessionKey->i2 = swapLong (sync->key.i2) ^ randLong (&passKey);
  sessionKey->i3 = swapLong (sync->key.i3) ^ randLong (&passKey);

  //fprintf(stdout,"their decrypted key %08lX %08lX %08lX\n",
  //      sessionKey->i1,
  //      sessionKey->i2,
  //      sessionKey->i3);
}

void buildNewSessionKey (Key * key1, Key * key2, Key * result)
{
  //fprintf(stdout,"key1 %08lX %08lX %08lX\n",key1->i1,key1->i2,key1->i3);
  //fprintf(stdout,"key2 %08lX %08lX %08lX\n",key2->i1,key2->i2,key2->i3);

  // XOR the two keys together to get a new unique key.
  result->i1 = key1->i1 ^ key2->i1;
  result->i2 = key1->i2 ^ key2->i2;
  result->i3 = key1->i3 ^ key2->i3;

  //fprintf(stdout,"new session key %08lX %08lX %08lX\n",result->i1,result->i2,result->i3);
}

// Encrypt or decrypt operation (direction depends on choice of key, which is updated)
//
// Note that this routine is still safe it is an in place encryption.
void encrypt (BYTE * plain, BYTE * cipher, Key * k, WORD length)
{
  int i;
  for (i = 0; i < length; ++i)
    cipher[i] = randByte (k) ^ plain[i];
}

// Encrypt or decrypt operation (direction depends on choice of key, which is updated)
void decrypt (BYTE * cipher, BYTE * plain, Key * k, WORD length)
{
  int i;
  for (i = 0; i < length; ++i)
    plain[i] = randByte (k) ^ cipher[i];
}

// Initialize a key from the provided password.
void initFromPass (BYTE * pwd, Key * key)
{
  BYTE buf[12];
  int i;

  //fprintf(stdout,"%s\n",pwd);

  memset (buf, 0xAA, 12);	// Default fill for key.

  strncpy (buf, pwd, strlen (pwd) < 12 ? strlen (pwd) : 12);	// Add up to 12 bytes of pass.

  // On a little endian machine (ie intel) we must fix byte order.
  key->i1 = swapLong (*(DWORD *) buf);
  key->i2 = swapLong (*(DWORD *) (buf + 4));
  key->i3 = swapLong (*(DWORD *) (buf + 8));

  //fprintf(stdout,"i1 = %08lX\n",key->i1);
  //fprintf(stdout,"i2 = %08lX\n",key->i2);
  //fprintf(stdout,"i3 = %08lX\n",key->i3);

  // randomize things.
  for (i = 0; i < 125; i++)
    (void) randByte (key);

  //fprintf(stdout,"i1 = %08lX\n",key->i1);
  //fprintf(stdout,"i2 = %08lX\n",key->i2);
  //fprintf(stdout,"i3 = %08lX\n",key->i3);
}

// Initialize a key from the clock.
void initFromTime (Key * key)
{
  int i;

  // Don't need to worry about byte order here since it is supposed to be random.
  key->i1 = (rand () << 16) | rand ();
  key->i2 = (rand () << 16) | rand ();
  key->i3 = (rand () << 16) | rand ();

  // Stir awhile.
  for (i = 0; i < 125; i++)
      randByte (key);
}

// Given key, return next random DWORD.
//     Note: modifies key
DWORD randLong (Key * k)
{
  int i;
  DWORD result = 0;

  for (i = 0; i < 32; i += 8)
      result |= (DWORD) randByte (k) << i;

  return result;
}

/* 
 * Given key, return next random byte (as int 0..255)
 * Note: modifies key
 */
BYTE randByte(Key *k)
{
  int i; BYTE result = 0;

  for (i = 0;i < 8;++i)
      result |= randBit(k) << i;

  return result;
}

/*
 * Cryptographically strong random bit generator, based on three lfsr.
 * Uses "alternating stop-and-go" generator.
 */
BYTE randBit (Key *k)
{
  if (lfsr(&k->i1,MASK1))
       lfsr (&k->i2,MASK2);
  else lfsr (&k->i3,MASK3);

  return (k->i2 ^ k->i3) & 1;
}

/*
 * Basic random bit generator, using linear feedback shift register
 */
BYTE lfsr (DWORD *i, DWORD mask)
{
  if (*i & 1)
  {
    *i = ((*i ^ mask) >> 1) | 0x80000000UL;
    return(1);
  }
  *i >>= 1;
  return (0);
}

void initPotp (void)
{
  srand (time(NULL));
}
