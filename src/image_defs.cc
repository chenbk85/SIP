/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines constants, types and functions for working with images. 
/// Image data is described using DirectDraw surface types and DXGI formats, 
/// which support 1D, 2D, 3D and cubemap images, with or without mip-chains, 
/// as well as image arrays (also used for multi-frame image definitions.)
///////////////////////////////////////////////////////////////////////////80*/

/*////////////////
//   Includes   //
////////////////*/

/*////////////////////
//   Preprocessor   //
////////////////////*/
/// @summary If Direct3D headers have been included, don't re-define the 
/// enumerations exposed by DirectDraw and Direct3D relating to DDS files.
#ifdef  DXGI_FORMAT_DEFINED
#define IMAGE_DEFS_DIRECTX_ENUMS_DEFINED 1
#endif

/*/////////////////
//   Constants   //
/////////////////*/

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Only define DirectDraw and Direct3D constants relating to the DDS 
/// image format if they haven't already been defined by including system headers.
#ifndef IMAGE_DEFS_DIRECTX_ENUMS_DEFINED

/// @summary Bitflags for dds_pixelformat_t::Flags. See MSDN documentation at:
/// http://msdn.microsoft.com/en-us/library/windows/desktop/bb943984(v=vs.85).aspx
/// for the DDS_PIXELFORMAT structure.
enum dds_pixelformat_flags_e : uint32_t
{
    DDPF_NONE                   = 0x00000000U,
    DDPF_ALPHAPIXELS            = 0x00000001U,
    DDPF_ALPHA                  = 0x00000002U,
    DDPF_FOURCC                 = 0x00000004U,
    DDPF_RGB                    = 0x00000040U,
    DDPF_YUV                    = 0x00000200U,
    DDPF_LUMINANCE              = 0x00020000U
};

/// @summary Bitflags for dds_header_t::Flags. See MSDN documentation at:
/// http://msdn.microsoft.com/en-us/library/windows/desktop/bb943982(v=vs.85).aspx
/// for the DDS_HEADER structure.
enum dds_header_flags_e : uint32_t
{
    DDSD_NONE                   = 0x00000000U,
    DDSD_CAPS                   = 0x00000001U,
    DDSD_HEIGHT                 = 0x00000002U,
    DDSD_WIDTH                  = 0x00000004U,
    DDSD_PITCH                  = 0x00000008U,
    DDSD_PIXELFORMAT            = 0x00001000U,
    DDSD_MIPMAPCOUNT            = 0x00020000U,
    DDSD_LINEARSIZE             = 0x00080000U,
    DDSD_DEPTH                  = 0x00800000U,
    DDS_HEADER_FLAGS_TEXTURE    = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT,
    DDS_HEADER_FLAGS_MIPMAP     = DDSD_MIPMAPCOUNT,
    DDS_HEADER_FLAGS_VOLUME     = DDSD_DEPTH,
    DDS_HEADER_FLAGS_PITCH      = DDSD_PITCH,
    DDS_HEADER_FLAGS_LINEARSIZE = DDSD_LINEARSIZE
};

/// @summary Bitflags for dds_header_t::Caps. See MSDN documentation at:
/// http://msdn.microsoft.com/en-us/library/windows/desktop/bb943982(v=vs.85).aspx
/// for the DDS_HEADER structure.
enum dds_caps_e : uint32_t
{
    DDSCAPS_NONE                = 0x00000000U,
    DDSCAPS_COMPLEX             = 0x00000008U,
    DDSCAPS_TEXTURE             = 0x00001000U,
    DDSCAPS_MIPMAP              = 0x00400000U,
    DDS_SURFACE_FLAGS_MIPMAP    = DDSCAPS_COMPLEX | DDSCAPS_MIPMAP,
    DDS_SURFACE_FLAGS_TEXTURE   = DDSCAPS_TEXTURE,
    DDS_SURFACE_FLAGS_CUBEMAP   = DDSCAPS_COMPLEX
};

/// @summary Bitflags for dds_header_t::Caps2. See MSDN documentation at:
/// http://msdn.microsoft.com/en-us/library/windows/desktop/bb943982(v=vs.85).aspx
/// for the DDS_HEADER structure.
enum dds_caps2_e : uint32_t
{
    DDSCAPS2_NONE               = 0x00000000U,
    DDSCAPS2_CUBEMAP            = 0x00000200U,
    DDSCAPS2_CUBEMAP_POSITIVEX  = 0x00000400U,
    DDSCAPS2_CUBEMAP_NEGATIVEX  = 0x00000800U,
    DDSCAPS2_CUBEMAP_POSITIVEY  = 0x00001000U,
    DDSCAPS2_CUBEMAP_NEGATIVEY  = 0x00002000U,
    DDSCAPS2_CUBEMAP_POSITIVEZ  = 0x00004000U,
    DDSCAPS2_CUBEMAP_NEGATIVEZ  = 0x00008000U,
    DDSCAPS2_VOLUME             = 0x00200000U,
    DDS_FLAG_VOLUME             = DDSCAPS2_VOLUME,
    DDS_CUBEMAP_POSITIVEX       = DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEX,
    DDS_CUBEMAP_NEGATIVEX       = DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEX,
    DDS_CUBEMAP_POSITIVEY       = DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEY,
    DDS_CUBEMAP_NEGATIVEY       = DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEY,
    DDS_CUBEMAP_POSITIVEZ       = DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEZ,
    DDS_CUBEMAP_NEGATIVEZ       = DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEZ,
    DDS_CUBEMAP_ALLFACES        = DDSCAPS2_CUBEMAP |
                                  DDSCAPS2_CUBEMAP_POSITIVEX |
                                  DDSCAPS2_CUBEMAP_NEGATIVEX |
                                  DDSCAPS2_CUBEMAP_POSITIVEY |
                                  DDSCAPS2_CUBEMAP_NEGATIVEY |
                                  DDSCAPS2_CUBEMAP_POSITIVEZ |
                                  DDSCAPS2_CUBEMAP_NEGATIVEZ
};

/// @summary Bitflags for dds_header_t::Caps3. See MSDN documentation at:
/// http://msdn.microsoft.com/en-us/library/windows/desktop/bb943982(v=vs.85).aspx
/// for the DDS_HEADER structure.
enum dds_caps3_e : uint32_t
{
    DDSCAPS3_NONE = 0x00000000U
};

/// @summary Bitflags for dds_header_t::Caps4. See MSDN documentation at:
/// http://msdn.microsoft.com/en-us/library/windows/desktop/bb943982(v=vs.85).aspx
/// for the DDS_HEADER structure.
enum dds_caps4_e
{
    DDSCAPS4_NONE = 0x00000000U
};

/// @summary Values for dds_header_dxt10_t::Format. See MSDN documentation at:
/// http://msdn.microsoft.com/en-us/library/windows/desktop/bb173059(v=vs.85).aspx
enum dxgi_format_e
{
    DXGI_FORMAT_UNKNOWN                     = 0,
    DXGI_FORMAT_R32G32B32A32_TYPELESS       = 1,
    DXGI_FORMAT_R32G32B32A32_FLOAT          = 2,
    DXGI_FORMAT_R32G32B32A32_UINT           = 3,
    DXGI_FORMAT_R32G32B32A32_SINT           = 4,
    DXGI_FORMAT_R32G32B32_TYPELESS          = 5,
    DXGI_FORMAT_R32G32B32_FLOAT             = 6,
    DXGI_FORMAT_R32G32B32_UINT              = 7,
    DXGI_FORMAT_R32G32B32_SINT              = 8,
    DXGI_FORMAT_R16G16B16A16_TYPELESS       = 9,
    DXGI_FORMAT_R16G16B16A16_FLOAT          = 10,
    DXGI_FORMAT_R16G16B16A16_UNORM          = 11,
    DXGI_FORMAT_R16G16B16A16_UINT           = 12,
    DXGI_FORMAT_R16G16B16A16_SNORM          = 13,
    DXGI_FORMAT_R16G16B16A16_SINT           = 14,
    DXGI_FORMAT_R32G32_TYPELESS             = 15,
    DXGI_FORMAT_R32G32_FLOAT                = 16,
    DXGI_FORMAT_R32G32_UINT                 = 17,
    DXGI_FORMAT_R32G32_SINT                 = 18,
    DXGI_FORMAT_R32G8X24_TYPELESS           = 19,
    DXGI_FORMAT_D32_FLOAT_S8X24_UINT        = 20,
    DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS    = 21,
    DXGI_FORMAT_X32_TYPELESS_G8X24_UINT     = 22,
    DXGI_FORMAT_R10G10B10A2_TYPELESS        = 23,
    DXGI_FORMAT_R10G10B10A2_UNORM           = 24,
    DXGI_FORMAT_R10G10B10A2_UINT            = 25,
    DXGI_FORMAT_R11G11B10_FLOAT             = 26,
    DXGI_FORMAT_R8G8B8A8_TYPELESS           = 27,
    DXGI_FORMAT_R8G8B8A8_UNORM              = 28,
    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB         = 29,
    DXGI_FORMAT_R8G8B8A8_UINT               = 30,
    DXGI_FORMAT_R8G8B8A8_SNORM              = 31,
    DXGI_FORMAT_R8G8B8A8_SINT               = 32,
    DXGI_FORMAT_R16G16_TYPELESS             = 33,
    DXGI_FORMAT_R16G16_FLOAT                = 34,
    DXGI_FORMAT_R16G16_UNORM                = 35,
    DXGI_FORMAT_R16G16_UINT                 = 36,
    DXGI_FORMAT_R16G16_SNORM                = 37,
    DXGI_FORMAT_R16G16_SINT                 = 38,
    DXGI_FORMAT_R32_TYPELESS                = 39,
    DXGI_FORMAT_D32_FLOAT                   = 40,
    DXGI_FORMAT_R32_FLOAT                   = 41,
    DXGI_FORMAT_R32_UINT                    = 42,
    DXGI_FORMAT_R32_SINT                    = 43,
    DXGI_FORMAT_R24G8_TYPELESS              = 44,
    DXGI_FORMAT_D24_UNORM_S8_UINT           = 45,
    DXGI_FORMAT_R24_UNORM_X8_TYPELESS       = 46,
    DXGI_FORMAT_X24_TYPELESS_G8_UINT        = 47,
    DXGI_FORMAT_R8G8_TYPELESS               = 48,
    DXGI_FORMAT_R8G8_UNORM                  = 49,
    DXGI_FORMAT_R8G8_UINT                   = 50,
    DXGI_FORMAT_R8G8_SNORM                  = 51,
    DXGI_FORMAT_R8G8_SINT                   = 52,
    DXGI_FORMAT_R16_TYPELESS                = 53,
    DXGI_FORMAT_R16_FLOAT                   = 54,
    DXGI_FORMAT_D16_UNORM                   = 55,
    DXGI_FORMAT_R16_UNORM                   = 56,
    DXGI_FORMAT_R16_UINT                    = 57,
    DXGI_FORMAT_R16_SNORM                   = 58,
    DXGI_FORMAT_R16_SINT                    = 59,
    DXGI_FORMAT_R8_TYPELESS                 = 60,
    DXGI_FORMAT_R8_UNORM                    = 61,
    DXGI_FORMAT_R8_UINT                     = 62,
    DXGI_FORMAT_R8_SNORM                    = 63,
    DXGI_FORMAT_R8_SINT                     = 64,
    DXGI_FORMAT_A8_UNORM                    = 65,
    DXGI_FORMAT_R1_UNORM                    = 66,
    DXGI_FORMAT_R9G9B9E5_SHAREDEXP          = 67,
    DXGI_FORMAT_R8G8_B8G8_UNORM             = 68,
    DXGI_FORMAT_G8R8_G8B8_UNORM             = 69,
    DXGI_FORMAT_BC1_TYPELESS                = 70,
    DXGI_FORMAT_BC1_UNORM                   = 71,
    DXGI_FORMAT_BC1_UNORM_SRGB              = 72,
    DXGI_FORMAT_BC2_TYPELESS                = 73,
    DXGI_FORMAT_BC2_UNORM                   = 74,
    DXGI_FORMAT_BC2_UNORM_SRGB              = 75,
    DXGI_FORMAT_BC3_TYPELESS                = 76,
    DXGI_FORMAT_BC3_UNORM                   = 77,
    DXGI_FORMAT_BC3_UNORM_SRGB              = 78,
    DXGI_FORMAT_BC4_TYPELESS                = 79,
    DXGI_FORMAT_BC4_UNORM                   = 80,
    DXGI_FORMAT_BC4_SNORM                   = 81,
    DXGI_FORMAT_BC5_TYPELESS                = 82,
    DXGI_FORMAT_BC5_UNORM                   = 83,
    DXGI_FORMAT_BC5_SNORM                   = 84,
    DXGI_FORMAT_B5G6R5_UNORM                = 85,
    DXGI_FORMAT_B5G5R5A1_UNORM              = 86,
    DXGI_FORMAT_B8G8R8A8_UNORM              = 87,
    DXGI_FORMAT_B8G8R8X8_UNORM              = 88,
    DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM  = 89,
    DXGI_FORMAT_B8G8R8A8_TYPELESS           = 90,
    DXGI_FORMAT_B8G8R8A8_UNORM_SRGB         = 91,
    DXGI_FORMAT_B8G8R8X8_TYPELESS           = 92,
    DXGI_FORMAT_B8G8R8X8_UNORM_SRGB         = 93,
    DXGI_FORMAT_BC6H_TYPELESS               = 94,
    DXGI_FORMAT_BC6H_UF16                   = 95,
    DXGI_FORMAT_BC6H_SF16                   = 96,
    DXGI_FORMAT_BC7_TYPELESS                = 97,
    DXGI_FORMAT_BC7_UNORM                   = 98,
    DXGI_FORMAT_BC7_UNORM_SRGB              = 99,
    DXGI_FORMAT_AYUV                        = 100,
    DXGI_FORMAT_Y410                        = 101,
    DXGI_FORMAT_Y416                        = 102,
    DXGI_FORMAT_NV12                        = 103,
    DXGI_FORMAT_P010                        = 104,
    DXGI_FORMAT_P016                        = 105,
    DXGI_FORMAT_420_OPAQUE                  = 106,
    DXGI_FORMAT_YUY2                        = 107,
    DXGI_FORMAT_Y210                        = 108,
    DXGI_FORMAT_Y216                        = 109,
    DXGI_FORMAT_NV11                        = 110,
    DXGI_FORMAT_AI44                        = 111,
    DXGI_FORMAT_IA44                        = 112,
    DXGI_FORMAT_P8                          = 113,
    DXGI_FORMAT_A8P8                        = 114,
    DXGI_FORMAT_B4G4R4A4_UNORM              = 115,
    DXGI_FORMAT_FORCE_UINT                  = 0xFFFFFFFFU
};

/// @summary Values for dds_header_dxt10_t::Dimension. See MSDN documentation at:
/// http://msdn.microsoft.com/en-us/library/windows/desktop/bb943983(v=vs.85).aspx
/// for the DDS_HEADER_DXT10 structure.
enum d3d11_resource_dimension_e
{
    D3D11_RESOURCE_DIMENSION_UNKNOWN        = 0,
    D3D11_RESOURCE_DIMENSION_BUFFER         = 1,
    D3D11_RESOURCE_DIMENSION_TEXTURE1D      = 2,
    D3D11_RESOURCE_DIMENSION_TEXTURE2D      = 3,
    D3D11_RESOURCE_DIMENSION_TEXTURE3D      = 4
};

/// @summary Values for dds_header_dxt10_t::Flags. See MSDN documentation at:
/// http://msdn.microsoft.com/en-us/library/windows/desktop/bb943983(v=vs.85).aspx
/// for the DDS_HEADER_DXT10 structure.
enum d3d11_resource_misc_flag_e
{
    D3D11_RESOURCE_MISC_TEXTURECUBE         = 0x00000004U,
};

/// @summary Values for dds_header_dxt10_t::Flags2. See MSDN documentation at:
/// http://msdn.microsoft.com/en-us/library/windows/desktop/bb943983(v=vs.85).aspx
/// for the DDS_HEADER_DXT10 structure.
enum dds_alpha_mode_e
{
    DDS_ALPHA_MODE_UNKNOWN                  = 0x00000000U,
    DDS_ALPHA_MODE_STRAIGHT                 = 0x00000001U,
    DDS_ALPHA_MODE_PREMULTIPLIED            = 0x00000002U,
    DDS_ALPHA_MODE_OPAQUE                   = 0x00000003U,
    DDS_ALPHA_MODE_CUSTOM                   = 0x00000004U
};

#endif /* !defined(IMAGE_DEFS_DIRECTX_ENUMS_DEFINED) */



/// @summary Define the recognized image encodings. The image encoding allows 
/// data to be uploaded to image memory in a non-native format, for example, 
/// RLE-encoded JPEG blocks, and decompressed via compute kernel or shader program.
enum image_encoding_e
{
    IMAGE_ENCODING_RAW             = 0,  /// The image is encoded in a DXGI format.
};



/// @summary The equivalent of the DDS_PIXELFORMAT structure. See MSDN at:
/// http://msdn.microsoft.com/en-us/library/windows/desktop/bb943984(v=vs.85).aspx
#pragma pack(push, 1)
struct dds_pixelformat_t
{
    uint32_t            Size;            /// The size of the structure (32 bytes).
    uint32_t            Flags;           /// Combination of dds_pixelformat_flags_e.
    uint32_t            FourCC;          /// DXT1, DXT2, DXT3, DXT4, DXT5 or DX10. See MSDN.
    uint32_t            RGBBitCount;     /// The number of bits per-pixel.
    uint32_t            BitMaskR;        /// Mask for reading red/luminance/Y data.
    uint32_t            BitMaskG;        /// Mask for reading green/U data.
    uint32_t            BitMaskB;        /// Mask for reading blue/V data.
    uint32_t            BitMaskA;        /// Mask for reading alpha channel data.
};
#pragma pack(pop)

/// @summary The equivalent of the DDS_HEADER structure. See MSDN at:
/// http://msdn.microsoft.com/en-us/library/windows/desktop/bb943982(v=vs.85).aspx
#pragma pack(push, 1)
struct dds_header_t
{
    uint32_t            Size;            /// The size of the structure (124 bytes).
    uint32_t            Flags;           /// Combination of dds_header_flags_e.
    uint32_t            Height;          /// The surface height, in pixels.
    uint32_t            Width;           /// The surface width, in pixels.
    uint32_t            Pitch;           /// Bytes per-scanline, or bytes in top-level (compressed).
    uint32_t            Depth;           /// The surface depth, in slices. For non-volume surfaces, 0.
    uint32_t            Levels;          /// The number of mipmap levels, or 0 if there are no mipmaps.
    uint32_t            Reserved1[11];   /// Reserved for future use.
    dds_pixelformat_t   Format;          /// Pixel format descriptor.
    uint32_t            Caps;            /// Combination of dds_caps_e.
    uint32_t            Caps2;           /// Combination of dds_caps2_e.
    uint32_t            Caps3;           /// Combination of dds_caps3_e.
    uint32_t            Caps4;           /// Combination of dds_caps4_e.
    uint32_t            Reserved2;       /// Reserved for future use.
};
#pragma pack(pop)

/// @summary The equivalent of the DDS_HEADER_DXT10 structure. See MSDN at:
/// http://msdn.microsoft.com/en-us/library/windows/desktop/bb943983(v=vs.85).aspx
#pragma pack(push, 1)
struct dds_header_dxt10_t
{
    uint32_t            Format;          /// One of dxgi_format_e.
    uint32_t            Dimension;       /// One of d3d11_resource_dimension_e.
    uint32_t            Flags;           /// Combination of d3d11_resource_misc_flag_e.
    uint32_t            ArraySize;       /// The number of of items in an array texture.
    uint32_t            Flags2;          /// One of dds_alpha_mode_e.
};
#pragma pack(pop)

/// @summary Describes a single level within the mipmap pyramid in a DDS. Level
/// zero represents the highest-resolution surface (the base surface.)
struct dds_level_desc_t
{
    size_t              Index;           /// The zero-based index of the mip-level.
    size_t              Width;           /// The width of the surface.
    size_t              Height;          /// The height of the surface.
    size_t              Slices;          /// The depth of the surface.
    size_t              BytesPerElement; /// The number of bytes per-pixel or block.
    size_t              BytesPerRow;     /// The number of bytes between scanlines.
    size_t              BytesPerSlice;   /// The number of bytes between slices.
    size_t              DataSize;        /// The total size of the data for the level, in bytes.
    uint32_t            Format;          /// One of dxgi_format_e.
};

/// @summary Defines the data needed to load an image. This data is captured in 
/// an image database and used to load or re-load the image if it is requested 
/// and does not currently exist within the target image cache.
struct image_open_info_t
{
    uintptr_t           ImageId;         /// The application-defined image identifier (duplicated in the ID table.)
    size_t              PathOffset;      /// The byte offset of the virtual path in the string table.
    int                 FileHints;       /// A combination of vfs_file_hint_e used when opening the file.
    int                 DecoderHint;     /// One of vfs_decoder_hint_e used to request a specific decoder.
};

/// @summary Data about an image that has been loaded into an image cache at 
/// some point. This data is captured in an image database and can be used to 
/// satisfy simple metadata queries, even if the image is not currently loaded.
struct image_cache_info_t
{
    uintptr_t           TargetCache;     /// The image cache into which the image data was loaded.
    size_t              BytesInCache;    /// The number of bytes reserved for the image in the target cache.
    uint32_t            ImageFormat;     /// The image pixel format. One of dxgi_format_e.
    uint32_t            ImageEncoding;   /// The image encoding. One of image_encoding_e.
    size_t              BaseWidth;       /// The width of the highest-resolution mip-level, in pixels.
    size_t              BaseHeight;      /// The height of the highest-resolution mip-level, in pixels.
    size_t              BaseSliceCount;  /// The number of slices in the highest-resolution mip-level.
    size_t              ElementCount;    /// The number of array elements or cubemap faces.
    size_t              LevelCount;      /// The number of levels in the mipmap chain.
    size_t              BitsPerPixel;    /// The number of bytes per-pixel.
    size_t              BytesPerBlock;   /// The number of bytes per-block for compressed formats, or zero.
    size_t              ImageDataOffset; /// The byte offset of the image data from the start of the file or buffer.
    dds_header_t        DDSHeader;       /// The DDS header describing the image.
    dds_header_dxt10_t  DX10Header;      /// The Direct3D 10 extended header describing the image.
    dds_level_desc_t   *LevelInfo;       /// An array of LevelCount mip-level descriptors, owned by the image blob.
    size_t             *LevelOffsets;    /// An array of ElementCount * LevelCount zero-based byte offsets of the start of each level, owned by the image blob.
};

/// @summary Represents a set of images that have been or will be loaded. The 
/// main application can insert images into the database, allowing them to be
/// loaded into an image cache at some future point.
struct image_database_t
{
    // image_database_begin_update()
    // image_database_end_update()
    // because we'll want to batch-insert
    SRWLOCK             Lock;            /// The lock guarding against concurrent insertions.
    SRWLOCK             CacheLock;       /// The lock guarding cache information updates specifically.
    id_table_t          IdTable;         /// The table mapping image ID to registry index.
    string_table_t      PathTable;       /// The string table of cached virtual file paths.
    image_open_info_t  *OpenInfo;        /// The list of VFS file open arguments for each image.
    image_cache_info_t *CacheInfo;       /// The list of cached image information for each image.
};

/*///////////////
//   Globals   //
///////////////*/

/*///////////////////////
//   Local Functions   //
///////////////////////*/
/// @summary Returns the minimum of two values.
/// @param a...b The input values.
/// @return The larger of the input values.
template<class T>
internal_function inline const T& image_min2(const T& a, const T& b)
{
    return (a < b) ? a : b;
}

/// @summary Returns the maximum of two values.
/// @param a...b The input values.
/// @return The larger of the input values.
template<class T>
internal_function inline const T& image_max2(const T& a, const T& b)
{
    return (a < b) ? b : a;
}

/// @summary Calculates the dimension of a mipmap level. This function may be used to calculate the width, height or depth dimensions.
/// @param dimension The dimension of the highest-resolution level (level 0.)
/// @param level_index The zero-based index of the mip-level to compute.
/// @return The corresponding dimension of the specified mipmap level.
internal_function inline size_t image_level_dimension(size_t base_dimension, size_t level_index)
{
    size_t  l_dimension  = base_dimension >> level_index;
    return (l_dimension == 0) ? 1 : l_dimension;
}

/*////////////////////////
//   Public Functions   //
////////////////////////*/
/// @summary Generates a little-endian FOURCC.
/// @param a...d The four characters comprising the code.
/// @return The packed four-cc value, in little-endian format.
public_function inline uint32_t image_fourcc_le(char a, char b, char c, char d)
{
    uint32_t A = (uint32_t) a;
    uint32_t B = (uint32_t) b;
    uint32_t C = (uint32_t) c;
    uint32_t D = (uint32_t) d;
    return ((D << 24) | (C << 16) | (B << 8) | (A << 0));
}

/// @summary Generates a big-endian FOURCC.
/// @param a...d The four characters comprising the code.
/// @return The packed four-cc value, in big-endian format.
public_function inline uint32_t image_fourcc_be(char a, char b, char c, char d)
{
    uint32_t A = (uint32_t) a;
    uint32_t B = (uint32_t) b;
    uint32_t C = (uint32_t) c;
    uint32_t D = (uint32_t) d;
    return ((A << 24) | (B << 16) | (C << 8) | (D << 0));
}

/// @summary Determines the dxgi_format value based on data in dds headers.
/// @param header the base surface header of the dds.
/// @param header_ex the extended surface header of the dds, or NULL.
/// @retur  one of the values of the dxgi_format_e enumeration.
public_function uint32_t dxgi_format(dds_header_t const *header, dds_header_dxt10_t const *header_ex)
{
    if (header_ex != NULL)
    {
        return header_ex->Format;
    }
    if (header == NULL)
    {
        return DXGI_FORMAT_UNKNOWN;
    }

    dds_pixelformat_t const &pf =  header->Format;
    #define ISBITMASK(r, g, b, a) (pf.BitMaskR == (r) && pf.BitMaskG == (g) && pf.BitMaskB == (b) && pf.BitMaskA == (a))

    if (pf.Flags & DDPF_FOURCC)
    {
        if (pf.FourCC == image_fourcc_le('D','X','T','1'))
        {
            return DXGI_FORMAT_BC1_UNORM;
        }
        if (pf.FourCC == image_fourcc_le('D','X','T','2'))
        {
            return DXGI_FORMAT_BC2_UNORM;
        }
        if (pf.FourCC == image_fourcc_le('D','X','T','3'))
        {
            return DXGI_FORMAT_BC2_UNORM;
        }
        if (pf.FourCC == image_fourcc_le('D','X','T','4'))
        {
            return DXGI_FORMAT_BC3_UNORM;
        }
        if (pf.FourCC == image_fourcc_le('D','X','T','5'))
        {
            return DXGI_FORMAT_BC3_UNORM;
        }
        if (pf.FourCC == image_fourcc_le('A','T','I','1'))
        {
            return DXGI_FORMAT_BC4_UNORM;
        }
        if (pf.FourCC == image_fourcc_le('A','T','I','2'))
        {
            return DXGI_FORMAT_BC5_UNORM;
        }
        if (pf.FourCC == image_fourcc_le('B','C','4','U'))
        {
            return DXGI_FORMAT_BC4_UNORM;
        }
        if (pf.FourCC == image_fourcc_le('B','C','4','S'))
        {
            return DXGI_FORMAT_BC4_SNORM;
        }
        if (pf.FourCC == image_fourcc_le('B','C','5','U'))
        {
            return DXGI_FORMAT_BC5_UNORM;
        }
        if (pf.FourCC == image_fourcc_le('B','C','5','S'))
        {
            return DXGI_FORMAT_BC5_SNORM;
        }
        switch (pf.FourCC)
        {
            case 36: // D3DFMT_A16B16G16R16
                return DXGI_FORMAT_R16G16B16A16_UNORM;

            case 110: // D3DFMT_Q16W16V16U16
                return DXGI_FORMAT_R16G16B16A16_SNORM;

            case 111: // D3DFMT_R16F
                return DXGI_FORMAT_R16_FLOAT;

            case 112: // D3DFMT_G16R16F
                return DXGI_FORMAT_R16G16_FLOAT;

            case 113: // D3DFMT_A16B16G16R16F
                return DXGI_FORMAT_R16G16B16A16_FLOAT;

            case 114: // D3DFMT_R32F
                return DXGI_FORMAT_R32_FLOAT;

            case 115: // D3DFMT_G32R32F
                return DXGI_FORMAT_R32G32_FLOAT;

            case 116: // D3DFMT_A32B32G32R32F
                return DXGI_FORMAT_R32G32B32A32_FLOAT;

            default:
                break;
        }
        return DXGI_FORMAT_UNKNOWN;
    }
    if (pf.Flags & DDPF_RGB)
    {
        switch (pf.RGBBitCount)
        {
            case 32:
                {
                    if (ISBITMASK(0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000))
                    {
                        return DXGI_FORMAT_R8G8B8A8_UNORM;
                    }
                    if (ISBITMASK(0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000))
                    {
                        return DXGI_FORMAT_B8G8R8A8_UNORM;
                    }
                    if (ISBITMASK(0x00ff0000, 0x0000ff00, 0x000000ff, 0x00000000))
                    {
                        return DXGI_FORMAT_B8G8R8X8_UNORM;
                    }

                    // No DXGI format maps to ISBITMASK(0x000000ff, 0x0000ff00, 0x00ff0000, 0x00000000) aka D3DFMT_X8B8G8R8
                    // Note that many common DDS reader/writers (including D3DX) swap the the RED/BLUE masks for 10:10:10:2
                    // formats. We assumme below that the 'backwards' header mask is being used since it is most likely
                    // written by D3DX. The more robust solution is to use the 'DX10' header extension and specify the
                    // DXGI_FORMAT_R10G10B10A2_UNORM format directly.

                    // For 'correct' writers, this should be 0x000003ff, 0x000ffc00, 0x3ff00000 for RGB data.
                    if (ISBITMASK(0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000))
                    {
                        return DXGI_FORMAT_R10G10B10A2_UNORM;
                    }
                    // No DXGI format maps to ISBITMASK(0x000003ff, 0x000ffc00, 0x3ff00000, 0xc0000000) aka D3DFMT_A2R10G10B10.
                    if (ISBITMASK(0x0000ffff, 0xffff0000, 0x00000000, 0x00000000))
                    {
                        return DXGI_FORMAT_R16G16_UNORM;
                    }
                    if (ISBITMASK(0xffffffff, 0x00000000, 0x00000000, 0x00000000))
                    {
                        // Only 32-bit color channel format in D3D9 was R32F
                        return DXGI_FORMAT_R32_FLOAT; // D3DX writes this out as a FourCC of 114.
                    }
                }
                break;

            case 24:
                // No 24bpp DXGI formats aka D3DFMT_R8G8B8
                break;

            case 16:
                {
                    if (ISBITMASK(0x7c00, 0x03e0, 0x001f, 0x8000))
                    {
                        return DXGI_FORMAT_B5G5R5A1_UNORM;
                    }
                    if (ISBITMASK(0xf800, 0x07e0, 0x001f, 0x0000))
                    {
                        return DXGI_FORMAT_B5G6R5_UNORM;
                    }
                    // No DXGI format maps to ISBITMASK(0x7c00, 0x03e0, 0x001f, 0x0000) aka D3DFMT_X1R5G5B5.
                    if (ISBITMASK(0x0f00, 0x00f0, 0x000f, 0xf000))
                    {
                        return DXGI_FORMAT_B4G4R4A4_UNORM;
                    }
                    // No DXGI format maps to ISBITMASK(0x0f00, 0x00f0, 0x000f, 0x0000) aka D3DFMT_X4R4G4B4.
                    // No 3:3:2, 3:3:2:8, or paletted DXGI formats aka D3DFMT_A8R3G3B2, D3DFMT_R3G3B2, D3DFMT_P8, D3DFMT_A8P8, etc.
                }
                break;
        }
    }
    if (pf.Flags & DDPF_ALPHA)
    {
        if (pf.RGBBitCount == 8)
        {
            return DXGI_FORMAT_A8_UNORM;
        }
    }
    if (pf.Flags & DDPF_LUMINANCE)
    {
        if (pf.RGBBitCount == 8)
        {
            if (ISBITMASK(0x000000ff, 0x00000000, 0x00000000, 0x00000000))
            {
                // D3DX10/11 writes this out as DX10 extension.
                return DXGI_FORMAT_R8_UNORM;
            }

            // No DXGI format maps to ISBITMASK(0x0f, 0x00, 0x00, 0xf0) aka D3DFMT_A4L4
        }
        if (pf.RGBBitCount == 16)
        {
            if (ISBITMASK(0x0000ffff, 0x00000000, 0x00000000, 0x00000000))
            {
                // D3DX10/11 writes this out as DX10 extension.
                return DXGI_FORMAT_R16_UNORM;
            }
            if (ISBITMASK(0x000000ff, 0x00000000, 0x00000000, 0x0000ff00))
            {
                // D3DX10/11 writes this out as DX10 extension
                return DXGI_FORMAT_R8G8_UNORM;
            }
        }
    }
    #undef ISBITMASK
    return DXGI_FORMAT_UNKNOWN;
}

/// @summary Determines if a DXGI format value is a block-compressed format.
/// @param format One of dxgi_format_e.
/// @return true if format is one of DXGI_FORMAT_BCn.
public_function bool dxgi_block_compressed(uint32_t format)
{
    switch (format)
    {
        case DXGI_FORMAT_BC1_TYPELESS:
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:
        case DXGI_FORMAT_BC4_TYPELESS:
        case DXGI_FORMAT_BC4_UNORM:
        case DXGI_FORMAT_BC4_SNORM:
        case DXGI_FORMAT_BC2_TYPELESS:
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:
        case DXGI_FORMAT_BC3_TYPELESS:
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:
        case DXGI_FORMAT_BC5_TYPELESS:
        case DXGI_FORMAT_BC5_UNORM:
        case DXGI_FORMAT_BC5_SNORM:
        case DXGI_FORMAT_BC6H_TYPELESS:
        case DXGI_FORMAT_BC6H_UF16:
        case DXGI_FORMAT_BC6H_SF16:
        case DXGI_FORMAT_BC7_TYPELESS:
        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:
            return true;

        default:
            break;
    }
    return false;
}

/// @summary Determines if a DXGI fomrat value specifies a packed format.
/// @param format One of dxgi_format_e.
/// @return true if format is one of DXGI_FORMAT_R8G8_B8G8_UNORM or DXGI_FORMAT_G8R8_G8B8_UNORM.
public_function bool dxgi_packed(uint32_t format)
{
    if (format == DXGI_FORMAT_R8G8_B8G8_UNORM ||
        format == DXGI_FORMAT_G8R8_G8B8_UNORM)
    {
        return true;
    }
    else return false;
}

/// @summary Determines if a DDS describes a cubemap surface.
/// @param header The base surface header of the DDS.
/// @param header_ex The extended surface header of the DDS, or NULL.
/// @return true if the DDS describes a cubemap.
public_function bool dxgi_cubemap(dds_header_t const *header, dds_header_dxt10_t const *header_ex)
{
    if (header_ex != NULL)
    {
        if (header_ex->Dimension == D3D11_RESOURCE_DIMENSION_TEXTURE2D &&
            header_ex->Flags     == D3D11_RESOURCE_MISC_TEXTURECUBE)
        {
            return true;
        }
        // else fall through and look at the dds_header_t.
    }
    if (header != NULL)
    {
        if ((header->Caps  & DDSCAPS_COMPLEX) == 0)
            return false;
        if ((header->Caps2 & DDSCAPS2_CUBEMAP) == 0)
            return false;
        if ((header->Caps2 & DDSCAPS2_CUBEMAP_POSITIVEX) ||
            (header->Caps2 & DDSCAPS2_CUBEMAP_NEGATIVEX) ||
            (header->Caps2 & DDSCAPS2_CUBEMAP_POSITIVEY) ||
            (header->Caps2 & DDSCAPS2_CUBEMAP_NEGATIVEY) ||
            (header->Caps2 & DDSCAPS2_CUBEMAP_POSITIVEZ) ||
            (header->Caps2 & DDSCAPS2_CUBEMAP_NEGATIVEZ))
            return true;
    }
    return false;
}

/// @summary Determines if a DDS describes a volume surface.
/// @param header The base surface header of the DDS.
/// @param header_ex The extended surface header of the DDS, or NULL.
/// @return true if the DDS describes a volume.
public_function bool dxgi_volume(dds_header_t const *header, dds_header_dxt10_t const *header_ex)
{
    if (header_ex != NULL)
    {
        if (header_ex->ArraySize != 1)
        {   // arrays of volumes are not supported.
            return false;
        }
    }
    if (header != NULL)
    {
        if ((header->Caps  & DDSCAPS_COMPLEX) == 0)
            return false;
        if ((header->Caps2 & DDSCAPS2_VOLUME) == 0)
            return false;
        if ((header->Flags & DDSD_DEPTH) == 0)
            return false;
        return header->Depth > 1;
    }
    return false;
}

/// @summary Determines if a DDS describes a surface array. Note that a volume
/// is not considered to be the same as a surface array.
/// @param header The base surface header of the DDS.
/// @param header_ex The extended surface header of the DDS, or NULL.
/// @return true if the DDS describes a surface array.
public_function bool dxgi_array(dds_header_t const *header, dds_header_dxt10_t const *header_ex)
{
    if (header != NULL && header_ex != NULL)
    {
        return header_ex->ArraySize > 1;
    }
    return false;
}

/// @summary Determines if a DDS describes a mipmap chain.
/// @param header The base surface header of the DDS.
/// @param header_ex The extended surface header of the DDS, or NULL.
/// @return true if the DDS describes a mipmap chain.
public_function bool dxgi_mipmap(dds_header_t const *header, dds_header_dxt10_t const *header_ex)
{
    if (header_ex != NULL)
    {
        if (header_ex->Dimension != D3D11_RESOURCE_DIMENSION_TEXTURE1D &&
            header_ex->Dimension != D3D11_RESOURCE_DIMENSION_TEXTURE2D &&
            header_ex->Dimension != D3D11_RESOURCE_DIMENSION_TEXTURE3D)
            return false;
    }
    if (header != NULL)
    {
        if (header->Caps & DDSCAPS_MIPMAP)
            return true;
        if (header->Flags & DDSD_MIPMAPCOUNT)
            return true;
        if (header->Levels > 0)
            return true;
    }
    return false;
}

/// @summary Calculate the number of bits-per-pixel for a given format. Block-
/// compressed formats are supported as well.
/// @param format One of dxgi_format_e.
/// @return The number of bits per-pixel.
public_function size_t dxgi_bits_per_pixel(uint32_t format)
{
    switch (format)
    {
        case DXGI_FORMAT_R32G32B32A32_TYPELESS:
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
        case DXGI_FORMAT_R32G32B32A32_UINT:
        case DXGI_FORMAT_R32G32B32A32_SINT:
            return 128;

        case DXGI_FORMAT_R32G32B32_TYPELESS:
        case DXGI_FORMAT_R32G32B32_FLOAT:
        case DXGI_FORMAT_R32G32B32_UINT:
        case DXGI_FORMAT_R32G32B32_SINT:
            return 96;

        case DXGI_FORMAT_R16G16B16A16_TYPELESS:
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R16G16B16A16_UNORM:
        case DXGI_FORMAT_R16G16B16A16_UINT:
        case DXGI_FORMAT_R16G16B16A16_SNORM:
        case DXGI_FORMAT_R16G16B16A16_SINT:
        case DXGI_FORMAT_R32G32_TYPELESS:
        case DXGI_FORMAT_R32G32_FLOAT:
        case DXGI_FORMAT_R32G32_UINT:
        case DXGI_FORMAT_R32G32_SINT:
        case DXGI_FORMAT_R32G8X24_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
        case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
            return 64;

        case DXGI_FORMAT_R10G10B10A2_TYPELESS:
        case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R10G10B10A2_UINT:
        case DXGI_FORMAT_R11G11B10_FLOAT:
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_R8G8B8A8_UINT:
        case DXGI_FORMAT_R8G8B8A8_SNORM:
        case DXGI_FORMAT_R8G8B8A8_SINT:
        case DXGI_FORMAT_R16G16_TYPELESS:
        case DXGI_FORMAT_R16G16_FLOAT:
        case DXGI_FORMAT_R16G16_UNORM:
        case DXGI_FORMAT_R16G16_UINT:
        case DXGI_FORMAT_R16G16_SNORM:
        case DXGI_FORMAT_R16G16_SINT:
        case DXGI_FORMAT_R32_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_R32_FLOAT:
        case DXGI_FORMAT_R32_UINT:
        case DXGI_FORMAT_R32_SINT:
        case DXGI_FORMAT_R24G8_TYPELESS:
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
        case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
        case DXGI_FORMAT_R8G8_B8G8_UNORM:
        case DXGI_FORMAT_G8R8_G8B8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8X8_UNORM:
        case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
        case DXGI_FORMAT_B8G8R8A8_TYPELESS:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8X8_TYPELESS:
        case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
            return 32;

        case DXGI_FORMAT_R8G8_TYPELESS:
        case DXGI_FORMAT_R8G8_UNORM:
        case DXGI_FORMAT_R8G8_UINT:
        case DXGI_FORMAT_R8G8_SNORM:
        case DXGI_FORMAT_R8G8_SINT:
        case DXGI_FORMAT_R16_TYPELESS:
        case DXGI_FORMAT_R16_FLOAT:
        case DXGI_FORMAT_D16_UNORM:
        case DXGI_FORMAT_R16_UNORM:
        case DXGI_FORMAT_R16_UINT:
        case DXGI_FORMAT_R16_SNORM:
        case DXGI_FORMAT_R16_SINT:
        case DXGI_FORMAT_B5G6R5_UNORM:
        case DXGI_FORMAT_B5G5R5A1_UNORM:
        case DXGI_FORMAT_B4G4R4A4_UNORM:
            return 16;

        case DXGI_FORMAT_R8_TYPELESS:
        case DXGI_FORMAT_R8_UNORM:
        case DXGI_FORMAT_R8_UINT:
        case DXGI_FORMAT_R8_SNORM:
        case DXGI_FORMAT_R8_SINT:
        case DXGI_FORMAT_A8_UNORM:
            return 8;

        case DXGI_FORMAT_R1_UNORM:
            return 1;

        case DXGI_FORMAT_BC1_TYPELESS:
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:
        case DXGI_FORMAT_BC4_TYPELESS:
        case DXGI_FORMAT_BC4_UNORM:
        case DXGI_FORMAT_BC4_SNORM:
            return 4;

        case DXGI_FORMAT_BC2_TYPELESS:
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:
        case DXGI_FORMAT_BC3_TYPELESS:
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:
        case DXGI_FORMAT_BC5_TYPELESS:
        case DXGI_FORMAT_BC5_UNORM:
        case DXGI_FORMAT_BC5_SNORM:
        case DXGI_FORMAT_BC6H_TYPELESS:
        case DXGI_FORMAT_BC6H_UF16:
        case DXGI_FORMAT_BC6H_SF16:
        case DXGI_FORMAT_BC7_TYPELESS:
        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:
            return 8;

        default:
            return 0;
    }
}

/// @summary Calculate the number of bytes per 4x4-pixel block.
/// @param format One of dxgi_format_e.
/// @return The number of bytes in a 4x4 pixel block, or 0 for non-block-compressed formats.
public_function size_t dxgi_bytes_per_block(uint32_t format)
{
    switch (format)
    {
        case DXGI_FORMAT_BC1_TYPELESS:
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:
        case DXGI_FORMAT_BC4_TYPELESS:
        case DXGI_FORMAT_BC4_UNORM:
        case DXGI_FORMAT_BC4_SNORM:
            return 8;

        case DXGI_FORMAT_BC2_TYPELESS:
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:
        case DXGI_FORMAT_BC3_TYPELESS:
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:
        case DXGI_FORMAT_BC5_TYPELESS:
        case DXGI_FORMAT_BC5_UNORM:
        case DXGI_FORMAT_BC5_SNORM:
        case DXGI_FORMAT_BC6H_TYPELESS:
        case DXGI_FORMAT_BC6H_UF16:
        case DXGI_FORMAT_BC6H_SF16:
        case DXGI_FORMAT_BC7_TYPELESS:
        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:
            return 16;

        default:
            break;
    }
    return 0;
}

/// @summary Determines the number of elements in a surface array.
/// @param header The base surface header of the DDS.
/// @param header_ex The extended surface header of the DDS, or NULL.
/// @return The number of elements in the surface array, or 1 if the DDS does not describe an array.
public_function size_t dxgi_array_count(dds_header_t const *header, dds_header_dxt10_t const *header_ex)
{
    if (header != NULL && header_ex != NULL)
    {
        size_t multiplier = 1;
        if (header->Caps2 & DDSCAPS2_CUBEMAP)
        {   // DX10+ cubemaps must specify all faces.
            multiplier = 6;
        }
        // DX10 extended header is required for surface arrays.
        return size_t(header_ex->ArraySize) * multiplier;
    }
    else if (header != NULL)
    {
        size_t nfaces = 1;
        if (header->Caps2 & DDSCAPS2_CUBEMAP)
        {   // non-DX10 cubemaps may specify only some faces.
            if (header->Caps2 & DDSCAPS2_CUBEMAP_POSITIVEX) nfaces++;
            if (header->Caps2 & DDSCAPS2_CUBEMAP_NEGATIVEX) nfaces++;
            if (header->Caps2 & DDSCAPS2_CUBEMAP_POSITIVEY) nfaces++;
            if (header->Caps2 & DDSCAPS2_CUBEMAP_NEGATIVEY) nfaces++;
            if (header->Caps2 & DDSCAPS2_CUBEMAP_POSITIVEZ) nfaces++;
            if (header->Caps2 & DDSCAPS2_CUBEMAP_NEGATIVEZ) nfaces++;
        }
        return nfaces;
    }
    else return 0;
}

/// @summary Given a baseline DDS header, generates a corresponding DX10 extended header.
/// @param dx10 The DX10 extended DDS header to populate.
/// @param dds The base DDS header describing the image.
public_function void dx10_header_for_dds(dds_header_dxt10_t *dx10, dds_header_t const *dds)
{
    dx10->Format    = dxgi_format(dds, NULL);
    dx10->Dimension = D3D11_RESOURCE_DIMENSION_TEXTURE1D;
    dx10->Flags     = 0; // determined below
    dx10->ArraySize = dxgi_array_count(dds, NULL);
    dx10->Flags2    = DDS_ALPHA_MODE_UNKNOWN;

    // determine a correct resource dimension:
    if (dxgi_cubemap(dds, NULL) && dx10->ArraySize == 6)
    {   // DX10+ cubemaps must specify all six faces.
        dx10->Dimension = D3D11_RESOURCE_DIMENSION_TEXTURE2D;
        dx10->Flags    |= D3D11_RESOURCE_MISC_TEXTURECUBE;
    }
    else if (dxgi_volume(dds, NULL))
    {
        dx10->Dimension = D3D11_RESOURCE_DIMENSION_TEXTURE3D;
    }
    else if ((dds->Flags & DDSD_WIDTH ) != 0 && dds->Width  > 1 && 
             (dds->Flags & DDSD_HEIGHT) != 0 && dds->Height > 1)
    {
        dx10->Dimension = D3D11_RESOURCE_DIMENSION_TEXTURE2D;
    }
}

/// @summary Determines the number of levels in the mipmap chain.
/// @param header The base surface header of the DDS.
/// @param header_ex The extended surface header of the DDS, or NULL.
/// @return The number of levels in the mipmap chain, or 1 if the DDS describes the top level only.
public_function size_t dxgi_level_count(dds_header_t const *header, dds_header_dxt10_t const *header_ex)
{
    if (dxgi_mipmap(header, header_ex))
    {
        return header->Levels;
    }
    else if (header != NULL) return 1;
    else return 0;
}

/// @summary Calculates the correct pitch value for a scanline, based on the
/// data format and width of the surface. This is necessary because many DDS
/// writers do not correctly compute the pitch value. See MSDN documentation at:
/// http://msdn.microsoft.com/en-us/library/windows/desktop/bb943991(v=vs.85).aspx
/// @param format One of the values of the dxgi_format_e enumeration.
public_function size_t dxgi_pitch(uint32_t format, size_t width)
{
    if (dxgi_block_compressed(format))
    {
        size_t bw = image_max2<size_t>(1, (width + 3) / 4);
        return bw * dxgi_bytes_per_block(format);
    }
    if (dxgi_packed(format))
    {
        return ((width + 1) >> 1) * 4;
    }
    return (width * dxgi_bits_per_pixel(format) + 7) / 8;
}

/// @summary Calculates the dimension of an image, in pixels, and accounting for block compression. Note that only width and height should be calculated using this logic as block compression is 2D-only.
/// @param format One of dxgi_format_e.
/// @param dimension The width or height of an image.
/// @return The width or height, in pixels.
public_function inline size_t dxgi_image_dimension(uint32_t format, size_t dimension)
{
    if (dxgi_block_compressed(format))
    {
        // all BC formats encode 4x4 blocks.
        return image_max2<size_t>(1, ((dimension + 3) / 4) * 4);
    }
    else return image_max2<size_t>(1, dimension);
}
