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
#include "cdrom_tools.h"

/* how many blocks VCDRead will read in each loop */
#define VCD_BLOCKS_ONCE 20
#define VCD_DATA_ONCE   (VCD_BLOCKS_ONCE * VCD_DATA_SIZE)

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
/* called from outside */
static int  VCDInit         ( struct input_thread_s * );
static void VCDEnd          ( struct input_thread_s * );
static int  PSDemux        ( struct input_thread_s * );
static int  VCDRewind       ( struct input_thread_s * );

static int  VCDOpen         ( struct input_thread_s *);
static void VCDClose        ( struct input_thread_s *);
static int  VCDRead         ( struct input_thread_s *, byte_t *, size_t );
static void VCDSeek         ( struct input_thread_s *, off_t );
static int  VCDSetArea      ( struct input_thread_s *, struct input_area_s * );
static int  VCDSetProgram   ( struct input_thread_s *, pgrm_descriptor_t * );

static ssize_t PSRead       ( struct input_thread_s *, data_packet_t ** );
/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( access_getfunctions )( function_list_t * p_function_list )
{
#define access p_function_list->functions.access
    access.pf_open             = VCDOpen;
    access.pf_close            = VCDClose;
    access.pf_read             = VCDRead;
    access.pf_set_area         = VCDSetArea;
    access.pf_set_program      = VCDSetProgram;
    access.pf_seek             = VCDSeek;
#undef access
}


void _M( demux_getfunctions )( function_list_t * p_function_list )
{
#define demux p_function_list->functions.demux
    demux.pf_init             = VCDInit;
    demux.pf_end              = VCDEnd;
    demux.pf_demux            = PSDemux;
    demux.pf_rewind           = VCDRewind;
#undef demux
}

/*
 * Data reading functions
 */

/*****************************************************************************
 * VCDOpen: open vcd
 *****************************************************************************/
static int VCDOpen( struct input_thread_s *p_input )
{
    char *                  psz_orig;
    char *                  psz_parser;
    char *                  psz_source;
    char *                  psz_next;
    thread_vcd_data_t *  p_vcd;
    int                  i;
    input_area_t *       p_area;
    int                  i_title = 1;
    int                  i_chapter = 1;

    p_vcd = malloc( sizeof(thread_vcd_data_t) );

    if( p_vcd == NULL )
    {
        intf_ErrMsg( "vcd error: out of memory" );
        p_input->b_error = 1;
        return -1;
    }

    p_input->i_mtu = VCD_DATA_ONCE;
    p_input->p_access_data = (void *)p_vcd;

    /* parse the options passed in command line : */
    psz_orig = psz_parser = psz_source = strdup( p_input->psz_name );
    
    if( !psz_orig )
    {
        return( -1 );
    }
 
    while( *psz_parser && *psz_parser != '@' )
    {
        psz_parser++;
    }

    if( *psz_parser == '@' )
    {
        /* Found options */
        *psz_parser = '\0';
        ++psz_parser;

        i_title = (int)strtol( psz_parser, &psz_next, 10 );
        if( *psz_next )
        {
            psz_parser = psz_next + 1;
            i_chapter = (int)strtol( psz_parser, &psz_next, 10 );
        }

        i_title = i_title ? i_title : 1;
        i_chapter = i_chapter ? i_chapter : 1;
    }

    if( !*psz_source )
    {
        if( !p_input->psz_access )
        {
            free( psz_orig );
            return -1;
        }
        psz_source = config_GetPszVariable( INPUT_VCD_DEVICE_VAR );
    }
 
    vlc_mutex_lock( &p_input->stream.stream_lock );

    /* If we are here we can control the pace... */
    p_input->stream.b_pace_control = 1;

    p_input->stream.b_seekable = 1;
    p_input->stream.p_selected_area->i_size = 0;
    p_input->stream.p_selected_area->i_tell = 0;

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    p_vcd->i_handle = open( psz_source, O_RDONLY | O_NONBLOCK );

    if( p_vcd->i_handle == -1 )
    {
        intf_ErrMsg( "input: vcd: Could not open %s\n", psz_source );
        p_input->b_error = 1;
        free (p_vcd);
        return -1;
    }

    /* We read the Table Of Content information */
    p_vcd->nb_tracks = ioctl_GetTrackCount( p_vcd->i_handle,
                                            psz_source );
    if( p_vcd->nb_tracks < 0 )
    {
        intf_ErrMsg( "input: vcd: was unable to count tracks" );
        free( p_vcd );
        p_input->b_error = 1;
        return -1;
    }
    else if( p_vcd->nb_tracks <= 1 )
    {
        intf_ErrMsg( "input: vcd: no movie tracks found" );
        free( p_vcd );
        p_input->b_error = 1;
        return -1;
    }

    p_vcd->p_sectors = ioctl_GetSectors( p_vcd->i_handle,
                                         psz_source );
    if ( p_vcd->p_sectors == NULL )
    {
        input_BuffersEnd( p_input->p_method_data );
        free( p_vcd );
        p_input->b_error = 1;
        return -1;
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

        area[i]->i_plugin_data = p_vcd->p_sectors[i];
    }
#undef area

    p_area = p_input->stream.pp_areas[i_title];

    VCDSetArea( p_input, p_area );

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return 0;
}

    

/*****************************************************************************
 * VCDClose: closes vcd
 *****************************************************************************/
static void VCDClose( struct input_thread_s *p_input )
{
    thread_vcd_data_t *     p_vcd
        = (thread_vcd_data_t *)p_input->p_access_data;
    close( p_vcd->i_handle );
    free( p_vcd );
}

/*****************************************************************************
 * VCDRead: reads from the VCD into PES packets.
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, otherwise the number of
 * bytes.
 *****************************************************************************/
static int VCDRead( input_thread_t * p_input, byte_t * p_buffer, 
                     size_t i_len )
{
    thread_vcd_data_t *     p_vcd;
    int                     i_blocks;
    int                     i_index;
    int                     i_read;
    byte_t                  p_last_sector[ VCD_DATA_SIZE ];

    p_vcd = (thread_vcd_data_t *)p_input->p_access_data;

    i_read = 0;

    /* Compute the number of blocks we have to read */

    i_blocks = i_len / VCD_DATA_SIZE;

    for ( i_index = 0 ; i_index < i_blocks ; i_index++ ) 
    {
        if ( ioctl_ReadSector( p_vcd->i_handle, p_vcd->i_sector, 
                    p_buffer + i_index * VCD_DATA_SIZE ) < 0 )
        {
            intf_ErrMsg( "input: vcd: could not read sector %d\n", 
                    p_vcd->i_sector );
            return -1;
        }

        p_vcd->i_sector ++;
        if ( p_vcd->i_sector == p_vcd->p_sectors[p_vcd->i_track + 1] )
        {
            /* FIXME we should go to next track */
            return 0;
        }
        i_read += VCD_DATA_SIZE;
    }
    
    if ( i_len % VCD_DATA_SIZE ) /* this should not happen */
    { 
        if ( ioctl_ReadSector( p_vcd->i_handle, p_vcd->i_sector, 
                    p_last_sector ) < 0 )
        {
            intf_ErrMsg( "input: vcd: could not read sector %d\n", 
                    p_vcd->i_sector );
            return -1;
        }
        
        FAST_MEMCPY( p_buffer + i_blocks * VCD_DATA_SIZE,
                    p_last_sector, i_len % VCD_DATA_SIZE );
        i_read += i_len % VCD_DATA_SIZE;
    }
    
    p_input->stream.p_selected_area->i_tell = 
        (off_t)p_vcd->i_sector * (off_t)VCD_DATA_SIZE
         - p_input->stream.p_selected_area->i_start;

    return i_read;
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

    p_vcd = (thread_vcd_data_t*)p_input->p_access_data;

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

    p_vcd = (thread_vcd_data_t *) p_input->p_access_data;

    p_vcd->i_sector = p_vcd->p_sectors[p_vcd->i_track]
                       + i_off / (off_t)VCD_DATA_SIZE;

    p_input->stream.p_selected_area->i_tell = 
        (off_t)p_vcd->i_sector * (off_t)VCD_DATA_SIZE
         - p_input->stream.p_selected_area->i_start;
}

/*
 * Demux functions
 */


/*****************************************************************************
 * VCDInit: initializes VCD structures
 *****************************************************************************/
static int VCDInit( input_thread_t * p_input )
{
    es_descriptor_t *       p_es;
    
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
    
    return 0;
}

/*****************************************************************************
 * VCDEnd: frees unused data
 *****************************************************************************/
static void VCDEnd( input_thread_t * p_input )
{
    thread_vcd_data_t *     p_vcd;

    input_BuffersEnd( p_input->p_method_data );

    p_vcd = (thread_vcd_data_t*)p_input->p_access_data;

    free( p_vcd );
}


/*****************************************************************************
 * The following functions are extracted from mpeg_ps, they should soon
 * be placed in mpeg_system.c so that plugins can use them.
 *****************************************************************************/

/*****************************************************************************
 * PSRead: reads one PS packet
 *****************************************************************************/
#define PEEK( SIZE )                                                        \
    i_error = input_Peek( p_input, &p_peek, SIZE );                         \
    if( i_error == -1 )                                                     \
    {                                                                       \
        return( -1 );                                                       \
    }                                                                       \
    else if( i_error < SIZE )                                               \
    {                                                                       \
        /* EOF */                                                           \
        return( 0 );                                                        \
    }

static __inline__ ssize_t PSRead( input_thread_t * p_input,
                                  data_packet_t ** pp_data )
{
    byte_t *            p_peek;
    size_t              i_packet_size;
    ssize_t             i_error, i_read;

    /* Read what we believe to be a packet header. */
    PEEK( 4 );

    if( *p_peek || *(p_peek + 1) || *(p_peek + 2) != 1 )
    {
        if( *p_peek || *(p_peek + 1) || *(p_peek + 2) )
        {
            /* It is common for MPEG-1 streams to pad with zeros
             * (although it is forbidden by the recommendation), so
             * don't bother everybody in this case. */
            intf_WarnMsg( 3, "input warning: garbage at input (0x%x%x%x%x)",
                 *p_peek, *(p_peek + 1), *(p_peek + 2), *(p_peek + 3) );
        }

        /* This is not the startcode of a packet. Read the stream
         * until we find one. */
        while( *p_peek || *(p_peek + 1) || *(p_peek + 2) != 1 )
        {
            p_input->p_current_data++;
            PEEK( 4 );
        }
        /* Packet found. */
    }

    /* 0x1B9 == SYSTEM_END_CODE, it is only 4 bytes long. */
    if( p_peek[3] != 0xB9 )
    {
        /* The packet is at least 6 bytes long. */
        PEEK( 6 );

        if( p_peek[3] != 0xBA )
        {
            /* That's the case for all packets, except pack header. */
            i_packet_size = (p_peek[4] << 8) | p_peek[5];
        }
        else
        {
            /* Pack header. */
            if( (p_peek[4] & 0xC0) == 0x40 )
            {
                /* MPEG-2 */
                i_packet_size = 8;
            }
            else if( (p_peek[4] & 0xF0) == 0x20 )
            {
                /* MPEG-1 */
                i_packet_size = 6;
            }
            else
            {
                intf_ErrMsg( "Unable to determine stream type" );
                return( -1 );
            }
        }
    }
    else
    {
        /* System End Code */
        i_packet_size = -2;
    }

    /* Fetch a packet of the appropriate size. */
    i_read = input_SplitBuffer( p_input, pp_data, i_packet_size + 6 );
    if( i_read <= 0 )
    {
        return( i_read );
    }

    /* In MPEG-2 pack headers we still have to read stuffing bytes. */
    if( ((*pp_data)->p_demux_start[3] == 0xBA) && (i_packet_size == 8) )
    {
        size_t i_stuffing = ((*pp_data)->p_demux_start[13] & 0x7);
        /* Force refill of the input buffer - though we don't care
         * about p_peek. Please note that this is unoptimized. */
        PEEK( i_stuffing );
        p_input->p_current_data += i_stuffing;
    }

    return( 1 );
}

/*****************************************************************************
 * PSDemux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, otherwise the number of
 * packets.
 *****************************************************************************/
static int PSDemux( input_thread_t * p_input )
{
    int                 i;

    for( i = 0; i < VCD_BLOCKS_ONCE; i++ )
    {
        data_packet_t *     p_data;
        ssize_t             i_result;

        i_result = PSRead( p_input, &p_data );

        if( i_result <= 0 )
        {
            return( i_result );
        }

        input_DemuxPS( p_input, p_data );
    }

    return( i );
}

