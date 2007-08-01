/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* This file should be compared with the corresponding thing in the TK
* distribution whenever updating to newer versions of TCL/TK. */

/* 
 * Copyright (c) 1993 The Regents of the University of California.
 * Copyright (c) 1994 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */


#ifndef __APPLE__     /* linux and IRIX only; in __APPLE__ we don't link this in */
#include "tk.h"
#include <stdlib.h>

/*
 *----------------------------------------------------------------------
 *
 * main --
 *
 *      This is the main program for the application.
 *
 * Results:
 *      None: Tk_Main never returns here, so this procedure never
 *      returns either.
 *
 * Side effects:
 *      Whatever the application does.
 *
 *----------------------------------------------------------------------
 */

void pdgui_startup(Tcl_Interp *interp);
void pdgui_setname(char *name);
void pdgui_setsock(int port);
void pdgui_sethost(char *name);

int
main(int argc, char **argv)
{
    pdgui_setname(argv[0]);
    if (argc >= 2)
    {
        pdgui_setsock(atoi(argv[1]));
        argc--; argv++;
        argv[0] = "Pd";
    }
    if (argc >= 2)
    {
        pdgui_sethost(argv[1]);
        argc--; argv++;
        argv[0] = "Pd";
    }
    Tk_Main(argc, argv, Tcl_AppInit);
    return 0;                   /* Needed only to prevent compiler warning. */
}


/*
 *----------------------------------------------------------------------
 *
 * Tcl_AppInit --
 *
 *      This procedure performs application-specific initialization.
 *      Most applications, especially those that incorporate additional
 *      packages, will have their own version of this procedure.
 *
 * Results:
 *      Returns a standard Tcl completion code, and leaves an error
 *      message in interp->result if an error occurs.
 *
 * Side effects:
 *      Depends on the startup script.
 *
 *----------------------------------------------------------------------
 */


int
Tcl_AppInit(interp)
    Tcl_Interp *interp;         /* Interpreter for application. */
{
    Tk_Window mainwindow;

    if (Tcl_Init(interp) == TCL_ERROR) {
        return TCL_ERROR;
    }
    if (Tk_Init(interp) == TCL_ERROR) {
        return TCL_ERROR;
    }

    /* setup specific to pd-gui: */

    pdgui_startup(interp);

    /*
     * Specify a user-specific startup file to invoke if the application
     * is run interactively.  Typically the startup file is "~/.apprc"
     * where "app" is the name of the application.  If this line is deleted
     * then no user-specific startup file will be run under any conditions.
     */

#if 0
    tcl_RcFileName = "~/.apprc";
#endif

    return TCL_OK;
}

#endif  /* __APPLE__ */
