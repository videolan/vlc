/*****************************************************************************
 * avi.c : AVI file Stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2009 VLC authors and VideoLAN
 * $Id$
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
#include <assert.h>
#include <ctype.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_input.h>

#include <vlc_dialog.h>

#include <vlc_meta.h>
#include <vlc_codecs.h>
#include <vlc_charset.h>
#include <vlc_memory.h>

#include "libavi.h"
#include "../rawdv.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

#define INTERLEAVE_TEXT N_("Force interleaved method" )

#define INDEX_TEXT N_("Force index creation")
#define INDEX_LONGTEXT N_( \
    "Recreate a index for the AVI file. Use this if your AVI file is damaged "\
    "or incomplete (not seekable)." )

static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

static const int pi_index[] = {0,1,2,3};

static const char *const ppsz_indexes[] = { N_("Ask for action"),
                                            N_("Always fix"),
                                            N_("Never fix"),
                                            N_("Fix when necessary")};

vlc_module_begin ()
    set_shortname( "AVI" )
    set_description( N_("AVI demuxer") )
    set_capability( "demux", 212 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )

    add_bool( "avi-interleaved", false,
              INTERLEAVE_TEXT, INTERLEAVE_TEXT, true )
    add_integer( "avi-index", 0,
              INDEX_TEXT, INDEX_LONGTEXT, false )
        change_integer_list( pi_index, ppsz_indexes )

    set_callbacks( Open, Close )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Control         ( demux_t *, int, va_list );
static int Seek            ( demux_t *, mtime_t, int );
static int Demux_Seekable  ( demux_t * );
static int Demux_UnSeekable( demux_t * );

static char *FromACP( const char *str )
{
    return FromCharset(vlc_pgettext("GetACP", "CP1252"), str, strlen(str));
}

#define IGNORE_ES NAV_ES

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
    int64_t      i_lengthtotal;

} avi_entry_t;

typedef struct
{
    unsigned int    i_size;
    unsigned int    i_max;
    avi_entry_t     *p_entry;

} avi_index_t;
static void avi_index_Init( avi_index_t * );
static void avi_index_Clean( avi_index_t * );
static void avi_index_Append( avi_index_t *, off_t *, avi_entry_t * );

typedef struct
{
    bool            b_activated;
    bool            b_eof;

    unsigned int    i_cat; /* AUDIO_ES, VIDEO_ES */
    vlc_fourcc_t    i_codec;

    int             i_rate;
    int             i_scale;
    unsigned int    i_samplesize;

    es_out_id_t     *p_es;

    int             i_dv_audio_rate;
    es_out_id_t     *p_es_dv_audio;

    /* Avi Index */
    avi_index_t     idx;

    unsigned int    i_idxposc;  /* numero of chunk */
    unsigned int    i_idxposb;  /* byte in the current chunk */

    /* For VBR audio only */
    unsigned int    i_blockno;
    unsigned int    i_blocksize;

} avi_track_t;

struct demux_sys_t
{
    mtime_t i_time;
    mtime_t i_length;

    bool  b_seekable;
    avi_chunk_t ck_root;

    bool  b_odml;

    off_t   i_movi_begin;
    off_t   i_movi_lastchunk_pos;   /* XXX position of last valid chunk */

    /* number of streams and information */
    unsigned int i_track;
    avi_track_t  **track;

    /* meta */
    vlc_meta_t  *meta;

    unsigned int       i_attachment;
    input_attachment_t **attachment;
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

static void AVI_ExtractSubtitle( demux_t *, unsigned int i_stream, avi_chunk_list_t *, avi_chunk_STRING_t * );

static void AVI_DvHandleAudio( demux_t *, avi_track_t *, block_t * );

static mtime_t  AVI_MovieGetLength( demux_t * );

static void AVI_MetaLoad( demux_t *, avi_chunk_list_t *p_riff, avi_chunk_avih_t *p_avih );

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

    bool       b_index = false, b_aborted = false;
    int              i_do_index;

    avi_chunk_list_t    *p_riff;
    avi_chunk_list_t    *p_hdrl, *p_movi;
    avi_chunk_avih_t    *p_avih;

    unsigned int i_track;
    unsigned int i, i_peeker;

    const uint8_t *p_peek;

    /* Is it an avi file ? */
    if( stream_Peek( p_demux->s, &p_peek, 200 ) < 200 ) return VLC_EGENERIC;

    for( i_peeker = 0; i_peeker < 188; i_peeker++ )
    {
        if( !strncmp( (char *)&p_peek[0], "RIFF", 4 ) && !strncmp( (char *)&p_peek[8], "AVI ", 4 ) )
            break;
        if( !strncmp( (char *)&p_peek[0], "ON2 ", 4 ) && !strncmp( (char *)&p_peek[8], "ON2f", 4 ) )
            break;
        p_peek++;
    }
    if( i_peeker == 188 )
    {
        return VLC_EGENERIC;
    }

    /* Initialize input structures. */
    p_sys = p_demux->p_sys = calloc( 1, sizeof(demux_sys_t) );
    p_sys->b_odml   = false;
    p_sys->track    = NULL;
    p_sys->meta     = NULL;
    TAB_INIT(p_sys->i_attachment, p_sys->attachment);

    stream_Control( p_demux->s, STREAM_CAN_FASTSEEK, &p_sys->b_seekable );

    p_demux->pf_control = Control;
    p_demux->pf_demux = Demux_Seekable;

    /* For unseekable stream, automatically use Demux_UnSeekable */
    if( !p_sys->b_seekable
     || var_InheritBool( p_demux, "avi-interleaved" ) )
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
        free(p_sys);
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
                p_sys->b_odml = true;
                break;
            }
        }
    }

    p_riff  = AVI_ChunkFind( &p_sys->ck_root, AVIFOURCC_RIFF, 0 );
    p_hdrl  = AVI_ChunkFind( p_riff, AVIFOURCC_hdrl, 0 );
    p_movi  = AVI_ChunkFind( p_riff, AVIFOURCC_movi, 0 );
    if( !p_movi )
        p_movi  = AVI_ChunkFind( &p_sys->ck_root, AVIFOURCC_movi, 0 );

    if( !p_hdrl || !p_movi )
    {
        msg_Err( p_demux, "invalid file: cannot find hdrl or movi chunks" );
        goto error;
    }

    if( !( p_avih = AVI_ChunkFind( p_hdrl, AVIFOURCC_avih, 0 ) ) )
    {
        msg_Err( p_demux, "invalid file: cannot find avih chunk" );
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

    AVI_MetaLoad( p_demux, p_riff, p_avih );

    /* now read info on each stream and create ES */
    for( i = 0 ; i < i_track; i++ )
    {
        avi_track_t           *tk     = calloc( 1, sizeof( avi_track_t ) );
        if( unlikely( !tk ) )
            goto error;

        avi_chunk_list_t      *p_strl = AVI_ChunkFind( p_hdrl, AVIFOURCC_strl, i );
        avi_chunk_strh_t      *p_strh = AVI_ChunkFind( p_strl, AVIFOURCC_strh, 0 );
        avi_chunk_STRING_t    *p_strn = AVI_ChunkFind( p_strl, AVIFOURCC_strn, 0 );
        avi_chunk_strf_auds_t *p_auds = NULL;
        avi_chunk_strf_vids_t *p_vids = NULL;
        es_format_t fmt;

        tk->b_eof = false;
        tk->b_activated = true;

        p_vids = (avi_chunk_strf_vids_t*)AVI_ChunkFind( p_strl, AVIFOURCC_strf, 0 );
        p_auds = (avi_chunk_strf_auds_t*)AVI_ChunkFind( p_strl, AVIFOURCC_strf, 0 );

        if( p_strl == NULL || p_strh == NULL || p_auds == NULL || p_vids == NULL )
        {
            msg_Warn( p_demux, "stream[%d] incomplete", i );
            free( tk );
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
                if( p_auds->p_wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                    p_auds->p_wf->cbSize >= sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX) )
                {
                    WAVEFORMATEXTENSIBLE *p_wfe = (WAVEFORMATEXTENSIBLE *)p_auds->p_wf;
                    tk->i_codec = AVI_FourccGetCodec( AUDIO_ES,
                                                      p_wfe->SubFormat.Data1 );
                }
                else
                    tk->i_codec = AVI_FourccGetCodec( AUDIO_ES,
                                                      p_auds->p_wf->wFormatTag );

                tk->i_blocksize = p_auds->p_wf->nBlockAlign;
                if( tk->i_blocksize == 0 )
                {
                    if( p_auds->p_wf->wFormatTag == 1 )
                        tk->i_blocksize = p_auds->p_wf->nChannels * (p_auds->p_wf->wBitsPerSample/8);
                    else
                        tk->i_blocksize = 1;
                }
                else if( tk->i_samplesize != 0 && tk->i_samplesize != tk->i_blocksize )
                {
                    msg_Warn( p_demux, "track[%d] samplesize=%d and blocksize=%d are not equal."
                                       "Using blocksize as a workaround.",
                                       i, tk->i_samplesize, tk->i_blocksize );
                    tk->i_samplesize = tk->i_blocksize;
                }

                if( tk->i_codec == VLC_CODEC_VORBIS )
                {
                    tk->i_blocksize = 0; /* fix vorbis VBR decoding */
                }

                es_format_Init( &fmt, AUDIO_ES, tk->i_codec );

                fmt.audio.i_channels        = p_auds->p_wf->nChannels;
                fmt.audio.i_rate            = p_auds->p_wf->nSamplesPerSec;
                fmt.i_bitrate               = p_auds->p_wf->nAvgBytesPerSec*8;
                fmt.audio.i_blockalign      = p_auds->p_wf->nBlockAlign;
                fmt.audio.i_bitspersample   = p_auds->p_wf->wBitsPerSample;
                fmt.b_packetized            = !tk->i_blocksize;

                msg_Dbg( p_demux,
                    "stream[%d] audio(0x%x - %s) %d channels %dHz %dbits",
                    i, p_auds->p_wf->wFormatTag,vlc_fourcc_GetDescription(AUDIO_ES,tk->i_codec),
                    p_auds->p_wf->nChannels,
                    p_auds->p_wf->nSamplesPerSec,
                    p_auds->p_wf->wBitsPerSample );

                fmt.i_extra = __MIN( p_auds->p_wf->cbSize,
                    p_auds->i_chunk_size - sizeof(WAVEFORMATEX) );
                if( fmt.i_extra > 0 )
                {
                    fmt.p_extra = malloc( fmt.i_extra );
                    if( !fmt.p_extra ) goto error;
                    memcpy( fmt.p_extra, &p_auds->p_wf[1], fmt.i_extra );
                }
                break;

            case( AVIFOURCC_vids ):
            {
                tk->i_cat   = VIDEO_ES;
                tk->i_codec = AVI_FourccGetCodec( VIDEO_ES,
                                                  p_vids->p_bih->biCompression );
                if( p_vids->p_bih->biCompression == VLC_FOURCC( 'D', 'X', 'S', 'B' ) )
                {
                   msg_Dbg( p_demux, "stream[%d] subtitles", i );
                   es_format_Init( &fmt, SPU_ES, p_vids->p_bih->biCompression );
                   tk->i_cat = SPU_ES;
                   break;
                }
                else if( p_vids->p_bih->biCompression == 0x00 )
                {
                    switch( p_vids->p_bih->biBitCount )
                    {
                        case 32:
                            tk->i_codec = VLC_CODEC_RGB32;
                            break;
                        case 24:
                            tk->i_codec = VLC_CODEC_RGB24;
                            break;
                        case 16: /* Yes it is RV15 */
                        case 15:
                            tk->i_codec = VLC_CODEC_RGB15;
                            break;
                        case 9: /* <- TODO check that */
                            tk->i_codec = VLC_CODEC_I410;
                            break;
                        case 8: /* <- TODO check that */
                            tk->i_codec = VLC_CODEC_GREY;
                            break;
                    }
                    es_format_Init( &fmt, VIDEO_ES, tk->i_codec );

                    switch( tk->i_codec )
                    {
                    case VLC_CODEC_RGB24:
                    case VLC_CODEC_RGB32:
                        fmt.video.i_rmask = 0x00ff0000;
                        fmt.video.i_gmask = 0x0000ff00;
                        fmt.video.i_bmask = 0x000000ff;
                        break;
                    case VLC_CODEC_RGB15:
                        fmt.video.i_rmask = 0x7c00;
                        fmt.video.i_gmask = 0x03e0;
                        fmt.video.i_bmask = 0x001f;
                        break;
                    default:
                        break;
                    }
                }
                else
                {
                    es_format_Init( &fmt, VIDEO_ES, p_vids->p_bih->biCompression );
                    if( tk->i_codec == VLC_CODEC_MP4V &&
                        !strncasecmp( (char*)&p_strh->i_handler, "XVID", 4 ) )
                    {
                        fmt.i_codec           =
                        fmt.i_original_fourcc = VLC_FOURCC( 'X', 'V', 'I', 'D' );
                    }
                }
                tk->i_samplesize = 0;
                fmt.video.i_width  = p_vids->p_bih->biWidth;
                fmt.video.i_height = p_vids->p_bih->biHeight;
                fmt.video.i_bits_per_pixel = p_vids->p_bih->biBitCount;
                fmt.video.i_frame_rate = tk->i_rate;
                fmt.video.i_frame_rate_base = tk->i_scale;
                avi_chunk_vprp_t *p_vprp = AVI_ChunkFind( p_strl, AVIFOURCC_vprp, 0 );
                if( p_vprp )
                {
                    uint32_t i_frame_aspect_ratio = p_vprp->i_frame_aspect_ratio;
                    if( p_vprp->i_video_format_token >= 1 &&
                        p_vprp->i_video_format_token <= 4 )
                        i_frame_aspect_ratio = 0x00040003;
                    fmt.video.i_sar_num = ((i_frame_aspect_ratio >> 16) & 0xffff) * fmt.video.i_height;
                    fmt.video.i_sar_den = ((i_frame_aspect_ratio >>  0) & 0xffff) * fmt.video.i_width;
                }
                /* Extradata is the remainder of the chunk less the BIH */
                fmt.i_extra = p_vids->i_chunk_size - sizeof(VLC_BITMAPINFOHEADER);
                if( fmt.i_extra > 0 )
                {
                    fmt.p_extra = malloc( fmt.i_extra );
                    if( !fmt.p_extra ) goto error;
                    memcpy( fmt.p_extra, &p_vids->p_bih[1], fmt.i_extra );
                }

                msg_Dbg( p_demux, "stream[%d] video(%4.4s) %"PRIu32"x%"PRIu32" %dbpp %ffps",
                         i, (char*)&p_vids->p_bih->biCompression,
                         (uint32_t)p_vids->p_bih->biWidth,
                         (uint32_t)p_vids->p_bih->biHeight,
                         p_vids->p_bih->biBitCount,
                         (float)tk->i_rate/(float)tk->i_scale );

                if( p_vids->p_bih->biCompression == 0x00 )
                {
                    /* RGB DIB are coded from bottom to top */
                    fmt.video.i_height =
                        (unsigned int)(-(int)p_vids->p_bih->biHeight);
                }

                /* Extract palette from extradata if bpp <= 8 */
                if( fmt.video.i_bits_per_pixel > 0 && fmt.video.i_bits_per_pixel <= 8 )
                {
                    /* The palette should not be included in biSize, but come
                     * directly after BITMAPINFORHEADER in the BITMAPINFO structure */
                    if( fmt.i_extra > 0 && fmt.p_extra )
                    {
                        const uint8_t *p_pal = fmt.p_extra;

                        fmt.video.p_palette = calloc( 1, sizeof(video_palette_t) );
                        fmt.video.p_palette->i_entries = __MIN(fmt.i_extra/4, 256);

                        for( int k = 0; k < fmt.video.p_palette->i_entries; k++ )
                        {
                            for( int j = 0; j < 4; j++ )
                                fmt.video.p_palette->palette[k][j] = p_pal[4*k+j];
                        }
                    }
                }
                break;
            }

            case( AVIFOURCC_txts):
                msg_Dbg( p_demux, "stream[%d] subtitle attachment", i );
                AVI_ExtractSubtitle( p_demux, i, p_strl, p_strn );
                free( tk );
                continue;

            case( AVIFOURCC_iavs):
            case( AVIFOURCC_ivas):
                msg_Dbg( p_demux, "stream[%d] iavs with handler %4.4s", i, (char *)&p_strh->i_handler );
                tk->i_cat   = VIDEO_ES;
                tk->i_codec = AVI_FourccGetCodec( VIDEO_ES, p_strh->i_handler );
                tk->i_samplesize = 0;
                tk->i_dv_audio_rate = tk->i_codec == VLC_CODEC_DV ? -1 : 0;

                es_format_Init( &fmt, VIDEO_ES, p_strh->i_handler );
                fmt.video.i_width  = p_avih->i_width;
                fmt.video.i_height = p_avih->i_height;
                break;

            case( AVIFOURCC_mids):
                msg_Dbg( p_demux, "stream[%d] midi is UNSUPPORTED", i );

            default:
                msg_Warn( p_demux, "stream[%d] unknown type %4.4s", i, (char *)&p_strh->i_type );
                free( tk );
                continue;
        }
        if( p_strn )
            fmt.psz_description = FromACP( p_strn->p_str );
        tk->p_es = es_out_Add( p_demux->out, &fmt );
        TAB_APPEND( p_sys->i_track, p_sys->track, tk );
        es_format_Clean( &fmt );
    }

    if( p_sys->i_track <= 0 )
    {
        msg_Err( p_demux, "no valid track" );
        goto error;
    }

    i_do_index = var_InheritInteger( p_demux, "avi-index" );
    if( i_do_index == 1 ) /* Always fix */
    {
aviindex:
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

    /* Check the index completeness */
    unsigned int i_idx_totalframes = 0;
    for( unsigned int i = 0; i < p_sys->i_track; i++ )
    {
        const avi_track_t *tk = p_sys->track[i];
        if( tk->i_cat == VIDEO_ES && tk->idx.p_entry )
            i_idx_totalframes = __MAX(i_idx_totalframes, tk->idx.i_size);
    }
    if( i_idx_totalframes != p_avih->i_totalframes &&
        p_sys->i_length < (mtime_t)p_avih->i_totalframes *
                          (mtime_t)p_avih->i_microsecperframe /
                          (mtime_t)1000000 )
    {
        if( !vlc_object_alive( p_demux) )
            goto error;

        msg_Warn( p_demux, "broken or missing index, 'seek' will be "
                           "approximative or will exhibit strange behavior" );
        if( (i_do_index == 0 || i_do_index == 3) && !b_index )
        {
            if( !p_sys->b_seekable ) {
                b_index = true;
                goto aviindex;
            }
            if( i_do_index == 0 )
            {
                switch( dialog_Question( p_demux, _("Broken or missing AVI Index") ,
                   _( "Because this AVI file index is broken or missing, "
                      "seeking will not work correctly.\n"
                      "VLC won't repair your file but can temporary fix this "
                      "problem by building an index in memory.\n"
                      "This step might take a long time on a large file.\n"
                      "What do you want to do?" ),
                      _( "Build index then play" ), _( "Play as is" ), _( "Do not play") ) )
                {
                    case 1:
                        b_index = true;
                        msg_Dbg( p_demux, "Fixing AVI index" );
                        goto aviindex;
                    case 3:
                        b_aborted = true;
                        goto error;
                }
            }
            else
            {
                b_index = true;
                msg_Dbg( p_demux, "Fixing AVI index" );
                goto aviindex;
            }
        }
    }

    /* fix some BeOS MediaKit generated file */
    for( i = 0 ; i < p_sys->i_track; i++ )
    {
        avi_track_t         *tk = p_sys->track[i];
        avi_chunk_list_t    *p_strl;
        avi_chunk_strf_auds_t    *p_auds;

        if( tk->i_cat != AUDIO_ES )
        {
            continue;
        }
        if( tk->idx.i_size < 1 ||
            tk->i_scale != 1 ||
            tk->i_samplesize != 0 )
        {
            continue;
        }
        p_strl = AVI_ChunkFind( p_hdrl, AVIFOURCC_strl, i );
        p_auds = AVI_ChunkFind( p_strl, AVIFOURCC_strf, 0 );

        if( p_auds->p_wf->wFormatTag != WAVE_FORMAT_PCM &&
            (unsigned int)tk->i_rate == p_auds->p_wf->nSamplesPerSec )
        {
            int64_t i_track_length =
                tk->idx.p_entry[tk->idx.i_size-1].i_length +
                tk->idx.p_entry[tk->idx.i_size-1].i_lengthtotal;
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
    for( unsigned i = 0; i < p_sys->i_attachment; i++)
        vlc_input_attachment_Delete(p_sys->attachment[i]);
    free(p_sys->attachment);

    if( p_sys->meta )
        vlc_meta_Delete( p_sys->meta );

    AVI_ChunkFreeRoot( p_demux->s, &p_sys->ck_root );
    free( p_sys );
    return b_aborted ? VLC_ETIMEOUT : VLC_EGENERIC;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close ( vlc_object_t * p_this )
{
    demux_t *    p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys = p_demux->p_sys  ;

    for( unsigned int i = 0; i < p_sys->i_track; i++ )
    {
        if( p_sys->track[i] )
        {
            avi_index_Clean( &p_sys->track[i]->idx );
            free( p_sys->track[i] );
        }
    }
    free( p_sys->track );

    AVI_ChunkFreeRoot( p_demux->s, &p_sys->ck_root );
    vlc_meta_Delete( p_sys->meta );

    for( unsigned i = 0; i < p_sys->i_attachment; i++)
        vlc_input_attachment_Delete(p_sys->attachment[i]);
    free(p_sys->attachment);

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
    bool b_ok;

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
    /* cannot be more than 100 stream (dcXX or wbXX) */
    avi_track_toread_t toread[100];


    /* detect new selected/unselected streams */
    for( i_track = 0; i_track < p_sys->i_track; i_track++ )
    {
        avi_track_t *tk = p_sys->track[i_track];
        bool  b;

        es_out_Control( p_demux->out, ES_OUT_GET_ES_STATE, tk->p_es, &b );
        if( tk->p_es_dv_audio )
        {
            bool b_extra;
            es_out_Control( p_demux->out, ES_OUT_GET_ES_STATE, tk->p_es_dv_audio, &b_extra );
            b |= b_extra;
        }
        if( b && !tk->b_activated )
        {
            if( p_sys->b_seekable)
            {
                AVI_TrackSeek( p_demux, i_track, p_sys->i_time );
            }
            tk->b_activated = true;
        }
        else if( !b && tk->b_activated )
        {
            tk->b_activated = false;
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

        toread[i_track].b_ok = tk->b_activated && !tk->b_eof;
        if( tk->i_idxposc < tk->idx.i_size )
        {
            toread[i_track].i_posf = tk->idx.p_entry[tk->i_idxposc].i_pos;
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
            toread[i_track].i_toread = AVI_PTSToByte( tk, llabs( i_dpts ) );
        }
        else
        {
            toread[i_track].i_toread = AVI_PTSToChunk( tk, llabs( i_dpts ) );
        }

        if( i_dpts < 0 )
        {
            toread[i_track].i_toread *= -1;
        }
    }

    for( ;; )
    {
        avi_track_t     *tk;
        bool       b_done;
        block_t         *p_frame;
        off_t i_pos;
        unsigned int i;
        size_t i_size;

        /* search for first chunk to be read */
        for( i = 0, b_done = true, i_pos = -1; i < p_sys->i_track; i++ )
        {
            if( !toread[i].b_ok ||
                AVI_GetDPTS( p_sys->track[i],
                             toread[i].i_toread ) <= -25 * 1000 )
            {
                continue;
            }

            if( toread[i].i_toread > 0 )
            {
                b_done = false; /* not yet finished */
            }
            if( toread[i].i_posf > 0 )
            {
                if( i_pos == -1 || i_pos > toread[i].i_posf )
                {
                    i_track = i;
                    i_pos = toread[i].i_posf;
                }
            }
        }

        if( b_done )
        {
            for( i = 0; i < p_sys->i_track; i++ )
            {
                if( toread[i].b_ok )
                    return 1;
            }
            msg_Warn( p_demux, "all tracks have failed, exiting..." );
            return 0;
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
                        if( !vlc_object_alive (p_demux) ) return -1;
                        msleep( 10000 );

                        if( !(i_loop_count % (1024 * 10)) )
                            msg_Warn( p_demux,
                                      "don't seem to find any data..." );
                    }
                    continue;
                }
                else
                {
                    i_track = avi_pk.i_stream;
                    tk = p_sys->track[i_track];

                    /* add this chunk to the index */
                    avi_entry_t index;
                    index.i_id     = avi_pk.i_fourcc;
                    index.i_flags  = AVI_GetKeyFlag(tk->i_codec, avi_pk.i_peek);
                    index.i_pos    = avi_pk.i_pos;
                    index.i_length = avi_pk.i_size;
                    avi_index_Append( &tk->idx, &p_sys->i_movi_lastchunk_pos, &index );

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
            i_size = __MIN( tk->idx.p_entry[tk->i_idxposc].i_length -
                                tk->i_idxposb,
                            i_toread );
        }
        else
        {
            i_size = tk->idx.p_entry[tk->i_idxposc].i_length;
        }

        if( tk->i_idxposb == 0 )
        {
            i_size += 8; /* need to read and skip header */
        }

        if( ( p_frame = stream_Block( p_demux->s, __EVEN( i_size ) ) )==NULL )
        {
            msg_Warn( p_demux, "failed reading data" );
            tk->b_eof = false;
            toread[i_track].b_ok = false;
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
        if( tk->idx.p_entry[tk->i_idxposc].i_flags&AVIIF_KEYFRAME )
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
                    tk->idx.p_entry[tk->i_idxposc].i_length )
            {
                tk->i_idxposb = 0;
                tk->i_idxposc++;
            }
        }
        else
        {
            int i_length = tk->idx.p_entry[tk->i_idxposc].i_length;

            tk->i_idxposc++;
            if( tk->i_cat == AUDIO_ES )
            {
                tk->i_blockno += tk->i_blocksize > 0 ? ( i_length + tk->i_blocksize - 1 ) / tk->i_blocksize : 1;
            }
            toread[i_track].i_toread--;
        }

        if( tk->i_idxposc < tk->idx.i_size)
        {
            toread[i_track].i_posf =
                tk->idx.p_entry[tk->i_idxposc].i_pos;
            if( tk->i_idxposb > 0 )
            {
                toread[i_track].i_posf += 8 + tk->i_idxposb;
            }

        }
        else
        {
            toread[i_track].i_posf = -1;
        }

        if( tk->i_cat != VIDEO_ES )
            p_frame->i_dts = p_frame->i_pts;
        else
        {
            p_frame->i_dts = p_frame->i_pts;
            p_frame->i_pts = VLC_TS_INVALID;
        }

        if( tk->i_dv_audio_rate )
            AVI_DvHandleAudio( p_demux, tk, p_frame );
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
        bool  b;

        es_out_Control( p_demux->out, ES_OUT_GET_ES_STATE, tk->p_es, &b );
        if( tk->p_es_dv_audio )
        {
            bool b_extra;
            es_out_Control( p_demux->out, ES_OUT_GET_ES_STATE, tk->p_es_dv_audio, &b_extra );
            b |= b_extra;
        }

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
            if( llabs( AVI_GetPTS( p_stream ) -
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
                    p_frame->i_pts = VLC_TS_INVALID;
                }

                if( p_stream->i_dv_audio_rate )
                    AVI_DvHandleAudio( p_demux, p_stream, p_frame );
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
    msg_Dbg( p_demux, "seek requested: %"PRId64" seconds %d%%",
             i_date / 1000000, i_percent );

    if( p_sys->b_seekable )
    {
        if( !p_sys->i_length )
        {
            avi_track_t *p_stream = NULL;
            unsigned i_stream = 0;
            int64_t i_pos;

            /* use i_percent to create a true i_date */
            msg_Warn( p_demux, "seeking without index at %d%%"
                      " only works for interleaved files", i_percent );
            if( i_percent >= 100 )
            {
                msg_Warn( p_demux, "cannot seek so far !" );
                return VLC_EGENERIC;
            }
            i_percent = __MAX( i_percent, 0 );

            /* try to find chunk that is at i_percent or the file */
            i_pos = __MAX( i_percent * stream_Size( p_demux->s ) / 100,
                           p_sys->i_movi_begin );
            /* search first selected stream (and prefer non-EOF ones) */
            for( unsigned i = 0; i < p_sys->i_track; i++ )
            {
                avi_track_t *p_track = p_sys->track[i];
                if( !p_track->b_activated )
                    continue;

                p_stream = p_track;
                i_stream = i;
                if( !p_track->b_eof )
                    break;
            }
            if( p_stream == NULL )
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

            while( i_pos >= p_stream->idx.p_entry[p_stream->i_idxposc].i_pos +
               p_stream->idx.p_entry[p_stream->i_idxposc].i_length + 8 )
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
            msg_Dbg( p_demux, "estimate date %"PRId64, i_date );
        }

        /* */
        for( unsigned i_stream = 0; i_stream < p_sys->i_track; i_stream++ )
        {
            avi_track_t *p_stream = p_sys->track[i_stream];

            if( !p_stream->b_activated )
                continue;

            p_stream->b_eof = AVI_TrackSeek( p_demux, i_stream, i_date ) != 0;
        }
        es_out_Control( p_demux->out, ES_OUT_SET_NEXT_DISPLAY_TIME, i_date );
        p_sys->i_time = i_date;
        msg_Dbg( p_demux, "seek: %"PRId64" seconds", p_sys->i_time /1000000 );
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
        double i64 = (uint64_t)stream_Tell( p_demux->s );
        return i64 / stream_Size( p_demux->s );
    }
    return 0.0;
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i;
    double   f, *pf;
    int64_t i64, *pi64;
    vlc_meta_t *p_meta;

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
            p_meta = (vlc_meta_t*)va_arg( args, vlc_meta_t* );
            vlc_meta_Merge( p_meta,  p_sys->meta );
            return VLC_SUCCESS;

        case DEMUX_GET_ATTACHMENTS:
        {
            if( p_sys->i_attachment <= 0 )
                return VLC_EGENERIC;

            input_attachment_t ***ppp_attach = va_arg( args, input_attachment_t*** );
            int *pi_int = va_arg( args, int * );

            *pi_int     = p_sys->i_attachment;
            *ppp_attach = calloc( p_sys->i_attachment, sizeof(**ppp_attach));
            for( unsigned i = 0; i < p_sys->i_attachment && *ppp_attach; i++ )
                (*ppp_attach)[i] = vlc_input_attachment_Duplicate( p_sys->attachment[i] );
            return VLC_SUCCESS;
        }

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
        if( tk->i_idxposc == tk->idx.i_size )
        {
            if( tk->i_idxposc )
            {
                /* use the last entry */
                i_count = tk->idx.p_entry[tk->idx.i_size - 1].i_lengthtotal
                            + tk->idx.p_entry[tk->idx.i_size - 1].i_length;
            }
        }
        else
        {
            i_count = tk->idx.p_entry[tk->i_idxposc].i_lengthtotal;
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
        if( !vlc_object_alive (p_demux) ) return VLC_EGENERIC;

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
                if( !vlc_object_alive (p_demux) ) return VLC_EGENERIC;
                msleep( 10000 );

                if( !(i_loop_count % (1024 * 10)) )
                    msg_Warn( p_demux, "don't seem to find any data..." );
            }
        }
        else
        {
            avi_track_t *tk_pk = p_sys->track[avi_pk.i_stream];

            /* add this chunk to the index */
            avi_entry_t index;
            index.i_id     = avi_pk.i_fourcc;
            index.i_flags  = AVI_GetKeyFlag(tk_pk->i_codec, avi_pk.i_peek);
            index.i_pos    = avi_pk.i_pos;
            index.i_length = avi_pk.i_size;
            avi_index_Append( &tk_pk->idx, &p_sys->i_movi_lastchunk_pos, &index );

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

    if(  i_ck >= p_stream->idx.i_size )
    {
        p_stream->i_idxposc = p_stream->idx.i_size - 1;
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

    if( ( p_stream->idx.i_size > 0 )
        &&( i_byte < p_stream->idx.p_entry[p_stream->idx.i_size - 1].i_lengthtotal +
                p_stream->idx.p_entry[p_stream->idx.i_size - 1].i_length ) )
    {
        /* index is valid to find the ck */
        /* uses dichototmie to be fast enougth */
        int i_idxposc = __MIN( p_stream->i_idxposc, p_stream->idx.i_size - 1 );
        int i_idxmax  = p_stream->idx.i_size;
        int i_idxmin  = 0;
        for( ;; )
        {
            if( p_stream->idx.p_entry[i_idxposc].i_lengthtotal > i_byte )
            {
                i_idxmax  = i_idxposc ;
                i_idxposc = ( i_idxmin + i_idxposc ) / 2 ;
            }
            else
            {
                if( p_stream->idx.p_entry[i_idxposc].i_lengthtotal +
                        p_stream->idx.p_entry[i_idxposc].i_length <= i_byte)
                {
                    i_idxmin  = i_idxposc ;
                    i_idxposc = (i_idxmax + i_idxposc ) / 2 ;
                }
                else
                {
                    p_stream->i_idxposc = i_idxposc;
                    p_stream->i_idxposb = i_byte -
                            p_stream->idx.p_entry[i_idxposc].i_lengthtotal;
                    return VLC_SUCCESS;
                }
            }
        }

    }
    else
    {
        p_stream->i_idxposc = p_stream->idx.i_size - 1;
        p_stream->i_idxposb = 0;
        do
        {
            p_stream->i_idxposc++;
            if( AVI_StreamChunkFind( p_demux, i_stream ) )
            {
                return VLC_EGENERIC;
            }

        } while( p_stream->idx.p_entry[p_stream->i_idxposc].i_lengthtotal +
                    p_stream->idx.p_entry[p_stream->i_idxposc].i_length <= i_byte );

        p_stream->i_idxposb = i_byte -
                       p_stream->idx.p_entry[p_stream->i_idxposc].i_lengthtotal;
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
                    tk->i_blockno += ( tk->idx.p_entry[i].i_length + tk->i_blocksize - 1 ) / tk->i_blocksize;
                }
                else
                {
                    tk->i_blockno++;
                }
            }
        }

        msg_Dbg( p_demux,
                 "old:%"PRId64" %s new %"PRId64,
                 i_oldpts,
                 i_oldpts > i_date ? ">" : "<",
                 i_date );

        if( p_stream->i_cat == VIDEO_ES )
        {
            /* search key frame */
            //if( i_date < i_oldpts || 1 )
            {
                while( p_stream->i_idxposc > 0 &&
                   !( p_stream->idx.p_entry[p_stream->i_idxposc].i_flags &
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
#if 0
            else
            {
                while( p_stream->i_idxposc < p_stream->idx.i_size &&
                        !( p_stream->idx.p_entry[p_stream->i_idxposc].i_flags &
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
#endif
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
 * Return true if it's a key frame
 ****************************************************************************/
static int AVI_GetKeyFlag( vlc_fourcc_t i_fourcc, uint8_t *p_byte )
{
    switch( i_fourcc )
    {
        case VLC_CODEC_DIV1:
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

        case VLC_CODEC_DIV2:
        case VLC_CODEC_DIV3:
        case VLC_CODEC_WMV1:
            /* we have
             *  picture type    0(I),1(P)     2bits
             */
            return p_byte[0] & 0xC0 ? 0 : AVIIF_KEYFRAME;
        case VLC_CODEC_MP4V:
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
            return vlc_fourcc_GetCodec( i_cat, i_codec );
        default:
            return VLC_FOURCC( 'u', 'n', 'd', 'f' );
    }
}

/****************************************************************************
 *
 ****************************************************************************/
static void AVI_ParseStreamHeader( vlc_fourcc_t i_id,
                                   unsigned int *pi_number, unsigned int *pi_type )
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
            case AVITWOCC_AC:
                SET_PTR( pi_type, VIDEO_ES );
                break;
            case AVITWOCC_tx:
            case AVITWOCC_sb:
                SET_PTR( pi_type, SPU_ES );
                break;
            case AVITWOCC_pc:
                SET_PTR( pi_type, IGNORE_ES );
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
    const uint8_t *p_peek;

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
            if( !vlc_object_alive (p_demux) ) return VLC_EGENERIC;

            msleep( 10000 );
            if( !(i_count % (1024 * 10)) )
                msg_Warn( p_demux, "trying to resync..." );
        }
    }
}

/****************************************************************************
 * Index stuff.
 ****************************************************************************/
static void avi_index_Init( avi_index_t *p_index )
{
    p_index->i_size  = 0;
    p_index->i_max   = 0;
    p_index->p_entry = NULL;
}
static void avi_index_Clean( avi_index_t *p_index )
{
    free( p_index->p_entry );
}
static void avi_index_Append( avi_index_t *p_index, off_t *pi_last_pos,
                              avi_entry_t *p_entry )
{
    /* Update last chunk position */
    if( *pi_last_pos < p_entry->i_pos )
         *pi_last_pos = p_entry->i_pos;

    /* add the entry */
    if( p_index->i_size >= p_index->i_max )
    {
        p_index->i_max += 16384;
        p_index->p_entry = realloc_or_free( p_index->p_entry,
                                            p_index->i_max * sizeof( *p_index->p_entry ) );
        if( !p_index->p_entry )
            return;
    }
    /* calculate cumulate length */
    if( p_index->i_size > 0 )
    {
        p_entry->i_lengthtotal =
            p_index->p_entry[p_index->i_size - 1].i_length +
                p_index->p_entry[p_index->i_size - 1].i_lengthtotal;
    }
    else
    {
        p_entry->i_lengthtotal = 0;
    }

    p_index->p_entry[p_index->i_size++] = *p_entry;
}

static int AVI_IndexFind_idx1( demux_t *p_demux,
                               avi_chunk_idx1_t **pp_idx1,
                               uint64_t *pi_offset )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    avi_chunk_list_t *p_riff = AVI_ChunkFind( &p_sys->ck_root, AVIFOURCC_RIFF, 0);
    avi_chunk_idx1_t *p_idx1 = AVI_ChunkFind( p_riff, AVIFOURCC_idx1, 0);

    if( !p_idx1 )
    {
        msg_Warn( p_demux, "cannot find idx1 chunk, no index defined" );
        return VLC_EGENERIC;
    }
    *pp_idx1 = p_idx1;

    /* The offset in the index should be from the start of the movi content,
     * but some broken files use offset from the start of the file. Just
     * checking the offset of the first packet is not enough as some files
     * has unused chunk at the beginning of the movi content.
     */
    avi_chunk_list_t *p_movi = AVI_ChunkFind( p_riff, AVIFOURCC_movi, 0);
    uint64_t i_first_pos = UINT64_MAX;
    for( unsigned i = 0; i < __MIN( p_idx1->i_entry_count, 100 ); i++ )
    {
        if ( p_idx1->entry[i].i_length > 0 )
            i_first_pos = __MIN( i_first_pos, p_idx1->entry[i].i_pos );
    }

    const uint64_t i_movi_content = p_movi->i_chunk_pos + 8;
    if( i_first_pos < i_movi_content )
    {
        *pi_offset = i_movi_content;
    }
    else if( p_sys->b_seekable && i_first_pos < UINT64_MAX )
    {
        const uint8_t *p_peek;
        if( !stream_Seek( p_demux->s, i_movi_content + i_first_pos ) &&
            stream_Peek( p_demux->s, &p_peek, 4 ) >= 4 &&
            ( !isdigit( p_peek[0] ) || !isdigit( p_peek[1] ) ||
              !isalpha( p_peek[2] ) || !isalpha( p_peek[3] ) ) )
            *pi_offset = 0;
        else
            *pi_offset = i_movi_content;
    }
    else
    {
        *pi_offset = 0;
    }
    return VLC_SUCCESS;
}

static int AVI_IndexLoad_idx1( demux_t *p_demux,
                               avi_index_t p_index[], off_t *pi_last_offset )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    avi_chunk_idx1_t *p_idx1;
    uint64_t         i_offset;
    if( AVI_IndexFind_idx1( p_demux, &p_idx1, &i_offset ) )
        return VLC_EGENERIC;

    for( unsigned i_index = 0; i_index < p_idx1->i_entry_count; i_index++ )
    {
        unsigned i_cat;
        unsigned i_stream;

        AVI_ParseStreamHeader( p_idx1->entry[i_index].i_fourcc,
                               &i_stream,
                               &i_cat );
        if( i_stream < p_sys->i_track &&
            (i_cat == p_sys->track[i_stream]->i_cat || i_cat == UNKNOWN_ES ) )
        {
            avi_entry_t index;
            index.i_id     = p_idx1->entry[i_index].i_fourcc;
            index.i_flags  = p_idx1->entry[i_index].i_flags&(~AVIIF_FIXKEYFRAME);
            index.i_pos    = p_idx1->entry[i_index].i_pos + i_offset;
            index.i_length = p_idx1->entry[i_index].i_length;

            avi_index_Append( &p_index[i_stream], pi_last_offset, &index );
        }
    }
    return VLC_SUCCESS;
}

static void __Parse_indx( demux_t *p_demux, avi_index_t *p_index, off_t *pi_max_offset,
                          avi_chunk_indx_t *p_indx )
{
    avi_entry_t index;

    msg_Dbg( p_demux, "loading subindex(0x%x) %d entries", p_indx->i_indextype, p_indx->i_entriesinuse );
    if( p_indx->i_indexsubtype == 0 )
    {
        for( unsigned i = 0; i < p_indx->i_entriesinuse; i++ )
        {
            index.i_id     = p_indx->i_id;
            index.i_flags  = p_indx->idx.std[i].i_size & 0x80000000 ? 0 : AVIIF_KEYFRAME;
            index.i_pos    = p_indx->i_baseoffset + p_indx->idx.std[i].i_offset - 8;
            index.i_length = p_indx->idx.std[i].i_size&0x7fffffff;

            avi_index_Append( p_index, pi_max_offset, &index );
        }
    }
    else if( p_indx->i_indexsubtype == AVI_INDEX_2FIELD )
    {
        for( unsigned i = 0; i < p_indx->i_entriesinuse; i++ )
        {
            index.i_id     = p_indx->i_id;
            index.i_flags  = p_indx->idx.field[i].i_size & 0x80000000 ? 0 : AVIIF_KEYFRAME;
            index.i_pos    = p_indx->i_baseoffset + p_indx->idx.field[i].i_offset - 8;
            index.i_length = p_indx->idx.field[i].i_size;

            avi_index_Append( p_index, pi_max_offset, &index );
        }
    }
    else
    {
        msg_Warn( p_demux, "unknown subtype index(0x%x)", p_indx->i_indexsubtype );
    }
}

static void AVI_IndexLoad_indx( demux_t *p_demux,
                                avi_index_t p_index[], off_t *pi_last_offset )
{
    demux_sys_t         *p_sys = p_demux->p_sys;

    avi_chunk_list_t    *p_riff;
    avi_chunk_list_t    *p_hdrl;

    p_riff = AVI_ChunkFind( &p_sys->ck_root, AVIFOURCC_RIFF, 0);
    p_hdrl = AVI_ChunkFind( p_riff, AVIFOURCC_hdrl, 0 );

    for( unsigned i_stream = 0; i_stream < p_sys->i_track; i_stream++ )
    {
        avi_chunk_list_t    *p_strl;
        avi_chunk_indx_t    *p_indx;

#define p_stream  p_sys->track[i_stream]
        p_strl = AVI_ChunkFind( p_hdrl, AVIFOURCC_strl, i_stream );
        p_indx = AVI_ChunkFind( p_strl, AVIFOURCC_indx, 0 );

        if( !p_indx )
        {
            if( p_sys->b_odml )
                msg_Warn( p_demux, "cannot find indx (misdetect/broken OpenDML "
                                   "file?)" );
            continue;
        }

        if( p_indx->i_indextype == AVI_INDEX_OF_CHUNKS )
        {
            __Parse_indx( p_demux, &p_index[i_stream], pi_last_offset, p_indx );
        }
        else if( p_indx->i_indextype == AVI_INDEX_OF_INDEXES )
        {
            avi_chunk_t    ck_sub;
            for( unsigned i = 0; i < p_indx->i_entriesinuse; i++ )
            {
                if( stream_Seek( p_demux->s, p_indx->idx.super[i].i_offset )||
                    AVI_ChunkRead( p_demux->s, &ck_sub, NULL  ) )
                {
                    break;
                }
                if( ck_sub.indx.i_indextype == AVI_INDEX_OF_CHUNKS )
                    __Parse_indx( p_demux, &p_index[i_stream], pi_last_offset, &ck_sub.indx );
                AVI_ChunkFree( p_demux->s, &ck_sub );
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

    /* Load indexes */
    assert( p_sys->i_track <= 100 );
    avi_index_t p_idx_indx[p_sys->i_track];
    avi_index_t p_idx_idx1[p_sys->i_track];
    for( unsigned i = 0; i < p_sys->i_track; i++ )
    {
        avi_index_Init( &p_idx_indx[i] );
        avi_index_Init( &p_idx_idx1[i] );
    }
    off_t i_indx_last_pos = p_sys->i_movi_lastchunk_pos;
    off_t i_idx1_last_pos = p_sys->i_movi_lastchunk_pos;

    AVI_IndexLoad_indx( p_demux, p_idx_indx, &i_indx_last_pos );
    if( !p_sys->b_odml )
        AVI_IndexLoad_idx1( p_demux, p_idx_idx1, &i_idx1_last_pos );

    /* Select the longest index */
    for( unsigned i = 0; i < p_sys->i_track; i++ )
    {
        if( p_idx_indx[i].i_size > p_idx_idx1[i].i_size )
        {
            msg_Dbg( p_demux, "selected ODML index for stream[%u]", i );
            p_sys->track[i]->idx = p_idx_indx[i];
            avi_index_Clean( &p_idx_idx1[i] );
        }
        else
        {
            msg_Dbg( p_demux, "selected standard index for stream[%u]", i );
            p_sys->track[i]->idx = p_idx_idx1[i];
            avi_index_Clean( &p_idx_indx[i] );
        }
    }
    p_sys->i_movi_lastchunk_pos = __MAX( i_indx_last_pos, i_idx1_last_pos );

    for( unsigned i = 0; i < p_sys->i_track; i++ )
    {
        avi_index_t *p_index = &p_sys->track[i]->idx;

        /* Fix key flag */
        bool b_key = false;
        for( unsigned j = 0; !b_key && j < p_index->i_size; j++ )
            b_key = p_index->p_entry[j].i_flags & AVIIF_KEYFRAME;
        if( !b_key )
        {
            msg_Err( p_demux, "no key frame set for track %u", i );
            for( unsigned j = 0; j < p_index->i_size; j++ )
                p_index->p_entry[j].i_flags |= AVIIF_KEYFRAME;
        }

        /* */
        msg_Dbg( p_demux, "stream[%d] created %d index entries",
                 i, p_index->i_size );
    }
}

static void AVI_IndexCreate( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    avi_chunk_list_t *p_riff;
    avi_chunk_list_t *p_movi;

    unsigned int i_stream;
    off_t i_movi_end;

    mtime_t i_dialog_update;
    dialog_progress_bar_t *p_dialog = NULL;

    p_riff = AVI_ChunkFind( &p_sys->ck_root, AVIFOURCC_RIFF, 0);
    p_movi = AVI_ChunkFind( p_riff, AVIFOURCC_movi, 0);

    if( !p_movi )
    {
        msg_Err( p_demux, "cannot find p_movi" );
        return;
    }

    for( i_stream = 0; i_stream < p_sys->i_track; i_stream++ )
        avi_index_Init( &p_sys->track[i_stream]->idx );

    i_movi_end = __MIN( (off_t)(p_movi->i_chunk_pos + p_movi->i_chunk_size),
                        stream_Size( p_demux->s ) );

    stream_Seek( p_demux->s, p_movi->i_chunk_pos + 12 );
    msg_Warn( p_demux, "creating index from LIST-movi, will take time !" );


    /* Only show dialog if AVI is > 10MB */
    i_dialog_update = mdate();
    if( stream_Size( p_demux->s ) > 10000000 )
        p_dialog = dialog_ProgressCreate( p_demux, _("Fixing AVI Index..."),
                                       NULL, _("Cancel") );

    for( ;; )
    {
        avi_packet_t pk;

        if( !vlc_object_alive (p_demux) )
            break;

        /* Don't update/check dialog too often */
        if( p_dialog && mdate() - i_dialog_update > 100000 )
        {
            if( dialog_ProgressCancelled( p_dialog ) )
                break;

            double f_current = stream_Tell( p_demux->s );
            double f_size    = stream_Size( p_demux->s );
            double f_pos     = f_current / f_size;
            dialog_ProgressSet( p_dialog, NULL, f_pos );

            i_dialog_update = mdate();
        }

        if( AVI_PacketGetHeader( p_demux, &pk ) )
            break;

        if( pk.i_stream < p_sys->i_track &&
            pk.i_cat == p_sys->track[pk.i_stream]->i_cat )
        {
            avi_track_t *tk = p_sys->track[pk.i_stream];

            avi_entry_t index;
            index.i_id      = pk.i_fourcc;
            index.i_flags   = AVI_GetKeyFlag(tk->i_codec, pk.i_peek);
            index.i_pos     = pk.i_pos;
            index.i_length  = pk.i_size;
            avi_index_Append( &tk->idx, &p_sys->i_movi_lastchunk_pos, &index );
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
                    if( stream_Seek( p_demux->s, p_sysx->i_chunk_pos + 24 ) )
                        goto print_stat;
                    break;
                }
                goto print_stat;

            case AVIFOURCC_RIFF:
                    msg_Dbg( p_demux, "new RIFF chunk found" );
                    break;

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
    if( p_dialog != NULL )
        dialog_ProgressDestroy( p_dialog );

    for( i_stream = 0; i_stream < p_sys->i_track; i_stream++ )
    {
        msg_Dbg( p_demux, "stream[%d] creating %d index entries",
                i_stream, p_sys->track[i_stream]->idx.i_size );
    }
}

/* */
static void AVI_MetaLoad( demux_t *p_demux,
                          avi_chunk_list_t *p_riff, avi_chunk_avih_t *p_avih )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    vlc_meta_t *p_meta = p_sys->meta = vlc_meta_New();
    if( !p_meta )
        return;

    char buffer[200];
    snprintf( buffer, sizeof(buffer), "%s%s%s%s",
              p_avih->i_flags&AVIF_HASINDEX      ? " HAS_INDEX"      : "",
              p_avih->i_flags&AVIF_MUSTUSEINDEX  ? " MUST_USE_INDEX" : "",
              p_avih->i_flags&AVIF_ISINTERLEAVED ? " IS_INTERLEAVED" : "",
              p_avih->i_flags&AVIF_TRUSTCKTYPE   ? " TRUST_CKTYPE"   : "" );
    vlc_meta_SetSetting( p_meta, buffer );

    avi_chunk_list_t *p_info = AVI_ChunkFind( p_riff, AVIFOURCC_INFO, 0 );
    if( !p_info )
        return;

    static const struct {
        vlc_fourcc_t i_id;
        int          i_type;
    } p_dsc[] = {
        { AVIFOURCC_IART, vlc_meta_Artist },
        { AVIFOURCC_ICMT, vlc_meta_Description },
        { AVIFOURCC_ICOP, vlc_meta_Copyright },
        { AVIFOURCC_IGNR, vlc_meta_Genre },
        { AVIFOURCC_INAM, vlc_meta_Title },
        { AVIFOURCC_ICRD, vlc_meta_Date },
        { AVIFOURCC_ILNG, vlc_meta_Language },
        { AVIFOURCC_IRTD, vlc_meta_Rating },
        { AVIFOURCC_IWEB, vlc_meta_URL },
        { AVIFOURCC_IPRT, vlc_meta_TrackNumber },
        { AVIFOURCC_IFRM, vlc_meta_TrackTotal },
        { 0, -1 }
    };
    for( int i = 0; p_dsc[i].i_id != 0; i++ )
    {
        avi_chunk_STRING_t *p_strz = AVI_ChunkFind( p_info, p_dsc[i].i_id, 0 );
        if( !p_strz )
            continue;
        char *psz_value = FromACP( p_strz->p_str );
        if( !psz_value )
            continue;

        if( *psz_value )
            vlc_meta_Set( p_meta, p_dsc[i].i_type, psz_value );
        free( psz_value );
    }

    static const vlc_fourcc_t p_extra[] = {
        AVIFOURCC_IARL, AVIFOURCC_ICMS, AVIFOURCC_ICRP, AVIFOURCC_IDIM, AVIFOURCC_IDPI,
        AVIFOURCC_IENG, AVIFOURCC_IKEY, AVIFOURCC_ILGT, AVIFOURCC_IMED, AVIFOURCC_IPLT,
        AVIFOURCC_IPRD, AVIFOURCC_ISBJ, AVIFOURCC_ISFT, AVIFOURCC_ISHP, AVIFOURCC_ISRC,
        AVIFOURCC_ISRF, AVIFOURCC_ITCH, AVIFOURCC_ISMP, AVIFOURCC_IDIT, AVIFOURCC_ISGN,
        AVIFOURCC_IWRI, AVIFOURCC_IPRO, AVIFOURCC_ICNM, AVIFOURCC_IPDS, AVIFOURCC_IEDT,
        AVIFOURCC_ICDS, AVIFOURCC_IMUS, AVIFOURCC_ISTD, AVIFOURCC_IDST, AVIFOURCC_ICNT,
        AVIFOURCC_ISTR, 0,
    };

    for( int i = 0; p_extra[i] != 0; i++ )
    {
        avi_chunk_STRING_t *p_strz = AVI_ChunkFind( p_info, p_extra[i], 0 );
        if( !p_strz )
            continue;
        char *psz_value = FromACP( p_strz->p_str );
        if( !psz_value )
            continue;

        if( *psz_value )
            vlc_meta_AddExtra( p_meta, p_strz->p_type, psz_value );
        free( psz_value );
    }
}

static void AVI_DvHandleAudio( demux_t *p_demux, avi_track_t *tk, block_t *p_frame )
{
    size_t i_offset = 80*6+80*16*3 + 3;
    if( p_frame->i_buffer < i_offset + 5 )
        return;

    if( p_frame->p_buffer[i_offset] != 0x50 )
        return;

    es_format_t fmt;
    dv_get_audio_format( &fmt, &p_frame->p_buffer[i_offset + 1] );

    if( tk->p_es_dv_audio && tk->i_dv_audio_rate != fmt.audio.i_rate )
    {
        es_out_Del( p_demux->out, tk->p_es_dv_audio );
        tk->p_es_dv_audio = es_out_Add( p_demux->out, &fmt );
    }
    else if( !tk->p_es_dv_audio )
    {
        tk->p_es_dv_audio = es_out_Add( p_demux->out, &fmt );
    }
    tk->i_dv_audio_rate = fmt.audio.i_rate;

    block_t *p_frame_audio = dv_extract_audio( p_frame );
    if( p_frame_audio )
        es_out_Send( p_demux->out, tk->p_es_dv_audio, p_frame_audio );
}

/*****************************************************************************
 * Subtitles
 *****************************************************************************/
static void AVI_ExtractSubtitle( demux_t *p_demux,
                                 unsigned int i_stream,
                                 avi_chunk_list_t *p_strl,
                                 avi_chunk_STRING_t *p_strn )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t *p_block = NULL;
    input_attachment_t *p_attachment = NULL;
    char *psz_description = NULL;
    avi_chunk_indx_t *p_indx = NULL;

    if( !p_sys->b_seekable )
        goto exit;

    p_indx = AVI_ChunkFind( p_strl, AVIFOURCC_indx, 0 );
    avi_chunk_t ck;
    int64_t  i_position;
    unsigned i_size;
    if( p_indx )
    {
        if( p_indx->i_indextype == AVI_INDEX_OF_INDEXES &&
            p_indx->i_entriesinuse > 0 )
        {
            if( stream_Seek( p_demux->s, p_indx->idx.super[0].i_offset )||
                AVI_ChunkRead( p_demux->s, &ck, NULL  ) )
                goto exit;
            p_indx = &ck.indx;
        }

        if( p_indx->i_indextype != AVI_INDEX_OF_CHUNKS ||
            p_indx->i_entriesinuse != 1 ||
            p_indx->i_indexsubtype != 0 )
            goto exit;

        i_position  = p_indx->i_baseoffset +
                      p_indx->idx.std[0].i_offset - 8;
        i_size      = (p_indx->idx.std[0].i_size & 0x7fffffff) + 8;
    }
    else
    {
        avi_chunk_idx1_t *p_idx1;
        uint64_t         i_offset;

        if( AVI_IndexFind_idx1( p_demux, &p_idx1, &i_offset ) )
            goto exit;

        i_size = 0;
        for( unsigned i = 0; i < p_idx1->i_entry_count; i++ )
        {
            const idx1_entry_t *e = &p_idx1->entry[i];
            unsigned i_cat;
            unsigned i_stream_idx;

            AVI_ParseStreamHeader( e->i_fourcc, &i_stream_idx, &i_cat );
            if( i_cat == SPU_ES && i_stream_idx == i_stream )
            {
                i_position = e->i_pos + i_offset;
                i_size     = e->i_length + 8;
                break;
            }
        }
        if( i_size <= 0 )
            goto exit;
    }

    /* */
    if( i_size > 10000000 )
    {
        msg_Dbg( p_demux, "Attached subtitle too big: %u", i_size );
        goto exit;
    }

    if( stream_Seek( p_demux->s, i_position ) )
        goto exit;
    p_block = stream_Block( p_demux->s, i_size );
    if( !p_block )
        goto exit;

    /* Parse packet header */
    const uint8_t *p = p_block->p_buffer;
    if( i_size < 8 || p[2] != 't' || p[3] != 'x' )
        goto exit;
    p += 8;
    i_size -= 8;

    /* Parse subtitle chunk header */
    if( i_size < 11 || memcmp( p, "GAB2", 4 ) ||
        p[4] != 0x00 || GetWLE( &p[5] ) != 0x2 )
        goto exit;
    const unsigned i_name = GetDWLE( &p[7] );
    if( 11 + i_size <= i_name )
        goto exit;
    if( i_name > 0 )
        psz_description = FromCharset( "UTF-16LE", &p[11], i_name );
    p += 11 + i_name;
    i_size -= 11 + i_name;
    if( i_size < 6 || GetWLE( &p[0] ) != 0x04 )
        goto exit;
    const unsigned i_payload = GetDWLE( &p[2] );
    if( i_size < 6 + i_payload || i_payload <= 0 )
        goto exit;
    p += 6;
    i_size -= 6;

    if( !psz_description )
        psz_description = p_strn ? FromACP( p_strn->p_str ) : NULL;
    char *psz_name;
    if( asprintf( &psz_name, "subtitle%d.srt", p_sys->i_attachment ) <= 0 )
        psz_name = NULL;
    p_attachment = vlc_input_attachment_New( psz_name,
                                             "application/x-srt",
                                             psz_description,
                                             p, i_payload );
    if( p_attachment )
        TAB_APPEND( p_sys->i_attachment, p_sys->attachment, p_attachment );
    free( psz_name );

exit:
    free( psz_description );

    if( p_block )
        block_Release( p_block );

    if( p_attachment )
        msg_Dbg( p_demux, "Loaded an embedded subtitle" );
    else
        msg_Warn( p_demux, "Failed to load an embedded subtitle" );

    if( p_indx == &ck.indx )
        AVI_ChunkFree( p_demux->s, &ck );
}
/*****************************************************************************
 * Stream management
 *****************************************************************************/
static int AVI_TrackStopFinishedStreams( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    unsigned int i;
    int b_end = true;

    for( i = 0; i < p_sys->i_track; i++ )
    {
        avi_track_t *tk = p_sys->track[i];
        if( tk->i_idxposc >= tk->idx.i_size )
        {
            tk->b_eof = true;
        }
        else
        {
            b_end = false;
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
        if( tk->idx.i_size < 1 || !tk->idx.p_entry )
        {
            continue;
        }

        if( tk->i_samplesize )
        {
            i_length = AVI_GetDPTS( tk,
                                    tk->idx.p_entry[tk->idx.i_size-1].i_lengthtotal +
                                        tk->idx.p_entry[tk->idx.i_size-1].i_length );
        }
        else
        {
            i_length = AVI_GetDPTS( tk, tk->idx.i_size );
        }
        i_length /= (mtime_t)1000000;    /* in seconds */

        msg_Dbg( p_demux,
                 "stream[%d] length:%"PRId64" (based on index)",
                 i,
                 i_length );
        i_maxlength = __MAX( i_maxlength, i_length );
    }

    return i_maxlength;
}
