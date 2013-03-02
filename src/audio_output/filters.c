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

static filter_t *FindFilter (vlc_object_t *obj, const char *type,
                             const char *name,
                             const audio_sample_format_t *infmt,
                             const audio_sample_format_t *outfmt)
{
    filter_t *filter = vlc_custom_create (obj, sizeof (*filter), type);
    if (unlikely(filter == NULL))
        return NULL;

    filter->fmt_in.audio = *infmt;
    filter->fmt_in.i_codec = infmt->i_format;
    filter->fmt_out.audio = *outfmt;
    filter->fmt_out.i_codec = outfmt->i_format;
    filter->p_module = module_need (filter, type, name, false);
    if (filter->p_module == NULL)
    {
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
    return FindFilter (obj, "audio converter", NULL, infmt, outfmt);
}

static filter_t *FindResampler (vlc_object_t *obj,
                                const audio_sample_format_t *infmt,
                                const audio_sample_format_t *outfmt)
{
    return FindFilter (obj, "audio resampler", "$audio-resampler",
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

#define aout_FiltersPipelineCreate(obj,f,n,m,i,o) \
        aout_FiltersPipelineCreate(VLC_OBJECT(obj),f,n,m,i,o)

static inline bool ChangeFiltersString (vlc_object_t *aout, const char *var,
                                        const char *filter, bool add)
{
    return aout_ChangeFilterString (aout, aout, var, filter, add);
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

/** Callback for visualization selection */
static int VisualizationCallback (vlc_object_t *obj, char const *var,
                                  vlc_value_t oldval, vlc_value_t newval,
                                  void *data)
{
    audio_output_t *aout = (audio_output_t *)obj;
    const char *mode = newval.psz_string;

    if (!*mode)
    {
        ChangeFiltersString (obj, "audio-visual", "goom", false);
        ChangeFiltersString (obj, "audio-visual", "visual", false);
        ChangeFiltersString (obj, "audio-visual", "projectm", false);
        ChangeFiltersString (obj, "audio-visual", "vsxu", false);
    }
    else if (!strcmp ("goom", mode))
    {
        ChangeFiltersString (obj, "audio-visual", "visual", false );
        ChangeFiltersString (obj, "audio-visual", "goom", true );
        ChangeFiltersString (obj, "audio-visual", "projectm", false );
        ChangeFiltersString (obj, "audio-visual", "vsxu", false);
    }
    else if (!strcmp ("projectm", mode))
    {
        ChangeFiltersString (obj, "audio-visual", "visual", false);
        ChangeFiltersString (obj, "audio-visual", "goom", false);
        ChangeFiltersString (obj, "audio-visual", "projectm", true);
        ChangeFiltersString (obj, "audio-visual", "vsxu", false);
    }
    else if (!strcmp ("vsxu", mode))
    {
        ChangeFiltersString (obj, "audio-visual", "visual", false);
        ChangeFiltersString (obj, "audio-visual", "goom", false);
        ChangeFiltersString (obj, "audio-visual", "projectm", false);
        ChangeFiltersString (obj, "audio-visual", "vsxu", true);
    }
    else
    {
        var_Create (obj, "effect-list", VLC_VAR_STRING);
        var_SetString (obj, "effect-list", mode);

        ChangeFiltersString (obj, "audio-visual", "goom", false);
        ChangeFiltersString (obj, "audio-visual", "visual", true);
        ChangeFiltersString (obj, "audio-visual", "projectm", false);
    }

    aout_InputRequestRestart (aout);
    (void) var; (void) oldval; (void) data;
    return VLC_SUCCESS;
}

static int EqualizerCallback (vlc_object_t *obj, char const *var,
                              vlc_value_t oldval, vlc_value_t newval,
                              void *data)
{
    audio_output_t *aout = (audio_output_t *)obj;
    char *mode = newval.psz_string;
    bool ret;

    if (!*mode)
        ret = ChangeFiltersString (obj, "audio-filter", "equalizer", false);
    else
    {
        var_Create (obj, "equalizer-preset", VLC_VAR_STRING);
        var_SetString (obj, "equalizer-preset", mode);
        ret = ChangeFiltersString (obj, "audio-filter", "equalizer", true);
    }

    /* That sucks */
    if (ret)
        aout_InputRequestRestart (aout);
    (void) var; (void) oldval; (void) data;
    return VLC_SUCCESS;
}

static vout_thread_t *RequestVout (void *data, vout_thread_t *vout,
                                   video_format_t *fmt, bool recycle)
{
    audio_output_t *aout = data;
    vout_configuration_t cfg = {
        .vout       = vout,
        .input      = NULL,
        .change_fmt = true,
        .fmt        = fmt,
        .dpb_size   = 1,
    };

    (void) recycle;
    return vout_Request (aout, &cfg);
}

vout_thread_t *aout_filter_RequestVout (filter_t *filter, vout_thread_t *vout,
                                        video_format_t *fmt)
{
    /* NOTE: This only works from audio output.
     * If you want to use visualization filters from another place, you will
     * need to add a new pf_aout_request_vout callback or store a pointer
     * to aout_request_vout_t inside filter_t (i.e. a level of indirection). */
    aout_owner_t *owner = aout_owner ((audio_output_t *)filter->p_parent);
    aout_request_vout_t *req = &owner->request_vout;

    return req->pf_request_vout (req->p_private, vout, fmt,
                                 owner->recycle_vout);
}

static filter_t *CreateFilter (vlc_object_t *parent, const char *name,
                               const audio_sample_format_t *restrict infmt,
                               const audio_sample_format_t *restrict outfmt,
                               bool visu)
{
    filter_t *filter = vlc_custom_create (parent, sizeof (*filter),
                                          "audio filter");
    if (unlikely(filter == NULL))
        return NULL;

    /*filter->p_owner = NOT NEEDED;*/
    filter->fmt_in.i_codec = infmt->i_format;
    filter->fmt_in.audio = *infmt;
    filter->fmt_out.i_codec = outfmt->i_format;
    filter->fmt_out.audio = *outfmt;

    if (!visu)
    {
        filter->p_module = module_need (filter, "audio filter", name, true);
        if (filter->p_module != NULL)
            return filter;

        /* If probing failed, formats shall not have been modified. */
        assert (AOUT_FMTS_IDENTICAL(&filter->fmt_in.audio, infmt));
        assert (AOUT_FMTS_IDENTICAL(&filter->fmt_out.audio, outfmt));
    }

    filter->p_module = module_need (filter, "visualization2", name, true);
    if (filter->p_module != NULL)
        return filter;

    vlc_object_release (filter);
    return NULL;
}

/**
 * Sets up the audio filters.
 */
int aout_FiltersNew (audio_output_t *aout,
                     const audio_sample_format_t *restrict infmt,
                     const audio_sample_format_t *restrict outfmt,
                     const aout_request_vout_t *request_vout)
{
    aout_owner_t *owner = aout_owner (aout);

    /* Prepare format structure */
    aout_FormatPrint (aout, "input", infmt);
    audio_sample_format_t input_format = *infmt;
    audio_sample_format_t output_format = *outfmt;

    /* Now add user filters */
    owner->nb_filters = 0;
    owner->rate_filter = NULL;
    owner->resampler = NULL;

    var_AddCallback (aout, "visual", VisualizationCallback, NULL);
    var_AddCallback (aout, "equalizer", EqualizerCallback, NULL);

    if (!AOUT_FMT_LINEAR(outfmt))
    {   /* Non-linear output: just convert formats, no filters/visu */
        if (!AOUT_FMTS_IDENTICAL(infmt, outfmt))
        {
            aout_FormatsPrint (aout, "pass-through:", infmt, outfmt);
            owner->filters[0] = FindConverter(VLC_OBJECT(aout), infmt, outfmt);
            if (owner->filters[0] == NULL)
            {
                msg_Err (aout, "cannot setup pass-through");
                goto error;
            }
            owner->nb_filters++;
        }
        return 0;
    }

    const char *scaletempo =
        var_InheritBool (aout, "audio-time-stretch") ? "scaletempo" : NULL;
    char *filters = var_InheritString (aout, "audio-filter");
    char *visual = var_InheritString (aout, "audio-visual");

    if (request_vout != NULL)
        owner->request_vout = *request_vout;
    else
    {
        owner->request_vout.pf_request_vout = RequestVout;
        owner->request_vout.p_private = aout;
    }
    owner->recycle_vout = (visual != NULL) && *visual;

    /* parse user filter lists */
    const char *list[AOUT_MAX_FILTERS];
    unsigned n = 0;

    if (scaletempo != NULL)
        list[n++] = scaletempo;
    if (filters != NULL)
    {
        char *p = filters, *name;
        while ((name = strsep (&p, " :")) != NULL && n < AOUT_MAX_FILTERS)
            list[n++] = name;
    }
    if (visual != NULL && n < AOUT_MAX_FILTERS)
        list[n++] = visual;

    for (unsigned i = 0; i < n; i++)
    {
        const char *name = list[i];

        if (owner->nb_filters >= AOUT_MAX_FILTERS)
        {
            msg_Err (aout, "maximum of %u filters reached", AOUT_MAX_FILTERS);
            msg_Err (aout, "cannot add user filter %s (skipped)", name);
            break;
        }

        filter_t *filter = CreateFilter (VLC_OBJECT(aout), name,
                                         &input_format, &output_format,
                                         name == visual);
        if (filter == NULL)
        {
            msg_Dbg (aout, "cannot add user filter %s (skipped)", name);
            continue;
        }

        /* convert to the filter input format if necessary */
        if (aout_FiltersPipelineCreate (aout, owner->filters,
                                        &owner->nb_filters,
                                        AOUT_MAX_FILTERS - 1,
                                        &input_format, &filter->fmt_in.audio))
        {
            msg_Err (aout, "cannot add user filter %s (skipped)", name);
            module_unneed (filter, filter->p_module);
            vlc_object_release (filter);
            continue;
        }

        assert (owner->nb_filters < AOUT_MAX_FILTERS);
        owner->filters[owner->nb_filters++] = filter;
        input_format = filter->fmt_out.audio;

        if (name == scaletempo)
            owner->rate_filter = filter;
    }
    free (visual);
    free (filters);

    /* convert to the output format (minus resampling) if necessary */
    output_format.i_rate = input_format.i_rate;
    if (aout_FiltersPipelineCreate (aout, owner->filters, &owner->nb_filters,
                                    AOUT_MAX_FILTERS,
                                    &input_format, &output_format))
    {
        msg_Err (aout, "cannot setup filtering pipeline");
        goto error;
    }
    input_format = output_format;

    /* insert the resampler */
    output_format.i_rate = outfmt->i_rate;
    assert (AOUT_FMTS_IDENTICAL(&output_format, outfmt));
    owner->resampler = FindResampler (VLC_OBJECT(aout), &input_format,
                                      &output_format);
    if (owner->resampler == NULL && input_format.i_rate != outfmt->i_rate)
    {
        msg_Err (aout, "cannot setup a resampler");
        goto error;
    }
    if (owner->rate_filter == NULL)
        owner->rate_filter = owner->resampler;
    owner->resampling = 0;

    return 0;

error:
    aout_FiltersPipelineDestroy (owner->filters, owner->nb_filters);
    var_DelCallback (aout, "equalizer", EqualizerCallback, NULL);
    var_DelCallback (aout, "visual", VisualizationCallback, NULL);
    return -1;
}

/**
 * Destroys the audio filters.
 */
void aout_FiltersDelete (audio_output_t *aout)
{
    aout_owner_t *owner = aout_owner (aout);

    if (owner->resampler != NULL)
        aout_FiltersPipelineDestroy (&owner->resampler, 1);
    aout_FiltersPipelineDestroy (owner->filters, owner->nb_filters);
    var_DelCallback (aout, "equalizer", EqualizerCallback, NULL);
    var_DelCallback (aout, "visual", VisualizationCallback, NULL);

    /* XXX We need to update recycle_vout before calling
     * aout_FiltersPipelineDestroy().
     * FIXME There may be a race condition if audio-visual is updated between
     * aout_FiltersDestroy() and the next aout_FiltersNew().
     */
    char *visual = var_InheritString (aout, "audio-visual");
    owner->recycle_vout = (visual != NULL) && *visual;
    free (visual);
}

bool aout_FiltersAdjustResampling (audio_output_t *aout, int adjust)
{
    aout_owner_t *owner = aout_owner (aout);

    if (owner->resampler == NULL)
        return false;

    if (adjust)
        owner->resampling += adjust;
    else
        owner->resampling = 0;
    return owner->resampling != 0;
}

block_t *aout_FiltersPlay (audio_output_t *aout, block_t *block, int rate)
{
    aout_owner_t *owner = aout_owner (aout);
    int nominal_rate = 0;

    if (rate != INPUT_RATE_DEFAULT)
    {
        filter_t *rate_filter = owner->rate_filter;

        if (rate_filter == NULL)
            goto drop; /* Without linear, non-nominal rate is impossible. */

        /* Override input rate */
        nominal_rate = rate_filter->fmt_in.audio.i_rate;
        rate_filter->fmt_in.audio.i_rate =
            (nominal_rate * INPUT_RATE_DEFAULT) / rate;
    }

    block = aout_FiltersPipelinePlay (owner->filters, owner->nb_filters,
                                      block);
    if (owner->resampler != NULL)
    {   /* NOTE: the resampler needs to run even if resampling is 0.
         * The decoder and output rates can still be different. */
        owner->resampler->fmt_in.audio.i_rate += owner->resampling;
        block = aout_FiltersPipelinePlay (&owner->resampler, 1, block);
        owner->resampler->fmt_in.audio.i_rate -= owner->resampling;
    }

    if (nominal_rate != 0)
    {   /* Restore input rate */
        assert (owner->rate_filter != NULL);
        owner->rate_filter->fmt_in.audio.i_rate = nominal_rate;
    }
    return block;

drop:
    block_Release (block);
    return NULL;
}
