/*****************************************************************************
 * Copyright (C) 2003 Rocky Bernstein (for VideoLAN)
 * $Id: vcdplayer.h,v 1.1 2003/10/04 18:55:13 gbazin Exp $
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

#define INPUT_DBG_MRL         1 
#define INPUT_DBG_EXT         2 /* Calls from external routines */
#define INPUT_DBG_CALL        4 /* all calls */
#define INPUT_DBG_LSN         8 /* LSN changes */
#define INPUT_DBG_PBC        16 /* Playback control */
#define INPUT_DBG_CDIO       32 /* Debugging from CDIO */
#define INPUT_DBG_SEEK       64 /* Seeks to set location */
#define INPUT_DBG_STILL     128 /* Still-frame */
#define INPUT_DBG_VCDINFO   256 /* Debugging from VCDINFO */

#define INPUT_DEBUG 1
#if INPUT_DEBUG
#define dbg_print(mask, s, args...) \
   if (p_vcd->i_debug & mask) \
     msg_Dbg(p_input, "%s: "s, __func__ , ##args)
#else
#define dbg_print(mask, s, args...) 
#endif

#define LOG_ERR(args...) msg_Err( p_input, args )

/* vcdplayer_read return status */
typedef enum {
  READ_BLOCK,
  READ_STILL_FRAME,
  READ_ERROR,
  READ_END,
} vcdplayer_read_status_t;

/*****************************************************************************
 * thread_vcd_data_t: VCD information
 *****************************************************************************/
typedef struct thread_vcd_data_s
{
  vcdinfo_obj_t *vcd;                   /* CD device descriptor */
  int            in_still;              /*  0 if not in still, 
                                            -2 if in infinite loop
                                            -5 if a still but haven't 
                                            read wait time yet
                                            >0 number of seconds yet to 
                                            wait */
  unsigned int num_tracks;              /* Nb of tracks (titles) */
  unsigned int num_segments;            /* Nb of segments */
  unsigned int num_entries;             /* Nb of entries */
  unsigned int num_lids;                /* Nb of List IDs */
  vcdinfo_itemid_t play_item;           /* play-item, VCDPLAYER_BAD_ENTRY 
                                           if none */
  int          cur_lid;                 /* LID that play item is in. Implies 
                                           PBC is on. VCDPLAYER_BAD_ENTRY if 
                                           not none or not in PBC */
  PsdListDescriptor pxd;                /* If PBC is on, the relevant 
                                           PSD/PLD */
  int          pdi;                     /* current pld index of pxd. -1 if 
                                           no index*/
  vcdinfo_itemid_t loop_item;           /* Where do we loop back to? 
                                           Meaningful only in a selection 
                                           list */
  int          loop_count;              /* # of times play-item has been 
                                           played. Meaningful only in a 
                                           selection list.              */
  track_t      cur_track;               /* Current track number */
  lsn_t        cur_lsn;                 /* Current logical sector number */
  lsn_t        end_lsn;                 /* LSN of end of current 
                                           entry/segment/track. */
  lsn_t        origin_lsn;              /* LSN of start of seek/slider */
  lsn_t *      p_sectors;               /* Track sectors */
  lsn_t *      p_entries;               /* Entry points */
  lsn_t *      p_segments;              /* Segments */
  bool         b_valid_ep;              /* Valid entry points flag */
  vlc_bool_t   b_end_of_track;          /* If the end of track was reached */
  int          i_debug;                 /* Debugging mask */
  
} thread_vcd_data_t;

bool  vcdplayer_inc_play_item( input_thread_t *p_input );
bool  vcdplayer_pbc_is_on(const thread_vcd_data_t *p_this);

vcdplayer_read_status_t vcdplayer_pbc_nav ( input_thread_t * p_input );
vcdplayer_read_status_t vcdplayer_non_pbc_nav ( input_thread_t * p_input );
lid_t vcdplayer_selection2lid ( input_thread_t *p_input, int entry_num ) ;

#endif /* _VCDPLAYER_H_ */
/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
