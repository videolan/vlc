/*****************************************************************************
 * qpsk.c : Satellite input module for vlc
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Jean-Paul Saman <saman@natlab.research.philips.com>
 *          Christopher Ross <ross@natlab.research.philips.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */

#include <vlc/vlc.h>

/*****************************************************************************
 * External prototypes
 *****************************************************************************/
int  E_(Open)    ( vlc_object_t * );
void E_(Close)   ( vlc_object_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/

/* Satellite options */
#define ADAPTER_TEXT N_("adapter card to tune")
#define ADAPTER_LONGTEXT N_("adapter cards have a device file named /dev/dvb/adapter[n] with n>=0")

#define DEVICE_TEXT N_("device nummer to use on adapter")
#define DEVICE_LONGTEXT ""

#define FREQ_TEXT N_("satellite default transponder frequency")
#define FREQ_LONGTEXT ""

#define POL_TEXT N_("satellite default transponder polarization")
#define POL_LONGTEXT ""

#define FEC_TEXT N_("satellite default transponder FEC")
#define FEC_LONGTEXT N_("FEC=Forward Error Correction mode")

#define SRATE_TEXT N_("satellite default transponder symbol rate")
#define SRATE_LONGTEXT ""

#define DISEQC_TEXT N_("use diseqc with antenna")
#define DISEQC_LONGTEXT ""

#define LNB_LOF1_TEXT N_("antenna lnb_lof1 (kHz)")
#define LNB_LOF1_LONGTEXT ""

#define LNB_LOF2_TEXT N_("antenna lnb_lof2 (kHz)")
#define LNB_LOF2_LONGTEXT ""

#define LNB_SLOF_TEXT N_("antenna lnb_slof (kHz)")
#define LNB_SLOF_LONGTEXT ""

#define PROBE_TEXT	N_("probe the dvb card for capabilities (default disabled)")
#define PROBE_LONGTEXT N_("some dvb cards do not like to be probed for their capabilities")

/* Cable */
#define MODULATION_TEXT N_("modulation type")
#define MODULATION_LONGTEXT N_("modulation type for frontend device ")

/* Terrestrial */
#define CODE_RATE_HP_TEXT N_("terrestrial high priority stream code rate (FEC)")
#define CODE_RATE_HP_LONGTEXT ""

#define CODE_RATE_LP_TEXT N_("terrestrial low priority stream code rate (FEC)")
#define CODE_RATE_LP_LONGTEXT ""

#define BANDWIDTH_TEXT N_("terrestrial bandwidth")
#define BANDWIDTH_LONGTEXT N_("terrestrial bandwidth [0=auto,6,7,8 in MHz]")

#define GUARD_TEXT N_("terrestrial guard interval")
#define GUARD_LONGTEXT ""

#define TRANSMISSION_TEXT N_("terrestrial transmission mode")
#define TRANSMISSION_LONGTEXT ""

#define HIERARCHY_TEXT N_("terrestrial hierarchy mode")
#define HIERARCHY_LONGTEXT ""

vlc_module_begin();
    add_category_hint( N_("Input"), NULL, VLC_FALSE );
			  add_integer( "adapter", 1, NULL, ADAPTER_TEXT, ADAPTER_LONGTEXT, VLC_FALSE );
				add_integer( "device", 0, NULL, DEVICE_TEXT, DEVICE_LONGTEXT, VLC_FALSE );
        add_integer( "frequency", 11954, NULL, FREQ_TEXT, FREQ_LONGTEXT, VLC_FALSE );
        add_integer( "polarization", 0, NULL, POL_TEXT, POL_LONGTEXT, VLC_FALSE );
        add_integer( "fec", 3, NULL, FEC_TEXT, FEC_LONGTEXT, VLC_FALSE );
        add_integer( "symbol-rate", 27500, NULL, SRATE_TEXT, SRATE_LONGTEXT, VLC_FALSE );
        add_bool( "diseqc", 0, NULL, DISEQC_TEXT, DISEQC_LONGTEXT, VLC_FALSE );
        add_integer( "lnb-lof1", 10000, NULL,
                     LNB_LOF1_TEXT, LNB_LOF1_LONGTEXT, VLC_FALSE );
        add_integer( "lnb-lof2", 10000, NULL,
                     LNB_LOF2_TEXT, LNB_LOF2_LONGTEXT, VLC_FALSE );
        add_integer( "lnb-slof", 11700, NULL,
                     LNB_SLOF_TEXT, LNB_SLOF_LONGTEXT, VLC_FALSE );
			  add_bool( "probe", 0, NULL, PROBE_TEXT, PROBE_LONGTEXT, VLC_FALSE );
	      add_integer( "code-rate-hp", 9, NULL, CODE_RATE_HP_TEXT, CODE_RATE_HP_LONGTEXT, VLC_FALSE );
	      add_integer( "code-rate-lp", 9, NULL, CODE_RATE_LP_TEXT, CODE_RATE_LP_LONGTEXT, VLC_FALSE );
			  add_integer( "bandwidth", 0, NULL, BANDWIDTH_TEXT, BANDWIDTH_LONGTEXT, VLC_FALSE );
			  add_integer( "modulation", 0, NULL, MODULATION_TEXT, MODULATION_LONGTEXT, VLC_FALSE );
			  add_integer( "quard", 0, NULL, GUARD_TEXT, GUARD_LONGTEXT, VLC_TRUE );
			  add_integer( "transmission", 0, NULL, TRANSMISSION_TEXT, TRANSMISSION_LONGTEXT, VLC_TRUE );
			  add_integer( "hierarchy", 0, NULL, HIERARCHY_TEXT, HIERARCHY_LONGTEXT, VLC_TRUE );
    set_description( _("DVB input module with v4l2 support") );
    set_capability( "access", 0 );
		add_shortcut( "qpsk" );
    add_shortcut( "cable" );
    add_shortcut( "terrestrial" );
    add_shortcut( "dvb" );
    add_shortcut( "satellite" );
    set_callbacks( E_(Open), E_(Close) );
vlc_module_end();

