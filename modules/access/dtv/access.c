/**
 * @file access.c
 * @brief Digital broadcasting input module for VLC media player
 */
/*****************************************************************************
 * Copyright © 2011 Rémi Denis-Courmont
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

#include <vlc_common.h>
#include <vlc_access.h>
#include <vlc_input.h>
#include <vlc_plugin.h>
#include <vlc_dialog.h>
#include <search.h>

#include "dtv/dtv.h"

#define ADAPTER_TEXT N_("DVB adapter")
#define ADAPTER_LONGTEXT N_( \
    "If there is more than one digital broadcasting adapter, " \
    "the adapter number must be selected. Numbering starts from zero.")

#define DEVICE_TEXT N_("DVB device")
#define DEVICE_LONGTEXT N_( \
    "If the adapter provides multiple independent tuner devices, " \
    "the device number must be selected. Numbering starts from zero.")
#define BUDGET_TEXT N_("Do not demultiplex")
#define BUDGET_LONGTEXT N_( \
    "Only useful programs are normally demultiplexed from the transponder. " \
    "This option will disable demultiplexing and receive all programs.")

#define NAME_TEXT N_("Network name")
#define NAME_LONGTEXT N_("Unique network name in the System Tuning Spaces")

#define CREATE_TEXT N_("Network name to create")
#define CREATE_LONGTEXT N_("Create unique name in the System Tuning Spaces")

#define FREQ_TEXT N_("Frequency (Hz)")
#define FREQ_LONGTEXT N_( \
    "TV channels are grouped by transponder (a.k.a. multiplex) " \
    "on a given frequency. This is required to tune the receiver.")

#define MODULATION_TEXT N_("Modulation / Constellation")
#define MODULATION_A_TEXT N_("Layer A modulation")
#define MODULATION_B_TEXT N_("Layer B modulation")
#define MODULATION_C_TEXT N_("Layer C modulation")
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
const int auto_off_on_vlc[] = { -1, 0, 1 };
static const char *const auto_off_on_user[] = { N_("Automatic"),
    N_("Off"), N_("On") };

#define CODE_RATE_TEXT N_("FEC code rate")
#define CODE_RATE_HP_TEXT N_("High-priority code rate")
#define CODE_RATE_LP_TEXT N_("Low-priority code rate")
#define CODE_RATE_A_TEXT N_("Layer A code rate")
#define CODE_RATE_B_TEXT N_("Layer B code rate")
#define CODE_RATE_C_TEXT N_("Layer C code rate")
#define CODE_RATE_LONGTEXT N_( \
    "The code rate for Forward Error Correction can be specified.")
static const char *const code_rate_vlc[] = { "",
    "0", /*"1/4", "1/3",*/ "1/2", "3/5", "2/3", "3/4",
    "4/5", "5/6", "6/7", "7/8", "8/9", "9/10",
};
static const char *const code_rate_user[] = { N_("Automatic"),
    N_("None"), /*"1/4", "1/3",*/ "1/2", "3/5", "2/3", "3/4",
    "4/5", "5/6", "6/7", "7/8", "8/9", "9/10",
};

#define TRANSMISSION_TEXT N_("Transmission mode")
const int transmission_vlc[] = { -1,
    1, 2, 4, 8, 16, 32,
};
static const char *const transmission_user[] = { N_("Automatic"),
    "1k", "2k", "4k", "8k", "16k", "32k",
};

#define BANDWIDTH_TEXT N_("Bandwidth (MHz)")
const int bandwidth_vlc[] = { 0,
    10, 8, 7, 6, 5, 2
};
static const char *const bandwidth_user[] = { N_("Automatic"),
    N_("10 MHz"), N_("8 MHz"), N_("7 MHz"), N_("6 MHz"),
    N_("5 MHz"), N_("1.712 MHz"),
};

#define GUARD_TEXT N_("Guard interval")
const char *const guard_vlc[] = { "",
    "1/128", "1/32", "1/16", "19/256", "1/8", "19/128", "1/4",
};
static const char *const guard_user[] = { N_("Automatic"),
    "1/128", "1/32", "1/16", "19/256", "1/8", "19/128", "1/4",
};

#define HIERARCHY_TEXT N_("Hierarchy mode")
const int hierarchy_vlc[] = { -1,
    0, 1, 2, 4,
};
static const char *const hierarchy_user[] = { N_("Automatic"),
    N_("None"), "1", "2", "4",
};

#define PLP_ID_TEXT N_("DVB-T2 Physical Layer Pipe")

#define SEGMENT_COUNT_A_TEXT N_("Layer A segments count")
#define SEGMENT_COUNT_B_TEXT N_("Layer B segments count")
#define SEGMENT_COUNT_C_TEXT N_("Layer C segments count")

#define TIME_INTERLEAVING_A_TEXT N_("Layer A time interleaving")
#define TIME_INTERLEAVING_B_TEXT N_("Layer B time interleaving")
#define TIME_INTERLEAVING_C_TEXT N_("Layer C time interleaving")

#define STREAM_ID_TEXT N_("Stream identifier")

#define PILOT_TEXT N_("Pilot")

#define ROLLOFF_TEXT N_("Roll-off factor")
const int rolloff_vlc[] = { -1,
    35, 20, 25,
};
static const char *const rolloff_user[] = { N_("Automatic"),
    N_("0.35 (same as DVB-S)"), N_("0.20"), N_("0.25"),
};

#define TS_ID_TEXT N_("Transport stream ID")

#define POLARIZATION_TEXT N_("Polarization (Voltage)")
#define POLARIZATION_LONGTEXT N_( \
    "To select the polarization of the transponder, a different voltage " \
    "is normally applied to the low noise block-downconverter (LNB).")
static const char *const polarization_vlc[] = { "", "V", "H", "R", "L" };
static const char *const polarization_user[] = { N_("Unspecified (0V)"),
    N_("Vertical (13V)"), N_("Horizontal (18V)"),
    N_("Circular Right Hand (13V)"), N_("Circular Left Hand (18V)") };

#define HIGH_VOLTAGE_TEXT N_("High LNB voltage")
#define HIGH_VOLTAGE_LONGTEXT N_( \
    "If the cables between the satellilte low noise block-downconverter and " \
    "the receiver are long, higher voltage may be required.\n" \
    "Not all receivers support this.")

#define LNB_LOW_TEXT N_("Local oscillator low frequency (kHz)")
#define LNB_HIGH_TEXT N_("Local oscillator high frequency (kHz)")
#define LNB_LONGTEXT N_( \
    "The downconverter (LNB) will subtract the local oscillator frequency " \
    "from the satellite transmission frequency. " \
    "The intermediate frequency (IF) on the RF cable is the result.")
#define LNB_SWITCH_TEXT N_("Universal LNB switch frequency (kHz)")
#define LNB_SWITCH_LONGTEXT N_( \
    "If the satellite transmission frequency exceeds the switch frequency, " \
    "the oscillator high frequency will be used as reference. " \
    "Furthermore the automatic continuous 22kHz tone will be sent.")
#define TONE_TEXT N_("Continuous 22kHz tone")
#define TONE_LONGTEXT N_( \
    "A continuous tone at 22kHz can be sent on the cable. " \
    "This normally selects the higher frequency band from a universal LNB.")

#define SATNO_TEXT N_("DiSEqC LNB number")
#define SATNO_LONGTEXT N_( \
    "If the satellite receiver is connected to multiple " \
    "low noise block-downconverters (LNB) through a DiSEqC 1.0 switch, " \
    "the correct LNB can be selected (1 to 4). " \
    "If there is no switch, this parameter should be 0.")
#ifdef HAVE_LINUX_DVB
static const int satno_vlc[] = { 0, 1, 2, 3, 4 };
static const char *const satno_user[] = { N_("Unspecified"),
    "A/1", "B/2", "C/3", "D/4" };
#endif

#define UNCOMMITTED_TEXT N_("Uncommitted DiSEqC LNB number")
#define UNCOMMITTED_LONGTEXT N_( \
    "If the satellite receiver is connected to multiple " \
    "low noise block-downconverters (LNB) through a cascade formed from " \
    "DiSEqC 1.1 uncommitted switch and DiSEqC 1.0 committed switch, " \
    "the correct uncommitted LNB can be selected (1 to 4). " \
    "If there is no uncommitted switch, this parameter should be 0.")

/* BDA module additional DVB-S Parameters */
#define NETID_TEXT N_("Network identifier")
#define AZIMUTH_TEXT N_("Satellite azimuth")
#define AZIMUTH_LONGTEXT N_("Satellite azimuth in tenths of degree")
#define ELEVATION_TEXT N_("Satellite elevation")
#define ELEVATION_LONGTEXT N_("Satellite elevation in tenths of degree")
#define LONGITUDE_TEXT N_("Satellite longitude")
#define LONGITUDE_LONGTEXT N_( \
    "Satellite longitude in tenths of degree. West is negative.")

#define RANGE_TEXT N_("Satellite range code")
#define RANGE_LONGTEXT N_("Satellite range code as defined by manufacturer " \
   "e.g. DISEqC switch code")

/* ATSC */
#define MAJOR_CHANNEL_TEXT N_("Major channel")
#define MINOR_CHANNEL_TEXT N_("ATSC minor channel")
#define PHYSICAL_CHANNEL_TEXT N_("Physical channel")

static int  Open (vlc_object_t *);
static void Close (vlc_object_t *);

vlc_module_begin ()
    set_shortname (N_("DTV"))
    set_description (N_("Digital Television and Radio"))
    set_category (CAT_INPUT)
    set_subcategory (SUBCAT_INPUT_ACCESS)
    set_capability ("access", 0)
    set_callbacks (Open, Close)
    add_shortcut ("dtv", "tv", "dvb", /* "radio", "dab",*/
                  "cable", "dvb-c", "cqam", "isdb-c",
                  "satellite", "dvb-s", "dvb-s2", "isdb-s",
                  "terrestrial", "dvb-t", "dvb-t2", "isdb-t", "atsc"
#ifdef _WIN32
                  ,"dvbt"
#endif
                 )

#ifdef HAVE_LINUX_DVB
    add_integer ("dvb-adapter", 0, ADAPTER_TEXT, ADAPTER_LONGTEXT, false)
        change_integer_range (0, 255)
        change_safe ()
    add_integer ("dvb-device", 0, DEVICE_TEXT, DEVICE_LONGTEXT, false)
        change_integer_range (0, 255)
        change_safe ()
    add_bool ("dvb-budget-mode", false, BUDGET_TEXT, BUDGET_LONGTEXT, true)
#endif
#ifdef _WIN32
    add_integer ("dvb-adapter", -1, ADAPTER_TEXT, ADAPTER_LONGTEXT, true)
        change_safe ()
    add_string ("dvb-network-name", "", NAME_TEXT, NAME_LONGTEXT, true)
    /* Hmm: is this one really safe??: */
    add_string ("dvb-create-name", "", CREATE_TEXT, CREATE_LONGTEXT, true)
        change_private ()
#endif
    add_integer ("dvb-frequency", 0, FREQ_TEXT, FREQ_LONGTEXT, false)
        change_integer_range (0, 107999999)
        change_safe ()
    add_integer ("dvb-inversion", -1, INVERSION_TEXT, INVERSION_LONGTEXT, true)
        change_integer_list (auto_off_on_vlc, auto_off_on_user)
        change_safe ()

    set_section (N_("Terrestrial reception parameters"), NULL)
    add_integer ("dvb-bandwidth", 0, BANDWIDTH_TEXT, BANDWIDTH_TEXT, true)
        change_integer_list (bandwidth_vlc, bandwidth_user)
        change_safe ()
    add_integer ("dvb-transmission", 0,
                 TRANSMISSION_TEXT, TRANSMISSION_TEXT, true)
        change_integer_list (transmission_vlc, transmission_user)
        change_safe ()
    add_string ("dvb-guard", "", GUARD_TEXT, GUARD_TEXT, true)
        change_string_list (guard_vlc, guard_user)
        change_safe ()

    set_section (N_("DVB-T reception parameters"), NULL)
    add_string ("dvb-code-rate-hp", "",
                CODE_RATE_HP_TEXT, CODE_RATE_LONGTEXT, true)
        change_string_list (code_rate_vlc, code_rate_user)
        change_safe ()
    add_string ("dvb-code-rate-lp", "",
                CODE_RATE_LP_TEXT, CODE_RATE_LONGTEXT, true)
        change_string_list (code_rate_vlc, code_rate_user)
        change_safe ()
    add_integer ("dvb-hierarchy", -1, HIERARCHY_TEXT, HIERARCHY_TEXT, true)
        change_integer_list (hierarchy_vlc, hierarchy_user)
        change_safe ()
    add_integer ("dvb-plp-id", 0, PLP_ID_TEXT, PLP_ID_TEXT, false)
        change_integer_range (0, 255)
        change_safe ()

    set_section (N_("ISDB-T reception parameters"), NULL)
    add_string ("dvb-a-modulation", NULL,
                MODULATION_A_TEXT, MODULATION_LONGTEXT, true)
        change_string_list (modulation_vlc, modulation_user)
        change_safe ()
    add_string ("dvb-a-fec", NULL, CODE_RATE_A_TEXT, CODE_RATE_LONGTEXT, true)
        change_string_list (code_rate_vlc, code_rate_user)
        change_safe ()
    add_integer ("dvb-a-count", 0, SEGMENT_COUNT_A_TEXT, NULL, true)
        change_integer_range (0, 13)
        change_safe ()
    add_integer ("dvb-a-interleaving", 0, TIME_INTERLEAVING_A_TEXT, NULL, true)
        change_integer_range (0, 3)
        change_safe ()
    add_string ("dvb-b-modulation", NULL,
                MODULATION_B_TEXT, MODULATION_LONGTEXT, true)
        change_string_list (modulation_vlc, modulation_user)
        change_safe ()
    add_string ("dvb-b-fec", NULL, CODE_RATE_B_TEXT, CODE_RATE_LONGTEXT, true)
        change_string_list (code_rate_vlc, code_rate_user)
        change_safe ()
    add_integer ("dvb-b-count", 0, SEGMENT_COUNT_B_TEXT, NULL, true)
        change_integer_range (0, 13)
        change_safe ()
    add_integer ("dvb-b-interleaving", 0, TIME_INTERLEAVING_B_TEXT, NULL, true)
        change_integer_range (0, 3)
        change_safe ()
    add_string ("dvb-c-modulation", NULL,
                MODULATION_C_TEXT, MODULATION_LONGTEXT, true)
        change_string_list (modulation_vlc, modulation_user)
        change_safe ()
    add_string ("dvb-c-fec", NULL, CODE_RATE_C_TEXT, CODE_RATE_LONGTEXT, true)
        change_string_list (code_rate_vlc, code_rate_user)
        change_safe ()
    add_integer ("dvb-c-count", 0, SEGMENT_COUNT_C_TEXT, NULL, true)
        change_integer_range (0, 13)
        change_safe ()
    add_integer ("dvb-c-interleaving", 0, TIME_INTERLEAVING_C_TEXT, NULL, true)
        change_integer_range (0, 3)
        change_safe ()

    set_section (N_("Cable and satellite reception parameters"), NULL)
    add_string ("dvb-modulation", NULL,
                 MODULATION_TEXT, MODULATION_LONGTEXT, false)
        change_string_list (modulation_vlc, modulation_user)
        change_safe ()
    add_integer ("dvb-srate", 0, SRATE_TEXT, SRATE_LONGTEXT, false)
        change_integer_range (0, UINT64_C(0xffffffff))
        change_safe ()
    add_string ("dvb-fec", "", CODE_RATE_TEXT, CODE_RATE_LONGTEXT, true)
        change_string_list (code_rate_vlc, code_rate_user)
        change_safe ()

    set_section (N_("DVB-S2 parameters"), NULL)
    add_integer ("dvb-stream", 0, STREAM_ID_TEXT, STREAM_ID_TEXT, false)
        change_integer_range (0, 255)
        change_safe ()
    add_integer ("dvb-pilot", -1, PILOT_TEXT, PILOT_TEXT, true)
        change_integer_list (auto_off_on_vlc, auto_off_on_user)
        change_safe ()
    add_integer ("dvb-rolloff", -1, ROLLOFF_TEXT, ROLLOFF_TEXT, true)
        change_integer_list (rolloff_vlc, rolloff_user)
        change_safe ()

    set_section (N_("ISDB-S parameters"), NULL)
    add_integer ("dvb-ts-id", 0, TS_ID_TEXT, TS_ID_TEXT, false)
        change_integer_range (0, 0xffff)
        change_safe ()

    set_section (N_("Satellite equipment control"), NULL)
    add_string ("dvb-polarization", "",
                POLARIZATION_TEXT, POLARIZATION_LONGTEXT, false)
        change_string_list (polarization_vlc, polarization_user)
        change_safe ()
    add_integer ("dvb-voltage", 13, "", "", true)
        change_integer_range (0, 18)
        change_private ()
        change_safe ()
#ifdef HAVE_LINUX_DVB
    add_bool ("dvb-high-voltage", false,
              HIGH_VOLTAGE_TEXT, HIGH_VOLTAGE_LONGTEXT, false)
#endif
    add_integer ("dvb-lnb-low", 0, LNB_LOW_TEXT, LNB_LONGTEXT, true)
        change_integer_range (0, 0x7fffffff)
    add_obsolete_integer ("dvb-lnb-lof1") /* since 2.0.0 */
    add_integer ("dvb-lnb-high", 0, LNB_HIGH_TEXT, LNB_LONGTEXT, true)
        change_integer_range (0, 0x7fffffff)
    add_obsolete_integer ("dvb-lnb-lof2") /* since 2.0.0 */
    add_integer ("dvb-lnb-switch", 11700000,
                 LNB_SWITCH_TEXT, LNB_SWITCH_LONGTEXT, true)
        change_integer_range (0, 0x7fffffff)
    add_obsolete_integer ("dvb-lnb-slof") /* since 2.0.0 */
#ifdef HAVE_LINUX_DVB
    add_integer ("dvb-satno", 0, SATNO_TEXT, SATNO_LONGTEXT, true)
        change_integer_list (satno_vlc, satno_user)
        change_safe ()
    add_integer ("dvb-uncommitted", 0, UNCOMMITTED_TEXT, UNCOMMITTED_LONGTEXT, true)
        change_integer_list (satno_vlc, satno_user)
        change_safe ()
    add_integer ("dvb-tone", -1, TONE_TEXT, TONE_LONGTEXT, true)
        change_integer_list (auto_off_on_vlc, auto_off_on_user)
#endif
#ifdef _WIN32
    add_integer ("dvb-network-id", 0, NETID_TEXT, NETID_TEXT, true)
    add_integer ("dvb-azimuth", 0, AZIMUTH_TEXT, AZIMUTH_LONGTEXT, true)
    add_integer ("dvb-elevation", 0, ELEVATION_TEXT, ELEVATION_LONGTEXT, true)
    add_integer ("dvb-longitude", 0, LONGITUDE_TEXT, LONGITUDE_LONGTEXT, true)
    add_string ("dvb-range", "", RANGE_TEXT, RANGE_LONGTEXT, true)
    /* dvb-range corresponds to the BDA InputRange parameter which is
    * used by some drivers to control the diseqc */

    set_section (N_("ATSC reception parameters"), NULL)
    add_integer ("dvb-major-channel", 0, MAJOR_CHANNEL_TEXT, NULL, true)
    add_integer ("dvb-minor-channel", 0, MINOR_CHANNEL_TEXT, NULL, true)
    add_integer ("dvb-physical-channel", 0, PHYSICAL_CHANNEL_TEXT, NULL, true)
#endif
vlc_module_end ()

struct access_sys_t
{
    dvb_device_t *dev;
    uint8_t signal_poll;
    tuner_setup_t pf_setup;
};

static block_t *Read (stream_t *, bool *);
static int Control (stream_t *, int, va_list);
static dtv_delivery_t GuessSystem (const char *, dvb_device_t *);
static dtv_delivery_t GetDeliveryByScheme(const char *psz_scheme);
static int Tune (vlc_object_t *, dvb_device_t *, tuner_setup_t, uint64_t);
static uint64_t var_InheritFrequency (vlc_object_t *);

tuner_setup_t dtv_get_delivery_tuner_setup( dtv_delivery_t d );

static int Open (vlc_object_t *obj)
{
    stream_t *access = (stream_t *)obj;
    access_sys_t *sys = malloc (sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    var_LocationParse (obj, access->psz_location, "dvb-");

    dvb_device_t *dev = dvb_open (obj);
    if (dev == NULL)
    {
        free (sys);
        return VLC_EGENERIC;
    }

    sys->dev = dev;
    sys->signal_poll = 0;
    sys->pf_setup = NULL;
    access->p_sys = sys;

    uint64_t freq = var_InheritFrequency (obj);
    if (freq != 0)
    {
        dtv_delivery_t d = GuessSystem (access->psz_name, dev);
        if(d != DTV_DELIVERY_NONE)
            sys->pf_setup = dtv_get_delivery_tuner_setup(d);

        if (sys->pf_setup == NULL || Tune (obj, dev, sys->pf_setup, freq))
        {
            msg_Err (obj, "tuning to %"PRIu64" Hz failed", freq);
            vlc_dialog_display_error (obj, N_("Digital broadcasting"),
                N_("The selected digital tuner does not support "
                   "the specified parameters.\n"
                   "Please check the preferences."));
            goto error;
        }
    }
    dvb_add_pid (dev, 0);

    access->pf_block = Read;
    access->pf_control = Control;
    return VLC_SUCCESS;

error:
    Close (obj);
    access->p_sys = NULL;
    return VLC_EGENERIC;
}

static void Close (vlc_object_t *obj)
{
    stream_t *access = (stream_t *)obj;
    access_sys_t *sys = access->p_sys;

    dvb_close (sys->dev);
    free (sys);
}

static block_t *Read (stream_t *access, bool *restrict eof)
{
#define BUFSIZE (20*188)
    block_t *block = block_Alloc (BUFSIZE);
    if (unlikely(block == NULL))
        return NULL;

    access_sys_t *sys = access->p_sys;
    ssize_t val = dvb_read (sys->dev, block->p_buffer, BUFSIZE, -1);

    if (val <= 0)
    {
        if (val == 0)
            *eof = true;
        block_Release (block);
        return NULL;
    }

    block->i_buffer = val;

    return block;
}

static int Control (stream_t *access, int query, va_list args)
{
    access_sys_t *sys = access->p_sys;
    dvb_device_t *dev = sys->dev;

    switch (query)
    {
        case STREAM_CAN_SEEK:
        case STREAM_CAN_FASTSEEK:
        case STREAM_CAN_PAUSE:
        case STREAM_CAN_CONTROL_PACE:
            *va_arg (args, bool *) = false;
            break;

        case STREAM_GET_PTS_DELAY:
        {
            int64_t *v = va_arg (args, int64_t *);
            *v = var_InheritInteger (access, "live-caching") * INT64_C(1000);
            break;
        }

        case STREAM_GET_CONTENT_TYPE:
            *va_arg (args, char **) = strdup ("video/MP2T");
            break;

        case STREAM_GET_SIGNAL:
            /* Fetch the signal levels only every so often to avoid stressing
             * the device bus. */
            if ((sys->signal_poll++))
                return VLC_EGENERIC;

            *va_arg (args, double *) = dvb_get_snr (dev);
            *va_arg (args, double *) = dvb_get_signal_strength (dev);
            return VLC_SUCCESS;

        case STREAM_SET_PRIVATE_ID_STATE:
        {
            unsigned pid = va_arg (args, int);
            bool add = va_arg (args, int);

            if (unlikely(pid > 0x1FFF))
                return VLC_EGENERIC;
            if (add)
            {
                if (dvb_add_pid (dev, pid))
                    return VLC_EGENERIC;
            }
            else
                dvb_remove_pid (dev, pid);
            break;
        }

        case STREAM_SET_PRIVATE_ID_CA:
        {
            en50221_capmt_info_t *pmt = va_arg (args, en50221_capmt_info_t *);

            if( !dvb_set_ca_pmt (dev, pmt) )
                return VLC_EGENERIC;
            break;
        }

        case STREAM_GET_PRIVATE_ID_STATE:
        {
            unsigned pid = va_arg (args, int);
            bool *on = va_arg (args, bool *);

            *on = likely(pid <= 0x1FFF) ? dvb_get_pid_state(dev, pid) : false;
            return VLC_SUCCESS;
        }

        default:
            return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}


/*** Generic tuning ***/
/** Set parameters and tune the device */
static int Tune (vlc_object_t *obj, dvb_device_t *dev, tuner_setup_t pf_setup,
                 uint64_t freq)
{
    if (pf_setup(obj, dev, freq)
     || dvb_set_inversion (dev, var_InheritInteger (obj, "dvb-inversion"))
     || dvb_tune (dev))
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

static uint64_t var_InheritFrequency (vlc_object_t *obj)
{
    uint64_t freq = var_InheritInteger (obj, "dvb-frequency");
    if (freq != 0 && freq < 30000000)
    {
        msg_Err (obj, "%"PRIu64" Hz carrier frequency is too low.", freq);
        freq *= 1000;
        msg_Info (obj, "Assuming %"PRIu64" Hz frequency instead.", freq);
    }
    return freq;
}

static uint32_t var_InheritCodeRate (vlc_object_t *obj, const char *varname)
{
    char *code_rate = var_InheritString (obj, varname);
    if (code_rate == NULL)
        return VLC_FEC_AUTO;

    uint16_t a, b;
    int v = sscanf (code_rate, "%"SCNu16"/%"SCNu16, &a, &b);
    free (code_rate);
    switch (v)
    {
        case 2:
            return VLC_FEC(a, b);
        case 1:
            if (a == 0)
                return 0;
            /* Backward compatibility with VLC < 1.2 (= Linux DVBv3 enum) */
            if (a < 9)
            {
                msg_Warn (obj, "\"%s=%"PRIu16"\" option is obsolete. "
                          "Use \"%s=%"PRIu16"/%"PRIu16"\" instead.",
                          varname + 4, a, varname + 4, a, a + 1);
                return VLC_FEC(a, a + 1);
            }
            else
                msg_Warn (obj, "\"fec=9\" option is obsolete.");
    }
    return VLC_FEC_AUTO;
}

static int modcmp (const void *a, const void *b)
{
    return strcasecmp (a, *(const char *const *)b);
}

static const char *var_InheritModulation (vlc_object_t *obj, const char *var)
{
    char *mod = var_InheritString (obj, var);
    if (mod == NULL)
        return "";

    size_t n = sizeof (modulation_vlc) / sizeof (modulation_vlc[0]);
    const char *const *p = lfind (mod, modulation_vlc, &n, sizeof (mod), modcmp);
    if (p != NULL)
    {
        free (mod);
        return *p;
    }

    /* Backward compatibility with VLC < 1.2 */
    const char *str;
    switch (atoi (mod))
    {
        case -1:  str = "QPSK";   break;
        case 0:   str = "QAM";    break;
        case 8:   str = "8VSB";   break;
        case 16:  str = "16QAM";  break;
        case 32:  str = "32QAM";  break;
        case 64:  str = "64QAM";  break;
        case 128: str = "128QAM"; break;
        case 256: str = "256QAM"; break;
        default:  return "";
    }

    msg_Warn (obj, "\"modulation=%s\" option is obsolete. "
                   "Use \"modulation=%s\" instead.", mod, str);
    free (mod);
    return str;
}

static unsigned var_InheritGuardInterval (vlc_object_t *obj)
{
    char *guard = var_InheritString (obj, "dvb-guard");
    if (guard == NULL)
        return VLC_GUARD_AUTO;

    uint16_t a, b;
    int v = sscanf (guard, "%"SCNu16"/%"SCNu16, &a, &b);
    free (guard);
    switch (v)
    {
        case 1:
            /* Backward compatibility with VLC < 1.2 */
            if (a == 0)
                break;
            msg_Warn (obj, "\"guard=%"PRIu16"\" option is obsolete. "
                           "Use \"guard=1/%"PRIu16" instead.", a, a);
            b = a;
            a = 1;
            /* fall through */
        case 2:
            return VLC_GUARD(a, b);
    }
    return VLC_GUARD_AUTO;
}


/*** ATSC ***/
static int atsc_setup (vlc_object_t *obj, dvb_device_t *dev, uint64_t freq)
{
    const char *mod = var_InheritModulation (obj, "dvb-modulation");

    return dvb_set_atsc (dev, freq, mod);
}

static int cqam_setup (vlc_object_t *obj, dvb_device_t *dev, uint64_t freq)
{
    const char *mod = var_InheritModulation (obj, "dvb-modulation");

    return dvb_set_cqam (dev, freq, mod);
}

/*** DVB-C ***/
static int dvbc_setup (vlc_object_t *obj, dvb_device_t *dev, uint64_t freq)
{
    const char *mod = var_InheritModulation (obj, "dvb-modulation");
    uint32_t fec = var_InheritCodeRate (obj, "dvb-fec");
    unsigned srate = var_InheritInteger (obj, "dvb-srate");

    return dvb_set_dvbc (dev, freq, mod, srate, fec);
}

/*** DVB-S ***/
static char var_InheritPolarization (vlc_object_t *obj)
{
    char pol;
    char *polstr = var_InheritString (obj, "dvb-polarization");
    if (polstr != NULL)
    {
        pol = *polstr;
        free (polstr);
        if (unlikely(pol >= 'a' && pol <= 'z'))
            pol -= 'a' - 'A';
        return pol;
    }

    /* Backward compatibility with VLC for Linux < 1.2 */
    unsigned voltage = var_InheritInteger (obj, "dvb-voltage");
    switch (voltage)
    {
        case 13:  pol = 'V'; break;
        case 18:  pol = 'H'; break;
        default:  return 0;
    }

    msg_Warn (obj, "\"voltage=%u\" option is obsolete. "
                   "Use \"polarization=%c\" instead.", voltage, pol);
    return pol;
}

static void sec_setup (vlc_object_t *obj, dvb_device_t *dev, uint64_t freq)
{
    char pol = var_InheritPolarization (obj);
    unsigned lowf = var_InheritInteger (obj, "dvb-lnb-low");
    unsigned highf = var_InheritInteger (obj, "dvb-lnb-high");
    unsigned switchf = var_InheritInteger (obj, "dvb-lnb-switch");

    dvb_set_sec (dev, freq, pol, lowf, highf, switchf);
}

static int dvbs_setup (vlc_object_t *obj, dvb_device_t *dev, uint64_t freq)
{
    uint32_t fec = var_InheritCodeRate (obj, "dvb-fec");
    uint32_t srate = var_InheritInteger (obj, "dvb-srate");

    int ret = dvb_set_dvbs (dev, freq, srate, fec);
    if (ret == 0)
        sec_setup (obj, dev, freq);
    return ret;
}

static int dvbs2_setup (vlc_object_t *obj, dvb_device_t *dev, uint64_t freq)
{
    const char *mod = var_InheritModulation (obj, "dvb-modulation");
    uint32_t fec = var_InheritCodeRate (obj, "dvb-fec");
    uint32_t srate = var_InheritInteger (obj, "dvb-srate");
    int pilot = var_InheritInteger (obj, "dvb-pilot");
    int rolloff = var_InheritInteger (obj, "dvb-rolloff");
    uint8_t sid = var_InheritInteger (obj, "dvb-stream");

    int ret = dvb_set_dvbs2 (dev, freq, mod, srate, fec, pilot, rolloff, sid);
    if (ret == 0)
        sec_setup (obj, dev, freq);
    return ret;
}

/*** DVB-T ***/
static int dvbt_setup (vlc_object_t *obj, dvb_device_t *dev, uint64_t freq)
{
    const char *mod = var_InheritModulation (obj, "dvb-modulation");
    uint32_t fec_hp = var_InheritCodeRate (obj, "dvb-code-rate-hp");
    uint32_t fec_lp = var_InheritCodeRate (obj, "dvb-code-rate-lp");
    uint32_t guard = var_InheritGuardInterval (obj);
    uint32_t bw = var_InheritInteger (obj, "dvb-bandwidth");
    int tx = var_InheritInteger (obj, "dvb-transmission");
    int h = var_InheritInteger (obj, "dvb-hierarchy");

    return dvb_set_dvbt (dev, freq, mod, fec_hp, fec_lp, bw, tx, guard, h);
}

static int dvbt2_setup (vlc_object_t *obj, dvb_device_t *dev, uint64_t freq)
{
    const char *mod = var_InheritModulation (obj, "dvb-modulation");
    uint32_t fec = var_InheritCodeRate (obj, "dvb-fec");
    uint32_t guard = var_InheritGuardInterval (obj);
    uint32_t bw = var_InheritInteger (obj, "dvb-bandwidth");
    uint32_t plp = var_InheritInteger (obj, "dvb-plp-id");
    int tx = var_InheritInteger (obj, "dvb-transmission");

    return dvb_set_dvbt2 (dev, freq, mod, fec, bw, tx, guard, plp);
}

/*** ISDB-C ***/
static int isdbc_setup (vlc_object_t *obj, dvb_device_t *dev, uint64_t freq)
{
    const char *mod = var_InheritModulation (obj, "dvb-modulation");
    uint32_t fec = var_InheritCodeRate (obj, "dvb-fec");
    unsigned srate = var_InheritInteger (obj, "dvb-srate");

    return dvb_set_isdbc (dev, freq, mod, srate, fec);
}

/*** ISDB-S ***/
static int isdbs_setup (vlc_object_t *obj, dvb_device_t *dev, uint64_t freq)
{
    uint16_t ts_id = var_InheritInteger (obj, "dvb-ts-id");

    int ret = dvb_set_isdbs (dev, freq, ts_id);
    if (ret == 0)
        sec_setup (obj, dev, freq);
    return ret;
}

/*** ISDB-T ***/
static int isdbt_setup (vlc_object_t *obj, dvb_device_t *dev, uint64_t freq)
{
    isdbt_layer_t layers[3];
    uint32_t guard = var_InheritGuardInterval (obj);
    uint32_t bw = var_InheritInteger (obj, "dvb-bandwidth");
    int tx = var_InheritInteger (obj, "dvb-transmission");

    for (unsigned i = 0; i < 3; i++)
    {
        char varname[sizeof ("dvb-X-interleaving")];
        memcpy (varname, "dvb-X-", 4);
        varname[4] = 'a' + i;

        strcpy (varname + 6, "modulation");
        layers[i].modulation = var_InheritModulation (obj, varname);
        strcpy (varname + 6, "fec");
        layers[i].code_rate = var_InheritCodeRate (obj, varname);
        strcpy (varname + 6, "count");
        layers[i].segment_count = var_InheritInteger (obj, varname);
        strcpy (varname + 6, "interleaving");
        layers[i].time_interleaving = var_InheritInteger (obj, varname);
    }

    return dvb_set_isdbt (dev, freq, bw, tx, guard, layers);
}

static const struct
{
    const dtv_delivery_t delivery;
    tuner_setup_t const pf_setup;
    const char *psz_scheme;
} delsys_mappings[] = {
    { DTV_DELIVERY_ATSC,    atsc_setup,    "atsc"      },
    { DTV_DELIVERY_CQAM,    cqam_setup,    "cqam"      },
    { DTV_DELIVERY_DVB_C,   dvbc_setup,    "dvb-c"     },
    { DTV_DELIVERY_DVB_C2,  dvbc_setup,    "dvb-c2"    },
    { DTV_DELIVERY_DVB_S,   dvbs_setup,    "dvb-s"     },
    { DTV_DELIVERY_DVB_S2,  dvbs2_setup,   "dvb-s2"    },
    { DTV_DELIVERY_DVB_T,   dvbt_setup,    "dvb-t"     },
    { DTV_DELIVERY_DVB_T2,  dvbt2_setup,   "dvb-t2"    },
    { DTV_DELIVERY_ISDB_C,  isdbc_setup,   "isdb-c"    },
    { DTV_DELIVERY_ISDB_S,  isdbs_setup,   "isdb-s"    },
    { DTV_DELIVERY_ISDB_T,  isdbt_setup,   "isdb-t"    },
};

tuner_setup_t dtv_get_delivery_tuner_setup( dtv_delivery_t d )
{
    for (size_t i=0; i<ARRAY_SIZE(delsys_mappings); i++)
    {
        if ( delsys_mappings[i].delivery == d )
            return delsys_mappings[i].pf_setup;
    }
    return NULL;
}

static dtv_delivery_t GetDeliveryByScheme(const char *psz_scheme)
{
    for (size_t i=0; i<ARRAY_SIZE(delsys_mappings); i++)
    {
        if(!strcasecmp(psz_scheme, delsys_mappings[i].psz_scheme))
            return delsys_mappings[i].delivery;
    }
    return DTV_DELIVERY_NONE;
}

static inline dtv_delivery_t GetSingleDelivery( dtv_delivery_t d )
{
    if( d == 0 )
        return DTV_DELIVERY_NONE;
    else
        return 1 << ctz( d );
}

/** Determines which delivery system to use. */
static dtv_delivery_t GuessSystem (const char *scheme, dvb_device_t *dev)
{
    /* Specific delivery system is specified */
    dtv_delivery_t d = GetDeliveryByScheme(scheme);
    if(d != DTV_DELIVERY_NONE)
        return d;

    /* If the demodulator supports 2G, we cannot guess whether
     * 1G or 2G is intended. For backward compatibility, 1G is assumed
     * (this is not a limitation of Linux DVB). We will probably need something
     * smarter when 2G (semi automatic) scanning is implemented. */
    unsigned systems = dvb_enum_systems (dev);

    systems &= ~DTV_DELGROUP_G2;

    if( systems )
    {
        /* Only wave carrier is specified */
        if (!strcasecmp (scheme, "cable"))
        {
            return GetSingleDelivery(systems & DTV_DELGROUP_CABLE);
        }
        else if (!strcasecmp (scheme, "satellite"))
        {
            return GetSingleDelivery(systems & DTV_DELGROUP_SAT);
        }
        else if (!strcasecmp (scheme, "terrestrial"))
        {
            return GetSingleDelivery(systems & DTV_DELGROUP_TERRES);
        }
        else
        {
            /* Only standards family or nothing is specified */
            return GetSingleDelivery(systems);
        }
    }

    return DTV_DELIVERY_NONE;
}
