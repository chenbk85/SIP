/*/////////////////////////////////////////////////////////////////////////////
/// @summary Define some terms to abstract compiler differences. This file is
/// included in all platform-specific code, and is also available for use in
/// application layer code.
///////////////////////////////////////////////////////////////////////////80*/

#ifndef INTRINSICS_H
#define INTRINSICS_H

/*////////////////
//   Includes   //
////////////////*/
#if    defined(_MSC_VER)
#include <intrin.h>
#elif  defined(__GNUC__)  && (defined(__x86_64__) ||  defined(__i386__))
#include <x86intrin.h>
#elif  defined(__GNUC__)  &&  defined(__ARM_NEON__)
#include <arm_neon.h>
#elif  defined(__GNUC__)  &&  defined(__IWMMXT__)
#include <mmintrin.h>
#elif (defined(__GNUC__)  ||  defined(__xlC__))   && (defined(__VEC__) || defined(__ALTIVEC__))
#include <altivec.h>
#elif  defined(__GNUC__)  &&  defined(__SPE__)
#include <spe.h>
#endif

/*////////////////////
//   Preprocessor   //
////////////////////*/
/// @summary Define common platform identifiers for platform detection, so that
/// client platform code isn't scattered with #if defined(...) blocks.
#define PLATFORM_UNKNOWN                    0
#define PLATFORM_iOS                        1
#define PLATFORM_ANDROID                    2
#define PLATFORM_WIN32                      3
#define PLATFORM_WINRT                      4
#define PLATFORM_WINP8                      5
#define PLATFORM_MACOS                      6
#define PLATFORM_LINUX                      7

/// @summary Define common compiler identifiers for compiler detection, so that
/// client platform code isn't scattered with #if defined(...) blocks.
#define COMPILER_UNKNOWN                    0
#define COMPILER_MSVC                       1
#define COMPILER_GNUC                       2

/// @summary Client code can check TARGET_PLATFORM to get the current target platform.
#define TARGET_PLATFORM                     PLATFORM_UNKNOWN

/// @summary Client code can check TARGET_COMPILER to get the current target compiler.
#define TARGET_COMPILER                     COMPILER_UNKNOWN

/// @summary Detect Visual C++ compiler. Visual C++ 11.0 (VS2012) is required.
#ifdef _MSC_VER
    #if _MSC_VER < 1700
        #error  Visual C++ 2012 or later is required.
    #else
        #undef  TARGET_COMPILER
        #define TARGET_COMPILER             COMPILER_MSVC
    #endif
#endif

/// @summary Detect GCC-compatible compilers (GNC C/C++, CLANG and Intel C++).
#ifdef __GNUC__
        #undef  TARGET_COMPILER
        #define TARGET_COMPILER             COMPILER_GNUC
#endif

/// @summary Detect Android-based platforms.
#if   defined(ANDROID)
        #undef  TARGET_PLATFORM
        #define TARGET_PLATFORM             PLATFORM_ANDROID
#endif

/// @summary Detect Apple platforms (MacOSX and iOS). Legacy MacOS not supported.
#if   defined(__APPLE__)
    #include <TargetConditionals.h>
    #if   defined(TARGET_OS_IPHONE) || defined(TARGET_IPHONE_SIMULATOR)
        #undef  TARGET_PLATFORM
        #define TARGET_PLATFORM             PLATFORM_iOS
    #elif defined(__MACH__)
        #undef  TARGET_PLATFORM
        #define TARGET_PLATFORM             PLATFORM_MACOS
    #else
        #error Legacy MacOS is not supported.
    #endif
#endif

/// @summary Detect Windows platforms. Desktop platforms are all PLATFORM_WIN32.
#if   defined(WIN32) || defined(WIN64) || defined(WINDOWS)
    #if   defined(WINRT)
        #undef  TARGET_PLATFORM
        #define TARGET_PLATFORM             PLATFORM_WINRT
    #elif defined(WP8)
        #undef  TARGET_PLATFORM
        #define TARGET_PLATFORM             PLATFORM_WINP8
    #else
        #undef  TARGET_PLATFORM
        #define TARGET_PLATFORM             PLATFORM_WIN32
    #endif
#endif

/// @summary Detect Linux and GNU/Linux.
#if   defined(__linux__) || defined(__gnu_linux__)
        #undef  TARGET_PLATFORM
        #define TARGET_PLATFORM             PLATFORM_LINUX
#endif

/// @summary Detect unsupported compilers.
#if   TARGET_COMPILER == COMPILER_UNKNOWN
        #error Unsupported compiler (update compiler detection?)
#endif

/// @summary Detect unsupported target platforms.
#if   TARGET_PLATFORM == PLATFORM_UNKNOWN
        #error Unsupported target platform (update platform detection?)
#endif

/// @summary Define force inline and never inline declarations for the compiler.
#if   defined(_MSC_VER)
        #define never_inline                __declspec(noinline)
        #define force_inline                __forceinline
#elif defined(__GNUC__)
        #define never_inline                __attribute__((noinline))
        #define force_inline                __attribute__((always_inline))
#else
        #error Unsupported compiler (need inlining declarations)
#endif

/// @summary Define alignment declarations for the compiler.
#if   defined(_MSC_VER)
        #define alignment(_alignment)       __declspec(align(_alignment))
        #define alignment_of(_x)            __alignof(_x)
#elif defined(__GNUC__)
        #define alignment(_alignment)       __attribute__((aligned(_alignment)))
        #define alignment_of(_x)            __alignof__(_x)
#else
        #error Unsupported compiler (need alignment declarations)
#endif

/// @summary Define restrict declarations for the compiler.
#if   defined(_MSC_VER)
        #define restrict_ptr                __restrict
#elif defined(__GNUC__)
        #define restrict_ptr                __restrict
#else
        #error Unsupported compiler (need restrict declaration)
#endif

/// @summary Define static/dynamic library import/export for the compiler.
#if   defined(_MSC_VER)
    #if   defined(BUILD_DYNAMIC)
        #define library_function            __declspec(dllexport)
    #elif defined(BUILD_STATIC)
        #define library_function
    #else
        #define library_function            __declspec(dllimport)
    #endif
#elif defined(__GNUC__)
    #if   defined(BUILD_DYNAMIC)
        #define library_function
    #elif defined(BUILD_STATIC)
        #define library_function
    #else
        #define library_function
    #endif
#else
        #error Unsupported compiler (need dynamic library declarations)
#endif

/// @summary Define memory barrier intrinsics for the compiler and processor.
#if   defined(_MSC_VER)
        #define COMPILER_BARRIER_LOAD       _ReadBarrier()
        #define COMPILER_BARRIER_STORE      _WriteBarrier()
        #define COMPILER_BARRIER_FULL       _ReadWriteBarrier()
        #define HARDWARE_BARRIER_LOAD       _mm_lfence()
        #define HARDWARE_BARRIER_STORE      _mm_sfence()
        #define HARDWARE_BARRIER_FULL       _mm_mfence()
#elif defined(__GNUC__)
    #if    defined(__INTEL_COMPILER)
        #define COMPILER_BARRIER_LOAD       __memory_barrier()
        #define COMPILER_BARRIER_STORE      __memory_barrier()
        #define COMPILER_BARRIER_FULL       __memory_barrier()
        #define HARDWARE_BARRIER_LOAD       __mm_lfence()
        #define HARDWARE_BARRIER_STORE      __mm_sfence()
        #define HARDWARE_BARRIER_FULL       __mm_mfence()
    #elif (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1)
        #define COMPILER_BARRIER_LOAD       __asm__ __volatile__ ("" : : : "memory")
        #define COMPILER_BARRIER_STORE      __asm__ __volatile__ ("" : : : "memory")
        #define COMPILER_BARRIER_FULL       __asm__ __volatile__ ("" : : : "memory")
        #define HARDWARE_BARRIER_LOAD       __sync_synchronize()
        #define HARDWARE_BARRIER_STORE      __sync_synchronize()
        #define HARDWARE_BARRIER_FULL       __sync_synchronize()
    #elif defined(__ppc__) || defined(__powerpc__) || defined(__PPC__)
        #define COMPILER_BARRIER_LOAD       __asm__ __volatile__ ("" : : : "memory")
        #define COMPILER_BARRIER_STORE      __asm__ __volatile__ ("" : : : "memory")
        #define COMPILER_BARRIER_FULL       __asm__ __volatile__ ("" : : : "memory")
        #define HARDWARE_BARRIER_LOAD       __asm__ __volatile__ ("lwsync" : : : "memory")
        #define HARDWARE_BARRIER_STORE      __asm__ __volatile__ ("lwsync" : : : "memory")
        #define HARDWARE_BARRIER_FULL       __asm__ __volatile__ ("lwsync" : : : "memory")
    #elif defined(__arm__)
        #define COMPILER_BARRIER_LOAD       __asm__ __volatile__ ("" : : : "memory")
        #define COMPILER_BARRIER_STORE      __asm__ __volatile__ ("" : : : "memory")
        #define COMPILER_BARRIER_FULL       __asm__ __volatile__ ("" : : : "memory")
        #define HARDWARE_BARRIER_LOAD       __asm__ __volatile__ ("dmb" : : : "memory")
        #define HARDWARE_BARRIER_STORE      __asm__ __volatile__ ("dmb" : : : "memory")
        #define HARDWARE_BARRIER_FULL       __asm__ __volatile__ ("dmb" : : : "memory")
    #elif defined(__i386__) || defined(__i486__) || defined(__i586__) || \
          defined(__i686__) || defined(__x86_64__)
        #define COMPILER_BARRIER_LOAD       __asm__ __volatile__ ("" : : : "memory")
        #define COMPILER_BARRIER_STORE      __asm__ __volatile__ ("" : : : "memory")
        #define COMPILER_BARRIER_FULL       __asm__ __volatile__ ("" : : : "memory")
        #define HARDWARE_BARRIER_LOAD       __asm__ __volatile__ ("lfence" : : : "memory")
        #define HARDWARE_BARRIER_STORE      __asm__ __volatile__ ("sfence" : : : "memory")
        #define HARDWARE_BARRIER_FULL       __asm__ __volatile__ ("mfence" : : : "memory")
    #else
        #error Unsupported __GNUC__ (need memory barrier intrinsics)
    #endif
#else
    #error Unsupported compiler (need memory barrier intrinsics)
#endif

/// @summary Define the size of a cacheline on the target CPU. Most modern
/// x86 and x86_64 processors have 64-byte cache lines. Cache line-sized
/// padding is used between data manipulated by the producer and consumer
/// in order to prevent performance related problems due to false sharing.
#ifndef CACHELINE_SIZE
#define CACHELINE_SIZE                      64
#endif

/// @summary Helper macro to begin a multi-line macro safely.
#ifndef MULTI_LINE_MACRO_BEGIN
#define MULTI_LINE_MACRO_BEGIN              do {
#endif

/// @summary Helper macro to close a multi-line macro safely.
#ifndef MULTI_LINE_MACRO_CLOSE
    #if   defined(_MSC_VER)
        #define MULTI_LINE_MACRO_CLOSE      \
        __pragma(warning(push))             \
        __pragma(warning(disable:4127))     \
        } while (0);                        \
        __pragma(warning(pop))
    #elif defined(__GNUC__)
        #define MULTI_LINE_MACRO_CLOSE      \
        } while (0)
    #endif
#endif

/// @summary Helper macro to prevent compiler warnings about unused arguments.
#ifndef UNUSED_HELPER
#define UNUSED_HELPER(x)                    (void)sizeof((x))
#endif

/// @summary Prevent compiler warnings about unused arguments.
#ifndef UNUSED_ARG
#define UNUSED_ARG(x)                       \
    MULTI_LINE_MACRO_BEGIN                  \
        UNUSED_HELPER(x);                   \
    MULTI_LINE_MACRO_CLOSE
#endif

/// @summary Tag used to mark a function as available for use outside of the
/// current translation unit (the default visibility).
#ifndef export_function
#define export_function                     library_function
#endif

/// @summary Tag used to mark a function as available for public use, but not
/// exported outside of the translation unit.
#ifndef public_function
#define public_function                     static
#endif

/// @summary Tag used to mark a function internal to the translation unit.
#ifndef internal_function
#define internal_function                   static
#endif

/// @summary Tag used to mark a variable as local to a function, and persistent
/// across invocations of that function.
#ifndef local_persist
#define local_persist                       static
#endif

/// @summary Tag used to mark a variable as global to the translation unit.
#ifndef global_variable
#define global_variable                     static
#endif

/// @summary Define an errno result code for success, since none is provided.
/// This is used for clairty purposes only.
#ifndef ENOERROR
#define ENOERROR                            0
#endif

/*/////////////////
//   Constants   //
/////////////////*/

/*////////////////////////////
//   Forward Declarations   //
////////////////////////////*/

/*//////////////////
//   Data Types   //
//////////////////*/
/// @summary Typedef for a cacheline-sized array of bytes. This is typically
/// used for padding to prevent false-sharing between cores.
typedef uint8_t cacheline_t[CACHELINE_SIZE];

/*/////////////////
//   Functions   //
/////////////////*/
/// @summary Rounds a size up to the nearest even multiple of a given power-of-two.
/// @param size The size value to round up.
/// @param pow2 The power-of-two alignment.
/// @return The input size, rounded up to the nearest even multiple of pow2.
public_function inline size_t align_up(size_t size, size_t pow2)
{
    assert((pow2 & (pow2-1)) == 0);
    return (size == 0) ? pow2 : ((size + (pow2-1)) & ~(pow2-1));
}

/// @summary Rounds a size up to the nearest even multiple of a given power-of-two.
/// @param size The size value to round up.
/// @param pow2 The power-of-two alignment.
/// @return The input size, rounded up to the nearest even multiple of pow2.
public_function inline int64_t align_up(int64_t size, size_t pow2)
{
    assert((pow2 & (pow2-1)) == 0);
    uint64_t pow2_64   = uint64_t(pow2);
    uint64_t pow2_64m  = pow2_64 - 1;
    return (size == 0) ? int64_t(pow2_64) : int64_t((size + pow2_64m) & ~pow2_64m);
}

/// @summary Clamps a value to a given maximum.
/// @param size The size value to clamp.
/// @param limit The upper-bound to clamp to.
/// @return The smaller of size and limit.
public_function inline size_t clamp_to(size_t size, size_t limit)
{
    return (size > limit) ? limit : size;
}

/// @summary Calculates the next power-of-two greater than or equal to a value.
/// @param x The input value.
/// @return A power-of-two value greater than or equal to the input value.
public_function inline size_t next_pow2(size_t x)
{
    size_t n = 1;
    while (n < x)
    {
        n  <<= 1;
    }
    return n;
}

#undef  PLATFORM_INTRINSICS_DEFINED
#define PLATFORM_INTRINSICS_DEFINED
#endif /* !defined(INTRINSICS_H) */

