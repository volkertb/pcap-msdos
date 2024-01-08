/************************************************************************/
/*	Copyright (C) 1986-1990 Phar Lap Software, Inc.                  */
/*	Unpublished - rights reserved under the Copyright Laws of the    */
/*	United States.  Use, duplication, or disclosure by the           */
/*	Government is subject to restrictions as set forth in            */
/*	subparagraph (c)(1)(ii) of the Rights in Technical Data and      */
/*	Computer Software clause at 252.227-7013.                        */
/*	Phar Lap Software, Inc., 60 Aberdeen Ave., Cambridge, MA 02138   */
/************************************************************************/

/* INCLUDES */

#include <stdio.h>
#include <dos.h>
#include <pharlap.h>
#include <hw386.h>
#include <pldos32.h>

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

void load_err(UCHAR *namep, int errv, UINT info, UINT dosinfo);
int call_child(LDEXP_BLK *ldblkp, ULONG ndword_params, ...);

/*
This example program is run as:
	load <.EXE filename>

It loads the specified file, and does a FAR call to its entry point.  The
loaded program terminates by executing a FAR return.

NOTE:	If you are loading a program built with a high level language, you
	MUST build the program to be loaded with a modified version of the
	initializer that comes with the compiler.  You have to modify the
	initializer to:
	(1) make sure the program terminates with a FAR RET, instead of
		DOS system call INT 21h func 4Ch, and
	(2) the heap initialization modifies the size of the correct segment
		(i.e., doesn't assume hardwired segments 000Ch and 0014h),
		and calculates the desired segment size correctly (i.e., does
		NOT pull program size values out of the PSP, which applies
		to this program not the loaded program, instead it uses
		size values returned in the load parameter block by the
		load program system call).
*/
int main (argc,argv)
int	argc;
char	**argv;
{
	UCHAR *namep;			/* ptr to pgm name string */
	LDEXP_BLK ldblk;		/* Load parameter block */
	CONFIG_INF cbuf;		/* config info */
	BOOL unprivf;			/* T ==> if this pgm runs unprivileged*/
	int errv;			/* Error value */
	UINT vmhandle;			/* 386|VMM handle */
	UINT einfo;			/* Error information */
	UINT doseinfo;			/* DOS error information */
	USHORT code_sel;		/* child's code segment selector */
	USHORT data_sel;		/* child's data segment selector */

/*
 * Get program name from command line
 */
	if (argc < 2)
	{
		printf("Usage: load filename");
		return TRUE;
	}
	namep = (UCHAR *) *(argv + 1);
	
/*
 * Get config info so we have CS for this program;  then look at the 
 * RPL bits in CS to see if we are running at privilege level 0 (-PRIV) or
 * unprivileged (-UNPRIV).  All we use this for is checking to see if child 
 * privilege selection is the same as ours.
 */
	_dx_config_inf(&cbuf, (UCHAR *) &cbuf);
	unprivf = (cbuf.c_cs_sel & SEL_RPL) != 0;

/*
 * Make DOS-X system call to load the program.  If the child privilege
 * level selection (-PRIV or -UNPRIV) is different from ours, print out 
 * a warning message to that effect.  A program linked with -UNPRIV should
 * normally be capable of running at privilege level 0 (-PRIV), but a 
 * program linked with -PRIV may not run unprivileged if it performs 
 * operations (such as accessing the GDT or IDT directly) that are not
 * possible when running unprivileged.
 */
	errv = _dx_ld_flat(namep, &ldblk, FALSE, &vmhandle, &einfo,&doseinfo);
	if (errv != _DOSE_NONE)
	{
		load_err(namep, errv, einfo, doseinfo);
		return TRUE;
	}
	if (ldblk.flags & LD_UNPRIV)
	{
		if (!unprivf)
			printf("Warning:  loader is running privileged,\n\
          child program %s is linked with -UNPRIV\n", namep);
	}
	else if (unprivf)
		printf("Warning:  loader is running unprivileged,\n\
          child program %s is linked with -PRIV\n", namep);

/*
 * Call assembly language routine to load regs and do FAR call to
 * loaded child pgm.  First save child's code and data segment selectors,
 * which we need to free up the child later.
 */
	printf("Calling loaded program: <<%s>>\n", namep);
	fflush(stdout);
	code_sel = ldblk.cs;
	data_sel = ldblk.ds;
	call_child(&ldblk, 0);

/*
 * Program terminated, now free up the segments for it in the LDT, and
 * if we have a VMM handle make a system call to release the handle.
 *
 * We have to free both the code & data segment to make the allocated memory
 * get freed;  freeing just one removes the segment aliasing but leaves the
 * other still allocated.
 */
	errv = (int) _dos_freemem(code_sel);
	if (errv != _DOSE_NONE)
		goto FREE_ERR;
	errv = (int) _dos_freemem(data_sel);
	if (errv != _DOSE_NONE)
	{
FREE_ERR:
		printf("Unexpected err freeing segment: %d\n", errv);
		return TRUE;
	}
	if (vmhandle != (UINT) -1)
	{
		errv = _dx_vmm_close(vmhandle);
		if (errv != _DOSE_NONE)
		{
			printf("Unexpected err releasing 386|VMM handle: %d\n",
								errv);
			return TRUE;
		}
	}

/*
 * Successful terminate
 */
	return FALSE;
}

/*	<load_err> - Print error message after load error */

void load_err(UCHAR *namep, int errv, UINT info, UINT dosinfo)

/*
Description:
	This routine takes the error information returned by the load 
	program system call and prints out an appropriate error message.

Calling arguments:
	namep		ptr to file name string
	errv		error code returned by load pgm system call
	info		sub-error code ret'd for file error or memory error
	dosinfo		DOS error code ret'd for DOS file error

Returned values:
	- none
*/
{
	if (errv == 2)
	{
		printf("File related error loading %s\n", namep);
		switch(info)
		{
		case 1:
			printf("       DOS error opening file\n");
			break;
		case 2:
			printf("       DOS error seeking in file\n");
			break;
		case 3:
			printf("       DOS error reading file\n");
			break;
		case 4:
			printf("       Not flat model EXP or REX file\n");
			break;
		case 5:
			printf("       Invalid file format\n");
			break;
		case 6:
			printf("       Program linked with -OFFSET not \
multiple of 4k\n");
			break;
		case 7:
			printf("       -NOPAGE in effect and program was \
linked with -REALBREAK or -OFFSET\n");
			break;
		default:
			printf("       Unexpected file error code: %d\n",
								info);
			break;
		}
		if (info == 1 || info == 2 || info == 3)
		{
			switch(dosinfo)
			{
			case 2:
				printf("       File not found\n");
				break;
			case 3:
				printf("       Path not found\n");
				break;
			case 4:
				printf("       Too many open files\n");
				break;
			case 5:
				printf("       Access denied\n");
				break;
			case 6:
				printf("       Invalid file handle\n");
				break;
			case 12:
				printf("       Invalid file access code\n");
				break;
			default:
				printf("       Unexpected DOS err code: %d\n",
								dosinfo);
				break;
			}
		}
	}

	else if (errv == 8)
	{
		printf("Memory related error loading %s\n", namep);
		switch(info){
			case 1:
				printf("       Out of physical memory\n");
				break;
			case 2:
				printf("       Out of swap space\n");
				break;
			case 3:
				printf("       Unable to grow LDT\n");
				break;
			case 4:
				printf("       Unable to change extended \
memory allocation mark\n");
				break;
			case 5:
				printf("       Maximum virtual size (from \
-MAXPGMMEM switch) exceeded\n");
				break;
			case 6:
				printf("       Insuff. conv memory to satisfy \
-REALBREAK switch setting\n");
				break;
			case 7:
				printf("       No conv memory available for \
program PSP and environment\n");
				break;
			default:
				printf("       Unexpected mem err code: %d\n",
								info);
				break;
		}
	}

	else
	{
		printf("Unexpected error code from load pgm: %d\n", errv);
		return;
	}

	return;
}
