#ifndef COMPILER_H
#define COMPILER_H

#if defined _MSC_VER
# include "intrin.h"
#endif

#if defined  _MSC_VER		   			
# define COMPILER_UNREACHABLE_ __assume(0);   		
#elif defined __GNUC__ || defined __clang__ 			
# define COMPILER_UNREACHABLE_ __builtin_unreachable();	
#else							 
# define COMPILER_UNREACHABLE_  			
#endif 						



#if defined __GNUC__
# define GCC_ASSUME__(x) do{ if(not (x)) {__builtin_unreachable();} } while(false);
#endif


#if defined  _MSC_VER || defined __INTEL_COMPILER		
# define COMPILER_ASSUME_(x) __assume(x);   		 			
#elif defined __clang__
# if __has_builtin(__builtin_assume)
#  define COMPILER_ASSUME_(x) __builtin_assume(x)
# else
#  define COMPILER_ASSUME_(x) GCC_ASSUME__(x)
# endif	
#elif defined __GNUC__
# define COMPILER_ASSUME_(x) GCC_ASSUME__(x)					
#else								
# define COMPILER_ASSUME_(x) 
#endif 


#if defined __GNUC__
# define COMPILER_LIKELY_(x) __builtin_expect(x, true)
# define COMPILER_UNLIKELY_(x) __builtin_expect(x, false)
#else
# define COMPILER_LIKELY_(x) 
# define COMPILER_UNLIKELY_(x) 
#endif


#if defined __GNUC__
# define COMPILER_PREFETCH_W_(x, loc) __builtin_prefetch(x, 1, loc)
# define COMPILER_PREFETCH_R_(x, loc) __builtin_prefetch(x, 0, loc)
// TODO: support msvc intrinsics for prefetch
// #elif defined _MSC_VER && defined _M_IX86
// #elif defined _MSC_VER && defined _M_ARM	   			
#else
# define COMPILER_PREFETCH_(x, rw, loc) 
#endif


#endif /* COMPILER_H */
