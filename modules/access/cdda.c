/*****************************************************************************
 * cdda.c : CD digital audio input module for vlc
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
 * $Id: cdda.c,v 1.8 2003/11/24 12:44:24 fenrir Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
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
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "vcd/cdrom.h"

/*****************************************************************************
 * Module descriptior
 *****************************************************************************/
static int  AccessOpen ( vlc_object_t * );
static void AccessClose( vlc_object_t * );

static int  DemuxOpen  ( vlc_object_t * );
static void DemuxClose ( vlc_object_t * );

#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for cdda streams. This " \
    "value should be set in miliseconds units." )

vlc_module_begin();
    set_description( _("CD Audio input") );
    set_capability( "access", 70 );
    add_integer( "cdda-caching", DEFAULT_PTS_DELAY / 1000, NULL, CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );
    set_callbacks( AccessOpen, AccessClose );
    add_shortcut( "cdda" );

    add_submodule();
        set_description( _("CD Audio demux") );
        set_capability( "demux", 0 );
        set_callbacks( DemuxOpen, DemuxClose );
        add_shortcut( "cdda" );
vlc_module_end();


/* how many blocks VCDRead will read in each loop */
#define CDDA_BLOCKS_ONCE 20
#define CDDA_DATA_ONCE   (CDDA_BLOCKS_ONCE * CDDA_DATA_SIZE)

/*****************************************************************************
 * Access: local prototypes
 *****************************************************************************/
struct access_sys_t
{
    vcddev_t    *vcddev;                            /* vcd device descriptor */
    int         i_nb_tracks;                        /* Nb of tracks (titles) */
    int         i_track;                                    /* Current track */
    int         i_sector;                                  /* Current Sector */
    int *       p_sectors;                                  /* Track sectors */
    vlc_bool_t  b_end_of_track;           /* If the end of track was reached */

};

static int  Read      ( input_thread_t *, byte_t *, size_t );
static void Seek      ( input_thread_t *, off_t );
static int  SetArea   ( input_thread_t *, input_area_t * );
static int  SetProgram( input_thread_t *, pgrm_descriptor_t * );

/*****************************************************************************
 * AccessOpen: open cdda
 *****************************************************************************/
static int AccessOpen( vlc_object_t *p_this )
{
    input_thread_t *        p_input = (input_thread_t *)p_this;
    access_sys_t *          p_sys;

    char *                  psz_orig;
    char *                  psz_parser;
    char *                  psz_source;
    int                     i;
    input_area_t *          p_area;
    int                     i_title = 1;
    vcddev_t                *vcddev;

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

        i_title = (int)strtol( psz_parser, NULL, 10 );
        i_title = i_title ? i_title : 1;
    }

    if( !*psz_source )
    {
        if( !p_input->psz_access )
        {
            free( psz_orig );
            return VLC_EGENERIC;
        }
        psz_source = config_GetPsz( p_input, "vcd" );
        if( !psz_source ) return -1;
    }

    /* Open CDDA */
    if( !(vcddev = ioctl_Open( p_this, psz_source )) )
    {
        msg_Warn( p_input, "could not open %s", psz_source );
        free( psz_source );
        return VLC_EGENERIC;
    }
    free( psz_source );

    p_input->p_access_data = p_sys = malloc( sizeof(access_sys_t) );
    if( p_sys == NULL )
    {
        msg_Err( p_input, "out of memory" );
        free( psz_source );
        return VLC_EGENERIC;
    }

    p_sys->vcddev = vcddev;

    p_input->i_mtu = CDDA_DATA_ONCE;

    /* We read the Table Of Content information */
    p_sys->i_nb_tracks = ioctl_GetTracksMap( VLC_OBJECT(p_input),
                                             p_sys->vcddev, &p_sys->p_sectors );
    if( p_sys->i_nb_tracks < 0 )
    {
        msg_Err( p_input, "unable to count tracks" );
    }
    else if( p_sys->i_nb_tracks <= 0 )
    {
        msg_Err( p_input, "no audio tracks found" );
    }

    if( p_sys->i_nb_tracks <= 1)
    {
        ioctl_Close( p_this, p_sys->vcddev );
        free( p_sys );
        return VLC_EGENERIC;
    }

    if( i_title >= p_sys->i_nb_tracks || i_title < 1 )
    {
        i_title = 1;
    }

    /* Set stream and area data */
    vlc_mutex_lock( &p_input->stream.stream_lock );

    /* Initialize ES structures */
    input_InitStream( p_input, 0 );

    /* cdda input method */
    p_input->stream.i_method = INPUT_METHOD_CDDA;

    p_input->stream.b_pace_control = 1;
    p_input->stream.b_seekable = 1;
    p_input->stream.i_mux_rate = 44100 * 4 / 50;

#define area p_input->stream.pp_areas
    for( i = 1 ; i <= p_sys->i_nb_tracks ; i++ )
    {
        input_AddArea( p_input, i, 1 );

        /* Absolute start offset and size */
        area[i]->i_start =
            (off_t)p_sys->p_sectors[i-1] * (off_t)CDDA_DATA_SIZE;
        area[i]->i_size =
            (off_t)(p_sys->p_sectors[i] - p_sys->p_sectors[i-1])
            * (off_t)CDDA_DATA_SIZE;
    }
#undef area

    p_area = p_input->stream.pp_areas[i_title];

    SetArea( p_input, p_area );

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    if( !p_input->psz_demux || !*p_input->psz_demux )
    {
        p_input->psz_demux = "cdda";
    }

    p_input->pf_read = Read;
    p_input->pf_seek = Seek;
    p_input->pf_set_area = SetArea;
    p_input->pf_set_program = SetProgram;

    /* Update default_pts to a suitable value for cdda access */
    p_input->i_pts_delay = config_GetInt( p_input, "cdda-caching" ) * 1000;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * AccessClose: closes cdda
 *****************************************************************************/
static void AccessClose( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    access_sys_t   *p_sys = p_input->p_access_data;

    ioctl_Close( p_this, p_sys->vcddev );
    free( p_sys );
}

/*****************************************************************************
 * Read: reads from the CDDA into PES packets.
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, otherwise the number of
 * bytes.
 *****************************************************************************/
static int Read( input_thread_t * p_input, byte_t * p_buffer,
                     size_t i_len )
{
    access_sys_t *p_sys = p_input->p_access_data;
    int          i_blocks = i_len / CDDA_DATA_SIZE;;
    int          i_read = 0;
    int          i_index;


    if( ioctl_ReadSectors( VLC_OBJECT(p_input), p_sys->vcddev, p_sys->i_sector,
                           p_buffer, i_blocks, CDDA_TYPE ) < 0 )
    {
        msg_Err( p_input, "could not read sector %d", p_sys->i_sector );
        return -1;
    }

    for( i_index = 0; i_index < i_blocks; i_index++ )
    {
        p_sys->i_sector ++;
        if( p_sys->i_sector == p_sys->p_sectors[p_sys->i_track + 1] )
        {
            input_area_t *p_area;

            if ( p_sys->i_track >= p_sys->i_nb_tracks - 1 )
            {
                return 0; /* EOF */
            }

            vlc_mutex_lock( &p_input->stream.stream_lock );
            p_area = p_input->stream.pp_areas[
                    p_input->stream.p_selected_area->i_id + 1 ];

            msg_Dbg( p_input, "new title" );

            p_area->i_part = 1;
            SetArea( p_input, p_area );
            vlc_mutex_unlock( &p_input->stream.stream_lock );
        }
        i_read += CDDA_DATA_SIZE;
    }

    if ( i_len % CDDA_DATA_SIZE ) /* this should not happen */
    {
        msg_Err( p_input, "must read full sectors" );
    }

    return i_read;
}


/*****************************************************************************
 * SetProgram: Does nothing since a CDDA is mono_program
 *****************************************************************************/
static int SetProgram( input_thread_t * p_input,
                           pgrm_descriptor_t * p_program)
{
    return VLC_EGENERIC;
}


/*****************************************************************************
 * SetArea: initialize input data for title x.
 * It should be called for each user navigation request.
 ****************************************************************************/
static int SetArea( input_thread_t * p_input, input_area_t * p_area )
{
    access_sys_t *p_sys = p_input->p_access_data;
    vlc_value_t  val;

    /* we can't use the interface slider until initilization is complete */
    p_input->stream.b_seekable = 0;

    if( p_area != p_input->stream.p_selected_area )
    {
        /* Change the default area */
        p_input->stream.p_selected_area = p_area;

        /* Change the current track */
        p_sys->i_track = p_area->i_id - 1;
        p_sys->i_sector = p_sys->p_sectors[p_sys->i_track];

        /* Update the navigation variables without triggering a callback */
        val.i_int = p_area->i_id;
        var_Change( p_input, "title", VLC_VAR_SETVALUE, &val, NULL );
    }

    p_sys->i_sector = p_sys->p_sectors[p_sys->i_track];

    p_input->stream.p_selected_area->i_tell =
        (off_t)p_sys->i_sector * (off_t)CDDA_DATA_SIZE
         - p_input->stream.p_selected_area->i_start;

    /* warn interface that something has changed */
    p_input->stream.b_seekable = 1;
    p_input->stream.b_changed = 1;

    return 0;
}


/****************************************************************************
 * Seek
 ****************************************************************************/
static void Seek( input_thread_t * p_input, off_t i_off )
{
    access_sys_t * p_sys = p_input->p_access_data;

    p_sys->i_sector = p_sys->p_sectors[p_sys->i_track]
                       + i_off / (off_t)CDDA_DATA_SIZE;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.p_selected_area->i_tell =
        (off_t)p_sys->i_sector * (off_t)CDDA_DATA_SIZE
         - p_input->stream.p_selected_area->i_start;
    vlc_mutex_unlock( &p_input->stream.stream_lock );
}




/*****************************************************************************
 * Demux: local prototypes
 *****************************************************************************/
struct demux_sys_t
{
    es_out_id_t *p_es;
    mtime_t     i_pts;
};

static int  Demux     ( input_thread_t * p_input );

/****************************************************************************
 * DemuxOpen:
 ****************************************************************************/
static int  DemuxOpen    ( vlc_object_t * p_this)
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t    *p_sys;

    es_format_t    fmt;

    if( p_input->stream.i_method != INPUT_METHOD_CDDA )
    {
        return VLC_EGENERIC;
    }

    p_input->pf_demux  = Demux;
    p_input->pf_rewind = NULL;
    p_input->pf_demux_control = demux_vaControlDefault;
    p_input->p_demux_data = p_sys = malloc( sizeof( es_descriptor_t ) );
    p_sys->i_pts = 0;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_InitStream( p_input, 0 ) == -1)
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot init stream" );
        free( p_sys );
        return VLC_EGENERIC;
    }
    p_input->stream.i_mux_rate = 4 * 44100 / 50;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    es_format_Init( &fmt, AUDIO_ES, VLC_FOURCC( 'a', 'r', 'a', 'w' ) );
    fmt.audio.i_channels = 2;
    fmt.audio.i_rate = 44100;
    fmt.audio.i_bitspersample = 16;
    fmt.audio.i_blockalign = 4;
    fmt.i_bitrate = 4 * 44100 * 8;

    p_sys->p_es =  es_out_Add( p_input->p_es_out, &fmt );

    return VLC_SUCCESS;
}

/****************************************************************************
 * DemuxClose:
 ****************************************************************************/
static void DemuxClose( vlc_object_t * p_this)
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    demux_sys_t    *p_sys = (demux_sys_t*)p_input->p_demux_data;

    free( p_sys );
    return;
}

/****************************************************************************
 * Demux:
 ****************************************************************************/
static int  Demux( input_thread_t * p_input )
{
    demux_sys_t    *p_sys = (demux_sys_t*)p_input->p_demux_data;
    block_t        *p_block;


    input_ClockManageRef( p_input,
                          p_input->stream.p_selected_program,
                          p_sys->i_pts );

    if( ( p_block = stream_Block( p_input->s, CDDA_DATA_SIZE ) ) == NULL )
    {
        /* eof */
        return 0;
    }
    p_block->i_dts =
    p_block->i_pts = input_ClockGetTS( p_input,
                                       p_input->stream.p_selected_program,
                                       p_sys->i_pts );
    p_block->i_length = (mtime_t)90000 * (mtime_t)p_block->i_buffer/44100/4;

    p_sys->i_pts += p_block->i_length;

    es_out_Send( p_input->p_es_out, p_sys->p_es, p_block );

    return 1;
}
