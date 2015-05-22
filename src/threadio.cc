/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines the high-level interface to the I/O system for a thread.
/// The thread_io_t class maintains all of the necessary allocators for 
/// interfacing with the VFS, PIO and AIO drivers in a safe manner. Each thread
/// that needs to perform I/O creates an instance of thread_io_t.
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

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Defines the high-level interface to the I/O system for a thread.
struct thread_io_t
{
    thread_io_t(void);
    ~thread_io_t(void);

    void                    initialize(
        vfs_driver_t       *vfs
    );                                        /// Set the target driver interfaces to use.

    bool                    mount(
        int                 path_id, 
        char const         *mount_path, 
        uint32_t            priority, 
        uintptr_t           mount_id
    );                                        /// Mount a well-known path in the virtual file system.

    bool                    mount(
        char const         *native_path, 
        char const         *mount_path, 
        uint32_t            priority, 
        uintptr_t           mount_id
    );                                        /// Mount a native filesystem path in the virtual file system.

    bool                    mountv(
        char const         *virtual_path, 
        char const         *mount_path, 
        uint32_t            priority, 
        uintptr_t           mount_id
    );                                        /// Mount a virtual path in the virtual file system.

    void                    unmount_all(
        char const         *mount_path
    );                                        /// Delete all mounts at a given mount path.

    void                    unmount(
        uintptr_t           mount_id
    );                                        /// Delete a specific mount point.

    bool                    put_file(
        char const         *virtual_path, 
        void const         *data, 
        int64_t             size_in_bytes
    );                                        /// Synchronously and atomically write an entire file.

    stream_decoder_t*       get_file(
        char const         *virtual_path, 
        int                 decoder_hint
    );                                        /// Synchronously read an entire file.

    stream_decoder_t*       load_file(
        char const         *virtual_path, 
        int                 file_hints, 
        int                 decoder_hint, 
        uintptr_t           stream_id, 
        uint8_t             priority, 
        stream_control_t   *control
    );                                        /// Asynchronously stream a file into memory, then close it.

    stream_decoder_t*       stream_file(
        char const         *virtual_path, 
        int                 file_hints, 
        int                 decoder_hint, 
        uintptr_t           stream_id,
        uint8_t             priority, 
        uint64_t            interval_ns, 
        size_t              chunk_size, 
        size_t              chunk_count,
        stream_control_t   *control
    );                                        /// Asynchronously stream a file into memory with a fixed delivery interval.

    // TODO(rlk): add stream control helpers so you don't have to have a stream_control_t instance.

    pio_sti_control_alloc_t PIOControlAlloc;  /// The stream-in control allocator for the thread.
    pio_sti_pending_alloc_t PIOStreamInAlloc; /// The stream-in request allocator for the thread.

    vfs_driver_t           *VFSDriver;        /// The target VFS driver, managed externally.
    pio_driver_t           *PIODriver;        /// The target PIO driver, managed externally.
    aio_driver_t           *AIODriver;        /// The target AIO driver, managed externally.
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
/// @summary Constructs an uninitialized thread I/O interface.
/// The thread_io_t::initialize() method must be called prior to use.
thread_io_t::thread_io_t(void)
    : 
    VFSDriver(NULL), 
    PIODriver(NULL), 
    AIODriver(NULL)
{
    fifo_allocator_init(&PIOControlAlloc);
    fifo_allocator_init(&PIOStreamInAlloc);
}

/// @summary Frees resources allocated by the thread I/O interface.
thread_io_t::~thread_io_t(void)
{
    fifo_allocator_reinit(&PIOStreamInAlloc);
    fifo_allocator_reinit(&PIOControlAlloc);
}

/// @summary Initialize the thread I/O interface and set the virtual file system driver used as the source and target of I/O operations.
/// @param vfs The virtual file system driver to use as the source and target for I/O operations.
void thread_io_t::initialize(vfs_driver_t *vfs)
{
    VFSDriver = vfs;
    PIODriver = vfs->PIO;
    AIODriver = vfs->AIO;
}

/// @summary Creates a mount point backed by a well-known directory.
/// @param folder_id One of vfs_known_path_e specifying the well-known directory.
/// @param mount_path The NULL-terminated UTF-8 string specifying the root of the mount point as exposed to the application code. Multiple mount points may share the same mount_path, but have different source_path, mount_id and priority values.
/// @param priority The priority assigned to this specific mount point, with higher numeric values corresponding to higher priority values.
/// @param mount_id A unique application-defined identifier for the mount point. This identifier can be used to reference the specific mount point.
/// @return true if the mount point was successfully created.
bool thread_io_t::mount(int folder_id, char const *mount_path, uint32_t priority, uintptr_t mount_id)
{
    return vfs_mount_known(VFSDriver, folder_id, mount_path, priority, mount_id);
}

/// @summary Creates a mount point backed by the specified archive file or directory.
/// @param native_path The NULL-terminated UTF-8 string specifying the path of the file or directory that represents the root of the mount point.
/// @param mount_path The NULL-terminated UTF-8 string specifying the root of the mount point as exposed to the application code. Multiple mount points may share the same mount_path, but have different source_path, mount_id and priority values.
/// @param priority The priority assigned to this specific mount point, with higher numeric values corresponding to higher priority values.
/// @param mount_id A unique application-defined identifier for the mount point. This identifier can be used to reference the specific mount point.
/// @return true if the mount point was successfully created.
bool thread_io_t::mount(char const *native_path, char const *mount_path, uint32_t priority, uintptr_t mount_id)
{
    return vfs_mount_native(VFSDriver, native_path, mount_path, priority, mount_id);
}

/// @summary Creates a mount point backed by the specified archive or directory, specified as a virtual path.
/// @param virtual_path The NULL-terminated UTF-8 string specifying the virtual path of the file or directory that represents the root of the mount point.
/// @param mount_path The NULL-terminated UTF-8 string specifying the root of the mount point as exposed to the application code. Multiple mount points may share the same mount_path, but have different source_path, mount_id and priority values.
/// @param priority The priority assigned to this specific mount point, with higher numeric values corresponding to higher priority values.
/// @param mount_id A unique application-defined identifier for the mount point. This identifier can be used to reference the specific mount point.
/// @return true if the mount point was successfully created.
bool thread_io_t::mountv(char const *virtual_path, char const *mount_path, uint32_t priority, uintptr_t mount_id)
{
    return vfs_mount_virtual(VFSDriver, virtual_path, mount_path, priority, mount_id);
}

/// @summary Deletes all mount points attached to a given root path.
/// @param driver The virtual file system driver.
/// @param mount_path A NULL-terminated UTF-8 string specifying the mount point root, 
/// as was supplied to vfs_mount(). All mount points that were mounted to this path will be deleted.
void thread_io_t::unmount_all(char const *mount_path)
{
    vfs_unmount_all(VFSDriver, mount_path);
}

/// @summary Removes a mount point associated with a specific application mount point identifier.
/// @param mount_id The application-defined mount point identifer, as supplied to vfs_mount().
void thread_io_t::unmount(uintptr_t mount_id)
{
    vfs_unmount(VFSDriver, mount_id);
}

/// @summary Saves a file to disk. If the file exists, it is overwritten. This operation is performed entirely synchronously and will block the calling thread until the file is written. The file is guaranteed to have been either written successfully, or not at all.
/// @param driver The virtual file system driver to use for path resolution.
/// @param path The virtual path of the file to write.
/// @param data The contents of the file.
/// @param size The number of bytes to read from data and write to the file.
/// @return true if the operation was successful.
bool thread_io_t::put_file(char const *virtual_path, void const *data, int64_t size_in_bytes)
{
    return vfs_put_file(VFSDriver, virtual_path, data, size_in_bytes);
}

/// @summary Load the entire contents of a file from disk. This operation is performed entirely synchronously and will block the calling thread until the entire file has been read. This function cannot read files larger than 4GB. Large files must use the streaming interface.
/// @param path The virtual path of the file to load.
/// @param decoder_hint One of vfs_decoder_hint_e specifying the preferred decoder type, or VFS_DECODER_HINT_USE_DEFAULT to let the implementation decide.
/// @return The stream decoder that can be used to access the file data. When finished accessing the file data, call the stream_decoder_t::release() method to delete the stream decoder instance.
stream_decoder_t* thread_io_t::get_file(char const *virtual_path, int decoder_hint)
{
    return vfs_get_file(VFSDriver, virtual_path, decoder_hint);
}

/// @summary Asynchronously loads a file by streaming it into memory as quickly as possible.
/// @param virtual_path A NULL-terminated UTF-8 string specifying the virtual file path.
/// @param file_hints A combination of vfs_file_hint_e specifying the preferred file behavior, or VFS_FILE_HINT_NONT (0) to let the implementation decide. These hints may not be honored.
/// @param decoder_hint One of vfs_decoder_hint_e specifying the preferred decoder type, or VFS_DECODER_HINT_USE_DEFAULT (0) to let the implementation decide.
/// @param stream_id An application-defined identifier for the stream.
/// @param priority The priority value for the load, with higher numeric values representing higher priority.
/// @param control If non-NULL, on return, this structure is initialized with information necessary to control streaming of the file.
/// @return The stream decoder that can be used to access the file data. When finished accessing the file data, call the stream_decoder_t::release() method to delete the stream decoder instance.
stream_decoder_t* thread_io_t::load_file(char const *virtual_path, int file_hints, int decoder_hint, uintptr_t stream_id, uint8_t priority, stream_control_t *control)
{
    return vfs_load_file(VFSDriver, virtual_path, stream_id, priority, file_hints, decoder_hint, &PIOStreamInAlloc, &PIOControlAlloc, control);
}

/// @summary Asynchronously loads a file by streaming it into memory in fixed-size chunks, delivered to the decoder at a given interval. 
/// @param virtual_path A NULL-terminated UTF-8 string specifying the virtual file path.
/// @param file_hints A combination of vfs_file_hint_e specifying the preferred file behavior, or VFS_FILE_HINT_NONT (0) to let the implementation decide. These hints may not be honored.
/// @param decoder_hint One of vfs_decoder_hint_e specifying the preferred decoder type, or VFS_DECODER_HINT_USE_DEFAULT (0) to let the implementation decide.
/// @param stream_id An application-defined identifier for the stream.
/// @param priority The priority value for the load, with higher numeric values representing higher priority.
/// @param interval_ns The desired data delivery interval, specified in nanoseconds.
/// @param chunk_size The minimum size of a single buffer delivered every interval, in bytes.
/// @param chunk_count The number of buffers to maintain. The system will fill the buffers as quickly as possible, but only deliver them at the delivery interval.
/// @param control If non-NULL, on return, this structure is initialized with information necessary to control streaming of the file.
/// @return The stream decoder that can be used to access the file data. When finished accessing the file data, call the stream_decoder_t::release() method to delete the stream decoder instance.
stream_decoder_t* thread_io_t::stream_file(char const *virtual_path, int file_hints, int decoder_hint, uintptr_t stream_id, uint8_t priority, uint64_t interval_ns, size_t chunk_size, size_t chunk_count, stream_control_t *control)
{
    return vfs_stream_file(VFSDriver, virtual_path, stream_id, priority, file_hints, decoder_hint, interval_ns, chunk_size, chunk_count, &PIOStreamInAlloc, &PIOControlAlloc, control);
}
