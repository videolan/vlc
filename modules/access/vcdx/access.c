/*****************************************************************************
 * vcd.c : VCD input module for vlc
 *         using libcdio, libvcd and libvcdinfo. vlc-specific things tend
 *         to go here.
 *****************************************************************************
 * Copyright (C) 2000,2003 VideoLAN
 * $Id: access.c,v 1.6 2003/12/02 04:22:10 rocky Exp $
 *
 * Authors: Rocky Bernstein <rocky@panix.com> 
 *          Johan Bilien <jobi@via.ecp.fr>
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

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc_interface.h>

#include "../../demux/mpeg/system.h"
#include "vcd.h"
#include "intf.h"
#include "vcdplayer.h"

#include <cdio/cdio.h>
#include <cdio/cd_types.h>
#include <cdio/logging.h>
#include <cdio/util.h>
#include <libvcd/info.h>
#include <libvcd/logging.h>

#include "cdrom.h"

/* how many blocks VCDRead will read in each loop */
#define VCD_BLOCKS_ONCE 20
#define VCD_DATA_ONCE   (VCD_BLOCKS_ONCE * M2F2_SECTOR_SIZE)

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/* First those which are accessed from outside (via pointers). */
static int  VCDRead         ( input_thread_t *, byte_t *, size_t );
static int  VCDSetProgram   ( input_thread_t *, pgrm_descriptor_t * );

/* Now those which are strictly internal */
static void VCDSetOrigin    ( input_thread_t *, lsn_t origin_lsn, 
                              lsn_t cur_lsn, lsn_t end_lsn, 
                              int cur_entry, track_t track );
static int  VCDEntryPoints  ( input_thread_t * );
static int  VCDLIDs         ( input_thread_t * );
static int  VCDSegments     ( input_thread_t * );
static void VCDTracks       ( input_thread_t * );
static int  VCDReadSector   ( vlc_object_t *p_this, 
                              const vcdinfo_obj_t *p_vcd, lsn_t cur_lsn, 
                              byte_t * p_buffer );
static char *VCDParse       ( input_thread_t *, 
                              /*out*/ vcdinfo_itemid_t * p_itemid );

static void VCDUpdateVar( input_thread_t *p_input, int i_entry, int i_action,
                          const char *varname, const char *label );

static vcdinfo_obj_t *vcd_Open   ( vlc_object_t *p_this, const char *psz_dev );

/****************************************************************************
 * Private functions
 ****************************************************************************/

/* FIXME: This variable is a hack. Would be nice to eliminate the 
   global-ness. */

static input_thread_t *p_vcd_input = NULL;

/* process messages that originate from libcdio. */
static void
cdio_log_handler (cdio_log_level_t level, const char message[])
{
  thread_vcd_data_t *p_vcd = (thread_vcd_data_t *)p_vcd_input->p_access_data;
  switch (level) {
  case CDIO_LOG_DEBUG:
  case CDIO_LOG_INFO:
    if (p_vcd->i_debug & INPUT_DBG_CDIO) 
      msg_Dbg( p_vcd_input, message);
    break;
  case CDIO_LOG_WARN:
    msg_Warn( p_vcd_input, message);
    break;
  case CDIO_LOG_ERROR:
  case CDIO_LOG_ASSERT:
    msg_Err( p_vcd_input, message);
    break;
  default:
    msg_Warn( p_vcd_input, message,
            _("The above message had unknown vcdimager log level"), 
            level);
  }
  return;
}

/* process messages that originate from vcdinfo. */
static void
vcd_log_handler (vcd_log_level_t level, const char message[])
{
  thread_vcd_data_t *p_vcd = (thread_vcd_data_t *)p_vcd_input->p_access_data;
  switch (level) {
  case VCD_LOG_DEBUG:
  case VCD_LOG_INFO:
    if (p_vcd->i_debug & INPUT_DBG_VCDINFO)
      msg_Dbg( p_vcd_input, message);
    break;
  case VCD_LOG_WARN:
    msg_Warn( p_vcd_input, message);
    break;
  case VCD_LOG_ERROR:
  case VCD_LOG_ASSERT:
    msg_Err( p_vcd_input, message);
    break;
  default:
    msg_Warn( p_vcd_input, "%s\n%s %d", message,
            _("The above message had unknown vcdimager log level"), 
            level);
  }
  return;
}

/*****************************************************************************
 * VCDRead: reads i_len bytes from the VCD into p_buffer.
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, otherwise the number of
 * bytes.
 *****************************************************************************/
static int 
VCDRead( input_thread_t * p_input, byte_t * p_buffer, size_t i_len )
{
    thread_vcd_data_t *     p_vcd= (thread_vcd_data_t *)p_input->p_access_data;
    int                     i_blocks;
    int                     i_index;
    int                     i_read;
    byte_t                  p_last_sector[ M2F2_SECTOR_SIZE ];

    i_read = 0;

    dbg_print( (INPUT_DBG_CALL), "lsn: %u", p_vcd->cur_lsn );

    /* Compute the number of blocks we have to read */

    i_blocks = i_len / M2F2_SECTOR_SIZE;

    for ( i_index = 0 ; i_index < i_blocks ; i_index++ )
    {

      if ( p_vcd->cur_lsn == p_vcd->end_lsn ) {
        vcdplayer_read_status_t read_status;

        /* We've run off of the end of this entry. Do we continue or stop? */
        dbg_print( (INPUT_DBG_LSN|INPUT_DBG_PBC), 
                   "end reached, cur: %u", p_vcd->cur_lsn );

        read_status = vcdplayer_pbc_is_on( p_vcd ) 
          ? vcdplayer_pbc_nav( p_input ) 
          : vcdplayer_non_pbc_nav( p_input );

        switch (read_status) {
        case READ_END:
          /* End reached. Return NULL to indicated this. */
        case READ_ERROR:
          /* Some sort of error. */
          return i_read;

        case READ_STILL_FRAME: 
          {
	    /* Reached the end of a still frame. */

            byte_t * p_buf = p_buffer;
	    pgrm_descriptor_t * p_pgrm = p_input->stream.p_selected_program;;

            p_buf += (i_index*M2F2_SECTOR_SIZE);
            memset(p_buf, 0, M2F2_SECTOR_SIZE);
            p_buf += 2;
            *p_buf = 0x01;
            dbg_print(INPUT_DBG_STILL, "Handled still event\n");

	    /* p_vcd->p_intf->b_end_of_cell = true; */
	    input_SetStatus( p_input, INPUT_STATUS_PAUSE );

	    vlc_mutex_lock( &p_input->stream.stream_lock );

	    p_pgrm = p_input->stream.p_selected_program;
	    p_pgrm->i_synchro_state = SYNCHRO_REINIT;

	    vlc_mutex_unlock( &p_input->stream.stream_lock );

	    input_ClockManageControl( p_input, p_pgrm, 0 );

            return i_read + M2F2_SECTOR_SIZE;
          }
        default:
        case READ_BLOCK:
          break;
        }
      }

      if ( VCDReadSector( VLC_OBJECT(p_input), p_vcd->vcd,
                          p_vcd->cur_lsn, 
                          p_buffer + (i_index*M2F2_SECTOR_SIZE) ) < 0 )
        {
          LOG_ERR ("could not read sector %d", p_vcd->cur_lsn );
          return -1;
        }
      
      p_vcd->cur_lsn ++;
      
      /* Update chapter */
      if( p_vcd->b_valid_ep &&
          /* FIXME kludge so that read does not update chapter
           * when a manual chapter change was requested and not
           * yet accomplished */
          !p_input->stream.p_new_area )
        {
          unsigned int i_entry = p_input->stream.p_selected_area->i_part;
          
          vlc_mutex_lock( &p_input->stream.stream_lock );
          
          if( i_entry < p_vcd->num_entries &&
              p_vcd->cur_lsn >= p_vcd->p_entries[i_entry+1] )
            {
              dbg_print( INPUT_DBG_PBC, 
                         "new entry, i_entry %d, sector %d, es %d",
                         i_entry, p_vcd->cur_lsn, 
                         p_vcd->p_entries[i_entry] );
              p_vcd->play_item.num = 
                ++ p_input->stream.p_selected_area->i_part;
              p_vcd->play_item.type = VCDINFO_ITEM_TYPE_ENTRY;
              VCDUpdateVar( p_input, p_vcd->play_item.num, VLC_VAR_SETVALUE,
                            "chapter", "Setting entry" );
            }
          vlc_mutex_unlock( &p_input->stream.stream_lock );
        }

        i_read += M2F2_SECTOR_SIZE;
    }

    if ( i_len % M2F2_SECTOR_SIZE ) /* this should not happen */
    {
        if ( VCDReadSector( VLC_OBJECT(p_input), p_vcd->vcd,
                            p_vcd->cur_lsn, p_last_sector ) < 0 )
        {
            LOG_ERR ("could not read sector %d", p_vcd->cur_lsn );
            return -1;
        }

        p_input->p_vlc->pf_memcpy( p_buffer + i_blocks * M2F2_SECTOR_SIZE,
                                   p_last_sector, i_len % M2F2_SECTOR_SIZE );
        i_read += i_len % M2F2_SECTOR_SIZE;
    }

    return i_read;
}


/*****************************************************************************
 * VCDSetProgram: Does nothing since a VCD is mono_program
 *****************************************************************************/
static int 
VCDSetProgram( input_thread_t * p_input, pgrm_descriptor_t * p_program)
{
    thread_vcd_data_t *     p_vcd= (thread_vcd_data_t *)p_input->p_access_data;
    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_EXT), "VCDSetProgram" );
    return 0;
}


/*****************************************************************************
 * VCDSetArea: initialize internal data structures and input stream data 
   so set subsequent reading and seeking to reflect that we are
   at track x, entry or segment y.
   This is called for each user navigation request, e.g. the GUI 
   Chapter/Title selections or in initial MRL parsing. 
 ****************************************************************************/
int 
VCDSetArea( input_thread_t * p_input, input_area_t * p_area )
{
    thread_vcd_data_t *p_vcd = (thread_vcd_data_t*)p_input->p_access_data;
    unsigned int i_entry = p_area->i_part;
    track_t i_track      = p_area->i_id;
    int old_seekable     = p_input->stream.b_seekable;
    unsigned int i_nb    = p_area->i_plugin_data + p_area->i_part_nb;

    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_EXT),
               "track: %d, entry %d, seekable %d, area %lx, select area %lx ",
               i_track, i_entry, old_seekable, 
	       (long unsigned int) p_area, 
	       (long unsigned int) p_input->stream.p_selected_area );

    /* we can't use the interface slider until initilization is complete */
    p_input->stream.b_seekable = 0;

    if( p_area != p_input->stream.p_selected_area )
    {
        unsigned int i;

        /* If is the result of a track change, make the entry valid. */
        if (i_entry < p_area->i_plugin_data || i_entry >= i_nb) 
          i_entry = p_area->i_plugin_data;

        /* Change the default area */
        p_input->stream.p_selected_area = p_area;

        /* Update the navigation variables without triggering a callback */
        VCDUpdateVar( p_input, i_track, VLC_VAR_SETVALUE, "title", 
                      "Setting track");

        var_Change( p_input, "chapter", VLC_VAR_CLEARCHOICES, NULL, NULL );
        for( i = p_area->i_plugin_data; i < i_nb; i++ )
        {
          VCDUpdateVar( p_input, i , VLC_VAR_ADDCHOICE, 
                        "chapter",  "Adding entry choice");
        }
    }

    if (i_track == 0) 
      VCDSetOrigin( p_input, p_vcd->p_segments[i_entry], 
                    p_vcd->p_segments[i_entry], p_vcd->p_segments[i_entry+1],
                    i_entry, 0 );
    else
      VCDSetOrigin( p_input, p_vcd->p_sectors[i_track], 
                    vcdinfo_get_entry_lsn(p_vcd->vcd, i_entry), 
                    p_vcd->p_sectors[i_track+1],
                    i_entry, i_track );

    p_input->stream.b_seekable = old_seekable;
    /* warn interface that something has changed */
    p_input->stream.b_changed = 1;

    return VLC_SUCCESS;
}


/****************************************************************************
 * VCDSeek
 ****************************************************************************/
void 
VCDSeek( input_thread_t * p_input, off_t i_off )
{
    thread_vcd_data_t * p_vcd;
    unsigned int i_entry=0; /* invalid entry */

    p_vcd = (thread_vcd_data_t *) p_input->p_access_data;

    p_vcd->cur_lsn = p_vcd->origin_lsn + (i_off / (off_t)M2F2_SECTOR_SIZE);

    vlc_mutex_lock( &p_input->stream.stream_lock );
#define p_area p_input->stream.p_selected_area
    /* Find entry */
    if( p_vcd->b_valid_ep )
    {
        for( i_entry = 1 ; i_entry < p_vcd->num_entries ; i_entry ++ )
        {
            if( p_vcd->cur_lsn < p_vcd->p_entries[i_entry] )
            {
              VCDUpdateVar( p_input, i_entry, VLC_VAR_SETVALUE, 
                            "chapter", "Setting entry" );
              break;
            }
        }
        p_vcd->play_item.num  = p_area->i_part = i_entry;
        p_vcd->play_item.type = VCDINFO_ITEM_TYPE_ENTRY;
    }
#undef p_area

    p_input->stream.p_selected_area->i_tell = i_off;

    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_EXT|INPUT_DBG_SEEK),
    "orig %d, cur: %d, offset: %lld, start: %lld, entry %d", 
               p_vcd->origin_lsn, p_vcd->cur_lsn, i_off, 
               p_input->stream.p_selected_area->i_start, i_entry );

    vlc_mutex_unlock( &p_input->stream.stream_lock );
}

/*****************************************************************************
  VCDPlay: set up internal structures so seeking/reading places an item.
  itemid: the thing to play.
  user_entry: true if itemid is a user selection (rather than internally-
  generated selection such as via PBC) in which case we may have to adjust 
  for differences in numbering. 
 *****************************************************************************/
int
VCDPlay( input_thread_t *p_input, vcdinfo_itemid_t itemid )
{
    thread_vcd_data_t *     p_vcd= (thread_vcd_data_t *)p_input->p_access_data;
    input_area_t *          p_area;
    
    p_vcd->in_still = 0;

    dbg_print(INPUT_DBG_CALL, "itemid.num: %d, itemid.type: %d\n", 
	      itemid.num, itemid.type);

#define area p_input->stream.pp_areas

    switch (itemid.type) {
    case VCDINFO_ITEM_TYPE_TRACK:

      /* Valid tracks go from 1...num_tracks-1, because track 0 is unplayable.
       */

      if (itemid.num == 0 || itemid.num >= p_vcd->num_tracks) {
        LOG_ERR ("Invalid track number %d", itemid.num );
        return VLC_EGENERIC;
      }
      p_area           = area[itemid.num];
      p_area->i_part   = p_area->i_plugin_data;
      p_input->stream.b_seekable = 1;
      break;
    case VCDINFO_ITEM_TYPE_SEGMENT: 
      /* Valid segments go from 0...num_segments-1. */
      if (itemid.num >= p_vcd->num_segments) {
        LOG_ERR ( "Invalid segment number: %d", itemid.num );
        return VLC_EGENERIC;
      } else {
        vcdinfo_video_segment_type_t segtype = 
          vcdinfo_get_video_type(p_vcd->vcd, itemid.num);
        
        dbg_print(INPUT_DBG_PBC, "%s (%d), seg_num: %d", 
                  vcdinfo_video_type2str(p_vcd->vcd, itemid.num), 
                  (int) segtype, itemid.num);
        
        p_area           = area[0];
        p_area->i_part   = itemid.num;
        
        switch (segtype)
          {
          case VCDINFO_FILES_VIDEO_NTSC_STILL:
          case VCDINFO_FILES_VIDEO_NTSC_STILL2:
          case VCDINFO_FILES_VIDEO_PAL_STILL:
          case VCDINFO_FILES_VIDEO_PAL_STILL2:
            p_input->stream.b_seekable = 0;
            p_vcd->in_still = -5;
            break;
          default:
            p_input->stream.b_seekable = 1;
            p_vcd->in_still = 0;
          }
      }
      break;
      
    case VCDINFO_ITEM_TYPE_LID:
      /* LIDs go from 1..num_lids. */
      if (itemid.num == 0 || itemid.num > p_vcd->num_lids) {
        LOG_ERR ( "Invalid LID number: %d", itemid.num );
        return VLC_EGENERIC;
      } else {
        p_vcd->cur_lid = itemid.num;
        vcdinfo_lid_get_pxd(p_vcd->vcd, &(p_vcd->pxd), itemid.num);
    
        switch (p_vcd->pxd.descriptor_type) {
      
        case PSD_TYPE_SELECTION_LIST:
        case PSD_TYPE_EXT_SELECTION_LIST: {
          vcdinfo_itemid_t trans_itemid;
          uint16_t trans_itemid_num;
          
          if (p_vcd->pxd.psd == NULL) return VLC_EGENERIC;
          trans_itemid_num  = vcdinf_psd_get_itemid(p_vcd->pxd.psd);
          vcdinfo_classify_itemid(trans_itemid_num, &trans_itemid);
          p_vcd->loop_count = 1;
          p_vcd->loop_item  = trans_itemid;
          return VCDPlay( p_input, trans_itemid );
          break;
        }
          
        case PSD_TYPE_PLAY_LIST: {
          if (p_vcd->pxd.pld == NULL) return VLC_EGENERIC;
          p_vcd->pdi = -1;
          return vcdplayer_inc_play_item(p_input) 
            ? VLC_SUCCESS : VLC_EGENERIC;
          break;
        }
          
        case PSD_TYPE_END_LIST:
        case PSD_TYPE_COMMAND_LIST:
          
        default:
          ;
        }
      }
      return VLC_EGENERIC;
    case VCDINFO_ITEM_TYPE_ENTRY:
      /* Entries go from 0..num_entries-1. */
      if (itemid.num >= p_vcd->num_entries) {
        LOG_ERR ("Invalid entry number: %d", itemid.num );
        return VLC_EGENERIC;
      } else {
        track_t cur_track  = vcdinfo_get_track(p_vcd->vcd,  itemid.num);
        p_area             = area[cur_track];
        p_area->i_part     = itemid.num;
        p_input->stream.b_seekable = 1;
      }
      break;
    default:
      LOG_ERR ("unknown entry type" );
      return VLC_EGENERIC;
    }

    VCDSetArea( p_input, p_area );

#undef area

    p_vcd->play_item = itemid;

    dbg_print( (INPUT_DBG_CALL), 
               "i_start %lld, i_size: %lld, i_tell: %lld, lsn %d", 
               p_area->i_start, p_area->i_size, 
               p_area->i_tell, p_vcd->cur_lsn );
        
    return VLC_SUCCESS;
}

/*****************************************************************************
  VCDEntryPoints: Reads the information about the entry points on the disc
  and initializes area information with that.
  Before calling this track information should have been read in.
 *****************************************************************************/
static int 
VCDEntryPoints( input_thread_t * p_input )
{
    thread_vcd_data_t *               p_vcd;
    unsigned int                      i_nb;
    unsigned int                      i, i_entry_index = 0;
    unsigned int                      i_previous_track = CDIO_INVALID_TRACK;

    p_vcd = (thread_vcd_data_t *) p_input->p_access_data;

    i_nb = vcdinfo_get_num_entries(p_vcd->vcd);
    if (0 == i_nb) 
      return -1;
    
    p_vcd->p_entries  = malloc( sizeof( lba_t ) * i_nb );

    if( p_vcd->p_entries == NULL )
    {
        LOG_ERR ("not enough memory for entry points treatment" );
        return -1;
    }

    p_vcd->num_entries = 0;

    for( i = 0 ; i < i_nb ; i++ )
    {
        track_t i_track = vcdinfo_get_track(p_vcd->vcd, i);
        if( i_track <= p_input->stream.i_area_nb )
        {
            p_vcd->p_entries[i] = 
              vcdinfo_get_entry_lsn(p_vcd->vcd, i);
            p_input->stream.pp_areas[i_track]->i_part_nb ++;

            /* if this entry belongs to a new track */
            if( i_track != i_previous_track )
            {
                /* i_plugin_data is used to store the first entry of the area*/
                p_input->stream.pp_areas[i_track]->i_plugin_data =
                                                            i_entry_index;
                i_previous_track = i_track;
                p_input->stream.pp_areas[i_track]->i_part_nb = 1;
            }
            i_entry_index ++;
            p_vcd->num_entries ++;
        }
        else
            msg_Warn( p_input, "wrong track number found in entry points" );
    }
    p_vcd->b_valid_ep = true;
    return 0;
}

/*****************************************************************************
 * VCDSegments: Reads the information about the segments the disc.
 *****************************************************************************/
static int
VCDSegments( input_thread_t * p_input )
{
    thread_vcd_data_t * p_vcd;
    unsigned int        i;
    unsigned int        num_segments;
    

    p_vcd = (thread_vcd_data_t *) p_input->p_access_data;
    num_segments = p_vcd->num_segments = vcdinfo_get_num_segments(p_vcd->vcd);

#define area p_input->stream.pp_areas

    /* area 0 is reserved for segments. Set Absolute start offset
         and size */
    area[0]->i_plugin_data = 0;
    input_DelArea( p_input, area[0] );
    input_AddArea( p_input, 0, 0 );
    
    area[0]->i_start = (off_t)p_vcd->p_sectors[0] 
      * (off_t)M2F2_SECTOR_SIZE;
    area[0]->i_size = (off_t)(p_vcd->p_sectors[1] - p_vcd->p_sectors[0])
      * (off_t)M2F2_SECTOR_SIZE;
    
    /* Default Segment  */
    area[0]->i_part = 0;
    
    /* i_plugin_data is used to store which entry point is the first
       of the track (area) */
    area[0]->i_plugin_data = 0;

    area[0]->i_part_nb = 0;
    
    dbg_print( INPUT_DBG_MRL, 
	       "area[0] id: %d, i_start: %lld, i_size: %lld", 
               area[0]->i_id, area[0]->i_start, area[0]->i_size );

    if (num_segments == 0) return 0;

    /* We have one additional segment allocated so we can get the size
       by subtracting seg[i+1] - seg[i].
     */
    p_vcd->p_segments = malloc( sizeof( lba_t ) * (num_segments+1) );
    if( p_vcd->p_segments == NULL )
    {
        LOG_ERR ("not enough memory for segment treatment" );
        return -1;
    }

    /* Update the navigation variables without triggering a callback */
    VCDUpdateVar( p_input, 0, VLC_VAR_SETVALUE, "title", "Setting track" );
    var_Change( p_input, "chapter", VLC_VAR_CLEARCHOICES, NULL, NULL );
    
    for( i = 0 ; i < num_segments ; i++ )
    {
      p_vcd->p_segments[i] = vcdinfo_get_seg_lsn(p_vcd->vcd, i);
      area[0]->i_part_nb ++;
      VCDUpdateVar( p_input, i , VLC_VAR_ADDCHOICE, 
                    "chapter", "Adding segment choice");
    }

#undef area

    p_vcd->p_segments[num_segments] = p_vcd->p_segments[num_segments-1]+
      vcdinfo_get_seg_sector_count(p_vcd->vcd, num_segments-1);
    
    return 0;
}

/*****************************************************************************
 VCDTracks: initializes area information. 
 Before calling this track information should have been read in.
 *****************************************************************************/
static void
VCDTracks( input_thread_t * p_input )
{
    thread_vcd_data_t * p_vcd;
    unsigned int        i;

    p_vcd = (thread_vcd_data_t *) p_input->p_access_data;

#define area p_input->stream.pp_areas

    /* We start area addressing for tracks at 1 since the default area 0
       is reserved for segments */

    for( i = 1 ; i < p_vcd->num_tracks ; i++ )
    {
        /* Tracks are Program Chains */
        input_AddArea( p_input, i, i );

        /* Absolute start byte offset and byte size */
        area[i]->i_start = (off_t) p_vcd->p_sectors[i] 
                           * (off_t)M2F2_SECTOR_SIZE;
        area[i]->i_size = (off_t)(p_vcd->p_sectors[i+1] - p_vcd->p_sectors[i])
                           * (off_t)M2F2_SECTOR_SIZE;

        /* Current entry being played in track */
        area[i]->i_part = 0;

        /* i_plugin_data is used to store which entry point is the first
         * of the track (area) */
        area[i]->i_plugin_data = 0;

        dbg_print( INPUT_DBG_MRL, 
                   "area[%d] id: %d, i_start: %lld, i_size: %lld", 
                   i, area[i]->i_id, area[i]->i_start, area[i]->i_size );
    }

#undef area

    return ;
}

/*****************************************************************************
  VCDLIDs: Reads the LIST IDs from the LOT.
 *****************************************************************************/
static int 
VCDLIDs( input_thread_t * p_input )
{
    thread_vcd_data_t *p_vcd = (thread_vcd_data_t *) p_input->p_access_data;

    p_vcd->num_lids = vcdinfo_get_num_LIDs(p_vcd->vcd);
    p_vcd->cur_lid  = VCDINFO_INVALID_ENTRY;

    if (vcdinfo_read_psd (p_vcd->vcd)) {
      
      vcdinfo_visit_lot (p_vcd->vcd, false);
      
#if FIXED
    /* 
       We need to change libvcdinfo to be more robust when there are 
       problems reading the extended PSD. Given that area-highlighting and 
       selection features in the extended PSD haven't been implemented,
       it's best then to not try to read this at all.
     */
      if (vcdinfo_get_psd_x_size(p_vcd->vcd))
        vcdinfo_visit_lot (p_vcd->vcd, true);
#endif 
    }

    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_MRL), 
               "num LIDs=%d", p_vcd->num_lids);

    return 0;
}

/*****************************************************************************
 * VCDParse: parse command line
 *****************************************************************************/
static char * 
VCDParse( input_thread_t * p_input, /*out*/ vcdinfo_itemid_t * p_itemid )
{
    thread_vcd_data_t *p_vcd = (thread_vcd_data_t *)p_input->p_access_data;
    char *             psz_parser;
    char *             psz_source;
    char *             psz_next;

    if ( config_GetInt( p_input, MODULE_STRING "-PBC" ) ) {
      p_itemid->type=VCDINFO_ITEM_TYPE_LID;
      p_itemid->num=1;
    } else {
      p_itemid->type=VCDINFO_ITEM_TYPE_TRACK;
      p_itemid->num=1;
    }
    
#ifdef WIN32
    /* On Win32 we want the VCD access plugin to be explicitly requested,
     * we end up with lots of problems otherwise */
    if( !p_input->psz_access || !*p_input->psz_access ) return NULL;
#endif

    if( !p_input->psz_name )
    {
        return NULL;
    }

    psz_parser = psz_source = strdup( p_input->psz_name );

    /* Parse input string :
     * [device][@[type][title]] */
    while( *psz_parser && *psz_parser != '@' )
    {
        psz_parser++;
    }

    if( *psz_parser == '@' )
    {
      /* Found the divide between the source name and the 
         type+entry number. */
      unsigned int num;
      
      *psz_parser = '\0';
      ++psz_parser;
      if( *psz_parser )
        {
          switch(*psz_parser) {
          case 'E': 
            p_itemid->type = VCDINFO_ITEM_TYPE_ENTRY;
            ++psz_parser;
            break;
          case 'P': 
            p_itemid->type = VCDINFO_ITEM_TYPE_LID;
            ++psz_parser;
            break;
          case 'S': 
            p_itemid->type = VCDINFO_ITEM_TYPE_SEGMENT;
            ++psz_parser;
            break;
          case 'T': 
            p_itemid->type = VCDINFO_ITEM_TYPE_TRACK;
            ++psz_parser;
            break;
          default: ;
          }
        }
      
      num = strtol( psz_parser, &psz_next, 10 );
      if ( *psz_parser != '\0' && *psz_next == '\0') 
        {
          p_itemid->num = num;
        }
      
    }

    if( !*psz_source ) {

      /* No source specified, so figure it out. */
      if( !p_input->psz_access ) return NULL;
      
      psz_source = config_GetPsz( p_input, MODULE_STRING "-device" );

      if( !psz_source || 0==strlen(psz_source) ) {
        /* Scan for a CD-ROM drive with a VCD in it. */
        char **cd_drives = cdio_get_devices_with_cap(NULL, 
                            (CDIO_FS_ANAL_SVCD|CDIO_FS_ANAL_CVD
                             |CDIO_FS_ANAL_VIDEOCD|CDIO_FS_UNKNOWN),
                                                     true);
        if (NULL == cd_drives) return NULL;
        if (cd_drives[0] == NULL) {
          cdio_free_device_list(cd_drives);
          return NULL;
        }
        psz_source = strdup(cd_drives[0]);
        cdio_free_device_list(cd_drives);
      }
    }

    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_MRL), 
               "source=%s entry=%d type=%d",
               psz_source, p_itemid->num, p_itemid->type);

    return psz_source;
}

/* 
   Set's start origin subsequent seeks/reads
*/
static void 
VCDSetOrigin( input_thread_t *p_input, lsn_t origin_lsn, 
              lsn_t cur_lsn, lsn_t end_lsn, int cur_entry, track_t cur_track )
{
  thread_vcd_data_t * p_vcd  = (thread_vcd_data_t *) p_input->p_access_data;

  p_vcd->origin_lsn = origin_lsn;
  p_vcd->cur_lsn    = cur_lsn;
  p_vcd->end_lsn    = end_lsn;
  p_vcd->cur_track  = cur_track;
  p_vcd->play_item.num  = cur_entry;
  p_vcd->play_item.type = VCDINFO_ITEM_TYPE_ENTRY;
  
  dbg_print( (INPUT_DBG_CALL|INPUT_DBG_LSN),
             "origin: %d, cur_lsn: %d, end_lsn: %d, entry: %d, track: %d",
             origin_lsn, cur_lsn, end_lsn, cur_entry, cur_track );

  p_input->stream.p_selected_area->i_tell =
    (off_t) (p_vcd->cur_lsn - p_vcd->origin_lsn) * (off_t)M2F2_SECTOR_SIZE;

  VCDUpdateVar( p_input, cur_entry, VLC_VAR_SETVALUE, 
                "chapter", "Setting entry");
}

/*****************************************************************************
 * vcd_Open: Opens a VCD device or file and returns an opaque handle
 *****************************************************************************/
static vcdinfo_obj_t *
vcd_Open( vlc_object_t *p_this, const char *psz_dev )
{
    vcdinfo_obj_t *p_vcdobj;
    char  *actual_dev;

    if( !psz_dev ) return NULL;

    /* Set where to log errors messages from libcdio. */
    p_vcd_input = (input_thread_t *)p_this;
    cdio_log_set_handler ( cdio_log_handler );
    vcd_log_set_handler ( vcd_log_handler );
    
    actual_dev=strdup(psz_dev);
    if ( vcdinfo_open(&p_vcdobj, &actual_dev, DRIVER_UNKNOWN, NULL) != 
         VCDINFO_OPEN_VCD) {
      free(actual_dev);
      return NULL;
    }
    free(actual_dev);

    return p_vcdobj;
}

/****************************************************************************
 * VCDReadSector: Read a sector (2324 bytes)
 ****************************************************************************/
static int 
VCDReadSector( vlc_object_t *p_this, const vcdinfo_obj_t *p_vcd,
               lsn_t cur_lsn, byte_t * p_buffer )
{
  typedef struct {
    uint8_t subheader   [8];
    uint8_t data        [M2F2_SECTOR_SIZE];
  } vcdsector_t;
  vcdsector_t vcd_sector;
  
  if (cdio_read_mode2_sector(vcdinfo_get_cd_image(p_vcd), 
                             &vcd_sector, cur_lsn, true) 
      != 0)
    {
      msg_Warn( p_this, "Could not read LSN %d", cur_lsn );
      return -1;
    }
    
  memcpy (p_buffer, vcd_sector.data, M2F2_SECTOR_SIZE);
  
  return( 0 );
}

/****************************************************************************
 Update the "varname" variable to i_num without triggering a callback.
****************************************************************************/
static void
VCDUpdateVar( input_thread_t *p_input, int i_num, int i_action,
              const char *varname, const char *label)
{
  vlc_value_t val;
  val.i_int = i_num;
  if (NULL != p_vcd_input) {
    thread_vcd_data_t *p_vcd = (thread_vcd_data_t *)p_vcd_input->p_access_data;
    dbg_print( INPUT_DBG_PBC, "%s %d", label, i_num );
  }
  var_Change( p_input, varname, i_action, &val, NULL );
}


#define meta_info_add_str(title, str) \
  if ( str ) {								\
    dbg_print( INPUT_DBG_META, "field: %s: %s\n", title, str);	\
    input_AddInfo( p_cat, _(title), "%s", str );			\
  }

#define meta_info_add_num(title, num) \
  dbg_print( INPUT_DBG_META, "field %s: %d\n", title, num);		\
  input_AddInfo( p_cat, _(title), "%d", num );				\

static void InformationCreate( input_thread_t *p_input  )
{
  thread_vcd_data_t *p_vcd = (thread_vcd_data_t *) p_input->p_access_data;
  input_info_category_t *p_cat;
  
  p_cat = input_InfoCategory( p_input, "General" );

  meta_info_add_str( "VCD Format", vcdinfo_get_format_version_str(p_vcd->vcd));
  meta_info_add_str( "Album",      vcdinfo_get_album_id(p_vcd->vcd));
  meta_info_add_str( "Application",vcdinfo_get_application_id(p_vcd->vcd));
  meta_info_add_str( "Preparer",   vcdinfo_get_preparer_id(p_vcd->vcd));
  meta_info_add_num( "Vol #",      vcdinfo_get_volume_num(p_vcd->vcd));
  meta_info_add_num( "Vol max #",  vcdinfo_get_volume_count(p_vcd->vcd));
  meta_info_add_str( "Volume Set", vcdinfo_get_volumeset_id(p_vcd->vcd));
  meta_info_add_str( "Volume",     vcdinfo_get_volume_id(p_vcd->vcd));
  meta_info_add_str( "Publisher",  vcdinfo_get_publisher_id(p_vcd->vcd));
  meta_info_add_str( "System Id",  vcdinfo_get_system_id(p_vcd->vcd));
  meta_info_add_num( "LIDs",       vcdinfo_get_num_LIDs(p_vcd->vcd));
  meta_info_add_num( "Entries",    vcdinfo_get_num_entries(p_vcd->vcd));
  meta_info_add_num( "Segments",   vcdinfo_get_num_segments(p_vcd->vcd));
  meta_info_add_num( "Tracks",     vcdinfo_get_num_tracks(p_vcd->vcd));

}

/*****************************************************************************
 * Public routines.
 *****************************************************************************/
int
E_(DebugCallback)   ( vlc_object_t *p_this, const char *psz_name,
		      vlc_value_t oldval, vlc_value_t val, void *p_data )
{
  thread_vcd_data_t *p_vcd;

  if (NULL == p_vcd_input) return VLC_EGENERIC;
  
  p_vcd = (thread_vcd_data_t *)p_vcd_input->p_access_data;

  if (p_vcd->i_debug & (INPUT_DBG_CALL|INPUT_DBG_EXT)) {
    msg_Dbg( p_vcd_input, "Old debug (x%0x) %d, new debug (x%0x) %d", 
             p_vcd->i_debug, p_vcd->i_debug, val.i_int, val.i_int);
  }
  p_vcd->i_debug = val.i_int;
  return VLC_SUCCESS;
}

/*****************************************************************************
  Open: open VCD.
  read in meta-information about VCD: the number of tracks, segments, 
  entries, size and starting information. Then set up state variables so
  that we read/seek starting at the location specified.

  On success we return VLC_SUCCESS, on memory exhausted VLC_ENOMEM, 
  and VLC_EGENERIC for some other error.
 *****************************************************************************/
int 
E_(Open) ( vlc_object_t *p_this )
{
    input_thread_t *        p_input = (input_thread_t *)p_this;
    thread_vcd_data_t *     p_vcd;
    char *                  psz_source;
    vcdinfo_itemid_t        itemid;
    bool                    b_play_ok;
    
    p_input->pf_read        = VCDRead;
    p_input->pf_seek        = VCDSeek;
    p_input->pf_set_area    = VCDSetArea;
    p_input->pf_set_program = VCDSetProgram;

    p_vcd = malloc( sizeof(thread_vcd_data_t) );

    if( p_vcd == NULL )
    {
        LOG_ERR ("out of memory" );
        return VLC_ENOMEM;
    }

    p_input->p_access_data = (void *)p_vcd;
    p_vcd->i_debug         = config_GetInt( p_this, MODULE_STRING "-debug" );
    psz_source             = VCDParse( p_input, &itemid );

    if ( NULL == psz_source ) 
    {
      free( p_vcd );
      return( VLC_EGENERIC );
    }

    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_EXT), "%s", psz_source );

    p_vcd->p_segments = NULL;
    p_vcd->p_entries  = NULL;
    
    /* set up input  */
    p_input->i_mtu = VCD_DATA_ONCE;

    vlc_mutex_lock( &p_input->stream.stream_lock );

    /* If we are here we can control the pace... */
    p_input->stream.b_pace_control = 1;

    p_input->stream.b_seekable = 1;
    p_input->stream.p_selected_area->i_size = 0;
    p_input->stream.p_selected_area->i_tell = 0;

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    if( !(p_vcd->vcd = vcd_Open( p_this, psz_source )) )
    {
        msg_Warn( p_input, "could not open %s", psz_source );
        free( psz_source );
        free( p_vcd );
        return VLC_EGENERIC;
    }

    /* Get track information. */
    p_vcd->num_tracks = ioctl_GetTracksMap( VLC_OBJECT(p_input),
                                            vcdinfo_get_cd_image(p_vcd->vcd), 
                                            &p_vcd->p_sectors );
    free( psz_source );
    if( p_vcd->num_tracks < 0 )
        LOG_ERR ("unable to count tracks" );
    else if( p_vcd->num_tracks <= 1 )
        LOG_ERR ("no movie tracks found" );
    if( p_vcd->num_tracks <= 1)
    {
        vcdinfo_close( p_vcd->vcd );
        free( p_vcd );
        return VLC_EGENERIC;
    }

    /* Set stream and area data */
    vlc_mutex_lock( &p_input->stream.stream_lock );

    /* Initialize ES structures */
    input_InitStream( p_input, sizeof( stream_ps_data_t ) );

    /* disc input method */
    p_input->stream.i_method = INPUT_METHOD_VCD;

    p_input->stream.i_area_nb = 1;
    

    /* Initialize segment information. */
    VCDSegments( p_input );
    
    /* Initialize track area information. */
    VCDTracks( p_input );
    
    if( VCDEntryPoints( p_input ) < 0 )
    {
        msg_Warn( p_input, "could not read entry points, will not use them" );
        p_vcd->b_valid_ep = false;
    }

    if( VCDLIDs( p_input ) < 0 )
    {
        msg_Warn( p_input, "could not read entry LIDs" );
    }

    b_play_ok = (VLC_SUCCESS == VCDPlay( p_input, itemid ));
    
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    if ( ! b_play_ok ) {
      vcdinfo_close( p_vcd->vcd );
      free( p_vcd );
      return VLC_EGENERIC;
    }

    if( !p_input->psz_demux || !*p_input->psz_demux )
    {
#if FIXED
      p_input->psz_demux = "vcdx";
#else
      p_input->psz_demux = "ps";
#endif
    }

    p_vcd->p_intf = intf_Create( p_input, "vcdx" );
    p_vcd->p_intf->b_block = VLC_FALSE;
    intf_RunThread( p_vcd->p_intf );

    InformationCreate( p_input );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: closes VCD releasing allocated memory.
 *****************************************************************************/
void 
E_(Close) ( vlc_object_t *p_this )
{
    input_thread_t *   p_input = (input_thread_t *)p_this;
    thread_vcd_data_t *p_vcd = (thread_vcd_data_t *)p_input->p_access_data;

    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_EXT), "VCDClose" );
    vcdinfo_close( p_vcd->vcd );

    free( p_vcd->p_entries );
    free( p_vcd->p_segments );

    /* For reasons that are a mystery to me we don't have to deal with
       stopping, and destroying the p_vcd->p_intf thread. And if we do
       it causes problems upstream.
     */
    if( p_vcd->p_intf != NULL )
    {
	p_vcd->p_intf = NULL;
    }

    free( p_vcd );
    p_input->p_access_data = NULL;
    p_vcd_input = NULL;
}
