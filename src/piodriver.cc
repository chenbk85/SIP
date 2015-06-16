/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines the constants, types and functions that implement the 
/// prioritized I/O driver. The prioritized I/O driver is responsible for 
/// the high-level coordination logic for different I/O types, and for pushing
/// a prioritized stream of I/O commands to the asynchronous I/O driver.
///////////////////////////////////////////////////////////////////////////80*/

/*////////////////////
//   Preprocessor   //
////////////////////*/

/*////////////////
//   Includes   //
////////////////*/

/*/////////////////
//   Constants   //
/////////////////*/
/// @summary The maximum number of buffers that can be queued for interval-based 
/// delivery. This value must be a small power-of-two greater than zero. 
static size_t const PIO_MAX_DELIVERY_BUFFERS     = 4;

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Bitflags that can be specified to control the behavior of a stream-in file load.
enum pio_stream_in_flags_e : uint32_t
{
    PIO_STREAM_IN_FLAGS_NONE     = (0 << 0), /// No special behavior is requested.
    PIO_STREAM_IN_FLAGS_LOAD     = (1 << 0), /// The stream should be loaded completely once, then closed.
};

/// @summary Bitflags specifying the status of an active stream-in file load.
enum pio_stream_in_status_e : uint32_t
{
    PIO_STREAM_IN_STATUS_NONE    = (0 << 0), /// No special status; the stream is currently active.
    PIO_STREAM_IN_STATUS_CLOSE   = (1 << 0), /// The stream is marked to be closed.
    PIO_STREAM_IN_STATUS_CLOSED  = (1 << 1), /// The stream has been closed.
    PIO_STREAM_IN_STATUS_PAUSED  = (1 << 2), /// The stream has been temporarily paused.
};

/// @summary Define the supported stream-in control command identifiers.
enum pio_stream_in_control_e : uint32_t
{
    PIO_STREAM_IN_CONTROL_PAUSE  = 0,        /// Stream loading should be paused.
    PIO_STREAM_IN_CONTROL_RESUME = 1,        /// Stream loading should be resumed from the current position.
    PIO_STREAM_IN_CONTROL_REWIND = 2,        /// Restart stream loading from the beginning of the stream.
    PIO_STREAM_IN_CONTROL_SEEK   = 3,        /// Seek to a position within the stream and start loading.
    PIO_STREAM_IN_CONTROL_STOP   = 4,        /// Stop stream loading and close the stream.

};

/// @summary Defines the data that must be specified to stream a file into memory.
struct pio_sti_request_t
{
    uintptr_t         Identifier;    /// The application-defined stream identifier.
    stream_decoder_t *StreamDecoder; /// The decoder interface that receives I/O results.
    HANDLE            Fildes;        /// The handle of the file to read from.
    size_t            SectorSize;    /// The physical disk sector size, in bytes.
    int64_t           BaseOffset;    /// The absolute offset of the start of the data within the file.
    int64_t           BaseSize;      /// The size of the data within the file, in bytes.
    uint64_t          IntervalNs;    /// The required delivery interval, in nanoseconds, or zero.
    uint32_t          StreamFlags;   /// A combination of pio_stream_in_flags_e.
    uint8_t           BasePriority;  /// The base priority of the stream.
};

/// @summary Defines the state information associated with a readable file opened
/// for stream-in style I/O. This structure is internal to the PIO driver.
struct pio_sti_state_t
{
    uint32_t          StreamFlags;   /// Flags controlling stream behavior, a combination of pio_stream_in_flags_e.
    uint32_t          SectorSize;    /// The physical disk sector size, in bytes.
    HANDLE            Fildes;        /// The file descriptor for the file.
    int64_t           BaseOffset;    /// The absolute byte offset of the start of the file data.
    int64_t           BaseSize;      /// The physical file size, in bytes.
    int64_t           ReadOffset;    /// The current read offset for the stream, in bytes.
};

/// @summary Defines the state information associated with interval-based buffer 
/// delivery for a file being streamed in. This structure is internal to the PIO driver.
struct pio_sti_delivery_t
{
    #define N         PIO_MAX_DELIVERY_BUFFERS
    uint64_t          DataInterval;  /// The buffer delivery interval, in nanoseconds.
    uint64_t          NextDeadline;  /// The next delivery time, in nanoseconds.
    size_t            HeadIndex;     /// The index of the oldest item in the queue.
    size_t            TailIndex;     /// The index of the next item to insert.
    aio_result_t      ResultList[N]; /// The queue of results to deliver.
    #undef  N
};

/// @summary Defines the data used to calculate the priority of a stream-in.
struct pio_sti_priority_t
{
    uint32_t          StreamOrder;   /// The order in which the stream was opened.
    uint32_t          BasePriority;  /// The baseline priority value of the stream.
};

/// @summary Defines the data sent with a stream-in control request used to pause, resume, 
/// seek or stop a file being streamed in to memory.
struct pio_sti_control_t
{
    uintptr_t         Identifier;    /// The application-defined stream identifier.
    int64_t           ByteOffset;    /// The byte offset to set, or 0 if unused.
    uint32_t          Command;       /// One of pio_stream_in_control_e specifying the command.
};

/// @summary Defines the data associated with an unbounded priority queue 
/// for I/O operations to be submitted to the asynchronous I/O driver. 
/// This priority queue is only accessed by the prioritized I/O driver 
/// thread and is updated on each tick.
struct pio_aio_priority_queue_t
{
    size_t            Count;         /// The number of items in the queue.
    size_t            Capacity;      /// The number of items that can be stored in the queue.
    uint64_t          InsertionId;   /// The unique index of the next item to insert.
    uint32_t         *Priority;      /// The set of I/O operation priority values.
    uint64_t         *InsertId;      /// The set of I/O operation insertion order identifiers.
    aio_request_t    *Request;       /// The set of I/O operation definitions.
};

/// @summary Defines the data associated with an unbounded priority queue 
/// for active stream-in files. This priority queue is only accessed by the
/// prioritized I/O driver thread and is regenerated each tick from the list
/// of opened, but unfinished, files being streamed in.
struct pio_sti_priority_queue_t
{
    size_t            Count;         /// The number of items in the queue.
    size_t            Capacity;      /// The number of items that can be stored in the queue.
    uint32_t         *Priority;      /// The set of computed stream priority values.
    uint32_t         *StreamOrder;   /// The set of stream start order identifiers.
    intptr_t         *StreamIndex;   /// The set of active stream-in list index values.
};

typedef fifo_allocator_t<pio_sti_request_t> pio_sti_pending_alloc_t;
typedef mpsc_fifo_u_t   <pio_sti_request_t> pio_sti_pending_queue_t;
typedef fifo_allocator_t<pio_sti_control_t> pio_sti_control_alloc_t;
typedef mpsc_fifo_u_t   <pio_sti_control_t> pio_sti_control_queue_t;

/// @summary Maintains all of the state for the prioritized I/O driver.
struct pio_driver_t
{
    aio_driver_t            *AIO;              /// The asynchronous I/O driver interface.
    pio_aio_priority_queue_t AIODriverQueue;   /// The priority queue of operations to push to the AIO driver.

    aio_result_alloc_t       SIDResultAlloc;   /// The FIFO node allocator for enqueuing interval-based delivery results.
    aio_result_queue_t       SIDResultQueue;   /// The SPSC unbounded FIFO for results from the AIO driver for interval-based delivery.

    LARGE_INTEGER            ClockFrequency;   /// The frequency of the high-resolution timer.
    uint64_t                 LastTickStart;    /// The nanosecond timestamp at which the most recent tick began.
    uint64_t                 TickHistory[8];   /// The eight most recent inter-tick intervals.
    size_t                   TickHistoryCount; /// The number of valid values in the TickHistory array.
    size_t                   TickCount;        /// The number of times pio_driver_main() has been called.
    
    uint32_t                 StreamIndex;      /// The number of streams that have been opened by the driver.
    size_t                   StreamInCount;    /// The number of active stream-in files.
    size_t                   StreamInCapacity; /// The number of stream-in files that can be stored.
    uintptr_t               *StreamInId;       /// The set of application-defined stream-in identifiers.
    uint32_t                *StreamInStatus;   /// The set of status flags associated with each stream-in file.
    pio_sti_state_t         *StreamInState;    /// The set of state data associated with each stream-in file.
    stream_decoder_t       **StreamInDecoder;  /// The set of stream decoder instances for each stream-in file.
    pio_sti_delivery_t      *StreamInDelivery; /// The set of data associated with interval-based buffer delivery.
    pio_sti_priority_t      *StreamInPriority; /// The set of priority data associated with each stream-in file.
    pio_sti_priority_queue_t STIActiveQueue;   /// The priority queue of stream-in files, rebuilt each tick.

    pio_sti_pending_queue_t  STIPendingQueue;  /// The MPSC unbounded FIFO of pending stream-in open requests.
    pio_sti_control_queue_t  STIControlQueue;  /// The MPSC unbounded FIFO of pending stream-in control requests.
};

/*///////////////
//   Globals   //
///////////////*/

/*///////////////////////
//   Local Functions   //
///////////////////////*/
/// @summary Reads the current tick count for use as a timestamp.
/// @return The current timestamp value, in nanoseconds.
internal_function inline uint64_t pio_driver_nanotime(pio_driver_t *driver)
{
    uint64_t const  SEC_TO_NANOSEC = 1000000000ULL;
    LARGE_INTEGER   counts; QueryPerformanceCounter(&counts);
    return uint64_t(SEC_TO_NANOSEC * (double(counts.QuadPart) / double(driver->ClockFrequency.QuadPart)));
}

/// @summary Perform a comparison between two elements in an I/O operation priority queue.
/// @param pq The I/O operation priority queue.
/// @param priority The priority of the item being inserted.
/// @param idx The zero-based index of the item in the queue to compare against.
/// @return -1 if item a should appear before item b, +1 if item a should appear after item b.
internal_function inline int pio_aio_priority_queue_cmp_put(pio_aio_priority_queue_t const &pq, uint32_t priority, intptr_t idx)
{   // when inserting, the new item is always ordered after the existing item
    // if the priority values of the two items are the same.
    uint32_t const p_a  = priority;
    uint32_t const p_b  = pq.Priority[idx];
    return ((p_a < p_b) ? -1 : +1);
}

/// @summary Perform a comparison between two elements in an I/O operation priority queue.
/// @param pq The I/O operation priority queue.
/// @param a The zero-based index of the first element.
/// @param b The zero-based index of the second element.
/// @return -1 if item a should appear before item b, +1 if item a should appear after item b.
internal_function inline int pio_aio_priority_queue_cmp_get(pio_aio_priority_queue_t const &pq, intptr_t a, intptr_t b)
{
    uint32_t const p_a  = pq.Priority[a];
    uint32_t const p_b  = pq.Priority[b];
    if (p_a < p_b) return -1;
    if (p_a > p_b) return +1;
    uint64_t const i_a  = pq.InsertId[a];
    uint64_t const i_b  = pq.InsertId[b];
    if (i_a < i_b) return -1;
    else           return +1; // i_a > i_b; i_a can never equal i_b.
}

/// @summary Initializes an I/O operation priority queue to empty and optionally preallocates storage.
/// @param pq The priority queue to initialize.
/// @param capacity The initial queue capacity, in items.
internal_function void pio_aio_priority_queue_create(pio_aio_priority_queue_t &pq, size_t capacity)
{   // initialize all of the priority queue fields and data to 0/NULL.
    pq.Count       = 0;
    pq.Capacity    = capacity;
    pq.InsertionId = 0;
    pq.Priority    = NULL;
    pq.InsertId    = NULL;
    pq.Request     = NULL;
    if (capacity   > 0)
    {   // pre-allocate storage for some queue items.
        pq.Priority  = (uint32_t     *) malloc(capacity * sizeof(uint32_t));
        pq.InsertId  = (uint64_t     *) malloc(capacity * sizeof(uint64_t));
        pq.Request   = (aio_request_t*) malloc(capacity * sizeof(aio_request_t));
    }
}

/// @summary Frees resources associated with an I/O operation priority queue.
/// @param pq The priority queue to delete.
internal_function void pio_aio_priority_queue_delete(pio_aio_priority_queue_t &pq)
{
    if (pq.Request  != NULL) free(pq.Request);
    if (pq.InsertId != NULL) free(pq.InsertId);
    if (pq.Priority != NULL) free(pq.Priority);
    pq.Count         = 0;
    pq.Capacity      = 0;
    pq.InsertionId   = 0;
    pq.Priority      = NULL;
    pq.InsertId      = NULL;
    pq.Request       = NULL;
}

/// @summary Resets a I/O operation priority queue to empty.
/// @param pq The priority queue to clear.
internal_function void pio_aio_priority_queue_clear(pio_aio_priority_queue_t &pq)
{
    pq.Count = 0;
    pq.InsertionId = 0;
}

/// @summary Attempts to insert an I/O operation in the priority queue.
/// @param pq The I/O operation priority queue to update.
/// @param priority The priority value associated with the item being inserted.
/// @param out_cmd If the function returns true, this location is updated with
/// the address of the AIO driver command to populate.
/// @param out_queues If the function returns true, this location is updated
/// with the address of the AIO driver command to populate.
/// @return The AIO request to populate, or NULL if the queue is full.
internal_function aio_request_t* pio_aio_priority_queue_put(pio_aio_priority_queue_t &pq, uint32_t priority)
{
    if (pq.Count == pq.Capacity)
    {   // grow all of the internal queue storage by doubling.
        size_t         nc = (pq.Capacity < 4096)  ? (pq.Capacity * 2) : (pq.Capacity + 1024);
        uint32_t      *np = (uint32_t     *) realloc(pq.Priority,  nc * sizeof(uint32_t));
        uint64_t      *ni = (uint64_t     *) realloc(pq.InsertId,  nc * sizeof(uint64_t));
        aio_request_t *nr = (aio_request_t*) realloc(pq.Request ,  nc * sizeof(aio_request_t));
        if (np != NULL)      pq.Priority  =  np;
        if (ni != NULL)      pq.InsertId  =  ni;
        if (nr != NULL)      pq.Request   =  nr;
        if (np != NULL && ni != NULL && nr != NULL)  pq.Capacity = nc;
    }
    if (pq.Count < pq.Capacity)
    {   // there's room in the queue for this operation.
        intptr_t pos = intptr_t(pq.Count++);
        intptr_t idx = intptr_t(pos - 1) / 2;
        while (pos > 0 && pio_aio_priority_queue_cmp_put(pq, priority, idx) < 0)
        {
            pq.Priority[pos] = pq.Priority[idx];
            pq.InsertId[pos] = pq.InsertId[idx];
            pq.Request [pos] = pq.Request [idx];
            pos = idx;
            idx =(idx - 1) / 2;
        }
        pq.Priority[pos] = priority;
        pq.InsertId[pos] = pq.InsertionId++;
        return &pq.Request[pos];
    }
    else return NULL;
}

/// @summary Retrieves the highest priority pending AIO driver request.
/// @param pq The I/O operation priority queue to update.
/// @param request If the queue is non-empty, the highest priority AIO driver request is copied to this location.
/// @return true if the queue is non-empty.
internal_function inline bool pio_aio_priority_queue_top(pio_aio_priority_queue_t &pq, aio_request_t &request)
{
    if (pq.Count > 0)
    {   request  = pq.Request[0];
        return true;
    }
    else return false;
}

/// @summary Removes the highest priority pending AIO command.
/// @param pq The I/O operation priority queue to update.
internal_function bool pio_aio_priority_queue_pop(pio_aio_priority_queue_t &pq)
{
    if (pq.Count > 0)
    {   // swap the last item into the position vacated by the first item.
        intptr_t     n = pq.Count - 1;
        pq.Priority[0] = pq.Priority[n];
        pq.InsertId[0] = pq.InsertId[n];
        pq.Request [0] = pq.Request [n];
        pq.Count       = n;

        // now re-heapify and restore the heap order.
        intptr_t pos = 0;
        for ( ; ; )
        {
            intptr_t l = (2 * pos) + 1; // left child
            intptr_t r = (2 * pos) + 2; // right child
            intptr_t m; // the child with the lowest frequency.

            // determine the child node with the lowest frequency.
            if  (l >= n) break; // node at pos has no children.
            if  (r >= n) m = l; // node at pos has no right child.
            else m  = pio_aio_priority_queue_cmp_get(pq, l, r) < 0 ? l : r;

            // now compare the node at pos with the highest priority child, m.
            if (pio_aio_priority_queue_cmp_get(pq, pos, m) < 0)
            {   // children have lower priority than parent. order restored.
                break;
            }

            // swap the parent with the largest child.
            uint32_t      tp = pq.Priority[pos];
            uint64_t      ti = pq.InsertId[pos];
            aio_request_t tr = pq.Request [pos];
            pq.Priority[pos] = pq.Priority[m];
            pq.InsertId[pos] = pq.InsertId[m];
            pq.Request [pos] = pq.Request [m];
            pq.Priority[m]   = tp;
            pq.InsertId[m]   = ti;
            pq.Request [m]   = tr;
            pos = m;
        }
        return true;
    }
    else return false;
}

/// @summary Perform a comparison between two elements in a stream-in priority queue.
/// @param pq The stream-in priority queue.
/// @param priority The priority of the item being inserted.
/// @param order The unique identifier assigned to the stream when it was opened.
/// @param idx The zero-based index of the item in the queue to compare against.
/// @return -1 if item a should appear before item b, +1 if item a should appear after item b.
internal_function inline int pio_sti_priority_queue_cmp_put(pio_sti_priority_queue_t const &pq, uint32_t priority, uint32_t order, intptr_t idx)
{   // the order of items in the active stream list changes as streams are closed.
    // if the priority values of the two items are the same, compare using the 
    // order in which the streams were originally started.
    uint32_t const p_a  = priority;
    uint32_t const p_b  = pq.Priority[idx];
    if (p_a < p_b) return -1;
    if (p_a > p_b) return +1;
    uint32_t const o_a  = order;
    uint32_t const o_b  = pq.StreamOrder[idx];
    if (o_a < o_b) return -1;
    else           return +1; // o_a > o_b; o_a can never equal o_b.
}

/// @summary Perform a comparison between two elements in an stream-in priority queue.
/// @param pq The stream-in priority queue.
/// @param a The zero-based index of the first element.
/// @param b The zero-based index of the second element.
/// @return -1 if item a should appear before item b, +1 if item a should appear after item b.
internal_function inline int pio_sti_priority_queue_cmp_get(pio_sti_priority_queue_t const &pq, intptr_t a, intptr_t b)
{
    uint32_t const p_a  = pq.Priority[a];
    uint32_t const p_b  = pq.Priority[b];
    if (p_a < p_b) return -1;
    if (p_a > p_b) return +1;
    uint32_t const o_a  = pq.StreamOrder[a];
    uint32_t const o_b  = pq.StreamOrder[b];
    if (o_a < o_b) return -1;
    else           return +1; // o_a > o_b; o_a can never equal o_b.
}

/// @summary Initializes a stream-in priority queue to empty and optionally preallocates storage.
/// @param pq The priority queue to initialize.
/// @param capacity The initial queue capacity, in items.
internal_function void pio_sti_priority_queue_create(pio_sti_priority_queue_t &pq, size_t capacity)
{   // initialize all of the priority queue fields and data to 0/NULL.
    pq.Count        = 0;
    pq.Capacity     = capacity;
    pq.Priority     = NULL;
    pq.StreamOrder  = NULL;
    pq.StreamIndex  = NULL;
    if (capacity  > 0)
    {   // pre-allocate storage for some queue items.
        pq.Priority    = (uint32_t*) malloc(capacity * sizeof(uint32_t));
        pq.StreamOrder = (uint32_t*) malloc(capacity * sizeof(uint32_t));
        pq.StreamIndex = (intptr_t*) malloc(capacity * sizeof(intptr_t));
    }
}

/// @summary Frees resources associated with a stream-in priority queue.
/// @param pq The priority queue to delete.
internal_function void pio_sti_priority_queue_delete(pio_sti_priority_queue_t &pq)
{
    if (pq.StreamIndex != NULL) free(pq.StreamIndex);
    if (pq.StreamOrder != NULL) free(pq.StreamOrder);
    if (pq.Priority    != NULL) free(pq.Priority);
    pq.Count            = 0;
    pq.Capacity         = 0;
    pq.Priority         = NULL;
    pq.StreamOrder      = NULL;
    pq.StreamIndex      = NULL;
}

/// @summary Resets a stream-in priority queue to empty.
/// @param pq The priority queue to clear.
internal_function void pio_sti_priority_queue_clear(pio_sti_priority_queue_t &pq)
{
    pq.Count = 0;
}

/// @summary Retrieves the highest priority pending stream-in index.
/// @param pq The stream-in priority queue to update.
/// @param stream_index On return, this location is updated with the zero-based index of the active stream in with the highest priority.
/// @param priority On return, this location is updated with the computed priority value of the stream.
/// @return true if the queue is non-empty and stream_index was updated.
internal_function inline bool pio_sti_priority_queue_top(pio_sti_priority_queue_t &pq, intptr_t &stream_index, uint32_t &priority)
{
    if (pq.Count > 0)
    {   
        priority = pq.Priority[0];
        stream_index = pq.StreamIndex[0];
        return true;
    }
    else return false;
}

/// @summary Attempts to insert a stream record in the priority queue.
/// @param pq The stream-in priority queue to update.
/// @param priority The priority value associated with the item being inserted.
/// @param order The unique identifier assigned to the stream when it was opened.
/// @param index The zero-based index of the stream in the active stream list.
internal_function void pio_sti_priority_queue_put(pio_sti_priority_queue_t &pq, uint32_t priority, uint32_t order, intptr_t index)
{
    if (pq.Count == pq.Capacity)
    {   // grow all of the internal queue storage by doubling.
        size_t      nc = (pq.Capacity  < 4096) ? (pq.Capacity   *  2) : (pq.Capacity + 1024);
        uint32_t   *np = (uint32_t  *)   realloc (pq.Priority   ,  nc * sizeof(uint32_t));
        uint32_t   *no = (uint32_t  *)   realloc (pq.StreamOrder,  nc * sizeof(uint32_t));
        intptr_t   *ni = (intptr_t  *)   realloc (pq.StreamIndex,  nc * sizeof(intptr_t));
        if (np != NULL)   pq.Priority    = np;
        if (no != NULL)   pq.StreamOrder = no;
        if (ni != NULL)   pq.StreamIndex = ni;
        if (np != NULL && no != NULL && no != NULL)  pq.Capacity = nc;
    }
    if (pq.Count < pq.Capacity)
    {   // there's room in the queue for this operation.
        intptr_t pos = intptr_t(pq.Count++);
        intptr_t idx = intptr_t(pos - 1) / 2;
        while (pos > 0 && pio_sti_priority_queue_cmp_put(pq, priority, order, idx) < 0)
        {
            pq.Priority   [pos] = pq.Priority   [idx];
            pq.StreamOrder[pos] = pq.StreamOrder[idx];
            pq.StreamIndex[pos] = pq.StreamIndex[idx];
            pos = idx;
            idx =(idx - 1) / 2;
        }
        pq.Priority   [pos] = priority;
        pq.StreamOrder[pos] = order;
        pq.StreamIndex[pos] = index;
    }
}

/// @summary Removes the highest priority stream-in index.
/// @param pq The I/O operation priority queue to update.
internal_function void pio_sti_priority_queue_pop(pio_sti_priority_queue_t &pq)
{
    if (pq.Count > 0)
    {   // swap the last item into the position vacated by the first item.
        intptr_t       n  = pq.Count - 1;
        pq.Priority   [0] = pq.Priority[n];
        pq.StreamOrder[0] = pq.StreamOrder[n];
        pq.StreamIndex[0] = pq.StreamIndex[n];
        pq.Count = n;

        // now re-heapify and restore the heap order.
        intptr_t pos = 0;
        for ( ; ; )
        {
            intptr_t l = (2 * pos) + 1; // left child
            intptr_t r = (2 * pos) + 2; // right child
            intptr_t m; // the child with the lowest frequency.

            // determine the child node with the lowest frequency.
            if  (l >= n) break; // node at pos has no children.
            if  (r >= n) m = l; // node at pos has no right child.
            else m  = pio_sti_priority_queue_cmp_get(pq, l, r) < 0 ? l : r;

            // now compare the node at pos with the highest priority child, m.
            if (pio_sti_priority_queue_cmp_get(pq, pos, m) < 0)
            {   // children have lower priority than parent. order restored.
                break;
            }

            // swap the parent with the largest child.
            uint32_t tp  = pq.Priority   [pos];
            uint32_t to  = pq.StreamOrder[pos];
            intptr_t ti  = pq.StreamIndex[pos];
            pq.Priority   [pos] = pq.Priority   [m];
            pq.StreamOrder[pos] = pq.StreamOrder[m];
            pq.StreamIndex[pos] = pq.StreamIndex[m];
            pq.Priority   [m]   = tp;
            pq.StreamOrder[m]   = to;
            pq.StreamIndex[m]   = ti;
            pos = m;
        }
    }
}

/// @summary Dequeues the results of any completed stream-out operations for a given PIO driver. This frees any resources allocated by the driver for writes.
/// @param driver The prioritized I/O driver state.
internal_function void pio_process_completed_stream_out(pio_driver_t *driver)
{
    UNREFERENCED_PARAMETER(driver);
    /*aio_result_t result;
    while (spsc_fifo_u_consume(&driver->StreamOutQueue, result))
    {
        if (result.Fildes > 0 && result.DataBuffer != NULL)
        {   // completed write. unmap the fixed-length buffer.
            munmap(result.DataBuffer, LINUX_PIO_STOBUF_SIZE);
        }
        if (result.Fildes < 0 && result.DataBuffer != NULL)
        {   // completed close. free the strdup()-allocated string.
            free(result.DataBuffer);
        }
        if (result.OSError != 0)
        {   // TODO(rlk): error handling.
        }
    }*/
}

/// @summary Ensure that the lists used to maintain state for active stream-ins in the PIO driver has at least the specified capacity. If not, grow the lists.
/// @param driver The platform I/O driver state.
/// @param capacity The minimum number of stream-in records to maintain.
/// @return true if the driver meets or exceeds the specified capacity for active stream-ins.
internal_function bool pio_sti_state_list_ensure(pio_driver_t *driver, size_t capacity)
{
    if (driver->StreamInCount == driver->StreamInCapacity || capacity > driver->StreamInCapacity)
    {   // resize the internal lists for maintaining stream-in state.
        size_t oldc = driver->StreamInCapacity;
        size_t newc = (oldc < 1024) ? (oldc * 2) : (oldc + 1024);
        if (capacity > newc)  newc  =  capacity;

        // re-allocate the existing buffers with the larger capacity.
        pio_sti_priority_t   *np    =(pio_sti_priority_t*)realloc(driver->StreamInPriority, newc * sizeof(pio_sti_priority_t));
        pio_sti_delivery_t   *nd    =(pio_sti_delivery_t*)realloc(driver->StreamInDelivery, newc * sizeof(pio_sti_delivery_t));
        stream_decoder_t    **nc    =(stream_decoder_t **)realloc(driver->StreamInDecoder , newc * sizeof(stream_decoder_t*));
        pio_sti_state_t      *ns    =(pio_sti_state_t   *)realloc(driver->StreamInState   , newc * sizeof(pio_sti_state_t));
        uint32_t             *nf    =(uint32_t          *)realloc(driver->StreamInStatus  , newc * sizeof(uint32_t));
        uintptr_t            *ni    =(uintptr_t         *)realloc(driver->StreamInId      , newc * sizeof(uintptr_t));
        if (np != NULL) driver->StreamInPriority = np;
        if (nd != NULL) driver->StreamInDelivery = nd;
        if (nc != NULL) driver->StreamInDecoder  = nc;
        if (ns != NULL) driver->StreamInState    = ns;
        if (nf != NULL) driver->StreamInStatus   = nf;
        if (ni != NULL) driver->StreamInId       = ni;
        if (np != NULL && nd != NULL && nc != NULL && ns != NULL && nf != NULL && ni != NULL)
        {   // all buffers reallocated successfully.
            driver->StreamInCapacity = newc;
            return true;
        }
        else return false;
    }
    else return true;
}

/// @summary Initializes the interval-based delivery tracking data for a stream.
/// @param sd The interval-based delivery record to initialize.
/// @param interval_ns The desired delivery interval, in nanoseconds. If zero, interval-based delivery is disabled.
/// @param now_ns The current timestamp, in nanoseconds.
internal_function void pio_sti_delivery_init(pio_sti_delivery_t *sd, uint64_t interval_ns, uint64_t now_ns)
{
    if (interval_ns == 0)
    {   // this stream-in isn't using interval-based delivery.
        sd->DataInterval = UINT64_MAX;
        sd->NextDeadline = UINT64_MAX;
        sd->HeadIndex    = 0;
        sd->TailIndex    = 0;
    }
    else
    {   // this stream-in is using interval-based delivery.
        sd->DataInterval = interval_ns;
        sd->NextDeadline = interval_ns + now_ns;
        sd->HeadIndex    = 0;
        sd->TailIndex    = 0;
    }
}

/// @summary Pushes an asynchronous I/O result onto the delivery queue. If the 
/// result indicates anything other than a successful read, it is immediately 
/// pushed to the stream decoder for processing.
/// @param sd The interval-based delivery state for the stream.
/// @param result The result of the asynchronous I/O operation.
/// @param decoder The stream decoder that receives the I/O results.
internal_function void pio_sti_delivery_push(pio_sti_delivery_t *sd, aio_result_t const &result, stream_decoder_t *decoder)
{
    if (SUCCEEDED(result.OSError) && result.DataBuffer != NULL && result.DataActual > 0)
    {   // enqueue the data buffer for delivery at a later time.
        sd->ResultList[((sd->TailIndex++) & (PIO_MAX_DELIVERY_BUFFERS-1))] = result;
    }
    else
    {   // errors, closes, and so on are reported to the decoder immediately.
        fifo_node_t<aio_result_t> *n = fifo_allocator_get(&decoder->AIOResultAlloc);
        n->Item = result;
        spsc_fifo_u_produce(&decoder->AIOResultQueue, n);
        decoder->addref();
    }
}

/// @summary Determines whether the next successful read should be delivered to 
/// the stream decoder, and if so, delivers the data buffer to the decoder.
/// @param sd The interval-based delivery state for the stream.
/// @param now_ns The start time of the current tick, in nanoseconds.
/// @param tick_ns The average interval between ticks, in nanoseconds.
/// @param decoder The stream decoder that receives the I/O results.
internal_function void pio_sti_delivery_next(pio_sti_delivery_t *sd, uint64_t now_ns, uint64_t tick_ns, stream_decoder_t *decoder)
{   // determine whether it's time to deliver the next result:
    if ((now_ns + tick_ns) >= sd->NextDeadline && (sd->HeadIndex != sd->TailIndex))
    {   // there's a result buffered to be delivered within the next tick.
        fifo_node_t<aio_result_t> *n = fifo_allocator_get(&decoder->AIOResultAlloc);
        n->Item = sd->ResultList[((sd->HeadIndex++) & (PIO_MAX_DELIVERY_BUFFERS-1))];
        spsc_fifo_u_produce(&decoder->AIOResultQueue, n);
        sd->NextDeadline += sd->DataInterval;
        decoder->addref();
    }
}

/// @summary Submit buffered commands from the prioritized I/O command list to the asynchronous I/O driver.
/// @param driver The prioritized I/O driver maintaining the buffered I/O command list.
/// @return true if the asynchronous I/O driver queue still has space.
internal_function bool pio_driver_submit_to_aio(pio_driver_t *driver)
{
    aio_driver_t             *aio       = driver->AIO;
    pio_aio_priority_queue_t &aio_queue = driver->AIODriverQueue;
    aio_request_t             aio_op;
    while (pio_aio_priority_queue_top(aio_queue, aio_op))
    {
        if (aio_driver_submit(aio, aio_op))
        {   // command submitted successfully; keep processing.
            pio_aio_priority_queue_pop(aio_queue);
        }
        else
        {   // the AIO driver queue is full; halt submission.
            return false;
        }
    }
    return true; // maybe still space in the AIO driver queue.
}

/// @summary Implements the per-tick entry point for the prioritized I/O driver. Ultimately, this builds a priority queue of pending I/O operations and pushes them down to the asynchronous I/O driver for processing.
/// @param driver The prioritized I/O driver state.
internal_function void pio_driver_main(pio_driver_t *driver)
{   // update timing data required for interval-based delivery.
    uint64_t tick_time    = pio_driver_nanotime(driver);           // start of this tick
    uint64_t time_elapsed = tick_time - driver->LastTickStart;     // time between ticks
    driver->TickHistory[(driver->TickCount++) & 7] = time_elapsed; // save intra-tick time
    if (driver->TickHistoryCount < 8) driver->TickHistoryCount++;  // update number of valid values
    // now calculate the average tick duration, in nanoseconds.
    uint64_t tick_time_s  = 0;
    uint64_t tick_time_a  = 0;
    for (size_t i = 0; i  < driver->TickHistoryCount; ++i)
    {
        tick_time_s += driver->TickHistory[i];
    }
    tick_time_a = tick_time_s / driver->TickHistoryCount;
    driver->LastTickStart = tick_time;

    // process all completion events received from the AIO driver.
    // pio_process_completed_stream_out(pio);

    // dequeue all completed interval-based delivery results from the AIO driver.
    aio_result_t aio_interval;
    while (spsc_fifo_u_consume(&driver->SIDResultQueue, aio_interval))
    {
        for (size_t i = 0,  n = driver->StreamInCount; i < n; ++i)
        {
            if (driver->StreamInId[i] == aio_interval.Identifier)
            {   // found the matching stream, enqueue result.
                stream_decoder_t   *sc = driver->StreamInDecoder [i];
                pio_sti_delivery_t *sd =&driver->StreamInDelivery[i];
                pio_sti_delivery_push(sd, aio_interval, sc);
                break;
            }
        }
    }
    // push buffers to stream decoders for any interval-based delivery.
    for (size_t i = 0, n = driver->StreamInCount; i < n; ++i)
    {
        stream_decoder_t   *sc =  driver->StreamInDecoder [i];
        pio_sti_delivery_t *sd = &driver->StreamInDelivery[i];
        pio_sti_delivery_next(sd, tick_time, tick_time_a, sc);
    }

    // push all pending stream-in closes directly to the AIO driver.
    for (size_t i = 0, n = driver->StreamInCount; i < n; ++i)
    {   
        uint32_t   flags = driver->StreamInStatus[i];
        // check for streams with a 'close pending' status.
        if (flags & PIO_STREAM_IN_STATUS_CLOSE)
        {   // push a stream close operation to the AIO driver.
            uintptr_t         id= driver->StreamInId     [i];
            pio_sti_state_t  &si= driver->StreamInState  [i];
            stream_decoder_t *sc= driver->StreamInDecoder[i];
            aio_request_t aio_op;
            aio_op.CommandType  = AIO_COMMAND_CLOSE;
            aio_op.Fildes       = si.Fildes;
            aio_op.DataAmount   = 0;
            aio_op.DataActual   = 0;
            aio_op.BaseOffset   = si.BaseOffset;
            aio_op.FileOffset   = si.ReadOffset;
            aio_op.DataBuffer   = NULL;
            aio_op.Identifier   = id;
            aio_op.StatusFlags  = STREAM_DECODE_STATUS_ENDOFSTREAM;
            aio_op.Priority     = 0;
            aio_op.ResultAlloc  =&sc->AIOResultAlloc;
            aio_op.ResultQueue  =&sc->AIOResultQueue;
            if (aio_driver_submit(driver->AIO, aio_op))
            {   // mark the stream as having been closed. it will be
                // removed from the active list in the loop below.
                flags &= ~PIO_STREAM_IN_STATUS_CLOSE;
                flags |=  PIO_STREAM_IN_STATUS_CLOSED;
                driver->StreamInStatus[i] = flags;
                // addref the decoder to account for the outstanding close.
                sc->addref();
            }
            else return; // the AIO driver queue is full. nothing to do.
        }
    }

    // remove all closed streams from the active stream-in list.
    for (size_t i = 0; i < driver->StreamInCount; /* empty */)
    {
        if (driver->StreamInStatus[i] & PIO_STREAM_IN_STATUS_CLOSED)
        {   // the prioritized I/O driver no longer needs access to the decoder.
            driver->StreamInDecoder[i]->release();
            // swap the last item in the list into slot 'i'.
            size_t  last_index          = driver->StreamInCount - 1;
            driver->StreamInPriority[i] = driver->StreamInPriority[last_index];
            driver->StreamInDelivery[i] = driver->StreamInDelivery[last_index];
            driver->StreamInDecoder [i] = driver->StreamInDecoder [last_index];
            driver->StreamInState   [i] = driver->StreamInState   [last_index];
            driver->StreamInStatus  [i] = driver->StreamInStatus  [last_index];
            driver->StreamInId      [i] = driver->StreamInId      [last_index];
            driver->StreamInCount       = last_index;
        }
        else ++i; // check the next stream.
    }

    // push all pending explicit I/O requests to the I/O operations queue.
    // these requests will be prioritized with the other requests.
    /*while (mpsc_fifo_u_consume(&pio->ExplicitAioQueue, explicit_aio))
    {   // attempt to allocate a request in the I/O operations queue.
        aio_request = pio_aio_driver_queue_put(pio->IoRequests, explicit_aio.Priority);
        if (aio_request != NULL)
        {   // copy the data for the request that was just dequeued.
            *aio_request = explicit_aio;
        }
        else goto push_io_ops_to_aio_driver;
    }*/

    // dequeue all pending stream-in opens and insert them into the list of active stream-ins.
    pio_sti_request_t  openrq;
    while (mpsc_fifo_u_consume(&driver->STIPendingQueue, openrq))
    {
        size_t  list_index = driver->StreamInCount;
        pio_sti_state_list_ensure(driver, driver->StreamInCount + 1);
        
        // data typically accessed sequentially during a loop.
        driver->StreamInId     [list_index] = openrq.Identifier;
        driver->StreamInStatus [list_index] = PIO_STREAM_IN_STATUS_NONE;
        driver->StreamInDecoder[list_index] = openrq.StreamDecoder;

        // data typically accessed together when submitting an I/O operation.
        pio_sti_state_t &si = driver->StreamInState[list_index];
        si.StreamFlags   = openrq.StreamFlags;
        si.SectorSize    =(uint32_t) openrq.SectorSize;
        si.Fildes        = openrq.Fildes;
        si.BaseOffset    = openrq.BaseOffset;
        si.BaseSize      = openrq.BaseSize;
        si.ReadOffset    = 0;

        // data required for interval-based delivery.
        pio_sti_delivery_t *sd = &driver->StreamInDelivery[list_index];
        pio_sti_delivery_init(sd, openrq.IntervalNs, tick_time);

        // data required for stream prioritization.
        pio_sti_priority_t &sp =  driver->StreamInPriority[list_index];
        sp.BasePriority  = openrq.BasePriority;
        sp.StreamOrder   = driver->StreamIndex++;

        driver->StreamInCount++;
    }

    // dequeue all pending stream-in control operations and update the stream status.
    pio_sti_control_t  control;
    while (mpsc_fifo_u_consume(&driver->STIControlQueue, control))
    {   // search for the stream-in by application-defined ID.
        uintptr_t const  sid = control.Identifier;
        for (size_t i = 0, n = driver->StreamInCount; i < n; ++i)
        {
            if (driver->StreamInId[i] == sid)
            {   // this is the stream associated with the control operation.
                switch (control.Command)
                {
                case PIO_STREAM_IN_CONTROL_PAUSE:
                    driver->StreamInStatus[i] |= PIO_STREAM_IN_STATUS_PAUSED;
                    break;
                case PIO_STREAM_IN_CONTROL_RESUME:
                    driver->StreamInStatus[i] &=~PIO_STREAM_IN_STATUS_PAUSED;
                    break;
                case PIO_STREAM_IN_CONTROL_REWIND:
                    driver->StreamInStatus[i] &=~PIO_STREAM_IN_STATUS_PAUSED;
                    driver->StreamInState [i].ReadOffset = 0;
                    break;
                case PIO_STREAM_IN_CONTROL_SEEK:
                    {   // reads can only start at even multiples of the disk sector size.
                        size_t     sector_size  = (size_t) driver->StreamInState[i].SectorSize;
                        if ((control.ByteOffset & (sector_size - 1)) != 0)
                        {   // round up to the nearest sector size multiple.
                            // then, subtract the sector size to get the next lowest multiple.
                            control.ByteOffset  =  align_up(control.ByteOffset, sector_size);
                            control.ByteOffset -=  sector_size;
                        }
                    }
                    driver->StreamInStatus[i] &=~PIO_STREAM_IN_STATUS_PAUSED;
                    driver->StreamInState [i].ReadOffset = control.ByteOffset;
                    break;
                case PIO_STREAM_IN_CONTROL_STOP:
                    driver->StreamInStatus[i] |= PIO_STREAM_IN_STATUS_CLOSE;
                    break;
                default: /* unrecognized control command */
                    break;
                }
                break;
            }
        }
    }

    // update the overall priority value for the stream, and populate the priority queue.
    // the priority queue is completely rebuilt each tick to account for added streams.
    pio_sti_priority_queue_clear(driver->STIActiveQueue);
    for (size_t i = 0, n = driver->StreamInCount; i < n; ++i)
    {
        if (driver->StreamInStatus[i] == PIO_STREAM_IN_STATUS_NONE)
        {   // this stream is still actively performing I/O operations.
            pio_sti_priority_t const &priority = driver->StreamInPriority[i];
            pio_sti_priority_queue_put(
                driver->STIActiveQueue, 
                priority.BasePriority , 
                priority.StreamOrder  , 
                (intptr_t) i);
        }
    }

    // generate stream-in read operations for active streams in priority order.
    // this loop will continue to generate operations for the highest-priority
    // stream until one of the following conditions is satisfied:
    // 1. There are no more active stream-in files.
    // 2. The stream is out of buffer space.
    // 3. The AIO driver queue is full.
    for ( ; ; )
    {
        intptr_t index;
        uint32_t priority;
        bool     end_of_stream = false;
        if (pio_sti_priority_queue_top(driver->STIActiveQueue, index, priority))
        {   // attempt to reserve a buffer for the read operation.
            stream_decoder_t *sc = driver->StreamInDecoder[index];
            void *rdbuf  =    sc->BufferAllocator->get_buffer();
            if   (rdbuf == NULL)
            {   // no buffer space. pop this stream and continue with the next.
                pio_sti_priority_queue_pop(driver->STIActiveQueue);
                continue;
            }

            // calculate the file offset after this read operation has completed, 
            // and determine if this read reaches or exceeds the end-of-file offset.
            pio_sti_state_t  &si  = driver->StreamInState[index];
            uint32_t data_actual  = 0;
            uint32_t data_amount  =(uint32_t) sc->BufferAllocator->AllocSize;
            uint32_t stream_flags = si.StreamFlags;
            HANDLE   fd           = si.Fildes;
            int64_t  file_size    = si.BaseSize;
            int64_t  base_offset  = si.BaseOffset;
            int64_t  start_offset = si.ReadOffset;
            int64_t  final_offset = si.ReadOffset + data_amount;
            uint32_t status_flags = STREAM_DECODE_STATUS_NONE;
            uint32_t close_flags  = AIO_CLOSE_FLAGS_NONE;

            if (final_offset  < file_size)
            {   // this read operation does not reach or exceed the end of the file.
                si.ReadOffset = final_offset;
                end_of_stream = false;
                data_actual   = data_amount;
            }
            else
            {   // this read operation reaches the end of the file.
                // compute the amount of data that's actually valid,
                // even though the read operation may transfer more.
                pio_sti_priority_queue_pop(driver->STIActiveQueue);
                data_actual   = uint32_t(file_size - start_offset);
                end_of_stream = true;
                if (stream_flags  & PIO_STREAM_IN_FLAGS_LOAD)
                {   // this is a load-once stream; mark it pending close.
                    driver->StreamInStatus[index] |= PIO_STREAM_IN_STATUS_CLOSED;
                    status_flags |= STREAM_DECODE_STATUS_ENDOFSTREAM;
                    close_flags  |= AIO_CLOSE_ON_COMPLETE;
                    si.ReadOffset = final_offset;
                }
                else
                {   // this is a controlled stream, so loop back to the beginning.
                    status_flags |= STREAM_DECODE_STATUS_RESTART;
                    si.ReadOffset = 0;
                }
            }

            // fill out the request to the asynchronous I/O driver.
            // the AIO driver holds a reference to the stream decoder
            // queues. this reference is released when the I/O buffer 
            // is returned (which happens during the decoding process.)
            aio_request_t *aio_read = pio_aio_priority_queue_put(driver->AIODriverQueue, priority);
            aio_read->CommandType   = AIO_COMMAND_READ;
            aio_read->CloseFlags    = close_flags;
            aio_read->Fildes        = fd;
            aio_read->DataAmount    = data_amount;
            aio_read->DataActual    = data_actual;
            aio_read->BaseOffset    = base_offset;
            aio_read->FileOffset    = start_offset;
            aio_read->DataBuffer    = rdbuf;
            aio_read->Identifier    = driver->StreamInId[index];
            aio_read->StatusFlags   = status_flags;
            aio_read->Priority      = priority;
            aio_read->ResultAlloc   =&sc->AIOResultAlloc;
            aio_read->ResultQueue   =&sc->AIOResultQueue;
            sc->addref();

            // if end-of-stream was reached, continue with the next file.
            if (end_of_stream)
            {   // flush out to the asynchronous I/O driver. if the AIO 
                // driver queue is full, end the update; otherwise, 
                // continue on to the next highest-priority stream.
                if (pio_driver_submit_to_aio(driver)) continue;
                else return;
            }
        }
        else break; // no more active streams.
    }

    // push as many pending operations to the AIO driver as possible.
    pio_driver_submit_to_aio(driver);
}

/*////////////////////////
//   Public Functions   //
////////////////////////*/
/// @summary Allocates resources for a prioritized I/O driver.
/// @param driver The prioritized I/O driver state to initialize.
/// @param aio The asynchronous I/O driver to use for asynchronous operations.
/// @return ERROR_SUCCESS on success, or an error code returned by GetLastError() on failure.
public_function DWORD pio_driver_open(pio_driver_t *driver, aio_driver_t *aio)
{   // store the asynchronous I/O driver reference and initialize the priority
    // queue used to push commands to the driver in priority order.
    driver->AIO = aio;
    pio_aio_priority_queue_create(driver->AIODriverQueue, 128);
    aio_create_result_queue(&driver->SIDResultQueue, &driver->SIDResultAlloc);
    // retrieve the counts-per-second frequency of the high-resolution clock.
    // this is used for updating files being streamed in at specific intervals.
    QueryPerformanceFrequency(&driver->ClockFrequency);
    driver->LastTickStart    = pio_driver_nanotime(driver);
    memset(driver->TickHistory,0,sizeof(driver->TickHistory));
    driver->TickHistoryCount = 0;
    driver->TickCount        = 0;
    // initialize the list of files being actively streamed in.
    driver->StreamIndex      = 0;
    driver->StreamInCount    = 0;
    driver->StreamInCapacity = 0;
    driver->StreamInId       = NULL;
    driver->StreamInStatus   = NULL;
    driver->StreamInState    = NULL;
    driver->StreamInDecoder  = NULL;
    driver->StreamInDelivery = NULL;
    driver->StreamInPriority = NULL;
    pio_sti_state_list_ensure(driver, 128);
    pio_sti_priority_queue_create(driver->STIActiveQueue, 128);
    // initialize the queue for pushing stream-in open requests to the driver.
    mpsc_fifo_u_init(&driver->STIPendingQueue);
    // initialize the queue for pushing stream-in control requests to the driver.
    mpsc_fifo_u_init(&driver->STIControlQueue);
    return ERROR_SUCCESS;
}

/// @summary Closes a prioritized I/O driver and frees associated resources.
/// @param driver The prioritized I/O driver state to clean up.
public_function void pio_driver_close(pio_driver_t *driver)
{
    mpsc_fifo_u_delete(&driver->STIControlQueue);
    mpsc_fifo_u_delete(&driver->STIPendingQueue);
    pio_sti_priority_queue_delete(driver->STIActiveQueue);
    free(driver->StreamInPriority);
    free(driver->StreamInDelivery);
    free(driver->StreamInDecoder);
    free(driver->StreamInState);
    free(driver->StreamInStatus);
    free(driver->StreamInId);
    driver->StreamIndex      = 0;
    driver->StreamInCount    = 0;
    driver->StreamInCapacity = 0;
    driver->StreamInId       = NULL;
    driver->StreamInStatus   = NULL;
    driver->StreamInState    = NULL;
    driver->StreamInDecoder  = NULL;
    driver->StreamInDelivery = NULL;
    driver->StreamInPriority = NULL;
    aio_delete_result_queue(&driver->SIDResultQueue, &driver->SIDResultAlloc);
    pio_aio_priority_queue_delete(driver->AIODriverQueue);
    driver->AIO = NULL;
}

/// @summary Queues a file to be streamed in. 
/// @param driver The prioritized I/O driver managing the stream.
/// @param request The stream-in request specifying information about the file.
/// @param thread_alloc The FIFO node allocator used for submitting commands from the calling thread.
public_function void pio_driver_stream_in(pio_driver_t *driver, pio_sti_request_t const &request, pio_sti_pending_alloc_t *thread_alloc)
{   // post the request to start the file streaming into memory.
    // this request will be picked up in pio_driver_main().
    fifo_node_t<pio_sti_request_t> *n = fifo_allocator_get(thread_alloc);
    n->Item = request;
    n->Item.StreamDecoder->addref();
    mpsc_fifo_u_produce(&driver->STIPendingQueue, n);
}

/// @summary Pauses reading of a given stream. Reading of the stream can be resumed at a later point in time, from the same location, by sending a resume command.
/// @param driver The prioritized I/O driver managing the stream.
/// @param id The application-defined identifier of the stream.
/// @param thread_alloc The FIFO node allocator used for submitting commands from the calling thread.
public_function void pio_driver_pause_stream(pio_driver_t *driver, uintptr_t id, pio_sti_control_alloc_t *thread_alloc)
{
    fifo_node_t<pio_sti_control_t> *n = fifo_allocator_get(thread_alloc);
    n->Item.Identifier = id;
    n->Item.ByteOffset = 0;
    n->Item.Command    = PIO_STREAM_IN_CONTROL_PAUSE;
    mpsc_fifo_u_produce(&driver->STIControlQueue, n);
}

/// @summary Resumes reading a paused stream from the pause location.
/// @param driver The prioritized I/O driver managing the stream.
/// @param id The application-defined identifier of the stream.
/// @param thread_alloc The FIFO node allocator used for submitting commands from the calling thread.
public_function void pio_driver_resume_stream(pio_driver_t *driver, uintptr_t id, pio_sti_control_alloc_t *thread_alloc)
{
    fifo_node_t<pio_sti_control_t> *n = fifo_allocator_get(thread_alloc);
    n->Item.Identifier = id;
    n->Item.ByteOffset = 0;
    n->Item.Command    = PIO_STREAM_IN_CONTROL_RESUME;
    mpsc_fifo_u_produce(&driver->STIControlQueue, n);
}

/// @summary Resumes reading a paused stream from the beginning of the stream.
/// @param driver The prioritized I/O driver managing the stream.
/// @param id The application-defined identifier of the stream.
/// @param thread_alloc The FIFO node allocator used for submitting commands from the calling thread.
public_function void pio_driver_rewind_stream(pio_driver_t *driver, uintptr_t id, pio_sti_control_alloc_t *thread_alloc)
{
    fifo_node_t<pio_sti_control_t> *n = fifo_allocator_get(thread_alloc);
    n->Item.Identifier = id;
    n->Item.ByteOffset = 0;
    n->Item.Command    = PIO_STREAM_IN_CONTROL_REWIND;
    mpsc_fifo_u_produce(&driver->STIControlQueue, n);
}

/// @summary Stops reading a stream and closed the underlying file handle.
/// @param driver The prioritized I/O driver managing the stream.
/// @param id The application-defined identifier of the stream.
/// @param thread_alloc The FIFO node allocator used for submitting commands from the calling thread.
public_function void pio_driver_stop_stream(pio_driver_t *driver, uintptr_t id, pio_sti_control_alloc_t *thread_alloc)
{
    fifo_node_t<pio_sti_control_t> *n = fifo_allocator_get(thread_alloc);
    n->Item.Identifier = id;
    n->Item.ByteOffset = 0;
    n->Item.Command    = PIO_STREAM_IN_CONTROL_STOP;
    mpsc_fifo_u_produce(&driver->STIControlQueue, n);
}

/// @summary Resumes reading a stream from the specified byte offset.
/// @param driver The prioritized I/O driver managing the stream.
/// @param id The application-defined identifier of the stream.
/// @param offset The byte offset from which reading will resume.
/// @param thread_alloc The FIFO node allocator used for submitting commands from the calling thread.
public_function void pio_driver_seek_stream(pio_driver_t *driver, uintptr_t id, int64_t offset, pio_sti_control_alloc_t *thread_alloc)
{
    fifo_node_t<pio_sti_control_t> *n = fifo_allocator_get(thread_alloc);
    n->Item.Identifier = id;
    n->Item.ByteOffset = offset;
    n->Item.Command    = PIO_STREAM_IN_CONTROL_SEEK;
    mpsc_fifo_u_produce(&driver->STIControlQueue, n);
}

/// @summary Executes a single AIO driver update in polling mode. The kernel AIO system will be polled for completed events without blocking. This dispatches completed command results to their associated target queues and launches as many not started commands as possible.
/// @param driver The AIO driver state to poll.
public_function void pio_driver_poll(pio_driver_t *driver)
{
    pio_driver_main(driver);
}
