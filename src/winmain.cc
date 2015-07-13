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

#ifndef DISPLAY_BACKEND_GDI
#define DISPLAY_BACKEND_GDI       0
#endif

#ifndef DISPLAY_BACKEND_OPENGL
#define DISPLAY_BACKEND_OPENGL    1
#endif

#ifndef DISPLAY_BACKEND_DIRECTX
#define DISPLAY_BACKEND_DIRECTX   0
#endif

/*////////////////
//   Includes   //
////////////////*/
#include <Windows.h>
#include <process.h>
#include <objbase.h>
#include <ShlObj.h>
#include <tchar.h>

#if DISPLAY_BACKEND_DIRECTX
#include <d2d1_1.h>
#include <d3d11_1.h>
#include <dwrite_1.h>

// OpenCL headers must be included after Direct3D.
#include "CL/cl.h"
#include "CL/cl_d3d11.h"
#include "icd.c"
#include "icd_dispatch.c"
#include "icd_windows.c"
#endif

#if DISPLAY_BACKEND_OPENGL
// GLEW configuration - the GLEW implementation is built directly into the 
// presentation library. build with support for multiple contexts since 
// we may have multiple windows that produce output. any place that makes 
// a call into OpenGL or WGL must have a local variable driver that points
// to a gl_display_t instance.
#define  GLEW_MX
#define  GLEW_STATIC
#include "GL/glew.h"
#include "GL/wglew.h"
#include "glew.c"
#undef   glewGetContext
#define  glewGetContext()    (&display->GLEW)
#undef   wglewGetContext
#define  wglewGetContext()   (&display->WGLEW)

// OpenCL headers must be included after GLEW.
#include "CL/cl.h"
#include "CL/cl_gl.h"
#include "CL/cl_gl_ext.h"
#include "icd.c"
#include "icd_dispatch.c"
#include "icd_windows.c"
#endif

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

#if DISPLAY_BACKEND_OPENGL
#include "gldisplay.cc"
#include "gl3driver.cc"
#endif

#if DISPLAY_BACKEND_DIRECTX
#include "d2ddriver.cc"
#endif

#if DISPLAY_BACKEND_GDI
#include "gdidriver.cc"
#endif

#include "ocldriver.cc"

/*/////////////////
//   Constants   //
/////////////////*/
/// @summary The scale used to convert from seconds into nanoseconds.
static uint64_t const SEC_TO_NANOSEC = 1000000000ULL;

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Defines the data required by the background I/O thread.
struct io_thread_args_t
{
    HANDLE          Shutdown;    /// A manual reset event used to signal thread shutdown.
    aio_driver_t   *AIODriver;   /// The application asynchronous I/O driver.
    pio_driver_t   *PIODriver;   /// The application prioritized I/O driver.
    vfs_driver_t   *VFSDriver;   /// The application virtual file system driver.
    image_memory_t *ImageMemory; /// The memory block into which image data will be loaded.
    image_cache_t  *ImageCache;  /// The image cache used to manage the image memory.
};

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

/// @summary Retrieve the current high-resolution timer value.
/// @param frequency The clock frequency, in ticks-per-second.
/// @return The current time value, specified in nanoseconds.
internal_function inline uint64_t nanotime(int64_t frequency)
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return (SEC_TO_NANOSEC * uint64_t(counter.QuadPart) / uint64_t(frequency));
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

/// @summary Convert a time duration specified in system ticks to seconds.
/// @param ticks The duration, specified in system ticks.
/// @param frequency The clock frequency in ticks-per-second.
/// @return The specified duration, specified in seconds.
internal_function inline float ticks_to_seconds(int64_t ticks, int64_t frequency)
{
    return ((float) ticks / (float) frequency);
}

/// @summary Convert a time duration specified in nanoseconds to seconds.
/// @param nanos The duration, specified in nanoseconds.
/// @return The specified duration, specified in nanoseconds.
internal_function inline float nanos_to_seconds(int64_t nanos)
{
    return ((float) nanos / (float) SEC_TO_NANOSEC);
}

/// @summary Toggles the window styles of a given window between standard 
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

/// @summary Helper function used to terminate the calling thread.
/// @param retval The thread exit code.
/// @return The value specified in retval.
internal_function unsigned thread_terminate(unsigned retval)
{
    _endthreadex(retval);
    return retval;
}

/// @summary Implements the main loop of the application background I/O thread.
/// @param args An instance of io_thread_args_t.
/// @return Zero if the thread has terminated normally.
internal_function unsigned __stdcall IoDriverThread(void *args)
{
    io_thread_args_t          *state  = (io_thread_args_t*) args;
    image_cache_error_queue_t  cache_error_queue;
    image_load_error_queue_t   load_error_queue;
    image_loader_t             raw_loader_state;
    image_loader_config_t      raw_loader_config;
    thread_image_loader_t      raw_image_loader;
    thread_io_t                io;

    // save local references to values used in the main loop:
    HANDLE         Shutdown    = state->Shutdown;
    aio_driver_t  *AIODriver   = state->AIODriver;
    pio_driver_t  *PIODriver   = state->PIODriver;
    vfs_driver_t  *VFSDriver   = state->VFSDriver;
    image_cache_t *ImageCache  = state->ImageCache;
    image_memory_t*ImageMemory = state->ImageMemory;

    // assist event identification in trace output.
    trace_thread_id("I/O");

    // save the high-resolution clock frequency.
    LARGE_INTEGER clock_frequency;
    QueryPerformanceFrequency(&clock_frequency);

    // initialize interfaces used to access the I/O subsystem:
    io.initialize(VFSDriver);

    // initialize interfaces used to access the imaging subsystem:
    mpsc_fifo_u_init(&load_error_queue);
    mpsc_fifo_u_init(&cache_error_queue);
    raw_loader_config.VFSDriver       = VFSDriver;
    raw_loader_config.ImageMemory     = ImageMemory;
    raw_loader_config.DefinitionQueue =&ImageCache->DefinitionQueue;
    raw_loader_config.PlacementQueue  =&ImageCache->LocationQueue;
    raw_loader_config.ErrorQueue      =&load_error_queue;
    raw_loader_config.ImageCapacity   = 1;
    raw_loader_config.Compression     = IMAGE_COMPRESSION_NONE;
    raw_loader_config.Encoding        = IMAGE_ENCODING_RAW;
    image_loader_create(&raw_loader_state, raw_loader_config);
    raw_image_loader.initialize(&raw_loader_state);

    // save the current timestamp, used to throttle the update loop.
    int64_t  const time_slice = (16LL * 1000000LL) + (6000000LL); // 16.6ms -> ns
    uint64_t       prev_time  = nanotime(clock_frequency.QuadPart);

    for ( ; ; )
    {   // check to see whether the I/O thread is being terminated:
        if (WaitForSingleObjectEx(Shutdown, 0, FALSE) == WAIT_OBJECT_0)
        {   // thread termination has been signaled:
            break;
        }

        // poll the underlying I/O drivers. the PIO driver generates AIO 
        // driver events, so execute its update first, followed by AIO.
        pio_driver_poll(PIODriver);
        aio_driver_poll(AIODriver);

        // poll the image cache. this may generate load and/or eviction requests.
        // load requests will be directed to one of the image loaders.
        // eviction requests are directed to the image memory manager.
        image_cache_update(ImageCache);
        
        // process events generated by the imaging subsystem.
        image_load_t ld;
        while (spsc_fifo_u_consume(&ImageCache->LoadQueue, ld))
        {
            raw_image_loader.load(ld);
        }
        image_location_t ev;
        while (spsc_fifo_u_consume(&ImageCache->EvictQueue, ev))
        {
            image_memory_evict_element(state->ImageMemory, ev.ImageId, ev.FrameIndex);
        }

        // poll the image loaders. this will produce events in the image cache's 
        // definition and location queues, and possibly error events in our queue.
        image_loader_update(&raw_loader_state);

        // process any image load errors generated by the loader update step.
        image_load_error_t le;
        while (mpsc_fifo_u_consume(&load_error_queue, le))
        {
            dbg_printf("Error loading %s.\n", le.FilePath);
        }

        // sleep for the remainder of the timeslice.
        uint64_t curr_time = nanotime(clock_frequency.QuadPart);
        int64_t  elapsed_t = elapsed_nanos(prev_time, curr_time);
        prev_time = curr_time; // swap time values
        if ((time_slice - elapsed_t) >= 1000000)
        {   // there's at least 1ms remaining in the timeslice. sleep.
            DWORD sleep_time = (DWORD) ((time_slice - elapsed_t) / 1000000);
            Sleep(sleep_time);
        }
    }

    // cleanup resources and terminate the thread.
    image_loader_delete(&raw_loader_state);
    mpsc_fifo_u_delete(&cache_error_queue);
    mpsc_fifo_u_delete(&load_error_queue);
    return thread_terminate(0);
}

/// @summary Launch the background I/O thread.
/// @param args I/O thread data.
/// @return The thread handle, or INVALID_HANDLE_VALUE.
internal_function HANDLE launch_io_thread(io_thread_args_t *args)
{
    return (HANDLE) _beginthreadex(NULL, 0, IoDriverThread, args, 0, NULL);
}

/// @summary Perform any mount-point setup for the I/O system on the main thread.
/// @param io The I/O system interface for the main thread.
/// @return true if the I/O system setup completed successfully.
internal_function bool io_setup(thread_io_t *io)
{
    io->mount(VFS_KNOWN_PATH_EXECUTABLE, "/exec", 0, 0);
    io->mountv("/exec/images", "/images", 0, 1);
    return true;
}

/// @summary Implement the Windows message callback for the main application window.
/// @param hWnd The handle of the main application window.
/// @param uMsg The identifier of the message being processed.
/// @param wParam An unsigned pointer-size value specifying data associated with the message.
/// @param lParam A signed pointer-size value specifying data associated with the message.
/// @return The return value depends on the message being processed.
internal_function LRESULT CALLBACK MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	uintptr_t display = (uintptr_t) GetWindowLongPtr(hWnd, GWLP_USERDATA);

    switch (uMsg)
    {
    case WM_DESTROY:
        {	// post the quit message with the application exit code.
			// this will be picked up back in WinMain and cause the
			// message pump and display update loop to terminate.
            PostQuitMessage(EXIT_SUCCESS);
        }
        return 0;

    // TODO(rlk): Detect the window being moved between displays.
    // TODO(rlk): Detect the window being resized and moving to another display.

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
    aio_driver_t  aio; aio_driver_open(&aio);
    pio_driver_t  pio; pio_driver_open(&pio, &aio);
    vfs_driver_t  vfs; vfs_driver_open(&vfs, &aio, &pio);
    thread_io_t    io; io.initialize(&vfs);
    if (!io_setup(&io))
    {
        OutputDebugString(_T("ERROR: Unable to initialize the I/O system.\n"));
        return 0;
    }

    // initialize the imaging subsystem.
    image_memory_t       image_memory;
    image_cache_t        cache_state;
    image_cache_config_t cache_config;
    thread_image_cache_t image_cache;
    image_memory_create(&image_memory, 256);
    cache_config.Behavior  = IMAGE_CACHE_BEHAVIOR_MANUAL;
    cache_config.CacheSize = 128 * 1024 * 1024;
    image_cache_create(&cache_state, 256, cache_config);
    image_cache.initialize(&cache_state);
    image_cache.add_source(0, "/images/test.dds");

    // configure and launch the background I/O thread.
    io_thread_args_t io_args;
    HANDLE  shutdown    = CreateEvent(NULL, TRUE, FALSE, NULL);
    io_args.Shutdown    = shutdown;
    io_args.AIODriver   = &aio;
    io_args.PIODriver   = &pio;
    io_args.VFSDriver   = &vfs;
    io_args.ImageMemory = &image_memory;
    io_args.ImageCache  = &cache_state;
    HANDLE  io_thread   = launch_io_thread(&io_args);
    if (io_thread == INVALID_HANDLE_VALUE)
    {
        OutputDebugString(_T("ERROR: Unable to launch background I/O thread.\n"));
        return 0;
    }
    
    // TODO(rlk): perhaps can specify flags like IMAGE_LOCK_NO_NOTIFY to just pre-load.
    //imcache.lock(0, 0, IMAGE_ALL_FRAMES, &imlock_queue, &imfail_queue, 0);

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

    // enumerate OpenGL displays and create the main application window.
    gl_display_list_t        display_list;
    if (!enumerate_displays(&display_list))
    {
        OutputDebugString(_T("ERROR: No OpenGL 3.2 displays available.\n"));
        return 0;
    }

    // the main window will be created on the default display:
    // TODO(rlk): add helper to get the primary display.
    gl_display_t *display    = display_for_ordinal(&display_list, display_list.DisplayOrdinal[0]);

    // register the window class for our main window:
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

    // TODO(rlk): make_window_on_display() should accept CW_USEDEFAULT for x, y, w and h.
    HWND  main_window  = make_window_on_display(display, wnd_class.lpszClassName, display->DisplayInfo.DeviceName, WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 0, 640, 480, NULL, NULL, hInstance, NULL, NULL);
    if (main_window == NULL)
    {
        OutputDebugString(_T("ERROR: Unable to create the main application window.\n"));
        return 0;
    }

    // create the renderer for the main display:
    uintptr_t renderer = 0;
    if ((renderer = create_renderer(display, main_window)) == 0)
    {
        OutputDebugString(_T("ERROR: Unable to create the main window renderer.\n"));
        return 0;
    }

	// attach the renderer handle to the window so it can be retrieved in WndProc.
	SetWindowLongPtr(main_window, GWLP_USERDATA, (LONG_PTR) renderer);

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
        pr_command_list_t *cmdlist  = renderer_command_list(renderer);
        pr_command_clear_color_buffer(cmdlist, 0.0, 0.0, 1.0, 1.0);

        // ...
        renderer_submit(renderer, cmdlist); // TODO(rlk): specify target HWND
        update_drawable(renderer);

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
        present_drawable(renderer);

        // update timestamps to calculate the total presentation time.
        int64_t present_ticks = elapsed_ticks(flip_clock, ticktime());
        float   present_secs  = ticks_to_seconds(present_ticks);
        flip_clock = ticktime();
    }

    // signal all threads to exit, and then wait for them:
    SetEvent(shutdown);
    HANDLE handle_obj[] = 
    {
        io_thread
    };
    DWORD  handle_count = (DWORD) (sizeof(handle_obj) / sizeof(handle_obj[0]));
    WaitForMultipleObjects(handle_count , handle_obj, TRUE, INFINITE);

    // clean up application resources, and exit.
    CloseHandle(shutdown);
    delete_renderer(renderer);
    delete_display_list(&display_list);
    image_cache_delete(&cache_state);
    image_memory_delete(&image_memory);
    vfs_driver_close(&vfs);
    pio_driver_close(&pio);
    aio_driver_close(&aio);
    return result;
}
