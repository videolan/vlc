/*****************************************************************************
 * cdda.c : CD digital audio input module for vlc
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
 * $Id: cdda.c,v 1.5 2003/06/17 20:03:50 hartman Exp $
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
#include <stdio.h>
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <sys/types.h>

#include "codecs.h"

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include <string.h>

#include "vcd/cdrom.h"

/* how many blocks VCDRead will read in each loop */
#define CDDA_BLOCKS_ONCE 20
#define CDDA_DATA_ONCE   (CDDA_BLOCKS_ONCE * CDDA_DATA_SIZE)

/*****************************************************************************
 * cdda_data_t: CD audio information
 *****************************************************************************/
typedef struct cdda_data_s
{
    vcddev_t    *vcddev;                            /* vcd device descriptor */
    int         i_nb_tracks;                        /* Nb of tracks (titles) */
    int         i_track;                                    /* Current track */
    int         i_sector;                                  /* Current Sector */
    int *       p_sectors;                                  /* Track sectors */
    vlc_bool_t  b_end_of_track;           /* If the end of track was reached */

} cdda_data_t;

struct demux_sys_t
{
    es_descriptor_t *p_es;
    mtime_t         i_pts;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  CDDAOpen         ( vlc_object_t * );
static void CDDAClose        ( vlc_object_t * );
static int  CDDARead         ( input_thread_t *, byte_t *, size_t );
static void CDDASeek         ( input_thread_t *, off_t );
static int  CDDASetArea      ( input_thread_t *, input_area_t * );
static int  CDDASetProgram   ( input_thread_t *, pgrm_descriptor_t * );

static int  CDDAOpenDemux    ( vlc_object_t * );
static void CDDACloseDemux   ( vlc_object_t * );
static int  CDDADemux        ( input_thread_t * p_input );

/*****************************************************************************
 * Module descriptior
 *****************************************************************************/
#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for cdda streams. This " \
    "value should be set in miliseconds units." )

vlc_module_begin();
    set_description( _("CD Audio input") );
    set_capability( "access", 70 );
    add_integer( "cdda-caching", DEFAULT_PTS_DELAY / 1000, NULL, CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );
    set_callbacks( CDDAOpen, CDDAClose );
    add_shortcut( "cdda" );

    add_submodule();
        set_description( _("CD Audio demux") );
        set_capability( "demux", 0 );
        set_callbacks( CDDAOpenDemux, CDDACloseDemux );
        add_shortcut( "cdda" );
vlc_module_end();

/*****************************************************************************
 * CDDAOpen: open cdda
 *****************************************************************************/
static int CDDAOpen( vlc_object_t *p_this )
{
    input_thread_t *        p_input = (input_thread_t *)p_this;
    char *                  psz_orig;
    char *                  psz_parser;
    char *                  psz_source;
    cdda_data_t *           p_cdda;
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
            return -1;
        }
        psz_source = config_GetPsz( p_input, "vcd" );
        if( !psz_source ) return -1;
    }

    /* Open CDDA */
    if( !(vcddev = ioctl_Open( p_this, psz_source )) )
    {
        msg_Warn( p_input, "could not open %s", psz_source );
        free( psz_source );
        return -1;
    }
    free( psz_source );

    p_cdda = malloc( sizeof(cdda_data_t) );
    if( p_cdda == NULL )
    {
        msg_Err( p_input, "out of memory" );
        free( psz_source );
        return -1;
    }

    p_cdda->vcddev = vcddev;
    p_input->p_access_data = (void *)p_cdda;

    p_input->i_mtu = CDDA_DATA_ONCE;

    /* We read the Table Of Content information */
    p_cdda->i_nb_tracks = ioctl_GetTracksMap( VLC_OBJECT(p_input),
                              p_cdda->vcddev, &p_cdda->p_sectors );
    if( p_cdda->i_nb_tracks < 0 )
        msg_Err( p_input, "unable to count tracks" );
    else if( p_cdda->i_nb_tracks <= 0 )
        msg_Err( p_input, "no audio tracks found" );

    if( p_cdda->i_nb_tracks <= 1)
    {
        ioctl_Close( p_this, p_cdda->vcddev );
        free( p_cdda );
        return -1;
    }

    if( i_title >= p_cdda->i_nb_tracks || i_title < 1 )
        i_title = 1;

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
    for( i = 1 ; i <= p_cdda->i_nb_tracks ; i++ )
    {
        input_AddArea( p_input, i, 1 );

        /* Absolute start offset and size */
        area[i]->i_start =
            (off_t)p_cdda->p_sectors[i-1] * (off_t)CDDA_DATA_SIZE;
        area[i]->i_size =
            (off_t)(p_cdda->p_sectors[i] - p_cdda->p_sectors[i-1])
            * (off_t)CDDA_DATA_SIZE;
    }
#undef area

    p_area = p_input->stream.pp_areas[i_title];

    CDDASetArea( p_input, p_area );

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    if( !p_input->psz_demux || !*p_input->psz_demux )
    {
        p_input->psz_demux = "cdda";
    }

    p_input->pf_read = CDDARead;
    p_input->pf_seek = CDDASeek;
    p_input->pf_set_area = CDDASetArea;
    p_input->pf_set_program = CDDASetProgram;

    /* Update default_pts to a suitable value for cdda access */
    p_input->i_pts_delay = config_GetInt( p_input, "cdda-caching" ) * 1000;

    return 0;
}

/*****************************************************************************
 * CDDAClose: closes cdda
 *****************************************************************************/
static void CDDAClose( vlc_object_t *p_this )
{
    input_thread_t *   p_input = (input_thread_t *)p_this;
    cdda_data_t *p_cdda = (cdda_data_t *)p_input->p_access_data;

    ioctl_Close( p_this, p_cdda->vcddev );
    free( p_cdda );
}

/*****************************************************************************
 * CDDARead: reads from the CDDA into PES packets.
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, otherwise the number of
 * bytes.
 *****************************************************************************/
static int CDDARead( input_thread_t * p_input, byte_t * p_buffer,
                     size_t i_len )
{
    cdda_data_t *           p_cdda;
    int                     i_blocks;
    int                     i_index;
    int                     i_read;

    p_cdda = (cdda_data_t *)p_input->p_access_data;

    i_read = 0;

    /* Compute the number of blocks we have to read */

    i_blocks = i_len / CDDA_DATA_SIZE;

    if ( ioctl_ReadSectors( VLC_OBJECT(p_input), p_cdda->vcddev,
             p_cdda->i_sector, p_buffer, i_blocks, CDDA_TYPE ) < 0 )
    {
        msg_Err( p_input, "could not read sector %d", p_cdda->i_sector );
        return -1;
    }

    for ( i_index = 0; i_index < i_blocks; i_index++ )
    {
        p_cdda->i_sector ++;
        if ( p_cdda->i_sector == p_cdda->p_sectors[p_cdda->i_track + 1] )
        {
            input_area_t *p_area;

            if ( p_cdda->i_track >= p_cdda->i_nb_tracks - 1 )
                return 0; /* EOF */

            vlc_mutex_lock( &p_input->stream.stream_lock );
            p_area = p_input->stream.pp_areas[
                    p_input->stream.p_selected_area->i_id + 1 ];

            msg_Dbg( p_input, "new title" );

            p_area->i_part = 1;
            CDDASetArea( p_input, p_area );
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
 * CDDASetProgram: Does nothing since a CDDA is mono_program
 *****************************************************************************/
static int CDDASetProgram( input_thread_t * p_input,
                           pgrm_descriptor_t * p_program)
{
    return 0;
}


/*****************************************************************************
 * CDDASetArea: initialize input data for title x.
 * It should be called for each user navigation request.
 ****************************************************************************/
static int CDDASetArea( input_thread_t * p_input, input_area_t * p_area )
{
    cdda_data_t *p_cdda = (cdda_data_t*)p_input->p_access_data;
    vlc_value_t val;

    /* we can't use the interface slider until initilization is complete */
    p_input->stream.b_seekable = 0;

    if( p_area != p_input->stream.p_selected_area )
    {
        /* Change the default area */
        p_input->stream.p_selected_area = p_area;

        /* Change the current track */
        p_cdda->i_track = p_area->i_id - 1;
        p_cdda->i_sector = p_cdda->p_sectors[p_cdda->i_track];

        /* Update the navigation variables without triggering a callback */
        val.i_int = p_area->i_id;
        var_Change( p_input, "title", VLC_VAR_SETVALUE, &val, NULL );
    }

    p_cdda->i_sector = p_cdda->p_sectors[p_cdda->i_track];

    p_input->stream.p_selected_area->i_tell =
        (off_t)p_cdda->i_sector * (off_t)CDDA_DATA_SIZE
         - p_input->stream.p_selected_area->i_start;

    /* warn interface that something has changed */
    p_input->stream.b_seekable = 1;
    p_input->stream.b_changed = 1;

    return 0;
}


/****************************************************************************
 * CDDASeek
 ****************************************************************************/
static void CDDASeek( input_thread_t * p_input, off_t i_off )
{
    cdda_data_t * p_cdda;

    p_cdda = (cdda_data_t *) p_input->p_access_data;

    p_cdda->i_sector = p_cdda->p_sectors[p_cdda->i_track]
                       + i_off / (off_t)CDDA_DATA_SIZE;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.p_selected_area->i_tell =
        (off_t)p_cdda->i_sector * (off_t)CDDA_DATA_SIZE
         - p_input->stream.p_selected_area->i_start;
    vlc_mutex_unlock( &p_input->stream.stream_lock );
}

/****************************************************************************
 * Demux Part
 ****************************************************************************/
static int  CDDAOpenDemux    ( vlc_object_t * p_this)
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t    *p_demux;
    WAVEFORMATEX   *p_wf;

    if( p_input->stream.i_method != INPUT_METHOD_CDDA )
    {
        return VLC_EGENERIC;
    }

    p_demux = malloc( sizeof( es_descriptor_t ) );
    p_demux->i_pts = 0;
    p_demux->p_es = NULL;

    p_input->pf_demux  = CDDADemux;
    p_input->pf_rewind = NULL;
    p_input->p_demux_data = p_demux;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_AddProgram( p_input, 0, 0) == NULL )
    {
        msg_Err( p_input, "cannot add program" );
        free( p_input->p_demux_data );
        return( -1 );
    }
    p_input->stream.pp_programs[0]->b_is_ok = 0;
    p_input->stream.p_selected_program = p_input->stream.pp_programs[0];

    /* create our ES */ 
    p_demux->p_es = input_AddES( p_input, 
                                 p_input->stream.p_selected_program,
                                 1 /* id */, AUDIO_ES, NULL, 0 );
    if( !p_demux->p_es )
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "out of memory" );
        free( p_input->p_demux_data );
        return( -1 );
    }
    p_demux->p_es->i_stream_id = 1;
    p_demux->p_es->i_fourcc = VLC_FOURCC('a','r','a','w');

    p_demux->p_es->p_waveformatex = p_wf = malloc( sizeof( WAVEFORMATEX ) );
    p_wf->wFormatTag = WAVE_FORMAT_PCM;
    p_wf->nChannels = 2;
    p_wf->nSamplesPerSec = 44100;
    p_wf->nAvgBytesPerSec = 2 * 44100 * 2;
    p_wf->nBlockAlign = 4;
    p_wf->wBitsPerSample = 16;
    p_wf->cbSize = 0;

    input_SelectES( p_input, p_demux->p_es );

    p_input->stream.p_selected_program->b_is_ok = 1;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return VLC_SUCCESS;
}

static void CDDACloseDemux( vlc_object_t * p_this)
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    demux_sys_t    *p_demux = (demux_sys_t*)p_input->p_demux_data;

    free( p_demux );
    p_input->p_demux_data = NULL;
    return;
}

static int  CDDADemux( input_thread_t * p_input )
{
    demux_sys_t    *p_demux = (demux_sys_t*)p_input->p_demux_data;
    ssize_t         i_read;
    data_packet_t * p_data;
    pes_packet_t *  p_pes;

    input_ClockManageRef( p_input,
                          p_input->stream.p_selected_program,
                          p_demux->i_pts );

    if( ( i_read = input_SplitBuffer( p_input, &p_data, CDDA_DATA_SIZE ) )
        <= 0 )
    {
        return 0; // EOF
    }

    p_pes = input_NewPES( p_input->p_method_data );

    if( p_pes == NULL )
    {
        msg_Err( p_input, "out of memory" );
        input_DeletePacket( p_input->p_method_data, p_data );
        return -1;
    }

    p_pes->i_rate = p_input->stream.control.i_rate;
    p_pes->p_first = p_pes->p_last = p_data;
    p_pes->i_nb_data = 1;
    p_pes->i_pes_size = i_read;

    p_pes->i_dts =
        p_pes->i_pts = input_ClockGetTS( p_input,
                                         p_input->stream.p_selected_program,
                                         p_demux->i_pts );

    if( p_demux->p_es->p_decoder_fifo )
    {
        input_DecodePES( p_demux->p_es->p_decoder_fifo, p_pes );
    }
    else
    {
        input_DeletePES( p_input->p_method_data, p_pes );
    }

    p_demux->i_pts += ((mtime_t)90000) * i_read
                      / (mtime_t)44100 / 4 /* stereo 16 bits */;
    return 1;
}
