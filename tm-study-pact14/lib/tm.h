#ifndef TM_H
#define TM_H 1

#include <stdio.h>

#define MAIN(argc, argv)              int main (int argc, char** argv)
#define MAIN_RETURN(val)              return val

#define GOTO_SIM()                    /* nothing */
#define GOTO_REAL()                   /* nothing */
#define IS_IN_SIM()                   (0)

#define SIM_GET_NUM_CPU(var)          /* nothing */

#define TM_PRINTF                     printf
#define TM_PRINT0                     printf
#define TM_PRINT1                     printf
#define TM_PRINT2                     printf
#define TM_PRINT3                     printf

#define P_MEMORY_STARTUP(numThread)   /* nothing */
#define P_MEMORY_SHUTDOWN()           /* nothing */

#include <assert.h>
#include "memory.h"
#include "thread.h"
#include "types.h"

#include <immintrin.h>
#include <rtmintrin.h>
#include "stm.h"
#include "norec.h"

#define STM_SELF                        Self

#define TM_ARG                        STM_SELF,
#define TM_ARG_ALONE                  STM_SELF
#define TM_ARGDECL                    STM_THREAD_T* TM_ARG
#define TM_ARGDECL_ALONE              STM_THREAD_T* TM_ARG_ALONE
#define TM_CALLABLE                   /* nothing */

#define TM_STARTUP(numThread)     STM_STARTUP() // ; THREAD_MUTEX_INIT(the_lock);
#define TM_SHUTDOWN()             STM_SHUTDOWN()

#define TM_THREAD_ENTER()         TM_ARGDECL_ALONE = STM_NEW_THREAD(); \
                                  STM_INIT_THREAD(TM_ARG_ALONE, thread_getId()); \
                                  abortFunPtr = &abortHTM; \
                                  sharedReadFunPtr = &sharedReadHTM;  \
                                  sharedWriteFunPtr = &sharedWriteHTM;    \


#define TM_THREAD_EXIT()              /* nothing */
#define TM_BEGIN_WAIVER()
#define TM_END_WAIVER()

#define TM_BEGIN() \
{ \
	int tries = 4;	\
	/* int stmretry = 3; */ \
	int isSingle = 0; \
	/* sigjmp_buf tmbuf; \
	do { \
		sigsetjmp(tmbuf, 2); \
	} while(0); */ \
	while (1) {	\
		while (is_fallback != 0) {} \
	        if (tries > 0) { \
    			int status = _xbegin();	\
    			if (status == _XBEGIN_STARTED) { \
    				if (is_fallback != 0) { _xabort(0xab); } \
    	                        break;	\
    			} \
			tries--; \
    		} else { \
			if (isSingle == 0) {  \
	    			STM_BEGIN_WR();   \
				/* stmretry--; */ \
				/* if (stmretry == 0) { \
					printf("retry\n"); \
					siglongjmp(tmbuf, 2); \
				} */ \
        	                abortFunPtr = &abortSTM;    \
                	        sharedReadFunPtr = &sharedReadSTM;  \
                        	sharedWriteFunPtr = &sharedWriteSTM;    \
	                        break;  \
			} else { \
				__sync_add_and_fetch(&is_fallback,1); \
				pthread_mutex_lock(&global_rtm_mutex); \
	                        sharedReadFunPtr = &sharedReadHTM; \
        	                sharedWriteFunPtr = &sharedWriteHTM; \
                	        break; \
			} \
                } \
	}


#define TM_END() \
	if (isSingle == 1) { \
		pthread_mutex_unlock(&global_rtm_mutex); \
		__sync_sub_and_fetch(&is_fallback,1); \
		isSingle = 0; \
	} \
	else { \
		if (tries > 0) {	\
	        	HTM_INC_CLOCK();  \
			_xend();	\
		} else {	\
			long local_global_clock = TX_END_HYBRID_FIRST_STEP();  \
			int retriesCommit = 4; \
			while (1) {  \
				int status = _xbegin();  \
				if (status == _XBEGIN_STARTED) { \
					if (is_fallback != 0) { _xabort(0xab); } \
					long res_htm = TX_END_HYBRID_LAST_STEP(local_global_clock); \
					if (res_htm == 1) { _xabort(0xac); }   \
                               		_xend();	\
	                                TX_AFTER_FINALIZE();    \
					break;  \
				} else if (_XABORT_CODE(status) == 0xab) { \
					__sync_add_and_fetch(&is_fallback,1);    \
					int ret = HYBRID_STM_END();  \
					__sync_sub_and_fetch(&is_fallback,1);    \
					if (ret == 0) { \
						STM_RESTART(); \
					} \
					break;  \
				} else if (_XABORT_CODE(status) == 0xac) { \
					STM_RESTART(); \
				} \
			}    \
	                abortFunPtr = &abortHTM;    \
        	        sharedReadFunPtr = &sharedReadHTM;  \
                	sharedWriteFunPtr = &sharedWriteHTM;    \
		} \
	} \
};



#define TM_BEGIN_RO()                 TM_BEGIN()
#define TM_RESTART()                  (*abortFunPtr)(Self);
#define TM_EARLY_RELEASE(var)         


#define P_MALLOC(size)            malloc(size)
#define P_FREE(ptr)               free(ptr)
#define SEQ_MALLOC(size)          malloc(size)
#define SEQ_FREE(ptr)             free(ptr)

#define TM_MALLOC(size)           malloc(size)
#define TM_FREE(ptr)              /* */


#define TM_SHARED_READ(var) (*sharedReadFunPtr)(Self, (vintp*)(void*)&(var))
#define TM_SHARED_READ_P(var) IP2VP(TM_SHARED_READ(var))
#define TM_SHARED_READ_D(var) IP2D((*sharedReadFunPtr)(Self, (vintp*)DP2IPP(&(var))))

#define TM_SHARED_WRITE(var, val) (*sharedWriteFunPtr)(Self, (vintp*)(void*)&(var), (intptr_t)val)
#define TM_SHARED_WRITE_P(var, val) (*sharedWriteFunPtr)(Self, (vintp*)(void*)&(var), VP2IP(val))
#define TM_SHARED_WRITE_D(var, val) (*sharedWriteFunPtr)(Self, (vintp*)DP2IPP(&(var)), D2IP(val))

#define TM_LOCAL_WRITE(var, val)      ({var = val; var;})
#define TM_LOCAL_WRITE_P(var, val)    ({var = val; var;})
#define TM_LOCAL_WRITE_D(var, val)    ({var = val; var;})

#endif /* TM_H */
