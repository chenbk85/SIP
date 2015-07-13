/*/////////////////////////////////////////////////////////////////////////////
/// @summary Implements display enumeration and OpenGL 3.2+ rendering context 
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

/// @summary Defines the maximum number of shader stages. OpenGL 3.2+ has
/// stages GL_VERTEX_SHADER, GL_GEOMETRY_SHADER and GL_FRAGMENT_SHADER.
#ifndef GL_MAX_SHADER_STAGES_32
#define GL_MAX_SHADER_STAGES_32               (3U)
#endif

/// @summary Defines the maximum number of shader stages. OpenGL 4.0+ has
/// stages GL_VERTEX_SHADER, GL_GEOMETRY_SHADER, GL_FRAGMENT_SHADER,
/// GL_TESS_CONTROL_SHADER, and GL_TESS_EVALUATION_SHADER.
#ifndef GL_MAX_SHADER_STAGES_40
#define GL_MAX_SHADER_STAGES_40               (5U)
#endif

/// @summary Defines the maximum number of shader stages. OpenGL 4.3+ has
/// stages GL_VERTEX_SHADER, GL_GEOMETRY_SHADER, GL_FRAGMENT_SHADER,
/// GL_TESS_CONTROL_SHADER, GL_TESS_EVALUATION_SHADER and GL_COMPUTE_SHADER.
#ifndef GL_MAX_SHADER_STAGES_43
#define GL_MAX_SHADER_STAGES_43               (6U)
#endif

/// @summary Macro to convert a byte offset into a pointer.
/// @param x The byte offset value.
/// @return The offset value, as a pointer.
#ifndef GL_BUFFER_OFFSET
#define GL_BUFFER_OFFSET(x)                   ((GLvoid*)(((uint8_t*)NULL)+(x)))
#endif

/// @summary Preprocessor identifier for OpenGL version 3.2 (GLSL 1.50).
#ifndef OPENGL_VERSION_32
#define OPENGL_VERSION_32                     32
#endif

/// @summary Preprocessor identifier for OpenGL version 3.3 (GLSL 3.30).
#ifndef OPENGL_VERSION_33
#define OPENGL_VERSION_33                     33
#endif

/// @summary Preprocessor identifier for OpenGL version 4.0 (GLSL 4.00).
#ifndef OPENGL_VERSION_40
#define OPENGL_VERSION_40                     40
#endif

/// @summary Preprocessor identifier for OpenGL version 4.1 (GLSL 4.10).
#ifndef OPENGL_VERSION_41
#define OPENGL_VERSION_41                     41
#endif

/// @summary Preprocessor identifier for OpenGL version 4.3 (GLSL 4.30).
#ifndef OPENGL_VERSION_43
#define OPENGL_VERSION_43                     43
#endif

/// @summary Define the version of OpenGL to build against.
#ifndef GL_DISPLAY_VERSION
#define GL_DISPLAY_VERSION                    OPENGL_VERSION_32
#endif

/// @summary Define generic names for version-specific constants.
#if     GL_DISPLAY_VERSION == OPENGL_VERSION_32
#define GL_MAX_SHADER_STAGES                  GL_MAX_SHADER_STAGES_32
#define GL_DISPLAY_VERSION_MAJOR              3
#define GL_DISPLAY_VERSION_MINOR              2
#define GL_DISPLAY_VERSION_SUPPORTED          GLEW_VERSION_3_2
#elif   GL_DISPLAY_VERSION == OPENGL_VERSION_33
#define GL_MAX_SHADER_STAGES                  GL_MAX_SHADER_STAGES_32
#define GL_DISPLAY_VERSION_MAJOR              3
#define GL_DISPLAY_VERSION_MINOR              3
#define GL_DISPLAY_VERSION_SUPPORTED          GLEW_VERSION_3_3
#elif   GL_DISPLAY_VERSION == OPENGL_VERSION_40
#define GL_MAX_SHADER_STAGES                  GL_MAX_SHADER_STAGES_40
#define GL_DISPLAY_VERSION_MAJOR              4
#define GL_DISPLAY_VERSION_MINOR              0
#define GL_DISPLAY_VERSION_SUPPORTED          GLEW_VERSION_4_0
#elif   GL_DISPLAY_VERSION == OPENGL_VERSION_41
#define GL_MAX_SHADER_STAGES                  GL_MAX_SHADER_STAGES_40
#define GL_DISPLAY_VERSION_MAJOR              4
#define GL_DISPLAY_VERSION_MINOR              1
#define GL_DISPLAY_VERSION_SUPPORTED          GLEW_VERSION_4_1
#elif   GL_DISPLAY_VERSION == OPENGL_VERSION_43
#define GL_MAX_SHADER_STAGES                  GL_MAX_SHADER_STAGES_43
#define GL_DISPLAY_VERSION_MAJOR              4
#define GL_DISPLAY_VERSION_MINOR              3
#define GL_DISPLAY_VERSION_SUPPORTED          GLEW_VERSION_4_3
#else
#error  No constants defined for target OpenGL version in gldisplay.cc!
#endif

/// @summary Define a special display ordinal value representing none/not found.
static uint32_t const DISPLAY_ORDINAL_NONE    = 0xFFFFFFFFU;

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Defines the data associated with a single OpenGL display device.
struct gl_display_t
{
    DWORD                  Ordinal;           /// The ordinal value of the display.
    DISPLAY_DEVICE         DisplayInfo;       /// Display information returned from EnumDisplayDevices.
    DEVMODE                DisplayMode;       /// The active display mode returned from EnumDisplaySettings.
    HWND                   DisplayHWND;       /// The hidden, full-screen HWND used to create the rendering context.
    HDC                    DisplayDC;         /// The GDI device context of the full-screen window.
    HGLRC                  DisplayRC;         /// The OpenGL rendering context, which can target any window on the display.
    int                    DisplayX;          /// The x-coordinate of the upper-left corner of the display. May be negative.
    int                    DisplayY;          /// The y-coordinate of the upper-left corner of the display. May be negative.
    size_t                 DisplayWidth;      /// The display width, in pixels.
    size_t                 DisplayHeight;     /// The display height, in pixels.
    GLEWContext            GLEW;              /// OpenGL function pointers and GLEW context.
    WGLEWContext           WGLEW;             /// WGL function pointers and GLEW context.
};

/// @summary Represents the list of available attached displays, identified by 
/// ordinal number. 
struct gl_display_list_t
{
    size_t                 DisplayCount;      /// The number of displays attached to the system.
    DWORD                 *DisplayOrdinal;    /// The set of ordinal values for each display.
    gl_display_t          *Displays;          /// The set of display information and rendering contexts.
};

/// @summary Describes an active GLSL attribute.
struct glsl_attribute_desc_t
{
    GLenum                 DataType;          /// The data type, ex. GL_FLOAT.
    GLint                  Location;          /// The assigned location within the program.
    size_t                 DataSize;          /// The size of the attribute data, in bytes.
    size_t                 DataOffset;        /// The offset of the attribute data, in bytes.
    GLsizei                Dimension;         /// The data dimension, for array types.
};

/// @summary Describes an active GLSL sampler.
struct glsl_sampler_desc_t
{
    GLenum                 SamplerType;       /// The sampler type, ex. GL_SAMPLER_2D.
    GLenum                 BindTarget;        /// The texture bind target, ex. GL_TEXTURE_2D.
    GLint                  Location;          /// The assigned location within the program.
    GLint                  ImageUnit;         /// The assigned texture image unit, ex. GL_TEXTURE0.
};

/// @summary Describes an active GLSL uniform.
struct glsl_uniform_desc_t
{
    GLenum                 DataType;          /// The data type, ex. GL_FLOAT.
    GLint                  Location;          /// The assigned location within the program.
    size_t                 DataSize;          /// The size of the uniform data, in bytes.
    size_t                 DataOffset;        /// The offset of the uniform data, in bytes.
    GLsizei                Dimension;         /// The data dimension, for array types.
};

/// @summary Describes a successfully compiled and linked GLSL shader program.
struct glsl_shader_desc_t
{
    size_t                 UniformCount;      /// Number of active uniforms.
    uint32_t              *UniformNames;      /// Hashed names of active uniforms.
    glsl_uniform_desc_t   *Uniforms;          /// Information about active uniforms.
    size_t                 AttributeCount;    /// Number of active inputs.
    uint32_t              *AttributeNames;    /// Hashed names of active inputs.
    glsl_attribute_desc_t *Attributes;        /// Information about active inputs.
    size_t                 SamplerCount;      /// Number of active samplers.
    uint32_t              *SamplerNames;      /// Hashed names of samplers.
    glsl_sampler_desc_t   *Samplers;          /// Information about active samplers.
    void                  *Metadata;          /// Pointer to the raw memory.
};

/// @summary Describes the source code input to the shader compiler and linker.
struct glsl_shader_source_t
{
    #define N              GL_MAX_SHADER_STAGES
    size_t                 StageCount;        /// Number of valid stages.
    GLenum                 StageNames [N];    /// GL_VERTEX_SHADER, etc.
    GLsizei                StringCount[N];    /// Number of strings per-stage.
    char                 **SourceCode [N];    /// NULL-terminated ASCII strings.
    #undef  N
};

/// @summary Describes a single level of an image in a mipmap chain. Essentially the same as dds_level_desc_t, but adds GL-specific type fields.
struct gl_level_desc_t
{
    size_t                 Index;             /// The mipmap level index (0=highest resolution).
    size_t                 Width;             /// The width of the level, in pixels.
    size_t                 Height;            /// The height of the level, in pixels.
    size_t                 Slices;            /// The number of slices in the level.
    size_t                 BytesPerElement;   /// The number of bytes per-channel or pixel.
    size_t                 BytesPerRow;       /// The number of bytes per-row.
    size_t                 BytesPerSlice;     /// The number of bytes per-slice.
    GLenum                 Layout;            /// The OpenGL base format.
    GLenum                 Format;            /// The OpenGL internal format.
    GLenum                 DataType;          /// The OpenGL element data type.
};

/// @summary Describes a transfer of pixel data from the device (GPU) to the host (CPU), using glReadPixels, glGetTexImage or glGetCompressedTexImage.
/// The Target[X/Y/Z] fields can be specified to have the call calculate the offset from the TransferBuffer pointer, or can be set to 0 if the TransferBuffer is pointing to the start of the target data. 
/// Note that for texture images, the entire mip level is transferred, and so the TransferX, TransferY, TransferWidth and TransferHeight fields are ignored. 
/// The fields Target[Width/Height] describe dimensions of the target image, which may be different than the dimensions of the region being transferred. 
/// The Format and DataType fields describe the target image.
struct gl_pixel_transfer_d2h_t
{
    GLenum                 Target;            /// The image source, ex. GL_READ_FRAMEBUFFER or GL_TEXTURE_2D.
    GLenum                 Layout;            /// The desired pixel data layout, ex. GL_BGRA.
    GLenum                 Format;            /// The internal format of the target, ex. GL_RGBA8.
    GLenum                 DataType;          /// The desired pixel data type, ex. GL_UNSIGNED_INT_8_8_8_8_REV.
    GLuint                 PackBuffer;        /// The PBO to use as the pack target, or 0.
    size_t                 SourceIndex;       /// The source mip level or array index, 0 for framebuffer.
    size_t                 TargetX;           /// The upper-left-front corner on the target image.
    size_t                 TargetY;           /// The upper-left-front corner on the target image.
    size_t                 TargetZ;           /// The upper-left-front corner on the target image.
    size_t                 TargetWidth;       /// The width of the full target image, in pixels.
    size_t                 TargetHeight;      /// The height of the full target image, in pixels.
    size_t                 TransferX;         /// The upper-left corner of the region to transfer.
    size_t                 TransferY;         /// The upper-left corner of the region to transfer.
    size_t                 TransferWidth;     /// The width of the region to transfer, in pixels.
    size_t                 TransferHeight;    /// The height of the region to transfer, in pixels.
    void                  *TransferBuffer;    /// Pointer to target image data, or PBO byte offset.
};

/// @summary Describes a transfer of pixel data from the host (CPU) to the device (GPU), using glTexSubImage or glCompressedTexSubImage. 
/// The fields Target[X/Y/Z] indicate where to place the data on the target texture. The Source[X/Y/Z] fields can be specified to have the call calculate the offset from the TransferBuffer pointer, or can be set to 0 if TransferBuffer is pointing to the start of the source data. 
/// The Source[Width/Height] describe the dimensions of the entire source image, which may be different than the dimensions of the region being transferred. 
/// The source of the transfer may be either a Pixel Buffer Object, in which case the UnpackBuffer should be set to the handle of the PBO, and TransferBuffer is an offset, or an application memory buffer, in which case UnpackBuffer should be set to 0 and TransferBuffer should point to the source image data. 
/// A host to device transfer can be used to upload a source image, or a portion thereof, to a texture object on the GPU. 
/// The Format and DataType fields describe the source image, and not the texture object.
struct gl_pixel_transfer_h2d_t
{
    GLenum                 Target;            /// The image target, ex. GL_TEXTURE_2D.
    GLenum                 Format;            /// The internal format of your pixel data, ex. GL_RGBA8.
    GLenum                 DataType;          /// The data type of your pixel data, ex. GL_UNSIGNED_INT_8_8_8_8_REV.
    GLuint                 UnpackBuffer;      /// The PBO to use as the unpack source, or 0.
    size_t                 TargetIndex;       /// The target mip level or array index.
    size_t                 TargetX;           /// The upper-left-front corner on the target texture.
    size_t                 TargetY;           /// The upper-left-front corner on the target texture.
    size_t                 TargetZ;           /// The upper-left-front corner on the target texture.
    size_t                 SourceX;           /// The upper-left-front corner on the source image.
    size_t                 SourceY;           /// The upper-left-front corner on the source image.
    size_t                 SourceZ;           /// The upper-left-front corner on the source image.
    size_t                 SourceWidth;       /// The width of the full source image, in pixels.
    size_t                 SourceHeight;      /// The height of the full source image, in pixels.
    size_t                 TransferWidth;     /// The width of the region to transfer, in pixels.
    size_t                 TransferHeight;    /// The height of the region to transfer, in pixels.
    size_t                 TransferSlices;    /// The number of slices to transfer.
    size_t                 TransferSize;      /// The total number of bytes to transfer.
    void                  *TransferBuffer;    /// Pointer to source image data, or PBO byte offset.
};

/// @summary Describes a buffer used to stream data from the host (CPU) to the device (GPU) using a GPU-asynchronous buffer transfer. 
/// The buffer usage is always GL_STREAM_DRAW, and the target is always GL_PIXEL_UNPACK_BUFFER.
/// The buffer is allocated in memory accessible to both the CPU and the driver.
/// Buffer regions are reserved, data copied into the buffer by the CPU, and then the reservation is committed. 
/// The transfer is then performed using the gl_pixel_transfer_h2d function. 
/// A single pixel streaming buffer can be used to target multiple texture objects.
struct gl_pixel_stream_h2d_t
{
    GLuint                 Pbo;               /// The pixel buffer object used for streaming.
    size_t                 Alignment;         /// The alignment for any buffer reservations.
    size_t                 BufferSize;        /// The size of the transfer buffer, in bytes.
    size_t                 ReserveOffset;     /// The byte offset of the active reservation.
    size_t                 ReserveSize;       /// The size of the active reservation, in bytes.
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
/// @param x The x-coordinate of the upper-left corner of the target display.
/// @param y The y-coordinate of the upper-left corner of the target display.
/// @param w The width of the target display, in pixels.
/// @param h The height of the target display, in pixels.
/// @return true if WGLEW is initialized successfully.
internal_function bool initialize_wglew(gl_display_t *display, int x, int y, int w, int h)
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

/// @summary Clamps a floating-point value into the given range.
/// @param x The value to clamp.
/// @param lower The inclusive lower-bound.
/// @param upper The inclusive upper-bound.
/// @return The value X: lower <= X <= upper.
internal_function inline float gl_clampf(float x, float lower, float upper)
{
    return (x < lower) ? lower : ((x > upper) ? upper : x);
}

/// @summary Converts an RGBA tuple into a packed 32-bit ABGR value.
/// @param rgba The RGBA tuple, with each component in [0, 1].
/// @return The color value packed into a 32-bit unsigned integer.
internal_function inline uint32_t gl_abgr32(float const *rgba)
{
    uint32_t r = (uint32_t) gl_clampf(rgba[0] * 255.0f, 0.0f, 255.0f);
    uint32_t g = (uint32_t) gl_clampf(rgba[1] * 255.0f, 0.0f, 255.0f);
    uint32_t b = (uint32_t) gl_clampf(rgba[2] * 255.0f, 0.0f, 255.0f);
    uint32_t a = (uint32_t) gl_clampf(rgba[3] * 255.0f, 0.0f, 255.0f);
    return ((a << 24) | (b << 16) | (g << 8) | r);
}

/// @summary Converts an RGBA value into a packed 32-bit ABGR value.
/// @param R The red channel value, in [0, 1].
/// @param G The green channel value, in [0, 1].
/// @param B The blue channel value, in [0, 1].
/// @param A The alpha channel value, in [0, 1].
/// @return The color value packed into a 32-bit unsigned integer.
internal_function inline uint32_t gl_abgr32(float R, float G, float B, float A)
{
    uint32_t r = (uint32_t) gl_clampf(R * 255.0f, 0.0f, 255.0f);
    uint32_t g = (uint32_t) gl_clampf(G * 255.0f, 0.0f, 255.0f);
    uint32_t b = (uint32_t) gl_clampf(B * 255.0f, 0.0f, 255.0f);
    uint32_t a = (uint32_t) gl_clampf(A * 255.0f, 0.0f, 255.0f);
    return ((a << 24) | (b << 16) | (g << 8) | r);
}

/// @summary Searches a list of name-value pairs for a named item.
/// @param name_u32 The 32-bit unsigned integer hash of the search query.
/// @param name_list A list of 32-bit unsigned integer name hashes.
/// @param value_list A list of values, ordered such that name_list[i]
/// corresponds to value_list[i].
/// @param count The number of items in the name and value lists.
template <typename T>
internal_function inline T* gl_kv_find(uint32_t name_u32, uint32_t const *name_list, T *value_list, size_t count)
{
    for (size_t i = 0;  i < count; ++i)
    {
        if (name_list[i] == name_u32)
            return &value_list[i];
    }
    return NULL;
}

/// @summary Searches a list of name-value pairs for a named item.
/// @param name_str A NULL-terminated ASCII string specifying the search query.
/// @param name_list A list of 32-bit unsigned integer name hashes.
/// @param value_list A list of values, ordered such that name_list[i]
/// corresponds to value_list[i].
/// @param count The number of items in the name and value lists.
template <typename T>
internal_function inline T* gl_kv_find(char const *name_str, uint32_t const *name_list, T *value_list, size_t count)
{
    uint32_t name_u32 = glsl_shader_name(name_str);
    return gl_kv_find(name_u32, name_list, value_list, count);
}

/// @summary Searches for a vertex attribute definition by name.
/// @param shader The shader program object to query.
/// @param name A NULL-terminated ASCII string vertex attribute identifier.
/// @return The corresponding vertex attribute definition, or NULL.
internal_function inline glsl_attribute_desc_t *glsl_find_attribute(glsl_shader_desc_t *shader, char const *name)
{
    return gl_kv_find(name, shader->AttributeNames, shader->Attributes, shader->AttributeCount);
}

/// @summary Searches for a texture sampler definition by name.
/// @param shader The shader program object to query.
/// @param name A NULL-terminated ASCII string texture sampler identifier.
/// @return The corresponding texture sampler definition, or NULL.
internal_function inline glsl_sampler_desc_t* glsl_find_sampler(glsl_shader_desc_t *shader, char const *name)
{
    return gl_kv_find(name, shader->SamplerNames, shader->Samplers, shader->SamplerCount);
}

/// @summary Searches for a uniform variable definition by name.
/// @param shader The shader program object to query.
/// @param name A NULL-terminated ASCII string uniform variable identifier.
/// @return The corresponding uniform variable definition, or NULL.
internal_function inline glsl_uniform_desc_t* glsl_find_uniform(glsl_shader_desc_t *shader, char const *name)
{
    return gl_kv_find(name, shader->UniformNames, shader->Uniforms, shader->UniformCount);
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
        if(initialize_wglew(display, x, y, w, h) == false)
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
                    WGL_CONTEXT_MAJOR_VERSION_ARB,    GL_DISPLAY_VERSION_MAJOR, 
                    WGL_CONTEXT_MINOR_VERSION_ARB,    GL_DISPLAY_VERSION_MINOR, 
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
                    WGL_CONTEXT_MAJOR_VERSION_ARB,    GL_DISPLAY_VERSION_MAJOR,
                    WGL_CONTEXT_MINOR_VERSION_ARB,    GL_DISPLAY_VERSION_MINOR,
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
        if (GL_DISPLAY_VERSION_SUPPORTED == GL_FALSE)
        {   // the target OpenGL version is not supported by the driver.
            OutputDebugString(_T("ERROR: Driver does not support required OpenGL on display: "));
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

    // retrieve the current bounds of the window.
    GetWindowRect(window, &wnd_rect);

    // check each display to determine which contains the majority of the window area.
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

    // CW_USEDEFAULT x, y => (0, 0)
    // CW_USEDEFAULT w, h => (width, height)
    if (x == CW_USEDEFAULT) x = (int) display->DisplayX;
    if (y == CW_USEDEFAULT) y = (int) display->DisplayY;
    if (w == CW_USEDEFAULT) w = (int) display->DisplayWidth;
    if (h == CW_USEDEFAULT) h = (int) display->DisplayHeight;

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

/// @summary Given an OpenGL data type value, calculates the corresponding size.
/// @param data_type The OpenGL data type value, for example, GL_UNSIGNED_BYTE.
/// @return The size of a single element of the specified type, in bytes.
public_function size_t gl_data_size(GLenum data_type)
{
    switch (data_type)
    {
    case GL_UNSIGNED_BYTE:               return sizeof(GLubyte);
    case GL_FLOAT:                       return sizeof(GLfloat);
    case GL_FLOAT_VEC2:                  return sizeof(GLfloat) *  2;
    case GL_FLOAT_VEC3:                  return sizeof(GLfloat) *  3;
    case GL_FLOAT_VEC4:                  return sizeof(GLfloat) *  4;
    case GL_INT:                         return sizeof(GLint);
    case GL_INT_VEC2:                    return sizeof(GLint)   *  2;
    case GL_INT_VEC3:                    return sizeof(GLint)   *  3;
    case GL_INT_VEC4:                    return sizeof(GLint)   *  4;
    case GL_BOOL:                        return sizeof(GLint);
    case GL_BOOL_VEC2:                   return sizeof(GLint)   *  2;
    case GL_BOOL_VEC3:                   return sizeof(GLint)   *  3;
    case GL_BOOL_VEC4:                   return sizeof(GLint)   *  4;
    case GL_FLOAT_MAT2:                  return sizeof(GLfloat) *  4;
    case GL_FLOAT_MAT3:                  return sizeof(GLfloat) *  9;
    case GL_FLOAT_MAT4:                  return sizeof(GLfloat) * 16;
    case GL_FLOAT_MAT2x3:                return sizeof(GLfloat) *  6;
    case GL_FLOAT_MAT2x4:                return sizeof(GLfloat) *  8;
    case GL_FLOAT_MAT3x2:                return sizeof(GLfloat) *  6;
    case GL_FLOAT_MAT3x4:                return sizeof(GLfloat) * 12;
    case GL_FLOAT_MAT4x2:                return sizeof(GLfloat) *  8;
    case GL_FLOAT_MAT4x3:                return sizeof(GLfloat) * 12;
    case GL_BYTE:                        return sizeof(GLbyte);
    case GL_UNSIGNED_SHORT:              return sizeof(GLushort);
    case GL_SHORT:                       return sizeof(GLshort);
    case GL_UNSIGNED_INT:                return sizeof(GLuint);
    case GL_UNSIGNED_SHORT_5_6_5:        return sizeof(GLushort);
    case GL_UNSIGNED_SHORT_5_6_5_REV:    return sizeof(GLushort);
    case GL_UNSIGNED_SHORT_4_4_4_4:      return sizeof(GLushort);
    case GL_UNSIGNED_SHORT_4_4_4_4_REV:  return sizeof(GLushort);
    case GL_UNSIGNED_SHORT_5_5_5_1:      return sizeof(GLushort);
    case GL_UNSIGNED_SHORT_1_5_5_5_REV:  return sizeof(GLushort);
    case GL_UNSIGNED_INT_8_8_8_8:        return sizeof(GLubyte);
    case GL_UNSIGNED_INT_8_8_8_8_REV:    return sizeof(GLubyte);
    case GL_UNSIGNED_INT_10_10_10_2:     return sizeof(GLuint);
    case GL_UNSIGNED_INT_2_10_10_10_REV: return sizeof(GLuint);
    case GL_UNSIGNED_BYTE_3_3_2:         return sizeof(GLubyte);
    case GL_UNSIGNED_BYTE_2_3_3_REV:     return sizeof(GLubyte);
    default:                             break;
    }
    return 0;
}

/// @summary Given an OpenGL block-compressed internal format identifier, determine the size of each compressed block, in pixels. For non block-compressed formats, the block size is defined to be 1.
/// @param internal_format The OpenGL internal format value.
/// @return The dimension of a block, in pixels.
public_function size_t gl_block_dimension(GLenum internal_format)
{
    switch (internal_format)
    {
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:        return 4;
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:       return 4;
        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:       return 4;
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:       return 4;
        case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:       return 4;
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT: return 4;
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT: return 4;
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT: return 4;
        default:
            break;
    }
    return 1;
}

/// @summary Given an OpenGL block-compressed internal format identifier, determine the size of each compressed block of pixels.
/// @param internal_format The OpenGL internal format value. RGB/RGBA/SRGB and SRGBA S3TC/DXT format identifiers are the only values currently accepted.
/// @return The number of bytes per compressed block of pixels, or zero.
public_function size_t gl_bytes_per_block(GLenum internal_format)
{
    switch (internal_format)
    {
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:        return 8;
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:       return 8;
        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:       return 16;
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:       return 16;
        case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:       return 8;
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT: return 8;
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT: return 16;
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT: return 16;
        default:
            break;
    }
    return 0;
}

/// @summary Given an OpenGL internal format value, calculates the number of
/// bytes per-element (or per-block, for block-compressed formats).
/// @param internal_format The OpenGL internal format, for example, GL_RGBA.
/// @param data_type The OpenGL data type, for example, GL_UNSIGNED_BYTE.
/// @return The number of bytes per element (pixel or block), or zero.
public_function size_t gl_bytes_per_element(GLenum internal_format, GLenum data_type)
{
    switch (internal_format)
    {
        case GL_DEPTH_COMPONENT:
        case GL_DEPTH_STENCIL:
        case GL_RED:
        case GL_R8:
        case GL_R8_SNORM:
        case GL_R16:
        case GL_R16_SNORM:
        case GL_R16F:
        case GL_R32F:
        case GL_R8I:
        case GL_R8UI:
        case GL_R16I:
        case GL_R16UI:
        case GL_R32I:
        case GL_R32UI:
        case GL_R3_G3_B2:
        case GL_RGB4:
        case GL_RGB5:
        case GL_RGB10:
        case GL_RGB12:
        case GL_RGBA2:
        case GL_RGBA4:
        case GL_RGB9_E5:
        case GL_R11F_G11F_B10F:
        case GL_RGB5_A1:
        case GL_RGB10_A2:
        case GL_RGB10_A2UI:
            return (gl_data_size(data_type) * 1);

        case GL_RG:
        case GL_RG8:
        case GL_RG8_SNORM:
        case GL_RG16:
        case GL_RG16_SNORM:
        case GL_RG16F:
        case GL_RG32F:
        case GL_RG8I:
        case GL_RG8UI:
        case GL_RG16I:
        case GL_RG16UI:
        case GL_RG32I:
        case GL_RG32UI:
            return (gl_data_size(data_type) * 2);

        case GL_RGB:
        case GL_RGB8:
        case GL_RGB8_SNORM:
        case GL_RGB16_SNORM:
        case GL_SRGB8:
        case GL_RGB16F:
        case GL_RGB32F:
        case GL_RGB8I:
        case GL_RGB8UI:
        case GL_RGB16I:
        case GL_RGB16UI:
        case GL_RGB32I:
        case GL_RGB32UI:
            return (gl_data_size(data_type) * 3);

        case GL_RGBA:
        case GL_RGBA8:
        case GL_RGBA8_SNORM:
        case GL_SRGB8_ALPHA8:
        case GL_RGBA16F:
        case GL_RGBA32F:
        case GL_RGBA8I:
        case GL_RGBA8UI:
        case GL_RGBA16I:
        case GL_RGBA16UI:
        case GL_RGBA32I:
        case GL_RGBA32UI:
            return (gl_data_size(data_type) * 4);

        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
            return gl_bytes_per_block(internal_format);

        default:
            break;
    }
    return 0;
}

/// @summary Given an OpenGL internal_format value and a width, calculates the number of bytes between rows in a 2D image slice.
/// @param internal_format The OpenGL internal format, for example, GL_RGBA.
/// @param data_type The OpenGL data type, for example, GL_UNSIGNED_BYTE.
/// @param width The row width, in pixels.
/// @param alignment The alignment requirement of the OpenGL implementation, corresponding to the pname of GL_PACK_ALIGNMENT or GL_UNPACK_ALIGNMENT for the glPixelStorei function. The specification default is 4.
/// @return The number of bytes per-row, or zero.
public_function size_t gl_bytes_per_row(GLenum internal_format, GLenum data_type, size_t width, size_t alignment)
{
    if (width == 0)  width = 1;
    switch (internal_format)
    {
        case GL_DEPTH_COMPONENT:
        case GL_DEPTH_STENCIL:
        case GL_RED:
        case GL_R8:
        case GL_R8_SNORM:
        case GL_R16:
        case GL_R16_SNORM:
        case GL_R16F:
        case GL_R32F:
        case GL_R8I:
        case GL_R8UI:
        case GL_R16I:
        case GL_R16UI:
        case GL_R32I:
        case GL_R32UI:
        case GL_R3_G3_B2:
        case GL_RGB4:
        case GL_RGB5:
        case GL_RGB10:
        case GL_RGB12:
        case GL_RGBA2:
        case GL_RGBA4:
        case GL_RGB9_E5:
        case GL_R11F_G11F_B10F:
        case GL_RGB5_A1:
        case GL_RGB10_A2:
        case GL_RGB10_A2UI:
            return align_up(width * gl_data_size(data_type), alignment);

        case GL_RG:
        case GL_RG8:
        case GL_RG8_SNORM:
        case GL_RG16:
        case GL_RG16_SNORM:
        case GL_RG16F:
        case GL_RG32F:
        case GL_RG8I:
        case GL_RG8UI:
        case GL_RG16I:
        case GL_RG16UI:
        case GL_RG32I:
        case GL_RG32UI:
            return align_up(width * gl_data_size(data_type) * 2, alignment);

        case GL_RGB:
        case GL_RGB8:
        case GL_RGB8_SNORM:
        case GL_RGB16_SNORM:
        case GL_SRGB8:
        case GL_RGB16F:
        case GL_RGB32F:
        case GL_RGB8I:
        case GL_RGB8UI:
        case GL_RGB16I:
        case GL_RGB16UI:
        case GL_RGB32I:
        case GL_RGB32UI:
            return align_up(width * gl_data_size(data_type) * 3, alignment);

        case GL_RGBA:
        case GL_RGBA8:
        case GL_RGBA8_SNORM:
        case GL_SRGB8_ALPHA8:
        case GL_RGBA16F:
        case GL_RGBA32F:
        case GL_RGBA8I:
        case GL_RGBA8UI:
        case GL_RGBA16I:
        case GL_RGBA16UI:
        case GL_RGBA32I:
        case GL_RGBA32UI:
            return align_up(width * gl_data_size(data_type) * 4, alignment);

        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
            return align_up(((width + 3) >> 2) * gl_bytes_per_block(internal_format), alignment);

        default:
            break;
    }
    return 0;
}

/// @summary Calculates the size of the buffer required to store an image with the specified attributes.
/// @param internal_format The OpenGL internal format value, for example, GL_RGBA. The most common compressed formats are supported (DXT/S3TC).
/// @param data_type The data type identifier, for example, GL_UNSIGNED_BYTE.
/// @param width The width of the image, in pixels.
/// @param height The height of the image, in pixels.
/// @param alignment The alignment requirement of the OpenGL implementation, corresponding to the pname of GL_PACK_ALIGNMENT or GL_UNPACK_ALIGNMENT for the glPixelStorei function. The specification default is 4.
/// @return The number of bytes required to store the image data.
public_function size_t gl_bytes_per_slice(GLenum internal_format, GLenum data_type, size_t width, size_t height, size_t alignment)
{
    if (width  == 0) width  = 1;
    if (height == 0) height = 1;
    switch (internal_format)
    {
        case GL_DEPTH_COMPONENT:
        case GL_DEPTH_STENCIL:
        case GL_RED:
        case GL_R8:
        case GL_R8_SNORM:
        case GL_R16:
        case GL_R16_SNORM:
        case GL_R16F:
        case GL_R32F:
        case GL_R8I:
        case GL_R8UI:
        case GL_R16I:
        case GL_R16UI:
        case GL_R32I:
        case GL_R32UI:
        case GL_R3_G3_B2:
        case GL_RGB4:
        case GL_RGB5:
        case GL_RGB10:
        case GL_RGB12:
        case GL_RGBA2:
        case GL_RGBA4:
        case GL_RGB9_E5:
        case GL_R11F_G11F_B10F:
        case GL_RGB5_A1:
        case GL_RGB10_A2:
        case GL_RGB10_A2UI:
            return align_up(width * gl_data_size(data_type), alignment) * height;

        case GL_RG:
        case GL_RG8:
        case GL_RG8_SNORM:
        case GL_RG16:
        case GL_RG16_SNORM:
        case GL_RG16F:
        case GL_RG32F:
        case GL_RG8I:
        case GL_RG8UI:
        case GL_RG16I:
        case GL_RG16UI:
        case GL_RG32I:
        case GL_RG32UI:
            return align_up(width * gl_data_size(data_type) * 2, alignment) * height;

        case GL_RGB:
        case GL_RGB8:
        case GL_RGB8_SNORM:
        case GL_RGB16_SNORM:
        case GL_SRGB8:
        case GL_RGB16F:
        case GL_RGB32F:
        case GL_RGB8I:
        case GL_RGB8UI:
        case GL_RGB16I:
        case GL_RGB16UI:
        case GL_RGB32I:
        case GL_RGB32UI:
            return align_up(width * gl_data_size(data_type) * 3, alignment) * height;

        case GL_RGBA:
        case GL_RGBA8:
        case GL_RGBA8_SNORM:
        case GL_SRGB8_ALPHA8:
        case GL_RGBA16F:
        case GL_RGBA32F:
        case GL_RGBA8I:
        case GL_RGBA8UI:
        case GL_RGBA16I:
        case GL_RGBA16UI:
        case GL_RGBA32I:
        case GL_RGBA32UI:
            return align_up(width * gl_data_size(data_type) * 4, alignment) * height;

        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
            // these formats operate on 4x4 blocks of pixels, so if a dimension
            // is not evenly divisible by four, it needs to be rounded up.
            return align_up(((width + 3) >> 2) * gl_bytes_per_block(internal_format), alignment) * ((height + 3) >> 2);

        default:
            break;
    }
    return 0;
}

/// @summary Calculates the dimension of an image (width, height, etc.) rounded up to the next alignment boundary based on the internal format.
/// @summary internal_format The OpenGL internal format value, for example, GL_RGBA. The most common compressed formats are supported (DXT/S3TC).
/// @param dimension The dimension value (width, height, etc.), in pixels.
/// @return The dimension value padded up to the next alignment boundary. The returned value is always specified in pixels.
public_function size_t gl_image_dimension(GLenum internal_format, size_t dimension)
{
    switch (internal_format)
    {
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT:
            // these formats operate on 4x4 blocks of pixels, so if a dimension
            // is not evenly divisible by four, it needs to be rounded up.
            return (((dimension + 3) >> 2) * gl_block_dimension(internal_format));

        default:
            break;
    }
    return dimension;
}

/// @summary Given an OpenGL internal format type value, determines the corresponding pixel layout.
/// @param internal_format The OpenGL internal format value. See the documentation for glTexImage2D(), internalFormat argument.
/// @return The OpenGL base internal format values. See the documentation for glTexImage2D(), format argument.
public_function GLenum gl_pixel_layout(GLenum internal_format)
{
    switch (internal_format)
    {
        case GL_DEPTH_COMPONENT:                     return GL_DEPTH_COMPONENT;
        case GL_DEPTH_STENCIL:                       return GL_DEPTH_STENCIL;
        case GL_RED:                                 return GL_RED;
        case GL_RG:                                  return GL_RG;
        case GL_RGB:                                 return GL_RGB;
        case GL_RGBA:                                return GL_BGRA;
        case GL_R8:                                  return GL_RED;
        case GL_R8_SNORM:                            return GL_RED;
        case GL_R16:                                 return GL_RED;
        case GL_R16_SNORM:                           return GL_RED;
        case GL_RG8:                                 return GL_RG;
        case GL_RG8_SNORM:                           return GL_RG;
        case GL_RG16:                                return GL_RG;
        case GL_RG16_SNORM:                          return GL_RG;
        case GL_R3_G3_B2:                            return GL_RGB;
        case GL_RGB4:                                return GL_RGB;
        case GL_RGB5:                                return GL_RGB;
        case GL_RGB8:                                return GL_RGB;
        case GL_RGB8_SNORM:                          return GL_RGB;
        case GL_RGB10:                               return GL_RGB;
        case GL_RGB12:                               return GL_RGB;
        case GL_RGB16_SNORM:                         return GL_RGB;
        case GL_RGBA2:                               return GL_RGB;
        case GL_RGBA4:                               return GL_RGB;
        case GL_RGB5_A1:                             return GL_RGBA;
        case GL_RGBA8:                               return GL_BGRA;
        case GL_RGBA8_SNORM:                         return GL_BGRA;
        case GL_RGB10_A2:                            return GL_RGBA;
        case GL_RGB10_A2UI:                          return GL_RGBA;
        case GL_RGBA12:                              return GL_RGBA;
        case GL_RGBA16:                              return GL_BGRA;
        case GL_SRGB8:                               return GL_RGB;
        case GL_SRGB8_ALPHA8:                        return GL_BGRA;
        case GL_R16F:                                return GL_RED;
        case GL_RG16F:                               return GL_RG;
        case GL_RGB16F:                              return GL_RGB;
        case GL_RGBA16F:                             return GL_BGRA;
        case GL_R32F:                                return GL_RED;
        case GL_RG32F:                               return GL_RG;
        case GL_RGB32F:                              return GL_RGB;
        case GL_RGBA32F:                             return GL_BGRA;
        case GL_R11F_G11F_B10F:                      return GL_RGB;
        case GL_RGB9_E5:                             return GL_RGB;
        case GL_R8I:                                 return GL_RED;
        case GL_R8UI:                                return GL_RED;
        case GL_R16I:                                return GL_RED;
        case GL_R16UI:                               return GL_RED;
        case GL_R32I:                                return GL_RED;
        case GL_R32UI:                               return GL_RED;
        case GL_RG8I:                                return GL_RG;
        case GL_RG8UI:                               return GL_RG;
        case GL_RG16I:                               return GL_RG;
        case GL_RG16UI:                              return GL_RG;
        case GL_RG32I:                               return GL_RG;
        case GL_RG32UI:                              return GL_RG;
        case GL_RGB8I:                               return GL_RGB;
        case GL_RGB8UI:                              return GL_RGB;
        case GL_RGB16I:                              return GL_RGB;
        case GL_RGB16UI:                             return GL_RGB;
        case GL_RGB32I:                              return GL_RGB;
        case GL_RGB32UI:                             return GL_RGB;
        case GL_RGBA8I:                              return GL_BGRA;
        case GL_RGBA8UI:                             return GL_BGRA;
        case GL_RGBA16I:                             return GL_BGRA;
        case GL_RGBA16UI:                            return GL_BGRA;
        case GL_RGBA32I:                             return GL_BGRA;
        case GL_RGBA32UI:                            return GL_BGRA;
        case GL_COMPRESSED_RED:                      return GL_RED;
        case GL_COMPRESSED_RG:                       return GL_RG;
        case GL_COMPRESSED_RGB:                      return GL_RGB;
        case GL_COMPRESSED_RGBA:                     return GL_RGBA;
        case GL_COMPRESSED_SRGB:                     return GL_RGB;
        case GL_COMPRESSED_SRGB_ALPHA:               return GL_RGBA;
        case GL_COMPRESSED_RED_RGTC1:                return GL_RED;
        case GL_COMPRESSED_SIGNED_RED_RGTC1:         return GL_RED;
        case GL_COMPRESSED_RG_RGTC2:                 return GL_RG;
        case GL_COMPRESSED_SIGNED_RG_RGTC2:          return GL_RG;
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:        return GL_RGB;
        case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:       return GL_RGBA;
        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:       return GL_RGBA;
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:       return GL_RGBA;
        case GL_COMPRESSED_SRGB_S3TC_DXT1_EXT:       return GL_RGB;
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT: return GL_RGBA;
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT: return GL_RGBA;
        case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT: return GL_RGBA;
        default:
            break;
    }
    return GL_NONE;
}

/// @summary Given an OpenGL sampler type value, determines the corresponding texture bind target identifier.
/// @param sampler_type The OpenGL sampler type, for example, GL_SAMPLER_2D.
/// @return The corresponding bind target, for example, GL_TEXTURE_2D.
public_function GLenum gl_texture_target(GLenum sampler_type)
{
    switch (sampler_type)
    {
        case GL_SAMPLER_1D:
        case GL_INT_SAMPLER_1D:
        case GL_UNSIGNED_INT_SAMPLER_1D:
        case GL_SAMPLER_1D_SHADOW:
            return GL_TEXTURE_1D;

        case GL_SAMPLER_2D:
        case GL_INT_SAMPLER_2D:
        case GL_UNSIGNED_INT_SAMPLER_2D:
        case GL_SAMPLER_2D_SHADOW:
            return GL_TEXTURE_2D;

        case GL_SAMPLER_3D:
        case GL_INT_SAMPLER_3D:
        case GL_UNSIGNED_INT_SAMPLER_3D:
            return GL_TEXTURE_3D;

        case GL_SAMPLER_CUBE:
        case GL_INT_SAMPLER_CUBE:
        case GL_UNSIGNED_INT_SAMPLER_CUBE:
        case GL_SAMPLER_CUBE_SHADOW:
            return GL_TEXTURE_CUBE_MAP;

        case GL_SAMPLER_1D_ARRAY:
        case GL_SAMPLER_1D_ARRAY_SHADOW:
        case GL_INT_SAMPLER_1D_ARRAY:
        case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
            return GL_TEXTURE_1D_ARRAY;

        case GL_SAMPLER_2D_ARRAY:
        case GL_SAMPLER_2D_ARRAY_SHADOW:
        case GL_INT_SAMPLER_2D_ARRAY:
        case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
            return GL_TEXTURE_2D_ARRAY;

        case GL_SAMPLER_BUFFER:
        case GL_INT_SAMPLER_BUFFER:
        case GL_UNSIGNED_INT_SAMPLER_BUFFER:
            return GL_TEXTURE_BUFFER;

        case GL_SAMPLER_2D_RECT:
        case GL_SAMPLER_2D_RECT_SHADOW:
        case GL_INT_SAMPLER_2D_RECT:
        case GL_UNSIGNED_INT_SAMPLER_2D_RECT:
            return GL_TEXTURE_RECTANGLE;

        case GL_SAMPLER_2D_MULTISAMPLE:
        case GL_INT_SAMPLER_2D_MULTISAMPLE:
        case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
            return GL_TEXTURE_2D_MULTISAMPLE;

        case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
        case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
        case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
            return GL_TEXTURE_2D_MULTISAMPLE_ARRAY;

        default:
            break;
    }
    return GL_TEXTURE_1D;
}

/// @summary Given a value from the DXGI_FORMAT enumeration, determine the appropriate OpenGL format, base format and data type values. This is useful when loading texture data from a DDS container.
/// @param dxgi A value of the DXGI_FORMAT enumeration (data::dxgi_format_e).
/// @param out_internalformat On return, stores the corresponding OpenGL internal format.
/// @param out_baseformat On return, stores the corresponding OpenGL base format (layout).
/// @param out_datatype On return, stores the corresponding OpenGL data type.
/// @return true if the input format could be mapped to OpenGL.
public_function bool dxgi_format_to_gl(uint32_t dxgi, GLenum &out_internalformat, GLenum &out_format, GLenum &out_datatype)
{
    switch (dxgi)
    {
        case DXGI_FORMAT_UNKNOWN:
        case DXGI_FORMAT_R32G32B32A32_TYPELESS:
        case DXGI_FORMAT_R32G32B32_TYPELESS:
        case DXGI_FORMAT_R16G16B16A16_TYPELESS:
        case DXGI_FORMAT_R32G32_TYPELESS:
        case DXGI_FORMAT_R32G8X24_TYPELESS:
        case DXGI_FORMAT_R10G10B10A2_TYPELESS:
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        case DXGI_FORMAT_R16G16_TYPELESS:
        case DXGI_FORMAT_R32_TYPELESS:
        case DXGI_FORMAT_R24G8_TYPELESS:
        case DXGI_FORMAT_R8G8_TYPELESS:
        case DXGI_FORMAT_R16_TYPELESS:
        case DXGI_FORMAT_R8_TYPELESS:
        case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
        case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        case DXGI_FORMAT_R1_UNORM:
        case DXGI_FORMAT_R8G8_B8G8_UNORM:
        case DXGI_FORMAT_G8R8_G8B8_UNORM:
        case DXGI_FORMAT_BC1_TYPELESS:
        case DXGI_FORMAT_BC2_TYPELESS:
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:
        case DXGI_FORMAT_BC3_TYPELESS:
        case DXGI_FORMAT_BC4_TYPELESS:
        case DXGI_FORMAT_BC4_UNORM:
        case DXGI_FORMAT_BC4_SNORM:
        case DXGI_FORMAT_BC5_TYPELESS:
        case DXGI_FORMAT_BC5_SNORM:
        case DXGI_FORMAT_B8G8R8A8_TYPELESS:
        case DXGI_FORMAT_B8G8R8X8_TYPELESS:
        case DXGI_FORMAT_BC6H_TYPELESS:
        case DXGI_FORMAT_BC7_TYPELESS:
        case DXGI_FORMAT_AYUV:
        case DXGI_FORMAT_Y410:
        case DXGI_FORMAT_Y416:
        case DXGI_FORMAT_NV12:
        case DXGI_FORMAT_P010:
        case DXGI_FORMAT_P016:
        case DXGI_FORMAT_420_OPAQUE:
        case DXGI_FORMAT_YUY2:
        case DXGI_FORMAT_Y210:
        case DXGI_FORMAT_Y216:
        case DXGI_FORMAT_NV11:
        case DXGI_FORMAT_AI44:
        case DXGI_FORMAT_IA44:
            break;
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
            out_internalformat = GL_RGBA32F;
            out_format         = GL_BGRA;
            out_datatype       = GL_FLOAT;
            return true;
        case DXGI_FORMAT_R32G32B32A32_UINT:
            out_internalformat = GL_RGBA32UI;
            out_format         = GL_BGRA_INTEGER;
            out_datatype       = GL_UNSIGNED_INT;
            return true;
        case DXGI_FORMAT_R32G32B32A32_SINT:
            out_internalformat = GL_RGBA32I;
            out_format         = GL_BGRA_INTEGER;
            out_datatype       = GL_INT;
            return true;
        case DXGI_FORMAT_R32G32B32_FLOAT:
            out_internalformat = GL_RGB32F;
            out_format         = GL_BGR;
            out_datatype       = GL_FLOAT;
            return true;
        case DXGI_FORMAT_R32G32B32_UINT:
            out_internalformat = GL_RGB32UI;
            out_format         = GL_BGR_INTEGER;
            out_datatype       = GL_UNSIGNED_INT;
            return true;
        case DXGI_FORMAT_R32G32B32_SINT:
            out_internalformat = GL_RGB32I;
            out_format         = GL_BGR_INTEGER;
            out_datatype       = GL_INT;
            return true;
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
            out_internalformat = GL_RGBA16F;
            out_format         = GL_BGRA;
            out_datatype       = GL_HALF_FLOAT;
            return true;
        case DXGI_FORMAT_R16G16B16A16_UNORM:
            out_internalformat = GL_RGBA16;
            out_format         = GL_BGRA_INTEGER;
            out_datatype       = GL_UNSIGNED_SHORT;
            return true;
        case DXGI_FORMAT_R16G16B16A16_UINT:
            out_internalformat = GL_RGBA16UI;
            out_format         = GL_BGRA_INTEGER;
            out_datatype       = GL_UNSIGNED_SHORT;
            return true;
        case DXGI_FORMAT_R16G16B16A16_SNORM:
            out_internalformat = GL_RGBA16_SNORM;
            out_format         = GL_BGRA_INTEGER;
            out_datatype       = GL_SHORT;
            return true;
        case DXGI_FORMAT_R16G16B16A16_SINT:
            out_internalformat = GL_RGBA16I;
            out_format         = GL_BGRA_INTEGER;
            out_datatype       = GL_SHORT;
            return true;
        case DXGI_FORMAT_R32G32_FLOAT:
            out_internalformat = GL_RG32F;
            out_format         = GL_RG;
            out_datatype       = GL_FLOAT;
            return true;
        case DXGI_FORMAT_R32G32_UINT:
            out_internalformat = GL_RG32UI;
            out_format         = GL_RG;
            out_datatype       = GL_UNSIGNED_INT;
            return true;
        case DXGI_FORMAT_R32G32_SINT:
            out_internalformat = GL_RG32I;
            out_format         = GL_RG;
            out_datatype       = GL_INT;
            return true;
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
            out_internalformat = GL_DEPTH_STENCIL;
            out_format         = GL_DEPTH_STENCIL;
            out_datatype       = GL_FLOAT; // ???
            return true;
        case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
            out_internalformat = GL_RG32F;
            out_format         = GL_RG;
            out_datatype       = GL_FLOAT;
            return true;
        case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
            return true;
        case DXGI_FORMAT_R10G10B10A2_UNORM:
            out_internalformat = GL_RGB10_A2;
            out_format         = GL_BGRA;
            out_datatype       = GL_UNSIGNED_INT_2_10_10_10_REV;
            return true;
        case DXGI_FORMAT_R10G10B10A2_UINT:
            out_internalformat = GL_RGB10_A2UI;
            out_format         = GL_BGRA;
            out_datatype       = GL_UNSIGNED_INT_2_10_10_10_REV;
            return true;
        case DXGI_FORMAT_R11G11B10_FLOAT:
            out_internalformat = GL_R11F_G11F_B10F;
            out_format         = GL_BGR;
            out_datatype       = GL_FLOAT; // ???
            return true;
        case DXGI_FORMAT_R8G8B8A8_UNORM:
            out_internalformat = GL_RGBA8;
            out_format         = GL_BGRA;
            out_datatype       = GL_UNSIGNED_INT_8_8_8_8_REV;
            return true;
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            out_internalformat = GL_SRGB8_ALPHA8;
            out_format         = GL_BGRA;
            out_datatype       = GL_UNSIGNED_INT_8_8_8_8_REV;
            return true;
        case DXGI_FORMAT_R8G8B8A8_UINT:
            out_internalformat = GL_RGBA8UI;
            out_format         = GL_BGRA;
            out_datatype       = GL_UNSIGNED_INT_8_8_8_8_REV;
            return true;
        case DXGI_FORMAT_R8G8B8A8_SNORM:
            out_internalformat = GL_RGBA8_SNORM;
            out_format         = GL_BGRA;
            out_datatype       = GL_UNSIGNED_INT_8_8_8_8_REV;
            return true;
        case DXGI_FORMAT_R8G8B8A8_SINT:
            out_internalformat = GL_RGBA8I;
            out_format         = GL_BGRA;
            out_datatype       = GL_BYTE;
            return true;
        case DXGI_FORMAT_R16G16_FLOAT:
            out_internalformat = GL_RG16F;
            out_format         = GL_RG;
            out_datatype       = GL_HALF_FLOAT;
            return true;
        case DXGI_FORMAT_R16G16_UNORM:
            out_internalformat = GL_RG16;
            out_format         = GL_RG;
            out_datatype       = GL_UNSIGNED_SHORT;
            return true;
        case DXGI_FORMAT_R16G16_UINT:
            out_internalformat = GL_RG16UI;
            out_format         = GL_RG;
            out_datatype       = GL_UNSIGNED_SHORT;
            return true;
        case DXGI_FORMAT_R16G16_SNORM:
            out_internalformat = GL_RG16_SNORM;
            out_format         = GL_RG;
            out_datatype       = GL_SHORT;
            return true;
        case DXGI_FORMAT_R16G16_SINT:
            out_internalformat = GL_RG16I;
            out_format         = GL_RG;
            out_datatype       = GL_SHORT;
            return true;
        case DXGI_FORMAT_D32_FLOAT:
            out_internalformat = GL_DEPTH_COMPONENT;
            out_format         = GL_DEPTH_COMPONENT;
            out_datatype       = GL_FLOAT;
            return true;
        case DXGI_FORMAT_R32_FLOAT:
            out_internalformat = GL_R32F;
            out_format         = GL_RED;
            out_datatype       = GL_FLOAT;
            return true;
        case DXGI_FORMAT_R32_UINT:
            out_internalformat = GL_R32UI;
            out_format         = GL_RED;
            out_datatype       = GL_UNSIGNED_INT;
            return true;
        case DXGI_FORMAT_R32_SINT:
            out_internalformat = GL_R32I;
            out_format         = GL_RED;
            out_datatype       = GL_INT;
            return true;
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
            out_internalformat = GL_DEPTH_STENCIL;
            out_format         = GL_DEPTH_STENCIL;
            out_datatype       = GL_UNSIGNED_INT;
            return true;
        case DXGI_FORMAT_R8G8_UNORM:
            out_internalformat = GL_RG8;
            out_format         = GL_RG;
            out_datatype       = GL_UNSIGNED_BYTE;
            return true;
        case DXGI_FORMAT_R8G8_UINT:
            out_internalformat = GL_RG8UI;
            out_format         = GL_RG;
            out_datatype       = GL_UNSIGNED_BYTE;
            return true;
        case DXGI_FORMAT_R8G8_SNORM:
            out_internalformat = GL_RG8_SNORM;
            out_format         = GL_RG;
            out_datatype       = GL_BYTE;
            return true;
        case DXGI_FORMAT_R8G8_SINT:
            out_internalformat = GL_RG8I;
            out_format         = GL_RG;
            out_datatype       = GL_BYTE;
            return true;
        case DXGI_FORMAT_R16_FLOAT:
            out_internalformat = GL_R16F;
            out_format         = GL_RED;
            out_datatype       = GL_HALF_FLOAT;
            return true;
        case DXGI_FORMAT_D16_UNORM:
            out_internalformat = GL_DEPTH_COMPONENT;
            out_format         = GL_DEPTH_COMPONENT;
            out_datatype       = GL_UNSIGNED_SHORT;
            return true;
        case DXGI_FORMAT_R16_UNORM:
            out_internalformat = GL_R16;
            out_format         = GL_RED;
            out_datatype       = GL_UNSIGNED_SHORT;
            return true;
        case DXGI_FORMAT_R16_UINT:
            out_internalformat = GL_R16UI;
            out_format         = GL_RED;
            out_datatype       = GL_UNSIGNED_SHORT;
            return true;
        case DXGI_FORMAT_R16_SNORM:
            out_internalformat = GL_R16_SNORM;
            out_format         = GL_RED;
            out_datatype       = GL_SHORT;
            return true;
        case DXGI_FORMAT_R16_SINT:
            out_internalformat = GL_R16I;
            out_format         = GL_RED;
            out_datatype       = GL_SHORT;
            return true;
        case DXGI_FORMAT_R8_UNORM:
            out_internalformat = GL_R8;
            out_format         = GL_RED;
            out_datatype       = GL_UNSIGNED_BYTE;
            return true;
        case DXGI_FORMAT_R8_UINT:
            out_internalformat = GL_R8UI;
            out_format         = GL_RED;
            out_datatype       = GL_UNSIGNED_BYTE;
            return true;
        case DXGI_FORMAT_R8_SNORM:
            out_internalformat = GL_R8_SNORM;
            out_format         = GL_RED;
            out_datatype       = GL_BYTE;
            return true;
        case DXGI_FORMAT_R8_SINT:
            out_internalformat = GL_R8I;
            out_format         = GL_RED;
            out_datatype       = GL_BYTE;
            return true;
        case DXGI_FORMAT_A8_UNORM:
            out_internalformat = GL_R8;
            out_format         = GL_RED;
            out_datatype       = GL_UNSIGNED_BYTE;
            return true;
        case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
            out_internalformat = GL_RGB9_E5;
            out_format         = GL_RGB;
            out_datatype       = GL_UNSIGNED_INT;
            return true;
        case DXGI_FORMAT_BC1_UNORM:
            out_internalformat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
            out_format         = GL_RGBA;
            out_datatype       = GL_UNSIGNED_BYTE;
            return true;
        case DXGI_FORMAT_BC1_UNORM_SRGB:
            out_internalformat = 0x8C4D;
            out_format         = GL_RGBA;
            out_datatype       = GL_UNSIGNED_BYTE;
            return true;
        case DXGI_FORMAT_BC3_UNORM:
            out_internalformat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
            out_format         = GL_RGBA;
            out_datatype       = GL_UNSIGNED_BYTE;
            return true;
        case DXGI_FORMAT_BC3_UNORM_SRGB:
            out_internalformat = 0x8C4E;
            out_format         = GL_RGBA;
            out_datatype       = GL_UNSIGNED_BYTE;
            return true;
        case DXGI_FORMAT_BC5_UNORM:
            out_internalformat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
            out_format         = GL_RGBA;
            out_datatype       = GL_UNSIGNED_BYTE;
            return true;
        case DXGI_FORMAT_B5G6R5_UNORM:
            out_internalformat = GL_RGB;
            out_format         = GL_BGR;
            out_datatype       = GL_UNSIGNED_SHORT_5_6_5_REV;
            return true;
        case DXGI_FORMAT_B5G5R5A1_UNORM:
            out_internalformat = GL_RGBA;
            out_format         = GL_BGRA;
            out_datatype       = GL_UNSIGNED_SHORT_1_5_5_5_REV;
            return true;
        case DXGI_FORMAT_B8G8R8A8_UNORM:
            out_internalformat = GL_RGBA8;
            out_format         = GL_BGRA;
            out_datatype       = GL_UNSIGNED_INT_8_8_8_8_REV;
            return true;
        case DXGI_FORMAT_B8G8R8X8_UNORM:
            out_internalformat = GL_RGBA8;
            out_format         = GL_BGR;
            out_datatype       = GL_UNSIGNED_INT_8_8_8_8_REV;
            return true;
        case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
            out_internalformat = GL_RGB10_A2;
            out_format         = GL_BGRA;
            out_datatype       = GL_UNSIGNED_INT_2_10_10_10_REV;
            return true;
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
            out_internalformat = GL_SRGB8_ALPHA8;
            out_format         = GL_BGRA;
            out_datatype       = GL_UNSIGNED_INT_8_8_8_8_REV;
            return true;
        case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
            out_internalformat = GL_SRGB8_ALPHA8;
            out_format         = GL_BGR;
            out_datatype       = GL_UNSIGNED_INT_8_8_8_8_REV;
            return true;
        case DXGI_FORMAT_BC6H_UF16:
            out_internalformat = 0x8E8F;
            out_format         = GL_RGB;
            out_datatype       = GL_UNSIGNED_BYTE;
            return true;
        case DXGI_FORMAT_BC6H_SF16:
            out_internalformat = 0x8E8E;
            out_format         = GL_RGB;
            out_datatype       = GL_UNSIGNED_BYTE;
            return true;
        case DXGI_FORMAT_BC7_UNORM:
            out_internalformat = 0x8E8C;
            out_format         = GL_RGBA;
            out_datatype       = GL_UNSIGNED_BYTE;
            return true;
        case DXGI_FORMAT_BC7_UNORM_SRGB:
            out_internalformat = 0x8E8D;
            out_format         = GL_RGBA;
            out_datatype       = GL_UNSIGNED_BYTE;
            return true;
        case DXGI_FORMAT_P8:
            out_internalformat = GL_R8;
            out_format         = GL_RED;
            out_datatype       = GL_UNSIGNED_BYTE;
            return true;
        case DXGI_FORMAT_A8P8:
            out_internalformat = GL_RG8;
            out_format         = GL_RG;
            out_datatype       = GL_UNSIGNED_BYTE;
            return true;
        case DXGI_FORMAT_B4G4R4A4_UNORM:
            out_internalformat = GL_RGBA4;
            out_format         = GL_BGRA;
            out_datatype       = GL_UNSIGNED_SHORT_4_4_4_4_REV;
            return true;
        default:
            break;
    }
    return false;
}

/// @summary Computes the number of levels in a mipmap chain given the dimensions of the highest resolution level.
/// @param width The width of the highest resolution level, in pixels.
/// @param height The height of the highest resolution level, in pixels.
/// @param slice_count The number of slices of the highest resolution level. For everything except a 3D image, this value should be specified as 1.
/// @param max_levels The maximum number of levels in the mipmap chain. If there is no limit, this value should be specified as 0.
public_function size_t gl_level_count(size_t width, size_t height, size_t slice_count, size_t max_levels)
{
    size_t levels = 0;
    size_t major  = 0;

    // select the largest of (width, height, slice_count):
    major = (width > height)      ? width : height;
    major = (major > slice_count) ? major : slice_count;
    // calculate the number of levels down to 1 in the largest dimension.
    while (major > 0)
    {
        major >>= 1;
        levels += 1;
    }
    if (max_levels == 0) return levels;
    else return (max_levels < levels) ? max_levels : levels;
}

/// @summary Computes the dimension (width, height or number of slices) of a particular level in a mipmap chain given the dimension for the highest resolution image.
/// @param dimension The dimension in the highest resolution image.
/// @param level_index The index of the target mip-level, with index 0 representing the highest resolution image.
/// @return The dimension in the specified mip level.
public_function size_t gl_level_dimension(size_t dimension, size_t level_index)
{
    size_t  l_dimension  = dimension >> level_index;
    return (l_dimension == 0) ? 1 : l_dimension;
}

/// @summary Given basic image attributes, builds a complete description of the levels in a mipmap chain.
/// @param internal_format The OpenGL internal format, for example GL_RGBA. See the documentation for glTexImage2D(), internalFormat argument.
/// @param data_type The OpenGL data type, for example, GL_UNSIGNED_BYTE.
/// @param width The width of the highest resolution image, in pixels.
/// @param height The height of the highest resolution image, in pixels.
/// @param slice_count The number of slices in the highest resolution image. For all image types other than 3D images, specify 1 for this value.
/// @param alignment The alignment requirement of the OpenGL implementation, corresponding to the pname of GL_PACK_ALIGNMENT or GL_UNPACK_ALIGNMENT for the glPixelStorei function. The specification default is 4.
/// @param max_levels The maximum number of levels in the mipmap chain. To describe all possible levels, specify 0 for this value.
/// @param level_desc The array of level descriptors to populate.
public_function void gl_describe_mipmaps(GLenum internal_format, GLenum data_type, size_t width, size_t height, size_t slice_count, size_t alignment, size_t max_levels, gl_level_desc_t *level_desc)
{
    size_t slices      = slice_count;
    GLenum type        = data_type;
    GLenum format      = internal_format;
    GLenum layout      = gl_pixel_layout(format);
    size_t bytes_per_e = gl_bytes_per_element(format, type);
    size_t num_levels  = gl_level_count(width, height, slices, max_levels);
    for (size_t i = 0; i < num_levels; ++i)
    {
        size_t lw = gl_level_dimension(width,  i);
        size_t lh = gl_level_dimension(height, i);
        size_t ls = gl_level_dimension(slices, i);
        level_desc[i].Index           = i;
        level_desc[i].Width           = gl_image_dimension(format, lw);
        level_desc[i].Height          = gl_image_dimension(format, lh);
        level_desc[i].Slices          = ls;
        level_desc[i].BytesPerElement = bytes_per_e;
        level_desc[i].BytesPerRow     = gl_bytes_per_row(format, type, level_desc[i].Width, alignment);
        level_desc[i].BytesPerSlice   = level_desc[i].BytesPerRow * level_desc[i].Height;
        level_desc[i].Layout          = layout;
        level_desc[i].Format          = internal_format;
        level_desc[i].DataType        = data_type;
    }
}

/// @summary Fills a memory buffer with a checkerboard pattern. This is useful for indicating uninitialized textures and for testing. The image internal format is expected to be GL_RGBA, data type GL_UNSIGNED_INT_8_8_8_8_REV, and the data is written using the native system byte ordering (GL_BGRA).
/// @param width The image width, in pixels.
/// @param height The image height, in pixels.
/// @param alpha The value to write to the alpha channel, in [0, 1].
/// @param buffer The buffer to which image data will be written.
public_function void gl_checker_image(size_t width, size_t height, float alpha, void *buffer)
{
    uint8_t  a = (uint8_t)((alpha - 0.5f) * 255.0f);
    uint8_t *p = (uint8_t*) buffer;
    for (size_t i = 0;  i < height; ++i)
    {
        for (size_t j = 0; j < width; ++j)
        {
            uint8_t v = ((((i & 0x8) == 0)) ^ ((j & 0x8) == 0)) ? 1 : 0;
            *p++  = v * 0xFF;
            *p++  = v ? 0x00 : 0xFF;
            *p++  = v * 0xFF;
            *p++  = a;
        }
    }
}

/// @summary Given basic texture attributes, allocates storage for all levels of a texture, such that the texture is said to be complete. This should only be performed once per-texture. After calling this function, the texture object attributes should be considered immutable. Transfer data to the texture using the gl_transfer_pixels_h2d() function. The wrapping modes are always set to GL_CLAMP_TO_EDGE. The caller is responsible for creating and binding the texture object prior to calling this function.
/// @param display The display managing the rendering context.
/// @param target The OpenGL texture target, defining the texture type.
/// @param internal_format The OpenGL internal format, for example GL_RGBA8.
/// @param data_type The OpenGL data type, for example, GL_UNSIGNED_INT_8_8_8_8_REV.
/// @param min_filter The minification filter to use.
/// @param mag_filter The magnification filter to use.
/// @param width The width of the highest resolution image, in pixels.
/// @param height The height of the highest resolution image, in pixels.
/// @param slice_count The number of slices in the highest resolution image. If the @a target argument specifies an array target, this represents the number of items in the texture array. For 3D textures, it represents the number of slices in the image. For all other types, this value must be 1.
/// @param max_levels The maximum number of levels in the mipmap chain. To define all possible levels, specify 0 for this value.
public_function void gl_texture_storage(gl_display_t *display, GLenum target, GLenum internal_format, GLenum data_type, GLenum min_filter, GLenum mag_filter, size_t width, size_t height, size_t slice_count, size_t max_levels)
{
    GLenum layout = gl_pixel_layout(internal_format);

    if (max_levels == 0)
    {
        // specify mipmaps all the way down to 1x1x1.
        max_levels  = gl_level_count(width, height, slice_count, max_levels);
    }

    // make sure that no PBO is bound as the unpack target.
    // we don't want to copy data to the texture now.
    glBindBuffer (GL_PIXEL_UNPACK_BUFFER, 0);

    // specify the maximum number of mipmap levels.
    if (target != GL_TEXTURE_RECTANGLE)
    {
        glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(target, GL_TEXTURE_MAX_LEVEL,  GLint(max_levels - 1));
    }
    else
    {
        // rectangle textures don't support mipmaps.
        glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(target, GL_TEXTURE_MAX_LEVEL,  0);
    }

    // specify the filtering and wrapping modes.
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, min_filter);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, mag_filter);
    glTexParameteri(target, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_WRAP_R,     GL_CLAMP_TO_EDGE);

    // use glTexImage to allocate storage for each mip-level of the texture.
    switch (target)
    {
            case GL_TEXTURE_1D:
                {
                    for (size_t lod = 0; lod < max_levels; ++lod)
                    {
                        size_t lw = gl_level_dimension(width, lod);
                        glTexImage1D(target, GLint(lod), internal_format, GLsizei(lw), 0, layout, data_type, NULL);
                    }
                }
                break;

            case GL_TEXTURE_1D_ARRAY:
                {
                    // 1D array textures specify slice_count for height.
                    for (size_t lod = 0; lod < max_levels; ++lod)
                    {
                        size_t lw = gl_level_dimension(width, lod);
                        glTexImage2D(target, GLint(lod), internal_format, GLsizei(lw), GLsizei(slice_count), 0, layout, data_type, NULL);
                    }
                }
                break;

            case GL_TEXTURE_RECTANGLE:
                {
                    // rectangle textures don't support mipmaps.
                    glTexImage2D(target, 0, internal_format, GLsizei(width), GLsizei(height), 0, layout, data_type, NULL);
                }
                break;

            case GL_TEXTURE_2D:
            case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
            case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
            case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
            case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
            case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
            case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
                {
                    for (size_t lod = 0; lod < max_levels; ++lod)
                    {
                        size_t lw = gl_level_dimension(width,  lod);
                        size_t lh = gl_level_dimension(height, lod);
                        glTexImage2D(target, GLint(lod), internal_format, GLsizei(lw), GLsizei(lh), 0, layout, data_type, NULL);
                    }
                }
                break;

            case GL_TEXTURE_2D_ARRAY:
            case GL_TEXTURE_CUBE_MAP_ARRAY:
                {
                    // 2D array texture specify slice_count as the number of
                    // items; the number of items is not decreased with LOD.
                    for (size_t lod = 0; lod < max_levels; ++lod)
                    {
                        size_t lw = gl_level_dimension(width,  lod);
                        size_t lh = gl_level_dimension(height, lod);
                        glTexImage3D(target, GLint(lod), internal_format, GLsizei(lw), GLsizei(lh), GLsizei(slice_count), 0, layout, data_type, NULL);
                    }
                }
                break;

            case GL_TEXTURE_3D:
                {
                    for (size_t lod = 0; lod < max_levels; ++lod)
                    {
                        size_t lw = gl_level_dimension(width,       lod);
                        size_t lh = gl_level_dimension(height,      lod);
                        size_t ls = gl_level_dimension(slice_count, lod);
                        glTexImage3D(target, GLint(lod), internal_format, GLsizei(lw), GLsizei(lh), GLsizei(ls), 0, layout, data_type, NULL);
                    }
                }
                break;
    }
}

/// @summary Copies pixel data from the device (GPU) to the host (CPU). The pixel data consists of the framebuffer contents, or the contents of a single mip-level of a texture image.
/// @param display The display managing the rendering context.
/// @param transfer An object describing the transfer operation to execute.
public_function void gl_transfer_pixels_d2h(gl_display_t *display, gl_pixel_transfer_d2h_t *transfer)
{
    if (transfer->PackBuffer != 0)
    {
        // select the PBO as the target of the pack operation.
        glBindBuffer(GL_PIXEL_PACK_BUFFER, transfer->PackBuffer);
    }
    else
    {
        // select the client memory as the target of the pack operation.
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    }
    if (transfer->TargetWidth != transfer->TransferWidth)
    {
        // transferring into a sub-rectangle of the image; tell GL how
        // many pixels are in a single row of the target image.
        glPixelStorei(GL_PACK_ROW_LENGTH,   GLint(transfer->TargetWidth));
        glPixelStorei(GL_PACK_IMAGE_HEIGHT, GLint(transfer->TargetHeight));
    }

    // perform the setup necessary to have GL calculate any byte offsets.
    if (transfer->TargetX != 0) glPixelStorei(GL_PACK_SKIP_PIXELS, GLint(transfer->TargetX));
    if (transfer->TargetY != 0) glPixelStorei(GL_PACK_SKIP_ROWS,   GLint(transfer->TargetY));
    if (transfer->TargetZ != 0) glPixelStorei(GL_PACK_SKIP_IMAGES, GLint(transfer->TargetZ));

    if (gl_bytes_per_block(transfer->Format) > 0)
    {
        // the texture image is compressed; use glGetCompressedTexImage.
        switch (transfer->Target)
        {
            case GL_TEXTURE_1D:
            case GL_TEXTURE_2D:
            case GL_TEXTURE_3D:
            case GL_TEXTURE_CUBE_MAP_ARRAY:
            case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
            case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
            case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
            case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
            case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
            case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
                {
                    glGetCompressedTexImage(transfer->Target, GLint(transfer->SourceIndex), transfer->TransferBuffer);
                }
                break;
        }
    }
    else
    {
        // the image is not compressed; read the framebuffer with
        // glReadPixels or the texture image using glGetTexImage.
        switch (transfer->Target)
        {
            case GL_READ_FRAMEBUFFER:
                {
                    // remember, x and y identify the lower-left corner.
                    glReadPixels(GLint(transfer->TransferX), GLint(transfer->TransferY), GLint(transfer->TransferWidth), GLint(transfer->TransferHeight), transfer->Layout, transfer->DataType, transfer->TransferBuffer);
                }
                break;

            case GL_TEXTURE_1D:
            case GL_TEXTURE_2D:
            case GL_TEXTURE_3D:
            case GL_TEXTURE_1D_ARRAY:
            case GL_TEXTURE_2D_ARRAY:
            case GL_TEXTURE_RECTANGLE:
            case GL_TEXTURE_CUBE_MAP_ARRAY:
            case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
            case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
            case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
            case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
            case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
            case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
                {
                    glGetTexImage(transfer->Target, GLint(transfer->SourceIndex), transfer->Layout, transfer->DataType, transfer->TransferBuffer);
                }
                break;
        }
    }

    // restore the pack state values to their defaults.
    if (transfer->PackBuffer  != 0)
        glBindBuffer(GL_PIXEL_PACK_BUFFER,  0);
    if (transfer->TargetWidth != transfer->TransferWidth)
    {
        glPixelStorei(GL_PACK_ROW_LENGTH,   0);
        glPixelStorei(GL_PACK_IMAGE_HEIGHT, 0);
    }
    if (transfer->TargetX != 0) glPixelStorei(GL_PACK_SKIP_PIXELS,  0);
    if (transfer->TargetY != 0) glPixelStorei(GL_PACK_SKIP_ROWS,    0);
    if (transfer->TargetZ != 0) glPixelStorei(GL_PACK_SKIP_IMAGES,  0);
}

/// @summary Allocates a buffer for streaming pixel data from the host (CPU) to the device (GPU). The buffer exists in driver-accessable memory.
/// @param display The display managing the rendering context.
/// @param stream The stream to initialize.
/// @param alignment The alignment required for mapped buffers. Specify zero to use the default alignment.
/// @param size The desired size of the buffer, in bytes.
/// @return true if the stream is initialized successfully.
public_function bool gl_create_pixel_stream_h2d(gl_display_t *display, gl_pixel_stream_h2d_t *stream, size_t alignment, size_t buffer_size)
{
    if (stream != NULL)
    {
        // ensure the alignment is a power-of-two.
        assert((alignment & (alignment-1)) == 0);

        // create the pixel buffer object.
        GLuint pbo = 0;
        glGenBuffers(1, &pbo);
        if (pbo == 0)
        {   // failed to allocate the buffer object.
            stream->Pbo           = 0;
            stream->Alignment     = 0;
            stream->BufferSize    = 0;
            stream->ReserveOffset = 0;
            stream->ReserveSize   = 0;
            return false;
        }
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);

        // determine the reservation alignment.
        GLint   align  = GLint(alignment);
        if (alignment == 0)
        {   // query for the default buffer alignment.
            glGetIntegerv(GL_UNPACK_ALIGNMENT, &align);
        }

        // round the buffer size up to a multiple of the alignment.
        // then allocate storage for the buffer within the driver.
        GLsizei size   = (buffer_size + (align - 1)) & ~(align - 1);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, size, NULL, GL_STREAM_DRAW);

        // fill out the buffer descriptor; we're done:
        stream->Pbo           = pbo;
        stream->Alignment     = size_t(align);
        stream->BufferSize    = size_t(size);
        stream->ReserveOffset = 0;
        stream->ReserveSize   = 0;
        return true;
    }
    else return false;
}

/// @summary Deletes a host-to-device pixel stream.
/// @param display The display managing the rendering context.
/// @param stream The pixel stream to delete.
public_function void gl_delete_pixel_stream_h2d(gl_display_t *display, gl_pixel_stream_h2d_t *stream)
{
    if (stream != NULL)
    {
        if (stream->Pbo != 0)
        {
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            glDeleteBuffers(1, &stream->Pbo);
        }
        stream->Pbo           = 0;
        stream->Alignment     = 0;
        stream->BufferSize    = 0;
        stream->ReserveOffset = 0;
        stream->ReserveSize   = 0;
    }
}

/// @summary Reserves a portion of the pixel streaming buffer for use by the application. Note that only one buffer reservation may be active at any given time.
/// @param display The display managing the rendering context.
/// @param stream The pixel stream to allocate from.
/// @param size The number of bytes to allocate.
/// @return A pointer to the start of the reserved region, or NULL.
public_function void* gl_pixel_stream_h2d_reserve(gl_display_t *display, gl_pixel_stream_h2d_t *stream, size_t amount)
{
    assert(stream->ReserveSize == 0);       // no existing reservation
    assert(amount <= stream->BufferSize);   // requested amount is less than the buffer size

    GLsizei    align  = GLsizei(stream->Alignment);
    GLsizei    size   = (amount + (align - 1)) & ~(align - 1);
    GLintptr   offset = GLintptr(stream->ReserveOffset);
    GLbitfield access = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT;

    if (offset + size > GLintptr(stream->BufferSize))
    {   // orphan the existing buffer and reserve a new buffer.
        access |= GL_MAP_INVALIDATE_BUFFER_BIT;
        offset  = 0;
    }

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, stream->Pbo);
    GLvoid  *b = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, offset, size, access);
    if (b != NULL)
    {   // store a description of the reserved range.
        stream->ReserveOffset = offset;
        stream->ReserveSize   = size;
    }
    return b;
}

/// @summary Commits the reserved region of the buffer, indicating that the application has finished accessing the buffer directly. The reserved portion of the buffer may now be used as a transfer source.
/// @param display The display managing the rendering context.
/// @param stream The stream to commit.
/// @param transfer The transfer object to populate. The UnpackBuffer, TransferSize and TransferBuffer fields will be modified.
/// @return true if the transfer was successfully committed.
public_function bool gl_pixel_stream_h2d_commit(gl_display_t *display, gl_pixel_stream_h2d_t *stream, gl_pixel_transfer_h2d_t *transfer)
{
    if (stream->ReserveSize > 0)
    {
        if (transfer != NULL)
        {
            transfer->UnpackBuffer   = stream->Pbo;
            transfer->TransferSize   = stream->ReserveSize;
            transfer->TransferBuffer = GL_BUFFER_OFFSET(stream->ReserveOffset);
        }
        glBindBuffer (GL_PIXEL_UNPACK_BUFFER, stream->Pbo);
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
        stream->ReserveOffset += stream->ReserveSize;
        stream->ReserveSize    = 0;
        return true;
    }
    else if (transfer != NULL)
    {   // there is no active reservation, so invalidate the transfer structure.
        transfer->UnpackBuffer   = 0;
        transfer->TransferSize   = 0;
        transfer->TransferBuffer = NULL;
        return false;
    }
    else return false;
}

/// @summary Cancels the reserved region of the buffer, indicating that the application has finished accessing the buffer directly. The reserved portion of the buffer will be rolled back, anc cannot be used as a transfer source.
/// @param display The display managing the rendering context.
/// @param stream The stream to roll back.
public_function void gl_pixel_stream_h2d_cancel(gl_display_t *display, gl_pixel_stream_h2d_t *stream)
{
    if (stream->ReserveSize > 0)
    {
        glBindBuffer (GL_PIXEL_UNPACK_BUFFER, stream->Pbo);
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
        stream->ReserveSize = 0;
    }
}

/// @summary Copies pixel data from the host (CPU) to the device (GPU). The pixel data is copied to a single mip-level of a texture image.
/// @param display The display managing the rendering context.
/// @param transfer An object describing the transfer operation to execute.
public_function void gl_transfer_pixels_h2d(gl_display_t *display, gl_pixel_transfer_h2d_t *transfer)
{
    if (transfer->UnpackBuffer != 0)
    {
        // select the PBO as the source of the unpack operation.
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, transfer->UnpackBuffer);
    }
    else
    {
        // select the client memory as the source of the unpack operation.
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }
    if (transfer->SourceWidth != transfer->TransferWidth)
    {
        // transferring a sub-rectangle of the image; tell GL how many
        // pixels are in a single row of the source image.
        glPixelStorei(GL_UNPACK_ROW_LENGTH, GLint(transfer->SourceWidth));
    }
    if (transfer->TransferSlices > 1)
    {
        // transferring an image volume; tell GL how many rows per-slice
        // in the source image, since we may only be transferring a sub-volume.
        glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, GLint(transfer->SourceHeight));
    }

    // perform the setup necessary to have GL calculate any byte offsets.
    if (transfer->SourceX != 0) glPixelStorei(GL_UNPACK_SKIP_PIXELS, GLint(transfer->SourceX));
    if (transfer->SourceY != 0) glPixelStorei(GL_UNPACK_SKIP_ROWS,   GLint(transfer->SourceY));
    if (transfer->SourceZ != 0) glPixelStorei(GL_UNPACK_SKIP_IMAGES, GLint(transfer->SourceZ));

    if (gl_bytes_per_block(transfer->Format) > 0)
    {
        // the image is compressed; use glCompressedTexSubImage to transfer.
        switch (transfer->Target)
        {
            case GL_TEXTURE_1D:
                {
                    glCompressedTexSubImage1D(transfer->Target, GLint(transfer->TargetIndex), GLint(transfer->TargetX), GLsizei(transfer->TransferWidth), transfer->Format, GLsizei(transfer->TransferSize), transfer->TransferBuffer);
                }
                break;

            case GL_TEXTURE_2D:
            case GL_TEXTURE_1D_ARRAY:
            case GL_TEXTURE_RECTANGLE:
            case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
            case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
            case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
            case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
            case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
            case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
                {
                    glCompressedTexSubImage2D(transfer->Target, GLint(transfer->TargetIndex), GLint(transfer->TargetX), GLint(transfer->TargetY), GLsizei(transfer->TransferWidth), GLsizei(transfer->TransferHeight), transfer->Format, GLsizei(transfer->TransferSize), transfer->TransferBuffer);
                }
                break;

            case GL_TEXTURE_3D:
            case GL_TEXTURE_2D_ARRAY:
            case GL_TEXTURE_CUBE_MAP_ARRAY:
                {
                    glCompressedTexSubImage3D(transfer->Target, GLint(transfer->TargetIndex), GLint(transfer->TargetX), GLint(transfer->TargetY), GLint(transfer->TargetZ), GLsizei(transfer->TransferWidth), GLsizei(transfer->TransferHeight), GLsizei(transfer->TransferSlices), transfer->Format, GLsizei(transfer->TransferSize), transfer->TransferBuffer);
                }
                break;
        }
    }
    else
    {
        // the image is not compressed, use glTexSubImage to transfer data.
        switch (transfer->Target)
        {
            case GL_TEXTURE_1D:
                {
                    glTexSubImage1D(transfer->Target, GLint(transfer->TargetIndex), GLint(transfer->TargetX), GLsizei(transfer->TransferWidth), transfer->Format, transfer->DataType, transfer->TransferBuffer);
                }
                break;

            case GL_TEXTURE_2D:
            case GL_TEXTURE_1D_ARRAY:
            case GL_TEXTURE_RECTANGLE:
            case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
            case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
            case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
            case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
            case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
            case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
                {
                    glTexSubImage2D(transfer->Target, GLint(transfer->TargetIndex), GLint(transfer->TargetX), GLint(transfer->TargetY), GLsizei(transfer->TransferWidth), GLsizei(transfer->TransferHeight), transfer->Format, transfer->DataType, transfer->TransferBuffer);
                }
                break;

            case GL_TEXTURE_3D:
            case GL_TEXTURE_2D_ARRAY:
            case GL_TEXTURE_CUBE_MAP_ARRAY:
                {
                    glTexSubImage3D(transfer->Target, GLint(transfer->TargetIndex), GLint(transfer->TargetX), GLint(transfer->TargetY), GLint(transfer->TargetZ), GLsizei(transfer->TransferWidth), GLsizei(transfer->TransferHeight), GLsizei(transfer->TransferSlices), transfer->Format, transfer->DataType, transfer->TransferBuffer);
                }
                break;
        }
    }

    // restore the unpack state values to their defaults.
    if (transfer->UnpackBuffer  != 0)
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER,  0);
    if (transfer->SourceWidth   != transfer->TransferWidth)
        glPixelStorei(GL_UNPACK_ROW_LENGTH,   0);
    if (transfer->TransferSlices > 1)
        glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
    if (transfer->SourceX != 0)
        glPixelStorei(GL_UNPACK_SKIP_PIXELS,  0);
    if (transfer->SourceY != 0)
        glPixelStorei(GL_UNPACK_SKIP_ROWS,    0);
    if (transfer->SourceZ != 0)
        glPixelStorei(GL_UNPACK_SKIP_IMAGES,  0);
}

/// @summary Given an ASCII string name, calculates a 32-bit hash value. This function is used for generating names for shader attributes, uniforms and samplers, allowing for more efficient look-up by name.
/// @param name A NULL-terminated ASCII string identifier.
/// @return A 32-bit unsigned integer hash of the name.
public_function uint32_t glsl_shader_name(char const *name)
{
    #define HAS_NULL_BYTE(x) (((x) - 0x01010101) & (~(x) & 0x80808080))
    #define ROTL32(x, y)     _rotl((x), (y))

    uint32_t hash = 0;
    if (name != NULL)
    {
        // hash the majority of the data in 4-byte chunks.
        while (!HAS_NULL_BYTE(*((uint32_t*)name)))
        {
            hash  = ROTL32(hash, 7) + name[0];
            hash  = ROTL32(hash, 7) + name[1];
            hash  = ROTL32(hash, 7) + name[2];
            hash  = ROTL32(hash, 7) + name[3];
            name += 4;
        }
        // hash the remaining 0-3 bytes.
        while (*name) hash = ROTL32(hash, 7) + *name++;
    }
    #undef HAS_NULL_BYTE
    #undef ROTL32
    return hash;
}

/// @summary Determines whether an identifier would be considered a GLSL built- in value; that is, whether the identifier starts with 'gl_'.
/// @param name A NULL-terminated ASCII string identifier.
/// @return true if @a name starts with 'gl_'.
public_function bool glsl_builtin(char const *name)
{
    char prefix[4] = {'g','l','_','\0'};
    return (strncmp(name, prefix, 3) == 0);
}

/// @summary Creates an OpenGL shader object and compiles shader source code.
/// @param display The display managing the rendering context.
/// @param shader_type The shader type, for example GL_VERTEX_SHADER.
/// @param shader_source An array of NULL-terminated ASCII strings representing the source code of the shader program.
/// @param string_count The number of strings in the @a shader_source array.
/// @param out_shader On return, this address stores the OpenGL shader object.
/// @param out_log_size On return, this address stores the number of bytes in the shader compile log. Retrieve log content with glsl_compile_log().
/// @return true if shader compilation was successful.
public_function bool glsl_compile_shader(gl_display_t *display, GLenum shader_type, char **shader_source, size_t string_count, GLuint *out_shader, size_t *out_log_size)
{
    GLuint  shader   = 0;
    GLsizei log_size = 0;
    GLint   result   = GL_FALSE;

    shader = glCreateShader(shader_type);
    if (0 == shader)
    {
        if (out_shader)   *out_shader   = 0;
        if (out_log_size) *out_log_size = 1;
        return false;
    }
    glShaderSource (shader, GLsizei(string_count), (GLchar const**) shader_source, NULL);
    glCompileShader(shader);
    glGetShaderiv  (shader, GL_COMPILE_STATUS,  &result);
    glGetShaderiv  (shader, GL_INFO_LOG_LENGTH, &log_size);
    glGetError();

    if (out_shader)   *out_shader   = shader;
    if (out_log_size) *out_log_size = log_size + 1;
    return (result == GL_TRUE);
}

/// @summary Retrieves the log for the most recent shader compilation.
/// @param display The display managing the rendering context.
/// @param shader The OpenGL shader object.
/// @param buffer The destination buffer.
/// @param buffer_size The maximum number of bytes to write to @a buffer.
public_function void glsl_copy_compile_log(gl_display_t *display, GLuint shader, char *buffer, size_t buffer_size)
{
    GLsizei len = 0;
    glGetShaderInfoLog(shader, (GLsizei) buffer_size, &len, buffer);
    buffer[len] = '\0';
}

/// @summary Creates an OpenGL program object and attaches, but does not link, associated shader fragments. Prior to linking, vertex attribute and fragment output bindings should be specified by the application using glsl_assign_vertex_attributes() and glsl_assign_fragment_outputs().
/// @param display The display managing the rendering context.
/// @param shader_list The list of shader object to attach.
/// @param shader_count The number of shader objects in the shader list.
/// @param out_program On return, this address stores the program object.
/// @return true if the shader objects were attached successfully.
public_function bool glsl_attach_shaders(gl_display_t *display, GLuint *shader_list, size_t shader_count, GLuint *out_program)
{
    GLuint  program = 0;
    program = glCreateProgram();
    if (0  == program)
    {
        if (out_program) *out_program = 0;
        return false;
    }

    for (size_t i = 0; i < shader_count;  ++i)
    {
        glAttachShader(program, shader_list[i]);
        if (glGetError() != GL_NO_ERROR)
        {
            glDeleteProgram(program);
            if (out_program) *out_program = 0;
            return false;
        }
    }
    if (out_program) *out_program = program;
    return true;
}

/// @summary Sets the mapping of vertex attribute names to zero-based indices of the vertex attributes within the vertex format definition. See the glEnableVertexAttribArray and glVertexAttribPointer documentation. The vertex attribute bindings should be set before linking the shader program.
/// @param display The display managing the rendering context.
/// @param program The OpenGL program object being modified.
/// @param attrib_names An array of NULL-terminated ASCII strings specifying the vertex attribute names.
/// @param attrib_locations An array of zero-based integers specifying the vertex attribute location assignments, such that attrib_names[i] is bound to attrib_locations[i].
/// @param attrib_count The number of elements in the name and location arrays.
/// @return true if the bindings were assigned without error.
public_function bool glsl_assign_vertex_attributes(gl_display_t *display, GLuint program, char const **attrib_names, GLuint *attrib_locations, size_t attrib_count)
{
    bool result   = true;
    for (size_t i = 0; i < attrib_count; ++i)
    {
        glBindAttribLocation(program, attrib_locations[i], attrib_names[i]);
        if (glGetError() != GL_NO_ERROR)
            result = false;
    }
    return result;
}

/// @summary Sets the mapping of fragment shader outputs to draw buffer indices. See the documentation for glBindFragDataLocation and glDrawBuffers for more information. The fragment shader output bindings should be set before linking the shader program.
/// @param display The display managing the rendering context.
/// @param program The OpenGL program object being modified.
/// @param output_names An array of NULL-terminated ASCII strings specifying the fragment shader output names.
/// @param output_locations An array of zero-based integers specifying the draw buffer index assignments, such that output_names[i] is bound to draw buffer output_locations[i].
/// @param output_count The number of elements in the name and location arrays.
/// @return true if the bindings were assigned without error.
public_function bool glsl_assign_fragment_outputs(gl_display_t *display, GLuint program, char const **output_names, GLuint *output_locations, size_t output_count)
{
    bool result   = true;
    for (size_t i = 0; i < output_count; ++i)
    {
        glBindFragDataLocation(program, output_locations[i], output_names[i]);
        if (glGetError() != GL_NO_ERROR)
            result = false;
    }
    return result;
}

/// @summary Links and validates shader fragments and assigns any automatic vertex attribute, fragment shader output and uniform locations.
/// @param display The display managing the rendering context.
/// @param program The OpenGL program object to link.
/// @param out_max_name On return, this address is updated with number of bytes required to store the longest name string of a vertex attribute, uniform, texture sampler or fragment shader output, including the terminating NULL.
/// @param out_log_size On return, this address is updated with the number of bytes in the shader linker log. Retrieve log content with the function glsl_linker_log().
/// @return true if shader linking was successful.
public_function bool glsl_link_program(gl_display_t *display, GLuint program, size_t *out_max_name, size_t *out_log_size)
{
    GLsizei log_size = 0;
    GLint   result   = GL_FALSE;

    glLinkProgram (program);
    glGetProgramiv(program, GL_LINK_STATUS, &result);
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_size);
    if (out_log_size) *out_log_size = log_size;

    if (result == GL_TRUE)
    {
        GLint a_max = 0;
        GLint u_max = 0;
        GLint n_max = 0;
        glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &u_max);
        glGetProgramiv(program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &a_max);
        n_max = u_max > a_max ? u_max : a_max;
        if (out_max_name)  *out_max_name = (size_t)(n_max + 1);
    }
    else if (out_max_name) *out_max_name = 1;
    return  (result == GL_TRUE);
}

/// @summary Retrieves the log for the most recent shader program linking.
/// @param display The display managing the rendering context.
/// @param program The OpenGL program object.
/// @param buffer The destination buffer.
/// @param buffer_size The maximum number of bytes to write to @a buffer.
public_function void glsl_copy_linker_log(gl_display_t *display, GLuint program, char *buffer, size_t buffer_size)
{
    GLsizei len = 0;
    glGetProgramInfoLog(program, (GLsizei) buffer_size, &len, buffer);
    buffer[len] = '\0';
}

/// @summary Allocates memory for a glsl_shader_desc_t structure using the standard C library malloc() function. The various counts can be obtained from the values returned by glsl_reflect_program_counts().
/// @param desc The shader description to initialize.
/// @param num_attribs The number of active attributes for the program.
/// @param num_samplers The number of active texture samplers for the program.
/// @param num_uniforms The number of active uniforms for the program.
/// @return true if memory was allocated successfully.
public_function bool glsl_shader_desc_alloc(glsl_shader_desc_t *desc, size_t num_attribs, size_t num_samplers, size_t num_uniforms)
{
    glsl_attribute_desc_t  *attribs       = NULL;
    glsl_sampler_desc_t    *samplers      = NULL;
    glsl_uniform_desc_t    *uniforms      = NULL;
    uint32_t               *attrib_names  = NULL;
    uint32_t               *sampler_names = NULL;
    uint32_t               *uniform_names = NULL;
    uint8_t                *memory_block  = NULL;
    uint8_t                *memory_ptr    = NULL;
    size_t                  aname_size    = 0;
    size_t                  sname_size    = 0;
    size_t                  uname_size    = 0;
    size_t                  attrib_size   = 0;
    size_t                  sampler_size  = 0;
    size_t                  uniform_size  = 0;
    size_t                  total_size    = 0;

    // calculate the total size of all shader program metadata.
    aname_size    = sizeof(uint32_t)              * num_attribs;
    sname_size    = sizeof(uint32_t)              * num_samplers;
    uname_size    = sizeof(uint32_t)              * num_uniforms;
    attrib_size   = sizeof(glsl_attribute_desc_t) * num_attribs;
    sampler_size  = sizeof(glsl_sampler_desc_t)   * num_samplers;
    uniform_size  = sizeof(glsl_uniform_desc_t)   * num_uniforms;
    total_size    = aname_size  + sname_size      + uname_size +
                    attrib_size + sampler_size    + uniform_size;

    // perform a single large memory allocation for the metadata.
    memory_block  = (uint8_t*) malloc(total_size);
    memory_ptr    = memory_block;
    if (memory_block == NULL)
        return false;

    // assign pointers to various sub-blocks within the larger memory block.
    attrib_names  = (uint32_t*) memory_ptr;
    memory_ptr   +=  aname_size;
    attribs       = (glsl_attribute_desc_t*) memory_ptr;
    memory_ptr   +=  attrib_size;

    sampler_names = (uint32_t*) memory_ptr;
    memory_ptr   +=  sname_size;
    samplers      = (glsl_sampler_desc_t  *) memory_ptr;
    memory_ptr   +=  sampler_size;

    uniform_names = (uint32_t*) memory_ptr;
    memory_ptr   +=  uname_size;
    uniforms      = (glsl_uniform_desc_t  *) memory_ptr;
    memory_ptr   +=  uniform_size;

    // set all of the fields on the shader_desc_t structure.
    desc->UniformCount   = num_uniforms;
    desc->UniformNames   = uniform_names;
    desc->Uniforms       = uniforms;
    desc->AttributeCount = num_attribs;
    desc->AttributeNames = attrib_names;
    desc->Attributes     = attribs;
    desc->SamplerCount   = num_samplers;
    desc->SamplerNames   = sampler_names;
    desc->Samplers       = samplers;
    desc->Metadata       = memory_block;
    return true;
}

/// @summary Releases memory for a glsl_shader_desc_t structure using the standard C library free() function.
/// @param desc The shader description to delete.
public_function void glsl_shader_desc_free(glsl_shader_desc_t *desc)
{
    if (desc->Metadata != NULL)
    {
        free(desc->Metadata);
        desc->Metadata       = NULL;
        desc->UniformCount   = 0;
        desc->UniformNames   = NULL;
        desc->Uniforms       = NULL;
        desc->AttributeCount = 0;
        desc->AttributeNames = NULL;
        desc->Attributes     = NULL;
        desc->SamplerCount   = 0;
        desc->SamplerNames   = NULL;
        desc->Samplers       = NULL;
    }
}

/// @summary Counts the number of active vertex attribues, texture samplers and uniform values defined in a shader program.
/// @param display The display managing the rendering context.
/// @param program The OpenGL program object to query.
/// @param buffer A temporary buffer used to hold attribute and uniform names.
/// @param buffer_size The maximum number of bytes that can be written to the temporary name buffer.
/// @param include_builtins Specify true to include GLSL builtin values in the returned vertex attribute count.
/// @param out_num_attribs On return, this address is updated with the number of active vertex attribute values.
/// @param out_num_samplers On return, this address is updated with the number of active texture sampler values.
/// @param out_num_uniforms On return, this address is updated with the number of active uniform values.
public_function void glsl_reflect_program_counts(gl_display_t *display, GLuint program, char *buffer, size_t buffer_size, bool include_builtins, size_t &out_num_attribs, size_t &out_num_samplers, size_t &out_num_uniforms)
{
    size_t  num_attribs  = 0;
    GLint   attrib_count = 0;
    GLsizei buf_size     = (GLsizei) buffer_size;

    glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &attrib_count);
    for (GLint i = 0; i < attrib_count; ++i)
    {
        GLenum type = GL_FLOAT;
        GLuint idx  = (GLuint) i;
        GLint  len  = 0;
        GLint  sz   = 0;
        glGetActiveAttrib(program, idx, buf_size, &len, &sz, &type, buffer);
        if (glsl_builtin(buffer) && !include_builtins)
            continue;
        num_attribs++;
    }

    size_t num_samplers  = 0;
    size_t num_uniforms  = 0;
    GLint  uniform_count = 0;

    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &uniform_count);
    for (GLint i = 0; i < uniform_count; ++i)
    {
        GLenum type = GL_FLOAT;
        GLuint idx  = (GLuint) i;
        GLint  len  = 0;
        GLint  sz   = 0;
        glGetActiveUniform(program, idx, buf_size, &len, &sz, &type, buffer);
        if (glsl_builtin(buffer) && !include_builtins)
            continue;

        switch (type)
        {
            case GL_SAMPLER_1D:
            case GL_INT_SAMPLER_1D:
            case GL_UNSIGNED_INT_SAMPLER_1D:
            case GL_SAMPLER_1D_SHADOW:
            case GL_SAMPLER_2D:
            case GL_INT_SAMPLER_2D:
            case GL_UNSIGNED_INT_SAMPLER_2D:
            case GL_SAMPLER_2D_SHADOW:
            case GL_SAMPLER_3D:
            case GL_INT_SAMPLER_3D:
            case GL_UNSIGNED_INT_SAMPLER_3D:
            case GL_SAMPLER_CUBE:
            case GL_INT_SAMPLER_CUBE:
            case GL_UNSIGNED_INT_SAMPLER_CUBE:
            case GL_SAMPLER_CUBE_SHADOW:
            case GL_SAMPLER_1D_ARRAY:
            case GL_SAMPLER_1D_ARRAY_SHADOW:
            case GL_INT_SAMPLER_1D_ARRAY:
            case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
            case GL_SAMPLER_2D_ARRAY:
            case GL_SAMPLER_2D_ARRAY_SHADOW:
            case GL_INT_SAMPLER_2D_ARRAY:
            case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
            case GL_SAMPLER_BUFFER:
            case GL_INT_SAMPLER_BUFFER:
            case GL_UNSIGNED_INT_SAMPLER_BUFFER:
            case GL_SAMPLER_2D_RECT:
            case GL_SAMPLER_2D_RECT_SHADOW:
            case GL_INT_SAMPLER_2D_RECT:
            case GL_UNSIGNED_INT_SAMPLER_2D_RECT:
            case GL_SAMPLER_2D_MULTISAMPLE:
            case GL_INT_SAMPLER_2D_MULTISAMPLE:
            case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
            case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
            case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
            case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
                num_samplers++;
                break;

            default:
                num_uniforms++;
                break;
        }
    }

    // set the output values for the caller.
    out_num_attribs  = num_attribs;
    out_num_samplers = num_samplers;
    out_num_uniforms = num_uniforms;
}

/// @summary Retrieves descriptions of active vertex attributes, texture samplers and uniform values defined in a shader program.
/// @param display The display managing the rendering context.
/// @param program The OpenGL program object to query.
/// @param buffer A temporary buffer used to hold attribute and uniform names.
/// @param buffer_size The maximum number of bytes that can be written to the temporary name buffer.
/// @param include_builtins Specify true to include GLSL builtin values in the returned vertex attribute count.
/// @param attrib_names Pointer to an array to be filled with the 32-bit hash values of the active vertex attribute names.
/// @param attrib_info Pointer to an array to be filled with vertex attribute descriptions.
/// @param sampler_names Pointer to an array to be filled with the 32-bit hash values of the active texture sampler names.
/// @param sampler_info Pointer to an array to be filled with texture sampler descriptions.
/// @param uniform_names Pointer to an array to be filled with the 32-bit hash values of the active uniform names.
/// @param uniform_info Pointer to an array to be filled with uniform descriptions.
public_function void glsl_reflect_program_details(gl_display_t *display, GLuint program, char *buffer, size_t buffer_size, bool include_builtins, uint32_t *attrib_names, glsl_attribute_desc_t *attrib_info, uint32_t *sampler_names, glsl_sampler_desc_t *sampler_info, uint32_t *uniform_names, glsl_uniform_desc_t *uniform_info)
{
    size_t  num_attribs  = 0;
    GLint   attrib_count = 0;
    GLsizei buf_size     = (GLsizei) buffer_size;

    glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &attrib_count);
    for (GLint i = 0; i < attrib_count; ++i)
    {
        GLenum type = GL_FLOAT;
        GLuint idx  = (GLuint) i;
        GLint  len  = 0;
        GLint  loc  = 0;
        GLint  sz   = 0;
        glGetActiveAttrib(program, idx, buf_size, &len, &sz, &type, buffer);
        if (glsl_builtin(buffer) && !include_builtins)
            continue;

        glsl_attribute_desc_t va;
        loc           = glGetAttribLocation(program, buffer);
        va.DataType   = (GLenum) type;
        va.Location   = (GLint)  loc;
        va.DataSize   = (size_t) gl_data_size(type) * sz;
        va.DataOffset = (size_t) 0; // for application use only
        va.Dimension  = (size_t) sz;
        attrib_names[num_attribs] = glsl_shader_name(buffer);
        attrib_info [num_attribs] = va;
        num_attribs++;
    }

    size_t num_samplers  = 0;
    size_t num_uniforms  = 0;
    GLint  uniform_count = 0;
    GLint  texture_unit  = 0;

    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &uniform_count);
    for (GLint i = 0; i < uniform_count; ++i)
    {
        GLenum type = GL_FLOAT;
        GLuint idx  = (GLuint) i;
        GLint  len  = 0;
        GLint  loc  = 0;
        GLint  sz   = 0;
        glGetActiveUniform(program, idx, buf_size, &len, &sz, &type, buffer);
        if (glsl_builtin(buffer) && !include_builtins)
            continue;

        switch (type)
        {
            case GL_SAMPLER_1D:
            case GL_INT_SAMPLER_1D:
            case GL_UNSIGNED_INT_SAMPLER_1D:
            case GL_SAMPLER_1D_SHADOW:
            case GL_SAMPLER_2D:
            case GL_INT_SAMPLER_2D:
            case GL_UNSIGNED_INT_SAMPLER_2D:
            case GL_SAMPLER_2D_SHADOW:
            case GL_SAMPLER_3D:
            case GL_INT_SAMPLER_3D:
            case GL_UNSIGNED_INT_SAMPLER_3D:
            case GL_SAMPLER_CUBE:
            case GL_INT_SAMPLER_CUBE:
            case GL_UNSIGNED_INT_SAMPLER_CUBE:
            case GL_SAMPLER_CUBE_SHADOW:
            case GL_SAMPLER_1D_ARRAY:
            case GL_SAMPLER_1D_ARRAY_SHADOW:
            case GL_INT_SAMPLER_1D_ARRAY:
            case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
            case GL_SAMPLER_2D_ARRAY:
            case GL_SAMPLER_2D_ARRAY_SHADOW:
            case GL_INT_SAMPLER_2D_ARRAY:
            case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
            case GL_SAMPLER_BUFFER:
            case GL_INT_SAMPLER_BUFFER:
            case GL_UNSIGNED_INT_SAMPLER_BUFFER:
            case GL_SAMPLER_2D_RECT:
            case GL_SAMPLER_2D_RECT_SHADOW:
            case GL_INT_SAMPLER_2D_RECT:
            case GL_UNSIGNED_INT_SAMPLER_2D_RECT:
            case GL_SAMPLER_2D_MULTISAMPLE:
            case GL_INT_SAMPLER_2D_MULTISAMPLE:
            case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
            case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
            case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
            case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
                {
                    glsl_sampler_desc_t ts;
                    loc            = glGetUniformLocation(program, buffer);
                    ts.SamplerType = (GLenum) type;
                    ts.BindTarget  = (GLenum) gl_texture_target(type);
                    ts.Location    = (GLint)  loc;
                    ts.ImageUnit   = (GLint)  texture_unit++;
                    sampler_names[num_samplers] = glsl_shader_name(buffer);
                    sampler_info [num_samplers] = ts;
                    num_samplers++;
                }
                break;

            default:
                {
                    glsl_uniform_desc_t uv;
                    loc            = glGetUniformLocation(program, buffer);
                    uv.DataType    = (GLenum) type;
                    uv.Location    = (GLint)  loc;
                    uv.DataSize    = (size_t) gl_data_size(type) * sz;
                    uv.DataOffset  = (size_t) 0; // for application use only
                    uv.Dimension   = (size_t) sz;
                    uniform_names[num_uniforms] = glsl_shader_name(buffer);
                    uniform_info [num_uniforms] = uv;
                    num_uniforms++;
                }
                break;
        }
    }
}

/// @summary Binds a texture object to a texture sampler for the currently bound shader program.
/// @param display The display managing the rendering context.
/// @param sampler The description of the sampler to set.
/// @param texture The OpenGL texture object to bind to the sampler.
public_function void glsl_set_sampler(gl_display_t *display, glsl_sampler_desc_t *sampler, GLuint texture)
{
    glActiveTexture(GL_TEXTURE0 + sampler->ImageUnit);
    glBindTexture(sampler->BindTarget, texture);
    glUniform1i(sampler->Location, sampler->ImageUnit);
}

/// @summary Sets a uniform value for the currently bound shader program.
/// @param display The display managing the rendering context.
/// @param uniform The description of the uniform to set.
/// @param value The data to copy to the uniform.
/// @param transpose For matrix values, specify true to transpose the matrix
/// elements before passing them to the shader program.
public_function void glsl_set_uniform(gl_display_t *display, glsl_uniform_desc_t *uniform, void const *value, bool transpose)
{
    GLint          loc = uniform->Location;
    GLsizei        dim = uniform->Dimension;
    GLboolean      tm  = transpose ? GL_TRUE : GL_FALSE;
    GLint const   *id  = (GLint const*)   value;
    GLfloat const *fd  = (GLfloat const*) value;
    switch (uniform->DataType)
    {
        case GL_FLOAT:        glUniform1fv(loc, dim, fd);             break;
        case GL_FLOAT_VEC2:   glUniform2fv(loc, dim, fd);             break;
        case GL_FLOAT_VEC3:   glUniform3fv(loc, dim, fd);             break;
        case GL_FLOAT_VEC4:   glUniform4fv(loc, dim, fd);             break;
        case GL_INT:          glUniform1iv(loc, dim, id);             break;
        case GL_INT_VEC2:     glUniform2iv(loc, dim, id);             break;
        case GL_INT_VEC3:     glUniform3iv(loc, dim, id);             break;
        case GL_INT_VEC4:     glUniform4iv(loc, dim, id);             break;
        case GL_BOOL:         glUniform1iv(loc, dim, id);             break;
        case GL_BOOL_VEC2:    glUniform2iv(loc, dim, id);             break;
        case GL_BOOL_VEC3:    glUniform3iv(loc, dim, id);             break;
        case GL_BOOL_VEC4:    glUniform4iv(loc, dim, id);             break;
        case GL_FLOAT_MAT2:   glUniformMatrix2fv  (loc, dim, tm, fd); break;
        case GL_FLOAT_MAT3:   glUniformMatrix3fv  (loc, dim, tm, fd); break;
        case GL_FLOAT_MAT4:   glUniformMatrix4fv  (loc, dim, tm, fd); break;
        case GL_FLOAT_MAT2x3: glUniformMatrix2x3fv(loc, dim, tm, fd); break;
        case GL_FLOAT_MAT2x4: glUniformMatrix2x4fv(loc, dim, tm, fd); break;
        case GL_FLOAT_MAT3x2: glUniformMatrix3x2fv(loc, dim, tm, fd); break;
        case GL_FLOAT_MAT3x4: glUniformMatrix3x4fv(loc, dim, tm, fd); break;
        case GL_FLOAT_MAT4x2: glUniformMatrix4x2fv(loc, dim, tm, fd); break;
        case GL_FLOAT_MAT4x3: glUniformMatrix4x3fv(loc, dim, tm, fd); break;
        default: break;
    }
}

/// @summary Initializes a shader source code buffer to empty.
/// @param source The source code buffer to clear.
public_function void glsl_shader_source_init(glsl_shader_source_t *source)
{
    source->StageCount = 0;
    for (size_t i = 0; i < GL_MAX_SHADER_STAGES; ++i)
    {
        source->StageNames [i] = 0;
        source->StringCount[i] = 0;
        source->SourceCode [i] = NULL;
    }
}

/// @summary Adds source code for a shader stage to a shader source buffer.
/// @param source The source code buffer to modify.
/// @param shader_stage The shader stage, for example, GL_VERTEX_SHADER.
/// @param source_code An array of NULL-terminated ASCII strings specifying the
/// source code fragments for the specified shader stage.
/// @param string_count The number of strings in the source_code array.
public_function void glsl_shader_source_add(glsl_shader_source_t *source, GLenum shader_stage, char **source_code, size_t string_count)
{
    if (source->StageCount < GL_MAX_SHADER_STAGES)
    {
        source->StageNames [source->StageCount] = shader_stage;
        source->StringCount[source->StageCount] = GLsizei(string_count);
        source->SourceCode [source->StageCount] = source_code;
        source->StageCount++;
    }
}

/// @summary Compiles, links and reflects a shader program.
/// @param source The shader source code buffer.
/// @param shader The shader program object to initialize.
/// @param out_program On return, this address is set to the identifier of the OpenGL shader program object. If an error occurs, this value will be 0.
/// @return true if the build process was successful.
public_function bool glsl_build_shader(gl_display_t *display, glsl_shader_source_t *source, glsl_shader_desc_t *shader, GLuint *out_program)
{
    GLuint shader_list[GL_MAX_SHADER_STAGES];
    GLuint program       = 0;
    char  *name_buffer   = NULL;
    size_t num_attribs   = 0;
    size_t num_samplers  = 0;
    size_t num_uniforms  = 0;
    size_t max_name      = 0;

    for (size_t  i = 0; i < source->StageCount;  ++i)
    {
        size_t  ls = 0;
        GLenum  sn = source->StageNames [i];
        GLsizei fc = source->StringCount[i];
        char  **sc = source->SourceCode [i];
        if (!glsl_compile_shader(display, sn, sc, fc, &shader_list[i], &ls))
            goto error_cleanup;
    }

    if (!glsl_attach_shaders(display, shader_list, source->StageCount, &program))
        goto error_cleanup;

    if (!glsl_link_program(display, program, &max_name, NULL))
        goto error_cleanup;

    // flag each attached shader for deletion when the program is deleted.
    // the shaders are automatically detached when the program is deleted.
    for (size_t i = 0; i < source->StageCount; ++i)
        glDeleteShader(shader_list[i]);

    // figure out how many attributes, samplers and uniforms we have.
    name_buffer = (char*) malloc(max_name);
    glsl_reflect_program_counts(display, program, name_buffer, max_name, false, num_attribs, num_samplers, num_uniforms);

    if (!glsl_shader_desc_alloc(shader, num_attribs, num_samplers, num_uniforms))
        goto error_cleanup;

    // now reflect the shader program to retrieve detailed information
    // about all vertex attributes, texture samplers and uniform variables.
    glsl_reflect_program_details(display, program, name_buffer, max_name, false, shader->AttributeNames, shader->Attributes, shader->SamplerNames, shader->Samplers, shader->UniformNames, shader->Uniforms);
    *out_program = program;
    return true;

error_cleanup:
    for (size_t i = 0; i < source->StageCount; ++i)
    {
        if (shader_list[i] != 0)
        {
            glDeleteShader(shader_list[i]);
        }
    }
    if (shader->Metadata != NULL) glsl_shader_desc_free(shader);
    if (name_buffer  != NULL)     free(name_buffer);
    if (program != 0)             glDeleteProgram(program);
    *out_program = 0;
    return false;
}
