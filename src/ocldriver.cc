/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines the interface to and identifiers of all available compute
/// pipelines, which perform transformations on input image data using OpenCL.
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
enum cl_cpu_compute_context_e         : int
{
    CL_CPU_COMPUTE_CONTEXT_SHARED     = 0,             /// A single OpenCL context is shared between all logical devices.
    CL_CPU_COMPUTE_CONTEXT_PER_DEVICE = 1,             /// Each logical device maintains its own private OpenCL context.
};

/// @summary Defines attributes that can be set on a device.
enum cl_compute_device_flags_e        : uint32_t
{
    CL_COMPUTE_DEVICE_FLAGS_NONE      = (0 << 0),      /// No special attributes are set on the device.
    CL_COMPUTE_DEVICE_FLAGS_DISPLAY   = (1 << 0),      /// The compute device is used for display output. GPU-only.
    CL_COMPUTE_DEVICE_FLAGS_SHARE_GL  = (1 << 1),      /// The compute device has OpenGL resource sharing enabled. GPU-only.
    CL_COMPUTE_DEVICE_FLAGS_SHARE_D3D = (1 << 2),      /// The compute device has Direct3D resource sharing enabled. GPU-only.
};

/// @summary Stores information about the capabilities of a device.
struct cl_device_caps_t
{
    cl_bool                      LittleEndian;         /// CL_DEVICE_ENDIAN_LITTLE
    cl_bool                      SupportECC;           /// CL_DEVICE_ERROR_CORRECTION_SUPPORT
    cl_bool                      UnifiedMemory;        /// CL_DEVICE_HOST_UNIFIED_MEMORY
    cl_bool                      CompilerAvailable;    /// CL_DEVICE_COMPILER_AVAILABLE
    cl_bool                      LinkerAvailable;      /// CL_DEVICE_LINKER_AVAILABLE
    cl_bool                      PreferUserSync;       /// CL_DEVICE_PREFERRED_INTEROP_USER_SYNC
    cl_uint                      AddressBits;          /// CL_DEVICE_ADDRESS_BITS
    cl_uint                      AddressAlign;         /// CL_DEVICE_MEM_BASE_ADDR_ALIGN
    cl_uint                      MinTypeAlign;         /// CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE
    size_t                       MaxPrintfBuffer;      /// CL_DEVICE_PRINTF_BUFFER_SIZE
    size_t                       TimerResolution;      /// CL_DEVICE_PROFILING_TIMER_RESOLUTION
    size_t                       MaxWorkGroupSize;     /// CL_DEVICE_MAX_WORK_GROUP_SIZE
    cl_ulong                     MaxMallocSize;        /// CL_DEVICE_MAX_MEM_ALLOC_SIZE
    size_t                       MaxParamSize;         /// CL_DEVICE_MAX_PARAMETER_SIZE
    cl_uint                      MaxConstantArgs;      /// CL_DEVICE_MAX_CONSTANT_ARGS
    cl_ulong                     MaxCBufferSize;       /// CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE
    cl_ulong                     GlobalMemorySize;     /// CL_DEVICE_GLOBAL_MEM_SIZE
    cl_device_mem_cache_type     GlobalCacheType;      /// CL_DEVICE_GLOBAL_MEM_CACHE_TYPE
    cl_ulong                     GlobalCacheSize;      /// CL_DEVICE_GLOBAL_MEM_CACHE_SIZE
    cl_uint                      GlobalCacheLineSize;  /// CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE
    cl_device_local_mem_type     LocalMemoryType;      /// CL_DEVICE_LOCAL_MEM_TYPE 
    cl_ulong                     LocalMemorySize;      /// CL_DEVICE_LOCAL_MEM_SIZE
    cl_uint                      ClockFrequency;       /// CL_DEVICE_MAX_CLOCK_FREQUENCY
    cl_uint                      ComputeUnits;         /// CL_DEVICE_MAX_COMPUTE_UNITS
    cl_uint                      VecWidthChar;         /// CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR
    cl_uint                      VecWidthShort;        /// CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT
    cl_uint                      VecWidthInt;          /// CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT
    cl_uint                      VecWidthLong;         /// CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG
    cl_uint                      VecWidthSingle;       /// CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT
    cl_uint                      VecWidthDouble;       /// CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE
    cl_device_fp_config          FPSingleConfig;       /// CL_DEVICE_SINGLE_FP_CONFIG
    cl_device_fp_config          FPDoubleConfig;       /// CL_DEVICE_DOUBLE_FP_CONFIG
    cl_command_queue_properties  CmdQueueConfig;       /// CL_DEVICE_QUEUE_PROPERTIES
    cl_device_exec_capabilities  ExecutionCapability;  /// CL_DEVICE_EXECUTION_CAPABILITIES
    cl_uint                      MaxSubDevices;        /// CL_DEVICE_PARTITION_MAX_SUB_DEVICES
    size_t                       NumPartitionTypes;    /// Computed; number of valid items in PartitionTypes.
    cl_device_partition_property PartitionTypes[4];    /// CL_DEVICE_PARTITION_PROPERTIES
    size_t                       NumAffinityDomains;   /// Computed; number of valid items in AffinityDomains.
    cl_device_affinity_domain    AffinityDomains[7];   /// CL_DEVICE_PARTITION_AFFINITY_DOMAIN
    cl_bool                      SupportImage;         /// CL_DEVICE_IMAGE_SUPPORT
    size_t                       MaxWidth2D;           /// CL_DEVICE_IMAGE2D_MAX_WIDTH
    size_t                       MaxHeight2D;          /// CL_DEVICE_IMAGE2D_MAX_HEIGHT
    size_t                       MaxWidth3D;           /// CL_DEVICE_IMAGE3D_MAX_WIDTH
    size_t                       MaxHeight3D;          /// CL_DEVICE_IMAGE3D_MAX_HEIGHT
    size_t                       MaxDepth3D;           /// CL_DEVICE_IMAGE3D_MAX_DEPTH
    size_t                       MaxImageArraySize;    /// CL_DEVICE_IMAGE_MAX_ARRAY_SIZE
    size_t                       MaxImageBufferSize;   /// CL_DEVICE_IMAGE_MAX_BUFFER_SIZE
    cl_uint                      MaxSamplers;          /// CL_DEVICE_MAX_SAMPLERS
    cl_uint                      MaxImageSources;      /// CL_DEVICE_MAX_READ_IMAGE_ARGS
    cl_uint                      MaxImageTargets;      /// CL_DEVICE_MAX_WRITE_IMAGE_ARGS
    cl_uint                      MaxWorkItemDimension; /// CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS
    size_t                      *MaxWorkItemSizes;     /// CL_DEVICE_MAX_WORK_ITEM_SIZES
};

/// @summary Defines the data used for compute device ranking, where the application is 
/// asking the question 'What is the most capable device on this platform?'.
struct cl_device_rank_t
{
    cl_platform_id               Platform;             /// The platform that defines the device.
    cl_uint                      ComputeUnits;         /// The number of compute units the device has available.
    cl_uint                      MaxSamplers;          /// The number of image samplers the device has available.
    cl_uint                      ClockFrequency;       /// The device clock frequency, higher is better.
};

/// @summary Represents a single platform and all of the physical devices it exposes.
/// The platform definition does not maintain any OpenCL state (contexts, queues, etc.)
/// All devices within a platform have the ability to share an OpenCL context.
/// All strings are NULL-terminated and contain only ASCII characters.
struct cl_platform_t
{
    cl_platform_id               PlatformId;           /// The OpenCL platform identifier.
    char                        *PlatformName;         /// A string specifying the platform name.
    char                        *PlatformVendor;       /// A string specifying the platform vendor name.
    char                        *PlatformVersion;      /// A string specifying the platform OpenCL version.
    char                        *PlatformProfile;      /// A string specifying the OpenCL profile of the platform.
    char                        *PlatformExtensions;   /// A space-separated list of platform extension names.
    size_t                       DeviceCount;          /// The number of devices exposed by the platform.
    cl_device_id                *DeviceIds;            /// The OpenCL device identifier for each device.
    cl_device_type              *DeviceTypes;          /// The OpenCL device type for each device.
    char                       **DeviceNames;          /// A string specifying the name of each device.
    char                       **DeviceVersions;       /// A string specifying the OpenCL version supported for each device.
    char                       **DeviceDrivers;        /// A string specifying the driver version for each device.
    char                       **DeviceExtensions;     /// A space-separated list of extensions exposed by each device.
    cl_device_caps_t            *DeviceCaps;           /// The capabilities of each device.
};

/// @summary Represents a CPU-only compute device. CPU compute devices may be configured 
/// to enable or disable certain compute units, configured for either task-parallel or 
/// data-parallel usage, partitioned by cache configuration, and so on. CPU devices 
/// only expose compute queues; there is no separate transfer queue.
struct cl_device_cpu_t
{
    cl_platform_id               PlatformId;           /// The OpenCL platform identifier.
    cl_device_id                 MasterDeviceId;       /// The OpenCL device identifier exposed by the platform.
    size_t                       SubDeviceCount;       /// The number of logical device partitions.
    cl_device_id                *SubDeviceId;          /// The OpenCL device partition identifier for each partition.
    cl_context                  *ComputeContext;       /// The OpenCL context assigned to each device partition.
    cl_command_queue            *ComputeQueue;         /// The OpenCL compute queue for each device partition.
    cl_device_caps_t            *SubDeviceCaps;        /// The capabilities of each device partition.
};
// cl_device_configure_cpu_data_parallel(compute_device_cpu_t *dev, CPU_COMPUTE_CONTEXT_PER_DEVICE)
// cl_device_configure_cpu_task_parallel(compute_device_cpu_t *dev, CPU_COMPUTE_CONTEXT_SHARED);
// cl_device_configure_cpu_high_throughput(compute_device_cpu_t *dev); // default CPU_COMPUTE_CONTEXT_PER_DEVICE
// cl_device_configure_cpu_partition_count(compute_device_cpu_t *dev, int *units_per_device, size_t device_count, CPU_COMPUTE_CONTEXT_SHARED);
// cl_device_configure_cpu_partition_units(compute_device_cpu_t *dev, int**units_enabled, size_t *unit_counts, size_t device_count, CPU_COMPUTE_CONTEXT_PER_DEVICE);

/// @summary Represents the set of all CPU-only compute devices in the system.
struct cl_device_group_cpu_t
{
    size_t                       DeviceCount;          /// The number of CPU-only OpenCL compute devices.
    cl_device_id                *DeviceIds;            /// The OpenCL device identifier for each master device.
    cl_device_rank_t            *DeviceRank;           /// The data used for ranking devices by capability.
    cl_device_cpu_t             *Devices;              /// The set of configurable CPU-only OpenCL compute devices.
};

/// @summary Represents a GPU-only compute device. Data must be transferred over the 
/// PCIe bus. Each GPU-only device maintains separate queues for compute and transfer.
/// GPU-only devices do not support partitioning.
struct cl_device_gpu_t
{
    cl_platform_id               PlatformId;           /// The OpenCL platform identifier.
    cl_device_id                 DeviceId;             /// The OpenCL device identifier exposed by the platform.
    cl_context                   ComputeContext;       /// The OpenCL context assigned to the device.
    cl_command_queue             ComputeQueue;         /// The OpenCL command queue for compute-only commands.
    cl_command_queue             TransferQueue;        /// The OpenCL command queue for data transfer commands.
    cl_device_caps_t const      *Capabilities;         /// The OpenCL device capabilities.
    uint32_t                     DeviceFlags;          /// A combination of cl_compute_device_flags_e values.
    uint32_t                     DisplayOrdinal;       /// The ordinal value of the attached display, or DISPLAY_ORDINAL_NONE.
};

/// @summary Represents the set of all GPU-only compute devices in the system.
struct cl_device_group_gpu_t
{
    size_t                       DeviceCount;          /// The number of GPU-only OpenCL compute devices.
    cl_device_id                *DeviceIds;            /// The OpenCL device identifier for each master device.
    cl_device_rank_t            *DeviceRank;           /// The data used for ranking devices by capability.
    cl_device_gpu_t             *Devices;              /// The set of GPU-only OpenCL compute devices.
};

/// @summary Represents a logical compute device. Instances of this structure can be 
/// created from a CPU-only device, a GPU-only device, or an APU hybrid device. This
/// structure is used for storing external device references and command submission.
struct cl_device_t
{
    cl_platform_id               PlatformId;           /// The OpenCL platform identifier of the device.
    cl_device_id                 MasterDeviceId;       /// The OpenCL device ID of the master device. Used for device identification only.
    cl_device_id                 DeviceId;             /// The OpenCL logical device identifier associated with the context and command queues.
    cl_device_type               DeviceType;           /// The type of compute device.
    cl_context                   ComputeContext;       /// The OpenCL device context used for resource sharing.
    cl_command_queue             ComputeQueue;         /// The OpenCL command queue for compute commands.
    cl_command_queue             TransferQueue;        /// The OpenCL command queue for data transfer commands.
    cl_device_caps_t const      *Capabilities;         /// The OpenCL logical device capabilities. This is a reference only.
    uint32_t                     DeviceFlags;          /// A combination of cl_compute_device_flags_e values.
    uint32_t                     DisplayOrdinal;       /// The ordinal value of the attached display, or DISPLAY_ORDINAL_NONE.
};
// cl_device_from_cpu(cl_device_t *dev, cl_device_cpu_t *cpu_device);
// cl_device_from_gpu(cl_device_t *dev, cl_device_gpu_t *gpu_device);

/// @summary Defines the state associated with the OpenCL compute driver, which maintains 
/// metadata, OpenCL contexts and command queues for all compute devices in the system.
struct cl_driver_t
{
    size_t                       PlatformCount;        /// The number of OpenCL-capable platforms defined in the system.
    cl_platform_id              *PlatformIds;          /// The unique OpenCL platform identifier.
    cl_platform_t               *Platforms;            /// The set of platform metadata.

    size_t                       PipelineCount;        /// The number of compute pipelines defined.
    uintptr_t                   *PipelineIds;          /// The unique ID of each registered compute pipeline.
    void                        *PipelineData;         /// Opaque data associated with each compute pipeline.

    cl_device_group_cpu_t        CPU;                  /// The set of all CPU-only OpenCL compute devices.
    cl_device_group_gpu_t        GPU;                  /// The set of all GPU-only OpenCL compute devices.
};

/// @summary Defines a simple structure for storing the number of CPU execution resources in the system.
struct cpu_counts_t
{
    size_t                       NUMANodes;        /// The number of NUMA nodes in the system.
    size_t                       PhysicalCPUs;     /// The number of physical processor packages in the system.
    size_t                       PhysicalCores;    /// The number of physical processor cores in the system.
    size_t                       HardwareThreads;  /// The number of logical CPUs/hardware threads in the system.
};

// Enumerate platforms and devices, store data in global lists
// Each pipeline defines an input struct and an output struct
// For each request, you can specify a specific device ID or a device type, or let the pipeline impl decide
// Each pipeline maintains a list of pairings of (context+queue) it can submit jobs to

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
    clGetDeviceInfo(device, CL_DEVICE_HOST_UNIFIED_MEMORY,           sizeof(caps->UnifiedMemory),        &caps->UnifiedMemory,        NULL);
    clGetDeviceInfo(device, CL_DEVICE_COMPILER_AVAILABLE,            sizeof(caps->CompilerAvailable),    &caps->CompilerAvailable,    NULL);
    clGetDeviceInfo(device, CL_DEVICE_LINKER_AVAILABLE,              sizeof(caps->LinkerAvailable),      &caps->LinkerAvailable,      NULL);
    clGetDeviceInfo(device, CL_DEVICE_PREFERRED_INTEROP_USER_SYNC,   sizeof(caps->PreferUserSync),       &caps->PreferUserSync,       NULL);
    clGetDeviceInfo(device, CL_DEVICE_ADDRESS_BITS,                  sizeof(caps->AddressBits),          &caps->AddressBits,          NULL);
    clGetDeviceInfo(device, CL_DEVICE_MEM_BASE_ADDR_ALIGN,           sizeof(caps->AddressAlign),         &caps->AddressAlign,         NULL);
    clGetDeviceInfo(device, CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE,      sizeof(caps->MinTypeAlign),         &caps->MinTypeAlign,         NULL);
    clGetDeviceInfo(device, CL_DEVICE_PRINTF_BUFFER_SIZE,            sizeof(caps->MaxPrintfBuffer),      &caps->MaxPrintfBuffer,      NULL);
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
    clGetDeviceInfo(device, CL_DEVICE_EXECUTION_CAPABILITIES,        sizeof(caps->ExecutionCapability),  &caps->ExecutionCapability,  NULL);
    clGetDeviceInfo(device, CL_DEVICE_PARTITION_MAX_SUB_DEVICES,     sizeof(caps->MaxSubDevices),        &caps->MaxSubDevices,        NULL);
    clGetDeviceInfo(device, CL_DEVICE_PARTITION_PROPERTIES,      4 * sizeof(caps->PartitionTypes),        caps->PartitionTypes,       NULL);
    clGetDeviceInfo(device, CL_DEVICE_PARTITION_AFFINITY_DOMAIN, 7 * sizeof(caps->AffinityDomains),       caps->AffinityDomains,      NULL);
    clGetDeviceInfo(device, CL_DEVICE_IMAGE_SUPPORT,                 sizeof(caps->SupportImage),         &caps->SupportImage,         NULL);
    clGetDeviceInfo(device, CL_DEVICE_IMAGE2D_MAX_WIDTH,             sizeof(caps->MaxWidth2D),           &caps->MaxWidth2D,           NULL);
    clGetDeviceInfo(device, CL_DEVICE_IMAGE2D_MAX_HEIGHT,            sizeof(caps->MaxHeight2D),          &caps->MaxHeight2D,          NULL);
    clGetDeviceInfo(device, CL_DEVICE_IMAGE3D_MAX_WIDTH,             sizeof(caps->MaxWidth3D),           &caps->MaxWidth3D,           NULL);
    clGetDeviceInfo(device, CL_DEVICE_IMAGE3D_MAX_HEIGHT,            sizeof(caps->MaxHeight3D),          &caps->MaxHeight3D,          NULL);
    clGetDeviceInfo(device, CL_DEVICE_IMAGE3D_MAX_DEPTH,             sizeof(caps->MaxDepth3D),           &caps->MaxDepth3D,           NULL);
    clGetDeviceInfo(device, CL_DEVICE_IMAGE_MAX_ARRAY_SIZE,          sizeof(caps->MaxImageArraySize),    &caps->MaxImageArraySize,    NULL);
    clGetDeviceInfo(device, CL_DEVICE_IMAGE_MAX_BUFFER_SIZE,         sizeof(caps->MaxImageBufferSize),   &caps->MaxImageBufferSize,   NULL);
    clGetDeviceInfo(device, CL_DEVICE_MAX_SAMPLERS,                  sizeof(caps->MaxSamplers),          &caps->MaxSamplers,          NULL);
    clGetDeviceInfo(device, CL_DEVICE_MAX_READ_IMAGE_ARGS,           sizeof(caps->MaxImageSources),      &caps->MaxImageSources,      NULL);
    clGetDeviceInfo(device, CL_DEVICE_MAX_WRITE_IMAGE_ARGS,          sizeof(caps->MaxImageTargets),      &caps->MaxImageTargets,      NULL);
    // determine the number of partition types we got:
    caps->NumPartitionTypes = 0;
    for (size_t i = 0; i < sizeof(caps->PartitionTypes) / sizeof(caps->PartitionTypes[0]); ++i)
    {
        if (caps->PartitionTypes[i] == 0) break;
        else caps->NumPartitionTypes++;
    }
    // determine the number of affinity domains we got:
    caps->NumAffinityDomains = 0;
    for (size_t i = 0; i < sizeof(caps->AffinityDomains) / sizeof(caps->AffinityDomains[0]); ++i)
    {
        if (caps->AffinityDomains[i] == 0) break;
        else caps->NumAffinityDomains++;
    }
}

/// @summary Allocates and populates an OpenCL compute platform description.
/// @param desc The OpenCL compute platform description to populate.
/// @param platform The identifier of the OpenCL platform to query.
internal_function void cl_platform_describe(cl_platform_t *desc, cl_platform_id platform)
{   // initialize the platform definition to empty:
    memset(desc, 0, sizeof(cl_platform_t));

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
internal_function void cl_platform_delete(cl_platform_t *desc)
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
    memset(desc, 0, sizeof(cl_platform_t));
}

/// @summary Check a device to determine whether it supports at least OpenCL 1.2.
/// @param platform The OpenCL platform that defines the device.
/// @param device_index The zero-based index of the OpenCL device to check.
/// @return true if the device supports OpenCL 1.2 or later.
internal_function bool cl_device_supported(cl_platform_t *platform, size_t device_index)
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

/*////////////////////////
//   Public Functions   //
////////////////////////*/
/// @summary Determine the number of NUMA nodes, physical processors, cores and hardware threads.
/// @param counts The CPU count information to populate.
public_function void cpu_counts(cpu_counts_t *counts)
{
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *lpibuf = NULL;
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *info   = NULL;
    uint8_t *bufferp     = NULL;
    uint8_t *buffere     = NULL;
    DWORD    buffer_size = 0;
    
    // figure out the amount of space required, and allocate a temporary buffer:
    GetLogicalProcessorInformationEx(RelationAll, NULL, &buffer_size);
    if ((lpibuf = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*) malloc(size_t(buffer_size))) == NULL)
    {   // unable to allocate the required memory:
        counts->NUMANodes       = 1;
        counts->PhysicalCPUs    = 1;
        counts->PhysicalCores   = 1;
        counts->HardwareThreads = 1;
        return;
    }
    GetLogicalProcessorInformationEx(RelationAll, lpibuf, &buffer_size);

    // initialize the output counts:
    counts->NUMANodes       = 0;
    counts->PhysicalCPUs    = 0;
    counts->PhysicalCores   = 0;
    counts->HardwareThreads = 0;

    // step through the buffer and update counts:
    bufferp = (uint8_t*) lpibuf;
    buffere =((uint8_t*) lpibuf) + size_t(buffer_size);
    while (bufferp < buffere)
    {
        info = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*) bufferp;
        switch (info->Relationship)
        {
        case RelationNumaNode:
            counts->NUMANodes++;
            break;
        case RelationProcessorPackage:
            counts->PhysicalCPUs++;
            break;
        case RelationProcessorCore:
            counts->PhysicalCores++;
            if (info->Processor.Flags == LTP_PC_SMT)
            {   // this core has two hardware threads:
                counts->HardwareThreads += 2;
            }
            else
            {   // this core only has one hardware thread:
                counts->HardwareThreads++;
            }
            break;
        default: // RelationGroup, RelationCache - don't care.
            break;
        }
        bufferp += size_t(info->Size);
    }
    // free the temporary buffer:
    free(lpibuf);
}

/// @summary Open the compute driver by enumerating all OpenCL-capable devices in the system. 
/// @param driver The compute driver to initialize.
/// @return 0 if the driver is opened successfully, or -1 if an error has occurred.
public_function int cl_driver_open(cl_driver_t *driver)
{   
    size_t          cpu_device_count  = 0;
    size_t          gpu_device_count  = 0;
    cl_uint         platform_count    = 0;
    cl_platform_id *platform_ids      = NULL;
    cl_platform_t  *platform_desc     = NULL;

    // zero out all fields of the driver structure:
    memset(driver, 0, sizeof(cl_driver_t));

    // query the number of OpenCL-capable platforms in the system.
    clGetPlatformIDs(cl_uint(-1), NULL, &platform_count);
    if (platform_count == 0)
    {
        OutputDebugStringA("ERROR: No OpenCL-capable platforms are installed.\n");
        return -1;
    }
    platform_ids  = (cl_platform_id*)malloc(platform_count * sizeof(cl_platform_id));
    platform_desc = (cl_platform_t *)malloc(platform_count * sizeof(cl_platform_t ));
    clGetPlatformIDs(platform_count, platform_ids, NULL);

    // describe each platform in the system. this doesn't create any execution resources, 
    // but it does build a list of devices and query all device properties and capabilities.
    for (cl_uint platform_index = 0; platform_index < platform_count; ++platform_index)
    {   // describe the platform, and then update total device-type counts.
        cl_platform_id platform =  platform_ids [platform_index];
        cl_platform_t *desc     = &platform_desc[platform_index];
        cl_platform_describe(desc, platform);

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
        cpu_device_count += cpu_count;
        gpu_device_count += gpu_count;
    }

    // store all of the resulting data on the driver structure.
    driver->PlatformCount     = (size_t) platform_count;
    driver->PlatformIds       = platform_ids;
    driver->Platforms         = platform_desc;

    // reserve storage for the compute device groups:
    driver->CPU.DeviceCount   =  0;
    driver->CPU.DeviceIds     = (cl_device_id    *) malloc(cpu_device_count * sizeof(cl_device_id));
    driver->CPU.DeviceRank    = (cl_device_rank_t*) malloc(cpu_device_count * sizeof(cl_device_rank_t));
    driver->CPU.Devices       = (cl_device_cpu_t *) malloc(cpu_device_count * sizeof(cl_device_cpu_t));
    driver->GPU.DeviceCount   =  0;
    driver->GPU.DeviceIds     = (cl_device_id    *) malloc(gpu_device_count * sizeof(cl_device_id));
    driver->GPU.DeviceRank    = (cl_device_rank_t*) malloc(gpu_device_count * sizeof(cl_device_rank_t));
    driver->GPU.Devices       = (cl_device_gpu_t *) malloc(gpu_device_count * sizeof(cl_device_gpu_t));

    // populate the compute groups:
    for (cl_uint platform_index  = 0; platform_index < platform_count; ++platform_index)
    {   // describe the platform, and then update total device-type counts.
        cl_platform_id platform  =  platform_ids [platform_index];
        cl_platform_t *desc      = &platform_desc[platform_index];
        for (size_t device_index = 0, device_count = desc->DeviceCount; device_index < device_count; ++device_index)
        {
            if (cl_device_supported(desc, device_index) == false)
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
                driver->GPU.Devices  [gpu_index].DeviceFlags    = CL_COMPUTE_DEVICE_FLAGS_NONE;
                driver->GPU.Devices  [gpu_index].DisplayOrdinal = DISPLAY_ORDINAL_NONE;
            }
        }
    }
    return 0;
}

/// @summary Releases all resources associated with an OpenCL compute driver.
/// @param driver The compute driver to delete.
public_function void cl_driver_close(cl_driver_t *driver)
{   // release any resources associated with compute pipelines:
    for (size_t i = 0, n = driver->PipelineCount; i < n; ++i)
    {   // TODO(rlk): pipeline cleanup...
    }
    free(driver->PipelineData);
    free(driver->PipelineIds);
    driver->PipelineCount = 0;

    // release any command queues and contexts allocated to configured devices:
    for (size_t i = 0, n = driver->CPU.DeviceCount; i < n; ++i)
    {
        for (size_t j = 0, m = driver->CPU.Devices[i].SubDeviceCount; j < m; ++j)
        {
            clReleaseCommandQueue(driver->CPU.Devices[i].ComputeQueue[j]);
            clReleaseContext(driver->CPU.Devices[i].ComputeContext[j]);
        }
        free(driver->CPU.Devices[i].ComputeContext);
        free(driver->CPU.Devices[i].ComputeQueue);
        free(driver->CPU.Devices[i].SubDeviceCaps);
        free(driver->CPU.Devices[i].SubDeviceId);
        driver->CPU.Devices[i].SubDeviceCount = 0;
    }
    for (size_t i = 0, n = driver->GPU.DeviceCount; i < n; ++i)
    {
        clReleaseCommandQueue(driver->GPU.Devices[i].TransferQueue);
        clReleaseCommandQueue(driver->GPU.Devices[i].ComputeQueue);
        clReleaseContext(driver->GPU.Devices[i].ComputeContext);
    }
    free(driver->GPU.Devices);
    free(driver->GPU.DeviceRank);
    free(driver->GPU.DeviceIds);
    free(driver->CPU.Devices);
    free(driver->CPU.DeviceRank);
    free(driver->CPU.DeviceIds);
    driver->CPU.DeviceCount = 0;
    driver->GPU.DeviceCount = 0;

    // release resources associated with platform definitions.
    for (size_t i = 0, n  = driver->PlatformCount; i < n; ++i)
    {
        cl_platform_delete(&driver->Platforms[i]);
    }
    free(driver->Platforms);
    free(driver->PlatformIds);
    driver->PlatformCount = 0;
}

/// @summary Configure a CPU device for data-parallel operation. This is the default mode of operation. The OpenCL runtime distributes work items to all resources on the CPU.
/// @param device The CPU compute device to configure.
/// @return true if device configuration is successful.
public_function bool cl_configure_cpu_data_parallel(cl_device_cpu_t *device)
{
    cl_device_id     *sub_ids  = (cl_device_id    *) malloc(1 * sizeof(cl_device_id));
    cl_context       *sub_ctx  = (cl_context      *) malloc(1 * sizeof(cl_context));
    cl_command_queue *sub_fifo = (cl_command_queue*) malloc(1 * sizeof(cl_command_queue));
    cl_device_caps_t *sub_caps = (cl_device_caps_t*) malloc(1 * sizeof(cl_device_caps_t));

    // set up a single sub-device, which is really just the master device.
    // the reference count of the master device is not incremented.
    sub_ids [0] = device->MasterDeviceId;
    sub_ctx [0] = NULL; // created elsewhere
    sub_fifo[0] = NULL; // created elsewhere
    cl_device_capabilities(&sub_caps[0], device->MasterDeviceId);

    // update the device fields:
    device->SubDeviceCount = 1;
    device->SubDeviceId    = sub_ids;
    device->ComputeContext = sub_ctx;
    device->ComputeQueue   = sub_fifo;
    device->SubDeviceCaps  = sub_caps;
    return true;
}

/// @summary Configure a CPU device for task-parallel operation. Each compute unit maintains its own work queue. The application distributes work between the compute units.
/// @param device The CPU compute device to configure.
/// @return true if device configuration is successful.
public_function bool cl_configure_cpu_task_parallel(cl_device_cpu_t *device)
{
    size_t                         props_out  = 0;
    size_t const                   props_n    = 3;
    cl_device_partition_property   props[props_n];
    cl_device_id      master_id  = device->MasterDeviceId;
    cl_device_id     *sub_ids    = NULL;
    cl_context       *sub_ctx    = NULL; 
    cl_command_queue *sub_fifo   = NULL;
    cl_device_caps_t *sub_caps   = NULL;
    cpu_counts_t      cpu_count  = {};
    cl_uint           sub_count  = 0;
    cl_uint           divisor    = 1;
    bool              equally_ok = false;

    // query the CPU topology of the system:
    cpu_counts(&cpu_count);

    // query the number of sub-devices available, and the supported partition types.
    // OpenCL 1.2 may return EQUALLY, BY_COUNTS or BY_AFFINITY_DOMAIN. 
    memset(props, 0, sizeof(cl_device_partition_property) * props_n);
    clGetDeviceInfo(master_id, CL_DEVICE_PARTITION_MAX_SUB_DEVICES, sizeof(cl_uint), &sub_count, NULL);
    clGetDeviceInfo(master_id, CL_DEVICE_PARTITION_PROPERTIES     , sizeof(cl_device_partition_property) * props_n, props, &props_out);
    if (sub_count == 0) return false; // doesn't support partitioning.

    // ensure that the runtime supports the required parition type:
    for (size_t i = 0, n = props_out / sizeof(cl_device_partition_property); i < n; ++i)
    {
        if (props[i] == CL_DEVICE_PARTITION_EQUALLY)
        {   // the required partition type is supported.
            equally_ok = true;
            break;
        }
    }
    if (!equally_ok) return false; // doesn't support required partition type.

    // configure device fission for hyperthreading:
    if (cpu_count.HardwareThreads == (cpu_count.PhysicalCores * 2))
    {   // this is a hyperthreaded system; each device gets 2 threads.
        divisor = 2;
    }
    else
    {   // this is a non-hyperthreaded system; each device gets 1 thread.
        divisor = 1;
    }

    // allocate memory for the sub-device lists:
    sub_ids  = (cl_device_id    *) malloc((sub_count / divisor) * sizeof(cl_device_id));
    sub_ctx  = (cl_context      *) malloc((sub_count / divisor) * sizeof(cl_context));
    sub_fifo = (cl_command_queue*) malloc((sub_count / divisor) * sizeof(cl_command_queue));
    sub_caps = (cl_device_caps_t*) malloc((sub_count / divisor) * sizeof(cl_device_caps_t));

    // partition the device equally into a number of sub-devices:
    cl_device_partition_property partition_list[3] = 
    {
        (cl_device_partition_property) CL_DEVICE_PARTITION_EQUALLY, 
        (cl_device_partition_property) divisor, 
        (cl_device_partition_property) 0
    };
    cl_uint num_entries = sub_count / divisor;
    cl_uint num_devices = 0;
    clCreateSubDevices(master_id, partition_list, num_entries, sub_ids, &num_devices);
    if (num_devices == 0)
    {   // unable to parition the device.
        OutputDebugString(_T("ERROR: Unable to parition device (equally).\n"));
        free(sub_caps); free(sub_fifo); free(sub_ctx); free(sub_ids);
        return false;
    }

    // set up the device lists. sub_ids was populated above.
    for (size_t  i  = 0, n = size_t(num_devices); i < n; ++i)
    {
        sub_ctx [i] = NULL; // created elsewhere
        sub_fifo[i] = NULL; // created elsewhere
        cl_device_capabilities(&sub_caps[i], sub_ids[i]);
    }

    // update the device fields:
    device->SubDeviceCount = size_t(num_devices);
    device->SubDeviceId    = sub_ids;
    device->ComputeContext = sub_ctx;
    device->ComputeQueue   = sub_fifo;
    device->SubDeviceCaps  = sub_caps;
    return true;
}

/// @summary Configure a CPU device for maximum throughput with limited or no data sharing.
/// @param device The CPU compute device to configure.
/// @return true if device configuration is successful.
public_function bool cl_configure_cpu_high_throughput(cl_device_cpu_t *device)
{
    size_t                         props_out  = 0;
    size_t const                   props_n    = 3;
    cl_device_partition_property   props[props_n];
    cl_device_id      master_id  = device->MasterDeviceId;
    cl_device_id     *sub_ids    = NULL;
    cl_context       *sub_ctx    = NULL; 
    cl_command_queue *sub_fifo   = NULL;
    cl_device_caps_t *sub_caps   = NULL;
    cpu_counts_t      cpu_count  = {};
    cl_uint           sub_count  = 0;
    bool              domain_ok  = false;

    // query the CPU topology of the system:
    cpu_counts(&cpu_count);

    // query the number of sub-devices available, and the supported partition types.
    // OpenCL 1.2 may return EQUALLY, BY_COUNTS or BY_AFFINITY_DOMAIN. 
    memset(props, 0, sizeof(cl_device_partition_property) * props_n);
    clGetDeviceInfo(master_id, CL_DEVICE_PARTITION_MAX_SUB_DEVICES, sizeof(cl_uint), &sub_count, NULL);
    clGetDeviceInfo(master_id, CL_DEVICE_PARTITION_PROPERTIES     , sizeof(cl_device_partition_property) * props_n, props, &props_out);
    if (sub_count == 0) return false; // doesn't support partitioning.

    // ensure that the runtime supports the required parition type:
    for (size_t i = 0, n = props_out / sizeof(cl_device_partition_property); i < n; ++i)
    {
        if (props[i] == CL_DEVICE_PARTITION_BY_AFFINITY_DOMAIN)
        {   // the required partition type is supported.
            domain_ok = true;
            break;
        }
    }
    if (!domain_ok) return false; // doesn't support required partition type.

    // allocate memory for the sub-device lists. this method should create one 
    // sub-device for each NUMA node in the system, and each sub-device has access
    // to all available execution resources (hardware threads+caches) on that node.
    sub_ids  = (cl_device_id    *) malloc(cpu_count.NUMANodes * sizeof(cl_device_id));
    sub_ctx  = (cl_context      *) malloc(cpu_count.NUMANodes * sizeof(cl_context));
    sub_fifo = (cl_command_queue*) malloc(cpu_count.NUMANodes * sizeof(cl_command_queue));
    sub_caps = (cl_device_caps_t*) malloc(cpu_count.NUMANodes * sizeof(cl_device_caps_t));

    // partition the device equally into a number of sub-devices:
    cl_device_partition_property partition_list[3] = 
    {
        (cl_device_partition_property) CL_DEVICE_PARTITION_BY_AFFINITY_DOMAIN, 
        (cl_device_partition_property) CL_DEVICE_AFFINITY_DOMAIN_NUMA, 
        (cl_device_partition_property) 0
    };
    cl_uint num_entries = cl_uint(cpu_count.NUMANodes);
    cl_uint num_devices = 0;
    clCreateSubDevices(master_id, partition_list, num_entries, sub_ids, &num_devices);
    if (num_devices == 0)
    {   // unable to parition the device.
        OutputDebugString(_T("ERROR: Unable to parition device (NUMA nodes).\n"));
        free(sub_caps); free(sub_fifo); free(sub_ctx); free(sub_ids);
        return false;
    }

    // set up the device lists. sub_ids was populated above.
    for (size_t  i  = 0, n = size_t(num_devices); i < n; ++i)
    {
        sub_ctx [i] = NULL; // created elsewhere
        sub_fifo[i] = NULL; // created elsewhere
        cl_device_capabilities(&sub_caps[i], sub_ids[i]);
    }

    // update the device fields:
    device->SubDeviceCount = size_t(num_devices);
    device->SubDeviceId    = sub_ids;
    device->ComputeContext = sub_ctx;
    device->ComputeQueue   = sub_fifo;
    device->SubDeviceCaps  = sub_caps;
    return true;
}

/// @summary Configure a CPU device into a number of logical devices, each with a fixed number of hardware threads. This can be used to limit the number of hardware threads.
/// @param device The CPU compute device to configure.
/// @param units_per_device An array of device_count values where each value specifies the number of hardware threads to assign to a device.
/// @param device_count The number of logical devices to configure.
/// @return true if device configuration is successful.
public_function bool cl_configure_cpu_partition_count(cl_device_cpu_t *device, int *units_per_device, size_t device_count)
{   // to disable hardware threads, using an example 8 core HT system where we only want 4 cores to be used:
    // size_t const device_count = 1;
    // int units_per_device[device_count] = {8}; // enable 8 hardware threads out of 16 total
    // cl_configure_cpu_partition_count(&cpu_device, units_per_device, device_count);
    size_t                         props_out  = 0;
    size_t const                   props_n    = 3;
    cl_device_partition_property   props[props_n];
    cl_device_id      master_id  = device->MasterDeviceId;
    cl_device_id     *sub_ids    = NULL;
    cl_context       *sub_ctx    = NULL; 
    cl_command_queue *sub_fifo   = NULL;
    cl_device_caps_t *sub_caps   = NULL;
    cl_uint           sub_count  = 0;
    bool              counts_ok  = false;

    // query the number of sub-devices available, and the supported partition types.
    // OpenCL 1.2 may return EQUALLY, BY_COUNTS or BY_AFFINITY_DOMAIN. 
    memset(props, 0, sizeof(cl_device_partition_property) * props_n);
    clGetDeviceInfo(master_id, CL_DEVICE_PARTITION_MAX_SUB_DEVICES, sizeof(cl_uint), &sub_count, NULL);
    clGetDeviceInfo(master_id, CL_DEVICE_PARTITION_PROPERTIES     , sizeof(cl_device_partition_property) * props_n, props, &props_out);
    if (sub_count == 0) return false; // doesn't support partitioning.
    if (sub_count < device_count) return false; // too many sub-devices requested.

    // ensure that the runtime supports the required parition type:
    for (size_t i = 0, n = props_out / sizeof(cl_device_partition_property); i < n; ++i)
    {
        if (props[i] == CL_DEVICE_PARTITION_BY_COUNTS)
        {   // the required partition type is supported.
            counts_ok = true;
            break;
        }
    }
    if (!counts_ok) return false; // doesn't support required partition type.

    // allocate memory for the sub-device lists:
    sub_ids  = (cl_device_id    *) malloc(device_count * sizeof(cl_device_id));
    sub_ctx  = (cl_context      *) malloc(device_count * sizeof(cl_context));
    sub_fifo = (cl_command_queue*) malloc(device_count * sizeof(cl_command_queue));
    sub_caps = (cl_device_caps_t*) malloc(device_count * sizeof(cl_device_caps_t));

    // partition the device equally into a number of sub-devices:
    size_t partition_list_count = device_count + 2;
    cl_device_partition_property *partition_list = (cl_device_partition_property*) malloc(partition_list_count * sizeof(cl_device_partition_property));
    partition_list[0] = CL_DEVICE_PARTITION_BY_COUNTS;
    for (size_t i = 1, d = 0; d < device_count; ++i, ++d)
    {
        partition_list[i] = (cl_device_partition_property) units_per_device[d];
    }
    partition_list[partition_list_count-1] = CL_DEVICE_PARTITION_BY_COUNTS_LIST_END;
    cl_uint num_entries = cl_uint(device_count);
    cl_uint num_devices = 0;
    clCreateSubDevices(master_id, partition_list, num_entries, sub_ids, &num_devices);
    free(partition_list);
    if (num_devices == 0)
    {   // unable to parition the device.
        OutputDebugString(_T("ERROR: Unable to parition device (equally).\n"));
        free(sub_caps); free(sub_fifo); free(sub_ctx); free(sub_ids);
        return false;
    }

    // set up the device lists. sub_ids was populated above.
    for (size_t  i  = 0, n = size_t(num_devices); i < n; ++i)
    {
        sub_ctx [i] = NULL; // created elsewhere
        sub_fifo[i] = NULL; // created elsewhere
        cl_device_capabilities(&sub_caps[i], sub_ids[i]);
    }

    // update the device fields:
    device->SubDeviceCount = size_t(num_devices);
    device->SubDeviceId    = sub_ids;
    device->ComputeContext = sub_ctx;
    device->ComputeQueue   = sub_fifo;
    device->SubDeviceCaps  = sub_caps;
    return true;
}

// TODO(rlk): CL-GL interop (find preferred device on platform for HDC & HGLRC)
// TODO(rlk): execution resource setup (for each display, find GPU, add other devices and share flags, context creation.)
