/* Function management */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "fatal.h"

#include "func_types.h"
#include "func.h"

#include "splaytree_types.h"
#include "splaytree.h"
#include "tree_types.h"

#include "builtin_funcs.h"

/* A splay tree of builtin functions */
splaytree_t * builtin_func_tree;

/* Private function prototypes */
int compare_func(char * name, char * name2);
int insert_func(func_t * name);
void * copy_func_key(char * string);


void * copy_func_key(char * string) {
	
	char * clone_string;
	
	if ((clone_string = malloc(MAX_TOKEN_SIZE)) == NULL)
		return NULL;
	
	strncpy(clone_string, string, MAX_TOKEN_SIZE-1);
	
	return (void*)clone_string;
}	


func_t * create_func (const char * name, double (*func_ptr)(), int num_args) {

  func_t * func;
  func = (func_t*)malloc(sizeof(func_t));
 
  if (func == NULL)
    return NULL;

  
  /* Clear name space */
  memset(func->name, 0, MAX_TOKEN_SIZE);

  /* Copy given name into function structure */
  strncpy(func->name, name, MAX_TOKEN_SIZE); 

  /* Assign value pointer */
  func->func_ptr = func_ptr;
  func->num_args = num_args;
  /* Return instantiated function */
  return func;

}

/* Initialize the builtin function database.
   Should only be necessary once */
int init_builtin_func_db() {
  int retval;

  builtin_func_tree = create_splaytree(compare_string, copy_string, free_string);

  if (builtin_func_tree == NULL)
    return OUTOFMEM_ERROR;

  retval = load_all_builtin_func();
  return SUCCESS;
}


/* Destroy the builtin function database.
   Generally, do this on projectm exit */
int destroy_builtin_func_db() {

  splay_traverse(free_func, builtin_func_tree);
  destroy_splaytree(builtin_func_tree);
  return SUCCESS;

}

/* Insert a function into the database */
int insert_func(func_t * func) {

  if (func == NULL)
    return ERROR;

  splay_insert(func, func->name, builtin_func_tree);

  return SUCCESS;
}

/* Frees a function type, real complicated... */
void free_func(func_t * func) {
  free(func);
}

/* Remove a function from the database */
int remove_func(func_t * func) {

  if (func == NULL)
    return ERROR;

    splay_delete(func->name, builtin_func_tree);

  return SUCCESS;
}

/* Find a function given its name */
func_t * find_func(char * name) {

  func_t * func = NULL;

  /* First look in the builtin database */
  func = (func_t *)splay_find(name, builtin_func_tree);
	
  return func;

}

/* Compare string name with function name */
int compare_func(char * name, char * name2) {

  int cmpval;

  /* Uses string comparison function */
  cmpval = strncmp(name, name2, MAX_TOKEN_SIZE-1);
  
  return cmpval;
}

/* Loads a builtin function */
int load_builtin_func(const char * name,  double (*func_ptr)(), int num_args) {

  func_t * func; 
  int retval; 

  /* Create new function */
  func = create_func(name, func_ptr, num_args);

  if (func == NULL)
    return OUTOFMEM_ERROR;

  retval = insert_func(func);

  return retval;

}

/* Loads all builtin functions */
int load_all_builtin_func() {

  
  if (load_builtin_func("int", int_wrapper, 1) < 0)
    return ERROR;
  if (load_builtin_func("abs", abs_wrapper, 1) < 0)
    return ERROR;
  if (load_builtin_func("sin", sin_wrapper, 1) < 0)
    return ERROR;
  if (load_builtin_func("cos", cos_wrapper, 1) < 0)
    return ERROR;
  if (load_builtin_func("tan", tan_wrapper, 1) < 0)
    return ERROR;
  if (load_builtin_func("asin", asin_wrapper, 1) < 0)
    return ERROR;
  if (load_builtin_func("acos", acos_wrapper, 1) < 0)
    return ERROR;
  if (load_builtin_func("atan", atan_wrapper, 1) < 0)
    return ERROR;
  if (load_builtin_func("sqr", sqr_wrapper, 1) < 0)
    return ERROR;
  if (load_builtin_func("sqrt", sqrt_wrapper, 1) < 0)
    return ERROR;
  if (load_builtin_func("pow", pow_wrapper, 2) < 0)
    return ERROR;
  if (load_builtin_func("exp", exp_wrapper, 1) < 0)
    return ERROR;
  if (load_builtin_func("log", log_wrapper, 1) < 0)
    return ERROR;
  if (load_builtin_func("log10", log10_wrapper, 1) < 0)
    return ERROR;
  if (load_builtin_func("sign", sign_wrapper, 1) < 0)
    return ERROR;
  if (load_builtin_func("min", min_wrapper, 2) < 0)
    return ERROR;
  if (load_builtin_func("max", max_wrapper, 2) < 0)
    return ERROR;
  if (load_builtin_func("sigmoid", sigmoid_wrapper, 2) < 0)
    return ERROR;
  if (load_builtin_func("atan2", atan2_wrapper, 2) < 0)
    return ERROR;
  if (load_builtin_func("rand", rand_wrapper, 1) < 0)
    return ERROR;
  if (load_builtin_func("band", band_wrapper, 2) < 0)
    return ERROR;
  if (load_builtin_func("bor", bor_wrapper, 2) < 0)
    return ERROR;
  if (load_builtin_func("bnot", bnot_wrapper, 1) < 0)
    return ERROR;
  if (load_builtin_func("if", if_wrapper, 3) < 0)
    return ERROR;
  if (load_builtin_func("equal", equal_wrapper, 2) < 0)
    return ERROR;
  if (load_builtin_func("above", above_wrapper, 2) < 0)
    return ERROR;
  if (load_builtin_func("below", below_wrapper, 2) < 0)
    return ERROR;
  if (load_builtin_func("nchoosek", nchoosek_wrapper, 2) < 0)
    return ERROR;
  if (load_builtin_func("fact", fact_wrapper, 1) < 0)
    return ERROR;


  return SUCCESS;
}
