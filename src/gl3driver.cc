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

/*/////////////////
//   Constants   //
/////////////////*/
#define GL2_HIDDEN_WNDCLASS_NAME             _T("GL2_Hidden_WndClass")

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Defines the data required to uniquely identify an image.
struct image_id_t
{
    uintptr_t              ImageId;          /// The application-defined image identifier.
    size_t                 FrameIndex;       /// The zero-based index of the frame of the image.
};

/// @summary Defines the data required to access an image.
struct image_data_t
{
    void                  *ImageData;        /// Pointer to the locked image data.
    size_t                 BytesLocked;      /// The number of bytes locked in memory.
    size_t                 FirstLevel;       /// The zero-based index of the first locked mipmap level.
    size_t                 LevelCount;       /// The number of locked mipmap levels.
    size_t                 Width;            /// The width of the first locked mipmap level, in pixels.
    size_t                 Height;           /// The height of the first locked mipmap level, in pixels.
    size_t                 Slices;           /// The number of slices in the first locked mipmap level.
    uint32_t               Format;           /// The underlying storage format (one of dxgi_format_e).
    int                    Compression;      /// The storage compression format (one of image_compression_e).
    int                    Encoding;         /// The storage encoding format (one of image_encoding_e).
};

/// @summary Defines the state data associated with a single in-flight frame. 
/// Each frame needs to perform several asynchronous operations, such as locking 
/// images, copying data to GPU memory, and executing compute pipelines. Once 
/// all asynchronous operations have completed, the frame can be submitted to the GPU.
struct display_frame_t
{
    pr_command_list_t     *CommandList;      /// Pointer to the command list for the frame.

    size_t                 ImageCount;       /// The number of images referenced by the frame.
    size_t                 ImageCapacity;    /// The current capacity of the image list for the frame.
    image_id_t            *ImageIds;         /// The set of image identifiers.
    image_data_t          *ImageData;        /// The set of image data, valid after the corresponding lock has completed.
};

/// @summary Define all of the state associated with an OpenGL 2.1 display driver. This 
/// display driver is designed for compatibility across a wide range of hardware, from 
/// integrated GPUs up, and on both native OS and virtual machines.
struct present_driver_gl2_t
{
    size_t                 Width;            /// The width of the window, in pixels.
    size_t                 Height;           /// The height of the window, in pixels.
    HDC                    WndDC;            /// The window device context used for rendering operations.
    HGLRC                  WndRC;            /// The OpenGL rendering context for the window.
    HWND                   Window;           /// The handle of the target window.

    size_t                 FrameCount;       /// The number of in-flight frames.
    size_t                 FrameCapacity;    /// The current capacity of the frame list.
    display_frame_t       *FrameList;        /// The list of in-flight frames.

    pr_command_queue_t     CommandQueue;     /// The driver's command list submission queue.
    GLEWContext            GLEW;             /// The set of GLEW function pointers for this rendering context.
    WGLEWContext           WGLEW;            /// The set of WGL GLEW function pointers for this rendering context.
};

/*///////////////
//   Globals   //
///////////////*/
/// @summary When using the Microsoft Linker, __ImageBase is set to the base address of the DLL.
/// This is the same as the HINSTANCE/HMODULE of the DLL passed to DllMain.
/// See: http://blogs.msdn.com/b/oldnewthing/archive/2004/10/25/247180.aspx
EXTERN_C IMAGE_DOS_HEADER  __ImageBase;

/// @summary The window class for hidden windows used for image streaming and context creation.
global_variable WNDCLASSEX HiddenWndClass;

/// @summary Global indicating whether the hidden window class needs to be registered.
global_variable bool       CreateWndClass = true;

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
/// @summary Implement the Windows message callback for the main application window.
/// @param hWnd The handle of the main application window.
/// @param uMsg The identifier of the message being processed.
/// @param wParam An unsigned pointer-size value specifying data associated with the message.
/// @param lParam A signed pointer-size value specifying data associated with the message.
/// @return The return value depends on the message being processed.
internal_function LRESULT CALLBACK HiddenWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

/// @summary Initialize Windows GLEW and resolve WGL extension entry points.
/// @param driver The presentation driver performing the initialization.
/// @param real_wnd The handle of the window that is the target of rendering operations.
/// @param active_dc The device context associated with the current OpenGL rendering context.
/// @param active_rc The OpenGL rendering context active on the current thread.
/// @return true if WGLEW is initialized successfully.
internal_function bool initialize_wglew(present_driver_gl2_t *driver, HWND real_wnd, HDC active_dc, HGLRC active_rc)
{   
    PIXELFORMATDESCRIPTOR pfd;
    RECT   window_rect;
    GLenum e = GLEW_OK;
    HWND wnd = NULL;
    HDC   dc = NULL;
    HGLRC rc = NULL;
    bool res = true;
    int  fmt = 0;

    // if necessary, register the window class used for hidden windows.
    if (CreateWndClass)
    {   // the window class is only registered once per-process.
        CreateWndClass                = false;
        HiddenWndClass.cbSize         = sizeof(WNDCLASSEX);
        HiddenWndClass.cbClsExtra     = 0;
        HiddenWndClass.cbWndExtra     = sizeof(void*);
        HiddenWndClass.hInstance      = (HINSTANCE)&__ImageBase;
        HiddenWndClass.lpszClassName  = GL2_HIDDEN_WNDCLASS_NAME;
        HiddenWndClass.lpszMenuName   = NULL;
        HiddenWndClass.lpfnWndProc    = HiddenWndProc;
        HiddenWndClass.hIcon          = LoadIcon  (0, IDI_APPLICATION);
        HiddenWndClass.hIconSm        = LoadIcon  (0, IDI_APPLICATION);
        HiddenWndClass.hCursor        = LoadCursor(0, IDC_ARROW);
        HiddenWndClass.style          = CS_HREDRAW | CS_VREDRAW ;
        HiddenWndClass.hbrBackground  = NULL;
        if (!RegisterClassEx(&HiddenWndClass))
        {   CreateWndClass = true;
            return false;
        }
    }

    // retrieve the window bounds to position and size the dummy window.
    GetWindowRect(real_wnd, &window_rect);

    // now that we've saved the current DC and rendering context, we need
    // to create a dummy window, pixel format and rendering context and 
    // make them current so we can retrieve the wgl extension entry points.
    // in order to create a rendering context, the pixel format must be set
    // on a window, and Windows only allows the pixel format to be set once.
    if ((wnd = CreateWindow(GL2_HIDDEN_WNDCLASS_NAME, _T("WGLEW_InitWnd"), WS_POPUP, window_rect.left, window_rect.top, window_rect.right - window_rect.left, window_rect.bottom - window_rect.top, NULL, NULL, (HINSTANCE)&__ImageBase, driver)) == NULL)
    {   // unable to create the hidden window - can't create an OpenGL rendering context.
        res  = false; goto cleanup;
    }
    if ((dc  = GetDC(wnd)) == NULL)
    {   // unable to retrieve the window device context - can't create an OpenGL rendering context.
        res  = false; goto cleanup;
    }
    // fill out a PIXELFORMATDESCRIPTOR using common pixel format attributes.
    memset(&pfd,   0, sizeof(PIXELFORMATDESCRIPTOR)); 
    pfd.nSize       = sizeof(PIXELFORMATDESCRIPTOR); 
    pfd.nVersion    = 1; 
    pfd.dwFlags     = PFD_DOUBLEBUFFER | PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW; 
    pfd.iPixelType  = PFD_TYPE_RGBA; 
    pfd.cColorBits  = 32; 
    pfd.cDepthBits  = 24; 
    pfd.cStencilBits=  8;
    pfd.iLayerType  = PFD_MAIN_PLANE; 
    if ((fmt = ChoosePixelFormat(dc, &pfd)) == 0)
    {   // unable to find a matching pixel format - can't create an OpenGL rendering context.
        res  = false; goto cleanup;
    }
    if (SetPixelFormat(dc, fmt, &pfd) != TRUE)
    {   // unable to set the dummy window pixel format - can't create an OpenGL rendering context.
        res  = false; goto cleanup;
    }
    if ((rc  = wglCreateContext(dc))  == NULL)
    {   // unable to create the dummy OpenGL rendering context.
        res  = false; goto cleanup;
    }
    if (wglMakeCurrent(dc, rc) != TRUE)
    {   // unable to make the OpenGL rednering context current.
        res  = false; goto cleanup;
    }
    // finally, we can initialize GLEW and get the function entry points.
    // this populates the function pointers in the driver->WGLEW field.
    if ((e   = wglewInit())  != GLEW_OK)
    {   // unable to initialize GLEW - WGLEW extensions are unavailable.
        res  = false; goto cleanup;
    }
    // initialization completed successfully. fallthrough to cleanup.
    res = true;

cleanup:
    wglMakeCurrent(NULL, NULL);
    wglMakeCurrent(active_dc , active_rc);
    if (rc  != NULL) wglDeleteContext(rc);
    if (dc  != NULL) ReleaseDC(wnd, dc);
    if (wnd != NULL) DestroyWindow(wnd);
    return res;
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
    present_driver_gl2_t *driver = (present_driver_gl2_t*) malloc(sizeof(present_driver_gl2_t));
    if (driver == NULL)   return 0;

    // initialize the fields of the driver structure to be invalid.
    driver->WndDC    = NULL;
    driver->WndRC    = NULL; 
    driver->Window   = NULL;

    // save the current device and GL rendering context.
    HDC   current_dc = wglGetCurrentDC();
    HGLRC current_rc = wglGetCurrentContext();

    // retrieve the current dimensions of the client area of the window.
    RECT  rc;
    GetClientRect(window, &rc);
    int   width  = (int)  (rc.right  - rc.left);
    int   height = (int)  (rc.bottom - rc.top);

    // initialize WGLEW - lets us proceed with 'modern' style initialization.
    if (initialize_wglew(driver, window, current_dc, current_rc) == false)
    {   // no worthwhile OpenGL implementation is available.
        return (uintptr_t) 0;
    }
    
    // save off the window device context:
    PIXELFORMATDESCRIPTOR pfd;
    HDC  win_dc = GetDC(window);
    HGLRC gl_rc = NULL;
    GLenum  err = GLEW_OK;
    int     fmt = 0;
    memset(&pfd , 0, sizeof(PIXELFORMATDESCRIPTOR));
    pfd.nSize   =    sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion= 1;

    // for creating a rendering context and pixel format, we can either use
    // the new method, or if that's not available, fall back to the old way.
    if (WGLEW_ARB_pixel_format)
    {   // use the more recent method to find a pixel format.
        UINT fmt_count     = 0;
        int  fmt_attribs[] = 
        {
            WGL_SUPPORT_OPENGL_ARB,                      GL_TRUE, 
            WGL_DRAW_TO_WINDOW_ARB,                      GL_TRUE, 
            WGL_DEPTH_BITS_ARB    ,                           24, 
            WGL_STENCIL_BITS_ARB  ,                            8,
            WGL_RED_BITS_ARB      ,                            8, 
            WGL_GREEN_BITS_ARB    ,                            8,
            WGL_BLUE_BITS_ARB     ,                            8,
            WGL_PIXEL_TYPE_ARB    ,            WGL_TYPE_RGBA_ARB,
            WGL_ACCELERATION_ARB  ,    WGL_FULL_ACCELERATION_ARB, 
            WGL_SAMPLE_BUFFERS_ARB,                     GL_FALSE, /* GL_TRUE to enable MSAA */
            WGL_SAMPLES_ARB       ,                            1, /* ex. 4 = 4x MSAA        */
            0
        };
        if (wglChoosePixelFormatARB(win_dc, fmt_attribs, NULL, 1, &fmt, &fmt_count) == FALSE)
        {   // unable to find a matching pixel format - can't create an OpenGL rendering context.
            ReleaseDC(window, win_dc);
            return (uintptr_t) 0;
        }
    }
    else
    {   // use the legacy method to find the pixel format.
        pfd.dwFlags     = PFD_DOUBLEBUFFER | PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW; 
        pfd.iPixelType  = PFD_TYPE_RGBA; 
        pfd.cColorBits  = 32; 
        pfd.cDepthBits  = 24; 
        pfd.cStencilBits=  8;
        pfd.iLayerType  = PFD_MAIN_PLANE; 
        if ((fmt = ChoosePixelFormat(win_dc, &pfd)) == 0)
        {   // unable to find a matching pixel format - can't create an OpenGL rendering context.
            ReleaseDC(window, win_dc);
            return (uintptr_t) 0;
        }
    }
    if (SetPixelFormat(win_dc, fmt, &pfd)  != TRUE)
    {   // unable to assign the pixel format to the window.
        ReleaseDC(window, win_dc);
        return (uintptr_t)0;
    }
    if (WGLEW_ARB_create_context)
    {
        if (WGLEW_ARB_create_context_profile)
        {
            int  rc_attribs[] = 
            {
                WGL_CONTEXT_MAJOR_VERSION_ARB,    2, 
                WGL_CONTEXT_MINOR_VERSION_ARB,    1, 
                WGL_CONTEXT_PROFILE_MASK_ARB ,    WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
#ifdef _DEBUG
                WGL_CONTEXT_FLAGS_ARB        ,    WGL_CONTEXT_DEBUG_BIT_ARB,
#endif
                0
            };
            if ((gl_rc = wglCreateContextAttribsARB(win_dc, current_rc, rc_attribs)) == NULL)
            {   // unable to create the OpenGL rendering context.
                ReleaseDC(window, win_dc);
                return (uintptr_t)0;
            }
        }
        else
        {
            int  rc_attribs[] = 
            {
                WGL_CONTEXT_MAJOR_VERSION_ARB,    2,
                WGL_CONTEXT_MINOR_VERSION_ARB,    1,
#ifdef _DEBUG
                WGL_CONTEXT_FLAGS_ARB        ,    WGL_CONTEXT_DEBUG_BIT_ARB,
#endif
                0
            };
            if ((gl_rc = wglCreateContextAttribsARB(win_dc, current_rc, rc_attribs)) == NULL)
            {   // unable to create the OpenGL rendering context.
                ReleaseDC(window, win_dc);
                return (uintptr_t)0;
            }
        }
    }
    else
    {   // use the legacy method to create the OpenGL rendering context.
        if ((gl_rc = wglCreateContext(win_dc)) == NULL)
        {   // unable to create the OpenGL rendering context.
            ReleaseDC(window, win_dc);
            return (uintptr_t)0;
        }
        if (current_rc != NULL)
        {   // share textures, shaders, programs, etc. between the rendering contexts.
            wglShareLists(gl_rc, current_rc);
        }
    }
    if (wglMakeCurrent(win_dc, gl_rc) != TRUE)
    {   // unable to activate the OpenGL rendering context on the current thread.
        wglMakeCurrent(current_dc, current_rc);
        ReleaseDC(window, win_dc);
        return (uintptr_t)0;
    }
    if ((err = glewInit()) != GLEW_OK)
    {   // unable to initialize GLEW - OpenGL entry points are not available.
        wglMakeCurrent(current_dc, current_rc);
        ReleaseDC(window, win_dc);
        return (uintptr_t)0;
    }
    if (GLEW_VERSION_2_1 == GL_FALSE)
    {   // OpenGL 2.1 is not supported by the driver.
        wglMakeCurrent(current_dc, current_rc);
        ReleaseDC(window, win_dc);
        return (uintptr_t) 0;
    }

    // optionally, enable synchronization with vertical retrace.
    if (WGLEW_EXT_swap_control)
    {
        if (WGLEW_EXT_swap_control_tear)
        {   // SwapBuffers synchronizes with the vertical retrace interval, 
            // except when the interval is missed, in which case the swap 
            // is performed immediately.
            wglSwapIntervalEXT(-1);
        }
        else
        {   // SwapBuffers synchronizes with the vertical retrace interval.
            wglSwapIntervalEXT(+1);
        }
    }

    // finally, select the master rendering context. 
    // the window rendering context is only active when submitting a command list.
    wglMakeCurrent(current_dc, current_rc);

    // initialize the fields of the driver structure.
    driver->Width   = size_t(width);
    driver->Height  = size_t(height);
    driver->WndDC   = win_dc;
    driver->WndRC   = gl_rc;
    driver->Window  = window;
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

    wglMakeCurrent(driver->WndDC, driver->WndRC);

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
    SwapBuffers(driver->WndDC);
    wglMakeCurrent(NULL, NULL);
}

/// @summary Closes a display driver instance and releases all associated resources.
/// @param drv The display driver handle returned by PrDisplayDriverOpen().
void __cdecl PrDisplayDriverClose(uintptr_t drv)
{
    present_driver_gl2_t *driver = (present_driver_gl2_t*) drv;
    if (driver == NULL)   return;
    if (driver->WndRC != NULL)
    {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(driver->WndRC);
    }
    if (driver->WndDC != NULL)
    {
        ReleaseDC(driver->Window, driver->WndDC);
    }
    driver->WndDC  = NULL;
    driver->WndRC  = NULL;
    driver->Window = NULL;
    pr_command_queue_delete(&driver->CommandQueue);
    free(driver);
}

