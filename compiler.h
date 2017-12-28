#ifndef COMPILER_H
#define COMPILER_H



#if defined  _MSC_VER		   			
	#define COMPILER_UNREACHABLE_ __assume(0);   		
#elif defined __GNUC__ || defined __clang__ 			
	#define COMPILER_UNREACHABLE_ __builtin_unreachable();	
#else							 
	#define COMPILER_UNREACHABLE_  			
#endif 						
	

#define GCC_ASSUME__(x) do{ if(not (x)) {__builtin_unreachable();} } while(false);



#if defined  _MSC_VER || defined __INTEL_COMPILER		
	#define COMPILER_ASSUME_(x) __assume(x);   		 			

#elif defined __clang__
	#if __has_builtin(__builtin_assume)
		#define COMPILER_ASSUME_(x) __builtin_assume(x)
	#else
		#define COMPILER_ASSUME_(x) GCC_ASSUME__(x)
	#endif	

#elif defined __GNUC__
	#define COMPILER_ASSUME_(x) GCC_ASSUME__(x)					

#else								
	#define COMPILER_ASSUME_(x) 

#endif 


#if defined __GNUC__
	#define COMPILER_LIKELY_(x) __builtin_expect(x, true)
	#define COMPILER_UNLIKELY_(x) __builtin_expect(x, false)

#else
	#define COMPILER_LIKELY_(x) 
	#define COMPILER_UNLIKELY_(x) 

#endif





#endif /* COMPILER_H */
