/*****************************************************************************
 * spatialaudio.cpp : Ambisonics audio renderer and binauralizer filter
 *****************************************************************************
 * Copyright © 2017 VLC authors and VideoLAN
 *
 * Authors: Adrien Maglo <magsoft@videolan.org>
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

#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_filter.h>
#include <vlc_viewpoint.h>

#include <new>
#include <vector>
#include <sstream>

#include <spatialaudio/Ambisonics.h>
#include <spatialaudio/SpeakersBinauralizer.h>

#define CFG_PREFIX "spatialaudio-"

#define DEFAULT_HRTF_PATH "hrtfs" DIR_SEP "dodeca_and_7channel_3DSL_HRTF.sofa"

#define HRTF_FILE_TEXT N_("HRTF file for the binauralization")
#define HRTF_FILE_LONGTEXT N_("Custom HRTF (Head-related transfer function) file " \
                              "in the SOFA format.")

#define HEADPHONES_TEXT N_("Headphones mode (binaural)")
#define HEADPHONES_LONGTEXT N_("If the output is stereo, render ambisonics " \
                               "with the binaural decoder.")

static int OpenBinauralizer(vlc_object_t *p_this);
static int Open( vlc_object_t * );
static void Close( vlc_object_t * );
static void Flush( filter_t * );

vlc_module_begin()
    set_shortname("Spatialaudio")
    set_description(N_("Ambisonics renderer and binauralizer"))
    set_capability("audio renderer", 1)
    set_category(CAT_AUDIO)
    set_subcategory(SUBCAT_AUDIO_AFILTER)
    set_callbacks(Open, Close)
    add_bool(CFG_PREFIX "headphones", false,
             HEADPHONES_TEXT, HEADPHONES_LONGTEXT, true)
    add_loadfile("hrtf-file", NULL, HRTF_FILE_TEXT, HRTF_FILE_LONGTEXT)
    add_shortcut("ambisonics")

    add_submodule()
    set_shortname(N_("Binauralizer"))
    set_capability("audio filter", 0)
    set_callbacks(OpenBinauralizer, Close)
    add_shortcut("binauralizer")
vlc_module_end()

#define AMB_BLOCK_TIME_LEN 1024

#define AMB_MAX_ORDER 3

struct filter_spatialaudio
{
    filter_spatialaudio()
        : speakers(NULL)
        , i_inputPTS(0)
        , inBuf(NULL)
        , outBuf(NULL)
    {}
    ~filter_spatialaudio()
    {
        delete[] speakers;
        if (inBuf != NULL)
            for (unsigned i = 0; i < i_inputNb; ++i)
                free(inBuf[i]);
        free(inBuf);

        if (outBuf != NULL)
            for (unsigned i = 0; i < i_outputNb; ++i)
                free(outBuf[i]);
        free(outBuf);
    }

    enum
    {
        AMBISONICS_DECODER, // Ambisonics decoding module
        AMBISONICS_BINAURAL_DECODER, // Ambisonics decoding module using binaural
        BINAURALIZER // Binauralizer module
    } mode;

    CAmbisonicBinauralizer binauralDecoder;
    SpeakersBinauralizer binauralizer;
    CAmbisonicDecoder speakerDecoder;
    CAmbisonicProcessor processor;
    CAmbisonicZoomer zoomer;

    CAmbisonicSpeaker *speakers;

    std::vector<float> inputSamples;
    vlc_tick_t i_inputPTS;
    unsigned i_order;
    unsigned i_nondiegetic;
    unsigned i_lr_channels; // number of physical left/right channel pairs

    float** inBuf;
    float** outBuf;
    unsigned i_inputNb;
    unsigned i_outputNb;

    /* View point. */
    float f_teta;
    float f_phi;
    float f_roll;
    float f_zoom;
};

static std::string getHRTFPath(filter_t *p_filter)
{
    std::string HRTFPath;

    char *userHRTFPath = var_InheritString(p_filter, "hrtf-file");

    if (userHRTFPath != NULL)
    {
        HRTFPath = std::string(userHRTFPath);
        free(userHRTFPath);
    }
    else
    {
        char *dataDir = config_GetSysPath(VLC_PKG_DATA_DIR, DEFAULT_HRTF_PATH);
        if (dataDir != NULL)
        {
            HRTFPath = std::string(dataDir);
            free(dataDir);
        }
    }

    return HRTFPath;
}

static block_t *Mix( filter_t *p_filter, block_t *p_buf )
{
    filter_spatialaudio *p_sys = reinterpret_cast<filter_spatialaudio *>(p_filter->p_sys);

    const size_t i_prevSize = p_sys->inputSamples.size();
    p_sys->inputSamples.resize(i_prevSize + p_buf->i_nb_samples * p_sys->i_inputNb);
    memcpy((char*)(p_sys->inputSamples.data() + i_prevSize), (char*)p_buf->p_buffer, p_buf->i_buffer);

    const size_t i_inputBlockSize = sizeof(float) * p_sys->i_inputNb * AMB_BLOCK_TIME_LEN;
    const size_t i_outputBlockSize = sizeof(float) * p_sys->i_outputNb * AMB_BLOCK_TIME_LEN;
    const size_t i_nbBlocks = p_sys->inputSamples.size() * sizeof(float) / i_inputBlockSize;

    block_t *p_out_buf = block_Alloc(i_outputBlockSize * i_nbBlocks);
    if (unlikely(p_out_buf == NULL))
    {
        block_Release(p_buf);
        return NULL;
    }

    p_out_buf->i_nb_samples = i_nbBlocks * AMB_BLOCK_TIME_LEN;
    if (p_sys->i_inputPTS == 0)
        p_out_buf->i_pts = p_buf->i_pts;
    else
        p_out_buf->i_pts = p_sys->i_inputPTS;
    p_out_buf->i_dts = p_out_buf->i_pts;
    p_out_buf->i_length = vlc_tick_from_samples(p_out_buf->i_nb_samples, p_filter->fmt_in.audio.i_rate);

    float *p_dest = (float *)p_out_buf->p_buffer;
    const float *p_src = (float *)p_sys->inputSamples.data();

    for (unsigned b = 0; b < i_nbBlocks; ++b)
    {
        for (unsigned i = 0; i < p_sys->i_inputNb; ++i)
        {
            for (unsigned j = 0; j < AMB_BLOCK_TIME_LEN; ++j)
            {
                float val = p_src[(b * AMB_BLOCK_TIME_LEN + j) * p_sys->i_inputNb + i];
                p_sys->inBuf[i][j] = val;
            }
        }

        // Compute
        switch (p_sys->mode)
        {
            case filter_spatialaudio::BINAURALIZER:
                p_sys->binauralizer.Process(p_sys->inBuf, p_sys->outBuf);
                break;
            case filter_spatialaudio::AMBISONICS_DECODER:
            case filter_spatialaudio::AMBISONICS_BINAURAL_DECODER:
            {
                CBFormat inData;
                inData.Configure(p_sys->i_order, true, AMB_BLOCK_TIME_LEN);

                for (unsigned i = 0; i < p_sys->i_inputNb - p_sys->i_nondiegetic; ++i)
                    inData.InsertStream(p_sys->inBuf[i], i, AMB_BLOCK_TIME_LEN);

                Orientation ori(p_sys->f_teta, p_sys->f_phi, p_sys->f_roll);
                p_sys->processor.SetOrientation(ori);
                p_sys->processor.Refresh();
                p_sys->processor.Process(&inData, inData.GetSampleCount());

                p_sys->zoomer.SetZoom(p_sys->f_zoom);
                p_sys->zoomer.Refresh();
                p_sys->zoomer.Process(&inData, inData.GetSampleCount());

                if (p_sys->mode == filter_spatialaudio::AMBISONICS_DECODER)
                    p_sys->speakerDecoder.Process(&inData, inData.GetSampleCount(), p_sys->outBuf);
                else
                    p_sys->binauralDecoder.Process(&inData, p_sys->outBuf);
                break;
            }
            default:
                vlc_assert_unreachable();
        }

        // Interleave the results.
        for (unsigned i = 0; i < p_sys->i_outputNb; ++i)
            for (unsigned j = 0; j < AMB_BLOCK_TIME_LEN; ++j)
                p_dest[(b * AMB_BLOCK_TIME_LEN + j) * p_sys->i_outputNb + i] = p_sys->outBuf[i][j];

        if (p_sys->i_nondiegetic == 2)
        {
            for (unsigned i = 0; i < p_sys->i_lr_channels * 2; i += 2)
                for (unsigned j = 0; j < AMB_BLOCK_TIME_LEN; ++j)
                {
                    p_dest[(b * AMB_BLOCK_TIME_LEN + j) * p_sys->i_outputNb + i] =
                            p_dest[(b * AMB_BLOCK_TIME_LEN + j) * p_sys->i_outputNb + i]  / 2.f
                            + p_sys->inBuf[p_sys->i_inputNb - 2][j] / 2.f; //left
                    p_dest[(b * AMB_BLOCK_TIME_LEN + j) * p_sys->i_outputNb + i + 1] =
                            p_dest[(b * AMB_BLOCK_TIME_LEN + j) * p_sys->i_outputNb + i + 1]  / 2.f
                            + p_sys->inBuf[p_sys->i_inputNb - 1][j] / 2.f; //right
                }
        }
    }

    p_sys->inputSamples.erase(p_sys->inputSamples.begin(),
                              p_sys->inputSamples.begin() + i_inputBlockSize * i_nbBlocks / sizeof(float));

    assert(p_sys->inputSamples.size() < i_inputBlockSize);

    p_sys->i_inputPTS = p_out_buf->i_pts + p_out_buf->i_length;

    block_Release(p_buf);
    return p_out_buf;
}

static void Flush( filter_t *p_filter )
{
    filter_spatialaudio *p_sys = reinterpret_cast<filter_spatialaudio *>(p_filter->p_sys);
    p_sys->inputSamples.clear();
    p_sys->i_inputPTS = 0;
}

static void ChangeViewpoint( filter_t *p_filter, const vlc_viewpoint_t *p_vp)
{
    filter_spatialaudio *p_sys = reinterpret_cast<filter_spatialaudio *>(p_filter->p_sys);

#define RAD(d) ((float) ((d) * M_PI / 180.f))
    p_sys->f_teta = -RAD(p_vp->yaw);
    p_sys->f_phi = RAD(p_vp->pitch);
    p_sys->f_roll = RAD(p_vp->roll);

    if (p_vp->fov >= FIELD_OF_VIEW_DEGREES_DEFAULT)
        p_sys->f_zoom = 0.f; // no unzoom as it does not really make sense.
    else
        p_sys->f_zoom = (FIELD_OF_VIEW_DEGREES_DEFAULT - p_vp->fov) / (FIELD_OF_VIEW_DEGREES_DEFAULT - FIELD_OF_VIEW_DEGREES_MIN);
#undef RAD
}

static int allocateBuffers(filter_spatialaudio *p_sys)
{
    p_sys->inBuf = (float**)calloc(p_sys->i_inputNb, sizeof(float*));
    if (p_sys->inBuf == NULL)
        return VLC_ENOMEM;

    for (unsigned i = 0; i < p_sys->i_inputNb; ++i)
    {
        p_sys->inBuf[i] = (float *)vlc_alloc(AMB_BLOCK_TIME_LEN, sizeof(float));
        if (p_sys->inBuf[i] == NULL)
            return VLC_ENOMEM;
    }

    p_sys->outBuf = (float**)calloc(p_sys->i_outputNb, sizeof(float*));
    if (p_sys->outBuf == NULL)
        return VLC_ENOMEM;

    for (unsigned i = 0; i < p_sys->i_outputNb; ++i)
    {
        p_sys->outBuf[i] = (float *)vlc_alloc(AMB_BLOCK_TIME_LEN, sizeof(float));
        if (p_sys->outBuf[i] == NULL)
            return VLC_ENOMEM;
    }

    return VLC_SUCCESS;
}

static int OpenBinauralizer(vlc_object_t *p_this)
{
    filter_t *p_filter = (filter_t *)p_this;
    audio_format_t *infmt = &p_filter->fmt_in.audio;
    audio_format_t *outfmt = &p_filter->fmt_out.audio;

    filter_spatialaudio *p_sys = new(std::nothrow)filter_spatialaudio();
    if (p_sys == NULL)
        return VLC_ENOMEM;

    p_sys->mode = filter_spatialaudio::BINAURALIZER;
    p_sys->i_inputNb = p_filter->fmt_in.audio.i_channels;
    p_sys->i_outputNb = 2;
    p_sys->i_lr_channels = 1;

    if (allocateBuffers(p_sys) != VLC_SUCCESS)
    {
        delete p_sys;
        return VLC_ENOMEM;
    }

    unsigned s = 0;
    p_sys->speakers = new(std::nothrow)CAmbisonicSpeaker[infmt->i_channels]();
    if (!p_sys->speakers)
    {
        delete p_sys;
        return VLC_ENOMEM;
    }

    p_sys->speakers[s++].SetPosition({DegreesToRadians(30), 0.f, 1.f});
    p_sys->speakers[s++].SetPosition({DegreesToRadians(-30), 0.f, 1.f});

    if ((infmt->i_physical_channels & AOUT_CHANS_MIDDLE) == AOUT_CHANS_MIDDLE)
    {
        /* Middle */
        p_sys->speakers[s++].SetPosition({DegreesToRadians(110), 0.f, 1.f});
        p_sys->speakers[s++].SetPosition({DegreesToRadians(-110), 0.f, 1.f});
    }

    if ((infmt->i_physical_channels & AOUT_CHANS_REAR) == AOUT_CHANS_REAR)
    {
        /* Rear */
        p_sys->speakers[s++].SetPosition({DegreesToRadians(145), 0.f, 1.f});
        p_sys->speakers[s++].SetPosition({DegreesToRadians(-145), 0.f, 1.f});
    }

    if ((infmt->i_physical_channels & AOUT_CHAN_CENTER) == AOUT_CHAN_CENTER)
        p_sys->speakers[s++].SetPosition({DegreesToRadians(0), 0.f, 1.f});

    if ((infmt->i_physical_channels & AOUT_CHAN_LFE) == AOUT_CHAN_LFE)
        p_sys->speakers[s++].SetPosition({DegreesToRadians(0), 0.f, 0.5f});

    std::string HRTFPath = getHRTFPath(p_filter);
    msg_Dbg(p_filter, "Using the HRTF file: %s", HRTFPath.c_str());

    unsigned i_tailLength = 0;
    if (!p_sys->binauralizer.Configure(p_filter->fmt_in.audio.i_rate, AMB_BLOCK_TIME_LEN,
                                       p_sys->speakers, infmt->i_channels, i_tailLength,
                                       HRTFPath))
    {
        msg_Err(p_filter, "Error creating the binauralizer.");
        delete p_sys;
        return VLC_EGENERIC;
    }
    p_sys->binauralizer.Reset();

    outfmt->i_format = infmt->i_format = VLC_CODEC_FL32;
    outfmt->i_rate = infmt->i_rate;
    outfmt->i_physical_channels = AOUT_CHANS_STEREO;
    aout_FormatPrepare(infmt);
    aout_FormatPrepare(outfmt);

    p_filter->p_sys = p_sys;
    p_filter->pf_audio_filter = Mix;
    p_filter->pf_flush = Flush;
    p_filter->pf_change_viewpoint = ChangeViewpoint;

    return VLC_SUCCESS;
}

static int Open(vlc_object_t *p_this)
{
    filter_t *p_filter = (filter_t *)p_this;
    audio_format_t *infmt = &p_filter->fmt_in.audio;
    audio_format_t *outfmt = &p_filter->fmt_out.audio;

    assert(infmt->channel_type != outfmt->channel_type);

    if (infmt->channel_type != AUDIO_CHANNEL_TYPE_AMBISONICS)
        return VLC_EGENERIC;

    if (infmt->i_format != VLC_CODEC_FL32 || outfmt->i_format != VLC_CODEC_FL32)
        return VLC_EGENERIC;

    //support order 1 to 3
    if ( infmt->i_channels < 4 || infmt->i_channels > ( (AMB_MAX_ORDER + 1) * (AMB_MAX_ORDER + 1) + 2 ) )
    {
        msg_Err(p_filter, "Unsupported number of Ambisonics channels");
        return VLC_EGENERIC;
    }

    filter_spatialaudio *p_sys = new(std::nothrow)filter_spatialaudio();
    if (p_sys == NULL)
        return VLC_ENOMEM;

    p_sys->f_teta = 0.f;
    p_sys->f_phi = 0.f;
    p_sys->f_roll = 0.f;
    p_sys->f_zoom = 0.f;
    p_sys->i_inputNb = p_filter->fmt_in.audio.i_channels;
    p_sys->i_outputNb = p_filter->fmt_out.audio.i_channels;

    if (allocateBuffers(p_sys) != VLC_SUCCESS)
    {
        delete p_sys;
        return VLC_ENOMEM;
    }

    int i_sqrt_channels = 1;
    while( ( i_sqrt_channels < ( AMB_MAX_ORDER + 2 ) )
           && ( i_sqrt_channels * i_sqrt_channels <= infmt->i_channels ) )
        i_sqrt_channels++;
    i_sqrt_channels--;

    p_sys->i_order = i_sqrt_channels - 1;
    p_sys->i_nondiegetic = infmt->i_channels - i_sqrt_channels * i_sqrt_channels;

    if ( p_sys->i_nondiegetic != 0 && p_sys->i_nondiegetic != 2 )
    {
        msg_Err(p_filter, "Invalid number of non-diegetic Ambisonics channels %i", p_sys->i_nondiegetic);
        delete p_sys;
        return VLC_EGENERIC;
    }

    msg_Dbg(p_filter, "Order: %d %d %d", p_sys->i_order, p_sys->i_nondiegetic, infmt->i_channels);

    static const char *const options[] = { "headphones", NULL };
    config_ChainParse(p_filter, CFG_PREFIX, options, p_filter->p_cfg);

    unsigned i_tailLength = 0;
    if (p_filter->fmt_out.audio.i_channels == 2
     && var_InheritBool(p_filter, CFG_PREFIX "headphones"))
    {
        p_sys->mode = filter_spatialaudio::AMBISONICS_BINAURAL_DECODER;

        std::string HRTFPath = getHRTFPath(p_filter);
        msg_Dbg(p_filter, "Using the HRTF file: %s", HRTFPath.c_str());

        if (!p_sys->binauralDecoder.Configure(p_sys->i_order, true,
                p_filter->fmt_in.audio.i_rate, AMB_BLOCK_TIME_LEN, i_tailLength,
                HRTFPath))
        {
            msg_Err(p_filter, "Error creating the binaural decoder.");
            delete p_sys;
            return VLC_EGENERIC;
        }
        p_sys->binauralDecoder.Reset();
    }
    else
    {
        p_sys->mode = filter_spatialaudio::AMBISONICS_DECODER;

        unsigned i_nbChannels = aout_FormatNbChannels(&p_filter->fmt_out.audio);
        if (i_nbChannels == 1
         || !p_sys->speakerDecoder.Configure(p_sys->i_order, true,
                                             kAmblib_CustomSpeakerSetUp,
                                             i_nbChannels))
        {
            msg_Err(p_filter, "Error creating the Ambisonics decoder.");
            delete p_sys;
            return VLC_EGENERIC;
        }

        /* Speaker setup, inspired from:
         * https://www.dolby.com/us/en/guide/surround-sound-speaker-setup/7-1-setup.html
         * The position must follow the order of pi_vlc_chan_order_wg4 */
        unsigned s = 0;

        p_sys->speakerDecoder.SetPosition(s++, {DegreesToRadians(30), 0.f, 1.f});
        p_sys->speakerDecoder.SetPosition(s++, {DegreesToRadians(-30), 0.f, 1.f});
        p_sys->i_lr_channels = 1;

        if ((outfmt->i_physical_channels & AOUT_CHANS_MIDDLE) == AOUT_CHANS_MIDDLE)
        {
            p_sys->speakerDecoder.SetPosition(s++, {DegreesToRadians(110), 0.f, 1.f});
            p_sys->speakerDecoder.SetPosition(s++, {DegreesToRadians(-110), 0.f, 1.f});
            p_sys->i_lr_channels++;
        }

        if ((outfmt->i_physical_channels & AOUT_CHANS_REAR) == AOUT_CHANS_REAR)
        {
            p_sys->speakerDecoder.SetPosition(s++, {DegreesToRadians(145), 0.f, 1.f});
            p_sys->speakerDecoder.SetPosition(s++, {DegreesToRadians(-145), 0.f, 1.f});
            p_sys->i_lr_channels++;
        }

        if ((outfmt->i_physical_channels & AOUT_CHAN_CENTER) == AOUT_CHAN_CENTER)
            p_sys->speakerDecoder.SetPosition(s++, {DegreesToRadians(0), 0.f, 1.f});

        if ((outfmt->i_physical_channels & AOUT_CHAN_LFE) == AOUT_CHAN_LFE)
            p_sys->speakerDecoder.SetPosition(s++, {DegreesToRadians(0), 0.f, 0.5f});

        /* Check we have setup the right number of speaker. */
        assert(s == i_nbChannels);

        p_sys->speakerDecoder.Refresh();
    }

    if (!p_sys->processor.Configure(p_sys->i_order, true, AMB_BLOCK_TIME_LEN, 0))
    {
        msg_Err(p_filter, "Error creating the ambisonic processor.");
        delete p_sys;
        return VLC_EGENERIC;
    }

    if (!p_sys->zoomer.Configure(p_sys->i_order, true, 0))
    {
        msg_Err(p_filter, "Error creating the ambisonic zoomer.");
        delete p_sys;
        return VLC_EGENERIC;
    }

    p_filter->p_sys = p_sys;
    p_filter->pf_audio_filter = Mix;
    p_filter->pf_flush = Flush;
    p_filter->pf_change_viewpoint = ChangeViewpoint;

    return VLC_SUCCESS;
}

static void Close(vlc_object_t *p_this)
{
    filter_t *p_filter = (filter_t *)p_this;

    filter_spatialaudio *p_sys = reinterpret_cast<filter_spatialaudio *>(p_filter->p_sys);
    delete p_sys;
}
