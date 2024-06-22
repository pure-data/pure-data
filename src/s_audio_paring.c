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
 * modified 2002/07/13 by olaf.matthes@gmx.de to allow any number if channels
 *
 * extensively hacked by msp@ucsd.edu for various reasons
 *
 */

#include "s_audio_paring.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else /* _WIN32 */
#include <sys/time.h>
#include <errno.h>
#if defined(__APPLE__) && defined(__MACH__)
/* macOS does not support unnamed posix semaphores,
 * so we use Mach semaphores instead. */
#include <mach/mach.h>
#define HAVE_MACH_SEMAPHORE
#elif defined(__linux__) || defined(__FreeBSD__) || \
    defined(__NetBSD__) || defined(__OpenBSD__)
#include <semaphore.h>
#define HAVE_POSIX_SEMAPHORE
#else
/* Some platforms (including Apple) do not support unnamed posix semaphores,
 * others might not implement sem_timedwait(); to be on the safe side, we
 * we simply emulate it with a counter, mutex and condition variable.
 * The critical section is so short that there should not be any locking.
 */
#include <pthread.h>
#endif
#endif /* _WIN32 */

/* Clear buffer. Should only be called when buffer is NOT being read. */
static void sys_ringbuf_Flush(sys_ringbuf *rbuf,
    void *dataPtr, long nfill);

/* Get address of region(s) to which we can write data.
** If the region is contiguous, size2 will be zero.
** If non-contiguous, size2 will be the size of second region.
** Returns room available to be written or numBytes, whichever is smaller.
*/
static long sys_ringbuf_GetWriteRegions(sys_ringbuf *rbuf,
    long numBytes, void **dataPtr1, long *sizePtr1,
    void **dataPtr2, long *sizePtr2, char *buffer);

static long sys_ringbuf_AdvanceWriteIndex(sys_ringbuf *rbuf, long numBytes);

/* Get address of region(s) from which we can read data.
** If the region is contiguous, size2 will be zero.
** If non-contiguous, size2 will be the size of second region.
** Returns room available to be read or numBytes, whichever is smaller.
*/
static long sys_ringbuf_GetReadRegions(sys_ringbuf *rbuf,
    long numBytes, void **dataPtr1, long *sizePtr1,
    void **dataPtr2, long *sizePtr2, char *buffer);

static long sys_ringbuf_AdvanceReadIndex(sys_ringbuf *rbuf, long numBytes);

/***************************************************************************
 * Initialize FIFO.
 */
long sys_ringbuf_init(sys_ringbuf *rbuf, long numBytes,
    char *dataPtr, long nfill)
{
    rbuf->bufferSize = numBytes;
    sys_ringbuf_Flush(rbuf, dataPtr, nfill);
    return 0;
}
/***************************************************************************
** Return number of bytes available for reading. */
long sys_ringbuf_getreadavailable(sys_ringbuf *rbuf)
{
    long ret = atomic_int_load(&rbuf->writeIndex)
        - atomic_int_load(&rbuf->readIndex);
    if (ret < 0)
        ret += 2 * rbuf->bufferSize;
    if (ret < 0 || ret > rbuf->bufferSize)
        fprintf(stderr,
            "consistency check failed: sys_ringbuf_getreadavailable\n");
    return (ret);
}
/***************************************************************************
** Return number of bytes available for writing. */
long sys_ringbuf_getwriteavailable(sys_ringbuf *rbuf)
{
    return (rbuf->bufferSize - sys_ringbuf_getreadavailable(rbuf));
}

/***************************************************************************
** Clear buffer. Should only be called when buffer is NOT being read. */
static void sys_ringbuf_Flush(sys_ringbuf *rbuf,
    void *dataPtr, long nfill)
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
static long sys_ringbuf_GetWriteRegions(sys_ringbuf *rbuf,
    long numBytes, void **dataPtr1, long *sizePtr1,
    void **dataPtr2, long *sizePtr2, char *buffer)
{
    long index;
    long available = sys_ringbuf_getwriteavailable(rbuf);
    if (numBytes > available) numBytes = available;
    /* Check to see if write is not contiguous. NB: this is always called
    by the writer, so we don't need an atomic load */
    index = rbuf->writeIndex;
    while (index >= rbuf->bufferSize)
        index -= rbuf->bufferSize;
    if( (index + numBytes) > rbuf->bufferSize )
    {
        /* Write data in two blocks that wrap the buffer. */
        long firstHalf = rbuf->bufferSize - index;
        *dataPtr1 = &buffer[index];
        *sizePtr1 = firstHalf;
        *dataPtr2 = &buffer[0];
        *sizePtr2 = numBytes - firstHalf;
    }
    else
    {
        *dataPtr1 = &buffer[index];
        *sizePtr1 = numBytes;
        *dataPtr2 = NULL;
        *sizePtr2 = 0;
    }
    return numBytes;
}


/***************************************************************************
*/
static long sys_ringbuf_AdvanceWriteIndex(sys_ringbuf *rbuf, long numBytes)
{
    long ret = (rbuf->writeIndex + numBytes);
    if (ret >= 2 * rbuf->bufferSize)
        ret -= 2 * rbuf->bufferSize;    /* check for end of buffer */
    atomic_int_store(&rbuf->writeIndex, ret);
    return ret;
}

/***************************************************************************
** Get address of region(s) from which we can read data.
** If the region is contiguous, size2 will be zero.
** If non-contiguous, size2 will be the size of second region.
** Returns room available to be written or numBytes, whichever is smaller.
*/
static long sys_ringbuf_GetReadRegions(sys_ringbuf *rbuf,
    long numBytes, void **dataPtr1, long *sizePtr1,
    void **dataPtr2, long *sizePtr2, char *buffer)
{
    long index;
    long available = sys_ringbuf_getreadavailable(rbuf);
    if( numBytes > available ) numBytes = available;
    /* Check to see if read is not contiguous. NB: this is always called
    by the reader, so we don't need an atomic load */
    index = rbuf->readIndex;
    while (index >= rbuf->bufferSize)
        index -= rbuf->bufferSize;

    if((index + numBytes) > rbuf->bufferSize)
    {
        /* Write data in two blocks that wrap the buffer. */
        long firstHalf = rbuf->bufferSize - index;
        *dataPtr1 = &buffer[index];
        *sizePtr1 = firstHalf;
        *dataPtr2 = &buffer[0];
        *sizePtr2 = numBytes - firstHalf;
    }
    else
    {
        *dataPtr1 = &buffer[index];
        *sizePtr1 = numBytes;
        *dataPtr2 = NULL;
        *sizePtr2 = 0;
    }
    return numBytes;
}
/***************************************************************************
*/
static long sys_ringbuf_AdvanceReadIndex(sys_ringbuf *rbuf,
    long numBytes)
{
    long ret = (rbuf->readIndex + numBytes);
    if( ret >= 2 * rbuf->bufferSize)
        ret -= 2 * rbuf->bufferSize;
    atomic_int_store(&rbuf->readIndex, ret);
    return ret;
}

/***************************************************************************
** Return bytes written. */
long sys_ringbuf_write(sys_ringbuf *rbuf, const void *data,
    long numBytes, char *buffer)
{
    long size1, size2, numWritten;
    void *data1, *data2;
    numWritten = sys_ringbuf_GetWriteRegions(rbuf, numBytes, &data1, &size1,
        &data2, &size2, buffer);
    if (size2 > 0)
    {

        memcpy((void *)data1, data, size1);
        data = ((char *)data) + size1;
        memcpy((void *)data2, data, size2);
    }
    else
    {
        memcpy((void *)data1, data, size1);
    }
    sys_ringbuf_AdvanceWriteIndex(rbuf, numWritten);
    return numWritten;
}

/***************************************************************************
** Return bytes read. */
long sys_ringbuf_read(sys_ringbuf *rbuf, void *data, long numBytes,
    char *buffer)
{
    long size1, size2, numRead;
    void *data1, *data2;
    numRead = sys_ringbuf_GetReadRegions(rbuf, numBytes, &data1, &size1,
        &data2, &size2, buffer);
    if (size2 > 0)
    {
        memcpy(data, (void *)data1, size1);
        data = ((char *)data) + size1;
        memcpy(data, (void *)data2, size2);
    }
    else
    {
        memcpy(data, (void *)data1, size1);
    }
    sys_ringbuf_AdvanceReadIndex(rbuf, numRead);
    return numRead;
}

/***************************************************************************
** t_semaphore */

#ifndef _WIN32
struct _semaphore
{
#ifdef HAVE_MACH_SEMAPHORE
    semaphore_t sem;
#elif defined(HAVE_POSIX_SEMAPHORE)
    sem_t sem;
#else
    pthread_mutex_t mutex;
    pthread_cond_t condvar;
    int count;
#endif
};
#endif

t_semaphore * sys_semaphore_create(void)
{
#ifdef _WIN32
    return (t_semaphore *)CreateSemaphoreA(0, 0, LONG_MAX, 0);
#else /* _WIN32 */
    t_semaphore *sem = (t_semaphore *)malloc(sizeof(t_semaphore));
#ifdef HAVE_MACH_SEMAPHORE
    semaphore_create(mach_task_self(), &sem->sem, SYNC_POLICY_FIFO, 0);
#elif defined(HAVE_POSIX_SEMAPHORE)
    sem_init(&sem->sem, 0, 0);
#else
    pthread_cond_init(&sem->condvar, 0);
    pthread_mutex_init(&sem->mutex, 0);
    sem->count = 0;
#endif
    return sem;
#endif /* _WIN32 */
}

void sys_semaphore_destroy(t_semaphore *sem)
{
#ifdef _WIN32
    CloseHandle((HANDLE)sem);
#else /* _WIN32 */
#ifdef HAVE_MACH_SEMAPHORE
    semaphore_destroy(mach_task_self(), sem->sem);
#elif defined(HAVE_POSIX_SEMAPHORE)
    sem_destroy(&sem->sem);
#else
    pthread_mutex_destroy(&sem->mutex);
    pthread_cond_destroy(&sem->condvar);
#endif
    free(sem);
#endif /* _WIN32 */
}

void sys_semaphore_wait(t_semaphore *sem)
{
#if defined(_WIN32)
    WaitForSingleObject((HANDLE)sem, INFINITE);
#elif defined(HAVE_MACH_SEMAPHORE)
    semaphore_wait(sem->sem);
#elif defined(HAVE_POSIX_SEMAPHORE)
    while (sem_wait(&sem->sem) == -1 && errno == EINTR) continue;
#else
    pthread_mutex_lock(&sem->mutex);
    while (sem->count == 0)
        pthread_cond_wait(&sem->condvar, &sem->mutex);
    sem->count--;
    pthread_mutex_unlock(&sem->mutex);
#endif
}

int sys_semaphore_waitfor(t_semaphore *sem, double seconds)
{
#ifdef _WIN32
    return WaitForSingleObject((HANDLE)sem, seconds * 1000.0) != WAIT_TIMEOUT;
#elif defined(HAVE_MACH_SEMAPHORE)
    mach_timespec_t ts;
    ts.tv_sec = (unsigned int)seconds;
    ts.tv_nsec = (seconds - ts.tv_sec) * 1000000000;
    return semaphore_timedwait(sem->sem, ts) != KERN_OPERATION_TIMED_OUT;
#else /* Posix semaphore and pthreads */
        /* first check if the semaphore is available */
#ifdef HAVE_POSIX_SEMAPHORE
    if (sem_trywait(&sem->sem) == 0)
        return 1;
#else
    pthread_mutex_lock(&sem->mutex);
    if (sem->count > 0)
    {
        sem->count--;
        pthread_mutex_unlock(&sem->mutex);
        return 1;
    }
    pthread_mutex_unlock(&sem->mutex);
#endif
        /* otherwise wait for it to become available */
    struct timespec ts;
    struct timeval now;
    gettimeofday(&now, 0);
        /* add fractional part to timeout */
    seconds += now.tv_usec * 0.000001;
    ts.tv_sec = now.tv_sec + (time_t)seconds;
    ts.tv_nsec = (seconds - (time_t)seconds) * 1000000000;
#ifdef HAVE_POSIX_SEMAPHORE
    while (sem_timedwait(&sem->sem, &ts) == -1)
    {
        if (errno != EINTR) /* ETIMEDOUT */
            return 0;
    }
    return 1;
#else
    pthread_mutex_lock(&sem->mutex);
    while (sem->count == 0)
    {
        if (pthread_cond_timedwait(&sem->condvar, &sem->mutex, &ts) == ETIMEDOUT)
        {
            pthread_mutex_unlock(&sem->mutex);
            return 0;
        }
    }
    sem->count--;
    pthread_mutex_unlock(&sem->mutex);
    return 1;
#endif
#endif
}

void sys_semaphore_post(t_semaphore *sem)
{
#if defined(_WIN32)
    ReleaseSemaphore((HANDLE)sem, 1, NULL);
#elif defined(HAVE_MACH_SEMAPHORE)
    semaphore_signal(sem->sem);
#elif defined(HAVE_POSIX_SEMAPHORE)
    sem_post(&sem->sem);
#else
    pthread_mutex_lock(&sem->mutex);
    sem->count++;
    pthread_mutex_unlock(&sem->mutex);
    pthread_cond_signal(&sem->condvar);
#endif
}
