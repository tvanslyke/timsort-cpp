#ifndef TIMSORT_COMPILER_H
#define TIMSORT_COMPILER_H

# ifndef TIMSORT_NO_USE_COMPILER_INTRINSICS

#  if defined _MSC_VER
#   include "intrin.h"
#  endif

#  if defined  _MSC_VER		   			
#   define COMPILER_UNREACHABLE_ __assume(0);   		
#  elif defined __GNUC__ || defined __clang__ 			
#   define COMPILER_UNREACHABLE_ __builtin_unreachable();	
#  else							 
#   define COMPILER_UNREACHABLE_  			
#  endif 						

#  if defined __GNUC__
#   define GCC_ASSUME__(x) do{ if(not (x)) {__builtin_unreachable();} } while(false);
#  endif

#  if defined  _MSC_VER || defined __INTEL_COMPILER		
#   define COMPILER_ASSUME_(x) __assume(x);   		 			
#  elif defined __clang__
#   if __has_builtin(__builtin_assume)
#    define COMPILER_ASSUME_(x) __builtin_assume(x)
#   else
#    define COMPILER_ASSUME_(x) GCC_ASSUME__(x)
#   endif	
#  elif defined __GNUC__
#   define COMPILER_ASSUME_(x) GCC_ASSUME__(x)					
#  else								
#   define COMPILER_ASSUME_(x) 
#  endif 

#  if defined __GNUC__
#   define COMPILER_LIKELY_(x) __builtin_expect(x, true)
#   define COMPILER_UNLIKELY_(x) __builtin_expect(x, false)
#  else
#   define COMPILER_LIKELY_(x)   (x) 
#   define COMPILER_UNLIKELY_(x) (x)
#  endif

# else
#  define COMPILER_LIKELY_(x)   (x)
#  define COMPILER_UNLIKELY_(x) (x)
#  define COMPILER_ASSUME_(x)   (x) 
#  define COMPILER_UNREACHABLE_ 

# endif /* TIMSORT_NO_USE_COMPILER_INTRINSICS */

#endif /* TIMSORT_COMPILER_H */
