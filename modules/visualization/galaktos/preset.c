/*****************************************************************************
 * preset.c:
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <vlc/vlc.h>


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <time.h>
#include "common.h"
#include "fatal.h"

#include "preset_types.h"
#include "preset.h"

#include "parser.h"

#include "expr_types.h"
#include "eval.h"

#include "splaytree_types.h"
#include "splaytree.h"
#include "tree_types.h"

#include "per_frame_eqn_types.h"
#include "per_frame_eqn.h"

#include "per_pixel_eqn_types.h"
#include "per_pixel_eqn.h"

#include "init_cond_types.h"
#include "init_cond.h"

#include "param_types.h"
#include "param.h"

#include "func_types.h"
#include "func.h"

#include "custom_wave_types.h"
#include "custom_wave.h"

#include "custom_shape_types.h"
#include "custom_shape.h"

#include "idle_preset.h"

/* The maximum number of preset names loaded into buffer */
#define MAX_PRESETS_IN_DIR 50000
extern int per_frame_eqn_count;
extern int per_frame_init_eqn_count;
//extern int custom_per_frame_eqn_count;

extern splaytree_t * builtin_param_tree;

preset_t * active_preset = NULL;
preset_t * idle_preset = NULL;
FILE * write_stream = NULL;


int preset_index = -1;
int preset_name_buffer_size = 0;
splaytree_t * chrono_order_preset_name_tree = NULL;
int get_preset_path(char ** preset_path_ptr, char * filepath, char * filename);
preset_t * load_preset(char * pathname);
int is_valid_extension(char * name);	
int load_preset_file(char * pathname, preset_t * preset);
int close_preset(preset_t * preset);

int write_preset_name(FILE * fs);
int write_per_pixel_equations(FILE * fs);
int write_per_frame_equations(FILE * fs);
int write_per_frame_init_equations(FILE * fs);
int write_init_conditions(FILE * fs);
void load_init_cond(param_t * param);
void load_init_conditions();
void write_init(init_cond_t * init_cond);
int init_idle_preset();
int destroy_idle_preset();
void load_custom_wave_init_conditions();
void load_custom_wave_init(custom_wave_t * custom_wave);

void load_custom_shape_init_conditions();
void load_custom_shape_init(custom_shape_t * custom_shape);

/* loadPresetDir: opens the directory buffer
   denoted by 'dir' to load presets */
   
int loadPresetDir(char * dir) {

  struct dirent ** name_list;
  char * preset_name;
  int i, j, dir_size;
  
  if (dir == NULL)
	return ERROR;
 
  if (chrono_order_preset_name_tree != NULL) {
	if (PRESET_DEBUG) printf("loadPresetDir: previous directory doesn't appear to be closed!\n");
	/* Let this slide for now */
  }	
  
  /* Scan the entire directory, storing each entry in a dirent struct array that needs 
     to be freed later. For more information, consult scandir(3) in the man pages */
  if ((dir_size = scandir(dir, &name_list, 0, alphasort)) < 0) {
	if (PRESET_DEBUG) printf("loadPresetDir: failed to open directory \"%s\"\n", dir);
	return ERROR;
  }
  
  chrono_order_preset_name_tree = create_splaytree(compare_int, copy_int, free_int);
  
  /* Iterate through entire dirent name list, adding to the preset name list if it
     is valid */  
  for (i = 0; ((i < dir_size) && (i < MAX_PRESETS_IN_DIR));i++) {

	/* Only perform the next set of operations if the preset name 
	   contains a valid extension */
	if (is_valid_extension(name_list[i]->d_name)) {
		
		/* Handle the out of memory case. My guess is xmms would
		   crash before this program would, but whatever...*/
		if ((preset_name = (char*)malloc(MAX_PATH_SIZE)) == NULL) {
			if (PRESET_DEBUG) printf("loadPresetDir: out of memory! \n");
			
			/* Free the rest of the dirent name list */
			for (j = i; j < dir_size; j++) 
				free(name_list[j]);
			destroy_splaytree(chrono_order_preset_name_tree);
			return OUTOFMEM_ERROR;
		}
				
		/* Now create the full path */
	    if (get_preset_path(&preset_name, dir, name_list[i]->d_name) < 0) {
			if (PRESET_DEBUG) printf("loadPresetDir: failed to generate full preset path name!\n");
			
			/* Free the rest of the dirent name list */
			for (j = i; j < dir_size; j++) 
				free(name_list[j]);
			destroy_splaytree(chrono_order_preset_name_tree);
			return OUTOFMEM_ERROR;
			
		}
		
		/* Insert the character string into the splay tree, with the key being its sequence number */
		splay_insert(preset_name, &preset_name_buffer_size, chrono_order_preset_name_tree);
		preset_name_buffer_size++;
	}
	
	/* Free the dirent struct */
	free(name_list[i]);
	
  }	
  
  free(name_list);
  
  /* No valid files in directory! */
  if (chrono_order_preset_name_tree->root == NULL) {
	if (PRESET_DEBUG) printf("loadPresetDir: no valid files in directory \"%s\"\n", dir);
	destroy_splaytree(chrono_order_preset_name_tree);
	chrono_order_preset_name_tree = NULL;
	return FAILURE;	  
  }	
  	  
  /* Start the prefix index right before the first entry, so next preset
     starts at the top of the list */
  preset_index = -1;
  
  /* Start the first preset */

  switchPreset(ALPHA_NEXT, HARD_CUT);
  
  return SUCCESS;
}

/* closePresetDir: closes the current
   preset directory buffer */

int closePresetDir() {

  /* No preset director appears to be loaded */	
  if (chrono_order_preset_name_tree == NULL) 
    return SUCCESS;
  
  if (PRESET_DEBUG) {
	 printf("closePresetDir: freeing directory buffer...");
	 fflush(stdout);
  }  
  
  /* Free each entry in the directory preset name tree */
  splay_traverse(free_int, chrono_order_preset_name_tree);
  
  /* Destroy the chronological order splay tree */
  destroy_splaytree(chrono_order_preset_name_tree);
  chrono_order_preset_name_tree = NULL;
  preset_name_buffer_size = 0;
  if (PRESET_DEBUG) printf("finished\n");
  
  return SUCCESS;
}



/* Converts a preset file name to a full path */ 
int get_preset_path(char ** preset_path_ptr, char * filepath, char * filename) {

  char * preset_path;
	
  /* An insanely paranoid sequence of argument checks */
  if (preset_path_ptr == NULL)
	return ERROR;
  if (*preset_path_ptr == NULL)
    return ERROR;
  if (filename == NULL)
	return ERROR;
  if (filepath == NULL)
	return ERROR;
  
  /* Mostly here for looks */
  preset_path = *preset_path_ptr;

  /* Clear the name space first */
  memset(preset_path, 0, MAX_PATH_SIZE);
  
  /* Now create the string "PATH/FILENAME", where path is either absolute or relative location
     of the .milk file, and filename is the name of the preset file itself */
  strcat(
  	strcat(
  		strncpy(
  			preset_path, 
  		    filepath, 
            MAX_PATH_SIZE-1),   
        "/"), 
    filename);	

  return SUCCESS;
}	

/* switchPreset: loads the next preset from the directory stream.
   loadPresetDir() must be called first. This is a
   sequential load function */

int switchPreset(switch_mode_t switch_mode, int cut_type) {

  preset_t * new_preset;
	
  int switch_index;
	
  /* Make sure a preset directory list is in the buffer */
  if (chrono_order_preset_name_tree == NULL) {
    if (PRESET_DEBUG) printf("switchPreset: it helps if you open a directory first with a loadPresetDir() call\n");
    return ERROR;
  }
  
  
  switch (switch_mode) {
	  
  case ALPHA_NEXT:
  /* An index variable that iterates through the directory
     buffer, doing wrap around when it reaches the end of
  	 the buffer */
  
  if (preset_index == (preset_name_buffer_size - 1))
		switch_index = preset_index = 0;
  else	
	  	switch_index = ++preset_index;
  break;

  case ALPHA_PREVIOUS:
	  
  if (preset_index == 0)
		switch_index = preset_index = preset_name_buffer_size - 1;
  else	
	  	switch_index = --preset_index;
  break;
  
  case RANDOM_NEXT:
	switch_index = (int) (preset_name_buffer_size*(rand()/(RAND_MAX+1.0)));
	break;
  case RESTART_ACTIVE:
	switch_index = preset_index;
	break;
  default:
  	return FAILURE;
  }
  
    
  /* Finally, load the preset using its actual path */
  if ((new_preset = load_preset((char*)splay_find(&switch_index, chrono_order_preset_name_tree))) == NULL) {
	if (PRESET_DEBUG) printf("switchPreset: failed to load preset\n");
	return ERROR;
  }

  /* Closes a preset currently loaded, if any */
  if ((active_preset != NULL) && (active_preset != idle_preset))
    close_preset(active_preset);

  /* Sets global active_preset pointer */
  active_preset = new_preset;

 
  /* Reinitialize the engine variables to sane defaults */
  reset_engine_vars();

  /* Add any missing initial conditions */
  load_init_conditions();

  /* Add any missing initial conditions for each wave */
  load_custom_wave_init_conditions();

/* Add any missing initial conditions for each shape */
  load_custom_shape_init_conditions();

  /* Need to evaluate the initial conditions once */
  evalInitConditions();

 
  //  evalInitPerFrameEquations();
  return SUCCESS;
}

/* Loads a specific preset by absolute path */
int loadPresetByFile(char * filename) {

  preset_t * new_preset;
 
  /* Finally, load the preset using its actual path */
  if ((new_preset = load_preset(filename)) == NULL) {
	if (PRESET_DEBUG) printf("loadPresetByFile: failed to load preset!\n");
	return ERROR;	  
  }

  /* Closes a preset currently loaded, if any */
  if ((active_preset != NULL) && (active_preset != idle_preset))
    close_preset(active_preset); 

  /* Sets active preset global pointer */
  active_preset = new_preset;

  /* Reinitialize engine variables */
  reset_engine_vars();

 
  /* Add any missing initial conditions for each wave */
  load_custom_wave_init_conditions();

 /* Add any missing initial conditions for each wave */
  load_custom_shape_init_conditions();

  /* Add any missing initial conditions */
  load_init_conditions();
  
  /* Need to do this once for menu */
  evalInitConditions();
  //  evalPerFrameInitEquations();
  return SUCCESS;

}

int init_idle_preset() {

  preset_t * preset;
  int i;

    /* Initialize idle preset struct */
  if ((preset = (preset_t*)malloc(sizeof(preset_t))) == NULL)
    return FAILURE;

  
  strncpy(preset->name, "idlepreset", strlen("idlepreset"));

  /* Initialize equation trees */
  preset->init_cond_tree = create_splaytree(compare_string, copy_string, free_string);
  preset->user_param_tree = create_splaytree(compare_string, copy_string, free_string);
  preset->per_frame_eqn_tree = create_splaytree(compare_int, copy_int, free_int);
  preset->per_pixel_eqn_tree = create_splaytree(compare_int, copy_int, free_int);
  preset->per_frame_init_eqn_tree = create_splaytree(compare_string, copy_string, free_string);
  preset->custom_wave_tree = create_splaytree(compare_int, copy_int, free_int);
  preset->custom_shape_tree = create_splaytree(compare_int, copy_int, free_int);
 
  /* Set file path to dummy name */  
  strncpy(preset->file_path, "IDLE PRESET", MAX_PATH_SIZE-1);
  
  /* Set initial index values */
  preset->per_pixel_eqn_string_index = 0;
  preset->per_frame_eqn_string_index = 0;
  preset->per_frame_init_eqn_string_index = 0;
  memset(preset->per_pixel_flag, 0, sizeof(int)*NUM_OPS);
  
  /* Clear string buffers */
  memset(preset->per_pixel_eqn_string_buffer, 0, STRING_BUFFER_SIZE);
  memset(preset->per_frame_eqn_string_buffer, 0, STRING_BUFFER_SIZE);
  memset(preset->per_frame_init_eqn_string_buffer, 0, STRING_BUFFER_SIZE);

  idle_preset = preset;
  
  return SUCCESS;
}

int destroy_idle_preset() {

  return close_preset(idle_preset);
  
}

/* initPresetLoader: initializes the preset
   loading library. this should be done before
   any parsing */
int initPresetLoader() {

  /* Initializes the builtin parameter database */
  init_builtin_param_db();

  /* Initializes the builtin function database */
  init_builtin_func_db();
	
  /* Initializes all infix operators */
  init_infix_ops();

  /* Set the seed to the current time in seconds */
  srand(time(NULL));

  /* Initialize the 'idle' preset */
  init_idle_preset();

 

  reset_engine_vars();

  active_preset = idle_preset;
  load_init_conditions();

  /* Done */
  if (PRESET_DEBUG) printf("initPresetLoader: finished\n");
  return SUCCESS;
}

/* Sort of experimental code here. This switches
   to a hard coded preset. Useful if preset directory
   was not properly loaded, or a preset fails to parse */

void switchToIdlePreset() {


  /* Idle Preset already activated */
  if (active_preset == idle_preset)
    return;


  /* Close active preset */
  if (active_preset != NULL)
    close_preset(active_preset);

  /* Sets global active_preset pointer */
  active_preset = idle_preset;

  /* Reinitialize the engine variables to sane defaults */
  reset_engine_vars();

  /* Add any missing initial conditions */
  load_init_conditions();

  /* Need to evaluate the initial conditions once */
  evalInitConditions();

}

/* destroyPresetLoader: closes the preset
   loading library. This should be done when 
   projectM does cleanup */

int destroyPresetLoader() {
  
  if ((active_preset != NULL) && (active_preset != idle_preset)) {	
  	close_preset(active_preset);      
  }	

  active_preset = NULL;
  
  destroy_idle_preset();
  destroy_builtin_param_db();
  destroy_builtin_func_db();
  destroy_infix_ops();

  return SUCCESS;

}

/* load_preset_file: private function that loads a specific preset denoted
   by the given pathname */
int load_preset_file(char * pathname, preset_t * preset) { 
  FILE * fs;
  int retval;

  if (pathname == NULL)
	  return FAILURE;
  if (preset == NULL)
	  return FAILURE;
  
  /* Open the file corresponding to pathname */
  if ((fs = fopen(pathname, "r")) == 0) {
    if (PRESET_DEBUG) printf("load_preset_file: loading of file %s failed!\n", pathname);
    return ERROR;	
  }

  if (PRESET_DEBUG) printf("load_preset_file: file stream \"%s\" opened successfully\n", pathname);

  /* Parse any comments */
  if (parse_top_comment(fs) < 0) {
    if (PRESET_DEBUG) printf("load_preset_file: no left bracket found...\n");
    fclose(fs);
    return FAILURE;
  }
  
  /* Parse the preset name and a left bracket */
  if (parse_preset_name(fs, preset->name) < 0) {
    if (PRESET_DEBUG) printf("load_preset_file: loading of preset name in file \"%s\" failed\n", pathname);
    fclose(fs);
    return ERROR;
  }
  
  if (PRESET_DEBUG) printf("load_preset_file: preset \"%s\" parsed\n", preset->name);

  /* Parse each line until end of file */
  if (PRESET_DEBUG) printf("load_preset_file: beginning line parsing...\n");
  while ((retval = parse_line(fs, preset)) != EOF) {
    if (retval == PARSE_ERROR) {
      if (PRESET_DEBUG > 1) printf("load_preset_file: parse error in file \"%s\"\n", pathname);
    }
  }
  
  if (PRESET_DEBUG) printf("load_preset_file: finished line parsing successfully\n"); 

  /* Now the preset has been loaded.
     Evaluation calls can be made at appropiate
     times in the frame loop */
  
  fclose(fs);
   
  if (PRESET_DEBUG) printf("load_preset_file: file \"%s\" closed, preset ready\n", pathname);
  return SUCCESS;
  
}

void evalInitConditions() {
  splay_traverse(eval_init_cond, active_preset->init_cond_tree);
  splay_traverse(eval_init_cond, active_preset->per_frame_init_eqn_tree);
}

void evalPerFrameEquations() {
  splay_traverse(eval_per_frame_eqn, active_preset->per_frame_eqn_tree);
}

void evalPerFrameInitEquations() {
  //printf("evalPerFrameInitEquations: per frame init unimplemented!\n");
  //  splay_traverse(eval_per_frame_eqn, active_preset->per_frame_init_eqn_tree);
}	

/* Returns nonzero if string 'name' contains .milk or
   (the better) .prjm extension. Not a very strong function currently */
int is_valid_extension(char * name) {

	if (PRESET_DEBUG > 1) {
		printf("is_valid_extension: scanning string \"%s\"...", name);
		fflush(stdout);
	}

	if (strstr(name, MILKDROP_FILE_EXTENSION)) {
			if (PRESET_DEBUG > 1) printf("\".milk\" extension found in string [true]\n");
			return TRUE;
	}	
	
	if (strstr(name, PROJECTM_FILE_EXTENSION)) {
		    if (PRESET_DEBUG > 1) printf("\".prjm\" extension found in string [true]\n");
			return TRUE;
	}
	 
	if (PRESET_DEBUG > 1) printf("no valid extension found [false]\n");
	return FALSE;
}

/* Private function to close a preset file */
int close_preset(preset_t * preset) {

  if (preset == NULL)
    return FAILURE;


  splay_traverse(free_init_cond, preset->init_cond_tree);
  destroy_splaytree(preset->init_cond_tree);
  
  splay_traverse(free_init_cond, preset->per_frame_init_eqn_tree);
  destroy_splaytree(preset->per_frame_init_eqn_tree);
  
  splay_traverse(free_per_pixel_eqn, preset->per_pixel_eqn_tree);
  destroy_splaytree(preset->per_pixel_eqn_tree);
  
  splay_traverse(free_per_frame_eqn, preset->per_frame_eqn_tree);
  destroy_splaytree(preset->per_frame_eqn_tree);
  
  splay_traverse(free_param, preset->user_param_tree);
  destroy_splaytree(preset->user_param_tree);
  
  splay_traverse(free_custom_wave, preset->custom_wave_tree);
  destroy_splaytree(preset->custom_wave_tree);

  splay_traverse(free_custom_shape, preset->custom_shape_tree);
  destroy_splaytree(preset->custom_shape_tree);

  free(preset); 
  
  return SUCCESS;

}

void reloadPerPixel(char *s, preset_t * preset) {
  
  FILE * fs;
  int slen;
  char c;
  int i;

  if (s == NULL)
    return;

  if (preset == NULL)
    return;

  /* Clear previous per pixel equations */
  splay_traverse(free_per_pixel_eqn, preset->per_pixel_eqn_tree);
  destroy_splaytree(preset->per_pixel_eqn_tree);
  preset->per_pixel_eqn_tree = create_splaytree(compare_int, copy_int, free_int);

  /* Convert string to a stream */
  fs = fmemopen (s, strlen(s), "r");

  while ((c = fgetc(fs)) != EOF) {
    ungetc(c, fs);
    parse_per_pixel_eqn(fs, preset);
  }

  fclose(fs);

  /* Clear string space */
  memset(preset->per_pixel_eqn_string_buffer, 0, STRING_BUFFER_SIZE);

  /* Compute length of string */
  slen = strlen(s);

  /* Copy new string into buffer */
  strncpy(preset->per_pixel_eqn_string_buffer, s, slen);

  /* Yet again no bounds checking */
  preset->per_pixel_eqn_string_index = slen;

  /* Finished */
 
  return;
}

/* Obviously unwritten */
void reloadPerFrameInit(char *s, preset_t * preset) {

}

void reloadPerFrame(char * s, preset_t * preset) {

  FILE * fs;
  int slen;
  char c;
  int eqn_count = 1;
  per_frame_eqn_t * per_frame;

  if (s == NULL)
    return;

  if (preset == NULL)
    return;

  /* Clear previous per frame equations */
  splay_traverse(free_per_frame_eqn, preset->per_frame_eqn_tree);
  destroy_splaytree(preset->per_frame_eqn_tree);
  preset->per_frame_eqn_tree = create_splaytree(compare_int, copy_int, free_int);

  /* Convert string to a stream */
  fs = fmemopen (s, strlen(s), "r");

  while ((c = fgetc(fs)) != EOF) {
    ungetc(c, fs);
    if ((per_frame = parse_per_frame_eqn(fs, eqn_count, preset)) != NULL) {
      splay_insert(per_frame, &eqn_count, preset->per_frame_eqn_tree);
      eqn_count++;
    }
  }

  fclose(fs);

  /* Clear string space */
  memset(preset->per_frame_eqn_string_buffer, 0, STRING_BUFFER_SIZE);

  /* Compute length of string */
  slen = strlen(s);

  /* Copy new string into buffer */
  strncpy(preset->per_frame_eqn_string_buffer, s, slen);

  /* Yet again no bounds checking */
  preset->per_frame_eqn_string_index = slen;

  /* Finished */
  printf("reloadPerFrame: %d eqns parsed succesfully\n", eqn_count-1);
  return;

}

preset_t * load_preset(char * pathname) {

  preset_t * preset;
  int i;

  /* Initialize preset struct */
  if ((preset = (preset_t*)malloc(sizeof(preset_t))) == NULL)
    return NULL;
   
  /* Initialize equation trees */
  preset->init_cond_tree = create_splaytree(compare_string, copy_string, free_string);
  preset->user_param_tree = create_splaytree(compare_string, copy_string, free_string);
  preset->per_frame_eqn_tree = create_splaytree(compare_int, copy_int, free_int);
  preset->per_pixel_eqn_tree = create_splaytree(compare_int, copy_int, free_int);
  preset->per_frame_init_eqn_tree = create_splaytree(compare_string, copy_string, free_string);
  preset->custom_wave_tree = create_splaytree(compare_int, copy_int, free_int);
  preset->custom_shape_tree = create_splaytree(compare_int, copy_int, free_int);

  memset(preset->per_pixel_flag, 0, sizeof(int)*NUM_OPS);

  /* Copy file path */  
  strncpy(preset->file_path, pathname, MAX_PATH_SIZE-1);
  
  /* Set initial index values */
  preset->per_pixel_eqn_string_index = 0;
  preset->per_frame_eqn_string_index = 0;
  preset->per_frame_init_eqn_string_index = 0;
  
  
  /* Clear string buffers */
  memset(preset->per_pixel_eqn_string_buffer, 0, STRING_BUFFER_SIZE);
  memset(preset->per_frame_eqn_string_buffer, 0, STRING_BUFFER_SIZE);
  memset(preset->per_frame_init_eqn_string_buffer, 0, STRING_BUFFER_SIZE);
  
  
  if (load_preset_file(pathname, preset) < 0) {
	if (PRESET_DEBUG) printf("load_preset: failed to load file \"%s\"\n", pathname);
	close_preset(preset);
	return NULL;
  }

  /* It's kind of ugly to reset these values here. Should definitely be placed in the parser somewhere */
  per_frame_eqn_count = 0;
  per_frame_init_eqn_count = 0;

  /* Finished, return new preset */
  return preset;
}

void savePreset(char * filename) {

  FILE * fs;

  if (filename == NULL)
    return;
  
  /* Open the file corresponding to pathname */
  if ((fs = fopen(filename, "w+")) == 0) {
    if (PRESET_DEBUG) printf("savePreset: failed to create filename \"%s\"!\n", filename);
    return;	
  }

  write_stream = fs;

  if (write_preset_name(fs) < 0) {
    write_stream = NULL;
    fclose(fs);
    return;
  }

  if (write_init_conditions(fs) < 0) {
    write_stream = NULL;
    fclose(fs);
    return;
  }

  if (write_per_frame_init_equations(fs) < 0) {
    write_stream = NULL;
    fclose(fs);
    return;
  }

  if (write_per_frame_equations(fs) < 0) {
    write_stream = NULL;
    fclose(fs);
    return;
  }

  if (write_per_pixel_equations(fs) < 0) {
    write_stream = NULL;
    fclose(fs);
    return;
  }
 
  write_stream = NULL;
  fclose(fs);

}

int write_preset_name(FILE * fs) {

  char s[256];
  int len;

  memset(s, 0, 256);

  if (fs == NULL)
    return FAILURE;

  /* Format the preset name in a string */
  sprintf(s, "[%s]\n", active_preset->name);

  len = strlen(s);

  /* Write preset name to file stream */
  if (fwrite(s, 1, len, fs) != len)
    return FAILURE;

  return SUCCESS;

}

int write_init_conditions(FILE * fs) {

  if (fs == NULL)
    return FAILURE;
  if (active_preset == NULL)
    return FAILURE;


  splay_traverse(write_init, active_preset->init_cond_tree);
  
  return SUCCESS;
}

void write_init(init_cond_t * init_cond) {

  char s[512];
  int len;

  if (write_stream == NULL)
    return;

  memset(s, 0, 512);

  if (init_cond->param->type == P_TYPE_BOOL)
    sprintf(s, "%s=%d\n", init_cond->param->name, init_cond->init_val.bool_val);

  else if (init_cond->param->type == P_TYPE_INT)    
    sprintf(s, "%s=%d\n", init_cond->param->name, init_cond->init_val.int_val);

  else if (init_cond->param->type == P_TYPE_DOUBLE)
  {
    lldiv_t div = lldiv( init_cond->init_val.double_val * 1000000,1000000 );
    sprintf(s, "%s="I64Fd".%06u\n", init_cond->param->name, div.quot,
                    (unsigned int) div.rem );
  }

  else { printf("write_init: unknown parameter type!\n"); return; }

  len = strlen(s);

  if ((fwrite(s, 1, len, write_stream)) != len)
    printf("write_init: failed writing to file stream! Out of disk space?\n");

}


int write_per_frame_init_equations(FILE * fs) {

  int len;

  if (fs == NULL)
    return FAILURE;
  if (active_preset == NULL)
    return FAILURE;
  
  len = strlen(active_preset->per_frame_init_eqn_string_buffer);

  if (fwrite(active_preset->per_frame_init_eqn_string_buffer, 1, len, fs) != len)
    return FAILURE;

  return SUCCESS;
}


int write_per_frame_equations(FILE * fs) {

  int len;

  if (fs == NULL)
    return FAILURE;
  if (active_preset == NULL)
    return FAILURE;

  len = strlen(active_preset->per_frame_eqn_string_buffer);

  if (fwrite(active_preset->per_frame_eqn_string_buffer, 1, len, fs) != len)
    return FAILURE;

  return SUCCESS;
}


int write_per_pixel_equations(FILE * fs) {

  int len;

  if (fs == NULL)
    return FAILURE;
  if (active_preset == NULL)
    return FAILURE;

  len = strlen(active_preset->per_pixel_eqn_string_buffer);

  if (fwrite(active_preset->per_pixel_eqn_string_buffer, 1, len, fs) != len)
    return FAILURE;

  return SUCCESS;
}


void load_init_conditions() {

  splay_traverse(load_init_cond, builtin_param_tree);

 
}

void load_init_cond(param_t * param) {

  init_cond_t * init_cond;
  value_t init_val;

  /* Don't count read only parameters as initial conditions */
  if (param->flags & P_FLAG_READONLY)
    return;

  /* If initial condition was not defined by the preset file, force a default one
     with the following code */
  if ((init_cond = splay_find(param->name, active_preset->init_cond_tree)) == NULL) {
    
    /* Make sure initial condition does not exist in the set of per frame initial equations */
    if ((init_cond = splay_find(param->name, active_preset->per_frame_init_eqn_tree)) != NULL)
      return;
    
    if (param->type == P_TYPE_BOOL)
      init_val.bool_val = 0;
    
    else if (param->type == P_TYPE_INT)
      init_val.int_val = *(int*)param->engine_val;

    else if (param->type == P_TYPE_DOUBLE)
      init_val.double_val = *(double*)param->engine_val;

    //printf("%s\n", param->name);
    /* Create new initial condition */
    if ((init_cond = new_init_cond(param, init_val)) == NULL)
      return;
    
    /* Insert the initial condition into this presets tree */
    if (splay_insert(init_cond, init_cond->param->name, active_preset->init_cond_tree) < 0) {
      free_init_cond(init_cond);
      return;
    }
    
  }
 
}

void load_custom_wave_init_conditions() {

  splay_traverse(load_custom_wave_init, active_preset->custom_wave_tree);

}

void load_custom_wave_init(custom_wave_t * custom_wave) {

  load_unspecified_init_conds(custom_wave);

}


void load_custom_shape_init_conditions() {

  splay_traverse(load_custom_shape_init, active_preset->custom_shape_tree);

}

void load_custom_shape_init(custom_shape_t * custom_shape) {
 
  load_unspecified_init_conds_shape(custom_shape);
 
}
