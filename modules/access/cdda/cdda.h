/*****************************************************************************
 * cdda.h : CD-DA input module header for vlc
 *          using libcdio, libvcd and libvcdinfo
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id$
 *
 * Author: Rocky Bernstein <rocky@panix.com>
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
#include "vlc_meta.h"

#ifdef HAVE_LIBCDDB
#include <cddb/cddb.h>
#endif

/*****************************************************************************
 * Debugging
 *****************************************************************************/
#define INPUT_DBG_META        1 /* Meta information */
#define INPUT_DBG_EVENT       2 /* Trace keyboard events */
#define INPUT_DBG_MRL         4 /* MRL debugging */
#define INPUT_DBG_EXT         8 /* Calls from external routines */
#define INPUT_DBG_CALL       16 /* all calls */
#define INPUT_DBG_LSN        32 /* LSN changes */
#define INPUT_DBG_SEEK       64 /* Seeks to set location */
#define INPUT_DBG_CDIO      128 /* Debugging from CDIO */
#define INPUT_DBG_CDDB      256 /* CDDB debugging  */

#define INPUT_DEBUG 1
#if INPUT_DEBUG
#define dbg_print(mask, s, args...) \
   if (p_cdda->i_debug & mask) \
     msg_Dbg(p_access, "%s: "s, __func__ , ##args)
#else
#define dbg_print(mask, s, args...)
#endif

/*****************************************************************************
 * cdda_data_t: CD audio information
 *****************************************************************************/
typedef struct cdda_data_s
{
    cddev_t     *p_cddev;                 /* CD device descriptor */
    int         i_tracks;                 /* # of tracks (titles) */

    /* Current position */
    int         i_track;                  /* Current track */
    lsn_t       i_sector;                 /* Current Sector */
    lsn_t *     p_sectors;                /* Track sectors */

    int         i_debug;                  /* Debugging mask */
    char *      psz_mcn;                  /* Media Catalog Number            */
    vlc_meta_t  *p_meta;

    input_title_t *p_title[CDIO_CD_MAX_TRACKS]; 


#ifdef HAVE_LIBCDDB
    int         i_cddb_enabled;
  struct  {
    bool             have_info;      /* True if we have any info */
    cddb_disc_t     *disc;           /* libcdio uses this to get disc info */
    int              disc_length;    /* Length in frames of cd. Used in
                                        CDDB lookups */
  } cddb;
#endif

    WAVEHEADER  waveheader;               /* Wave header for the output data */
    vlc_bool_t  b_header;

} cdda_data_t;

/*****************************************************************************
 * CDDAPlay: Arrange things so we play the specified track.
 * VLC_TRUE is returned if there was no error.
 *****************************************************************************/
vlc_bool_t  CDDAPlay         ( input_thread_t *, int );
