/*****************************************************************************
 * input_dvdread.c: DvdRead plugin.
 *****************************************************************************
 * This plugins should handle all the known specificities of the DVD format,
 * especially the 2048 bytes logical block size.
 * It depends on: libdvdread for ifo files and block reading.
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: input_dvdread.c,v 1.11 2001/12/30 04:26:53 sam Exp $
 *
 * Author: Stéphane Borel <stef@via.ecp.fr>
 *
 * Some code taken form the play_title.c by Billy Biggs <vektor@dumbterm.net>
 * in libdvdread.
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

#define MODULE_NAME dvdread
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
#include <assert.h>

#ifdef STRNCASECMP_IN_STRINGS_H
#   include <strings.h>
#endif

#if defined( WIN32 )
#   include <io.h>                                                 /* read() */
#else
#   include <sys/uio.h>                                      /* struct iovec */
#endif


#include "common.h"
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"
#include "iso_lang.h"
#include "tests.h"

#if defined( WIN32 )
#   include "input_iovec.h"
#endif

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"
#include "input_ext-plugins.h"

#include "input_dvdread.h"

#include "debug.h"

#include "modules.h"
#include "modules_export.h"

/* how many blocks DVDRead will read in each loop */
#define DVD_BLOCK_READ_ONCE 64
#define DVD_DATA_READ_ONCE  (4 * DVD_BLOCK_READ_ONCE)

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
/* called from outside */
static int  DvdReadProbe    ( probedata_t *p_data );
static void DvdReadInit     ( struct input_thread_s * );
static void DvdReadEnd      ( struct input_thread_s * );
static void DvdReadOpen     ( struct input_thread_s * );
static void DvdReadClose    ( struct input_thread_s * );
static int  DvdReadSetArea  ( struct input_thread_s *, struct input_area_s * );
static int  DvdReadRead     ( struct input_thread_s *, data_packet_t ** );
static void DvdReadSeek     ( struct input_thread_s *, off_t );
static int  DvdReadRewind   ( struct input_thread_s * );

/* called only from here */
static void DvdReadHandleDSI( thread_dvd_data_t * p_dvd, u8 * p_data );
static void DvdReadFindCell ( thread_dvd_data_t * p_dvd );

/*****************************************************************************
 * Declare a buffer manager
 *****************************************************************************/
#define FLAGS           BUFFERS_UNIQUE_SIZE
#define NB_LIFO         1
DECLARE_BUFFERS_SHARED( FLAGS, NB_LIFO );
DECLARE_BUFFERS_INIT( FLAGS, NB_LIFO );
DECLARE_BUFFERS_END_SHARED( FLAGS, NB_LIFO );
DECLARE_BUFFERS_NEWPACKET_SHARED( FLAGS, NB_LIFO );
DECLARE_BUFFERS_DELETEPACKET_SHARED( FLAGS, NB_LIFO, 1000 );
DECLARE_BUFFERS_NEWPES( FLAGS, NB_LIFO );
DECLARE_BUFFERS_DELETEPES( FLAGS, NB_LIFO, 1000 );
DECLARE_BUFFERS_TOIO( FLAGS, DVD_VIDEO_LB_LEN );
DECLARE_BUFFERS_SHAREBUFFER( FLAGS );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( input_getfunctions )( function_list_t * p_function_list )
{
#define input p_function_list->functions.input
    p_function_list->pf_probe = DvdReadProbe;
    input.pf_init             = DvdReadInit;
    input.pf_open             = DvdReadOpen;
    input.pf_close            = DvdReadClose;
    input.pf_end              = DvdReadEnd;
    input.pf_init_bit_stream  = InitBitstream;
    input.pf_read             = DvdReadRead;
    input.pf_set_area         = DvdReadSetArea;
    input.pf_demux            = input_DemuxPS;
    input.pf_new_packet       = input_NewPacket;
    input.pf_new_pes          = input_NewPES;
    input.pf_delete_packet    = input_DeletePacket;
    input.pf_delete_pes       = input_DeletePES;
    input.pf_rewind           = DvdReadRewind;
    input.pf_seek             = DvdReadSeek;
#undef input
}

/*
 * Data reading functions
 */

/*****************************************************************************
 * DvdReadProbe: verifies that the stream is a PS stream
 *****************************************************************************/
static int DvdReadProbe( probedata_t *p_data )
{
    input_thread_t * p_input = (input_thread_t *)p_data;

    char * psz_name = p_input->p_source;
    int i_score = 5;

    if( TestMethod( INPUT_METHOD_VAR, "dvdread" ) )
    {
        return( 999 );
    }

    if( ( strlen(psz_name) > 8 ) && !strncasecmp( psz_name, "dvdread:", 8 ) )
    {
        /* If the user specified "dvdread:" then he probably wants
         * to use libdvdread */
        i_score = 100;
        psz_name += 4;
    }

    return( i_score );
}

/*****************************************************************************
 * DvdReadInit: initializes DVD structures
 *****************************************************************************/
static void DvdReadInit( input_thread_t * p_input )
{
    thread_dvd_data_t *  p_dvd;
    input_area_t *       p_area;
    int                  i_title;
    int                  i_chapter;
    int                  i;

    p_dvd = malloc( sizeof(thread_dvd_data_t) );
    if( p_dvd == NULL )
    {
        intf_ErrMsg( "dvdread error: out of memory" );
        p_input->b_error = 1;
        return;
    }

    /* we take the pointer to dvd_reader_t back  */
    p_dvd->p_dvdread = (dvd_reader_t *)p_input->p_plugin_data;
    p_dvd->p_title = NULL;
    p_dvd->p_vts_file = NULL;

    p_input->p_plugin_data = (void *)p_dvd;

    if( (p_input->p_method_data = input_BuffersInit()) == NULL )
    {
        free( p_dvd );
        p_input->b_error = 1;
        return;
    }

    /* We read DVD_BLOCK_READ_ONCE in each loop, so the input will receive
     * DVD_DATA_READ_ONCE at most */
    p_dvd->i_block_once = DVD_BLOCK_READ_ONCE;

    /* Ifo allocation & initialisation */
    if( ! ( p_dvd->p_vmg_file = ifoOpen( p_dvd->p_dvdread, 0 ) ) )
    {
        intf_ErrMsg( "dvdread error: can't open VMG info" );
        DVDClose( p_dvd->p_dvdread );
        input_BuffersEnd( p_input->p_method_data );
        free( p_dvd );
        p_input->b_error = 1;
        return;
    }
    intf_WarnMsg( 2, "dvdread info: VMG opened" );

    /* Set stream and area data */
    vlc_mutex_lock( &p_input->stream.stream_lock );

    /* Initialize ES structures */
    input_InitStream( p_input, sizeof( stream_ps_data_t ) );

    /* disc input method */
    p_input->stream.i_method = INPUT_METHOD_DVD;

#define tt_srpt p_dvd->p_vmg_file->tt_srpt
    intf_WarnMsg( 2, "dvdread info: number of titles: %d", tt_srpt->nr_of_srpts );

#define area p_input->stream.pp_areas
    /* We start from 1 here since the default area 0
     * is reserved for video_ts.vob */
    for( i = 1 ; i <= tt_srpt->nr_of_srpts ; i++ )
    {
        input_AddArea( p_input );

        /* Titles are Program Chains */
        area[i]->i_id = i;

        /* Absolute start offset and size
         * We can only set that with vts ifo, so we do it during the
         * first call to DVDSetArea */
        area[i]->i_start = 0;
        area[i]->i_size = 0;

        /* Number of chapters */
        area[i]->i_part_nb = tt_srpt->title[i-1].nr_of_ptts;
        area[i]->i_part = 1;

        /* Number of angles */
        area[i]->i_angle_nb = 0;
        area[i]->i_angle = 1;

        area[i]->i_plugin_data = tt_srpt->title[i-1].title_set_nr;
    }
#undef area

    /* Get requested title - if none try the first title */
    i_title = main_GetIntVariable( INPUT_TITLE_VAR, 1 );
    if( i_title <= 0 || i_title > tt_srpt->nr_of_srpts )
    {
        i_title = 1;
    }

#undef tt_srpt

    /* Get requested chapter - if none defaults to first one */
    i_chapter = main_GetIntVariable( INPUT_CHAPTER_VAR, 1 );
    if( i_chapter <= 0 )
    {
        i_chapter = 1;
    }

    p_input->stream.pp_areas[i_title]->i_part = i_chapter;

    p_area = p_input->stream.pp_areas[i_title];

    /* set title, chapter, audio and subpic */
    DvdReadSetArea( p_input, p_area );

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return;
}

/*****************************************************************************
 * DvdReadOpen: open libdvdread
 *****************************************************************************/
static void DvdReadOpen( struct input_thread_s *p_input )
{
    dvd_reader_t    *p_dvdread;

    vlc_mutex_lock( &p_input->stream.stream_lock );

    /* If we are here we can control the pace... */
    p_input->stream.b_pace_control = 1;

    p_input->stream.b_seekable = 1;
    p_input->stream.p_selected_area->i_size = 0;

    p_input->stream.p_selected_area->i_tell = 0;

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /* XXX: put this shit in an access plugin */
    if( strlen( p_input->p_source ) > 8
         && !strncasecmp( p_input->p_source, "dvdread:", 8 ) )
    {
        p_dvdread = DVDOpen( p_input->p_source + 8 );
    }
    else
    {
        p_dvdread = DVDOpen( p_input->p_source );
    }

    if( ! p_dvdread )
    {
        p_input->b_error = 1;
        return;
    }

    /* the vlc input doesn't handle the file descriptor with libdvdread */
    p_input->i_handle = -1;

    /* p_dvdread is stored in p_plugin_data until we have
     * malloc'ed a dvd_thread_data_t */
    p_input->p_plugin_data = (void *) p_dvdread;
}

/*****************************************************************************
 * DvdReadClose: close libdvdread
 *****************************************************************************/
static void DvdReadClose( struct input_thread_s *p_input )
{
    DVDClose( ((thread_dvd_data_t*)p_input->p_plugin_data)->p_dvdread );
    free( p_input->p_plugin_data );
    p_input->p_plugin_data = NULL;

}

/*****************************************************************************
 * DvdReadEnd: frees unused data
 *****************************************************************************/
static void DvdReadEnd( input_thread_t * p_input )
{
    thread_dvd_data_t *     p_dvd;

    p_dvd = (thread_dvd_data_t *)p_input->p_plugin_data;

    /* close libdvdread */
    DVDCloseFile( p_dvd->p_title );
    ifoClose( p_dvd->p_vts_file );
    ifoClose( p_dvd->p_vmg_file );

    input_BuffersEnd( p_input->p_method_data );
}

#define p_pgc         p_dvd->p_cur_pgc

/*****************************************************************************
 * DvdReadSetArea: initialize input data for title x, chapter y.
 * It should be called for each user navigation request.
 *****************************************************************************
 * Take care that i_title starts from 0 (vmg) and i_chapter start from 1.
 * Note that you have to take the lock before entering here.
 *****************************************************************************/
static int DvdReadSetArea( input_thread_t * p_input, input_area_t * p_area )
{
    thread_dvd_data_t *  p_dvd;
    int                  pgc_id = 0;
    int                  pgn = 0;

    p_dvd = (thread_dvd_data_t*)p_input->p_plugin_data;

    /* we can't use the interface slider until initilization is complete */
    p_input->stream.b_seekable = 0;

    if( p_area != p_input->stream.p_selected_area )
    {
        es_descriptor_t *    p_es;
        int                  i_cell = 0;
        int                  i_audio_nb = 0;
        int                  i_spu_nb = 0;
        int                  i;

#define p_vmg         p_dvd->p_vmg_file
#define p_vts         p_dvd->p_vts_file
        if( p_dvd->p_title != NULL )
        {
            DVDCloseFile( p_dvd->p_title );
        }

        if( p_vts != NULL )
        {
            ifoClose( p_vts );
        }

        /* Reset the Chapter position of the old title */
        p_input->stream.p_selected_area->i_part = 1;

        /*
         *  We have to load all title information
         */
        /* Change the default area */
        p_input->stream.p_selected_area = p_area;

        intf_WarnMsg( 12, "dvdread: open VTS %d, for title %d",
            p_vmg->tt_srpt->title[ p_area->i_id - 1 ].title_set_nr,
            p_area->i_id );

        /* ifo vts */
        if( ! ( p_vts = ifoOpen( p_dvd->p_dvdread,
                p_vmg->tt_srpt->title[ p_area->i_id - 1 ].title_set_nr ) ) )
        {
            intf_ErrMsg( "dvdread error: fatal error in vts ifo" );
            ifoClose( p_vmg );
            DVDClose( p_dvd->p_dvdread );
            p_input->b_error = 1;
            return -1;
        }

        /* title position inside the selected vts */
        p_dvd->i_ttn = p_vmg->tt_srpt->title[ p_area->i_id - 1 ].vts_ttn;

        /*
         * Set selected title start
         */
        pgc_id = p_vts->vts_ptt_srpt->title[p_dvd->i_ttn-1].ptt[0].pgcn;
        pgn = p_vts->vts_ptt_srpt->title[p_dvd->i_ttn-1].ptt[0].pgn;
        p_pgc = p_vts->vts_pgcit->pgci_srp[ pgc_id - 1 ].pgc;
        i_cell = p_pgc->program_map[ pgn - 1 ] - 1;

        p_area->i_start =
            LB2OFF( p_dvd->p_cur_pgc->cell_playback[ i_cell ].first_sector );

        intf_WarnMsg( 3, "dvdread: start %d vts_title %d pgc %d pgn %d",
                         p_area->i_id, p_dvd->i_ttn, pgc_id, pgn );

        /*
         * Find title end
         */
        i_cell = p_dvd->p_cur_pgc->nr_of_cells - 1;

        p_dvd->i_end_block = p_pgc->cell_playback[ i_cell ].last_sector;
        p_area->i_size = LB2OFF( p_dvd->i_end_block )- p_area->i_start;

        intf_WarnMsg( 12, "dvdread: start %lld size %lld end %d",
                          p_area->i_start , p_area->i_size, p_dvd->i_end_block );

        /*
         * Set properties for current chapter
         */
        /* Remeber current chapter */
        p_dvd->i_chapter = p_area->i_part;
        p_dvd->b_eoc = 0;

        pgc_id = p_vts->vts_ptt_srpt->title[
                    p_dvd->i_ttn-1].ptt[p_area->i_part-1].pgcn;
        pgn = p_vts->vts_ptt_srpt->title[
                    p_dvd->i_ttn-1].ptt[p_area->i_part-1].pgn;

        p_pgc = p_vts->vts_pgcit->pgci_srp[pgc_id-1].pgc;
        p_dvd->i_pack_len = 0;
        p_dvd->i_next_cell = p_dvd->i_cur_cell = p_pgc->program_map[pgn-1] - 1;
        DvdReadFindCell( p_dvd );

        p_dvd->i_next_vobu = p_dvd->i_cur_block =
            p_pgc->cell_playback[p_dvd->i_cur_cell].first_sector;

        /*
         * Angle management
         */
        p_area->i_angle_nb = p_vmg->tt_srpt->title[p_area->i_id-1].nr_of_angles;
        p_area->i_angle = main_GetIntVariable( INPUT_ANGLE_VAR, 1 );

        if( ( p_area->i_angle <= 0 ) || p_area->i_angle > p_area->i_angle_nb )
        {
            p_area->i_angle = 1;
        }
        p_dvd->i_angle = p_area->i_angle;
        p_dvd->i_angle_nb = p_area->i_angle_nb;

        /*
         * We've got enough info, time to open the title set data.
         */
        if( ! ( p_dvd->p_title = DVDOpenFile( p_dvd->p_dvdread,
            p_vmg->tt_srpt->title[ p_area->i_id - 1 ].title_set_nr,
            DVD_READ_TITLE_VOBS ) ) )
        {
            intf_ErrMsg( "dvdread error: can't open title (VTS_%02d_1.VOB)",
                         p_vmg->tt_srpt->title[p_area->i_id-1].title_set_nr );
            ifoClose( p_vts );
            ifoClose( p_vmg );
            DVDClose( p_dvd->p_dvdread );
            return -1;
        }

//        IfoPrintTitle( p_dvd );

        /*
         * Destroy obsolete ES by reinitializing program 0
         * and find all ES in title with ifo data
         */
        if( p_input->stream.pp_programs != NULL )
        {
            /* We don't use input_EndStream here since
             * we keep area structures */

            for( i = 0 ; i < p_input->stream.i_selected_es_number ; i++ )
            {
                input_UnselectES( p_input, p_input->stream.pp_selected_es[i] );
            }

            free( p_input->stream.pp_selected_es );
            input_DelProgram( p_input, p_input->stream.pp_programs[0] );

            p_input->stream.pp_selected_es = NULL;
            p_input->stream.i_selected_es_number = 0;
        }

        input_AddProgram( p_input, 0, sizeof( stream_ps_data_t ) );

        /* No PSM to read in DVD mode, we already have all information */
        p_input->stream.pp_programs[0]->b_is_ok = 1;

        p_es = NULL;

        /* ES 0 -> video MPEG2 */
//        IfoPrintVideo( p_dvd );

        p_es = input_AddES( p_input, p_input->stream.pp_programs[0], 0xe0, 0 );
        p_es->i_stream_id = 0xe0;
        p_es->i_type = MPEG2_VIDEO_ES;
        p_es->i_cat = VIDEO_ES;
        if( p_main->b_video )
        {
            input_SelectES( p_input, p_es );
        }

#define audio_control \
    p_dvd->p_vts_file->vts_pgcit->pgci_srp[pgc_id-1].pgc->audio_control[i-1]
        /* Audio ES, in the order they appear in .ifo */
        for( i = 1 ; i <= p_vts->vtsi_mat->nr_of_vts_audio_streams ; i++ )
        {
            int i_position = 0;
            u16 i_id;

//            IfoPrintAudio( p_dvd, i );

            /* audio channel is active if first byte is 0x80 */
            if( audio_control & 0x8000 )
            {
                i_audio_nb++;
                i_position = ( audio_control & 0x7F00 ) >> 8;

            intf_WarnMsg( 12, "dvd audio position  %d", i_position );
                switch( p_vts->vtsi_mat->vts_audio_attr[i-1].audio_format )
                {
                case 0x00:              /* AC3 */
                    i_id = ( ( 0x80 + i_position ) << 8 ) | 0xbd;
                    p_es = input_AddES( p_input,
                               p_input->stream.pp_programs[0], i_id, 0 );
                    p_es->i_stream_id = 0xbd;
                    p_es->i_type = AC3_AUDIO_ES;
                    p_es->b_audio = 1;
                    p_es->i_cat = AUDIO_ES;
                    strcpy( p_es->psz_desc, DecodeLanguage( hton16(
                        p_vts->vtsi_mat->vts_audio_attr[i-1].lang_code ) ) ); 
                    strcat( p_es->psz_desc, " (ac3)" );

                    break;
                case 0x02:
                case 0x03:              /* MPEG audio */
                    i_id = 0xc0 + i_position;
                    p_es = input_AddES( p_input,
                                    p_input->stream.pp_programs[0], i_id, 0 );
                    p_es->i_stream_id = i_id;
                    p_es->i_type = MPEG2_AUDIO_ES;
                    p_es->b_audio = 1;
                    p_es->i_cat = AUDIO_ES;
                    strcpy( p_es->psz_desc, DecodeLanguage( hton16(
                        p_vts->vtsi_mat->vts_audio_attr[i-1].lang_code ) ) ); 
                    strcat( p_es->psz_desc, " (mpeg)" );

                    break;
                case 0x04:              /* LPCM */

                    i_id = ( ( 0xa0 + i_position ) << 8 ) | 0xbd;
                    p_es = input_AddES( p_input,
                                    p_input->stream.pp_programs[0], i_id, 0 );
                    p_es->i_stream_id = i_id;
                    p_es->i_type = LPCM_AUDIO_ES;
                    p_es->b_audio = 1;
                    p_es->i_cat = AUDIO_ES;
                    strcpy( p_es->psz_desc, DecodeLanguage( hton16(
                        p_vts->vtsi_mat->vts_audio_attr[i-1].lang_code ) ) ); 
                    strcat( p_es->psz_desc, " (lpcm)" );

                    break;
                case 0x06:              /* DTS */
                    i_id = ( ( 0x88 + i_position ) << 8 ) | 0xbd;
                    intf_ErrMsg( "dvd warning: DTS audio not handled yet"
                                 "(0x%x)", i_id );
                    break;
                default:
                    i_id = 0;
                    intf_ErrMsg( "dvd warning: unknown audio type %.2x",
                             p_vts->vtsi_mat->vts_audio_attr[i-1].audio_format );
                }
            }
        }
#undef audio_control
#define spu_control \
    p_dvd->p_vts_file->vts_pgcit->pgci_srp[pgc_id-1].pgc->subp_control[i-1]

        /* Sub Picture ES */

        for( i = 1 ; i <= p_vts->vtsi_mat->nr_of_vts_subp_streams; i++ )
        {
            int i_position = 0;
            u16 i_id;

//            IfoPrintSpu( p_dvd, i );
            intf_WarnMsg( 12, "dvd spu %d 0x%02x", i, spu_control );

            if( spu_control & 0x80000000 )
            {
                i_spu_nb++;

                /*  there are several streams for one spu */
                if(  p_vts->vtsi_mat->vts_video_attr.display_aspect_ratio )
                {
                    /* 16:9 */
                    switch( p_vts->vtsi_mat->vts_video_attr.permitted_df )
                    {
                    case 1:
                        i_position = spu_control & 0xff;
                        break;
                    case 2:
                        i_position = ( spu_control >> 8 ) & 0xff;
                        break;
                    default:
                        i_position = ( spu_control >> 16 ) & 0xff;
                        break;
                    }
                }
                else
                {
                    /* 4:3 */
                    i_position = ( spu_control >> 24 ) & 0x7F;
                }

                i_id = ( ( 0x20 + i_position ) << 8 ) | 0xbd;
                p_es = input_AddES( p_input,
                                    p_input->stream.pp_programs[0], i_id, 0 );
                p_es->i_stream_id = 0xbd;
                p_es->i_type = DVD_SPU_ES;
                p_es->i_cat = SPU_ES;
                strcpy( p_es->psz_desc, DecodeLanguage( hton16(
                    p_vts->vtsi_mat->vts_subp_attr[i-1].lang_code ) ) ); 
            }
        }
#undef spu_control
        if( p_main->b_audio )
        {
            /* For audio: first one if none or a not existing one specified */
            int i_audio = main_GetIntVariable( INPUT_CHANNEL_VAR, 1 );
            if( i_audio < 0 || i_audio > i_audio_nb )
            {
                main_PutIntVariable( INPUT_CHANNEL_VAR, 1 );
                i_audio = 1;
            }
            if( i_audio > 0 && i_audio_nb > 0 )
            {
                if( main_GetIntVariable( AOUT_SPDIF_VAR, 0 ) ||
                    ( main_GetIntVariable( INPUT_AUDIO_VAR, 0 ) ==
                      REQUESTED_AC3 ) )
                {
                    int     i_ac3 = i_audio;
                    while( ( p_input->stream.pp_es[i_ac3]->i_type !=
                             AC3_AUDIO_ES ) && ( i_ac3 <=
                             p_vts->vtsi_mat->nr_of_vts_audio_streams ) )
                    {
                        i_ac3++;
                    }
                    if( p_input->stream.pp_es[i_ac3]->i_type == AC3_AUDIO_ES )
                    {
                        input_SelectES( p_input,
                                        p_input->stream.pp_es[i_ac3] );
                    }
                }
                else
                {
                    input_SelectES( p_input,
                                    p_input->stream.pp_es[i_audio] );
                }
            }
        }

        if( p_main->b_video )
        {
            /* for spu, default is none */
            int i_spu = main_GetIntVariable( INPUT_SUBTITLE_VAR, 0 );
            if( i_spu < 0 || i_spu > i_spu_nb )
            {
                main_PutIntVariable( INPUT_SUBTITLE_VAR, 0 );
                i_spu = 0;
            }
            if( i_spu > 0 && i_spu_nb > 0 )
            {
                i_spu += p_vts->vtsi_mat->nr_of_vts_audio_streams;
                input_SelectES( p_input, p_input->stream.pp_es[i_spu] );
            }
        }
    } /* i_title >= 0 */
    else
    {
        p_area = p_input->stream.p_selected_area;
    }

    /*
     * Chapter selection
     */

    if( p_area->i_part != p_dvd->i_chapter )
    {
        if( ( p_area->i_part > 0 ) &&
            ( p_area->i_part <= p_area->i_part_nb ))
        {
            p_dvd->i_ttn = p_vmg->tt_srpt->title[p_area->i_id-1].vts_ttn;
            pgc_id = p_vts->vts_ptt_srpt->title[
                        p_dvd->i_ttn-1].ptt[p_area->i_part-1].pgcn;
            pgn = p_vts->vts_ptt_srpt->title[
                        p_dvd->i_ttn-1].ptt[p_area->i_part-1].pgn;

            p_pgc = p_vts->vts_pgcit->pgci_srp[ pgc_id - 1 ].pgc;

            p_dvd->i_cur_cell = p_pgc->program_map[ pgn - 1 ] - 1;
            p_dvd->i_chapter = p_area->i_part;
            DvdReadFindCell( p_dvd );

            p_dvd->i_pack_len = 0;
            p_dvd->i_next_vobu = p_dvd->i_cur_block =
                    p_pgc->cell_playback[p_dvd->i_cur_cell].first_sector;
        }
        else
        {
            p_area->i_part = p_dvd->i_chapter;
        }
    }
#undef p_vts
#undef p_vmg

    if( p_area->i_angle != p_dvd->i_angle )
    {
        p_dvd->i_angle = p_area->i_angle;

        intf_WarnMsg( 3, "dvd info: angle %d selected", p_area->i_angle );
    }
    /* warn interface that something has changed */
    p_area->i_tell = LB2OFF( p_dvd->i_next_vobu ) - p_area->i_start;
    p_input->stream.b_seekable = 1;
    p_input->stream.b_changed = 1;

    return 0;
}


/*****************************************************************************
 * DvdReadRead: reads data packets into the netlist.
 *****************************************************************************
 * Returns -1 in case of error, 0 if everything went well, and 1 in case of
 * EOF.
 *****************************************************************************/
static int DvdReadRead( input_thread_t * p_input,
                        data_packet_t ** pp_data )
{
    thread_dvd_data_t *     p_dvd;
    u8                      p_data[DVD_VIDEO_LB_LEN];
    struct iovec            p_vec[DVD_DATA_READ_ONCE];
    u8 *                    pi_cur;
    int                     i_blocks;
    int                     i_read;
    int                     i_iovec;
    int                     i_packet_size;
    int                     i_packet;
    int                     i_pos;
    data_packet_t *         p_data_p;

    p_dvd = (thread_dvd_data_t *)p_input->p_plugin_data;

    *pp_data = NULL;

    /*
     * Playback by cell in this pgc, starting at the cell for our chapter.
     */

    /* 
     * End of pack, we select the following one
     */
    if( ! p_dvd->i_pack_len )
    {
        /*
         * Read NAV packet.
         */
        if( ( i_read = DVDReadBlocks( p_dvd->p_title, p_dvd->i_next_vobu,
                       1, p_data ) ) != 1 )
        {
            intf_ErrMsg( "dvdread error: read failed for block %d",
                         p_dvd->i_next_vobu );
            return -1;
        }

        assert( p_data[41] == 0xbf && p_data[1027] == 0xbf );

        /*
         * Parse the contained dsi packet.
         */

        DvdReadHandleDSI( p_dvd, p_data );

        /* End of File */
        if( p_dvd->i_next_vobu >= p_dvd->i_end_block + 1 )
        {
            return 1;
        }

        assert( p_dvd->i_pack_len < 1024 );
        /* Ugly kludge: we send the pack block to the input for it
         * sometimes has a zero scr and restart the sync */
//        p_dvd->i_cur_block ++;
        p_dvd->i_pack_len++;

    }

    /*
     * Compute the number of blocks to read
     */
    i_blocks = p_dvd->i_pack_len >= DVD_BLOCK_READ_ONCE
             ? DVD_BLOCK_READ_ONCE : p_dvd->i_pack_len;
    p_dvd->i_pack_len -= i_blocks;

    /* Get iovecs */
    *pp_data = p_data_p = input_BuffersToIO( p_input->p_method_data, p_vec,
                                             DVD_DATA_READ_ONCE );

    if ( p_data_p == NULL )
    {
        return( -1 );
    }

    /* Reads from DVD */
    i_read = DVDReadVBlocks( p_dvd->p_title, p_dvd->i_cur_block,
                             i_blocks, p_vec );
    if( i_read != i_blocks )
    {
        intf_ErrMsg( "dvdread error: read failed for %d/%d blocks at 0x%02x",
                     i_read, i_blocks, p_dvd->i_cur_block );
        return -1;
    }

    p_dvd->i_cur_block += i_read;
/*
    intf_WarnMsg( 12, "dvdread i_blocks: %d len: %d current: 0x%02x", i_read, p_dvd->i_pack_len, p_dvd->i_cur_block );
*/
    i_packet = 0;

    /* Read headers to compute payload length */
    for( i_iovec = 0 ; i_iovec < i_read ; i_iovec++ )
    {
        data_packet_t * p_current = p_data_p;
        i_pos = 0;

        while( i_pos < DVD_VIDEO_LB_LEN )
        {
            pi_cur = (u8*)p_vec[i_iovec].iov_base + i_pos;

            /*default header */
            if( U32_AT( pi_cur ) != 0x1BA )
            {
                /* That's the case for all packets, except pack header. */
                i_packet_size = U16_AT( pi_cur + 4 );
            }
            else
            {
                /* MPEG-2 Pack header. */
                i_packet_size = 8;
            }
            if( i_pos != 0 )
            {
                *pp_data = input_ShareBuffer( p_input->p_method_data,
                                              p_current );
            }
            else
            {
                *pp_data = p_data_p;
                p_data_p = p_data_p->p_next;
            }

            (*pp_data)->p_payload_start = (*pp_data)->p_demux_start =
                    (*pp_data)->p_demux_start + i_pos;
            

            (*pp_data)->p_payload_end =
                    (*pp_data)->p_payload_start + i_packet_size + 6;

            i_packet++;
            i_pos += i_packet_size + 6;
            pp_data = &(*pp_data)->p_next;
        }
    }

    p_input->pf_delete_packet( p_input->p_method_data, p_data_p );
    *pp_data = NULL;

    vlc_mutex_lock( &p_input->stream.stream_lock );

    p_input->stream.p_selected_area->i_tell =
        LB2OFF( p_dvd->i_cur_block ) -
            p_input->stream.p_selected_area->i_start;

    if( p_dvd->b_eoc )
    {
        /* We modify i_part only at end of chapter not to erase
         * some modification from the interface */
        p_input->stream.p_selected_area->i_part = p_dvd->i_chapter;
        p_dvd->b_eoc = 0;
    }


    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return 0;
}
#undef p_pgc

/*****************************************************************************
 * DVDRewind : reads a stream backward
 *****************************************************************************/
static int DvdReadRewind( input_thread_t * p_input )
{
    return( -1 );
}

/*****************************************************************************
 * DvdReadSeek : Goes to a given position on the stream.
 *****************************************************************************
 * This one is used by the input and translate chronological position from
 * input to logical position on the device.
 * The lock should be taken before calling this function.
 *****************************************************************************/
static void DvdReadSeek( input_thread_t * p_input, off_t i_off )
{
    thread_dvd_data_t *     p_dvd;
    int                     i_lb;
    int                     i_tmp;
    int                     i_chapter = 0;
    int                     i_cell = 0;
    int                     i_vobu = 0;
    int                     i_sub_cell = 0;

    i_off += p_input->stream.p_selected_area->i_start;
    i_lb = OFF2LB( i_off );
    p_dvd = ( thread_dvd_data_t * )p_input->p_plugin_data;

    /* find cell */
    while( p_dvd->p_cur_pgc->cell_playback[i_cell].last_sector < i_lb )
    {
        i_cell++;
    }

    /* find chapter */
    do
    {
        pgc_t *     p_pgc;
        int         pgc_id, pgn;

        i_chapter++;
        pgc_id = p_dvd->p_vts_file->vts_ptt_srpt->title[
                    p_dvd->i_ttn-1].ptt[i_chapter].pgcn;
        pgn = p_dvd->p_vts_file->vts_ptt_srpt->title[
                    p_dvd->i_ttn-1].ptt[i_chapter].pgn;

        p_pgc = p_dvd->p_vts_file->vts_pgcit->pgci_srp[pgc_id-1].pgc;
        i_tmp = p_pgc->program_map[pgn-1];

    } while( i_tmp <= i_cell );

    /* find vobu */
    while( p_dvd->p_vts_file->vts_vobu_admap->vobu_start_sectors[i_vobu]
            <= i_lb )
    {
        i_vobu++;
    }

    /* find sub_cell */
    while( p_dvd->p_vts_file->vts_c_adt->cell_adr_table[i_sub_cell].start_sector <
            p_dvd->p_vts_file->vts_vobu_admap->vobu_start_sectors[i_vobu-1] )
    {
        i_sub_cell++;
    }

/*
    intf_WarnMsg(12, "cell %d i_sub_cell %d chapter %d vobu %d cell_sector %d vobu_sector %d sub_cell_sector %d",
            i_cell, i_sub_cell,i_chapter, i_vobu,
            p_dvd->p_cur_pgc->cell_playback[i_cell].first_sector,
            p_dvd->p_vts_file->vts_vobu_admap->vobu_start_sectors[i_vobu],
            p_dvd->p_vts_file->vts_c_adt->cell_adr_table[i_sub_cell-1].start_sector);
*/
    p_dvd->i_cur_block = i_lb;
    p_dvd->i_next_vobu =
        p_dvd->p_vts_file->vts_vobu_admap->vobu_start_sectors[i_vobu];
    p_dvd->i_pack_len = p_dvd->i_next_vobu - i_lb;
    p_dvd->i_cur_cell = i_cell;
    p_dvd->i_chapter = i_chapter;
    DvdReadFindCell( p_dvd );

    p_input->stream.p_selected_area->i_tell =
        LB2OFF ( p_dvd->i_cur_block )
         - p_input->stream.p_selected_area->i_start;
    p_input->stream.p_selected_area->i_part = p_dvd->i_chapter;

    return;
}

/*****************************************************************************
 * DvdReadHandleDSI
 *****************************************************************************/
static void DvdReadHandleDSI( thread_dvd_data_t * p_dvd, u8 * p_data )
{
    navRead_DSI( &(p_dvd->dsi_pack), &(p_data[ DSI_START_BYTE ]) );

    /*
     * Determine where we go next.  These values are the ones we mostly
     * care about.
     */
    p_dvd->i_cur_block = p_dvd->dsi_pack.dsi_gi.nv_pck_lbn;

    /*
     * If we're not at the end of this cell, we can determine the next
     * VOBU to display using the VOBU_SRI information section of the
     * DSI.  Using this value correctly follows the current angle,
     * avoiding the doubled scenes in The Matrix, and makes our life
     * really happy.
     */
    if( p_dvd->dsi_pack.vobu_sri.next_vobu != SRI_END_OF_CELL )
    {
#if 1
        switch( ( p_dvd->dsi_pack.sml_pbi.category & 0xf000 ) >> 12 )
        {
            case 0x4:
                /* interleaved unit with no angle */
                if( p_dvd->dsi_pack.sml_pbi.ilvu_sa != -1 )
                {
                    p_dvd->i_next_vobu = p_dvd->i_cur_block +
                        p_dvd->dsi_pack.sml_pbi.ilvu_sa;
                    p_dvd->i_pack_len = p_dvd->dsi_pack.sml_pbi.ilvu_ea;
                }
                else
                {
                    p_dvd->i_next_vobu = p_dvd->i_cur_block +
                        p_dvd->dsi_pack.dsi_gi.vobu_ea + 1;
                    p_dvd->i_pack_len = p_dvd->dsi_pack.dsi_gi.vobu_ea;
                }
                break;
            case 0x5:
                /* vobu is end of ilvu */
                if( p_dvd->dsi_pack.sml_agli.data[p_dvd->i_angle-1].address )
                {
                    p_dvd->i_next_vobu = p_dvd->i_cur_block +
                        p_dvd->dsi_pack.sml_agli.data[p_dvd->i_angle-1].address;
                    p_dvd->i_pack_len = p_dvd->dsi_pack.sml_pbi.ilvu_ea;

                    break;
                }
            case 0x6:
                /* vobu is beginning of ilvu */
            case 0x9:
                /* next scr is 0 */
            case 0xa:
                /* entering interleaved section */
            case 0x8:
                /* non interleaved cells in interleaved section */
            default:
                p_dvd->i_next_vobu = p_dvd->i_cur_block +
                    ( p_dvd->dsi_pack.vobu_sri.next_vobu & 0x7fffffff );
                p_dvd->i_pack_len = p_dvd->dsi_pack.dsi_gi.vobu_ea;
                break;
        }
#else
        p_dvd->i_next_vobu = p_dvd->i_cur_block +
            ( p_dvd->dsi_pack.vobu_sri.next_vobu & 0x7fffffff );
        p_dvd->i_pack_len = p_dvd->dsi_pack.dsi_gi.vobu_ea;
#endif
    }
    else
    {
        p_dvd->i_cur_cell = p_dvd->i_next_cell;
        DvdReadFindCell( p_dvd );

        p_dvd->i_pack_len = p_dvd->dsi_pack.dsi_gi.vobu_ea;
        p_dvd->i_next_vobu =
            p_dvd->p_cur_pgc->cell_playback[p_dvd->i_cur_cell].first_sector;
    }

#if 0
    intf_WarnMsg( 12, "scr %d lbn 0x%02x vobu_ea %d vob_id %d c_id %d",
            p_dvd->dsi_pack.dsi_gi.nv_pck_scr,
            p_dvd->dsi_pack.dsi_gi.nv_pck_lbn,
            p_dvd->dsi_pack.dsi_gi.vobu_ea,
            p_dvd->dsi_pack.dsi_gi.vobu_vob_idn,
            p_dvd->dsi_pack.dsi_gi.vobu_c_idn );

    intf_WarnMsg( 12, "cat 0x%02x ilvu_ea %d ilvu_sa %d size %d", 
            p_dvd->dsi_pack.sml_pbi.category,
            p_dvd->dsi_pack.sml_pbi.ilvu_ea,
            p_dvd->dsi_pack.sml_pbi.ilvu_sa,
            p_dvd->dsi_pack.sml_pbi.size );

    intf_WarnMsg( 12, "next_vobu %d next_ilvu1 %d next_ilvu2 %d",
            p_dvd->dsi_pack.vobu_sri.next_vobu & 0x7fffffff,
            p_dvd->dsi_pack.sml_agli.data[ p_dvd->i_angle - 1 ].address,
            p_dvd->dsi_pack.sml_agli.data[ p_dvd->i_angle ].address);
#endif
}

/*****************************************************************************
 * DvdReadFindCell
 *****************************************************************************/
static void DvdReadFindCell( thread_dvd_data_t * p_dvd )
{
    int         pgc_id, pgn;
    int         i = 0;
    pgc_t *     p_pgc;
#define cell p_dvd->p_cur_pgc->cell_playback
    if( cell[p_dvd->i_cur_cell].block_type == BLOCK_TYPE_ANGLE_BLOCK )
    {
#if 0
        p_dvd->i_next_cell = p_dvd->i_cur_cell + p_dvd->i_angle_nb;
        p_dvd->i_cur_cell += p_dvd->i_angle - 1;
#else
        p_dvd->i_cur_cell += p_dvd->i_angle - 1;

        while( cell[p_dvd->i_cur_cell+i].block_mode != BLOCK_MODE_LAST_CELL )
        {
            i++;
        }
        p_dvd->i_next_cell = p_dvd->i_cur_cell + i + 1;
#endif
    }
    else
    {
        p_dvd->i_next_cell = p_dvd->i_cur_cell + 1;
    }
#undef cell
    pgc_id = p_dvd->p_vts_file->vts_ptt_srpt->title[
                p_dvd->i_ttn-1].ptt[p_dvd->i_chapter].pgcn;
    pgn = p_dvd->p_vts_file->vts_ptt_srpt->title[
                p_dvd->i_ttn-1].ptt[p_dvd->i_chapter].pgn;
    p_pgc = p_dvd->p_vts_file->vts_pgcit->pgci_srp[pgc_id-1].pgc;

    if( p_pgc->program_map[pgn-1] <= p_dvd->i_cur_cell )
    {
        p_dvd->i_chapter++;
	p_dvd->b_eoc = 1;
    }
}
