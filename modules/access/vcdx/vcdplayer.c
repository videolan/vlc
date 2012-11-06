/*****************************************************************************
 * vcdplayer.c : VCD input module for vlc
 *               using libcdio, libvcd and libvcdinfo
 *****************************************************************************
 * Copyright (C) 2003, 2004 Rocky Bernstein <rocky@panix.com>
 * $Id$
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

/*
   This contains more of the vlc-independent parts that might be used
   in any VCD input module for a media player. However at present there
   are vlc-specific structures. See also vcdplayer.c of the xine plugin.
 */
/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_input.h>
#include <vlc_interface.h>
#include <vlc_rand.h>

#include "vcd.h"
#include "vcdplayer.h"
#include "access.h"

#include <cdio/cdio.h>
#include <cdio/util.h>
#include <libvcd/info.h>

/*!
  Return true if playback control (PBC) is on
*/
bool
vcdplayer_pbc_is_on( const vcdplayer_t *p_vcdplayer )
{
  return VCDINFO_INVALID_ENTRY != p_vcdplayer->i_lid;
}

/* Given an itemid, return the size for the object (via information
   previously stored when opening the vcd). */
static size_t
vcdplayer_get_item_size(access_t * p_access, vcdinfo_itemid_t itemid)
{
  vcdplayer_t *p_vcdplayer= (vcdplayer_t *)p_access->p_sys;

  switch (itemid.type) {
  case VCDINFO_ITEM_TYPE_ENTRY:
    return p_vcdplayer->entry[itemid.num].size;
    break;
  case VCDINFO_ITEM_TYPE_SEGMENT:
    return p_vcdplayer->segment[itemid.num].size;
    break;
  case VCDINFO_ITEM_TYPE_TRACK:
    return p_vcdplayer->track[itemid.num-1].size;
    break;
  case VCDINFO_ITEM_TYPE_LID:
    /* Play list number (LID) */
    return 0;
    break;
  case VCDINFO_ITEM_TYPE_NOTFOUND:
  case VCDINFO_ITEM_TYPE_SPAREID2:
  default:
    LOG_ERR("%s %d", "bad item type", itemid.type);
    return 0;
  }
}

static void
vcdplayer_update_entry( access_t * p_access, uint16_t ofs,
                        uint16_t *entry, const char *label)
{
  vcdplayer_t *p_vcdplayer= (vcdplayer_t *)p_access->p_sys;

  if ( ofs == VCDINFO_INVALID_OFFSET ) {
    *entry = VCDINFO_INVALID_ENTRY;
  } else {
    vcdinfo_offset_t *off = vcdinfo_get_offset_t(p_vcdplayer->vcd, ofs);
    if (off != NULL) {
      *entry = off->lid;
      dbg_print(INPUT_DBG_PBC, "%s: LID %d", label, off->lid);
    } else
      *entry = VCDINFO_INVALID_ENTRY;
  }
}

/* Handles navigation when NOT in PBC reaching the end of a play item.

   The navigations rules here may be sort of made up, but the intent
   is to do something that's probably right or helpful.

   return true if the caller should return.
*/
static vcdplayer_read_status_t
vcdplayer_non_pbc_nav ( access_t *p_access, uint8_t *wait_time )
{
  vcdplayer_t *p_vcdplayer= (vcdplayer_t *)p_access->p_sys;

  /* Not in playback control. Do we advance automatically or stop? */
  switch (p_vcdplayer->play_item.type) {
  case VCDINFO_ITEM_TYPE_TRACK:
  case VCDINFO_ITEM_TYPE_ENTRY: {
    if ( ! vcdplayer_play_next( p_access ) )
      return READ_END;
    break;
  }
  case VCDINFO_ITEM_TYPE_SPAREID2:
    dbg_print( (INPUT_DBG_STILL|INPUT_DBG_LSN),
               "SPAREID2" );
    if (p_vcdplayer->in_still)
    {
      dbg_print( (INPUT_DBG_STILL|INPUT_DBG_LSN),
                 "End of still spareid2" );
      *wait_time = 255;
      return READ_STILL_FRAME ;
    }
    return READ_END;
  case VCDINFO_ITEM_TYPE_NOTFOUND:
    LOG_ERR ("NOTFOUND outside PBC -- not supposed to happen");
    return READ_ERROR;
  case VCDINFO_ITEM_TYPE_LID:
    LOG_ERR ("LID outside PBC -- not supposed to happen");
    return READ_ERROR;
  case VCDINFO_ITEM_TYPE_SEGMENT:
      /* Hack: Just go back and do still again */
    /* FIXME */
    if (p_vcdplayer->in_still)
    {
      dbg_print( (INPUT_DBG_STILL|INPUT_DBG_LSN),
                 "End of still Segment" );
      *wait_time = 10;
      return READ_STILL_FRAME;
    }
    return READ_END;
  }
  return READ_BLOCK;
}

/*!
  Set reading to play an entire track.
*/
static void
_vcdplayer_set_track(access_t * p_access, track_t i_track)
{
  vcdplayer_t *p_vcdplayer = (vcdplayer_t *)p_access->p_sys;
  if (i_track < 1 || i_track > p_vcdplayer->i_tracks)
    return;
  else {
    const vcdinfo_obj_t *p_vcdinfo = p_vcdplayer->vcd;
    vcdinfo_itemid_t itemid;

    itemid.num             = i_track;
    itemid.type            = VCDINFO_ITEM_TYPE_TRACK;
    p_vcdplayer->in_still  = 0;

    VCDSetOrigin(p_access, vcdinfo_get_track_lsn(p_vcdinfo, i_track),
         i_track, &itemid);

    dbg_print(INPUT_DBG_LSN, "LSN: %u", p_vcdplayer->i_lsn);
  }
}

/*!
  Set reading to play an entry
*/
static void
_vcdplayer_set_entry(access_t * p_access, unsigned int num)
{
  vcdplayer_t   *p_vcdplayer = (vcdplayer_t *)p_access->p_sys;
  vcdinfo_obj_t *p_vcdinfo   = p_vcdplayer->vcd;
  const unsigned int   i_entries = vcdinfo_get_num_entries(p_vcdinfo);

  if (num >= i_entries) {
    LOG_ERR("%s %d", "bad entry number", num);
    return;
  } else {
    vcdinfo_itemid_t itemid;

    itemid.num            = num;
    itemid.type           = VCDINFO_ITEM_TYPE_ENTRY;
    p_vcdplayer->i_still  = 0;

    VCDSetOrigin(p_access, vcdinfo_get_entry_lsn(p_vcdinfo, num),
        vcdinfo_get_track(p_vcdinfo, num), &itemid);

    dbg_print(INPUT_DBG_LSN, "LSN: %u, track_end LSN: %u",
              p_vcdplayer->i_lsn, p_vcdplayer->track_end_lsn);
  }
}

/*!
  Set reading to play an segment (e.g. still frame)
*/
static void
_vcdplayer_set_segment(access_t * p_access, unsigned int num)
{
  vcdplayer_t   *p_vcdplayer = (vcdplayer_t *)p_access->p_sys;
  vcdinfo_obj_t *p_vcdinfo   = p_vcdplayer->vcd;
  segnum_t       i_segs    = vcdinfo_get_num_segments(p_vcdinfo);

  if (num >= i_segs) {
    LOG_ERR("%s %d", "bad segment number", num);
    return;
  } else {
    vcdinfo_itemid_t itemid;

    if (VCDINFO_NULL_LSN==p_vcdplayer->i_lsn) {
      LOG_ERR("%s %d",
              "Error in getting current segment number", num);
      return;
    }
 
    itemid.num = num;
    itemid.type = VCDINFO_ITEM_TYPE_SEGMENT;

    VCDSetOrigin(p_access, vcdinfo_get_seg_lsn(p_vcdinfo, num), 0, &itemid);
 
    dbg_print(INPUT_DBG_LSN, "LSN: %u", p_vcdplayer->i_lsn);
  }
}

/* Play entry. */
/* Play a single item. */
static bool
vcdplayer_play_single_item( access_t * p_access, vcdinfo_itemid_t itemid)
{
  vcdplayer_t   *p_vcdplayer = (vcdplayer_t *)p_access->p_sys;
  vcdinfo_obj_t *p_vcdinfo = p_vcdplayer->vcd;

  dbg_print(INPUT_DBG_CALL, "called itemid.num: %d, itemid.type: %d",
            itemid.num, itemid.type);

  p_vcdplayer->i_still = 0;

  switch (itemid.type) {
  case VCDINFO_ITEM_TYPE_SEGMENT:
    {
      vcdinfo_video_segment_type_t segtype
        = vcdinfo_get_video_type(p_vcdinfo, itemid.num);
      segnum_t i_segs = vcdinfo_get_num_segments(p_vcdinfo);

      dbg_print(INPUT_DBG_PBC, "%s (%d), itemid.num: %d",
                vcdinfo_video_type2str(p_vcdinfo, itemid.num),
                (int) segtype, itemid.num);

      if (itemid.num >= i_segs) return false;
      _vcdplayer_set_segment(p_access, itemid.num);
 
      switch (segtype)
        {
        case VCDINFO_FILES_VIDEO_NTSC_STILL:
        case VCDINFO_FILES_VIDEO_NTSC_STILL2:
        case VCDINFO_FILES_VIDEO_PAL_STILL:
        case VCDINFO_FILES_VIDEO_PAL_STILL2:
          p_vcdplayer->i_still = STILL_READING;
          break;
        default:
          p_vcdplayer->i_still = 0;
        }
 
      break;
    }
 
  case VCDINFO_ITEM_TYPE_TRACK:
    dbg_print(INPUT_DBG_PBC, "track %d", itemid.num);
    if (itemid.num < 1 || itemid.num > p_vcdplayer->i_tracks) return false;
    _vcdplayer_set_track(p_access, itemid.num);
    break;
 
  case VCDINFO_ITEM_TYPE_ENTRY:
    {
      unsigned int i_entries = vcdinfo_get_num_entries(p_vcdinfo);
      dbg_print(INPUT_DBG_PBC, "entry %d", itemid.num);
      if (itemid.num >= i_entries) return false;
      _vcdplayer_set_entry(p_access, itemid.num);
      break;
    }
 
  case VCDINFO_ITEM_TYPE_LID:
    LOG_ERR("%s", "Should have converted p_vcdplayer above");
    return false;
    break;

  case VCDINFO_ITEM_TYPE_NOTFOUND:
    dbg_print(INPUT_DBG_PBC, "play nothing");
    p_vcdplayer->i_lsn = p_vcdplayer->end_lsn;
    return false;

  default:
    LOG_ERR("item type %d not implemented.", itemid.type);
    return false;
  }
 
  p_vcdplayer->play_item = itemid;

  /* Some players like xine, have a fifo queue of audio and video buffers
     that need to be flushed when playing a new selection. */
  /*  if (p_vcdplayer->flush_buffers)
      p_vcdplayer->flush_buffers(); */
  return true;
}

/*
   Set's start origin and size for subsequent seeks.
   input: p_vcdplayer->i_lsn, p_vcdplayer->play_item
   changed: p_vcdplayer->origin_lsn, p_vcdplayer->end_lsn
*/

void
vcdplayer_set_origin(access_t *p_access, lsn_t i_lsn, track_t i_track,
             const vcdinfo_itemid_t *p_itemid)
{
  vcdplayer_t *p_vcdplayer = (vcdplayer_t *)p_access->p_sys;
  const size_t i_size= vcdplayer_get_item_size(p_access, *p_itemid);

  if( VCDINFO_NULL_LSN == i_lsn )
  {
    LOG_ERR("%s %d", "Invalid LSN for track", i_track);
    return;
  }

  p_vcdplayer->play_item.num  = p_itemid->num;
  p_vcdplayer->play_item.type = p_itemid->type;
  p_vcdplayer->i_lsn          = i_lsn;
  p_vcdplayer->end_lsn        = p_vcdplayer->i_lsn + i_size;
  p_vcdplayer->origin_lsn     = p_vcdplayer->i_lsn;
  p_vcdplayer->i_track        = i_track;
  p_vcdplayer->track_lsn      = vcdinfo_get_track_lsn(p_vcdplayer->vcd,
                              i_track);
  p_vcdplayer->track_end_lsn  = p_vcdplayer->track_lsn +
    vcdinfo_get_track_sect_count(p_vcdplayer->vcd, i_track);

  dbg_print((INPUT_DBG_CALL|INPUT_DBG_LSN),
        "lsn %u, end LSN: %u item.num %d, item.type %d",
        p_vcdplayer->i_lsn, p_vcdplayer->end_lsn,
        p_vcdplayer->play_item.num, p_vcdplayer->play_item.type);
}

/*!
  Get the next play-item in the list given in the LIDs. Note play-item
  here refers to list of play-items for a single LID It shouldn't be
  confused with a user's list of favorite things to play or the
  "next" field of a LID which moves us to a different LID.
 */
static bool
vcdplayer_inc_play_item(access_t *p_access)
{
  vcdplayer_t *p_vcdplayer = (vcdplayer_t *)p_access->p_sys;
  int noi;

  dbg_print(INPUT_DBG_CALL, "called pli: %d", p_vcdplayer->pdi);

  if ( NULL == p_vcdplayer || NULL == p_vcdplayer->pxd.pld  )
    return false;

  noi = vcdinf_pld_get_noi(p_vcdplayer->pxd.pld);
  if ( noi <= 0 )
    return false;
 
  /* Handle delays like autowait or wait here? */

  p_vcdplayer->pdi++;

  if ( p_vcdplayer->pdi < 0 || p_vcdplayer->pdi >= noi )
    return false;

  uint16_t trans_itemid_num=vcdinf_pld_get_play_item(p_vcdplayer->pxd.pld,
                                                     p_vcdplayer->pdi);
  vcdinfo_itemid_t trans_itemid;

  if (VCDINFO_INVALID_ITEMID == trans_itemid_num) return false;
 
  vcdinfo_classify_itemid(trans_itemid_num, &trans_itemid);
  dbg_print(INPUT_DBG_PBC, "  play-item[%d]: %s",
            p_vcdplayer->pdi, vcdinfo_pin2str (trans_itemid_num));
  return vcdplayer_play_single_item(p_access, trans_itemid);
}

void
vcdplayer_play(access_t *p_access, vcdinfo_itemid_t itemid)
{
  vcdplayer_t *p_vcdplayer = (vcdplayer_t *)p_access->p_sys;

  dbg_print(INPUT_DBG_CALL, "called itemid.num: %d itemid.type: %d",
            itemid.num, itemid.type);

  if  (!vcdplayer_pbc_is_on(p_vcdplayer)) {
    vcdplayer_play_single_item(p_access, itemid);
  } else {
    /* PBC on - Itemid.num is LID. */

    vcdinfo_obj_t *p_vcdinfo = p_vcdplayer->vcd;

    if (p_vcdinfo == NULL)
      return;

    p_vcdplayer->i_lid = itemid.num;
    vcdinfo_lid_get_pxd(p_vcdinfo, &(p_vcdplayer->pxd), itemid.num);
 
    switch (p_vcdplayer->pxd.descriptor_type) {
 
    case PSD_TYPE_SELECTION_LIST:
    case PSD_TYPE_EXT_SELECTION_LIST: {
      vcdinfo_itemid_t trans_itemid;
      uint16_t trans_itemid_num;

      if (p_vcdplayer->pxd.psd == NULL) return;
      trans_itemid_num  = vcdinf_psd_get_itemid(p_vcdplayer->pxd.psd);
      vcdinfo_classify_itemid(trans_itemid_num, &trans_itemid);
      p_vcdplayer->i_loop     = 1;
      p_vcdplayer->loop_item  = trans_itemid;
      vcdplayer_play_single_item(p_access, trans_itemid);
      break;
    }
 
    case PSD_TYPE_PLAY_LIST: {
      if (p_vcdplayer->pxd.pld == NULL) return;
      p_vcdplayer->pdi = -1;
      vcdplayer_inc_play_item(p_access);
      break;
    }
 
    case PSD_TYPE_END_LIST:
    case PSD_TYPE_COMMAND_LIST:
 
    default:
      ;
    }
  }
}

/* Handles PBC navigation when reaching the end of a play item. */
static vcdplayer_read_status_t
vcdplayer_pbc_nav ( access_t * p_access, uint8_t *wait_time )
{
  vcdplayer_t *p_vcdplayer= (vcdplayer_t *)p_access->p_sys;

  /* We are in playback control. */
  vcdinfo_itemid_t itemid;

  /* The end of an entry is really the end of the associated
     sequence (or track). */
 
  if ( (VCDINFO_ITEM_TYPE_ENTRY == p_vcdplayer->play_item.type) &&
       (p_vcdplayer->i_lsn < p_vcdplayer->end_lsn) ) {
    /* Set up to just continue to the next entry */
    p_vcdplayer->play_item.num++;
    dbg_print( (INPUT_DBG_LSN|INPUT_DBG_PBC),
               "continuing into next entry: %u", p_vcdplayer->play_item.num);
    vcdplayer_play_single_item( p_access, p_vcdplayer->play_item );
    /* p_vcdplayer->update_title(); */
    return READ_BLOCK;
  }
 
  switch (p_vcdplayer->pxd.descriptor_type) {
  case PSD_TYPE_END_LIST:
    return READ_END;
    break;
  case PSD_TYPE_PLAY_LIST: {
    if (vcdplayer_inc_play_item(p_access))
      return READ_BLOCK;

    /* Set up for caller process wait time given. */
    if (p_vcdplayer->i_still) {
      *wait_time = vcdinf_get_wait_time(p_vcdplayer->pxd.pld);
      dbg_print((INPUT_DBG_PBC|INPUT_DBG_STILL),
        "playlist wait time: %d", *wait_time);
      return READ_STILL_FRAME;
    }

    /* Wait time has been processed; continue with next entry. */
    vcdplayer_update_entry( p_access,
                            vcdinf_pld_get_next_offset(p_vcdplayer->pxd.pld),
                            &itemid.num, "next" );
    itemid.type = VCDINFO_ITEM_TYPE_LID;
    vcdplayer_play( p_access, itemid );
    break;
  }
  case PSD_TYPE_SELECTION_LIST:     /* Selection List (+Ext. for SVCD) */
  case PSD_TYPE_EXT_SELECTION_LIST: /* Extended Selection List (VCD2.0) */
    {
      uint16_t timeout_offs = vcdinf_get_timeout_offset(p_vcdplayer->pxd.psd);
      uint16_t max_loop     = vcdinf_get_loop_count(p_vcdplayer->pxd.psd);
      vcdinfo_offset_t *offset_timeout_LID =
        vcdinfo_get_offset_t(p_vcdplayer->vcd, timeout_offs);
 
      dbg_print(INPUT_DBG_PBC, "looped: %d, max_loop %d",
                p_vcdplayer->i_loop, max_loop);
 
      /* Set up for caller process wait time given. */
      if (p_vcdplayer->i_still) {
        *wait_time = vcdinf_get_timeout_time(p_vcdplayer->pxd.psd);
        dbg_print((INPUT_DBG_PBC|INPUT_DBG_STILL),
                  "playlist wait_time: %d", *wait_time);
        return READ_STILL_FRAME;
      }
 
      /* Wait time has been processed; continue with next entry. */
      /* Handle any looping given. */
      if ( max_loop == 0 || p_vcdplayer->i_loop < max_loop ) {
        p_vcdplayer->i_loop++;
        if (p_vcdplayer->i_loop == 0x7f) p_vcdplayer->i_loop = 0;
        vcdplayer_play_single_item(p_access, p_vcdplayer->loop_item);
        /* if (p_vcdplayer->i_still) p_vcdplayer->force_redisplay();*/
        return READ_BLOCK;
      }
 
      /* Looping finished and wait finished. Move to timeout
         entry or next entry, or handle still. */
 
      if (NULL != offset_timeout_LID) {
        /* Handle timeout_LID */
        itemid.num  = offset_timeout_LID->lid;
        itemid.type = VCDINFO_ITEM_TYPE_LID;
        dbg_print(INPUT_DBG_PBC, "timeout to: %d", itemid.num);
        vcdplayer_play( p_access, itemid );
        return READ_BLOCK;
      } else {
        int i_selections = vcdinf_get_num_selections(p_vcdplayer->pxd.psd);
        if (i_selections > 0) {
          /* Pick a random selection. */
          unsigned int bsn=vcdinf_get_bsn(p_vcdplayer->pxd.psd);
          int rand_selection=bsn +
            ((unsigned)vlc_lrand48() % (unsigned)i_selections);
          lid_t rand_lid=vcdinfo_selection_get_lid (p_vcdplayer->vcd,
                            p_vcdplayer->i_lid,
                            rand_selection);
          itemid.num = rand_lid;
          itemid.type = VCDINFO_ITEM_TYPE_LID;
          dbg_print(INPUT_DBG_PBC, "random selection %d, lid: %d",
                    rand_selection - bsn, rand_lid);
          vcdplayer_play( p_access, itemid );
          return READ_BLOCK;
        } else if (p_vcdplayer->i_still) {
          /* Hack: Just go back and do still again */
          msleep(10000);
          return READ_STILL_FRAME;
        }
      }
      break;
    }
  default:
    ;
  }
  /* FIXME: Should handle autowait ...  */

  return READ_ERROR;
}

/*!
  Read block into p_buf and return the status back.

  This routine is a bit complicated because on reaching the end of
  a track or entry we may automatically advance to the item, or
  interpret the next item in the playback-control list.
*/
vcdplayer_read_status_t
vcdplayer_read (access_t * p_access, uint8_t *p_buf)
{

  /* p_access->handle_events (); */
  uint8_t wait_time=0;

  vcdplayer_t *p_vcdplayer= (vcdplayer_t *)p_access->p_sys;
  if ( p_vcdplayer->i_lsn > p_vcdplayer->end_lsn ) {
    vcdplayer_read_status_t read_status;
 
    /* We've run off of the end of this entry. Do we continue or stop? */
    dbg_print( (INPUT_DBG_LSN|INPUT_DBG_PBC),
              "end reached, cur: %u, end: %u\n",
              p_vcdplayer->i_lsn, p_vcdplayer->end_lsn);

  handle_item_continuation:
    read_status = vcdplayer_pbc_is_on( p_vcdplayer )
      ? vcdplayer_pbc_nav( p_access, &wait_time )
      : vcdplayer_non_pbc_nav( p_access, &wait_time );

    if (READ_STILL_FRAME == read_status) {
      *p_buf = wait_time;
      return READ_STILL_FRAME;
    }

    if (READ_BLOCK != read_status) return read_status;
  }

  /* Read the next block.
 
    Important note: we probably speed things up by removing "data"
    and the memcpy to it by extending vcd_image_source_read_mode2
    to allow a mode to do what's below in addition to its
    "raw" and "block" mode. It also would probably improve the modularity
    a little bit as well.
  */

  {
    CdIo *p_img = vcdinfo_get_cd_image(p_vcdplayer->vcd);
    typedef struct {
      uint8_t subheader [CDIO_CD_SUBHEADER_SIZE];
      uint8_t data    [M2F2_SECTOR_SIZE];
      uint8_t spare     [4];
    } vcdsector_t;
    vcdsector_t vcd_sector;

    do {
      if (cdio_read_mode2_sector(p_img, &vcd_sector,
                                 p_vcdplayer->i_lsn, true)!=0) {
        dbg_print(INPUT_DBG_LSN, "read error\n");
        p_vcdplayer->i_lsn++;
        return READ_ERROR;
      }
      p_vcdplayer->i_lsn++;

      if ( p_vcdplayer->i_lsn >= p_vcdplayer->end_lsn ) {
        /* We've run off of the end of this entry. Do we continue or stop? */
        dbg_print( (INPUT_DBG_LSN|INPUT_DBG_PBC),
                   "end reached in reading, cur: %u, end: %u\n",
                   p_vcdplayer->i_lsn, p_vcdplayer->end_lsn);
        break;
      }
 
      /* Check header ID for a padding sector and simply discard
         these.  It is alleged that VCD's put these in to keep the
         bitrate constant.
      */
    } while((vcd_sector.subheader[2]&~0x01)==0x60);

    if ( p_vcdplayer->i_lsn >= p_vcdplayer->end_lsn )
      /* We've run off of the end of this entry. Do we continue or stop? */
      goto handle_item_continuation;
 
    memcpy (p_buf, vcd_sector.data, M2F2_SECTOR_SIZE);
    return READ_BLOCK;
  }
}

/*!
  Play item assocated with the "default" selection.

  Return false if there was some problem.
*/
bool
vcdplayer_play_default( access_t * p_access )
{
  vcdplayer_t *p_vcdplayer= (vcdplayer_t *)p_access->p_sys;

  vcdinfo_itemid_t itemid;

  if (!p_vcdplayer) {
    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_PBC),
           "null p_vcdplayer" );
    return VLC_EGENERIC;
  }
 

  dbg_print( (INPUT_DBG_CALL|INPUT_DBG_PBC),
         "current: %d" , p_vcdplayer->play_item.num);

  itemid.type = p_vcdplayer->play_item.type;

  if (vcdplayer_pbc_is_on(p_vcdplayer)) {

#if defined(LIBVCD_VERSION)
    lid_t lid=vcdinfo_get_multi_default_lid(p_vcdplayer->vcd, p_vcdplayer->i_lid,
                        p_vcdplayer->i_lsn);

    if (VCDINFO_INVALID_LID != lid) {
      itemid.num  = lid;
      itemid.type = VCDINFO_ITEM_TYPE_LID;
      dbg_print(INPUT_DBG_PBC, "DEFAULT to %d", itemid.num);
    } else {
      dbg_print(INPUT_DBG_PBC, "no DEFAULT for LID %d", p_vcdplayer->i_lid);
      return VLC_EGENERIC;
    }

#else
    vcdinfo_lid_get_pxd(p_vcdplayer->vcd, &(p_vcdplayer->pxd),
                        p_vcdplayer->i_lid);
 
    switch (p_vcdplayer->pxd.descriptor_type) {
    case PSD_TYPE_SELECTION_LIST:
    case PSD_TYPE_EXT_SELECTION_LIST:
      if (p_vcdplayer->pxd.psd == NULL) return false;
      vcdplayer_update_entry(p_access,
                             vcdinfo_get_default_offset(p_vcdplayer->vcd,
                                                        p_vcdplayer->i_lid),
                             &itemid.num, "default");
      break;

    case PSD_TYPE_PLAY_LIST:
    case PSD_TYPE_END_LIST:
    case PSD_TYPE_COMMAND_LIST:
      LOG_WARN( "There is no PBC 'default' selection here" );
      return false;
    }
#endif /* LIBVCD_VERSION (< 0.7.21) */
 

  } else {

    /* PBC is not on. "default" selection beginning of current
       selection . */
 
    itemid.num = p_vcdplayer->play_item.num;
 
  }

  /** ??? p_vcdplayer->update_title(); ***/
  vcdplayer_play( p_access, itemid );
  return VLC_SUCCESS;

}

/*!
  Play item assocated with the "next" selection.

  Return false if there was some problem.
*/
bool
vcdplayer_play_next( access_t * p_access )
{
  vcdplayer_t *p_vcdplayer= (vcdplayer_t *)p_access->p_sys;

  vcdinfo_obj_t     *p_vcdinfo;
  vcdinfo_itemid_t   itemid;

  if (!p_vcdplayer) return false;

  dbg_print( (INPUT_DBG_CALL|INPUT_DBG_PBC),
         "current: %d" , p_vcdplayer->play_item.num);

  p_vcdinfo = p_vcdplayer->vcd;

  itemid = p_vcdplayer->play_item;

  if  (vcdplayer_pbc_is_on(p_vcdplayer)) {

    vcdinfo_lid_get_pxd(p_vcdinfo, &(p_vcdplayer->pxd), p_vcdplayer->i_lid);
 
    switch (p_vcdplayer->pxd.descriptor_type) {
    case PSD_TYPE_SELECTION_LIST:
    case PSD_TYPE_EXT_SELECTION_LIST:
      if (p_vcdplayer->pxd.psd == NULL) return false;
      vcdplayer_update_entry(p_access,
                             vcdinf_psd_get_next_offset(p_vcdplayer->pxd.psd),
                             &itemid.num, "next");
      itemid.type = VCDINFO_ITEM_TYPE_LID;
      break;

    case PSD_TYPE_PLAY_LIST:
      if (p_vcdplayer->pxd.pld == NULL) return false;
      vcdplayer_update_entry(p_access,
                             vcdinf_pld_get_next_offset(p_vcdplayer->pxd.pld),
                             &itemid.num, "next");
      itemid.type = VCDINFO_ITEM_TYPE_LID;
      break;
 
    case PSD_TYPE_END_LIST:
    case PSD_TYPE_COMMAND_LIST:
      LOG_WARN( "There is no PBC 'next' selection here" );
      return false;
    }
  } else {

    /* PBC is not on. "Next" selection is play_item.num+1 if possible. */
 
    int max_entry = 0;

    switch (p_vcdplayer->play_item.type) {
    case VCDINFO_ITEM_TYPE_ENTRY:
    case VCDINFO_ITEM_TYPE_SEGMENT:
    case VCDINFO_ITEM_TYPE_TRACK:
 
      switch (p_vcdplayer->play_item.type) {
      case VCDINFO_ITEM_TYPE_ENTRY:
        max_entry = p_vcdplayer->i_entries;
        break;
      case VCDINFO_ITEM_TYPE_SEGMENT:
        max_entry = p_vcdplayer->i_segments;
        break;
      case VCDINFO_ITEM_TYPE_TRACK:
        max_entry = p_vcdplayer->i_tracks;
        break;
      default: ; /* Handle exceptional cases below */
      }
 
      if (p_vcdplayer->play_item.num+1 < max_entry) {
        itemid.num = p_vcdplayer->play_item.num+1;
      } else {
        LOG_WARN( "At the end - non-PBC 'next' not possible here" );
        return false;
      }
 
      break;
 
    case VCDINFO_ITEM_TYPE_LID:
      /* Should have handled above. */
      LOG_WARN( "Internal inconsistency - should not have gotten here." );
      return false;
    default:
      return false;
    }
  }

  /** ??? p_vcdplayer->update_title(); ***/
  vcdplayer_play( p_access, itemid );
  return VLC_SUCCESS;

}

/*!
  Play item assocated with the "prev" selection.

  Return false if there was some problem.
*/
bool
vcdplayer_play_prev( access_t * p_access )
{
  vcdplayer_t      *p_vcdplayer= (vcdplayer_t *)p_access->p_sys;
  vcdinfo_obj_t    *p_vcdinfo  = p_vcdplayer->vcd;
  vcdinfo_itemid_t  itemid;

  dbg_print( (INPUT_DBG_CALL|INPUT_DBG_PBC),
         "current: %d" , p_vcdplayer->play_item.num);

  itemid = p_vcdplayer->play_item;

  if (vcdplayer_pbc_is_on(p_vcdplayer)) {

    vcdinfo_lid_get_pxd(p_vcdinfo, &(p_vcdplayer->pxd), p_vcdplayer->i_lid);
 
    switch (p_vcdplayer->pxd.descriptor_type) {
    case PSD_TYPE_SELECTION_LIST:
    case PSD_TYPE_EXT_SELECTION_LIST:
      if (p_vcdplayer->pxd.psd == NULL) return false;
      vcdplayer_update_entry(p_access,
                             vcdinf_psd_get_prev_offset(p_vcdplayer->pxd.psd),
                             &itemid.num, "prev");
      itemid.type = VCDINFO_ITEM_TYPE_LID;
      break;

    case PSD_TYPE_PLAY_LIST:
      if (p_vcdplayer->pxd.pld == NULL) return false;
      vcdplayer_update_entry(p_access,
                             vcdinf_pld_get_prev_offset(p_vcdplayer->pxd.pld),
                             &itemid.num, "prev");
      itemid.type = VCDINFO_ITEM_TYPE_LID;
      break;
 
    case PSD_TYPE_END_LIST:
    case PSD_TYPE_COMMAND_LIST:
      LOG_WARN( "There is no PBC 'prev' selection here" );
      return false;
    }
  } else {

    /* PBC is not on. "Prev" selection is play_item.num-1 if possible. */
 
    int min_entry = (VCDINFO_ITEM_TYPE_ENTRY == p_vcdplayer->play_item.type)
                    ? 0 : 1;
 
    if (p_vcdplayer->play_item.num > min_entry) {
      itemid.num = p_vcdplayer->play_item.num-1;
    } else {
      LOG_WARN( "At the beginning - non-PBC 'prev' not possible here" );
      return false;
    }
 
  }

  /** ??? p_vcdplayer->update_title(); ***/
  vcdplayer_play( p_access, itemid );
  return VLC_SUCCESS;

}

/*!
  Play item assocated with the "return" selection.

  Return false if there was some problem.
*/
bool
vcdplayer_play_return( access_t * p_access )
{
  vcdplayer_t      *p_vcdplayer= (vcdplayer_t *)p_access->p_sys;
  vcdinfo_obj_t    *p_vcdinfo  = p_vcdplayer->vcd;
  vcdinfo_itemid_t  itemid;

  dbg_print( (INPUT_DBG_CALL|INPUT_DBG_PBC),
             "current: %d" , p_vcdplayer->play_item.num);

  itemid = p_vcdplayer->play_item;

  if (vcdplayer_pbc_is_on(p_vcdplayer)) {

    vcdinfo_lid_get_pxd(p_vcdinfo, &(p_vcdplayer->pxd), p_vcdplayer->i_lid);
 
    switch (p_vcdplayer->pxd.descriptor_type) {
    case PSD_TYPE_SELECTION_LIST:
    case PSD_TYPE_EXT_SELECTION_LIST:
      if (p_vcdplayer->pxd.psd == NULL) return false;
      vcdplayer_update_entry(p_access,
                             vcdinf_psd_get_return_offset(p_vcdplayer->pxd.psd),
                             &itemid.num, "return");
      itemid.type = VCDINFO_ITEM_TYPE_LID;
      break;

    case PSD_TYPE_PLAY_LIST:
      if (p_vcdplayer->pxd.pld == NULL) return false;
      vcdplayer_update_entry(p_access,
                             vcdinf_pld_get_return_offset(p_vcdplayer->pxd.pld),
                             &itemid.num, "return");
      itemid.type = VCDINFO_ITEM_TYPE_LID;
      break;
 
    case PSD_TYPE_END_LIST:
    case PSD_TYPE_COMMAND_LIST:
      LOG_WARN( "There is no PBC 'return' selection here" );
      return false;
    }
  } else {

    /* PBC is not on. "Return" selection is min_entry if possible. */
 
    p_vcdplayer->play_item.num =
      (VCDINFO_ITEM_TYPE_ENTRY == p_vcdplayer->play_item.type)
      ? 0 : 1;
 
  }

  /** ??? p_vcdplayer->update_title(); ***/
  vcdplayer_play( p_access, itemid );
  return VLC_SUCCESS;

}
