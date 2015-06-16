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
    DDS_PARSE_STATE_SEEK_OFFSET          = 0,   /// The parser is looking for a known byte offset.
    DDS_PARSE_STATE_FIND_MAGIC           = 1,   /// The parser is looking for the magic bytes.
    DDS_PARSE_STATE_BUFFER_HEADER        = 2,   /// The parser is receiving data for the core DDS file header.
    DDS_PARSE_STATE_BUFFER_HEADER_DX10   = 3,   /// The parser is receiving data for the DX10 extended header.
    DDS_PARSE_STATE_RECEIVE_NEXT_ELEMENT = 4,   /// The parser is beginning receipt of the next cube face or array element.
    DDS_PARSE_STATE_RECEIVE_NEXT_LEVEL   = 5,   /// The parser is beginning receipt of the next mipmap level.
    DDS_PARSE_STATE_ENCODE_LEVEL_DATA    = 6,   /// The parser is receiving and encoding mipmap level data. 
    DDS_PARSE_STATE_COMPLETE             = 7,   /// The parser has processed the entire file contents.
    DDS_PARSE_STATE_ERROR                = 8    /// The parser has encountered a fatal error.
};

/// @summary Define identifiers for the recognized parser errors.
enum dds_parser_error_e : int
{
    DDS_PARSE_ERROR_SUCCESS              = 0,   /// No error has occurred.
    DDS_PARSE_ERROR_NOMEMORY             = 1,   /// Required memory could not be allocated.
    DDS_PARSE_ERROR_DECODER              = 2,   /// The underlying stream decoder returned an error.
};

/// @summary Define the possible return codes from the top-level streaming parser update function.
enum dds_parser_result_e : int
{
    DDS_PARSE_RESULT_CONTINUE            = 0,   /// The parser is yielding, waiting for more data.
    DDS_PARSE_RESULT_COMPLETE            = 1,   /// Stop parsing. All data was parsed successfully.
    DDS_PARSE_RESULT_ERROR               = 2    /// Stop parsing. An error was encountered.
};

/// @summary Define the state data associated with a streaming DDS file parser.
struct dds_parser_state_t
{
    #define BUF_DDSH      DDS_HEADER_BUFFER_SIZE
    #define BUF_DX10      DDS_HEADER10_BUFFER_SIZE
    int                   CurrentState;         /// One of dds_parser_state_e.
    int                   ParserError;          /// One of dds_parser_error_e.
    image_parser_config_t Config;               /// The input parser configuration.
    image_definition_t   *Metadata;             /// Pointer to the image metadata block.
    size_t                ElementFinal;         /// The zero-based index of the final face or array element to read.
    size_t                ElementIndex;         /// The zero-based index of the face or array element currently being read.
    size_t                LevelCount;           /// The number of levels in a single mipmap chain.
    size_t                LevelIndex;           /// The zero-based index of the level being parsed.
    dds_header_t         *DDSHeader;            /// Pointer to DDSHBuffer, if it's a valid DDS header.
    dds_header_dxt10_t   *DX10Header;           /// Pointer to DX10Buffer, if it's a valid header.
    dds_level_desc_t     *LevelInfo;            /// Mipmap level descriptors, dds_level_count() total. Owned by the image blob.
    stream_decode_pos_t  *BlockOffsets;         /// Mipmap level byte offsets, dds_array_count() * dds_level_count() total. Owned by the image blob.
    size_t                LevelWrite;           /// The current write position in the current level data.
    size_t                LevelSize;            /// The size of the level being written to image data.
    size_t                DDSHWritePos;         /// The current write position in DDSHBuffer.
    size_t                DX10WritePos;         /// The current write position in DX10Buffer.
    uint8_t               DDSHBuffer[BUF_DDSH]; /// Internal buffer for the base image header.
    uint8_t               DX10Buffer[BUF_DX10]; /// Internal buffer for the extended DX10 header.
    uint32_t              MagicBuffer;          /// Internal buffer for the magic FourCC.
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
/// @param encoder The encoder used to write data to image memory and specify image attributes.
/// @return The new parser state.
internal_function int dds_parser_setup_image_info(dds_parser_state_t *ddsp, image_encoder_t *encoder)
{
    image_definition_t   *meta  = ddsp->Metadata;
    if (ddsp->Config.ParseFlags & IMAGE_PARSER_FLAGS_READ_METADATA)
    {   // completely initialize the metadata block with information we've read.
        dds_header_t        *dds      = ddsp->DDSHeader;
        dds_header_dxt10_t  *dx10     = ddsp->DX10Header;
        uint32_t             format   = dxgi_format(dds, dx10);
        size_t               basew    =(dds->Flags & DDSD_WIDTH ) ? dds->Width  : 0;
        size_t               baseh    =(dds->Flags & DDSD_HEIGHT) ? dds->Height : 0;
        size_t               based    = dxgi_volume(dds, dx10)    ? dds->Depth  : 1;
        size_t               bitspp   = dxgi_bits_per_pixel(format);
        size_t               blocksz  = dxgi_bytes_per_block(format);
        size_t               nitems   = dxgi_array_count(dds, dx10);
        size_t               nlevels  = dxgi_level_count(dds, dx10);
        bool                 blockcf  =(blocksz > 0);
        dds_level_desc_t    *levels   = NULL;
        stream_decode_pos_t *offsets  = NULL;

        // allocate storage for the mipmap level descriptors and byte offsets.
        if ((levels = (dds_level_desc_t*) malloc(nlevels * sizeof(dds_level_desc_t))) == NULL)
        {   // unable to allocate the necessary storage for mip-level descriptors.
            ddsp->ParserError = DDS_PARSE_ERROR_NOMEMORY;
            return DDS_PARSE_STATE_ERROR;
        }
        if ((offsets = (stream_decode_pos_t*) malloc(nitems * nlevels * sizeof(stream_decode_pos_t))) == NULL)
        {   // unable to allocate the necessary storage for mip-level byte offsets.
            free(levels); ddsp->ParserError = DDS_PARSE_ERROR_NOMEMORY;
            return DDS_PARSE_STATE_ERROR;
        }

        // initialize basic metadata attributes:
        meta->ImageId      = ddsp->Config.ImageId;
        meta->ImageFormat  = format;
        meta->Compression  = IMAGE_COMPRESSION_NONE;
        meta->Width        = basew;
        meta->Height       = baseh;
        meta->SliceCount   = based;
        meta->ElementIndex = 0;
        meta->ElementCount = nitems;
        meta->LevelCount   = nlevels;
        meta->BytesPerPixel= bitspp;
        meta->BytesPerBlock= blocksz;
        meta->DDSHeader    =*dds;
        meta->DX10Header   =*dx10;
        meta->LevelInfo    = levels;
        meta->BlockOffsets = offsets;

        // initialize mipmap chain attributes:
        for (size_t i = 0; i < nlevels; ++i)
        {
            dds_level_desc_t &dst = levels[i];
            size_t levelw         = image_level_dimension(basew, i);
            size_t levelh         = image_level_dimension(baseh, i);
            size_t leveld         = image_level_dimension(based, i);
            size_t levelp         = dxgi_pitch(format, levelw);
            size_t blockh         = image_max2<size_t>(1, (levelh + 3) / 4);
            dst.Index             = i;
            dst.Width             = dxgi_image_dimension(format, levelw);
            dst.Height            = dxgi_image_dimension(format, levelh);
            dst.Slices            = leveld;
            dst.BytesPerElement   = blockcf ? blocksz : (bitspp / 8); // DXGI_FORMAT_R1_UNORM...?
            dst.BytesPerRow       = levelp;
            dst.BytesPerSlice     = blockcf ? levelp  *  blockh : levelp * levelh;
            dst.DataSize          = dst.BytesPerSlice *  leveld;
            dst.Format            = format;
        }

        // zero out all of the offset data. it will be set as the container is parsed.
        memset(offsets, 0, nitems * nlevels * sizeof(stream_decode_pos_t));

        // notify the encoder of the image attributes:
        encoder->define_image(meta);
    }

    // initialize internal state based on parser flags and image metadata.
    if (ddsp->Config.FinalFrame > meta->ElementCount)
    {   // read all elements starting from the specified frame to the end.
        ddsp->Config.FinalFrame = meta->ElementCount;
    }
    ddsp->ElementIndex = ddsp->Config.FirstFrame;
    ddsp->ElementFinal = ddsp->Config.FinalFrame;
    ddsp->LevelIndex   = 0;
    ddsp->LevelCount   = meta->LevelCount;
    ddsp->LevelInfo    = meta->LevelInfo;
    ddsp->BlockOffsets = meta->BlockOffsets;

    // transition to the appropriate state based on parser configuration.
    if ((ddsp->Config.ParseFlags & IMAGE_PARSER_FLAGS_READ_PIXELS) == 0)
    {   // not reading any pixel data, so we're done.
        return DDS_PARSE_STATE_COMPLETE;
    }
    else
    {   // proceed with reading pixel data.
        return DDS_PARSE_STATE_RECEIVE_NEXT_ELEMENT;
    }
}

/// @summary Implements the parser logic for the DDS_PARSE_STATE_SEEK_OFFSET.
/// @param decoder The stream decoder providing the data to consume.
/// @param ddsp The DDS parser state to update.
/// @param encoder The image encoder to which pixel data should be written.
/// @return The updated parser state identifier.
internal_function int dds_seek_offset(stream_decoder_t *decoder, dds_parser_state_t *ddsp, image_encoder_t *encoder)
{   UNREFERENCED_PARAMETER(encoder);
    stream_decode_pos_t &target  = ddsp->Config.StartOffset;
    stream_decode_pos_t  current;  decoder->pos(current);
    if (current.FileOffset      >= target.FileOffset && 
        current.FileOffset      <= target.FileOffset)
    {   // this encoded data chunk contains the start of the data we're looking for.
        size_t available         = decoder->amount();
        size_t consume           =(target.DecodeOffset >= available) ? available : target.DecodeOffset;
        decoder->ReadCursor     += consume; // consume from available decoded input
        target.DecodeOffset     -= consume; // decrement bytes remaining to consume
        if (decoder->ReadCursor != decoder->FinalByte)
        {   // ReadCursor is positioned on the first byte to read.
            if (ddsp->Config.ParseFlags & IMAGE_PARSER_FLAGS_READ_METADATA)
            {   // proceed with reading the image metadata.
                return DDS_PARSE_STATE_FIND_MAGIC;
            }
            else
            {   // skip metadata, reading pixels only.
                return dds_parser_setup_image_info(ddsp, encoder);
            }
        }
        // else, refill the decoded data buffer and remain in the same state.
    }
    return DDS_PARSE_STATE_SEEK_OFFSET;
}

/// @summary Implements the parser logic for the DDS_PARSE_STATE_FIND_MAGIC.
/// @param decoder The stream decoder providing the data to consume.
/// @param ddsp The DDS parser state to update.
/// @param encoder The image encoder to which pixel data should be written.
/// @return The updated parser state identifier.
internal_function int dds_find_magic(stream_decoder_t *decoder, dds_parser_state_t *ddsp, image_encoder_t *encoder)
{   UNREFERENCED_PARAMETER(encoder);
    while (decoder->ReadCursor != decoder->FinalByte)
    {
        ddsp->MagicBuffer >>= 8;                             // shift eight bits off the right
        ddsp->MagicBuffer  |=(*decoder->ReadCursor++) << 24; // add eight bits at the left
        if (ddsp->MagicBuffer == DDS_MAGIC_LE)
            return DDS_PARSE_STATE_BUFFER_HEADER;
    }
    return DDS_PARSE_STATE_FIND_MAGIC;
}

/// @summary Implements the parser logic for the DDS_PARSE_STATE_BUFFER_HEADER.
/// @param decoder The stream decoder providing the data to consume.
/// @param ddsp The DDS parser state to update.
/// @param encoder The image encoder to which pixel data should be written.
/// @return The updated parser state identifier.
internal_function int dds_buffer_header(stream_decoder_t *decoder, dds_parser_state_t *ddsp, image_encoder_t *encoder)
{   UNREFERENCED_PARAMETER(encoder);
    size_t bytes_available   = decoder->amount();
    if (ddsp->DDSHWritePos   + bytes_available >= DDS_HEADER_BUFFER_SIZE)
    {   // this read will complete the header data.
        size_t bytes_to_copy = DDS_HEADER_BUFFER_SIZE-ddsp->DDSHWritePos;
        memcpy(&ddsp->DDSHBuffer[ddsp->DDSHWritePos], decoder->ReadCursor, bytes_to_copy);
        ddsp->DDSHWritePos  += bytes_to_copy;
        decoder->ReadCursor += bytes_to_copy;
        // save a pointer to the DDS base header for easy reference.
        ddsp->DDSHeader = (dds_header_t   *)ddsp->DDSHBuffer;
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
            return dds_parser_setup_image_info(ddsp, encoder);
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
/// @param encoder The image encoder to which pixel data should be written.
/// @return The updated parser state identifier.
internal_function int dds_buffer_header_dx10(stream_decoder_t *decoder, dds_parser_state_t *ddsp, image_encoder_t *encoder)
{   UNREFERENCED_PARAMETER(encoder);
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
        return dds_parser_setup_image_info(ddsp, encoder);
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
/// @param encoder The image encoder to which pixel data should be written.
/// @return The updated parser state identifier.
internal_function int dds_receive_next_element(stream_decoder_t *decoder, dds_parser_state_t *ddsp, image_encoder_t *encoder)
{   UNREFERENCED_PARAMETER(decoder);
    if (ddsp->ElementIndex == ddsp->ElementFinal)
    {   // all elements (and all mip-levels) have been read. done parsing.
        return DDS_PARSE_STATE_COMPLETE;
    }
    else
    {   // set up to receive the first mip-level of cube face or array element ddsp->ElementIndex.
        ddsp->LevelIndex = 0;
        encoder->reset_element(ddsp->ElementIndex);
        return DDS_PARSE_STATE_RECEIVE_NEXT_LEVEL;
    }
}

/// @summary Implements the parser logic for the DDS_PARSE_STATE_RECEIVE_NEXT_LEVEL.
/// @param decoder The stream decoder providing the data to consume.
/// @param ddsp The DDS parser state to update.
/// @param encoder The image encoder to which pixel data should be written.
/// @return The updated parser state identifier.
internal_function int dds_receive_next_level(stream_decoder_t *decoder, dds_parser_state_t *ddsp, image_encoder_t *encoder)
{
    if (ddsp->LevelIndex == ddsp->LevelCount)
    {   // finished receiving all mip-levels of the current cube face or array element.
        encoder->mark_element(ddsp->ElementIndex++);
        return DDS_PARSE_STATE_RECEIVE_NEXT_ELEMENT;
    }
    else
    {   // setup to write the next mip-level of the current cube face or array element.
        decoder->pos(ddsp->BlockOffsets[(ddsp->ElementIndex * ddsp->LevelCount) + ddsp->LevelIndex]);
        ddsp->LevelSize  = ddsp->LevelInfo[ddsp->LevelIndex].DataSize;
        ddsp->LevelWrite = 0;
        return DDS_PARSE_STATE_ENCODE_LEVEL_DATA;
    }
}

/// @summary Implements the parser logic for the DDS_PARSE_STATE_ENCODE_LEVEL_DATA.
/// @param decoder The stream decoder providing the data to consume.
/// @param ddsp The DDS parser state to update.
/// @param encoder The image encoder to which pixel data should be written.
/// @return The updated parser state identifier.
internal_function int dds_encode_level(stream_decoder_t *decoder, dds_parser_state_t *ddsp, image_encoder_t *encoder)
{
    size_t bytes_available   = decoder->amount();
    if (ddsp->LevelWrite     + bytes_available >= ddsp->LevelSize)
    {   // consume enough data to finish out the current mip-level.
        size_t bytes_to_copy = ddsp->LevelSize  - ddsp->LevelWrite;
        encoder->encode(ddsp->ElementIndex, decoder->ReadCursor, bytes_to_copy);
        encoder->mark_level(ddsp->ElementIndex);
        decoder->ReadCursor += bytes_to_copy;
        ddsp->LevelWrite    += bytes_to_copy;
        ddsp->LevelIndex++;
        return DDS_PARSE_STATE_RECEIVE_NEXT_LEVEL;
    }
    else
    {   // consume all available data; yield until more is available.
        encoder->encode(ddsp->ElementIndex, decoder->ReadCursor, bytes_available);
        decoder->ReadCursor += bytes_available;
        ddsp->LevelWrite    += bytes_available;
        return DDS_PARSE_STATE_ENCODE_LEVEL_DATA;
    }
}

/// @summary Implements the primary update tick for a streaming DDS parser.
/// @param decoder The stream decoder providing the data to consume.
/// @param ddsp The DDS parser state to update.
/// @param encoder The image encoder to which pixel data should be written.
/// @return One of dds_parser_result_e.
internal_function int dds_parser_update(stream_decoder_t *decoder, dds_parser_state_t *ddsp, image_encoder_t *encoder)
{
    while (!decoder->atend())
    {   // refill the decoder buffer with any available data.
        switch (decoder->refill(decoder))
        {
        case STREAM_REFILL_RESULT_START:
            break;                            // begin reading decoded data.
        case STREAM_REFILL_RESULT_YIELD:
            return DDS_PARSE_RESULT_CONTINUE; // no additional data available at this time.
        case STREAM_REFILL_RESULT_ERROR:
            ddsp->ParserError = DDS_PARSE_ERROR_DECODER;
            return DDS_PARSE_RESULT_ERROR;    // the underlying stream encountered an error.
        }
        // parse as much decoded data as possible.
        while (decoder->ReadCursor != decoder->FinalByte)
        {   // dispatch to the appropriate function based on parser state.
            int new_state;
            switch (ddsp->CurrentState)
            {
            case DDS_PARSE_STATE_SEEK_OFFSET:
                new_state = dds_seek_offset(decoder, ddsp, encoder);
                break;
            case DDS_PARSE_STATE_FIND_MAGIC:
                new_state = dds_find_magic(decoder, ddsp, encoder);
                break;
            case DDS_PARSE_STATE_BUFFER_HEADER:
                new_state = dds_buffer_header(decoder, ddsp, encoder);
                break;
            case DDS_PARSE_STATE_BUFFER_HEADER_DX10:
                new_state = dds_buffer_header_dx10(decoder, ddsp, encoder);
                break;
            case DDS_PARSE_STATE_RECEIVE_NEXT_ELEMENT:
                new_state = dds_receive_next_element(decoder, ddsp, encoder);
                break;
            case DDS_PARSE_STATE_RECEIVE_NEXT_LEVEL:
                new_state = dds_receive_next_level(decoder, ddsp, encoder);
                break;
            case DDS_PARSE_STATE_ENCODE_LEVEL_DATA:
                new_state = dds_encode_level(decoder, ddsp, encoder);
                break;
            case DDS_PARSE_STATE_COMPLETE:
                return DDS_PARSE_RESULT_COMPLETE;
            case DDS_PARSE_STATE_ERROR:
                return DDS_PARSE_RESULT_ERROR;
            default:
                new_state = ddsp->CurrentState;
                break;
            }
            ddsp->CurrentState = new_state;
        }
    }
    // reached end-of-stream; process any final state updates.
    // these states do not require any input data to execute.
    for ( ; ; )
    {
        switch (ddsp->CurrentState)
        {
        case DDS_PARSE_STATE_RECEIVE_NEXT_ELEMENT:
            ddsp->CurrentState = dds_receive_next_element(decoder, ddsp, encoder);
            break;
        case DDS_PARSE_STATE_RECEIVE_NEXT_LEVEL:
            ddsp->CurrentState = dds_receive_next_level(decoder, ddsp, encoder);
            break;
        case DDS_PARSE_STATE_COMPLETE:
            return DDS_PARSE_RESULT_COMPLETE;
        default:
            return DDS_PARSE_RESULT_ERROR;
        }
    }
    // and then return the final status.
    return (ddsp->ParserError == DDS_PARSE_ERROR_SUCCESS) ? DDS_PARSE_RESULT_COMPLETE : DDS_PARSE_RESULT_ERROR;
}

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


/// @summary Builds a description of the contents of a DDS file loaded into memory.
/// @param data The buffer containing the entire contents of the DDS file.
/// @param data_size The maximum number of bytes to read from the input buffer.
/// @param header The base surface header of the DDS.
/// @param header_ex The extended surface header of the DDS, or NULL.
/// @param out_levels An array of dxgi_level_count() level descriptors to populate with data (or max_levels, whichever is less.)
/// @param out_offsets An array of dxgi_array_count() * dxgi_level_count()|max_levels values to populate with byte offsets. 
/// @param max_levels The maximum number of items to write to out_levels.
/// @return The number of level descriptors written to out_levels.
public_function size_t dds_describe(void const *data, size_t data_size, dds_header_t const *header, dds_header_dxt10_t const *header_ex, dds_level_desc_t *out_levels, size_t *out_offsets, size_t max_levels)
{   UNREFERENCED_PARAMETER(data);
    UNREFERENCED_PARAMETER(data_size);
    uint32_t  format  = DXGI_FORMAT_UNKNOWN;
    size_t    offset  = 0;
    size_t    basew   = 0;
    size_t    baseh   = 0;
    size_t    based   = 0;
    size_t    bitspp  = 0;
    size_t    blocksz = 0;
    size_t    nitems  = dxgi_array_count(header, header_ex);
    size_t    nlevels = dxgi_level_count(header, header_ex);
    size_t    dst_i   = 0;
    bool      blockcf = false;

    if (header == NULL)
    {   // the base DDS header is required.
        return false;
    }

    // get some basic data about the DDS.
    format  = dxgi_format(header, header_ex);
    bitspp  = dxgi_bits_per_pixel(format);
    blocksz = dxgi_bytes_per_block(format);
    basew   = (header->Flags & DDSD_WIDTH)   ? header->Width  : 0;
    baseh   = (header->Flags & DDSD_HEIGHT)  ? header->Height : 0;
    based   = dxgi_volume(header, header_ex) ? header->Depth  : 1;
    blockcf = (blocksz > 0);

    // now update the byte offset and data pointer, and write the output.
    offset += sizeof(uint32_t);
    offset += sizeof(dds_header_t);
    if (header_ex) offset += sizeof(dds_header_dxt10_t);

    // generate the mipmap level descriptors. each cube face or array element
    // has the same dimensions, and thus the same mipmap chain layout.
    size_t stride = 0;
    for (size_t i = 0; i < nlevels && i < max_levels; ++i)
    {
        dds_level_desc_t &dst = out_levels[dst_i++];
        size_t levelw         = image_level_dimension(basew, i);
        size_t levelh         = image_level_dimension(baseh, i);
        size_t leveld         = image_level_dimension(based, i);
        size_t levelp         = dxgi_pitch(format, levelw);
        size_t blockh         = image_max2<size_t>(1, (levelh + 3) / 4);
        dst.Index             = i;
        dst.Width             = dxgi_image_dimension(format, levelw);
        dst.Height            = dxgi_image_dimension(format, levelh);
        dst.Slices            = leveld;
        dst.BytesPerElement   = blockcf ? blocksz : (bitspp / 8); // DXGI_FORMAT_R1_UNORM...?
        dst.BytesPerRow       = levelp;
        dst.BytesPerSlice     = blockcf ? levelp  *  blockh : levelp * levelh;
        dst.DataSize          = dst.BytesPerSlice *  leveld;
        dst.Format            = format;
        stride               += dst.DataSize;
    }

    // generate the byte offsets of each level, for each element or cube face.
    // byte offsets are specified from the start of the input buffer.
    for (size_t i = 0; i < nitems; i++)
    {   // for each array element or cube face...
        for (size_t j = 0; j < nlevels && j < max_levels; ++j)
        {   // for each desired level in the mipchain...
            out_offsets[(i * nlevels) + j]  = offset;
            offset += out_levels[j].DataSize;
        }
    }
    return image_min2<size_t>(nlevels, max_levels);
}

/// @summary Initializes or resets the state of a streaming DDS parser.
/// @param ddsp The streaming DDS parser state to initialize.
/// @param config Parser configuration data indicating what portions of the file to parse.
public_function void dds_parser_state_init(dds_parser_state_t *ddsp, image_parser_config_t const &config)
{
    ddsp->CurrentState  = DDS_PARSE_STATE_SEEK_OFFSET;
    ddsp->ParserError   = DDS_PARSE_ERROR_SUCCESS;
    ddsp->Config        = config;
    ddsp->Metadata      = config.Metadata;
    ddsp->ElementFinal  = 0;
    ddsp->ElementIndex  = 0;
    ddsp->LevelCount    = 0;
    ddsp->LevelIndex    = 0;
    ddsp->DDSHeader     = NULL;
    ddsp->DX10Header    = NULL;
    ddsp->LevelInfo     = config.Metadata->LevelInfo;
    ddsp->BlockOffsets  = config.Metadata->BlockOffsets;
    ddsp->LevelWrite    = 0;
    ddsp->LevelSize     = 0;
    ddsp->DDSHWritePos  = 0;
    ddsp->DX10WritePos  = 0;
    ddsp->MagicBuffer   = 0;
}
