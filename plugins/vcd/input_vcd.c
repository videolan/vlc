/****************************************************************************
 * input_vcd.c: VideoCD raw reading plugin.
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 *
 * Author: Johan Bilien <jobi@via.ecp.fr>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdio.h>
#include <stdlib.h>

#include <videolan/vlc.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>

#ifdef STRNCASECMP_IN_STRINGS_H
#   include <strings.h>
#endif

#if defined( WIN32 )
#   include <io.h>                                                 /* read() */
#else
#   include <sys/uio.h>                                      /* struct iovec */
#endif

#if defined( WIN32 )
#   include "input_iovec.h"
#endif

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"
#include "input_ext-plugins.h"

#include "debug.h"

#include "input_vcd.h"
#include "linux_cdrom_tools.h"

/* how many blocks VCDRead will read in each loop */
#define VCD_BLOCKS_ONCE 4
#define VCD_DATA_ONCE   (2 * VCD_BLOCKS_ONCE)
#define BUFFER_SIZE VCD_DATA_SIZE

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
/* called from outside */
static int  VCDProbe        ( probedata_t *p_data );
static void VCDInit         ( struct input_thread_s * );
static int  VCDRead         ( struct input_thread_s *, data_packet_t ** );
static int  VCDSetArea      ( struct input_thread_s *, struct input_area_s * );
static int  VCDSetProgram   ( struct input_thread_s *, pgrm_descriptor_t * );
static void VCDOpen         ( struct input_thread_s *);
static void VCDClose        ( struct input_thread_s *);
static void VCDEnd          ( struct input_thread_s *);
static void VCDSeek         ( struct input_thread_s *, off_t );
static int  VCDRewind       ( struct input_thread_s * );

/*****************************************************************************
 * Declare a buffer manager
 *****************************************************************************/
#define FLAGS           BUFFERS_NOFLAGS
#define NB_LIFO         2
DECLARE_BUFFERS_EMBEDDED( FLAGS, NB_LIFO );
DECLARE_BUFFERS_INIT( FLAGS, NB_LIFO );
DECLARE_BUFFERS_END( FLAGS, NB_LIFO );
DECLARE_BUFFERS_NEWPACKET( FLAGS, NB_LIFO );
DECLARE_BUFFERS_DELETEPACKET( FLAGS, NB_LIFO, 1000 );
DECLARE_BUFFERS_NEWPES( FLAGS, NB_LIFO );
DECLARE_BUFFERS_DELETEPES( FLAGS, NB_LIFO, 1000 );


/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( input_getfunctions )( function_list_t * p_function_list )
{
#define input p_function_list->functions.input
    p_function_list->pf_probe = VCDProbe;
    input.pf_init             = VCDInit;
    input.pf_open             = VCDOpen;
    input.pf_close            = VCDClose;
    input.pf_end              = VCDEnd;
    input.pf_init_bit_stream  = InitBitstream;
    input.pf_read             = VCDRead;
    input.pf_set_area         = VCDSetArea;
    input.pf_set_program      = VCDSetProgram;
    input.pf_demux            = input_DemuxPS;
    input.pf_new_packet       = input_NewPacket;
    input.pf_new_pes          = input_NewPES;
    input.pf_delete_packet    = input_DeletePacket;
    input.pf_delete_pes       = input_DeletePES;
    input.pf_rewind           = VCDRewind;
    input.pf_seek             = VCDSeek;
#undef input
}

/*
 * Data reading functions
 */

/*****************************************************************************
 * VCDProbe: verifies that the stream is a PS stream
 *****************************************************************************/
static int VCDProbe( probedata_t *p_data )
{
    input_thread_t * p_input = (input_thread_t *)p_data;

    char * psz_name = p_input->p_source;
    int i_score = 0;

    if( ( strlen(psz_name) > 4 ) && !strncasecmp( psz_name, "vcd:", 4 ) )
    {
        /* If the user specified "vcd:" then it's probably a VCD */
        i_score = 100;
        psz_name += 4;
    }

    return( i_score );
}

/*****************************************************************************
 * VCDOpen: open vcd
 *****************************************************************************/
static void VCDOpen( struct input_thread_s *p_input )
{
    vlc_mutex_lock( &p_input->stream.stream_lock );

    /* If we are here we can control the pace... */
    p_input->stream.b_pace_control = 1;

    p_input->stream.b_seekable = 1;
    p_input->stream.p_selected_area->i_size = 0;
    p_input->stream.p_selected_area->i_tell = 0;

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /* XXX: put this shit in an access plugin */
    if( strlen( p_input->p_source ) > 4
         && !strncasecmp( p_input->p_source, "vcd:", 4 ) )
    {
        p_input->i_handle
            = open( p_input->p_source + 4, O_RDONLY | O_NONBLOCK );
    }
    else
    {
        p_input->i_handle
            = open( p_input->p_source, O_RDONLY | O_NONBLOCK );
    }

    if( p_input->i_handle == -1 )
    {
        p_input->b_error = 1;
    }
}

/*****************************************************************************
 * VCDClose: close vcd
 *****************************************************************************/
static void VCDClose( struct input_thread_s *p_input )
{
    close( p_input->i_handle );
}

/*****************************************************************************
 * VCDInit: initializes VCD structures
 *****************************************************************************/
static void VCDInit( input_thread_t * p_input )
{
    thread_vcd_data_t *  p_vcd;
    int                  i_title;
    int                  i_chapter;
    int                  i;
    input_area_t *       p_area;
    es_descriptor_t *    p_es;

    p_vcd = malloc( sizeof(thread_vcd_data_t) );

    if( p_vcd == NULL )
    {
        intf_ErrMsg( "vcd error: out of memory" );
        p_input->b_error = 1;
        return;
    }

    p_input->p_plugin_data = (void *)p_vcd;

    if( (p_input->p_method_data = input_BuffersInit()) == NULL )
    {
        free( p_vcd );
        p_input->b_error = 1;
        return;
    }

    p_vcd->i_handle = p_input->i_handle;

    /* We read the Table Of Content information */
    p_vcd->nb_tracks = ioctl_GetTrackCount( p_input->i_handle );
    if( p_vcd->nb_tracks < 0 )
    {
        input_BuffersEnd( p_input->p_method_data );
        free( p_vcd );
        p_input->b_error = 1;
        return;
    }

    p_vcd->p_sectors = ioctl_GetSectors( p_input->i_handle );
    if ( p_vcd->p_sectors == NULL )
    {
        input_BuffersEnd( p_input->p_method_data );
        free( p_vcd );
        p_input->b_error = 1;
        return;
    }

    /* Set stream and area data */
    vlc_mutex_lock( &p_input->stream.stream_lock );

    /* Initialize ES structures */
    input_InitStream( p_input, sizeof( stream_ps_data_t ) );

    /* disc input method */
    p_input->stream.i_method = INPUT_METHOD_VCD;

#define area p_input->stream.pp_areas
    for( i = 1 ; i <= p_vcd->nb_tracks - 1 ; i++ )
    {
        input_AddArea( p_input );

        /* Titles are Program Chains */
        area[i]->i_id = i;

        /* Absolute start offset and size */
        area[i]->i_start = (off_t)p_vcd->p_sectors[i] * (off_t)VCD_DATA_SIZE;
        area[i]->i_size = (off_t)(p_vcd->p_sectors[i+1] - p_vcd->p_sectors[i])
                           * (off_t)VCD_DATA_SIZE;

        /* Number of chapters */
        area[i]->i_part_nb = 0;   // will be the entry points
        area[i]->i_part = 1;

        /* Number of angles */
        area[i]->i_angle_nb = 1; // no angle support in VCDs
        area[i]->i_angle = 1;

        area[i]->i_plugin_data = p_vcd->p_sectors[i];
    }
#undef area

    /* Get requested title - if none try the first title */
    i_title = main_GetIntVariable( INPUT_TITLE_VAR, 1 );
    if( i_title <= 0 )
    {
        i_title = 1;
    }

    // p_vcd->i_track = i_title-1;

    /* Get requested chapter - if none defaults to first one */
    i_chapter = main_GetIntVariable( INPUT_CHAPTER_VAR, 1 );
    if( i_chapter <= 0 )
    {
        i_chapter = 1;
    }

    p_input->stream.pp_areas[i_title]->i_part = i_chapter;

    p_area = p_input->stream.pp_areas[i_title];

    VCDSetArea( p_input, p_area );

    /* Set program information. */

    input_AddProgram( p_input, 0, sizeof( stream_ps_data_t ) );
    p_input->stream.p_selected_program = p_input->stream.pp_programs[0];

    /* No PSM to read in disc mode, we already have all the information */
    p_input->stream.p_selected_program->b_is_ok = 1;

    p_es = input_AddES( p_input, p_input->stream.p_selected_program, 0xe0, 0 );
    p_es->i_stream_id = 0xe0;
    p_es->i_type = MPEG1_VIDEO_ES;
    p_es->i_cat = VIDEO_ES;

    if( p_main->b_video )
    {
        input_SelectES( p_input, p_es );
    }

    p_es = input_AddES( p_input, p_input->stream.p_selected_program, 0xc0, 0 );
    p_es->i_stream_id = 0xc0;
    p_es->i_type = MPEG1_AUDIO_ES;
    p_es->b_audio = 1;
    p_es->i_cat = AUDIO_ES;

    if( p_main->b_audio )
    {
        input_SelectES( p_input, p_es );
    }

    vlc_mutex_unlock( &p_input->stream.stream_lock );
}

/*****************************************************************************
 * VCDEnd: frees unused data
 *****************************************************************************/
static void VCDEnd( input_thread_t * p_input )
{
    thread_vcd_data_t *     p_vcd;

    input_BuffersEnd( p_input->p_method_data );

    p_vcd = (thread_vcd_data_t*)p_input->p_plugin_data;

    free( p_vcd );
}

/*****************************************************************************
 * VCDSetProgram: Does nothing since a VCD is mono_program
 *****************************************************************************/
static int VCDSetProgram( input_thread_t * p_input,
                          pgrm_descriptor_t * p_program)
{
    return 0;
}


/*****************************************************************************
 * VCDSetArea: initialize input data for title x, chapter y.
 * It should be called for each user navigation request.
 ****************************************************************************/
static int VCDSetArea( input_thread_t * p_input, input_area_t * p_area )
{
    thread_vcd_data_t *     p_vcd;

    p_vcd = (thread_vcd_data_t*)p_input->p_plugin_data;

    /* we can't use the interface slider until initilization is complete */
    p_input->stream.b_seekable = 0;

    if( p_area != p_input->stream.p_selected_area )
    {
        /* Reset the Chapter position of the current title */
        p_input->stream.p_selected_area->i_part = 1;
        p_input->stream.p_selected_area->i_tell = 0;

        /* Change the default area */
        p_input->stream.p_selected_area = p_area;

        /* Change the current track */
        /* The first track is not a valid one  */
        p_vcd->i_track = p_area->i_id;
        p_vcd->i_sector = p_vcd->p_sectors[p_vcd->i_track];
    }

    /* warn interface that something has changed */
    p_input->stream.b_seekable = 1;
    p_input->stream.b_changed = 1;

    return 0;
}

/*****************************************************************************
 * VCDRead: reads from the VCD into PES packets.
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, otherwise the number of
 * packets.
 *****************************************************************************/
static int VCDRead( input_thread_t * p_input, data_packet_t ** pp_data )
{
    thread_vcd_data_t *     p_vcd;
    data_packet_t *         p_data;
    int                     i_packet_size;
    int                     i_index;
    int                     i_packet;
    u32                     i_header;
    byte_t                  p_buffer[ VCD_DATA_SIZE ];
    boolean_t               b_eot = 0; /* end of track */
    /* boolean_t               b_eoc; No chapters yet */

    p_vcd = (thread_vcd_data_t *)p_input->p_plugin_data;

    i_packet = 0;
    *pp_data = NULL;

    while( i_packet < VCD_DATA_ONCE )
    {
        if( ioctl_ReadSector( p_vcd->i_handle, p_vcd->i_sector, p_buffer ) )
        {
            /* Read error, but assume we reached the end of the track */
            b_eot = 1;
            break;
        }

        p_vcd->i_sector++;

        if( p_vcd->i_sector >= p_vcd->p_sectors[p_vcd->i_track + 1] )
        {
            b_eot = 1;
            break;
        }

        i_index = 0;

        while( i_index < BUFFER_SIZE-6 && i_packet < VCD_DATA_ONCE )
        {
            i_header = U32_AT(p_buffer + i_index);

            /* It is common for MPEG-1 streams to pad with zeros
             * (although it is forbidden by the recommendation), so
             * don't bother everybody in this case. */
            while( !i_header && (++i_index < BUFFER_SIZE - 4) )
            {
                i_header = U32_AT(p_buffer + i_index);
            }

            if( !i_header )
            {
                intf_WarnMsg( 12, "vcd warning: zero-padded packet" );
                break;
            }

            /* Read the stream until we find a startcode. */
            while( (i_header & 0xFFFFFF00) != 0x100L
                     && (++i_index < BUFFER_SIZE - 4) )
            {
                i_header = U32_AT(p_buffer + i_index);
            }

            if( (i_header & 0xFFFFFF00) != 0x100L )
            {
                intf_WarnMsg( 3, "vcd warning: no packet at sector %d",
                                 p_vcd->i_sector - 1 );
                break; /* go to the next sector */
            }

            intf_DbgMsg( "packet start code : %X", i_header );

            switch( i_header )
            {
                /* 0x1b9 == SYSTEM_END_CODE, it is only 4 bytes long. */
                case 0x1b9:
                    i_packet_size = -2;
                    break;

                /* Pack header */
                case 0x1ba:
                    if( ( *( p_buffer + ( i_index + 4 ) ) & 0xC0) == 0x40 )
                    {
                        /* MPEG-2 */
                        i_packet_size = 8;
                    }
                    else if( (*(p_buffer + ( i_index + 4 ) ) & 0xF0) == 0x20 )
                    {
                        /* MPEG-1 */
                        i_packet_size = 6;
                    }
                    else
                    {
                        intf_ErrMsg( "vcd error: unable to determine "
                                     "stream type" );
                        return( -1 );
                    }
                    break;

                /* The packet is at least 6 bytes long. */
                default:
                    /* That's the case for all packets, except pack header. */
                    i_packet_size = U16_AT((p_buffer + ( i_index + 4 )));
                    break;
            }

            intf_DbgMsg( "i_index : %d", i_index );
            intf_DbgMsg( "i_packet_size : %d", i_packet_size );

            if ( i_index + i_packet_size > BUFFER_SIZE )
            {
                intf_ErrMsg( "vcd error: packet too long (%i)",
                             i_index + i_packet_size );
                continue;
            }

            /* Fetch a packet of the appropriate size. */
            p_data = p_input->pf_new_packet( p_input->p_method_data,
                                             i_packet_size + 6 );

            if( p_data == NULL )
            {
                intf_ErrMsg( "vcd error: out of memory" );
                return( -1 );
            }

            if( U32_AT(p_buffer) != 0x1B9 )
            {
                FAST_MEMCPY( p_data->p_demux_start, p_buffer + i_index,
                             6 + i_packet_size );
                i_index += ( 6 + i_packet_size );
            }
            else
            {
                /* Copy the small header. */
                memcpy( p_data->p_demux_start, p_buffer + i_index, 4 );
                i_index += 4;
            }

            /* Give the packet to the other input stages. */
            *pp_data = p_data;
            pp_data = &p_data->p_next;

            i_packet++;
        }
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );

    p_input->stream.p_selected_area->i_tell =
        (off_t)p_vcd->i_sector * (off_t)VCD_DATA_SIZE
          - p_input->stream.p_selected_area->i_start;

    /* no chapter for the moment*/
#if 0
    if( b_eoc )
    {
        /* We modify i_part only at end of chapter not to erase
         * some modification from the interface */
        p_input->stream.p_selected_area->i_part = p_vcd->i_chapter;
    }
#endif

    if( b_eot )
    {
        input_area_t *p_area;

        /* EOF ? */
        if( p_vcd->i_track >= p_vcd->nb_tracks - 1 )
        {
            vlc_mutex_unlock( &p_input->stream.stream_lock );
            return 0;
        }

        intf_WarnMsg( 4, "vcd info: new title" );

        p_area = p_input->stream.pp_areas[
                                 p_input->stream.p_selected_area->i_id + 1 ];

        p_area->i_part = 1;
        VCDSetArea( p_input, p_area );
    }

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return( i_packet + 1 );
}

/*****************************************************************************
 * VCDRewind : reads a stream backward
 *****************************************************************************/
static int VCDRewind( input_thread_t * p_input )
{
    return( -1 );
}

/****************************************************************************
 * VCDSeek
 ****************************************************************************/
static void VCDSeek( input_thread_t * p_input, off_t i_off )
{
    thread_vcd_data_t *               p_vcd;

    p_vcd = (thread_vcd_data_t *) p_input->p_plugin_data;

    p_vcd->i_sector = p_vcd->p_sectors[p_vcd->i_track]
                       + i_off / (off_t)VCD_DATA_SIZE;

    p_input->stream.p_selected_area->i_tell = 
        (off_t)p_vcd->i_sector * (off_t)VCD_DATA_SIZE
         - p_input->stream.p_selected_area->i_start;
}

