/**
 * @file access.c
 * @brief Digital broadcasting input module for VLC media player
 */
/*****************************************************************************
 * Copyright © 2011 Rémi Denis-Courmont
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 ****************************************************************************/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_access.h>
#include <vlc_plugin.h>
#include <vlc_dialog.h>

#include "dtv/dtv.h"

#define CACHING_TEXT N_("Caching value (ms)")
#define CACHING_LONGTEXT N_( \
    "The cache size (delay) for digital broadcasts (in milliseconds).")

#define ADAPTER_TEXT N_("DVB adapter")
#define ADAPTER_LONGTEXT N_( \
    "If there is more than one digital broadcasting adapter, " \
    "the adapter number must be selected. Numbering start from zero.")

#define DEVICE_TEXT N_("DVB device")
#define DEVICE_LONGTEXT N_( \
    "If the selected adapter has more than one tuner, " \
    "the tuner number must be selected. Numbering start from zero.")

#define BUDGET_TEXT N_("Do not demultiplex")
#define BUDGET_LONGTEXT N_( \
    "Only useful programs are normally demultiplexed from the transponder. " \
    "This option will disable demultiplexing and receive all programs.")

#define FREQ_TEXT N_("Frequency (Hz)")
#define FREQ_LONGTEXT N_( \
    "TV channels are grouped by transponder (a.k.a. multiplex) " \
    "on a given frequency. This is required to tune the receiver.")

#define MODULATION_TEXT N_("Modulation / Constellation")
#define MODULATION_LONGTEXT N_( \
    "The digital signal can be modulated according with different " \
    "constellations (depending on the delivery system). " \
    "If the demodulator cannot detect the constellation automatically, " \
    "it needs to be configured manually.")
static const char *const modulation_vlc[] = { "",
    "QAM", "16QAM", "32QAM", "64QAM", "128QAM", "256QAM",
    "8VSB", "16VSB",
    "QPSK", "DQPSK", "8PSK", "16APSK", "32APSK",
};
static const char *const modulation_user[] = { N_("Undefined"),
    "Auto QAM", "16-QAM", "32-QAM", "64-QAM", "128-QAM", "256-QAM",
    "8-VSB", "16-VSB",
    "QPSK", "DQPSK", "8-PSK", "16-APSK", "32-APSK",
};

#define SRATE_TEXT N_("Symbol rate (bauds)")
#define SRATE_LONGTEXT N_( \
    "The symbol rate must be specified manually for some systems, " \
    "notably DVB-C, DVB-S and DVB-S2.")

#define INVERSION_TEXT N_("Spectrum inversion")
#define INVERSION_LONGTEXT N_( \
    "If the demodulator cannot detect spectral inversion correctly, " \
    "it needs to be configured manually.")
//TODO const int inversion_linux[] = { INVERSION_AUTO,
//    INVERSION_OFF, INVERSION_ON,
//};
const int inversion_vlc[] = { -1, 0, 1 };
static const char *const auto_off_on[] = { N_("Automatic"),
    N_("Off"), N_("On") };

#define CODE_RATE_TEXT N_("FEC code rate")
#define CODE_RATE_HP_TEXT N_("High-priority code rate")
#define CODE_RATE_LP_TEXT N_("Low-priority code rate")
#define CODE_RATE_LONGTEXT N_( \
    "The code rate for Forward Error Correction can be specified.")
static const char *const code_rate_vlc[] = { "",
    "none", /*"1/4", "1/3",*/ "1/2", "3/5", "2/3", "3/4",
    "4/5", "5/6", "6/7", "7/8", "8/9", "9/10",
};
static const char *const code_rate_user[] = { N_("Automatic"),
    N_("None"), /*"1/4", "1/3",*/ "1/2", "3/5", "2/3", "3/4",
    "4/5", "5/6", "6/7", "7/8", "8/9", "9/10",
};

#define TRANSMISSION_TEXT N_("Transmission mode")
const int transmission_vlc[] = { -1,
    2, 4, 8, /*16, 32,*/
};
static const char *const transmission_user[] = { N_("Automatic"),
    "2k", "4k", "8k", /*"16k", "32k", */
};

#define BANDWIDTH_TEXT N_("Bandwidth (MHz)")
const int bandwidth_vlc[] = { 0,
    8, 7, 6,
};
static const char *const bandwidth_user[] = { N_("Automatic"),
    N_("8 MHz"), N_("7 MHz"), N_("6 MHz"),
};

#define GUARD_TEXT N_("Guard interval")
const char *const guard_vlc[] = { "",
    /*"1/128",*/ "1/32", "1/16", /*"19/128",*/ "1/8", /*"9/256",*/ "1/4",
};
static const char *const guard_user[] = { N_("Automatic"),
    /*"1/128",*/ "1/32", "1/16", /*"19/128",*/ "1/8", /*"9/256",*/ "1/4",
};

#define HIERARCHY_TEXT N_("Hierarchy mode")
const int hierarchy_vlc[] = { -1,
    0, 1, 2, 4,
};
static const char *const hierarchy_user[] = { N_("Automatic"),
    N_("None"), "1", "2", "4",
};

static int  Open (vlc_object_t *);
static void Close (vlc_object_t *);

vlc_module_begin ()
    set_shortname (N_("DTV"))
    set_description (N_("Digital Television and Radio (Linux DVB)"))
    set_category (CAT_INPUT)
    set_subcategory (SUBCAT_INPUT_ACCESS)
    set_capability ("access", 0)
    set_callbacks (Open, Close)
    add_shortcut ("dtv", "tv", "dvb", /* "radio", "dab",*/
                  "cable", "dvb-c", /*"satellite", "dvb-s", "dvb-s2",*/
                  "terrestrial", "dvb-t", "atsc")

    /* All options starting with dvb- can be overriden in the MRL, so they
     * must all be "safe". Nevertheless, we do not mark as safe those that are
     * really specific to the local system (e.g. device ID...).
     * It wouldn't make sense to deliver those through a playlist. */

    add_integer ("dvb-caching", DEFAULT_PTS_DELAY / 1000,
                 CACHING_TEXT, CACHING_LONGTEXT, true)
        change_integer_range (0, 60000)
        change_safe ()
#ifdef __linux__
    add_integer ("dvb-adapter", 0, ADAPTER_TEXT, ADAPTER_LONGTEXT, false)
        change_integer_range (0, 255)
    add_integer ("dvb-device", 0, DEVICE_TEXT, DEVICE_LONGTEXT, false)
        change_integer_range (0, 255)
    add_bool ("dvb-budget-mode", false, BUDGET_TEXT, BUDGET_LONGTEXT, true)
#endif
    add_integer ("dvb-frequency", 0, FREQ_TEXT, FREQ_LONGTEXT, false)
        change_integer_range (0, UINT64_C(0xffffffff) * 1000)
        change_safe ()
    add_string ("dvb-modulation", 0,
                 MODULATION_TEXT, MODULATION_LONGTEXT, false)
        change_string_list (modulation_vlc, modulation_user, NULL)
        change_safe ()
    /* Legacy modulation option for backward compatibility: TODO */
    /*add_integer ("dvb-modulation", 0, " ", " ", true)
        change_integer_range (-1, 256)
        change_private ()
        change_safe ()*/
    add_integer ("dvb-inversion", -1, INVERSION_TEXT, INVERSION_LONGTEXT, true)
        change_integer_list (inversion_vlc, auto_off_on)
        change_safe ()

    set_section (N_("Terrestrial reception parameters"), NULL)
    add_integer ("dvb-bandwidth", -1, BANDWIDTH_TEXT, BANDWIDTH_TEXT, true)
        change_integer_list (bandwidth_vlc, bandwidth_user)
        change_safe ()
    add_string ("dvb-code-rate-hp", "",
                CODE_RATE_HP_TEXT, CODE_RATE_LONGTEXT, false)
        change_string_list (code_rate_vlc, code_rate_user, NULL)
        change_safe ()
    add_string ("dvb-code-rate-lp", "",
                CODE_RATE_LP_TEXT, CODE_RATE_LONGTEXT, false)
        change_string_list (code_rate_vlc, code_rate_user, NULL)
        change_safe ()
    add_integer ("dvb-transmission", 0,
                 TRANSMISSION_TEXT, TRANSMISSION_TEXT, true)
        change_integer_list (transmission_vlc, transmission_user)
        change_safe ()
    add_string ("dvb-guard", "", GUARD_TEXT, GUARD_TEXT, true)
        change_string_list (guard_vlc, guard_user, NULL)
        change_safe ()
    add_integer ("dvb-hierarchy", -1, HIERARCHY_TEXT, HIERARCHY_TEXT, true)
        change_integer_list (hierarchy_vlc, hierarchy_user)
        change_safe ()

    set_section (N_("Cable (DVB-C) and satellite (DVB-S) parameters"), NULL)
    add_integer ("dvb-srate", 0, SRATE_TEXT, SRATE_LONGTEXT, false)
        change_integer_range (0, UINT64_C(0xffffffff))
        change_safe ()
    add_string ("dvb-code-rate", "", CODE_RATE_TEXT, CODE_RATE_LONGTEXT, true)
        change_string_list (code_rate_vlc, code_rate_user, NULL)
        change_safe ()
    /* Legacy FEC option for backward compatibility: TODO */
    add_integer ("dvb-fec", 9, " ", " ", true)
        change_integer_range (0, 9)
        change_private ()
        change_safe ()
#ifdef FIXME
    set_section (N_("Satellite (DVB-S) parameters"), NULL)
    add_integer ("dvb-satno", 0, SATNO_TEXT, SATNO_LONGTEXT, true)
        change_integer_list (satno_vlc, satno_user)
        change_safe ()
    add_integer ("dvb-voltage", 13, VOLTAGE_TEXT, VOLTAGE_LONGTEXT, true)
        change_integer_list (voltage_vlc, voltage_user)
        change_safe ()
    add_bool ("dvb-high-voltage", false,
              HIGH_VOLTAGE_TEXT, HIGH_VOLTAGE_LONGTEXT, false)
    add_integer ("dvb-tone", -1, TONE_TEXT, TONE_LONGTEXT, true)
        change_integer_list (tone_vlc, auto_off_on)
        change_safe ()
    add_integer ("dvb-lnb-lof1", 0, LNB_LOF1_TEXT, LNB_LOF1_LONGTEXT, true)
        change_integer_range (0, 0x7fffffff)
        change_safe ()
    add_integer ("dvb-lnb-lof2", 0, LNB_LOF2_TEXT, LNB_LOF2_LONGTEXT, true)
        change_integer_range (0, 0x7fffffff)
        change_safe ()
    add_integer ("dvb-lnb-slof", 0, LNB_SLOF_TEXT, LNB_SLOF_LONGTEXT, true)
        change_integer_range (0, 0x7fffffff)
        change_safe ()
#endif
vlc_module_end ()

struct access_sys_t
{
    dvb_device_t *dev;
};

struct delsys
{
    int (*tune) (vlc_object_t *, dvb_device_t *, uint64_t freq);
    /* TODO: scan stuff */
};

static block_t *Read (access_t *);
static int Control (access_t *, int, va_list);

static const delsys_t *GuessSystem (const char *scheme, dvb_device_t *dev)
{
    /* NOTE: We should guess the delivery system for the "cable", "satellite"
     * and "terrestrial" shortcuts (i.e. DVB, ISDB, ATSC...). But there is
     * seemingly no sane way to do get the info with Linux DVB version 5.2.
     * In particular, the frontend infos distinguish only the modulator class
     * (QPSK, QAM, OFDM or ATSC).
     *
     * Furthermore, if the demodulator supports 2G, we cannot guess whether
     * 1G or 2G is intended. For backward compatibility, 1G is assumed
     * (this is not a limitation of Linux DVB). We will probably need something
     * smarter when 2G (semi automatic) scanning is implemented. */
 
    if (!strcasecmp (scheme, "cable"))
        scheme = "dvb-c";
    else
    if (!strcasecmp (scheme, "satellite"))
        scheme = "dvb-s";
    else
    if (!strcasecmp (scheme, "terrestrial"))
        scheme = "dvb-t";

    if (!strcasecmp (scheme, "atsc"))
        return &atsc;
    if (!strcasecmp (scheme, "dvb-c"))
        return &dvbc;
    if (!strcasecmp (scheme, "dvb-s"))
        return &dvbs;
    if (!strcasecmp (scheme, "dvb-s2"))
        return &dvbs2;
    if (!strcasecmp (scheme, "dvb-t"))
        return &dvbt;

    return dvb_guess_system (dev);
}


static int Open (vlc_object_t *obj)
{
    access_t *access = (access_t *)obj;
    access_sys_t *sys = malloc (sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    var_LocationParse (obj, access->psz_location, "dvb-");
    uint64_t freq = var_InheritInteger (obj, "dvb-frequency");

    dvb_device_t *dev = dvb_open (obj, freq != 0);
    if (dev == NULL)
    {
        free (sys);
        return VLC_EGENERIC;
    }

    sys->dev = dev;
    access->p_sys = sys;

    if (freq != 0)
    {
        const delsys_t *delsys = GuessSystem (access->psz_access, dev);
        if (delsys == NULL || delsys->tune (obj, dev, freq))
        {
            msg_Err (obj, "tuning to %"PRIu64" Hz failed", freq);
            dialog_Fatal (obj, N_("Digital broadcasting"),
                          N_("The selected digital tuner does not support "
                             "the specified parameters. "
                             "Please check the preferences."));
            goto error;
        }
    }
    dvb_add_pid (dev, 0);

    access->pf_block = Read;
    access->pf_control = Control;
    if (access->psz_demux == NULL || !access->psz_demux[0])
    {
        free (access->psz_demux);
        access->psz_demux = strdup ("ts");
    }
    return VLC_SUCCESS;

error:
    Close (obj);
    access->p_sys = NULL;
    return VLC_EGENERIC;
}

static void Close (vlc_object_t *obj)
{
    access_t *access = (access_t *)obj;
    access_sys_t *sys = access->p_sys;

    dvb_close (sys->dev);
    free (sys);
}

static block_t *Read (access_t *access)
{
#define BUFSIZE (20*188)
    block_t *block = block_Alloc (BUFSIZE);
    if (unlikely(block == NULL))
        return NULL;

    access_sys_t *sys = access->p_sys;
    ssize_t val = dvb_read (sys->dev, block->p_buffer, BUFSIZE);

    if (val <= 0)
    {
        if (val == 0)
            access->info.b_eof = true;
        block_Release (block);
        return NULL;
    }

    block->i_buffer = val;
    return block;
}

static int Control (access_t *access, int query, va_list args)
{
    access_sys_t *sys = access->p_sys;
    dvb_device_t *dev = sys->dev;

    switch (query)
    {
        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
        {
            bool *v = va_arg (args, bool *);
            *v = false;
            return VLC_SUCCESS;
        }

        case ACCESS_GET_PTS_DELAY:
        {
            int64_t *v = va_arg (args, int64_t *);
            *v = var_InheritInteger (access, "dvb-caching") * INT64_C(1000);
            return VLC_SUCCESS;
        }

        case ACCESS_GET_TITLE_INFO:
        case ACCESS_GET_META:
            return VLC_EGENERIC;

        case ACCESS_GET_CONTENT_TYPE:
        {
            char **pt = va_arg (args, char **);
            *pt = strdup ("video/MP2T");
            return VLC_SUCCESS;
        }

        case ACCESS_SET_PAUSE_STATE:
        case ACCESS_SET_TITLE:
        case ACCESS_SET_SEEKPOINT:
            return VLC_EGENERIC;

        case ACCESS_GET_SIGNAL:
            *va_arg (args, double *) = dvb_get_snr (dev);
            *va_arg (args, double *) = dvb_get_signal_strength (dev);
            return VLC_SUCCESS;

        case ACCESS_SET_PRIVATE_ID_STATE:
        {
            unsigned pid = va_arg (args, unsigned);
            bool add = va_arg (args, unsigned);

            if (unlikely(pid > 0x1FFF))
                return VLC_EGENERIC;
            if (add)
            {
                if (dvb_add_pid (dev, pid))
                    return VLC_EGENERIC;
            }
            else
                dvb_remove_pid (dev, pid);
            return VLC_SUCCESS;
        }

        case ACCESS_SET_PRIVATE_ID_CA:
            /* TODO */
            return VLC_EGENERIC;

        case ACCESS_GET_PRIVATE_ID_STATE:
            return VLC_EGENERIC;
    }

    msg_Warn (access, "unimplemented query %d in control", query);
    return VLC_EGENERIC;
}


/*** ATSC ***/
static int atsc_tune (vlc_object_t *obj, dvb_device_t *dev, uint64_t freq)
{
    char *mod = var_InheritString (obj, "dvb-modulation");

    int ret = dvb_set_atsc (dev, freq, mod);
    free (mod);
    if (ret == 0)
        ret = dvb_tune (dev);
    return ret;
}

const delsys_t atsc = { .tune = atsc_tune };


/*** DVB-C ***/
static int dvbc_tune (vlc_object_t *obj, dvb_device_t *dev, uint64_t freq)
{
    char *mod = var_InheritString (obj, "dvb-modulation");
    char *fec = var_InheritString (obj, "dvb-code-rate");
    unsigned srate = var_InheritInteger (obj, "dvb-srate");

    int ret = dvb_set_dvbc (dev, freq, mod, srate, fec);
    free (fec);
    free (mod);
    if (ret == 0)
        ret = dvb_tune (dev);
    return ret;
}

const delsys_t dvbc = { .tune = dvbc_tune };


/*** DVB-S ***/
static int dvbs_tune (vlc_object_t *obj, dvb_device_t *dev, uint64_t freq)
{
    (void) dev; (void) freq;
    msg_Err (obj, "DVB-S not implemented");
    return -1;
}


const delsys_t dvbs = { .tune = dvbs_tune };
const delsys_t dvbs2 = { .tune = dvbs_tune };


/*** DVB-T ***/
static int dvbt_tune (vlc_object_t *obj, dvb_device_t *dev, uint64_t freq)
{
    char *mod = var_InheritString (obj, "dvb-modulation");
    char *fec_hp = var_InheritString (obj, "dvb-code-rate-hp");
    char *fec_lp = var_InheritString (obj, "dvb-code-rate-lp");
    char *guard = var_InheritString (obj, "dvb-guard");
    uint32_t bw = var_InheritInteger (obj, "dvb-bandwidth");
    int tx = var_InheritInteger (obj, "dvb-transmission");
    int h = var_InheritInteger (obj, "dvb-hierarchy");

    int ret = dvb_set_dvbt (dev, freq, mod, fec_hp, fec_lp, bw, tx, guard, h);
    free (guard);
    free (fec_lp);
    free (fec_hp);
    free (mod);
    if (ret == 0)
        ret = dvb_tune (dev);
    return ret;
}

const delsys_t dvbt = { .tune = dvbt_tune };
