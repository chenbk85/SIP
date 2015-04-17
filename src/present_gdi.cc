/// @summary Defines the exported presentation DLL entry points for the GDI 
/// presentation library implementation.

#include <Windows.h>

/// @summary Null presentation driver function to copy the contents of the current frame
/// to the specified application window.
/// @param window The handle of the target window.
/// @param window_dc The device context of the target window.
/// @param x The x-coordinate of the upper-left corner of the client rectangle.
/// @param y The y-coordinate of the upper-left corner of the client rectangle.
/// @param width The width of the client rectangle, in pixels.
/// @param height The height of the client rectangle, in pixels.
void __cdecl PrDisplayFrameToWindow(HWND window, HDC window_dc, int x, int y, int width, int height)
{
    UNREFERENCED_PARAMETER(window);
    UNREFERENCED_PARAMETER(window_dc);
    UNREFERENCED_PARAMETER(x);
    UNREFERENCED_PARAMETER(y);
    UNREFERENCED_PARAMETER(width);
    UNREFERENCED_PARAMETER(height);
}
