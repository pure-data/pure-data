/*
 * ringbuffer.c
 * Ring Buffer utility..
 *
 * Author: Phil Burk, http://www.softsynth.com
 *
 * This program uses the PortAudio Portable Audio Library.
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
/*
 * modified 2002/07/13 by olaf.matthes@gmx.de to allow any number if channels
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "s_audio_paring.h"
#include <string.h>

/***************************************************************************
 * Initialize FIFO.
 */
long sys_ringbuf_Init(sys_ringbuf *rbuf, long numBytes, void *dataPtr, long nfill)
{
    rbuf->bufferSize = numBytes;
    rbuf->buffer = (char *)dataPtr;
    sys_ringbuf_Flush(rbuf, dataPtr,  nfill);
    return 0;
}
/***************************************************************************
** Return number of bytes available for reading. */
long sys_ringbuf_GetReadAvailable( sys_ringbuf *rbuf )
{
    long ret = rbuf->writeIndex - rbuf->readIndex;
    if (ret < 0)
        ret += 2 * rbuf->bufferSize;
    if (ret < 0 || ret > rbuf->bufferSize)
        fprintf(stderr,
            "consistency check failed: sys_ringbuf_GetReadAvailable\n");
    return ( ret );
}
/***************************************************************************
** Return number of bytes available for writing. */
long sys_ringbuf_GetWriteAvailable( sys_ringbuf *rbuf )
{
    return ( rbuf->bufferSize - sys_ringbuf_GetReadAvailable(rbuf));
}

/***************************************************************************
** Clear buffer. Should only be called when buffer is NOT being read. */
void sys_ringbuf_Flush(sys_ringbuf *rbuf, void *dataPtr, long nfill)
{
    char *s;
    long n;
    rbuf->readIndex = 0;
    rbuf->writeIndex = nfill;
    for (n = nfill, s = dataPtr; n--; s++)
        *s = 0;
}

/***************************************************************************
** Get address of region(s) to which we can write data.
** If the region is contiguous, size2 will be zero.
** If non-contiguous, size2 will be the size of second region.
** Returns room available to be written or numBytes, whichever is smaller.
*/
long sys_ringbuf_GetWriteRegions( sys_ringbuf *rbuf, long numBytes,
                                 void **dataPtr1, long *sizePtr1,
                                 void **dataPtr2, long *sizePtr2 )
{
    long   index;
    long   available = sys_ringbuf_GetWriteAvailable( rbuf );
    if( numBytes > available ) numBytes = available;
    /* Check to see if write is not contiguous. */
    index = rbuf->writeIndex;
    while (index >= rbuf->bufferSize)
        index -= rbuf->bufferSize;
    if( (index + numBytes) > rbuf->bufferSize )
    {
        /* Write data in two blocks that wrap the buffer. */
        long   firstHalf = rbuf->bufferSize - index;
        *dataPtr1 = &rbuf->buffer[index];
        *sizePtr1 = firstHalf;
        *dataPtr2 = &rbuf->buffer[0];
        *sizePtr2 = numBytes - firstHalf;
    }
    else
    {
        *dataPtr1 = &rbuf->buffer[index];
        *sizePtr1 = numBytes;
        *dataPtr2 = NULL;
        *sizePtr2 = 0;
    }
    return numBytes;
}


/***************************************************************************
*/
long sys_ringbuf_AdvanceWriteIndex( sys_ringbuf *rbuf, long numBytes )
{
    long ret = (rbuf->writeIndex + numBytes);
    if ( ret >= 2 * rbuf->bufferSize)
        ret -= 2 * rbuf->bufferSize;    /* check for end of buffer */
    return rbuf->writeIndex = ret;
}

/***************************************************************************
** Get address of region(s) from which we can read data.
** If the region is contiguous, size2 will be zero.
** If non-contiguous, size2 will be the size of second region.
** Returns room available to be written or numBytes, whichever is smaller.
*/
long sys_ringbuf_GetReadRegions( sys_ringbuf *rbuf, long numBytes,
                                void **dataPtr1, long *sizePtr1,
                                void **dataPtr2, long *sizePtr2 )
{
    long   index;
    long   available = sys_ringbuf_GetReadAvailable( rbuf );
    if( numBytes > available ) numBytes = available;
    /* Check to see if read is not contiguous. */
    index = rbuf->readIndex;
    while (index >= rbuf->bufferSize)
        index -= rbuf->bufferSize;
    
    if( (index + numBytes) > rbuf->bufferSize )
    {
        /* Write data in two blocks that wrap the buffer. */
        long firstHalf = rbuf->bufferSize - index;
        *dataPtr1 = &rbuf->buffer[index];
        *sizePtr1 = firstHalf;
        *dataPtr2 = &rbuf->buffer[0];
        *sizePtr2 = numBytes - firstHalf;
    }
    else
    {
        *dataPtr1 = &rbuf->buffer[index];
        *sizePtr1 = numBytes;
        *dataPtr2 = NULL;
        *sizePtr2 = 0;
    }
    return numBytes;
}
/***************************************************************************
*/
long sys_ringbuf_AdvanceReadIndex( sys_ringbuf *rbuf, long numBytes )
{
    long ret = (rbuf->readIndex + numBytes);
    if( ret >= 2 * rbuf->bufferSize)
        ret -= 2 * rbuf->bufferSize;
    return rbuf->readIndex = ret;
}

/***************************************************************************
** Return bytes written. */
long sys_ringbuf_Write( sys_ringbuf *rbuf, const void *data, long numBytes )
{
    long size1, size2, numWritten;
    void *data1, *data2;
    numWritten = sys_ringbuf_GetWriteRegions( rbuf, numBytes, &data1, &size1, &data2, &size2 );
    if( size2 > 0 )
    {

        memcpy( data1, data, size1 );
        data = ((char *)data) + size1;
        memcpy( data2, data, size2 );
    }
    else
    {
        memcpy( data1, data, size1 );
    }
    sys_ringbuf_AdvanceWriteIndex( rbuf, numWritten );
    return numWritten;
}

/***************************************************************************
** Return bytes read. */
long sys_ringbuf_Read( sys_ringbuf *rbuf, void *data, long numBytes )
{
    long size1, size2, numRead;
    void *data1, *data2;
    numRead = sys_ringbuf_GetReadRegions( rbuf, numBytes, &data1, &size1, &data2, &size2 );
    if( size2 > 0 )
    {
        memcpy( data, data1, size1 );
        data = ((char *)data) + size1;
        memcpy( data, data2, size2 );
    }
    else
    {
        memcpy( data, data1, size1 );
    }
    sys_ringbuf_AdvanceReadIndex( rbuf, numRead );
    return numRead;
}
