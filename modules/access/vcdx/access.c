/*****************************************************************************
 * vcd.c : VCD input module for vlc using libcdio, libvcd and libvcdinfo.
 *         vlc-specific things tend to go here.
 *****************************************************************************
 * Copyright (C) 2000, 2003, 2004, 2005 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_interface.h>
#include <vlc_input.h>
#include <vlc_access.h>
#include <vlc_charset.h>
#include "vlc_keys.h"

#include <cdio/cdio.h>
#include <cdio/cd_types.h>
#include <cdio/logging.h>
#include <cdio/util.h>
#include <libvcd/info.h>
#include <libvcd/logging.h>
#include "vcd.h"
#include "info.h"
#include "intf.h"

extern void VCDSetOrigin( access_t *p_access, lsn_t i_lsn, track_t i_track,
                          const vcdinfo_itemid_t *p_itemid );

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

/* First those which are accessed from outside (via pointers). */
static block_t *VCDReadBlock    ( access_t * );

static int      VCDControl      ( access_t *p_access, int i_query,
                                  va_list args );

/* Now those which are strictly internal */
static bool  VCDEntryPoints  ( access_t * );
static bool  VCDLIDs         ( access_t * );
static bool  VCDSegments     ( access_t * );
static int  VCDTitles       ( access_t * );
static char *VCDParse       ( access_t *,
                              /*out*/ vcdinfo_itemid_t * p_itemid ,
                              /*out*/ bool *play_single_item );

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
  const vcdplayer_t *p_vcdplayer = (vcdplayer_t *)p_vcd_access->p_sys;
  switch (level) {
  case CDIO_LOG_DEBUG:
  case CDIO_LOG_INFO:
    if (p_vcdplayer->i_debug & INPUT_DBG_CDIO)
      msg_Dbg( p_vcd_access, "%s", message);
    break;
  case CDIO_LOG_WARN:
    msg_Warn( p_vcd_access, "%s", message);
    break;
  case CDIO_LOG_ERROR:
  case CDIO_LOG_ASSERT:
    msg_Err( p_vcd_access, "%s", message);
    break;
  default:
    msg_Warn( p_vcd_access, "%s\n%s %d", message,
            _("The above message had unknown log level"),
            level);
  }
  return;
}

/* process messages that originate from vcdinfo. */
static void
vcd_log_handler (vcd_log_level_t level, const char message[])
{
  vcdplayer_t *p_vcdplayer = (vcdplayer_t *)p_vcd_access->p_sys;
  switch (level) {
  case VCD_LOG_DEBUG:
  case VCD_LOG_INFO:
    if (p_vcdplayer->i_debug & INPUT_DBG_VCDINFO)
      msg_Dbg( p_vcd_access, "%s", message);
    break;
  case VCD_LOG_WARN:
    msg_Warn( p_vcd_access, "%s", message);
    break;
  case VCD_LOG_ERROR:
  case VCD_LOG_ASSERT:
    msg_Err( p_vcd_access, "%s", message);
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
    vcdplayer_t *p_vcdplayer= (vcdplayer_t *)p_access->p_sys;
    const int    i_blocks   = p_vcdplayer->i_blocks_per_read;
    block_t     *p_block;
    int          i_read;
    uint8_t     *p_buf;

    i_read = 0;

    dbg_print( (INPUT_DBG_LSN), "lsn: %lu",
               (long unsigned int) p_vcdplayer->i_lsn );

    /* Allocate a block for the reading */
    if( !( p_block = block_New( p_access, i_blocks * M2F2_SECTOR_SIZE ) ) )
    {
        msg_Err( p_access, "cannot get a new block of size: %i",
                 i_blocks * M2F2_SECTOR_SIZE );
        block_Release( p_block );
        return NULL;
    }

    p_buf = (uint8_t *) p_block->p_buffer;
    for ( i_read = 0 ; i_read < i_blocks ; i_read++ )
    {
      vcdplayer_read_status_t read_status = vcdplayer_read(p_access, p_buf);

      p_access->info.i_pos += M2F2_SECTOR_SIZE;

      switch ( read_status ) {
      case READ_END:
        /* End reached. Return NULL to indicated this. */
        /* We also set the postion to the end so the higher level
           (demux?) doesn't try to keep reading. If everything works out
           right this shouldn't have to happen.
         */
#if 0
        if ( p_access->info.i_pos != p_access->info.i_size ) {
          msg_Warn( p_access,
                    "At end but pos (%llu) is not size (%llu). Adjusting.",
                    p_access->info.i_pos, p_access->info.i_size );
          p_access->info.i_pos = p_access->info.i_size;
        }
#endif

        block_Release( p_block );
        return NULL;

      case READ_ERROR:
        /* Some sort of error. Should we increment lsn? to skip block?
        */
        block_Release( p_block );
        return NULL;
      case READ_STILL_FRAME:
        {
          /* FIXME The below should be done in an event thread.
             Until then...
           */
#if 1
          msleep( MILLISECONDS_PER_SEC * *p_buf );
      VCDSetOrigin(p_access, p_vcdplayer->origin_lsn, p_vcdplayer->i_track,
               &(p_vcdplayer->play_item));
          // p_vcd->in_still = false;
          dbg_print(INPUT_DBG_STILL, "still wait time done");
#else
          vcdIntfStillTime(p_vcdplayer->p_intf, *p_buf);
#endif

          block_Release( p_block );
          return NULL;
        }

      default:
      case READ_BLOCK:
        /* Read buffer */
        ;
      }

      p_buf += M2F2_SECTOR_SIZE;
      /* Update seekpoint */
      if ( VCDINFO_ITEM_TYPE_ENTRY == p_vcdplayer->play_item.type )
      {
        unsigned int i_entry = p_vcdplayer->play_item.num+1;
        lsn_t        i_lsn   = vcdinfo_get_entry_lsn(p_vcdplayer->vcd, i_entry);
        if ( p_vcdplayer->i_lsn >= i_lsn && i_lsn != VCDINFO_NULL_LSN )
        {
            const track_t i_track = p_vcdplayer->i_track;

        dbg_print( (INPUT_DBG_LSN|INPUT_DBG_PBC),
               "entry change to %d, current LSN %u >= end %u",
               i_entry, p_vcdplayer->i_lsn, i_lsn);

            p_vcdplayer->play_item.num = i_entry;

            VCDSetOrigin( p_access,  i_lsn, i_track,
                          &(p_vcdplayer->play_item) );
        }
      }
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
      vcdplayer_t         *p_vcdplayer = (vcdplayer_t *)p_vcd_access->p_sys;
      const input_title_t *t = p_vcdplayer->p_title[p_access->info.i_title];
      unsigned int         i_entry = VCDINFO_INVALID_ENTRY;
      int i_seekpoint;

      /* Next sector to read */
      p_access->info.i_pos = i_pos;
      p_vcdplayer->i_lsn = (i_pos / (int64_t) M2F2_SECTOR_SIZE) +
    p_vcdplayer->origin_lsn;

      switch (p_vcdplayer->play_item.type) {
      case VCDINFO_ITEM_TYPE_TRACK:
      case VCDINFO_ITEM_TYPE_ENTRY:
        break ;
      default:
        p_vcdplayer->b_valid_ep = false;
      }

      /* Find entry */
      if( p_vcdplayer->b_valid_ep )
      {
          for( i_entry = 0 ; i_entry < p_vcdplayer->i_entries ; i_entry ++ )
          {
              if( p_vcdplayer->i_lsn < p_vcdplayer->p_entries[i_entry] )
              {
                  VCDUpdateVar( p_access, i_entry, VLC_VAR_SETVALUE,
                                "chapter", _("Entry"), "Setting entry" );
                  break;
              }
          }

          {
              vcdinfo_itemid_t itemid;
              itemid.num  = i_entry;
              itemid.type = VCDINFO_ITEM_TYPE_ENTRY;
              VCDSetOrigin(p_access, p_vcdplayer->i_lsn, p_vcdplayer->i_track,
                           &itemid);
          }
        }

      dbg_print( (INPUT_DBG_CALL|INPUT_DBG_EXT|INPUT_DBG_SEEK),
                 "orig %lu, cur: %lu, offset: %lld, entry %d",
                 (long unsigned int) p_vcdplayer->origin_lsn,
                 (long unsigned int) p_vcdplayer->i_lsn, i_pos,
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
    p_access->info.b_eof = false;
    return VLC_SUCCESS;
 
}

/*****************************************************************************
  VCDEntryPoints: Reads the information about the entry points on the disc
  and initializes area information with that.
  Before calling this track information should have been read in.
 *****************************************************************************/
static bool
VCDEntryPoints( access_t * p_access )
{
  if (!p_access || !p_access->p_sys) return false;
 
  {
    vcdplayer_t       *p_vcdplayer = (vcdplayer_t *) p_access->p_sys;
    const unsigned int i_entries   =
      vcdinfo_get_num_entries(p_vcdplayer->vcd);
    const track_t      i_last_track
      = cdio_get_num_tracks(vcdinfo_get_cd_image(p_vcdplayer->vcd))
      + cdio_get_first_track_num(vcdinfo_get_cd_image(p_vcdplayer->vcd));
    unsigned int i;
 
    if (0 == i_entries) {
      LOG_ERR ("no entires found -- something is wrong" );
      return false;
    }
 
    p_vcdplayer->p_entries  = malloc( sizeof( lsn_t ) * i_entries );
 
    if( p_vcdplayer->p_entries == NULL )
      {
    LOG_ERR ("not enough memory for entry points treatment" );
    return false;
      }
 
    p_vcdplayer->i_entries = i_entries;
 
    for( i = 0 ; i < i_entries ; i++ )
    {
    const track_t i_track = vcdinfo_get_track(p_vcdplayer->vcd, i);
    if( i_track <= i_last_track ) {
      seekpoint_t *s = vlc_seekpoint_New();
      char psz_entry[100];
    
      snprintf(psz_entry, sizeof(psz_entry), "%s %02d", _("Entry"), i );

      p_vcdplayer->p_entries[i] = vcdinfo_get_entry_lsn(p_vcdplayer->vcd, i);
    
      s->psz_name      = strdup(psz_entry);
      s->i_byte_offset =
        (p_vcdplayer->p_entries[i] - vcdinfo_get_track_lsn(p_vcdplayer->vcd, i_track))
        * M2F2_SECTOR_SIZE;
    
      dbg_print( INPUT_DBG_MRL,
             "%s, lsn %d,  byte_offset %ld",
             s->psz_name, p_vcdplayer->p_entries[i],
             (unsigned long int) s->i_byte_offset);
          TAB_APPEND( p_vcdplayer->p_title[i_track-1]->i_seekpoint,
                      p_vcdplayer->p_title[i_track-1]->seekpoint, s );

        } else
          msg_Warn( p_access, "wrong track number found in entry points" );
    }
    p_vcdplayer->b_valid_ep = true;
    return true;
  }
}

/*****************************************************************************
 * VCDSegments: Reads the information about the segments the disc.
 *****************************************************************************/
static bool
VCDSegments( access_t * p_access )
{
    vcdplayer_t   *p_vcdplayer = (vcdplayer_t *) p_access->p_sys;
    unsigned int  i;
    input_title_t *t;

    p_vcdplayer->i_segments = vcdinfo_get_num_segments(p_vcdplayer->vcd);

    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_MRL),
               "Segments: %d", p_vcdplayer->i_segments);

    if ( 0 == p_vcdplayer->i_segments ) return false;

    t = p_vcdplayer->p_title[p_vcdplayer->i_titles] = vlc_input_title_New();
    p_vcdplayer->i_titles++;

    t->i_size    = 0; /* Not sure Segments have a size associated */
    t->psz_name  = strdup(_("Segments"));

    /* We have one additional segment allocated so we can get the size
       by subtracting seg[i+1] - seg[i].
     */
    p_vcdplayer->p_segments =
      malloc( sizeof( lsn_t ) * (p_vcdplayer->i_segments+1) );
    if( p_vcdplayer->p_segments == NULL )
    {
        LOG_ERR ("not enough memory for segment treatment" );
        return false;
    }

    for( i = 0 ; i < p_vcdplayer->i_segments ; i++ )
    {
        char psz_segment[100];
        seekpoint_t *s = vlc_seekpoint_New();
        p_vcdplayer->p_segments[i] = vcdinfo_get_seg_lsn(p_vcdplayer->vcd, i);

        snprintf( psz_segment, sizeof(psz_segment), "%s %02d", _("Segment"),
                  i );

        s->i_byte_offset = 0; /* Not sure what this would mean here */
        s->psz_name  = strdup(psz_segment);
        TAB_APPEND( t->i_seekpoint, t->seekpoint, s );
    }

    p_vcdplayer->p_segments[p_vcdplayer->i_segments] =
      p_vcdplayer->p_segments[p_vcdplayer->i_segments-1]+
      vcdinfo_get_seg_sector_count(p_vcdplayer->vcd,
                                   p_vcdplayer->i_segments-1);

    return true;
}

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
        vcdplayer_t *p_vcdplayer = (vcdplayer_t *) p_access->p_sys;
        track_t      i;

        p_vcdplayer->i_titles = 0;
        for( i = 1 ; i <= p_vcdplayer->i_tracks ; i++ )
        {
            input_title_t *t = p_vcdplayer->p_title[i-1] =
              vlc_input_title_New();
            char psz_track[80];

            snprintf( psz_track, sizeof(psz_track), "%s %02d", _("Track"),
                                                    i );
            t->i_size    = (int64_t) vcdinfo_get_track_size( p_vcdplayer->vcd,
                                 i )
          * M2F2_SECTOR_SIZE / CDIO_CD_FRAMESIZE ;
            t->psz_name  = strdup(psz_track);

        dbg_print( INPUT_DBG_MRL, "track[%d] i_size: %lld", i, t->i_size );

            p_vcdplayer->i_titles++;
        }

      return VLC_SUCCESS;
    }
}

/*****************************************************************************
  VCDLIDs: Reads the LIST IDs from the LOT.
 *****************************************************************************/
static bool
VCDLIDs( access_t * p_access )
{
    vcdplayer_t   *p_vcdplayer = (vcdplayer_t *) p_access->p_sys;
    input_title_t *t;
    unsigned int   i_lid, i_title;

    p_vcdplayer->i_lids = vcdinfo_get_num_LIDs(p_vcdplayer->vcd);
    p_vcdplayer->i_lid  = VCDINFO_INVALID_ENTRY;

    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_MRL),
               "LIDs: %d", p_vcdplayer->i_lids);

    if ( 0 == p_vcdplayer->i_lids ) return false;

    if (vcdinfo_read_psd (p_vcdplayer->vcd)) {

      vcdinfo_visit_lot (p_vcdplayer->vcd, false);

#if FIXED
    /*
       We need to change libvcdinfo to be more robust when there are
       problems reading the extended PSD. Given that area-highlighting and
       selection features in the extended PSD haven't been implemented,
       it's best then to not try to read this at all.
     */
      if (vcdinfo_get_psd_x_size(p_vcdplayer->vcd))
        vcdinfo_visit_lot (p_vcdplayer->vcd, true);
#endif
    }

    /* Set up LIDs Navigation Menu */
    t = vlc_input_title_New();
    t->b_menu = true;
    t->psz_name = strdup( "LIDs" );

    i_title = p_vcdplayer->i_tracks;
    for( i_lid =  1 ; i_lid <=  p_vcdplayer->i_lids ; i_lid++ )
    {
        char psz_lid[100];
        seekpoint_t *s = vlc_seekpoint_New();

        snprintf( psz_lid, sizeof(psz_lid), "%s %02d", _("LID"),
                  i_lid );

        s->i_byte_offset = 0; /*  A lid doesn't have an offset
                                  size associated with it */
        s->psz_name  = strdup(psz_lid);
        TAB_APPEND( t->i_seekpoint, t->seekpoint, s );
    }

#if DYNAMICALLY_ALLOCATED
    TAB_APPEND( p_vcdplayer->i_titles, p_vcdplayer->p_title, t );
#else
    p_vcdplayer->p_title[p_vcdplayer->i_titles] = t;
    p_vcdplayer->i_titles++;
#endif

    return true;
}

/*****************************************************************************
 * VCDParse: parse command line
 *****************************************************************************/
static char *
VCDParse( access_t * p_access, /*out*/ vcdinfo_itemid_t * p_itemid,
          /*out*/ bool *play_single_item )
{
    vcdplayer_t *p_vcdplayer = (vcdplayer_t *)p_access->p_sys;
    char        *psz_parser;
    char        *psz_source;
    char        *psz_next;

    if( config_GetInt( p_access, MODULE_STRING "-PBC" ) ) {
      p_itemid->type = VCDINFO_ITEM_TYPE_LID;
      p_itemid->num = 1;
      *play_single_item = false;
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
            *play_single_item = true;
            break;
          case 'P':
            p_itemid->type = VCDINFO_ITEM_TYPE_LID;
            ++psz_parser;
            *play_single_item = false;
            break;
          case 'S':
            p_itemid->type = VCDINFO_ITEM_TYPE_SEGMENT;
            ++psz_parser;
            *play_single_item = true;
            break;
          case 'T':
            p_itemid->type = VCDINFO_ITEM_TYPE_TRACK;
            ++psz_parser;
            *play_single_item = true;
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
        free( psz_source );
        /* Scan for a CD-ROM drive with a VCD in it. */
        char **cd_drives = cdio_get_devices_with_cap( NULL,
                            ( CDIO_FS_ANAL_SVCD | CDIO_FS_ANAL_CVD
                              |CDIO_FS_ANAL_VIDEOCD | CDIO_FS_UNKNOWN ),
                                                     true );
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
   Sets start origin for subsequent seeks/reads
*/
void
VCDSetOrigin( access_t *p_access, lsn_t i_lsn, track_t i_track,
              const vcdinfo_itemid_t *p_itemid )
{
  vcdplayer_t *p_vcdplayer= (vcdplayer_t *)p_access->p_sys;

  dbg_print( (INPUT_DBG_CALL|INPUT_DBG_LSN),
             "i_lsn: %lu, track: %d", (long unsigned int) i_lsn,
             i_track );

  vcdplayer_set_origin(p_access, i_lsn, i_track, p_itemid);

  switch (p_vcdplayer->play_item.type) {
  case VCDINFO_ITEM_TYPE_ENTRY:
      VCDUpdateVar( p_access, p_itemid->num, VLC_VAR_SETVALUE,
                    "chapter", _("Entry"), "Setting entry/segment");
      p_access->info.i_title     = i_track-1;
      if (p_vcdplayer->b_track_length)
      {
    p_access->info.i_size = p_vcdplayer->p_title[i_track-1]->i_size;
    p_access->info.i_pos  = (int64_t) M2F2_SECTOR_SIZE *
      (vcdinfo_get_track_lsn(p_vcdplayer->vcd, i_track) - i_lsn) ;
      } else {
    p_access->info.i_size = M2F2_SECTOR_SIZE * (int64_t)
      vcdinfo_get_entry_sect_count(p_vcdplayer->vcd, p_itemid->num);
    p_access->info.i_pos = 0;
      }
      dbg_print( (INPUT_DBG_LSN|INPUT_DBG_PBC), "size: %llu, pos: %llu",
         p_access->info.i_size, p_access->info.i_pos );
      p_access->info.i_seekpoint = p_itemid->num;
      break;

  case VCDINFO_ITEM_TYPE_SEGMENT:
      VCDUpdateVar( p_access, p_itemid->num, VLC_VAR_SETVALUE,
                  "chapter", _("Segment"),  "Setting entry/segment");
      /* The last title entry is the for segments (when segments exist
         and they must here. The segment seekpoints are stored after
         the entry seekpoints and (zeroed) lid seekpoints.
      */
      p_access->info.i_title     = p_vcdplayer->i_titles - 1;
      p_access->info.i_size      = 0; /* No seeking on stills, please. */
      p_access->info.i_pos       = 0;
      p_access->info.i_seekpoint = p_vcdplayer->i_entries
        + p_vcdplayer->i_lids + p_itemid->num;
      break;

  case VCDINFO_ITEM_TYPE_TRACK:
      p_access->info.i_title     = i_track-1;
      p_access->info.i_size      = p_vcdplayer->p_title[i_track-1]->i_size;
      p_access->info.i_pos       = 0;
      p_access->info.i_seekpoint = vcdinfo_track_get_entry(p_vcdplayer->vcd,
                                                           i_track);
      break;

  default:
      msg_Warn( p_access, "can't set origin for play type %d",
                p_vcdplayer->play_item.type );
  }

  p_access->info.i_update = INPUT_UPDATE_TITLE|INPUT_UPDATE_SIZE
    |  INPUT_UPDATE_SEEKPOINT;

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
    vcdplayer_t *p_vcdplayer    = (vcdplayer_t *) p_access->p_sys;
    vcdinfo_obj_t *p_vcdobj;
    char  *actual_dev;
    unsigned int i;

    dbg_print(INPUT_DBG_CALL, "called with %s", psz_dev);

    if( !psz_dev ) return NULL;

    actual_dev= ToLocaleDup(psz_dev);
    if ( vcdinfo_open(&p_vcdobj, &actual_dev, DRIVER_UNKNOWN, NULL) !=
         VCDINFO_OPEN_VCD) {
      free(actual_dev);
      return NULL;
    }
    free(actual_dev);

    /*
       Save summary info on tracks, segments and entries...
    */

    if ( 0 < (p_vcdplayer->i_tracks = vcdinfo_get_num_tracks(p_vcdobj)) ) {
      p_vcdplayer->track = (vcdplayer_play_item_info_t *)
        calloc(p_vcdplayer->i_tracks, sizeof(vcdplayer_play_item_info_t));

      for (i=0; i<p_vcdplayer->i_tracks; i++) {
        unsigned int track_num=i+1;
        p_vcdplayer->track[i].size  =
          vcdinfo_get_track_sect_count(p_vcdobj, track_num);
        p_vcdplayer->track[i].start_LSN =
          vcdinfo_get_track_lsn(p_vcdobj, track_num);
      }
    } else
      p_vcdplayer->track = NULL;

    if ( 0 < (p_vcdplayer->i_entries = vcdinfo_get_num_entries(p_vcdobj)) ) {
      p_vcdplayer->entry = (vcdplayer_play_item_info_t *)
        calloc(p_vcdplayer->i_entries, sizeof(vcdplayer_play_item_info_t));

      for (i=0; i<p_vcdplayer->i_entries; i++) {
        p_vcdplayer->entry[i].size =
          vcdinfo_get_entry_sect_count(p_vcdobj, i);
        p_vcdplayer->entry[i].start_LSN = vcdinfo_get_entry_lsn(p_vcdobj, i);
      }
    } else
      p_vcdplayer->entry = NULL;

    if ( 0 < (p_vcdplayer->i_segments = vcdinfo_get_num_segments(p_vcdobj)) ) {
      p_vcdplayer->segment = (vcdplayer_play_item_info_t *)
        calloc(p_vcdplayer->i_segments,  sizeof(vcdplayer_play_item_info_t));

      for (i=0; i<p_vcdplayer->i_segments; i++) {
        p_vcdplayer->segment[i].size =
          vcdinfo_get_seg_sector_count(p_vcdobj, i);
        p_vcdplayer->segment[i].start_LSN = vcdinfo_get_seg_lsn(p_vcdobj, i);
      }
    } else
      p_vcdplayer->segment = NULL;

    return p_vcdobj;
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
    const vcdplayer_t *p_vcdplayer = (vcdplayer_t *)p_vcd_access->p_sys;
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

/*****************************************************************************
  VCDOpen: open VCD.
  read in meta-information about VCD: the number of tracks, segments,
  entries, size and starting information. Then set up state variables so
  that we read/seek starting at the location specified.

  On success we return VLC_SUCCESS, on memory exhausted VLC_ENOMEM,
  and VLC_EGENERIC for some other error.
 *****************************************************************************/
int
VCDOpen ( vlc_object_t *p_this )
{
    access_t         *p_access = (access_t *)p_this;
    vcdplayer_t      *p_vcdplayer;
    char             *psz_source;
    vcdinfo_itemid_t  itemid;
    bool        play_single_item = false;

    p_access->pf_read          = NULL;
    p_access->pf_block         = VCDReadBlock;
    p_access->pf_control       = VCDControl;
    p_access->pf_seek          = VCDSeek;

    p_access->info.i_update    = 0;
    p_access->info.i_size      = 0;
    p_access->info.i_pos       = 0;
    p_access->info.b_eof       = false;
    p_access->info.i_title     = 0;
    p_access->info.i_seekpoint = 0;

    p_vcdplayer = malloc( sizeof(vcdplayer_t) );

    if( p_vcdplayer == NULL )
    {
        LOG_ERR ("out of memory" );
        return VLC_ENOMEM;
    }

    p_vcdplayer->i_debug = config_GetInt( p_this, MODULE_STRING "-debug" );
    p_access->p_sys = (access_sys_t *) p_vcdplayer;

    /* Set where to log errors messages from libcdio. */
    p_vcd_access = p_access;
    cdio_log_set_handler ( cdio_log_handler );
    vcd_log_set_handler ( vcd_log_handler );

    psz_source = VCDParse( p_access, &itemid, &play_single_item );

    if ( NULL == psz_source )
    {
      free( p_vcdplayer );
      return( VLC_EGENERIC );
    }

    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_EXT), "source: %s: mrl: %s",
               psz_source, p_access->psz_path );

    p_vcdplayer->psz_source        = strdup(psz_source);
    p_vcdplayer->i_blocks_per_read = config_GetInt( p_this, MODULE_STRING
                                                    "-blocks-per-read" );
    p_vcdplayer->b_track_length    = config_GetInt( p_this, MODULE_STRING
                                                    "-track-length" );
    p_vcdplayer->in_still          = false;
    p_vcdplayer->play_item.type    = VCDINFO_ITEM_TYPE_NOTFOUND;
    p_vcdplayer->p_input           = vlc_object_find( p_access,
                                                      VLC_OBJECT_INPUT,
                                                      FIND_PARENT );
//    p_vcdplayer->p_meta            = vlc_meta_New();
    p_vcdplayer->p_segments        = NULL;
    p_vcdplayer->p_entries         = NULL;

    /* set up input  */

    if( !(p_vcdplayer->vcd = vcd_Open( p_this, psz_source )) )
    {
        goto err_exit;
    }

    p_vcdplayer->b_svd= (bool) vcdinfo_get_tracksSVD(p_vcdplayer->vcd);;

    /* Get track information. */
    p_vcdplayer->i_tracks = vcdinfo_get_num_tracks(p_vcdplayer->vcd);

    if( p_vcdplayer->i_tracks < 1 || CDIO_INVALID_TRACK == p_vcdplayer->i_tracks ) {
        vcdinfo_close( p_vcdplayer->vcd );
        LOG_ERR ("no movie tracks found" );
        goto err_exit;
    }

    /* Build Navigation Title table for the tracks. */
    VCDTitles( p_access );

    /* Add into the above entry points as "Chapters". */
    if( ! VCDEntryPoints( p_access ) )
    {
        msg_Warn( p_access, "could not read entry points, will not use them" );
        p_vcdplayer->b_valid_ep = false;
    }

    /* Initialize LID info and add that as a menu item */
    if( ! VCDLIDs( p_access ) )
    {
        msg_Warn( p_access, "could not read entry LIDs" );
    }

    /* Do we set PBC (via LID) on? */
    p_vcdplayer->i_lid =
      ( VCDINFO_ITEM_TYPE_LID == itemid.type
        && p_vcdplayer->i_lids > itemid.num )
      ? itemid.num
      :  VCDINFO_INVALID_ENTRY;

    /* Initialize segment information and add that a "Track". */
    VCDSegments( p_access );

    vcdplayer_play( p_access, itemid );

    free( p_access->psz_demux );
    p_access->psz_demux = strdup( "ps" );

#if FIXED
    if (play_single_item)
      VCDFixupPlayList( p_access, p_vcd, psz_source, &itemid,
                        play_single_item );
#endif

#if FIXED
    p_vcdplayer->p_intf = intf_Create( p_access, "vcdx" );
    p_vcdplayer->p_intf->b_block = false;
#endif
    p_vcdplayer->p_access = p_access;

#ifdef FIXED
    intf_RunThread( p_vcdplayer->p_intf );
#endif

    free( psz_source );

    return VLC_SUCCESS;
 err_exit:
    if( p_vcdplayer->p_input ) vlc_object_release( p_vcdplayer->p_input );
    free( psz_source );
    free( p_vcdplayer );
    return VLC_EGENERIC;
}

/*****************************************************************************
 * VCDClose: closes VCD releasing allocated memory.
 *****************************************************************************/
void
VCDClose ( vlc_object_t *p_this )
{
    access_t    *p_access = (access_t *)p_this;
    vcdplayer_t *p_vcdplayer = (vcdplayer_t *)p_access->p_sys;

    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_EXT), "VCDClose" );

    {
      unsigned int i;
      for (i=0 ; i<p_vcdplayer->i_titles; i++)
    if (p_vcdplayer->p_title[i])
      free(p_vcdplayer->p_title[i]->psz_name);
    }
 
    vcdinfo_close( p_vcdplayer->vcd );

    if( p_vcdplayer->p_input ) vlc_object_release( p_vcdplayer->p_input );

    FREENULL( p_vcdplayer->p_entries );
    FREENULL( p_vcdplayer->p_segments );
    FREENULL( p_vcdplayer->psz_source );
    FREENULL( p_vcdplayer->track );
    FREENULL( p_vcdplayer->segment );
    FREENULL( p_vcdplayer->entry );
    FREENULL( p_access->psz_demux );
    FREENULL( p_vcdplayer );
    p_vcd_access    = NULL;
}

/*****************************************************************************
 * Control: The front-end or vlc engine calls here to ether get
 * information such as meta information or plugin capabilities or to
 * issue miscellaneous "set" requests.
 *****************************************************************************/
static int VCDControl( access_t *p_access, int i_query, va_list args )
{
    vcdplayer_t *p_vcdplayer = (vcdplayer_t *)p_access->p_sys;
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
#if 0
            if( p_vcdplayer->p_meta )
            {
                *pp_meta = vlc_meta_Duplicate( p_vcdplayer->p_meta );
                dbg_print( INPUT_DBG_META, "%s", "Meta copied" );
            }
            else
#endif
              msg_Warn( p_access, "tried to copy NULL meta info" );

            return VLC_SUCCESS;
          }
          return VLC_EGENERIC;

        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
        {
            bool *pb_bool = (bool*)va_arg( args, bool* );

            dbg_print( INPUT_DBG_EVENT,
                       "seek/fastseek/pause/can_control_pace" );
            *pb_bool = true;
            return VLC_SUCCESS;
            break;
          }

        /* */
        case ACCESS_GET_MTU:
            pi_int = (int*)va_arg( args, int * );
            *pi_int = (p_vcdplayer->i_blocks_per_read * M2F2_SECTOR_SIZE);
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
              + strlen(p_vcdplayer->psz_source) + sizeof("@E999")+3;
            input_title_t ***ppp_title
              = (input_title_t***)va_arg( args, input_title_t*** );
            char *psz_mrl = malloc( psz_mrl_max );
            unsigned int i;

            pi_int    = (int*)va_arg( args, int* );

            dbg_print( INPUT_DBG_EVENT, "GET TITLE: i_titles %d",
                       p_vcdplayer->i_titles );

            if( psz_mrl == NULL ) {
               msg_Warn( p_access, "out of memory" );
            } else {
               snprintf(psz_mrl, psz_mrl_max, "%s%s",
                        VCD_MRL_PREFIX, p_vcdplayer->psz_source);
               VCDMetaInfo( p_access, psz_mrl );
               free(psz_mrl);
            }

            /* Duplicate title info */
            if( p_vcdplayer->i_titles == 0 )
            {
                *pi_int = 0; ppp_title = NULL;
                return VLC_SUCCESS;
            }
            *pi_int = p_vcdplayer->i_titles;
            *ppp_title = malloc( sizeof( input_title_t **)
                                         * p_vcdplayer->i_titles );

            if (!*ppp_title) return VLC_ENOMEM;

            for( i = 0; i < p_vcdplayer->i_titles; i++ )
            {
                if ( p_vcdplayer->p_title[i] )
                  (*ppp_title)[i] =
                    vlc_input_title_Duplicate( p_vcdplayer->p_title[i] );
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
                  vcdinfo_track_get_entry( p_vcdplayer->vcd, i_track);

        if( i < p_vcdplayer->i_tracks )
        {
            /* FIXME! For now we are assuming titles are only
               tracks and that track == title+1 */
            itemid.num = i_track;
            itemid.type = VCDINFO_ITEM_TYPE_TRACK;
        }
        else
        {
            /* FIXME! i_tracks+2 are Segments, but we need to
               be able to figure out which segment of that.
                       i_tracks+1 is either Segments (if no LIDs) or
               LIDs otherwise. Again need a way to get the LID
               number. */
            msg_Warn( p_access,
                    "Trying to set track (%u) beyond end of last track (%u).",
                  i+1, p_vcdplayer->i_tracks );
            return VLC_EGENERIC;
        }
        
                VCDSetOrigin(p_access,
                     vcdinfo_get_entry_lsn(p_vcdplayer->vcd, i_entry),
                             i_track, &itemid );
            }
            break;

        case ACCESS_SET_SEEKPOINT:
        {
            input_title_t *t = p_vcdplayer->p_title[p_access->info.i_title];
            unsigned int i = (unsigned int)va_arg( args, unsigned int );

            dbg_print( INPUT_DBG_EVENT, "set seekpoint %d", i );
            if( t->i_seekpoint > 0 )
            {
                track_t i_track = p_access->info.i_title+1;
                lsn_t lsn;

                /* FIXME! For now we are assuming titles are only
                 tracks and that track == title+1 and we the play
                 item is entries (not tracks or lids).
                 We need to generalize all of this.
                */

                if (i < p_vcdplayer->i_entries)
                {
                    p_vcdplayer->play_item.num  = i;
                    p_vcdplayer->play_item.type = VCDINFO_ITEM_TYPE_ENTRY;
                    lsn = vcdinfo_get_entry_lsn(p_vcdplayer->vcd, i);
                } else if ( i < p_vcdplayer->i_entries + p_vcdplayer->i_lids )
                {
                    p_vcdplayer->play_item.num  = i
                      = i - p_vcdplayer->i_entries;
                    p_vcdplayer->play_item.type = VCDINFO_ITEM_TYPE_LID;
                    lsn = 0;
                } else
                {
                    p_vcdplayer->play_item.num  = i
                      = i - p_vcdplayer->i_entries - p_vcdplayer->i_lids;
                    p_vcdplayer->play_item.type = VCDINFO_ITEM_TYPE_SEGMENT;
                    lsn = vcdinfo_get_seg_lsn(p_vcdplayer->vcd, i);
                }

                VCDSetOrigin( p_access,
                              vcdinfo_get_entry_lsn(p_vcdplayer->vcd, i),
                              i_track, &(p_vcdplayer->play_item) );
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
