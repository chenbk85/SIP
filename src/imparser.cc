/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines the interface to an image parser, which is responsible for
/// extracting image definitions and pixel data from a file. The loader always
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
/// @summary Define a standard set of flags that control parsing of image containers.
enum image_parser_flags_e : uint32_t
{
    IMAGE_PARSER_FLAGS_READ_METADATA   = (1 << 0), /// Indicates that the metadata should be read.
    IMAGE_PARSER_FLAGS_READ_PIXELS     = (1 << 1), /// Indicates that the pixel data should be read.
    IMAGE_PARSER_FLAGS_METADATA_SET    = (1 << 2), /// Indicates that the image metadata has been set externally.
    IMAGE_PARSER_FLAGS_START_AT_OFFSET = (1 << 3), /// Indicates that the StartOffset should be utilized.
    IMAGE_PARSER_FLAGS_SINGLE_FRAME    = (1 << 4), /// Indicates that only a single image element/frame is being loaded, and the count is known.
    IMAGE_PARSER_FLAGS_FRAME_RANGE     = (1 << 5), /// Indicates that one or more image elements/frames are being loaded, and the count is known.
    IMAGE_PARSER_FLAGS_ALL_FRAMES      = (1 << 6), /// Indicates that all frames are being loaded, and the count is unknown.
    IMAGE_PARSER_FLAGS_READ_ALL_DATA   = 
        IMAGE_PARSER_FLAGS_READ_METADATA  | 
        IMAGE_PARSER_FLAGS_READ_PIXELS,            /// Indicates the metadata and pixel data should be read.
    IMAGE_PARSER_FLAGS_READ_ALL        = 
        IMAGE_PARSER_FLAGS_READ_ALL_DATA  | 
        IMAGE_PARSER_FLAGS_ALL_FRAMES              /// Indicates that the file will be read in its entirety.
};

/// @summary Helper template for maintaining a list of image container parser states.
template <typename ParserT>
struct image_parser_list_t
{
    size_t              Count;                     /// The number of active parsers in the list.
    size_t              Capacity;                  /// The capacity of the list.
    image_encoder_t   **TargetStream;              /// The set of pointers to image encoders (output) for each active parser.
    stream_decoder_t  **SourceStream;              /// The set of pointers to stream decoders (input) for each active parser.
    image_definition_t *ImageMetadata;             /// The image metadata for each active parser.
    ParserT            *ParseState;                /// The format-specific parser state for each active parser.
};

/// @summary Defines the configuration data passed to an image parser.
struct image_parser_config_t
{
    uintptr_t           ImageId;                   /// The application-defined logical image identifier.
    uintptr_t           Context;                   /// Opaque data passed to the metadata callback.
    size_t              FirstFrame;                /// The zero-based index of the first frame to read.
    size_t              FinalFrame;                /// The zero-based index of the final frame to read, or IMAGE_ALL_FRAMES to read all frames.
    image_definition_t *Metadata;                  /// The known image metadata. Valid if ParseFlags has IMAGE_PARSER_FLAGS_METADATA_SET set.
    stream_decode_pos_t StartOffset;               /// The position within the file at which to start reading data. Valid if ParseFlags has IMAGE_PARSER_FLAGS_START_OFFSET set.
    uint32_t            ParseFlags;                /// A combination image_parser_flags_e controlling parser behavior.
};

/*///////////////
//   Globals   //
///////////////*/

/*///////////////////////
//   Local Functions   //
///////////////////////*/

/*////////////////////////
//   Public Functions   //
////////////////////////*/
/// @summary Initialize an image parser list with a given initial capacity.
/// @param ipsl The image parser list to initialize.
/// @param capacity The initial capacity of the list.
template <typename T>
void image_parser_list_create(image_parser_list_t<T> *ipsl, size_t capacity)
{
    ipsl->Count         = 0;
    ipsl->Capacity      = 0;
    ipsl->TargetStream  = NULL;
    ipsl->SourceStream  = NULL;
    ipsl->ImageMetadata = NULL;
    ipsl->ParseState    = NULL;
    if (capacity > 0)
    {   // pre-allocate storage for the specified number of items.
        image_encoder_t      **te = (image_encoder_t     **)malloc(capacity * sizeof(image_encoder_t  *));
        stream_decoder_t     **sd = (stream_decoder_t    **)malloc(capacity * sizeof(stream_decoder_t *));
        image_definition_t    *im = (image_definition_t   *)malloc(capacity * sizeof(image_definition_t));
        T                     *ps = (T                    *)malloc(capacity * sizeof(T));
        if (te != NULL)   ipsl->TargetStream  = te;
        if (sd != NULL)   ipsl->SourceStream  = sd;
        if (im != NULL)   ipsl->ImageMetadata = im;
        if (ps != NULL)   ipsl->ParseState    = ps;
        if (te != NULL && sd != NULL && im != NULL && ps != NULL) ipsl->Capacity = capacity;
    }
}

/// @summary Ensure that an image parser list has at least the specified capacity.
/// If not, grow the storage capacity by reallocating the underlying arrays.
/// @param ipsl The image parser list to check and possibly grow.
/// @param capacity The minimum capacity of the list.
template <typename T>
void image_parser_list_ensure(image_parser_list_t<T> *ipsl, size_t capacity)
{
    if (ipsl->Capacity >= capacity)
    {   // no need to do anything, capacity already available.
        return;
    }
    // determine new capacity and reallocate lists.
    size_t newc               = calculate_capacity(ipsl->Capacity, capacity, 128, 16);
    image_encoder_t      **te = (image_encoder_t     **) realloc(ipsl->TargetStream , newc * sizeof(image_encoder_t  *));
    stream_decoder_t     **sd = (stream_decoder_t    **) realloc(ipsl->SourceStream , newc * sizeof(stream_decoder_t *));
    image_definition_t    *im = (image_definition_t   *) realloc(ipsl->ImageMetadata, newc * sizeof(image_definition_t));
    T                     *ps = (T                    *) realloc(ipsl->ParseState   , newc * sizeof(T));
    if (te != NULL)   ipsl->TargetStream  = te;
    if (sd != NULL)   ipsl->SourceStream  = sd;
    if (im != NULL)   ipsl->ImageMetadata = im;
    if (ps != NULL)   ipsl->ParseState    = ps;
    if (te != NULL && sd != NULL && im != NULL && ps != NULL) ipsl->Capacity = newc;
}
