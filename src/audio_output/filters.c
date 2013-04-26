/*****************************************************************************
 * filters.c : audio output filters management
 *****************************************************************************
 * Copyright (C) 2002-2007 VLC authors and VideoLAN
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_dialog.h>
#include <vlc_modules.h>
#include <vlc_aout.h>
#include <vlc_filter.h>
#include <vlc_vout.h>                  /* for vout_Request */
#include <vlc_input.h>

#include <libvlc.h>
#include "aout_internal.h"

static filter_t *CreateFilter (vlc_object_t *obj, const char *type,
                               const char *name, filter_owner_sys_t *owner,
                               const audio_sample_format_t *infmt,
                               const audio_sample_format_t *outfmt)
{
    filter_t *filter = vlc_custom_create (obj, sizeof (*filter), type);
    if (unlikely(filter == NULL))
        return NULL;

    filter->p_owner = owner;
    filter->fmt_in.audio = *infmt;
    filter->fmt_in.i_codec = infmt->i_format;
    filter->fmt_out.audio = *outfmt;
    filter->fmt_out.i_codec = outfmt->i_format;
    filter->p_module = module_need (filter, type, name, false);
    if (filter->p_module == NULL)
    {
        /* If probing failed, formats shall not have been modified. */
        assert (AOUT_FMTS_IDENTICAL(&filter->fmt_in.audio, infmt));
        assert (AOUT_FMTS_IDENTICAL(&filter->fmt_out.audio, outfmt));
        vlc_object_release (filter);
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
    return CreateFilter (obj, "audio converter", NULL, NULL, infmt, outfmt);
}

static filter_t *FindResampler (vlc_object_t *obj,
                                const audio_sample_format_t *infmt,
                                const audio_sample_format_t *outfmt)
{
    return CreateFilter (obj, "audio resampler", "$audio-resampler", NULL,
                         infmt, outfmt);
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
        vlc_object_release( p_filter );
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
                                 const audio_sample_format_t *restrict outfmt)
{
    aout_FormatsPrint (obj, "conversion:", infmt, outfmt);
    max -= *count;
    filters += *count;

    /* There is a lot of second guessing on what the conversion plugins can
     * and cannot do. This seems hardly avoidable, the conversion problem need
     * to be reduced somehow. */
    audio_sample_format_t input = *infmt;
    unsigned n = 0;

    /* Encapsulate or decode non-linear formats */
    if (!AOUT_FMT_LINEAR(infmt) && infmt->i_format != outfmt->i_format)
    {
        if (n == max)
            goto overflow;

        filter_t *f = TryFormat (obj, VLC_CODEC_S32N, &input);
        if (f == NULL)
            f = TryFormat (obj, VLC_CODEC_FL32, &input);
        if (f == NULL)
        {
            msg_Err (obj, "cannot find %s for conversion pipeline",
                     "decoder");
            goto error;
        }

        filters[n++] = f;
    }
    assert (AOUT_FMT_LINEAR(&input));

    /* Remix channels */
    if (infmt->i_physical_channels != outfmt->i_physical_channels
     || infmt->i_original_channels != outfmt->i_original_channels)
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
        output.i_original_channels = outfmt->i_original_channels;
        aout_FormatPrepare (&output);

        filter_t *f = FindConverter (obj, &input, &output);
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
    dialog_Fatal (obj, _("Audio filtering failed"),
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

#define AOUT_MAX_FILTERS 10

struct aout_filters
{
    filter_t *rate_filter; /**< The filter adjusting samples count
        (either the scaletempo filter or a resampler) */
    filter_t *resampler; /**< The resampler */
    int resampling; /**< Current resampling (Hz) */

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
     && strcasecmp (mode, "projectm") && strcasecmp (mode, "vsxu"))
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

static int EqualizerCallback (vlc_object_t *obj, const char *var,
                              vlc_value_t oldval, vlc_value_t newval,
                              void *data)
{
    const char *val = newval.psz_string;

    if (*val)
    {
        var_Create (obj, "equalizer-preset", VLC_VAR_STRING);
        var_SetString (obj, "equalizer-preset", val);
    }

    if (aout_ChangeFilterString (obj, obj, "audio-filter", "equalizer", *val))
        aout_InputRequestRestart ((audio_output_t *)obj); /* <- That sucks! */

    (void) var; (void) oldval; (void) data;
    return VLC_SUCCESS;
}

vout_thread_t *aout_filter_RequestVout (filter_t *filter, vout_thread_t *vout,
                                        video_format_t *fmt)
{
    /* NOTE: This only works from aout_filters_t.
     * If you want to use visualization filters from another place, you will
     * need to add a new pf_aout_request_vout callback or store a pointer
     * to aout_request_vout_t inside filter_t (i.e. a level of indirection). */
    const aout_request_vout_t *req = (void *)filter->p_owner;
    char *visual = var_InheritString (filter->p_parent, "audio-visual");
    bool recycle = (visual != NULL) && strcasecmp(visual, "none");
    free (visual);

    return req->pf_request_vout (req->p_private, vout, fmt, recycle);
}

static int AppendFilter(vlc_object_t *obj, const char *type, const char *name,
                        aout_filters_t *restrict filters, const void *owner,
                        audio_sample_format_t *restrict infmt,
                        const audio_sample_format_t *restrict outfmt)
{
    const unsigned max = sizeof (filters->tab) / sizeof (filters->tab[0]);
    if (filters->count >= max)
    {
        msg_Err (obj, "maximum of %u filters reached", max);
        return -1;
    }

    filter_t *filter = CreateFilter (obj, type, name,
                                     (void *)owner, infmt, outfmt);
    if (filter == NULL)
    {
        msg_Err (obj, "cannot add user %s \"%s\" (skipped)", type, name);
        return -1;
    }

    /* convert to the filter input format if necessary */
    if (aout_FiltersPipelineCreate (obj, filters->tab, &filters->count,
                                    max - 1, infmt, &filter->fmt_in.audio))
    {
        msg_Err (filter, "cannot add user %s \"%s\" (skipped)", type, name);
        module_unneed (filter, filter->p_module);
        vlc_object_release (filter);
        return -1;
    }

    assert (filters->count < max);
    filters->tab[filters->count] = filter;
    filters->count++;
    *infmt = filter->fmt_out.audio;
    return 0;
}

#undef aout_FiltersNew
/**
 * Sets a chain of audio filters up.
 * \param obj parent object for the filters
 * \param infmt chain input format [IN]
 * \param outfmt chain output format [IN]
 * \param request_vout visualization video output request callback
 * \return a filters chain or NULL on failure
 *
 * \note
 * *request_vout (if not NULL) must remain valid until aout_FiltersDelete().
 *
 * \bug
 * If request_vout is non NULL, obj is assumed to be an audio_output_t pointer.
 */
aout_filters_t *aout_FiltersNew (vlc_object_t *obj,
                                 const audio_sample_format_t *restrict infmt,
                                 const audio_sample_format_t *restrict outfmt,
                                 const aout_request_vout_t *request_vout)
{
    aout_filters_t *filters = malloc (sizeof (*filters));
    if (unlikely(filters == NULL))
        return NULL;

    filters->rate_filter = NULL;
    filters->resampler = NULL;
    filters->resampling = 0;
    filters->count = 0;

    /* Prepare format structure */
    aout_FormatPrint (obj, "input", infmt);
    audio_sample_format_t input_format = *infmt;
    audio_sample_format_t output_format = *outfmt;

    /* Callbacks (before reading values and also before return statement) */
    if (request_vout != NULL)
    {
        var_AddCallback (obj, "equalizer", EqualizerCallback, NULL);
        var_AddCallback (obj, "visual", VisualizationCallback, NULL);
    }

    /* Now add user filters */
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

    /* parse user filter lists */
    if (var_InheritBool (obj, "audio-time-stretch"))
    {
        if (AppendFilter(obj, "audio filter", "scaletempo",
                         filters, NULL, &input_format, &output_format) == 0)
            filters->rate_filter = filters->tab[filters->count - 1];
    }

    char *str = var_InheritString (obj, "audio-filter");
    if (str != NULL)
    {
        char *p = str, *name;
        while ((name = strsep (&p, " :")) != NULL)
        {
            AppendFilter(obj, "audio filter", name, filters,
                         NULL, &input_format, &output_format);
        }
        free (str);
    }

    if (request_vout != NULL)
    {
        char *visual = var_InheritString (obj, "audio-visual");
        if (visual != NULL && strcasecmp (visual, "none"))
            AppendFilter(obj, "visualization", visual, filters,
                         request_vout, &input_format, &output_format);
        free (visual);
    }

    /* convert to the output format (minus resampling) if necessary */
    output_format.i_rate = input_format.i_rate;
    if (aout_FiltersPipelineCreate (obj, filters->tab, &filters->count,
                              AOUT_MAX_FILTERS, &input_format, &output_format))
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
    var_DelCallback (obj, "equalizer", EqualizerCallback, NULL);
    var_DelCallback (obj, "visual", VisualizationCallback, NULL);
    free (filters);
    return NULL;
}

#undef aout_FiltersDelete
/**
 * Destroys a chain of audio filters.
 * \param obj object used with aout_FiltersNew()
 * \param filters chain to be destroyed
 * \bug
 * obj must be NULL iff request_vout was NULL in aout_FiltersNew()
 * (this implies obj is an audio_output_t pointer if non NULL).
 */
void aout_FiltersDelete (vlc_object_t *obj, aout_filters_t *filters)
{
    if (filters->resampler != NULL)
        aout_FiltersPipelineDestroy (&filters->resampler, 1);
    aout_FiltersPipelineDestroy (filters->tab, filters->count);
    if (obj != NULL)
    {
        var_DelCallback (obj, "equalizer", EqualizerCallback, NULL);
        var_DelCallback (obj, "visual", VisualizationCallback, NULL);
    }
    free (filters);
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

block_t *aout_FiltersPlay (aout_filters_t *filters, block_t *block, int rate)
{
    int nominal_rate = 0;

    if (rate != INPUT_RATE_DEFAULT)
    {
        filter_t *rate_filter = filters->rate_filter;

        if (rate_filter == NULL)
            goto drop; /* Without linear, non-nominal rate is impossible. */

        /* Override input rate */
        nominal_rate = rate_filter->fmt_in.audio.i_rate;
        rate_filter->fmt_in.audio.i_rate =
            (nominal_rate * INPUT_RATE_DEFAULT) / rate;
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
