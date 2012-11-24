#ifndef _RINGBUFFER_H
#define _RINGBUFFER_H
#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/*
 * ringbuffer.h
 * Ring Buffer utility..
 *
 * Author: Phil Burk, http://www.softsynth.com
 *
 * This program is distributed with the PortAudio Portable Audio Library.
 * For more information see: http://www.audiomulch.com/portaudio/
 * Copyright (c) 1999-2000 Ross Bencina and Phil Burk
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * Any person wishing to distribute modifications to the Software is
 * requested to send the modifications to the original developer so that
 * they can be incorporated into the canonical version.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "s_audio_paring.h"
#include <string.h>

typedef struct
{
    long   bufferSize; /* Number of bytes in FIFO. Power of 2. Set by sys_ringbuf_Init. */
/* These are declared volatile because they are written by a different thread than the reader. */
    volatile long   writeIndex; /* Index of next writable byte. Set by sys_ringbuf_AdvanceWriteIndex. */
    volatile long   readIndex;  /* Index of next readable byte. Set by sys_ringbuf_AdvanceReadIndex. */
    long   bigMask;    /* Used for wrapping indices with extra bit to distinguish full/empty. */
    long   smallMask;  /* Used for fitting indices to buffer. */
    char *buffer;
}
sys_ringbuf;
/*
 * Initialize Ring Buffer.
 */
long sys_ringbuf_Init(sys_ringbuf *rbuf, long numBytes, void *dataPtr, long nfill);

/* Clear buffer. Should only be called when buffer is NOT being read. */
void sys_ringbuf_Flush(sys_ringbuf *rbuf, void *dataPtr, long nfill);

/* Return number of bytes available for writing. */
long sys_ringbuf_GetWriteAvailable( sys_ringbuf *rbuf );
/* Return number of bytes available for read. */
long sys_ringbuf_GetReadAvailable( sys_ringbuf *rbuf );
/* Return bytes written. */
long sys_ringbuf_Write( sys_ringbuf *rbuf, const void *data, long numBytes );
/* Return bytes read. */
long sys_ringbuf_Read( sys_ringbuf *rbuf, void *data, long numBytes );

/* Get address of region(s) to which we can write data.
** If the region is contiguous, size2 will be zero.
** If non-contiguous, size2 will be the size of second region.
** Returns room available to be written or numBytes, whichever is smaller.
*/
long sys_ringbuf_GetWriteRegions( sys_ringbuf *rbuf, long numBytes,
                                 void **dataPtr1, long *sizePtr1,
                                 void **dataPtr2, long *sizePtr2 );
long sys_ringbuf_AdvanceWriteIndex( sys_ringbuf *rbuf, long numBytes );

/* Get address of region(s) from which we can read data.
** If the region is contiguous, size2 will be zero.
** If non-contiguous, size2 will be the size of second region.
** Returns room available to be read or numBytes, whichever is smaller.
*/
long sys_ringbuf_GetReadRegions( sys_ringbuf *rbuf, long numBytes,
                                void **dataPtr1, long *sizePtr1,
                                void **dataPtr2, long *sizePtr2 );

long sys_ringbuf_AdvanceReadIndex( sys_ringbuf *rbuf, long numBytes );

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* _RINGBUFFER_H */
