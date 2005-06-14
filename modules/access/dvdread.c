*****************************************************************************
 * dvdread.c : DvdRead input module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2004 VideoLAN
 * $Id$
 *
 * Authors: Stéphane Borel <stef@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "iso_lang.h"

#include "../demux/ps.h"

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#include <dvdread/dvd_reader.h>
#include <dvdread/ifo_types.h>
#include <dvdread/ifo_read.h>
#include <dvdread/nav_read.h>
#include <dvdread/nav_print.h>

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ANGLE_TEXT N_("DVD angle")
#define ANGLE_LONGTEXT N_( \
    "Allows you to select the default DVD angle." )

#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for DVDread streams. " \
    "This value should be set in millisecond units." )

#define CSSMETHOD_TEXT N_("Method used by libdvdcss for decryption")
#define CSSMETHOD_LONGTEXT N_( \
    "Set the method used by libdvdcss for key decryption.\n" \
    "title: decrypted title key is guessed from the encrypted sectors of " \
           "the stream. Thus it should work with a file as well as the " \
           "DVD device. But it sometimes takes much time to decrypt a title " \
           "key and may even fail. With this method, the key is only checked "\
           "at the beginning of each title, so it won't work if the key " \
           "changes in the middle of a title.\n" \
    "disc: the disc key is first cracked, then all title keys can be " \
           "decrypted instantly, which allows us to check them often.\n" \
    "key: the same as \"disc\" if you don't have a file with player keys " \
           "at compilation time. If you do, the decryption of the disc key " \
           "will be faster with this method. It is the one that was used by " \
           "libcss.\n" \
    "The default method is: key.")

static char *psz_css_list[] = { "title", "disc", "key" };
static char *psz_css_list_text[] = { N_("title"), N_("Disc"), N_("Key") };

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_shortname( _("DVD without menus") );
    set_description( _("DVDRead Input (DVD without menu support)") );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACCESS );
    add_integer( "dvdread-angle", 1, NULL, ANGLE_TEXT,
        ANGLE_LONGTEXT, VLC_FALSE );
    add_integer( "dvdread-caching", DEFAULT_PTS_DELAY / 1000, NULL,
        CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );
    add_string( "dvdread-css-method", NULL, NULL, CSSMETHOD_TEXT,
                CSSMETHOD_LONGTEXT, VLC_TRUE );
        change_string_list( psz_css_list, psz_css_list_text, 0 );
    set_capability( "access_demux", 0 );
    add_shortcut( "dvd" );
    add_shortcut( "dvdread" );
    add_shortcut( "dvdsimple" );
    set_callbacks( Open, Close );
vlc_module_end();

/* how many blocks DVDRead will read in each loop */
#define DVD_BLOCK_READ_ONCE 4

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

struct demux_sys_t
{
    /* DVDRead state */
    dvd_reader_t *p_dvdread;
    dvd_file_t   *p_title;

    ifo_handle_t *p_vmg_file;
    ifo_handle_t *p_vts_file;

    int i_title;
    int i_chapter, i_chapters;
    int i_angle, i_angles;

    tt_srpt_t    *p_tt_srpt;
    pgc_t        *p_cur_pgc;
    dsi_t        dsi_pack;
    int          i_ttn;

    int i_pack_len;
    int i_cur_block;
    int i_next_vobu;

    /* Current title start/end blocks */
    int i_title_start_block;
    int i_title_end_block;
    int i_title_blocks;
    int i_title_offset;
    mtime_t i_title_cur_time;

    int i_title_start_cell;
    int i_title_end_cell;
    int i_cur_cell;
    int i_next_cell;
    mtime_t i_cell_cur_time;
    mtime_t i_cell_duration;

    /* Track */
    ps_track_t    tk[PS_TK_COUNT];

    int           i_titles;
    input_title_t **titles;

    /* Video */
    int i_aspect;

    /* SPU */
    uint32_t clut[16];
};

static int Control   ( demux_t *, int, va_list );
static int Demux     ( demux_t * );
static int DemuxBlock( demux_t *, uint8_t *, int );

static void DemuxTitles( demux_t *, int * );
static void ESNew( demux_t *, int, int );

static int  DvdReadSetArea  ( demux_t *, int, int, int );
static void DvdReadSeek     ( demux_t *, int );
static void DvdReadHandleDSI( demux_t *, uint8_t * );
static void DvdReadFindCell ( demux_t * );

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    demux_t      *p_demux = (demux_t*)p_this;
    demux_sys_t  *p_sys;
    char         *psz_name;
    char         *psz_dvdcss_env;
    dvd_reader_t *p_dvdread;
    ifo_handle_t *p_vmg_file;
    vlc_value_t  val;

    if( !p_demux->psz_path || !*p_demux->psz_path )
    {
        /* Only when selected */
        if( !p_this->b_force ) return VLC_EGENERIC;

        psz_name = var_CreateGetString( p_this, "dvd" );
        if( !psz_name || !*psz_name )
        {
            if( psz_name ) free( psz_name );
            return VLC_EGENERIC;
        }
    }
    else psz_name = strdup( p_demux->psz_path );

#ifdef WIN32
    if( psz_name[0] && psz_name[1] == ':' &&
        psz_name[2] == '\\' && psz_name[3] == '\0' ) psz_name[2] = '\0';
#endif

    /* Override environment variable DVDCSS_METHOD with config option
     * (FIXME: this creates a small memory leak) */
    psz_dvdcss_env = config_GetPsz( p_demux, "dvdread-css-method" );
    if( psz_dvdcss_env && *psz_dvdcss_env )
    {
        char *psz_env;

        psz_env = malloc( strlen("DVDCSS_METHOD=") +
                          strlen( psz_dvdcss_env ) + 1 );
        if( !psz_env )
        {
            free( psz_dvdcss_env );
            return VLC_ENOMEM;
        }

        sprintf( psz_env, "%s%s", "DVDCSS_METHOD=", psz_dvdcss_env );

        putenv( psz_env );
    }
    if( psz_dvdcss_env ) free( psz_dvdcss_env );

    /* Open dvdread */
    if( !(p_dvdread = DVDOpen( psz_name )) )
    {
        msg_Err( p_demux, "DVDRead cannot open source: %s", psz_name );
        free( psz_name );
        return VLC_EGENERIC;
    }
    free( psz_name );

    /* Ifo allocation & initialisation */
    if( !( p_vmg_file = ifoOpen( p_dvdread, 0 ) ) )
    {
        msg_Warn( p_demux, "cannot open VMG info" );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_demux, "VMG opened" );

    /* Fill p_demux field */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;
    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );
    memset( p_sys, 0, sizeof( demux_sys_t ) );

    ps_track_init( p_sys->tk );
    p_sys->i_aspect = -1;
    p_sys->i_title_cur_time = (mtime_t) 0;
    p_sys->i_cell_cur_time = (mtime_t) 0;
    p_sys->i_cell_duration = (mtime_t) 0;

    p_sys->p_dvdread = p_dvdread;
    p_sys->p_vmg_file = p_vmg_file;
    p_sys->p_title = NULL;
    p_sys->p_vts_file = NULL;

    p_sys->i_title = p_sys->i_chapter = -1;

    var_Create( p_demux, "dvdread-angle", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );
    var_Get( p_demux, "dvdread-angle", &val );
    p_sys->i_angle = val.i_int > 0 ? val.i_int : 1;

    DemuxTitles( p_demux, &p_sys->i_angle );
    if( DvdReadSetArea( p_demux, 0, 0, p_sys->i_angle ) != VLC_SUCCESS )
    {
        Close( p_this );
        msg_Err( p_demux, "DvdReadSetArea(0,0,%i) failed (can't decrypt DVD?)",
                 p_sys->i_angle );
        return VLC_EGENERIC;
    }

    /* Update default_pts to a suitable value for dvdread access */
    var_Create( p_demux, "dvdread-caching",
                VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;
    int i;

    for( i = 0; i < PS_TK_COUNT; i++ )
    {
        ps_track_t *tk = &p_sys->tk[i];
        if( tk->b_seen )
        {
            es_format_Clean( &tk->fmt );
            if( tk->es ) es_out_Del( p_demux->out, tk->es );
        }
    }

    /* Close libdvdread */
    if( p_sys->p_title ) DVDCloseFile( p_sys->p_title );
    if( p_sys->p_vts_file ) ifoClose( p_sys->p_vts_file );
    if( p_sys->p_vmg_file ) ifoClose( p_sys->p_vmg_file );
    DVDClose( p_sys->p_dvdread );

    free( p_sys );
}

static int64_t dvdtime_to_time( dvd_time_t *dtime, uint8_t still_time )
{
/* Macro to convert Binary Coded Decimal to Decimal */
#define BCD2D(__x__) (((__x__ & 0xf0) >> 4) * 10 + (__x__ & 0x0f))

    double f_fps, f_ms;
    int64_t i_micro_second = 0;

    if (still_time == 0 || still_time == 0xFF)
    {
        i_micro_second += (int64_t)(BCD2D(dtime->hour)) * 60 * 60 * 1000000;
        i_micro_second += (int64_t)(BCD2D(dtime->minute)) * 60 * 1000000;
        i_micro_second += (int64_t)(BCD2D(dtime->second)) * 1000000;

        switch((dtime->frame_u & 0xc0) >> 6) 
        {
        case 1:
            f_fps = 25.0;
            break;
        case 3:
            f_fps = 29.97;
            break;
        default:
            f_fps = 2500.0;
            break;
        }
        f_ms = BCD2D(dtime->frame_u&0x3f) * 1000.0 / f_fps;
        i_micro_second += (int64_t)(f_ms * 1000.0);
    }
    else
    {
        i_micro_second = still_time;
        i_micro_second = (int64_t)((double)i_micro_second * 1000000.0);
    }
    
    return i_micro_second;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    double f, *pf;
    vlc_bool_t *pb;
    int64_t *pi64;
    input_title_t ***ppp_title;
    int *pi_int;
    int i;

    switch( i_query )
    {
        case DEMUX_GET_POSITION:
        {
            pf = (double*) va_arg( args, double* );

            if( p_sys->i_title_blocks > 0 )
                *pf = (double)p_sys->i_title_offset / p_sys->i_title_blocks;
            else
                *pf = 0.0;

            return VLC_SUCCESS;
        }
        case DEMUX_SET_POSITION:
        {
            f = (double)va_arg( args, double );

            DvdReadSeek( p_demux, f * p_sys->i_title_blocks );

            return VLC_SUCCESS;
        }
        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_sys->i_title_cur_time > 0 )
            {
                *pi64 = (int64_t)p_sys->i_title_cur_time;
                return VLC_SUCCESS;
            }
            *pi64 = 0;
            return VLC_EGENERIC;

        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if (p_demux->info.i_title >= 0 && p_demux->info.i_title < p_sys->i_titles)
            {
                *pi64 = dvdtime_to_time( &p_sys->p_cur_pgc->playback_time, 0 );
                return VLC_SUCCESS;
            }
            *pi64 = 0;
            return VLC_EGENERIC;

        /* Special for access_demux */
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_CONTROL_PACE:
            /* TODO */
            pb = (vlc_bool_t*)va_arg( args, vlc_bool_t * );
            *pb = VLC_TRUE;
            return VLC_SUCCESS;

        case DEMUX_SET_PAUSE_STATE:
            return VLC_SUCCESS;

        case DEMUX_GET_TITLE_INFO:
            ppp_title = (input_title_t***)va_arg( args, input_title_t*** );
            pi_int    = (int*)va_arg( args, int* );
            *((int*)va_arg( args, int* )) = 1; /* Title offset */
            *((int*)va_arg( args, int* )) = 1; /* Chapter offset */

            /* Duplicate title infos */
            *pi_int = p_sys->i_titles;
            *ppp_title = malloc( sizeof(input_title_t **) * p_sys->i_titles );
            for( i = 0; i < p_sys->i_titles; i++ )
            {
                (*ppp_title)[i] = vlc_input_title_Duplicate(p_sys->titles[i]);
            }
            return VLC_SUCCESS;

        case DEMUX_SET_TITLE:
            i = (int)va_arg( args, int );
            if( DvdReadSetArea( p_demux, i, 0, -1 ) != VLC_SUCCESS )
            {
                msg_Warn( p_demux, "cannot set title/chapter" );
                return VLC_EGENERIC;
            }
            p_demux->info.i_update |=
                INPUT_UPDATE_TITLE | INPUT_UPDATE_SEEKPOINT;
            p_demux->info.i_title = i;
            p_demux->info.i_seekpoint = 0;
            return VLC_SUCCESS;

        case DEMUX_SET_SEEKPOINT:
            i = (int)va_arg( args, int );
            if( DvdReadSetArea( p_demux, -1, i, -1 ) != VLC_SUCCESS )
            {
                msg_Warn( p_demux, "cannot set title/chapter" );
                return VLC_EGENERIC;
            }
            p_demux->info.i_update |= INPUT_UPDATE_SEEKPOINT;
            p_demux->info.i_seekpoint = i;
            return VLC_SUCCESS;

        case DEMUX_GET_PTS_DELAY:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = (int64_t)var_GetInteger( p_demux, "dvdread-caching" )*1000;
            return VLC_SUCCESS;

        /* TODO implement others */
        default:
            return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * Demux:
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    uint8_t p_buffer[DVD_VIDEO_LB_LEN * DVD_BLOCK_READ_ONCE];
    int i_blocks_once, i_read;
    int i;

    /*
     * Playback by cell in this pgc, starting at the cell for our chapter.
     */

    /*
     * Check end of pack, and select the following one
     */
    if( !p_sys->i_pack_len )
    {
        /* Read NAV packet */
        if( DVDReadBlocks( p_sys->p_title, p_sys->i_next_vobu,
                           1, p_buffer ) != 1 )
        {
            msg_Err( p_demux, "read failed for block %d", p_sys->i_next_vobu );
            return -1;
        }

        /* Basic check to be sure we don't have a empty title
         * go to next title if so */
        //assert( p_buffer[41] == 0xbf && p_buffer[1027] == 0xbf );

        /* Parse the contained dsi packet */
        DvdReadHandleDSI( p_demux, p_buffer );

        /* End of title */
        if( p_sys->i_cur_cell >= p_sys->p_cur_pgc->nr_of_cells )
        {
            if( p_sys->i_title + 1 >= p_sys->i_titles )
            {
                return 0; /* EOF */
            }

            DvdReadSetArea( p_demux, p_sys->i_title + 1, 0, -1 );
        }

        if( p_sys->i_pack_len >= 1024 )
        {
            msg_Err( p_demux, "i_pack_len >= 1024 (%i). "
                     "This shouldn't happen!", p_sys->i_pack_len );
            return 0; /* EOF */
        }

        /* FIXME: Ugly kludge: we send the pack block to the input for it
         * sometimes has a zero scr and restart the sync */
        p_sys->i_cur_block++;
        p_sys->i_title_offset++;

        DemuxBlock( p_demux, p_buffer, DVD_VIDEO_LB_LEN );
    }

    if( p_sys->i_cur_cell >= p_sys->p_cur_pgc->nr_of_cells )
    {
        if( p_sys->i_title + 1 >= p_sys->i_titles )
        {
            return 0; /* EOF */
        }

        DvdReadSetArea( p_demux, p_sys->i_title + 1, 0, -1 );
    }

    /*
     * Read actual data
     */
    i_blocks_once = __MIN( p_sys->i_pack_len, DVD_BLOCK_READ_ONCE );
    p_sys->i_pack_len -= i_blocks_once;

    /* Reads from DVD */
    i_read = DVDReadBlocks( p_sys->p_title, p_sys->i_cur_block,
                            i_blocks_once, p_buffer );
    if( i_read != i_blocks_once )
    {
        msg_Err( p_demux, "read failed for %d/%d blocks at 0x%02x",
                 i_read, i_blocks_once, p_sys->i_cur_block );
        return -1;
    }

    p_sys->i_cur_block += i_read;
    p_sys->i_title_offset += i_read;

#if 0
    msg_Dbg( p_demux, "i_blocks: %d len: %d current: 0x%02x",
             i_read, p_sys->i_pack_len, p_sys->i_cur_block );
#endif

    for( i = 0; i < i_read; i++ )
    {
        DemuxBlock( p_demux, p_buffer + i * DVD_VIDEO_LB_LEN,
                    DVD_VIDEO_LB_LEN );
    }

#undef p_pgc

    return 1;
}

/*****************************************************************************
 * DemuxBlock: demux a given block
 *****************************************************************************/
static int DemuxBlock( demux_t *p_demux, uint8_t *pkt, int i_pkt )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    uint8_t     *p = pkt;

    while( p < &pkt[i_pkt] )
    {
        int i_size = ps_pkt_size( p, &pkt[i_pkt] - p );
        block_t *p_pkt;
        if( i_size <= 0 )
        {
            break;
        }

        /* Create a block */
        p_pkt = block_New( p_demux, i_size );
        memcpy( p_pkt->p_buffer, p, i_size);

        /* Parse it and send it */
        switch( 0x100 | p[3] )
        {
        case 0x1b9:
        case 0x1bb:
        case 0x1bc:

#ifdef DVDREAD_DEBUG
            if( p[3] == 0xbc )
            {
                msg_Warn( p_demux, "received a PSM packet" );
            }
            else if( p[3] == 0xbb )
            {
                msg_Warn( p_demux, "received a SYSTEM packet" );
            }
#endif
            block_Release( p_pkt );
            break;

        case 0x1ba:
        {
            int64_t i_scr;
            int i_mux_rate;
            if( !ps_pkt_parse_pack( p_pkt, &i_scr, &i_mux_rate ) )
            {
                es_out_Control( p_demux->out, ES_OUT_SET_PCR, i_scr );
            }
            block_Release( p_pkt );
            break;
        }
        default:
        {
            int i_id = ps_pkt_id( p_pkt );
            if( i_id >= 0xc0 )
            {
                ps_track_t *tk = &p_sys->tk[PS_ID_TO_TK(i_id)];

                if( !tk->b_seen )
                {
                    ESNew( p_demux, i_id, 0 );
                }
                if( tk->b_seen && tk->es &&
                    !ps_pkt_parse_pes( p_pkt, tk->i_skip ) )
                {
                    es_out_Send( p_demux->out, tk->es, p_pkt );
                }
                else
                {
                    block_Release( p_pkt );
                }
            }
            else
            {
                block_Release( p_pkt );
            }
            break;
        }
        }

        p += i_size;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * ESNew: register a new elementary stream
 *****************************************************************************/
static void ESNew( demux_t *p_demux, int i_id, int i_lang )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    ps_track_t  *tk = &p_sys->tk[PS_ID_TO_TK(i_id)];
    char psz_language[3];

    if( tk->b_seen ) return;

    if( ps_track_fill( tk, 0, i_id ) )
    {
        msg_Warn( p_demux, "unknown codec for id=0x%x", i_id );
        return;
    }

    psz_language[0] = psz_language[1] = psz_language[2] = 0;
    if( i_lang && i_lang != 0xffff )
    {
        psz_language[0] = (i_lang >> 8)&0xff;
        psz_language[1] = (i_lang     )&0xff;
    }

    /* Add a new ES */
    if( tk->fmt.i_cat == VIDEO_ES )
    {
        if( p_sys->i_aspect >= 0 )
        {
            tk->fmt.video.i_aspect = p_sys->i_aspect;
        }
    }
    else if( tk->fmt.i_cat == AUDIO_ES )
    {
        int i_audio = -1;
        /* find the audio number PLEASE find another way */
        if( (i_id&0xbdf8) == 0xbd88 )       /* dts */
        {
            i_audio = i_id&0x07;
        }
        else if( (i_id&0xbdf0) == 0xbd80 )  /* a52 */
        {
            i_audio = i_id&0xf;
        }
        else if( (i_id&0xbdf0) == 0xbda0 )  /* lpcm */
        {
            i_audio = i_id&0x1f;
        }
        else if( ( i_id&0xe0 ) == 0xc0 )    /* mpga */
        {
            i_audio = i_id&0x1f;
        }

        if( psz_language[0] ) tk->fmt.psz_language = strdup( psz_language );
    }
    else if( tk->fmt.i_cat == SPU_ES )
    {
        /* Palette */
        tk->fmt.subs.spu.palette[0] = 0xBeef;
        memcpy( &tk->fmt.subs.spu.palette[1], p_sys->clut,
                16 * sizeof( uint32_t ) );

        if( psz_language[0] ) tk->fmt.psz_language = strdup( psz_language );
    }

    tk->es = es_out_Add( p_demux->out, &tk->fmt );
    tk->b_seen = VLC_TRUE;
}

/*****************************************************************************
 * DvdReadSetArea: initialize input data for title x, chapter y.
 * It should be called for each user navigation request.
 *****************************************************************************
 * Take care that i_title and i_chapter start from 0.
 *****************************************************************************/
static int DvdReadSetArea( demux_t *p_demux, int i_title, int i_chapter,
                           int i_angle )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int pgc_id = 0, pgn = 0;
    int i;

#define p_pgc p_sys->p_cur_pgc
#define p_vmg p_sys->p_vmg_file
#define p_vts p_sys->p_vts_file

    if( i_title >= 0 && i_title < p_sys->i_titles &&
        i_title != p_sys->i_title )
    {
        int i_start_cell, i_end_cell;

        if( p_sys->p_title != NULL ) DVDCloseFile( p_sys->p_title );
        if( p_vts != NULL ) ifoClose( p_vts );
        p_sys->i_title = i_title;

        /*
         *  We have to load all title information
         */
        msg_Dbg( p_demux, "open VTS %d, for title %d",
                 p_vmg->tt_srpt->title[i_title].title_set_nr, i_title + 1 );

        /* Ifo vts */
        if( !( p_vts = ifoOpen( p_sys->p_dvdread,
               p_vmg->tt_srpt->title[i_title].title_set_nr ) ) )
        {
            msg_Err( p_demux, "fatal error in vts ifo" );
            return VLC_EGENERIC;
        }

        /* Title position inside the selected vts */
        p_sys->i_ttn = p_vmg->tt_srpt->title[i_title].vts_ttn;

        /* Find title start/end */
        pgc_id = p_vts->vts_ptt_srpt->title[p_sys->i_ttn - 1].ptt[0].pgcn;
        pgn = p_vts->vts_ptt_srpt->title[p_sys->i_ttn - 1].ptt[0].pgn;
        p_pgc = p_vts->vts_pgcit->pgci_srp[pgc_id - 1].pgc;

        p_sys->i_title_start_cell =
            i_start_cell = p_pgc->program_map[pgn - 1] - 1;
        p_sys->i_title_start_block =
            p_pgc->cell_playback[i_start_cell].first_sector;

        p_sys->i_title_end_cell =
            i_end_cell = p_pgc->nr_of_cells - 1;
        p_sys->i_title_end_block =
            p_pgc->cell_playback[i_end_cell].last_sector;

        p_sys->i_title_offset = 0;

        p_sys->i_title_blocks = 0;
        for( i = i_start_cell; i <= i_end_cell; i++ )
        {
            p_sys->i_title_blocks += p_pgc->cell_playback[i].last_sector -
                p_pgc->cell_playback[i].first_sector + 1;
        }

        msg_Dbg( p_demux, "title %d vts_title %d pgc %d pgn %d "
                 "start %d end %d blocks: %d",
                 i_title + 1, p_sys->i_ttn, pgc_id, pgn,
                 p_sys->i_title_start_block, p_sys->i_title_end_block,
                 p_sys->i_title_blocks );

        /*
         * Set properties for current chapter
         */
        p_sys->i_chapter = 0;
        p_sys->i_chapters =
            p_vts->vts_ptt_srpt->title[p_sys->i_ttn - 1].nr_of_ptts;

        pgc_id = p_vts->vts_ptt_srpt->title[
                    p_sys->i_ttn - 1].ptt[p_sys->i_chapter].pgcn;
        pgn = p_vts->vts_ptt_srpt->title[
                    p_sys->i_ttn - 1].ptt[p_sys->i_chapter].pgn;

        p_pgc = p_vts->vts_pgcit->pgci_srp[pgc_id - 1].pgc;
        p_sys->i_pack_len = 0;
        p_sys->i_next_cell =
            p_sys->i_cur_cell = p_pgc->program_map[pgn - 1] - 1;
        DvdReadFindCell( p_demux );

        p_sys->i_next_vobu = p_sys->i_cur_block =
            p_pgc->cell_playback[p_sys->i_cur_cell].first_sector;

        /*
         * Angle management
         */
        p_sys->i_angles = p_vmg->tt_srpt->title[i_title].nr_of_angles;
        if( p_sys->i_angle > p_sys->i_angles ) p_sys->i_angle = 1;

        /*
         * We've got enough info, time to open the title set data.
         */
        if( !( p_sys->p_title = DVDOpenFile( p_sys->p_dvdread,
            p_vmg->tt_srpt->title[i_title].title_set_nr,
            DVD_READ_TITLE_VOBS ) ) )
        {
            msg_Err( p_demux, "cannot open title (VTS_%02d_1.VOB)",
                     p_vmg->tt_srpt->title[i_title].title_set_nr );
            return VLC_EGENERIC;
        }

        //IfoPrintTitle( p_demux );

        /*
         * Destroy obsolete ES by reinitializing program 0
         * and find all ES in title with ifo data
         */
        es_out_Control( p_demux->out, ES_OUT_RESET_PCR );

        for( i = 0; i < PS_TK_COUNT; i++ )
        {
            ps_track_t *tk = &p_sys->tk[i];
            if( tk->b_seen )
            {
                es_format_Clean( &tk->fmt );
                if( tk->es ) es_out_Del( p_demux->out, tk->es );
            }
            tk->b_seen = VLC_FALSE;
        }

        if( p_demux->info.i_title != i_title )
        {
            p_demux->info.i_update |=
                INPUT_UPDATE_TITLE | INPUT_UPDATE_SEEKPOINT;
            p_demux->info.i_title = i_title;
            p_demux->info.i_seekpoint = 0;
        }

        /* TODO: re-add angles */


        ESNew( p_demux, 0xe0, 0 ); /* Video, FIXME ? */
        p_sys->i_aspect = p_vts->vtsi_mat->vts_video_attr.display_aspect_ratio;

#define audio_control \
    p_sys->p_vts_file->vts_pgcit->pgci_srp[pgc_id-1].pgc->audio_control[i-1]

        /* Audio ES, in the order they appear in the .ifo */
        for( i = 1; i <= p_vts->vtsi_mat->nr_of_vts_audio_streams; i++ )
        {
            int i_position = 0;
            uint16_t i_id;

            //IfoPrintAudio( p_demux, i );

            /* Audio channel is active if first byte is 0x80 */
            if( audio_control & 0x8000 )
            {
                i_position = ( audio_control & 0x7F00 ) >> 8;

                msg_Dbg( p_demux, "audio position  %d", i_position );
                switch( p_vts->vtsi_mat->vts_audio_attr[i - 1].audio_format )
                {
                case 0x00: /* A52 */
                    i_id = (0x80 + i_position) | 0xbd00;
                    break;
                case 0x02:
                case 0x03: /* MPEG audio */
                    i_id = 0xc000 + i_position;
                    break;
                case 0x04: /* LPCM */
                    i_id = (0xa0 + i_position) | 0xbd00;
                    break;
                case 0x06: /* DTS */
                    i_id = (0x88 + i_position) | 0xbd00;
                    break;
                default:
                    i_id = 0;
                    msg_Err( p_demux, "unknown audio type %.2x",
                        p_vts->vtsi_mat->vts_audio_attr[i - 1].audio_format );
                }

                ESNew( p_demux, i_id, p_sys->p_vts_file->vtsi_mat->
                       vts_audio_attr[i - 1].lang_code );
            }
        }
#undef audio_control

#define spu_palette \
    p_sys->p_vts_file->vts_pgcit->pgci_srp[pgc_id-1].pgc->palette

        memcpy( p_sys->clut, spu_palette, 16 * sizeof( uint32_t ) );

#define spu_control \
    p_sys->p_vts_file->vts_pgcit->pgci_srp[pgc_id-1].pgc->subp_control[i-1]

        /* Sub Picture ES */
        for( i = 1; i <= p_vts->vtsi_mat->nr_of_vts_subp_streams; i++ )
        {
            int i_position = 0;
            uint16_t i_id;

            //IfoPrintSpu( p_sys, i );
            msg_Dbg( p_demux, "spu %d 0x%02x", i, spu_control );

            if( spu_control & 0x80000000 )
            {
                /*  there are several streams for one spu */
                if( p_vts->vtsi_mat->vts_video_attr.display_aspect_ratio )
                {
                    /* 16:9 */
                    switch( p_vts->vtsi_mat->vts_video_attr.permitted_df )
                    {
                    case 1: /* letterbox */
                        i_position = spu_control & 0xff;
                        break;
                    case 2: /* pan&scan */
                        i_position = ( spu_control >> 8 ) & 0xff;
                        break;
                    default: /* widescreen */
                        i_position = ( spu_control >> 16 ) & 0xff;
                        break;
                    }
                }
                else
                {
                    /* 4:3 */
                    i_position = ( spu_control >> 24 ) & 0x7F;
                }

                i_id = (0x20 + i_position) | 0xbd00;

                ESNew( p_demux, i_id, p_sys->p_vts_file->vtsi_mat->
                       vts_subp_attr[i - 1].lang_code );
            }
        }
#undef spu_control

    }
    else if( i_title != -1 && i_title != p_sys->i_title )

    {
        return VLC_EGENERIC; /* Couldn't set title */
    }

    /*
     * Chapter selection
     */

    if( i_chapter >= 0 && i_chapter < p_sys->i_chapters )
    {
        pgc_id = p_vts->vts_ptt_srpt->title[
                     p_sys->i_ttn - 1].ptt[i_chapter].pgcn;
        pgn = p_vts->vts_ptt_srpt->title[
                  p_sys->i_ttn - 1].ptt[i_chapter].pgn;

        p_pgc = p_vts->vts_pgcit->pgci_srp[pgc_id - 1].pgc;

        p_sys->i_cur_cell = p_pgc->program_map[pgn - 1] - 1;
        p_sys->i_chapter = i_chapter;
        DvdReadFindCell( p_demux );

        p_sys->i_title_offset = 0;
        for( i = p_sys->i_title_start_cell; i < p_sys->i_cur_cell; i++ )
        {
            p_sys->i_title_offset += p_pgc->cell_playback[i].last_sector -
                p_pgc->cell_playback[i].first_sector + 1;
        }

        p_sys->i_pack_len = 0;
        p_sys->i_next_vobu = p_sys->i_cur_block =
            p_pgc->cell_playback[p_sys->i_cur_cell].first_sector;

        if( p_demux->info.i_seekpoint != i_chapter )
        {
            p_demux->info.i_update |= INPUT_UPDATE_SEEKPOINT;
            p_demux->info.i_seekpoint = i_chapter;
        }
    }
    else if( i_chapter != -1 )

    {
        return VLC_EGENERIC; /* Couldn't set chapter */
    }

#undef p_pgc
#undef p_vts
#undef p_vmg

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DvdReadSeek : Goes to a given position on the stream.
 *****************************************************************************
 * This one is used by the input and translate chronological position from
 * input to logical position on the device.
 *****************************************************************************/
static void DvdReadSeek( demux_t *p_demux, int i_block_offset )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_chapter = 0;
    int i_cell = 0;
    int i_vobu = 0;
    int i_sub_cell = 0;
    int i_block;

#define p_pgc p_sys->p_cur_pgc
#define p_vts p_sys->p_vts_file

    /* Find cell */
    i_block = i_block_offset;
    for( i_cell = p_sys->i_title_start_cell;
         i_cell <= p_sys->i_title_end_cell; i_cell++ )
    {
        if( i_block < (int)p_pgc->cell_playback[i_cell].last_sector -
            (int)p_pgc->cell_playback[i_cell].first_sector + 1 ) break;

        i_block -= (p_pgc->cell_playback[i_cell].last_sector -
            p_pgc->cell_playback[i_cell].first_sector + 1);
    }
    if( i_cell > p_sys->i_title_end_cell )
    {
        msg_Err( p_demux, "couldn't find cell for block %i", i_block_offset );
        return;
    }
    i_block += p_pgc->cell_playback[i_cell].first_sector;
    p_sys->i_title_offset = i_block_offset;

    /* Find chapter */
    for( i_chapter = 0; i_chapter < p_sys->i_chapters; i_chapter++ )
    {
        int pgc_id, pgn, i_tmp;

        pgc_id = p_vts->vts_ptt_srpt->title[
                    p_sys->i_ttn - 1].ptt[i_chapter].pgcn;
        pgn = p_vts->vts_ptt_srpt->title[
                    p_sys->i_ttn - 1].ptt[i_chapter].pgn;

        i_tmp = p_vts->vts_pgcit->pgci_srp[pgc_id - 1].pgc->program_map[pgn-1];

        if( i_tmp > i_cell ) break;
    }

    if( i_chapter < p_sys->i_chapters &&
        p_demux->info.i_seekpoint != i_chapter )
    {
        p_demux->info.i_update |= INPUT_UPDATE_SEEKPOINT;
        p_demux->info.i_seekpoint = i_chapter;
    }

    /* Find vobu */
    while( (int)p_vts->vts_vobu_admap->vobu_start_sectors[i_vobu] <= i_block )
    {
        i_vobu++;
    }

    /* Find sub_cell */
    while( p_vts->vts_c_adt->cell_adr_table[i_sub_cell].start_sector <
           p_vts->vts_vobu_admap->vobu_start_sectors[i_vobu-1] )
    {
        i_sub_cell++;
    }

#if 1
    msg_Dbg( p_demux, "cell %d i_sub_cell %d chapter %d vobu %d "
             "cell_sector %d vobu_sector %d sub_cell_sector %d",
             i_cell, i_sub_cell, i_chapter, i_vobu,
             p_sys->p_cur_pgc->cell_playback[i_cell].first_sector,
             p_vts->vts_vobu_admap->vobu_start_sectors[i_vobu],
             p_vts->vts_c_adt->cell_adr_table[i_sub_cell - 1].start_sector);
#endif

    p_sys->i_cur_block = i_block;
    p_sys->i_next_vobu = p_vts->vts_vobu_admap->vobu_start_sectors[i_vobu];
    p_sys->i_pack_len = p_sys->i_next_vobu - i_block;
    p_sys->i_cur_cell = i_cell;
    p_sys->i_chapter = i_chapter;
    DvdReadFindCell( p_demux );

#undef p_vts
#undef p_pgc

    return;
}

/*****************************************************************************
 * DvdReadHandleDSI
 *****************************************************************************/
static void DvdReadHandleDSI( demux_t *p_demux, uint8_t *p_data )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    navRead_DSI( &p_sys->dsi_pack, &p_data[DSI_START_BYTE] );

    /*
     * Determine where we go next.  These values are the ones we mostly
     * care about.
     */
    p_sys->i_cur_block = p_sys->dsi_pack.dsi_gi.nv_pck_lbn;
    p_sys->i_pack_len = p_sys->dsi_pack.dsi_gi.vobu_ea;

    /*
     * Store the timecodes so we can get the current time
     */
    p_sys->i_title_cur_time = (mtime_t) p_sys->dsi_pack.dsi_gi.nv_pck_scr / 90 * 1000;
    p_sys->i_cell_cur_time = (mtime_t) dvdtime_to_time( &p_sys->dsi_pack.dsi_gi.c_eltm, 0 );

    /*
     * If we're not at the end of this cell, we can determine the next
     * VOBU to display using the VOBU_SRI information section of the
     * DSI.  Using this value correctly follows the current angle,
     * avoiding the doubled scenes in The Matrix, and makes our life
     * really happy.
     */

    p_sys->i_next_vobu = p_sys->i_cur_block +
        ( p_sys->dsi_pack.vobu_sri.next_vobu & 0x7fffffff );

    if( p_sys->dsi_pack.vobu_sri.next_vobu != SRI_END_OF_CELL
        && p_sys->i_angle > 1 )
    {
        switch( ( p_sys->dsi_pack.sml_pbi.category & 0xf000 ) >> 12 )
        {
        case 0x4:
            /* Interleaved unit with no angle */
            if( p_sys->dsi_pack.sml_pbi.ilvu_sa != 0 )
            {
                p_sys->i_next_vobu = p_sys->i_cur_block +
                    p_sys->dsi_pack.sml_pbi.ilvu_sa;
                p_sys->i_pack_len = p_sys->dsi_pack.sml_pbi.ilvu_ea;
            }
            else
            {
                p_sys->i_next_vobu = p_sys->i_cur_block +
                    p_sys->dsi_pack.dsi_gi.vobu_ea + 1;
            }
            break;
        case 0x5:
            /* vobu is end of ilvu */
            if( p_sys->dsi_pack.sml_agli.data[p_sys->i_angle-1].address )
            {
                p_sys->i_next_vobu = p_sys->i_cur_block +
                    p_sys->dsi_pack.sml_agli.data[p_sys->i_angle-1].address;
                p_sys->i_pack_len = p_sys->dsi_pack.sml_pbi.ilvu_ea;

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
            p_sys->i_next_vobu = p_sys->i_cur_block +
                ( p_sys->dsi_pack.vobu_sri.next_vobu & 0x7fffffff );
            break;
        }
    }
    else if( p_sys->dsi_pack.vobu_sri.next_vobu == SRI_END_OF_CELL )
    {
        p_sys->i_cur_cell = p_sys->i_next_cell;

        /* End of title */
        if( p_sys->i_cur_cell >= p_sys->p_cur_pgc->nr_of_cells ) return;

        DvdReadFindCell( p_demux );

        p_sys->i_next_vobu =
            p_sys->p_cur_pgc->cell_playback[p_sys->i_cur_cell].first_sector;

        p_sys->i_cell_duration = (mtime_t)dvdtime_to_time( &p_sys->p_cur_pgc->cell_playback[p_sys->i_cur_cell].playback_time, 0 );
    }


#if 0
    msg_Dbg( p_demux, "scr %d lbn 0x%02x vobu_ea %d vob_id %d c_id %d c_time %lld",
             p_sys->dsi_pack.dsi_gi.nv_pck_scr,
             p_sys->dsi_pack.dsi_gi.nv_pck_lbn,
             p_sys->dsi_pack.dsi_gi.vobu_ea,
             p_sys->dsi_pack.dsi_gi.vobu_vob_idn,
             p_sys->dsi_pack.dsi_gi.vobu_c_idn,
             dvdtime_to_time( &p_sys->dsi_pack.dsi_gi.c_eltm, 0 ) );

    msg_Dbg( p_demux, "cell duration: %lld", 
             (mtime_t)dvdtime_to_time( &p_sys->p_cur_pgc->cell_playback[p_sys->i_cur_cell].playback_time, 0 ) );

    msg_Dbg( p_demux, "cat 0x%02x ilvu_ea %d ilvu_sa %d size %d",
             p_sys->dsi_pack.sml_pbi.category,
             p_sys->dsi_pack.sml_pbi.ilvu_ea,
             p_sys->dsi_pack.sml_pbi.ilvu_sa,
             p_sys->dsi_pack.sml_pbi.size );

    msg_Dbg( p_demux, "next_vobu %d next_ilvu1 %d next_ilvu2 %d",
             p_sys->dsi_pack.vobu_sri.next_vobu & 0x7fffffff,
             p_sys->dsi_pack.sml_agli.data[ p_sys->i_angle - 1 ].address,
             p_sys->dsi_pack.sml_agli.data[ p_sys->i_angle ].address);
#endif
}

/*****************************************************************************
 * DvdReadFindCell
 *****************************************************************************/
static void DvdReadFindCell( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    pgc_t *p_pgc;
    int   pgc_id, pgn;
    int   i = 0;

#define cell p_sys->p_cur_pgc->cell_playback

    if( cell[p_sys->i_cur_cell].block_type == BLOCK_TYPE_ANGLE_BLOCK )
    {
        p_sys->i_cur_cell += p_sys->i_angle - 1;

        while( cell[p_sys->i_cur_cell+i].block_mode != BLOCK_MODE_LAST_CELL )
        {
            i++;
        }
        p_sys->i_next_cell = p_sys->i_cur_cell + i + 1;
    }
    else
    {
        p_sys->i_next_cell = p_sys->i_cur_cell + 1;
    }

#undef cell

    if( p_sys->i_chapter + 1 >= p_sys->i_chapters ) return;

    pgc_id = p_sys->p_vts_file->vts_ptt_srpt->title[
                p_sys->i_ttn - 1].ptt[p_sys->i_chapter + 1].pgcn;
    pgn = p_sys->p_vts_file->vts_ptt_srpt->title[
              p_sys->i_ttn - 1].ptt[p_sys->i_chapter + 1].pgn;
    p_pgc = p_sys->p_vts_file->vts_pgcit->pgci_srp[pgc_id - 1].pgc;

    if( p_sys->i_cur_cell >= p_pgc->program_map[pgn - 1] - 1 )
    {
        p_sys->i_chapter++;

        if( p_sys->i_chapter < p_sys->i_chapters &&
            p_demux->info.i_seekpoint != p_sys->i_chapter )
        {
            p_demux->info.i_update |= INPUT_UPDATE_SEEKPOINT;
            p_demux->info.i_seekpoint = p_sys->i_chapter;
        }
    }
}

/*****************************************************************************
 * DemuxTitles: get the titles/chapters structure
 *****************************************************************************/
static void DemuxTitles( demux_t *p_demux, int *pi_angle )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    input_title_t *t;
    seekpoint_t *s;
    int32_t i_titles;
    int i;

    /* Find out number of titles/chapters */
#define tt_srpt p_sys->p_vmg_file->tt_srpt

    i_titles = tt_srpt->nr_of_srpts;
    msg_Dbg( p_demux, "number of titles: %d", i_titles );

    for( i = 0; i < i_titles; i++ )
    {
        int32_t i_chapters = 0;
        int j;

        i_chapters = tt_srpt->title[i].nr_of_ptts;
        msg_Dbg( p_demux, "title %d has %d chapters", i, i_chapters );

        t = vlc_input_title_New();

        for( j = 0; j < __MAX( i_chapters, 1 ); j++ )
        {
            s = vlc_seekpoint_New();
            TAB_APPEND( t->i_seekpoint, t->seekpoint, s );
        }

        TAB_APPEND( p_sys->i_titles, p_sys->titles, t );
    }

#undef tt_srpt
}
