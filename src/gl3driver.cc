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
enum gl_frame_state_e                   : int
{
    GLRC_FRAME_STATE_WAIT_FOR_IMAGES    = 0,   /// The frame is waiting for images to be locked in host memory.
    GLRC_FRAME_STATE_WAIT_FOR_COMPUTE   = 1,   /// The frame is waiting for compute jobs to complete.
    GLRC_FRAME_STATE_WAIT_FOR_DISPLAY   = 2,   /// The frame is waiting for display jobs to complete.
    GLRC_FRAME_STATE_COMPLETE           = 3,   /// The frame has completed.
    GLRC_FRAME_STATE_ERROR              = 4,   /// The frame encountered one or more errors.
};

/// @summary Define bitflags used to specify renderer configuration.
enum gl_renderer_flags_e                : uint32_t
{
    GLRC_RENDER_FLAGS_NONE              = (0 << 0),
    GLRC_RENDER_FLAGS_WINDOW            = (1 << 0),
    GLRC_RENDER_FLAGS_HEADLESS          = (1 << 1),
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

/// 
struct gl3_renderer_t
{   typedef image_cache_result_queue_t             image_lock_queue_t;
    typedef image_cache_error_queue_t              image_error_queue_t;

    size_t                  Width;             /// The current viewport width.
    size_t                  Height;            /// The current viewport height.
    HDC                     Drawable;          /// The target drawable, which may reference the display's hidden drawable.
    HWND                    Window;            /// The target window, which may reference the display's hidden window.
    gl_display_t           *Display;           /// The display on which the drawable or window resides.
    pr_command_queue_t      CommandQueue;      /// The presentation command queue to which command lists are submitted.

    uint32_t                RenderFlags;       /// A combination of gl_renderer_flags_e.
    GLuint                  Framebuffer;       /// The target framebuffer object (0 = render to window).
    GLuint                  Renderbuffer[2];   /// Index 0 => color, Index 1 => depth/stencil.

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
/// @summary Creates a new renderer attached to a window. The window is created and managed externally.
/// @param display The display on which the window content is rendered.
/// @param window The window to which output will be presented.
/// @return A handle to the renderer state, or 0.
public_function uintptr_t create_renderer(gl_display_t *display, HWND window)
{
    gl3_renderer_t   *gl  = NULL;
    size_t  target_width  = 0;
    size_t  target_height = 0;
    RECT         rcclient = {};

    GetClientRect(window, &rcclient);
    target_width  = size_t(rcclient.right  - rcclient.left);
    target_height = size_t(rcclient.bottom - rcclient.top );

    // bind the default framebuffer and set the viewport to the entire window bounds.
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glViewport(0, 0, (GLsizei) target_width, (GLsizei) target_height);

    // allocate memory for the renderer state:
    gl = (gl3_renderer_t*)  malloc(sizeof(gl3_renderer_t));

    gl->Width           = target_width;
    gl->Height          = target_height;
    gl->Drawable        = GetDC(window);
    gl->Window          = window;
    gl->Display         = display;

    // initialize the queue for receiving submitted command lists:
    pr_command_queue_init(&gl->CommandQueue);

    // save names of local display resources:
    gl->RenderFlags     = GLRC_RENDER_FLAGS_WINDOW;
    gl->Framebuffer     = 0;
    gl->Renderbuffer[0] = 0;
    gl->Renderbuffer[1] = 0;

    // initialize a dynamic list holding interfaces used to submit 
    // image cache control commands from the presentation thread.
    gl->CacheCount    = 0;
    gl->CacheCapacity = 4;
    gl->CacheList     =(thread_image_cache_t*) malloc(4 * sizeof(thread_image_cache_t));

    // initialize a dynamic list of images referenced by in-flight frames.
    gl->ImageCount    = 0;
    gl->ImageCapacity = 8;
    gl->ImageIds      =(gl_image_id_t       *) malloc(8 * sizeof(gl_image_id_t));
    gl->ImageData     =(gl_image_data_t     *) malloc(8 * sizeof(gl_image_data_t));
    gl->ImageRefs     =(size_t              *) malloc(8 * sizeof(size_t));

    // initialize queues for receiving image data from the host:
    mpsc_fifo_u_init(&gl->ImageLockQueue);
    mpsc_fifo_u_init(&gl->ImageErrorQueue);

    // initialize the queue of in-flight frame state:
    gl->FrameHead = 0;
    gl->FrameTail = 0;
    gl->FrameMask = GLRC_MAX_FRAMES - 1;
    memset(&gl->FrameQueue, 0, GLRC_MAX_FRAMES * sizeof(gl_frame_t));
    return (uintptr_t) gl;
}

/// @summary Creates a new headless renderer. Output is rendered to an offscreen buffer that can be read back by the application.
/// @param display The display used for processing renderer commands.
/// @param target_width The output width, in pixels. Specify 0 to use the display width.
/// @param target_height The output height, in pixels. Specify 0 to use the display height.
/// @param depth_stencil Specify true if a depth or stencil buffer is required for rendering operations.
/// @param multisample_count Specify a value greater than one to create multisample renderbuffers.
/// @return A handle to the renderer state, or 0.
public_function uintptr_t create_renderer(gl_display_t *display, size_t target_width=0, size_t target_height=0, bool depth_stencil=false, size_t multisample_count=1)
{
    gl3_renderer_t *gl    =  NULL;
    GLuint         fbo    =  0;
    GLsizei        rbn    =  depth_stencil ? 2 : 1;
    GLuint         rbo[2] = {0, 0};

    if (target_width  == 0) target_width  = display->DisplayWidth;
    if (target_height == 0) target_height = display->DisplayHeight;

    // create and set up a framebuffer and associated renderbuffers:
    glGenFramebuffers (1  , &fbo);
    glGenRenderbuffers(rbn,  rbo);
    glBindFramebuffer (GL_DRAW_FRAMEBUFFER, fbo);
    if (multisample_count > 1)
    {   // allocate storage for the color buffer:
        glBindRenderbuffer(GL_RENDERBUFFER, rbo[0]);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, multisample_count, GL_RGBA8, target_width, target_height);
        glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo[0]);
        if (depth_stencil)
        {   // allocate storage for the depth+stencil buffer:
            glBindRenderbuffer(GL_RENDERBUFFER, rbo[1]);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, multisample_count, GL_DEPTH_STENCIL, target_width, target_height);
            glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo[1]);
        }
    }
    else
    {   // allocate storage for the color buffer:
        glBindRenderbuffer(GL_RENDERBUFFER, rbo[0]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, target_width, target_height);
        glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo[0]);
        if (depth_stencil)
        {   // allocate storage for the depth+stencil buffer:
            glBindRenderbuffer(GL_RENDERBUFFER, rbo[1]);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_STENCIL, target_width, target_height);
            glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo[1]);
        }
    }
    GLenum completeness = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
    if (completeness   != GL_FRAMEBUFFER_COMPLETE)
    {
        OutputDebugString(_T("ERROR: Incomplete framebuffer on display: "));
        OutputDebugString(display->DisplayInfo.DeviceName);
        OutputDebugString(_T(".\n"));
    }

    // default the viewport to the entire render target bounds:
    glViewport(0, 0, target_width, target_height);

    // allocate memory for the renderer state:
    gl = (gl3_renderer_t*)  malloc(sizeof(gl3_renderer_t));

    gl->Width           = target_width;
    gl->Height          = target_height;
    gl->Drawable        = display->DisplayDC;
    gl->Window          = display->DisplayHWND;
    gl->Display         = display;

    // initialize the queue for receiving submitted command lists:
    pr_command_queue_init(&gl->CommandQueue);

    // save names of local display resources:
    gl->RenderFlags     = GLRC_RENDER_FLAGS_HEADLESS;
    gl->Framebuffer     = fbo;
    gl->Renderbuffer[0] = rbo[0];
    gl->Renderbuffer[1] = rbo[1];

    // initialize a dynamic list holding interfaces used to submit 
    // image cache control commands from the presentation thread.
    gl->CacheCount    = 0;
    gl->CacheCapacity = 4;
    gl->CacheList     =(thread_image_cache_t*) malloc(4 * sizeof(thread_image_cache_t));

    // initialize a dynamic list of images referenced by in-flight frames.
    gl->ImageCount    = 0;
    gl->ImageCapacity = 8;
    gl->ImageIds      =(gl_image_id_t       *) malloc(8 * sizeof(gl_image_id_t));
    gl->ImageData     =(gl_image_data_t     *) malloc(8 * sizeof(gl_image_data_t));
    gl->ImageRefs     =(size_t              *) malloc(8 * sizeof(size_t));

    // initialize queues for receiving image data from the host:
    mpsc_fifo_u_init(&gl->ImageLockQueue);
    mpsc_fifo_u_init(&gl->ImageErrorQueue);

    // initialize the queue of in-flight frame state:
    gl->FrameHead = 0;
    gl->FrameTail = 0;
    gl->FrameMask = GLRC_MAX_FRAMES - 1;
    memset(&gl->FrameQueue, 0, GLRC_MAX_FRAMES * sizeof(gl_frame_t));
    return (uintptr_t) gl;
}

/// @summary Resets the state of the renderer, keeping it attached to its current display and drawable.
/// @param handle The handle of the renderer to reset.
public_function void reset_renderer(uintptr_t handle)
{
    gl3_renderer_t    *gl = (gl3_renderer_t*) handle;
    gl_display_t *display =  gl->Display;

    // flush the image command queues:
    mpsc_fifo_u_flush(&gl->ImageErrorQueue);
    mpsc_fifo_u_flush(&gl->ImageLockQueue);
    
    // clear the global image list:
    gl->ImageCount = 0;
    
    // reset the in-flight frame queue:
    gl->FrameHead  = 0;
    gl->FrameTail  = 0;
    for (size_t i  = 0; i < GLRC_MAX_FRAMES; ++i)
    {
        gl->FrameQueue[i].CommandList = NULL;
        gl->FrameQueue[i].FrameState  = GLRC_FRAME_STATE_WAIT_FOR_IMAGES;
        gl->FrameQueue[i].ImageCount  = 0;
    }
    
    // reset the command queue:
    pr_command_queue_clear(&gl->CommandQueue);

    // OpenGL commands target the renderer drawable:
    target_drawable(gl->Display, gl->Drawable);
    
    // reset the framebufer and viewport:
    glBindFramebuffer(GL_DRAW_BUFFER, gl->Framebuffer);
    glViewport(0, 0, gl->Width, gl->Height);
}

/// @summary Resets the state of the renderer when its attached drawable changes displays.
/// @param handle The handle of the renderer to reset.
/// @param display The display that owns the drawable.
/// @param window The window to which rendered content will be presented.
public_function void reset_renderer(uintptr_t handle, gl_display_t *display, HWND window)
{
    gl3_renderer_t  *gl =(gl3_renderer_t*) handle;

    if (gl->RenderFlags & GLRC_RENDER_FLAGS_HEADLESS)
    {   // OpenGL commands target the active drawable:
        target_drawable(gl->Display, gl->Drawable);

        // free headless display resources:
        if (gl->Framebuffer != 0)
        {
            glDeleteFramebuffers(1, &gl->Framebuffer);
            gl->Framebuffer  = 0;
        }
        if (gl->Renderbuffer[0] != 0)
        {
            glDeleteRenderbuffers(1, &gl->Renderbuffer[0]);
            gl->Renderbuffer[0] = 0;
        }
        if (gl->Renderbuffer[1] != 0)
        {
            glDeleteRenderbuffers(1, &gl->Renderbuffer[1]);
            gl->Renderbuffer[1] = 0;
        }

        // change the renderer type to windowed:
        gl->RenderFlags &=~GLRC_RENDER_FLAGS_HEADLESS;
        gl->RenderFlags |= GLRC_RENDER_FLAGS_WINDOW;

        // NULL-out the window and drawable references.
        // these referenced the old display, but don't have to be released.
        gl->Drawable = NULL;
        gl->Window   = NULL;
    }
    else
    {   // already windowed; release the current display context.
        ReleaseDC(gl->Window, gl->Drawable);
    }

    // retrieve the client bounds of the new window:
    RECT rcclient = {};
    GetClientRect(window, &rcclient);

    // update renderer state:
    gl->Width     = size_t(rcclient.right  - rcclient.left);
    gl->Height    = size_t(rcclient.bottom - rcclient.top);
    gl->Drawable  = GetDC(window);
    gl->Window    = window;
    gl->Display   = display;

    // reset the remaining renderer state:
    reset_renderer(handle);
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
    gl3_renderer_t    *gl = (gl3_renderer_t*) handle;
    gl_display_t *display =  gl->Display;
    RECT         rcclient = {0, 0, (LONG) gl->Width, (LONG) gl->Height}; 

    if (gl->RenderFlags & GLRC_RENDER_FLAGS_WINDOW)
    {   // retrieve the current window bounds.
        GetClientRect(gl->Window, &rcclient);
        gl->Width   = size_t(rcclient.right  - rcclient.left);
        gl->Height  = size_t(rcclient.bottom - rcclient.top );
    }

    // OpenGL commands target the current drawable:
    target_drawable(gl->Display, gl->Drawable);

    // bind the output renderbuffer, and set the viewport to the entire region:
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gl->Framebuffer);
    glViewport(0, 0, gl->Width, gl->Height);

    // launch as many frames as possible:
    while (frame_count(gl) < GLRC_MAX_FRAMES)
    {
        pr_command_list_t *cmdlist = pr_command_queue_next_submitted(&gl->CommandQueue);
        if (cmdlist == NULL) break;  // no more pending frames
        launch_frame(gl, cmdlist);
    }

    // process any pending image lock errors:
    image_cache_error_t lock_error;
    while (mpsc_fifo_u_consume(&gl->ImageErrorQueue, lock_error))
    {   // locate the image in the global image list:
        for (size_t gi = 0, gn = gl->ImageCount; gi < gn; ++gi)
        {
            if (gl->ImageIds [gi].ImageId    == lock_error.ImageId && 
                gl->ImageIds [gi].FrameIndex == lock_error.FirstFrame)
            {   // save the error code. it will be checked later.
                gl->ImageData[gi].ErrorCode   = lock_error.ErrorCode;
                break;
            }
        }
    }

    // process any completed image locks:
    image_cache_result_t lock_result;
    while (mpsc_fifo_u_consume(&gl->ImageLockQueue, lock_result))
    {   // locate the image in the global image list:
        for (size_t gi = 0, gn = gl->ImageCount; gi < gn; ++gi)
        {
            if (gl->ImageIds [gi].ImageId    == lock_result.ImageId && 
                gl->ImageIds [gi].FrameIndex == lock_result.FrameIndex)
            {   // save the image metadata and host memory pointer.
                gl->ImageData[gi].ErrorCode         = ERROR_SUCCESS;
                gl->ImageData[gi].SourceFormat      = lock_result.ImageFormat;
                gl->ImageData[gi].SourceCompression = lock_result.Compression;
                gl->ImageData[gi].SourceEncoding    = lock_result.Encoding;
                gl->ImageData[gi].SourceWidth       = lock_result.LevelInfo[0].Width;
                gl->ImageData[gi].SourceHeight      = lock_result.LevelInfo[0].Height;
                gl->ImageData[gi].SourcePitch       = lock_result.LevelInfo[0].BytesPerRow;
                gl->ImageData[gi].SourceData        =(uint8_t*) lock_result.BaseAddress;
                gl->ImageData[gi].SourceSize        = lock_result.BytesReserved;
                break;
            }
        }
    }
    
    // update the state of all in-flight frames:
    size_t frame_id  = gl->FrameHead;
    while (frame_count(gl) > 0 &&  frame_id != gl->FrameTail)
    {
        size_t fifo_index  = size_t(frame_id & gl->FrameMask);
        gl_frame_t *frame  = &gl->FrameQueue[fifo_index];

        if (frame->FrameState == GLRC_FRAME_STATE_WAIT_FOR_IMAGES)
        {   // have all pending image locks completed successfully?
            size_t n_expected  = frame->ImageCount;
            size_t n_success   = 0;
            size_t n_error     = 0;
            for (size_t li = 0, ln = frame->ImageCount; li < ln; ++li)
            {   // locate this frame in the global image list.
                for (size_t gi = 0, gn = gl->ImageCount; gi < gn; ++gi)
                {
                    if (frame->ImageIds[li].ImageId     == gl->ImageIds[gi].ImageId && 
                        frame->ImageIds[li].FrameIndex  == gl->ImageIds[gi].FrameIndex)
                    {
                        if (gl->ImageData[gi].ErrorCode != ERROR_SUCCESS)
                        {   // the image could not be locked in host memory.
                            n_error++;
                        }
                        else if (gl->ImageData[gi].SourceData != NULL)
                        {   // the image was locked in host memory and is ready for use.
                            n_success++;
                        }
                        // else, the lock request hasn't completed yet.
                        break;
                    }
                }
            }
            if (n_success == n_expected)
            {   // all images are ready, wait for compute jobs.
                frame->FrameState = GLRC_FRAME_STATE_WAIT_FOR_COMPUTE;
            }
            if (n_error > 0)
            {   // at least one image could not be locked. complete immediately.
                frame->FrameState = GLRC_FRAME_STATE_ERROR;
            }
        }

        // TODO(rlk): launch any compute jobs whose dependencies are satisfied. 
        
        if (frame->FrameState == GLRC_FRAME_STATE_WAIT_FOR_COMPUTE)
        {   // have all pending compute jobs completed successfully?
            // TODO(rlk): when we have compute jobs implemented...
            frame->FrameState  = GLRC_FRAME_STATE_WAIT_FOR_DISPLAY;
        }

        // TODO(rlk): launch any display jobs whose dependencies are satisfied.

        if (frame->FrameState == GLRC_FRAME_STATE_WAIT_FOR_DISPLAY)
        {   // have all pending display jobs completed successfully?
            // TODO(rlk): when we have display jobs implemented...
            frame->FrameState  = GLRC_FRAME_STATE_COMPLETE;
        }

        if (frame->FrameState == GLRC_FRAME_STATE_COMPLETE)
        {   // there's no work remaining for this frame.
        }
        
        if (frame->FrameState == GLRC_FRAME_STATE_ERROR)
        {   // TODO(rlk): cancel any in-progress jobs.
        }

        // check the next pending frame.
        ++frame_id;
    }

    // if the oldest frame has completed, retire it.
    if (frame_count(gl) > 0)
    {
        size_t oldest_index = size_t(gl->FrameHead & gl->FrameMask);
        gl_frame_t   *frame =&gl->FrameQueue[oldest_index];

        if (frame->FrameState == GLRC_FRAME_STATE_COMPLETE)
        {   // execute the display list. all data is ready.
            bool       end_of_frame = false;
            uint8_t const *read_ptr = frame->CommandList->CommandData;
            uint8_t const *end_ptr  = frame->CommandList->CommandData + frame->CommandList->BytesUsed;
            while (read_ptr < end_ptr && !end_of_frame)
            {
                pr_command_t *cmd_info = (pr_command_t*) read_ptr;
                size_t        cmd_size =  PR_COMMAND_SIZE_BASE + cmd_info->DataSize;
                switch (cmd_info->CommandId)
                {
                case PR_COMMAND_END_OF_FRAME:
                    {   // break out of the command processing loop and submit
                        // submit the translated command list to the system driver.
                        end_of_frame = true;
                    }
                    break;
                case PR_COMMAND_CLEAR_COLOR_BUFFER:
                    {
                        pr_color_t  *c = (pr_color_t*) cmd_info->Data;
                        glClearColor(c->Red, c->Green, c->Blue, c->Alpha);
                        glClear(GL_COLOR_BUFFER_BIT);
                    }
                    break;
                }
                // advance to the start of the next buffered command.
                read_ptr += cmd_size;
            }
            // signal to any waiters that processing of the command list has finished.
            SetEvent(frame->CommandList->SyncHandle);
        }
        if (frame->FrameState == GLRC_FRAME_STATE_COMPLETE || 
            frame->FrameState == GLRC_FRAME_STATE_ERROR)
        {
            retire_frame(gl);
        }
    }
}

/// @summary Copies or swaps the backbuffer to the front buffer.
/// @param handle The handle of the renderer associated with the window.
public_function void present_drawable(uintptr_t handle)
{
    gl3_renderer_t  *gl = (gl3_renderer_t*) handle;
    if (gl->RenderFlags & GLRC_RENDER_FLAGS_WINDOW)
    {   // only renderers attached to a window need to swap buffers.
        swap_buffers(gl->Drawable);
    }
}

/// @summary Frees resources associated with a renderer and detaches it from its drawable.
/// @param handle The renderer handle, returned by one of the create_renderer functions.
public_function void delete_renderer(uintptr_t handle)
{
    gl3_renderer_t     *gl = (gl3_renderer_t*) handle;
    gl_display_t  *display =  gl->Display;

    // TODO(rlk): The display manages all of the shared resources.

    // free resources associated with the frame queue:
    for (size_t i = 0; i < GLRC_MAX_FRAMES; ++i)
    {
        gl_frame_t &frame = gl->FrameQueue[i];
        free(frame.ImageIds);
    }
    
    // free resources associated with the image queues:
    mpsc_fifo_u_delete(&gl->ImageErrorQueue);
    mpsc_fifo_u_delete(&gl->ImageLockQueue);

    // free resources associated with the global image list:
    free(gl->ImageRefs);
    free(gl->ImageData);
    free(gl->ImageIds);

    // free resources associated with the image cache list:
    for (size_t i = 0, n = gl->CacheCapacity; i < n; ++i)
    {
        gl->CacheList[i].dispose();
    }
    free(gl->CacheList);

    // OpenGL commands target the current drawable:
    target_drawable(gl->Display, gl->Drawable);

    // delete private display resources:
    if (gl->Framebuffer != 0)
    {
        glDeleteFramebuffers(1, &gl->Framebuffer);
        gl->Framebuffer  = 0;
    }
    if (gl->Renderbuffer[0] != 0)
    {
        glDeleteRenderbuffers(1, &gl->Renderbuffer[0]);
        gl->Renderbuffer[0] = 0;
    }
    if (gl->Renderbuffer[1] != 0)
    {
        glDeleteRenderbuffers(1, &gl->Renderbuffer[1]);
        gl->Renderbuffer[1] = 0;
    }
    if (gl->RenderFlags & GLRC_RENDER_FLAGS_WINDOW)
    {   // release the drawable reference:
        ReleaseDC(gl->Window, gl->Drawable);
    }

    // free the command queue, and then the render state memory itself:
    pr_command_queue_delete(&gl->CommandQueue);
    free(gl);
}
