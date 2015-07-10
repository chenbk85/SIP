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
#define GL_HIDDEN_WNDCLASS_NAME               _T("GL_Hidden_WndClass")

/// @summary Define a special display ordinal value representing none/not found.
static uint32_t const DISPLAY_ORDINAL_NONE    = 0xFFFFFFFFU;

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Defines the data associated with a single OpenGL display device.
struct gl_display_t
{
    DWORD          Ordinal;               /// The ordinal value of the display.
    DISPLAY_DEVICE DisplayInfo;           /// Display information returned from EnumDisplayDevices.
    DEVMODE        DisplayMode;           /// The active display mode returned from EnumDisplaySettings.
    HWND           DisplayHWND;           /// The hidden, full-screen HWND used to create the rendering context.
    HDC            DisplayDC;             /// The GDI device context of the full-screen window.
    HGLRC          DisplayRC;             /// The OpenGL rendering context, which can target any window on the display.
    int            DisplayX;              /// The x-coordinate of the upper-left corner of the display. May be negative.
    int            DisplayY;              /// The y-coordinate of the upper-left corner of the display. May be negative.
    size_t         DisplayWidth;          /// The display width, in pixels.
    size_t         DisplayHeight;         /// The display height, in pixels.
    GLEWContext    GLEW;                  /// OpenGL function pointers and GLEW context.
    WGLEWContext   WGLEW;                 /// WGL function pointers and GLEW context.
};

/// @summary Represents the list of available attached displays, identified by 
/// ordinal number. 
struct gl_display_list_t
{
    size_t         DisplayCount;          /// The number of displays attached to the system.
    DWORD         *DisplayOrdinal;        /// The set of ordinal values for each display.
    gl_display_t  *Displays;              /// The set of display information and rendering contexts.
};

/*///////////////
//   Globals   //
///////////////*/
/// @summary When using the Microsoft Linker, __ImageBase is set to the base address of the DLL.
/// This is the same as the HINSTANCE/HMODULE of the DLL passed to DllMain.
/// See: http://blogs.msdn.com/b/oldnewthing/archive/2004/10/25/247180.aspx
EXTERN_C IMAGE_DOS_HEADER  __ImageBase;

/*///////////////////////
//   Local Functions   //
///////////////////////*/
/// @summary Initialize Windows GLEW and resolve WGL extension entry points.
/// @param driver The presentation driver performing the initialization.
/// @param window The handle of the window that is the target of rendering operations.
/// @param x The x-coordinate of the upper-left corner of the target display.
/// @param y The y-coordinate of the upper-left corner of the target display.
/// @param w The width of the target display, in pixels.
/// @param h The height of the target display, in pixels.
/// @return true if WGLEW is initialized successfully.
internal_function bool initialize_wglew(gl_display_t *display, HWND real_wnd, int x, int y, int w, int h)
{   
    PIXELFORMATDESCRIPTOR pfd;
    GLenum e = GLEW_OK;
    HWND wnd = NULL;
    HDC   dc = NULL;
    HGLRC rc = NULL;
    bool res = true;
    int  fmt = 0;

    // create a dummy window, pixel format and rendering context and 
    // make them current so we can retrieve the wgl extension entry 
    // points. in order to create a rendering context, the pixel format
    // must be set on a window, and Windows only allows the pixel format
    // to be set once for a given window - which is dumb.
    if ((wnd = CreateWindow(GL_HIDDEN_WNDCLASS_NAME, _T("WGLEW_InitWnd"), WS_POPUP, x, y, w, h, NULL, NULL, (HINSTANCE)&__ImageBase, NULL)) == NULL)
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
    // this populates the function pointers in the display->WGLEW field.
    if ((e   = wglewInit())  != GLEW_OK)
    {   // unable to initialize GLEW - WGLEW extensions are unavailable.
        res  = false; goto cleanup;
    }
    // initialization completed successfully. fallthrough to cleanup.
    res = true;

cleanup:
    wglMakeCurrent(NULL, NULL);
    if (rc  != NULL) wglDeleteContext(rc);
    if (dc  != NULL) ReleaseDC(wnd, dc);
    if (wnd != NULL) DestroyWindow(wnd);
    return res;
}

/*///////////////////////
//  Public Functions   //
///////////////////////*/
/// @summary Builds a list of all OpenGL 3.2-capable displays attached to the system and creates any associated rendering contexts.
/// @param display_list The display list to populate. Its current contents is overwritten.
/// @return true if at least one OpenGL 3.2-capable display is attached.
public_function bool enumerate_displays(gl_display_list_t *display_list)
{   
    DISPLAY_DEVICE dd;
    HINSTANCE      instance      =(HINSTANCE)&__ImageBase;
    WNDCLASSEX     wndclass      = {};
    gl_display_t  *displays      = NULL;
    DWORD         *ordinals      = NULL;
    size_t         display_count = 0;

    // initialize the fields of the output display list.
    display_list->DisplayCount   = 0;
    display_list->DisplayOrdinal = NULL;
    display_list->Displays       = NULL;

    // enumerate all displays in the system to determine the number of attached displays.
    dd.cb = sizeof(DISPLAY_DEVICE);
    for (DWORD ordinal = 0; EnumDisplayDevices(NULL, ordinal, &dd, 0); ++ordinal)
    {   // ignore pseudo-displays.
        if ((dd.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER) != 0)
            continue;
        if ((dd.StateFlags & DISPLAY_DEVICE_ACTIVE) == 0)
            continue;
        if ((dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) == 0)
            continue;
        display_count++;
    }
    if (display_count == 0)
    {
        OutputDebugString(_T("ERROR: No attached displays in current session.\n"));
        return false;
    }

    // register the window class used for hidden windows, if necessary.
    if (!GetClassInfoEx(instance, GL_HIDDEN_WNDCLASS_NAME, &wndclass))
    {   // the window class has not been registered yet.
        wndclass.cbSize         = sizeof(WNDCLASSEX);
        wndclass.cbClsExtra     = 0;
        wndclass.cbWndExtra     = sizeof(void*);
        wndclass.hInstance      = (HINSTANCE)&__ImageBase;
        wndclass.lpszClassName  = GL_HIDDEN_WNDCLASS_NAME;
        wndclass.lpszMenuName   = NULL;
        wndclass.lpfnWndProc    = DefWindowProc;
        wndclass.hIcon          = LoadIcon  (0, IDI_APPLICATION);
        wndclass.hIconSm        = LoadIcon  (0, IDI_APPLICATION);
        wndclass.hCursor        = LoadCursor(0, IDC_ARROW);
        wndclass.style          = CS_OWNDC;
        wndclass.hbrBackground  = NULL;
        if (!RegisterClassEx(&wndclass))
        {
            OutputDebugString(_T("ERROR: Unable to register hidden window class.\n"));
            return false;
        }
    }

    // allocate memory for the display list.
    ordinals      = (DWORD       *) malloc(display_count * sizeof(DWORD));
    displays      = (gl_display_t*) malloc(display_count * sizeof(gl_display_t));
    display_count = 0;
    if (ordinals == NULL || displays == NULL)
    {
        OutputDebugString(_T("ERROR: Unable to allocate display list memory.\n"));
        return false;
    }
    
    // enumerate all displays and retrieve their properties.
    // reset the display count to only track displays for which an OpenGL 3.2 
    // context can be successfully created - hide displays that fail.
    for (DWORD ordinal = 0; EnumDisplayDevices(NULL, ordinal, &dd, 0); ++ordinal)
    {   
        gl_display_t *display = &displays[display_count];

        // ignore pseudo-displays.
        if ((dd.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER) != 0)
            continue;
        if ((dd.StateFlags & DISPLAY_DEVICE_ACTIVE) == 0)
            continue;
        if ((dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) == 0)
            continue;

        // retrieve the current display settings and extract the 
        // position and size of the display area for window creation.
        DEVMODE dm;
        memset(&dm, 0, sizeof(DEVMODE));
        dm.dmSize    = sizeof(DEVMODE);
        if (!EnumDisplaySettingsEx(dd.DeviceName, ENUM_CURRENT_SETTINGS, &dm, 0))
        {   // try for the registry settings instead.
            if (!EnumDisplaySettingsEx(dd.DeviceName, ENUM_REGISTRY_SETTINGS, &dm, 0))
            {   // unable to retrieve the display mode settings. skip this display.
                continue;
            }
        }

        int x, y, w, h;
        if (dm.dmFields & DM_POSITION)
        {   // save the position of the display, in virtual space.
            x = dm.dmPosition.x;
            y = dm.dmPosition.y;
        }
        else continue;
        if (dm.dmFields & DM_PELSWIDTH ) w = dm.dmPelsWidth;
        else continue;
        if (dm.dmFields & DM_PELSHEIGHT) h = dm.dmPelsHeight;
        else continue;

        // create a hidden window on the display.
        HWND window  = CreateWindowEx(0, GL_HIDDEN_WNDCLASS_NAME, dd.DeviceName, WS_POPUP, x, y, w, h, NULL, NULL, instance, NULL);
        if  (window == NULL)
        {
            OutputDebugString(_T("ERROR: Unable to create hidden window on display: "));
            OutputDebugString(dd.DeviceName);
            OutputDebugString(_T(".\n"));
            continue;
        }

        // initialize WGLEW, allowing us to possibly use modern-style context creation.
        if(initialize_wglew(display, window, x, y, w, h) == false)
        {
            OutputDebugString(_T("ERROR: Unable to initialize WGLEW on display: "));
            OutputDebugString(dd.DeviceName);
            OutputDebugString(_T(".\n"));
            DestroyWindow(window);
            continue;
        }

        // save off the window device context and set up a PIXELFORMATDESCRIPTOR.
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
                //WGL_SAMPLE_BUFFERS_ARB,                     GL_FALSE, /* GL_TRUE to enable MSAA */
                //WGL_SAMPLES_ARB       ,                            1, /* ex. 4 = 4x MSAA        */
                0
            };
            if (wglChoosePixelFormatARB(win_dc, fmt_attribs, NULL, 1, &fmt, &fmt_count) != TRUE)
            {   // unable to find a matching pixel format - can't create an OpenGL rendering context.
                OutputDebugString(_T("ERROR: wglChoosePixelFormatARB failed on display: "));
                OutputDebugString(dd.DeviceName);
                OutputDebugString(_T(".\n"));
                ReleaseDC(window, win_dc);
                DestroyWindow(window);
                continue;
            }
        }
        else
        {   // use the legacy method to find the pixel format.
            pfd.dwFlags      = PFD_DOUBLEBUFFER | PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW; 
            pfd.iPixelType   = PFD_TYPE_RGBA; 
            pfd.cColorBits   = 32; 
            pfd.cDepthBits   = 24; 
            pfd.cStencilBits =  8;
            pfd.iLayerType   = PFD_MAIN_PLANE; 
            if ((fmt = ChoosePixelFormat(win_dc, &pfd)) == 0)
            {   // unable to find a matching pixel format - can't create an OpenGL rendering context.
                OutputDebugString(_T("ERROR: ChoosePixelFormat failed on display: "));
                OutputDebugString(dd.DeviceName);
                OutputDebugString(_T(".\n"));
                ReleaseDC(window, win_dc);
                DestroyWindow(window);
                continue;
            }
        }
        if (SetPixelFormat(win_dc, fmt, &pfd) != TRUE)
        {   // unable to assign the pixel format to the window.
            OutputDebugString(_T("ERROR: SetPixelFormat failed on display: "));
            OutputDebugString(dd.DeviceName);
            OutputDebugString(_T(".\n"));
            ReleaseDC(window, win_dc);
            DestroyWindow(window);
            continue;
        }
        if (WGLEW_ARB_create_context)
        {
            if (WGLEW_ARB_create_context_profile)
            {
                int  rc_attribs[] = 
                {
                    WGL_CONTEXT_MAJOR_VERSION_ARB,    3, 
                    WGL_CONTEXT_MINOR_VERSION_ARB,    2, 
                    WGL_CONTEXT_PROFILE_MASK_ARB ,    WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
#ifdef _DEBUG
                    WGL_CONTEXT_FLAGS_ARB        ,    WGL_CONTEXT_DEBUG_BIT_ARB,
#endif
                    0
                };
                if ((gl_rc = wglCreateContextAttribsARB(win_dc, NULL, rc_attribs)) == NULL)
                {   // unable to create the OpenGL rendering context.
                    OutputDebugString(_T("ERROR: wglCreateContextAttribsARB failed on display: "));
                    OutputDebugString(dd.DeviceName);
                    OutputDebugString(_T(".\n"));
                    ReleaseDC(window, win_dc);
                    DestroyWindow(window);
                    continue;
                }
            }
            else
            {
                int  rc_attribs[] = 
                {
                    WGL_CONTEXT_MAJOR_VERSION_ARB,    3,
                    WGL_CONTEXT_MINOR_VERSION_ARB,    2,
#ifdef _DEBUG
                    WGL_CONTEXT_FLAGS_ARB        ,    WGL_CONTEXT_DEBUG_BIT_ARB,
#endif
                    0
                };
                if ((gl_rc = wglCreateContextAttribsARB(win_dc, NULL, rc_attribs)) == NULL)
                {   // unable to create the OpenGL rendering context.
                    OutputDebugString(_T("ERROR: wglCreateContextAttribsARB failed on display: "));
                    OutputDebugString(dd.DeviceName);
                    OutputDebugString(_T(".\n"));
                    ReleaseDC(window, win_dc);
                    DestroyWindow(window);
                    continue;
                }
            }
        }
        else
        {   // use the legacy method to create the OpenGL rendering context.
            if ((gl_rc = wglCreateContext(win_dc)) == NULL)
            {   // unable to create the OpenGL rendering context.
                OutputDebugString(_T("ERROR: wglCreateContext failed on display: "));
                OutputDebugString(dd.DeviceName);
                OutputDebugString(_T(".\n"));
                ReleaseDC(window, win_dc);
                DestroyWindow(window);
                continue;
            }
        }
        if (wglMakeCurrent(win_dc, gl_rc) != TRUE)
        {   // unable to activate the OpenGL rendering context on the current thread.
            OutputDebugString(_T("ERROR: wglMakeCurrent failed on display: "));
            OutputDebugString(dd.DeviceName);
            OutputDebugString(_T(".\n"));
            ReleaseDC(window, win_dc);
            DestroyWindow(window);
            continue;
        }
        glewExperimental = GL_TRUE; // else we crash - see: http://www.opengl.org/wiki/OpenGL_Loading_Library
        if ((err = glewInit()) != GLEW_OK)
        {   // unable to initialize GLEW - OpenGL entry points are not available.
            OutputDebugString(_T("ERROR: glewInit() failed on display: "));
            OutputDebugString(dd.DeviceName);
            OutputDebugStringA(", error: ");
            OutputDebugStringA((char const*) glewGetErrorString(err));
            OutputDebugString(_T(".\n"));
            wglMakeCurrent(NULL, NULL);
            ReleaseDC(window, win_dc);
            DestroyWindow(window);
            continue;
        }
        if (GLEW_VERSION_3_2 == GL_FALSE)
        {   // OpenGL 3.2 is not supported by the driver.
            OutputDebugString(_T("ERROR: Driver does not support OpenGL 3.2 on display: "));
            OutputDebugString(dd.DeviceName);
            OutputDebugString(_T(".\n"));
            wglMakeCurrent(NULL, NULL);
            ReleaseDC(window, win_dc);
            DestroyWindow(window);
            continue;
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

        // everything was successful; deselect the rendering context.
        wglMakeCurrent(NULL, NULL);

        // populate the entry in the ordinal and display lists.
        ordinals[display_count] = ordinal;
        displays[display_count].Ordinal       = ordinal;
        displays[display_count].DisplayInfo   = dd;
        displays[display_count].DisplayMode   = dm;
        displays[display_count].DisplayHWND   = window;
        displays[display_count].DisplayDC     = win_dc;
        displays[display_count].DisplayRC     = gl_rc;
        displays[display_count].DisplayX      = x;
        displays[display_count].DisplayY      = y;
        displays[display_count].DisplayWidth  = size_t(w);
        displays[display_count].DisplayHeight = size_t(h);
        display_count++;
    }
    if (display_count == 0)
    {   // there are no displays supporting OpenGL 3.2 or above.
        OutputDebugString(_T("ERROR: No displays found with support for OpenGL 3.2+.\n"));
        free(displays);
        free(ordinals);
        return false;
    }

    // at least one supported display was found; we're done.
    display_list->DisplayCount   = display_count;
    display_list->DisplayOrdinal = ordinals;
    display_list->Displays       = displays;
    return true;
}

/// @summary Retrieve the display state given the display ordinal number.
/// @param display_list The display list to search.
/// @param ordinal The ordinal number uniquely identifying the display.
/// @return The display state, or NULL.
public_function gl_display_t* display_for_ordinal(gl_display_list_t *display_list, DWORD ordinal)
{
    for (size_t i = 0, n = display_list->DisplayCount; i < n; ++i)
    {
        if (display_list->DisplayOrdinal[i] == ordinal)
        {
            return &display_list->Displays[i];
        }
    }
    return NULL;
}

/// @summary Determines which display contains the majority of the client area of a window. This function should be called whenever a window moves.
/// @param display_list The list of active displays.
/// @param window The window handle to search for.
/// @param ordinal On return, this value is set to the display ordinal, or DISPLAY_ORDINAL_NONE.
/// @return A pointer to the display that owns the window.
public_function gl_display_t* display_for_window(gl_display_list_t *display_list, HWND window, DWORD &ordinal)
{
    // does window rect intersect display rect?
    // if yes, compute rectangle of intersection and then area of intersection
    // if area of intersection is greater than existing area, this is the prime candidate
    gl_display_t *display  = NULL;
    size_t        max_area = 0;
    RECT          wnd_rect;

    // initially, we have no match:
    ordinal = DISPLAY_ORDINAL_NONE;

    // retrieve the current client bounds of the window.
    GetClientRect(window, &wnd_rect);

    // check each display to determine which contains the majority of the window client area.
    for (size_t i = 0, n = display_list->DisplayCount; i < n; ++i)
    {   // convert the display rectangle from {x,y,w,h} to {l,t,r,b}.
        RECT int_rect;
        RECT mon_rect = 
        {
            (LONG) display_list->Displays[i].DisplayX, 
            (LONG) display_list->Displays[i].DisplayY, 
            (LONG)(display_list->Displays[i].DisplayX + display_list->Displays[i].DisplayWidth), 
            (LONG)(display_list->Displays[i].DisplayY + display_list->Displays[i].DisplayHeight)
        };
        if (IntersectRect(&int_rect, &wnd_rect, &mon_rect))
        {   // the window client area intersects this display. what's the area of intersection?
            size_t int_w  = size_t(int_rect.right  - int_rect.left);
            size_t int_h  = size_t(int_rect.bottom - int_rect.top );
            size_t int_a  = int_w * int_h;
            if (int_a > max_area)
            {   // more of this window is on display 'i' than the previous best match.
                ordinal   = display_list->Displays[i].Ordinal;
                display   =&display_list->Displays[i];
                max_area  = int_a;
            }
        }
    }
    return display;
}

/// @summary Create a new window on the specified display and prepare it for rendering operations.
/// @param display The target display.
/// @param class_name The name of the window class. See CreateWindowEx, lpClassName argument.
/// @param title The text to set as the window title. See CreateWindowEx, lpWindowName argument.
/// @param style The window style. See CreateWindowEx, dwStyle argument.
/// @param style_ex The extended window style. See CreateWindowEx, dwExStyle argument.
/// @param x The x-coordinate of the upper left corner of the window, in display coordinates, or 0.
/// @param y The y-coordinate of the upper left corner of the window, in display coordinates, or 0.
/// @param w The window width, in pixels, or 0.
/// @param h The window height, in pixels, or 0.
/// @param parent The parent HWND, or NULL. See CreateWindowEx, hWndParent argument.
/// @param menu The menu to be used with the window, or NULL. See CreateWindowEx, hMenu argument.
/// @param instance The module instance associated with the window, or NULL. See CreateWindowEx, hInstance argument.
/// @param user_data Passed to the window through the CREATESTRUCT, or NULL. See CreateWindowEx, lpParam argument.
/// @param out_windowrect On return, stores the window bounds. May be NULL.
/// @return The window handle, or NULL.
public_function HWND make_window_on_display(gl_display_t *display, TCHAR const *class_name, TCHAR const *title, DWORD style, DWORD style_ex, int x, int y, int w, int h, HWND parent, HMENU menu, HINSTANCE instance, LPVOID user_data, RECT *out_windowrect)
{   // default to a full screen window if no size is specified.
    RECT display_rect = 
    {
        (LONG) display->DisplayX, 
        (LONG) display->DisplayY, 
        (LONG)(display->DisplayX + display->DisplayWidth), 
        (LONG)(display->DisplayY + display->DisplayHeight)
    };
    if (x == 0) x = (int) display->DisplayX;
    if (y == 0) y = (int) display->DisplayY;
    if (w == 0) w = (int)(display->DisplayWidth  - (display_rect.right  - x));
    if (h == 0) h = (int)(display->DisplayHeight - (display_rect.bottom - y));

    // clip the window bounds to the display bounds.
    if (x + w <= display_rect.left || x >= display_rect.right)   x = display_rect.left;
    if (x + w >  display_rect.right ) w  = display_rect.right  - x;
    if (y + h <= display_rect.top  || y >= display_rect.bottom)  y = display_rect.bottom;
    if (y + h >  display_rect.bottom) h  = display_rect.bottom - y;

    // retrieve the pixel format description for the target display.
    PIXELFORMATDESCRIPTOR pfd_dst;
    HDC                   hdc_dst  = display->DisplayDC;
    int                   fmt_dst  = GetPixelFormat(display->DisplayDC);
    DescribePixelFormat(hdc_dst, fmt_dst, sizeof(PIXELFORMATDESCRIPTOR), &pfd_dst);

    // create the new window on the display.
    HWND src_wnd  = CreateWindowEx(style_ex, class_name, title, style, x, y, w, h, parent, menu, instance, user_data);
    if  (src_wnd == NULL)
    {   // failed to create the window, no point in continuing.
        return NULL;
    }

    // assign the target display pixel format to the window.
    HDC  hdc_src  = GetDC(src_wnd);
    if (!SetPixelFormat(hdc_src, fmt_dst, &pfd_dst))
    {   // cannot set the pixel format; window is unusable.
        ReleaseDC(src_wnd, hdc_src);
        DestroyWindow(src_wnd);
        return NULL;
    }

    // everything is good; cleanup and set output parameters.
    ReleaseDC(src_wnd, hdc_src);
    if (out_windowrect != NULL) GetWindowRect(src_wnd, out_windowrect);
    return src_wnd;
}

/// @summary Moves a window to a new display, ensuring that the correct pixel format is set. If necessary, the window is destroyed and recreated.
/// @param display The target display.
/// @param src_window The window being moved to the target display.
/// @param out_window On return, this value is set to the handle representing the window.
/// @return true if the window could be moved to the specified display.
public_function bool move_window_to_display(gl_display_t *display, HWND src_window, HWND &out_window)
{
    PIXELFORMATDESCRIPTOR pfd_dst;
    HDC                   hdc_src  = GetDC(src_window);
    HDC                   hdc_dst  = display->DisplayDC;
    int                   fmt_dst  = GetPixelFormat(display->DisplayDC);

    DescribePixelFormat(hdc_dst, fmt_dst, sizeof(PIXELFORMATDESCRIPTOR), &pfd_dst);
    if (!SetPixelFormat(hdc_src, fmt_dst, &pfd_dst))
    {   // the window may have already had its pixel format set.
        // we need to destroy and then re-create the window.
        // when re-creating the window, use WS_CLIPCHILDREN | WS_CLIPSIBLINGS.
        DWORD      style_r  =  WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
        HINSTANCE  instance = (HINSTANCE) GetWindowLongPtr(src_window, GWLP_HINSTANCE);
        DWORD      style    = (DWORD    ) GetWindowLongPtr(src_window, GWL_STYLE) | style_r;
        DWORD      style_ex = (DWORD    ) GetWindowLongPtr(src_window, GWL_EXSTYLE);
        HWND       parent   = (HWND     ) GetWindowLongPtr(src_window, GWLP_HWNDPARENT);
        LONG_PTR   userdata = (LONG_PTR ) GetWindowLongPtr(src_window, GWLP_USERDATA);
        WNDPROC    wndproc  = (WNDPROC  ) GetWindowLongPtr(src_window, GWLP_WNDPROC);
        HMENU      wndmenu  = (HMENU    ) GetMenu(src_window);
        size_t     titlelen = (size_t   ) GetWindowTextLength(src_window);
        TCHAR     *titlebuf = (TCHAR   *) malloc((titlelen+1) * sizeof(TCHAR));
        BOOL       visible  = IsWindowVisible(src_window);
        RECT       wndrect; GetWindowRect(src_window, &wndrect);

        if (titlebuf == NULL)
        {   // unable to allocate the required memory.
            ReleaseDC(src_window, hdc_src);
            out_window = src_window;
            return false;
        }
        GetWindowText(src_window, titlebuf, (int)(titlelen+1));

        TCHAR class_name[256+1]; // see MSDN for WNDCLASSEX lpszClassName field.
        if (!GetClassName(src_window, class_name, 256+1))
        {   // unable to retrieve the window class name.
            ReleaseDC(src_window, hdc_src);
            out_window = src_window;
            free(titlebuf);
            return false;
        }

        int  x = (int) wndrect.left;
        int  y = (int) wndrect.top;
        int  w = (int)(wndrect.right  - wndrect.left);
        int  h = (int)(wndrect.bottom - wndrect.top );
        HWND new_window  = CreateWindowEx(style_ex, class_name, titlebuf, style, x, y, w, h, parent, wndmenu, instance, (LPVOID) userdata);
        if  (new_window == NULL)
        {   // unable to re-create the window.
            ReleaseDC(src_window, hdc_src);
            out_window = src_window;
            free(titlebuf);
            return false;
        }
        SetWindowLongPtr(new_window, GWLP_USERDATA, (LONG_PTR) userdata);
        SetWindowLongPtr(new_window, GWLP_WNDPROC , (LONG_PTR) wndproc);
        ReleaseDC(src_window, hdc_src);
        hdc_src = GetDC(new_window);
        free(titlebuf);

        // finally, we can try and set the pixel format again.
        if (!SetPixelFormat(hdc_src, fmt_dst, &pfd_dst))
        {   // still unable to set the pixel format - we're out of options.
            ReleaseDC(new_window, hdc_src);
            DestroyWindow(new_window);
            out_window = src_window;
            return false;
        }

        // show the new window, and activate it:
        if (visible) ShowWindow(new_window, SW_SHOW);
        else ShowWindow(new_window, SW_HIDE);

        // everything was successful, so destroy the old window:
        ReleaseDC(new_window, hdc_src);
        DestroyWindow(src_window);
        out_window = new_window;
        return true;
    }
    else
    {   // the pixel format was set successfully without recreating the window.
        ReleaseDC(src_window, hdc_src);
        out_window = src_window;
        return true;
    }
}

/// @summary Sets the target drawable for a display. Subsequent rendering commands will update the drawable.
/// @param display The target display. If this value is NULL, the current rendering context is unbound.
/// @param target_dc The device context to bind as the drawable. If this value is NULL, the display DC is used.
public_function void target_drawable(gl_display_t *display, HDC target_dc)
{
    if (display != NULL)
    {   // target the drawable with the display's rendering context.
        if (target_dc != NULL) wglMakeCurrent(target_dc, display->DisplayRC);
        else wglMakeCurrent(display->DisplayDC, display->DisplayRC);
    }
    else
    {   // deactivate the current drawable and rendering context.
        wglMakeCurrent(NULL, NULL);
    }
}

/// @summary Sets the target drawable for a display. Subsequent rendering commands will update the drawable.
/// @param display_list The display list to search.
/// @param ordinal The ordinal number of the display to target.
/// @param target_dc The device context to bind as the drawable. If this value is NULL, the display DC is used.
public_function void target_drawable(gl_display_list_t *display_list, DWORD ordinal, HDC target_dc)
{
    for (size_t i = 0, n = display_list->DisplayCount; i < n; ++i)
    {
        if (display_list->DisplayOrdinal[i] == ordinal)
        {   // located the matching display object.
            target_drawable(&display_list->Displays[i], target_dc);
            return;
        }
    }
}

/// @summary Flushes all pending rendering commands to the GPU for the specified display.
/// @param display The display to flush.
public_function void flush_display(gl_display_t *display)
{
    if (display != NULL) glFlush();
}

/// @summary Flushes all pending rendering commands to the GPU, and blocks the calling thread until they have been processed.
/// @param display The display to synchronize.
public_function void synchronize_display(gl_display_t *display)
{
    if (display != NULL) glFinish();
}

/// @summary Flushes all rendering commands targeting the specified drawable, and updates the pixel data.
/// @param dc The drawable to present.
public_function void swap_buffers(HDC dc)
{
    SwapBuffers(dc);
}

/// @summary Deletes any rendering contexts and display resources, and then deletes the display list.
/// @param display_list The display list to delete.
public_function void delete_display_list(gl_display_list_t *display_list)
{   // deselect any active OpenGL context:
    wglMakeCurrent(NULL, NULL);
    // free resources associated with any displays:
    for (size_t i = 0, n =  display_list->DisplayCount; i < n; ++i)
    {
        gl_display_t *display = &display_list->Displays[i];
        wglDeleteContext(display->DisplayRC);
        ReleaseDC(display->DisplayHWND, display->DisplayDC);
        DestroyWindow(display->DisplayHWND);
        memset(display, 0, sizeof(gl_display_t));
    }
    free(display_list->Displays);
    free(display_list->DisplayOrdinal);
    display_list->DisplayCount   = 0;
    display_list->DisplayOrdinal = NULL;
    display_list->Displays       = NULL;
}
