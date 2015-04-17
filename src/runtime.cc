/*/////////////////////////////////////////////////////////////////////////////
/// @summary Defines constants, data types and functions that comprise the
/// Windows runtime shim (RTS). This module dynamically loads kernel32.dll and
/// attempts to resolve various optional entry points that may or may not be
/// present in the current environment. It also defines types that may not be
/// defined by the MinGW compiler, if that build environment is being used.
/// Windows Vista is the minimum supported runtime environment. Additionally
/// defines the prototypes and functions for Event Tracing for Windows. The
/// system will attempt to load etwprovider.dll and resolve various custom
/// event functions useful when profiling and debugging.
///////////////////////////////////////////////////////////////////////////80*/

/*////////////////////
//   Preprocessor   //
////////////////////*/
#ifdef _MSC_VER
    #define ETW_C_API                          __cdecl
#else
    #define ETW_C_API
#endif

#ifdef __GNUC__
#ifndef QUOTA_LIMITS_HARDWS_MIN_ENABLE
    #define QUOTA_LIMITS_HARDWS_MIN_ENABLE     0x00000001
#endif

#ifndef QUOTA_LIMITS_HARDWS_MAX_DISABLE
    #define QUOTA_LIMITS_HARDWS_MAX_DISABLE    0x00000008
#endif
#ifndef METHOD_BUFFERED
    #define METHOD_BUFFERED                    0
#endif
#ifndef FILE_ANY_ACCESS
    #define FILE_ANY_ACCESS                    0
#endif

#ifndef FILE_DEVICE_MASS_STORAGE
    #define FILE_DEVICE_MASS_STORAGE           0x0000002d
#endif

#ifndef CTL_CODE
    #define CTL_CODE(DeviceType, Function, Method, Access )                      \
        (((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method))
#endif

#ifndef IOCTL_STORAGE_BASE
    #define IOCTL_STORAGE_BASE                 FILE_DEVICE_MASS_STORAGE
#endif

#ifndef IOCTL_STORAGE_QUERY_PROPERTY
    #define IOCTL_STORAGE_QUERY_PROPERTY CTL_CODE(IOCTL_STORAGE_BASE, 0x0500, METHOD_BUFFERED, FILE_ANY_ACCESS)
#endif

#ifndef FILE_SKIP_COMPLETION_PORT_ON_SUCCESS
    #define FILE_SKIP_COMPLETION_PORT_ON_SUCCESS    0x1
#endif

#ifndef FILE_SKIP_SET_EVENT_ON_HANDLE
    #define FILE_SKIP_SET_EVENT_ON_HANDLE           0x2
#endif

#if   (__GNUC__ < 4) || (__GNUC__ == 4 && __GNUC_MINOR__ <= 8)
typedef enum _STORAGE_PROPERTY_ID {
    StorageDeviceProperty = 0,
    StorageAdapterProperty,
    StorageDeviceIdProperty,
    StorageDeviceUniqueIdProperty,
    StorageDeviceWriteCacheProperty,
    StorageMiniportProperty,
    StorageAccessAlignmentProperty,
    StorageDeviceSeekPenaltyProperty,
    StorageDeviceTrimProperty,
    StorageDeviceWriteAggregationProperty,
    StorageDeviceTelemetryProperty,
    StorageDeviceLBProvisioningProperty,
    StorageDevicePowerProperty,
    StorageDeviceCopyOffloadProperty,
    StorageDeviceResiliencyPropery
} STORAGE_PROPERTY_ID, *PSTORAGE_PROPERTY_ID;

typedef enum _STORAGE_QUERY_TYPE {
    PropertyStandardQuery = 0,
    propertyExistsQuery,
    PropertyMaskQuery,
    PropertyQueryMaxDefined
} STORAGE_QUERY_TYPE, *PSTORAGE_QUERY_TYPE;

typedef struct _STORAGE_PROPERTY_QUERY {
    STORAGE_PROPERTY_ID PropertyId;
    STORAGE_QUERY_TYPE  QueryType;
    UCHAR               AdditionalParameters[1];
} STORAGE_PROPERTY_QUERY, *PSTORAGE_PROPERTY_QUERY;
#endif /* __GNUC__ < 4.9 */

#if (__GNUC__ < 4) || (__GNUC__ == 4 && __GNUC_MINOR__ <= 9)
typedef enum _FILE_INFO_BY_HANDLE_CLASS {
    FileBasicInfo = 0,
    FileStandardInfo = 1,
    FileNameInfo = 2,
    FileRenameInfo = 3,
    FileDispositionInfo = 4,
    FileAllocationInfo = 5,
    FileEndOfFileInfo = 6,
    FileStreamInfo = 7,
    FileCompressionInfo = 8,
    FileAttributeTagInfo = 9,
    FileIdBothDirectoryInfo = 10,
    FileIdBothDirectoryRestartInfo = 11,
    FileIoPriorityHintInfo = 12,
    FileRemoteProtocolInfo = 13,
    FileFullDirectoryInfo = 14,
    FileFullDirectoryRestartInfo = 15,
    FileStorageInfo = 16,
    FileAlignmentInfo = 17,
    FileIdInfo = 18,
    FileIdExtdDirectoryInfo = 19,
    FileIdExtdDirectoryRestartInfo = 20,
    MaximumFileInfoByHandlesClass
} FILE_INFO_BY_HANDLE_CLASS, *PFILE_INFO_BY_HANDLE_CLASS;

typedef struct _STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR {
    DWORD Version;
    DWORD Size;
    DWORD BytesPerCacheLine;
    DWORD BytesOffsetForCacheAlignment;
    DWORD BytesPerLogicalSector;
    DWORD BytesPerPhysicalSector;
    DWORD BytesOffsetForSectorAlignment;
} STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR, *PSTORAGE_ACCESS_ALIGNMENT_DESCRIPTOR;

typedef struct _FILE_ALLOCATION_INFO {
    LARGE_INTEGER AllocationSize;
} FILE_ALLOCATION_INFO, *PFILE_ALLOCATION_INFO;

typedef struct _FILE_END_OF_FILE_INFO {
    LARGE_INTEGER EndOfFile;
} FILE_END_OF_FILE_INFO, *PFILE_END_OF_FILE_INFO;

typedef struct _FILE_NAME_INFO {
    DWORD FileNameLength;
    WCHAR FileName[1];
} FILE_NAME_INFO, *PFILE_NAME_INFO;
#endif /* __GNUC__ <= 4.9 - MinGW */
#endif /* __GNUC__ comple - MinGW */

/// @summary Resolve a function pointer from etwprovider.dll, and set the
/// function pointer to the stub function if it can't be resolved. For this
/// to work, you must following some naming conventions. Given function name:
///
/// name = ETWFoo (without quotes)
///
/// The function pointer (typedef) should be: ETWFooFn
/// The global function pointer should be:    ETWFoo_Func
/// The stub/no-op function name should be:   ETWFoo_Stub
/// The resolve call in win32_runtime_init:   ETW_DLL_RESOLVE(dll_inst, ETWFoo);
#ifdef _MSC_VER
#define ETW_DLL_RESOLVE(dll, fname)                                            \
    do {                                                                       \
        fname##_Func = (fname##Fn) GetProcAddress(dll, #fname);                \
        if (fname##_Func == NULL){ fname##_Func = fname##_Stub; }              \
    __pragma(warning(push));                                                   \
    __pragma(warning(disable:4127));                                           \
    } while(0);                                                                \
    __pragma(warning(pop))
#endif

#ifdef __GNUC__
#define ETW_DLL_RESOLVE(dll, fname)                                            \
    do {                                                                       \
        fname##_Func = (fname##Fn) GetProcAddress(dll, #fname);                \
        if (fname##_Func == NULL){ fname##_Func = fname##_Stub; }              \
    } while(0)
#endif

/*////////////////
//   Includes   //
////////////////*/
#include <sal.h>

#ifndef _Printf_format_string_
#define _Printf_format_string_
#endif

/*/////////////////
//   Constants   //
/////////////////*/
/// @summary Define the size of the output buffer when writing formatted markers.
/// This may be overridden at compile time; ie. /D ETW_FORMAT_BUFFER_SIZE=###.
#ifndef ETW_FORMAT_BUFFER_SIZE
#define ETW_FORMAT_BUFFER_SIZE     1024
#endif

/*///////////////////
//   Local Types   //
///////////////////*/
/// @summary Define constants for the standard set of mouse buttons.
enum etw_button_e : int32_t
{
    ETW_BUTTON_LEFT              = 0,
    ETW_BUTTON_MIDDLE            = 1,
    ETW_BUTTON_RIGHT             = 2
};

/// @summary Define various bitflags that specify input event attributes.
enum etw_input_flags_e : uint32_t
{
    ETW_INPUT_FLAGS_NONE         = (0 << 0), /// Indicates no special input attributes.
    ETW_INPUT_FLAGS_DOUBLE_CLICK = (1 << 0)  /// Indicates a double-click mouse event.
};

/// @summary Define function signatures for various functions loaded dynamically.
/// For more information, refer to MSDN for the function type, minus the 'Fn'.
typedef void     (WINAPI    *GetNativeSystemInfoFn)(SYSTEM_INFO*);
typedef BOOL     (WINAPI    *SetProcessWorkingSetSizeExFn)(HANDLE, SIZE_T, SIZE_T, DWORD);
typedef BOOL     (WINAPI    *SetFileInformationByHandleFn)(HANDLE, FILE_INFO_BY_HANDLE_CLASS, LPVOID, DWORD);
typedef BOOL     (WINAPI    *GetQueuedCompletionStatusExFn)(HANDLE, LPOVERLAPPED_ENTRY, ULONG, PULONG, DWORD, BOOL);
typedef BOOL     (WINAPI    *SetFileCompletionNotificationModesFn)(HANDLE, UCHAR);
typedef DWORD    (WINAPI    *GetFinalPathNameByHandleFn)(HANDLE, LPWSTR, DWORD, DWORD);

/// @summary Function pointer typedefs for the functions exported from etwprovider.dll.
/// The DLL may export more or fewer functions, but these are the ones we look for.
/// Any that can't be resolved are set to no-op stub functions.
typedef void     (ETW_C_API *ETWRegisterCustomProvidersFn)(void);
typedef void     (ETW_C_API *ETWUnregisterCustomProvidersFn)(void);
typedef void     (ETW_C_API *ETWThreadIdFn)(char const*, DWORD);
typedef void     (ETW_C_API *ETWMarkerMainFn)(char const*);
typedef void     (ETW_C_API *ETWMarkerFormatMainVFn)(char*, size_t, char const*, va_list);
typedef void     (ETW_C_API *ETWMarkerTaskFn)(char const*);
typedef void     (ETW_C_API *ETWMarkerFormatTaskVFn)(char*, size_t, char const*, va_list);
typedef void     (ETW_C_API *ETWMouseDownFn)(int, DWORD, int, int);
typedef void     (ETW_C_API *ETWMouseUpFn)(int, DWORD, int, int);
typedef void     (ETW_C_API *ETWMouseMoveFn)(DWORD, int, int);
typedef void     (ETW_C_API *ETWMouseWheelFn)(DWORD, int, int, int);
typedef void     (ETW_C_API *ETWKeyDownFn)(DWORD, char const*, DWORD, DWORD);
typedef LONGLONG (ETW_C_API *ETWEnterScopeMainFn)(char const*);
typedef LONGLONG (ETW_C_API *ETWLeaveScopeMainFn)(char const*, LONGLONG);
typedef LONGLONG (ETW_C_API *ETWEnterScopeTaskFn)(char const*);
typedef LONGLONG (ETW_C_API *ETWLeaveScopeTaskFn)(char const*, LONGLONG);

/// @summary A helper class for tracing scope enter and exit on the main thread.
class trace_scope_main_t
{
public:
    trace_scope_main_t(char const *description);
    ~trace_scope_main_t(void);
private:
    trace_scope_main_t(void);
    trace_scope_main_t(trace_scope_main_t const &);
    trace_scope_main_t& operator =(trace_scope_main_t const &);
private:
    char const *Description;
    LONGLONG    EnterTime;
};

/// @summary A helper class for tracing scope enter and exit on a task thread.
class trace_scope_task_t
{
public:
    trace_scope_task_t(char const *description);
    ~trace_scope_task_t(void);
private:
    trace_scope_task_t(void);
    trace_scope_task_t(trace_scope_task_t const &);
    trace_scope_task_t& operator =(trace_scope_task_t const &);
private:
    char const *Description;
    LONGLONG    EnterTime;
};

/*///////////////
//   Globals   //
///////////////*/
/// @summary Global function pointers for the Vista+ functions dynamically loaded from kernel32.dll.
global_variable GetNativeSystemInfoFn                GetNativeSystemInfo_Func                = NULL;
global_variable GetFinalPathNameByHandleFn           GetFinalPathNameByHandle_Func           = NULL;
global_variable SetProcessWorkingSetSizeExFn         SetProcessWorkingSetSizeEx_Func         = NULL;
global_variable SetFileInformationByHandleFn         SetFileInformationByHandle_Func         = NULL;
global_variable GetQueuedCompletionStatusExFn        GetQueuedCompletionStatusEx_Func        = NULL;
global_variable SetFileCompletionNotificationModesFn SetFileCompletionNotificationModes_Func = NULL;

/// @summary Global function pointers for the custom ETW functions dynamically loaded from etwprovider.dll.
/// Any functions that cannot be loaded will be set to no-op stub implementations in win32_runtime_init().
global_variable ETWRegisterCustomProvidersFn         ETWRegisterCustomProviders_Func         = NULL;
global_variable ETWUnregisterCustomProvidersFn       ETWUnregisterCustomProviders_Func       = NULL;
global_variable ETWThreadIdFn                        ETWThreadId_Func                        = NULL;
global_variable ETWMarkerMainFn                      ETWMarkerMain_Func                      = NULL;
global_variable ETWMarkerFormatMainVFn               ETWMarkerFormatMainV_Func               = NULL;
global_variable ETWMarkerTaskFn                      ETWMarkerTask_Func                      = NULL;
global_variable ETWMarkerFormatTaskVFn               ETWMarkerFormatTaskV_Func               = NULL;
global_variable ETWEnterScopeMainFn                  ETWEnterScopeMain_Func                  = NULL;
global_variable ETWLeaveScopeMainFn                  ETWLeaveScopeMain_Func                  = NULL;
global_variable ETWEnterScopeTaskFn                  ETWEnterScopeTask_Func                  = NULL;
global_variable ETWLeaveScopeTaskFn                  ETWLeaveScopeTask_Func                  = NULL;
global_variable ETWMouseDownFn                       ETWMouseDown_Func                       = NULL;
global_variable ETWMouseUpFn                         ETWMouseUp_Func                         = NULL;
global_variable ETWMouseMoveFn                       ETWMouseMove_Func                       = NULL;
global_variable ETWMouseWheelFn                      ETWMouseWheel_Func                      = NULL;
global_variable ETWKeyDownFn                         ETWKeyDown_Func                         = NULL;

/// @summary Global handles to DLLs loaded dynamically by win32_runtime_init().
global_variable HMODULE Kernel32_DLL    = NULL;
global_variable HMODULE ETWProvider_DLL = NULL;

/*///////////////////////
//   Local Functions   //
///////////////////////*/
internal_function void ETW_C_API ETWRegisterCustomProviders_Stub(void)
{
    /* empty */
}

internal_function void ETW_C_API ETWUnregisterCustomProviders_Stub(void)
{
    /* empty */
}

/// @summary Emits an event specifying the name associated with a given thread ID.
/// This function should be called when the thread first starts up.
/// @param thread_name A NULL-terminated string identifying the thread.
/// @param thread_id The operating system identifier of the thread.
internal_function void ETW_C_API ETWThreadId_Stub(char const *thread_name, DWORD thread_id)
{
    UNREFERENCED_PARAMETER(thread_name);
    UNREFERENCED_PARAMETER(thread_id);
}

/// @summary Emits a string marker event to the tracing system.
/// @param message A NULL-terminated string to appear in the trace output.
internal_function void ETW_C_API ETWMarkerMain_Stub(char const *message)
{
    UNREFERENCED_PARAMETER(message);
}

/// @summary Emits a formatted string marker event to the tracing system.
/// @param format A NULL-terminated string following Windows printf format specifier rules.
/// @param ... Substitution arguments for the format string.
internal_function void ETW_C_API ETWMarkerFormatMainV_Stub(char *buffer, size_t count, char const *format, va_list args)
{
    UNREFERENCED_PARAMETER(buffer);
    UNREFERENCED_PARAMETER(count);
    UNREFERENCED_PARAMETER(format);
    UNREFERENCED_PARAMETER(args);
}

/// @summary Emits a string marker event to the tracing system.
/// @param message A NULL-terminated string to appear in the trace output.
internal_function void ETW_C_API ETWMarkerTask_Stub(char const *message)
{
    UNREFERENCED_PARAMETER(message);
}

/// @summary Emits a formatted string marker event to the tracing system.
/// @param format A NULL-terminated string following Windows printf format specifier rules.
/// @param ... Substitution arguments for the format string.
internal_function void ETW_C_API ETWMarkerFormatTaskV_Stub(char *buffer, size_t count, char const *format, va_list args)
{
    UNREFERENCED_PARAMETER(buffer);
    UNREFERENCED_PARAMETER(count);
    UNREFERENCED_PARAMETER(format);
    UNREFERENCED_PARAMETER(args);
}

/// @summary Emits a mouse button press event to the tracing system.
/// @param button One of the values of etw_button_e, or a custom value identifying the button.
/// @param flags A combination of one or more of etw_input_flags_e.
/// @param x The x-coordinate of the mouse cursor when the button was pressed.
/// @param y The y-coordinate of the mouse cursor when the button was pressed.
internal_function void ETW_C_API ETWMouseDown_Stub(int button, DWORD flags, int x, int y)
{
    UNREFERENCED_PARAMETER(button);
    UNREFERENCED_PARAMETER(flags);
    UNREFERENCED_PARAMETER(x);
    UNREFERENCED_PARAMETER(y);
}

/// @summary Emits a mouse button release event to the tracing system.
/// @param button One of the values of etw_button_e, or a custom value identifying the button.
/// @param flags A combination of one or more of etw_input_flags_e.
/// @param x The x-coordinate of the mouse cursor when the button was released.
/// @param y The y-coordinate of the mouse cursor when the button was released.
internal_function void ETW_C_API ETWMouseUp_Stub(int button, DWORD flags, int x, int y)
{
    UNREFERENCED_PARAMETER(button);
    UNREFERENCED_PARAMETER(flags);
    UNREFERENCED_PARAMETER(x);
    UNREFERENCED_PARAMETER(y);
}

/// @summary Emits a mouse move event to the tracing system.
/// @param flags A combination of one or more of etw_input_flags_e.
/// @param x The x-coordinate of the mouse cursor.
/// @param y The y-coordinate of the mouse cursor.
internal_function void ETW_C_API ETWMouseMove_Stub(DWORD flags, int x, int y)
{
    UNREFERENCED_PARAMETER(flags);
    UNREFERENCED_PARAMETER(x);
    UNREFERENCED_PARAMETER(y);
}

/// @summary Emits a mouse wheel change event to the tracing system.
/// @param flags A combination of one or more of etw_input_flags_e.
/// @param delta_z The amount of movement on the z-axis (mouse wheel).
/// @param x The x-coordinate of the mouse cursor at the time of the event.
/// @param y The y-coordinate of the mouse cursor at the time of the event.
internal_function void ETW_C_API ETWMouseWheel_Stub(DWORD flags, int delta_z, int x, int y)
{
    UNREFERENCED_PARAMETER(flags);
    UNREFERENCED_PARAMETER(delta_z);
    UNREFERENCED_PARAMETER(x);
    UNREFERENCED_PARAMETER(y);
}

/// @summary Emits a key press event to the tracing system.
/// @param character The raw character code of the key that was pressed.
/// @param name A NULL-terminated string specifying a name for the pressed key.
/// @param repeat_count The number of key repeats that have occurred for this key.
/// @param flags A combination of one or more values of etw_input_flags_e.
internal_function void ETW_C_API ETWKeyDown_Stub(DWORD character, char const *name, DWORD repeat_count, DWORD flags)
{
    UNREFERENCED_PARAMETER(character);
    UNREFERENCED_PARAMETER(name);
    UNREFERENCED_PARAMETER(repeat_count);
    UNREFERENCED_PARAMETER(flags);
}

/// @summary Indicates that a named, timed scope is being entered. Typically,
/// this function is not called directly by user code; instead, it is easier
/// and safer to use the trace_scope_main_t class.
/// @param message A NULL-terminated string identifying the scope.
/// @return The current timestamp, which must be saved and passed to trace_leave_scope_main().
internal_function LONGLONG ETW_C_API ETWEnterScopeMain_Stub(char const *message)
{
    UNREFERENCED_PARAMETER(message);
    return 0;
}

/// @summary Indicates that a named, time scope is being exited. Typically,
/// this function is not called directly by user code; instead, it is easier
/// and safer to use the trace_scope_main_t class.
/// @param message A NULL-terminated string identifying the scope.
/// @param enter_time The timestamp returned from trace_enter_scope_main().
internal_function LONGLONG ETW_C_API ETWLeaveScopeMain_Stub(char const *message, LONGLONG enter_time)
{
    UNREFERENCED_PARAMETER(message);
    UNREFERENCED_PARAMETER(enter_time);
    return 0;
}

/// @summary Indicates that a named, timed scope is being entered. Typically,
/// this function is not called directly by user code; instead, it is easier
/// and safer to use the trace_scope_task_t class.
/// @param message A NULL-terminated string identifying the scope.
/// @return The current timestamp, which must be saved and passed to trace_leave_scope_task().
internal_function LONGLONG ETW_C_API ETWEnterScopeTask_Stub(char const *message)
{
    UNREFERENCED_PARAMETER(message);
    return 0;
}

/// @summary Indicates that a named, time scope is being exited. Typically,
/// this function is not called directly by user code; instead, it is easier
/// and safer to use the trace_scope_task_t class.
/// @param message A NULL-terminated string identifying the scope.
/// @param enter_time The timestamp returned from trace_enter_scope_task().
internal_function LONGLONG ETW_C_API ETWLeaveScopeTask_Stub(char const *message, LONGLONG enter_time)
{
    UNREFERENCED_PARAMETER(message);
    UNREFERENCED_PARAMETER(enter_time);
    return 0;
}

/// @summary Initializes the ETW event tracing system. This function should be
/// called from the main application thread. This function must not be called
/// from DllMain(), as it calls LoadLibraru. This function looks for the
/// etwprovider.dll file and dynamically loads it into the process address
/// space if found. If the DLL cannot be found, or cannot be loaded, all ETW
/// functions are safe to call, but no custom events will be emitted.
internal_function void ETWInitialize(void)
{
#ifndef ETW_STRIP_IMPLEMENTATION
    static TCHAR const *ETW_PROVIDER_DLL_PATH = _T("%TEMP%\\etwprovider.dll");

    HMODULE dll_inst = NULL;
    TCHAR  *dll_path = NULL;
    DWORD   path_len = 0;
    if ((path_len = ExpandEnvironmentStrings(ETW_PROVIDER_DLL_PATH, NULL, 0)) == 0)
    {   // ExpandEnvironmentStrings() failed. Use the stub functions.
        // call GetLastError() if you want to know the specific reason.
        goto use_stubs;
    }

    // allocate a buffer large enough for the full path.
    // ExpandEnvironmentStrings() returns the necessary length, in *characters*,
    // and including the terminating NULL character, but MSDN says that for ANSI
    // strings, the buffer size should include space for one extra zero byte, and
    // it isn't clear from the docs whether path_len includes this extra byte...
    if ((dll_path = (TCHAR*)malloc((path_len + 1) * sizeof(TCHAR))) == NULL)
    {   // unable to allocate the necessary memory; fail. check errno.
        goto use_stubs;
    }
    if ((path_len = ExpandEnvironmentStrings(ETW_PROVIDER_DLL_PATH, dll_path, path_len + 1)) == 0)
    {   // for some reason now we can't perform the expansion.
        // call GetLastError() if you want to know the specific reason.
        goto cleanup_and_use_stubs;
    }

    // now attempt to load etwprovider.dll from the specific location.
    if ((dll_inst = LoadLibrary(dll_path)) == NULL)
    {   // if that didn't work, check the standard search paths.
        if ((dll_inst = LoadLibrary(_T("etwprovider.dll"))) == NULL)
        {   // can't find it, so use the stubs.
            goto cleanup_and_use_stubs;
        }
    }

    // the DLL was loaded from somewhere, so resolve the ETW functions.
    // we could be loading from an older version of the DLL, so it's possible
    // that some functions are available while others are not. the macro will
    // set any missing functions to use the stub implementation.
    ETW_DLL_RESOLVE(dll_inst, ETWRegisterCustomProviders);
    ETW_DLL_RESOLVE(dll_inst, ETWUnregisterCustomProviders);
    ETW_DLL_RESOLVE(dll_inst, ETWThreadId);
    ETW_DLL_RESOLVE(dll_inst, ETWMarkerMain);
    ETW_DLL_RESOLVE(dll_inst, ETWMarkerFormatMainV);
    ETW_DLL_RESOLVE(dll_inst, ETWEnterScopeMain);
    ETW_DLL_RESOLVE(dll_inst, ETWLeaveScopeMain);
    ETW_DLL_RESOLVE(dll_inst, ETWMarkerTask);
    ETW_DLL_RESOLVE(dll_inst, ETWMarkerFormatTaskV);
    ETW_DLL_RESOLVE(dll_inst, ETWEnterScopeTask);
    ETW_DLL_RESOLVE(dll_inst, ETWLeaveScopeTask);
    ETW_DLL_RESOLVE(dll_inst, ETWMouseDown);
    ETW_DLL_RESOLVE(dll_inst, ETWMouseUp);
    ETW_DLL_RESOLVE(dll_inst, ETWMouseMove);
    ETW_DLL_RESOLVE(dll_inst, ETWMouseWheel);
    ETW_DLL_RESOLVE(dll_inst, ETWKeyDown);

    // register the custom providers as part of the initialization.
    ETWRegisterCustomProviders_Func();

    // done with everything, so clean up.
    free(dll_path);   dll_path = NULL;
    ETWProvider_DLL = dll_inst;
    return;

cleanup_and_use_stubs:
    if (dll_inst != NULL) FreeLibrary(dll_inst);
    if (dll_path != NULL) free(dll_path);
    /* fallthrough */

use_stubs:
    ETWRegisterCustomProviders_Func   = ETWRegisterCustomProviders_Stub;
    ETWUnregisterCustomProviders_Func = ETWUnregisterCustomProviders_Stub;
    ETWThreadId_Func                  = ETWThreadId_Stub;
    ETWMarkerMain_Func                = ETWMarkerMain_Stub;
    ETWMarkerFormatMainV_Func         = ETWMarkerFormatMainV_Stub;
    ETWEnterScopeMain_Func            = ETWEnterScopeMain_Stub;
    ETWLeaveScopeMain_Func            = ETWLeaveScopeMain_Stub;
    ETWMarkerTask_Func                = ETWMarkerTask_Stub;
    ETWMarkerFormatTaskV_Func         = ETWMarkerFormatTaskV_Stub;
    ETWEnterScopeTask_Func            = ETWEnterScopeTask_Stub;
    ETWLeaveScopeTask_Func            = ETWLeaveScopeTask_Stub;
    ETWMouseDown_Func                 = ETWMouseDown_Stub;
    ETWMouseUp_Func                   = ETWMouseUp_Stub;
    ETWMouseMove_Func                 = ETWMouseMove_Stub;
    ETWMouseWheel_Func                = ETWMouseWheel_Stub;
    ETWKeyDown_Func                   = ETWKeyDown_Stub;
#else
    /* stripped */
#endif
}

/// @summary Shut down the custom ETW event tracing system. This function must
/// not be called from DllMain(), since it calls FreeLibrary().
internal_function void ETWShutdown(void)
{
#ifndef ETW_STRIP_IMPLEMENTATION
    // unregister all custom providers; no more custom events will be emitted.
    ETWUnregisterCustomProviders_Func();

    // point all of the function pointers at the local stubs for safety.
    ETWRegisterCustomProviders_Func   = ETWRegisterCustomProviders_Stub;
    ETWUnregisterCustomProviders_Func = ETWUnregisterCustomProviders_Stub;
    ETWThreadId_Func                  = ETWThreadId_Stub;
    ETWMarkerMain_Func                = ETWMarkerMain_Stub;
    ETWMarkerFormatMainV_Func         = ETWMarkerFormatMainV_Stub;
    ETWEnterScopeMain_Func            = ETWEnterScopeMain_Stub;
    ETWLeaveScopeMain_Func            = ETWLeaveScopeMain_Stub;
    ETWMarkerTask_Func                = ETWMarkerTask_Stub;
    ETWMarkerFormatTaskV_Func         = ETWMarkerFormatTaskV_Stub;
    ETWEnterScopeTask_Func            = ETWEnterScopeTask_Stub;
    ETWLeaveScopeTask_Func            = ETWLeaveScopeTask_Stub;
    ETWMouseDown_Func                 = ETWMouseDown_Stub;
    ETWMouseUp_Func                   = ETWMouseUp_Stub;
    ETWMouseMove_Func                 = ETWMouseMove_Stub;
    ETWMouseWheel_Func                = ETWMouseWheel_Stub;
    ETWKeyDown_Func                   = ETWKeyDown_Stub;

    // unload the DLL, which should only have one reference.
    if (ETWProvider_DLL != NULL)
    {
        FreeLibrary(ETWProvider_DLL);
        ETWProvider_DLL  = NULL;
    }
#else
    /* stripped */
#endif
}

/*////////////////////////
//   Public Functions   //
////////////////////////*/
/// @summary Loads function entry points that may not be available at compile
/// time with some build environments.
public_function bool win32_runtime_init(void)
{   // it's a safe assumption that kernel32.dll is mapped into our process
    // address space, and will remain mapped for the duration of execution.
    HMODULE kernel = GetModuleHandleA("kernel32.dll");
    if (kernel != NULL)
    {
        GetNativeSystemInfo_Func                 = (GetNativeSystemInfoFn)                GetProcAddress(kernel, "GetNativeSystemInfo");
        GetFinalPathNameByHandle_Func            = (GetFinalPathNameByHandleFn)           GetProcAddress(kernel, "GetFinalPathNameByHandleW");
        SetProcessWorkingSetSizeEx_Func          = (SetProcessWorkingSetSizeExFn)         GetProcAddress(kernel, "SetProcessWorkingSetSizeEx");
        SetFileInformationByHandle_Func          = (SetFileInformationByHandleFn)         GetProcAddress(kernel, "SetFileInformationByHandle");
        GetQueuedCompletionStatusEx_Func         = (GetQueuedCompletionStatusExFn)        GetProcAddress(kernel, "GetQueuedCompletionStatusEx");
        SetFileCompletionNotificationModes_Func  = (SetFileCompletionNotificationModesFn) GetProcAddress(kernel, "SetFileCompletionNotificationModes");
    }
    // fail if any of these APIs are not available.
    if (GetNativeSystemInfo_Func                == NULL) return false;
    if (GetFinalPathNameByHandle_Func           == NULL) return false;
    if (SetProcessWorkingSetSizeEx_Func         == NULL) return false;
    if (SetFileInformationByHandle_Func         == NULL) return false;
    if (GetQueuedCompletionStatusEx_Func        == NULL) return false;
    if (SetFileCompletionNotificationModes_Func == NULL) return false;
    // initialize event tracing support.
    ETWInitialize();
    return true;
}

/// @summary Elevates the privileges for the process to include the privilege
/// SE_MANAGE_VOLUME_NAME, so that SetFileValidData() can be used to initialize
/// a file without having to zero-fill the underlying sectors. This is optional,
/// and we don't want it failing to prevent the application from launching.
public_function void win32_runtime_elevate(void)
{
    TOKEN_PRIVILEGES tp;
    HANDLE        token;
    LUID           luid;

    OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &token);
    if (LookupPrivilegeValue(NULL, SE_MANAGE_VOLUME_NAME, &luid))
    {
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        if (AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), NULL, NULL))
        {   // Privilege adjustment succeeded.
            CloseHandle(token);
        }
        else
        {   // Privilege adjustment failed.
            CloseHandle(token);
        }
    }
    else
    {   // lookup of SE_MANAGE_VOLUME_NAME failed.
        CloseHandle(token);
    }
}

/// @summary Indicates that a named, timed scope is being entered. Typically,
/// this function is not called directly by user code; instead, it is easier
/// and safer to use the trace_scope_main_t class.
/// @param message A NULL-terminated string identifying the scope.
/// @return The current timestamp, which must be saved and passed to trace_leave_scope_main().
public_function LONGLONG trace_enter_scope_main(char const *message)
{
#ifndef ETW_STRIP_IMPLEMENTATION
    return ETWEnterScopeMain_Func(message);
#else
    UNREFERENCED_PARAMETER(message);
    return 0;
#endif
}

/// @summary Indicates that a named, time scope is being exited. Typically,
/// this function is not called directly by user code; instead, it is easier
/// and safer to use the trace_scope_main_t class.
/// @param message A NULL-terminated string identifying the scope.
/// @param enter_time The timestamp returned from trace_enter_scope_main().
public_function LONGLONG trace_leave_scope_main(char const *message, LONGLONG enter_time)
{
#ifndef ETW_STRIP_IMPLEMENTATION
    return ETWLeaveScopeMain_Func(message, enter_time);
#else
    UNREFERENCED_PARAMETER(message);
    UNREFERENCED_PARAMETER(enter_time);
    return 0;
#endif
}

/// @summary Indicates that a named, timed scope is being entered. Typically,
/// this function is not called directly by user code; instead, it is easier
/// and safer to use the trace_scope_task_t class.
/// @param message A NULL-terminated string identifying the scope.
/// @return The current timestamp, which must be saved and passed to trace_leave_scope_task().
public_function LONGLONG trace_enter_scope_task(char const *message)
{
#ifndef ETW_STRIP_IMPLEMENTATION
    return ETWEnterScopeTask_Func(message);
#else
    UNREFERENCED_PARAMETER(message);
    return 0;
#endif
}

/// @summary Indicates that a named, time scope is being exited. Typically,
/// this function is not called directly by user code; instead, it is easier
/// and safer to use the trace_scope_task_t class.
/// @param message A NULL-terminated string identifying the scope.
/// @param enter_time The timestamp returned from trace_enter_scope_task().
public_function LONGLONG trace_leave_scope_task(char const *message, LONGLONG enter_time)
{
#ifndef ETW_STRIP_IMPLEMENTATION
    return ETWLeaveScopeTask_Func(message, enter_time);
#else
    UNREFERENCED_PARAMETER(message);
    UNREFERENCED_PARAMETER(enter_time);
    return 0;
#endif
}

/// @summary Emits an event specifying the name associated with a given thread ID.
/// This function should be called when the thread first starts up.
/// @param thread_name A NULL-terminated string identifying the thread.
public_function void trace_thread_id(char const *thread_name)
{
#ifndef ETW_STRIP_IMPLEMENTATION
    return ETWThreadId_Func(thread_name, GetCurrentThreadId());
#else
    UNREFERENCED_PARAMETER(thread_name);
#endif
}

/// @summary Emits a string marker event to the tracing system.
/// @param message A NULL-terminated string to appear in the trace output.
public_function void trace_marker_main(char const *message)
{
#ifndef ETW_STRIP_IMPLEMENTATION
    return ETWMarkerMain_Func(message);
#else
    UNREFERENCED_PARAMETER(message);
#endif
}

/// @summary Emits a formatted string marker event to the tracing system.
/// @param format A NULL-terminated string following Windows printf format specifier rules.
/// @param ... Substitution arguments for the format string.
public_function void trace_marker_mainf(_Printf_format_string_ char const *format, ...)
{
#ifndef ETW_STRIP_IMPLEMENTATION
    char buffer[ETW_FORMAT_BUFFER_SIZE];
    va_list  args;
    va_start(args, format);
    ETWMarkerFormatMainV_Func(buffer, ETW_FORMAT_BUFFER_SIZE, format, args);
    va_end(args);
#else
    UNREFERENCED_PARAMETER(format);
#endif
}

/// @summary Emits a string marker event to the tracing system.
/// @param message A NULL-terminated string to appear in the trace output.
public_function void trace_marker_task(char const *message)
{
#ifndef ETW_STRIP_IMPLEMENTATION
    return ETWMarkerTask_Func(message);
#else
    UNREFERENCED_PARAMETER(message);
#endif
}

/// @summary Emits a formatted string marker event to the tracing system.
/// @param format A NULL-terminated string following Windows printf format specifier rules.
/// @param ... Substitution arguments for the format string.
public_function void trace_marker_taskf(_Printf_format_string_ char const *format, ...)
{
#ifndef ETW_STRIP_IMPLEMENTATION
    char buffer[ETW_FORMAT_BUFFER_SIZE];
    va_list  args;
    va_start(args, format);
    ETWMarkerFormatTaskV_Func(buffer, ETW_FORMAT_BUFFER_SIZE, format, args);
    va_end(args);
#else
    UNREFERENCED_PARAMETER(format);
#endif
}

/// @summary Emits a mouse button press event to the tracing system.
/// @param button One of the values of etw_button_e, or a custom value identifying the button.
/// @param flags A combination of one or more of etw_input_flags_e.
/// @param x The x-coordinate of the mouse cursor when the button was pressed.
/// @param y The y-coordinate of the mouse cursor when the button was pressed.
public_function void trace_mouse_down(int button, DWORD flags, int x, int y)
{
#ifndef ETW_STRIP_IMPLEMENTATION
    ETWMouseDown_Func(button, flags, x, y);
#else
    UNREFERENCED_PARAMETER(button);
    UNREFERENCED_PARAMETER(flags);
    UNREFERENCED_PARAMETER(x);
    UNREFERENCED_PARAMETER(y);
#endif
}

/// @summary Emits a mouse button release event to the tracing system.
/// @param button One of the values of etw_button_e, or a custom value identifying the button.
/// @param flags A combination of one or more of etw_input_flags_e.
/// @param x The x-coordinate of the mouse cursor when the button was released.
/// @param y The y-coordinate of the mouse cursor when the button was released.
public_function void trace_mouse_up(int button, DWORD flags, int x, int y)
{
#ifndef ETW_STRIP_IMPLEMENTATION
    ETWMouseUp_Func(button, flags, x, y);
#else
    UNREFERENCED_PARAMETER(button);
    UNREFERENCED_PARAMETER(flags);
    UNREFERENCED_PARAMETER(x);
    UNREFERENCED_PARAMETER(y);
#endif
}

/// @summary Emits a mouse move event to the tracing system.
/// @param flags A combination of one or more of etw_input_flags_e.
/// @param x The x-coordinate of the mouse cursor.
/// @param y The y-coordinate of the mouse cursor.
public_function void trace_mouse_move(DWORD flags, int x, int y)
{
#ifndef ETW_STRIP_IMPLEMENTATION
    ETWMouseMove_Func(flags, x, y);
#else
    UNREFERENCED_PARAMETER(flags);
    UNREFERENCED_PARAMETER(x);
    UNREFERENCED_PARAMETER(y);
#endif
}

/// @summary Emits a mouse wheel change event to the tracing system.
/// @param flags A combination of one or more of etw_input_flags_e.
/// @param delta_z The amount of movement on the z-axis (mouse wheel).
/// @param x The x-coordinate of the mouse cursor at the time of the event.
/// @param y The y-coordinate of the mouse cursor at the time of the event.
public_function void trace_mouse_wheel(DWORD flags, int delta_z, int x, int y)
{
#ifndef ETW_STRIP_IMPLEMENTATION
    ETWMouseWheel_Func(flags, delta_z, x, y);
#else
    UNREFERENCED_PARAMETER(flags);
    UNREFERENCED_PARAMETER(delta_z);
    UNREFERENCED_PARAMETER(x);
    UNREFERENCED_PARAMETER(y);
#endif
}

/// @summary Emits a key press event to the tracing system.
/// @param character The raw character code of the key that was pressed.
/// @param name A NULL-terminated string specifying a name for the pressed key.
/// @param repeat_count The number of key repeats that have occurred for this key.
/// @param flags A combination of one or more values of etw_input_flags_e.
public_function void trace_key_down(DWORD character, char const *name, DWORD repeat_count, DWORD flags)
{
#ifndef ETW_STRIP_IMPLEMENTATION
    ETWKeyDown_Func(character, name, repeat_count, flags);
#else
    UNREFERENCED_PARAMETER(character);
    UNREFERENCED_PARAMETER(name);
    UNREFERENCED_PARAMETER(repeat_count);
    UNREFERENCED_PARAMETER(flags);
#endif
}

/// @summary Indicates that a scope is being entered on the main thread.
/// @param description A NULL-terminated string specifying a scope description.
inline trace_scope_main_t::trace_scope_main_t(char const *description)
    :
    Description(description)
{
    EnterTime = trace_enter_scope_main(description);
}

/// @summary Indicates that the scope is being exited on the main thread.
inline trace_scope_main_t::~trace_scope_main_t(void)
{
    trace_leave_scope_main(Description, EnterTime);
}

/// @summary Indicates that a scope is being entered on a task thread.
/// @param description A NULL-terminated string specifying a scope description.
inline trace_scope_task_t::trace_scope_task_t(char const *description)
    :
    Description(description)
{
    EnterTime = trace_enter_scope_task(description);
}

/// @summary Indicates that the scope is being exited on a task thread.
inline trace_scope_task_t::~trace_scope_task_t(void)
{
    trace_leave_scope_task(Description, EnterTime);
}

