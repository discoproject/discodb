
#ifndef __DDB_PROFILE_H__
#define __DDB_PROFILE_H__

#include <sys/time.h>
#include <stdio.h>

#ifdef DDB_PROFILE
#define DDB_TIMER_DEF struct timeval __start; struct timeval __end;
#define DDB_TIMER_START gettimeofday(&__start, NULL);
#define DDB_TIMER_END(msg)\
        gettimeofday(&__end, NULL);\
        fprintf(stderr, "PROF: %s took %2.4fms\n", msg,\
            ((__end.tv_sec * 1000000LLU + __end.tv_usec) -\
             (__start.tv_sec * 1000000LLU + __start.tv_usec)) / (double)1000.0);
#else
#define DDB_TIMER_DEF
#define DDB_TIMER_START
#define DDB_TIMER_END(x)
#endif

#endif /* __DDB_PROFILE_H__ */
