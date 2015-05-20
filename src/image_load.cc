/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines constants, types and functions for asynchronously loading
/// image data and caching it in image blobs.
///////////////////////////////////////////////////////////////////////////80*/

/*////////////////
//   Includes   //
////////////////*/

/*////////////////////
//   Preprocessor   //
////////////////////*/

/*/////////////////
//   Constants   //
/////////////////*/

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Define the data returned after a request to load an image has completed.
struct image_load_result_t
{
    uintptr_t           ImageId;          /// The application-defined image identifier.
    uint32_t            StreamError;      /// The error returned by the stream decoder. See stream_decode_error_e.
    int                 ParserError;      /// The error returned by the image parser. A value of zero indicates no error.
    uint32_t            ImageFormat;      /// The image pixel format. One of dxgi_format_e.
    uint32_t            ImageEncoding;    /// The image encoding. One of image_encoding_e.
    size_t              BaseWidth;        /// The width of the highest-resolution mip-level, in pixels.
    size_t              BaseHeight;       /// The height of the highest-resolution mip-level, in pixels.
    size_t              BaseSliceCount;   /// The number of slices in the highest-resolution mip-level.
    size_t              ElementCount;     /// The number of array elements or cubemap faces.
    size_t              LevelCount;       /// The number of levels in the mipmap chain.
    size_t              BitsPerPixel;     /// The number of bytes per-pixel.
    size_t              BytesPerBlock;    /// The number of bytes per-block for compressed formats, or zero.
    size_t              ImageDataOffset;  /// The byte offset of the image data from the start of the file or buffer.
    dds_level_desc_t   *LevelInfo;        /// An array of LevelCount mip-level descriptors.
    size_t             *LevelOffsets;     /// An array of ElementCount * LevelCount zero-based byte offsets of the start of each level.
    dds_header_t        DDSHeader;        /// The DDS header describing the image.
    dds_header_dxt10_t  DX10Header;       /// The Direct3D 10 extended header describing the image.
    stream_decoder_t   *Decoder;          /// The stream decoder from which the image data was read.
};

typedef fifo_allocator_t<image_load_result_t> image_result_alloc_t;
typedef spsc_fifo_u_t   <image_load_result_t> image_result_queue_t;

/*///////////////
//   Globals   //
///////////////*/

/*///////////////////////
//   Local Functions   //
///////////////////////*/

/*////////////////////////
//   Public Functions   //
////////////////////////*/
/// @summary Checks the error codes on a DDS file load result to determine whether an error occurred.
/// @param result The result to check.
/// @return true if an error occurred during loading.
public_function bool image_load_error(image_load_result_t const &result)
{
    return (FAILED(result.StreamError) || (result.ParserError != 0));
}
