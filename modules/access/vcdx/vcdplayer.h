/*****************************************************************************
 * Copyright (C) 2003, 2004 Rocky Bernstein (for VLC authors and VideoLAN)
 * $Id$
 *
 * Authors: Rocky Bernstein <rocky@panix.com>
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

/* VCD Player header. More or less media-player independent. Or at least
   that is the goal. So we prefer bool to vlc_bool.
 */

#ifndef _VCDPLAYER_H_
#define _VCDPLAYER_H_

#include <libvcd/info.h>
#include <vlc_meta.h>
#include <vlc_input.h>
#include <vlc_access.h>

#define INPUT_DBG_META        1 /* Meta information */
#define INPUT_DBG_EVENT       2 /* input (keyboard/mouse) events */
#define INPUT_DBG_MRL         4 /* MRL parsing */
#define INPUT_DBG_EXT         8 /* Calls from external routines */
#define INPUT_DBG_CALL       16 /* routine calls */
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
   if (p_vcdplayer && p_vcdplayer->i_debug & mask) \
     msg_Dbg(p_access, "%s: "s, __func__ , ##args)
#else
#define dbg_print(mask, s, args...)
#endif

#define LOG_ERR(...)  msg_Err( p_access, __VA_ARGS__ )
#define LOG_WARN(...) msg_Warn( p_access, __VA_ARGS__ )

/*------------------------------------------------------------------
  General definitions and structures.
---------------------------------------------------------------------*/

/* Value for indefinite wait period on a still frame */
#define STILL_INDEFINITE_WAIT 255
/* Value when we have yet to finish reading blocks of a frame. */
#define STILL_READING          -5

typedef struct {
  lsn_t  start_LSN; /* LSN where play item starts */
  size_t size;      /* size in sector units of play item. */
} vcdplayer_play_item_info_t;

/*****************************************************************************
 * vcdplayer_t: VCD information
 *****************************************************************************/
typedef struct vcdplayer_input_s
{
  vcdinfo_obj_t *vcd;                   /* CD device descriptor */

  /*------------------------------------------------------------------
    User-settable options
   --------------------------------------------------------------*/
  unsigned int i_debug;                 /* Debugging mask */
  unsigned int i_blocks_per_read;       /* number of blocks per read */

  /*-------------------------------------------------------------
     Playback control fields
   --------------------------------------------------------------*/
  bool         in_still;                /* true if in still */
  int          i_lid;                   /* LID that play item is in. Implies
                                           PBC is on. VCDPLAYER_BAD_ENTRY if
                                           not none or not in PBC */
  PsdListDescriptor_t pxd;              /* If PBC is on, the relevant
                                            PSD/PLD */
  int          pdi;                     /* current pld index of pxd. -1 if
                                           no index*/
  vcdinfo_itemid_t play_item;           /* play-item, VCDPLAYER_BAD_ENTRY
                                           if none */
  vcdinfo_itemid_t loop_item;           /* Where do we loop back to?
                                           Meaningful only in a selection
                                           list */
  int          i_loop;                  /* # of times play-item has been
                                           played. Meaningful only in a
                                           selection list.              */
  track_t      i_track;                 /* current track number */

  /*-----------------------------------
     location fields
   ------------------------------------*/
  lsn_t        i_lsn;                   /* LSN of where we are right now */
  lsn_t        end_lsn;                 /* LSN of end of current
                                           entry/segment/track. This block
                                           can be read (and is not one after
                                           the "end").
                                        */
  lsn_t        origin_lsn;              /* LSN of start of seek/slider */
  lsn_t        track_lsn;               /* LSN of start track origin of track
                                           we are in. */
  lsn_t        track_end_lsn;           /* LSN of end of current track (if
                                           entry). */
  lsn_t *      p_entries;               /* Entry points */
  lsn_t *      p_segments;              /* Segments */
  bool         b_valid_ep;              /* Valid entry points flag */
  bool         b_end_of_track;          /* If the end of track was reached */

  /*--------------------------------------------------------------
    (S)VCD Medium information
   ---------------------------------------------------------------*/

  char        *psz_source;              /* (S)VCD drive or image filename */
  bool         b_svd;                   /* true if we have SVD info */
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

  /* Tracks, segment, and entry information. The number of entries for
     each is given by the corresponding i_* field above.  */
  vcdplayer_play_item_info_t *track;
  vcdplayer_play_item_info_t *segment;
  vcdplayer_play_item_info_t *entry;

  unsigned int i_titles;                /* # of navigatable titles. */

  /*
     # tracks + menu for segments + menu for LIDs
   */
  input_title_t *p_title[CDIO_CD_MAX_TRACKS+2];

  /* Probably gets moved into another structure...*/
  int            i_audio_nb;
  int            i_still;
  bool           b_end_of_cell;
  bool           b_track_length; /* Use track as max unit in seek */
  input_thread_t *p_input;
  access_t       *p_access;
 
} vcdplayer_t;

/* vcdplayer_read return status */
typedef enum {
  READ_BLOCK,
  READ_STILL_FRAME,
  READ_ERROR,
  READ_END,
} vcdplayer_read_status_t;


/* ----------------------------------------------------------------------
   Function Prototypes
  -----------------------------------------------------------------------*/

/*!
  Return true if playback control (PBC) is on
*/
bool vcdplayer_pbc_is_on(const vcdplayer_t *p_vcdplayer);

/*!
  Play item assocated with the "default" selection.

  Return false if there was some problem.
*/
bool vcdplayer_play_default( access_t * p_access );

/*!
  Play item assocated with the "next" selection.

  Return false if there was some problem.
*/
bool vcdplayer_play_next( access_t * p_access );

/*!
  Play item assocated with the "prev" selection.

  Return false if there was some problem.
*/
bool vcdplayer_play_prev( access_t * p_access );

/*!
  Play item assocated with the "return" selection.

  Return false if there was some problem.
*/
bool vcdplayer_play_return( access_t * p_access );

/*
   Set's start origin and size for subsequent seeks.
   input: p_vcd->i_lsn, p_vcd->play_item
   changed: p_vcd->origin_lsn, p_vcd->end_lsn
*/
void vcdplayer_set_origin(access_t *p_access, lsn_t i_lsn, track_t i_track,
                          const vcdinfo_itemid_t *p_itemid);

void vcdplayer_play(access_t *p_access, vcdinfo_itemid_t itemid);

vcdplayer_read_status_t vcdplayer_read (access_t * p_access_t, uint8_t *p_buf);

#endif /* _VCDPLAYER_H_ */
/*
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
