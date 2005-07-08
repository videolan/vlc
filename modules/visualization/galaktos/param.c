/*****************************************************************************
 * param.c:
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN (Centrale Réseaux) and its contributors
 * $Id$
 *
 * Authors: Cyril Deguet <asmax@videolan.org>
 *          code from projectM http://xmms-projectm.sourceforge.net
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/



/* Basic Parameter Functions */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "fatal.h"
#include "common.h"

#include "splaytree_types.h"
#include "splaytree.h"
#include "tree_types.h"

#include "param_types.h"
#include "param.h"

#include "expr_types.h"
#include "eval.h"

#include "engine_vars.h" 

void reset_param(param_t * param);

int is_valid_param_string(char * string); /* true if string is valid variable or function name */


/* A splay tree of builtin parameters */
splaytree_t * builtin_param_tree = NULL;

int insert_param_alt_name(param_t * param, char * alt_name);

int insert_builtin_param(param_t * param);

/* Private function prototypes */
int compare_param(char * name, char * name2);

int load_builtin_param_double(char * name, void * engine_val, void * matrix, short int flags, 
								double init_val, double upper_bound, double lower_bound, char * alt_name);

int load_builtin_param_int(char * name, void * engine_val, short int flags, 
								int init_val, int upper_bound, int lower_bound, char * alt_name);
								
int load_builtin_param_bool(char * name, void * engine_val, short int flags, 
								int init_val, char * alt_name);
								
								
								
param_t * create_param (char * name, short int type, short int flags, void * engine_val, void * matrix,
							value_t default_init_val, value_t upper_bound, value_t lower_bound) {

  param_t * param = NULL;

  param = (param_t*)malloc(sizeof(param_t));
 
  if (param == NULL) {
	printf("create_param: out of memory!!!\n");
	return NULL;
  }
  
  /* Clear name space, think the strncpy statement makes this redundant */
  //memset(param->name, 0, MAX_TOKEN_SIZE);

  /* Copy given name into parameter structure */
  strncpy(param->name, name, MAX_TOKEN_SIZE-1); 
  
  /* Assign other entries in a constructor like fashion */
  param->type = type;
  param->flags = flags;
  param->matrix_flag = 0;
  param->matrix = matrix;
  param->engine_val = engine_val;
  param->default_init_val = default_init_val;
  //*param->init_val = default_init_val;
  param->upper_bound = upper_bound;
  param->lower_bound = lower_bound;
  
  /* Return instantiated parameter */
  return param;

}

/* Creates a user defined parameter */
param_t * create_user_param(char * name) {

  param_t * param;
  value_t iv;
  value_t ub;
  value_t lb;
  double * engine_val;
  
  /* Set initial values to default */
  iv.double_val = DEFAULT_DOUBLE_IV;
  ub.double_val = DEFAULT_DOUBLE_UB;
  lb.double_val = DEFAULT_DOUBLE_LB;

  /* Argument checks */
  if (name == NULL)
    return NULL;

  /* Allocate space for the engine variable */
  if ((engine_val = (double*)malloc(sizeof(double))) == NULL)
    return NULL;

  (*engine_val) = iv.double_val; /* set some default init value */
  
  /* Create the new user parameter */
  if ((param = create_param(name, P_TYPE_DOUBLE, P_FLAG_USERDEF, engine_val, NULL, iv, ub, lb)) == NULL) {
    free(engine_val);
    return NULL;
  }
  if (PARAM_DEBUG) printf("create_param: \"%s\" initialized\n", param->name);
  /* Return the instantiated parameter */
  return param;
}

/* Initialize the builtin parameter database.
   Should only be necessary once */
int init_builtin_param_db() {
	
  /* Create the builtin parameter splay tree (go Sleator...) */
  if ((builtin_param_tree = create_splaytree(compare_string, copy_string, free_string)) == NULL) {
	  if (PARAM_DEBUG) printf("init_builtin_param_db: failed to initialize database (FATAL)\n");  
	  return OUTOFMEM_ERROR;
  } 

  if (PARAM_DEBUG) {
	  printf("init_builtin_param: loading database...");
	  fflush(stdout);
  }
  
  /* Loads all builtin parameters into the database */
  if (load_all_builtin_param() < 0) {
	if (PARAM_DEBUG) printf("failed loading builtin parameters (FATAL)\n");
    return ERROR;
  }
  
  if (PARAM_DEBUG) printf("success!\n");
	  
  /* Finished, no errors */
  return SUCCESS;
}

/* Destroy the builtin parameter database.
   Generally, do this on projectm exit */
int destroy_builtin_param_db() {
  
  splay_traverse(free_param, builtin_param_tree);
  destroy_splaytree(builtin_param_tree);
  builtin_param_tree = NULL;
  return SUCCESS;	

}


/* Insert a parameter into the database with an alternate name */
int insert_param_alt_name(param_t * param, char * alt_name) {
  
  if (param == NULL)
    return ERROR;
  if (alt_name == NULL)
	  return ERROR;
  	
  splay_insert_link(alt_name, param->name, builtin_param_tree);

  return SUCCESS;
}


param_t * find_builtin_param(char * name) {

  /* Null argument checks */
  if (name == NULL)
	  return NULL;
    
  return splay_find(name, builtin_param_tree);

}

/* Find a parameter given its name, will create one if not found */
param_t * find_param(char * name, preset_t * preset, int flags) {

  param_t * param = NULL;

  /* Null argument checks */
  if (name == NULL)
	  return NULL;
  if (preset == NULL)
	  return NULL;
  
  /* First look in the builtin database */
  param = (param_t *)splay_find(name, builtin_param_tree);

  /* If the search failed, check the user database */
  if (param == NULL) {
    param = (param_t*)splay_find(name, preset->user_param_tree);
  }
  /* If it doesn't exist in the user (or builtin) database and 
  	  create_flag is set, then make it and insert into the database 
  */
  
  if ((param == NULL) && (flags & P_CREATE)) {
	
	/* Check if string is valid */
    if (!is_valid_param_string(name)) {
      if (PARAM_DEBUG) printf("find_param: invalid parameter name:\"%s\"\n", name);
      return NULL;
    }
    /* Now, create the user defined parameter given the passed name */
    if ((param = create_user_param(name)) == NULL) {
      if (PARAM_DEBUG) printf("find_param: failed to create a new user parameter!\n");
      return NULL;
    }
    /* Finally, insert the new parameter into this preset's proper splaytree */
    if (splay_insert(param, param->name, preset->user_param_tree) < 0) {
      if (PARAM_DEBUG) printf("PARAM \"%s\" already exists in user parameter tree!\n", param->name);
      free_param(param);
      return NULL;
    }	 
    
  }	  
  
  /* Return the found (or created) parameter. Note that if P_CREATE is not set, this could be null */
  return param;
  
}

/* Compare string name with parameter name */
int compare_param(char * name, char * name2) {

  int cmpval;
  printf("am i used\n");
  /* Uses string comparison function */
  cmpval = strncmp(name, name2, MAX_TOKEN_SIZE-1);
  
  return cmpval;
}

/* Loads all builtin parameters, limits are also defined here */
int load_all_builtin_param() {

  load_builtin_param_double("fRating", (void*)&fRating, NULL, P_FLAG_NONE, 0.0 , 5.0, 0.0, NULL);
  load_builtin_param_double("fWaveScale", (void*)&fWaveScale, NULL, P_FLAG_NONE, 0.0, MAX_DOUBLE_SIZE, -MAX_DOUBLE_SIZE, NULL);
  load_builtin_param_double("gamma", (void*)&fGammaAdj, NULL, P_FLAG_NONE, 0.0, MAX_DOUBLE_SIZE, 0, "fGammaAdj");
  load_builtin_param_double("echo_zoom", (void*)&fVideoEchoZoom, NULL, P_FLAG_NONE, 0.0, MAX_DOUBLE_SIZE, 0, "fVideoEchoZoom");
  load_builtin_param_double("echo_alpha", (void*)&fVideoEchoAlpha, NULL, P_FLAG_NONE, 0.0, MAX_DOUBLE_SIZE, 0, "fVideoEchoAlpha");
  load_builtin_param_double("wave_a", (void*)&fWaveAlpha, NULL, P_FLAG_NONE, 0.0, 1.0, 0, "fWaveAlpha");
  load_builtin_param_double("fWaveSmoothing", (void*)&fWaveSmoothing, NULL, P_FLAG_NONE, 0.0, 1.0, -1.0, NULL);  
  load_builtin_param_double("fModWaveAlphaStart", (void*)&fModWaveAlphaStart, NULL, P_FLAG_NONE, 0.0, 1.0, -1.0, NULL);
  load_builtin_param_double("fModWaveAlphaEnd", (void*)&fModWaveAlphaEnd, NULL, P_FLAG_NONE, 0.0, 1.0, -1.0, NULL);
  load_builtin_param_double("fWarpAnimSpeed",  (void*)&fWarpAnimSpeed, NULL, P_FLAG_NONE, 0.0, 1.0, -1.0, NULL);
  //  load_builtin_param_double("warp", (void*)&warp, warp_mesh, P_FLAG_NONE, 0.0, MAX_DOUBLE_SIZE, 0, NULL);
	
  load_builtin_param_double("fShader", (void*)&fShader, NULL, P_FLAG_NONE, 0.0, 1.0, -1.0, NULL);
  load_builtin_param_double("decay", (void*)&decay, NULL, P_FLAG_NONE, 0.0, 1.0, 0, "fDecay");

  load_builtin_param_int("echo_orient", (void*)&nVideoEchoOrientation, P_FLAG_NONE, 0, 3, 0, "nVideoEchoOrientation");
  load_builtin_param_int("wave_mode", (void*)&nWaveMode, P_FLAG_NONE, 0, 7, 0, "nWaveMode");
  
  load_builtin_param_bool("wave_additive", (void*)&bAdditiveWaves, P_FLAG_NONE, FALSE, "bAdditiveWaves");
  load_builtin_param_bool("bModWaveAlphaByVolume", (void*)&bModWaveAlphaByVolume, P_FLAG_NONE, FALSE, NULL);
  load_builtin_param_bool("wave_brighten", (void*)&bMaximizeWaveColor, P_FLAG_NONE, FALSE, "bMaximizeWaveColor");
  load_builtin_param_bool("wrap", (void*)&bTexWrap, P_FLAG_NONE, FALSE, "bTexWrap");
  load_builtin_param_bool("darken_center", (void*)&bDarkenCenter, P_FLAG_NONE, FALSE, "bDarkenCenter");
  load_builtin_param_bool("bRedBlueStereo", (void*)&bRedBlueStereo, P_FLAG_NONE, FALSE, NULL);
  load_builtin_param_bool("brighten", (void*)&bBrighten, P_FLAG_NONE, FALSE, "bBrighten");
  load_builtin_param_bool("darken", (void*)&bDarken, P_FLAG_NONE, FALSE, "bDarken");
  load_builtin_param_bool("solarize", (void*)&bSolarize, P_FLAG_NONE, FALSE, "bSolarize");
  load_builtin_param_bool("invert", (void*)&bInvert, P_FLAG_NONE, FALSE, "bInvert");
  load_builtin_param_bool("bMotionVectorsOn", (void*)&bMotionVectorsOn, P_FLAG_NONE, FALSE, NULL);
  load_builtin_param_bool("wave_dots", (void*)&bWaveDots, P_FLAG_NONE, FALSE, "bWaveDots");
  load_builtin_param_bool("wave_thick", (void*)&bWaveThick, P_FLAG_NONE, FALSE, "bWaveThick");
 
  
  
  load_builtin_param_double("zoom", (void*)&zoom, zoom_mesh,  P_FLAG_PER_PIXEL |P_FLAG_DONT_FREE_MATRIX, 0.0, MAX_DOUBLE_SIZE, 0, NULL);
  load_builtin_param_double("rot", (void*)&rot, rot_mesh,  P_FLAG_PER_PIXEL |P_FLAG_DONT_FREE_MATRIX, 0.0, MAX_DOUBLE_SIZE, MIN_DOUBLE_SIZE, NULL);
  load_builtin_param_double("zoomexp", (void*)&zoomexp, zoomexp_mesh,  P_FLAG_PER_PIXEL |P_FLAG_NONE, 0.0, MAX_DOUBLE_SIZE, 0, "fZoomExponent");
 
  load_builtin_param_double("cx", (void*)&cx, cx_mesh, P_FLAG_PER_PIXEL | P_FLAG_DONT_FREE_MATRIX, 0.0, 1.0, 0, NULL);
  load_builtin_param_double("cy", (void*)&cy, cy_mesh, P_FLAG_PER_PIXEL | P_FLAG_DONT_FREE_MATRIX, 0.0, 1.0, 0, NULL);
  load_builtin_param_double("dx", (void*)&dx, dx_mesh,  P_FLAG_PER_PIXEL | P_FLAG_DONT_FREE_MATRIX, 0.0, MAX_DOUBLE_SIZE, MIN_DOUBLE_SIZE, NULL);
  load_builtin_param_double("dy", (void*)&dy, dy_mesh,  P_FLAG_PER_PIXEL |P_FLAG_DONT_FREE_MATRIX, 0.0, MAX_DOUBLE_SIZE, MIN_DOUBLE_SIZE, NULL);
  load_builtin_param_double("sx", (void*)&sx, sx_mesh,  P_FLAG_PER_PIXEL |P_FLAG_DONT_FREE_MATRIX, 0.0, MAX_DOUBLE_SIZE, 0, NULL);
  load_builtin_param_double("sy", (void*)&sy, sy_mesh,  P_FLAG_PER_PIXEL |P_FLAG_DONT_FREE_MATRIX, 0.0, MAX_DOUBLE_SIZE, 0, NULL);

  load_builtin_param_double("wave_r", (void*)&wave_r, NULL, P_FLAG_NONE, 0.0, 1.0, 0.0, NULL);
  load_builtin_param_double("wave_g", (void*)&wave_g, NULL, P_FLAG_NONE, 0.0, 1.0, 0.0, NULL);
  load_builtin_param_double("wave_b", (void*)&wave_b, NULL, P_FLAG_NONE, 0.0, 1.0, 0.0, NULL);
  load_builtin_param_double("wave_x", (void*)&wave_x, NULL, P_FLAG_NONE, 0.0, 1.0, 0.0, NULL);
  load_builtin_param_double("wave_y", (void*)&wave_y, NULL, P_FLAG_NONE, 0.0, 1.0, 0.0, NULL);
  load_builtin_param_double("wave_mystery", (void*)&wave_mystery, NULL, P_FLAG_NONE, 0.0, 1.0, -1.0, "fWaveParam");
  
  load_builtin_param_double("ob_size", (void*)&ob_size, NULL, P_FLAG_NONE, 0.0, 0.5, 0, NULL);
  load_builtin_param_double("ob_r", (void*)&ob_r, NULL, P_FLAG_NONE, 0.0, 1.0, 0.0, NULL);
  load_builtin_param_double("ob_g", (void*)&ob_g, NULL, P_FLAG_NONE, 0.0, 1.0, 0.0, NULL);
  load_builtin_param_double("ob_b", (void*)&ob_b, NULL, P_FLAG_NONE, 0.0, 1.0, 0.0, NULL);
  load_builtin_param_double("ob_a", (void*)&ob_a, NULL, P_FLAG_NONE, 0.0, 1.0, 0.0, NULL);

  load_builtin_param_double("ib_size", (void*)&ib_size,  NULL,P_FLAG_NONE, 0.0, .5, 0.0, NULL);
  load_builtin_param_double("ib_r", (void*)&ib_r,  NULL,P_FLAG_NONE, 0.0, 1.0, 0.0, NULL);
  load_builtin_param_double("ib_g", (void*)&ib_g,  NULL,P_FLAG_NONE, 0.0, 1.0, 0.0, NULL);
  load_builtin_param_double("ib_b", (void*)&ib_b,  NULL,P_FLAG_NONE, 0.0, 1.0, 0.0, NULL);
  load_builtin_param_double("ib_a", (void*)&ib_a,  NULL,P_FLAG_NONE, 0.0, 1.0, 0.0, NULL);

  load_builtin_param_double("mv_r", (void*)&mv_r,  NULL,P_FLAG_NONE, 0.0, 1.0, 0.0, NULL);
  load_builtin_param_double("mv_g", (void*)&mv_g,  NULL,P_FLAG_NONE, 0.0, 1.0, 0.0, NULL);
  load_builtin_param_double("mv_b", (void*)&mv_b,  NULL,P_FLAG_NONE, 0.0, 1.0, 0.0, NULL);
  load_builtin_param_double("mv_x", (void*)&mv_x,  NULL,P_FLAG_NONE, 0.0, 64.0, 0.0, "nMotionVectorsX");
  load_builtin_param_double("mv_y", (void*)&mv_y,  NULL,P_FLAG_NONE, 0.0, 48.0, 0.0, "nMotionVectorsY");
  load_builtin_param_double("mv_l", (void*)&mv_l,  NULL,P_FLAG_NONE, 0.0, 5.0, 0.0, NULL);
  load_builtin_param_double("mv_dy", (void*)&mv_dy, NULL, P_FLAG_NONE, 0.0, 1.0, -1.0, NULL);
  load_builtin_param_double("mv_dx", (void*)&mv_dx,  NULL,P_FLAG_NONE, 0.0, 1.0, -1.0, NULL);
  load_builtin_param_double("mv_a", (void*)&mv_a,  NULL,P_FLAG_NONE, 0.0, 1.0, 0.0, NULL);

  load_builtin_param_double("time", (void*)&Time,  NULL,P_FLAG_READONLY, 0.0, MAX_DOUBLE_SIZE, 0.0, NULL);        
  load_builtin_param_double("bass", (void*)&bass,  NULL,P_FLAG_READONLY, 0.0, MAX_DOUBLE_SIZE, 0.0, NULL);
  load_builtin_param_double("mid", (void*)&mid,  NULL,P_FLAG_READONLY, 0.0, MAX_DOUBLE_SIZE, 0, NULL);      
  load_builtin_param_double("bass_att", (void*)&bass_att,  NULL,P_FLAG_READONLY, 0.0, MAX_DOUBLE_SIZE, 0, NULL);
  load_builtin_param_double("mid_att", (void*)&mid_att,  NULL,P_FLAG_READONLY, 0.0, MAX_DOUBLE_SIZE, 0, NULL);
  load_builtin_param_double("treb_att", (void*)&treb_att,  NULL,P_FLAG_READONLY, 0.0, MAX_DOUBLE_SIZE, 0, NULL);
  load_builtin_param_int("frame", (void*)&frame, P_FLAG_READONLY, 0, MAX_INT_SIZE, 0, NULL);
  load_builtin_param_double("progress", (void*)&progress,  NULL,P_FLAG_READONLY, 0.0, 1, 0, NULL);
  load_builtin_param_int("fps", (void*)&fps, P_FLAG_NONE, 15, MAX_INT_SIZE, 0, NULL);



  load_builtin_param_double("x", (void*)&x_per_pixel, x_mesh,  P_FLAG_PER_PIXEL |P_FLAG_ALWAYS_MATRIX | P_FLAG_READONLY | P_FLAG_DONT_FREE_MATRIX, 
			    0, MAX_DOUBLE_SIZE, -MAX_DOUBLE_SIZE, NULL);
  load_builtin_param_double("y", (void*)&y_per_pixel, y_mesh,  P_FLAG_PER_PIXEL |P_FLAG_ALWAYS_MATRIX |P_FLAG_READONLY | P_FLAG_DONT_FREE_MATRIX, 
			    0, MAX_DOUBLE_SIZE, -MAX_DOUBLE_SIZE, NULL);
  load_builtin_param_double("ang", (void*)&ang_per_pixel, theta_mesh,  P_FLAG_PER_PIXEL |P_FLAG_ALWAYS_MATRIX | P_FLAG_READONLY | P_FLAG_DONT_FREE_MATRIX, 
			    0, MAX_DOUBLE_SIZE, -MAX_DOUBLE_SIZE, NULL);      
  load_builtin_param_double("rad", (void*)&rad_per_pixel, rad_mesh,  P_FLAG_PER_PIXEL |P_FLAG_ALWAYS_MATRIX | P_FLAG_READONLY | P_FLAG_DONT_FREE_MATRIX, 
			    0, MAX_DOUBLE_SIZE, -MAX_DOUBLE_SIZE, NULL);


  load_builtin_param_double("q1", (void*)&q1,  NULL, P_FLAG_PER_PIXEL |P_FLAG_QVAR, 0, MAX_DOUBLE_SIZE, -MAX_DOUBLE_SIZE, NULL);
  load_builtin_param_double("q2", (void*)&q2,  NULL, P_FLAG_PER_PIXEL |P_FLAG_QVAR, 0, MAX_DOUBLE_SIZE, -MAX_DOUBLE_SIZE, NULL);
  load_builtin_param_double("q3", (void*)&q3,  NULL, P_FLAG_PER_PIXEL |P_FLAG_QVAR, 0, MAX_DOUBLE_SIZE, -MAX_DOUBLE_SIZE, NULL);
  load_builtin_param_double("q4", (void*)&q4,  NULL, P_FLAG_PER_PIXEL |P_FLAG_QVAR, 0, MAX_DOUBLE_SIZE, -MAX_DOUBLE_SIZE, NULL);
  load_builtin_param_double("q5", (void*)&q5,  NULL, P_FLAG_PER_PIXEL |P_FLAG_QVAR, 0, MAX_DOUBLE_SIZE, -MAX_DOUBLE_SIZE, NULL);
  load_builtin_param_double("q6", (void*)&q6,  NULL, P_FLAG_PER_PIXEL |P_FLAG_QVAR, 0, MAX_DOUBLE_SIZE, -MAX_DOUBLE_SIZE, NULL);
  load_builtin_param_double("q7", (void*)&q7,  NULL, P_FLAG_PER_PIXEL |P_FLAG_QVAR, 0, MAX_DOUBLE_SIZE, -MAX_DOUBLE_SIZE, NULL);
  load_builtin_param_double("q8", (void*)&q8,  NULL, P_FLAG_PER_PIXEL |P_FLAG_QVAR, 0, MAX_DOUBLE_SIZE, -MAX_DOUBLE_SIZE, NULL);



  /* variables added in 1.04 */
  load_builtin_param_int("meshx", (void*)&gx, P_FLAG_READONLY, 32, 96, 8, NULL);
  load_builtin_param_int("meshy", (void*)&gy, P_FLAG_READONLY, 24, 72, 6, NULL);

  return SUCCESS;  
  
}

/* Free's a parameter type */
void free_param(param_t * param) {
  int x;
  if (param == NULL)
	return;
  
  if (param->flags & P_FLAG_USERDEF) {
    free(param->engine_val);

  }

  if (!(param->flags & P_FLAG_DONT_FREE_MATRIX)) {

    if (param->flags & P_FLAG_PER_POINT)
      free(param->matrix);

    else if (param->flags & P_FLAG_PER_PIXEL) {
       for(x = 0; x < gx; x++) 
	   free(((double**)param->matrix)[x]);
       free(param->matrix);
      }
  }

  if (PARAM_DEBUG) printf("free_param: freeing \"%s\".\n", param->name);
  free(param);

}

/* Loads a double parameter into the builtin database */
int load_builtin_param_double(char * name, void * engine_val, void * matrix, short int flags, 
						double init_val, double upper_bound, double lower_bound, char * alt_name) {

  param_t * param = NULL;
  value_t iv, ub, lb;

  iv.double_val = init_val;
  ub.double_val = upper_bound;
  lb.double_val = lower_bound;
							
  /* Create new parameter of type double */
  if (PARAM_DEBUG == 2) {
	  printf("load_builtin_param_double: (name \"%s\") (alt_name = \"%s\") ", name, alt_name);
	  fflush(stdout);
  }  
  
 if ((param = create_param(name, P_TYPE_DOUBLE, flags, engine_val, matrix, iv, ub, lb)) == NULL) {
    return OUTOFMEM_ERROR;
  }
  
  if (PARAM_DEBUG == 2) {
	printf("created...");
	fflush(stdout);
   }	  
   
  /* Insert the paremeter into the database */

  if (insert_builtin_param(param) < 0) {
	free_param(param);
    return ERROR;
  }

  if (PARAM_DEBUG == 2) {
	  printf("inserted...");
	  fflush(stdout);
  }  
  
  /* If this parameter has an alternate name, insert it into the database as link */
  
  if (alt_name != NULL) {
	insert_param_alt_name(param, alt_name); 

  if (PARAM_DEBUG == 2) {
	  printf("alt_name inserted...");
	  fflush(stdout);
  	}
  
	
  }  	  

  if (PARAM_DEBUG == 2) printf("finished\n");	  
  /* Finished, return success */
  return SUCCESS;
}



/* Loads a double parameter into the builtin database */
param_t * new_param_double(char * name, short int flags, void * engine_val, void * matrix,
						double upper_bound, double lower_bound, double init_val) {

  param_t * param;
  value_t iv, ub, lb;

  iv.double_val = init_val;
  ub.double_val = upper_bound;
  lb.double_val = lower_bound;
			     			
  if ((param = create_param(name, P_TYPE_DOUBLE, flags, engine_val, matrix,iv, ub, lb)) == NULL) 
    return NULL;
  
  
  /* Finished, return success */
  return param;
}


/* Creates a new parameter of type int */
param_t * new_param_int(char * name, short int flags, void * engine_val,
						int upper_bound, int lower_bound, int init_val) {

  param_t * param;
  value_t iv, ub, lb;

  iv.int_val = init_val;
  ub.int_val = upper_bound;
  lb.int_val = lower_bound;
							
  if ((param = create_param(name, P_TYPE_INT, flags, engine_val, NULL, iv, ub, lb)) == NULL) 
    return NULL;
  
 
  /* Finished, return success */
  return param;
}

/* Creates a new parameter of type bool */
param_t * new_param_bool(char * name, short int flags, void * engine_val,
						int upper_bound, int lower_bound, int init_val) {

  param_t * param;
  value_t iv, ub, lb;

  iv.bool_val = init_val;
  ub.bool_val = upper_bound;
  lb.bool_val = lower_bound;
							
  if ((param = create_param(name, P_TYPE_BOOL, flags, engine_val, NULL, iv, ub, lb)) == NULL)
    return NULL;
  
 
  /* Finished, return success */
  return param;
}


/* Loads a integer parameter into the builtin database */
int load_builtin_param_int(char * name, void * engine_val, short int flags,
						int init_val, int upper_bound, int lower_bound, char * alt_name) {

  param_t * param;
  value_t iv, ub, lb;

  iv.int_val = init_val;
  ub.int_val = upper_bound;
  lb.int_val = lower_bound;	
							
  param = create_param(name, P_TYPE_INT, flags, engine_val, NULL, iv, ub, lb);

  if (param == NULL) {
    return OUTOFMEM_ERROR;
  }

  if (insert_builtin_param(param) < 0) {
	free_param(param);
    return ERROR;
  }
  
  if (alt_name != NULL) {
	insert_param_alt_name(param, alt_name);  	 
  }  
  
  return SUCCESS;

}							
							
/* Loads a boolean parameter */
int load_builtin_param_bool(char * name, void * engine_val, short int flags, 
								int init_val, char * alt_name) {

  param_t * param;
  value_t iv, ub, lb;

  iv.int_val = init_val;
  ub.int_val = TRUE;
  lb.int_val = FALSE;	
																
  param = create_param(name, P_TYPE_BOOL, flags, engine_val, NULL, iv, ub, lb);

  if (param == NULL) {
    return OUTOFMEM_ERROR;
  }

  if (insert_builtin_param(param) < 0) {
	free_param(param);
    return ERROR;
  }
  
  if (alt_name != NULL) {
	insert_param_alt_name(param, alt_name);  	 
  }  
  
  return SUCCESS;

}


	

/* Returns nonzero if the string is valid parameter name */
int is_valid_param_string(char * string) {
  
  if (string == NULL)
    return FALSE;
  
  /* This ensures the first character is non numeric */
  if( ((*string) >= 48) && ((*string) <= 57))
    return FALSE; 

  /* These probably should never happen */
  if (*string == '.')
	return FALSE;
  
  if (*string == '+')
	return FALSE;
  
  if (*string == '-')
	return FALSE;
  
  /* Could also add checks for other symbols. May do later */
  
  return TRUE;
   
}

/* Inserts a parameter into the builtin database */
int insert_builtin_param(param_t * param) {

	if (param == NULL)
		return FAILURE;
	
	return splay_insert(param, param->name, builtin_param_tree);	
}

/* Inserts a parameter into the builtin database */
int insert_param(param_t * param, splaytree_t * database) {

	if (param == NULL)
	  return FAILURE;
	if (database == NULL)
	  return FAILURE;

	return splay_insert(param, param->name, database);	
}


/* Sets the parameter engine value to value val.
	clipping occurs if necessary */
void set_param(param_t * param, double val) {

	switch (param->type) {
		
	case P_TYPE_BOOL:
		if (val < 0)
			*((int*)param->engine_val) = 0;
		else if (val > 0)
			*((int*)param->engine_val) = 1;
		else
			*((int*)param->engine_val) = 0;
		break;
	case P_TYPE_INT:
		/* Make sure value is an integer */
		val = floor(val);
		if (val < param->lower_bound.int_val)
				*((int*)param->engine_val) = param->lower_bound.int_val;
		else if (val > param->upper_bound.int_val)
				*((int*)param->engine_val) = param->upper_bound.int_val;
		else
				*((int*)param->engine_val) = val;
		break;
	case P_TYPE_DOUBLE:
	  /* Make sure value is an integer */	

	 
	  if (val < param->lower_bound.double_val) 
	    *((double*)param->engine_val) = param->lower_bound.double_val;	  
	  else if (val > param->upper_bound.double_val)
	    *((double*)param->engine_val) = param->upper_bound.double_val;
	  else
	    *((double*)param->engine_val) = val;
	  break;
	default:
	  break;

	}
	
	return;
}




/* Search for parameter 'name' in 'database', if create_flag is true, then generate the parameter 
   and insert it into 'database' */
param_t * find_param_db(char * name, splaytree_t * database, int create_flag) {

  param_t * param = NULL;

  /* Null argument checks */
  if (name == NULL)
    return NULL;
  if (database == NULL)
    return NULL;
  
  /* First look in the builtin database */
  param = (param_t *)splay_find(name, database);

  
  if (((param = (param_t *)splay_find(name, database)) == NULL) && (create_flag == TRUE)) {
	
	/* Check if string is valid */
	if (!is_valid_param_string(name))
		return NULL;
	
	/* Now, create the user defined parameter given the passed name */
	if ((param = create_user_param(name)) == NULL)
		return NULL;
	
	/* Finally, insert the new parameter into this preset's proper splaytree */
	if (splay_insert(param, param->name, database) < 0) {
		free_param(param);
		return NULL;
	}	 
	
  }	  
  
  /* Return the found (or created) parameter. Note that this could be null */
  return param;

}

