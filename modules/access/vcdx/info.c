/*****************************************************************************
 * info.c : CD digital audio input information routines
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id: info.c 8845 2004-09-29 09:00:41Z rocky $
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

#include <vlc/vlc.h>
#include <vlc/input.h>

#include "vcd.h"
#include <vlc_playlist.h> 
#include "vcdplayer.h"
#include "vlc_keys.h"

#include <cdio/cdio.h>
#include <cdio/cd_types.h>
#include <cdio/logging.h>
#include <cdio/util.h>
#include <libvcd/info.h>
#include <libvcd/logging.h>

static char *VCDFormatStr(const access_t *p_access, access_vcd_data_t *p_vcd,
			  const char format_str[], const char *mrl,
			  const vcdinfo_itemid_t *itemid);

static inline void
MetaInfoAddStr(access_t *p_access, char *p_cat,
               char *title, const char *str)
{
  access_vcd_data_t *p_vcd = (access_vcd_data_t *) p_access->p_sys;
  if ( str ) {
    dbg_print( INPUT_DBG_META, "field: %s: %s", title, str);
    input_Control( p_vcd->p_input, INPUT_ADD_INFO, p_cat, title, "%s", str);
  }
}


static inline void
MetaInfoAddNum(access_t *p_access, char *psz_cat, char *title, int num)
{
  access_vcd_data_t *p_vcd = (access_vcd_data_t *) p_access->p_sys;
  dbg_print( INPUT_DBG_META, "field %s: %d", title, num);
  input_Control( p_vcd->p_input, INPUT_ADD_INFO, psz_cat, title, "%d", num );
}

#define addstr(title, str) \
  MetaInfoAddStr( p_access, psz_cat, title, str );

#define addnum(title, num) \
  MetaInfoAddNum( p_access, psz_cat, title, num );

void 
VCDMetaInfo( access_t *p_access, /*const*/ char *psz_mrl )
{
  access_vcd_data_t *p_vcd = (access_vcd_data_t *) p_access->p_sys;
  unsigned int i_entries = vcdinfo_get_num_entries(p_vcd->vcd);
  unsigned int last_entry = 0;
  char *psz_cat;
  track_t i_track;

  psz_cat = _("General");

  addstr( _("VCD Format"),  vcdinfo_get_format_version_str(p_vcd->vcd) );
  addstr( _("Album"),       vcdinfo_get_album_id(p_vcd->vcd));
  addstr( _("Application"), vcdinfo_get_application_id(p_vcd->vcd) );
  addstr( _("Preparer"),    vcdinfo_get_preparer_id(p_vcd->vcd) );
  addnum( _("Vol #"),       vcdinfo_get_volume_num(p_vcd->vcd) );
  addnum( _("Vol max #"),   vcdinfo_get_volume_count(p_vcd->vcd) );
  addstr( _("Volume Set"),  vcdinfo_get_volumeset_id(p_vcd->vcd) );
  addstr( _("Volume"),      vcdinfo_get_volume_id(p_vcd->vcd) );
  addstr( _("Publisher"),   vcdinfo_get_publisher_id(p_vcd->vcd) );
  addstr( _("System Id"),   vcdinfo_get_system_id(p_vcd->vcd) );
  addnum( "LIDs",           vcdinfo_get_num_LIDs(p_vcd->vcd) );
  addnum( _("Entries"),     vcdinfo_get_num_entries(p_vcd->vcd) );
  addnum( _("Segments"),    vcdinfo_get_num_segments(p_vcd->vcd) );
  addnum( _("Tracks"),      vcdinfo_get_num_tracks(p_vcd->vcd) );

  /* Spit out track information. Could also include MSF info.
     Also build title table.
   */

#define TITLE_MAX 30
  for( i_track = 1 ; i_track < p_vcd->i_tracks ; i_track++ ) {
    unsigned int audio_type = vcdinfo_get_track_audio_type(p_vcd->vcd, 
							   i_track);
    uint32_t i_secsize = vcdinfo_get_track_sect_count(p_vcd->vcd, i_track);

    if (p_vcd->b_svd) {
      addnum(_("Audio Channels"),  
	     vcdinfo_audio_type_num_channels(p_vcd->vcd, audio_type) );
    }

    addnum(_("First Entry Point"), last_entry );
    for ( ; last_entry < i_entries 
	    && vcdinfo_get_track(p_vcd->vcd, last_entry) == i_track;
	  last_entry++ ) ;
    addnum(_("Last Entry Point"), last_entry-1 );
    addnum(_("Track size (in sectors)"), i_secsize );
  }

  if ( CDIO_INVALID_TRACK != i_track )
  { 
    char *psz_name = 
      VCDFormatStr( p_access, p_vcd,
		    config_GetPsz( p_access, MODULE_STRING "-title-format" ),
		    psz_mrl, &(p_vcd->play_item) );
    
    input_Control( p_vcd->p_input, INPUT_SET_NAME, psz_name );
  }

}

#define add_format_str_info(val)			       \
  {							       \
    const char *str = val;				       \
    unsigned int len;					       \
    if (val != NULL) {					       \
      len=strlen(str);					       \
      if (len != 0) {					       \
        strncat(tp, str, TEMP_STR_LEN-(tp-temp_str));	       \
        tp += len;					       \
      }                                                        \
      saw_control_prefix = VLC_FALSE;			       \
    }							       \
  }

#define add_format_num_info( val, fmt )			       \
  {							       \
    char num_str[10];					       \
    unsigned int len;					       \
    sprintf(num_str, fmt, val);                                \
    len = strlen(num_str);				       \
    if( len != 0 )                                             \
    {					                       \
      strncat(tp, num_str, TEMP_STR_LEN-(tp-temp_str));        \
      tp += len;					       \
    }							       \
    saw_control_prefix = VLC_FALSE;                                \
  }

/*!
   Take a format string and expand escape sequences, that is sequences that
   begin with %, with information from the current VCD.
   The expanded string is returned. Here is a list of escape sequences:

   %A : The album information
   %C : The VCD volume count - the number of CD's in the collection.
   %c : The VCD volume num - the number of the CD in the collection.
   %F : The VCD Format, e.g. VCD 1.0, VCD 1.1, VCD 2.0, or SVCD
   %I : The current entry/segment/playback type, e.g. ENTRY, TRACK, SEGMENT...
   %L : The playlist ID prefixed with " LID" if it exists
   %M : MRL
   %N : The current number of the %I - a decimal number
   %P : The publisher ID
   %p : The preparer ID
   %S : If we are in a segment (menu), the kind of segment
   %T : The track number
   %V : The volume set ID
   %v : The volume ID
       A number between 1 and the volume count.
   %% : a %
*/
char *
VCDFormatStr(const access_t *p_access, access_vcd_data_t *p_vcd,
             const char format_str[], const char *mrl,
             const vcdinfo_itemid_t *itemid)
{
#define TEMP_STR_SIZE 256
#define TEMP_STR_LEN (TEMP_STR_SIZE-1)
  static char    temp_str[TEMP_STR_SIZE];
  size_t         i;
  char *         tp = temp_str;
  vlc_bool_t     saw_control_prefix = VLC_FALSE;
  size_t         format_len = strlen(format_str);

  memset(temp_str, 0, TEMP_STR_SIZE);

  for (i=0; i<format_len; i++) {

    if (!saw_control_prefix && format_str[i] != '%') {
      *tp++ = format_str[i];
      saw_control_prefix = VLC_FALSE;
      continue;
    }

    switch(format_str[i]) {
    case '%':
      if (saw_control_prefix) {
        *tp++ = '%';
      }
      saw_control_prefix = !saw_control_prefix;
      break;
    case 'A':
      add_format_str_info(vcdinfo_strip_trail(vcdinfo_get_album_id(p_vcd->vcd),
                                              MAX_ALBUM_LEN));
      break;

    case 'c':
      add_format_num_info(vcdinfo_get_volume_num(p_vcd->vcd), "%d");
      break;

    case 'C':
      add_format_num_info(vcdinfo_get_volume_count(p_vcd->vcd), "%d");
      break;

    case 'F':
      add_format_str_info(vcdinfo_get_format_version_str(p_vcd->vcd));
      break;

    case 'I':
      {
        switch (itemid->type) {
        case VCDINFO_ITEM_TYPE_TRACK:
          strncat(tp, _("Track"), TEMP_STR_LEN-(tp-temp_str));
          tp += strlen(_("Track"));
        break;
        case VCDINFO_ITEM_TYPE_ENTRY:
          strncat(tp, _("Entry"), TEMP_STR_LEN-(tp-temp_str));
          tp += strlen(_("Entry"));
          break;
        case VCDINFO_ITEM_TYPE_SEGMENT:
          strncat(tp, _("Segment"), TEMP_STR_LEN-(tp-temp_str));
          tp += strlen(_("Segment"));
          break;
        case VCDINFO_ITEM_TYPE_LID:
          strncat(tp, _("List ID"), TEMP_STR_LEN-(tp-temp_str));
          tp += strlen(_("List ID"));
          break;
        case VCDINFO_ITEM_TYPE_SPAREID2:
          strncat(tp, _("Navigation"), TEMP_STR_LEN-(tp-temp_str));
          tp += strlen(_("Navigation"));
          break;
        default:
          /* What to do? */
          ;
        }
        saw_control_prefix = VLC_FALSE;
      }
      break;

    case 'L':
      if (vcdplayer_pbc_is_on(p_vcd)) {
        char num_str[40];
        sprintf(num_str, "%s %d", _("List ID"), p_vcd->i_lid);
        strncat(tp, num_str, TEMP_STR_LEN-(tp-temp_str));
        tp += strlen(num_str);
      }
      saw_control_prefix = VLC_FALSE;
      break;

    case 'M':
      add_format_str_info(mrl);
      break;

    case 'N':
      add_format_num_info(itemid->num, "%d");
      break;

    case 'p':
      add_format_str_info(vcdinfo_get_preparer_id(p_vcd->vcd));
      break;

    case 'P':
      add_format_str_info(vcdinfo_get_publisher_id(p_vcd->vcd));
      break;

    case 'S':
      if ( VCDINFO_ITEM_TYPE_SEGMENT==itemid->type ) {
        char seg_type_str[10];

        sprintf(seg_type_str, " %s",
                vcdinfo_video_type2str(p_vcd->vcd, itemid->num));
        strncat(tp, seg_type_str, TEMP_STR_LEN-(tp-temp_str));
        tp += strlen(seg_type_str);
      }
      saw_control_prefix = VLC_FALSE;
      break;

    case 'T':
      add_format_num_info(p_vcd->i_track, "%d");
      break;

    case 'V':
      add_format_str_info(vcdinfo_get_volumeset_id(p_vcd->vcd));
      break;

    case 'v':
      add_format_str_info(vcdinfo_get_volume_id(p_vcd->vcd));
      break;

    default:
      *tp++ = '%';
      *tp++ = format_str[i];
      saw_control_prefix = VLC_FALSE;
    }
  }
  return strdup(temp_str);
}

static void
VCDCreatePlayListItem(const access_t *p_access,
                      access_vcd_data_t *p_vcd,
                      playlist_t *p_playlist,
                      const vcdinfo_itemid_t *itemid,
                      char *psz_mrl, int psz_mrl_max,
                      const char *psz_source, int playlist_operation,
                      int i_pos)
{
  char *p_author;
  char *p_title;
  char c_type;

  switch(itemid->type) {
  case VCDINFO_ITEM_TYPE_TRACK:
    c_type='T';
    break;
  case VCDINFO_ITEM_TYPE_SEGMENT:
    c_type='S';
    break;
  case VCDINFO_ITEM_TYPE_LID:
    c_type='P';
    break;
  case VCDINFO_ITEM_TYPE_ENTRY:
    c_type='E';
    break;
  default:
    c_type='?';
    break;
  }

  snprintf(psz_mrl, psz_mrl_max, "%s%s@%c%u", VCD_MRL_PREFIX, psz_source,
           c_type, itemid->num);

  p_title =
    VCDFormatStr( p_access, p_vcd,
		  config_GetPsz( p_access, MODULE_STRING "-title-format" ),
		  psz_mrl, itemid );
  
  playlist_Add( p_playlist, psz_mrl, p_title, playlist_operation, i_pos );

  p_author =
    VCDFormatStr( p_access, p_vcd,
		  config_GetPsz( p_access, MODULE_STRING "-author-format" ),
		  psz_mrl, itemid );

  if( i_pos == PLAYLIST_END ) i_pos = p_playlist->i_size - 1;
  playlist_AddInfo(p_playlist, i_pos, _("General"), _("Author"), "%s",
		   p_author);
}

int
VCDFixupPlayList( access_t *p_access, access_vcd_data_t *p_vcd,
                  const char *psz_source, vcdinfo_itemid_t *itemid,
                  vlc_bool_t b_single_item )
{
  unsigned int i;
  playlist_t * p_playlist;
  char       * psz_mrl;
  unsigned int psz_mrl_max = strlen(VCD_MRL_PREFIX) + strlen(psz_source) +
    strlen("@T") + strlen("100") + 1;

  psz_mrl = malloc( psz_mrl_max );

  if( psz_mrl == NULL )
    {
      msg_Warn( p_access, "out of memory" );
      return -1;
    }

  p_playlist = (playlist_t *) vlc_object_find( p_access, VLC_OBJECT_PLAYLIST,
                                               FIND_ANYWHERE );
  if( !p_playlist )
    {
      msg_Warn( p_access, "can't find playlist" );
      free(psz_mrl);
      return -1;
    }

  {
    vcdinfo_itemid_t list_itemid;
    list_itemid.type=VCDINFO_ITEM_TYPE_ENTRY;

    playlist_LockDelete( p_playlist, p_playlist->i_index);

    for( i = 0 ; i < p_vcd->i_entries ; i++ )
      {
        list_itemid.num=i;
        VCDCreatePlayListItem(p_access, p_vcd, p_playlist, &list_itemid,
                              psz_mrl, psz_mrl_max, psz_source,
                              PLAYLIST_APPEND, PLAYLIST_END);
      }

#if LOOKED_OVER
    playlist_Command( p_playlist, PLAYLIST_GOTO, 0 );
#endif

  }

  vlc_object_release( p_playlist );
  free(psz_mrl);
  return 0;
}

