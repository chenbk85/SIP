/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines the entry point of the application layer. This is where we
/// open up the main application window and process all messages it receives, 
/// along with loading the presentation layer and initializing the system.
///////////////////////////////////////////////////////////////////////////80*/

#pragma warning (disable:4505) // unreferenced local function was removed

#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#ifdef  _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
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
#include "presentation.cc"

/*/////////////////
//   Constants   //
/////////////////*/
/// @summary The scale used to convert from seconds into nanoseconds.
static uint64_t const SEC_TO_NANOSEC = 1000000000ULL;

/*///////////////////
//   Local Types   //
///////////////////*/

/*///////////////
//   Globals   //
///////////////*/
/// @summary A flag used to control whether the main application loop is still running.
global_variable bool            Global_IsRunning       = true;

/// @summary The high-resolution timer frequency. This is queried once at startup.
global_variable int64_t         Global_ClockFrequency  = {0};

/// @summary Information about the window placement saved when entering fullscreen mode.
/// This data is necessary to restore the window to the same position and size when 
/// the user exits fullscreen mode back to windowed mode.
global_variable WINDOWPLACEMENT Global_WindowPlacement = {0};

/*///////////////////////
//   Local Functions   //
///////////////////////*/
/// @summary Format a message and write it to the debug output channel.
/// @param fmt The printf-style format string.
/// ... Substitution arguments.
internal_function void dbg_printf(char const *fmt, ...)
{
    char buffer[1024];
    va_list  args;
    va_start(args, fmt);
    vsnprintf_s(buffer, 1024, fmt, args);
    va_end  (args);
    OutputDebugStringA(buffer);
}

/// @summary Retrieve the current high-resolution timer value. 
/// @return The current time value, specified in system ticks.
internal_function inline int64_t ticktime(void)
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter.QuadPart;
}

/// @summary Retrieve the current high-resolution timer value.
/// @return The current time value, specified in nanoseconds.
internal_function inline uint64_t nanotime(void)
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (SEC_TO_NANOSEC * uint64_t(counter.QuadPart) / uint64_t(Global_ClockFrequency));
}

/// @summary Calculate the elapsed time in nanoseconds.
/// @param start The nanosecond timestamp at the start of the measured interval.
/// @param end The nanosecond timestamp at the end of the measured interval.
/// @return The elapsed time, specified in nanoseconds.
internal_function inline int64_t elapsed_nanos(uint64_t start, uint64_t end)
{
    return int64_t(end - start);
}

/// @summary Calculate the elapsed time in system ticks.
/// @param start The timestamp at the start of the measured interval.
/// @param end The timestamp at the end of the measured interval.
/// @return The elapsed time, specified in system ticks.
internal_function inline int64_t elapsed_ticks(int64_t start, int64_t end)
{
    return (end - start);
}

/// @summary Convert a time duration specified in system ticks to seconds.
/// @param ticks The duration, specified in system ticks.
/// @return The specified duration, specified in seconds.
internal_function inline float ticks_to_seconds(int64_t ticks)
{
    return ((float) ticks / (float) Global_ClockFrequency);
}

/// @summary Convert a time duration specified in nanoseconds to seconds.
/// @param nanos The duration, specified in nanoseconds.
/// @return The specified duration, specified in nanoseconds.
internal_function inline float nanos_to_seconds(int64_t nanos)
{
    return ((float) nanos / (float) SEC_TO_NANOSEC);
}

/// @summaey Toggles the window styles of a given window between standard 
/// windowed mode and a borderless fullscreen" mode. The display resolution
/// is not changed, so rendering will be performed at the desktop resolution.
/// @param window The window whose styles will be updated.
internal_function void toggle_fullscreen(HWND window)
{
    LONG_PTR style = GetWindowLongPtr(window, GWL_STYLE);
    if (style & WS_OVERLAPPEDWINDOW)
    {   // switch to a fullscreen-style window.
        MONITORINFO monitor_info = { sizeof(MONITORINFO)     };
        WINDOWPLACEMENT win_info = { sizeof(WINDOWPLACEMENT) };
        if (GetWindowPlacement(window, &win_info) && GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY), &monitor_info))
        {
            RECT rc = monitor_info.rcMonitor;
            Global_WindowPlacement = win_info;
            SetWindowLongPtr(window, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(window, HWND_TOP , rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    }
    else
    {   // go back to windowed mode.
        SetWindowLongPtr(window, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(window, &Global_WindowPlacement);
        SetWindowPos(window, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

/// @summary Implement the Windows message callback for the main application window.
/// @param hWnd The handle of the main application window.
/// @param uMsg The identifier of the message being processed.
/// @param wParam An unsigned pointer-size value specifying data associated with the message.
/// @param lParam A signed pointer-size value specifying data associated with the message.
/// @return The return value depends on the message being processed.
internal_function LRESULT CALLBACK MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	display_driver_t *display = (display_driver_t*) GetWindowLongPtr(hWnd, GWLP_USERDATA);

    switch (uMsg)
    {
    case WM_DESTROY:
        {	// post the quit message with the application exit code.
			// this will be picked up back in WinMain and cause the
			// message pump and display update loop to terminate.
            PostQuitMessage(EXIT_SUCCESS);
        }
        return 0;

    case WM_SIZE:
        {	// we'll receive WM_SIZE messages prior to the display driver being set up.
            if (display != NULL) display->resize();
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
                    toggle_fullscreen(hWnd);
                    return 0; // prevent default handling.
                }
            }
        }
        break;
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

/*///////////////////////
//  Public Functions   //
///////////////////////*/
/// @summary Implements the entry point of the application.
/// @param hInstance A handle to the current instance of the application.
/// @param hPrev A handle to the previous instance of the application. Always NULL.
/// @param lpCmdLine The command line for the application, excluding the program name.
/// @param nCmdShow Controls how the window is to be shown. See MSDN for possible values.
/// @return Zero if the message loop was not entered, or the value of the WM_QUIT wParam.
int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR lpCmdLine, int nCmdShow)
{
#ifdef  _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    file_list_t      image_files;
	display_driver_t display;
    int result = 0; // return zero if the message loop isn't entered.
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

    // initialize the I/O subsystem.
    aio_driver_t aio; aio_driver_open(&aio);
    pio_driver_t pio; pio_driver_open(&pio, &aio);
    vfs_driver_t vfs; vfs_driver_open(&vfs, &aio, &pio);
    thread_io_t   io; io.initialize(&vfs);
    io.mount(VFS_KNOWN_PATH_EXECUTABLE, "/exec", 0, 0);
    io.mountv("/exec/images", "/images", 0, 1);

    // initialize the imaging subsystem.
    image_cache_t              imc;
    image_loader_t             imld;
    image_memory_t             immemory;
    image_cache_config_t       cache_cfg;
    image_loader_config_t      loader_cfg;
    image_cache_error_queue_t  imfail_queue;
    image_cache_result_queue_t imlock_queue;
    thread_image_cache_t       imcache;
    thread_image_loader_t      imloader;

    image_memory_create(&immemory, 256);
    
    cache_cfg.Behavior  = IMAGE_CACHE_BEHAVIOR_MANUAL;
    cache_cfg.CacheSize = 128 * 1024 * 1024;
    image_cache_create(&imc, 1, cache_cfg);
    imcache.initialize(&imc);
    imcache.add_source(0, "/images/test.dds");
    mpsc_fifo_u_init(&imfail_queue);
    mpsc_fifo_u_init(&imlock_queue);

    loader_cfg.VFSDriver       = &vfs;
    loader_cfg.ImageMemory     = &immemory;
    loader_cfg.DefinitionQueue = &imc.DefinitionQueue;
    loader_cfg.PlacementQueue  = &imc.LocationQueue;
    loader_cfg.ErrorQueue      = NULL;
    loader_cfg.ImageCapacity   = 1;
    loader_cfg.Compression     = IMAGE_COMPRESSION_NONE;
    loader_cfg.Encoding        = IMAGE_ENCODING_RAW;
    image_loader_create(&imld, loader_cfg);
    imloader.initialize(&imld);
    
    // TODO(rlk): perhaps can specify flags like IMAGE_LOCK_NO_NOTIFY to just pre-load.
    imcache.lock(0, 0, IMAGE_ALL_FRAMES, &imlock_queue, &imfail_queue, 0);

    // get a list of all files in the images subdirectory. these files will be 
    // loaded asynchronously and displayed in the main application window.
    /*if (!create_file_list(&image_files, 0, 0) || !enumerate_files(&image_files, "images", "*.*", true))
    {
        OutputDebugString(_T("ERROR: Unable to enumerate files in images subdirectory.\n"));
        return 0;
    }
    if (image_files.PathCount == 0)
    {
        OutputDebugString(_T("ERROR: No images found in images subdirectory.\n"));
        return 0;
    }*/
    
    // @TODO(rlk): init I/O driver and submit all images.

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
    wnd_class.cbWndExtra     = sizeof(void*);
    wnd_class.hInstance      = hInstance;
    wnd_class.lpszClassName  =_T("HAPPI_WndClass");
    wnd_class.lpszMenuName   = NULL;
    wnd_class.lpfnWndProc    = MainWndProc;
    wnd_class.hIcon          = LoadIcon(0, IDI_APPLICATION);
    wnd_class.hIconSm        = LoadIcon(0, IDI_APPLICATION);
    wnd_class.hCursor        = LoadCursor(0, IDC_ARROW);
    wnd_class.style          = CS_HREDRAW | CS_VREDRAW ;
    wnd_class.hbrBackground  = NULL;
    if (!RegisterClassEx(&wnd_class))
    {
        OutputDebugString(_T("ERROR: Unable to register window class.\n"));
        return 0;
    }
    HWND main_window  = CreateWindowEx(0, wnd_class.lpszClassName, _T("HAPPI Main Window"), WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, hInstance, 0);
    if  (main_window == NULL)
    {
        OutputDebugString(_T("ERROR: Unable to create main application window.\n"));
        return 0;
    }
    if (!display_driver_create(&display, PRESENTATION_TYPE_GDI, main_window))
    {
        OutputDebugString(_T("ERROR: Unable to initialize the display driver.\n"));
        return 0;
    }

	// attach the display driver reference to the window so it can be retrieved in WndProc.
	SetWindowLongPtr(main_window, GWLP_USERDATA, (LONG_PTR) &display);

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

    int64_t frame_update_time = ticktime();
    size_t  frame_index = 0;

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
        pr_command_list_t *cmdlist = display.create_command_list();
        pr_command_clear_color_buffer(cmdlist, 0.0, 0.0, 1.0, 1.0);

        // pump everything
        pio_driver_poll(&pio);  // TODO(rlk): move to background thread
        aio_driver_poll(&aio);  // TODO(rlk): move to background thread
        image_loader_update(&imld);
        image_cache_update(&imc);
        image_load_t ld;
        while(spsc_fifo_u_consume(&imc.LoadQueue, ld))
        {
            dbg_printf("Load : %s [%Iu-%Iu]\n", ld.FilePath, ld.FirstFrame, ld.FinalFrame);
            imloader.load(ld);
        }
        image_location_t ev;
        while (spsc_fifo_u_consume(&imc.EvictQueue, ev))
        {
            dbg_printf("Evict: Frame %Iu of %Iu at %p (%Iu bytes).\n", ev.FrameIndex, ev.ImageId, ev.BaseAddress, ev.BytesReserved);
            image_memory_evict_element(&immemory, ev.ImageId, ev.FrameIndex);
        }

        /*image_cache_error_t cache_err;
        while (mpsc_fifo_u_consume(&imfail_queue, cache_err))
        {   // TODO(rlk): handle errors.
            dbg_printf("Error: %08X on image %Iu frames %Iu-%Iu.\n", cache_err.ErrorCode, cache_err.ImageId, cache_err.FirstFrame, cache_err.FinalFrame);
        }

        image_cache_result_t res;
        while (mpsc_fifo_u_consume(&imlock_queue, res))
        {   // TODO(rlk): process the pixel data.
            dbg_printf("Lock : Frame %Iu of image %Iu at %p (%Iu bytes).\n", res.FrameIndex, res.ImageId, res.BaseAddress, res.BytesReserved);
            RECT rc; GetWindowRect(main_window, &rc);
            image_basic_data_t attr;
            imcache.image_attributes(res.ImageId, attr);
            //pr_command_draw_image_2d(cmdlist, res.ImageId, res.FrameIndex, attr.ImageFormat, attr.Width, attr.Height, 0, 0, attr.Width, attr.Height, 0, 0, size_t(rc.right-rc.left), size_t(rc.bottom-rc.top), 0.0f, res.BaseAddress, &imc, 0);
            int64_t nowt = ticktime();
            if (ticks_to_seconds(nowt-frame_update_time) > 1.0f)
            {   // time to display the next frame of the image.
                frame_index=(frame_index+1)%attr.ElementCount;
                frame_update_time=nowt;
            }
            //imcache.unlock(res.ImageId, res.FrameIndex, res.FrameIndex, 0);
            imcache.lock(0, frame_index, frame_index, &imlock_queue, &imfail_queue, 0);
        }*/

        // ...
        pr_command_end_of_frame(cmdlist);
        display.submit_command_list(cmdlist, false, 0);

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
        display.present();

        // update timestamps to calculate the total presentation time.
        int64_t present_ticks = elapsed_ticks(flip_clock, ticktime());
        float   present_secs  = ticks_to_seconds(present_ticks);
        flip_clock = ticktime();
    }

    // perform any system-level cleanup, and exit.
    return result;
}
