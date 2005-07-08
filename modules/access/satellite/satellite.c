/*****************************************************************************
 * satellite.c : Satellite input module for vlc
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN (Centrale Réseaux) and its contributors
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

#define DEMUX_TEXT N_("Demux number")
#define DEMUX_LONGTEXT ""

#define TUNER_TEXT N_("Tuner number")
#define TUNER_LONGTEXT ""

#define FREQ_TEXT N_("Satellite default transponder frequency (kHz)")
#define FREQ_LONGTEXT ""

#define POL_TEXT N_("Satellite default transponder polarization")
#define POL_LONGTEXT ""

#define FEC_TEXT N_("Satellite default transponder FEC")
#define FEC_LONGTEXT ""

#define SRATE_TEXT N_("Satellite default transponder symbol rate (kHz)")
#define SRATE_LONGTEXT ""

#define DISEQC_TEXT N_("Use diseqc with antenna")
#define DISEQC_LONGTEXT ""

#define LNB_LOF1_TEXT N_("Antenna lnb_lof1 (kHz)")
#define LNB_LOF1_LONGTEXT ""

#define LNB_LOF2_TEXT N_("Antenna lnb_lof2 (kHz)")
#define LNB_LOF2_LONGTEXT ""

#define LNB_SLOF_TEXT N_("Antenna lnb_slof (kHz)")
#define LNB_SLOF_LONGTEXT ""

vlc_module_begin();
    set_description( _("Satellite input") );
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_ACCESS );

    add_integer( "dvb-dmx", 0, NULL, DEMUX_TEXT, DEMUX_LONGTEXT, VLC_FALSE );
    add_integer( "dvb-tuner", 0, NULL, TUNER_TEXT, TUNER_LONGTEXT, VLC_FALSE );
    add_integer( "frequency", 0, NULL, FREQ_TEXT, FREQ_LONGTEXT, VLC_FALSE );
    add_integer( "polarization", 0, NULL, POL_TEXT, POL_LONGTEXT, VLC_FALSE );
    add_integer( "fec", 3, NULL, FEC_TEXT, FEC_LONGTEXT, VLC_FALSE );
    add_integer( "symbol-rate", 27500000, NULL, SRATE_TEXT, SRATE_LONGTEXT,
                  VLC_FALSE );
    add_bool( "diseqc", 0, NULL, DISEQC_TEXT, DISEQC_LONGTEXT, VLC_TRUE );
    add_integer( "lnb-lof1", 10000000, NULL,
                 LNB_LOF1_TEXT, LNB_LOF1_LONGTEXT, VLC_TRUE );
    add_integer( "lnb-lof2", 10000000, NULL,
                 LNB_LOF2_TEXT, LNB_LOF2_LONGTEXT, VLC_TRUE );
    add_integer( "lnb-slof", 11700000, NULL,
                 LNB_SLOF_TEXT, LNB_SLOF_LONGTEXT, VLC_TRUE );

    set_capability( "access", 0 );
    add_shortcut( "sat" );
    set_callbacks( E_(Open), E_(Close) );
vlc_module_end();

