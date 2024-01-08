/*
 * Copyright (c) 1993,1994
 *	Texas A&M University.  All rights reserved.
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
 *	This product includes software developed by Texas A&M University
 *	and its contributors.
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
#define IS_BROADCAST(address) \
	((mediaType == MEDIA_TOKEN) ? (address).bytes[0] & 0x80 : (address).bytes[0] & 0x01)

#define PERROR(string) { \
	fprintf(stderr,"filter: %s line %d: %s\n",__FILE__,__LINE__,string); \
	exit(1); \
}

#define PWARN(string) { \
	fprintf(stderr,"filter: %s line %d: %s\n",__FILE__,__LINE__,string); \
}

#define GET_DEBUG_STRING ( debugStrings[stopDebug + 1 == MAX_NUM_DEBUG_STRINGS ? stopDebug = 0 : ++stopDebug] )


#define MAC_CHAR(card) ((MacCharacteristics *) (card)->common->serviceCharacteristics)
#define MAC_DISPATCH(card) ((MacUpperDispatch *) (card)->common->upperDispatchTable)
#define MAC_STATUS(card) ((MacStatusTable *) (card)->common->serviceStatus)

#define GUARD \
	asm pushf; \
	asm cli

#define UNGUARD \
	asm popf
