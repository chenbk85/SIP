/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines the interface to an image memory manager that can allocate
/// image buffers in system memory using the virtual memory management services
/// of the underlying operating system.
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
/// @summary The mask used to access the per-level lock count.
#define IMAGE_LEVEL_LOCK_MASK     0x0000FFFFU

/// @summary The mask used to access the per-level status bits.
#define IMAGE_LEVEL_STATUS_MASK   0xFFFF0000U

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Define flags that can be set on either an individual mip-level or an entire image.
enum image_memory_flags_e : uint32_t
{
    IMAGE_MEMORY_FLAG_NONE        = (0 << 0), /// The level memory is reserved, but not committed.
    IMAGE_MEMORY_FLAG_COMMITTED   = (1 << 0), /// The level memory is committed.
    IMAGE_MEMORY_FLAG_EVICT       = (1 << 0), /// The level memory should be decommitted when the lock count drops to zero.
    IMAGE_MEMORY_FLAG_DROP        = (1 << 1)  /// The image memory should be decommitted and released.
};

/// @summary Define the data describing a single level of the mip-chain. Each element has at least
/// one mip-level (0 = highest resolution). Each mip-level in image memory begins on an address 
/// that is an even multiple of the system page size.
struct image_memory_level_t
{
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
    uint32_t              Format;             /// One of dxgi_format_e specifying the storage format.
    int                   Compression;        /// One of image_compression_e specifying the storage compression.
    int                   Encoding;           /// One of image_encoding_e specifying the storage encoding type.
    int                   AccessType;         /// One of image_type_e specifying the access type.
    size_t                ElementCount;       /// The number of array elements, frames, or cubemap faces.
    size_t                LevelCount;         /// The number of mip-levels per element.
    size_t                BytesPerPixel;      /// The number of bytes allocated per-pixel.
    size_t                BytesPerBlock;      /// The number of bytes allocated per-block, or 0 if not block compressed.
    size_t                BytesPerElement;    /// The number of bytes reserved per-element (a sum of all items in LevelSizes.)
    uint32_t             *LevelStatus;        /// ElementCount * LevelCount items, lock count and image_memory_flags_e.
    image_memory_level_t *LevelAttribs;       /// LevelCount descriptions of each mip-level (0 = highest resolution).
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

    SRWLOCK               ImageLock;          /// Reader-Writer lock protecting the image list.
    size_t                ImageCount;         /// The number of images known to the image memory.
    size_t                ImageCapacity;      /// The number of image records that can be stored without reallocating lists.
    id_table_t            ImageIds;           /// The table mapping application defined image ID to list index.
    image_memory_addr_t  *AddressList;        /// The list of memory allocation information for each known image.
    image_memory_info_t  *AttributeList;      /// The list of image attributes for each known image.
};

/*///////////////
//   Globals   //
///////////////*/

/*///////////////////////
//   Local Functions   //
///////////////////////*/
/// @summary Calculate the size of a single image element (array item, frame, or cube face).
/// @param def The image definition.
/// @param page_size The size of a system virtual memory page, in bytes.
/// @return The size of a single image element, in bytes, rounded up to the nearest page size multiple.
internal_function size_t image_memory_element_size(image_definition_t const &def, size_t page_size)
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
    return align_up(s_base, page_size);
}

/*////////////////////////
//   Public Functions   //
////////////////////////*/
public_function void image_memory_create(image_memory_t *mem, size_t expected_image_count)
{
    SYSTEM_INFO sysinfo = {};
    GetNativeSystemInfo(&sysinfo);

    if (expected_image_count < 128)
    {   // TODO(rlk): make this cleaner
        expected_image_count = 128;
    }

    mem->BytesReserved  = 0;
    mem->BytesCommitted = 0;
    mem->PageSize       =(size_t) sysinfo.dwPageSize;
    mem->Granularity    =(size_t) sysinfo.dwAllocationGranularity;

    InitializeSRWLock(&mem->ImageLock);
    mem->ImageCount     = 0;
    mem->ImageCapacity  = 0;
    mem->AddressList    = NULL;
    mem->AttributeList  = NULL;

    id_table_create(&mem->ImageIds, expected_image_count / 128);
    mem->AddressList    =(image_memory_addr_t*) malloc(expected_image_count * sizeof(image_memory_addr_t));
    mem->AttributeList  =(image_memory_info_t*) malloc(expected_image_count * sizeof(image_memory_info_t));
    mem->ImageCapacity  = expected_image_count;
}

public_function void image_memory_delete(image_memory_t *mem)
{
}

public_function uint32_t image_memory_reserve_image(image_memory_t *mem, uintptr_t image_id, image_definition_t const &def, int encoding, int access_type)
{
    size_t image_index;
    if (id_table_get(&mem->ImageIds, image_id, &image_index))
    {   // this image is already defined. is this definition identical?
        image_memory_info_t const &existing = mem->AttributeList[image_index];
        if (def.ElementCount    == existing.ElementCount                 && 
            def.LevelCount      == existing.LevelCount                   && 
            def.ImageFormat     == existing.Format                       && 
            def.Width           == existing.LevelAttribs[0].LevelWidth   && 
            def.Height          == existing.LevelAttribs[0].LevelHeight  && 
            def.SliceCount      == existing.LevelAttribs[0].LevelSlices)
        {   // the definitions are identical; we're done.
            // TODO(rlk): release the lock.
            return ERROR_SUCCESS;
        }
        // else, the image exists with non-resolvable differences.
        // even if it's only the element count that changes, buffers 
        // could be locked which would prevent relocation.
        // TODO(rlk): release the lock.
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
    size_t element_size       = image_memory_element_size(def, mem->PageSize);
    size_t total_levels       = def.ElementCount * def.LevelCount;
    size_t reserve_bytes      = def.ElementCount * element_size;
    void  *reserve_buffer     = VirtualAlloc(NULL, reserve_bytes, MEM_RESERVE, PAGE_READWRITE);
    uint32_t              *ls =(uint32_t            *) malloc(total_levels   * sizeof(uint32_t));
    image_memory_level_t  *la =(image_memory_level_t*) malloc(def.LevelCount * sizeof(image_memory_level_t));
    if (reserve_buffer == NULL || ls == NULL || la == NULL)
    {   // memory allocation failed. 
        free(la); free(ls); VirtualFree(reserve_buffer, reserve_bytes, MEM_RELEASE);
        return ERROR_OUTOFMEMORY;
    }

    // initialize the address range block.
    addr.BaseAddress          = reserve_buffer;
    addr.BytesReserved        = reserve_bytes;
    addr.BytesCommitted       = 0;
    addr.ImageStatus          = IMAGE_MEMORY_FLAG_NONE;
    
    // initialize the image attribute block.
    info.Format               = def.ImageFormat;
    info.Compression          = def.Compression;
    info.Encoding             = encoding;
    info.AccessType           = access_type;
    info.ElementCount         = def.ElementCount;
    info.LevelCount           = def.LevelCount;
    info.BytesPerPixel        = def.BytesPerPixel;
    info.BytesPerBlock        = def.BytesPerBlock;
    info.BytesPerElement      = element_size;
    info.LevelStatus          = ls;
    info.LevelAttribs         = la;

    // level status start out as zero (no locks, no commits):
    memset(info.LevelStatus, 0, total_levels * sizeof(uint32_t));

    // generate level attributes from the dds_level_desc_t list:
    for (size_t i = 0, n = def.LevelCount; i < n; ++i)
    {
        info.LevelAttribs[i].LevelWidth      = def.LevelInfo[i].Width;
        info.LevelAttribs[i].LevelHeight     = def.LevelInfo[i].Height;
        info.LevelAttribs[i].LevelSlices     = def.LevelInfo[i].Slices;
        info.LevelAttribs[i].BytesPerElement = def.LevelInfo[i].BytesPerElement;
        info.LevelAttribs[i].BytesPerRow     = def.LevelInfo[i].BytesPerRow;
        info.LevelAttribs[i].BytesPerSlice   = def.LevelInfo[i].BytesPerSlice;
        // TODO(rlk): add a byte offset here for easy access
    }
    
    // finished defining the allocator image record.
    mem->ImageCount++;
    return ERROR_SUCCESS;
}

public_function void* image_memory_lock_level(image_memory_t *mem, uintptr_t image_id, size_t element, size_t level)
{
    // TODO(rlk): look up the image
    // TODO(rlk): update LockStatus (lock count) [element * level]
    // TODO(rlk): calculate the element address (image_memory_info_t::BytesPerElement * element) + level offset
}

public_function void image_memory_unlock_level(image_memory_t *mem, uintptr_t image_id, size_t element, size_t level)
{
    // TODO(rlk): look up the image
    // TODO(rlk): update LockStatus (lock count) [element * level]
    // TODO(rlk): if lock count is 0 and evict is set, release the memory
}

public_function void image_memory_evict_element(image_memory_t *mem, uintptr_t image_id, void *addr, size_t size)
{
}

public_function void image_memory_evict_element(image_memory_t *mem, uintptr_t image_id, size_t element)
{
}

public_function void image_memory_evict_image(image_memory_t *mem, uintptr_t image_id)
{
}

public_function void image_memory_drop_image(image_memory_t *mem, uintptr_t image_id)
{
}
