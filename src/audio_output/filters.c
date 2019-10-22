/*****************************************************************************
 * filters.c : audio output filters management
 *****************************************************************************
 * Copyright (C) 2002-2007 VLC authors and VideoLAN
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <math.h>
#include <string.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_dialog.h>
#include <vlc_modules.h>
#include <vlc_aout.h>
#include <vlc_filter.h>
#include <libvlc.h>
#include "aout_internal.h"
#include "../video_output/vout_internal.h" /* for vout_Request */

static filter_t *CreateFilter(vlc_object_t *obj, vlc_clock_t *clock,
                              const char *type, const char *name,
                              const audio_sample_format_t *infmt,
                              const audio_sample_format_t *outfmt,
                              config_chain_t *cfg, bool const_fmt)
{
    filter_t *filter = vlc_custom_create (obj, sizeof (*filter), type);
    if (unlikely(filter == NULL))
        return NULL;

    filter->owner.sys = clock;
    filter->p_cfg = cfg;
    filter->fmt_in.audio = *infmt;
    filter->fmt_in.i_codec = infmt->i_format;
    filter->fmt_out.audio = *outfmt;
    filter->fmt_out.i_codec = outfmt->i_format;

#ifndef NDEBUG
    /* Assure that infmt/oufmt are well prepared and that channels
     * configurations are valid*/
    if( infmt->i_physical_channels != 0 )
        assert( aout_FormatNbChannels( infmt ) == infmt->i_channels );
    if( outfmt->i_physical_channels != 0 )
        assert( aout_FormatNbChannels( outfmt ) == outfmt->i_channels );
#endif

    filter->p_module = module_need (filter, type, name, false);

#ifndef NDEBUG
    if (filter->p_module == NULL || const_fmt)
    {
        /* If probing failed, formats shall not have been modified. */
        assert (AOUT_FMTS_IDENTICAL(&filter->fmt_in.audio, infmt));
        assert (AOUT_FMTS_IDENTICAL(&filter->fmt_out.audio, outfmt));
    }
#endif

    if (filter->p_module == NULL)
    {
        vlc_object_delete(filter);
        filter = NULL;
    }
    else
        assert (filter->pf_audio_filter != NULL);
    return filter;
}

static filter_t *FindConverter (vlc_object_t *obj,
                                const audio_sample_format_t *infmt,
                                const audio_sample_format_t *outfmt)
{
    return CreateFilter(obj, NULL, "audio converter", NULL, infmt, outfmt,
                        NULL, true);
}

static filter_t *FindResampler (vlc_object_t *obj,
                                const audio_sample_format_t *infmt,
                                const audio_sample_format_t *outfmt)
{
    char *modlist = var_InheritString(obj, "audio-resampler");
    filter_t *filter = CreateFilter(obj, NULL, "audio resampler", modlist,
                                    infmt, outfmt, NULL, true);
    free(modlist);
    return filter;
}

/**
 * Destroys a chain of audio filters.
 */
static void aout_FiltersPipelineDestroy(filter_t *const *filters, unsigned n)
{
    for( unsigned i = 0; i < n; i++ )
    {
        filter_t *p_filter = filters[i];

        module_unneed( p_filter, p_filter->p_module );
        vlc_object_delete(p_filter);
    }
}

static filter_t *TryFormat (vlc_object_t *obj, vlc_fourcc_t codec,
                            audio_sample_format_t *restrict fmt)
{
    audio_sample_format_t output = *fmt;

    assert (codec != fmt->i_format);
    output.i_format = codec;
    aout_FormatPrepare (&output);

    filter_t *filter = FindConverter (obj, fmt, &output);
    if (filter != NULL)
        *fmt = output;
    return filter;
}

/**
 * Allocates audio format conversion filters
 * @param obj parent VLC object for new filters
 * @param filters table of filters [IN/OUT]
 * @param count pointer to the number of filters in the table [IN/OUT]
 * @param max size of filters table [IN]
 * @param infmt input audio format
 * @param outfmt output audio format
 * @return 0 on success, -1 on failure
 */
static int aout_FiltersPipelineCreate(vlc_object_t *obj, filter_t **filters,
                                      unsigned *count, unsigned max,
                                 const audio_sample_format_t *restrict infmt,
                                 const audio_sample_format_t *restrict outfmt,
                                 bool headphones)
{
    aout_FormatsPrint (obj, "conversion:", infmt, outfmt);
    max -= *count;
    filters += *count;

    /* There is a lot of second guessing on what the conversion plugins can
     * and cannot do. This seems hardly avoidable, the conversion problem need
     * to be reduced somehow. */
    audio_sample_format_t input = *infmt;
    unsigned n = 0;

    if (!AOUT_FMT_LINEAR(&input))
    {
        msg_Err(obj, "Can't convert non linear input");
        return -1;
    }

    /* Remix channels */
    if (infmt->i_physical_channels != outfmt->i_physical_channels
     || infmt->i_chan_mode != outfmt->i_chan_mode
     || infmt->channel_type != outfmt->channel_type)
    {   /* Remixing currently requires FL32... TODO: S16N */
        if (input.i_format != VLC_CODEC_FL32)
        {
            if (n == max)
                goto overflow;

            filter_t *f = TryFormat (obj, VLC_CODEC_FL32, &input);
            if (f == NULL)
            {
                msg_Err (obj, "cannot find %s for conversion pipeline",
                         "pre-mix converter");
                goto error;
            }

            filters[n++] = f;
        }

        if (n == max)
            goto overflow;

        audio_sample_format_t output;
        output.i_format = input.i_format;
        output.i_rate = input.i_rate;
        output.i_physical_channels = outfmt->i_physical_channels;
        output.channel_type = outfmt->channel_type;
        output.i_chan_mode = outfmt->i_chan_mode;
        aout_FormatPrepare (&output);

        const char *filter_type =
            infmt->channel_type != outfmt->channel_type ?
            "audio renderer" : "audio converter";

        config_chain_t *cfg = NULL;
        if (headphones)
            config_ChainParseOptions(&cfg, "{headphones=true}");
        filter_t *f = CreateFilter(obj, NULL, filter_type, NULL,
                                   &input, &output, cfg, true);
        if (cfg)
            config_ChainDestroy(cfg);

        if (f == NULL)
        {
            msg_Err (obj, "cannot find %s for conversion pipeline",
                     "remixer");
            goto error;
        }

        input = output;
        filters[n++] = f;
    }

    /* Resample */
    if (input.i_rate != outfmt->i_rate)
    {   /* Resampling works with any linear format, but may be ugly. */
        if (n == max)
            goto overflow;

        audio_sample_format_t output = input;
        output.i_rate = outfmt->i_rate;

        filter_t *f = FindConverter (obj, &input, &output);
        if (f == NULL)
        {
            msg_Err (obj, "cannot find %s for conversion pipeline",
                     "resampler");
            goto error;
        }

        input = output;
        filters[n++] = f;
    }

    /* Format */
    if (input.i_format != outfmt->i_format)
    {
        if (max == 0)
            goto overflow;

        filter_t *f = TryFormat (obj, outfmt->i_format, &input);
        if (f == NULL)
        {
            msg_Err (obj, "cannot find %s for conversion pipeline",
                     "post-mix converter");
            goto error;
        }
        filters[n++] = f;
    }

    msg_Dbg (obj, "conversion pipeline complete");
    *count += n;
    return 0;

overflow:
    msg_Err (obj, "maximum of %u conversion filters reached", max);
    vlc_dialog_display_error (obj, _("Audio filtering failed"),
        _("The maximum number of filters (%u) was reached."), max);
error:
    aout_FiltersPipelineDestroy (filters, n);
    return -1;
}

/**
 * Filters an audio buffer through a chain of filters.
 */
static block_t *aout_FiltersPipelinePlay(filter_t *const *filters,
                                         unsigned count, block_t *block)
{
    /* TODO: use filter chain */
    for (unsigned i = 0; (i < count) && (block != NULL); i++)
    {
        filter_t *filter = filters[i];

        /* Please note that p_block->i_nb_samples & i_buffer
         * shall be set by the filter plug-in. */
        block = filter->pf_audio_filter (filter, block);
    }
    return block;
}


/**
 * Drain the chain of filters.
 */
static block_t *aout_FiltersPipelineDrain(filter_t *const *filters,
                                          unsigned count)
{
    block_t *chain = NULL;

    for (unsigned i = 0; i < count; i++)
    {
        filter_t *filter = filters[i];

        block_t *block = filter_DrainAudio (filter);
        if (block)
        {
            /* If there is a drained block, filter it through the following
             * chain of filters  */
            if (i + 1 < count)
                block = aout_FiltersPipelinePlay (&filters[i + 1],
                                                  count - i - 1, block);
            if (block)
                block_ChainAppend (&chain, block);
        }
    }

    if (chain)
        return block_ChainGather(chain);
    else
        return NULL;
}

/**
 * Flush the chain of filters.
 */
static void aout_FiltersPipelineFlush(filter_t *const *filters,
                                      unsigned count)
{
    for (unsigned i = 0; i < count; i++)
        filter_Flush (filters[i]);
}

static void aout_FiltersPipelineChangeViewpoint(filter_t *const *filters,
                                                unsigned count,
                                                const vlc_viewpoint_t *vp)
{
    for (unsigned i = 0; i < count; i++)
        filter_ChangeViewpoint (filters[i], vp);
}

#define AOUT_MAX_FILTERS 10

struct aout_filters
{
    filter_t *rate_filter; /**< The filter adjusting samples count
        (either the scaletempo filter or a resampler) */
    filter_t *resampler; /**< The resampler */
    int resampling; /**< Current resampling (Hz) */
    vlc_clock_t *clock;

    unsigned count; /**< Number of filters */
    filter_t *tab[AOUT_MAX_FILTERS]; /**< Configured user filters
        (e.g. equalization) and their conversions */
};

/** Callback for visualization selection */
static int VisualizationCallback (vlc_object_t *obj, const char *var,
                                  vlc_value_t oldval, vlc_value_t newval,
                                  void *data)
{
    const char *mode = newval.psz_string;

    if (!*mode)
        mode = "none";
    /* FIXME: This ugly hack enforced by visual effect-list, as is the need for
     * separate "visual" (external) and "audio-visual" (internal) variables...
     * The visual plugin should have one submodule per effect instead. */
    if (strcasecmp (mode, "none") && strcasecmp (mode, "goom")
     && strcasecmp (mode, "projectm") && strcasecmp (mode, "vsxu")
     && strcasecmp (mode, "glspectrum"))
    {
        var_Create (obj, "effect-list", VLC_VAR_STRING);
        var_SetString (obj, "effect-list", mode);
        mode = "visual";
    }

    var_SetString (obj, "audio-visual", mode);
    aout_InputRequestRestart ((audio_output_t *)obj);
    (void) var; (void) oldval; (void) data;
    return VLC_SUCCESS;
}

vout_thread_t *aout_filter_GetVout(filter_t *filter, const video_format_t *fmt)
{
    vout_thread_t *vout = vout_Create(VLC_OBJECT(filter));
    if (unlikely(vout == NULL))
        return NULL;

    video_format_t adj_fmt = *fmt;
    vout_configuration_t cfg = {
        .vout = vout, .clock = filter->owner.sys, .fmt = &adj_fmt,
    };

    video_format_AdjustColorSpace(&adj_fmt);

    if (vout_Request(&cfg, NULL, NULL)) {
        vout_Close(vout);
        vout = NULL;
    }
    return vout;
}

static int AppendFilter(vlc_object_t *obj, const char *type, const char *name,
                        aout_filters_t *restrict filters,
                        audio_sample_format_t *restrict infmt,
                        const audio_sample_format_t *restrict outfmt,
                        config_chain_t *cfg)
{
    const unsigned max = sizeof (filters->tab) / sizeof (filters->tab[0]);
    if (filters->count >= max)
    {
        msg_Err (obj, "maximum of %u filters reached", max);
        return -1;
    }

    filter_t *filter = CreateFilter(obj, filters->clock, type, name,
                                    infmt, outfmt, cfg, false);
    if (filter == NULL)
    {
        msg_Err (obj, "cannot add user %s \"%s\" (skipped)", type, name);
        return -1;
    }

    /* convert to the filter input format if necessary */
    if (aout_FiltersPipelineCreate (obj, filters->tab, &filters->count,
                                    max - 1, infmt, &filter->fmt_in.audio, false))
    {
        msg_Err (filter, "cannot add user %s \"%s\" (skipped)", type, name);
        module_unneed (filter, filter->p_module);
        vlc_object_delete(filter);
        return -1;
    }

    assert (filters->count < max);
    filters->tab[filters->count] = filter;
    filters->count++;
    *infmt = filter->fmt_out.audio;
    return 0;
}

static int AppendRemapFilter(vlc_object_t *obj, aout_filters_t *restrict filters,
                             audio_sample_format_t *restrict infmt,
                             const audio_sample_format_t *restrict outfmt,
                             const int *wg4_remap)
{
    char *name;
    config_chain_t *cfg;

    /* The remap audio filter use a different order than wg4 */
    static const uint8_t wg4_to_remap[] = { 0, 2, 6, 7, 3, 5, 4, 1, 8 };
    int remap[AOUT_CHAN_MAX];
    bool needed = false;
    for (int i = 0; i < AOUT_CHAN_MAX; ++i)
    {
        if (wg4_remap[i] != i)
            needed = true;
        remap[i] = wg4_remap[i] >= 0 ? wg4_to_remap[wg4_remap[i]] : -1;
    }
    if (!needed)
        return 0;

    char *str;
    int ret = asprintf(&str, "remap{channel-left=%d,channel-right=%d,"
                       "channel-middleleft=%d,channel-middleright=%d,"
                       "channel-rearleft=%d,channel-rearright=%d,"
                       "channel-rearcenter=%d,channel-center=%d,"
                       "channel-lfe=%d,normalize=false}",
                       remap[0], remap[1], remap[2], remap[3], remap[4],
                       remap[5], remap[6], remap[7], remap[8]);
    if (ret == -1)
        return -1;

    free(config_ChainCreate(&name, &cfg, str));
    if (name != NULL && cfg != NULL)
        ret = AppendFilter(obj, "audio filter", name, filters,
                           infmt, outfmt, cfg);
    else
        ret = -1;

    free(str);
    free(name);
    if (cfg)
        config_ChainDestroy(cfg);
    return ret;
}

aout_filters_t *aout_FiltersNewWithClock(vlc_object_t *obj, const vlc_clock_t *clock,
                                         const audio_sample_format_t *restrict infmt,
                                         const audio_sample_format_t *restrict outfmt,
                                         const aout_filters_cfg_t *cfg)
{
    aout_filters_t *filters = malloc (sizeof (*filters));
    if (unlikely(filters == NULL))
        return NULL;

    filters->rate_filter = NULL;
    filters->resampler = NULL;
    filters->resampling = 0;
    filters->count = 0;
    if (clock)
    {
        filters->clock = vlc_clock_CreateSlave(clock, AUDIO_ES);
        if (!filters->clock)
            goto error;
    }
    else
        filters->clock = NULL;

    /* Prepare format structure */
    aout_FormatPrint (obj, "input", infmt);
    audio_sample_format_t input_format = *infmt;
    audio_sample_format_t output_format = *outfmt;

    /* Callbacks (before reading values and also before return statement) */
    var_AddCallback (obj, "visual", VisualizationCallback, NULL);

    if (!AOUT_FMT_LINEAR(outfmt))
    {   /* Non-linear output: just convert formats, no filters/visu */
        if (!AOUT_FMTS_IDENTICAL(infmt, outfmt))
        {
            aout_FormatsPrint (obj, "pass-through:", infmt, outfmt);
            filters->tab[0] = FindConverter(obj, infmt, outfmt);
            if (filters->tab[0] == NULL)
            {
                msg_Err (obj, "cannot setup pass-through");
                goto error;
            }
            filters->count++;
        }
        return filters;
    }
    if (aout_FormatNbChannels(outfmt) == 0)
    {
        msg_Warn (obj, "No output channel mask, cannot setup filters");
        goto error;
    }

    assert(output_format.channel_type == AUDIO_CHANNEL_TYPE_BITMAP);
    if (input_format.channel_type != output_format.channel_type)
    {
        /* Do the channel type conversion before any filters since audio
         * converters and filters handle only AUDIO_CHANNEL_TYPE_BITMAP */

        /* convert to the output format (minus resampling) if necessary */
        output_format.i_rate = input_format.i_rate;
        if (aout_FiltersPipelineCreate (obj, filters->tab, &filters->count,
                                  AOUT_MAX_FILTERS, &input_format, &output_format,
                                  cfg->headphones))
        {
            msg_Warn (obj, "cannot setup audio renderer pipeline");
            /* Fallback to bitmap without any conversions */
            input_format.channel_type = AUDIO_CHANNEL_TYPE_BITMAP;
            aout_FormatPrepare(&input_format);
        }
        else
            input_format = output_format;
    }

    if (aout_FormatNbChannels(&input_format) == 0)
    {
        /* The input channel map is unknown, use the WAVE one and add a
         * converter that will drop extra channels that are not handled by VLC
         * */
        msg_Info(obj, "unknown channel map, using the WAVE channel layout.");

        assert(input_format.i_channels > 0);
        audio_sample_format_t input_phys_format = input_format;
        aout_SetWavePhysicalChannels(&input_phys_format);

        filter_t *f = FindConverter (obj, &input_format, &input_phys_format);
        if (f == NULL)
        {
            msg_Err (obj, "cannot find channel converter");
            goto error;
        }

        input_format = input_phys_format;
        filters->tab[filters->count++] = f;
    }

    assert(input_format.channel_type == AUDIO_CHANNEL_TYPE_BITMAP);

    /* parse user filter lists */
    if (var_InheritBool (obj, "audio-time-stretch"))
    {
        if (AppendFilter(obj, "audio filter", "scaletempo",
                         filters, &input_format, &output_format, NULL) == 0)
            filters->rate_filter = filters->tab[filters->count - 1];
    }

    if (cfg != NULL)
    {
        AppendRemapFilter(obj, filters, &input_format, &output_format,
                          cfg->remap);

        if (input_format.i_channels > 2 && cfg->headphones)
            AppendFilter(obj, "audio filter", "binauralizer", filters,
                         &input_format, &output_format, NULL);
    }

    /* Now add user filters */
    char *str = var_InheritString (obj, "audio-filter");
    if (str != NULL)
    {
        char *p = str, *name;
        while ((name = strsep (&p, " :")) != NULL)
        {
            AppendFilter(obj, "audio filter", name, filters,
                         &input_format, &output_format, NULL);
        }
        free (str);
    }

    char *visual = var_InheritString(obj, "audio-visual");
    if (visual != NULL && strcasecmp(visual, "none"))
        AppendFilter(obj, "visualization", visual, filters,
                     &input_format, &output_format, NULL);
    free(visual);

    /* convert to the output format (minus resampling) if necessary */
    output_format.i_rate = input_format.i_rate;
    if (aout_FiltersPipelineCreate (obj, filters->tab, &filters->count,
                              AOUT_MAX_FILTERS, &input_format, &output_format, false))
    {
        msg_Err (obj, "cannot setup filtering pipeline");
        goto error;
    }
    input_format = output_format;

    /* insert the resampler */
    output_format.i_rate = outfmt->i_rate;
    assert (AOUT_FMTS_IDENTICAL(&output_format, outfmt));
    filters->resampler = FindResampler (obj, &input_format,
                                        &output_format);
    if (filters->resampler == NULL && input_format.i_rate != outfmt->i_rate)
    {
        msg_Err (obj, "cannot setup a resampler");
        goto error;
    }
    if (filters->rate_filter == NULL)
        filters->rate_filter = filters->resampler;

    return filters;

error:
    aout_FiltersPipelineDestroy (filters->tab, filters->count);
    var_DelCallback(obj, "visual", VisualizationCallback, NULL);
    if (filters->clock)
        vlc_clock_Delete(filters->clock);
    free (filters);
    return NULL;
}

void aout_FiltersResetClock(aout_filters_t *filters)
{
    assert(filters->clock);
    vlc_clock_Reset(filters->clock);
}

void aout_FiltersSetClockDelay(aout_filters_t *filters, vlc_tick_t delay)
{
    assert(filters->clock);
    vlc_clock_SetDelay(filters->clock, delay);
}

#undef aout_FiltersNew
/**
 * Sets a chain of audio filters up.
 * \param obj parent object for the filters
 * \param infmt chain input format [IN]
 * \param outfmt chain output format [IN]
 * \param cfg a valid aout_filters_cfg_t struct or NULL.
 * \return a filters chain or NULL on failure
 */
aout_filters_t *aout_FiltersNew(vlc_object_t *obj,
                                const audio_sample_format_t *restrict infmt,
                                const audio_sample_format_t *restrict outfmt,
                                const aout_filters_cfg_t *cfg)
{
    return aout_FiltersNewWithClock(obj, NULL, infmt, outfmt, cfg);
}

#undef aout_FiltersDelete
/**
 * Destroys a chain of audio filters.
 * \param obj object used with aout_FiltersNew()
 * \param filters chain to be destroyed
 */
void aout_FiltersDelete (vlc_object_t *obj, aout_filters_t *filters)
{
    if (filters->resampler != NULL)
        aout_FiltersPipelineDestroy (&filters->resampler, 1);
    aout_FiltersPipelineDestroy (filters->tab, filters->count);
    var_DelCallback(obj, "visual", VisualizationCallback, NULL);
    if (filters->clock)
        vlc_clock_Delete(filters->clock);
    free (filters);
}

bool aout_FiltersCanResample (aout_filters_t *filters)
{
    return (filters->resampler != NULL);
}

bool aout_FiltersAdjustResampling (aout_filters_t *filters, int adjust)
{
    if (filters->resampler == NULL)
        return false;

    if (adjust)
        filters->resampling += adjust;
    else
        filters->resampling = 0;
    return filters->resampling != 0;
}

block_t *aout_FiltersPlay(aout_filters_t *filters, block_t *block, float rate)
{
    int nominal_rate = 0;

    if (rate != 1.f)
    {
        filter_t *rate_filter = filters->rate_filter;

        if (rate_filter == NULL)
            goto drop; /* Without linear, non-nominal rate is impossible. */

        /* Override input rate */
        nominal_rate = rate_filter->fmt_in.audio.i_rate;
        rate_filter->fmt_in.audio.i_rate = lroundf(nominal_rate * rate);
    }

    block = aout_FiltersPipelinePlay (filters->tab, filters->count, block);
    if (filters->resampler != NULL)
    {   /* NOTE: the resampler needs to run even if resampling is 0.
         * The decoder and output rates can still be different. */
        filters->resampler->fmt_in.audio.i_rate += filters->resampling;
        block = aout_FiltersPipelinePlay (&filters->resampler, 1, block);
        filters->resampler->fmt_in.audio.i_rate -= filters->resampling;
    }

    if (nominal_rate != 0)
    {   /* Restore input rate */
        assert (filters->rate_filter != NULL);
        filters->rate_filter->fmt_in.audio.i_rate = nominal_rate;
    }
    return block;

drop:
    block_Release (block);
    return NULL;
}

block_t *aout_FiltersDrain (aout_filters_t *filters)
{
    /* Drain the filters pipeline */
    block_t *block = aout_FiltersPipelineDrain (filters->tab, filters->count);

    if (filters->resampler != NULL)
    {
        block_t *chain = NULL;

        filters->resampler->fmt_in.audio.i_rate += filters->resampling;

        if (block)
        {
            /* Resample the drained block from the filters pipeline */
            block = aout_FiltersPipelinePlay (&filters->resampler, 1, block);
            if (block)
                block_ChainAppend (&chain, block);
        }

        /* Drain the resampler filter */
        block = aout_FiltersPipelineDrain (&filters->resampler, 1);
        if (block)
            block_ChainAppend (&chain, block);

        filters->resampler->fmt_in.audio.i_rate -= filters->resampling;

        return chain ? block_ChainGather (chain) : NULL;
    }
    else
        return block;
}

void aout_FiltersFlush (aout_filters_t *filters)
{
    aout_FiltersPipelineFlush (filters->tab, filters->count);

    if (filters->resampler != NULL)
        aout_FiltersPipelineFlush (&filters->resampler, 1);
}

void aout_FiltersChangeViewpoint (aout_filters_t *filters,
                                  const vlc_viewpoint_t *vp)
{
    aout_FiltersPipelineChangeViewpoint (filters->tab, filters->count, vp);
}
