## lock-obj-pub.posix.h
## File created by Rémi Denis-Courmont - DO EDIT
## To be included by mkheader into gpg-error.h

#include <pthread.h>

typedef struct
{
  long vers;
  union {
    pthread_mutex_t mtx;
    /* In theory, pointers could have a higher alignment than pthread_mutex_t,
     * so keep in the union to match _gpgrt_lock_t. */
    long *alignme;
  } u;
} gpgrt_lock_t;

#define GPGRT_LOCK_INITIALIZER { 1, { PTHREAD_MUTEX_INITIALIZER } }
