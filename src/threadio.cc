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

    void                    initialize
    (
        vfs_driver_t       *vfs
    );                                        /// Set the target driver interfaces to use.

    bool                    mount
    (
        int                 path_id, 
        char const         *mount_path, 
        uint32_t            priority, 
        uintptr_t           mount_id
    );                                        /// Mount a well-known path in the virtual file system.

    bool                    mount
    (
        char const         *native_path, 
        char const         *mount_path, 
        uint32_t            priority, 
        uintptr_t           mount_id
    );                                        /// Mount a native filesystem path in the virtual file system.

    bool                    mountv
    (
        char const         *virtual_path, 
        char const         *mount_path, 
        uint32_t            priority, 
        uintptr_t           mount_id
    );                                        /// Mount a virtual path in the virtual file system.

    void                    unmount_all
    (
        char const         *mount_path
    );                                        /// Delete all mounts at a given mount path.

    void                    unmount
    (
        uintptr_t           mount_id
    );                                        /// Delete a specific mount point.

    DWORD                   open_file
    (
        char const         *virtual_path, 
        uint32_t            file_hints, 
        int32_t             decoder_hint, 
        vfs_file_t         *file
    );                                        /// Synchronously open a file for manual I/O.

    DWORD                   read_sync
    (
        vfs_file_t         *file, 
        int64_t             offset,
        void               *buffer,
        size_t              size, 
        size_t             &bytes_read
    );                                        /// Synchronously read data from a file opened for manual I/O.

    DWORD                   write_sync
    (
        vfs_file_t         *file, 
        int64_t             offset, 
        void const         *buffer,
        size_t              size, 
        size_t             &bytes_written
    );                                        /// Synchronously write data to a file opened for manual I/O.

    DWORD                   flush_sync
    (
        vfs_file_t         *file
    );                                        /// Synchronously flush buffered writes and file metadata.

    DWORD                   read_async
    (
        vfs_file_t         *file,
        int64_t             offset, 
        void               *buffer, 
        size_t              size, 
        uint32_t            priority,
        uint32_t            close_flags, 
        aio_result_queue_t *result_queue=NULL,
        aio_result_alloc_t *result_alloc=NULL
    );                                        /// Asynchronously read data from a file opened for manual I/O.

    DWORD                   write_async
    (
        vfs_file_t         *file, 
        int64_t             offset,
        void const         *buffer, 
        size_t              size, 
        uint32_t            priority, 
        uint32_t            status_flags, 
        aio_result_queue_t *result_queue=NULL, 
        aio_result_alloc_t *result_alloc=NULL
    );                                        /// Asynchronously write data to a file opened for manual I/O.

    void                    close_file
    (
        vfs_file_t         *file
    );                                        /// Synchronously close a file opened for manual I/O.

    bool                    put_file
    (
        char const         *virtual_path, 
        void const         *data, 
        int64_t             size_in_bytes
    );                                        /// Synchronously and atomically write an entire file.

    stream_decoder_t*       get_file
    (
        char const         *virtual_path, 
        int                 decoder_hint
    );                                        /// Synchronously read an entire file.

    stream_decoder_t*       load_file
    (
        char const         *virtual_path, 
        int                 file_hints, 
        int                 decoder_hint, 
        uintptr_t           stream_id, 
        uint8_t             priority, 
        stream_control_t   *control
    );                                        /// Asynchronously stream a file into memory, then close it.

    stream_decoder_t*       stream_file
    (
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

    void                    pause_stream
    (
        uintptr_t           stream_id
    );                                        /// Suspend stream-in, but do not close the stream.

    void                    resume_stream
    (
        uintptr_t           stream_id
    );                                        /// Resume stream-in of a paused stream.

    void                    rewind_stream
    (
        uintptr_t           stream_id
    );                                        /// Seek to the beginning of a stream and resume stream-in.

    void                    seek_stream
    (
        uintptr_t           stream_id, 
        int64_t             absolute_offset
    );                                        /// Seek to a byte offset in a stream and resume stream-in.

    void                    stop_stream
    (
        uintptr_t           stream_id
    );                                        /// Stop stream-in and close the stream.

    pio_sti_control_alloc_t PIOControlAlloc;  /// The stream-in control allocator for the thread.
    pio_sti_pending_alloc_t PIOStreamInAlloc; /// The stream-in request allocator for the thread.
    pio_aio_request_alloc_t PIOManualIoAlloc; /// The manual I/O request allocator for the thread.

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
    fifo_allocator_init(&PIOManualIoAlloc);
}

/// @summary Frees resources allocated by the thread I/O interface.
thread_io_t::~thread_io_t(void)
{
    fifo_allocator_reinit(&PIOManualIoAlloc);
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

/// @summary Open a file for manual I/O. Close the file using vfs_close_file().
/// @param virtual_path A NULL-terminated UTF-8 string specifying the virtual file path.
/// @param file_hints A combination of vfs_file_hint_e specifying how to open the file. The file is opened for reading and writing.
/// @param decoder_hint One of vfs_decoder_hint_e specifying the type of stream decoder to create, or VFS_DECODER_HINT_NONE to not create a decoder.
/// @param file On return, this structure is populated with file information.
/// @return ERROR_SUCCESS or a system error code.
DWORD thread_io_t::open_file(char const *virtual_path, uint32_t file_hints, int32_t decoder_hint, vfs_file_t *file)
{
    return vfs_open_file(VFSDriver, virtual_path, file_hints, decoder_hint, file);
}

/// @summary Synchronously reads data from a file.
/// @param file The file to read from.
/// @param offset The absolute byte offset at which to start reading data.
/// @param buffer The buffer into which data will be written.
/// @param size The maximum number of bytes to read.
/// @param bytes_read On return, this value is set to the number of bytes actually read. This may be less than the number of bytes requested, or 0 at end-of-file.
/// @return ERROR_SUCCESS or a system error code.
DWORD thread_io_t::read_sync(vfs_file_t *file, int64_t offset, void *buffer, size_t size, size_t &bytes_read)
{
    return vfs_read_file_sync(VFSDriver, file, offset, buffer, size, bytes_read);
}

/// @summary Synchronously writes data to a file.
/// @param file The file to write to.
/// @param offset The absolute byte offset at which to start writing data.
/// @param buffer The data to be written to the file.
/// @param size The number of bytes to write to the file.
/// @param bytes_written On return, this value is set to the number of bytes actually written to the file.
/// @return ERROR_SUCCESS or a system error code.
DWORD thread_io_t::write_sync(vfs_file_t *file, int64_t offset, void const *buffer, size_t size, size_t &bytes_written)
{
    return vfs_write_file_sync(VFSDriver, file, offset, buffer, size, bytes_written);
}

/// @summary Flush any buffered writes to the file and update file metadata.
/// @param file The file to flush.
/// @return ERROR_SUCCESS or a system error code.
DWORD thread_io_t::flush_sync(vfs_file_t *file)
{
    return vfs_flush_file_sync(VFSDriver, file);
}

/// @summary Reads data from a file asynchronously by submitting commands to the AIO driver. The input pointer file is stored in the Identifier field of each AIO result descriptor.
/// @param file The file to read from.
/// @param offset The absolute byte offset at which to start reading data.
/// @param buffer The buffer into which data will be written.
/// @param size The maximum number of bytes to read.
/// @param priority The priority value to assign to the read request(s).
/// @param close_flags One of aio_close_flags_e specifying the auto-close behavior.
/// @param result_queue The SPSC unbounded FIFO where the completed read result will be placed.
/// @param result_alloc The FIFO node allocator used to write data to the result queue.
/// @return ERROR_SUCCESS or a system error code.
DWORD thread_io_t::read_async(vfs_file_t *file, int64_t offset, void *buffer, size_t size, uint32_t priority, uint32_t close_flags, aio_result_queue_t *result_queue, aio_result_alloc_t *result_alloc)
{
    return vfs_read_file_async(VFSDriver, file, offset, buffer, size, close_flags, priority, &PIOManualIoAlloc, result_queue, result_alloc);
}

/// @summary Writes data to a file asynchronously by submitting commands to the AIO driver. The input pointer file is stored in the Identifier field of each AIO result descriptor.
/// @param file The file to write to.
/// @param offset The absolute byte offset at which to start writing data.
/// @param buffer The data to be written to the file.
/// @param size The number of bytes to write to the file.
/// @param priority The priority value to assign to the request(s).
/// @param status_flags Application-defined status flags to pass through with the request.
/// @param result_queue The SPSC unbounded FIFO where the completed write result will be placed.
/// @param result_alloc The FIFO node allocator used to write data to the result queue.
/// @return ERROR_SUCCESS or a system error code.
DWORD thread_io_t::write_async(vfs_file_t *file, int64_t offset, void const *buffer, size_t size, uint32_t priority, uint32_t status_flags, aio_result_queue_t *result_queue, aio_result_alloc_t *result_alloc)
{
    return vfs_write_file_async(VFSDriver, file, offset, buffer, size, status_flags, priority, &PIOManualIoAlloc, result_queue, result_alloc);
}

/// @summary Closes the underlying file handle and releases the VFS driver's 
/// reference to the underlying stream decoder.
/// @param file_info Internal information relating to the file to close.
void thread_io_t::close_file(vfs_file_t *file)
{
    vfs_close_file(file);
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

/// @summary Pause stream-in of a given stream.
/// @param stream_id The application-defined identifier of the stream.
void thread_io_t::pause_stream(uintptr_t stream_id)
{
    pio_driver_pause_stream(PIODriver, stream_id, &PIOControlAlloc);
}

/// @summary Resume stream-in from the previous read location.
/// @param stream_id The application-defined identifier of the stream.
void thread_io_t::resume_stream(uintptr_t stream_id)
{
    pio_driver_resume_stream(PIODriver, stream_id, &PIOControlAlloc);
}

/// @summary Seek to the beginning of a stream and resume stream-in.
/// @param stream_id The application-defined identifier of the stream.
void thread_io_t::rewind_stream(uintptr_t stream_id)
{
    pio_driver_rewind_stream(PIODriver, stream_id, &PIOControlAlloc);
}

/// @summary Seek to a given location in a stream, and resume stream-in from that point. Note that due to alignment restrictions, stream-in may resume from before the specified byte offset.
/// @param stream_id The application-defined identifier of the stream.
/// @param absolute_offset The byte offset to seek to, relative to the start of the stream.
void thread_io_t::seek_stream(uintptr_t stream_id, int64_t absolute_offset)
{
    pio_driver_seek_stream(PIODriver, stream_id, absolute_offset, &PIOControlAlloc);
}

/// @summary Halt stream-in for a stream, and close the underlying file.
/// @param stream_id The application-defined identifier of the stream.
void thread_io_t::stop_stream(uintptr_t stream_id)
{
    pio_driver_stop_stream(PIODriver, stream_id, &PIOControlAlloc);
}
