/*****************************************************************************
 * vorbis.c: vorbis decoder/encoder/packetizer module using of libvorbis.
 *****************************************************************************
 * Copyright (C) 2001-2003 the VideoLAN team
 * Copyright (C) 2007 Société des arts technologiques
 * Copyright (C) 2007 Savoir-faire Linux
 *
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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
#include <vlc_aout.h>
#include <vlc_input.h>
#include <vlc_playlist.h>
#include <vlc_sout.h>

#include <ogg/ogg.h>

#ifdef MODULE_NAME_IS_tremor
#include <tremor/ivorbiscodec.h>

#else
#include <vorbis/codec.h>

/* vorbis header */
#ifdef HAVE_VORBIS_VORBISENC_H
#   include <vorbis/vorbisenc.h>
#   ifndef OV_ECTL_RATEMANAGE_AVG
#       define OV_ECTL_RATEMANAGE_AVG 0x0
#   endif
#endif

#endif

/*****************************************************************************
 * decoder_sys_t : vorbis decoder descriptor
 *****************************************************************************/
struct decoder_sys_t
{
    /* Module mode */
    bool b_packetizer;

    /*
     * Input properties
     */
    int i_headers;

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
    audio_date_t end_date;
    int          i_last_block_size;

    int i_input_rate;

    /*
    ** Channel reordering
    */
    int pi_chan_table[AOUT_CHAN_MAX];
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
     | AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT | AOUT_CHAN_MIDDLELEFT
     | AOUT_CHAN_MIDDLERIGHT,
    AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT | AOUT_CHAN_CENTER | AOUT_CHAN_REARLEFT
     | AOUT_CHAN_REARRIGHT | AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT
     | AOUT_CHAN_LFE
};

/*
**  channel order as defined in http://www.ogghelp.com/ogg/glossary.cfm#Audio_Channels
*/

/* recommended vorbis channel order for 6 channels */
static const uint32_t pi_6channels_in[] =
{ AOUT_CHAN_LEFT, AOUT_CHAN_CENTER, AOUT_CHAN_RIGHT,
  AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT,AOUT_CHAN_LFE,0 };

/* recommended vorbis channel order for 4 channels */
static const uint32_t pi_4channels_in[] =
{ AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT, AOUT_CHAN_CENTER, AOUT_CHAN_LFE, 0 };

/* recommended vorbis channel order for 3 channels */
static const uint32_t pi_3channels_in[] =
{ AOUT_CHAN_LEFT, AOUT_CHAN_CENTER, AOUT_CHAN_RIGHT, 0 };

/****************************************************************************
 * Local prototypes
 ****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static int  OpenPacketizer( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );
static void *DecodeBlock  ( decoder_t *, block_t ** );

static int  ProcessHeaders( decoder_t * );
static void *ProcessPacket ( decoder_t *, ogg_packet *, block_t ** );

static aout_buffer_t *DecodePacket  ( decoder_t *, ogg_packet * );
static block_t *SendPacket( decoder_t *, ogg_packet *, block_t * );

static void ParseVorbisComments( decoder_t * );

static void ConfigureChannelOrder(int *, int, uint32_t, bool );

#ifdef MODULE_NAME_IS_tremor
static void Interleave   ( int32_t *, const int32_t **, int, int, int * );
#else
static void Interleave   ( float *, const float **, int, int, int * );
#endif

#ifndef MODULE_NAME_IS_tremor
static int OpenEncoder   ( vlc_object_t * );
static void CloseEncoder ( vlc_object_t * );
static block_t *Encode   ( encoder_t *, aout_buffer_t * );
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

vlc_module_begin();
    set_shortname( "Vorbis" );
    set_description( N_("Vorbis audio decoder") );
#ifdef MODULE_NAME_IS_tremor
    set_capability( "decoder", 90 );
#else
    set_capability( "decoder", 100 );
#endif
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACODEC );
    set_callbacks( OpenDecoder, CloseDecoder );

    add_submodule();
    set_description( N_("Vorbis audio packetizer") );
    set_capability( "packetizer", 100 );
    set_callbacks( OpenPacketizer, CloseDecoder );

#ifndef MODULE_NAME_IS_tremor
#   define ENC_CFG_PREFIX "sout-vorbis-"
    add_submodule();
    set_description( N_("Vorbis audio encoder") );
    set_capability( "encoder", 100 );
#if defined(HAVE_VORBIS_VORBISENC_H)
    set_callbacks( OpenEncoder, CloseEncoder );
#endif

    add_integer( ENC_CFG_PREFIX "quality", 0, NULL, ENC_QUALITY_TEXT,
                 ENC_QUALITY_LONGTEXT, false );
    add_integer( ENC_CFG_PREFIX "max-bitrate", 0, NULL, ENC_MAXBR_TEXT,
                 ENC_MAXBR_LONGTEXT, false );
    add_integer( ENC_CFG_PREFIX "min-bitrate", 0, NULL, ENC_MINBR_TEXT,
                 ENC_MINBR_LONGTEXT, false );
    add_bool( ENC_CFG_PREFIX "cbr", 0, NULL, ENC_CBR_TEXT,
                 ENC_CBR_LONGTEXT, false );
#endif

vlc_module_end();

#ifndef MODULE_NAME_IS_tremor
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

    if( p_dec->fmt_in.i_codec != VLC_FOURCC('v','o','r','b') )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys =
          (decoder_sys_t *)malloc(sizeof(decoder_sys_t)) ) == NULL )
        return VLC_ENOMEM;

    /* Misc init */
    aout_DateSet( &p_sys->end_date, 0 );
    p_sys->i_last_block_size = 0;
    p_sys->b_packetizer = false;
    p_sys->i_headers = 0;
    p_sys->i_input_rate = INPUT_RATE_DEFAULT;

    /* Take care of vorbis init */
    vorbis_info_init( &p_sys->vi );
    vorbis_comment_init( &p_sys->vc );

    /* Set output properties */
    p_dec->fmt_out.i_cat = AUDIO_ES;
#ifdef MODULE_NAME_IS_tremor
    p_dec->fmt_out.i_codec = VLC_FOURCC('f','i','3','2');
#else
    p_dec->fmt_out.i_codec = VLC_FOURCC('f','l','3','2');
#endif

    /* Set callbacks */
    p_dec->pf_decode_audio = (aout_buffer_t *(*)(decoder_t *, block_t **))
        DecodeBlock;
    p_dec->pf_packetize    = (block_t *(*)(decoder_t *, block_t **))
        DecodeBlock;

    return VLC_SUCCESS;
}

static int OpenPacketizer( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;

    int i_ret = OpenDecoder( p_this );

    if( i_ret == VLC_SUCCESS )
    {
        p_dec->p_sys->b_packetizer = true;
        p_dec->fmt_out.i_codec = VLC_FOURCC('v','o','r','b');
    }

    return i_ret;
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * This function must be fed with ogg packets.
 ****************************************************************************/
static void *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    ogg_packet oggpacket;

    if( !pp_block ) return NULL;

    if( *pp_block )
    {
        /* Block to Ogg packet */
        oggpacket.packet = (*pp_block)->p_buffer;
        oggpacket.bytes = (*pp_block)->i_buffer;

        if( (*pp_block)->i_rate > 0 )
            p_sys->i_input_rate = (*pp_block)->i_rate;
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
    if( p_sys->i_headers == 0 && p_dec->fmt_in.i_extra )
    {
        /* Headers already available as extra data */
        msg_Dbg( p_dec, "headers already available as extra data" );
        p_sys->i_headers = 3;
    }
    else if( oggpacket.bytes && p_sys->i_headers < 3 )
    {
        /* Backup headers as extra data */
        uint8_t *p_extra;

        p_dec->fmt_in.p_extra =
            realloc( p_dec->fmt_in.p_extra, p_dec->fmt_in.i_extra +
                     oggpacket.bytes + 2 );
        p_extra = (uint8_t *)p_dec->fmt_in.p_extra + p_dec->fmt_in.i_extra;
        *(p_extra++) = oggpacket.bytes >> 8;
        *(p_extra++) = oggpacket.bytes & 0xFF;

        memcpy( p_extra, oggpacket.packet, oggpacket.bytes );
        p_dec->fmt_in.i_extra += oggpacket.bytes + 2;

        block_Release( *pp_block );
        p_sys->i_headers++;
        return NULL;
    }

    if( p_sys->i_headers == 3 )
    {
        if( ProcessHeaders( p_dec ) != VLC_SUCCESS )
        {
            p_sys->i_headers = 0;
            p_dec->fmt_in.i_extra = 0;
            block_Release( *pp_block );
            return NULL;
        }
        else p_sys->i_headers++;
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
    uint8_t *p_extra;
    int i_extra;

    if( !p_dec->fmt_in.i_extra ) return VLC_EGENERIC;

    oggpacket.granulepos = -1;
    oggpacket.b_o_s = 1; /* yes this actually is a b_o_s packet :) */
    oggpacket.e_o_s = 0;
    oggpacket.packetno = 0;
    p_extra = p_dec->fmt_in.p_extra;
    i_extra = p_dec->fmt_in.i_extra;

    /* Take care of the initial Vorbis header */
    oggpacket.bytes = *(p_extra++) << 8;
    oggpacket.bytes |= (*(p_extra++) & 0xFF);
    oggpacket.packet = p_extra;
    p_extra += oggpacket.bytes;
    i_extra -= (oggpacket.bytes + 2);
    if( i_extra < 0 )
    {
        msg_Err( p_dec, "header data corrupted");
        return VLC_EGENERIC;
    }

    if( vorbis_synthesis_headerin( &p_sys->vi, &p_sys->vc, &oggpacket ) < 0 )
    {
        msg_Err( p_dec, "this bitstream does not contain Vorbis audio data");
        return VLC_EGENERIC;
    }

    /* Setup the format */
    p_dec->fmt_out.audio.i_rate     = p_sys->vi.rate;
    p_dec->fmt_out.audio.i_channels = p_sys->vi.channels;

    if( p_dec->fmt_out.audio.i_channels > 9 )
    {
        msg_Err( p_dec, "invalid number of channels (not between 1 and 9): %i",
                 p_dec->fmt_out.audio.i_channels );
        return VLC_EGENERIC;
    }

    p_dec->fmt_out.audio.i_physical_channels =
        p_dec->fmt_out.audio.i_original_channels =
            pi_channels_maps[p_sys->vi.channels];
    p_dec->fmt_out.i_bitrate = p_sys->vi.bitrate_nominal;

    aout_DateInit( &p_sys->end_date, p_sys->vi.rate );

    msg_Dbg( p_dec, "channels:%d samplerate:%ld bitrate:%ld",
             p_sys->vi.channels, p_sys->vi.rate, p_sys->vi.bitrate_nominal );

    /* The next packet in order is the comments header */
    oggpacket.b_o_s = 0;
    oggpacket.bytes = *(p_extra++) << 8;
    oggpacket.bytes |= (*(p_extra++) & 0xFF);
    oggpacket.packet = p_extra;
    p_extra += oggpacket.bytes;
    i_extra -= (oggpacket.bytes + 2);
    if( i_extra < 0 )
    {
        msg_Err( p_dec, "header data corrupted");
        return VLC_EGENERIC;
    }

    if( vorbis_synthesis_headerin( &p_sys->vi, &p_sys->vc, &oggpacket ) < 0 )
    {
        msg_Err( p_dec, "2nd Vorbis header is corrupted" );
        return VLC_EGENERIC;
    }
    ParseVorbisComments( p_dec );

    /* The next packet in order is the codebooks header
     * We need to watch out that this packet is not missing as a
     * missing or corrupted header is fatal. */
    oggpacket.bytes = *(p_extra++) << 8;
    oggpacket.bytes |= (*(p_extra++) & 0xFF);
    oggpacket.packet = p_extra;
    i_extra -= (oggpacket.bytes + 2);
    if( i_extra < 0 )
    {
        msg_Err( p_dec, "header data corrupted");
        return VLC_EGENERIC;
    }

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
        p_dec->fmt_out.p_extra =
            realloc( p_dec->fmt_out.p_extra, p_dec->fmt_out.i_extra );
        memcpy( p_dec->fmt_out.p_extra,
                p_dec->fmt_in.p_extra, p_dec->fmt_out.i_extra );
    }

    ConfigureChannelOrder(p_sys->pi_chan_table, p_sys->vi.channels,
            p_dec->fmt_out.audio.i_physical_channels, true);

    return VLC_SUCCESS;
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
    if( p_block && p_block->i_pts > 0 &&
        p_block->i_pts != aout_DateGet( &p_sys->end_date ) )
    {
        aout_DateSet( &p_sys->end_date, p_block->i_pts );
    }

    if( !aout_DateGet( &p_sys->end_date ) )
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
        aout_buffer_t *p_aout_buffer;

        if( p_sys->i_headers >= 3 )
            p_aout_buffer = DecodePacket( p_dec, p_oggpacket );
        else
            p_aout_buffer = NULL;

        if( p_block ) block_Release( p_block );
        return p_aout_buffer;
    }
}

/*****************************************************************************
 * DecodePacket: decodes a Vorbis packet.
 *****************************************************************************/
static aout_buffer_t *DecodePacket( decoder_t *p_dec, ogg_packet *p_oggpacket )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    int           i_samples;

#ifdef MODULE_NAME_IS_tremor
    int32_t       **pp_pcm;
#else
    float         **pp_pcm;
#endif

    if( p_oggpacket->bytes &&
#ifdef MODULE_NAME_IS_tremor
        vorbis_synthesis( &p_sys->vb, p_oggpacket, 1 ) == 0 )
#else
        vorbis_synthesis( &p_sys->vb, p_oggpacket ) == 0 )
#endif
        vorbis_synthesis_blockin( &p_sys->vd, &p_sys->vb );

    /* **pp_pcm is a multichannel float vector. In stereo, for
     * example, pp_pcm[0] is left, and pp_pcm[1] is right. i_samples is
     * the size of each channel. Convert the float values
     * (-1.<=range<=1.) to whatever PCM format and write it out */

    if( ( i_samples = vorbis_synthesis_pcmout( &p_sys->vd, &pp_pcm ) ) > 0 )
    {

        aout_buffer_t *p_aout_buffer;

        p_aout_buffer =
            p_dec->pf_aout_buffer_new( p_dec, i_samples );

        if( p_aout_buffer == NULL ) return NULL;

        /* Interleave the samples */
#ifdef MODULE_NAME_IS_tremor
        Interleave( (int32_t *)p_aout_buffer->p_buffer,
                    (const int32_t **)pp_pcm, p_sys->vi.channels, i_samples, p_sys->pi_chan_table);
#else
        Interleave( (float *)p_aout_buffer->p_buffer,
                    (const float **)pp_pcm, p_sys->vi.channels, i_samples, p_sys->pi_chan_table);
#endif

        /* Tell libvorbis how many samples we actually consumed */
        vorbis_synthesis_read( &p_sys->vd, i_samples );

        /* Date management */
        p_aout_buffer->start_date = aout_DateGet( &p_sys->end_date );
        p_aout_buffer->end_date = aout_DateIncrement( &p_sys->end_date,
                                                      i_samples * p_sys->i_input_rate / INPUT_RATE_DEFAULT );
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
    p_block->i_dts = p_block->i_pts = aout_DateGet( &p_sys->end_date );

    if( p_sys->i_headers >= 3 )
        p_block->i_length = aout_DateIncrement( &p_sys->end_date,
            i_samples * p_sys->i_input_rate / INPUT_RATE_DEFAULT ) -
                p_block->i_pts;
    else
        p_block->i_length = 0;

    return p_block;
}

/*****************************************************************************
 * ParseVorbisComments
 *****************************************************************************/
static void ParseVorbisComments( decoder_t *p_dec )
{
    input_thread_t *p_input = (input_thread_t *)p_dec->p_parent;
    char *psz_name, *psz_value, *psz_comment;
    input_item_t *p_item;
    int i = 0;

    if( p_input->i_object_type != VLC_OBJECT_INPUT ) return;

    p_item = input_GetItem( p_input );

    while( i < p_dec->p_sys->vc.comments )
    {
        psz_comment = strdup( p_dec->p_sys->vc.user_comments[i] );
        if( !psz_comment )
            break;
        psz_name = psz_comment;
        psz_value = strchr( psz_comment, '=' );
        if( psz_value )
        {
            *psz_value = '\0';
            psz_value++;
            input_Control( p_input, INPUT_ADD_INFO, _("Vorbis comment"),
                           psz_name, "%s", psz_value );
/*TODO: dot he test at the beginning and save time !! */
#ifndef HAVE_TAGLIB
            if( psz_value && ( *psz_value != '\0' ) )
            {
                if( !strcasecmp( psz_name, "artist" ) )
                    input_item_SetArtist( p_item, psz_value );
                else if( !strcasecmp( psz_name, "title" ) )
                {
                    input_item_SetTitle( p_item, psz_value );
                    p_item->psz_name = strdup( psz_value );
                }
                else if( !strcasecmp( psz_name, "album" ) )
                {
                    input_item_SetAlbum( p_item, psz_value );
                }
                else if( !strcasecmp( psz_name, "musicbrainz_trackid" ) )
                    input_item_SetTrackID( p_item, psz_value );
#if 0 //not used
                else if( !strcasecmp( psz_name, "musicbrainz_artistid" ) )
                    vlc_meta_SetArtistID( p_item, psz_value );
                else if( !strcasecmp( psz_name, "musicbrainz_albumid" ) )
                    input_item_SetAlbumID( p_item, psz_value );
#endif
            }
#endif
            if( !strcasecmp( psz_name, "REPLAYGAIN_TRACK_GAIN" ) ||
                     !strcasecmp( psz_name, "RG_RADIO" ) )
            {
                audio_replay_gain_t *r = &p_dec->fmt_out.audio_replay_gain;

                r->pb_gain[AUDIO_REPLAY_GAIN_TRACK] = true;
                r->pf_gain[AUDIO_REPLAY_GAIN_TRACK] = atof( psz_value );
            }
            else if( !strcasecmp( psz_name, "REPLAYGAIN_TRACK_PEAK" ) ||
                     !strcasecmp( psz_name, "RG_PEAK" ) )
            {
                audio_replay_gain_t *r = &p_dec->fmt_out.audio_replay_gain;

                r->pb_peak[AUDIO_REPLAY_GAIN_TRACK] = true;
                r->pf_peak[AUDIO_REPLAY_GAIN_TRACK] = atof( psz_value );
            }
            else if( !strcasecmp( psz_name, "REPLAYGAIN_ALBUM_GAIN" ) ||
                     !strcasecmp( psz_name, "RG_AUDIOPHILE" ) )
            {
                audio_replay_gain_t *r = &p_dec->fmt_out.audio_replay_gain;

                r->pb_gain[AUDIO_REPLAY_GAIN_ALBUM] = true;
                r->pf_gain[AUDIO_REPLAY_GAIN_ALBUM] = atof( psz_value );
            }
            else if( !strcasecmp( psz_name, "REPLAYGAIN_ALBUM_PEAK" ) )
            {
                audio_replay_gain_t *r = &p_dec->fmt_out.audio_replay_gain;

                r->pb_peak[AUDIO_REPLAY_GAIN_ALBUM] = true;
                r->pf_peak[AUDIO_REPLAY_GAIN_ALBUM] = atof( psz_value );
            }
        }
        var_SetInteger( pl_Yield( p_input ), "item-change", p_item->i_id );
        pl_Release( p_input );
        free( psz_comment );
        i++;
    }
}

/*****************************************************************************
 * Interleave: helper function to interleave channels
 *****************************************************************************/
static void ConfigureChannelOrder(int *pi_chan_table, int i_channels, uint32_t i_channel_mask, bool b_decode)
{
    const uint32_t *pi_channels_in;
    switch( i_channels )
    {
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
                                  i_channel_mask & AOUT_CHAN_PHYSMASK,
                                  i_channels,
                                  pi_chan_table );
    else
        aout_CheckChannelReorder( NULL, pi_channels_in,
                                  i_channel_mask & AOUT_CHAN_PHYSMASK,
                                  i_channels,
                                  pi_chan_table );
}

/*****************************************************************************
 * Interleave: helper function to interleave channels
 *****************************************************************************/
#ifdef MODULE_NAME_IS_tremor
static void Interleave( int32_t *p_out, const int32_t **pp_in,
                        int i_nb_channels, int i_samples, int *pi_chan_table)
{
    int i, j;

    for ( j = 0; j < i_samples; j++ )
        for ( i = 0; i < i_nb_channels; i++ )
            p_out[j * i_nb_channels + pi_chan_table[i]] = pp_in[i][j] * (FIXED32_ONE >> 24);
}
#else
static void Interleave( float *p_out, const float **pp_in,
                        int i_nb_channels, int i_samples, int *pi_chan_table )
{
    int i, j;

    for ( j = 0; j < i_samples; j++ )
        for ( i = 0; i < i_nb_channels; i++ )
            p_out[j * i_nb_channels + pi_chan_table[i]] = pp_in[i][j];
}
#endif

/*****************************************************************************
 * CloseDecoder: vorbis decoder destruction
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( !p_sys->b_packetizer && p_sys->i_headers > 3 )
    {
        vorbis_block_clear( &p_sys->vb );
        vorbis_dsp_clear( &p_sys->vd );
    }

    vorbis_comment_clear( &p_sys->vc );
    vorbis_info_clear( &p_sys->vi );  /* must be called last */

    free( p_sys );
}

#if defined(HAVE_VORBIS_VORBISENC_H) && !defined(MODULE_NAME_IS_tremor)

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
    int i_channels;

    /*
     * Common properties
     */
    mtime_t i_pts;

    /*
    ** Channel reordering
    */
    int pi_chan_table[AOUT_CHAN_MAX];

};

/*****************************************************************************
 * OpenEncoder: probe the encoder and return score
 *****************************************************************************/
static int OpenEncoder( vlc_object_t *p_this )
{
    encoder_t *p_enc = (encoder_t *)p_this;
    encoder_sys_t *p_sys;
    int i_quality, i_min_bitrate, i_max_bitrate, i;
    ogg_packet header[3];
    vlc_value_t val;
    uint8_t *p_extra;

    if( p_enc->fmt_out.i_codec != VLC_FOURCC('v','o','r','b') &&
        !p_enc->b_force )
    {
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_sys = (encoder_sys_t *)malloc(sizeof(encoder_sys_t)) ) == NULL )
        return VLC_ENOMEM;
    p_enc->p_sys = p_sys;

    p_enc->pf_encode_audio = Encode;
    p_enc->fmt_in.i_codec = VLC_FOURCC('f','l','3','2');
    p_enc->fmt_out.i_codec = VLC_FOURCC('v','o','r','b');

    config_ChainParse( p_enc, ENC_CFG_PREFIX, ppsz_enc_options, p_enc->p_cfg );

    var_Get( p_enc, ENC_CFG_PREFIX "quality", &val );
    i_quality = val.i_int;
    if( i_quality > 10 ) i_quality = 10;
    if( i_quality < 0 ) i_quality = 0;
    var_Get( p_enc, ENC_CFG_PREFIX "cbr", &val );
    if( val.b_bool ) i_quality = 0;
    var_Get( p_enc, ENC_CFG_PREFIX "max-bitrate", &val );
    i_max_bitrate = val.i_int;
    var_Get( p_enc, ENC_CFG_PREFIX "min-bitrate", &val );
    i_min_bitrate = val.i_int;

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
    p_enc->fmt_out.i_extra = 3 * 2 + header[0].bytes +
       header[1].bytes + header[2].bytes;
    p_extra = p_enc->fmt_out.p_extra = malloc( p_enc->fmt_out.i_extra );
    for( i = 0; i < 3; i++ )
    {
        *(p_extra++) = header[i].bytes >> 8;
        *(p_extra++) = header[i].bytes & 0xFF;
        memcpy( p_extra, header[i].packet, header[i].bytes );
        p_extra += header[i].bytes;
    }

    p_sys->i_channels = p_enc->fmt_in.audio.i_channels;
    p_sys->i_last_block_size = 0;
    p_sys->i_samples_delay = 0;
    p_sys->i_pts = 0;

    ConfigureChannelOrder(p_sys->pi_chan_table, p_sys->vi.channels,
            p_enc->fmt_in.audio.i_physical_channels, true);

    return VLC_SUCCESS;
}

/****************************************************************************
 * Encode: the whole thing
 ****************************************************************************
 * This function spits out ogg packets.
 ****************************************************************************/
static block_t *Encode( encoder_t *p_enc, aout_buffer_t *p_aout_buf )
{
    encoder_sys_t *p_sys = p_enc->p_sys;
    ogg_packet oggpacket;
    block_t *p_block, *p_chain = NULL;
    float **buffer;
    int i;
    unsigned int j;

    p_sys->i_pts = p_aout_buf->start_date -
                (mtime_t)1000000 * (mtime_t)p_sys->i_samples_delay /
                (mtime_t)p_enc->fmt_in.audio.i_rate;

    p_sys->i_samples_delay += p_aout_buf->i_nb_samples;

    buffer = vorbis_analysis_buffer( &p_sys->vd, p_aout_buf->i_nb_samples );

    /* convert samples to float and uninterleave */
    for( i = 0; i < p_sys->i_channels; i++ )
    {
        for( j = 0 ; j < p_aout_buf->i_nb_samples ; j++ )
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
            p_block = block_New( p_enc, oggpacket.bytes );
            memcpy( p_block->p_buffer, oggpacket.packet, oggpacket.bytes );

            i_block_size = vorbis_packet_blocksize( &p_sys->vi, &oggpacket );

            if( i_block_size < 0 ) i_block_size = 0;
            i_samples = ( p_sys->i_last_block_size + i_block_size ) >> 2;
            p_sys->i_last_block_size = i_block_size;

            p_block->i_length = (mtime_t)1000000 *
                (mtime_t)i_samples / (mtime_t)p_enc->fmt_in.audio.i_rate;

            p_block->i_dts = p_block->i_pts = p_sys->i_pts;

            p_sys->i_samples_delay -= i_samples;

            /* Update pts */
            p_sys->i_pts += p_block->i_length;
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

#endif /* HAVE_VORBIS_VORBISENC_H && !MODULE_NAME_IS_tremor */
