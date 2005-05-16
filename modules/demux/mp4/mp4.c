/*****************************************************************************
 * mp4.c : MP4 file input module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2004 VideoLAN
 * $Id$
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc_playlist.h>
#include "iso_lang.h"
#include "vlc_meta.h"

#include "libmp4.h"
#include "drms.h"

#ifdef UNDER_CE
#define uint64_t int64_t
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );
    set_description( _("MP4 stream demuxer") );
    set_capability( "demux2", 242 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int   Demux   ( demux_t * );
static int   DemuxRef( demux_t *p_demux ){ return 0;}
static int   Seek    ( demux_t *, mtime_t );
static int   Control ( demux_t *, int, va_list );

/* Contain all information about a chunk */
typedef struct
{
    uint64_t     i_offset; /* absolute position of this chunk in the file */
    uint32_t     i_sample_description_index; /* index for SampleEntry to use */
    uint32_t     i_sample_count; /* how many samples in this chunk */
    uint32_t     i_sample_first; /* index of the first sample in this chunk */

    /* now provide way to calculate pts, dts, and offset without to
        much memory and with fast acces */

    /* with this we can calculate dts/pts without waste memory */
    uint64_t     i_first_dts;
    uint32_t     *p_sample_count_dts;
    uint32_t     *p_sample_delta_dts;   /* dts delta */

    uint32_t     *p_sample_count_pts;
    int32_t      *p_sample_offset_pts;  /* pts-dts */

    /* TODO if needed add pts
        but quickly *add* support for edts and seeking */

} mp4_chunk_t;

 /* Contain all needed information for read all track with vlc */
typedef struct
{
    int i_track_ID;     /* this should be unique */

    int b_ok;           /* The track is usable */
    int b_enable;       /* is the trak enable by default */
    vlc_bool_t b_selected;     /* is the trak being played */

    es_format_t fmt;
    es_out_id_t *p_es;

    /* display size only ! */
    int i_width;
    int i_height;

    /* more internal data */
    uint64_t        i_timescale;    /* time scale for this track only */

    /* elst */
    int             i_elst;         /* current elst */
    int64_t         i_elst_time;    /* current elst start time (in movie time scale)*/
    MP4_Box_t       *p_elst;        /* elst (could be NULL) */

    /* give the next sample to read, i_chunk is to find quickly where
      the sample is located */
    uint32_t         i_sample;       /* next sample to read */
    uint32_t         i_chunk;        /* chunk where next sample is stored */
    /* total count of chunk and sample */
    uint32_t         i_chunk_count;
    uint32_t         i_sample_count;

    mp4_chunk_t    *chunk; /* always defined  for each chunk */

    /* sample size, p_sample_size defined only if i_sample_size == 0
        else i_sample_size is size for all sample */
    uint32_t         i_sample_size;
    uint32_t         *p_sample_size; /* XXX perhaps add file offset if take
                                    too much time to do sumations each time*/

    MP4_Box_t *p_stbl;  /* will contain all timing information */
    MP4_Box_t *p_stsd;  /* will contain all data to initialize decoder */
    MP4_Box_t *p_sample;/* point on actual sdsd */

    vlc_bool_t b_drms;
    void      *p_drms;

} mp4_track_t;


struct demux_sys_t
{
    MP4_Box_t    *p_root;      /* container for the whole file */

    mtime_t      i_pcr;

    uint64_t     i_time;        /* time position of the presentation
                                 * in movie timescale */
    uint64_t     i_timescale;   /* movie time scale */
    uint64_t     i_duration;    /* movie duration */
    unsigned int i_tracks;      /* number of tracks */
    mp4_track_t *track;    /* array of track */
};

/*****************************************************************************
 * Declaration of local function
 *****************************************************************************/
static void MP4_TrackCreate ( demux_t *, mp4_track_t *, MP4_Box_t  *);
static void MP4_TrackDestroy( demux_t *, mp4_track_t * );

static int  MP4_TrackSelect ( demux_t *, mp4_track_t *, mtime_t );
static void MP4_TrackUnselect(demux_t *, mp4_track_t * );

static int  MP4_TrackSeek   ( demux_t *, mp4_track_t *, mtime_t );

static uint64_t MP4_TrackGetPos    ( mp4_track_t * );
static int      MP4_TrackSampleSize( mp4_track_t * );
static int      MP4_TrackNextSample( demux_t *, mp4_track_t * );
static void     MP4_TrackSetELST( demux_t *, mp4_track_t *, int64_t );

/* Return time in µs of a track */
static inline int64_t MP4_TrackGetDTS( demux_t *p_demux, mp4_track_t *p_track )
{
#define chunk p_track->chunk[p_track->i_chunk]

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
            i_sample = 0;
            break;
        }
    }

#undef chunk

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

    return I64C(1000000) * i_dts / p_track->i_timescale;
}

static inline int64_t MP4_TrackGetPTSDelta( demux_t *p_demux, mp4_track_t *p_track )
{
    mp4_chunk_t *ck = &p_track->chunk[p_track->i_chunk];
    unsigned int i_index = 0;
    unsigned int i_sample = p_track->i_sample - ck->i_sample_first;

    if( ck->p_sample_count_pts == NULL || ck->p_sample_offset_pts == NULL )
        return -1;

    for( i_index = 0;; i_index++ )
    {
        if( i_sample < ck->p_sample_count_pts[i_index] )
            return ck->p_sample_offset_pts[i_index] * I64C(1000000) /
                   (int64_t)p_track->i_timescale;

        i_sample -= ck->p_sample_count_pts[i_index];
    }
}

static inline int64_t MP4_GetMoviePTS(demux_sys_t *p_sys )
{
    return I64C(1000000) * p_sys->i_time / p_sys->i_timescale;
}

#define FREE( p ) if( p ) { free( p ); (p) = NULL;}

/*****************************************************************************
 * Open: check file and initializes MP4 structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t  *p_demux = (demux_t *)p_this;
    demux_sys_t     *p_sys;

    uint8_t         *p_peek;

    MP4_Box_t       *p_ftyp;
    MP4_Box_t       *p_rmra;
    MP4_Box_t       *p_mvhd;
    MP4_Box_t       *p_trak;

    unsigned int    i;
    vlc_bool_t      b_seekable;

    /* A little test to see if it could be a mp4 */
    if( stream_Peek( p_demux->s, &p_peek, 8 ) < 8 ) return VLC_EGENERIC;

    switch( VLC_FOURCC( p_peek[4], p_peek[5], p_peek[6], p_peek[7] ) )
    {
        case FOURCC_ftyp:
        case FOURCC_moov:
        case FOURCC_foov:
        case FOURCC_moof:
        case FOURCC_mdat:
        case FOURCC_udta:
        case FOURCC_free:
        case FOURCC_skip:
        case FOURCC_wide:
        case VLC_FOURCC( 'p', 'n', 'o', 't' ):
            break;
         default:
            return VLC_EGENERIC;
    }

    /* I need to seek */
    stream_Control( p_demux->s, STREAM_CAN_SEEK, &b_seekable );
    if( !b_seekable )
    {
        msg_Warn( p_demux, "MP4 plugin discarded (unseekable)" );
        return VLC_EGENERIC;
    }

    /*Set exported functions */
    p_demux->pf_demux = Demux;
    p_demux->pf_control = Control;

    /* create our structure that will contains all data */
    p_demux->p_sys = p_sys = malloc( sizeof( demux_sys_t ) );
    memset( p_sys, 0, sizeof( demux_sys_t ) );

    /* Now load all boxes ( except raw data ) */
    if( ( p_sys->p_root = MP4_BoxGetRoot( p_demux->s ) ) == NULL )
    {
        msg_Warn( p_demux, "MP4 plugin discarded (not a valid file)" );
        goto error;
    }

    MP4_BoxDumpStructure( p_demux->s, p_sys->p_root );

    if( ( p_ftyp = MP4_BoxGet( p_sys->p_root, "/ftyp" ) ) )
    {
        switch( p_ftyp->data.p_ftyp->i_major_brand )
        {
            case( FOURCC_isom ):
                msg_Dbg( p_demux,
                         "ISO Media file (isom) version %d.",
                         p_ftyp->data.p_ftyp->i_minor_version );
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
            msg_Err( p_demux, "MP4 plugin discarded (no moov box)" );
            goto error;
        }
        /* we have a free box as a moov, rename it */
        p_foov->i_type = FOURCC_moov;
    }

    if( ( p_rmra = MP4_BoxGet( p_sys->p_root,  "/moov/rmra" ) ) )
    {
        playlist_t *p_playlist;
        playlist_item_t *p_item;
        int        i_count = MP4_BoxCount( p_rmra, "rmda" );
        int        i;
        vlc_bool_t b_play = VLC_FALSE;

        msg_Dbg( p_demux, "detected playlist mov file (%d ref)", i_count );

        p_playlist =
            (playlist_t *)vlc_object_find( p_demux,
                                           VLC_OBJECT_PLAYLIST,
                                           FIND_ANYWHERE );
        if( p_playlist )
        {
            p_item = playlist_LockItemGetByInput( p_playlist,
                      ((input_thread_t *)p_demux->p_parent)->input.p_item );
            playlist_ItemToNode( p_playlist, p_item );

            for( i = 0; i < i_count; i++ )
            {
                MP4_Box_t *p_rdrf = MP4_BoxGet( p_rmra, "rmda[%d]/rdrf", i );
                char      *psz_ref;
                uint32_t  i_ref_type;

                if( !p_rdrf || !( psz_ref = p_rdrf->data.p_rdrf->psz_ref ) )
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
                        msg_Dbg( p_demux, "adding ref = `%s'", psz_ref );
                        if( p_item )
                        {
                            playlist_item_t *p_child =
                                        playlist_ItemNew( p_playlist,
                                                          psz_ref, psz_ref );
                            if( p_child )
                            {
                                playlist_NodeAddItem( p_playlist, p_child,
                                                 p_item->pp_parents[0]->i_view,
                                                 p_item, PLAYLIST_APPEND,
                                                 PLAYLIST_END );
                                playlist_CopyParents( p_item, p_child );
                                b_play = VLC_TRUE;
                            }
                        }
                    }
                    else
                    {
                        /* msg dbg relative ? */
                        char *psz_absolute = alloca( strlen( p_demux->psz_access ) + 3 + strlen( p_demux->psz_path ) + strlen( psz_ref ) + 1);
                        char *end = strrchr( p_demux->psz_path, '/' );

                        if( end )
                        {
                            int i_len = end + 1 - p_demux->psz_path;

                            strcpy( psz_absolute, p_demux->psz_access );
                            strcat( psz_absolute, "://" );
                            strncat( psz_absolute, p_demux->psz_path, i_len);
                        }
                        else
                        {
                            strcpy( psz_absolute, "" );
                        }
                        strcat( psz_absolute, psz_ref );
                        msg_Dbg( p_demux, "adding ref = `%s'", psz_absolute );
                        if( p_item )
                        {
                            playlist_item_t *p_child =
                                        playlist_ItemNew( p_playlist,
                                                          psz_absolute,
                                                          psz_absolute );
                            if( p_child )
                            {
                                playlist_NodeAddItem( p_playlist, p_child,
                                                 p_item->pp_parents[0]->i_view,
                                                 p_item, PLAYLIST_APPEND,
                                                 PLAYLIST_END );
                                playlist_CopyParents( p_item, p_child );
                                b_play = VLC_TRUE;
                            }
                        }
                    }
                }
                else
                {
                    msg_Err( p_demux, "unknown ref type=%4.4s FIXME (send a bug report)",
                             (char*)&p_rdrf->data.p_rdrf->i_ref_type );
                }
            }
            if( b_play == VLC_TRUE )
            {
                 playlist_Control( p_playlist, PLAYLIST_VIEWPLAY,
                                   p_playlist->status.i_view,
                                   p_playlist->status.p_item, NULL );
            }
            vlc_object_release( p_playlist );
        }
        else
        {
            msg_Err( p_demux, "can't find playlist" );
        }
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
        p_sys->i_duration = p_mvhd->data.p_mvhd->i_duration;
    }

    if( !( p_sys->i_tracks = MP4_BoxCount( p_sys->p_root, "/moov/trak" ) ) )
    {
        msg_Err( p_demux, "cannot find any /moov/trak" );
        goto error;
    }
    msg_Dbg( p_demux, "find %d track%c",
                        p_sys->i_tracks,
                        p_sys->i_tracks ? 's':' ' );

    /* allocate memory */
    p_sys->track = calloc( p_sys->i_tracks, sizeof( mp4_track_t ) );
    memset( p_sys->track, 0, p_sys->i_tracks * sizeof( mp4_track_t ) );

    /* now process each track and extract all usefull information */
    for( i = 0; i < p_sys->i_tracks; i++ )
    {
        p_trak = MP4_BoxGet( p_sys->p_root, "/moov/trak[%d]", i );
        MP4_TrackCreate( p_demux, &p_sys->track[i], p_trak );

        if( p_sys->track[i].b_ok )
        {
            char *psz_cat;
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
        else
        {
            msg_Dbg( p_demux, "ignoring track[Id 0x%x]",
                     p_sys->track[i].i_track_ID );
        }

    }
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
        vlc_bool_t b;

        if( !tk->b_ok ||
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

    /* first wait for the good time to read a packet */
    es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_sys->i_pcr + 1 );

    p_sys->i_pcr = MP4_GetMoviePTS( p_sys );

    /* we will read 100ms for each stream so ...*/
    p_sys->i_time += __MAX( p_sys->i_timescale / 10 , 1 );

    for( i_track = 0; i_track < p_sys->i_tracks; i_track++ )
    {
        mp4_track_t *tk = &p_sys->track[i_track];

        if( !tk->b_ok || !tk->b_selected || tk->i_sample >= tk->i_sample_count )
        {
            continue;
        }

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

                if( tk->b_drms && tk->p_drms )
                {
                    drms_decrypt( tk->p_drms, (uint32_t*)p_block->p_buffer,
                                  p_block->i_buffer );
                }
                else if( tk->fmt.i_cat == SPU_ES )
                {
                    if( tk->fmt.i_codec == VLC_FOURCC( 's', 'u', 'b', 't' ) &&
                        p_block->i_buffer >= 2 )
                    {
                        uint16_t i_size = GetWBE( p_block->p_buffer );

                        if( i_size + 2 <= p_block->i_buffer )
                        {
                            char *p;
                            /* remove the length field, and append a '\0' */
                            memmove( &p_block->p_buffer[0],
                                     &p_block->p_buffer[2], i_size );
                            p_block->p_buffer[i_size] = '\0';
                            p_block->i_buffer = i_size + 1;

                            /* convert \r -> \n */
                            while( ( p = strchr( p_block->p_buffer, '\r' ) ) )
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
                p_block->i_dts = MP4_TrackGetDTS( p_demux, tk ) + 1;
                /* pts */
                i_delta = MP4_TrackGetPTSDelta( p_demux, tk );
                if( i_delta >= 0 )
                    p_block->i_pts = p_block->i_dts + i_delta;
                else if( tk->fmt.i_cat != VIDEO_ES )
                    p_block->i_pts = p_block->i_dts;
                else
                    p_block->i_pts = 0;

                if( !tk->b_drms || ( tk->b_drms && tk->p_drms ) )
                    es_out_Send( p_demux->out, tk->p_es, p_block );
            }

            /* Next sample */
            if( MP4_TrackNextSample( p_demux, tk ) )
            {
                break;
            }
        }
    }

    return 1;
}
/*****************************************************************************
 * Seek: Got to i_date
******************************************************************************/
static int Seek( demux_t *p_demux, mtime_t i_date )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    unsigned int i_track;

    /* First update update global time */
    p_sys->i_time = i_date * p_sys->i_timescale / 1000000;
    p_sys->i_pcr  = i_date;

    /* Now for each stream try to go to this time */
    for( i_track = 0; i_track < p_sys->i_tracks; i_track++ )
    {
        mp4_track_t *tk = &p_sys->track[i_track];
        MP4_TrackSeek( p_demux, tk, i_date );
    }
    return VLC_SUCCESS;
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
            if( p_sys->i_timescale > 0 )
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
            msg_Warn( p_demux, "DEMUX_GET_FPS unimplemented !!" );
            return VLC_EGENERIC;

        case DEMUX_GET_META:
        {
            vlc_meta_t **pp_meta = (vlc_meta_t**)va_arg( args, vlc_meta_t** );
            vlc_meta_t *meta;
            MP4_Box_t  *p_udta   = MP4_BoxGet( p_sys->p_root, "/moov/udta" );
            MP4_Box_t  *p_0xa9xxx;
            if( p_udta == NULL )
            {
                return VLC_EGENERIC;
            }
            *pp_meta = meta = vlc_meta_New();
            for( p_0xa9xxx = p_udta->p_first; p_0xa9xxx != NULL;
                 p_0xa9xxx = p_0xa9xxx->p_next )
            {
                switch( p_0xa9xxx->i_type )
                {
                case FOURCC_0xa9nam: /* Full name */
                    vlc_meta_Add( meta, VLC_META_TITLE,
                                  p_0xa9xxx->data.p_0xa9xxx->psz_text );
                    break;
                case FOURCC_0xa9aut:
                    vlc_meta_Add( meta, VLC_META_AUTHOR,
                                  p_0xa9xxx->data.p_0xa9xxx->psz_text );
                    break;
                case FOURCC_0xa9ART:
                    vlc_meta_Add( meta, VLC_META_ARTIST,
                                  p_0xa9xxx->data.p_0xa9xxx->psz_text );
                    break;
                case FOURCC_0xa9cpy:
                    vlc_meta_Add( meta, VLC_META_COPYRIGHT,
                                  p_0xa9xxx->data.p_0xa9xxx->psz_text );
                    break;
                case FOURCC_0xa9day: /* Creation Date */
                    vlc_meta_Add( meta, VLC_META_DATE,
                                  p_0xa9xxx->data.p_0xa9xxx->psz_text );
                    break;
                case FOURCC_0xa9des: /* Description */
                    vlc_meta_Add( meta, VLC_META_DESCRIPTION,
                                  p_0xa9xxx->data.p_0xa9xxx->psz_text );
                    break;
                case FOURCC_0xa9gen: /* Genre */
                    vlc_meta_Add( meta, VLC_META_GENRE,
                                  p_0xa9xxx->data.p_0xa9xxx->psz_text );
                    break;

                case FOURCC_0xa9swr:
                case FOURCC_0xa9inf: /* Information */
                case FOURCC_0xa9alb: /* Album */
                case FOURCC_0xa9dir: /* Director */
                case FOURCC_0xa9dis: /* Disclaimer */
                case FOURCC_0xa9enc: /* Encoded By */
                case FOURCC_0xa9trk: /* Track */
                case FOURCC_0xa9cmt: /* Commment */
                case FOURCC_0xa9url: /* URL */
                case FOURCC_0xa9req: /* Requirements */
                case FOURCC_0xa9fmt: /* Original Format */
                case FOURCC_0xa9dsa: /* Display Source As */
                case FOURCC_0xa9hst: /* Host Computer */
                case FOURCC_0xa9prd: /* Producer */
                case FOURCC_0xa9prf: /* Performers */
                case FOURCC_0xa9ope: /* Original Performer */
                case FOURCC_0xa9src: /* Providers Source Content */
                case FOURCC_0xa9wrt: /* Writer */
                case FOURCC_0xa9com: /* Composer */
                case FOURCC_WLOC:    /* Window Location */
                    /* TODO one day, but they aren't really meaningfull */
                    break;

                default:
                    break;
                }
            }
            return VLC_SUCCESS;
        }

        case DEMUX_GET_TITLE_INFO:
        case DEMUX_SET_NEXT_DEMUX_TIME:
        case DEMUX_SET_GROUP:
            return VLC_EGENERIC;

        default:
            msg_Warn( p_demux, "control query unimplemented !!!" );
            return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close ( vlc_object_t * p_this )
{
    unsigned int i_track;
    demux_t *  p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    msg_Dbg( p_demux, "freeing all memory" );

    MP4_BoxFree( p_demux->s, p_sys->p_root );
    for( i_track = 0; i_track < p_sys->i_tracks; i_track++ )
    {
        MP4_TrackDestroy( p_demux, &p_sys->track[i_track] );
    }
    FREE( p_sys->track );

    free( p_sys );
}



/****************************************************************************
 * Local functions, specific to vlc
 ****************************************************************************/

/* now create basic chunk data, the rest will be filled by MP4_CreateSamplesIndex */
static int TrackCreateChunksIndex( demux_t *p_demux,
                                   mp4_track_t *p_demux_track )
{
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
    MP4_Box_t *p_box;
    MP4_Box_data_stsz_t *stsz;
    MP4_Box_data_stts_t *stts;
    /* TODO use also stss and stsh table for seeking */
    /* FIXME use edit table */
    int64_t i_sample;
    int64_t i_chunk;

    int64_t i_index;
    int64_t i_index_sample_used;

    int64_t i_last_dts;

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

    i_last_dts = 0;
    i_index = 0; i_index_sample_used = 0;
    for( i_chunk = 0; i_chunk < p_demux_track->i_chunk_count; i_chunk++ )
    {
        mp4_chunk_t *ck = &p_demux_track->chunk[i_chunk];
        int64_t i_entry, i_sample_count, i;

        /* save last dts */
        ck->i_first_dts = i_last_dts;

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

            ck->p_sample_count_dts[i] = i_used;
            ck->p_sample_delta_dts[i] = stts->i_sample_delta[i_index];

            i_last_dts += i_used * ck->p_sample_delta_dts[i];

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

    msg_Dbg( p_demux, "track[Id 0x%x] read %d samples length:"I64Fd"s",
             p_demux_track->i_track_ID, p_demux_track->i_sample_count,
             i_last_dts / p_demux_track->i_timescale );

    return VLC_SUCCESS;
}

/*
 * TrackCreateES:
 * Create ES and PES to init decoder if needed, for a track starting at i_chunk
 */
static int TrackCreateES( demux_t *p_demux, mp4_track_t *p_track,
                          unsigned int i_chunk, es_out_id_t **pp_es )
{
    MP4_Box_t   *p_sample;
    MP4_Box_t   *p_esds;
    MP4_Box_t   *p_box;

    *pp_es = NULL;

    if( !p_track->chunk[i_chunk].i_sample_description_index )
    {
        msg_Warn( p_demux, "invalid SampleEntry index (track[Id 0x%x])",
                  p_track->i_track_ID );
        return VLC_EGENERIC;
    }

    p_sample = MP4_BoxGet(  p_track->p_stsd, "[%d]",
                p_track->chunk[i_chunk].i_sample_description_index - 1 );

    if( !p_sample ||
        ( !p_sample->data.p_data && p_track->fmt.i_cat != SPU_ES ) )
    {
        msg_Warn( p_demux, "cannot find SampleEntry (track[Id 0x%x])",
                  p_track->i_track_ID );
        return VLC_EGENERIC;
    }

    p_track->p_sample = p_sample;

    if( p_track->fmt.i_cat == AUDIO_ES && p_track->i_sample_size == 1 )
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


    /* It's a little ugly but .. there are special cases */
    switch( p_sample->i_type )
    {
        case( VLC_FOURCC( '.', 'm', 'p', '3' ) ):
        case( VLC_FOURCC( 'm', 's', 0x00, 0x55 ) ):
            p_track->fmt.i_codec = VLC_FOURCC( 'm', 'p', 'g', 'a' );
            break;
        case( VLC_FOURCC( 'r', 'a', 'w', ' ' ) ):
            p_track->fmt.i_codec = VLC_FOURCC( 'a', 'r', 'a', 'w' );
            break;
        case( VLC_FOURCC( 's', '2', '6', '3' ) ):
            p_track->fmt.i_codec = VLC_FOURCC( 'h', '2', '6', '3' );
            break;

        case( VLC_FOURCC( 't', 'e', 'x', 't' ) ):
            p_track->fmt.i_codec = VLC_FOURCC( 's', 'u', 'b', 't' );
            /* FIXME: Not true, could be UTF-16 with a Byte Order Mark (0xfeff) */
            /* FIXME UTF-8 doesn't work here ? */
            /* p_track->fmt.subs.psz_encoding = strdup( "UTF-8" ); */
            break;

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
                p_track->fmt.i_codec = VLC_FOURCC( 'm','p','4','v' );
                break;
            case( 0x40):
                p_track->fmt.i_codec = VLC_FOURCC( 'm','p','4','a' );
                break;
            case( 0x60):
            case( 0x61):
            case( 0x62):
            case( 0x63):
            case( 0x64):
            case( 0x65): /* MPEG2 video */
                p_track->fmt.i_codec = VLC_FOURCC( 'm','p','g','v' );
                break;
            /* Theses are MPEG2-AAC */
            case( 0x66): /* main profile */
            case( 0x67): /* Low complexity profile */
            case( 0x68): /* Scaleable Sampling rate profile */
                p_track->fmt.i_codec = VLC_FOURCC( 'm','p','4','a' );
                break;
            /* true MPEG 2 audio */
            case( 0x69):
                p_track->fmt.i_codec = VLC_FOURCC( 'm','p','g','a' );
                break;
            case( 0x6a): /* MPEG1 video */
                p_track->fmt.i_codec = VLC_FOURCC( 'm','p','g','v' );
                break;
            case( 0x6b): /* MPEG1 audio */
                p_track->fmt.i_codec = VLC_FOURCC( 'm','p','g','a' );
                break;
            case( 0x6c ): /* jpeg */
                p_track->fmt.i_codec = VLC_FOURCC( 'j','p','e','g' );
                break;

            /* Private ID */
            case( 0xe0 ): /* NeroDigital: dvd subs */
                if( p_track->fmt.i_cat == SPU_ES )
                {
                    p_track->fmt.i_codec = VLC_FOURCC( 's','p','u',' ' );
                    break;
                }
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
            case VLC_FOURCC( 'Q', 'D', 'M', 'C' ):
            case VLC_FOURCC( 'Q', 'D', 'M', '2' ):
            case VLC_FOURCC( 'Q', 'c', 'l', 'p' ):
            case VLC_FOURCC( 's', 'a', 'm', 'r' ):
            case VLC_FOURCC( 'a', 'l', 'a', 'c' ):
                p_track->fmt.i_extra =
                    p_sample->data.p_sample_soun->i_qt_description;
                if( p_track->fmt.i_extra > 0 )
                {
                    p_track->fmt.p_extra = malloc( p_track->fmt.i_extra );
                    memcpy( p_track->fmt.p_extra,
                            p_sample->data.p_sample_soun->p_qt_description,
                            p_track->fmt.i_extra);
                }
                break;

            /* avc1: send avcC (h264 without annexe B, ie without start code)*/
            case VLC_FOURCC( 'a', 'v', 'c', '1' ):
            {
                MP4_Box_t *p_avcC = MP4_BoxGet( p_sample, "avcC" );

                if( p_avcC )
                {
                    /* Hack: use a packetizer to reecampsulate data in anexe B format */
                    msg_Dbg( p_demux, "avcC: size=%d", p_avcC->data.p_avcC->i_avcC );
                    p_track->fmt.i_extra = p_avcC->data.p_avcC->i_avcC;
                    p_track->fmt.p_extra = malloc( p_avcC->data.p_avcC->i_avcC );
                    memcpy( p_track->fmt.p_extra, p_avcC->data.p_avcC->p_avcC, p_track->fmt.i_extra );
                    p_track->fmt.b_packetized = VLC_FALSE;
                }
                else
                {
                    msg_Err( p_demux, "missing avcC" );
                }
                break;
            }

            default:
                break;
        }
    }

#undef p_decconfig

    /* some last initialisation */
    switch( p_track->fmt.i_cat )
    {
    case( VIDEO_ES ):
        p_track->fmt.video.i_width = p_sample->data.p_sample_vide->i_width;
        p_track->fmt.video.i_height = p_sample->data.p_sample_vide->i_height;

        /* fall on display size */
        if( p_track->fmt.video.i_width <= 0 )
            p_track->fmt.video.i_width = p_track->i_width;
        if( p_track->fmt.video.i_height <= 0 )
            p_track->fmt.video.i_height = p_track->i_height;

        /* Find out apect ratio from display size */
        if( p_track->i_width > 0 && p_track->i_height > 0 &&
            /* Work-around buggy muxed files */
            p_sample->data.p_sample_vide->i_width != p_track->i_width )
            p_track->fmt.video.i_aspect =
                VOUT_ASPECT_FACTOR * p_track->i_width / p_track->i_height;

        /* Frame rate */
        p_track->fmt.video.i_frame_rate = p_track->i_timescale;
        p_track->fmt.video.i_frame_rate_base = 1;

        if( p_track->fmt.video.i_frame_rate &&
            (p_box = MP4_BoxGet( p_track->p_stbl, "stts" )) &&
            p_box->data.p_stts->i_entry_count >= 1 )
        {
            p_track->fmt.video.i_frame_rate_base =
                p_box->data.p_stts->i_sample_delta[0];
        }

        break;

    case( AUDIO_ES ):
        p_track->fmt.audio.i_channels =
            p_sample->data.p_sample_soun->i_channelcount;
        p_track->fmt.audio.i_rate =
            p_sample->data.p_sample_soun->i_sampleratehi;
        p_track->fmt.i_bitrate = p_sample->data.p_sample_soun->i_channelcount *
            p_sample->data.p_sample_soun->i_sampleratehi *
                p_sample->data.p_sample_soun->i_samplesize;
        p_track->fmt.audio.i_bitspersample =
            p_sample->data.p_sample_soun->i_samplesize;
        break;

    default:
        break;
    }

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
    MP4_Box_t   *p_stss;
    uint64_t     i_dts;
    unsigned int i_sample;
    unsigned int i_chunk;
    int          i_index;

    /* FIXME see if it's needed to check p_track->i_chunk_count */
    if( !p_track->b_ok || p_track->i_chunk_count == 0 )
    {
        return( VLC_EGENERIC );
    }

    /* handle elst (find the correct one) */
    MP4_TrackSetELST( p_demux, p_track, i_start );
    if( p_track->p_elst && p_track->p_elst->data.p_elst->i_entry_count > 0 )
    {
        MP4_Box_data_elst_t *elst = p_track->p_elst->data.p_elst;
        int64_t i_mvt= i_start * p_sys->i_timescale / (int64_t)1000000;

        /* now calculate i_start for this elst */
        /* offset */
        i_start -= p_track->i_elst_time * I64C(1000000) / p_sys->i_timescale;
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

        msg_Dbg( p_demux, "elst (%d) gives "I64Fd"ms (movie)-> "I64Fd
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

        if( i_start >= p_track->chunk[i_chunk].i_first_dts &&
            i_start <  p_track->chunk[i_chunk + 1].i_first_dts )
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
            p_track->chunk[i_chunk].p_sample_delta_dts[i_index] < i_start )
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
    if( ( p_stss = MP4_BoxGet( p_track->p_stbl, "stss" ) ) )
    {
        unsigned int i_index;
        msg_Dbg( p_demux,
                    "track[Id 0x%x] using Sync Sample Box (stss)",
                    p_track->i_track_ID );
        for( i_index = 0; i_index < p_stss->data.p_stss->i_entry_count; i_index++ )
        {
            if( p_stss->data.p_stss->i_sample_number[i_index] >= i_sample )
            {
                if( i_index > 0 )
                {
                    msg_Dbg( p_demux, "stts gives %d --> %d (sample number)",
                            i_sample,
                            p_stss->data.p_stss->i_sample_number[i_index-1] );
                    i_sample = p_stss->data.p_stss->i_sample_number[i_index-1];
                    /* new i_sample is less than old so i_chunk can only decreased */
                    while( i_chunk > 0 &&
                            i_sample < p_track->chunk[i_chunk].i_sample_first )
                    {
                        i_chunk--;
                    }
                }
                else
                {
                    msg_Dbg( p_demux, "stts gives %d --> %d (sample number)",
                            i_sample,
                            p_stss->data.p_stss->i_sample_number[i_index] );
                    i_sample = p_stss->data.p_stss->i_sample_number[i_index];
                    /* new i_sample is more than old so i_chunk can only increased */
                    while( i_chunk < p_track->i_chunk_count - 1 &&
                           i_sample >= p_track->chunk[i_chunk].i_sample_first +
                             p_track->chunk[i_chunk].i_sample_count )
                    {
                        i_chunk++;
                    }
                }
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
    vlc_bool_t b_reselect = VLC_FALSE;

    /* now see if actual es is ok */
    if( p_track->i_chunk < 0 ||
        p_track->i_chunk >= p_track->i_chunk_count - 1 ||
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

            p_track->b_ok       = VLC_FALSE;
            p_track->b_selected = VLC_FALSE;
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
                             MP4_Box_t *p_box_trak )
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

    MP4_Box_t *p_drms;

    unsigned int i;
    char language[4];

    /* hint track unsuported */

    /* set default value (-> track unusable) */
    p_track->b_ok       = VLC_FALSE;
    p_track->b_enable   = VLC_FALSE;
    p_track->b_selected = VLC_FALSE;

    es_format_Init( &p_track->fmt, UNKNOWN_ES, 0 );

    if( !p_tkhd )
    {
        return;
    }

    /* do we launch this track by default ? */
    p_track->b_enable =
        ( ( p_tkhd->data.p_tkhd->i_flags&MP4_TRACK_ENABLED ) != 0 );

    p_track->i_track_ID = p_tkhd->data.p_tkhd->i_track_ID;
    p_track->i_width = p_tkhd->data.p_tkhd->i_width / 65536;
    p_track->i_height = p_tkhd->data.p_tkhd->i_height / 65536;

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

    for( i = 0; i < 3; i++ )
    {
        language[i] = p_mdhd->data.p_mdhd->i_language[i];
    }
    language[3] = '\0';

    switch( p_hdlr->data.p_hdlr->i_handler_type )
    {
        case( FOURCC_soun ):
            if( !( p_smhd = MP4_BoxGet( p_box_trak, "mdia/minf/smhd" ) ) )
            {
                return;
            }
            p_track->fmt.i_cat = AUDIO_ES;
            break;

        case( FOURCC_vide ):
            if( !( p_vmhd = MP4_BoxGet( p_box_trak, "mdia/minf/vmhd" ) ) )
            {
                return;
            }
            p_track->fmt.i_cat = VIDEO_ES;
            break;

        case( FOURCC_text ):
        case( FOURCC_subp ):
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
        int i;

        msg_Warn( p_demux, "elst box found" );
        for( i = 0; i < elst->i_entry_count; i++ )
        {
            msg_Dbg( p_demux, "   - [%d] duration="I64Fd"ms media time="I64Fd
                     "ms) rate=%d.%d", i,
                     elst->i_segment_duration[i] * 1000 / p_sys->i_timescale,
                     elst->i_media_time[i] >= 0 ?
                     elst->i_media_time[i] * 1000 / p_track->i_timescale : -1,
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

    p_drms = MP4_BoxGet( p_track->p_stsd, "drms" );
    p_track->b_drms = p_drms != NULL;
    p_track->p_drms = p_track->b_drms ?
        p_drms->data.p_sample_soun->p_drms : NULL;

    /* Set language */
    if( strcmp( language, "```" ) && strcmp( language, "und" ) )
    {
        p_track->fmt.psz_language = strdup( language );
    }

    p_udta = MP4_BoxGet( p_box_trak, "udta" );
    if( p_udta )
    {
        MP4_Box_t *p_0xa9xxx;
        for( p_0xa9xxx = p_udta->p_first; p_0xa9xxx != NULL;
                 p_0xa9xxx = p_0xa9xxx->p_next )
        {
            switch( p_0xa9xxx->i_type )
            {
                case FOURCC_0xa9nam:
                    p_track->fmt.psz_description =
                        strdup( p_0xa9xxx->data.p_0xa9xxx->psz_text );
                    break;
            }
        }
    }

    /* fxi i_timescale for AUDIO_ES with i_qt_version == 0 */
    if( p_track->fmt.i_cat == AUDIO_ES ) //&& p_track->i_sample_size == 1 )
    {
        MP4_Box_t *p_sample;

        p_sample = MP4_BoxGet(  p_track->p_stsd, "[0]" );
        if( p_sample && p_sample->data.p_sample_soun)
        {
            MP4_Box_data_sample_soun_t *p_soun = p_sample->data.p_sample_soun;
            if( p_soun->i_qt_version == 0 &&
                p_track->i_timescale != p_soun->i_sampleratehi )
            {
                msg_Warn( p_demux,
                          "i_timescale ("I64Fu") != i_sampleratehi (%u) with "
                          "qt_version == 0\n"
                          "Making both equal. (report any problem)",
                          p_track->i_timescale, p_soun->i_sampleratehi );

                if( p_soun->i_sampleratehi )
                    p_track->i_timescale = p_soun->i_sampleratehi;
                else
                    p_soun->i_sampleratehi = p_track->i_timescale;
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

    /* now create es */
    if( TrackCreateES( p_demux,
                       p_track, p_track->i_chunk,
                       &p_track->p_es ) )
    {
        msg_Err( p_demux, "cannot create es for track[Id 0x%x]",
                 p_track->i_track_ID );
        return;
    }

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
    p_track->b_ok = VLC_TRUE;
}

/****************************************************************************
 * MP4_TrackDestroy:
 ****************************************************************************
 * Destroy a track created by MP4_TrackCreate.
 ****************************************************************************/
static void MP4_TrackDestroy( demux_t *p_demux, mp4_track_t *p_track )
{
    unsigned int i_chunk;

    p_track->b_ok = VLC_FALSE;
    p_track->b_enable   = VLC_FALSE;
    p_track->b_selected = VLC_FALSE;

    es_format_Clean( &p_track->fmt );

    for( i_chunk = 0; i_chunk < p_track->i_chunk_count; i_chunk++ )
    {
        if( p_track->chunk )
        {
           FREE(p_track->chunk[i_chunk].p_sample_count_dts);
           FREE(p_track->chunk[i_chunk].p_sample_delta_dts );

           FREE(p_track->chunk[i_chunk].p_sample_count_pts);
           FREE(p_track->chunk[i_chunk].p_sample_offset_pts );
        }
    }
    FREE( p_track->chunk );

    if( !p_track->i_sample_size )
    {
        FREE( p_track->p_sample_size );
    }
}

static int MP4_TrackSelect( demux_t *p_demux, mp4_track_t *p_track,
                            mtime_t i_start )
{
    if( !p_track->b_ok )
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
    if( !p_track->b_ok )
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
                        p_track->p_es, VLC_FALSE );
    }

    p_track->b_selected = VLC_FALSE;
}

static int MP4_TrackSeek( demux_t *p_demux, mp4_track_t *p_track,
                          mtime_t i_start )
{
    uint32_t i_chunk;
    uint32_t i_sample;

    if( !p_track->b_ok )
    {
        return( VLC_EGENERIC );
    }

    p_track->b_selected = VLC_FALSE;

    if( TrackTimeToSampleChunk( p_demux, p_track, i_start,
                                &i_chunk, &i_sample ) )
    {
        msg_Warn( p_demux, "cannot select track[Id 0x%x]",
                  p_track->i_track_ID );
        return( VLC_EGENERIC );
    }

    p_track->b_selected = VLC_TRUE;

    if( TrackGotoChunkSample( p_demux, p_track, i_chunk, i_sample ) ==
        VLC_SUCCESS )
    {
        p_track->b_selected = VLC_TRUE;
    }
    return( p_track->b_selected ? VLC_SUCCESS : VLC_EGENERIC );
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
        i_size = p_track->chunk[p_track->i_chunk].i_sample_count /
            p_soun->i_sample_per_packet * p_soun->i_bytes_per_frame;
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

        if( p_soun->i_qt_version == 0 )
        {
            i_pos += ( p_track->i_sample -
                       p_track->chunk[p_track->i_chunk].i_sample_first ) *
                     p_track->i_sample_size;
        }
        else
        {
            /* we read chunk by chunk */
            i_pos += 0;
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
            /* chunk by chunk */
            p_track->i_sample =
                p_track->chunk[p_track->i_chunk].i_sample_first +
                p_track->chunk[p_track->i_chunk].i_sample_count;
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
        int64_t i_mvt = MP4_TrackGetDTS( p_demux, p_track ) *
                        p_sys->i_timescale / (int64_t)1000000;

        if( p_track->i_elst < elst->i_entry_count &&
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

        for( tk->i_elst = 0; tk->i_elst < elst->i_entry_count; tk->i_elst++ )
        {
            mtime_t i_dur = elst->i_segment_duration[tk->i_elst];

            if( tk->i_elst_time <= i_mvt && i_mvt < tk->i_elst_time + i_dur )
            {
                break;
            }
            tk->i_elst_time += i_dur;
        }

        if( tk->i_elst >= elst->i_entry_count )
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
