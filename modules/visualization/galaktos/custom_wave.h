#ifndef CUSTOM_WAVE_H
#define CUSTOM_WAVE_H
#define CUSTOM_WAVE_DEBUG 0
#include "expr_types.h"
#include "custom_wave_types.h"
#include "preset_types.h"
#include "splaytree.h"
#include "init_cond.h"


void free_custom_wave(custom_wave_t * custom_wave);
custom_wave_t * new_custom_wave(int id);

void free_per_point_eqn(per_point_eqn_t * per_point_eqn);
per_point_eqn_t * new_per_point_eqn(int index, param_t * param,gen_expr_t * gen_expr);
void reset_per_point_eqn_array(custom_wave_t * custom_wave);
custom_wave_t * find_custom_wave(int id, preset_t * preset, int create_flag);

int add_per_point_eqn(char * name, gen_expr_t * gen_expr, custom_wave_t * custom_wave);
void evalCustomWaveInitConditions();
void evalPerPointEqns();
custom_wave_t * nextCustomWave();
void load_unspecified_init_conds(custom_wave_t * custom_wave);

static inline void eval_custom_wave_init_conds(custom_wave_t * custom_wave) {
  splay_traverse(eval_init_cond, custom_wave->init_cond_tree);
  splay_traverse(eval_init_cond, custom_wave->per_frame_init_eqn_tree);
}
#endif
