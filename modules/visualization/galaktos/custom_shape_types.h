#ifndef CUSTOM_SHAPE_TYPES_H
#define CUSTOM_SHAPE_TYPES_H
#include "common.h"
#include "splaytree_types.h"
#include "expr_types.h"

typedef struct CUSTOM_SHAPE_T {

  /* Numerical id */
  int id;
  int per_frame_count;

  /* Parameter tree associated with this custom shape */
  splaytree_t * param_tree;


  /* Engine variables */
  int sides;
  int thickOutline;
  int enabled;
  int additive;
  int textured;

  double tex_zoom;
  double tex_ang;
  
  double x; /* x position for per point equations */
  double y; /* y position for per point equations */
  double rad;
  double ang;

  double r; /* red color value */
  double g; /* green color value */
  double b; /* blue color value */
  double a; /* alpha color value */
 
  double r2; /* red color value */
  double g2; /* green color value */
  double b2; /* blue color value */
  double a2; /* alpha color value */

  double border_r; /* red color value */
  double border_g; /* green color value */
  double border_b; /* blue color value */
  double border_a; /* alpha color value */

  /* stupid t variables */
  double t1;
  double t2;
  double t3;
  double t4;
  double t5;
  double t6;
  double t7;
  double t8;

  /* Data structure to hold per frame  / per frame init equations */
  splaytree_t * init_cond_tree;
  splaytree_t * per_frame_eqn_tree;
  splaytree_t * per_frame_init_eqn_tree;

  /* Denotes the index of the last character for each string buffer */
  int per_frame_eqn_string_index;
  int per_frame_init_eqn_string_index;

  /* String buffers for per frame / per frame init equations */
  char per_frame_eqn_string_buffer[STRING_BUFFER_SIZE];
  char per_frame_init_eqn_string_buffer[STRING_BUFFER_SIZE];
  /* Per point equation array */
  
  
} custom_shape_t;


#endif
