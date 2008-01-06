#ifndef INIT_COND_H
#define INIT_COND_H
#define INIT_COND_DEBUG 0
#include "param_types.h"
#include "init_cond_types.h"
#include "splaytree_types.h"

void eval_init_cond(init_cond_t * init_cond);
init_cond_t * new_init_cond(param_t * param, value_t init_val);
void free_init_cond(init_cond_t * init_cond);
char * create_init_cond_string_buffer(splaytree_t * init_cond_tree);
#endif
