#ifndef SPLAYTREE_TYPES_H
#define SPLAYTREE_TYPES_H

typedef struct SPLAYNODE_T {
  int type;
  struct SPLAYNODE_T * left, * right;
  void * data;
  void * key;
} splaynode_t;

typedef struct SPLAYTREE_T {
  splaynode_t * root;
  int (*compare)();
  void * (*copy_key)();
  void (*free_key)();
} splaytree_t;
#endif
