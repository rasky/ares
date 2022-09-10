#include "fpuexcept.hpp"

#define FE_UNKNOWN  ((int)0)

#if defined(PLATFORM_MACOS) || defined(PLATFORM_WINDOWS)
int feenableexcept(unsigned int excepts);
int fedisableexcept(unsigned int excepts);
#endif

#if defined(PLATFORM_MACOS)
int feenableexcept(unsigned int excepts) {
    static fenv_t fenv;
    unsigned int new_excepts = excepts & FE_ALL_EXCEPT;

    if (fegetenv(&fenv)) return -1;

    // unmask
    #ifdef ARCHITECTURE_ARM64
    unsigned int old_excepts = fenv.__fpcr & (FE_ALL_EXCEPT << 8);
    fenv.__fpcr |= new_excepts << 8;
    #else
    unsigned int old_excepts = fenv.__control & FE_ALL_EXCEPT;
    fenv.__control &= ~new_excepts;
    fenv.__mxcsr   &= ~(new_excepts << 7);
    #endif

    return fesetenv(&fenv) ? -1 : old_excepts;
}

int fedisableexcept(unsigned int excepts) {
    static fenv_t fenv;
    unsigned int new_excepts = excepts & FE_ALL_EXCEPT;

    if (fegetenv(&fenv)) return -1;

    // mask
    #ifdef ARCHITECTURE_ARM64
    unsigned int old_excepts = fenv.__fpcr & (FE_ALL_EXCEPT << 8);
    fenv.__fpcr &= ~(new_excepts << 8);
    #else
    unsigned int old_excepts = fenv.__control & FE_ALL_EXCEPT;
    fenv.__control |= new_excepts;
    fenv.__mxcsr   |= new_excepts << 7;
    #endif

    return fesetenv(&fenv) ? -1 : old_excepts;
}

#elif defined(PLATFORM_WINDOWS)

inline auto control87_mask(int excepts) -> unsigned int {
  unsigned int mask = 0;
  if(excepts & FE_INEXACT)   mask |= _EM_INEXACT;
  if(excepts & FE_UNDERFLOW) mask |= _EM_UNDERFLOW;
  if(excepts & FE_OVERFLOW)  mask |= _EM_OVERFLOW;
  if(excepts & FE_DIVBYZERO) mask |= _EM_ZERODIVIDE;
  if(excepts & FE_INVALID)   mask |= _EM_INVALID;
  return mask;
}

auto feenableexcept(int excepts) -> int {
  unsigned int mask = control87_mask(excepts);
  _control87(0, mask);
  return 0;
}

auto fedisableexcept(int excepts) -> int {
  unsigned int mask = control87_mask(excepts);
  _control87(mask, mask);
  return 0;
}

#endif

namespace fpe {

    namespace internal {
        volatile int raised_mask;
        volatile int saved_mask;
        
        #if defined(FPE_HANDLER_SIGNAL_SJLJ)
        sigjmp_buf jmpbuf;
        #endif
        #if defined(FPE_HANDLER_VECTORED)
        void* handler = nullptr;
        #endif
    }

#if defined(FPE_HANDLER_SIGNAL) || defined(FPE_HANDLER_SIGNAL_SJLJ)
auto exceptionHandler(int signo, siginfo_t *si, void *data) -> void {
  #if defined(FPE_HANDLER_SIGNAL_SJLJ)
    // SJLJ mode can only handle exceptions withint a CHECK_FPE macro.
    // Anything else causes an abort.
    if(internal::raised_mask != FE_SJLJ) abort();
  #endif

  #if defined(PLATFORM_MACOS) && defined(ARCHITECTURE_ARM64)
    // Unfortunately, there does not seem to be a way to know which FPU exception triggered
    // on macOS ARM64. The information does not appear to be present in siginfo_t, nor it
    // is available from the state returned by fegetenv() (FPCR, FPSR). If it's somewhere,
    // it's well hidden.
    internal::raised_mask = FE_INVALID;
  #else
    switch(si->si_code) {
    case FPE_FLTDIV: internal::raised_mask = FE_DIVBYZERO; break;
    case FPE_FLTOVF: internal::raised_mask = FE_OVERFLOW; break;
    case FPE_FLTUND: internal::raised_mask = FE_UNDERFLOW; break;
    case FPE_FLTRES: internal::raised_mask = FE_INEXACT; break;
    case FPE_FLTINV: internal::raised_mask = FE_INVALID; break;
    default: internal::raised_mask = FE_INVALID; break;
    }
  #endif

#if defined(FPE_HANDLER_SIGNAL_SJLJ)
  siglongjmp(internal::jmpbuf, 1);
#else
  #if defined(ARCHITECTURE_AMD64)
    auto uc = (ucontext_t*)data;
    auto fpregs = uc->uc_mcontext.fpregs;
    internal::saved_mask = (fpregs->mxcsr >> 7) & FE_ALL_EXCEPT;
    fpregs->mxcsr |= 0x1e80; //pm um om zm im
  #elif defined(ARCHITECTURE_ARM64) && defined(PLATFORM_MACOS)
    auto uc = (ucontext_t*)data;
    auto fpregs = &uc->uc_mcontext->__ns;
    internal::saved_mask = (fpregs->__fpcr >> 8) & FE_ALL_EXCEPT;
    fpregs->__fpcr &= ~(FE_ALL_EXCEPT << 8);
  #else
    #error Unimplemented architecture
  #endif
#endif
}
#endif

#if defined(FPE_HANDLER_VECTORED) || defined(FPE_HANDLER_SEH)
auto internal::exceptionFilter(u32 code) -> int {
  switch(code) {
  case EXCEPTION_FLT_DIVIDE_BY_ZERO:    internal::raised_mask = FE_DIVBYZERO; break;
  case EXCEPTION_FLT_INEXACT_RESULT:    internal::raised_mask = FE_INEXACT; break;
  case EXCEPTION_FLT_OVERFLOW:          internal::raised_mask = FE_OVERFLOW; break;
  case EXCEPTION_FLT_UNDERFLOW:         internal::raised_mask = FE_UNDERFLOW; break;
  case EXCEPTION_FLT_INVALID_OPERATION: internal::raised_mask = FE_INVALID; break;
  default: return EXCEPTION_CONTINUE_SEARCH;
  }
  return EXCEPTION_EXECUTE_HANDLER;
}
#endif

#if defined(FPE_HANDLER_VECTORED)
auto NTAPI vectoredExceptionHandler(EXCEPTION_POINTERS* info) -> LONG
{
	auto code = info->ExceptionRecord->ExceptionCode;
  if(internal::exceptionFilter(code) == EXCEPTION_CONTINUE_SEARCH) {
    return EXCEPTION_CONTINUE_SEARCH;
  }
  //disable exceptions
#if defined(ARCHITECTURE_AMD64)
  info->ContextRecord->MxCsr |= 0x1e80; //pm um om zm im
#elif defined(ARCHITECTURE_ARM64)
  info->ContextRecord->Fpcr &= ~0x1f00; //ixe ufe ofe dze ioe
#else
  #error Unimplemented architecture
#endif
  return EXCEPTION_CONTINUE_EXECUTION;
}
#endif


auto install() -> void {
#if defined(FPE_HANDLER_SIGNAL) || defined(FPE_HANDLER_SIGNAL_SJLJ)
  struct sigaction act = {0};
  act.sa_sigaction = exceptionHandler;
  act.sa_flags = SA_NODEFER | SA_SIGINFO | SA_ONSTACK;
  #if defined(PLATFORM_MACOS) && defined(ARCHITECTURE_ARM64)
  sigaction(SIGILL, &act, NULL);
  #else
  sigaction(SIGFPE, &act, NULL);
  #endif
#endif
#if defined(FPE_HANDLER_VECTORED)
  internal::handler = AddVectoredExceptionHandler(1, vectoredExceptionHandler);
#endif
}

auto uninstall() -> void {
#if defined(FPE_HANDLER_SIGNAL) || defined(FPE_HANDLER_SIGNAL_SJLJ)
  struct sigaction act = { .sa_handler = SIG_DFL };
  #if defined(PLATFORM_MACOS) && defined(ARCHITECTURE_ARM64)
  sigaction(SIGILL, &act, NULL);
  #else
  sigaction(SIGFPE, &act, NULL);
  #endif
#endif

#if defined(FPE_HANDLER_VECTORED)
  if(internal::handler) {
    RemoveVectoredExceptionHandler(internal::handler);
    internal::handler = nullptr;
  }
#endif
}

auto begin(int exc_mask) -> void {
    feenableexcept(exc_mask);
#if !(defined(PLATFORM_MACOS) && defined(ARCHITECTURE_ARM64))
    // macOS/ARM64 seems to have a bug: if you disable the exception flags, 
    // exceptins don't trigger even if they are active (!).
    feclearexcept(FE_ALL_EXCEPT);
#endif
}

auto end(int exc_mask) -> void {
  fedisableexcept(exc_mask);
}

}


