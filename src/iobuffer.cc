/*/////////////////////////////////////////////////////////////////////////////
/// @summary Implements a buffer management scheme specifically designed for 
/// file and network I/O, ensuring that buffers have the necessary alignment 
/// to support overlapped and unbuffered I/O operations.
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
/// @summary Defines the state associated with an I/O buffer manager, which 
/// allocates a single large chunk of memory aligned to a multiple of the 
/// physical disk sector size, and then allows the caller to allocate fixed-
/// size chunks from within that buffer. The allocator is only safe to use 
/// from a single thread. Use returned buffers for either cached or direct I/O.
struct io_buffer_allocator_t final
{
    size_t  TotalSize;   /// The total number of bytes reserved.
    size_t  AllocSize;   /// The size of a single allocation, in bytes.
    size_t  FreeCount;   /// The number of unallocated AllocSize blocks.
    void  **FreeList;    /// Pointers to the start of each unallocated block.
    void   *BaseAddress; /// The base address of the committed address space range.
    size_t  PageSize;    /// The size of a single VMM page, in bytes.

    io_buffer_allocator_t(void);
    ~io_buffer_allocator_t(void);

    size_t  bytes_free(void) const;
    size_t  bytes_used(void) const;
    size_t  buffers_free(void) const;
    size_t  buffers_used(void) const;

    bool    reserve(size_t total_size, size_t alloc_size);
    void    release(void);
    void   *get_buffer(void);
    void    put_buffer(void *buffer);
    void    flush(void);
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
/// @summary Retrieve the physical sector size for a block-access device.
/// @param file The handle to an open file on the device.
/// @return The size of a physical sector on the specified device.
public_function size_t physical_sector_size(HANDLE file)
{   // http://msdn.microsoft.com/en-us/library/ff800831(v=vs.85).aspx
    // for structure STORAGE_ACCESS_ALIGNMENT
    // Vista and Server 2008+ only - XP not supported.
    size_t const DefaultPhysicalSectorSize = 4096;
    STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR desc;
    STORAGE_PROPERTY_QUERY    query;
    memset(&desc  , 0, sizeof(desc));
    memset(&query , 0, sizeof(query));
    query.QueryType  = PropertyStandardQuery;
    query.PropertyId = StorageAccessAlignmentProperty;
    DWORD bytes = 0;
    BOOL result = DeviceIoControl(
        file,
        IOCTL_STORAGE_QUERY_PROPERTY,
        &query, sizeof(query),
        &desc , sizeof(desc),
        &bytes, NULL);
    return result ? desc.BytesPerPhysicalSector : DefaultPhysicalSectorSize;
}

/// @summary Initializes all fields of the allocator to 0/NULL. No memory is
/// reserved or allocated until io_buffer_allocator_t::reserve() is called.
inline io_buffer_allocator_t::io_buffer_allocator_t(void) 
    : 
    TotalSize(0), 
    AllocSize(0), 
    FreeCount(0), 
    FreeList(NULL), 
    BaseAddress(NULL),
    PageSize(0)
{
    /* empty */
}

/// @summary Releases any memory resevations held by the allocator.
inline io_buffer_allocator_t::~io_buffer_allocator_t(void)
{
    release();
}

/// @summary Calaculate the number of bytes currently unused.
/// @return The number of bytes currently available for use by the application.
inline size_t io_buffer_allocator_t::bytes_free(void) const
{
    return (AllocSize * FreeCount);
}

/// @summary Calaculate the number of bytes currently allocated.
/// @return The number of bytes currently in-use by the application.
inline size_t io_buffer_allocator_t::bytes_used(void) const
{
    return TotalSize - (AllocSize * FreeCount);
}

/// @summary Calculate the number of buffers currently allocated.
/// @return The number of buffers currently in-use by the application.
inline size_t io_buffer_allocator_t::buffers_used(void) const
{
    return ((TotalSize / AllocSize) - FreeCount);
}

/// @summary Calculate the number of buffers currently available for use.
/// @return The number of buffers currently available for use by the application.
inline size_t io_buffer_allocator_t::buffers_free(void) const
{
    return FreeCount;
}

/// @summary Reserve the I/O buffer pool with the specified minimum buffer and
/// allocation sizes. The total buffer size and allocation size may be somewhat
/// larger than what is requested in order to meet alignment requirements.
/// @param total_size The total number of bytes to allocate. This size is
/// rounded up to the nearest even multiple of the sub-allocation size. It is
/// best to keep the total buffer size as small as is reasonable for the application
/// workload, as this memory may be allocated from the non-paged pool.
/// @param alloc_size The sub-allocation size, in bytes. This is the size of a
/// single buffer that can be returned to the application. This size is rounded
/// up to the nearest even multiple of the largest disk sector size.
/// @return true if the memory pool was reserved. Check the TotalSize and
/// AllocSize fields to determine the values selected by the system.
bool io_buffer_allocator_t::reserve(size_t total_size, size_t alloc_size)
{
    SYSTEM_INFO sysinfo = {0};
    GetNativeSystemInfo_Func(&sysinfo);

    // round the allocation size up to an even multiple of the page size.
    // round the total size up to an even multiple of the allocation size.
    // note that VirtualAlloc will further round up the total size of the
    // allocation to the nearest sysinfo.dwAllocationGranularity (64K)
    // boundary, but this extra padding will be 'lost' to us.
    size_t page_size = (size_t) sysinfo.dwPageSize;
    alloc_size       = align_up(alloc_size, page_size);
    total_size       = align_up(total_size, alloc_size);
    size_t nallocs   = total_size / alloc_size;

    // in order to lock the entire allocated region in physical memory, we
    // might need to increase the size of the process' working set. this is
    // Vista and Windows Server 2003+ only, and requires that the process
    // be running as (at least) a Power User or Administrator.
    HANDLE process   = GetCurrentProcess();
    SIZE_T min_wss   = 0;
    SIZE_T max_wss   = 0;
    DWORD  wss_flags = QUOTA_LIMITS_HARDWS_MIN_ENABLE | QUOTA_LIMITS_HARDWS_MAX_DISABLE;
    GetProcessWorkingSetSize(process, &min_wss, &max_wss);
    min_wss += total_size;
    max_wss += total_size;
    if (!SetProcessWorkingSetSizeEx_Func(process, min_wss, max_wss, wss_flags))
    {   // the minimum working set size could not be set.
        return false;
    }

    // reserve and commit the entire region, and then pin it in physical memory.
    // this prevents the buffers from being paged out during normal execution.
    // if the address range cannot be pinned, it's not a fatal error.
    DWORD  protect  = PAGE_READWRITE;
    DWORD  flags    = MEM_COMMIT | MEM_RESERVE;
    void  *baseaddr = VirtualAlloc(NULL, total_size, flags, protect);
    if (baseaddr == NULL)
    {   // the requested amount of memory could not be allocated.
        return false;
    }
    if (!VirtualLock(baseaddr, total_size))
    {   // the pages could not be pinned in physical memory.
        // it's still possible to run in this case; don't fail.
    }

    void **freelist = new void*[nallocs];
    if (freelist == NULL)
    {   // the requested memory could not be allocated.
        VirtualUnlock(baseaddr , total_size);
        VirtualFree(baseaddr, 0, MEM_RELEASE);
        return false;
    }

    // at this point, everything that could have failed has succeeded.
    // set the fields of the allocator and initialize the free list.
    TotalSize   = total_size;
    AllocSize   = alloc_size;
    FreeCount   = nallocs;
    FreeList    = freelist;
    PageSize    = page_size;
    BaseAddress = baseaddr;
    uint8_t *buf_it = (uint8_t*) baseaddr;
    for (size_t i = 0; i < nallocs; ++i)
    {
        freelist[i] = buf_it;
        buf_it     += alloc_size;
    }
    return true;
}

/// @summary Release the backing buffer pool for an I/O buffer allocator.
void io_buffer_allocator_t::release(void)
{
    if (FreeList != NULL)
    {
        delete[] FreeList;
    }
    if (BaseAddress != NULL)
    {
        VirtualUnlock(BaseAddress , TotalSize);
        VirtualFree(BaseAddress, 0, MEM_RELEASE);
    }
    TotalSize   = 0;
    AllocSize   = 0;
    BaseAddress = NULL;
    FreeCount   = 0;
    FreeList    = NULL;
}

/// @summary Retrieves an I/O buffer from the pool.
/// @return A pointer to the I/O buffer, or NULL if no buffers are available.
inline void* io_buffer_allocator_t::get_buffer(void)
{
    if (FreeCount > 0)
    {   // return the next buffer from the free list.
        return FreeList[--FreeCount];
    }
    else return NULL; // no buffers available for use.
}

/// @summary Returns an I/O buffer to the pool.
/// @param iobuf The address of the buffer returned by io_buffer_allocator_get().
inline void io_buffer_allocator_t::put_buffer(void *iobuf)
{
    assert(iobuf != NULL);
    FreeList[FreeCount++] = iobuf;
}

/// @summary Returns all I/O buffers to the free list of the allocator, regardless
/// of whether any I/O buffers are in use by the application. The backing buffer
/// pool remains committed, and new allocations can be performed.
/// @param alloc The I/O buffer allocator to flush.
void io_buffer_allocator_t::flush(void)
{
    size_t const nallocs = TotalSize / AllocSize;
    size_t const allocsz = AllocSize;
    uint8_t       *bufit = (uint8_t*) BaseAddress;
    void         **freel = FreeList;
    for (size_t i = 0; i < nallocs; ++i)
    {
        freel[i]  = bufit;
        bufit    += allocsz;
    }
    FreeCount = nallocs;
}
