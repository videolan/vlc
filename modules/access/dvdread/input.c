/*****************************************************************************
 * input.c: DvdRead plugin.
 *****************************************************************************
 * This plugins should handle all the known specificities of the DVD format,
 * especially the 2048 bytes logical block size.
 * It depends on: libdvdread for ifo files and block reading.
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: input.c,v 1.3 2002/08/29 23:53:22 massiot Exp $
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
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#ifdef STRNCASECMP_IN_STRINGS_H
#   include <strings.h>
#endif

#if defined( WIN32 )
#   include <io.h>                                                 /* read() */
#endif

#include <dvdread/dvd_reader.h>
#include <dvdread/ifo_types.h>
#include <dvdread/ifo_read.h>
#include <dvdread/nav_read.h>
#include <dvdread/nav_print.h>

#include "input.h"

#include "iso_lang.h"

/* how many blocks DVDRead will read in each loop */
#define DVD_BLOCK_READ_ONCE 64

/*****************************************************************************
 * Private structure
 *****************************************************************************/
struct demux_sys_t
{
    module_t *   p_module;
    mpeg_demux_t mpeg;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
/* called from outside */
static int  DvdReadDemux    ( input_thread_t * );
static int  DvdReadRewind   ( input_thread_t * );

static int  DvdReadSetArea    ( input_thread_t *, input_area_t * );
static int  DvdReadSetProgram ( input_thread_t *, pgrm_descriptor_t * );
static int  DvdReadRead       ( input_thread_t *, byte_t *, size_t );
static void DvdReadSeek       ( input_thread_t *, off_t );

/* called only from here */
static void DvdReadLauchDecoders( input_thread_t * p_input );
static void DvdReadHandleDSI( thread_dvd_data_t * p_dvd, u8 * p_data );
static void DvdReadFindCell ( thread_dvd_data_t * p_dvd );

/*
 * Data demux functions
 */

/*****************************************************************************
 * InitDVD: initialize DVD structures
 *****************************************************************************/
int E_(InitDVD) ( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t *   p_demux;

    if( p_input->stream.i_method != INPUT_METHOD_DVD )
    {
        return -1;
    }

    p_demux = p_input->p_demux_data = malloc( sizeof(demux_sys_t ) );
    if( p_demux == NULL )
    {
        return -1;
    }

    p_input->p_private = (void*)&p_demux->mpeg;
    p_demux->p_module = module_Need( p_input, "mpeg-system", NULL );
    if( p_demux->p_module == NULL )
    {
        free( p_input->p_demux_data );
        return -1;
    }

    p_input->pf_demux = DvdReadDemux;
    p_input->pf_rewind = NULL;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    
    DvdReadLauchDecoders( p_input );
    
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return 0;
}

/*****************************************************************************
 * EndDVD: end DVD structures
 *****************************************************************************/
void E_(EndDVD) ( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;

    module_Unneed( p_input, p_input->p_demux_data->p_module );
    free( p_input->p_demux_data );
}

/*****************************************************************************
 * DvdReadDemux
 *****************************************************************************/
#define PEEK( SIZE )                                                        \
    i_result = input_Peek( p_input, &p_peek, SIZE );                        \
    if( i_result == -1 )                                                    \
    {                                                                       \
        return( -1 );                                                       \
    }                                                                       \
    else if( i_result < SIZE )                                              \
    {                                                                       \
        /* EOF */                                                           \
        return( 0 );                                                        \
    }

static int DvdReadDemux( input_thread_t * p_input )
{
    int                 i;
    byte_t *            p_peek;
    data_packet_t *     p_data;
    ssize_t             i_result;
    int                 i_packet_size;


    /* Read headers to compute payload length */
    for( i = 0 ; i < DVD_BLOCK_READ_ONCE ; i++ )
    {

        /* Read what we believe to be a packet header. */
        PEEK( 4 );
            
        /* Default header */ 
        if( U32_AT( p_peek ) != 0x1BA )
        {
            /* That's the case for all packets, except pack header. */
            i_packet_size = U16_AT( p_peek + 4 );
        }
        else
        {
            /* MPEG-2 Pack header. */
            i_packet_size = 8;
        }

        /* Fetch a packet of the appropriate size. */
        i_result = input_SplitBuffer( p_input, &p_data, i_packet_size + 6 );
        if( i_result <= 0 )
        {
            return( i_result );
        }

        /* In MPEG-2 pack headers we still have to read stuffing bytes. */
        if( (p_data->p_demux_start[3] == 0xBA) && (i_packet_size == 8) )
        {
            size_t i_stuffing = (p_data->p_demux_start[13] & 0x7);
            /* Force refill of the input buffer - though we don't care
             * about p_peek. Please note that this is unoptimized. */
            PEEK( i_stuffing );
            p_input->p_current_data += i_stuffing;
        }

        p_input->p_demux_data->mpeg.pf_demux_ps( p_input, p_data );
     
    }

    return i;
}

/*****************************************************************************
 * DVDRewind : reads a stream backward
 *****************************************************************************/
static int DvdReadRewind( input_thread_t * p_input )
{
    return( -1 );
}

/*
 * Data access functions
 */

/*****************************************************************************
 * OpenDVD: open libdvdread
 *****************************************************************************/
int E_(OpenDVD) ( vlc_object_t *p_this )
{
    input_thread_t *        p_input = (input_thread_t *)p_this;
    char *                  psz_orig;
    char *                  psz_parser;
    char *                  psz_source;
    char *                  psz_next;
    struct stat             stat_info;
    thread_dvd_data_t *     p_dvd;
    dvd_reader_t *          p_dvdread;
    input_area_t *          p_area;
    int                     i_title = 1;
    int                     i_chapter = 1;
    int                     i_angle = 1;
    int                     i;

    psz_orig = psz_parser = psz_source = strdup( p_input->psz_name );
    if( !psz_orig )
    {
        return( -1 );
    }

    p_input->pf_read = DvdReadRead;
    p_input->pf_seek = DvdReadSeek;
    p_input->pf_set_area = DvdReadSetArea;
    p_input->pf_set_program = DvdReadSetProgram;

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
            if( *psz_next )
            {
                i_angle = (int)strtol( psz_next + 1, NULL, 10 );
            }
        }

        i_title = i_title ? i_title : 1;
        i_chapter = i_chapter ? i_chapter : 1;
        i_angle = i_angle ? i_angle : 1;
    }

    if( !*psz_source )
    {
        if( !p_input->psz_access )
        {
            free( psz_orig );
            return -1;
        }
        psz_source = config_GetPsz( p_input, "dvd" );
    }

    if( stat( psz_source, &stat_info ) == -1 )
    {
        msg_Err( p_input, "cannot stat() source `%s' (%s)",
                          psz_source, strerror(errno));
        return( -1 );
    }
    if( !S_ISBLK(stat_info.st_mode) &&
        !S_ISCHR(stat_info.st_mode) &&
        !S_ISDIR(stat_info.st_mode) )
    {
        msg_Warn( p_input, "dvdread module discarded (not a valid source)" );
        return -1;
    }
    
    msg_Dbg( p_input, "dvdroot=%s title=%d chapter=%d angle=%d",
                      psz_source, i_title, i_chapter, i_angle );
    

    p_dvdread = DVDOpen( psz_source );

    /* free allocated strings */
    if( psz_source != psz_orig )
        free( psz_source );
    free( psz_orig );

    if( ! p_dvdread )
    {
        msg_Err( p_input, "libdvdcss cannot open source" );
        return -1;
    }

    /* set up input  */
    p_input->i_mtu = 0;

    p_dvd = malloc( sizeof(thread_dvd_data_t) );
    if( p_dvd == NULL )
    {
        msg_Err( p_input, "out of memory" );
        return -1;
    }

    p_dvd->p_dvdread = p_dvdread;
    p_dvd->p_title = NULL;
    p_dvd->p_vts_file = NULL;


    p_input->p_access_data = (void *)p_dvd;

    /* Ifo allocation & initialisation */
    if( ! ( p_dvd->p_vmg_file = ifoOpen( p_dvd->p_dvdread, 0 ) ) )
    {
        msg_Err( p_input, "cannot open VMG info" );
        free( p_dvd );
        return -1;
    }
    msg_Dbg( p_input, "VMG opened" );

    /* Set stream and area data */
    vlc_mutex_lock( &p_input->stream.stream_lock );

    p_input->stream.i_method = INPUT_METHOD_DVD;

    /* If we are here we can control the pace... */
    p_input->stream.b_pace_control = 1;
    p_input->stream.b_seekable = 1;
    
    p_input->stream.p_selected_area->i_size = 0;
    p_input->stream.p_selected_area->i_tell = 0;

    /* Initialize ES structures */
    input_InitStream( p_input, sizeof( stream_ps_data_t ) );

    /* disc input method */
    p_input->stream.i_method = INPUT_METHOD_DVD;

#define tt_srpt p_dvd->p_vmg_file->tt_srpt
    msg_Dbg( p_input, "number of titles: %d", tt_srpt->nr_of_srpts );

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

        area[i]->i_plugin_data = tt_srpt->title[i-1].title_set_nr;
    }
#undef area

    p_dvd->i_title = i_title <= tt_srpt->nr_of_srpts ? i_title : 1;
#undef tt_srpt

    p_area = p_input->stream.pp_areas[p_dvd->i_title];
    p_dvd->i_chapter = i_chapter;

    p_dvd->i_chapter = i_chapter < p_area->i_part_nb ? i_chapter : 1;
    p_area->i_part = p_dvd->i_chapter;
    
    p_dvd->i_angle = i_angle;

    /* set title, chapter, audio and subpic */
    if( DvdReadSetArea( p_input, p_area ) )
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        return -1;
    }

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    p_input->psz_demux = "dvdread";

    return 0;
}

/*****************************************************************************
 * CloseDVD: close libdvdread
 *****************************************************************************/
void E_(CloseDVD) ( vlc_object_t *p_this )
{
    input_thread_t *    p_input = (input_thread_t *)p_this;
    thread_dvd_data_t * p_dvd = (thread_dvd_data_t *)p_input->p_access_data;

    /* close libdvdread */
    DVDCloseFile( p_dvd->p_title );
    ifoClose( p_dvd->p_vts_file );
    ifoClose( p_dvd->p_vmg_file );

    DVDClose( p_dvd->p_dvdread );
    free( p_dvd );
    p_input->p_access_data = NULL;

}

/*****************************************************************************
 * DvdReadSetProgram: Does nothing, a DVD is mono-program
 *****************************************************************************/
static int DvdReadSetProgram( input_thread_t * p_input,
                              pgrm_descriptor_t * p_program )
{
    if( p_input->stream.p_selected_program != p_program )
    {
        thread_dvd_data_t *  p_dvd;
    
        p_dvd = (thread_dvd_data_t*)(p_input->p_access_data);
        p_dvd->i_angle = p_program->i_number;

        memcpy( p_program, p_input->stream.p_selected_program,
                sizeof(pgrm_descriptor_t) );
        p_program->i_number = p_dvd->i_angle;
        p_input->stream.p_selected_program = p_program;

        msg_Dbg( p_input, "angle %d selected", p_dvd->i_angle );
    }

    return 0;
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

    p_dvd = (thread_dvd_data_t*)p_input->p_access_data;

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

        msg_Dbg( p_input, "open VTS %d, for title %d",
            p_vmg->tt_srpt->title[ p_area->i_id - 1 ].title_set_nr,
            p_area->i_id );

        /* ifo vts */
        if( ! ( p_vts = ifoOpen( p_dvd->p_dvdread,
                p_vmg->tt_srpt->title[ p_area->i_id - 1 ].title_set_nr ) ) )
        {
            msg_Err( p_input, "fatal error in vts ifo" );
            ifoClose( p_vmg );
            DVDClose( p_dvd->p_dvdread );
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

        msg_Dbg( p_input, "start %d vts_title %d pgc %d pgn %d",
                  p_area->i_id, p_dvd->i_ttn, pgc_id, pgn );

        /*
         * Find title end
         */
        i_cell = p_dvd->p_cur_pgc->nr_of_cells - 1;

        p_dvd->i_end_block = p_pgc->cell_playback[ i_cell ].last_sector;
        p_area->i_size = LB2OFF( p_dvd->i_end_block )- p_area->i_start;

        msg_Dbg( p_input, "start %lld size %lld end %d",
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
        p_dvd->i_angle_nb = p_vmg->tt_srpt->title[p_area->i_id-1].nr_of_angles;

        if( p_dvd->i_angle > p_dvd->i_angle_nb )
        {
            p_dvd->i_angle = 1;
        }

        /*
         * We've got enough info, time to open the title set data.
         */
        if( ! ( p_dvd->p_title = DVDOpenFile( p_dvd->p_dvdread,
            p_vmg->tt_srpt->title[ p_area->i_id - 1 ].title_set_nr,
            DVD_READ_TITLE_VOBS ) ) )
        {
            msg_Err( p_input, "cannot open title (VTS_%02d_1.VOB)",
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

            while( p_input->stream.i_es_number )
            {
                input_DelES( p_input, p_input->stream.pp_es[0] );
            }

            while( p_input->stream.i_pgrm_number )
            {
                input_DelProgram( p_input, p_input->stream.pp_programs[0] );
            }

            if( p_input->stream.pp_selected_es )
            {
                free( p_input->stream.pp_selected_es );
                p_input->stream.pp_selected_es = NULL;
            }
            p_input->stream.i_selected_es_number = 0;
        }

        input_AddProgram( p_input, 1, sizeof( stream_ps_data_t ) );
        p_input->stream.p_selected_program = p_input->stream.pp_programs[0];

        for( i = 1 ; i < p_dvd->i_angle_nb ; i++ )
        {
            input_AddProgram( p_input, i+1, 0 );
        }
        
        DvdReadSetProgram( p_input,
                           p_input->stream.pp_programs[p_dvd->i_angle-1] ); 

        /* No PSM to read in DVD mode, we already have all information */
        p_input->stream.p_selected_program->b_is_ok = 1;

        p_es = NULL;

        /* ES 0 -> video MPEG2 */
//        IfoPrintVideo( p_dvd );

        p_es = input_AddES( p_input, NULL, 0xe0, 0 );
        p_es->i_stream_id = 0xe0;
        p_es->i_fourcc = VLC_FOURCC('m','p','g','v');
        p_es->i_cat = VIDEO_ES;

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

            msg_Dbg( p_input, "audio position  %d", i_position );
                switch( p_vts->vtsi_mat->vts_audio_attr[i-1].audio_format )
                {
                case 0x00:              /* A52 */
                    i_id = ( ( 0x80 + i_position ) << 8 ) | 0xbd;
                    p_es = input_AddES( p_input, NULL, i_id, 0 );
                    p_es->i_stream_id = 0xbd;
                    p_es->i_fourcc = VLC_FOURCC('a','5','2',' ');
                    p_es->i_cat = AUDIO_ES;
                    strcpy( p_es->psz_desc, DecodeLanguage(
                        p_vts->vtsi_mat->vts_audio_attr[i-1].lang_code ) ); 
                    strcat( p_es->psz_desc, " (A52)" );

                    break;
                case 0x02:
                case 0x03:              /* MPEG audio */
                    i_id = 0xc0 + i_position;
                    p_es = input_AddES( p_input, NULL, i_id, 0 );
                    p_es->i_stream_id = i_id;
                    p_es->i_fourcc = VLC_FOURCC('m','p','g','a');
                    p_es->i_cat = AUDIO_ES;
                    strcpy( p_es->psz_desc, DecodeLanguage(
                        p_vts->vtsi_mat->vts_audio_attr[i-1].lang_code ) ); 
                    strcat( p_es->psz_desc, " (mpeg)" );

                    break;
                case 0x04:              /* LPCM */

                    i_id = ( ( 0xa0 + i_position ) << 8 ) | 0xbd;
                    p_es = input_AddES( p_input, NULL, i_id, 0 );
                    p_es->i_stream_id = i_id;
                    p_es->i_fourcc = VLC_FOURCC('l','p','c','m');
                    p_es->i_cat = AUDIO_ES;
                    strcpy( p_es->psz_desc, DecodeLanguage(
                        p_vts->vtsi_mat->vts_audio_attr[i-1].lang_code ) ); 
                    strcat( p_es->psz_desc, " (lpcm)" );

                    break;
                case 0x06:              /* DTS */
                    i_id = ( ( 0x88 + i_position ) << 8 ) | 0xbd;
                    msg_Err( p_input, "DTS audio not handled yet"
                                      "(0x%x)", i_id );
                    break;
                default:
                    i_id = 0;
                    msg_Err( p_input, "unknown audio type %.2x",
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
            msg_Dbg( p_input, "spu %d 0x%02x", i, spu_control );

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
                p_es = input_AddES( p_input, NULL, i_id, 0 );
                p_es->i_stream_id = 0xbd;
                p_es->i_fourcc = VLC_FOURCC('s','p','u',' ');
                p_es->i_cat = SPU_ES;
                strcpy( p_es->psz_desc, DecodeLanguage(
                    p_vts->vtsi_mat->vts_subp_attr[i-1].lang_code ) ); 
            }
        }
#undef spu_control

        /* FIXME: hack to check that the demuxer is ready, and set
         * the decoders */
        if( p_input->p_demux )
        {
            DvdReadLauchDecoders( p_input );
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
                        byte_t * p_buffer, size_t i_count )
{
    thread_dvd_data_t *     p_dvd;
    byte_t *                p_buf;
    int                     i_blocks_once;
    int                     i_blocks;
    int                     i_read;
    int                     i_read_total;
    vlc_bool_t              b_eot = 0;

    p_dvd = (thread_dvd_data_t *)p_input->p_access_data;
    p_buf = p_buffer;

    /*
     * Playback by cell in this pgc, starting at the cell for our chapter.
     */
    i_blocks = OFF2LB( i_count );
    i_read_total = 0;
    i_read = 0;

    while( i_blocks )
    {
        /* 
         * End of pack, we select the following one
         */
        if( ! p_dvd->i_pack_len )
        {
            /*
             * Read NAV packet.
             */
            if( ( i_read = DVDReadBlocks( p_dvd->p_title, p_dvd->i_next_vobu,
                           1, p_buf ) ) != 1 )
            {
                msg_Err( p_input, "read failed for block %d",
                                  p_dvd->i_next_vobu );
                return -1;
            }

            /* basic check to be sure we don't have a empty title
             * go to next title if so */
            //assert( p_buffer[41] == 0xbf && p_buffer[1027] == 0xbf );
            
            /*
             * Parse the contained dsi packet.
             */

            DvdReadHandleDSI( p_dvd, p_buf );

            /* End of File */
            if( p_dvd->i_next_vobu >= p_dvd->i_end_block + 1 )
            {
                return 1;
            }

            assert( p_dvd->i_pack_len < 1024 );
            /* FIXME: Ugly kludge: we send the pack block to the input for it
             * sometimes has a zero scr and restart the sync */
            p_dvd->i_cur_block ++;
            //p_dvd->i_pack_len++;

            i_read_total++;
            p_buf += DVD_VIDEO_LB_LEN;
            i_blocks--;
        }

        /*
         * Compute the number of blocks to read
         */
        i_blocks_once = p_dvd->i_pack_len >= i_blocks
                 ? i_blocks : p_dvd->i_pack_len;
        p_dvd->i_pack_len -= i_blocks_once;

        /* Reads from DVD */
        i_read = DVDReadBlocks( p_dvd->p_title, p_dvd->i_cur_block,
                                i_blocks_once, p_buf );
        if( i_read != i_blocks_once )
        {
            msg_Err( p_input, "read failed for %d/%d blocks at 0x%02x",
                              i_read, i_blocks_once, p_dvd->i_cur_block );
            return -1;
        }

        i_blocks -= i_read;
        i_read_total += i_read;
        p_dvd->i_cur_block += i_read;
        p_buf += LB2OFF( i_read );

    }
/*
    msg_Dbg( p_input, "i_blocks: %d len: %d current: 0x%02x", i_read, p_dvd->i_pack_len, p_dvd->i_cur_block );
*/

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
    
    if( p_input->stream.p_selected_area->i_tell
            >= p_input->stream.p_selected_area->i_size || b_eot )
    {
        if( ( p_input->stream.p_selected_area->i_id + 1 ) >= 
                        p_input->stream.i_area_nb )
        {
            /* EOF */
            vlc_mutex_unlock( &p_input->stream.stream_lock );
            return 1;
        }

        /* EOT */
        msg_Dbg( p_input, "new title" );
        DvdReadSetArea( p_input, p_input->stream.pp_areas[
                        p_input->stream.p_selected_area->i_id+1] );
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        return 0;
    }

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return LB2OFF( i_read_total );
}
#undef p_pgc

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

    vlc_mutex_lock( &p_input->stream.stream_lock );
    i_off += p_input->stream.p_selected_area->i_start;
    vlc_mutex_unlock( &p_input->stream.stream_lock );
    
    i_lb = OFF2LB( i_off );
    p_dvd = ( thread_dvd_data_t * )p_input->p_access_data;

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
                    p_dvd->i_ttn-1].ptt[i_chapter-1].pgcn;
        pgn = p_dvd->p_vts_file->vts_ptt_srpt->title[
                    p_dvd->i_ttn-1].ptt[i_chapter-1].pgn;

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
    msg_Dbg( p_input, "cell %d i_sub_cell %d chapter %d vobu %d cell_sector %d vobu_sector %d sub_cell_sector %d",
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

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.p_selected_area->i_tell =
        LB2OFF ( p_dvd->i_cur_block )
         - p_input->stream.p_selected_area->i_start;
    p_input->stream.p_selected_area->i_part = p_dvd->i_chapter;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

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
    msg_Dbg( p_input, 12, "scr %d lbn 0x%02x vobu_ea %d vob_id %d c_id %d",
             p_dvd->dsi_pack.dsi_gi.nv_pck_scr,
             p_dvd->dsi_pack.dsi_gi.nv_pck_lbn,
             p_dvd->dsi_pack.dsi_gi.vobu_ea,
             p_dvd->dsi_pack.dsi_gi.vobu_vob_idn,
             p_dvd->dsi_pack.dsi_gi.vobu_c_idn );

    msg_Dbg( p_input, 12, "cat 0x%02x ilvu_ea %d ilvu_sa %d size %d", 
             p_dvd->dsi_pack.sml_pbi.category,
             p_dvd->dsi_pack.sml_pbi.ilvu_ea,
             p_dvd->dsi_pack.sml_pbi.ilvu_sa,
             p_dvd->dsi_pack.sml_pbi.size );

    msg_Dbg( p_input, 12, "next_vobu %d next_ilvu1 %d next_ilvu2 %d",
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
                p_dvd->i_ttn-1].ptt[p_dvd->i_chapter-1].pgcn;
    pgn = p_dvd->p_vts_file->vts_ptt_srpt->title[
                p_dvd->i_ttn-1].ptt[p_dvd->i_chapter-1].pgn;
    p_pgc = p_dvd->p_vts_file->vts_pgcit->pgci_srp[pgc_id-1].pgc;

    if( p_pgc->program_map[pgn-1] <= p_dvd->i_cur_cell )
    {
        p_dvd->i_chapter++;
        p_dvd->b_eoc = 1;
    }
}

/*****************************************************************************
 * DvdReadLaunchDecoders
 *****************************************************************************/
static void DvdReadLauchDecoders( input_thread_t * p_input )
{
    thread_dvd_data_t *  p_dvd;
    
    p_dvd = (thread_dvd_data_t*)(p_input->p_access_data);            
            
    if( config_GetInt( p_input, "video" ) )
    {
        input_SelectES( p_input, p_input->stream.pp_es[0] );
    }

    if( config_GetInt( p_input, "audio" ) )
    {
        /* For audio: first one if none or a not existing one specified */
        int i_audio = config_GetInt( p_input, "audio-channel" );
        if( i_audio < 0 /*|| i_audio > i_audio_nb*/ )
        {
            config_PutInt( p_input, "audio-channel", 1 );
            i_audio = 1;
        }
        if( i_audio > 0/* && i_audio_nb > 0*/ )
        {
            if( config_GetInt( p_input, "audio-type" )
                 == REQUESTED_A52 )
            {
                int     i_a52 = i_audio;
                while( ( p_input->stream.pp_es[i_a52]->i_fourcc !=
                       VLC_FOURCC('a','5','2',' ') ) && ( i_a52 <=
                       p_dvd->p_vts_file->vtsi_mat->nr_of_vts_audio_streams ) )
                {
                    i_a52++;
                }
                if( p_input->stream.pp_es[i_a52]->i_fourcc
                     == VLC_FOURCC('a','5','2',' ') )
                {
                    input_SelectES( p_input,
                                    p_input->stream.pp_es[i_a52] );
                }
            }
            else
            {
                input_SelectES( p_input,
                                p_input->stream.pp_es[i_audio] );
            }
        }
    }

    if( config_GetInt( p_input, "video" ) )
    {
        /* for spu, default is none */
        int i_spu = config_GetInt( p_input, "spu-channel" );
        if( i_spu < 0 /*|| i_spu > i_spu_nb*/ )
        {
            config_PutInt( p_input, "spu-channel", 0 );
            i_spu = 0;
        }
        if( i_spu > 0 /*&& i_spu_nb > 0*/ )
        {
            i_spu += p_dvd->p_vts_file->vtsi_mat->nr_of_vts_audio_streams;
            input_SelectES( p_input, p_input->stream.pp_es[i_spu] );
        }
    }
}
