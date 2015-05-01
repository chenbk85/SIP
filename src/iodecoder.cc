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
struct aio_result_t
{
};

struct io_buffer_allocator_t
{
};

enum stream_refill_result_e : int
{
    STREAM_REFILL_RESULT_START = 0, /// At least one byte of data is currently available.
    STREAM_REFILL_RESULT_YIELD = 1, /// No input is currently available. Try again later.
    STREAM_REFILL_RESULT_ERROR = 2, /// An error was encountered. Halt.
};

enum stream_decode_error_e  : int
{
    STREAM_DECODE_ERROR_NONE   = 0, /// No error has occurred.
    // ...
};

struct stream_decoder_t
{
    stream_decoder_t(void);                               /// Initialize the decoder. No buffer allocator is selected.
    virtual ~stream_decoder_t(void);                      /// Free resources owned by the stream decoder.

    size_t    amount (void) const;                        /// Get the number of decoded bytes available.
    void*     nextbuf(void);                              /// Dequeue the next queued encoded data buffer, if any.
    int     (*refill)(stream_decoder_t *decoder);         /// Decode the next chunk of encoded data buffer. Return stream_refill_result_e.

    uint8_t                        *FirstByte;            /// The first byte of available data.
    uint8_t                        *FinalByte;            /// One past the last byte of available data.
    uint8_t                        *ReadCursor;           /// The current read cursor within the decoded buffer.
    int                             ErrorCode;            /// The sticky error code value.

    uint8_t                        *EncodedData;          /// The current block of encoded data.
    size_t                          EncodedDataOffset;    /// The current byte offset within the encoded data block.
    size_t                          EncodedDataSize;      /// The number of bytes in the current encoded data block.

    fifo_allocator_t<aio_result_t>  AIOResultAlloc;       /// The FIFO node allocator for the AIO driver queue.
    spsc_fifo_u_t   <aio_result_t>  AIOResultQueue;       /// The SPSC FIFO to which the AIO driver posts read results.

    io_buffer_allocator_t          *BufferAllocator;      /// The active I/O buffer allocator.
    fifo_allocator_t<void*>        *BufferReturnAlloc;    /// The FIFO node allocator for the I/O buffer return queue.
    mpsc_fifo_u_t   <void*>        *BufferReturnQueue;    /// The MPSC FIFO to which the decoder returns I/O buffers.

    io_buffer_allocator_t           InternalAllocator;    /// The (possibly unused) internal I/O buffer allocator.
    fifo_allocator_t<void*>         InternalReturnAlloc;  /// The FIFO node allocator for the internal I/O buffer return queue.
    mpsc_fifo_u_t   <void*>         InternalReturnQueue;  /// The MPSC FIFO for the internal I/O buffer return queue.
};

/*///////////////
//   Globals   //
///////////////*/

/*///////////////////////
//   Local Functions   //
///////////////////////*/

/*////////////////////////
//   Public Functions   //
////////////////////////*/
