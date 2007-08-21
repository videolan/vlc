#ifndef PARSER_H
#define PARSER_H
#define PARSE_DEBUG 0
#include "expr_types.h"
#include "per_frame_eqn_types.h"
#include "init_cond_types.h"
#include "preset_types.h"
#include <stdio.h>

per_frame_eqn_t * parse_per_frame_eqn(FILE * fs, int index, struct PRESET_T * preset);
int parse_per_pixel_eqn(FILE * fs, preset_t * preset);
init_cond_t * parse_init_cond(FILE * fs, char * name, struct PRESET_T * preset);
int parse_preset_name(FILE * fs, char * name);
int parse_top_comment(FILE * fs);
int parse_line(FILE * fs, struct PRESET_T * preset);
#endif
