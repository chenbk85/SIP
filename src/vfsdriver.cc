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
/// @summary Define the maximum number of characters that can appear in a path
/// string. Since each character is two bytes, this amounts to a total of 64KB.
local_persist size_t const MAX_PATH_CHARS = (32 * 1024);

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Forward-declare types used in the vfs_mount_t function pointers.
struct vfs_file_t;                     /// Data describing an open file.
struct vfs_mount_t;                    /// Data describing a file system mount point.

/// @summary Define the recognized categories of file system mount points.
enum vfs_mount_type_e : int32_t
{
    VFS_MOUNT_TYPE_LOCAL          = 0, /// The mount point is a local device (attached SSD, etc.)
    VFS_MOUNT_TYPE_REMOTE         = 1, /// The mount point is a non-local device (mapped drive, etc.)
    VFS_MOUNT_TYPE_ARCHIVE        = 2, /// The mount point is backed by an archive file (ZIP, TAR, etc.)
    VFS_MOUNT_TYPE_COUNT          = 3  /// The number of recognized file system mount point categories.
};

/// @summary Define the recognized types of file system entries. Mainly the 
/// VFS layer cares about whether something is a file or a directory.
enum vfs_entry_type_e : int32_t
{
    VFS_ENTRY_TYPE_UNKNOWN        = 0, /// The entry type is not known or could not be determined.
    VFS_ENTRY_TYPE_FILE           = 1, /// The entry is a regular file.
    VFS_ENTRY_TYPE_DIRECTORY      = 2, /// The entry is a regular directory.
    VFS_ENTRY_TYPE_ALIAS          = 3, /// The entry is an alias to another file or directory.
    VFS_ENTRY_TYPE_IGNORE         = 4, /// The entry exists, but should be ignored.
    VFS_ENTRY_TYPE_COUNT          = 5  /// The number of recognized file system entry types.
};

/// @summary Define hint values that can be used to customize file open attributes.
enum vfs_file_hint_e  : uint32_t
{
    VFS_FILE_HINT_NONE            = (0 << 0), /// No special hints are specified.
    VFS_FILE_HINT_UNBUFFERED      = (1 << 0), /// Prefer to perform I/O directly to/from user buffers.
};

/// @summary Defines the intended usage when opening a file, which in turn defines 
/// the access and sharing modes, as well as file open flags.
enum vfs_file_usage_e : int32_t
{
    VFS_USAGE_STREAM_IN           = 0, /// The file will be opened for manual stream-in control (read-only).
    VFS_USAGE_STREAM_IN_LOAD      = 1, /// The file will be streamed into memory and then closed (read-only).
    VFS_USAGE_STREAM_OUT          = 2, /// The file will be created or overwritten (write-only). 
    VFS_USAGE_MANUAL_IO           = 3, /// The file will be opened for manual I/O.
};

/// @summary Defines status bits that may be set on opened files. These flags are
/// typically used to communicate special status from the mount point that opens 
/// the file back up to the VFS driver layer.
enum vfs_file_flags_e : uint32_t
{
    VFS_FILE_FLAG_NONE            = (0 << 0), /// No special flags are specified.
    VFS_FILE_FLAG_EXPLICIT_CLOSE  = (1 << 0), /// The returned file handle should be explicitly closed.
};

/// @summary Defines identifiers for special folder paths. 
enum vfs_known_path_e : int
{
    VFS_KNOWN_PATH_EXE_ROOT        = 0, /// The absolute path to the current executable, without filename.
    VFS_KNOWN_PATH_DOC_ROOT        = 1, /// The absolute path to the user's documents folder.
    VFS_KNOWN_PATH_HOME            = 2, /// The absolute path to the user's home directory.
};

/// @summary Define the result codes that can be returned when opening a file.
enum vfs_open_result_e : int
{
    VFS_OPEN_RESULT_SUCCESS       = 0, /// The file was opened successfully.
    VFS_OPEN_RESULT_NOT_FOUND     = 1, /// The file was not found; open failed.
    VFS_OPEN_RESULT_OSERROR       = 2, /// The file exists, but could not be opened. Check OSError. 
    VFS_OPEN_RESULT_UNSUPPORTED   = 3, /// The vfs_file_usage_e specified is not supported.
};

/// @summary Opens a file for access. Information necessary to access the file 
/// is written to the vfs_file_t structure. This function is called for each 
/// mount point, in priority order, until one is successful at opening the file
/// @param The mount point performing the file open.
/// @param The path of the file to open, relative to the mount point root.
/// @param One of vfs_file_usage_e specifying the intended use of the file.
/// @param On return, this structure should be populated with the data necessary
/// to access the file. If the file cannot be opened, set the OSError field.
/// @return One of vfs_open_result_e specifying the result of the operation.
typedef int  (*vfs_open_fn)(vfs_mount_t*, char const*, int32_t, uint32_t, vfs_file_t*);

/// @summary Unmount the mount point. This function should close any open file 
/// handles and free any resources allocated for the mount point, including the
/// memory allocated for the vfs_mount_t::State field.
/// @param The mount point being unmounted.
typedef void (*vfs_unmount_fn)(vfs_mount_t*);

/// @summary Defines the information about an open file. This information is 
/// returned by the mount point and returned to the VFS driver, which may then 
/// pass the information on to the prioritized I/O driver to perform I/O ops.
struct vfs_file_t
{
    DWORD          OSError;       /// The reason the file could not be opened.
    DWORD          AccessMode;    /// The access mode flags used to open the file handle. See CreateFile.
    DWORD          ShareMode;     /// The sharing mode flags used to open the file handle. See CreateFile.
    DWORD          OpenFlags;     /// The open mode flags used to open the file handle. See CreateFile.
    HANDLE         Fildes;        /// The system file handle.
    size_t         SectorSize;    /// The physical sector size of the disk, in bytes.
    int64_t        BaseOffset;    /// The offset of the start of the file data, in bytes.
    int64_t        BaseSize;      /// The size of the file data, in bytes, as stored.
    int64_t        FileSize;      /// The runtime size of the file data, in bytes.
    uint32_t       FileHints;     /// A combination of vfs_file_hints_e.
    uint32_t       FileFlags;     /// A combination of vfs_file_flags_e.
};

/// @summary Represents a single mounted file system within the larger VFS.
/// The file system could be represented by a local disk, a remote disk, or
/// an archive file such as a ZIP or TAR file. Mount points are maintained 
/// in a priority queue, and are created during application startup.
struct vfs_mount_t
{
    uintptr_t      Identifier;    /// The application-defined mount point identifier.
    pio_driver_t  *PIO;           /// The prioritized I/O driver interface.
    void          *State;         /// State data associated with the mount point.
    char          *Root;          /// The mount point path exposed to the VFS.
    vfs_open_fn    open;          /// Open a file handle.
    vfs_unmount_fn unmount;       /// Close file handles and free memory.
};

/// @summary Defines the internal state data associated with a filesystem mount point.
struct vfs_mount_fs_t
{
    #define N      MAX_PATH_CHARS
    WCHAR          LocalPath[N];  /// The fully-resolved absolute path of the mount point root.
    size_t         LocalPathLen;  /// The number of characters that make up the LocalPath string.
    #undef  N
};

/// @summary Defines the data associated with a dynamic list of virtual file 
/// system mount points. Mount points are ordered using a priority, which allows
/// more fine-grained control with regard to overriding. The internal structure
/// is designed such that items can be iterated over in priority order.
struct vfs_mounts_t
{
    size_t         Count;         /// The number of defined mount points.
    size_t         Capacity;      /// The number of slots of queue storage.
    uintptr_t     *MountIds;      /// The set of application-defined mount identifiers.
    vfs_mount_t   *MountData;     /// The set of mount point internal data and functions.
    uint32_t      *Priority;      /// The set of mount point priority values.
};

/// @summary Maintains all of the state for a virtual file system driver.
struct vfs_driver_t
{
    pio_driver_t  *PIO;           /// The prioritized I/O driver interface.
    vfs_mounts_t   Mounts;        /// The set of active mount points.
};

/*///////////////
//   Globals   //
///////////////*/

/*///////////////////////
//   Local Functions   //
///////////////////////*/
/// @summary Unmount a mount point and free all associated resources.
/// @param m The mount point being unmounted.
internal_function void vfs_unmount_mountpoint(vfs_mount_t *m)
{
    m->unmount(m);
    free(m->Root);
    m->State = NULL;
    m->Root  = NULL;
}

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
    if (plist.MountData != NULL)
    {   // free additional data associated with each mount point.
        for (size_t i = 0; i < plist.Count; ++i)
        {   // first unmount, and then free associated memory.
            vfs_unmount_mountpoint(&plist.MountData[i]);
        }
    }
    if (plist.Priority  != NULL) free(plist.Priority);
    if (plist.MountData != NULL) free(plist.MountData);
    if (plist.MountIds  != NULL) free(plist.MountIds);
    plist.Count     = plist.Capacity = 0;
    plist.MountIds  = NULL;
    plist.MountData = NULL;
    plist.Priority  = NULL;
}

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
internal_function void vfs_mounts_remove(vfs_mounts_t &plist, uintptr_t id)
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
    {   // found the desired item so unmount it and free up resources.
        size_t dst = pos;
        size_t src = pos + 1;
        vfs_unmount_mountpoint(&plist.MountData[pos]);
        // now shift all of the other array items up on top of the old item.
        for (size_t i = pos, n = plist.Count - 1; i < n; ++i, ++dst, ++src)
        {
            plist.MountIds [dst] = plist.MountIds [src];
            plist.MountData[dst] = plist.MountData[src];
            plist.Priority [dst] = plist.Priority [src];
        }
        plist.Count--;
    }
}

/// @summary Retrieve a system known folder path using SHGetKnownFolderPath.
/// @param buf The buffer to which the path will be copied.
/// @param buf_bytes The maximum number of bytes that can be written to buf.
/// @param bytes_needed On return, this location stores the number of bytes 
/// required to store the path string, including the zero terminator.
/// @param folder_id The Windows Shell known folder identifier.
/// @return true if the requested path was retrieved.
internal_function bool vfs_shell_folder_path(WCHAR *buf, size_t buf_bytes, size_t &bytes_needed, REFKNOWNFOLDERID folder_id)
{
    WCHAR *sysbuf = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(folder_id, KF_FLAG_NO_ALIAS, NULL, &sysbuf)))
    {   // calculate bytes needed and copy to user buffer.
        if (buf != NULL)  memset(buf, 0, buf_bytes);
        else buf_bytes =  0;
        bytes_needed   = (wcslen(sysbuf) + 1) * sizeof(WCHAR);
        if (buf_bytes >=  bytes_needed)
        {   // the path will fit, so copy it over.
            memcpy(buf, sysbuf, bytes_needed);
        }
        CoTaskMemFree(sysbuf);
        return true;
    }
    else
    {   // unable to retrieve the specified path.
        bytes_needed = 0;
        return false;
    }
}

/// @summary Retrieve a special system folder path.
/// @param buf The buffer to which the path will be copied.
/// @param buf_bytes The maximum number of bytes that can be written to buf.
/// @param bytes_needed On return, this location stores the number of bytes 
/// required to store the path string, including the zero terminator.
/// @param folder_id One of vfs_known_folder_e identifying the folder.
/// @return true if the requested path was retrieved.
internal_function bool vfs_known_path(WCHAR *buf, size_t buf_bytes, size_t &bytes_needed, int folder_id)
{
    switch (folder_id)
    {
    case VFS_KNOWN_PATH_EXE_ROOT:
        {   // zero out the path buffer, and retrieve the EXE path.
            size_t bufsiz = MAX_PATH_CHARS * sizeof(WCHAR);
            WCHAR *sysbuf = (WCHAR*) malloc(bufsiz);
            if    (sysbuf == NULL)
            {   // unable to allocate enough space for the temporary buffer.
                bytes_needed = 0;
                return false;
            }
            memset(sysbuf, 0, bufsiz);
            size_t buflen = GetModuleFileNameW(GetModuleHandle(NULL), sysbuf, MAX_PATH_CHARS * sizeof(WCHAR));
            bytes_needed  =(buflen + 1) * sizeof(WCHAR); // convert characters to bytes, add null byte
            // now strip off the name of the executable.
            WCHAR *bufitr = sysbuf + buflen - 1;
            while (bufitr > sysbuf)
            {
                if (*bufitr == L'\\' || *bufitr == L'/')
                {   // replace the trailing slash with a NULL terminator.
                    *bufitr  = 0;
                    break;
                }  --bufitr; --bytes_needed;
            }
            // finally, copy the path and directory name into the output buffer.
            if (buf != NULL) memset(buf, 0 ,  buf_bytes);
            else buf_bytes = 0;
            if (buf_bytes >= bytes_needed)
            {   // the path will fit, so copy it over.
                memcpy(buf, sysbuf, bytes_needed);
            }
            free(sysbuf);
            return true;
        }

    case VFS_KNOWN_PATH_DOC_ROOT:
        return vfs_shell_folder_path(buf, buf_bytes, bytes_needed, FOLDERID_Documents);

    case VFS_KNOWN_PATH_HOME:
        return vfs_shell_folder_path(buf, buf_bytes, bytes_needed, FOLDERID_Profile);

    default:
        break;
    }
    bytes_needed = 0;
    return false;
}

/// @summary Retrieve the physical sector size for a block-access device.
/// @param file The handle to an open file on the device.
/// @return The size of a physical sector on the specified device.
internal_function size_t vfs_physical_sector_size(HANDLE file)
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


/// @summary Opens a file for access. Information necessary to access the file 
/// is written to the vfs_file_t structure. This function is called for each 
/// mount point, in priority order, until one is successful at opening the file
/// @param m The mount point performing the file open.
/// @param path The path of the file to open, relative to the mount point root.
/// @param usage One of vfs_file_usage_e specifying the intended use of the file.
/// @param hints A combination of vfs_file_hint_e.
/// @param file On return, this structure should be populated with the data necessary
/// to access the file. If the file cannot be opened, set the OSError field.
/// @return One of vfs_open_result_e specifying the result of the operation.
internal_function int vfs_open_fs(vfs_mount_t *m, char const *path, int32_t usage, uint32_t hints, vfs_file_t *file)
{
    vfs_mount_fs_t *fs      = (vfs_mount_fs_t*) m->State;
    LARGE_INTEGER   fsize   = {0};
    HANDLE          hFile   = INVALID_HANDLE_VALUE;
    WCHAR          *pathbuf = NULL;
    DWORD           access  = 0;
    DWORD           share   = 0;
    DWORD           create  = 0;
    DWORD           flags   = 0;
    size_t          ssize   = 0;
    size_t          offset  = fs->LocalPathLen * sizeof(WCHAR);
    int             nchars  = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    int             result  = VFS_OPEN_RESULT_UNSUPPORTED;

    // figure out access flags, share modes, etc. based on the intended usage.
    switch (usage)
    {
    case VFS_USAGE_STREAM_IN:
    case VFS_USAGE_STREAM_IN_LOAD:
        access = GENERIC_READ;
        share  = FILE_SHARE_READ;
        create = OPEN_EXISTING;
        flags  = FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_OVERLAPPED;
        if (hints & VFS_FILE_HINT_UNBUFFERED) flags |= FILE_FLAG_NO_BUFFERING;
        break;

    case VFS_USAGE_STREAM_OUT:
        access = GENERIC_READ | GENERIC_WRITE;
        share  = FILE_SHARE_READ;
        create = CREATE_ALWAYS;
        flags  = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED;
        break;

    case VFS_USAGE_MANUAL_IO:
        access = GENERIC_READ | GENERIC_WRITE;
        share  = FILE_SHARE_READ;
        create = OPEN_ALWAYS;
        flags  = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_OVERLAPPED;
        if (hints & VFS_FILE_HINT_UNBUFFERED) flags |= FILE_FLAG_NO_BUFFERING;
        break;

    default:
        result = VFS_OPEN_RESULT_UNSUPPORTED;
        goto error_cleanup;
    }

    // convert the path from UTF-8 to UCS-2, which Windows requires.
    if (nchars == 0)
    {   // the path cannot be converted from UTF-8 to UCS-2.
        result = VFS_OPEN_RESULT_OSERROR;
        goto error_cleanup;
    }
    if ((pathbuf = (WCHAR*) malloc((nchars + fs->LocalPathLen + 1) * sizeof(WCHAR))) == NULL)
    {   // unable to allocate temporary memory for UCS-2 path.
        result = VFS_OPEN_RESULT_OSERROR;
        goto error_cleanup;
    }
    // start with the local root, and then append the relative path portion.
    memcpy(pathbuf, fs->LocalPath,  fs->LocalPathLen * sizeof(WCHAR));
    pathbuf[offset] = L'\\'; offset++;
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, &pathbuf[offset], nchars) == 0)
    {   // the path cannot be converted from UTF-8 to UCS-2.
        result = VFS_OPEN_RESULT_OSERROR;
        goto error_cleanup;
    }

    // open the file as specified by the caller.
    hFile = CreateFile(pathbuf, access, share, NULL, create, flags, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {   // the file cannot be opened.
        if (GetLastError() == ERROR_FILE_NOT_FOUND) result = VFS_OPEN_RESULT_NOT_FOUND;
        else result = VFS_OPEN_RESULT_OSERROR;
        goto error_cleanup;
    }

    // retrieve basic file attributes.
    ssize = vfs_physical_sector_size(hFile);
    if (!GetFileSizeEx(hFile, &fsize))
    {   // the file size cannot be retrieved.
        result = VFS_OPEN_RESULT_OSERROR;
        goto error_cleanup;
    }

    // clean up temporary buffers.
    free(pathbuf);

    // set output parameters and clean up.
    file->OSError    = ERROR_SUCCESS;
    file->AccessMode = access;
    file->ShareMode  = share;
    file->OpenFlags  = flags;
    file->Fildes     = hFile;
    file->SectorSize = ssize;
    file->BaseOffset = 0;
    file->BaseSize   = fsize.QuadPart;
    file->FileSize   = fsize.QuadPart;
    file->FileFlags  = VFS_FILE_FLAG_EXPLICIT_CLOSE;
    return VFS_OPEN_RESULT_SUCCESS;

error_cleanup:
    if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
    if (pathbuf != NULL) free(pathbuf);
    file->OSError    = GetLastError();
    file->AccessMode = access;
    file->ShareMode  = share;
    file->OpenFlags  = flags;
    file->Fildes     = INVALID_HANDLE_VALUE;
    file->SectorSize = 0;
    file->BaseOffset = 0;
    file->BaseSize   = 0;
    file->FileSize   = 0;
    file->FileFlags  = VFS_FILE_FLAG_NONE;
    return result;
}

/// @summary Unmount the mount point. This function should close any open file 
/// handles and free any resources allocated for the mount point, including the
/// memory allocated for the vfs_mount_t::State field.
/// @param m The mount point being unmounted.
internal_function void vfs_unmount_fs(vfs_mount_t *m)
{
    if (m->State != NULL)
    {   // free internal state data.
        free(m->State);
    }
    m->State   = NULL;
    m->open    = NULL;
    m->unmount = NULL;
}

/// @summary Initialize a filesystem-based mountpoint.
/// @param m The mountpoint instance to initialize.
/// @param local_path The NULL-terminated string specifying the local path that 
/// functions as the root of the mount point. This directory must exist.
/// @return true if the mount point was initialized successfully.
internal_function bool vfs_init_mount_fs(vfs_mount_t *m, WCHAR const *local_path)
{
    vfs_mount_fs_t *fs = NULL;
    if ((fs = (vfs_mount_fs_t*) malloc(sizeof(vfs_mount_fs_t))) == NULL)
    {   // unable to allocate memory for internal state.
        return false;
    }

    // we want a consistent, fully-resolved path to use as our base path.
    // use CreateFile() to open the directory, then GetFileInformationByHandleEx
    // to retrieve the fully resolved path string.
    DWORD  share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    HANDLE hdir  = CreateFile(local_path, 0, share, NULL, OPEN_EXISTING, 0, NULL);
    if (hdir == INVALID_HANDLE_VALUE)
    {   // unable to open the directory; check GetLastError().
        free(fs);
        return false;
    }
    size_t       bufsz = sizeof(DWORD) + (32 * 1024 * sizeof(WCHAR));
    FILE_NAME_INFO *ni = (FILE_NAME_INFO*) malloc(bufsz);
    ni->FileNameLength = 0;
    if (GetFileInformationByHandleEx(hdir, FileNameInfo, ni,(DWORD)bufsz) == FALSE)
    {   // unable to retrieve the file system entry name.
        CloseHandle(hdir); free(fs);
        return false;
    }
    CloseHandle(hdir);

    // copy the fully-resolved path into the LocalPath buffer. this path will 
    // be used to resolve relative filenames into absolute filenames.
    memset(fs->LocalPath, 0, sizeof(fs->LocalPath));
    memcpy(fs->LocalPath, ni->FileName, ni->FileNameLength);
    fs->LocalPathLen = ni->FileNameLength / sizeof(WCHAR);
    free(ni);

    // set the function pointers, etc. on the mount point.
    m->State   = fs;
    m->open    = vfs_open_fs;
    m->unmount = vfs_unmount_fs;
    return true;
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

/*public_function stream_decoder_t* vfs_load_file(vfs_driver_t *driver, char const *path, int64_t &file_size)
{
    // TODO(rlk): resolve path to a VFS mount point.
    // - mount has a fn ptr bool resolve_type(path, out_type).
    // TODO(rlk): create the appropriate type of decoder.
    // - for now, always just the base stream_decoder_t.
    // - set the buffer allocator to use (internal or global).
    // - looking like the VFS driver should own the global allocator and app can configure it.
    // TODO(rlk): this should be a stream-in once type of load.
}*/
