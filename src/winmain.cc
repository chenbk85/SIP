/// @summary Defines the entry point of the application layer. This is where we open up the 
/// main application window and process all messages it receives, along with loading the 
/// presentation layer and initializing the system.

#include <Windows.h>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <float.h>
#include <tchar.h>

#include "intrinsics.h"
#include "atomic_fifo.h"

#include "runtime.cc"
#include "presentation.cc"

/*****************************************************************************/
/*                                   CONSTANTS                               */
/*****************************************************************************/

/// @summary The scale used to convert from seconds into nanoseconds.
static uint64_t const SEC_TO_NANOSEC = 1000000000ULL;

/*****************************************************************************/
/*                                     TYPES                                 */
/*****************************************************************************/

/*****************************************************************************/
/*                                    GLOBALS                                */
/*****************************************************************************/

/// @summary A flag used to control whether the main application loop is still running.
static bool    Global_IsRunning               = true;

/// @summary The high-resolution timer frequency. This is queried once at startup.
static int64_t Global_ClockFrequency          = {0};

/// @summary Information about the window placement saved when entering fullscreen mode.
static WINDOWPLACEMENT Global_WindowPlacement = {0};
    
static display_driver_t Global_Display;

/*****************************************************************************/
/*                                TIME MANAGEMENT                            */
/*****************************************************************************/

/// @summary Retrieve the current high-resolution timer value. 
/// @return The current time value, specified in system ticks.
static inline int64_t ticktime(void)
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter.QuadPart;
}

/// @summary Retrieve the current high-resolution timer value.
/// @return The current time value, specified in nanoseconds.
static inline uint64_t nanotime(void)
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (SEC_TO_NANOSEC * uint64_t(counter.QuadPart) / uint64_t(Global_ClockFrequency));
}

/// @summary Calculate the elapsed time in nanoseconds.
/// @param start The nanosecond timestamp at the start of the measured interval.
/// @param end The nanosecond timestamp at the end of the measured interval.
/// @return The elapsed time, specified in nanoseconds.
static inline int64_t elapsed_nanos(uint64_t start, uint64_t end)
{
    return int64_t(end - start);
}

/// @summary Calculate the elapsed time in system ticks.
/// @param start The timestamp at the start of the measured interval.
/// @param end The timestamp at the end of the measured interval.
/// @return The elapsed time, specified in system ticks.
static inline int64_t elapsed_ticks(int64_t start, int64_t end)
{
    return (end - start);
}

/// @summary Convert a time duration specified in system ticks to seconds.
/// @param ticks The duration, specified in system ticks.
/// @return The specified duration, specified in seconds.
static inline float ticks_to_seconds(int64_t ticks)
{
    return ((float) ticks / (float) Global_ClockFrequency);
}

/// @summary Convert a time duration specified in nanoseconds to seconds.
/// @param nanos The duration, specified in nanoseconds.
/// @return The specified duration, specified in nanoseconds.
static inline float nanos_to_seconds(int64_t nanos)
{
    return ((float) nanos / (float) SEC_TO_NANOSEC);
}

/*****************************************************************************/
/*                               WINDOW MANAGEMENT                           */
/*****************************************************************************/

static void toggle_fullscreen(HWND window)
{
    DWORD style = GetWindowLong(window, GWL_STYLE);
    if   (style & WS_OVERLAPPEDWINDOW)
    {   // switch to a fullscreen-style window.
        MONITORINFO monitor_info = { sizeof(MONITORINFO)     };
        WINDOWPLACEMENT win_info = { sizeof(WINDOWPLACEMENT) };
        if (GetWindowPlacement(window, &win_info) && GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY), &monitor_info))
        {
            RECT rc = monitor_info.rcMonitor;
            Global_WindowPlacement = win_info;
            SetWindowLong(window, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos (window, HWND_TOP , rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    }
    else
    {   // go back to windowed mode.
        SetWindowLong(window, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(window, &Global_WindowPlacement);
        SetWindowPos (window, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

/// @summary Implement the Windows message callback for the main application window.
/// @param hWnd The handle of the main application window.
/// @param uMsg The identifier of the message being processed.
/// @param wParam An unsigned pointer-size value specifying data associated with the message.
/// @param lParam A signed pointer-size value specifying data associated with the message.
/// @return The return value depends on the message being processed.
static LRESULT CALLBACK MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
        {
            PostQuitMessage(EXIT_SUCCESS);
        }
        return 0;

    case WM_SIZE:
        {
            if (Global_Display.Presentation != NULL)
            {
                Global_Display.resize();
            }
        }
        break;

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP:
        {   // application-level user keyboard input handling.
            uint32_t vk_code  = (uint32_t)  wParam;
            bool     is_down  = ((lParam & (1 << 31)) == 0);
            bool     was_down = ((lParam & (1 << 30)) != 0);
            
            if (is_down)
            {
                bool alt_down = ((lParam & (1 << 29)) != 0);
                if (vk_code == VK_RETURN && alt_down)
                {
                    // TODO(rlk): With Direct2D this doesn't work correctly.
                    // Direct2D seems to try and change the display mode...
                    toggle_fullscreen(hWnd);
                    return 0; // prevent default handling.
                }
            }
        }
        break;
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

/*****************************************************************************/
/*                                 ENTRY POINT                               */
/*****************************************************************************/

/// @summary Implements the entry point of the application.
/// @param hInstance A handle to the current instance of the application.
/// @param hPrev A handle to the previous instance of the application. Always NULL.
/// @param lpCmdLine The command line for the application, excluding the program name.
/// @param nCmdShow Controls how the window is to be shown. See MSDN for possible values.
/// @return Zero if the message loop was not entered, or the value of the WM_QUIT wParam.
int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR lpCmdLine, int nCmdShow)
{
    int result = 0; // return zero if the message loop isn't entered.
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrev);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);
    if (!win32_runtime_init())
    {
        OutputDebugString(_T("ERROR: Unable to initialize Windows runtime support.\n"));
        return 0;
    }
    
    // elevate process privileges (necessary for some I/O functions).
    // it's not a fatal error if this can't be completed.
    trace_thread_id("main");
    win32_runtime_elevate();

    // query the clock frequency of the high-resolution timer.
    LARGE_INTEGER clock_frequency;
    QueryPerformanceFrequency(&clock_frequency);
    Global_ClockFrequency = clock_frequency.QuadPart;

    // set the scheduler granularity to 1ms for more accurate Sleep.
    UINT desired_granularity = 1; // millisecond
    BOOL sleep_is_granular   =(timeBeginPeriod(desired_granularity) == TIMERR_NOERROR);

    // create the main application window.
    WNDCLASSEX wnd_class     = {};
    wnd_class.cbSize         = sizeof(WNDCLASSEX);
    wnd_class.cbClsExtra     = 0;
    wnd_class.cbWndExtra     = 0;
    wnd_class.hInstance      = hInstance;
    wnd_class.lpszClassName  =_T("AIP_WndClass");
    wnd_class.lpszMenuName   = NULL;
    wnd_class.lpfnWndProc    = MainWndProc;
    wnd_class.hIcon          = NULL;
    wnd_class.hIconSm        = NULL;
    wnd_class.hCursor        = LoadCursor(0, IDC_ARROW);
    wnd_class.style          = CS_HREDRAW | CS_VREDRAW ;
    wnd_class.hbrBackground  = NULL;
    if (!RegisterClassEx(&wnd_class))
    {
        OutputDebugString(_T("ERROR: Unable to register window class.\n"));
        return 0;
    }
    HWND main_window  = CreateWindowEx(0, wnd_class.lpszClassName, _T("AIP Main Window"), WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, hInstance, 0);
    if  (main_window == NULL)
    {
        OutputDebugString(_T("ERROR: Unable to create main application window.\n"));
        return 0;
    }
    if (!display_driver_create(&Global_Display, PRESENTATION_TYPE_DIRECT2D, main_window))
    {
        OutputDebugString(_T("ERROR: Unable to initialize the display driver.\n"));
        return 0;
    }

    // query the monitor refresh rate and use that as our target frame rate.
    int monitor_refresh_hz =  60;
    HDC monitor_refresh_dc =  GetDC(main_window);
    int refresh_rate_hz    =  GetDeviceCaps(monitor_refresh_dc, VREFRESH);
    if (refresh_rate_hz >  1) monitor_refresh_hz =  refresh_rate_hz;
    float present_rate_hz  =  monitor_refresh_hz /  2.0f;
    float present_rate_sec =  1.0f / present_rate_hz;
    ReleaseDC(main_window, monitor_refresh_dc);

    // initialize timer snapshots used to throttle update rate.
    int64_t last_clock     =  ticktime();
    int64_t flip_clock     =  ticktime();

    // run the main window message pump and user interface loop.
    while (Global_IsRunning)
    {   // emit an ETW marker to indicate the start of an update.
        trace_marker_main("tick_update");
        // dispatch Windows messages while messages are waiting.
        // specify NULL as the HWND to retrieve all messages for
        // the current thread, not just messages for the window.
        MSG msg;
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            switch (msg.message)
            {
            case WM_QUIT:
                result = (int) msg.wParam;
                Global_IsRunning = false;
                break;

            default:
                TranslateMessage(&msg);
                DispatchMessage (&msg);
                break;
            }
        }

        // TODO(rlk): perform application update here.
        // this consists primarily of queueing commands to the display driver.

        // throttle the application update rate.
        trace_marker_main("tick_throttle");
        int64_t update_ticks = elapsed_ticks(last_clock, ticktime());
        float   update_secs  = ticks_to_seconds(update_ticks);
        if (update_secs < present_rate_sec)
        {   // TODO(rlk): can we do this in all-integer arithmetic?
            float sleep_ms_f =(1000.0f * (present_rate_sec - update_secs));
            if   (sleep_ms_f > desired_granularity)
            {   // should be able to sleep accurately.
                Sleep((DWORD)  sleep_ms_f);
            }
            else
            {   // sleep time is less than scheduler granularity; busy wait.
                while (update_secs < present_rate_sec)
                {
                    update_ticks   = elapsed_ticks(last_clock, ticktime());
                    update_secs    = ticks_to_seconds(update_ticks);
                }
            }
        }
        // else, we missed our target rate, so run full-throttle.
        last_clock = ticktime();

        // now perform the actual presentation of the display buffer to the window.
        // this may take some non-negligible amount of time, as it involves processing
        // queued command buffers to construct the current frame.
        trace_marker_main("tick_present");
        Global_Display.present();

        // update timestamps to calculate the total presentation time.
        int64_t present_ticks = elapsed_ticks(flip_clock, ticktime());
        float   present_secs  = ticks_to_seconds(present_ticks);
        flip_clock = ticktime();
    }

    // perform any system-level cleanup, and exit.
    return result;
}
