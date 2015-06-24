/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines some global state and helper routines for dynamically 
/// loading presentation layers and managing presentation driver state.
///////////////////////////////////////////////////////////////////////////80*/

/*////////////////
//   Includes   //
////////////////*/

/*//////////////////
//  Null Driver   //
//////////////////*/
/// @summary Initialize a new instance of the null presentation driver.
/// @param window The window to which the display driver instance is attached.
/// @return A handle to the display driver used to render to the given window.
internal_function uintptr_t __cdecl PrDisplayDriverOpen_Null(HWND window)
{
    UNREFERENCED_PARAMETER(window);
    return (intptr_t) 1;
}

/// @summary Performs an explicit reinitialization of the display driver used to render 
/// content into a given window. This function is primarily used internally to handle 
/// detected device removal conditions, but may be called by the application as well.
/// @param drv The display driver handle returned by PrDisplayDriverOpen().
internal_function void     __cdecl PrDisplayDriverReset_Null(uintptr_t drv)
{
    UNREFERENCED_PARAMETER(drv);
}

/// @summary Handles the case where the window associated with a display driver is resized.
/// This function should be called in response to a WM_SIZE or WM_DISPLAYCHANGE event.
/// @param drv The display driver handle returned by PrDisplayDriverResize().
internal_function void     __cdecl PrDisplayDriverResize_Null(uintptr_t drv)
{
    UNREFERENCED_PARAMETER(drv);
}

/// @summary Retrieves an empty command list that can be used for display command submission.
/// @param drv The display driver handle returned by PrDisplayDriverOpen().
/// @return A display driver command list, or NULL.
internal_function pr_command_list_t* __cdecl PrCreateCommandList_Null(uintptr_t drv)
{
    UNREFERENCED_PARAMETER(drv);
    return NULL;
}

/// @summary Submits a list of buffered commands to the display driver for processing.
/// @param drv The display driver handle returned by PrDisplayDriverOpen().
/// @param cmdlist The command list to submit, allocated with PrCreateCommandList().
/// @param wait Specify true to block the calling thread until the command list is processed by the driver.
/// @param wait_timeout The maximum amount of time to wait, in milliseconds, if wait is also true.
internal_function void __cdecl PrSubmitCommandList_Null(uintptr_t drv, pr_command_list_t *cmdlist, bool wait, uint32_t wait_timeout)
{
    UNREFERENCED_PARAMETER(drv);
    UNREFERENCED_PARAMETER(cmdlist);
    UNREFERENCED_PARAMETER(wait);
    UNREFERENCED_PARAMETER(wait_timeout);
}

/// @summary Copies the current frame to the application window.
/// @param drv The display driver handle returned by PrDisplayDriverOpen().
internal_function void     __cdecl PrPresentFrameToWindow_Null(uintptr_t drv)
{
    UNREFERENCED_PARAMETER(drv);
}

/// @summary Closes a display driver instance and releases all associated resources.
/// @param drv The display driver handle returned by PrDisplayDriverOpen().
internal_function void     __cdecl PrDisplayDriverClose_Null(uintptr_t drv)
{
    UNREFERENCED_PARAMETER(drv);
}

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Define the supported presentation implementations.
enum presentation_type_e : uint32_t
{
    PRESENTATION_TYPE_NULL     = 0,                  /// All presentation operations resolve to no-ops.
    PRESENTATION_TYPE_GDI      = 1,                  /// Standard Windows GDI used for rendering.
    PRESENTATION_TYPE_DIRECT2D = 2,                  /// Direct3D and Direct2D used for accelerated rendering.
    PRESENTATION_TYPE_OPENGL21 = 3,                  /// OpenGL 2.1 (compatibility) used for accelerated rendering.
    PRESENTATION_TYPE_COUNT    = 4,                  /// The number of loadable presentation types.
    PRESENTATION_TYPE_FIRST    = PRESENTATION_TYPE_NULL
};

/// @summary Function signatures for the functions loaded from the presentation DLL.
typedef uintptr_t          (__cdecl *PrDisplayDriverOpenFn   )(HWND);
typedef void               (__cdecl *PrDisplayDriverResetFn  )(uintptr_t);
typedef void               (__cdecl *PrDisplayDriverResizeFn )(uintptr_t);
typedef pr_command_list_t* (__cdecl *PrCreateCommandListFn   )(uintptr_t);
typedef void               (__cdecl *PrSubmitCommandListFn   )(uintptr_t, pr_command_list_t*, bool, uint32_t);
typedef void               (__cdecl *PrPresentFrameToWindowFn)(uintptr_t);
typedef void               (__cdecl *PrDisplayDriverCloseFn  )(uintptr_t);

/// @summary Define the data associated with a presentation driver implementation, 
/// loaded from a DLL at runtime.
struct presentation_driver_t
{
    HMODULE                  DllHandle;              /// The handle of the DLL defining the implementation.
    PrDisplayDriverOpenFn    PrDisplayDriverOpen;    /// Open a rendering context to a specific window.
    PrDisplayDriverResetFn   PrDisplayDriverReset;   /// Reset the display driver state and recreate buffers.
    PrDisplayDriverResizeFn  PrDisplayDriverResize;  /// Resize buffers to the client area of the window.
    PrCreateCommandListFn    PrCreateCommandList;    /// Allocate an empty display command list.
    PrSubmitCommandListFn    PrSubmitCommandList;    /// Submit a previously allocated display command list.
    PrPresentFrameToWindowFn PrPresentFrameToWindow; /// Copy the current frame from the backbuffer to the foreground.
    PrDisplayDriverCloseFn   PrDisplayDriverClose;   /// Close a rendering context and release associated resources.
};

/// @summary Define the data associated with a display driver instance used to 
/// render content to the client area of a single window.
struct display_driver_t
{
    HWND                     WindowHandle;           /// The target window.
    uintptr_t                DriverHandle;           /// The display driver handle returned by PrDisplayDriverOpen.
    presentation_driver_t   *Presentation;           /// The presentation implementation used for rendering operations.

     display_driver_t(void);                         /// Initialize to use null driver.
    ~display_driver_t(void)
    {   // syntatically, in this case, this makes sense.
        if (Presentation != NULL)
        {   // free the presentation driver resources.
            Presentation->PrDisplayDriverClose(DriverHandle);
            Presentation  = NULL;
            DriverHandle  = 0;
        }
    }

    inline void reset(void)
    { 
        Presentation->PrDisplayDriverReset  (DriverHandle);
    }
    inline void resize(void)
    { 
        Presentation->PrDisplayDriverResize (DriverHandle);
    }
    inline void present(void)
    {
        Presentation->PrPresentFrameToWindow(DriverHandle);
    }
    inline pr_command_list_t* create_command_list(void)
    {
        return Presentation->PrCreateCommandList(DriverHandle);
    }
    inline void submit_command_list(pr_command_list_t *cmdlist, bool wait, uint32_t timeout) 
    {
        Presentation->PrSubmitCommandList(DriverHandle,  cmdlist, wait, timeout);
    }
};

/*///////////////
//   Globals   //
///////////////*/
/// @summary Helper macro containing boiler plate code used when resolving a DLL entry point.
#define RESOLVE_PRESENTATION_DLL_FUNCTION(drv, dll, fname)                                          \
    do {                                                                                            \
        drv->fname = (fname##Fn) GetProcAddress(dll, #fname);                                       \
        if (drv->fname == NULL)                                                                     \
        {                                                                                           \
            OutputDebugString(_T("WARNING: No entry point ") _T(#fname) _T("; will use no-op.\n")); \
            drv->fname = fname##_Null;                                                              \
        }                                                                                           \
    __pragma(warning(push));                                                                        \
    __pragma(warning(disable:4127));                                                                \
    } while(0);                                                                                     \
    __pragma(warning(pop))

/// @summary Perform static initialization of a presentation_driver_t to point 
/// to the null (no-op) presentation driver.
#define NULL_PRESENTATION_DRIVER                                \
    {                                                           \
        (HMODULE)                  NULL,                        \
        (PrDisplayDriverOpenFn)    PrDisplayDriverOpen_Null,    \
        (PrDisplayDriverResetFn)   PrDisplayDriverReset_Null,   \
        (PrDisplayDriverResizeFn)  PrDisplayDriverResize_Null,  \
        (PrCreateCommandListFn)    PrCreateCommandList_Null,    \
        (PrSubmitCommandListFn)    PrSubmitCommandList_Null,    \
        (PrPresentFrameToWindowFn) PrPresentFrameToWindow_Null, \
        (PrDisplayDriverCloseFn)   PrDisplayDriverClose_Null    \
    }

/// @summary State information for the various presentation driver types.
/// Each driver type redirects to the null driver until it is loaded.
global_variable presentation_driver_t Global_PresentationDrivers[PRESENTATION_TYPE_COUNT] = 
{
    NULL_PRESENTATION_DRIVER, 
    NULL_PRESENTATION_DRIVER, 
    NULL_PRESENTATION_DRIVER,
    NULL_PRESENTATION_DRIVER
};

/*///////////////////////
//   Local Functions   //
///////////////////////*/

/*///////////////////////
//  Public Functions   //
///////////////////////*/
display_driver_t::display_driver_t(void) 
	: 
    WindowHandle(NULL), 
    DriverHandle(0), 
    Presentation(&Global_PresentationDrivers[PRESENTATION_TYPE_NULL])
{ 
    /* empty */ 
}

/// @summary Query the system to determine whether a particular presentation 
/// driver has been loaded. If the driver has not yet been loaded, it redirects
/// to the null driver, which resolves all operations to no-ops.
/// @param presentation_type One of the values of the presentation_type_e enumeration.
public_function inline bool presentation_driver_loaded(uint32_t presentation_type)
{
    if (presentation_type != PRESENTATION_TYPE_NULL)
    {   // if the DLL has been loaded, the handle will be non-NULL.
        return (Global_PresentationDrivers[presentation_type].DllHandle != NULL);
    }
    else return true;
}

/// @summary Attempts to load a specific presentation driver.
/// @param presentation_type One of the values of the presentation_type_e enumeration.
public_function bool load_presentation_driver(uint32_t presentation_type)
{
    if (presentation_driver_loaded(presentation_type))
    {   // there's no need to load the driver a second time.
        return true;
    }

    TCHAR const *dll_name = NULL; // points to a string literal
    switch (presentation_type)
    {
    case PRESENTATION_TYPE_GDI:
        OutputDebugString(_T("STATUS: Beginning load of presentation driver ") _T("PresentGDI.dll") _T(".\n"));
        dll_name  = _T("PresentGDI.dll");
        break;
    case PRESENTATION_TYPE_DIRECT2D:
        OutputDebugString(_T("STATUS: Beginning load of presentation driver ") _T("PresentD2D.dll") _T(".\n"));
        dll_name  = _T("PresentD2D.dll");
        break;
    case PRESENTATION_TYPE_OPENGL21:
        OutputDebugString(_T("STATUS: Beginning load of presentation driver ") _T("PresentGL2.dll") _T(".\n"));
        dll_name  = _T("PresentGL2.dll");
        break;
    default:
        dll_name  = NULL;
        break;
    }
    if (dll_name == NULL)
    {
        OutputDebugString(_T("ERROR: Requested to load unknown presentation type.\n"));
        return false;
    }

    presentation_driver_t *driver = &Global_PresentationDrivers[presentation_type];
    HMODULE driver_dll = LoadLibrary(dll_name);
    if (driver_dll != NULL)
    {   // resolve all of the known entry points. any that cannot be found
        // will be resolved to the null-driver (no-op) implementation.
        RESOLVE_PRESENTATION_DLL_FUNCTION(driver, driver_dll, PrDisplayDriverOpen);
        RESOLVE_PRESENTATION_DLL_FUNCTION(driver, driver_dll, PrDisplayDriverReset);
        RESOLVE_PRESENTATION_DLL_FUNCTION(driver, driver_dll, PrDisplayDriverResize);
        RESOLVE_PRESENTATION_DLL_FUNCTION(driver, driver_dll, PrCreateCommandList);
        RESOLVE_PRESENTATION_DLL_FUNCTION(driver, driver_dll, PrSubmitCommandList);
        RESOLVE_PRESENTATION_DLL_FUNCTION(driver, driver_dll, PrPresentFrameToWindow);
        RESOLVE_PRESENTATION_DLL_FUNCTION(driver, driver_dll, PrDisplayDriverClose);
        driver->DllHandle = driver_dll;
        OutputDebugString(_T("STATUS: Presentation driver loaded successfully.\n"));
        return true;
    }
    else
    {   // unable to find the DLL in the runtime search paths.
        OutputDebugString(_T("ERROR: Unable to load presentation DLL.\n"));
        return false;
    }
}

/// @summary Attempts to load all presentation driver types.
public_function void load_available_presentation_drivers(void)
{
    for (uint32_t i = PRESENTATION_TYPE_FIRST; i < PRESENTATION_TYPE_COUNT; ++i)
    {
        load_presentation_driver(i);
    }
}

/// @summary Initializes a new display driver used to render content into a given window.
/// If necessary, the associated presentation driver is loaded automatically.
/// @param driver The display driver instance to initialize.
/// @param presentation_type One of the values of presentation_type_e indicating the rendering driver to use.
/// @param window The target window.
/// @return true if the display driver is initialized.
public_function bool display_driver_create(display_driver_t *driver, uint32_t presentation_type, HWND window)
{
    if (load_presentation_driver(presentation_type))
    {
        presentation_driver_t *driver_p = &Global_PresentationDrivers[presentation_type];
        uintptr_t driver_h =   driver_p->PrDisplayDriverOpen(window);
        if (driver_h == 0)
        {   // the display driver state could not be initialized.
            OutputDebugString(_T("ERROR: Unable to initialize display driver state.\n"));
            return false;
        }
        driver->WindowHandle = window;
        driver->DriverHandle = driver_h;
        driver->Presentation = driver_p;
        return true;
    }
    else
    {   // the underlying presentation driver could not be loaded.
        // this should only happen if presentation_type is invalid.
        OutputDebugString(_T("ERROR: Unable to load the requested presentation driver.\n"));
        return false;
    }
}
