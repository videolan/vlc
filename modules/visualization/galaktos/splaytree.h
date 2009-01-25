#ifndef SPLAYTREE_H
#define SPLAYTREE_H
#define REGULAR_NODE_TYPE 0
#define SYMBOLIC_NODE_TYPE 1

#define PERFECT_MATCH 0
#define CLOSEST_MATCH 1



void * splay_find(void * key, splaytree_t * t);
int splay_insert(void * data, void * key, splaytree_t * t);
int splay_insert_link(const void * alias_key, void * orig_key, splaytree_t * splaytree);
int splay_delete(void * key, splaytree_t * splaytree);
int splay_size(splaytree_t * t);
splaytree_t * create_splaytree(int (*compare)(), void * (*copy_key)(), void (*free_key)());
int destroy_splaytree(splaytree_t * splaytree);
void splay_traverse(void (*func_ptr)(), splaytree_t * splaytree);
splaynode_t  * get_splaynode_of(void * key, splaytree_t * splaytree);
void * splay_find_above_min(void * key, splaytree_t * root);
void * splay_find_below_max(void * key, splaytree_t * root);
void * splay_find_min(splaytree_t * t);
void * splay_find_max(splaytree_t * t);
#endif
