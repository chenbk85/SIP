/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines the exported presentation DLL entry points for the 
/// Direct2D presentation library implementation.
/// Some useful links:
/// https://katyscode.wordpress.com/
/// http://blog.airesoft.co.uk/2014/05/windows-sdk-8-1-iso/#debug
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

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Define all of the state associated with a Direct3D/Direct2D display driver.
/// This presentation driver requires Windows 7 w/Platform Update 1 or later, and relies
/// on both Direct3D 11.1, Direct2D 1.1 and DirectWrite 1.1.
struct present_driver_d2d_t
{
    ID2D1Factory1        *D2DFactory;    /// The Direct2D interface factory.
    ID2D1Device          *D2DDevice;     /// The Direct2D resource management interface.
    ID2D1DeviceContext   *D2DContext;    /// The Direct2D rendering context.
    ID2D1Bitmap1         *BackBuffer;    /// The Direct2D bitmap representing the back buffer.
    ID3D11DeviceContext1 *D3DContext;    /// The Direct3D rendering context.
    ID3D11Device1        *D3DDevice;     /// The Direct3D resource management interface.
    IDXGISwapChain1      *SwapChain;     /// The swap chain connecting Direct3D, Direct2D and the windowing system.
    IDWriteFactory1      *DWFactory;     /// The DirectWrite interface factory.
    D3D_FEATURE_LEVEL     D3DFeature;    /// The Direct3D feature level supported by the display adapter.
    HWND                  Window;        /// The handle of the target window.
    bool                  IsWARP;        /// True if the WARP rendering engine is in use.
    pr_command_queue_t    CommandQueue;  /// The driver's command list submission queue.
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
    present_driver_d2d_t *driver = (present_driver_d2d_t*) malloc(sizeof(present_driver_d2d_t));
    if (driver == NULL)   return 0;

    UINT flags  = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_SINGLETHREADED;
    DXGI_SWAP_CHAIN_DESC1   swap_desc    = {};
    D2D1_FACTORY_OPTIONS    option_2d    = {};
    D2D1_BITMAP_PROPERTIES1 bitmap_desc  = {};
    D3D_FEATURE_LEVEL       got_level    =(D3D_FEATURE_LEVEL) 0;
    D3D_FEATURE_LEVEL       d3d_level[]  = {
        D3D_FEATURE_LEVEL_11_1, 
        D3D_FEATURE_LEVEL_11_0, 
        D3D_FEATURE_LEVEL_10_1, 
        D3D_FEATURE_LEVEL_10_0, 
        D3D_FEATURE_LEVEL_9_3, 
        D3D_FEATURE_LEVEL_9_2, 
        D3D_FEATURE_LEVEL_9_1
    };
    UINT                    level_count  =(UINT)(sizeof(d3d_level) / sizeof(d3d_level[0]));
    bool                    warp_device  = false;
    IDXGIDevice            *dxgi_device  = NULL; // temporary, used to create Direct2D device.
    IDXGIAdapter           *dxgi_adapter = NULL; // temporary, used to retrieve DXGI factory.
    IDXGIFactory2          *dxgi_factory = NULL; // temporary, used to create swap chain.
    IDXGISurface           *back_buffer  = NULL; // temporary, used to create back buffer render target.
    ID3D11Device           *d3d_device   = NULL; // temporary, used for retrieving 11.1 device.
    ID3D11DeviceContext    *d3d_context  = NULL; // temporary, used for retrieving 11.1 context.

    // initialize pointer fields to NULL to avoid releasing them if an error occurs.
    driver->D2DFactory  = NULL;
    driver->D2DDevice   = NULL;
    driver->D2DContext  = NULL;
    driver->BackBuffer  = NULL;
    driver->D3DContext  = NULL;
    driver->D3DDevice   = NULL;
    driver->SwapChain   = NULL;
    driver->DWFactory   = NULL;
    driver->D3DFeature  =(D3D_FEATURE_LEVEL) 0;
    driver->Window      = NULL;
    driver->IsWARP      = false;

    // initialize the Direct2D and DirectWrite interface factories.
#if _DEBUG
    option_2d.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#else
    option_2d.debugLevel = D2D1_DEBUG_LEVEL_NONE;
#endif
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &option_2d, (void**) &driver->D2DFactory)))
    {
        OutputDebugString(_T("ERROR: Unable to create Direct2D interface factory.\n"));
        goto error_cleanup;
    }
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory1), (IUnknown**) &driver->DWFactory)))
    {
        OutputDebugString(_T("ERROR: Unable to create DirectWrite interface factory.\n"));
        goto error_cleanup;
    }

    // now create the Direct3D device. Direct3D is used internally by Direct2D.
    // if a hardware-accelerated device isn't available, fallback to using WARP.
    // this is (supposedly) still signifcantly faster than using straight GDI.
    if (FAILED(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags, d3d_level, level_count, D3D11_SDK_VERSION, &d3d_device, &got_level, &d3d_context)))
    {   // if we couldn't create a hardware-accelerated device, use WARP.
        if (FAILED(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_WARP, NULL, flags, d3d_level, level_count, D3D11_SDK_VERSION, &d3d_device, &got_level, &d3d_context)))
        {   // no Direct3D devices are available. this renderer cannot be used.
            OutputDebugString(_T("ERROR: No Direct3D 11.0 device is available.\n"));
            goto error_cleanup;
        }
        warp_device = true; // got a WARP device
    }
    else warp_device = false; // got a hardware device

    // use QueryInterface to get the 11.1 interfaces from the 11.0 interfaces.
    if (FAILED(d3d_device->QueryInterface(IID_PPV_ARGS(&driver->D3DDevice))))
    {
        OutputDebugString(_T("ERROR: Unable to retrieve Direct3D 11.1 device.\n"));
        goto error_cleanup;
    }
    if (FAILED(d3d_context->QueryInterface(IID_PPV_ARGS(&driver->D3DContext))))
    {
        OutputDebugString(_T("ERROR: Unable to retrieve Direct3D 11.1 device context.\n"));
        goto error_cleanup;
    }
    // retrieve the DXGI interop device and use it to create the Direct2D device.
    if (FAILED(driver->D3DDevice->QueryInterface(IID_PPV_ARGS(&dxgi_device))))
    {
        OutputDebugString(_T("ERROR: Unable to retrieve DXGI device interface.\n"));
        goto error_cleanup;
    }
    if (FAILED(driver->D2DFactory->CreateDevice(dxgi_device, &driver->D2DDevice)))
    {
        OutputDebugString(_T("ERROR: Unable to create Direct2D device.\n"));
        goto error_cleanup;
    }
    // TODO(rlk): Maybe specify D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS?
    // This distributes geometry generation load across multiple cores in the system.
    if (FAILED(driver->D2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &driver->D2DContext)))
    {
        OutputDebugString(_T("ERROR: Unable to create Direct2D device context.\n"));
        goto error_cleanup;
    }

    // -- begin window-size dependent resources initialization

    // now create a swap chain attached to the supplied window. each swap chain is double-buffered.
    swap_desc.Width            = 0; // will obtain from window
    swap_desc.Height           = 0; // will obtain from window
    swap_desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    swap_desc.Stereo           = FALSE;
    swap_desc.SampleDesc.Count = 1; // Direct2D will handle MSAA
    swap_desc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_desc.BufferCount      = 2; // double buffered
    swap_desc.Scaling          = DXGI_SCALING_STRETCH;
    swap_desc.SwapEffect       = DXGI_SWAP_EFFECT_DISCARD;
    swap_desc.AlphaMode        = DXGI_ALPHA_MODE_IGNORE;
    swap_desc.Flags            = DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE | DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    if (FAILED(dxgi_device->GetAdapter(&dxgi_adapter)))
    {
        OutputDebugString(_T("ERROR: Unable to retrieve DXGI virtual display adapter.\n"));
        goto error_cleanup;
    }
    if (FAILED(dxgi_adapter->GetParent(IID_PPV_ARGS(&dxgi_factory))))
    {
        OutputDebugString(_T("ERROR: Unable to retrieve DXGI interface factory.\n"));
        goto error_cleanup;
    }
    if (FAILED(dxgi_factory->CreateSwapChainForHwnd(driver->D3DDevice, window, &swap_desc, NULL, NULL, &driver->SwapChain)))
    {
        OutputDebugString(_T("ERROR: Unable to create DXGI swap chain for presentation.\n"));
        goto error_cleanup;
    }
    if (FAILED(driver->SwapChain->GetBuffer(0, IID_PPV_ARGS(&back_buffer))))
    {
        OutputDebugString(_T("ERROR: Unable to retrieve DXGI surface representing back buffer.\n"));
        goto error_cleanup;
    }

    // disable Alt+Enter handling by DXGI; it does it poorly.
    dxgi_factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);
    
    // the final steps are to create a Direct2D bitmap attached to the DXGI
    // surface that represents the swap chain back buffer, and set up D2D scaling.
    driver->D2DFactory->GetDesktopDpi(&bitmap_desc.dpiX, &bitmap_desc.dpiY);
    bitmap_desc.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW | D2D1_BITMAP_OPTIONS_GDI_COMPATIBLE;
    bitmap_desc.colorContext  = NULL;
    bitmap_desc.pixelFormat   = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE);
    if (FAILED(driver->D2DContext->CreateBitmapFromDxgiSurface(back_buffer, bitmap_desc, &driver->BackBuffer)))
    {
        OutputDebugString(_T("ERROR: Unable to create render target from DXGI backbuffer.\n"));
        goto error_cleanup;
    }
    driver->D2DContext->SetTarget(driver->BackBuffer);
    driver->D2DContext->SetDpi(bitmap_desc.dpiX, bitmap_desc.dpiY);
    driver->D3DFeature= got_level;
    driver->Window    = window;
    driver->IsWARP    = warp_device;
    pr_command_queue_init(&driver->CommandQueue);

    // release references to all temporary interface pointers.
    back_buffer ->Release();
    dxgi_factory->Release();
    dxgi_adapter->Release();
    dxgi_device ->Release();
    d3d_context ->Release(); // maintain the 11.1 context
    d3d_device  ->Release(); // maintain the 11.1 device
    return (uintptr_t)driver;

error_cleanup:
    // release references to all temporary interface pointers.
    if (back_buffer  != NULL) back_buffer ->Release();
    if (dxgi_factory != NULL) dxgi_factory->Release();
    if (dxgi_adapter != NULL) dxgi_adapter->Release();
    if (dxgi_device  != NULL) dxgi_device ->Release();
    if (d3d_context  != NULL) d3d_context ->Release();
    if (d3d_device   != NULL) d3d_device  ->Release();
    // release references to any interfaces stored in the driver.
    if (driver != NULL)
    {   // perform a complete cleanup here, since the driver is being destroyed.
        if (driver->BackBuffer != NULL) { driver->BackBuffer->Release(); driver->BackBuffer = NULL; }
        if (driver->SwapChain  != NULL) { driver->SwapChain ->Release(); driver->SwapChain  = NULL; }
        if (driver->D2DContext != NULL) { driver->D2DContext->Release(); driver->D2DContext = NULL; }
        if (driver->D2DDevice  != NULL) { driver->D2DDevice ->Release(); driver->D2DDevice  = NULL; }
        if (driver->D3DContext != NULL) { driver->D3DContext->Release(); driver->D3DContext = NULL; }
        if (driver->D3DDevice  != NULL) { driver->D3DDevice ->Release(); driver->D3DDevice  = NULL; }
        if (driver->DWFactory  != NULL) { driver->DWFactory ->Release(); driver->DWFactory  = NULL; }
        if (driver->D2DFactory != NULL) { driver->D2DFactory->Release(); driver->D2DFactory = NULL; }
        free(driver);
    }
    return (uintptr_t)0;
}

/// @summary Performs an explicit reinitialization of the display driver used to render 
/// content into a given window. This function is primarily used internally to handle 
/// detected device removal conditions, but may be called by the application as well.
/// @param drv The display driver handle returned by PrDisplayDriverOpen().
void __cdecl PrDisplayDriverReset(uintptr_t drv)
{   // this is essentially the contents of PrDisplayDriverOpen copied and pasted.
    // there are some minor differences in that we don't allocate storage for the
    // driver state, and don't accept the window handle, but it's otherwise the same.
    present_driver_d2d_t *driver = (present_driver_d2d_t*) drv;
    if (driver == NULL)   return;

    // all device interfaces need to be deleted and re-created. 
    // possible reasons for this include:
    //   * A display driver upgrade was performed.
    //   * The display adapter was reset after a hang.
    //   * The window attached to the swap chain was moved to a different monitor.
    // the only interfaces that are kept are the D2DFactory and DWFactory interfaces.
    if (driver->BackBuffer != NULL) { driver->BackBuffer->Release(); driver->BackBuffer = NULL; }
    if (driver->SwapChain  != NULL) { driver->SwapChain ->Release(); driver->SwapChain  = NULL; }
    if (driver->D2DContext != NULL) { driver->D2DContext->Release(); driver->D2DContext = NULL; }
    if (driver->D2DDevice  != NULL) { driver->D2DDevice ->Release(); driver->D2DDevice  = NULL; }
    if (driver->D3DContext != NULL) { driver->D3DContext->Release(); driver->D3DContext = NULL; }
    if (driver->D3DDevice  != NULL) { driver->D3DDevice ->Release(); driver->D3DDevice  = NULL; }

        // now begin the process of recreating all interfaces and resources.
    UINT flags  = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_SINGLETHREADED;
    DXGI_SWAP_CHAIN_DESC1   swap_desc    = {};
    D2D1_BITMAP_PROPERTIES1 bitmap_desc  = {};
    D3D_FEATURE_LEVEL       got_level    =(D3D_FEATURE_LEVEL) 0;
    D3D_FEATURE_LEVEL       d3d_level[]  = {
        D3D_FEATURE_LEVEL_11_1, 
        D3D_FEATURE_LEVEL_11_0, 
        D3D_FEATURE_LEVEL_10_1, 
        D3D_FEATURE_LEVEL_10_0, 
        D3D_FEATURE_LEVEL_9_3, 
        D3D_FEATURE_LEVEL_9_2, 
        D3D_FEATURE_LEVEL_9_1
    };
    UINT                    level_count  =(UINT)(sizeof(d3d_level) / sizeof(d3d_level[0]));
    bool                    warp_device  = false;
    IDXGIDevice            *dxgi_device  = NULL; // temporary, used to create Direct2D device.
    IDXGIAdapter           *dxgi_adapter = NULL; // temporary, used to retrieve DXGI factory.
    IDXGIFactory2          *dxgi_factory = NULL; // temporary, used to create swap chain.
    IDXGISurface           *back_buffer  = NULL; // temporary, used to create back buffer render target.
    ID3D11Device           *d3d_device   = NULL; // temporary, used for retrieving 11.1 device.
    ID3D11DeviceContext    *d3d_context  = NULL; // temporary, used for retrieving 11.1 context.

    // now create the Direct3D device. Direct3D is used internally by Direct2D.
    // if a hardware-accelerated device isn't available, fallback to using WARP.
    // this is (supposedly) still signifcantly faster than using straight GDI.
    if (FAILED(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags, d3d_level, level_count, D3D11_SDK_VERSION, &d3d_device, &got_level, &d3d_context)))
    {   // if we couldn't create a hardware-accelerated device, use WARP.
        if (FAILED(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_WARP, NULL, flags, d3d_level, level_count, D3D11_SDK_VERSION, &d3d_device, &got_level, &d3d_context)))
        {   // no Direct3D devices are available. this renderer cannot be used.
            OutputDebugString(_T("ERROR: No Direct3D 11.0 device is available.\n"));
            goto error_cleanup;
        }
        warp_device = true; // got a WARP device
    }
    else warp_device = false; // got a hardware device

    // use QueryInterface to get the 11.1 interfaces from the 11.0 interfaces.
    if (FAILED(d3d_device->QueryInterface(IID_PPV_ARGS(&driver->D3DDevice))))
    {
        OutputDebugString(_T("ERROR: Unable to retrieve Direct3D 11.1 device.\n"));
        goto error_cleanup;
    }
    if (FAILED(d3d_context->QueryInterface(IID_PPV_ARGS(&driver->D3DContext))))
    {
        OutputDebugString(_T("ERROR: Unable to retrieve Direct3D 11.1 device context.\n"));
        goto error_cleanup;
    }
    // retrieve the DXGI interop device and use it to create the Direct2D device.
    if (FAILED(driver->D3DDevice->QueryInterface(IID_PPV_ARGS(&dxgi_device))))
    {
        OutputDebugString(_T("ERROR: Unable to retrieve DXGI device interface.\n"));
        goto error_cleanup;
    }
    if (FAILED(driver->D2DFactory->CreateDevice(dxgi_device, &driver->D2DDevice)))
    {
        OutputDebugString(_T("ERROR: Unable to create Direct2D device.\n"));
        goto error_cleanup;
    }
    // TODO(rlk): Maybe specify D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS?
    // This distributes geometry generation load across multiple cores in the system.
    if (FAILED(driver->D2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &driver->D2DContext)))
    {
        OutputDebugString(_T("ERROR: Unable to create Direct2D device context.\n"));
        goto error_cleanup;
    }

    // -- begin window-size dependent resources initialization

    // now create a swap chain attached to the supplied window. each swap chain is double-buffered.
    swap_desc.Width            = 0; // will obtain from window
    swap_desc.Height           = 0; // will obtain from window
    swap_desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    swap_desc.Stereo           = FALSE;
    swap_desc.SampleDesc.Count = 1; // Direct2D will handle MSAA
    swap_desc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_desc.BufferCount      = 2; // double buffered
    swap_desc.Scaling          = DXGI_SCALING_STRETCH;
    swap_desc.SwapEffect       = DXGI_SWAP_EFFECT_DISCARD;
    swap_desc.AlphaMode        = DXGI_ALPHA_MODE_IGNORE;
    swap_desc.Flags            = DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE | DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    if (FAILED(dxgi_device->GetAdapter(&dxgi_adapter)))
    {
        OutputDebugString(_T("ERROR: Unable to retrieve DXGI virtual display adapter.\n"));
        goto error_cleanup;
    }
    if (FAILED(dxgi_adapter->GetParent(IID_PPV_ARGS(&dxgi_factory))))
    {
        OutputDebugString(_T("ERROR: Unable to retrieve DXGI interface factory.\n"));
        goto error_cleanup;
    }
    if (FAILED(dxgi_factory->CreateSwapChainForHwnd(driver->D3DDevice, driver->Window, &swap_desc, NULL, NULL, &driver->SwapChain)))
    {
        OutputDebugString(_T("ERROR: Unable to create DXGI swap chain for presentation.\n"));
        goto error_cleanup;
    }
    if (FAILED(driver->SwapChain->GetBuffer(0, IID_PPV_ARGS(&back_buffer))))
    {
        OutputDebugString(_T("ERROR: Unable to retrieve DXGI surface representing back buffer.\n"));
        goto error_cleanup;
    }

    // disable DXGI handling of Alt+Enter; it does it poorly.
    dxgi_factory->MakeWindowAssociation(driver->Window, DXGI_MWA_NO_ALT_ENTER);
    
    // the final steps are to create a Direct2D bitmap attached to the DXGI
    // surface that represents the swap chain back buffer, and set up D2D scaling.
    driver->D2DFactory->GetDesktopDpi(&bitmap_desc.dpiX, &bitmap_desc.dpiY);
    bitmap_desc.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW | D2D1_BITMAP_OPTIONS_GDI_COMPATIBLE;
    bitmap_desc.colorContext  = NULL;
    bitmap_desc.pixelFormat   = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE);
    if (FAILED(driver->D2DContext->CreateBitmapFromDxgiSurface(back_buffer, bitmap_desc, &driver->BackBuffer)))
    {
        OutputDebugString(_T("ERROR: Unable to create render target from DXGI backbuffer.\n"));
        goto error_cleanup;
    }
    driver->D2DContext->SetTarget(driver->BackBuffer);
    driver->D2DContext->SetDpi(bitmap_desc.dpiX, bitmap_desc.dpiY);
    driver->D3DFeature= got_level;
    driver->IsWARP    = warp_device;
    pr_command_queue_clear(&driver->CommandQueue);

    // release references to all temporary interface pointers.
    back_buffer ->Release();
    dxgi_factory->Release();
    dxgi_adapter->Release();
    dxgi_device ->Release();
    d3d_context ->Release(); // maintain the 11.1 context
    d3d_device  ->Release(); // maintain the 11.1 device
    return;

error_cleanup:
    // release references to all temporary interface pointers.
    if (back_buffer  != NULL) back_buffer ->Release();
    if (dxgi_factory != NULL) dxgi_factory->Release();
    if (dxgi_adapter != NULL) dxgi_adapter->Release();
    if (dxgi_device  != NULL) dxgi_device ->Release();
    if (d3d_context  != NULL) d3d_context ->Release();
    if (d3d_device   != NULL) d3d_device  ->Release();
    // release references to any interfaces stored in the driver.
    if (driver->BackBuffer != NULL) { driver->BackBuffer->Release(); driver->BackBuffer = NULL; }
    if (driver->SwapChain  != NULL) { driver->SwapChain ->Release(); driver->SwapChain  = NULL; }
    if (driver->D2DContext != NULL) { driver->D2DContext->Release(); driver->D2DContext = NULL; }
    if (driver->D2DDevice  != NULL) { driver->D2DDevice ->Release(); driver->D2DDevice  = NULL; }
    if (driver->D3DContext != NULL) { driver->D3DContext->Release(); driver->D3DContext = NULL; }
    if (driver->D3DDevice  != NULL) { driver->D3DDevice ->Release(); driver->D3DDevice  = NULL; }
    return;
}

/// @summary Handles the case where the window associated with a display driver is resized.
/// This function should be called in response to a WM_SIZE or WM_DISPLAYCHANGE event.
/// @param drv The display driver handle returned by PrDisplayDriverResize().
void __cdecl PrDisplayDriverResize(uintptr_t drv)
{
    present_driver_d2d_t   *driver       = (present_driver_d2d_t*) drv;
    D2D1_BITMAP_PROPERTIES1 bitmap_desc  = {};
    IDXGISurface           *back_buffer  = NULL; // temporary, used to create back buffer render target.
    
    // wipe out the current render target. if we don't do this, 
    // the Direct2D context will hold on to a reference and the
    // ResizeBuffers operation will fail.
    driver->BackBuffer->Release();
    driver->BackBuffer = NULL;
    driver->D2DContext->SetTarget(NULL);

    // have the swap chain resize its buffers.
    // specify 0 and DXGI_FORMAT_UNKNOWN to preserve the existing attributes.
    if (!SUCCEEDED(driver->SwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE)))
    {   // unable to resize the swap chain buffers. the device might have been removed, 
        // so attempt to re-create the device and all of its resources. the process of 
        // re-creating the device will take care of the creating the swap chain at the 
        // correct size, so in this case we'll return early from the function.
        OutputDebugString(_T("WARNING: Unable to resize swap chain buffers; will re-create device.\n"));
        PrDisplayDriverReset(drv);
        return;
    }

    // re-create the render target attached to the swap chain backbuffer.
    if (FAILED(driver->SwapChain->GetBuffer(0, IID_PPV_ARGS(&back_buffer))))
    {
        OutputDebugString(_T("WARNING: Unable to retrieve DXGI surface representing back buffer on resize; will re-create device.\n"));
        PrDisplayDriverReset(drv);
        return;
    }
    
    driver->D2DFactory->GetDesktopDpi(&bitmap_desc.dpiX, &bitmap_desc.dpiY);
    bitmap_desc.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW | D2D1_BITMAP_OPTIONS_GDI_COMPATIBLE;
    bitmap_desc.colorContext  = NULL;
    bitmap_desc.pixelFormat   = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE);
    if (FAILED(driver->D2DContext->CreateBitmapFromDxgiSurface(back_buffer, bitmap_desc, &driver->BackBuffer)))
    {
        OutputDebugString(_T("ERROR: Unable to create render target from DXGI backbuffer on resize; will re-create device.\n"));
        back_buffer->Release();
        PrDisplayDriverReset(drv);
        return;
    }
    driver->D2DContext->SetTarget(driver->BackBuffer);
    driver->D2DContext->SetDpi(bitmap_desc.dpiX, bitmap_desc.dpiY);

    // release references to all temporary interface pointers.
    back_buffer->Release();
}

/// @summary Copies the current frame to the application window.
/// @param drv The display driver handle returned by PrDisplayDriverOpen().
void __cdecl PrPresentFrameToWindow(uintptr_t drv)
{
    present_driver_d2d_t *driver = (present_driver_d2d_t*) drv;
    if (driver != NULL)
    {   // translate queued commands into driver equivalents.
        driver->D2DContext->BeginDraw();
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
                        pr_color_t   *data = (pr_color_t*) cmd_info->Data;
                        D2D1_COLOR_F  clear_color;
                        clear_color.r = data->Red;
                        clear_color.g = data->Green;
                        clear_color.b = data->Blue;
                        clear_color.a = data->Alpha;
                        driver->D2DContext->Clear(clear_color);
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
        // indicate the end of command submission for this frame.
        driver->D2DContext->EndDraw();
        // synchronize presentation to the vertical blank interval.
        HRESULT res = driver->SwapChain->Present(1, 0);
        switch (res)
        {
        case S_OK:
        case DXGI_STATUS_OCCLUDED:
            return;
        default:
            PrDisplayDriverReset(drv);
            return;
        }
    }
}

/// @summary Allocates a new command list for use by the application. The 
/// application should write display commands to the command list and then 
/// submit the list for processing by the presentation driver.
/// @param drv The display driver handle returned by PrDisplayDriverOpen().
/// @return An empty command list, or NULL if no command lists are available.
pr_command_list_t* __cdecl PrCreateCommandList(uintptr_t drv)
{
    present_driver_d2d_t *driver = (present_driver_d2d_t*) drv;
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
    present_driver_d2d_t *driver = (present_driver_d2d_t*) drv;
    if (driver == NULL)   return;
    HANDLE sync = cmdlist->SyncHandle;
    pr_command_queue_submit(&driver->CommandQueue, cmdlist);
    if (wait) safe_wait(sync, wait_timeout);
}

/// @summary Closes a display driver instance and releases all associated resources.
/// @param drv The display driver handle returned by PrDisplayDriverOpen().
void __cdecl PrDisplayDriverClose(uintptr_t drv)
{
    present_driver_d2d_t *driver = (present_driver_d2d_t*) drv;
    if (driver != NULL)
    {
        if  (driver->BackBuffer != NULL) { driver->BackBuffer->Release(); driver->BackBuffer = NULL; }
        if  (driver->SwapChain  != NULL) { driver->SwapChain ->Release(); driver->SwapChain  = NULL; }
        if  (driver->D2DContext != NULL) { driver->D2DContext->Release(); driver->D2DContext = NULL; }
        if  (driver->D2DDevice  != NULL) { driver->D2DDevice ->Release(); driver->D2DDevice  = NULL; }
        if  (driver->D3DContext != NULL) { driver->D3DContext->Release(); driver->D3DContext = NULL; }
        if  (driver->D3DDevice  != NULL) { driver->D3DDevice ->Release(); driver->D3DDevice  = NULL; }
        if  (driver->DWFactory  != NULL) { driver->DWFactory ->Release(); driver->DWFactory  = NULL; }
        if  (driver->D2DFactory != NULL) { driver->D2DFactory->Release(); driver->D2DFactory = NULL; }
        pr_command_queue_delete(&driver->CommandQueue);
        free(driver);
    }
}

// TODO(rlk):
// The rest of the interface consists of writing variable-size messages into a command queue
// ala Remotery for doing things like displaying bitmaps, drawing text, vector shapes and so on.
// This queue is processed during the PrPresentFrameToWindow() call and should be MPSC safe as 
// only a single thread should ever call PrPresentFrameToWindow(). When a frame is being 
// presented, an atomic command buffer swap should be performed. Note that we'll have some trouble
// synchronizing things this way; end-of-frame should probably trigger the command buffer being 
// queued to the display driver.

// DXGI best practices: https://msdn.microsoft.com/en-us/library/windows/desktop/ee417025(v=vs.85).aspx
// http://stackoverflow.com/questions/25369231/properly-handling-alt-enter-alt-tab-fullscreen-resolution
