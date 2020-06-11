/*****************************************************************************
 * avi.c : AVI file Stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2009 VLC authors and VideoLAN
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
#include <limits.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_input.h>

#include <vlc_dialog.h>

#include <vlc_meta.h>
#include <vlc_codecs.h>
#include <vlc_charset.h>

#include "libavi.h"
#include "../rawdv.h"
#include "bitmapinfoheader.h"

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
static int Seek            ( demux_t *, vlc_tick_t, double, bool );
static int Demux_Seekable  ( demux_t * );
static int Demux_UnSeekable( demux_t * );

static char *FromACP( const char *str )
{
    return FromCharset(vlc_pgettext("GetACP", "CP1252"), str, strlen(str));
}

#define IGNORE_ES DATA_ES
#define READ_LENGTH                VLC_TICK_FROM_MS(25)
#define READ_LENGTH_NONINTERLEAVED VLC_TICK_FROM_MS(1500)

//#define AVI_DEBUG

typedef struct
{
    vlc_fourcc_t i_fourcc;
    uint64_t     i_pos;
    uint32_t     i_size;
    vlc_fourcc_t i_type;     /* only for AVIFOURCC_LIST */

    uint8_t      i_peek[8];  /* first 8 bytes */

    unsigned int i_stream;
    enum es_format_category_e i_cat;
} avi_packet_t;


typedef struct
{
    vlc_fourcc_t i_id;
    uint32_t     i_flags;
    uint64_t     i_pos;
    uint32_t     i_length;
    uint64_t     i_lengthtotal;

} avi_entry_t;

typedef struct
{
    uint32_t        i_size;
    uint32_t        i_max;
    avi_entry_t     *p_entry;

} avi_index_t;
static void avi_index_Init( avi_index_t * );
static void avi_index_Clean( avi_index_t * );
static void avi_index_Append( avi_index_t *, uint64_t *, avi_entry_t * );

typedef struct
{
    bool            b_activated;
    bool            b_eof;

    unsigned int    i_rate;
    unsigned int    i_scale;
    unsigned int    i_samplesize;

    struct bitmapinfoheader_properties bihprops;

    es_format_t     fmt;
    es_out_id_t     *p_es;
    int             i_next_block_flags;

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

typedef struct
{
    vlc_tick_t i_time;
    vlc_tick_t i_length;

    bool  b_interleaved;
    bool  b_seekable;
    bool  b_fastseekable;
    bool  b_indexloaded; /* if we read indexes from end of file before starting */
    vlc_tick_t i_read_increment;
    uint32_t i_avih_flags;
    avi_chunk_t ck_root;

    bool  b_odml;

    uint64_t i_movi_begin;
    uint64_t i_movi_lastchunk_pos;   /* XXX position of last valid chunk */

    /* number of streams and information */
    unsigned int i_track;
    avi_track_t  **track;

    /* meta */
    vlc_meta_t  *meta;

    unsigned int       i_attachment;
    input_attachment_t **attachment;
} demux_sys_t;

#define __EVEN(x) (((x) & 1) ? (x) + 1 : (x))

static int64_t AVI_PTSToChunk( avi_track_t *, vlc_tick_t i_pts );
static int64_t AVI_PTSToByte ( avi_track_t *, vlc_tick_t i_pts );
static vlc_tick_t AVI_GetDPTS   ( avi_track_t *, int64_t i_count );
static vlc_tick_t AVI_GetPTS    ( avi_track_t * );


static int AVI_StreamChunkFind( demux_t *, unsigned int i_stream );
static int AVI_StreamChunkSet ( demux_t *,
                                unsigned int i_stream, unsigned int i_ck );
static int AVI_StreamBytesSet ( demux_t *,
                                unsigned int i_stream, uint64_t i_byte );

vlc_fourcc_t AVI_FourccGetCodec( unsigned int i_cat, vlc_fourcc_t );
static int   AVI_GetKeyFlag    ( vlc_fourcc_t , uint8_t * );

static int AVI_PacketGetHeader( demux_t *, avi_packet_t *p_pk );
static int AVI_PacketNext     ( demux_t * );
static int AVI_PacketSearch   ( demux_t * );

static void AVI_IndexLoad    ( demux_t * );
static void AVI_IndexCreate  ( demux_t * );

static void AVI_ExtractSubtitle( demux_t *, unsigned int i_stream, avi_chunk_list_t *, avi_chunk_STRING_t * );

static void AVI_DvHandleAudio( demux_t *, avi_track_t *, block_t * );

static vlc_tick_t  AVI_MovieGetLength( demux_t * );

static void AVI_MetaLoad( demux_t *, avi_chunk_list_t *p_riff, avi_chunk_avih_t *p_avih );

/*****************************************************************************
 * Stream management
 *****************************************************************************/
static int        AVI_TrackSeek  ( demux_t *, int, vlc_tick_t );
static int        AVI_TrackStopFinishedStreams( demux_t *);

/* Remarks:
 - For VBR mp3 stream:
    count blocks by rounded-up chunksizes instead of chunks
    we need full emulation of dshow avi demuxer bugs :(
    fixes silly nandub-style a-v delaying in avi with vbr mp3...
    (from mplayer 2002/08/02)
 - to complete....
 */

#define QNAP_HEADER_SIZE 56
static bool IsQNAPCodec(vlc_fourcc_t codec)
{
    switch (codec)
    {
        case VLC_FOURCC('w', '2', '6', '4'):
        case VLC_FOURCC('q', '2', '6', '4'):
        case VLC_FOURCC('Q', '2', '6', '4'):
        case VLC_FOURCC('w', 'M', 'P', '4'):
        case VLC_FOURCC('q', 'M', 'P', '4'):
        case VLC_FOURCC('Q', 'M', 'P', '4'):
        case VLC_FOURCC('w', 'I', 'V', 'G'):
        case VLC_FOURCC('q', 'I', 'V', 'G'):
        case VLC_FOURCC('Q', 'I', 'V', 'G'):
            return true;
        default:
            return false;
    }
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
            es_format_Clean( &p_sys->track[i]->fmt );
            avi_index_Clean( &p_sys->track[i]->idx );
            free( p_sys->track[i] );
        }
    }
    free( p_sys->track );

    AVI_ChunkFreeRoot( p_demux->s, &p_sys->ck_root );
    if( p_sys->meta )
        vlc_meta_Delete( p_sys->meta );

    for( unsigned i = 0; i < p_sys->i_attachment; i++)
        vlc_input_attachment_Delete(p_sys->attachment[i]);
    free(p_sys->attachment);

    free( p_sys );
}

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
    unsigned int i_peeker;

    const uint8_t *p_peek;

    /* Is it an avi file ? */
    if( vlc_stream_Peek( p_demux->s, &p_peek, 200 ) < 200 )
        return VLC_EGENERIC;

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

    if( i_peeker > 0
     && vlc_stream_Read( p_demux->s, NULL, i_peeker ) < i_peeker )
        return VLC_EGENERIC;

    /* Initialize input structures. */
    p_sys = p_demux->p_sys = calloc( 1, sizeof(demux_sys_t) );
    if( unlikely(!p_sys) )
        return VLC_EGENERIC;
    p_sys->b_odml   = false;
    p_sys->meta     = NULL;
    TAB_INIT(p_sys->i_track, p_sys->track);
    TAB_INIT(p_sys->i_attachment, p_sys->attachment);

    vlc_stream_Control( p_demux->s, STREAM_CAN_FASTSEEK,
                        &p_sys->b_fastseekable );
    vlc_stream_Control( p_demux->s, STREAM_CAN_SEEK, &p_sys->b_seekable );

    p_sys->b_interleaved = var_InheritBool( p_demux, "avi-interleaved" );

    if( AVI_ChunkReadRoot( p_demux->s, &p_sys->ck_root ) )
    {
        msg_Err( p_demux, "avi module discarded (invalid file)" );
        free(p_sys);
        return VLC_EGENERIC;
    }

    if( AVI_ChunkCount( &p_sys->ck_root, AVIFOURCC_RIFF, true ) > 1 )
    {
        unsigned int i_count =
            AVI_ChunkCount( &p_sys->ck_root, AVIFOURCC_RIFF, true );

        msg_Warn( p_demux, "multiple riff -> OpenDML ?" );
        for( unsigned i = 1; i < i_count; i++ )
        {
            avi_chunk_list_t *p_sysx;

            p_sysx = AVI_ChunkFind( &p_sys->ck_root, AVIFOURCC_RIFF, i, true );
            if( p_sysx && p_sysx->i_type == AVIFOURCC_AVIX )
            {
                msg_Warn( p_demux, "detected OpenDML file" );
                p_sys->b_odml = true;
                break;
            }
        }
    }

    p_riff  = AVI_ChunkFind( &p_sys->ck_root, AVIFOURCC_RIFF, 0, true );
    p_hdrl  = AVI_ChunkFind( p_riff, AVIFOURCC_hdrl, 0, true );
    p_movi  = AVI_ChunkFind( p_riff, AVIFOURCC_movi, 0, true );
    if( !p_movi )
        p_movi  = AVI_ChunkFind( &p_sys->ck_root, AVIFOURCC_movi, 0, true );

    if( !p_hdrl || !p_movi )
    {
        msg_Err( p_demux, "invalid file: cannot find hdrl or movi chunks" );
        goto error;
    }

    if( !( p_avih = AVI_ChunkFind( p_hdrl, AVIFOURCC_avih, 0, false ) ) )
    {
        msg_Err( p_demux, "invalid file: cannot find avih chunk" );
        goto error;
    }
    i_track = AVI_ChunkCount( p_hdrl, AVIFOURCC_strl, true );
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

    p_sys->b_interleaved |= (p_avih->i_flags & AVIF_ISINTERLEAVED);

    /* Set callbacks */
    p_demux->pf_control = Control;

    if( p_sys->b_fastseekable )
    {
        p_demux->pf_demux = Demux_Seekable;
        p_sys->i_read_increment = READ_LENGTH;
    }
    else if( p_sys->b_seekable )
    {
        p_demux->pf_demux = Demux_Seekable;
        p_sys->i_read_increment = READ_LENGTH_NONINTERLEAVED;
        if( !p_sys->b_interleaved )
            msg_Warn( p_demux, "Non interleaved content over slow seekable, "
                               "expect bad performance" );
    }
    else
    {
        msg_Warn( p_demux, "Non seekable content " );

        p_demux->pf_demux = Demux_UnSeekable;
        p_sys->i_read_increment = READ_LENGTH_NONINTERLEAVED;
         /* non seekable and non interleaved case ? well... */
        if( !p_sys->b_interleaved )
        {
            msg_Warn( p_demux, "Non seekable non interleaved content, "
                               "disabling other tracks" );
            i_track = __MIN(i_track, 1);
        }
    }

    AVI_MetaLoad( p_demux, p_riff, p_avih );
    p_sys->i_avih_flags = p_avih->i_flags;

    /* now read info on each stream and create ES */
    for( unsigned i = 0 ; i < i_track; i++ )
    {
        avi_track_t           *tk     = calloc( 1, sizeof( avi_track_t ) );
        if( unlikely( !tk ) )
            goto error;

        avi_chunk_list_t      *p_strl = AVI_ChunkFind( p_hdrl, AVIFOURCC_strl, i, true );
        avi_chunk_strh_t      *p_strh = AVI_ChunkFind( p_strl, AVIFOURCC_strh, 0, false );
        avi_chunk_STRING_t    *p_strn = AVI_ChunkFind( p_strl, AVIFOURCC_strn, 0, false );
        avi_chunk_strf_auds_t *p_auds = NULL;
        avi_chunk_strf_vids_t *p_vids = NULL;

        tk->b_eof = false;
        tk->b_activated = true;

        p_vids = (avi_chunk_strf_vids_t*)AVI_ChunkFind( p_strl, AVIFOURCC_strf, 0, false );
        p_auds = (avi_chunk_strf_auds_t*)p_vids;

        if( p_strl == NULL || p_strh == NULL || p_vids == NULL )
        {
            msg_Warn( p_demux, "stream[%d] incomplete", i );
            free( tk );
            continue;
        }

        tk->i_rate  = p_strh->i_rate;
        tk->i_scale = p_strh->i_scale;
        tk->i_samplesize = p_strh->i_samplesize;
        msg_Dbg( p_demux, "stream[%u] rate:%u scale:%u samplesize:%u",
                i, tk->i_rate, tk->i_scale, tk->i_samplesize );
        if( !tk->i_scale || !tk->i_rate || !(tk->i_rate * CLOCK_FREQ / tk->i_scale) )
        {
            free( tk );
            continue;
        }

        switch( p_strh->i_type )
        {
            case( AVIFOURCC_auds ):
            {
                es_format_Init( &tk->fmt, AUDIO_ES, 0 );

                if( p_auds->p_wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                    p_auds->p_wf->cbSize >= sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX) )
                {
                    WAVEFORMATEXTENSIBLE *p_wfe = (WAVEFORMATEXTENSIBLE *)p_auds->p_wf;
                    tk->fmt.i_codec = AVI_FourccGetCodec( AUDIO_ES, p_wfe->SubFormat.Data1 );
                }
                else
                    tk->fmt.i_codec = AVI_FourccGetCodec( AUDIO_ES, p_auds->p_wf->wFormatTag );

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
                    msg_Warn( p_demux, "track[%u] samplesize=%u and blocksize=%u are not equal."
                                       "Using blocksize as a workaround.",
                                       i, tk->i_samplesize, tk->i_blocksize );
                    tk->i_samplesize = tk->i_blocksize;
                }

                if( tk->fmt.i_codec == VLC_CODEC_VORBIS )
                {
                    tk->i_blocksize = 0; /* fix vorbis VBR decoding */
                }

                if ( tk->fmt.i_codec == VLC_CODEC_MP4A )
                {
                    tk->i_samplesize = 0; /* ADTS/AAC VBR */
                }

                /* Fix broken scale/rate */
                if ( tk->fmt.i_codec == VLC_CODEC_ADPCM_IMA_WAV &&
                     tk->i_samplesize && tk->i_samplesize > tk->i_rate )
                {
                    tk->i_scale = 1017;
                    tk->i_rate = p_auds->p_wf->nSamplesPerSec;
                }

                /* From libavformat */
                /* Fix broken sample size (which is mp2 num samples / frame) #12722 */
                if( tk->fmt.i_codec == VLC_CODEC_MPGA &&
                    tk->i_samplesize == 1152 && p_auds->p_wf->nBlockAlign == 1152 )
                {
                    p_auds->p_wf->nBlockAlign = tk->i_samplesize = 0;
                }

                tk->fmt.audio.i_channels        = p_auds->p_wf->nChannels;
                tk->fmt.audio.i_rate            = p_auds->p_wf->nSamplesPerSec;
                tk->fmt.i_bitrate               = p_auds->p_wf->nAvgBytesPerSec*8;
                tk->fmt.audio.i_blockalign      = p_auds->p_wf->nBlockAlign;
                tk->fmt.audio.i_bitspersample   = p_auds->p_wf->wBitsPerSample;
                tk->fmt.b_packetized            = !tk->i_blocksize;

                avi_chunk_list_t *p_info = AVI_ChunkFind( p_riff, AVIFOURCC_INFO, 0, true );
                if( p_info )
                {
                    int i_chunk = AVIFOURCC_IAS1 + ((i - 1) << 24);
                    avi_chunk_STRING_t *p_lang = AVI_ChunkFind( p_info, i_chunk, 0, false );
                    if( p_lang != NULL && p_lang->p_str != NULL )
                        tk->fmt.psz_language = FromACP( p_lang->p_str );
                }

                msg_Dbg( p_demux,
                    "stream[%u] audio(0x%x - %s) %d channels %dHz %dbits",
                    i, p_auds->p_wf->wFormatTag,
                    vlc_fourcc_GetDescription(AUDIO_ES, tk->fmt.i_codec),
                    p_auds->p_wf->nChannels,
                    p_auds->p_wf->nSamplesPerSec,
                    p_auds->p_wf->wBitsPerSample );

                const size_t i_cboff = sizeof(WAVEFORMATEX);
                const size_t i_incboff = ( p_auds->p_wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE ) ?
                                          sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX): 0;
                if( p_auds->i_chunk_size >= i_cboff + p_auds->p_wf->cbSize &&
                    p_auds->p_wf->cbSize > i_incboff )
                {
                    int i_extra = p_auds->p_wf->cbSize - i_incboff;
                    tk->fmt.p_extra = malloc( i_extra );
                    if( unlikely(tk->fmt.p_extra == NULL) )
                    {
                        es_format_Clean( &tk->fmt );
                        free( tk );
                        goto error;
                    }
                    tk->fmt.i_extra = i_extra;
                    memcpy( tk->fmt.p_extra, ((uint8_t *)(&p_auds->p_wf[1])) + i_incboff, i_extra );
                }
                break;
            }

            case( AVIFOURCC_vids ):
            {
                if( p_vids->p_bih->biCompression == VLC_FOURCC( 'D', 'X', 'S', 'B' ) )
                {
                   msg_Dbg( p_demux, "stream[%u] subtitles", i );
                   es_format_Init( &tk->fmt, SPU_ES, p_vids->p_bih->biCompression );
                   break;
                }

                es_format_Init( &tk->fmt, VIDEO_ES,
                        AVI_FourccGetCodec( VIDEO_ES, p_vids->p_bih->biCompression ) );

                if( ParseBitmapInfoHeader( p_vids->p_bih, p_vids->i_chunk_size, &tk->fmt,
                                           &tk->bihprops ) != VLC_SUCCESS )
                {
                    es_format_Clean( &tk->fmt );
                    free( tk );
                    goto error;
                }

                if( tk->fmt.i_codec == VLC_CODEC_MP4V &&
                    !strncasecmp( (char*)&p_strh->i_handler, "XVID", 4 ) )
                {
                    tk->fmt.i_codec           =
                    tk->fmt.i_original_fourcc = VLC_FOURCC( 'X', 'V', 'I', 'D' );
                }

                if( IsQNAPCodec( tk->fmt.i_codec ) )
                    tk->fmt.b_packetized = false;

                tk->i_samplesize = 0;
                tk->fmt.video.i_frame_rate = tk->i_rate;
                tk->fmt.video.i_frame_rate_base = tk->i_scale;

                avi_chunk_vprp_t *p_vprp = AVI_ChunkFind( p_strl, AVIFOURCC_vprp, 0, false );
                if( p_vprp )
                {
                    uint32_t i_frame_aspect_ratio = p_vprp->i_frame_aspect_ratio;
                    if( p_vprp->i_video_format_token >= 1 &&
                        p_vprp->i_video_format_token <= 4 )
                        i_frame_aspect_ratio = 0x00040003;
                    tk->fmt.video.i_sar_num = ((i_frame_aspect_ratio >> 16) & 0xffff) *
                                              tk->fmt.video.i_height;
                    tk->fmt.video.i_sar_den = ((i_frame_aspect_ratio >>  0) & 0xffff) *
                                              tk->fmt.video.i_width;
                }

                msg_Dbg( p_demux, "stream[%u] video(%4.4s) %"PRIu32"x%"PRIu32" %dbpp %ffps",
                         i, (char*)&p_vids->p_bih->biCompression,
                         p_vids->p_bih->biWidth,
                         (p_vids->p_bih->biHeight <= INT_MAX) ? p_vids->p_bih->biHeight
                                                              : -1 * p_vids->p_bih->biHeight,
                         p_vids->p_bih->biBitCount,
                         (float)tk->i_rate/(float)tk->i_scale );
                break;
            }

            case( AVIFOURCC_txts):
                msg_Dbg( p_demux, "stream[%u] subtitle attachment", i );
                AVI_ExtractSubtitle( p_demux, i, p_strl, p_strn );
                free( tk );
                continue;

            case( AVIFOURCC_iavs):
            case( AVIFOURCC_ivas):
                msg_Dbg( p_demux, "stream[%u] iavs with handler %4.4s", i, (char *)&p_strh->i_handler );
                es_format_Init( &tk->fmt, VIDEO_ES, AVI_FourccGetCodec( VIDEO_ES, p_strh->i_handler ) );
                tk->i_samplesize = 0;
                tk->i_dv_audio_rate = tk->fmt.i_codec == VLC_CODEC_DV ? -1 : 0;

                tk->fmt.video.i_visible_width =
                tk->fmt.video.i_width  = p_avih->i_width;
                tk->fmt.video.i_visible_height =
                tk->fmt.video.i_height = p_avih->i_height;
                break;

            case( AVIFOURCC_mids):
                msg_Dbg( p_demux, "stream[%u] midi is UNSUPPORTED", i );
                /* fall through */

            default:
                msg_Warn( p_demux, "stream[%u] unknown type %4.4s", i, (char *)&p_strh->i_type );
                free( tk );
                continue;
        }
        tk->fmt.i_id = i;
        if( p_strn && p_strn->p_str )
            tk->fmt.psz_description = FromACP( p_strn->p_str );
        tk->p_es = es_out_Add( p_demux->out, &tk->fmt );
        TAB_APPEND( p_sys->i_track, p_sys->track, tk );
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
        if( p_sys->b_fastseekable )
        {
            AVI_IndexCreate( p_demux );
        }
        else if( p_sys->b_seekable )
        {
            AVI_IndexLoad( p_demux );
        }
        else
        {
            msg_Warn( p_demux, "cannot create index (unseekable stream)" );
        }
    }
    else if( p_sys->b_seekable )
    {
        AVI_IndexLoad( p_demux );
    }

    /* *** movie length in vlc_tick_t *** */
    p_sys->i_length = AVI_MovieGetLength( p_demux );

    /* Check the index completeness */
    unsigned int i_idx_totalframes = 0;
    for( unsigned int i = 0; i < p_sys->i_track; i++ )
    {
        const avi_track_t *tk = p_sys->track[i];
        if( tk->fmt.i_cat == VIDEO_ES && tk->idx.p_entry )
            i_idx_totalframes = __MAX(i_idx_totalframes, tk->idx.i_size);
    }
    if( i_idx_totalframes != p_avih->i_totalframes &&
        p_sys->i_length < VLC_TICK_FROM_US( p_avih->i_totalframes *
                                            p_avih->i_microsecperframe ) )
    {
        msg_Warn( p_demux, "broken or missing index, 'seek' will be "
                           "approximative or will exhibit strange behavior" );
        if( (i_do_index == 0 || i_do_index == 3) && !b_index )
        {
            if( !p_sys->b_fastseekable ) {
                b_index = true;
                goto aviindex;
            }
            if( i_do_index == 0 )
            {
                const char *psz_msg = _(
                    "Because this file index is broken or missing, "
                    "seeking will not work correctly.\n"
                    "VLC won't repair your file but can temporary fix this "
                    "problem by building an index in memory.\n"
                    "This step might take a long time on a large file.\n"
                    "What do you want to do?");
                switch( vlc_dialog_wait_question( p_demux,
                                                  VLC_DIALOG_QUESTION_NORMAL,
                                                  _("Do not play"),
                                                  _("Build index then play"),
                                                  _("Play as is"),
                                                  _("Broken or missing Index"),
                                                  "%s", psz_msg ) )
                {
                    case 0:
                        b_aborted = true;
                        goto error;
                    case 1:
                        b_index = true;
                        msg_Dbg( p_demux, "Fixing AVI index" );
                        goto aviindex;
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
    for( unsigned i = 0 ; i < p_sys->i_track; i++ )
    {
        avi_track_t         *tk = p_sys->track[i];
        avi_chunk_list_t    *p_strl;
        avi_chunk_strf_auds_t    *p_auds;

        if( tk->fmt.i_cat != AUDIO_ES )
        {
            continue;
        }
        if( tk->idx.i_size < 1 ||
            tk->i_scale != 1 ||
            tk->i_samplesize != 0 )
        {
            continue;
        }
        p_strl = AVI_ChunkFind( p_hdrl, AVIFOURCC_strl, i, true );
        p_auds = AVI_ChunkFind( p_strl, AVIFOURCC_strf, 0, false );

        if( p_auds &&
            p_auds->p_wf->wFormatTag != WAVE_FORMAT_PCM &&
            tk->i_rate == p_auds->p_wf->nSamplesPerSec )
        {
            int64_t i_track_length =
                tk->idx.p_entry[tk->idx.i_size-1].i_length +
                tk->idx.p_entry[tk->idx.i_size-1].i_lengthtotal;
            vlc_tick_t i_length = VLC_TICK_FROM_US( p_avih->i_totalframes *
                                                    p_avih->i_microsecperframe );

            if( i_length == 0 )
            {
                msg_Warn( p_demux, "track[%u] cannot be fixed (BeOS MediaKit generated)", i );
                continue;
            }
            tk->i_samplesize = 1;
            tk->i_rate       = i_track_length  * CLOCK_FREQ / i_length;
            msg_Warn( p_demux, "track[%u] fixed with rate=%u scale=%u (BeOS MediaKit generated)", i, tk->i_rate, tk->i_scale );
        }
    }

    if( p_sys->b_seekable )
    {
        /* we have read all chunk so go back to movi */
        if( vlc_stream_Seek( p_demux->s, p_movi->i_chunk_pos ) )
            goto error;
    }
    /* Skip movi header */
    if( vlc_stream_Read( p_demux->s, NULL, 12 ) < 12 )
        goto error;

    p_sys->i_movi_begin = p_movi->i_chunk_pos;
    return VLC_SUCCESS;

error:
    Close( p_this );
    return b_aborted ? VLC_ETIMEOUT : VLC_EGENERIC;
}

/*****************************************************************************
 * ReadFrame: Reads frame, using stride if necessary
 *****************************************************************************/

static block_t * ReadFrame( demux_t *p_demux, const avi_track_t *tk,
                     const unsigned int i_header, const int i_size )
{
    block_t *p_frame = vlc_stream_Block( p_demux->s, __EVEN( i_size ) );
    if ( !p_frame ) return p_frame;

    if( i_size % 2 )    /* read was padded on word boundary */
    {
        p_frame->i_buffer--;
    }

    if( i_header >= p_frame->i_buffer || tk->bihprops.i_stride > INT32_MAX - 3 )
    {
        p_frame->i_buffer = 0;
        return p_frame;
    }

    /* skip header */
    p_frame->p_buffer += i_header;
    p_frame->i_buffer -= i_header;

    const unsigned int i_stride_bytes = (tk->bihprops.i_stride + 3) & ~3;

    if ( !tk->bihprops.i_stride || !i_stride_bytes )
        return p_frame;

    if( p_frame->i_buffer < i_stride_bytes )
    {
        p_frame->i_buffer = 0;
        return p_frame;
    }

    if( !tk->bihprops.b_flipped )
    {
        const uint8_t *p_src = p_frame->p_buffer + i_stride_bytes;
        const uint8_t *p_end = p_frame->p_buffer + p_frame->i_buffer;
        uint8_t *p_dst = p_frame->p_buffer + tk->bihprops.i_stride;

        p_frame->i_buffer = tk->bihprops.i_stride;

        while ( p_src + i_stride_bytes <= p_end )
        {
            memmove( p_dst, p_src, tk->bihprops.i_stride );
            p_src += i_stride_bytes;
            p_dst += tk->bihprops.i_stride;
            p_frame->i_buffer += tk->bihprops.i_stride;
        }
    }
    else
    {
        block_t *p_flippedframe = block_Alloc( p_frame->i_buffer );
        if ( !p_flippedframe )
        {
            block_Release( p_frame );
            return NULL;
        }

        unsigned int i_lines = p_frame->i_buffer / i_stride_bytes;
        const uint8_t *p_src = p_frame->p_buffer + i_lines * i_stride_bytes;
        uint8_t *p_dst = p_flippedframe->p_buffer;

        p_flippedframe->i_buffer = 0;

        while ( i_lines-- > 0 )
        {
            p_src -= i_stride_bytes;
            memcpy( p_dst, p_src, tk->bihprops.i_stride );
            p_dst += tk->bihprops.i_stride;
            p_flippedframe->i_buffer += tk->bihprops.i_stride;
        }

        block_Release( p_frame );
        p_frame = p_flippedframe;
    }

    return p_frame;
}

/*****************************************************************************
 * SendFrame: Sends frame to ES and does payload processing
 *****************************************************************************/
static void AVI_SendFrame( demux_t *p_demux, avi_track_t *tk, block_t *p_frame )
{
    if( tk->fmt.i_cat != VIDEO_ES )
        p_frame->i_dts = p_frame->i_pts;
    else
    {
        p_frame->i_dts = p_frame->i_pts;
        p_frame->i_pts = VLC_TICK_INVALID;
    }

    if( tk->i_dv_audio_rate )
        AVI_DvHandleAudio( p_demux, tk, p_frame );

    /* Strip QNAP header */
    if( IsQNAPCodec( tk->fmt.i_codec ) )
    {
        if( p_frame->i_buffer <= QNAP_HEADER_SIZE )
        {
            block_Release( p_frame );
            return;
        }

        p_frame->i_buffer -= QNAP_HEADER_SIZE;
        p_frame->p_buffer += QNAP_HEADER_SIZE;
    }

    if( tk->i_next_block_flags )
    {
        p_frame->i_flags = tk->i_next_block_flags;
        tk->i_next_block_flags = 0;
    }

    if( tk->p_es )
        es_out_Send( p_demux->out, tk->p_es, p_frame );
    else
        block_Release( p_frame );
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

    int64_t i_toread;

    int64_t i_posf; /* where we will read :
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
        bool  b = false;

        es_out_Control( p_demux->out, ES_OUT_GET_ES_STATE, tk->p_es, &b );
        if( tk->p_es_dv_audio )
        {
            bool b_extra = false;
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
        p_sys->i_time += p_sys->i_read_increment;
        if( p_sys->i_length != 0 )
        {
            if( p_sys->i_time >= p_sys->i_length )
                return VLC_DEMUXER_EOF;
            return VLC_DEMUXER_SUCCESS;
        }
        msg_Warn( p_demux, "no track selected, exiting..." );
        return VLC_DEMUXER_EOF;
    }

    /* wait for the good time */
    es_out_SetPCR( p_demux->out, VLC_TICK_0 + p_sys->i_time );
    p_sys->i_time += p_sys->i_read_increment;

    /* init toread */
    for( i_track = 0; i_track < p_sys->i_track; i_track++ )
    {
        avi_track_t *tk = p_sys->track[i_track];

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

        vlc_tick_t i_dpts = p_sys->i_time - AVI_GetPTS( tk );

        if( tk->i_samplesize )
        {
            toread[i_track].i_toread = AVI_PTSToByte( tk, i_dpts );
        }
        else if ( i_dpts > VLC_TICK_FROM_SEC(-2) ) /* don't send a too early dts (low fps video) */
        {
            int64_t i_chunks_count = AVI_PTSToChunk( tk, i_dpts );
            if( i_dpts > 0 && AVI_GetDPTS( tk, i_chunks_count ) < i_dpts )
            {
                /* AVI code is crap. toread is either bytes, or here, chunk count.
                 * That does not even work when reading amount < scale / rate */
                i_chunks_count++;
            }
            toread[i_track].i_toread = i_chunks_count;
        }
        else
            toread[i_track].i_toread = -1;
    }

    for( ;; )
    {
        avi_track_t     *tk;
        bool       b_done;
        block_t         *p_frame;
        int64_t i_pos;
        unsigned int i;
        size_t i_size;

        /* search for first chunk to be read */
        for( i = 0, b_done = true, i_pos = -1; i < p_sys->i_track; i++ )
        {
            if( !toread[i].b_ok ||
                ( p_sys->b_fastseekable && p_sys->b_interleaved &&
                  AVI_GetDPTS( p_sys->track[i], toread[i].i_toread ) <= -p_sys->i_read_increment ) )
            {
                continue;
            }

            if( toread[i].i_toread > 0 )
            {
                b_done = false; /* not yet finished */

                if( toread[i].i_posf > 0 )
                {
                    if( i_pos == -1 || i_pos > toread[i].i_posf )
                    {
                        i_track = i;
                        i_pos = toread[i].i_posf;
                    }
                }
            }
        }

        if( b_done )
        {
            for( i = 0; i < p_sys->i_track; i++ )
            {
                if( toread[i].b_ok && toread[i].i_toread >= 0 )
                    return VLC_DEMUXER_SUCCESS;
            }
            msg_Warn( p_demux, "all tracks have failed, exiting..." );
            return VLC_DEMUXER_EOF;
        }

        if( i_pos == -1 )
        {
            unsigned short i_loop_count = 0;

            /* no valid index, we will parse directly the stream
             * in case we fail we will disable all finished stream */
            if( p_sys->b_seekable && p_sys->i_movi_lastchunk_pos >= p_sys->i_movi_begin + 12 )
            {
                if (vlc_stream_Seek(p_demux->s, p_sys->i_movi_lastchunk_pos))
                    return VLC_DEMUXER_EGENERIC;

                if( AVI_PacketNext( p_demux ) )
                {
                    return( AVI_TrackStopFinishedStreams( p_demux ) ? 0 : 1 );
                }
            }
            else
            {
                if (vlc_stream_Seek(p_demux->s, p_sys->i_movi_begin + 12))
                    return VLC_DEMUXER_EGENERIC;
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

                    if( !++i_loop_count )
                         msg_Warn( p_demux, "don't seem to find any data..." );
                    continue;
                }
                else
                {
                    i_track = avi_pk.i_stream;
                    tk = p_sys->track[i_track];

                    /* add this chunk to the index */
                    avi_entry_t index;
                    index.i_id     = avi_pk.i_fourcc;
                    index.i_flags  = AVI_GetKeyFlag(tk->fmt.i_codec, avi_pk.i_peek);
                    index.i_pos    = avi_pk.i_pos;
                    index.i_length = avi_pk.i_size;
                    index.i_lengthtotal = index.i_length;
                    avi_index_Append( &tk->idx, &p_sys->i_movi_lastchunk_pos, &index );

                    /* do we will read this data ? */
                    if( AVI_GetDPTS( tk, toread[i_track].i_toread ) > -p_sys->i_read_increment )
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
            if (vlc_stream_Seek(p_demux->s, i_pos))
                return VLC_DEMUXER_EGENERIC;
        }

        /* Set the track to use */
        tk = p_sys->track[i_track];

        /* read thoses data */
        if( tk->i_samplesize )
        {
            int64_t i_toread;

            if( ( i_toread = toread[i_track].i_toread ) <= 0 )
            {
                if( tk->i_samplesize > 1 )
                {
                    i_toread = tk->i_samplesize;
                }
                else
                {
                    i_toread = AVI_PTSToByte( tk, VLC_TICK_FROM_MS(20) );
                    i_toread = __MAX( i_toread, 100 );
                }
            }
            i_size = __MIN( tk->idx.p_entry[tk->i_idxposc].i_length -
                                tk->i_idxposb,
                            (size_t) i_toread );
        }
        else
        {
            i_size = tk->idx.p_entry[tk->i_idxposc].i_length;
        }

        if( tk->i_idxposb == 0 )
        {
            i_size += 8; /* need to read and skip header */
        }

        if( ( p_frame = ReadFrame( p_demux, tk,
                        ( tk->i_idxposb == 0 ) ? 8 : 0, i_size ) )==NULL )
        {
            msg_Warn( p_demux, "failed reading data" );
            tk->b_eof = false;
            toread[i_track].b_ok = false;
            continue;
        }

        p_frame->i_pts = VLC_TICK_0 + AVI_GetPTS( tk );
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
            if( tk->fmt.i_cat == AUDIO_ES )
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

        AVI_SendFrame( p_demux, tk, p_frame );
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

    es_out_SetPCR( p_demux->out, VLC_TICK_0 + p_sys->i_time );

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

        if( b )
        {
            if( tk->fmt.i_cat == VIDEO_ES )
            {
                p_stream_master = tk;
                break;
            }
            else if( !p_stream_master )
            {
                p_stream_master = tk;
            }
        }
    }

    if( !p_stream_master )
    {
        if( p_sys->i_track )
        {
            p_stream_master = p_sys->track[0];
        }
        else
        {
            msg_Warn( p_demux, "no more stream selected" );
            return VLC_DEMUXER_EOF;
        }
    }

    p_sys->i_time = AVI_GetPTS( p_stream_master );

    for( i_packet = 0; i_packet < 10; i_packet++)
    {
        avi_packet_t    avi_pk;

        if( AVI_PacketGetHeader( p_demux, &avi_pk ) )
        {
            return VLC_DEMUXER_EOF;
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
                    return VLC_DEMUXER_EOF;
                default:
                    msg_Warn( p_demux,
                              "seems to have lost position @%"PRIu64", resync",
                              vlc_stream_Tell(p_demux->s) );
                    if( AVI_PacketSearch( p_demux ) )
                    {
                        msg_Err( p_demux, "resync failed" );
                        return VLC_DEMUXER_EGENERIC;
                    }
            }
        }
        else
        {
            avi_track_t *p_stream = p_sys->track[avi_pk.i_stream];
            /* check for time */
            if( p_stream == p_stream_master ||
                llabs( AVI_GetPTS( p_stream ) -
                        AVI_GetPTS( p_stream_master ) )< VLC_TICK_FROM_SEC(2) )
            {
                /* load it and send to decoder */
                block_t *p_frame = ReadFrame( p_demux, p_stream, 8, avi_pk.i_size + 8 ) ;
                if( p_frame == NULL )
                {
                    return VLC_DEMUXER_EGENERIC;
                }
                p_frame->i_pts = VLC_TICK_0 + AVI_GetPTS( p_stream );

                AVI_SendFrame( p_demux, p_stream, p_frame );
            }
            else
            {
                if( AVI_PacketNext( p_demux ) )
                {
                    return VLC_DEMUXER_EOF;
                }
            }

            /* *** update stream time position *** */
            if( p_stream->i_samplesize )
            {
                p_stream->i_idxposb += avi_pk.i_size;
            }
            else
            {
                if( p_stream->fmt.i_cat == AUDIO_ES )
                {
                    p_stream->i_blockno += p_stream->i_blocksize > 0 ? ( avi_pk.i_size + p_stream->i_blocksize - 1 ) / p_stream->i_blocksize : 1;
                }
                p_stream->i_idxposc++;
            }

        }
    }

    return VLC_DEMUXER_SUCCESS;
}

/*****************************************************************************
 * Seek: goto to i_date or i_percent
 *****************************************************************************/
static int Seek( demux_t *p_demux, vlc_tick_t i_date, double f_ratio, bool b_accurate )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    msg_Dbg( p_demux, "seek requested: %"PRId64" seconds %2.2f%%",
             SEC_FROM_VLC_TICK(i_date), f_ratio * 100 );

    if( p_sys->b_seekable )
    {
        uint64_t i_pos_backup = vlc_stream_Tell( p_demux->s );

        /* Check and lazy load indexes if it was not done (not fastseekable) */
        if ( !p_sys->b_indexloaded && ( p_sys->i_avih_flags & AVIF_HASINDEX ) )
        {
            avi_chunk_t *p_riff = AVI_ChunkFind( &p_sys->ck_root, AVIFOURCC_RIFF, 0, true );
            if (unlikely( !p_riff ))
                return VLC_EGENERIC;

            int i_ret = AVI_ChunkFetchIndexes( p_demux->s, p_riff );
            if ( i_ret )
            {
                /* Go back to position before index failure */
                if (vlc_stream_Tell(p_demux->s) != i_pos_backup
                 && vlc_stream_Seek(p_demux->s, i_pos_backup))
                    return VLC_EGENERIC;

                if ( p_sys->i_avih_flags & AVIF_MUSTUSEINDEX )
                    return VLC_EGENERIC;
            }
            else AVI_IndexLoad( p_demux );

            p_sys->b_indexloaded = true; /* we don't want to try each time */
        }

        if( p_sys->i_length == 0 )
        {
            avi_track_t *p_stream = NULL;
            unsigned i_stream = 0;
            uint64_t i_pos;

            if ( !p_sys->i_movi_lastchunk_pos && /* set when index is successfully loaded */
                 ! ( p_sys->i_avih_flags & AVIF_ISINTERLEAVED ) )
            {
                msg_Err( p_demux, "seeking without index at %2.2f%%"
                         " only works for interleaved files", f_ratio * 100 );
                goto failandresetpos;
            }
            /* use i_percent to create a true i_date */
            if( f_ratio >= 1.0 )
            {
                msg_Warn( p_demux, "cannot seek so far !" );
                goto failandresetpos;
            }
            f_ratio = __MAX( f_ratio, 0 );

            /* try to find chunk that is at i_percent or the file */
            i_pos = __MAX( f_ratio * stream_Size( p_demux->s ),
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
                goto failandresetpos;
            }

            /* be sure that the index exist */
            if( AVI_StreamChunkSet( p_demux, i_stream, 0 ) )
            {
                msg_Warn( p_demux, "cannot seek" );
                goto failandresetpos;
            }

            while( i_pos >= p_stream->idx.p_entry[p_stream->i_idxposc].i_pos +
               p_stream->idx.p_entry[p_stream->i_idxposc].i_length + 8 )
            {
                /* search after i_idxposc */
                if( AVI_StreamChunkSet( p_demux,
                                        i_stream, p_stream->i_idxposc + 1 ) )
                {
                    msg_Warn( p_demux, "cannot seek" );
                    goto failandresetpos;
                }
            }

            i_date = AVI_GetPTS( p_stream );
            /* TODO better support for i_samplesize != 0 */
            msg_Dbg( p_demux, "estimate date %"PRId64, i_date );
        }

        /* */
        vlc_tick_t i_wanted = i_date;
        vlc_tick_t i_start = i_date;
        /* Do a 2 pass seek, first with video (can seek ahead due to keyframes),
           so we can seek audio to the same starting time */
        for(int i=0; i<2; i++)
        {
            for( unsigned i_stream = 0; i_stream < p_sys->i_track; i_stream++ )
            {
                avi_track_t *p_stream = p_sys->track[i_stream];

                if( !p_stream->b_activated )
                    continue;

                if( (i==0 && p_stream->fmt.i_cat != VIDEO_ES) ||
                    (i!=0 && p_stream->fmt.i_cat == VIDEO_ES) )
                    continue;

                p_stream->b_eof = AVI_TrackSeek( p_demux, i_stream, i_wanted ) != 0;
                if( !p_stream->b_eof )
                {
                    p_stream->i_next_block_flags |= BLOCK_FLAG_DISCONTINUITY;

                    if( p_stream->fmt.i_cat == AUDIO_ES || p_stream->fmt.i_cat == VIDEO_ES )
                        i_start = __MIN(i_start, AVI_GetPTS( p_stream ));

                    if( i == 0 && p_stream->fmt.i_cat == VIDEO_ES )
                        i_wanted = i_start;
                }
            }
        }
        p_sys->i_time = i_start;
        es_out_SetPCR( p_demux->out, VLC_TICK_0 + p_sys->i_time );
        if( b_accurate )
            es_out_Control( p_demux->out, ES_OUT_SET_NEXT_DISPLAY_TIME, VLC_TICK_0 + i_date );
        msg_Dbg( p_demux, "seek: %"PRId64" seconds", SEC_FROM_VLC_TICK(p_sys->i_time) );
        return VLC_SUCCESS;

failandresetpos:
        /* Go back to position before index failure */
        if ( vlc_stream_Tell( p_demux->s ) - i_pos_backup )
            vlc_stream_Seek( p_demux->s, i_pos_backup );

        return VLC_EGENERIC;
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

    if( p_sys->i_length != 0 )
    {
        return (double)p_sys->i_time / (double)p_sys->i_length;
    }
    else if( stream_Size( p_demux->s ) > 0 )
    {
        double i64 = (uint64_t)vlc_stream_Tell( p_demux->s );
        return i64 / stream_Size( p_demux->s );
    }
    return 0.0;
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    double   f, *pf;
    vlc_tick_t i64;
    bool b;
    vlc_meta_t *p_meta;

    switch( i_query )
    {
        case DEMUX_CAN_SEEK:
            *va_arg( args, bool * ) = p_sys->b_seekable;
            return VLC_SUCCESS;

        case DEMUX_GET_POSITION:
            pf = va_arg( args, double * );
            *pf = ControlGetPosition( p_demux );
            return VLC_SUCCESS;
        case DEMUX_SET_POSITION:
            f = va_arg( args, double );
            b = va_arg( args, int );
            if ( !p_sys->b_seekable )
            {
                return VLC_EGENERIC;
            }
            else
            {
                i64 = f * p_sys->i_length;
                return Seek( p_demux, i64, f, b );
            }

        case DEMUX_GET_TIME:
            *va_arg( args, vlc_tick_t * ) = p_sys->i_time;
            return VLC_SUCCESS;

        case DEMUX_SET_TIME:
        {
            f = 0;

            i64 = va_arg( args, vlc_tick_t );
            b = va_arg( args, int );
            if( !p_sys->b_seekable )
            {
                return VLC_EGENERIC;
            }
            else if( p_sys->i_length != 0 )
            {
                f = (double)i64 / p_sys->i_length;
            }
            else if( p_sys->i_time > 0 )
            {
                f = ControlGetPosition( p_demux ) *
                   (double) i64 / (double)p_sys->i_time;
            }
            return Seek( p_demux, i64, f, b );
        }
        case DEMUX_GET_LENGTH:
            *va_arg( args, vlc_tick_t * ) = p_sys->i_length;
            return VLC_SUCCESS;

        case DEMUX_GET_FPS:
            pf = va_arg( args, double * );
            *pf = 0.0;
            for( unsigned i = 0; i < p_sys->i_track; i++ )
            {
                avi_track_t *tk = p_sys->track[i];
                if( tk->fmt.i_cat == VIDEO_ES && tk->i_scale > 0)
                {
                    *pf = (float)tk->i_rate / (float)tk->i_scale;
                    break;
                }
            }
            return VLC_SUCCESS;

        case DEMUX_GET_META:
            p_meta = va_arg( args, vlc_meta_t * );
            vlc_meta_Merge( p_meta,  p_sys->meta );
            return VLC_SUCCESS;

        case DEMUX_GET_ATTACHMENTS:
        {
            if( p_sys->i_attachment <= 0 )
                return VLC_EGENERIC;

            input_attachment_t ***ppp_attach = va_arg( args, input_attachment_t*** );
            int *pi_int = va_arg( args, int * );

            *ppp_attach = calloc( p_sys->i_attachment, sizeof(**ppp_attach) );
            if( likely(*ppp_attach) )
            {
                *pi_int = p_sys->i_attachment;
                for( unsigned i = 0; i < p_sys->i_attachment; i++ )
                    (*ppp_attach)[i] = vlc_input_attachment_Duplicate( p_sys->attachment[i] );
                return VLC_SUCCESS;
            }
            return VLC_EGENERIC;
        }

        case DEMUX_CAN_PAUSE:
        case DEMUX_SET_PAUSE_STATE:
        case DEMUX_CAN_CONTROL_PACE:
        case DEMUX_GET_PTS_DELAY:
            return demux_vaControlHelper( p_demux->s, 0, -1, 0, 1, i_query, args );

        default:
            return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * Function to convert pts to chunk or byte
 *****************************************************************************/

static int64_t AVI_Rescale( vlc_tick_t i_value, uint32_t i_timescale, uint32_t i_newscale )
{
    /* TODO: replace (and mp4) with better global helper (recursive checks) */
    if( i_timescale == i_newscale )
        return i_value;

    if( (i_value >= 0 && i_value <= INT64_MAX / i_newscale) ||
        (i_value < 0  && i_value >= INT64_MIN / i_newscale) )
        return i_value * i_newscale / i_timescale;

    /* overflow */
    int64_t q = i_value / i_timescale;
    int64_t r = i_value % i_timescale;
    return q * i_newscale + r * i_newscale / i_timescale;
}

static int64_t AVI_PTSToChunk( avi_track_t *tk, vlc_tick_t i_pts )
{
    if( !tk->i_scale )
        return 0;

    i_pts = AVI_Rescale( i_pts, tk->i_scale, tk->i_rate );
    return SEC_FROM_VLC_TICK(i_pts);
}

static int64_t AVI_PTSToByte( avi_track_t *tk, vlc_tick_t i_pts )
{
    if( !tk->i_scale || !tk->i_samplesize )
        return 0;

    i_pts = AVI_Rescale( i_pts, tk->i_scale, tk->i_rate );
    return i_pts / CLOCK_FREQ * tk->i_samplesize;
}

static vlc_tick_t AVI_GetDPTS( avi_track_t *tk, int64_t i_count )
{
    vlc_tick_t i_dpts = 0;

    if( !tk->i_rate )
        return 0;

    if( !tk->i_scale )
        return 0;

    i_dpts = AVI_Rescale( CLOCK_FREQ * i_count, tk->i_rate, tk->i_scale );

    if( tk->i_samplesize )
    {
        return i_dpts / tk->i_samplesize;
    }
    return i_dpts;
}

static vlc_tick_t AVI_GetPTS( avi_track_t *tk )
{
    /* Lookup samples index */
    if( tk->i_samplesize && tk->idx.i_size )
    {
        int64_t i_count = 0;
        unsigned int idx = tk->i_idxposc;

        /* we need a valid entry we will emulate one */
        if( idx >= tk->idx.i_size )
        {
            /* use the last entry */
            idx = tk->idx.i_size - 1;
            i_count = tk->idx.p_entry[idx].i_lengthtotal
                    + tk->idx.p_entry[idx].i_length;
        }
        else
        {
            i_count = tk->idx.p_entry[idx].i_lengthtotal;
        }
        return AVI_GetDPTS( tk, i_count + tk->i_idxposb );
    }

    if( tk->fmt.i_cat == AUDIO_ES )
        return AVI_GetDPTS( tk, tk->i_blockno );
    else
        return AVI_GetDPTS( tk, tk->i_idxposc );
}

static int AVI_StreamChunkFind( demux_t *p_demux, unsigned int i_stream )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    avi_packet_t avi_pk;
    unsigned short i_loop_count = 0;

    /* find first chunk of i_stream that isn't in index */

    if( p_sys->i_movi_lastchunk_pos >= p_sys->i_movi_begin + 12 )
    {
        if (vlc_stream_Seek(p_demux->s, p_sys->i_movi_lastchunk_pos))
            return VLC_EGENERIC;
        if( AVI_PacketNext( p_demux ) )
        {
            return VLC_EGENERIC;
        }
    }
    else
    {
        if (vlc_stream_Seek(p_demux->s, p_sys->i_movi_begin + 12))
            return VLC_EGENERIC;
    }

    for( ;; )
    {
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

            if( !++i_loop_count )
                 msg_Warn( p_demux, "don't seem to find any data..." );
        }
        else
        {
            avi_track_t *tk_pk = p_sys->track[avi_pk.i_stream];

            /* add this chunk to the index */
            avi_entry_t index;
            index.i_id     = avi_pk.i_fourcc;
            index.i_flags  = AVI_GetKeyFlag(tk_pk->fmt.i_codec, avi_pk.i_peek);
            index.i_pos    = avi_pk.i_pos;
            index.i_length = avi_pk.i_size;
            index.i_lengthtotal = index.i_length;
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
                               uint64_t  i_byte )
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
                           vlc_tick_t i_date )
{
    demux_sys_t  *p_sys = p_demux->p_sys;
    avi_track_t  *tk = p_sys->track[i_stream];

#define p_stream    p_sys->track[i_stream]
    vlc_tick_t i_oldpts;

    i_oldpts = AVI_GetPTS( p_stream );

    if( !p_stream->i_samplesize )
    {
        if( AVI_StreamChunkSet( p_demux,
                                i_stream,
                                AVI_PTSToChunk( p_stream, i_date ) ) )
        {
            return VLC_EGENERIC;
        }

        if( p_stream->fmt.i_cat == AUDIO_ES )
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

        if( p_stream->fmt.i_cat == VIDEO_ES )
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
            return VLC_CODEC_UNKNOWN;
    }
}

/****************************************************************************
 *
 ****************************************************************************/
static void AVI_ParseStreamHeader( vlc_fourcc_t i_id,
                                   unsigned int *pi_number,
                                   enum es_format_category_e *pi_type )
{
    int c1, c2;

    c1 = ((uint8_t *)&i_id)[0];
    c2 = ((uint8_t *)&i_id)[1];

    if( c1 < '0' || c1 > '9' || c2 < '0' || c2 > '9' )
    {
        *pi_number =  100; /* > max stream number */
        *pi_type = UNKNOWN_ES;
    }
    else
    {
        *pi_number = (c1 - '0') * 10 + (c2 - '0' );
        switch( VLC_TWOCC( ((uint8_t *)&i_id)[2], ((uint8_t *)&i_id)[3] ) )
        {
            case AVITWOCC_wb:
                *pi_type = AUDIO_ES;
                break;
            case AVITWOCC_dc:
            case AVITWOCC_db:
            case AVITWOCC_AC:
                *pi_type = VIDEO_ES;
                break;
            case AVITWOCC_tx:
            case AVITWOCC_sb:
                *pi_type = SPU_ES;
                break;
            case AVITWOCC_pc:
                *pi_type = IGNORE_ES;
                break;
            default:
                *pi_type = UNKNOWN_ES;
                break;
        }
    }
}

/****************************************************************************
 *
 ****************************************************************************/
static int AVI_PacketGetHeader( demux_t *p_demux, avi_packet_t *p_pk )
{
    const uint8_t *p_peek;

    if( vlc_stream_Peek( p_demux->s, &p_peek, 16 ) < 16 )
    {
        return VLC_EGENERIC;
    }
    p_pk->i_fourcc  = VLC_FOURCC( p_peek[0], p_peek[1], p_peek[2], p_peek[3] );
    p_pk->i_size    = GetDWLE( p_peek + 4 );
    p_pk->i_pos     = vlc_stream_Tell( p_demux->s );
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
    size_t          i_skip = 0;

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
        if( avi_ck.i_size > UINT32_MAX - 9 )
            return VLC_EGENERIC;
        i_skip = __EVEN( avi_ck.i_size ) + 8;
    }

    if( i_skip > SSIZE_MAX )
        return VLC_EGENERIC;

    ssize_t i_ret = vlc_stream_Read( p_demux->s, NULL, i_skip );
    if( i_ret < 0 || (size_t) i_ret != i_skip )
    {
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int AVI_PacketSearch( demux_t *p_demux )
{
    demux_sys_t     *p_sys = p_demux->p_sys;
    avi_packet_t    avi_pk;
    unsigned short  i_count = 0;

    for( ;; )
    {
        if( vlc_stream_Read( p_demux->s, NULL, 1 ) != 1 )
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

        if( !++i_count )
            msg_Warn( p_demux, "trying to resync..." );
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
static void avi_index_Append( avi_index_t *p_index, uint64_t *pi_last_pos,
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

    avi_chunk_list_t *p_riff = AVI_ChunkFind( &p_sys->ck_root, AVIFOURCC_RIFF, 0, true);
    avi_chunk_idx1_t *p_idx1 = AVI_ChunkFind( p_riff, AVIFOURCC_idx1, 0, false);

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
    avi_chunk_list_t *p_movi = AVI_ChunkFind( p_riff, AVIFOURCC_movi, 0, true );
    if( !p_movi )
        return VLC_EGENERIC;
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
        if( !vlc_stream_Seek( p_demux->s, i_movi_content + i_first_pos ) &&
            vlc_stream_Peek( p_demux->s, &p_peek, 4 ) >= 4 &&
            ( !isdigit( p_peek[0] ) || !isdigit( p_peek[1] ) ||
              !isalpha( p_peek[2] ) || !isalpha( p_peek[3] ) ) )
            *pi_offset = 0;
        else
            *pi_offset = i_movi_content;

        if( p_idx1->i_entry_count )
        {
            /* Invalidate offset if index refers past the data section to avoid false
               positives when the offset equals sample size */
            size_t i_dataend = *pi_offset + p_idx1->entry[p_idx1->i_entry_count - 1].i_pos +
                                            p_idx1->entry[p_idx1->i_entry_count - 1].i_length;
            if( i_dataend > p_movi->i_chunk_pos + p_movi->i_chunk_size )
                *pi_offset = 0;
        }
    }
    else
    {
        *pi_offset = 0;
    }

    return VLC_SUCCESS;
}

static int AVI_IndexLoad_idx1( demux_t *p_demux,
                               avi_index_t p_index[], uint64_t *pi_last_offset )
{
    demux_sys_t *p_sys = p_demux->p_sys;

    avi_chunk_idx1_t *p_idx1;
    uint64_t         i_offset;
    if( AVI_IndexFind_idx1( p_demux, &p_idx1, &i_offset ) )
        return VLC_EGENERIC;

    p_sys->b_indexloaded = true;

    for( unsigned i_index = 0; i_index < p_idx1->i_entry_count; i_index++ )
    {
        enum es_format_category_e i_cat;
        unsigned i_stream;

        AVI_ParseStreamHeader( p_idx1->entry[i_index].i_fourcc,
                               &i_stream,
                               &i_cat );
        if( i_stream < p_sys->i_track &&
            (i_cat == p_sys->track[i_stream]->fmt.i_cat || i_cat == UNKNOWN_ES ) )
        {
            avi_entry_t index;
            index.i_id     = p_idx1->entry[i_index].i_fourcc;
            index.i_flags  = p_idx1->entry[i_index].i_flags&(~AVIIF_FIXKEYFRAME);
            index.i_pos    = p_idx1->entry[i_index].i_pos + i_offset;
            index.i_length = p_idx1->entry[i_index].i_length;
            index.i_lengthtotal = index.i_length;

            avi_index_Append( &p_index[i_stream], pi_last_offset, &index );
        }
    }

#ifdef AVI_DEBUG
    for( unsigned i_index = 0; i_index< p_idx1->i_entry_count && i_index < p_sys->i_track; i_index++ )
    {
        for( unsigned i = 0; i < p_index[i_index].i_size; i++ )
        {
            vlc_tick_t i_length;
            if( p_sys->track[i_index]->i_samplesize )
            {
                i_length = AVI_GetDPTS( p_sys->track[i_index],
                                        p_index[i_index].p_entry[i].i_lengthtotal );
            }
            else
            {
                i_length = AVI_GetDPTS( p_sys->track[i_index], i );
            }
            msg_Dbg( p_demux, "index stream %d @%ld time %ld", i_index,
                     p_index[i_index].p_entry[i].i_pos, i_length );
        }
    }
#endif
    return VLC_SUCCESS;
}

static void __Parse_indx( demux_t *p_demux, avi_index_t *p_index, uint64_t *pi_max_offset,
                          avi_chunk_indx_t *p_indx )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    avi_entry_t index;

    p_sys->b_indexloaded = true;

    msg_Dbg( p_demux, "loading subindex(0x%x) %d entries", p_indx->i_indextype, p_indx->i_entriesinuse );
    if( p_indx->i_indexsubtype == 0 )
    {
        for( unsigned i = 0; i < p_indx->i_entriesinuse; i++ )
        {
            index.i_id     = p_indx->i_id;
            index.i_flags  = p_indx->idx.std[i].i_size & 0x80000000 ? 0 : AVIIF_KEYFRAME;
            index.i_pos    = p_indx->i_baseoffset + p_indx->idx.std[i].i_offset - 8;
            index.i_length = p_indx->idx.std[i].i_size&0x7fffffff;
            index.i_lengthtotal = index.i_length;

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
            index.i_lengthtotal = index.i_length;

            avi_index_Append( p_index, pi_max_offset, &index );
        }
    }
    else
    {
        msg_Warn( p_demux, "unknown subtype index(0x%x)", p_indx->i_indexsubtype );
    }
}

static void AVI_IndexLoad_indx( demux_t *p_demux,
                                avi_index_t p_index[], uint64_t *pi_last_offset )
{
    demux_sys_t         *p_sys = p_demux->p_sys;

    avi_chunk_list_t    *p_riff;
    avi_chunk_list_t    *p_hdrl;

    p_riff = AVI_ChunkFind( &p_sys->ck_root, AVIFOURCC_RIFF, 0, true);
    p_hdrl = AVI_ChunkFind( p_riff, AVIFOURCC_hdrl, 0, true );

    for( unsigned i_stream = 0; i_stream < p_sys->i_track; i_stream++ )
    {
        avi_chunk_list_t    *p_strl;
        avi_chunk_indx_t    *p_indx;

#define p_stream  p_sys->track[i_stream]
        p_strl = AVI_ChunkFind( p_hdrl, AVIFOURCC_strl, i_stream, true );
        p_indx = AVI_ChunkFind( p_strl, AVIFOURCC_indx, 0, false );

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
            if ( !p_sys->b_seekable )
                return;
            avi_chunk_t    ck_sub;
            for( unsigned i = 0; i < p_indx->i_entriesinuse; i++ )
            {
                if( vlc_stream_Seek( p_demux->s,
                                     p_indx->idx.super[i].i_offset ) ||
                    AVI_ChunkRead( p_demux->s, &ck_sub, NULL  ) )
                {
                    break;
                }
                if( ck_sub.indx.i_indextype == AVI_INDEX_OF_CHUNKS )
                    __Parse_indx( p_demux, &p_index[i_stream], pi_last_offset, &ck_sub.indx );
                AVI_ChunkClean( p_demux->s, &ck_sub );
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
    uint64_t i_indx_last_pos = p_sys->i_movi_lastchunk_pos;
    uint64_t i_idx1_last_pos = p_sys->i_movi_lastchunk_pos;

    AVI_IndexLoad_indx( p_demux, p_idx_indx, &i_indx_last_pos );
    if( !p_sys->b_odml )
        AVI_IndexLoad_idx1( p_demux, p_idx_idx1, &i_idx1_last_pos );

    /* Select the longest index */
    for( unsigned i = 0; i < p_sys->i_track; i++ )
    {
        if( p_idx_indx[i].i_size > p_idx_idx1[i].i_size )
        {
            msg_Dbg( p_demux, "selected ODML index for stream[%u]", i );
            free(p_sys->track[i]->idx.p_entry);
            p_sys->track[i]->idx = p_idx_indx[i];
            avi_index_Clean( &p_idx_idx1[i] );
        }
        else
        {
            msg_Dbg( p_demux, "selected standard index for stream[%u]", i );
            free(p_sys->track[i]->idx.p_entry);
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
    uint32_t i_movi_end;

    vlc_tick_t i_dialog_update;
    vlc_dialog_id *p_dialog_id = NULL;

    p_riff = AVI_ChunkFind( &p_sys->ck_root, AVIFOURCC_RIFF, 0, true );
    p_movi = AVI_ChunkFind( p_riff, AVIFOURCC_movi, 0, true );

    if( !p_movi )
    {
        msg_Err( p_demux, "cannot find p_movi" );
        return;
    }

    for( i_stream = 0; i_stream < p_sys->i_track; i_stream++ )
        avi_index_Init( &p_sys->track[i_stream]->idx );

    i_movi_end = __MIN( (uint32_t)(p_movi->i_chunk_pos + p_movi->i_chunk_size),
                        stream_Size( p_demux->s ) );

    vlc_stream_Seek( p_demux->s, p_movi->i_chunk_pos + 12 );
    msg_Warn( p_demux, "creating index from LIST-movi, will take time !" );


    /* Only show dialog if AVI is > 10MB */
    i_dialog_update = vlc_tick_now();
    if( stream_Size( p_demux->s ) > 10000000 )
    {
        p_dialog_id =
            vlc_dialog_display_progress( p_demux, false, 0.0, _("Cancel"),
                                         _("Broken or missing AVI Index"),
                                         _("Fixing AVI Index...") );
    }

    for( ;; )
    {
        avi_packet_t pk;

        /* Don't update/check dialog too often */
        if( p_dialog_id != NULL && vlc_tick_now() - i_dialog_update > VLC_TICK_FROM_MS(100) )
        {
            if( vlc_dialog_is_cancelled( p_demux, p_dialog_id ) )
                break;

            double f_current = vlc_stream_Tell( p_demux->s );
            double f_size    = stream_Size( p_demux->s );
            double f_pos     = f_current / f_size;
            vlc_dialog_update_progress( p_demux, p_dialog_id, f_pos );

            i_dialog_update = vlc_tick_now();
        }

        if( AVI_PacketGetHeader( p_demux, &pk ) )
            break;

        if( pk.i_stream < p_sys->i_track &&
            pk.i_cat == p_sys->track[pk.i_stream]->fmt.i_cat )
        {
            avi_track_t *tk = p_sys->track[pk.i_stream];

            avi_entry_t index;
            index.i_id      = pk.i_fourcc;
            index.i_flags   = AVI_GetKeyFlag(tk->fmt.i_codec, pk.i_peek);
            index.i_pos     = pk.i_pos;
            index.i_length  = pk.i_size;
            index.i_lengthtotal = pk.i_size;
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
                                            AVIFOURCC_RIFF, 1, true );

                    msg_Dbg( p_demux, "looking for new RIFF chunk" );
                    if( !p_sysx || vlc_stream_Seek( p_demux->s,
                                         p_sysx->i_chunk_pos + 24 ) )
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
    if( p_dialog_id != NULL )
        vlc_dialog_release( p_demux, p_dialog_id );

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

    avi_chunk_list_t *p_info = AVI_ChunkFind( p_riff, AVIFOURCC_INFO, 0, true );
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
        avi_chunk_STRING_t *p_strz = AVI_ChunkFind( p_info, p_dsc[i].i_id, 0, false );
        if( !p_strz || !p_strz->p_str )
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
        avi_chunk_STRING_t *p_strz = AVI_ChunkFind( p_info, p_extra[i], 0, false );
        if( !p_strz || !p_strz->p_str )
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
    size_t i_offset = 80 * 6 + 80 * 16 * 3 + 3;
    if( p_frame->i_buffer < i_offset + 5 )
        return;

    if( p_frame->p_buffer[i_offset] != 0x50 )
        return;

    es_format_t fmt;
    dv_get_audio_format( &fmt, &p_frame->p_buffer[i_offset + 1] );

    if( tk->p_es_dv_audio && tk->i_dv_audio_rate != (int)fmt.audio.i_rate )
    {
        es_out_Del( p_demux->out, tk->p_es_dv_audio );
        tk->p_es_dv_audio = NULL;
    }

    if( !tk->p_es_dv_audio )
    {
        tk->p_es_dv_audio = es_out_Add( p_demux->out, &fmt );
        tk->i_dv_audio_rate = fmt.audio.i_rate;
    }

    es_format_Clean( &fmt );

    block_t *p_frame_audio = dv_extract_audio( p_frame );
    if( p_frame_audio )
    {
        if( tk->p_es_dv_audio )
            es_out_Send( p_demux->out, tk->p_es_dv_audio, p_frame_audio );
        else
            block_Release( p_frame_audio );
    }
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

    p_indx = AVI_ChunkFind( p_strl, AVIFOURCC_indx, 0, false );
    avi_chunk_t ck;
    int64_t  i_position;
    unsigned i_size;
    if( p_indx )
    {
        if( p_indx->i_indextype == AVI_INDEX_OF_INDEXES &&
            p_indx->i_entriesinuse > 0 )
        {
            if( vlc_stream_Seek( p_demux->s, p_indx->idx.super[0].i_offset ) ||
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
            enum es_format_category_e i_cat;
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

    if( vlc_stream_Seek( p_demux->s, i_position ) )
        goto exit;
    p_block = vlc_stream_Block( p_demux->s, i_size );
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
        psz_description = p_strn && p_strn->p_str ? FromACP( p_strn->p_str ) : NULL;
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
        AVI_ChunkClean( p_demux->s, &ck );
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
 * AVI_MovieGetLength give max streams length in ticks
 ****************************************************************************/
static vlc_tick_t  AVI_MovieGetLength( demux_t *p_demux )
{
    demux_sys_t  *p_sys = p_demux->p_sys;
    vlc_tick_t   i_maxlength = 0;
    unsigned int i;

    for( i = 0; i < p_sys->i_track; i++ )
    {
        avi_track_t *tk = p_sys->track[i];
        vlc_tick_t i_length;

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

        msg_Dbg( p_demux,
                 "stream[%d] length:%"PRId64" (based on index)",
                 i,
                 SEC_FROM_VLC_TICK(i_length) );
        i_maxlength = __MAX( i_maxlength, i_length );
    }

    return i_maxlength;
}
