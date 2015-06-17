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
local_persist size_t const MAX_PATH_CHARS        = (32  * 1024);

/// @summary Define the size of the stream buffer allocated by the VFS driver.
/// This buffer is divided into fixed-size chunks.
local_persist size_t const STREAM_BUFFER_SIZE    = (16  * 1024 * 1024); // 16MB

/// @summary Define the size of a single chunk used for streaming data into 
/// memory from disk. Streaming data out to disk uses a different chunk size.
local_persist size_t const STREAM_IN_CHUNK_SIZE  = (128 * 1024);        // 256KB

/// @summary Define the size of a single chunk used for streaming data out 
/// from memory to disk. This must be a multiple of the system page size.
local_persist size_t const STREAM_OUT_CHUNK_SIZE = (64  * 1024);        // 64KB

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
    VFS_FILE_HINT_ASYNCHRONOUS    = (1 << 1), /// All I/O operations will be performed asynchronously.
    VFS_FILE_HINT_TRUNCATE        = (1 << 2), /// If the file exists, existing contents are overwritten.
};

/// @summary Defines the intended usage when opening a file, which in turn defines 
/// the access and sharing modes, as well as file open flags.
enum vfs_file_usage_e : int32_t
{
    VFS_USAGE_STREAM_IN           = 0, /// The file will be opened for manual stream-in control (read-only).
    VFS_USAGE_STREAM_IN_LOAD      = 1, /// The file will be streamed into memory and then closed (read-only).
    VFS_USAGE_STREAM_OUT          = 2, /// The file will be created or overwritten (write-only). 
    VFS_USAGE_MANUAL_IO           = 3, /// The file will be opened for manual I/O (read-write).
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
    VFS_DECODER_HINT_NONE         = 1, /// Don't create any stream decoder.
    // ...
};

/// @summary Defines the recognized types of entries in a tarball. 
/// Vendor extensions may additionally have a type 'A'-'Z'.
enum tar_entry_type_e : char
{
    TAR_ENTRY_TYPE_FILE      ='0',/// The entry represents a regular file.
    TAR_ENTRY_TYPE_HARDLINK  ='1',/// The entry represents a hard link.
    TAR_ENTRY_TYPE_SYMLINK   ='2',/// The entry represents a symbolic link.
    TAR_ENTRY_TYPE_CHARACTER ='3',/// The entry represents a character device file.
    TAR_ENTRY_TYPE_BLOCK     ='4',/// The entry represents a block device file.
    TAR_ENTRY_TYPE_DIRECTORY ='5',/// The entry represents a directory.
    TAR_ENTRY_TYPE_FIFO      ='6',/// The entry represents a named pipe file.
    TAR_ENTRY_TYPE_CONTIGUOUS='7',/// The entry represents a contiguous file.
    TAR_ENTRY_TYPE_GMETA     ='g',/// The entry is a global extended header with metadata.
    TAR_ENTRY_TYPE_XMETA     ='x' /// The entry is an extended header with metadata for the next file.
};

#pragma pack(push, 1)
/// @summary Defines the fields of the base TAR header as they exist in the file.
struct tar_header_encoded_t
{
    char           FileName[100]; /// The filename, with no path information.
    char           FileMode[8];   /// The octal file mode value.
    char           OwnerUID[8];   /// The octal owner user ID value.
    char           GroupUID[8];   /// The octal group ID value.
    char           FileSize[12];  /// The octal file size, in bytes.
    char           FileTime[12];  /// The octal UNIX file time of last modification.
    char           Checksum[8];   /// The octal checksum value of the file.
    char           FileType;      /// One of the values of the tar_entry_type_e enumeration.
    char           LinkName[100]; /// The filename of the linked file, with no path information.
    char           ExtraPad[255]; /// Additional unused data, or, possibly, a ustar_header_encoded_t.
};

/// @summary Defines the fields of the extended USTAR header as they exist in the file.
struct ustar_header_encoded_t
{
    char           MagicStr[6];   /// The string "ustar\0".
    char           Version [2];   /// The version number, terminated with a space.
    char           OwnerStr[32];  /// The ASCII name of the file owner.
    char           GroupStr[32];  /// The ASCII name of the file group.
    char           DevMajor[8];   /// The octal device major number.
    char           DevMinor[8];   /// The octal device minor number.
    char           FileBase[155]; /// The prefix value for the FileName, with no trailing slash.
};
#pragma pack(pop)

/// @summary Defines the set of information maintained at runtime for each entry in a tarball.
struct tar_entry_t
{
    int64_t        FileSize;      /// The size of the file, in bytes.
    uint64_t       FileTime;      /// The last write time of the file, as a UNIX timestamp.
    int64_t        DataOffset;    /// The absolute byte offset to the start of the file data.
    uint32_t       Checksum;      /// The checksum of the file data.
    uint32_t       Reserved;      /// Reserved for future use. Set to 0.
    char           FileType;      /// One of the values of the tar_entry_type_e enumeration.
    char           FullPath[257]; /// The full path of the file, "<FileBase>/<FileName>\0".
    char           LinkName[101]; /// The filename of the linked file, zero-terminated.
    char           Padding;       /// One extra byte of padding. Set to 0.
};

/// @summary Synchronously opens a file for access. Information necessary to 
/// access the file is written to the vfs_file_t structure. This function is
/// called for each mount point, in priority order, until one either returns 
/// ERROR_SUCCESS or an error code other than ERROR_NOT_SUPPORTED.
/// @param The mount point performing the file open.
/// @param The path of the file to open, relative to the mount point root.
/// @param One of vfs_file_usage_e specifying the intended use of the file.
/// @param A combination of vfs_file_hint_e used to control how the file is opened.
/// @param One of vfs_decoder_hint_e specifying the preferred decoder type,
/// or VFS_DECODER_HINT_USE_DEFAULT to let the implementation decide.
/// @param On return, this structure should be populated with the data necessary
/// to access the file. If the file cannot be opened, set the OSError field.
/// @return ERROR_SUCCESS, ERROR_NOT_SUPPORTED or the OS error code.
typedef DWORD (*vfs_open_fn)(vfs_mount_t*, char const*, int32_t, uint32_t, int32_t, vfs_file_t*);

/// @summary Atomically and synchronously writes a file. If the file exists, its
/// contents are overwritten; otherwise a new file is created. This operation is
/// generally not supported by archive-based mount points.
/// @param The mount point performing the file save.
/// @param The path of the file to save, relative to the mount point root.
/// @param The buffer containing the data to write to the file.
/// @param The number of bytes to copy from the buffer into the file.
/// @return ERROR_SUCCESS, ERROR_NOT_SUPPORTED or the OS error code.
typedef DWORD (*vfs_save_fn)(vfs_mount_t*, char const*, void const*, int64_t);

/// @summary Queries a mount point to determine whether it supports the specified usage.
/// @param The mount point being queried.
/// @param One of vfs_file_usage_e specifying the intended usage of the file.
/// @param One of vfs_decoder_hint_t specifying the type of decoder desired.
/// @return Either ERROR_SUCCESS or ERROR_NOT_SUPPORTED.
typedef DWORD (*vfs_support_fn)(vfs_mount_t*, int32_t, int32_t);

/// @summary Unmount the mount point. This function should close any open file 
/// handles and free any resources allocated for the mount point, including the
/// memory allocated for the vfs_mount_t::State field.
/// @param The mount point being unmounted.
typedef void  (*vfs_unmount_fn)(vfs_mount_t*);

/// @summary Provides an interface to control playback of a stream.
struct stream_control_t
{   typedef pio_sti_control_alloc_t     pio_thread_alloc_t;

    stream_control_t(void);         /// Construct unattached stream controller.
    ~stream_control_t(void);        /// Release decoder reference.

    void pause(void);               /// Pause reading of the stream, but don't close the file.
    void resume(void);              /// Resume reading of a paused stream.
    void rewind(void);              /// Resume reading the stream from the beginning.
    void seek(int64_t offset);      /// Seek to a specified location and resume streaming.
    void stop(void);                /// Stop streaming the file and close it.

    uintptr_t           SID;        /// The application-defined stream identifier.
    pio_driver_t       *PIO;        /// The prioritized I/O driver interface.
    pio_thread_alloc_t *PIOAlloc;   /// The allocator used when submitting stream control commands to the PIO driver.
    int64_t             EncodedSize;/// The number of bytes of data as stored.
    int64_t             DecodedSize;/// The number of bytes of data when decoded, if known.
};

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
    vfs_save_fn    save;          /// Atomically save a file.
    vfs_unmount_fn unmount;       /// Close file handles and free memory.
    vfs_support_fn supports;      /// Test mount point feature support.
};

/// @summary Defines the internal state data associated with a filesystem mount point.
struct vfs_mount_fs_t
{
    #define N      MAX_PATH_CHARS
    size_t         LocalPathLen;  /// The number of characters that make up the LocalPath string.
    WCHAR          LocalPath[N];  /// The fully-resolved absolute path of the mount point root.
    #undef  N
};

/// @summary Defines the internal state data associated with a tarball mount point.
struct vfs_mount_tarball_t
{
    #define N      MAX_PATH_CHARS
    HANDLE         TarFildes;     /// The file descriptor for the tarball. Duplicated for each opened file.
    size_t         SectorSize;    /// The physical disk sector size, in bytes.
    size_t         EntryCount;    /// The number of regular file entries defined in the tarball.
    size_t         EntryCapacity; /// The maximum number of entries that can be stored in the EntryList.
    uint32_t      *EntryHash;     /// The set of hashes of entry filenames.
    tar_entry_t   *EntryInfo;     /// The set of entries defined in the tarball; no data.
    size_t         LocalPathLen;  /// The number of characters that make up the LocalPath string.
    WCHAR          LocalPath[N];  /// The fully-resolved absolute path of the TAR file.
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
{   typedef io_buffer_allocator_t     iobuf_alloc_t;
    aio_driver_t  *AIO;           /// The asynchronous I/O driver interface.
    pio_driver_t  *PIO;           /// The prioritized I/O driver interface.
    vfs_mounts_t   Mounts;        /// The set of active mount points.
    SRWLOCK        MountsLock;    /// The slim reader/writer lock protecting the list of mount points.
    iobuf_alloc_t  StreamBuffer;  /// The global buffer used for streaming files into memory.
};

/*///////////////
//   Globals   //
///////////////*/

/*///////////////////////
//   Local Functions   //
///////////////////////*/
/// @summary Constructs an unattached stream control interface.
stream_control_t::stream_control_t(void)
    : 
    SID(0), 
    PIO(NULL), 
    PIOAlloc(NULL),
    EncodedSize(0), 
    DecodedSize(0)
{
    /* empty */
}

/// @summary Releases references held by the stream control interface.
stream_control_t::~stream_control_t(void)
{
    SID      = 0;
    PIO      = NULL;
    PIOAlloc = NULL;
}

/// @summary Pauses reading of the stream.
void stream_control_t::pause(void)
{
    pio_driver_pause_stream(PIO, SID, PIOAlloc);
}

/// @summary Resumes reading of the stream from the pause position.
void stream_control_t::resume(void)
{
    pio_driver_resume_stream(PIO, SID, PIOAlloc);
}

/// @summary Resumes reading of the stream from the beginning.
void stream_control_t::rewind(void)
{
    pio_driver_resume_stream(PIO, SID, PIOAlloc);
}

/// @summary Resumes reading of the stream from the specified byte offset.
/// @param offset The absolute byte offset within the stream. The offset 
/// will be rounded down to the nearest even multiple of the physical 
/// disk sector size.
void stream_control_t::seek(int64_t offset)
{
    pio_driver_seek_stream(PIO, SID, offset, PIOAlloc);
}

/// @summary Stops reading the stream and closes the underlying file handle.
void stream_control_t::stop(void)
{
    pio_driver_stop_stream(PIO, SID, PIOAlloc);
}

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

/// @summary Helper function to convert a UTF-8 encoded string to the system native WCHAR. Free the returned buffer using the standard C library free() call. This function is defined in the VFS because the functionality is needed everywhere. Non-file related parts of the code should just stick to using UTF-8 where strings are required.
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
        intptr_t ins_idx  = 0;
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
            else if (priority > p_b)
            {   // continue searching the lower half of the range.
                max_idx = mid - 1;
            }
            else
            {   // continue searching the upper half of the range.
                min_idx = mid + 1;
            }
        }
        if (ins_idx == plist.Count)
        {   // insert the item at the end of the list.
            ins_idx =  plist.Count++;
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
/// @param bytes_needed On return, this location stores the number of bytes required to store the path string, including the zero terminator.
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
        if (buf_bytes >= (bytes_needed + 1))
        {   // the path will fit, so copy it over.
            memcpy(buf, sysbuf, bytes_needed);
            buf[bytes_needed] = 0;
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
/// @param bytes_needed On return, this location stores the number of bytes required to store the path string, including the zero terminator.
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
    BOOL result = DeviceIoControl(file, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), &desc , sizeof(desc), &bytes, NULL);
    return result ? desc.BytesPerPhysicalSector : DefaultPhysicalSectorSize;
}

/// @summary Factory function to instantiate a stream decoder for a given usage.
/// @param usage One of vfs_file_usage_e specifying the indended use of the file.
/// @param decoder_hint One of vfs_decoder_hint_e specifying the preferred decoder type, or VFS_DECODER_HINT_USE_DEFAULT to let the implementation decide.
/// @return A stream decoder instance, or NULL if the intended usage does not require or support stream decoding. The reference count of the stream decoder is zero; it is up to higher-level code to manage the reference count.
internal_function stream_decoder_t* vfs_create_decoder(int32_t usage, int32_t decoder_hint)
{
    if (usage != VFS_USAGE_STREAM_IN      && 
        usage != VFS_USAGE_STREAM_IN_LOAD &&
        usage != VFS_USAGE_MANUAL_IO)
    {   // this usage does not need a decoder as reads are disallowed.
        return NULL;
    }

    // TODO(rlk): eventually there will be several different decoder types.
    // the caller should be able to explicitly request any one of them.
    switch (decoder_hint)
    {
    case VFS_DECODER_HINT_USE_DEFAULT:
        break;
    default:
        break;
    }
    return new stream_decoder_t();
}

/// @summary Generate a Windows API-ready file path for a given filesystem mount.
/// @param fs Internal data for the filesystem mount, specifying the mount root.
/// @param relative_path The portion of the virtual_path specified relative to the mount point.
/// @return The API-ready system path, or NULL. Free the returned buffer using free().
internal_function WCHAR* vfs_make_system_path_fs(vfs_mount_fs_t *fs, char const *relative_path)
{
    WCHAR  *pathbuf = NULL;
    size_t  offset  = fs->LocalPathLen;
    int     nchars  = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, relative_path, -1, NULL, 0);
    // convert the virtual_path from UTF-8 to UCS-2, which Windows requires.
    if (nchars == 0)
    {   // the path cannot be converted from UTF-8 to UCS-2.
        return NULL;
    }
    if ((pathbuf = (WCHAR*) malloc((nchars + fs->LocalPathLen + 1) * sizeof(WCHAR))) == NULL)
    {   // unable to allocate memory for UCS-2 path.
        return NULL;
    }
    // start with the local root, and then append the relative path portion.
    memcpy(pathbuf, fs->LocalPath, fs->LocalPathLen * sizeof(WCHAR));
    pathbuf[fs->LocalPathLen] = L'\\';
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, relative_path, -1, &pathbuf[fs->LocalPathLen+1], nchars) == 0)
    {   // the path cannot be converted from UTF-8 to UCS-2.
        free(pathbuf);
        return NULL;
    }
    // normalize the directory separator characters to the system default.
    // on Windows, this is not strictly necessary, but on other OS's it is.
    for (size_t i = offset + 1, n = offset + nchars; i < n; ++i)
    {
        if (pathbuf[i] == L'/')
            pathbuf[i]  = L'\\';
    }
    return pathbuf;
}

/// @summary Synchronously opens a file for access. Information necessary to access the file is written to the vfs_file_t structure. This function is called for each mount point, in priority order, until one either returns ERROR_SUCCESS or an error code other than ERROR_NOT_SUPPORTED.
/// @param m The mount point performing the file open.
/// @param path The path of the file to open, relative to the mount point root.
/// @param usage One of vfs_file_usage_e specifying the intended use of the file.
/// @param file_hints A combination of vfs_file_hint_e used to control how the file is opened.
/// @param decoder_hint One of vfs_decoder_hint_e specifying the preferred decoder type, or VFS_DECODER_HINT_USE_DEFAULT to let the implementation decide.
/// @param file On return, this structure should be populated with the data necessary to access the file. If the file cannot be opened, set the OSError field.
/// @return ERROR_SUCCESS, ERROR_NOT_SUPPORTED or the OS error code.
internal_function DWORD vfs_open_fs(vfs_mount_t *m, char const *path, int32_t usage, uint32_t file_hints, int32_t decoder_hint, vfs_file_t *file)
{
    vfs_mount_fs_t *fs      =(vfs_mount_fs_t*)   m->State;
    LARGE_INTEGER   fsize   = {0};
    WCHAR          *pathbuf = vfs_make_system_path_fs(fs, path);
    HANDLE          hFile   = INVALID_HANDLE_VALUE;
    DWORD           result  = ERROR_NOT_SUPPORTED;
    DWORD           access  = 0;
    DWORD           share   = 0;
    DWORD           create  = 0;
    DWORD           flags   = 0;
    size_t          ssize   = 0;

    // figure out access flags, share modes, etc. based on the intended usage.
    switch (usage)
    {
    case VFS_USAGE_STREAM_IN:
    case VFS_USAGE_STREAM_IN_LOAD:
        access = GENERIC_READ;
        share  = FILE_SHARE_READ;
        create = OPEN_EXISTING;
        flags  = FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_OVERLAPPED;
        if (file_hints & VFS_FILE_HINT_UNBUFFERED)   flags |= FILE_FLAG_NO_BUFFERING;
        break;

    case VFS_USAGE_STREAM_OUT:
        access = GENERIC_READ | GENERIC_WRITE;
        share  = FILE_SHARE_READ;
        create = CREATE_ALWAYS;
        flags  = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_OVERLAPPED;
        if (file_hints & VFS_FILE_HINT_UNBUFFERED)   flags |= FILE_FLAG_NO_BUFFERING;
        break;

    case VFS_USAGE_MANUAL_IO:
        access = GENERIC_READ | GENERIC_WRITE;
        share  = FILE_SHARE_READ;
        create = OPEN_ALWAYS;
        flags  = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN;
        if (file_hints & VFS_FILE_HINT_UNBUFFERED)   flags |= FILE_FLAG_NO_BUFFERING;
        if (file_hints & VFS_FILE_HINT_ASYNCHRONOUS) flags |= FILE_FLAG_OVERLAPPED;
        if (file_hints & VFS_FILE_HINT_TRUNCATE)     create = CREATE_ALWAYS;
        break;

    default:
        result = ERROR_NOT_SUPPORTED;
        goto error_cleanup;
    }

    // open the file as specified by the caller.
    hFile = CreateFile(pathbuf, access, share, NULL, create, flags, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {   // the file cannot be opened.
        result = GetLastError();
        goto error_cleanup;
    }

    // retrieve basic file attributes.
    ssize = vfs_physical_sector_size(hFile);
    if (!GetFileSizeEx(hFile, &fsize))
    {   // the file size cannot be retrieved.
        result = GetLastError();
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
    file->FileHints  = file_hints;
    file->FileFlags  = VFS_FILE_FLAG_EXPLICIT_CLOSE;
    file->Decoder    = vfs_create_decoder(usage, decoder_hint);
    return ERROR_SUCCESS;

error_cleanup:
    if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
    if (pathbuf != NULL) free(pathbuf);
    file->OSError     =  result;
    file->AccessMode  =  access;
    file->ShareMode   =  share;
    file->OpenFlags   =  flags;
    file->Fildes      =  INVALID_HANDLE_VALUE;
    file->SectorSize  =  0;
    file->BaseOffset  =  0;
    file->BaseSize    =  0;
    file->FileSize    =  0;
    file->FileHints   =  file_hints;
    file->FileFlags   =  VFS_FILE_FLAG_NONE;
    file->Decoder     =  NULL;
    return result;
}

/// @summary Atomically and synchronously writes a file. If the file exists, its contents are overwritten; otherwise a new file is created. This operation is generally not supported by archive-based mount points.
/// @param m The mount point performing the file save.
/// @param path The path of the file to save, relative to the mount point root.
/// @param data The buffer containing the data to write to the file.
/// @param size The number of bytes to copy from the buffer into the file.
/// @return ERROR_SUCCESS, ERROR_NOT_SUPPORTED or the OS error code.
internal_function DWORD vfs_save_fs(vfs_mount_t *m, char const *path, void const *data, int64_t size)
{
    FILE_END_OF_FILE_INFO eof;
    FILE_ALLOCATION_INFO  sec;
    vfs_mount_fs_t        *fs = (vfs_mount_fs_t*)m->State;
    uint8_t const     *buffer = (uint8_t const *)data;
    SYSTEM_INFO sysinfo       = {0};
    HANDLE      fd            = INVALID_HANDLE_VALUE;
    DWORD       mem_flags     = MEM_RESERVE  | MEM_COMMIT;
    DWORD       mem_protect   = PAGE_READWRITE;
    DWORD       access        = GENERIC_READ | GENERIC_WRITE;
    DWORD       share         = FILE_SHARE_READ;
    DWORD       create        = CREATE_ALWAYS;
    DWORD       flags         = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING;
    WCHAR      *file_path     = NULL;
    size_t      file_path_len = 0;
    WCHAR      *temp_path     = NULL;
    WCHAR      *temp_ext      = NULL;
    size_t      temp_ext_len  = 0;
    DWORD       page_size     = 4096;
    int64_t     file_size     = 0;
    uint8_t    *sector_buffer = NULL;
    size_t      sector_count  = 0;
    int64_t     sector_bytes  = 0;
    size_t      sector_over   = 0;
    size_t      sector_size   = 0;
    DWORD       error         = ERROR_SUCCESS;

    // get the system page size, and allocate a single-page buffer. to support
    // unbuffered I/O, the buffer must be allocated on a sector-size boundary
    // and must also be a multiple of the physical disk sector size. allocating
    // a virtual memory page using VirtualAlloc will satisfy these constraints.
    GetNativeSystemInfo(&sysinfo);
    page_size     = sysinfo.dwPageSize;
    sector_buffer = (uint8_t*) VirtualAlloc(NULL, page_size, mem_flags, mem_protect);
    if (sector_buffer == NULL)
    {   // unable to allocate the overage buffer; check GetLastError().
        goto error_cleanup;
    }

    // generate a temporary filename in the same location as the output file.
    // this prevents the file from being copied when it is moved, which happens
    // if the temp file isn't on the same volume as the output file.
    if ((file_path = vfs_make_system_path_fs(fs, path)) == NULL)
    {   // unable to allocate the output path buffer.
        error = ERROR_OUTOFMEMORY;
        goto error_cleanup;
    }
    temp_ext_len   =  4; // wcslen(L".tmp")
    file_path_len  =  wcslen(file_path);
    if ((temp_path = (WCHAR*) malloc((file_path_len + temp_ext_len + 1) * sizeof(WCHAR))) == NULL)
    {   // unable to allocate temporary path memory.
        error = ERROR_OUTOFMEMORY;
        goto error_cleanup;
    }
    temp_ext  = temp_path + file_path_len;
    memcpy(temp_path, file_path, file_path_len   * sizeof(WCHAR));
    memcpy(temp_ext , L".tmp"  ,(temp_ext_len+1) * sizeof(WCHAR));

    // create the temporary file, and get the physical disk sector size.
    // pre-allocate storage for the file contents, which should improve performance.
    if ((fd = CreateFile(temp_path, access, share, NULL, create, flags, NULL)) == INVALID_HANDLE_VALUE)
    {   // unable to create the new file; check GetLastError().
        goto error_cleanup;
    }
    sector_size = vfs_physical_sector_size(fd);
    file_size   = align_up(size, sector_size);
    sec.AllocationSize.QuadPart = file_size;
    SetFileInformationByHandle(fd, FileAllocationInfo , &sec, sizeof(sec));

    // copy the data extending into the tail sector into the overage buffer.
    sector_count = size_t (size / sector_size);
    sector_bytes = int64_t(sector_size) * sector_count;
    sector_over  = size_t (size - sector_bytes);
    if (sector_over > 0)
    {   // buffer the overlap amount. note that the page will have been zeroed.
        memcpy(sector_buffer, &buffer[sector_bytes], sector_over);
    }

    // write the data to the file.
    if (sector_bytes > 0)
    {   // write the bulk of the data, if the data is > 1 sector.
        int64_t amount = 0;
        DWORD   nwrite = 0;
        while  (amount < sector_bytes)
        {
            DWORD n =(sector_bytes - amount) < 0xFFFFFFFFU ? DWORD(sector_bytes - amount) : 0xFFFFFFFFU;
            WriteFile(fd, &buffer[amount], n, &nwrite, NULL);
            amount += nwrite;
        }
    }
    if (sector_over > 0)
    {   // write the remaining sector-sized chunk of data.
        DWORD n = (DWORD) sector_size;
        DWORD w = (DWORD) 0;
        WriteFile(fd, sector_buffer, n, &w, NULL);
    }

    // set the correct end-of-file marker.
    eof.EndOfFile.QuadPart = size;
    SetFileInformationByHandle(fd, FileEndOfFileInfo, &eof, sizeof(eof));
    SetFileValidData(fd, eof.EndOfFile.QuadPart);

    // close the file, and move it to the destination path.
    CloseHandle(fd); fd = INVALID_HANDLE_VALUE;
    if (!MoveFileEx(temp_path, file_path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {   // the file could not be moved into place; check GetLastError().
        goto error_cleanup;
    }

    // clean up our temporary buffers; we're done.
    free(temp_path);
    free(file_path);
    VirtualFree(sector_buffer, 0, MEM_RELEASE);
    return ERROR_SUCCESS;

error_cleanup:
    if (error == ERROR_SUCCESS)     error = GetLastError();
    if (fd != INVALID_HANDLE_VALUE) CloseHandle(fd);
    if (temp_path     != NULL)      DeleteFile(temp_path);
    if (temp_path     != NULL)      free(temp_path);
    if (file_path     != NULL)      free(file_path);
    if (sector_buffer != NULL)      VirtualFree(sector_buffer, 0, MEM_RELEASE);
    return error;
}

/// @summary Queries a mount point to determine whether it supports the specified usage.
/// @param m The mount point being queried.
/// @param usage One of vfs_file_usage_e specifying the intended usage of the file.
/// @param decoder_hint One of vfs_decoder_hint_t specifying the type of decoder desired.
/// @return Either ERROR_SUCCESS or ERROR_NOT_SUPPORTED.
internal_function DWORD vfs_support_fs(vfs_mount_t *m, int32_t usage, int32_t decoder_hint)
{
    switch (usage)
    {
    case VFS_USAGE_STREAM_IN:
    case VFS_USAGE_STREAM_IN_LOAD:
    case VFS_USAGE_STREAM_OUT:
    case VFS_USAGE_MANUAL_IO:
        break;
    default:
        return ERROR_NOT_SUPPORTED;
    }

    switch (decoder_hint)
    {
    case VFS_DECODER_HINT_USE_DEFAULT:
        break;
    default:
        break;
    }
    UNREFERENCED_PARAMETER(m);
    return ERROR_SUCCESS;
}

/// @summary Unmount the mount point. This function should close any open file handles and free any resources allocated for the mount point, including the memory allocated for the vfs_mount_t::State field.
/// @param m The mount point being unmounted.
internal_function void vfs_unmount_fs(vfs_mount_t *m)
{
    if (m->State != NULL)
    {   // free internal state data.
        free(m->State);
    }
    m->State      = NULL;
    m->open       = NULL;
    m->save       = NULL;
    m->unmount    = NULL;
    m->supports   = NULL;
}

/// @summary Initialize a filesystem-based mountpoint.
/// @param m The mountpoint instance to initialize.
/// @param local_path The NULL-terminated string specifying the local path that functions as the root of the mount point. This directory must exist.
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
    m->State    = fs;
    m->open     = vfs_open_fs;
    m->save     = vfs_save_fs;
    m->unmount  = vfs_unmount_fs;
    m->supports = vfs_support_fs;
    return true;
}

/// @summary Calculate the byte offset of the start of the next header in a tarball.
/// @param data_offset The absolute byte offset of the start of the data.
/// @param data_size The un-padded size of the file data, in bytes.
/// @return The byte offset of the start of the next header record.
internal_function inline int64_t tar_next_header_offset(int64_t data_offset, int64_t data_size)
{   // assuming a block size of 512 bytes, which is typical.
    // round the data size up to the nearest even block multiple as
    // the data blocks are padded up to the block size with zero bytes.
    return data_offset + (((data_size + 511) / 512) * 512);
}

/// @summary Copy a string value until a zero-byte is encountered.
/// @param dst The start of the destination buffer.
/// @param src The start of the source buffer.
/// @param dst_offset The offset into the destination buffer at which to begin writing, in bytes.
/// @param max The maximum number of bytes to copy from the source to the destination.
/// @return The input dst_offset, plus the number of bytes copied from src to dst minus one.
internal_function inline size_t tar_strcpy(char *dst, char const *src, size_t dst_offset, size_t max)
{
    size_t s = 0;
    size_t d = dst_offset;
    do
    {
        dst[d++] = src[s];
    }
    while (src[s] != 0 && s++ < max);
    return dst_offset + s;
}

/// @summary Parses a zero-padded octal string and converts it to a decimal value.
/// @param str The buffer containing the value to parse.
/// @param length The maximum number of bytes to read from the input string.
/// @return The decoded decimal integer value.
template <typename int_type>
internal_function  int_type tar_octal_to_decimal(char const *str, size_t length)
{
    int_type    n = 0;
    for (size_t i = 0; i < length; ++i)
    {
        if (str[i] >= '0' && str[i] <= '8')
        {
            n *= 8;
            n += str[i] - 48;
        }
        else break;
    }
    return n;
}

/// @summary Parses an encoded TAR header entry to generate the data stored by the mount point.
/// @param dst The record to populate.
/// @param src The raw header record as it exists on disk.
/// @param offset The absolute byte offset of the start of the header record within the tarball.
/// @return The absolute byte offset of the start of the next header record.
internal_function int64_t tar_decode_entry(tar_entry_t *dst, tar_header_encoded_t const *src, int64_t offset)
{   // decode the base header fields.
    dst->FileSize    = tar_octal_to_decimal< int64_t>(src->FileSize, 12);
    dst->FileTime    = tar_octal_to_decimal<uint64_t>(src->FileTime, 12);
    dst->DataOffset  = offset + sizeof(tar_header_encoded_t);
    dst->Checksum    = tar_octal_to_decimal<uint32_t>(src->Checksum,  8);
    dst->FileType    = src->FileType;
    // check to see if a ustar extended header is present.
    ustar_header_encoded_t const *ustar = (ustar_header_encoded_t const*) src->ExtraPad;
    if (strncmp(ustar->MagicStr, "ustar", 5) == 0)
    {   // a ustar extended header is present, which affects the path strings.
        size_t slen  = tar_strcpy(dst->FullPath, ustar->FileBase, 0, 155);
        // a single forward slash separates the base path and filename.
        if (slen > 0)  dst->FullPath[slen++] = '/';
        // append the filename to the path string.
        slen = tar_strcpy(dst->FullPath, src->FileName, slen, 100); dst->FullPath[slen] = 0;
        // the link name never has the BasePath prepended; copy it over as-is.
        memcpy(dst->LinkName, src->LinkName, 100 * sizeof(char)); dst->LinkName[100] = 0;
    }
    else
    {   // no extended header is present, so copy over the file and link name.
        memcpy(dst->FullPath, src->FileName, 100 * sizeof(char)); dst->FullPath[100] = 0;
        memcpy(dst->LinkName, src->LinkName, 100 * sizeof(char)); dst->LinkName[100] = 0;
    }
    return tar_next_header_offset(dst->DataOffset, dst->FileSize);
}

/// @summary Normalize path separators for a character.
/// @param ch The character to inspect.
/// @return The forward slash if ch is a backslash, otherwise ch.
internal_function force_inline uint32_t tar_normalize_ch(char ch)
{
    return ch != '\\' ? uint32_t(ch) : uint32_t('/');
}

/// @summary Calculates a 32-bit hash value for a path string. 
/// Forward and backslashes are treated as equivalent.
/// @param path A NULL-terminated UTF-8 path string.
/// @return The hash of the specified string.
internal_function uint32_t tar_hash_path(char const *path)
{
    if (path != NULL && path[0] != 0)
    {
        uint32_t    code = 0;
        uint32_t    hash = 0;
        char const *iter = path;
        do
        {
            code = tar_normalize_ch(*iter++);
            hash = _lrotl(hash, 7) + code;
        }
        while (code != 0);
        return hash;
    }
    else return 0;
}

/// @summary Ensure that a TAR entry list meets or exceeds the specified capacity. Grow the list if necessary.
/// @param tar The TAR mountpoint state.
/// @param caapcity The minimum capacity of the TAR file entry list.
/// @return true if the TAR mountpoint state has the specified capacity.
internal_function bool vfs_ensure_tar_entry_list(vfs_mount_tarball_t *tar, size_t capacity)
{
    if (capacity <= tar->EntryCapacity)
    {   // no need to expand the list storage.
        return true;
    }
    size_t       nc =  calculate_capacity(tar->EntryCapacity, capacity, 1024, 256);
    uint32_t    *nh = (uint32_t   *)  realloc(tar->EntryHash, nc * sizeof(uint32_t));
    tar_entry_t *ni = (tar_entry_t*)  realloc(tar->EntryInfo, nc * sizeof(tar_entry_t));
    if (nh != NULL)    tar->EntryHash = nh;
    if (ni != NULL)    tar->EntryInfo = ni;
    if (nh != NULL && ni != NULL)
    {
        tar->EntryCapacity = nc;
        return true;
    }
    else return false;
}

/// @summary Parse a TAR file and construct the list of file and directory entries.
/// @param tar The TAR file mountpoint state to populate with entity data.
/// @param tarfd The file descriptor to access for reading the TAR file data.
/// @return ERROR_SUCCESS or a system error code.
internal_function DWORD vfs_load_tarball(vfs_mount_tarball_t *tar, HANDLE tarfd)
{
    DWORD result  = ERROR_SUCCESS;
    LARGE_INTEGER offset;
    LARGE_INTEGER newptr;
    LARGE_INTEGER startp;

    // begin reading tar entries from the current file position.
    // retrieve the current file pointer location.
    offset.QuadPart = 0; newptr.QuadPart = 0; startp.QuadPart = 0;
    SetFilePointerEx(tarfd, offset, &startp, FILE_CURRENT);
    offset.QuadPart = startp.QuadPart;
    newptr.QuadPart = startp.QuadPart;
    for ( ; ; )
    {   // read the next archive entry header and check for end-of-archive.
        DWORD nread = 0;
        tar_header_encoded_t  header  = {};
        if (!ReadFile(tarfd, &header, sizeof(header), &nread, NULL) || nread < sizeof(header))
        {   // unable to read a complete header block.
            result = GetLastError();
            break;
        }
        if (header.FileName[0] == 0)
        {   // an entry with no filename indicates the end of the archive.
            result = ERROR_SUCCESS;
            break;
        }

        // filter out types of entries we aren't interested in:
        int64_t  header_end  = offset.QuadPart + sizeof(header);
        if (header.FileType != TAR_ENTRY_TYPE_FILE && header.FileType != TAR_ENTRY_TYPE_HARDLINK && header.FileType != TAR_ENTRY_TYPE_SYMLINK)
        {   // this is an entry type we don't care about, so skip it.
            int64_t file_size = tar_octal_to_decimal<int64_t>(header.FileSize, 12);
            offset.QuadPart   = tar_next_header_offset(header_end,   file_size);
            continue;
        }

        // create a record to represent this entry, and calculate the offset of the 
        // next header entry. only header entries are stored in memory; data is not.
        if (!vfs_ensure_tar_entry_list(tar, tar->EntryCount + 1))
        {   // unable to allocate memory ...
            result = ERROR_OUTOFMEMORY;
            break;
        }
        size_t n = tar->EntryCount++;
        offset.QuadPart = tar_decode_entry(&tar->EntryInfo[n], &header, offset.QuadPart);
        tar->EntryHash[n] = tar_hash_path(tar->EntryInfo[n].FullPath);
        SetFilePointerEx(tarfd, offset, &newptr, FILE_BEGIN);
    }
    return result;
}

/// @summary Synchronously opens a file for access. Information necessary to access the file is written to the vfs_file_t structure. This function is called for each mount point, in priority order, until one either returns ERROR_SUCCESS or an error code other than ERROR_NOT_SUPPORTED.
/// @param m The mount point performing the file open.
/// @param path The path of the file to open, relative to the mount point root.
/// @param usage One of vfs_file_usage_e specifying the intended use of the file.
/// @param file_hints A combination of vfs_file_hint_e used to control how the file is opened.
/// @param decoder_hint One of vfs_decoder_hint_e specifying the preferred decoder type, or VFS_DECODER_HINT_USE_DEFAULT to let the implementation decide.
/// @param file On return, this structure should be populated with the data necessary to access the file. If the file cannot be opened, set the OSError field.
/// @return ERROR_SUCCESS, ERROR_NOT_SUPPORTED or the OS error code.
internal_function DWORD vfs_open_tarball(vfs_mount_t *m, char const *path, int32_t usage, uint32_t file_hints, int32_t decoder_hint, vfs_file_t *file)
{   // figure out access flags, share modes, etc. based on the intended usage.
    HANDLE  hFile  = INVALID_HANDLE_VALUE;
    DWORD   result = ERROR_NOT_SUPPORTED;
    DWORD   access = 0;
    DWORD   share  = 0;
    DWORD   create = 0;
    DWORD   flags  = 0;
    bool    dupfd  = false;
    switch (usage)
    {
    case VFS_USAGE_STREAM_IN:
    case VFS_USAGE_STREAM_IN_LOAD:
        dupfd  = true;
        access = GENERIC_READ;
        share  = FILE_SHARE_READ;
        create = OPEN_EXISTING;
        flags  = FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_OVERLAPPED;
        // alignment restrictions can't be guaranteed, so unbuffered I/O isn't supported.
        break;

    case VFS_USAGE_MANUAL_IO:
        dupfd  = false;
        access = GENERIC_READ;
        share  = FILE_SHARE_READ;
        create = OPEN_EXISTING;
        flags  = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN;
        if (file_hints & VFS_FILE_HINT_UNBUFFERED)   flags |= FILE_FLAG_NO_BUFFERING;
        if (file_hints & VFS_FILE_HINT_ASYNCHRONOUS) flags |= FILE_FLAG_OVERLAPPED;
        break;

    default:
        result = ERROR_NOT_SUPPORTED;
        goto error_cleanup;
    }

    // locate the file in the tarball by comparing hashes.
    vfs_mount_tarball_t *tar       = (vfs_mount_tarball_t*) m->State;
    uint32_t             hash      =  tar_hash_path(path);
    uint32_t const      *hash_list =  tar->EntryHash;
    for (size_t i = 0, n  = tar->EntryCount; i < n; ++i)
    {
        if (hash_list[i] == hash)
        {   // hashes match, confirm by comparing strings.
            if (!_stricmp(path, tar->EntryInfo[i].FullPath))
            {   // found an exact match for the path; proceed with open.
                // if the open attributes are an exact match for the attributes
                // used to mount the tarball, we can just duplicate the handle;
                // otherwise, the file must be opened again.
                if (dupfd)
                {   // duplicate the existing file handle.
                    HANDLE p = GetCurrentProcess();
                    DuplicateHandle(p, tar->TarFildes, p, &hFile, 0, FALSE, DUPLICATE_SAME_ACCESS);
                }
                else
                {   // open the file as specified by the caller.
                    hFile = CreateFile(tar->LocalPath, access, share, NULL, create, flags, NULL);
                    if (hFile == INVALID_HANDLE_VALUE)
                    {   // the file cannot be opened.
                        result = GetLastError();
                        goto error_cleanup;
                    }
                }
                // finished with setup, the open succeeds.
                file->OSError     = ERROR_SUCCESS;
                file->AccessMode  = access;
                file->ShareMode   = share;
                file->OpenFlags   = flags;
                file->Fildes      = hFile;
                file->SectorSize  = tar->SectorSize;
                file->BaseOffset  = tar->EntryInfo[i].DataOffset;
                file->BaseSize    = tar->EntryInfo[i].FileSize;
                file->FileSize    = tar->EntryInfo[i].FileSize;
                file->FileHints   = file_hints;
                file->FileFlags   = VFS_FILE_FLAG_EXPLICIT_CLOSE;
                file->Decoder     = vfs_create_decoder(usage, decoder_hint);
                return ERROR_SUCCESS;
            }
        }
    }

error_cleanup:
    if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
    file->OSError     =  result;
    file->AccessMode  =  access;
    file->ShareMode   =  share;
    file->OpenFlags   =  flags;
    file->Fildes      =  INVALID_HANDLE_VALUE;
    file->SectorSize  =  0;
    file->BaseOffset  =  0;
    file->BaseSize    =  0;
    file->FileSize    =  0;
    file->FileHints   =  file_hints;
    file->FileFlags   =  VFS_FILE_FLAG_NONE;
    file->Decoder     =  NULL;
    return result;
}

/// @summary Atomically and synchronously writes a file. If the file exists, its contents are overwritten; otherwise a new file is created. This operation is generally not supported by archive-based mount points.
/// @param m The mount point performing the file save.
/// @param path The path of the file to save, relative to the mount point root.
/// @param data The buffer containing the data to write to the file.
/// @param size The number of bytes to copy from the buffer into the file.
/// @return ERROR_SUCCESS, ERROR_NOT_SUPPORTED or the OS error code.
internal_function DWORD vfs_save_tarball(vfs_mount_t *m, char const *path, void const *data, int64_t size)
{
    UNREFERENCED_PARAMETER(m);
    UNREFERENCED_PARAMETER(path);
    UNREFERENCED_PARAMETER(data);
    UNREFERENCED_PARAMETER(size);
   return ERROR_NOT_SUPPORTED;
}

/// @summary Queries a mount point to determine whether it supports the specified usage.
/// @param m The mount point being queried.
/// @param usage One of vfs_file_usage_e specifying the intended usage of the file.
/// @param decoder_hint One of vfs_decoder_hint_t specifying the type of decoder desired.
/// @return Either ERROR_SUCCESS or ERROR_NOT_SUPPORTED.
internal_function DWORD vfs_support_tarball(vfs_mount_t *m, int32_t usage, int32_t decoder_hint)
{
    switch (usage)
    {
    case VFS_USAGE_STREAM_IN:
    case VFS_USAGE_STREAM_IN_LOAD:
    case VFS_USAGE_MANUAL_IO:
        break;
    default:
        return ERROR_NOT_SUPPORTED;
    }

    switch (decoder_hint)
    {
    case VFS_DECODER_HINT_USE_DEFAULT:
        break;
    default:
        break;
    }
    UNREFERENCED_PARAMETER(m);
    return ERROR_SUCCESS;
}

/// @summary Unmount the mount point. This function should close any open file handles and free any resources allocated for the mount point, including the memory allocated for the vfs_mount_t::State field.
/// @param m The mount point being unmounted.
internal_function void vfs_unmount_tarball(vfs_mount_t *m)
{
    if (m->State != NULL)
    {   // free internal state data.
        vfs_mount_tarball_t *tar = (vfs_mount_tarball_t*) m->State;
        CloseHandle(tar->TarFildes);
        free(tar->EntryInfo);
        free(tar->EntryHash);
        tar->TarFildes     = INVALID_HANDLE_VALUE;
        tar->EntryCapacity = 0;
        tar->EntryCount    = 0;
        tar->EntryHash     = NULL;
        tar->EntryInfo     = NULL;
        free(m->State);
    }
    m->State      = NULL;
    m->open       = NULL;
    m->save       = NULL;
    m->unmount    = NULL;
    m->supports   = NULL;
}

/// @summary Initialize a filesystem-based mountpoint.
/// @param m The mountpoint instance to initialize.
/// @param local_path The NULL-terminated string specifying the local path that functions as the root of the mount point. This directory must exist.
/// @return true if the mount point was initialized successfully.
internal_function bool vfs_init_mount_tarball(vfs_mount_t *m, WCHAR const *local_path)
{
    vfs_mount_tarball_t *tar = NULL;
    if ((tar =(vfs_mount_tarball_t*) malloc(sizeof(vfs_mount_tarball_t))) == NULL)
    {   // unable to allocate memory for internal state.
        return false;
    }

    // open the TAR file. this handle remains open so that most file 
    // operations can just duplicate the handle as opposed to reopening 
    // the file handle (which I'm guessing is more efficient.)
    DWORD  access = GENERIC_READ;
    DWORD  share  = FILE_SHARE_READ;
    DWORD  flags  = FILE_FLAG_SEQUENTIAL_SCAN;
    DWORD  create = OPEN_EXISTING;
    HANDLE tarfd  = CreateFile(local_path, access, share, NULL, create, flags, NULL);
    if (tarfd == INVALID_HANDLE_VALUE)
    {   // unable to open the source tarball.
        free(tar);
        return false;
    }

    // retrieve a fully-resolved path name to the TAR file for the cases 
    // where it needs to be re-opened due to differing file flags.
    DWORD fnflags      = FILE_NAME_NORMALIZED | VOLUME_NAME_DOS;
    tar->LocalPathLen  = GetFinalPathNameByHandle(tarfd, tar->LocalPath, MAX_PATH_CHARS, fnflags);

    // save the file handle for later duplication and initialize the entry list.
    // read all of the metadata from the file into memory.
    tar->EntryCapacity = 0;
    tar->EntryCount    = 0;
    tar->EntryHash     = NULL;
    tar->EntryInfo     = NULL;
    tar->SectorSize    = vfs_physical_sector_size(tarfd);
    vfs_ensure_tar_entry_list(tar, 128);
    vfs_load_tarball(tar, tarfd);

    // close and re-open the TAR file with FILE_FLAG_OVERLAPPED specified.
    tar->TarFildes = CreateFile(tar->LocalPath, access, share, NULL, create, flags | FILE_FLAG_OVERLAPPED, NULL);
    CloseHandle(tarfd);

    // set the function pointers, etc. on the mount point.
    m->State    = tar;
    m->open     = vfs_open_tarball;
    m->save     = vfs_save_tarball;
    m->unmount  = vfs_unmount_tarball;
    m->supports = vfs_support_tarball;
    return true;
}

/// @summary Performs all of the common setup when creating a new mount point.
/// @param driver The VFS driver instance the mount point is attached to.
/// @param source_wide The NULL-terminated source directory or file path.
/// @param source_attr The attributes of the source path, as returned by GetFileAttributes().
/// @param mount_path The NULL-terminated UTF-8 string specifying the mount point root exposed externally.
/// @param priority The priority of the mount point. Higher numeric values indicate higher priority.
/// @param mount_id An application-defined unique identifier for the mount point. This is necessary if the application later wants to remove this specific mount point, but not any of the others that are mounted with the same mount_path.
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
            else ext--;
        }
        // now a bunch of if (wcsicmp(ext, L"zip") { ... } etc.
        if (!_wcsicmp(ext, L"tar"))
        {   // set up a tarball-based mount point.
            if (!vfs_init_mount_tarball(mount, source_wide))
            {   // remove the item we just added and clean up.
                vfs_mounts_remove_at(driver->Mounts, driver->Mounts.Count-1);
                ReleaseSRWLockExclusive(&driver->MountsLock);
                free(mount->Root); mount->Root = NULL;
                return false;
            }
            ReleaseSRWLockExclusive(&driver->MountsLock);
            return true;
        }
        // TODO(rlk): need to make sure to release MountsLock.
        ReleaseSRWLockExclusive(&driver->MountsLock);
        return false;
    }
}

/// @summary Resolves a virtual path to a filesystem mount point, and then converts the 
/// path to an absolute path on the filesystem. Non-filesystem mount points are skipped.
/// @param driver The virtual file system driver to search.
/// @param path The virtual path to translate to a filesystem path.
/// @param pathbuf The buffer that will receive the filesystem path.
/// @param buflen The maximum number of wide characters to write to pathbuf.
/// @return ERROR_SUCCESS, ERROR_NOT_FOUND, or a system error code.
internal_function DWORD vfs_resolve_filesystem_path(vfs_driver_t *driver, char const *path, WCHAR *pathbuf, size_t buflen)
{   // iterate over the mount points, which are stored in priority order.
    AcquireSRWLockShared(&driver->MountsLock);
    DWORD        result  = ERROR_NOT_FOUND;
    vfs_mount_t *mounts  = driver->Mounts.MountData;
    for (size_t i = 0; i < driver->Mounts.Count; ++i)
    {
        vfs_mount_t  *m  = &mounts[i];
        if (vfs_mount_point_match_start(m->Root, path, m->RootLen) && m->open == vfs_open_fs)
        {   // found the matching filesystem mount point; resolve the absolute path.
            vfs_mount_fs_t *fs            =(vfs_mount_fs_t*) m->State;
            char const     *relative_path = path + m->RootLen + 1;
            size_t          offset        = fs->LocalPathLen;
            int             nchars        = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, relative_path, -1, NULL, 0);
            if (nchars == 0 || buflen < (nchars + offset + 1))
            {   // the path cannot be converted from UTF-8 to UCS-2.
                result  = GetLastError();
                break;
            }
            // start with the local root, and then append the relative path portion.
            memcpy(pathbuf, fs->LocalPath, fs->LocalPathLen * sizeof(WCHAR));
            pathbuf[fs->LocalPathLen] = L'\\';
            if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, relative_path, -1, &pathbuf[fs->LocalPathLen+1], nchars) == 0)
            {   // the path cannot be converted from UTF-8 to UCS-2.
                result  = GetLastError();
                break;
            }
            // normalize the directory separator characters to the system default.
            // on Windows, this is not strictly necessary, but on other OS's it is.
            for (size_t i = offset + 1, n = offset + nchars; i < n; ++i)
            {
                if (pathbuf[i] == L'/')
                    pathbuf[i]  = L'\\';
            }
            result = ERROR_SUCCESS;
            break;
        }
    }
    ReleaseSRWLockShared(&driver->MountsLock);
    return result;
}

/// @summary Opens a file for access. Information necessary to access the file is written to the vfs_file_t structure. This function tests each mount point matching the path, in priority order, until one is successful at opening the file or all matching mount points have been tested.
/// @param driver The virtual file system driver to query.
/// @param path The absolute virtual path of the file to open.
/// @param usage One of vfs_file_usage_e specifying the intended use of the file.
/// @param file_hints A combination of vfs_file_hint_e.
/// @param decoder_hint One of vfs_decoder_hint_e specifying the preferred decoder type, or VFS_DECODER_HINT_USE_DEFAULT to let the implementation decide.
/// @param file On return, this structure should be populated with the data necessary to access the file. If the file cannot be opened, check the OSError field.
/// @param relative_path On return, this pointer is updated to point into path, at the start of the portion of the path representing the path to the file, relative to the mount point root path.
/// @return ERROR_SUCCESS, ERROR_NOT_SUPPORTED, or an OS error code.
internal_function DWORD vfs_resolve_and_open_file(vfs_driver_t *driver, char const *path, int32_t usage, uint32_t file_hints, int32_t decoder_hint, vfs_file_t *file, char const **relative_path)
{   // iterate over the mount points, which are stored in priority order.
    AcquireSRWLockShared(&driver->MountsLock);
    DWORD        result  = ERROR_NOT_SUPPORTED;
    vfs_mount_t *mounts  = driver->Mounts.MountData;
    for (size_t i = 0; i < driver->Mounts.Count; ++i)
    {
        vfs_mount_t  *m  = &mounts[i];
        if (vfs_mount_point_match_start(m->Root, path, m->RootLen))
        {   // attempt to open the file on the mount point.
            DWORD openrc = ERROR_NOT_SUPPORTED;
            if  ((openrc = m->open(m, path + m->RootLen + 1, usage, file_hints, decoder_hint, file)) == ERROR_SUCCESS)
            {   // the file was successfully opened, so we're done.
                if (relative_path != NULL)
                {
                    *relative_path =  path + m->RootLen + 1;
                }
                ReleaseSRWLockShared(&driver->MountsLock);
                return ERROR_SUCCESS;
            }
            if (openrc == ERROR_NOT_SUPPORTED)
            {   // continue checking downstream mount points.
                // one of them might support the operation.
                continue;
            }
            else if (openrc == ERROR_NOT_FOUND || openrc == ERROR_FILE_NOT_FOUND)
            {   // continue checking downstream mount points.
                // one of them might be able to open the file.
                result  = ERROR_FILE_NOT_FOUND;
                continue;
            }
            else
            {   // an actual error was returned; push it upstream.
                ReleaseSRWLockShared(&driver->MountsLock);
                return openrc;
            }
        }
    }
    ReleaseSRWLockShared(&driver->MountsLock);
    return result;
}

/// @summary Atomically and synchronously writes a file. If the file exists, its contents are overwritten; otherwise a new file is created. This operation is generally not supported by archive-based mount points. This function tests each mount point matching the path, in priority order, until one is successful at saving the file or until all matching mount points have been tested.
/// @param driver The virtual file system driver.
/// @param path The virtual path of the file to save.
/// @param buffer The buffer containing the data to write to the file.
/// @param amount The number of bytes to copy from the buffer into the file.
/// @param relative_path On return, this pointer is updated to point into path, at the start of the portion of the path representing the path to the file, relative to the mount point root path.
/// @return ERROR_SUCCESS, ERROR_NOT_SUPPORTED or the OS error code.
internal_function DWORD vfs_resolve_and_save_file(vfs_driver_t *driver, char const *path, void const *buffer, int64_t amount, char const **relative_path)
{   // iterate over the mount points, which are stored in priority order.
    AcquireSRWLockShared(&driver->MountsLock);
    DWORD        result  = ERROR_NOT_SUPPORTED;
    vfs_mount_t *mounts  = driver->Mounts.MountData;
    for (size_t i = 0; i < driver->Mounts.Count; ++i)
    {
        vfs_mount_t  *m  = &mounts[i];
        if (vfs_mount_point_match_start(m->Root, path, m->RootLen))
        {   // attempt to save the file on the mount point.
            DWORD openrc = ERROR_NOT_SUPPORTED;
            if  ((openrc = m->save(m, path + m->RootLen + 1, buffer, amount)) == ERROR_SUCCESS)
            {   // the file was successfully opened, so we're done.
                if (relative_path != NULL)
                {
                    *relative_path =  path + m->RootLen + 1;
                }
                ReleaseSRWLockShared(&driver->MountsLock);
                return ERROR_SUCCESS;
            }
            else result = openrc;
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
/// @param aio The asynchronous I/O driver to use for I/O operations.
/// @param pio The prioritized I/O driver to use for I/O operations.
/// @return ERROR_SUCCESS on success, or an error code returned by GetLastError() on failure.
public_function DWORD vfs_driver_open(vfs_driver_t *driver, aio_driver_t *aio, pio_driver_t *pio)
{
    driver->AIO = aio;
    driver->PIO = pio;
    InitializeSRWLock(&driver->MountsLock);
    vfs_mounts_create( driver->Mounts, 128);
    driver->StreamBuffer.reserve(STREAM_BUFFER_SIZE, STREAM_IN_CHUNK_SIZE);
    return ERROR_SUCCESS;
}


/// @summary Closes a virtual file system driver and frees all associated resources.
/// @param driver The virtual file system driver to clean up.
public_function void vfs_driver_close(vfs_driver_t *driver)
{
    driver->StreamBuffer.release();
    vfs_mounts_delete(driver->Mounts);
    driver->PIO = NULL;
    driver->AIO = NULL;
}

/// @summary Creates a mount point backed by a well-known directory.
/// @param driver The virtual file system driver.
/// @param folder_id One of vfs_known_path_e specifying the well-known directory.
/// @param mount_path The NULL-terminated UTF-8 string specifying the root of the mount point as exposed to the application code. Multiple mount points may share the same mount_path, but have different source_path, mount_id and priority values.
/// @param priority The priority assigned to this specific mount point, with higher numeric values corresponding to higher priority values.
/// @param mount_id A unique application-defined identifier for the mount point. This identifier can be used to reference the specific mount point.
/// @return true if the mount point was successfully created.
public_function bool vfs_mount_known(vfs_driver_t *driver, int folder_id, char const *mount_path, uint32_t priority, uintptr_t mount_id)
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
/// @param source_path The NULL-terminated UTF-8 string specifying the path of the file or directory that represents the root of the mount point.
/// @param mount_path The NULL-terminated UTF-8 string specifying the root of the mount point as exposed to the application code. Multiple mount points may share the same mount_path, but have different source_path, mount_id and priority values.
/// @param priority The priority assigned to this specific mount point, with higher numeric values corresponding to higher priority values.
/// @param mount_id A unique application-defined identifier for the mount point. This identifier can be used to reference the specific mount point.
/// @return true if the mount point was successfully created.
public_function bool vfs_mount_native(vfs_driver_t *driver, char const *source_path, char const *mount_path, uint32_t priority, uintptr_t mount_id)
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

/// @summary Creates a mount point backed by the specified archive or directory, specified as a virtual path.
/// @param driver The virtual file system driver.
/// @param virtual_path The NULL-terminated UTF-8 string specifying the virtual path of the file or directory that represents the root of the mount point.
/// @param mount_path The NULL-terminated UTF-8 string specifying the root of the mount point as exposed to the application code. Multiple mount points may share the same mount_path, but have different source_path, mount_id and priority values.
/// @param priority The priority assigned to this specific mount point, with higher numeric values corresponding to higher priority values.
/// @param mount_id A unique application-defined identifier for the mount point. This identifier can be used to reference the specific mount point.
/// @return true if the mount point was successfully created.
public_function bool vfs_mount_virtual(vfs_driver_t *driver, char const *virtual_path, char const *mount_path, uint32_t priority, uintptr_t mount_id)
{   // convert the virtual path to a native filesystem path.
    DWORD  source_attr  = 0;
    WCHAR  source_wide[MAX_PATH_CHARS]; // 64KB - probably overkill
    if (!SUCCESS(vfs_resolve_filesystem_path(driver, virtual_path, source_wide, MAX_PATH_CHARS)))
    {   // cannot resolve the virtual path to a filesystem mount point.
        return false;
    }

    // determine whether the source path is a directory or a file.
    // TODO(rlk): GetFileAttributes() won't work (GetLastError() => ERROR_BAD_NETPATH)
    // in that case that source_path is the root of a network share. maybe fix this.
    if ((source_attr = GetFileAttributesW(source_wide)) == INVALID_FILE_ATTRIBUTES)
    {   // the source path must exist. mount fails.
        return false;
    }
    if (!vfs_setup_mount(driver, source_wide, source_attr, mount_path, priority, mount_id))
    {   // unable to finish internal setup of the mount point.
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

/// @summary Closes the underlying file handle and releases the VFS driver's 
/// reference to the underlying stream decoder. For internal use only.
/// @param file_info Internal information relating to the file to close.
public_function void vfs_close_file(vfs_file_t *file_info)
{
    if (file_info->Fildes != INVALID_HANDLE_VALUE)
    {
        CloseHandle(file_info->Fildes);
    }
    if (file_info->Decoder != NULL)
    {
        file_info->Decoder->release();
    }
    file_info->Fildes  = INVALID_HANDLE_VALUE;
    file_info->Decoder = NULL;
}

/// @summary Open a file for manual I/O. Close the file using vfs_close_file().
/// @param driver The virtual file system driver used to resolve the file path.
/// @param path A NULL-terminated UTF-8 string specifying the virtual file path.
/// @param file_hints A combination of vfs_file_hint_e specifying how to open the file. The file is opened for reading and writing.
/// @param decoder_hint One of vfs_decoder_hint_e specifying the type of stream decoder to create, or VFS_DECODER_HINT_NONE to not create a decoder.
/// @return ERROR_SUCCESS or a system error code.
public_function DWORD vfs_open_file(vfs_driver_t *driver, char const *path, uint32_t file_hints, int32_t decoder_hint, vfs_file_t *file)
{
    char const  *relpath     = NULL;
    int32_t      usage       = VFS_USAGE_MANUAL_IO;
    DWORD        open_result = vfs_resolve_and_open_file(driver, path, usage, file_hints, decoder_hint, file, &relpath);
    if (!SUCCESS(open_result)) return open_result;
    if (file_hints & VFS_FILE_HINT_ASYNCHRONOUS)
    {   // associate the file handle with the I/O completion port managed by the AIO driver.
        DWORD    asio_result = aio_driver_prepare(driver->AIO, file->Fildes);
        if (!SUCCESS(asio_result))
        {   // could not associate the file handle with the I/O completion port.
            vfs_close_file(file);
            return asio_result;
        }
    }
    if (file->Decoder != NULL)
    {   // addref the decoder for the caller.
        file->Decoder->addref();
    }
    return ERROR_SUCCESS;
}

/// @summary Synchronously reads data from a file.
/// @param driver The virtual file system driver used to open the file.
/// @param file The file state returned from vfs_open_file().
/// @param offset The absolute byte offset at which to start reading data.
/// @param buffer The buffer into which data will be written.
/// @param size The maximum number of bytes to read.
/// @param bytes_read On return, this value is set to the number of bytes actually read. This may be less than the number of bytes requested, or 0 at end-of-file.
/// @return ERROR_SUCCESS or a system error code.
public_function DWORD vfs_read_file_sync(vfs_driver_t *driver, vfs_file_t *file, int64_t offset, void *buffer, size_t size, size_t &bytes_read)
{   UNREFERENCED_PARAMETER(driver);
    LARGE_INTEGER oldpos;
    LARGE_INTEGER newpos;
    oldpos.QuadPart  = 0;
    newpos.QuadPart  = offset;
    if (!SetFilePointerEx(file->Fildes, newpos, &oldpos, FILE_BEGIN))
    {   // unable to seek to the specified offset.
        bytes_read = 0;
        return GetLastError();
    }
    // safely read the requested number of bytes. for very large reads,
    // size > UINT32_MAX, the result is undefined, so possibly split the
    // read up into several sub-reads (though this case is unlikely...)
    uint8_t*bufferp =(uint8_t*) buffer;
    DWORD   error   = ERROR_SUCCESS;
    bytes_read      = 0;
    while  (bytes_read < size)
    {
        DWORD nread = 0;
        DWORD rsize = 1024U * 1024U; // read 1MB
        if (size_t(rsize)   > (size - bytes_read))
        {   // read only the amount remaining.
            rsize =   (DWORD) (size - bytes_read);
        }
        if (ReadFile(file->Fildes, &bufferp[bytes_read], rsize, &nread, NULL))
        {   // the synchronous read completed successfully.
            bytes_read += nread;
        }
        else
        {   // an error occurred; save the error code.
            error = GetLastError(); break;
        }
    }
    return error;
}

/// @summary Reads data from a file asynchronously by submitting commands to the AIO driver. The input pointer file is stored in the Identifier field of each AIO result descriptor.
/// @param driver The virtual file system driver used to open the file.
/// @param file The file state returned from vfs_open_file().
/// @param offset The absolute byte offset at which to start reading data.
/// @param buffer The buffer into which data will be written.
/// @param size The maximum number of bytes to read.
/// @param close_flags One of aio_close_flags_e specifying the auto-close behavior.
/// @param priority The priority value to assign to the read request(s).
/// @param thread_alloc The FIFO node allocator used for submitting commands from the calling thread.
/// @param result_queue The SPSC unbounded FIFO where the completed read result will be placed.
/// @param result_alloc The FIFO node allocator used to write data to the result queue.
/// @return ERROR_SUCCESS or a system error code.
public_function DWORD vfs_read_file_async(vfs_driver_t *driver, vfs_file_t *file, int64_t offset, void *buffer, size_t size, uint32_t close_flags, uint32_t priority, pio_aio_request_alloc_t *thread_alloc, aio_result_queue_t *result_queue=NULL, aio_result_alloc_t *result_alloc=NULL)
{
    if (result_queue == NULL || result_alloc == NULL)
    {   // if no result queue was specified, use the queue on file->Decoder.
        if (file->Decoder == NULL)
        {   // 
            return ERROR_INVALID_PARAMETER;
        }
        // the AIO driver holds a reference to the stream decoder queues.
        result_queue =&file->Decoder->AIOResultQueue;
        result_alloc =&file->Decoder->AIOResultAlloc;
        file->Decoder->addref();
    }
    if (file->OpenFlags & FILE_FLAG_NO_BUFFERING)
    {   // files opened for unbuffered I/O are subject to alignment restrictions.
        uintptr_t addr  = (uintptr_t) buffer;
        if ((offset & (file->SectorSize-1)) != 0)
        {   // the read offset is not properly aligned.
            return ERROR_INVALID_PARAMETER;
        }
        if ((size   & (file->SectorSize-1)) != 0)
        {   // the number of bytes to read is not a multiple of the sector size.
            return ERROR_INVALID_PARAMETER;
        }
        if ((addr   & (file->SectorSize-1)) != 0)
        {   // the buffer is not aligned to a sector boundary.
            return ERROR_INVALID_PARAMETER;
        }
    }

    uint8_t  *bufferp =(uint8_t*) buffer;
    size_t bytes_read = 0;
    while (bytes_read < size)
    {
        DWORD rsize = 1024U * 1024U; // read 1MB
        if (size_t(rsize)   > (size - bytes_read))
        {   // read only the amount remaining.
            rsize =   (DWORD) (size - bytes_read);
        }
        // figure out the flags to pass through to the decoder.
        uint32_t flags   = STREAM_DECODE_STATUS_NONE;
        if (offset == 0)   flags = STREAM_DECODE_STATUS_RESTART;
        if (offset + rsize >= file->FileSize) flags |= STREAM_DECODE_STATUS_ENDOFSTREAM;
        // generate the I/O request for the AIO driver.
        aio_request_t req;
        req.CommandType  = AIO_COMMAND_READ;
        req.CloseFlags   = close_flags;
        req.Fildes       = file->Fildes;
        req.DataAmount   = rsize;
        req.DataActual   = rsize;
        req.BaseOffset   = 0;
        req.FileOffset   = offset;
        req.DataBuffer   =&bufferp[bytes_read];
        req.Identifier   = (uintptr_t) file;
        req.ResultAlloc  = result_alloc;
        req.ResultQueue  = result_queue;
        req.StatusFlags  = flags;
        req.Priority     = priority;
        // push the request to the AIO driver through the PIO driver queue.
        pio_driver_explicit_io(driver->PIO, req, thread_alloc);
        // update state for the next request, if any.
        bytes_read += rsize;
        offset     += rsize;
    }
    return ERROR_SUCCESS;
}

/// @summary Synchronously writes data to a file.
/// @param driver The virtual file system driver used to open the file.
/// @param file The file state returned from vfs_open_file().
/// @param offset The absolute byte offset at which to start writing data.
/// @param buffer The data to be written to the file.
/// @param size The number of bytes to write to the file.
/// @param bytes_written On return, this value is set to the number of bytes actually written to the file.
/// @return ERROR_SUCCESS or a system error code.
public_function DWORD vfs_write_file_sync(vfs_driver_t *driver, vfs_file_t *file, int64_t offset, void const *buffer, size_t size, size_t &bytes_written)
{   UNREFERENCED_PARAMETER(driver);
    LARGE_INTEGER oldpos;
    LARGE_INTEGER newpos;
    oldpos.QuadPart  = 0;
    newpos.QuadPart  = offset;
    if (!SetFilePointerEx(file->Fildes, newpos, &oldpos, FILE_BEGIN))
    {   // unable to seek to the specified offset.
        bytes_written= 0;
        return GetLastError();
    }
    // safely write the requested number of bytes. for very large writes,
    // size > UINT32_MAX, the result is undefined, so possibly split the
    // write up into several sub-writes (though this case is unlikely...)
    uint8_t const *bufferp = (uint8_t const*) buffer;
    DWORD   error  = ERROR_SUCCESS;
    bytes_written  = 0;
    while  (bytes_written < size)
    {
        DWORD nwritten = 0;
        DWORD wsize    = 1024U * 1024U; // write 1MB
        if (size_t(wsize)   > (size - bytes_written))
        {   // write only the amount remaining.
            wsize =   (DWORD) (size - bytes_written);
        }
        if (WriteFile(file->Fildes, &bufferp[bytes_written], wsize, &nwritten, NULL))
        {   // the synchronous write completed successfully.
            bytes_written += nwritten;
        }
        else
        {   // an error occurred; save the error code.
            error = GetLastError(); break;
        }
    }
    return error;
}

/// @summary Writes data to a file asynchronously by submitting commands to the AIO driver. The input pointer file is stored in the Identifier field of each AIO result descriptor.
/// @param driver The virtual file system driver used to open the file.
/// @param file The file state returned from vfs_open_file().
/// @param offset The absolute byte offset at which to start writing data.
/// @param buffer The data to be written to the file.
/// @param size The number of bytes to write to the file.
/// @param status_flags Application-defined status flags to pass through with the request.
/// @param priority The priority value to assign to the request(s).
/// @param thread_alloc The FIFO node allocator used for submitting commands from the calling thread.
/// @param result_queue The SPSC unbounded FIFO where the completed write result will be placed.
/// @param result_alloc The FIFO node allocator used to write data to the result queue.
/// @return ERROR_SUCCESS or a system error code.
public_function DWORD vfs_write_file_async(vfs_driver_t *driver, vfs_file_t *file, int64_t offset, void const *buffer, size_t size, uint32_t status_flags, uint32_t priority, pio_aio_request_alloc_t *thread_alloc, aio_result_queue_t *result_queue=NULL, aio_result_alloc_t *result_alloc=NULL)
{
    if (file->OpenFlags & FILE_FLAG_NO_BUFFERING)
    {   // files opened for unbuffered I/O are subject to alignment restrictions.
        uintptr_t addr  = (uintptr_t) buffer;
        if ((offset & (file->SectorSize-1)) != 0)
        {   // the write offset is not properly aligned.
            return ERROR_INVALID_PARAMETER;
        }
        if ((size   & (file->SectorSize-1)) != 0)
        {   // the number of bytes to write is not a multiple of the sector size.
            return ERROR_INVALID_PARAMETER;
        }
        if ((addr   & (file->SectorSize-1)) != 0)
        {   // the buffer is not aligned to a sector boundary.
            return ERROR_INVALID_PARAMETER;
        }
    }

    uint8_t const *bufferp =(uint8_t const*) buffer;
    size_t bytes_written   = 0;
    while (bytes_written   < size)
    {
        DWORD wsize = 1024U * 1024U; // write 1MB
        if (size_t(wsize)   > (size - bytes_written))
        {   // write only the amount remaining.
            wsize =   (DWORD) (size - bytes_written);
        }
        // generate the I/O request for the AIO driver.
        aio_request_t req;
        req.CommandType  = AIO_COMMAND_WRITE;
        req.CloseFlags   = AIO_CLOSE_FLAGS_NONE;
        req.Fildes       = file->Fildes;
        req.DataAmount   = wsize;
        req.DataActual   = wsize;
        req.BaseOffset   = 0;
        req.FileOffset   = offset;
        req.DataBuffer   = (void*) &bufferp[bytes_written];
        req.Identifier   = (uintptr_t) file;
        req.ResultAlloc  = result_alloc;
        req.ResultQueue  = result_queue;
        req.StatusFlags  = status_flags;
        req.Priority     = priority;
        // push the request to the AIO driver through the PIO driver queue.
        pio_driver_explicit_io(driver->PIO, req, thread_alloc);
        // update state for the next request, if any.
        bytes_written   += wsize;
        offset          += wsize;
    }
    return ERROR_SUCCESS;
}

/// @summary Flush any buffered writes to the file and update file metadata.
/// @param driver The virtual file system driver that opened the file.
/// @param file The file file state populated by vfs_open_file.
/// @return ERROR_SUCCESS or a system error code.
public_function DWORD vfs_flush_file_sync(vfs_driver_t *driver, vfs_file_t *file)
{   UNREFERENCED_PARAMETER(driver);
    if (FlushFileBuffers(file->Fildes)) return ERROR_SUCCESS;
    else return GetLastError();
}

/// @summary Saves a file to disk. If the file exists, it is overwritten. This operation is performed entirely synchronously and will block the calling thread until the file is written. The file is guaranteed to have been either written successfully, or not at all.
/// @param driver The virtual file system driver to use for path resolution.
/// @param path The virtual path of the file to write.
/// @param data The contents of the file.
/// @param size The number of bytes to read from data and write to the file.
/// @return true if the operation was successful.
public_function bool vfs_put_file(vfs_driver_t *driver, char const *path, void const *data, int64_t size)
{
    return SUCCEEDED(vfs_resolve_and_save_file(driver, path, data, size, NULL));
}

/// @summary Load the entire contents of a file from disk. This operation is performed entirely synchronously and will block the calling thread until the entire file has been read. This function cannot read files larger than 4GB. Large files must use the streaming interface.
/// @param driver The virtual file system driver to use for path resolution.
/// @param path The virtual path of the file to load.
/// @param decoder_hint One of vfs_decoder_hint_e specifying the preferred decoder type, or VFS_DECODER_HINT_USE_DEFAULT to let the implementation decide.
/// @return The stream decoder that can be used to access the file data. When finished accessing the file data, call the stream_decoder_t::release() method to delete the stream decoder instance.
public_function stream_decoder_t* vfs_get_file(vfs_driver_t *driver, char const *path, int32_t decoder_hint)
{   // open the file and retrieve relevant information.
    vfs_file_t  file_info;
    char const *relpath = NULL;
    int32_t     usage       = VFS_USAGE_MANUAL_IO;
    uint32_t    file_hints  = VFS_FILE_HINT_UNBUFFERED;
    DWORD       open_result = vfs_resolve_and_open_file(driver, path, usage, file_hints, decoder_hint, &file_info, &relpath);
    if (FAILED (open_result)) return NULL;

    // add a reference to the decoder while we're using it:
    file_info.Decoder->addref();

    // files larger than 4GB cannot be loaded this way; they must be streamed in.
    // typically you would want to do that anyway, so this is not a problem.
    if (file_info.BaseSize > (UINT32_MAX - sizeof(uint32_t)))
    {   // this file is too large to be loaded into memory all at once.
        vfs_close_file(&file_info);
        return NULL;
    }

    // move the file pointer to the beginning of the logical file.
    LARGE_INTEGER start_pos;
    start_pos.QuadPart =  file_info.BaseOffset;
    if (!SetFilePointerEx(file_info.Fildes, start_pos, NULL, FILE_BEGIN))
    {   // can't seek to the start of the file; don't proceed with reading.
        vfs_close_file(&file_info);
        return NULL;
    }

    // initialize the buffer allocator internal to the stream decoder.
    // it will hold a single buffer with the entire file contents, plus
    // an extra four zero bytes at the end, in case the resulting 
    // encoded data is passed to a string processing function.
    size_t    file_size = (size_t) (file_info.BaseSize + sizeof(uint32_t));
    stream_decoder_t *d = file_info.Decoder;
    if (!d->InternalAllocator.reserve(file_size, file_size))
    {   // unable to allocate the necessary buffer space.
        vfs_close_file(&file_info);
        return NULL;
    }

    // the stream decoder will use its internal buffer allocator.
    d->BufferAllocator = &d->InternalAllocator;

    // grab the buffer and add the zero bytes at the end.
    DWORD     buffer_n =(DWORD    ) d->BufferAllocator->AllocSize;
    uint8_t  *buffer_p =(uint8_t *) d->BufferAllocator->get_buffer();
    uint32_t *buffer_z =(uint32_t*)(buffer_p + (file_size - sizeof(uint32_t)));
    *buffer_z = 0U;

    // synchronously read the file in 1MB chunks. for archive files, 
    // this may end up reading slightly more data than the actual 
    // size of the file, but only the actual size of the file is 
    // exposed to the caller in the returned stream decoder.
    DWORD   error   = ERROR_SUCCESS;
    DWORD   amount  = 0;
    while  (amount  < file_info.FileSize)
    {
        DWORD nread = 0;
        DWORD rsize = 1024U * 1024U;
        if (rsize   > buffer_n - amount)
        {   // read only up to the available buffer space.
            rsize   = buffer_n - amount;
        }
        if (ReadFile(file_info.Fildes, &buffer_p[amount], rsize, &nread, NULL))
        {   // the synchronous read completed successfully.
            amount += nread;
        }
        else
        {   // an error occurred; save the error code.
            error   = GetLastError();
            break;
        }
    }

    // now enqueue a completed read request in the stream decoder queue.
    fifo_node_t<aio_result_t> *result  = fifo_allocator_get(&d->AIOResultAlloc);
    result->Item.Fildes     =  file_info.Fildes;
    result->Item.OSError    =  error;
    if (SUCCEEDED(error))      result->Item.DataAmount = (uint32_t)file_info.FileSize;
    else                       result->Item.DataAmount = (uint32_t)0;
    result->Item.DataActual =  result->Item.DataAmount;
    result->Item.FileOffset =  0;
    result->Item.DataBuffer =  buffer_p;
    result->Item.Identifier = (uintptr_t)path;
    result->Item.StatusFlags=  STREAM_DECODE_STATUS_ENDOFSTREAM;
    result->Item.Priority   =  0;
    spsc_fifo_u_produce(&d->AIOResultQueue, result);
    d->addref(); // release()'d on the decoder thread.

    // add a reference for the caller (ref count = 4)
    // release the reference for the VFS driver (ref count = 3)
    d->addref(); vfs_close_file(&file_info);
    return d;
}

/// @summary Asynchronously loads a file by streaming it into memory as quickly as possible.
/// @param driver The virtual file system driver used to resolve the file path.
/// @param path A NULL-terminated UTF-8 string specifying the virtual file path.
/// @param id An application-defined identifier for the stream.
/// @param priority The priority value for the load, with higher numeric values representing higher priority.
/// @param user_hints A combination of vfs_file_hint_e specifying the preferred file behavior, or VFS_FILE_HINT_NONE (0) to let the implementation decide. These hints may not be honored.
/// @param decoder_hint One of vfs_decoder_hint_e specifying the preferred decoder type, or VFS_DECODER_HINT_USE_DEFAULT (0) to let the implementation decide.
/// @param thread_alloc_open The FIFO node allocator used to enqueue stream open commands to the PIO driver.
/// @param thread_alloc_control The FIFO node allocator used to enqueue stream control commands to the PIO driver. May be NULL if control is NULL.
/// @param control If non-NULL, on return, this structure is initialized with information necessary to control streaming of the file.
/// @return The stream decoder that can be used to access the file data. When finished accessing the file data, call the stream_decoder_t::release() method to delete the stream decoder instance.
public_function stream_decoder_t* vfs_load_file(vfs_driver_t *driver, char const *path, uintptr_t id, uint8_t priority, int32_t user_hints, int32_t decoder_hint, pio_sti_pending_alloc_t *thread_alloc_open, pio_sti_control_alloc_t *thread_alloc_control, stream_control_t *control)
{   // open the file and retrieve relevant information.
    vfs_file_t   file_info;
    char const  *relpath     = NULL;
    int32_t      usage       = VFS_USAGE_STREAM_IN_LOAD;
    uint32_t     file_hints  =(user_hints == VFS_FILE_HINT_NONE) ? VFS_FILE_HINT_UNBUFFERED | VFS_FILE_HINT_ASYNCHRONOUS : user_hints;
    DWORD        open_result = vfs_resolve_and_open_file(driver, path, usage, file_hints, decoder_hint, &file_info, &relpath);
    if (!SUCCESS(open_result)) return NULL;
    DWORD        asio_result = aio_driver_prepare(driver->AIO, file_info.Fildes);
    if (!SUCCESS(asio_result))
    {   // could not associate the file handle with the I/O completion port.
        vfs_close_file(&file_info);
        return NULL;
    }

    // the decoder allocates buffer space from the I/O system pool.
    file_info.Decoder->BufferAllocator = &driver->StreamBuffer;
    
    // initialize the stream control structure for the caller.
    if (control != NULL)
    {
        control->SID = id;
        control->PIO = driver->PIO;
        control->PIOAlloc    = thread_alloc_control;
        control->EncodedSize = file_info.BaseSize;
        control->DecodedSize = file_info.FileSize;
    }

    // push the stream-in request down to the PIO driver.
    // the PIO driver holds a reference to the decoder.
    pio_sti_request_t iocmd;
    iocmd.Identifier   = id;
    iocmd.StreamDecoder= file_info.Decoder;
    iocmd.Fildes       = file_info.Fildes;
    iocmd.SectorSize   = file_info.SectorSize;
    iocmd.BaseOffset   = file_info.BaseOffset;
    iocmd.BaseSize     = file_info.BaseSize;
    iocmd.IntervalNs   = 0;
    iocmd.StreamFlags  = PIO_STREAM_IN_FLAGS_LOAD;
    iocmd.BasePriority = priority;
    pio_driver_stream_in(driver->PIO, iocmd, thread_alloc_open); // +1=1
    
    // add a decoder reference for the caller:
    file_info.Decoder->addref(); // +1=2
    return file_info.Decoder;
}

/// @summary Asynchronously loads a file by streaming it into memory in fixed-size chunks, delivered to the decoder at a given interval. 
/// @param driver The virtual file system driver used to resolve the file path.
/// @param path A NULL-terminated UTF-8 string specifying the virtual file path.
/// @param id An application-defined identifier for the stream.
/// @param priority The priority value for the load, with higher numeric values representing higher priority.
/// @param user_hints A combination of vfs_file_hint_e specifying the preferred file behavior, or VFS_FILE_HINT_NONT (0) to let the implementation decide. These hints may not be honored.
/// @param decoder_hint One of vfs_decoder_hint_e specifying the preferred decoder type, or VFS_DECODER_HINT_USE_DEFAULT (0) to let the implementation decide.
/// @param interval_ns The desired data delivery interval, specified in nanoseconds.
/// @param chunk_size The minimum size of a single buffer delivered every interval, in bytes.
/// @param chunk_count The number of buffers to maintain. The system will fill the buffers as quickly as possible, but only deliver them at the delivery interval.
/// @param thread_alloc_open The FIFO node allocator used to enqueue stream open commands to the PIO driver.
/// @param thread_alloc_control The FIFO node allocator used to enqueue stream control commands to the PIO driver. May be NULL if control is NULL.
/// @param control If non-NULL, on return, this structure is initialized with information necessary to control streaming of the file.
/// @return The stream decoder that can be used to access the file data. When finished accessing the file data, call the stream_decoder_t::release() method to delete the stream decoder instance.
public_function stream_decoder_t* vfs_stream_file(vfs_driver_t *driver, char const *path, uintptr_t id, uint8_t priority, int32_t user_hints, int32_t decoder_hint, uint64_t interval_ns, size_t chunk_size, size_t chunk_count, pio_sti_pending_alloc_t *thread_alloc_open, pio_sti_control_alloc_t *thread_alloc_control, stream_control_t *control)
{   // open the file and retrieve relevant information.
    // interval-based delivery prefers buffered I/O, if possible.
    vfs_file_t   file_info;
    char const  *relpath     = NULL;
    int32_t      usage       = VFS_USAGE_STREAM_IN;
    uint32_t     file_hints  =(user_hints == VFS_FILE_HINT_NONE) ? VFS_FILE_HINT_ASYNCHRONOUS : user_hints;
    DWORD        open_result = vfs_resolve_and_open_file(driver, path, usage, file_hints, decoder_hint, &file_info, &relpath);
    if (!SUCCESS(open_result)) return NULL;
    DWORD        asio_result = aio_driver_prepare(driver->AIO, file_info.Fildes);
    if (!SUCCESS(asio_result))
    {   // could not associate the file handle with the I/O completion port.
        vfs_close_file(&file_info);
        return NULL;
    }

    // set up the buffer allocator. allocate a fixed number of chunk_size chunks.
    if (!file_info.Decoder->InternalAllocator.reserve(chunk_size * chunk_count, chunk_size))
    {   // unable to reserve the requested amount of buffer space.
        vfs_close_file(&file_info);
        return NULL;
    }
    if ((file_info.OpenFlags & FILE_FLAG_NO_BUFFERING) == 0)
    {   // using buffered I/O, we can deliver exactly the requested chunk size
        // as there are no sector-alignment restrictions on offsets or sizes.
        file_info.Decoder->InternalAllocator.AllocSize  = chunk_size;
    }
    file_info.Decoder->BufferAllocator = &file_info.Decoder->InternalAllocator;
    
    // initialize the stream control structure for the caller.
    if (control != NULL)
    {
        control->SID = id;
        control->PIO = driver->PIO;
        control->PIOAlloc    = thread_alloc_control;
        control->EncodedSize = file_info.BaseSize;
        control->DecodedSize = file_info.FileSize;
    }

    // push the stream-in request down to the PIO driver.
    // the PIO driver holds a reference to the decoder.
    pio_sti_request_t iocmd;
    iocmd.Identifier   = id;
    iocmd.StreamDecoder= file_info.Decoder;
    iocmd.Fildes       = file_info.Fildes;
    iocmd.SectorSize   = file_info.SectorSize;
    iocmd.BaseOffset   = file_info.BaseOffset;
    iocmd.BaseSize     = file_info.BaseSize;
    iocmd.IntervalNs   = interval_ns;
    iocmd.StreamFlags  = PIO_STREAM_IN_FLAGS_LOAD;
    iocmd.BasePriority = priority;
    pio_driver_stream_in(driver->PIO, iocmd, thread_alloc_open); // +1=1
    
    // add a decoder reference for the caller:
    file_info.Decoder->addref(); // +1=2
    return file_info.Decoder;
}
