/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines the constants, types and functions that implement the 
/// virtual file system driver. The virtual file system driver abstracts access
/// to the underlying file system, allowing files to be loaded from a local 
/// disk, remote disk or archive file (zip file, tar file, etc.) The VFS driver
/// provides a consistent and friendly interface to the application. All I/O, 
/// with a few exceptions for utility functions, is performed asynchronously. 
/// The virtual file system driver generates prioritized I/O driver commands 
/// and is responsible for creating stream decoders based on the underlying 
/// file system or archive type. These stream decoders will receive I/O results
/// from the asynchronous I/O driver and are returned directly to the caller, 
/// who may poll the decoder for data on a separate thread.
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
/// @summary Represents a single mounted file system within the larger VFS.
/// The file system could be represented by a local disk, a remote disk, or
/// an archive file such as a ZIP or TAR file. Mount points are maintained 
/// in a priority queue, and are created during application startup.
struct vfs_mount_t
{
    uintptr_t     Identifier;    /// The application-defined mount point identifier.
    pio_driver_t *PIO;           /// The prioritized I/O driver interface.
    void         *State;         /// State data associated with the mount point.
    // NOTE: everything here should be trivally movable.
    // and a bunch of function pointers.
    // ...
};

/// @summary Defines the data associated with a dynamic list of virtual file 
/// system mount points. Mount points are ordered using a priority, which allows
/// more fine-grained control with regard to overriding. The internal structure
/// is designed such that items can be iterated over in priority order.
struct vfs_mounts_t
{
    size_t        Count;         /// The number of defined mount points.
    size_t        Capacity;      /// The number of slots of queue storage.
    uintptr_t    *MountIds;      /// The set of application-defined mount identifiers.
    vfs_mount_t  *MountData;     /// The set of mount point internal data and functions.
    uint32_t     *Priority;      /// The set of mount point priority values.
};

/// @summary Maintains all of the state for a virtual file system driver.
struct vfs_driver_t
{
    pio_driver_t *PIO;           /// The prioritized I/O driver interface.
    vfs_mounts_t  Mounts;        /// The set of active mount points.
};

/*///////////////
//   Globals   //
///////////////*/

/*///////////////////////
//   Local Functions   //
///////////////////////*/
/// @summary Initializes a prioritized mount point list to empty.
/// @param plist The prioritized mount point list to initialize.
/// @param capacity The initial capacity of the mount point list.
internal_function void vfs_mounts_create(vfs_mounts_t &plist, size_t capacity)
{
    plist.Count     = 0;
    plist.Capacity  = 0;
    plist.MountIds  = NULL;
    plist.MountData = NULL;
    plist.Priority  = NULL;
    if (capacity  > 0)
    {   // pre-allocate storage for the specified number of items.
        uintptr_t  *i = (uintptr_t  *) malloc(capacity * sizeof(uintptr_t));
        vfs_mount_t*m = (vfs_mount_t*) malloc(capacity * sizeof(vfs_mount_t));
        uint32_t   *p = (uint32_t   *) malloc(capacity * sizeof(uint32_t));
        if (i != NULL)  plist.MountIds   = i;
        if (m != NULL)  plist.MountData  = m;
        if (p != NULL)  plist.Priority   = p;
        if (i != NULL && m != NULL && p != NULL) plist.Capacity = capacity;
    }
}

/// @summary Frees resources associated with a prioritized mount point list.
/// @param plist The prioritized mount point list to delete.
internal_function void vfs_mounts_delete(vfs_mounts_t &plist)
{
    if (plist.Priority  != NULL) free(plist.Priority);
    if (plist.MountData != NULL) free(plist.MountData);
    if (plist.MountIds  != NULL) free(plist.MountIds);
    plist.Count     = plist.Capacity = 0;
    plist.MountIds  = NULL;
    plist.MountData = NULL;
    plist.Priority  = NULL;
}

// insert an item such that 

/// @summary Inserts a mount point into a prioritized mount point list.
/// @param plist The prioritized mount point list to update.
/// @param id A unique, application-defined mount point identifier.
/// @param priority The priority value of the mount point being added.
/// @return The mount point definition to populate. 
internal_function vfs_mount_t* vfs_mounts_insert(vfs_mounts_t &plist, uintptr_t id, uint32_t priority)
{
    if (plist.Count == plist.Capacity)
    {   // grow all of the internal list storage to accomodate.
        size_t    newc = (plist.Capacity < 1024) ? (plist.Capacity * 2) : (plist.Capacity + 1024);
        uintptr_t   *i = (uintptr_t  *)    realloc (plist.MountIds , newc * sizeof(uintptr_t));
        vfs_mount_t *m = (vfs_mount_t*)    realloc (plist.MountData, newc * sizeof(vfs_mount_t));
        uint32_t    *p = (uint32_t   *)    realloc (plist.Priority , newc * sizeof(uint32_t));
        if (i != NULL)  plist.MountIds   = i;
        if (m != NULL)  plist.MountData  = m;
        if (p != NULL)  plist.Priority   = p;
        if (i != NULL && m != NULL && p != NULL) plist.Capacity = newc;
    }
    if (plist.Count < plist.Capacity)
    {   // perform a binary search of the priority list to 
        // determine where this new item should be inserted. 
        intptr_t ins_idx  =-1;
        intptr_t min_idx  = 0;
        intptr_t max_idx  = intptr_t(plist.Count  - 1);
        while   (min_idx <= max_idx)
        {   // for this use, we expect that many items will have 
            // the same priority value, so test for this case first.
            intptr_t mid  =(min_idx  +  max_idx) >> 1;
            uint32_t p_b  = plist.Priority[mid];
            if (priority == p_b)
            {   // now search for the end of this run of priority p_b
                // so that the new item appears last in the run.
                ins_idx   = mid;
                while (priority == plist.Priority[ins_idx])
                {
                    ++ins_idx;
                }
                break;
            }
            else if (priority < p_b)
            {   // continue searching the lower half of the range.
                max_idx = mid - 1;
            }
            else
            {   // continue searching the upper half of the range.
                min_idx = mid + 1;
            }
        }
        if (ins_idx < 0)
        {   // insert the item at the end of the list.
            ins_idx = plist.Count++;
            plist.MountIds [ins_idx] = id;
            plist.Priority [ins_idx] = priority;
        }
        else
        {   // shift the other list items down by one.
            intptr_t dst  = plist.Count;
            intptr_t src  = plist.Count - 1;
            do
            {
                plist.MountIds [dst] = plist.MountIds [src];
                plist.MountData[dst] = plist.MountData[src];
                plist.Priority [dst] = plist.Priority [src];
                dst= src--;
            } while (src >= ins_idx);
            // and then insert the new item in the list.
            plist.MountIds [ins_idx] = id;
            plist.Priority [ins_idx] = priority;
            plist.Count++;
        }
        return &plist.MountData[ins_idx];
    }
    else return NULL;
}

/// @summary Removes a mount point with a given ID from a prioritized mount list.
/// @param plist The prioritized mount point list to update.
/// @param id The unique, application mount point identifier of the mount point to remove.
/// @return A pointer to the mount point state data, which may be NULL.
internal_function void* vfs_mounts_remove(vfs_mounts_t &plist, uintptr_t id)
{   // locate the index of the specified item in the ordered list.
    intptr_t  pos =-1;
    for (size_t i = 0, n = plist.Count; i < n; ++i)
    {
        if (plist.MountIds[i] == id)
        {   
            pos = intptr_t(i);
            break;
        }
    }
    if (pos >= 0)
    {   // found the desired item so shift everything up on top of it.
        size_t dst    = pos;
        size_t src    = pos + 1;
        void  *state  = plist.MountData[pos].State;
        for (size_t i = pos, n = plist.Count - 1; i < n; ++i, ++dst, ++src)
        {
            plist.MountIds [dst] = plist.MountIds [src];
            plist.MountData[dst] = plist.MountData[src];
            plist.Priority [dst] = plist.Priority [src];
        }
        plist.Count--;
        return state;
    }
    else return NULL;
}

/*////////////////////////
//   Public Functions   //
////////////////////////*/
/// @summary Initializes a new virtual file system driver.
/// @param driver The virtual file system driver to initialize.
/// @param pio The prioritized I/O driver to use for I/O operations.
/// @return ERROR_SUCCESS on success, or an error code returned by GetLastError() on failure.
public_function DWORD vfs_driver_open(vfs_driver_t *driver, pio_driver_t *pio)
{
    driver->PIO = pio;
    vfs_mounts_create(driver->Mounts, 128);
    return ERROR_SUCCESS;
}

/// @summary Closes a virtual file system driver and frees all associated resources.
/// @param driver The virtual file system driver to clean up.
public_function void vfs_driver_close(vfs_driver_t *driver)
{
    vfs_mounts_delete(driver->Mounts);
    driver->PIO = NULL;
}

public_function stream_decoder_t* vfs_load_file(vfs_driver_t *driver, char const *path, int64_t &file_size)
{
    // TODO(rlk): resolve path to a VFS mount point.
    // TODO(rlk): create the appropriate type of decoder.
    // TODO(rlk): this should be a stream-in once type of load.
}
