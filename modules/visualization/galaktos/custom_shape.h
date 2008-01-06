#ifndef CUSTOM_SHAPE_H
#define CUSTOM_SHAPE_H
#define CUSTOM_SHAPE_DEBUG 0
#include "expr_types.h"
#include "custom_shape_types.h"
#include "preset_types.h"

void free_custom_shape(custom_shape_t * custom_shape);
custom_shape_t * new_custom_shape(int id);
custom_shape_t * find_custom_shape(int id, preset_t * preset, int create_flag);
void load_unspecified_init_conds_shape(custom_shape_t * custom_shape);
void evalCustomShapeInitConditions();
custom_shape_t * nextCustomShape();
#endif
