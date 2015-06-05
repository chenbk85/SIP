/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines constants, types and functions for asynchronously loading
/// image data and caching it in image blobs.
///////////////////////////////////////////////////////////////////////////80*/

/*////////////////
//   Includes   //
////////////////*/

/*////////////////////
//   Preprocessor   //
////////////////////*/

/*/////////////////
//   Constants   //
/////////////////*/

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Define the supported image data file formats.
enum image_file_format_e : int
{
    IMAGE_FILE_FORMAT_UNKNOWN     = 0,      /// The source file format is not known.
    IMAGE_FILE_FORMAT_DDS         = 1,      /// The source file format follows the Microsoft DDS specification.
    /// ...
};

/// @summary Define the data returned after a request to load an image has completed.
struct image_load_result_t
{
    uintptr_t              ImageId;         /// The application-defined image identifier.
    uint32_t               StreamError;     /// The error returned by the stream decoder. See stream_decode_error_e.
    int                    ParserError;     /// The error returned by the image parser. A value of zero indicates no error.
    uint32_t               ImageFormat;     /// The image pixel format. One of dxgi_format_e.
    uint32_t               ImageEncoding;   /// The image encoding. One of image_encoding_e.
    size_t                 BaseWidth;       /// The width of the highest-resolution mip-level, in pixels.
    size_t                 BaseHeight;      /// The height of the highest-resolution mip-level, in pixels.
    size_t                 BaseSliceCount;  /// The number of slices in the highest-resolution mip-level.
    size_t                 ElementCount;    /// The number of array elements or cubemap faces.
    size_t                 LevelCount;      /// The number of levels in the mipmap chain.
    size_t                 BitsPerPixel;    /// The number of bytes per-pixel.
    size_t                 BytesPerBlock;   /// The number of bytes per-block for compressed formats, or zero.
    size_t                 ImageDataOffset; /// The byte offset of the image data from the start of the file or buffer.
    dds_level_desc_t      *LevelInfo;       /// An array of LevelCount mip-level descriptors, owned by the image blob.
    size_t                *LevelOffsets;    /// An array of ElementCount * LevelCount zero-based byte offsets of the start of each level, owned by the image blob.
    dds_header_t           DDSHeader;       /// The DDS header describing the image.
    dds_header_dxt10_t     DX10Header;      /// The Direct3D 10 extended header describing the image.
};

typedef fifo_allocator_t<image_load_result_t>   image_result_alloc_t;
typedef mpsc_fifo_u_t   <image_load_result_t>   image_result_queue_t;

/// @summary Define the data used to request that image data be loaded into an image blob.
struct image_load_request_t
{
    uintptr_t              ImageId;         /// The application-defined image identifier.
    stream_decoder_t      *Decoder;         /// The decoder used to read the file data.
    image_result_alloc_t  *ResultAlloc;     /// The FIFO node allocator used to enqueue items into the result queue.
    image_result_queue_t  *ResultQueue;     /// The MPSC unbounded FIFO where the result of the load operation will be placed.
    int                    FileFormat;      /// One of image_file_format_e specifying the type of parser to create.
};

typedef fifo_allocator_t<image_load_request_t>  image_request_alloc_t;
typedef mpsc_fifo_u_t   <image_load_request_t>  image_request_queue_t;

/// @summary Define the result queues associated with an image parser state.
struct image_result_queues_t
{
    image_result_alloc_t  *ResultAlloc;     /// The FIFO node allocator used to enqueue load results.
    image_result_queue_t  *ResultQueue;     /// The MPSC unbounded FIFO that will receive the image load result.
};

/// @summary Define the data associated with a dynamic list of parser state.
/// This type is specialized for each supported file format.
template <typename T>
struct image_parser_list_t
{
    size_t                 Count;           /// The number of active items in the list.
    size_t                 Capacity;        /// The number of parser state objects allocated.
    stream_decoder_t     **SourceStream;    /// An array of pointers to the source stream decoders.
    image_result_queues_t *ResultQueue;     /// An array of result queue objects.
    T                     *ParseState;      /// An array of parser state objects.
};

typedef image_parser_list_t<dds_parser_state_t> dds_parser_list_t;

/// @summary Define the data associated with the image loader. This is the 
/// application-facing interface used to request that images be loaded. It
/// receives load requests from one or more threads, and on each tick, updates
/// the current state of all active parsers. 
struct image_loader_t
{
    image_request_queue_t  RequestQueue;    /// The MPSC unbounded FIFO for receiving image load requests.
    dds_parser_list_t      DDSList;         /// The set of active parsers for DDS files.
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
/// @summary Initialize an image parser list with a given initial capacity.
/// @param ipsl The image parser list to initialize.
/// @param capacity The initial capacity of the list.
template <typename T>
void image_parser_list_create(image_parser_list_t<T> *ipsl, size_t capacity)
{
    ipsl->Count       = 0;
    ipsl->Capacity    = 0;
    ipsl->ParseState  = NULL;
    ipsl->ResultQueue = NULL;
    if (capacity > 0)
    {   // pre-allocate storage for the specified number of items.
        image_result_queues_t *rq = (image_result_queues_t*)malloc(capacity * sizeof(image_result_queues_t));
        stream_decoder_t     **sd = (stream_decoder_t    **)malloc(capacity * sizeof(stream_decoder_t*));
        T                     *ps = (T                    *)malloc(capacity * sizeof(T));
        if (rq != NULL)   ipsl->ResultQueue  = rq;
        if (sd != NULL)   ipsl->SourceStream = sd;
        if (ps != NULL)   ipsl->ParseState   = ps;
        if (rq != NULL && sd != NULL && ps  != NULL) ipsl->Capacity = capacity;
    }
}

/// @summary Ensure that an image parser list has at least the specified capacity.
/// If not, grow the storage capacity by reallocating the underlying arrays.
/// @param ipsl The image parser list to check and possibly grow.
/// @param capacity The minimum capacity of the list.
template <typename T>
void image_parser_list_ensure(image_parser_list_t<T> *ipsl, size_t capacity)
{
    if (ipsl->Capacity >= capacity)
    {   // no need to do anything, capacity already available.
        return;
    }
    // determine new capacity and reallocate lists.
    size_t newc               = calculate_capacity(ipsl->Capacity, capacity, 128, 16);
    image_result_queues_t *rq =(image_result_queues_t*)  realloc(ipsl->ResultQueue , newc * sizeof(image_result_queues_t));
    stream_decoder_t     **sd =(stream_decoder_t    **)  realloc(ipsl->SourceStream, newc * sizeof(stream_decoder_t*));
    T                     *ps =(T                    *)  realloc(ipsl->ParseState  , newc * sizeof(T));
    if (rq != NULL)   ipsl->ResultQueue  = rq;
    if (sd != NULL)   ipsl->SourceStream = sd;
    if (ps != NULL)   ipsl->ParseState   = ps;
    if (rq != NULL && sd != NULL && ps  != NULL) ipsl->Capacity = newc;
}

/// @summary Enqueue a DDS stream to be loaded into an image blob.
/// @param ddsp The dynamic list of DDS file parsers.
/// @param request The image load request.
public_function void dds_parser_list_add(dds_parser_list_t *ddsp, image_load_request_t const &request)
{
    image_parser_list_ensure(ddsp, ddsp->Count + 1);
    size_t index = ddsp->Count++;
    ddsp->ResultQueue [index].ResultAlloc = request.ResultAlloc;
    ddsp->ResultQueue [index].ResultQueue = request.ResultQueue;
    ddsp->SourceStream[index]             = request.Decoder;
    dds_parser_state_init(&ddsp->ParseState[index], request.ImageId);
}

/// @summary Update the state of all DDS parsers and generate results for any completed items.
/// @param ddsp The dynamic list of DDS file parsers.
public_function void dds_parser_list_update(dds_parser_list_t *ddsp)
{
    image_result_queues_t *queues   = ddsp->ResultQueue;
    dds_parser_state_t    *parsers  = ddsp->ParseState;
    stream_decoder_t     **decoders = ddsp->SourceStream;

    int    res   = DDS_PARSE_RESULT_CONTINUE;
    size_t index = 0;
    while (index < ddsp->Count)
    {
        if ((res = dds_parser_update(decoders[index], &parsers[index])) == DDS_PARSE_RESULT_CONTINUE)
        {   // not finished with this stream yet.
            index++;
            continue;
        }

        // the image has either been parsed and encoded successfully, 
        // or an error has occurred - either way, generate a result.
        fifo_node_t<image_load_result_t>  *n = fifo_allocator_get(queues[index].ResultAlloc);
        n->Item.ImageId         = parsers [index].Identifier;
        n->Item.StreamError     = decoders[index]->ErrorCode;
        n->Item.ParserError     = parsers [index].ParserError;
        n->Item.ImageFormat     = parsers [index].ImageFormat;
        n->Item.ImageEncoding   = parsers [index].ImageEncoding;
        n->Item.BaseWidth       = parsers [index].BaseDimension[0];
        n->Item.BaseHeight      = parsers [index].BaseDimension[1];
        n->Item.BaseSliceCount  = parsers [index].BaseDimension[2];
        n->Item.ElementCount    = parsers [index].ElementCount;
        n->Item.LevelCount      = parsers [index].LevelCount;
        n->Item.BitsPerPixel    = parsers [index].BitsPerPixel;
        n->Item.BytesPerBlock   = parsers [index].BytesPerBlock;
        n->Item.ImageDataOffset = parsers [index].ImageDataOffset;
        n->Item.LevelInfo       = parsers [index].LevelInfo;    // owned by image blob
        n->Item.LevelOffsets    = parsers [index].LevelOffsets; // owned by image blob
        if (parsers[index].DDSHeader != NULL)
        {   // copy the DDS file header.
            memcpy(&n->Item.DDSHeader, parsers[index].DDSHeader, sizeof(dds_header_t));
            if (parsers[index].DX10Header != NULL)
            {   // copy the existing DX10 extended header.
                memcpy(&n->Item.DX10Header, parsers[index].DX10Header, sizeof(dds_header_dxt10_t));
            }
            else
            {   // generate the DX10 header from the base DDS header.
                dx10_header_for_dds(&n->Item.DX10Header, parsers[index].DDSHeader);
            }
        }
       
        // produce the result in the destination queue.
        mpsc_fifo_u_produce(queues[index].ResultQueue, n);

        // release the reference we're holding to the decoder.
        decoders[index]->release();

        // now remove this parser from the active set by swapping the 
        // last element into its place. do not update the index.
        size_t   last   = ddsp->Count - 1;
        queues  [index] = queues  [last];
        decoders[index] = decoders[last];
        parsers [index] = parsers [last];
        ddsp->Count     = last;
    }
}

/// @summary Checks the error codes on a DDS file load result to determine whether an error occurred.
/// @param result The result to check.
/// @return true if an error occurred during loading.
public_function bool image_load_error(image_load_result_t const &result)
{
    return (FAILED(result.StreamError) || (result.ParserError != 0));
}

/// @summary Examines the file extension portion of a path string to determine the correct image_file_format_e.
/// @param path A NULL-terminated UTF-8 string specifying a filename.
/// @return One of image_file_format_e.
public_function int image_file_format_from_extension(char const *path)
{
    size_t      len = strlen(path);
    char const *ext = path + len - 1;
    while (ext >= path)
    {
        if (*ext == '.')
        {
            ext++; // skip '.'
            break;
        }
        else ext--;
    }
    if (ext < path)
    {   // no extension was found, so the format cannot be determined.
        return IMAGE_FILE_FORMAT_UNKNOWN;
    }
    if (_stricmp(ext, "dds") == 0)
    {
        return IMAGE_FILE_FORMAT_DDS;
    }
    else
    {
        return IMAGE_FILE_FORMAT_UNKNOWN;
    }
}

/// @summary Initializes a new image loader instance.
/// @param loader The image loader instance to initialize.
/// @return true if the image loader is ready for use.
public_function bool image_loader_create(image_loader_t *loader/*, image_cache_t*? */)
{
    mpsc_fifo_u_init(&loader->RequestQueue);
    image_parser_list_create(&loader->DDSList, 16);
}

/// @summary Queues an image for loading. The application is responsible for opening the stream.
/// @param loader The image loader that will manage parsing the image file format.
/// @param decoder The stream decoder used to read the stream data.
/// @param file_type One of image_file_format_e indicating the file format of the image data.
/// @param thread_alloc The FIFO node allocator for the thread requesting the file load.
/// @param queues The FIFO node allocator and queue to which the load result will be posted.
/// @return true if the image load was queued.
public_function bool load_image(image_loader_t *loader, stream_decoder_t *decoder, int file_format, image_request_alloc_t *thread_alloc, image_result_queues_t const &queues)
{   // TODO(rlk): need to validate the file_format prior to enqueueing the request.
    fifo_node_t<image_load_request_t> *n = fifo_allocator_get(thread_alloc);
    n->Item.ImageId    = decoder->Identifier;
    n->Item.Decoder    = decoder;
    n->Item.ResultAlloc= queues.ResultAlloc;
    n->Item.ResultQueue= queues.ResultQueue;
    n->Item.FileFormat = file_format;
    mpsc_fifo_u_produce(&loader->RequestQueue, n);
    return true;
}

/// @summary Updates the state of all in-progress image loads queued against an image loader.
/// @param loader The image loader state to update.
public_function void image_loader_update(image_loader_t *loader)
{   // start any pending image load requests.
    image_load_request_t load;
    while (mpsc_fifo_u_consume(&loader->RequestQueue, load))
    {
        switch (load.FileFormat)
        {
        case IMAGE_FILE_FORMAT_DDS:
                dds_parser_list_add(&loader->DDSList, load);
                break;
        }
    }
    // update the state of each parser list. this generates image 
    // load results in the target queue for each load request.
    dds_parser_list_update(&loader->DDSList);
}
