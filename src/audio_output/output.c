/*****************************************************************************
 * output.c : internal management of output streams for the audio output
 *****************************************************************************
 * Copyright (C) 2002-2004 VLC authors and VideoLAN
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

#include <stdlib.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_configuration.h>
#include <vlc_aout.h>
#include <vlc_modules.h>
#include <vlc_atomic.h>

#include "libvlc.h"
#include "aout_internal.h"

typedef struct aout_dev
{
    struct vlc_list node;
    char *name;
    char id[1];
} aout_dev_t;


/* Local functions */

static inline float clampf(const float value, const float min, const float max)
{
    if (value < min)
        return min;
    else if (value > max)
        return max;
    else
        return value;
}

static int var_Copy (vlc_object_t *src, const char *name, vlc_value_t prev,
                     vlc_value_t value, void *data)
{
    vlc_object_t *dst = data;

    (void) src; (void) prev;
    return var_Set (dst, name, value);
}

static int var_CopyDevice (vlc_object_t *src, const char *name,
                           vlc_value_t prev, vlc_value_t value, void *data)
{
    vlc_object_t *dst = data;

    (void) src; (void) name; (void) prev;
    return var_Set (dst, "audio-device", value);
}

static void aout_TimingNotify(audio_output_t *aout, vlc_tick_t system_ts,
                              vlc_tick_t audio_ts)
{
    aout_owner_t *owner = aout_owner (aout);
    assert(owner->main_stream);
    vlc_aout_stream_NotifyTiming(owner->main_stream, system_ts, audio_ts);
}

static void aout_DrainedNotify(audio_output_t *aout)
{
    aout_owner_t *owner = aout_owner (aout);
    assert(owner->main_stream);
    vlc_aout_stream_NotifyDrained(owner->main_stream);
}

/**
 * Supply or update the current custom ("hardware") volume.
 *
 * @param aout the audio output notifying the new volume
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
    (cork ? var_IncInteger : var_DecInteger)(vlc_object_parent(aout), "corks");
}

static void aout_DeviceNotify (audio_output_t *aout, const char *id)
{
    var_SetString (aout, "device", (id != NULL) ? id : "");
}

static void aout_HotplugNotify (audio_output_t *aout,
                                const char *id, const char *name)
{
    aout_owner_t *owner = aout_owner (aout);
    aout_dev_t *dev = NULL, *p;

    vlc_mutex_lock (&owner->dev.lock);
    vlc_list_foreach(p, &owner->dev.list, node)
    {
        if (!strcmp (id, p->id))
        {
            dev = p;
            break;
        }
    }

    if (name != NULL)
    {
        if (dev == NULL) /* Added device */
        {
            dev = malloc (sizeof (*dev) + strlen (id));
            if (unlikely(dev == NULL))
                goto out;
            strcpy (dev->id, id);
            vlc_list_append(&dev->node, &owner->dev.list);
            owner->dev.count++;
        }
        else /* Modified device */
            free (dev->name);
        dev->name = strdup (name);
    }
    else
    {
        if (dev != NULL) /* Removed device */
        {
            owner->dev.count--;
            vlc_list_remove(&dev->node);
            free (dev->name);
            free (dev);
        }
    }
out:
    vlc_mutex_unlock (&owner->dev.lock);
}

static void aout_RestartNotify (audio_output_t *aout, unsigned mode)
{
    aout_owner_t *owner = aout_owner (aout);
    if (owner->main_stream)
        vlc_aout_stream_RequestRestart(owner->main_stream, mode);
}

void aout_InputRequestRestart(audio_output_t *aout)
{
    aout_RestartNotify(aout, AOUT_RESTART_FILTERS);
}

static int aout_GainNotify (audio_output_t *aout, float gain)
{
    aout_owner_t *owner = aout_owner (aout);

    vlc_mutex_assert(&owner->lock);
    /* XXX: ideally, return -1 if format cannot be amplified */
    if (owner->main_stream != NULL)
        vlc_aout_stream_NotifyGain(owner->main_stream, gain);
    return 0;
}

static const struct vlc_audio_output_events aout_events = {
    aout_TimingNotify,
    aout_DrainedNotify,
    aout_VolumeNotify,
    aout_MuteNotify,
    aout_PolicyNotify,
    aout_DeviceNotify,
    aout_HotplugNotify,
    aout_RestartNotify,
    aout_GainNotify,
};

static int FilterCallback (vlc_object_t *obj, const char *var,
                           vlc_value_t prev, vlc_value_t cur, void *data)
{
    if (strcmp(prev.psz_string, cur.psz_string))
        aout_InputRequestRestart ((audio_output_t *)obj);
    (void) var; (void) data;
    return VLC_SUCCESS;
}

static int StereoModeCallback (vlc_object_t *obj, const char *varname,
                               vlc_value_t oldval, vlc_value_t newval, void *data)
{
    audio_output_t *aout = (audio_output_t *)obj;
    (void)varname; (void)oldval; (void)newval; (void)data;

    aout_owner_t *owner = aout_owner (aout);
    vlc_mutex_lock (&owner->lock);
    owner->requested_stereo_mode = newval.i_int;
    vlc_mutex_unlock (&owner->lock);

    aout_RestartRequest (aout, AOUT_RESTART_STEREOMODE);
    return 0;
}

static int MixModeCallback (vlc_object_t *obj, const char *varname,
                               vlc_value_t oldval, vlc_value_t newval, void *data)
{
    audio_output_t *aout = (audio_output_t *)obj;
    (void)varname; (void)oldval; (void)newval; (void)data;

    aout_owner_t *owner = aout_owner (aout);
    vlc_mutex_lock (&owner->lock);
    owner->requested_mix_mode = newval.i_int;
    vlc_mutex_unlock (&owner->lock);

    aout_RestartRequest (aout, AOUT_RESTART_STEREOMODE);
    return 0;
}

static void aout_ChangeViewpoint(audio_output_t *, const vlc_viewpoint_t *);

static int ViewpointCallback (vlc_object_t *obj, const char *var,
                              vlc_value_t prev, vlc_value_t cur, void *data)
{
    if( cur.p_address != NULL )
        aout_ChangeViewpoint((audio_output_t *)obj, cur.p_address );
    (void) var; (void) data; (void) prev;
    return VLC_SUCCESS;
}

#undef aout_New
/**
 * Creates an audio output object and initializes an output module.
 */
audio_output_t *aout_New (vlc_object_t *parent)
{
    vlc_value_t val;

    audio_output_t *aout = vlc_custom_create (parent, sizeof (aout_instance_t),
                                              "audio output");
    if (unlikely(aout == NULL))
        return NULL;

    aout_owner_t *owner = aout_owner (aout);

    vlc_mutex_init (&owner->lock);
    vlc_mutex_init (&owner->dev.lock);
    vlc_mutex_init (&owner->vp.lock);
    vlc_viewpoint_init (&owner->vp.value);
    vlc_list_init(&owner->dev.list);
    atomic_init (&owner->vp.update, false);
    vlc_atomic_rc_init(&owner->rc);
    vlc_audio_meter_Init(&owner->meter, aout);

    owner->main_stream = NULL;

    /* Audio output module callbacks */
    var_Create (aout, "volume", VLC_VAR_FLOAT);
    var_AddCallback (aout, "volume", var_Copy, parent);
    var_Create (aout, "mute", VLC_VAR_BOOL);
    var_AddCallback (aout, "mute", var_Copy, parent);
    var_Create (aout, "device", VLC_VAR_STRING);
    var_AddCallback (aout, "device", var_CopyDevice, parent);

    aout->events = &aout_events;

    /* Audio output module initialization */
    aout->start = NULL;
    aout->stop = NULL;
    aout->volume_set = NULL;
    aout->mute_set = NULL;
    aout->device_select = NULL;
    owner->module = module_need_var(aout, "audio output", "aout");
    if (owner->module == NULL)
    {
        msg_Err (aout, "no suitable audio output module");
        vlc_object_delete(aout);
        return NULL;
    }
    assert(aout->start && aout->stop);

    /*
     * Persistent audio output variables
     */
    module_config_t *cfg;
    char *str;

    /* Visualizations */
    var_Create (aout, "visual", VLC_VAR_STRING);
    var_Change(aout, "visual", VLC_VAR_SETTEXT, _("Visualizations"));
    val.psz_string = (char *)"";
    var_Change(aout, "visual", VLC_VAR_ADDCHOICE, val, _("Disable"));
    val.psz_string = (char *)"spectrometer";
    var_Change(aout, "visual", VLC_VAR_ADDCHOICE, val, _("Spectrometer"));
    val.psz_string = (char *)"scope";
    var_Change(aout, "visual", VLC_VAR_ADDCHOICE, val, _("Scope"));
    val.psz_string = (char *)"spectrum";
    var_Change(aout, "visual", VLC_VAR_ADDCHOICE, val, _("Spectrum"));
    val.psz_string = (char *)"vuMeter";
    var_Change(aout, "visual", VLC_VAR_ADDCHOICE, val, _("VU meter"));
    /* Look for goom plugin */
    if (module_exists ("goom"))
    {
        val.psz_string = (char *)"goom";
        var_Change(aout, "visual", VLC_VAR_ADDCHOICE, val, "Goom");
    }
    /* Look for libprojectM plugin */
    if (module_exists ("projectm"))
    {
        val.psz_string = (char *)"projectm";
        var_Change(aout, "visual", VLC_VAR_ADDCHOICE, val, "projectM");
    }
    /* Look for VSXu plugin */
    if (module_exists ("vsxu"))
    {
        val.psz_string = (char *)"vsxu";
        var_Change(aout, "visual", VLC_VAR_ADDCHOICE, val, "Vovoid VSXU");
    }
    /* Look for glspectrum plugin */
    if (module_exists ("glspectrum"))
    {
        val.psz_string = (char *)"glspectrum";
        var_Change(aout, "visual", VLC_VAR_ADDCHOICE, val, "3D spectrum");
    }
    str = var_GetNonEmptyString (aout, "effect-list");
    if (str != NULL)
    {
        var_SetString (aout, "visual", str);
        free (str);
    }

    var_Create (aout, "audio-filter", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    var_AddCallback (aout, "audio-filter", FilterCallback, NULL);
    var_Change(aout, "audio-filter", VLC_VAR_SETTEXT, _("Audio filters"));

    var_Create (aout, "viewpoint", VLC_VAR_ADDRESS );
    var_AddCallback (aout, "viewpoint", ViewpointCallback, NULL);

    var_Create (aout, "audio-visual", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    var_Change(aout, "audio-visual", VLC_VAR_SETTEXT,
               _("Audio visualizations"));

    /* Replay gain */
    var_Create (aout, "audio-replay-gain-mode",
                VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Change(aout, "audio-replay-gain-mode", VLC_VAR_SETTEXT,
               _("Replay gain"));
    cfg = config_FindConfig("audio-replay-gain-mode");
    if (likely(cfg != NULL))
        for (unsigned i = 0; i < cfg->list_count; i++)
        {
            val.psz_string = (char *)cfg->list.psz[i];
            var_Change(aout, "audio-replay-gain-mode", VLC_VAR_ADDCHOICE,
                       val, vlc_gettext(cfg->list_text[i]));
        }

    /* Stereo mode */
    var_Create (aout, "stereo-mode", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);
    owner->requested_stereo_mode = var_GetInteger (aout, "stereo-mode");

    var_AddCallback (aout, "stereo-mode", StereoModeCallback, NULL);
    var_Change(aout, "stereo-mode", VLC_VAR_SETTEXT, _("Stereo audio mode"));

    /* Mix mode */
    var_Create (aout, "mix-mode", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);
    owner->requested_mix_mode = var_GetInteger (aout, "mix-mode");
    var_AddCallback (aout, "mix-mode", MixModeCallback, NULL);
    var_Change(aout, "mix-mode", VLC_VAR_SETTEXT, _("Audio mix mode"));

    /* Equalizer */
    var_Create (aout, "equalizer-preamp", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT);
    var_Create (aout, "equalizer-bands", VLC_VAR_STRING | VLC_VAR_DOINHERIT);
    var_Create (aout, "equalizer-preset", VLC_VAR_STRING | VLC_VAR_DOINHERIT);

    owner->bitexact = var_InheritBool (aout, "audio-bitexact");

    return aout;
}

audio_output_t *aout_Hold(audio_output_t *aout)
{
    aout_owner_t *owner = aout_owner(aout);

    vlc_atomic_rc_inc(&owner->rc);
    return aout;
}

/**
 * Deinitializes an audio output module and destroys an audio output object.
 */
static void aout_Destroy (audio_output_t *aout)
{
    aout_owner_t *owner = aout_owner (aout);

    vlc_mutex_lock(&owner->lock);
    module_unneed (aout, owner->module);
    /* Protect against late call from intf.c */
    aout->volume_set = NULL;
    aout->mute_set = NULL;
    aout->device_select = NULL;
    vlc_audio_meter_Destroy(&owner->meter);
    vlc_mutex_unlock(&owner->lock);

    var_DelCallback (aout, "viewpoint", ViewpointCallback, NULL);
    var_DelCallback (aout, "audio-filter", FilterCallback, NULL);
    var_DelCallback(aout, "device", var_CopyDevice, vlc_object_parent(aout));
    var_DelCallback(aout, "mute", var_Copy, vlc_object_parent(aout));
    var_SetFloat (aout, "volume", -1.f);
    var_DelCallback(aout, "volume", var_Copy, vlc_object_parent(aout));
    var_DelCallback (aout, "stereo-mode", StereoModeCallback, NULL);
    var_DelCallback (aout, "mix-mode", MixModeCallback, NULL);

    aout_dev_t *dev;
    vlc_list_foreach(dev, &owner->dev.list, node)
    {
        vlc_list_remove(&dev->node);
        free (dev->name);
        free (dev);
    }

    vlc_object_delete(VLC_OBJECT(aout));
}

void aout_Release(audio_output_t *aout)
{
    aout_owner_t *owner = aout_owner(aout);

    if (!vlc_atomic_rc_dec(&owner->rc))
        return;

    aout_Destroy(aout);
}

static int aout_PrepareStereoMode(audio_output_t *aout,
                                  const audio_sample_format_t *restrict fmt)
{
    aout_owner_t *owner = aout_owner (aout);

    /* Fill Stereo mode choices */
    vlc_value_t val;
    const char *txt;
    val.i_int = 0;

    if (!AOUT_FMT_LINEAR(fmt) || fmt->i_channels != 2)
        return AOUT_VAR_CHAN_UNSET;

    int i_default_mode = owner->requested_stereo_mode;

    val.i_int = AOUT_VAR_CHAN_MONO;
    var_Change(aout, "stereo-mode", VLC_VAR_ADDCHOICE, val, _("Mono"));

    if (fmt->i_chan_mode & AOUT_CHANMODE_DOLBYSTEREO)
    {
        val.i_int = AOUT_VAR_CHAN_DOLBYS;
        txt = _("Dolby Surround");
    }
    else
    {
        val.i_int = AOUT_VAR_CHAN_STEREO;
        txt = _("Stereo");
    }
    var_Change(aout, "stereo-mode", VLC_VAR_ADDCHOICE, val, txt);

    if (fmt->i_chan_mode & AOUT_CHANMODE_DUALMONO)
        i_default_mode = AOUT_VAR_CHAN_LEFT;
    else
        i_default_mode = val.i_int; /* Stereo or Dolby Surround */

    val.i_int = AOUT_VAR_CHAN_LEFT;
    var_Change(aout, "stereo-mode", VLC_VAR_ADDCHOICE, val, _("Left"));
    val.i_int = AOUT_VAR_CHAN_RIGHT;
    var_Change(aout, "stereo-mode", VLC_VAR_ADDCHOICE, val, _("Right"));

    val.i_int = AOUT_VAR_CHAN_RSTEREO;
    var_Change(aout, "stereo-mode", VLC_VAR_ADDCHOICE, val,
               _("Reverse stereo"));

    return i_default_mode;
}

static void aout_UpdateStereoMode(audio_output_t *aout, int mode,
                                  audio_sample_format_t *restrict fmt,
                                  aout_filters_cfg_t *filters_cfg)
{
    /* The user may have selected a different channels configuration. */
    switch (mode)
    {
        case AOUT_VAR_CHAN_RSTEREO:
            filters_cfg->remap[AOUT_CHANIDX_LEFT] = AOUT_CHANIDX_RIGHT;
            filters_cfg->remap[AOUT_CHANIDX_RIGHT] = AOUT_CHANIDX_LEFT;
            break;
        case AOUT_VAR_CHAN_STEREO:
            break;
        case AOUT_VAR_CHAN_LEFT:
            filters_cfg->remap[AOUT_CHANIDX_RIGHT] = AOUT_CHANIDX_DISABLE;
            fmt->i_physical_channels = AOUT_CHAN_CENTER;
            aout_FormatPrepare (fmt);
            break;
        case AOUT_VAR_CHAN_RIGHT:
            filters_cfg->remap[AOUT_CHANIDX_LEFT] = AOUT_CHANIDX_DISABLE;
            fmt->i_physical_channels = AOUT_CHAN_CENTER;
            aout_FormatPrepare (fmt);
            break;
        case AOUT_VAR_CHAN_DOLBYS:
            fmt->i_chan_mode = AOUT_CHANMODE_DOLBYSTEREO;
            break;
        case AOUT_VAR_CHAN_MONO:
            /* Remix all channels into one */
            for (size_t i = 0; i < AOUT_CHANIDX_MAX; ++ i)
                filters_cfg->remap[i] = AOUT_CHANIDX_LEFT;
            break;
        default:
            break;
    }

    var_Change(aout, "stereo-mode", VLC_VAR_SETVALUE,
               (vlc_value_t) { .i_int = mode});
}

static bool aout_HasStereoMode(audio_output_t *aout, int mode)
{
    bool mode_available = false;
    vlc_value_t *vals;
    size_t count;

    if (!var_Change(aout, "stereo-mode", VLC_VAR_GETCHOICES,
                    &count, &vals, (char ***)NULL))
    {
        for (size_t i = 0; !mode_available && i < count; ++i)
        {
            if (vals[i].i_int == mode)
                mode_available = true;
        }
        free(vals);
    }
    return mode_available;
}

static void aout_AddMixModeChoice(audio_output_t *aout, int mode,
                                  const char *suffix,
                                  const audio_sample_format_t *restrict fmt)
{
    assert(suffix);
    const char *text;
    char *buffer = NULL;

    if (fmt == NULL)
        text = suffix;
    else
    {
        const char *channels = aout_FormatPrintChannels(fmt);
        if (asprintf(&buffer, "%s: %s", suffix, channels) < 0)
            return;
        text = buffer;
    }

    vlc_value_t val = { .i_int = mode };
    var_Change(aout, "mix-mode", VLC_VAR_ADDCHOICE, val, text);

    free(buffer);
}

static void aout_SetupMixModeChoices (audio_output_t *aout,
                                      const audio_sample_format_t *restrict fmt)
{
    if (fmt->i_channels <= 2)
        return;

    const bool has_spatialaudio = module_exists("spatialaudio");

    /* Don't propose the mix option if we don't have the spatialaudio module
     * and if the content is ambisonics */
    if (fmt->channel_type != AUDIO_CHANNEL_TYPE_AMBISONICS || has_spatialaudio)
    {
        aout_AddMixModeChoice(aout, AOUT_MIX_MODE_UNSET, _("Original"), fmt);
        aout_AddMixModeChoice(aout, AOUT_MIX_MODE_STEREO, _("Stereo"), NULL);
    }

    if (has_spatialaudio)
        aout_AddMixModeChoice(aout, AOUT_MIX_MODE_BINAURAL, _("Binaural"), NULL);

    /* Only propose Original and Binaural for Ambisonics content */
    if (fmt->channel_type == AUDIO_CHANNEL_TYPE_AMBISONICS && has_spatialaudio)
        return;

    if (fmt->i_physical_channels != AOUT_CHANS_4_0)
    {
        static const audio_sample_format_t fmt_4_0 = {
            .i_physical_channels = AOUT_CHANS_4_0,
            .i_channels = 4,
        };
        aout_AddMixModeChoice(aout, AOUT_MIX_MODE_4_0, _("4.0"), &fmt_4_0);
    }

    if (fmt->i_physical_channels != AOUT_CHANS_5_1)
    {
        static const audio_sample_format_t fmt_5_1 = {
            .i_physical_channels = AOUT_CHANS_5_1,
            .i_channels = 6,
        };
        aout_AddMixModeChoice(aout, AOUT_MIX_MODE_5_1, _("5.1"), &fmt_5_1);
    }

    if (fmt->i_physical_channels != AOUT_CHANS_7_1)
    {
        static const audio_sample_format_t fmt_7_1 = {
            .i_physical_channels = AOUT_CHANS_7_1,
            .i_channels = 8,
        };
        aout_AddMixModeChoice(aout, AOUT_MIX_MODE_7_1, _("7.1"), &fmt_7_1);
    }
}

static bool aout_HasMixModeChoice(audio_output_t *aout, int mode)
{
    bool mode_available = false;
    vlc_value_t *vals;
    size_t count;

    if (!var_Change(aout, "mix-mode", VLC_VAR_GETCHOICES,
                    &count, &vals, (char ***)NULL))
    {
        for (size_t i = 0; !mode_available && i < count; ++i)
        {
            if (vals[i].i_int == mode)
                mode_available = true;
        }
        free(vals);
    }
    return mode_available;
}


static void aout_UpdateMixMode(audio_output_t *aout, int mode,
                               audio_sample_format_t *restrict fmt)
{
    /* The user may have selected a different channels configuration. */
    switch (mode)
    {
        case AOUT_MIX_MODE_UNSET:
            break;
        case AOUT_MIX_MODE_BINAURAL:
            fmt->i_physical_channels = AOUT_CHANS_STEREO;
            fmt->i_chan_mode = AOUT_CHANMODE_BINAURAL;
            break;
        case AOUT_MIX_MODE_STEREO:
            fmt->i_physical_channels = AOUT_CHANS_STEREO;
            break;
        case AOUT_MIX_MODE_4_0:
            fmt->i_physical_channels = AOUT_CHANS_4_0;
            break;
        case AOUT_MIX_MODE_5_1:
            fmt->i_physical_channels = AOUT_CHANS_5_1;
            break;
        case AOUT_MIX_MODE_7_1:
            fmt->i_physical_channels = AOUT_CHANS_7_1;
            break;
        default:
            break;
    }

    assert(mode == AOUT_VAR_CHAN_UNSET || aout_HasMixModeChoice(aout, mode));

    var_Change(aout, "mix-mode", VLC_VAR_SETVALUE, (vlc_value_t) { .i_int = mode});
}

int aout_OutputNew(audio_output_t *aout, vlc_aout_stream *stream,
                   audio_sample_format_t *fmt, int input_profile,
                   audio_sample_format_t *filter_fmt,
                   aout_filters_cfg_t *filters_cfg)
{
    aout_owner_t *owner = aout_owner (aout);

    vlc_fourcc_t formats[] = {
        fmt->i_format, 0, 0
    };

    var_Change(aout, "stereo-mode", VLC_VAR_CLEARCHOICES);
    var_Change(aout, "mix-mode", VLC_VAR_CLEARCHOICES);

    /* Ideally, the audio filters would be created before the audio output,
     * and the ideal audio format would be the output of the filters chain.
     * But that scheme would not really play well with digital pass-through. */
    if (AOUT_FMT_LINEAR(fmt))
    {
        if (fmt->channel_type == AUDIO_CHANNEL_TYPE_BITMAP
         && aout_FormatNbChannels(fmt) == 0)
        {
            /* The output channel map is unknown, use the WAVE one. */
            assert(fmt->i_channels > 0);
            aout_SetWavePhysicalChannels(fmt);
        }

        if (fmt->channel_type == AUDIO_CHANNEL_TYPE_AMBISONICS)
        {
            /* Set the maximum of channels to render ambisonics contents. The
             * aout module will still be free to select less channels in order
             * to respect the sink setup. */
            fmt->i_physical_channels = AOUT_CHANS_7_1;
        }

        /* Try to stay in integer domain if possible for no/slow FPU. */
        fmt->i_format = (fmt->i_bitspersample > 16) ? VLC_CODEC_FL32
                                                    : VLC_CODEC_S16N;

        aout_SetupMixModeChoices(aout, fmt);

        /* Prefer the user requested mode if available, otherwise, use the
         * default one */
        if (aout_HasMixModeChoice(aout, owner->requested_mix_mode))
            aout_UpdateMixMode(aout, owner->requested_mix_mode, fmt);

        aout_FormatPrepare (fmt);
        assert (aout_FormatNbChannels(fmt) > 0);
    }
    else
    {
        switch (fmt->i_format)
        {
            case VLC_CODEC_DTS:
                if (input_profile > 0)
                {
                    assert(ARRAY_SIZE(formats) >= 3);
                    /* DTSHD can be played as DTSHD or as DTS */
                    formats[0] = VLC_CODEC_DTSHD;
                    formats[1] = VLC_CODEC_DTS;
                }
                break;
            case VLC_CODEC_A52:
                if (input_profile > 0)
                {
                    assert(ARRAY_SIZE(formats) >= 3);
                    formats[0] = VLC_CODEC_EAC3;
                    formats[1] = VLC_CODEC_A52;
                }
                break;
            default:
                break;
        }
    }

    int stereo_mode = aout_PrepareStereoMode(aout, fmt);

    if (stereo_mode != AOUT_VAR_CHAN_UNSET
     && aout_HasStereoMode(aout, stereo_mode))
        aout_UpdateStereoMode(aout, stereo_mode, fmt, filters_cfg);

    aout->current_sink_info.headphones = false;

    vlc_mutex_lock(&owner->lock);
    /* XXX: Remove when aout/stream support is complete (in all modules) */
    assert(owner->main_stream == NULL);

    int ret = VLC_EGENERIC;
    for (size_t i = 0; formats[i] != 0 && ret != VLC_SUCCESS; ++i)
    {
        filter_fmt->i_format = fmt->i_format = formats[i];
        owner->main_stream = stream;
        ret = aout->start(aout, fmt);
        if (ret != 0)
            owner->main_stream = NULL;
    }
    vlc_mutex_unlock(&owner->lock);
    if (ret)
    {
        if (AOUT_FMT_LINEAR(fmt))
            msg_Err (aout, "failed to start audio output");
        else
            msg_Warn (aout, "failed to start passthrough audio output, "
                      "failing back to linear format");
        return -1;
    }
    assert(aout->flush != NULL && aout->play != NULL);

    /* Autoselect the headphones mode if available and if the user didn't
     * request any mode */
    if (aout->current_sink_info.headphones
     && owner->requested_mix_mode == AOUT_VAR_CHAN_UNSET
     && fmt->i_physical_channels == AOUT_CHANS_STEREO
     && aout_HasMixModeChoice(aout, AOUT_MIX_MODE_BINAURAL))
    {
        assert(stereo_mode == AOUT_VAR_CHAN_UNSET);
        aout_UpdateMixMode(aout, AOUT_MIX_MODE_BINAURAL, fmt);
    }

    aout_FormatPrepare (fmt);
    assert (fmt->i_bytes_per_frame > 0 && fmt->i_frame_length > 0);
    aout_FormatPrint (aout, "output", fmt);

    return 0;
}

void aout_OutputDelete (audio_output_t *aout)
{
    aout_owner_t *owner = aout_owner(aout);
    vlc_mutex_lock(&owner->lock);
    aout->stop (aout);
    owner->main_stream = NULL;
    vlc_mutex_unlock(&owner->lock);
}

float aout_VolumeGet (audio_output_t *aout)
{
    return var_GetFloat (aout, "volume");
}

int aout_VolumeSet (audio_output_t *aout, float vol)
{
    aout_owner_t *owner = aout_owner(aout);
    int ret;

    vlc_mutex_lock(&owner->lock);
    ret = aout->volume_set ? aout->volume_set(aout, vol) : -1;
    vlc_mutex_unlock(&owner->lock);
    return ret ? -1 : 0;
}

int aout_VolumeUpdate (audio_output_t *aout, int value, float *volp)
{
    int ret = -1;
    const float defaultVolume = (float)AOUT_VOLUME_DEFAULT;
    const float stepSize = var_InheritFloat (aout, "volume-step") / defaultVolume;
    float vol = aout_VolumeGet (aout);

    if (vol >= 0.f)
    {
        vol += (value * stepSize);
        vol = (roundf (vol / stepSize)) * stepSize;
        vol = clampf(vol, 0.f, AOUT_VOLUME_MAX / defaultVolume);
        if (volp != NULL)
            *volp = vol;
        ret = aout_VolumeSet (aout, vol);
    }
    return ret;
}

int aout_MuteGet (audio_output_t *aout)
{
    return var_InheritBool (aout, "mute");
}

int aout_MuteSet (audio_output_t *aout, bool mute)
{
    aout_owner_t *owner = aout_owner(aout);
    int ret;

    vlc_mutex_lock(&owner->lock);
    ret = aout->mute_set ? aout->mute_set(aout, mute) : -1;
    vlc_mutex_unlock(&owner->lock);
    return ret ? -1 : 0;
}

char *aout_DeviceGet (audio_output_t *aout)
{
    return var_GetNonEmptyString (aout, "device");
}

int aout_DeviceSet (audio_output_t *aout, const char *id)
{
    aout_owner_t *owner = aout_owner(aout);
    int ret;

    vlc_mutex_lock(&owner->lock);
    ret = aout->device_select ? aout->device_select(aout, id) : -1;
    vlc_mutex_unlock(&owner->lock);
    return ret ? -1 : 0;
}

int aout_DevicesList (audio_output_t *aout, char ***ids, char ***names)
{
    aout_owner_t *owner = aout_owner (aout);
    char **tabid, **tabname;
    unsigned i = 0;

    vlc_mutex_lock (&owner->dev.lock);
    tabid = vlc_alloc (owner->dev.count, sizeof (*tabid));
    tabname = vlc_alloc (owner->dev.count, sizeof (*tabname));

    if (unlikely(tabid == NULL || tabname == NULL))
        goto error;

    *ids = tabid;
    *names = tabname;

    aout_dev_t *dev;
    vlc_list_foreach(dev, &owner->dev.list, node)
    {
        tabid[i] = strdup(dev->id);
        if (unlikely(tabid[i] == NULL))
            goto error;

        tabname[i] = strdup(dev->name);
        if (unlikely(tabname[i] == NULL))
        {
            free(tabid[i]);
            goto error;
        }

        i++;
    }
    vlc_mutex_unlock (&owner->dev.lock);

    return i;

error:
    vlc_mutex_unlock(&owner->dev.lock);
    while (i > 0)
    {
        i--;
        free(tabname[i]);
        free(tabid[i]);
    }
    free(tabname);
    free(tabid);
    return -1;
}

static void aout_ChangeViewpoint(audio_output_t *aout,
                                 const vlc_viewpoint_t *p_viewpoint)
{
    aout_owner_t *owner = aout_owner(aout);

    vlc_mutex_lock(&owner->vp.lock);
    owner->vp.value = *p_viewpoint;
    atomic_store_explicit(&owner->vp.update, true, memory_order_relaxed);
    vlc_mutex_unlock(&owner->vp.lock);
}

vlc_audio_meter_plugin *
aout_AddMeterPlugin(audio_output_t *aout, const char *chain,
                    const struct vlc_audio_meter_plugin_owner *meter_plugin_owner)
{
    aout_owner_t *owner = aout_owner(aout);

    return vlc_audio_meter_AddPlugin(&owner->meter, chain, meter_plugin_owner);
}

void
aout_RemoveMeterPlugin(audio_output_t *aout, vlc_audio_meter_plugin *plugin)
{
    aout_owner_t *owner = aout_owner(aout);

    vlc_audio_meter_RemovePlugin(&owner->meter, plugin);
}
