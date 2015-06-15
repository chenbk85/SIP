/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines the interface to an image parser, which is responsible for
/// extracting image definitions and pixel data from a file. Several callback 
/// function signatures are provided. The functions themselves are implemented
/// and provided by an image loader.
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
/// @summary Defines the interface to an image encoder, which takes source data 
/// in a particular compression and encoding type and converts it to another 
/// compression and encoding. The output data is written to an image memory blob.
struct image_encoder_t
{
    image_encoder_t(void);
    virtual ~image_encoder_t(void);

    virtual uint32_t   define_image
    (
        image_definition_t const &def
    ) = 0;                                   /// Reserve address space, if required.

    virtual uint32_t   reset_element
    (
        size_t         element
    ) = 0;                                   /// Start writing data to an image element.
                                             /// Existing contents are lost and overwritten.

    virtual uint32_t   encode
    (
        size_t         element, 
        void const    *src_data, 
        size_t         src_size
    ) = 0;                                   /// Encode and append data to an image element.

    virtual uint32_t   mark_level
    (
        size_t         element
    ) = 0;                                   /// Mark the end of the current level.

    virtual uint32_t   mark_element
    (
        size_t         element
    ) = 0;                                   /// Mark the end of the current element.

    image_memory_t    *Memory;               /// The image memory manager to which output data is written.
    uintptr_t          ImageId;              /// The application-defined image identifier. Constant.
    int                AccessType;           /// The image data access type. Constant.
    int                SourceCompression;    /// The image_compression_e of the source data. Constant.
    int                TargetCompression;    /// The image_compression_e of the output data. Constant.
    int                SourceEncoding;       /// The image_encoding_e of the source data. Constant.
    int                TargetEncoding;       /// The image_encoding_e of the target data. Constant.
    image_definition_t Metadata;             /// The generic image definiton. Constant; read-only.

private: // non-copyable
    image_encoder_t(image_encoder_t const&);
    image_encoder_t& operator =(image_encoder_t const&);
};

/// @summary Defines an image encoder type that performs no transformation to the source data.
struct image_encoder_identity_t final : public image_encoder_t
{
    image_encoder_identity_t(void);
    ~image_encoder_identity_t(void);

    uint32_t           define_image
    (
        image_definition_t const &def
    ) override;                              /// Reserve address space, if required.

    uint32_t           reset_element
    (
        size_t         element
    ) override;                              /// Start writing data to an image element.

    uint32_t           encode
    (
        size_t         element,
        void const    *src_data, 
        size_t         src_size
    ) override;                              /// Encode and append data to the current level.

    uint32_t           mark_level
    (
        size_t         element
    ) override;                              /// Mark the end of the current level.

    uint32_t           mark_element
    (
        size_t         element
    ) override;                              /// Mark the end of the current element.
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
/// @summary Allocate and initialize a new image encoder implementing the given transformation.
/// @param image_id The application-defined image identifier.
/// @param mem The image memory manager to which the output data will be written.
/// @param src_comp One of image_compression_e specifying the source data compression type.
/// @param src_enc One of image_encoding_e specifying the source data encoding type.
/// @param dst_comp One of image_compression_e specifying the target data compression type.
/// @param dst_enc One of image_encoding_e specifying the target data encoding type.
/// @param access_type One of image_access_type_e specifying how the data will be accessed.
/// @param def Image metadata specifying basic layout and attributes.
/// @return The image encoder, or NULL if no encoder type can perform the specified conversion.
public_function image_encoder_t* create_image_encoder(uintptr_t image_id, image_memory_t *mem, int src_comp, int src_enc, int dst_comp, int dst_enc, int access_type, image_definition_t const &def)
{
    if (src_comp == dst_comp && src_enc == dst_enc)
    {   // use the most common encoder type - the identity encoder.
        image_encoder_identity_t *enc = new image_encoder_identity_t();
        enc->Memory            = mem;
        enc->ImageId           = image_id;
        enc->AccessType        = access_type;
        enc->SourceCompression = src_comp;
        enc->SourceEncoding    = src_enc;
        enc->TargetCompression = dst_comp;
        enc->TargetEncoding    = dst_enc;
        if (SUCCEEDED(enc->define_image(def)))
        {   // the encoder is initialized and ready to go.
            return enc;
        }
        else
        {   // something wrong with the image definition.
            delete enc;
            return NULL;
        }
    }
    return NULL;
}

/// @summary Default constructor. Doesn't initialize any fields, because all fields should instead be initialized by the create_image_encoder() factory function.
image_encoder_t::image_encoder_t(void)
{
    /* empty */
}

/// @summary Default destructor. Does nothing because there are no resources to free.
image_encoder_t::~image_encoder_t(void)
{
    /* empty */
}

/// @summary Constructs a new identity encoder, which performs no transformation on the source data.
image_encoder_identity_t::image_encoder_identity_t(void)
{
    /* empty */
}

/// @summary Frees any resources associated with the identity encoder.
image_encoder_identity_t::~image_encoder_identity_t(void)
{
    /* empty */
}

/// @summary Defines the complete attributes of an image and reserves process address space for image storage.
/// @param def The image definition. The ElementCount field must be set to the total number of array elements or frames in the image.
/// @return ERROR_SUCCESS, or a system error code.
uint32_t image_encoder_identity_t::define_image(image_definition_t const &def)
{   // no additional transformation will be performed, so the size won't change as long as the image dimension attributes are the same. 
    size_t base_element_size  = image_memory_base_element_size(def);
    // TODO(rlk): determine scale factor for base_element_size using source compression/encoding.
    return image_memory_reserve_image(Memory, ImageId, base_element_size, def, TargetEncoding, AccessType);
}

/// @summary Decommits all memory associated with an image array element or frame. Subsequent encode calls targeting the element will begin writing data to level 0.
/// @param element The zero-based index of the image array element or frame to reset.
/// @return ERROR_SUCCESS, ERROR_NOT_FOUND or a system error code.
uint32_t image_encoder_identity_t::reset_element(size_t element)
{
    if (image_memory_reset_element_storage(Memory, ImageId, element) == NULL)
    {
        uint32_t err  = GetLastError();
        if (SUCCEEDED(err))
        {   // no OS error, so we couldn't find the image.
            return ERROR_NOT_FOUND;
        }
        else
        {   // return the OS error.
            return err;
        }
    }
    return ERROR_SUCCESS;
}

/// @summary Performs the encoding transformation to the source data and appends it to the current mipmap level of the specified image element.
/// @param element The zero-based index of the element to write.
uint32_t image_encoder_identity_t::encode(size_t element, void const *src_data, size_t src_size)
{   // TODO(rlk): if this were an asynchronous encoder, it must synchronously copy the src_data here.
    return image_memory_write(Memory, ImageId, element, src_data, src_size);
}

/// @summary Indicates that all data for the current mipmap level of an image element has been written. Subsequent encode calls targeting the element will begin writing data to the next mipmap level.
/// @param element The zero-based index of the image element being written.
/// @return ERROR_SUCCESS, ERROR_NOT_FOUND, or a system error code.
uint32_t image_encoder_identity_t::mark_level(size_t element)
{
    return image_memory_mark_level_end(Memory, ImageId, element);
}

/// @summary Indicates that all data for an image element has been encoded.
/// @param element The zero-based index of the image element being written.
/// @return ERROR_SUCCESS, ERROR_NOT_FOUND or a system error code.
uint32_t image_encoder_identity_t::mark_element(size_t element)
{   // do not increment the element index here. pick it up during the encode call.
    return image_memory_mark_element_end(Memory, ImageId, element);
}
