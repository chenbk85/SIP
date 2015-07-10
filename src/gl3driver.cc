/*/////////////////////////////////////////////////////////////////////////////
/// @summary Implements display enumeration and OpenGL 3.2 rendering context 
/// initialization. Additionally, OpenCL interop can be set up for attached 
/// devices supporting OpenCL 1.2 or later and cl_khr_gl_sharing.
///////////////////////////////////////////////////////////////////////////80*/

/*////////////////
//   Includes   //
////////////////*/

/*/////////////////
//   Constants   //
/////////////////*/
/// @summary Define the registered name of the WNDCLASS used for hidden windows.
#define GLRC_HIDDEN_WNDCLASS_NAME       _T("GLRC_Hidden_WndClass")

/// @summary Define the maximum number of in-flight frames per-driver.
static uint32_t const GLRC_MAX_FRAMES   = 4;

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Define the possible states of an in-flight frame.
enum gl_frame_state_e : int
{
    GLRC_FRAME_STATE_WAIT_FOR_IMAGES    = 0,   /// The frame is waiting for images to be locked in host memory.
    GLRC_FRAME_STATE_WAIT_FOR_COMPUTE   = 1,   /// The frame is waiting for compute jobs to complete.
    GLRC_FRAME_STATE_WAIT_FOR_DISPLAY   = 2,   /// The frame is waiting for display jobs to complete.
    GLRC_FRAME_STATE_COMPLETE           = 3,   /// The frame has completed.
    GLRC_FRAME_STATE_ERROR              = 4,   /// The frame encountered one or more errors.
};

/// @summary Represents a unique identifier for a locked image.
struct gl_image_id_t
{
    uintptr_t               ImageId;           /// The application-defined image identifier.
    size_t                  FrameIndex;        /// The zero-based index of the frame.
};

/// @summary Defines the data stored for a locked image.
struct gl_image_data_t
{
    DWORD                   ErrorCode;         /// ERROR_SUCCESS or a system error code.
    uint32_t                SourceFormat;      /// One of dxgi_format_e defining the source data storage format.
    uint32_t                SourceCompression; /// One of image_compression_e defining the source data storage compression.
    uint32_t                SourceEncoding;    /// One of image_encoding_e defining the source data storage encoding.
    thread_image_cache_t   *SourceCache;       /// The image cache that manages the source image data.
    size_t                  SourceWidth;       /// The width of the source image, in pixels.
    size_t                  SourceHeight;      /// The height of the source image, in pixels.
    size_t                  SourcePitch;       /// The number of bytes per-row in the source image data.
    uint8_t                *SourceData;        /// Pointer to the start of the locked image data on the host.
    size_t                  SourceSize;        /// The number of bytes of source data.
};

/// @summary Defines the data associated with a single in-flight frame.
struct gl_frame_t
{
    int                     FrameState;        /// One of gdi_frame_state_e indicating the current state of the frame.

    pr_command_list_t      *CommandList;       /// The command list that generated this frame.

    size_t                  ImageCount;        /// The number of images referenced in this frame.
    size_t                  ImageCapacity;     /// The capacity of the frame image list.
    gl_image_id_t          *ImageIds;          /// The unique identifiers of the referenced images.
};

struct gl3_renderer_t
{   typedef image_cache_result_queue_t             image_lock_queue_t;
    typedef image_cache_error_queue_t              image_error_queue_t;

    size_t                  Width;             /// The current viewport width.
    size_t                  Height;            /// The current viewport height.
    HDC                     Drawable;          /// The target drawable, which may reference the display's hidden drawable.
    HWND                    Window;            /// The target window, which may reference the display's hidden window.
    gl_display_t           *Display;           /// The display on which the drawable or window resides.
    pr_command_queue_t      CommandQueue;      /// The presentation command queue to which command lists are submitted.

    size_t                  CacheCount;        /// The number of thread-local cache writers.
    size_t                  CacheCapacity;     /// The maximum capacity of the presentation thread image cache list.
    thread_image_cache_t   *CacheList;         /// The list of interfaces used to access image caches from the presentation thread.

    size_t                  ImageCount;        /// The number of locked images.
    size_t                  ImageCapacity;     /// The maximum capacity of the locked image list.
    gl_image_id_t          *ImageIds;          /// The list of locked image identifiers.
    gl_image_data_t        *ImageData;         /// The list of locked image data descriptors.
    size_t                 *ImageRefs;         /// The list of reference counts for each locked image.

    image_lock_queue_t      ImageLockQueue;    /// The MPSC unbounded FIFO where completed image lock requests are stored.
    image_error_queue_t     ImageErrorQueue;   /// The MPSC unbounded FIFO where failed image lock requests are stored.

    #define MAXF            GLRC_MAX_FRAMES
    uint32_t                FrameHead;         /// The zero-based index of the oldest in-flight frame.
    uint32_t                FrameTail;         /// The zero-based index of the newest in-flight frame.
    uint32_t                FrameMask;         /// The bitmask used to map an index into the frame array.
    gl_frame_t              FrameQueue[MAXF];  /// Storage for in-flight frame data.
    #undef  MAXF
};

/*///////////////
//   Globals   //
///////////////*/
// presentation command list is dequeued
// - create a new local list entry
// - read all commands, generate list of compute jobs
// - submit lock requests to image cache
//   - the pointer can be used to track lock completion status
// - as each lock result is received, launch the corresponding compute job
//   - needs a pointer to the locked data
//   - needs basic data like image dimensions
//   - needs a job identifier
// - compute job completion is the job ID plus a status code plus an output uintptr_t?
//   - key thing is how to represent the output data...
// - so one open question is how do we link up compute pipeline output with with graphics pipeline input?
//   - there may not be any graphics pipeline input
// - each compute pipeline job needs its own presentation command
//   - the data for the command defines input and output parameters, ie. output to this texture or buffer.
// - once all (blocking) compute jobs have completed, process the command list

/*///////////////////////
//   Local Functions   //
///////////////////////*/
/// @summary Calculate the number of in-flight frames.
/// @param gl The render state to query.
/// @return The number of in-flight frames.
internal_function inline size_t frame_count(gl3_renderer_t *gl)
{
    return size_t(gl->FrameTail - gl->FrameHead);
}

/// @summary Prepare an image for use by locking its data in host memory. The lock request completes asynchronously.
/// @param gl The render state processing the request.
/// @param frame The in-flight frame state requiring the image data.
/// @param image A description of the portion of the image required for use.
internal_function void prepare_image(gl3_renderer_t *gl, gl_frame_t *frame, pr_image_subresource_t *image)
{
    uintptr_t const image_id     = image->ImageId;
    size_t    const frame_index  = image->FrameIndex;
    size_t          local_index  = 0;
    size_t          global_index = 0;
    bool            found_image  = false;
    // check the frame-local list first. if we find the image here, we know 
    // that the frame has already been requested, and we don't need to lock it.
    for (size_t i = 0, n = frame->ImageCount; i < n; ++i)
    {
        if (frame->ImageIds[i].ImageId    == image_id && 
            frame->ImageIds[i].FrameIndex == frame_index)
        {
            local_index = i;
            found_image = true;
            break;
        }
    }
    if (!found_image)
    {   // add this image/frame to the local frame list.
        if (frame->ImageCount == frame->ImageCapacity)
        {   // increase the capacity of the local image list.
            size_t old_amount  = frame->ImageCapacity;
            size_t new_amount  = calculate_capacity(old_amount, old_amount+1, 1024, 1024);
            gl_image_id_t *il  =(gl_image_id_t*)realloc(frame->ImageIds , new_amount * sizeof(gl_image_id_t));
            if (il != NULL)
            {
                frame->ImageIds      = il;
                frame->ImageCapacity = new_amount;
            }
        }
        local_index   = frame->ImageCount++;
        frame->ImageIds[local_index].ImageId     = image_id;
        frame->ImageIds[local_index].FrameIndex  = frame_index;
        // attempt to locate the image in the global image list.
        for (size_t gi = 0, gn = gl->ImageCount; gi < gn; ++gi)
        {
            if (gl->ImageIds[gi].ImageId    == image_id && 
                gl->ImageIds[gi].FrameIndex == frame_index)
            {   // this image already exists in the global image list.
                // there's no need to lock it; just increment the reference count.
                gl->ImageRefs[gi]++;
                found_image = true;
                break;
            }
        }
        // if the image wasn't in the global list, add it.
        if (!found_image)
        {
            if (gl->ImageCount == gl->ImageCapacity)
            {   // increase the capacity of the global image list.
                size_t old_amount   = gl->ImageCapacity;
                size_t new_amount   = calculate_capacity(old_amount, old_amount+1, 1024 , 1024);
                gl_image_id_t   *il = (gl_image_id_t  *) realloc(gl->ImageIds , new_amount * sizeof(gl_image_id_t));
                gl_image_data_t *dl = (gl_image_data_t*) realloc(gl->ImageData, new_amount * sizeof(gl_image_data_t));
                size_t          *rl = (size_t         *) realloc(gl->ImageRefs, new_amount * sizeof(size_t));
                if (il != NULL)  gl->ImageIds  = il;
                if (dl != NULL)  gl->ImageData = dl;
                if (rl != NULL)  gl->ImageRefs = rl;
                if (il != NULL && dl != NULL  && rl != NULL) gl->ImageCapacity = new_amount;
            }
            global_index = gl->ImageCount++;
            gl->ImageRefs[global_index]                   = 1;
            gl->ImageIds [global_index].ImageId           = image_id;
            gl->ImageIds [global_index].FrameIndex        = frame_index;
            gl->ImageData[global_index].ErrorCode         = ERROR_SUCCESS;
            gl->ImageData[global_index].SourceFormat      = DXGI_FORMAT_UNKNOWN;
            gl->ImageData[global_index].SourceCompression = IMAGE_COMPRESSION_NONE;
            gl->ImageData[global_index].SourceEncoding    = IMAGE_ENCODING_RAW;
            gl->ImageData[global_index].SourceCache       = NULL;
            gl->ImageData[global_index].SourceWidth       = 0;
            gl->ImageData[global_index].SourceHeight      = 0;
            gl->ImageData[global_index].SourcePitch       = 0;
            gl->ImageData[global_index].SourceData        = NULL;
            gl->ImageData[global_index].SourceSize        = 0;
            // locate the presentation thread cache interface for the source data:
            bool found_cache = false;
            for (size_t ci = 0, cn = gl->CacheCount; ci < cn; ++ci)
            {
                if (gl->CacheList[ci].Cache == image->ImageSource)
                {
                    gl->ImageData[global_index].SourceCache = &gl->CacheList[ci];
                    found_cache = true;
                    break;
                }
            }
            if (!found_cache)
            {   // create a new interface to write to this image cache.
                if (gl->CacheCount == gl->CacheCapacity)
                {   // increase the capacity of the image cache list.
                    size_t old_amount   = gl->CacheCapacity;
                    size_t new_amount   = calculate_capacity(old_amount, old_amount+1, 16, 16);
                    thread_image_cache_t *cl = (thread_image_cache_t*)   realloc(gl->CacheList, new_amount * sizeof(thread_image_cache_t));
                    if (cl != NULL)
                    {
                        gl->CacheList     = cl;
                        gl->CacheCapacity = new_amount;
                    }
                }
                thread_image_cache_t *cache = &gl->CacheList[gl->CacheCount++];
                gl->ImageData[global_index].SourceCache = cache;
                cache->initialize(image->ImageSource);
            }
            // finally, submit a lock request for the frame.
            gl->ImageData[global_index].SourceCache->lock(image_id, frame_index, frame_index, &gl->ImageLockQueue, &gl->ImageErrorQueue, 0);
        }
    }
}

/// @summary Attempt to start a new in-flight frame.
/// @param gl The renderer state managing the frame.
/// @param cmdlist The presentation command list defining the frame.
/// @return true if the frame was launched, or false if too many frames are in-flight.
internal_function bool launch_frame(gl3_renderer_t *gl, pr_command_list_t *cmdlist)
{   // insert a new frame at the tail of the queue.
    if (frame_count(gl) == GLRC_MAX_FRAMES)
    {   // there are too many frames in-flight.
        return false;
    }
    uint32_t     id    = gl->FrameTail;
    size_t       index = size_t(id & gl->FrameMask); 
    gl_frame_t  *frame =&gl->FrameQueue[index];
    frame->FrameState  = GLRC_FRAME_STATE_WAIT_FOR_IMAGES;
    frame->CommandList = cmdlist;
    frame->ImageCount  = 0;

    // iterate through the command list and locate all PREPARE_IMAGE commands.
    // for each of these, update the frame-local image list as well as the 
    // the driver-global image list. eventually, we'll also use this to initialize
    // any compute and display jobs as well.
    bool       end_of_frame = false;
    uint8_t const *read_ptr = cmdlist->CommandData;
    uint8_t const *end_ptr  = cmdlist->CommandData + cmdlist->BytesUsed;
    while (read_ptr < end_ptr && !end_of_frame)
    {
        pr_command_t *cmd_info = (pr_command_t*) read_ptr;
        size_t        cmd_size =  PR_COMMAND_SIZE_BASE + cmd_info->DataSize;
        switch (cmd_info->CommandId)
        {
        case PR_COMMAND_END_OF_FRAME:
            {   // break out of the command processing loop.
                end_of_frame = true;
                gl->FrameTail++;
            }
            break;
        case PR_COMMAND_PREPARE_IMAGE:
            {   // lock the source data in host memory.
                prepare_image(gl, frame, (pr_image_subresource_t*) cmd_info->Data);
            }
            break;
        // TODO(rlk): case PR_COMMAND_compute_x: init compute job record, but don't start
        // TODO(rlk): case PR_COMMAND_display_x: init display job record, but don't start
        default:
            break;
        }
        // advance to the start of the next buffered command.
        read_ptr += cmd_size;
    }
    return true;
}

/// @summary Perform cleanup operations when the display commands for the oldest in-flight frame have completed.
/// @param gl The renderer state managing the frame.
internal_function void retire_frame(gl3_renderer_t *gl)
{   // retire the frame at the head of the queue.
    uint32_t          id = gl->FrameHead++;
    size_t         index = size_t(id & gl->FrameMask);
    gl_frame_t    *frame =&gl->FrameQueue[index];
    gl_image_id_t *local_ids  = frame->ImageIds;
    gl_image_id_t *global_ids = gl->ImageIds;
    for (size_t local_index = 0, local_count = frame->ImageCount; local_index < local_count; ++local_index)
    {   // locate the corresponding entry in the global list.
        gl_image_id_t const &local_id = local_ids[local_index];
        for (size_t global_index = 0; global_index < gl->ImageCount; ++global_index)
        {   
            gl_image_id_t const &global_id  = global_ids[global_index];
            if (local_id.ImageId == global_id.ImageId && local_id.FrameIndex == global_id.FrameIndex)
            {   // found a match. decrement the reference count.
                if (gl->ImageRefs[global_index]-- == 1)
                {   // the image reference count has dropped to zero.
                    // unlock the image data, and delete the entry from the global list.
                    size_t frame_index = global_id.FrameIndex;
                    size_t  last_index = gl->ImageCount - 1;
                    gl->ImageData[global_index].SourceCache->unlock(global_id.ImageId, frame_index, frame_index);
                    gl->ImageIds [global_index] = gl->ImageIds [last_index];
                    gl->ImageData[global_index] = gl->ImageData[last_index];
                    gl->ImageRefs[global_index] = gl->ImageRefs[last_index];
                    gl->ImageCount--;
                }
                break;
            }
        }
    }
    // TODO(rlk): cleanup compute jobs
    // TODO(rlk): cleanup display state
    pr_command_queue_return(&gl->CommandQueue, frame->CommandList);
    frame->ImageCount = 0;
}

/*///////////////////////
//  Public Functions   //
///////////////////////*/
/// @summary Creates a new renderer attached to a window.
public_function uintptr_t create_renderer_window(gl_display_t *display, HWND window)
{
}

public_function uintptr_t create_renderer_headless(gl_display_t *display, size_t target_width, size_t target_height)
{
    // TODO(rlk): needs to create a FBO, etc.
}

public_function void reset_renderer(uintptr_t handle)
{
    gl3_renderer_t *gl = (gl3_renderer_t*) handle;
    // TODO(rlk): clear image lists, in-flight frames, etc.
    pr_command_queue_clear(&gl->CommandQueue);
}

public_function void reset_renderer(uintptr_t handle, gl_display_t *display, HWND window)
{
    // TODO(rlk): called when a window changes displays.
}

/// @summary Allocates a new command list for use by the application. The application should write display commands to the command list and then submit the list for processing by the presentation driver.
/// @param handle The renderer handle returned by one of the create_renderer functions.
/// @return An empty command list, or NULL if no command lists are available.
public_function pr_command_list_t* renderer_command_list(uintptr_t handle)
{
    return pr_command_queue_next_available(&((gl3_renderer_t*)handle)->CommandQueue);
}

/// @summary Submits a command list, representing a single frame, to the renderer.
/// @param handle The renderer handle, returned by one of the create_renderer functions.
/// @param cmdlist The command list to submit to the renderer. The end-of-frame command is automatically appended.
/// @return A wait handle that can be used to wait for the renderer to process the command list.
public_function HANDLE renderer_submit(uintptr_t handle, pr_command_list_t *cmdlist)
{
    gl3_renderer_t   *gl = (gl3_renderer_t*) handle;
    HANDLE          sync =  cmdlist->SyncHandle;
    pr_command_end_of_frame(cmdlist);
    pr_command_queue_submit(&gl->CommandQueue, cmdlist);
    return sync;
}

/// @summary Processes command lists representing the next frame and submits commands to the GPU.
/// @param handle The renderer handle, returned by one of the create_renderer functions.
public_function void update_drawable(uintptr_t handle)
{
    // TODO(rlk): set the viewport to the entire renderable (window or FBO) bounds.
    // will need to GetClientRect() for the window.
}

/// @summary Frees resources associated with a renderer and detaches it from its drawable.
/// @param handle The renderer handle, returned by one of the create_renderer functions.
public_function void delete_renderer(uintptr_t handle)
{
    gl3_renderer_t *gl = (gl3_renderer_t*) handle;
    // TODO(rlk): The display manages all of the shared resources.
    // If this is a headless renderer, the FBOs etc. are private.
    pr_command_queue_delete(&gl->CommandQueue);
    free(gl);
}
