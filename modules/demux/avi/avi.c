/*****************************************************************************
 * avi.c : AVI file Stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: avi.c,v 1.79 2003/11/28 13:24:52 fenrir Exp $
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
#include "codecs.h"

#include "libavi.h"
#include "avi.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open   ( vlc_object_t * );
static void Close  ( vlc_object_t * );

vlc_module_begin();
    add_category_hint( N_("avi-demuxer"), NULL, VLC_TRUE );
        add_bool( "avi-interleaved", 0, NULL,
                  N_("force interleaved method"),
                  N_("force interleaved method"), VLC_TRUE );
        add_bool( "avi-index", 0, NULL,
                  N_("force index creation"),
                  N_("force index creation"), VLC_TRUE );

    set_description( N_("AVI demuxer") );
    set_capability( "demux", 212 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int    Control         ( input_thread_t *, int, va_list );
static int    Seek            ( input_thread_t *, mtime_t, int );
static int    Demux_Seekable  ( input_thread_t * );
static int    Demux_UnSeekable( input_thread_t *p_input );

#define FREE( p ) if( p ) { free( p ); (p) = NULL; }
#define __ABS( x ) ( (x) < 0 ? (-(x)) : (x) )

static inline off_t __EVEN( off_t i )
{
    return (i & 1) ? i + 1 : i;
}

static mtime_t AVI_PTSToChunk( avi_track_t *, mtime_t i_pts );
static mtime_t AVI_PTSToByte ( avi_track_t *, mtime_t i_pts );
static mtime_t AVI_GetDPTS   ( avi_track_t *, int64_t i_count );
static mtime_t AVI_GetPTS    ( avi_track_t * );


static int AVI_StreamChunkFind( input_thread_t *, unsigned int i_stream );
static int AVI_StreamChunkSet ( input_thread_t *,
                                unsigned int i_stream, unsigned int i_ck );
static int AVI_StreamBytesSet ( input_thread_t *,
                                unsigned int i_stream, off_t   i_byte );

vlc_fourcc_t AVI_FourccGetCodec( unsigned int i_cat, vlc_fourcc_t );
static int   AVI_GetKeyFlag    ( vlc_fourcc_t , uint8_t * );

static int AVI_PacketGetHeader( input_thread_t *, avi_packet_t *p_pk );
static int AVI_PacketNext     ( input_thread_t * );
static int AVI_PacketRead     ( input_thread_t *, avi_packet_t *, block_t **);
static int AVI_PacketSearch   ( input_thread_t * );

static void AVI_IndexLoad    ( input_thread_t * );
static void AVI_IndexCreate  ( input_thread_t * );
static void AVI_IndexAddEntry( demux_sys_t *, int, AVIIndexEntry_t * );

static mtime_t  AVI_MovieGetLength( input_thread_t * );

/*****************************************************************************
 * Stream management
 *****************************************************************************/
static int        AVI_TrackSeek  ( input_thread_t *, int, mtime_t );
static int        AVI_TrackStopFinishedStreams( input_thread_t *);

/*****************************************************************************
 * Open: check file and initializes AVI structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    input_thread_t  *p_input = (input_thread_t *)p_this;
    demux_sys_t     *p_sys;

    avi_chunk_t         ck_riff;
    avi_chunk_list_t    *p_riff = (avi_chunk_list_t*)&ck_riff;
    avi_chunk_list_t    *p_hdrl, *p_movi;
    avi_chunk_avih_t    *p_avih;

    unsigned int i_track;
    unsigned int i;

    uint8_t  *p_peek;


    /* Is it an avi file ? */
    if( stream_Peek( p_input->s, &p_peek, 12 ) < 12 )
    {
        msg_Err( p_input, "cannot peek()" );
        return VLC_EGENERIC;
    }
    if( strncmp( &p_peek[0], "RIFF", 4 ) || strncmp( &p_peek[8], "AVI ", 4 ) )
    {
        msg_Warn( p_input, "avi module discarded (invalid header)" );
        return VLC_EGENERIC;
    }

    /* Initialize input  structures. */
    p_sys = p_input->p_demux_data = malloc( sizeof(demux_sys_t) );
    memset( p_sys, 0, sizeof( demux_sys_t ) );
    p_sys->i_time   = 0;
    p_sys->i_length = 0;
    p_sys->i_pcr    = 0;
    p_sys->i_movi_lastchunk_pos = 0;
    p_sys->b_odml   = VLC_FALSE;
    p_sys->i_track  = 0;
    p_sys->track    = NULL;

    stream_Control( p_input->s, STREAM_CAN_FASTSEEK, &p_sys->b_seekable );

    p_input->pf_demux_control = Control;
    p_input->pf_demux = Demux_Seekable;
    /* For unseekable stream, automaticaly use Demux_UnSeekable */
    if( !p_sys->b_seekable || config_GetInt( p_input, "avi-interleaved" ) )
    {
        p_input->pf_demux = Demux_UnSeekable;
    }

    if( AVI_ChunkReadRoot( p_input->s, &p_sys->ck_root ) )
    {
        msg_Err( p_input, "avi module discarded (invalid file)" );
        return VLC_EGENERIC;
    }

    if( AVI_ChunkCount( &p_sys->ck_root, AVIFOURCC_RIFF ) > 1 )
    {
        unsigned int i_count =
            AVI_ChunkCount( &p_sys->ck_root, AVIFOURCC_RIFF );

        msg_Warn( p_input, "multiple riff -> OpenDML ?" );
        for( i = 1; i < i_count; i++ )
        {
            avi_chunk_list_t *p_sysx;

            p_sysx = AVI_ChunkFind( &p_sys->ck_root, AVIFOURCC_RIFF, i );
            if( p_sysx->i_type == AVIFOURCC_AVIX )
            {
                msg_Warn( p_input, "detected OpenDML file" );
                p_sys->b_odml = VLC_TRUE;
                break;
            }
        }
    }

    p_riff  = AVI_ChunkFind( &p_sys->ck_root, AVIFOURCC_RIFF, 0 );
    p_hdrl  = AVI_ChunkFind( p_riff, AVIFOURCC_hdrl, 0 );
    p_movi  = AVI_ChunkFind( p_riff, AVIFOURCC_movi, 0 );

    if( !p_hdrl || !p_movi )
    {
        msg_Err( p_input, "avi module discarded (invalid file)" );
        goto error;
    }

    if( !( p_avih = AVI_ChunkFind( p_hdrl, AVIFOURCC_avih, 0 ) ) )
    {
        msg_Err( p_input, "cannot find avih chunk" );
        goto error;
    }
    i_track = AVI_ChunkCount( p_hdrl, AVIFOURCC_strl );
    if( p_avih->i_streams != i_track )
    {
        msg_Warn( p_input,
                  "found %d stream but %d are declared",
                  i_track, p_avih->i_streams );
    }
    if( i_track == 0 )
    {
        msg_Err( p_input, "no stream defined!" );
        goto error;
    }

    /*  create one program */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_InitStream( p_input, 0 ) == -1)
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot init stream" );
        goto error;
    }
    p_input->stream.i_mux_rate = 0; /* Fixed later */
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /* print informations on streams */
    msg_Dbg( p_input, "AVIH: %d stream, flags %s%s%s%s ",
             i_track,
             p_avih->i_flags&AVIF_HASINDEX?" HAS_INDEX":"",
             p_avih->i_flags&AVIF_MUSTUSEINDEX?" MUST_USE_INDEX":"",
             p_avih->i_flags&AVIF_ISINTERLEAVED?" IS_INTERLEAVED":"",
             p_avih->i_flags&AVIF_TRUSTCKTYPE?" TRUST_CKTYPE":"" );
    {
        input_info_category_t *p_cat = input_InfoCategory( p_input, _("Avi") );
        input_AddInfo( p_cat, _("Number of Streams"), "%d", i_track );
        input_AddInfo( p_cat, _("Flags"), "%s%s%s%s",
                       p_avih->i_flags&AVIF_HASINDEX?" HAS_INDEX":"",
                       p_avih->i_flags&AVIF_MUSTUSEINDEX?" MUST_USE_INDEX":"",
                       p_avih->i_flags&AVIF_ISINTERLEAVED?" IS_INTERLEAVED":"",
                       p_avih->i_flags&AVIF_TRUSTCKTYPE?" TRUST_CKTYPE":"" );
    }

    /* now read info on each stream and create ES */
    for( i = 0 ; i < i_track; i++ )
    {
        avi_track_t      *tk = malloc( sizeof( avi_track_t ) );
        avi_chunk_list_t *p_strl = AVI_ChunkFind( p_hdrl, AVIFOURCC_strl, i );
        avi_chunk_strh_t *p_strh = AVI_ChunkFind( p_strl, AVIFOURCC_strh, 0 );
        avi_chunk_strf_auds_t *p_auds;
        avi_chunk_strf_vids_t *p_vids;
        es_format_t fmt;

        tk->b_activated = VLC_FALSE;
        tk->p_index     = 0;
        tk->i_idxnb     = 0;
        tk->i_idxmax    = 0;
        tk->i_idxposc   = 0;
        tk->i_idxposb   = 0;

        p_auds = (void*)p_vids = (void*)AVI_ChunkFind( p_strl, AVIFOURCC_strf, 0 );

        if( p_strl == NULL || p_strh == NULL || p_auds == NULL || p_vids == NULL )
        {
            msg_Warn( p_input, "stream[%d] incomplete", i );
            continue;
        }

        tk->i_rate  = p_strh->i_rate;
        tk->i_scale = p_strh->i_scale;
        tk->i_samplesize = p_strh->i_samplesize;
        msg_Dbg( p_input, "stream[%d] rate:%d scale:%d samplesize:%d",
                 i, tk->i_rate, tk->i_scale, tk->i_samplesize );

        switch( p_strh->i_type )
        {
            case( AVIFOURCC_auds ):
                tk->i_cat   = AUDIO_ES;
                tk->i_codec = AVI_FourccGetCodec( AUDIO_ES,
                                                  p_auds->p_wf->wFormatTag );
                es_format_Init( &fmt, AUDIO_ES, tk->i_codec );

                fmt.audio.i_channels        = p_auds->p_wf->nChannels;
                fmt.audio.i_rate            = p_auds->p_wf->nSamplesPerSec;
                fmt.i_bitrate               = p_auds->p_wf->nAvgBytesPerSec*8;
                fmt.audio.i_blockalign      = p_auds->p_wf->nBlockAlign;
                fmt.audio.i_bitspersample   = p_auds->p_wf->wBitsPerSample;
                fmt.i_extra = __MIN( p_auds->p_wf->cbSize,
                    p_auds->i_chunk_size - sizeof(WAVEFORMATEX) );
                fmt.p_extra = &p_auds->p_wf[1];
                msg_Dbg( p_input, "stream[%d] audio(0x%x) %d channels %dHz %dbits",
                         i, p_auds->p_wf->wFormatTag, p_auds->p_wf->nChannels,
                         p_auds->p_wf->nSamplesPerSec, p_auds->p_wf->wBitsPerSample);
                break;

            case( AVIFOURCC_vids ):
                tk->i_cat   = VIDEO_ES;
                tk->i_codec = AVI_FourccGetCodec( VIDEO_ES,
                                                  p_vids->p_bih->biCompression );
                es_format_Init( &fmt, VIDEO_ES, p_vids->p_bih->biCompression );
                tk->i_samplesize = 0;
                fmt.video.i_width  = p_vids->p_bih->biWidth;
                fmt.video.i_height = p_vids->p_bih->biHeight;
                fmt.i_extra =
                    __MIN( p_vids->p_bih->biSize - sizeof( BITMAPINFOHEADER ),
                           p_vids->i_chunk_size - sizeof(BITMAPINFOHEADER) );
                fmt.p_extra = &p_vids->p_bih[1];
                msg_Dbg( p_input, "stream[%d] video(%4.4s) %dx%d %dbpp %ffps",
                        i,
                         (char*)&p_vids->p_bih->biCompression,
                         p_vids->p_bih->biWidth,
                         p_vids->p_bih->biHeight,
                         p_vids->p_bih->biBitCount,
                         (float)tk->i_rate/(float)tk->i_scale );
                break;
            default:
                msg_Warn( p_input, "stream[%d] unknown type", i );
                free( tk );
                continue;
        }
        tk->p_es = es_out_Add( p_input->p_es_out, &fmt );
        TAB_APPEND( p_sys->i_track, p_sys->track, tk );
    }

    if( p_sys->i_track <= 0 )
    {
        msg_Err( p_input, "No valid track" );
        goto error;
    }

    if( config_GetInt( p_input, "avi-index" ) )
    {
        if( p_sys->b_seekable )
        {
            AVI_IndexCreate( p_input );
        }
        else
        {
            msg_Warn( p_input, "cannot create index (unseekable stream)" );
            AVI_IndexLoad( p_input );
        }
    }
    else
    {
        AVI_IndexLoad( p_input );
    }

    /* *** movie length in sec *** */
    p_sys->i_length = AVI_MovieGetLength( p_input );
    if( p_sys->i_length < (mtime_t)p_avih->i_totalframes *
                          (mtime_t)p_avih->i_microsecperframe /
                          (mtime_t)1000000 )
    {
        msg_Warn( p_input, "broken or missing index, 'seek' will be axproximative or will have strange behavour" );
    }
    /* fix some BeOS MediaKit generated file */
    for( i = 0 ; i < p_sys->i_track; i++ )
    {
        avi_track_t         *tk = p_sys->track[i];
        avi_chunk_list_t    *p_strl;
        avi_chunk_strh_t    *p_strh;
        avi_chunk_strf_auds_t    *p_auds;

        if( tk->i_cat != AUDIO_ES )
        {
            continue;
        }
        if( tk->i_idxnb < 1 ||
            tk->i_scale != 1 ||
            tk->i_samplesize != 0 )
        {
            continue;
        }
        p_strl = AVI_ChunkFind( p_hdrl, AVIFOURCC_strl, i );
        p_strh = AVI_ChunkFind( p_strl, AVIFOURCC_strh, 0 );
        p_auds = AVI_ChunkFind( p_strl, AVIFOURCC_strf, 0 );

        if( p_auds->p_wf->wFormatTag != WAVE_FORMAT_PCM &&
            (unsigned int)tk->i_rate == p_auds->p_wf->nSamplesPerSec )
        {
            int64_t i_track_length =
                tk->p_index[tk->i_idxnb-1].i_length +
                tk->p_index[tk->i_idxnb-1].i_lengthtotal;
            mtime_t i_length = (mtime_t)p_avih->i_totalframes *
                               (mtime_t)p_avih->i_microsecperframe;

            if( i_length == 0 )
            {
                msg_Warn( p_input, "track[%d] cannot be fixed (BeOS MediaKit generated)", i );
                continue;
            }
            tk->i_samplesize = 1;
            tk->i_rate       = i_track_length  * (int64_t)1000000/ i_length;
            msg_Warn( p_input, "track[%d] fixed with rate=%d scale=%d (BeOS MediaKit generated)", i, tk->i_rate, tk->i_scale );
        }
    }

    if( p_sys->i_length )
    {
        p_input->stream.i_mux_rate =
            stream_Size( p_input->s ) / 50 / p_sys->i_length;
    }

    if( p_sys->b_seekable )
    {
        /* we have read all chunk so go back to movi */
        stream_Seek( p_input->s, p_movi->i_chunk_pos );
    }
    /* Skip movi header */
    stream_Read( p_input->s, NULL, 12 );

    p_sys->i_movi_begin = p_movi->i_chunk_pos;
    return VLC_SUCCESS;

error:
    AVI_ChunkFreeRoot( p_input->s, &p_sys->ck_root );
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close ( vlc_object_t * p_this )
{
    input_thread_t *    p_input = (input_thread_t *)p_this;
    unsigned int i;
    demux_sys_t *p_sys = p_input->p_demux_data  ;

    for( i = 0; i < p_sys->i_track; i++ )
    {
        if( p_sys->track[i] )
        {
            FREE( p_sys->track[i]->p_index );
            free( p_sys->track[i] );
        }
    }
    FREE( p_sys->track );
    AVI_ChunkFreeRoot( p_input->s, &p_sys->ck_root );

    free( p_sys );
}

/*****************************************************************************
 * Demux_Seekable: reads and demuxes data packets for stream seekable
 *****************************************************************************
 * AVIDemux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
typedef struct
{
    vlc_bool_t b_ok;

    int i_toread;

    off_t i_posf; /* where we will read :
                   if i_idxposb == 0 : begining of chunk (+8 to acces data)
                   else : point on data directly */
} avi_track_toread_t;

static int Demux_Seekable( input_thread_t *p_input )
{
    demux_sys_t *p_sys = p_input->p_demux_data;

    unsigned int i_track_count = 0;
    unsigned int i_track;
    vlc_bool_t b_stream;
    /* cannot be more than 100 stream (dcXX or wbXX) */
    avi_track_toread_t toread[100];


    /* detect new selected/unselected streams */
    for( i_track = 0; i_track < p_sys->i_track; i_track++ )
    {
        avi_track_t *tk = p_sys->track[i_track];
        vlc_bool_t  b;

        es_out_Control( p_input->p_es_out, ES_OUT_GET_ES_STATE, tk->p_es, &b );
        if( b && !tk->b_activated )
        {
            if( p_sys->b_seekable)
            {
                AVI_TrackSeek( p_input, i_track, p_sys->i_time );
            }
            tk->b_activated = VLC_TRUE;
        }
        else if( !b && tk->b_activated )
        {
            tk->b_activated = VLC_FALSE;
        }
        if( b )
        {
            i_track_count++;
        }
    }

    if( i_track_count <= 0 )
    {
        msg_Warn( p_input, "no track selected, exiting..." );
        return( 0 );
    }

    /* wait for the good time */
    p_sys->i_pcr = p_sys->i_time * 9 / 100;

    input_ClockManageRef( p_input,
                          p_input->stream.p_selected_program,
                          p_sys->i_pcr );


    p_sys->i_time += 25*1000;  /* read 25ms */

    /* init toread */
    for( i_track = 0; i_track < p_sys->i_track; i_track++ )
    {
        avi_track_t *tk = p_sys->track[i_track];
        mtime_t i_dpts;

        toread[i_track].b_ok = tk->b_activated;
        if( tk->i_idxposc < tk->i_idxnb )
        {
            toread[i_track].i_posf = tk->p_index[tk->i_idxposc].i_pos;
           if( tk->i_idxposb > 0 )
           {
                toread[i_track].i_posf += 8 + tk->i_idxposb;
           }
        }
        else
        {
            toread[i_track].i_posf = -1;
        }

        i_dpts = p_sys->i_time - AVI_GetPTS( tk  );

        if( tk->i_samplesize )
        {
            toread[i_track].i_toread = AVI_PTSToByte( tk, __ABS( i_dpts ) );
        }
        else
        {
            toread[i_track].i_toread = AVI_PTSToChunk( tk, __ABS( i_dpts ) );
        }

        if( i_dpts < 0 )
        {
            toread[i_track].i_toread *= -1;
        }
    }

    b_stream = VLC_FALSE;

    for( ;; )
    {
        avi_track_t     *tk;
        vlc_bool_t       b_done;
        block_t         *p_frame;
        off_t i_pos;
        unsigned int i;
        size_t i_size;

        /* search for first chunk to be read */
        for( i = 0, b_done = VLC_TRUE, i_pos = -1; i < p_sys->i_track; i++ )
        {
            if( !toread[i].b_ok ||
                AVI_GetDPTS( p_sys->track[i],
                             toread[i].i_toread ) <= -25 * 1000 )
            {
                continue;
            }

            if( toread[i].i_toread > 0 )
            {
                b_done = VLC_FALSE; /* not yet finished */
            }
            if( toread[i].i_posf > 0 )
            {
                if( i_pos == -1 || i_pos > toread[i_track].i_posf )
                {
                    i_track = i;
                    i_pos = toread[i].i_posf;
                }
            }
        }

        if( b_done )
        {
            return( 1 );
        }

        if( i_pos == -1 )
        {
            /* no valid index, we will parse directly the stream
             * in case we fail we will disable all finished stream */
            if( p_sys->i_movi_lastchunk_pos >= p_sys->i_movi_begin + 12 )
            {
                stream_Seek( p_input->s, p_sys->i_movi_lastchunk_pos );
                if( AVI_PacketNext( p_input ) )
                {
                    return( AVI_TrackStopFinishedStreams( p_input ) ? 0 : 1 );
                }
            }
            else
            {
                stream_Seek( p_input->s, p_sys->i_movi_begin + 12 );
            }

            for( ;; )
            {
                avi_packet_t avi_pk;

                if( AVI_PacketGetHeader( p_input, &avi_pk ) )
                {
                    msg_Warn( p_input,
                             "cannot get packet header, track disabled" );
                    return( AVI_TrackStopFinishedStreams( p_input ) ? 0 : 1 );
                }
                if( avi_pk.i_stream >= p_sys->i_track ||
                    ( avi_pk.i_cat != AUDIO_ES && avi_pk.i_cat != VIDEO_ES ) )
                {
                    if( AVI_PacketNext( p_input ) )
                    {
                        msg_Warn( p_input,
                                  "cannot skip packet, track disabled" );
                        return( AVI_TrackStopFinishedStreams( p_input ) ? 0 : 1 );
                    }
                    continue;
                }
                else
                {
                    /* add this chunk to the index */
                    AVIIndexEntry_t index;

                    index.i_id = avi_pk.i_fourcc;
                    index.i_flags =
                       AVI_GetKeyFlag(p_sys->track[avi_pk.i_stream]->i_codec,
                                      avi_pk.i_peek);
                    index.i_pos = avi_pk.i_pos;
                    index.i_length = avi_pk.i_size;
                    AVI_IndexAddEntry( p_sys, avi_pk.i_stream, &index );

                    i_track = avi_pk.i_stream;
                    tk = p_sys->track[i_track];
                    /* do we will read this data ? */
                    if( AVI_GetDPTS( tk, toread[i_track].i_toread ) > -25*1000 )
                    {
                        break;
                    }
                    else
                    {
                        if( AVI_PacketNext( p_input ) )
                        {
                            msg_Warn( p_input,
                                      "cannot skip packet, track disabled" );
                            return( AVI_TrackStopFinishedStreams( p_input ) ? 0 : 1 );
                        }
                    }
                }
            }

        }
        else
        {
            stream_Seek( p_input->s, i_pos );
        }

        /* Set the track to use */
        tk = p_sys->track[i_track];

        /* read thoses data */
        if( tk->i_samplesize )
        {
            unsigned int i_toread;

            if( ( i_toread = toread[i_track].i_toread ) <= 0 )
            {
                if( tk->i_samplesize > 1 )
                {
                    i_toread = tk->i_samplesize;
                }
                else
                {
                    i_toread = __MAX( AVI_PTSToByte( tk, 20 * 1000 ), 100 );
                }
            }
            i_size = __MIN( tk->p_index[tk->i_idxposc].i_length -
                                tk->i_idxposb,
                            i_toread );
        }
        else
        {
            i_size = tk->p_index[tk->i_idxposc].i_length;
        }

        if( tk->i_idxposb == 0 )
        {
            i_size += 8; /* need to read and skip header */
        }

        if( ( p_frame = stream_Block( p_input->s, __EVEN( i_size ) ) )==NULL )
        {
            msg_Warn( p_input, "failled reading data" );
            tk->b_activated = VLC_FALSE;
            toread[i_track].b_ok = VLC_FALSE;
            continue;
        }
        if( i_size % 2 )    /* read was padded on word boundary */
        {
            p_frame->i_buffer--;
        }
        /* skip header */
        if( tk->i_idxposb == 0 )
        {
            p_frame->p_buffer += 8;
            p_frame->i_buffer -= 8;
        }
        p_frame->i_pts = AVI_GetPTS( tk );

        /* read data */
        if( tk->i_samplesize )
        {
            if( tk->i_idxposb == 0 )
            {
                i_size -= 8;
            }
            toread[i_track].i_toread -= i_size;
            tk->i_idxposb += i_size;
            if( tk->i_idxposb >=
                    tk->p_index[tk->i_idxposc].i_length )
            {
                tk->i_idxposb = 0;
                tk->i_idxposc++;
            }
        }
        else
        {
            toread[i_track].i_toread--;
            tk->i_idxposc++;
        }

        if( tk->i_idxposc < tk->i_idxnb)
        {
            toread[i_track].i_posf =
                tk->p_index[tk->i_idxposc].i_pos;
            if( tk->i_idxposb > 0 )
            {
                toread[i_track].i_posf += 8 + tk->i_idxposb;
            }

        }
        else
        {
            toread[i_track].i_posf = -1;
        }

        b_stream = VLC_TRUE; /* at least one read succeed */

        p_frame->i_pts =
            input_ClockGetTS( p_input,
                              p_input->stream.p_selected_program,
                              p_frame->i_pts * 9/100);

        if( tk->i_cat != VIDEO_ES )
            p_frame->i_dts = p_frame->i_pts;
        else
        {
            p_frame->i_dts = p_frame->i_pts;
            p_frame->i_pts = 0;
        }

        //p_pes->i_rate = p_input->stream.control.i_rate;
        es_out_Send( p_input->p_es_out, tk->p_es, p_frame );
    }
}


/*****************************************************************************
 * Demux_UnSeekable: reads and demuxes data packets for unseekable file
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux_UnSeekable( input_thread_t *p_input )
{
    demux_sys_t     *p_sys = p_input->p_demux_data;
    avi_track_t *p_stream_master = NULL;
    vlc_bool_t b_audio;
    unsigned int i_stream;
    unsigned int i_packet;

    /* Check if we need to send the audio data to decoder */
    b_audio = !p_input->stream.control.b_mute;

    input_ClockManageRef( p_input,
                          p_input->stream.p_selected_program,
                          p_sys->i_pcr );

    /* *** find master stream for data packet skipping algo *** */
    /* *** -> first video, if any, or first audio ES *** */
    for( i_stream = 0; i_stream < p_sys->i_track; i_stream++ )
    {
        avi_track_t *tk = p_sys->track[i_stream];
        vlc_bool_t  b;

        es_out_Control( p_input->p_es_out, ES_OUT_GET_ES_STATE, tk->p_es, &b );

        if( b && tk->i_cat == VIDEO_ES )
        {
            p_stream_master = tk;
        }
        else if( b )
        {
            p_stream_master = tk;
        }
    }

    if( !p_stream_master )
    {
        msg_Warn( p_input, "no more stream selected" );
        return( 0 );
    }

    p_sys->i_pcr = AVI_GetPTS( p_stream_master ) * 9 / 100;

    for( i_packet = 0; i_packet < 10; i_packet++)
    {
#define p_stream    p_sys->track[avi_pk.i_stream]

        avi_packet_t    avi_pk;

        if( AVI_PacketGetHeader( p_input, &avi_pk ) )
        {
            return( 0 );
        }

        if( avi_pk.i_stream >= p_sys->i_track ||
            ( avi_pk.i_cat != AUDIO_ES && avi_pk.i_cat != VIDEO_ES ) )
        {
            /* we haven't found an audio or video packet:
             *  - we have seek, found first next packet
             *  - others packets could be found, skip them
             */
            switch( avi_pk.i_fourcc )
            {
                case AVIFOURCC_JUNK:
                case AVIFOURCC_LIST:
                case AVIFOURCC_RIFF:
                    return( !AVI_PacketNext( p_input ) ? 1 : 0 );
                case AVIFOURCC_idx1:
                    if( p_sys->b_odml )
                    {
                        return( !AVI_PacketNext( p_input ) ? 1 : 0 );
                    }
                    return( 0 );    /* eof */
                default:
                    msg_Warn( p_input,
                              "seems to have lost position, resync" );
                    if( AVI_PacketSearch( p_input ) )
                    {
                        msg_Err( p_input, "resync failed" );
                        return( -1 );
                    }
            }
        }
        else
        {
            /* do will send this packet to decoder ? */
            if( !b_audio && avi_pk.i_cat == AUDIO_ES )
            {
                if( AVI_PacketNext( p_input ) )
                {
                    return( 0 );
                }
            }
            else
            {
                /* it's a selected stream, check for time */
                if( __ABS( AVI_GetPTS( p_stream ) -
                            AVI_GetPTS( p_stream_master ) )< 600*1000 )
                {
                    /* load it and send to decoder */
                    block_t *p_frame;
                    if( AVI_PacketRead( p_input, &avi_pk, &p_frame ) || p_frame == NULL )
                    {
                        return( -1 );
                    }
                    p_frame->i_pts =
                        input_ClockGetTS( p_input,
                                          p_input->stream.p_selected_program,
                                          AVI_GetPTS( p_stream ) * 9/100);

                    if( avi_pk.i_cat != VIDEO_ES )
                        p_frame->i_dts = p_frame->i_pts;
                    else
                    {
                        p_frame->i_dts = p_frame->i_pts;
                        p_frame->i_pts = 0;
                    }

                    //p_pes->i_rate = p_input->stream.control.i_rate;
                    es_out_Send( p_input->p_es_out, p_stream->p_es, p_frame );
                }
                else
                {
                    if( AVI_PacketNext( p_input ) )
                    {
                        return( 0 );
                    }
                }
            }

            /* *** update stream time position *** */
            if( p_stream->i_samplesize )
            {
                p_stream->i_idxposb += avi_pk.i_size;
            }
            else
            {
                p_stream->i_idxposc++;
            }

        }
#undef p_stream
    }

    return( 1 );
}

/*****************************************************************************
 * Seek: goto to i_date or i_percent
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Seek( input_thread_t *p_input, mtime_t i_date, int i_percent )
{

    demux_sys_t *p_sys = p_input->p_demux_data;
    unsigned int i_stream;
    msg_Dbg( p_input,
             "seek requested: "I64Fd" secondes %d%%",
             i_date / 1000000,
             i_percent );

    if( p_sys->b_seekable )
    {
        if( !p_sys->i_length )
        {
            avi_track_t *p_stream;
            int64_t i_pos;

            /* use i_percent to create a true i_date */
            msg_Warn( p_input,
                      "mmh, seeking without index at %d%%"
                      " work only for interleaved file", i_percent );
            if( i_percent >= 100 )
            {
                msg_Warn( p_input, "cannot seek so far !" );
                return( -1 );
            }
            i_percent = __MAX( i_percent, 0 );

            /* try to find chunk that is at i_percent or the file */
            i_pos = __MAX( i_percent *
                           stream_Size( p_input->s ) / 100,
                           p_sys->i_movi_begin );
            /* search first selected stream */
            for( i_stream = 0, p_stream = NULL;
                        i_stream < p_sys->i_track; i_stream++ )
            {
                p_stream = p_sys->track[i_stream];
                if( p_stream->b_activated )
                {
                    break;
                }
            }
            if( !p_stream || !p_stream->b_activated )
            {
                msg_Warn( p_input, "cannot find any selected stream" );
                return( -1 );
            }

            /* be sure that the index exist */
            if( AVI_StreamChunkSet( p_input,
                                    i_stream,
                                    0 ) )
            {
                msg_Warn( p_input, "cannot seek" );
                return( -1 );
            }

            while( i_pos >= p_stream->p_index[p_stream->i_idxposc].i_pos +
               p_stream->p_index[p_stream->i_idxposc].i_length + 8 )
            {
                /* search after i_idxposc */
                if( AVI_StreamChunkSet( p_input,
                                        i_stream, p_stream->i_idxposc + 1 ) )
                {
                    msg_Warn( p_input, "cannot seek" );
                    return( -1 );
                }
            }
            i_date = AVI_GetPTS( p_stream );
            /* TODO better support for i_samplesize != 0 */
            msg_Dbg( p_input, "estimate date "I64Fd, i_date );
        }

#define p_stream    p_sys->track[i_stream]
        p_sys->i_time = 0;
        /* seek for chunk based streams */
        for( i_stream = 0; i_stream < p_sys->i_track; i_stream++ )
        {
            if( p_stream->b_activated && !p_stream->i_samplesize )
/*            if( p_stream->b_activated ) */
            {
                AVI_TrackSeek( p_input, i_stream, i_date );
                p_sys->i_time = __MAX( AVI_GetPTS( p_stream ),
                                        p_sys->i_time );
            }
        }
#if 1
        if( p_sys->i_time )
        {
            i_date = p_sys->i_time;
        }
        /* seek for bytes based streams */
        for( i_stream = 0; i_stream < p_sys->i_track; i_stream++ )
        {
            if( p_stream->b_activated && p_stream->i_samplesize )
            {
                AVI_TrackSeek( p_input, i_stream, i_date );
/*                p_sys->i_time = __MAX( AVI_GetPTS( p_stream ), p_sys->i_time );*/
            }
        }
        msg_Dbg( p_input, "seek: "I64Fd" secondes", p_sys->i_time /1000000 );
        /* set true movie time */
#endif
        if( !p_sys->i_time )
        {
            p_sys->i_time = i_date;
        }
#undef p_stream
        return( 1 );
    }
    else
    {
        msg_Err( p_input, "shouldn't yet be executed" );
        return( -1 );
    }
}

/*****************************************************************************
 * Control:
 *****************************************************************************
 *
 *****************************************************************************/
static double ControlGetPosition( input_thread_t *p_input )
{
    demux_sys_t *p_sys = p_input->p_demux_data;

    if( p_sys->i_length > 0 )
    {
        return (double)p_sys->i_time / (double)( p_sys->i_length * (mtime_t)1000000 );
    }
    else if( stream_Size( p_input->s ) > 0 )
    {
        unsigned int i;
        int64_t i_tmp;
        int64_t i64 = 0;

        /* search the more advanced selected es */
        for( i = 0; i < p_sys->i_track; i++ )
        {
            avi_track_t *tk = p_sys->track[i];
            if( tk->b_activated && tk->i_idxposc < tk->i_idxnb )
            {
                i_tmp = tk->p_index[tk->i_idxposc].i_pos +
                        tk->p_index[tk->i_idxposc].i_length + 8;
                if( i_tmp > i64 )
                {
                    i64 = i_tmp;
                }
            }
        }
        return (double)i64 / (double)stream_Size( p_input->s );
    }
    return 0.0;
}

static int    Control( input_thread_t *p_input, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_input->p_demux_data;
    int i;
    double   f, *pf;
    int64_t i64, *pi64;

    switch( i_query )
    {
        case DEMUX_GET_POSITION:
            pf = (double*)va_arg( args, double * );
            *pf = ControlGetPosition( p_input );
            return VLC_SUCCESS;
        case DEMUX_SET_POSITION:
            if( p_sys->b_seekable )
            {
                f = (double)va_arg( args, double );
                i64 = (mtime_t)(1000000.0 * p_sys->i_length * f );
                return Seek( p_input, i64, (int)(f * 100) );
            }
            return demux_vaControlDefault( p_input, i_query, args );

        case DEMUX_GET_TIME:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = p_sys->i_time;
            return VLC_SUCCESS;

        case DEMUX_SET_TIME:
        {
            int i_percent = 0;

            i64 = (int64_t)va_arg( args, int64_t );
            if( p_sys->i_length > 0 )
            {
                i_percent = 100 * i64 / (p_sys->i_length*1000000ULL);
            }
            else if( p_sys->i_time > 0 )
            {
                i_percent = (int)( 100.0 * ControlGetPosition( p_input ) *
                                   (double)i64 / (double)p_sys->i_time );
            }
            return Seek( p_input, i64, i_percent );
        }
        case DEMUX_GET_LENGTH:
            pi64 = (int64_t*)va_arg( args, int64_t * );
            *pi64 = p_sys->i_length * (mtime_t)1000000;
            return VLC_SUCCESS;

        case DEMUX_GET_FPS:
            pf = (double*)va_arg( args, double * );
            *pf = 0.0;
            for( i = 0; i < (int)p_sys->i_track; i++ )
            {
                avi_track_t *tk = p_sys->track[i];
                if( tk->i_cat == VIDEO_ES && tk->i_scale > 0)
                {
                    *pf = (float)tk->i_rate / (float)tk->i_scale;
                    break;
                }
            }
            return VLC_SUCCESS;

        default:
            return demux_vaControlDefault( p_input, i_query, args );
    }
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Function to convert pts to chunk or byte
 *****************************************************************************/

static mtime_t AVI_PTSToChunk( avi_track_t *tk, mtime_t i_pts )
{
    return (mtime_t)((int64_t)i_pts *
                     (int64_t)tk->i_rate /
                     (int64_t)tk->i_scale /
                     (int64_t)1000000 );
}
static mtime_t AVI_PTSToByte( avi_track_t *tk, mtime_t i_pts )
{
    return (mtime_t)((int64_t)i_pts *
                     (int64_t)tk->i_rate /
                     (int64_t)tk->i_scale /
                     (int64_t)1000000 *
                     (int64_t)tk->i_samplesize );
}

static mtime_t AVI_GetDPTS( avi_track_t *tk, int64_t i_count )
{
    mtime_t i_dpts;

    i_dpts = (mtime_t)( (int64_t)1000000 *
                        (int64_t)i_count *
                        (int64_t)tk->i_scale /
                        (int64_t)tk->i_rate );

    if( tk->i_samplesize )
    {
        return i_dpts / tk->i_samplesize;
    }
    return i_dpts;
}

static mtime_t AVI_GetPTS( avi_track_t *tk )
{
    if( tk->i_samplesize )
    {
        int64_t i_count = 0;

        /* we need a valid entry we will emulate one */
        if( tk->i_idxposc == tk->i_idxnb )
        {
            if( tk->i_idxposc )
            {
                /* use the last entry */
                i_count = tk->p_index[tk->i_idxnb - 1].i_lengthtotal
                            + tk->p_index[tk->i_idxnb - 1].i_length;
            }
        }
        else
        {
            i_count = tk->p_index[tk->i_idxposc].i_lengthtotal;
        }
        return AVI_GetDPTS( tk, i_count + tk->i_idxposb );
    }
    else
    {
        return AVI_GetDPTS( tk, tk->i_idxposc );
    }
}

static int AVI_StreamChunkFind( input_thread_t *p_input,
                                unsigned int i_stream )
{
    demux_sys_t *p_sys = p_input->p_demux_data;
    avi_packet_t avi_pk;

    /* find first chunk of i_stream that isn't in index */

    if( p_sys->i_movi_lastchunk_pos >= p_sys->i_movi_begin + 12 )
    {
        stream_Seek( p_input->s, p_sys->i_movi_lastchunk_pos );
        if( AVI_PacketNext( p_input ) )
        {
            return VLC_EGENERIC;
        }
    }
    else
    {
        stream_Seek( p_input->s, p_sys->i_movi_begin + 12 );
    }

    for( ;; )
    {
        if( p_input->b_die )
        {
            return VLC_EGENERIC;
        }

        if( AVI_PacketGetHeader( p_input, &avi_pk ) )
        {
            msg_Warn( p_input, "cannot get packet header" );
            return VLC_EGENERIC;
        }
        if( avi_pk.i_stream >= p_sys->i_track ||
            ( avi_pk.i_cat != AUDIO_ES && avi_pk.i_cat != VIDEO_ES ) )
        {
            if( AVI_PacketNext( p_input ) )
            {
                return VLC_EGENERIC;
            }
        }
        else
        {
            /* add this chunk to the index */
            AVIIndexEntry_t index;

            index.i_id = avi_pk.i_fourcc;
            index.i_flags =
               AVI_GetKeyFlag(p_sys->track[avi_pk.i_stream]->i_codec,
                              avi_pk.i_peek);
            index.i_pos = avi_pk.i_pos;
            index.i_length = avi_pk.i_size;
            AVI_IndexAddEntry( p_sys, avi_pk.i_stream, &index );

            if( avi_pk.i_stream == i_stream  )
            {
                return VLC_SUCCESS;
            }

            if( AVI_PacketNext( p_input ) )
            {
                return VLC_EGENERIC;
            }
        }
    }
}


/* be sure that i_ck will be a valid index entry */
static int AVI_StreamChunkSet( input_thread_t    *p_input,
                               unsigned int i_stream,
                               unsigned int i_ck )
{
    demux_sys_t *p_sys = p_input->p_demux_data;
    avi_track_t *p_stream = p_sys->track[i_stream];

    p_stream->i_idxposc = i_ck;
    p_stream->i_idxposb = 0;

    if(  i_ck >= p_stream->i_idxnb )
    {
        p_stream->i_idxposc = p_stream->i_idxnb - 1;
        do
        {
            p_stream->i_idxposc++;
            if( AVI_StreamChunkFind( p_input, i_stream ) )
            {
                return VLC_EGENERIC;
            }

        } while( p_stream->i_idxposc < i_ck );
    }

    return VLC_SUCCESS;
}


/* XXX FIXME up to now, we assume that all chunk are one after one */
static int AVI_StreamBytesSet( input_thread_t    *p_input,
                               unsigned int i_stream,
                               off_t   i_byte )
{
    demux_sys_t *p_sys = p_input->p_demux_data;
    avi_track_t *p_stream = p_sys->track[i_stream];

    if( ( p_stream->i_idxnb > 0 )
        &&( i_byte < p_stream->p_index[p_stream->i_idxnb - 1].i_lengthtotal +
                p_stream->p_index[p_stream->i_idxnb - 1].i_length ) )
    {
        /* index is valid to find the ck */
        /* uses dichototmie to be fast enougth */
        int i_idxposc = __MIN( p_stream->i_idxposc, p_stream->i_idxnb - 1 );
        int i_idxmax  = p_stream->i_idxnb;
        int i_idxmin  = 0;
        for( ;; )
        {
            if( p_stream->p_index[i_idxposc].i_lengthtotal > i_byte )
            {
                i_idxmax  = i_idxposc ;
                i_idxposc = ( i_idxmin + i_idxposc ) / 2 ;
            }
            else
            {
                if( p_stream->p_index[i_idxposc].i_lengthtotal +
                        p_stream->p_index[i_idxposc].i_length <= i_byte)
                {
                    i_idxmin  = i_idxposc ;
                    i_idxposc = (i_idxmax + i_idxposc ) / 2 ;
                }
                else
                {
                    p_stream->i_idxposc = i_idxposc;
                    p_stream->i_idxposb = i_byte -
                            p_stream->p_index[i_idxposc].i_lengthtotal;
                    return VLC_SUCCESS;
                }
            }
        }

    }
    else
    {
        p_stream->i_idxposc = p_stream->i_idxnb - 1;
        p_stream->i_idxposb = 0;
        do
        {
            p_stream->i_idxposc++;
            if( AVI_StreamChunkFind( p_input, i_stream ) )
            {
                return VLC_EGENERIC;
            }

        } while( p_stream->p_index[p_stream->i_idxposc].i_lengthtotal +
                    p_stream->p_index[p_stream->i_idxposc].i_length <= i_byte );

        p_stream->i_idxposb = i_byte -
                       p_stream->p_index[p_stream->i_idxposc].i_lengthtotal;
        return VLC_SUCCESS;
    }
}

static int AVI_TrackSeek( input_thread_t *p_input,
                           int i_stream,
                           mtime_t i_date )
{
    demux_sys_t  *p_sys = p_input->p_demux_data;
#define p_stream    p_sys->track[i_stream]
    mtime_t i_oldpts;

    i_oldpts = AVI_GetPTS( p_stream );

    if( !p_stream->i_samplesize )
    {
        if( AVI_StreamChunkSet( p_input,
                                i_stream,
                                AVI_PTSToChunk( p_stream, i_date ) ) )
        {
            return VLC_EGENERIC;
        }

        msg_Dbg( p_input,
                 "old:"I64Fd" %s new "I64Fd,
                 i_oldpts,
                 i_oldpts > i_date ? ">" : "<",
                 i_date );

        if( p_stream->i_cat == VIDEO_ES )
        {
            /* search key frame */
            if( i_date < i_oldpts )
            {
                while( p_stream->i_idxposc > 0 &&
                   !( p_stream->p_index[p_stream->i_idxposc].i_flags &
                                                                AVIIF_KEYFRAME ) )
                {
                    if( AVI_StreamChunkSet( p_input,
                                            i_stream,
                                            p_stream->i_idxposc - 1 ) )
                    {
                        return VLC_EGENERIC;
                    }
                }
            }
            else
            {
                while( p_stream->i_idxposc < p_stream->i_idxnb &&
                        !( p_stream->p_index[p_stream->i_idxposc].i_flags &
                                                                AVIIF_KEYFRAME ) )
                {
                    if( AVI_StreamChunkSet( p_input,
                                            i_stream,
                                            p_stream->i_idxposc + 1 ) )
                    {
                        return VLC_EGENERIC;
                    }
                }
            }
        }
    }
    else
    {
        if( AVI_StreamBytesSet( p_input,
                                i_stream,
                                AVI_PTSToByte( p_stream, i_date ) ) )
        {
            return VLC_EGENERIC;
        }
    }
    return VLC_SUCCESS;
#undef p_stream
}

/****************************************************************************
 * Return VLC_TRUE if it's a key frame
 ****************************************************************************/
static int AVI_GetKeyFlag( vlc_fourcc_t i_fourcc, uint8_t *p_byte )
{
    switch( i_fourcc )
    {
        case FOURCC_DIV1:
            /* we have:
             *  startcode:      0x00000100   32bits
             *  framenumber     ?             5bits
             *  piture type     0(I),1(P)     2bits
             */
            if( GetDWBE( p_byte ) != 0x00000100 )
            {
                /* it's not an msmpegv1 stream, strange...*/
                return AVIIF_KEYFRAME;
            }
            else
            {
                return p_byte[4] & 0x06 ? 0 : AVIIF_KEYFRAME;
            }
        case FOURCC_DIV2:
        case FOURCC_DIV3:   /* wmv1 also */
            /* we have
             *  picture type    0(I),1(P)     2bits
             */
            return p_byte[0] & 0xC0 ? 0 : AVIIF_KEYFRAME;
        case FOURCC_mp4v:
            /* we should find first occurence of 0x000001b6 (32bits)
             *  startcode:      0x000001b6   32bits
             *  piture type     0(I),1(P)     2bits
             */
            if( GetDWBE( p_byte ) != 0x000001b6 )
            {
                /* not true , need to find the first VOP header */
                return AVIIF_KEYFRAME;
            }
            else
            {
                return p_byte[4] & 0xC0 ? 0 : AVIIF_KEYFRAME;
            }
        default:
            /* I can't do it, so say yes */
            return AVIIF_KEYFRAME;
    }
}

vlc_fourcc_t AVI_FourccGetCodec( unsigned int i_cat, vlc_fourcc_t i_codec )
{
    switch( i_cat )
    {
        case AUDIO_ES:
            wf_tag_to_fourcc( i_codec, &i_codec, NULL );
            return i_codec;

        case VIDEO_ES:
            /* XXX DIV1 <- msmpeg4v1, DIV2 <- msmpeg4v2, DIV3 <- msmpeg4v3, mp4v for mpeg4 */
            switch( i_codec )
            {
                case FOURCC_DIV1:
                case FOURCC_div1:
                case FOURCC_MPG4:
                case FOURCC_mpg4:
                    return FOURCC_DIV1;
                case FOURCC_DIV2:
                case FOURCC_div2:
                case FOURCC_MP42:
                case FOURCC_mp42:
                case FOURCC_MPG3:
                case FOURCC_mpg3:
                    return FOURCC_DIV2;
                case FOURCC_div3:
                case FOURCC_MP43:
                case FOURCC_mp43:
                case FOURCC_DIV3:
                case FOURCC_DIV4:
                case FOURCC_div4:
                case FOURCC_DIV5:
                case FOURCC_div5:
                case FOURCC_DIV6:
                case FOURCC_div6:
                case FOURCC_AP41:
                case FOURCC_3IV1:
                case FOURCC_3iv1:
                case FOURCC_3IVD:
                case FOURCC_3ivd:
                case FOURCC_3VID:
                case FOURCC_3vid:
                    return FOURCC_DIV3;
                case FOURCC_DIVX:
                case FOURCC_divx:
                case FOURCC_MP4S:
                case FOURCC_mp4s:
                case FOURCC_M4S2:
                case FOURCC_m4s2:
                case FOURCC_xvid:
                case FOURCC_XVID:
                case FOURCC_XviD:
                case FOURCC_DX50:
                case FOURCC_mp4v:
                case FOURCC_4:
                case FOURCC_3IV2:
                case FOURCC_3iv2:
                    return FOURCC_mp4v;
            }
        default:
            return VLC_FOURCC( 'u', 'n', 'd', 'f' );
    }
}

/****************************************************************************
 *
 ****************************************************************************/
static void AVI_ParseStreamHeader( vlc_fourcc_t i_id,
                                   int *pi_number, int *pi_type )
{
#define SET_PTR( p, v ) if( p ) *(p) = (v);
    int c1, c2;

    c1 = ((uint8_t *)&i_id)[0];
    c2 = ((uint8_t *)&i_id)[1];

    if( c1 < '0' || c1 > '9' || c2 < '0' || c2 > '9' )
    {
        SET_PTR( pi_number, 100 ); /* > max stream number */
        SET_PTR( pi_type, UNKNOWN_ES );
    }
    else
    {
        SET_PTR( pi_number, (c1 - '0') * 10 + (c2 - '0' ) );
        switch( VLC_TWOCC( ((uint8_t *)&i_id)[2], ((uint8_t *)&i_id)[3] ) )
        {
            case AVITWOCC_wb:
                SET_PTR( pi_type, AUDIO_ES );
                break;
            case AVITWOCC_dc:
            case AVITWOCC_db:
                SET_PTR( pi_type, VIDEO_ES );
                break;
            default:
                SET_PTR( pi_type, UNKNOWN_ES );
                break;
        }
    }
#undef SET_PTR
}

/****************************************************************************
 *
 ****************************************************************************/
static int AVI_PacketGetHeader( input_thread_t *p_input, avi_packet_t *p_pk )
{
    uint8_t  *p_peek;

    if( stream_Peek( p_input->s, &p_peek, 16 ) < 16 )
    {
        return VLC_EGENERIC;
    }
    p_pk->i_fourcc  = VLC_FOURCC( p_peek[0], p_peek[1], p_peek[2], p_peek[3] );
    p_pk->i_size    = GetDWLE( p_peek + 4 );
    p_pk->i_pos     = stream_Tell( p_input->s );
    if( p_pk->i_fourcc == AVIFOURCC_LIST || p_pk->i_fourcc == AVIFOURCC_RIFF )
    {
        p_pk->i_type = VLC_FOURCC( p_peek[8],  p_peek[9],
                                   p_peek[10], p_peek[11] );
    }
    else
    {
        p_pk->i_type = 0;
    }

    memcpy( p_pk->i_peek, p_peek + 8, 8 );

    AVI_ParseStreamHeader( p_pk->i_fourcc, &p_pk->i_stream, &p_pk->i_cat );
    return VLC_SUCCESS;
}

static int AVI_PacketNext( input_thread_t *p_input )
{
    avi_packet_t    avi_ck;
    int             i_skip = 0;

    if( AVI_PacketGetHeader( p_input, &avi_ck ) )
    {
        return VLC_EGENERIC;
    }

    if( avi_ck.i_fourcc == AVIFOURCC_LIST &&
        ( avi_ck.i_type == AVIFOURCC_rec || avi_ck.i_type == AVIFOURCC_movi ) )
    {
        i_skip = 12;
    }
    else if( avi_ck.i_fourcc == AVIFOURCC_RIFF &&
             avi_ck.i_type == AVIFOURCC_AVIX )
    {
        i_skip = 24;
    }
    else
    {
        i_skip = __EVEN( avi_ck.i_size ) + 8;
    }

    if( stream_Read( p_input->s, NULL, i_skip ) != i_skip )
    {
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}
static int AVI_PacketRead( input_thread_t   *p_input,
                           avi_packet_t     *p_pk,
                           block_t          **pp_frame )
{
    size_t i_size;

    i_size = __EVEN( p_pk->i_size + 8 );

    if( ( *pp_frame = stream_Block( p_input->s, i_size ) ) == NULL )
    {
        return VLC_EGENERIC;
    }
    (*pp_frame)->p_buffer += 8;
    (*pp_frame)->i_buffer -= 8;

    if( i_size != p_pk->i_size + 8 )
    {
        (*pp_frame)->i_buffer--;
    }

    return VLC_SUCCESS;
}

static int AVI_PacketSearch( input_thread_t *p_input )
{
    demux_sys_t     *p_sys = p_input->p_demux_data;
    avi_packet_t    avi_pk;
    int             i_count = 0;

    for( ;; )
    {
        if( stream_Read( p_input->s, NULL, 1 ) != 1 )
        {
            return VLC_EGENERIC;
        }
        AVI_PacketGetHeader( p_input, &avi_pk );
        if( avi_pk.i_stream < p_sys->i_track &&
            ( avi_pk.i_cat == AUDIO_ES || avi_pk.i_cat == VIDEO_ES ) )
        {
            return VLC_SUCCESS;
        }
        switch( avi_pk.i_fourcc )
        {
            case AVIFOURCC_JUNK:
            case AVIFOURCC_LIST:
            case AVIFOURCC_RIFF:
            case AVIFOURCC_idx1:
                return VLC_SUCCESS;
        }

        /* Prevents from eating all the CPU with broken files.
         * This value should be low enough so that it doesn't affect the
         * reading speed too much (not that we care much anyway because
         * this code is called only on broken files). */
        if( !(++i_count % 1024) )
        {
            if( p_input->b_die ) return VLC_EGENERIC;

            msleep( 10000 );
            if( !(i_count % (1024 * 10)) )
                msg_Warn( p_input, "trying to resync..." );
        }
    }
}

/****************************************************************************
 * Index stuff.
 ****************************************************************************/
static void AVI_IndexAddEntry( demux_sys_t *p_sys,
                               int i_stream,
                               AVIIndexEntry_t *p_index)
{
    avi_track_t *tk = p_sys->track[i_stream];

    /* Update i_movi_lastchunk_pos */
    if( p_sys->i_movi_lastchunk_pos < p_index->i_pos )
    {
        p_sys->i_movi_lastchunk_pos = p_index->i_pos;
    }

    /* add the entry */
    if( tk->i_idxnb >= tk->i_idxmax )
    {
        tk->i_idxmax += 16384;
        tk->p_index = realloc( tk->p_index,
                               tk->i_idxmax * sizeof( AVIIndexEntry_t ) );
        if( tk->p_index == NULL )
        {
            return;
        }
    }
    /* calculate cumulate length */
    if( tk->i_idxnb > 0 )
    {
        p_index->i_lengthtotal =
            tk->p_index[tk->i_idxnb - 1].i_length +
                tk->p_index[tk->i_idxnb - 1].i_lengthtotal;
    }
    else
    {
        p_index->i_lengthtotal = 0;
    }

    tk->p_index[tk->i_idxnb++] = *p_index;
}

static int AVI_IndexLoad_idx1( input_thread_t *p_input )
{
    demux_sys_t *p_sys = p_input->p_demux_data;

    avi_chunk_list_t    *p_riff;
    avi_chunk_list_t    *p_movi;
    avi_chunk_idx1_t    *p_idx1;

    unsigned int i_stream;
    unsigned int i_index;
    off_t        i_offset;
    unsigned int i;

    p_riff = AVI_ChunkFind( &p_sys->ck_root, AVIFOURCC_RIFF, 0);
    p_idx1 = AVI_ChunkFind( p_riff, AVIFOURCC_idx1, 0);
    p_movi = AVI_ChunkFind( p_riff, AVIFOURCC_movi, 0);

    if( !p_idx1 )
    {
        msg_Warn( p_input, "cannot find idx1 chunk, no index defined" );
        return VLC_EGENERIC;
    }

    /* *** calculate offset *** */
    /* Well, avi is __SHIT__ so test more than one entry 
     * (needed for some avi files) */
    i_offset = 0;
    for( i = 0; i < __MIN( p_idx1->i_entry_count, 10 ); i++ )
    {
        if( p_idx1->entry[i].i_pos < p_movi->i_chunk_pos )
        {
            i_offset = p_movi->i_chunk_pos + 8;
            break;
        }
    }

    for( i_index = 0; i_index < p_idx1->i_entry_count; i_index++ )
    {
        unsigned int i_cat;

        AVI_ParseStreamHeader( p_idx1->entry[i_index].i_fourcc,
                               &i_stream,
                               &i_cat );
        if( i_stream < p_sys->i_track &&
            i_cat == p_sys->track[i_stream]->i_cat )
        {
            AVIIndexEntry_t index;
            index.i_id      = p_idx1->entry[i_index].i_fourcc;
            index.i_flags   =
                p_idx1->entry[i_index].i_flags&(~AVIIF_FIXKEYFRAME);
            index.i_pos     = p_idx1->entry[i_index].i_pos + i_offset;
            index.i_length  = p_idx1->entry[i_index].i_length;
            AVI_IndexAddEntry( p_sys, i_stream, &index );
        }
    }
    return VLC_SUCCESS;
}

static void __Parse_indx( input_thread_t    *p_input,
                          int               i_stream,
                          avi_chunk_indx_t  *p_indx )
{
    demux_sys_t         *p_sys    = p_input->p_demux_data;
    AVIIndexEntry_t     index;
    int32_t             i;

    msg_Dbg( p_input, "loading subindex(0x%x) %d entries", p_indx->i_indextype, p_indx->i_entriesinuse );
    if( p_indx->i_indexsubtype == 0 )
    {
        for( i = 0; i < p_indx->i_entriesinuse; i++ )
        {
            index.i_id      = p_indx->i_id;
            index.i_flags   = p_indx->idx.std[i].i_size & 0x80000000 ? 0 : AVIIF_KEYFRAME;
            index.i_pos     = p_indx->i_baseoffset + p_indx->idx.std[i].i_offset - 8;
            index.i_length  = p_indx->idx.std[i].i_size&0x7fffffff;

            AVI_IndexAddEntry( p_sys, i_stream, &index );
        }
    }
    else if( p_indx->i_indexsubtype == AVI_INDEX_2FIELD )
    {
        for( i = 0; i < p_indx->i_entriesinuse; i++ )
        {
            index.i_id      = p_indx->i_id;
            index.i_flags   = p_indx->idx.field[i].i_size & 0x80000000 ? 0 : AVIIF_KEYFRAME;
            index.i_pos     = p_indx->i_baseoffset + p_indx->idx.field[i].i_offset - 8;
            index.i_length  = p_indx->idx.field[i].i_size;

            AVI_IndexAddEntry( p_sys, i_stream, &index );
        }
    }
    else
    {
        msg_Warn( p_input, "unknow subtype index(0x%x)", p_indx->i_indexsubtype );
    }
}

static void AVI_IndexLoad_indx( input_thread_t *p_input )
{
    demux_sys_t         *p_sys = p_input->p_demux_data;
    unsigned int        i_stream;
    int32_t             i;

    avi_chunk_list_t    *p_riff;
    avi_chunk_list_t    *p_hdrl;

    p_riff = AVI_ChunkFind( &p_sys->ck_root, AVIFOURCC_RIFF, 0);
    p_hdrl = AVI_ChunkFind( p_riff, AVIFOURCC_hdrl, 0 );

    for( i_stream = 0; i_stream < p_sys->i_track; i_stream++ )
    {
        avi_chunk_list_t    *p_strl;
        avi_chunk_indx_t    *p_indx;

#define p_stream  p_sys->track[i_stream]
        p_strl = AVI_ChunkFind( p_hdrl, AVIFOURCC_strl, i_stream );
        p_indx = AVI_ChunkFind( p_strl, AVIFOURCC_indx, 0 );

        if( !p_indx )
        {
            msg_Warn( p_input, "cannot find indx (misdetect/broken OpenDML file?)" );
            continue;
        }

        if( p_indx->i_indextype == AVI_INDEX_OF_CHUNKS )
        {
            __Parse_indx( p_input, i_stream, p_indx );
        }
        else if( p_indx->i_indextype == AVI_INDEX_OF_INDEXES )
        {
            avi_chunk_indx_t    ck_sub;
            for( i = 0; i < p_indx->i_entriesinuse; i++ )
            {
                if( stream_Seek( p_input->s, p_indx->idx.super[i].i_offset )||
                    AVI_ChunkRead( p_input->s, &ck_sub, NULL  ) )
                {
                    break;
                }
                __Parse_indx( p_input, i_stream, &ck_sub );
            }
        }
        else
        {
            msg_Warn( p_input, "unknow type index(0x%x)", p_indx->i_indextype );
        }
#undef p_stream
    }
}

static void AVI_IndexLoad( input_thread_t *p_input )
{
    demux_sys_t *p_sys = p_input->p_demux_data;
    unsigned int i_stream;

    for( i_stream = 0; i_stream < p_sys->i_track; i_stream++ )
    {
        p_sys->track[i_stream]->i_idxnb  = 0;
        p_sys->track[i_stream]->i_idxmax = 0;
        p_sys->track[i_stream]->p_index  = NULL;
    }

    if( p_sys->b_odml )
    {
        AVI_IndexLoad_indx( p_input );
    }
    else  if( AVI_IndexLoad_idx1( p_input ) )
    {
        /* try indx if idx1 failed as some "normal" file have indx too */
        AVI_IndexLoad_indx( p_input );
    }

    for( i_stream = 0; i_stream < p_sys->i_track; i_stream++ )
    {
        msg_Dbg( p_input, "stream[%d] created %d index entries",
                i_stream, p_sys->track[i_stream]->i_idxnb );
    }
}

static void AVI_IndexCreate( input_thread_t *p_input )
{
    demux_sys_t *p_sys = p_input->p_demux_data;

    avi_chunk_list_t    *p_riff;
    avi_chunk_list_t    *p_movi;

    unsigned int i_stream;
    off_t i_movi_end;

    p_riff = AVI_ChunkFind( &p_sys->ck_root, AVIFOURCC_RIFF, 0);
    p_movi = AVI_ChunkFind( p_riff, AVIFOURCC_movi, 0);

    if( !p_movi )
    {
        msg_Err( p_input, "cannot find p_movi" );
        return;
    }

    for( i_stream = 0; i_stream < p_sys->i_track; i_stream++ )
    {
        p_sys->track[i_stream]->i_idxnb  = 0;
        p_sys->track[i_stream]->i_idxmax = 0;
        p_sys->track[i_stream]->p_index  = NULL;
    }
    i_movi_end = __MIN( (off_t)(p_movi->i_chunk_pos + p_movi->i_chunk_size),
                        stream_Size( p_input->s ) );

    stream_Seek( p_input->s, p_movi->i_chunk_pos + 12 );
    msg_Warn( p_input, "creating index from LIST-movi, will take time !" );
    for( ;; )
    {
        avi_packet_t pk;

        if( p_input->b_die )
        {
            return;
        }

        if( AVI_PacketGetHeader( p_input, &pk ) )
        {
            break;
        }
        if( pk.i_stream < p_sys->i_track &&
            pk.i_cat == p_sys->track[pk.i_stream]->i_cat )
        {
            AVIIndexEntry_t index;
            index.i_id      = pk.i_fourcc;
            index.i_flags   =
               AVI_GetKeyFlag(p_sys->track[pk.i_stream]->i_codec, pk.i_peek);
            index.i_pos     = pk.i_pos;
            index.i_length  = pk.i_size;
            AVI_IndexAddEntry( p_sys, pk.i_stream, &index );
        }
        else
        {
            switch( pk.i_fourcc )
            {
                case AVIFOURCC_idx1:
                    if( p_sys->b_odml )
                    {
                        avi_chunk_list_t *p_sysx;
                        p_sysx = AVI_ChunkFind( &p_sys->ck_root,
                                                AVIFOURCC_RIFF, 1 );

                        msg_Dbg( p_input, "looking for new RIFF chunk" );
                        if( stream_Seek( p_input->s, p_sysx->i_chunk_pos + 24))
                        {
                            goto print_stat;
                        }
                        break;
                    }
                    goto print_stat;
                case AVIFOURCC_RIFF:
                        msg_Dbg( p_input, "new RIFF chunk found" );
                case AVIFOURCC_rec:
                case AVIFOURCC_JUNK:
                    break;
                default:
                    msg_Warn( p_input, "need resync, probably broken avi" );
                    if( AVI_PacketSearch( p_input ) )
                    {
                        msg_Warn( p_input, "lost sync, abord index creation" );
                        goto print_stat;
                    }
            }
        }

        if( ( !p_sys->b_odml && pk.i_pos + pk.i_size >= i_movi_end ) ||
            AVI_PacketNext( p_input ) )
        {
            break;
        }
    }

print_stat:
    for( i_stream = 0; i_stream < p_sys->i_track; i_stream++ )
    {
        msg_Dbg( p_input,
                "stream[%d] creating %d index entries",
                i_stream,
                p_sys->track[i_stream]->i_idxnb );
    }
}

/*****************************************************************************
 * Stream management
 *****************************************************************************/
static int AVI_TrackStopFinishedStreams( input_thread_t *p_input )
{
    demux_sys_t *p_sys = p_input->p_demux_data;
    unsigned int i;
    int b_end = VLC_TRUE;

    for( i = 0; i < p_sys->i_track; i++ )
    {
        avi_track_t *tk = p_sys->track[i];
        if( tk->i_idxposc >= tk->i_idxnb )
        {
            tk->b_activated = VLC_FALSE;
            es_out_Control( p_input->p_es_out, ES_OUT_SET_ES_STATE, tk->p_es, VLC_FALSE );
        }
        else
        {
            b_end = VLC_FALSE;
        }
    }
    return( b_end );
}

/****************************************************************************
 * AVI_MovieGetLength give max streams length in second
 ****************************************************************************/
static mtime_t  AVI_MovieGetLength( input_thread_t *p_input )
{
    demux_sys_t  *p_sys = p_input->p_demux_data;
    mtime_t      i_maxlength = 0;
    unsigned int i;

    for( i = 0; i < p_sys->i_track; i++ )
    {
        avi_track_t *tk = p_sys->track[i];
        mtime_t i_length;

        /* fix length for each stream */
        if( tk->i_idxnb < 1 || !tk->p_index )
        {
            continue;
        }

        if( tk->i_samplesize )
        {
            i_length = AVI_GetDPTS( tk,
                                    tk->p_index[tk->i_idxnb-1].i_lengthtotal +
                                        tk->p_index[tk->i_idxnb-1].i_length );
        }
        else
        {
            i_length = AVI_GetDPTS( tk, tk->i_idxnb );
        }
        i_length /= (mtime_t)1000000;    /* in seconds */

        msg_Dbg( p_input,
                 "stream[%d] length:"I64Fd" (based on index)",
                 i,
                 i_length );
        i_maxlength = __MAX( i_maxlength, i_length );
    }

    return i_maxlength;
}


