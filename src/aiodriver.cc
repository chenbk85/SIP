/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines the types, constants and functions that implement the 
/// asynchronous I/O driver. On the Windows platform, the asynchronous I/O 
/// driver is implemented using overlapped I/O and I/O completion ports. 
/// The AIO driver exposes a bounded SPSC command queue and posts results to 
/// an unbounded SPSC queue.
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
/// @summary Define the maximum number of concurrently active AIO operations.
/// We set this based on what the maximum number of AIO operations we want to
/// poll during each tick, and the maximum number the underlying OS can handle.
/// This value must be a power of two greater than zero.
#ifndef WINDOWS_AIO_MAX_ACTIVE    /// Can override at compile time
#define WINDOWS_AIO_MAX_ACTIVE        128
#endif

/// @summary The special I/O completion port completion key used to notify the
/// AIO driver to begin its shutdown process.
static ULONG_PTR const AIO_SHUTDOWN = ~ULONG_PTR(0);

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Define the supported AIO driver commands.
enum aio_command_type_e : int32_t
{
    AIO_COMMAND_READ        = 0,  /// Data should be read from the file.
    AIO_COMMAND_WRITE       = 1,  /// Data should be written to the file.
    AIO_COMMAND_FLUSH       = 2,  /// Any pending writes should be flushed to disk.
    AIO_COMMAND_CLOSE       = 3,  /// The file should be closed.
    AIO_COMMAND_CLOSE_TEMP  = 4,  /// The file should be closed and renamed.
    AIO_COMMAND_COUNT       = 5   /// The number of recognized commands.
};

/// @summary Define bitflags that are used to specify file handle close 
/// behavior when an asynchronous I/O operation has completed.
enum aio_close_flags_e : uint32_t
{
    AIO_CLOSE_FLAGS_NONE    = (0 << 0), /// Do not close the underlying file handle.
    AIO_CLOSE_ON_ERROR      = (1 << 0), /// Close the underlying file handle if an error occurs.
    AIO_CLOSE_ON_COMPLETE   = (1 << 1), /// Close the underlying file handle when the request has completed.
};

/// @summary Defines the data associated with a completed AIO operation. The
/// operation may have completed successfully, or may have completed in error.
struct aio_result_t
{
    HANDLE          Fildes;       /// The file descriptor of the file.
    DWORD           OSError;      /// The error code returned by the operation, or ERROR_SUCCESS.
    uint32_t        DataAmount;   /// The amount of data transferred.
    uint32_t        DataActual;   /// The amount of data that's valid to read.
    int64_t         FileOffset;   /// The absolute byte offset of the start of the operation, or 0.
    void           *DataBuffer;   /// The source or target buffer, or NULL.
    uintptr_t       Identifier;   /// The application-defined ID for the file.
    uint32_t        StatusFlags;  /// The application-defined status flags for the operation.
    uint32_t        Priority;     /// The application-defined priority value for the operation.
};

/// @summary Defines the data associated with an AIO driver command to read, write,
/// flush or close a file. These commands are transformed into an iocb structure.
/// The maximum amount of data transferred in any single operation is 4GB.
struct aio_request_t
{   typedef aio_result_t     res_t;
    typedef spsc_fifo_u_t   <res_t>   result_queue_t;
    typedef fifo_allocator_t<res_t>   result_alloc_t;
    int32_t         CommandType;  /// The AIO command type, one of aio_command_type_e.
    uint32_t        CloseFlags;   /// A combination of aio_close_flags_e.
    HANDLE          Fildes;       /// The file descriptor of the file. Required.
    uint32_t        DataAmount;   /// The amount of data to transfer, or 0. Subject to alignment requirements.
    uint32_t        DataActual;   /// The amount of data that's actually valid. Not subject to alignment requirements.
    int64_t         BaseOffset;   /// The absolute byte offset of the start of the file, or 0.
    int64_t         FileOffset;   /// The absolute byte offset of the start of the operation, or 0.
    void           *DataBuffer;   /// The source or target buffer, or NULL.
    uintptr_t       Identifier;   /// The application-defined ID for the file. Passthrough.
    result_alloc_t *ResultAlloc;  /// The FIFO node allocator for the result queue. Required.
    result_queue_t *ResultQueue;  /// The SPSC unbounded FIFO that will receive the result of the operation. Required.
    uint32_t        StatusFlags;  /// The application-defined status flags for the operation. Passthrough.
    uint32_t        Priority;     /// The application-defined priority value for the operation. Passthrough.
};

/// @summary Defines the data associated with the asynchronous I/O (AIO) driver.
/// The AIO driver typically executes on a background thread due to the blocking
/// nature of some operations. Sitting above the AIO driver is the prioritized I/O
/// (PIO) layer, which implements higher-level logic for I/O prioritization,
/// throttling, streaming and buffer management.
struct aio_driver_t
{   typedef aio_request_t    req_t;
    typedef spsc_fifo_b_t   <req_t>   request_queue_t;
    static  size_t const     N      = WINDOWS_AIO_MAX_ACTIVE;
    request_queue_t RequestQueue; /// The bounded queue of pending requests from the PIO driver.
    HANDLE          AIOContext;   /// The kernel AIO context descriptor.
    size_t          ActiveCount;  /// The number of in-flight AIO operations.
    OVERLAPPED     *IOCBList[N];  /// The list of in-use kernel iocb descriptors.
    aio_request_t   AIOCList[N];  /// The list of active AIO command descriptors.
    OVERLAPPED     *IOCBFree[N];  /// The list of unused kernel iocb descriptors.
    OVERLAPPED      IOCBPool[N];  /// The pool of kernel iocb descriptors.
};

/// @summary Some typedefs for user code to hide template ugliness.
typedef lplc_fifo_u_t   <aio_request_t> aio_request_queue_t;
typedef spsc_fifo_u_t   <aio_result_t>  aio_result_queue_t;
typedef fifo_allocator_t<aio_result_t>  aio_result_alloc_t;
typedef mpsc_fifo_u_t   <void*>         aio_return_queue_t;
typedef fifo_allocator_t<void*>         aio_return_alloc_t;

/*///////////////
//   Globals   //
///////////////*/

/*///////////////////////
//   Local Functions   //
///////////////////////*/
/// @summary Initialize an aio_result_t with data in response to a completed
/// command and post the result to the target result queue.
/// @param cmd The AIO driver command that completed.
/// @param oserr The system error code result (0 indicates success/no error.)
/// @param amount The number of bytes transferred, or 0.
/// @return The input value oserr.
internal_function DWORD aio_driver_post_result(aio_request_t const &cmd, DWORD oserr, uint32_t amount)
{   // post the operation result to the result queue specified in the request.
    fifo_node_t<aio_result_t> *node = fifo_allocator_get(cmd.ResultAlloc);
    aio_result_t &res = node->Item;
    res.Fildes        = cmd.Fildes;
    res.OSError       = oserr;
    res.DataAmount    = amount;
    res.DataActual    = cmd.DataActual;
    res.FileOffset    = cmd.FileOffset;
    res.DataBuffer    = cmd.DataBuffer;
    res.Identifier    = cmd.Identifier;
    res.StatusFlags   = cmd.StatusFlags;
    res.Priority      = cmd.Priority;
    spsc_fifo_u_produce(cmd.ResultQueue, node);
    // close the underlying file handle, if requested.
    if (cmd.CloseFlags != AIO_CLOSE_FLAGS_NONE)
    {
        if ( ((cmd.CloseFlags & AIO_CLOSE_ON_COMPLETE) != 0) ||
            (((cmd.CloseFlags & AIO_CLOSE_ON_ERROR   ) != 0) && FAILED(oserr)) )
        {   // the close condition has been satisfied.
            CloseHandle(cmd.Fildes);
        }
    }
    return oserr;
}

/// @summary Synchronously processes a file flush operation.
/// @param cmd The data associated with the flush operation.
/// @return Either ERROR_SUCCESS or the result of calling GetLastError().
internal_function DWORD aio_driver_flush_file(aio_request_t &cmd)
{   // synchronously flush all pending writes to the file.
    DWORD error = ERROR_SUCCESS;
    BOOL result = FlushFileBuffers(cmd.Fildes);
    if (!result)  error = GetLastError();
    // generate the completion and push it to the queue.
    return aio_driver_post_result(cmd, error, 0);
}

/// @summary Synchronously process a file close command. This closes both the
/// file handle and the eventfd handle, and posts the result to the target queue.
/// @param cmd The data associated with the close operation.
/// @return Either ERROR_SUCCESS or the result of calling GetLastError().
internal_function DWORD aio_driver_close_file(aio_request_t &cmd)
{   // close the file descriptors associated with the file.
    cmd.CloseFlags |= AIO_CLOSE_ON_COMPLETE;
    // generate the completion result and push it to the queue.
    return aio_driver_post_result(cmd, ERROR_SUCCESS, 0);
}

/// @summary Synchronously process a finalize command. This closes both the
/// file handle and the eventfd handle, and posts the result to the target queue.
/// If the command data buffer is NULL, the temporary file is deleted; otherwise,
/// the temporary file is moved to the path specified by the command data buffer.
/// @param cmd The data associated with the close-and-rename operation.
/// @return Either ERROR_SUCCESS or the result of calling GetLastError().
internal_function DWORD aio_driver_close_and_rename(aio_request_t &cmd)
{
    DWORD error   = 0;    // saved return value from GetLastError().
    DWORD ncharsp = 0;    // number of characters in source path; GetFinalPathNameByHandle().
    size_t  ssize = 0;    // the disk physical sector size, in bytes.
    int64_t lsize = 0;    // the logical file size, in bytes
    int64_t psize = 0;    // the physical disk allocation size, in bytes.
    WCHAR *target = (WCHAR*) cmd.DataBuffer;
    WCHAR *source = NULL;
    FILE_END_OF_FILE_INFO eof;

    // get the absolute path of the temporary file (the source file).
    ncharsp = GetFinalPathNameByHandle(cmd.Fildes, NULL, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    source  = (WCHAR*) malloc(ncharsp * sizeof(WCHAR));
    if (ncharsp == 0 || source == NULL)
    {   // couldn't allocate the temporary buffer for the source path.
        goto error_cleanup;
    }
    GetFinalPathNameByHandle(cmd.Fildes, source, ncharsp, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);

    // is the temporary file being deleted, or is it being moved?
    if (target == NULL)
    {   // handle the simple case of deleting the temporary file.
        CloseHandle(cmd.Fildes); DeleteFile(source); free(source);
        return aio_driver_post_result(cmd, ERROR_SUCCESS, 0);
    }

    // flush all pending writes to the file.
    FlushFileBuffers(cmd.Fildes);
    // the file needs to be moved into place. set the file size.
    lsize = cmd.FileOffset;
    eof.EndOfFile.QuadPart = lsize;
    ssize = physical_sector_size(cmd.Fildes);
    psize = align_up(cmd.FileOffset, ssize);
    SetFileInformationByHandle(cmd.Fildes, FileEndOfFileInfo, &eof, sizeof(eof));
    SetFileValidData(cmd.Fildes, eof.EndOfFile.QuadPart); // requires elevate_process_privileges().

    // close the open file handle, and move the file into place.
    // note that goto error_cleanup can't be used after this point,
    // since we have already closed the file handle.
    CloseHandle(cmd.Fildes);
    if (!MoveFileEx(source, target, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {   // unable to move the file to the target location.
        error = GetLastError();
        DeleteFile(source); free(source); free(target);
        return aio_driver_post_result(cmd, error, 0);
    }

    // finally, we're done. complete the operation successfully.
    free(source); free(target);
    return aio_driver_post_result(cmd, ERROR_SUCCESS, 0);

error_cleanup:
    error = GetLastError();
    CloseHandle(cmd.Fildes);
    if (source != NULL) DeleteFile(source);
    if (source != NULL) free(source);
    if (target != NULL) free(target);
    return aio_driver_post_result(cmd, error, 0);
}

/// @summary Builds a read operation and submits it to the kernel.
/// @param driver The AIO driver state processing the AIO request.
/// @param cmd The data associated with the read operation. The file handle must
/// have been set to not notify the completion queue on synchronous completion
/// by calling SetFileCompletionNotificationModes with flags FILE_SKIP_COMPLETION_PORT_ON_SUCCESS.
/// @return Either ERROR_SUCCESS or the result of calling GetLastError().
internal_function int aio_driver_submit_read(aio_driver_t *driver, aio_request_t &cmd)
{
    int64_t absolute_ofs = cmd.BaseOffset + cmd.FileOffset; // relative->absolute
    OVERLAPPED     *asio = driver->IOCBFree[WINDOWS_AIO_MAX_ACTIVE-driver->ActiveCount-1];
    DWORD           xfer = 0;
    asio->Internal       = 0;
    asio->InternalHigh   = 0;
    asio->Offset         = DWORD((absolute_ofs & 0x00000000FFFFFFFFULL) >>  0);
    asio->OffsetHigh     = DWORD((absolute_ofs & 0xFFFFFFFFFFFFFFFFULL) >> 32);
    BOOL res = ReadFile(cmd.Fildes, cmd.DataBuffer, cmd.DataAmount, &xfer, asio);
    if (!res)
    {   // the common case is that GetLastError() returns ERROR_IO_PENDING,
        // which means that the operation will complete asynchronously.
        DWORD error= GetLastError();
        if (error == ERROR_IO_PENDING)
        {   // the operation was queued by kernel AIO.
            error  = ERROR_SUCCESS;
        }
        else if (error == ERROR_HANDLE_EOF)
        {   // attempted to read past end-of-file. not an error; synchronous.
            return aio_driver_post_result(cmd, ERROR_SUCCESS, 0);
        }
        else
        {   // an actual error occurred; synchronous completion.
            return aio_driver_post_result(cmd, error, 0);
        }
        // append to the active list; GetQueuedCompletionStatusEx will notify.
        size_t index = driver->ActiveCount++;
        driver->IOCBList[index] = asio;
        driver->AIOCList[index] = cmd;
        return error;
    }
    else
    {   // the read operation has completed synchronously. complete immediately.
        // normally, GetQueuedCompletionStatusEx would notify, but we used the
        // SetFileCompletionNotificationModes to override that behavior.
        return aio_driver_post_result(cmd, ERROR_SUCCESS, xfer);
    }
}

/// @summary Builds a write operation and submits it to the kernel.
/// @param driver The AIO driver state processing the AIO request.
/// @param cmd The data associated with the write operation. The file handle
/// must have been set to not notify the completion queue on synchronous
/// completion by calling SetFileCompletionNotificationModes with flags FILE_SKIP_COMPLETION_PORT_ON_SUCCESS.
/// @return Either ERROR_SUCCESS or the result of calling GetLastError().
internal_function DWORD aio_driver_submit_write(aio_driver_t *driver, aio_request_t &cmd)
{
    int64_t absolute_ofs = cmd.BaseOffset + cmd.FileOffset; // relative->absolute
    OVERLAPPED     *asio = driver->IOCBFree[WINDOWS_AIO_MAX_ACTIVE-driver->ActiveCount-1];
    DWORD           xfer = 0;
    asio->Internal       = 0;
    asio->InternalHigh   = 0;
    asio->Offset         = DWORD((absolute_ofs & 0x00000000FFFFFFFFULL) >>  0);
    asio->OffsetHigh     = DWORD((absolute_ofs & 0xFFFFFFFFFFFFFFFFULL) >> 32);
    BOOL res = WriteFile(cmd.Fildes, cmd.DataBuffer, cmd.DataAmount, &xfer, asio);
    if (!res)
    {   // the common case is that GetLastError() returns ERROR_IO_PENDING,
        // which means that the operation will complete asynchronously.
        DWORD error= GetLastError();
        if (error == ERROR_IO_PENDING)
        {   // the operation was queued by kernel AIO. append to the active list.
            error = ERROR_SUCCESS;
        }
        else
        {   // an actual error occurred; synchronous completion.
            return aio_driver_post_result(cmd, error, 0);
        }
        // append to the active list; GetQueuedCompletionStatusEx will notify.
        size_t index = driver->ActiveCount++;
        driver->IOCBList[index] = asio;
        driver->AIOCList[index] = cmd;
        return error;
    }
    else
    {   // the write operation has completed synchronously. complete immediately.
        // normally, GetQueuedCompletionStatusEx would notify, but we used the
        // SetFileCompletionNotificationModes to override that behavior.
        return aio_driver_post_result(cmd, ERROR_SUCCESS, xfer);
    }
}

/// @summary Poll Windows kernel AIO for completed events.
/// @param driver The AIO driver state to update.
/// @param timeout The amount of time to block the calling thread waiting for
/// the kernel to report events, in milliseconds. Specify INFINITE to wait
/// indefinitely, or a timeout of zero to operate in a polling mode and return immediately.
/// @param shutdown On return, this value will be set to TRUE if a shutdown signal is received.
/// @return The number of in-flight commands.
internal_function size_t aio_driver_poll_ev(aio_driver_t *driver, DWORD timeout, BOOL &shutdown)
{   // poll the kernel for events and process them as appropriate.
    ULONG const N = WINDOWS_AIO_MAX_ACTIVE;
    OVERLAPPED_ENTRY events[N];
    ULONG nevents = 0;
    BOOL  iocpres = GetQueuedCompletionStatusEx(driver->AIOContext, events, N, &nevents, timeout, FALSE);
    if (iocpres  && nevents > 0)
    {   // kernel AIO reported one or more ready events.
        for (size_t i = 0,  n = (size_t) nevents; i < n; ++i)
        {
            OVERLAPPED_ENTRY &evt = events[i];
            OVERLAPPED      *iocb = evt.lpOverlapped;

            if (evt.lpCompletionKey == AIO_SHUTDOWN)
            {   // indicate that a shutdown signal has been received.
                // don't process any additional completions.
                shutdown = TRUE;
                break;
            }
            // locate the corresponding command in the active list
            // by searching through the active IOCB descriptors.
            // this is necessary because the active list is unordered.
            size_t const ncmd = driver->ActiveCount;
            OVERLAPPED  **ios = driver->IOCBList;
            for (size_t index = 0; index < ncmd; ++index)
            {   // the test is a simple pointer comparison.
                if (ios[index] == iocb)
                {
                    DWORD    err = HRESULT_FROM_NT(evt.Internal);
                    uint32_t amt = evt.dwNumberOfBytesTransferred;
                    // post the result to the target queue for the command.
                    aio_request_t &cmd = driver->AIOCList[index];
                    aio_driver_post_result(cmd, err, amt);
                    // return the OVERLAPPED instance to the free list.
                    driver->IOCBFree[N-ncmd]= iocb;
                    // swap the last active command into slot 'index'.
                    driver->AIOCList[index] = driver->AIOCList[ncmd-1];
                    driver->IOCBList[index] = driver->IOCBList[ncmd-1];
                    driver->ActiveCount--;
                    break;
                }
            }
        }
    }
    return driver->ActiveCount;
}

/// @summary Implements the main loop of the AIO driver using a polling mechanism.
/// @param driver The AIO driver state to update.
/// @param timeout The timeout value indicating the amount of time to wait, or NULL to block indefinitely.
/// @return Zero to continue with the next tick, 1 if the shutdown signal was received, -1 if an error occurred.
internal_function int aio_driver_main(aio_driver_t *driver, DWORD timeout)
{
    BOOL         shutdown     = FALSE;
    size_t const active_count = aio_driver_poll_ev(driver, timeout, shutdown);
    size_t const active_max   = WINDOWS_AIO_MAX_ACTIVE;
    if (active_count < active_max)
    {   // now dequeue and submit as many AIO commands as we can.
        // dequeue as many requests from the queue as possible,
        // convert them to iocbs (if necessary) and submit them
        // to the kernel all in a single call. because io_submit
        // does not guarantee that requests will be processed in
        // the order received, we pre-sort by type.
        size_t        npending = active_count;
        size_t        reqcount[AIO_COMMAND_COUNT];
        aio_request_t requests[AIO_COMMAND_COUNT][active_max];
        memset(reqcount,0,AIO_COMMAND_COUNT*sizeof(size_t));
        while (npending < active_max)
        {   // grab the next request from the queue and sort by type.
            aio_request_t req;
            if (spsc_fifo_b_consume(&driver->RequestQueue, req))
            {   // a request was retrieved.
                size_t type  = req.CommandType;
                size_t index = reqcount[type]++;
                requests[type][index] = req;
                npending++;
            }
            else break; // no more pending requests.
        }

        // now submit all read and write operations, which *MAY* be asynchronous.
        aio_request_t *read_list   = requests[AIO_COMMAND_READ];
        size_t const   read_count  = reqcount[AIO_COMMAND_READ];
        for (size_t i = 0; i < read_count; ++i)
        {   // TODO(rlk): error handling here.
            aio_driver_submit_read(driver, read_list[i]);
        }
        aio_request_t *write_list  = requests[AIO_COMMAND_WRITE];
        size_t const   write_count = reqcount[AIO_COMMAND_WRITE];
        for (size_t i = 0; i < write_count; ++i)
        {   // TODO(rlk): error handling here.
            aio_driver_submit_write(driver, write_list[i]);
        }
        // now process all flush, close and finalize operations, which are synchronous.
        aio_request_t *flush_list  = requests[AIO_COMMAND_FLUSH];
        size_t const   flush_count = reqcount[AIO_COMMAND_FLUSH];
        for (size_t i = 0; i < flush_count; ++i)
        {   // TODO(rlk): error handling here.
            aio_driver_flush_file(flush_list[i]);
        }
        aio_request_t *close_list  = requests[AIO_COMMAND_CLOSE];
        size_t const   close_count = reqcount[AIO_COMMAND_CLOSE];
        for (size_t i = 0; i < close_count; ++i)
        {
            aio_driver_close_file(close_list[i]);
        }
        aio_request_t *rename_list  = requests[AIO_COMMAND_CLOSE_TEMP];
        size_t const   rename_count = reqcount[AIO_COMMAND_CLOSE_TEMP];
        for (size_t i = 0; i < rename_count; ++i)
        {
            aio_driver_close_and_rename(rename_list[i]);
        }
    }
    return (shutdown) ? 1 : 0;
}

/*////////////////////////
//   Public Functions   //
////////////////////////*/
/// @summary Opens a connection to kernel AIO and allocates driver resources.
/// @param driver The AIO driver state to initialize.
/// @return ERROR_SUCCESS on success, or an error code returned by GetLastError() on failure.
public_function DWORD aio_driver_open(aio_driver_t *driver)
{
#pragma warning(push)
#pragma warning(disable:4127) // conditional expression is constant
    if ((WINDOWS_AIO_MAX_ACTIVE & (WINDOWS_AIO_MAX_ACTIVE-1)) != 0)
    {
        assert(false && "WINDOWS_AIO_MAX_ACTIVE must be a power-of-two");
        return ERROR_INVALID_PARAMETER;
    }
#pragma warning(pop)
    // zero out all fields of the driver structure.
    memset(driver, 0, sizeof(aio_driver_t));
    // create the I/O completion port for kernel notification.
    HANDLE iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
    if (iocp == NULL)
    {   // unable to create the AIO context; everything else fails.
        return GetLastError();
    }
    // setup the iocb free list. all items are initially available.
    for (size_t i = 0, n = WINDOWS_AIO_MAX_ACTIVE; i < n; ++i)
    {
        driver->IOCBFree[i] = &driver->IOCBPool[i];
    }
    // populate fields of the driver structure.
    driver->AIOContext   = iocp;
    driver->ActiveCount  = 0;
    spsc_fifo_b_init(&driver->RequestQueue, WINDOWS_AIO_MAX_ACTIVE);
    return ERROR_SUCCESS;
}

/// @summary Closes a connection to kernel AIO and frees driver resources.
/// @param driver The AIO driver state to clean up.
public_function void aio_driver_close(aio_driver_t *driver)
{
    spsc_fifo_b_delete(&driver->RequestQueue);
    CloseHandle(driver->AIOContext);
    driver->AIOContext  = NULL;
    driver->ActiveCount = 0;
}

/// @summary Executes a single AIO driver update in polling mode. The kernel
/// AIO system will be polled for completed events without blocking. This
/// dispatches completed command results to their associated target queues and
/// launches as many not started commands as possible.
/// @param driver The AIO driver state to poll.
public_function void aio_driver_poll(aio_driver_t *driver)
{   // configure a zero timeout so we won't block.
    aio_driver_main(driver, 0);
}

/// @summary Executes a single AIO driver update in blocking mode. The kernel
/// AIO system will be polled for completed events, blocking the calling thread
/// until at least one command has completed or the specified timeout has elapsed.
/// This dispatches completed command results to their associated target queues
/// and launches as many not started commands as possible.
/// @param driver The AIO driver state to poll.
/// @param timeout The maximum amount of time to wait for the kernel to report
/// completed events, in milliseconds. Specify INFINITE to wait indefinitely.
/// Specify 0 to poll (return immediately), or specify a non-zero value to wait
/// for up to the specified amount of milliseconds.
public_function void aio_driver_wait(aio_driver_t *driver, DWORD timeout)
{
    aio_driver_main(driver, timeout);
}

/// @summary Prepare a file handle to perform asynchronous I/O operations.
/// @param driver The asynchronous I/O driver state.
/// @param fd The handle of the file to prepare for asynchronous I/O.
/// @return ERROR_SUCCESS, or a system error code.
public_function DWORD aio_driver_prepare(aio_driver_t *driver, HANDLE fd)
{   // associate the file handle to the I/O completion port.
    if (CreateIoCompletionPort(fd, driver->AIOContext, 0, 0) != driver->AIOContext)
    {   // the file handle could not be associated with the IOCP.
        return GetLastError();
    }
    // immediately complete requests that execute synchronously;
    // don't post a completion port notification to the IOCP.
    SetFileCompletionNotificationModes(fd, FILE_SKIP_COMPLETION_PORT_ON_SUCCESS);
    return ERROR_SUCCESS;
}

/// @summary Submits a single command to the AIO driver. The command will not
/// be processed any sooner than the next update tick. This function can only
/// be called safely from a single thread (typically the PIO thread.)
/// @param driver The AIO driver that will process the command.
/// @param command A description of the I/O operation to perform.
/// @return true if the command was submitted.
public_function bool aio_driver_submit(aio_driver_t *driver, aio_request_t const &command)
{
    return spsc_fifo_b_produce(&driver->RequestQueue, command);
}

/// @summary Submits as many commands as possible from a thread-local command queue.
/// @param driver The AIO driver that will process the commands.
/// @param iocmdq The thread-local command queue.
/// @return true if all commands were submitted.
public_function bool aio_driver_submit(aio_driver_t *driver, aio_request_queue_t *iocmdq)
{
    aio_request_t iocmd;
    while (lplc_fifo_u_front(iocmdq, iocmd))
    {   // attempt to submit this command to the AIO driver.
        if (spsc_fifo_b_produce(&driver->RequestQueue, iocmd))
        {   // command submitted successfully. remove it from the queue.
            lplc_fifo_u_consume(iocmdq, iocmd);
        }
        else
        {   // the driver command queue is full. no point in continuing.
            return false;
        }
    }
    return true;
}

/// @summary Helper function to initialize an I/O result queue and its associated allocator.
/// @param queue The result queue to initialize.
/// @param allocator The FIFO node pool allocator to initialize.
public_function void aio_create_result_queue(aio_result_queue_t *queue, aio_result_alloc_t *allocator)
{
    spsc_fifo_u_init(queue);
    fifo_allocator_init(allocator);
}

/// @summary Helper function to release resources associate with an I/O result queue.
/// @param queue The result queue to empty.
/// @param allocator The FIFO node pool allocator associated with the queue.
public_function void aio_delete_result_queue(aio_result_queue_t *queue, aio_result_alloc_t *allocator)
{
    spsc_fifo_u_delete(queue);
    fifo_allocator_reinit(allocator);
}

/// @summary Helper function to initialize an I/O buffer return queue and its associated allocator.
/// @param queue The I/O buffer return queue to initialize.
/// @param allocator The FIFO node pool allocator to initialize.
public_function void aio_create_return_queue(aio_return_queue_t *queue, aio_return_alloc_t *allocator)
{
    mpsc_fifo_u_init(queue);
    fifo_allocator_init(allocator);
}

/// @summary Helper function to release resources associate with an I/O buffer return queue.
/// @param queue The I/O buffer return queue to empty.
/// @param allocator The FIFO node pool allocator associated with the queue.
public_function void aio_delete_return_queue(aio_return_queue_t *queue, aio_return_alloc_t *allocator)
{
    mpsc_fifo_u_delete(queue);
    fifo_allocator_reinit(allocator);
}

/// @summary Helper function to process all queued I/O buffer returns. This
/// function would be called on the thread that manages the I/O buffer allocator.
/// @param queue The I/O buffer return queue to process.
/// @param iobufalloc The I/O buffer allocator to which buffers will be returned.
public_function inline void aio_process_buffer_returns(aio_return_queue_t *queue, io_buffer_allocator_t *iobufalloc)
{
    if (queue != NULL && iobufalloc != NULL)
    {
        void  *buffer = NULL;
        while (mpsc_fifo_u_consume(queue, buffer))
        {
            iobufalloc->put_buffer(buffer);
        }
    }
}

/// @summary Helper function to inialize a thread-local AIO command queue.
/// @param queue The thread-local AIO command queue to initialize.
public_function inline void aio_create_request_queue(aio_request_queue_t *queue)
{
    lplc_fifo_u_init(queue);
}

/// @summary Helper function to release resources associated with a thread-local AIO command queue.
/// @param queue The thread-local AIO command queue to delete.
public_function inline void aio_delete_request_queue(aio_request_queue_t *queue)
{
    lplc_fifo_u_delete(queue);
}

/// @summary Helper function to submit a command to a thread-local AIO command queue.
/// @param queue The target thread-local AIO command queue.
/// @param iocmd The AIO driver command to buffer.
public_function inline void aio_submit_request_queue(aio_request_queue_t *queue, aio_request_t const &iocmd)
{
    lplc_fifo_u_produce(queue, iocmd);
}
