/* input_dvd.c: DVD raw reading plugin.
 *****************************************************************************
 * This plugins should handle all the known specificities of the DVD format,
 * especially the 2048 bytes logical block size.
 * It depends on:
 *  -libdvdcss for access and unscrambling
 *  -dvd_ifo for ifo parsing and analyse
 *  -dvd_udf to find files
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: input_dvd.c,v 1.128 2002/03/04 02:26:23 stef Exp $
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

#include <videolan/vlc.h>

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

#if defined( WIN32 )
#   include <io.h>                                                 /* read() */
#endif

#ifdef GOD_DAMN_DMCA
#   include "dummy_dvdcss.h"
#else
#   include <videolan/dvdcss.h>
#endif

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"
#include "input_ext-plugins.h"

#include "input_dvd.h"
#include "dvd_ifo.h"
#include "dvd_summary.h"
#include "iso_lang.h"

#include "debug.h"

/* how many packets DVDDemux will read in each loop */
#define DVD_READ_ONCE 64

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/* called from outside */
static int  DVDOpen         ( struct input_thread_s * );
static void DVDClose        ( struct input_thread_s * );
static int  DVDSetArea      ( struct input_thread_s *, struct input_area_s * );
static int  DVDSetProgram   ( struct input_thread_s *, pgrm_descriptor_t * );
static int  DVDRead         ( struct input_thread_s *, byte_t *, size_t );
static void DVDSeek         ( struct input_thread_s *, off_t );

static int  DVDRewind       ( struct input_thread_s * );
static int  DVDDemux        ( struct input_thread_s * );
static int  DVDInit         ( struct input_thread_s * );
static void DVDEnd          ( struct input_thread_s * );

/* called only inside */
static void DVDLaunchDecoders( input_thread_t * p_input );
static int  DVDChooseAngle( thread_dvd_data_t * );
static int  DVDFindCell( thread_dvd_data_t * );
static int  DVDFindSector( thread_dvd_data_t * );
static int  DVDChapterSelect( thread_dvd_data_t *, int );

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

void _M( demux_getfunctions)( function_list_t * p_function_list )
{
#define demux p_function_list->functions.demux
    demux.pf_init             = DVDInit;
    demux.pf_end              = DVDEnd;
    demux.pf_demux            = DVDDemux;
    demux.pf_rewind           = DVDRewind;
#undef demux
}

/*
 * Data demux functions
 */

/*****************************************************************************
 * DVDInit: initializes DVD structures
 *****************************************************************************/
static int DVDInit( input_thread_t * p_input )
{

    if( strncmp( p_input->p_access_module->psz_name, "dvd", 3 ) )
    {
        return -1;
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );
    
    DVDLaunchDecoders( p_input );
    
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return 0;
}

/*****************************************************************************
 * DVDEnd: frees unused data
 *****************************************************************************/
static void DVDEnd( input_thread_t * p_input )
{
}

/*****************************************************************************
 * DVDDemux
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

static int DVDDemux( input_thread_t * p_input )
{
    int                 i;
    byte_t *            p_peek;
    data_packet_t *     p_data;
    ssize_t             i_result;
    int                 i_packet_size;
            

    /* Read headers to compute payload length */
    for( i = 0 ; i < DVD_READ_ONCE ; i++ )
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

        input_DemuxPS( p_input, p_data );
                        
    }
    
    return i;
}

/*****************************************************************************
 * DVDRewind : reads a stream backward
 *****************************************************************************/
static int DVDRewind( input_thread_t * p_input )
{
    return( -1 );
}



/*
 * Data access functions
 */

#define PARSE( chr, action )                            \
    psz_parser = p_input->psz_name;                     \
    while( *(psz_parser) && *(psz_parser) != (chr) )    \
    {                                                   \
        (psz_parser)++;                                 \
    }                                                   \
                                                        \
    if( *(psz_parser) == (chr) )                        \
    {                                                   \
        *(psz_parser) = '\0';                           \
        (action);                                       \
    }

/*****************************************************************************
 * DVDOpen: open dvd
 *****************************************************************************/
static int DVDOpen( struct input_thread_s *p_input )
{
    struct stat          stat_info;
    char *               psz_parser = p_input->psz_name;
    char *               psz_device = p_input->psz_name;
    char *               psz_raw;
    char *               psz_next;
    dvdcss_handle        dvdhandle;
    thread_dvd_data_t *  p_dvd;
    input_area_t *       p_area;
    boolean_t            b_need_free = 0;
    boolean_t            b_options = 0;
    int                  i_title = 1;
    int                  i_chapter = 1;
    int                  i_angle = 1;
    int                  i;

    /* Parse input string :
     * [device][@rawdevice][@[title][,[chapter][,angle]]] */
    while( *psz_parser && *psz_parser != '@' )
    {
        psz_parser++;
    }

    if( *psz_parser == '@' )
    {
        /* Found raw device */
        *psz_parser = '\0';
        psz_raw = ++psz_parser;
    }
    else
    {
        psz_raw = NULL;
    }

    if( !strtol( psz_parser, NULL, 10 ) )
    {
        while( *psz_parser && *psz_parser != '@' )
        {
            psz_parser++;
        }
        
        if( *psz_parser == '@' )
        {
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
                    break;
                }
                if( strtol( psz_parser, NULL, 10 ) )
                {
                    psz_parser = psz_raw;
                    psz_raw = NULL;
                    b_options = 1;
                    break;
                }
                psz_parser++;
            }
        }
    }
    else
    {
        psz_raw = NULL;
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

        i_title = i_title ? i_title : 1;
        i_chapter = i_chapter ? i_chapter : 1;
        i_angle = i_angle ? i_angle : 1;
    }

    if( !*psz_device )
    {
        psz_device = config_GetPszVariable( INPUT_DVD_DEVICE_VAR );
        b_need_free = 1;
    }

    if( stat( psz_device, &stat_info ) == -1 )
    {
        intf_ErrMsg( "input error: cannot stat() device `%s' (%s)",
                     psz_device, strerror(errno));
        return( -1 );                    
    }
    
#ifndef WIN32    
    if( !S_ISBLK(stat_info.st_mode) && !S_ISCHR(stat_info.st_mode) )
    {
        intf_WarnMsg( 3, "input: DVD plugin discarded"
                         " (not a valid block device)" );
        return -1;
    }
#endif
    
    if( psz_raw )
    {
        if( *psz_raw )
        {
            char * psz_env;
            psz_env = malloc( strlen("DVDCSS_RAW_DEVICE=")
                            + strlen( psz_raw ) + 1 );
            sprintf( psz_env, "DVDCSS_RAW_DEVICE=%s", psz_raw );
            fprintf(stderr, "%s\n", psz_env );
            putenv( psz_env );
        }
        else
        {
            psz_raw = NULL;
        }
    }

    intf_WarnMsg( 2, "input: dvd=%s raw=%s title=%d chapter=%d angle=%d",
                  psz_device, psz_raw, i_title, i_chapter, i_angle );
    
    /* 
     * set up input
     */ 
    p_input->i_mtu = 0;

    vlc_mutex_lock( &p_input->stream.stream_lock );

    p_input->stream.i_method = INPUT_METHOD_DVD;

    /* If we are here we can control the pace... */
    p_input->stream.b_pace_control = 1;

    p_input->stream.b_seekable = 1;
    p_input->stream.p_selected_area->i_size = 0;

    p_input->stream.p_selected_area->i_tell = 0;

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /*
     *  get plugin ready
     */ 
    dvdhandle = dvdcss_open( psz_device );
    
    if( b_need_free )
    {
        free( psz_device );
    }

    if( dvdhandle == NULL )
    {
        intf_ErrMsg( "dvd error: dvdcss can't open device" );
        return -1;
    }

    p_dvd = malloc( sizeof(thread_dvd_data_t) );
    if( p_dvd == NULL )
    {
        intf_ErrMsg( "dvd error: out of memory" );
        return -1;
    }

    p_dvd->dvdhandle = (dvdcss_handle) dvdhandle;
    p_input->p_access_data = (void *)p_dvd;

    p_dvd->i_title = i_title;
    p_dvd->i_chapter = i_chapter;
    p_dvd->i_angle = i_angle;

    if( dvdcss_seek( p_dvd->dvdhandle, 0, DVDCSS_NOFLAGS ) < 0 )
    {
        intf_ErrMsg( "dvd error: %s", dvdcss_error( p_dvd->dvdhandle ) );
        return -1;
    }

    /* Ifo allocation & initialisation */
    if( IfoCreate( p_dvd ) < 0 )
    {
        intf_ErrMsg( "dvd error: allcation error in ifo" );
        free( p_dvd );
        return -1;
    }

    if( IfoInit( p_dvd->p_ifo ) < 0 )
    {
        intf_ErrMsg( "dvd error: fatal failure in ifo" );
        IfoDestroy( p_dvd->p_ifo );
        free( p_dvd );
        return -1;
    }

    /* Set stream and area data */
    vlc_mutex_lock( &p_input->stream.stream_lock );

    /* Initialize ES structures */
    input_InitStream( p_input, sizeof( stream_ps_data_t ) );

#define title_inf p_dvd->p_ifo->vmg.title_inf
    intf_WarnMsg( 2, "dvd info: number of titles: %d", title_inf.i_title_nb );

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

        /* Number of angles */
        area[i]->i_angle_nb = 0;
        area[i]->i_angle = 1;

        /* Offset to vts_i_0.ifo */
        area[i]->i_plugin_data = p_dvd->p_ifo->i_start +
                       title_inf.p_attr[i-1].i_start_sector;
    }   
#undef area
#undef title_inf

    p_area = p_input->stream.pp_areas[i_title];
    p_area->i_part = i_chapter;

    /* set title, chapter, audio and subpic */
    if( DVDSetArea( p_input, p_area ) )
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        return -1;
    }

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return 0;

}

/*****************************************************************************
 * DVDClose: close dvd
 *****************************************************************************/
static void DVDClose( struct input_thread_s *p_input )
{
    thread_dvd_data_t *     p_dvd;

    p_dvd = (thread_dvd_data_t*)p_input->p_access_data;

    IfoDestroy( p_dvd->p_ifo );

    p_input->p_access_data = (void *)(p_dvd->dvdhandle);
    free( p_dvd );

    /* Clean up libdvdcss */
    dvdcss_close( (dvdcss_handle) p_input->p_access_data );
}

/*****************************************************************************
 * DVDSetProgram: Does nothing, a DVD is mono-program
 *****************************************************************************/
static int DVDSetProgram( input_thread_t * p_input, 
            pgrm_descriptor_t * p_program ) 
{
    return 0;
}

/*****************************************************************************
 * DVDSetArea: initialize input data for title x, chapter y.
 * It should be called for each user navigation request.
 *****************************************************************************
 * Take care that i_title starts from 0 (vmg) and i_chapter start from 1.
 * Note that you have to take the lock before entering here.
 *****************************************************************************/
static int DVDSetArea( input_thread_t * p_input, input_area_t * p_area )
{
    thread_dvd_data_t *  p_dvd;
    es_descriptor_t *    p_es;
    u16                  i_id;
    int                  i_vts_title;
    int                  i_audio_nb = 0;
    int                  i_spu_nb = 0;
    int                  i;

    p_dvd = (thread_dvd_data_t*)(p_input->p_access_data);

    /* we can't use the interface slider until initilization is complete */
    p_input->stream.b_seekable = 0;

    if( p_area != p_input->stream.p_selected_area )
    {
        /* Reset the Chapter position of the old title */
        p_input->stream.p_selected_area->i_part = 0;

        /*
         *  We have to load all title information
         */
        /* Change the default area */
        p_input->stream.p_selected_area =
                    p_input->stream.pp_areas[p_area->i_id];

        /* title number: it is not vts nb!,
         * it is what appears in the interface list */
        p_dvd->i_title = p_area->i_id;
        p_dvd->p_ifo->i_title = p_dvd->i_title;

        /* set number of chapters of current title */
        p_dvd->i_chapter_nb = p_area->i_part_nb;

        /* ifo vts */
        if( IfoTitleSet( p_dvd->p_ifo ) < 0 )
        {
            intf_ErrMsg( "dvd error: fatal error in vts ifo" );
            free( p_dvd );
            p_input->b_error = 1;
            return -1;
        }

#define vmg p_dvd->p_ifo->vmg
#define vts p_dvd->p_ifo->vts
        /* title position inside the selected vts */
        i_vts_title = vmg.title_inf.p_attr[p_dvd->i_title-1].i_title_num;
        p_dvd->i_title_id =
            vts.title_inf.p_title_start[i_vts_title-1].i_title_id;

        intf_WarnMsg( 3, "dvd: title %d vts_title %d pgc %d",
                      p_dvd->i_title, i_vts_title, p_dvd->i_title_id );

        /*
         * Angle management
         */
        p_dvd->i_angle_nb = vmg.title_inf.p_attr[p_dvd->i_title-1].i_angle_nb;
        if( ( p_dvd->i_angle <= 0 ) || p_dvd->i_angle > p_dvd->i_angle_nb )
        {
            p_dvd->i_angle = 1;
        }
    
        /*
         * Set selected title start and size
         */
        
        /* title set offset XXX: convert to block values */
        p_dvd->i_title_start =
            vts.i_pos + vts.manager_inf.i_title_vob_start_sector;

        /* last video cell */
        p_dvd->i_cell = 0;
        p_dvd->i_prg_cell = -1 +
            vts.title_unit.p_title[p_dvd->i_title_id-1].title.i_cell_nb;

        if( DVDFindCell( p_dvd ) < 0 )
        {
            intf_ErrMsg( "dvd error: can't find title end" );
            p_input->b_error = 1;
            return -1;
        }
        
        /* temporary hack to fix size in some dvds */
        if( p_dvd->i_cell >= vts.cell_inf.i_cell_nb )
        {
            p_dvd->i_cell = vts.cell_inf.i_cell_nb - 1;
        }

        p_dvd->i_sector = 0;
        p_dvd->i_size = vts.cell_inf.p_cell_map[p_dvd->i_cell].i_end_sector;

        if( DVDChapterSelect( p_dvd, 1 ) < 0 )
        {
            intf_ErrMsg( "dvd error: can't find first chapter" );
            p_input->b_error = 1;
            return -1;
        }
        
        /* Force libdvdcss to check its title key.
         * It is only useful for title cracking method. Methods using the
         * decrypted disc key are fast enough to check the key at each seek */

        if( dvdcss_seek( p_dvd->dvdhandle, p_dvd->i_start,
                            DVDCSS_SEEK_KEY ) < 0 )
        {
            intf_ErrMsg( "dvd error: %s", dvdcss_error( p_dvd->dvdhandle ) );
            return -1;
        }

        p_dvd->i_size -= p_dvd->i_sector + 1;

        IfoPrintTitle( p_dvd );

        /* Area definition */
        p_input->stream.p_selected_area->i_start = LB2OFF( p_dvd->i_start );
        p_input->stream.p_selected_area->i_size = LB2OFF( p_dvd->i_size );
        p_input->stream.p_selected_area->i_angle_nb = p_dvd->i_angle_nb;
        p_input->stream.p_selected_area->i_angle = p_dvd->i_angle;

#if 0
        /* start at the beginning of the title */
        /* FIXME: create a conf option to select whether to restart
         * title or not */
        p_input->stream.p_selected_area->i_tell = 0;
        p_input->stream.p_selected_area->i_part = 1;
#endif

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
            input_DelProgram( p_input, p_input->stream.p_selected_program );

            p_input->stream.pp_selected_es = NULL;
            p_input->stream.i_selected_es_number = 0;
        }

        input_AddProgram( p_input, 0, sizeof( stream_ps_data_t ) );
        p_input->stream.p_selected_program = p_input->stream.pp_programs[0]; 
//        p_input->stream.p_new_program = p_input->stream.pp_programs[0]; 

        /* No PSM to read in DVD mode, we already have all information */
        p_input->stream.p_selected_program->b_is_ok = 1;

        p_es = NULL;

        /* ES 0 -> video MPEG2 */
        IfoPrintVideo( p_dvd );

        p_es = input_AddES( p_input, p_input->stream.p_selected_program, 
                0xe0, 0 );
        p_es->i_stream_id = 0xe0;
        p_es->i_type = MPEG2_VIDEO_ES;
        p_es->i_cat = VIDEO_ES;
        
#define audio_status \
    vts.title_unit.p_title[p_dvd->i_title_id-1].title.pi_audio_status[i-1]
        /* Audio ES, in the order they appear in .ifo */
        for( i = 1 ; i <= vts.manager_inf.i_audio_nb ; i++ )
        {
            IfoPrintAudio( p_dvd, i );

            /* audio channel is active if first byte is 0x80 */
            if( audio_status.i_available )
            {
                i_audio_nb++;

                switch( vts.manager_inf.p_audio_attr[i-1].i_coding_mode )
                {
                case 0x00:              /* AC3 */
                    i_id = ( ( 0x80 + audio_status.i_position ) << 8 ) | 0xbd;
                    p_es = input_AddES( p_input,
                               p_input->stream.p_selected_program, i_id, 0 );
                    p_es->i_stream_id = 0xbd;
                    p_es->i_type = AC3_AUDIO_ES;
                    p_es->b_audio = 1;
                    p_es->i_cat = AUDIO_ES;
                    strcpy( p_es->psz_desc, DecodeLanguage( hton16(
                        vts.manager_inf.p_audio_attr[i-1].i_lang_code ) ) ); 
                    strcat( p_es->psz_desc, " (ac3)" );
    
                    break;
                case 0x02:
                case 0x03:              /* MPEG audio */
                    i_id = 0xc0 + audio_status.i_position;
                    p_es = input_AddES( p_input,
                                    p_input->stream.p_selected_program, i_id
                                    , 0 );
                    p_es->i_stream_id = i_id;
                    p_es->i_type = MPEG2_AUDIO_ES;
                    p_es->b_audio = 1;
                    p_es->i_cat = AUDIO_ES;
                    strcpy( p_es->psz_desc, DecodeLanguage( hton16(
                        vts.manager_inf.p_audio_attr[i-1].i_lang_code ) ) ); 
                    strcat( p_es->psz_desc, " (mpeg)" );
    
                    break;
                case 0x04:              /* LPCM */
    
                    i_id = ( ( 0xa0 + audio_status.i_position ) << 8 ) | 0xbd;
                    p_es = input_AddES( p_input,
                                    p_input->stream.p_selected_program,
                                    i_id, 0 );
                    p_es->i_stream_id = 0xbd;
                    p_es->i_type = LPCM_AUDIO_ES;
                    p_es->b_audio = 1;
                    p_es->i_cat = AUDIO_ES;
                    strcpy( p_es->psz_desc, DecodeLanguage( hton16(
                        vts.manager_inf.p_audio_attr[i-1].i_lang_code ) ) ); 
                    strcat( p_es->psz_desc, " (lpcm)" );
    
                    break;
                case 0x06:              /* DTS */
                    i_id = ( ( 0x88 + audio_status.i_position ) << 8 ) | 0xbd;
                    intf_ErrMsg( "dvd warning: DTS audio not handled yet"
                                 "(0x%x)", i_id );
                    break;
                default:
                    i_id = 0;
                    intf_ErrMsg( "dvd warning: unknown audio type %.2x",
                             vts.manager_inf.p_audio_attr[i-1].i_coding_mode );
                }
            }
        }
#undef audio_status
#define spu_status \
    vts.title_unit.p_title[p_dvd->i_title_id-1].title.pi_spu_status[i-1]

        /* Sub Picture ES */
           
        for( i = 1 ; i <= vts.manager_inf.i_spu_nb; i++ )
        {
            IfoPrintSpu( p_dvd, i );

            if( spu_status.i_available )
            {
                i_spu_nb++;

                /*  there are several streams for one spu */
                if(  vts.manager_inf.video_attr.i_ratio )
                {
                    /* 16:9 */
                    switch( vts.manager_inf.video_attr.i_perm_displ )
                    {
                    case 1:
                        i_id = ( ( 0x20 + spu_status.i_position_pan ) << 8 )
                               | 0xbd;
                        break;
                    case 2:
                        i_id = ( ( 0x20 + spu_status.i_position_letter ) << 8 )
                               | 0xbd;
                        break;
                    default:
                        i_id = ( ( 0x20 + spu_status.i_position_wide ) << 8 )
                               | 0xbd;
                        break;
                    }
                }
                else
                {
                    /* 4:3 */
                    i_id = ( ( 0x20 + spu_status.i_position_43 ) << 8 )
                           | 0xbd;
                }
                p_es = input_AddES( p_input,
                                    p_input->stream.p_selected_program,
                                    i_id, 0 );
                p_es->i_stream_id = 0xbd;
                p_es->i_type = DVD_SPU_ES;
                p_es->i_cat = SPU_ES;
                strcpy( p_es->psz_desc, DecodeLanguage( hton16(
                    vts.manager_inf.p_spu_attr[i-1].i_lang_code ) ) ); 
            }
        }
#undef spu_status
    
        /* FIXME: hack to check that the demuxer is ready, and set
        * the decoders */
        if( p_input->p_demux_module )
        {
            DVDLaunchDecoders( p_input );
        }

    } /* i_title >= 0 */
    else
    {
        p_area = p_input->stream.p_selected_area;
    }
#undef vts
#undef vmg

    /*
     * Chapter selection
     */

    if( p_area->i_part != p_dvd->i_chapter )
    {
        if( ( p_area->i_part > 0 ) &&
            ( p_area->i_part <= p_area->i_part_nb ))
        {
            if( DVDChapterSelect( p_dvd, p_area->i_part ) < 0 )
            {
                intf_ErrMsg( "dvd error: can't set chapter in area" );
                p_input->b_error = 1;
                return -1;
            }
    
            p_input->stream.p_selected_area->i_tell =
                                   LB2OFF( p_dvd->i_start ) - p_area->i_start;
            p_input->stream.p_selected_area->i_part = p_dvd->i_chapter;
    
            intf_WarnMsg( 4, "dvd info: chapter %d start at: %lld",
                                        p_area->i_part, p_area->i_tell );
        }
        else
        {
            p_area->i_part = 1;
            p_dvd->i_chapter = 1;
        }
    }

#define title \
    p_dvd->p_ifo->vts.title_unit.p_title[p_dvd->i_title_id-1].title
    if( p_area->i_angle != p_dvd->i_angle )
    {
        if( title.p_cell_play[p_dvd->i_prg_cell].i_category & 0xf000 )
        {
            if( ( p_area->i_angle - p_dvd->i_angle ) < 0 )
            {
                p_dvd->i_cell = 0;
            }
            p_dvd->i_prg_cell += ( p_area->i_angle - p_dvd->i_angle );
            p_dvd->i_angle = p_area->i_angle;
    
            DVDFindSector( p_dvd );
            p_dvd->i_cell += p_dvd->i_angle_cell;
        }
        else
        {
            p_dvd->i_angle = p_area->i_angle;
        }

        intf_WarnMsg( 3, "dvd info: angle %d selected", p_area->i_angle );
    }

    /* warn interface that something has changed */
    p_input->stream.b_seekable = 1;
    p_input->stream.b_changed = 1;

    return 0;
}


/*****************************************************************************
 * DVDRead: reads data packets.
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, otherwise the number of
 * bytes.
 *****************************************************************************/
static int DVDRead( input_thread_t * p_input,
                    byte_t * p_buffer, size_t i_count )
{
    thread_dvd_data_t *     p_dvd;
    int                     i_block_once;
    int                     i_read_blocks;
    int                     i_read_total;
    int                     i_sector;
    int                     i_blocks;
    boolean_t               b_eoc;

    p_dvd = (thread_dvd_data_t *)(p_input->p_access_data);

    i_sector = 0;
    i_read_total = 0;
    i_read_blocks = 0;
    b_eoc = 0;

    i_blocks = OFF2LB(i_count);

    while( i_blocks )
    {
        i_sector = p_dvd->i_title_start + p_dvd->i_sector;
        i_block_once = p_dvd->i_end_sector - p_dvd->i_sector + 1;

        /* Get the position of the next cell if we're at cell end */
        if( i_block_once <= 0 )
        {
            int     i_angle;

            p_dvd->i_cell++;
            p_dvd->i_angle_cell++;

            /* Find cell index in adress map */
            if( DVDFindSector( p_dvd ) < 0 )
            {
                intf_ErrMsg( "dvd error: can't find next cell" );
                return 1;
            }

            /* Position the fd pointer on the right address */
            if( ( i_sector = dvdcss_seek( p_dvd->dvdhandle,
                                p_dvd->i_title_start + p_dvd->i_sector,
                                DVDCSS_SEEK_MPEG ) ) < 0 )
            {
                intf_ErrMsg( "dvd error: %s", dvdcss_error( p_dvd->dvdhandle ) );
                return -1;
            }

            /* update chapter : it will be easier when we have navigation
             * ES support */
            if( p_dvd->i_chapter < ( p_dvd->i_chapter_nb - 1 ) )
            {
                if( title.p_cell_play[p_dvd->i_prg_cell].i_category & 0xf000 )
                {
                    i_angle = p_dvd->i_angle - 1;
                }
                else
                {
                    i_angle = 0;
                }
                if( title.chapter_map.pi_start_cell[p_dvd->i_chapter] <=
                    ( p_dvd->i_prg_cell - i_angle + 1 ) )
                {
                    p_dvd->i_chapter++;
                    b_eoc = 1;
                }
            }

            i_block_once = p_dvd->i_end_sector - p_dvd->i_sector + 1;
        }

        /* The number of blocks read is the max between the requested
         * value and the leaving block in the cell */
        if( i_block_once > i_blocks )
        {
            i_block_once = i_blocks;
        }
    /*
    intf_WarnMsg( 2, "Sector: 0x%x Read: %d Chapter: %d", p_dvd->i_sector, i_block_once, p_dvd->i_chapter );
    */

        /* Reads from DVD */
        i_read_blocks = dvdcss_read( p_dvd->dvdhandle, p_buffer,
                                     i_block_once, DVDCSS_READ_DECRYPT );

        i_blocks -= i_read_blocks;
        p_buffer += LB2OFF( i_read_blocks );
        i_read_total += i_read_blocks;

        /* Update global position */
        p_dvd->i_sector += i_read_blocks;
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );

    p_input->stream.p_selected_area->i_tell =
        LB2OFF( i_sector + i_read_total ) -
        p_input->stream.p_selected_area->i_start;
    if( b_eoc )
    {
        /* We modify i_part only at end of chapter not to erase
         * some modification from the interface */
        p_input->stream.p_selected_area->i_part = p_dvd->i_chapter;
    }

    if( p_input->stream.p_selected_area->i_tell
            >= p_input->stream.p_selected_area->i_size )
    {
        if( ( p_dvd->i_title + 1 ) >= p_input->stream.i_area_nb )
        {
            /* EOF */
            vlc_mutex_unlock( &p_input->stream.stream_lock );
            return 0;
        }

        /* EOT */
        intf_WarnMsg( 4, "dvd info: new title" );
        p_dvd->i_title++;
        DVDSetArea( p_input, p_input->stream.pp_areas[p_dvd->i_title] );
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        return LB2OFF( i_read_total );
    }

    vlc_mutex_unlock( &p_input->stream.stream_lock );
/*
    if( i_read_blocks != i_block_once )
    {
        return -1;
    }
*/
    return LB2OFF( i_read_total );
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
    int                     i_block;
    int                     i_prg_cell;
    int                     i_cell;
    int                     i_chapter;
    int                     i_angle;
    
    p_dvd = ( thread_dvd_data_t * )(p_input->p_access_data);

    vlc_mutex_lock( &p_input->stream.stream_lock );
    /* we have to take care of offset of beginning of title */
    p_dvd->i_sector = OFF2LB(i_off + p_input->stream.p_selected_area->i_start)
                       - p_dvd->i_title_start;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    i_prg_cell = 0;
    i_chapter = 0;

    /* parse vobu address map to find program cell */
    while( title.p_cell_play[i_prg_cell].i_end_sector < p_dvd->i_sector  )
    {
        i_prg_cell++;
    }

    p_dvd->i_prg_cell = i_prg_cell;

    if( DVDChooseAngle( p_dvd ) < 0 )
    {
        p_input->b_error = 1;
        return;        
    }

    p_dvd->i_cell = 0;

    /* Find first title cell which is inside program cell */
    if( DVDFindCell( p_dvd ) < 0 )
    {
        /* no following cell : we're at eof */
        intf_ErrMsg( "dvd error: cell seeking failed" );
        p_input->b_error = 1;
        return;
    }

    i_cell = p_dvd->i_cell;

#define cell p_dvd->p_ifo->vts.cell_inf.p_cell_map[i_cell]
    /* parse cell address map to find title cell containing sector */
    while( cell.i_end_sector < p_dvd->i_sector )
    {
        i_cell++;
    }

    p_dvd->i_cell = i_cell;

    /* if we're inside a multi-angle zone, we have to choose i_sector
     * in the current angle ; we can't do it all the time since cells
     * can be very wide out of such zones */
    if( title.p_cell_play[p_dvd->i_prg_cell].i_category & 0xf000 )
    {
        p_dvd->i_sector = __MAX(
                cell.i_start_sector,
                title.p_cell_play[p_dvd->i_prg_cell].i_start_sector );
    }

    p_dvd->i_end_sector = __MIN(
            cell.i_end_sector,
            title.p_cell_play[p_dvd->i_prg_cell].i_end_sector );
#undef cell
    /* update chapter */
    if( title.p_cell_play[p_dvd->i_prg_cell].i_category & 0xf000 )
    {
        i_angle = p_dvd->i_angle - 1;
    }
    else
    {
        i_angle = 0;
    }
    if( p_dvd->i_chapter_nb > 1 )
    {
        while( ( title.chapter_map.pi_start_cell[i_chapter] <=
                    ( p_dvd->i_prg_cell - i_angle + 1 ) ) &&
               ( i_chapter < ( p_dvd->i_chapter_nb - 1 ) ) )
        {
            i_chapter++;
        }
    }
    else
    {
        i_chapter = 1;
    }

    p_dvd->i_chapter = i_chapter;

    if( ( i_block = dvdcss_seek( p_dvd->dvdhandle,
                                 p_dvd->i_title_start + p_dvd->i_sector,
                                 DVDCSS_SEEK_MPEG ) ) < 0 )
    {
        intf_ErrMsg( "dvd error: %s", dvdcss_error( p_dvd->dvdhandle ) );
        p_input->b_error = 1;
        return;
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.p_selected_area->i_part = p_dvd->i_chapter;
    p_input->stream.p_selected_area->i_tell =
        LB2OFF ( i_block ) - p_input->stream.p_selected_area->i_start;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    intf_WarnMsg( 7, "Program Cell: %d Cell: %d Chapter: %d",
                     p_dvd->i_prg_cell, p_dvd->i_cell, p_dvd->i_chapter );

    return;
}

#define cell  p_dvd->p_ifo->vts.cell_inf

/*****************************************************************************
 * DVDFindCell: adjust the title cell index with the program cell
 *****************************************************************************/
static int DVDFindCell( thread_dvd_data_t * p_dvd )
{
    int                 i_cell;
    int                 i_index;

    i_cell = p_dvd->i_cell;
    i_index = p_dvd->i_prg_cell;

    if( i_cell >= cell.i_cell_nb )
    {
        return -1;
    }

    while( ( ( title.p_cell_pos[i_index].i_vob_id !=
                   cell.p_cell_map[i_cell].i_vob_id ) ||
      ( title.p_cell_pos[i_index].i_cell_id !=
                   cell.p_cell_map[i_cell].i_cell_id ) ) &&
           ( i_cell < cell.i_cell_nb - 1 ) )
    {
        i_cell++;
    }

/*
intf_WarnMsg( 12, "FindCell: i_cell %d i_index %d found %d nb %d",
                    p_dvd->i_cell,
                    p_dvd->i_prg_cell,
                    i_cell,
                    cell.i_cell_nb );
*/

    p_dvd->i_cell = i_cell;

    return 0;    
}

#undef cell

/*****************************************************************************
 * DVDFindSector: find cell index in adress map from index in
 * information table program map and give corresponding sectors.
 *****************************************************************************/
static int DVDFindSector( thread_dvd_data_t * p_dvd )
{

    if( p_dvd->i_sector > title.p_cell_play[p_dvd->i_prg_cell].i_end_sector )
    {
        p_dvd->i_prg_cell++;

        if( DVDChooseAngle( p_dvd ) < 0 )
        {
            return -1;
        }
    }

    if( DVDFindCell( p_dvd ) < 0 )
    {
        intf_ErrMsg( "dvd error: can't find sector" );
        return -1;
    }
    
    /* Find start and end sectors of new cell */
#if 1
    p_dvd->i_sector = __MAX(
         p_dvd->p_ifo->vts.cell_inf.p_cell_map[p_dvd->i_cell].i_start_sector,
         title.p_cell_play[p_dvd->i_prg_cell].i_start_sector );
    p_dvd->i_end_sector = __MIN(
         p_dvd->p_ifo->vts.cell_inf.p_cell_map[p_dvd->i_cell].i_end_sector,
         title.p_cell_play[p_dvd->i_prg_cell].i_end_sector );
#else
    p_dvd->i_sector = title.p_cell_play[p_dvd->i_prg_cell].i_start_sector;
    p_dvd->i_end_sector = title.p_cell_play[p_dvd->i_prg_cell].i_end_sector;
#endif

/*
    intf_WarnMsg( 12, "cell: %d sector1: 0x%x end1: 0x%x\n"
                   "index: %d sector2: 0x%x end2: 0x%x\n"
                   "category: 0x%x ilvu end: 0x%x vobu start 0x%x", 
        p_dvd->i_cell,
        p_dvd->p_ifo->vts.cell_inf.p_cell_map[p_dvd->i_cell].i_start_sector,
        p_dvd->p_ifo->vts.cell_inf.p_cell_map[p_dvd->i_cell].i_end_sector,
        p_dvd->i_prg_cell,
        title.p_cell_play[p_dvd->i_prg_cell].i_start_sector,
        title.p_cell_play[p_dvd->i_prg_cell].i_end_sector,
        title.p_cell_play[p_dvd->i_prg_cell].i_category, 
        title.p_cell_play[p_dvd->i_prg_cell].i_first_ilvu_vobu_esector,
        title.p_cell_play[p_dvd->i_prg_cell].i_last_vobu_start_sector );
*/

    return 0;
}

/*****************************************************************************
 * DVDChapterSelect: find the cell corresponding to requested chapter
 *****************************************************************************/
static int DVDChapterSelect( thread_dvd_data_t * p_dvd, int i_chapter )
{

    /* Find cell index in Program chain for current chapter */
    p_dvd->i_prg_cell = title.chapter_map.pi_start_cell[i_chapter-1] - 1;
    p_dvd->i_cell = 0;
    p_dvd->i_sector = 0;

    DVDChooseAngle( p_dvd );

    /* Search for cell_index in cell adress_table and initialize
     * start sector */
    if( DVDFindSector( p_dvd ) < 0 )
    {
        intf_ErrMsg( "dvd error: can't select chapter" );
        return -1;
    }

    /* start is : beginning of vts vobs + offset to vob x */
    p_dvd->i_start = p_dvd->i_title_start + p_dvd->i_sector;

    /* Position the fd pointer on the right address */
    if( ( p_dvd->i_start = dvdcss_seek( p_dvd->dvdhandle,
                                        p_dvd->i_start,
                                        DVDCSS_SEEK_MPEG ) ) < 0 )
    {
        intf_ErrMsg( "dvd error: %s", dvdcss_error( p_dvd->dvdhandle ) );
        return -1;
    }

    p_dvd->i_chapter = i_chapter;
    return 0;
}

/*****************************************************************************
 * DVDChooseAngle: select the cell corresponding to the selected angle
 *****************************************************************************/
static int DVDChooseAngle( thread_dvd_data_t * p_dvd )
{
    /* basic handling of angles */
    switch( ( ( title.p_cell_play[p_dvd->i_prg_cell].i_category & 0xf000 )
                    >> 12 ) )
    {
        /* we enter a muli-angle section */
        case 0x5:
            p_dvd->i_prg_cell += p_dvd->i_angle - 1;
            p_dvd->i_angle_cell = 0;
            break;
        /* we exit a multi-angle section */
        case 0x9:
        case 0xd:
            p_dvd->i_prg_cell += p_dvd->i_angle_nb - p_dvd->i_angle;
            break;
    }

    return 0;
}

#undef title
/*****************************************************************************
 * DVDLaunchDecoders
 *****************************************************************************/
static void DVDLaunchDecoders( input_thread_t * p_input )
{
    thread_dvd_data_t *  p_dvd;
    int                  i_audio;
    int                  i_spu;

    p_dvd = (thread_dvd_data_t*)(p_input->p_access_data);

    /* Select Video stream (always 0) */
    if( p_main->b_video )
    {
        input_SelectES( p_input, p_input->stream.pp_es[0] );
    }

    /* Select audio stream */
    if( p_main->b_audio )
    {
        /* For audio: first one if none or a not existing one specified */
        i_audio = config_GetIntVariable( INPUT_CHANNEL_VAR );
        if( i_audio < 0 /*|| i_audio > i_audio_nb*/ )
        {
            config_PutIntVariable( INPUT_CHANNEL_VAR, 1 );
            i_audio = 1;
        }
        if( i_audio > 0 /*&& i_audio_nb > 0*/ )
        {
            if( config_GetIntVariable( AOUT_SPDIF_VAR ) ||
                ( config_GetIntVariable( INPUT_AUDIO_VAR ) ==
                  REQUESTED_AC3 ) )
            {
                int     i_ac3 = i_audio;
                while( ( p_input->stream.pp_es[i_ac3]->i_type !=
                         AC3_AUDIO_ES ) && ( i_ac3 <=
                         p_dvd->p_ifo->vts.manager_inf.i_audio_nb ) )
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

    /* Select subtitle */
    if( p_main->b_video )
    {
        /* for spu, default is none */
        i_spu = config_GetIntVariable( INPUT_SUBTITLE_VAR );
        if( i_spu < 0 /*|| i_spu > i_spu_nb*/ )
        {
            config_PutIntVariable( INPUT_SUBTITLE_VAR, 0 );
            i_spu = 0;
        }
        if( i_spu > 0 /* && i_spu_nb > 0*/ )
        {
            i_spu += p_dvd->p_ifo->vts.manager_inf.i_audio_nb;
            input_SelectES( p_input, p_input->stream.pp_es[i_spu] );
        }
    }
}
