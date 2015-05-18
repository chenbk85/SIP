/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines some utility functions for determining text encodings and
/// parsing text to numeric data and vice versa.
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
/// @summary A string defining the valid characters in a base-64 encoding.
/// This table is used when encoding binary data to base-64.
local_persist char const        Base64_Chars[]   =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/// @summary A lookup table mapping the 256 possible values of a byte to a
/// value in [0, 63] (or -1 for invalid values in a base-64 encoding.) This
/// table is used when decoding base64-encoded binary data.
local_persist signed char const Base64_Indices[] =
{
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,

    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, 62, -1, -1, -1, 63,  /* ... , '+', ... '/' */
    52, 53, 54, 55, 56, 57, 58, 59,  /* '0' - '7'          */
    60, 61, -1, -1, -1, -1, -1, -1,  /* '8', '9', ...      */

    -1, 0,  1,  2,  3,  4,  5,  6,   /* ..., 'A' - 'G'     */
     7, 8,  9,  10, 11, 12, 13, 14,  /* 'H' - 'O'          */
    15, 16, 17, 18, 19, 20, 21, 22,  /* 'P' - 'W'          */
    23, 24, 25, -1, -1, -1, -1, -1,  /* 'X', 'Y', 'Z', ... */

    -1, 26, 27, 28, 29, 30, 31, 32,  /* ..., 'a' - 'g'     */
    33, 34, 35, 36, 37, 38, 39, 40,  /* 'h' - 'o'          */
    41, 42, 43, 44, 45, 46, 47, 48,  /* 'p' - 'w'          */
    49, 50, 51, -1, -1, -1, -1, -1,  /* 'x', 'y', 'z', ... */

    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,

    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,

    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,

    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1
};

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Defines the different text encodings that can be detected by
/// inspecting the first four bytes of a text document for a byte order marker.
enum text_encoding_e
{
    TEXT_ENCODING_UNSURE   = 0,
    TEXT_ENCODING_ASCII    = 1,
    TEXT_ENCODING_UTF8     = 2,
    TEXT_ENCODING_UTF16_BE = 3,
    TEXT_ENCODING_UTF16_LE = 4,
    TEXT_ENCODING_UTF32_BE = 5,
    TEXT_ENCODING_UTF32_LE = 6,
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
internal_function inline const T& min2(const T& a, const T& b)
{
    return (a < b) ? a : b;
}

/// @summary Returns the maximum of two values.
/// @param a...b The input values.
/// @return The larger of the input values.
template<class T>
internal_function inline const T& max2(const T& a, const T& b)
{
    return (a < b) ? b : a;
}

/*////////////////////////
//   Public Functions   //
////////////////////////*/
/// @summary Utility function to return a pointer to the data at a given byte
/// offset from the start of a buffer, cast to the desired type.
/// @param buf Pointer to the buffer.
/// @param offset The byte offset relative to the buffer pointer.
/// @return A pointer to the data at the specified byte offset.
template <typename T>
public_function inline T const* data_at(void const *buf, ptrdiff_t offset)
{
    uint8_t const *base_ptr = (uint8_t const*) buf;
    uint8_t const *data_ptr = base_ptr + offset;
    return (T const*) data_ptr;
}

/// @summary Determines if a character represents a decimal digit.
/// @param ch The input character.
/// @return true if ch is any of '0', '1', '2', '3', '4', '5', '6', '7', '8' or '9'.
public_function inline bool is_digit(char ch)
{
    return (ch >= '0' && ch <= '9');
}

/// @summary Generates a little-endian FOURCC.
/// @param a...d The four characters comprising the code.
/// @return The packed four-cc value, in little-endian format.
public_function inline uint32_t fourcc_le(char a, char b, char c, char d)
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
public_function inline uint32_t fourcc_be(char a, char b, char c, char d)
{
    uint32_t A = (uint32_t) a;
    uint32_t B = (uint32_t) b;
    uint32_t C = (uint32_t) c;
    uint32_t D = (uint32_t) d;
    return ((A << 24) | (B << 16) | (C << 8) | (D << 0));
}


/// @summary Gets the bytes (up to four) representing the Unicode BOM associated with a specific text encoding.
/// @param encoding One of the text encoding constants, specifying the encoding for which the BOM will be returned.
/// @param out_BOM An array of four bytes that will hold the BOM corresponding to the specified text encoding. Between zero and four bytes will be written to this array.
/// @return The number of bytes written to @a out_BOM.
public_function size_t bom(int32_t encoding, uint8_t out_BOM[4])
{
    size_t  bom_size = 0;
    switch (encoding)
    {
        case TEXT_ENCODING_UTF8:
            {
                bom_size   = 3;
                out_BOM[0] = 0xEF;
                out_BOM[1] = 0xBB;
                out_BOM[2] = 0xBF;
                out_BOM[3] = 0x00;
            }
            break;

        case TEXT_ENCODING_UTF16_BE:
            {
                bom_size   = 2;
                out_BOM[0] = 0xFE;
                out_BOM[1] = 0xFF;
                out_BOM[2] = 0x00;
                out_BOM[3] = 0x00;
            }
            break;

        case TEXT_ENCODING_UTF16_LE:
            {
                bom_size   = 2;
                out_BOM[0] = 0xFF;
                out_BOM[1] = 0xFE;
                out_BOM[2] = 0x00;
                out_BOM[3] = 0x00;
            }
            break;

        case TEXT_ENCODING_UTF32_BE:
            {
                bom_size   = 4;
                out_BOM[0] = 0x00;
                out_BOM[1] = 0x00;
                out_BOM[2] = 0xFE;
                out_BOM[3] = 0xFF;
            }
            break;

        case TEXT_ENCODING_UTF32_LE:
            {
                bom_size   = 4;
                out_BOM[0] = 0xFF;
                out_BOM[1] = 0xFE;
                out_BOM[2] = 0x00;
                out_BOM[3] = 0x00;
            }
            break;

        default:
            {
                // no byte order marker.
                bom_size   = 0;
                out_BOM[0] = 0;
                out_BOM[1] = 0;
                out_BOM[2] = 0;
                out_BOM[3] = 0;
            }
            break;
    }
    return bom_size;
}

/// @summary Given four bytes possibly representing a Unicode byte-order-marker, attempts to determine the text encoding and actual size of the BOM.
/// @param BOM The array of four bytes potentially containing a BOM.
/// @param out_size If this value is non-NULL, on return it is updated with the actual size of the BOM, in bytes.
/// @return One of the text_encoding_e constants, specifying the text encoding indicated by the BOM.
public_function int32_t encoding(uint8_t BOM[4], size_t *out_size)
{
    size_t  bom_size = 0;
    int32_t text_enc = TEXT_ENCODING_UNSURE;

    if (0 == BOM[0])
    {
        if (0 == BOM[1] && 0xFE == BOM[2] && 0xFF == BOM[3])
        {
            // UTF32 big-endian.
            bom_size = 4;
            text_enc = TEXT_ENCODING_UTF32_BE;
        }
        else
        {
            // no BOM (or unrecognized).
            bom_size = 0;
            text_enc = TEXT_ENCODING_UNSURE;
        }
    }
    else if (0xFF == BOM[0])
    {
        if (0xFE == BOM[1])
        {
            if (0 == BOM[2] && 0 == BOM[3])
            {
                // UTF32 little-endian.
                bom_size = 4;
                text_enc = TEXT_ENCODING_UTF32_LE;
            }
            else
            {
                // UTF16 little-endian.
                bom_size = 2;
                text_enc = TEXT_ENCODING_UTF16_LE;
            }
        }
        else
        {
            // no BOM (or unrecognized).
            bom_size = 0;
            text_enc = TEXT_ENCODING_UNSURE;
        }
    }
    else if (0xFE == BOM[0] && 0xFF == BOM[1])
    {
        // UTF16 big-endian.
        bom_size = 2;
        text_enc = TEXT_ENCODING_UTF16_BE;
    }
    else if (0xEF == BOM[0] && 0xBB == BOM[1] && 0xBF == BOM[2])
    {
        // UTF-8.
        bom_size = 3;
        text_enc = TEXT_ENCODING_UTF8;
    }
    else
    {
        // no BOM (or unrecognized).
        bom_size = 0;
        text_enc = TEXT_ENCODING_UNSURE;
    }

    if (out_size != NULL)
    {
        // store the BOM size in bytes.
        *out_size = bom_size;
    }
    return text_enc;
}

/// @summary Computes the maximum number of bytes required to base64-encode a binary data block. All data is assumed to be output on one line. One byte is included for a trailing NULL.
/// @param binary_size The size of the binary data block, in bytes.
/// @param out_pad_size If non-NULL, on return this value is updated with the number of padding bytes that will be added during encoding.
/// @return The maximum number of bytes required to base64-encode a data block of the specified size.
public_function size_t base64_size(size_t binary_size, size_t *out_pad_size)
{
    // base64 transforms 3 input bytes into 4 output bytes.
    // pad the binary size so it is evenly divisible by 3.
    size_t rem = binary_size % 3;
    size_t adj = (rem != 0)  ? 3 - rem : 0;
    if (out_pad_size  != 0)  *out_pad_size = adj;
    return ((binary_size + adj) / 3) * 4 + 1; // +1 for NULL
}

/// @summary Computes the number of raw bytes required to store a block of binary data when converted back from base64. All source data is assumed to be on one line.
/// @param base64_size The size of the base64-encoded data.
/// @param pad_size The number of bytes of padding data. If not known, specify a value of zero.
/// @return The number of bytes of binary data generated during decoding.
public_function size_t binary_size(size_t base64_size, size_t pad_size)
{
    return (((3 * base64_size) / 4) - pad_size);
}

/// @summary Computes the number of raw bytes required to store a block of binary data when converted back from base64. All source data is assumed to be on one line. This version of the function examines the source data directly, and so can provide a precise value.
/// @param base64_source Pointer to the start of the base64-encoded data.
/// @param base64_length The number of ASCII characters in the base64 data string. This value can be computed using the standard C library strlen function if the length is not otherwise available.
/// @return The number of bytes of binary data that will be generated during decoding.
public_function size_t binary_size(char const *base64_source, size_t base64_length)
{
    if (NULL == base64_source || 0 == base64_length)
    {
        // zero-length input - zero-length output.
        return 0;
    }

    // end points at the last character in the base64 source data.
    char const *end = base64_source + base64_length - 1;
    size_t      pad = 0;
    if (base64_length >= 1 && '=' == *end--)  ++pad;
    if (base64_length >= 2 && '=' == *end--)  ++pad;
    return binary_size(base64_length, pad);
}

/// @summary Base64-encodes a block of arbitrary data. All data is returned on a single line; no newlines are inserted and no formatting is performed. The output buffer is guaranteed to be NULL-terminated.
/// @param dst Pointer to the start of the output buffer.
/// @param dst_size The maximum number of bytes that can be written to the output buffer. This value must be at least as large as the value returned by the base64_size() function.
/// @param src Pointer to the start of the input data.
/// @param src_size The number of bytes of input data to encode.
/// @return The number of bytes written to the output buffer.
public_function size_t base64_encode(char *dst, size_t dst_size, void const *src, size_t src_size)
{
    size_t         pad    =  0;
    size_t         req    =  base64_size(src_size, &pad);
    size_t         ins    =  src_size;
    uint8_t const *inp    = (uint8_t const*) src;
    char          *outp   =  dst;
    uint8_t        buf[4] = {0};

    if (dst_size < req)
    {
        // insufficient space in buffer.
        return 0;
    }

    // process input three bytes at a time.
    while (ins >= 3)
    {
        // buf[0] = left  6 bits of inp[0].
        // buf[1] = right 2 bits of inp[0], left 4 bits of inp[1].
        // buf[2] = right 4 bits of inp[1], left 2 bits of inp[2].
        // buf[3] = right 6 bits of inp[2].
        buf[0]  = (uint8_t)  ((inp[0] & 0xFC) >> 2);
        buf[1]  = (uint8_t) (((inp[0] & 0x03) << 4) + ((inp[1] & 0xF0) >> 4));
        buf[2]  = (uint8_t) (((inp[1] & 0x0F) << 2) + ((inp[2] & 0xC0) >> 6));
        buf[3]  = (uint8_t)   (inp[2] & 0x3F);
        // produce four bytes of output from three bytes of input.
        *outp++ = Base64_Chars[buf[0]];
        *outp++ = Base64_Chars[buf[1]];
        *outp++ = Base64_Chars[buf[2]];
        *outp++ = Base64_Chars[buf[3]];
        // we've consumed and processed three bytes of input.
        inp    += 3;
        ins    -= 3;
    }
    // pad any remaining input (either 1 or 2 bytes) up to three bytes; encode.
    if (ins > 0)
    {
        uint8_t src[3];
        size_t  i  = 0;

        // copy remaining real bytes from input; pad with nulls.
        for (i = 0; i  < ins; ++i) src[i] = *inp++;
        for (     ; i != 3;   ++i) src[i] = 0;
        // buf[0] = left  6 bits of inp[0].
        // buf[1] = right 2 bits of inp[0], left 4 bits of inp[1].
        // buf[2] = right 4 bits of inp[1], left 2 bits of inp[2].
        // buf[3] = right 6 bits of inp[2].
        buf[0]  = (uint8_t)  ((src[0] & 0xFC) >> 2);
        buf[1]  = (uint8_t) (((src[0] & 0x03) << 4) + ((src[1] & 0xF0) >> 4));
        buf[2]  = (uint8_t) (((src[1] & 0x0F) << 2) + ((src[2] & 0xC0) >> 6));
        buf[3]  = (uint8_t)   (src[2] & 0x3F);
        // produce four bytes of output from three bytes of input.
        *(outp+0) = Base64_Chars[buf[0]];
        *(outp+1) = Base64_Chars[buf[1]];
        *(outp+2) = Base64_Chars[buf[2]];
        *(outp+3) = Base64_Chars[buf[3]];
        // overwrite the junk characters with '=' characters.
        for (outp += 1 + ins; ins++ != 3;)  *outp++ = '=';
    }
    // always append the trailing null.
    *outp++ = '\0';
    // return the number of bytes written.
    return ((size_t)(outp - dst));
}

/// @summary Decodes a base64-encoded block of text into the corresponding raw binary representation.
/// @param dst Pointer to the start of the output buffer.
/// @param dst_size The maximum number of bytes that can be written to the output buffer. This value can be up to two bytes less than the value returned by binary_size() depending on the amount of padding added during encoding.
/// @param src Pointer to the start of the base64-encoded input data.
/// @param src_size The number of bytes of input data to read and decode.
/// @return The number of bytes written to the output buffer.
public_function size_t base64_decode(void *dst, size_t dst_size, char const *src, size_t src_size)
{
    char const *inp    =  src;
    char const *end    =  src + src_size;
    signed char idx[4] = {0};
    uint8_t    *outp   = (uint8_t*) dst;
    size_t      curr   =  0;
    size_t      pad    =  0;
    size_t      req    = binary_size(src_size, 0);

    if (dst_size < (req - 2))
    {
        // insufficient space in buffer.
        return 0;
    }

    while (inp != end)
    {
        char ch = *inp++;
        if (ch != '=')
        {
            signed char chi = Base64_Indices[(unsigned char)ch];
            if (chi != -1)
            {
                // valid character, buffer it.
                idx[curr++] = chi;
                pad         = 0;
            }
            else continue; // unknown character, skip it.
        }
        else
        {
            // this is a padding character.
            idx[curr++] = 0;
            ++pad;
        }

        if (4 == curr)
        {
            // we've read three bytes of data; generate output.
            curr     = 0;
            *outp++  = (uint8_t) ((idx[0] << 2) + ((idx[1] & 0x30) >> 4));
            if (pad != 2)
            {
                *outp++  = (uint8_t) (((idx[1] & 0xF) << 4) + ((idx[2] & 0x3C) >> 2));
                if (pad != 1)
                {
                    *outp++ = (uint8_t) (((idx[2] & 0x3) << 6) + idx[3]);
                }
            }
            if (pad != 0) break;
        }
    }
    // return the number of bytes written.
    return ((size_t)(outp - ((uint8_t*)dst)));
}

/// @summary Parse a string value representing a signed decimal integer into a binary value.
/// @param first Pointer to the first byte of the numeric sequence.
/// @param last Pointer to one past the last byte to parse.
/// @param out On return, this location is updated with the parsed value.
/// @return A pointer to one past the last digit.
template <typename int_type>
public_function char* parse_decimal_signed(char *first, char *last, int_type *out)
{
    int_type sign   = 1;
    int_type result = 0;

    if (first != last)
    {
        if ('-' == *first)
        {
            sign = -1;
            ++first;
        }
        else if ('+' == *first)
        {
            sign = +1;
            ++first;
        }
    }
    for (; first != last && is_digit(*first); ++first)
    {
        result = 10 * result + (*first - '0');
    }
    *out = result * sign;
    return first;
}

/// @summary Parse a string value representing an unsigned decimal integer into a binary value.
/// @param first Pointer to the first byte of the numeric sequence.
/// @param last Pointer to one past the last byte to parse.
/// @param out On return, this location is updated with the parsed value.
/// @return A pointer to one past the last digit.
template <typename int_type>
public_function char* parse_decimal_unsigned(char *first, char *last, int_type *out)
{
    int_type result = 0;
    if (first != last)
    {
        if ('+' == *first)
        {
            ++first;
        }
    }
    for (; first != last && is_digit(*first); ++first)
    {
        result = 10 * result + (*first - '0');
    }
    *out = result;
    return first;
}

/// @summary Parse a string value representing an unsigned hexadecimal integer into a binary value.
/// @param first Pointer to the first byte of the numeric sequence.
/// @param last Pointer to one past the last byte to parse.
/// @param out On return, this location is updated with the parsed value.
/// @return A pointer to one past the last digit.
template <typename int_type>
public_function char* parse_hexadecimal(char *first, char *last, int_type *out)
{
    int_type result = 0;
    for (; first != last; ++first)
    {
        unsigned int digit;
        if (is_digit(*first))
        {
            digit = *first - '0';
        }
        else if (*first >= 'a' && *first <= 'f')
        {
            digit = *first - 'a' + 10;
        }
        else if (*first >= 'A' && *first <= 'F')
        {
            digit = *first - 'A' + 10;
        }
        else break;
        result = 16 * result + digit;
    }
    *out = result;
    return first;
}

/// @summary Parse a string value representing a real number into a binary value.
/// @param first Pointer to the first byte of the numeric sequence.
/// @param last Pointer to one past the last byte to parse.
/// @param out On return, this location is updated with the parsed value.
/// @return A pointer to one past the last digit.
template <typename float_type>
public_function char* parse_float(char *first, char *last, float_type *out)
{
    double sign     = 1.0;
    double result   = 0.0;
    bool   exp_neg  = false;
    int    exponent = 0;

    if (first != last)
    {
        if ('-' == *first)
        {
            sign = -1.0;
            ++first;
        }
        else if ('+' == *first)
        {
            sign = +1.0;
            ++first;
        }
    }
    for (; first != last && is_digit(*first); ++first)
    {
        result = 10 * result + (*first - '0');
    }
    if (first != last && '.' == *first)
    {
        double inv_base = 0.1;
        ++first;
        for (; first != last && is_digit(*first); ++first)
        {
            result   += (*first - '0') * inv_base;
            inv_base *= 0.1;
        }
    }
    result *= sign;
    if (first != last && ('e' == *first || 'E' == *first))
    {
        ++first;
        if ('-' == *first)
        {
            exp_neg = true;
            ++first;
        }
        else if ('+' == *first)
        {
            exp_neg = false;
            ++first;
        }
        for (; first != last && is_digit(*first); ++first)
        {
            exponent = 10 * exponent + (*first - '0');
        }
    }
    if (exponent != 0)
    {
        double power_of_ten = 10;
        for (; exponent > 1; exponent--)
        {
            power_of_ten *= 10;
        }
        if (exp_neg) result /= power_of_ten;
        else         result *= power_of_ten;
    }
    *out = (float_type) result;
    return first;
}
