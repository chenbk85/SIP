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
/// @summary Defines the location of the position and texture attributes within
/// the position-texture-color vertex. These attributes are encoded as a vec4.
#ifndef GL_SPRITE_PTC_LOCATION_PTX
#define GL_SPRITE_PTC_LOCATION_PTX            (0)
#endif

/// @summary Defines the location of the tint color attribute within the vertex.
/// This attribute is encoded as a packed 32-bit RGBA color value.
#ifndef GL_SPRITE_PTC_LOCATION_CLR
#define GL_SPRITE_PTC_LOCATION_CLR            (1)
#endif

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

/// @summary Select the appropriate shader source code based on the target OpenGL version.
#if     GL_DISPLAY_VERSION == OPENGL_VERSION_32
#define SpriteShaderPTC_CLR_VSS               SpriteShaderPTC_CLR_VSS_150
#define SpriteShaderPTC_CLR_FSS               SpriteShaderPTC_CLR_FSS_150
#define SpriteShaderPTC_TEX_VSS               SpriteShaderPTC_TEX_VSS_150
#define SpriteShaderPTC_TEX_FSS               SpriteShaderPTC_TEX_FSS_150
#elif   GL_DISPLAY_VERSION == OPENGL_VERSION_33
#define SpriteShaderPTC_CLR_VSS               SpriteShaderPTC_CLR_VSS_330
#define SpriteShaderPTC_CLR_FSS               SpriteShaderPTC_CLR_FSS_330
#define SpriteShaderPTC_TEX_VSS               SpriteShaderPTC_TEX_VSS_330
#define SpriteShaderPTC_TEX_FSS               SpriteShaderPTC_TEX_FSS_330
#else
#error  No sprite shader implementation for your OpenGL version!
#endif

/*///////////////////
//   Local Types   //
///////////////////*/
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

/*//////////////
//  Functors  //
//////////////*/
/// @summary Functor used for sorting sprites into back-to-front order.
struct gl_sprite_back_to_front
{
    gl_sprite_batch_t *batch;

    inline gl_sprite_back_to_front(gl_sprite_batch_t *sprite_batch)
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
struct gl_sprite_front_to_back
{
    gl_sprite_batch_t *batch;

    inline gl_sprite_front_to_back(gl_sprite_batch_t *sprite_batch)
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
struct gl_sprite_by_render_state
{
    gl_sprite_batch_t *batch;

    inline gl_sprite_by_render_state(gl_sprite_batch_t *sprite_batch)
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

/*///////////////////////
//  Public Functions   //
///////////////////////*/
/// @summary Initializes a sprite batch with the specified capacity.
/// @param batch The sprite batch.
/// @param capacity The initial capacity of the batch, in quads.
public_function void gl_create_sprite_batch(gl_sprite_batch_t *batch, size_t capacity)
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
public_function void gl_delete_sprite_batch(gl_sprite_batch_t *batch)
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
public_function void gl_ensure_sprite_batch(gl_sprite_batch_t *batch, size_t capacity)
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
public_function void gl_flush_sprite_batch(gl_sprite_batch_t *batch)
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
public_function void gl_generate_quads(gl_sprite_quad_t *quads, gl_sprite_sort_data_t *sdata, uint32_t *indices, size_t quad_offset, gl_sprite_t const *sprites, size_t sprite_offset, size_t sprite_count)
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
public_function void gl_generate_quad_vertices_ptc(void *buffer, size_t buffer_offset, gl_sprite_quad_t const *quads, uint32_t const *indices, size_t quad_offset, size_t quad_count)
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
public_function void gl_generate_quad_indices_u16(void *buffer, size_t offset, size_t base_vertex, size_t quad_count)
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
public_function void gl_generate_quad_indices_u32(void *buffer, size_t offset, size_t base_vertex, size_t quad_count)
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
public_function bool gl_create_sprite_effect(gl_display_t *display, gl_sprite_effect_t *effect, size_t quad_count, size_t vertex_size, size_t index_size)
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
public_function void gl_delete_sprite_effect(gl_display_t *display, gl_sprite_effect_t *effect)
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
public_function void gl_sprite_effect_blend_none(gl_sprite_effect_t *effect)
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
public_function void gl_sprite_effect_blend_alpha(gl_sprite_effect_t *effect)
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
public_function void gl_sprite_effect_blend_additive(gl_sprite_effect_t *effect)
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
public_function void gl_sprite_effect_blend_premultiplied(gl_sprite_effect_t *effect)
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
public_function void gl_sprite_effect_set_viewport(gl_sprite_effect_t *effect, int width, int height)
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
public_function void gl_sprite_effect_bind_buffers(gl_display_t *display, gl_sprite_effect_t *effect)
{
    glBindVertexArray(effect->VertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, effect->VertexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, effect->IndexBuffer);
}

/// @summary Applies the alpha blending state specified by the effect.
/// @param display The display managing the rendering context.
/// @param effect The effect being applied.
public_function void gl_sprite_effect_apply_blendstate(gl_display_t *display, gl_sprite_effect_t *effect)
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
public_function void gl_sprite_effect_setup_vao_ptc(gl_display_t *display, gl_sprite_effect_t *effect)
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
public_function size_t gl_sprite_effect_buffer_data_ptc(gl_display_t *display, gl_sprite_effect_t *effect, gl_sprite_quad_t const *quads, uint32_t const *indices, size_t quad_offset, size_t quad_count, size_t *base_index_arg)
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
public_function void gl_sprite_effect_draw_batch_region_ptc(gl_display_t *display, gl_sprite_effect_t *effect, gl_sprite_batch_t *batch, size_t quad_offset, size_t quad_count, size_t base_index, gl_sprite_effect_apply_t const *fxfuncs, void *context)
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
public_function void gl_sprite_effect_draw_batch_ptc(gl_display_t *display, gl_sprite_effect_t *effect, gl_sprite_batch_t *batch, gl_sprite_effect_apply_t const *fxfuncs, void *context)
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
public_function bool gl_create_sprite_shader_ptc_clr(gl_display_t *display, gl_sprite_shader_ptc_clr_t *shader)
{
    if (shader)
    {
        glsl_shader_source_t     sources;
        glsl_shader_source_init(&sources);
        glsl_shader_source_add (&sources, GL_VERTEX_SHADER,   (char**) &SpriteShaderPTC_CLR_VSS, 1);
        glsl_shader_source_add (&sources, GL_FRAGMENT_SHADER, (char**) &SpriteShaderPTC_CLR_FSS, 1);
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
public_function void gl_delete_sprite_shader_ptc_clr(gl_display_t *display, gl_sprite_shader_ptc_clr_t *shader)
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
public_function bool gl_create_sprite_shader_ptc_tex(gl_display_t *display, gl_sprite_shader_ptc_tex_t *shader)
{
    if (shader)
    {
        glsl_shader_source_t     sources;
        glsl_shader_source_init(&sources);
        glsl_shader_source_add (&sources, GL_VERTEX_SHADER,   (char**) &SpriteShaderPTC_TEX_VSS, 1);
        glsl_shader_source_add (&sources, GL_FRAGMENT_SHADER, (char**) &SpriteShaderPTC_TEX_FSS, 1);
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
public_function void gl_delete_sprite_shader_ptc_tex(gl_display_t *display, gl_sprite_shader_ptc_tex_t *shader)
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
