/*****************************************************************************
 * access.c: access capabilities for dvdplay plugin.
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: access.c,v 1.16 2003/04/05 12:32:19 gbazin Exp $
 *
 * Author: Stéphane Borel <stef@via.ecp.fr>
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

#include "../../demux/mpeg/system.h"

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

#include "dvd.h"
#include "es.h"
#include "tools.h"
#include "intf.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
/* called from outside */
static int    dvdplay_SetArea       ( input_thread_t *, input_area_t * );
static int    dvdplay_SetProgram    ( input_thread_t *, pgrm_descriptor_t * );
static int    dvdplay_Read          ( input_thread_t *, byte_t *, size_t );
static void   dvdplay_Seek          ( input_thread_t *, off_t );

static void   pf_vmg_callback       ( void*, dvdplay_event_t );

/* only from inside */
static int dvdNewArea( input_thread_t *, input_area_t * );
static int dvdNewPGC ( input_thread_t * );

/*****************************************************************************
 * OpenDVD: open libdvdplay
 *****************************************************************************/
int E_(OpenDVD) ( vlc_object_t *p_this )
{
    input_thread_t *        p_input = (input_thread_t *)p_this;
    char *                  psz_source;
    dvd_data_t *            p_dvd;
    input_area_t *          p_area;
    unsigned int            i_title_nr;
    unsigned int            i_title;
    unsigned int            i_chapter;
    unsigned int            i_angle;
    unsigned int            i;

    p_dvd = malloc( sizeof(dvd_data_t) );
    if( p_dvd == NULL )
    {
        msg_Err( p_input, "out of memory" );
        return -1;
    }

    p_input->p_access_data = (void *)p_dvd;

    p_input->pf_read = dvdplay_Read;
    p_input->pf_seek = dvdplay_Seek;
    p_input->pf_set_area = dvdplay_SetArea;
    p_input->pf_set_program = dvdplay_SetProgram;

    /* command line */
    if( ( psz_source = dvdplay_ParseCL( p_input,
                        &i_title, &i_chapter, &i_angle ) ) == NULL )
    {
        free( p_dvd );
        return -1;
    }

    /* Open libdvdplay */
    p_dvd->vmg = dvdplay_open( psz_source, pf_vmg_callback, (void*)p_input );

    if( p_dvd->vmg == NULL )
    {
        msg_Warn( p_input, "cannot open %s", psz_source );
        free( psz_source );
        free( p_dvd );
        return -1;
    }

    /* free allocated strings */
    free( psz_source );

    p_dvd->p_intf = NULL;

    p_dvd->i_still_time = 0;

    /* set up input  */
    p_input->i_mtu = 0;

    /* Set stream and area data */
    vlc_mutex_lock( &p_input->stream.stream_lock );

    /* If we are here we can control the pace... */
    p_input->stream.b_pace_control = 1;
    /* seek is only allowed when we have size info */
    p_input->stream.b_seekable = 0;

    /* Initialize ES structures */
    input_InitStream( p_input, sizeof( stream_ps_data_t ) );

    /* disc input method */
    p_input->stream.i_method = INPUT_METHOD_DVD;

    i_title_nr = dvdplay_title_nr( p_dvd->vmg );
#define area p_input->stream.pp_areas

    /* Area 0 for menu */
    area[0]->i_plugin_data = 0;
    input_DelArea( p_input, p_input->stream.pp_areas[0] );
    input_AddArea( p_input, 0, 1 );

    for( i = 1 ; i <= i_title_nr ; i++ )
    {
        input_AddArea( p_input, i, dvdplay_chapter_nr( p_dvd->vmg, i ) );
        area[i]->i_plugin_data = 0;
    }
#undef area
    msg_Dbg( p_input, "number of titles: %d", i_title_nr );

    i_title = i_title <= i_title_nr ? i_title : 0;

    p_area = p_input->stream.pp_areas[i_title];
    p_area->i_part = i_chapter;
    p_input->stream.p_selected_area = NULL;

    /* set title, chapter, audio and subpic */
    if( dvdplay_SetArea( p_input, p_area ) )
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        return -1;
    }

    if( i_angle <= p_input->stream.i_pgrm_number )
    {
        dvdplay_SetProgram( p_input,
                            p_input->stream.pp_programs[i_angle - 1] );
    }

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    if( !p_input->psz_demux || !*p_input->psz_demux )
    {
        p_input->psz_demux = "dvdplay";
    }

    /* FIXME: we might lose variables here */
    var_Create( p_input, "x-start", VLC_VAR_INTEGER );
    var_Create( p_input, "y-start", VLC_VAR_INTEGER );
    var_Create( p_input, "x-end", VLC_VAR_INTEGER );
    var_Create( p_input, "y-end", VLC_VAR_INTEGER );

    var_Create( p_input, "color", VLC_VAR_ADDRESS );
    var_Create( p_input, "contrast", VLC_VAR_ADDRESS );

    var_Create( p_input, "highlight", VLC_VAR_BOOL );
    var_Create( p_input, "highlight-mutex", VLC_VAR_MUTEX );

    return 0;
}

/*****************************************************************************
 * CloseDVD: close libdvdplay
 *****************************************************************************/
void E_(CloseDVD) ( vlc_object_t *p_this )
{
    input_thread_t * p_input = (input_thread_t *)p_this;
    dvd_data_t *     p_dvd = (dvd_data_t *)p_input->p_access_data;

    var_Destroy( p_input, "highlight-mutex" );
    var_Destroy( p_input, "highlight" );

    var_Destroy( p_input, "x-start" );
    var_Destroy( p_input, "x-end" );
    var_Destroy( p_input, "y-start" );
    var_Destroy( p_input, "y-end" );

    var_Destroy( p_input, "color" );
    var_Destroy( p_input, "contrast" );

    /* close libdvdplay */
    dvdplay_close( p_dvd->vmg );

    free( p_dvd );
    p_input->p_access_data = NULL;

}

/*****************************************************************************
 * dvdplay_SetProgram: set dvd angle.
 *****************************************************************************
 * This is actually a hack to make angle change through vlc interface with
 * no need for a specific button.
 *****************************************************************************/
static int dvdplay_SetProgram( input_thread_t *     p_input,
                               pgrm_descriptor_t *  p_program )
{
    if( p_input->stream.p_selected_program != p_program )
    {
        dvd_data_t *    p_dvd;
        int             i_angle;
        vlc_value_t     val;

        p_dvd = (dvd_data_t*)(p_input->p_access_data);
        i_angle = p_program->i_number;

        if( !dvdplay_angle( p_dvd->vmg, i_angle ) )
        {
            memcpy( p_program, p_input->stream.p_selected_program,
                    sizeof(pgrm_descriptor_t) );
            p_program->i_number = i_angle;
            p_input->stream.p_selected_program = p_program;

            msg_Dbg( p_input, "angle %d selected", i_angle );
        }

        /* Update the navigation variables without triggering a callback */
        val.i_int = p_program->i_number;
        var_Change( p_input, "program", VLC_VAR_SETVALUE, &val );
    }

    return 0;
}

/*****************************************************************************
 * dvdplay_SetArea: initialize input data for title x, chapter y.
 * It should be called for each user navigation request.
 *****************************************************************************
 * Take care that i_title starts from 0 (vmg) and i_chapter start from 1.
 * Note that you have to take the lock before entering here.
 *****************************************************************************/
static int dvdplay_SetArea( input_thread_t * p_input, input_area_t * p_area )
{
    dvd_data_t *    p_dvd;
    vlc_value_t     val;

    p_dvd = (dvd_data_t*)p_input->p_access_data;

    /*
     * Title selection
     */
    if( p_area != p_input->stream.p_selected_area )
    {
        int i_chapter;

        /* prevent intf to try to seek */
        p_input->stream.b_seekable = 0;

        /* Store selected chapter */
        i_chapter = p_area->i_part;

        dvdNewArea( p_input, p_area );

        /* Reinit ES */
        dvdNewPGC( p_input );

        dvdplay_start( p_dvd->vmg, p_area->i_id );

        p_area->i_part = i_chapter;

    } /* i_title >= 0 */
    else
    {
        p_area = p_input->stream.p_selected_area;
    }

    /*
     * Chapter selection
     */

    if( (int)p_area->i_part != dvdplay_chapter_cur( p_dvd->vmg ) )
    {
        if( ( p_area->i_part > 0 ) &&
            ( p_area->i_part <= p_area->i_part_nb ))
        {
            dvdplay_pg( p_dvd->vmg, p_area->i_part );
        }
        p_area->i_part = dvdplay_chapter_cur( p_dvd->vmg );
    }

    /* warn interface that something has changed */
    p_area->i_tell =
        LB2OFF( dvdplay_position( p_dvd->vmg ) ) - p_area->i_start;
    p_input->stream.b_changed = 1;

    /* Update the navigation variables without triggering a callback */
    val.i_int = p_area->i_part;
    var_Change( p_input, "chapter", VLC_VAR_SETVALUE, &val );

    return 0;
}

/*****************************************************************************
 * dvdplay_Read: reads data packets.
 *****************************************************************************
 * Returns -1 in case of error, the number of bytes read if everything went
 * well.
 *****************************************************************************/
static int dvdplay_Read( input_thread_t * p_input,
                         byte_t * p_buffer, size_t i_count )
{
    dvd_data_t *    p_dvd;
    off_t           i_read;

    p_dvd = (dvd_data_t *)p_input->p_access_data;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    i_read = LB2OFF( dvdplay_read( p_dvd->vmg, p_buffer, OFF2LB( i_count ) ) );
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return i_read;
}

/*****************************************************************************
 * dvdplay_Seek : Goes to a given position on the stream.
 *****************************************************************************
 * This one is used by the input and translate chronological position from
 * input to logical position on the device.
 * The lock should be taken before calling this function.
 *****************************************************************************/
static void dvdplay_Seek( input_thread_t * p_input, off_t i_off )
{
    dvd_data_t *     p_dvd;

    p_dvd = (dvd_data_t *)p_input->p_access_data;

    vlc_mutex_lock( &p_input->stream.stream_lock );

    dvdplay_seek( p_dvd->vmg, OFF2LB( i_off ) );

    p_input->stream.p_selected_area->i_tell  =
        LB2OFF( dvdplay_position( p_dvd->vmg ) ) -
        p_input->stream.p_selected_area->i_start;

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return;
}


/*****************************************************************************
 * pf_vmg_callback: called by libdvdplay when some event happens
 *****************************************************************************
 * The stream lock has to be taken before entering here
 *****************************************************************************/
static void pf_vmg_callback( void* p_args, dvdplay_event_t event )
{
    input_thread_t *    p_input;
    dvd_data_t *        p_dvd;
    vlc_value_t         val;
    unsigned int        i;

    p_input = (input_thread_t*)p_args;
    p_dvd   = (dvd_data_t*)p_input->p_access_data;

    switch( event )
    {
    case NEW_DOMAIN:
        break;
    case NEW_VTS:
        break;
    case NEW_FILE:
        break;
    case NEW_PGC:
        /* prevent intf to try to seek  by default */
        p_input->stream.b_seekable = 0;

        i = dvdplay_title_cur( p_dvd->vmg );
        if( i != p_input->stream.p_selected_area->i_id )
        {
            /* the title number has changed: update area */
            msg_Warn( p_input, "new title %d (%d)", i,
                               p_input->stream.p_selected_area->i_id );
            dvdNewArea( p_input,
                        p_input->stream.pp_areas[i] );
        }

        /* new pgc in same title: reinit ES */
        dvdNewPGC( p_input );

        p_input->stream.b_changed = 1;

        break;
    case NEW_PG:
        /* update current chapter */
        p_input->stream.p_selected_area->i_part =
            dvdplay_chapter_cur( p_dvd->vmg );

        p_input->stream.p_selected_area->i_tell =
            LB2OFF( dvdplay_position( p_dvd->vmg ) ) -
            p_input->stream.p_selected_area->i_start;

        /* warn interface that something has changed */
        p_input->stream.b_changed = 1;

        /* Update the navigation variables without triggering a callback */
        val.i_int = p_input->stream.p_selected_area->i_part;
        var_Change( p_input, "chapter", VLC_VAR_SETVALUE, &val );
        break;
    case NEW_CELL:
        p_dvd->b_end_of_cell = 0;
        break;
    case END_OF_CELL:
        p_dvd->b_end_of_cell = 1;
        break;
    case JUMP:
        dvdplay_ES( p_input );
        break;
    case STILL_TIME:
        /* we must pause only from demux
         * when the data in cache has been decoded */
        p_dvd->i_still_time = dvdplay_still_time( p_dvd->vmg );
        msg_Dbg( p_input, "still time %d", p_dvd->i_still_time );
        break;
    case COMPLETE_VIDEO:
        break;
    case NEW_HIGHLIGHT:
        if( var_Get( p_input, "highlight-mutex", &val ) == VLC_SUCCESS )
        {
            vlc_mutex_t *p_mutex = val.p_address;
            vlc_mutex_lock( p_mutex );

            /* Retrieve the highlight from dvdplay */
            dvdplay_highlight( p_dvd->vmg, &p_dvd->hli );

            if( p_dvd->hli.i_x_start || p_dvd->hli.i_y_start ||
                p_dvd->hli.i_x_end || p_dvd->hli.i_y_end )
            {
                /* Fill our internal variables with this data */
                val.i_int = p_dvd->hli.i_x_start;
                var_Set( p_input, "x-start", val );
                val.i_int = p_dvd->hli.i_y_start;
                var_Set( p_input, "y-start", val );
                val.i_int = p_dvd->hli.i_x_end;
                var_Set( p_input, "x-end", val );
                val.i_int = p_dvd->hli.i_y_end;
                var_Set( p_input, "y-end", val );

                val.p_address = (void *)p_dvd->hli.pi_color;
                var_Set( p_input, "color", val );
                val.p_address = (void *)p_dvd->hli.pi_contrast;
                var_Set( p_input, "contrast", val );

                /* Tell the SPU decoder that there's a new highlight */
                val.b_bool = VLC_TRUE;
            }
            else
            {
                /* Turn off the highlight */
                val.b_bool = VLC_FALSE;
            }
            var_Set( p_input, "highlight", val );

            vlc_mutex_unlock( p_mutex );
        }
        break;
    default:
        msg_Err( p_input, "unknown event from libdvdplay (%d)", event );
    }

    return;
}

static int dvdNewArea( input_thread_t * p_input, input_area_t * p_area )
{
    dvd_data_t *    p_dvd;
    int             i_angle_nb, i_angle;
    vlc_value_t     val;
    int             i;

    p_dvd = (dvd_data_t*)p_input->p_access_data;

    p_input->stream.p_selected_area = p_area;

    /*
     * One program for each angle
     */
    while( p_input->stream.i_pgrm_number )
    {
        input_DelProgram( p_input, p_input->stream.pp_programs[0] );
    }

    input_AddProgram( p_input, 1, sizeof( stream_ps_data_t ) );
    p_input->stream.p_selected_program = p_input->stream.pp_programs[0];

    dvdplay_angle_info( p_dvd->vmg, &i_angle_nb, &i_angle );
    for( i = 1 ; i < i_angle_nb ; i++ )
    {
        input_AddProgram( p_input, i+1, 0 );
    }

    if( i_angle )
        dvdplay_SetProgram( p_input,
                            p_input->stream.pp_programs[i_angle-1] );
    else
        dvdplay_SetProgram( p_input,
                            p_input->stream.pp_programs[0] );

    /* No PSM to read in DVD mode, we already have all information */
    p_input->stream.p_selected_program->b_is_ok = 1;

    /* Update the navigation variables without triggering a callback */
    val.i_int = p_area->i_id;
    var_Change( p_input, "title", VLC_VAR_SETVALUE, &val );
    var_Change( p_input, "chapter", VLC_VAR_CLEARCHOICES, NULL );
    for( i = 1; (unsigned int)i <= p_area->i_part_nb; i++ )
    {
        val.i_int = i;
        var_Change( p_input, "chapter", VLC_VAR_ADDCHOICE, &val );
    }

    /* Update the navigation variables without triggering a callback */
    val.i_int = p_area->i_part;
    var_Change( p_input, "chapter", VLC_VAR_SETVALUE, &val );

    return 0;
}

static int dvdNewPGC( input_thread_t * p_input )
{
    dvd_data_t *    p_dvd;
//    int             i_audio_nr  = -1;
//    int             i_audio     = -1;
//    int             i_subp_nr   = -1;
//    int             i_subp      = -1;
//    int             i_sec;

    p_dvd = (dvd_data_t*)p_input->p_access_data;

//    dvdplay_audio_info( p_dvd->vmg, &i_audio_nr, &i_audio );
//    dvdplay_subp_info( p_dvd->vmg, &i_subp_nr, &i_subp );

    dvdplay_ES( p_input );
    p_input->stream.p_selected_area->i_start =
        LB2OFF( dvdplay_title_first( p_dvd->vmg ) );
    p_input->stream.p_selected_area->i_size  =
        LB2OFF( dvdplay_title_end ( p_dvd->vmg ) ) -
        p_input->stream.p_selected_area->i_start;
    p_input->stream.p_selected_area->i_tell = 0;

    if( p_input->stream.p_selected_area->i_size > 0 )
    {
        p_input->stream.b_seekable = 1;
    }
    else
    {
        p_input->stream.b_seekable = 0;
    }

#if 0
    i_sec = dvdplay_title_time( p_dvd->vmg );
    msg_Dbg( p_input, "title time: %d:%02d:%02d (%d)",
                     i_sec/3600, (i_sec%3600)/60, i_sec%60, i_sec );
#endif

    return 0;
}
