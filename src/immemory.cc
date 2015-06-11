/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines the interface to an image memory manager that can allocate
/// image buffers in system memory using the virtual memory management services
/// of the underlying operating system. For finer granularity, if mipmaps will 
/// frequently be used, allow each individual level to be committed/decommitted
/// individually. This means that each level would have to start on a page-
/// aligned address. Currently, memory is committed or decommitted for all 
/// levels in an image element (array item, frame, or cubemap face.)
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
/// @summary The mask used to access the per-element lock count.
#define IMAGE_ELEMENT_LOCK_MASK     0x0000FFFFU

/// @summary The mask used to access the per-element status bits.
#define IMAGE_ELEMENT_STATUS_MASK   0xFFFF0000U

/// @summary The number of bits to shift the packed element status flags to the left.
#define IMAGE_ELEMENT_STATUS_SHIFT  16U

/// @summary Define the default size of a hash bucket in the image memory ID table.
#ifndef IMAGE_MEMORY_BUCKET_SIZE
#define IMAGE_MEMORY_BUCKET_SIZE    128U
#endif

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Define flags that can be set on either an individual mip-level or an entire image.
enum image_memory_flags_e : uint32_t
{
    IMAGE_MEMORY_FLAG_NONE        = (0 << 0), /// The level memory is reserved, but not committed.
    IMAGE_MEMORY_FLAG_COMMITTED   = (1 << 0), /// The level memory is committed.
    IMAGE_MEMORY_FLAG_EVICT       = (1 << 1), /// The level memory should be decommitted when the lock count drops to zero.
    IMAGE_MEMORY_FLAG_DROP        = (1 << 2)  /// The image memory should be decommitted and released.
};

/// @summary Define the data describing a single level of the mip-chain. Each element has at least
/// one mip-level (0 = highest resolution). Each mip-level in image memory begins on an address 
/// that is an even multiple of the system page size.
struct image_memory_level_t
{
    size_t                LevelOffset;        /// The byte offset of the level from the start of the element.
    size_t                LevelWidth;         /// The width of the level, in pixels.
    size_t                LevelHeight;        /// The height of the level, in pixels.
    size_t                LevelSlices;        /// The number of slices in the level, in pixels.
    size_t                BytesPerElement;    /// The number of bytes per-pixel or per-block.
    size_t                BytesPerRow;        /// The number of bytes between scanlines.
    size_t                BytesPerSlice;      /// The number of bytes between slices.
};

/// @summary Define the data describing a single logical image. Each image reserves enough 
/// contiguous address space for all levels of all elements, even if they aren't used.
struct image_memory_info_t
{
    uintptr_t             ImageId;            /// The application-defined identifier of the image.
    uint32_t              Format;             /// One of dxgi_format_e specifying the storage format.
    int                   Compression;        /// One of image_compression_e specifying the storage compression.
    int                   Encoding;           /// One of image_encoding_e specifying the storage encoding type.
    int                   AccessType;         /// One of image_type_e specifying the access type.
    size_t                ElementCount;       /// The number of array elements, frames, or cubemap faces.
    size_t                LevelCount;         /// The number of mip-levels per element.
    size_t                BytesPerPixel;      /// The number of bytes allocated per-pixel.
    size_t                BytesPerBlock;      /// The number of bytes allocated per-block, or 0 if not block compressed.
    size_t                BytesPerElement;    /// The number of bytes reserved per-element. Always a multiple of allocation granularity.
    size_t                BytesPerElementUsed;/// The number of bytes actually used per-element.
    uint32_t             *ElementStatus;      /// ElementCount items, lock count and image_memory_flags_e.
    image_memory_level_t *Levels;             /// LevelCount descriptions of each mip-level (0 = highest resolution).
};

/// @summary Define the memory allocation data for a single logical image.
struct image_memory_addr_t
{
    void                 *BaseAddress;        /// The base address of the reserved page range.
    size_t                BytesReserved;      /// The number of bytes reserved, rounded to the allocation granularity.
    size_t                BytesCommitted;     /// The number of bytes actually committed, a multiple of the page size.
    uint32_t              ImageStatus;        /// Either IMAGE_MEMORY_FLAG_NONE or IMAGE_MEMORY_FLAG_DROP.
};

/// @summary Defines all of the state associated with a virtual-memory based image memory manager.
struct image_memory_t
{
    size_t                BytesReserved;      /// The total number of bytes of address space reserved for all images.
    size_t                BytesCommitted;     /// The number of bytes actually committed for all images.

    size_t                PageSize;           /// The operating system page size, in bytes.
    size_t                Granularity;        /// The operating system virtual memory allocation granularity, in bytes.

    size_t                ImageCount;         /// The number of images known to the image memory.
    size_t                ImageCapacity;      /// The number of image records that can be stored without reallocating lists.
    id_table_t            ImageIds;           /// The table mapping application defined image ID to list index.
    image_memory_addr_t  *AddressList;        /// The list of memory allocation information for each known image.
    image_memory_info_t  *AttributeList;      /// The list of image attributes for each known image.
};

/// @summary Defines the set of storage attributes that can be returned for an image.
/// This data is only available after the image is registered with the memory manager.
struct image_storage_info_t
{
    uint32_t              ImageFormat;        /// One of dxgi_format_e specifying the data storage format.
    int                   Compression;        /// One of image_compression_e specifying the stored compression format.
    int                   Encoding;           /// One of image_encoding_e specifying how the stored image data is encoded.
    int                   AccessType;         /// One of image_access_type_e specifying how the image data is accessed.
    size_t                ElementCount;       /// The number of image elements (array items or frames.)
    size_t                LevelCount;         /// The number of mip-levels defined for each image element.
    size_t                BytesReserved;      /// The number of bytes of address space reserved for the image.
};

/*///////////////
//   Globals   //
///////////////*/

/*///////////////////////
//   Local Functions   //
///////////////////////*/
/// @summary Extract the lock count from a packed element status value.
/// @param element_status A packed element status value.
/// @return The lock count.
internal_function inline size_t image_memory_element_lock_count(uint32_t element_status)
{
    return size_t(element_status & IMAGE_ELEMENT_LOCK_MASK);
}

/// @summary Extract the status flags from a packed element status value.
/// @param element_status A packed element status value.
/// @return The element status flags, a combination of image_memory_flags_e.
internal_function inline uint32_t image_memory_element_status_flags(uint32_t element_status)
{
    return (element_status & IMAGE_ELEMENT_STATUS_MASK) >> IMAGE_ELEMENT_STATUS_SHIFT;
}

/// @summary Construct a packed element status value from the flags and lock count.
/// @param status_bits A combination of image_memory_flags_e.
/// @param lock_count The number of outstanding locks on the element.
/// @return The packed element status value.
internal_function inline uint32_t image_memory_make_element_status(uint32_t status_bits,  size_t lock_count)
{
    return (((status_bits << IMAGE_ELEMENT_STATUS_SHIFT) & IMAGE_ELEMENT_STATUS_MASK) | uint32_t(lock_count & IMAGE_ELEMENT_LOCK_MASK));
}

/// @summary Calculate the size of a single image element (array item, frame, or cube face).
/// @param def The image definition.
/// @param page_size The size of a system virtual memory page, in bytes.
/// @param used On return, stores the number of bytes per-element actually used.
/// @return The size of a single image element, in bytes, rounded up to the nearest page size multiple.
internal_function size_t image_memory_element_size(image_definition_t const &def, size_t page_size, size_t &used)
{   // sum the sizes of all mip-levels defined for each element.
    size_t s_base = 0;
    for (size_t i = 0, n = def.LevelCount; i < n; ++i)
    {
        s_base   += def.LevelInfo[i].Slices  * 
                    def.LevelInfo[i].BytesPerSlice;
    }
    // elements must start at even multiples of the page size, 
    // and since they're tightly packed, that means they must 
    // be rounded up to an even multiple of the page size.
    used = s_base;
    return align_up(s_base, page_size);
}

/// @summary Evicts an element, decommitting its memory, if the element is marked for eviction and there are no active locks.
/// @param mem The image memory manager that owns the image data.
/// @param image_index The zero-based index of the image in the image list.
/// @param element The zero-based index of the element to check.
internal_function void image_memory_process_pending_evict(image_memory_t *mem, size_t image_index, size_t element)
{
    image_memory_addr_t &addr = mem->AddressList  [image_index];
    image_memory_info_t &info = mem->AttributeList[image_index];
    uint32_t            flags = image_memory_element_status_flags(info.ElementStatus[element]);
    size_t              locks = image_memory_element_lock_count  (info.ElementStatus[element]);
    if ((flags  & IMAGE_MEMORY_FLAG_EVICT    ) != 0 && 
        (flags  & IMAGE_MEMORY_FLAG_COMMITTED) != 0 && 
        (locks == 0))
    {
        flags &=~(IMAGE_MEMORY_FLAG_EVICT | IMAGE_MEMORY_FLAG_COMMITTED);
        uint8_t    *element_data    =((uint8_t*)addr.BaseAddress) + (info.BytesPerElement * element);
        VirtualFree(element_data, info.BytesPerElement, MEM_DECOMMIT);
        addr.BytesCommitted        -= info.BytesPerElement;
        mem->BytesCommitted        -= info.BytesPerElement;
        info.ElementStatus[element] = image_memory_make_element_status(flags, locks);
    }
}

/// @summary Drops an image if the image is marked to be dropped and there are no active locks.
/// @param mem The image memory manager that owns the image data.
/// @param image_index The zero-based index of the image within the image list.
internal_function void image_memory_process_pending_drop(image_memory_t *mem, size_t image_index)
{
    image_memory_addr_t &addr = mem->AddressList  [image_index];
    image_memory_info_t &info = mem->AttributeList[image_index];
    // process any pending drop if all elements are unlocked.
    if ((addr.BytesCommitted == 0) && (addr.ImageStatus & IMAGE_MEMORY_FLAG_DROP) != 0)
    {   // save the identifiers of the image records we're working with.
        size_t    last_index  = mem->ImageCount - 1;
        uintptr_t this_id     = info.ImageId;
        uintptr_t last_id     = mem->AttributeList[last_index].ImageId;
        // free internal descriptor memory for the image.
        free(info.Levels);        info.Levels        = NULL; info.LevelCount   = 0;    
        free(info.ElementStatus); info.ElementStatus = NULL; info.ElementCount = 0;
        // decommit and release the entire reserved range for the image.
        VirtualFree(addr.BaseAddress, 0, MEM_RELEASE);
        mem->BytesReserved-= addr.BytesReserved;
        addr.BytesCommitted= 0;
        addr.BytesReserved = 0;
        addr.BaseAddress   = NULL;
        // remove the item in the list by swapping with the last element.
        if (this_id != last_id)
        {   // perform the swap with the last element.
            mem->AddressList  [image_index] = mem->AddressList  [last_index];
            mem->AttributeList[image_index] = mem->AttributeList[last_index];
            // update the image ID => index table for the moved element.
            id_table_update(&mem->ImageIds, last_id, image_index, NULL);
        }
        // delete the removed item from the image ID => index table.
        id_table_remove(&mem->ImageIds, this_id, NULL);
        mem->ImageCount--;
    }
}

/*////////////////////////
//   Public Functions   //
////////////////////////*/
/// @summary Initializes a new image memory manager.
/// @param mem The image memory manager.
/// @param expected_image_count The maximum number of images expected to be loaded at any one time.
public_function void image_memory_create(image_memory_t *mem, size_t expected_image_count)
{   // retrieve the system page size and allocation granularity.
    SYSTEM_INFO sysinfo = {};
    GetNativeSystemInfo(&sysinfo);

    if (expected_image_count < IMAGE_MEMORY_BUCKET_SIZE)
    {   // enforce a minimum capacity; we need at least one bucket.
        expected_image_count = IMAGE_MEMORY_BUCKET_SIZE;
    }
    size_t bucket_count = expected_image_count / IMAGE_MEMORY_BUCKET_SIZE;
    mem->BytesReserved  = 0;
    mem->BytesCommitted = 0;
    mem->PageSize       =(size_t) sysinfo.dwPageSize;
    mem->Granularity    =(size_t) sysinfo.dwAllocationGranularity;

    mem->ImageCount     = 0;
    mem->ImageCapacity  = 0;
    mem->AddressList    = NULL;
    mem->AttributeList  = NULL;

    id_table_create(&mem->ImageIds, bucket_count);
    mem->AddressList    =(image_memory_addr_t  *) malloc(expected_image_count * sizeof(image_memory_addr_t));
    mem->AttributeList  =(image_memory_info_t  *) malloc(expected_image_count * sizeof(image_memory_info_t));
    mem->ImageCapacity  = expected_image_count;
}

/// @summary Frees all resources associated with an image memory manager.
/// @param mem The image memory manager to delete.
public_function void image_memory_delete(image_memory_t *mem)
{   // decommit and release all reserved memory immediately.
    for (size_t i = 0, n = mem->ImageCount; i < n; ++i)
    {
        VirtualFree(mem->AddressList[i].BaseAddress, 0, MEM_RELEASE);
    }
    // free the image list data.
    id_table_delete(&mem->ImageIds);
    free(mem->AttributeList);
    free(mem->AddressList);
    // clear counts and NULL pointers.
    mem->BytesReserved  = 0;
    mem->BytesCommitted = 0;
    mem->ImageCount     = 0;
    mem->ImageCapacity  = 0;
    mem->AddressList    = NULL;
    mem->AttributeList  = NULL;
}

/// @summary Retrieves information required to access an image.
/// @param mem The image memory manager that owns the image data.
/// @param image_id The application-defined image identifier.
/// @param storage On return, this structure is updated with image storage attributes.
/// @return true if storage information was retrieved.
public_function bool image_memory_storage_info(image_memory_t *mem, uintptr_t image_id, image_storage_info_t &storage)
{
    size_t image_index;
    if (id_table_get(&mem->ImageIds , image_id, &image_index))
    {   // copy the image information into the output structure.
        image_memory_addr_t   &addr = mem->AddressList  [image_index];
        image_memory_info_t   &info = mem->AttributeList[image_index];
        storage.ImageFormat   = info.Format;
        storage.Compression   = info.Compression;
        storage.Encoding      = info.Encoding;
        storage.AccessType    = info.AccessType;
        storage.ElementCount  = info.ElementCount;
        storage.LevelCount    = info.LevelCount;
        storage.BytesReserved = addr.BytesReserved;
        return true;
    }
    else
    {   // this image is not known. return data indicating this.
        storage.ImageFormat   = DXGI_FORMAT_UNKNOWN;
        storage.Compression   = IMAGE_COMPRESSION_NONE;
        storage.Encoding      = IMAGE_ENCODING_RAW;
        storage.AccessType    = IMAGE_ACCESS_UNKNOWN;
        storage.ElementCount  = 0;
        storage.LevelCount    = 0;
        storage.BytesReserved = 0;
        return false;
    }
}

/// @summary Reserves address space for all elements of an image.
/// @param mem The image memory manager to allocate from.
/// @param image_id The application-defined identifier of the image.
/// @param def The complete image attributes, specifying the total element and level count.
/// @param encoding One of image_encoding_e specifying the storage encoding of the image data.
/// @param access_type One of image_access_type_e specifying the storage access type of the image data.
/// @return ERROR_SUCCESS or a system error code.
public_function uint32_t image_memory_reserve_image(image_memory_t *mem, uintptr_t image_id, image_definition_t const &def, int encoding, int access_type)
{
    size_t image_index;
    if (id_table_get(&mem->ImageIds, image_id, &image_index))
    {   // this image is already defined. is this definition identical?
        image_memory_info_t const &existing = mem->AttributeList[image_index];
        if (def.ElementCount    == existing.ElementCount           && 
            def.LevelCount      == existing.LevelCount             && 
            def.ImageFormat     == existing.Format                 && 
            def.Width           == existing.Levels[0].LevelWidth   && 
            def.Height          == existing.Levels[0].LevelHeight  && 
            def.SliceCount      == existing.Levels[0].LevelSlices)
        {   // the definitions are identical; we're done.
            return ERROR_SUCCESS;
        }
        // else, the image exists with non-resolvable differences.
        // even if it's only the element count that changes, buffers 
        // could be locked which would prevent relocation.
        return ERROR_ALREADY_EXISTS;
    }
    else
    {   // this image is not currently known, so create a new record.
        if (mem->ImageCount  == mem->ImageCapacity)
        {   // grow the capacity of the image list.
            size_t old_amount = mem->ImageCapacity;
            size_t new_amount = calculate_capacity(old_amount, old_amount+1, 1024, 1024);
            image_memory_addr_t *new_addr =(image_memory_addr_t*) realloc(mem->AddressList  , new_amount * sizeof(image_memory_addr_t));
            image_memory_info_t *new_info =(image_memory_info_t*) realloc(mem->AttributeList, new_amount * sizeof(image_memory_info_t));
            if (new_addr != NULL) mem->AddressList   = new_addr;
            if (new_info != NULL) mem->AttributeList = new_info;
            if (new_addr != NULL && new_info != NULL)  mem->ImageCapacity = new_amount;
            else return ERROR_OUTOFMEMORY;

            // initialize the newly allocated elements.
            size_t new_index  = old_amount;
            size_t new_count  = new_amount - old_amount;
            memset(&mem->AddressList  [new_index], 0, new_count * sizeof(image_memory_addr_t));
            memset(&mem->AttributeList[new_index], 0, new_count * sizeof(image_memory_info_t));
        }
        // reserve the slot for the new image.
        image_index = mem->ImageCount;
    }

    // store a reference to the image data records.
    image_memory_addr_t &addr = mem->AddressList  [image_index];
    image_memory_info_t &info = mem->AttributeList[image_index];

    // figure out how much memory needs to be allocated.
    size_t element_used       = 0;
    size_t element_size       = image_memory_element_size(def, mem->PageSize , element_used);
    size_t reserve_bytes      = def.ElementCount * element_size;
    void  *reserve_buffer     = VirtualAlloc(NULL, reserve_bytes, MEM_RESERVE, PAGE_READWRITE);
    uint32_t              *es =(uint32_t            *) malloc(def.ElementCount * sizeof(uint32_t));
    image_memory_level_t  *la =(image_memory_level_t*) malloc(def.LevelCount   * sizeof(image_memory_level_t));
    if (reserve_buffer == NULL || es == NULL || la == NULL)
    {   // memory allocation failed. 
        free(la); free(es); VirtualFree(reserve_buffer, reserve_bytes, MEM_RELEASE);
        return ERROR_OUTOFMEMORY;
    }

    // initialize the address range block.
    addr.BaseAddress          = reserve_buffer;
    addr.BytesReserved        = reserve_bytes;
    addr.BytesCommitted       = 0;
    addr.ImageStatus          = IMAGE_MEMORY_FLAG_NONE;
    
    // initialize the image attribute block.
    info.ImageId              = image_id;
    info.Format               = def.ImageFormat;
    info.Compression          = def.Compression;
    info.Encoding             = encoding;
    info.AccessType           = access_type;
    info.ElementCount         = def.ElementCount;
    info.LevelCount           = def.LevelCount;
    info.BytesPerPixel        = def.BytesPerPixel;
    info.BytesPerBlock        = def.BytesPerBlock;
    info.BytesPerElement      = element_size;
    info.BytesPerElementUsed  = element_used;
    info.ElementStatus        = es;
    info.Levels               = la;

    // element status start out as zero (no locks, no commits):
    memset(info.ElementStatus, 0, def.ElementCount * sizeof(uint32_t));

    // generate level attributes from the dds_level_desc_t list:
    size_t level_offset  = 0;
    for (size_t i = 0, n = def.LevelCount; i < n; ++i)
    {
        info.Levels[i].LevelOffset     = level_offset;
        info.Levels[i].LevelWidth      = def.LevelInfo[i].Width;
        info.Levels[i].LevelHeight     = def.LevelInfo[i].Height;
        info.Levels[i].LevelSlices     = def.LevelInfo[i].Slices;
        info.Levels[i].BytesPerElement = def.LevelInfo[i].BytesPerElement;
        info.Levels[i].BytesPerRow     = def.LevelInfo[i].BytesPerRow;
        info.Levels[i].BytesPerSlice   = def.LevelInfo[i].BytesPerSlice;
        level_offset                  += def.LevelInfo[i].Slices * def.LevelInfo[i].BytesPerSlice;
    }

    // make the image visible to the rest of the system.
    id_table_put(&mem->ImageIds, image_id, image_index);
    mem->BytesReserved += reserve_bytes;
    mem->ImageCount++;
    return ERROR_SUCCESS;
}

/// @summary Locks an entire image element, including all mipmap levels, so allow the data to be read or written. Levels can be unlocked individually or as a group.
/// @param mem The image memory manager.
/// @param image_id The application-defined image identifier.
/// @param element The zero-based index of the array item or frame to lock.
/// @param levels If non-NULL, points to an array of LevelCount items to populate with information about the element mipmap levels.
/// @param storage On return, this structure is populated with image storage attributes for the image element.
/// @return A pointer to the start of the image element data for miplevel 0, or NULL.
public_function void* image_memory_lock_element(image_memory_t *mem, uintptr_t image_id, size_t element, dds_level_desc_t *levels, image_storage_info_t &storage)
{
    size_t image_index;
    if (id_table_get(&mem->ImageIds, image_id, &image_index))
    {
        image_memory_addr_t   &addr  = mem->AddressList  [image_index];
        image_memory_info_t   &info  = mem->AttributeList[image_index];
        uint8_t       *element_data  = ((uint8_t*)  addr.BaseAddress) + (info.BytesPerElement * element);
        uint32_t       element_flags = image_memory_element_status_flags(info.ElementStatus    [element]);
        size_t         element_locks = image_memory_element_lock_count  (info.ElementStatus    [element]);
        
        if ((element_flags & IMAGE_MEMORY_FLAG_COMMITTED) == 0)
        {   // the memory region hasn't been committed yet. do so now.
            if (VirtualAlloc(element_data, info.BytesPerElement, MEM_COMMIT, PAGE_READWRITE) == NULL)
            {   // unable to commit the memory region; the lock fails.
                return NULL;
            }
            element_flags  = IMAGE_MEMORY_FLAG_COMMITTED;
            addr.BytesCommitted  += info.BytesPerElement;
            mem->BytesCommitted  += info.BytesPerElement;
        }
        
        // update the packed element status with any new flags, and increase the lock count by the number of levels.
        info.ElementStatus[element] = image_memory_make_element_status(element_flags, element_locks+info.LevelCount);

        // populate the mip-level descriptors, if the caller wants that information.
        for (size_t i = 0, n = info.LevelCount; levels != NULL && i < n; ++i)
        {
            levels[i].Index           = i;
            levels[i].Width           = info.Levels[i].LevelWidth;
            levels[i].Height          = info.Levels[i].LevelHeight;
            levels[i].Slices          = info.Levels[i].LevelSlices;
            levels[i].BytesPerElement = info.Levels[i].BytesPerElement;
            levels[i].BytesPerRow     = info.Levels[i].BytesPerRow;
            levels[i].BytesPerSlice   = info.Levels[i].BytesPerSlice;
            levels[i].DataSize        = info.Levels[i].BytesPerSlice * info.Levels[i].LevelSlices;
            levels[i].Format          = info.Format;
        }

        // populate the storage descriptor.
        storage.ImageFormat   = info.Format;
        storage.Compression   = info.Compression;
        storage.Encoding      = info.Encoding;
        storage.AccessType    = info.AccessType;
        storage.ElementCount  = 1;
        storage.LevelCount    = info.LevelCount;
        storage.BytesReserved = info.BytesPerElementUsed;

        // return a pointer to the start of the first level:
        return element_data;
    }
    else return NULL; // the image is not known.
}

/// @summary Lock a single mipmap level of an image element to read or write the data.
/// @param mem The image memory manager that defines the image.
/// @param image_id The application-defined image identifier.
/// @param element The zero-based index of the image array item, frame, or cubemap face.
/// @param level The zero-based index of the mipmap level to lock (0 = highest resolution.)
/// @param desc On return, this structure is populated with information used to access the data.
/// @param storage On return, this structure is populated with image storage attributes for the mipmap level.
/// @return A pointer to the start of the level data, or NULL.
public_function void* image_memory_lock_level(image_memory_t *mem, uintptr_t image_id, size_t element, size_t level, dds_level_desc_t &desc, image_storage_info_t &storage)
{
    size_t image_index;
    if (id_table_get(&mem->ImageIds, image_id, &image_index))
    {
        image_memory_addr_t   &addr  = mem->AddressList  [image_index];
        image_memory_info_t   &info  = mem->AttributeList[image_index];
        image_memory_level_t  &attr  = info.Levels       [level];
        uint8_t       *element_data  = ((uint8_t*)  addr.BaseAddress) + (info.BytesPerElement * element);
        uint32_t       element_flags = image_memory_element_status_flags(info.ElementStatus    [element]);
        size_t         element_locks = image_memory_element_lock_count  (info.ElementStatus    [element]);
        
        if ((element_flags & IMAGE_MEMORY_FLAG_COMMITTED) == 0)
        {   // the memory region hasn't been committed yet. do so now.
            if (VirtualAlloc(element_data, info.BytesPerElement, MEM_COMMIT, PAGE_READWRITE) == NULL)
            {   // unable to commit the memory region; the lock fails.
                return NULL;
            }
            element_flags |= IMAGE_MEMORY_FLAG_COMMITTED;
            addr.BytesCommitted  += info.BytesPerElement;
            mem->BytesCommitted  += info.BytesPerElement;
        }
        
        // update the packed element status with any new flags, and increase the lock count by one.
        info.ElementStatus[element] = image_memory_make_element_status(element_flags, element_locks+1);
        
        // populate the mip-level descriptor.
        desc.Index            = level;
        desc.Width            = attr.LevelWidth;
        desc.Height           = attr.LevelHeight;
        desc.Slices           = attr.LevelSlices;
        desc.BytesPerElement  = attr.BytesPerElement;
        desc.BytesPerRow      = attr.BytesPerRow;
        desc.BytesPerSlice    = attr.BytesPerSlice;
        desc.DataSize         = attr.BytesPerSlice * attr.LevelSlices;
        desc.Format           = info.Format;
        
        // populate the storage descriptor.
        storage.ImageFormat   = info.Format;
        storage.Compression   = info.Compression;
        storage.Encoding      = info.Encoding;
        storage.AccessType    = info.AccessType;
        storage.ElementCount  = 1;
        storage.LevelCount    = 1;
        storage.BytesReserved = attr.BytesPerSlice * attr.LevelSlices;
        
        // return a pointer to the start of the requested level.
        return (element_data  + attr.LevelOffset);
    }
    else return NULL; // the image is not known.
}

/// @summary Unlock a single mipmap level of an image element.
/// @param mem The image memory manager.
/// @param image_id The application-defined image identifier.
/// @param element The zero-based index of the image element (array item or frame.)
/// @param level The zero-based index of the mipmap level to unlock (0 = highest resolution.)
public_function void image_memory_unlock_level(image_memory_t *mem, uintptr_t image_id, size_t element, size_t level)
{
    size_t image_index;
    if (id_table_get(&mem->ImageIds, image_id, &image_index))
    {
        image_memory_info_t &i   = mem->AttributeList[image_index];
        uint32_t element_flags   = image_memory_element_status_flags(i.ElementStatus[element]);
        size_t   element_locks   = image_memory_element_lock_count  (i.ElementStatus[element]);
        if (element_locks > 0)     element_locks--;
        i.ElementStatus[element] = image_memory_make_element_status (element_flags, element_locks);
        image_memory_process_pending_evict(mem, image_index, element);
    }
    UNREFERENCED_PARAMETER(level);
}

/// @summary Unlock all miplevels of an image element.
/// @param mem The image memory manager.
/// @param image_id The application-defined image identifier.
/// @param element The zero-based index of the image element (array item or frame.)
public_function void image_memory_unlock_element(image_memory_t *mem, uintptr_t image_id, size_t element)
{
    size_t image_index;
    if (id_table_get(&mem->ImageIds, image_id, &image_index))
    {
        image_memory_info_t &i = mem->AttributeList[image_index];
        uint32_t element_flags = image_memory_element_status_flags(i.ElementStatus[element]);
        size_t   element_locks = image_memory_element_lock_count  (i.ElementStatus[element]);
        if (element_locks >= i.LevelCount)
        {   // image_memory_lock_element increased the lock count by i.LevelCount.
            element_locks -= i.LevelCount;
        }
        else
        {   // clear the lock count. this may or may not indicate an error, as the
            // user might want to just lock one level and then somewhere else unlock
            // the entire element without keeping track of the level index...
            element_locks  = 0;
        }
        i.ElementStatus[element] = image_memory_make_element_status(element_flags, element_locks);
        image_memory_process_pending_evict(mem, image_index, element);
    }
}

/// @summary Marks an image element (including all of its mipmap levels) for eviction.
/// @param mem The image memory manager.
/// @param image_id The application-defined image identifier.
/// @param eptr The pointer to level 0 of the image element data.
/// @param size The size of the image element data, in bytes (unused).
/// @param force_evict Specify true to decommit the image element memory immediately, regardless of any outstanding locks.
public_function void image_memory_evict_element(image_memory_t *mem, uintptr_t image_id, void *eptr, size_t size, bool force_evict=false)
{
    size_t image_index;
    if (id_table_get(&mem->ImageIds, image_id, &image_index))
    {
        image_memory_addr_t  &addr = mem->AddressList  [image_index];
        image_memory_info_t  &info = mem->AttributeList[image_index];
        uint8_t const        *elem =(uint8_t const*)    eptr;
        uint8_t const        *base =(uint8_t const*)    addr.BaseAddress;
        size_t       element_index =(elem   -  base) /  info.BytesPerElement;
        uint32_t     element_flags = image_memory_element_status_flags(info.ElementStatus[element_index]);
        size_t       element_locks = image_memory_element_lock_count  (info.ElementStatus[element_index]);
        if (force_evict)   element_locks  = 0;
        info.ElementStatus[element_index] = image_memory_make_element_status(element_flags | IMAGE_MEMORY_FLAG_EVICT, element_locks);
        image_memory_process_pending_evict(mem, image_index, element_index);
    }
    UNREFERENCED_PARAMETER(size);
}

/// @summary Marks an image element (including all of its mipmap levels) for eviction. Image elements are not evicted until their lock count drops to zero.
/// @param mem The image memory manager.
/// @param image_id The application-defined image identifier.
/// @param element The zero-based index of the image element (array item or frame) to evict.
/// @param force_evict Specify true to decommit the image element memory immediately, regardless of any outstanding locks.
public_function void image_memory_evict_element(image_memory_t *mem, uintptr_t image_id, size_t element, bool force_evict=false)
{
    size_t image_index;
    if (id_table_get(&mem->ImageIds, image_id, &image_index))
    {
        image_memory_info_t  &info  = mem->AttributeList[image_index];
        uint32_t     element_flags  = image_memory_element_status_flags(info.ElementStatus[element]);
        size_t       element_locks  = image_memory_element_lock_count  (info.ElementStatus[element]);
        if (force_evict)              element_locks = 0;
        info.ElementStatus[element] = image_memory_make_element_status (element_flags | IMAGE_MEMORY_FLAG_EVICT, element_locks);
        image_memory_process_pending_evict(mem, image_index, element);
    }
}

/// @summary Marks all image elements and mipmap levels for eviction. Image elements are not evicted until their lock count drops to zero.
/// @param mem The image memory manager.
/// @param image_id The application-defined image identifier.
public_function void image_memory_evict_image(image_memory_t *mem, uintptr_t image_id)
{
    size_t image_index;
    if (id_table_get(&mem->ImageIds, image_id, &image_index))
    {
        image_memory_info_t  &info = mem->AttributeList[image_index];
        for (size_t i = 0, n = info.ElementCount; i < n; ++i)
        {
            uint32_t element_flags = image_memory_element_status_flags(info.ElementStatus[i]);
            size_t   element_locks = image_memory_element_lock_count  (info.ElementStatus[i]);
            info.ElementStatus[i]  = image_memory_make_element_status (element_flags | IMAGE_MEMORY_FLAG_EVICT, element_locks);
            image_memory_process_pending_evict(mem, image_index, i);
        }
    }
}

/// @summary Marks all image elements and mipmap levels for eviction, and releases all reserved address space. Image elements are not evicted until their lock count drops to zero.
/// @param mem The image memory manager.
/// @param image_id The application-defined image identifier.
/// @param force_drop Specify true to drop the image immediately, regardless of any outstanding locks.
public_function void image_memory_drop_image(image_memory_t *mem, uintptr_t image_id, bool force_drop=false)
{
    size_t image_index;
    if (id_table_get(&mem->ImageIds, image_id, &image_index))
    {
        image_memory_addr_t  &addr = mem->AddressList  [image_index];
        image_memory_info_t  &info = mem->AttributeList[image_index];
        if (force_drop)
        {   // force the image to be dropped by setting BytesCommitted to 0.
            // this decommits and releases the entire reserved range at once.
            mem->BytesCommitted -= addr.BytesCommitted;
            addr.BytesCommitted  = 0;
            addr.ImageStatus     = IMAGE_MEMORY_FLAG_DROP;
            image_memory_process_pending_drop(mem, image_index);
        }
        else
        {   // mark each image element for eviction.
            for (size_t i = 0, n = info.ElementCount; i < n; ++i)
            {
                uint32_t element_flags = image_memory_element_status_flags(info.ElementStatus[i]);
                size_t   element_locks = image_memory_element_lock_count  (info.ElementStatus[i]);
                info.ElementStatus[i]  = image_memory_make_element_status (element_flags | IMAGE_MEMORY_FLAG_EVICT, element_locks);
                image_memory_process_pending_evict(mem, image_index, i);
            }
            addr.ImageStatus|= IMAGE_MEMORY_FLAG_DROP;
            image_memory_process_pending_drop(mem, image_index);
        }
    }
}
