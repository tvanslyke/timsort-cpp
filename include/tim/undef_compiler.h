#ifndef COMPILER_H
#define COMPILER_H


#ifdef 	COMPILER_LIKELY_ 
# undef COMPILER_LIKELY_
#endif

#ifdef 	COMPILER_UNLIKELY_ 
# undef COMPILER_UNLIKELY_
#endif

#ifdef 	COMPILER_ASSUME_ 
# undef COMPILER_ASSUME_
#endif

#ifdef 	COMPILER_UNREACHABLE_ 
# undef COMPILER_UNREACHABLE_
#endif


#endif /* COMPILER_H */
