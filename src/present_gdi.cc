/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines the exported presentation DLL entry points for the GDI 
/// presentation library implementation.
///////////////////////////////////////////////////////////////////////////80*/

#pragma warning (disable:4505) // unreferenced local function was removed

#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

/*////////////////
//   Includes   //
////////////////*/
#include <Windows.h>
#include <objbase.h>
#include <ShlObj.h>
#include <tchar.h>

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <float.h>

#include "intrinsics.h"
#include "atomic_fifo.h"

#include "runtime.cc"

#include "idtable.cc"
#include "strtable.cc"
#include "parseutl.cc"

#include "filepath.cc"
#include "iobuffer.cc"
#include "aiodriver.cc"
#include "iodecoder.cc"
#include "piodriver.cc"
#include "vfsdriver.cc"
#include "threadio.cc"

#include "imtypes.cc"
#include "immemory.cc"
#include "imencode.cc"
#include "imparser.cc"
#include "imparser_dds.cc"
#include "imloader.cc"
#include "imcache.cc"

#include "prcmdlist.cc"

#include "cpipeline.cc"
#include "gpipeline.cc"
#include "gpipeline_gdi.cc"

/*/////////////////
//   Constants   //
/////////////////*/
/// @summary Define the maximum number of in-flight frames per-driver.
static uint32_t const PRESENT_DRIVER_GDI_MAX_FRAMES = 4;

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Define the possible states of an in-flight frame.
enum gdi_frame_state_e : int
{
    GDI_FRAME_STATE_WAIT_FOR_IMAGES     = 0,   /// The frame is waiting for images to be locked in host memory.
    GDI_FRAME_STATE_WAIT_FOR_COMPUTE    = 1,   /// The frame is waiting for compute jobs to complete.
    GDI_FRAME_STATE_WAIT_FOR_DISPLAY    = 2,   /// The frame is waiting for display jobs to complete.
    GDI_FRAME_STATE_COMPLETE            = 3,   /// The frame has completed.
    GDI_FRAME_STATE_ERROR               = 4,   /// The frame encountered one or more errors.
};

/// @summary Represents a unique identifier for a locked image.
struct gdi_image_id_t
{
    uintptr_t               ImageId;           /// The application-defined image identifier.
    size_t                  FrameIndex;        /// The zero-based index of the frame.
};

/// @summary Defines the data stored for a locked image.
struct gdi_image_data_t
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
struct gdi_frame_t
{
    int                     FrameState;        /// One of gdi_frame_state_e indicating the current state of the frame.

    pr_command_list_t      *CommandList;       /// The command list that generated this frame.

    size_t                  ImageCount;        /// The number of images referenced in this frame.
    size_t                  ImageCapacity;     /// The capacity of the frame image list.
    gdi_image_id_t         *ImageIds;          /// The unique identifiers of the referenced images.
};

/// @summary Define all of the state associated with a GDI display driver. This display
/// driver is compatible with Windows versions all the way back to Windows 95. Even 
/// though technically it is not required to call CreateDIBSection, the driver does so 
/// because some operations may need to render into the bitmap through the device context.
struct present_driver_gdi_t
{
    typedef image_cache_result_queue_t             image_lock_queue_t;
    typedef image_cache_error_queue_t              image_error_queue_t;

    size_t                  Pitch;             /// The number of bytes per-scanline in the image.
    size_t                  Height;            /// The number of scanlines in the image.
    uint8_t                *DIBMemory;         /// The DIBSection memory, allocated with VirtualAlloc.
    size_t                  Width;             /// The actual width of the image, in pixels.
    size_t                  BytesPerPixel;     /// The number of bytes allocated for each pixel.
    HDC                     MemoryDC;          /// The memory device context used for rendering operations.
    HWND                    Window;            /// The handle of the target window.
    HBITMAP                 DIBSection;        /// The DIBSection backing the memory device context.
    BITMAPINFO              BitmapInfo;        /// A description of the bitmap layout and attributes.
    pr_command_queue_t      CommandQueue;      /// The driver's command list submission queue.

    size_t                  CacheCount;        /// The number of thread-local cache writers.
    size_t                  CacheCapacity;     /// The maximum capacity of the presentation thread image cache list.
    thread_image_cache_t   *CacheList;         /// The list of interfaces used to access image caches from the presentation thread.

    size_t                  ImageCount;        /// The number of locked images.
    size_t                  ImageCapacity;     /// The maximum capacity of the locked image list.
    gdi_image_id_t         *ImageIds;          /// The list of locked image identifiers.
    gdi_image_data_t       *ImageData;         /// The list of locked image data descriptors.
    size_t                 *ImageRefs;         /// The list of reference counts for each locked image.

    image_lock_queue_t      ImageLockQueue;    /// The MPSC unbounded FIFO where completed image lock requests are stored.
    image_error_queue_t     ImageErrorQueue;   /// The MPSC unbounded FIFO where failed image lock requests are stored.

    #define MAXF            PRESENT_DRIVER_GDI_MAX_FRAMES
    uint32_t                FrameHead;         /// The zero-based index of the oldest in-flight frame.
    uint32_t                FrameTail;         /// The zero-based index of the newest in-flight frame.
    uint32_t                FrameMask;         /// The bitmask used to map an index into the frame array.
    gdi_frame_t             FrameQueue[MAXF];  /// Storage for in-flight frame data.
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
/// @summary Convert a floating point value, normally in [0, 1], to an unsigned 8-bit
/// integer value, clamping to the range [0, 255].
/// @param value The input value.
/// @return A value in [0, 255].
internal_function inline uint8_t float_to_u8_sat(float value)
{
    float   scaled = value * 255.0f;
    return (scaled > 255.0f) ? 255 : uint8_t(scaled);
}

/// @summary Calculate the number of in-flight frames.
/// @param driver The presentation driver to query.
/// @return The number of in-flight frames.
internal_function inline size_t frame_count(present_driver_gdi_t *driver)
{
    return size_t(driver->FrameTail - driver->FrameHead);
}

/// @summary Prepare an image for use by locking its data in host memory. The lock request completes asynchronously.
/// @param driver The presentation driver handling the request.
/// @param frame The in-flight frame state requiring the image data.
/// @param image A description of the portion of the image required for use.
internal_function void prepare_image(present_driver_gdi_t *driver, gdi_frame_t *frame, pr_image_subresource_t *image)
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
            gdi_image_id_t *il =(gdi_image_id_t*)   realloc(frame->ImageIds , new_amount * sizeof(gdi_image_id_t));
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
        for (size_t gi = 0, gn = driver->ImageCount; gi < gn; ++gi)
        {
            if (driver->ImageIds[gi].ImageId    == image_id && 
                driver->ImageIds[gi].FrameIndex == frame_index)
            {   // this image already exists in the global image list.
                // there's no need to lock it; just increment the reference count.
                driver->ImageRefs[gi]++;
                found_image = true;
                break;
            }
        }
        // if the image wasn't in the global list, add it.
        if (!found_image)
        {
            if (driver->ImageCount == driver->ImageCapacity)
            {   // increase the capacity of the global image list.
                size_t old_amount   = driver->ImageCapacity;
                size_t new_amount   = calculate_capacity(old_amount, old_amount+1, 1024 , 1024);
                gdi_image_id_t       *il = (gdi_image_id_t  *) realloc(driver->ImageIds , new_amount * sizeof(gdi_image_id_t));
                gdi_image_data_t     *dl = (gdi_image_data_t*) realloc(driver->ImageData, new_amount * sizeof(gdi_image_data_t));
                size_t               *rl = (size_t          *) realloc(driver->ImageRefs, new_amount * sizeof(size_t));
                if (il != NULL) driver->ImageIds    = il;
                if (dl != NULL) driver->ImageData   = dl;
                if (rl != NULL) driver->ImageRefs   = rl;
                if (il != NULL && dl != NULL && rl != NULL) driver->ImageCapacity = new_amount;
            }
            global_index    = driver->ImageCount++;
            driver->ImageRefs[global_index]                   = 1;
            driver->ImageIds [global_index].ImageId           = image_id;
            driver->ImageIds [global_index].FrameIndex        = frame_index;
            driver->ImageData[global_index].ErrorCode         = ERROR_SUCCESS;
            driver->ImageData[global_index].SourceFormat      = DXGI_FORMAT_UNKNOWN;
            driver->ImageData[global_index].SourceCompression = IMAGE_COMPRESSION_NONE;
            driver->ImageData[global_index].SourceEncoding    = IMAGE_ENCODING_RAW;
            driver->ImageData[global_index].SourceCache       = NULL;
            driver->ImageData[global_index].SourceWidth       = 0;
            driver->ImageData[global_index].SourceHeight      = 0;
            driver->ImageData[global_index].SourcePitch       = 0;
            driver->ImageData[global_index].SourceData        = NULL;
            driver->ImageData[global_index].SourceSize        = 0;
            // locate the presentation thread cache interface for the source data:
            bool found_cache = false;
            for (size_t ci = 0, cn = driver->CacheCount; ci < cn; ++ci)
            {
                if (driver->CacheList[ci].Cache == image->ImageSource)
                {
                    driver->ImageData[global_index].SourceCache = &driver->CacheList[ci];
                    found_cache = true;
                    break;
                }
            }
            if (!found_cache)
            {   // create a new interface to write to this image cache.
                if (driver->CacheCount == driver->CacheCapacity)
                {   // increase the capacity of the image cache list.
                    size_t old_amount   = driver->CacheCapacity;
                    size_t new_amount   = calculate_capacity(old_amount, old_amount+1, 16, 16);
                    thread_image_cache_t *cl = (thread_image_cache_t*)   realloc(driver->CacheList, new_amount * sizeof(thread_image_cache_t));
                    if (cl != NULL)
                    {
                        driver->CacheList     = cl;
                        driver->CacheCapacity = new_amount;
                    }
                }
                thread_image_cache_t *cache = &driver->CacheList[driver->CacheCount++];
                driver->ImageData[global_index].SourceCache = cache;
                cache->initialize(image->ImageSource);
            }
            // finally, submit a lock request for the frame.
            driver->ImageData[global_index].SourceCache->lock( image_id, frame_index, frame_index, &driver->ImageLockQueue, &driver->ImageErrorQueue, 0);
        }
    }
}

/// @summary Attempt to start a new in-flight frame.
/// @param driver The presentation driver managing the frame.
/// @param cmdlist The presentation command list defining the frame.
/// @return true if the frame was launched, or false if too many frames are in-flight.
internal_function bool launch_frame(present_driver_gdi_t *driver, pr_command_list_t *cmdlist)
{   // insert a new frame at the tail of the queue.
    if (frame_count(driver) == PRESENT_DRIVER_GDI_MAX_FRAMES)
    {   // there are too many frames in-flight.
        return false;
    }
    uint32_t     id    = driver->FrameTail++;
    size_t       index = size_t(id & driver->FrameMask); 
    gdi_frame_t *frame =&driver->FrameQueue[index];
    frame->FrameState  = GDI_FRAME_STATE_WAIT_FOR_IMAGES;
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
            }
            break;
        case PR_COMMAND_PREPARE_IMAGE:
            {   // lock the source data in host memory.
                prepare_image(driver, frame, (pr_image_subresource_t*) cmd_info->Data);
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
/// @param driver The presentation driver to update.
internal_function void retire_frame(present_driver_gdi_t *driver)
{   // retire the frame at the head of the queue.
    uint32_t     id    = driver->FrameHead++;
    size_t       index = size_t(id & driver->FrameMask);
    gdi_frame_t *frame =&driver->FrameQueue[index];
    gdi_image_id_t *gl = driver->ImageIds;
    gdi_image_id_t *ll = frame->ImageIds;
    for (size_t li = 0 , ln = frame->ImageCount; li < ln; ++li)
    {   // locate the corresponding entry in the global list.
        gdi_image_id_t &lid = ll[li];
        for (size_t gi = 0; gi < driver->ImageCount; ++gi)
        {   gdi_image_id_t &gid  = gl[gi];
            if (lid.ImageId == gid.ImageId && lid.FrameIndex == gid.FrameIndex)
            {   // found a match. decrement the reference count.
                if (driver->ImageRefs[gi]-- == 1)
                {   // the image reference count has dropped to zero.
                    // unlock the image data, and delete the entry from the global list.
                    size_t  last_index  = driver->ImageCount - 1;
                    driver->ImageData[gi].SourceCache->unlock(gid.ImageId, gid.FrameIndex, gid.FrameIndex);
                    driver->ImageIds [gi] = driver->ImageIds [last_index];
                    driver->ImageData[gi] = driver->ImageData[last_index];
                    driver->ImageRefs[gi] = driver->ImageRefs[last_index];
                    driver->ImageCount--;
                }
                break;
            }
        }
    }
    // TODO(rlk): cleanup compute jobs
    // TODO(rlk): cleanup display state
    pr_command_queue_return(&driver->CommandQueue, frame->CommandList);
    frame->ImageCount = 0;
}

/*///////////////////////
//  Public Functions   //
///////////////////////*/
/// @summary Perform the initialization necessary to set up both Direct3D and Direct2D 
/// and their associated swap chain, and attach the interfaces to a given window. Each
/// window maintains its own device (generally just a command queue and some state.)
/// @param window The window to which the display driver instance is attached.
/// @return A handle to the display driver used to render to the given window.
uintptr_t __cdecl PrDisplayDriverOpen(HWND window)
{
    present_driver_gdi_t *driver = (present_driver_gdi_t*) malloc(sizeof(present_driver_gdi_t));
    if (driver == NULL)   return 0;

    // initialize the fields of the driver structure to be invalid.
    driver->Pitch         = 0;
    driver->Height        = 0;
    driver->DIBMemory     = NULL;
    driver->Width         = 0;
    driver->BytesPerPixel = 0;
    driver->MemoryDC      = NULL;
    driver->Window        = NULL;
    driver->DIBSection    = NULL;
    ZeroMemory(&driver->BitmapInfo, sizeof(driver->BitmapInfo));

    driver->CacheCount    = 0;
    driver->CacheCapacity = 0;
    driver->CacheList     = NULL;
    
    driver->ImageCount    = 0;
    driver->ImageCapacity = 0;
    driver->ImageIds      = NULL;
    driver->ImageData     = NULL;
    driver->ImageRefs     = NULL;
    
    driver->FrameHead     = 0;
    driver->FrameTail     = 0;
    driver->FrameMask     = PRESENT_DRIVER_GDI_MAX_FRAMES-1;
    ZeroMemory(&driver->FrameQueue, sizeof(gdi_frame_t) * PRESENT_DRIVER_GDI_MAX_FRAMES);

    // retrieve the current dimensions of the client area of the window.
    RECT rc;
    GetClientRect(window, &rc);
    int width   =(int)    (rc.right  - rc.left);
    int height  =(int)    (rc.bottom - rc.top);

    // create a memory device context compatible with the screen (window):
    HDC win_dc  = GetDC(window);
    HDC mem_dc  = CreateCompatibleDC(win_dc);
    if (mem_dc == NULL)
    {
        OutputDebugString(_T("ERROR: Cannot create memory device context.\n"));
        ReleaseDC(window, win_dc);
        free(driver);
        return (uintptr_t) 0;
    }
    // the window DC is no longer required so release it back to the pool.
    ReleaseDC(window, win_dc);

    // initialize the DIB section description.
    BITMAPINFO  bmi;
    ZeroMemory(&bmi, sizeof(BITMAPINFO));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = width;
    bmi.bmiHeader.biHeight      =-height;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    // create the DIBSection itself, and select it into the memory DC
    // so it is used as the backing store for drawing operations.
    void *dib_p = NULL;
    HBITMAP dib = CreateDIBSection(mem_dc, &bmi, DIB_RGB_COLORS, &dib_p, NULL, 0);
    if (dib == NULL || dib_p == NULL)
    {
        OutputDebugString(_T("ERROR: Cannot create DIB section.\n"));
        DeleteDC(mem_dc);
        free(driver);
        return (uintptr_t) 0;
    }
    SelectObject(mem_dc, dib);

    // initialize the fields of the driver structure.
    driver->Pitch         = (size_t)  (width * 4);
    driver->Height        = (size_t)  (height);
    driver->DIBMemory     = (uint8_t*)(dib_p);
    driver->Width         = (size_t)  (width);
    driver->BytesPerPixel = (size_t)  (4);
    driver->MemoryDC      = mem_dc;
    driver->Window        = window;
    driver->DIBSection    = dib;
    CopyMemory(&driver->BitmapInfo, &bmi, sizeof(BITMAPINFO));

    // initialize a dynamic list holding interfaces used to submit 
    // image cache control commands from the presentation thread.
    driver->CacheCapacity = 4;
    driver->CacheList     =(thread_image_cache_t*) malloc(4 * sizeof(thread_image_cache_t));

    // initialize a dynamic list of images referenced by in-flight frames.
    driver->ImageCapacity = 8;
    driver->ImageIds      =(gdi_image_id_t      *) malloc(8 * sizeof(gdi_image_id_t));
    driver->ImageData     =(gdi_image_data_t    *) malloc(8 * sizeof(gdi_image_data_t));
    driver->ImageRefs     =(size_t              *) malloc(8 * sizeof(size_t));

    // initialize queues for receiving image data from the host:
    mpsc_fifo_u_init(&driver->ImageLockQueue);
    mpsc_fifo_u_init(&driver->ImageErrorQueue);

    // initialize the queue for receiving submitted command lists:
    pr_command_queue_init(&driver->CommandQueue);

    // finally, finished with driver initialization.
    return (uintptr_t)driver;
}

/// @summary Performs an explicit reinitialization of the display driver used to render 
/// content into a given window. This function is primarily used internally to handle 
/// detected device removal conditions, but may be called by the application as well.
/// @param drv The display driver handle returned by PrDisplayDriverOpen().
void __cdecl PrDisplayDriverReset(uintptr_t drv)
{
    present_driver_gdi_t   *driver = (present_driver_gdi_t*) drv;
    pr_command_queue_clear(&driver->CommandQueue);
}

/// @summary Handles the case where the window associated with a display driver is resized.
/// This function should be called in response to a WM_SIZE or WM_DISPLAYCHANGE event.
/// @param drv The display driver handle returned by PrDisplayDriverResize().
void __cdecl PrDisplayDriverResize(uintptr_t drv)
{
    present_driver_gdi_t *driver = (present_driver_gdi_t*) drv;
    if (driver == NULL)   return;

    // retrieve the current dimensions of the client area of the window.
    RECT rc;
    GetClientRect(driver->Window, &rc);
    int width  = (int)    (rc.right  - rc.left);
    int height = (int)    (rc.bottom - rc.top);

    if (driver->Width == size_t(width) && driver->Height == size_t(height))
    {   // there's no need to reallocate the DIB section memory.
        return;
    }

    // the window dimensions changed. free the existing DIB memory and allocate anew.
    if (driver->DIBMemory != NULL)
    {   // release the existing memory and reset driver fields.
        DeleteDC(driver->MemoryDC);
        DeleteObject(driver->DIBSection);
        driver->Pitch         = 0;
        driver->Height        = 0;
        driver->DIBMemory     = NULL;
        driver->Width         = 0;
        driver->BytesPerPixel = 0;
        driver->MemoryDC      = NULL;
        driver->DIBSection    = NULL;
        ZeroMemory(&driver->BitmapInfo, sizeof(driver->BitmapInfo));
    }

    // create a memory device context compatible with the screen (window):
    HDC win_dc  = GetDC(driver->Window);
    HDC mem_dc  = CreateCompatibleDC(win_dc);
    if (mem_dc == NULL)
    {
        OutputDebugString(_T("ERROR: Cannot create memory device context (on resize).\n"));
        ReleaseDC(driver->Window, win_dc);
        return;
    }
    // the window DC is no longer required so release it back to the pool.
    ReleaseDC(driver->Window, win_dc);

    // initialize the DIB section description.
    BITMAPINFO  bmi;
    ZeroMemory(&bmi, sizeof(BITMAPINFO));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = width;
    bmi.bmiHeader.biHeight      =-height;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    // create the DIBSection itself, and select it into the memory DC
    // so it is used as the backing store for drawing operations.
    void *dib_p = NULL;
    HBITMAP dib = CreateDIBSection(mem_dc, &bmi, DIB_RGB_COLORS, &dib_p, NULL, 0);
    if (dib == NULL || dib_p == NULL)
    {
        OutputDebugString(_T("ERROR: Cannot create DIB section (on resize).\n"));
        DeleteDC(mem_dc);
        return;
    }
    SelectObject(mem_dc, dib);

    // initialize the fields of the driver structure. we always allocate 32bpp bitmaps.
    driver->Pitch         = (size_t)  (width * 4);
    driver->Height        = (size_t)  (height);
    driver->DIBMemory     = (uint8_t*)(dib_p);
    driver->Width         = (size_t)  (width);
    driver->BytesPerPixel = (size_t)  (4);
    driver->MemoryDC      = mem_dc;
    driver->DIBSection    = dib;
    CopyMemory(&driver->BitmapInfo, &bmi, sizeof(BITMAPINFO));
}

/// @summary Allocates a new command list for use by the application. The 
/// application should write display commands to the command list and then 
/// submit the list for processing by the presentation driver.
/// @param drv The display driver handle returned by PrDisplayDriverOpen().
/// @return An empty command list, or NULL if no command lists are available.
pr_command_list_t* __cdecl PrCreateCommandList(uintptr_t drv)
{
    present_driver_gdi_t *driver = (present_driver_gdi_t*) drv;
    if (driver == NULL)   return NULL;
    return pr_command_queue_next_available(&driver->CommandQueue);
}

/// @summary Submits a list of buffered display commands for processing.
/// @param drv The display driver handle returned by PrDisplayDriverOpen().
/// @param cmdlist The list of buffered display commands being submitted.
/// @param wait Specify true to block the calling thread until the command list is processed by the driver.
/// @param wait_timeout The maximum amount of time to wait, in milliseconds, if wait is also true.
void __cdecl PrSubmitCommandList(uintptr_t drv, pr_command_list_t *cmdlist, bool wait, uint32_t wait_timeout)
{
    present_driver_gdi_t *driver = (present_driver_gdi_t*) drv;
    if (driver == NULL)   return;
    HANDLE sync = cmdlist->SyncHandle;
    pr_command_queue_submit(&driver->CommandQueue, cmdlist);
    if (wait) safe_wait(sync, wait_timeout);
}

/// @summary Copies the current frame to the application window.
/// @param drv The display driver handle returned by PrDisplayDriverOpen().
void __cdecl PrPresentFrameToWindow(uintptr_t drv)
{
    present_driver_gdi_t *driver = (present_driver_gdi_t*) drv;
    if (driver == NULL)   return;

    // launch as many frames as possible:
    while (frame_count(driver) < PRESENT_DRIVER_GDI_MAX_FRAMES)
    {
        pr_command_list_t *cmdlist = pr_command_queue_next_submitted(&driver->CommandQueue);
        if (cmdlist == NULL) break;  // no more pending frames
        launch_frame(driver, cmdlist);
    }

    // process any pending image lock errors:
    image_cache_error_t lock_error;
    while (mpsc_fifo_u_consume(&driver->ImageErrorQueue, lock_error))
    {   // locate the image in the global image list:
        for (size_t gi = 0, gn = driver->ImageCount; gi < gn; ++gi)
        {
            if (driver->ImageIds [gi].ImageId    == lock_error.ImageId && 
                driver->ImageIds [gi].FrameIndex == lock_error.FirstFrame)
            {   // save the error code. it will be checked later.
                driver->ImageData[gi].ErrorCode   = lock_error.ErrorCode;
                break;
            }
        }
    }

    // process any completed image locks:
    image_cache_result_t lock_result;
    while (mpsc_fifo_u_consume(&driver->ImageLockQueue, lock_result))
    {   // locate the image in the global image list:
        for (size_t gi = 0, gn = driver->ImageCount; gi < gn; ++gi)
        {
            if (driver->ImageIds [gi].ImageId    == lock_result.ImageId && 
                driver->ImageIds [gi].FrameIndex == lock_result.FrameIndex)
            {   // save the image metadata and host memory pointer.
                driver->ImageData[gi].ErrorCode         = ERROR_SUCCESS;
                driver->ImageData[gi].SourceFormat      = lock_result.ImageFormat;
                driver->ImageData[gi].SourceCompression = lock_result.Compression;
                driver->ImageData[gi].SourceEncoding    = lock_result.Encoding;
                driver->ImageData[gi].SourceWidth       = lock_result.LevelInfo[0].Width;
                driver->ImageData[gi].SourceHeight      = lock_result.LevelInfo[0].Height;
                driver->ImageData[gi].SourcePitch       = lock_result.LevelInfo[0].BytesPerRow;
                driver->ImageData[gi].SourceData        =(uint8_t*) lock_result.BaseAddress;
                driver->ImageData[gi].SourceSize        = lock_result.BytesReserved;
                break;
            }
        }
    }
    
    // update the state of all in-flight frames:
    size_t frame_id  = driver->FrameHead;
    while (frame_count(driver) > 0 && frame_id != driver->FrameTail)
    {
        size_t  fifo_index = size_t(frame_id & driver->FrameMask);
        gdi_frame_t *frame = &driver->FrameQueue[fifo_index];

        if (frame->FrameState == GDI_FRAME_STATE_WAIT_FOR_IMAGES)
        {   // have all pending image locks completed successfully?
            size_t n_expected  = frame->ImageCount;
            size_t n_success   = 0;
            size_t n_error     = 0;
            for (size_t li = 0, ln = frame->ImageCount; li < ln; ++li)
            {   // locate this frame in the global image list.
                for (size_t gi = 0, gn = driver->ImageCount; gi < gn; ++gi)
                {
                    if (frame->ImageIds[li].ImageId    == driver->ImageIds[gi].ImageId && 
                        frame->ImageIds[li].FrameIndex == driver->ImageIds[gi].FrameIndex)
                    {
                        if (driver->ImageData[gi].ErrorCode != ERROR_SUCCESS)
                        {   // the image could not be locked in host memory.
                            n_error++;
                        }
                        else if (driver->ImageData[gi].SourceData != NULL)
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
                frame->FrameState = GDI_FRAME_STATE_WAIT_FOR_COMPUTE;
            }
            if (n_error > 0)
            {   // at least one image could not be locked. complete immediately.
                frame->FrameState = GDI_FRAME_STATE_ERROR;
            }
        }

        // TODO(rlk): launch any compute jobs whose dependencies are satisfied. 
        
        if (frame->FrameState == GDI_FRAME_STATE_WAIT_FOR_COMPUTE)
        {   // have all pending compute jobs completed successfully?
            // TODO(rlk): when we have compute jobs implemented...
            frame->FrameState  = GDI_FRAME_STATE_WAIT_FOR_DISPLAY;
        }

        // TODO(rlk): launch any display jobs whose dependencies are satisfied.

        if (frame->FrameState == GDI_FRAME_STATE_WAIT_FOR_DISPLAY)
        {   // have all pending display jobs completed successfully?
            // TODO(rlk): when we have display jobs implemented...
            frame->FrameState  = GDI_FRAME_STATE_COMPLETE;
        }

        if (frame->FrameState == GDI_FRAME_STATE_COMPLETE)
        {   // there's no work remaining for this frame.
        }
        
        if (frame->FrameState == GDI_FRAME_STATE_ERROR)
        {   // TODO(rlk): cancel any in-progress jobs.
        }

        // check the next pending frame.
        ++frame_id;
    }

    // if the oldest frame has completed, retire it.
    if (frame_count(driver) > 0)
    {
        size_t oldest_index = size_t(driver->FrameHead & driver->FrameMask);
        gdi_frame_t  *frame =&driver->FrameQueue[oldest_index];

        if (frame->FrameState == GDI_FRAME_STATE_COMPLETE)
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
                        pr_color_t *c = (pr_color_t*) cmd_info->Data;
                        RECT fill_rc  = { 0, 0, (int) driver->Width, (int) driver->Height };
                        uint8_t    r  = float_to_u8_sat(c->Red);
                        uint8_t    g  = float_to_u8_sat(c->Green);
                        uint8_t    b  = float_to_u8_sat(c->Blue);
                        HBRUSH brush  = CreateSolidBrush(RGB(r, g, b));
                        FillRect(driver->MemoryDC, &fill_rc, brush);
                        DeleteObject(brush);
                    }
                    break;
                /*case PR_COMMAND_DRAW_IMAGE_2D:
                    {
                        BITMAPINFO bmi;
                        pr_draw_image2d_data_t *data = (pr_draw_image2d_data_t*) cmd_info->Data;
                        // TODO(rlk): support additional bitmap formats. if source is not a supported format, must convert.
                        // TODO(rlk): rotation is not supported, must perform the rotation in software (see handmade hero).
                        // TODO(rlk): red and blue channels are swapped
                        bmi.bmiHeader.biSize         = sizeof(BITMAPINFOHEADER);
                        bmi.bmiHeader.biWidth        = (LONG) data->ImageWidth; // TODO(rlk): alignment
                        bmi.bmiHeader.biHeight       =-(LONG) data->ImageHeight;
                        bmi.bmiHeader.biPlanes       = 1;
                        bmi.bmiHeader.biBitCount     = 32;
                        bmi.bmiHeader.biCompression  = BI_RGB;
                        bmi.bmiHeader.biSizeImage    = 0;
                        bmi.bmiHeader.biXPelsPerMeter= 0;
                        bmi.bmiHeader.biYPelsPerMeter= 0;
                        bmi.bmiHeader.biClrUsed      = 0;
                        bmi.bmiHeader.biClrImportant = 0;
                        SetStretchBltMode(driver->MemoryDC, COLORONCOLOR); // or HALFTONE, which is much more expensive (COLORONCOLOR=NEAREST, HALFTONE=LINEAR)
                        StretchDIBits(driver->MemoryDC, (int) data->TargetX, (int) data->TargetY, (int) data->TargetWidth, (int) data->TargetHeight, (int) data->SourceX, (int) data->SourceY, (int) data->SourceWidth, (int) data->SourceHeight, data->PixelData, &bmi, DIB_RGB_COLORS, SRCCOPY);
                        image_cache_unlock_frames(data->ImageCache, data->ImageId, data->FrameId, data->FrameId, 0, &driver->CacheCmdAlloc);
                    }
                    break;*/
                }
                // advance to the start of the next buffered command.
                read_ptr += cmd_size;
            }
            // signal to any waiters that processing of the command list has finished.
            SetEvent(frame->CommandList->SyncHandle);
        }
        if (frame->FrameState == GDI_FRAME_STATE_COMPLETE || 
            frame->FrameState == GDI_FRAME_STATE_ERROR)
        {
            retire_frame(driver);
        }
    }

    // now perform a 1-1 blit from the backbuffer to the screen/window.
    HDC win_dc  = GetDC(driver->Window);
    HDC mem_dc  = driver->MemoryDC;
    int width   = (int) driver->Width;
    int height  = (int) driver->Height;
    BitBlt(win_dc, 0, 0, width, height, mem_dc, 0, 0, SRCCOPY);
    ReleaseDC(driver->Window, win_dc);
}

/// @summary Closes a display driver instance and releases all associated resources.
/// @param drv The display driver handle returned by PrDisplayDriverOpen().
void __cdecl PrDisplayDriverClose(uintptr_t drv)
{
    present_driver_gdi_t *driver = (present_driver_gdi_t*) drv;
    if (driver == NULL)   return;
    if (driver->DIBMemory != NULL)
    {
        pr_command_queue_delete(&driver->CommandQueue);
        DeleteDC(driver->MemoryDC);
        DeleteObject(driver->DIBSection);
        driver->DIBMemory  = NULL;
        driver->DIBSection = NULL;
        driver->MemoryDC   = NULL;
        driver->Pitch      = 0;
    }
    mpsc_fifo_u_delete(&driver->ImageErrorQueue);
    mpsc_fifo_u_delete(&driver->ImageLockQueue);
    for (size_t i = 0, n = driver->CacheCount; i < n; ++i)
    {   // dispose of any cache writer data for caches that have been initialized.
        driver->CacheList[i].dispose();
    }
    for (size_t i = 0, n = PRESENT_DRIVER_GDI_MAX_FRAMES; i < n; ++i)
    {   // free memory allocated for each in-flight frame.
        free(driver->FrameQueue[i].ImageIds);
    }
    free(driver->ImageRefs);
    free(driver->ImageData);
    free(driver->ImageIds );
    free(driver->CacheList);
    free(driver);
}
