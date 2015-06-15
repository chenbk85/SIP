/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines the interfaces and implementations for stream decoders,
/// which accept buffers of possibly encoded data via a SPSC queue, and produce
/// a small buffer of decoded data suitable for input to a parser or similar. 
/// The decoded data is exposed to the downstream consumer via a pull model 
/// designed to minimize the amount of additional allocation and copying.
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
/// @summary Define the possible return values for the function pointer
/// stream_decoder_t::refill. These codes inform the client on how to 
/// proceed pulling data from the stream decoder.
enum stream_refill_result_e : int
{
    STREAM_REFILL_RESULT_START       = 0,         /// At least one byte of data is currently available.
    STREAM_REFILL_RESULT_YIELD       = 1,         /// No input is currently available. Try again later.
    STREAM_REFILL_RESULT_ERROR       = 2,         /// An error was encountered. Halt and check ErrorCode.
};

/// @summary Define the recognized error codes. The only defined code is 
/// the 'no error' code. The remaining error codes are defined by the 
/// operating system, and may be an HRESULT, errno code, etc.
enum stream_decode_error_e  : uint32_t
{
    STREAM_DECODE_ERROR_NONE         = 0,         /// No error has occurred.
    // ...
};

/// @summary Bitflags defining the possible status values passed through 
/// the result of an asynchronous I/O driver operation. These flags are 
/// specified by the prioritized I/O driver and pass through the AIO driver.
enum stream_decode_status_e : uint32_t
{
    STREAM_DECODE_STATUS_NONE        = (0 << 0),  /// No special status bits are set.
    STREAM_DECODE_STATUS_RESTART     = (1 << 0),  /// After processing the current buffer, reset internal state.
    STREAM_DECODE_STATUS_ENDOFSTREAM = (1 << 1),  /// The current buffer represents the final data in the stream.
    // ...
};

/// @summary Defines the data required to represent a position within a stream.
/// The file or source offset defines the start of the encoded data block, and 
/// then the number of bytes decoded from the encoded data block is required to
/// be able to map a client's concept of byte offset into an encoded stream.
struct stream_decode_pos_t
{
    int64_t                FileOffset;            /// The byte offset of the encoded data chunk in the file.
    size_t                 DecodeOffset;          /// The number of decoded bytes consumed by the client.
};

/// @summary Defines the interface to a stream decoder. The default decoder
/// implementation performs no decoding at all. Derived decoder types may 
/// add additional buffers (for decompressed data, for example) and custom 
/// buffer refill functions. The refill field is a function pointer, which 
/// allows basic state machines to be implemented. The stream decoder is 
/// reference counted, as typically one reference is held by the I/O driver
/// streaming data into the decoder, and another is held by the parser 
/// reading data from the decoder.
struct stream_decoder_t
{
    stream_decoder_t(void);                       /// Initialize the decoder. No buffer allocator is selected.
    virtual ~stream_decoder_t(void);              /// Free resources owned by the stream decoder.

    intptr_t       addref (void);                 /// Increment the reference count by one.
    intptr_t       release(void);                 /// Decrement the reference count by one, possibly auto-delete.
    size_t         amount (void) const;           /// Get the number of decoded bytes available.
    bool           atend  (void) const;           /// Test the current status flags for ENDOFSTREAM.
    void*          nextbuf(void);                 /// Dequeue the next queued encoded data buffer, if any.
    void           pos(stream_decode_pos_t  &p);  /// Retrieve the current stream position.
    int          (*refill)(stream_decoder_t *s);  /// Decode the next chunk of encoded data buffer. Return stream_refill_result_e.

    virtual void   reset  (void);                 /// Reset the decoder state in preparation for stream restart.

    uintptr_t              Identifier;            /// The application-defined stream identifier.
    uint64_t               StreamType;            /// The application-defined stream type.
    uint8_t               *FirstByte;             /// The first byte of available data.
    uint8_t               *FinalByte;             /// One past the last byte of available data.
    uint8_t               *ReadCursor;            /// The current read cursor within the decoded buffer.
    uint32_t               StatusFlags;           /// Status flags associated with the current buffer.
    uint32_t               ErrorCode;             /// The sticky error code value.
    int64_t                FileOffset;            /// The file offset of the start of the encoded data block.
    size_t                 DecodeOffset;          /// The number of bytes of decoded data read from the current file data block.
    uint8_t               *EncodedData;           /// The current block of encoded data.
    size_t                 EncodedDataOffset;     /// The current byte offset within the encoded data block.
    size_t                 EncodedDataSize;       /// The number of bytes in the current encoded data block.
    aio_result_alloc_t     AIOResultAlloc;        /// The FIFO node allocator for the AIO driver queue.
    aio_result_queue_t     AIOResultQueue;        /// The SPSC FIFO to which the AIO driver posts read results.
    io_buffer_allocator_t *BufferAllocator;       /// The active I/O buffer allocator; may not be &InternalAllocator.
    io_buffer_allocator_t  InternalAllocator;     /// The (possibly unused) internal I/O buffer allocator.
    std::atomic<intptr_t>  ReferenceCount;        /// The reference count used for lifetime management.
};

/*///////////////
//   Globals   //
///////////////*/

/*///////////////////////
//   Local Functions   //
///////////////////////*/
/// @summary Stream refill implementation that returns an error status, 
/// and does not refill the internal buffer.
/// @param s The stream decoder being refilled.
/// @return One of stream_refill_result_e specifying the current status.
internal_function int stream_refill_error(stream_decoder_t *s)
{
    UNREFERENCED_PARAMETER(s);
    return STREAM_REFILL_RESULT_ERROR;
}

/// @summary Stream refill implementation that returns a yield status, 
/// indicating that no more data is immediately available, but no error
/// has occurred, and does not refill the internal buffer.
/// @param s The stream decoder being refilled.
/// @return One of stream_refill_result_e specifying the current status.
internal_function int stream_refill_yield(stream_decoder_t *s)
{
    UNREFERENCED_PARAMETER(s);
    return STREAM_REFILL_RESULT_YIELD;
}

/// @summary Helper function that sets the error status of a stream decoder, and
/// also sets the refill function to pio_stream_decoder_refill_error().
/// @param s The stream decoder that encountered the error.
/// @param error One of stream_in_decode_error_e specifying the error encountered by the decoder.
/// @return One of stream_refill_result_e specifying the current status.
internal_function int stream_decode_fail(stream_decoder_t *s, uint32_t error)
{
    s->ErrorCode = error;
    s->refill    = stream_refill_error;
    return s->refill(s);
}

/// @summary Implements a dummy refill function that refills the buffer with zero-bytes.
/// @param s The stream decoder being refilled.
/// @return One of stream_refill_result_e specifying the current status.
internal_function int stream_refill_zeroes(stream_decoder_t *s)
{   local_persist uint8_t ZERO_DATA[256] = {0};
    s->FirstByte        = ZERO_DATA;
    s->FinalByte        = ZERO_DATA + sizeof(ZERO_DATA);
    s->ReadCursor       = ZERO_DATA;
    s->DecodeOffset    += sizeof(ZERO_DATA);
    return s->ErrorCode ? STREAM_REFILL_RESULT_ERROR : STREAM_REFILL_RESULT_START;
}

/// @summary Implements a refill function that polls the queue of completed I/O requests.
/// @param s The stream decoder being refilled.
/// @return One of the stream_refill_result_e specifying the current status.
internal_function int stream_refill_nextbuf(stream_decoder_t *s)
{
    int rc = STREAM_REFILL_RESULT_START;
    if (s->EncodedDataOffset == s->EncodedDataSize)
    {   // there's no encoded input remaining, so attempt to grab the next encoded block.
        void *encbuf  = s->nextbuf();
        if   (encbuf != NULL)
        {   // the consumer can begin reading data.
            rc = s->ErrorCode ? STREAM_REFILL_RESULT_ERROR : STREAM_REFILL_RESULT_START;
            // your refill implementation might choose to decode a chunk of data here.
            // ...
            // this refill implementation performs a zero-copy, no-op transformation.
            // all of the encoded input buffer is consumed and made available.
            s->FirstByte          = (uint8_t*) s->EncodedData;
            s->FinalByte          = (uint8_t*) s->EncodedData + s->EncodedDataSize;
            s->ReadCursor         = (uint8_t*) s->EncodedData;
            s->EncodedDataOffset  = s->EncodedDataSize;
        }
        else
        {   // no additional data is available at the present time.
            rc = s->ErrorCode ? STREAM_REFILL_RESULT_ERROR : STREAM_REFILL_RESULT_YIELD;
        }
    }
    else
    {   // there's additional encoded input remaining. decode a bit more.
        // your refill implementation might choose to decode a chunk of data here.
        // ...
        // and then increment s->EncodedDataOffset by the decoded buffer size.
        // s->FirstByte, s->FinalByte and s->ReadCursor point to the decoded buffer.
        s->DecodeOffset  +=(s->FinalByte - s->FirstByte); // consumed decoded buffer size bytes.
        rc = s->ErrorCode ? STREAM_REFILL_RESULT_ERROR : STREAM_REFILL_RESULT_START;
    }
    return rc;
}

/*////////////////////////
//   Public Functions   //
////////////////////////*/
/// @summary Initialize all fields of the decoder to their default values.
/// The creator should specify the identifier, stream type and set the 
/// BufferAllocator field to point to either the internal allocator, or to 
/// another externally managed allocator.
stream_decoder_t::stream_decoder_t(void)
    : 
    refill(stream_refill_nextbuf),
    Identifier(0), 
    StreamType(0),
    FirstByte(NULL),
    FinalByte(NULL), 
    ReadCursor(NULL), 
    StatusFlags(STREAM_DECODE_STATUS_RESTART), 
    ErrorCode(STREAM_DECODE_ERROR_NONE), 
    EncodedData(NULL), 
    EncodedDataOffset(0), 
    EncodedDataSize(0), 
    BufferAllocator(NULL), 
    ReferenceCount(0)
{
    aio_create_result_queue(&AIOResultQueue, &AIOResultAlloc);
}

/// @summary Release resource managed by the decoder. The I/O result queue and 
/// any internal buffer space are released back to the system.
stream_decoder_t::~stream_decoder_t(void)
{
    aio_delete_result_queue(&AIOResultQueue, &AIOResultAlloc);
}

/// @summary Retains a reference to the stream decoder instance.
/// @return The new reference count.
inline intptr_t stream_decoder_t::addref(void)
{
    return ReferenceCount.fetch_add(1) + 1;
}

/// @summary Releases a reference to the stream decoder instance.
/// If the reference count reaches zero, the decoder is deleted.
/// @return The new reference count.
inline intptr_t stream_decoder_t::release(void)
{
    intptr_t n = ReferenceCount.fetch_sub(1) - 1;
    if (n <= 0)  delete this;
    return n;
}

/// @summary Calculate the number of bytes available for reading before the 
/// stream needs to be refilled (by calling the refill function.)
/// @return The number of bytes available for reading.
inline size_t stream_decoder_t::amount(void) const
{
    return size_t(FinalByte - ReadCursor);
}

/// @summary Checks the current status flags to see if the end-of-stream status
/// is set, indicating that the current buffer 
inline bool stream_decoder_t::atend(void) const
{
    return (StatusFlags & STREAM_DECODE_STATUS_ENDOFSTREAM) != 0;
}

/// @summary Retrieves the current stream position.
/// @param p On return, stores the current stream position.
inline void stream_decoder_t::pos(stream_decode_pos_t &p)
{
    p.FileOffset   = FileOffset;
    p.DecodeOffset = size_t(ReadCursor-FirstByte);
}

/// @summary Allows the implementation to reset internal state in preparation for
/// a stream reset, which could indicate the start of a nested stream, or looping
/// back to the beginning of the current data set. Assume that buffers are not 
/// valid when this method is called.
void stream_decoder_t::reset(void)
{
    /* empty */
}

/// @summary Dequeues the next completed I/O operation and sets up the encoded 
/// data buffers. All of the encoded data fields are updated accordingly. Both
/// error status and stream status flags are updated. Any stream resets are 
/// processed after returning the existing I/O buffer, but prior to dequeuing 
/// the next pending I/O result. The refill implementation that calls this 
/// method is responsible for decoding the first chunk of data into its internal
/// buffer and setting the FirstByte, FinalByte and ReadCursor fields, and then 
/// updating the EncodedDataOffset field with the number of bytes decoded.
/// @return A pointer to the start of the encoded data buffer, or NULL if either
/// the stream was closed, or if there are no completed I/O operations waiting.
void* stream_decoder_t::nextbuf(void)
{   // start by returning any existing buffer - the data isn't needed anymore.
    if (EncodedData != NULL)
    {   // the buffer is returned to the active allocator, which may or may 
        // not be the internal I/O buffer allocator managed by the decoder.
        BufferAllocator->put_buffer(EncodedData);
        EncodedData       = NULL;
        EncodedDataOffset = 0;
        EncodedDataSize   = 0;
    }

    // if this buffer indicated that a stream restart is coming, reset state.
    if (StatusFlags &   STREAM_DECODE_STATUS_RESTART)
    {   // also clear the restart status so we don't reset multiple times.
        StatusFlags &= ~STREAM_DECODE_STATUS_RESTART;
        StatusFlags  =  STREAM_DECODE_STATUS_NONE;
        this->reset();
    }

    // now attempt to dequeue the next pending AIO result and update internal state.
    aio_result_t io_result;
    uint8_t     *io_buffer = NULL;
    if (spsc_fifo_u_consume(&AIOResultQueue,io_result))
    {   // update internal state for the new buffer.
        if (io_result.DataBuffer != NULL && io_result.DataActual > 0)
        {   // some data was returned from a read.
            io_buffer         = (uint8_t*)  io_result.DataBuffer;
            FileOffset        = io_result.FileOffset;
            DecodeOffset      = 0;
            EncodedData       = io_buffer;
            EncodedDataOffset = 0;
            EncodedDataSize   = io_result.DataActual;
            StatusFlags       = io_result.StatusFlags;
            if (SUCCEEDED(io_result.OSError))
            {   // clear the current error code.
                ErrorCode = STREAM_DECODE_ERROR_NONE;
            }
            else
            {   // save the error code for the caller.
                ErrorCode = io_result.OSError;
            }
        }
        else
        {   // the underlying data source was closed.
            // reading from the stream will return zero bytes.
            stream_refill_zeroes(this);
            refill      = stream_refill_zeroes;
            ErrorCode   = ERROR_HANDLE_EOF;
            StatusFlags = STREAM_DECODE_STATUS_ENDOFSTREAM;
        }
        // a queue entry was consumed, so release a reference.
        this->release();
    }
    return io_buffer; // NULL if no additional data is available.
}
