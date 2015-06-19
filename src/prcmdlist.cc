/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines the types and functions for working with a list of 
/// commands to be send to a presentation driver. The command types are not 
/// driver specific. Each command list is safe for access from a single thread
/// only, but multiple threads can each generate command lists and submit them
/// to the same presentation driver instance concurrently.
///////////////////////////////////////////////////////////////////////////80*/

/*////////////////
//   Includes   //
////////////////*/

/*/////////////////
//   Constants   //
/////////////////*/
/// @summary Define the size of a command with no data.
static size_t const PR_COMMAND_SIZE_BASE                   = sizeof(uint32_t);

/// @summary Define the maximum number of command lists per command queue.
static size_t const PR_COMMAND_LIST_QUEUE_MAX              = 16;

/// @summary Define the allocation granularity for command list command data.
static size_t const PR_COMMAND_LIST_ALLOCATION_GRANULARITY = 64 * 1024;

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Forward declae the structure representing a command list.
struct  pr_command_list_t;

/// @summary Define aliases for types used internally by a command list queue.
typedef fifo_allocator_t<pr_command_list_t*> cmdq_alloc_t;
typedef mpsc_fifo_u_t<pr_command_list_t*>    cmdq_queue_t;

/// @summary Define the set of recognized command identifiers.
enum pr_command_type_e : uint16_t
{
    /// @summary A no-op command with no data.
    PR_COMMAND_NO_OP                = 0,
    /// @summary Specifies an end-of-frame marker, which terminates the display update. 
    /// This command is added to the buffer when it is submitted to the presentation driver.
    PR_COMMAND_END_OF_FRAME         = 1, 
    /// @summary Clear the color buffer to a specified color.
    /// @param red A 32-bit floating-point value in [0, 1] specifying the red channel value.
    /// @param green A 32-bit floating-point value in [0, 1] specifying the green channel value.
    /// @param blue A 32-bit floating-point value in [0, 1] specifying the blue channel value.
    /// @param alpha A 32-bit floating-point value in [0, 1] specifying the alpha channel value.
    PR_COMMAND_CLEAR_COLOR_BUFFER   = 2,
    /// @summary Process and display a 2D image.
    /// @param image_id The application-defined identifier of the logical image.
    /// @param frame_index The zero-based index of the frame to display.
    /// ...
    PR_COMMAND_DRAW_IMAGE_2D        = 3,
};

/// @summary Defines the structure of a single presentation command. Each command consists
/// of a 32-bit header specifying the command identifier and data size, followed by data 
/// specific to that type of command.
struct pr_command_t
{
    uint16_t           CommandId;             /// The command identifier, one of pr_command_type_e.
    uint16_t           DataSize;              /// The size of the command data, up to 64KB.
    uint8_t            Data[1];               /// The command data, up to 64KB in length.
};

/// @summary Define the data associated with the command list.
struct pr_command_list_t
{
    size_t             BytesTotal;            /// The number of bytes allocated for this command list.
    size_t             BytesUsed;             /// The number of bytes actually used for this command list.
    size_t             CommandCount;          /// The number of buffered commands.
    uint8_t           *CommandData;           /// The allocated memory block.
    HANDLE             SyncHandle;            /// A ManualReset event used to wait for the command list to be processed.
};

/// @summary Define the data associated with a MPSC FIFO queue of command lists. 
/// Each command list can be populated by a thread and then submitted to the queue.
struct pr_command_queue_t
{
    #define N  PR_COMMAND_LIST_QUEUE_MAX
    cmdq_alloc_t       CommandListAlloc;      /// The FIFO node allocator for the queue.
    cmdq_queue_t       CommandListQueue;      /// The MPSC queue of submitted command lists.
    size_t             ListFreeCount;         /// The number of valid items in the free list.
    pr_command_list_t *CommandListFree [N];   /// Pointers into CommandListStore representing available lists.
    pr_command_list_t  CommandListStore[N];   /// Actual storage for all of the command lists.
    #undef  N
};

/// @summary Define the data associated with PR_COMMAND_CLEAR_COLOR_BUFFER.
struct pr_clear_color_buffer_data_t
{
    float              Red;                   /// The value to write to the red channel.
    float              Green;                 /// The value to write to the green channel.
    float              Blue;                  /// The value to write to the blue channel.
    float              Alpha;                 /// The value to write to the alpha channel.
};

/// @summary Define the data associated with PR_COMMAND_DRAW_IMAGE_2D.
struct pr_draw_image2d_data_t
{
    uintptr_t          ImageId;               /// The application-defined logical image identifier.
    uintptr_t          PipelineId;            /// The identifier of the presentation pipeline.
    uint32_t           FrameId;               /// The zero-based index of the frame.
    uint32_t           DXGIFormat;            /// One of dxgi_format_e specifying the data storage format.
    uint32_t           ImageWidth;            /// The total width of the image, in pixels.
    uint32_t           ImageHeight;           /// The total height of the image, in pixels.
    uint32_t           SourceX;               /// The x-coordinate of the upper-left corner of the source rectangle on the image.
    uint32_t           SourceY;               /// The y-coordinate of the upper-left corner of the source rectangle on the image.
    uint32_t           SourceWidth;           /// The width of the source rectangle, in pixels.
    uint32_t           SourceHeight;          /// The height of the source rectangle, in pixels.
    uint32_t           TargetX;               /// The x-coordinate of the upper-left corner of the image on the render target.
    uint32_t           TargetY;               /// The y-coordinate of the upper-left corner of the image on the render target.
    uint32_t           TargetWidth;           /// The width of the destination rectangle, in pixels.
    uint32_t           TargetHeight;          /// The height of the destination rectangle, in pixels.
    float              Rotation;              /// The image rotation value, in radians.
    void              *PixelData;             /// The locked pixel data of the image.
    image_cache_t     *ImageCache;            /// The image cache managing the pixel data for the image.
};

/*///////////////
//   Globals   //
///////////////*/
/// @summary A table for translating command IDs into string literals.
/// Values in this table must appear in the same order they appear in the 
/// pr_command_type_e enumeration.
static char const *PR_COMMAND_NAMES[] = 
{
    "PR_COMMAND_NO_OP", 
    "PR_COMMAND_END_OF_FRAME",
    "PR_COMMAND_CLEAR_COLOR_BUFFER", 
    "PR_COMMAND_DRAW_IMAGE_2D"
};

/*///////////////////////
//   Local Functions   //
///////////////////////*/
/// @summary Helper function to safely wait for an event to become signaled.
/// @param event The handle of a manual or auto-reset event.
/// @param timeout The wait timeout, in milliseconds, or INFINITE (-1) to wait forever.
internal_function void safe_wait(HANDLE event, uint32_t timeout)
{
    bool keep_waiting = true;
    do
    {
        DWORD  result = WaitForSingleObjectEx(event, timeout, TRUE);
        if (result != WAIT_IO_COMPLETION)
        {   // event became signaled, or an error occurred.
            keep_waiting = false;
            break;
        }
    } while (keep_waiting);
}

/*///////////////////////
//  Public Functions   //
///////////////////////*/
/// @summary Initialize a command list to empty, with no memory allocated.
/// @param cmdlist The command list to initialize.
public_function void pr_command_list_init(pr_command_list_t *cmdlist)
{
    cmdlist->BytesTotal   = 0;
    cmdlist->BytesUsed    = 0;
    cmdlist->CommandCount = 0;
    cmdlist->CommandData  = NULL;
    cmdlist->SyncHandle   = CreateEvent(NULL, TRUE, FALSE, NULL);
}

/// @summary Clear a command list without allocating or freeing any memory.
/// @param cmdlist The command list to clear.
public_function void pr_command_list_clear(pr_command_list_t *cmdlist)
{
    cmdlist->BytesUsed    = 0;
    cmdlist->CommandCount = 0;
    ResetEvent(cmdlist->SyncHandle);
}

/// @summary Frees all memory allocated to a command list and re-initializes it to empty.
/// @param cmdlist The command list to delete.
public_function void pr_command_list_free(pr_command_list_t *cmdlist)
{
    if (cmdlist->BytesTotal > 0 && cmdlist->CommandData != NULL)
    {
        free(cmdlist->CommandData);
    }
    if (cmdlist->SyncHandle != NULL)
    {
        CloseHandle(cmdlist->SyncHandle);
    }
    cmdlist->BytesTotal   = 0;
    cmdlist->BytesUsed    = 0;
    cmdlist->CommandCount = 0;
    cmdlist->CommandData  = NULL;
    cmdlist->SyncHandle   = NULL;
}

/// @summary Reserves memory for a single command.
/// @param cmdlist The command list to allocate from.
/// @param total_size The number of bytes to allocate for the command, including the 32-bit header.
/// @return A pointer to the newly allocated command, or NULL.
public_function pr_command_t* pr_command_list_allocate(pr_command_list_t *cmdlist, size_t total_size)
{
    if (cmdlist->BytesUsed + total_size > cmdlist->BytesTotal)
    {   // allocate additional memory for command data.
        size_t new_size = align_up(cmdlist->BytesTotal + total_size, PR_COMMAND_LIST_ALLOCATION_GRANULARITY);
        void  *new_mem  = realloc (cmdlist->CommandData, new_size);
        if (new_mem == NULL) return NULL;
        cmdlist->BytesTotal  = new_size;
        cmdlist->CommandData = (uint8_t*) new_mem;
    }
    // save a pointer to the newly allocated command:
    pr_command_t *cmd = (pr_command_t*) &cmdlist->CommandData[cmdlist->BytesUsed];
    // bump up the number of bytes used and command count:
    cmdlist->BytesUsed += total_size;
    cmdlist->CommandCount++;
    // initialize the new command to a no-op:
    cmd->CommandId = PR_COMMAND_NO_OP;
    cmd->DataSize  = 0;
    return cmd;
}

/// @summary Initialize a command list submission queue to empty.
/// @param fifo The command list submission queue to initialize.
public_function void pr_command_queue_init(pr_command_queue_t *fifo)
{   // initialize and add all command lists to the free set.
    fifo->ListFreeCount  = 0;
    for (size_t i = 0, n = PR_COMMAND_LIST_QUEUE_MAX; i < n; ++i)
    {
        pr_command_list_init(&fifo->CommandListStore[i]);
        fifo->CommandListFree[fifo->ListFreeCount++] = &fifo->CommandListStore[i];
    }
    // initialize the allocator and FIFO used for command list submission.
    mpsc_fifo_u_init(&fifo->CommandListQueue);
    fifo_allocator_init(&fifo->CommandListAlloc);
}

/// @summary Clear a command list submission queue. The caller is responsible for 
/// ensuring that no command lists remain outstanding and owned by the client.
/// @param fifo The command list submission queue to clear.
public_function void pr_command_queue_clear(pr_command_queue_t *fifo)
{
    // return all command lists to free status. no storage is freed.
    fifo->ListFreeCount  = 0;
    for (size_t i = 0, n = PR_COMMAND_LIST_QUEUE_MAX; i < n; ++i)
    {
        pr_command_list_clear(&fifo->CommandListStore[i]);
        fifo->CommandListFree[ fifo->ListFreeCount++] = &fifo->CommandListStore[i];
    }
    // flush the queue of submitted command lists.
    mpsc_fifo_u_flush(&fifo->CommandListQueue);
}

/// @summary Submits a command list for later processing.
/// @param fifo The command list submission queue.
/// @param cmdlist The populated command list to submit. 
public_function void pr_command_queue_submit(pr_command_queue_t *fifo, pr_command_list_t *cmdlist)
{
    fifo_node_t<pr_command_list_t*> *node = fifo_allocator_get(&fifo->CommandListAlloc);
    node->Item = cmdlist;
    mpsc_fifo_u_produce(&fifo->CommandListQueue, node);
}

/// @summary Return a command list to the free list, making it available for use.
/// @param fifo The command list submission queue that owns the command list.
/// @param list The command list being freed.
public_function void pr_command_queue_return(pr_command_queue_t *fifo, pr_command_list_t *list)
{
    fifo->CommandListFree[fifo->ListFreeCount++] = list;
}

/// @summary Retrieve the next unused command list from the free list.
/// @param fifo The command list submission queue to allocate from.
/// @return An available command list, or NULL.
public_function pr_command_list_t* pr_command_queue_next_available(pr_command_queue_t *fifo)
{
    if (fifo->ListFreeCount > 0)
    {   // retrieve the command list from the free list.
        pr_command_list_t *list = fifo->CommandListFree[--fifo->ListFreeCount];
        pr_command_list_clear(list);
        return list;
    }
    else return NULL;
}

/// @summary Retrieve the next waiting populated command list.
/// @param fifo The command list submission queue to query.
/// @return The next waiting command list, or NULL.
public_function pr_command_list_t* pr_command_queue_next_submitted(pr_command_queue_t *fifo)
{
    pr_command_list_t *list = NULL;
    mpsc_fifo_u_consume(&fifo->CommandListQueue, list);
    return list;
}

/// @summary Frees all resources associated with a command list submission queue.
/// @param fifo The command list submission queue to delete.
public_function void pr_command_queue_delete(pr_command_queue_t *fifo)
{   // free storage associated with the individual command lists.
    for (size_t i = 0, n = PR_COMMAND_LIST_QUEUE_MAX; i < n; ++i)
    {
        pr_command_list_free(&fifo->CommandListStore[i]);
        fifo->CommandListFree[i]  = NULL;
    }
    fifo->ListFreeCount = 0;
    // flush the queue of submitted command lists and free allocated nodes.
    mpsc_fifo_u_delete(&fifo->CommandListQueue);
    fifo_allocator_reinit(&fifo->CommandListAlloc);
}

/// @summary Translate a command identifier into its corresponding string name.
/// @param command_id The command identifier, one of pr_command_type_e.
/// @return A NULL-terminated string literal corresponding to the command ID.
public_function char const* pr_command_name(uint16_t command_id)
{
    return PR_COMMAND_NAMES[command_id];
}

/// @summary Translate a command identifier into its corresponding string name.
/// @param command The command to translate.
/// @return A NULL-terminated string literal corresponding to the command ID.
public_function char const* pr_command_name(pr_command_t *command)
{
    return PR_COMMAND_NAMES[command->CommandId];
}

/// @summary Write a no-op command into a command list.
/// @param cmdlist The command list to update.
public_function void pr_command_no_op(pr_command_list_t *cmdlist)
{
    pr_command_t *cmd = pr_command_list_allocate(cmdlist, PR_COMMAND_SIZE_BASE);
    cmd->CommandId    = PR_COMMAND_NO_OP;
    cmd->DataSize     = 0;
}

/// @summary Write an end-of-frame marker into a command list.
/// @param cmdlist The command list to update.
public_function void pr_command_end_of_frame(pr_command_list_t *cmdlist)
{
    pr_command_t *cmd = pr_command_list_allocate(cmdlist, PR_COMMAND_SIZE_BASE);
    cmd->CommandId    = PR_COMMAND_END_OF_FRAME;
    cmd->DataSize     = 0;
}

/// @summary Write a color buffer clear command into a command list.
/// @param cmdlist The command list to update.
/// @param rgba Pointer to an array specifying four values representing the values to write to 
/// the red, green, blue and alpha channels, in that order.
public_function void pr_command_clear_color_buffer(pr_command_list_t *cmdlist, float const *rgba)
{
    size_t        dsz = sizeof(float) * 4;
    pr_command_t *cmd = pr_command_list_allocate(cmdlist, PR_COMMAND_SIZE_BASE + dsz);
    cmd->CommandId    = PR_COMMAND_CLEAR_COLOR_BUFFER;
    cmd->DataSize     = (uint16_t) dsz;

    pr_clear_color_buffer_data_t *data = (pr_clear_color_buffer_data_t*) cmd->Data;
    data->Red   = rgba[0];
    data->Green = rgba[1];
    data->Blue  = rgba[2];
    data->Alpha = rgba[3];
}

/// @summary Write a color buffer clear command into a command list.
/// @param cmdlist The command list to update.
/// @param r The red channel value of the clear color.
/// @param g The green channel value of the clear color.
/// @param b The blue channel value of the clear color.
/// @param a The alpha channel value of the clear color.
public_function void pr_command_clear_color_buffer(pr_command_list_t *cmdlist, float r, float g, float b, float a)
{
    size_t        dsz = sizeof(float) * 4;
    pr_command_t *cmd = pr_command_list_allocate(cmdlist, PR_COMMAND_SIZE_BASE + dsz);
    cmd->CommandId    = PR_COMMAND_CLEAR_COLOR_BUFFER;
    cmd->DataSize     = (uint16_t) dsz;

    pr_clear_color_buffer_data_t *data = (pr_clear_color_buffer_data_t*) cmd->Data;
    data->Red   = r;
    data->Green = g;
    data->Blue  = b;
    data->Alpha = a;
}

/// @summary Write a command to draw a 2D image, possibly with rotation, scaling and image processing.
/// The presentation layer will automatically unlock the image data after the command is processed.
/// @param cmdlist The command list to submit to.
/// @param image_id The application-defined logical image identifier.
/// @param frame_index The zero-based index of the frame being displayed.
/// @param format One of dxgi_format_e specifying the data storage format.
/// @param image_width The total width of the supplied image data, in pixels.
/// @param image_height The total height of the supplied image data, in pixels.
/// @param src_x The x-coordinate of the upper-left corner of the source rectangle on the image.
/// @param src_y The y-coordinate of the upper-left corner of the source rectangle on the image.
/// @param src_w The width of the source rectangle on the image, in pixels.
/// @param src_h The height of the source rectangle on the image, in pixels.
/// @param dst_x The x-coordinate of the upper-left corner of the destination rectangle on the render target.
/// @param dst_y The y-coordinate of the upper-left corner of the destination rectangle on the render target.
/// @param dst_w The width of the destination rectangle, in pixels.
/// @param dst_h The height of the destination rectangle, in pixels.
/// @param rotation The image rotation, specified in radians.
/// @param pixels The locked pixel data for the frame.
/// @param cache The image cache managing the image data.
/// @param pipeline_id The application-defined image presentation pipeline identifier.
public_function void pr_command_draw_image_2d(pr_command_list_t *cmdlist, uintptr_t image_id, size_t frame_index, uint32_t format, size_t image_width, size_t image_height, size_t src_x, size_t src_y, size_t src_w, size_t src_h, size_t dst_x, size_t dst_y, size_t dst_w, size_t dst_h, float rotation, void *pixels, image_cache_t *cache, uintptr_t pipeline_id)
{
    size_t        dsz = sizeof(pr_draw_image2d_data_t);
    pr_command_t *cmd = pr_command_list_allocate(cmdlist, PR_COMMAND_SIZE_BASE + dsz);
    cmd->CommandId    = PR_COMMAND_DRAW_IMAGE_2D;
    cmd->DataSize     = (uint16_t) dsz;

    // TODO(rlk): data validation is probably worth it here.
    pr_draw_image2d_data_t *data = (pr_draw_image2d_data_t*) cmd->Data;
    data->ImageId     = image_id;
    data->PipelineId  = pipeline_id;
    data->FrameId     = (uint32_t) frame_index;
    data->DXGIFormat  = (uint32_t) format;
    data->ImageWidth  = (uint32_t) image_width;
    data->ImageHeight = (uint32_t) image_height;
    data->SourceX     = (uint32_t) src_x;
    data->SourceY     = (uint32_t) src_y;
    data->SourceWidth = (uint32_t) src_w;
    data->SourceHeight= (uint32_t) src_h;
    data->TargetX     = (uint32_t) dst_x;
    data->TargetY     = (uint32_t) dst_y;
    data->TargetWidth = (uint32_t) dst_w;
    data->TargetHeight= (uint32_t) dst_h;
    data->Rotation    = rotation;
    data->PixelData   = pixels;
    data->ImageCache  = cache;
}
