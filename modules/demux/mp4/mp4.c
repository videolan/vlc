/*****************************************************************************
 * mp4.c : MP4 file input module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2004, 2010 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>

#include <vlc_demux.h>
#include <vlc_charset.h>                           /* EnsureUTF8 */
#include <vlc_meta.h>                              /* vlc_meta_t, vlc_meta_ */
#include <vlc_input.h>
#include <assert.h>

#include "libmp4.h"
#include "id3genres.h"                             /* for ATOM_gnre */

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin ()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_description( N_("MP4 stream demuxer") )
    set_shortname( N_("MP4") )
    set_capability( "demux", 240 )
    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int   Demux   ( demux_t * );
static int   DemuxRef( demux_t *p_demux ){ (void)p_demux; return 0;}
static int   DemuxFrg( demux_t * );
static int   Seek    ( demux_t *, mtime_t );
static int   Control ( demux_t *, int, va_list );

struct demux_sys_t
{
    MP4_Box_t    *p_root;      /* container for the whole file */

    mtime_t      i_pcr;

    uint64_t     i_time;         /* time position of the presentation
                                  * in movie timescale */
    uint64_t     i_timescale;    /* movie time scale */
    uint64_t     i_duration;     /* movie duration */
    unsigned int i_tracks;       /* number of tracks */
    mp4_track_t  *track;         /* array of track */
    float        f_fps;          /* number of frame per seconds */

    bool         b_fragmented;   /* fMP4 */

    /* */
    MP4_Box_t    *p_tref_chap;

    /* */
    input_title_t *p_title;
};

/*****************************************************************************
 * Declaration of local function
 *****************************************************************************/
static void MP4_TrackCreate ( demux_t *, mp4_track_t *, MP4_Box_t  *, bool b_force_enable );
static int MP4_frg_TrackCreate( demux_t *, mp4_track_t *, MP4_Box_t *);
static void MP4_TrackDestroy(  mp4_track_t * );

static int  MP4_TrackSelect ( demux_t *, mp4_track_t *, mtime_t );
static void MP4_TrackUnselect(demux_t *, mp4_track_t * );

static int  MP4_TrackSeek   ( demux_t *, mp4_track_t *, mtime_t );

static uint64_t MP4_TrackGetPos    ( mp4_track_t * );
static int      MP4_TrackSampleSize( mp4_track_t * );
static int      MP4_TrackNextSample( demux_t *, mp4_track_t * );
static void     MP4_TrackSetELST( demux_t *, mp4_track_t *, int64_t );

static void     MP4_UpdateSeekpoint( demux_t * );
static const char *MP4_ConvertMacCode( uint16_t );

/* Return time in microsecond of a track */
static inline int64_t MP4_TrackGetDTS( demux_t *p_demux, mp4_track_t *p_track )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    mp4_chunk_t chunk;
    if( p_sys->b_fragmented )
        chunk = *p_track->cchunk;
    else
        chunk = p_track->chunk[p_track->i_chunk];

    unsigned int i_index = 0;
    unsigned int i_sample = p_track->i_sample - chunk.i_sample_first;
    int64_t i_dts = chunk.i_first_dts;

    while( i_sample > 0 )
    {
        if( i_sample > chunk.p_sample_count_dts[i_index] )
        {
            i_dts += chunk.p_sample_count_dts[i_index] *
                chunk.p_sample_delta_dts[i_index];
            i_sample -= chunk.p_sample_count_dts[i_index];
            i_index++;
        }
        else
        {
            i_dts += i_sample * chunk.p_sample_delta_dts[i_index];
            break;
        }
    }

    /* now handle elst */
    if( p_track->p_elst )
    {
        demux_sys_t         *p_sys = p_demux->p_sys;
        MP4_Box_data_elst_t *elst = p_track->p_elst->data.p_elst;

        /* convert to offset */
        if( ( elst->i_media_rate_integer[p_track->i_elst] > 0 ||
              elst->i_media_rate_fraction[p_track->i_elst] > 0 ) &&
            elst->i_media_time[p_track->i_elst] > 0 )
        {
            i_dts -= elst->i_media_time[p_track->i_elst];
        }

        /* add i_elst_time */
        i_dts += p_track->i_elst_time * p_track->i_timescale /
            p_sys->i_timescale;

        if( i_dts < 0 ) i_dts = 0;
    }

    return INT64_C(1000000) * i_dts / p_track->i_timescale;
}

static inline int64_t MP4_TrackGetPTSDelta( demux_t *p_demux, mp4_track_t *p_track )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    mp4_chunk_t *ck;
    if( p_sys->b_fragmented )
        ck = p_track->cchunk;
    else
        ck = &p_track->chunk[p_track->i_chunk];

    unsigned int i_index = 0;
    unsigned int i_sample = p_track->i_sample - ck->i_sample_first;

    if( ck->p_sample_count_pts == NULL || ck->p_sample_offset_pts == NULL )
        return -1;

    for( i_index = 0;; i_index++ )
    {
        if( i_sample < ck->p_sample_count_pts[i_index] )
            return ck->p_sample_offset_pts[i_index] * INT64_C(1000000) /
                   (int64_t)p_track->i_timescale;

        i_sample -= ck->p_sample_count_pts[i_index];
    }
}

static inline int64_t MP4_GetMoviePTS(demux_sys_t *p_sys )
{
    return INT64_C(1000000) * p_sys->i_time / p_sys->i_timescale;
}

static void LoadChapter( demux_t  *p_demux );

static int LoadInitFrag( demux_t *p_demux, const bool b_smooth )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if( b_smooth ) /* Smooth Streaming */
    {
        if( ( p_sys->p_root = MP4_BoxGetSmooBox( p_demux->s ) ) == NULL )
        {
            goto LoadInitFragError;
        }
        else
        {
            MP4_Box_t *p_smoo = MP4_BoxGet( p_sys->p_root, "uuid" );
            if( !p_smoo || CmpUUID( &p_smoo->i_uuid, &SmooBoxUUID ) )
                goto LoadInitFragError;
            /* Get number of tracks */
            p_sys->i_tracks = 0;
            for( int i = 0; i < 3; i++ )
            {
                MP4_Box_t *p_stra = MP4_BoxGet( p_smoo, "uuid[%d]", i );
                if( p_stra && p_stra->data.p_stra->i_track_ID )
                    p_sys->i_tracks++;
                /* Get timescale and duration of the video track; */
                if( p_sys->i_timescale == 0 )
                {
                    p_sys->i_timescale = p_stra->data.p_stra->i_timescale;
                    p_sys->i_duration = p_stra->data.p_stra->i_duration;
                    if( p_sys->i_timescale == 0 )
                    {
                        msg_Err( p_demux, "bad timescale" );
                        goto LoadInitFragError;
                    }
                }
            }
        }
    }
    else
    {
        /* Load all boxes ( except raw data ) */
        if( ( p_sys->p_root = MP4_BoxGetRoot( p_demux->s ) ) == NULL )
        {
            goto LoadInitFragError;
        }
    }
    return VLC_SUCCESS;

LoadInitFragError:
    msg_Warn( p_demux, "MP4 plugin discarded (not a valid initialization chunk)" );
    return VLC_EGENERIC;
}

static int InitTracks( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    p_sys->track = calloc( p_sys->i_tracks, sizeof( mp4_track_t ) );
    if( p_sys->track == NULL )
        return VLC_EGENERIC;

    if( p_sys->b_fragmented )
    {
        mp4_track_t *p_track;
        for( uint16_t i = 0; i < p_sys->i_tracks; i++ )
        {
            p_track = &p_sys->track[i];
            p_track->cchunk = calloc( 1, sizeof( mp4_chunk_t ) );
            if( unlikely( !p_track->cchunk ) )
            {
                free( p_sys->track );
                return VLC_EGENERIC;
            }
        }
    }
    return VLC_SUCCESS;
}

static void CreateTracksFromSmooBox( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    MP4_Box_t *p_smoo = MP4_BoxGet( p_sys->p_root, "uuid" );
    mp4_track_t *p_track;
    int j = 0;
    for( int i = 0; i < 3; i++ )
    {
        MP4_Box_t *p_stra = MP4_BoxGet( p_smoo, "uuid[%d]", i );
        if( !p_stra || p_stra->data.p_stra->i_track_ID == 0 )
            continue;
        else
        {
            p_track = &p_sys->track[j]; j++;
            MP4_frg_TrackCreate( p_demux, p_track, p_stra );
            p_track->p_es = es_out_Add( p_demux->out, &p_track->fmt );
        }
    }
}

/*****************************************************************************
 * Open: check file and initializes MP4 structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t  *p_demux = (demux_t *)p_this;
    demux_sys_t     *p_sys;

    const uint8_t   *p_peek;

    MP4_Box_t       *p_ftyp;
    MP4_Box_t       *p_rmra;
    MP4_Box_t       *p_mvhd;
    MP4_Box_t       *p_trak;

    unsigned int    i;
    bool      b_seekable;
    bool      b_enabled_es;

    /* A little test to see if it could be a mp4 */
    if( stream_Peek( p_demux->s, &p_peek, 11 ) < 11 ) return VLC_EGENERIC;

    switch( VLC_FOURCC( p_peek[4], p_peek[5], p_peek[6], p_peek[7] ) )
    {
        case ATOM_moov:
        case ATOM_foov:
        case ATOM_moof:
        case ATOM_mdat:
        case ATOM_udta:
        case ATOM_free:
        case ATOM_skip:
        case ATOM_wide:
        case ATOM_uuid:
        case VLC_FOURCC( 'p', 'n', 'o', 't' ):
            break;
        case ATOM_ftyp:
            /* We don't yet support f4v, but avformat does. */
            if( p_peek[8] == 'f' && p_peek[9] == '4' && p_peek[10] == 'v' )
                return VLC_EGENERIC;
            break;
         default:
            return VLC_EGENERIC;
    }

    /* I need to seek */
    stream_Control( p_demux->s, STREAM_CAN_SEEK, &b_seekable );
    if( !b_seekable )
    {
        msg_Warn( p_demux, "MP4 plugin discarded (not seekable)" );
        return VLC_EGENERIC;
    }

    /*Set exported functions */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;

    /* create our structure that will contains all data */
    p_demux->p_sys = p_sys = calloc( 1, sizeof( demux_sys_t ) );

    /* Is it Smooth Streaming? */
    bool b_smooth = false;
    if( stream_Peek( p_demux->s, &p_peek, 24 ) < 24 ) return VLC_EGENERIC;
    if( !CmpUUID( (UUID_t *)(p_peek + 8), &SmooBoxUUID ) )
    {
        b_smooth = true;
        p_sys->b_fragmented = true;
    }

    if( LoadInitFrag( p_demux, b_smooth ) != VLC_SUCCESS )
        goto error;

    if( MP4_BoxCount( p_sys->p_root, "/moov/mvex" ) > 0 )
    {
        p_sys->b_fragmented = true;
    }
    if( p_sys->b_fragmented )
    {
        p_demux->pf_demux = DemuxFrg;
    }

    stream_Control( p_demux->s, STREAM_CAN_FASTSEEK, &b_seekable );
    if( b_smooth )
    {
        if( InitTracks( p_demux ) != VLC_SUCCESS )
            goto error;
        CreateTracksFromSmooBox( p_demux );
        return VLC_SUCCESS;
    }
    else if( p_sys->b_fragmented && b_seekable )
    {
        /* We are not yet able to demux a fragmented MP4 file, using the 'mfra'
         * box. So if STREAM_CAN_FASTSEEK is true, we're assuming we've got such
         * a file, and we let avformat do the job. */
        msg_Warn( p_demux, "MP4 plugin discarded "\
                "(fast-seekable and fragmented, let avformat demux it)" );
        stream_Seek( p_demux->s, 0 ); /* rewind, for other demux */
        goto error;
    }
    else if( !p_sys->b_fragmented && !b_seekable )
    {
        msg_Warn( p_demux, "MP4 plugin discarded (not fast-seekable)" );
        stream_Seek( p_demux->s, 0 ); /* rewind, for other demux */
        goto error;
    }

    MP4_BoxDumpStructure( p_demux->s, p_sys->p_root );

    if( ( p_ftyp = MP4_BoxGet( p_sys->p_root, "/ftyp" ) ) )
    {
        switch( p_ftyp->data.p_ftyp->i_major_brand )
        {
            case( ATOM_isom ):
                msg_Dbg( p_demux,
                         "ISO Media file (isom) version %d.",
                         p_ftyp->data.p_ftyp->i_minor_version );
                break;
            case( ATOM_3gp4 ):
            case( VLC_FOURCC( '3', 'g', 'p', '5' ) ):
            case( VLC_FOURCC( '3', 'g', 'p', '6' ) ):
            case( VLC_FOURCC( '3', 'g', 'p', '7' ) ):
                msg_Dbg( p_demux, "3GPP Media file Release: %c",
#ifdef WORDS_BIGENDIAN
                        p_ftyp->data.p_ftyp->i_major_brand
#else
                        p_ftyp->data.p_ftyp->i_major_brand >> 24
#endif
                        );
                break;
            case( VLC_FOURCC( 'q', 't', ' ', ' ') ):
                msg_Dbg( p_demux, "Apple QuickTime file" );
                break;
            case( VLC_FOURCC( 'i', 's', 'm', 'l') ):
                msg_Dbg( p_demux, "PIFF (= isml = fMP4) file" );
                break;
            default:
                msg_Dbg( p_demux,
                         "unrecognized major file specification (%4.4s).",
                          (char*)&p_ftyp->data.p_ftyp->i_major_brand );
                break;
        }
    }
    else
    {
        msg_Dbg( p_demux, "file type box missing (assuming ISO Media file)" );
    }

    /* the file need to have one moov box */
    if( MP4_BoxCount( p_sys->p_root, "/moov" ) <= 0 )
    {
        MP4_Box_t *p_foov = MP4_BoxGet( p_sys->p_root, "/foov" );

        if( !p_foov )
        {
            msg_Err( p_demux, "MP4 plugin discarded (no moov,foov,moof box)" );
            goto error;
        }
        /* we have a free box as a moov, rename it */
        p_foov->i_type = ATOM_moov;
    }

    if( ( p_rmra = MP4_BoxGet( p_sys->p_root,  "/moov/rmra" ) ) )
    {
        int        i_count = MP4_BoxCount( p_rmra, "rmda" );
        int        i;

        msg_Dbg( p_demux, "detected playlist mov file (%d ref)", i_count );

        input_thread_t *p_input = demux_GetParentInput( p_demux );
        input_item_t *p_current = input_GetItem( p_input );

        input_item_node_t *p_subitems = input_item_node_Create( p_current );

        for( i = 0; i < i_count; i++ )
        {
            MP4_Box_t *p_rdrf = MP4_BoxGet( p_rmra, "rmda[%d]/rdrf", i );
            char      *psz_ref;
            uint32_t  i_ref_type;

            if( !p_rdrf || !( psz_ref = strdup( p_rdrf->data.p_rdrf->psz_ref ) ) )
            {
                continue;
            }
            i_ref_type = p_rdrf->data.p_rdrf->i_ref_type;

            msg_Dbg( p_demux, "new ref=`%s' type=%4.4s",
                     psz_ref, (char*)&i_ref_type );

            if( i_ref_type == VLC_FOURCC( 'u', 'r', 'l', ' ' ) )
            {
                if( strstr( psz_ref, "qt5gateQT" ) )
                {
                    msg_Dbg( p_demux, "ignoring pseudo ref =`%s'", psz_ref );
                    continue;
                }
                if( !strncmp( psz_ref, "http://", 7 ) ||
                    !strncmp( psz_ref, "rtsp://", 7 ) )
                {
                    ;
                }
                else
                {
                    char *psz_absolute;
                    char *psz_path = strdup( p_demux->psz_location );
                    char *end = strrchr( psz_path, '/' );
                    if( end ) end[1] = '\0';
                    else *psz_path = '\0';

                    if( asprintf( &psz_absolute, "%s://%s%s",
                                  p_demux->psz_access, psz_path, psz_ref ) < 0 )
                    {
                        free( psz_ref );
                        free( psz_path );
                        vlc_object_release( p_input) ;
                        return VLC_ENOMEM;
                    }

                    free( psz_ref );
                    psz_ref = psz_absolute;
                    free( psz_path );
                }
                msg_Dbg( p_demux, "adding ref = `%s'", psz_ref );
                input_item_t *p_item = input_item_New( psz_ref, NULL );
                input_item_CopyOptions( p_current, p_item );
                input_item_node_AppendItem( p_subitems, p_item );
                vlc_gc_decref( p_item );
            }
            else
            {
                msg_Err( p_demux, "unknown ref type=%4.4s FIXME (send a bug report)",
                         (char*)&p_rdrf->data.p_rdrf->i_ref_type );
            }
            free( psz_ref );
        }
        input_item_node_PostAndDelete( p_subitems );
        vlc_object_release( p_input );
    }

    if( !(p_mvhd = MP4_BoxGet( p_sys->p_root, "/moov/mvhd" ) ) )
    {
        if( !p_rmra )
        {
            msg_Err( p_demux, "cannot find /moov/mvhd" );
            goto error;
        }
        else
        {
            msg_Warn( p_demux, "cannot find /moov/mvhd (pure ref file)" );
            p_demux->pf_demux = DemuxRef;
            return VLC_SUCCESS;
        }
    }
    else
    {
        p_sys->i_timescale = p_mvhd->data.p_mvhd->i_timescale;
        if( p_sys->i_timescale == 0 )
        {
            msg_Err( p_this, "bad timescale" );
            goto error;
        }
        p_sys->i_duration = p_mvhd->data.p_mvhd->i_duration;
    }

    if( !( p_sys->i_tracks = MP4_BoxCount( p_sys->p_root, "/moov/trak" ) ) )
    {
        msg_Err( p_demux, "cannot find any /moov/trak" );
        goto error;
    }
    msg_Dbg( p_demux, "found %d track%c",
                        p_sys->i_tracks,
                        p_sys->i_tracks ? 's':' ' );

    if( InitTracks( p_demux ) != VLC_SUCCESS )
        goto error;

    /* Search the first chap reference (like quicktime) and
     * check that at least 1 stream is enabled */
    p_sys->p_tref_chap = NULL;
    b_enabled_es = false;
    for( i = 0; i < p_sys->i_tracks; i++ )
    {
        MP4_Box_t *p_trak = MP4_BoxGet( p_sys->p_root, "/moov/trak[%d]", i );


        MP4_Box_t *p_tkhd = MP4_BoxGet( p_trak, "tkhd" );
        if( p_tkhd && (p_tkhd->data.p_tkhd->i_flags&MP4_TRACK_ENABLED) )
            b_enabled_es = true;

        MP4_Box_t *p_chap = MP4_BoxGet( p_trak, "tref/chap", i );
        if( p_chap && p_chap->data.p_tref_generic->i_entry_count > 0 && !p_sys->p_tref_chap )
            p_sys->p_tref_chap = p_chap;
    }

    /* now process each track and extract all useful information */
    for( i = 0; i < p_sys->i_tracks; i++ )
    {
        p_trak = MP4_BoxGet( p_sys->p_root, "/moov/trak[%d]", i );
        MP4_TrackCreate( p_demux, &p_sys->track[i], p_trak, !b_enabled_es );

        if( p_sys->track[i].b_ok && !p_sys->track[i].b_chapter )
        {
            const char *psz_cat;
            switch( p_sys->track[i].fmt.i_cat )
            {
                case( VIDEO_ES ):
                    psz_cat = "video";
                    break;
                case( AUDIO_ES ):
                    psz_cat = "audio";
                    break;
                case( SPU_ES ):
                    psz_cat = "subtitle";
                    break;

                default:
                    psz_cat = "unknown";
                    break;
            }

            msg_Dbg( p_demux, "adding track[Id 0x%x] %s (%s) language %s",
                     p_sys->track[i].i_track_ID, psz_cat,
                     p_sys->track[i].b_enable ? "enable":"disable",
                     p_sys->track[i].fmt.psz_language ?
                     p_sys->track[i].fmt.psz_language : "undef" );
        }
        else if( p_sys->track[i].b_ok && p_sys->track[i].b_chapter )
        {
            msg_Dbg( p_demux, "using track[Id 0x%x] for chapter language %s",
                     p_sys->track[i].i_track_ID,
                     p_sys->track[i].fmt.psz_language ?
                     p_sys->track[i].fmt.psz_language : "undef" );
        }
        else
        {
            msg_Dbg( p_demux, "ignoring track[Id 0x%x]",
                     p_sys->track[i].i_track_ID );
        }
    }

    /* */
    LoadChapter( p_demux );

    return VLC_SUCCESS;

error:
    if( p_sys->p_root )
    {
        MP4_BoxFree( p_demux->s, p_sys->p_root );
    }
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Demux: read packet and send them to decoders
 *****************************************************************************
 * TODO check for newly selected track (ie audio upt to now )
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    unsigned int i_track;


    unsigned int i_track_selected;

    /* check for newly selected/unselected track */
    for( i_track = 0, i_track_selected = 0; i_track < p_sys->i_tracks;
         i_track++ )
    {
        mp4_track_t *tk = &p_sys->track[i_track];
        bool b;

        if( !tk->b_ok || tk->b_chapter ||
            ( tk->b_selected && tk->i_sample >= tk->i_sample_count ) )
        {
            continue;
        }

        es_out_Control( p_demux->out, ES_OUT_GET_ES_STATE, tk->p_es, &b );

        if( tk->b_selected && !b )
        {
            MP4_TrackUnselect( p_demux, tk );
        }
        else if( !tk->b_selected && b)
        {
            MP4_TrackSelect( p_demux, tk, MP4_GetMoviePTS( p_sys ) );
        }

        if( tk->b_selected )
        {
            i_track_selected++;
        }
    }

    if( i_track_selected <= 0 )
    {
        p_sys->i_time += __MAX( p_sys->i_timescale / 10 , 1 );
        if( p_sys->i_timescale > 0 )
        {
            int64_t i_length = (mtime_t)1000000 *
                               (mtime_t)p_sys->i_duration /
                               (mtime_t)p_sys->i_timescale;
            if( MP4_GetMoviePTS( p_sys ) >= i_length )
                return 0;
            return 1;
        }

        msg_Warn( p_demux, "no track selected, exiting..." );
        return 0;
    }

    /* */
    MP4_UpdateSeekpoint( p_demux );

    /* first wait for the good time to read a packet */
    es_out_Control( p_demux->out, ES_OUT_SET_PCR, VLC_TS_0 + p_sys->i_pcr );

    p_sys->i_pcr = MP4_GetMoviePTS( p_sys );

    /* we will read 100ms for each stream so ...*/
    p_sys->i_time += __MAX( p_sys->i_timescale / 10 , 1 );

    for( i_track = 0; i_track < p_sys->i_tracks; i_track++ )
    {
        mp4_track_t *tk = &p_sys->track[i_track];

        if( !tk->b_ok || tk->b_chapter || !tk->b_selected || tk->i_sample >= tk->i_sample_count )
            continue;

        while( MP4_TrackGetDTS( p_demux, tk ) < MP4_GetMoviePTS( p_sys ) )
        {
#if 0
            msg_Dbg( p_demux, "tk(%i)=%lld mv=%lld", i_track,
                     MP4_TrackGetDTS( p_demux, tk ),
                     MP4_GetMoviePTS( p_sys ) );
#endif

            if( MP4_TrackSampleSize( tk ) > 0 )
            {
                block_t *p_block;
                int64_t i_delta;

                /* go,go go ! */
                if( stream_Seek( p_demux->s, MP4_TrackGetPos( tk ) ) )
                {
                    msg_Warn( p_demux, "track[0x%x] will be disabled (eof?)",
                              tk->i_track_ID );
                    MP4_TrackUnselect( p_demux, tk );
                    break;
                }

                /* now read pes */
                if( !(p_block =
                         stream_Block( p_demux->s, MP4_TrackSampleSize(tk) )) )
                {
                    msg_Warn( p_demux, "track[0x%x] will be disabled (eof?)",
                              tk->i_track_ID );
                    MP4_TrackUnselect( p_demux, tk );
                    break;
                }

                else if( tk->fmt.i_cat == SPU_ES )
                {
                    if( tk->fmt.i_codec == VLC_CODEC_SUBT &&
                        p_block->i_buffer >= 2 )
                    {
                        size_t i_size = GetWBE( p_block->p_buffer );

                        if( i_size + 2 <= p_block->i_buffer )
                        {
                            char *p;
                            /* remove the length field, and append a '\0' */
                            memmove( &p_block->p_buffer[0],
                                     &p_block->p_buffer[2], i_size );
                            p_block->p_buffer[i_size] = '\0';
                            p_block->i_buffer = i_size + 1;

                            /* convert \r -> \n */
                            while( ( p = strchr((char *) p_block->p_buffer, '\r' ) ) )
                            {
                                *p = '\n';
                            }
                        }
                        else
                        {
                            /* Invalid */
                            p_block->i_buffer = 0;
                        }
                    }
                }
                /* dts */
                p_block->i_dts = VLC_TS_0 + MP4_TrackGetDTS( p_demux, tk );
                /* pts */
                i_delta = MP4_TrackGetPTSDelta( p_demux, tk );
                if( i_delta != -1 )
                    p_block->i_pts = p_block->i_dts + i_delta;
                else if( tk->fmt.i_cat != VIDEO_ES )
                    p_block->i_pts = p_block->i_dts;
                else
                    p_block->i_pts = VLC_TS_INVALID;

                es_out_Send( p_demux->out, tk->p_es, p_block );
            }

            /* Next sample */
            if( MP4_TrackNextSample( p_demux, tk ) )
                break;
        }
    }

    return 1;
}

static void MP4_UpdateSeekpoint( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int64_t i_time;
    int i;
    if( !p_sys->p_title )
        return;
    i_time = MP4_GetMoviePTS( p_sys );
    for( i = 0; i < p_sys->p_title->i_seekpoint; i++ )
    {
        if( i_time < p_sys->p_title->seekpoint[i]->i_time_offset )
            break;
    }
    i--;

    if( i != p_demux->info.i_seekpoint && i >= 0 )
    {
        p_demux->info.i_seekpoint = i;
        p_demux->info.i_update |= INPUT_UPDATE_SEEKPOINT;
    }
}
/*****************************************************************************
 * Seek: Go to i_date
******************************************************************************/
static int Seek( demux_t *p_demux, mtime_t i_date )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    unsigned int i_track;

    /* First update global time */
    p_sys->i_time = i_date * p_sys->i_timescale / 1000000;
    p_sys->i_pcr  = i_date;

    /* Now for each stream try to go to this time */
    for( i_track = 0; i_track < p_sys->i_tracks; i_track++ )
    {
        mp4_track_t *tk = &p_sys->track[i_track];
        MP4_TrackSeek( p_demux, tk, i_date );
    }
    MP4_UpdateSeekpoint( p_demux );

    es_out_Control( p_demux->out, ES_OUT_SET_NEXT_DISPLAY_TIME, i_date );

    return VLC_SUCCESS;
}

static int MP4_frg_Seek( demux_t *p_demux, double f )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    int64_t i64 = stream_Size( p_demux->s );
    if( stream_Seek( p_demux->s, (int64_t)(i64 * f) ) )
    {
        return VLC_EGENERIC;
    }
    else
    {
        /* update global time */
        p_sys->i_time = (uint64_t)(f * (double)p_sys->i_duration);
        p_sys->i_pcr  = MP4_GetMoviePTS( p_sys );

        for( unsigned i_track = 0; i_track < p_sys->i_tracks; i_track++ )
        {
            mp4_track_t *tk = &p_sys->track[i_track];

            /* We don't want the current chunk to be flushed */
            tk->cchunk->i_sample = tk->cchunk->i_sample_count;

            /* reset/update some values */
            tk->i_sample = tk->i_sample_first = 0;
            tk->i_first_dts = p_sys->i_time;

            /* We want to discard the current chunk and get the next one at once */
            tk->b_has_non_empty_cchunk = false;
        }
        es_out_Control( p_demux->out, ES_OUT_SET_NEXT_DISPLAY_TIME, p_sys->i_pcr );
        return VLC_SUCCESS;
    }
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    double f, *pf;
    int64_t i64, *pi64;

    switch( i_query )
    {
        case DEMUX_GET_POSITION:
            pf = (double*)va_arg( args, double * );
            if( p_sys->i_duration > 0 )
            {
                *pf = (double)p_sys->i_time / (double)p_sys->i_duration;
            }
            else
            {
                *pf = 0.0;
            }
            return VLC_SUCCESS;

        case DEMUX_SET_POSITION:
            f = (double)va_arg( args, double );
            if( p_sys->b_fragmented )
            {
                return MP4_frg_Seek( p_demux, f );
            }
            else if( p_sys->i_timescale > 0 )
            {
                i64 = (int64_t)( f * (double)1000000 *
                                 (double)p_sys->i_duration /
                                 (double)p_sys->i_timescale );
                return Seek( p_demux, i64 );
            }
            else return VLC_SUCCESS;

        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_sys->i_timescale > 0 )
            {
                *pi64 = (mtime_t)1000000 *
                        (mtime_t)p_sys->i_time /
                        (mtime_t)p_sys->i_timescale;
            }
            else *pi64 = 0;
            return VLC_SUCCESS;

        case DEMUX_SET_TIME:
            i64 = (int64_t)va_arg( args, int64_t );
            return Seek( p_demux, i64 );

        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            if( p_sys->i_timescale > 0 )
            {
                *pi64 = (mtime_t)1000000 *
                        (mtime_t)p_sys->i_duration /
                        (mtime_t)p_sys->i_timescale;
            }
            else *pi64 = 0;
            return VLC_SUCCESS;

        case DEMUX_GET_FPS:
            pf = (double*)va_arg( args, double* );
            *pf = p_sys->f_fps;
            return VLC_SUCCESS;

        case DEMUX_GET_META:
        {
            vlc_meta_t *p_meta = (vlc_meta_t *)va_arg( args, vlc_meta_t*);
            MP4_Box_t  *p_0xa9xxx;

            MP4_Box_t  *p_udta = MP4_BoxGet( p_sys->p_root, "/moov/udta/meta/ilst" );
            if( p_udta == NULL )
            {
                p_udta = MP4_BoxGet( p_sys->p_root, "/moov/udta" );
                if( p_udta == NULL )
                {
                    return VLC_EGENERIC;
                }
            }

            for( p_0xa9xxx = p_udta->p_first; p_0xa9xxx != NULL;
                 p_0xa9xxx = p_0xa9xxx->p_next )
            {

                if( !p_0xa9xxx || !p_0xa9xxx->data.p_0xa9xxx )
                    continue;

                /* FIXME FIXME: should convert from whatever the character
                 * encoding of MP4 meta data is to UTF-8. */
#define SET(fct) do { char *psz_utf = strdup( p_0xa9xxx->data.p_0xa9xxx->psz_text ? p_0xa9xxx->data.p_0xa9xxx->psz_text : "" ); \
    if( psz_utf ) { EnsureUTF8( psz_utf );  \
                    fct( p_meta, psz_utf ); free( psz_utf ); } } while(0)

                /* XXX Becarefull p_udta can have box that are not 0xa9xx */
                switch( p_0xa9xxx->i_type )
                {
                case ATOM_0xa9nam: /* Full name */
                    SET( vlc_meta_SetTitle );
                    break;
                case ATOM_0xa9aut:
                    SET( vlc_meta_SetArtist );
                    break;
                case ATOM_0xa9ART:
                    SET( vlc_meta_SetArtist );
                    break;
                case ATOM_0xa9cpy:
                    SET( vlc_meta_SetCopyright );
                    break;
                case ATOM_0xa9day: /* Creation Date */
                    SET( vlc_meta_SetDate );
                    break;
                case ATOM_0xa9des: /* Description */
                    SET( vlc_meta_SetDescription );
                    break;
                case ATOM_0xa9gen: /* Genre */
                    SET( vlc_meta_SetGenre );
                    break;

                case ATOM_gnre:
                    if( p_0xa9xxx->data.p_gnre->i_genre <= NUM_GENRES )
                        vlc_meta_SetGenre( p_meta, ppsz_genres[p_0xa9xxx->data.p_gnre->i_genre - 1] );
                    break;

                case ATOM_0xa9alb: /* Album */
                    SET( vlc_meta_SetAlbum );
                    break;

                case ATOM_0xa9trk: /* Track */
                    SET( vlc_meta_SetTrackNum );
                    break;
                case ATOM_trkn:
                {
                    char psz_trck[11];
                    snprintf( psz_trck, sizeof( psz_trck ), "%i",
                              p_0xa9xxx->data.p_trkn->i_track_number );
                    vlc_meta_SetTrackNum( p_meta, psz_trck );
                    if( p_0xa9xxx->data.p_trkn->i_track_total > 0 )
                    {
                        snprintf( psz_trck, sizeof( psz_trck ), "%i",
                                  p_0xa9xxx->data.p_trkn->i_track_total );
                        vlc_meta_Set( p_meta, vlc_meta_TrackTotal, psz_trck );
                    }
                    break;
                }
                case ATOM_0xa9cmt: /* Commment */
                    SET( vlc_meta_SetDescription );
                    break;

                case ATOM_0xa9url: /* URL */
                    SET( vlc_meta_SetURL );
                    break;

                case ATOM_0xa9too: /* Encoder Tool */
                case ATOM_0xa9enc: /* Encoded By */
                    SET( vlc_meta_SetEncodedBy );
                    break;

                case ATOM_0xa9pub:
                    SET( vlc_meta_SetPublisher );
                    break;

                default:
                    break;
                }
#undef SET
                static const struct { uint32_t xa9_type; char metadata[25]; } xa9typetoextrameta[] =
                {
                    { ATOM_0xa9wrt, N_("Writer") },
                    { ATOM_0xa9com, N_("Composer") },
                    { ATOM_0xa9prd, N_("Producer") },
                    { ATOM_0xa9inf, N_("Information") },
                    { ATOM_0xa9dir, N_("Director") },
                    { ATOM_0xa9dis, N_("Disclaimer") },
                    { ATOM_0xa9req, N_("Requirements") },
                    { ATOM_0xa9fmt, N_("Original Format") },
                    { ATOM_0xa9dsa, N_("Display Source As") },
                    { ATOM_0xa9hst, N_("Host Computer") },
                    { ATOM_0xa9prf, N_("Performers") },
                    { ATOM_0xa9ope, N_("Original Performer") },
                    { ATOM_0xa9src, N_("Providers Source Content") },
                    { ATOM_0xa9wrn, N_("Warning") },
                    { ATOM_0xa9swr, N_("Software") },
                    { ATOM_0xa9lyr, N_("Lyrics") },
                    { ATOM_0xa9mak, N_("Record Company") },
                    { ATOM_0xa9mod, N_("Model") },
                    { ATOM_0xa9PRD, N_("Product") },
                    { ATOM_0xa9grp, N_("Grouping") },
                    { ATOM_0xa9gen, N_("Genre") },
                    { ATOM_0xa9st3, N_("Sub-Title") },
                    { ATOM_0xa9arg, N_("Arranger") },
                    { ATOM_0xa9ard, N_("Art Director") },
                    { ATOM_0xa9cak, N_("Copyright Acknowledgement") },
                    { ATOM_0xa9con, N_("Conductor") },
                    { ATOM_0xa9des, N_("Song Description") },
                    { ATOM_0xa9lnt, N_("Liner Notes") },
                    { ATOM_0xa9phg, N_("Phonogram Rights") },
                    { ATOM_0xa9pub, N_("Publisher") },
                    { ATOM_0xa9sne, N_("Sound Engineer") },
                    { ATOM_0xa9sol, N_("Soloist") },
                    { ATOM_0xa9thx, N_("Thanks") },
                    { ATOM_0xa9xpd, N_("Executive Producer") },
                    { 0, "" },
                };
                for( unsigned i = 0; xa9typetoextrameta[i].xa9_type; i++ )
                {
                    if( p_0xa9xxx->i_type == xa9typetoextrameta[i].xa9_type )
                    {
                        char *psz_utf = strdup( p_0xa9xxx->data.p_0xa9xxx->psz_text ? p_0xa9xxx->data.p_0xa9xxx->psz_text : "" );
                        if( psz_utf )
                        {
                             EnsureUTF8( psz_utf );
                             vlc_meta_AddExtra( p_meta, _(xa9typetoextrameta[i].metadata), psz_utf );
                             free( psz_utf );
                        }
                        break;
                    }
                }
            }
            return VLC_SUCCESS;
        }

        case DEMUX_GET_TITLE_INFO:
        {
            input_title_t ***ppp_title = (input_title_t***)va_arg( args, input_title_t*** );
            int *pi_int    = (int*)va_arg( args, int* );
            int *pi_title_offset = (int*)va_arg( args, int* );
            int *pi_seekpoint_offset = (int*)va_arg( args, int* );

            if( !p_sys->p_title )
                return VLC_EGENERIC;

            *pi_int = 1;
            *ppp_title = malloc( sizeof( input_title_t*) );
            (*ppp_title)[0] = vlc_input_title_Duplicate( p_sys->p_title );
            *pi_title_offset = 0;
            *pi_seekpoint_offset = 0;
            return VLC_SUCCESS;
        }
        case DEMUX_SET_TITLE:
        {
            const int i_title = (int)va_arg( args, int );
            if( !p_sys->p_title || i_title != 0 )
                return VLC_EGENERIC;
            return VLC_SUCCESS;
        }
        case DEMUX_SET_SEEKPOINT:
        {
            const int i_seekpoint = (int)va_arg( args, int );
            if( !p_sys->p_title )
                return VLC_EGENERIC;
            return Seek( p_demux, p_sys->p_title->seekpoint[i_seekpoint]->i_time_offset );
        }

        case DEMUX_SET_NEXT_DEMUX_TIME:
        case DEMUX_SET_GROUP:
        case DEMUX_HAS_UNSUPPORTED_META:
        case DEMUX_GET_ATTACHMENTS:
        case DEMUX_GET_PTS_DELAY:
        case DEMUX_CAN_RECORD:
            return VLC_EGENERIC;

        default:
            msg_Warn( p_demux, "control query %u unimplemented", i_query );
            return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close ( vlc_object_t * p_this )
{
    demux_t *  p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;
    unsigned int i_track;

    msg_Dbg( p_demux, "freeing all memory" );

    MP4_BoxFree( p_demux->s, p_sys->p_root );
    for( i_track = 0; i_track < p_sys->i_tracks; i_track++ )
    {
        MP4_TrackDestroy(  &p_sys->track[i_track] );
    }
    FREENULL( p_sys->track );

    if( p_sys->p_title )
        vlc_input_title_Delete( p_sys->p_title );

    free( p_sys );
}



/****************************************************************************
 * Local functions, specific to vlc
 ****************************************************************************/
/* Chapters */
static void LoadChapterGpac( demux_t  *p_demux, MP4_Box_t *p_chpl )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i;

    p_sys->p_title = vlc_input_title_New();
    for( i = 0; i < p_chpl->data.p_chpl->i_chapter; i++ )
    {
        seekpoint_t *s = vlc_seekpoint_New();

        s->psz_name = strdup( p_chpl->data.p_chpl->chapter[i].psz_name );
        EnsureUTF8( s->psz_name );
        s->i_time_offset = p_chpl->data.p_chpl->chapter[i].i_start / 10;
        TAB_APPEND( p_sys->p_title->i_seekpoint, p_sys->p_title->seekpoint, s );
    }
}
static void LoadChapterApple( demux_t  *p_demux, mp4_track_t *tk )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    for( tk->i_sample = 0; tk->i_sample < tk->i_sample_count; tk->i_sample++ )
    {
        const int64_t i_dts = MP4_TrackGetDTS( p_demux, tk );
        const int64_t i_pts_delta = MP4_TrackGetPTSDelta( p_demux, tk );
        const unsigned int i_size = MP4_TrackSampleSize( tk );

        if( i_size > 0 && !stream_Seek( p_demux->s, MP4_TrackGetPos( tk ) ) )
        {
            char p_buffer[256];
            const int i_read = stream_Read( p_demux->s, p_buffer, __MIN( sizeof(p_buffer), i_size ) );
            const int i_len = __MIN( GetWBE(p_buffer), i_read-2 );

            if( i_len > 0 )
            {
                seekpoint_t *s = vlc_seekpoint_New();

                s->psz_name = strndup( &p_buffer[2], i_len );
                EnsureUTF8( s->psz_name );

                s->i_time_offset = i_dts + __MAX( i_pts_delta, 0 );

                if( !p_sys->p_title )
                    p_sys->p_title = vlc_input_title_New();
                TAB_APPEND( p_sys->p_title->i_seekpoint, p_sys->p_title->seekpoint, s );
            }
        }
        if( tk->i_sample+1 >= tk->chunk[tk->i_chunk].i_sample_first +
                              tk->chunk[tk->i_chunk].i_sample_count )
            tk->i_chunk++;
    }
}
static void LoadChapter( demux_t  *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    MP4_Box_t *p_chpl;

    if( ( p_chpl = MP4_BoxGet( p_sys->p_root, "/moov/udta/chpl" ) ) && p_chpl->data.p_chpl->i_chapter > 0 )
    {
        LoadChapterGpac( p_demux, p_chpl );
    }
    else if( p_sys->p_tref_chap )
    {
        MP4_Box_data_tref_generic_t *p_chap = p_sys->p_tref_chap->data.p_tref_generic;
        unsigned int i, j;

        /* Load the first subtitle track like quicktime */
        for( i = 0; i < p_chap->i_entry_count; i++ )
        {
            for( j = 0; j < p_sys->i_tracks; j++ )
            {
                mp4_track_t *tk = &p_sys->track[j];
                if( tk->b_ok && tk->i_track_ID == p_chap->i_track_ID[i] &&
                    tk->fmt.i_cat == SPU_ES && tk->fmt.i_codec == VLC_CODEC_SUBT )
                    break;
            }
            if( j < p_sys->i_tracks )
            {
                LoadChapterApple( p_demux, &p_sys->track[j] );
                break;
            }
        }
    }

    /* Add duration if titles are enabled */
    if( p_sys->p_title )
    {
        p_sys->p_title->i_length = (uint64_t)1000000 *
                       (uint64_t)p_sys->i_duration / (uint64_t)p_sys->i_timescale;
    }
}

/* now create basic chunk data, the rest will be filled by MP4_CreateSamplesIndex */
static int TrackCreateChunksIndex( demux_t *p_demux,
                                   mp4_track_t *p_demux_track )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    if( p_sys->b_fragmented )
        return VLC_SUCCESS;

    MP4_Box_t *p_co64; /* give offset for each chunk, same for stco and co64 */
    MP4_Box_t *p_stsc;

    unsigned int i_chunk;
    unsigned int i_index, i_last;

    if( ( !(p_co64 = MP4_BoxGet( p_demux_track->p_stbl, "stco" ) )&&
          !(p_co64 = MP4_BoxGet( p_demux_track->p_stbl, "co64" ) ) )||
        ( !(p_stsc = MP4_BoxGet( p_demux_track->p_stbl, "stsc" ) ) ))
    {
        return( VLC_EGENERIC );
    }

    p_demux_track->i_chunk_count = p_co64->data.p_co64->i_entry_count;
    if( !p_demux_track->i_chunk_count )
    {
        msg_Warn( p_demux, "no chunk defined" );
        return( VLC_EGENERIC );
    }
    p_demux_track->chunk = calloc( p_demux_track->i_chunk_count,
                                   sizeof( mp4_chunk_t ) );
    if( p_demux_track->chunk == NULL )
    {
        return VLC_ENOMEM;
    }

    /* first we read chunk offset */
    for( i_chunk = 0; i_chunk < p_demux_track->i_chunk_count; i_chunk++ )
    {
        mp4_chunk_t *ck = &p_demux_track->chunk[i_chunk];

        ck->i_offset = p_co64->data.p_co64->i_chunk_offset[i_chunk];

        ck->i_first_dts = 0;
        ck->p_sample_count_dts = NULL;
        ck->p_sample_delta_dts = NULL;
        ck->p_sample_count_pts = NULL;
        ck->p_sample_offset_pts = NULL;
    }

    /* now we read index for SampleEntry( soun vide mp4a mp4v ...)
        to be used for the sample XXX begin to 1
        We construct it begining at the end */
    i_last = p_demux_track->i_chunk_count; /* last chunk proceded */
    i_index = p_stsc->data.p_stsc->i_entry_count;
    if( !i_index )
    {
        msg_Warn( p_demux, "cannot read chunk table or table empty" );
        return( VLC_EGENERIC );
    }

    while( i_index-- )
    {
        for( i_chunk = p_stsc->data.p_stsc->i_first_chunk[i_index] - 1;
             i_chunk < i_last; i_chunk++ )
        {
            if( i_chunk >= p_demux_track->i_chunk_count )
            {
                msg_Warn( p_demux, "corrupted chunk table" );
                return VLC_EGENERIC;
            }

            p_demux_track->chunk[i_chunk].i_sample_description_index =
                    p_stsc->data.p_stsc->i_sample_description_index[i_index];
            p_demux_track->chunk[i_chunk].i_sample_count =
                    p_stsc->data.p_stsc->i_samples_per_chunk[i_index];
        }
        i_last = p_stsc->data.p_stsc->i_first_chunk[i_index] - 1;
    }

    p_demux_track->chunk[0].i_sample_first = 0;
    for( i_chunk = 1; i_chunk < p_demux_track->i_chunk_count; i_chunk++ )
    {
        p_demux_track->chunk[i_chunk].i_sample_first =
            p_demux_track->chunk[i_chunk-1].i_sample_first +
                p_demux_track->chunk[i_chunk-1].i_sample_count;
    }

    msg_Dbg( p_demux, "track[Id 0x%x] read %d chunk",
             p_demux_track->i_track_ID, p_demux_track->i_chunk_count );

    return VLC_SUCCESS;
}

static int TrackCreateSamplesIndex( demux_t *p_demux,
                                    mp4_track_t *p_demux_track )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    if( p_sys->b_fragmented )
        return VLC_SUCCESS;

    MP4_Box_t *p_box;
    MP4_Box_data_stsz_t *stsz;
    MP4_Box_data_stts_t *stts;
    /* TODO use also stss and stsh table for seeking */
    /* FIXME use edit table */
    int64_t i_sample;
    int64_t i_chunk;

    int64_t i_index;
    int64_t i_index_sample_used;

    int64_t i_next_dts;

    /* Find stsz
     *  Gives the sample size for each samples. There is also a stz2 table
     *  (compressed form) that we need to implement TODO */
    p_box = MP4_BoxGet( p_demux_track->p_stbl, "stsz" );
    if( !p_box )
    {
        /* FIXME and stz2 */
        msg_Warn( p_demux, "cannot find STSZ box" );
        return VLC_EGENERIC;
    }
    stsz = p_box->data.p_stsz;

    /* Find stts
     *  Gives mapping between sample and decoding time
     */
    p_box = MP4_BoxGet( p_demux_track->p_stbl, "stts" );
    if( !p_box )
    {
        msg_Warn( p_demux, "cannot find STTS box" );
        return VLC_EGENERIC;
    }
    stts = p_box->data.p_stts;

    /* Use stsz table to create a sample number -> sample size table */
    p_demux_track->i_sample_count = stsz->i_sample_count;
    if( stsz->i_sample_size )
    {
        /* 1: all sample have the same size, so no need to construct a table */
        p_demux_track->i_sample_size = stsz->i_sample_size;
        p_demux_track->p_sample_size = NULL;
    }
    else
    {
        /* 2: each sample can have a different size */
        p_demux_track->i_sample_size = 0;
        p_demux_track->p_sample_size =
            calloc( p_demux_track->i_sample_count, sizeof( uint32_t ) );
        if( p_demux_track->p_sample_size == NULL )
            return VLC_ENOMEM;

        for( i_sample = 0; i_sample < p_demux_track->i_sample_count; i_sample++ )
        {
            p_demux_track->p_sample_size[i_sample] =
                    stsz->i_entry_size[i_sample];
        }
    }

    /* Use stts table to create a sample number -> dts table.
     * XXX: if we don't want to waste too much memory, we can't expand
     *  the box! so each chunk will contain an "extract" of this table
     *  for fast research (problem with raw stream where a sample is sometime
     *  just channels*bits_per_sample/8 */

    i_next_dts = 0;
    i_index = 0; i_index_sample_used = 0;
    for( i_chunk = 0; i_chunk < p_demux_track->i_chunk_count; i_chunk++ )
    {
        mp4_chunk_t *ck = &p_demux_track->chunk[i_chunk];
        int64_t i_entry, i_sample_count, i;

        /* save first dts */
        ck->i_first_dts = i_next_dts;
        ck->i_last_dts  = i_next_dts;

        /* count how many entries are needed for this chunk
         * for p_sample_delta_dts and p_sample_count_dts */
        i_sample_count = ck->i_sample_count;

        i_entry = 0;
        while( i_sample_count > 0 )
        {
            i_sample_count -= stts->i_sample_count[i_index+i_entry];
            /* don't count already used sample in this entry */
            if( i_entry == 0 )
                i_sample_count += i_index_sample_used;

            i_entry++;
        }

        /* allocate them */
        ck->p_sample_count_dts = calloc( i_entry, sizeof( uint32_t ) );
        ck->p_sample_delta_dts = calloc( i_entry, sizeof( uint32_t ) );

        if( !ck->p_sample_count_dts || !ck->p_sample_delta_dts )
            return VLC_ENOMEM;

        /* now copy */
        i_sample_count = ck->i_sample_count;
        for( i = 0; i < i_entry; i++ )
        {
            int64_t i_used;
            int64_t i_rest;

            i_rest = stts->i_sample_count[i_index] - i_index_sample_used;

            i_used = __MIN( i_rest, i_sample_count );

            i_index_sample_used += i_used;
            i_sample_count -= i_used;
            i_next_dts += i_used * stts->i_sample_delta[i_index];

            ck->p_sample_count_dts[i] = i_used;
            ck->p_sample_delta_dts[i] = stts->i_sample_delta[i_index];
            if( i_used > 0 )
                ck->i_last_dts = i_next_dts - ck->p_sample_delta_dts[i];

            if( i_index_sample_used >= stts->i_sample_count[i_index] )
            {
                i_index++;
                i_index_sample_used = 0;
            }
        }
    }

    /* Find ctts
     *  Gives the delta between decoding time (dts) and composition table (pts)
     */
    p_box = MP4_BoxGet( p_demux_track->p_stbl, "ctts" );
    if( p_box )
    {
        MP4_Box_data_ctts_t *ctts = p_box->data.p_ctts;

        msg_Warn( p_demux, "CTTS table" );

        /* Create pts-dts table per chunk */
        i_index = 0; i_index_sample_used = 0;
        for( i_chunk = 0; i_chunk < p_demux_track->i_chunk_count; i_chunk++ )
        {
            mp4_chunk_t *ck = &p_demux_track->chunk[i_chunk];
            int64_t i_entry, i_sample_count, i;

            /* count how many entries are needed for this chunk
             * for p_sample_delta_dts and p_sample_count_dts */
            i_sample_count = ck->i_sample_count;

            i_entry = 0;
            while( i_sample_count > 0 )
            {
                i_sample_count -= ctts->i_sample_count[i_index+i_entry];

                /* don't count already used sample in this entry */
                if( i_entry == 0 )
                    i_sample_count += i_index_sample_used;

                i_entry++;
            }

            /* allocate them */
            ck->p_sample_count_pts = calloc( i_entry, sizeof( uint32_t ) );
            ck->p_sample_offset_pts = calloc( i_entry, sizeof( int32_t ) );
            if( !ck->p_sample_count_pts || !ck->p_sample_offset_pts )
                return VLC_ENOMEM;

            /* now copy */
            i_sample_count = ck->i_sample_count;
            for( i = 0; i < i_entry; i++ )
            {
                int64_t i_used;
                int64_t i_rest;

                i_rest = ctts->i_sample_count[i_index] -
                    i_index_sample_used;

                i_used = __MIN( i_rest, i_sample_count );

                i_index_sample_used += i_used;
                i_sample_count -= i_used;

                ck->p_sample_count_pts[i] = i_used;
                ck->p_sample_offset_pts[i] = ctts->i_sample_offset[i_index];

                if( i_index_sample_used >= ctts->i_sample_count[i_index] )
                {
                    i_index++;
                    i_index_sample_used = 0;
                }
            }
        }
    }

    msg_Dbg( p_demux, "track[Id 0x%x] read %d samples length:%"PRId64"s",
             p_demux_track->i_track_ID, p_demux_track->i_sample_count,
             i_next_dts / p_demux_track->i_timescale );

    return VLC_SUCCESS;
}

/**
 * It computes the sample rate for a video track using the given sample
 * description index
 */
static void TrackGetESSampleRate( unsigned *pi_num, unsigned *pi_den,
                                  const mp4_track_t *p_track,
                                  unsigned i_sd_index,
                                  unsigned i_chunk )
{
    *pi_num = 0;
    *pi_den = 0;

    if( p_track->i_chunk_count <= 0 )
        return;

    /* */
    const mp4_chunk_t *p_chunk = &p_track->chunk[i_chunk];
    while( p_chunk > &p_track->chunk[0] &&
           p_chunk[-1].i_sample_description_index == i_sd_index )
    {
        p_chunk--;
    }

    uint64_t i_sample = 0;
    uint64_t i_first_dts = p_chunk->i_first_dts;
    uint64_t i_last_dts;
    do
    {
        i_sample += p_chunk->i_sample_count;
        i_last_dts = p_chunk->i_last_dts;
        p_chunk++;
    }
    while( p_chunk < &p_track->chunk[p_track->i_chunk_count] &&
           p_chunk->i_sample_description_index == i_sd_index );

    if( i_sample > 1 && i_first_dts < i_last_dts )
        vlc_ureduce( pi_num, pi_den,
                     ( i_sample - 1) *  p_track->i_timescale,
                     i_last_dts - i_first_dts,
                     UINT16_MAX);
}

/*
 * TrackCreateES:
 * Create ES and PES to init decoder if needed, for a track starting at i_chunk
 */
static int TrackCreateES( demux_t *p_demux, mp4_track_t *p_track,
                          unsigned int i_chunk, es_out_id_t **pp_es )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    unsigned int i_sample_description_index;

    if( p_sys->b_fragmented )
        i_sample_description_index = 1; /* XXX */
    else
        i_sample_description_index =
                p_track->chunk[i_chunk].i_sample_description_index;

    MP4_Box_t   *p_sample;
    MP4_Box_t   *p_esds;
    MP4_Box_t   *p_frma;
    MP4_Box_t   *p_enda;
    MP4_Box_t   *p_pasp;

    if( pp_es )
        *pp_es = NULL;

    if( !i_sample_description_index )
    {
        msg_Warn( p_demux, "invalid SampleEntry index (track[Id 0x%x])",
                  p_track->i_track_ID );
        return VLC_EGENERIC;
    }

    p_sample = MP4_BoxGet(  p_track->p_stsd, "[%d]",
                            i_sample_description_index - 1 );

    if( !p_sample ||
        ( !p_sample->data.p_data && p_track->fmt.i_cat != SPU_ES ) )
    {
        msg_Warn( p_demux, "cannot find SampleEntry (track[Id 0x%x])",
                  p_track->i_track_ID );
        return VLC_EGENERIC;
    }

    p_track->p_sample = p_sample;

    if( ( p_frma = MP4_BoxGet( p_track->p_sample, "sinf/frma" ) ) )
    {
        msg_Warn( p_demux, "Original Format Box: %4.4s", (char *)&p_frma->data.p_frma->i_type );

        p_sample->i_type = p_frma->data.p_frma->i_type;
    }

    p_enda = MP4_BoxGet( p_sample, "wave/enda" );
    if( !p_enda )
        p_enda = MP4_BoxGet( p_sample, "enda" );

    p_pasp = MP4_BoxGet( p_sample, "pasp" );

    /* */
    switch( p_track->fmt.i_cat )
    {
    case VIDEO_ES:
        p_track->fmt.video.i_width = p_sample->data.p_sample_vide->i_width;
        p_track->fmt.video.i_height = p_sample->data.p_sample_vide->i_height;
        p_track->fmt.video.i_bits_per_pixel =
            p_sample->data.p_sample_vide->i_depth;

        /* fall on display size */
        if( p_track->fmt.video.i_width <= 0 )
            p_track->fmt.video.i_width = p_track->i_width;
        if( p_track->fmt.video.i_height <= 0 )
            p_track->fmt.video.i_height = p_track->i_height;

        /* Find out apect ratio from display size */
        if( p_track->i_width > 0 && p_track->i_height > 0 &&
            /* Work-around buggy muxed files */
            p_sample->data.p_sample_vide->i_width != p_track->i_width )
        {
            p_track->fmt.video.i_sar_num = p_track->i_width  * p_track->fmt.video.i_height;
            p_track->fmt.video.i_sar_den = p_track->i_height * p_track->fmt.video.i_width;
        }
        if( p_pasp && p_pasp->data.p_pasp->i_horizontal_spacing > 0 &&
                      p_pasp->data.p_pasp->i_vertical_spacing > 0 )
        {
            p_track->fmt.video.i_sar_num = p_pasp->data.p_pasp->i_horizontal_spacing;
            p_track->fmt.video.i_sar_den = p_pasp->data.p_pasp->i_vertical_spacing;
        }

        /* Support for cropping (eg. in H263 files) */
        p_track->fmt.video.i_visible_width = p_track->fmt.video.i_width;
        p_track->fmt.video.i_visible_height = p_track->fmt.video.i_height;

        /* Frame rate */
        TrackGetESSampleRate( &p_track->fmt.video.i_frame_rate,
                              &p_track->fmt.video.i_frame_rate_base,
                              p_track, i_sample_description_index, i_chunk );
        p_demux->p_sys->f_fps = (float)p_track->fmt.video.i_frame_rate /
                                (float)p_track->fmt.video.i_frame_rate_base;

        /* Rotation */
        switch( (int)p_track->f_rotation ) {
            case 90:
                p_track->fmt.video.orientation = ORIENT_ROTATED_90;
                break;
            case 180:
                p_track->fmt.video.orientation = ORIENT_ROTATED_180;
                break;
            case 270:
                p_track->fmt.video.orientation = ORIENT_ROTATED_270;
                break;
        }

        break;

    case AUDIO_ES:
        p_track->fmt.audio.i_channels =
            p_sample->data.p_sample_soun->i_channelcount;
        p_track->fmt.audio.i_rate =
            p_sample->data.p_sample_soun->i_sampleratehi;
        p_track->fmt.i_bitrate = p_sample->data.p_sample_soun->i_channelcount *
            p_sample->data.p_sample_soun->i_sampleratehi *
                p_sample->data.p_sample_soun->i_samplesize;
        p_track->fmt.audio.i_bitspersample =
            p_sample->data.p_sample_soun->i_samplesize;

        if( ( p_track->i_sample_size == 1 || p_track->i_sample_size == 2 ) )
        {
            MP4_Box_data_sample_soun_t *p_soun;
            p_soun = p_sample->data.p_sample_soun;

            if( p_soun->i_qt_version == 0 )
            {
                switch( p_sample->i_type )
                {
                    case VLC_FOURCC( 'i', 'm', 'a', '4' ):
                        p_soun->i_qt_version = 1;
                        p_soun->i_sample_per_packet = 64;
                        p_soun->i_bytes_per_packet  = 34;
                        p_soun->i_bytes_per_frame   = 34 * p_soun->i_channelcount;
                        p_soun->i_bytes_per_sample  = 2;
                        break;
                    case VLC_FOURCC( 'M', 'A', 'C', '3' ):
                        p_soun->i_qt_version = 1;
                        p_soun->i_sample_per_packet = 6;
                        p_soun->i_bytes_per_packet  = 2;
                        p_soun->i_bytes_per_frame   = 2 * p_soun->i_channelcount;
                        p_soun->i_bytes_per_sample  = 2;
                        break;
                    case VLC_FOURCC( 'M', 'A', 'C', '6' ):
                        p_soun->i_qt_version = 1;
                        p_soun->i_sample_per_packet = 12;
                        p_soun->i_bytes_per_packet  = 2;
                        p_soun->i_bytes_per_frame   = 2 * p_soun->i_channelcount;
                        p_soun->i_bytes_per_sample  = 2;
                        break;
                    case VLC_FOURCC( 'a', 'l', 'a', 'w' ):
                    case VLC_FOURCC( 'u', 'l', 'a', 'w' ):
                        p_soun->i_samplesize = 8;
                        p_track->i_sample_size = p_soun->i_channelcount;
                        break;
                    case VLC_FOURCC( 'N', 'O', 'N', 'E' ):
                    case VLC_FOURCC( 'r', 'a', 'w', ' ' ):
                    case VLC_FOURCC( 't', 'w', 'o', 's' ):
                    case VLC_FOURCC( 's', 'o', 'w', 't' ):
                        /* What would be the fun if you could trust the .mov */
                        p_track->i_sample_size = ((p_soun->i_samplesize+7)/8) * p_soun->i_channelcount;
                        break;
                    default:
                        break;
                }

            }
            else if( p_soun->i_qt_version == 1 && p_soun->i_sample_per_packet <= 0 )
            {
                p_soun->i_qt_version = 0;
            }
        }
        else if( p_sample->data.p_sample_soun->i_qt_version == 1 )
        {
            MP4_Box_data_sample_soun_t *p_soun = p_sample->data.p_sample_soun;

            switch( p_sample->i_type )
            {
                case( VLC_FOURCC( '.', 'm', 'p', '3' ) ):
                case( VLC_FOURCC( 'm', 's', 0x00, 0x55 ) ):
                    {
                        if( p_track->i_sample_size > 1 )
                            p_soun->i_qt_version = 0;
                        break;
                    }
                case( VLC_FOURCC( 'a', 'c', '-', '3' ) ):
                case( VLC_FOURCC( 'e', 'c', '-', '3' ) ):
                case( VLC_FOURCC( 'm', 's', 0x20, 0x00 ) ):
                    p_soun->i_qt_version = 0;
                    break;
                default:
                    break;
            }
        }

        if( p_track->i_sample_size != 0 &&
                p_sample->data.p_sample_soun->i_qt_version == 1 &&
                p_sample->data.p_sample_soun->i_sample_per_packet <= 0 )
        {
            msg_Err( p_demux, "Invalid sample per packet value for qt_version 1. Broken muxer!" );
            p_sample->data.p_sample_soun->i_qt_version = 0;
        }
        break;

    default:
        break;
    }


    /* It's a little ugly but .. there are special cases */
    switch( p_sample->i_type )
    {
        case( VLC_FOURCC( '.', 'm', 'p', '3' ) ):
        case( VLC_FOURCC( 'm', 's', 0x00, 0x55 ) ):
        {
            p_track->fmt.i_codec = VLC_CODEC_MPGA;
            break;
        }
        case( VLC_FOURCC( 'a', 'c', '-', '3' ) ):
        {
            MP4_Box_t *p_dac3_box = MP4_BoxGet(  p_sample, "dac3", 0 );

            p_track->fmt.i_codec = VLC_CODEC_A52;
            if( p_dac3_box )
            {
                static const int pi_bitrate[] = {
                     32,  40,  48,  56,
                     64,  80,  96, 112,
                    128, 160, 192, 224,
                    256, 320, 384, 448,
                    512, 576, 640,
                };
                MP4_Box_data_dac3_t *p_dac3 = p_dac3_box->data.p_dac3;
                p_track->fmt.audio.i_channels = 0;
                p_track->fmt.i_bitrate = 0;
                if( p_dac3->i_bitrate_code < sizeof(pi_bitrate)/sizeof(*pi_bitrate) )
                    p_track->fmt.i_bitrate = pi_bitrate[p_dac3->i_bitrate_code] * 1000;
                p_track->fmt.audio.i_bitspersample = 0;
            }
            break;
        }
        case( VLC_FOURCC( 'e', 'c', '-', '3' ) ):
        {
            p_track->fmt.i_codec = VLC_CODEC_EAC3;
            break;
        }

        case( VLC_FOURCC( 'r', 'a', 'w', ' ' ) ):
        case( VLC_FOURCC( 'N', 'O', 'N', 'E' ) ):
        {
            MP4_Box_data_sample_soun_t *p_soun = p_sample->data.p_sample_soun;

            if(p_soun && (p_soun->i_samplesize+7)/8 == 1 )
                p_track->fmt.i_codec = VLC_FOURCC( 'u', '8', ' ', ' ' );
            else
                p_track->fmt.i_codec = VLC_FOURCC( 't', 'w', 'o', 's' );

            /* Buggy files workaround */
            if( p_sample->data.p_sample_soun && (p_track->i_timescale !=
                p_sample->data.p_sample_soun->i_sampleratehi) )
            {
                MP4_Box_data_sample_soun_t *p_soun =
                    p_sample->data.p_sample_soun;

                msg_Warn( p_demux, "i_timescale (%"PRIu64") != i_sampleratehi "
                          "(%u), making both equal (report any problem).",
                          p_track->i_timescale, p_soun->i_sampleratehi );

                if( p_soun->i_sampleratehi != 0 )
                    p_track->i_timescale = p_soun->i_sampleratehi;
                else
                    p_soun->i_sampleratehi = p_track->i_timescale;
            }
            break;
        }

        case( VLC_FOURCC( 's', '2', '6', '3' ) ):
            p_track->fmt.i_codec = VLC_CODEC_H263;
            break;

        case( VLC_FOURCC( 't', 'e', 'x', 't' ) ):
        case( VLC_FOURCC( 't', 'x', '3', 'g' ) ):
            p_track->fmt.i_codec = VLC_CODEC_SUBT;
            /* FIXME: Not true, could be UTF-16 with a Byte Order Mark (0xfeff) */
            /* FIXME UTF-8 doesn't work here ? */
            if( p_track->b_mac_encoding )
                p_track->fmt.subs.psz_encoding = strdup( "MAC" );
            else
                p_track->fmt.subs.psz_encoding = strdup( "UTF-8" );
            break;

        case VLC_FOURCC('y','v','1','2'):
            p_track->fmt.i_codec = VLC_CODEC_YV12;
            break;
        case VLC_FOURCC('y','u','v','2'):
            p_track->fmt.i_codec = VLC_FOURCC('Y','U','Y','2');
            break;

        case VLC_FOURCC('i','n','2','4'):
            p_track->fmt.i_codec = p_enda && p_enda->data.p_enda->i_little_endian == 1 ?
                                    VLC_FOURCC('4','2','n','i') : VLC_FOURCC('i','n','2','4');
            break;
        case VLC_FOURCC('f','l','3','2'):
            p_track->fmt.i_codec = p_enda && p_enda->data.p_enda->i_little_endian == 1 ?
                                    VLC_CODEC_F32L : VLC_CODEC_F32B;
            break;
        case VLC_FOURCC('f','l','6','4'):
            p_track->fmt.i_codec = p_enda && p_enda->data.p_enda->i_little_endian == 1 ?
                                    VLC_CODEC_F64L : VLC_CODEC_F64B;
            break;
        case VLC_FOURCC( 'l', 'p', 'c', 'm' ):
        {
            MP4_Box_data_sample_soun_t *p_soun = p_sample->data.p_sample_soun;
            if( p_soun->i_qt_version == 2 &&
                p_soun->i_qt_description > 20 + 28 )
            {
                /* Flags:
                 *  0x01: IsFloat
                 *  0x02: IsBigEndian
                 *  0x04: IsSigned
                 */
                static const struct {
                    unsigned     i_flags;
                    unsigned     i_mask;
                    unsigned     i_bits;
                    vlc_fourcc_t i_codec;
                } p_formats[] = {
                    { 0x01,           0x03, 32, VLC_CODEC_F32L },
                    { 0x01,           0x03, 64, VLC_CODEC_F64L },
                    { 0x01|0x02,      0x03, 32, VLC_CODEC_F32B },
                    { 0x01|0x02,      0x03, 64, VLC_CODEC_F64B },

                    { 0x00,           0x05,  8, VLC_CODEC_U8 },
                    { 0x00|     0x04, 0x05,  8, VLC_CODEC_S8 },

                    { 0x00,           0x07, 16, VLC_CODEC_U16L },
                    { 0x00|0x02,      0x07, 16, VLC_CODEC_U16B },
                    { 0x00     |0x04, 0x07, 16, VLC_CODEC_S16L },
                    { 0x00|0x02|0x04, 0x07, 16, VLC_CODEC_S16B },

                    { 0x00,           0x07, 24, VLC_CODEC_U24L },
                    { 0x00|0x02,      0x07, 24, VLC_CODEC_U24B },
                    { 0x00     |0x04, 0x07, 24, VLC_CODEC_S24L },
                    { 0x00|0x02|0x04, 0x07, 24, VLC_CODEC_S24B },

                    { 0x00,           0x07, 32, VLC_CODEC_U32L },
                    { 0x00|0x02,      0x07, 32, VLC_CODEC_U32B },
                    { 0x00     |0x04, 0x07, 32, VLC_CODEC_S32L },
                    { 0x00|0x02|0x04, 0x07, 32, VLC_CODEC_S32B },

                    {0, 0, 0, 0}
                };
                uint32_t i_bits  = GetDWBE(&p_soun->p_qt_description[20 + 20]);
                uint32_t i_flags = GetDWBE(&p_soun->p_qt_description[20 + 24]);

                for( int i = 0; p_formats[i].i_codec; i++ )
                {
                    if( p_formats[i].i_bits == i_bits &&
                        (i_flags & p_formats[i].i_mask) == p_formats[i].i_flags )
                    {
                        p_track->fmt.i_codec = p_formats[i].i_codec;
                        p_track->fmt.audio.i_bitspersample = i_bits;
                        p_track->fmt.audio.i_blockalign = p_soun->i_channelcount * i_bits / 8;
                        p_track->i_sample_size = p_track->fmt.audio.i_blockalign;

                        p_soun->i_qt_version = 0;
                        break;
                    }
                }
            }
            break;
        }
        default:
            p_track->fmt.i_codec = p_sample->i_type;
            break;
    }

    /* now see if esds is present and if so create a data packet
        with decoder_specific_info  */
#define p_decconfig p_esds->data.p_esds->es_descriptor.p_decConfigDescr
    if( ( ( p_esds = MP4_BoxGet( p_sample, "esds" ) ) ||
          ( p_esds = MP4_BoxGet( p_sample, "wave/esds" ) ) )&&
        ( p_esds->data.p_esds )&&
        ( p_decconfig ) )
    {
        /* First update information based on i_objectTypeIndication */
        switch( p_decconfig->i_objectTypeIndication )
        {
            case( 0x20 ): /* MPEG4 VIDEO */
                p_track->fmt.i_codec = VLC_CODEC_MP4V;
                break;
            case( 0x21 ): /* H.264 */
                p_track->fmt.i_codec = VLC_CODEC_H264;
                break;
            case( 0x40):
                p_track->fmt.i_codec = VLC_CODEC_MP4A;
                if( p_decconfig->i_decoder_specific_info_len >= 2 &&
                     p_decconfig->p_decoder_specific_info[0]       == 0xF8 &&
                    (p_decconfig->p_decoder_specific_info[1]&0xE0) == 0x80 )
                {
                    p_track->fmt.i_codec = VLC_CODEC_ALS;
                }
                break;
            case( 0x60):
            case( 0x61):
            case( 0x62):
            case( 0x63):
            case( 0x64):
            case( 0x65): /* MPEG2 video */
                p_track->fmt.i_codec = VLC_CODEC_MPGV;
                break;
            /* Theses are MPEG2-AAC */
            case( 0x66): /* main profile */
            case( 0x67): /* Low complexity profile */
            case( 0x68): /* Scaleable Sampling rate profile */
                p_track->fmt.i_codec = VLC_CODEC_MP4A;
                break;
            /* True MPEG 2 audio */
            case( 0x69):
                p_track->fmt.i_codec = VLC_CODEC_MPGA;
                break;
            case( 0x6a): /* MPEG1 video */
                p_track->fmt.i_codec = VLC_FOURCC( 'm','p','g','v' );
                break;
            case( 0x6b): /* MPEG1 audio */
                p_track->fmt.i_codec = VLC_CODEC_MPGA;
                break;
            case( 0x6c ): /* jpeg */
                p_track->fmt.i_codec = VLC_FOURCC( 'j','p','e','g' );
                break;
            case( 0x6d ): /* png */
                p_track->fmt.i_codec = VLC_FOURCC( 'p','n','g',' ' );
                break;
            case( 0x6e ): /* jpeg2000 */
                p_track->fmt.i_codec = VLC_FOURCC( 'M','J','2','C' );
                break;
            case( 0xa3 ): /* vc1 */
                p_track->fmt.i_codec = VLC_CODEC_VC1;
                break;
            case( 0xa4 ):
                p_track->fmt.i_codec = VLC_CODEC_DIRAC;
                break;
            case( 0xa5 ):
                p_track->fmt.i_codec = VLC_CODEC_A52;
                break;
            case( 0xa6 ):
                p_track->fmt.i_codec = VLC_CODEC_EAC3;
                break;
            case( 0xa9 ): /* dts */
            case( 0xaa ): /* DTS-HD HRA */
            case( 0xab ): /* DTS-HD Master Audio */
                p_track->fmt.i_codec = VLC_CODEC_DTS;
                break;
            case( 0xDD ):
                p_track->fmt.i_codec = VLC_CODEC_VORBIS;
                break;

            /* Private ID */
            case( 0xe0 ): /* NeroDigital: dvd subs */
                if( p_track->fmt.i_cat == SPU_ES )
                {
                    p_track->fmt.i_codec = VLC_FOURCC( 's','p','u',' ' );
                    if( p_track->i_width > 0 )
                        p_track->fmt.subs.spu.i_original_frame_width = p_track->i_width;
                    if( p_track->i_height > 0 )
                        p_track->fmt.subs.spu.i_original_frame_height = p_track->i_height;
                    break;
                }
            case( 0xe1 ): /* QCelp for 3gp */
                if( p_track->fmt.i_cat == AUDIO_ES )
                {
                    p_track->fmt.i_codec = VLC_FOURCC( 'Q','c','l','p' );
                }
                break;

            /* Fallback */
            default:
                /* Unknown entry, but don't touch i_fourcc */
                msg_Warn( p_demux,
                          "unknown objectTypeIndication(0x%x) (Track[ID 0x%x])",
                          p_decconfig->i_objectTypeIndication,
                          p_track->i_track_ID );
                break;
        }
        p_track->fmt.i_extra = p_decconfig->i_decoder_specific_info_len;
        if( p_track->fmt.i_extra > 0 )
        {
            p_track->fmt.p_extra = malloc( p_track->fmt.i_extra );
            memcpy( p_track->fmt.p_extra, p_decconfig->p_decoder_specific_info,
                    p_track->fmt.i_extra );
        }
    }
    else
    {
        switch( p_sample->i_type )
        {
            /* qt decoder, send the complete chunk */
            case VLC_FOURCC ('h', 'd', 'v', '1'): // HDV 720p30
            case VLC_FOURCC ('h', 'd', 'v', '2'): // HDV 1080i60
            case VLC_FOURCC ('h', 'd', 'v', '3'): // HDV 1080i50
            case VLC_FOURCC ('h', 'd', 'v', '5'): // HDV 720p25
            case VLC_FOURCC ('m', 'x', '5', 'n'): // MPEG2 IMX NTSC 525/60 50mb/s produced by FCP
            case VLC_FOURCC ('m', 'x', '5', 'p'): // MPEG2 IMX PAL 625/60 50mb/s produced by FCP
            case VLC_FOURCC ('m', 'x', '4', 'n'): // MPEG2 IMX NTSC 525/60 40mb/s produced by FCP
            case VLC_FOURCC ('m', 'x', '4', 'p'): // MPEG2 IMX PAL 625/60 40mb/s produced by FCP
            case VLC_FOURCC ('m', 'x', '3', 'n'): // MPEG2 IMX NTSC 525/60 30mb/s produced by FCP
            case VLC_FOURCC ('m', 'x', '3', 'p'): // MPEG2 IMX PAL 625/50 30mb/s produced by FCP
            case VLC_FOURCC ('x', 'd', 'v', '2'): // XDCAM HD 1080i60
            case VLC_FOURCC ('A', 'V', 'm', 'p'): // AVID IMX PAL
                p_track->fmt.i_codec = VLC_FOURCC( 'm','p','g','v' );
                break;
            /* qt decoder, send the complete chunk */
            case VLC_FOURCC( 'S', 'V', 'Q', '3' ):
            case VLC_FOURCC( 'S', 'V', 'Q', '1' ):
            case VLC_FOURCC( 'V', 'P', '3', '1' ):
            case VLC_FOURCC( '3', 'I', 'V', '1' ):
            case VLC_FOURCC( 'Z', 'y', 'G', 'o' ):
                p_track->fmt.i_extra =
                    p_sample->data.p_sample_vide->i_qt_image_description;
                if( p_track->fmt.i_extra > 0 )
                {
                    p_track->fmt.p_extra = malloc( p_track->fmt.i_extra );
                    memcpy( p_track->fmt.p_extra,
                            p_sample->data.p_sample_vide->p_qt_image_description,
                            p_track->fmt.i_extra);
                }
                break;

            case VLC_CODEC_AMR_NB:
                p_track->fmt.audio.i_rate = 8000;
                break;
            case VLC_CODEC_AMR_WB:
                p_track->fmt.audio.i_rate = 16000;
                break;
            case VLC_FOURCC( 'Q', 'D', 'M', 'C' ):
            case VLC_CODEC_QDM2:
            case VLC_CODEC_ALAC:
                p_track->fmt.i_extra =
                    p_sample->data.p_sample_soun->i_qt_description;
                if( p_track->fmt.i_extra > 0 )
                {
                    p_track->fmt.p_extra = malloc( p_track->fmt.i_extra );
                    memcpy( p_track->fmt.p_extra,
                            p_sample->data.p_sample_soun->p_qt_description,
                            p_track->fmt.i_extra);
                }
                if( p_track->fmt.i_extra == 56 && p_sample->i_type == VLC_CODEC_ALAC )
                {
                    p_track->fmt.audio.i_channels = *((uint8_t*)p_track->fmt.p_extra + 41);
                    p_track->fmt.audio.i_rate = GetDWBE((uint8_t*)p_track->fmt.p_extra + 52);
                }
                break;

            case VLC_FOURCC( 'v', 'c', '-', '1' ):
            {
                MP4_Box_t *p_dvc1 = MP4_BoxGet( p_sample, "dvc1" );
                if( p_dvc1 )
                {
                    p_track->fmt.i_extra = p_dvc1->data.p_dvc1->i_vc1;
                    if( p_track->fmt.i_extra > 0 )
                    {
                        p_track->fmt.p_extra = malloc( p_dvc1->data.p_dvc1->i_vc1 );
                        memcpy( p_track->fmt.p_extra, p_dvc1->data.p_dvc1->p_vc1,
                                p_track->fmt.i_extra );
                    }
                }
                else
                {
                    msg_Err( p_demux, "missing dvc1" );
                }
                break;
            }

            /* avc1: send avcC (h264 without annexe B, ie without start code)*/
            case VLC_FOURCC( 'a', 'v', 'c', '1' ):
            {
                MP4_Box_t *p_avcC = MP4_BoxGet( p_sample, "avcC" );

                if( p_avcC )
                {
                    p_track->fmt.i_extra = p_avcC->data.p_avcC->i_avcC;
                    if( p_track->fmt.i_extra > 0 )
                    {
                        p_track->fmt.p_extra = malloc( p_avcC->data.p_avcC->i_avcC );
                        memcpy( p_track->fmt.p_extra, p_avcC->data.p_avcC->p_avcC,
                                p_track->fmt.i_extra );
                    }
                }
                else
                {
                    msg_Err( p_demux, "missing avcC" );
                }
                break;
            }
            case VLC_FOURCC( 'h', 'v', 'c', '1' ):
            {
                MP4_Box_t *p_hvcC = MP4_BoxGet( p_sample, "hvcC" );

                if( p_hvcC )
                {
                    p_track->fmt.i_extra = p_hvcC->data.p_hvcC->i_hvcC;
                    if( p_track->fmt.i_extra > 0 )
                    {
                        p_track->fmt.p_extra = malloc( p_hvcC->data.p_hvcC->i_hvcC );
                        memcpy( p_track->fmt.p_extra, p_hvcC->data.p_hvcC->p_hvcC,
                                p_track->fmt.i_extra );
                    }
                    p_track->fmt.i_codec = VLC_CODEC_HEVC;
                }
                else
                {
                    msg_Err( p_demux, "missing hvcC" );
                }
                break;
            }


            case VLC_CODEC_ADPCM_MS:
            case VLC_CODEC_ADPCM_IMA_WAV:
            case VLC_CODEC_QCELP:
                p_track->fmt.audio.i_blockalign = p_sample->data.p_sample_soun->i_bytes_per_frame;
                break;

            default:
                msg_Dbg( p_demux, "Unrecognized FourCC %4.4s", (char *)&p_sample->i_type );
                break;
        }
    }

#undef p_decconfig

    if( pp_es )
        *pp_es = es_out_Add( p_demux->out, &p_track->fmt );

    return VLC_SUCCESS;
}

/* given a time it return sample/chunk
 * it also update elst field of the track
 */
static int TrackTimeToSampleChunk( demux_t *p_demux, mp4_track_t *p_track,
                                   int64_t i_start, uint32_t *pi_chunk,
                                   uint32_t *pi_sample )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    MP4_Box_t   *p_box_stss;
    uint64_t     i_dts;
    unsigned int i_sample;
    unsigned int i_chunk;
    int          i_index;

    /* FIXME see if it's needed to check p_track->i_chunk_count */
    if( p_track->i_chunk_count == 0 )
        return( VLC_EGENERIC );

    /* handle elst (find the correct one) */
    MP4_TrackSetELST( p_demux, p_track, i_start );
    if( p_track->p_elst && p_track->p_elst->data.p_elst->i_entry_count > 0 )
    {
        MP4_Box_data_elst_t *elst = p_track->p_elst->data.p_elst;
        int64_t i_mvt= i_start * p_sys->i_timescale / (int64_t)1000000;

        /* now calculate i_start for this elst */
        /* offset */
        i_start -= p_track->i_elst_time * INT64_C(1000000) / p_sys->i_timescale;
        if( i_start < 0 )
        {
            *pi_chunk = 0;
            *pi_sample= 0;

            return VLC_SUCCESS;
        }
        /* to track time scale */
        i_start  = i_start * p_track->i_timescale / (int64_t)1000000;
        /* add elst offset */
        if( ( elst->i_media_rate_integer[p_track->i_elst] > 0 ||
             elst->i_media_rate_fraction[p_track->i_elst] > 0 ) &&
            elst->i_media_time[p_track->i_elst] > 0 )
        {
            i_start += elst->i_media_time[p_track->i_elst];
        }

        msg_Dbg( p_demux, "elst (%d) gives %"PRId64"ms (movie)-> %"PRId64
                 "ms (track)", p_track->i_elst,
                 i_mvt * 1000 / p_sys->i_timescale,
                 i_start * 1000 / p_track->i_timescale );
    }
    else
    {
        /* convert absolute time to in timescale unit */
        i_start = i_start * p_track->i_timescale / (int64_t)1000000;
    }

    /* we start from sample 0/chunk 0, hope it won't take too much time */
    /* *** find good chunk *** */
    for( i_chunk = 0; ; i_chunk++ )
    {
        if( i_chunk + 1 >= p_track->i_chunk_count )
        {
            /* at the end and can't check if i_start in this chunk,
               it will be check while searching i_sample */
            i_chunk = p_track->i_chunk_count - 1;
            break;
        }

        if( (uint64_t)i_start >= p_track->chunk[i_chunk].i_first_dts &&
            (uint64_t)i_start <  p_track->chunk[i_chunk + 1].i_first_dts )
        {
            break;
        }
    }

    /* *** find sample in the chunk *** */
    i_sample = p_track->chunk[i_chunk].i_sample_first;
    i_dts    = p_track->chunk[i_chunk].i_first_dts;
    for( i_index = 0; i_sample < p_track->chunk[i_chunk].i_sample_count; )
    {
        if( i_dts +
            p_track->chunk[i_chunk].p_sample_count_dts[i_index] *
            p_track->chunk[i_chunk].p_sample_delta_dts[i_index] < (uint64_t)i_start )
        {
            i_dts    +=
                p_track->chunk[i_chunk].p_sample_count_dts[i_index] *
                p_track->chunk[i_chunk].p_sample_delta_dts[i_index];

            i_sample += p_track->chunk[i_chunk].p_sample_count_dts[i_index];
            i_index++;
        }
        else
        {
            if( p_track->chunk[i_chunk].p_sample_delta_dts[i_index] <= 0 )
            {
                break;
            }
            i_sample += ( i_start - i_dts ) /
                p_track->chunk[i_chunk].p_sample_delta_dts[i_index];
            break;
        }
    }

    if( i_sample >= p_track->i_sample_count )
    {
        msg_Warn( p_demux, "track[Id 0x%x] will be disabled "
                  "(seeking too far) chunk=%d sample=%d",
                  p_track->i_track_ID, i_chunk, i_sample );
        return( VLC_EGENERIC );
    }


    /* *** Try to find nearest sync points *** */
    if( ( p_box_stss = MP4_BoxGet( p_track->p_stbl, "stss" ) ) )
    {
        MP4_Box_data_stss_t *p_stss = p_box_stss->data.p_stss;
        msg_Dbg( p_demux, "track[Id 0x%x] using Sync Sample Box (stss)",
                 p_track->i_track_ID );
        for( unsigned i_index = 0; i_index < p_stss->i_entry_count; i_index++ )
        {
            if( i_index >= p_stss->i_entry_count - 1 ||
                i_sample < p_stss->i_sample_number[i_index+1] )
            {
                unsigned i_sync_sample = p_stss->i_sample_number[i_index];
                msg_Dbg( p_demux, "stts gives %d --> %d (sample number)",
                         i_sample, i_sync_sample );

                if( i_sync_sample <= i_sample )
                {
                    while( i_chunk > 0 &&
                           i_sync_sample < p_track->chunk[i_chunk].i_sample_first )
                        i_chunk--;
                }
                else
                {
                    while( i_chunk < p_track->i_chunk_count - 1 &&
                           i_sync_sample >= p_track->chunk[i_chunk].i_sample_first +
                                            p_track->chunk[i_chunk].i_sample_count )
                        i_chunk++;
                }
                i_sample = i_sync_sample;
                break;
            }
        }
    }
    else
    {
        msg_Dbg( p_demux, "track[Id 0x%x] does not provide Sync "
                 "Sample Box (stss)", p_track->i_track_ID );
    }

    *pi_chunk  = i_chunk;
    *pi_sample = i_sample;

    return VLC_SUCCESS;
}

static int TrackGotoChunkSample( demux_t *p_demux, mp4_track_t *p_track,
                                 unsigned int i_chunk, unsigned int i_sample )
{
    bool b_reselect = false;

    /* now see if actual es is ok */
    if( p_track->i_chunk >= p_track->i_chunk_count ||
        p_track->chunk[p_track->i_chunk].i_sample_description_index !=
            p_track->chunk[i_chunk].i_sample_description_index )
    {
        msg_Warn( p_demux, "recreate ES for track[Id 0x%x]",
                  p_track->i_track_ID );

        es_out_Control( p_demux->out, ES_OUT_GET_ES_STATE,
                        p_track->p_es, &b_reselect );

        es_out_Del( p_demux->out, p_track->p_es );

        p_track->p_es = NULL;

        if( TrackCreateES( p_demux, p_track, i_chunk, &p_track->p_es ) )
        {
            msg_Err( p_demux, "cannot create es for track[Id 0x%x]",
                     p_track->i_track_ID );

            p_track->b_ok       = false;
            p_track->b_selected = false;
            return VLC_EGENERIC;
        }
    }

    /* select again the new decoder */
    if( b_reselect )
    {
        es_out_Control( p_demux->out, ES_OUT_SET_ES, p_track->p_es );
    }

    p_track->i_chunk    = i_chunk;
    p_track->i_sample   = i_sample;

    return p_track->b_selected ? VLC_SUCCESS : VLC_EGENERIC;
}

/****************************************************************************
 * MP4_TrackCreate:
 ****************************************************************************
 * Parse track information and create all needed data to run a track
 * If it succeed b_ok is set to 1 else to 0
 ****************************************************************************/
static void MP4_TrackCreate( demux_t *p_demux, mp4_track_t *p_track,
                             MP4_Box_t *p_box_trak,
                             bool b_force_enable )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    MP4_Box_t *p_tkhd = MP4_BoxGet( p_box_trak, "tkhd" );
    MP4_Box_t *p_tref = MP4_BoxGet( p_box_trak, "tref" );
    MP4_Box_t *p_elst;

    MP4_Box_t *p_mdhd;
    MP4_Box_t *p_udta;
    MP4_Box_t *p_hdlr;

    MP4_Box_t *p_vmhd;
    MP4_Box_t *p_smhd;

    char language[4] = { '\0' };

    /* hint track unsupported */

    /* set default value (-> track unusable) */
    p_track->b_ok       = false;
    p_track->b_enable   = false;
    p_track->b_selected = false;
    p_track->b_chapter  = false;
    p_track->b_mac_encoding = false;

    es_format_Init( &p_track->fmt, UNKNOWN_ES, 0 );

    if( !p_tkhd )
    {
        return;
    }

    /* do we launch this track by default ? */
    p_track->b_enable =
        ( ( p_tkhd->data.p_tkhd->i_flags&MP4_TRACK_ENABLED ) != 0 );
    if( !p_track->b_enable )
        p_track->fmt.i_priority = -1;

    p_track->i_track_ID = p_tkhd->data.p_tkhd->i_track_ID;

    p_track->i_width = p_tkhd->data.p_tkhd->i_width / 65536;
    p_track->i_height = p_tkhd->data.p_tkhd->i_height / 65536;
    p_track->f_rotation = p_tkhd->data.p_tkhd->f_rotation;

    if( p_tref )
    {
/*        msg_Warn( p_demux, "unhandled box: tref --> FIXME" ); */
    }

    p_mdhd = MP4_BoxGet( p_box_trak, "mdia/mdhd" );
    p_hdlr = MP4_BoxGet( p_box_trak, "mdia/hdlr" );

    if( ( !p_mdhd )||( !p_hdlr ) )
    {
        return;
    }

    p_track->i_timescale = p_mdhd->data.p_mdhd->i_timescale;
    if( p_track->i_timescale == 0 )
        return;

    if( p_mdhd->data.p_mdhd->i_language_code < 0x400 )
    {
        strcpy( language, MP4_ConvertMacCode( p_mdhd->data.p_mdhd->i_language_code ) );
        p_track->b_mac_encoding = true;
    }
    else if( p_mdhd->data.p_mdhd->i_language_code == 0x7fff )
        p_track->b_mac_encoding = true;
    else
    {
        for( unsigned i = 0; i < 3; i++ )
            language[i] = p_mdhd->data.p_mdhd->i_language[i];
        language[3] = '\0';
    }

    switch( p_hdlr->data.p_hdlr->i_handler_type )
    {
        case( ATOM_soun ):
            if( !( p_smhd = MP4_BoxGet( p_box_trak, "mdia/minf/smhd" ) ) )
            {
                return;
            }
            p_track->fmt.i_cat = AUDIO_ES;
            break;

        case( ATOM_vide ):
            if( !( p_vmhd = MP4_BoxGet( p_box_trak, "mdia/minf/vmhd" ) ) )
            {
                return;
            }
            p_track->fmt.i_cat = VIDEO_ES;
            break;

        case( ATOM_text ):
        case( ATOM_subp ):
        case( ATOM_tx3g ):
        case( ATOM_sbtl ):
            p_track->fmt.i_cat = SPU_ES;
            break;

        default:
            return;
    }

    p_track->i_elst = 0;
    p_track->i_elst_time = 0;
    if( ( p_track->p_elst = p_elst = MP4_BoxGet( p_box_trak, "edts/elst" ) ) )
    {
        MP4_Box_data_elst_t *elst = p_elst->data.p_elst;
        unsigned int i;

        msg_Warn( p_demux, "elst box found" );
        for( i = 0; i < elst->i_entry_count; i++ )
        {
            msg_Dbg( p_demux, "   - [%d] duration=%"PRId64"ms media time=%"PRId64
                     "ms) rate=%d.%d", i,
                     elst->i_segment_duration[i] * 1000 / p_sys->i_timescale,
                     elst->i_media_time[i] >= 0 ?
                     (int64_t)(elst->i_media_time[i] * 1000 / p_track->i_timescale) :
                     INT64_C(-1),
                     elst->i_media_rate_integer[i],
                     elst->i_media_rate_fraction[i] );
        }
    }


/*  TODO
    add support for:
    p_dinf = MP4_BoxGet( p_minf, "dinf" );
*/
    if( !( p_track->p_stbl = MP4_BoxGet( p_box_trak,"mdia/minf/stbl" ) ) ||
        !( p_track->p_stsd = MP4_BoxGet( p_box_trak,"mdia/minf/stbl/stsd") ) )
    {
        return;
    }

    /* Set language */
    if( *language && strcmp( language, "```" ) && strcmp( language, "und" ) )
    {
        p_track->fmt.psz_language = strdup( language );
    }

    p_udta = MP4_BoxGet( p_box_trak, "udta" );
    if( p_udta )
    {
        MP4_Box_t *p_box_iter;
        for( p_box_iter = p_udta->p_first; p_box_iter != NULL;
                 p_box_iter = p_box_iter->p_next )
        {
            switch( p_box_iter->i_type )
            {
                case ATOM_0xa9nam:
                    p_track->fmt.psz_description =
                        strdup( p_box_iter->data.p_0xa9xxx->psz_text );
                    break;
                case ATOM_name:
                    p_track->fmt.psz_description =
                        strdup( p_box_iter->data.p_name->psz_text );
                    break;
            }
        }
    }

    /* Create chunk index table and sample index table */
    if( TrackCreateChunksIndex( p_demux,p_track  ) ||
        TrackCreateSamplesIndex( p_demux, p_track ) )
    {
        return; /* cannot create chunks index */
    }

    p_track->i_chunk  = 0;
    p_track->i_sample = 0;

    /* Mark chapter only track */
    if( p_sys->p_tref_chap )
    {
        MP4_Box_data_tref_generic_t *p_chap = p_sys->p_tref_chap->data.p_tref_generic;
        unsigned int i;

        for( i = 0; i < p_chap->i_entry_count; i++ )
        {
            if( p_track->i_track_ID == p_chap->i_track_ID[i] )
            {
                p_track->b_chapter = true;
                p_track->b_enable = false;
                break;
            }
        }
    }

    /* now create es */
    if( b_force_enable &&
        ( p_track->fmt.i_cat == VIDEO_ES || p_track->fmt.i_cat == AUDIO_ES ) )
    {
        msg_Warn( p_demux, "Enabling track[Id 0x%x] (buggy file without enabled track)",
                  p_track->i_track_ID );
        p_track->b_enable = true;
        p_track->fmt.i_priority = 0;
    }

    p_track->p_es = NULL;
    if( TrackCreateES( p_demux,
                       p_track, p_track->i_chunk,
                       p_track->b_chapter ? NULL : &p_track->p_es ) )
    {
        msg_Err( p_demux, "cannot create es for track[Id 0x%x]",
                 p_track->i_track_ID );
        return;
    }
    p_track->b_ok = true;
#if 0
    {
        int i;
        for( i = 0; i < p_track->i_chunk_count; i++ )
        {
            fprintf( stderr, "%-5d sample_count=%d pts=%lld\n",
                     i, p_track->chunk[i].i_sample_count,
                     p_track->chunk[i].i_first_dts );

        }
    }
#endif
}

static void FreeAndResetChunk( mp4_chunk_t *ck )
{
    free( ck->p_sample_count_dts );
    free( ck->p_sample_delta_dts );
    free( ck->p_sample_count_pts );
    free( ck->p_sample_offset_pts );
    free( ck->p_sample_size );
    for( uint32_t i = 0; i < ck->i_sample_count; i++ )
        free( ck->p_sample_data[i] );
    free( ck->p_sample_data );
    memset( ck, 0, sizeof( mp4_chunk_t ) );
}

/****************************************************************************
 * MP4_TrackDestroy:
 ****************************************************************************
 * Destroy a track created by MP4_TrackCreate.
 ****************************************************************************/
static void MP4_TrackDestroy( mp4_track_t *p_track )
{
    unsigned int i_chunk;

    p_track->b_ok = false;
    p_track->b_enable   = false;
    p_track->b_selected = false;

    es_format_Clean( &p_track->fmt );

    for( i_chunk = 0; i_chunk < p_track->i_chunk_count; i_chunk++ )
    {
        if( p_track->chunk )
        {
           FREENULL(p_track->chunk[i_chunk].p_sample_count_dts);
           FREENULL(p_track->chunk[i_chunk].p_sample_delta_dts );

           FREENULL(p_track->chunk[i_chunk].p_sample_count_pts);
           FREENULL(p_track->chunk[i_chunk].p_sample_offset_pts );
        }
    }
    FREENULL( p_track->chunk );
    if( p_track->cchunk ) {
        FreeAndResetChunk( p_track->cchunk );
        FREENULL( p_track->cchunk );
    }

    if( !p_track->i_sample_size )
    {
        FREENULL( p_track->p_sample_size );
    }
}

static int MP4_TrackSelect( demux_t *p_demux, mp4_track_t *p_track,
                            mtime_t i_start )
{
    if( !p_track->b_ok || p_track->b_chapter )
    {
        return VLC_EGENERIC;
    }

    if( p_track->b_selected )
    {
        msg_Warn( p_demux, "track[Id 0x%x] already selected",
                  p_track->i_track_ID );
        return VLC_SUCCESS;
    }

    return MP4_TrackSeek( p_demux, p_track, i_start );
}

static void MP4_TrackUnselect( demux_t *p_demux, mp4_track_t *p_track )
{
    if( !p_track->b_ok || p_track->b_chapter )
    {
        return;
    }

    if( !p_track->b_selected )
    {
        msg_Warn( p_demux, "track[Id 0x%x] already unselected",
                  p_track->i_track_ID );
        return;
    }
    if( p_track->p_es )
    {
        es_out_Control( p_demux->out, ES_OUT_SET_ES_STATE,
                        p_track->p_es, false );
    }

    p_track->b_selected = false;
}

static int MP4_TrackSeek( demux_t *p_demux, mp4_track_t *p_track,
                          mtime_t i_start )
{
    uint32_t i_chunk;
    uint32_t i_sample;

    if( !p_track->b_ok || p_track->b_chapter )
        return VLC_EGENERIC;

    p_track->b_selected = false;

    if( TrackTimeToSampleChunk( p_demux, p_track, i_start,
                                &i_chunk, &i_sample ) )
    {
        msg_Warn( p_demux, "cannot select track[Id 0x%x]",
                  p_track->i_track_ID );
        return VLC_EGENERIC;
    }

    p_track->b_selected = true;

    if( !TrackGotoChunkSample( p_demux, p_track, i_chunk, i_sample ) )
        p_track->b_selected = true;

    return p_track->b_selected ? VLC_SUCCESS : VLC_EGENERIC;
}


/*
 * 3 types: for audio
 *
 */
#define QT_V0_MAX_SAMPLES 1024
static int MP4_TrackSampleSize( mp4_track_t *p_track )
{
    int i_size;
    MP4_Box_data_sample_soun_t *p_soun;

    if( p_track->i_sample_size == 0 )
    {
        /* most simple case */
        return p_track->p_sample_size[p_track->i_sample];
    }
    if( p_track->fmt.i_cat != AUDIO_ES )
    {
        return p_track->i_sample_size;
    }

    p_soun = p_track->p_sample->data.p_sample_soun;

    if( p_soun->i_qt_version == 1 )
    {
        int i_samples = p_track->chunk[p_track->i_chunk].i_sample_count;
        if( p_track->fmt.audio.i_blockalign > 1 )
            i_samples = p_soun->i_sample_per_packet;

        i_size = i_samples / p_soun->i_sample_per_packet * p_soun->i_bytes_per_frame;
    }
    else if( p_track->i_sample_size > 256 )
    {
        /* We do that so we don't read too much data
         * (in this case we are likely dealing with compressed data) */
        i_size = p_track->i_sample_size;
    }
    else
    {
        /* Read a bunch of samples at once */
        int i_samples = p_track->chunk[p_track->i_chunk].i_sample_count -
            ( p_track->i_sample -
              p_track->chunk[p_track->i_chunk].i_sample_first );

        i_samples = __MIN( QT_V0_MAX_SAMPLES, i_samples );
        i_size = i_samples * p_track->i_sample_size;
    }

    //fprintf( stderr, "size=%d\n", i_size );
    return i_size;
}

static uint64_t MP4_TrackGetPos( mp4_track_t *p_track )
{
    unsigned int i_sample;
    uint64_t i_pos;

    i_pos = p_track->chunk[p_track->i_chunk].i_offset;

    if( p_track->i_sample_size )
    {
        MP4_Box_data_sample_soun_t *p_soun =
            p_track->p_sample->data.p_sample_soun;

        if( p_track->fmt.i_cat != AUDIO_ES || p_soun->i_qt_version == 0 )
        {
            i_pos += ( p_track->i_sample -
                       p_track->chunk[p_track->i_chunk].i_sample_first ) *
                     p_track->i_sample_size;
        }
        else
        {
            /* we read chunk by chunk unless a blockalign is requested */
            if( p_track->fmt.audio.i_blockalign > 1 )
                i_pos += ( p_track->i_sample - p_track->chunk[p_track->i_chunk].i_sample_first ) /
                                p_soun->i_sample_per_packet * p_soun->i_bytes_per_frame;
        }
    }
    else
    {
        for( i_sample = p_track->chunk[p_track->i_chunk].i_sample_first;
             i_sample < p_track->i_sample; i_sample++ )
        {
            i_pos += p_track->p_sample_size[i_sample];
        }
    }

    return i_pos;
}

static int MP4_TrackNextSample( demux_t *p_demux, mp4_track_t *p_track )
{
    if( p_track->fmt.i_cat == AUDIO_ES && p_track->i_sample_size != 0 )
    {
        MP4_Box_data_sample_soun_t *p_soun;

        p_soun = p_track->p_sample->data.p_sample_soun;

        if( p_soun->i_qt_version == 1 )
        {
            /* we read chunk by chunk unless a blockalign is requested */
            if( p_track->fmt.audio.i_blockalign > 1 )
                p_track->i_sample += p_soun->i_sample_per_packet;
            else
                p_track->i_sample += p_track->chunk[p_track->i_chunk].i_sample_count;
        }
        else if( p_track->i_sample_size > 256 )
        {
            /* We do that so we don't read too much data
             * (in this case we are likely dealing with compressed data) */
            p_track->i_sample += 1;
        }
        else
        {
            /* FIXME */
            p_track->i_sample += QT_V0_MAX_SAMPLES;
            if( p_track->i_sample >
                p_track->chunk[p_track->i_chunk].i_sample_first +
                p_track->chunk[p_track->i_chunk].i_sample_count )
            {
                p_track->i_sample =
                    p_track->chunk[p_track->i_chunk].i_sample_first +
                    p_track->chunk[p_track->i_chunk].i_sample_count;
            }
        }
    }
    else
    {
        p_track->i_sample++;
    }

    if( p_track->i_sample >= p_track->i_sample_count )
        return VLC_EGENERIC;

    /* Have we changed chunk ? */
    if( p_track->i_sample >=
            p_track->chunk[p_track->i_chunk].i_sample_first +
            p_track->chunk[p_track->i_chunk].i_sample_count )
    {
        if( TrackGotoChunkSample( p_demux, p_track, p_track->i_chunk + 1,
                                  p_track->i_sample ) )
        {
            msg_Warn( p_demux, "track[0x%x] will be disabled "
                      "(cannot restart decoder)", p_track->i_track_ID );
            MP4_TrackUnselect( p_demux, p_track );
            return VLC_EGENERIC;
        }
    }

    /* Have we changed elst */
    if( p_track->p_elst && p_track->p_elst->data.p_elst->i_entry_count > 0 )
    {
        demux_sys_t *p_sys = p_demux->p_sys;
        MP4_Box_data_elst_t *elst = p_track->p_elst->data.p_elst;
        uint64_t i_mvt = MP4_TrackGetDTS( p_demux, p_track ) *
                        p_sys->i_timescale / (int64_t)1000000;

        if( (unsigned int)p_track->i_elst < elst->i_entry_count &&
            i_mvt >= p_track->i_elst_time +
                     elst->i_segment_duration[p_track->i_elst] )
        {
            MP4_TrackSetELST( p_demux, p_track,
                              MP4_TrackGetDTS( p_demux, p_track ) );
        }
    }

    return VLC_SUCCESS;
}

static void MP4_TrackSetELST( demux_t *p_demux, mp4_track_t *tk,
                              int64_t i_time )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int         i_elst_last = tk->i_elst;

    /* handle elst (find the correct one) */
    tk->i_elst      = 0;
    tk->i_elst_time = 0;
    if( tk->p_elst && tk->p_elst->data.p_elst->i_entry_count > 0 )
    {
        MP4_Box_data_elst_t *elst = tk->p_elst->data.p_elst;
        int64_t i_mvt= i_time * p_sys->i_timescale / (int64_t)1000000;

        for( tk->i_elst = 0; (unsigned int)tk->i_elst < elst->i_entry_count; tk->i_elst++ )
        {
            mtime_t i_dur = elst->i_segment_duration[tk->i_elst];

            if( tk->i_elst_time <= i_mvt && i_mvt < tk->i_elst_time + i_dur )
            {
                break;
            }
            tk->i_elst_time += i_dur;
        }

        if( (unsigned int)tk->i_elst >= elst->i_entry_count )
        {
            /* msg_Dbg( p_demux, "invalid number of entry in elst" ); */
            tk->i_elst = elst->i_entry_count - 1;
            tk->i_elst_time -= elst->i_segment_duration[tk->i_elst];
        }

        if( elst->i_media_time[tk->i_elst] < 0 )
        {
            /* track offset */
            tk->i_elst_time += elst->i_segment_duration[tk->i_elst];
        }
    }
    if( i_elst_last != tk->i_elst )
    {
        msg_Warn( p_demux, "elst old=%d new=%d", i_elst_last, tk->i_elst );
    }
}

/* */
static const char *MP4_ConvertMacCode( uint16_t i_code )
{
    static const struct { const char psz_iso639_1[3]; uint16_t i_code; } p_cvt[] = {
        { "en",   0 }, { "fr",   1 }, { "de",   2 }, { "it",   3 }, { "nl",   4 },
        { "sv",   5 }, { "es",   6 }, { "da",   7 }, { "pt",   8 }, { "no",   9 },
        { "he",  10 }, { "ja",  11 }, { "ar",  12 }, { "fi",  13 }, { "el",  14 },
        { "is",  15 }, { "mt",  16 }, { "tr",  17 }, { "hr",  18 }, { "zh",  19 },
        { "ur",  20 }, { "hi",  21 }, { "th",  22 }, { "ko",  23 }, { "lt",  24 },
        { "pl",  25 }, { "hu",  26 }, { "et",  27 }, { "lv",  28 }, //{ "??",  29 },
        { "fo",  30 }, { "fa",  31 }, { "ru",  32 }, { "zh",  33 }, { "nl",  34 },
        { "ga",  35 }, { "sq",  36 }, { "ro",  37 }, { "cs",  38 }, { "sk",  39 },
        { "sl",  40 }, { "yi",  41 }, { "sr",  42 }, { "mk",  43 }, { "bg",  44 },
        { "uk",  45 }, { "be",  46 }, { "uz",  47 }, { "az",  48 }, { "kk",  48 },
        { "az",  50 }, { "hy",  51 }, { "ka",  52 }, { "mo",  53 }, { "ky",  54 },
        { "tg",  55 }, { "tk",  56 }, { "mn",  57 }, { "mn",  58 }, { "ps",  59 },
        { "ku",  60 }, { "ks",  61 }, { "sd",  62 }, { "bo",  63 }, { "ne",  64 },
        { "sa",  65 }, { "mr",  66 }, { "bn",  67 }, { "as",  68 }, { "gu",  69 },
        { "pa",  70 }, { "or",  71 }, { "ml",  72 }, { "kn",  73 }, { "ta",  74 },
        { "te",  75 }, { "si",  76 }, { "my",  77 }, { "km",  78 }, { "lo",  79 },
        { "vi",  80 }, { "id",  81 }, { "tl",  82 }, { "ms",  83 }, { "ms",  84 },
        { "am",  85 }, { "ti",  86 }, { "om",  87 }, { "so",  88 }, { "sw",  89 },
        { "rw",  90 }, { "rn",  91 }, { "ny",  92 }, { "mg",  93 }, { "eo",  94 },

                                                     { "cy", 128 }, { "eu", 129 },
        { "ca", 130 }, { "la", 131 }, { "qu", 132 }, { "gn", 133 }, { "ay", 134 },
        { "tt", 135 }, { "ug", 136 }, { "dz", 137 }, { "jv", 138 }, { "su", 139 },
        { "gl", 140 }, { "af", 141 }, { "br", 142 }, { "iu", 143 }, { "gd", 144 },
        { "gv", 145 }, { "ga", 146 }, { "to", 147 }, { "el", 148 },
        /* */
        { "", 0 }
    };
    int i;
    for( i = 0; *p_cvt[i].psz_iso639_1; i++ )
    {
        if( p_cvt[i].i_code == i_code )
            return p_cvt[i].psz_iso639_1;
    }
    return "";
}

/******************************************************************************
 *     Here are the functions used for fragmented MP4
 *****************************************************************************/

/**
 * It computes the sample rate for a video track using current video chunk
 */
static void ChunkGetESSampleRate( unsigned *pi_num, unsigned *pi_den,
                                  const mp4_track_t *p_track )
{
    if( p_track->cchunk->i_last_dts == 0 )
        return;

    *pi_num = 0;
    *pi_den = 0;

    /* */
    const mp4_chunk_t *p_chunk = p_track->cchunk;

    uint32_t i_sample = p_chunk->i_sample_count;
    uint64_t i_first_dts = p_chunk->i_first_dts;
    uint64_t i_last_dts =  p_chunk->i_last_dts;

    if( i_sample > 1 && i_first_dts < i_last_dts )
        vlc_ureduce( pi_num, pi_den,
                     ( i_sample - 1) *  p_track->i_timescale,
                     i_last_dts - i_first_dts,
                     UINT16_MAX);
}

/**
 * Build raw avcC box (without the 8 bytes header)
 * \return The size of the box.
 */
static int build_raw_avcC( uint8_t **p_extra, const uint8_t *CodecPrivateData,
                                                       const unsigned cpd_len )
{
    uint8_t *avcC;
    unsigned sps_len = 0, pps_len = 0;
    const uint32_t mark = 0x00000001;

    assert( CodecPrivateData[0] == 0 );
    assert( CodecPrivateData[1] == 0 );
    assert( CodecPrivateData[2] == 0 );
    assert( CodecPrivateData[3] == 1 );

    uint32_t length = cpd_len + 3;
    avcC = calloc( length, 1 );
    if( unlikely( avcC == NULL ) )
        return 0;

    uint8_t *sps = avcC + 8;

    uint32_t candidate = ~mark;
    CodecPrivateData += 4;
    for( unsigned i = 0; i < cpd_len - 4; i++ )
    {
        sps[i] = CodecPrivateData[i];
        candidate = (candidate << 8) | CodecPrivateData[i];
        if( candidate == mark )
        {
            sps_len = i - 3;
            break;
        }
    }
    if( sps_len == 0 )
        return 0;
    uint8_t *pps = sps + sps_len + 3;
    pps_len = cpd_len - sps_len - 4 * 2;
    memcpy( pps, CodecPrivateData + sps_len + 4, pps_len );

    /* XXX */
    uint8_t AVCProfileIndication = 0x64;
    uint8_t profile_compatibility = 0x40;
    uint8_t AVCLevelIndication = 0x1f;
    uint8_t lengthSizeMinusOne = 0x03;

    avcC[0] = 1;
    avcC[1] = AVCProfileIndication;
    avcC[2] = profile_compatibility;
    avcC[3] = AVCLevelIndication;
    avcC[4] = 0xfc + lengthSizeMinusOne;
    avcC[5] = 0xe0 + 1;
    avcC[6] = (sps_len & 0xff00)>>8;
    avcC[7] = sps_len & 0xff;

    avcC[8+sps_len] = 1;
    avcC[9+sps_len] = (pps_len & 0xff00) >> 8;
    avcC[10+sps_len] = pps_len & 0xff;

    *p_extra = avcC;
    return length;
}

/**
 * Build a mp4_track_t from a StraBox
 */

static inline int MP4_SetCodecExtraData( es_format_t *fmt, MP4_Box_data_stra_t *p_data )
{
    fmt->i_extra = p_data->cpd_len;
    fmt->p_extra = malloc( p_data->cpd_len );
    if( unlikely( !fmt->p_extra ) )
        return VLC_ENOMEM;
    memcpy( fmt->p_extra, p_data->CodecPrivateData, p_data->cpd_len );
    return VLC_SUCCESS;
  }

static int MP4_frg_TrackCreate( demux_t *p_demux, mp4_track_t *p_track, MP4_Box_t *p_stra )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int ret;
    MP4_Box_data_stra_t *p_data = p_stra->data.p_stra;
    if( !p_data )
        return VLC_EGENERIC;

    p_track->b_ok       = true;
    p_track->b_selected = false;
    p_track->i_sample_count = UINT32_MAX;

    p_track->i_timescale = p_sys->i_timescale;
    p_track->i_width = p_data->MaxWidth;
    p_track->i_height = p_data->MaxHeight;
    p_track->i_track_ID = p_data->i_track_ID;

    es_format_t *fmt = &p_track->fmt;
    if( fmt == NULL )
        return VLC_EGENERIC;

    es_format_Init( fmt, p_data->i_es_cat, 0 );

    /* Set language FIXME */
    fmt->psz_language = strdup( "en" );

    fmt->i_original_fourcc = p_data->FourCC;
    fmt->i_codec = vlc_fourcc_GetCodec( fmt->i_cat, p_data->FourCC );

    uint8_t **p_extra = (uint8_t **)&fmt->p_extra;
    /* See http://msdn.microsoft.com/en-us/library/ff728116%28v=vs.90%29.aspx
     * for MS weird use of FourCC*/
    switch( fmt->i_cat )
    {
        case VIDEO_ES:
            if( p_data->FourCC == VLC_FOURCC( 'A', 'V', 'C', '1' ) ||
                p_data->FourCC == VLC_FOURCC( 'A', 'V', 'C', 'B' ) ||
                p_data->FourCC == VLC_FOURCC( 'H', '2', '6', '4' ) )
            {
                fmt->i_extra = build_raw_avcC( p_extra,
                        p_data->CodecPrivateData, p_data->cpd_len );
            }
            else
            {
                ret = MP4_SetCodecExtraData( fmt, p_data );
                if( ret != VLC_SUCCESS )
                    return ret;
            }

            fmt->video.i_width = p_data->MaxWidth;
            fmt->video.i_height = p_data->MaxHeight;
            fmt->video.i_bits_per_pixel = 0x18;
            fmt->video.i_visible_width = p_data->MaxWidth;
            fmt->video.i_visible_height = p_data->MaxHeight;

            /* Frame rate */
            ChunkGetESSampleRate( &fmt->video.i_frame_rate,
                                  &fmt->video.i_frame_rate_base, p_track );

            if( fmt->video.i_frame_rate_base != 0 )
            {
                p_demux->p_sys->f_fps = (float)fmt->video.i_frame_rate /
                                        (float)fmt->video.i_frame_rate_base;
            }
            else
                p_demux->p_sys->f_fps = 24;

            break;

        case AUDIO_ES:
            fmt->audio.i_channels = p_data->Channels;
            fmt->audio.i_rate = p_data->SamplingRate;
            fmt->audio.i_bitspersample = p_data->BitsPerSample;
            fmt->audio.i_blockalign = p_data->nBlockAlign;

            fmt->i_bitrate = p_data->Bitrate;

            ret = MP4_SetCodecExtraData( fmt, p_data );
            if( ret != VLC_SUCCESS )
                return ret;

            break;

        default:
            break;
    }

    return VLC_SUCCESS;
}

/**
 * Return the track identified by tid
 */
static mp4_track_t *MP4_frg_GetTrack( demux_t *p_demux, const uint32_t tid )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    mp4_track_t *ret = NULL;
    for( unsigned i = 0; i < p_sys->i_tracks; i++ )
    {
        ret = &p_sys->track[i];
        if( !ret )
            return NULL;
        if( ret->i_track_ID == tid )
            return ret;
    }
    msg_Err( p_demux, "MP4_frg_GetTrack: track %"PRIu32" not found!", tid );
    return NULL;
}

static void FlushChunk( demux_t *p_demux, mp4_track_t *tk )
{
    msg_Dbg( p_demux, "Flushing chunk for track id %u", tk->i_track_ID );
    mp4_chunk_t *ck = tk->cchunk;
    while( ck->i_sample < ck->i_sample_count )
    {
        block_t *p_block;
        int64_t i_delta;

        if( ck->p_sample_size == NULL || ck->p_sample_data == NULL )
            return;

        uint32_t sample_size = ck->p_sample_size[ck->i_sample];
        assert( sample_size > 0 );
        p_block = block_Alloc( sample_size );
        if( unlikely( !p_block ) )
            return;

        uint8_t *src = ck->p_sample_data[ck->i_sample];
        memcpy( p_block->p_buffer, src, sample_size );
        ck->i_sample++;

        /* dts */
        p_block->i_dts = VLC_TS_0 + MP4_TrackGetDTS( p_demux, tk );
        /* pts */
        i_delta = MP4_TrackGetPTSDelta( p_demux, tk );
        if( i_delta != -1 )
            p_block->i_pts = p_block->i_dts + i_delta;
        else if( tk->fmt.i_cat != VIDEO_ES )
            p_block->i_pts = p_block->i_dts;
        else
            p_block->i_pts = VLC_TS_INVALID;

        es_out_Send( p_demux->out, tk->p_es, p_block );

        tk->i_sample++;
    }
}

/**
 * Re-init decoder.
 * \Note If we call that function too soon,
 * before the track has been selected by MP4_TrackSelect
 * (during the first execution of Demux), then the track gets disabled
 */
static int ReInitDecoder( demux_t *p_demux, mp4_track_t *p_track )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    uint32_t i_sample = 0;
    bool b_smooth = false;
    MP4_Box_t *p_stra = NULL, *p_trak = NULL;

    if( !CmpUUID( &p_sys->p_root->p_first->i_uuid, &SmooBoxUUID ) )
        b_smooth = true;

    if( b_smooth )
    {
        p_stra = MP4_BoxGet( p_sys->p_root, "uuid/uuid[0]" );
        if( !p_stra || CmpUUID( &p_stra->i_uuid, &StraBoxUUID ) )
            return VLC_EGENERIC;
    }
    else /* DASH */
    {
        p_trak = MP4_BoxGet( p_sys->p_root, "/moov/trak[0]" );
        if( !p_trak )
            return VLC_EGENERIC;
    }

    i_sample = p_track->i_sample;
    es_out_Del( p_demux->out, p_track->p_es );
    es_format_Clean( &p_track->fmt );

    if( b_smooth )
        MP4_frg_TrackCreate( p_demux, p_track, p_stra );
    else /* DASH */
        MP4_TrackCreate( p_demux, p_track, p_trak, true );

    p_track->i_sample = i_sample;

    /* Temporary hack until we support track selection */
    p_track->b_selected = true;
    p_track->b_ok = true;
    p_track->b_enable = true;

    p_track->p_es = es_out_Add( p_demux->out, &p_track->fmt );
    p_track->b_codec_need_restart = false;

    return VLC_SUCCESS;
}

/**
 * This function fills a mp4_chunk_t structure from a MP4_Box_t (p_chunk).
 * The 'i_tk_id' argument returns the ID of the track the chunk belongs to.
 * \note p_chunk usually contains a 'moof' and a 'mdat', and might contain a 'sidx'.
 * \return VLC_SUCCESS, VLC_EGENERIC or VLC_ENOMEM.
 */
static int MP4_frg_GetChunk( demux_t *p_demux, MP4_Box_t *p_chunk, unsigned *i_tk_id )
{
    MP4_Box_t *p_sidx = MP4_BoxGet( p_chunk, "sidx" );
    MP4_Box_t *p_moof = MP4_BoxGet( p_chunk, "moof" );
    if( p_moof == NULL)
    {
        msg_Warn( p_demux, "no moof box found!" );
        return VLC_EGENERIC;
    }

    MP4_Box_t *p_traf = MP4_BoxGet( p_moof, "traf" );
    if( p_traf == NULL)
    {
        msg_Warn( p_demux, "no traf box found!" );
        return VLC_EGENERIC;
    }

    MP4_Box_t *p_tfhd = MP4_BoxGet( p_traf, "tfhd" );
    if( p_tfhd == NULL)
    {
        msg_Warn( p_demux, "no tfhd box found!" );
        return VLC_EGENERIC;
    }

    uint32_t i_track_ID = p_tfhd->data.p_tfhd->i_track_ID;
    *i_tk_id = i_track_ID;
    assert( i_track_ID > 0 );
    msg_Dbg( p_demux, "GetChunk: track ID is %"PRIu32"", i_track_ID );

    mp4_track_t *p_track = MP4_frg_GetTrack( p_demux, i_track_ID );
    if( !p_track )
        return VLC_EGENERIC;

    mp4_chunk_t *ret = p_track->cchunk;

    if( p_tfhd->data.p_tfhd->b_empty )
        msg_Warn( p_demux, "No samples in this chunk!" );

    /* Usually we read 100 ms of each track. However, suppose we have two tracks,
     * Ta and Tv (audio and video). Suppose also that Ta is the first track to be
     * read, i.e. we read 100 ms of Ta, then 100 ms of Tv, then 100 ms of Ta,
     * and so on. Finally, suppose that we get the chunks the other way around,
     * i.e. first a chunk of Tv, then a chunk of Ta, then a chunk of Tv, and so on.
     * In that case, it is very likely that at some point, Ta->cchunk or Tv->cchunk
     * is not emptied when MP4_frg_GetChunks is called. It is therefore necessary to
     * flush it, i.e. send to the decoder the samples not yet sent.
     * Note that all the samples to be flushed should worth less than 100 ms,
     * (though I did not do the formal proof) and thus this flushing mechanism
     * should not cause A/V sync issues, or delays or whatever.
     */
    if( ret->i_sample < ret->i_sample_count )
        FlushChunk( p_demux, p_track );

    if( ret->i_sample_count )
        FreeAndResetChunk( ret );

    uint32_t default_duration = 0;
    if( p_tfhd->data.p_tfhd->i_flags & MP4_TFHD_DFLT_SAMPLE_DURATION )
        default_duration = p_tfhd->data.p_tfhd->i_default_sample_duration;

    uint32_t default_size = 0;
    if( p_tfhd->data.p_tfhd->i_flags & MP4_TFHD_DFLT_SAMPLE_SIZE )
        default_size = p_tfhd->data.p_tfhd->i_default_sample_size;

    MP4_Box_t *p_trun = MP4_BoxGet( p_traf, "trun");
    if( p_trun == NULL)
    {
        msg_Warn( p_demux, "no trun box found!" );
        return VLC_EGENERIC;
    }
    MP4_Box_data_trun_t *p_trun_data = p_trun->data.p_trun;

    ret->i_sample_count = p_trun_data->i_sample_count;
    assert( ret->i_sample_count > 0 );
    ret->i_sample_description_index = 1; /* seems to be always 1, is it? */
    ret->i_sample_first = p_track->i_sample_first;
    p_track->i_sample_first += ret->i_sample_count;

    ret->i_first_dts = p_track->i_first_dts;

    /* XXX I already saw DASH content with no default_duration and no
     * MP4_TRUN_SAMPLE_DURATION flag, but a sidx box was present.
     * This chunk of code might be buggy with segments having
     * more than one subsegment. */
    if( !default_duration )
    {
        if( p_sidx )
        {
            MP4_Box_data_sidx_t *p_sidx_data = p_sidx->data.p_sidx;
            assert( p_sidx_data->i_reference_count == 1 );

            if( p_sidx_data->i_timescale == 0 )
                return VLC_EGENERIC;

            unsigned i_chunk_duration = p_sidx_data->p_items[0].i_subsegment_duration /
                                        p_sidx_data->i_timescale;
            default_duration = i_chunk_duration * p_track->i_timescale / ret->i_sample_count;

        }
    }

    msg_Dbg( p_demux, "Default sample duration is %"PRIu32, default_duration );

    ret->p_sample_count_dts = calloc( ret->i_sample_count, sizeof( uint32_t ) );
    ret->p_sample_delta_dts = calloc( ret->i_sample_count, sizeof( uint32_t ) );

    if( !ret->p_sample_count_dts || !ret->p_sample_delta_dts )
        return VLC_ENOMEM;

    ret->p_sample_count_pts = calloc( ret->i_sample_count, sizeof( uint32_t ) );
    if( !ret->p_sample_count_pts )
        return VLC_ENOMEM;

    if( p_trun_data->i_flags & MP4_TRUN_SAMPLE_TIME_OFFSET )
    {
        ret->p_sample_offset_pts = calloc( ret->i_sample_count, sizeof( int32_t ) );
        if( !ret->p_sample_offset_pts )
            return VLC_ENOMEM;
    }

    ret->p_sample_size = calloc( ret->i_sample_count, sizeof( uint32_t ) );
    if( !ret->p_sample_size )
        return VLC_ENOMEM;

    ret->p_sample_data = calloc( ret->i_sample_count, sizeof( uint8_t * ) );
    if( !ret->p_sample_data )
        return VLC_ENOMEM;

    uint32_t dur = 0, len;
    uint32_t chunk_duration = 0, chunk_size = 0;

    /* Skip header of mdat */
    stream_Read( p_demux->s, NULL, 8 );

    for( uint32_t i = 0; i < ret->i_sample_count; i++)
    {
        if( p_trun_data->i_flags & MP4_TRUN_SAMPLE_DURATION )
            dur = p_trun_data->p_samples[i].i_duration;
        else
            dur = default_duration;
        ret->p_sample_delta_dts[i] = dur;
        chunk_duration += dur;

        ret->p_sample_count_dts[i] = ret->p_sample_count_pts[i] = 1;

        if( ret->p_sample_offset_pts )
        {
            ret->p_sample_offset_pts[i] =
                        p_trun_data->p_samples[i].i_composition_time_offset;
        }

        if( p_trun_data->i_flags & MP4_TRUN_SAMPLE_SIZE )
            len = ret->p_sample_size[i] = p_trun_data->p_samples[i].i_size;
        else
            len = ret->p_sample_size[i] = default_size;

        ret->p_sample_data[i] = malloc( len );
        if( ret->p_sample_data[i] == NULL )
            return VLC_ENOMEM;
        int read = stream_Read( p_demux->s, ret->p_sample_data[i], len );
        if( read < (int)len )
            return VLC_EGENERIC;
        chunk_size += len;
    }
    ret->i_last_dts = ret->i_first_dts + chunk_duration - dur;
    p_track->i_first_dts = chunk_duration + ret->i_first_dts;

    if( p_track->b_codec_need_restart &&
            p_track->fmt.i_cat == VIDEO_ES )
        ReInitDecoder( p_demux, p_track );

    p_track->b_has_non_empty_cchunk = true;
    return VLC_SUCCESS;
}

/**
 * Get the next chunk of the track identified by i_tk_id.
 * \Note We don't want to seek all the time, so if the first chunk given by the
 * input method doesn't belong to the right track, we don't throw it away,
 * and so, in general, this function fetch more than one chunk.
 * Not to mention that a new init segment might be put everywhere
 * between two chunks by the input method.
 */
static int MP4_frg_GetChunks( demux_t *p_demux, const unsigned i_tk_id )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    mp4_track_t *p_track;

    for( unsigned i = 0; i < p_sys->i_tracks; i++ )
    {
        MP4_Box_t *p_chunk = MP4_BoxGetNextChunk( p_demux->s );
        if( !p_chunk )
            return VLC_EGENERIC;

        if( !p_chunk->p_first )
            goto MP4_frg_GetChunks_Error;
        uint32_t i_type = p_chunk->p_first->i_type;
        uint32_t tid = 0;
        if( i_type == ATOM_uuid || i_type == ATOM_ftyp )
        {
            MP4_BoxFree( p_demux->s, p_sys->p_root );
            p_sys->p_root = p_chunk;

            if( i_type == ATOM_ftyp ) /* DASH */
            {
                MP4_Box_t *p_tkhd = MP4_BoxGet( p_chunk, "/moov/trak[0]/tkhd" );
                if( !p_tkhd )
                {
                    msg_Warn( p_demux, "No tkhd found!" );
                    goto MP4_frg_GetChunks_Error;
                }
                tid = p_tkhd->data.p_tkhd->i_track_ID;
            }
            else                      /* Smooth Streaming */
            {
                assert( !CmpUUID( &p_chunk->p_first->i_uuid, &SmooBoxUUID ) );
                MP4_Box_t *p_stra = MP4_BoxGet( p_chunk, "/uuid/uuid[0]" );
                if( !p_stra || CmpUUID( &p_stra->i_uuid, &StraBoxUUID ) )
                {
                    msg_Warn( p_demux, "No StraBox found!" );
                    goto MP4_frg_GetChunks_Error;
                }
                tid = p_stra->data.p_stra->i_track_ID;
            }

            p_track = MP4_frg_GetTrack( p_demux, tid );
            if( !p_track )
                goto MP4_frg_GetChunks_Error;
            p_track->b_codec_need_restart = true;

            return MP4_frg_GetChunks( p_demux, i_tk_id );
        }

        if( MP4_frg_GetChunk( p_demux, p_chunk, &tid ) != VLC_SUCCESS )
            goto MP4_frg_GetChunks_Error;

        MP4_BoxFree( p_demux->s, p_chunk );

        if( tid == i_tk_id )
            break;
        else
            continue;

MP4_frg_GetChunks_Error:
        MP4_BoxFree( p_demux->s, p_chunk );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static int MP4_frg_TrackSelect( demux_t *p_demux, mp4_track_t *p_track )
{
    if( !p_track->b_ok || p_track->b_chapter )
    {
        return VLC_EGENERIC;
    }

    if( p_track->b_selected )
    {
        msg_Warn( p_demux, "track[Id 0x%x] already selected", p_track->i_track_ID );
        return VLC_SUCCESS;
    }

    msg_Dbg( p_demux, "Select track id %u", p_track->i_track_ID );
    p_track->b_selected = true;
    return VLC_SUCCESS;
}

/**
 * DemuxFrg: read packet and send them to decoders
 * \return 1 on success, 0 on error.
 * TODO check for newly selected track
 */
int DemuxFrg( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    unsigned i_track;
    unsigned i_track_selected;

    /* check for newly selected/unselected track */
    for( i_track = 0, i_track_selected = 0; i_track < p_sys->i_tracks; i_track++ )
    {
        mp4_track_t *tk = &p_sys->track[i_track];
        bool b;

        if( !tk->b_ok || tk->b_chapter )
            continue;

        es_out_Control( p_demux->out, ES_OUT_GET_ES_STATE, tk->p_es, &b );
        msg_Dbg( p_demux, "track %u %s!", tk->i_track_ID, b ? "enabled" : "disabled" );

        if( tk->b_selected && !b )
        {
            MP4_TrackUnselect( p_demux, tk );
        }
        else if( !tk->b_selected && b)
        {
            MP4_frg_TrackSelect( p_demux, tk );
        }

        if( tk->b_selected )
            i_track_selected++;
    }

    if( i_track_selected <= 0 )
    {
        p_sys->i_time += __MAX( p_sys->i_timescale / 10 , 1 );
        if( p_sys->i_timescale > 0 )
        {
            int64_t i_length = (mtime_t)1000000 *
                               (mtime_t)p_sys->i_duration /
                               (mtime_t)p_sys->i_timescale;
            if( MP4_GetMoviePTS( p_sys ) >= i_length )
                return 0;
            return 1;
        }

        msg_Warn( p_demux, "no track selected, exiting..." );
        return 0;
    }

    /* first wait for the good time to read a packet */
    es_out_Control( p_demux->out, ES_OUT_SET_PCR, VLC_TS_0 + p_sys->i_pcr );

    p_sys->i_pcr = MP4_GetMoviePTS( p_sys );

    /* we will read 100ms for each stream so ...*/
    p_sys->i_time += __MAX( p_sys->i_timescale / 10, 1 );

    for( i_track = 0; i_track < p_sys->i_tracks; i_track++ )
    {
        mp4_track_t *tk = &p_sys->track[i_track];

        if( !tk->b_ok || tk->b_chapter || !tk->b_selected )
        {
            msg_Warn( p_demux, "Skipping track id %u", tk->i_track_ID );
            continue;
        }

        if( !tk->b_has_non_empty_cchunk )
        {
            if( MP4_frg_GetChunks( p_demux, tk->i_track_ID ) != VLC_SUCCESS )
            {
                msg_Info( p_demux, "MP4_frg_GetChunks returned error. End of stream?" );
                return 0;
            }
        }

        while( MP4_TrackGetDTS( p_demux, tk ) < MP4_GetMoviePTS( p_sys ) )
        {
            block_t *p_block;
            int64_t i_delta;

            mp4_chunk_t *ck = tk->cchunk;
            if( ck->i_sample >= ck->i_sample_count )
            {
                msg_Err( p_demux, "sample %"PRIu32" of %"PRIu32"",
                                    ck->i_sample, ck->i_sample_count );
                return 0;
            }

            uint32_t sample_size = ck->p_sample_size[ck->i_sample];
            p_block = block_Alloc( sample_size );
            uint8_t *src = ck->p_sample_data[ck->i_sample];
            memcpy( p_block->p_buffer, src, sample_size );

            ck->i_sample++;
            if( ck->i_sample == ck->i_sample_count )
                tk->b_has_non_empty_cchunk = false;

            /* dts */
            p_block->i_dts = VLC_TS_0 + MP4_TrackGetDTS( p_demux, tk );
            /* pts */
            i_delta = MP4_TrackGetPTSDelta( p_demux, tk );
            if( i_delta != -1 )
                p_block->i_pts = p_block->i_dts + i_delta;
            else if( tk->fmt.i_cat != VIDEO_ES )
                p_block->i_pts = p_block->i_dts;
            else
                p_block->i_pts = VLC_TS_INVALID;

            es_out_Send( p_demux->out, tk->p_es, p_block );

            tk->i_sample++;

            if( !tk->b_has_non_empty_cchunk )
                break;
        }
    }
    return 1;
}
