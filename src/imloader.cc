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
/// @summary Define the number of images per hash-bucket for the image loader.
#define IMAGE_LOADER_BUCKET_SIZE    128

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Define the supported image data file formats.
enum image_file_format_e : int
{
    IMAGE_FILE_FORMAT_UNKNOWN     = 0,         /// The source file format is not known.
    IMAGE_FILE_FORMAT_DDS         = 1,         /// The source file format follows the Microsoft DDS specification.
    /// ...
};

/// @summary Define the supported error result identifiers.
enum image_load_error_e : int
{
    IMAGE_LOAD_ERROR_SUCCESS      = 0,         /// This value is not specified in an error. The image data was successfully loaded.
    IMAGE_LOAD_ERROR_FILE_ACCESS  = 1,         /// The image could not be loaded because the source file could not be accessed.
    IMAGE_LOAD_ERROR_NO_ENCODER   = 2,         /// The image could not be loaded because no encoder can perform the transformation.
    IMAGE_LOAD_ERROR_BAD_DATA     = 3,         /// The image could not be loaded because of an error parsing the container format.
    IMAGE_LOAD_ERROR_NO_MEMORY    = 4,         /// The image could not be loaded because there is not enough image memory.
    IMAGE_LOAD_ERROR_OSERROR      = 5,         /// The image could not be loaded. Check to OSError field.
    IMAGE_LOAD_ERROR_NO_PARSER    = 6,         /// THe image could not be loaded because there is no parser for the specified container format.
    /// ...
};

/// @summary Defines the data associated with a request to load image data from a file.
/// For images where the frame data is spread across multiple files (like a PNG sequence), 
/// one load request is generated for each file.
struct image_load_t
{
    uintptr_t                 ImageId;         /// The application-defined logical image identifier.
    char const               *FilePath;        /// The NULL-terminated UTF-8 virtual file path.
    size_t                    FirstFrame;      /// The zero-based index of the first frame to load.
    size_t                    FinalFrame;      /// The zero-based index of the last frame to load, or IMAGE_ALL_FRAMES.
    size_t                    DecodeOffset;    /// The number of bytes of decoded data to read from the chunk to reach the start of the first frame.
    int64_t                   FileOffset;      /// The byte offset of the chunk of encoded data containing the start of the first frame to load, or 0.
    int                       FileHints;       /// A combination of vfs_file_hint_t to use when opening the file.
    int                       DecoderHint;     /// One of vfs_decoder_hint_e to use when opening the file.
    image_definition_t        Metadata;        /// The image metadata. If not known, the ImageFormat field will be set to DXGI_FORMAT_UNKNOWN.
    uint8_t                   Priority;        /// The file load priority.
};
typedef fifo_allocator_t<image_load_t>             image_load_alloc_t;
typedef mpsc_fifo_u_t   <image_load_t>             image_load_queue_t;

/// @summary Defines the data returned for an unsuccessful image load. Error results are 
/// posted to an optional user-defined queue.
struct image_load_error_t
{
    uintptr_t                 ImageId;         /// The application-defined logical image identifier.
    char const               *FilePath;        /// The NULL-terminated UTF-8 virtual file path of the source file.
    size_t                    FirstFrame;      /// The zero-based index of the first frame to load.
    size_t                    FinalFrame;      /// The zero-based index of the last frame to load, or IMAGE_ALL_FRAMES.
    int                       SrcCompression;  /// One of image_compression_e specifying the source compression format.
    int                       SrcEncoding;     /// One of image_encoding_e specifying the source encoding format.
    int                       DstCompression;  /// One of image_compression_e specifying the target storage compression format.
    int                       DstEncoding;     /// One of image_encoding_e specifying the target storage encoding format.
    int                       ErrorCode;       /// One of image_load_error_e specifying the error code.
    uint32_t                  OSError;         /// The system error code, or ERROR_SUCCESS.
};
typedef fifo_allocator_t<image_load_error_t>       image_load_error_alloc_t;
typedef mpsc_fifo_u_t   <image_load_error_t>       image_load_error_queue_t;

/// @summary Typedefs for lists maintaining parse state for each supported container format.
typedef image_parser_list_t<dds_parser_state_t>    dds_parser_list_t;

/// @summary Define the information used to configure image loading.
struct image_loader_config_t
{
    vfs_driver_t             *VFSDriver;       /// The virtual file system driver used for accessing files.

    image_memory_t           *ImageMemory;     /// The image memory where pixel data will be placed.
    image_definition_queue_t *DefinitionQueue; /// The MPSC unbounded queue for image metadata.
    image_location_queue_t   *PlacementQueue;  /// The MPSC unbounded queue for image placement information. 
    image_load_error_queue_t *ErrorQueue;      /// The MPSC unbounded queue for image load error information.

    size_t                    ImageCapacity;   /// The number of images expected to be loaded through this instance.
    int                       Compression;     /// The compression type used to store pixel data in memory.
    int                       Encoding;        /// The encoding type used to store pixel data in memory.
};

/// @summary Define the data associated with the image loader. This is the 
/// application-facing interface used to request that images be loaded. It
/// receives load requests from a thread, and on each tick, updates the 
/// current state of all active parsers.
struct image_loader_t
{
    image_load_queue_t        RequestQueue;    /// The MPSC unbounded FIFO for receiving image load requests.
    image_memory_t           *ImageMemory;     /// Image memory where pixel data will be placed.
    image_definition_queue_t *DefinitionQueue; /// The queue where image definitions should be placed.
    image_location_queue_t   *PlacementQueue;  /// The queue where image placement information should be placed.
    image_load_error_queue_t *ErrorQueue;      /// The queue where error information for unsuccessful loads should be placed.
    int                       Compression;     /// The compression used to store pixel data in memory.
    int                       Encoding;        /// The encoding used to store pixel data in memory.

    SRWLOCK                   ImageLock;       /// Reader-Writer lock protecting the image list.
    size_t                    ImageCount;      /// The number of images loaded through this loader.
    size_t                    ImageCapacity;   /// The capacity of the image metadata list.
    id_table_t                ImageIds;        /// The table mapping image ID to metadata index.
    image_definition_t       *ImageMetadata;   /// The set of metadata for all images loaded via this loader.

    thread_io_t               io;              /// The system I/O interface for the loader thread.
    dds_parser_list_t         ActiveDDS;       /// The set of active parsers for DDS files.
    // ...

    image_definition_alloc_t  DefinitionAlloc; /// The FIFO node allocator used to write to the definition queue.
    image_location_alloc_t    PlacementAlloc;  /// The FIFO node allocator used to write to the location queue.
    image_load_error_alloc_t  ErrorAlloc;      /// The FIFO node allocator used to write to the error queue.
};

/// @summary Encapsulates all of the thread-local data required to access an image loader.
struct thread_image_loader_t
{
    thread_image_loader_t(void);
    ~thread_image_loader_t(void);

    void                      initialize
    (
        image_loader_t       *loader
    );                                         /// Set the target loader and allocate thread-local resources.

    void                      load
    (
        image_load_t const   &load_info
    );                                         /// Request that an image be loaded into memory.

    image_loader_t           *Loader;          /// The image loader to use for all requests.
    image_load_alloc_t        LoadAlloc;       /// The FIFO node allocator used to submit load requests for the thread.
};

/*///////////////
//   Globals   //
///////////////*/

/*///////////////////////
//   Local Functions   //
///////////////////////*/
/// @summary Adds an image definition to the loader, growing the image list if necessary.
/// @param loader The image loader that received the load request.
/// @param load The image load request.
/// @return The zero-based index of the image record.
internal_function size_t image_loader_add_image(image_loader_t *loader, image_load_t const &load)
{
    AcquireSRWLockShared(&loader->ImageLock);
    size_t image_index;
    if (id_table_get(&loader->ImageIds, load.ImageId, &image_index))
    {   // this image already exists.
        ReleaseSRWLockShared(&loader->ImageLock);
        return image_index;
    }
    ReleaseSRWLockShared(&loader->ImageLock);
    AcquireSRWLockExclusive(&loader->ImageLock);
    if (loader->ImageCount == loader->ImageCapacity)
    {   // grow the image list capacity.
        size_t old_amount   = loader->ImageCapacity;
        size_t new_amount   = calculate_capacity(old_amount, old_amount+1, 1024, 1024);
        image_definition_t *md = (image_definition_t*) realloc(loader->ImageMetadata, new_amount * sizeof(image_definition_t));
        if (md != NULL) loader->ImageMetadata = md;
        if (md != NULL) loader->ImageCapacity = new_amount;
    }
    image_index = loader->ImageCount++;
    image_definition_init(&loader->ImageMetadata[image_index]);
    id_table_put(&loader->ImageIds, load.ImageId,image_index);
    ReleaseSRWLockExclusive(&loader->ImageLock);
    return image_index;
}

/// @summary Enqueue a DDS stream to be loaded into image memory.
/// @param loader The image loader that received the request.
/// @param image_index The zero-based index of the image record in the loader's image list.
/// @param request The image load request.
/// @return true if the request was accepted and the load was started.
internal_function bool image_loader_start_dds(image_loader_t *loader, size_t image_index, image_load_t const &request)
{
    dds_parser_list_t *ddsp=&loader->ActiveDDS;
    image_parser_list_ensure(ddsp, ddsp->Count + 1);
    size_t parser_index    = ddsp->Count;
    stream_decoder_t  *dds = NULL; // ...
    uint32_t         flags = IMAGE_PARSER_FLAGS_READ_PIXELS | IMAGE_PARSER_FLAGS_START_AT_OFFSET;

    if (request.Metadata.ImageFormat == DXGI_FORMAT_UNKNOWN)
    {   // image data is not known, so be sure to read the metadata.
        flags |= IMAGE_PARSER_FLAGS_READ_METADATA;
    }
    if (request.FinalFrame == IMAGE_ALL_FRAMES)
    {
        if (request.FirstFrame == 0)
        {   // read metadata and pixels, load all frames.
            flags |= IMAGE_PARSER_FLAGS_READ_ALL;
        }
        else
        {   // reading only a range of frames.
            flags |= IMAGE_PARSER_FLAGS_FRAME_RANGE;
        }
    }
    else if (request.FirstFrame == request.FinalFrame)
    {   // reading only a single frame of the image.
        flags |= IMAGE_PARSER_FLAGS_SINGLE_FRAME;
    }
    else
    {   // reading a defined range of frames.
        flags |= IMAGE_PARSER_FLAGS_FRAME_RANGE;
    }

    // stream the file data in from beginning to end.
    // TODO(rlk): something more efficient if only reading a small portion?
    // if reading metadata, must go from beginning to end.
    // otherwise, only read the portion we're interested in.
    // currently, we lack a mechanism to do this in the I/O layer.
    if ((dds = loader->io.load_file(request.FilePath, request.FileHints, request.DecoderHint, request.ImageId, request.Priority, NULL)) == NULL)
    {   // unable to load the file - not found?
        return false;
    }
    if (request.FileOffset != 0)
    {   // seek the stream to the specified location.
        loader->io.seek_stream(request.ImageId, request.FileOffset);
    }

    // configure the DDS file parser. parsing doesn't begin until the next update.
    image_parser_config_t parse_config;
    parse_config.ImageId                  = request.ImageId;
    parse_config.FirstFrame               = request.FirstFrame;
    parse_config.FinalFrame               = request.FinalFrame;
    parse_config.Memory                   = loader->ImageMemory;
    parse_config.Decoder                  = dds;
    parse_config.Metadata                 =&loader->ImageMetadata[image_index];
    parse_config.DefinitionQueue          = loader->DefinitionQueue;
    parse_config.DefinitionAlloc          =&loader->DefinitionAlloc;
    parse_config.PlacementQueue           = loader->PlacementQueue;
    parse_config.PlacementAlloc           =&loader->PlacementAlloc;
    parse_config.ParseFlags               = flags;
    parse_config.Compression              = loader->Compression;
    parse_config.Encoding                 = loader->Encoding;
    parse_config.StartOffset.DecodeOffset = request.DecodeOffset;
    parse_config.StartOffset.FileOffset   = request.FileOffset;
    dds_parser_state_init(&ddsp->ParseState[parser_index], parse_config);

    // mark the parser as 'live':
    ddsp->SourceStream[parser_index] = dds;
    ddsp->SourceFile  [parser_index] = request.FilePath;
    ddsp->Count++;
    return true;
}

/// @summary Update the state of all active DDS parsers.
/// @param loader The image loader managing the active DDS parser list.
internal_function void image_loader_update_dds(image_loader_t *loader)
{   dds_parser_list_t *ddsp=&loader->ActiveDDS;
    size_t index = 0;
    while (index < ddsp->Count)
    {
        int res  = dds_parser_update(&ddsp->ParseState[index]);
        if (res == DDS_PARSE_RESULT_CONTINUE)
        {   // not finished parsing this stream yet.
            index++; continue;
        }
        if (res == DDS_PARSE_RESULT_ERROR && loader->ErrorQueue != NULL)
        {   // post the error to the error queue.
            fifo_node_t<image_load_error_t> *n = fifo_allocator_get(&loader->ErrorAlloc);
            n->Item.ImageId            = ddsp->SourceStream[index]->Identifier;
            n->Item.FilePath           = ddsp->SourceFile  [index];
            n->Item.FirstFrame         = ddsp->ParseState  [index].Config.FirstFrame;
            n->Item.FinalFrame         = ddsp->ParseState  [index].Config.FinalFrame;
            n->Item.DstCompression     = loader->Compression;
            n->Item.DstEncoding        = loader->Encoding;
            if (ddsp->ParseState[index].Encoder != NULL)
            {   // pick up the source data attributes from the encoder.
                n->Item.SrcCompression = ddsp->ParseState[index].Encoder->TargetCompression;
                n->Item.SrcEncoding    = ddsp->ParseState[index].Encoder->TargetEncoding;
            }
            else
            {   // if there's no encoder, we don't know what the compression was.
                n->Item.SrcCompression = IMAGE_COMPRESSION_NONE;
                n->Item.SrcEncoding    = IMAGE_ENCODING_RAW;
            }
            // determine the appropriate high-level error code.
            switch (ddsp->ParseState[index].ParserError)
            {
            case DDS_PARSE_ERROR_DECODER:
                n->Item.ErrorCode      = IMAGE_LOAD_ERROR_BAD_DATA;
                n->Item.OSError        = GetLastError();
                break;
            case DDS_PARSE_ERROR_NOMEMORY:
                n->Item.ErrorCode      = IMAGE_LOAD_ERROR_NO_MEMORY;
                n->Item.OSError        = GetLastError();
                break;
            case DDS_PARSE_ERROR_NOENCODER:
                n->Item.ErrorCode      = IMAGE_LOAD_ERROR_NO_ENCODER;
                n->Item.OSError        = ERROR_SUCCESS;
                break;
            case DDS_PARSE_ERROR_ENCODER:
                n->Item.ErrorCode      = IMAGE_LOAD_ERROR_BAD_DATA;
                n->Item.OSError        = ERROR_SUCCESS;
                break;
            default:
                n->Item.ErrorCode      = IMAGE_LOAD_ERROR_OSERROR;
                n->Item.OSError        = GetLastError();
                break;
            }
            // post the result to the target queue.
            mpsc_fifo_u_produce(loader->ErrorQueue, n);
        }
        // perform any parser state cleanup (delete the encoder, etc.)
        dds_parser_state_cleanup(&ddsp->ParseState[index]);
        // release the reference to the stream decoder.
        ddsp->SourceStream[index]->release();
        // remove the parser from the active list by swapping.
        size_t last_index = ddsp->Count - 1;
        ddsp->SourceStream[index] = ddsp->SourceStream[last_index];
        ddsp->SourceFile  [index] = ddsp->SourceFile  [last_index];
        ddsp->ParseState  [index] = ddsp->ParseState  [last_index];
        ddsp->Count--;
    }
}

/*////////////////////////
//   Public Functions   //
////////////////////////*/
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
public_function void image_loader_create(image_loader_t *loader, image_loader_config_t const &config)
{
    size_t capacity = config.ImageCapacity;
    if (capacity < IMAGE_LOADER_BUCKET_SIZE)
    {   // we need at least one bucket. enforce a minimum limit.
        capacity = IMAGE_LOADER_BUCKET_SIZE;
    }
    size_t bucket_count = capacity / IMAGE_LOADER_BUCKET_SIZE;

    mpsc_fifo_u_init(&loader->RequestQueue);
    loader->ImageMemory     = config.ImageMemory;
    loader->DefinitionQueue = config.DefinitionQueue;
    loader->PlacementQueue  = config.PlacementQueue;
    loader->ErrorQueue      = config.ErrorQueue;
    loader->Compression     = config.Compression;
    loader->Encoding        = config.Encoding;

    InitializeSRWLock(&loader->ImageLock);
    loader->ImageCount      = 0;
    loader->ImageCapacity   = capacity;
    loader->ImageMetadata   =(image_definition_t  *) malloc(capacity * sizeof(image_definition_t));
    id_table_create(&loader->ImageIds, bucket_count);

    loader->io.initialize(config.VFSDriver);
    image_parser_list_create(&loader->ActiveDDS, 16);

    fifo_allocator_init(&loader->DefinitionAlloc);
    fifo_allocator_init(&loader->PlacementAlloc);
    fifo_allocator_init(&loader->ErrorAlloc);
}

/// @summary Frees resources associated with an image loader instance.
/// @param loader The image loader to delete.
public_function void image_loader_delete(image_loader_t *loader)
{
    fifo_allocator_reinit(&loader->ErrorAlloc);
    fifo_allocator_reinit(&loader->PlacementAlloc);
    fifo_allocator_reinit(&loader->DefinitionAlloc);

    image_parser_list_delete(&loader->ActiveDDS);

    for (size_t i = 0, n = loader->ImageCount; i < n; ++i)
    {
        image_definition_free(&loader->ImageMetadata[i]);
    }
    free(loader->ImageMetadata);
    id_table_delete(&loader->ImageIds);
    loader->ImageCount    = 0;
    loader->ImageCapacity = 0;
    loader->ImageMetadata = NULL;

    mpsc_fifo_u_delete(&loader->RequestQueue);
}

/// @summary Queues a request to load one or more frames of image data.
/// @param loader The image loader that will track the load status.
/// @param load_info Information about the file to load and what data is being requested.
/// @param thread_alloc The FIFO node allocator used to write to the request queue from the current thread.
public_function void image_loader_queue_load(image_loader_t *loader, image_load_t const &load_info, image_load_alloc_t *thread_alloc)
{   // TODO(rlk): verify load info? it's usually produced by the cache...
    // also, potential lifetime issue with load_info.FilePath (except if load is generated from cache).
    fifo_node_t<image_load_t> *n = fifo_allocator_get(thread_alloc);
    n->Item = load_info;
    mpsc_fifo_u_produce(&loader->RequestQueue, n);
}

/// @summary Drives image loading, updating the state of all parsers and consuming new load requests.
/// @param loader The image loader to update.
public_function void image_loader_update(image_loader_t *loader)
{   // process any pending load requests:
    image_load_t load_info;
    while  (mpsc_fifo_u_consume(&loader->RequestQueue, load_info))
    {
        int fmt  = image_file_format_from_extension(load_info.FilePath);
        if (fmt == IMAGE_FILE_FORMAT_DDS)
        {
            size_t index = image_loader_add_image(loader, load_info);
            image_loader_start_dds(loader, index, load_info);
        }
        // else if (fmt == ...)
        else if (loader->ErrorQueue != NULL)
        {   // the loader doesn't recognize this container format. complete with an error.
            fifo_node_t<image_load_error_t> *n = fifo_allocator_get(&loader->ErrorAlloc);
            n->Item.ImageId        = load_info.ImageId;
            n->Item.FilePath       = load_info.FilePath;
            n->Item.FirstFrame     = load_info.FirstFrame;
            n->Item.FinalFrame     = load_info.FinalFrame;
            n->Item.SrcCompression = IMAGE_COMPRESSION_NONE;
            n->Item.SrcEncoding    = IMAGE_ENCODING_RAW;
            n->Item.DstCompression = IMAGE_COMPRESSION_NONE;
            n->Item.DstEncoding    = IMAGE_ENCODING_RAW;
            n->Item.ErrorCode      = IMAGE_LOAD_ERROR_NO_PARSER;
            n->Item.OSError        = ERROR_SUCCESS;
            mpsc_fifo_u_produce(loader->ErrorQueue, n);
        }
    }

    // update the state of all active parsers:
    image_loader_update_dds(loader);
    // ...
}

/// @summary Construct a new image loader instance for the calling thread.
thread_image_loader_t::thread_image_loader_t(void)
    :
    Loader(NULL)
{
    fifo_allocator_init(&LoadAlloc);
}

/// @summary Free thread-local resources.
thread_image_loader_t::~thread_image_loader_t(void)
{
    fifo_allocator_reinit(&LoadAlloc);
    Loader = NULL;
}

/// @summary Set the target image loader instance.
/// @param loader The target image loader.
void thread_image_loader_t::initialize(image_loader_t *loader)
{
    Loader = loader;
}

/// @summary Queue an image to be loaded into image memory.
/// @param load_info Information about the image data to load.
void thread_image_loader_t::load(image_load_t const &load_info)
{   // TODO(rlk): memory lifetime problem here with the FilePath.
    image_loader_queue_load(Loader, load_info, &LoadAlloc);
}
