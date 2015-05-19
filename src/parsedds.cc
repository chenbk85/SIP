/*/////////////////////////////////////////////////////////////////////////////
/// @summary Implements both a streaming and an in-place parser for DDS files, 
/// which is an on-disk format for storing DirectDraw and Direct3D surfaces.
/// The types and data structures defined here are used for all management of 
/// image or surface data that is uploaded to or processed by a GPU.
///////////////////////////////////////////////////////////////////////////80*/

/*////////////////////
//   Preprocessor   //
////////////////////*/

/*////////////////
//   Includes   //
////////////////*/

/*/////////////////
//   Constants   //
/////////////////*/
/// @summary The FourCC 'DDS ' using little-endian byte ordering.
#ifndef DDS_MAGIC_LE
#define DDS_MAGIC_LE              0x20534444U
#endif

/// @summary Define the size of a buffer needed to store the base DDS header.
#define DDS_HEADER_BUFFER_SIZE    sizeof(dds_header_t)

/// @summary Define the size 
#define DDS_HEADER10_BUFFER_SIZE  sizeof(dds_header_dxt10_t)

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Define identifiers for the recognized parser states.
enum dds_parser_state_e : int
{
    DDS_PARSE_STATE_BUFFER_HEADER = 0, 
    DDS_PARSE_STATE_BUFFER_HEADER_DX10, 
    DDS_PARSE_STATE_RECEIVE_NEXT_ELEMENT, 
    DDS_PARSE_STATE_RECEIVE_NEXT_LEVEL, 
    DDS_PARSE_STATE_DISCARD_LEVEL_DATA, 
    DDS_PARSE_STATE_ENCODE_LEVEL_DATA, 
    DDS_PARSE_STATE_COMPLETE, 
    DDS_PARSE_STATE_ERROR
};

/// @summary Define identifiers for the recognized parser errors.
enum dds_parser_error_e : int
{
    DDS_PARSE_ERROR_SUCCESS = 0, 
    DDS_PARSE_ERROR_NOMEMORY = 1,
};

/// @summary Define the state data associated with a streaming DDS file parser.
struct dds_parser_state_t
{
    #define BUF_DDSH    DDS_HEADER_BUFFER_SIZE
    #define BUF_DX10    DDS_HEADER10_BUFFER_SIZE
    int                 CurrentState;         /// One of dds_parser_state_e.
    int                 ParserError;          /// One of dds_parser_error_e.
    uint32_t            ImageFormat;          /// One of dxgi_format_e.
    uint32_t            ImageEncoding;        /// One of image_encoding_e. Always IMAGE_ENCODING_RAW.
    uintptr_t           Identifier;           /// The application-defined image identifier.
    size_t              ImageDataOffset;      /// The byte offset of the image data from the start of the file.
    size_t              ElementCount;         /// The number of cubemap faces or array elements.
    size_t              ElementIndex;         /// The zero-based index of the current face or array element.
    size_t              LevelCount;           /// The number of levels in a single mipmap chain.
    size_t              LevelIndex;           /// The zero-based index of the level being parsed.
    size_t              BitsPerPixel;         /// The number of bits per-pixel.
    size_t              BytesPerBlock;        /// The number of bytes per-block, or 0 if not block-compressed.
    size_t              BaseDimension[3];     /// The base image dimensions, width, height and depth.
    dds_header_t       *DDSHeader;            /// Pointer to DDSHBuffer, if it's a valid DDS header.
    dds_header_dxt10_t *DX10Header;           /// Pointer to DX10Buffer, if it's a valid header.
    dds_level_desc_t   *LevelInfo;            /// Mipmap level descriptors, dds_array_count() * dds_level_count().
    void               *MappedAddress;        /// The base address of the mapped image blob.
    uint8_t            *WriteCursor;          /// Pointer to the current image data location.
    uint8_t            *EndOfLevel;           /// Pointer to the end of the current mip-level.
    size_t              DDSHWritePos;         /// The current write position in DDSHBuffer.
    size_t              DX10WritePos;         /// The current write position in DX10Buffer.
    uint8_t             DDSHBuffer[BUF_DDSH]; /// Internal buffer for the base image header.
    uint8_t             DX10Buffer[BUF_DX10]; /// Internal buffer for the extended DX10 header.
    #undef  BUF_DX10
    #undef  BUF_DDSH
};

/*///////////////
//   Globals   //
///////////////*/

/*///////////////////////
//   Local Functions   //
///////////////////////*/
/// @summary Calculates all of the static data once the DDS header(s) have been received and validated.
/// @param ddsp The parser state to update.
/// @return The new parser state.
internal_function int dds_parser_setup_image_info(dds_parser_state_t *ddsp)
{
    dds_header_t       *dds      = ddsp->DDSHeader;
    dds_header_dxt10_t *dx10     = ddsp->DX10Header;
    uint32_t            format   = dds_format(dds, dx10);
    size_t              basew    =(dds->Flags & DDSD_WIDTH ) ? dds->Width  : 0;
    size_t              baseh    =(dds->Flags & DDSD_HEIGHT) ? dds->Height : 0;
    size_t              based    = dds_volume(dds, dx10)     ? dds->Depth  : 1;
    size_t              bitspp   = dds_bits_per_pixel(format);
    size_t              blocksz  = dds_bytes_per_block(format);
    size_t              nitems   = dds_array_count(dds, dx10);
    size_t              nlevels  = dds_level_count(dds, dx10);
    size_t              offset   = 0;
    size_t              dst_i    = 0;
    bool                bcn      =(blocksz > 0);
    dds_level_desc_t   *levels   = NULL;

    // allocate storage for the mipmap level descriptors.
    if ((levels = (dds_level_desc_t*) malloc(nlevels * nitems * sizeof(dds_level_desc_t))) == NULL)
    {   // unable to allocate the necessary storage for mip-level descriptors.
        ddsp->ParserError = DDS_PARSE_ERROR_NOMEMORY;
        return DDS_PARSE_STATE_ERROR;
    }

    // TODO(rlk): Lock data in the image blob...
    ddsp->MappedAddress   = NULL; // TODO(rlk): set to the mapped address.
    ddsp->WriteCursor     = NULL; // TODO(rlk): set to the mapped address.
    ddsp->EndOfLevel      = NULL; // TODO(rlk): set to the mapped address.

    // write all of the data out to the parser state.
    ddsp->ImageFormat     = format;
    ddsp->ImageEncoding   = IMAGE_ENCODING_RAW;
    ddsp->ImageDataOffset = 0;
    ddsp->ElementCount    = nitems;
    ddsp->ElementIndex    = 0;
    ddsp->LevelCount      = nlevels;
    ddsp->LevelIndex      = 0;
    ddsp->BitsPerPixel    = bitspp;
    ddsp->BytesPerBlock   = blocksz;
    ddsp->BaseDimension[0]= basew;
    ddsp->BaseDimension[1]= baseh;
    ddsp->BaseDimension[2]= based;

    // calculate the byte offset of the image data from the start of the file.
    // this value can be added to the level offsets to calculate file offsets.
    ddsp->ImageDataOffset+= sizeof(uint32_t);     // magic bytes
    ddsp->ImageDataOffset+= sizeof(dds_header_t); // base header (always present)
    if (dx10 != NULL) ddsp->ImageDataOffset += sizeof(dds_header_dxt10_t);

    // calculate the byte offset for each level from the start of the image data blob
    // and also compute the dimensions of the mip-level in the mipmap chain.
    for (size_t i = 0; i < nitems; ++i)
    {   // loop for each cube face or array element...
        for (size_t j = 0; j < nlevels; ++j)
        {   // loop for each level in the mipmap chain...
            dds_level_desc_t &dst = levels[dst_i++];
            size_t levelw         = image_level_dimension(basew, j);
            size_t levelh         = image_level_dimension(baseh, j);
            size_t leveld         = image_level_dimension(based, j);
            size_t levelp         = dds_pitch(format, levelw);
            size_t blockh         = image_max2<size_t>(1, (levelh + 3) / 4);
            dst.Index             = j;
            dst.Width             = dds_image_dimension(format, levelw);
            dst.Height            = dds_image_dimension(format, levelh);
            dst.Slices            = leveld;
            dst.BytesPerElement   = bcn ? blocksz : (bitspp / 8); // DXGI_FORMAT_R1_UNORM...?
            dst.BytesPerRow       = levelp;
            dst.BytesPerSlice     = bcn ? levelp * blockh : levelp * levelh;
            dst.DataSize          = dst.BytesPerSlice  * leveld;
            dst.LevelData         = offset;
            dst.Format            = format;
            offset               += dst.DataSize;
        }
    }
    return DDS_PARSE_STATE_RECEIVE_NEXT_ELEMENT;
}

/// @summary Implements the parser logic for the DDS_PARSE_STATE_BUFFER_HEADER.
/// @param decoder The stream decoder providing the data to consume.
/// @param ddsp The DDS parser state to update.
/// @return The updated parser state identifier.
internal_function int dds_buffer_header(stream_decoder_t *decoder, dds_parser_state_t *ddsp)
{
    size_t bytes_available   = decoder->amount();
    if (ddsp->DDSHWritePos   + bytes_available >= DDS_HEADER_BUFFER_SIZE)
    {   // this read will complete the header data.
        size_t bytes_to_copy = DDS_HEADER_BUFFER_SIZE-ddsp->DDSHWritePos;
        memcpy(&ddsp->DDSHBuffer[ddsp->DDSHWritePos], decoder->ReadCursor, bytes_to_copy);
        ddsp->DDSHWritePos  += bytes_to_copy;
        decoder->ReadCursor += bytes_to_copy;
        // check the base DDS header for the correct magic.
        uint32_t magic = *((uint32_t const*) ddsp->DDSHBuffer);
        if (magic != DDS_MAGIC_LE)
        {   // invalid magic bytes, we expect 'DDS '.
            return false;
        }
        // save a pointer to the DDS base header for easy reference.
        ddsp->DDSHeader = (dds_header_t   *) ddsp->DDSHBuffer;
        // check the fourcc to determine whether a DX10 header is expected.
        uint32_t flags  = ddsp->DDSHeader->Format.Flags;
        uint32_t fourcc = ddsp->DDSHeader->Format.FourCC;
        if ((flags & DDPF_FOURCC) != 0 && fourcc == image_fourcc_le('D','X','1','0'))
        {   // don't consume any additional data. wait for the DX10 header.
            return DDS_PARSE_STATE_BUFFER_HEADER_DX10;
        }
        else
        {   // no DX10 header present, so determine image properties and proceed.
            ddsp->DX10Header = NULL;
            return dds_parser_setup_image_info(ddsp);
        }
    }
    else
    {   // this is a partial read; consume all available bytes.
        memcpy(&ddsp->DDSHBuffer[ddsp->DDSHWritePos], decoder->ReadCursor, bytes_available);
        ddsp->DDSHWritePos  += bytes_available;
        decoder->ReadCursor += bytes_available;
        return DDS_PARSE_STATE_BUFFER_HEADER;
    }
}

/// @summary Implements the parser logic for the DDS_PARSE_STATE_BUFFER_HEADER.
/// @param decoder The stream decoder providing the data to consume.
/// @param ddsp The DDS parser state to update.
/// @return The updated parser state identifier.
internal_function int dds_buffer_header_dx10(stream_decoder_t *decoder, dds_parser_state_t *ddsp)
{
    size_t bytes_available   = decoder->amount();
    if (ddsp->DX10WritePos   + bytes_available >= DDS_HEADER10_BUFFER_SIZE)
    {   // this read will complete the header data.
        size_t bytes_to_copy = DDS_HEADER10_BUFFER_SIZE-ddsp->DX10WritePos;
        memcpy(&ddsp->DX10Buffer[ddsp->DX10WritePos], decoder->ReadCursor, bytes_to_copy);
        ddsp->DX10WritePos  += bytes_to_copy;
        decoder->ReadCursor += bytes_to_copy;
        // save a pointer to the DX10 header for easy reference.
        ddsp->DX10Header     = (dds_header_dxt10_t*) ddsp->DX10Buffer;
        // determine image properties and proceed with receiving data.
        return dds_parser_setup_image_info(ddsp);
    }
    else
    {   // this is a partial read; consume all available bytes.
        memcpy(&ddsp->DX10Buffer[ddsp->DX10WritePos], decoder->ReadCursor, bytes_available);
        ddsp->DX10WritePos  += bytes_available;
        decoder->ReadCursor += bytes_available;
        return DDS_PARSE_STATE_BUFFER_HEADER_DX10;
    }
}

/// @summary Implements the parser logic for the DDS_PARSE_STATE_RECEIVE_NEXT_ELEMENT.
/// @param decoder The stream decoder providing the data to consume.
/// @param ddsp The DDS parser state to update.
/// @return The updated parser state identifier.
internal_function int dds_receive_next_element(stream_decoder_t *decoder, dds_parser_state_t *ddsp)
{
    if (ddsp->ElementIndex == ddsp->ElementCount)
    {   // all elements (and all mip-levels) have been read. done parsing.
        return DDS_PARSE_STATE_COMPLETE;
    }
    else
    {   // set up to receive the first mip-level of cube face or array element ddsp->ElementIndex.
        ddsp->LevelIndex = 0;
        return DDS_PARSE_STATE_RECEIVE_NEXT_LEVEL;
    }
}

/// @summary Implements the parser logic for the DDS_PARSE_STATE_RECEIVE_NEXT_LEVEL.
/// @param decoder The stream decoder providing the data to consume.
/// @param ddsp The DDS parser state to update.
/// @return The updated parser state identifier.
internal_function int dds_receive_next_level(stream_decoder_t *decoder, dds_parser_state_t *ddsp)
{
    if (ddsp->LevelIndex == ddsp->LevelCount)
    {   // finished receiving all mip-levels of the current cube face or array element.
        ddsp->ElementIndex++;
        return DDS_PARSE_STATE_RECEIVE_NEXT_ELEMENT;
    }
    else
    {   // setup to write the next mip-level of the current cube face or array element.
        // ddsp->WriteCursor is incremented during the copy, so no need to recompute it.
        size_t level_idx = ddsp->ElementIndex * ddsp->LevelCount + ddsp->LevelIndex;
        ddsp->EndOfLevel = ddsp->WriteCursor  + ddsp->LevelInfo[level_idx].DataSize;
        return DDS_PARSE_STATE_ENCODE_LEVEL_DATA;
    }
}

/// @summary Implements the parser logic for the DDS_PARSE_STATE_ENCODE_LEVEL_DATA.
/// @param decoder The stream decoder providing the data to consume.
/// @param ddsp The DDS parser state to update.
/// @return The updated parser state identifier.
internal_function int dds_parse_state_encode_level(stream_decoder_t *decoder, dds_parser_state_t *ddsp)
{
    size_t bytes_available   = decoder->amount();
    if (ddsp->WriteCursor    + bytes_available >= ddsp->EndOfLevel)
    {   // consume enough data to finish out the current mip-level.
        // NOTE(rlk): if not IMAGE_ENCODING_RAW, memcpy is something more complex.
        size_t bytes_to_copy = ddsp->EndOfLevel - ddsp->WriteCursor;
        memcpy(ddsp->WriteCursor, decoder->ReadCursor, bytes_to_copy);
        decoder->ReadCursor += bytes_to_copy;
        ddsp->WriteCursor   += bytes_to_copy;
        ddsp->LevelIndex++;
        return DDS_PARSE_STATE_RECEIVE_NEXT_LEVEL;
    }
    else
    {   // consume all available data; yield until more is available.
        // NOTE(rlk): if not IMAGE_ENCODING_RAW, memcpy is something more complex.
        memcpy(ddsp->WriteCursor, decoder->ReadCursor, bytes_available);
        decoder->ReadCursor += bytes_available;
        ddsp->WriteCursor   += bytes_available;
        return DDS_PARSE_STATE_ENCODE_LEVEL_DATA;
    }
}
// Each state implemented by a function int state_name(stream_decoder_t*, dds_parser_state_t*)
// Call state function in a loop while decoder->ReadCursor != decoder->FinalByte
// Each state implementation consumes up to the available amount of data, then yields
// - Returns its own state ID if it's waiting for more data
// Outer loop performs the refill and end-checking
//
// dds_parser_t (not dds_parser_state_t) maintains MPSC unbounded FIFO accepting stream_control_t.
// also receives pointer to MPSC notify queue where uintptr_t app IDs and results are placed when load completed.
// dds_parser_t has a main tick function that is called from some image upload thread or whatever.
//
// One image blob maintained per-format+encoding. Each blob starts out not having any address space reserved.
// Address space is reserved only when the first image is added; then committed as-needed.
// The default encoding is IMAGE_ENCODING_RAW, which means data is stored as-is (mempcy).
// Could also have IMAGE_ENCODING_JPEG_RLE, etc. to directly store compressed data for upload.
// The DDS parser always uses IMAGE_ENCODING_RAW, but DICOM parser would use other encodings.
// Image blob module exposes functions to perform the encoding for a block of data and flush.
// Format+Encoding pair uniquely identifies blob.
//
// All DDS enums and structures need to be put in a header file as they are referenced throughout 
// the imaging pipeline (the image blob implementation needs them, etc.)

/*////////////////////////
//   Public Functions   //
////////////////////////*/
/// @summary Reads the surface header present in all DDS files.
/// @param data The buffer from which the header data should be read.
/// @param data_size The maximum number of bytes to read from the input buffer.
/// @param out_header Pointer to the structure to populate.
/// @return true if the file appears to be a valid DDS file.
public_function bool dds_header(void const *data, size_t data_size, dds_header_t *out_header)
{
    size_t const offset   = sizeof(uint32_t);
    size_t const min_size = offset + sizeof(dds_header_t);

    if (data == NULL)
    {   // no input data specified.
        return false;
    }
    if (data_size < min_size)
    {   // not enough input data specified.
        return false;
    }

    uint32_t magic = *((uint32_t const*) data);
    if (magic != DDS_MAGIC_LE)
    {   // invalid magic bytes, we expect 'DDS '.
        return false;
    }

    if (out_header)
    {
        *out_header = *(data_at<dds_header_t>(data, offset));
    }
    return true;
}

/// @summary Reads the extended surface header from a DDS buffer, if present.
/// @param data The buffer from which the header data should be read.
/// @param data_size The maximum number of bytes to read from the input buffer.
/// @param out_header Pointer to the structure to populate.
/// @return true if the file contains an extended surface header.
public_function bool dds_header_dxt10(void const *data, size_t data_size, dds_header_dxt10_t *out_header)
{
    dds_header_t header;
    if (dds_header(data, data_size, &header))
    {
        size_t const    offset   = sizeof(uint32_t) + sizeof(dds_header_t);
        size_t const    min_size = offset + sizeof(dds_header_dxt10_t);
        if (data_size < min_size)
        {   // not enough input data specified.
            return false;
        }

        if ((header.Format.Flags & DDPF_FOURCC) == 0)
        {   // no fourCC present on this file.
            return false;
        }

        if (header.Format.FourCC != image_fourcc_le('D','X','1','0'))
        {   // the expected DX10 fourCC isn't present, so there's no header.
            return false;
        }

        if (out_header)
        {
            *out_header = *(data_at<dds_header_dxt10_t>(data, offset));
        }
        return true;
    }
    else return false;
}


/// @summary Retrieves a description of and pointer to the start of the data for a mipmap level.
/// @param data The buffer from which the data should be read.
/// @param data_size The maximum number of bytes to read from the input buffer.
/// @param header The base surface header of the DDS.
/// @param header_ex The extended surface header of the DDS, or NULL.
/// @param out_levels A buffer of dds_array_count() * dds_level_count() level
/// descriptors to populate with data (or max_levels, whichever is less.)
/// @param max_levels The maximum number of items to write to out_levels.
/// @return The number of level descriptors written to out_levels.
public_function size_t dds_describe(void const *data, size_t data_size, dds_header_t const *header, dds_header_dxt10_t const *header_ex, dds_level_desc_t *out_levels, size_t max_levels)
{
    uint32_t  format = DXGI_FORMAT_UNKNOWN;
    size_t offset    = 0;
    size_t basew     = 0;
    size_t baseh     = 0;
    size_t based     = 0;
    size_t bitspp    = 0;
    size_t blocksz   = 0;
    size_t nitems    = dds_array_count(header, header_ex);
    size_t nlevels   = dds_level_count(header, header_ex);
    size_t dst_i     = 0;
    bool   bcn       = false;

    if (header == NULL)
    {   // the base DDS header is required.
        return false;
    }

    // get some basic data about the DDS.
    format  = dds_format(header, header_ex);
    bitspp  = dds_bits_per_pixel(format);
    blocksz = dds_bytes_per_block(format);
    basew   = (header->Flags & DDSD_WIDTH)  ? header->Width  : 0;
    baseh   = (header->Flags & DDSD_HEIGHT) ? header->Height : 0;
    based   = dds_volume(header, header_ex) ? header->Depth  : 1;
    bcn     = (blocksz > 0);

    // now update the byte offset and data pointer, and write the output.
    offset += sizeof(uint32_t);
    offset += sizeof(dds_header_t);
    if (header_ex) offset += sizeof(dds_header_dxt10_t);
    for (size_t i = 0; i < nitems && dst_i < max_levels; ++i)
    {
        for (size_t j = 0; j < nlevels && dst_i < max_levels && offset < data_size; ++j)
        {
            dds_level_desc_t &dst = out_levels[dst_i++];
            size_t levelw         = image_level_dimension(basew, j);
            size_t levelh         = image_level_dimension(baseh, j);
            size_t leveld         = image_level_dimension(based, j);
            size_t levelp         = dds_pitch(format, levelw);
            size_t blockh         = image_max2<size_t>(1, (levelh + 3) / 4);
            dst.Index             = j;
            dst.Width             = dds_image_dimension(format, levelw);
            dst.Height            = dds_image_dimension(format, levelh);
            dst.Slices            = leveld;
            dst.BytesPerElement   = bcn ? blocksz : (bitspp / 8); // DXGI_FORMAT_R1_UNORM...?
            dst.BytesPerRow       = levelp;
            dst.BytesPerSlice     = bcn ? levelp * blockh : levelp * levelh;
            dst.DataSize          = dst.BytesPerSlice  * leveld;
            dst.LevelData         = offset;
            dst.Format            = format;
            offset               += dst.DataSize;
        }
    }
    return dst_i;
}

public_function void dds_parser_state_init(dds_parser_state_t *ddsp, uintptr_t image_id)
{
    ddsp->CurrentState  = DDS_PARSE_STATE_BUFFER_HEADER;
    ddsp->ParserError   = DDS_PARSE_ERROR_SUCCESS;
    ddsp->ImageFormat   = DXGI_FORMAT_UNKNOWN;
    ddsp->ImageEncoding = IMAGE_ENCODING_RAW;
    ddsp->Identifier    = image_id;
    ddsp->ElementCount  = 0;
    ddsp->ElementIndex  = 0;
    ddsp->LevelCount    = 0;
    ddsp->LevelIndex    = 0;
    ddsp->BitsPerPixel  = 0;
    ddsp->BytesPerBlock = 0;
    ddsp->BaseDimension[0] = 1; // width
    ddsp->BaseDimension[1] = 1; // height
    ddsp->BaseDimension[2] = 1; // depth
    ddsp->DDSHeader     = NULL;
    ddsp->DX10Header    = NULL;
    ddsp->LevelInfo     = NULL;
    ddsp->WriteCursor   = NULL;
    ddsp->DDSHWritePos  = 0;
    ddsp->DX10WritePos  = 0;
}

public_function void dds_parser_state_free(dds_parser_state_t *ddsp)
{
    if (ddsp->LevelInfo != NULL)
    {
        free(ddsp->LevelInfo);
        ddsp->LevelInfo  = NULL;
    }
}
