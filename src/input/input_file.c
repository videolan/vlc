/*******************************************************************************
 * file.c: functions to read from a file 
 * (c)1999 VideoLAN
 *******************************************************************************/

/*******************************************************************************
 * Preamble
 *******************************************************************************/
#include <pthread.h>
#include <sys/uio.h>

#include "common.h"
#include "config.h"

#include "input.h"
#include "input_file.h"

/******************************************************************************
 * input_FileCreateMethod : open a file descriptor
 ******************************************************************************/
int input_FileCreateMethod( input_thread_t *p_input ,
                            input_cfg_t *p_cfg )
{
  return( -1 );
}

/******************************************************************************
 * input_FileRead : read from a file
 ******************************************************************************/
int input_FileRead( input_thread_t *p_input, const struct iovec *p_vector,
                    size_t i_count )
{
  return( -1 );
}

/******************************************************************************
 * input_FileDestroyMethod : close a file descriptor
 ******************************************************************************/
void input_FileDestroyMethod( input_thread_t *p_input )
{
}



