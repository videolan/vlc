#ifndef PER_FRAME_EQN_H
#define PER_FRAME_EQN_H
#define PER_FRAME_EQN_DEBUG 0

per_frame_eqn_t * new_per_frame_eqn(int index, param_t * param, struct GEN_EXPR_T * gen_expr);
void eval_per_frame_eqn(per_frame_eqn_t * per_frame_eqn);
void free_per_frame_eqn(per_frame_eqn_t * per_frame_eqn);
void eval_per_frame_init_eqn(per_frame_eqn_t * per_frame_eqn);
#endif
