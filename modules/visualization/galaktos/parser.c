/*****************************************************************************
 * parser.c:
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



/* parser.c */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "fatal.h"

#include "splaytree_types.h"
#include "splaytree.h"
#include "tree_types.h"

#include "expr_types.h"
#include "eval.h"

#include "param_types.h"
#include "param.h"

#include "func_types.h"
#include "func.h"

#include "preset_types.h"
#include "builtin_funcs.h"

#include "per_pixel_eqn_types.h"
#include "per_pixel_eqn.h"

#include "init_cond_types.h"
#include "init_cond.h"

#include "per_frame_eqn_types.h"
#include "per_frame_eqn.h"

#include "parser.h"
#include "engine_vars.h"

#include "custom_wave_types.h"
#include "custom_wave.h"

#include "custom_shape_types.h"
#include "custom_shape.h"

/* Strings that prefix (and denote the type of) equations */


#define PER_FRAME_STRING "per_frame_"
#define PER_FRAME_STRING_LENGTH 10

#define PER_PIXEL_STRING "per_pixel_"
#define PER_PIXEL_STRING_LENGTH 10

#define PER_FRAME_INIT_STRING "per_frame_init_"
#define PER_FRAME_INIT_STRING_LENGTH 15

#define WAVECODE_STRING "wavecode_"
#define WAVECODE_STRING_LENGTH 9

#define WAVE_STRING "wave_"
#define WAVE_STRING_LENGTH 5

#define PER_POINT_STRING "per_point"
#define PER_POINT_STRING_LENGTH 9

#define PER_FRAME_STRING_NO_UNDERSCORE "per_frame"
#define PER_FRAME_STRING_NO_UNDERSCORE_LENGTH 9

#define SHAPECODE_STRING "shapecode_"
#define SHAPECODE_STRING_LENGTH 10

#define SHAPE_STRING "shape_"
#define SHAPE_STRING_LENGTH 6

#define SHAPE_INIT_STRING "init"
#define SHAPE_INIT_STRING_LENGTH 4

#define WAVE_INIT_STRING "init"
#define WAVE_INIT_STRING_LENGTH 4

/* Stores a line of a file as its being parsed */
char string_line_buffer[STRING_LINE_SIZE]; 

/* The current position of the string line buffer (see above) */
int string_line_buffer_index = 0;

/* All infix operators (except '=') are prototyped here */
extern infix_op_t * infix_add, * infix_minus, * infix_div, * infix_mult,
  * infix_or, * infix_and, * infix_mod, * infix_positive, * infix_negative;

/* If the parser reads a line with a custom wave, this pointer is set to
   the custom wave of concern */
custom_wave_t * current_wave = NULL;
custom_shape_t * current_shape = NULL;
/* Counts the number of lines parsed */
unsigned int line_count = 1;
int per_frame_eqn_count  = 0;
int per_frame_init_eqn_count = 0;

typedef enum {
  NORMAL_LINE_MODE,
  PER_FRAME_LINE_MODE,
  PER_PIXEL_LINE_MODE,
  INIT_COND_LINE_MODE,
  CUSTOM_WAVE_PER_POINT_LINE_MODE,
  CUSTOM_WAVE_PER_FRAME_LINE_MODE,
  CUSTOM_WAVE_WAVECODE_LINE_MODE,
  CUSTOM_SHAPE_SHAPECODE_LINE_MODE,
} line_mode_t;

line_mode_t line_mode = NORMAL_LINE_MODE;

/* Token enumeration type */
typedef enum {

  tEOL,   /* end of a line, usually a '/n' or '/r' */
  tEOF,   /* end of file */
  tLPr,   /* ( */
  tRPr,   /* ) */
  tLBr,   /* [ */
  tRBr,   /* ] */
  tEq,    /* = */
  tPlus,  /* + */
  tMinus, /* - */
  tMult,  /* * */
  tMod,   /* % */
  tDiv,   /* / */
  tOr,    /* | */
  tAnd,   /* & */
  tComma, /* , */
  tPositive, /* + as a prefix operator */
  tNegative, /* - as a prefix operator */
  tSemiColon, /* ; */
  tStringTooLong, /* special token to indicate an invalid string length */
  tStringBufferFilled /* the string buffer for this line is maxed out */
} token_t;


int get_string_prefix_len(char * string);
tree_expr_t * insert_gen_expr(gen_expr_t * gen_expr, tree_expr_t ** root);
tree_expr_t * insert_infix_op(infix_op_t * infix_op, tree_expr_t ** root);
token_t parseToken(FILE * fs, char * string);
gen_expr_t ** parse_prefix_args(FILE * fs, int num_args, struct PRESET_T * preset);
gen_expr_t * parse_infix_op(FILE * fs, token_t token, tree_expr_t * tree_expr, struct PRESET_T * preset);
gen_expr_t * parse_sign_arg(FILE * fs);
int parse_float(FILE * fs, double * float_ptr);
int parse_int(FILE * fs, int * int_ptr);
int insert_gen_rec(gen_expr_t * gen_expr, tree_expr_t * root);
int insert_infix_rec(infix_op_t * infix_op, tree_expr_t * root);
gen_expr_t * parse_gen_expr(FILE * fs, tree_expr_t * tree_expr, struct PRESET_T * preset);
per_frame_eqn_t * parse_implicit_per_frame_eqn(FILE * fs, char * param_string, int index, struct PRESET_T * preset);
init_cond_t * parse_per_frame_init_eqn(FILE * fs, struct PRESET_T * preset, splaytree_t * database);
int parse_wavecode_prefix(char * token, int * id, char ** var_string);
int parse_wavecode(char * token, FILE * fs, preset_t * preset);
int parse_wave_prefix(char * token, int * id, char ** eqn_string);

int parse_shapecode(char * eqn_string, FILE * fs, preset_t * preset);
int parse_shapecode_prefix(char * token, int * id, char ** var_string);

int parse_wave(char * eqn_string, FILE * fs, preset_t * preset);
int parse_shape(char * eqn_string, FILE * fs, preset_t * preset);
int parse_shape_prefix(char * token, int * id, char ** eqn_string);

int update_string_buffer(char * buffer, int * index);
int string_to_float(char * string, double * float_ptr);

/* Grabs the next token from the file. The second argument points
   to the raw string */

token_t parseToken(FILE * fs, char * string) {
  
  char c;
  int i;
  
  if (string != NULL)
    memset(string, 0, MAX_TOKEN_SIZE);
  
  /* Loop until a delimiter is found, or the maximum string size is found */
  for (i = 0; i < MAX_TOKEN_SIZE;i++) {
    c = fgetc(fs);
    
    /* If the string line buffer is full, quit */
    if (string_line_buffer_index == (STRING_LINE_SIZE - 1))
      return tStringBufferFilled;
    
    /* Otherwise add this character to the string line buffer */
    string_line_buffer[string_line_buffer_index++] = c;
    /* Now interpret the character */
    switch (c) {
      
    case '+':
      return tPlus; 
    case '-':
      return tMinus;
    case '%':
      return tMod;
    case '/':
      
      /* check for line comment here */
      if ((c = fgetc(fs)) == '/') {
	while(1) {
	  c = fgetc(fs);
	  if (c == EOF) {
	    line_mode = NORMAL_LINE_MODE;
	    return tEOF;				
	  }
	  if (c == '\n') {
	    line_mode = NORMAL_LINE_MODE;
	    return tEOL;
	  }
	}
	
      }
      
      /* Otherwise, just a regular division operator */
      ungetc(c, fs);
      return tDiv;
      
    case '*':
      return tMult;
    case '|':
      return tOr;
    case '&':
      return tAnd;
    case '(': 
      return tLPr;
    case ')':
      return tRPr;
    case '[': 
      return tLBr;
    case ']':
      return tRBr;
    case '=': 
      return tEq;
      //    case '\r':
      //break;
    case '\n':
      line_count++;
      line_mode = NORMAL_LINE_MODE;
      return tEOL;
    case ',':
      return tComma;
    case ';':
      return tSemiColon;
    case ' ': /* space, skip the character */
      i--;
      break;
    case EOF:
      line_count = 1;
      line_mode = NORMAL_LINE_MODE;
      return tEOF;
      
    default: 
      if (string != NULL)
	string[i] = c;
    } 
    
  }
  
 /* String reached maximum length, return special token error */ 
  return tStringTooLong;
  
}

/* Parse input in the form of "exp, exp, exp, ...)" 
   Returns a general expression list */

gen_expr_t ** parse_prefix_args(FILE * fs, int num_args, struct PRESET_T * preset) {

  int i, j;
  gen_expr_t ** expr_list; /* List of arguments to function */
  gen_expr_t * gen_expr;
  
  /* Malloc the expression list */
  expr_list =  (gen_expr_t**)malloc(sizeof(gen_expr_t*)*num_args);
  
  /* Malloc failed */
  if (expr_list == NULL)
    return NULL;
  
  
  i = 0;

  while (i < num_args) {
    //if (PARSE_DEBUG) printf("parse_prefix_args: parsing argument %d...\n", i+1);
    /* Parse the ith expression in the list */
    if ((gen_expr = parse_gen_expr(fs, NULL, preset)) == NULL) {
      //if (PARSE_DEBUG) printf("parse_prefix_args: failed to get parameter # %d for function (LINE %d)\n", i+1, line_count);
      for (j = 0; j < i; j++) 
	free_gen_expr(expr_list[j]);
      free(expr_list);
      return NULL;
    }
    /* Assign entry in expression list */
    expr_list[i++] = gen_expr;
  }
  
  //if (PARSE_DEBUG) printf("parse_prefix_args: finished parsing %d arguments (LINE %d)\n", num_args, line_count);	
  /* Finally, return the resulting expression list */
  return expr_list;
  
}

/* Parses a comment at the top of the file. Stops when left bracket is found */
int parse_top_comment(FILE * fs) {

  char string[MAX_TOKEN_SIZE];
  token_t token;
	  
  /* Process tokens until left bracket is found */
  while ((token = parseToken(fs, string)) != tLBr) {
    if (token == tEOF) 
      return PARSE_ERROR;
  }

 /* Done, return success */
 return SUCCESS; 
}	

/* Right Bracket is parsed by this function.
   puts a new string into name */
int parse_preset_name(FILE * fs, char * name) {

  token_t token;

  if (name == NULL)
	return FAILURE;

  if ((token = parseToken(fs, name)) != tRBr)
    return PARSE_ERROR;
 	
  //if (PARSE_DEBUG) printf("parse_preset_name: parsed preset (name = \"%s\")\n", name);
  
  return SUCCESS;
}


/* Parses per pixel equations */
int parse_per_pixel_eqn(FILE * fs, preset_t * preset) {


  char string[MAX_TOKEN_SIZE];
  gen_expr_t * gen_expr;

  if (PARSE_DEBUG) printf("parse_per_pixel: per_pixel equation parsing start...(LINE %d)\n", line_count);

  if (parseToken(fs, string) != tEq) { /* parse per pixel operator  name */
    if (PARSE_DEBUG) printf("parse_per_pixel: equal operator expected after per pixel operator \"%s\", but not found (LINE %d)\n", 
    			    string, line_count);
    return PARSE_ERROR;
  }
  
  /* Parse right side of equation as an expression */
  if ((gen_expr = parse_gen_expr(fs, NULL, preset)) == NULL) {
    if (PARSE_DEBUG) printf("parse_per_pixel: equation evaluated to null? (LINE %d)\n", line_count);
    return PARSE_ERROR;
  }
  
  /* Add the per pixel equation */
  if (add_per_pixel_eqn(string, gen_expr, preset) < 0) {
    free_gen_expr(gen_expr);
    return PARSE_ERROR;
  }

  return SUCCESS;
}

/* Parses an equation line, this function is way too big, should add some helper functions */
int parse_line(FILE * fs, struct PRESET_T * preset) {

  char eqn_string[MAX_TOKEN_SIZE];
  token_t token;
  init_cond_t * init_cond;
  per_frame_eqn_t * per_frame_eqn;
  
  /* Clear the string line buffer */
  memset(string_line_buffer, 0, STRING_LINE_SIZE);
  string_line_buffer_index = 0;
  
  
  switch (token = parseToken(fs, eqn_string)) {
    
    /* Invalid Cases */
  case tRBr:
  case tLPr:
  case tRPr:
  case tComma:
  case tLBr:
  case tPlus:
  case tMinus:
  case tMod:
  case tMult:
  case tOr:
  case tAnd:
  case tDiv:
    
    //    if (PARSE_DEBUG) printf("parse_line: invalid token found at start of line (LINE %d)\n", line_count);
    /* Invalid token found, return a parse error */
    return PARSE_ERROR;
    
    
  case tEOL:  /* Empty line */
    line_mode = NORMAL_LINE_MODE;
    return SUCCESS;
    
  case tEOF: /* End of File */
    line_mode = NORMAL_LINE_MODE;
    line_count = 1;
    return EOF;
    
  case tSemiColon: /* Indicates end of expression */
    return SUCCESS;
    
    /* Valid Case, either an initial condition or equation should follow */
  case tEq:
    
    /* CASE: PER FRAME INIT EQUATION */	    
    if (!strncmp(eqn_string, PER_FRAME_INIT_STRING, PER_FRAME_INIT_STRING_LENGTH)) {
      
      //if (PARSE_DEBUG) printf("parse_line: per frame init equation found...(LINE %d)\n", line_count);
      
      /* Set the line mode to normal */
      line_mode = NORMAL_LINE_MODE;
      
      /* Parse the per frame equation */
      if ((init_cond = parse_per_frame_init_eqn(fs, preset, NULL)) == NULL) {
	//if (PARSE_DEBUG) printf("parse_line: per frame init equation parsing failed (LINE %d)\n", line_count);
	return PARSE_ERROR;
      }	
      
      /* Insert the equation in the per frame equation tree */
      if (splay_insert(init_cond, init_cond->param->name, preset->per_frame_init_eqn_tree) < 0) {
	//if (PARSE_DEBUG) printf("parse_line: failed to add a perframe equation (ERROR)\n");
	free_init_cond(init_cond); /* will free the gen expr too */		
	return ERROR;
      }
      
     
      if (update_string_buffer(preset->per_frame_init_eqn_string_buffer, 
			       &preset->per_frame_init_eqn_string_index) < 0)
	{	return FAILURE;}
      
      return SUCCESS;
      
    }

    /* Per frame equation case */	    
    if (!strncmp(eqn_string, PER_FRAME_STRING, PER_FRAME_STRING_LENGTH)) {
      
      /* Sometimes per frame equations are implicitly defined without the
	 per_frame_ prefix. This informs the parser that one could follow */
      line_mode = PER_FRAME_LINE_MODE;
      
      //if (PARSE_DEBUG) printf("parse_line: per frame equation found...(LINE %d)\n", line_count);
      
      /* Parse the per frame equation */
      if ((per_frame_eqn = parse_per_frame_eqn(fs, ++per_frame_eqn_count, preset)) == NULL) {
	if (PARSE_DEBUG) printf("parse_line: per frame equation parsing failed (LINE %d)\n", line_count);
	return PARSE_ERROR;
      }	
      
      /* Insert the equation in the per frame equation tree */
      if (splay_insert(per_frame_eqn, &per_frame_eqn_count, preset->per_frame_eqn_tree) < 0) {
	if (PARSE_DEBUG) printf("parse_line: failed to add a perframe equation (ERROR)\n");
	free_per_frame_eqn(per_frame_eqn); /* will free the gen expr too */		
	return ERROR;
      }
    
      if (update_string_buffer(preset->per_frame_eqn_string_buffer, 
			       &preset->per_frame_eqn_string_index) < 0)
	return FAILURE;
      
      
      
      return SUCCESS;
      
    }
    
    /* Wavecode initial condition case */
    if (!strncmp(eqn_string, WAVECODE_STRING, WAVECODE_STRING_LENGTH)) {
      
          line_mode = CUSTOM_WAVE_WAVECODE_LINE_MODE;

      //if (PARSE_DEBUG) 
      //      printf("parse_line: wavecode prefix found: \"%s\"\n", eqn_string);

	  //      printf("string:%d\n", 5);

	  //SUPER MYSTERIO-BUG - Don't Remove
	  printf("");
	  
      return parse_wavecode(eqn_string, fs, preset);
    }
    
    /* Custom Wave Prefix */
    if ((!strncmp(eqn_string, WAVE_STRING, WAVE_STRING_LENGTH)) && 
	((eqn_string[5] >= 48) && (eqn_string[5] <= 57))) {
      
      //    if (PARSE_DEBUG) printf("parse_line wave prefix found: \"%s\"\n", eqn_string);
      
      return parse_wave(eqn_string, fs, preset);
      
    }
    
    
    /* Shapecode initial condition case */
    if (!strncmp(eqn_string, SHAPECODE_STRING, SHAPECODE_STRING_LENGTH)) {
      
      line_mode = CUSTOM_SHAPE_SHAPECODE_LINE_MODE;
      
      if (PARSE_DEBUG) printf("parse_line: shapecode prefix found: \"%s\"\n", eqn_string);
      
      return parse_shapecode(eqn_string, fs, preset);
    }
    
    /* Custom Shape Prefix */
    if ((!strncmp(eqn_string, SHAPE_STRING, SHAPE_STRING_LENGTH)) && 
	((eqn_string[6] >= 48) && (eqn_string[6] <= 57))) {
      
      if (PARSE_DEBUG) printf("parse_line shape prefix found: \"%s\"\n", eqn_string);
      return parse_shape(eqn_string, fs, preset);
      
    }
    
    /* Per pixel equation case */
    if (!strncmp(eqn_string, PER_PIXEL_STRING, PER_PIXEL_STRING_LENGTH)) {
      line_mode = PER_PIXEL_LINE_MODE;
      
      if (parse_per_pixel_eqn(fs, preset) < 0)
	return PARSE_ERROR;
      
      
      if (update_string_buffer(preset->per_pixel_eqn_string_buffer, 
			       &preset->per_pixel_eqn_string_index) < 0)
	return FAILURE;
      
      if (PARSE_DEBUG) printf("parse_line: finished parsing per pixel equation (LINE %d)\n", line_count);
      return SUCCESS;
    } 
    
    /* Sometimes equations are written implicitly in milkdrop files, in the form
       
    per_frame_1 = p1 = eqn1; p2 = eqn2; p3 = eqn3;..; 
    
    which is analagous to:
    
    per_frame_1 = p1 = eqn1; per_frame_2 = p2 = eqn2; per_frame_3 = p3 = eqn3; ...;
    
    The following line mode hack allows such implicit declaration of the 
    prefix that specifies the equation type. An alternative method
    may be to associate each equation line as list of equations separated
    by semicolons (and a new line ends the list). Instead, however, a global
    variable called "line_mode" specifies the last type of equation found,
    and bases any implicitly typed input on this fact
    
    Note added by Carmelo Piccione (cep@andrew.cmu.edu) 10/19/03
    */
    
    /* Per frame line mode previously, try to parse the equation implicitly */
    if (line_mode == PER_FRAME_LINE_MODE) {
      if ((per_frame_eqn = parse_implicit_per_frame_eqn(fs, eqn_string, ++per_frame_eqn_count, preset)) == NULL)
	return PARSE_ERROR;
      
      /* Insert the equation in the per frame equation tree */
      if (splay_insert(per_frame_eqn, &per_frame_eqn_count, preset->per_frame_eqn_tree) < 0) {
	if (PARSE_DEBUG) printf("parse_line: failed to add a perframe equation (ERROR)\n");
	free_per_frame_eqn(per_frame_eqn); /* will free the gen expr too */		
	return ERROR;
      }
      
      
      if (update_string_buffer(preset->per_frame_eqn_string_buffer, 
			       &preset->per_frame_eqn_string_index) < 0)
	return FAILURE;
      
      
      
      return SUCCESS;
    }
    
    //if (PARSE_DEBUG) printf("parse_line: found initial condition: name = \"%s\" (LINE %d)\n", eqn_string, line_count);
    
    /* Evaluate the initial condition */
    if ((init_cond = parse_init_cond(fs, eqn_string, preset)) == NULL) {
       if (PARSE_DEBUG) printf("parse_line: failed to parse initial condition (LINE %d)\n", line_count);
      return PARSE_ERROR; 
    }	
    
    /* Add equation to initial condition tree */
    if (splay_insert(init_cond, init_cond->param->name, preset->init_cond_tree) < 0) {
      if (PARSE_DEBUG) printf("parse_line: failed to add initial condition \"%s\" to equation tree (LINE %d)\n", 
      		      init_cond->param->name, line_count);
      free_init_cond(init_cond);
      return FAILURE;
    }
    
    /* Finished with initial condition line */
    //    if (PARSE_DEBUG) printf("parse_line: initial condition parsed successfully\n");
    
    return SUCCESS;
    
    /* END INITIAL CONDITIONING PARSING */
    
    
  default: /* an uncaught type or an error has occurred */
    if (PARSE_DEBUG) printf("parse_line: uncaught case, token val = %d\n", token); 
    return PARSE_ERROR;
  }
  
  /* Because of the default in the case statement, 
     control flow should never actually reach here */ 
  return PARSE_ERROR;
}



/* Parses a general expression, this function is the meat of the parser */
gen_expr_t * parse_gen_expr (FILE * fs, tree_expr_t * tree_expr, struct PRESET_T * preset) {
  
  int i;
  char string[MAX_TOKEN_SIZE];
  token_t token;
  gen_expr_t * gen_expr;
  double val;
  param_t * param = NULL;
  func_t * func;
  gen_expr_t ** expr_list;

  switch (token = parseToken(fs,string)) {
  /* Left Parentice Case */
  case tLPr:
    
    /* CASE 1 (Left Parentice): See if the previous string before this parentice is a function name */
    if ((func = find_func(string)) != NULL) {
        if (PARSE_DEBUG) printf("parse_gen_expr: found prefix function (name = %s) (LINE %d)\n", func->name, line_count);
      
      /* Parse the functions arguments */
      if ((expr_list = parse_prefix_args(fs, func->num_args, preset)) == NULL) {
	if (PARSE_DEBUG) printf("parse_prefix_args: failed to generate an expresion list! (LINE %d) \n", line_count);
	free_tree_expr(tree_expr);
	return NULL;
      }
      
      /* Convert function to expression */
      if ((gen_expr = prefun_to_expr((void*)func->func_ptr, expr_list, func->num_args)) == NULL)  { 	
	  if (PARSE_DEBUG) printf("parse_prefix_args: failed to convert prefix function to general expression (LINE %d) \n", 
	  				line_count);
	free_tree_expr(tree_expr);
	for (i = 0; i < func->num_args;i++)
	  free_gen_expr(expr_list[i]);
	free(expr_list);
	return NULL;
      }
    
      
      
      token = parseToken(fs, string);

      if (*string != 0) {
	if (PARSE_DEBUG) printf("parse_prefix_args: empty string expected, but not found...(LINE %d)\n", line_count);
	/* continue anyway for now, could be implicit multiplication */				
      }		
      
      return parse_infix_op(fs, token, insert_gen_expr(gen_expr, &tree_expr), preset);
    }
     
    
    /* Case 2: (Left Parentice), a string coupled with a left parentice. Either an error or implicit 
       multiplication operator. For now treat it as an error */
    if (*string != 0) {
      if (PARSE_DEBUG) printf("parse_gen_expr: implicit multiplication case unimplemented!\n");
      free_tree_expr(tree_expr);
      return NULL;
    }
    
    /* CASE 3 (Left Parentice): the following is enclosed parentices to change order
       of operations. So we create a new expression tree */
    
    if ((gen_expr = parse_gen_expr(fs, NULL, preset)) == NULL) {
      //if (PARSE_DEBUG) printf("parse_gen_expr:  found left parentice, but failed to create new expression tree \n");
      free_tree_expr(tree_expr);
      return NULL;
    }
    
    if (PARSE_DEBUG) printf("parse_gen_expr: finished enclosed expression tree...\n");	
    token = parseToken(fs, string);
    return parse_infix_op(fs, token, insert_gen_expr(gen_expr, &tree_expr), preset);

    /* Plus is a prefix operator check */
  case tPlus:
    if (*string == 0) {
      
      //if (PARSE_DEBUG) printf("parse_gen_expr: plus used as prefix (LINE %d)\n", line_count);

	  /* Treat prefix plus as implict 0 preceding operator */
      gen_expr = const_to_expr(0);

      return parse_infix_op(fs, tPositive, insert_gen_expr(gen_expr, &tree_expr), preset);	
    }
    
    /* Minus is a prefix operator check */
  case tMinus:
    if (*string == 0) {
     
      /* Use the negative infix operator, but first add an implicit zero to the operator tree */
      gen_expr = const_to_expr(0);
      //return parse_gen_expr(fs, insert_gen_expr(gen_expr, &tree_expr), preset);
		return parse_infix_op(fs, tNegative, insert_gen_expr(gen_expr, &tree_expr), preset);
    }
    
    /* All the following cases are strings followed by an infix operator or terminal */
  case tRPr:
  case tEOL: 
  case tEOF:
  case tSemiColon:
  case tComma:
    
    /* CASE 1 (terminal): string is empty, but not null. Not sure if this will actually happen
       any more. */
    if (*string == 0) {
      //if (PARSE_DEBUG) printf("parse_gen_expr: empty string coupled with terminal (LINE %d) \n", line_count);
      return parse_infix_op(fs, token, tree_expr, preset);
      
    }
    
  default:  

    /* CASE 0: Empty string, parse error */
    if (*string == 0) {
      if (PARSE_DEBUG) printf("parse_gen_expr: empty string coupled with infix op (ERROR!) (LINE %d) \n", line_count);
      free_tree_expr(tree_expr);
      return NULL;
    }

    /* CASE 1: Check if string is a just a floating point number */
    if (string_to_float(string, &val) != PARSE_ERROR) {
      if ((gen_expr = const_to_expr(val)) == NULL) {
	free_tree_expr(tree_expr);
	return NULL;
      }
      
      /* Parse the rest of the line */
      return parse_infix_op(fs, token, insert_gen_expr(gen_expr, &tree_expr), preset);          
    
    }

      
    /* CASE 4: custom shape variable */
    if (current_shape != NULL) {
      if ((param = find_param_db(string, current_shape->param_tree, FALSE)) == NULL) {
	if ((param = find_builtin_param(string)) == NULL)
	  if ((param = find_param_db(string, current_shape->param_tree, TRUE)) == NULL) {
	    free_tree_expr(tree_expr);
	    return NULL;
	  }
      }
      
      if (PARSE_DEBUG) {
	printf("parse_gen_expr: custom shape parameter (name = %s)... ", param->name);
	fflush(stdout);
      }  
      
      /* Convert parameter to an expression */
      if ((gen_expr = param_to_expr(param)) == NULL) {
	free_tree_expr(tree_expr);
	return NULL;
      }
      
      //if (PARSE_DEBUG) printf("converted to expression (LINE %d)\n", line_count);
      
      /* Parse the rest of the line */
      return parse_infix_op(fs, token, insert_gen_expr(gen_expr, &tree_expr), preset);
    }
    
    /* CASE 5: custom wave variable */
    if (current_wave != NULL) {
      if ((param = find_param_db(string, current_wave->param_tree, FALSE)) == NULL) {
	if ((param = find_builtin_param(string)) == NULL) 
	  if ((param = find_param_db(string, current_wave->param_tree, TRUE)) == NULL) {
	    free_tree_expr(tree_expr);
	    return NULL;
	  }
        
      }

      if (PARSE_DEBUG) {
	printf("parse_gen_expr: custom wave parameter (name = %s)... ", param->name);
	fflush(stdout);
      }
	
	/* Convert parameter to an expression */
	if ((gen_expr = param_to_expr(param)) == NULL) {
	  free_tree_expr(tree_expr);
	  return NULL;
	}
	
	if (PARSE_DEBUG) printf("converted to expression (LINE %d)\n", line_count);
	
	/* Parse the rest of the line */
	return parse_infix_op(fs, token, insert_gen_expr(gen_expr, &tree_expr), preset);
      
    }

    /* CASE 6: regular parameter. Will be created if necessary and the string has no invalid characters */
    if ((param = find_param(string, preset, P_CREATE)) != NULL) {
      
      if (PARSE_DEBUG) {
	printf("parse_gen_expr: parameter (name = %s)... ", param->name);
	fflush(stdout);
      }  
    
		/* Convert parameter to an expression */
      if ((gen_expr = param_to_expr(param)) == NULL) {
	free_tree_expr(tree_expr);
	return NULL;
      }
      
      if (PARSE_DEBUG) printf("converted to expression (LINE %d)\n", line_count);
      
      /* Parse the rest of the line */
      return parse_infix_op(fs, token, insert_gen_expr(gen_expr, &tree_expr), preset);
          
    }
   
    /* CASE 7: Bad string, give up */
    if (PARSE_DEBUG) printf("parse_gen_expr: syntax error [string = \"%s\"] (LINE %d)\n", string, line_count);
    free_tree_expr(tree_expr);
    return NULL;
  }
}
  


/* Inserts expressions into tree according to operator precedence.
   If root is null, a new tree is created, with gen_expr as only element */

tree_expr_t * insert_infix_op(infix_op_t * infix_op, tree_expr_t **root) {

  tree_expr_t * new_root;
  
  /* Sanity check */
  if (infix_op == NULL)
    return NULL;
  
  /* The root is null, so make this operator
     the new root */
  
  if (*root == NULL) {
    new_root = new_tree_expr(infix_op, NULL, NULL, NULL);
    *root = new_root;
    return new_root;		
  }
  
  /* The root node is not an infix function,
     so we make this infix operator the new root  */ 
  
  if ((*root)->infix_op == NULL) {
    new_root = new_tree_expr(infix_op, NULL, *root, NULL);
    (*root) = new_root;
    return new_root;
  }
  
  /* The root is an infix function. If the precedence
     of the item to be inserted is greater than the root's
     precedence, then make gen_expr the root */
  
  if (infix_op->precedence > (*root)->infix_op->precedence) {
    new_root = new_tree_expr(infix_op, NULL, *root, NULL);
    (*root) = new_root;
      return new_root;
  }
  
  /* If control flow reaches here, use a recursive helper
     with the knowledge that the root is higher precedence
     than the item to be inserted */
  
  insert_infix_rec(infix_op, *root);
  return *root;
  
}


tree_expr_t * insert_gen_expr(gen_expr_t * gen_expr, tree_expr_t ** root) {

  tree_expr_t * new_root;
  
  /* If someone foolishly passes a null
     pointer to insert, return the original tree */
  
  if (gen_expr == NULL) {
    return *root;
  }

  /* If the root is null, generate a new expression tree,
     using the passed expression as the root element */
  
  if (*root == NULL) {
    new_root = new_tree_expr(NULL, gen_expr, NULL, NULL);
    *root = new_root;
    return new_root;
  }
  
  
  /* Otherwise. the new element definitely will not replace the current root.
     Use a recursive helper function to do insertion */

  insert_gen_rec(gen_expr, *root);
  return *root;
}

/* A recursive helper function to insert general expression elements into the operator tree */
int insert_gen_rec(gen_expr_t * gen_expr, tree_expr_t * root) {
  
  /* Trivial Case: root is null */
  
  if (root == NULL) {
    ////if (PARSE_DEBUG) printf("insert_gen_rec: root is null, returning failure\n");
    return FAILURE;
  }
  
  
  /* The current node's left pointer is null, and this
     current node is an infix operator, so insert the
     general expression at the left pointer */
  
  if ((root->left == NULL) && (root->infix_op != NULL)) {
    root->left = new_tree_expr(NULL, gen_expr, NULL, NULL);
    return SUCCESS;
  }
  
  /* The current node's right pointer is null, and this
     current node is an infix operator, so insert the
     general expression at the right pointer */
  
  if ((root->right == NULL) && (root->infix_op != NULL)) {
    root->right = new_tree_expr(NULL, gen_expr, NULL, NULL);
    return SUCCESS;
  }
  
  /* Otherwise recurse down to the left. If
     this succeeds then return. If it fails, try
     recursing down to the right */
  
  if (insert_gen_rec(gen_expr, root->left) == FAILURE) 
    return insert_gen_rec(gen_expr, root->right);

  /* Impossible for control flow to reach here, but in
     the world of C programming, who knows... */
  //if (PARSE_DEBUG) printf("insert_gen_rec: should never reach here!\n");  
  return FAILURE;	
}	


/* A recursive helper function to insert infix arguments by operator precedence */
int insert_infix_rec(infix_op_t * infix_op, tree_expr_t * root) {

  /* Shouldn't happen, implies a parse error */

  if (root == NULL)
    return FAILURE;
  
  /* Also shouldn't happen, also implies a (different) parse error */

  if (root->infix_op == NULL)
    return FAILURE;

  /* Left tree is empty, attach this operator to it. 
     I don't think this will ever happen */
  if (root->left == NULL) {
    root->left = new_tree_expr(infix_op, NULL, root->left, NULL);
    return SUCCESS;
  }
 
  /* Right tree is empty, attach this operator to it */
  if (root->right == NULL) {
    root->right = new_tree_expr(infix_op, NULL, root->right, NULL);
    return SUCCESS;
  }

  /* The left element can now be ignored, since there is no way for this
     operator to use those expressions */

  /* If the right element is not an infix operator,
     then insert the expression here, attaching the old right branch
     to the left of the new expression */

  if (root->right->infix_op == NULL) {
    root->right = new_tree_expr(infix_op, NULL, root->right, NULL);
    return SUCCESS;
  }
  
  /* Traverse deeper if the inserting operator precedence is less than the
     the root's right operator precedence */
  if (infix_op->precedence < root->right->infix_op->precedence) 
    return insert_infix_rec(infix_op, root->right);

  /* Otherwise, insert the operator here */
  
  root->right = new_tree_expr(infix_op, NULL, root->right, NULL);
  return SUCCESS;

}

/* Parses an infix operator */
gen_expr_t * parse_infix_op(FILE * fs, token_t token, tree_expr_t * tree_expr, struct PRESET_T * preset) {
	
  gen_expr_t * gen_expr;

  switch (token) {
 	/* All the infix operators */
  case tPlus:
    //if (PARSE_DEBUG) printf("parse_infix_op: found addition operator (LINE %d)\n", line_count);
    return parse_gen_expr(fs, insert_infix_op(infix_add, &tree_expr), preset);
  case tMinus:
    //if (PARSE_DEBUG) printf("parse_infix_op: found subtraction operator (LINE %d)\n", line_count);
    return parse_gen_expr(fs, insert_infix_op(infix_minus, &tree_expr), preset);
  case tMult:
    //if (PARSE_DEBUG) printf("parse_infix_op: found multiplication operator (LINE %d)\n", line_count);
    return parse_gen_expr(fs, insert_infix_op(infix_mult, &tree_expr), preset);
  case tDiv:
    //if (PARSE_DEBUG) printf("parse_infix_op: found division operator (LINE %d)\n", line_count);  
    return parse_gen_expr(fs, insert_infix_op(infix_div, &tree_expr), preset);
  case tMod:
    //if (PARSE_DEBUG) printf("parse_infix_op: found modulo operator (LINE %d)\n", line_count);  
    return parse_gen_expr(fs, insert_infix_op(infix_mod, &tree_expr), preset);
  case tOr:  
    //if (PARSE_DEBUG) printf("parse_infix_op: found bitwise or operator (LINE %d)\n", line_count);  	  
    return parse_gen_expr(fs, insert_infix_op(infix_or, &tree_expr), preset);
  case tAnd: 	  
    //if (PARSE_DEBUG) printf("parse_infix_op: found bitwise and operator (LINE %d)\n", line_count);  	  
    return parse_gen_expr(fs, insert_infix_op(infix_and, &tree_expr), preset);
  case tPositive:
    //if (PARSE_DEBUG) printf("parse_infix_op: found positive operator (LINE %d)\n", line_count);  	  
    return parse_gen_expr(fs, insert_infix_op(infix_positive, &tree_expr), preset);
  case tNegative:
    //if (PARSE_DEBUG) printf("parse_infix_op: found negative operator (LINE %d)\n", line_count);  	  
    return parse_gen_expr(fs, insert_infix_op(infix_negative, &tree_expr), preset);

  case tEOL:
  case tEOF:
  case tSemiColon:
  case tRPr:
  case tComma:	  
	//if (PARSE_DEBUG) printf("parse_infix_op: terminal found (LINE %d)\n", line_count);
  	gen_expr = new_gen_expr(TREE_T, (void*)tree_expr);
  	return gen_expr;
  default:
    //if (PARSE_DEBUG) printf("parse_infix_op: operator or terminal expected, but not found (LINE %d)\n", line_count);
    free_tree_expr(tree_expr);
    return NULL;
  }  

  /* Will never happen */
  return NULL;
  
}

/* Parses an integer, checks for +/- prefix */
int parse_int(FILE * fs, int * int_ptr) {

char string[MAX_TOKEN_SIZE];
  token_t token;
  int sign;
  char * end_ptr = " ";
	
  token = parseToken(fs, string);

 
  switch (token) {
  case tMinus:
    sign = -1;
    token = parseToken(fs, string); 
    break;
  case tPlus:
    sign = 1;
    token = parseToken(fs, string);
    break;
  default: 
    sign = 1;
    break;
  }

 
  if (string[0] == 0) 
    return PARSE_ERROR;
  
  /* Convert the string to an integer. *end_ptr
     should end up pointing to null terminator of 'string' 
     if the conversion was successful. */
  //  printf("STRING: \"%s\"\n", string);

  (*int_ptr) = sign*strtol(string, &end_ptr, 10);

  /* If end pointer is a return character or null terminator, all is well */
  if ((*end_ptr == '\r') || (*end_ptr == '\0')) 
    return SUCCESS;

    return PARSE_ERROR;
  
}
/* Parses a floating point number */
int string_to_float(char * string, double * float_ptr) {

  char ** error_ptr;

  if (*string == 0)
    return PARSE_ERROR;

  error_ptr = malloc(sizeof(char**));
  
  (*float_ptr) = strtod(string, error_ptr);
 
  /* These imply a succesful parse of the string */
  if ((**error_ptr == '\0') || (**error_ptr == '\r')) {
    free(error_ptr);
    return SUCCESS;
  }
    
  (*float_ptr) = 0;
  free(error_ptr);
  return PARSE_ERROR;  
}

/* Parses a floating point number */
int parse_float(FILE * fs, double * float_ptr) {

  char string[MAX_TOKEN_SIZE];
  char ** error_ptr;
  token_t token;
  int sign;
  
  error_ptr = malloc(sizeof(char**));

  token = parseToken(fs, string);

  switch (token) {
  case tMinus:
  sign = -1;
  token = parseToken(fs, string); 
  break;
  case tPlus:
  sign = 1;
  token = parseToken(fs, string);
  break;
  default: 
    sign = 1;  
  }
 
  if (string[0] == 0) {
    free(error_ptr);
    return PARSE_ERROR;
  }

  (*float_ptr) = sign*strtod(string, error_ptr);
 
  /* No conversion was performed */
  if ((**error_ptr == '\0') || (**error_ptr == '\r')) {
    free(error_ptr);
    return SUCCESS;
  }
    
  //if (PARSE_DEBUG) printf("parse_float: double conversion failed for string \"%s\"\n", string);

  (*float_ptr) = 0;
  free(error_ptr);
  return PARSE_ERROR;
  

  
}

/* Parses a per frame equation. That is, interprets a stream of data as a per frame equation */
per_frame_eqn_t * parse_per_frame_eqn(FILE * fs, int index, struct PRESET_T * preset) {
  
  char string[MAX_TOKEN_SIZE];
  param_t * param;
  per_frame_eqn_t * per_frame_eqn;
  gen_expr_t * gen_expr;
  
  if (parseToken(fs, string) != tEq) {
    //if (PARSE_DEBUG) printf("parse_per_frame_eqn: no equal sign after string \"%s\" (LINE %d)\n", string, line_count);
    return NULL;			
  }
  
  /* Find the parameter associated with the string, create one if necessary */
  if ((param = find_param(string, preset, P_CREATE)) == NULL) { 
    return NULL;	
  }
  
  /* Make sure parameter is writable */
  if (param->flags & P_FLAG_READONLY) {
      //if (PARSE_DEBUG) printf("parse_per_frame_eqn: parameter %s is marked as read only (LINE %d)\n", param->name, line_count);  
      return NULL;
  }
  
  /* Parse right side of equation as an expression */
  if ((gen_expr = parse_gen_expr(fs, NULL, preset)) == NULL) {
    //if (PARSE_DEBUG) printf("parse_per_frame_eqn: equation evaluated to null (LINE %d)\n", line_count);
    return NULL;
  }
  
  //if (PARSE_DEBUG) printf("parse_per_frame_eqn: finished per frame equation evaluation (LINE %d)\n", line_count);
  
  /* Create a new per frame equation */
  if ((per_frame_eqn = new_per_frame_eqn(index, param, gen_expr)) == NULL) {
    //if (PARSE_DEBUG) printf("parse_per_frame_eqn: failed to create a new per frame eqn, out of memory?\n");
    free_gen_expr(gen_expr);
    return NULL;
  }
  
  //if (PARSE_DEBUG) printf("parse_per_frame_eqn: per_frame eqn parsed succesfully\n");
  
  return per_frame_eqn;
}

/* Parses an 'implicit' per frame equation. That is, interprets a stream of data as a per frame equation without a prefix */
per_frame_eqn_t * parse_implicit_per_frame_eqn(FILE * fs, char * param_string, int index, struct PRESET_T * preset) {
  
  param_t * param;
  per_frame_eqn_t * per_frame_eqn;
  gen_expr_t * gen_expr;
  
  if (fs == NULL)
    return NULL;
  if (param_string == NULL)
    return NULL;
  if (preset == NULL)
    return NULL;

  //rintf("param string: %s\n", param_string);
  /* Find the parameter associated with the string, create one if necessary */
  if ((param = find_param(param_string, preset, P_CREATE)) == NULL) { 
    return NULL;	
  }
  
  //printf("parse_implicit_per_frame_eqn: param is %s\n", param->name);

  /* Make sure parameter is writable */
  if (param->flags & P_FLAG_READONLY) {
    //if (PARSE_DEBUG) printf("parse_implicit_per_frame_eqn: parameter %s is marked as read only (LINE %d)\n", param->name, line_count);  
    return NULL;
  }
  
  /* Parse right side of equation as an expression */
  if ((gen_expr = parse_gen_expr(fs, NULL, preset)) == NULL) {
    //if (PARSE_DEBUG) printf("parse_implicit_per_frame_eqn: equation evaluated to null (LINE %d)\n", line_count);
    return NULL;
  }
  
  //if (PARSE_DEBUG) printf("parse_implicit_per_frame_eqn: finished per frame equation evaluation (LINE %d)\n", line_count);
  
  /* Create a new per frame equation */
  if ((per_frame_eqn = new_per_frame_eqn(index, param, gen_expr)) == NULL) {
    //if (PARSE_DEBUG) printf("parse_implicit_per_frame_eqn: failed to create a new per frame eqn, out of memory?\n");
    free_gen_expr(gen_expr);
    return NULL;
  }
  
  //if (PARSE_DEBUG) printf("parse_implicit_per_frame_eqn: per_frame eqn parsed succesfully\n");
  
  return per_frame_eqn;
}

/* Parses an initial condition */
init_cond_t * parse_init_cond(FILE * fs, char * name, struct PRESET_T * preset) {

  param_t * param;
  value_t init_val;
  init_cond_t * init_cond;
	
  if (name == NULL)
    return NULL;
  if (preset == NULL)
    return NULL;
  
  /* Search for the paramater in the database, creating it if necessary */
  if ((param = find_param(name, preset, P_CREATE)) == NULL) {
    return NULL;
  }
  
  //if (PARSE_DEBUG) printf("parse_init_cond: parameter = \"%s\" (LINE %d)\n", param->name, line_count);
  
  if (param->flags & P_FLAG_READONLY) {
    //if (PARSE_DEBUG) printf("parse_init_cond: builtin parameter \"%s\" marked as read only!\n", param->name);
    return NULL;
  }		
  
  /* At this point, a parameter has been created or was found
     in the database. */
  
  //if (PARSE_DEBUG) printf("parse_init_cond: parsing initial condition value... (LINE %d)\n", line_count);
  
  /* integer value (boolean is an integer in C) */
  if ((param->type == P_TYPE_INT) || (param->type == P_TYPE_BOOL)) {
    if ((parse_int(fs, (int*)&init_val.int_val)) == PARSE_ERROR) {	
      //if (PARSE_DEBUG) printf("parse_init_cond: error parsing integer!\n");
      return NULL;
    }
  }
  
  /* double value */
  else if (param->type == P_TYPE_DOUBLE) {
    if ((parse_float(fs, (double*)&init_val.double_val)) == PARSE_ERROR) {
      //if (PARSE_DEBUG) printf("parse_init_cond: error parsing double!\n");
      return NULL;
    }
  }
  
  /* Unknown value */
  else {
    //if (PARSE_DEBUG) printf("parse_init_cond: unknown parameter type!\n");
    return NULL;
  }
  
  /* Create new initial condition */
  if ((init_cond = new_init_cond(param, init_val)) == NULL) {
      //if (PARSE_DEBUG) printf("parse_init_cond: new_init_cond failed!\n");
      return NULL;
  }
  
  /* Finished */
  return init_cond;
}

/* Parses a per frame init equation, not sure if this works right now */
init_cond_t * parse_per_frame_init_eqn(FILE * fs, struct PRESET_T * preset, splaytree_t * database) {
  
  char name[MAX_TOKEN_SIZE];
  param_t * param = NULL;
  value_t init_val;
  init_cond_t * init_cond;
  gen_expr_t * gen_expr;
  double val;
  token_t token;


  if (preset == NULL)
    return NULL;
  if (fs == NULL)
    return NULL;

  if ((token = parseToken(fs, name)) != tEq)
    return NULL;
  

  /* If a database was specified,then use find_param_db instead */
  if ((database != NULL) && ((param = find_param_db(name, database, TRUE)) == NULL)) {
    return NULL;
  }

  /* Otherwise use the builtin parameter and user databases. This is confusing. Sorry. */
  if ((param == NULL) && ((param = find_param(name, preset, P_CREATE)) == NULL)) {
    return NULL;
  }
  
  //if (PARSE_DEBUG) printf("parse_per_frame_init_eqn: parameter = \"%s\" (LINE %d)\n", param->name, line_count);
  
  if (param->flags & P_FLAG_READONLY) {
    //if (PARSE_DEBUG) printf("pars_per_frame_init_eqn: builtin parameter \"%s\" marked as read only!\n", param->name);
    return NULL;
  }		
  
  /* At this point, a parameter has been created or was found
     in the database. */
  
  //if (PARSE_DEBUG) printf("parse_per_frame_init_eqn: parsing right hand side of per frame init equation.. (LINE %d)\n", line_count);
  
  if ((gen_expr = parse_gen_expr(fs, NULL, preset)) == NULL) {
    //if (PARSE_DEBUG) printf("parse_per_frame_init_eqn: failed to parse general expresion!\n");
    return NULL;
  }
 
  /* Compute initial condition value */
  val = eval_gen_expr(gen_expr);
  
  /* Free the general expression now that we are done with it */
  free_gen_expr(gen_expr);

  /* integer value (boolean is an integer in C) */
  if ((param->type == P_TYPE_INT) || (param->type == P_TYPE_BOOL)) {
    init_val.int_val = (int)val;
  }
  
  /* double value */
  else if (param->type == P_TYPE_DOUBLE) {
    init_val.double_val = val;
  }
  
  /* Unknown value */
  else {
    //if (PARSE_DEBUG) printf("parse_per_frame_init_eqn: unknown parameter type!\n");
    return NULL;
  }
  

  /* Create new initial condition */
  if ((init_cond = new_init_cond(param, init_val)) == NULL) {
      //if (PARSE_DEBUG) printf("parse_per_frame_init_eqn: new_init_cond failed!\n");
      return NULL;
  }


  /* Finished */
  return init_cond;
}

int parse_wavecode(char * token, FILE * fs, preset_t * preset) {

  char * var_string;
  init_cond_t * init_cond;
  custom_wave_t * custom_wave;
  int id;
  value_t init_val;
  param_t * param;

  /* Null argument checks */
  if (preset == NULL)
    return FAILURE;
  if (fs == NULL)
    return FAILURE;
  if (token == NULL)
    return FAILURE;

  /* token should be in the form wavecode_N_var, such as wavecode_1_samples */
  
  /* Get id and variable name from token string */
  if (parse_wavecode_prefix(token, &id, &var_string) < 0)   
    return PARSE_ERROR;
  
  //if (PARSE_DEBUG) printf("parse_wavecode: wavecode id = %d, parameter = \"%s\"\n", id, var_string);

  /* Retrieve custom wave information from preset. The 3rd argument
     if true creates a custom wave if one does not exist */
  if ((custom_wave = find_custom_wave(id, preset, TRUE)) == NULL) {
    //if (PARSE_DEBUG) printf("parse_wavecode: failed to load (or create) custom wave (id = %d)!\n", id);
    return FAILURE;
  }
  //if (PARSE_DEBUG) printf("parse_wavecode: custom wave found (id = %d)\n", custom_wave->id);

  /* Retrieve parameter from this custom waves parameter db */
  if ((param = find_param_db(var_string, custom_wave->param_tree, TRUE)) == NULL)
    return FAILURE;

  //if (PARSE_DEBUG) printf("parse_wavecode: custom wave parameter found (name = %s)\n", param->name);

  /* integer value (boolean is an integer in C) */
  if ((param->type == P_TYPE_INT) || (param->type == P_TYPE_BOOL)) {
    if ((parse_int(fs, (int*)&init_val.int_val)) == PARSE_ERROR) {	
      //if (PARSE_DEBUG) printf("parse_wavecode: error parsing integer!\n");
      return PARSE_ERROR;
    }
  }
  
  /* double value */
  else if (param->type == P_TYPE_DOUBLE) {
    if ((parse_float(fs, (double*)&init_val.double_val)) == PARSE_ERROR) {
      //if (PARSE_DEBUG) printf("parse_wavecode: error parsing double!\n");
      return PARSE_ERROR;
    }
  }
  
  /* Unknown value */
  else {
    //if (PARSE_DEBUG) printf("parse_wavecode: unknown parameter type!\n");
    return PARSE_ERROR;
  }
  
  /* Create new initial condition */
  if ((init_cond = new_init_cond(param, init_val)) == NULL) {
      //if (PARSE_DEBUG) printf("parse_wavecode: new_init_cond failed!\n");
      return FAILURE;
  }
  
  if (splay_insert(init_cond, param->name, custom_wave->init_cond_tree) < 0) {
    free_init_cond(init_cond);
    return PARSE_ERROR;
  }

  //if (PARSE_DEBUG) printf("parse_wavecode: [success]\n");
  return SUCCESS;
}

int parse_shapecode(char * token, FILE * fs, preset_t * preset) {

  char * var_string;
  init_cond_t * init_cond;
  custom_shape_t * custom_shape;
  int id;
  value_t init_val;
  param_t * param;

  /* Null argument checks */
  if (preset == NULL)
    return FAILURE;
  if (fs == NULL)
    return FAILURE;
  if (token == NULL)
    return FAILURE;

  /* token should be in the form shapecode_N_var, such as shapecode_1_samples */
  
  /* Get id and variable name from token string */
  if (parse_shapecode_prefix(token, &id, &var_string) < 0)   
    return PARSE_ERROR;
  
  //if (PARSE_DEBUG) printf("parse_shapecode: shapecode id = %d, parameter = \"%s\"\n", id, var_string);

  /* Retrieve custom shape information from preset. The 3rd argument
     if true creates a custom shape if one does not exist */
  if ((custom_shape = find_custom_shape(id, preset, TRUE)) == NULL) {
    //if (PARSE_DEBUG) printf("parse_shapecode: failed to load (or create) custom shape (id = %d)!\n", id);
    return FAILURE;
  }
  //if (PARSE_DEBUG) printf("parse_shapecode: custom shape found (id = %d)\n", custom_shape->id);

  /* Retrieve parameter from this custom shapes parameter db */
  if ((param = find_param_db(var_string, custom_shape->param_tree, TRUE)) == NULL) {
    //if (PARSE_DEBUG) printf("parse_shapecode: failed to create parameter.\n");
    return FAILURE;
  }
  //if (PARSE_DEBUG) printf("parse_shapecode: custom shape parameter found (name = %s)\n", param->name);

  /* integer value (boolean is an integer in C) */
  if ((param->type == P_TYPE_INT) || (param->type == P_TYPE_BOOL)) {
    if ((parse_int(fs, (int*)&init_val.int_val)) == PARSE_ERROR) {	
      //if (PARSE_DEBUG) printf("parse_shapecode: error parsing integer!\n");
      return PARSE_ERROR;
    }
  }
  
  /* double value */
  else if (param->type == P_TYPE_DOUBLE) {
    if ((parse_float(fs, (double*)&init_val.double_val)) == PARSE_ERROR) {
      //if (PARSE_DEBUG) printf("parse_shapecode: error parsing double!\n");
      return PARSE_ERROR;
    }
  }
  
  /* Unknown value */
  else {
    //if (PARSE_DEBUG) printf("parse_shapecode: unknown parameter type!\n");
    return PARSE_ERROR;
  }
  
  /* Create new initial condition */
  if ((init_cond = new_init_cond(param, init_val)) == NULL) {
      //if (PARSE_DEBUG) printf("parse_shapecode: new_init_cond failed!\n");
      return FAILURE;
  }
 
  if (splay_insert(init_cond, param->name, custom_shape->init_cond_tree) < 0) {
    free_init_cond(init_cond);
    //if (PARSE_DEBUG) printf("parse_shapecode: initial condition already set, not reinserting it (param = \"%s\")\n", param->name);
    return PARSE_ERROR;
  }

  //if (PARSE_DEBUG) printf("parse_shapecode: [success]\n");
  return SUCCESS;
}


int parse_wavecode_prefix(char * token, int * id, char ** var_string) {

  int len, i, j;
  
  if (token == NULL)
    return FAILURE;
  if (*var_string == NULL)
    return FAILURE;
  if (id == NULL)
    return FAILURE;
  
  len = strlen(token);

  /* Move pointer passed "wavecode_" prefix */
  if (len <= WAVECODE_STRING_LENGTH)
    return FAILURE;
  i = WAVECODE_STRING_LENGTH;
  j = 0;
  (*id) = 0;
  
  /* This loop grabs the integer id for this custom wave */
  while ((i < len) && (token[i] >=  48) && (token[i] <= 57)) {
    if (j >= MAX_TOKEN_SIZE)
      return FAILURE;
    
    (*id) = 10*(*id) + (token[i]-48);
    j++;
    i++;
  }

 
  if (i > (len - 2))
    return FAILURE;
  
  *var_string = token + i + 1;
 
  return SUCCESS;

}


int parse_shapecode_prefix(char * token, int * id, char ** var_string) {

  int len, i, j;
  
  if (token == NULL)
    return FAILURE;
  if (*var_string == NULL)
    return FAILURE;
  if (id == NULL)
    return FAILURE;
  
  len = strlen(token);

  /* Move pointer passed "shapecode_" prefix */
  if (len <= SHAPECODE_STRING_LENGTH)
    return FAILURE;
  i = SHAPECODE_STRING_LENGTH;
  j = 0;
  (*id) = 0;
  
  /* This loop grabs the integer id for this custom shape */
  while ((i < len) && (token[i] >=  48) && (token[i] <= 57)) {
    if (j >= MAX_TOKEN_SIZE)
      return FAILURE;
    
    (*id) = 10*(*id) + (token[i]-48);
    j++;
    i++;
  }

 
  if (i > (len - 2))
    return FAILURE;
  
  *var_string = token + i + 1;
 
  return SUCCESS;

}

int parse_wave_prefix(char * token, int * id, char ** eqn_string) {

  int len, i, j;
  
  if (token == NULL)
    return FAILURE;
  if (eqn_string == NULL)
    return FAILURE;
  if (id == NULL)
    return FAILURE;
  
  len = strlen(token);
 
  if (len <= WAVE_STRING_LENGTH)
    return FAILURE;


  i = WAVE_STRING_LENGTH;
  j = 0;
  (*id) = 0;
  
  /* This loop grabs the integer id for this custom wave */
  while ((i < len) && (token[i] >=  48) && (token[i] <= 57)) {
    if (j >= MAX_TOKEN_SIZE)
      return FAILURE;
    
    (*id) = 10*(*id) + (token[i]-48);
    j++;
    i++;
  }

  if (i > (len - 2))
    return FAILURE;
 
  *eqn_string = token + i + 1;
 
  return SUCCESS;

}

int parse_shape_prefix(char * token, int * id, char ** eqn_string) {

  int len, i, j;
  
  if (token == NULL)
    return FAILURE;
  if (eqn_string == NULL)
    return FAILURE;
  if (id == NULL)
    return FAILURE;
  
  len = strlen(token);
 
  if (len <= SHAPE_STRING_LENGTH)
    return FAILURE;


  i = SHAPE_STRING_LENGTH;
  j = 0;
  (*id) = 0;
  
  /* This loop grabs the integer id for this custom wave */
  while ((i < len) && (token[i] >=  48) && (token[i] <= 57)) {
    if (j >= MAX_TOKEN_SIZE)
      return FAILURE;
    
    (*id) = 10*(*id) + (token[i]-48);
    j++;
    i++;
  }

  if (i > (len - 2))
    return FAILURE;
 
  *eqn_string = token + i + 1;
 
  return SUCCESS;

}

/* Parses custom wave equations */
int parse_wave(char * token, FILE * fs, preset_t * preset) {
  
  int id;
  char * eqn_type;
  char string[MAX_TOKEN_SIZE];
  param_t * param;
  per_frame_eqn_t * per_frame_eqn;
  gen_expr_t * gen_expr;
  custom_wave_t * custom_wave;
  init_cond_t * init_cond;

  if (token == NULL)
    return FAILURE;
  if (fs == NULL)
    return FAILURE;
  if (preset == NULL)
    return FAILURE;
  
  /* Grab custom wave id and equation type (per frame or per point) from string token */
  if (parse_wave_prefix(token, &id, &eqn_type) < 0) {
    //if (PARSE_DEBUG) printf("parse_wave: syntax error in custom wave prefix!\n");
    return FAILURE;
  }
  /* Retrieve custom wave associated with this id */
  if ((custom_wave = find_custom_wave(id, preset, TRUE)) == NULL)
    return FAILURE;


  /* per frame init equation case */	    
  if (!strncmp(eqn_type, WAVE_INIT_STRING, WAVE_INIT_STRING_LENGTH)) {

    //if (PARSE_DEBUG) printf("parse_wave (per frame init): [begin] (LINE %d)\n", line_count);

    /* Parse the per frame equation */
    if ((init_cond = parse_per_frame_init_eqn(fs, preset, custom_wave->param_tree)) == NULL) {
      //if (PARSE_DEBUG) printf("parse_wave (per frame init): equation parsing failed (LINE %d)\n", line_count);
      return PARSE_ERROR;
    }	

    /* Insert the equation in the per frame equation tree */
    if (splay_insert(init_cond, init_cond->param->name, custom_wave->per_frame_init_eqn_tree) < 0) {
      //if (PARSE_DEBUG) printf("parse_wave (per frame init): failed to add equation (ERROR)\n");
       free_init_cond(init_cond); /* will free the gen expr too */		
      return FAILURE;
    }
   
    if (update_string_buffer(custom_wave->per_frame_init_eqn_string_buffer, 
			     &custom_wave->per_frame_init_eqn_string_index) < 0)
      return FAILURE;
	
    return SUCCESS;
  
  }

  /* per frame equation case */
  if (!strncmp(eqn_type, PER_FRAME_STRING_NO_UNDERSCORE, PER_FRAME_STRING_NO_UNDERSCORE_LENGTH)) {

    //if (PARSE_DEBUG) printf("parse_wave (per_frame): [start] (custom wave id = %d)\n", custom_wave->id);
    
    if (parseToken(fs, string) != tEq) {
      //if (PARSE_DEBUG) printf("parse_wave (per_frame): no equal sign after string \"%s\" (LINE %d)\n", string, line_count);
      return PARSE_ERROR;			
    }
  
    /* Find the parameter associated with the string in the custom wave database */
    if ((param = find_param_db(string, custom_wave->param_tree, TRUE)) == NULL) { 
      //if (PARSE_DEBUG) printf("parse_wave (per_frame): parameter \"%s\" not found or cannot be malloc'ed!!\n", string);
      return FAILURE;	
    }
  
    
    /* Make sure parameter is writable */
    if (param->flags & P_FLAG_READONLY) {
      //if (PARSE_DEBUG) printf("parse_wave (per_frame): parameter %s is marked as read only (LINE %d)\n", param->name, line_count);  
      return FAILURE;
    }
  
    /* Parse right side of equation as an expression */

    current_wave = custom_wave;
    if ((gen_expr = parse_gen_expr(fs, NULL, preset)) == NULL) {
      //if (PARSE_DEBUG) printf("parse_wave (per_frame): equation evaluated to null (LINE %d)\n", line_count);
      current_wave = NULL;
      return PARSE_ERROR;

    }
    current_wave = NULL;

    //if (PARSE_DEBUG) printf("parse_wave (per_frame): [finished parsing equation] (LINE %d)\n", line_count);
  
    /* Create a new per frame equation */
    if ((per_frame_eqn = new_per_frame_eqn(custom_wave->per_frame_count++, param, gen_expr)) == NULL) {
      //if (PARSE_DEBUG) printf("parse_wave (per_frame): failed to create a new per frame eqn, out of memory?\n");
      free_gen_expr(gen_expr);
      return FAILURE;
    }
 
    if (splay_insert(per_frame_eqn, &per_frame_eqn->index, custom_wave->per_frame_eqn_tree) < 0) {
      free_per_frame_eqn(per_frame_eqn);
      return FAILURE;
    }
       
    //if (PARSE_DEBUG) printf("parse_wave (per_frame): equation %d associated with custom wave %d [success]\n", 
    //			    per_frame_eqn->index, custom_wave->id);

    
    /* Need to add stuff to string buffer so the editor can read the equations. 
       Why not make a nice little helper function for this? - here it is: */

   
    if (update_string_buffer(custom_wave->per_frame_eqn_string_buffer, &custom_wave->per_frame_eqn_string_index) < 0)
      return FAILURE;

 
    return SUCCESS;
  }


  /* per point equation case */
  if (!strncmp(eqn_type, PER_POINT_STRING, PER_POINT_STRING_LENGTH)) {

    //if (PARSE_DEBUG) printf("parse_wave (per_point): per_pixel equation parsing start...(LINE %d)\n", line_count);

    if (parseToken(fs, string) != tEq) { /* parse per pixel operator  name */
      //if (PARSE_DEBUG) printf("parse_wave (per_point): equal operator missing after per pixel operator! (LINE %d)\n", line_count);
      return PARSE_ERROR;
    }
    
    /* Parse right side of equation as an expression */
    current_wave = custom_wave;
    if ((gen_expr = parse_gen_expr(fs, NULL, preset)) == NULL) {
      //if (PARSE_DEBUG) printf("parse_wave (per_point): equation evaluated to null? (LINE %d)\n", line_count);
      return PARSE_ERROR;
    }
    current_wave = NULL;

    /* Add the per point equation */
    if (add_per_point_eqn(string, gen_expr, custom_wave) < 0) {
      free_gen_expr(gen_expr);
      return PARSE_ERROR;
    }

   
    if (update_string_buffer(custom_wave->per_point_eqn_string_buffer, &custom_wave->per_point_eqn_string_index) < 0)
      return FAILURE;

    //if (PARSE_DEBUG) printf("parse_wave (per_point): [finished] (custom wave id = %d)\n", custom_wave->id);
    return SUCCESS;
  }


  /* Syntax error, return parse error */
  return PARSE_ERROR;
}



/* Parses custom shape equations */
int parse_shape(char * token, FILE * fs, preset_t * preset) {
  
  int id;
  char * eqn_type;
  char string[MAX_TOKEN_SIZE];
  param_t * param;
  per_frame_eqn_t * per_frame_eqn;
  gen_expr_t * gen_expr;
  custom_shape_t * custom_shape;
  init_cond_t * init_cond;

  if (token == NULL)

    return FAILURE;
  if (fs == NULL)
    return FAILURE;
  if (preset == NULL)
    return FAILURE;
  
  /* Grab custom shape id and equation type (per frame or per point) from string token */
  if (parse_shape_prefix(token, &id, &eqn_type) < 0) {
    //if (PARSE_DEBUG) printf("parse_shape: syntax error in custom shape prefix!\n");
    return PARSE_ERROR;
  }
  /* Retrieve custom shape associated with this id */
  if ((custom_shape = find_custom_shape(id, preset, TRUE)) == NULL)
    return FAILURE;


  /* per frame init equation case */	    
  if (!strncmp(eqn_type, SHAPE_INIT_STRING, SHAPE_INIT_STRING_LENGTH)) {

    //if (PARSE_DEBUG) printf("parse_shape (per frame init): [begin] (LINE %d)\n", line_count);

    /* Parse the per frame equation */
    if ((init_cond = parse_per_frame_init_eqn(fs, preset, custom_shape->param_tree)) == NULL) {
      //if (PARSE_DEBUG) printf("parse_shape (per frame init): equation parsing failed (LINE %d)\n", line_count);
      return PARSE_ERROR;
    }	
    
    /* Insert the equation in the per frame equation tree */
    if (splay_insert(init_cond, init_cond->param->name, custom_shape->per_frame_init_eqn_tree) < 0) {
      //if (PARSE_DEBUG) printf("parse_shape (per frame init): failed to add equation (ERROR)\n");
      free_init_cond(init_cond); /* will free the gen expr too */		
      return ERROR;
    }

    if (update_string_buffer(custom_shape->per_frame_init_eqn_string_buffer, 
			     &custom_shape->per_frame_init_eqn_string_index) < 0)
      return FAILURE;
	
    return SUCCESS;
  
  }

  /* per frame equation case */
  if (!strncmp(eqn_type, PER_FRAME_STRING_NO_UNDERSCORE, PER_FRAME_STRING_NO_UNDERSCORE_LENGTH)) {

    //if (PARSE_DEBUG) printf("parse_shape (per_frame): [start] (custom shape id = %d)\n", custom_shape->id);
    
    if (parseToken(fs, string) != tEq) {
      //if (PARSE_DEBUG) printf("parse_shape (per_frame): no equal sign after string \"%s\" (LINE %d)\n", string, line_count);
      return PARSE_ERROR;			
    }
  
    /* Find the parameter associated with the string in the custom shape database */
    if ((param = find_param_db(string, custom_shape->param_tree, TRUE)) == NULL) { 
      //if (PARSE_DEBUG) printf("parse_shape (per_frame): parameter \"%s\" not found or cannot be malloc'ed!!\n", string);
      return FAILURE;	
    }
  
    
    /* Make sure parameter is writable */
    if (param->flags & P_FLAG_READONLY) {
      //if (PARSE_DEBUG) printf("parse_shape (per_frame): parameter %s is marked as read only (LINE %d)\n", param->name, line_count);  
      return FAILURE;
    }
  
    /* Parse right side of equation as an expression */

    current_shape = custom_shape;
    if ((gen_expr = parse_gen_expr(fs, NULL, preset)) == NULL) {
      //if (PARSE_DEBUG) printf("parse_shape (per_frame): equation evaluated to null (LINE %d)\n", line_count);
      current_shape = NULL;
      return PARSE_ERROR;
    }

    current_shape = NULL;

    //if (PARSE_DEBUG) printf("parse_shape (per_frame): [finished parsing equation] (LINE %d)\n", line_count);
  
    /* Create a new per frame equation */
    if ((per_frame_eqn = new_per_frame_eqn(custom_shape->per_frame_count++, param, gen_expr)) == NULL) {
      //if (PARSE_DEBUG) printf("parse_shape (per_frame): failed to create a new per frame eqn, out of memory?\n");
      free_gen_expr(gen_expr);
      return FAILURE;
    }
 
    if (splay_insert(per_frame_eqn, &per_frame_eqn->index, custom_shape->per_frame_eqn_tree) < 0) {
      free_per_frame_eqn(per_frame_eqn);
      return FAILURE;
    }
       
    //if (PARSE_DEBUG) printf("parse_shape (per_frame): equation %d associated with custom shape %d [success]\n", 
    //			    per_frame_eqn->index, custom_shape->id);

    
    /* Need to add stuff to string buffer so the editor can read the equations.
       Why not make a nice little helper function for this? - here it is: */
    
    if (update_string_buffer(custom_shape->per_frame_eqn_string_buffer, &custom_shape->per_frame_eqn_string_index) < 0)
      return FAILURE;

 
    return SUCCESS;
  }


  /* Syntax error, return parse error */
  return PARSE_ERROR;
}

/* Helper function to update the string buffers used by the editor */
int update_string_buffer(char * buffer, int * index) {

  int string_length;
  int skip_size;

  if (!buffer)
    return FAILURE;
  if (!index)
    return FAILURE;

  
  /* If the string line buffer used by the parser is already full then quit */
  if (string_line_buffer_index == (STRING_LINE_SIZE-1))
    return FAILURE;

  if ((skip_size = get_string_prefix_len(string_line_buffer)) == FAILURE)
    return FAILURE;

  string_line_buffer[string_line_buffer_index++] = '\n';

  //  string_length = strlen(string_line_buffer + strlen(eqn_string)+1);
  if (skip_size >= STRING_LINE_SIZE)
    return FAILURE;

  string_length = strlen(string_line_buffer + skip_size);

  if (skip_size > (STRING_LINE_SIZE-1))
    return FAILURE;

  /* Add line to string buffer */
  strncpy(buffer + (*index), 
	  string_line_buffer + skip_size, string_length);
  
  /* Buffer full, quit */
  if ((*index) > (STRING_BUFFER_SIZE - 1)) {
    //if (PARSE_DEBUG) printf("update_string_buffer: string buffer full!\n");
    return FAILURE;
  }	
  
  /* Otherwise, increment string index by the added string length */
  (*index)+=string_length;
    
  return SUCCESS;
  
}


/* Helper function: returns the length of the prefix portion in the line
   buffer (the passed string here). In other words, given
   the string 'per_frame_1 = x = ....', return the length of 'per_frame_1 = '
   Returns -1 if syntax error
*/

int get_string_prefix_len(char * string) {
  
  int i = 0;

  /* Null argument check */
  if (string == NULL)
    return FAILURE;

  
  /* First find the equal sign */
  while (string[i] != '=') {
    if (string[i] == 0)
      return FAILURE;
    i++;
  }

  /* If the string already ends at the next char then give up */
  if (string[i+1] == 0)
    return FAILURE;

  /* Move past the equal sign */
  i++;

  /* Now found the start of the LHS variable, ie skip the spaces */
  while(string[i] == ' ') {
    i++;
  }

  /* If this is the end of the string then its a syntax error */
  if (string[i] == 0)
    return FAILURE;

  /* Finished succesfully, return the length */
  return i;
}
