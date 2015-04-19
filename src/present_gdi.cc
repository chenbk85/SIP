/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines the exported presentation DLL entry points for the GDI 
/// presentation library implementation.
///////////////////////////////////////////////////////////////////////////80*/

/*////////////////
//   Includes   //
////////////////*/
#include <Windows.h>
#include <tchar.h>

/*/////////////////
//   Constants   //
/////////////////*/

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Define all of the state associated with a GDI display driver. This display
/// driver is compatible with Windows versions all the way back to Windows 95.
struct present_driver_gdi_t
{
    HWND                  Window;        /// The handle of the target window.
};

/*///////////////
//   Globals   //
///////////////*/

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
    present_driver_gdi_t *driver = (present_driver_gdi_t*) malloc(sizeof(present_driver_gdi_t));
    if (driver == NULL)   return 0;

    // TODO(rlk): Initialize the DIB section to be used as the back buffer.
    driver->Window = window;
    return (uintptr_t)driver;
}

/// @summary Performs an explicit reinitialization of the display driver used to render 
/// content into a given window. This function is primarily used internally to handle 
/// detected device removal conditions, but may be called by the application as well.
/// @param drv The display driver handle returned by PrDisplayDriverOpen().
void __cdecl PrDisplayDriverReset(uintptr_t drv)
{
    /* empty */
}

/// @summary Handles the case where the window associated with a display driver is resized.
/// This function should be called in response to a WM_SIZE or WM_DISPLAYCHANGE event.
/// @param drv The display driver handle returned by PrDisplayDriverResize().
void __cdecl PrDisplayDriverResize(uintptr_t drv)
{
    // TODO(rlk): reallocate the DIB section at the new size.
}

/// @summary Copies the current frame to the application window.
/// @param drv The display driver handle returned by PrDisplayDriverOpen().
void __cdecl PrPresentFrameToWindow(uintptr_t drv)
{
    // TODO(rlk): StretchDIBBitsToDevice or something.
}

/// @summary Closes a display driver instance and releases all associated resources.
/// @param drv The display driver handle returned by PrDisplayDriverOpen().
void __cdecl PrDisplayDriverClose(uintptr_t drv)
{
    // TODO(rlk): Delete the DIB section and free backbuffer memory.
}
