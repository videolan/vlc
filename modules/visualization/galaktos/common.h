#ifndef COMMON_H
#define COMMON_H

#define DEFAULT_FONT_PATH "/home/carm/fonts/courier1.glf"
#define MAX_TOKEN_SIZE 512
#define MAX_PATH_SIZE 4096

#define STRING_BUFFER_SIZE 1024*150
#define STRING_LINE_SIZE 1024

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif 

#define PROJECTM_FILE_EXTENSION ".prjm"
#define MILKDROP_FILE_EXTENSION ".milk"

#define MAX_DOUBLE_SIZE  10000000.0
#define MIN_DOUBLE_SIZE -10000000.0

#define MAX_INT_SIZE  10000000
#define MIN_INT_SIZE -10000000

#define DEFAULT_DOUBLE_IV 0.0 /* default double initial value */
#define DEFAULT_DOUBLE_LB MIN_DOUBLE_SIZE /* default double lower bound */
#define DEFAULT_DOUBLE_UB MAX_DOUBLE_SIZE /* default double upper bound */

#endif
