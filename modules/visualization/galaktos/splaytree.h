#ifndef SPLAYTREE_H
#define SPLAYTREE_H
#define REGULAR_NODE_TYPE 0
#define SYMBOLIC_NODE_TYPE 1

#define PERFECT_MATCH 0
#define CLOSEST_MATCH 1



inline void * splay_find(void * key, splaytree_t * t);
inline int splay_insert(void * data, void * key, splaytree_t * t);
inline int splay_insert_link(void * alias_key, void * orig_key, splaytree_t * splaytree);
inline int splay_delete(void * key, splaytree_t * splaytree);
inline int splay_size(splaytree_t * t);
inline splaytree_t * create_splaytree(int (*compare)(), void * (*copy_key)(), void (*free_key)());
inline int destroy_splaytree(splaytree_t * splaytree);
inline void splay_traverse(void (*func_ptr)(), splaytree_t * splaytree);
inline splaynode_t  * get_splaynode_of(void * key, splaytree_t * splaytree);
inline void * splay_find_above_min(void * key, splaytree_t * root);
inline void * splay_find_below_max(void * key, splaytree_t * root);
inline void * splay_find_min(splaytree_t * t);
inline void * splay_find_max(splaytree_t * t);
#endif
