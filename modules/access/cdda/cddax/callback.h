/*****************************************************************************
 * callback.h : Callbacks for CD digital audio input module
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * $Id$
 *
 * Authors: Rocky Bernstein <rocky@panix.com>
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

#include <vlc/vlc.h>

/* 
   Minimum, maximum and default number of blocks we allow on read.
*/
#define MIN_BLOCKS_PER_READ 1
#define MAX_BLOCKS_PER_READ 25
#define DEFAULT_BLOCKS_PER_READ 20

int  CDDADebugCB  ( vlc_object_t *p_this, const char *psz_name,
                        vlc_value_t oldval, vlc_value_t val,
                        void *p_data );

int  CDDBEnabledCB( vlc_object_t *p_this, const char *psz_name,
                        vlc_value_t oldval, vlc_value_t val,
                        void *p_data );


int  CDTextEnabledCB( vlc_object_t *p_this, const char *psz_name,
			  vlc_value_t oldval, vlc_value_t val,
			  void *p_data );

int  CDTextPreferCB( vlc_object_t *p_this, const char *psz_name,
			 vlc_value_t oldval, vlc_value_t val,
			 void *p_data );

int  CDDANavModeCB( vlc_object_t *p_this, const char *psz_name,
		    vlc_value_t oldval, vlc_value_t val,
		    void *p_data );


int CDDABlocksPerReadCB ( vlc_object_t *p_this, const char *psz_name,
			      vlc_value_t oldval, vlc_value_t val, 
			      void *p_data );

