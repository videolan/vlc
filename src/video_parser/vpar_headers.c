/*****************************************************************************
 * vpar_headers.c : headers parsing
 * (c)1999 VideoLAN
 *****************************************************************************/

/* ?? passer en terminate/destroy avec les signaux supplémentaires */

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/uio.h>
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>

#include "config.h"
#include "common.h"
#include "mtime.h"
#include "vlc_thread.h"

#include "intf_msg.h"
#include "debug.h"                    /* ?? temporaire, requis par netlist.h */

#include "input.h"
#include "input_netlist.h"
#include "decoder_fifo.h"
#include "video.h"
#include "video_output.h"
#include "video_parser.h"

#include "undec_picture.h"
#include "video_fifo.h"
#include "video_decoder.h"

/*
 * Local prototypes
 */

/*****************************************************************************
 * vpar_NextSequenceHeader : Find the next sequence header
 *****************************************************************************/
void vpar_NextSequenceHeader( vpar_thread_t * p_vpar )
{
    while( !p_vpar->b_die )
    {
        NextStartCode( p_vpar );
        if( ShowBits( &p_vpar->bit_stream, 32 ) == SEQUENCE_START_CODE )
            return;
    }
}

/*****************************************************************************
 * vpar_ParseHeader : Parse the next header
 *****************************************************************************/
int vpar_ParseHeader( vpar_thread_t * p_vpar )
{
    while( !p_vpar->b_die )
    {
        NextStartCode( p_vpar );
        switch( GetBits32( &p_vpar->bit_stream ) )
        {
        case SEQUENCE_HEADER_CODE:
            SequenceHeader( p_vpar );
            return 0;
            break;

        case GROUP_START_CODE:
            GroupHeader( p_vpar );
            return 0;
            break;

        case PICTURE_START_CODE:
            PictureHeader( p_vpar );
            return 0;
            break;

        case SEQUENCE_END_CODE:
            return 1;
            break;

        default:
        }
    }

    return 0;
}

/*
 * Following functions are local
 */

/*****************************************************************************
 * NextStartCode : Find the next start code
 *****************************************************************************/
static __inline__ void NextStartCode( vpar_thread_t * p_vpar )
{
    /* Re-align the buffer to an 8-bit boundary */
    DumpBits( &p_vpar->bit_stream, p_vpar->bit_stream.fifo.i_available & 7 );

    while( ShowBits( &p_vpar->bit_stream, 24 ) != 0x01L && !p_vpar->b_die )
    {
        DumpBits( &p_vpar->bit_stream, 8 );
    }
}

/*****************************************************************************
 * SequenceHeader : Parse the next sequence header
 *****************************************************************************/
static void SequenceHeader( vpar_thread_t * p_vpar )
{
    int         i_frame_rate_code;

    p_vpar->sequence.i_height = ntohl( GetBits( p_vpar->bit_stream, 12 ) );
    p_vpar->sequence.i_width = ntohl( GetBits( p_vpar->bit_stream, 12 ) );
    p_vpar->sequence.i_ratio = GetBits( p_vpar->bit_stream, 4 );
    i_frame_rate_code = GetBits( p_vpar->bit_stream, 4 );

    /* We don't need bit_rate_value, marker_bit, vbv_buffer_size,
     * constrained_parameters_flag */
    DumpBits( p_vpar->bits_stream, 30 );
}

/*****************************************************************************
 * GroupHeader : Parse the next group of pictures header
 *****************************************************************************/
static void GroupHeader( vpar_thread_t * p_vpar )
{

}
/*****************************************************************************
 * PictureHeader : Parse the next picture header
 *****************************************************************************/
static void PictureHeader( vpar_thread_t * p_vpar )
{

}
