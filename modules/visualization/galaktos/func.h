#ifndef FUNC_H
#define FUNC_H

/* Public Prototypes */
func_t * create_func (const char * name, double (*func_ptr)(), int num_args);
int remove_func(func_t * func);
func_t * find_func(char * name);
int init_builtin_func_db();
int destroy_builtin_func_db();
int load_all_builtin_func();
int load_builtin_func(const char * name, double (*func_ptr)(), int num_args);
void free_func(func_t * func);

#endif
