/*****************************************************************************
 * avi.c : AVI file Stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2004 the VideoLAN team
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

#include "vlc_meta.h"
#include "codecs.h"

#include "libavi.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define INTERLEAVE_TEXT N_("Force interleaved method" )
#define INTERLEAVE_LONGTEXT N_( "Force interleaved method" )

#define INDEX_TEXT N_("Force index creation")
#define INDEX_LONGTEXT N_( \
    "Recreate a index for the AVI file so we can seek trough it more reliably." )

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_shortname( "AVI" );
    set_description( _("AVI demuxer") );
    set_capability( "demux2", 212 );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );

    add_bool( "avi-interleaved", 0, NULL,
              INTERLEAVE_TEXT, INTERLEAVE_LONGTEXT, VLC_TRUE );
    add_bool( "avi-index", 0, NULL,
              INDEX_TEXT, INDEX_LONGTEXT, VLC_TRUE );

    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Control         ( demux_t *, int, va_list );
static int Seek            ( demux_t *, mtime_t, int );
static int Demux_Seekable  ( demux_t * );
static int Demux_UnSeekable( demux_t * );

#define FREE( p ) if( p ) { free( p ); (p) = NULL; }
#define __ABS( x ) ( (x) < 0 ? (-(x)) : (x) )

typedef struct
{
    vlc_fourcc_t i_fourcc;
    off_t        i_pos;
    uint32_t     i_size;
    vlc_fourcc_t i_type;     /* only for AVIFOURCC_LIST */

    uint8_t      i_peek[8];  /* first 8 bytes */

    unsigned int i_stream;
    unsigned int i_cat;
} avi_packet_t;


typedef struct
{
    vlc_fourcc_t i_id;
    uint32_t     i_flags;
    off_t        i_pos;
    uint32_t     i_length;
    uint32_t     i_lengthtotal;

} avi_entry_t;

typedef struct
{
    vlc_bool_t      b_activated;

    unsigned int    i_cat; /* AUDIO_ES, VIDEO_ES */
    vlc_fourcc_t    i_codec;

    int             i_rate;
    int             i_scale;
    int             i_samplesize;

    es_out_id_t     *p_es;

    avi_entry_t     *p_index;
    unsigned int        i_idxnb;
    unsigned int        i_idxmax;

    unsigned int        i_idxposc;  /* numero of chunk */
    unsigned int        i_idxposb;  /* byte in the current chunk */

    /* For VBR audio only */
    unsigned int        i_blockno;
    unsigned int        i_blocksize;
} avi_track_t;

struct demux_sys_t
{
    mtime_t i_time;
    mtime_t i_length;

    vlc_bool_t  b_seekable;
    avi_chunk_t ck_root;

    vlc_bool_t  b_odml;

    off_t   i_movi_begin;
    off_t   i_movi_lastchunk_pos;   /* XXX position of last valid chunk */

    /* number of streams and information */
    unsigned int i_track;
    avi_track_t  **track;

    /* meta */
    vlc_meta_t  *meta;
};

static inline off_t __EVEN( off_t i )
{
    return (i & 1) ? i + 1 : i;
}

static mtime_t AVI_PTSToChunk( avi_track_t *, mtime_t i_pts );
static mtime_t AVI_PTSToByte ( avi_track_t *, mtime_t i_pts );
static mtime_t AVI_GetDPTS   ( avi_track_t *, int64_t i_count );
static mtime_t AVI_GetPTS    ( avi_track_t * );


static int AVI_StreamChunkFind( demux_t *, unsigned int i_stream );
static int AVI_StreamChunkSet ( demux_t *,
                                unsigned int i_stream, unsigned int i_ck );
static int AVI_StreamBytesSet ( demux_t *,
                                unsigned int i_stream, off_t   i_byte );

vlc_fourcc_t AVI_FourccGetCodec( unsigned int i_cat, vlc_fourcc_t );
static int   AVI_GetKeyFlag    ( vlc_fourcc_t , uint8_t * );

static int AVI_PacketGetHeader( demux_t *, avi_packet_t *p_pk );
static int AVI_PacketNext     ( demux_t * );
static int AVI_PacketRead     ( demux_t *, avi_packet_t *, block_t **);
static int AVI_PacketSearch   ( demux_t * );

static void AVI_IndexLoad    ( demux_t * );
static void AVI_IndexCreate  ( demux_t * );
static void AVI_IndexAddEntry( demux_sys_t *, int, avi_entry_t * );

static mtime_t  AVI_MovieGetLength( demux_t * );

/*****************************************************************************
 * Stream management
 *****************************************************************************/
static int        AVI_TrackSeek  ( demux_t *, int, mtime_t );
static int        AVI_TrackStopFinishedStreams( demux_t *);

/* Remarks:
 - For VBR mp3 stream:
    count blocks by rounded-up chunksizes instead of chunks
    we need full emulation of dshow avi demuxer bugs :(
    fixes silly nandub-style a-v delaying in avi with vbr mp3...
    (from mplayer 2002/08/02)
 - to complete....
 */

/*****************************************************************************
 * Open: check file and initializes AVI structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t  *p_demux = (demux_t *)p_this;
    demux_sys_t     *p_sys;

    avi_chunk_t         ck_riff;
    avi_chunk_list_t    *p_riff = (avi_chunk_list_t*)&ck_riff;
    avi_chunk_list_t    *p_hdrl, *p_movi;
    avi_chunk_avih_t    *p_avih;

    unsigned int i_track;
    unsigned int i, i_peeker;

    uint8_t  *p_peek;

    /* Is it an avi file ? */
    if( stream_Peek( p_demux->s, &p_peek, 200 ) < 200 ) return VLC_EGENERIC;

    for( i_peeker = 0; i_peeker < 188; i_peeker++ )
    {
        if( !strncmp( (char *)&p_peek[0], "RIFF", 4 ) &&
            !strncmp( (char *)&p_peek[8], "AVI ", 4 ) ) break;
        p_peek++;
    }
    if( i_peeker == 188 )
    {
        return VLC_EGENERIC;
    }

    /* Initialize input  structures. */
    p_sys = p_demux->p_sys = malloc( sizeof(demux_sys_t) );
    memset( p_sys, 0, sizeof( demux_sys_t ) );
    p_sys->i_time   = 0;
    p_sys->i_length = 0;
    p_sys->i_movi_lastchunk_pos = 0;
    p_sys->b_odml   = VLC_FALSE;
    p_sys->i_track  = 0;
    p_sys->track    = NULL;
    p_sys->meta     = NULL;

    stream_Control( p_demux->s, STREAM_CAN_FASTSEEK, &p_sys->b_seekable );

    p_demux->pf_control = Control;
    p_demux->pf_demux = Demux_Seekable;

    /* For unseekable stream, automaticaly use Demux_UnSeekable */
    if( !p_sys->b_seekable || config_GetInt( p_demux, "avi-interleaved" ) )
    {
        p_demux->pf_demux = Demux_UnSeekable;
    }

    if( i_peeker > 0 )
    {
        stream_Read( p_demux->s, NULL, i_peeker );
    }

    if( AVI_ChunkReadRoot( p_demux->s, &p_sys->ck_root ) )
    {
        msg_Err( p_demux, "avi module discarded (invalid file)" );
        return VLC_EGENERIC;
    }

    if( AVI_ChunkCount( &p_sys->ck_root, AVIFOURCC_RIFF ) > 1 )
    {
        unsigned int i_count =
            AVI_ChunkCount( &p_sys->ck_root, AVIFOURCC_RIFF );

        msg_Warn( p_demux, "multiple riff -> OpenDML ?" );
        for( i = 1; i < i_count; i++ )
        {
            avi_chunk_list_t *p_sysx;

            p_sysx = AVI_ChunkFind( &p_sys->ck_root, AVIFOURCC_RIFF, i );
            if( p_sysx->i_type == AVIFOURCC_AVIX )
            {
                msg_Warn( p_demux, "detected OpenDML file" );
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
        msg_Err( p_demux, "avi module discarded (invalid file)" );
        goto error;
    }

    if( !( p_avih = AVI_ChunkFind( p_hdrl, AVIFOURCC_avih, 0 ) ) )
    {
        msg_Err( p_demux, "cannot find avih chunk" );
        goto error;
    }
    i_track = AVI_ChunkCount( p_hdrl, AVIFOURCC_strl );
    if( p_avih->i_streams != i_track )
    {
        msg_Warn( p_demux,
                  "found %d stream but %d are declared",
                  i_track, p_avih->i_streams );
    }
    if( i_track == 0 )
    {
        msg_Err( p_demux, "no stream defined!" );
        goto error;
    }

    /* print information on streams */
    msg_Dbg( p_demux, "AVIH: %d stream, flags %s%s%s%s ",
             i_track,
             p_avih->i_flags&AVIF_HASINDEX?" HAS_INDEX":"",
             p_avih->i_flags&AVIF_MUSTUSEINDEX?" MUST_USE_INDEX":"",
             p_avih->i_flags&AVIF_ISINTERLEAVED?" IS_INTERLEAVED":"",
             p_avih->i_flags&AVIF_TRUSTCKTYPE?" TRUST_CKTYPE":"" );
    if( ( p_sys->meta = vlc_meta_New() ) )
    {
        char buffer[200];
        sprintf( buffer, "%s%s%s%s",
                 p_avih->i_flags&AVIF_HASINDEX?" HAS_INDEX":"",
                 p_avih->i_flags&AVIF_MUSTUSEINDEX?" MUST_USE_INDEX":"",
                 p_avih->i_flags&AVIF_ISINTERLEAVED?" IS_INTERLEAVED":"",
                 p_avih->i_flags&AVIF_TRUSTCKTYPE?" TRUST_CKTYPE":"" );
        vlc_meta_Add( p_sys->meta, VLC_META_SETTING, buffer );
    }

    /* now read info on each stream and create ES */
    for( i = 0 ; i < i_track; i++ )
    {
        avi_track_t      *tk = malloc( sizeof( avi_track_t ) );
        avi_chunk_list_t *p_strl = AVI_ChunkFind( p_hdrl, AVIFOURCC_strl, i );
        avi_chunk_strh_t *p_strh = AVI_ChunkFind( p_strl, AVIFOURCC_strh, 0 );
        avi_chunk_STRING_t *p_strn = AVI_ChunkFind( p_strl, AVIFOURCC_strn, 0 );
        avi_chunk_strf_auds_t *p_auds;
        avi_chunk_strf_vids_t *p_vids;
        es_format_t fmt;

        tk->b_activated = VLC_FALSE;
        tk->p_index     = 0;
        tk->i_idxnb     = 0;
        tk->i_idxmax    = 0;
        tk->i_idxposc   = 0;
        tk->i_idxposb   = 0;

        tk->i_blockno   = 0;
        tk->i_blocksize = 0;

        p_vids = (avi_chunk_strf_vids_t*)AVI_ChunkFind( p_strl, AVIFOURCC_strf, 0 );
        p_auds = (avi_chunk_strf_auds_t*)AVI_ChunkFind( p_strl, AVIFOURCC_strf, 0 );

        if( p_strl == NULL || p_strh == NULL || p_auds == NULL || p_vids == NULL )
        {
            msg_Warn( p_demux, "stream[%d] incomplete", i );
            continue;
        }

        tk->i_rate  = p_strh->i_rate;
        tk->i_scale = p_strh->i_scale;
        tk->i_samplesize = p_strh->i_samplesize;
        msg_Dbg( p_demux, "stream[%d] rate:%d scale:%d samplesize:%d",
                 i, tk->i_rate, tk->i_scale, tk->i_samplesize );

        switch( p_strh->i_type )
        {
            case( AVIFOURCC_auds ):
                tk->i_cat   = AUDIO_ES;
                tk->i_codec = AVI_FourccGetCodec( AUDIO_ES,
                                                  p_auds->p_wf->wFormatTag );
                if( ( tk->i_blocksize = p_auds->p_wf->nBlockAlign ) == 0 )
                {
                    if( p_auds->p_wf->wFormatTag == 1 )
                    {
                        tk->i_blocksize = p_auds->p_wf->nChannels * (p_auds->p_wf->wBitsPerSample/8);
                    }
                    else
                    {
                        tk->i_blocksize = 1;
                    }
                }
                es_format_Init( &fmt, AUDIO_ES, tk->i_codec );

                fmt.audio.i_channels        = p_auds->p_wf->nChannels;
                fmt.audio.i_rate            = p_auds->p_wf->nSamplesPerSec;
                fmt.i_bitrate               = p_auds->p_wf->nAvgBytesPerSec*8;
                fmt.audio.i_blockalign      = p_auds->p_wf->nBlockAlign;
                fmt.audio.i_bitspersample   = p_auds->p_wf->wBitsPerSample;
                fmt.i_extra = __MIN( p_auds->p_wf->cbSize,
                    p_auds->i_chunk_size - sizeof(WAVEFORMATEX) );
                fmt.p_extra = &p_auds->p_wf[1];
                msg_Dbg( p_demux, "stream[%d] audio(0x%x) %d channels %dHz %dbits",
                         i, p_auds->p_wf->wFormatTag, p_auds->p_wf->nChannels,
                         p_auds->p_wf->nSamplesPerSec, p_auds->p_wf->wBitsPerSample);
                break;

            case( AVIFOURCC_vids ):
                tk->i_cat   = VIDEO_ES;
                tk->i_codec = AVI_FourccGetCodec( VIDEO_ES,
                                                  p_vids->p_bih->biCompression );
                if( p_vids->p_bih->biCompression == 0x00 )
                {
                    switch( p_vids->p_bih->biBitCount )
                    {
                        case 32:
                            tk->i_codec = VLC_FOURCC('R','V','3','2');
                            break;
                        case 24:
                            tk->i_codec = VLC_FOURCC('R','V','2','4');
                            break;
                        case 16:
                            /* tk->i_codec = VLC_FOURCC('R','V','1','6');*/
                            /* break;*/
                        case 15:
                            tk->i_codec = VLC_FOURCC('R','V','1','5');
                            break;
                        case 9:
                            tk->i_codec = VLC_FOURCC( 'Y', 'V', 'U', '9' ); /* <- TODO check that */
                            break;
                    }
                    es_format_Init( &fmt, VIDEO_ES, tk->i_codec );

                    if( p_vids->p_bih->biBitCount == 24 )
                    {
                        /* This is in BGR format */
                        fmt.video.i_bmask = 0x00ff0000;
                        fmt.video.i_gmask = 0x0000ff00;
                        fmt.video.i_rmask = 0x000000ff;
                    }
                }
                else
                {
                    es_format_Init( &fmt, VIDEO_ES, p_vids->p_bih->biCompression );
                    if( tk->i_codec == FOURCC_mp4v &&
                        !strncasecmp( (char*)&p_strh->i_handler, "XVID", 4 ) )
                    {
                        fmt.i_codec = VLC_FOURCC( 'X', 'V', 'I', 'D' );
                    }
                }
                tk->i_samplesize = 0;
                fmt.video.i_width  = p_vids->p_bih->biWidth;
                fmt.video.i_height = p_vids->p_bih->biHeight;
                fmt.video.i_bits_per_pixel = p_vids->p_bih->biBitCount;
                fmt.video.i_frame_rate = tk->i_rate;
                fmt.video.i_frame_rate_base = tk->i_scale;
                fmt.i_extra =
                    __MIN( p_vids->p_bih->biSize - sizeof( BITMAPINFOHEADER ),
                           p_vids->i_chunk_size - sizeof(BITMAPINFOHEADER) );
                fmt.p_extra = &p_vids->p_bih[1];
                msg_Dbg( p_demux, "stream[%d] video(%4.4s) %dx%d %dbpp %ffps",
                         i, (char*)&p_vids->p_bih->biCompression,
                         p_vids->p_bih->biWidth,
                         p_vids->p_bih->biHeight,
                         p_vids->p_bih->biBitCount,
                         (float)tk->i_rate/(float)tk->i_scale );

                if( p_vids->p_bih->biCompression == 0x00 )
                {
                    /* RGB DIB are coded from bottom to top */
                    fmt.video.i_height =
                        (unsigned int)(-(int)p_vids->p_bih->biHeight);
                }

                /* Extract palette from extradata if bpp <= 8
                 * (assumes that extradata contains only palette but appears
                 *  to be true for all palettized codecs we support) */
                if( fmt.i_extra && fmt.video.i_bits_per_pixel <= 8 &&
                    fmt.video.i_bits_per_pixel > 0 )
                {
                    int i;

                    fmt.video.p_palette = calloc( sizeof(video_palette_t), 1 );
                    fmt.video.p_palette->i_entries = 1;

                    /* Apparently this is necessary. But why ? */
                    fmt.i_extra =
                        p_vids->i_chunk_size - sizeof(BITMAPINFOHEADER);
                    for( i = 0; i < __MIN(fmt.i_extra/4, 256); i++ )
                    {
                        ((uint32_t *)&fmt.video.p_palette->palette[0][0])[i] =
                            GetDWLE((uint32_t*)fmt.p_extra + i);
                    }
                }
                break;

            case( AVIFOURCC_txts):
                tk->i_cat   = SPU_ES;
                tk->i_codec = VLC_FOURCC( 's', 'u', 'b', 't' );
                msg_Dbg( p_demux, "stream[%d] subtitles", i );
                es_format_Init( &fmt, SPU_ES, tk->i_codec );
                break;

            case( AVIFOURCC_mids):
                msg_Dbg( p_demux, "stream[%d] midi is UNSUPPORTED", i );

            default:
                msg_Warn( p_demux, "stream[%d] unknown type", i );
                free( tk );
                continue;
        }
        if( p_strn )
        {
            fmt.psz_description = strdup( p_strn->p_str );
        }
        tk->p_es = es_out_Add( p_demux->out, &fmt );
        TAB_APPEND( p_sys->i_track, p_sys->track, tk );
    }

    if( p_sys->i_track <= 0 )
    {
        msg_Err( p_demux, "no valid track" );
        goto error;
    }

    if( config_GetInt( p_demux, "avi-index" ) )
    {
        if( p_sys->b_seekable )
        {
            AVI_IndexCreate( p_demux );
        }
        else
        {
            msg_Warn( p_demux, "cannot create index (unseekable stream)" );
            AVI_IndexLoad( p_demux );
        }
    }
    else
    {
        AVI_IndexLoad( p_demux );
    }

    /* *** movie length in sec *** */
    p_sys->i_length = AVI_MovieGetLength( p_demux );
    if( p_sys->i_length < (mtime_t)p_avih->i_totalframes *
                          (mtime_t)p_avih->i_microsecperframe /
                          (mtime_t)1000000 )
    {
        msg_Warn( p_demux, "broken or missing index, 'seek' will be axproximative or will have strange behavour" );
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
                msg_Warn( p_demux, "track[%d] cannot be fixed (BeOS MediaKit generated)", i );
                continue;
            }
            tk->i_samplesize = 1;
            tk->i_rate       = i_track_length  * (int64_t)1000000/ i_length;
            msg_Warn( p_demux, "track[%d] fixed with rate=%d scale=%d (BeOS MediaKit generated)", i, tk->i_rate, tk->i_scale );
        }
    }

    if( p_sys->b_seekable )
    {
        /* we have read all chunk so go back to movi */
        stream_Seek( p_demux->s, p_movi->i_chunk_pos );
    }
    /* Skip movi header */
    stream_Read( p_demux->s, NULL, 12 );

    p_sys->i_movi_begin = p_movi->i_chunk_pos;
    return VLC_SUCCESS;

error:
    if( p_sys->meta )
    {
        vlc_meta_Delete( p_sys->meta );
    }
    AVI_ChunkFreeRoot( p_demux->s, &p_sys->ck_root );
    free( p_sys );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close ( vlc_object_t * p_this )
{
    demux_t *    p_demux = (demux_t *)p_this;
    unsigned int i;
    demux_sys_t *p_sys = p_demux->p_sys  ;

    for( i = 0; i < p_sys->i_track; i++ )
    {
        if( p_sys->track[i] )
        {
            FREE( p_sys->track[i]->p_index );
            free( p_sys->track[i] );
        }
    }
    FREE( p_sys->track );
    AVI_ChunkFreeRoot( p_demux->s, &p_sys->ck_root );
    vlc_meta_Delete( p_sys->meta );

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

static int Demux_Seekable( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

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

        es_out_Control( p_demux->out, ES_OUT_GET_ES_STATE, tk->p_es, &b );
        if( b && !tk->b_activated )
        {
            if( p_sys->b_seekable)
            {
                AVI_TrackSeek( p_demux, i_track, p_sys->i_time );
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
        int64_t i_length = p_sys->i_length * (mtime_t)1000000;

        p_sys->i_time += 25*1000;  /* read 25ms */
        if( i_length > 0 )
        {
            if( p_sys->i_time >= i_length )
                return 0;
            return 1;
        }
        msg_Warn( p_demux, "no track selected, exiting..." );
        return 0;
    }

    /* wait for the good time */
    es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_sys->i_time + 1 );
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
            int i_loop_count = 0;

            /* no valid index, we will parse directly the stream
             * in case we fail we will disable all finished stream */
            if( p_sys->i_movi_lastchunk_pos >= p_sys->i_movi_begin + 12 )
            {
                stream_Seek( p_demux->s, p_sys->i_movi_lastchunk_pos );
                if( AVI_PacketNext( p_demux ) )
                {
                    return( AVI_TrackStopFinishedStreams( p_demux ) ? 0 : 1 );
                }
            }
            else
            {
                stream_Seek( p_demux->s, p_sys->i_movi_begin + 12 );
            }

            for( ;; )
            {
                avi_packet_t avi_pk;

                if( AVI_PacketGetHeader( p_demux, &avi_pk ) )
                {
                    msg_Warn( p_demux,
                             "cannot get packet header, track disabled" );
                    return( AVI_TrackStopFinishedStreams( p_demux ) ? 0 : 1 );
                }
                if( avi_pk.i_stream >= p_sys->i_track ||
                    ( avi_pk.i_cat != AUDIO_ES && avi_pk.i_cat != VIDEO_ES ) )
                {
                    if( AVI_PacketNext( p_demux ) )
                    {
                        msg_Warn( p_demux,
                                  "cannot skip packet, track disabled" );
                        return( AVI_TrackStopFinishedStreams( p_demux ) ? 0 : 1 );
                    }

                    /* Prevents from eating all the CPU with broken files.
                     * This value should be low enough so that it doesn't
                     * affect the reading speed too much. */
                    if( !(++i_loop_count % 1024) )
                    {
                        if( p_demux->b_die ) return -1;
                        msleep( 10000 );

                        if( !(i_loop_count % (1024 * 10)) )
                            msg_Warn( p_demux,
                                      "don't seem to find any data..." );
                    }
                    continue;
                }
                else
                {
                    /* add this chunk to the index */
                    avi_entry_t index;

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
                        if( AVI_PacketNext( p_demux ) )
                        {
                            msg_Warn( p_demux,
                                      "cannot skip packet, track disabled" );
                            return( AVI_TrackStopFinishedStreams( p_demux ) ? 0 : 1 );
                        }
                    }
                }
            }

        }
        else
        {
            stream_Seek( p_demux->s, i_pos );
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
                    i_toread = AVI_PTSToByte( tk, 20 * 1000 );
                    i_toread = __MAX( i_toread, 100 );
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

        if( ( p_frame = stream_Block( p_demux->s, __EVEN( i_size ) ) )==NULL )
        {
            msg_Warn( p_demux, "failed reading data" );
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
        p_frame->i_pts = AVI_GetPTS( tk ) + 1;
        if( tk->p_index[tk->i_idxposc].i_flags&AVIIF_KEYFRAME )
        {
            p_frame->i_flags = BLOCK_FLAG_TYPE_I;
        }
        else
        {
            p_frame->i_flags = BLOCK_FLAG_TYPE_PB;
        }

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
            int i_length = tk->p_index[tk->i_idxposc].i_length;

            tk->i_idxposc++;
            if( tk->i_cat == AUDIO_ES )
            {
                tk->i_blockno += tk->i_blocksize > 0 ? ( i_length + tk->i_blocksize - 1 ) / tk->i_blocksize : 1;
            }
            toread[i_track].i_toread--;
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

        if( tk->i_cat != VIDEO_ES )
            p_frame->i_dts = p_frame->i_pts;
        else
        {
            p_frame->i_dts = p_frame->i_pts;
            p_frame->i_pts = 0;
        }

        //p_pes->i_rate = p_demux->stream.control.i_rate;
        es_out_Send( p_demux->out, tk->p_es, p_frame );
    }
}


/*****************************************************************************
 * Demux_UnSeekable: reads and demuxes data packets for unseekable file
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux_UnSeekable( demux_t *p_demux )
{
    demux_sys_t     *p_sys = p_demux->p_sys;
    avi_track_t *p_stream_master = NULL;
    unsigned int i_stream;
    unsigned int i_packet;

    es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_sys->i_time + 1 );

    /* *** find master stream for data packet skipping algo *** */
    /* *** -> first video, if any, or first audio ES *** */
    for( i_stream = 0; i_stream < p_sys->i_track; i_stream++ )
    {
        avi_track_t *tk = p_sys->track[i_stream];
        vlc_bool_t  b;

        es_out_Control( p_demux->out, ES_OUT_GET_ES_STATE, tk->p_es, &b );

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
        msg_Warn( p_demux, "no more stream selected" );
        return( 0 );
    }

    p_sys->i_time = AVI_GetPTS( p_stream_master );

    for( i_packet = 0; i_packet < 10; i_packet++)
    {
#define p_stream    p_sys->track[avi_pk.i_stream]

        avi_packet_t    avi_pk;

        if( AVI_PacketGetHeader( p_demux, &avi_pk ) )
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
                    return( !AVI_PacketNext( p_demux ) ? 1 : 0 );
                case AVIFOURCC_idx1:
                    if( p_sys->b_odml )
                    {
                        return( !AVI_PacketNext( p_demux ) ? 1 : 0 );
                    }
                    return( 0 );    /* eof */
                default:
                    msg_Warn( p_demux,
                              "seems to have lost position, resync" );
                    if( AVI_PacketSearch( p_demux ) )
                    {
                        msg_Err( p_demux, "resync failed" );
                        return( -1 );
                    }
            }
        }
        else
        {
            /* check for time */
            if( __ABS( AVI_GetPTS( p_stream ) -
                        AVI_GetPTS( p_stream_master ) )< 600*1000 )
            {
                /* load it and send to decoder */
                block_t *p_frame;
                if( AVI_PacketRead( p_demux, &avi_pk, &p_frame ) || p_frame == NULL )
                {
                    return( -1 );
                }
                p_frame->i_pts = AVI_GetPTS( p_stream ) + 1;

                if( avi_pk.i_cat != VIDEO_ES )
                    p_frame->i_dts = p_frame->i_pts;
                else
                {
                    p_frame->i_dts = p_frame->i_pts;
                    p_frame->i_pts = 0;
                }

                //p_pes->i_rate = p_demux->stream.control.i_rate;
                es_out_Send( p_demux->out, p_stream->p_es, p_frame );
            }
            else
            {
                if( AVI_PacketNext( p_demux ) )
                {
                    return( 0 );
                }
            }

            /* *** update stream time position *** */
            if( p_stream->i_samplesize )
            {
                p_stream->i_idxposb += avi_pk.i_size;
            }
            else
            {
                if( p_stream->i_cat == AUDIO_ES )
                {
                    p_stream->i_blockno += p_stream->i_blocksize > 0 ? ( avi_pk.i_size + p_stream->i_blocksize - 1 ) / p_stream->i_blocksize : 1;
                }
                p_stream->i_idxposc++;
            }

        }
#undef p_stream
    }

    return( 1 );
}

/*****************************************************************************
 * Seek: goto to i_date or i_percent
 *****************************************************************************/
static int Seek( demux_t *p_demux, mtime_t i_date, int i_percent )
{

    demux_sys_t *p_sys = p_demux->p_sys;
    unsigned int i_stream;
    msg_Dbg( p_demux, "seek requested: "I64Fd" secondes %d%%",
             i_date / 1000000, i_percent );

    if( p_sys->b_seekable )
    {
        if( !p_sys->i_length )
        {
            avi_track_t *p_stream;
            int64_t i_pos;

            /* use i_percent to create a true i_date */
            msg_Warn( p_demux, "mmh, seeking without index at %d%%"
                      " work only for interleaved file", i_percent );
            if( i_percent >= 100 )
            {
                msg_Warn( p_demux, "cannot seek so far !" );
                return VLC_EGENERIC;
            }
            i_percent = __MAX( i_percent, 0 );

            /* try to find chunk that is at i_percent or the file */
            i_pos = __MAX( i_percent * stream_Size( p_demux->s ) / 100,
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
                msg_Warn( p_demux, "cannot find any selected stream" );
                return VLC_EGENERIC;
            }

            /* be sure that the index exist */
            if( AVI_StreamChunkSet( p_demux, i_stream, 0 ) )
            {
                msg_Warn( p_demux, "cannot seek" );
                return VLC_EGENERIC;
            }

            while( i_pos >= p_stream->p_index[p_stream->i_idxposc].i_pos +
               p_stream->p_index[p_stream->i_idxposc].i_length + 8 )
            {
                /* search after i_idxposc */
                if( AVI_StreamChunkSet( p_demux,
                                        i_stream, p_stream->i_idxposc + 1 ) )
                {
                    msg_Warn( p_demux, "cannot seek" );
                    return VLC_EGENERIC;
                }
            }

            i_date = AVI_GetPTS( p_stream );
            /* TODO better support for i_samplesize != 0 */
            msg_Dbg( p_demux, "estimate date "I64Fd, i_date );
        }

#define p_stream    p_sys->track[i_stream]
        p_sys->i_time = 0;
        /* seek for chunk based streams */
        for( i_stream = 0; i_stream < p_sys->i_track; i_stream++ )
        {
            if( p_stream->b_activated && !p_stream->i_samplesize )
/*            if( p_stream->b_activated ) */
            {
                AVI_TrackSeek( p_demux, i_stream, i_date );
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
                AVI_TrackSeek( p_demux, i_stream, i_date );
/*                p_sys->i_time = __MAX( AVI_GetPTS( p_stream ), p_sys->i_time );*/
            }
        }
        msg_Dbg( p_demux, "seek: "I64Fd" seconds", p_sys->i_time /1000000 );
        /* set true movie time */
#endif
        if( !p_sys->i_time )
        {
            p_sys->i_time = i_date;
        }
#undef p_stream
        return VLC_SUCCESS;
    }
    else
    {
        msg_Err( p_demux, "shouldn't yet be executed" );
        return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * Control:
 *****************************************************************************
 *
 *****************************************************************************/
static double ControlGetPosition( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    if( p_sys->i_length > 0 )
    {
        return (double)p_sys->i_time / (double)( p_sys->i_length * (mtime_t)1000000 );
    }
    else if( stream_Size( p_demux->s ) > 0 )
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
        return (double)i64 / (double)stream_Size( p_demux->s );
    }
    return 0.0;
}

static int    Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i;
    double   f, *pf;
    int64_t i64, *pi64;
    vlc_meta_t **pp_meta;

    switch( i_query )
    {
        case DEMUX_GET_POSITION:
            pf = (double*)va_arg( args, double * );
            *pf = ControlGetPosition( p_demux );
            return VLC_SUCCESS;
        case DEMUX_SET_POSITION:
            f = (double)va_arg( args, double );
            if( p_sys->b_seekable )
            {
                i64 = (mtime_t)(1000000.0 * p_sys->i_length * f );
                return Seek( p_demux, i64, (int)(f * 100) );
            }
            else
            {
                int64_t i_pos = stream_Size( p_demux->s ) * f;
                return stream_Seek( p_demux->s, i_pos );
            }

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
                i_percent = 100 * i64 / (p_sys->i_length*1000000);
            }
            else if( p_sys->i_time > 0 )
            {
                i_percent = (int)( 100.0 * ControlGetPosition( p_demux ) *
                                   (double)i64 / (double)p_sys->i_time );
            }
            return Seek( p_demux, i64, i_percent );
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
        case DEMUX_GET_META:
            pp_meta = (vlc_meta_t**)va_arg( args, vlc_meta_t** );
            *pp_meta = vlc_meta_Duplicate( p_sys->meta );
            return VLC_SUCCESS;

        default:
            return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * Function to convert pts to chunk or byte
 *****************************************************************************/

static mtime_t AVI_PTSToChunk( avi_track_t *tk, mtime_t i_pts )
{
    if( !tk->i_scale )
        return (mtime_t)0;

    return (mtime_t)((int64_t)i_pts *
                     (int64_t)tk->i_rate /
                     (int64_t)tk->i_scale /
                     (int64_t)1000000 );
}
static mtime_t AVI_PTSToByte( avi_track_t *tk, mtime_t i_pts )
{
    if( !tk->i_scale || !tk->i_samplesize )
        return (mtime_t)0;

    return (mtime_t)((int64_t)i_pts *
                     (int64_t)tk->i_rate /
                     (int64_t)tk->i_scale /
                     (int64_t)1000000 *
                     (int64_t)tk->i_samplesize );
}

static mtime_t AVI_GetDPTS( avi_track_t *tk, int64_t i_count )
{
    mtime_t i_dpts = 0;

    if( !tk->i_rate )
        return i_dpts;

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
        if( tk->i_cat == AUDIO_ES )
        {
            return AVI_GetDPTS( tk, tk->i_blockno );
        }
        else
        {
            return AVI_GetDPTS( tk, tk->i_idxposc );
        }
    }
}

static int AVI_StreamChunkFind( demux_t *p_demux, unsigned int i_stream )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    avi_packet_t avi_pk;
    int i_loop_count = 0;

    /* find first chunk of i_stream that isn't in index */

    if( p_sys->i_movi_lastchunk_pos >= p_sys->i_movi_begin + 12 )
    {
        stream_Seek( p_demux->s, p_sys->i_movi_lastchunk_pos );
        if( AVI_PacketNext( p_demux ) )
        {
            return VLC_EGENERIC;
        }
    }
    else
    {
        stream_Seek( p_demux->s, p_sys->i_movi_begin + 12 );
    }

    for( ;; )
    {
        if( p_demux->b_die ) return VLC_EGENERIC;

        if( AVI_PacketGetHeader( p_demux, &avi_pk ) )
        {
            msg_Warn( p_demux, "cannot get packet header" );
            return VLC_EGENERIC;
        }
        if( avi_pk.i_stream >= p_sys->i_track ||
            ( avi_pk.i_cat != AUDIO_ES && avi_pk.i_cat != VIDEO_ES ) )
        {
            if( AVI_PacketNext( p_demux ) )
            {
                return VLC_EGENERIC;
            }

            /* Prevents from eating all the CPU with broken files.
             * This value should be low enough so that it doesn't
             * affect the reading speed too much. */
            if( !(++i_loop_count % 1024) )
            {
                if( p_demux->b_die ) return VLC_EGENERIC;
                msleep( 10000 );

                if( !(i_loop_count % (1024 * 10)) )
                    msg_Warn( p_demux, "don't seem to find any data..." );
            }
        }
        else
        {
            /* add this chunk to the index */
            avi_entry_t index;

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

            if( AVI_PacketNext( p_demux ) )
            {
                return VLC_EGENERIC;
            }
        }
    }
}

/* be sure that i_ck will be a valid index entry */
static int AVI_StreamChunkSet( demux_t *p_demux, unsigned int i_stream,
                               unsigned int i_ck )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    avi_track_t *p_stream = p_sys->track[i_stream];

    p_stream->i_idxposc = i_ck;
    p_stream->i_idxposb = 0;

    if(  i_ck >= p_stream->i_idxnb )
    {
        p_stream->i_idxposc = p_stream->i_idxnb - 1;
        do
        {
            p_stream->i_idxposc++;
            if( AVI_StreamChunkFind( p_demux, i_stream ) )
            {
                return VLC_EGENERIC;
            }

        } while( p_stream->i_idxposc < i_ck );
    }

    return VLC_SUCCESS;
}

/* XXX FIXME up to now, we assume that all chunk are one after one */
static int AVI_StreamBytesSet( demux_t    *p_demux,
                               unsigned int i_stream,
                               off_t   i_byte )
{
    demux_sys_t *p_sys = p_demux->p_sys;
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
            if( AVI_StreamChunkFind( p_demux, i_stream ) )
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

static int AVI_TrackSeek( demux_t *p_demux,
                           int i_stream,
                           mtime_t i_date )
{
    demux_sys_t  *p_sys = p_demux->p_sys;
    avi_track_t  *tk = p_sys->track[i_stream];

#define p_stream    p_sys->track[i_stream]
    mtime_t i_oldpts;

    i_oldpts = AVI_GetPTS( p_stream );

    if( !p_stream->i_samplesize )
    {
        if( AVI_StreamChunkSet( p_demux,
                                i_stream,
                                AVI_PTSToChunk( p_stream, i_date ) ) )
        {
            return VLC_EGENERIC;
        }

        if( p_stream->i_cat == AUDIO_ES )
        {
            unsigned int i;
            tk->i_blockno = 0;
            for( i = 0; i < tk->i_idxposc; i++ )
            {
                if( tk->i_blocksize > 0 )
                {
                    tk->i_blockno += ( tk->p_index[i].i_length + tk->i_blocksize - 1 ) / tk->i_blocksize;
                }
                else
                {
                    tk->i_blockno++;
                }
            }
        }

        msg_Dbg( p_demux,
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
                    if( AVI_StreamChunkSet( p_demux,
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
                    if( AVI_StreamChunkSet( p_demux,
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
        if( AVI_StreamBytesSet( p_demux,
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
            return p_byte[4] & 0x06 ? 0 : AVIIF_KEYFRAME;

        case FOURCC_DIV2:
        case FOURCC_DIV3:   /* wmv1 also */
            /* we have
             *  picture type    0(I),1(P)     2bits
             */
            return p_byte[0] & 0xC0 ? 0 : AVIIF_KEYFRAME;
        case FOURCC_mp4v:
            /* we should find first occurrence of 0x000001b6 (32bits)
             *  startcode:      0x000001b6   32bits
             *  piture type     0(I),1(P)     2bits
             */
            if( GetDWBE( p_byte ) != 0x000001b6 )
            {
                /* not true , need to find the first VOP header */
                return AVIIF_KEYFRAME;
            }
            return p_byte[4] & 0xC0 ? 0 : AVIIF_KEYFRAME;

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
                case FOURCC_1:
                    return VLC_FOURCC('m','r','l','e');
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
                case FOURCC_dx50:
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
static int AVI_PacketGetHeader( demux_t *p_demux, avi_packet_t *p_pk )
{
    uint8_t  *p_peek;

    if( stream_Peek( p_demux->s, &p_peek, 16 ) < 16 )
    {
        return VLC_EGENERIC;
    }
    p_pk->i_fourcc  = VLC_FOURCC( p_peek[0], p_peek[1], p_peek[2], p_peek[3] );
    p_pk->i_size    = GetDWLE( p_peek + 4 );
    p_pk->i_pos     = stream_Tell( p_demux->s );
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

static int AVI_PacketNext( demux_t *p_demux )
{
    avi_packet_t    avi_ck;
    int             i_skip = 0;

    if( AVI_PacketGetHeader( p_demux, &avi_ck ) )
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

    if( stream_Read( p_demux->s, NULL, i_skip ) != i_skip )
    {
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int AVI_PacketRead( demux_t   *p_demux,
                           avi_packet_t     *p_pk,
                           block_t          **pp_frame )
{
    size_t i_size;

    i_size = __EVEN( p_pk->i_size + 8 );

    if( ( *pp_frame = stream_Block( p_demux->s, i_size ) ) == NULL )
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

static int AVI_PacketSearch( demux_t *p_demux )
{
    demux_sys_t     *p_sys = p_demux->p_sys;
    avi_packet_t    avi_pk;
    int             i_count = 0;

    for( ;; )
    {
        if( stream_Read( p_demux->s, NULL, 1 ) != 1 )
        {
            return VLC_EGENERIC;
        }
        AVI_PacketGetHeader( p_demux, &avi_pk );
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
            if( p_demux->b_die ) return VLC_EGENERIC;

            msleep( 10000 );
            if( !(i_count % (1024 * 10)) )
                msg_Warn( p_demux, "trying to resync..." );
        }
    }
}

/****************************************************************************
 * Index stuff.
 ****************************************************************************/
static void AVI_IndexAddEntry( demux_sys_t *p_sys,
                               int i_stream,
                               avi_entry_t *p_index)
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
                               tk->i_idxmax * sizeof( avi_entry_t ) );
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

static int AVI_IndexLoad_idx1( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    avi_chunk_list_t    *p_riff;
    avi_chunk_list_t    *p_movi;
    avi_chunk_idx1_t    *p_idx1;

    unsigned int i_stream;
    unsigned int i_index;
    off_t        i_offset;
    unsigned int i;

    vlc_bool_t b_keyset[100];

    p_riff = AVI_ChunkFind( &p_sys->ck_root, AVIFOURCC_RIFF, 0);
    p_idx1 = AVI_ChunkFind( p_riff, AVIFOURCC_idx1, 0);
    p_movi = AVI_ChunkFind( p_riff, AVIFOURCC_movi, 0);

    if( !p_idx1 )
    {
        msg_Warn( p_demux, "cannot find idx1 chunk, no index defined" );
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

    /* Reset b_keyset */
    for( i_stream = 0; i_stream < p_sys->i_track; i_stream++ )
        b_keyset[i_stream] = VLC_FALSE;

    for( i_index = 0; i_index < p_idx1->i_entry_count; i_index++ )
    {
        unsigned int i_cat;

        AVI_ParseStreamHeader( p_idx1->entry[i_index].i_fourcc,
                               &i_stream,
                               &i_cat );
        if( i_stream < p_sys->i_track &&
            i_cat == p_sys->track[i_stream]->i_cat )
        {
            avi_entry_t index;
            index.i_id      = p_idx1->entry[i_index].i_fourcc;
            index.i_flags   =
                p_idx1->entry[i_index].i_flags&(~AVIIF_FIXKEYFRAME);
            index.i_pos     = p_idx1->entry[i_index].i_pos + i_offset;
            index.i_length  = p_idx1->entry[i_index].i_length;
            AVI_IndexAddEntry( p_sys, i_stream, &index );

            if( index.i_flags&AVIIF_KEYFRAME )
                b_keyset[i_stream] = VLC_TRUE;
        }
    }

    for( i_stream = 0; i_stream < p_sys->i_track; i_stream++ )
    {
        if( !b_keyset[i_stream] )
        {
            avi_track_t *tk = p_sys->track[i_stream];

            msg_Dbg( p_demux, "no key frame set for track %d", i_stream );
            for( i_index = 0; i_index < tk->i_idxnb; i_index++ )
                tk->p_index[i_index].i_flags |= AVIIF_KEYFRAME;
        }
    }
    return VLC_SUCCESS;
}

static void __Parse_indx( demux_t    *p_demux,
                          int               i_stream,
                          avi_chunk_indx_t  *p_indx )
{
    demux_sys_t         *p_sys    = p_demux->p_sys;
    avi_entry_t     index;
    int32_t             i;

    msg_Dbg( p_demux, "loading subindex(0x%x) %d entries", p_indx->i_indextype, p_indx->i_entriesinuse );
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
        msg_Warn( p_demux, "unknown subtype index(0x%x)", p_indx->i_indexsubtype );
    }
}

static void AVI_IndexLoad_indx( demux_t *p_demux )
{
    demux_sys_t         *p_sys = p_demux->p_sys;
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
            msg_Warn( p_demux, "cannot find indx (misdetect/broken OpenDML file?)" );
            continue;
        }

        if( p_indx->i_indextype == AVI_INDEX_OF_CHUNKS )
        {
            __Parse_indx( p_demux, i_stream, p_indx );
        }
        else if( p_indx->i_indextype == AVI_INDEX_OF_INDEXES )
        {
            avi_chunk_t    ck_sub;
            for( i = 0; i < p_indx->i_entriesinuse; i++ )
            {
                if( stream_Seek( p_demux->s, p_indx->idx.super[i].i_offset )||
                    AVI_ChunkRead( p_demux->s, &ck_sub, NULL  ) )
                {
                    break;
                }
                __Parse_indx( p_demux, i_stream, &ck_sub.indx );
            }
        }
        else
        {
            msg_Warn( p_demux, "unknown type index(0x%x)", p_indx->i_indextype );
        }
#undef p_stream
    }
}

static void AVI_IndexLoad( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    unsigned int i_stream;

    for( i_stream = 0; i_stream < p_sys->i_track; i_stream++ )
    {
        p_sys->track[i_stream]->i_idxnb  = 0;
        p_sys->track[i_stream]->i_idxmax = 0;
        p_sys->track[i_stream]->p_index  = NULL;
    }

    if( p_sys->b_odml )
    {
        AVI_IndexLoad_indx( p_demux );
    }
    else  if( AVI_IndexLoad_idx1( p_demux ) )
    {
        /* try indx if idx1 failed as some "normal" file have indx too */
        AVI_IndexLoad_indx( p_demux );
    }

    for( i_stream = 0; i_stream < p_sys->i_track; i_stream++ )
    {
        msg_Dbg( p_demux, "stream[%d] created %d index entries",
                i_stream, p_sys->track[i_stream]->i_idxnb );
    }
}

static void AVI_IndexCreate( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    avi_chunk_list_t    *p_riff;
    avi_chunk_list_t    *p_movi;

    unsigned int i_stream;
    off_t i_movi_end;

    p_riff = AVI_ChunkFind( &p_sys->ck_root, AVIFOURCC_RIFF, 0);
    p_movi = AVI_ChunkFind( p_riff, AVIFOURCC_movi, 0);

    if( !p_movi )
    {
        msg_Err( p_demux, "cannot find p_movi" );
        return;
    }

    for( i_stream = 0; i_stream < p_sys->i_track; i_stream++ )
    {
        p_sys->track[i_stream]->i_idxnb  = 0;
        p_sys->track[i_stream]->i_idxmax = 0;
        p_sys->track[i_stream]->p_index  = NULL;
    }
    i_movi_end = __MIN( (off_t)(p_movi->i_chunk_pos + p_movi->i_chunk_size),
                        stream_Size( p_demux->s ) );

    stream_Seek( p_demux->s, p_movi->i_chunk_pos + 12 );
    msg_Warn( p_demux, "creating index from LIST-movi, will take time !" );
    for( ;; )
    {
        avi_packet_t pk;

        if( p_demux->b_die )
        {
            return;
        }

        if( AVI_PacketGetHeader( p_demux, &pk ) )
        {
            break;
        }
        if( pk.i_stream < p_sys->i_track &&
            pk.i_cat == p_sys->track[pk.i_stream]->i_cat )
        {
            avi_entry_t index;
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

                        msg_Dbg( p_demux, "looking for new RIFF chunk" );
                        if( stream_Seek( p_demux->s, p_sysx->i_chunk_pos + 24))
                        {
                            goto print_stat;
                        }
                        break;
                    }
                    goto print_stat;
                case AVIFOURCC_RIFF:
                        msg_Dbg( p_demux, "new RIFF chunk found" );
                case AVIFOURCC_rec:
                case AVIFOURCC_JUNK:
                    break;
                default:
                    msg_Warn( p_demux, "need resync, probably broken avi" );
                    if( AVI_PacketSearch( p_demux ) )
                    {
                        msg_Warn( p_demux, "lost sync, abord index creation" );
                        goto print_stat;
                    }
            }
        }

        if( ( !p_sys->b_odml && pk.i_pos + pk.i_size >= i_movi_end ) ||
            AVI_PacketNext( p_demux ) )
        {
            break;
        }
    }

print_stat:
    for( i_stream = 0; i_stream < p_sys->i_track; i_stream++ )
    {
        msg_Dbg( p_demux,
                "stream[%d] creating %d index entries",
                i_stream,
                p_sys->track[i_stream]->i_idxnb );
    }
}

/*****************************************************************************
 * Stream management
 *****************************************************************************/
static int AVI_TrackStopFinishedStreams( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    unsigned int i;
    int b_end = VLC_TRUE;

    for( i = 0; i < p_sys->i_track; i++ )
    {
        avi_track_t *tk = p_sys->track[i];
        if( tk->i_idxposc >= tk->i_idxnb )
        {
            tk->b_activated = VLC_FALSE;
            es_out_Control( p_demux->out, ES_OUT_SET_ES_STATE, tk->p_es, VLC_FALSE );
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
static mtime_t  AVI_MovieGetLength( demux_t *p_demux )
{
    demux_sys_t  *p_sys = p_demux->p_sys;
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

        msg_Dbg( p_demux,
                 "stream[%d] length:"I64Fd" (based on index)",
                 i,
                 i_length );
        i_maxlength = __MAX( i_maxlength, i_length );
    }

    return i_maxlength;
}
