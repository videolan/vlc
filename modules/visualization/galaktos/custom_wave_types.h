#ifndef CUSTOM_WAVE_TYPES_H
#define CUSTOM_WAVE_TYPES_H
#include "common.h"
#include "splaytree_types.h"
#include "expr_types.h"

#define X_POINT_OP 0
#define Y_POINT_OP 1
#define R_POINT_OP 2
#define G_POINT_OP 3
#define B_POINT_OP 4
#define A_POINT_OP 5
#define NUM_POINT_OPS 6

typedef struct PER_POINT_EQN_T {
  int index;
  param_t * param;
  gen_expr_t * gen_expr;	
} per_point_eqn_t;

typedef struct CUSTOM_WAVE_T {

  /* Numerical id */
  int id;
  int per_frame_count;

  /* Parameter tree associated with this custom wave */
  splaytree_t * param_tree;


  /* Engine variables */

  double x; /* x position for per point equations */
  double y; /* y position for per point equations */
  double r; /* red color value */
  double g; /* green color value */
  double b; /* blue color value */
  double a; /* alpha color value */
  double * x_mesh;
  double * y_mesh;
  double * r_mesh;
  double * b_mesh;
  double * g_mesh;
  double * a_mesh;
  double * value1;
  double * value2;
  double * sample_mesh;

  int enabled; /* if nonzero then wave is visible, hidden otherwise */
  int samples; /* number of samples associated with this wave form. Usually powers of 2 */
  double sample;
  int bSpectrum; /* spectrum data or pcm data */
  int bUseDots; /* draw wave as dots or lines */
  int bDrawThick; /* draw thicker lines */
  int bAdditive; /* add color values together */

  double scaling; /* scale factor of waveform */
  double smoothing; /* smooth factor of waveform */
  int sep;  /* no idea what this is yet... */

  /* stupid t variables */
  double t1;
  double t2;
  double t3;
  double t4;
  double t5;
  double t6;
  double t7;
  double t8;
  double v1,v2;
  /* Data structure to hold per frame and per point equations */
  splaytree_t * init_cond_tree;
  splaytree_t * per_frame_eqn_tree;
  splaytree_t * per_point_eqn_tree;
  splaytree_t * per_frame_init_eqn_tree;

  /* Denotes the index of the last character for each string buffer */
  int per_point_eqn_string_index;
  int per_frame_eqn_string_index;
  int per_frame_init_eqn_string_index;

  /* String buffers for per point and per frame equations */
  char per_point_eqn_string_buffer[STRING_BUFFER_SIZE];
  char per_frame_eqn_string_buffer[STRING_BUFFER_SIZE];
  char per_frame_init_eqn_string_buffer[STRING_BUFFER_SIZE];
  /* Per point equation array */
  gen_expr_t * per_point_eqn_array[NUM_POINT_OPS];
  
} custom_wave_t;


#endif
