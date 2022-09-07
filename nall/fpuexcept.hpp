#pragma once

#if defined(PLATFORM_MACOS)

inline int feenableexcept(unsigned int excepts)
{
    static fenv_t fenv;
    unsigned int new_excepts = excepts & FE_ALL_EXCEPT;

    if (fegetenv(&fenv)) {
        return -1;
    }

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

inline int fedisableexcept(unsigned int excepts)
{
    static fenv_t fenv;
    unsigned int new_excepts = excepts & FE_ALL_EXCEPT;

    if (fegetenv(&fenv)) {
        return -1;
    }

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

inline auto feenableexcept(int excepts) -> int {
  unsigned int mask = control87_mask(excepts);
  _control87(0, mask);
  return 0;
}

inline auto fedisableexcept(int excepts) -> int {
  unsigned int mask = control87_mask(excepts);
  _control87(mask, mask);
  return 0;
}

#endif
