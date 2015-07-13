/*/////////////////////////////////////////////////////////////////////////////
/// @summary Implements display enumeration and OpenGL 3.2 rendering context 
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
#ifndef GLRC_HIDDEN_WNDCLASS_NAME
#define GLRC_HIDDEN_WNDCLASS_NAME       _T("GLRC_Hidden_WndClass")
#endif

/// @summary Defines the maximum number of shader stages. Currently, we have
/// stages GL_VERTEX_SHADER, GL_GEOMETRY_SHADER and GL_FRAGMENT_SHADER.
#ifndef GL_MAX_SHADER_STAGES
#define GL_MAX_SHADER_STAGES            (3U)
#endif

/// @summary Macro to convert a byte offset into a pointer.
/// @param x The byte offset value.
/// @return The offset value, as a pointer.
#ifndef GL_BUFFER_OFFSET
#define GL_BUFFER_OFFSET(x)             ((GLvoid*)(((uint8_t*)NULL)+(x)))
#endif

/// @summary Defines the location of the position and texture attributes within
/// the position-texture-color vertex. These attributes are encoded as a vec4.
#ifndef GL_SPRITE_PTC_LOCATION_PTX
#define GL_SPRITE_PTC_LOCATION_PTX      (0)
#endif

/// @summary Defines the location of the tint color attribute within the vertex.
/// This attribute is encoded as a packed 32-bit RGBA color value.
#ifndef GL_SPRITE_PTC_LOCATION_CLR
#define GL_SPRITE_PTC_LOCATION_CLR      (1)
#endif

/// @summary Define the maximum number of in-flight frames per-driver.
static uint32_t const GLRC_MAX_FRAMES   = 4;

/// @summary The vertex shader source code for rendering solid-colored quads specified in screen-space.
/// Targeting OpenGL 3.2/GLSL 1.50.
static char const *SpriteShaderPTC_CLR_VSS_150 =
    "#version 150\n"
    "uniform mat4 uMSS;\n"
    "in      vec4 aPTX;\n"
    "in      vec4 aCLR;\n"
    "out     vec4 vCLR;\n"
    "void main() {\n"
    "    vCLR = aCLR;\n"
    "    gl_Position = uMSS * vec4(aPTX.x, aPTX.y, 0, 1);\n"
    "}\n";

/// @summary The vertex shader source code for rendering solid-colored quads specified in screen-space.
/// Targeting OpenGL 3.3/GLSL 3.30.
static char const *SpriteShaderPTC_CLR_VSS_330 =
    "#version 330\n"
    "uniform mat4 uMSS;\n"
    "layout (location = 0) in vec4 aPTX;\n"
    "layout (location = 1) in vec4 aCLR;\n"
    "out vec4 vCLR;\n"
    "void main() {\n"
    "    vCLR = aCLR;\n"
    "    gl_Position = uMSS * vec4(aPTX.x, aPTX.y, 0, 1);\n"
    "}\n";

/// @summary The fragment shader source code for rendering solid-colored quads specified in screen-space.
/// Targeting OpenGL 3.2/GLSL 1.50.
static char const *SpriteShaderPTC_CLR_FSS_150 =
    "#version 150\n"
    "in  vec4 vCLR;\n"
    "out vec4 oCLR;\n"
    "void main() {\n"
    "    oCLR = vCLR;\n"
    "}\n";

/// @summary The fragment shader source code for rendering solid-colored quads specified in screen-space.
/// Targeting OpenGL 3.3/GLSL 3.30.
static char const *SpriteShaderPTC_CLR_FSS_330 =
    "#version 330\n"
    "in  vec4 vCLR;\n"
    "out vec4 oCLR;\n"
    "void main() {\n"
    "    oCLR = vCLR;\n"
    "}\n";

/// @summary The vertex shader source code for rendering textured and tinted quads specified in screen-space.
/// Targeting OpenGL 3.2/GLSL 1.50.
static char const *SpriteShaderPTC_TEX_VSS_150 =
    "#version 150\n"
    "uniform mat4 uMSS;\n"
    "in      vec4 aPTX;\n"
    "in      vec4 aCLR;\n"
    "out     vec4 vCLR;\n"
    "out     vec2 vTEX;\n"
    "void main() {\n"
    "    vCLR = aCLR;\n"
    "    vTEX = vec2(aPTX.z, aPTX.w);\n"
    "    gl_Position = uMSS * vec4(aPTX.x, aPTX.y, 0, 1);\n"
    "}\n";

/// @summary The vertex shader source code for rendering textured and tinted quads specified in screen-space.
/// Targeting OpenGL 3.3/GLSL 3.30.
static char const *SpriteShaderPTC_TEX_VSS_330 =
    "#version 330\n"
    "uniform mat4 uMSS;\n"
    "layout (location = 0) in vec4 aPTX;\n"
    "layout (location = 1) in vec4 aCLR;\n"
    "out vec4 vCLR;\n"
    "out vec2 vTEX;\n"
    "void main() {\n"
    "    vCLR = aCLR;\n"
    "    vTEX = vec2(aPTX.z, aPTX.w);\n"
    "    gl_Position = uMSS * vec4(aPTX.x, aPTX.y, 0, 1);\n"
    "}\n";

/// @summary The fragment shader source code for rendering textured and tinted quads specified in screen-space.
/// Targeting OpenGL 3.2/GLSL 1.50.
static char const *SpriteShaderPTC_TEX_FSS_150 =
    "#version 150\n"
    "uniform sampler2D sTEX;\n"
    "in      vec2      vTEX;\n"
    "in      vec4      vCLR;\n"
    "out     vec4      oCLR;\n"
    "void main() {\n"
    "    oCLR = texture(sTEX, vTEX) * vCLR;\n"
    "}\n";

/// @summary The fragment shader source code for rendering textured and tinted quads specified in screen-space.
/// Targeting OpenGL 3.3/GLSL 3.30.
static char const *SpriteShaderPTC_TEX_FSS_330 =
    "#version 330\n"
    "uniform sampler2D sTEX;\n"
    "in      vec2      vTEX;\n"
    "in      vec4      vCLR;\n"
    "out     vec4      oCLR;\n"
    "void main() {\n"
    "    oCLR = texture(sTEX, vTEX) * vCLR;\n"
    "}\n";

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Define the possible states of an in-flight frame.
enum gl_frame_state_e                   : int
{
    GLRC_FRAME_STATE_WAIT_FOR_IMAGES    = 0,   /// The frame is waiting for images to be locked in host memory.
    GLRC_FRAME_STATE_WAIT_FOR_COMPUTE   = 1,   /// The frame is waiting for compute jobs to complete.
    GLRC_FRAME_STATE_WAIT_FOR_DISPLAY   = 2,   /// The frame is waiting for display jobs to complete.
    GLRC_FRAME_STATE_COMPLETE           = 3,   /// The frame has completed.
    GLRC_FRAME_STATE_ERROR              = 4,   /// The frame encountered one or more errors.
};

/// @summary Define bitflags used to specify renderer configuration.
enum gl_renderer_flags_e                : uint32_t
{
    GLRC_RENDER_FLAGS_NONE              = (0 << 0),
    GLRC_RENDER_FLAGS_WINDOW            = (1 << 0),
    GLRC_RENDER_FLAGS_HEADLESS          = (1 << 1),
};

/// @summary Describes an active GLSL attribute.
struct glsl_attribute_desc_t
{
    GLenum                  DataType;          /// The data type, ex. GL_FLOAT.
    GLint                   Location;          /// The assigned location within the program.
    size_t                  DataSize;          /// The size of the attribute data, in bytes.
    size_t                  DataOffset;        /// The offset of the attribute data, in bytes.
    GLsizei                 Dimension;         /// The data dimension, for array types.
};

/// @summary Describes an active GLSL sampler.
struct glsl_sampler_desc_t
{
    GLenum                  SamplerType;       /// The sampler type, ex. GL_SAMPLER_2D.
    GLenum                  BindTarget;        /// The texture bind target, ex. GL_TEXTURE_2D.
    GLint                   Location;          /// The assigned location within the program.
    GLint                   ImageUnit;         /// The assigned texture image unit, ex. GL_TEXTURE0.
};

/// @summary Describes an active GLSL uniform.
struct glsl_uniform_desc_t
{
    GLenum                  DataType;          /// The data type, ex. GL_FLOAT.
    GLint                   Location;          /// The assigned location within the program.
    size_t                  DataSize;          /// The size of the uniform data, in bytes.
    size_t                  DataOffset;        /// The offset of the uniform data, in bytes.
    GLsizei                 Dimension;         /// The data dimension, for array types.
};

/// @summary Describes a successfully compiled and linked GLSL shader program.
struct glsl_shader_desc_t
{
    size_t                  UniformCount;      /// Number of active uniforms.
    uint32_t               *UniformNames;      /// Hashed names of active uniforms.
    glsl_uniform_desc_t    *Uniforms;          /// Information about active uniforms.
    size_t                  AttributeCount;    /// Number of active inputs.
    uint32_t               *AttributeNames;    /// Hashed names of active inputs.
    glsl_attribute_desc_t  *Attributes;        /// Information about active inputs.
    size_t                  SamplerCount;      /// Number of active samplers.
    uint32_t               *SamplerNames;      /// Hashed names of samplers.
    glsl_sampler_desc_t    *Samplers;          /// Information about active samplers.
    void                   *Metadata;          /// Pointer to the raw memory.
};

/// @summary Describes the source code input to the shader compiler and linker.
struct glsl_shader_source_t
{
    #define N               GL_MAX_SHADER_STAGES
    size_t                  StageCount;        /// Number of valid stages.
    GLenum                  StageNames [N];    /// GL_VERTEX_SHADER, etc.
    GLsizei                 StringCount[N];    /// Number of strings per-stage.
    char                  **SourceCode [N];    /// NULL-terminated ASCII strings.
    #undef  N
};

/// @summary Describes a single level of an image in a mipmap chain. Essentially the same as dds_level_desc_t, but adds GL-specific type fields.
struct gl_level_desc_t
{
    size_t                  Index;             /// The mipmap level index (0=highest resolution).
    size_t                  Width;             /// The width of the level, in pixels.
    size_t                  Height;            /// The height of the level, in pixels.
    size_t                  Slices;            /// The number of slices in the level.
    size_t                  BytesPerElement;   /// The number of bytes per-channel or pixel.
    size_t                  BytesPerRow;       /// The number of bytes per-row.
    size_t                  BytesPerSlice;     /// The number of bytes per-slice.
    GLenum                  Layout;            /// The OpenGL base format.
    GLenum                  Format;            /// The OpenGL internal format.
    GLenum                  DataType;          /// The OpenGL element data type.
};

/// @summary Describes a transfer of pixel data from the device (GPU) to the host (CPU), using glReadPixels, glGetTexImage or glGetCompressedTexImage.
/// The Target[X/Y/Z] fields can be specified to have the call calculate the offset from the TransferBuffer pointer, or can be set to 0 if the TransferBuffer is pointing to the start of the target data. 
/// Note that for texture images, the entire mip level is transferred, and so the TransferX, TransferY, TransferWidth and TransferHeight fields are ignored. 
/// The fields Target[Width/Height] describe dimensions of the target image, which may be different than the dimensions of the region being transferred. 
/// The Format and DataType fields describe the target image.
struct gl_pixel_transfer_d2h_t
{
    GLenum                  Target;            /// The image source, ex. GL_READ_FRAMEBUFFER or GL_TEXTURE_2D.
    GLenum                  Layout;            /// The desired pixel data layout, ex. GL_BGRA.
    GLenum                  Format;            /// The internal format of the target, ex. GL_RGBA8.
    GLenum                  DataType;          /// The desired pixel data type, ex. GL_UNSIGNED_INT_8_8_8_8_REV.
    GLuint                  PackBuffer;        /// The PBO to use as the pack target, or 0.
    size_t                  SourceIndex;       /// The source mip level or array index, 0 for framebuffer.
    size_t                  TargetX;           /// The upper-left-front corner on the target image.
    size_t                  TargetY;           /// The upper-left-front corner on the target image.
    size_t                  TargetZ;           /// The upper-left-front corner on the target image.
    size_t                  TargetWidth;       /// The width of the full target image, in pixels.
    size_t                  TargetHeight;      /// The height of the full target image, in pixels.
    size_t                  TransferX;         /// The upper-left corner of the region to transfer.
    size_t                  TransferY;         /// The upper-left corner of the region to transfer.
    size_t                  TransferWidth;     /// The width of the region to transfer, in pixels.
    size_t                  TransferHeight;    /// The height of the region to transfer, in pixels.
    void                   *TransferBuffer;    /// Pointer to target image data, or PBO byte offset.
};

/// @summary Describes a transfer of pixel data from the host (CPU) to the device (GPU), using glTexSubImage or glCompressedTexSubImage. 
/// The fields Target[X/Y/Z] indicate where to place the data on the target texture. The Source[X/Y/Z] fields can be specified to have the call calculate the offset from the TransferBuffer pointer, or can be set to 0 if TransferBuffer is pointing to the start of the source data. 
/// The Source[Width/Height] describe the dimensions of the entire source image, which may be different than the dimensions of the region being transferred. 
/// The source of the transfer may be either a Pixel Buffer Object, in which case the UnpackBuffer should be set to the handle of the PBO, and TransferBuffer is an offset, or an application memory buffer, in which case UnpackBuffer should be set to 0 and TransferBuffer should point to the source image data. 
/// A host to device transfer can be used to upload a source image, or a portion thereof, to a texture object on the GPU. 
/// The Format and DataType fields describe the source image, and not the texture object.
struct gl_pixel_transfer_h2d_t
{
    GLenum                  Target;            /// The image target, ex. GL_TEXTURE_2D.
    GLenum                  Format;            /// The internal format of your pixel data, ex. GL_RGBA8.
    GLenum                  DataType;          /// The data type of your pixel data, ex. GL_UNSIGNED_INT_8_8_8_8_REV.
    GLuint                  UnpackBuffer;      /// The PBO to use as the unpack source, or 0.
    size_t                  TargetIndex;       /// The target mip level or array index.
    size_t                  TargetX;           /// The upper-left-front corner on the target texture.
    size_t                  TargetY;           /// The upper-left-front corner on the target texture.
    size_t                  TargetZ;           /// The upper-left-front corner on the target texture.
    size_t                  SourceX;           /// The upper-left-front corner on the source image.
    size_t                  SourceY;           /// The upper-left-front corner on the source image.
    size_t                  SourceZ;           /// The upper-left-front corner on the source image.
    size_t                  SourceWidth;       /// The width of the full source image, in pixels.
    size_t                  SourceHeight;      /// The height of the full source image, in pixels.
    size_t                  TransferWidth;     /// The width of the region to transfer, in pixels.
    size_t                  TransferHeight;    /// The height of the region to transfer, in pixels.
    size_t                  TransferSlices;    /// The number of slices to transfer.
    size_t                  TransferSize;      /// The total number of bytes to transfer.
    void                   *TransferBuffer;    /// Pointer to source image data, or PBO byte offset.
};

/// @summary Describes a buffer used to stream data from the host (CPU) to the device (GPU) using a GPU-asynchronous buffer transfer. 
/// The buffer usage is always GL_STREAM_DRAW, and the target is always GL_PIXEL_UNPACK_BUFFER.
/// The buffer is allocated in memory accessible to both the CPU and the driver.
/// Buffer regions are reserved, data copied into the buffer by the CPU, and then the reservation is committed. 
/// The transfer is then performed using the gl_pixel_transfer_h2d function. 
/// A single pixel streaming buffer can be used to target multiple texture objects.
struct gl_pixel_stream_h2d_t
{
    GLuint                  Pbo;               /// The pixel buffer object used for streaming.
    size_t                  Alignment;         /// The alignment for any buffer reservations.
    size_t                  BufferSize;        /// The size of the transfer buffer, in bytes.
    size_t                  ReserveOffset;     /// The byte offset of the active reservation.
    size_t                  ReserveSize;       /// The size of the active reservation, in bytes.
};

/// @summary A structure representing a single interleaved sprite vertex in the vertex buffer. The vertex encodes 2D screen space position, texture coordinate, and packed ABGR color values into 20 bytes per-vertex. The GPU expands the vertex into 32 bytes. Tint color is constant per-sprite.
#pragma pack(push, 1)
struct gl_sprite_vertex_ptc_t
{
    float                   XYUV[4];           /// Position.x,y (screen space), Texcoord.u,v.
    uint32_t                TintColor;         /// The ABGR tint color.
};
#pragma pack(pop)

/// @summary A structure representing the data required to describe a single sprite within the application. Each sprite is translated into four vertices and six indices. The application fills out a sprite descriptor and pushes it to the batch for later processing.
struct gl_sprite_t
{
    float                   ScreenX;           /// Screen-space X coordinate of the origin.
    float                   ScreenY;           /// Screen-space Y coordinate of the origin.
    float                   OriginX;           /// Origin X-offset from the upper-left corner.
    float                   OriginY;           /// Origin Y-offset from the upper-left corner.
    float                   ScaleX;            /// The horizontal scale factor.
    float                   ScaleY;            /// The vertical scale factor.
    float                   Orientation;       /// The angle of orientation, in radians.
    uint32_t                TintColor;         /// The ABGR tint color.
    uint32_t                ImageX;            /// Y-offset of the upper-left corner of the source image.
    uint32_t                ImageY;            /// Y-offset of the upper-left corner of the source image.
    uint32_t                ImageWidth;        /// The width of the source image, in pixels.
    uint32_t                ImageHeight;       /// The height of the source image, in pixels.
    uint32_t                TextureWidth;      /// The width of the texture defining the source image.
    uint32_t                TextureHeight;     /// The height of the texture defining the source image.
    uint32_t                LayerDepth;        /// The layer depth of the sprite, increasing into the background.
    uint32_t                RenderState;       /// An application-defined render state identifier.
};

/// @summary A structure storing the data required to represent a sprite within the sprite batch. Sprites are transformed to quads when they are pushed to the sprite batch. Each quad definition is 64 bytes.
struct gl_sprite_quad_t
{
    float                   Source[4];         /// The XYWH rectangle on the source texture.
    float                   Target[4];         /// The XYWH rectangle on the screen.
    float                   Origin[2];         /// The XY origin point of rotation.
    float                   Scale[2];          /// Texture coordinate scale factors.
    float                   Orientation;       /// The angle of orientation, in radians.
    uint32_t                TintColor;         /// The ABGR tint color.
};

/// @summary Data used for sorting buffered quads. Grouped together to improve cache usage by avoiding loading all of the data for a gl_sprite_quad_t.
struct gl_sprite_sort_data_t
{
    uint32_t                LayerDepth;        /// The layer depth of the sprite, increasing into the background.
    uint32_t                RenderState;       /// The render state associated with the sprite.
};

/// @summary A structure storing all of the data required to render sprites using a particular effect. All of the shader state is maintained externally.
struct gl_sprite_effect_t
{
    size_t                  VertexCapacity;    /// The maximum number of vertices we can buffer.
    size_t                  VertexOffset;      /// Current offset (in vertices) in buffer.
    size_t                  VertexSize;        /// Size of one vertex, in bytes.
    size_t                  IndexCapacity;     /// The maximum number of indices we can buffer.
    size_t                  IndexOffset;       /// Current offset (in indices) in buffer.
    size_t                  IndexSize;         /// Size of one index, in bytes.
    uint32_t                CurrentState;      /// The active render state identifier.
    GLuint                  VertexArray;       /// The VAO describing the vertex layout.
    GLuint                  VertexBuffer;      /// Buffer object for dynamic vertex data.
    GLuint                  IndexBuffer;       /// Buffer object for dynamic index data.
    GLboolean               BlendEnabled;      /// GL_TRUE if blending is enabled.
    GLenum                  BlendSourceColor;  /// The source color blend factor.
    GLenum                  BlendSourceAlpha;  /// The source alpha blend factor.
    GLenum                  BlendTargetColor;  /// The destination color blend factor.
    GLenum                  BlendTargetAlpha;  /// The destination alpha blend factor.
    GLenum                  BlendFuncColor;    /// The color channel blend function.
    GLenum                  BlendFuncAlpha;    /// The alpha channel blend function.
    GLfloat                 BlendColor[4];     /// RGBA constant blend color.
    float                   Projection[16];    /// Projection matrix for current viewport
};

/// @summary Signature for a function used to apply render state for an effect prior to rendering any quads. The function should perform operations like setting the active program, calling gl_sprite_effect_bind_buffers() and gl_sprite_effect_apply_blendstate(), and so on.
/// @param display The display managing the rendering context.
/// @param effect The effect being used for rendering.
/// @param context Opaque data passed by the application.
typedef void (*gl_sprite_effect_setup_fn)(gl_display_t *display, gl_sprite_effect_t *effect, void *context);

/// @summary Signature for a function used to apply render state for a quad primitive. The function should perform operations like setting up samplers, uniforms, and so on.
/// @param display The display managing the rendering context.
/// @param effect The effect being used for rendering.
/// @param render_state The application render state identifier.
/// @param context Opaque data passed by the application.
typedef void (*gl_sprite_effect_apply_fn)(gl_display_t *display, gl_sprite_effect_t *effect, uint32_t render_state, void *context);

/// @summary Wraps a set of function pointers used to apply effect-specific state.
struct gl_sprite_effect_apply_t
{
    gl_sprite_effect_setup_fn SetupEffect;     /// Callback used to perform initial setup.
    gl_sprite_effect_apply_fn ApplyState;      /// Callback used to perform state changes.
};

/// @summary A structure for buffering data associated with a set of sprites.
struct gl_sprite_batch_t
{
    size_t                  Count;             /// The number of buffered sprites.
    size_t                  Capacity;          /// The capacity of the various buffers.
    gl_sprite_quad_t       *Quads;             /// Buffer for transformed quad data.
    gl_sprite_sort_data_t  *State;             /// Render state identifiers for each quad.
    uint32_t               *Order;             /// Insertion order values for each quad.
};

/// @summary Maintains the state associated with a default sprite shader
/// accepting position, texture and color attributes as vertex input.
struct gl_sprite_shader_ptc_clr_t
{
    GLuint                  Program;           /// The OpenGL program object ID.
    glsl_shader_desc_t      ShaderDesc;        /// Metadata about the shader program.
    glsl_attribute_desc_t  *AttribPTX;         /// Information about the Position-Texture attribute.
    glsl_attribute_desc_t  *AttribCLR;         /// Information about the ARGB color attribute.
    glsl_uniform_desc_t    *UniformMSS;        /// Information about the screenspace -> clipspace matrix.
};

/// @summary Maintains the state associated with a default sprite shader
/// accepting position, texture and color attributes as vertex input.
struct gl_sprite_shader_ptc_tex_t
{
    GLuint                  Program;           /// The OpenGL program object ID.
    glsl_shader_desc_t      ShaderDesc;        /// Metadata about the shader program.
    glsl_attribute_desc_t  *AttribPTX;         /// Information about the Position-Texture attribute.
    glsl_attribute_desc_t  *AttribCLR;         /// Information about the ARGB color attribute.
    glsl_sampler_desc_t    *SamplerTEX;        /// Information about the texture sampler.
    glsl_uniform_desc_t    *UniformMSS;        /// Information about the screenspace -> clipspace matrix.
};

/// @summary Represents a unique identifier for a locked image.
struct gl_image_id_t
{
    uintptr_t               ImageId;           /// The application-defined image identifier.
    size_t                  FrameIndex;        /// The zero-based index of the frame.
};

/// @summary Defines the data stored for a locked image.
struct gl_image_data_t
{
    DWORD                   ErrorCode;         /// ERROR_SUCCESS or a system error code.
    uint32_t                SourceFormat;      /// One of dxgi_format_e defining the source data storage format.
    uint32_t                SourceCompression; /// One of image_compression_e defining the source data storage compression.
    uint32_t                SourceEncoding;    /// One of image_encoding_e defining the source data storage encoding.
    thread_image_cache_t   *SourceCache;       /// The image cache that manages the source image data.
    size_t                  SourceWidth;       /// The width of the source image, in pixels.
    size_t                  SourceHeight;      /// The height of the source image, in pixels.
    size_t                  SourcePitch;       /// The number of bytes per-row in the source image data.
    uint8_t                *SourceData;        /// Pointer to the start of the locked image data on the host.
    size_t                  SourceSize;        /// The number of bytes of source data.
};

/// @summary Defines the data associated with a single in-flight frame.
struct gl_frame_t
{
    int                     FrameState;        /// One of gdi_frame_state_e indicating the current state of the frame.

    pr_command_list_t      *CommandList;       /// The command list that generated this frame.

    size_t                  ImageCount;        /// The number of images referenced in this frame.
    size_t                  ImageCapacity;     /// The capacity of the frame image list.
    gl_image_id_t          *ImageIds;          /// The unique identifiers of the referenced images.
};

/// 
struct gl3_renderer_t
{   typedef image_cache_result_queue_t             image_lock_queue_t;
    typedef image_cache_error_queue_t              image_error_queue_t;

    size_t                  Width;             /// The current viewport width.
    size_t                  Height;            /// The current viewport height.
    HDC                     Drawable;          /// The target drawable, which may reference the display's hidden drawable.
    HWND                    Window;            /// The target window, which may reference the display's hidden window.
    gl_display_t           *Display;           /// The display on which the drawable or window resides.
    pr_command_queue_t      CommandQueue;      /// The presentation command queue to which command lists are submitted.

    uint32_t                RenderFlags;       /// A combination of gl_renderer_flags_e.
    GLuint                  Framebuffer;       /// The target framebuffer object (0 = render to window).
    GLuint                  Renderbuffer[2];   /// Index 0 => color, Index 1 => depth/stencil.

    size_t                  CacheCount;        /// The number of thread-local cache writers.
    size_t                  CacheCapacity;     /// The maximum capacity of the presentation thread image cache list.
    thread_image_cache_t   *CacheList;         /// The list of interfaces used to access image caches from the presentation thread.

    size_t                  ImageCount;        /// The number of locked images.
    size_t                  ImageCapacity;     /// The maximum capacity of the locked image list.
    gl_image_id_t          *ImageIds;          /// The list of locked image identifiers.
    gl_image_data_t        *ImageData;         /// The list of locked image data descriptors.
    size_t                 *ImageRefs;         /// The list of reference counts for each locked image.

    image_lock_queue_t      ImageLockQueue;    /// The MPSC unbounded FIFO where completed image lock requests are stored.
    image_error_queue_t     ImageErrorQueue;   /// The MPSC unbounded FIFO where failed image lock requests are stored.

    #define MAXF            GLRC_MAX_FRAMES
    uint32_t                FrameHead;         /// The zero-based index of the oldest in-flight frame.
    uint32_t                FrameTail;         /// The zero-based index of the newest in-flight frame.
    uint32_t                FrameMask;         /// The bitmask used to map an index into the frame array.
    gl_frame_t              FrameQueue[MAXF];  /// Storage for in-flight frame data.
    #undef  MAXF
};

/*///////////////
//   Globals   //
///////////////*/
// presentation command list is dequeued
// - create a new local list entry
// - read all commands, generate list of compute jobs
// - submit lock requests to image cache
//   - the pointer can be used to track lock completion status
// - as each lock result is received, launch the corresponding compute job
//   - needs a pointer to the locked data
//   - needs basic data like image dimensions
//   - needs a job identifier
// - compute job completion is the job ID plus a status code plus an output uintptr_t?
//   - key thing is how to represent the output data...
// - so one open question is how do we link up compute pipeline output with with graphics pipeline input?
//   - there may not be any graphics pipeline input
// - each compute pipeline job needs its own presentation command
//   - the data for the command defines input and output parameters, ie. output to this texture or buffer.
// - once all (blocking) compute jobs have completed, process the command list
/*//////////////
//  Functors  //
//////////////*/
/// @summary Functor used for sorting sprites into back-to-front order.
struct back_to_front
{
    gl_sprite_batch_t *batch;

    inline back_to_front(gl_sprite_batch_t *sprite_batch)
        :
        batch(sprite_batch)
    { /* empty */ }

    inline bool operator()(uint32_t ia, uint32_t ib)
    {
        gl_sprite_sort_data_t const &sdata_a = batch->State[ia];
        gl_sprite_sort_data_t const &sdata_b = batch->State[ib];
        if (sdata_a.LayerDepth  > sdata_b.LayerDepth)  return true;
        if (sdata_a.LayerDepth  < sdata_b.LayerDepth)  return false;
        if (sdata_a.RenderState < sdata_b.RenderState) return true;
        if (sdata_a.RenderState > sdata_b.RenderState) return false;
        return  (ia < ib);
    }
};

/// @summary Functor used for sorting sprites into front-to-back order.
struct front_to_back
{
    gl_sprite_batch_t *batch;

    inline front_to_back(gl_sprite_batch_t *sprite_batch)
        :
        batch(sprite_batch)
    { /* empty */ }

    inline bool operator()(uint32_t ia, uint32_t ib)
    {
        gl_sprite_sort_data_t const &sdata_a = batch->State[ia];
        gl_sprite_sort_data_t const &sdata_b = batch->State[ib];
        if (sdata_a.LayerDepth  < sdata_b.LayerDepth)  return true;
        if (sdata_a.LayerDepth  > sdata_b.LayerDepth)  return false;
        if (sdata_a.RenderState < sdata_b.RenderState) return true;
        if (sdata_a.RenderState > sdata_b.RenderState) return false;
        return  (ia > ib);
    }
};

/// @summary Functor used for sorting sprites by render state.
struct by_render_state
{
    gl_sprite_batch_t *batch;

    inline by_render_state(gl_sprite_batch_t *sprite_batch)
        :
        batch(sprite_batch)
    { /* empty */ }

    inline bool operator()(uint32_t ia, uint32_t ib)
    {
        gl_sprite_sort_data_t const &sdata_a = batch->State[ia];
        gl_sprite_sort_data_t const &sdata_b = batch->State[ib];
        if (sdata_a.RenderState < sdata_b.RenderState) return true;
        if (sdata_a.RenderState > sdata_b.RenderState) return false;
        return  (ia < ib);
    }
};

/*///////////////////////
//   Local Functions   //
///////////////////////*/
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

/// @summary Sorts a sprite batch using std::sort(). The sort is indirect; the
/// order array is what gets sorted. The order array can then be used to read
/// quad definitions from the batch in sorted order.
/// @typename TComp A functor type (uint32_t index_a, uint32_t index_b).
/// @param batch The sprite batch to sort.
template <typename TComp>
internal_function inline void sort_sprite_batch(gl_sprite_batch_t *batch)
{
    TComp cmp(batch);
    std::sort(batch->Order, batch->Order + batch->Count, cmp);
}

/// @summary Calculate the number of in-flight frames.
/// @param gl The render state to query.
/// @return The number of in-flight frames.
internal_function inline size_t frame_count(gl3_renderer_t *gl)
{
    return size_t(gl->FrameTail - gl->FrameHead);
}

/// @summary Prepare an image for use by locking its data in host memory. The lock request completes asynchronously.
/// @param gl The render state processing the request.
/// @param frame The in-flight frame state requiring the image data.
/// @param image A description of the portion of the image required for use.
internal_function void prepare_image(gl3_renderer_t *gl, gl_frame_t *frame, pr_image_subresource_t *image)
{
    uintptr_t const image_id     = image->ImageId;
    size_t    const frame_index  = image->FrameIndex;
    size_t          local_index  = 0;
    size_t          global_index = 0;
    bool            found_image  = false;
    // check the frame-local list first. if we find the image here, we know 
    // that the frame has already been requested, and we don't need to lock it.
    for (size_t i = 0, n = frame->ImageCount; i < n; ++i)
    {
        if (frame->ImageIds[i].ImageId    == image_id && 
            frame->ImageIds[i].FrameIndex == frame_index)
        {
            local_index = i;
            found_image = true;
            break;
        }
    }
    if (!found_image)
    {   // add this image/frame to the local frame list.
        if (frame->ImageCount == frame->ImageCapacity)
        {   // increase the capacity of the local image list.
            size_t old_amount  = frame->ImageCapacity;
            size_t new_amount  = calculate_capacity(old_amount, old_amount+1, 1024, 1024);
            gl_image_id_t *il  =(gl_image_id_t*)realloc(frame->ImageIds , new_amount * sizeof(gl_image_id_t));
            if (il != NULL)
            {
                frame->ImageIds      = il;
                frame->ImageCapacity = new_amount;
            }
        }
        local_index   = frame->ImageCount++;
        frame->ImageIds[local_index].ImageId     = image_id;
        frame->ImageIds[local_index].FrameIndex  = frame_index;
        // attempt to locate the image in the global image list.
        for (size_t gi = 0, gn = gl->ImageCount; gi < gn; ++gi)
        {
            if (gl->ImageIds[gi].ImageId    == image_id && 
                gl->ImageIds[gi].FrameIndex == frame_index)
            {   // this image already exists in the global image list.
                // there's no need to lock it; just increment the reference count.
                gl->ImageRefs[gi]++;
                found_image = true;
                break;
            }
        }
        // if the image wasn't in the global list, add it.
        if (!found_image)
        {
            if (gl->ImageCount == gl->ImageCapacity)
            {   // increase the capacity of the global image list.
                size_t old_amount   = gl->ImageCapacity;
                size_t new_amount   = calculate_capacity(old_amount, old_amount+1, 1024 , 1024);
                gl_image_id_t   *il = (gl_image_id_t  *) realloc(gl->ImageIds , new_amount * sizeof(gl_image_id_t));
                gl_image_data_t *dl = (gl_image_data_t*) realloc(gl->ImageData, new_amount * sizeof(gl_image_data_t));
                size_t          *rl = (size_t         *) realloc(gl->ImageRefs, new_amount * sizeof(size_t));
                if (il != NULL)  gl->ImageIds  = il;
                if (dl != NULL)  gl->ImageData = dl;
                if (rl != NULL)  gl->ImageRefs = rl;
                if (il != NULL && dl != NULL  && rl != NULL) gl->ImageCapacity = new_amount;
            }
            global_index = gl->ImageCount++;
            gl->ImageRefs[global_index]                   = 1;
            gl->ImageIds [global_index].ImageId           = image_id;
            gl->ImageIds [global_index].FrameIndex        = frame_index;
            gl->ImageData[global_index].ErrorCode         = ERROR_SUCCESS;
            gl->ImageData[global_index].SourceFormat      = DXGI_FORMAT_UNKNOWN;
            gl->ImageData[global_index].SourceCompression = IMAGE_COMPRESSION_NONE;
            gl->ImageData[global_index].SourceEncoding    = IMAGE_ENCODING_RAW;
            gl->ImageData[global_index].SourceCache       = NULL;
            gl->ImageData[global_index].SourceWidth       = 0;
            gl->ImageData[global_index].SourceHeight      = 0;
            gl->ImageData[global_index].SourcePitch       = 0;
            gl->ImageData[global_index].SourceData        = NULL;
            gl->ImageData[global_index].SourceSize        = 0;
            // locate the presentation thread cache interface for the source data:
            bool found_cache = false;
            for (size_t ci = 0, cn = gl->CacheCount; ci < cn; ++ci)
            {
                if (gl->CacheList[ci].Cache == image->ImageSource)
                {
                    gl->ImageData[global_index].SourceCache = &gl->CacheList[ci];
                    found_cache = true;
                    break;
                }
            }
            if (!found_cache)
            {   // create a new interface to write to this image cache.
                if (gl->CacheCount == gl->CacheCapacity)
                {   // increase the capacity of the image cache list.
                    size_t old_amount   = gl->CacheCapacity;
                    size_t new_amount   = calculate_capacity(old_amount, old_amount+1, 16, 16);
                    thread_image_cache_t *cl = (thread_image_cache_t*)   realloc(gl->CacheList, new_amount * sizeof(thread_image_cache_t));
                    if (cl != NULL)
                    {
                        gl->CacheList     = cl;
                        gl->CacheCapacity = new_amount;
                    }
                }
                thread_image_cache_t *cache = &gl->CacheList[gl->CacheCount++];
                gl->ImageData[global_index].SourceCache = cache;
                cache->initialize(image->ImageSource);
            }
            // finally, submit a lock request for the frame.
            gl->ImageData[global_index].SourceCache->lock(image_id, frame_index, frame_index, &gl->ImageLockQueue, &gl->ImageErrorQueue, 0);
        }
    }
}

/// @summary Attempt to start a new in-flight frame.
/// @param gl The renderer state managing the frame.
/// @param cmdlist The presentation command list defining the frame.
/// @return true if the frame was launched, or false if too many frames are in-flight.
internal_function bool launch_frame(gl3_renderer_t *gl, pr_command_list_t *cmdlist)
{   // insert a new frame at the tail of the queue.
    if (frame_count(gl) == GLRC_MAX_FRAMES)
    {   // there are too many frames in-flight.
        return false;
    }
    uint32_t     id    = gl->FrameTail;
    size_t       index = size_t(id & gl->FrameMask); 
    gl_frame_t  *frame =&gl->FrameQueue[index];
    frame->FrameState  = GLRC_FRAME_STATE_WAIT_FOR_IMAGES;
    frame->CommandList = cmdlist;
    frame->ImageCount  = 0;

    // iterate through the command list and locate all PREPARE_IMAGE commands.
    // for each of these, update the frame-local image list as well as the 
    // the driver-global image list. eventually, we'll also use this to initialize
    // any compute and display jobs as well.
    bool       end_of_frame = false;
    uint8_t const *read_ptr = cmdlist->CommandData;
    uint8_t const *end_ptr  = cmdlist->CommandData + cmdlist->BytesUsed;
    while (read_ptr < end_ptr && !end_of_frame)
    {
        pr_command_t *cmd_info = (pr_command_t*) read_ptr;
        size_t        cmd_size =  PR_COMMAND_SIZE_BASE + cmd_info->DataSize;
        switch (cmd_info->CommandId)
        {
        case PR_COMMAND_END_OF_FRAME:
            {   // break out of the command processing loop.
                end_of_frame = true;
                gl->FrameTail++;
            }
            break;
        case PR_COMMAND_PREPARE_IMAGE:
            {   // lock the source data in host memory.
                prepare_image(gl, frame, (pr_image_subresource_t*) cmd_info->Data);
            }
            break;
        // TODO(rlk): case PR_COMMAND_compute_x: init compute job record, but don't start
        // TODO(rlk): case PR_COMMAND_display_x: init display job record, but don't start
        default:
            break;
        }
        // advance to the start of the next buffered command.
        read_ptr += cmd_size;
    }
    return true;
}

/// @summary Perform cleanup operations when the display commands for the oldest in-flight frame have completed.
/// @param gl The renderer state managing the frame.
internal_function void retire_frame(gl3_renderer_t *gl)
{   // retire the frame at the head of the queue.
    uint32_t          id = gl->FrameHead++;
    size_t         index = size_t(id & gl->FrameMask);
    gl_frame_t    *frame =&gl->FrameQueue[index];
    gl_image_id_t *local_ids  = frame->ImageIds;
    gl_image_id_t *global_ids = gl->ImageIds;
    for (size_t local_index = 0, local_count = frame->ImageCount; local_index < local_count; ++local_index)
    {   // locate the corresponding entry in the global list.
        gl_image_id_t const &local_id = local_ids[local_index];
        for (size_t global_index = 0; global_index < gl->ImageCount; ++global_index)
        {   
            gl_image_id_t const &global_id  = global_ids[global_index];
            if (local_id.ImageId == global_id.ImageId && local_id.FrameIndex == global_id.FrameIndex)
            {   // found a match. decrement the reference count.
                if (gl->ImageRefs[global_index]-- == 1)
                {   // the image reference count has dropped to zero.
                    // unlock the image data, and delete the entry from the global list.
                    size_t frame_index = global_id.FrameIndex;
                    size_t  last_index = gl->ImageCount - 1;
                    gl->ImageData[global_index].SourceCache->unlock(global_id.ImageId, frame_index, frame_index);
                    gl->ImageIds [global_index] = gl->ImageIds [last_index];
                    gl->ImageData[global_index] = gl->ImageData[last_index];
                    gl->ImageRefs[global_index] = gl->ImageRefs[last_index];
                    gl->ImageCount--;
                }
                break;
            }
        }
    }
    // TODO(rlk): cleanup compute jobs
    // TODO(rlk): cleanup display state
    pr_command_queue_return(&gl->CommandQueue, frame->CommandList);
    frame->ImageCount = 0;
}

/// @summary Given an OpenGL data type value, calculates the corresponding size.
/// @param data_type The OpenGL data type value, for example, GL_UNSIGNED_BYTE.
/// @return The size of a single element of the specified type, in bytes.
internal_function size_t gl_data_size(GLenum data_type)
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
internal_function size_t gl_block_dimension(GLenum internal_format)
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
internal_function size_t gl_bytes_per_block(GLenum internal_format)
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
internal_function size_t gl_bytes_per_element(GLenum internal_format, GLenum data_type)
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
internal_function size_t gl_bytes_per_row(GLenum internal_format, GLenum data_type, size_t width, size_t alignment)
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
internal_function size_t gl_bytes_per_slice(GLenum internal_format, GLenum data_type, size_t width, size_t height, size_t alignment)
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
internal_function size_t gl_image_dimension(GLenum internal_format, size_t dimension)
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
internal_function GLenum gl_pixel_layout(GLenum internal_format)
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
internal_function GLenum gl_texture_target(GLenum sampler_type)
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
internal_function bool dxgi_format_to_gl(uint32_t dxgi, GLenum &out_internalformat, GLenum &out_format, GLenum &out_datatype)
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
internal_function size_t gl_level_count(size_t width, size_t height, size_t slice_count, size_t max_levels)
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
internal_function size_t gl_level_dimension(size_t dimension, size_t level_index)
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
internal_function void gl_describe_mipmaps(GLenum internal_format, GLenum data_type, size_t width, size_t height, size_t slice_count, size_t alignment, size_t max_levels, gl_level_desc_t *level_desc)
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
internal_function void gl_checker_image(size_t width, size_t height, float alpha, void *buffer)
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
internal_function void gl_texture_storage(gl_display_t *display, GLenum target, GLenum internal_format, GLenum data_type, GLenum min_filter, GLenum mag_filter, size_t width, size_t height, size_t slice_count, size_t max_levels)
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
internal_function void gl_transfer_pixels_d2h(gl_display_t *display, gl_pixel_transfer_d2h_t *transfer)
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
internal_function bool gl_create_pixel_stream_h2d(gl_display_t *display, gl_pixel_stream_h2d_t *stream, size_t alignment, size_t buffer_size)
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
internal_function void gl_delete_pixel_stream_h2d(gl_display_t *display, gl_pixel_stream_h2d_t *stream)
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
internal_function void* gl_pixel_stream_h2d_reserve(gl_display_t *display, gl_pixel_stream_h2d_t *stream, size_t amount)
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
internal_function bool gl_pixel_stream_h2d_commit(gl_display_t *display, gl_pixel_stream_h2d_t *stream, gl_pixel_transfer_h2d_t *transfer)
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
internal_function void gl_pixel_stream_h2d_cancel(gl_display_t *display, gl_pixel_stream_h2d_t *stream)
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
internal_function void gl_transfer_pixels_h2d(gl_display_t *display, gl_pixel_transfer_h2d_t *transfer)
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
internal_function uint32_t glsl_shader_name(char const *name)
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
internal_function bool glsl_builtin(char const *name)
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
internal_function bool glsl_compile_shader(gl_display_t *display, GLenum shader_type, char **shader_source, size_t string_count, GLuint *out_shader, size_t *out_log_size)
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
internal_function void glsl_copy_compile_log(gl_display_t *display, GLuint shader, char *buffer, size_t buffer_size)
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
internal_function bool glsl_attach_shaders(gl_display_t *display, GLuint *shader_list, size_t shader_count, GLuint *out_program)
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
internal_function bool glsl_assign_vertex_attributes(gl_display_t *display, GLuint program, char const **attrib_names, GLuint *attrib_locations, size_t attrib_count)
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
internal_function bool glsl_assign_fragment_outputs(gl_display_t *display, GLuint program, char const **output_names, GLuint *output_locations, size_t output_count)
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
internal_function bool glsl_link_program(gl_display_t *display, GLuint program, size_t *out_max_name, size_t *out_log_size)
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
internal_function void glsl_copy_linker_log(gl_display_t *display, GLuint program, char *buffer, size_t buffer_size)
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
internal_function bool glsl_shader_desc_alloc(glsl_shader_desc_t *desc, size_t num_attribs, size_t num_samplers, size_t num_uniforms)
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
internal_function void glsl_shader_desc_free(glsl_shader_desc_t *desc)
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
internal_function void glsl_reflect_program_counts(gl_display_t *display, GLuint program, char *buffer, size_t buffer_size, bool include_builtins, size_t &out_num_attribs, size_t &out_num_samplers, size_t &out_num_uniforms)
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
internal_function void glsl_reflect_program_details(gl_display_t *display, GLuint program, char *buffer, size_t buffer_size, bool include_builtins, uint32_t *attrib_names, glsl_attribute_desc_t *attrib_info, uint32_t *sampler_names, glsl_sampler_desc_t *sampler_info, uint32_t *uniform_names, glsl_uniform_desc_t *uniform_info)
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
internal_function void glsl_set_sampler(gl_display_t *display, glsl_sampler_desc_t *sampler, GLuint texture)
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
internal_function void glsl_set_uniform(gl_display_t *display, glsl_uniform_desc_t *uniform, void const *value, bool transpose)
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
internal_function void glsl_shader_source_init(glsl_shader_source_t *source)
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
internal_function void glsl_shader_source_add(glsl_shader_source_t *source, GLenum shader_stage, char **source_code, size_t string_count)
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
internal_function bool glsl_build_shader(gl_display_t *display, glsl_shader_source_t *source, glsl_shader_desc_t *shader, GLuint *out_program)
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

/// @summary Initializes a sprite batch with the specified capacity.
/// @param batch The sprite batch.
/// @param capacity The initial capacity of the batch, in quads.
internal_function void gl_create_sprite_batch(gl_sprite_batch_t *batch, size_t capacity)
{
    if (batch)
    {
        batch->Count    = 0;
        batch->Capacity = capacity;
        if (capacity)
        {
            batch->Quads = (gl_sprite_quad_t*)      malloc(capacity * sizeof(gl_sprite_quad_t));
            batch->State = (gl_sprite_sort_data_t*) malloc(capacity * sizeof(gl_sprite_sort_data_t));
            batch->Order = (uint32_t*)              malloc(capacity * sizeof(uint32_t));
        }
        else
        {
            batch->Quads = NULL;
            batch->State = NULL;
            batch->Order = NULL;
        }
    }
}

/// @summary Frees the memory associated with a sprite batch.
/// @param batch The sprite batch to free.
internal_function void gl_delete_sprite_batch(gl_sprite_batch_t *batch)
{
    if (batch)
    {
        if (batch->Capacity)
        {
            free(batch->Order);
            free(batch->State);
            free(batch->Quads);
        }
        batch->Count    = 0;
        batch->Capacity = 0;
        batch->Quads    = NULL;
        batch->State    = NULL;
        batch->Order    = NULL;
    }
}

/// @summary Ensures that the sprite batch has at least the specified capacity.
/// @param batch The sprite batch.
/// @param capacity need to pass batch->Count + NumToAdd as capacity
internal_function void gl_ensure_sprite_batch(gl_sprite_batch_t *batch, size_t capacity)
{
    if (batch->Capacity < capacity)
    {
        batch->Capacity = capacity;
        batch->Quads    = (gl_sprite_quad_t*)      realloc(batch->Quads, capacity * sizeof(gl_sprite_quad_t));
        batch->State    = (gl_sprite_sort_data_t*) realloc(batch->State, capacity * sizeof(gl_sprite_sort_data_t));
        batch->Order    = (uint32_t*)              realloc(batch->Order, capacity * sizeof(uint32_t));
    }
}

/// @summary Discards data buffered by a sprite batch.
/// @param batch The sprite batch to flush.
internal_function void gl_flush_sprite_batch(gl_sprite_batch_t *batch)
{
    batch->Count = 0;
}

/// @summary Transforms a set of sprite definitions into a series of quad definitions.
/// @param quads The buffer of quads to write to.
/// @param sdata The buffer of state data to write to.
/// @param indices The buffer of order indices to write to.
/// @param quad_offset The zero-based index of the first quad to write.
/// @param sprites The buffer of sprite definitions to read from.
/// @param sprite_offset The zero-based index of the first sprite to read.
/// @param count The number of sprite definitions to read.
internal_function void gl_generate_quads(gl_sprite_quad_t *quads, gl_sprite_sort_data_t *sdata, uint32_t *indices, size_t quad_offset, gl_sprite_t const *sprites, size_t sprite_offset, size_t sprite_count)
{
    size_t qindex = quad_offset;
    size_t sindex = sprite_offset;
    for (size_t i = 0;  i < sprite_count; ++i, ++qindex, ++sindex)
    {
        gl_sprite_t const     &s = sprites[sindex];
        gl_sprite_sort_data_t &r = sdata[qindex];
        gl_sprite_quad_t      &q = quads[qindex];

        q.Source[0]   = float(s.ImageX);
        q.Source[1]   = float(s.ImageY);
        q.Source[2]   = float(s.ImageWidth);
        q.Source[3]   = float(s.ImageHeight);
        q.Target[0]   = s.ScreenX;
        q.Target[1]   = s.ScreenY;
        q.Target[2]   = s.ImageWidth  * s.ScaleX;
        q.Target[3]   = s.ImageHeight * s.ScaleY;
        q.Origin[0]   = s.OriginX;
        q.Origin[1]   = s.OriginY;
        q.Scale [0]   = 1.0f / s.TextureWidth;
        q.Scale [1]   = 1.0f / s.TextureHeight;
        q.Orientation = s.Orientation;
        q.TintColor   = s.TintColor;

        r.LayerDepth  = s.LayerDepth;
        r.RenderState = s.RenderState;

        indices[qindex] = uint32_t(qindex);
    }
}

/// @summary Generates transformed position-texture-color vertex data for a set, or subset, of quads.
/// @param buffer The buffer to which vertex data will be written.
/// @param buffer_offset The offset into the buffer of the first vertex.
/// @param quads The buffer from which quad attributes will be read.
/// @param indices An array of zero-based indices specifying the order in which to read quads from the quad buffer.
/// @param quad_offset The offset into the quad list of the first quad.
/// @param quad_count The number of quads to generate.
internal_function void gl_generate_quad_vertices_ptc(void *buffer, size_t buffer_offset, gl_sprite_quad_t const *quads, uint32_t const *indices, size_t quad_offset, size_t quad_count)
{
    static size_t const X      =  0;
    static size_t const Y      =  1;
    static size_t const W      =  2;
    static size_t const H      =  3;
    static float  const XCO[4] = {0.0f, 1.0f, 1.0f, 0.0f};
    static float  const YCO[4] = {0.0f, 0.0f, 1.0f, 1.0f};

    gl_sprite_vertex_ptc_t *vertex_buffer = (gl_sprite_vertex_ptc_t*) buffer;
    size_t                  vertex_offset = buffer_offset;
    for (size_t i = 0; i < quad_count; ++i)
    {
        // figure out which quad we're working with.
        uint32_t const id = indices[quad_offset + i];
        gl_sprite_quad_t const &quad = quads[id];

        // pre-calculate values constant across the quad.
        float    const src_x = quad.Source[X];
        float    const src_y = quad.Source[Y];
        float    const src_w = quad.Source[W];
        float    const src_h = quad.Source[H];
        float    const dst_x = quad.Target[X];
        float    const dst_y = quad.Target[Y];
        float    const dst_w = quad.Target[W];
        float    const dst_h = quad.Target[H];
        float    const ctr_x = quad.Origin[X] / src_w;
        float    const ctr_y = quad.Origin[Y] / src_h;
        float    const scl_u = quad.Scale[X];
        float    const scl_v = quad.Scale[Y];
        float    const angle = quad.Orientation;
        uint32_t const color = quad.TintColor;
        float    const sin_o = sinf(angle);
        float    const cos_o = cosf(angle);

        // calculate values that change per-vertex.
        for (size_t j = 0; j < 4; ++j)
        {
            gl_sprite_vertex_ptc_t &vert = vertex_buffer[vertex_offset++];
            float ofs_x    = XCO[j];
            float ofs_y    = YCO[j];
            float x_dst    = (ofs_x - ctr_x) *  dst_w;
            float y_dst    = (ofs_y - ctr_y) *  dst_h;
            vert.XYUV[0]   = (dst_x + (x_dst * cos_o)) - (y_dst * sin_o);
            vert.XYUV[1]   = (dst_y + (x_dst * sin_o)) + (y_dst * cos_o);
            vert.XYUV[2]   = (src_x + (ofs_x * src_w)) *  scl_u;
            vert.XYUV[3]   = 1.0f   -((src_y +(ofs_y   *  src_h)) *  scl_v);
            vert.TintColor = color;
        }
    }
}

/// @summary Generates index data for a set, or subset, of quads. Triangles are specified using counter-clockwise winding. Indices are 16-bit unsigned int.
/// @param buffer The destination buffer.
/// @param offset The offset into the buffer of the first index.
/// @param base_vertex The zero-based index of the first vertex of the batch. This allows multiple batches to write into the same index buffer.
/// @param quad_count The number of quads being generated.
internal_function void gl_generate_quad_indices_u16(void *buffer, size_t offset, size_t base_vertex, size_t quad_count)
{
    uint16_t *u16 = (uint16_t*) buffer;
    uint16_t  b16 = (uint16_t ) base_vertex;
    for (size_t i = 0; i < quad_count; ++i)
    {
        u16[offset++]  = (b16 + 1);
        u16[offset++]  = (b16 + 0);
        u16[offset++]  = (b16 + 2);
        u16[offset++]  = (b16 + 2);
        u16[offset++]  = (b16 + 0);
        u16[offset++]  = (b16 + 3);
        b16 += 4;
    }
}

/// @summary Generates index data for a set, or subset, of quads. Triangles are specified using counter-clockwise winding. Indices are 32-bit unsigned int.
/// @param buffer The destination buffer.
/// @param offset The offset into the buffer of the first index.
/// @param base_vertex The zero-based index of the first vertex of the batch. This allows multiple batches to write into the same index buffer.
/// @param quad_count The number of quads being generated.
internal_function void gl_generate_quad_indices_u32(void *buffer, size_t offset, size_t base_vertex, size_t quad_count)
{
    uint32_t *u32 = (uint32_t*) buffer;
    uint32_t  b32 = (uint32_t ) base_vertex;
    for (size_t i = 0; i < quad_count; ++i)
    {
        u32[offset++]  = (b32 + 1);
        u32[offset++]  = (b32 + 0);
        u32[offset++]  = (b32 + 2);
        u32[offset++]  = (b32 + 2);
        u32[offset++]  = (b32 + 0);
        u32[offset++]  = (b32 + 3);
        b32 += 4;
    }
}

/// @summary Creates the GPU resources required to buffer and render quads.
/// @param display The display managing the rendering context.
/// @param effect The effect to initialize.
/// @param quad_count The maximum number of quads that can be buffered.
/// @param vertex_size The size of a single vertex, in bytes.
/// @param index_size The size of a single index, in bytes.
internal_function bool gl_create_sprite_effect(gl_display_t *display, gl_sprite_effect_t *effect, size_t quad_count, size_t vertex_size, size_t index_size)
{
    GLuint  vao        =  0;
    GLuint  buffers[2] = {0, 0};
    size_t  vcount     = quad_count  * 4;
    size_t  icount     = quad_count  * 6;
    GLsizei abo_size   = GLsizei(vertex_size * vcount);
    GLsizei eao_size   = GLsizei(index_size  * icount);

    glGenBuffers(2, buffers);
    glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
    glBufferData(GL_ARRAY_BUFFER, abo_size, NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, eao_size, NULL, GL_DYNAMIC_DRAW);
    glGenVertexArrays(1, &vao);

    effect->VertexCapacity   = vcount;
    effect->VertexOffset     = 0;
    effect->VertexSize       = vertex_size;
    effect->IndexCapacity    = icount;
    effect->IndexOffset      = 0;
    effect->IndexSize        = index_size;
    effect->CurrentState     = 0xFFFFFFFFU;
    effect->VertexArray      = vao;
    effect->VertexBuffer     = buffers[0];
    effect->IndexBuffer      = buffers[1];
    effect->BlendEnabled     = GL_FALSE;
    effect->BlendSourceColor = GL_ONE;
    effect->BlendSourceAlpha = GL_ONE;
    effect->BlendTargetColor = GL_ZERO;
    effect->BlendTargetAlpha = GL_ZERO;
    effect->BlendFuncColor   = GL_FUNC_ADD;
    effect->BlendFuncAlpha   = GL_FUNC_ADD;
    effect->BlendColor[0]    = 0.0f;
    effect->BlendColor[1]    = 0.0f;
    effect->BlendColor[2]    = 0.0f;
    effect->BlendColor[3]    = 0.0f;
    return true;
}

/// @summary Releases the GPU resources used for buffering and rendering quads.
/// @param display The display managing the rendering context.
/// @param effect The effect to destroy.
internal_function void gl_delete_sprite_effect(gl_display_t *display, gl_sprite_effect_t *effect)
{
    GLuint buffers[2] = {
        effect->VertexBuffer,
        effect->IndexBuffer
    };
    glDeleteBuffers(2, buffers);
    glDeleteVertexArrays(1, &effect->VertexArray);
    effect->VertexCapacity = 0;
    effect->VertexOffset   = 0;
    effect->VertexSize     = 0;
    effect->IndexCapacity  = 0;
    effect->IndexOffset    = 0;
    effect->IndexSize      = 0;
    effect->VertexArray    = 0;
    effect->VertexBuffer   = 0;
    effect->IndexBuffer    = 0;
}

/// @summary Disables alpha blending for an effect. The state changes do not take effect until the effect is (re)bound.
/// @param effect The effect to update.
internal_function void gl_sprite_effect_blend_none(gl_sprite_effect_t *effect)
{
    effect->BlendEnabled     = GL_FALSE;
    effect->BlendSourceColor = GL_ONE;
    effect->BlendSourceAlpha = GL_ONE;
    effect->BlendTargetColor = GL_ZERO;
    effect->BlendTargetAlpha = GL_ZERO;
    effect->BlendFuncColor   = GL_FUNC_ADD;
    effect->BlendFuncAlpha   = GL_FUNC_ADD;
    effect->BlendColor[0]    = 0.0f;
    effect->BlendColor[1]    = 0.0f;
    effect->BlendColor[2]    = 0.0f;
    effect->BlendColor[3]    = 0.0f;
}

/// @summary Enables standard alpha blending (texture transparency) for an effect. The state changes do not take effect until the effect is (re)bound.
/// @param effect The effect to update.
internal_function void gl_sprite_effect_blend_alpha(gl_sprite_effect_t *effect)
{
    effect->BlendEnabled     = GL_TRUE;
    effect->BlendSourceColor = GL_SRC_COLOR;
    effect->BlendSourceAlpha = GL_SRC_ALPHA;
    effect->BlendTargetColor = GL_ONE_MINUS_SRC_ALPHA;
    effect->BlendTargetAlpha = GL_ONE_MINUS_SRC_ALPHA;
    effect->BlendFuncColor   = GL_FUNC_ADD;
    effect->BlendFuncAlpha   = GL_FUNC_ADD;
    effect->BlendColor[0]    = 0.0f;
    effect->BlendColor[1]    = 0.0f;
    effect->BlendColor[2]    = 0.0f;
    effect->BlendColor[3]    = 0.0f;
}

/// @summary Enables additive alpha blending for an effect. The state changes do not take effect until the effect is (re)bound.
/// @param effect The effect to update.
internal_function void gl_sprite_effect_blend_additive(gl_sprite_effect_t *effect)
{
    effect->BlendEnabled     = GL_TRUE;
    effect->BlendSourceColor = GL_SRC_COLOR;
    effect->BlendSourceAlpha = GL_SRC_ALPHA;
    effect->BlendTargetColor = GL_ONE;
    effect->BlendTargetAlpha = GL_ONE;
    effect->BlendFuncColor   = GL_FUNC_ADD;
    effect->BlendFuncAlpha   = GL_FUNC_ADD;
    effect->BlendColor[0]    = 0.0f;
    effect->BlendColor[1]    = 0.0f;
    effect->BlendColor[2]    = 0.0f;
    effect->BlendColor[3]    = 0.0f;
}

/// @summary Enables alpha blending with premultiplied alpha in the source texture. The state changes do not take effect until the effect is (re)bound.
/// @param effect The effect to update.
internal_function void gl_sprite_effect_blend_premultiplied(gl_sprite_effect_t *effect)
{
    effect->BlendEnabled     = GL_TRUE;
    effect->BlendSourceColor = GL_ONE;
    effect->BlendSourceAlpha = GL_ONE;
    effect->BlendTargetColor = GL_ONE_MINUS_SRC_ALPHA;
    effect->BlendTargetAlpha = GL_ONE_MINUS_SRC_ALPHA;
    effect->BlendFuncColor   = GL_FUNC_ADD;
    effect->BlendFuncAlpha   = GL_FUNC_ADD;
    effect->BlendColor[0]    = 0.0f;
    effect->BlendColor[1]    = 0.0f;
    effect->BlendColor[2]    = 0.0f;
    effect->BlendColor[3]    = 0.0f;
}

/// @summary Sets up the effect projection matrix for the given viewport.
/// @param effect The effect to update.
/// @param width The viewport width.
/// @param height The viewport height.
internal_function void gl_sprite_effect_set_viewport(gl_sprite_effect_t *effect, int width, int height)
{
    float *dst16 = effect->Projection;
    float  s_x = 1.0f / (width   * 0.5f);
    float  s_y = 1.0f / (height  * 0.5f);
    dst16[ 0]  = s_x ; dst16[ 1] = 0.0f; dst16[ 2] = 0.0f; dst16[ 3] = 0.0f;
    dst16[ 4]  = 0.0f; dst16[ 5] = -s_y; dst16[ 6] = 0.0f; dst16[ 7] = 0.0f;
    dst16[ 8]  = 0.0f; dst16[ 9] = 0.0f; dst16[10] = 1.0f; dst16[11] = 0.0f;
    dst16[12]  =-1.0f; dst16[13] = 1.0f; dst16[14] = 0.0f; dst16[15] = 1.0f;
}

/// @summary Binds the vertex and index buffers of an effect for use in subsequent rendering commands.
/// @param display The display managing the rendering context.
/// @param effect The effect being applied.
internal_function void gl_sprite_effect_bind_buffers(gl_display_t *display, gl_sprite_effect_t *effect)
{
    glBindVertexArray(effect->VertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, effect->VertexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, effect->IndexBuffer);
}

/// @summary Applies the alpha blending state specified by the effect.
/// @param display The display managing the rendering context.
/// @param effect The effect being applied.
internal_function void gl_sprite_effect_apply_blendstate(gl_display_t *display, gl_sprite_effect_t *effect)
{
    if (effect->BlendEnabled)
    {
        glEnable(GL_BLEND);
        glBlendColor(effect->BlendColor[0], effect->BlendColor[1], effect->BlendColor[2], effect->BlendColor[3]);
        glBlendFuncSeparate(effect->BlendSourceColor, effect->BlendTargetColor, effect->BlendSourceAlpha, effect->BlendTargetAlpha);
        glBlendEquationSeparate(effect->BlendFuncColor, effect->BlendFuncAlpha);
    }
    else glDisable(GL_BLEND);
}

/// @summary Configures the Vertex Array Object for an effect using the standard Position-TexCoord-Color layout configuration.
/// @param display The display managing the rendering context.
/// @param effect The effect being applied.
internal_function void gl_sprite_effect_setup_vao_ptc(gl_display_t *display, gl_sprite_effect_t *effect)
{
    glBindVertexArray(effect->VertexArray);
    glEnableVertexAttribArray(GL_SPRITE_PTC_LOCATION_PTX);
    glEnableVertexAttribArray(GL_SPRITE_PTC_LOCATION_CLR);
    glVertexAttribPointer(GL_SPRITE_PTC_LOCATION_PTX, 4, GL_FLOAT,         GL_FALSE, sizeof(gl_sprite_vertex_ptc_t), (GLvoid const*)  0);
    glVertexAttribPointer(GL_SPRITE_PTC_LOCATION_CLR, 4, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof(gl_sprite_vertex_ptc_t), (GLvoid const*) 16);
}

/// @summary Generates and uploads vertex and index data for a batch of quads to the vertex and index buffers of an effect. The buffers act as circular buffers. If the end of the buffers is reached, as much data as possible is buffered, and the function returns.
/// @param display The display managing the rendering context.
/// @param effect The effect to update.
/// @param quads The source quad definitions.
/// @param indices An array of zero-based indices specifying the order in which to read quads from the quad buffer.
/// @param quad_offset The offset, in quads, of the first quad to read.
/// @param quad_count The number of quads to read.
/// @param base_index On return, this address is updated with the offset, in indices, of the first buffered primitive written to the index buffer.
/// @return The number of quads actually buffered. May be less than @a count.
internal_function size_t gl_sprite_effect_buffer_data_ptc(gl_display_t *display, gl_sprite_effect_t *effect, gl_sprite_quad_t const *quads, uint32_t const *indices, size_t quad_offset, size_t quad_count, size_t *base_index_arg)
{
    if (effect->VertexOffset == effect->VertexCapacity)
    {
        // the buffer is completely full. time to discard it and
        // request a new buffer from the driver, to avoid stalls.
        GLsizei abo_size     = GLsizei(effect->VertexCapacity * effect->VertexSize);
        GLsizei eao_size     = GLsizei(effect->IndexCapacity  * effect->IndexSize);
        effect->VertexOffset = 0;
        effect->IndexOffset  = 0;
        glBufferData(GL_ARRAY_BUFFER, abo_size, NULL, GL_DYNAMIC_DRAW);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, eao_size, NULL, GL_DYNAMIC_DRAW);
    }

    size_t num_indices  = quad_count * 6;
    size_t num_vertices = quad_count * 4;
    size_t max_vertices = effect->VertexCapacity;
    size_t base_vertex  = effect->VertexOffset;
    size_t vertex_size  = effect->VertexSize;
    size_t max_indices  = effect->IndexCapacity;
    size_t base_index   = effect->IndexOffset;
    size_t index_size   = effect->IndexSize;
    if (max_vertices < base_vertex + num_vertices)
    {   // not enough space in the buffer to fit everything.
        // only a portion of the desired data will be buffered.
        num_vertices = max_vertices - base_vertex;
        num_indices  = max_indices  - base_index;
    }

    size_t buffer_count =  num_vertices / 4;
    if (buffer_count == 0) return 0;

    GLintptr   v_offset = base_vertex  * vertex_size;
    GLsizeiptr v_size   = num_vertices * vertex_size;
    GLbitfield v_access = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT;
    GLvoid    *v_data   = glMapBufferRange(GL_ARRAY_BUFFER, v_offset, v_size, v_access);
    if (v_data != NULL)
    {
        gl_generate_quad_vertices_ptc(v_data, 0, quads, indices, quad_offset, buffer_count);
        glUnmapBuffer(GL_ARRAY_BUFFER);
    }

    GLintptr   i_offset = base_index  * index_size;
    GLsizeiptr i_size   = num_indices * index_size;
    GLbitfield i_access = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT;
    GLvoid    *i_data   = glMapBufferRange(GL_ELEMENT_ARRAY_BUFFER, i_offset, i_size, i_access);
    if (i_data != NULL)
    {
        if (index_size == sizeof(uint16_t)) gl_generate_quad_indices_u16(i_data, 0, base_vertex, buffer_count);
        else gl_generate_quad_indices_u32(i_data, 0, base_vertex, buffer_count);
        glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
    }

    effect->VertexOffset += buffer_count * 4;
    effect->IndexOffset  += buffer_count * 6;
    *base_index_arg       = base_index;
    return buffer_count;
}

/// @summary Renders a portion of a sprite batch for which the vertex and index data has already been buffered. This function is generally not called by the user directly; it is called internally from gl_sprite_effect_draw_batch_ptc().
/// @param display The display managing the rendering context.
/// @param effect The effect being applied.
/// @param batch The sprite batch being rendered.
/// @param quad_offset The index of the first quad in the batch to be rendered.
/// @param quad_count The number of quads to draw.
/// @param base_index The first index to read from the index buffer.
internal_function void gl_sprite_effect_draw_batch_region_ptc(gl_display_t *display, gl_sprite_effect_t *effect, gl_sprite_batch_t *batch, size_t quad_offset, size_t quad_count, size_t base_index, gl_sprite_effect_apply_t const *fxfuncs, void *context)
{
    uint32_t state_0 = effect->CurrentState;
    uint32_t state_1 = effect->CurrentState;
    size_t   index   = 0; // index of start of sub-batch
    size_t   nquad   = 0; // count of quads in sub-batch
    size_t   nindex  = 0; // count of indices in sub-batch
    size_t   quad_id = 0; // quad insertion index
    GLsizei  size    = GLsizei(effect->IndexSize);
    GLenum   type    = size == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;

    for (size_t i = 0; i < quad_count; ++i)
    {
        quad_id = batch->Order[quad_offset + i];
        state_1 = batch->State[quad_id].RenderState;
        if (state_1 != state_0)
        {   // render the previous sub-batch with the current state,
            // as long as it has at least one quad in the sub-batch.
            if (i > index)
            {
                nquad  = i - index;  // the number of quads being submitted
                nindex = nquad * 6;  // the number of indices being submitted
                glDrawElements(GL_TRIANGLES, GLsizei(nindex), type, GL_BUFFER_OFFSET(base_index * size));
                base_index += nindex;
            }
            // now apply the new state and start a new sub-batch.
            fxfuncs->ApplyState(display, effect, state_1, context);
            state_0 = state_1;
            index   = i;
        }
    }
    // submit the remainder of the sub-batch.
    nquad  = quad_count - index;
    nindex = nquad * 6;
    glDrawElements(GL_TRIANGLES, GLsizei(nindex), type, GL_BUFFER_OFFSET(base_index * size));
    effect->CurrentState = state_1;
}

/// @summary Renders an entire sprite batch with a given effect.
/// @param display The display managing the rendering context.
/// @param effect The effect being applied.
/// @param batch The sprite batch being rendered.
/// @param fxfuncs The effect-specific functions for applying render state.
/// @param context Opaque data defined by the application.
internal_function void gl_sprite_effect_draw_batch_ptc(gl_display_t *display, gl_sprite_effect_t *effect, gl_sprite_batch_t *batch, gl_sprite_effect_apply_t const *fxfuncs, void *context)
{
    size_t quad_count = batch->Count;
    size_t quad_index = 0;
    size_t base_index = 0;
    size_t n          = 0;

    fxfuncs->SetupEffect(display, effect, context);
    effect->CurrentState = 0xFFFFFFFFU;

    while (quad_count > 0)
    {
        n = gl_sprite_effect_buffer_data_ptc(display, effect, batch->Quads, batch->Order, quad_index, quad_count, &base_index);
        gl_sprite_effect_draw_batch_region_ptc(display, effect, batch, quad_index, n, base_index, fxfuncs, context);
        base_index  = effect->IndexOffset;
        quad_index += n;
        quad_count -= n;
    }
}

/// @summary Creates a shader program consisting of a vertex and fragment shader used for rendering solid-colored 2D sprites in screen space.
/// @param display The display managing the rendering context.
/// @param shader The shader state to initialize.
/// @return true if the shader program was created successfully.
internal_function bool gl_create_sprite_shader_ptc_clr(gl_display_t *display, gl_sprite_shader_ptc_clr_t *shader)
{
    if (shader)
    {
        glsl_shader_source_t     sources;
        glsl_shader_source_init(&sources);
        glsl_shader_source_add (&sources, GL_VERTEX_SHADER,   (char**) &SpriteShaderPTC_CLR_VSS_150, 1);
        glsl_shader_source_add (&sources, GL_FRAGMENT_SHADER, (char**) &SpriteShaderPTC_CLR_FSS_150, 1);
        if (glsl_build_shader(display, &sources, &shader->ShaderDesc, &shader->Program))
        {
            shader->AttribPTX  = glsl_find_attribute(&shader->ShaderDesc, "aPTX");
            shader->AttribCLR  = glsl_find_attribute(&shader->ShaderDesc, "aCLR");
            shader->UniformMSS = glsl_find_uniform  (&shader->ShaderDesc, "uMSS");
            return true;
        }
        else return false;
    }
    else return false;
}

/// @summary Frees the resources associated with a solid-color sprite shader.
/// @param display The display managing the rendering context.
/// @param shader The shader program object to free.
internal_function void gl_delete_sprite_shader_ptc_clr(gl_display_t *display, gl_sprite_shader_ptc_clr_t *shader)
{
    if (shader && shader->Program)
    {
        glsl_shader_desc_free(&shader->ShaderDesc);
        glDeleteProgram(shader->Program);
        shader->AttribPTX  = NULL;
        shader->AttribCLR  = NULL;
        shader->UniformMSS = NULL;
        shader->Program    = 0;
    }
}

/// @summary Creates a shader program consisting of a vertex and fragment shader used for rendering standard 2D sprites in screen space. The shaders sample image data from a single 2D texture object, and modulate the sample value with the per-sprite tint color.
/// @param display The display managing the rendering context.
/// @param shader The shader state to initialize.
/// @return true if the shader program was created successfully.
internal_function bool gl_create_sprite_shader_ptc_tex(gl_display_t *display, gl_sprite_shader_ptc_tex_t *shader)
{
    if (shader)
    {
        glsl_shader_source_t     sources;
        glsl_shader_source_init(&sources);
        glsl_shader_source_add (&sources, GL_VERTEX_SHADER,   (char**) &SpriteShaderPTC_TEX_VSS_150, 1);
        glsl_shader_source_add (&sources, GL_FRAGMENT_SHADER, (char**) &SpriteShaderPTC_TEX_FSS_150, 1);
        if (glsl_build_shader(display, &sources, &shader->ShaderDesc, &shader->Program))
        {
            shader->AttribPTX  = glsl_find_attribute(&shader->ShaderDesc, "aPTX");
            shader->AttribCLR  = glsl_find_attribute(&shader->ShaderDesc, "aCLR");
            shader->SamplerTEX = glsl_find_sampler  (&shader->ShaderDesc, "sTEX");
            shader->UniformMSS = glsl_find_uniform  (&shader->ShaderDesc, "uMSS");
            return true;
        }
        else return false;
    }
    else return false;
}

/// @summary Frees the resources associated with a textured sprite shader.
/// @param display The display managing the rendering context.
/// @param shader The shader program object to free.
internal_function void gl_delete_sprite_shader_ptc_tex(gl_display_t *display, gl_sprite_shader_ptc_tex_t *shader)
{
    if (shader && shader->Program)
    {
        glsl_shader_desc_free(&shader->ShaderDesc);
        glDeleteProgram(shader->Program);
        shader->AttribPTX  = NULL;
        shader->AttribCLR  = NULL;
        shader->SamplerTEX = NULL;
        shader->UniformMSS = NULL;
        shader->Program    = 0;
    }
}

/*///////////////////////
//  Public Functions   //
///////////////////////*/
/// @summary Creates a new renderer attached to a window. The window is created and managed externally.
/// @param display The display on which the window content is rendered.
/// @param window The window to which output will be presented.
/// @return A handle to the renderer state, or 0.
public_function uintptr_t create_renderer(gl_display_t *display, HWND window)
{
    gl3_renderer_t   *gl  = NULL;
    size_t  target_width  = 0;
    size_t  target_height = 0;
    RECT         rcclient = {};

    GetClientRect(window, &rcclient);
    target_width  = size_t(rcclient.right  - rcclient.left);
    target_height = size_t(rcclient.bottom - rcclient.top );

    // bind the default framebuffer and set the viewport to the entire window bounds.
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glViewport(0, 0, (GLsizei) target_width, (GLsizei) target_height);

    // allocate memory for the renderer state:
    gl = (gl3_renderer_t*)  malloc(sizeof(gl3_renderer_t));

    gl->Width           = target_width;
    gl->Height          = target_height;
    gl->Drawable        = GetDC(window);
    gl->Window          = window;
    gl->Display         = display;

    // initialize the queue for receiving submitted command lists:
    pr_command_queue_init(&gl->CommandQueue);

    // save names of local display resources:
    gl->RenderFlags     = GLRC_RENDER_FLAGS_WINDOW;
    gl->Framebuffer     = 0;
    gl->Renderbuffer[0] = 0;
    gl->Renderbuffer[1] = 0;

    // initialize a dynamic list holding interfaces used to submit 
    // image cache control commands from the presentation thread.
    gl->CacheCount    = 0;
    gl->CacheCapacity = 4;
    gl->CacheList     =(thread_image_cache_t*) malloc(4 * sizeof(thread_image_cache_t));

    // initialize a dynamic list of images referenced by in-flight frames.
    gl->ImageCount    = 0;
    gl->ImageCapacity = 8;
    gl->ImageIds      =(gl_image_id_t       *) malloc(8 * sizeof(gl_image_id_t));
    gl->ImageData     =(gl_image_data_t     *) malloc(8 * sizeof(gl_image_data_t));
    gl->ImageRefs     =(size_t              *) malloc(8 * sizeof(size_t));

    // initialize queues for receiving image data from the host:
    mpsc_fifo_u_init(&gl->ImageLockQueue);
    mpsc_fifo_u_init(&gl->ImageErrorQueue);

    // initialize the queue of in-flight frame state:
    gl->FrameHead = 0;
    gl->FrameTail = 0;
    gl->FrameMask = GLRC_MAX_FRAMES - 1;
    memset(&gl->FrameQueue, 0, GLRC_MAX_FRAMES * sizeof(gl_frame_t));
    return (uintptr_t) gl;
}

/// @summary Creates a new headless renderer. Output is rendered to an offscreen buffer that can be read back by the application.
/// @param display The display used for processing renderer commands.
/// @param target_width The output width, in pixels. Specify 0 to use the display width.
/// @param target_height The output height, in pixels. Specify 0 to use the display height.
/// @param depth_stencil Specify true if a depth or stencil buffer is required for rendering operations.
/// @param multisample_count Specify a value greater than one to create multisample renderbuffers.
/// @return A handle to the renderer state, or 0.
public_function uintptr_t create_renderer(gl_display_t *display, size_t target_width=0, size_t target_height=0, bool depth_stencil=false, size_t multisample_count=1)
{
    gl3_renderer_t *gl    =  NULL;
    GLuint         fbo    =  0;
    GLsizei        rbn    =  depth_stencil ? 2 : 1;
    GLuint         rbo[2] = {0, 0};
    GLsizei        w      = (GLsizei) target_width;
    GLsizei        h      = (GLsizei) target_height;
    GLsizei        s      = (GLsizei) multisample_count;

    if (target_width  == 0) w = (GLsizei) display->DisplayWidth;
    if (target_height == 0) h = (GLsizei) display->DisplayHeight;

    // create and set up a framebuffer and associated renderbuffers:
    glGenFramebuffers (1  , &fbo);
    glGenRenderbuffers(rbn,  rbo);
    glBindFramebuffer (GL_DRAW_FRAMEBUFFER, fbo);
    if (multisample_count > 1)
    {   // allocate storage for the color buffer:
        glBindRenderbuffer(GL_RENDERBUFFER, rbo[0]);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, s, GL_RGBA8, w, h);
        glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo[0]);
        if (depth_stencil)
        {   // allocate storage for the depth+stencil buffer:
            glBindRenderbuffer(GL_RENDERBUFFER, rbo[1]);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, s, GL_DEPTH_STENCIL, w, h);
            glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo[1]);
        }
    }
    else
    {   // allocate storage for the color buffer:
        glBindRenderbuffer(GL_RENDERBUFFER, rbo[0]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, w, h);
        glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo[0]);
        if (depth_stencil)
        {   // allocate storage for the depth+stencil buffer:
            glBindRenderbuffer(GL_RENDERBUFFER, rbo[1]);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_STENCIL, w, h);
            glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo[1]);
        }
    }
    GLenum completeness = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
    if (completeness   != GL_FRAMEBUFFER_COMPLETE)
    {
        OutputDebugString(_T("ERROR: Incomplete framebuffer on display: "));
        OutputDebugString(display->DisplayInfo.DeviceName);
        OutputDebugString(_T(".\n"));
    }

    // default the viewport to the entire render target bounds:
    glViewport(0, 0, w, h);

    // allocate memory for the renderer state:
    gl = (gl3_renderer_t*)  malloc(sizeof(gl3_renderer_t));

    gl->Width           = (size_t) w;
    gl->Height          = (size_t) h;
    gl->Drawable        = display->DisplayDC;
    gl->Window          = display->DisplayHWND;
    gl->Display         = display;

    // initialize the queue for receiving submitted command lists:
    pr_command_queue_init(&gl->CommandQueue);

    // save names of local display resources:
    gl->RenderFlags     = GLRC_RENDER_FLAGS_HEADLESS;
    gl->Framebuffer     = fbo;
    gl->Renderbuffer[0] = rbo[0];
    gl->Renderbuffer[1] = rbo[1];

    // initialize a dynamic list holding interfaces used to submit 
    // image cache control commands from the presentation thread.
    gl->CacheCount    = 0;
    gl->CacheCapacity = 4;
    gl->CacheList     =(thread_image_cache_t*) malloc(4 * sizeof(thread_image_cache_t));

    // initialize a dynamic list of images referenced by in-flight frames.
    gl->ImageCount    = 0;
    gl->ImageCapacity = 8;
    gl->ImageIds      =(gl_image_id_t       *) malloc(8 * sizeof(gl_image_id_t));
    gl->ImageData     =(gl_image_data_t     *) malloc(8 * sizeof(gl_image_data_t));
    gl->ImageRefs     =(size_t              *) malloc(8 * sizeof(size_t));

    // initialize queues for receiving image data from the host:
    mpsc_fifo_u_init(&gl->ImageLockQueue);
    mpsc_fifo_u_init(&gl->ImageErrorQueue);

    // initialize the queue of in-flight frame state:
    gl->FrameHead = 0;
    gl->FrameTail = 0;
    gl->FrameMask = GLRC_MAX_FRAMES - 1;
    memset(&gl->FrameQueue, 0, GLRC_MAX_FRAMES * sizeof(gl_frame_t));
    return (uintptr_t) gl;
}

/// @summary Resets the state of the renderer, keeping it attached to its current display and drawable.
/// @param handle The handle of the renderer to reset.
public_function void reset_renderer(uintptr_t handle)
{
    gl3_renderer_t    *gl = (gl3_renderer_t*) handle;
    GLsizei             w = (GLsizei) gl->Width;
    GLsizei             h = (GLsizei) gl->Height;
    gl_display_t *display =  gl->Display;

    // flush the image command queues:
    mpsc_fifo_u_flush(&gl->ImageErrorQueue);
    mpsc_fifo_u_flush(&gl->ImageLockQueue);
    
    // clear the global image list:
    gl->ImageCount = 0;
    
    // reset the in-flight frame queue:
    gl->FrameHead  = 0;
    gl->FrameTail  = 0;
    for (size_t i  = 0; i < GLRC_MAX_FRAMES; ++i)
    {
        gl->FrameQueue[i].CommandList = NULL;
        gl->FrameQueue[i].FrameState  = GLRC_FRAME_STATE_WAIT_FOR_IMAGES;
        gl->FrameQueue[i].ImageCount  = 0;
    }
    
    // reset the command queue:
    pr_command_queue_clear(&gl->CommandQueue);

    // OpenGL commands target the renderer drawable:
    target_drawable(gl->Display, gl->Drawable);
    
    // reset the framebufer and viewport:
    glBindFramebuffer(GL_DRAW_BUFFER, gl->Framebuffer);
    glViewport(0, 0, w, h);
}

/// @summary Resets the state of the renderer when its attached drawable changes displays.
/// @param handle The handle of the renderer to reset.
/// @param display The display that owns the drawable.
/// @param window The window to which rendered content will be presented.
public_function void reset_renderer(uintptr_t handle, gl_display_t *display, HWND window)
{
    gl3_renderer_t  *gl =(gl3_renderer_t*) handle;

    if (gl->RenderFlags & GLRC_RENDER_FLAGS_HEADLESS)
    {   // OpenGL commands target the active drawable:
        target_drawable(gl->Display, gl->Drawable);

        // free headless display resources:
        if (gl->Framebuffer != 0)
        {
            glDeleteFramebuffers(1, &gl->Framebuffer);
            gl->Framebuffer  = 0;
        }
        if (gl->Renderbuffer[0] != 0)
        {
            glDeleteRenderbuffers(1, &gl->Renderbuffer[0]);
            gl->Renderbuffer[0] = 0;
        }
        if (gl->Renderbuffer[1] != 0)
        {
            glDeleteRenderbuffers(1, &gl->Renderbuffer[1]);
            gl->Renderbuffer[1] = 0;
        }

        // change the renderer type to windowed:
        gl->RenderFlags &=~GLRC_RENDER_FLAGS_HEADLESS;
        gl->RenderFlags |= GLRC_RENDER_FLAGS_WINDOW;

        // NULL-out the window and drawable references.
        // these referenced the old display, but don't have to be released.
        gl->Drawable = NULL;
        gl->Window   = NULL;
    }
    else
    {   // already windowed; release the current display context.
        ReleaseDC(gl->Window, gl->Drawable);
    }

    // retrieve the client bounds of the new window:
    RECT rcclient = {};
    GetClientRect(window, &rcclient);

    // update renderer state:
    gl->Width     = size_t(rcclient.right  - rcclient.left);
    gl->Height    = size_t(rcclient.bottom - rcclient.top);
    gl->Drawable  = GetDC(window);
    gl->Window    = window;
    gl->Display   = display;

    // reset the remaining renderer state:
    reset_renderer(handle);
}

/// @summary Allocates a new command list for use by the application. The application should write display commands to the command list and then submit the list for processing by the presentation driver.
/// @param handle The renderer handle returned by one of the create_renderer functions.
/// @return An empty command list, or NULL if no command lists are available.
public_function pr_command_list_t* renderer_command_list(uintptr_t handle)
{
    return pr_command_queue_next_available(&((gl3_renderer_t*)handle)->CommandQueue);
}

/// @summary Submits a command list, representing a single frame, to the renderer.
/// @param handle The renderer handle, returned by one of the create_renderer functions.
/// @param cmdlist The command list to submit to the renderer. The end-of-frame command is automatically appended.
/// @return A wait handle that can be used to wait for the renderer to process the command list.
public_function HANDLE renderer_submit(uintptr_t handle, pr_command_list_t *cmdlist)
{
    gl3_renderer_t   *gl = (gl3_renderer_t*) handle;
    HANDLE          sync =  cmdlist->SyncHandle;
    pr_command_end_of_frame(cmdlist);
    pr_command_queue_submit(&gl->CommandQueue, cmdlist);
    return sync;
}

/// @summary Processes command lists representing the next frame and submits commands to the GPU.
/// @param handle The renderer handle, returned by one of the create_renderer functions.
public_function void update_drawable(uintptr_t handle)
{
    gl3_renderer_t    *gl = (gl3_renderer_t*) handle;
    gl_display_t *display =  gl->Display;
    RECT         rcclient = {0, 0, (LONG) gl->Width, (LONG) gl->Height}; 

    if (gl->RenderFlags & GLRC_RENDER_FLAGS_WINDOW)
    {   // retrieve the current window bounds.
        GetClientRect(gl->Window, &rcclient);
        gl->Width   = size_t(rcclient.right  - rcclient.left);
        gl->Height  = size_t(rcclient.bottom - rcclient.top );
    }

    // OpenGL commands target the current drawable:
    target_drawable(gl->Display, gl->Drawable);

    // bind the output renderbuffer, and set the viewport to the entire region:
    GLsizei w = (GLsizei) gl->Width;
    GLsizei h = (GLsizei) gl->Height;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, gl->Framebuffer);
    glViewport(0, 0, w, h);

    // launch as many frames as possible:
    while (frame_count(gl) < GLRC_MAX_FRAMES)
    {
        pr_command_list_t *cmdlist = pr_command_queue_next_submitted(&gl->CommandQueue);
        if (cmdlist == NULL) break;  // no more pending frames
        launch_frame(gl, cmdlist);
    }

    // process any pending image lock errors:
    image_cache_error_t lock_error;
    while (mpsc_fifo_u_consume(&gl->ImageErrorQueue, lock_error))
    {   // locate the image in the global image list:
        for (size_t gi = 0, gn = gl->ImageCount; gi < gn; ++gi)
        {
            if (gl->ImageIds [gi].ImageId    == lock_error.ImageId && 
                gl->ImageIds [gi].FrameIndex == lock_error.FirstFrame)
            {   // save the error code. it will be checked later.
                gl->ImageData[gi].ErrorCode   = lock_error.ErrorCode;
                break;
            }
        }
    }

    // process any completed image locks:
    image_cache_result_t lock_result;
    while (mpsc_fifo_u_consume(&gl->ImageLockQueue, lock_result))
    {   // locate the image in the global image list:
        for (size_t gi = 0, gn = gl->ImageCount; gi < gn; ++gi)
        {
            if (gl->ImageIds [gi].ImageId    == lock_result.ImageId && 
                gl->ImageIds [gi].FrameIndex == lock_result.FrameIndex)
            {   // save the image metadata and host memory pointer.
                gl->ImageData[gi].ErrorCode         = ERROR_SUCCESS;
                gl->ImageData[gi].SourceFormat      = lock_result.ImageFormat;
                gl->ImageData[gi].SourceCompression = lock_result.Compression;
                gl->ImageData[gi].SourceEncoding    = lock_result.Encoding;
                gl->ImageData[gi].SourceWidth       = lock_result.LevelInfo[0].Width;
                gl->ImageData[gi].SourceHeight      = lock_result.LevelInfo[0].Height;
                gl->ImageData[gi].SourcePitch       = lock_result.LevelInfo[0].BytesPerRow;
                gl->ImageData[gi].SourceData        =(uint8_t*) lock_result.BaseAddress;
                gl->ImageData[gi].SourceSize        = lock_result.BytesReserved;
                break;
            }
        }
    }
    
    // update the state of all in-flight frames:
    size_t frame_id  = gl->FrameHead;
    while (frame_count(gl) > 0 &&  frame_id != gl->FrameTail)
    {
        size_t fifo_index  = size_t(frame_id & gl->FrameMask);
        gl_frame_t *frame  = &gl->FrameQueue[fifo_index];

        if (frame->FrameState == GLRC_FRAME_STATE_WAIT_FOR_IMAGES)
        {   // have all pending image locks completed successfully?
            size_t n_expected  = frame->ImageCount;
            size_t n_success   = 0;
            size_t n_error     = 0;
            for (size_t li = 0, ln = frame->ImageCount; li < ln; ++li)
            {   // locate this frame in the global image list.
                for (size_t gi = 0, gn = gl->ImageCount; gi < gn; ++gi)
                {
                    if (frame->ImageIds[li].ImageId     == gl->ImageIds[gi].ImageId && 
                        frame->ImageIds[li].FrameIndex  == gl->ImageIds[gi].FrameIndex)
                    {
                        if (gl->ImageData[gi].ErrorCode != ERROR_SUCCESS)
                        {   // the image could not be locked in host memory.
                            n_error++;
                        }
                        else if (gl->ImageData[gi].SourceData != NULL)
                        {   // the image was locked in host memory and is ready for use.
                            n_success++;
                        }
                        // else, the lock request hasn't completed yet.
                        break;
                    }
                }
            }
            if (n_success == n_expected)
            {   // all images are ready, wait for compute jobs.
                frame->FrameState = GLRC_FRAME_STATE_WAIT_FOR_COMPUTE;
            }
            if (n_error > 0)
            {   // at least one image could not be locked. complete immediately.
                frame->FrameState = GLRC_FRAME_STATE_ERROR;
            }
        }

        // TODO(rlk): launch any compute jobs whose dependencies are satisfied. 
        
        if (frame->FrameState == GLRC_FRAME_STATE_WAIT_FOR_COMPUTE)
        {   // have all pending compute jobs completed successfully?
            // TODO(rlk): when we have compute jobs implemented...
            frame->FrameState  = GLRC_FRAME_STATE_WAIT_FOR_DISPLAY;
        }

        // TODO(rlk): launch any display jobs whose dependencies are satisfied.

        if (frame->FrameState == GLRC_FRAME_STATE_WAIT_FOR_DISPLAY)
        {   // have all pending display jobs completed successfully?
            // TODO(rlk): when we have display jobs implemented...
            frame->FrameState  = GLRC_FRAME_STATE_COMPLETE;
        }

        if (frame->FrameState == GLRC_FRAME_STATE_COMPLETE)
        {   // there's no work remaining for this frame.
        }
        
        if (frame->FrameState == GLRC_FRAME_STATE_ERROR)
        {   // TODO(rlk): cancel any in-progress jobs.
        }

        // check the next pending frame.
        ++frame_id;
    }

    // if the oldest frame has completed, retire it.
    if (frame_count(gl) > 0)
    {
        size_t oldest_index = size_t(gl->FrameHead & gl->FrameMask);
        gl_frame_t   *frame =&gl->FrameQueue[oldest_index];

        if (frame->FrameState == GLRC_FRAME_STATE_COMPLETE)
        {   // execute the display list. all data is ready.
            bool       end_of_frame = false;
            uint8_t const *read_ptr = frame->CommandList->CommandData;
            uint8_t const *end_ptr  = frame->CommandList->CommandData + frame->CommandList->BytesUsed;
            while (read_ptr < end_ptr && !end_of_frame)
            {
                pr_command_t *cmd_info = (pr_command_t*) read_ptr;
                size_t        cmd_size =  PR_COMMAND_SIZE_BASE + cmd_info->DataSize;
                switch (cmd_info->CommandId)
                {
                case PR_COMMAND_END_OF_FRAME:
                    {   // break out of the command processing loop and submit
                        // submit the translated command list to the system driver.
                        end_of_frame = true;
                    }
                    break;
                case PR_COMMAND_CLEAR_COLOR_BUFFER:
                    {
                        pr_color_t  *c = (pr_color_t*) cmd_info->Data;
                        glClearColor(c->Red, c->Green, c->Blue, c->Alpha);
                        glClear(GL_COLOR_BUFFER_BIT);
                    }
                    break;
                }
                // advance to the start of the next buffered command.
                read_ptr += cmd_size;
            }
            // signal to any waiters that processing of the command list has finished.
            SetEvent(frame->CommandList->SyncHandle);
        }
        if (frame->FrameState == GLRC_FRAME_STATE_COMPLETE || 
            frame->FrameState == GLRC_FRAME_STATE_ERROR)
        {
            retire_frame(gl);
        }
    }
}

/// @summary Copies or swaps the backbuffer to the front buffer.
/// @param handle The handle of the renderer associated with the window.
public_function void present_drawable(uintptr_t handle)
{
    gl3_renderer_t  *gl = (gl3_renderer_t*) handle;
    if (gl->RenderFlags & GLRC_RENDER_FLAGS_WINDOW)
    {   // only renderers attached to a window need to swap buffers.
        swap_buffers(gl->Drawable);
    }
}

/// @summary Frees resources associated with a renderer and detaches it from its drawable.
/// @param handle The renderer handle, returned by one of the create_renderer functions.
public_function void delete_renderer(uintptr_t handle)
{
    gl3_renderer_t     *gl = (gl3_renderer_t*) handle;
    gl_display_t  *display =  gl->Display;

    // TODO(rlk): The display manages all of the shared resources.

    // free resources associated with the frame queue:
    for (size_t i = 0; i < GLRC_MAX_FRAMES; ++i)
    {
        gl_frame_t &frame = gl->FrameQueue[i];
        free(frame.ImageIds);
    }
    
    // free resources associated with the image queues:
    mpsc_fifo_u_delete(&gl->ImageErrorQueue);
    mpsc_fifo_u_delete(&gl->ImageLockQueue);

    // free resources associated with the global image list:
    free(gl->ImageRefs);
    free(gl->ImageData);
    free(gl->ImageIds);

    // free resources associated with the image cache list:
    for (size_t i = 0, n = gl->CacheCount; i < n; ++i)
    {
        gl->CacheList[i].dispose();
    }
    free(gl->CacheList);

    // OpenGL commands target the current drawable:
    target_drawable(gl->Display, gl->Drawable);

    // delete private display resources:
    if (gl->Framebuffer != 0)
    {
        glDeleteFramebuffers(1, &gl->Framebuffer);
        gl->Framebuffer  = 0;
    }
    if (gl->Renderbuffer[0] != 0)
    {
        glDeleteRenderbuffers(1, &gl->Renderbuffer[0]);
        gl->Renderbuffer[0] = 0;
    }
    if (gl->Renderbuffer[1] != 0)
    {
        glDeleteRenderbuffers(1, &gl->Renderbuffer[1]);
        gl->Renderbuffer[1] = 0;
    }
    if (gl->RenderFlags & GLRC_RENDER_FLAGS_WINDOW)
    {   // release the drawable reference:
        ReleaseDC(gl->Window, gl->Drawable);
    }

    // free the command queue, and then the render state memory itself:
    pr_command_queue_delete(&gl->CommandQueue);
    free(gl);
}
