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
    uint32_t          StatusFlags;   /// The stream status, a combination of pio_stream_in_status_e.
    uint32_t          StreamFlags;   /// Flags controlling stream behavior, a combination of pio_stream_in_flags_e.
    HANDLE            Fildes;        /// The file descriptor for the file.
    int64_t           BaseOffset;    /// The absolute byte offset of the start of the file data.
    int64_t           BaseSize;      /// The physical file size, in bytes.
    int64_t           ReadOffset;    /// The current read offset for the stream, in bytes.
    stream_decoder_t *StreamDecoder; /// The stream decoder instance for this specific stream.
};

/// @summary Defines the data used to calculate the priority of a stream-in.
struct pio_sti_priority_t
{
    uint32_t          StreamOrder;   /// The order in which the stream was opened.
    uint32_t          BasePriority;  /// The baseline priority value of the stream.
    uint64_t          DataInterval;  /// The data delivery interval, in nanoseconds, or 0.
    uint64_t          NextDeadline;  /// The nanosecond timestamp of the next delivery deadline.
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
    pio_aio_priority_queue_t AIODriverQueue;   /// 

    LARGE_INTEGER            ClockFrequency;   /// The frequency of the high-resolution timer.
    
    uint32_t                 StreamIndex;      /// The number of streams that have been opened by the driver.
    size_t                   StreamInCount;    /// The number of active stream-in files.
    size_t                   StreamInCapacity; /// The number of stream-in files that can be stored.
    uintptr_t               *StreamInId;       /// The set of application-defined stream-in identifiers.
    pio_sti_state_t         *StreamInState;    /// The set of state data associated with each stream-in file.
    pio_sti_priority_t      *StreamInPriority; /// The set of priority data associated with each stream-in file.
    pio_sti_priority_queue_t STIActiveQueue;   /// The priority queue of stream-in files, rebuilt each tick.

    pio_sti_pending_alloc_t  STIPendingAlloc;  /// 
    pio_sti_pending_queue_t  STIPendingQueue;  /// 

    pio_sti_control_alloc_t  STIControlAlloc;  /// 
    pio_sti_control_queue_t  STIControlQueue;  /// 
};
// So, we need the following:
// 1. A priority queue for submitting I/O operations to the AIO driver.
// 2. A priority queue for ordering stream ins.
// 3. A list of data associated with each active stream in.
// 4. A MPSC unbounded FIFO for enqueueing stream ins.
// 5. A MPSC unbounded FIFO for enqueueing explicit I/O operations.
// 6. A MPSC unbounded FIFO for enqueueing stream in control operations.


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
    uint64_t const SEC_TO_NANOSEC = 1000000000ULL;
    LARGE_INTEGER  counts; QueryPerformanceCounter(&counts);
    return (SEC_TO_NANOSEC * (uint64_t(counts.QuadPart) / uint64_t(driver->ClockFrequency.QuadPart)));
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

/// @summary Calculate a unified priority value from stream priority parameters.
/// @param data The stream priority parameters.
/// @param now_time The time at which the current driver tick start, in nanoseconds.
/// @return An overall priority value for the stream.
internal_function inline uint32_t pio_stream_in_priority(pio_sti_priority_t const &data, uint64_t now_time)
{
    uint32_t const NEED_NOW = 4000000;                    // due immediately cutoff, in nanoseconds.
    uint32_t basep = data.BasePriority * (INT_MAX / 256); // at most, INT_MAX.
    int64_t  delta = data.NextDeadline - now_time;       // how long until next delivery deadline - may be negative.
    uint32_t boost = delta >= NEED_NOW ? (INT_MAX / delta) * NEED_NOW : INT_MAX; // deadline boost similar to 1/x.
    return  (basep + boost);
}

/// @summary Dequeues the results of any completed stream-out operations for a given PIO driver. This frees any resources allocated by the driver for writes.
/// @param driver The prioritized I/O driver state.
internal_function void pio_process_completed_stream_out(pio_driver_t *driver)
{
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
        if (capacity > newc)  newc =  capacity;

        // re-allocate the existing buffers with the larger capacity.
        pio_sti_priority_t   *np   = (pio_sti_priority_t*)realloc(driver->StreamInPriority, newc * sizeof(pio_sti_priority_t));
        pio_sti_state_t      *ns   = (pio_sti_state_t   *)realloc(driver->StreamInState   , newc * sizeof(pio_sti_state_t));
        uintptr_t            *ni   = (uintptr_t         *)realloc(driver->StreamInId      , newc * sizeof(uintptr_t));
        if (np != NULL) driver->StreamInPriority = np;
        if (ns != NULL) driver->StreamInState    = ns;
        if (ni != NULL) driver->StreamInId       = ni;
        if (np != NULL && ns != NULL && ni != NULL)
        {   // all buffers reallocated successfully.
            driver->StreamInCapacity = newc;
            return true;
        }
        else return false;
    }
    else return true;
}

/// @summary Implements the per-tick entry point for the prioritized I/O driver. Ultimately, this builds a priority queue of pending I/O operations and pushes them down to the asynchronous I/O driver for processing.
/// @param driver The prioritized I/O driver state.
/// @return Zero to continue running, or non-zero if a shutdown signal was received.
internal_function int pio_driver_main(pio_driver_t *driver)
{
    pio_sti_request_t  openrq;
    aio_request_t      explicit_aio, aio_op;
    aio_request_t     *aio_request = NULL;
    uint64_t           tick_time   = pio_driver_nanotime(driver);
    size_t             i, n;

    // process all completion events received from the AIO driver.
    // pio_process_completed_stream_out(pio);

    // remove all closed streams from the active stream-in list.
    for (i = 0; i < driver->StreamInCount; /* empty */)
    {
        if (driver->StreamInState[i].StatusFlags & PIO_STREAM_IN_STATUS_CLOSED)
        {   // the prioritized I/O driver no longer needs access to the decoder.
            driver->StreamInState[i].StreamDecoder->release();
            // swap the last item in the list into this slot.
            driver->StreamInPriority[i] = driver->StreamInPriority[driver->StreamInCount-1];
            driver->StreamInState[i]    = driver->StreamInState[driver->StreamInCount-1];
            driver->StreamInId[i]       = driver->StreamInId[driver->StreamInCount-1];
            driver->StreamInCount--;
        }
        else ++i; // check the next stream.
    }

    // push all pending stream-in closes directly to the AIO driver.
    for (i = 0, n = driver->StreamInCount; i < n; ++i)
    {   // check for streams with a 'close pending' status.
        if (driver->StreamInState[i].StatusFlags & PIO_STREAM_IN_STATUS_CLOSE)
        {   // addref the stream decoder to account for the pending AIO operation.
            driver->StreamInState[i].StreamDecoder->addref();
            // push a stream close operation to the AIO driver.
            aio_op.CommandType  = AIO_COMMAND_CLOSE;
            aio_op.Fildes       = driver->StreamInState[i].Fildes;
            aio_op.DataAmount   = 0;
            aio_op.BaseOffset   = driver->StreamInState[i].BaseOffset;
            aio_op.FileOffset   = driver->StreamInState[i].ReadOffset;
            aio_op.DataBuffer   = NULL;
            aio_op.Identifier   = driver->StreamInId[i];
            aio_op.StatusFlags  = STREAM_DECODE_STATUS_ENDOFSTREAM;
            aio_op.Priority     = 0;
            aio_op.ResultAlloc  =&driver->StreamInState[i].StreamDecoder->AIOResultAlloc;
            aio_op.ResultQueue  =&driver->StreamInState[i].StreamDecoder->AIOResultQueue;
            if (aio_driver_submit(driver->AIO, aio_op))
            {   // mark the stream as having been closed.
                driver->StreamInState[i].StatusFlags &= ~PIO_STREAM_IN_STATUS_CLOSE;
                driver->StreamInState[i].StatusFlags |=  PIO_STREAM_IN_STATUS_CLOSED;
            }
            else return 0; // the AIO driver queue is full. nothing to do.
        }
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
    while (mpsc_fifo_u_consume(&driver->STIPendingQueue, openrq))
    {
        size_t i = driver->StreamInCount;
        pio_sti_state_list_ensure(driver, driver->StreamInCount + 1);
        driver->StreamInId[i]   = openrq.Identifier;
        driver->StreamInState[i].StatusFlags     = PIO_STREAM_IN_STATUS_NONE;
        driver->StreamInState[i].StreamFlags     = openrq.StreamFlags;
        driver->StreamInState[i].Fildes          = openrq.Fildes;
        driver->StreamInState[i].BaseOffset      = openrq.BaseOffset;
        driver->StreamInState[i].BaseSize        = openrq.BaseSize;
        driver->StreamInState[i].ReadOffset      = 0;
        driver->StreamInState[i].StreamDecoder   = openrq.StreamDecoder;
        driver->StreamInPriority[i].BasePriority = openrq.BasePriority;
        driver->StreamInPriority[i].DataInterval = openrq.IntervalNs;
        driver->StreamInPriority[i].NextDeadline = tick_time + openrq.IntervalNs;
        driver->StreamInPriority[i].StreamOrder  = driver->StreamIndex++;
        driver->StreamInCount++;
    }

    // update the overall priority value for the stream, and populate the priority queue.
    pio_sti_priority_queue_clear(driver->STIActiveQueue);
    for (i = 0, n = driver->StreamInCount; i < n; ++i)
    {
        uint32_t pv = pio_stream_in_priority(driver->StreamInPriority[i], tick_time);
        uint32_t so = driver->StreamInPriority[i].StreamOrder;
        intptr_t si =(intptr_t)   i;
        if (driver->StreamInState[i].StatusFlags == PIO_STREAM_IN_STATUS_NONE)
        {   // this stream is still actively performing I/O operations.
            pio_sti_priority_queue_put(driver->STIActiveQueue, pv, so, si);
        }
    }

    // generate stream-in read operations for active streams in priority order.
    // this loop will continue to generate operations for the highest-priority
    // stream until one of the following conditions is satisfied:
    // 1. The AIO driver queue is full.
    // 2. There are no more active stream-in files.
    // 3. The stream is out of buffer space, in which case we move to the next stream.
    for ( ; ; )
    {
        intptr_t index;
        uint32_t priority;
        bool     end_of_stream = false;
        if (pio_sti_priority_queue_top(driver->STIActiveQueue, index, priority))
        {   // save off the computed priority value for the stream.
            // attempt to reserve a buffer for this new read operation.
            stream_decoder_t *decoder= driver->StreamInState[index].StreamDecoder;
            uint32_t bufsz = (uint32_t)decoder->BufferAllocator->AllocSize;
            void    *rdbuf =  decoder->BufferAllocator->get_buffer();
            if (rdbuf == NULL)
            {   // no buffer space. pop this stream and continue with the next.
                pio_sti_priority_queue_pop(driver->STIActiveQueue);
                continue;
            }

            // attempt to reserve a slot in the I/O operations queue.
            if ((aio_request = pio_aio_priority_queue_put(driver->AIODriverQueue, priority)) != NULL)
            {   // calculate the file offset after this read operation has completed.
                int64_t start_offset = driver->StreamInState[index].ReadOffset;
                int64_t final_offset = driver->StreamInState[index].ReadOffset + bufsz;
                driver->StreamInState[index].ReadOffset = final_offset;
                // fill out the (already enqueued) request to the AIO driver.
                aio_request->CommandType = AIO_COMMAND_READ;
                aio_request->Fildes      = driver->StreamInState[index].Fildes;
                aio_request->DataAmount  = bufsz;
                aio_request->BaseOffset  = driver->StreamInState[index].BaseOffset;
                aio_request->FileOffset  = start_offset;
                aio_request->DataBuffer  = rdbuf;
                aio_request->Identifier  = driver->StreamInId[index];
                aio_request->StatusFlags = STREAM_DECODE_STATUS_NONE; // restart, end-of-stream
                aio_request->Priority    = priority;
                aio_request->ResultAlloc =&decoder->AIOResultAlloc;
                aio_request->ResultQueue =&decoder->AIOResultQueue;
                if (start_offset == 0)
                {   // restarting the read of the stream, so tell the decoder to reset.
                    aio_request->StatusFlags |= STREAM_DECODE_STATUS_RESTART;
                }
                if (final_offset >= driver->StreamInState[index].BaseSize)
                {   // handle end-of-stream, either by restarting the stream or marking it for close.
                    if (driver->StreamInState[index].StreamFlags &  PIO_STREAM_IN_FLAGS_LOAD)
                    {   // if it's a one-shot, mark the stream as pending close.
                        driver->StreamInState[index].StatusFlags |= PIO_STREAM_IN_STATUS_CLOSE;
                        aio_request->StatusFlags |= STREAM_DECODE_STATUS_ENDOFSTREAM;
                    }
                    else
                    {   // otherwise, rewind the stream to the beginning.
                        driver->StreamInState[index].ReadOffset = 0;
                    }
                    pio_sti_priority_queue_pop(driver->STIActiveQueue);
                    end_of_stream = true;
                }
                // add a stream decoder reference for the newly enqueued operation.
                driver->StreamInState[index].StreamDecoder->addref();
                // if end-of-stream was reached, continue with the next file.
                if (end_of_stream) continue;
            }
            else
            {   // the I/O operations queue is full. return the buffer.
                decoder->BufferAllocator->put_buffer(rdbuf);
                goto push_io_ops_to_aio_driver;
            }
        }
        else goto push_io_ops_to_aio_driver; // no more active streams.
    }

push_io_ops_to_aio_driver:
    while (pio_aio_priority_queue_top(driver->AIODriverQueue, aio_op))
    {   if (aio_driver_submit(driver->AIO, aio_op))
            pio_aio_priority_queue_pop(driver->AIODriverQueue);
    }
    return 0;
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
    // retrieve the counts-per-second frequency of the high-resolution clock.
    // this is used for updating files being streamed in at specific intervals.
    QueryPerformanceFrequency(&driver->ClockFrequency);
    // initialize the list of files being actively streamed in.
    driver->StreamIndex      = 0;
    driver->StreamInCount    = 0;
    driver->StreamInCapacity = 0;
    driver->StreamInId       = NULL;
    driver->StreamInState    = NULL;
    driver->StreamInPriority = NULL;
    pio_sti_state_list_ensure(driver, 128);
    pio_sti_priority_queue_create(driver->STIActiveQueue, 128);
    // initialize the queue for pushing stream-in open requests to the driver.
    fifo_allocator_init(&driver->STIPendingAlloc);
    mpsc_fifo_u_init   (&driver->STIPendingQueue);
    // initialize the queue for pushing stream-in control requests to the driver.
    fifo_allocator_init(&driver->STIControlAlloc);
    mpsc_fifo_u_init   (&driver->STIControlQueue);
    return ERROR_SUCCESS;
}

/// @summary Closes a prioritized I/O driver and frees associated resources.
/// @param driver The prioritized I/O driver state to clean up.
public_function void pio_driver_close(pio_driver_t *driver)
{
    driver->AIO = NULL;
}

/// @summary Queues a file to be streamed in. 
/// @param driver The prioritized I/O driver state managing the stream.
/// @param request The stream-in request specifying information about the file.
public_function void pio_driver_stream_in(pio_driver_t *driver, pio_sti_request_t const &request)
{
    fifo_node_t<pio_sti_request_t> *n = fifo_allocator_get(&driver->STIPendingAlloc);
    n->Item  =  request;
    mpsc_fifo_u_produce(&driver->STIPendingQueue, n);
}

/// @summary 
/*public_function void pio_driver_pause_stream(pio_driver_t *driver, uintptr_t id)
{
    fifo_node_t<pio_sti_control_t> *n = fifo_allocator_get(&driver->STIControlAlloc);
    n->Item.Identifier = id;
    n->Item.ByteOffset = 0;
    n->Item.Command    = PIO_STREAM_IN_CONTROL_PAUSE;
    mpsc_fifo_u_produce(&driver->STIControlQueue, n);
}*/

/// @summary Executes a single AIO driver update in polling mode. The kernel AIO system will be polled for completed events without blocking. This dispatches completed command results to their associated target queues and launches as many not started commands as possible.
/// @param driver The AIO driver state to poll.
public_function void pio_driver_poll(pio_driver_t *driver)
{
    pio_driver_main(driver);
}
