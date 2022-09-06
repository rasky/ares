#pragma once

#ifdef PLATFORM_MACOS

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
    printf("enable: %llx %llx\n", fenv.__fpcr, fenv.__fpsr);

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

#endif
