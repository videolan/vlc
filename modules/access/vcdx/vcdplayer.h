/*****************************************************************************
 * Copyright (C) 2003, 2004 Rocky Bernstein (for VideoLAN)
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

/* VCD Player header. More or less media-player independent */

#ifndef _VCDPLAYER_H_
#define _VCDPLAYER_H_

#include <libvcd/info.h>
#include "vlc_meta.h"

#define INPUT_DBG_META        1 /* Meta information */
#define INPUT_DBG_EVENT       2 /* input (keyboard/mouse) events */
#define INPUT_DBG_MRL         4 /* MRL parsing */
#define INPUT_DBG_EXT         8 /* Calls from external routines */
#define INPUT_DBG_CALL       16 /* all calls */
#define INPUT_DBG_LSN        32 /* LSN changes */
#define INPUT_DBG_PBC        64 /* Playback control */
#define INPUT_DBG_CDIO      128 /* Debugging from CDIO */
#define INPUT_DBG_SEEK      256 /* Seeks to set location */
#define INPUT_DBG_SEEK_CUR  512 /* Seeks to find current location */
#define INPUT_DBG_STILL    1024 /* Still-frame */
#define INPUT_DBG_VCDINFO  2048 /* Debugging from VCDINFO */

#define INPUT_DEBUG 1
#if INPUT_DEBUG
#define dbg_print(mask, s, args...) \
   if (p_vcd && p_vcd->i_debug & mask) \
     msg_Dbg(p_access, "%s: "s, __func__ , ##args)
#else
#define dbg_print(mask, s, args...) 
#endif

#define LOG_ERR(args...)  msg_Err( p_access, args )
#define LOG_WARN(args...) msg_Warn( p_access, args )

/* vcdplayer_read return status */
typedef enum {
  READ_BLOCK,
  READ_STILL_FRAME,
  READ_ERROR,
  READ_END,
} vcdplayer_read_status_t;

/*****************************************************************************
 * access_vcd_data_t: VCD information
 *****************************************************************************/
typedef struct thread_vcd_data_s
{
  vcdinfo_obj_t *vcd;                   /* CD device descriptor */

  /* Current State: position */
  int          i_debug;                 /* Debugging mask */
  vlc_bool_t   in_still;                /* true if in still */
  vcdinfo_itemid_t play_item;           /* play-item, VCDPLAYER_BAD_ENTRY 
                                           if none */
  int          i_lid;                   /* LID that play item is in. Implies 
                                           PBC is on. VCDPLAYER_BAD_ENTRY if 
                                           not none or not in PBC */
  PsdListDescriptor_t pxd;              /* If PBC is on, the relevant 
                                            PSD/PLD */
  int          pdi;                     /* current pld index of pxd. -1 if 
                                           no index*/
  vcdinfo_itemid_t loop_item;           /* Where do we loop back to? 
                                           Meaningful only in a selection 
                                           list */
  int          loop_count;              /* # of times play-item has been 
                                           played. Meaningful only in a 
                                           selection list.              */
  track_t      i_track;                 /* Current track number */
  lsn_t        i_lsn;                   /* Current logical sector number */
  lsn_t        end_lsn;                 /* LSN of end of current 
                                           entry/segment/track. */
  lsn_t        origin_lsn;              /* LSN of start of seek/slider */

  lsn_t *      p_entries;               /* Entry points */
  lsn_t *      p_segments;              /* Segments */
  vlc_bool_t   b_valid_ep;              /* Valid entry points flag */
  vlc_bool_t   b_end_of_track;          /* If the end of track was reached */

  /* Information about (S)VCD */
  char *       psz_source;              /* (S)VCD drive or image filename */
  vlc_bool_t   b_svd;                   /* true if we have SVD info */
  vlc_meta_t  *p_meta;
  track_t      i_tracks;                /* # of playable MPEG tracks. This is 
                                           generally one less than the number
                                           of CD tracks as the first CD track
                                           is an ISO-9660 track and is not
                                           playable.
                                        */
  unsigned int i_segments;              /* # of segments */
  unsigned int i_entries;               /* # of entries */
  unsigned int i_lids;                  /* # of List IDs */
  unsigned int i_titles;                /* # of navigatable titles. */

  input_title_t *p_title[CDIO_CD_MAX_TRACKS];

  /* Probably gets moved into another structure...*/
  intf_thread_t *p_intf;
  int            i_audio_nb;
  int            i_still_time;
  vlc_bool_t     b_end_of_cell;
  input_thread_t *p_input;
  
} access_vcd_data_t;

/*!
  Get the next play-item in the list given in the LIDs. Note play-item
  here refers to list of play-items for a single LID It shouldn't be
  confused with a user's list of favorite things to play or the 
  "next" field of a LID which moves us to a different LID.
 */
vlc_bool_t vcdplayer_inc_play_item( access_t *p_access );

/*!
  Return true if playback control (PBC) is on
*/
vlc_bool_t vcdplayer_pbc_is_on(const access_vcd_data_t *p_this);

/*!
  Play item assocated with the "default" selection.

  Return false if there was some problem.
*/
vlc_bool_t vcdplayer_play_default( access_t * p_access );

/*!
  Play item assocated with the "next" selection.

  Return false if there was some problem.
*/
vlc_bool_t vcdplayer_play_next( access_t * p_access );

/*!
  Play item assocated with the "prev" selection.

  Return false if there was some problem.
*/
vlc_bool_t vcdplayer_play_prev( access_t * p_access );

/*!
  Play item assocated with the "return" selection.

  Return false if there was some problem.
*/
vlc_bool_t
vcdplayer_play_return( access_t * p_access );

vcdplayer_read_status_t vcdplayer_pbc_nav     ( access_t * p_access );
vcdplayer_read_status_t vcdplayer_non_pbc_nav ( access_t * p_access );

#endif /* _VCDPLAYER_H_ */
/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
