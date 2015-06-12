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

#include "filepath.cc"
#include "iobuffer.cc"
#include "aiodriver.cc"
#include "iodecoder.cc"
#include "piodriver.cc"
#include "vfsdriver.cc"
#include "threadio.cc"

#include "imtypes.cc"
#include "imcache.cc"
#include "immemory.cc"

#include "parseutl.cc"
#include "parsedds.cc"
#include "imloader.cc"

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

    aio_driver_t     aio;
    aio_driver_open(&aio);

    pio_driver_t     pio;
    pio_driver_open(&pio, &aio);

    vfs_driver_t     vfs;
    vfs_driver_open(&vfs, &aio, &pio);

    thread_io_t    io;
    io.initialize(&vfs);

    io.mount(VFS_KNOWN_PATH_USER_DOCUMENTS  , "/doc", 1, 0);
    io.mount(VFS_KNOWN_PATH_PUBLIC_DOCUMENTS, "/doc", 0, 1);
    io.put_file("/doc/test_save.txt", "This is a test (user)...\n", 25);
    io.unmount(0); // remove user documents mount
    io.put_file("/doc/test_save.txt", "This is a test (public)...\n", 27);
    io.mount(VFS_KNOWN_PATH_USER_DOCUMENTS  , "/doc", 1, 0);
    //io.mountv("/doc/test_archive.tar", "/doc", 2, 2);
    uint64_t stns = pio_driver_nanotime(&pio);
    stream_decoder_t *d1 = io.load_file("/doc/test_save.txt", VFS_FILE_HINT_NONE, VFS_DECODER_HINT_USE_DEFAULT, 0, 0, NULL);
    if (d1 != NULL)
    {   // here's how you read data from the stream.
        while (!d1->atend())
        {   // refill the stream with available data.
            switch (d1->refill(d1))
            {
            case STREAM_REFILL_RESULT_START:
                break; // begin reading decoded data.
            case STREAM_REFILL_RESULT_YIELD:
                pio_driver_poll(&pio);
                aio_driver_poll(&aio);
                continue; // try and refill again later.
            case STREAM_REFILL_RESULT_ERROR:
                break; // check d1->ErrorCode.
            }
            // read as much decoded data as possible.
            while  (d1->ReadCursor != d1->FinalByte)
            {
                char ch     = (char) *d1->ReadCursor++;
                char buf[2] = {ch,0};
                OutputDebugStringA(buf);
            }
            pio_driver_poll(&pio);
            aio_driver_poll(&aio);
        }
        d1->release();
    }
    uint64_t etns = pio_driver_nanotime(&pio);
    uint64_t dtns = etns - stns;
    double   dts  = (double) dtns / (double) 1000000000ULL;
    vfs_driver_close(&vfs);
    pio_driver_close(&pio);
    aio_driver_close(&aio);

    image_memory_t mem;
    image_memory_create(&mem, 128);

    image_definition_t img;
    img.ImageId        = 0;
    img.ImageFormat    = DXGI_FORMAT_R8G8B8A8_UNORM;
    img.Width          = 2048;
    img.Height         = 2048;
    img.SliceCount     = 1;
    img.ElementIndex   = 0;
    img.ElementCount   = 2;
    img.BytesPerPixel  = 4;
    img.BytesPerBlock  = 0;
    img.Compression    = IMAGE_COMPRESSION_NONE;
    img.LevelCount     = 1;
    img.FileOffsets    =(int64_t*) malloc(2 * sizeof(int64_t));
    img.FileOffsets[0] = 0;
    img.FileOffsets[1] = 4 * 2048 * 2048;
    img.LevelInfo      =(dds_level_desc_t*) malloc(2 * sizeof(dds_level_desc_t));
    img.LevelInfo[0].Index           = 0;
    img.LevelInfo[0].Width           = 2048;
    img.LevelInfo[0].Height          = 2048;
    img.LevelInfo[0].Slices          = 1;
    img.LevelInfo[0].BytesPerElement = 4;
    img.LevelInfo[0].BytesPerRow     = 4 * 2048;
    img.LevelInfo[0].BytesPerSlice   = 4 * 2048 * 2048;
    img.LevelInfo[0].DataSize        = 4 * 2048 * 2048;
    img.LevelInfo[0].Format          = DXGI_FORMAT_R8G8B8A8_UNORM;
    img.LevelInfo[1].Index           = 0;
    img.LevelInfo[1].Width           = 2048;
    img.LevelInfo[1].Height          = 2048;
    img.LevelInfo[1].Slices          = 1;
    img.LevelInfo[1].BytesPerElement = 4;
    img.LevelInfo[1].BytesPerRow     = 4 * 2048;
    img.LevelInfo[1].BytesPerSlice   = 4 * 2048 * 2048;
    img.LevelInfo[1].DataSize        = 4 * 2048 * 2048;
    img.LevelInfo[1].Format          = DXGI_FORMAT_R8G8B8A8_UNORM;

    uint32_t ime = image_memory_reserve_image(&mem, 0, img, IMAGE_ENCODING_RAW, IMAGE_ACCESS_2D);
    dds_level_desc_t l0;
    image_storage_info_t stor;
    void    *pel = image_memory_lock_element(&mem, 0, 0, &l0, stor);
    image_memory_unlock_element(&mem, 0, 0);
    image_memory_evict_element(&mem, 0, pel, stor.BytesReserved, false);
    image_memory_drop_image(&mem, 0);
    image_memory_delete(&mem);

    size_t ofs_a = 0;
    size_t ofs_b = 0;
    size_t ofs_c = 0;
    string_table_t st;
    string_table_create(&st, 128, 0);
    string_table_put(&st, "String A", ofs_a);
    string_table_put(&st, "String B", ofs_b);
    OutputDebugStringA(string_table_get(&st, ofs_a));
    OutputDebugStringA(string_table_get(&st, ofs_b));
    assert(string_table_put(&st, "String A", ofs_c) == string_table_get(&st, ofs_a));
    string_table_delete(&st);

    aio_driver_open(&aio);
    pio_driver_open(&pio, &aio);
    vfs_driver_open(&vfs, &aio, &pio);
    io.initialize(&vfs);
    io.mount(VFS_KNOWN_PATH_EXECUTABLE, "/exec", 0, 0);
    io.mountv("/exec/images", "/images", 0, 1);

    image_memory_t       imm;
    image_memory_create(&imm, 256);

    image_cache_t        imc;
    image_cache_config_t imcc;
    imcc.Behavior      = IMAGE_CACHE_BEHAVIOR_MANUAL;
    imcc.CacheSize     = 16 * 1024 * 1024; // 16MB
    image_cache_create (&imc , 256, imcc);

    image_cache_error_queue_t  errq;
    image_cache_result_queue_t resq;
    mpsc_fifo_u_init(&errq);
    mpsc_fifo_u_init(&resq);

    thread_image_cache_t       cache;
    cache.initialize(&imc);
    cache.add_source(0, "/images/test.dds");
    cache.lock(0, 0, IMAGE_ALL_FRAMES, &resq, &errq, 0);

    // the following items are owned by the image loader.
    image_definition_alloc_t   imgdef_alloc;
    fifo_allocator_init(&imgdef_alloc);
    image_location_alloc_t     imgpos_alloc;
    fifo_allocator_init(&imgpos_alloc);

    size_t nexpected  = 1;
    size_t ncomplete  = 0;
    while (ncomplete != nexpected)
    {
        image_load_t ld;
        while(spsc_fifo_u_consume(&imc.LoadQueue, ld))
        {   // TODO(rlk): dispatch the load request.
            char b[1024] = {};
            sprintf_s(b, "Load : %s [%Iu-%Iu]\n", ld.FilePath, ld.FirstFrame, ld.FinalFrame);
            OutputDebugStringA(b);
            // pretend that we're loading some data here.
            // re-use our image_definition_t from up above.
            //
            // below is what our image loader would do.
            // TODO(rlk): kind of a funky mix of async and sync APIs here. 
            // ultimately it doesn't matter because we process everything in the correct order.
            image_cache_define_frames(&imc, img, &imgdef_alloc); // async
            image_memory_reserve_image(&imm, 0, img, IMAGE_ENCODING_RAW, IMAGE_ACCESS_2D); // sync
            for (size_t i = 0, n = img.ElementCount; i < n; ++i)
            {
                for (size_t j = 0, m = img.LevelCount; j < m; ++j)
                {
                    dds_level_desc_t l;
                    image_storage_info_t s;
                    void *p = image_memory_lock_level(&imm, 0, i, j, l, s);
                    // TODO(rlk): copy some data into the image here...
                    image_memory_unlock_level(&imm, 0, i, j);
                }
                // notify the cache of where the frame data is located in memory.
                dds_level_desc_t frame_desc;
                image_storage_info_t frame_stor;
                if (image_memory_element_info(&imm, 0, 0, frame_desc, frame_stor))
                {
                    image_location_t pos;
                    pos.ImageId      = 0;
                    pos.FrameIndex    = i;
                    pos.BaseAddress   = frame_stor.BaseAddress;
                    pos.BytesReserved = frame_stor.BytesReserved;
                    pos.Context       = (uintptr_t) &imm;
                    image_cache_place_frame(&imc, pos, &imgpos_alloc);
                }
            }
        }

        image_location_t ev;
        while (spsc_fifo_u_consume(&imc.EvictQueue, ev))
        {   // TODO(rlk): dispatch the eviction request.
            char b[1024] = {};
            sprintf_s(b, "Evict: Frame %Iu of %Iu at %p (%Iu bytes).\n", ev.FrameIndex, ev.ImageId, ev.BaseAddress, ev.BytesReserved);
            OutputDebugStringA(b);
            image_memory_evict_element(&imm, ev.ImageId, ev.FrameIndex);
            ncomplete++;
        }

        image_basic_data_t attribs;
        if (cache.image_attributes(0, attribs))
        {
            nexpected = attribs.ElementCount;
        }

        image_cache_error_t err;
        while (mpsc_fifo_u_consume(&errq, err))
        {   // TODO(rlk): handle errors.
            char b[1024] = {};
            sprintf_s(b, "Error: %08X on image %Iu frames %Iu-%Iu.\n", err.ErrorCode, err.ImageId, err.FirstFrame, err.FinalFrame);
            OutputDebugStringA(b);
        }

        image_cache_result_t res;
        while (mpsc_fifo_u_consume(&resq, res))
        {   // TODO(rlk): process the pixel data.
            char b[1024] = {};
            sprintf_s(b, "Lock : Frame %Iu of image %Iu at %p (%Iu bytes).\n", res.FrameIndex, res.ImageId, res.BaseAddress, res.BytesReserved);
            OutputDebugStringA(b);
            cache.unlock(res.ImageId, res.FrameIndex, res.FrameIndex, IMAGE_CACHE_COMMAND_OPTION_EVICT);
        }

        image_cache_update(&imc);
    }

    vfs_driver_close(&vfs);
    pio_driver_close(&pio);
    aio_driver_close(&aio);

    // get a list of all files in the images subdirectory. these files will be 
    // loaded asynchronously and displayed in the main application window.
    if (!create_file_list(&image_files, 0, 0) || !enumerate_files(&image_files, "images", "*.*", true))
    {
        OutputDebugString(_T("ERROR: Unable to enumerate files in images subdirectory.\n"));
        return 0;
    }
    if (image_files.PathCount == 0)
    {
        OutputDebugString(_T("ERROR: No images found in images subdirectory.\n"));
        return 0;
    }
    
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
    if (!display_driver_create(&display, PRESENTATION_TYPE_DIRECT2D, main_window))
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
        pr_command_clear_color_buffer(cmdlist, 1.0, 0.0, 0.0, 1.0);
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
