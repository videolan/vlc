/*****************************************************************************
 * info.c : CD digital audio input information routines
 *****************************************************************************
 * Copyright (C) 2004 VLC authors and VideoLAN
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_input.h>
#include <vlc_access.h>
#include "vcd.h"
#include "info.h"

#include <cdio/cdio.h>
#include <cdio/cd_types.h>
#include <cdio/logging.h>
#include <cdio/util.h>
#include <libvcd/info.h>
#include <libvcd/logging.h>

static char *
VCDFormatStr(vcdplayer_t *p_vcdplayer,
             const char *format_str, const char *mrl,
             const vcdinfo_itemid_t *itemid);
void
VCDMetaInfo( access_t *p_access, /*const*/ char *psz_mrl )
{
  vcdplayer_t    *p_vcdplayer  = (vcdplayer_t *) p_access->p_sys;
  input_thread_t *p_input = p_vcdplayer->p_input;
  vcdinfo_obj_t  *p_vcdev = p_vcdplayer->vcd;

  size_t i_entries = vcdinfo_get_num_entries(p_vcdev);
  size_t last_entry = 0;
  char *psz_cat = _("Disc");

  track_t i_track;

# define addstr(t,v) input_Control(p_input,INPUT_ADD_INFO,psz_cat,t,"%s",v)
# define addnum(t,v) input_Control(p_input,INPUT_ADD_INFO,psz_cat,t,"%d",v)
# define addhex(t,v) input_Control(p_input,INPUT_ADD_INFO,psz_cat,t,"%x",v)

  addstr(_("VCD Format"),  vcdinfo_get_format_version_str(p_vcdev));
  addstr(_("Album"),       vcdinfo_get_album_id          (p_vcdev));
  addstr(_("Application"), vcdinfo_get_application_id    (p_vcdev));
  addstr(_("Preparer"),    vcdinfo_get_preparer_id       (p_vcdev));
  addnum(_("Vol #"),       vcdinfo_get_volume_num        (p_vcdev));
  addnum(_("Vol max #"),   vcdinfo_get_volume_count      (p_vcdev));
  addstr(_("Volume Set"),  vcdinfo_get_volumeset_id      (p_vcdev));
  addstr(_("Volume"),      vcdinfo_get_volume_id         (p_vcdev));
  addstr(_("Publisher"),   vcdinfo_get_publisher_id      (p_vcdev));
  addstr(_("System Id"),   vcdinfo_get_system_id         (p_vcdev));
  addnum("LIDs",           vcdinfo_get_num_LIDs          (p_vcdev));
  addnum(_("Entries"),     vcdinfo_get_num_entries       (p_vcdev));
  addnum(_("Segments"),    vcdinfo_get_num_segments      (p_vcdev));
  addnum(_("Tracks"),      vcdinfo_get_num_tracks        (p_vcdev));

  /* Spit out track information. Could also include MSF info.
     Also build title table.
   */

  for( i_track = 1 ; i_track < p_vcdplayer->i_tracks ; i_track++ ) {
    unsigned int audio_type = vcdinfo_get_track_audio_type(p_vcdev, i_track);
    uint32_t i_secsize = vcdinfo_get_track_sect_count(p_vcdev, i_track);

    if (p_vcdplayer->b_svd) {
      addnum(_("Audio Channels"),
             vcdinfo_audio_type_num_channels(p_vcdev, audio_type) );
    }

    addnum(_("First Entry Point"), 0 );

    for ( last_entry = 0 ; last_entry < i_entries
        && vcdinfo_get_track(p_vcdev, last_entry) == i_track; last_entry++ ) ;

    addnum(_("Last Entry Point"), last_entry-1 );
    addnum(_("Track size (in sectors)"), i_secsize );
  }
 
  {
    lid_t i_lid;
    for( i_lid = 1 ; i_lid <= p_vcdplayer->i_lids ; i_lid++ ) {
      PsdListDescriptor_t pxd;
      if (vcdinfo_lid_get_pxd(p_vcdev, &pxd, i_lid)) {
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
      addstr(_("type"), PSD_TYPE_SELECTION_LIST == pxd.descriptor_type
             ? _("extended selection list") : _("selection list") );
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
# undef  addstr
# undef  addnum
# undef  addhex

  if ( CDIO_INVALID_TRACK != i_track )
  {
    char *psz_tfmt = var_InheritString( p_access, MODULE_STRING "-title-format" );
    char *psz_name = VCDFormatStr( p_vcdplayer, psz_tfmt, psz_mrl,
                                                  &(p_vcdplayer->play_item) );
    free( psz_tfmt );
 
    input_Control( p_input, INPUT_SET_NAME, psz_name );
    free( psz_name );
  }

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
static char *
VCDFormatStr(vcdplayer_t *p_vcdplayer,
             const char *format_str, const char *mrl,
             const vcdinfo_itemid_t *itemid)
{
#define TEMP_STR_SIZE 256
  char        temp_str[TEMP_STR_SIZE];
  char       *tp = temp_str;
  const char *te = tp+TEMP_STR_SIZE-1;
  bool        saw_control_prefix = false;

  memset(temp_str, 0, TEMP_STR_SIZE);

  for (; *format_str && tp<te; ++format_str) {

    if (!saw_control_prefix && *format_str != '%') {
      *tp++ = *format_str;
      saw_control_prefix = false;
      continue;
    }

    switch(*format_str) {
    case '%':
      if (saw_control_prefix) {
        *tp++ = '%';
      }
      saw_control_prefix = !saw_control_prefix;
      break;
    case 'A':
      tp += snprintf(tp,te-tp,"%s",
                   vcdinfo_strip_trail(vcdinfo_get_album_id(p_vcdplayer->vcd),
                                                              MAX_ALBUM_LEN));
      break;

    case 'c':
      tp += snprintf(tp,te-tp,"%d",vcdinfo_get_volume_num(p_vcdplayer->vcd));
      break;

    case 'C':
      tp += snprintf(tp,te-tp,"%d",vcdinfo_get_volume_count(p_vcdplayer->vcd));
      break;

    case 'F':
      tp += snprintf(tp,te-tp,"%s",
                            vcdinfo_get_format_version_str(p_vcdplayer->vcd));
      break;

    case 'I':
      {
        switch (itemid->type) {
        case VCDINFO_ITEM_TYPE_TRACK:
          tp += snprintf(tp,te-tp,"%s",_("Track"));
        break;
        case VCDINFO_ITEM_TYPE_ENTRY:
          tp += snprintf(tp,te-tp,"%s",_("Entry"));
          break;
        case VCDINFO_ITEM_TYPE_SEGMENT:
          tp += snprintf(tp,te-tp,"%s",_("Segment"));
          break;
        case VCDINFO_ITEM_TYPE_LID:
          tp += snprintf(tp,te-tp,"%s",_("List ID"));
          break;
        case VCDINFO_ITEM_TYPE_SPAREID2:
          tp += snprintf(tp,te-tp,"%s",_("Navigation"));
          break;
        default:
          /* What to do? */
          ;
        }
        saw_control_prefix = false;
      }
      break;

    case 'L':
      if (vcdplayer_pbc_is_on(p_vcdplayer))
        tp += snprintf(tp,te-tp,"%s %d",_("List ID"),p_vcdplayer->i_lid);
      saw_control_prefix = false;
      break;

    case 'M':
      tp += snprintf(tp,te-tp,"%s",mrl);
      break;

    case 'N':
      tp += snprintf(tp,te-tp,"%d",itemid->num);
      break;

    case 'p':
      tp += snprintf(tp,te-tp,"%s",vcdinfo_get_preparer_id(p_vcdplayer->vcd));
      break;

    case 'P':
      tp += snprintf(tp,te-tp,"%s",vcdinfo_get_publisher_id(p_vcdplayer->vcd));
      break;

    case 'S':
      if ( VCDINFO_ITEM_TYPE_SEGMENT==itemid->type ) {
        tp += snprintf(tp,te-tp," %s",
                vcdinfo_video_type2str(p_vcdplayer->vcd, itemid->num));
      }
      saw_control_prefix = false;
      break;

    case 'T':
      tp += snprintf(tp,te-tp,"%d",p_vcdplayer->i_track);
      break;

    case 'V':
      tp += snprintf(tp,te-tp,"%s",vcdinfo_get_volumeset_id(p_vcdplayer->vcd));
      break;

    case 'v':
      tp += snprintf(tp,te-tp,"%s",vcdinfo_get_volume_id(p_vcdplayer->vcd));
      break;

    default:
      *tp++ = '%';
      if(tp<te)
        *tp++ = *format_str;
      saw_control_prefix = false;
    }
  }
  return strdup(temp_str);
}

void
VCDUpdateTitle( access_t *p_access )
{
    vcdplayer_t *p_vcdplayer= (vcdplayer_t *)p_access->p_sys;

    size_t psz_mrl_max = strlen(VCD_MRL_PREFIX)
                       + strlen(p_vcdplayer->psz_source) + sizeof("@E999")+3;
    char *psz_mrl = malloc( psz_mrl_max );

    if( psz_mrl )
    {
        char *psz_name;
        char *psz_tfmt = var_InheritString( p_access, MODULE_STRING "-title-format" );
        snprintf( psz_mrl, psz_mrl_max, "%s%s",
                  VCD_MRL_PREFIX, p_vcdplayer->psz_source );
        if( psz_tfmt )
        {
            psz_name = VCDFormatStr( p_vcdplayer, psz_tfmt, psz_mrl,
                                     &(p_vcdplayer->play_item) );
            free(psz_tfmt);
            input_Control( p_vcdplayer->p_input, INPUT_SET_NAME, psz_name );
            free(psz_name);
        }
        free(psz_mrl);
    }
}

