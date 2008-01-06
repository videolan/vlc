/* eval.h: evaluation functions of expressions */
#ifndef EVAL_H
#define EVAL_H
#include "func_types.h"
#include "param_types.h"

#define VAL_T 1
#define PREFUN_T 3
#define TREE_T 4
#define NONE_T 0


#define CONSTANT_TERM_T 0
#define PARAM_TERM_T 1

#define INFIX_ADD 0
#define INFIX_MINUS 1
#define INFIX_MOD 2
#define INFIX_DIV 3
#define INFIX_MULT 4
#define INFIX_OR 5
#define INFIX_AND 6

//#define EVAL_DEBUG 


double eval_gen_expr(gen_expr_t * gen_expr);
inline gen_expr_t * opt_gen_expr(gen_expr_t * gen_expr, int ** param_list);

gen_expr_t * const_to_expr(double val);
gen_expr_t * param_to_expr(struct PARAM_T * param);
gen_expr_t * prefun_to_expr(double (*func_ptr)(), gen_expr_t ** expr_list, int num_args);

tree_expr_t * new_tree_expr(infix_op_t * infix_op, gen_expr_t * gen_expr, tree_expr_t * left, tree_expr_t * right);
gen_expr_t * new_gen_expr(int type, void * item);
val_expr_t * new_val_expr(int type, term_t term);

int free_gen_expr(gen_expr_t * gen_expr);
int free_prefun_expr(prefun_expr_t * prefun_expr);
int free_tree_expr(tree_expr_t * tree_expr);
int free_val_expr(val_expr_t * val_expr);

infix_op_t * new_infix_op(int type, int precedence);
int init_infix_ops();
int destroy_infix_ops();
void reset_engine_vars();

gen_expr_t * clone_gen_expr(gen_expr_t * gen_expr);
tree_expr_t * clone_tree_expr(tree_expr_t * tree_expr);
val_expr_t * clone_val_expr(val_expr_t * val_expr);
prefun_expr_t * clone_prefun_expr(prefun_expr_t * prefun_expr);



#endif
