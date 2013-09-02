/*****************************************************************************
 * vorbis.c: vorbis decoder/encoder/packetizer module using of libvorbis.
 *****************************************************************************
 * Copyright (C) 2001-2012 VLC authors and VideoLAN
 * Copyright (C) 2007 Société des arts technologiques
 * Copyright (C) 2007 Savoir-faire Linux
 *
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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
#include <vlc_codec.h>
#include <vlc_charset.h>
#include <vlc_aout.h>
#include <vlc_input.h>
#include <vlc_sout.h>
#include "../demux/xiph.h"

#include <ogg/ogg.h>

#ifdef MODULE_NAME_IS_tremor
# include <tremor/ivorbiscodec.h>
# define INTERLEAVE_TYPE int32_t

#else
# include <vorbis/codec.h>
# define INTERLEAVE_TYPE float

# ifdef ENABLE_SOUT
#  define HAVE_VORBIS_ENCODER
#  include <vorbis/vorbisenc.h>
#  ifndef OV_ECTL_RATEMANAGE_AVG
#   define OV_ECTL_RATEMANAGE_AVG 0x0
#  endif
# endif
#endif

/*****************************************************************************
 * decoder_sys_t : vorbis decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    /* Module mode */
    bool b_packetizer;

    bool            b_has_headers;

    /*
     * Vorbis properties
     */
    vorbis_info      vi; /* struct that stores all the static vorbis bitstream
                            settings */
    vorbis_comment   vc; /* struct that stores all the bitstream user
                          * comments */
    vorbis_dsp_state vd; /* central working state for the packet->PCM
                          * decoder */
    vorbis_block     vb; /* local working space for packet->PCM decode */

    /*
     * Common properties
     */
    date_t       end_date;
    int          i_last_block_size;

    /*
    ** Channel reordering
    */
    uint8_t pi_chan_table[AOUT_CHAN_MAX];
};

static const int pi_channels_maps[9] =
{
    0,
    AOUT_CHAN_CENTER,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_CENTER | AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_REARLEFT
     | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_LFE,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER
     | AOUT_CHAN_REARCENTER | AOUT_CHAN_MIDDLELEFT
     | AOUT_CHAN_MIDDLERIGHT | AOUT_CHAN_LFE,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER | AOUT_CHAN_REARLEFT
     | AOUT_CHAN_REARRIGHT | AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT
     | AOUT_CHAN_LFE,
};

/*
**  channel order as defined in http://www.xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-800004.3.9
*/

/* recommended vorbis channel order for 8 channels */
static const uint32_t pi_8channels_in[] =
{ AOUT_CHAN_LEFT, AOUT_CHAN_CENTER, AOUT_CHAN_RIGHT,
  AOUT_CHAN_MIDDLELEFT, AOUT_CHAN_MIDDLERIGHT,
  AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT,AOUT_CHAN_LFE, 0 };

/* recommended vorbis channel order for 7 channels */
static const uint32_t pi_7channels_in[] =
{ AOUT_CHAN_LEFT, AOUT_CHAN_CENTER, AOUT_CHAN_RIGHT,
  AOUT_CHAN_MIDDLELEFT, AOUT_CHAN_MIDDLERIGHT,
  AOUT_CHAN_REARCENTER, AOUT_CHAN_LFE, 0 };

/* recommended vorbis channel order for 6 channels */
static const uint32_t pi_6channels_in[] =
{ AOUT_CHAN_LEFT, AOUT_CHAN_CENTER, AOUT_CHAN_RIGHT,
  AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT, AOUT_CHAN_LFE, 0 };

/* recommended vorbis channel order for 4 channels */
static const uint32_t pi_4channels_in[] =
{ AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT, AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT, 0 };

/* recommended vorbis channel order for 3 channels */
static const uint32_t pi_3channels_in[] =
{ AOUT_CHAN_LEFT, AOUT_CHAN_CENTER, AOUT_CHAN_RIGHT, 0 };

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static int  OpenPacketizer( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );
static block_t *DecodeBlock  ( decoder_t *, block_t ** );

static int  ProcessHeaders( decoder_t * );
static void *ProcessPacket ( decoder_t *, ogg_packet *, block_t ** );

static block_t *DecodePacket( decoder_t *, ogg_packet * );
static block_t *SendPacket( decoder_t *, ogg_packet *, block_t * );

static void ParseVorbisComments( decoder_t * );

static void ConfigureChannelOrder(uint8_t *, int, uint32_t, bool );

#ifdef HAVE_VORBIS_ENCODER
static int OpenEncoder   ( vlc_object_t * );
static void CloseEncoder ( vlc_object_t * );
static block_t *Encode   ( encoder_t *, block_t * );
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ENC_QUALITY_TEXT N_("Encoding quality")
#define ENC_QUALITY_LONGTEXT N_( \
  "Enforce a quality between 1 (low) and 10 (high), instead " \
  "of specifying a particular bitrate. This will produce a VBR stream." )
#define ENC_MAXBR_TEXT N_("Maximum encoding bitrate")
#define ENC_MAXBR_LONGTEXT N_( \
  "Maximum bitrate in kbps. This is useful for streaming applications." )
#define ENC_MINBR_TEXT N_("Minimum encoding bitrate")
#define ENC_MINBR_LONGTEXT N_( \
  "Minimum bitrate in kbps. This is useful for encoding for a fixed-size channel." )
#define ENC_CBR_TEXT N_("CBR encoding")
#define ENC_CBR_LONGTEXT N_( \
  "Force a constant bitrate encoding (CBR)." )

vlc_module_begin ()
    set_shortname( "Vorbis" )
    set_description( N_("Vorbis audio decoder") )
#ifdef MODULE_NAME_IS_tremor
    set_capability( "decoder", 90 )
#else
    set_capability( "decoder", 100 )
#endif
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACODEC )
    set_callbacks( OpenDecoder, CloseDecoder )

    add_submodule ()
    set_description( N_("Vorbis audio packetizer") )
    set_capability( "packetizer", 100 )
    set_callbacks( OpenPacketizer, CloseDecoder )

#ifdef HAVE_VORBIS_ENCODER
#   define ENC_CFG_PREFIX "sout-vorbis-"
    add_submodule ()
    set_description( N_("Vorbis audio encoder") )
    set_capability( "encoder", 130 )
    set_callbacks( OpenEncoder, CloseEncoder )

    add_integer( ENC_CFG_PREFIX "quality", 0, ENC_QUALITY_TEXT,
                 ENC_QUALITY_LONGTEXT, false )
        change_integer_range( 0, 10 )
    add_integer( ENC_CFG_PREFIX "max-bitrate", 0, ENC_MAXBR_TEXT,
                 ENC_MAXBR_LONGTEXT, false )
    add_integer( ENC_CFG_PREFIX "min-bitrate", 0, ENC_MINBR_TEXT,
                 ENC_MINBR_LONGTEXT, false )
    add_bool( ENC_CFG_PREFIX "cbr", false, ENC_CBR_TEXT,
                 ENC_CBR_LONGTEXT, false )
#endif

vlc_module_end ()

#ifdef HAVE_VORBIS_ENCODER
static const char *const ppsz_enc_options[] = {
    "quality", "max-bitrate", "min-bitrate", "cbr", NULL
};
#endif

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_VORBIS )
        return VLC_EGENERIC;

    /* Allocate the memory needed to store the decoder's structure */
    p_dec->p_sys = p_sys = malloc( sizeof(*p_sys) );
    if( unlikely( !p_sys ) )
        return VLC_ENOMEM;

    /* Misc init */
    date_Set( &p_sys->end_date, 0 );
    p_sys->i_last_block_size = 0;
    p_sys->b_packetizer = false;
    p_sys->b_has_headers = false;

    /* Take care of vorbis init */
    vorbis_info_init( &p_sys->vi );
    vorbis_comment_init( &p_sys->vc );

    /* Set output properties */
    p_dec->fmt_out.i_cat = AUDIO_ES;
#ifdef MODULE_NAME_IS_tremor
    p_dec->fmt_out.i_codec = VLC_CODEC_S32N;
#else
    p_dec->fmt_out.i_codec = VLC_CODEC_FL32;
#endif

    /* Set callbacks */
    p_dec->pf_decode_audio = DecodeBlock;
    p_dec->pf_packetize    = DecodeBlock;

    return VLC_SUCCESS;
}

static int OpenPacketizer( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    int i_ret = OpenDecoder( p_this );

    if( i_ret == VLC_SUCCESS )
    {
        p_dec->p_sys->b_packetizer = true;
        p_dec->fmt_out.i_codec = VLC_CODEC_VORBIS;
    }

    return i_ret;
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * This function must be fed with ogg packets.
 ****************************************************************************/
static block_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    ogg_packet oggpacket;

    if( !pp_block ) return NULL;

    if( *pp_block )
    {
        /* Block to Ogg packet */
        oggpacket.packet = (*pp_block)->p_buffer;
        oggpacket.bytes = (*pp_block)->i_buffer;
    }
    else
    {
        if( p_sys->b_packetizer ) return NULL;

        /* Block to Ogg packet */
        oggpacket.packet = NULL;
        oggpacket.bytes = 0;
    }

    oggpacket.granulepos = -1;
    oggpacket.b_o_s = 0;
    oggpacket.e_o_s = 0;
    oggpacket.packetno = 0;

    /* Check for headers */
    if( !p_sys->b_has_headers )
    {
        if( ProcessHeaders( p_dec ) )
        {
            block_Release( *pp_block );
            return NULL;
        }
        p_sys->b_has_headers = true;
    }

    return ProcessPacket( p_dec, &oggpacket, pp_block );
}

/*****************************************************************************
 * ProcessHeaders: process Vorbis headers.
 *****************************************************************************/
static int ProcessHeaders( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    ogg_packet oggpacket;

    unsigned pi_size[XIPH_MAX_HEADER_COUNT];
    void     *pp_data[XIPH_MAX_HEADER_COUNT];
    unsigned i_count;
    if( xiph_SplitHeaders( pi_size, pp_data, &i_count,
                           p_dec->fmt_in.i_extra, p_dec->fmt_in.p_extra) )
        return VLC_EGENERIC;
    if( i_count < 3 )
        goto error;

    oggpacket.granulepos = -1;
    oggpacket.e_o_s = 0;
    oggpacket.packetno = 0;

    /* Take care of the initial Vorbis header */
    oggpacket.b_o_s  = 1; /* yes this actually is a b_o_s packet :) */
    oggpacket.bytes  = pi_size[0];
    oggpacket.packet = pp_data[0];
    if( vorbis_synthesis_headerin( &p_sys->vi, &p_sys->vc, &oggpacket ) < 0 )
    {
        msg_Err( p_dec, "this bitstream does not contain Vorbis audio data");
        goto error;
    }

    /* Setup the format */
    p_dec->fmt_out.audio.i_rate     = p_sys->vi.rate;
    p_dec->fmt_out.audio.i_channels = p_sys->vi.channels;

    if( p_dec->fmt_out.audio.i_channels > 9 )
    {
        msg_Err( p_dec, "invalid number of channels (not between 1 and 9): %i",
                 p_dec->fmt_out.audio.i_channels );
        goto error;
    }

    p_dec->fmt_out.audio.i_physical_channels =
        p_dec->fmt_out.audio.i_original_channels =
            pi_channels_maps[p_sys->vi.channels];
    p_dec->fmt_out.i_bitrate = p_sys->vi.bitrate_nominal;

    date_Init( &p_sys->end_date, p_sys->vi.rate, 1 );

    msg_Dbg( p_dec, "channels:%d samplerate:%ld bitrate:%ld",
             p_sys->vi.channels, p_sys->vi.rate, p_sys->vi.bitrate_nominal );

    /* The next packet in order is the comments header */
    oggpacket.b_o_s  = 0;
    oggpacket.bytes  = pi_size[1];
    oggpacket.packet = pp_data[1];
    if( vorbis_synthesis_headerin( &p_sys->vi, &p_sys->vc, &oggpacket ) < 0 )
    {
        msg_Err( p_dec, "2nd Vorbis header is corrupted" );
        goto error;
    }
    ParseVorbisComments( p_dec );

    /* The next packet in order is the codebooks header
     * We need to watch out that this packet is not missing as a
     * missing or corrupted header is fatal. */
    oggpacket.b_o_s  = 0;
    oggpacket.bytes  = pi_size[2];
    oggpacket.packet = pp_data[2];
    if( vorbis_synthesis_headerin( &p_sys->vi, &p_sys->vc, &oggpacket ) < 0 )
    {
        msg_Err( p_dec, "3rd Vorbis header is corrupted" );
        return VLC_EGENERIC;
    }

    if( !p_sys->b_packetizer )
    {
        /* Initialize the Vorbis packet->PCM decoder */
        vorbis_synthesis_init( &p_sys->vd, &p_sys->vi );
        vorbis_block_init( &p_sys->vd, &p_sys->vb );
    }
    else
    {
        p_dec->fmt_out.i_extra = p_dec->fmt_in.i_extra;
        p_dec->fmt_out.p_extra = xrealloc( p_dec->fmt_out.p_extra,
                                           p_dec->fmt_out.i_extra );
        memcpy( p_dec->fmt_out.p_extra,
                p_dec->fmt_in.p_extra, p_dec->fmt_out.i_extra );
    }

    ConfigureChannelOrder(p_sys->pi_chan_table, p_sys->vi.channels,
            p_dec->fmt_out.audio.i_physical_channels, true);

    for( unsigned i = 0; i < i_count; i++ )
        free( pp_data[i] );
    return VLC_SUCCESS;

error:
    for( unsigned i = 0; i < i_count; i++ )
        free( pp_data[i] );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * ProcessPacket: processes a Vorbis packet.
 *****************************************************************************/
static void *ProcessPacket( decoder_t *p_dec, ogg_packet *p_oggpacket,
                            block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_block = *pp_block;

    /* Date management */
    if( p_block && p_block->i_pts > VLC_TS_INVALID &&
        p_block->i_pts != date_Get( &p_sys->end_date ) )
    {
        date_Set( &p_sys->end_date, p_block->i_pts );
    }

    if( !date_Get( &p_sys->end_date ) )
    {
        /* We've just started the stream, wait for the first PTS. */
        if( p_block ) block_Release( p_block );
        return NULL;
    }

    *pp_block = NULL; /* To avoid being fed the same packet again */

    if( p_sys->b_packetizer )
    {
        return SendPacket( p_dec, p_oggpacket, p_block );
    }
    else
    {
        block_t *p_aout_buffer = DecodePacket( p_dec, p_oggpacket );
        if( p_block )
            block_Release( p_block );
        return p_aout_buffer;
    }
}

/*****************************************************************************
 * Interleave: helper function to interleave channels
 *****************************************************************************/
static void Interleave( INTERLEAVE_TYPE *p_out, const INTERLEAVE_TYPE **pp_in,
                        int i_nb_channels, int i_samples, uint8_t *pi_chan_table)
{
    for( int j = 0; j < i_samples; j++ )
        for( int i = 0; i < i_nb_channels; i++ )
#ifdef MODULE_NAME_IS_tremor
            p_out[j * i_nb_channels + pi_chan_table[i]] = pp_in[i][j] << 8;
#else
            p_out[j * i_nb_channels + pi_chan_table[i]] = pp_in[i][j];
#endif
}

/*****************************************************************************
 * DecodePacket: decodes a Vorbis packet.
 *****************************************************************************/
static block_t *DecodePacket( decoder_t *p_dec, ogg_packet *p_oggpacket )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int           i_samples;

    INTERLEAVE_TYPE **pp_pcm;

    if( p_oggpacket->bytes &&
        vorbis_synthesis( &p_sys->vb, p_oggpacket ) == 0 )
        vorbis_synthesis_blockin( &p_sys->vd, &p_sys->vb );

    /* **pp_pcm is a multichannel float vector. In stereo, for
     * example, pp_pcm[0] is left, and pp_pcm[1] is right. i_samples is
     * the size of each channel. Convert the float values
     * (-1.<=range<=1.) to whatever PCM format and write it out */

    if( ( i_samples = vorbis_synthesis_pcmout( &p_sys->vd, &pp_pcm ) ) > 0 )
    {

        block_t *p_aout_buffer;

        p_aout_buffer =
            decoder_NewAudioBuffer( p_dec, i_samples );

        if( p_aout_buffer == NULL ) return NULL;

        /* Interleave the samples */
        Interleave( (INTERLEAVE_TYPE*)p_aout_buffer->p_buffer,
                    (const INTERLEAVE_TYPE**)pp_pcm, p_sys->vi.channels, i_samples,
                    p_sys->pi_chan_table);

        /* Tell libvorbis how many samples we actually consumed */
        vorbis_synthesis_read( &p_sys->vd, i_samples );

        /* Date management */
        p_aout_buffer->i_pts = date_Get( &p_sys->end_date );
        p_aout_buffer->i_length = date_Increment( &p_sys->end_date,
                                           i_samples ) - p_aout_buffer->i_pts;
        return p_aout_buffer;
    }
    else
    {
        return NULL;
    }
}

/*****************************************************************************
 * SendPacket: send an ogg dated packet to the stream output.
 *****************************************************************************/
static block_t *SendPacket( decoder_t *p_dec, ogg_packet *p_oggpacket,
                            block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int i_block_size, i_samples;

    i_block_size = vorbis_packet_blocksize( &p_sys->vi, p_oggpacket );
    if( i_block_size < 0 ) i_block_size = 0; /* non audio packet */
    i_samples = ( p_sys->i_last_block_size + i_block_size ) >> 2;
    p_sys->i_last_block_size = i_block_size;

    /* Date management */
    p_block->i_dts = p_block->i_pts = date_Get( &p_sys->end_date );

    p_block->i_length = date_Increment( &p_sys->end_date, i_samples ) - p_block->i_pts;

    return p_block;
}

/*****************************************************************************
 * ParseVorbisComments
 *****************************************************************************/
static void ParseVorbisComments( decoder_t *p_dec )
{
    char *psz_name, *psz_value, *psz_comment;
    int i = 0;

    while( i < p_dec->p_sys->vc.comments )
    {
        psz_comment = strdup( p_dec->p_sys->vc.user_comments[i] );
        if( !psz_comment )
            break;
        psz_name = psz_comment;
        psz_value = strchr( psz_comment, '=' );
        /* Don't add empty values */
        if( psz_value && psz_value[1] != '\0')
        {
            *psz_value = '\0';
            psz_value++;

            if( !strcasecmp( psz_name, "REPLAYGAIN_TRACK_GAIN" ) ||
                !strcasecmp( psz_name, "RG_RADIO" ) )
            {
                audio_replay_gain_t *r = &p_dec->fmt_out.audio_replay_gain;

                r->pb_gain[AUDIO_REPLAY_GAIN_TRACK] = true;
                r->pf_gain[AUDIO_REPLAY_GAIN_TRACK] = us_atof( psz_value );
            }
            else if( !strcasecmp( psz_name, "REPLAYGAIN_TRACK_PEAK" ) ||
                     !strcasecmp( psz_name, "RG_PEAK" ) )
            {
                audio_replay_gain_t *r = &p_dec->fmt_out.audio_replay_gain;

                r->pb_peak[AUDIO_REPLAY_GAIN_TRACK] = true;
                r->pf_peak[AUDIO_REPLAY_GAIN_TRACK] = us_atof( psz_value );
            }
            else if( !strcasecmp( psz_name, "REPLAYGAIN_ALBUM_GAIN" ) ||
                     !strcasecmp( psz_name, "RG_AUDIOPHILE" ) )
            {
                audio_replay_gain_t *r = &p_dec->fmt_out.audio_replay_gain;

                r->pb_gain[AUDIO_REPLAY_GAIN_ALBUM] = true;
                r->pf_gain[AUDIO_REPLAY_GAIN_ALBUM] = us_atof( psz_value );
            }
            else if( !strcasecmp( psz_name, "REPLAYGAIN_ALBUM_PEAK" ) )
            {
                audio_replay_gain_t *r = &p_dec->fmt_out.audio_replay_gain;

                r->pb_peak[AUDIO_REPLAY_GAIN_ALBUM] = true;
                r->pf_peak[AUDIO_REPLAY_GAIN_ALBUM] = us_atof( psz_value );
            }
            else if( !strcasecmp( psz_name, "METADATA_BLOCK_PICTURE" ) )
            { /* Do nothing, for now */ }
            else
            {
                if( !p_dec->p_description )
                    p_dec->p_description = vlc_meta_New();
                if( p_dec->p_description )
                    vlc_meta_AddExtra( p_dec->p_description, psz_name, psz_value );
            }

        }
        free( psz_comment );
        i++;
    }
}

/*****************************************************************************
 *
 *****************************************************************************/
static void ConfigureChannelOrder(uint8_t *pi_chan_table, int i_channels,
                                  uint32_t i_channel_mask, bool b_decode)
{
    const uint32_t *pi_channels_in;
    switch( i_channels )
    {
        case 8:
            pi_channels_in = pi_8channels_in;
            break;
        case 7:
            pi_channels_in = pi_7channels_in;
            break;
        case 6:
        case 5:
            pi_channels_in = pi_6channels_in;
            break;
        case 4:
            pi_channels_in = pi_4channels_in;
            break;
        case 3:
            pi_channels_in = pi_3channels_in;
            break;
        default:
            {
                int i;
                for( i = 0; i< i_channels; ++i )
                {
                    pi_chan_table[i] = i;
                }
                return;
            }
    }

    if( b_decode )
        aout_CheckChannelReorder( pi_channels_in, NULL,
                                  i_channel_mask, pi_chan_table );
    else
        aout_CheckChannelReorder( NULL, pi_channels_in,
                                  i_channel_mask, pi_chan_table );
}

/*****************************************************************************
 * CloseDecoder: vorbis decoder destruction
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( !p_sys->b_packetizer && p_sys->b_has_headers )
    {
        vorbis_block_clear( &p_sys->vb );
        vorbis_dsp_clear( &p_sys->vd );
    }

    vorbis_comment_clear( &p_sys->vc );
    vorbis_info_clear( &p_sys->vi );  /* must be called last */

    free( p_sys );
}

#ifdef HAVE_VORBIS_ENCODER
/*****************************************************************************
 * encoder_sys_t : vorbis encoder descriptor
 *****************************************************************************/
struct encoder_sys_t
{
    /*
     * Vorbis properties
     */
    vorbis_info      vi; /* struct that stores all the static vorbis bitstream
                            settings */
    vorbis_comment   vc; /* struct that stores all the bitstream user
                          * comments */
    vorbis_dsp_state vd; /* central working state for the packet->PCM
                          * decoder */
    vorbis_block     vb; /* local working space for packet->PCM decode */

    int i_last_block_size;
    int i_samples_delay;
    unsigned int i_channels;

    /*
    ** Channel reordering
    */
    uint8_t pi_chan_table[AOUT_CHAN_MAX];

};

/*****************************************************************************
 * OpenEncoder: probe the encoder and return score
 *****************************************************************************/
static int OpenEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys;
    int i_quality, i_min_bitrate, i_max_bitrate;
    ogg_packet header[3];

    if( p_enc->fmt_out.i_codec != VLC_CODEC_VORBIS &&
        !p_enc->b_force )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_sys = (encoder_sys_t *)malloc(sizeof(encoder_sys_t)) ) == NULL )
        return VLC_ENOMEM;
    p_enc->p_sys = p_sys;

    p_enc->pf_encode_audio = Encode;
    p_enc->fmt_in.i_codec  = VLC_CODEC_FL32;
    p_enc->fmt_out.i_codec = VLC_CODEC_VORBIS;

    config_ChainParse( p_enc, ENC_CFG_PREFIX, ppsz_enc_options, p_enc->p_cfg );

    i_quality = var_GetInteger( p_enc, ENC_CFG_PREFIX "quality" );
    if( i_quality > 10 ) i_quality = 10;
    if( i_quality < 0 ) i_quality = 0;

    if( var_GetBool( p_enc, ENC_CFG_PREFIX "cbr" ) ) i_quality = 0;
    i_max_bitrate = var_GetInteger( p_enc, ENC_CFG_PREFIX "max-bitrate" );
    i_min_bitrate = var_GetInteger( p_enc, ENC_CFG_PREFIX "min-bitrate" );

    /* Initialize vorbis encoder */
    vorbis_info_init( &p_sys->vi );

    if( i_quality > 0 )
    {
        /* VBR mode */
        if( vorbis_encode_setup_vbr( &p_sys->vi,
              p_enc->fmt_in.audio.i_channels, p_enc->fmt_in.audio.i_rate,
              i_quality * 0.1 ) )
        {
            vorbis_info_clear( &p_sys->vi );
            free( p_enc->p_sys );
            msg_Err( p_enc, "VBR mode initialisation failed" );
            return VLC_EGENERIC;
        }

        /* Do we have optional hard quality restrictions? */
        if( i_max_bitrate > 0 || i_min_bitrate > 0 )
        {
            struct ovectl_ratemanage_arg ai;
            vorbis_encode_ctl( &p_sys->vi, OV_ECTL_RATEMANAGE_GET, &ai );

            ai.bitrate_hard_min = i_min_bitrate;
            ai.bitrate_hard_max = i_max_bitrate;
            ai.management_active = 1;

            vorbis_encode_ctl( &p_sys->vi, OV_ECTL_RATEMANAGE_SET, &ai );

        }
        else
        {
            /* Turn off management entirely */
            vorbis_encode_ctl( &p_sys->vi, OV_ECTL_RATEMANAGE_SET, NULL );
        }
    }
    else
    {
        if( vorbis_encode_setup_managed( &p_sys->vi,
              p_enc->fmt_in.audio.i_channels, p_enc->fmt_in.audio.i_rate,
              i_min_bitrate > 0 ? i_min_bitrate * 1000: -1,
              p_enc->fmt_out.i_bitrate,
              i_max_bitrate > 0 ? i_max_bitrate * 1000: -1 ) )
          {
              vorbis_info_clear( &p_sys->vi );
              msg_Err( p_enc, "CBR mode initialisation failed" );
              free( p_enc->p_sys );
              return VLC_EGENERIC;
          }
    }

    vorbis_encode_setup_init( &p_sys->vi );

    /* Add a comment */
    vorbis_comment_init( &p_sys->vc);
    vorbis_comment_add_tag( &p_sys->vc, "ENCODER", "VLC media player");

    /* Set up the analysis state and auxiliary encoding storage */
    vorbis_analysis_init( &p_sys->vd, &p_sys->vi );
    vorbis_block_init( &p_sys->vd, &p_sys->vb );

    /* Create and store headers */
    vorbis_analysis_headerout( &p_sys->vd, &p_sys->vc,
                               &header[0], &header[1], &header[2]);
    for( int i = 0; i < 3; i++ )
    {
        if( xiph_AppendHeaders( &p_enc->fmt_out.i_extra, &p_enc->fmt_out.p_extra,
                                header[i].bytes, header[i].packet ) )
        {
            p_enc->fmt_out.i_extra = 0;
            p_enc->fmt_out.p_extra = NULL;
        }
    }

    p_sys->i_channels = p_enc->fmt_in.audio.i_channels;
    p_sys->i_last_block_size = 0;
    p_sys->i_samples_delay = 0;

    ConfigureChannelOrder(p_sys->pi_chan_table, p_sys->vi.channels,
            p_enc->fmt_in.audio.i_physical_channels, true);

    return VLC_SUCCESS;
}

/****************************************************************************
 * Encode: the whole thing
 ****************************************************************************
 * This function spits out ogg packets.
 ****************************************************************************/
static block_t *Encode( encoder_t *p_enc, block_t *p_aout_buf )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    ogg_packet oggpacket;
    block_t *p_block, *p_chain = NULL;
    float **buffer;

    /* FIXME: flush buffers in here */
    if( unlikely( !p_aout_buf ) ) return NULL;

    mtime_t i_pts = p_aout_buf->i_pts -
                (mtime_t)1000000 * (mtime_t)p_sys->i_samples_delay /
                (mtime_t)p_enc->fmt_in.audio.i_rate;

    p_sys->i_samples_delay += p_aout_buf->i_nb_samples;

    buffer = vorbis_analysis_buffer( &p_sys->vd, p_aout_buf->i_nb_samples );

    /* convert samples to float and uninterleave */
    for( unsigned int i = 0; i < p_sys->i_channels; i++ )
    {
        for( unsigned int j = 0 ; j < p_aout_buf->i_nb_samples ; j++ )
        {
            buffer[i][j]= ((float *)p_aout_buf->p_buffer)
                                    [j * p_sys->i_channels + p_sys->pi_chan_table[i]];
        }
    }

    vorbis_analysis_wrote( &p_sys->vd, p_aout_buf->i_nb_samples );

    while( vorbis_analysis_blockout( &p_sys->vd, &p_sys->vb ) == 1 )
    {
        int i_samples;

        vorbis_analysis( &p_sys->vb, NULL );
        vorbis_bitrate_addblock( &p_sys->vb );

        while( vorbis_bitrate_flushpacket( &p_sys->vd, &oggpacket ) )
        {
            int i_block_size;
            p_block = block_Alloc( oggpacket.bytes );
            memcpy( p_block->p_buffer, oggpacket.packet, oggpacket.bytes );

            i_block_size = vorbis_packet_blocksize( &p_sys->vi, &oggpacket );

            if( i_block_size < 0 ) i_block_size = 0;
            i_samples = ( p_sys->i_last_block_size + i_block_size ) >> 2;
            p_sys->i_last_block_size = i_block_size;

            p_block->i_length = (mtime_t)1000000 *
                (mtime_t)i_samples / (mtime_t)p_enc->fmt_in.audio.i_rate;

            p_block->i_dts = p_block->i_pts = i_pts;

            p_sys->i_samples_delay -= i_samples;

            /* Update pts */
            i_pts += p_block->i_length;
            block_ChainAppend( &p_chain, p_block );
        }
    }

    return p_chain;
}

/*****************************************************************************
 * CloseEncoder: vorbis encoder destruction
 *****************************************************************************/
static void CloseEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys = p_enc->p_sys;

    vorbis_block_clear( &p_sys->vb );
    vorbis_dsp_clear( &p_sys->vd );
    vorbis_comment_clear( &p_sys->vc );
    vorbis_info_clear( &p_sys->vi );  /* must be called last */

    free( p_sys );
}

#endif /* HAVE_VORBIS_ENCODER */
