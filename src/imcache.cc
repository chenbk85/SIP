/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines the interface to an image cache, which maintains metadata
/// for images known to the system, coordinates loading of image data into 
/// cache memory, and eviction of images to limit cache memory usage.
///////////////////////////////////////////////////////////////////////////80*/

// TODO(rlk): Review and clean up hard-coded integer constants
// TODO(rlk): Implement victim selection algorithm(s)

/*////////////////
//   Includes   //
////////////////*/

/*////////////////////
//   Preprocessor   //
////////////////////*/

/*/////////////////
//   Constants   //
/////////////////*/
/// @summary Define the default size of a hash bucket in the image cache ID table.
#ifndef IMAGE_CACHE_BUCKET_SIZE
#define IMAGE_CACHE_BUCKET_SIZE          128U
#endif

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Defines the support image cache control commands.
enum image_cache_command_e          : uint32_t
{
    IMAGE_CACHE_COMMAND_UNLOCK      = 0,          /// Unlock an image; its data is no longer needed.
    IMAGE_CACHE_COMMAND_LOCK        = 1,          /// Lock an image to access its data. Load it if necessary.
    IMAGE_CACHE_COMMAND_EVICT       = 2,          /// Evict frames when their lock count reaches zero.
    IMAGE_CACHE_COMMAND_DROP        = 3,          /// Evict all frames and drop an image as soon as its lock count reaches zero.
};

/// @summary Defines modifier options that can be specified with a cache control command.
enum image_cache_command_option_e   : uint32_t
{
    IMAGE_CACHE_COMMAND_OPTION_NONE =(0 << 0),    /// No special behavior is requested.
    IMAGE_CACHE_COMMAND_OPTION_EVICT=(1 << 0),    /// Valid on unlock. If the lock count after unlock is zero, drop the image.
};

/// @summary Defines status flags that can be set on cache entries.
enum image_cache_entry_flag_e       : uint32_t
{
    IMAGE_CACHE_ENTRY_FLAG_NONE     =(0 << 0),    /// No special status flags are set.
    IMAGE_CACHE_ENTRY_FLAG_EVICT    =(1 << 0),    /// The frame(s) should be dropped when its lock count reaches zero.
    IMAGE_CACHE_ENTRY_FLAG_DROP     =(1 << 1),    /// All information about the image should be deleted when it falls out of cache.
};

/// @summary Defines the supported victim selection behaviors of the image cache.
enum image_cache_behavior_e
{
    IMAGE_CACHE_BEHAVIOR_MANUAL              = 0, /// The cache does not automatically evict frames.
    IMAGE_CACHE_BEHAVIOR_IMAGE_LRU_FRAME_MRU = 1, /// The most recently used frame of the least recently used image is selected.
    // ...
};

/// @summary Defines the data associated with an image file declaration. 
/// A file may define one or more frames of a single logical image.
struct image_declaration_t
{
    uintptr_t            ImageId;                 /// The application-defined image identifer.
    char const          *FilePath;                /// A NULL-terminated, UTF-8 string specifying the virtual file path.
    size_t               FirstFrame;              /// The zero based index of the first frame defined in the file.
    size_t               FinalFrame;              /// The zero-based index of the last frame defined in the file, or IMAGE_ALL_FRAMES.
    uint32_t             FileHints;               /// A combination of vfs_file_hint_e to pass to the VFS driver when opening the file.
    int                  DecoderHint;             /// One of vfs_decoder_hint_e, or VFS_DECODER_HINT_USE_DEFAULT.
};
typedef fifo_allocator_t<image_declaration_t>         image_declaration_alloc_t;
typedef mpsc_fifo_u_t   <image_declaration_t>         image_declaration_queue_t;

/// @summary Defines the data returned when a cache control command has completed. Generally, 
/// only lock commands will return anything useful; for other command types, the ResultQueue
/// is set to NULL and no result is generated. In the case where a range of frames are locked, 
/// one result will be generated for each frame individually.
struct image_cache_result_t
{
    uint32_t             CommandId;               /// One of image_cache_command_e specifying the command type.
    uint32_t             ImageFormat;             /// One of dxgi_format_e specifying the storage format of the pixel data.
    uint32_t             Compression;             /// One of image_compression_e specifying the storage compression format of the pixel data.
    uint32_t             Encoding;                /// One of image_encoding_e specifying the storage encoding format for the pixel data.
    uintptr_t            ImageId;                 /// The application-defined identifier of the logical image.
    size_t               FrameIndex;              /// The zero-based frame index.
    size_t               LevelCount;              /// The number of levels in the mipmap chain.
    size_t               BytesPerPixel;           /// The number of bytes per-pixel of the image data.
    size_t               BytesPerBlock;           /// The number of bytes per-block, for block-compressed data, or zero.
    dds_header_t         DDSHeader;               /// Metadata defining basic image attributes.
    dds_header_dxt10_t   DX10Header;              /// Metadata defining extended image attributes.
    dds_level_desc_t    *LevelInfo;               /// An array of LevelCount entries describing the levels in the mipmap chain.
    void                *BaseAddress;             /// The base address of the frame data, if it was locked.
    size_t               BytesReserved;           /// The number of bytes of frame data, if the frame was locked.
};
typedef fifo_allocator_table_t<image_cache_result_t>  image_cache_result_alloc_table_t;
typedef fifo_allocator_t      <image_cache_result_t>  image_cache_result_alloc_t;
typedef mpsc_fifo_u_t         <image_cache_result_t>  image_cache_result_queue_t; // or SPSC?

/// @summary Defines the data returned when a cache control command completes with an error.
/// Generally, only lock commands will generate errors. In the case where the command operates
/// on a range of frames, only one error result will be generated for the entire range.
struct image_cache_error_t
{
    uint32_t             CommandId;               /// One of image_cache_command_e specifying the command type.
    DWORD                ErrorCode;               /// ERROR_SUCCESS, or a system error code.
    uintptr_t            ImageId;                 /// The application-defined identifier of the logical image.
    size_t               FirstFrame;              /// The zero-based index of the first frame of the request.
    size_t               FinalFrame;              /// The zero-based index of the last frame of the request, or IMAGE_ALL_FRAMES.
};
typedef fifo_allocator_table_t<image_cache_error_t>   image_cache_error_alloc_table_t;
typedef fifo_allocator_t      <image_cache_error_t>   image_cache_error_alloc_t;
typedef mpsc_fifo_u_t         <image_cache_error_t>   image_cache_error_queue_t;

/// @summary Defines the data specified with a cache update command, which will generate 
/// some sequence of operations (images may be evicted, loads may be generated, and so on.)
struct image_cache_command_t
{   typedef image_cache_error_queue_t                 error_queue_t;
    typedef image_cache_result_queue_t                result_queue_t;
    uint32_t             CommandId;               /// One of image_cache_command_e specifying the operation to perform.
    uint32_t             Options;                 /// A combination of image_cache_command_option_e.
    uintptr_t            ImageId;                 /// The application-defined identifier of the logical image.
    size_t               FirstFrame;              /// The zero-based index of the first frame to operate on.
    size_t               FinalFrame;              /// The zero-based index of the last frame to operate on, or IMAGE_ALL_FRAMES.
    uint8_t              Priority;                /// The priority value to use when loading the file (if necessary.)
    error_queue_t       *ErrorQueue;              /// The queue in which error results should be placed, or NULL.
    result_queue_t      *ResultQueue;             /// The queue in which successful completion results should be placed, or NULL.
};
typedef fifo_allocator_t<image_cache_command_t>       image_command_alloc_t;
typedef mpsc_fifo_u_t   <image_cache_command_t>       image_command_queue_t;

/// @summary Defines the parameters controlling cache behavior. The cache behavior may 
/// be modified at runtime, for example, to tune memory usage.
struct image_cache_config_t
{
    size_t               CacheSize;               /// The maximum amount of cache memory to use, in bytes.
    int                  Behavior;                /// One of image_cache_behavior_e defining how victims are selected.
};

/// @summary Defines the data associated with a single image data file. The file may 
/// define one or more frames of image data.
struct image_file_t
{
    char const          *FilePath;                /// The NULL-terminated UTF-8 virtual file path.
    size_t               FirstFrame;              /// The zero-based index of the first frame defined in the file.
    size_t               FinalFrame;              /// The zero-based index of the last frame defined in the file, or IMAGE_ALL_FRAMES.
};

/// @summary Defines the set of image files that make up a single logical image, as 
/// well as any information necessary to open the files.
struct image_files_data_t
{
    uintptr_t            ImageId;                 /// The application-defined identifier of the logical image.
    int                  FileHints;               /// A combination of vfs_file_hint_e specifying how the files should be opened.
    int                  DecoderHint;             /// One of vfs_decoder_hint_e specifying the type of decoder to create.
    size_t               FileCount;               /// The number of individual files making up the image.
    size_t               FileCapacity;            /// The number of file definitions that can be stored.
    image_file_t        *FileList;                /// Information about each individual file.
};

/// @summary Maintains a dynamic list of queues to be notified of some condition
/// when a single frame of an image has finished being loaded. 
template <typename T>
struct frame_load_queue_list_t
{
    size_t               QueueCount;              /// The number of queues to notify.
    size_t               QueueCapacity;           /// The allocated capacity of the queue list.
    T                  **QueueList;               /// The set of queues to notify.
};

/// @summary Defines the set of data associated with an outstanding load request. 
/// The queue lists are tracked separately because the same error queue may be 
/// specified with every request (so error handling/reporting can be done in one place)
/// while several different request queues may be specified.
struct image_loads_data_t
{   typedef frame_load_queue_list_t<image_cache_error_queue_t > error_queues_t;
    typedef frame_load_queue_list_t<image_cache_result_queue_t> result_queues_t;
    uintptr_t            ImageId;                 /// The application-defined identifier of the logical image.
    size_t               TotalFrames;             /// The total number of frames defined on the image, or IMAGE_ALL_FRAMES if unknown.
    size_t               FrameCount;              /// The number of frames waiting to load.
    size_t               FrameCapacity;           /// The number of frame indices that can be stored in FrameList.
    size_t              *FrameList;               /// The set of frame indices waiting to be loaded.
    uint64_t            *RequestTime;             /// The set of frame initial request times, in nanoseconds.
    error_queues_t      *ErrorQueues;             /// The set of frame load error queues.
    result_queues_t     *ResultQueues;            /// The set of frame load result queues.
};

/// @summary Defines the metadata maintained for each logical image.
/// This is essentially the same data supplied with the image definition.
struct image_basic_data_t
{
    uintptr_t            ImageId;                 /// The application-defined logical image identifier.
    uint32_t             ImageFormat;             /// One of dxgi_format_e specifying the storage format of the pixel data.
    uint32_t             Compression;             /// One of image_compression_e specifying the compression format of the pixel data.
    uint32_t             Encoding;                /// One of image_encoding_e specifying the storage format encoding.
    size_t               Width;                   /// The width of the highest-resolution level of the image, in pixels.
    size_t               Height;                  /// The height of the highest-resolution level of the image, in pixels.
    size_t               SliceCount;              /// The number of slices in the highest-resolution level of the image.
    size_t               ElementCount;            /// The number of array elements, cubemap faces, or frames.
    size_t               LevelCount;              /// The number of levels in the mipmap chain.
    size_t               BytesPerPixel;           /// The number of bytes per-pixel of the image data.
    size_t               BytesPerBlock;           /// The number of bytes per-block, for block-compressed data, or zero.
    dds_header_t         DDSHeader;               /// Metadata defining basic image attributes.
    dds_header_dxt10_t   DX10Header;              /// Metadata defining extended image attributes.
    dds_level_desc_t    *LevelInfo;               /// An array of LevelCount entries describing the levels in the mipmap chain.
    stream_decode_pos_t *BlockOffsets;            /// An array of LevelCount * ElementCount entries specifying the byte offsets of each level in the source file.
};
#define IMAGE_BASIC_DATA_STATIC_INIT(iid) \
    { (iid), DXGI_FORMAT_UNKNOWN, IMAGE_COMPRESSION_NONE, IMAGE_ENCODING_RAW, 0, 0, 0, 0, 0, 0, 0, {}, {}, NULL, NULL }

/// @summary Defines per-frame image memory location information maintained for a 
/// single entry in the image cache and updated from an image_location_t request.
struct image_frame_info_t
{
    uintptr_t            Context;                 /// Opaque data used to locate the image.
    void                *BaseAddress;             /// The address at which the frame data begins.
    size_t               BytesReserved;           /// The number of bytes of cache memory reserved for the frame.
};

/// @summary Defines the data used when deciding which frames to evict from the cache.
struct image_cache_info_t
{
    uint32_t             LockCount;               /// The number of outstanding locks against this frame.
    uint32_t             Attributes;              /// A combination of image_cache_entry_flags_e applied to the frame.
    uint64_t             LastRequestTime;         /// The timestamp at which the frame was last locked.
    uint64_t             TimeToLoad;              /// The approximate amount of time required to reload the frame from disk.
};

/// @summary Defines the data associated with a single logical image in the cache.
struct image_cache_entry_t
{
    uintptr_t            ImageId;                 /// The application-defined identifier of the logical image.
    uint32_t             Attributes;              /// A combination of image_cache_entry_flags_e applied to the image.
    uint64_t             LastRequestTime;         /// The timestamp at which any frame from this image was last locked.
    size_t               FrameCount;              /// The number of frames currently loaded into cache memory.
    size_t               FrameCapacity;           /// The capacity of the internal frame data lists.
    size_t              *FrameList;               /// The unordered list of frame indices.
    image_frame_info_t  *FrameData;               /// The unordered list of frame memory location information.
    image_cache_info_t  *FrameState;              /// The unordered list of frame cache state information.
};

/// @summary Define the queue and allocator types used for emitting frame eviction notifications.
typedef fifo_allocator_t<image_location_t>            image_eviction_alloc_t;
typedef spsc_fifo_u_t   <image_location_t>            image_eviction_queue_t;

/// @summary Defines the data and queues associated with an image cache. The image 
/// cache is not responsible for allocating or committing image memory.
struct image_cache_t
{   typedef image_cache_result_alloc_table_t          result_alloc_table_t;
    typedef image_cache_error_alloc_table_t           error_alloc_table_t;
    typedef image_declaration_alloc_t                 declaration_alloc_t;
    typedef image_declaration_queue_t                 declaration_queue_t;
    typedef image_definition_alloc_t                  definition_alloc_t;
    typedef image_definition_queue_t                  definition_queue_t;
    typedef image_location_alloc_t                    location_alloc_t;
    typedef image_location_queue_t                    location_queue_t;
    typedef fifo_allocator_t<image_load_t>            load_alloc_t;
    typedef spsc_fifo_u_t   <image_load_t>            load_queue_t;
    typedef image_eviction_alloc_t                    eviction_alloc_t;
    typedef image_eviction_queue_t                    eviction_queue_t;
    typedef image_command_alloc_t                     command_alloc_t;
    typedef image_command_queue_t                     command_queue_t;

    LARGE_INTEGER          ClockFrequency;        /// The tick frequency of the system high-resolution timer.

    SRWLOCK                AttribLock;            /// Lock protecting cache behavior data.
    size_t                 LimitBytes;            /// The maximum number of bytes of cached image data.
    size_t                 TotalBytes;            /// The current number of bytes of cached image data.
    int                    BehaviorId;            /// One of image_cache_behavior_e specifying the cache behavior mode.

    SRWLOCK                MetadataLock;          /// Lock protecting access to the image metadata.
    size_t                 ImageCount;            /// The number of logical images currently defined.
    size_t                 ImageCapacity;         /// The total number of allocated storage slots in the metadata lists.
    id_table_t             ImageIds;              /// The table mapping logical image ID to metadata list index.
    image_files_data_t    *FileData;              /// The list of image file metadata.
    image_basic_data_t    *MetaData;              /// The list of image attribute metadata.

    size_t                 EntryCount;            /// The number of currently used storage slots in EntryList.
    size_t                 EntryCapacity;         /// The total number of allocated storage slots in EntryList.
    id_table_t             EntryIds;              /// The internal table of mapping logical image ID to cache state.
    image_cache_entry_t   *EntryList;             /// The list of cache entries for images with at least one in-cache frame.

    size_t                 LoadCount;             /// The number of outstanding load requests.
    size_t                 LoadCapacity;          /// The total number of allocated storage slots in LoadList.
    id_table_t             LoadIds;               /// The list of image IDs for outstanding load requests. May contain duplicates.
    image_loads_data_t    *LoadList;              /// The list of unique outstanding load request state.

    load_queue_t           LoadQueue;             /// The SPSC output queue for image load requests.
    load_alloc_t           LoadAlloc;             /// The internal FIFO node allocator for the image load request queue.

    eviction_queue_t       EvictQueue;            /// The SPSC output queue for image eviction requests.
    eviction_alloc_t       EvictAlloc;            /// The internal FIFO node allocator for the image eviction queue.

    declaration_queue_t    DeclarationQueue;      /// The MPSC input queue for image declaration requests.
    definition_queue_t     DefinitionQueue;       /// The MPSC input queue for image definition requests.
    location_queue_t       LocationQueue;         /// The MPSC input queue for cache memory location updates.
    command_queue_t        CommandQueue;          /// The MPSC input queue for cache control commands.

    error_alloc_table_t    ErrorAlloc;            /// The table of FIFO node allocators for posting error results to client queues.
    result_alloc_table_t   ResultAlloc;           /// The table of FIFO node allocators for posting lock results to client queues.
};

/// @summary Defines the usage statistics that can be returned by an image cache instance.
struct image_cache_stat_t
{
    size_t                 BytesLimit;            /// The configured memory budget, in bytes.
    size_t                 BytesUsed;             /// The number of bytes currently in-use.
};

/// @summary Defines the client interface to an image cache from a single thread.
struct thread_image_cache_t
{   typedef image_cache_result_queue_t                result_queue_t;
    typedef image_cache_error_queue_t                 error_queue_t;
    typedef image_declaration_alloc_t                 declaration_alloc_t;
    typedef image_command_alloc_t                     command_alloc_t;

    thread_image_cache_t(void);
    ~thread_image_cache_t(void);

    void initialize
    (
        image_cache_t     *cache
    );                                            /// Set the target cache.

    void configure
    (
        image_cache_config_t const &cfg
    );                                            /// Reconfigure the target cache.

    void add_source
    (
        uintptr_t          id, 
        char const        *path, 
        size_t             first_frame  = 0, 
        size_t             final_frame  = IMAGE_ALL_FRAMES, 
        uint32_t           file_hints   = VFS_FILE_HINT_NONE, 
        int                decoder_hint = VFS_DECODER_HINT_USE_DEFAULT
    );                                            /// Define the file source of one or more image frames.

    void stat
    (
        image_cache_stat_t &stats 
    );                                            /// Synchronously retrieve cache statistics.

    bool image_attributes
    (
        uintptr_t          id, 
        image_basic_data_t&attribs
    );                                            /// Synchronoulsy retrieve image attributes.

    void lock
    (
        uintptr_t          id, 
        size_t             first_frame, 
        size_t             final_frame, 
        result_queue_t    *result_queue, 
        error_queue_t     *error_queue, 
        uint8_t            priority
    );                                            /// Asynchronously lock one or more frames into cache memory.

    void unlock
    (
        uintptr_t          id, 
        size_t             first_frame, 
        size_t             final_frame, 
        uint32_t           options      = IMAGE_CACHE_COMMAND_OPTION_NONE
    );                                            /// Asynchronously unlock one or more frames in cache memory.

    void evict
    (
        uintptr_t          id
    );                                            /// Asynchronously evict all frames of an image from cache memory.

    void drop
    (
        uintptr_t          id
    );                                            /// Asynchronoulsy evict all frames of an image, and drop the image record.

    image_cache_t         *Cache;                 /// The target image cache.
    command_alloc_t        CommandAlloc;          /// The FIFO node allocator for the thread, used to submit control commands.
    declaration_alloc_t    DeclarationAlloc;      /// The FIFO node allocator for the thread, used to submit frame source definitions.
};

/*///////////////
//   Globals   //
///////////////*/

/*///////////////////////
//   Local Functions   //
///////////////////////*/
/// @summary Initialize, and possible pre-allocate space for storing a set of 
/// pointers to queues of some type. Each item in the set is unique.
/// @param list The queue list to initialize.
/// @param capacity The initial capacity of the queue list.
template <typename T>
inline void frame_load_queue_list_create(frame_load_queue_list_t<T> *list, size_t capacity)
{
    list->QueueCount    = 0;
    list->QueueCapacity = 0;
    list->QueueList     = NULL;
    if (capacity > 0)
    {   // preallocate storage for the specified number of items.
        T **storage  = (T**)malloc(capacity * sizeof(T*));
        if (storage != NULL)
        {
            list->QueueList      = storage;
            list->QueueCapacity  = capacity;
        }
    }
}

/// @summary Free resources associated with a frame load queue list.
/// @param list The queue list to delete.
template <typename T>
inline void frame_load_queue_list_delete(frame_load_queue_list_t<T> *list)
{
    free(list->QueueList); 
    list->QueueCount     = 0;
    list->QueueCapacity  = 0;
    list->QueueList      = NULL;
}

/// @summary Mark a frame load queue list as empty without freeing any memory.
/// @param list The queue list to clear.
template <typename T>
inline void frame_load_queue_list_clear(frame_load_queue_list_t<T> *list)
{
    list->QueueCount = 0;
}

/// @summary Store a queue reference in a frame load queue list. If the specified
/// queue already exists in the list, it is not re-added. NULL items are excluded.
/// @param list The queue list to update.
/// @param queue The queue reference to store.
template <typename T>
inline void frame_load_queue_list_put(frame_load_queue_list_t<T> *list, T *queue)
{
    if (queue == NULL)
    {   // don't allow NULL entries to be inserted into the list.
        return;
    }
    // search for this item in the list. we only want to generate one event notification.
    for (size_t i = 0, n = list->QueueCount; i < n; ++i)
    {
        if (list->QueueList[i] == queue)
            return;
    }
    // queue not found in the queue list; add it.
    if (list->QueueCount == list->QueueCapacity)
    {
        size_t old_amount = list->QueueCapacity;
        size_t new_amount = calculate_capacity(old_amount, old_amount+1, 8, 8);
        T    **new_queues =(T**) realloc(list->QueueList , new_amount * sizeof(T*));
        if (new_queues != NULL)
        {   // memory allocation was successful. update state.
            list->QueueList     = new_queues;
            list->QueueCapacity = new_amount;
        }
        // else, memory allocation failed. how to handle?
    }
    if (list->QueueCount < list->QueueCapacity)
    {   // proceed with adding the new item to the list.
        list->QueueList[list->QueueCount++] = queue;
    }
}

/// @summary Reads the current tick count for use as a timestamp.
/// @return The current timestamp value, in nanoseconds.
internal_function inline uint64_t image_cache_nanotime(image_cache_t *cache)
{
    uint64_t const  SEC_TO_NANOSEC = 1000000000ULL;
    LARGE_INTEGER   counts; QueryPerformanceCounter(&counts);
    return uint64_t(SEC_TO_NANOSEC * (double(counts.QuadPart) / double(cache->ClockFrequency.QuadPart)));
}

/// @summary Normalize a single character for path comparisons.
/// @param ch The character to normalize.
/// @return If ch specifies an upper-case ASCII character, it is converted to lower case. If ch specifies a backslash, it is converted to a forward slash.
internal_function inline char image_cache_path_normalize(char ch)
{
    return ((ch >= 'A' && ch <= 'Z') ? ((ch - 'A') + 'a') : ((ch == '\\') ? '/' : ch));
}

/// @summary Compares two path strings for equality.
/// @param a A NULL-terminated path string.
/// @param b A NULL-terminated path string.
/// @return true if paths a and b represent the same entity.
internal_function bool image_cache_path_match(char const *a, char const *b)
{
    if (a == b)
    {   // easy out.
        return true;
    }
    if (a == NULL || b == NULL)
    {   // easy out.
        return false;
    }
    char ca, cb; // both a and b are non-NULL.
    do
    {
        ca = image_cache_path_normalize(*a++);
        cb = image_cache_path_normalize(*b++);
    } 
    while  (ca == cb && ca != 0);
    return (ca == cb);
}

/// @summary Immediately drop an image record. The image can no longer be reloaded into cache.
/// @param cache The image cache to query and update.
/// @param image_id The application-defined identifier of the logical image to delete.
/// @return true if the specified image was found and deleted.
internal_function bool image_cache_drop_image_record(image_cache_t *cache, uintptr_t image_id)
{
    image_files_data_t files  = {};
    image_basic_data_t attribs= {};
    bool  deleted_item = false;

    AcquireSRWLockExclusive(&cache->MetadataLock);
    size_t meta_index;
    size_t last_index  = cache->ImageCount - 1;
    if (id_table_remove(&cache->ImageIds, image_id, &meta_index))
    {   // copy the records to delete so we aren't freeing memory while holding the lock.
        files   = cache->FileData[meta_index];
        attribs = cache->MetaData[meta_index];
        // delete the item from the file and metadata lists.
        if (meta_index != last_index)
        {   // swap the last list entry into slot meta_index.
            uintptr_t moved_id = cache->FileData[last_index].ImageId;
            id_table_update(&cache->ImageIds, moved_id, meta_index, NULL);
            cache->FileData[meta_index] = cache->FileData[last_index];
            cache->MetaData[meta_index] = cache->MetaData[last_index];
        }
        cache->ImageCount--;
        deleted_item = true;
    }
    ReleaseSRWLockExclusive(&cache->MetadataLock);

    if (deleted_item)
    {   // free memory associated with file path strings.
        for (size_t i = 0, n = files.FileCount; i < n; ++i)
        {
            free((void*)files.FileList[i].FilePath);
            files.FileList[i].FilePath = NULL;
        }
        free(files.FileList); files.FileList = NULL;
        files.FileCapacity  = 0;
        files.FileCount     = 0;

        // free memory associated with the image metadata.
        free(attribs.BlockOffsets); attribs.BlockOffsets = NULL;
        free(attribs.LevelInfo);    attribs.LevelInfo    = NULL;
        attribs.ImageFormat  = DXGI_FORMAT_UNKNOWN;
        attribs.ElementCount = 0;
        attribs.LevelCount   = 0;
    }
    return deleted_item;
}

/// @summary Processes any pending frame eviction and image deletion for a cache entry.
/// If, after evicting frames, the entry has no frames in-cache, it is deleted from the cache table.
/// @param cache The image cache to update.
/// @param entry_index The zero-based index of the cache entry to process.
internal_function void image_cache_process_pending_evict_and_drop(image_cache_t *cache, size_t entry_index)
{
    image_cache_entry_t &entry = cache->EntryList[entry_index];
    size_t       bytes_dropped = 0;

    // generate eviction commands for any marked frames. 
    for (size_t i = 0; i < entry.FrameCount; /* empty */)
    {   // we can only drop frames with a lock count of zero.
        if ((entry.FrameState[i].LockCount == 0) && 
            (entry.FrameState[i].Attributes & IMAGE_CACHE_ENTRY_FLAG_EVICT) != 0)
        {   // evict this frame from cache memory.
            fifo_node_t<image_location_t>*n = fifo_allocator_get(&cache->EvictAlloc);
            n->Item.ImageId       = entry.ImageId;
            n->Item.FrameIndex    = entry.FrameList[i];
            n->Item.BaseAddress   = entry.FrameData[i].BaseAddress;
            n->Item.BytesReserved = entry.FrameData[i].BytesReserved;
            n->Item.Context       = entry.FrameData[i].Context;
            spsc_fifo_u_produce(&cache->EvictQueue, n);
            // consider the frame to have been immediately evicted.
            bytes_dropped += entry.FrameData[i].BytesReserved;
            // remove it from the list of in-cache frames.
            array_swap(entry.FrameList , i, entry.FrameCount-1);
            array_swap(entry.FrameData , i, entry.FrameCount-1);
            array_swap(entry.FrameState, i, entry.FrameCount-1);
            entry.FrameCount--;
        }
        else i++;
    }

    // if any frames were evicted, update the cache usage.
    if (bytes_dropped > 0)
    {
        AcquireSRWLockExclusive(&cache->AttribLock);
        cache->TotalBytes -= bytes_dropped;
        ReleaseSRWLockExclusive(&cache->AttribLock);
    }

    // if the entry has no frames in-cache, drop it from the list(s).
    if (entry.FrameCount == 0)
    {   // update the list of cache entries.
        size_t       last_idx = cache->EntryCount - 1;
        uintptr_t    moved_id = cache->EntryList[last_idx].ImageId;
        uintptr_t    entry_id = entry.ImageId;
        if (entry_id != moved_id)
        {   // swap the last cache entry into this item's slot.
            cache->EntryList[entry_index] = cache->EntryList[last_idx];
            id_table_update(&cache->EntryIds, moved_id, entry_index, NULL);
        }
        id_table_remove(&cache->EntryIds, entry_id, NULL);
        cache->EntryCount--;

        // if the image is marked to be dropped, update the metadata list.
        if (entry.Attributes & IMAGE_CACHE_ENTRY_FLAG_DROP)
        {   // this will also free the associated declaration and definition.
            image_cache_drop_image_record(cache, entry_id);
        }
    }
}

/// @summary Processes a command to unlock one or more image frames.
/// @param cache The image cache that received the command.
/// @param cmd The unlock command to process.
internal_function void image_cache_process_unlock(image_cache_t *cache, image_cache_command_t const &cmd)
{
    size_t index;
    if (id_table_get(&cache->EntryIds, cmd.ImageId , &index))
    {   // this image has a corresponding entry in the cache, so perform the unlock.
        image_cache_entry_t &entry = cache->EntryList[index];
        uint32_t         add_flags = IMAGE_CACHE_ENTRY_FLAG_NONE;

        // determine whether frames should be marked for eviction. 
        // frames may be marked either via an unlock flag, or via 
        // an image-global setting. note that IMAGE_CACHE_ENTRY_FLAG_DROP
        // implies IMAGE_CACHE_ENTRY_FLAG_EVICT.
        if ((cmd.Options      & IMAGE_CACHE_COMMAND_OPTION_EVICT) != 0 || 
            (entry.Attributes & IMAGE_CACHE_ENTRY_FLAG_EVICT    ) != 0 || 
            (entry.Attributes & IMAGE_CACHE_ENTRY_FLAG_DROP     ) != 0)
        {   // we'll tag each frame as pending-eviction.
            add_flags |= IMAGE_CACHE_ENTRY_FLAG_EVICT;
        }

        // update the lock count on any affected frames.
        for (size_t i = 0, n = entry.FrameCount; i < n; ++i)
        {   // unlock any frames in the specified range.
            if (entry.FrameList[i] >= cmd.FirstFrame && 
                entry.FrameList[i] <= cmd.FinalFrame)
            {   // apply any pending drop attribute to the frame...
                entry.FrameState[i].Attributes |= add_flags;
                // ...and then update the lock count for the frame.
                if (entry.FrameState[i].LockCount > 0)
                {   // the lock count will drop to be >= 0.
                    entry.FrameState[i].LockCount--;
                }
            }
        }

        // we might have marked frames for eviction, so process that status.
        image_cache_process_pending_evict_and_drop(cache, index);
    }
}

/// @summary Processes a command to evict one or more image frames.
/// @param cache The image cache that received the command.
/// @param cmd The frame eviction command to process.
internal_function void image_cache_process_evict(image_cache_t *cache, image_cache_command_t const &cmd)
{
    size_t index;
    if (id_table_get(&cache->EntryIds, cmd.ImageId , &index))
    {   // this image has a corresponding entry in the cache.
        image_cache_entry_t &entry = cache->EntryList[index];

        // mark all frames in the specified range for eviction.
        for (size_t i = 0, n = entry.FrameCount; i < n; ++i)
        {   // unlock any frames in the specified range.
            if (entry.FrameList[i] >= cmd.FirstFrame && 
                entry.FrameList[i] <= cmd.FinalFrame)
            {   // apply any pending drop attribute to the frame...
                entry.FrameState[i].Attributes |= IMAGE_CACHE_ENTRY_FLAG_EVICT;
            }
        }

        // evict any frames that don't have any active locks.
        image_cache_process_pending_evict_and_drop(cache, index);
    }
}

/// @summary Proceses a command to evict all frames of an image, and then delete the image record.
/// @param cache The image cache that received the command.
/// @param cmd The image drop command to process.
internal_function void image_cache_process_drop(image_cache_t *cache, image_cache_command_t const &cmd)
{
    size_t index;
    if (id_table_get(&cache->EntryIds, cmd.ImageId , &index))
    {   // this image has a corresponding entry in the cache, so mark all frames for eviction.
        // ensure that the associated image will be dropped when all frames are unlocked.
        image_cache_entry_t &entry = cache->EntryList[index];
        entry.Attributes |= IMAGE_CACHE_ENTRY_FLAG_DROP;

        // mark all frames in the image for eviction, regardless of the range in cmd.
        for (size_t i = 0, n = entry.FrameCount; i < n; ++i)
        {
            entry.FrameState[i].Attributes |= IMAGE_CACHE_ENTRY_FLAG_EVICT;
        }

        // evict any frames that don't have active locks. this may or may not 
        // also delete the image record right away - if not, it will be deleted
        // when all in-cache frames have been unlocked.
        image_cache_process_pending_evict_and_drop(cache, index);
    }
    else
    {   // this image does not have any entry in the cache, so just delete it.
        image_cache_drop_image_record(cache, cmd.ImageId);
    }
}

/// @summary Generates a completion event for a lock command for a single frame.
/// @param cache The image cache that processed the lock command.
/// @param result_queue The result queue to which the completion event will be posted.
/// @param loc Information about the placement of the frame in cache memory.
/// @return The function always returns ERROR_SUCCESS.
internal_function uint32_t image_cache_complete_lock(image_cache_t *cache, image_cache_command_t::result_queue_t *result_queue, image_location_t const &loc, image_basic_data_t const &meta)
{
    if (result_queue != NULL)
    {   // get the producer allocator from the table. one will be created if necessary.
        image_cache_result_alloc_t    *alloc = fifo_allocator_table_get(&cache->ResultAlloc, result_queue);
        fifo_node_t<image_cache_result_t> *n = fifo_allocator_get(alloc);
        n->Item.CommandId      = IMAGE_CACHE_COMMAND_LOCK;
        n->Item.ImageFormat    = meta.ImageFormat;
        n->Item.Compression    = meta.Compression;
        n->Item.Encoding       = meta.Encoding;
        n->Item.ImageId        = loc.ImageId;
        n->Item.FrameIndex     = loc.FrameIndex;
        n->Item.LevelCount     = meta.LevelCount;
        n->Item.BytesPerPixel  = meta.BytesPerPixel;
        n->Item.BytesPerBlock  = meta.BytesPerBlock;
        n->Item.DDSHeader      = meta.DDSHeader;
        n->Item.DX10Header     = meta.DX10Header;
        n->Item.LevelInfo      = meta.LevelInfo;
        n->Item.BaseAddress    = loc.BaseAddress;
        n->Item.BytesReserved  = loc.BytesReserved;
        mpsc_fifo_u_produce(result_queue, n);
    }
    return ERROR_SUCCESS;
}

/// @summary Generates an error event for an image cache control command.
/// @param cache The image cache that processed the command.
/// @param cmd The command being completed.
/// @param error The system error code to return to the application.
/// @return The error code specified by @a error.
internal_function uint32_t image_cache_complete_error(image_cache_t *cache, image_cache_command_t const &cmd, uint32_t error)
{
    if (cmd.ErrorQueue != NULL)
    {   // get the producer allocator from the table. one will be created if necessary.
        image_cache_error_alloc_t     *alloc = fifo_allocator_table_get(&cache->ErrorAlloc, cmd.ErrorQueue);
        fifo_node_t<image_cache_error_t>  *n = fifo_allocator_get(alloc);
        n->Item.CommandId  = cmd.CommandId;
        n->Item.ErrorCode  = error;
        n->Item.ImageId    = cmd.ImageId;
        n->Item.FirstFrame = cmd.FirstFrame;
        n->Item.FinalFrame = cmd.FinalFrame;
        mpsc_fifo_u_produce(cmd.ErrorQueue, n);
    }
    return error;
}

/// @summary Initializes a new cache entry for a given image.
/// @param cache The image cache managing the image.
/// @param image_id The application-defined identifier of the image associated with the cache entry.
/// @param now_time The current cache update timestamp, specified in nanoseconds.
/// @param index On return, this location is updated with the zero-based index of the new entry.
/// @return Either ERROR_SUCCESS or a system error code.
internal_function uint32_t image_cache_add_entry(image_cache_t *cache, uintptr_t image_id, uint64_t now_time, size_t &index)
{
    if (cache->EntryCount == cache->EntryCapacity)
    {   // need to grow the cache entry list.
        size_t old_amount  = cache->EntryCapacity;
        size_t new_amount  = calculate_capacity(old_amount, old_amount+1, 4096, 1024);
        image_cache_entry_t *new_list = (image_cache_entry_t*) realloc(cache->EntryList, new_amount * sizeof(image_cache_entry_t));
        if (new_list != NULL)
        {   // the list was successfully reallocated. save the new values.
            cache->EntryList     = new_list;
            cache->EntryCapacity = new_amount;
        }
        else return ERROR_OUTOFMEMORY;

        // initialize the fields of the newly-allocated entries.
        size_t new_first = cache->EntryCount;
        size_t new_count = cache->EntryCapacity - cache->EntryCount;
        memset(&cache->EntryList[new_first], 0, new_count * sizeof(image_cache_entry_t));
    }
    // initialize the fields of the new entry.
    index = cache->EntryCount++;
    image_cache_entry_t &entry = cache->EntryList[index];
    entry.ImageId         = image_id;
    entry.Attributes      = IMAGE_CACHE_ENTRY_FLAG_NONE;
    entry.LastRequestTime = now_time;
    entry.FrameCount      = 0;
    // if we have any metadata for the image, pre-allocate the frame data lists.
    AcquireSRWLockShared(&cache->MetadataLock);
    size_t meta_index     = 0;
    size_t frame_count    = 0;
    if (id_table_get(&cache->ImageIds, image_id, &meta_index))
    {   // save off the frame count.
        frame_count = cache->MetaData[meta_index].ElementCount;
    }
    ReleaseSRWLockShared(&cache->MetadataLock);
    if (frame_count > entry.FrameCapacity)
    {   // grow each of the frame data lists to match the expected count.
        size_t             *fl = (size_t            *) realloc(entry.FrameList , frame_count * sizeof(size_t));
        image_frame_info_t *fi = (image_frame_info_t*) realloc(entry.FrameData , frame_count * sizeof(image_frame_info_t));
        image_cache_info_t *ci = (image_cache_info_t*) realloc(entry.FrameState, frame_count * sizeof(image_cache_info_t));
        if (fl != NULL) entry.FrameList  = fl;
        if (fi != NULL) entry.FrameData  = fi;
        if (ci != NULL) entry.FrameState = ci;
        if (fl != NULL && fi != NULL && ci != NULL) entry.FrameCapacity = frame_count;
        memset(entry.FrameList, 0, frame_count * sizeof(size_t));
        memset(entry.FrameData, 0, frame_count * sizeof(image_frame_info_t));
        memset(entry.FrameState,0, frame_count * sizeof(image_cache_info_t));
    }
    // finally, update the image ID->cache entry table.
    id_table_put(&cache->EntryIds, image_id, index);
    return ERROR_SUCCESS;
}

/// @summary Updates a pending load record to include a new frame. The frame is only
/// added to the pending load record if it doesn't already exist; otherwise, the 
/// response queues are stored and the requestor will be notified when the frame is 
/// ready, or when an error has occurred.
/// @param load The frame load record to update.
/// @param frame_index The zero-based index of the frame to load, or IMAGE_ALL_FRAMES.
/// @param new_frame On return, this value is set to true if this frame was not previously marked to load.
/// @param error On return, this value is updated with ERROR_SUCCESS, or a system error code.
/// @return The zero-based index of the frame record in the frame list.
internal_function size_t image_cache_frame_load_list_put(image_loads_data_t &load, size_t frame_index, bool &new_item, uint32_t &error)
{   typedef image_loads_data_t::error_queues_t  equeue_t;
    typedef image_loads_data_t::result_queues_t rqueue_t;
    // if the frame is already in the list, just return the existing index.
    for (size_t i = 0, n = load.FrameCount; i < n; ++i)
    {
        if (load.FrameList[i] == frame_index || load.FrameList[i] == IMAGE_ALL_FRAMES)
        {   // there's a load pending for this frame. 
            new_item = false;
            return i;
        }
    }
    // the frame is not in the pending load list, so create a new entry.
    if (load.FrameCount == load.FrameCapacity)
    {   // increase the capacity of the frame list.
        size_t  old_amount = load.FrameCapacity;
        size_t  new_amount = calculate_capacity(old_amount, old_amount+1, 1024, 128);
        size_t         *nf =(size_t  *)realloc (load.FrameList   , new_amount * sizeof(size_t));
        uint64_t       *nt =(uint64_t*)realloc (load.RequestTime , new_amount * sizeof(uint64_t));
        equeue_t       *ne =(equeue_t*)realloc (load.ErrorQueues , new_amount * sizeof(equeue_t));
        rqueue_t       *nr =(rqueue_t*)realloc (load.ResultQueues, new_amount * sizeof(rqueue_t));
        if (nf != NULL) load.FrameList      = nf;
        if (nt != NULL) load.RequestTime    = nt;
        if (ne != NULL) load.ErrorQueues    = ne;
        if (nr != NULL) load.ResultQueues   = nr;
        if (nf != NULL && nt != NULL && ne != NULL && nr != NULL)
        {   // all lists reallocated successfully. update capacity.
            load.FrameCapacity = new_amount;
            // initialize any new queue lists.
            size_t new_first   = old_amount;
            size_t new_count   = new_amount - old_amount;
            for (size_t n = 0, i = new_first; n < new_count; ++n, ++i)
            {
                frame_load_queue_list_create(&load.ErrorQueues[i] , 1);
                frame_load_queue_list_create(&load.ResultQueues[i], 8);
            }
        }
        else
        {   // unable to re-allocate one or more lists. don't update capacity.
            error = ERROR_OUTOFMEMORY;
            return IMAGE_ALL_FRAMES;
        }
    }
    else
    {   // use an existing, but currently unused item.
        size_t index = load.FrameCount;
        frame_load_queue_list_clear(&load.ErrorQueues [index]);
        frame_load_queue_list_clear(&load.ResultQueues[index]);
    }
    new_item = true;
    error    = ERROR_SUCCESS;
    return load.FrameCount++;
}

/// @summary Generate the load notification for a single frame.
/// @param cache The image cache processing the lock request.
/// @param load The pending-load record for the image, which will be updated with a new frame load record.
/// @param cmd The image lock command that's initiating the load request.
/// @param image_info Known information about the image.
/// @param file_info Information about the mapping of frames to source files.
/// @param frame_index The zero-based index of the frame to load, or IMAGE_ALL_FRAMES.
/// @param now_time The nanosecond timestamp of the current update tick, used for measuring load time.
/// @return ERROR_SUCCESS or a system error code.
internal_function uint32_t image_cache_load_frame(image_cache_t *cache, image_loads_data_t &load, image_cache_command_t const &cmd, image_basic_data_t const &image_info, image_files_data_t const &file_info, size_t frame_index, uint64_t now_time)
{
    size_t   first_frame = frame_index;
    size_t   final_frame = frame_index;
    bool     new_frame   = false;
    uint32_t error       = ERROR_SUCCESS;
    size_t   list_index  = image_cache_frame_load_list_put(load, frame_index, new_frame, error);
    if (FAILED(error))     return error;

    stream_decode_pos_t frame_pos;
    if (frame_index < image_info.ElementCount)
    {   // supply the byte offset of the start of the frame.
        frame_pos   = image_info.BlockOffsets[frame_index * image_info.LevelCount];
    }
    else// frame_index = IMAGE_ALL_FRAMES
    {   // begin loading from the beginning of the file.
        frame_pos.FileOffset   = 0;
        frame_pos.DecodeOffset = 0;
        first_frame            = 0;
    }
    
    if (new_frame)
    {   // initialize the frame pending-load state.
        load.FrameList  [list_index] = frame_index;
        load.RequestTime[list_index] = now_time;
        // submit the load requests for each frame.
        uint32_t    file_error = ERROR_NOT_FOUND;
        for (size_t file_index = 0, file_count = file_info.FileCount; file_index < file_count; ++file_index)
        {
            image_file_t const &f = file_info.FileList[file_index];
            if (frame_index >=  f.FirstFrame && frame_index <= f.FinalFrame)
            {   // found the matching source file record. emit the notification.
                // convert any known image data to an image_definition_t format.
                fifo_node_t<image_load_t> *n  = fifo_allocator_get(&cache->LoadAlloc);
                n->Item.Metadata.ImageId        = cmd.ImageId;
                n->Item.Metadata.ImageFormat    = image_info.ImageFormat;
                n->Item.Metadata.Compression    = image_info.Compression;
                n->Item.Metadata.Width          = image_info.Width;
                n->Item.Metadata.Height         = image_info.Height;
                n->Item.Metadata.SliceCount     = image_info.SliceCount;
                n->Item.Metadata.ElementIndex   = 0;
                n->Item.Metadata.ElementCount   = image_info.ElementCount;
                n->Item.Metadata.LevelCount     = image_info.LevelCount;
                n->Item.Metadata.BytesPerPixel  = image_info.BytesPerPixel;
                n->Item.Metadata.BytesPerBlock  = image_info.BytesPerBlock;
                n->Item.Metadata.DDSHeader      = image_info.DDSHeader;
                n->Item.Metadata.DX10Header     = image_info.DX10Header;
                n->Item.Metadata.LevelInfo      = image_info.LevelInfo;
                n->Item.Metadata.BlockOffsets   = image_info.BlockOffsets;
                n->Item.ImageId                 = cmd.ImageId;
                n->Item.FilePath                = f.FilePath;
                n->Item.FirstFrame              = first_frame;
                n->Item.FinalFrame              = final_frame;
                n->Item.DecodeOffset            = frame_pos.DecodeOffset;
                n->Item.FileOffset              = frame_pos.FileOffset;
                n->Item.FileHints               = file_info.FileHints;
                n->Item.DecoderHint             = file_info.DecoderHint;
                n->Item.Priority                = cmd.Priority;
                spsc_fifo_u_produce(&cache->LoadQueue, n);
                file_error = ERROR_SUCCESS;
                break;
            }
        }
        if (FAILED(file_error))
        {
            return file_error;
        }
    }
    // add the queues to the notify lists.
    frame_load_queue_list_put(&load.ErrorQueues [list_index], cmd.ErrorQueue);
    frame_load_queue_list_put(&load.ResultQueues[list_index], cmd.ResultQueue);
    return ERROR_SUCCESS;
}

/// @summary Generates pending load records and load requests for one or more frames.
/// @param cache The image cache processing the lock request.
/// @param cmd The image lock command that's initiating the load request.
/// @param image_info Known information about the image.
/// @param first_frame The zero-based index of the first frame to start loading.
/// @param final_frame The zero-based index of the last frame to load, or IMAGE_ALL_FRAMES.
/// @param now_time The nanosecond timestamp of the current update tick, used for measuring load time.
/// @return ERROR_SUCCESS or a system error code.
internal_function uint32_t image_cache_submit_load(image_cache_t *cache, image_cache_command_t const &cmd, image_basic_data_t const &image_info, size_t first_frame, size_t final_frame, uint64_t now_time)
{   // locate the information describing the mapping of frames to files.
    image_files_data_t    file_info;
    size_t                file_index;
    AcquireSRWLockShared(&cache->MetadataLock);
    if (id_table_get(&cache->ImageIds, cmd.ImageId, &file_index))
    {   // save off the file mapping information.
        file_info   = cache->FileData[file_index];
        ReleaseSRWLockShared(&cache->MetadataLock);
    }
    else
    {   // this image has no file records - we can't load it.
        ReleaseSRWLockShared(&cache->MetadataLock);
        return ERROR_NOT_FOUND;
    }

    // locate any existing pending-load information for this image:
    size_t load_index;
    if (id_table_get(&cache->LoadIds, cmd.ImageId, &load_index) == false)
    {   // there's no existing load for this image. create one.
        if (cache->LoadCount == cache->LoadCapacity)
        {   // need to grow the list of in-progress load state.
            size_t old_amount = cache->LoadCapacity;
            size_t new_amount = calculate_capacity(old_amount, old_amount+1, 1024, 128);
            image_loads_data_t *ld_list =(image_loads_data_t*) realloc(cache->LoadList, new_amount * sizeof(image_loads_data_t));
            if (ld_list != NULL)
            {   // the list was reallocated successfully. save the new state.
                cache->LoadList     = ld_list;
                cache->LoadCapacity = new_amount;
                // initialize the new list entries to NULL/0.
                size_t new_index    = cache->LoadCount;
                size_t new_count    = new_amount - old_amount;
                memset(&cache->LoadList[new_index], 0, new_count * sizeof(image_loads_data_t));
            }
            else return ERROR_OUTOFMEMORY;
        }
        // grab an unused item from the list.
        load_index = cache->LoadCount++;
        image_loads_data_t &ldinit = cache->LoadList[load_index];
        size_t  nknown     = image_info.ElementCount;
        size_t  frames     = nknown ? nknown : IMAGE_ALL_FRAMES;
        ldinit.ImageId     = cmd.ImageId;
        ldinit.TotalFrames = frames;
        ldinit.FrameCount  = 0;
        // insert the new item into the image ID-> load index table.
        id_table_put(&cache->LoadIds, cmd.ImageId, load_index);
    }

    // finally, generate load requests for the individual frames.
    image_loads_data_t &load = cache->LoadList[load_index];
    if (final_frame == IMAGE_ALL_FRAMES)
    {   // we're loading all frames, but don't know how many there are.
        // generate one load request for all frames with index IMAGE_ALL_FRAMES.
        uint32_t   e = image_cache_load_frame(cache, load, cmd, image_info, file_info, IMAGE_ALL_FRAMES, now_time);
        if (FAILED(e)) return e;
    }
    else
    {   // we're loading a known range of frames and have a valid frame count.
        // generate one load request for each frame not already pending load.
        for (size_t frame_index = first_frame ; frame_index <= final_frame; ++frame_index)
        {
            uint32_t   e = image_cache_load_frame(cache, load, cmd, image_info, file_info, frame_index, now_time);
            if (FAILED(e)) return e;
        }
    }
    return ERROR_SUCCESS;
}

/// @summary Process a command to lock one or more frames of an image into cache memory.
/// @param cache The image cache processing the command.
/// @param cmd Data supplied with the lock command. 
/// @param now_time The nanosecond timestamp of the current update tick, used for measuring load time.
/// @return ERROR_SUCCESS, ERROR_IO_PENDING, or a system error code.
internal_function uint32_t image_cache_process_lock(image_cache_t *cache, image_cache_command_t const &cmd, uint64_t now_time)
{
    size_t meta_index;
    image_basic_data_t image_info = IMAGE_BASIC_DATA_STATIC_INIT(cmd.ImageId);
    AcquireSRWLockShared(&cache->MetadataLock);
    if (id_table_get(&cache->ImageIds, cmd.ImageId, &meta_index))
    {   // save a copy of the image metadata.
        image_info = cache->MetaData[meta_index];
        ReleaseSRWLockShared(&cache->MetadataLock);
    }
    else
    {   // this image isn't known at all, so we can't lock it.
        ReleaseSRWLockShared(&cache->MetadataLock);
        return ERROR_NOT_FOUND;
    }

    size_t cache_index;
    if (id_table_get(&cache->EntryIds, cmd.ImageId, &cache_index) == false)
    {   // this image has no frames in cache. create a new cache entry.
        uint32_t error = image_cache_add_entry(cache, cmd.ImageId, now_time, cache_index);
        if (FAILED(error)) return error;
    }

    // save off the cache entry, which will be updated below.
    // figure out the actual set of frames being requested.
    image_cache_entry_t &entry = cache->EntryList[cache_index];
    size_t first_frame = cmd.FirstFrame;
    size_t final_frame = cmd.FinalFrame;
    if (image_info.ElementCount != 0)
    {   // this image has been loaded previously, we have metadata.
        if (cmd.FinalFrame >= image_info.ElementCount)
        {   // replace IMAGE_ALL_FRAMES with the last frame index.
            final_frame = image_info.ElementCount - 1;
        }
    }
    else
    {   // we don't have any metadata, so we don't know how many frames
        // there are, and we don't know what the frame offsets are.
        first_frame  = 0;
        final_frame  = IMAGE_ALL_FRAMES;
    }

    // check whether each frame is in-cache, and if not, request it be loaded.
    // the only way final_frame is IMAGE_ALL_FRAMES at this point is if we 
    // don't have any image metadata to know the frame count, and in this
    // case we'll always stream in the entire image.
    if (final_frame != IMAGE_ALL_FRAMES)
    {   // check to see if any or all of the requested frames are in-cache.
        // if so, complete these immediately; generate pending loads for misses.
        uint32_t error_pending  = ERROR_IO_PENDING;
        size_t frames_in_cache  = 0;
        size_t frames_requested =(final_frame - first_frame) + 1;
        for (size_t frame_index = first_frame ; frame_index <= final_frame; ++frame_index)
        {   bool load_the_frame = true;
            // the frame index list is unordered, so check every element.
            for (size_t i = 0, n = entry.FrameCount; i < n; ++i)
            {
                if (entry.FrameList[i] == frame_index)
                {   // update the state of the cache entry. increment the lock 
                    // count so that the frame won't get evicted from cache memory.
                    entry.FrameState[i].LastRequestTime = now_time;
                    entry.FrameState[i].LockCount++;
                    // complete the lock request for the caller.
                    image_location_t loc;
                    loc.ImageId        = cmd.ImageId;
                    loc.FrameIndex     = frame_index;
                    loc.BaseAddress    = entry.FrameData[i].BaseAddress;
                    loc.BytesReserved  = entry.FrameData[i].BytesReserved;
                    loc.Context        = entry.FrameData[i].Context;
                    image_cache_complete_lock(cache, cmd.ResultQueue, loc, image_info);
                    // no need to re-load this frame into cache.
                    load_the_frame = false;
                    frames_in_cache++;
                    break;
                }
            }
            if (load_the_frame)
            {   // submit a single-frame pending load to the queue.
                uint32_t   err = image_cache_submit_load(cache, cmd, image_info, frame_index, frame_index, now_time);
                if (FAILED(err))
                {   // immediately complete with an error result.
                    error_pending = image_cache_complete_error(cache, cmd, err);
                }
            }
        }
        if (frames_in_cache == frames_requested)
        {   // the lock request has completed. nothing needs to be loaded.
            return ERROR_SUCCESS;
        }
        return error_pending;
    }
    else
    {   // load every frame of the image into cache. this is represented by 
        // a single load request. in this case, no frames are currently loaded.
        uint32_t   err = image_cache_submit_load(cache, cmd, image_info, 0, IMAGE_ALL_FRAMES, now_time);
        if (FAILED(err))
        {   // immediately complete with an error result.
            return image_cache_complete_error(cache, cmd, err);
        }
        return ERROR_IO_PENDING;
    }
}

/// @summary Updates the internal table of image definitions to define a new image, or a new frame in an existing image.
/// @param cache The image cache to update.
/// @param decl The image declaration.
internal_function void image_cache_define_image(image_cache_t *cache, image_declaration_t const &decl)
{
    size_t index;
    if (id_table_get(&cache->ImageIds, decl.ImageId , &index))
    {   // there's an existing entry; update it.
        image_files_data_t &file_info= cache->FileData[index];
        // search for an existing file definition. if found, don't re-add.
        for (size_t i = 0, n = file_info.FileCount; i < n; ++i)
        {
            if (file_info.FileList[i].FinalFrame == IMAGE_ALL_FRAMES)
            {   // a record defining all frames is already present.
                return;
            }
            if (decl.FirstFrame >= file_info.FileList[i].FirstFrame &&
                decl.FinalFrame <= file_info.FileList[i].FinalFrame)
            {   // a record defining these frames is already present.
                return;
            }
        }
        // this is a new file being defined for the image.
        if (file_info.FileCount == file_info.FileCapacity)
        {   // grow the file list capacity.
            size_t old_amount    = file_info.FileCapacity;
            size_t new_amount    = calculate_capacity(old_amount, old_amount+1, 64, 64);
            image_file_t  *nl    =(image_file_t*)realloc(file_info.FileList, new_amount * sizeof(image_file_t));
            if (nl != NULL)
            {   // update the list data pointer and capacity.
                file_info.FileList = nl;
                file_info.FileCapacity = new_amount;
            }
            else return; // unable to allocate memory
        }
        // insert the new record into the list.
        file_info.FileList[file_info.FileCount].FilePath   = decl.FilePath;
        file_info.FileList[file_info.FileCount].FirstFrame = decl.FirstFrame;
        file_info.FileList[file_info.FileCount].FinalFrame = decl.FinalFrame;
        file_info.FileCount++;
    }
    else
    {   // this is a new image definition; create it.
        if (cache->ImageCount == cache->ImageCapacity)
        {   // increase the capacity of the image lists.
            size_t old_amount  = cache->ImageCapacity;
            size_t new_amount  = calculate_capacity(old_amount, old_amount+1, 1024, 1024);
            image_files_data_t *nf = (image_files_data_t*) realloc(cache->FileData, new_amount * sizeof(image_files_data_t));
            image_basic_data_t *nm = (image_basic_data_t*) realloc(cache->MetaData, new_amount * sizeof(image_basic_data_t));
            if (nf != NULL) cache->FileData = nf;
            if (nm != NULL) cache->MetaData = nm;
            if (nf != NULL && nm != NULL)
            {   // initialize the fields of the new items.
                cache->ImageCapacity = new_amount;
                size_t new_first  = old_amount;
                size_t new_count  = new_amount - old_amount;
                memset(&cache->FileData[new_first], 0, new_count * sizeof(image_files_data_t));
                memset(&cache->MetaData[new_first], 0, new_count * sizeof(image_basic_data_t));
            }
            else return;
        }
        image_files_data_t &file_info    = cache->FileData[cache->ImageCount];
        image_basic_data_t &meta_data    = cache->MetaData[cache->ImageCount];
        file_info.ImageId                = decl.ImageId;
        file_info.FileCount              = 1;
        file_info.FileCapacity           = 1;
        file_info.FileList               =(image_file_t*) malloc(sizeof(image_file_t));
        file_info.FileList[0].FilePath   = decl.FilePath;
        file_info.FileList[0].FirstFrame = decl.FirstFrame;
        file_info.FileList[0].FinalFrame = decl.FinalFrame;
        file_info.FileHints              = decl.FileHints;
        file_info.DecoderHint            = decl.DecoderHint;
        meta_data = IMAGE_BASIC_DATA_STATIC_INIT(decl.ImageId);
        // make the new record visible externally.
        id_table_put(&cache->ImageIds, decl.ImageId, cache->ImageCount);
        cache->ImageCount++;
    }
}

/// @summary Updates the internal table of image metadata.
/// @param cache The image cache that received the image metadata.
/// @param def The image attributes.
internal_function void image_cache_update_image_definition(image_cache_t *cache, image_definition_t const &def)
{
    size_t index;
    if (id_table_get(&cache->ImageIds, def.ImageId, &index))
    {   // allocated memory will be freed when the image is dropped.
        image_basic_data_t &meta = cache->MetaData[index];
        if (meta.ImageFormat    == DXGI_FORMAT_UNKNOWN)
        {   // store the basic image attributes as none have yet been set.
            meta.ImageId         = def.ImageId;
            meta.ImageFormat     = def.ImageFormat;
            meta.Compression     = def.Compression;
            meta.Encoding        = def.Encoding;
            meta.Width           = def.Width;
            meta.Height          = def.Height;
            meta.SliceCount      = def.SliceCount;
            meta.ElementCount    = 0;                 // set below
            meta.LevelCount      = 0;                 // set below
            meta.BytesPerPixel   = def.BytesPerPixel;
            meta.BytesPerBlock   = def.BytesPerBlock;
            meta.DDSHeader       = def.DDSHeader;
            meta.DX10Header      = def.DX10Header;
            meta.LevelInfo       = NULL;              // copied below
            meta.BlockOffsets    = NULL;              // copied below
            if (def.LevelCount   > 0)
            {   // copy the mip-level descriptors over.
                // these remain constant for all elements.
                size_t copy_bytes= def.LevelCount  *  sizeof(dds_level_desc_t);
                meta.LevelCount  = def.LevelCount;
                meta.LevelInfo   =(dds_level_desc_t*) malloc(copy_bytes);
                memcpy(meta.LevelInfo, def.LevelInfo, copy_bytes);
            }
        }
        if (meta.ElementCount <= def.ElementIndex)
        {   // ensure that file offsets get copied to the correct location.
            // it's possible that frame 0 might not be the first frame received.
            size_t   new_count    = def.ElementIndex + def.ElementCount;
            size_t   new_bytes    = new_count * meta.LevelCount * sizeof(stream_decode_pos_t);
            stream_decode_pos_t    *new_offsets = (stream_decode_pos_t*) realloc(meta.BlockOffsets, new_bytes);
            if (new_offsets != NULL)
            {   // copy the file offsets for the new elements.
                size_t  ofs_index = def.ElementIndex * meta.LevelCount;
                size_t copy_bytes = def.ElementCount * meta.LevelCount * sizeof(stream_decode_pos_t);
                meta.ElementCount = new_count;
                meta.BlockOffsets = new_offsets;
                memcpy(&meta.BlockOffsets[ofs_index], def.BlockOffsets, copy_bytes);
            }
        }
    }
}

/// @summary Updates the cache status in response to a notification that an image frame has been loaded or moved in cache memory.
/// @param cache The image cache to update.
/// @param pos The location of the image frame in cache memory.
/// @param now_time The nanosecond timestamp of the current update tick.
/// @return ERROR_SUCCESS, or a system error code.
internal_function uint32_t image_cache_update_location(image_cache_t *cache, image_location_t const &pos, uint64_t now_time)
{   
    uint64_t  t_start   = 0;
    bool   lock_frame   = false;
    size_t load_index   = 0;
    size_t total_frames = 0;

    // determine the total number of image elements.
    AcquireSRWLockShared(&cache->MetadataLock);
    size_t meta_index;
    image_basic_data_t image_info;
    if (id_table_get(&cache->ImageIds, pos.ImageId, &meta_index))
    {   // save off the total number of image elements/frames.
        image_info    = cache->MetaData[meta_index];
        total_frames  = cache->MetaData[meta_index].ElementCount;
    }
    ReleaseSRWLockShared(&cache->MetadataLock);
    if (total_frames == 0)
    {   // the image is not known - ignore the update request.
        return ERROR_NOT_FOUND;
    }

    // retire pending load commands by posting to their result queue.
    if (id_table_get(&cache->LoadIds, pos.ImageId, &load_index))
    {   // locate the completed frame in the load list, and emit the error or result.
        image_loads_data_t  &load = cache->LoadList[load_index];

        // we may not have known the number of frames beforehand; for example, the 
        // initial load of an image sequence where all frames are stored in one file.
        // in this case, we need to create new frame load records from the metadata 
        // that would have been set when parsing the source file(s).
        if (load.TotalFrames == IMAGE_ALL_FRAMES)
        {   // update the load request with the real frame count.
            load.TotalFrames  = total_frames;

            // the load for frame IMAGE_ALL_FRAMES becomes the load for pos.FrameIndex.
            // save off the index for the IMAGE_ALL_FRAMES record. we'll need it to 
            // copy data over to the new frame records.
            size_t all_frames_ix = 0;
            for (size_t i = 0, n = load.FrameCount; i < n; ++i)
            {
                if (load.FrameList[i] == IMAGE_ALL_FRAMES)
                {
                    load.FrameList[i]  = pos.FrameIndex;
                    all_frames_ix      = i;
                    break;
                }
            }

            // set up frame list records for each frame.
            for (size_t i = 0, n = load.TotalFrames; i < n; ++i)
            {
                bool      new_frame  = false;
                uint32_t  error      = ERROR_SUCCESS;
                size_t    list_index = image_cache_frame_load_list_put(load, i, new_frame, error);
                if (new_frame)
                {   // copy the request time to the new record.
                    load.FrameList  [list_index] = i;
                    load.RequestTime[list_index] = load.RequestTime[all_frames_ix];
                }
                if (list_index != all_frames_ix)
                {   // copy the request and error queues to the new record.
                    for (size_t j = 0, m = load.ErrorQueues[all_frames_ix].QueueCount; j < m; ++j)
                    {
                        frame_load_queue_list_put(
                            &load.ErrorQueues[list_index], 
                             load.ErrorQueues[all_frames_ix].QueueList[j]);
                    }
                    for (size_t j = 0, m = load.ResultQueues[all_frames_ix].QueueCount; j < m; ++j)
                    {  
                        frame_load_queue_list_put(
                            &load.ResultQueues[list_index], 
                             load.ResultQueues[all_frames_ix].QueueList[j]);
                    }
                }
            }
        }
        
        // complete the load for the specified frame.
        for (size_t frame_index = 0, frame_count = load.FrameCount; frame_index < frame_count; ++frame_index)
        {
            if (load.FrameList[frame_index] == pos.FrameIndex)
            {   // post the lock result to every registered queue.
                for (size_t i = 0, n = load.ResultQueues[frame_index].QueueCount; i < n; ++i)
                {
                    image_cache_complete_lock(cache, load.ResultQueues[frame_index].QueueList[i], pos, image_info);
                }
                // images are only ever loaded in response to a lock request.
                // indicate that we need to increment the lock count, and also
                // save off the request time to track frame load time.
                lock_frame = true;
                t_start    = load.RequestTime[frame_index];
                // the frame has finished loading, so remove it from the list.
                frame_load_queue_list_clear(&load.ErrorQueues [frame_index]);
                frame_load_queue_list_clear(&load.ResultQueues[frame_index]);
                // remove from the list by swapping the last item into place.
                size_t this_frame = frame_index;
                size_t last_frame = load.FrameCount - 1;
                array_swap(load.FrameList   , this_frame, last_frame);
                array_swap(load.RequestTime , this_frame, last_frame);
                array_swap(load.ErrorQueues , this_frame, last_frame);
                array_swap(load.ResultQueues, this_frame, last_frame);
                load.FrameCount--;
                // if there are no frames remaining to load, remove the record.
                if (load.FrameCount == 0)
                {   // remove from the list by swapping the last item into place.
                    size_t this_load = load_index;
                    size_t last_load = cache->LoadCount - 1;
                    if (this_load != last_load)
                    {   // update the ID look up table so it can find the relocated item.
                        id_table_update(&cache->LoadIds, cache->LoadList[last_load].ImageId, load_index, NULL);
                        cache->LoadList[this_load]     = cache->LoadList[last_load];
                    }
                    // remove the unused item from the ID look up table.
                    id_table_remove(&cache->LoadIds, pos.ImageId, NULL);
                    cache->LoadCount--;
                }
                break;
            }
        }
    }

    size_t entry_index;
    if (id_table_get(&cache->EntryIds, pos.ImageId, &entry_index) == false)
    {   // there's no corresponding cache entry, so create one.
        uint32_t error = image_cache_add_entry(cache, pos.ImageId, now_time, entry_index);
        if (FAILED(error)) return error;
    }

    // update the state of the frame in the cache.
    bool             found_frame = false;
    image_cache_entry_t   &entry = cache->EntryList[entry_index];
    for (size_t i = 0, n = entry.FrameCount; i < n; ++i)
    {
        if (entry.FrameList[i] == pos.FrameIndex)
        {   // store the updated frame location information.
            entry.FrameData[i].BaseAddress   = pos.BaseAddress;
            entry.FrameData[i].BytesReserved = pos.BytesReserved;
            entry.FrameData[i].Context       = pos.Context;
            // update the cache data of the entry, if it was just loaded.
            if (lock_frame)
            {
                entry.FrameState[i].LockCount++;
                entry.FrameState[i].Attributes      = IMAGE_CACHE_ENTRY_FLAG_NONE;
                entry.FrameState[i].LastRequestTime = now_time;
                entry.FrameState[i].TimeToLoad      = now_time - t_start;
            }
            found_frame = true;
            break;
        }
    }
    if (!found_frame)
    {   // this frame is being added to the cache.
        size_t   frame_index  = entry.FrameCount;
        if (entry.FrameCount == entry.FrameCapacity)
        {   // grow entry frame list storage.
            size_t             *fl = (size_t            *) realloc(entry.FrameList , total_frames * sizeof(size_t));
            image_frame_info_t *fi = (image_frame_info_t*) realloc(entry.FrameData , total_frames * sizeof(image_frame_info_t));
            image_cache_info_t *ci = (image_cache_info_t*) realloc(entry.FrameState, total_frames * sizeof(image_cache_info_t));
            if (fl != NULL) entry.FrameList  = fl;
            if (fi != NULL) entry.FrameData  = fi;
            if (ci != NULL) entry.FrameState = ci;
            if (fl != NULL && fi != NULL && ci != NULL) entry.FrameCapacity = total_frames;
        }
        // update the frame with location data.
        entry.FrameList [frame_index]                 = pos.FrameIndex;
        entry.FrameData [frame_index].BaseAddress     = pos.BaseAddress;
        entry.FrameData [frame_index].BytesReserved   = pos.BytesReserved;
        entry.FrameData [frame_index].Context         = pos.Context;
        entry.FrameState[frame_index].LockCount       = 1;
        entry.FrameState[frame_index].Attributes      = IMAGE_CACHE_ENTRY_FLAG_NONE;
        entry.FrameState[frame_index].LastRequestTime = now_time;
        entry.FrameState[frame_index].TimeToLoad      = now_time - t_start;
        entry.FrameCount++;
        // update the total number of bytes used.
        AcquireSRWLockExclusive(&cache->AttribLock);
        cache->TotalBytes += pos.BytesReserved;
        ReleaseSRWLockExclusive(&cache->AttribLock);
    }

    // retrieve the cache memory load status.
    int    behavior_id; 
    size_t bytes_total = 0;
    size_t bytes_limit = 0;
    AcquireSRWLockShared(&cache->AttribLock);
    behavior_id = cache->BehaviorId;
    bytes_limit = cache->LimitBytes;
    bytes_total = cache->TotalBytes;
    ReleaseSRWLockShared(&cache->AttribLock);

    // are we over the allowed memory budget?
    if (bytes_total > bytes_limit)
    {   // over memory budget, so we need to evict some frames.
        switch (behavior_id)
        {
        case IMAGE_CACHE_BEHAVIOR_MANUAL:
            // do nothing in this case. it is up to the user to 
            // select images or frames and evict them manually.
            // TODO(rlk): maybe we want to emit some event?
            // otherwise, how will "the user" know?
            break;

        case IMAGE_CACHE_BEHAVIOR_IMAGE_LRU_FRAME_MRU:
            // ...
            break;
        }
    }
    return ERROR_SUCCESS;
}

/*////////////////////////
//   Public Functions   //
////////////////////////*/
/// @summary Initialize a new image cache.
/// @param cache The image cache to create.
/// @param expected_image_count The number of unique images expected to be defined.
/// @param config Behavior and size configuration options for the cache.
public_function void image_cache_create(image_cache_t *cache, size_t expected_image_count, image_cache_config_t const &config)
{
    if (expected_image_count < IMAGE_CACHE_BUCKET_SIZE)
    {   // enforce a minimum value.
        expected_image_count = IMAGE_CACHE_BUCKET_SIZE;
    }
    size_t bucket_count = expected_image_count / IMAGE_CACHE_BUCKET_SIZE;

    QueryPerformanceFrequency(&cache->ClockFrequency);

    InitializeSRWLock(&cache->AttribLock);
    cache->LimitBytes = config.CacheSize;
    cache->BehaviorId = config.Behavior;
    cache->TotalBytes = 0;

    InitializeSRWLock(&cache->MetadataLock);
    id_table_create(&cache->ImageIds, bucket_count);
    cache->FileData      = NULL;
    cache->MetaData      = NULL;
    cache->ImageCount    = 0;
    cache->ImageCapacity = 0;

    id_table_create(&cache->EntryIds, bucket_count);
    cache->EntryList     = NULL;
    cache->EntryCount    = 0;
    cache->EntryCapacity = 0;

    id_table_create(&cache->LoadIds, bucket_count);
    cache->LoadList      = NULL;
    cache->LoadCount     = 0;
    cache->LoadCapacity  = 0;

    spsc_fifo_u_init(&cache->LoadQueue);
    fifo_allocator_init(&cache->LoadAlloc);

    spsc_fifo_u_init(&cache->EvictQueue);
    fifo_allocator_init(&cache->EvictAlloc);

    mpsc_fifo_u_init(&cache->DeclarationQueue);
    mpsc_fifo_u_init(&cache->DefinitionQueue);
    mpsc_fifo_u_init(&cache->LocationQueue);
    mpsc_fifo_u_init(&cache->CommandQueue);

    fifo_allocator_table_create(&cache->ErrorAlloc, 1);
    fifo_allocator_table_create(&cache->ResultAlloc, 8);
}

/// @summary Frees resources associated with an image cache instance.
/// @param cache The image cache to delete.
public_function void image_cache_delete(image_cache_t *cache)
{
    fifo_allocator_table_delete(&cache->ResultAlloc);
    fifo_allocator_table_delete(&cache->ErrorAlloc);

    mpsc_fifo_u_delete(&cache->CommandQueue);
    mpsc_fifo_u_delete(&cache->LocationQueue);
    mpsc_fifo_u_delete(&cache->DefinitionQueue);
    mpsc_fifo_u_delete(&cache->DeclarationQueue);

    spsc_fifo_u_delete(&cache->EvictQueue);
    fifo_allocator_reinit(&cache->EvictAlloc);

    spsc_fifo_u_delete(&cache->LoadQueue);
    fifo_allocator_reinit(&cache->LoadAlloc);

    for (size_t i = 0, n = cache->LoadCapacity; i < n; ++i)
    {
        for (size_t j = 0, m = cache->LoadList[i].FrameCapacity; j < m; ++j)
        {
            frame_load_queue_list_delete(&cache->LoadList[i].ErrorQueues [j]);
            frame_load_queue_list_delete(&cache->LoadList[i].ResultQueues[j]);
        }
        free(cache->LoadList[i].ErrorQueues);
        free(cache->LoadList[i].ResultQueues);
        free(cache->LoadList[i].RequestTime);
        free(cache->LoadList[i].FrameList);
    }
    free(cache->LoadList);
    cache->LoadCount    = 0;
    cache->LoadCapacity = 0;
    cache->LoadList     = NULL;
    id_table_delete(&cache->LoadIds);

    for (size_t i = 0, n = cache->EntryCapacity; i < n; ++i)
    {
        free(cache->EntryList[i].FrameState);
        free(cache->EntryList[i].FrameData);
        free(cache->EntryList[i].FrameList);
    }
    free(cache->EntryList);
    cache->EntryCount    = 0;
    cache->EntryCapacity = 0;
    cache->EntryList     = NULL;
    id_table_delete(&cache->EntryIds);

    for (size_t i = 0, n = cache->ImageCapacity; i < n; ++i)
    {   // clean up file data (file list, file paths):
        for (size_t j = 0, m = cache->FileData[i].FileCapacity; j < m; ++j)
        {
            free((void*)cache->FileData[i].FileList[j].FilePath);
        }
        free(cache->FileData[i].FileList);
        cache->FileData[i].FileList      = NULL;
        cache->FileData[i].FileCount     = 0;
        cache->FileData[i].FileCapacity  = 0;

        // clean up image metadata (level info, block offsets):
        free(cache->MetaData[i].BlockOffsets);
        free(cache->MetaData[i].LevelInfo);
        cache->MetaData[i].ElementCount = 0;
        cache->MetaData[i].LevelCount   = 0;
        cache->MetaData[i].LevelInfo    = NULL;
        cache->MetaData[i].BlockOffsets = NULL;
    }
    free(cache->MetaData);
    free(cache->FileData);
    cache->ImageCount    = 0;
    cache->ImageCapacity = 0;
    id_table_delete(&cache->ImageIds);
}

/// @summary Reconfigure the image cache, modifying the victim selection algorithm and memory budget.
/// @param cache The image cache to reconfigure.
/// @param config The new cache configuration values.
public_function void image_cache_configure(image_cache_t *cache, image_cache_config_t const &config)
{
    AcquireSRWLockExclusive(&cache->AttribLock);
    cache->BehaviorId = config.Behavior;
    cache->LimitBytes = config.CacheSize;
    ReleaseSRWLockExclusive(&cache->AttribLock);
}

/// @summary Query the image cache for usage statistics as of the most recent update.
/// @param cache The image cache to query.
/// @param stat The cache statsitics to populate.
public_function void image_cache_stats(image_cache_t *cache, image_cache_stat_t &stat)
{
    AcquireSRWLockShared(&cache->AttribLock);
    stat.BytesLimit = cache->LimitBytes;
    stat.BytesUsed  = cache->TotalBytes;
    ReleaseSRWLockShared(&cache->AttribLock);
}

/// @summary Mark all frames of an image to be evicted from cache memory.
/// @param cache The image cache managing the image.
/// @param id The application-defined image identifier.
/// @param thread_alloc The allocator used to submit cache control commands from the calling thread.
public_function void image_cache_evict_image(image_cache_t *cache, uintptr_t id, image_command_alloc_t *thread_alloc)
{
    fifo_node_t<image_cache_command_t> *n = fifo_allocator_get(thread_alloc);
    n->Item.CommandId   = IMAGE_CACHE_COMMAND_EVICT;
    n->Item.ImageId     = id;
    n->Item.Options     = IMAGE_CACHE_COMMAND_OPTION_NONE;
    n->Item.FirstFrame  = 0;
    n->Item.FinalFrame  = IMAGE_ALL_FRAMES;
    n->Item.ErrorQueue  = NULL;
    n->Item.ResultQueue = NULL;
    n->Item.Priority    = 0;
    mpsc_fifo_u_produce(&cache->CommandQueue, n);
}

/// @summary Mark all frames of an image to be evicted from cache memory, and then drop the image to prevent it from being reloaded.
/// @param cache The image cache managing the image.
/// @param id The application-defined image identifier.
/// @param thread_alloc The allocator used to submit cache control commands from the calling thread.
public_function void image_cache_drop_image(image_cache_t *cache, uintptr_t id, image_command_alloc_t *thread_alloc)
{
    fifo_node_t<image_cache_command_t> *n = fifo_allocator_get(thread_alloc);
    n->Item.CommandId   = IMAGE_CACHE_COMMAND_DROP;
    n->Item.ImageId     = id;
    n->Item.Options     = IMAGE_CACHE_COMMAND_OPTION_NONE;
    n->Item.FirstFrame  = 0;
    n->Item.FinalFrame  = IMAGE_ALL_FRAMES;
    n->Item.ErrorQueue  = NULL;
    n->Item.ResultQueue = NULL;
    n->Item.Priority    = 0;
    mpsc_fifo_u_produce(&cache->CommandQueue, n);
}

/// @summary Define the source file for one or more frames of an image. The image is created, if necessary.
/// @param cache The image cache managing the image.
/// @param id The application-defined identifier of the image.
/// @param file_path The NULL-terminated UTF-8 string specifying the virtual file path. The string will be copied.
/// @param first_frame The zero-based index of the first frame defined in the file.
/// @param final_frame The zero-based index of the last frame defined in the file, or IMAGE_ALL_FRAMES.
/// @param file_hints A combination of vfs_file_hint_e specifying hints used to open the file, or VFS_FILE_HINT_NONE to let the implementation decide.
/// @param decoder_hint One of vfs_decoder_hint_e specifying the type of decoder to create, or VFS_DECODER_HINT_NONE to let the implementation decide.
/// @param thread_alloc The allocator used to submit image declaration commands from the calling thread.
public_function void image_cache_add_frames(image_cache_t *cache, uintptr_t id, char const *file_path, size_t first_frame, size_t final_frame, uint32_t file_hints, int decoder_hint, image_declaration_alloc_t *thread_alloc)
{
    fifo_node_t<image_declaration_t> *n = fifo_allocator_get(thread_alloc);
    n->Item.ImageId     = id;
    n->Item.FilePath    =_strdup(file_path);
    n->Item.FirstFrame  = first_frame;
    n->Item.FinalFrame  = final_frame;
    n->Item.FileHints   = file_hints;
    n->Item.DecoderHint = decoder_hint;
    mpsc_fifo_u_produce(&cache->DeclarationQueue, n);
}

/// @summary Define the source file for all frames of an image. The image is created, if necessary.
/// @param cache The image cache managing the image.
/// @param id The application-defined identifier of the image.
/// @param file_path The NULL-terminated UTF-8 string specifying the virtual file path. The string will be copied.
/// @param thread_alloc The allocator used to submit image declaration commands from the calling thread.
public_function void image_cache_add_image(image_cache_t *cache, uintptr_t id, char const *file_path, image_declaration_alloc_t *thread_alloc)
{
    image_cache_add_frames(cache, id, file_path, 0, IMAGE_ALL_FRAMES, VFS_FILE_HINT_NONE, VFS_DECODER_HINT_USE_DEFAULT, thread_alloc);
}

/// @summary Retrieve image metadata.
/// @param cache The image cache to query.
/// @param id The application-defined identifier of the image.
/// @param attribs On return, if the image is known, its attributes are copied to this location.
/// @return true if the image attributes have been set.
public_function bool image_cache_image_attributes(image_cache_t *cache, uintptr_t id, image_basic_data_t &attribs)
{
    AcquireSRWLockShared(&cache->MetadataLock);
    size_t index;
    if (id_table_get(&cache->ImageIds, id, &index))
    {
        attribs = cache->MetaData[index];
        ReleaseSRWLockShared(&cache->MetadataLock);
        return(attribs.ImageFormat != DXGI_FORMAT_UNKNOWN);
    }
    else
    {
        ReleaseSRWLockShared(&cache->MetadataLock);
        return false;
    }
}

/// @summary Request that one or more frames of an image be locked in cache memory for access.
/// @param cache The image cache managing the image.
/// @param id The application-defined identifier of the image to lock.
/// @param first_frame The zero-based index of the first frame to lock.
/// @param final_frame The zero-based index of the last frame to lock, or IMAGE_ALL_FRAMES.
/// @param result_queue The queue where results will be placed for each frame when available.
/// @param error_queue The queue where errors will be placed for each frame.
/// @param priority The priority value to use if a frame needs to be re-loaded into cache memory.
/// @param thread_alloc The allocator used to submit cache control commands from the calling thread.
public_function void image_cache_lock_frames(image_cache_t *cache, uintptr_t id, size_t first_frame, size_t final_frame, image_cache_result_queue_t *result_queue, image_cache_error_queue_t *error_queue, uint8_t priority, image_command_alloc_t *thread_alloc)
{
    fifo_node_t<image_cache_command_t> *n = fifo_allocator_get(thread_alloc);
    n->Item.CommandId   = IMAGE_CACHE_COMMAND_LOCK;
    n->Item.ImageId     = id;
    n->Item.Options     = IMAGE_CACHE_COMMAND_OPTION_NONE;
    n->Item.FirstFrame  = first_frame;
    n->Item.FinalFrame  = final_frame;
    n->Item.ErrorQueue  = error_queue;
    n->Item.ResultQueue = result_queue;
    n->Item.Priority    = priority;
    mpsc_fifo_u_produce(&cache->CommandQueue, n);
}

/// @summary Request that one or more frames of an image be unlocked in cache memory, allowing them to be evicted.
/// @param cache The image cache managing the image.
/// @param id The application-defined identifier of the image to unlock.
/// @param first_frame The zero-based index of the first frame to unlock.
/// @param final_frame The zero-based index of the last frame to lunock, or IMAGE_ALL_FRAMES.
/// @param thread_alloc The allocator used to submit cache control commands from the calling thread.
public_function void image_cache_unlock_frames(image_cache_t *cache, uintptr_t id, size_t first_frame, size_t final_frame, uint32_t options, image_command_alloc_t *thread_alloc)
{
    fifo_node_t<image_cache_command_t> *n = fifo_allocator_get(thread_alloc);
    n->Item.CommandId   = IMAGE_CACHE_COMMAND_UNLOCK;
    n->Item.ImageId     = id;
    n->Item.Options     = options;
    n->Item.FirstFrame  = first_frame;
    n->Item.FinalFrame  = final_frame;
    n->Item.ErrorQueue  = NULL;
    n->Item.ResultQueue = NULL;
    n->Item.Priority    = 0;
    mpsc_fifo_u_produce(&cache->CommandQueue, n);
}

/// @summary Executes a single update tick for an image cache.
/// @param cache The image cache to update.
public_function void image_cache_update(image_cache_t *cache)
{   // get the timestamp of the start of this tick for use in tagging cache entries.
    uint64_t now_time = image_cache_nanotime(cache);

    // process any pending image declarations. do this first so that later operations
    // that may also be queued can reference the most up-to-date information.
    image_declaration_t imgdecl;
    while (mpsc_fifo_u_consume(&cache->DeclarationQueue, imgdecl))
    {
        AcquireSRWLockExclusive(&cache->MetadataLock);
        image_cache_define_image(cache, imgdecl);
        ReleaseSRWLockExclusive(&cache->MetadataLock);
    }

    // process any pending image definitions. these are stored in the list 
    // managed by image declarations, so only perform this operation using
    // the most up-to-date definitions. do this prior to processing commands.
    image_definition_t imgdef;
    while (mpsc_fifo_u_consume(&cache->DefinitionQueue, imgdef))
    {   // check the metadata ID table for an existing entry.
        AcquireSRWLockExclusive(&cache->MetadataLock);
        image_cache_update_image_definition(cache, imgdef);
        ReleaseSRWLockExclusive(&cache->MetadataLock);
        image_definition_free(&imgdef);
    }

    // process all completed loads of image frames to cache memory.
    image_location_t imgpos;
    while (mpsc_fifo_u_consume(&cache->LocationQueue, imgpos))
    {
        image_cache_update_location(cache, imgpos, now_time);
    }

    // process all newly received commands. unlock commands and lock 
    // commands for in-cache frames will complete immediately. lock 
    // commands that are not in-cache generate load commands and 
    // possibly eviction commands. this is the most complicated step.
    image_cache_command_t imgcmd;
    while (mpsc_fifo_u_consume(&cache->CommandQueue, imgcmd))
    {
        switch (imgcmd.CommandId)
        {
        case IMAGE_CACHE_COMMAND_UNLOCK:
            image_cache_process_unlock(cache, imgcmd);
            break;
        case IMAGE_CACHE_COMMAND_LOCK:
            image_cache_process_lock  (cache, imgcmd, now_time);
            break;
        case IMAGE_CACHE_COMMAND_EVICT:
            image_cache_process_evict (cache, imgcmd);
            break;
        case IMAGE_CACHE_COMMAND_DROP:
            image_cache_process_drop  (cache, imgcmd);
            break;
        }
    }
}

/// @summary Default constructor. Call thread_image_cache_t::initialize() prior to use.
thread_image_cache_t::thread_image_cache_t(void)
    :
    Cache(NULL)
{
    fifo_allocator_init(&CommandAlloc);
    fifo_allocator_init(&DeclarationAlloc);
}

/// @summary Frees resources and invalidates all outstanding queue entries.
thread_image_cache_t::~thread_image_cache_t(void)
{
    fifo_allocator_reinit(&DeclarationAlloc);
    fifo_allocator_reinit(&CommandAlloc);
    Cache = NULL;
}

/// @summary Set the image cache used as the target of all operations.
/// @param cache The target image cache.
void thread_image_cache_t::initialize(image_cache_t *cache)
{
    Cache = cache;
}

/// @summary Synchronously reconfigure the target image cache.
/// @param config The image cache configuration to apply.
void thread_image_cache_t::configure(image_cache_config_t const &config)
{
    image_cache_configure(Cache, config);
}

/// @summary Define an image and declare the source file for one or more frames of data.
/// @param id The application-defined identifier of the image.
/// @param path The NULL-terminated UTF-8 string specifying the virtual file path. The string will be copied.
/// @param first_frame The zero-based index of the first frame defined in the file.
/// @param final_frame The zero-based index of the last frame defined in the file, or IMAGE_ALL_FRAMES.
/// @param file_hints A combination of vfs_file_hint_e specifying hints used to open the file, or VFS_FILE_HINT_NONE to let the implementation decide.
/// @param decoder_hint One of vfs_decoder_hint_e specifying the type of decoder to create, or VFS_DECODER_HINT_NONE to let the implementation decide.
void thread_image_cache_t::add_source(uintptr_t id, char const *path, size_t first_frame, size_t final_frame, uint32_t file_hints, int decoder_hint)
{
    image_cache_add_frames(Cache, id, path, first_frame, final_frame, file_hints, decoder_hint, &DeclarationAlloc);
}

/// @summary Retrieve cache statistics as of the most recent update.
/// @param stats The cache statistics information to populate.
void thread_image_cache_t::stat(image_cache_stat_t &stats)
{
    image_cache_stats(Cache, stats);
}

/// @summary Synchronously retrieve image attributes.
/// @param id The application-defined identifier of the image.
/// @param attribs On return, if the image is known, its attributes are copied to this location.
/// @return true if the image attributes have been set.
bool thread_image_cache_t::image_attributes(uintptr_t id, image_basic_data_t &attribs)
{
    return image_cache_image_attributes(Cache, id, attribs);
}

/// @summary Lock one or more frames of an image into cache memory for access.
/// @param id The application-defined identifier of the image to lock.
/// @param first_frame The zero-based index of the first frame to lock.
/// @param final_frame The zero-based index of the last frame to lock, or IMAGE_ALL_FRAMES.
/// @param result_queue The queue where results will be placed for each frame when available.
/// @param error_queue The queue where errors will be placed for each frame.
/// @param priority The priority value to use if a frame needs to be re-loaded into cache memory.
void thread_image_cache_t::lock(uintptr_t id, size_t first_frame, size_t final_frame, image_cache_result_queue_t *result_queue, image_cache_error_queue_t *error_queue, uint8_t priority)
{
    image_cache_lock_frames(Cache, id, first_frame, final_frame, result_queue, error_queue, priority, &CommandAlloc);
}

/// @summary Request that one or more frames of an image be unlocked in cache memory, allowing them to be evicted.
/// @param id The application-defined identifier of the image to unlock.
/// @param first_frame The zero-based index of the first frame to unlock.
/// @param final_frame The zero-based index of the last frame to lunock, or IMAGE_ALL_FRAMES.
void thread_image_cache_t::unlock(uintptr_t id, size_t first_frame, size_t final_frame, uint32_t options)
{
    image_cache_unlock_frames(Cache, id, first_frame, final_frame, options, &CommandAlloc);
}

/// @summary Mark all frames of an image to be evicted from cache memory.
/// @param id The application-defined image identifier.
void thread_image_cache_t::evict(uintptr_t id)
{
    image_cache_evict_image(Cache, id, &CommandAlloc);
}

/// @summary Mark all frames of an image to be evicted from cache memory, and then drop the image to prevent it from being reloaded.
/// @param id The application-defined image identifier.
void thread_image_cache_t::drop(uintptr_t id)
{
    image_cache_drop_image(Cache, id, &CommandAlloc);
}
