/*****************************************************************************
 * vcd.c : VCD input module for vlc using libcdio, libvcd and libvcdinfo. 
 *         vlc-specific things tend to go here.
 *****************************************************************************
 * Copyright (C) 2000, 2003, 2004 VideoLAN
 * $Id$
 *
 * Authors: Rocky Bernstein <rocky@panix.com>
 *   Some code is based on the non-libcdio VCD plugin (as there really
 *   isn't real developer documentation yet on how to write a 
 *   navigable plugin.)
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
#include <vlc/intf.h>
#include <vlc/input.h>

#include "vcd.h"
#include "info.h"
#include "intf.h"
#include "vlc_keys.h"

#include <cdio/cdio.h>
#include <cdio/cd_types.h>
#include <cdio/logging.h>
#include <cdio/util.h>
#include <libvcd/info.h>
#include <libvcd/logging.h>

#define FREE_AND_NULL(ptr) if (NULL != ptr) free(ptr); ptr = NULL;

/* how many blocks VCDRead will read in each loop */
#define VCD_BLOCKS_ONCE 20

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/* First those which are accessed from outside (via pointers). */
static block_t *VCDReadBlock    ( access_t * );

static int      VCDControl      ( access_t *p_access, int i_query, 
				  va_list args );

/* Now those which are strictly internal */
static void VCDSetOrigin    ( access_t *p_access, lsn_t i_lsn, 
                              track_t track,  
			      const vcdinfo_itemid_t * p_itemid );
static int  VCDEntryPoints  ( access_t * );
static int  VCDLIDs         ( access_t * );
#ifdef FIXED
static int  VCDSegments     ( access_t * );
#endif
static int  VCDTitles       ( access_t * );
static int  VCDReadSector   ( vlc_object_t *p_this,
                              const vcdinfo_obj_t *p_vcd, lsn_t i_lsn,
                              byte_t * p_buffer );
static char *VCDParse       ( access_t *,
                              /*out*/ vcdinfo_itemid_t * p_itemid ,
                              /*out*/ vlc_bool_t *play_single_item );

static void VCDUpdateVar( access_t *p_access, int i_entry, int i_action,
                          const char *p_varname, char *p_label,
			  const char *p_debug_label );

static vcdinfo_obj_t *vcd_Open   ( vlc_object_t *p_this, const char *psz_dev );

/****************************************************************************
 * Private functions
 ****************************************************************************/

/* FIXME: This variable is a hack. Would be nice to eliminate the
   global-ness. */

static access_t *p_vcd_access = NULL;

/* process messages that originate from libcdio. */
static void
cdio_log_handler (cdio_log_level_t level, const char message[])
{
  const vcdplayer_t *p_vcd = (vcdplayer_t *)p_vcd_access->p_sys;
  switch (level) {
  case CDIO_LOG_DEBUG:
  case CDIO_LOG_INFO:
    if (p_vcd->i_debug & INPUT_DBG_CDIO)
      msg_Dbg( p_vcd_access, message);
    break;
  case CDIO_LOG_WARN:
    msg_Warn( p_vcd_access, message);
    break;
  case CDIO_LOG_ERROR:
  case CDIO_LOG_ASSERT:
    msg_Err( p_vcd_access, message);
    break;
  default:
    msg_Warn( p_vcd_access, message,
            _("The above message had unknown log level"),
            level);
  }
  return;
}

/* process messages that originate from vcdinfo. */
static void
vcd_log_handler (vcd_log_level_t level, const char message[])
{
  vcdplayer_t *p_vcd = (vcdplayer_t *)p_vcd_access->p_sys;
  switch (level) {
  case VCD_LOG_DEBUG:
  case VCD_LOG_INFO:
    if (p_vcd->i_debug & INPUT_DBG_VCDINFO)
      msg_Dbg( p_vcd_access, message);
    break;
  case VCD_LOG_WARN:
    msg_Warn( p_vcd_access, message);
    break;
  case VCD_LOG_ERROR:
  case VCD_LOG_ASSERT:
    msg_Err( p_vcd_access, message);
    break;
  default:
    msg_Warn( p_vcd_access, "%s\n%s %d", message,
            _("The above message had unknown vcdimager log level"),
            level);
  }
  return;
}

/*****************************************************************************
  VCDRead: reads VCD_BLOCKS_ONCE from the VCD and returns that.
  NULL is returned if something went wrong.
 *****************************************************************************/
static block_t *
VCDReadBlock( access_t * p_access )
{
    vcdplayer_t *p_vcd= (vcdplayer_t *)p_access->p_sys;
    block_t     *p_block;
    int         i_blocks = VCD_BLOCKS_ONCE;
    int         i_read;
    byte_t      p_last_sector[ M2F2_SECTOR_SIZE ];

    i_read = 0;

#if 0
    dbg_print( (INPUT_DBG_CALL), "lsn: %lu", 
	       (long unsigned int) p_vcd->i_lsn );
#endif

    /* Compute the number of blocks we have to read */

    i_blocks = VCD_BLOCKS_ONCE ;

    /* Allocate a block for the reading */
    if( !( p_block = block_New( p_access, i_blocks * M2F2_SECTOR_SIZE ) ) )
    {
        msg_Err( p_access, "cannot get a new block of size: %i",
                 i_blocks * M2F2_SECTOR_SIZE );
	block_Release( p_block );
        return NULL;
    }

    for ( i_read = 0 ; i_read < i_blocks ; i_read++ )
    {

      if ( p_vcd->i_lsn >= p_vcd->end_lsn ) {
        vcdplayer_read_status_t read_status;

        /* We've run off of the end of this entry. Do we continue or stop? */
        dbg_print( (INPUT_DBG_LSN|INPUT_DBG_PBC),
                   "end reached, cur: %lu, end: %u", 
		   (long unsigned int) p_vcd->i_lsn,
		   p_vcd->end_lsn);

        read_status = vcdplayer_pbc_is_on( p_vcd )
          ? vcdplayer_pbc_nav( p_access )
          : vcdplayer_non_pbc_nav( p_access );

        switch (read_status) {
        case READ_END:
          /* End reached. Return NULL to indicated this. */
        case READ_ERROR:
          /* Some sort of error. */
          return NULL;

        case READ_STILL_FRAME:
          {
            /* Reached the end of a still frame. */
            byte_t * p_buf = (byte_t *) p_block->p_buffer;

            p_buf += (i_read*M2F2_SECTOR_SIZE);
            memset(p_buf, 0, M2F2_SECTOR_SIZE);
            p_buf += 2;
            *p_buf = 0x01;
            dbg_print(INPUT_DBG_STILL, "Handled still event");

            p_vcd->p_intf->p_sys->b_still = VLC_TRUE;
            var_SetInteger( p_access, "state", PAUSE_S );

            return p_block;
          }
        default:
        case READ_BLOCK:
          break;
        }
      }

      if ( VCDReadSector( VLC_OBJECT(p_access), p_vcd->vcd,
                          p_vcd->i_lsn,
                          (byte_t *) p_block->p_buffer 
			  + (i_read*M2F2_SECTOR_SIZE) ) < 0 )
      {
          LOG_ERR ("could not read sector %lu", 
		   (long unsigned int) p_vcd->i_lsn );
        /* Try to skip one sector (in case of bad sectors) */
	  p_vcd->i_lsn ++;
	  p_access->info.i_pos += M2F2_SECTOR_SIZE;
          return NULL;
      }

      p_vcd->i_lsn ++;
      p_access->info.i_pos += M2F2_SECTOR_SIZE;

      /* Update seekpoint */
      if ( VCDINFO_ITEM_TYPE_ENTRY == p_vcd->play_item.type )
      {
	unsigned int i_entry = p_vcd->play_item.num+1;

	if (p_vcd->i_lsn >= vcdinfo_get_entry_lba(p_vcd->vcd, i_entry))
        {
	    const track_t i_track = p_vcd->i_track;
	    p_vcd->play_item.num = i_entry;
	    dbg_print( (INPUT_DBG_LSN|INPUT_DBG_PBC), "entry change" );
	    VCDSetOrigin( p_access, 
			  vcdinfo_get_entry_lba(p_vcd->vcd, i_entry),
			  i_track, &(p_vcd->play_item) );
        }
      }
    }

    if ( i_read != i_blocks ) /* this should not happen */
    {
        if ( VCDReadSector( VLC_OBJECT(p_access), p_vcd->vcd,
                            p_vcd->i_lsn, p_last_sector ) < 0 )
        {
            LOG_ERR ("could not read sector %lu", 
		     (long unsigned int) p_vcd->i_lsn );
        }
	
	p_vcd->i_lsn ++;
	return NULL;
    }

    return p_block;
}


/****************************************************************************
 * VCDSeek
 ****************************************************************************/
int
VCDSeek( access_t * p_access, int64_t i_pos )
{
    if (!p_access || !p_access->p_sys) return VLC_EGENERIC;
    
    {
      vcdplayer_t *p_vcd = (vcdplayer_t *)p_vcd_access->p_sys;
      const input_title_t *t = p_vcd->p_title[p_access->info.i_title];
      int i_seekpoint;
      unsigned int i_entry=VCDINFO_INVALID_ENTRY; 
      
      /* Next sector to read */
      p_access->info.i_pos = i_pos;
      p_vcd->i_lsn = (i_pos / (int64_t)M2F2_SECTOR_SIZE) +
	p_vcd->track_lsn;
      
      /* Find entry */
      if( p_vcd->b_valid_ep )
	{
	  for( i_entry = 0 ; i_entry < p_vcd->i_entries ; i_entry ++ )
	    {
	      if( p_vcd->i_lsn < p_vcd->p_entries[i_entry] )
		{
		  VCDUpdateVar( p_access, i_entry, VLC_VAR_SETVALUE,
				"chapter", _("Entry"), "Setting entry" );
		  break;
		}
	    }
	  p_vcd->play_item.num  = i_entry;
	  p_vcd->play_item.type = VCDINFO_ITEM_TYPE_ENTRY;
	  vcdplayer_set_origin(p_access);
	  VCDUpdateTitle(p_access);
	}
      
      dbg_print( (INPUT_DBG_CALL|INPUT_DBG_EXT|INPUT_DBG_SEEK),
		 "orig %lu, cur: %lu, offset: %lld, entry %d",
		 (long unsigned int) p_vcd->origin_lsn, 
		 (long unsigned int) p_vcd->i_lsn, i_pos,
		 i_entry );
      
      /* Find seekpoint */
      for( i_seekpoint = 0; i_seekpoint < t->i_seekpoint; i_seekpoint++ )
	{
	  if( i_seekpoint + 1 >= t->i_seekpoint ) break;
	  if( i_pos < t->seekpoint[i_seekpoint + 1]->i_byte_offset ) break;
	}
      
      /* Update current seekpoint */
      if( i_seekpoint != p_access->info.i_seekpoint )
	{
	  dbg_print( (INPUT_DBG_SEEK), "seekpoint change %lu", 
		     (long unsigned int) i_seekpoint );
	  p_access->info.i_update |= INPUT_UPDATE_SEEKPOINT;
	  p_access->info.i_seekpoint = i_seekpoint;
	}

    }
    return VLC_SUCCESS;
    
}

/*****************************************************************************
  VCDPlay: set up internal structures so seeking/reading places an item.
  itemid: the thing to play.
  user_entry: true if itemid is a user selection (rather than internally-
  generated selection such as via PBC) in which case we may have to adjust
  for differences in numbering.
 *****************************************************************************/
int
VCDPlay( access_t *p_access, vcdinfo_itemid_t itemid )
{
    if (!p_access || !p_access->p_sys) return VLC_EGENERIC;
    
    {
      
      vcdplayer_t *p_vcd= (vcdplayer_t *)p_access->p_sys;
#if 0
      const vlc_bool_t   b_was_still = p_vcd->in_still;
#endif
      
      dbg_print(INPUT_DBG_CALL, "itemid.num: %d, itemid.type: %d\n",
		itemid.num, itemid.type);

      switch (itemid.type) {
      case VCDINFO_ITEM_TYPE_TRACK:
	
	{
	  track_t i_track = itemid.num;
	  unsigned int i_entry = 
	    vcdinfo_track_get_entry( p_vcd->vcd, i_track);
	  
	  /* Valid MPEG tracks go from 1...i_tracks. */
	
	  if (i_track == 0 || i_track > p_vcd->i_tracks) {
	    LOG_ERR ("Invalid track number %d", i_track );
	    return VLC_EGENERIC;
	  }
	  p_vcd->in_still  = VLC_FALSE;
	  VCDSetOrigin( p_access, 
			vcdinfo_get_entry_lba(p_vcd->vcd, i_entry),
			i_track, &itemid );
	  break;
	}
	
      case VCDINFO_ITEM_TYPE_SEGMENT:
	{
	  int i_seg = itemid.num;
	  
	  /* Valid segments go from 0...i_segments-1. */
	  if (itemid.num >= p_vcd->i_segments) {
	    LOG_ERR ( "Invalid segment number: %d", i_seg );
	    return VLC_EGENERIC;
	  } else {
	    vcdinfo_video_segment_type_t segtype =
	      vcdinfo_get_video_type(p_vcd->vcd, i_seg);
	  
	    dbg_print(INPUT_DBG_PBC, "%s (%d), seg_num: %d",
		      vcdinfo_video_type2str(p_vcd->vcd, i_seg),
		      (int) segtype, itemid.num);
	    
	    switch (segtype)
	      {
	      case VCDINFO_FILES_VIDEO_NTSC_STILL:
	      case VCDINFO_FILES_VIDEO_NTSC_STILL2:
	      case VCDINFO_FILES_VIDEO_PAL_STILL:
	      case VCDINFO_FILES_VIDEO_PAL_STILL2:
		p_vcd->in_still = VLC_TRUE;
		break;
	      default:
		p_vcd->in_still = VLC_FALSE;
	      }
	    VCDSetOrigin( p_access, p_vcd->p_segments[i_seg], 
			  0, &itemid );
	  }

	}
	break;
	
      case VCDINFO_ITEM_TYPE_LID:
	/* LIDs go from 1..i_lids. */
	if (itemid.num == 0 || itemid.num > p_vcd->i_lids) {
	  LOG_ERR ( "Invalid LID number: %d", itemid.num );
	  return VLC_EGENERIC;
	} else {
	  p_vcd->i_lid = itemid.num;
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
	    return VCDPlay( p_access, trans_itemid );
	    break;
	  }
	    
	  case PSD_TYPE_PLAY_LIST: {
	    if (p_vcd->pxd.pld == NULL) return VLC_EGENERIC;
	    p_vcd->pdi = -1;
	    return vcdplayer_inc_play_item(p_access)
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
	{
	  int i_entry = itemid.num;
	  
	  /* Entries go from 0..i_entries-1. */
	  if (itemid.num >= p_vcd->i_entries) {
	    LOG_ERR ("Invalid entry number: %d", i_entry );
	    return VLC_EGENERIC;
	  } else {
	    track_t i_track = vcdinfo_get_track(p_vcd->vcd,  i_entry);
	    p_vcd->in_still = VLC_FALSE;
	    VCDSetOrigin( p_access, 
			  vcdinfo_get_entry_lba(p_vcd->vcd, i_entry),
			  i_track, &itemid );
	  }
	  break;
	}
	
      default:
	LOG_ERR ("unknown entry type" );
	return VLC_EGENERIC;
      }

      p_vcd->play_item = itemid;
      
    }
    
      
    return VLC_SUCCESS;
}

/*****************************************************************************
  VCDEntryPoints: Reads the information about the entry points on the disc
  and initializes area information with that.
  Before calling this track information should have been read in.
 *****************************************************************************/
static int
VCDEntryPoints( access_t * p_access )
{
  if (!p_access || !p_access->p_sys) return VLC_EGENERIC;
  
  {
    vcdplayer_t *p_vcd = (vcdplayer_t *) p_access->p_sys;
    const unsigned int i_entries  =  vcdinfo_get_num_entries(p_vcd->vcd);
    const track_t      i_last_track 
      = cdio_get_num_tracks(vcdinfo_get_cd_image(p_vcd->vcd))
      + cdio_get_first_track_num(vcdinfo_get_cd_image(p_vcd->vcd));
    unsigned int i;
   
    if (0 == i_entries) {
      LOG_ERR ("no entires found -- something is wrong" );
      return VLC_EGENERIC;
    }
    
    p_vcd->p_entries  = malloc( sizeof( lsn_t ) * i_entries );
    
    if( p_vcd->p_entries == NULL )
      {
	LOG_ERR ("not enough memory for entry points treatment" );
	return VLC_EGENERIC;
      }
    
    p_vcd->i_entries = i_entries;
    
    for( i = 0 ; i < i_entries ; i++ )
    {
	const track_t i_track = vcdinfo_get_track(p_vcd->vcd, i);
	if( i_track <= i_last_track ) {
	  seekpoint_t *s = vlc_seekpoint_New();
	  char psz_entry[100];
	  
	  snprintf(psz_entry, sizeof(psz_entry), "%s%02d", _("Entry "), i );

	  p_vcd->p_entries[i] = vcdinfo_get_entry_lba(p_vcd->vcd, i);
	  
	  s->psz_name      = strdup(psz_entry);
	  s->i_byte_offset = 
	    (p_vcd->p_entries[i] - vcdinfo_get_track_lba(p_vcd->vcd, i_track))
	    * M2F2_SECTOR_SIZE;
	  
	  dbg_print( INPUT_DBG_MRL, 
		     "%s, lsn %d,  byte_offset %ld\n",
		     s->psz_name, p_vcd->p_entries[i], 
		     (unsigned long int) s->i_byte_offset);
	  TAB_APPEND( p_vcd->p_title[i_track-1]->i_seekpoint,
		      p_vcd->p_title[i_track-1]->seekpoint, s );

	} else 
	  msg_Warn( p_access, "wrong track number found in entry points" );
    }
    p_vcd->b_valid_ep = VLC_TRUE;
    return 0;
  }
}

/*????? FIXME!!! */
#ifdef FIXED 
/*****************************************************************************
 * VCDSegments: Reads the information about the segments the disc.
 *****************************************************************************/
static int
VCDSegments( access_t * p_access )
{
    vcdplayer_t  *p_vcd;
    unsigned int  i;
    unsigned int  i_segments;


    p_vcd = (vcdplayer_t *) p_access->p_sys;
    i_segments = p_vcd->num_segments = vcdinfo_get_num_segments(p_vcd->vcd);

#define area p_access->stream.pp_areas

    /* area 0 is reserved for segments. Set Absolute start offset
         and size */
    area[0]->i_plugin_data = 0;
    input_DelArea( p_access, area[0] );
    input_AddArea( p_access, 0, 0 );

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

    if (i_segments == 0) return 0;

    /* We have one additional segment allocated so we can get the size
       by subtracting seg[i+1] - seg[i].
     */
    p_vcd->p_segments = malloc( sizeof( lsn_t ) * (i_segments+1) );
    if( p_vcd->p_segments == NULL )
    {
        LOG_ERR ("not enough memory for segment treatment" );
        return -1;
    }

    /* Update the navigation variables without triggering a callback */
    VCDUpdateVar( p_access, 0, VLC_VAR_SETVALUE, "title", _("Track"),
		  "Setting track" );

    var_Change( p_access, "chapter", VLC_VAR_CLEARCHOICES, NULL, NULL );

    for( i = 0 ; i < i_segments ; i++ )
    {
      p_vcd->p_segments[i] = vcdinfo_get_seg_lsn(p_vcd->vcd, i);
      area[0]->i_part_nb ++;
      VCDUpdateVar( p_access, i , VLC_VAR_ADDCHOICE,
                    "chapter", _("Segment"), "Adding segment choice");
    }

#undef area

    p_vcd->p_segments[i_segments] = p_vcd->p_segments[i_segments-1]+
      vcdinfo_get_seg_sector_count(p_vcd->vcd, i_segments-1);

    return 0;
}
#endif

/*****************************************************************************
 Build title table which will be returned via ACCESS_GET_TITLE_INFO.

 We start area addressing for tracks at 1 since the default area 0
 is reserved for segments. 
 *****************************************************************************/
static int
VCDTitles( access_t * p_access )
{
    /* We'll assume a VCD has its first MPEG track
       cdio_get_first_track_num()+1 could be used if one wanted to be
       very careful about this. Note: cdio_get_first_track() will give the
       ISO-9660 track before the MPEG tracks.
     */
  
    if (!p_access || !p_access->p_sys) return VLC_EGENERIC;

    {
        vcdplayer_t *p_vcd = (vcdplayer_t *) p_access->p_sys;
	track_t            i;

	p_vcd->i_titles = 0;
	for( i = 1 ; i <= p_vcd->i_tracks ; i++ )
        {
	    input_title_t *t = p_vcd->p_title[i-1] = vlc_input_title_New();
	    char psz_track[100];
	    uint32_t i_secsize = vcdinfo_get_track_sect_count( p_vcd->vcd, i );
	    
	    snprintf( psz_track, sizeof(psz_track), "%s%02d", _("Track "), 
		      i );
	    
	    t->i_size    = (i_secsize) * (int64_t) M2F2_SECTOR_SIZE;
	    t->psz_name  = strdup(psz_track);
	    
	    dbg_print( INPUT_DBG_MRL, "track[%d] i_size: %lld",
		       i, t->i_size );

	    p_vcd->i_titles++;
	}
      
      return VLC_SUCCESS;
    }
}

/*****************************************************************************
  VCDLIDs: Reads the LIST IDs from the LOT.
 *****************************************************************************/
static int
VCDLIDs( access_t * p_access )
{
    vcdplayer_t *p_vcd = (vcdplayer_t *) p_access->p_sys;

    p_vcd->i_lids = vcdinfo_get_num_LIDs(p_vcd->vcd);
    p_vcd->i_lid  = VCDINFO_INVALID_ENTRY;

    if (vcdinfo_read_psd (p_vcd->vcd)) {

      vcdinfo_visit_lot (p_vcd->vcd, VLC_FALSE);

#if FIXED
    /*
       We need to change libvcdinfo to be more robust when there are
       problems reading the extended PSD. Given that area-highlighting and
       selection features in the extended PSD haven't been implemented,
       it's best then to not try to read this at all.
     */
      if (vcdinfo_get_psd_x_size(p_vcd->vcd))
        vcdinfo_visit_lot (p_vcd->vcd, VLC_TRUE);
#endif
    }

    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_MRL),
               "num LIDs=%d", p_vcd->i_lids);

    return 0;
}

/*****************************************************************************
 * VCDParse: parse command line
 *****************************************************************************/
static char *
VCDParse( access_t * p_access, /*out*/ vcdinfo_itemid_t * p_itemid,
          /*out*/ vlc_bool_t *play_single_item )
{
    vcdplayer_t *p_vcd = (vcdplayer_t *)p_access->p_sys;
    char        *psz_parser;
    char        *psz_source;
    char        *psz_next;

    if( config_GetInt( p_access, MODULE_STRING "-PBC" ) ) {
      p_itemid->type = VCDINFO_ITEM_TYPE_LID;
      p_itemid->num = 1;
      *play_single_item = VLC_FALSE;
    }
    else 
    {
      p_itemid->type = VCDINFO_ITEM_TYPE_ENTRY;
      p_itemid->num = 0;
    }

#ifdef WIN32
    /* On Win32 we want the VCD access plugin to be explicitly requested,
     * we end up with lots of problems otherwise */
    if( !p_access->psz_access || !*p_access->psz_access ) return NULL;
#endif

    if( !p_access->psz_path )
    {
        return NULL;
    }

    psz_parser = psz_source = strdup( p_access->psz_path );

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
            *play_single_item = VLC_TRUE;
            break;
          case 'P':
            p_itemid->type = VCDINFO_ITEM_TYPE_LID;
            ++psz_parser;
            *play_single_item = VLC_FALSE;
            break;
          case 'S':
            p_itemid->type = VCDINFO_ITEM_TYPE_SEGMENT;
            ++psz_parser;
            *play_single_item = VLC_TRUE;
            break;
          case 'T':
            p_itemid->type = VCDINFO_ITEM_TYPE_TRACK;
            ++psz_parser;
            *play_single_item = VLC_TRUE;
            break;
          default: ;
          }
        }

      num = strtol( psz_parser, &psz_next, 10 );
      if ( *psz_parser != '\0' && *psz_next == '\0')
        {
          p_itemid->num = num;
        }

    } else {
      *play_single_item = ( VCDINFO_ITEM_TYPE_LID == p_itemid->type );
    }


    if( !*psz_source ) {

      /* No source specified, so figure it out. */
      if( !p_access->psz_access ) return NULL;

      psz_source = config_GetPsz( p_access, "vcd" );

      if( !psz_source || 0==strlen(psz_source) ) {
        /* Scan for a CD-ROM drive with a VCD in it. */
        char **cd_drives = cdio_get_devices_with_cap( NULL,
                            ( CDIO_FS_ANAL_SVCD | CDIO_FS_ANAL_CVD
                              |CDIO_FS_ANAL_VIDEOCD | CDIO_FS_UNKNOWN ),
                                                     VLC_TRUE );
        if( NULL == cd_drives ) return NULL;
        if( cd_drives[0] == NULL )
	{
          cdio_free_device_list( cd_drives );
          return NULL;
        }
        psz_source = strdup( cd_drives[0] );
        cdio_free_device_list( cd_drives );
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
VCDSetOrigin( access_t *p_access, lsn_t i_lsn, track_t i_track, 
	      const vcdinfo_itemid_t *p_itemid )
{
  vcdplayer_t *p_vcd= (vcdplayer_t *)p_access->p_sys;
  
  unsigned int i_title = i_track - 1; /* For now */

  p_vcd->i_lsn      = i_lsn;
  p_vcd->i_track    = i_track;
  p_vcd->track_lsn  = vcdinfo_get_track_lsn(p_vcd->vcd, i_track);

  p_vcd->play_item.num  = p_itemid->num;
  p_vcd->play_item.type = p_itemid->type;

  vcdplayer_set_origin(p_access);

  p_access->info.i_title     = i_track-1;
  p_access->info.i_size      = p_vcd->p_title[i_title]->i_size;
  p_access->info.i_pos       = ( i_lsn - p_vcd->track_lsn ) 
                             * M2F2_SECTOR_SIZE;
  p_access->info.i_update   |= INPUT_UPDATE_TITLE|INPUT_UPDATE_SIZE
                            |  INPUT_UPDATE_SEEKPOINT;

  dbg_print( (INPUT_DBG_CALL|INPUT_DBG_LSN),
             "i_lsn: %lu, track: %d", (long unsigned int) i_lsn, 
	     i_track );

  if (p_itemid->type == VCDINFO_ITEM_TYPE_ENTRY) {
    VCDUpdateVar( p_access, p_itemid->num, VLC_VAR_SETVALUE,
		  "chapter", _("Entry"), "Setting entry/segment");
    p_access->info.i_seekpoint = p_itemid->num;
  } else {
    VCDUpdateVar( p_access, p_itemid->num, VLC_VAR_SETVALUE,
		  "chapter", _("Segment"),  "Setting entry/segment");
    /* seekpoint is what? ??? */ 
  }

  VCDUpdateTitle( p_access );
  
}

/*****************************************************************************
 * vcd_Open: Opens a VCD device or file initializes, a list of 
   tracks, segements and entry lsns and sizes and returns an opaque handle.
 *****************************************************************************/
static vcdinfo_obj_t *
vcd_Open( vlc_object_t *p_this, const char *psz_dev )
{
    access_t    *p_access = (access_t *)p_this;
    vcdplayer_t *p_vcd    = (vcdplayer_t *) p_access->p_sys;
    vcdinfo_obj_t *p_vcdobj;
    char  *actual_dev;
    unsigned int i;

    dbg_print(INPUT_DBG_CALL, "called with %s\n", psz_dev);

    if( !psz_dev ) return NULL;

    actual_dev=strdup(psz_dev);
    if ( vcdinfo_open(&p_vcdobj, &actual_dev, DRIVER_UNKNOWN, NULL) !=
         VCDINFO_OPEN_VCD) {
      free(actual_dev);
      return NULL;
    }
    free(actual_dev);

    /* 
       Save summary info on tracks, segments and entries... 
    */
    
    if ( 0 < (p_vcd->i_tracks = vcdinfo_get_num_tracks(p_vcdobj)) ) {
      p_vcd->track = (vcdplayer_play_item_info_t *) 
	calloc(p_vcd->i_tracks, sizeof(vcdplayer_play_item_info_t));
      
      for (i=0; i<p_vcd->i_tracks; i++) { 
	unsigned int track_num=i+1;
	p_vcd->track[i].size  = 
	  vcdinfo_get_track_sect_count(p_vcdobj, track_num);
	p_vcd->track[i].start_LSN = 
	  vcdinfo_get_track_lsn(p_vcdobj, track_num);
      }
    } else 
      p_vcd->track = NULL;
    
    if ( 0 < (p_vcd->i_entries = vcdinfo_get_num_entries(p_vcdobj)) ) {
      p_vcd->entry = (vcdplayer_play_item_info_t *) 
	calloc(p_vcd->i_entries, sizeof(vcdplayer_play_item_info_t));
      
      for (i=0; i<p_vcd->i_entries; i++) { 
	p_vcd->entry[i].size      = vcdinfo_get_entry_sect_count(p_vcdobj, i);
	p_vcd->entry[i].start_LSN = vcdinfo_get_entry_lba(p_vcdobj, i);
      }
    } else 
      p_vcd->entry = NULL;
    
    if ( 0 < (p_vcd->i_segments = vcdinfo_get_num_segments(p_vcdobj)) ) {
      p_vcd->segment = (vcdplayer_play_item_info_t *) 
	calloc(p_vcd->i_segments,  sizeof(vcdplayer_play_item_info_t));
      
      for (i=0; i<p_vcd->i_segments; i++) { 
	p_vcd->segment[i].size = vcdinfo_get_seg_sector_count(p_vcdobj, i);
	p_vcd->segment[i].start_LSN = vcdinfo_get_seg_lba(p_vcdobj, i);
      }
    } else 
      p_vcd->segment = NULL;
    
    
    return p_vcdobj;
}

/****************************************************************************
 * VCDReadSector: Read a sector (2324 bytes)
 ****************************************************************************/
static int
VCDReadSector( vlc_object_t *p_this, const vcdinfo_obj_t *p_vcd,
               lsn_t i_lsn, byte_t * p_buffer )
{
  typedef struct {
    uint8_t subheader   [CDIO_CD_SUBHEADER_SIZE];
    uint8_t data        [M2F2_SECTOR_SIZE];
    uint8_t spare       [4];
  } vcdsector_t;
  vcdsector_t vcd_sector;

  if( cdio_read_mode2_sector( vcdinfo_get_cd_image( p_vcd ),
                              &vcd_sector, i_lsn, VLC_TRUE )
      != 0)
  {
      msg_Warn( p_this, "Could not read LSN %lu", 
		(long unsigned int) i_lsn );
      return -1;
  }

  memcpy (p_buffer, vcd_sector.data, M2F2_SECTOR_SIZE);

  return( 0 );
}

/****************************************************************************
 Update the "varname" variable to i_num without triggering a callback.
****************************************************************************/
static void
VCDUpdateVar( access_t *p_access, int i_num, int i_action,
              const char *p_varname, char *p_label, 
	      const char *p_debug_label)
{
  vlc_value_t val;
  val.i_int = i_num;
  if (p_access) {
    const vcdplayer_t *p_vcd = (vcdplayer_t *)p_vcd_access->p_sys;
    dbg_print( INPUT_DBG_PBC, "%s %d", p_debug_label, i_num );
  }
  if (p_label) {
    vlc_value_t text;
    text.psz_string = p_label;
    var_Change( p_access, p_varname, VLC_VAR_SETTEXT, &text, NULL );
  }
  var_Change( p_access, p_varname, i_action, &val, NULL );
}


/*****************************************************************************
 * Public routines.
 *****************************************************************************/
int
E_(DebugCallback)   ( vlc_object_t *p_this, const char *psz_name,
                      vlc_value_t oldval, vlc_value_t val, void *p_data )
{
  vcdplayer_t *p_vcd;

  if (NULL == p_vcd_access) return VLC_EGENERIC;

  p_vcd = (vcdplayer_t *)p_vcd_access->p_sys;

  if (p_vcd->i_debug & (INPUT_DBG_CALL|INPUT_DBG_EXT)) {
    msg_Dbg( p_vcd_access, "Old debug (x%0x) %d, new debug (x%0x) %d",
             p_vcd->i_debug, p_vcd->i_debug, val.i_int, val.i_int);
  }
  p_vcd->i_debug = val.i_int;
  return VLC_SUCCESS;
}


/*****************************************************************************
  VCDOpen: open VCD.
  read in meta-information about VCD: the number of tracks, segments,
  entries, size and starting information. Then set up state variables so
  that we read/seek starting at the location specified.

  On success we return VLC_SUCCESS, on memory exhausted VLC_ENOMEM,
  and VLC_EGENERIC for some other error.
 *****************************************************************************/
int
E_(VCDOpen) ( vlc_object_t *p_this )
{
    access_t         *p_access = (access_t *)p_this;
    vcdplayer_t      *p_vcd;
    char             *psz_source;
    vcdinfo_itemid_t  itemid;
    vlc_bool_t        b_play_ok;
    vlc_bool_t        play_single_item = VLC_FALSE;

    p_access->pf_read          = NULL;
    p_access->pf_block         = VCDReadBlock; 
    p_access->pf_control       = VCDControl;
    p_access->pf_seek          = VCDSeek;

    p_access->info.i_update    = 0;
    p_access->info.i_size      = 0;
    p_access->info.i_pos       = 0;
    p_access->info.b_eof       = VLC_FALSE;
    p_access->info.i_title     = 0;
    p_access->info.i_seekpoint = 0;

    p_vcd = malloc( sizeof(vcdplayer_t) );

    if( p_vcd == NULL )
    {
        LOG_ERR ("out of memory" );
        return VLC_ENOMEM;
    }

    p_access->p_sys     = (access_sys_t *) p_vcd;

    /* Set where to log errors messages from libcdio. */
    p_vcd_access = p_access;
    cdio_log_set_handler ( cdio_log_handler );
    vcd_log_set_handler ( vcd_log_handler );

    psz_source = VCDParse( p_access, &itemid, &play_single_item );

    if ( NULL == psz_source )
    {
      free( p_vcd );
      return( VLC_EGENERIC );
    }

    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_EXT), "source: %s: mrl: %s",
               psz_source, p_access->psz_path );

    p_vcd->psz_source      = strdup(psz_source);
    p_vcd->i_debug         = config_GetInt( p_this, MODULE_STRING "-debug" );
    p_vcd->in_still        = VLC_FALSE;
    p_vcd->play_item.type  = VCDINFO_ITEM_TYPE_NOTFOUND;
    p_vcd->p_input         = vlc_object_find( p_access, VLC_OBJECT_INPUT, 
					      FIND_PARENT );
    p_vcd->p_meta          = vlc_meta_New();
    p_vcd->p_segments      = NULL;
    p_vcd->p_entries       = NULL;

    /* set up input  */

    if( !(p_vcd->vcd = vcd_Open( p_this, psz_source )) )
    {
        msg_Warn( p_access, "could not open %s", psz_source );
        goto err_exit;
    }

    p_vcd->b_svd= (vlc_bool_t) vcdinfo_get_tracksSVD(p_vcd->vcd);;
    
    /* Get track information. */
    p_vcd->i_tracks = vcdinfo_get_num_tracks(p_vcd->vcd);

    if( p_vcd->i_tracks < 1 || CDIO_INVALID_TRACK == p_vcd->i_tracks ) {
        vcdinfo_close( p_vcd->vcd );
        LOG_ERR ("no movie tracks found" );
        goto err_exit;
    }
    
#ifdef FIXED
    /* Initialize segment information. */
    VCDSegments( p_access );
#endif

    /* Build Navigation Title table. */
    VCDTitles( p_access );

    /* Map entry points into Chapters */
    if( VCDEntryPoints( p_access ) < 0 )
    {
        msg_Warn( p_access, "could not read entry points, will not use them" );
        p_vcd->b_valid_ep = VLC_FALSE;
    }

    if( VCDLIDs( p_access ) < 0 )
    {
        msg_Warn( p_access, "could not read entry LIDs" );
    }

    b_play_ok = (VLC_SUCCESS == VCDPlay( p_access, itemid ));

    if ( ! b_play_ok ) {
      vcdinfo_close( p_vcd->vcd );
      goto err_exit;
    }

    p_access->psz_demux = strdup( "ps" );

#if FIXED
    p_vcd->p_intf = intf_Create( p_access, "vcdx" );
    p_vcd->p_intf->b_block = VLC_FALSE;
    intf_RunThread( p_vcd->p_intf );
#endif

#if FIXED
    if (play_single_item)
      VCDFixupPlayList( p_access, p_vcd, psz_source, &itemid, 
			play_single_item );
#endif
    

    free( psz_source );

    return VLC_SUCCESS;
 err_exit:
    free( psz_source );
    free( p_vcd );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * VCDClose: closes VCD releasing allocated memory.
 *****************************************************************************/
void
E_(VCDClose) ( vlc_object_t *p_this )
{
    access_t    *p_access = (access_t *)p_this;
    vcdplayer_t *p_vcd = (vcdplayer_t *)p_access->p_sys;

    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_EXT), "VCDClose" );

    vcdinfo_close( p_vcd->vcd );

    FREE_AND_NULL( p_vcd->p_entries );
    FREE_AND_NULL( p_vcd->p_segments );
    FREE_AND_NULL( p_vcd->psz_source );
    FREE_AND_NULL( p_vcd->track );
    FREE_AND_NULL( p_vcd->segment );
    FREE_AND_NULL( p_vcd->entry ); 

    free( p_vcd );
    p_access->p_sys = NULL;
    p_vcd_access    = NULL;
}

/*****************************************************************************
 * Control: The front-end or vlc engine calls here to ether get
 * information such as meta information or plugin capabilities or to
 * issue miscellaneous "set" requests.
 *****************************************************************************/
static int VCDControl( access_t *p_access, int i_query, va_list args )
{
    vcdplayer_t *p_vcd= (vcdplayer_t *)p_access->p_sys;
    int         *pi_int;
    int i;

    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_EXT|INPUT_DBG_EVENT),
               "query %d", i_query );

    switch( i_query )
    {
        /* Pass back a copy of meta information that was gathered when we
	   during the Open/Initialize call.
	 */
        case ACCESS_GET_META:
	  { 
	    vlc_meta_t **pp_meta = (vlc_meta_t**)va_arg( args, vlc_meta_t** );

	    dbg_print( INPUT_DBG_EVENT, "get meta info" );

	    if ( p_vcd->p_meta ) {
	      *pp_meta = vlc_meta_Duplicate( p_vcd->p_meta );
	      dbg_print( INPUT_DBG_META, "%s", "Meta copied" );
	    } else 
	      msg_Warn( p_access, "tried to copy NULL meta info" );
	    
	    return VLC_SUCCESS;
	  }
	  return VLC_EGENERIC;

        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE: 
	  {
            vlc_bool_t *pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );

	    dbg_print( INPUT_DBG_EVENT, 
		       "seek/fastseek/pause/can_control_pace" );
            *pb_bool = VLC_TRUE;
	    return VLC_SUCCESS;
            break;
	  }

        /* */
        case ACCESS_GET_MTU:
            pi_int = (int*)va_arg( args, int * );
            *pi_int = (VCD_BLOCKS_ONCE * M2F2_SECTOR_SIZE);
	    dbg_print( INPUT_DBG_EVENT, "GET MTU: %d", *pi_int );
            break;

        case ACCESS_GET_PTS_DELAY:
	  { 
	    int64_t *pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = var_GetInteger( p_access, MODULE_STRING "-caching" )
	      * MILLISECONDS_PER_SEC;
	    dbg_print( INPUT_DBG_EVENT, "GET PTS DELAY" );
	    return VLC_SUCCESS;
            break;
	  }

        /* */
        case ACCESS_SET_PAUSE_STATE:
	    dbg_print( INPUT_DBG_EVENT, "SET PAUSE STATE" );
	    return VLC_SUCCESS;
            break;

        case ACCESS_GET_TITLE_INFO:
	  { 
	    unsigned int psz_mrl_max = strlen(VCD_MRL_PREFIX) 
	      + strlen(p_vcd->psz_source) + sizeof("@E999")+3;
	    input_title_t ***ppp_title
	      = (input_title_t***)va_arg( args, input_title_t*** );
	    char *psz_mrl = malloc( psz_mrl_max );
	    unsigned int i;

            pi_int    = (int*)va_arg( args, int* );

	    dbg_print( INPUT_DBG_EVENT, "GET TITLE: i_titles %d", 
		       p_vcd->i_titles );

	    if( psz_mrl == NULL ) {
	       msg_Warn( p_access, "out of memory" );
	    } else {
  	       snprintf(psz_mrl, psz_mrl_max, "%s%s",
			VCD_MRL_PREFIX, p_vcd->psz_source);
	       VCDMetaInfo( p_access, psz_mrl );
	       free(psz_mrl);
	    }

            /* Duplicate title info */
            if( p_vcd->i_titles == 0 )
            {
                *pi_int = 0; ppp_title = NULL;
                return VLC_SUCCESS;
            }
            *pi_int = p_vcd->i_titles;
            *ppp_title = malloc(sizeof( input_title_t **) * p_vcd->i_titles );

	    if (!*ppp_title) return VLC_ENOMEM;

	    for( i = 0; i < p_vcd->i_titles; i++ )
	    {
		if ( p_vcd->p_title[i] )
		  (*ppp_title)[i] = 
		    vlc_input_title_Duplicate( p_vcd->p_title[i] );
	    }
	  }
	  break;

        case ACCESS_SET_TITLE:
            i = (int)va_arg( args, int );

	    dbg_print( INPUT_DBG_EVENT, "set title %d" , i);
            if( i != p_access->info.i_title )
            {
	        vcdinfo_itemid_t itemid;
		track_t          i_track = i+1;
		unsigned int     i_entry = 
		  vcdinfo_track_get_entry( p_vcd->vcd, i_track);

		/* FIXME! For now we are assuming titles are only 
		 tracks and that track == title+1 */
		itemid.num = i_track;
		itemid.type = VCDINFO_ITEM_TYPE_TRACK;
		
		VCDSetOrigin(p_access, 
			     vcdinfo_get_entry_lba(p_vcd->vcd, i_entry),
			     i_track, &itemid );
	    }
            break;

        case ACCESS_SET_SEEKPOINT:
        {
            input_title_t *t = p_vcd->p_title[p_access->info.i_title];
            i = (int)va_arg( args, int );

	    dbg_print( INPUT_DBG_EVENT, "set seekpoint" );
            if( t->i_seekpoint > 0 )
            {
		track_t i_track = p_access->info.i_title+1;

		/* FIXME! For now we are assuming titles are only 
		 tracks and that track == title+1 and we the play
		 item is entries (not tracks or lids).
		 We need to generalize all of this.
		*/

		p_vcd->play_item.num  = i;
		p_vcd->play_item.type = VCDINFO_ITEM_TYPE_ENTRY;

		VCDSetOrigin( p_access, 
			      vcdinfo_get_entry_lba(p_vcd->vcd, i),
			      i_track, &(p_vcd->play_item) );
            }
            return VLC_SUCCESS;
        }

        case ACCESS_SET_PRIVATE_ID_STATE:
	    dbg_print( INPUT_DBG_EVENT, "set private id" );
            return VLC_EGENERIC;

        default:
	  msg_Warn( p_access, "unimplemented query in control" );
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}
