#ifndef EXPR_TYPES_H
#define EXPR_TYPES_H
#include "param_types.h"

#define CONST_STACK_ELEMENT 0
#define EXPR_STACK_ELEMENT 1

/* General Expression Type */
typedef struct GEN_EXPR_T {
  int type;
  void * item;
} gen_expr_t;

typedef union TERM_T {
  double constant; /* static variable */
  struct PARAM_T * param; /* pointer to a changing variable */
} term_t;

/* Value expression, contains a term union */
typedef struct VAL_EXPR_T {
  int type;
  term_t term;
} val_expr_t;

/* Infix Operator Function */
typedef struct INFIX_OP_T {
  int type;
  int precedence;  
} infix_op_t;

/* A binary expression tree ordered by operator precedence */
typedef struct TREE_EXPR_T {
  infix_op_t * infix_op; /* null if leaf */
  gen_expr_t * gen_expr;
  struct TREE_EXPR_T * left, * right;
} tree_expr_t;

/* A function expression in prefix form */
typedef struct PREFUN_EXPR_T {
  double (*func_ptr)();
  int num_args;
  gen_expr_t ** expr_list;
} prefun_expr_t;




#endif
