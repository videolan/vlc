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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "callback.h"      /* FIXME - reorganize callback.h, cdda.h better */
#include "cdda.h"          /* private structures. Also #includes vlc things */
#include <vlc_playlist.h>  /* Has to come *after* cdda.h */

#include <cdio/cdio.h>
#include <cdio/cdtext.h>
#include <cdio/logging.h>
#include <cdio/cd_types.h>
#include "info.h"

#ifdef HAVE_ERRNO_H
#   include <errno.h>
#endif

#define CDDA_MRL_PREFIX "cddax://"

#ifdef HAVE_LIBCDDB

#define free_and_dup(var, val) \
  if (var) free(var);          \
  if (val) var=strdup(val);


static void
GetCDDBInfo( access_t *p_access, cdda_data_t *p_cdda )
{

  int i, i_matches;
  cddb_conn_t  *conn = cddb_new();
  const CdIo *p_cdio = p_cdda->p_cdio;

  dbg_print( (INPUT_DBG_CALL), "" );

#ifdef FIXME_NOW
  cddb_log_set_handler (uninit_log_handler);
#endif
  
  if (!conn) {
    msg_Warn( p_access, "Unable to initialize libcddb" );
    goto cddb_destroy;
  }
  
  cddb_set_email_address( conn,
			  config_GetPsz( p_access,
					 MODULE_STRING "-cddb-email") );
  
  cddb_set_server_name( conn,
			config_GetPsz( p_access,
				       MODULE_STRING "-cddb-server") );
  
  cddb_set_server_port(conn,
		       config_GetInt( p_access,
				      MODULE_STRING "-cddb-port") );
  
  /* Set the location of the local CDDB cache directory.
     The default location of this directory is */
  
  if (!config_GetInt( p_access, MODULE_STRING "-cddb-enable-cache" ))
    cddb_cache_disable(conn);
  
  cddb_cache_set_dir(conn,
		     config_GetPsz( p_access,
				    MODULE_STRING "-cddb-cachedir") );
  
  cddb_set_timeout(conn,
		   config_GetInt( p_access, MODULE_STRING "-cddb-timeout") );
  
  
  if (config_GetInt( p_access, MODULE_STRING "-cddb-httpd" )) {
    cddb_http_enable(conn);
  } else
    cddb_http_disable(conn);
  
  p_cdda->cddb.disc = cddb_disc_new();
  if (!p_cdda->cddb.disc) {
    msg_Err( p_access, "Unable to create CDDB disc structure." );
    goto cddb_end;
  }
  
  for(i = 0; i < p_cdda->i_tracks; i++) {
    track_t i_track =  p_cdda->i_first_track + i;
    cddb_track_t *t = cddb_track_new();
    t->frame_offset = cdio_get_track_lba(p_cdio, i_track);
    cddb_disc_add_track(p_cdda->cddb.disc, t);
  }
  
  p_cdda->cddb.disc->length =
    cdio_get_track_lba(p_cdio, CDIO_CDROM_LEADOUT_TRACK)
    / CDIO_CD_FRAMES_PER_SEC;
  
  if (!cddb_disc_calc_discid(p_cdda->cddb.disc)) {
    msg_Err( p_access, "CDDB disc ID calculation failed" );
    goto cddb_destroy;
  }
  
  i_matches = cddb_query(conn, p_cdda->cddb.disc);
  if (i_matches > 0) {
    if (i_matches > 1)
      msg_Warn( p_access, "Found %d matches in CDDB. Using first one.",
		i_matches);
    cddb_read(conn, p_cdda->cddb.disc);
    
    if (p_cdda->i_debug & INPUT_DBG_CDDB)
      cddb_disc_print(p_cdda->cddb.disc);
    
  } else {
    msg_Warn( p_access, "CDDB error: %s", cddb_error_str(errno));
  }

 cddb_destroy:
  cddb_destroy(conn);

 cddb_end: ;
}
#endif /*HAVE_LIBCDDB*/

#define add_meta_val(FIELD, VLC_META, VAL)				\
  if ( p_cdda->p_meta && VAL) {						\
    vlc_meta_Add( p_cdda->p_meta, VLC_META, VAL );			\
    dbg_print( INPUT_DBG_META, "field %s: %s\n", VLC_META, VAL );	\
  }									\
    
#define add_cddb_meta(FIELD, VLC_META)					\
  add_meta_val(FIELD, VLC_META, p_cdda->cddb.disc->FIELD);
    
#define add_cddb_meta_fmt(FIELD, FORMAT_SPEC, VLC_META)			\
  {									\
    char psz_buf[100];							\
    snprintf( psz_buf, sizeof(psz_buf)-1, FORMAT_SPEC,			\
	      p_cdda->cddb.disc->FIELD );				\
    psz_buf[sizeof(psz_buf)-1] = '\0';					\
    add_meta_val(FIELD, VLC_META, psz_buf);				\
  }    

/* Adds a string-valued entry to the stream and media information if
   the string is not null or the null string.
 */
#define add_info_str(CATEGORY, TITLE, FIELD)			  \
  if (FIELD && strlen(FIELD)) {					  \
    input_Control( p_cdda->p_input, INPUT_ADD_INFO, CATEGORY,	  \
		   _(TITLE), "%s", FIELD );			  \
  }  
  
/* Adds a numeric-valued entry to the stream and media information 
   if the number is not zero. */
#define add_info_val(CATEGORY, TITLE, FMT, FIELD)		  \
  if (FIELD) {							  \
    input_Control( p_cdda->p_input, INPUT_ADD_INFO, CATEGORY,	  \
		   _(TITLE), FMT, FIELD );			  \
  }  

/* Adds a CDDB string-valued entry to the stream and media information
   under category "Disc" if the string is not null or the null string.
 */
#define add_cddb_disc_info_str(TITLE, FIELD)			\
  add_info_str("Disc", TITLE, p_cdda->cddb.disc->FIELD)

/* Adds a CDDB numeric-valued entry to the stream and media information
   under category "Disc" if the string is not null or the null string.
 */
#define add_cddb_disc_info_val(TITLE, FMT, FIELD)		\
  add_info_val("Disc", TITLE, FMT, p_cdda->cddb.disc->FIELD)

/* Adds a CD-Text string-valued entry to the stream and media information
   under category "Disc" if the string is not null or the null string.
 */
#define add_cdtext_info_str(CATEGORY, TITLE, INDEX, FIELD)		\
    add_info_str(CATEGORY, TITLE, p_cdda->p_cdtext[INDEX]->field[FIELD])

/* Adds a CD-Text string-valued entry to the stream and media information
   under category "Disc" if the string is not null or the null string.
 */
#define add_cdtext_disc_info_str(TITLE, FIELD)	\
  add_cdtext_info_str("Disc", TITLE, 0, FIELD)


/*
 Gets and saves CDDA Meta Information about the CD.

 In the Control routine, we handle Meta Information requests and
 basically copy what we've saved here.

 Meta information is also used elsewhere such as in "stream and
 media info" or in playlist info. The intialization of CD-Text or CDDB
 is done here though.
 */    
void 
CDDAMetaInfo( access_t *p_access  )
{
  cdda_data_t *p_cdda = (cdda_data_t *) p_access->p_sys;

  if ( ! p_cdda ) return;

  p_cdda->psz_mcn = cdio_get_mcn(p_cdda->p_cdio);
  
#ifdef HAVE_LIBCDDB
  if ( p_cdda->b_cddb_enabled ) {
    
    GetCDDBInfo(p_access, p_cdda);
    
    if ( p_cdda->cddb.disc ) {
      
      p_cdda->p_meta = vlc_meta_New();
      
      add_cddb_meta(title,    VLC_META_CDDB_TITLE);
      add_cddb_meta(artist,   VLC_META_CDDB_ARTIST);
      add_cddb_meta(genre,    VLC_META_CDDB_GENRE);
      add_cddb_meta(ext_data, VLC_META_CDDB_EXT_DATA);
      
      add_cddb_meta_fmt(year,   "%d", VLC_META_CDDB_YEAR);
      add_cddb_meta_fmt(discid, "%x", VLC_META_CDDB_DISCID);
    }
  }

#endif /*HAVE_LIBCDDB*/
#define TITLE_MAX 30
  
  {
    track_t i = p_cdda->i_tracks;
    const int i_first_track = p_cdda->i_first_track;
    char psz_buffer[MSTRTIME_MAX_SIZE];
    mtime_t i_duration =
      (p_cdda->lsn[i_first_track+i] - p_cdda->lsn[i_first_track])
      / CDIO_CD_FRAMES_PER_SEC;
    
    dbg_print( INPUT_DBG_META, "Duration %ld", (long int) i_duration );
    input_Control( p_cdda->p_input, INPUT_ADD_INFO, 
		   _("Disc"), _("Duration"), "%s",
		   secstotimestr( psz_buffer, i_duration ) );
    
#ifdef HAVE_LIBCDDB
    if (p_cdda->b_cddb_enabled && p_cdda->cddb.disc) {
      add_cddb_disc_info_str("Artist (CDDB)", artist);
      add_cddb_disc_info_str("Genre (CDDB)",  genre);
      add_cddb_disc_info_str("Extended Data (CDDB)", ext_data);
      add_cddb_disc_info_val("Year (CDDB)", "%d", year);
      add_cddb_disc_info_val("Disc ID (CDDB)", "%x", discid);
      add_cddb_disc_info_str("Title (CDDB)",  title);
      add_info_str("Disc", "Category (CDDB)",  
		   CDDB_CATEGORY[p_cdda->cddb.disc->category]);
    }
#endif /*HAVE_LIBCDDB*/

    p_cdda->p_cdtext[0] = cdio_get_cdtext(p_cdda->p_cdio, 0);
    if (p_cdda->p_cdtext[0]) {
      add_cdtext_disc_info_str("Arranger (CD-Text)",    CDTEXT_ARRANGER);
      add_cdtext_disc_info_str("Composer (CD-Text)",    CDTEXT_COMPOSER);
      add_cdtext_disc_info_str("Disc ID (CD-Text)",     CDTEXT_DISCID);
      add_cdtext_disc_info_str("Genre (CD-Text)",       CDTEXT_GENRE);
      add_cdtext_disc_info_str("Message (CD-Text)",     CDTEXT_MESSAGE);
      add_cdtext_disc_info_str("Performer (CD-Text)",   CDTEXT_PERFORMER);
      add_cdtext_disc_info_str("Songwriter (CD-Text)",  CDTEXT_SONGWRITER);
      add_cdtext_disc_info_str("Title (CD-Text)",       CDTEXT_TITLE);
    }
    
    for( i = 0 ; i < p_cdda->i_tracks ; i++ ) {

      char psz_track[TITLE_MAX];
      const track_t i_track = i_first_track + i;
      mtime_t i_duration = (p_cdda->lsn[i_track+1] - p_cdda->lsn[i_track]) 
        / CDIO_CD_FRAMES_PER_SEC;
      snprintf(psz_track, TITLE_MAX, "%s %02d", _("Track"), i_track);
      input_Control( p_cdda->p_input, INPUT_ADD_INFO, psz_track, 
		     _("Duration"), "%s", 
		     secstotimestr( psz_buffer, i_duration ) );

      p_cdda->p_cdtext[i_track] = cdio_get_cdtext(p_cdda->p_cdio, i_track);
      
      if (p_cdda->p_cdtext[i_track]) {
	add_cdtext_info_str(psz_track, "Arranger (CD-Text)",    i_track, 
			    CDTEXT_ARRANGER);
	add_cdtext_info_str(psz_track, "Composer (CD-Text)",    i_track, 
			    CDTEXT_COMPOSER);
	add_cdtext_info_str(psz_track, "Disc ID (CD-Text)",     i_track, 
			    CDTEXT_DISCID);
	add_cdtext_info_str(psz_track, "Genre (CD-Text)",       i_track, 
			    CDTEXT_GENRE);
	add_cdtext_info_str(psz_track, "Message (CD-Text)",     i_track, 
			    CDTEXT_MESSAGE);
	add_cdtext_info_str(psz_track, "Performer (CD-Text)",   i_track, 
			    CDTEXT_PERFORMER);
	add_cdtext_info_str(psz_track, "Songwriter (CD-Text)",  i_track, 
			    CDTEXT_SONGWRITER);
	add_cdtext_info_str(psz_track, "Title (CD-Text)",       i_track, 
			    CDTEXT_TITLE);
      }

#ifdef HAVE_LIBCDDB
      if (p_cdda->b_cddb_enabled) {
        cddb_track_t *t=cddb_disc_get_track(p_cdda->cddb.disc, i);
        if (t != NULL) {
	  add_info_str(psz_track, "Artist (CDDB)", t->artist);
	  add_info_str(psz_track, "Title (CDDB)",  t->title);
	  add_info_str(psz_track, "Extended Data (CDDB)", t->ext_data);
        }
      }
#endif /*HAVE_LIBCDDB*/
    }
  }
}

#define add_format_str_info(val)                         \
  {                                                      \
    const char *str = val;                               \
    unsigned int len;                                    \
    if (val != NULL) {                                   \
      len=strlen(str);                                   \
      if (len != 0) {                                    \
        strncat(tp, str, TEMP_STR_LEN-(tp-temp_str));    \
        tp += len;                                       \
      }                                                  \
      saw_control_prefix = false;                        \
    }                                                    \
  }

#define add_format_num_info(val, fmt)                    \
  {                                                      \
    char num_str[10];                                    \
    unsigned int len;                                    \
    sprintf(num_str, fmt, val);                          \
    len=strlen(num_str);                                 \
    if (len != 0) {                                      \
      strncat(tp, num_str, TEMP_STR_LEN-(tp-temp_str));  \
      tp += len;                                         \
    }                                                    \
    saw_control_prefix = false;                          \
  }

/*!
   Take a format string and expand escape sequences, that is sequences that
   begin with %, with information from the current CD.
   The expanded string is returned. Here is a list of escape sequences:

   %a : The album artist **
   %A : The album information **
   %C : Category **
   %I : CDDB disk ID **
   %G : Genre **
   %M : The current MRL
   %m : The CD-DA Media Catalog Number (MCN)
   %n : The number of tracks on the CD
   %p : The artist/performer/composer in the track **
   %T : The track number **
   %s : Number of seconds in this track
   %t : The name **
   %Y : The year 19xx or 20xx **
   %% : a %
*/
static char *
CDDAFormatStr( const access_t *p_access, cdda_data_t *p_cdda,
	       const char format_str[], const char *mrl, track_t i_track)
{
#define TEMP_STR_SIZE 256
#define TEMP_STR_LEN (TEMP_STR_SIZE-1)
  static char    temp_str[TEMP_STR_SIZE];
  size_t i;
  char * tp = temp_str;
  vlc_bool_t saw_control_prefix = false;
  size_t format_len = strlen(format_str);

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
#ifdef HAVE_LIBCDDB
    case 'a':
      if (!p_cdda->b_cddb_enabled) goto not_special;
      if (p_cdda->cddb.disc)
	add_format_str_info(p_cdda->cddb.disc->artist);
      break;
    case 'A':
      if (!p_cdda->b_cddb_enabled) goto not_special;
      if (p_cdda->cddb.disc)
	add_format_str_info(p_cdda->cddb.disc->title);
      break;
    case 'C':
      if (!p_cdda->b_cddb_enabled) goto not_special;
      if (p_cdda->cddb.disc)
	add_format_str_info(CDDB_CATEGORY[p_cdda->cddb.disc->category]);
      break;
    case 'G':
      if (!p_cdda->b_cddb_enabled) goto not_special;
      if (p_cdda->cddb.disc)
	add_format_str_info(p_cdda->cddb.disc->genre);
      break;
    case 'I':
      if (!p_cdda->b_cddb_enabled) goto not_special;
      if (p_cdda->cddb.disc)
	add_format_num_info(p_cdda->cddb.disc->discid, "%x");
      break;
    case 'Y':
      if (!p_cdda->b_cddb_enabled) goto not_special;
      if (p_cdda->cddb.disc)
	add_format_num_info(p_cdda->cddb.disc->year, "%5d");
      break;
    case 't':
      if (p_cdda && p_cdda->b_cddb_enabled && p_cdda->cddb.disc) {
        cddb_track_t *t=cddb_disc_get_track(p_cdda->cddb.disc,
                                            i_track-1);
        if (t != NULL && t->title != NULL)
          add_format_str_info(t->title);
      } else goto not_special;
      break;
    case 'p':
      if (p_cdda->b_cddb_enabled && p_cdda->cddb.disc) {
        cddb_track_t *t=cddb_disc_get_track(p_cdda->cddb.disc,
                                            i_track-1);
        if (t != NULL && t->artist != NULL)
          add_format_str_info(t->artist);
      } else goto not_special;
      break;
    case 'e':
      if (p_cdda->b_cddb_enabled && p_cdda->cddb.disc) {
        cddb_track_t *t=cddb_disc_get_track(p_cdda->cddb.disc,
                                            i_track-1);
        if (t != NULL && t->ext_data != NULL)
          add_format_str_info(t->ext_data);
      } else goto not_special;
      break;
    case 's':
      if (p_cdda->b_cddb_enabled) {
        char psz_buffer[MSTRTIME_MAX_SIZE];
        mtime_t i_duration = (p_cdda->lsn[i_track+1] - p_cdda->lsn[i_track]) 
          / CDIO_CD_FRAMES_PER_SEC;
        add_format_str_info(secstotimestr( psz_buffer, i_duration ) );
      } else 
	goto not_special;
      break;
#endif /*HAVE_LIBCDDB*/

    case 'M':
      add_format_str_info(mrl);
      break;

    case 'm':
      add_format_str_info(p_cdda->psz_mcn);
      break;

    case 'n':
      add_format_num_info(p_cdda->i_tracks, "%d");
      break;

    case 'T':
      add_format_num_info(i_track, "%02d");
      break;
#ifdef HAVE_LIBCDDB
    not_special:
#endif
    default:
      *tp++ = '%';
      *tp++ = format_str[i];
      saw_control_prefix = false;
    }
  }
  return strdup(temp_str);
}

/* Adds a string-valued entry to the playlist information under "Track"
   if the string is not null or the null string.
 */
#define add_playlist_track_info_str(TITLE, FIELD)		  \
  if (FIELD && strlen(FIELD)) {					  \
    playlist_ItemAddInfo( p_item, _("Track"), _(TITLE),		  \
			  "%s", FIELD);				  \
  }  

void
CDDACreatePlaylistItem(const access_t *p_access, cdda_data_t *p_cdda,
                       playlist_t *p_playlist, track_t i_track,
                       char *psz_mrl, int psz_mrl_max,
                       const char *psz_source, int playlist_operation,
                       int i_pos)
{
  mtime_t i_duration = (p_cdda->lsn[i_track+1] - p_cdda->lsn[i_track])
    * (1000000 / CDIO_CD_FRAMES_PER_SEC) ;
  char *psz_title;
  char *config_varname = MODULE_STRING "-title-format";
  playlist_item_t *p_item;

#ifdef HAVE_LIBCDDB
  if (p_cdda->b_cddb_enabled) {
    config_varname = MODULE_STRING "-cddb-title-format";
  }
#endif /*HAVE_LIBCDDB*/

  snprintf(psz_mrl, psz_mrl_max, "%s%s@T%u",
           CDDA_MRL_PREFIX, psz_source, i_track);

  psz_title = CDDAFormatStr(p_access, p_cdda,
			    config_GetPsz( p_access, config_varname ),
			    psz_mrl, i_track);

  dbg_print( INPUT_DBG_META, "mrl: %s, title: %s, duration, %ld, pos %d",
             psz_mrl, psz_title, (long int) i_duration / 1000000 , i_pos );
  playlist_AddExt( p_playlist, psz_mrl, psz_title, playlist_operation,
                         i_pos, i_duration , NULL, 0);

  if( i_pos == PLAYLIST_END ) i_pos = p_playlist->i_size - 1;

  vlc_mutex_lock( &p_playlist->object_lock );
  p_item = playlist_ItemGetByPos( p_playlist, i_pos );
  vlc_mutex_unlock( &p_playlist->object_lock );
  if( !p_item )
      return;

  vlc_mutex_lock( &p_item->input.lock );

  add_playlist_track_info_str("Source",  psz_source);
  playlist_ItemAddInfo( p_item, _("Track"), _("Track Number"), "%d", i_track );

  if (p_cdda->p_cdtext[0]) {
    const cdtext_t *p = p_cdda->p_cdtext[0];
    add_playlist_track_info_str("Disc Arranger (CD-Text)",
				p->field[CDTEXT_ARRANGER]);
    add_playlist_track_info_str("Disc Composer (CD-Text)",
				p->field[CDTEXT_COMPOSER]);
    add_playlist_track_info_str("Disc ID (CD-Text)",
				p->field[CDTEXT_DISCID]);
    add_playlist_track_info_str("Disc Genre (CD-Text)",
				p->field[CDTEXT_GENRE]);
    add_playlist_track_info_str("Disc Message (CD-Text)",
				p->field[CDTEXT_MESSAGE]);
    add_playlist_track_info_str("Disc Performer (CD-Text)",
				p->field[CDTEXT_PERFORMER]);
    add_playlist_track_info_str("Disc Songwriter (CD-Text)",
				p->field[CDTEXT_SONGWRITER]);
    add_playlist_track_info_str("Disc Title (CD-Text)",
				p->field[CDTEXT_TITLE]);
  }

  if (p_cdda->p_cdtext[i_track]) {
    const cdtext_t *p = p_cdda->p_cdtext[i_track];
    add_playlist_track_info_str("Arranger (CD-Text)",
				p->field[CDTEXT_ARRANGER]);
    add_playlist_track_info_str("Composer (CD-Text)",
				p->field[CDTEXT_COMPOSER]);
    add_playlist_track_info_str("Genre (CD-Text)",
				p->field[CDTEXT_GENRE]);
    add_playlist_track_info_str("Message (CD-Text)",
				p->field[CDTEXT_MESSAGE]);
    add_playlist_track_info_str("Performer (CD-Text)",
				p->field[CDTEXT_PERFORMER]);
    add_playlist_track_info_str("Songwriter (CD-Text)",
				p->field[CDTEXT_SONGWRITER]);
    add_playlist_track_info_str("Title (CD-Text)",
				p->field[CDTEXT_TITLE]);
  }
  

#ifdef HAVE_LIBCDDB
  if (p_cdda->b_cddb_enabled) {
    cddb_track_t *t=cddb_disc_get_track(p_cdda->cddb.disc, 
					i_track-p_cdda->i_first_track);

    add_playlist_track_info_str("Disc Album (CDDB)",
			       p_cdda->cddb.disc->title);
    add_playlist_track_info_str("Disc Artist(s) (CDDB)", 
			       p_cdda->cddb.disc->artist);
    add_playlist_track_info_str("Disc Category (CDDB)", 
			       CDDB_CATEGORY[p_cdda->cddb.disc->category]);
    add_playlist_track_info_str("Disc Genre (CDDB)", 
			       p_cdda->cddb.disc->genre);
    if ( p_cdda->cddb.disc->discid ) {
      playlist_ItemAddInfo( p_item, _("Track"), _("Disc ID (CDDB)"),
			    "%x", p_cdda->cddb.disc->discid );
    }
    if (p_cdda->cddb.disc->year != 0) {
      playlist_ItemAddInfo( p_item, _("Track"), _("Year (CDDB)"), 
			    "%5d", p_cdda->cddb.disc->year );
    }
    
    if (t) {
      if (t->artist)
	add_playlist_track_info_str("Track Artist (CDDB)", 
				   t->artist);
      if (t->title)
	add_playlist_track_info_str("Track Title (CDDB)", 
				   t->title);
    }
  }
#endif /*HAVE_LIBCDDB*/

  vlc_mutex_unlock( &p_item->input.lock );
}

int
CDDAFixupPlaylist( access_t *p_access, cdda_data_t *p_cdda, 
		   const char *psz_source, vlc_bool_t b_single_track )
{
  int i;
  playlist_t * p_playlist;
  char       * psz_mrl;
  unsigned int psz_mrl_max = strlen(CDDA_MRL_PREFIX) + strlen(psz_source) +
    strlen("@T") + strlen("100") + 1;
  const track_t i_first_track = p_cdda->i_first_track;

#ifdef HAVE_LIBCDDB
  p_cdda->b_cddb_enabled =
    config_GetInt( p_access, MODULE_STRING "-cddb-enabled" );
  if( b_single_track && !p_cdda->b_cddb_enabled ) return VLC_SUCCESS;
#else
  if( b_single_track ) return VLC_SUCCESS;
#endif

  psz_mrl = malloc( psz_mrl_max );

  if( psz_mrl == NULL )
    {
      msg_Warn( p_access, "out of memory" );
      return VLC_ENOMEM;
    }

  p_playlist = (playlist_t *) vlc_object_find( p_access, VLC_OBJECT_PLAYLIST,
                                               FIND_ANYWHERE );
  if( !p_playlist )
    {
      msg_Warn( p_access, "can't find playlist" );
      free(psz_mrl);
      return VLC_EGENERIC;
    }

  CDDAMetaInfo(p_access);

  if (b_single_track) {
    /* May fill out more information when the playlist user interface becomes
       more mature.
     */
    track_t i_track = p_cdda->i_track;

    input_title_t *t = p_cdda->p_title[i_track-i_first_track] = 
      vlc_input_title_New();
	
    asprintf( &t->psz_name, _("Track %i"), i_track );
    t->i_size = p_access->info.i_size =
      ( p_cdda->lsn[i_track+1] - p_cdda->lsn[i_track] ) * 
      (int64_t) CDIO_CD_FRAMESIZE_RAW;
	
    t->i_length = I64C(1000000) * t->i_size / CDDA_FREQUENCY_SAMPLE / 4;
    CDDACreatePlaylistItem(p_access, p_cdda, p_playlist, i_track,
                           psz_mrl, psz_mrl_max, psz_source, PLAYLIST_REPLACE,
                           p_playlist->i_index);
    p_cdda->i_titles = 1;
  } else {

    for( i = 0 ; i < p_cdda->i_tracks ; i++ )
      {
	const track_t i_track = i_first_track + i;
	input_title_t *t = p_cdda->p_title[i] = vlc_input_title_New();
	
	asprintf( &t->psz_name, _("Track %i"), i_track );
	t->i_size = ( p_cdda->lsn[i_track+1] - p_cdda->lsn[i_track] ) *
	  (int64_t) CDIO_CD_FRAMESIZE_RAW;
	
	t->i_length = I64C(1000000) * t->i_size / CDDA_FREQUENCY_SAMPLE / 4;
	CDDACreatePlaylistItem(p_access, p_cdda, p_playlist, i_track, 
			       psz_mrl, psz_mrl_max, psz_source, 
			       PLAYLIST_APPEND, PLAYLIST_END);
      }
    p_cdda->i_titles = p_cdda->i_tracks; /* should be +1 */
    p_access->info.i_size = 
      (p_cdda->lsn[i_first_track + p_cdda->i_tracks] 
       - p_cdda->lsn[i_first_track]) * (int64_t) CDIO_CD_FRAMESIZE_RAW;
  }

  return VLC_SUCCESS;
}

