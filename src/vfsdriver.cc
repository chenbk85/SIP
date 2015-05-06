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
    VFS_MOUNT_TYPE_DIRECTORY      = 0, /// The mount point is a filesystem directory.
    VFS_MOUNT_TYPE_ARCHIVE        = 1, /// The mount point is backed by an archive file (ZIP, TAR, etc.)
    VFS_MOUNT_TYPE_COUNT          = 2  /// The number of recognized file system mount point categories.
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
    VFS_KNOWN_PATH_EXECUTABLE       = 0,      /// The absolute path to the current executable, without filename.
    VFS_KNOWN_PATH_USER_HOME        = 1,      /// The absolute path to the user's home directory.
    VFS_KNOWN_PATH_USER_DESKTOP     = 2,      /// The absolute path to the user's desktop directory.
    VFS_KNOWN_PATH_USER_DOCUMENTS   = 3,      /// The absolute path to the user's documents directory.
    VFS_KNOWN_PATH_USER_DOWNLOADS   = 4,      /// The absolute path to the user's downloads directory.
    VFS_KNOWN_PATH_USER_MUSIC       = 5,      /// The absolute path to the user's music directory.
    VFS_KNOWN_PATH_USER_PICTURES    = 6,      /// The absolute path to the user's pictures directory.
    VFS_KNOWN_PATH_USER_SAVE_GAMES  = 7,      /// The absolute path to the user's saved games directory.
    VFS_KNOWN_PATH_USER_VIDEOS      = 8,      /// The absolute path to the user's videos directory.
    VFS_KNOWN_PATH_USER_PREFERENCES = 9,      /// The absolute path to the user's local application data directory.
    VFS_KNOWN_PATH_PUBLIC_DOCUMENTS = 10,     /// The absolute path to the public documents directory.
    VFS_KNOWN_PATH_PUBLIC_DOWNLOADS = 11,     /// The absolute path to the public downloads directory.
    VFS_KNOWN_PATH_PUBLIC_MUSIC     = 12,     /// The absolute path to the public music directory.
    VFS_KNOWN_PATH_PUBLIC_PICTURES  = 13,     /// The absolute path to the public pictures directory.
    VFS_KNOWN_PATH_PUBLIC_VIDEOS    = 14,     /// The absolute path to the public videos directory.
    VFS_KNOWN_PATH_SYSTEM_FONTS     = 15,     /// The absolute path to the system fonts directory.
};

/// @summary Defines identifiers for the recognized types of stream decoders.
/// A decoder hint can be specified when opening a stream to control the type
/// of stream decoder that gets returned. 
enum vfs_decocder_hint_e : int32_t
{
    VFS_DECODER_HINT_USE_DEFAULT  = 0, /// Let the mount point decide the decoder to use.
    // ...
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
/// @param A combination of vfs_file_hint_e used to control how the file is opened.
/// @param One of vfs_decoder_hint_e specifying the type of decoder to create.
/// @param On return, this structure should be populated with the data necessary
/// to access the file. If the file cannot be opened, set the OSError field.
/// @return One of vfs_open_result_e specifying the result of the operation.
typedef int  (*vfs_open_fn)(vfs_mount_t*, char const*, int32_t, uint32_t, int32_t, vfs_file_t*);

/// @summary Unmount the mount point. This function should close any open file 
/// handles and free any resources allocated for the mount point, including the
/// memory allocated for the vfs_mount_t::State field.
/// @param The mount point being unmounted.
typedef void (*vfs_unmount_fn)(vfs_mount_t*);

/// @summary Defines the information about an open file. This information is 
/// returned by the mount point and returned to the VFS driver, which may then 
/// pass the information on to the prioritized I/O driver to perform I/O ops.
struct vfs_file_t
{   typedef stream_decoder_t          decoder_t;
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
    decoder_t     *Decoder;       /// The stream decoder to use, for usages that read data.
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
    size_t         RootLen;       /// The number of bytes that make up the mount point path.
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
    uintptr_t     *MountIds;      /// The set of application-defined unique mount point identifiers.
                                  /// Necessary because multiple mount points can be defined at the 
                                  /// same root location, but with different priorities/order.
    vfs_mount_t   *MountData;     /// The set of mount point internal data and functions.
    uint32_t      *Priority;      /// The set of mount point priority values.
};

/// @summary Maintains all of the state for a virtual file system driver.
struct vfs_driver_t
{
    pio_driver_t  *PIO;           /// The prioritized I/O driver interface.
    vfs_mounts_t   Mounts;        /// The set of active mount points.
    SRWLOCK        MountsLock;    /// The slim reader/writer lock protecting the list of mount points.
};

/*///////////////
//   Globals   //
///////////////*/

/*///////////////////////
//   Local Functions   //
///////////////////////*/
/// @summary Compare two mount point roots to determine if they match.
/// @param s1 A NULL-terminated UTF-8 string specifying the mount point identifier to match, with trailing slash '/'.
/// @param s2 A NULL-terminated UTF-8 string specifying the mount point identifier to check, with trailing slash '/'.
/// @return true if the mount point identifiers match.
internal_function inline bool vfs_mount_point_match_exact(char const *s1, char const *s2)
{   // use strcasecmp on Linux.
    return _stricmp(s1, s2) == 0;
}

/// @summary Determine whether a path references an entry under a given mount point.
/// @param mount A NULL-terminated UTF-8 string specifying the mount point root to match, with trailing slash '/'.
/// @param path A NULL-terminated UTF-8 string specifying the path to check.
/// @param len The number of bytes to compare from mount.
/// @return true if the path is under the mount point.
internal_function inline bool vfs_mount_point_match_start(char const *mount, char const *path, size_t len)
{   // use strncasecmp on Linux.
    if (len == 0) len = strlen(mount);
    return _strnicmp(mount, path, len - 1) == 0;
}

/// @summary Helper function to convert a UTF-8 encoded string to the system native WCHAR.
/// Free the returned buffer using the standard C library free() call. This function is 
/// defined in the VFS because the functionality is needed everywhere. Non-file related 
/// parts of the code should just stick to using UTF-8 where strings are required.
/// @param str The NULL-terminated UTF-8 string to convert.
/// @param size_chars On return, stores the length of the string in characters, not including NULL-terminator.
/// @param size_bytes On return, stores the length of the string in bytes, including the NULL-terminator.
/// @return The WCHAR string buffer, or NULL if the string could not be converted.
internal_function WCHAR* vfs_utf8_to_native(char const *str, size_t &size_chars, size_t &size_bytes)
{   
    // figure out how much memory needs to be allocated, including NULL terminator.
    int nchars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str, -1, NULL, 0);
    if (nchars == 0)
    {   // the path cannot be converted from UTF-8 to UCS-2.
        size_chars = 0;
        size_bytes = 0;
        return NULL;
    }
    // store output values for the caller.
    size_chars = nchars - 1;
    size_bytes = nchars * sizeof(WCHAR);
    // allocate buffer space for the wide character string.
    WCHAR *pathbuf = NULL;
    if   ((pathbuf = (WCHAR*) malloc(size_bytes)) == NULL)
    {   // unable to allocate temporary memory for UCS-2 path.
        return NULL;
    }
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str, -1, pathbuf, nchars) == 0)
    {   // the path cannot be converted from UTF-8 to UCS-2.
        free(pathbuf);
        return NULL;
    }
    return pathbuf;
}

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
            plist.MountIds[ins_idx] = id;
            plist.Priority[ins_idx] = priority;
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

/// @summary Removes a mount point at a given index within the prioritized mount point list.
/// @param plist The prioritized mount point list to update.
/// @param pos The zero-based index of the mount point to remove.
internal_function void vfs_mounts_remove_at(vfs_mounts_t &plist, intptr_t pos)
{
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

/// @summary Removes a mount point with a given ID from a prioritized mount list.
/// @param plist The prioritized mount point list to update.
/// @param mount_id The unique application identifier of the mount point to remove.
internal_function void vfs_mounts_remove(vfs_mounts_t &plist, uintptr_t mount_id)
{   // locate the index of the specified item in the ordered list.
    for (size_t i = 0, n = plist.Count; i < n; ++i)
    {
        if (plist.MountIds[i] == mount_id)
        {
            vfs_mounts_remove_at(plist, intptr_t(i));
            break;
        }
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
    case VFS_KNOWN_PATH_EXECUTABLE:
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
                }
                --bytes_needed;
                --bufitr; 
            }
            // finally, copy the path and directory name into the output buffer.
            if  (buf != NULL) memset(buf, 0 ,  buf_bytes);
            else buf_bytes  = 0;
            if  (buf_bytes >= bytes_needed)
            {   // the path will fit, so copy it over.
                memcpy(buf, sysbuf, bytes_needed);
            }
            free(sysbuf);
            return true;
        }
    case VFS_KNOWN_PATH_USER_HOME:
        return vfs_shell_folder_path(buf, buf_bytes, bytes_needed, FOLDERID_Profile);
    case VFS_KNOWN_PATH_USER_DESKTOP:
        return vfs_shell_folder_path(buf, buf_bytes, bytes_needed, FOLDERID_Desktop);
    case VFS_KNOWN_PATH_USER_DOCUMENTS:
        return vfs_shell_folder_path(buf, buf_bytes, bytes_needed, FOLDERID_Documents);
    case VFS_KNOWN_PATH_USER_DOWNLOADS:
        return vfs_shell_folder_path(buf, buf_bytes, bytes_needed, FOLDERID_Downloads);
    case VFS_KNOWN_PATH_USER_MUSIC:
        return vfs_shell_folder_path(buf, buf_bytes, bytes_needed, FOLDERID_Music);
    case VFS_KNOWN_PATH_USER_PICTURES:
        return vfs_shell_folder_path(buf, buf_bytes, bytes_needed, FOLDERID_Pictures);
    case VFS_KNOWN_PATH_USER_SAVE_GAMES:
        return vfs_shell_folder_path(buf, buf_bytes, bytes_needed, FOLDERID_SavedGames);
    case VFS_KNOWN_PATH_USER_VIDEOS:
        return vfs_shell_folder_path(buf, buf_bytes, bytes_needed, FOLDERID_Videos);
    case VFS_KNOWN_PATH_USER_PREFERENCES:
        return vfs_shell_folder_path(buf, buf_bytes, bytes_needed, FOLDERID_LocalAppData);
    case VFS_KNOWN_PATH_PUBLIC_DOCUMENTS:
        return vfs_shell_folder_path(buf, buf_bytes, bytes_needed, FOLDERID_PublicDocuments);
    case VFS_KNOWN_PATH_PUBLIC_DOWNLOADS:
        return vfs_shell_folder_path(buf, buf_bytes, bytes_needed, FOLDERID_PublicDownloads);
    case VFS_KNOWN_PATH_PUBLIC_MUSIC:
        return vfs_shell_folder_path(buf, buf_bytes, bytes_needed, FOLDERID_PublicMusic);
    case VFS_KNOWN_PATH_PUBLIC_PICTURES:
        return vfs_shell_folder_path(buf, buf_bytes, bytes_needed, FOLDERID_PublicPictures);
    case VFS_KNOWN_PATH_PUBLIC_VIDEOS:
        return vfs_shell_folder_path(buf, buf_bytes, bytes_needed, FOLDERID_PublicVideos);
    case VFS_KNOWN_PATH_SYSTEM_FONTS:
        return vfs_shell_folder_path(buf, buf_bytes, bytes_needed, FOLDERID_Fonts);

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

/// @summary Factory function to instantiate a stream decoder for a given usage.
/// @param usage One of vfs_file_usage_e specifying the indended use of the file.
/// @param decoder_hint One of vfs_decoder_hint_e specifying the preferred decoder 
/// type, or VFS_DECODER_HINT_USE_DEFAULT to let the implementation decide.
/// @return A stream decoder instance, or NULL if the intended usage does not 
/// require or support stream decoding.
internal_function stream_decoder_t* vfs_create_decoder(int32_t usage, int32_t decoder_hint)
{
    if (usage != VFS_USAGE_STREAM_IN      && 
        usage != VFS_USAGE_STREAM_IN_LOAD &&
        usage != VFS_USAGE_MANUAL_IO)
    {   // this usage does not need a decoder as reads are disallowed.
        return NULL;
    }

    switch (decoder_hint)
    {
    case VFS_DECODER_HINT_USE_DEFAULT:
        break;
    default:
        break;
    }
    return new stream_decoder_t();
}

/// @summary Opens a file for access. Information necessary to access the file 
/// is written to the vfs_file_t structure. This function is called for each 
/// mount point, in priority order, until one is successful at opening the file
/// @param m The mount point performing the file open.
/// @param path The path of the file to open, relative to the mount point root.
/// @param usage One of vfs_file_usage_e specifying the intended use of the file.
/// @param file_hints A combination of vfs_file_hint_e.
/// @param decoder_hint One of vfs_decoder_hint_e specifying the preferred decoder 
/// type, or VFS_DECODER_HINT_USE_DEFAULT to let the implementation decide.
/// @param file On return, this structure should be populated with the data necessary
/// to access the file. If the file cannot be opened, set the OSError field.
/// @return One of vfs_open_result_e specifying the result of the operation.
internal_function int vfs_open_fs(vfs_mount_t *m, char const *path, int32_t usage, uint32_t file_hints, int32_t decoder_hint, vfs_file_t *file)
{
    vfs_mount_fs_t *fs      = (vfs_mount_fs_t*)m->State;
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
        if (file_hints & VFS_FILE_HINT_UNBUFFERED) flags |= FILE_FLAG_NO_BUFFERING;
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
        if (file_hints & VFS_FILE_HINT_UNBUFFERED) flags |= FILE_FLAG_NO_BUFFERING;
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
    file->Decoder    = vfs_create_decoder(usage, decoder_hint);
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
    file->Decoder    = NULL;
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
    HANDLE hdir  = CreateFile(local_path, 0, share, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (hdir == INVALID_HANDLE_VALUE)
    {   // unable to open the directory; check GetLastError().
        free(fs);
        return false;
    }
    DWORD  flags     = FILE_NAME_NORMALIZED | VOLUME_NAME_DOS;
    fs->LocalPathLen = GetFinalPathNameByHandle(hdir, fs->LocalPath, MAX_PATH_CHARS, flags);
    CloseHandle(hdir);

    // set the function pointers, etc. on the mount point.
    m->State   = fs;
    m->open    = vfs_open_fs;
    m->unmount = vfs_unmount_fs;
    return true;
}

/// @summary Performs all of the common setup when creating a new mount point.
/// @param driver The VFS driver instance the mount point is attached to.
/// @param source_wide The NULL-terminated source directory or file path.
/// @param source_attr The attributes of the source path, as returned by GetFileAttributes().
/// @param mount_path The NULL-terminated UTF-8 string specifying the mount point root exposed externally.
/// @param priority The priority of the mount point. Higher numeric values indicate higher priority.
/// @param mount_id An application-defined unique identifier for the mount point. This is necessary
/// if the application later wants to remove this specific mount point, but not any of the others 
/// that are mounted with the same mount_path.
/// @return true if the mount point was successfully installed.
internal_function bool vfs_setup_mount(vfs_driver_t *driver, WCHAR *source_wide, DWORD source_attr, char const *mount_path, uint32_t priority, uintptr_t mount_id)
{   
    vfs_mount_t *mount        = NULL;
    size_t       mount_bytes  = strlen(mount_path);
    size_t       source_chars = wcslen(source_wide);
    
    // create the mount point record in the prioritized list.
    AcquireSRWLockExclusive(&driver->MountsLock);
    if ((mount = vfs_mounts_insert(driver->Mounts, mount_id, priority)) == NULL)
    {   // unable to insert the mount point in the prioritized list.
        ReleaseSRWLockExclusive(&driver->MountsLock);
        return false;
    }

    // initialize the basic mount point properties:
    mount->Identifier = mount_id;
    mount->PIO        = driver->PIO;
    mount->Root       = NULL;
    mount->RootLen    = mount_bytes;

    // create version of mount_path with trailing slash.
    if (mount_bytes && mount_path[mount_bytes-1] != '/')
    {   // it's necessary to append the trailing slash.
        char  *r  = (char*)   malloc(mount_bytes  + 2);
        memcpy(r, mount_path, mount_bytes);
        r[mount_bytes+0] = '/';
        r[mount_bytes+1] =  0 ;
        mount->Root = r;
    }
    else
    {   // the mount path has the trailing slash; copy the string.
        char  *r  = (char*)   malloc(mount_bytes  + 1);
        memcpy(r, mount_path, mount_bytes);
        r[mount_bytes] = 0;
        mount->Root = r;
    }

    // initialize the mount point internals based on the source type.
    if (source_attr & FILE_ATTRIBUTE_DIRECTORY)
    {   // set up a filesystem-based mount point.
        if (!vfs_init_mount_fs(mount, source_wide))
        {   // remove the item we just added and clean up.
            vfs_mounts_remove_at(driver->Mounts, driver->Mounts.Count-1);
            ReleaseSRWLockExclusive(&driver->MountsLock);
            free(mount->Root); mount->Root = NULL;
            return false;
        }
        ReleaseSRWLockExclusive(&driver->MountsLock);
        return true;
    }
    else
    {   // this is an archive file. extract the file extension.
        WCHAR *ext = source_wide + source_chars;
        while (ext > source_wide)
        {   // search for the final period character.
            if (*ext == L'.')
            {   // ext points to the first character of the extension.
                ext++;
                break;
            }
        }
        // now a bunch of if (wcsicmp(ext, L"zip") { ... } etc.
        // TODO(rlk): need to make sure to release MountsLock.
        ReleaseSRWLockExclusive(&driver->MountsLock);
        return false;
    }
}

internal_function int vfs_resolve_and_open_file(vfs_driver_t *driver, char const *path, int32_t usage, uint32_t file_hints, int32_t decoder_hint, vfs_file_t *file, char const **relative_path)
{   // iterate over the mount points, which are stored in priority order.
    AcquireSRWLockShared(&driver->MountsLock);
    int          result  = VFS_OPEN_RESULT_NOT_FOUND;
    vfs_mount_t *mounts  = driver->Mounts.MountData;
    for (size_t i = 0; i < driver->Mounts.Count; ++i)
    {
        vfs_mount_t  *m  = &mounts[i];
        if (vfs_mount_point_match_start(m->Root, path, m->RootLen))
        {   // attempt to open the file on the mount point.
            if ((result = m->open(m, path + m->RootLen, usage, file_hints, decoder_hint, file)) == VFS_OPEN_RESULT_SUCCESS)
            {   // the file was successfully opened, so we're done.
                if (relative_path != NULL)
                {
                    *relative_path = path + m->RootLen;
                }
                ReleaseSRWLockShared(&driver->MountsLock);
                return VFS_OPEN_RESULT_SUCCESS;
            }
        }
    }
    ReleaseSRWLockShared(&driver->MountsLock);
    return result;
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
    InitializeSRWLock(&driver->MountsLock);
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

/// @summary Creates a mount point backed by a well-known directory.
/// @param driver The virtual file system driver.
/// @param folder_id One of vfs_known_path_e specifying the well-known directory.
/// @param mount_path The NULL-terminated UTF-8 string specifying the root of the 
/// mount point as exposed to the application code. Multiple mount points may share
/// the same mount_path, but have different source_path, mount_id and priority values.
/// @param priority The priority assigned to this specific mount point, with higher 
/// numeric values corresponding to higher priority values.
/// @param mount_id A unique application-defined identifier for the mount point. This
/// identifier can be used to reference the specific mount point.
/// @return true if the mount point was successfully created.
public_function bool vfs_mount(vfs_driver_t *driver, int folder_id, char const *mount_path, uint32_t priority, uintptr_t mount_id)
{   // retrieve the path specified by folder_id for use as the source path.
    size_t buffer_bytes = MAX_PATH_CHARS * sizeof(WCHAR);
    size_t source_bytes = 0;
    DWORD  source_attr  = FILE_ATTRIBUTE_DIRECTORY;
    WCHAR  source_wide[MAX_PATH_CHARS]; // 64KB - probably overkill
    if (!vfs_known_path(source_wide, buffer_bytes, source_bytes, folder_id))
    {   // unrecognized folder_id.
        return false;
    }
    if (!vfs_setup_mount(driver, source_wide, source_attr, mount_path, priority, mount_id))
    {   // unable to finish internal setup of the mount point.
        return false;
    }
    return true;
}

/// @summary Creates a mount point backed by the specified archive file or directory.
/// @param driver The virtual file system driver.
/// @param source_path The NULL-terminated UTF-8 string specifying the path of the 
/// file or directory that represents the root of the mount point.
/// @param mount_path The NULL-terminated UTF-8 string specifying the root of the 
/// mount point as exposed to the application code. Multiple mount points may share
/// the same mount_path, but have different source_path, mount_id and priority values.
/// @param priority The priority assigned to this specific mount point, with higher 
/// numeric values corresponding to higher priority values.
/// @param mount_id A unique application-defined identifier for the mount point. This
/// identifier can be used to reference the specific mount point.
/// @return true if the mount point was successfully created.
public_function bool vfs_mount(vfs_driver_t *driver, char const *source_path, char const *mount_path, uint32_t priority, uintptr_t mount_id)
{   // convert the source path to a wide character string once during mounting.
    size_t source_chars = 0;
    size_t source_bytes = 0;
    DWORD  source_attr  = 0;
    WCHAR *source_wide  = NULL;
    if   ((source_wide  = vfs_utf8_to_native(source_path, source_chars, source_bytes)) == NULL)
    {   // unable to convert UTF-8 to wide character string.
        return false;
    }

    // determine whether the source path is a directory or a file.
    // TODO(rlk): GetFileAttributes() won't work (GetLastError() => ERROR_BAD_NETPATH)
    // in that case that source_path is the root of a network share. maybe fix this.
    if ((source_attr = GetFileAttributesW(source_wide)) == INVALID_FILE_ATTRIBUTES)
    {   // the source path must exist. mount fails.
        free(source_wide);
        return false;
    }
    if (!vfs_setup_mount(driver, source_wide, source_attr, mount_path, priority, mount_id))
    {   // unable to finish internal setup of the mount point.
        free(source_wide);
        return false;
    }
    return true;
}


/// @summary Removes a mount point associated with a specific application mount point identifier.
/// @param driver The virtual file system driver.
/// @param mount_id The application-defined mount point identifer, as supplied to vfs_mount().
public_function void vfs_unmount(vfs_driver_t *driver, uintptr_t mount_id)
{
    AcquireSRWLockExclusive(&driver->MountsLock);
    vfs_mounts_remove(driver->Mounts, mount_id);
    ReleaseSRWLockExclusive(&driver->MountsLock);
}

/// @summary Deletes all mount points attached to a given root path.
/// @param driver The virtual file system driver.
/// @param mount_path A NULL-terminated UTF-8 string specifying the mount point root, 
/// as was supplied to vfs_mount(). All mount points that were mounted to this path will be deleted.
public_function void vfs_unmount_all(vfs_driver_t *driver, char const *mount_path)
{   // create version of mount_path with trailing slash.
    size_t len        = strlen(mount_path);
    char  *mount_root = (char*)mount_path;
    if (len > 0 && mount_path[len-1]  != '/')
    {   // allocate a temporary buffer on the stack.
        mount_root = (char*)alloca(len + 2);
        memcpy(mount_root , mount_path , len);
        mount_root[len]   = '/';
        mount_root[len+1] = 0;
    }
    // search for matches and remove items in reverse.
    // this minimizes data movement and simplifies logic.
    AcquireSRWLockExclusive(&driver->MountsLock);
    for (intptr_t i = driver->Mounts.Count - 1; i >= 0; --i)
    {
        if (vfs_mount_point_match_exact(driver->Mounts.MountData[i].Root, mount_root))
        {   // the mount point paths match, so remove this item.
            vfs_mounts_remove_at(driver->Mounts, i);
        }
    }
    ReleaseSRWLockExclusive(&driver->MountsLock);
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
    // TODO(rlk): in addition to returning a stream_decoder_t, also return a stream_control_t.
}*/
