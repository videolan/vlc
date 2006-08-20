/*****************************************************************************
 * mono.c : stereo2mono downmixsimple channel mixer plug-in
 *****************************************************************************
 * Copyright (C) 2006 M2X
 * $Id$
 *
 * Authors: Jean-Paul Saman <jpsaman at m2x dot nl>
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
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#ifdef HAVE_STDINT_H
#   include <stdint.h>                                         /* int16_t .. */
#elif HAVE_INTTYPES_H
#   include <inttypes.h>                                       /* int16_t .. */
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include <vlc/vlc.h>
#include <vlc_es.h>
#include <vlc_block.h>
#include <vlc_filter.h>
#include <audio_output.h>
#include <aout_internal.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenFilter    ( vlc_object_t * );
static void CloseFilter   ( vlc_object_t * );

static block_t *Convert( filter_t *p_filter, block_t *p_block );
static void stereo_mono_downmix( aout_instance_t *, aout_filter_t *,
                                 aout_buffer_t *, aout_buffer_t * );
static unsigned int stereo_to_mono( int16_t *, int16_t *, unsigned int );

static void silence_channel( aout_instance_t *, aout_filter_t *,
                             aout_buffer_t *, aout_buffer_t * );

/*****************************************************************************
 * Local structures
 *****************************************************************************/
struct filter_sys_t
{
    vlc_bool_t b_block_channel;
    int i_nb_channels; /* number of float32 per sample */
    unsigned int i_channel_selected;
    int i_bitspersample;
};

#define MONO_CHANNEL_TEXT ("Select channel to keep")
#define MONO_CHANNEL_LONGTEXT ("This option silcences all other channels " \
    "except the selected channel. Choose one from (0=left, 1=right " \
    "2=rear left, 3=rear right, 4=center, 5=left front")

static int pi_pos_values[] = { 0, 1, 2, 4, 8, 5 };
static char *ppsz_pos_descriptions[] =
{ N_("Left"), N_("Right"), N_("Left rear"), N_("Right rear"), N_("Center"),
  N_("Left front") };

/* our internal channel order (WG-4 order) */
static const uint32_t pi_channels_out[] =
{ AOUT_CHAN_LEFT, AOUT_CHAN_RIGHT, AOUT_CHAN_REARLEFT, AOUT_CHAN_REARRIGHT,
  AOUT_CHAN_CENTER, AOUT_CHAN_LFE, 0 };

#define MONO_CFG "sout-"
/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Audio filter for stereo to mono conversion") );
    set_capability( "audio filter2", 5 );

    add_integer( MONO_CFG "mono-channel", 0, NULL, MONO_CHANNEL_TEXT, MONO_CHANNEL_LONGTEXT, VLC_FALSE );
        change_integer_list( pi_pos_values, ppsz_pos_descriptions, 0 );

    set_category( CAT_AUDIO );
    set_subcategory( SUBCAT_AUDIO_MISC );
    set_callbacks( OpenFilter, CloseFilter );
    set_shortname( "Mono" );
vlc_module_end();

/*****************************************************************************
 * OpenFilter
 *****************************************************************************/
static int OpenFilter( vlc_object_t *p_this )
{
    filter_t * p_filter = (filter_t *)p_this;
    filter_sys_t *p_sys = NULL;

    if( aout_FormatNbChannels( &(p_filter->fmt_in.audio) ) == 1 )
    {
        msg_Dbg( p_filter, "filter discarded (incompatible format)" );
        return VLC_EGENERIC;
    }

    if( (p_filter->fmt_in.i_codec != AOUT_FMT_S16_NE) ||
        (p_filter->fmt_out.i_codec != AOUT_FMT_S16_NE) )
    {
        msg_Err( p_this, "invalid format" );
        return -1;
    }

    if( (p_filter->fmt_in.audio.i_format != p_filter->fmt_out.audio.i_format) &&
        (p_filter->fmt_in.audio.i_rate != p_filter->fmt_out.audio.i_rate) &&
        (p_filter->fmt_in.audio.i_format != AOUT_FMT_S16_NE) &&
        (p_filter->fmt_out.audio.i_format != AOUT_FMT_S16_NE) )
    {
        msg_Err( p_this, "couldn't load mono filter" );
        return VLC_EGENERIC;
    }

    /* Allocate the memory needed to store the module's structure */
    p_sys = p_filter->p_sys = malloc( sizeof(filter_sys_t) );
    if( p_sys == NULL )
    {
        msg_Err( p_filter, "out of memory" );
        return VLC_EGENERIC;
    }

    var_Create( p_this, MONO_CFG "mono-channel",
                VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    p_sys->i_channel_selected =
            (unsigned int) var_GetInteger( p_this, MONO_CFG "mono-channel" );

    /* temporarily force channel silence */
    p_sys->b_block_channel = VLC_TRUE;
    if( p_sys->b_block_channel )
    {
        p_filter->fmt_out.audio.i_physical_channels =
                            (AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT);
    }
    else
        p_filter->fmt_out.audio.i_physical_channels = AOUT_CHAN_CENTER;

    p_filter->pf_audio_filter = Convert;
    p_filter->fmt_out.audio.i_rate = p_filter->fmt_in.audio.i_rate;
    p_filter->fmt_out.audio.i_format = p_filter->fmt_out.i_codec;

    p_sys->i_nb_channels = aout_FormatNbChannels( &(p_filter->fmt_out.audio) );
    p_sys->i_bitspersample = p_filter->fmt_out.audio.i_bitspersample;

    msg_Dbg( p_this, "%4.4s->%4.4s, channels %d->%d, bits per sample: %i->%i",
             (char *)&p_filter->fmt_in.i_codec,
             (char *)&p_filter->fmt_out.i_codec,
             p_filter->fmt_in.audio.i_physical_channels,
             p_filter->fmt_out.audio.i_physical_channels,
             p_filter->fmt_in.audio.i_bitspersample,
             p_filter->fmt_out.audio.i_bitspersample );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseFilter
 *****************************************************************************/
static void CloseFilter( vlc_object_t *p_this)
{
    filter_t *p_filter = (filter_t *) p_this;
    filter_sys_t *p_sys = p_filter->p_sys;

    var_Destroy( p_this, MONO_CFG "mono-channel" );
    free( p_sys );
}

/*****************************************************************************
 * Convert
 *****************************************************************************/
static block_t *Convert( filter_t *p_filter, block_t *p_block )
{
    aout_filter_t aout_filter;
    aout_buffer_t in_buf, out_buf;
    block_t *p_out = NULL;
    int i_out_size;

    if( !p_block || !p_block->i_samples )
    {
        if( p_block )
            p_block->pf_release( p_block );
        return NULL;
    }

    i_out_size = p_block->i_samples * p_filter->p_sys->i_bitspersample/8 *
                 p_filter->p_sys->i_nb_channels;

    p_out = p_filter->pf_audio_buffer_new( p_filter, i_out_size );
    if( !p_out )
    {
        msg_Warn( p_filter, "can't get output buffer" );
        p_block->pf_release( p_block );
        return NULL;
    }

    p_out->i_samples = p_block->i_samples;
    p_out->i_dts = p_block->i_dts;
    p_out->i_pts = p_block->i_pts;
    p_out->i_length = p_block->i_length;

    aout_filter.p_sys = (struct aout_filter_sys_t *)p_filter->p_sys;
    aout_filter.input = p_filter->fmt_in.audio;
    aout_filter.input.i_format = p_filter->fmt_in.i_codec;
    aout_filter.output = p_filter->fmt_out.audio;
    aout_filter.output.i_format = p_filter->fmt_out.i_codec;

    in_buf.p_buffer = p_block->p_buffer;
    in_buf.i_nb_bytes = p_block->i_buffer;
    in_buf.i_nb_samples = p_block->i_samples;

#if 0
    if( in_buf.i_nb_bytes != (p_filter->p_sys->i_bitspersample/8) * in_buf.i_nb_samples )
    {
        msg_Err( p_filter, "input buffer is not alligned" );
/*        if( in_buf.i_nb_bytes > (p_filter->p_sys->i_bitspersample/8) * in_buf.i_nb_samples)
            in_buf.i_nb_bytes = (p_filter->p_sys->i_bitspersample/8) * in_buf.i_nb_samples;
        else
            //in_buf*/
    }
#endif

    out_buf.p_buffer = p_out->p_buffer;
    out_buf.i_nb_bytes = p_out->i_buffer;
    out_buf.i_nb_samples = p_out->i_samples;

    stereo_mono_downmix( (aout_instance_t *)p_filter, &aout_filter, &in_buf, &out_buf );

    p_out->i_buffer = out_buf.i_nb_bytes;
    p_out->i_samples = out_buf.i_nb_samples;

    p_block->pf_release( p_block );
    return p_out;
}

static void stereo_mono_downmix( aout_instance_t * p_aout, aout_filter_t * p_filter,
                                 aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    filter_sys_t *p_sys = (filter_sys_t *)p_filter->p_sys;

    if( p_sys->b_block_channel )
    {
        silence_channel( p_aout, p_filter, p_out_buf, p_in_buf );
    }
    else
    {
        unsigned int i_samples;

        i_samples = stereo_to_mono( (int16_t *)p_out_buf->p_buffer, (int16_t *)p_in_buf->p_buffer,
                                    p_out_buf->i_nb_samples );
    }

    p_out_buf->i_nb_samples = p_in_buf->i_nb_samples;
}

/* silence_channel - play silence on all channels except the selected one.
 */
static void silence_channel( aout_instance_t * p_aout, aout_filter_t * p_filter,
                             aout_buffer_t *p_out_buf, aout_buffer_t *p_in_buf )
{
    filter_sys_t *p_sys = (filter_sys_t *)p_filter->p_sys;
    unsigned int n = 0;
    int16_t *p_in, *p_out;

    p_in = (int16_t *)p_in_buf->p_buffer;
    p_out = (int16_t *)p_out_buf->p_buffer;

    for( n = 0; n < p_in_buf->i_nb_samples * p_sys->i_nb_channels; n++ )
    {
        if( (n%p_sys->i_nb_channels) == p_sys->i_channel_selected )
        {
            p_out[n] = p_in[n];
        }
        else
        {
            p_out[n] = 0x0;
        }
    }
}

/* stereo_to_mono() function is from ffmpeg file libavcodec/resample.c 
 * Copyright (c) 2000 Fabrice Bellard.
 */
static unsigned int stereo_to_mono( int16_t *p_output, int16_t *p_input,
                                    unsigned int i_samples )
{
    int16_t *p, *q;
    unsigned int n = i_samples;

    p = p_input;
    q = p_output;

    while (n >= 4) {
        q[0] = (p[0] + p[1]) >> 1;
        q[1] = (p[2] + p[3]) >> 1;
        q[2] = (p[4] + p[5]) >> 1;
        q[3] = (p[6] + p[7]) >> 1;
        q += 4;
        p += 8;
        n -= 4;
    }
    while (n > 0) {
        q[0] = (p[0] + p[1]) >> 1;
        q++;
        p += 2;
        n--;
    }
    return n;
}
