#ifndef PER_PIXEL_EQN_TYPES_H
#define PER_PIXEL_EQN_TYPES_H
/* This is sort of ugly, but it is also the fastest way to access the per pixel equations */
#include "common.h"
#include "expr_types.h"

typedef struct PER_PIXEL_EQN_T {
  int index; /* used for splay tree ordering. */
  int flags; /* primarily to specify if this variable is user-defined */
  param_t * param; 
  gen_expr_t * gen_expr;	
} per_pixel_eqn_t;


#define ZOOM_OP 0
#define ZOOMEXP_OP 1
#define ROT_OP 2
#define CX_OP 3
#define CY_OP 4
#define SX_OP 5
#define SY_OP  6
#define DX_OP 7
#define DY_OP 8
#define WARP_OP 9
#define NUM_OPS 10 /* obviously, this number is dependent on the number of existing per pixel operations */
#endif
