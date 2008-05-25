/*****************************************************************************
 * cdda.h : CD-DA input module header for vlc using libcdio.
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Author: Rocky Bernstein <rocky@panix.com>
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

#include <vlc_input.h>
#include <vlc_access.h>
#include <cdio/cdio.h>
#include <cdio/cdtext.h>
#if LIBCDIO_VERSION_NUM >= 73
#include <cdio/audio.h>
#include <cdio/mmc.h>
#endif

#include <vlc_meta.h>
#include <vlc_codecs.h>

#ifdef HAVE_LIBCDDB
#include <cddb/cddb.h>
#endif


#define CDDA_MRL_PREFIX "cddax://"

/* Frequency of sample in bits per second. */
#define CDDA_FREQUENCY_SAMPLE 44100

/*****************************************************************************
 * Debugging
 *****************************************************************************/
#define INPUT_DBG_META        1 /* Meta information */
#define INPUT_DBG_EVENT       2 /* Trace keyboard events */
#define INPUT_DBG_MRL         4 /* MRL debugging */
#define INPUT_DBG_EXT         8 /* Calls from external routines */
#define INPUT_DBG_CALL       16 /* all calls */
#define INPUT_DBG_LSN        32 /* LSN changes */
#define INPUT_DBG_SEEK       64 /* Seeks to set location */
#define INPUT_DBG_CDIO      128 /* Debugging from CDIO */
#define INPUT_DBG_CDDB      256 /* CDDB debugging  */

#define INPUT_DEBUG 1
#if INPUT_DEBUG
#define dbg_print(mask, s, args...) \
   if (p_cdda->i_debug & mask) \
     msg_Dbg(p_access, "%s: "s, __func__ , ##args)
#else
#define dbg_print(mask, s, args...)
#endif

#if LIBCDIO_VERSION_NUM >= 72
#include <cdio/cdda.h>
#include <cdio/paranoia.h>
#else
#define CdIo_t CdIo
#endif
 
/*****************************************************************************
 * cdda_data_t: CD audio information
 *****************************************************************************/
typedef struct cdda_data_s
{
  CdIo_t         *p_cdio;             /* libcdio CD device */
  track_t        i_tracks;            /* # of tracks */
  track_t        i_first_track;       /* # of first track */
  track_t        i_titles;            /* # of titles in playlist */
 
  /* Current position */
  track_t        i_track;             /* Current track */
  lsn_t          i_lsn;               /* Current Logical Sector Number */
 
  lsn_t          first_frame;         /* LSN of first frame of this track   */
  lsn_t          last_frame;          /* LSN of last frame of this track    */
  lsn_t          last_disc_frame;     /* LSN of last frame on CD            */
  int            i_blocks_per_read;   /* # blocks to get in a read */
  int            i_debug;             /* Debugging mask */

  /* Information about CD */
  vlc_meta_t    *p_meta;
  char *         psz_mcn;             /* Media Catalog Number */
  char *         psz_source;          /* CD drive or CD image filename */
  input_title_t *p_title[CDIO_CD_MAX_TRACKS]; /* This *is* 0 origin, not
                             track number origin */

#if LIBCDIO_VERSION_NUM >= 72
  /* Paranoia support */
  paranoia_mode_t e_paranoia;         /* Use cd paranoia for reads? */
  cdrom_drive_t *paranoia_cd;         /* Place to store drive
                     handle given by paranoia. */
  cdrom_paranoia_t *paranoia;

#endif
 
#ifdef HAVE_LIBCDDB
  bool     b_cddb_enabled;      /* Use CDDB at all? */
  struct  {
    bool   have_info;           /* True if we have any info */
    cddb_disc_t *disc;                /* libcdio uses this to get disc
                     info */
    int          disc_length;         /* Length in frames of cd. Used
                     in CDDB lookups */
  } cddb;
#endif

  bool   b_audio_ctl;           /* Use CD-Text audio controls and
                     audio output? */

  bool   b_cdtext;              /* Use CD-Text at all? If not,
                     cdtext_preferred is meaningless. */
  bool   b_cdtext_prefer;       /* Prefer CD-Text info over
                     CDDB? If no CDDB, the issue
                     is moot. */

  const cdtext_t *p_cdtext[CDIO_CD_MAX_TRACKS]; /* CD-Text info. Origin is NOT
                           0 origin but origin of track
                           number (usually 1).
                         */

  WAVEHEADER   waveheader;            /* Wave header for the output data  */
  bool   b_header;
  bool   b_nav_mode;           /* If false we view the entire CD as
                    as a unit rather than each track
                    as a unit. If b_nav_mode then the
                    slider area represents the Disc rather
                    than a track
                      */
 
  input_thread_t *p_input;
 
} cdda_data_t;

/* FIXME: This variable is a hack. Would be nice to eliminate. */
extern access_t *p_cdda_input;
