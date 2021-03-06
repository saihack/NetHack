/*	SCCS Id: @(#)windows.c	3.1	93/07/13	*/
/* Copyright (c) D. Cohrs, 1993. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#ifdef TTY_GRAPHICS
#include "wintty.h"
#endif
#ifdef X11_GRAPHICS
/* cannot just blindly include winX.h without including all of X11 stuff */
/* and must get the order of include files right.  Don't bother */
extern struct window_procs X11_procs;
extern void NDECL(win_X11_init);
#endif
#ifdef MAC
extern struct window_procs mac_procs;
#endif
#ifdef AMIGA_INTUITION
extern struct window_procs amii_procs;
extern struct window_procs amiv_procs;
extern void NDECL(amii_loadlib);
extern void NDECL(amiv_loadlib);
#endif

static void FDECL(def_raw_print, (const char *s));

NEARDATA struct window_procs windowprocs;

static
struct win_choices {
    struct window_procs *procs;
    void NDECL((*ini_routine));		/* optional (can be 0) */
} winchoices[] = {
#ifdef TTY_GRAPHICS
    { &tty_procs, win_tty_init },
#endif
#ifdef X11_GRAPHICS
    { &X11_procs, win_X11_init },
#endif
#ifdef MAC
    { &mac_procs, 0 },
#endif
#ifdef AMIGA_INTUITION
	/* A shared library is used for implementing the access to these two
	 * different display mechanisms.  This means that the function names
	 * are actually the same (assembler stubs) and the libraries do
	 * different things.
	 */
    { &amii_procs, amii_loadlib },
    { &amiv_procs, amiv_loadlib },
#endif
    { 0, 0 }		/* must be last */
};

static
void
def_raw_print(s)
const char *s;
{
    puts(s);
}

void
choose_windows(s)
const char *s;
{
    register int i;

    for(i=0; winchoices[i].procs; i++)
	if (!strcmpi(s, winchoices[i].procs->name)) {
	    windowprocs = *winchoices[i].procs;
	    if (winchoices[i].ini_routine) (*winchoices[i].ini_routine)();
	    return;
	}

    if (!windowprocs.win_raw_print)
	windowprocs.win_raw_print = def_raw_print;

    raw_printf("Window type %s not recognized.  Choices are:", s);
    for(i=0; winchoices[i].procs; i++)
	raw_printf("        %s", winchoices[i].procs->name);

    if (windowprocs.win_raw_print == def_raw_print)
	terminate(0);
}

/*windows.c*/
