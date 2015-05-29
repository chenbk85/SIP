/*/////////////////////////////////////////////////////////////////////////////
/// @summary 
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
/// @summary A special value that may be assigned to image_declaration_t::FinalFrame
/// to indicate that the specified image file defines all frames of the image.
static size_t const IMAGE_ALL_FRAMES  = ~0;

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
    IMAGE_CACHE_BEHAVIOR_IMAGE_LRU_FRAME_MRU = 0, /// The most recently used frame of the least recently used image is selected.
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
    int                  FileHints;               /// A combination of vfs_file_hint_e to pass to the VFS driver when opening the file.
    int                  DecoderHint;             /// One of vfs_decoder_hint_e, or VFS_DECODER_HINT_USE_DEFAULT.
};
typedef fifo_allocator_t<image_declaration_t>         image_declaration_alloc_t;
typedef mpsc_fifo_u_t   <image_declaration_t>         image_declaration_queue_t;

/// @summary Defines the metadata describing the runtime attributes of a logical image.
/// This information is usually obtained during parsing from the image header fields.
struct image_definition_t
{
    uintptr_t            ImageId;                 /// The application-defined logical image identifier.
    uint32_t             ImageFormat;             /// One of dxgi_format_e specifying the storage format of the pixel data.
    uint32_t             Compression;             /// One of image_compression_e specifying the compression format of the pixel data.
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
    int64_t             *FileOffsets;             /// An array of LevelCount * ElementCount entries specifying the byte offsets of each level in the source file.
};
typedef fifo_allocator_t<image_definition_t>          image_definition_alloc_t;
typedef mpsc_fifo_u_t   <image_definition_t>          image_definition_queue_t;

/// @summary Defines the data used to specify where an individual frame of an image has 
/// been loaded into memory. This data can be used to indicate that a frame has just been
/// loaded into cache, or to specify the location from which the frame should be evicted.
struct image_location_t
{
    uintptr_t            ImageId;                 /// The application-defined logical image identifier.
    size_t               FrameIndex;              /// The zero-based index of the frame.
    void                *BaseAddress;             /// The address of the start of the frame data.
    size_t               BytesReserved;           /// The number of bytes reserved for the frame data.
    uintptr_t            Context;                 /// Opaque information associated with the frame.
};
typedef fifo_allocator_t<image_location_t>            image_location_alloc_t;
typedef fifo_allocator_t<image_location_t>            image_eviction_alloc_t;
typedef mpsc_fifo_u_t   <image_location_t>            image_location_queue_t;
typedef spsc_fifo_u_t   <image_location_t>            image_eviction_queue_t;

/// @summary Defines the data output by the cache when it needs to load a frames from a file.
/// For images where the frame data is spread across multiple files (like a PNG sequence), 
/// one load request is generated for each file.
struct image_load_t
{
    char const          *FilePath;                /// The NULL-terminated UTF-8 virtual file path.
    size_t               FirstFrame;              /// The zero-based index of the first frame to load.
    size_t               FinalFrame;              /// The zero-based index of the last frame to load, or IMAGE_ALL_FRAMES.
    int                  FileHints;               /// A combination of vfs_file_hint_t to use when opening the file.
    int                  DecoderHint;             /// One of vfs_decoder_hint_e to use when opening the file.
    uint8_t              Priority;                /// The file load priority.
};
typedef fifo_allocator_t<image_load_t>                image_load_alloc_t;
typedef spsc_fifo_u_t   <image_load_t>                image_load_queue_t;

/// @summary Defines the data returned when a cache control command has completed. Generally, 
/// only lock commands will return anything useful; for other command types, the ResultQueue
/// is set to NULL and no result is generated. In the case where a range of frames are locked, 
/// one result will be generated for each frame individually.
struct image_cache_result_t
{
    uint32_t             CommandId;               /// One of image_cache_command_e specifying the command type.
    uint32_t             ErrorCode;               /// ERROR_SUCCESS, or a system error code.
    uintptr_t            ImageId;                 /// The application-defined identifier of the logical image.
    size_t               FrameIndex;              /// The zero-based frame index.
    void                *BaseAddress;             /// The base address of the frame data, if it was locked.
    size_t               BytesReserved;           /// The number of bytes of frame data, if the frame was locked.
    // ... other stuff
};
typedef fifo_allocator_table_t<image_cache_result_t>  image_cache_result_alloc_table_t;
typedef fifo_allocator_t      <image_cache_result_t>  image_cache_result_alloc_t;
typedef mpsc_fifo_u_t         <image_cache_result_t>  image_cache_result_queue_t; // or SPSC?

/// @summary Defines the data specified with a cache update command, which will generate 
/// some sequence of operations (images may be evicted, loads may be generated, and so on.)
struct image_cache_command_t
{   typedef image_cache_result_queue_t                result_queue_t;
    uint32_t             CommandId;               /// One of image_cache_command_e specifying the operation to perform.
    uint32_t             Options;                 /// A combination of image_cache_command_option_e.
    uintptr_t            ImageId;                 /// The application-defined identifier of the logical image.
    size_t               FirstFrame;              /// The zero-based index of the first frame to operate on.
    size_t               FinalFrame;              /// The zero-based index of the last frame to operate on, or IMAGE_ALL_FRAMES.
    uint8_t              Priority;                /// The priority value to use when loading the file (if necessary.)
    result_queue_t      *ResultQueue;             /// The queue in which results should be placed, or NULL.
};
typedef fifo_allocator_t<image_cache_command_t> image_command_alloc_t;
typedef mpsc_fifo_u_t   <image_cache_command_t> image_command_queue_t;

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

/// @summary Defines the metadata maintained for each logical image.
/// This is essentially the same data supplied with the image definition.
struct image_basic_data_t
{
    uintptr_t            ImageId;                 /// The application-defined logical image identifier.
    uint32_t             ImageFormat;             /// One of dxgi_format_e specifying the storage format of the pixel data.
    uint32_t             Compression;             /// One of image_compression_e specifying the compression format of the pixel data.
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
    int64_t             *FileOffsets;             /// An array of LevelCount * ElementCount entries specifying the byte offsets of each level in the source file.
};

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

/// @summary Defines the data and queues associated with an image cache. The image 
/// cache is not responsible for allocating or committing image memory.
struct image_cache_t
{   typedef image_cache_result_alloc_table_t          result_alloc_table_t;
    typedef image_declaration_alloc_t                 declaration_alloc_t;
    typedef image_declaration_queue_t                 declaration_queue_t;
    typedef image_definition_alloc_t                  definition_alloc_t;
    typedef image_definition_queue_t                  definition_queue_t;
    typedef image_location_alloc_t                    location_alloc_t;
    typedef image_location_queue_t                    location_queue_t;
    typedef image_load_alloc_t                        load_alloc_t;
    typedef image_load_queue_t                        load_queue_t;
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

    load_queue_t           LoadQueue;             /// The SPSC output queue for image load requests.
    load_alloc_t           LoadAlloc;             /// The internal FIFO node allocator for the image load request queue.

    eviction_queue_t       EvictQueue;            /// The SPSC output queue for image eviction requests.
    eviction_alloc_t       EvictAlloc;            /// The internal FIFO node allocator for the image eviction queue.

    declaration_queue_t    DeclarationQueue;      /// The MPSC input queue for image declaration requests.
    definition_queue_t     DefinitionQueue;       /// The MPSC input queue for image definition requests.
    location_queue_t       LocationQueue;         /// The MPSC input queue for cache memory location updates.
    command_queue_t        CommandQueue;          /// The MPSC input queue for cache control commands.

    size_t                 PendingCount;          /// The number of in-progress frame lock commands.
    size_t                 PendingCapacity;       /// The total number of allocated storage slots for pending frame lock commands.
    image_cache_command_t *PendingLocks;          /// The list of pending frame lock commands.
    result_alloc_table_t   ResultAlloc;           /// The table of FIFO node allocators for posting lock results to client queues.
};

/*///////////////
//   Globals   //
///////////////*/

/*///////////////////////
//   Local Functions   //
///////////////////////*/
/// @summary Reads the current tick count for use as a timestamp.
/// @return The current timestamp value, in nanoseconds.
internal_function inline uint64_t image_cache_nanotime(image_cache_t *cache)
{
    uint64_t const  SEC_TO_NANOSEC = 1000000000ULL;
    LARGE_INTEGER   counts; QueryPerformanceCounter(&counts);
    return uint64_t(SEC_TO_NANOSEC * (double(counts.QuadPart) / double(cache->ClockFrequency.QuadPart)));
}

/// @summary Immediately drop an image record. The image can no longer be reloaded into cache.
/// @param cache The image cache to query and update.
/// @param image_id The application-defined identifier of the logical image to delete.
/// @return true if the specified image was found and deleted.
internal_function bool image_cache_drop_image_record(image_cache_t *cache, uintptr_t image_id)
{
    image_files_data_t files;
    image_basic_data_t attribs;
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
        free(attribs.FileOffsets); attribs.FileOffsets = NULL;
        free(attribs.LevelInfo);   attribs.LevelInfo   = NULL;
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
internal_function bool image_cache_process_pending_evict_and_drop(image_cache_t *cache, size_t entry_index)
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
            size_t           e    = entry.FrameCount - 1;
            entry.FrameList [i]   = entry.FrameList [e];
            entry.FrameData [i]   = entry.FrameData [e];
            entry.FrameState[i]   = entry.FrameState[e];
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

/*////////////////////////
//   Public Functions   //
////////////////////////*/

public_function void image_cache_update(image_cache_t *cache)
{   // get the timestamp of the start of this tick for use in tagging cache entries.
    uint64_t now_time = image_cache_nanotime(cache);

    // process any pending image declarations. do this first so that later operations
    // that may also be queued can reference the most up-to-date information.
    image_declaration_t imgdecl;
    while (mpsc_fifo_u_consume(&cache->DeclarationQueue, imgdecl))
    {   // check the metadata ID table to see if there's an existing entry.
        AcquireSRWLockExclusive(&cache->MetadataLock);
        size_t index;
        if (id_table_get(&cache->ImageIds, imgdecl.ImageId, &index))
        {   // there's an existing entry, so update it.
            // ...
        }
        else
        {   // this is a new entry, so create it.
            // ...
        }
        ReleaseSRWLockExclusive(&cache->MetadataLock);
    }

    // process any pending image definitions. these are stored in the list 
    // managed by image declarations, so only perform this operation using
    // the most up-to-date definitions. do this prior to processing commands.
    image_definition_t imgdef;
    while (mpsc_fifo_u_consume(&cache->DefinitionQueue, imgdef))
    {   // check the metadata ID table for an existing entry.
        AcquireSRWLockExclusive(&cache->MetadataLock);
        size_t index;
        if (id_table_get(&cache->ImageIds, imgdef.ImageId, &index))
        {   // the internal image metadata record takes ownership of 
            // the data provided as part of the image definition. 
            // allocated memory will be freed when the image is dropped.
            image_basic_data_t  &meta = cache->MetaData[index];
            // ...
        }
        // else, there's no corresponding definition. it's 
        // possible that the image was dropped, so ignore.
        ReleaseSRWLockExclusive(&cache->MetadataLock);
    }

    // process all completed loads of image frames to cache memory.
    image_location_t imgpos;
    while (mpsc_fifo_u_consume(&cache->LocationQueue, imgpos))
    {
        size_t index;
        if (id_table_get(&cache->EntryIds, imgpos.ImageId, &index))
        {   // update the existing cache entry. the frame may have 
            // been relocated, or it may have been loaded for the first time.
            // ...
        }
        else
        {   // there's no corresponding cache entry, so create one.
            // ...
        }
        // retire pending lock commands by posting to their result queue.
        // ...
    }

    // process all newly received commands. unlock commands and lock 
    // commands for in-cache frames will complete immediately. lock 
    // commands that are not in-cache generate load commands and 
    // possibly eviction commands. this is the most complicated step.
    image_cache_command_t imgcmd;
    while (mpsc_fifo_u_consume(&cache->CommandQueue, imgcmd))
    {
    }
}
