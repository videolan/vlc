/*****************************************************************************
 * fluidsynth.c: Software MIDI synthetizer using libfluidsynth
 *****************************************************************************
 * Copyright © 2007 Rémi Denis-Courmont
 * $Id$
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_codec.h>

#include <fluidsynth.h>

#define SOUNDFONT_TEXT N_("Sound fonts (required)")
#define SOUNDFONT_LONGTEXT N_( \
    "A sound fonts file is required for software synthesis." )

static int  Open  (vlc_object_t *);
static void Close (vlc_object_t *);

vlc_module_begin();
    set_description (N_("FluidSynth MIDI synthetizer"));
    set_capability ("decoder", 100);
    set_category (CAT_INPUT);
    set_subcategory (SUBCAT_INPUT_ACODEC);
    set_callbacks (Open, Close);
    add_file ("soundfont", "", NULL,
              SOUNDFONT_TEXT, SOUNDFONT_LONGTEXT, false);
vlc_module_end();


struct decoder_sys_t
{
    fluid_settings_t *settings;
    fluid_synth_t    *synth;
    int               soundfont;
    audio_date_t      end_date;
};


static aout_buffer_t *DecodeBlock (decoder_t *p_dec, block_t **pp_block);


static int Open (vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys;

    if (p_dec->fmt_in.i_codec != VLC_FOURCC ('M', 'I', 'D', 'I'))
        return VLC_EGENERIC;

    char *font_path = var_CreateGetNonEmptyString (p_this, "soundfont");
    if (font_path == NULL)
    {
        msg_Err (p_this, "sound fonts file required for synthesis");
        return VLC_EGENERIC;
    }

    p_dec->fmt_out.i_cat = AUDIO_ES;
    p_dec->fmt_out.audio.i_rate = 44100;
    p_dec->fmt_out.audio.i_channels = 2;
    p_dec->fmt_out.audio.i_original_channels =
    p_dec->fmt_out.audio.i_physical_channels =
        AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
    p_dec->fmt_out.i_codec = VLC_FOURCC('f', 'l', '3', '2');
    p_dec->fmt_out.audio.i_bitspersample = 32;

    p_dec->pf_decode_audio = DecodeBlock;
    p_sys = p_dec->p_sys = malloc (sizeof (*p_sys));
    if (p_sys == NULL)
    {
        free (font_path);
        return VLC_ENOMEM;
    }

    p_sys->settings = new_fluid_settings ();
    p_sys->synth = new_fluid_synth (p_sys->settings);
    /* FIXME: I bet this is not thread-safe */
    p_sys->soundfont = fluid_synth_sfload (p_sys->synth, font_path, 1);
    free (font_path);
    if (p_sys->soundfont == -1)
    {
        msg_Err (p_this, "cannot load sound fonts file");
        Close (p_this);
        return VLC_EGENERIC;
    }

    aout_DateInit (&p_sys->end_date, p_dec->fmt_out.audio.i_rate);
    aout_DateSet (&p_sys->end_date, 0);

    return VLC_SUCCESS;
}


static void Close (vlc_object_t *p_this)
{
    decoder_sys_t *p_sys = ((decoder_t *)p_this)->p_sys;

    if (p_sys->soundfont != -1)
        fluid_synth_sfunload (p_sys->synth, p_sys->soundfont, 1);
    delete_fluid_synth (p_sys->synth);
    delete_fluid_settings (p_sys->settings);
    free (p_sys);
}


static aout_buffer_t *DecodeBlock (decoder_t *p_dec, block_t **pp_block)
{
    block_t *p_block;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if (pp_block == NULL)
        return NULL;
    p_block = *pp_block;
    if (p_block == NULL)
        return NULL;

    if (p_block->i_pts && !aout_DateGet (&p_sys->end_date))
        aout_DateSet (&p_sys->end_date, p_block->i_pts);
    else
    if (p_block->i_pts < aout_DateGet (&p_sys->end_date))
    {
        msg_Warn (p_dec, "MIDI message in the past?");
        block_Release (p_block);
        return NULL;
    }

    if (p_block->i_buffer < 1)
        return NULL;

    uint8_t channel = p_block->p_buffer[0] & 0xf;
    uint8_t p1 = (p_block->i_buffer > 1) ? (p_block->p_buffer[1] & 0x7f) : 0;
    uint8_t p2 = (p_block->i_buffer > 2) ? (p_block->p_buffer[2] & 0x7f) : 0;

    switch (p_block->p_buffer[0] & 0xf0)
    {
        case 0x80:
            fluid_synth_noteoff (p_sys->synth, channel, p1);
            break;
        case 0x90:
            fluid_synth_noteon (p_sys->synth, channel, p1, p2);
            break;
        case 0xB0:
            fluid_synth_cc (p_sys->synth, channel, p1, p2);
            break;
        case 0xC0:
            fluid_synth_program_change (p_sys->synth, channel, p1);
            break;
        case 0xE0:
            fluid_synth_pitch_bend (p_sys->synth, channel, (p1 << 7) | p2);
            break;
    }
    p_block->p_buffer += p_block->i_buffer;
    p_block->i_buffer = 0;

    unsigned samples =
        (p_block->i_pts - aout_DateGet (&p_sys->end_date)) * 441 / 10000;
    if (samples == 0)
        return NULL;

    aout_buffer_t *p_out = p_dec->pf_aout_buffer_new (p_dec, samples);
    if (p_out == NULL)
    {
        block_Release (p_block);
        return NULL;
    }

    p_out->start_date = aout_DateGet (&p_sys->end_date );
    p_out->end_date   = aout_DateIncrement (&p_sys->end_date, samples);
    fluid_synth_write_float (p_sys->synth, samples,
                             p_out->p_buffer, 0, 2,
                             p_out->p_buffer, 1, 2);
    return p_out;
}
