/*****************************************************************************
 * cdda.h : CD-DA input module header for vlc
 *          using libcdio, libvcd and libvcdinfo
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: cdda.h,v 1.1 2003/11/26 03:35:26 rocky Exp $
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

#include "../vcdx/cdrom.h"

/*****************************************************************************
 * Debugging 
 *****************************************************************************/
#define INPUT_DBG_MRL         1 
#define INPUT_DBG_EVENT       2 /* Trace keyboard events */
#define INPUT_DBG_EXT         4 /* Calls from external routines */
#define INPUT_DBG_CALL        8 /* all calls */
#define INPUT_DBG_LSN        16 /* LSN changes */
#define INPUT_DBG_CDIO       32 /* Debugging from CDIO */
#define INPUT_DBG_SEEK       64 /* Seeks to set location */

#define INPUT_DEBUG 1
#if INPUT_DEBUG
#define dbg_print(mask, s, args...) \
   if (p_cdda->i_debug & mask) \
     msg_Dbg(p_input, "%s: "s, __func__ , ##args)
#else
#define dbg_print(mask, s, args...) 
#endif

/*****************************************************************************
 * cdda_data_t: CD audio information
 *****************************************************************************/
typedef struct cdda_data_s
{
    cddev_t     *p_cddev;                           /* CD device descriptor */
    int         i_nb_tracks;                        /* Nb of tracks (titles) */
    int         i_track;                                    /* Current track */
    lsn_t       i_sector;                                  /* Current Sector */
    lsn_t *     p_sectors;                                  /* Track sectors */
    vlc_bool_t  b_end_of_track;           /* If the end of track was reached */
    int         i_debug;                  /* Debugging mask */
    intf_thread_t *p_intf;

} cdda_data_t;

/*****************************************************************************
 * CDDAPlay: Arrange things so we play the specified track.
 * VLC_TRUE is returned if there was no error.
 *****************************************************************************/
vlc_bool_t  CDDAPlay         ( input_thread_t *, int );
