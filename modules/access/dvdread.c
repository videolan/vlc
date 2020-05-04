/*****************************************************************************
 * dvdread.c : DvdRead input module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2006 VLC authors and VideoLAN
 *
 * Authors: Stéphane Borel <stef@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * NOTA BENE: this module requires the linking against a library which is
 * known to require licensing under the GNU General Public License version 2
 * (or later). Therefore, the result of compiling this module will normally
 * be subject to the terms of that later license.
 *****************************************************************************/


/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_input.h>
#include <vlc_access.h>
#include <vlc_charset.h>
#include <vlc_interface.h>
#include <vlc_dialog.h>

#include <vlc_iso_lang.h>

#include "../demux/mpeg/pes.h"
#include "../demux/mpeg/ps.h"

#include <sys/types.h>
#include <unistd.h>

#include <dvdread/dvd_reader.h>
#include <dvdread/ifo_types.h>
#include <dvdread/ifo_read.h>
#include <dvdread/nav_read.h>
#include <dvdread/nav_print.h>

#include <assert.h>
#include <limits.h>

#include "disc_helper.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ANGLE_TEXT N_("DVD angle")
#define ANGLE_LONGTEXT N_( \
    "Default DVD angle." )

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin ()
    set_shortname( N_("DVD without menus") )
    set_description( N_("DVDRead Input (no menu support)") )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )
    add_integer( "dvdread-angle", 1, ANGLE_TEXT,
        ANGLE_LONGTEXT, false )
    add_obsolete_string( "dvdread-css-method" ) /* obsolete since 1.1.0 */
    set_capability( "access", 0 )
    add_shortcut( "dvd", "dvdread", "dvdsimple" )
    set_callbacks( Open, Close )
vlc_module_end ()

/* how many blocks DVDRead will read in each loop */
#define DVD_BLOCK_READ_ONCE 4

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

typedef struct
{
    /* DVDRead state */
    dvd_reader_t *p_dvdread;
    dvd_file_t   *p_title;

    ifo_handle_t *p_vmg_file;
    ifo_handle_t *p_vts_file;

    unsigned updates;
    int i_title;
    int cur_title;
    int i_chapter, i_chapters;
    int cur_chapter;
    int i_angle, i_angles;

    tt_srpt_t    *p_tt_srpt;
    pgc_t        *p_cur_pgc;
    dsi_t        dsi_pack;
    int          i_ttn;

    int i_pack_len;
    int i_cur_block;
    int i_next_vobu;

    int i_mux_rate;

    /* Current title start/end blocks */
    int i_title_start_block;
    int i_title_end_block;
    int i_title_blocks;
    int i_title_offset;

    int i_title_start_cell;
    int i_title_end_cell;
    int i_cur_cell;
    int i_next_cell;

    /* Track */
    ps_track_t    tk[PS_TK_COUNT];

    int           i_titles;
    input_title_t **titles;

    /* Video */
    int i_sar_num;
    int i_sar_den;

    /* SPU */
    uint32_t clut[16];
} demux_sys_t;

static int Control   ( demux_t *, int, va_list );
static int Demux     ( demux_t * );
static int DemuxBlock( demux_t *, const uint8_t *, int );

static void DemuxTitles( demux_t *, int * );
static void ESNew( demux_t *, int, int );

static int  DvdReadSetArea  ( demux_t *, int, int, int );
static int  DvdReadSeek     ( demux_t *, int );
static void DvdReadHandleDSI( demux_t *, uint8_t * );
static void DvdReadFindCell ( demux_t * );

#if DVDREAD_VERSION >= DVDREAD_VERSION_CODE(6, 1, 0)
static void DvdReadLog( void *foo, dvd_logger_level_t i, const char *p, va_list z )
{
    demux_t *p_demux = (demux_t*)foo;
    msg_GenericVa( p_demux, i, p, z );
}
#endif
/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    demux_t      *p_demux = (demux_t*)p_this;
    demux_sys_t  *p_sys;
    char         *psz_file;
    ifo_handle_t *p_vmg_file;

    if (p_demux->out == NULL)
        return VLC_EGENERIC;

    if( !p_demux->psz_filepath || !*p_demux->psz_filepath )
        psz_file = var_InheritString( p_this, "dvd" );
    else
        psz_file = strdup( p_demux->psz_filepath );

#if defined( _WIN32 ) || defined( __OS2__ )
    if( psz_file != NULL )
    {
        size_t flen = strlen( psz_file );
        if( flen > 0 && psz_file[flen - 1] == '\\' )
            psz_file[flen - 1] = '\0';
    }
    else
        psz_file = strdup("");
#endif
    if( unlikely(psz_file == NULL) )
        return VLC_EGENERIC;

    if( DiscProbeMacOSPermission( p_this, psz_file ) != VLC_SUCCESS )
    {
        free( psz_file );
        return VLC_EGENERIC;
    }

    /* Open dvdread */
    const char *psz_path = ToLocale( psz_file );
#if DVDREAD_VERSION >= DVDREAD_VERSION_CODE(6, 1, 0)
    dvd_logger_cb cbs;
    cbs.pf_log = DvdReadLog;
    dvd_reader_t *p_dvdread = DVDOpen2( p_demux, &cbs, psz_path );
#else
    dvd_reader_t *p_dvdread = DVDOpen( psz_path );
#endif
    LocaleFree( psz_path );
    if( p_dvdread == NULL )
    {
        msg_Err( p_demux, "DVDRead cannot open source: %s", psz_file );
        vlc_dialog_display_error( p_demux, _("Playback failure"),
                      _("DVDRead could not open the disc \"%s\"."), psz_file );

        free( psz_file );
        return VLC_EGENERIC;
    }
    free( psz_file );

    /* Ifo allocation & initialisation */
    if( !( p_vmg_file = ifoOpen( p_dvdread, 0 ) ) )
    {
        char rgsz_volid[32];
        if( DVDUDFVolumeInfo( p_dvdread, rgsz_volid, 32, NULL, 0 ) )
        {
            if( DVDISOVolumeInfo( p_dvdread, rgsz_volid, 32, NULL, 0 ) == 0 )
            {
                vlc_dialog_display_error( p_demux, _("Playback failure"),
                              _("Cannot play a non-UDF mastered DVD." ) );
                msg_Err( p_demux, "Invalid UDF DVD. (Found ISO9660 '%s')", rgsz_volid );
            }
        }
        msg_Warn( p_demux, "cannot open VMG info" );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_demux, "VMG opened" );

    /* Fill p_demux field */
    DEMUX_INIT_COMMON(); p_sys = p_demux->p_sys;

    ps_track_init( p_sys->tk );
    p_sys->i_sar_num = 0;
    p_sys->i_sar_den = 0;

    p_sys->p_dvdread = p_dvdread;
    p_sys->p_vmg_file = p_vmg_file;
    p_sys->p_title = NULL;
    p_sys->p_vts_file = NULL;

    p_sys->updates = 0;
    p_sys->i_title = p_sys->i_chapter = -1;
    p_sys->cur_title = p_sys->cur_chapter = 0;
    p_sys->i_mux_rate = 0;

    p_sys->i_angle = var_CreateGetInteger( p_demux, "dvdread-angle" );
    if( p_sys->i_angle <= 0 ) p_sys->i_angle = 1;

    DemuxTitles( p_demux, &p_sys->i_angle );
    if( DvdReadSetArea( p_demux, 0, 0, p_sys->i_angle ) != VLC_SUCCESS )
    {
        msg_Err( p_demux, "DvdReadSetArea(0,0,%i) failed (can't decrypt DVD?)",
                 p_sys->i_angle );
        Close( p_this );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    for( int i = 0; i < PS_TK_COUNT; i++ )
    {
        ps_track_t *tk = &p_sys->tk[i];
        if( tk->b_configured )
        {
            es_format_Clean( &tk->fmt );
            if( tk->es ) es_out_Del( p_demux->out, tk->es );
        }
    }

    /* Free the array of titles */
    for( int i = 0; i < p_sys->i_titles; i++ )
        vlc_input_title_Delete( p_sys->titles[i] );
    TAB_CLEAN( p_sys->i_titles, p_sys->titles );

    /* Close libdvdread */
    if( p_sys->p_title ) DVDCloseFile( p_sys->p_title );
    if( p_sys->p_vts_file ) ifoClose( p_sys->p_vts_file );
    if( p_sys->p_vmg_file ) ifoClose( p_sys->p_vmg_file );
    DVDClose( p_sys->p_dvdread );

    free( p_sys );
}

static vlc_tick_t dvdtime_to_time( dvd_time_t *dtime )
{
/* Macro to convert Binary Coded Decimal to Decimal */
#define BCD2D(__x__) (((__x__ & 0xf0) >> 4) * 10 + (__x__ & 0x0f))

    double f_fps, f_ms;

    int64_t sec = (int64_t)(BCD2D(dtime->hour)) * 60 * 60;
    sec += (int64_t)(BCD2D(dtime->minute)) * 60;
    sec += (int64_t)(BCD2D(dtime->second));

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
    return vlc_tick_from_sec(sec) + VLC_TICK_FROM_MS(f_ms);
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    double f, *pf;
    bool *pb;
    input_title_t ***ppp_title;
    int *pi_int;
    int i;

    if(unlikely(!p_sys->p_vts_file))
        return VLC_EGENERIC;

    switch( i_query )
    {
        case DEMUX_GET_POSITION:
        {
            pf = va_arg( args, double * );

            if( p_sys->i_title_blocks > 0 )
                *pf = (double)p_sys->i_title_offset / p_sys->i_title_blocks;
            else
                *pf = 0.0;

            return VLC_SUCCESS;
        }
        case DEMUX_SET_POSITION:
        {
            f = va_arg( args, double );

            return DvdReadSeek( p_demux, f * p_sys->i_title_blocks );
        }
        case DEMUX_GET_TIME:
            if( p_sys->cur_title >= 0 && p_sys->cur_title < p_sys->i_titles )
            {
                *va_arg( args, vlc_tick_t * ) = dvdtime_to_time( &p_sys->p_cur_pgc->playback_time ) /
                        p_sys->i_title_blocks * p_sys->i_title_offset;
                return VLC_SUCCESS;
            }
            *va_arg( args, vlc_tick_t * ) = 0;
            return VLC_EGENERIC;

        case DEMUX_GET_LENGTH:
            if( p_sys->cur_title >= 0 && p_sys->cur_title < p_sys->i_titles )
            {
                *va_arg( args, vlc_tick_t * ) = dvdtime_to_time( &p_sys->p_cur_pgc->playback_time );
                return VLC_SUCCESS;
            }
            *va_arg( args, vlc_tick_t * ) = 0;
            return VLC_EGENERIC;

        /* Special for access_demux */
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_SEEK:
        case DEMUX_CAN_CONTROL_PACE:
            /* TODO */
            pb = va_arg( args, bool * );
            *pb = true;
            return VLC_SUCCESS;

        case DEMUX_SET_PAUSE_STATE:
            return VLC_SUCCESS;

        case DEMUX_GET_TITLE_INFO:
            ppp_title = va_arg( args, input_title_t *** );
            pi_int    = va_arg( args, int * );
            *va_arg( args, int * ) = 1; /* Title offset */
            *va_arg( args, int * ) = 1; /* Chapter offset */

            /* Duplicate title infos */
            *pi_int = 0;
            *ppp_title = vlc_alloc( p_sys->i_titles, sizeof(input_title_t *) );
            if(!*ppp_title)
                return VLC_EGENERIC;
            for (i = 0; i < p_sys->i_titles; i++)
            {
                input_title_t *p_dup = vlc_input_title_Duplicate(p_sys->titles[i]);
                if(p_dup)
                    (*ppp_title)[(*pi_int)++] = p_dup;
            }
            return VLC_SUCCESS;

        case DEMUX_SET_TITLE:
            i = va_arg( args, int );
            if( DvdReadSetArea( p_demux, i, 0, -1 ) != VLC_SUCCESS )
            {
                msg_Warn( p_demux, "cannot set title/chapter" );
                return VLC_EGENERIC;
            }
            p_sys->updates |= INPUT_UPDATE_TITLE | INPUT_UPDATE_SEEKPOINT;
            p_sys->cur_title = i;
            p_sys->cur_chapter = 0;
            return VLC_SUCCESS;

        case DEMUX_SET_SEEKPOINT:
            i = va_arg( args, int );
            if( DvdReadSetArea( p_demux, -1, i, -1 ) != VLC_SUCCESS )
            {
                msg_Warn( p_demux, "cannot set title/chapter" );
                return VLC_EGENERIC;
            }
            p_sys->updates |= INPUT_UPDATE_SEEKPOINT;
            p_sys->cur_chapter = i;
            return VLC_SUCCESS;

        case DEMUX_TEST_AND_CLEAR_FLAGS:
        {
            unsigned *restrict flags = va_arg(args, unsigned *);
            *flags &= p_sys->updates;
            p_sys->updates &= ~*flags;
            return VLC_SUCCESS;
        }

        case DEMUX_GET_TITLE:
            *va_arg( args, int * ) = p_sys->cur_title;
            return VLC_SUCCESS;

        case DEMUX_GET_SEEKPOINT:
            *va_arg( args, int * ) = p_sys->cur_chapter;
            return VLC_SUCCESS;

        case DEMUX_GET_PTS_DELAY:
            *va_arg( args, vlc_tick_t * ) =
                VLC_TICK_FROM_MS(var_InheritInteger( p_demux, "disc-caching" ));
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

    if(unlikely(!p_sys->p_vts_file))
        return VLC_DEMUXER_EOF;

    uint8_t p_buffer[DVD_VIDEO_LB_LEN * DVD_BLOCK_READ_ONCE];
    int i_blocks_once, i_read;

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
            vlc_dialog_display_error( p_demux, _("Playback failure"),
                          _("DVDRead could not read block %d."),
                          p_sys->i_next_vobu );
            return -1;
        }

        /* Basic check to be sure we don't have a empty title
         * go to next title if so */
        //assert( p_buffer[41] == 0xbf && p_buffer[1027] == 0xbf );
        DemuxBlock( p_demux, p_buffer, DVD_VIDEO_LB_LEN );

        /* Parse the contained dsi packet */
        DvdReadHandleDSI( p_demux, p_buffer );

        /* End of title */
        if( p_sys->i_cur_cell >= p_sys->p_cur_pgc->nr_of_cells )
        {
            int k = p_sys->i_title;

            /* Looking for a not broken title */
            while( k < p_sys->i_titles && DvdReadSetArea( p_demux, ++k, 0, -1 ) != VLC_SUCCESS )
            {
                msg_Err(p_demux, "Failed next title, trying another: %i", k );
                if( k >= p_sys->i_titles )
                    return 0; // EOF
            }
        }

        if( p_sys->i_pack_len >= 1024 )
        {
            msg_Err( p_demux, "i_pack_len >= 1024 (%i). "
                     "This shouldn't happen!", p_sys->i_pack_len );
            return 0; /* EOF */
        }

        p_sys->i_cur_block++;
        p_sys->i_title_offset++;
    }

    if( p_sys->i_cur_cell >= p_sys->p_cur_pgc->nr_of_cells )
    {
        int k = p_sys->i_title;

        /* Looking for a not broken title */
        while( k < p_sys->i_titles && DvdReadSetArea( p_demux, ++k, 0, -1 ) != VLC_SUCCESS )
        {
            msg_Err(p_demux, "Failed next title, trying another: %i", k );
            if( k >= p_sys->i_titles )
                return 0; // EOF
        }
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
        vlc_dialog_display_error( p_demux, _("Playback failure"),
                        _("DVDRead could not read %d/%d blocks at 0x%02x."),
                        i_read, i_blocks_once, p_sys->i_cur_block );
        return -1;
    }

    p_sys->i_cur_block += i_read;
    p_sys->i_title_offset += i_read;

#if 0
    msg_Dbg( p_demux, "i_blocks: %d len: %d current: 0x%02x",
             i_read, p_sys->i_pack_len, p_sys->i_cur_block );
#endif

    for( int i = 0; i < i_read; i++ )
    {
        DemuxBlock( p_demux, p_buffer + i * DVD_VIDEO_LB_LEN,
                    DVD_VIDEO_LB_LEN );
    }

    return 1;
}

/*****************************************************************************
 * DemuxBlock: demux a given block
 *****************************************************************************/
static int DemuxBlock( demux_t *p_demux, const uint8_t *p, int len )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    while( len > 0 )
    {
        int i_size = ps_pkt_size( p, len );
        if( i_size <= 0 || i_size > len )
        {
            break;
        }

        /* Create a block */
        block_t *p_pkt = block_Alloc( i_size );
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
            vlc_tick_t i_scr;
            int i_mux_rate;
            if( !ps_pkt_parse_pack( p_pkt->p_buffer, p_pkt->i_buffer,
                                    &i_scr, &i_mux_rate ) )
            {
                es_out_SetPCR( p_demux->out, VLC_TICK_0 + i_scr );
                if( i_mux_rate > 0 ) p_sys->i_mux_rate = i_mux_rate;
            }
            block_Release( p_pkt );
            break;
        }
        default:
        {
            int i_id = ps_pkt_id( p_pkt->p_buffer, p_pkt->i_buffer );
            if( i_id >= 0xc0 )
            {
                ps_track_t *tk = &p_sys->tk[ps_id_to_tk(i_id)];

                if( !tk->b_configured )
                {
                    ESNew( p_demux, i_id, 0 );
                }
                if( tk->es &&
                    !ps_pkt_parse_pes( VLC_OBJECT(p_demux), p_pkt, tk->i_skip ) )
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
        len -= i_size;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * ESNew: register a new elementary stream
 *****************************************************************************/
static void ESNew( demux_t *p_demux, int i_id, int i_lang )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    ps_track_t  *tk = &p_sys->tk[ps_id_to_tk(i_id)];
    char psz_language[3];

    if( tk->b_configured ) return;

    if( ps_track_fill( tk, 0, i_id, NULL, 0, true ) )
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
        tk->fmt.video.i_sar_num = p_sys->i_sar_num;
        tk->fmt.video.i_sar_den = p_sys->i_sar_den;
    }
    else if( tk->fmt.i_cat == AUDIO_ES )
    {
#if 0
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
#endif

        if( psz_language[0] ) tk->fmt.psz_language = strdup( psz_language );
    }
    else if( tk->fmt.i_cat == SPU_ES )
    {
        /* Palette */
        tk->fmt.subs.spu.palette[0] = SPU_PALETTE_DEFINED;
        memcpy( &tk->fmt.subs.spu.palette[1], p_sys->clut,
                16 * sizeof( uint32_t ) );

        if( psz_language[0] ) tk->fmt.psz_language = strdup( psz_language );
    }

    tk->es = es_out_Add( p_demux->out, &tk->fmt );
    tk->b_configured = true;
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
    VLC_UNUSED( i_angle );

    demux_sys_t *p_sys = p_demux->p_sys;
    int pgc_id = 0, pgn = 0;

    if( i_title >= 0 && i_title < p_sys->i_titles &&
        i_title != p_sys->i_title )
    {
        int i_start_cell, i_end_cell;

        if( p_sys->p_title != NULL )
        {
            DVDCloseFile( p_sys->p_title );
            p_sys->p_title = NULL;
        }
        p_sys->i_title = i_title;
        const ifo_handle_t *p_vmg = p_sys->p_vmg_file;

        /*
         *  We have to load all title information
         */
        msg_Dbg( p_demux, "open VTS %d, for title %d",
                 p_vmg->tt_srpt->title[i_title].title_set_nr, i_title + 1 );

        /* Ifo vts */
        if( p_sys->p_vts_file != NULL )
            ifoClose( p_sys->p_vts_file );
        if( !( p_sys->p_vts_file = ifoOpen( p_sys->p_dvdread,
               p_vmg->tt_srpt->title[i_title].title_set_nr ) ) )
        {
            msg_Err( p_demux, "fatal error in vts ifo" );
            return VLC_EGENERIC;
        }

        const ifo_handle_t *p_vts = p_sys->p_vts_file;

        /* Title position inside the selected vts */
        p_sys->i_ttn = p_vmg->tt_srpt->title[i_title].vts_ttn;

        /* Find title start/end */
        pgc_id = p_vts->vts_ptt_srpt->title[p_sys->i_ttn - 1].ptt[0].pgcn;
        pgn = p_vts->vts_ptt_srpt->title[p_sys->i_ttn - 1].ptt[0].pgn;
        const pgc_t *p_pgc =
            p_sys->p_cur_pgc = p_vts->vts_pgcit->pgci_srp[pgc_id - 1].pgc;

        if( p_pgc->cell_playback == NULL )
        {
            msg_Err( p_demux, "Invalid PGC (cell_playback_offset)" );
            return VLC_EGENERIC;
        }

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
        for( int i = i_start_cell; i <= i_end_cell; i++ )
        {
            const uint32_t cell_blocks = p_pgc->cell_playback[i].last_sector -
                                         p_pgc->cell_playback[i].first_sector + 1;
            if(unlikely( cell_blocks == 0 || cell_blocks > INT_MAX ||
                 INT_MAX - p_sys->i_title_blocks < (int)cell_blocks ))
                return VLC_EGENERIC;
            p_sys->i_title_blocks += cell_blocks;
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

        for( int i = 0; i < PS_TK_COUNT; i++ )
        {
            ps_track_t *tk = &p_sys->tk[i];
            if( tk->b_configured )
            {
                es_format_Clean( &tk->fmt );
                if( tk->es ) es_out_Del( p_demux->out, tk->es );
            }
            tk->b_configured = false;
        }

        if( p_sys->cur_title != i_title )
        {
            p_sys->updates |= INPUT_UPDATE_TITLE | INPUT_UPDATE_SEEKPOINT;
            p_sys->cur_title = i_title;
            p_sys->cur_chapter = 0;
        }

        /* TODO: re-add angles */


        ESNew( p_demux, 0xe0, 0 ); /* Video, FIXME ? */
        const video_attr_t *p_attr = &p_vts->vtsi_mat->vts_video_attr;
        int i_video_height = p_attr->video_format != 0 ? 576 : 480;
        int i_video_width;
        switch( p_attr->picture_size )
        {
        case 0:
            i_video_width = 720;
            break;
        case 1:
            i_video_width = 704;
            break;
        case 2:
            i_video_width = 352;
            break;
        default:
        case 3:
            i_video_width = 352;
            i_video_height /= 2;
            break;
        }
        switch( p_attr->display_aspect_ratio )
        {
        case 0:
            p_sys->i_sar_num = 4 * i_video_height;
            p_sys->i_sar_den = 3 * i_video_width;
            break;
        case 3:
            p_sys->i_sar_num = 16 * i_video_height;
            p_sys->i_sar_den =  9 * i_video_width;
            break;
        default:
            p_sys->i_sar_num = 0;
            p_sys->i_sar_den = 0;
            break;
        }

        /* Audio ES, in the order they appear in the .ifo */
        for( int i = 1; i <= p_vts->vtsi_mat->nr_of_vts_audio_streams; i++ )
        {
            int i_position = 0;
            uint16_t i_id;

            //IfoPrintAudio( p_demux, i );

            /* Audio channel is active if first byte is 0x80 */
            uint16_t i_audio_control = p_pgc->audio_control[i-1];
            if( i_audio_control & 0x8000 )
            {
                i_position = ( i_audio_control & 0x7F00 ) >> 8;

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

        memcpy( p_sys->clut, p_pgc->palette, 16 * sizeof( uint32_t ) );

        /* Sub Picture ES */
        for( int i = 1; i <= p_vts->vtsi_mat->nr_of_vts_subp_streams; i++ )
        {
            int i_position = 0;
            uint16_t i_id;
            uint32_t i_spu_control = p_pgc->subp_control[i-1];

            //IfoPrintSpu( p_sys, i );
            msg_Dbg( p_demux, "spu %d 0x%02x", i, i_spu_control );

            if( i_spu_control & 0x80000000 )
            {
                /*  there are several streams for one spu */
                if( p_vts->vtsi_mat->vts_video_attr.display_aspect_ratio )
                {
                    /* 16:9 */
                    switch( p_vts->vtsi_mat->vts_video_attr.permitted_df )
                    {
                    case 1: /* letterbox */
                        i_position = i_spu_control & 0xff;
                        break;
                    case 2: /* pan&scan */
                        i_position = ( i_spu_control >> 8 ) & 0xff;
                        break;
                    default: /* widescreen */
                        i_position = ( i_spu_control >> 16 ) & 0xff;
                        break;
                    }
                }
                else
                {
                    /* 4:3 */
                    i_position = ( i_spu_control >> 24 ) & 0x7F;
                }

                i_id = (0x20 + i_position) | 0xbd00;

                ESNew( p_demux, i_id, p_sys->p_vts_file->vtsi_mat->
                       vts_subp_attr[i - 1].lang_code );
            }
        }

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
        const ifo_handle_t *p_vts = p_sys->p_vts_file;
        pgc_id = p_vts->vts_ptt_srpt->title[
                     p_sys->i_ttn - 1].ptt[i_chapter].pgcn;
        pgn = p_vts->vts_ptt_srpt->title[
                  p_sys->i_ttn - 1].ptt[i_chapter].pgn;

        const pgc_t *p_pgc =
            p_sys->p_cur_pgc = p_vts->vts_pgcit->pgci_srp[pgc_id - 1].pgc;
        if( p_pgc->cell_playback == NULL )
            return VLC_EGENERIC; /* Couldn't set chapter */

        p_sys->i_cur_cell = p_pgc->program_map[pgn - 1] - 1;
        p_sys->i_chapter = i_chapter;
        DvdReadFindCell( p_demux );

        p_sys->i_title_offset = 0;
        for( int i = p_sys->i_title_start_cell; i < p_sys->i_cur_cell; i++ )
        {
            p_sys->i_title_offset += p_pgc->cell_playback[i].last_sector -
                p_pgc->cell_playback[i].first_sector + 1;
        }

        p_sys->i_pack_len = 0;
        p_sys->i_next_vobu = p_sys->i_cur_block =
            p_pgc->cell_playback[p_sys->i_cur_cell].first_sector;

        if( p_sys->cur_chapter != i_chapter )
        {
            p_sys->updates |= INPUT_UPDATE_SEEKPOINT;
            p_sys->cur_chapter = i_chapter;
        }
    }
    else if( i_chapter != -1 )

    {
        return VLC_EGENERIC; /* Couldn't set chapter */
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DvdReadSeek : Goes to a given position on the stream.
 *****************************************************************************
 * This one is used by the input and translate chronological position from
 * input to logical position on the device.
 *****************************************************************************/
static int DvdReadSeek( demux_t *p_demux, int i_block_offset )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_chapter = 0;
    int i_cell = 0;
    int i_block;
    const pgc_t *p_pgc = p_sys->p_cur_pgc;
    const ifo_handle_t *p_vts = p_sys->p_vts_file;

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
        return VLC_EGENERIC;
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
        p_sys->cur_chapter != i_chapter )
    {
        p_sys->updates |= INPUT_UPDATE_SEEKPOINT;
        p_sys->cur_chapter = i_chapter;
    }

    /* Find vobu */
    /* see ifo_read.c / ifoRead_VOBU_ADMAP_internal for index count */
    int i_vobu = 1;
    const size_t i_vobu_sect_index_count =
            (p_vts->vts_vobu_admap->last_byte + 1 - VOBU_ADMAP_SIZE) / sizeof(uint32_t);
    for( size_t i=0; i<i_vobu_sect_index_count; i++ )
    {
        if( p_vts->vts_vobu_admap->vobu_start_sectors[i] > (uint32_t) i_block )
            break;
        i_vobu = i + 1;
    }

#if 1
    int i_sub_cell = 1;
    /* Find sub_cell */
    /* need to check cell # <= vob count as cell table alloc only ensures:
     * info_length / sizeof(cell_adr_t) < c_adt->nr_of_vobs, see ifo_read.c */
    const uint32_t vobu_start_sector = p_vts->vts_vobu_admap->vobu_start_sectors[i_vobu-1];
    for( int i = 0; i + 1<p_vts->vts_c_adt->nr_of_vobs; i++ )
    {
        const cell_adr_t *p_cell = &p_vts->vts_c_adt->cell_adr_table[i];
        if(p_cell->start_sector <= vobu_start_sector)
           i_sub_cell = i + 1;
    }

    msg_Dbg( p_demux, "cell %d i_sub_cell %d chapter %d vobu %d "
             "cell_sector %d vobu_sector %d sub_cell_sector %d",
             i_cell, i_sub_cell, i_chapter, i_vobu,
             p_sys->p_cur_pgc->cell_playback[i_cell].first_sector,
             p_vts->vts_vobu_admap->vobu_start_sectors[i_vobu],
             p_vts->vts_c_adt->cell_adr_table[i_sub_cell - 1].start_sector);
#endif

    p_sys->i_cur_block = i_block;
    if(likely( (size_t)i_vobu < i_vobu_sect_index_count ))
        p_sys->i_next_vobu = p_vts->vts_vobu_admap->vobu_start_sectors[i_vobu];
    else
        p_sys->i_next_vobu = i_block;
    p_sys->i_pack_len = p_sys->i_next_vobu - i_block;
    p_sys->i_cur_cell = i_cell;
    p_sys->i_chapter = i_chapter;
    DvdReadFindCell( p_demux );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * DvdReadHandleDSI
 *****************************************************************************/
static void DvdReadHandleDSI( demux_t *p_demux, uint8_t *p_data )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    /* Check we are really on a DSI packet
     * http://www.mpucoder.com/DVD/dsi_pkt.html
     * Some think it's funny to fill with 0x42 */
    const uint8_t dsiheader[7] = { 0x00, 0x00, 0x01, 0xbf, 0x03, 0xfa, 0x01 };
    if(!memcmp(&p_data[DSI_START_BYTE-7], dsiheader, 7))
    {
        navRead_DSI( &p_sys->dsi_pack, &p_data[DSI_START_BYTE] );

        /*
         * Determine where we go next.  These values are the ones we mostly
        * care about.
        */
        p_sys->i_cur_block = p_sys->dsi_pack.dsi_gi.nv_pck_lbn;
        p_sys->i_pack_len = p_sys->dsi_pack.dsi_gi.vobu_ea;

        /*
        * If we're not at the end of this cell, we can determine the next
        * VOBU to display using the VOBU_SRI information section of the
        * DSI.  Using this value correctly follows the current angle,
        * avoiding the doubled scenes in The Matrix, and makes our life
        * really happy.
        */

        p_sys->i_next_vobu = p_sys->i_cur_block +
            ( p_sys->dsi_pack.vobu_sri.next_vobu & 0x7fffffff );
    }
    else
    {
        /* resync after decoy/corrupted titles */
        msg_Warn(p_demux, "Invalid DSI packet in VOBU %d found, skipping Cell %d / %d",
                 p_sys->i_next_vobu, p_sys->i_cur_cell, p_sys->i_title_end_cell);
        p_sys->dsi_pack.vobu_sri.next_vobu = SRI_END_OF_CELL;
    }

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
            /* fall through */
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
    }


#if 0
    msg_Dbg( p_demux, "scr %d lbn 0x%02x vobu_ea %d vob_id %d c_id %d c_time %lld",
             p_sys->dsi_pack.dsi_gi.nv_pck_scr,
             p_sys->dsi_pack.dsi_gi.nv_pck_lbn,
             p_sys->dsi_pack.dsi_gi.vobu_ea,
             p_sys->dsi_pack.dsi_gi.vobu_vob_idn,
             p_sys->dsi_pack.dsi_gi.vobu_c_idn,
             dvdtime_to_time( &p_sys->dsi_pack.dsi_gi.c_eltm ) );

    msg_Dbg( p_demux, "cell duration: %lld",
             dvdtime_to_time( &p_sys->p_cur_pgc->cell_playback[p_sys->i_cur_cell].playback_time ) );

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

    const pgc_t *p_pgc;
    int   pgc_id, pgn;
    int   i = 0;

    const cell_playback_t *cell = p_sys->p_cur_pgc->cell_playback;
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
            p_sys->cur_chapter != p_sys->i_chapter )
        {
            p_sys->updates |= INPUT_UPDATE_SEEKPOINT;
            p_sys->cur_chapter = p_sys->i_chapter;
        }
    }
}

/*****************************************************************************
 * DemuxTitles: get the titles/chapters structure
 *****************************************************************************/
static void DemuxTitles( demux_t *p_demux, int *pi_angle )
{
    VLC_UNUSED( pi_angle );

    demux_sys_t *p_sys = p_demux->p_sys;
    input_title_t *t;
    seekpoint_t *s;

    /* Find out number of titles/chapters */
    const tt_srpt_t *tt_srpt = p_sys->p_vmg_file->tt_srpt;

    int32_t i_titles = tt_srpt->nr_of_srpts;
    msg_Dbg( p_demux, "number of titles: %d", i_titles );

    for( int i = 0; i < i_titles; i++ )
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
}
