/*****************************************************************************
 * cddax.c : CD digital audio input module for vlc using libcdio
 *****************************************************************************
 * Copyright (C) 2000, 2003, 2004 VideoLAN
 * $Id$
 *
 * Authors: Rocky Bernstein <rocky@panix.com>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
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
#include <stdio.h>
#include <stdlib.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc_playlist.h>

#include <sys/types.h>
#include <cdio/cdio.h>
#include <cdio/logging.h>
#include <cdio/cd_types.h>

#include "codecs.h"
#include "vlc_keys.h"

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#ifdef HAVE_ERRNO_H
#   include <errno.h>
#endif

#include <string.h>

#include "cdda.h"

#define CDDA_MRL_PREFIX "cddax://"

/* how many blocks Open will read in each loop. Note libcdio and
   SCSI MMC devices can read at most 25 blocks.
*/
#define CDDA_BLOCKS_ONCE 20
#define CDDA_DATA_ONCE   (CDDA_BLOCKS_ONCE * CDIO_CD_FRAMESIZE_RAW)

/* Frequency of sample in bits per second. */
#define CDDA_FREQUENCY_SAMPLE 44100

/* FIXME: This variable is a hack. Would be nice to eliminate. */
static access_t *p_cdda_input = NULL;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static block_t *CDDABlock( access_t * p_access );
static int      CDDASeek( access_t * p_access, int64_t i_pos );
static int      CDDAControl( access_t *p_access, int i_query, 
			     va_list args );
static void     CDDAMetaInfo( access_t *p_access  );
static int      CDDAFixupPlaylist( access_t *p_access, cdda_data_t *p_cdda, 
				   const char *psz_source, 
				   vlc_bool_t b_single_track );
static void     CDDACreatePlaylistItem(const access_t *p_access, 
				       cdda_data_t *p_cdda,
				       playlist_t *p_playlist, 
				       track_t i_track,
				       char *psz_mrl, int psz_mrl_max,
				       const char *psz_source, 
				       int playlist_operation,
				       int i_pos);

static int      GetCDInfo( access_t *p_access, cdda_data_t *p_cdda ) ;




/****************************************************************************
 * Private functions
 ****************************************************************************/

/* process messages that originate from libcdio. */
static void
cdio_log_handler (cdio_log_level_t level, const char message[])
{
  cdda_data_t *p_cdda = (cdda_data_t *)p_cdda_input->p_sys;
  switch (level) {
  case CDIO_LOG_DEBUG:
  case CDIO_LOG_INFO:
    if (p_cdda->i_debug & INPUT_DBG_CDIO)
      msg_Dbg( p_cdda_input, message);
    break;
  case CDIO_LOG_WARN:
    msg_Warn( p_cdda_input, message);
    break;
  case CDIO_LOG_ERROR:
  case CDIO_LOG_ASSERT:
    msg_Err( p_cdda_input, message);
    break;
  default:
    msg_Warn( p_cdda_input, message,
            _("The above message had unknown cdio log level"),
            level);
  }
  return;
}


#ifdef HAVE_LIBCDDB
/*! This routine is called by libcddb routines on error.
   Setup is done by init_input_plugin.
*/
static void
cddb_log_handler (cddb_log_level_t level, const char message[])
{
  cdda_data_t *p_cdda = (cdda_data_t *)p_cdda_input->p_sys;
  switch (level) {
  case CDDB_LOG_DEBUG:
  case CDDB_LOG_INFO:
    if (!(p_cdda->i_debug & INPUT_DBG_CDDB)) return;
    /* Fall through if to warn case */
  default:
    cdio_log_handler (level, message);
  }
}
#endif /*HAVE_LIBCDDB*/


/*! This routine is when xine is not fully set up (before full initialization)
   or is not around (before finalization).
*/
static void
uninit_log_handler (cdio_log_level_t level, const char message[])
{
  cdda_data_t *p_cdda = NULL;

  if (p_cdda_input)
    p_cdda = (cdda_data_t *)p_cdda_input->p_sys;

  switch (level) {
  case CDIO_LOG_DEBUG:
  case CDIO_LOG_INFO:
    if (!p_cdda || !(p_cdda->i_debug & (INPUT_DBG_CDIO|INPUT_DBG_CDDB)))
      return;
    /* Fall through if to warn case */
  case CDIO_LOG_WARN:
    fprintf(stderr, "WARN: %s\n", message);
    break;
  case CDIO_LOG_ERROR:
    fprintf(stderr, "ERROR: %s\n", message);
    break;
  case CDIO_LOG_ASSERT:
    fprintf(stderr, "ASSERT ERROR: %s\n", message);
    break;
  default:
    fprintf(stderr, "UNKNOWN ERROR: %s\n%s %d\n",
            message,
            _("The above message had unknown cdio log level"),
            level);
  }

  /* gl_default_cdio_log_handler (level, message); */
}

/*****************************************************************************
 * CDDARead: reads CDDA_BLOCKS_ONCE from the CD-DA and returns an
 * allocated pointer to the data. NULL is returned if no data read. It
 * is also possible if we haven't read a RIFF header in which case one 
 * that we creaded during Open/Initialization is returned.
 *****************************************************************************/
static block_t *
CDDABlock( access_t * p_access )
{
    block_t     *p_block;
    cdda_data_t *p_cdda   = (cdda_data_t *) p_access->p_sys;
    int          i_blocks = CDDA_BLOCKS_ONCE;

    dbg_print((INPUT_DBG_CALL|INPUT_DBG_EXT|INPUT_DBG_LSN), "called %d", 
	      p_cdda->i_lsn);

    /* Check end of file */
    if( p_access->info.b_eof ) return NULL;

    if( !p_cdda->b_header )
      {
        /* Return only the dummy RIFF header we created in Open/Init */
        p_block = block_New( p_access, sizeof( WAVEHEADER ) );
        memcpy( p_block->p_buffer, &p_cdda->waveheader, sizeof(WAVEHEADER) );
        p_cdda->b_header = VLC_TRUE;
        return p_block;
    }

    /* Check end of track */
    while( p_cdda->i_lsn >= p_cdda->p_lsns[p_access->info.i_title + 1] )
    {
        if( p_access->info.i_title + 1 >= p_cdda->i_tracks )
        {
            p_access->info.b_eof = VLC_TRUE;
            return NULL;
        }

        p_access->info.i_update |= INPUT_UPDATE_TITLE | INPUT_UPDATE_SIZE;
        p_access->info.i_title++;
        p_access->info.i_size = 
	  p_cdda->p_title[p_access->info.i_title]->i_size;
        p_access->info.i_pos = 0;
	p_cdda->i_track++;
    }

    /* Don't read after the end of a title */
    if( p_cdda->i_lsn + i_blocks >=
        p_cdda->p_lsns[p_access->info.i_title + 1] )
    {
        i_blocks = p_cdda->p_lsns[p_access->info.i_title + 1 ] -
                   p_cdda->i_lsn;
    }

    /* Do the actual reading */
    p_block = block_New( p_access, i_blocks * CDIO_CD_FRAMESIZE_RAW );
    if( !p_block)
    {
        msg_Err( p_access, "cannot get a new block of size: %i",
                 i_blocks * CDIO_CD_FRAMESIZE_RAW );
        return NULL;
    }

    if( cdio_read_audio_sectors( p_cdda->p_cdio, p_block->p_buffer,
				 p_cdda->i_lsn, i_blocks) != 0 )
        {
            msg_Err( p_access, "could not read sector %lu", 
		     (long unsigned int) p_cdda->i_lsn );
            block_Release( p_block );

	    /* If we had problems above, assume the problem is with
	       the first sector of the read and set to skip it.  In
	       the future libcdio may have cdparanoia support.
	     */
	    p_cdda->i_lsn++;
	    p_access->info.i_pos += CDIO_CD_FRAMESIZE_RAW;
	    return NULL;
        }

    p_cdda->i_lsn     += i_blocks;
    p_access->info.i_pos += p_block->i_buffer;

    return p_block;
}

/****************************************************************************
 * CDDASeek - change position for subsequent reads. For example, this
 * can happen if the user moves a position slider bar in a GUI.
 ****************************************************************************/
static int 
CDDASeek( access_t * p_access, int64_t i_pos )
{
    cdda_data_t *p_cdda = (cdda_data_t *) p_access->p_sys;

    p_cdda->i_lsn = p_cdda->p_lsns[p_access->info.i_title]
                       + i_pos / CDIO_CD_FRAMESIZE_RAW;
    p_access->info.i_pos = i_pos;

    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_EXT|INPUT_DBG_SEEK),
               "lsn %lu, offset: %lld",  
	       (long unsigned int) p_cdda->i_lsn, i_pos );
    return VLC_SUCCESS;
}

#ifdef HAVE_LIBCDDB

#define free_and_dup(var, val) \
  if (var) free(var);          \
  if (val) var=strdup(val);


static void
GetCDDBInfo( access_t *p_access, cdda_data_t *p_cdda )
{

  dbg_print( (INPUT_DBG_CALL|INPUT_DBG_EXT), "" );

  if (config_GetInt( p_access, MODULE_STRING "-cddb-enabled" )) {
    int i, i_matches;
    cddb_conn_t  *conn = cddb_new();
    const CdIo *p_cdio = p_cdda->p_cdio;


    cddb_log_set_handler (uninit_log_handler);

    if (!conn) {
      msg_Warn( p_access, _("Unable to initialize libcddb") );
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
      msg_Err( p_access, _("Unable to create CDDB disc structure.") );
      goto cddb_end;
    }

    p_cdda->psz_mcn = cdio_get_mcn(p_cdio);

    for(i = 1; i <= p_cdda->i_tracks; i++) {
      cddb_track_t *t = cddb_track_new();
      t->frame_offset = cdio_get_track_lba(p_cdio, i);
      cddb_disc_add_track(p_cdda->cddb.disc, t);
    }

    p_cdda->cddb.disc->length =
      cdio_get_track_lba(p_cdio, CDIO_CDROM_LEADOUT_TRACK)
      / CDIO_CD_FRAMES_PER_SEC;

    if (!cddb_disc_calc_discid(p_cdda->cddb.disc)) {
      msg_Err( p_access, _("CDDB disc ID calculation failed") );
      goto cddb_destroy;
    }

    i_matches = cddb_query(conn, p_cdda->cddb.disc);
    if (i_matches > 0) {
      if (i_matches > 1)
        msg_Warn( p_access, _("Found %d matches in CDDB. Using first one."),
                  i_matches);
      cddb_read(conn, p_cdda->cddb.disc);

      if (p_cdda->i_debug & INPUT_DBG_CDDB)
        cddb_disc_print(p_cdda->cddb.disc);

    } else {
      msg_Warn( p_access, _("CDDB error: %s"), cddb_error_str(errno));
    }

  cddb_destroy:
    cddb_destroy(conn);
  }
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

/*
 Gets and saves CDDA Meta Information. In the Control routine, 
 we handle Meta Information requests and basically copy what we've
 saved here. 
 */    
static void CDDAMetaInfo( access_t *p_access  )
{
  cdda_data_t *p_cdda = (cdda_data_t *) p_access->p_sys;

#ifdef HAVE_LIBCDDB
  if ( p_cdda && p_cdda->i_cddb_enabled ) {

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

#if UPDATE_TRACK_INFORMATION_FINISHED
  {
    track_t i_track = p_cdda->i_tracks;
    char psz_buffer[MSTRTIME_MAX_SIZE];
    mtime_t i_duration =
      (p_cdda->p_lsns[i_track] - p_cdda->p_lsns[0])
      / CDIO_CD_FRAMES_PER_SEC;

    dbg_print( INPUT_DBG_META, "Duration %ld", (long int) i_duration );
    input_Control( p_access, INPUT_ADD_INFO, _("General"), _("Duration"), "%s",
		   secstotimestr( psz_buffer, i_duration ) );

    for( i_track = 0 ; i_track < p_cdda->i_tracks ; i_track++ ) {
      char track_str[TITLE_MAX];
      mtime_t i_duration =
        (p_cdda->p_lsns[i_track+1] - p_cdda->p_lsns[i_track])
        / CDIO_CD_FRAMES_PER_SEC;
      snprintf(track_str, TITLE_MAX, "%s %02d", _("Track"), i_track+1);
      input_Control( p_access, INPUT_ADD_INFO, track_str, _("Duration"), "%s",
                     secstotimestr( psz_buffer, i_duration ) );

#ifdef HAVE_LIBCDDB
      if (p_cdda->i_cddb_enabled) {
        cddb_track_t *t=cddb_disc_get_track(p_cdda->cddb.disc,
                                            i_track);
        if (t != NULL) {
          if ( t->artist != NULL && strlen(t->artist) ) {
            input_Control( p_access, INPUT_ADD_INFO, track_str,
                           _("Artist"), "%s", t->artist );
          }
          if ( t->title != NULL && strlen(t->title) ) {
            input_Control( p_access, INPUT_ADD_INFO, track_str,
                           _("Title"), "%s",  t->title );
          }
          if ( t->ext_data != NULL && strlen(t->ext_data) ) {
            input_Control( p_access, INPUT_ADD_INFO, track_str,
                           _("Extended Data"), "%s",  t->ext_data );
          }
        }
      }
#endif /*HAVE_LIBCDDB*/
    }
  }
#endif /* UPDATE_TRACK_INFORMATION_FINISHED */
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
	       const char format_str[], const char *mrl, int i_track)
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
      if (!p_cdda->i_cddb_enabled) goto not_special;
      if (p_cdda->cddb.disc)
	add_format_str_info(p_cdda->cddb.disc->artist);
      break;
    case 'A':
      if (!p_cdda->i_cddb_enabled) goto not_special;
      if (p_cdda->cddb.disc)
	add_format_str_info(p_cdda->cddb.disc->title);
      break;
    case 'C':
      if (!p_cdda->i_cddb_enabled) goto not_special;
      if (p_cdda->cddb.disc)
	add_format_str_info(CDDB_CATEGORY[p_cdda->cddb.disc->category]);
      break;
    case 'G':
      if (!p_cdda->i_cddb_enabled) goto not_special;
      if (p_cdda->cddb.disc)
	add_format_str_info(p_cdda->cddb.disc->genre);
      break;
    case 'I':
      if (!p_cdda->i_cddb_enabled) goto not_special;
      if (p_cdda->cddb.disc)
	add_format_num_info(p_cdda->cddb.disc->discid, "%x");
      break;
    case 'Y':
      if (!p_cdda->i_cddb_enabled) goto not_special;
      if (p_cdda->cddb.disc)
	add_format_num_info(p_cdda->cddb.disc->year, "%5d");
      break;
    case 't':
      if (p_cdda && p_cdda->i_cddb_enabled && p_cdda->cddb.disc) {
        cddb_track_t *t=cddb_disc_get_track(p_cdda->cddb.disc,
                                            i_track-1);
        if (t != NULL && t->title != NULL)
          add_format_str_info(t->title);
      } else goto not_special;
      break;
    case 'p':
      if (p_cdda->i_cddb_enabled && p_cdda->cddb.disc) {
        cddb_track_t *t=cddb_disc_get_track(p_cdda->cddb.disc,
                                            i_track-1);
        if (t != NULL && t->artist != NULL)
          add_format_str_info(t->artist);
      } else goto not_special;
      break;
    case 'e':
      if (p_cdda->i_cddb_enabled && p_cdda->cddb.disc) {
        cddb_track_t *t=cddb_disc_get_track(p_cdda->cddb.disc,
                                            i_track-1);
        if (t != NULL && t->ext_data != NULL)
          add_format_str_info(t->ext_data);
      } else goto not_special;
      break;
#endif

    case 'M':
      add_format_str_info(mrl);
      break;

    case 'm':
      add_format_str_info(p_cdda->psz_mcn);
      break;

    case 'n':
      add_format_num_info(p_cdda->i_tracks, "%d");
      break;

    case 's':
      if (p_cdda->i_cddb_enabled) {
        char psz_buffer[MSTRTIME_MAX_SIZE];
        mtime_t i_duration =
          (p_cdda->p_lsns[i_track] - p_cdda->p_lsns[i_track-1])
          / CDIO_CD_FRAMES_PER_SEC;
        add_format_str_info(secstotimestr( psz_buffer, i_duration ) );
      } else goto not_special;
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

static void
CDDACreatePlaylistItem(const access_t *p_access, cdda_data_t *p_cdda,
                       playlist_t *p_playlist, track_t i_track,
                       char *psz_mrl, int psz_mrl_max,
                       const char *psz_source, int playlist_operation,
                       int i_pos)
{
  mtime_t i_duration =
    (p_cdda->p_lsns[i_track] - p_cdda->p_lsns[i_track-1])
    * (1000000 / CDIO_CD_FRAMES_PER_SEC) ;
  char *p_author;
  char *p_title;
  char *config_varname = MODULE_STRING "-title-format";
  playlist_item_t *p_item;

#ifdef HAVE_LIBCDDB
  if (p_cdda->i_cddb_enabled) {
    config_varname = MODULE_STRING "-cddb-title-format";
  }
#endif /*HAVE_LIBCDDB*/

  snprintf(psz_mrl, psz_mrl_max, "%s%s@T%u",
           CDDA_MRL_PREFIX, psz_source, i_track);

  p_title = CDDAFormatStr(p_access, p_cdda,
                          config_GetPsz( p_access, config_varname ),
                          psz_mrl, i_track);

  dbg_print( INPUT_DBG_META, "mrl: %s, title: %s, duration, %ld, pos %d",
             psz_mrl, p_title, (long int) i_duration / 1000000 , i_pos );
  playlist_AddExt( p_playlist, psz_mrl, p_title, playlist_operation,
                         i_pos, i_duration , NULL, 0);

  if( i_pos == PLAYLIST_END ) i_pos = p_playlist->i_size - 1;

  vlc_mutex_lock( &p_playlist->object_lock );
  p_item = playlist_ItemGetByPos( p_playlist, i_pos );
  vlc_mutex_unlock( &p_playlist->object_lock );
  if( !p_item )
      return;

  vlc_mutex_lock( &p_item->input.lock );

  p_author =
    CDDAFormatStr( p_access, p_cdda,
                   config_GetPsz( p_access, MODULE_STRING "-author-format" ),
                   psz_mrl, i_track );

  playlist_ItemAddInfo( p_item ,  _("General"),_("Author"), p_author);

#ifdef HAVE_LIBCDDB
  if (p_cdda->i_cddb_enabled) {
    const char *psz_general_cat = _("General");

    playlist_ItemAddInfo( p_item, psz_general_cat, _("Album"),
                      "%s", p_cdda->cddb.disc->title);
    playlist_ItemAddInfo( p_item, psz_general_cat, _("Disc Artist(s)"),
                      "%s", p_cdda->cddb.disc->artist);
    playlist_ItemAddInfo( p_item, psz_general_cat,
                        _("CDDB Disc Category"),
                      "%s", CDDB_CATEGORY[p_cdda->cddb.disc->category]);
    playlist_ItemAddInfo( p_item, psz_general_cat, _("Genre"),
                      "%s", p_cdda->cddb.disc->genre);
    if ( p_cdda->cddb.disc->discid ) {
      playlist_ItemAddInfo( p_item, psz_general_cat, _("CDDB Disc ID"),
                        "%x", p_cdda->cddb.disc->discid );
    }
    if (p_cdda->cddb.disc->year != 0) {
      playlist_ItemAddInfo( p_item, psz_general_cat,
                        _("Year"), "%5d", p_cdda->cddb.disc->year );
    }

    if (p_cdda->i_cddb_enabled) {
      cddb_track_t *t=cddb_disc_get_track(p_cdda->cddb.disc,
                                          i_track-1);
      if (t != NULL && t->artist != NULL) {
        playlist_ItemAddInfo( p_item, psz_general_cat,
                          _("Track Artist"), "%s", t->artist );
        playlist_ItemAddInfo( p_item , psz_general_cat,
                          _("Track Title"), "%s",  t->title );
      }
    }

  }
#endif /*HAVE_LIBCDDB*/

  vlc_mutex_unlock( &p_item->input.lock );
}

static int
CDDAFixupPlaylist( access_t *p_access, cdda_data_t *p_cdda, 
		   const char *psz_source, vlc_bool_t b_single_track )
{
  int i;
  playlist_t * p_playlist;
  char       * psz_mrl;
  unsigned int psz_mrl_max = strlen(CDDA_MRL_PREFIX) + strlen(psz_source) +
    strlen("@T") + strlen("100") + 1;

#ifdef HAVE_LIBCDDB
  p_cdda->i_cddb_enabled =
    config_GetInt( p_access, MODULE_STRING "-cddb-enabled" );
  if( b_single_track && !p_cdda->i_cddb_enabled ) return 0;
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
    CDDACreatePlaylistItem(p_access, p_cdda, p_playlist, p_cdda->i_track,
                           psz_mrl, psz_mrl_max, psz_source, PLAYLIST_REPLACE,
                           p_playlist->i_index);
  } else {

    for( i = 1 ; i <= p_cdda->i_tracks ; i++ )
      {
	input_title_t *t = p_cdda->p_title[i-1] = vlc_input_title_New();
	
	asprintf( &t->psz_name, _("Track %i"), i );
	t->i_size = ( p_cdda->p_lsns[i] - p_cdda->p_lsns[i-1] ) *
	  (int64_t)CDIO_CD_FRAMESIZE_RAW;
	
	t->i_length = I64C(1000000) * t->i_size / CDDA_FREQUENCY_SAMPLE / 4;
	CDDACreatePlaylistItem(p_access, p_cdda, p_playlist, i, psz_mrl,
			       psz_mrl_max, psz_source, PLAYLIST_APPEND,
			       PLAYLIST_END);
      }
  }

  return VLC_SUCCESS;
}

/****************************************************************************
 * Public functions
 ****************************************************************************/
int
E_(CDDADebugCB)   ( vlc_object_t *p_this, const char *psz_name,
		    vlc_value_t oldval, vlc_value_t val, void *p_data )
{
  cdda_data_t *p_cdda;

  if (NULL == p_cdda_input) return VLC_EGENERIC;

  p_cdda = (cdda_data_t *)p_cdda_input->p_sys;

  if (p_cdda->i_debug & (INPUT_DBG_CALL|INPUT_DBG_EXT)) {
    msg_Dbg( p_cdda_input, "Old debug (x%0x) %d, new debug (x%0x) %d",
             p_cdda->i_debug, p_cdda->i_debug, val.i_int, val.i_int);
  }
  p_cdda->i_debug = val.i_int;
  return VLC_SUCCESS;
}

int
E_(CDDBEnabledCB)   ( vlc_object_t *p_this, const char *psz_name,
                      vlc_value_t oldval, vlc_value_t val, void *p_data )
{
  cdda_data_t *p_cdda;

  if (NULL == p_cdda_input) return VLC_EGENERIC;

  p_cdda = (cdda_data_t *)p_cdda_input->p_sys;

#ifdef HAVE_LIBCDDB
  if (p_cdda->i_debug & (INPUT_DBG_CALL|INPUT_DBG_EXT)) {
    msg_Dbg( p_cdda_input, "Old CDDB Enabled (x%0x) %d, new (x%0x) %d",
             p_cdda->i_cddb_enabled, p_cdda->i_cddb_enabled,
             val.i_int, val.i_int);
  }
  p_cdda->i_cddb_enabled = val.i_int;
#endif
  return VLC_SUCCESS;
}

/*****************************************************************************
 * Open: open cdda device or image file and initialize structures 
 * for subsequent operations.
 *****************************************************************************/
int
E_(CDDAOpen)( vlc_object_t *p_this )
{
    access_t    *p_access = (access_t*)p_this;
    char *      psz_source = NULL;
    cdda_data_t *p_cdda;
    CdIo        *p_cdio;
    track_t     i_track = 1;
    vlc_bool_t  b_single_track = false;
    int         i_rc = VLC_EGENERIC;

    /* Set where to log errors messages from libcdio. */
    p_cdda_input = p_access;

    /* parse the options passed in command line : */

    if( p_access->psz_path && *p_access->psz_path )
    {
      char *psz_parser = psz_source = strdup( p_access->psz_path );

      while( *psz_parser && *psz_parser != '@' )
	{
	  psz_parser++;
	}
      
      if( *psz_parser == '@' )
	{
	  /* Found options */
	  *psz_parser = '\0';
	  ++psz_parser;
	  
	  if ('T' == *psz_parser || 't' == *psz_parser )
            ++psz_parser;
	  
	  i_track = (int)strtol( psz_parser, NULL, 10 );
	  i_track = i_track ? i_track : 1;
	  b_single_track = true;
	}
    } else {

        /* No device/track given. Continue only when this plugin was 
	   selected */
        if( !p_this->b_force ) return VLC_EGENERIC;

        psz_source = var_CreateGetString( p_this, "cd-audio" );

	if( !psz_source || !*psz_source ) {
        /* Scan for a CD-ROM drive with a CD-DA in it. */
        char **cd_drives =
          cdio_get_devices_with_cap(NULL,  CDIO_FS_AUDIO, false);

        if (NULL == cd_drives || NULL == cd_drives[0] ) {
	  msg_Err( p_access, 
		   _("libcdio couldn't find something with a CD-DA in it") );
          if (cd_drives) cdio_free_device_list(cd_drives);
	  return VLC_EGENERIC;
	}
	
        psz_source = strdup(cd_drives[0]);
        cdio_free_device_list(cd_drives);
      }
    }

    cdio_log_set_handler ( cdio_log_handler );

    /* Open CDDA */
    if( !(p_cdio = cdio_open( psz_source, DRIVER_UNKNOWN )) )
    {
        msg_Warn( p_access, "could not open %s", psz_source );
	goto error2;
    }

    p_cdda = malloc( sizeof(cdda_data_t) );
    if( p_cdda == NULL )
    {
        msg_Err( p_access, "out of memory" );
        free( psz_source );
        return VLC_ENOMEM;
    }
    memset( p_cdda, 0, sizeof(cdda_data_t) );

#ifdef HAVE_LIBCDDB
    cddb_log_set_handler ( cddb_log_handler );
    p_cdda->cddb.disc = NULL;
    p_cdda->i_cddb_enabled =
      config_GetInt( p_access, MODULE_STRING "-cddb-enabled" );
#endif

    p_cdda->b_header = VLC_FALSE;
    p_cdda->p_cdio   = p_cdio;
    p_cdda->i_track  = i_track;
    p_cdda->i_debug  = config_GetInt( p_this, MODULE_STRING "-debug" );

    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_EXT), "%s", psz_source );

    /* Set up p_access */
    p_access->pf_read    = NULL;
    p_access->pf_block   = CDDABlock;
    p_access->pf_control = CDDAControl;
    p_access->pf_seek    = CDDASeek;

    p_access->info.i_update    = 0;
    p_access->info.i_size      = 0;
    p_access->info.i_pos       = 0;
    p_access->info.b_eof       = VLC_FALSE;
    p_access->info.i_title     = 0;
    p_access->info.i_seekpoint = 0;

    p_access->p_sys     = (access_sys_t *) p_cdda;

    /* We read the Table Of Content information */
    i_rc = GetCDInfo( p_access, p_cdda );
    if ( VLC_SUCCESS != i_rc ) goto error;

    CDDAFixupPlaylist( p_access, p_cdda, psz_source, b_single_track );
    
    /* Build a WAV header to put in front of the output data. 
       This gets sent back in the Block (read) routine.
     */
    memset( &p_cdda->waveheader, 0, sizeof(WAVEHEADER) );
    SetWLE( &p_cdda->waveheader.Format, 1 ); /*WAVE_FORMAT_PCM*/
    SetWLE( &p_cdda->waveheader.BitsPerSample, 16);
    p_cdda->waveheader.MainChunkID = VLC_FOURCC('R', 'I', 'F', 'F');
    p_cdda->waveheader.Length = 0;                     /* we just don't know */
    p_cdda->waveheader.ChunkTypeID = VLC_FOURCC('W', 'A', 'V', 'E');
    p_cdda->waveheader.SubChunkID  = VLC_FOURCC('f', 'm', 't', ' ');
    SetDWLE( &p_cdda->waveheader.SubChunkLength, 16);
    SetWLE( &p_cdda->waveheader.Modus, 2);
    SetDWLE( &p_cdda->waveheader.SampleFreq, CDDA_FREQUENCY_SAMPLE);
    SetWLE( &p_cdda->waveheader.BytesPerSample,
            2 /*Modus*/ * 16 /*BitsPerSample*/ / 8 );
    SetDWLE( &p_cdda->waveheader.BytesPerSec,
	     2*16/8 /*BytesPerSample*/ * CDDA_FREQUENCY_SAMPLE );
    p_cdda->waveheader.DataChunkID = VLC_FOURCC('d', 'a', 't', 'a');
    p_cdda->waveheader.DataLength  = 0;    /* we just don't know */

    /* PTS delay */
    var_Create( p_access, MODULE_STRING "-caching", 
		VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );
    return VLC_SUCCESS;

 error:
    cdio_destroy( p_cdda->p_cdio );
    free( p_cdda );
 error2:
    free( psz_source );
    return i_rc;

}

/*****************************************************************************
 * CDDAClose: closes cdda and frees any resources associded with it.
 *****************************************************************************/
void
E_(CDDAClose)( vlc_object_t *p_this )
{
    access_t    *p_access = (access_t *) p_this;
    cdda_data_t *p_cdda   = (cdda_data_t *) p_access->p_sys;
    track_t      i;

    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_EXT), "" );

    /* Remove playlist titles */
    for( i = 0; i < p_cdda->i_tracks; i++ )
    {
        vlc_input_title_Delete( p_cdda->p_title[i] );
    }

    cdio_destroy( p_cdda->p_cdio );

    cdio_log_set_handler (uninit_log_handler);

#ifdef HAVE_LIBCDDB
    cddb_log_set_handler (uninit_log_handler);
    if (p_cdda->i_cddb_enabled)
      cddb_disc_destroy(p_cdda->cddb.disc);
#endif

    free( p_cdda->p_lsns );
    if (p_cdda->psz_mcn) free( p_cdda->psz_mcn );
    free( p_cdda );
    p_cdda_input = NULL;
}

/*****************************************************************************
 * Control: The front-end or vlc engine calls here to ether get
 * information such as meta information or plugin capabilities or to
 * issue miscellaneous "set" requests.
 *****************************************************************************/
static int CDDAControl( access_t *p_access, int i_query, va_list args )
{
    cdda_data_t  *p_cdda = (cdda_data_t *) p_access->p_sys;
    int          *pi_int;
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
	    if ( p_cdda->p_meta ) {
	      *pp_meta = vlc_meta_Duplicate( p_cdda->p_meta );
	      dbg_print( INPUT_DBG_META, "%s", _("Meta copied") );
	    } else 
	      msg_Warn( p_access, _("Tried to copy NULL meta info") );
	    
	    return VLC_SUCCESS;
	  }
	  return VLC_EGENERIC;

        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE: 
	  {
            vlc_bool_t *pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = VLC_TRUE;
            break;
	  }

        /* */
        case ACCESS_GET_MTU:
            pi_int = (int*)va_arg( args, int * );
            *pi_int = CDDA_DATA_ONCE;
            break;

        case ACCESS_GET_PTS_DELAY:
	  { 
	    int64_t *pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = var_GetInteger( p_access, MODULE_STRING "-caching" )
	      * MILLISECONDS_PER_SEC;
            break;
	  }

        /* */
        case ACCESS_SET_PAUSE_STATE:
            break;

        case ACCESS_GET_TITLE_INFO:
	  { input_title_t ***ppp_title;
            ppp_title = (input_title_t***)va_arg( args, input_title_t*** );
            pi_int    = (int*)va_arg( args, int* );
	    *((int*)va_arg( args, int* )) = 1; /* Title offset */

            /* Duplicate track info */
            *pi_int = p_cdda->i_tracks;
            *ppp_title = malloc(sizeof( input_title_t **) * p_cdda->i_tracks );

	    if (!*ppp_title) return VLC_ENOMEM;

            for( i = 0; i < p_cdda->i_tracks; i++ )
            {
	      if ( p_cdda->p_title[i] )
                (*ppp_title)[i] = 
		  vlc_input_title_Duplicate( p_cdda->p_title[i] );
            }
	  }
	  break;

        case ACCESS_SET_TITLE:
            i = (int)va_arg( args, int );
            if( i != p_access->info.i_title )
            {
                /* Update info */
                p_access->info.i_update |=
                    INPUT_UPDATE_TITLE|INPUT_UPDATE_SIZE;
                p_access->info.i_title = i;
                p_access->info.i_size = p_cdda->p_title[i]->i_size;
                p_access->info.i_pos = 0;

                /* Next sector to read */
                p_cdda->i_lsn = p_cdda->p_lsns[i];
            }
            break;

        case ACCESS_SET_SEEKPOINT:
        case ACCESS_SET_PRIVATE_ID_STATE:
            return VLC_EGENERIC;
        default:
	  msg_Warn( p_access, _("unimplemented query in control") );
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}

/*****************************************************************************
  GetCDInfo: 

 Initialize information pertaining to the CD: the number of tracks,
 first track number, LSNs for each track and the leadout. The leadout
 information is stored after the last track. The LSN array is
 0-origin, same as p_access->info.  Add first_track to get what track
 number this is on the CD. Note: libcdio uses the real track number.

 We return the VLC-type status, e.g. VLC_SUCCESS, VLC_ENOMEM, etc.
 *****************************************************************************/
static int
GetCDInfo( access_t *p_access, cdda_data_t *p_cdda ) 
{
    track_t i;
    discmode_t  discmode = CDIO_DISC_MODE_NO_INFO;

    p_cdda->i_tracks       = cdio_get_num_tracks(p_cdda->p_cdio);
    p_cdda->i_first_track  = cdio_get_first_track_num(p_cdda->p_cdio);

    discmode = cdio_get_discmode(p_cdda->p_cdio);
    switch(discmode) {
    case CDIO_DISC_MODE_CD_DA:
    case CDIO_DISC_MODE_CD_MIXED:
      /* These are possible for CD-DA */
      break;
    default:
      /* These are not possible for CD-DA */
      msg_Err( p_access, 
	       _("Disc seems not to be CD-DA. libcdio reports it is %s"),
	       discmode2str[discmode]
	       );
      return VLC_EGENERIC;
    }
    
    p_cdda->p_lsns = malloc( (p_cdda->i_tracks + 1) * sizeof(lsn_t) );

    if( p_cdda->p_lsns == NULL )
      {
        msg_Err( p_access, "out of memory" );
        return VLC_ENOMEM;
      }

    

    /* Fill the p_lsns structure with the track/sector matches.
       Note cdio_get_track_lsn when given num_tracks + 1 will return
       the leadout LSN.
     */
    for( i = 0 ; i <= p_cdda->i_tracks ; i++ )
      {
        (p_cdda->p_lsns)[ i ] = 
	  cdio_get_track_lsn(p_cdda->p_cdio, p_cdda->i_first_track+i);
      }
    
    return VLC_SUCCESS;
}
