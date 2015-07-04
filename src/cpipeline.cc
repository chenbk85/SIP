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
/// @summary Define the supported CPU compute context configurations. For layouts 
/// that don't need to share data, a context-per-device model is most appropriate.
enum cpu_compute_context_e : int
{
    CPU_COMPUTE_CONTEXT_SHARED        = 0,            /// A single OpenCL context is shared between all logical devices.
    CPU_COMPUTE_CONTEXT_PER_DEVICE    = 1,            /// Each logical device maintains its own private OpenCL context.
};

/// @summary Stores information about the capabilities of a device.
struct cl_device_caps_t
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

/// @summary Defines the data used for compute device ranking, where the application is 
/// asking the question 'What is the most capable device on this platform?'.
struct cl_device_rank_t
{
    cl_platform_id              Platform;             /// The platform that defines the device.
    cl_uint                     ComputeUnits;         /// The number of compute units the device has available.
    cl_uint                     MaxSamplers;          /// The number of image samplers the device has available.
    cl_uint                     ClockFrequency;       /// The device clock frequency, higher is better.
};

/// @summary Represents a single platform and all of the physical devices it exposes.
/// The platform definition does not maintain any OpenCL state (contexts, queues, etc.)
/// All strings are NULL-terminated and contain only ASCII characters.
struct compute_platform_t
{
    cl_platform_id              PlatformId;           /// The OpenCL platform identifier.
    char                       *PlatformName;         /// A string specifying the platform name.
    char                       *PlatformVendor;       /// A string specifying the platform vendor name.
    char                       *PlatformVersion;      /// A string specifying the platform OpenCL version.
    char                       *PlatformProfile;      /// A string specifying the OpenCL profile of the platform.
    char                       *PlatformExtensions;   /// A space-separated list of platform extension names.
    size_t                      DeviceCount;          /// The number of devices exposed by the platform.
    cl_device_id               *DeviceIds;            /// The OpenCL device identifier for each device.
    cl_device_type             *DeviceTypes;          /// The OpenCL device type for each device.
    char                      **DeviceNames;          /// A string specifying the name of each device.
    char                      **DeviceVersions;       /// A string specifying the OpenCL version supported for each device.
    char                      **DeviceDrivers;        /// A string specifying the driver version for each device.
    char                      **DeviceExtensions;     /// A space-separated list of extensions exposed by each device.
    cl_device_caps_t           *DeviceCaps;           /// The capabilities of each device.
};

/// @summary Represents a CPU-only compute device. CPU compute devices may be configured 
/// to enable or disable certain compute units, configured for either task-parallel or 
/// data-parallel usage, partitioned by cache configuration, and so on. CPU devices 
/// only expose compute queues; there is no separate transfer queue.
struct compute_device_cpu_t
{
    cl_platform_id              PlatformId;           /// The OpenCL platform identifier.
    cl_device_id                MasterDeviceId;       /// The OpenCL device identifier exposed by the platform.
    size_t                      SubDeviceCount;       /// The number of logical device partitions.
    cl_device_id               *SubDeviceId;          /// The OpenCL device partition identifier for each partition.
    cl_context                 *ComputeContext;       /// The OpenCL context assigned to each device partition.
    cl_command_queue           *ComputeQueue;         /// The OpenCL compute queue for each device partition.
    cl_device_caps_t           *SubDeviceCaps;        /// The capabilities of each device partition.
};
// compute_device_configure_cpu_data_parallel(compute_device_cpu_t *dev, CPU_COMPUTE_CONTEXT_PER_DEVICE)
// compute_device_configure_cpu_task_parallel(compute_device_cpu_t *dev, CPU_COMPUTE_CONTEXT_SHARED);
// compute_device_configure_cpu_high_throughput(compute_device_cpu_t *dev); // default CPU_COMPUTE_CONTEXT_PER_DEVICE
// compute_device_configure_cpu_partition_count(compute_device_cpu_t *dev, int *units_per_device, size_t device_count, CPU_COMPUTE_CONTEXT_SHARED);
// compute_device_configure_cpu_partition_units(compute_device_cpu_t *dev, int**units_enabled, size_t *unit_counts, size_t device_count, CPU_COMPUTE_CONTEXT_PER_DEVICE);

/// @summary Represents the set of all CPU-only compute devices in the system.
struct compute_group_cpu_t
{
    size_t                      DeviceCount;          /// The number of CPU-only OpenCL compute devices.
    cl_device_id               *DeviceIds;            /// The OpenCL device identifier for each master device.
    cl_device_rank_t           *DeviceRank;           /// The data used for ranking devices by capability.
    compute_device_cpu_t       *Devices;              /// The set of configurable CPU-only OpenCL compute devices.
};

/// @summary Represents a GPU-only compute device. Data must be transferred over the 
/// PCIe bus. Each GPU-only device maintains separate queues for compute and transfer.
/// GPU-only devices do not support partitioning.
struct compute_device_gpu_t
{
    cl_platform_id              PlatformId;           /// The OpenCL platform identifier.
    cl_device_id                DeviceId;             /// The OpenCL device identifier exposed by the platform.
    cl_context                  ComputeContext;       /// The OpenCL context assigned to the device.
    cl_command_queue            ComputeQueue;         /// The OpenCL command queue for compute-only commands.
    cl_command_queue            TransferQueue;        /// The OpenCL command queue for data transfer commands.
    cl_device_caps_t const     *Capabilities;         /// The OpenCL device capabilities.
};

/// @summary Represents the set of all GPU-only compute devices in the system.
struct compute_group_gpu_t
{
    size_t                      DeviceCount;          /// The number of GPU-only OpenCL compute devices.
    cl_device_id               *DeviceIds;            /// The OpenCL device identifier for each master device.
    cl_device_rank_t           *DeviceRank;           /// The data used for ranking devices by capability.
    compute_device_gpu_t       *Devices;              /// The set of GPU-only OpenCL compute devices.
};

/// @summary Represents a CPU device with integrated/on-die GPU. The CPU and GPU share 
/// the same address space, and so data may be shared between them without copying.
struct compute_device_apu_t
{
    cl_platform_id              PlatformId;           /// The OpenCL platform identifier.
    cl_context                  SharedContext;        /// The OpenCL context used for resources shared between CPU and GPU.
    compute_device_cpu_t        CPU;                  /// The interface to the CPU portion of the compute device.
    compute_device_gpu_t        GPU;                  /// The interface to the GPU portion of the compute device.
};
// compute_device_configure_apu_data_parallel(compute_device_apu_t *dev) // CPU and GPU share everything, always
// compute_device_configure_apu_task_parallel(compute_device_apu_t *dev);
// compute_device_configure_apu_high_throughput(compute_device_apu_t *dev);
// compute_device_configure_apu_partition_count(compute_device_apu_t *dev, int *units_per_device, size_t device_count);
// compute_device_configure_apu_partition_units(compute_device_apu_t *dev, int**units_enabled, size_t *unit_counts, size_t device_count);

/// @summary Represents the set of all hybrid CPU/GPU compute devices in the system.
struct compute_group_apu_t
{
    size_t                      DeviceCount;          /// The number of OpenCL APU compute devices.
    cl_device_id               *CPUDeviceIds;         /// The OpenCL device identifier for each master CPU device.
    cl_device_id               *GPUDeviceIds;         /// The OpenCL device identifier for each master GPU device.
    cl_device_rank_t           *CPUDeviceRank;        /// The data used for ranking CPU devices by capability.
    cl_device_rank_t           *GPUDeviceRank;        /// The data used for ranking GPU devices by capability.
    compute_device_apu_t       *Devices;              /// The set of hybrid OpenCL compute devices.
};

/// @summary Represents a logical compute device. Instances of this structure can be 
/// created from a CPU-only device, a GPU-only device, or an APU hybrid device. This
/// structure is used for storing external device references and command submission.
struct compute_device_t
{
    cl_platform_id              PlatformId;           /// The OpenCL platform identifier of the device.
    cl_device_id                MasterDeviceId;       /// The OpenCL device ID of the master device. Used for device identification only.
    cl_device_id                DeviceId;             /// The OpenCL logical device identifier associated with the context and command queues.
    cl_device_type              DeviceType;           /// The type of compute device.
    cl_context                  ComputeContext;       /// The OpenCL device context used for resource sharing.
    cl_command_queue            ComputeQueue;         /// The OpenCL command queue for compute commands.
    cl_command_queue            TransferQueue;        /// The OpenCL command queue for data transfer commands.
    cl_device_caps_t const     *Capabilities;         /// The OpenCL logical device capabilities. This is a reference only.
};
// compute_device_from_cpu(compute_device_t *dev, compute_device_cpu_t *cpu_device);
// compute_device_from_gpu(compute_device_t *dev, compute_device_gpu_t *gpu_device);

/// @summary Defines the state associated with the OpenCL compute driver, which maintains 
/// metadata, OpenCL contexts and command queues for all compute devices in the system.
struct compute_driver_t
{
    size_t                      PlatformCount;        /// The number of OpenCL-capable platforms defined in the system.
    cl_platform_id             *PlatformIds;          /// The unique OpenCL platform identifier.
    compute_platform_t         *Platforms;            /// The set of platform metadata.

    size_t                      PipelineCount;        /// The number of compute pipelines defined.
    uintptr_t                   PipelineIds;          /// The unique ID of each registered compute pipeline.
    void                       *PipelineData;         /// Opaque data associated with each compute pipeline.

    compute_group_cpu_t         CPU;                  /// The set of all CPU-only OpenCL compute devices.
    compute_group_gpu_t         GPU;                  /// The set of all GPU-only OpenCL compute devices.
    compute_group_apu_t         APU;                  /// The set of all hybrid OpenCL compute devices.
};

// Enumerate platforms and devices, store data in global lists
// Each pipeline defines an input struct and an output struct
// For each request, you can specify a specific device ID or a device type, or let the pipeline impl decide
// Each pipeline maintains a list of pairings of (context+queue) it can submit jobs to

/*///////////////
//   Globals   //
///////////////*/
/// @summary A special identifier indicating 'all compute platforms'.
static cl_uint   const MAX_COMPUTE_PLATFORM_COUNT_ID = (cl_uint) -1;

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

/// @summary Helper function to determine whether an OpenCL platform or device extension is supported.
/// @param extension_name A NULL-terminated ASCII string specifying the OpenCL extension name to query.
/// @param extension_list A NULL-terminated ASCII string of space-delimited OpenCL extension names supported by the platform or device.
internal_function bool cl_extension_supported(char const *extension_name, char const *extension_list)
{
    return (strstr(extension_list, extension_name) != NULL);
}

/// @summary Initialize an OpenCL device ranking structure based on device capabilities.
/// @param rank The OpenCL device ranking attributes to update.
/// @param caps The OpenCL device capabilities.
/// @param platform The platform that defines the OpenCL device.
internal_function void cl_device_rank_init(cl_device_rank_t *rank, cl_device_caps_t const *caps, cl_platform_id platform)
{
    rank->Platform       = platform;
    rank->ComputeUnits   = caps->ComputeUnits;
    rank->MaxSamplers    = caps->MaxSamplers;
    rank->ClockFrequency = caps->ClockFrequency;
}

/// @summary Initializes an OpenCL device capabilities definition such that all values are zero or NULL.
/// @param caps The device capabilities definition to initialize.
internal_function void cl_device_caps_init(cl_device_caps_t *caps)
{
    memset(caps, 0, sizeof(cl_device_caps_t));
}

/// @summary Releases resources associated with an OpenCL device capabilities definition.
/// @param caps The device capabilities definition to delete.
internal_function void cl_device_caps_free(cl_device_caps_t *caps)
{
    free(caps->MaxWorkItemSizes);
    caps->MaxWorkItemDimension = 0;
    caps->MaxWorkItemSizes = NULL;
}

/// @summary Query the capabilities for an OpenCL compute device.
/// @param caps The device capabilities definition to populate.
/// @param device The OpenCL device identifier of the device to query.
internal_function void cl_device_capabilities(cl_device_caps_t *caps, cl_device_id device)
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

/// @summary Allocates and populates an OpenCL compute platform description.
/// @param desc The OpenCL compute platform description to populate.
/// @param platform The identifier of the OpenCL platform to query.
internal_function void compute_platform_describe(compute_platform_t *desc, cl_platform_id platform)
{   // initialize the platform definition to empty:
    memset(desc, 0, sizeof(compute_platform_t));

    // query platform attributes and populate the description:
    desc->PlatformId         = platform;
    desc->PlatformName       = cl_platform_str(platform, CL_PLATFORM_NAME);
    desc->PlatformVendor     = cl_platform_str(platform, CL_PLATFORM_VENDOR);
    desc->PlatformVersion    = cl_platform_str(platform, CL_PLATFORM_VERSION);
    desc->PlatformProfile    = cl_platform_str(platform, CL_PLATFORM_PROFILE);
    desc->PlatformExtensions = cl_platform_str(platform, CL_PLATFORM_EXTENSIONS);

    // determine the number of devices available on the platform and build the device lists.
    cl_uint        device_count = 0;
    cl_device_type type_filter  = CL_DEVICE_TYPE_CPU | CL_DEVICE_TYPE_GPU;
    clGetDeviceIDs(platform, type_filter, 0, NULL, &device_count);
    cl_device_id     *device_ids   = (cl_device_id    *) malloc(device_count * sizeof(cl_device_id));
    cl_device_type   *device_types = (cl_device_type  *) malloc(device_count * sizeof(cl_device_type));
    char            **device_names = (char           **) malloc(device_count * sizeof(char*));
    char            **device_ver   = (char           **) malloc(device_count * sizeof(char*));
    char            **driver_ver   = (char           **) malloc(device_count * sizeof(char*));
    char            **device_ext   = (char           **) malloc(device_count * sizeof(char*));
    cl_device_caps_t *device_caps  = (cl_device_caps_t*) malloc(device_count * sizeof(cl_device_caps_t));
    clGetDeviceIDs(platform, type_filter, device_count, device_ids, NULL);

    // query the properties of each device on the platform:
    for (cl_uint device_index = 0; device_index < device_count; ++device_index)
    {
        cl_device_type  device_type;
        clGetDeviceInfo(device_ids[device_index], CL_DEVICE_TYPE, sizeof(device_type), &device_type, NULL);
        device_types[device_index] = device_type;
        device_names[device_index] = cl_device_str(device_ids[device_index], CL_DEVICE_NAME);
        device_ver  [device_index] = cl_device_str(device_ids[device_index], CL_DEVICE_VERSION);
        driver_ver  [device_index] = cl_device_str(device_ids[device_index], CL_DRIVER_VERSION);
        device_ext  [device_index] = cl_device_str(device_ids[device_index], CL_DEVICE_EXTENSIONS);
        cl_device_capabilities(&device_caps[device_index], device_ids[device_index]);
    }

    // update the fields on the platform descriptor:
    desc->DeviceCount      = (size_t) device_count;
    desc->DeviceIds        = device_ids;
    desc->DeviceTypes      = device_types;
    desc->DeviceNames      = device_names;
    desc->DeviceVersions   = device_ver;
    desc->DeviceDrivers    = driver_ver;
    desc->DeviceExtensions = device_ext;
    desc->DeviceCaps       = device_caps;
}

/// @summary Releases all resources associated with an OpenCL compute platform description.
/// @param desc The OpenCL compute platform description to delete.
internal_function void compute_platform_delete(compute_platform_t *desc)
{
    for (size_t i = 0, n = desc->DeviceCount; i < n; ++i)
    {
        cl_device_caps_free(&desc->DeviceCaps[i]);
        free(desc->DeviceExtensions[i]);
        free(desc->DeviceDrivers[i]);
        free(desc->DeviceVersions[i]);
        free(desc->DeviceNames[i]);
    }
    free(desc->DeviceCaps);
    free(desc->DeviceExtensions);
    free(desc->DeviceDrivers);
    free(desc->DeviceVersions);
    free(desc->DeviceNames);
    free(desc->DeviceTypes);
    free(desc->DeviceIds);
    free(desc->PlatformExtensions);
    free(desc->PlatformProfile);
    free(desc->PlatformVersion);
    free(desc->PlatformVendor);
    free(desc->PlatformName);
    memset(desc, 0, sizeof(compute_platform_t));
}

/// @summary Check a device to determine whether it supports at least OpenCL 1.2.
/// @param platform The OpenCL platform that defines the device.
/// @param device_index The zero-based index of the OpenCL device to check.
/// @return true if the device supports OpenCL 1.2 or later.
internal_function bool compute_device_supported(compute_platform_t *platform, size_t device_index)
{
    int major_ver = 0;
    int minor_ver = 0;
    if (sscanf(platform->DeviceVersions[device_index], "OpenCL %d.%d", &major_ver , &minor_ver) != 2)
    {   // the version string doesn't appear to follow the format spec; ignore this device.
        return false;
    }
    if (major_ver == 1 && minor_ver < 2)
    {   // the device doesn't support OpenCL 1.2; ignore the device.
        return false;
    }
    return true;
}

/// @summary Queries the number of OpenCL platforms on the system.
/// @return The number of OpenCL platforms on the system.
internal_function cl_uint compute_platform_count(void)
{
    cl_uint n = 0;
    clGetPlatformIDs(cl_uint(-1), NULL, &n);
    return n;
}

/*////////////////////////
//   Public Functions   //
////////////////////////*/
/// @summary Open the compute driver by enumerating all OpenCL-capable devices in the system. 
/// @param driver The compute driver to initialize.
/// @return 0 if the driver is opened successfully, or -1 if an error has occurred.
public_function int compute_driver_open(compute_driver_t *driver)
{   
    size_t              cpu_device_count  = 0;
    size_t              gpu_device_count  = 0;
    size_t              apu_device_count  = 0;
    cl_uint             platform_count    = 0;
    cl_platform_id     *platform_ids      = NULL;
    compute_platform_t *platform_desc     = NULL;

    // zero out all fields of the driver structure:
    memset(driver, 0, sizeof(compute_driver_t));

    // query the number of OpenCL-capable platforms in the system.
    clGetPlatformIDs(cl_uint(-1), NULL, &platform_count);
    if (platform_count == 0)
    {
        OutputDebugStringA("ERROR: No OpenCL-capable platforms are installed.\n");
        return -1;
    }
    platform_ids  = (cl_platform_id    *) malloc(platform_count * sizeof(cl_platform_id));
    platform_desc = (compute_platform_t*) malloc(platform_count * sizeof(compute_platform_t));
    clGetPlatformIDs(platform_count, platform_ids, NULL);

    // describe each platform in the system. this doesn't create any execution resources, 
    // but it does build a list of devices and query all device properties and capabilities.
    for (cl_uint platform_index = 0; platform_index < platform_count; ++platform_index)
    {   // describe the platform, and then update total device-type counts.
        cl_platform_id      platform =  platform_ids [platform_index];
        compute_platform_t *desc     = &platform_desc[platform_index];
        compute_platform_describe(desc, platform);

        size_t  cpu_count = 0;
        size_t  gpu_count = 0;
        for (size_t device_index = 0, device_count = desc->DeviceCount; device_index < device_count; ++device_index)
        {
            int major_ver = 0;
            int minor_ver = 0;
            if (sscanf(desc->DeviceVersions[device_index], "OpenCL %d.%d", &major_ver , &minor_ver) != 2)
            {   // the version string doesn't appear to follow the format spec; ignore this device.
                OutputDebugString(_T("Unable to determine OpenCL device version from: "));
                OutputDebugStringA(desc->DeviceNames[device_index]);
                OutputDebugStringA(desc->DeviceVersions[device_index]);
                OutputDebugString(_T(".\n"));
                continue;
            }
            if (major_ver == 1 && minor_ver < 2)
            {   // the device doesn't support OpenCL 1.2; ignore the device.
                OutputDebugString(_T("Device doesn't support OpenCL 1.2+; skipping: "));
                OutputDebugStringA(desc->DeviceNames[device_index]);
                OutputDebugStringA(desc->DeviceVersions[device_index]);
                OutputDebugStringA(desc->DeviceVersions[device_index]);
                OutputDebugString(_T(".\n"));
                continue;
            }
            if (desc->DeviceTypes[device_index] == CL_DEVICE_TYPE_CPU) ++cpu_count;
            if (desc->DeviceTypes[device_index] == CL_DEVICE_TYPE_GPU) ++gpu_count;
        }
        if ((cpu_count == gpu_count) && (cpu_count > 0))
        {   // CPUs and GPUs are paired, so these are all APU devices.
            // TODO(rlk): is there a more robust way to detect this?
            // it looks like maybe the device IDs will be the same?
            apu_device_count += cpu_count;
        }
        else
        {   // CPUs and GPUs are not paired. treat them individually.
            cpu_device_count += cpu_count;
            gpu_device_count += gpu_count;
        }
    }

    // store all of the resulting data on the driver structure.
    driver->PlatformCount     = (size_t) platform_count;
    driver->PlatformIds       = platform_ids;
    driver->Platforms         = platform_desc;

    // reserve storage for the compute groups:
    driver->CPU.DeviceCount   =  0;
    driver->CPU.DeviceIds     = (cl_device_id        *) malloc(cpu_device_count * sizeof(cl_device_id));
    driver->CPU.DeviceRank    = (cl_device_rank_t    *) malloc(cpu_device_count * sizeof(cl_device_rank_t));
    driver->CPU.Devices       = (compute_device_cpu_t*) malloc(cpu_device_count * sizeof(compute_device_cpu_t));
    driver->GPU.DeviceCount   =  0;
    driver->GPU.DeviceIds     = (cl_device_id        *) malloc(gpu_device_count * sizeof(cl_device_id));
    driver->GPU.DeviceRank    = (cl_device_rank_t    *) malloc(gpu_device_count * sizeof(cl_device_rank_t));
    driver->GPU.Devices       = (compute_device_gpu_t*) malloc(gpu_device_count * sizeof(compute_device_gpu_t));
    driver->APU.DeviceCount   =  0;
    driver->APU.CPUDeviceIds  = (cl_device_id        *) malloc(apu_device_count * sizeof(cl_device_id));
    driver->APU.CPUDeviceRank = (cl_device_rank_t    *) malloc(apu_device_count * sizeof(cl_device_rank_t));
    driver->APU.GPUDeviceIds  = (cl_device_id        *) malloc(apu_device_count * sizeof(cl_device_id));
    driver->APU.GPUDeviceRank = (cl_device_rank_t    *) malloc(apu_device_count * sizeof(cl_device_rank_t));
    driver->APU.Devices       = (compute_device_apu_t*) malloc(apu_device_count * sizeof(compute_device_apu_t));

    // populate the compute groups:
    for (cl_uint platform_index = 0; platform_index < platform_count; ++platform_index)
    {   // describe the platform, and then update total device-type counts.
        cl_platform_id      platform =  platform_ids [platform_index];
        compute_platform_t *desc     = &platform_desc[platform_index];

        size_t  cpu_count = 0;
        size_t  gpu_count = 0;
        for (size_t device_index = 0, device_count = desc->DeviceCount; device_index < device_count; ++device_index)
        {
            if (compute_device_supported(desc, device_index))
            {   // only include devices meeting our OpenCL requirements.
                if (desc->DeviceTypes[device_index] == CL_DEVICE_TYPE_CPU) ++cpu_count;
                if (desc->DeviceTypes[device_index] == CL_DEVICE_TYPE_GPU) ++gpu_count;
            }
        }
        if ((cpu_count == gpu_count) && (cpu_count > 0))
        {   // CPUs and GPUs are paired, so these are all APU devices.
            for (size_t device_index = 0, device_count = desc->DeviceCount; device_index < device_count; ++device_index)
            {
                if (compute_device_supported(desc, device_index) == false)
                {   // skip unsupported devices.
                    continue;
                }
                if (desc->DeviceTypes[device_index] == CL_DEVICE_TYPE_CPU)
                {   // find the matching GPU device, which will have the same device ID.
                    for (size_t i = 0, n = desc->DeviceCount; i < n; ++i)
                    {
                        if (desc->DeviceTypes[i] == CL_DEVICE_TYPE_GPU && desc->DeviceIds[i] == desc->DeviceIds[device_index])
                        {   // we've located the corresponding GPU device.
                            // the device ranks, caps, contexts and queues are set on APU configuration.
                            size_t  apu_index = driver->APU.DeviceCount++;
                            driver->APU.CPUDeviceIds[apu_index] = desc->DeviceIds[device_index];
                            driver->APU.GPUDeviceIds[apu_index] = desc->DeviceIds[i];
                            driver->APU.Devices     [apu_index].PlatformId         = platform;
                            driver->APU.Devices     [apu_index].SharedContext      = NULL;
                            driver->APU.Devices     [apu_index].CPU.PlatformId     = platform;
                            driver->APU.Devices     [apu_index].CPU.MasterDeviceId = desc->DeviceIds[device_index];
                            driver->APU.Devices     [apu_index].CPU.SubDeviceCount = 0;
                            driver->APU.Devices     [apu_index].CPU.SubDeviceId    = NULL;
                            driver->APU.Devices     [apu_index].CPU.SubDeviceCaps  = NULL;
                            driver->APU.Devices     [apu_index].CPU.ComputeContext = NULL;
                            driver->APU.Devices     [apu_index].CPU.ComputeQueue   = NULL;
                            driver->APU.Devices     [apu_index].GPU.PlatformId     = platform;
                            driver->APU.Devices     [apu_index].GPU.DeviceId       = desc->DeviceIds[i];
                            driver->APU.Devices     [apu_index].GPU.ComputeContext = NULL;
                            driver->APU.Devices     [apu_index].GPU.ComputeQueue   = NULL;
                            driver->APU.Devices     [apu_index].GPU.TransferQueue  = NULL;
                            driver->APU.Devices     [apu_index].GPU.Capabilities   =&desc->DeviceCaps[i];
                            break;
                        }
                    }
                }
            }
        }
        else
        {   // CPUs and GPUs are not paired. treat them individually.
            for (size_t device_index = 0, device_count = desc->DeviceCount; device_index < device_count; ++device_index)
            {
                if (compute_device_supported(desc, device_index) == false)
                {   // skip unsupported devices.
                    continue;
                }
                if (desc->DeviceTypes[device_index] == CL_DEVICE_TYPE_CPU)
                {   // the device ranks, caps, contexts and queues are set on CPU configuration.
                    size_t  cpu_index = driver->CPU.DeviceCount++;
                    driver->CPU.DeviceIds[cpu_index] = desc->DeviceIds[device_index];
                    driver->CPU.Devices  [cpu_index].PlatformId     = platform;
                    driver->CPU.Devices  [cpu_index].MasterDeviceId = desc->DeviceIds[device_index];
                    driver->CPU.Devices  [cpu_index].ComputeQueue   = NULL;
                    driver->CPU.Devices  [cpu_index].ComputeContext = NULL;
                    driver->CPU.Devices  [cpu_index].SubDeviceCount = 0;
                    driver->CPU.Devices  [cpu_index].SubDeviceId    = NULL;
                    driver->CPU.Devices  [cpu_index].SubDeviceCaps  = NULL;
                }
                if (desc->DeviceTypes[device_index] == CL_DEVICE_TYPE_GPU)
                {   // GPU devices can't be sub-divided, so set caps, etc.
                    size_t  gpu_index = driver->GPU.DeviceCount++;
                    driver->GPU.DeviceIds[gpu_index] = desc->DeviceIds[device_index];
                    driver->GPU.Devices  [gpu_index].PlatformId     = platform;
                    driver->GPU.Devices  [gpu_index].DeviceId       = desc->DeviceIds[device_index];
                    driver->GPU.Devices  [gpu_index].ComputeContext = NULL;
                    driver->GPU.Devices  [gpu_index].ComputeQueue   = NULL;
                    driver->GPU.Devices  [gpu_index].TransferQueue  = NULL;
                    driver->GPU.Devices  [gpu_index].Capabilities   =&desc->DeviceCaps[device_index];
                }
            }
        }
    }
    // TODO(rlk): configure devices.
    // TODO(rlk): initialize pipelines.
    return 0;
}

public_function void compute_driver_close(compute_driver_t *driver)
{
}

// TODO(rlk): need a function to query the number of physical and logical CPUs/NUMA nodes.
