#ifndef PER_FRAME_EQN_TYPES_H
#define PER_FRAME_EQN_TYPES_H
#include "param_types.h"
#include "expr_types.h"

typedef struct PER_FRAME_EQN_T {
  int index;
  struct PARAM_T * param; /* parameter to be assigned a value */
  struct GEN_EXPR_T * gen_expr;   /* expression that paremeter is equal to */
} per_frame_eqn_t;
#endif
