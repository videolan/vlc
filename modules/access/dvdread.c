/*****************************************************************************
 * dvdread.c : DvdRead input module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2025 VLC authors and VideoLAN
 *
 * Authors: Stéphane Borel <stef@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Saifelden Mohamed Ismail <saifeldenmi@gmail.com>
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

#include "dvdread.h"

#include <vlc_dialog.h>
#include <vlc_iso_lang.h>
#include <limits.h>
#include <vlc_plugin.h>
#include <vlc_arrays.h>
#include <vlc_input.h>
#include "disc_helper.h"

extern const dvdread_ops_t DvdReadVideoOps;
extern void DvdReadHandleDSI( demux_t *, uint8_t * );
void DvdReadESNew( demux_t *, int, int );

#ifdef DVDREAD_HAS_DVDVIDEORECORDING
extern const dvdread_ops_t DvdVRReadOps;
#endif
#ifdef DVDREAD_HAS_DVDAUDIO
extern const dvdread_ops_t DvdAudioReadOps;
#endif

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
    set_subcategory( SUBCAT_INPUT_ACCESS )
    add_integer( "dvdread-angle", 1, ANGLE_TEXT,
        ANGLE_LONGTEXT )
    set_capability( "access", 0 )
    add_shortcut( "dvd", "dvdread", "dvdsimple" )
    set_callbacks( Open, Close )
    add_submodule()
        set_capability( "access", 1 )
        add_shortcut( "dvda" )
        set_callbacks( OpenAudio, Close )
    add_submodule()
        set_capability( "access", 1 )
        add_shortcut( "dvdvr" )
        set_callbacks( OpenVideoRecording, Close )
vlc_module_end ()

/* how many blocks DVDRead will read in each loop */
#define DVD_BLOCK_READ_ONCE 4

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static int Control   ( demux_t *, int, va_list );
static int Demux     ( demux_t * );
static int DemuxBlock( demux_t *, const uint8_t *, int );

static void DemuxTitles( demux_t *, int * );
static void DvdReadSetOps( demux_sys_t *, const dvdread_ops_t * );

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
int OpenCommonDvdread( vlc_object_t *p_this , dvd_type_t type,
                       const dvdread_ops_t *video_ops )
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

#ifdef __APPLE__
    if( DiscProbeMacOSPermission( p_this, psz_file ) != VLC_SUCCESS )
    {
        free( psz_file );
        return VLC_EGENERIC;
    }
#endif

    /* Open dvdread */
#if DVDREAD_VERSION < DVDREAD_VERSION_CODE(6, 1, 2)
    /* In libdvdread prior to 6.1.2, UTF8 is not supported for windows and
     * requires a prior conversion.
     * For non win32/os2 platforms, this is just a no-op */
    const char *psz_path = ToLocale( psz_file );
#else
    const char *psz_path = psz_file;
#endif
#if DVDREAD_VERSION >= DVDREAD_VERSION_CODE(6, 1, 0)
    dvd_logger_cb cbs = { .pf_log = DvdReadLog };
#endif
    dvd_reader_t *p_dvdread;
#ifdef DVDREAD_HAS_DVDAUDIO
    switch (type) {
        case DVD_A:
            p_dvdread = DVDOpenAudio( p_demux, &cbs, psz_path );
            break;
#ifndef DVDREAD_HAS_DVDVIDEORECORDING
        case DVD_VR:
            msg_Err( p_demux, "Version of libdvdread does not support DVD-VideoRecording" );
            free( psz_file );
            return VLC_EGENERIC;
#endif
        default:
            p_dvdread = DVDOpen2( p_demux, &cbs, psz_path );
            break;
    }
#elif DVDREAD_VERSION >= DVDREAD_VERSION_CODE(6, 1, 0)
    if ( type != DVD_V )
    {
        msg_Err( p_demux, "Version of libdvdread does not support %s",
                 type == DVD_A ? "DVD-Audio" : "DVD-VideoRecording" );
        free( psz_file );
        return VLC_EGENERIC;
    }
    p_dvdread = DVDOpen2( p_demux, &cbs, psz_path );
#else
    if ( type != DVD_V )
    {
        msg_Err( p_demux, "Version of libdvdread does not support %s",
                 type == DVD_A ? "DVD-Audio" : "DVD-VideoRecording" );
        free( psz_file );
        return VLC_EGENERIC;
    }
    p_dvdread = DVDOpen( psz_path );
#endif
#if DVDREAD_VERSION < DVDREAD_VERSION_CODE(6, 1, 2)
    LocaleFree( psz_path );
#endif
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
        msg_Warn( p_demux, "cannot open %s info", ( type == DVD_V ? "VMG" : type == DVD_VR ? "RTAV_VMGI" : "AMG" ) );
        DVDClose( p_dvdread );
        return VLC_EGENERIC;
    }
    msg_Dbg( p_demux, "%s opened", ( type == DVD_V ? "VMG" : type == DVD_VR ? "RTAV_VMGI" : "AMG" ) );

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

    /* reset anchors so the first cell seen after open captures fresh timestamps */
    DvdReadResetCellTs( p_sys );

    p_sys->i_angle = var_CreateGetInteger( p_demux, "dvdread-angle" );
    if( p_sys->i_angle <= 0 ) p_sys->i_angle = 1;

    /* store type state internally */
    /* if open2 is called, in 7.1.0 dvd may force the type */
#if defined(DVDREAD_HAS_DVDAUDIO)
    switch( p_sys->p_vmg_file->ifo_format )
    {
#ifdef DVDREAD_HAS_DVDVIDEORECORDING
        case IFO_VIDEO_RECORDING:
            p_sys->type = DVD_VR;
            p_sys->ud_pgcit = p_sys->p_vmg_file->ud_pgcit;
            p_sys->pgc_gi = p_sys->p_vmg_file->pgc_gi;
            break;
#endif
        case IFO_AUDIO:
            p_sys->type = DVD_A;
            break;
        default:
            p_sys->type = type;
            break;
    }
#else
    p_sys->type = type;
#endif
    DvdReadSetOps( p_sys, video_ops );
    if( unlikely( p_sys->ops == NULL ) )
    {
        msg_Err( p_demux, "internal error: dvdread ops not initialized" );
        Close( p_this );
        return VLC_EGENERIC;
    }

    DemuxTitles( p_demux, &p_sys->i_angle );
    if( p_sys->ops->set_area( p_demux, 0, 0, p_sys->i_angle ) != VLC_SUCCESS )
    {
        msg_Err( p_demux, "DvdReadSetArea(0,0,%i) failed (can't decrypt DVD?)",
                 p_sys->i_angle );
        Close( p_this );
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/*Call Backs*/
static int Open( vlc_object_t *p_this )
{
    if( OpenCommonDvdread( p_this, DVD_V, &DvdReadVideoOps ) != VLC_SUCCESS )
    {
        msg_Dbg( p_this, "Trying DVD-Video Recording as a fallback" );
        if( OpenCommonDvdread( p_this, DVD_VR, NULL ) == VLC_SUCCESS )
            return VLC_SUCCESS;

        msg_Dbg( p_this, "Trying DVD-Audio as a fallback" );
        return OpenCommonDvdread( p_this, DVD_A, NULL );
    }
    return VLC_SUCCESS;
}

static void DvdReadSetOps( demux_sys_t *p_sys, const dvdread_ops_t *video_ops )
{
    p_sys->ops = video_ops;

#ifdef DVDREAD_HAS_DVDVIDEORECORDING
    if( p_sys->type == DVD_VR )
    {
        p_sys->ops = &DvdVRReadOps;
        return;
    }
#endif

#ifdef DVDREAD_HAS_DVDAUDIO
    if( p_sys->type == DVD_A )
    {
        p_sys->ops = &DvdAudioReadOps;
        return;
    }
#endif

    if( p_sys->type != DVD_V )
        p_sys->ops = NULL;
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

static bool DvdReadCellTsShift( const demux_sys_t *p_sys, vlc_tick_t *shift )
{
    if( p_sys->cell_ts.dvd == VLC_TICK_INVALID ||
        p_sys->cell_ts.ps == VLC_TICK_INVALID )
        return false;
    *shift = p_sys->cell_ts.dvd - p_sys->cell_ts.ps;
    return true;
}

static bool DvdReadGetTimelineBase( const demux_sys_t *p_sys, vlc_tick_t *base )
{
    return p_sys->ops->timeline_base( p_sys, base );
}

static vlc_tick_t DvdReadGetControlLength( const demux_sys_t *p_sys )
{
    return p_sys->ops->title_length( p_sys );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    if( unlikely( p_sys->ops == NULL ) )
        return VLC_EGENERIC;
    double f, *pf;
    bool *pb;
    input_title_t ***ppp_title;
    int *pi_int;
    int i;

    /* dvd-vr does not have vts */
    if( unlikely( !p_sys->p_vts_file && p_sys->ops->requires_vts ) )
        return VLC_EGENERIC;

    switch( i_query )
    {
        case DEMUX_GET_POSITION:
        {
            pf = va_arg( args, double * );

            /* vr i_title_offset is vobu count, block ratio wrong */
            if( p_sys->type == DVD_VR &&
                p_sys->cur_title >= 0 && p_sys->cur_title < p_sys->i_titles )
            {
                const vlc_tick_t length = p_sys->titles[p_sys->cur_title]->i_length;
                vlc_tick_t time = 0;
                if( length > 0 && DvdReadGetTimelineBase( p_sys, &time ) )
                {
                    if( time < 0 ) time = 0;
                    if( time > length ) time = length;
                    *pf = (double)time / length;
                    return VLC_SUCCESS;
                }
            }
            if( p_sys->i_title_blocks )
                *pf = (double)p_sys->i_title_offset / p_sys->i_title_blocks;
            else
                *pf = 0.0;

            return VLC_SUCCESS;
        }
        case DEMUX_SET_POSITION:
        {
            f = va_arg( args, double );
            return p_sys->ops->seek( p_demux, f * p_sys->i_title_blocks );
        }
        case DEMUX_GET_TIME:
            if( p_sys->cur_title >= 0 && p_sys->cur_title < p_sys->i_titles )
            {
                if( unlikely( p_sys->i_title_blocks == 0 ) )
                {
                    *va_arg( args, vlc_tick_t * ) = 0;
                    return VLC_EGENERIC;
                }
                const vlc_tick_t length = DvdReadGetControlLength( p_sys );

                vlc_tick_t time = 0;
                if( p_sys->type == DVD_VR &&
                    length > 0 && DvdReadGetTimelineBase( p_sys, &time ) )
                {
                    if( time < 0 ) time = 0;
                    if( time > length ) time = length;
                    *va_arg( args, vlc_tick_t * ) = time;
                    return VLC_SUCCESS;
                }
                *va_arg( args, vlc_tick_t * ) = p_sys->i_title_offset * length
                    / p_sys->i_title_blocks;
                return VLC_SUCCESS;
            }
            *va_arg( args, vlc_tick_t * ) = 0;
            return VLC_EGENERIC;

        case DEMUX_SET_TIME:
        {
            const vlc_tick_t i_time = va_arg( args, vlc_tick_t );
            if( p_sys->cur_title < 0 || p_sys->cur_title >= p_sys->i_titles ||
                p_sys->i_title_blocks == 0 )
                return VLC_EGENERIC;

            const vlc_tick_t length = DvdReadGetControlLength( p_sys );

            if( length <= 0 )
                return VLC_EGENERIC;

            vlc_tick_t t = i_time;
            if( t < 0 ) t = 0;
            if( t > length ) t = length;

            const uint32_t i_block_offset =
                (uint32_t)( t * p_sys->i_title_blocks / length );

            const uint32_t i_seek_offset =
                p_sys->ops->time_to_seek_offset( p_sys, t, i_block_offset );
            return p_sys->ops->seek( p_demux, i_seek_offset );
        }

        case DEMUX_GET_LENGTH:
            if( p_sys->cur_title >= 0 && p_sys->cur_title < p_sys->i_titles )
            {
                const vlc_tick_t length = DvdReadGetControlLength( p_sys );

                *va_arg( args, vlc_tick_t * ) = length;
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

            if( p_sys->ops->set_area( p_demux, i, 0, -1 ) != VLC_SUCCESS )
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
            if( p_sys->ops->set_area( p_demux, -1, i, -1 ) != VLC_SUCCESS )
            {
                msg_Warn( p_demux, "cannot set title/chapter" );
                return VLC_EGENERIC;
            }
            p_sys->updates |= INPUT_UPDATE_SEEKPOINT;
            p_sys->cur_chapter = i;
            msg_Dbg( p_demux,"Cur_chapter is: %d",p_sys->cur_chapter );
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

        case DEMUX_GET_TYPE:
            *va_arg( args, int* ) = ITEM_TYPE_DISC;
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
    if( unlikely( p_sys->ops == NULL ) )
        return 0;
    dvd_type_t type = p_sys->type;

    if( unlikely( !p_sys->p_vts_file && p_sys->ops->requires_vts ) )
        return VLC_DEMUXER_EOF;

    uint8_t p_buffer[DVD_VIDEO_LB_LEN * DVD_BLOCK_READ_ONCE];
    int i_blocks_once, i_read;

    /* type-gated branches rely on runtime dvd_type_t value
     * type can only be DVD_VR / DVD_A when libdvdread provides them
     * fields used here are unconditional in demux_sys_t */
    bool at_end_of_title =
        (type == DVD_VR && p_sys->i_cur_cell >= p_sys->i_title_end_cell
            && !p_sys->i_pack_len) ||
        (type == DVD_A && !p_sys->i_pack_len &&
            p_sys->i_cur_block > p_sys->i_title_end_block) ||
        (type == DVD_V && p_sys->i_cur_cell >= p_sys->p_cur_pgc->nr_of_cells);

    /*
     * Playback by cell in this pgc, starting at the cell for our chapter.
     */

    /*  there are no system packets containing navigation information in DVD-A,
     *  and an AOB file just contains a single audio stream */

    /*
     * Check end of pack, and select the following one
     */
#if defined(DVDREAD_HAS_DVDVIDEORECORDING)
    if( !p_sys->i_pack_len && type == DVD_VR
        && p_sys->i_cur_cell < p_sys->i_title_end_cell )
    {
        bool advanced = false;
        uint16_t srpn = p_sys->ud_pgcit->m_c_gi[p_sys->i_cur_cell].m_vobi_srpn;
        if( srpn == 0 || srpn > p_sys->pgc_gi->nr_of_programs )
        {
            msg_Err( p_demux, "invalid m_vobi_srpn %u", srpn );
            return VLC_EGENERIC;
        }

        vobu_map_t *map = &p_sys->pgc_gi->pgi[srpn - 1].map;
        const uint32_t total_sectors = DvdVRGetProgramSectorSpan( p_sys, map );
        const uint32_t block_end = map->vob_offset + total_sectors;

        /* only advance to the next vr program once the current one is consumed
         * vr has no dsi, block_end overshoot triggers advance */
        if( total_sectors > 0 && p_sys->i_cur_block >= 0 &&
            (uint32_t)p_sys->i_cur_block >= block_end )
        {
            /* recount from title start, valid after seeks */
            uint32_t cell_offset = 0;
            for( int c = p_sys->i_title_start_cell; c < p_sys->i_cur_cell; c++ )
            {
                uint16_t cell_srpn = p_sys->ud_pgcit->m_c_gi[c].m_vobi_srpn;
                if( cell_srpn == 0 || cell_srpn > p_sys->pgc_gi->nr_of_programs )
                {
                    msg_Err( p_demux, "invalid m_vobi_srpn %" PRIu16, cell_srpn );
                    return VLC_EGENERIC;
                }
                cell_offset += p_sys->pgc_gi->pgi[cell_srpn - 1].map.nr_of_vobu_info;
            }
            p_sys->i_title_offset = cell_offset + map->nr_of_vobu_info;
            p_sys->i_cur_cell++;
            DvdReadResetCellTs( p_sys );
            advanced = true;

            if( p_sys->i_cur_cell < p_sys->i_title_end_cell )
            {
                srpn = p_sys->ud_pgcit->m_c_gi[p_sys->i_cur_cell].m_vobi_srpn;
                if( unlikely( srpn == 0 || srpn > p_sys->pgc_gi->nr_of_programs ) )
                {
                    msg_Err( p_demux, "invalid m_vobi_srpn %" PRIu16, srpn );
                    return VLC_EGENERIC;
                }
                map = &p_sys->pgc_gi->pgi[srpn - 1].map;
            }
        }

        if( advanced && p_sys->i_cur_cell < p_sys->i_title_end_cell )
        {
            p_sys->i_cur_block = map->vob_offset;
            p_sys->i_pack_len = DvdVRGetProgramSectorSpan( p_sys, map );
            p_sys->ops->find_cell( p_demux );
        }
        at_end_of_title |=
            (p_sys->i_cur_cell >= p_sys->i_title_end_cell
                && !p_sys->i_pack_len);
    }
#endif
#if defined(DVDREAD_HAS_DVDAUDIO)
    if( !p_sys->i_pack_len && type == DVD_A )
    {
        if( p_sys->i_cur_block <= p_sys->i_title_end_block )
            p_sys->i_pack_len = p_sys->i_title_end_block - p_sys->i_cur_block + 1;
    }
    else
#endif
    if( !p_sys->i_pack_len && type == DVD_V )
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

        if( p_sys->i_pack_len >= 1024 )
        {
            msg_Err( p_demux, "i_pack_len >= 1024 (%i). "
                     "This shouldn't happen!", p_sys->i_pack_len );
            return 0; /* EOF */
        }

        p_sys->i_cur_block++;
        p_sys->i_title_offset++;
    }

    if( at_end_of_title )
    {
        int k = p_sys->i_title;

        /* Looking for a not broken title */
        while( k < p_sys->i_titles &&
               p_sys->ops->set_area( p_demux, ++k, 0, -1 ) != VLC_SUCCESS )
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

    const uint32_t i_block_start = p_sys->i_cur_block;
    const int i_title_offset_start = p_sys->i_title_offset;

#if 0
    msg_Dbg( p_demux, "i_blocks: %d len: %d current: 0x%02x",
             i_read, p_sys->i_pack_len, i_block_start );
#endif

    for( int i = 0; i < i_read; i++ )
    {
        p_sys->i_cur_block = i_block_start + i;

        DemuxBlock( p_demux, p_buffer + i * DVD_VIDEO_LB_LEN,
                    DVD_VIDEO_LB_LEN );
    }

    p_sys->i_cur_block = i_block_start + i_read;
    if( type != DVD_VR )
        p_sys->i_title_offset = i_title_offset_start + i_read;

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
                if( p_sys->cell_ts.dvd == VLC_TICK_INVALID ||
                    p_sys->cell_ts.ps == VLC_TICK_INVALID )
                {
                    vlc_tick_t base;
                    if( DvdReadGetTimelineBase( p_sys, &base ) )
                    {
                        p_sys->cell_ts.dvd = base;
                        p_sys->cell_ts.ps = i_scr;
                    }
                }

                vlc_tick_t shift;
                if( DvdReadCellTsShift( p_sys, &shift ) )
                {
                    vlc_tick_t pcr = VLC_TICK_0 + i_scr + shift;
                    /* negative shift when cell time precedes first scr */
                    if( pcr < VLC_TICK_0 )
                        pcr = VLC_TICK_0;
                    es_out_SetPCR( p_demux->out, pcr );
                }
                else
                {
                    if( p_sys->cell_ts.dvd != VLC_TICK_INVALID )
                        msg_Dbg( p_demux, "segment timestamp anchor not initialized yet" );
                    es_out_SetPCR( p_demux->out, VLC_TICK_0 + i_scr );
                }
                if( i_mux_rate > 0 ) p_sys->i_mux_rate = i_mux_rate;
            }
            block_Release( p_pkt );
            break;
        }
        default:
        {
            int i_id = ps_pkt_id( p_pkt->p_buffer, p_pkt->i_buffer,
                                  p_sys->ops->ps_source() );
            if( i_id >= 0xc0 )
            {
                ps_track_t *tk = &p_sys->tk[ps_id_to_tk(i_id)];

                if( !tk->b_configured )
                {
                    DvdReadESNew( p_demux, i_id, 0 );
                }
                if( tk->es &&
                    !ps_pkt_parse_pes( VLC_OBJECT(p_demux), p_pkt, tk->i_skip ) )
                {
                    /* for vr anchor to earliest pts/dts seen,
                     * reduces underflow risk in rebased timestamps */
                    p_sys->ops->adjust_anchor( p_sys, p_pkt );
                    vlc_tick_t shift;
                    if( DvdReadCellTsShift( p_sys, &shift ) )
                    {
                        /* shared lift keeps dts and pts both >= VLC_TICK_0
                         * while preserving the original dts/pts delta */
                        vlc_tick_t lift = 0;
                        if( p_pkt->i_dts != VLC_TICK_INVALID )
                        {
                            vlc_tick_t need = VLC_TICK_0 - ( p_pkt->i_dts + shift );
                            if( need > lift ) lift = need;
                        }
                        if( p_pkt->i_pts != VLC_TICK_INVALID )
                        {
                            vlc_tick_t need = VLC_TICK_0 - ( p_pkt->i_pts + shift );
                            if( need > lift ) lift = need;
                        }
                        if( p_pkt->i_dts != VLC_TICK_INVALID )
                            p_pkt->i_dts += shift + lift;
                        if( p_pkt->i_pts != VLC_TICK_INVALID )
                            p_pkt->i_pts += shift + lift;
                    }
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
void DvdReadESNew( demux_t *p_demux, int i_id, int i_lang )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    ps_track_t  *tk = &p_sys->tk[ps_id_to_tk(i_id)];
    char psz_language[3];

    if( tk->b_configured ) return;

    if( ps_track_fill( tk, NULL, i_id, NULL, 0, true ) )
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
        tk->fmt.subs.spu.b_palette = true;
        static_assert(sizeof(tk->fmt.subs.spu.palette) == sizeof(p_sys->clut),
                      "CLUT palette size mismatch");
        memcpy( tk->fmt.subs.spu.palette, p_sys->clut, sizeof( p_sys->clut ) );

        if( psz_language[0] ) tk->fmt.psz_language = strdup( psz_language );
    }

    tk->es = es_out_Add( p_demux->out, &tk->fmt );
    tk->b_configured = true;
}

static void DemuxTitles( demux_t *p_demux, int *pi_angle )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    if( unlikely( p_sys->ops == NULL ) )
        return;
    p_sys->ops->demux_titles( p_demux, pi_angle );
}
