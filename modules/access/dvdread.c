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

#include <vlc_common.h>
#include <vlc_arrays.h>
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
#include <stdckdint.h>
#include <limits.h>

#include "disc_helper.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ANGLE_TEXT N_("DVD angle")
#define ANGLE_LONGTEXT N_( \
    "Default DVD angle." )


/*****************************************************************************
 * DVDRead Version Compatibility
 *****************************************************************************/
#if DVDREAD_VERSION > DVDREAD_VERSION_CODE(7, 0, 1)
#define DVDREAD_HAS_DVDVIDEORECORDING 1
#endif

#if DVDREAD_VERSION >= DVDREAD_VERSION_CODE(7, 0, 0)
#define DVDREAD_HAS_DVDAUDIO 1
#endif

#if defined(DVDREAD_HAS_DVDVIDEORECORDING)
#define SET_AREA( p_demux, title, chapter, angle ) \
    (((p_sys->type) == DVD_VR ? DvdVRReadSetArea : \
      (p_sys->type) == DVD_A  ? DvdAudioReadSetArea : \
                               DvdReadSetArea)( p_demux, title, chapter, -1 ))
#elif defined(DVDREAD_HAS_DVDAUDIO)
#define SET_AREA( p_demux, title, chapter, angle ) \
    (((p_sys->type) == DVD_A ? DvdAudioReadSetArea : DvdReadSetArea)( p_demux, title, chapter, -1 ))
#else
#define SET_AREA( p_demux, title, chapter, angle ) \
    (DvdReadSetArea( p_demux, title, chapter, -1 ))
#endif

/*local prototype*/
typedef enum
{
    DVD_V = 0,
    DVD_A = 1,
    DVD_VR = 2,
} dvd_type_t;

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

/* DVD-VideoRecording Optional force method */
static int OpenVideoRecording( vlc_object_t *p_this );

/* Sets DVD-audio behavior for Open function*/
static int  OpenAudio ( vlc_object_t * );

static int  OpenCommon( vlc_object_t * , dvd_type_t );

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

    dvd_type_t type;
    union
    {
        /* Video tables */
        struct
        {
            tt_srpt_t    *p_tt_srpt;
            pgc_t        *p_cur_pgc;
        };

#ifdef DVDREAD_HAS_DVDAUDIO
        /* Audio tables */
        struct
        {
            atsi_title_record_t *p_title_table;
        };
#endif

#ifdef DVDREAD_HAS_DVDVIDEORECORDING
        /* VideoRecording tables */
        struct
        {
          /* address map of recordings */
          pgc_gi_t    *pgc_gi;
          /* titles, labels for recordings */
          ud_pgcit_t  *ud_pgcit;
          /* keep track of vobu index */
          /* pointer to current program */
          pgi_t *p_cur_pgi;
        };
#endif

    };

    dsi_t        dsi_pack;
    int          i_ttn;

    int i_pack_len;
    int i_cur_block;
    int i_next_vobu;

    int i_mux_rate;

    /* Current title start/end blocks */
    int i_title_start_block;
    int i_title_end_block;
    uint32_t i_title_blocks;
    int i_title_offset;

    int i_title_start_cell;
    int i_title_end_cell;
    int i_cur_cell;
    int i_next_cell;

    /* ps/dvd anchor pair for rebasing ps scr/pts onto the disc timeline
     * dvd is accumulated cell playback time from title start
     * ps is the matching scr from the first ps pack of that cell
     * both are VLC_TICK_INVALID until the first pack of a cell is seen */
    struct
    {
        vlc_tick_t dvd;
        vlc_tick_t ps;
    } cell_ts;

    /* Track */
    ps_track_t    tk[PS_TK_COUNT];

    int           i_titles;
    input_title_t **titles;

    /* Video */
    int i_sar_num;
    int i_sar_den;

    /* SPU */
    uint32_t clut[VIDEO_PALETTE_CLUT_COUNT];
} demux_sys_t;

static int Control   ( demux_t *, int, va_list );
static int Demux     ( demux_t * );
static int DemuxBlock( demux_t *, const uint8_t *, int );

static inline void DvdReadResetCellTs( demux_sys_t *p_sys )
{
    p_sys->cell_ts.dvd = VLC_TICK_INVALID;
    p_sys->cell_ts.ps = VLC_TICK_INVALID;
}

static void DemuxTitles( demux_t *, int * );
static void ESNew( demux_t *, int, int );

static int  DvdReadSetArea  ( demux_t *, int, int, int );
static int  DvdReadSeek     ( demux_t *, uint32_t );
#ifdef DVDREAD_HAS_DVDVIDEORECORDING
static int  DvdVRReadSetArea  ( demux_t *, int, int, int );
static int  DvdVRReadSeek( demux_t *, uint32_t );
static vlc_tick_t DVDVRGetTitleLength( pgc_gi_t *pgc_gi, ud_pgcit_t *ud_pgcit, int program);
static void DvdVRFindCell( demux_t *p_demux );
#endif
#ifdef DVDREAD_HAS_DVDAUDIO
static int  DvdAudioReadSetArea  ( demux_t *, int, int, int );
static int  DvdAudioReadSeek( demux_t *, uint32_t );
#endif
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
static int OpenCommon( vlc_object_t *p_this , dvd_type_t type )
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
    switch (p_sys->p_vmg_file->ifo_format)
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

    DemuxTitles( p_demux, &p_sys->i_angle );
    if( SET_AREA( p_demux, 0, 0, p_sys->i_angle ) != VLC_SUCCESS )
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
    if( OpenCommon( p_this, DVD_V ) != VLC_SUCCESS )
    {
        msg_Dbg( p_this, "Trying DVD-Video Recording as a fallback" );
        if( OpenCommon( p_this, DVD_VR ) == VLC_SUCCESS )
            return VLC_SUCCESS;

        msg_Dbg( p_this, "Trying DVD-Audio as a fallback" );
        return OpenCommon( p_this, DVD_A );
    }
    return VLC_SUCCESS;
}

static int OpenAudio( vlc_object_t *p_this )
{
    return OpenCommon( p_this, DVD_A );
}

/* should not need a specific callback if libdvdread picks up the type */
static int OpenVideoRecording( vlc_object_t *p_this )
{
    return OpenCommon( p_this, DVD_VR );
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

static vlc_tick_t dvdtime_to_time( const dvd_time_t *dtime )
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

/* total playback duration of the current title in vlc_tick_t
 * for dvd-video sums per-cell playback_time across the active cell range
 * so angle-restricted titles report the correct length
 * falls back to pgc playback_time for other disc types */
static vlc_tick_t DvdReadGetTitleDuration( const demux_sys_t *p_sys )
{
    if( p_sys->type == DVD_V && p_sys->p_cur_pgc && p_sys->p_cur_pgc->cell_playback &&
        p_sys->i_title_start_cell >= 0 &&
        p_sys->i_title_start_cell < p_sys->i_title_end_cell )
    {
        vlc_tick_t dur = 0;
        for( int ci = p_sys->i_title_start_cell; ci <= p_sys->i_title_end_cell; ci++ )
        {
            dur += dvdtime_to_time( &p_sys->p_cur_pgc->cell_playback[ci].playback_time );
        }
        return dur;
    }

    if( p_sys->p_cur_pgc )
        return dvdtime_to_time( &p_sys->p_cur_pgc->playback_time );
    return 0;
}

#ifdef DVDREAD_HAS_DVDVIDEORECORDING
#define DVDVR_FALLBACK_SECTORS_PER_VOBU 512

/* estimated sector span of a vr program's vobu map
 * dvd-vr has no flat vobu address map like VOBU_ADMAP in dvd-video
 * instead time_info entries store vobu_adr as sector offset and vobu_entn
 * as vobu index so the last entry gives a sectors-per-vobu ratio
 * falls back to a per-program or constant ratio when no usable entries exist
 * returns 0 when the map has no VOBUs */
static uint32_t DvdVRGetProgramSectorSpan( const demux_sys_t *p_sys,
                                           const vobu_map_t *map )
{
    if( map->nr_of_time_info > 0 )
    {
        const uint32_t last_adr = map->time_infos[map->nr_of_time_info - 1].vobu_adr;
        const uint32_t last_entn = map->time_infos[map->nr_of_time_info - 1].vobu_entn;
        if( last_entn > 1 )
            return (uint32_t)( (uint64_t)last_adr * map->nr_of_vobu_info /
                               ( last_entn - 1 ) );
    }

    uint32_t sects_per_vobu = DVDVR_FALLBACK_SECTORS_PER_VOBU;
    for( int p = 0; p < p_sys->pgc_gi->nr_of_programs; p++ )
    {
        const vobu_map_t *m = &p_sys->pgc_gi->pgi[p].map;
        if( m->nr_of_time_info > 0 &&
            m->time_infos[m->nr_of_time_info - 1].vobu_entn > 1 )
        {
            sects_per_vobu = m->time_infos[m->nr_of_time_info - 1].vobu_adr /
                             ( m->time_infos[m->nr_of_time_info - 1].vobu_entn - 1 );
            break;
        }
    }

    return map->nr_of_vobu_info * sects_per_vobu;
}

static vlc_tick_t DvdVRProgramDuration( const pgi_t *pgi )
{
    if( pgi->header.vob_v_e_ptm.ptm <= pgi->header.vob_v_s_ptm.ptm )
        return 0;

    const uint32_t delta_ptm =
        pgi->header.vob_v_e_ptm.ptm - pgi->header.vob_v_s_ptm.ptm;
    return FROM_SCALE_NZ( delta_ptm );
}

static uint32_t DvdVRReadTimeToVobuOffset( const demux_sys_t *p_sys, vlc_tick_t t )
{
    if( p_sys->type != DVD_VR || !p_sys->pgc_gi || !p_sys->ud_pgcit ||
        p_sys->i_title_start_cell < 0 || p_sys->i_title_end_cell <= p_sys->i_title_start_cell )
        return 0;

    vlc_tick_t acc = 0;
    uint32_t vobu_acc = 0;

    for( int ci = p_sys->i_title_start_cell; ci < p_sys->i_title_end_cell; ci++ )
    {
        uint16_t srpn = p_sys->ud_pgcit->m_c_gi[ci].m_vobi_srpn;
        if( srpn == 0 || srpn > p_sys->pgc_gi->nr_of_programs )
            return vobu_acc;

        const pgi_t *pgi = &p_sys->pgc_gi->pgi[srpn - 1];
        const uint32_t vobus = pgi->map.nr_of_vobu_info;
        if( vobus == 0 )
            continue;

        const vlc_tick_t dur = DvdVRProgramDuration( pgi );
        if( dur == 0 )
            continue;
        if( acc + dur > t )
        {
            const vlc_tick_t within = t - acc;
            const uint32_t within_vobu = (uint32_t)(within * vobus / dur);
            return vobu_acc + within_vobu;
        }

        acc += dur;
        vobu_acc += vobus;
    }

    return p_sys->i_title_blocks > 0 ? p_sys->i_title_blocks - 1 : 0;
}

#endif

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
    switch( p_sys->type )
    {
        case DVD_V:
        {
            const pgc_t *p_pgc = p_sys->p_cur_pgc;
            if( !p_pgc || !p_pgc->cell_playback ||
                p_sys->i_title_start_cell < 0 || p_sys->i_cur_cell < 0 ||
                p_sys->i_title_start_cell >= p_pgc->nr_of_cells ||
                p_sys->i_cur_cell >= p_pgc->nr_of_cells ||
                p_sys->i_title_start_cell > p_sys->i_cur_cell )
                return false;

            vlc_tick_t cell_dvd_time = 0;
            for( int i = p_sys->i_title_start_cell; i < p_sys->i_cur_cell; i++ )
                cell_dvd_time += dvdtime_to_time( &p_pgc->cell_playback[i].playback_time );

            const cell_playback_t *p_cell = &p_pgc->cell_playback[p_sys->i_cur_cell];
            vlc_tick_t cell_duration = dvdtime_to_time( &p_cell->playback_time );
            const uint32_t cell_sectors = p_cell->last_sector - p_cell->first_sector + 1;

            if( cell_duration > 0 && cell_sectors > 0 )
            {
                int64_t within_sectors = p_sys->i_cur_block - (int64_t)p_cell->first_sector;

                if( within_sectors < 0 )
                    within_sectors = 0;
                else if( within_sectors >= (int64_t)cell_sectors )
                    within_sectors = (int64_t)cell_sectors - 1;

                vlc_tick_t scaled;
                if( !ckd_mul( &scaled, within_sectors, cell_duration ) )
                    cell_dvd_time += scaled / cell_sectors;
            }

            *base = cell_dvd_time;
            return true;
        }
#ifdef DVDREAD_HAS_DVDAUDIO
        case DVD_A:
        {
            if( !p_sys->p_title_table || p_sys->i_title_blocks == 0 )
                return false;

            const uint32_t first_sector =
                p_sys->p_title_table->atsi_track_pointer_rows[0].start_sector;
            const uint32_t current_sector = p_sys->i_cur_block > 0 ? (uint32_t)p_sys->i_cur_block : 0;
            const uint32_t current_offset = current_sector > first_sector ? current_sector - first_sector : 0;
            const vlc_tick_t title_length =
                FROM_SCALE_NZ( p_sys->p_title_table->length_pts );
            *base = (vlc_tick_t)( current_offset * title_length
                                  / p_sys->i_title_blocks );
            return true;
        }
#endif
#ifdef DVDREAD_HAS_DVDVIDEORECORDING
        case DVD_VR:
        {
            if( p_sys->i_title_start_cell < 0 || p_sys->i_cur_cell < 0 ||
                p_sys->i_title_start_cell > p_sys->i_cur_cell ||
                p_sys->i_cur_cell >= p_sys->i_title_end_cell )
                return false;

            vlc_tick_t cell_dvd_time = 0;
            for( int ci = p_sys->i_title_start_cell;
                 ci < p_sys->i_cur_cell && ci < p_sys->i_title_end_cell; ci++ )
            {
                uint16_t srpn = p_sys->ud_pgcit->m_c_gi[ci].m_vobi_srpn;
                if( srpn == 0 || srpn > p_sys->pgc_gi->nr_of_programs )
                    return false;
                const pgi_t *scan_pgi = &p_sys->pgc_gi->pgi[srpn - 1];
                const vlc_tick_t scan_dur = DvdVRProgramDuration( scan_pgi );
                if( scan_dur <= 0 )
                    return false;
                cell_dvd_time += scan_dur;
            }

            uint16_t srpn = p_sys->ud_pgcit->m_c_gi[p_sys->i_cur_cell].m_vobi_srpn;
            if( srpn == 0 || srpn > p_sys->pgc_gi->nr_of_programs )
                return false;

            const pgi_t *pgi = &p_sys->pgc_gi->pgi[srpn - 1];
            const vlc_tick_t program_duration = DvdVRProgramDuration( pgi );
            if( program_duration <= 0 )
                return false;
            const uint32_t total_sectors = DvdVRGetProgramSectorSpan( p_sys, &pgi->map );
            if( total_sectors > 0 )
            {
                int64_t within_sectors = (int64_t)p_sys->i_cur_block - pgi->map.vob_offset;
                if( within_sectors < 0 )
                    within_sectors = 0;
                else if( within_sectors >= (int64_t)total_sectors )
                    within_sectors = (int64_t)total_sectors - 1;

                vlc_tick_t scaled;
                if( !ckd_mul( &scaled, within_sectors, program_duration ) )
                    cell_dvd_time += scaled / total_sectors;
            }

            *base = cell_dvd_time;
            return true;
        }
#endif
        default:
            return false;
    }
}

static vlc_tick_t DvdReadGetControlLength( const demux_sys_t *p_sys )
{
    if( p_sys->type == DVD_VR && p_sys->cur_title >= 0 && p_sys->cur_title < p_sys->i_titles )
        return p_sys->titles[p_sys->cur_title]->i_length;
#ifdef DVDREAD_HAS_DVDAUDIO
    if( p_sys->type == DVD_A )
        return FROM_SCALE_NZ( p_sys->p_title_table->length_pts );
#endif
    return DvdReadGetTitleDuration( p_sys );
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

    /* dvd-vr does not have vts */
    if(unlikely(!p_sys->p_vts_file && p_sys->type != DVD_VR))
        return VLC_EGENERIC;

    switch( i_query )
    {
        case DEMUX_GET_POSITION:
        {
            pf = va_arg( args, double * );

            /* vr i_title_offset is vobu count, block ratio wrong */
            if( p_sys->type == DVD_VR && p_sys->cur_title >= 0 &&
                p_sys->cur_title < p_sys->i_titles )
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

#if defined(DVDREAD_HAS_DVDVIDEORECORDING)
            if ( p_sys->type == DVD_VR )
                return DvdVRReadSeek( p_demux, f * p_sys->i_title_blocks );
#endif
#if defined(DVDREAD_HAS_DVDAUDIO)
            if ( p_sys->type == DVD_A )
                return DvdAudioReadSeek( p_demux, f * p_sys->i_title_blocks );
#endif
            return DvdReadSeek( p_demux, f * p_sys->i_title_blocks );
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

                if( p_sys->type == DVD_VR )
                {
                    vlc_tick_t time = 0;
                    if( length > 0 && DvdReadGetTimelineBase( p_sys, &time ) )
                    {
                        if( time < 0 ) time = 0;
                        if( time > length ) time = length;
                        *va_arg( args, vlc_tick_t * ) = time;
                        return VLC_SUCCESS;
                    }
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

#if defined(DVDREAD_HAS_DVDVIDEORECORDING)
            if ( p_sys->type == DVD_VR )
            {
                const uint32_t vr_vobu =
                    DvdVRReadTimeToVobuOffset( p_sys, t );
                return DvdVRReadSeek( p_demux, vr_vobu );
            }
#endif
#if defined(DVDREAD_HAS_DVDAUDIO)
            if ( p_sys->type == DVD_A )
                return DvdAudioReadSeek( p_demux, i_block_offset );
#endif
            return DvdReadSeek( p_demux, i_block_offset );
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

            if( SET_AREA( p_demux, i, 0, -1 ) != VLC_SUCCESS )
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
            if( SET_AREA(p_demux, -1, i, -1) != VLC_SUCCESS )
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
    dvd_type_t type = p_sys->type;

    if(unlikely(!p_sys->p_vts_file && p_sys->type != DVD_VR))
        return VLC_DEMUXER_EOF;

    uint8_t p_buffer[DVD_VIDEO_LB_LEN * DVD_BLOCK_READ_ONCE];
    int i_blocks_once, i_read;

    /* type-gated branches rely on runtime dvd_type_t value
     * type can only be DVD_VR / DVD_A when libdvdread provides them
     * fields used here are unconditional in demux_sys_t */
    bool at_end_of_title =
        (type == DVD_VR && p_sys->i_cur_cell >= p_sys->i_title_end_cell
            && !p_sys->i_pack_len) ||
        (type == DVD_A && p_sys->i_cur_block >= p_sys->i_title_end_block) ||
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
            DvdVRFindCell( p_demux );
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
               SET_AREA( p_demux, ++k, 0, -1 ) != VLC_SUCCESS )
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
            int i_id = ps_pkt_id( p_pkt->p_buffer, p_pkt->i_buffer, p_sys->type == DVD_A ? PS_SOURCE_AOB : PS_SOURCE_VOB );
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
                    /* for vr anchor to earliest pts/dts seen,
                     * reduces underflow risk in rebased timestamps */
                    if( p_sys->type == DVD_VR &&
                        p_sys->cell_ts.dvd != VLC_TICK_INVALID &&
                        p_sys->cell_ts.ps != VLC_TICK_INVALID )
                    {
                        vlc_tick_t anchor = p_sys->cell_ts.ps;
                        if( p_pkt->i_dts != VLC_TICK_INVALID && p_pkt->i_dts < anchor )
                            anchor = p_pkt->i_dts;
                        if( p_pkt->i_pts != VLC_TICK_INVALID && p_pkt->i_pts < anchor )
                            anchor = p_pkt->i_pts;
                        if( anchor < p_sys->cell_ts.ps )
                            p_sys->cell_ts.ps = anchor;
                    }
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
static void ESNew( demux_t *p_demux, int i_id, int i_lang )
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
                 INT_MAX - p_sys->i_title_blocks < cell_blocks ))
                return VLC_EGENERIC;
            p_sys->i_title_blocks += cell_blocks;
        }

        msg_Dbg( p_demux, "title %d vts_title %d pgc %d pgn %d "
                 "start %d end %d blocks: %u",
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

        /* walk ptts accumulating per-cell durations to set each seekpoint's i_time_offset */
        if( p_sys->i_chapters > 0 )
        {
            input_title_t *p_title = p_sys->titles[i_title];
            if( p_title->seekpoint )
            {
                const int max_sp = __MIN( p_title->i_seekpoint,
                                            p_sys->i_chapters );
                const int max_ptt = p_vts->vts_ptt_srpt->title[p_sys->i_ttn - 1].nr_of_ptts;
                const pgc_t *p_pgc_seek = p_sys->p_cur_pgc;
                int cell = p_sys->i_title_start_cell;
                vlc_tick_t t = 0;
                for( int sj = 0; sj < max_sp && sj < max_ptt; sj++ )
                {
                    const uint16_t sj_pgc_id =
                        p_vts->vts_ptt_srpt->title[p_sys->i_ttn - 1]
                                             .ptt[sj].pgcn;
                    const uint16_t sj_pgn =
                        p_vts->vts_ptt_srpt->title[p_sys->i_ttn - 1]
                                             .ptt[sj].pgn;

                    /* cells are pgc-local, stop on pgc mismatch */
                    if( sj_pgc_id != pgc_id || sj_pgn == 0 )
                        break;
                    if( sj_pgn > p_pgc_seek->nr_of_programs )
                        break;

                    const int chapter_cell =
                        p_pgc_seek->program_map[sj_pgn - 1] - 1;
                    while( cell < chapter_cell &&
                           cell <= p_sys->i_title_end_cell )
                    {
                        t += dvdtime_to_time( &p_pgc_seek->cell_playback[cell]
                                                  .playback_time );
                        cell++;
                    }
                    p_title->seekpoint[sj]->i_time_offset = t;
                }
                p_title->i_length = DvdReadGetTitleDuration( p_sys );
            }
        }

        DvdReadResetCellTs( p_sys );

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

        es_out_Control( p_demux->out, ES_OUT_RESET_PCR );

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
                    i_id = (0x80 + i_position) | PS_PACKET_ID_MASK_VOB;
                    break;
                case 0x02:
                case 0x03: /* MPEG audio */
                    i_id = 0xc000 + i_position;
                    break;
                case 0x04: /* LPCM */
                    i_id = (0xa0 + i_position) | PS_PACKET_ID_MASK_VOB;
                    break;
                case 0x06: /* DTS */
                    i_id = (0x88 + i_position) | PS_PACKET_ID_MASK_VOB;
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

        static_assert(sizeof(p_sys->clut) == sizeof(p_pgc->palette),
                      "mismatch CLUT size");
        memcpy( p_sys->clut, p_pgc->palette, sizeof( p_sys->clut ) );

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

                i_id = (0x20 + i_position) | PS_PACKET_ID_MASK_VOB;

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

#ifdef DVDREAD_HAS_DVDAUDIO
/* function different enough to warrant a seperate implementation*/
static int DvdAudioReadSetArea( demux_t *p_demux, int i_title, int i_track,
                                int i_angle )
{
    VLC_UNUSED( i_angle );

    demux_sys_t *p_sys = p_demux->p_sys;

    if( i_title >= 0 && i_title < p_sys->i_titles &&
        i_title != p_sys->i_title )
    {
        if( p_sys->p_title != NULL )
        {
            DVDCloseFile( p_sys->p_title );
            p_sys->p_title = NULL;
        }
        p_sys->i_title = i_title;
        DvdReadResetCellTs( p_sys );

        /* reusing p_vmg variable for p_amg */
        const ifo_handle_t *p_vmg = p_sys->p_vmg_file;

        /*
         *  We have to load all title information
         */
        msg_Dbg( p_demux, "open ATS %d, for group %d",
                 p_vmg->info_table_second_sector->tracks_info[i_title].group_property, i_title+ 1 );

        /* Ifo ats */
        /* reusing p_vts variable for p_ats */
        if( p_sys->p_vts_file != NULL )
            ifoClose( p_sys->p_vts_file );
        if( !( p_sys->p_vts_file = ifoOpen( p_sys->p_dvdread,
               p_vmg->info_table_second_sector->tracks_info[i_title].group_property ) ) )
        {
            msg_Err( p_demux, "fatal error in ats ifo" );
            return VLC_EGENERIC;
        }

        const ifo_handle_t *p_vts = p_sys->p_vts_file;

        /* Title position inside the selected ats, i_title is the overall title number */
        p_sys->i_ttn = p_vmg->info_table_second_sector->tracks_info[i_title].title_property;

        const atsi_title_record_t *atsi_title_table=
            p_sys->p_title_table = &p_vts->atsi_title_table->atsi_title_row_tables[p_sys->i_ttn-1];

        p_sys->i_chapter = 0;
        p_sys->i_chapters =
            p_vts->atsi_title_table->atsi_title_row_tables[p_sys->i_ttn - 1].nr_pointer_records;

        /* there are no cells in dvd audio, bellow the start and end sectors of the title set. */
        p_sys->i_title_start_block = atsi_title_table->atsi_track_pointer_rows[0].start_sector;
        p_sys->i_title_end_block = atsi_title_table->atsi_track_pointer_rows[p_sys->i_chapters - 1].end_sector;

        p_sys->i_title_blocks = p_sys->i_title_end_block - p_sys->i_title_start_block + 1;
        msg_Dbg( p_demux, "title %d ttn %d start %d end %d blocks: %u",
                 i_title, p_sys->i_ttn,
                 p_sys->i_title_start_block, p_sys->i_title_end_block,
                 p_sys->i_title_blocks );

        if( p_sys->i_chapters > 0 )
        {
            input_title_t *p_title = p_sys->titles[i_title];
            if( p_title->seekpoint && p_sys->i_title_blocks > 0 )
            {
                const vlc_tick_t title_length = FROM_SCALE_NZ(
                    p_sys->p_title_table->length_pts );
                const uint32_t first_sector =
                    p_sys->p_title_table->atsi_track_pointer_rows[0].start_sector;

                p_title->i_length = title_length;

                const int max_sp = __MIN( p_title->i_seekpoint,
                                            p_sys->i_chapters );
                /* offset relative to title start */
                for( int sj = 0; sj < max_sp; sj++ )
                {
                    const uint32_t sj_sector =
                        p_sys->p_title_table->atsi_track_pointer_rows[sj].start_sector;
                    const uint32_t offset = sj_sector > first_sector
                        ? sj_sector - first_sector : 0;
                    if( title_length > 0 &&
                        offset > UINT64_MAX / (uint64_t)title_length )
                    {
                        msg_Err( p_demux, "chapter offset multiply overflow" );
                        return VLC_EGENERIC;
                    }
                    /* widen for the multiply, product fits uint64_t */
                    p_title->seekpoint[sj]->i_time_offset =
                        (vlc_tick_t)( (uint64_t)offset * title_length /
                                      p_sys->i_title_blocks );
                }
            }
        }

        /* The structure of DVD-A discs seems to be the following
         * each ATS IFO-> is a GROUP -> Contains multiple titles, Span across one or more AOBs -> each title contains multiple tracks or "Trackpoints",
         *
         * trackpoints are counted as tracks. They have records in the pointer table, and nr_pointer_records will include trackpoints. this is why it is different to nr_tracks*/

        /*
         * Set properties for current track, consider i_chapter as tracks/trackpoints
         */
       /*
         * We've got enough info, time to open the ATS AOB, indexed by "group_property"
         */
        if( !( p_sys->p_title = DVDOpenFile( p_sys->p_dvdread,
               p_vmg->info_table_second_sector->tracks_info[i_title].group_property,
               DVD_READ_TITLE_VOBS ) ) )
        {
            msg_Err( p_demux, "cannot open title (ATS_%02d_1.AOB)",
                     p_vmg->info_table_second_sector->tracks_info[i_title].group_property );
            return VLC_EGENERIC;
        }

        /* for now we are using the "second table, which includes no VOB tracks, assuming that if a user wants to open the video side he will select the standard DVD option"*/

        /*
         * Destroy obsolete ES by reinitializing program 0
         * and find all ES in title with ifo data
         */

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

        es_out_Control( p_demux->out, ES_OUT_RESET_PCR );

        if( p_sys->cur_title != i_title)
        {
            p_sys->updates |= INPUT_UPDATE_TITLE | INPUT_UPDATE_SEEKPOINT;
            p_sys->cur_title = i_title;
            p_sys->cur_chapter = 0;
        }
    }
    else if( i_title != -1 && i_title != p_sys->i_title )

    {
        return VLC_EGENERIC; /* Couldn't set title */
    }

    /*
     * Chapter selection
     */
    if( i_track >= 0 && i_track < p_sys->i_chapters )
    {
        p_sys->i_chapter = i_track;

        p_sys->i_title_offset =  p_sys->p_title_table->atsi_track_pointer_rows[i_track].start_sector
            - p_sys->p_title_table->atsi_track_pointer_rows[0].start_sector;

        msg_Dbg(p_demux, "Title Offset: %d", p_sys->i_title_offset);

        p_sys->i_pack_len = 0;
        /* current block relative to start of title*/
        p_sys->i_cur_block=p_sys->p_title_table->atsi_track_pointer_rows[i_track].start_sector;
        DvdReadResetCellTs( p_sys );

        if( p_sys->cur_chapter != i_track)
        {
            p_sys->updates |= INPUT_UPDATE_SEEKPOINT;
            p_sys->cur_chapter = i_track;
        }
    }
    else if( i_track != -1 )
    {

        msg_Dbg( p_demux, "Couldn't set chapter" );
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}
#endif

#ifdef DVDREAD_HAS_DVDVIDEORECORDING
static int DvdVRReadSetArea( demux_t *p_demux, int i_title, int i_chapter,
                             int i_angle )
{
    VLC_UNUSED( i_angle );
    demux_sys_t *p_sys = p_demux->p_sys;

    /* user made title selection */
    if( i_title >= 0 && i_title < p_sys->i_titles &&
        i_title != p_sys->i_title )
    {
        int i_start_cell, i_end_cell;

        if( p_sys->p_title != NULL )
            DVDCloseFile( p_sys->p_title );
        if( !( p_sys->p_title = DVDOpenFile( p_sys->p_dvdread, 0, DVD_READ_TITLE_VOBS ) ) )
        {
            msg_Err( p_demux, "cannot open VR_MOVIE.VRO");
            return VLC_EGENERIC;
        }

        uint16_t fpid = p_sys->ud_pgcit->ud_pgci_items[i_title].first_prog_id;
        if( fpid == 0 || fpid == 0xFFFF
            || fpid > p_sys->pgc_gi->nr_of_programs )
        {
            msg_Warn( p_demux, "invalid first_prog_id %u for title %d",
                      fpid, i_title );
            return VLC_EGENERIC;
        }
        i_start_cell = p_sys->i_title_start_cell = fpid - 1;

        p_sys->i_cur_cell = i_start_cell;
        i_end_cell = p_sys->i_title_end_cell = p_sys->ud_pgcit->ud_pgci_items[i_title].first_prog_id - 1 +
           p_sys->ud_pgcit->ud_pgci_items[i_title].nr_of_programs;

        p_sys->i_chapters = 0;
        for (int c = i_start_cell; c < i_end_cell; c++)
        {
            m_c_gi_t *cell = &p_sys->ud_pgcit->m_c_gi[c];
            p_sys->i_chapters += cell->c_epi_n ? cell->c_epi_n : 1;
        }

        /* which program we should search for our timestamp */
        uint16_t m_vobi_srpn =  p_sys->ud_pgcit->m_c_gi[i_start_cell].m_vobi_srpn;
        if( m_vobi_srpn == 0 || m_vobi_srpn > p_sys->pgc_gi->nr_of_programs )
        {
            msg_Err( p_demux, "invalid m_vobi_srpn %u for cell %d",
                     m_vobi_srpn, i_start_cell );
            return VLC_EGENERIC;
        }

        /* find the corresponding address */
        p_sys->p_cur_pgi = &p_sys->pgc_gi->pgi[m_vobi_srpn - 1];

        uint32_t vob_offset = p_sys->p_cur_pgi->map.vob_offset;

        p_sys->i_cur_block = vob_offset;

        p_sys->i_title_blocks = 0;
        for( int c = i_start_cell; c < i_end_cell; c++ )
        {
            uint16_t srpn = p_sys->ud_pgcit->m_c_gi[c].m_vobi_srpn;
            if( srpn == 0 || srpn > p_sys->pgc_gi->nr_of_programs )
                continue;
            p_sys->i_title_blocks += p_sys->pgc_gi->pgi[srpn - 1].map.nr_of_vobu_info;
        }
        p_sys->i_title_offset = 0;
        p_sys->i_pack_len = 0;

        p_sys->i_title = i_title;

        /*
         * Destroy obsolete ES by reinitializing program 0
         * and find all ES in title with ifo data
         */

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

        es_out_Control( p_demux->out, ES_OUT_RESET_PCR );

        if( p_sys->cur_title != i_title)
        {
            p_sys->updates |= INPUT_UPDATE_TITLE | INPUT_UPDATE_SEEKPOINT;
            p_sys->cur_title = i_title;
            p_sys->cur_chapter = 0;
        }

    }
    else if( i_title != -1 && i_title != p_sys->i_title )
    {
        return VLC_EGENERIC; /* Couldn't set title */
    }

    /* Search for chapter */
    if( i_chapter >= 0 && i_chapter < p_sys->i_chapters )
    {
        int i, j;
        int accum_chapter = 0;
        uint32_t chapter_ptm;
        uint32_t chapter_program;
        int found = 0;

        /* find the correct entry point */
        for (i = p_sys->i_title_start_cell;
            i < p_sys->i_title_end_cell && !found; i++)
        {
            int n_ep = p_sys->ud_pgcit->m_c_gi[i].c_epi_n;
            if( n_ep == 0 )
            {
                /* cell has no explicit entry points — treat as one chapter */
                if( accum_chapter == i_chapter )
                {
                    chapter_ptm = p_sys->ud_pgcit->m_c_gi[i].c_v_s_ptm.ptm;
                    chapter_program = p_sys->ud_pgcit->m_c_gi[i].m_vobi_srpn;
                    found = 1;
                    break;
                }
                accum_chapter++;
            }
            else
            {
                for (j = 0; j < n_ep; j++)
                {
                    if (accum_chapter == i_chapter)
                    {
                        chapter_ptm = p_sys->ud_pgcit->m_c_gi[i].m_c_epi[j].ep_ptm.ptm;
                        chapter_program = p_sys->ud_pgcit->m_c_gi[i].m_vobi_srpn;
                        found = 1;
                        break;
                    }
                    accum_chapter++;
                }
            }
        }
        if (!found)
            return VLC_EGENERIC;

        /* search map for the address */
        vobu_map_t map = p_sys->pgc_gi->pgi[chapter_program - 1].map;

        /* find a vobu index to search for */
        uint32_t relative_ptm = chapter_ptm
            - p_sys->pgc_gi->pgi[chapter_program - 1].header.vob_v_s_ptm.ptm;

        uint32_t total_pts = p_sys->pgc_gi->pgi[chapter_program - 1].header.vob_v_e_ptm.ptm
            - p_sys->pgc_gi->pgi[chapter_program - 1].header.vob_v_s_ptm.ptm;

        uint32_t estimated_vobu = 0;
        if( total_pts > 0 && map.nr_of_vobu_info > 0 )
            estimated_vobu = (uint32_t)( (uint64_t)relative_ptm
                * map.nr_of_vobu_info / total_pts );

        int time_offset_i = 0;
        while ( time_offset_i < map.nr_of_time_info - 1
            && map.time_infos[time_offset_i + 1].vobu_entn <= estimated_vobu )
            time_offset_i++;

        uint32_t chapter_offset = map.time_infos[time_offset_i].vobu_adr + map.vob_offset;
        p_sys->i_cur_block = chapter_offset;
        p_sys->i_pack_len = 0;
        p_sys->i_chapter = i_chapter;
        p_sys->cur_chapter = i_chapter;

        for( int ci = p_sys->i_title_start_cell;
             ci < p_sys->i_title_end_cell; ci++ )
        {
            if( p_sys->ud_pgcit->m_c_gi[ci].m_vobi_srpn == chapter_program )
            {
                p_sys->i_cur_cell = ci;
                break;
            }
        }

        p_sys->i_title_offset = 0;
        for( int ci = p_sys->i_title_start_cell;
             ci < p_sys->i_cur_cell; ci++ )
        {
            uint16_t s = p_sys->ud_pgcit->m_c_gi[ci].m_vobi_srpn;
            p_sys->i_title_offset += p_sys->pgc_gi->pgi[s - 1].map.nr_of_vobu_info;
        }

        return VLC_SUCCESS;
    }
    else if( i_chapter != -1 )
    {
        return VLC_EGENERIC; /* Couln't set chapter */
    }

    return VLC_SUCCESS;
}

static void DvdVRFindCell( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    p_sys->i_next_cell = p_sys->i_cur_cell + 1;

    if( p_sys->i_cur_cell >= p_sys->i_title_end_cell )
        return;

    /* labelled as entry point in dvd-vr */
    int chapter = 0;

    /* see if we've reached the next entry point */
    ud_pgci_t *psi = &p_sys->ud_pgcit->ud_pgci_items[p_sys->i_title];
    int cell_base = psi->first_prog_id - 1;

    for( int i = 0; i < psi->nr_of_programs; i++ )
    {
        int cell_idx = cell_base + i;
        m_c_gi_t *cell = &p_sys->ud_pgcit->m_c_gi[cell_idx];

        if ( cell_idx == p_sys->i_cur_cell )
            break;

        /* entry points are chapters, count cell as 1 if no entry points */
        chapter += cell->c_epi_n ? cell->c_epi_n : 1;

    }

    if( chapter >= p_sys->i_chapters )
        chapter = p_sys->i_chapters > 0 ? p_sys->i_chapters - 1 : 0;

    if( chapter != p_sys->i_chapter )
    {
        p_sys->i_chapter = chapter;
        if( p_sys->cur_chapter != chapter )
        {
            p_sys->updates |= INPUT_UPDATE_SEEKPOINT;
            p_sys->cur_chapter = chapter;
        }
    }

}

static vlc_tick_t DVDVRGetTitleLength( pgc_gi_t *pgc_gi, ud_pgcit_t *ud_pgcit, int program)
{
    uint64_t length_ptm = 0;
    uint16_t first_prog_id = ud_pgcit->ud_pgci_items[program].first_prog_id;
    uint16_t nr_of_programs = ud_pgcit->ud_pgci_items[program].nr_of_programs;

    for ( int i = 0; i < nr_of_programs; i++ )
        length_ptm += pgc_gi->pgi[first_prog_id -1 + i].header.vob_v_e_ptm.ptm
            - pgc_gi->pgi[first_prog_id -1 + i].header.vob_v_s_ptm.ptm;

    return FROM_SCALE_NZ( length_ptm );
}
#endif

/*****************************************************************************
 * DvdReadSeek : Goes to a given position on the stream.
 *****************************************************************************
 * This one is used by the input and translate chronological position from
 * input to logical position on the device.
 *****************************************************************************/
static int DvdReadSeek( demux_t *p_demux, uint32_t i_block_offset )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_chapter = 0;
    int i_cell = 0;
    uint32_t i_block;
    const pgc_t *p_pgc = p_sys->p_cur_pgc;
    const ifo_handle_t *p_vts = p_sys->p_vts_file;

    /* Find cell */
    i_block = i_block_offset;
    for( i_cell = p_sys->i_title_start_cell;
         i_cell <= p_sys->i_title_end_cell; i_cell++ )
    {
        if( i_block < p_pgc->cell_playback[i_cell].last_sector -
            p_pgc->cell_playback[i_cell].first_sector + 1 ) break;

        i_block -= (p_pgc->cell_playback[i_cell].last_sector -
            p_pgc->cell_playback[i_cell].first_sector + 1);
    }
    if( i_cell > p_sys->i_title_end_cell )
    {
        msg_Err( p_demux, "couldn't find cell for block %u", i_block_offset );
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
    if( unlikely( p_vts->vts_vobu_admap == NULL ||
                  p_vts->vts_vobu_admap->last_byte < VOBU_ADMAP_SIZE ) )
        return VLC_EGENERIC;

    size_t i_vobu = 1;
    const size_t i_vobu_sect_index_count =
            (p_vts->vts_vobu_admap->last_byte + 1 - VOBU_ADMAP_SIZE) /
            sizeof(*p_vts->vts_vobu_admap->vobu_start_sectors);
    if( unlikely( i_vobu_sect_index_count == 0 ) )
        return VLC_EGENERIC;
    for( size_t i=0; i<i_vobu_sect_index_count; i++ )
    {
        if( p_vts->vts_vobu_admap->vobu_start_sectors[i] > i_block )
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

    /* i_vobu reaches count when no entry exceeds i_block, guard the debug peek */
    const size_t i_vobu_dbg_next = i_vobu < i_vobu_sect_index_count
        ? i_vobu : i_vobu_sect_index_count - 1;
    msg_Dbg( p_demux, "cell %d i_sub_cell %d chapter %d vobu %zu "
             "cell_sector %d vobu_sector %d sub_cell_sector %d",
             i_cell, i_sub_cell, i_chapter, i_vobu,
             p_sys->p_cur_pgc->cell_playback[i_cell].first_sector,
             p_vts->vts_vobu_admap->vobu_start_sectors[i_vobu_dbg_next],
             p_vts->vts_c_adt->cell_adr_table[i_sub_cell - 1].start_sector);
#endif

    /* snap to vobu start, nav packet only at vobu head */
    const uint32_t vobu_start_sector_snap =
        p_vts->vts_vobu_admap->vobu_start_sectors[i_vobu - 1];

    p_sys->i_cur_block = vobu_start_sector_snap;
    p_sys->i_next_vobu = vobu_start_sector_snap;
    /* len set by DvdReadHandleDSI from dsi */
    p_sys->i_pack_len = 0;
    p_sys->i_cur_cell = i_cell;
    p_sys->i_chapter = i_chapter;
    DvdReadFindCell( p_demux );

    DvdReadResetCellTs( p_sys );
    es_out_Control( p_demux->out, ES_OUT_RESET_PCR );

    return VLC_SUCCESS;
}


#ifdef DVDREAD_HAS_DVDAUDIO
static int DvdAudioReadSeek( demux_t *p_demux, uint32_t i_block_offset )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_chapter;
    uint32_t i_seek_blocks = 0;

    /* set pack length */
    /* set current block*/

    /* find current chapter */
    for ( i_chapter = 0 ; i_chapter < p_sys->i_chapters; i_chapter++ ) {
        uint32_t start = p_sys->p_title_table->atsi_track_pointer_rows[i_chapter].start_sector;
        uint32_t end = p_sys->p_title_table->atsi_track_pointer_rows[i_chapter].end_sector;
        uint32_t chapter_len = end - start + 1;

        if ( i_block_offset < i_seek_blocks + chapter_len )
            break;

        i_seek_blocks += chapter_len;
    }

    /* exit if i_chapter is invalid */
    if( i_chapter >= p_sys->i_chapters )
        return VLC_EGENERIC;

    if( p_sys->cur_chapter != i_chapter )
    {
        p_sys->updates |= INPUT_UPDATE_SEEKPOINT;
        p_sys->cur_chapter = i_chapter;
    }

    const uint32_t start_sector =
        p_sys->p_title_table->atsi_track_pointer_rows[i_chapter].start_sector;
    /* i_block_offset title-relative, i_seek_blocks is chapter start */
    const uint32_t within_chapter = i_block_offset - i_seek_blocks;
    p_sys->i_cur_block = start_sector + within_chapter;

    if( p_sys->i_cur_block <= p_sys->i_title_end_block )
        p_sys->i_pack_len = p_sys->i_title_end_block - p_sys->i_cur_block + 1;
    else
        p_sys->i_pack_len = 0;
    p_sys->i_title_offset = i_block_offset;
    p_sys->i_chapter = i_chapter;
    DvdReadResetCellTs( p_sys );

    return VLC_SUCCESS;
}
#endif

#ifdef DVDREAD_HAS_DVDVIDEORECORDING
static int DvdVRReadSeek( demux_t *p_demux, uint32_t i_block_offset )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_chapter = 0;
    uint32_t vobu_accum = 0;

    int cell_base = p_sys->ud_pgcit->ud_pgci_items[p_sys->i_title].first_prog_id - 1;
    int nr = p_sys->ud_pgcit->ud_pgci_items[p_sys->i_title].nr_of_programs;

    int found = 0;
    for( int c = 0; c < nr; c++ )
    {
        int cell_idx = cell_base + c;
        uint16_t srpn = p_sys->ud_pgcit->m_c_gi[cell_idx].m_vobi_srpn;
        if( srpn == 0 || srpn > p_sys->pgc_gi->nr_of_programs )
            continue;
        uint16_t cell_vobus = p_sys->pgc_gi->pgi[srpn - 1].map.nr_of_vobu_info;

        if( i_block_offset < vobu_accum + cell_vobus )
        {
            p_sys->i_cur_cell = cell_idx;
            found = 1;
            break;
        }
        vobu_accum += cell_vobus;

        i_chapter += p_sys->ud_pgcit->m_c_gi[cell_idx].c_epi_n
                   ? p_sys->ud_pgcit->m_c_gi[cell_idx].c_epi_n : 1;
    }

    if( !found )
    {
        msg_Err( p_demux, "couldn't find cell for block offset %u", i_block_offset );
        return VLC_EGENERIC;
    }

    if( i_chapter < p_sys->i_chapters &&
        p_sys->cur_chapter != i_chapter )
    {
        p_sys->updates |= INPUT_UPDATE_SEEKPOINT;
        p_sys->cur_chapter = i_chapter;
    }

    p_sys->i_pack_len = 0;
    p_sys->i_title_offset = vobu_accum;
    p_sys->i_chapter = i_chapter;

    return VLC_SUCCESS;
}
#endif

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

        DvdReadResetCellTs( p_sys );

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

#ifdef DVDREAD_HAS_DVDVIDEORECORDING
static const char* ParseTxtEncoding( uint8_t txt_encoding )
{
    const char* charset = "Unknown";

    switch (txt_encoding) {
    case 0x00: charset = "ASCII"; break;
    case 0x01: charset = "ISO646-JP"; break;
    case 0x10: charset = "JIS_C6220-1969-RO"; break;
    case 0x11: charset = "ISO_8859-1"; break;
    case 0x12: charset = "SHIFT_JIS"; break;
    default:
        charset = "ISO_8859-15";
        break;
    }

    return charset;
}
#endif

/*****************************************************************************
 * DemuxTitles: get the titles/chapters or group/tracks structure
 *****************************************************************************/
static void DemuxTitles( demux_t *p_demux, int *pi_angle )
{
    VLC_UNUSED( pi_angle );

    demux_sys_t *p_sys = p_demux->p_sys;
    input_title_t *t;
    seekpoint_t *s;

    /* Find out number of titles/chapters */
    int32_t i_titles;
#ifdef DVDREAD_HAS_DVDVIDEORECORDING
    const char* disc_charset = NULL;
    if ( p_sys->type == DVD_VR ) {
        disc_charset = ParseTxtEncoding( p_sys->p_vmg_file->rtav_vmgi->txt_encoding );
        i_titles = p_sys->ud_pgcit->nr_of_pgci;
    } else 
#endif
#ifdef DVDREAD_HAS_DVDAUDIO
    if ( p_sys->type == DVD_A )
        i_titles = p_sys->p_vmg_file->info_table_second_sector->nr_of_titles;
    else
#endif
    i_titles = p_sys->p_vmg_file->tt_srpt->nr_of_srpts;



    msg_Dbg( p_demux, "number of titles: %d", i_titles );

    for( int i = 0; i < i_titles; i++ )
    {
        int32_t i_chapters = 0;
        int j;
#ifdef DVDREAD_HAS_DVDVIDEORECORDING
        if ( p_sys->type == DVD_VR )
        {
            /* chapters are entry points across all cells */
            uint16_t fpid = p_sys->ud_pgcit->ud_pgci_items[i].first_prog_id;
            if( fpid == 0 || fpid == 0xFFFF
                || fpid > p_sys->pgc_gi->nr_of_programs )
                continue; /* invalid PSI, skip this title */
            int cell_base = fpid - 1;
            int nr = p_sys->ud_pgcit->ud_pgci_items[i].nr_of_programs;
            for( int c = 0; c < nr; c++ )
            {
                m_c_gi_t *cell = &p_sys->ud_pgcit->m_c_gi[cell_base + c];
                i_chapters += cell->c_epi_n ? cell->c_epi_n : 1;
            }
        }
        else
#endif
#ifdef DVDREAD_HAS_DVDAUDIO
        if ( p_sys->type == DVD_A )
            i_chapters = p_sys->p_vmg_file->info_table_second_sector->tracks_info[i].nr_chapters_in_title;
        else
#endif
        i_chapters = p_sys->p_vmg_file->tt_srpt->title[i].nr_of_ptts;

        msg_Dbg( p_demux, "title %d has %d chapters", i, i_chapters );

        t = vlc_input_title_New();
        if ( unlikely( !t ) )
            return;
#ifdef DVDREAD_HAS_DVDVIDEORECORDING
        if ( p_sys->type == DVD_VR ) {
            char* converted_title = FromCharset(
                disc_charset,
                p_sys->ud_pgcit->ud_pgci_items[i].title,
                strnlen( p_sys->ud_pgcit->ud_pgci_items[i].title,
                         sizeof(p_sys->ud_pgcit->ud_pgci_items[i].title) )
            );
            if ( converted_title && *converted_title )
                t->psz_name = converted_title;
            else {
                free(converted_title);
                t->psz_name = strndup(p_sys->ud_pgcit->ud_pgci_items[i].label,
                                      sizeof(p_sys->ud_pgcit->ud_pgci_items[i].label) );
            }
            t->i_length = DVDVRGetTitleLength( p_sys->pgc_gi, p_sys->ud_pgcit, i );

            /* create one seekpoint per entry point */
            int cell_base = p_sys->ud_pgcit->ud_pgci_items[i].first_prog_id - 1;
            int nr = p_sys->ud_pgcit->ud_pgci_items[i].nr_of_programs;
            for( int c = 0; c < nr; c++ )
            {
                m_c_gi_t *cell = &p_sys->ud_pgcit->m_c_gi[cell_base + c];
                if( cell->c_epi_n > 0 )
                {
                    for( int ep = 0; ep < cell->c_epi_n; ep++ )
                    {
                        s = vlc_seekpoint_New();
                        if ( unlikely( !s ) )
                            goto fail;

                        s->i_time_offset = FROM_SCALE_NZ( (uint64_t)cell->m_c_epi[ep].ep_ptm.ptm );
                        TAB_APPEND( t->i_seekpoint, t->seekpoint, s );
                    }
                }
                else
                {
                    s = vlc_seekpoint_New();
                    if ( unlikely( !s ) )
                        goto fail;

                    s->i_time_offset = FROM_SCALE_NZ( (uint64_t)cell->c_v_s_ptm.ptm );
                    TAB_APPEND( t->i_seekpoint, t->seekpoint, s );
                }
            }

        }
        else
#endif
        {
#ifdef DVDREAD_HAS_DVDAUDIO
            ifo_handle_t *p_ats_ifo = NULL;
            const atsi_track_timestamp_t *p_track_ts = NULL;
            uint8_t i_track_ts = 0;

            if ( p_sys->type == DVD_A ) {
                const track_info_t * const p_track_info =
                    &p_sys->p_vmg_file->info_table_second_sector->tracks_info[i];
                t->i_length = FROM_SCALE_NZ( p_track_info->len_audio_zone_pts );

                p_ats_ifo = ifoOpen( p_sys->p_dvdread, p_track_info->group_property );
                if ( p_ats_ifo != NULL && p_ats_ifo->atsi_title_table != NULL
                     && p_track_info->title_property > 0
                     && p_track_info->title_property <= p_ats_ifo->atsi_title_table->nr_titles ) {
                    const atsi_title_record_t * const p_title_rec =
                        &p_ats_ifo->atsi_title_table->atsi_title_row_tables[p_track_info->title_property - 1];
                    p_track_ts = p_title_rec->atsi_track_timestamp_rows;
                    i_track_ts = p_title_rec->nr_tracks;
                }
            }
#endif
            for( j = 0; j < __MAX( i_chapters, 1 ); j++ )
            {
                s = vlc_seekpoint_New();
#ifdef DVDREAD_HAS_DVDAUDIO
                if ( p_track_ts != NULL && j < i_track_ts )
                    s->i_time_offset =
                        FROM_SCALE_NZ( (uint64_t)p_track_ts[j].first_pts_of_track );
#endif
                TAB_APPEND( t->i_seekpoint, t->seekpoint, s );
            }
#ifdef DVDREAD_HAS_DVDAUDIO
            if ( p_ats_ifo != NULL )
                ifoClose( p_ats_ifo );
#endif
        }
        TAB_APPEND( p_sys->i_titles, p_sys->titles, t );
    }
    return;

#ifdef DVDREAD_HAS_DVDVIDEORECORDING
fail:
    vlc_input_title_Delete( t );
    return;
#endif
}
