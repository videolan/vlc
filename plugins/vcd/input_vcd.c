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

#define MODULE_NAME vcd
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdio.h>
#include <stdlib.h>

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


#include "config.h"
#include "common.h"
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#if defined( WIN32 )
#   include "input_iovec.h"
#endif

#include "main.h"

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"
#include "input_ext-plugins.h"

#include "debug.h"

#include "modules.h"
#include "modules_export.h"

#include "../mpeg/input_ps.h"
#include "input_vcd.h"
#include "linux_cdrom_tools.h"

/* how many blocks VCDRead will read in each loop */
#define VCD_BLOCKS_ONCE 64
#define VCD_DATA_ONCE  (2 * VCD_BLOCKS_ONCE)
#define BUFFER_SIZE VCD_DATA_SIZE



/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
/* called from outside */
static int  VCDProbe     ( probedata_t *p_data );
static void VCDInit      ( struct input_thread_s * );
static int  VCDRead      ( struct input_thread_s *, data_packet_t ** );
static int  VCDSetArea   ( struct input_thread_s *, struct input_area_s * ); 
static void VCDOpen      ( struct input_thread_s *);
static void VCDClose     ( struct input_thread_s *);
static void VCDEnd       ( struct input_thread_s *);
static void VCDSeek      ( struct input_thread_s *, off_t );
static int  VCDRewind    ( struct input_thread_s * );
static struct data_packet_s * NewPacket( void *, size_t );
static pes_packet_t *         NewPES   ( void * );
static void DeletePacket ( void *, data_packet_t * );
static void DeletePES    ( void *, pes_packet_t *);



/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( input_getfunctions )( function_list_t * p_function_list )
{
#define input p_function_list->functions.input
    p_function_list->pf_probe = VCDProbe ;
    input.pf_init             = VCDInit ;
    input.pf_open             = VCDOpen ;
    input.pf_close            = VCDClose ;
    input.pf_end              = VCDEnd ;
    input.pf_init_bit_stream  = InitBitstream;
    input.pf_read             = VCDRead ;
    input.pf_set_area         = VCDSetArea ;
    input.pf_demux            = input_DemuxPS;
    input.pf_new_packet       = NewPacket;
    input.pf_new_pes          = NewPES;
    input.pf_delete_packet    = DeletePacket;
    input.pf_delete_pes       = DeletePES;
    input.pf_rewind           = VCDRewind ;
    input.pf_seek             = VCDSeek ;
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
    int i_score = 5;

    if( TestMethod( INPUT_METHOD_VAR, "vcd" ) )
    {
        return( 999 );
    }

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
    int vcdhandle;

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
        vcdhandle = open( p_input->p_source + 4, O_RDONLY | O_NONBLOCK );
    }
    else
    {
        vcdhandle = open( p_input->p_source + 4, O_RDONLY | O_NONBLOCK );
    }

    if( vcdhandle == -1 )
    {
        p_input->b_error = 1;
        return;
    }

    p_input->i_handle = (int) vcdhandle;
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
    packet_cache_t *     p_packet_cache;
    
    p_vcd = malloc( sizeof(thread_vcd_data_t) );
        
    if( p_vcd == NULL )
    {
        intf_ErrMsg( "vcd error: out of memory" );
        p_input->b_error = 1;
        return;
    }

    
    
    p_input->p_plugin_data = (void *)p_vcd;
    p_input->p_method_data = NULL;

    p_vcd->vcdhandle = p_input->i_handle;
    p_vcd->b_end_of_track = 0;

    /* we read the Table Of Content information */
    if ( read_toc(p_vcd) == -1 )
    {
        intf_ErrMsg("An error occured when reading vcd's TOC");
    }
    
    p_input->i_read_once = VCD_DATA_ONCE;
    
    p_packet_cache = malloc( sizeof(packet_cache_t) );
    
    if ( p_packet_cache == NULL )
    {
        intf_ErrMsg( "vcd error: out of memory" );
        p_input->b_error = 1;
        return;
    }
    
    p_input->p_method_data = (void *)p_packet_cache;
     /* Initialize packet cache mutex */
    vlc_mutex_init( &p_packet_cache->lock );
    
    /* allocates the data cache */
    p_packet_cache->data.p_stack = malloc( DATA_CACHE_SIZE * 
        sizeof(data_packet_t*) );
    if ( p_packet_cache->data.p_stack == NULL )
    {
        intf_ErrMsg( "Out of memory" );
        p_input->b_error = 1;
        return;
    }
    p_packet_cache->data.l_index = 0;
    
    /* allocates the PES cache */
    p_packet_cache->pes.p_stack = malloc( PES_CACHE_SIZE * 
        sizeof(pes_packet_t*) );
    if ( p_packet_cache->pes.p_stack == NULL )
    {
        intf_ErrMsg( "Out of memory" );
        p_input->b_error = 1;
        return;
    }
    p_packet_cache->pes.l_index = 0;
    
    /* allocates the small buffer cache */
    p_packet_cache->smallbuffer.p_stack = malloc( SMALL_CACHE_SIZE * 
        sizeof(packet_buffer_t) );
    if ( p_packet_cache->smallbuffer.p_stack == NULL )
    {
        intf_ErrMsg( "Out of memory" );
        p_input->b_error = 1;
        return;
    }
    p_packet_cache->smallbuffer.l_index = 0;
    
    /* allocates the large buffer cache */
    p_packet_cache->largebuffer.p_stack = malloc( LARGE_CACHE_SIZE * 
        sizeof(packet_buffer_t) );
    if ( p_packet_cache->largebuffer.p_stack == NULL )
    {
        intf_ErrMsg( "Out of memory" );
        p_input->b_error = 1;
        return;
    }
    p_packet_cache->largebuffer.l_index = 0;

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
        area[i]->i_start = p_vcd->tracks_sector[i+1];
        area[i]->i_size = p_vcd->tracks_sector[i+2] - p_vcd->tracks_sector[i+1];

        /* Number of chapters */
        area[i]->i_part_nb = 0;   // will be the entry points
        area[i]->i_part = 1;

        /* Number of angles */
        area[i]->i_angle_nb = 1; // no angle support in VCDs
        area[i]->i_angle = 1;

        area[i]->i_plugin_data = p_vcd->tracks_sector[i+1];
    }   
#undef area

    /* Get requested title - if none try the first title */
    i_title = main_GetIntVariable( INPUT_TITLE_VAR, 1 );
    if( i_title <= 0)
    {
        i_title = 2;
    }
    
    // p_vcd->current_track = i_title-1 ;
    
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

    /* No PSM to read in disc mode, we already have all information */
    p_input->stream.p_selected_program->b_is_ok = 1;

    p_es = input_AddES( p_input, p_input->stream.p_selected_program, 0xe0, 0 );
    p_es->i_stream_id = 0xe0;
    p_es->i_type = MPEG1_VIDEO_ES;
    p_es->i_cat = VIDEO_ES;
    
    if( p_main->b_video )
    {
        input_SelectES( p_input, p_es );
    }
    
    p_es = input_AddES( p_input,
                p_input->stream.p_selected_program, 0xc0, 0 );
    p_es->i_stream_id = 0xc0;
    p_es->i_type = MPEG1_AUDIO_ES;
    p_es->b_audio = 1;
    p_es->i_cat = AUDIO_ES;
     
    if( p_main->b_audio )
    {
        input_SelectES( p_input, p_es );
    }
    
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    
    return;
}


/*****************************************************************************
 * VCDEnd: frees unused data
 *****************************************************************************/
static void VCDEnd( input_thread_t * p_input )
{
    thread_vcd_data_t *     p_vcd;

    p_vcd = (thread_vcd_data_t*)p_input->p_plugin_data;

    free( p_vcd );

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
    
    if ( p_area != p_input->stream.p_selected_area ) 
    {
        /* Reset the Chapter position of the old title */
        p_input->stream.p_selected_area->i_part = 0;
        
        /* Change the default area */
        p_input->stream.p_selected_area =
                    p_input->stream.pp_areas[p_area->i_id];

        /* Change the current track */
        /* The first track is not a valid one  */
        p_vcd->current_track = p_area->i_id + 1 ;
        p_vcd->current_sector = p_vcd->tracks_sector[p_vcd->current_track] ;
    }
    /* warn interface that something has changed */
    p_input->stream.b_seekable = 1;
    p_input->stream.b_changed = 1;
    return 0 ;
    
}



/*****************************************************************************
 * VCDRead: reads from the VCD into PES packets.
 *****************************************************************************
 * Returns -1 in case of error, 0 if everything went well, and 1 in case of
 * EOF.
 *****************************************************************************/
static int VCDRead( input_thread_t * p_input,
                    data_packet_t ** pp_packets )
{
    thread_vcd_data_t *     p_vcd;
    data_packet_t *         p_data;
    int                     i_packet_size;
    int                     i_index;
    int                     i_packet;   
    boolean_t               b_eof;
    byte_t *                p_buffer;
    boolean_t                  b_no_packet;
    /* boolean_t               b_eoc; No chapters yet */
    
    p_vcd = (thread_vcd_data_t *)p_input->p_plugin_data;
   

    p_buffer = malloc ( VCD_DATA_SIZE );

    if ( p_buffer == NULL )
    {
        intf_ErrMsg("Could not malloc the read buffer");
        return -1;
    }

    
    i_packet = 0;
    b_no_packet = 0;

    while( i_packet < VCD_DATA_ONCE ) 
    {
        i_index = 0;
        
        if ( VCD_sector_read( p_vcd, p_buffer ) == -1 )
        {
              return -1;
        }
        
        while (i_index < BUFFER_SIZE - 6) 
        {
            
            if( (U32_AT(p_buffer + i_index) & 0xFFFFFF00) != 0x100L )
            {
                /* This is not the startcode of a packet. Read the stream
                 * until we find one. */
    
                if( !U32_AT( p_buffer + i_index ) )
                {
                    /* It is common for MPEG-1 streams to pad with zeros
                     * (although it is forbidden by the recommendation), so
                     * don't bother everybody in this case. */
                    intf_WarnMsg( 3, "Garbage at input" );
                }
    
                while( ( (U32_AT(p_buffer + i_index) & 0xFFFFFF00) != 0x100L )
                       && ( i_index < BUFFER_SIZE - 4 ) )
                {
                    i_index ++;
                }
    
                if ( i_index == BUFFER_SIZE - 4 )
                {
                    b_no_packet = 1;
                }
                /* Packet found. */
            }
            
            if (b_no_packet)
            {
                b_no_packet = 0;
                intf_WarnMsg(3, "No packet found on sector %d\n", 
                            p_vcd->current_sector -1 );
                break; /* go to the next sector */
            }
            
#ifdef DEBUG
            intf_DbgMsg("packet start code : %X\n", 
                        U32_AT(p_buffer + i_index));
#endif
            /* 0x1B9 == SYSTEM_END_CODE, it is only 4 bytes long. */
            if( U32_AT(p_buffer + i_index) != 0x1B9 )
            {
                /* The packet is at least 6 bytes long. */
    
                if( U32_AT(p_buffer + i_index) != 0x1BA )
                {
                    /* That's the case for all packets, except pack header. */
                    i_packet_size = U16_AT((p_buffer + ( i_index + 4 )));
                }
                else
                {
                    /* Pack header. */
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
#ifdef DEBUG
            intf_DbgMsg("i_index : %d\n", i_index);
            intf_DbgMsg("i_packet_size : %d\n", i_packet_size);
#endif
            if ( i_index + i_packet_size > BUFFER_SIZE )
            {
                intf_ErrMsg( "Too long packet");
                continue;
            }
            
            /* Fetch a packet of the appropriate size. */
            
            p_data = NewPacket( p_input->p_method_data, i_packet_size + 6 );
            
            if( p_data == NULL )
            {
                intf_ErrMsg( "Out of memory" );
                return( -1 );
            }
    
            if( U32_AT(p_buffer) != 0x1B9 )
            {
                pf_fast_memcpy( p_data->p_buffer, p_buffer + i_index,
                                6 + i_packet_size );
                i_index += ( 6 + i_packet_size );
    
            }
            else
            {
                /* Copy the small header. */
                memcpy( p_data->p_buffer, p_buffer + i_index, 4 );
                i_index += 4;
            }
    
            /* Give the packet to the other input stages. */
            pp_packets[i_packet] = p_data;
            i_packet ++;
        }
        
        if ( p_vcd->b_end_of_track )
            break;
    }





    
    vlc_mutex_lock( &p_input->stream.stream_lock );

    p_input->stream.p_selected_area->i_tell =
        p_vcd->current_sector -
        p_input->stream.p_selected_area->i_start ;
    
    /* no chapter for the moment*/
    /*if( b_eoc )
    {
        * We modify i_part only at end of chapter not to erase
         * some modification from the interface *
        p_input->stream.p_selected_area->i_part = p_vcd->i_chapter;
    }*/

    
    b_eof = p_vcd->b_end_of_track && 
            ( ( p_vcd->current_track + 1 ) >= p_vcd->nb_tracks );

    if( b_eof )
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        return 1;
    }

    if( p_vcd->b_end_of_track )
    {
        intf_WarnMsg( 4, "vcd info: new title" );
        p_vcd->current_track++;
        p_vcd->b_end_of_track = 0;
        VCDSetArea( p_input, p_input->stream.pp_areas[p_vcd->current_track] );
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        return 0;
    }
    
    vlc_mutex_unlock( &p_input->stream.stream_lock );


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

    p_vcd = (thread_vcd_data_t *) p_input->p_plugin_data;

    p_vcd->current_sector = p_vcd->tracks_sector[p_vcd->current_track]
                                + i_off;

    p_input->stream.p_selected_area->i_tell = p_vcd->current_sector
        - p_input->stream.p_selected_area->i_start;
    
    return ;
}



/*
 * Packet management utilities
 */


/*****************************************************************************
 * NewPacket: allocates a data packet
 *****************************************************************************/
static struct data_packet_s * NewPacket( void * p_packet_cache,
                                         size_t l_size )
{ 
    packet_cache_t *   p_cache;
    data_packet_t *    p_data;
    long               l_index;

    p_cache = (packet_cache_t *)p_packet_cache;

#ifdef DEBUG
    if ( p_cache == NULL )
    {
        intf_ErrMsg( "PPacket cache not initialized" );
        return NULL;
    }
#endif

    /* Safety check */
    if( l_size > INPUT_MAX_PACKET_SIZE )
    {
        intf_ErrMsg( "Packet too big (%d)", l_size );
        return NULL;
    }

    vlc_mutex_lock( &p_cache->lock );

    /* Checks whether the data cache is empty */
    if( p_cache->data.l_index == 0 )
    {
        /* Allocates a new packet */
        p_data = malloc( sizeof(data_packet_t) );
        if( p_data == NULL )
        {
            intf_ErrMsg( "Out of memory" );
            vlc_mutex_unlock( &p_cache->lock );
            return NULL;
        }
#ifdef TRACE_INPUT
        intf_DbgMsg( "PS input: data packet allocated" );
#endif
    }
    else
    {
        /* Takes the packet out from the cache */
        if( (p_data = p_cache->data.p_stack[ -- p_cache->data.l_index ]) 
            == NULL )
        {
            intf_ErrMsg( "NULL packet in the data cache" );
            vlc_mutex_unlock( &p_cache->lock );
            return NULL;
        }
    }
    
    if( l_size < MAX_SMALL_SIZE )
    {
        /* Small buffer */  
   
        /* Checks whether the buffer cache is empty */
        if( p_cache->smallbuffer.l_index == 0 )
        {
            /* Allocates a new packet */
            p_data->p_buffer = malloc( l_size );
            if( p_data->p_buffer == NULL )
            {
                intf_DbgMsg( "Out of memory" );
                free( p_data );
                vlc_mutex_unlock( &p_cache->lock );
                return NULL;
            }
#ifdef TRACE_INPUT
            intf_DbgMsg( "PS input: small buffer allocated" );
#endif
            p_data->l_size = l_size;
        }
        else
        {
            /* Takes the packet out from the cache */
            l_index = -- p_cache->smallbuffer.l_index;    
            if( (p_data->p_buffer = p_cache->smallbuffer.p_stack[l_index].p_data)
                == NULL )
            {
                intf_ErrMsg( "NULL packet in the small buffer cache" );
                free( p_data );
                vlc_mutex_unlock( &p_cache->lock );
                return NULL;
            }
            /* Reallocates the packet if it is too small or too large */
            if( p_cache->smallbuffer.p_stack[l_index].l_size < l_size ||
                p_cache->smallbuffer.p_stack[l_index].l_size > 2*l_size )
            {
                p_data->p_buffer = realloc( p_data->p_buffer, l_size );
                p_data->l_size = l_size;
            }
            else
            {
                p_data->l_size = p_cache->smallbuffer.p_stack[l_index].l_size;
            }
        }
    }
    else
    {
        /* Large buffer */  
   
        /* Checks whether the buffer cache is empty */
        if( p_cache->largebuffer.l_index == 0 )
        {
            /* Allocates a new packet */
            p_data->p_buffer = malloc( l_size );
            if ( p_data->p_buffer == NULL )
            {
                intf_ErrMsg( "Out of memory" );
                free( p_data );
                vlc_mutex_unlock( &p_cache->lock );
                return NULL;
            }
#ifdef TRACE_INPUT
            intf_DbgMsg( "PS input: large buffer allocated" );
#endif
            p_data->l_size = l_size;
        }
        else
        {
            /* Takes the packet out from the cache */
            l_index = -- p_cache->largebuffer.l_index;    
            p_data->p_buffer = p_cache->largebuffer.p_stack[l_index].p_data;
            if( p_data->p_buffer == NULL )
            {
                intf_ErrMsg( "NULL packet in the small buffer cache" );
                free( p_data );
                vlc_mutex_unlock( &p_cache->lock );
                return NULL;
            }
            /* Reallocates the packet if it is too small or too large */
            if( p_cache->largebuffer.p_stack[l_index].l_size < l_size ||
                p_cache->largebuffer.p_stack[l_index].l_size > 2*l_size )
            {
                p_data->p_buffer = realloc( p_data->p_buffer, l_size );
                p_data->l_size = l_size;
            }
            else
            {
                p_data->l_size = p_cache->largebuffer.p_stack[l_index].l_size;
            }
        }
    }

    vlc_mutex_unlock( &p_cache->lock );

    /* Initialize data */
    p_data->p_next = NULL;
    p_data->b_discard_payload = 0;
    p_data->p_payload_start = p_data->p_buffer;
    p_data->p_payload_end = p_data->p_buffer + l_size;

    return( p_data );

}


/*****************************************************************************
 * NewPES: allocates a pes packet
 *****************************************************************************/
static pes_packet_t * NewPES( void * p_packet_cache )
{
    packet_cache_t *   p_cache;
    pes_packet_t *     p_pes;

    p_cache = (packet_cache_t *)p_packet_cache;

#ifdef DEBUG
    if ( p_cache == NULL )
    {
        intf_ErrMsg( "Packet cache not initialized" );
        return NULL;
    }
#endif

    vlc_mutex_lock( &p_cache->lock );

    /* Checks whether the PES cache is empty */
    if( p_cache->pes.l_index == 0 )
    {
        /* Allocates a new packet */
        p_pes = malloc( sizeof(pes_packet_t) );
        if( p_pes == NULL )
        {
            intf_DbgMsg( "Out of memory" );
            vlc_mutex_unlock( &p_cache->lock );
            return NULL;
        }
#ifdef TRACE_INPUT
        intf_DbgMsg( "PS input: PES packet allocated" );
#endif
    }
    else
    {
        /* Takes the packet out from the cache */
        p_pes = p_cache->pes.p_stack[ -- p_cache->pes.l_index ];
        if( p_pes == NULL )
        {
            intf_ErrMsg( "NULL packet in the data cache" );
            vlc_mutex_unlock( &p_cache->lock );
            return NULL;
        }
    }

    vlc_mutex_unlock( &p_cache->lock );

    p_pes->b_data_alignment = p_pes->b_discontinuity =
        p_pes->i_pts = p_pes->i_dts = 0;
    p_pes->i_pes_size = 0;
    p_pes->p_first = NULL;

    return( p_pes );
    
}

/*****************************************************************************
 * DeletePacket: deletes a data packet
 *****************************************************************************/
static void DeletePacket( void * p_packet_cache,
                          data_packet_t * p_data )
{
    packet_cache_t *   p_cache;

    p_cache = (packet_cache_t *)p_packet_cache;

#ifdef DEBUG
    if ( p_cache == NULL )
    {
        intf_ErrMsg( "Packet cache not initialized" );
        return;
    }
#endif

    ASSERT( p_data );

    vlc_mutex_lock( &p_cache->lock );

    /* Checks whether the data cache is full */
    if ( p_cache->data.l_index < DATA_CACHE_SIZE )
    {
        /* Cache not full: store the packet in it */
        p_cache->data.p_stack[ p_cache->data.l_index ++ ] = p_data;
        /* Small buffer or large buffer? */
        if ( p_data->l_size < MAX_SMALL_SIZE )
        {
            /* Checks whether the small buffer cache is full */
            if ( p_cache->smallbuffer.l_index < SMALL_CACHE_SIZE )
            {
                p_cache->smallbuffer.p_stack[
                    p_cache->smallbuffer.l_index ].l_size = p_data->l_size;
                p_cache->smallbuffer.p_stack[
                    p_cache->smallbuffer.l_index++ ].p_data = p_data->p_buffer;
            }
            else
            {
                ASSERT( p_data->p_buffer );
                free( p_data->p_buffer );
#ifdef TRACE_INPUT
                intf_DbgMsg( "PS input: small buffer freed" );
#endif
            }
        }
        else
        {
            /* Checks whether the large buffer cache is full */
            if ( p_cache->largebuffer.l_index < LARGE_CACHE_SIZE )
            {
                p_cache->largebuffer.p_stack[
                    p_cache->largebuffer.l_index ].l_size = p_data->l_size;
                p_cache->largebuffer.p_stack[
                    p_cache->largebuffer.l_index++ ].p_data = p_data->p_buffer;
            }
            else
            {
                ASSERT( p_data->p_buffer );
                free( p_data->p_buffer );
#ifdef TRACE_INPUT
                intf_DbgMsg( "PS input: large buffer freed" );
#endif
            }
        }
    }
    else
    {
        /* Cache full: the packet must be freed */
        free( p_data->p_buffer );
        free( p_data );
#ifdef TRACE_INPUT
        intf_DbgMsg( "PS input: data packet freed" );
#endif
    }

    vlc_mutex_unlock( &p_cache->lock );
}

/*****************************************************************************
 * DeletePES: deletes a PES packet and associated data packets
 *****************************************************************************/
static void DeletePES( void * p_packet_cache, pes_packet_t * p_pes )
{
    packet_cache_t *    p_cache;
    data_packet_t *     p_data;
    data_packet_t *     p_next;

    p_cache = (packet_cache_t *)p_packet_cache;

#ifdef DEBUG
    if ( p_cache == NULL )
    {
        intf_ErrMsg( "Packet cache not initialized" );
        return;
    }
#endif

    ASSERT( p_pes);

    p_data = p_pes->p_first;

    while( p_data != NULL )
    {
        p_next = p_data->p_next;
        DeletePacket( p_cache, p_data );
        p_data = p_next;
    }

    vlc_mutex_lock( &p_cache->lock );

    /* Checks whether the PES cache is full */
    if ( p_cache->pes.l_index < PES_CACHE_SIZE )
    {
        /* Cache not full: store the packet in it */
        p_cache->pes.p_stack[ p_cache->pes.l_index ++ ] = p_pes;
    }
    else
    {
        /* Cache full: the packet must be freed */
        free( p_pes );
#ifdef TRACE_INPUT
        intf_DbgMsg( "PS input: PES packet freed" );
#endif
    }

    vlc_mutex_unlock( &p_cache->lock );
}
