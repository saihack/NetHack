/*	SCCS Id: @(#)alloc.c	3.1	90/22/02
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/* to get the malloc() prototype from system.h */
#define ALLOC_C		/* comment line for pre-compiled headers */
/* since this file is also used in auxiliary programs, don't include all the 
 * function declarations for all of nethack
 */
#define EXTERN_H	/* comment line for pre-compiled headers */
#include "config.h"
long *FDECL(alloc,(unsigned int));

#ifndef __TURBOC__
extern void VDECL(panic, (const char *,...)) PRINTF_F(1,2);

long *
alloc(lth)
register unsigned int lth;
{
	genericptr_t ptr;

	if(!(ptr = calloc(lth,1))) /* allways initialize memory */
		panic("Cannot get %d bytes", lth);
	return((long *) ptr);
}
#endif


/*alloc.c*/
