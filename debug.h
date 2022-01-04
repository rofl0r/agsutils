#ifndef DEBUG_H
#define DEBUG_H

#define empty_body do {} while(0)
#if defined(__x86_64) || defined(__x86_64__) || defined(_M_X64)
#  define INTEL_CPU 64
#elif defined(__x86) || defined(__x86__) || defined(__i386) || defined(__i386__) || defined(_M_IX86)
#  define INTEL_CPU 32
#endif

//#define NO_BREAKPOINTS
#ifdef NO_BREAKPOINTS
#  define breakpoint() empty_body
#else
#  ifdef INTEL_CPU
#   ifdef __GNUC__
#    define breakpoint() __asm__("int3")
#   elif defined(__POCC__)
#    define breakpoint() __asm { int3 }
#   else
#    define breakpoint() empty_body
#   endif
#  elif defined __unix__
#    include <signal.h>
#    include <unistd.h>
#    define breakpoint() kill(getpid(), SIGTRAP)
#  else
#   define breakpoint() empty_body
#  endif
#endif

#if !defined(__GNUC__) && (defined(__POCC__) || __STDC_VERSION__+0 >= 199901L)
#define __FUNCTION__ __func__
#endif

//#define NO_ASSERT
#ifdef NO_ASSERT
#  define assert_dbg(exp) empty_body
#  define assert_lt(exp) empty_body
#  define assert_lte(exp) empty_body
#  define assert(x) empty_body
#else
#  include <stdio.h>
#  define assert_dbg(exp) do { if (!(exp)) breakpoint(); } while(0)
#  define assert_op(val, op, max) do { if(!((val) op (max))) { \
    fprintf(stderr, "[%s:%d] %s: assert failed: %s < %s\n", \
    __FILE__, __LINE__, __FUNCTION__, # val, # max); \
    breakpoint();}} while(0)

#  define assert_lt (val, max) assert_op(val, <, max)
#  define assert_lte(val, max) assert_op(val, <= ,max)
#endif

#endif
