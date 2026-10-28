#pragma once

// Real mutexes (SwitchWakerHD patch 1, see PATCHES.md). uam's own header was a single-threaded stub
// (every lock a no-op), which let two threads build Mesa's built-in GLSL function library at the
// same time and damage the ralloc heap. uam stays NOT reentrant (static gl_context in
// glsl_frontend.cpp): this only keeps Mesa's global tables consistent; compile on one thread.
#include <pthread.h>

typedef pthread_mutex_t mtx_t;
enum { mtx_plain = 0, mtx_recursive = 1, mtx_timed = 2 };

static inline int mtx_init(mtx_t *m, int type)
{
	pthread_mutexattr_t a;
	pthread_mutexattr_init(&a);
	pthread_mutexattr_settype(&a, (type & mtx_recursive) ? PTHREAD_MUTEX_RECURSIVE : PTHREAD_MUTEX_NORMAL);
	int r = pthread_mutex_init(m, &a);
	pthread_mutexattr_destroy(&a);
	return r;
}
static inline void mtx_destroy(mtx_t *m) { pthread_mutex_destroy(m); }
static inline int mtx_lock(mtx_t *m) { return pthread_mutex_lock(m); }
static inline int mtx_unlock(mtx_t *m) { return pthread_mutex_unlock(m); }

#define _MTX_INITIALIZER_NP PTHREAD_MUTEX_INITIALIZER
