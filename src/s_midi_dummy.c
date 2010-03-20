/* Copyright (c) 1997-2003 Guenter Geiger, Miller Puckette, Larry Troxler,
* Winfried Ritsch, Karl MacMillan, and others.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.

   this file is a dummy for systems without any MIDI support

*/

void sys_do_open_midi(int nmidiin, int *midiinvec,
    int nmidiout, int *midioutvec)
{
}

void sys_close_midi( void)
{
}

void sys_putmidimess(int portno, int a, int b, int c)
{
}

void sys_putmidibyte(int portno, int byte)
{
}

void sys_poll_midi(void)
{
}

void midi_getdevs(char *indevlist, int *nindevs,
    char *outdevlist, int *noutdevs, int maxndev, int devdescsize)
{
}
