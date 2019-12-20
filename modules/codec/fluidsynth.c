/*****************************************************************************
 * fluidsynth.c: Software MIDI synthesizer using libfluidsynth
 *****************************************************************************
 * Copyright © 2007 Rémi Denis-Courmont
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_codec.h>
#include <vlc_dialog.h>

#include <unistd.h>
#if defined( _POSIX_VERSION ) && !defined(__ANDROID__)
# include <glob.h>
#endif

/* On Win32, we link statically */
#ifdef _WIN32
# define FLUIDSYNTH_NOT_A_DLL
#endif

#ifndef HAVE_FLUIDLITE_H
#include <fluidsynth.h>
#else
#include <fluidlite.h>
#endif

#define SOUNDFONT_TEXT N_("SoundFont file")
#define SOUNDFONT_LONGTEXT N_( \
    "SoundFont file to use for software synthesis." )

#define CHORUS_TEXT N_("Chorus")

#define GAIN_TEXT N_("Synthesis gain")
#define GAIN_LONGTEXT N_("This gain is applied to synthesis output. " \
    "High values may cause saturation when many notes are played at a time." )

#define POLYPHONY_TEXT N_("Polyphony")
#define POLYPHONY_LONGTEXT N_( \
    "The polyphony defines how many voices can be played at a time. " \
    "Larger values require more processing power.")

#define REVERB_TEXT N_("Reverb")

#define SAMPLE_RATE_TEXT N_("Sample rate")

static int  Open  (vlc_object_t *);
static void Close (vlc_object_t *);

vlc_module_begin ()
    set_description (N_("FluidSynth MIDI synthesizer"))
    set_capability ("audio decoder", 100)
    set_shortname (N_("FluidSynth"))
    set_category (CAT_INPUT)
    set_subcategory (SUBCAT_INPUT_ACODEC)
    set_callbacks (Open, Close)
    add_loadfile("soundfont", "", SOUNDFONT_TEXT, SOUNDFONT_LONGTEXT)
    add_bool ("synth-chorus", true, CHORUS_TEXT, CHORUS_TEXT, false)
    add_float ("synth-gain", .5, GAIN_TEXT, GAIN_LONGTEXT, false)
        change_float_range (0., 10.)
    add_integer ("synth-polyphony", 256,
                 POLYPHONY_TEXT, POLYPHONY_LONGTEXT, false)
        change_integer_range (1, 65535)
    add_bool ("synth-reverb", true, REVERB_TEXT, REVERB_TEXT, true)
    add_integer ("synth-sample-rate", 44100,
                 SAMPLE_RATE_TEXT, SAMPLE_RATE_TEXT, true)
        change_integer_range (22050, 96000)
vlc_module_end ()


typedef struct
{
    fluid_settings_t *settings;
    fluid_synth_t    *synth;
    int               soundfont;
    date_t            end_date;
} decoder_sys_t;


static int  DecodeBlock (decoder_t *p_dec, block_t *p_block);
static void Flush (decoder_t *);

static int Open (vlc_object_t *p_this)
{
    decoder_t *p_dec = (decoder_t *)p_this;

    if (p_dec->fmt_in.i_codec != VLC_CODEC_MIDI)
        return VLC_EGENERIC;

    decoder_sys_t *p_sys = malloc (sizeof (*p_sys));
    if (unlikely(p_sys == NULL))
        return VLC_ENOMEM;

    p_sys->settings = new_fluid_settings ();
    p_sys->synth = new_fluid_synth (p_sys->settings);
    p_sys->soundfont = -1;

    char *font_path = var_InheritString (p_this, "soundfont");
    if (font_path != NULL)
    {
        msg_Dbg (p_this, "loading sound fonts file %s", font_path);
        p_sys->soundfont = fluid_synth_sfload (p_sys->synth, font_path, 1);
        if (p_sys->soundfont == -1)
            msg_Err (p_this, "cannot load sound fonts file %s", font_path);
        free (font_path);
    }
#if defined( _POSIX_VERSION ) && !defined(__ANDROID__)
    else
    {
        glob_t gl;

        glob ("/usr/share/sounds/sf2/*.sf2", GLOB_NOESCAPE, NULL, &gl);
        for (size_t i = 0; i < gl.gl_pathc; i++)
        {
            const char *path = gl.gl_pathv[i];

            msg_Dbg (p_this, "loading sound fonts file %s", path);
            p_sys->soundfont = fluid_synth_sfload (p_sys->synth, path, 1);
            if (p_sys->soundfont != -1)
                break; /* it worked! */
            msg_Err (p_this, "cannot load sound fonts file %s", path);
        }
        globfree (&gl);
    }
#endif

    if (p_sys->soundfont == -1)
    {
        msg_Err (p_this, "sound font file required for synthesis");
        vlc_dialog_display_error (p_this, _("MIDI synthesis not set up"),
            _("A sound font file (.SF2) is required for MIDI synthesis.\n"
              "Please install a sound font and configure it "
              "from the VLC preferences "
              "(Input / Codecs > Audio codecs > FluidSynth).\n"));
        delete_fluid_synth (p_sys->synth);
        delete_fluid_settings (p_sys->settings);
        free (p_sys);
        return VLC_EGENERIC;
    }

    fluid_synth_set_chorus_on (p_sys->synth,
                               var_InheritBool (p_this, "synth-chorus"));
    fluid_synth_set_gain (p_sys->synth,
                          var_InheritFloat (p_this, "synth-gain"));
    fluid_synth_set_polyphony (p_sys->synth,
                               var_InheritInteger (p_this, "synth-polyphony"));
    fluid_synth_set_reverb_on (p_sys->synth,
                               var_InheritBool (p_this, "synth-reverb"));

    p_dec->fmt_out.audio.i_rate =
        var_InheritInteger (p_this, "synth-sample-rate");;
    fluid_synth_set_sample_rate (p_sys->synth, p_dec->fmt_out.audio.i_rate);
    p_dec->fmt_out.audio.i_channels = 2;
    p_dec->fmt_out.audio.i_physical_channels = AOUT_CHAN_LEFT | AOUT_CHAN_RIGHT;
    p_dec->fmt_out.i_codec = VLC_CODEC_FL32;
    p_dec->fmt_out.audio.i_bitspersample = 32;
    date_Init (&p_sys->end_date, p_dec->fmt_out.audio.i_rate, 1);

    p_dec->p_sys = p_sys;
    p_dec->pf_decode = DecodeBlock;
    p_dec->pf_flush  = Flush;
    return VLC_SUCCESS;
}


static void Close (vlc_object_t *p_this)
{
    decoder_sys_t *p_sys = ((decoder_t *)p_this)->p_sys;

    fluid_synth_sfunload (p_sys->synth, p_sys->soundfont, 1);
    delete_fluid_synth (p_sys->synth);
    delete_fluid_settings (p_sys->settings);
    free (p_sys);
}

static void Flush (decoder_t *p_dec)
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    date_Set (&p_sys->end_date, VLC_TICK_INVALID);
    //fluid_synth_system_reset (p_sys->synth);
    fluid_synth_program_reset (p_sys->synth);
    for (unsigned channel = 0; channel < 16; channel++)
        for (unsigned note = 0; note < 128; note++)
            fluid_synth_noteoff (p_sys->synth, channel, note);
}

static int DecodeBlock (decoder_t *p_dec, block_t *p_block)
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    block_t *p_out = NULL;

    if (p_block == NULL) /* No Drain */
        return VLCDEC_SUCCESS;

    if (p_block->i_flags & (BLOCK_FLAG_DISCONTINUITY|BLOCK_FLAG_CORRUPTED))
    {
        Flush (p_dec);
        if (p_block->i_flags & BLOCK_FLAG_CORRUPTED)
        {
            block_Release(p_block);
            return VLCDEC_SUCCESS;
        }
    }

    if (p_block->i_pts != VLC_TICK_INVALID
     && date_Get(&p_sys->end_date) == VLC_TICK_INVALID)
        date_Set (&p_sys->end_date, p_block->i_pts);
    else
    if (p_block->i_pts < date_Get (&p_sys->end_date))
    {
        msg_Warn (p_dec, "MIDI message in the past?");
        goto drop;
    }

    if (p_block->i_buffer < 1)
        goto drop;

    uint8_t event = p_block->p_buffer[0];
    uint8_t channel = p_block->p_buffer[0] & 0xf;
    event &= 0xF0;

    if (event == 0xF0)
        switch (channel)
        {
            case 0:
                if (p_block->p_buffer[p_block->i_buffer - 1] != 0xF7)
                {
            case 7:
                    msg_Warn (p_dec, "fragmented SysEx not implemented");
                    goto drop;
                }
                fluid_synth_sysex (p_sys->synth, (char *)p_block->p_buffer + 1,
                                   p_block->i_buffer - 2, NULL, NULL, NULL, 0);
                break;
            case 0xF:
                fluid_synth_system_reset (p_sys->synth);
                break;
        }

    uint8_t p1 = (p_block->i_buffer > 1) ? (p_block->p_buffer[1] & 0x7f) : 0;
    uint8_t p2 = (p_block->i_buffer > 2) ? (p_block->p_buffer[2] & 0x7f) : 0;

    switch (event & 0xF0)
    {
        case 0x80:
            fluid_synth_noteoff (p_sys->synth, channel, p1);
            break;
        case 0x90:
            fluid_synth_noteon (p_sys->synth, channel, p1, p2);
            break;
#if (FLUIDSYNTH_VERSION_MAJOR >= 2)
        case 0xA0:
            fluid_synth_key_pressure (p_sys->synth, channel, p1, p2);
            break;
#endif
        case 0xB0:
            fluid_synth_cc (p_sys->synth, channel, p1, p2);
            break;
        case 0xC0:
            fluid_synth_program_change (p_sys->synth, channel, p1);
            break;
        case 0xD0:
            fluid_synth_channel_pressure (p_sys->synth, channel, p1);
            break;
        case 0xE0:
            fluid_synth_pitch_bend (p_sys->synth, channel, (p2 << 7) | p1);
            break;
    }

    unsigned samples =
        (p_block->i_pts - date_Get (&p_sys->end_date)) * 441 / 10000;
    if (samples == 0)
        goto drop;

    if (decoder_UpdateAudioFormat (p_dec))
        goto drop;
    p_out = decoder_NewAudioBuffer (p_dec, samples);
    if (p_out == NULL)
        goto drop;

    p_out->i_pts = date_Get (&p_sys->end_date );
    p_out->i_length = date_Increment (&p_sys->end_date, samples)
                      - p_out->i_pts;
    fluid_synth_write_float (p_sys->synth, samples, p_out->p_buffer, 0, 2,
                             p_out->p_buffer, 1, 2);
drop:
    block_Release (p_block);
    if (p_out != NULL)
        decoder_QueueAudio (p_dec, p_out);
    return VLCDEC_SUCCESS;
}
