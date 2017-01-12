#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <assert.h>
#include <stdlib.h>
#include <sched.h>
#include "thread.h"
#include "types.h"
// #include "rapl.h"

int singlecount = 0;
int stmcount = 0;
int htmcount = 0;
int totalcounts = 0;
static THREAD_LOCAL_T    global_threadId;
static long              global_numThread       = 1;
static THREAD_BARRIER_T* global_barrierPtr      = NULL;
static long*             global_threadIds       = NULL;
static THREAD_ATTR_T     global_threadAttr;
static THREAD_T*         global_threads         = NULL;
static void            (*global_funcPtr)(void*) = NULL;
static void*             global_argPtr          = NULL;
static volatile bool_t   global_doShutdown      = FALSE;
int*		thread_access		= NULL;

__attribute__((aligned(CACHE_LINE_SIZE))) pthread_mutex_t the_lock;

int global_single_lock = 0;
int singlelock_count = 0;

void abortHTM(Thread* Self) {
    _xabort(0xab);
}
intptr_t sharedReadHTM(Thread* Self, intptr_t* addr) {
    return *addr;
}
void sharedWriteHTM(Thread* Self, intptr_t* addr, intptr_t val) {
    *addr = val;
}

void abortSTM(Thread* Self) {
    STM_RESTART();
}
intptr_t sharedReadSTM(Thread* Self, intptr_t* addr) {
    return STM_HYBRID_READ(addr);
}
void sharedWriteSTM(Thread* Self, intptr_t* addr, intptr_t val) {
    STM_HYBRID_WRITE(addr, val);
}

__thread void (*abortFunPtr)(Thread* Self);
__thread intptr_t (*sharedReadFunPtr)(Thread* Self, intptr_t* addr);
__thread void (*sharedWriteFunPtr)(Thread* Self, intptr_t* addr, intptr_t val);

volatile unsigned long is_fallback = 0;


static void threadWait (void* argPtr)
{
    long threadId = *(long*)argPtr;
    
    THREAD_LOCAL_SET(global_threadId, (long)threadId);

    abortFunPtr = &abortHTM;
    sharedReadFunPtr = &sharedReadHTM;
    sharedWriteFunPtr = &sharedWriteHTM;

    cpu_set_t my_set;
    CPU_ZERO(&my_set);
    CPU_SET(threadId % 8, &my_set);
    sched_setaffinity(0, sizeof(cpu_set_t), &my_set);

    while (1) {
        THREAD_BARRIER(global_barrierPtr, threadId); /* wait for start parallel */
        if (global_doShutdown) {
            break;
        }
        global_funcPtr(global_argPtr);
        THREAD_BARRIER(global_barrierPtr, threadId); /* wait for end parallel */
        if (threadId == 0) {
            break;
        }
    }
}

void thread_startup (long numThread)
{
    long i;

    global_numThread = numThread;
    global_doShutdown = FALSE;

    /* Set up barrier */
    assert(global_barrierPtr == NULL);
    global_barrierPtr = THREAD_BARRIER_ALLOC(numThread);
    assert(global_barrierPtr);
    THREAD_BARRIER_INIT(global_barrierPtr, numThread);

    /* Set up ids */
    THREAD_LOCAL_INIT(global_threadId);
    assert(global_threadIds == NULL);
    global_threadIds = (long*)malloc(numThread * sizeof(long));
    assert(global_threadIds);
    for (i = 0; i < numThread; i++) {
        global_threadIds[i] = i;
    }

    /* Set up thread access */
    thread_access = (int*)malloc(numThread * sizeof(int));
    for (i = 0; i < numThread; i++) {
	thread_access[i] = 0; // -1 single lock, 0 idle, 1 process
    }

    /* Set up thread list */
    assert(global_threads == NULL);
    global_threads = (THREAD_T*)malloc(numThread * sizeof(THREAD_T));
    assert(global_threads);

    /* Set up pool */
    THREAD_ATTR_INIT(global_threadAttr);
    for (i = 1; i < numThread; i++) {
        THREAD_CREATE(global_threads[i],
                      global_threadAttr,
                      &threadWait,
                      &global_threadIds[i]);
    }

    /*
     * Wait for primary thread to call thread_start
     */
}


void thread_start (void (*funcPtr)(void*), void* argPtr)
{
    global_funcPtr = funcPtr;
    global_argPtr = argPtr;

    long threadId = 0; /* primary */
    threadWait((void*)&threadId);
}


void thread_shutdown ()
{
    /* Make secondary threads exit wait() */
    global_doShutdown = TRUE;
    THREAD_BARRIER(global_barrierPtr, 0);

    long numThread = global_numThread;

    long i;
    for (i = 1; i < numThread; i++) {
        THREAD_JOIN(global_threads[i]);
    }

    THREAD_BARRIER_FREE(global_barrierPtr);
    global_barrierPtr = NULL;

    free(global_threadIds);
    global_threadIds = NULL;

    free(global_threads);
    global_threads = NULL;

    global_numThread = 1;
}

barrier_t *barrier_alloc() {
    return (barrier_t *)malloc(sizeof(barrier_t));
}

void barrier_free(barrier_t *b) {
    free(b);
}

void barrier_init(barrier_t *b, int n) {
    pthread_cond_init(&b->complete, NULL);
    pthread_mutex_init(&b->mutex, NULL);
    b->count = n;
    b->crossing = 0;
}

void barrier_cross(barrier_t *b) {
    pthread_mutex_lock(&b->mutex);
    /* One more thread through */
    b->crossing++;
    /* If not all here, wait */
    if (b->crossing < b->count) {
        pthread_cond_wait(&b->complete, &b->mutex);
    } else {
        /* Reset for next time */
        b->crossing = 0;
        pthread_cond_broadcast(&b->complete);
    }
    pthread_mutex_unlock(&b->mutex);
}

void thread_barrier_wait()
{
    long threadId = thread_getId();
    THREAD_BARRIER(global_barrierPtr, threadId);
}


long thread_getId()
{
    return (long)THREAD_LOCAL_GET(global_threadId);
}


long thread_getNumThread()
{
    return global_numThread;
}
