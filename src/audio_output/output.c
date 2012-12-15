/*****************************************************************************
 * output.c : internal management of output streams for the audio output
 *****************************************************************************
 * Copyright (C) 2002-2004 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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
#include <vlc_aout.h>
#include <vlc_modules.h>

#include "libvlc.h"
#include "aout_internal.h"

/* Local functions */
static void aout_Destructor( vlc_object_t * p_this );

static int var_Copy (vlc_object_t *src, const char *name, vlc_value_t prev,
                     vlc_value_t value, void *data)
{
    vlc_object_t *dst = data;

    (void) src; (void) prev;
    return var_Set (dst, name, value);
}

static int aout_DeviceSelect (vlc_object_t *obj, const char *varname,
                              vlc_value_t prev, vlc_value_t value, void *data)
{
    audio_output_t *aout = (audio_output_t *)obj;
    int ret = 0; /* FIXME: default value should be -1 */

    msg_Dbg (aout, "changing device: %s -> %s", prev.psz_string,
             value.psz_string);
    aout_lock (aout);
    if (aout->device_select != NULL)
        ret = aout->device_select (aout, value.psz_string);
    aout_unlock (aout);

    (void) varname; (void) data;
    return ret ? VLC_EGENERIC : VLC_SUCCESS;
}

/**
 * Supply or update the current custom ("hardware") volume.
 * @note This only makes sense after calling aout_VolumeHardInit().
 * @param volume current custom volume
 *
 * @warning The caller (i.e. the audio output plug-in) is responsible for
 * interlocking and synchronizing call to this function and to the
 * audio_output_t.volume_set callback. This ensures that VLC gets correct
 * volume information (possibly with a latency).
 */
static void aout_VolumeNotify (audio_output_t *aout, float volume)
{
    var_SetFloat (aout, "volume", volume);
}

static void aout_MuteNotify (audio_output_t *aout, bool mute)
{
    var_SetBool (aout, "mute", mute);
}

static void aout_PolicyNotify (audio_output_t *aout, bool cork)
{
    (cork ? var_IncInteger : var_DecInteger) (aout->p_parent, "corks");
}

static void aout_DeviceNotify (audio_output_t *aout, const char *id)
{
    vlc_value_t val;

    val.psz_string = (char *)id;
    var_Change (aout, "device", VLC_VAR_SETVALUE, &val, NULL);

    /* FIXME: Remove this hack. Needed if device is not in the list. */
    char *tmp = var_GetNonEmptyString (aout, "device");
    if (tmp == NULL || strcmp (tmp, id))
    {
        var_Change (aout, "device", VLC_VAR_ADDCHOICE, &val, &val);
        var_Change (aout, "device", VLC_VAR_SETVALUE, &val, NULL);
    }
    free (tmp);
}

static int aout_GainNotify (audio_output_t *aout, float gain)
{
    aout_owner_t *owner = aout_owner (aout);

    aout_assert_locked (aout);
    aout_volume_SetVolume (owner->volume, gain);
    /* XXX: ideally, return -1 if format cannot be amplified */
    return 0;
}

#undef aout_New
/**
 * Creates an audio output object and initializes an output module.
 */
audio_output_t *aout_New (vlc_object_t *parent)
{
    vlc_value_t val, text;

    audio_output_t *aout = vlc_custom_create (parent, sizeof (aout_instance_t),
                                              "audio output");
    if (unlikely(aout == NULL))
        return NULL;

    aout_owner_t *owner = aout_owner (aout);

    vlc_mutex_init (&owner->lock);
    vlc_object_set_destructor (aout, aout_Destructor);

    /* Audio output module callbacks */
    var_Create (aout, "volume", VLC_VAR_FLOAT);
    var_AddCallback (aout, "volume", var_Copy, parent);
    var_Create (aout, "mute", VLC_VAR_BOOL | VLC_VAR_DOINHERIT);
    var_AddCallback (aout, "mute", var_Copy, parent);
    var_Create (aout, "device", VLC_VAR_STRING | VLC_VAR_HASCHOICE);
    text.psz_string = _("Audio Device");
    var_Change (aout, "device", VLC_VAR_SETTEXT, &text, NULL);
    var_AddCallback (aout, "device", aout_DeviceSelect, NULL);

    aout->event.volume_report = aout_VolumeNotify;
    aout->event.mute_report = aout_MuteNotify;
    aout->event.device_report = aout_DeviceNotify;
    aout->event.policy_report = aout_PolicyNotify;
    aout->event.gain_request = aout_GainNotify;

    /* Audio output module initialization */
    aout->start = NULL;
    aout->stop = NULL;
    aout->volume_set = NULL;
    aout->mute_set = NULL;
    aout->device_enum = NULL;
    aout->device_select = NULL;
    owner->module = module_need (aout, "audio output", "$aout", false);
    if (owner->module == NULL)
    {
        msg_Err (aout, "no suitable audio output module");
        vlc_object_release (aout);
        return NULL;
    }

    if (aout->device_enum != NULL)
    {   /* Backward compatibility with UIs using GETCHOICES/GETLIST */
        /* Note: this does NOT support hot-plugged devices. */
        char **ids, **names;
        int n = aout->device_enum (aout, &ids, &names);

        for (int i = 0; i < n; i++)
        {
            val.psz_string = ids[i];
            text.psz_string = names[i];
            var_Change (aout, "device", VLC_VAR_ADDCHOICE, &val, &text);
            free (names[i]);
            free (ids[i]);
        }
        if (n >= 0)
        {
            free (names);
            free (ids);
        }
    }

    /*
     * Persistent audio output variables
     */
    module_config_t *cfg;
    char *str;

    /* Visualizations */
    var_Create (aout, "visual", VLC_VAR_STRING | VLC_VAR_HASCHOICE);
    text.psz_string = _("Visualizations");
    var_Change (aout, "visual", VLC_VAR_SETTEXT, &text, NULL);
    val.psz_string = (char *)"";
    text.psz_string = _("Disable");
    var_Change (aout, "visual", VLC_VAR_ADDCHOICE, &val, &text);
    val.psz_string = (char *)"spectrometer";
    text.psz_string = _("Spectrometer");
    var_Change (aout, "visual", VLC_VAR_ADDCHOICE, &val, &text);
    val.psz_string = (char *)"scope";
    text.psz_string = _("Scope");
    var_Change (aout, "visual", VLC_VAR_ADDCHOICE, &val, &text);
    val.psz_string = (char *)"spectrum";
    text.psz_string = _("Spectrum");
    var_Change (aout, "visual", VLC_VAR_ADDCHOICE, &val, &text);
    val.psz_string = (char *)"vuMeter";
    text.psz_string = _("Vu meter");
    var_Change (aout, "visual", VLC_VAR_ADDCHOICE, &val, &text);
    /* Look for goom plugin */
    if (module_exists ("goom"))
    {
        val.psz_string = (char *)"goom";
        text.psz_string = (char *)"Goom";
        var_Change (aout, "visual", VLC_VAR_ADDCHOICE, &val, &text);
    }
    /* Look for libprojectM plugin */
    if (module_exists ("projectm"))
    {
        val.psz_string = (char *)"projectm";
        text.psz_string = (char*)"projectM";
        var_Change (aout, "visual", VLC_VAR_ADDCHOICE, &val, &text);
    }
    /* Look for VSXu plugin */
    if (module_exists ("vsxu"))
    {
        val.psz_string = (char *)"vsxu";
        text.psz_string = (char*)"Vovoid VSXu";
        var_Change (aout, "visual", VLC_VAR_ADDCHOICE, &val, &text);
    }
    str = var_GetNonEmptyString (aout, "effect-list");
    if (str != NULL)
    {
        var_SetString (aout, "visual", str);
        free (str);
    }

    /* Equalizer */
    var_Create (aout, "equalizer", VLC_VAR_STRING | VLC_VAR_HASCHOICE);
    text.psz_string = _("Equalizer");
    var_Change (aout, "equalizer", VLC_VAR_SETTEXT, &text, NULL);
    val.psz_string = (char*)"";
    text.psz_string = _("Disable");
    var_Change (aout, "equalizer", VLC_VAR_ADDCHOICE, &val, &text);
    cfg = config_FindConfig (VLC_OBJECT(aout), "equalizer-preset");
    if (likely(cfg != NULL))
        for (unsigned i = 0; i < cfg->list_count; i++)
        {
            val.psz_string = cfg->list.psz[i];
            text.psz_string = vlc_gettext(cfg->list_text[i]);
            var_Change (aout, "equalizer", VLC_VAR_ADDCHOICE, &val, &text);
        }

    var_Create (aout, "audio-filter", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    text.psz_string = _("Audio filters");
    var_Change (aout, "audio-filter", VLC_VAR_SETTEXT, &text, NULL);


    var_Create (aout, "audio-visual", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    text.psz_string = _("Audio visualizations");
    var_Change (aout, "audio-visual", VLC_VAR_SETTEXT, &text, NULL);

    /* Replay gain */
    var_Create (aout, "audio-replay-gain-mode",
                VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    text.psz_string = _("Replay gain");
    var_Change (aout, "audio-replay-gain-mode", VLC_VAR_SETTEXT, &text, NULL);
    cfg = config_FindConfig (VLC_OBJECT(aout), "audio-replay-gain-mode");
    if (likely(cfg != NULL))
        for (unsigned i = 0; i < cfg->list_count; i++)
        {
            val.psz_string = cfg->list.psz[i];
            text.psz_string = vlc_gettext(cfg->list_text[i]);
            var_Change (aout, "audio-replay-gain-mode", VLC_VAR_ADDCHOICE,
                            &val, &text);
        }

    return aout;
}

/**
 * Deinitializes an audio output module and destroys an audio output object.
 */
void aout_Destroy (audio_output_t *aout)
{
    aout_owner_t *owner = aout_owner (aout);

    aout_lock (aout);
    module_unneed (aout, owner->module);
    /* Protect against late call from intf.c */
    aout->volume_set = NULL;
    aout->mute_set = NULL;
    aout_unlock (aout);

    var_DelCallback (aout, "device", aout_DeviceSelect, NULL);
    var_DelCallback (aout, "mute", var_Copy, aout->p_parent);
    var_SetFloat (aout, "volume", -1.f);
    var_DelCallback (aout, "volume", var_Copy, aout->p_parent);
    vlc_object_release (aout);
}

/**
 * Destroys the audio output lock used (asynchronously) by interface functions.
 */
static void aout_Destructor (vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    aout_owner_t *owner = aout_owner (aout);

    vlc_mutex_destroy (&owner->lock);
}

/**
 * Gets the volume of the audio output stream (independent of mute).
 * \return Current audio volume (0. = silent, 1. = nominal),
 * or a strictly negative value if undefined.
 */
float aout_VolumeGet (audio_output_t *aout)
{
    return var_GetFloat (aout, "volume");
}

/**
 * Sets the volume of the audio output stream.
 * \note The mute status is not changed.
 * \return 0 on success, -1 on failure.
 */
int aout_VolumeSet (audio_output_t *aout, float vol)
{
    int ret = -1;

    aout_lock (aout);
    if (aout->volume_set != NULL)
        ret = aout->volume_set (aout, vol);
    aout_unlock (aout);
    return ret;
}

/**
 * Gets the audio output stream mute flag.
 * \return 0 if not muted, 1 if muted, -1 if undefined.
 */
int aout_MuteGet (audio_output_t *aout)
{
    return var_InheritBool (aout, "mute");
}

/**
 * Sets the audio output stream mute flag.
 * \return 0 on success, -1 on failure.
 */
int aout_MuteSet (audio_output_t *aout, bool mute)
{
    int ret = -1;

    aout_lock (aout);
    if (aout->mute_set != NULL)
        ret = aout->mute_set (aout, mute);
    aout_unlock (aout);
    return ret;
}

/**
 * Starts an audio output stream.
 * \param fmt audio output stream format [IN/OUT]
 * \warning The caller must hold the audio output lock.
 */
int aout_OutputNew (audio_output_t *aout, audio_sample_format_t *restrict fmt)
{
    aout_assert_locked (aout);

    /* Ideally, the audio filters would be created before the audio output,
     * and the ideal audio format would be the output of the filters chain.
     * But that scheme would not really play well with digital pass-through. */
    if (AOUT_FMT_LINEAR(fmt))
    {   /* Try to stay in integer domain if possible for no/slow FPU. */
        fmt->i_format = (fmt->i_bitspersample > 16) ? VLC_CODEC_FL32
                                                    : VLC_CODEC_S16N;
        aout_FormatPrepare (fmt);
    }

    if (aout->start (aout, fmt))
    {
        msg_Err (aout, "module not functional");
        return -1;
    }

    if (!var_Type (aout, "stereo-mode"))
        var_Create (aout, "stereo-mode",
                    VLC_VAR_INTEGER | VLC_VAR_HASCHOICE | VLC_VAR_DOINHERIT);

    /* The user may have selected a different channels configuration. */
    var_AddCallback (aout, "stereo-mode", aout_ChannelsRestart, NULL);
    switch (var_GetInteger (aout, "stereo-mode"))
    {
        case AOUT_VAR_CHAN_RSTEREO:
            fmt->i_original_channels |= AOUT_CHAN_REVERSESTEREO;
            break;
        case AOUT_VAR_CHAN_STEREO:
            fmt->i_original_channels = AOUT_CHANS_STEREO;
            break;
        case AOUT_VAR_CHAN_LEFT:
            fmt->i_original_channels = AOUT_CHAN_LEFT;
            break;
        case AOUT_VAR_CHAN_RIGHT:
            fmt->i_original_channels = AOUT_CHAN_RIGHT;
            break;
        case AOUT_VAR_CHAN_DOLBYS:
            fmt->i_original_channels = AOUT_CHANS_STEREO|AOUT_CHAN_DOLBYSTEREO;
            break;
        default:
        {
            if ((fmt->i_original_channels & AOUT_CHAN_PHYSMASK)
                                                          != AOUT_CHANS_STEREO)
                 break;

            vlc_value_t val, txt;
            val.i_int = 0;
            var_Change (aout, "stereo-mode", VLC_VAR_DELCHOICE, &val, NULL);
            txt.psz_string = _("Stereo audio mode");
            var_Change (aout, "stereo-mode", VLC_VAR_SETTEXT, &txt, NULL);
            if (fmt->i_original_channels & AOUT_CHAN_DOLBYSTEREO)
            {
                val.i_int = AOUT_VAR_CHAN_DOLBYS;
                txt.psz_string = _("Dolby Surround");
            }
            else
            {
                val.i_int = AOUT_VAR_CHAN_STEREO;
                txt.psz_string = _("Stereo");
            }
            var_Change (aout, "stereo-mode", VLC_VAR_ADDCHOICE, &val, &txt);
            var_Change (aout, "stereo-mode", VLC_VAR_SETVALUE, &val, NULL);
            val.i_int = AOUT_VAR_CHAN_LEFT;
            txt.psz_string = _("Left");
            var_Change (aout, "stereo-mode", VLC_VAR_ADDCHOICE, &val, &txt);
            if (fmt->i_original_channels & AOUT_CHAN_DUALMONO)
            {   /* Go directly to the left channel. */
                fmt->i_original_channels = AOUT_CHAN_LEFT;
                var_Change (aout, "stereo-mode", VLC_VAR_SETVALUE, &val, NULL);
            }
            val.i_int = AOUT_VAR_CHAN_RIGHT;
            txt.psz_string = _("Right");
            var_Change (aout, "stereo-mode", VLC_VAR_ADDCHOICE, &val, &txt);
            val.i_int = AOUT_VAR_CHAN_RSTEREO;
            txt.psz_string = _("Reverse stereo");
            var_Change (aout, "stereo-mode", VLC_VAR_ADDCHOICE, &val, &txt);
        }
    }

    aout_FormatPrepare (fmt);
    aout_FormatPrint (aout, "output", fmt);
    return 0;
}

/**
 * Stops the audio output stream (undoes aout_OutputNew()).
 * \note This can only be called after a succesful aout_OutputNew().
 * \warning The caller must hold the audio output lock.
 */
void aout_OutputDelete (audio_output_t *aout)
{
    aout_assert_locked (aout);

    var_DelCallback (aout, "stereo-mode", aout_ChannelsRestart, NULL);
    if (aout->stop != NULL)
        aout->stop (aout);
}

int aout_OutputTimeGet (audio_output_t *aout, mtime_t *delay)
{
    aout_assert_locked (aout);

    if (aout->time_get == NULL)
        return -1;
    return aout->time_get (aout, delay);
}

/**
 * Plays a decoded audio buffer.
 * \note This can only be called after a succesful aout_OutputNew().
 * \warning The caller must hold the audio output lock.
 */
void aout_OutputPlay (audio_output_t *aout, block_t *block)
{
    aout_assert_locked (aout);
    aout->play (aout, block);
}

static void PauseDefault (audio_output_t *aout, bool pause, mtime_t date)
{
    if (pause)
        aout_OutputFlush (aout, false);
    (void) date;
}

/**
 * Notifies the audio output (if any) of pause/resume events.
 * This enables the output to expedite pause, instead of waiting for its
 * buffers to drain.
 * \note This can only be called after a succesful aout_OutputNew().
 * \warning The caller must hold the audio output lock.
 */
void aout_OutputPause( audio_output_t *aout, bool pause, mtime_t date )
{
    aout_assert_locked (aout);
    ((aout->pause != NULL) ? aout->pause : PauseDefault) (aout, pause, date);
}

/**
 * Flushes or drains the audio output buffers.
 * This enables the output to expedite seek and stop.
 * \param wait if true, wait for buffer playback (i.e. drain),
 *             if false, discard the buffers immediately (i.e. flush)
 * \note This can only be called after a succesful aout_OutputNew().
 * \warning The caller must hold the audio output lock.
 */
void aout_OutputFlush( audio_output_t *aout, bool wait )
{
    aout_assert_locked( aout );
    aout->flush (aout, wait);
}
