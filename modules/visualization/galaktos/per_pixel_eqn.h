#ifndef PER_PIXEL_EQN_H
#define PER_PIXEL_EQN_H

#include "expr_types.h"
#include "preset_types.h"

#define PER_PIXEL_EQN_DEBUG 0
void evalPerPixelEqns();
int add_per_pixel_eqn(char * name, gen_expr_t * gen_expr, struct PRESET_T * preset);
void free_per_pixel_eqn(per_pixel_eqn_t * per_pixel_eqn);
per_pixel_eqn_t * new_per_pixel_eqn(int index, param_t  * param, gen_expr_t * gen_expr);
inline int resetPerPixelEqnFlags();

#endif
