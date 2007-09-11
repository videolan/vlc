#ifndef FUNC_TYPES_H
#define FUNC_TYPES_H
#include "common.h"


/* Function Type */
typedef struct FUNC_T {
  char name[MAX_TOKEN_SIZE];  
  double (*func_ptr)();
  int num_args;
} func_t;

#endif
