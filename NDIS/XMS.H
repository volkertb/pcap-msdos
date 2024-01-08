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
#define ERR_NO_ERR		0x00
#define ERR_NOT_IMPLEMENTED	0x80
#define ERR_VDISK_FOUND		0x81
#define ERR_A20			0x82
#define ERR_GENERAL		0x8E
#define ERR_UNRECOVERABLE	0x8F
#define ERR_HMA_NOT_EXIST	0x90
#define ERR_HMA_IN_USE		0x91
#define ERR_HMA_MIN_SIZE	0x92
#define ERR_HMA_NOT_ALLOCED	0x93
#define ERR_A20_STILL_ON	0x94
#define ERR_OUT_OF_MEMORY	0xA0
#define ERR_OUT_OF_HANDLES	0xA1
#define ERR_INVALID_HANDLE	0xA2
#define ERR_SH_INVALID		0xA3
#define ERR_SO_INVALID		0xA4
#define ERR_DH_INVALID		0xA5
#define ERR_DO_INVALID		0xA6
#define ERR_LEN_INVALID		0xA7
#define ERR_OVERLAP		0xA8
#define ERR_PARITY		0xA9
#define ERR_EMB_UNLOCKED	0xAA
#define ERR_EMB_LOCKED		0xAB
#define ERR_LOCK_OVERFLOW	0xAC
#define ERR_LOCK_FAIL		0xAD
#define ERR_UMB_SIZE_TOO_BIG	0xB0
#define ERR_NO_UMBS		0xB1
#define ERR_INVALID_UMB		0xB2


typedef struct _XmsRegs {
	WORD ax;
	WORD bx;
	WORD dx;
	WORD si;
	WORD ds;
} XmsRegs;

typedef struct _XmsMove {
	DWORD length;
	WORD srcHandle;
	DWORD srcOffset;
	WORD destHandle;
	DWORD destOffset;
} XmsMove;
