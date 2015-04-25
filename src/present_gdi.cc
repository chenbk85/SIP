/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines the exported presentation DLL entry points for the GDI 
/// presentation library implementation.
///////////////////////////////////////////////////////////////////////////80*/

/*////////////////
//   Includes   //
////////////////*/
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <Windows.h>
#include <tchar.h>

#include "intrinsics.h"
#include "atomic_fifo.h"

#include "prcmdlist.cc"

/*/////////////////
//   Constants   //
/////////////////*/

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Define all of the state associated with a GDI display driver. This display
/// driver is compatible with Windows versions all the way back to Windows 95. Even 
/// though technically it is not required to call CreateDIBSection, the driver does so 
/// because some operations may need to render into the bitmap through the device context.
struct present_driver_gdi_t
{
    size_t             Pitch;            /// The number of bytes per-scanline in the image.
    size_t             Height;           /// The number of scanlines in the image.
    uint8_t           *DIBMemory;        /// The DIBSection memory, allocated with VirtualAlloc.
    size_t             Width;            /// The actual width of the image, in pixels.
    size_t             BytesPerPixel;    /// The number of bytes allocated for each pixel.
    HDC                MemoryDC;         /// The memory device context used for rendering operations.
    HWND               Window;           /// The handle of the target window.
    HBITMAP            DIBSection;       /// The DIBSection backing the memory device context.
    BITMAPINFO         BitmapInfo;       /// A description of the bitmap layout and attributes.
    pr_command_queue_t CommandQueue;     /// The driver's command list submission queue.
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
    pr_command_queue_init(&driver->CommandQueue);
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
void __cdecl PrSubmitCommandList(uintptr_t drv, pr_command_list_t *cmdlist)
{
    present_driver_gdi_t *driver = (present_driver_gdi_t*) drv;
    if (driver == NULL)   return;
    pr_command_queue_submit(&driver->CommandQueue, cmdlist);
}

/// @summary Copies the current frame to the application window.
/// @param drv The display driver handle returned by PrDisplayDriverOpen().
void __cdecl PrPresentFrameToWindow(uintptr_t drv)
{
    present_driver_gdi_t *driver = (present_driver_gdi_t*) drv;
    if (driver == NULL)   return;
    // TODO(rlk): Process the queued command list to build the frame.
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
    free(driver);
}
