/*****************************************************************************
 * dvb.c : Satellite input module for vlc
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
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

#include <videolan/vlc.h>

/*****************************************************************************
 * Capabilities defined in the other files.
 *****************************************************************************/
void _M( access_getfunctions )( function_list_t * p_function_list );
void _M( demux_getfunctions )( function_list_t * p_function_list );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/

#define FREQ_TEXT N_("satellite transponder frequency")
#define FREQ_LONGTEXT ""

#define POL_TEXT N_("satellite transponder polarization")
#define POL_LONGTEXT ""

#define FEC_TEXT N_("satellite transponder FEC")
#define FEC_LONGTEXT ""

#define SRATE_TEXT N_("satellite transponder symbol rate")
#define SRATE_LONGTEXT ""

#define DISEQC_TEXT N_("use diseqc with antenna")
#define DISEQC_LONGTEXT ""

#define LNB_LOF1_TEXT N_("antenna lnb_lof1 (kHz)")
#define LNB_LOF1_LONGTEXT ""

#define LNB_LOF2_TEXT N_("antenna lnb_lof2 (kHz)")
#define LNB_LOF2_LONGTEXT ""

#define LNB_SLOF_TEXT N_("antenna lnb_slof (kHz)")
#define LNB_SLOF_LONGTEXT ""

MODULE_CONFIG_START
    ADD_CATEGORY_HINT( N_("Input"), NULL )
    ADD_INTEGER ( "frequency", 11954, NULL, FREQ_TEXT, FREQ_LONGTEXT )
    ADD_INTEGER ( "polarization", 0, NULL, POL_TEXT, POL_LONGTEXT )
    ADD_INTEGER ( "fec", 3, NULL, FEC_TEXT, FEC_LONGTEXT )
    ADD_INTEGER ( "symbol-rate", 27500, NULL, SRATE_TEXT, SRATE_LONGTEXT )
    ADD_BOOL    ( "diseqc", 0, DISEQC_TEXT, DISEQC_LONGTEXT )
    ADD_INTEGER ( "lnb-lof1", 10000, NULL, LNB_LOF1_TEXT, LNB_LOF1_LONGTEXT )
    ADD_INTEGER ( "lnb-lof2", 10000, NULL, LNB_LOF2_TEXT, LNB_LOF2_LONGTEXT )
    ADD_INTEGER ( "lnb-slof", 11700, NULL, LNB_SLOF_TEXT, LNB_SLOF_LONGTEXT )
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( _("satellite input module") )
    ADD_CAPABILITY( DEMUX, 0 )
    ADD_CAPABILITY( ACCESS, 50 )
    ADD_SHORTCUT( "satellite" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    _M( access_getfunctions )( &p_module->p_functions->access );
    _M( demux_getfunctions )( &p_module->p_functions->demux );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

