/* dvd_access.c: DVD access plugin.
 *****************************************************************************
 * This plugins should handle all the known specificities of the DVD format,
 * especially the 2048 bytes logical block size.
 * It depends on:
 *  -libdvdcss for access and unscrambling
 *  -dvd_ifo for ifo parsing and analyse
 *  -dvd_udf to find files
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: dvd_access.c,v 1.21 2002/07/17 21:28:19 stef Exp $
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
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#ifdef STRNCASECMP_IN_STRINGS_H
#   include <strings.h>
#endif

#ifdef GOD_DAMN_DMCA
#   include "dummy_dvdcss.h"
#else
#   include <dvdcss/dvdcss.h>
#endif

#include "dvd.h"
#include "dvd_es.h"
#include "dvd_seek.h"
#include "dvd_ifo.h"
#include "dvd_summary.h"
#include "iso_lang.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/* called from outside */
static int  DVDOpen         ( input_thread_t * );
static void DVDClose        ( input_thread_t * );
static int  DVDSetArea      ( input_thread_t *, input_area_t * );
static int  DVDSetProgram   ( input_thread_t *, pgrm_descriptor_t * );
static ssize_t DVDRead      ( input_thread_t *, byte_t *, size_t );
static void DVDSeek         ( input_thread_t *, off_t );

static char * DVDParse( input_thread_t * );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( access_getfunctions)( function_list_t * p_function_list )
{
#define input p_function_list->functions.access
    input.pf_open             = DVDOpen;
    input.pf_close            = DVDClose;
    input.pf_read             = DVDRead;
    input.pf_set_area         = DVDSetArea;
    input.pf_set_program      = DVDSetProgram;
    input.pf_seek             = DVDSeek;
#undef input
}

/*
 * Data access functions
 */

#define DVDTell   LB2OFF( p_dvd->i_vts_start + p_dvd->i_vts_lb ) \
                  - p_input->stream.p_selected_area->i_start

/*****************************************************************************
 * DVDOpen: open dvd
 *****************************************************************************/
static int DVDOpen( input_thread_t *p_input )
{
    char *               psz_device;
    thread_dvd_data_t *  p_dvd;
    input_area_t *       p_area;
    int                  i;

    p_dvd = malloc( sizeof(thread_dvd_data_t) );
    if( p_dvd == NULL )
    {
        msg_Err( p_input, "out of memory" );
        return -1;
    }
    p_input->p_access_data = (void *)p_dvd;
    
    /* Parse command line */
    if( !( psz_device = DVDParse( p_input ) ) )
    {
        free( p_dvd );
        return -1;
    }
    
    /* 
     * set up input
     */ 
    p_input->i_mtu = 0;

    /*
     *  get plugin ready
     */ 
    p_dvd->dvdhandle = dvdcss_open( psz_device );
    
    /* free allocated string */
    free( psz_device );

    if( p_dvd->dvdhandle == NULL )
    {
        msg_Err( p_input, "dvdcss cannot open device" );
        free( p_dvd );
        return -1;
    }

    if( dvdcss_seek( p_dvd->dvdhandle, 0, DVDCSS_NOFLAGS ) < 0 )
    {
        msg_Err( p_input, "%s", dvdcss_error( p_dvd->dvdhandle ) );
        dvdcss_close( p_dvd->dvdhandle );
        free( p_dvd );
        return -1;
    }

    /* Ifo allocation & initialisation */
    if( IfoCreate( p_dvd ) < 0 )
    {
        msg_Err( p_input, "allcation error in ifo" );
        dvdcss_close( p_dvd->dvdhandle );
        free( p_dvd );
        return -1;
    }

    if( IfoInit( p_dvd->p_ifo ) < 0 )
    {
        msg_Err( p_input, "fatal failure in ifo" );
        IfoDestroy( p_dvd->p_ifo );
        dvdcss_close( p_dvd->dvdhandle );
        free( p_dvd );
        return -1;
    }

    /* Set stream and area data */
    vlc_mutex_lock( &p_input->stream.stream_lock );

    p_input->stream.i_method = INPUT_METHOD_DVD;
    p_input->stream.b_pace_control = 1;
    p_input->stream.b_seekable = 1;
    p_input->stream.p_selected_area->i_size = 0;
    p_input->stream.p_selected_area->i_tell = 0;

    /* Initialize ES structures */
    input_InitStream( p_input, sizeof( stream_ps_data_t ) );

#define title_inf p_dvd->p_ifo->vmg.title_inf
    msg_Dbg( p_input, "number of titles: %d", title_inf.i_title_nb );

#define area p_input->stream.pp_areas
    /* We start from 1 here since the default area 0
     * is reserved for video_ts.vob */
    for( i = 1 ; i <= title_inf.i_title_nb ; i++ )
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
        area[i]->i_part_nb = title_inf.p_attr[i-1].i_chapter_nb;
        area[i]->i_part = 1;

        /* Offset to vts_i_0.ifo */
        area[i]->i_plugin_data = p_dvd->p_ifo->i_start +
                       title_inf.p_attr[i-1].i_start_sector;
    }   
#undef area
    
    p_dvd->i_title = p_dvd->i_title <= title_inf.i_title_nb ?
                     p_dvd->i_title : 1;
#undef title_inf

    p_area = p_input->stream.pp_areas[p_dvd->i_title];
    
    p_area->i_part = p_dvd->i_chapter <= p_area->i_part_nb ?
                     p_dvd->i_chapter : 1;
    p_dvd->i_chapter = 1;
    
    p_dvd->b_new_chapter = 0;
    p_dvd->i_audio_nb = 0;
    p_dvd->i_spu_nb = 0;
    
    /* set title, chapter, audio and subpic */
    if( DVDSetArea( p_input, p_area ) < 0 )
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        IfoDestroy( p_dvd->p_ifo );
        dvdcss_close( p_dvd->dvdhandle );
        free( p_dvd );
        return -1;
    }

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    p_input->psz_demux = "dvdold";

    return 0;
}

/*****************************************************************************
 * DVDClose: close dvd
 *****************************************************************************/
static void DVDClose( input_thread_t *p_input )
{
    thread_dvd_data_t *p_dvd = (thread_dvd_data_t*)p_input->p_access_data;

    IfoDestroy( p_dvd->p_ifo );
    dvdcss_close( p_dvd->dvdhandle );
    free( p_dvd );
}

/*****************************************************************************
 * DVDSetProgram: used to change angle
 *****************************************************************************/
static int DVDSetProgram( input_thread_t    * p_input,
                          pgrm_descriptor_t * p_program ) 
{
    if( p_input->stream.p_selected_program != p_program )
    {
        thread_dvd_data_t *  p_dvd;
        int                  i_angle;
    
        p_dvd   = (thread_dvd_data_t*)(p_input->p_access_data);
        i_angle = p_program->i_number;

        /* DVD is actually mono-program: we only need the current angle
         * number, so copy the data between programs */
        memcpy( p_program,
                p_input->stream.p_selected_program,
                sizeof(pgrm_descriptor_t) );
        p_program->i_number                = i_angle;
        p_input->stream.p_selected_program = p_program;

#define title \
    p_dvd->p_ifo->vts.title_unit.p_title[p_dvd->i_title_id-1].title
        if( title.p_cell_play[p_dvd->i_prg_cell].i_category & 0xf000 )
        {
            if( ( p_program->i_number - p_dvd->i_angle ) < 0 )
            {
                /* we have to go backwards */
                p_dvd->i_map_cell = 0;
            }
            p_dvd->i_prg_cell += ( p_program->i_number - p_dvd->i_angle );
            p_dvd->i_map_cell =  CellPrg2Map( p_dvd );
            p_dvd->i_map_cell += p_dvd->i_angle_cell;
            p_dvd->i_vts_lb   =  CellFirstSector( p_dvd );
            p_dvd->i_last_lb  =  CellLastSector( p_dvd );
            p_dvd->i_angle    =  p_program->i_number;
        }
        else
        {
            p_dvd->i_angle    =  p_program->i_number;
        }
#undef title
        msg_Dbg( p_input, "angle %d selected", p_dvd->i_angle );
    }

    return 0;
}

/*****************************************************************************
 * DVDSetArea: initialize input data for title x, chapter y.
 * It should be called for each user navigation request.
 *****************************************************************************
 * Take care that i_title starts from 0 (vmg) and i_chapter start from 1.
 * Note that you have to take the lock before entering here.
 *****************************************************************************/
#define vmg p_dvd->p_ifo->vmg
#define vts p_dvd->p_ifo->vts

static void DVDFlushStream( input_thread_t * p_input )
{
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

    return;
}

static int DVDReadAngle( input_thread_t * p_input )
{
    thread_dvd_data_t * p_dvd;
    int                 i_angle_nb;
    int                 i;

    p_dvd      = (thread_dvd_data_t*)(p_input->p_access_data);
    i_angle_nb = vmg.title_inf.p_attr[p_dvd->i_title-1].i_angle_nb;
    
    input_AddProgram( p_input, 1, sizeof( stream_ps_data_t ) );
    p_input->stream.p_selected_program = p_input->stream.pp_programs[0];

    for( i = 1 ; i < i_angle_nb ; i++ )
    {
        input_AddProgram( p_input, i+1, 0 );
    }

    return i_angle_nb;
}

static int DVDSetArea( input_thread_t * p_input, input_area_t * p_area )
{
    thread_dvd_data_t *  p_dvd;

    p_dvd = (thread_dvd_data_t*)(p_input->p_access_data);

    /* we can't use the interface slider until initilization is complete */
    p_input->stream.b_seekable = 0;

    if( p_area != p_input->stream.p_selected_area )
    {
        int     i_vts_title;
        u32     i_first;
        u32     i_last;

        /* Reset the Chapter position of the old title */
        p_input->stream.p_selected_area->i_part = 1;
        p_input->stream.p_selected_area         = p_area;

        /*
         *  We have to load all title information
         */

        /* title number as it appears in the interface list */
        p_dvd->i_title      = p_area->i_id;
        p_dvd->i_chapter_nb = p_area->i_part_nb;

        if( IfoTitleSet( p_dvd->p_ifo, p_dvd->i_title ) < 0 )
        {
            msg_Err( p_input, "fatal error in vts ifo" );
            free( p_dvd );
            return -1;
        }

        /* title position inside the selected vts */
        i_vts_title       = vmg.title_inf.p_attr[p_dvd->i_title-1].i_title_num;
        p_dvd->i_title_id =
            vts.title_inf.p_title_start[i_vts_title-1].i_title_id;

        msg_Dbg( p_input, "title %d vts_title %d pgc %d",
                          p_dvd->i_title, i_vts_title, p_dvd->i_title_id );

        /* title set offset XXX: convert to block values */
        p_dvd->i_vts_start =
            vts.i_pos + vts.manager_inf.i_title_vob_start_sector;

        /* last cell */
        p_dvd->i_prg_cell = -1 +
            vts.title_unit.p_title[p_dvd->i_title_id-1].title.i_cell_nb;
        p_dvd->i_map_cell = 0;
        p_dvd->i_map_cell = CellPrg2Map( p_dvd );
        i_last            = CellLastSector( p_dvd );

        /* first cell */
        p_dvd->i_prg_cell   = 0;
        p_dvd->i_map_cell   = 0;
        p_dvd->i_angle_cell = 0;
        p_dvd->i_map_cell   = CellPrg2Map    ( p_dvd );
        p_dvd->i_vts_lb     = CellFirstSector( p_dvd );
        p_dvd->i_last_lb    = CellLastSector ( p_dvd );

        /* Force libdvdcss to check its title key.
         * It is only useful for title cracking method. Methods using the
         * decrypted disc key are fast enough to check the key at each seek */
        i_first = dvdcss_seek( p_dvd->dvdhandle,
                               p_dvd->i_vts_start + p_dvd->i_vts_lb,
                               DVDCSS_SEEK_KEY );
        if( i_first < 0 )
        {
            msg_Err( p_input, "%s", dvdcss_error( p_dvd->dvdhandle ) );
            return -1;
        }

        /* Area definition */
        p_input->stream.p_selected_area->i_start = LB2OFF( i_first );
        p_input->stream.p_selected_area->i_size  =
                                        LB2OFF( i_last + 1 - p_dvd->i_vts_lb );

        /* Destroy obsolete ES by reinitializing programs */
        DVDFlushStream( p_input );
        
        /* Angle management: angles are handled through programs */
        p_dvd->i_angle_nb = DVDReadAngle( p_input );
        if( ( p_dvd->i_angle <= 0 ) || p_dvd->i_angle > p_dvd->i_angle_nb )
        {
            p_dvd->i_angle = 1;
        }
       
        DVDSetProgram( p_input,
                       p_input->stream.pp_programs[p_dvd->i_angle-1] ); 

        msg_Dbg( p_input, "title first %i, last %i, size %i",
                          i_first, i_last, i_last + 1 - p_dvd->i_vts_lb );
        IfoPrintTitle( p_dvd );

        /* No PSM to read in DVD mode, we already have all information */
        p_input->stream.p_selected_program->b_is_ok = 1;

        /* Find all ES in title with ifo data */
        DVDReadVideo( p_input );
        DVDReadAudio( p_input );
        DVDReadSPU  ( p_input );
   
        if( p_input->p_demux_module )
        {
            DVDLaunchDecoders( p_input );
        }

    } /* i_title >= 0 */
    else
    {
        p_area = p_input->stream.p_selected_area;
    }

    /* Chapter selection */
    p_dvd->i_chapter = DVDSetChapter( p_dvd, p_area->i_part );
    
    p_input->stream.p_selected_area->i_tell = DVDTell;

    /* warn interface that something has changed */
    p_input->stream.b_seekable = 1;
    p_input->stream.b_changed  = 1;

    return 0;
}
#undef vts
#undef vmg

#define title \
    p_dvd->p_ifo->vts.title_unit.p_title[p_dvd->i_title_id-1].title
    
/*****************************************************************************
 * DVDRead: reads data packets.
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, otherwise the number of
 * bytes.
 *****************************************************************************/
static ssize_t DVDRead( input_thread_t * p_input,
                        byte_t * p_buffer, size_t i_count )
{
    thread_dvd_data_t *     p_dvd;
    int                     i_read;
    int                     i_blocks;
    int                     i_block_once = 0;

    p_dvd = (thread_dvd_data_t *)(p_input->p_access_data);

    i_read = 0;
    i_blocks = OFF2LB(i_count);

    while( i_blocks )
    {
        i_block_once = LbMaxOnce( p_dvd );
        if( i_block_once > i_blocks )
        {
            i_block_once = i_blocks;
        }
        else if( i_block_once <= 0 )
        {
            /* EOT */
            break;
        }

        if( i_block_once != dvdcss_read( p_dvd->dvdhandle, p_buffer,
                                         i_block_once, DVDCSS_READ_DECRYPT ) )
        {
            return -1;
        }

        i_blocks -= i_block_once;
        i_read += i_block_once;
        p_buffer += LB2OFF( i_block_once );

        /* Update global position */
        p_dvd->i_vts_lb += i_block_once;
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );

    p_input->stream.p_selected_area->i_tell += LB2OFF( i_read );
    if( p_dvd->b_new_chapter )
    {
        p_input->stream.p_selected_area->i_part = p_dvd->i_chapter;
        p_dvd->b_new_chapter                    = 0;
    }

    if( ( p_input->stream.p_selected_area->i_tell
            >= p_input->stream.p_selected_area->i_size )
       || ( i_block_once  <= 0 ) )
    {
        if( ( p_dvd->i_title + 1 ) >= p_input->stream.i_area_nb )
        {
            /* EOF */
            vlc_mutex_unlock( &p_input->stream.stream_lock );
            return 0;
        }

        /* EOT */
        msg_Dbg( p_input, "new title" );
        p_dvd->i_title++;
        DVDSetArea( p_input, p_input->stream.pp_areas[p_dvd->i_title] );
    }

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return LB2OFF( i_read );
}

/*****************************************************************************
 * DVDSeek : Goes to a given position on the stream.
 *****************************************************************************
 * This one is used by the input and translate chronological position from
 * input to logical position on the device.
 * The lock should be taken before calling this function.
 *****************************************************************************/
static void DVDSeek( input_thread_t * p_input, off_t i_off )
{
    thread_dvd_data_t *     p_dvd;
    
    p_dvd = ( thread_dvd_data_t * )(p_input->p_access_data);

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_dvd->i_vts_lb = OFF2LB(i_off + p_input->stream.p_selected_area->i_start)
                       - p_dvd->i_vts_start;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    p_dvd->i_prg_cell = Lb2CellPrg( p_dvd );
    p_dvd->i_map_cell = Lb2CellMap( p_dvd );
    
    if( CellIsInterleaved( p_dvd ) )
    {
        /* if we're inside a multi-angle zone, we have to choose i_sector
         * in the current angle ; we can't do it all the time since cells
         * can be very wide out of such zones */
        p_dvd->i_vts_lb = CellFirstSector( p_dvd );
    }
    
    p_dvd->i_last_lb  = CellLastSector( p_dvd );
    p_dvd->i_chapter  = CellPrg2Chapter( p_dvd );

    if( dvdcss_seek( p_dvd->dvdhandle, p_dvd->i_vts_start + p_dvd->i_vts_lb,
                     DVDCSS_SEEK_MPEG ) < 0 )
    {
        msg_Err( p_input, "%s", dvdcss_error( p_dvd->dvdhandle ) );
        p_input->b_error = 1;
        return;
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.p_selected_area->i_part = p_dvd->i_chapter;
    p_input->stream.p_selected_area->i_tell = DVDTell;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    msg_Dbg( p_input, "program cell: %d cell: %d chapter: %d tell %lld",
             p_dvd->i_prg_cell, p_dvd->i_map_cell, p_dvd->i_chapter, DVDTell );

    return;
}

/*****************************************************************************
 * DVDParse: parse command line
 *****************************************************************************/
static char * DVDParse( input_thread_t * p_input )
{
    thread_dvd_data_t *  p_dvd;
    struct stat          stat_info;
    char *               psz_parser;
    char *               psz_device;
    char *               psz_raw;
    char *               psz_next;
    vlc_bool_t           b_options = 0;
    int                  i_title = 1;
    int                  i_chapter = 1;
    int                  i_angle = 1;
    int                  i;

    p_dvd = (thread_dvd_data_t*)(p_input->p_access_data);

#ifdef WIN32
    /* On Win32 we want the DVD access plugin to be explicitly requested,
     * we end up with lots of problems otherwise */
    if( !p_input->psz_access || !*p_input->psz_access ) return NULL;
#endif

    psz_parser = psz_device = strdup( p_input->psz_name );
    if( !psz_parser )
    {
        return NULL;
    }

    /* Parse input string :
     * [device][@rawdevice][@[title][,[chapter][,angle]]] */
    while( *psz_parser && *psz_parser != '@' )
    {
        psz_parser++;
    }

    if( *psz_parser == '@' )
    {
        /* Maybe found raw device or option list */
        *psz_parser = '\0';
        psz_raw = ++psz_parser;
    }
    else
    {
        psz_raw = "";
    }

    if( *psz_parser && !strtol( psz_parser, NULL, 10 ) )
    {
        /* what we've found is either a raw device or a partial option
         * list e.g. @,29 or both a device and a list ; search end of string */
        while( *psz_parser && *psz_parser != '@' )
        {
            psz_parser++;
        }
        
        if( *psz_parser == '@' )
        {
            /* found end of raw device, and beginning of options */
            *psz_parser = '\0';
            ++psz_parser;
            b_options = 1;
        }
        else
        {
            psz_parser = psz_raw + 1;
            for( i=0 ; i<3 ; i++ )
            {
                if( !*psz_parser )
                {
                    /* we have only a raw device */
                    break;
                }
                if( strtol( psz_parser, NULL, 10 ) )
                {
                    /* we have only a partial list of options, no device */
                    psz_parser = psz_raw;
                    psz_raw = "";
                    b_options = 1;
                    break;
                }
                psz_parser++;
            }
        }
    }
    else
    {
        /* found beginning of options ; no raw device specified */
        psz_raw = "";
        b_options = 1;
    }

    if( b_options )
    {
        /* Found options */
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

        p_dvd->i_title = i_title ? i_title : 1;
        p_dvd->i_chapter = i_chapter ? i_chapter : 1;
        p_dvd->i_angle = i_angle ? i_angle : 1;
    }

    if( *psz_raw )
    {
        if( *psz_raw )
        {
            /* check the raw device */
            if( stat( psz_raw, &stat_info ) == -1 )
            {
                msg_Warn( p_input, "cannot stat() raw device `%s' (%s)",
                                   psz_raw, strerror(errno));
                /* put back '@' */
                *(psz_raw - 1) = '@';
                psz_raw = "";
            }
            else
            {
                char * psz_env;
                
#ifndef WIN32    
                if( !S_ISCHR(stat_info.st_mode) )
                {
                    msg_Warn( p_input, "raw device %s is"
                              " not a valid char device", psz_raw );
                    /* put back '@' */
                    *(psz_raw - 1) = '@';
                    psz_raw = "";
                }
                else
#endif
                {
                    psz_env = malloc( strlen("DVDCSS_RAW_DEVICE=")
                                    + strlen( psz_raw ) + 1 );
                    sprintf( psz_env, "DVDCSS_RAW_DEVICE=%s", psz_raw );
                    putenv( psz_env );
                }
            }
        }
        else
        {
            psz_raw = "";
        }
    }
    
    if( !*psz_device )
    {
        free( psz_device );
        
        if( !p_input->psz_access )
        {
            /* no device and no access specified: we probably don't want DVD */
            return NULL;
        }
        psz_device = config_GetPsz( p_input, "dvd" );
    }

#ifndef WIN32    
    /* check block device */
    if( stat( psz_device, &stat_info ) == -1 )
    {
        msg_Err( p_input, "cannot stat() device `%s' (%s)",
                          psz_device, strerror(errno));
        free( psz_device );
        return NULL;                    
    }
    
    if( !S_ISBLK(stat_info.st_mode) && !S_ISCHR(stat_info.st_mode) )
    {
        msg_Warn( p_input,
                  "dvd module discarded (not a valid block device)" );
        free( psz_device );
        return NULL;
    }
#endif
    
    msg_Dbg( p_input, "dvd=%s raw=%s title=%d chapter=%d angle=%d",
                      psz_device, psz_raw, p_dvd->i_title,
                      p_dvd->i_chapter, p_dvd->i_angle );

    return psz_device;
} 
