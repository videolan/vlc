#ifndef INIT_COND_TYPES_H
#define INIT_COND_TYPES_H

#include "param_types.h"
#include "expr_types.h"

typedef struct INIT_COND_T {
  struct PARAM_T * param;
  value_t init_val;
} init_cond_t;
#endif
