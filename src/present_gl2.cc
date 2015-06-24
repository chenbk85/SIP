/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines the exported presentation DLL entry points for the OpenGL
/// 2.1 presentation library implementation.
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

#define  GLEW_STATIC
#include "GL/glew.h"
#include "GL/wglew.h"

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

#include "glew.c"

/*/////////////////
//   Constants   //
/////////////////*/

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Define all of the state associated with an OpenGL 2.1 display driver. This 
/// display driver is designed for compatibility across a wide range of hardware, from 
/// integrated GPUs up, and on both native OS and virtual machines.
struct present_driver_gl2_t
{
    size_t                Width;            /// The width of the window, in pixels.
    size_t                Height;           /// The height of the window, in pixels.
    HDC                   DC;               /// The window device context used for rendering operations.
    HGLRC                 WndRC;            /// The OpenGL rendering context for the window.
    HGLRC                 MainRC;           /// The primary OpenGL rendering context. All resources are created in this context.
    HWND                  Window;           /// The handle of the target window.
    pr_command_queue_t    CommandQueue;     /// The driver's command list submission queue.
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
    present_driver_gl2_t *driver = (present_driver_gl2_t*) malloc(sizeof(present_driver_gl2_t));
    if (driver == NULL)   return 0;

    // initialize the fields of the driver structure to be invalid.
    driver->DC            = NULL;
    driver->WndRC         = NULL; 
    driver->MainRC        = wglGetCurrentContext();
    driver->Window        = NULL;

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

    // ...
    if (driver->MainRC != NULL)
    {   // share textures, shaders, programs, etc. between the contexts.
        wglShareLists(driver->WndRC, driver->MainRC);
    }
    else
    {   // the WndRC is essentially the main rendering context.
        driver->MainRC = NULL; // TODO(rlk): set this to WndRC
    }

    GLenum glewError = glewInit(); // TODO(rlk): requires current context

    // initialize the fields of the driver structure.
    driver->Width         = size_t(width);
    driver->Height        = size_t(height);
    driver->DC            = mem_dc;
    driver->WndRC         = NULL; // TODO(rlk): set this
    driver->Window        = window;
    pr_command_queue_init(&driver->CommandQueue);
    return (uintptr_t)driver;
}

/// @summary Performs an explicit reinitialization of the display driver used to render 
/// content into a given window. This function is primarily used internally to handle 
/// detected device removal conditions, but may be called by the application as well.
/// @param drv The display driver handle returned by PrDisplayDriverOpen().
void __cdecl PrDisplayDriverReset(uintptr_t drv)
{
    present_driver_gl2_t   *driver = (present_driver_gl2_t*) drv;
    pr_command_queue_clear(&driver->CommandQueue);
}

/// @summary Handles the case where the window associated with a display driver is resized.
/// This function should be called in response to a WM_SIZE or WM_DISPLAYCHANGE event.
/// @param drv The display driver handle returned by PrDisplayDriverResize().
void __cdecl PrDisplayDriverResize(uintptr_t drv)
{
    present_driver_gl2_t *driver = (present_driver_gl2_t*) drv;
    if (driver == NULL)   return;

    // retrieve the current dimensions of the client area of the window.
    RECT rc;
    GetClientRect(driver->Window, &rc);
    int width  = (int)    (rc.right  - rc.left);
    int height = (int)    (rc.bottom - rc.top);
    
    // glViewport, etc.
    driver->Width  = size_t(width);
    driver->Height = size_t(height);
}

/// @summary Allocates a new command list for use by the application. The 
/// application should write display commands to the command list and then 
/// submit the list for processing by the presentation driver.
/// @param drv The display driver handle returned by PrDisplayDriverOpen().
/// @return An empty command list, or NULL if no command lists are available.
pr_command_list_t* __cdecl PrCreateCommandList(uintptr_t drv)
{
    present_driver_gl2_t *driver = (present_driver_gl2_t*) drv;
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
    present_driver_gl2_t *driver = (present_driver_gl2_t*) drv;
    if (driver == NULL)   return;
    HANDLE sync = cmdlist->SyncHandle;
    pr_command_queue_submit(&driver->CommandQueue, cmdlist);
    if (wait) safe_wait(sync, wait_timeout);
}

/// @summary Copies the current frame to the application window.
/// @param drv The display driver handle returned by PrDisplayDriverOpen().
void __cdecl PrPresentFrameToWindow(uintptr_t drv)
{
    present_driver_gl2_t *driver = (present_driver_gl2_t*) drv;
    if (driver == NULL)   return;

    pr_command_list_t *cmdlist = pr_command_queue_next_submitted(&driver->CommandQueue);
    while (cmdlist != NULL)
    {
        bool       end_of_frame = false;
        uint8_t const *read_ptr = cmdlist->CommandData;
        uint8_t const *end_ptr  = cmdlist->CommandData + cmdlist->BytesUsed;
        while (read_ptr < end_ptr)
        {
            pr_command_t *cmd_info = (pr_command_t*) read_ptr;
            size_t        cmd_size =  PR_COMMAND_SIZE_BASE + cmd_info->DataSize;
            switch (cmd_info->CommandId)
            {
            case PR_COMMAND_NO_OP:
                break;
            case PR_COMMAND_END_OF_FRAME:
                {   // break out of the command processing loop and submit
                    // submit the translated command list to the system driver.
                    end_of_frame = true;
                }
                break;
            case PR_COMMAND_CLEAR_COLOR_BUFFER:
                {
                    pr_clear_color_buffer_data_t *data = (pr_clear_color_buffer_data_t*) cmd_info->Data;
                    glClearColor(data->Red, data->Green, data->Blue, data->Alpha);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
                }
                break;
            }
            // advance to the start of the next buffered command.
            read_ptr += cmd_size;
        }
        // signal to any waiters that processing of the command list has finished.
        SetEvent(cmdlist->SyncHandle);
        // return the current command list to the driver's free list
        // and determine whether to continue submitting commands.
        pr_command_queue_return(&driver->CommandQueue, cmdlist);
        if (end_of_frame)
        {   // finished with command submission for this frame.
            break;
        }
        else
        {   // advance to the next queued command list.
            cmdlist = pr_command_queue_next_submitted(&driver->CommandQueue);
        }
    }

    // swap buffers to submit the command list to the OpenGL driver.
    SwapBuffers(driver->DC);
}

/// @summary Closes a display driver instance and releases all associated resources.
/// @param drv The display driver handle returned by PrDisplayDriverOpen().
void __cdecl PrDisplayDriverClose(uintptr_t drv)
{
    present_driver_gl2_t *driver = (present_driver_gl2_t*) drv;
    if (driver == NULL)   return;
    pr_command_queue_delete(&driver->CommandQueue);
    free(driver);
}

