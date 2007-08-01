/* pmutil.h -- some helpful utilities for building midi 
               applications that use PortMidi 
 */

typedef void PmQueue;

/*
    A single-reader, single-writer queue is created by
    Pm_QueueCreate(), which takes the number of messages and
    the message size as parameters. The queue only accepts
    fixed sized messages. Returns NULL if memory cannot be allocated.

    Pm_QueueDestroy() destroys the queue and frees its storage.
 */

PmQueue *Pm_QueueCreate(long num_msgs, long bytes_per_msg);
PmError Pm_QueueDestroy(PmQueue *queue);

/* 
    Pm_Dequeue() removes one item from the queue, copying it into msg.
    Returns 1 if successful, and 0 if the queue is empty.
    Returns pmBufferOverflow, clears the overflow flag, and does not
    return a data item if the overflow flag is set. (This protocol
    ensures that the reader will be notified when data is lost due 
    to overflow.)
 */
PmError Pm_Dequeue(PmQueue *queue, void *msg);


/*
    Pm_Enqueue() inserts one item into the queue, copying it from msg.
    Returns pmNoError if successful and pmBufferOverflow if the queue was 
    already full. If pmBufferOverflow is returned, the overflow flag is set.
 */
PmError Pm_Enqueue(PmQueue *queue, void *msg);


/*
    Pm_QueueFull() returns non-zero if the queue is full
    Pm_QueueEmpty() returns non-zero if the queue is empty

    Either condition may change immediately because a parallel
    enqueue or dequeue operation could be in progress.
 */
int Pm_QueueFull(PmQueue *queue);
int Pm_QueueEmpty(PmQueue *queue);


/*
    Pm_QueuePeek() returns a pointer to the item at the head of the queue,
    or NULL if the queue is empty. The item is not removed from the queue.
    If queue is in an overflow state, a valid pointer is returned and the
    queue remains in the overflow state.
 */
void *Pm_QueuePeek(PmQueue *queue);

