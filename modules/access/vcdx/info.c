/*****************************************************************************
 * info.c : CD digital audio input information routines
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_input.h>
#include <vlc_access.h>
#include "vcd.h"
#include <vlc_keys.h>
#include "info.h"

#include <cdio/cdio.h>
#include <cdio/cd_types.h>
#include <cdio/logging.h>
#include <cdio/util.h>
#include <libvcd/info.h>
#include <libvcd/logging.h>

static inline void
MetaInfoAddStr(access_t *p_access, char *psz_cat,
               char *title, const char *psz)
{
  vcdplayer_t *p_vcdplayer = (vcdplayer_t *) p_access->p_sys;
  if ( psz ) {
    dbg_print( INPUT_DBG_META, "cat %s, field: %s: %s", psz_cat, title, psz);
    input_Control( p_vcdplayer->p_input, INPUT_ADD_INFO, psz_cat, title, "%s",
           psz);
  }
}


static inline void
MetaInfoAddNum(access_t *p_access, char *psz_cat, char *title, int num)
{
  vcdplayer_t *p_vcdplayer = (vcdplayer_t *) p_access->p_sys;
  dbg_print( INPUT_DBG_META, "cat %s, field %s: %d", psz_cat,  title, num);
  input_Control( p_vcdplayer->p_input, INPUT_ADD_INFO, psz_cat, title,
         "%d", num );
}

static inline void
MetaInfoAddHex(access_t *p_access, char *psz_cat, char *title, int hex)
{
  vcdplayer_t *p_vcdplayer = (vcdplayer_t *) p_access->p_sys;
  dbg_print( INPUT_DBG_META, "cat %s, field %s: %d", psz_cat, title, hex);
  input_Control( p_vcdplayer->p_input, INPUT_ADD_INFO, psz_cat, title,
         "%x", hex );
}

#define addstr(title, str) \
  MetaInfoAddStr( p_access, psz_cat, title, str );

#define addnum(title, num) \
  MetaInfoAddNum( p_access, psz_cat, title, num );

#define addhex(title, hex) \
  MetaInfoAddHex( p_access, psz_cat, title, hex );

void
VCDMetaInfo( access_t *p_access, /*const*/ char *psz_mrl )
{
  vcdplayer_t *p_vcdplayer = (vcdplayer_t *) p_access->p_sys;
  unsigned int i_entries = vcdinfo_get_num_entries(p_vcdplayer->vcd);
  unsigned int last_entry = 0;
  char *psz_cat;
  track_t i_track;

  psz_cat = _("Disc");

  addstr( _("VCD Format"),  vcdinfo_get_format_version_str(p_vcdplayer->vcd) );
  addstr( _("Album"),       vcdinfo_get_album_id(p_vcdplayer->vcd));
  addstr( _("Application"), vcdinfo_get_application_id(p_vcdplayer->vcd) );
  addstr( _("Preparer"),    vcdinfo_get_preparer_id(p_vcdplayer->vcd) );
  addnum( _("Vol #"),       vcdinfo_get_volume_num(p_vcdplayer->vcd) );
  addnum( _("Vol max #"),   vcdinfo_get_volume_count(p_vcdplayer->vcd) );
  addstr( _("Volume Set"),  vcdinfo_get_volumeset_id(p_vcdplayer->vcd) );
  addstr( _("Volume"),      vcdinfo_get_volume_id(p_vcdplayer->vcd) );
  addstr( _("Publisher"),   vcdinfo_get_publisher_id(p_vcdplayer->vcd) );
  addstr( _("System Id"),   vcdinfo_get_system_id(p_vcdplayer->vcd) );
  addnum( "LIDs",           vcdinfo_get_num_LIDs(p_vcdplayer->vcd) );
  addnum( _("Entries"),     vcdinfo_get_num_entries(p_vcdplayer->vcd) );
  addnum( _("Segments"),    vcdinfo_get_num_segments(p_vcdplayer->vcd) );
  addnum( _("Tracks"),      vcdinfo_get_num_tracks(p_vcdplayer->vcd) );

  /* Spit out track information. Could also include MSF info.
     Also build title table.
   */

#define TITLE_MAX 30
  for( i_track = 1 ; i_track < p_vcdplayer->i_tracks ; i_track++ ) {
    char psz_cat[20];
    unsigned int audio_type = vcdinfo_get_track_audio_type(p_vcdplayer->vcd,
                               i_track);
    uint32_t i_secsize = vcdinfo_get_track_sect_count(p_vcdplayer->vcd, i_track);

    snprintf(psz_cat, sizeof(psz_cat), "Track %d", i_track);
    if (p_vcdplayer->b_svd) {
      addnum(_("Audio Channels"),
         vcdinfo_audio_type_num_channels(p_vcdplayer->vcd, audio_type) );
    }

    addnum(_("First Entry Point"), last_entry );
    for ( ; last_entry < i_entries
        && vcdinfo_get_track(p_vcdplayer->vcd, last_entry) == i_track;
      last_entry++ ) ;
    addnum(_("Last Entry Point"), last_entry-1 );
    addnum(_("Track size (in sectors)"), i_secsize );
  }
 
  {
    lid_t i_lid;
    for( i_lid = 1 ; i_lid <= p_vcdplayer->i_lids ; i_lid++ ) {
      PsdListDescriptor_t pxd;
      char psz_cat[20];
      snprintf(psz_cat, sizeof(psz_cat), "LID %d", i_lid);
      if (vcdinfo_lid_get_pxd(p_vcdplayer->vcd, &pxd, i_lid)) {
    switch (pxd.descriptor_type) {
    case PSD_TYPE_END_LIST:
      addstr(_("type"), _("end"));
      break;
    case PSD_TYPE_PLAY_LIST:
      addstr(_("type"), _("play list"));
      addnum("items",     vcdinf_pld_get_noi(pxd.pld));
      addhex("next",      vcdinf_pld_get_next_offset(pxd.pld));
      addhex("previous",  vcdinf_pld_get_prev_offset(pxd.pld));
      addhex("return",    vcdinf_pld_get_return_offset(pxd.pld));
      addnum("wait time", vcdinf_get_wait_time(pxd.pld));
      break;
    case PSD_TYPE_SELECTION_LIST:
    case PSD_TYPE_EXT_SELECTION_LIST:
      addstr(_("type"),
         PSD_TYPE_SELECTION_LIST == pxd.descriptor_type
         ? _("extended selection list")
         : _("selection list")
         );
      addhex("default",          vcdinf_psd_get_default_offset(pxd.psd));
      addhex("loop count",       vcdinf_get_loop_count(pxd.psd));
      addhex("next",             vcdinf_psd_get_next_offset(pxd.psd));
      addhex("previous",         vcdinf_psd_get_prev_offset(pxd.psd));
      addhex("return",           vcdinf_psd_get_return_offset(pxd.psd));
      addhex("rejected",         vcdinf_psd_get_lid_rejected(pxd.psd));
      addhex("time-out offset",  vcdinf_get_timeout_offset(pxd.psd));
      addnum("time-out time",    vcdinf_get_timeout_time(pxd.psd));
      break;
    default:
      addstr(_("type"), _("unknown type"));
      break;
    }
      }
    }
  }

  if ( CDIO_INVALID_TRACK != i_track )
  {
    char* psz_title_format = config_GetPsz( p_access, MODULE_STRING "-title-format" );
    char *psz_name =
      VCDFormatStr( p_access, p_vcdplayer, psz_title_format, psz_mrl,
                    &(p_vcdplayer->play_item) );
    free( psz_title_format );
 
    input_Control( p_vcdplayer->p_input, INPUT_SET_NAME, psz_name );
  }

}

#define add_format_str_info(val)                   \
  {                                   \
    const char *str = strdup(val);                   \
    unsigned int len;                           \
    if (val != NULL) {                           \
      len=strlen(str);                           \
      if (len != 0) {                           \
        strncat(tp, str, TEMP_STR_LEN-(tp-temp_str));           \
        tp += len;                           \
      }                                                        \
      saw_control_prefix = false;                   \
    }                                   \
  }

#define add_format_num_info( val, fmt )                   \
  {                                   \
    char num_str[10];                           \
    unsigned int len;                           \
    sprintf(num_str, fmt, val);                                \
    len = strlen(num_str);                       \
    if( len != 0 )                                             \
    {                                           \
      strncat(tp, num_str, TEMP_STR_LEN-(tp-temp_str));        \
      tp += len;                           \
    }                                   \
    saw_control_prefix = false;                                \
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
VCDFormatStr(const access_t *p_access, vcdplayer_t *p_vcdplayer,
             const char format_str[], const char *mrl,
             const vcdinfo_itemid_t *itemid)
{
#define TEMP_STR_SIZE 256
#define TEMP_STR_LEN (TEMP_STR_SIZE-1)
  char           temp_str[TEMP_STR_SIZE];
  size_t         i;
  char *         tp = temp_str;
  bool     saw_control_prefix = false;
  size_t         format_len = strlen(format_str);

  memset(temp_str, 0, TEMP_STR_SIZE);

  for (i=0; i<format_len; i++) {

    if (!saw_control_prefix && format_str[i] != '%') {
      *tp++ = format_str[i];
      saw_control_prefix = false;
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
      add_format_str_info(vcdinfo_strip_trail(vcdinfo_get_album_id(p_vcdplayer->vcd),
                                              MAX_ALBUM_LEN));
      break;

    case 'c':
      add_format_num_info(vcdinfo_get_volume_num(p_vcdplayer->vcd), "%d");
      break;

    case 'C':
      add_format_num_info(vcdinfo_get_volume_count(p_vcdplayer->vcd), "%d");
      break;

    case 'F':
      add_format_str_info(vcdinfo_get_format_version_str(p_vcdplayer->vcd));
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
        saw_control_prefix = false;
      }
      break;

    case 'L':
      if (vcdplayer_pbc_is_on(p_vcdplayer)) {
        char num_str[40];
        sprintf(num_str, "%s %d", _("List ID"), p_vcdplayer->i_lid);
        strncat(tp, num_str, TEMP_STR_LEN-(tp-temp_str));
        tp += strlen(num_str);
      }
      saw_control_prefix = false;
      break;

    case 'M':
      add_format_str_info(mrl);
      break;

    case 'N':
      add_format_num_info(itemid->num, "%d");
      break;

    case 'p':
      add_format_str_info(vcdinfo_get_preparer_id(p_vcdplayer->vcd));
      break;

    case 'P':
      add_format_str_info(vcdinfo_get_publisher_id(p_vcdplayer->vcd));
      break;

    case 'S':
      if ( VCDINFO_ITEM_TYPE_SEGMENT==itemid->type ) {
        char seg_type_str[30];

        sprintf(seg_type_str, " %s",
                vcdinfo_video_type2str(p_vcdplayer->vcd, itemid->num));
        strncat(tp, seg_type_str, TEMP_STR_LEN-(tp-temp_str));
        tp += strlen(seg_type_str);
      }
      saw_control_prefix = false;
      break;

    case 'T':
      add_format_num_info(p_vcdplayer->i_track, "%d");
      break;

    case 'V':
      add_format_str_info(vcdinfo_get_volumeset_id(p_vcdplayer->vcd));
      break;

    case 'v':
      add_format_str_info(vcdinfo_get_volume_id(p_vcdplayer->vcd));
      break;

    default:
      *tp++ = '%';
      *tp++ = format_str[i];
      saw_control_prefix = false;
    }
  }
  return strdup(temp_str);
}

void
VCDUpdateTitle( access_t *p_access )
{

    vcdplayer_t *p_vcdplayer= (vcdplayer_t *)p_access->p_sys;

    unsigned int psz_mrl_max = strlen(VCD_MRL_PREFIX)
      + strlen(p_vcdplayer->psz_source) + sizeof("@E999")+3;
    char *psz_mrl = malloc( psz_mrl_max );

    if( psz_mrl )
    {
        char *psz_name;
        char* psz_title_format = config_GetPsz( p_access, MODULE_STRING "-title-format" );
        snprintf( psz_mrl, psz_mrl_max, "%s%s",
                  VCD_MRL_PREFIX, p_vcdplayer->psz_source );
        psz_name = VCDFormatStr( p_access, p_vcdplayer, psz_title_format, psz_mrl,
                                 &(p_vcdplayer->play_item) );
        input_Control( p_vcdplayer->p_input, INPUT_SET_NAME, psz_name );
        free( psz_title_format );
        free(psz_mrl);
    }
}

