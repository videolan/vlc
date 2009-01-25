#ifndef PARAM_H
#define PARAM_H
#include "preset_types.h"
#include "splaytree_types.h"
/* Debug level, zero for none */
#define PARAM_DEBUG 0

/* Used to store a number of decidable type */

/* Function prototypes */
param_t * create_param (const char * name, short int type, short int flags, void * eqn_val, void * matrix,
							value_t default_init_val, value_t upper_bound, value_t lower_bound);
param_t * create_user_param(char * name);
int init_builtin_param_db();
int init_user_param_db();
int destroy_user_param_db();
int destroy_builtin_param_db();
void set_param(param_t * param, double val);
int remove_param(param_t * param);
param_t * find_param(char * name, struct PRESET_T * preset, int flags);
void free_param(param_t * param);
int load_all_builtin_param();
int insert_param(param_t * param, splaytree_t * database);
param_t * find_builtin_param(char * name);
param_t * new_param_double(const char * name, short int flags, void * engine_val, void * matrix,
		        double upper_bound, double lower_bound, double init_val);

param_t * new_param_int(const char * name, short int flags, void * engine_val,
			int upper_bound, int lower_bound, int init_val);

param_t * new_param_bool(const char * name, short int flags, void * engine_val,
			 int upper_bound, int lower_bound, int init_val);

param_t * find_param_db(char * name, splaytree_t * database, int create_flag);

#endif
