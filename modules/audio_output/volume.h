/*****************************************************************************
 * volume.c : helper for software audio amplification
 *****************************************************************************
 * Copyright (C) 2012 Rémi Denis-Courmont
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
# include <config.h>
#endif
#include <stdio.h>
#include <math.h>
#include <vlc_common.h>
#include <vlc_aout.h>

#define add_sw_gain() \
        add_float(MODULE_STRING"-gain", 1., N_("Software gain"), \
                  N_("This linear gain will be applied in software."), true) \
             change_float_range(0., 8.)

static int aout_SoftVolumeSet(audio_output_t *aout, float volume)
{
    aout_sys_t *sys = aout->sys;
    /*
     * Cubic mapping from software volume to amplification factor.
     * This provides a good tradeoff between low and high volume ranges.
     *
     * This code is only used for the VLC software mixer. If you change this
     * formula, be sure to update the volume-capable plugins also.
     */
    float gain = volume * volume * volume;

    if (!sys->soft_mute && aout_GainRequest(aout, gain))
        return -1;
    sys->soft_gain = gain;
    if (var_InheritBool(aout, "volume-save"))
        config_PutFloat(aout, MODULE_STRING"-gain", gain);

    aout_VolumeReport(aout, volume);
    return 0;
}

static int aout_SoftMuteSet (audio_output_t *aout, bool mute)
{
    aout_sys_t *sys = aout->sys;

    if (aout_GainRequest(aout, mute ? 0.f : sys->soft_gain))
        return -1;
    sys->soft_mute = mute;

    aout_MuteReport(aout, mute);
    return 0;
}

static void aout_SoftVolumeInit(audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;
    float gain = var_InheritFloat(aout, MODULE_STRING"-gain");
    bool mute = var_InheritBool(aout, "mute");

    aout->volume_set = aout_SoftVolumeSet;
    aout->mute_set = aout_SoftMuteSet;
    sys->soft_gain = gain;
    sys->soft_mute = mute;

    aout_MuteReport(aout, mute);
    aout_VolumeReport(aout, cbrtf(gain));
}

static void aout_SoftVolumeStart (audio_output_t *aout)
{
    aout_sys_t *sys = aout->sys;

    if (aout_GainRequest(aout, sys->soft_mute ? 0.f : sys->soft_gain))
    {
        aout_MuteReport(aout, false);
        aout_VolumeReport(aout, 1.f);
    }
}
