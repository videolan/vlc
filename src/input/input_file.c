/*****************************************************************************
 * input_file.c: functions to read from a file
 * (c)1999 VideoLAN
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <sys/types.h>
#include <sys/uio.h>

#include "common.h"
#include "config.h"
#include "mtime.h"
#include "vlc_thread.h"

#include "input.h"
#include "input_file.h"

/*****************************************************************************
 * input_FileOpen : open a file descriptor
 *****************************************************************************/
int input_FileOpen( input_thread_t *p_input )
{
    //??
    return( 1 );
}

/*****************************************************************************
 * input_FileRead : read from a file
 *****************************************************************************/
int input_FileRead( input_thread_t *p_input, const struct iovec *p_vector,
                    size_t i_count )
{
    //??
    return( -1 );
}

/*****************************************************************************
 * input_FileClose : close a file descriptor
 *****************************************************************************/
void input_FileClose( input_thread_t *p_input )
{
    //??
}
