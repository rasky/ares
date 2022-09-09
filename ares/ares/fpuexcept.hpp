#pragma once

#include <nall/intrinsics.hpp>
#include <nall/maybe.hpp>

/*
 * Support for intercepting FPU exceptions.
 *
 * We implement different strategies:
 *   * FPE_HANDLER_VECTORED: use Windows Vectored Exception Handler.
 *   * FPE_HANDLER_SEH: use Windows Structured Exception Handler (__try / __except).
 *   * FPE_HANDLER_SIGNAL: use POSIX signals (SIGFPE) to intercept the exception, then disable
 *     it to continue and re-execute the offending instruction.
 *   * FPE_HANDLER_SIGNAL_SJLJ: use POSIX signals (SIGFPE) to intercept the exception, then
 *     use setjmp/longjmp to recover execution to a safe place and skip the offending instruction.
 * 
 * NOTE:
 *   * GCC doesn't support SEH (while LLVM does). We use VEH in this case.
 *   * macOS/ARM64 has broken FPE support in general. It generates a SIGILL instead of SIGFPE,
 *     and there is no way to know which kind of exception was triggered.
 */

#if defined(PLATFORM_WINDOWS)
  #if defined(COMPILER_GCC)
    #define FPE_HANDLER_VECTORED
  #else
    #define FPE_HANDLER_SEH
  #endif
#elif defined(PLATFORM_LINUX) && defined(ARCHITECTURE_AMD64)
    #define FPE_HANDLER_SIGNAL
#elif defined(PLATFORM_MACOS) && defined(ARCHITECTURE_ARM64)
    #define FPE_HANDLER_SIGNAL
#else
    #define FPE_HANDLER_SIGNAL_SJLJ
#endif

#if defined(FPE_HANDLER_SIGNAL_SJLJ)
#include <setjmp.h>
#endif
#if defined(FPE_HANDLER_SIGNAL) || defined(FPE_HANDLER_SIGNAL_SJLJ)
#include <signal.h>
#endif

namespace fpe {
  namespace internal {
    extern volatile int raised_mask;
    extern volatile int saved_mask;
    
    #if defined(FPE_HANDLER_SIGNAL_SJLJ)
    extern sigjmp_buf jmpbuf;
    #endif

    #if defined(FPE_HANDLER_VECTORED) || defined(FPE_HANDLER_SEH)
    auto exceptionFilter(u32 code) -> int;
    #endif
  }

  auto install() -> void;
  auto uninstall() -> void;
  auto begin(int exc_mask) -> void;
  auto end(int exc_mask) -> void;
}

#if defined(FPE_HANDLER_SEH)
#define CHECK_FPE(type, operation, exc_func) ({ \
  type res; \
  __try { \
    /* due to an LLVM limitation, it is not possible to catch an asynchronous */ \
    /* exception generated in the same frame as the catching __try. */ \
    res = [&] { return operation; }(); \
  } __except(fpe::internal::exceptionFilter(exception_code())) { \
    exc_func(fpe::internal::raised_mask); \
    return; \
  } \
  (res); \
})
#endif

#if defined(FPE_HANDLER_VECTORED) || defined(FPE_HANDLER_SIGNAL)
#define CHECK_FPE(type, operation, exc_func) ({ \
  fpe::internal::raised_mask = 0; \
  volatile type res = operation; \
  if(fpe::internal::raised_mask) { \
    exc_func(fpe::internal::raised_mask); \
    fpe::begin(fpe::internal::saved_mask); \
    return; \
  } \
  (res); \
})
#endif

#if defined(FPE_HANDLER_SIGNAL_SJLJ)
#define FE_SJLJ     -1

#define CHECK_FPE(type, operation, exc_func) ({ \
  volatile type res; \
  fpe::internal::raised_mask = FE_SJLJ; \
  if(sigsetjmp(fpe::internal::jmpbuf, 1)) { \
    exc_func(fpe::internal::raised_mask); \
    fpe::internal::raised_mask = 0; \
    return; \
  } else { \
    res = operation; \
    fpe::internal::raised_mask = 0; \
  } \
  (res); \
})
#endif
