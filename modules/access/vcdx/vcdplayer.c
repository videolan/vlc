/*****************************************************************************
 * vcdplayer.c : VCD input module for vlc
 *               using libcdio, libvcd and libvcdinfo
 *****************************************************************************
 * Copyright (C) 2003 Rocky Bernstein <rocky@panix.com>
 * $Id$
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

/*
   This contains more of the vlc-independent parts that might be used
   in any VCD input module for a media player. However at present there
   are vlc-specific structures. See also vcdplayer.c of the xine plugin.
 */
/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/intf.h>

#include "vcd.h"
#include "vcdplayer.h"
#include "intf.h"

#include <string.h>

#include <cdio/cdio.h>
#include <cdio/util.h>
#include <libvcd/info.h>

#ifdef WIN32
#define sleep(A) Sleep((A)*1000)
#endif

/*!
  Return true if playback control (PBC) is on
*/
bool
vcdplayer_pbc_is_on(const thread_vcd_data_t *p_vcd) 
{
  return VCDINFO_INVALID_ENTRY != p_vcd->cur_lid; 
}

static void
vcdplayer_update_entry( input_thread_t * p_input, uint16_t ofs, 
                        uint16_t *entry, const char *label)
{
  thread_vcd_data_t *     p_vcd= (thread_vcd_data_t *)p_input->p_access_data;

  if ( ofs == VCDINFO_INVALID_OFFSET ) {
    *entry = VCDINFO_INVALID_ENTRY;
  } else {
    vcdinfo_offset_t *off = vcdinfo_get_offset_t(p_vcd->vcd, ofs);
    if (off != NULL) {
      *entry = off->lid;
      dbg_print(INPUT_DBG_PBC, "%s: LID %d\n", label, off->lid);
    } else
      *entry = VCDINFO_INVALID_ENTRY;
  }
}

/* Handles navigation when NOT in PBC reaching the end of a play item. 

   The navigations rules here may be sort of made up, but the intent 
   is to do something that's probably right or helpful.

   return true if the caller should return.
*/
vcdplayer_read_status_t
vcdplayer_non_pbc_nav ( input_thread_t * p_input )
{
  thread_vcd_data_t *     p_vcd= (thread_vcd_data_t *)p_input->p_access_data;

  /* Not in playback control. Do we advance automatically or stop? */
  switch (p_vcd->play_item.type) {
  case VCDINFO_ITEM_TYPE_TRACK:
  case VCDINFO_ITEM_TYPE_ENTRY: {

    dbg_print( INPUT_DBG_LSN, "new track %d, lsn %d", p_vcd->cur_track, 
               p_vcd->p_sectors[p_vcd->cur_track+1] );
    return READ_END;
    break;
  }
  case VCDINFO_ITEM_TYPE_SPAREID2:  
    dbg_print( (INPUT_DBG_STILL|INPUT_DBG_LSN), 
               "SPAREID2" );
    /* FIXME */
    p_input->stream.b_seekable = 0;
    if (p_vcd->in_still)
    {
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
    p_input->stream.b_seekable = 0;
    if (p_vcd->in_still) 
    {
      dbg_print( (INPUT_DBG_STILL|INPUT_DBG_LSN), 
                 "End of Segment - looping" );
      return READ_STILL_FRAME;
    }
    return READ_END;
  }
  return READ_BLOCK;
}

/* Handles PBC navigation when reaching the end of a play item. */
vcdplayer_read_status_t
vcdplayer_pbc_nav ( input_thread_t * p_input )
{
  thread_vcd_data_t *     p_vcd= (thread_vcd_data_t *)p_input->p_access_data;

  /* We are in playback control. */
  vcdinfo_itemid_t itemid;

  /* The end of an entry is really the end of the associated 
     sequence (or track). */
  
  if ( (VCDINFO_ITEM_TYPE_ENTRY == p_vcd->play_item.type) && 
       (p_vcd->cur_lsn < p_vcd->end_lsn) ) {
    /* Set up to just continue to the next entry */
    p_vcd->play_item.num++;
    dbg_print( (INPUT_DBG_LSN|INPUT_DBG_PBC), 
               "continuing into next entry: %u", p_vcd->play_item.num);
    VCDPlay( p_input, p_vcd->play_item );
    /* p_vcd->update_title(); */
    return READ_BLOCK;
  }
  
  switch (p_vcd->pxd.descriptor_type) {
  case PSD_TYPE_END_LIST:
    return READ_END;
    break;
  case PSD_TYPE_PLAY_LIST: {
    int wait_time = vcdinf_get_wait_time(p_vcd->pxd.pld);
    
    dbg_print(INPUT_DBG_PBC, "playlist wait_time: %d", wait_time);
    
    if (vcdplayer_inc_play_item(p_input))
      return READ_BLOCK;

    /* Handle any wait time given. */
    if (p_vcd->in_still) {
      vcdIntfStillTime( p_vcd->p_intf, wait_time );
      return READ_STILL_FRAME;
    }

    vcdplayer_update_entry( p_input, 
                            vcdinf_pld_get_next_offset(p_vcd->pxd.pld),
                            &itemid.num, "next" );
    itemid.type = VCDINFO_ITEM_TYPE_LID;
    VCDPlay( p_input, itemid );
    break;
  }
  case PSD_TYPE_SELECTION_LIST:     /* Selection List (+Ext. for SVCD) */
  case PSD_TYPE_EXT_SELECTION_LIST: /* Extended Selection List (VCD2.0) */
    {
      int wait_time         = vcdinf_get_timeout_time(p_vcd->pxd.psd);
      uint16_t timeout_offs = vcdinf_get_timeout_offset(p_vcd->pxd.psd);
      uint16_t max_loop     = vcdinf_get_loop_count(p_vcd->pxd.psd);
      vcdinfo_offset_t *offset_timeout_LID = 
        vcdinfo_get_offset_t(p_vcd->vcd, timeout_offs);
      
      dbg_print(INPUT_DBG_PBC, "wait_time: %d, looped: %d, max_loop %d", 
                wait_time, p_vcd->loop_count, max_loop);
      
      /* Handle any wait time given */
      if (p_vcd->in_still) {
	vcdIntfStillTime( p_vcd->p_intf, wait_time );
	return READ_STILL_FRAME;
      } 
      
      /* Handle any looping given. */
      if ( max_loop == 0 || p_vcd->loop_count < max_loop ) {
        p_vcd->loop_count++;
        if (p_vcd->loop_count == 0x7f) p_vcd->loop_count = 0;
        VCDSeek( p_input, 0 );
        /* if (p_vcd->in_still) p_vcd->force_redisplay();*/
        return READ_BLOCK;
      }
      
      /* Looping finished and wait finished. Move to timeout
         entry or next entry, or handle still. */
      
      if (NULL != offset_timeout_LID) {
        /* Handle timeout_LID */
        itemid.num  = offset_timeout_LID->lid;
        itemid.type = VCDINFO_ITEM_TYPE_LID;
        dbg_print(INPUT_DBG_PBC, "timeout to: %d", itemid.num);
        VCDPlay( p_input, itemid );
        return READ_BLOCK;
      } else {
        int num_selections = vcdinf_get_num_selections(p_vcd->pxd.psd);
        if (num_selections > 0) {
          /* Pick a random selection. */
          unsigned int bsn=vcdinf_get_bsn(p_vcd->pxd.psd);
          int rand_selection=bsn +
            (int) ((num_selections+0.0)*rand()/(RAND_MAX+1.0));
          lid_t rand_lid=vcdinfo_selection_get_lid (p_vcd->vcd, 
						    p_vcd->cur_lid, 
						    rand_selection);
          itemid.num = rand_lid;
          itemid.type = VCDINFO_ITEM_TYPE_LID;
          dbg_print(INPUT_DBG_PBC, "random selection %d, lid: %d", 
                    rand_selection - bsn, rand_lid);
          VCDPlay( p_input, itemid );
          return READ_BLOCK;
        } else if (p_vcd->in_still) {
          /* Hack: Just go back and do still again */
          sleep(1);
          return READ_STILL_FRAME;
        }
      }
      break;
    }
  case VCDINFO_ITEM_TYPE_NOTFOUND:  
    LOG_ERR( "NOTFOUND in PBC -- not supposed to happen" );
    break;
  case VCDINFO_ITEM_TYPE_SPAREID2:  
    LOG_ERR( "SPAREID2 in PBC -- not supposed to happen" );
    break;
  case VCDINFO_ITEM_TYPE_LID:  
    LOG_ERR( "LID in PBC -- not supposed to happen" );
    break;
    
  default:
    ;
  }
  /* FIXME: Should handle autowait ...  */

  return READ_ERROR;
}

/*!
  Get the next play-item in the list given in the LIDs. Note play-item
  here refers to list of play-items for a single LID It shouldn't be
  confused with a user's list of favorite things to play or the 
  "next" field of a LID which moves us to a different LID.
 */
bool
vcdplayer_inc_play_item( input_thread_t *p_input )
{
  thread_vcd_data_t *     p_vcd= (thread_vcd_data_t *)p_input->p_access_data;

  int noi;

  dbg_print(INPUT_DBG_CALL, "called pli: %d", p_vcd->pdi);

  if ( NULL == p_vcd || NULL == p_vcd->pxd.pld  ) return false;

  noi = vcdinf_pld_get_noi(p_vcd->pxd.pld);
  
  if ( noi <= 0 ) return false;
  
  /* Handle delays like autowait or wait here? */

  p_vcd->pdi++;

  if ( p_vcd->pdi < 0 || p_vcd->pdi >= noi ) return false;

  else {
    uint16_t trans_itemid_num=vcdinf_pld_get_play_item(p_vcd->pxd.pld, 
                                                       p_vcd->pdi);
    vcdinfo_itemid_t trans_itemid;

    if (VCDINFO_INVALID_ITEMID == trans_itemid_num) return false;
    
    vcdinfo_classify_itemid(trans_itemid_num, &trans_itemid);
    dbg_print(INPUT_DBG_PBC, "  play-item[%d]: %s",
              p_vcd->pdi, vcdinfo_pin2str (trans_itemid_num));
    return VLC_SUCCESS == VCDPlay( p_input, trans_itemid );
  }
}

/*!
  Play item assocated with the "default" selection.

  Return false if there was some problem.
*/
bool
vcdplayer_play_default( input_thread_t * p_input )
{
  thread_vcd_data_t *p_vcd= (thread_vcd_data_t *)p_input->p_access_data;

  vcdinfo_itemid_t   itemid;

  if (!p_vcd) {
    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_PBC), 
	       "null p_vcd" );
    return VLC_EGENERIC;
  }
  

  dbg_print( (INPUT_DBG_CALL|INPUT_DBG_PBC), 
	     "current: %d" , p_vcd->play_item.num);

  itemid.type = p_vcd->play_item.type;

  if  (vcdplayer_pbc_is_on(p_vcd)) {

#if defined(LIBVCD_VERSION)
    lid_t lid=vcdinfo_get_multi_default_lid(p_vcd->vcd, p_vcd->cur_lid,
					    p_vcd->cur_lsn);

    if (VCDINFO_INVALID_LID != lid) {
      itemid.num  = lid;
      itemid.type = VCDINFO_ITEM_TYPE_LID;
      dbg_print(INPUT_DBG_PBC, "DEFAULT to %d\n", itemid.num);
    } else {
      dbg_print(INPUT_DBG_PBC, "no DEFAULT for LID %d\n", p_vcd->cur_lid);
    }

#else 
    vcdinfo_lid_get_pxd(p_vcd->vcd, &(p_vcd->pxd), p_vcd->cur_lid);
    
    switch (p_vcd->pxd.descriptor_type) {
    case PSD_TYPE_SELECTION_LIST:
    case PSD_TYPE_EXT_SELECTION_LIST:
      if (p_vcd->pxd.psd == NULL) return false;
      vcdplayer_update_entry( p_input, 
			      vcdinfo_get_default_offset(p_vcd->vcd, 
							 p_vcd->cur_lid), 
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
  
    p_vcd->play_item.num = p_vcd->play_item.num;
    
  }

  /** ??? p_vcd->update_title(); ***/
  return VLC_SUCCESS == VCDPlay( p_input, itemid );

}

/*!
  Play item assocated with the "next" selection.

  Return false if there was some problem.
*/
bool
vcdplayer_play_next( input_thread_t * p_input )
{
  thread_vcd_data_t *p_vcd= (thread_vcd_data_t *)p_input->p_access_data;

  vcdinfo_obj_t     *obj;
  vcdinfo_itemid_t   itemid;

  if (!p_vcd) return false;

  dbg_print( (INPUT_DBG_CALL|INPUT_DBG_PBC), 
	     "current: %d" , p_vcd->play_item.num);

  obj = p_vcd->vcd;

  itemid.type = p_vcd->play_item.type;

  if  (vcdplayer_pbc_is_on(p_vcd)) {

    vcdinfo_lid_get_pxd(obj, &(p_vcd->pxd), p_vcd->cur_lid);
    
    switch (p_vcd->pxd.descriptor_type) {
    case PSD_TYPE_SELECTION_LIST:
    case PSD_TYPE_EXT_SELECTION_LIST:
      if (p_vcd->pxd.psd == NULL) return false;
      vcdplayer_update_entry( p_input, 
			      vcdinf_psd_get_next_offset(p_vcd->pxd.psd), 
			      &itemid.num, "next");
      itemid.type = VCDINFO_ITEM_TYPE_LID;
      break;

    case PSD_TYPE_PLAY_LIST: 
      if (p_vcd->pxd.pld == NULL) return false;
      vcdplayer_update_entry( p_input, 
			      vcdinf_pld_get_next_offset(p_vcd->pxd.pld), 
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

    switch (p_vcd->play_item.type) {
    case VCDINFO_ITEM_TYPE_ENTRY: 
    case VCDINFO_ITEM_TYPE_SEGMENT: 
    case VCDINFO_ITEM_TYPE_TRACK: 
      
      switch (p_vcd->play_item.type) {
      case VCDINFO_ITEM_TYPE_ENTRY: 
	max_entry = p_vcd->num_entries;
	break;
      case VCDINFO_ITEM_TYPE_SEGMENT: 
	max_entry = p_vcd->num_segments;
	break;
      case VCDINFO_ITEM_TYPE_TRACK: 
	max_entry = p_vcd->num_tracks;
	break;
      default: ; /* Handle exceptional cases below */
      }
      
      if (p_vcd->play_item.num+1 < max_entry) {
	itemid.num = p_vcd->play_item.num+1;
      } else {
	LOG_WARN( "At the end - non-PBC 'next' not possible here" );
	return false;
      }
      
      break;
      
    case VCDINFO_ITEM_TYPE_LID: 
      {
	/* Should have handled above. */
	LOG_WARN( "Internal inconsistency - should not have gotten here." );
	return false;
      }
    default: 
      return false;
    }
  }

  /** ??? p_vcd->update_title(); ***/
  return VLC_SUCCESS == VCDPlay( p_input, itemid );

}

/*!
  Play item assocated with the "prev" selection.

  Return false if there was some problem.
*/
bool
vcdplayer_play_prev( input_thread_t * p_input )
{
  thread_vcd_data_t *p_vcd= (thread_vcd_data_t *)p_input->p_access_data;

  vcdinfo_obj_t     *obj  = p_vcd->vcd;
  vcdinfo_itemid_t   itemid;

  dbg_print( (INPUT_DBG_CALL|INPUT_DBG_PBC), 
	     "current: %d" , p_vcd->play_item.num);

  itemid.type = p_vcd->play_item.type;

  if  (vcdplayer_pbc_is_on(p_vcd)) {

    vcdinfo_lid_get_pxd(obj, &(p_vcd->pxd), p_vcd->cur_lid);
    
    switch (p_vcd->pxd.descriptor_type) {
    case PSD_TYPE_SELECTION_LIST:
    case PSD_TYPE_EXT_SELECTION_LIST:
      if (p_vcd->pxd.psd == NULL) return false;
      vcdplayer_update_entry( p_input, 
			      vcdinf_psd_get_prev_offset(p_vcd->pxd.psd), 
			      &itemid.num, "prev");
      itemid.type = VCDINFO_ITEM_TYPE_LID;
      break;

    case PSD_TYPE_PLAY_LIST: 
      if (p_vcd->pxd.pld == NULL) return false;
      vcdplayer_update_entry( p_input, 
			      vcdinf_pld_get_prev_offset(p_vcd->pxd.pld), 
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
  
    int min_entry = (VCDINFO_ITEM_TYPE_ENTRY == p_vcd->play_item.type) 
      ? 0 : 1;
    
    if (p_vcd->play_item.num > min_entry) {
      itemid.num = p_vcd->play_item.num-1;
    } else {
      LOG_WARN( "At the beginning - non-PBC 'prev' not possible here" );
      return false;
    }
      
  }

  /** ??? p_vcd->update_title(); ***/
  return VLC_SUCCESS == VCDPlay( p_input, itemid );

}

/*!
  Play item assocated with the "return" selection.

  Return false if there was some problem.
*/
bool
vcdplayer_play_return( input_thread_t * p_input )
{
  thread_vcd_data_t *p_vcd= (thread_vcd_data_t *)p_input->p_access_data;

  vcdinfo_obj_t     *obj  = p_vcd->vcd;
  vcdinfo_itemid_t   itemid;

  dbg_print( (INPUT_DBG_CALL|INPUT_DBG_PBC), 
	     "current: %d" , p_vcd->play_item.num);

  itemid.type = p_vcd->play_item.type;

  if  (vcdplayer_pbc_is_on(p_vcd)) {

    vcdinfo_lid_get_pxd(obj, &(p_vcd->pxd), p_vcd->cur_lid);
    
    switch (p_vcd->pxd.descriptor_type) {
    case PSD_TYPE_SELECTION_LIST:
    case PSD_TYPE_EXT_SELECTION_LIST:
      if (p_vcd->pxd.psd == NULL) return false;
      vcdplayer_update_entry( p_input, 
			      vcdinf_psd_get_return_offset(p_vcd->pxd.psd), 
			      &itemid.num, "return");
      itemid.type = VCDINFO_ITEM_TYPE_LID;
      break;

    case PSD_TYPE_PLAY_LIST: 
      if (p_vcd->pxd.pld == NULL) return false;
      vcdplayer_update_entry( p_input, 
			      vcdinf_pld_get_return_offset(p_vcd->pxd.pld), 
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
  
    p_vcd->play_item.num = (VCDINFO_ITEM_TYPE_ENTRY == p_vcd->play_item.type) 
      ? 0 : 1;
    
  }

  /** ??? p_vcd->update_title(); ***/
  return VLC_SUCCESS == VCDPlay( p_input, itemid );

}
