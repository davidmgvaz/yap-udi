#ifndef PL_THREAD_H

#define PL_THREAD_H 1

#if defined(THREADS) && !defined(O_PLMT)
#define O_PLMT 1
#endif

#if defined(O_PLMT) // && defined(PL_KERNEL)
/* Support PL_LOCK in the interface */
#if THREADS

#include <pthread.h>

typedef pthread_mutex_t simpleMutex;

#define simpleMutexInit(p)	pthread_mutex_init(p, NULL)
#define simpleMutexDelete(p)	pthread_mutex_destroy(p)
#define simpleMutexLock(p)	pthread_mutex_lock(p)
#define simpleMutexUnlock(p)	pthread_mutex_unlock(p)

typedef pthread_mutex_t recursiveMutex;

#define NEED_RECURSIVE_MUTEX_INIT 1
extern int recursiveMutexInit(recursiveMutex *m);
#define recursiveMutexDelete(p)  pthread_mutex_destroy(p)
#define recursiveMutexLock(p)    pthread_mutex_lock(p)
#define recursiveMutexTryLock(p) pthread_mutex_trylock(p)
#define recursiveMutexUnlock(p)  pthread_mutex_unlock(p)

#define IF_MT(id, g) if ( id == L_THREAD || GD->thread.enabled ) g

typedef struct counting_mutex
{ simpleMutex mutex;                    /* mutex itself */
  const char *name;                     /* name of the mutex */
  long count;                           /* # times locked */
  long unlocked;                        /* # times unlocked */
#ifdef O_CONTENTION_STATISTICS
  long collisions;                      /* # contentions */
#endif
  struct counting_mutex *next;          /* next of allocated chain */
} counting_mutex;

extern counting_mutex  *allocSimpleMutex(const char *name);
extern void             freeSimpleMutex(counting_mutex *m);

extern counting_mutex _PL_mutexes[];	/* Prolog mutexes */

#define L_MISC		0
#define L_ALLOC		1
#define L_ATOM		2
#define L_FLAG	        3
#define L_FUNCTOR	4
#define L_RECORD	5
#define L_THREAD	6
#define L_PREDICATE	7
#define L_MODULE	8
#define L_TABLE		9
#define L_BREAK	       10
#define L_FILE	       11
#define L_PLFLAG       12
#define L_OP	       13
#define L_INIT	       14
#define L_TERM	       15
#define L_GC	       16
#define L_AGC	       17
#define L_FOREIGN      18
#define L_OS	       19

#ifdef O_CONTENTION_STATISTICS
#define countingMutexLock(cm) \
	do \
	{ if ( pthread_mutex_trylock(&(cm)->mutex) == EBUSY ) \
	  { (cm)->collisions++; \
	    pthread_mutex_lock(&(cm)->mutex); \
	  } \
	  (cm)->count++; \
	} while(0)
#else
#define countingMutexLock(cm) \
	do \
	{ simpleMutexLock(&(cm)->mutex); \
	  (cm)->count++; \
	} while(0)
#endif
#define countingMutexUnlock(cm) \
	do \
	{ (cm)->unlocked++; \
	  assert((cm)->unlocked <= (cm)->count); \
	  simpleMutexUnlock(&(cm)->mutex); \
	} while(0)

#define PL_LOCK(id)   IF_MT(id, countingMutexLock(&_PL_mutexes[id]))
#define PL_UNLOCK(id) IF_MT(id, countingMutexUnlock(&_PL_mutexes[id]))

#define IOLOCK  recursiveMutex

#endif

#else
#define PL_LOCK(X)		
#define PL_UNLOCK(X)

typedef void *		IOLOCK;		
#endif

#endif
