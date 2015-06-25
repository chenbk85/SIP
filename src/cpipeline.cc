/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines the interface to and identifiers of all available compute
/// pipelines, which perform transformations on input image data using OpenCL.
///////////////////////////////////////////////////////////////////////////80*/

/*////////////////
//   Includes   //
////////////////*/
#include <CL/cl.h>

/*////////////////////
//   Preprocessor   //
////////////////////*/

/*/////////////////
//   Constants   //
/////////////////*/
// TODO(rlk): Each compute pipeline has a known ID.
// Items in the presentation command list reference the pipeline using this ID.
// As part of the presentation command indicating that the pipeline should be executed, 
// any data is specified. The presentation layer receives the submitted command list 
// and reads all commands. Each pipeline command gets submitted to the appropriate 
// pipeline execution queue (SPSC(/LPLC?) maintained by each pipeline) and a pending 
// completion token is saved. The presentation layer then saves a reference to the 
// presentation command list along with all of the tokens in a list somewhere.
// During each present call, it polls the pipeline output queues and updates the 
// status of the corresponding completion tokens. When all completion tokens have 
// been signaled, the presentation command list is dequeued and executed.

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Represents all of the metadata associated with an OpenCL platform.
struct cl_platform_t
{
    cl_platform_id              Id;                   /// The OpenCL unique platform identifier.
    char                       *Name;                 /// The name of the platform.
    char                       *Vendor;               /// The name of the platform vendor.
    char                       *Version;              /// The OpenCL version, "1.2 ..."
    char                       *Profile;              /// Either "FULL_PROFILE" or "EMBEDDED_PROFILE".
    char                       *Extensions;           /// A space-separated list of extension names.
};

/// @summary Represents all of the metadata associated with an OpenCL device,
/// not including the device capabilities, which are stored and queried separately.
struct cl_device_t
{
    cl_device_id                Id;                   /// The OpenCL unique device identifier.
    cl_device_type              Type;                 /// CL_DEVICE_TYPE_CPU, CL_DEVICE_TYPE_GPU or CL_DEVICE_TYPE_ACCELERATOR.
    cl_platform_id              Platform;             /// The OpenCL unique identifier of the platform that owns the device.
    char                       *Name;                 /// The name of the device.
    char                       *Vendor;               /// The name of the platform vendor.
    char                       *Version;              /// The version of the device implementation, "1.2 ..."
    char                       *DriverVersion;        /// The version number of the device driver.
    char                       *Extensions;           /// A space-separated list of extension names.
};

/// @summary Stores information about the capabilities of a device.
struct cl_dev_caps_t
{
    cl_bool                     LittleEndian;         /// CL_DEVICE_ENDIAN_LITTLE
    cl_bool                     SupportECC;           /// CL_DEVICE_ERROR_CORRECTION_SUPPORT
    cl_uint                     AddressBits;          /// CL_DEVICE_ADDRESS_BITS
    cl_uint                     AddressAlign;         /// CL_DEVICE_MEM_BASE_ADDR_ALIGN
    cl_uint                     MinTypeAlign;         /// CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE
    size_t                      TimerResolution;      /// CL_DEVICE_PROFILING_TIMER_RESOLUTION
    size_t                      MaxWorkGroupSize;     /// CL_DEVICE_MAX_WORK_GROUP_SIZE
    cl_ulong                    MaxMallocSize;        /// CL_DEVICE_MAX_MEM_ALLOC_SIZE
    size_t                      MaxParamSize;         /// CL_DEVICE_MAX_PARAMETER_SIZE
    cl_uint                     MaxConstantArgs;      /// CL_DEVICE_MAX_CONSTANT_ARGS
    cl_ulong                    MaxCBufferSize;       /// CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE
    cl_ulong                    GlobalMemorySize;     /// CL_DEVICE_GLOBAL_MEM_SIZE
    cl_device_mem_cache_type    GlobalCacheType;      /// CL_DEVICE_GLOBAL_MEM_CACHE_TYPE
    cl_ulong                    GlobalCacheSize;      /// CL_DEVICE_GLOBAL_MEM_CACHE_SIZE
    cl_uint                     GlobalCacheLineSize;  /// CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE
    cl_device_local_mem_type    LocalMemoryType;      /// CL_DEVICE_LOCAL_MEM_TYPE 
    cl_ulong                    LocalMemorySize;      /// CL_DEVICE_LOCAL_MEM_SIZE
    cl_uint                     ClockFrequency;       /// CL_DEVICE_MAX_CLOCK_FREQUENCY
    cl_uint                     ComputeUnits;         /// CL_DEVICE_MAX_COMPUTE_UNITS
    cl_uint                     VecWidthChar;         /// CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR
    cl_uint                     VecWidthShort;        /// CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT
    cl_uint                     VecWidthInt;          /// CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT
    cl_uint                     VecWidthLong;         /// CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG
    cl_uint                     VecWidthSingle;       /// CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT
    cl_uint                     VecWidthDouble;       /// CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE
    cl_device_fp_config         FPSingleConfig;       /// CL_DEVICE_SINGLE_FP_CONFIG
    cl_device_fp_config         FPDoubleConfig;       /// CL_DEVICE_DOUBLE_FP_CONFIG
    cl_command_queue_properties CmdQueueConfig;       /// CL_DEVICE_QUEUE_PROPERTIES
    cl_bool                     SupportImage;         /// CL_DEVICE_IMAGE_SUPPORT
    size_t                      MaxWidth2D;           /// CL_DEVICE_IMAGE2D_MAX_WIDTH
    size_t                      MaxHeight2D;          /// CL_DEVICE_IMAGE2D_MAX_HEIGHT
    size_t                      MaxWidth3D;           /// CL_DEVICE_IMAGE3D_MAX_WIDTH
    size_t                      MaxHeight3D;          /// CL_DEVICE_IMAGE3D_MAX_HEIGHT
    size_t                      MaxDepth3D;           /// CL_DEVICE_IMAGE3D_MAX_DEPTH
    cl_uint                     MaxSamplers;          /// CL_DEVICE_MAX_SAMPLERS
    cl_uint                     MaxImageSources;      /// CL_DEVICE_MAX_READ_IMAGE_ARGS
    cl_uint                     MaxImageTargets;      /// CL_DEVICE_MAX_WRITE_IMAGE_ARGS
    cl_uint                     MaxWorkItemDimension; /// CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS
    size_t                     *MaxWorkItemSizes;     /// CL_DEVICE_MAX_WORK_ITEM_SIZES
};

// TODO(rlk): need to know what consititutes a request
// TODO(rlk): need to know what is required for a response

struct compute_pipeline_t
{
    // SPSC queue for requests
    // SPSC queue for responses
};

/*///////////////
//   Globals   //
///////////////*/
/// @summary A special identifier indicating 'all compute platforms'.
static cl_uint const MAX_COMPUTE_PLATFORM_COUNT_ID = (cl_uint) -1;

/*///////////////////////
//   Local Functions   //
///////////////////////*/
/// @summary Helper function to retrieve the length of an OpenCL platform string value, allocate a buffer and store a copy of the value.
/// @param id The OpenCL platform identifier being queried.
/// @param param The OpenCL parameter identifier to query.
/// @return A pointer to the NULL-terminated ASCII string. Free the returned buffer using free().
internal_function char* cl_platform_str(cl_platform_id const &id, cl_platform_info param)
{
    size_t nbytes = 0;
    char  *buffer = NULL;
    clGetPlatformInfo(id, param, 0, NULL, &nbytes);
    buffer = (char*) malloc(sizeof(char) * nbytes);
    clGetPlatformInfo(id, param, nbytes, buffer, NULL);
    buffer[nbytes-1] = '\0';
    return buffer;
}

/// @summary Helper function to retrieve the length of an OpenCL device string value, allocate a buffer and store a copy of the value.
/// @param id The OpenCL device identifier being queried.
/// @param param The OpenCL parameter identifier to query.
/// @return A pointer to the NULL-terminated ASCII string. Free the returned buffer using free().
internal_function char* cl_device_str(cl_device_id const &id, cl_device_info param)
{
    size_t nbytes = 0;
    char  *buffer = NULL;
    clGetDeviceInfo(id, param, 0, NULL, &nbytes);
    buffer = (char*) malloc(sizeof(char) * nbytes);
    clGetDeviceInfo(id, param, nbytes, buffer, NULL);
    buffer[nbytes-1] = '\0';
    return buffer;
}

/// @summary Queries the number of OpenCL platforms on the system.
/// @return The number of OpenCL platforms on the system.
internal_function cl_uint compute_platform_count(void)
{
    cl_uint n = 0;
    clGetPlatformIDs(MAX_COMPUTE_PLATFORM_COUNT_ID, NULL, &n);
    return n;
}

/// @summary Initializes a single cl_platform_t structure.
/// @param platform The platform definition to initialize.
internal_function void compute_platform_init(cl_platform_t *platform)
{
    platform->Id         =(cl_platform_id) 0;
    platform->Name       = NULL;
    platform->Vendor     = NULL;
    platform->Version    = NULL;
    platform->Profile    = NULL;
    platform->Extensions = NULL;
}


/// @summary Releases resources associated with a cl_platform_t instance.
/// @param platform The platform definition to free.
internal_function void compute_platform_free(cl_platform_t *platform)
{
    free(platform->Extensions); free(platform->Profile); free(platform->Version); free(platform->Vendor); free(platform->Name);
    platform->Id         =(cl_platform_id) 0;
    platform->Name       = NULL;
    platform->Vendor     = NULL;
    platform->Version    = NULL;
    platform->Profile    = NULL;
    platform->Extensions = NULL;
}

/// @summary Queries the driver for information about a specific platform.
/// @param platform The platform definition to populate.
/// @param id The unique identifier of the OpenCL platform.
internal_function void compute_platform_info(cl_platform_t *platform, cl_platform_id const &id)
{
    platform->Id         = id;
    platform->Name       = cl_platform_str(id, CL_PLATFORM_NAME);
    platform->Vendor     = cl_platform_str(id, CL_PLATFORM_VENDOR);
    platform->Version    = cl_platform_str(id, CL_PLATFORM_VERSION);
    platform->Profile    = cl_platform_str(id, CL_PLATFORM_PROFILE);
    platform->Extensions = cl_platform_str(id, CL_PLATFORM_EXTENSIONS);
}

/// @summary Determines whether a platform supports a given extension.
/// @param platform The platform to query.
/// @param extension The NULL-terminated extension name.
/// @return true if the platform supports the specified extension.
internal_function bool compute_platform_support(cl_platform_t *platform, char const *extension)
{
    return (strstr(platform->Extensions, extension) != NULL);
}

/// @summary Queries the number of devices of a given type on a platform.
/// @param platform The OpenCL unique identifier of the platform to query.
/// @param of_type A combination of cl_device_type indicating the device types to count.
/// @return The number of devices matching the specified criteria.
internal_function cl_uint compute_device_count(cl_platform_id const &platform, cl_device_type of_type)
{
    cl_uint ndevs = 0;
    clGetDeviceIDs(platform, of_type, 0, NULL, &ndevs);
    return ndevs;
}

/// @summary Initializes a single cl_device_t structure.
/// @param dev The device definition to initialize.
internal_function void compute_device_init(cl_device_t *dev)
{
    dev->Id            = (cl_device_id) 0;
    dev->Type          = CL_DEVICE_TYPE_DEFAULT;
    dev->Platform      = (cl_platform_id) 0;
    dev->Name          = NULL;
    dev->Vendor        = NULL;
    dev->Version       = NULL;
    dev->DriverVersion = NULL;
    dev->Extensions    = NULL;
}

/// @summary Releases resources associated with a cl_device_t instance.
/// @param dev The device definition to free.
internal_function void compute_device_free(cl_device_t *dev)
{
    free(dev->Extensions); free(dev->DriverVersion); free(dev->Version); free(dev->Vendor); free(dev->Name);
    dev->Id            =(cl_device_id) 0;
    dev->Type          = CL_DEVICE_TYPE_DEFAULT;
    dev->Platform      =(cl_platform_id) 0;
    dev->Name          = NULL;
    dev->Vendor        = NULL;
    dev->Version       = NULL;
    dev->DriverVersion = NULL;
    dev->Extensions    = NULL;
}

/// @summary Queries the driver for information about a specific device.
/// @param dev The device definition to populate.
/// @param platform The OpenCL unique identifier of the platform defining the device.
/// @param id The OpenCL unique identifer of the device.
internal_function void compute_device_info(cl_device_t *dev, cl_platform_id const &platform, cl_device_id const &id)
{
    dev->Id            = id;
    dev->Platform      = platform;
    dev->Name          = cl_device_str(id, CL_DEVICE_NAME);
    dev->Vendor        = cl_device_str(id, CL_DEVICE_VENDOR);
    dev->Version       = cl_device_str(id, CL_DEVICE_VERSION);
    dev->DriverVersion = cl_device_str(id, CL_DRIVER_VERSION);
    dev->Extensions    = cl_device_str(id, CL_DEVICE_EXTENSIONS);
    clGetDeviceInfo(id , CL_DEVICE_TYPE, sizeof(dev->Type), &dev->Type, NULL);
}

/// @summary Determines whether a device supports a given extension.
/// @param dev The device to query.
/// @param extension The NULL-terminated extension name.
/// @return true if the device supports the specified extension.
internal_function bool compute_device_support(cl_device_t *dev, char const *extension)
{
    return (strstr(dev->Extensions, extension) != NULL);
}

/// @summary Initializes a single cl_dev_caps_t structure.
/// @param caps The device capabilities to initialize.
internal_function void compute_dev_caps_init(cl_dev_caps_t *caps)
{
    memset(caps, 0, sizeof(cl_dev_caps_t));
}

/// @summary Releases resources associated with a cl_dev_caps_t instance.
/// @param caps The device capabilities to free.
internal_function void compute_dev_caps_free(cl_dev_caps_t *caps)
{
    free(caps->MaxWorkItemSizes);
    caps->MaxWorkItemDimension  = 0;
    caps->MaxWorkItemSizes      = NULL;
}

/// @summary Query the capabilities for a device.
/// @param caps The device capabilities structure to populate.
/// @param device The OpenCL unique identifier of the device.
internal_function void compute_dev_caps_info(cl_dev_caps_t *caps, cl_device_id const &device)
{
    cl_uint mid = 0;
    clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, sizeof(mid), &mid, NULL);
    caps->MaxWorkItemDimension = mid;
    caps->MaxWorkItemSizes = (size_t*) malloc(mid * sizeof(size_t));
    clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_ITEM_SIZES,     mid * sizeof(size_t),                      caps->MaxWorkItemSizes,     NULL);
    clGetDeviceInfo(device, CL_DEVICE_ENDIAN_LITTLE,                 sizeof(caps->LittleEndian),         &caps->LittleEndian,         NULL);
    clGetDeviceInfo(device, CL_DEVICE_ERROR_CORRECTION_SUPPORT,      sizeof(caps->SupportECC),           &caps->SupportECC,           NULL);
    clGetDeviceInfo(device, CL_DEVICE_ADDRESS_BITS,                  sizeof(caps->AddressBits),          &caps->AddressBits,          NULL);
    clGetDeviceInfo(device, CL_DEVICE_MEM_BASE_ADDR_ALIGN,           sizeof(caps->AddressAlign),         &caps->AddressAlign,         NULL);
    clGetDeviceInfo(device, CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE,      sizeof(caps->MinTypeAlign),         &caps->MinTypeAlign,         NULL);
    clGetDeviceInfo(device, CL_DEVICE_PROFILING_TIMER_RESOLUTION,    sizeof(caps->TimerResolution),      &caps->TimerResolution,      NULL);
    clGetDeviceInfo(device, CL_DEVICE_MAX_WORK_GROUP_SIZE,           sizeof(caps->MaxWorkGroupSize),     &caps->MaxWorkGroupSize,     NULL);
    clGetDeviceInfo(device, CL_DEVICE_MAX_MEM_ALLOC_SIZE,            sizeof(caps->MaxMallocSize),        &caps->MaxMallocSize,        NULL);
    clGetDeviceInfo(device, CL_DEVICE_MAX_PARAMETER_SIZE,            sizeof(caps->MaxParamSize),         &caps->MaxParamSize,         NULL);
    clGetDeviceInfo(device, CL_DEVICE_MAX_CONSTANT_ARGS,             sizeof(caps->MaxConstantArgs),      &caps->MaxConstantArgs,      NULL);
    clGetDeviceInfo(device, CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE,      sizeof(caps->MaxCBufferSize),       &caps->MaxCBufferSize,       NULL);
    clGetDeviceInfo(device, CL_DEVICE_GLOBAL_MEM_SIZE,               sizeof(caps->GlobalMemorySize),     &caps->GlobalMemorySize,     NULL);
    clGetDeviceInfo(device, CL_DEVICE_GLOBAL_MEM_CACHE_TYPE,         sizeof(caps->GlobalCacheType),      &caps->GlobalCacheType,      NULL);
    clGetDeviceInfo(device, CL_DEVICE_GLOBAL_MEM_CACHE_SIZE,         sizeof(caps->GlobalCacheSize),      &caps->GlobalCacheSize,      NULL);
    clGetDeviceInfo(device, CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE,     sizeof(caps->GlobalCacheLineSize),  &caps->GlobalCacheLineSize,  NULL);
    clGetDeviceInfo(device, CL_DEVICE_LOCAL_MEM_TYPE,                sizeof(caps->LocalMemoryType),      &caps->LocalMemoryType,      NULL);
    clGetDeviceInfo(device, CL_DEVICE_LOCAL_MEM_SIZE,                sizeof(caps->LocalMemorySize),      &caps->LocalMemorySize,      NULL);
    clGetDeviceInfo(device, CL_DEVICE_MAX_CLOCK_FREQUENCY,           sizeof(caps->ClockFrequency),       &caps->ClockFrequency,       NULL);
    clGetDeviceInfo(device, CL_DEVICE_MAX_COMPUTE_UNITS,             sizeof(caps->ComputeUnits),         &caps->ComputeUnits,         NULL);
    clGetDeviceInfo(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR,   sizeof(caps->VecWidthChar),         &caps->VecWidthChar,         NULL);
    clGetDeviceInfo(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT,  sizeof(caps->VecWidthShort),        &caps->VecWidthShort,        NULL);
    clGetDeviceInfo(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT,    sizeof(caps->VecWidthInt),          &caps->VecWidthInt,          NULL);
    clGetDeviceInfo(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG,   sizeof(caps->VecWidthLong),         &caps->VecWidthLong,         NULL);
    clGetDeviceInfo(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT,  sizeof(caps->VecWidthSingle),       &caps->VecWidthSingle,       NULL);
    clGetDeviceInfo(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE, sizeof(caps->VecWidthDouble),       &caps->VecWidthDouble,       NULL);
    clGetDeviceInfo(device, CL_DEVICE_SINGLE_FP_CONFIG,              sizeof(caps->FPSingleConfig),       &caps->FPSingleConfig,       NULL);
    clGetDeviceInfo(device, CL_DEVICE_DOUBLE_FP_CONFIG,              sizeof(caps->FPDoubleConfig),       &caps->FPDoubleConfig,       NULL);
    clGetDeviceInfo(device, CL_DEVICE_QUEUE_PROPERTIES,              sizeof(caps->CmdQueueConfig),       &caps->CmdQueueConfig,       NULL);
    clGetDeviceInfo(device, CL_DEVICE_IMAGE_SUPPORT,                 sizeof(caps->SupportImage),         &caps->SupportImage,         NULL);
    clGetDeviceInfo(device, CL_DEVICE_IMAGE2D_MAX_WIDTH,             sizeof(caps->MaxWidth2D),           &caps->MaxWidth2D,           NULL);
    clGetDeviceInfo(device, CL_DEVICE_IMAGE2D_MAX_HEIGHT,            sizeof(caps->MaxHeight2D),          &caps->MaxHeight2D,          NULL);
    clGetDeviceInfo(device, CL_DEVICE_IMAGE3D_MAX_WIDTH,             sizeof(caps->MaxWidth3D),           &caps->MaxWidth3D,           NULL);
    clGetDeviceInfo(device, CL_DEVICE_IMAGE3D_MAX_HEIGHT,            sizeof(caps->MaxHeight3D),          &caps->MaxHeight3D,          NULL);
    clGetDeviceInfo(device, CL_DEVICE_IMAGE3D_MAX_DEPTH,             sizeof(caps->MaxDepth3D),           &caps->MaxDepth3D,           NULL);
    clGetDeviceInfo(device, CL_DEVICE_MAX_SAMPLERS,                  sizeof(caps->MaxSamplers),          &caps->MaxSamplers,          NULL);
    clGetDeviceInfo(device, CL_DEVICE_MAX_READ_IMAGE_ARGS,           sizeof(caps->MaxImageSources),      &caps->MaxImageSources,      NULL);
    clGetDeviceInfo(device, CL_DEVICE_MAX_WRITE_IMAGE_ARGS,          sizeof(caps->MaxImageTargets),      &caps->MaxImageTargets,      NULL);
}

/*////////////////////////
//   Public Functions   //
////////////////////////*/
public_function bool register_compute_pipelines(void)
{
    // TODO(rlk): what data needs to be passed in to initialize OpenCL? we might need any GL or D3D context.
    return true;
}
