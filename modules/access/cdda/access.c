/*****************************************************************************
 * cddax.c : CD digital audio input module for vlc using libcdio
 *****************************************************************************
 * Copyright (C) 2000,2003 VideoLAN
 * $Id: access.c,v 1.6 2003/12/01 01:08:42 rocky Exp $
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
#include <vlc/intf.h>

#include <sys/types.h>
#include <cdio/cdio.h>
#include <cdio/cd_types.h>

#include "codecs.h"
#include "vlc_keys.h"

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include <string.h>

#include "cdda.h"

/* how many blocks Open will read in each loop */
#define CDDA_BLOCKS_ONCE 1
#define CDDA_DATA_ONCE   (CDDA_BLOCKS_ONCE * CDIO_CD_FRAMESIZE_RAW)

#define CDDA_MRL_PREFIX "cddax://"

/* FIXME: This variable is a hack. Would be nice to eliminate. */
static input_thread_t *p_cdda_input = NULL;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  CDDARead         ( input_thread_t *, byte_t *, size_t );
static void CDDASeek         ( input_thread_t *, off_t );
static int  CDDASetArea      ( input_thread_t *, input_area_t * );
static int  CDDASetProgram   ( input_thread_t *, pgrm_descriptor_t * );

static int  CDDAFixupPlayList( input_thread_t *p_input, 
			      cdda_data_t *p_cdda, const char *psz_source, 
			      bool play_single_track);

/****************************************************************************
 * Private functions
 ****************************************************************************/

int
E_(DebugCallback)   ( vlc_object_t *p_this, const char *psz_name,
		      vlc_value_t oldval, vlc_value_t val, void *p_data )
{
  cdda_data_t *p_cdda;

  if (NULL == p_cdda_input) return VLC_EGENERIC;
  
  p_cdda = (cdda_data_t *)p_cdda_input->p_access_data;

  if (p_cdda->i_debug & (INPUT_DBG_CALL|INPUT_DBG_EXT)) {
    msg_Dbg( p_cdda_input, "Old debug (x%0x) %d, new debug (x%0x) %d", 
             p_cdda->i_debug, p_cdda->i_debug, val.i_int, val.i_int);
  }
  p_cdda->i_debug = val.i_int;
  return VLC_SUCCESS;
}

/* process messages that originate from libcdio. */
static void
cdio_log_handler (cdio_log_level_t level, const char message[])
{
  cdda_data_t *p_cdda = (cdda_data_t *)p_cdda_input->p_access_data;
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
            _("The above message had unknown vcdimager log level"), 
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
  cdda_data_t *p_cdda = (cdda_data_t *)p_cdda_input->p_access_data;
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
  cdda_data_t *p_cdda = (cdda_data_t *)p_cdda_input->p_access_data;
  switch (level) {
  case CDIO_LOG_DEBUG:
  case CDIO_LOG_INFO:
    if (!(p_cdda->i_debug & (INPUT_DBG_CDIO|INPUT_DBG_CDDB)))
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
 * Open: open cdda
 *****************************************************************************/
int 
E_(Open)( vlc_object_t *p_this )
{
    input_thread_t *        p_input = (input_thread_t *)p_this;
    char *                  psz_orig;
    char *                  psz_parser;
    char *                  psz_source;
    cdda_data_t *           p_cdda;
    int                     i;
    int                     i_track = 1;
    cddev_t                 *p_cddev;
    bool                    play_single_track = false;

    /* Set where to log errors messages from libcdio. */
    p_cdda_input = (input_thread_t *)p_this;

    /* parse the options passed in command line : */
    psz_orig = psz_parser = psz_source = strdup( p_input->psz_name );

    if( !psz_orig )
    {
        return( -1 );
    }

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
	play_single_track = true;
    }

    if( !*psz_source ) {
      /* No source specified, so figure it out. */
      if( !p_input->psz_access ) {
        free( psz_orig );
        return -1;
      }
      psz_source = config_GetPsz( p_input, MODULE_STRING "-device" );
      
      if( !psz_source || 0==strlen(psz_source) ) {
        /* Scan for a CD-ROM drive with a CD-DA in it. */
        char **cd_drives = 
          cdio_get_devices_with_cap(NULL,  CDIO_FS_AUDIO, false);
        if (NULL == cd_drives) return -1;
        if (cd_drives[0] == NULL) {
          cdio_free_device_list(cd_drives);
          return -1;
        }
        psz_source = strdup(cd_drives[0]);
        cdio_free_device_list(cd_drives);
      }
    }

    /* Open CDDA */
    cdio_log_set_handler ( cdio_log_handler );
#ifdef HAVE_LIBCDDB
    cddb_log_set_handler ( cddb_log_handler );
#endif

    if( !(p_cddev = ioctl_Open( p_this, psz_source )) )
    {
        msg_Warn( p_input, "could not open %s", psz_source );
        free( psz_source );
        return -1;
    }

    p_cdda = malloc( sizeof(cdda_data_t) );
    if( p_cdda == NULL )
    {
        msg_Err( p_input, "out of memory" );
        free( psz_source );
        return -1;
    }

    p_cdda->p_cddev        = p_cddev;
    p_cdda->i_debug        = config_GetInt( p_this, MODULE_STRING "-debug" );
    p_input->p_access_data = (void *)p_cdda;

    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_EXT), "%s", psz_source );

    p_input->i_mtu = CDDA_DATA_ONCE;

    /* We read the Table Of Content information */
    p_cdda->i_nb_tracks = ioctl_GetTracksMap( VLC_OBJECT(p_input),
                              p_cdda->p_cddev->cdio, &p_cdda->p_sectors );
    if( p_cdda->i_nb_tracks < 0 )
        msg_Err( p_input, "unable to count tracks" );
    else if( p_cdda->i_nb_tracks <= 0 )
        msg_Err( p_input, "no audio tracks found" );

    if( p_cdda->i_nb_tracks <= 1)
    {
        ioctl_Close( p_cdda->p_cddev );
        free( p_cdda );
	free( psz_source );
        return -1;
    }

    if( i_track >= p_cdda->i_nb_tracks || i_track < 1 )
        i_track = 1;

    /* Set stream and area data */
    vlc_mutex_lock( &p_input->stream.stream_lock );

    /* Initialize ES structures */
    input_InitStream( p_input, 0 );

    /* cdda input method */
    p_input->stream.i_method = INPUT_METHOD_CDDA;

    p_input->stream.b_pace_control = 1;
    p_input->stream.b_seekable = 1;
    p_input->stream.i_mux_rate = 44100 * 4 / 50;

#define area p_input->stream.pp_areas
    for( i = 1 ; i <= p_cdda->i_nb_tracks ; i++ )
    {
        input_AddArea( p_input, i, 1 );

        /* Absolute start offset and size */
        area[i]->i_start =
            (off_t)p_cdda->p_sectors[i-1] * (off_t)CDIO_CD_FRAMESIZE_RAW;
        area[i]->i_size =
            (off_t)(p_cdda->p_sectors[i] - p_cdda->p_sectors[i-1])
            * (off_t)CDIO_CD_FRAMESIZE_RAW;
    }
#undef area

    CDDAPlay( p_input, i_track);

    CDDAFixupPlayList(p_input, p_cdda, psz_source, play_single_track);

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    if( !p_input->psz_demux || !*p_input->psz_demux )
    {
        p_input->psz_demux = "cdda";
    }

    p_input->pf_read = CDDARead;
    p_input->pf_seek = CDDASeek;
    p_input->pf_set_area = CDDASetArea;
    p_input->pf_set_program = CDDASetProgram;

    /* Update default_pts to a suitable value for cdda access */
    p_input->i_pts_delay = config_GetInt( p_input, 
					  MODULE_STRING "-caching" ) * 1000;

    p_cdda->p_intf = intf_Create( p_input, "cddax" );
    intf_RunThread( p_cdda->p_intf );
    free( psz_source );

    return 0;
}

/*****************************************************************************
 * CDDAPlay: Arrange things so we play the specified track.
 * VLC_TRUE is returned if there was no error.
 *****************************************************************************/
vlc_bool_t
CDDAPlay( input_thread_t *p_input, int i_track )
{
  cdda_data_t *p_cdda = (cdda_data_t *) p_input->p_access_data;

  if( i_track >= p_cdda->i_nb_tracks || i_track < 1 )
    return VLC_FALSE;

  CDDASetArea( p_input, p_input->stream.pp_areas[i_track] );
  return VLC_TRUE;
}

/*****************************************************************************
 * CDDAClose: closes cdda
 *****************************************************************************/
void 
E_(Close)( vlc_object_t *p_this )
{
    input_thread_t *   p_input = (input_thread_t *)p_this;
    cdda_data_t *p_cdda = (cdda_data_t *)p_input->p_access_data;

    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_EXT), "" );
    ioctl_Close( p_cdda->p_cddev );

    cdio_log_set_handler (uninit_log_handler);

#ifdef HAVE_LIBCDDB
    cddb_log_set_handler (uninit_log_handler);
    if (p_cdda->i_cddb_enabled)
      cddb_disc_destroy(p_cdda->cddb.disc);
#endif

    free( p_cdda );
    p_cdda_input = NULL;
}

/*****************************************************************************
 * CDDARead: reads from the CDDA into PES packets.
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, otherwise the number of
 * bytes.
 *****************************************************************************/
static int CDDARead( input_thread_t * p_input, byte_t * p_buffer,
                     size_t i_len )
{
    cdda_data_t *           p_cdda;
    int                     i_blocks;
    int                     i_index;
    int                     i_read;

    p_cdda = (cdda_data_t *)p_input->p_access_data;

    i_read = 0;

    /* Compute the number of blocks we have to read */

    i_blocks = i_len / CDIO_CD_FRAMESIZE_RAW;

    for ( i_index = 0; i_index < i_blocks; i_index++ )
    {

      if (cdio_read_audio_sector(p_cdda->p_cddev->cdio, p_buffer, 
                                 p_cdda->i_sector) != 0)
        {
          msg_Err( p_input, "could not read sector %d", p_cdda->i_sector );
          return -1;
        }

        p_cdda->i_sector ++;
        if ( p_cdda->i_sector == p_cdda->p_sectors[p_cdda->i_track + 1] )
        {
            input_area_t *p_area;

	    dbg_print( (INPUT_DBG_LSN|INPUT_DBG_CALL), 
		       "end of track, cur: %u", p_cdda->i_sector );

            if ( p_cdda->i_track >= p_cdda->i_nb_tracks - 1 )
                return 0; /* EOF */

            vlc_mutex_lock( &p_input->stream.stream_lock );
            p_area = p_input->stream.pp_areas[
                    p_input->stream.p_selected_area->i_id + 1 ];

            p_area->i_part = 1;
            CDDASetArea( p_input, p_area );
            vlc_mutex_unlock( &p_input->stream.stream_lock );
        }
        i_read += CDIO_CD_FRAMESIZE_RAW;
    }

    if ( i_len % CDIO_CD_FRAMESIZE_RAW ) /* this should not happen */
    {
        msg_Err( p_input, "must read full sectors" );
    }

    return i_read;
}

/*****************************************************************************
 * CDDASetProgram: Does nothing since a CDDA is mono_program
 *****************************************************************************/
static int CDDASetProgram( input_thread_t * p_input,
                           pgrm_descriptor_t * p_program)
{
    cdda_data_t * p_cdda= (cdda_data_t *) p_input->p_access_data;
    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_EXT), "" );
    return 0;
}

/*****************************************************************************
 * CDDASetArea: initialize input data for title x.
 * It should be called for each user navigation request.
 ****************************************************************************/
static int CDDASetArea( input_thread_t * p_input, input_area_t * p_area )
{
    cdda_data_t *p_cdda = (cdda_data_t*) p_input->p_access_data;
    vlc_value_t val;

    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_EXT), "");

    /* we can't use the interface slider until initilization is complete */
    p_input->stream.b_seekable = 0;

    if( p_area != p_input->stream.p_selected_area )
    {
        /* Change the default area */
        p_input->stream.p_selected_area = p_area;

        /* Change the current track */
        p_cdda->i_track = p_area->i_id - 1;
        p_cdda->i_sector = p_cdda->p_sectors[p_cdda->i_track];

        /* Update the navigation variables without triggering a callback */
        val.i_int = p_area->i_id;
        var_Change( p_input, "title", VLC_VAR_SETVALUE, &val, NULL );
    }

    p_cdda->i_sector = p_cdda->p_sectors[p_cdda->i_track];

    p_input->stream.p_selected_area->i_tell =
        (off_t)p_cdda->i_sector * (off_t)CDIO_CD_FRAMESIZE_RAW
         - p_input->stream.p_selected_area->i_start;

    /* warn interface that something has changed */
    p_input->stream.b_seekable = 1;
    p_input->stream.b_changed = 1;

    return 0;
}

/****************************************************************************
 * CDDASeek
 ****************************************************************************/
static void CDDASeek( input_thread_t * p_input, off_t i_off )
{
    cdda_data_t * p_cdda;

    p_cdda = (cdda_data_t *) p_input->p_access_data;

    p_cdda->i_sector = p_cdda->p_sectors[p_cdda->i_track]
                       + i_off / (off_t)CDIO_CD_FRAMESIZE_RAW;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.p_selected_area->i_tell =
        (off_t)p_cdda->i_sector * (off_t)CDIO_CD_FRAMESIZE_RAW
         - p_input->stream.p_selected_area->i_start;

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_EXT|INPUT_DBG_SEEK),
    "sector %ud, offset: %lld, i_tell: %lld",  p_cdda->i_sector, i_off, 
               p_input->stream.p_selected_area->i_tell );

}

static 
char * secs2TimeStr( int64_t i_sec )
{
  int h = i_sec / 3600;
  int m = ( i_sec / 60 ) % 60;
  int s = i_sec % 60;
  static char buffer[20];

  snprintf(buffer, 20, "%d:%2.2d:%2.2d", h, m, s);
  return buffer;
}

#define meta_info_add_str(title, str) \
  if ( str ) {						\
    printf("field %s str %s\n", title, str);		\
    input_AddInfo( p_cat, _(title), "%s", str );	\
  }


static void InformationCreate( input_thread_t *p_input  )
{
  cdda_data_t *p_cdda = (cdda_data_t *) p_input->p_access_data;
  input_info_category_t *p_cat;
  int use_cddb = p_cdda->i_cddb_enabled;
  
  p_cat = input_InfoCategory( p_input, "General" );

#ifdef HAVE_LIBCDDB
  if (use_cddb) {
    
    if( p_cdda->cddb.disc->length > 1000 )
      {
	int64_t i_sec = (int64_t) p_cdda->cddb.disc->length;
	input_AddInfo( p_cat, _("Duration"), "%s", secs2TimeStr( i_sec ) );
      } else 
      {
	use_cddb = 0;
      }
    
    meta_info_add_str( "Title", p_cdda->cddb.disc->title );
    meta_info_add_str( "Artist", p_cdda->cddb.disc->artist );
    meta_info_add_str( "Genre", p_cdda->cddb.disc->genre );
    meta_info_add_str( "Extended Data", p_cdda->cddb.disc->ext_data );
    {
      char year[5];
      if (p_cdda->cddb.disc->year != 0) {
	snprintf(year, 5, "%d", p_cdda->cddb.disc->year);
	meta_info_add_str( "Year", year );
      }
      if ( p_cdda->cddb.disc->discid ) {
	input_AddInfo( p_cat, _("CDDB Disc ID"), "%x", 
		       p_cdda->cddb.disc->discid );
      }
      
      if ( p_cdda->cddb.disc->category != CDDB_CAT_INVALID ) {
	input_AddInfo( p_cat, _("CDDB Disc Category"), "%s", 
		       CDDB_CATEGORY[p_cdda->cddb.disc->category] );
      }
      
    }
  }

#endif /*HAVE_LIBCDDB*/

  if (!use_cddb)
  {
    lba_t i_sec = cdio_get_track_lba(p_cdda->p_cddev->cdio, 
				       CDIO_CDROM_LEADOUT_TRACK) ;

    i_sec /= CDIO_CD_FRAMES_PER_SEC;

    if ( i_sec > 1000 )
      input_AddInfo( p_cat, _("Duration"), "%s", secs2TimeStr( i_sec ) );
  }
}


#ifdef HAVE_LIBCDDB

#define free_and_dup(var, val) \
  if (var) free(var);	       \
  if (val) var=strdup(val);	       
  

static void
GetCDDBInfo( const input_thread_t *p_input, cdda_data_t *p_cdda )
{

  dbg_print( (INPUT_DBG_CALL|INPUT_DBG_EXT), "" );

  if (config_GetInt( p_input, MODULE_STRING "-cddb-enabled" )) {
    int i, i_matches;
    cddb_conn_t  *conn = cddb_new();
    const CdIo *cdio = p_cdda->p_cddev->cdio;
    
    
    cddb_log_set_handler (uninit_log_handler);

    if (!conn) {
      msg_Warn( p_input, "unable to initialize libcddb" );
      goto cddb_destroy;
    }
    
    cddb_set_email_address( conn, 
			    config_GetPsz( p_input, 
					   MODULE_STRING "-cddb-email") );
    
    cddb_set_server_name( conn, 
			  config_GetPsz( p_input, 
					 MODULE_STRING "-cddb-server") );

    cddb_set_server_port(conn, 
			  config_GetInt( p_input, 
					 MODULE_STRING "-cddb-port") );

    /* Set the location of the local CDDB cache directory.
       The default location of this directory is */

    if (!config_GetInt( p_input, MODULE_STRING "-cddb-enable-cache" )) 
      cddb_cache_disable(conn);

    cddb_cache_set_dir(conn, 
		       config_GetPsz( p_input, 
				      MODULE_STRING "-cddb-cachedir") );

    cddb_set_timeout(conn, 
		     config_GetInt( p_input, MODULE_STRING "-cddb-timeout") );


    if (config_GetInt( p_input, MODULE_STRING "-cddb-httpd" )) {
      cddb_http_enable(conn);
    } else
      cddb_http_disable(conn);
    
    p_cdda->cddb.disc = cddb_disc_new();
    if (!p_cdda->cddb.disc) {
      msg_Err( p_input, "Unable to create CDDB disc structure." );
      goto cddb_end;
    }

    for(i = 1; i <= p_cdda->i_nb_tracks; i++) {
      cddb_track_t *t = cddb_track_new(); 
      t->frame_offset = cdio_get_track_lba(cdio, i);
      cddb_disc_add_track(p_cdda->cddb.disc, t);
    }
    
    p_cdda->cddb.disc->length = 
      cdio_get_track_lba(cdio, CDIO_CDROM_LEADOUT_TRACK) 
      / CDIO_CD_FRAMES_PER_SEC;


    if (!cddb_disc_calc_discid(p_cdda->cddb.disc)) {
      msg_Err( p_input, "CDDB disc calc failed" );
      goto cddb_destroy;
    }

    i_matches = cddb_query(conn, p_cdda->cddb.disc);
    if (i_matches > 0) {
      if (i_matches > 1)
	msg_Warn( p_input, "Found %d matches in CDDB. Using first one.", 
		  i_matches);
      cddb_read(conn, p_cdda->cddb.disc);

      if (p_cdda->i_debug & INPUT_DBG_CDDB) 
	cddb_disc_print(p_cdda->cddb.disc);

    } else {
      msg_Warn( p_input, "CDDB error: %s", cddb_error_str(errno));
    }

  cddb_destroy:
    cddb_destroy(conn);
  }
  cddb_end: ;
}
#endif /*HAVE_LIBCDDB*/

#define add_format_str_info(val)			\
  {							\
    const char *str = val;				\
    unsigned int len;					\
    if (val != NULL) {					\
      len=strlen(str);					\
      if (len != 0) {					\
	strncat(tp, str, TEMP_STR_LEN-(tp-temp_str));	\
	tp += len;					\
      }							\
      saw_control_prefix = false;			\
    }							\
  }

#define add_format_num_info(val, fmt)			\
  {							\
    char num_str[10];					\
    unsigned int len;                                   \
    sprintf(num_str, fmt, val);				\
    len=strlen(num_str);                                \
    if (len != 0) {					\
      strncat(tp, num_str, TEMP_STR_LEN-(tp-temp_str));	\
      tp += len;					\
    }							\
    saw_control_prefix = false;				\
  }

/*!
   Take a format string and expand escape sequences, that is sequences that
   begin with %, with information from the current CD. 
   The expanded string is returned. Here is a list of escape sequences:

   %a : The album artist
   %A : The album information 
   %C : Category
   %I : CDDB disk ID
   %G : Genre
   %M : The current MRL
   %m : The CD-DA Media Catalog Number (MCN)
   %p : The artist/performer/composer in the track
   %T : The track number
   %s : Number of seconds in this track
   %t : The name
   %Y : The year 19xx or 20xx
   %% : a %
*/
static char *
CDDAFormatStr(const input_thread_t *p_input, cdda_data_t *p_cdda,
	      const char format_str[], const char *mrl, int i_track)
{
#define TEMP_STR_SIZE 256
#define TEMP_STR_LEN (TEMP_STR_SIZE-1)
  static char    temp_str[TEMP_STR_SIZE];
  size_t i;
  char * tp = temp_str;
  bool saw_control_prefix = false;
  size_t format_len = strlen(format_str);

  bzero(temp_str, TEMP_STR_SIZE);

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
      add_format_str_info(p_cdda->cddb.disc->artist);
      break;
    case 'A':
      if (!p_cdda->i_cddb_enabled) goto not_special;
      add_format_str_info(p_cdda->cddb.disc->title);
      break;
    case 'C':
      if (!p_cdda->i_cddb_enabled) goto not_special;
      add_format_str_info(CDDB_CATEGORY[p_cdda->cddb.disc->category]);
      break;
    case 'G':
      if (!p_cdda->i_cddb_enabled) goto not_special;
      add_format_str_info(p_cdda->cddb.disc->genre);
      break;
    case 'I':
      if (!p_cdda->i_cddb_enabled) goto not_special;
      add_format_num_info(p_cdda->cddb.disc->discid, "%x");
      break;
    case 'Y':
      if (!p_cdda->i_cddb_enabled) goto not_special;
      add_format_num_info(p_cdda->cddb.disc->year, "%5d");
      break;
    case 't':
      if (p_cdda->i_cddb_enabled) {
	cddb_track_t *t=cddb_disc_get_track(p_cdda->cddb.disc, 
					    i_track-1);
	if (t != NULL && t->title != NULL) 
	  add_format_str_info(t->title);
      } else goto not_special;
      break;
    case 'p':
      if (p_cdda->i_cddb_enabled) {
	cddb_track_t *t=cddb_disc_get_track(p_cdda->cddb.disc, 
					    i_track-1);
	if (t != NULL && t->artist != NULL) 
	  add_format_str_info(t->artist);
      } else goto not_special;
      break;
    case 's':
      if (p_cdda->i_cddb_enabled) {
	cddb_track_t *t=cddb_disc_get_track(p_cdda->cddb.disc, 
					    i_track-1);
	if (t != NULL && t->length > 1000 ) {
	  add_format_str_info(secs2TimeStr(t->length));
	}
      } else goto not_special;
      break;

#endif

    case 'M':
      add_format_str_info(mrl);
      break;
#if FINISHED
    case 'm':
      add_format_str_info(p_cdda->mcn);
      break;
#endif
    case 'T':
      add_format_num_info(i_track, "%d");
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
CDDACreatePlayListItem(const input_thread_t *p_input, cdda_data_t *p_cdda, 
		       playlist_t *p_playlist, unsigned int i_track, 
		       char *psz_mrl, int psz_mrl_max, 
		       const char *psz_source, int playlist_operation, 
		       unsigned int i_pos)
{
  mtime_t i_duration = 
    (p_cdda->p_sectors[i_track] - p_cdda->p_sectors[i_track-1]) 
    * 1000 / CDIO_CD_FRAMES_PER_SEC;
  char *p_title;
  
  snprintf(psz_mrl, psz_mrl_max, "%s%s@T%u", 
	   CDDA_MRL_PREFIX, psz_source, i_track);

  p_title = CDDAFormatStr(p_input, p_cdda, 
			  config_GetPsz( p_input, 
					 MODULE_STRING "-title-format" ),
			  psz_mrl, i_track);

  playlist_AddExt( p_playlist, psz_mrl, p_title, i_duration, 
		   0, 0, playlist_operation, i_pos );
}

static int
CDDAFixupPlayList( input_thread_t *p_input, cdda_data_t *p_cdda, 
		  const char *psz_source, bool play_single_track) 
{
  int i;
  playlist_t * p_playlist;
  char       * psz_mrl;
  unsigned int psz_mrl_max = strlen(CDDA_MRL_PREFIX) + strlen(psz_source) + 
    strlen("@T") + strlen("100") + 1;

#ifdef HAVE_LIBCDDB
  p_cdda->i_cddb_enabled = 
    config_GetInt( p_input, MODULE_STRING "-cddb-enabled" );
#endif
  
  if (play_single_track && !p_cdda->i_cddb_enabled) return 0;

  psz_mrl = malloc( psz_mrl_max );

  if( psz_mrl == NULL )
    {
      msg_Warn( p_input, "out of memory" );
      return -1;
    }

  p_playlist = (playlist_t *) vlc_object_find( p_input, VLC_OBJECT_PLAYLIST,
					       FIND_ANYWHERE );
  if( !p_playlist )
    {
      msg_Warn( p_input, "can't find playlist" );
      free(psz_mrl);
      return -1;
    }

#ifdef HAVE_LIBCDDB
  if (p_cdda->i_cddb_enabled)
    GetCDDBInfo(p_input, p_cdda);
  else 
    p_cdda->cddb.disc = NULL;
#endif

  InformationCreate(p_input);
  
  if (play_single_track) {
    /* May fill out more information when the playlist user interface becomes
       more mature.
     */
    CDDACreatePlayListItem(p_input, p_cdda, p_playlist, p_cdda->i_track+1, 
			   psz_mrl, psz_mrl_max, psz_source, PLAYLIST_REPLACE, 
			   p_playlist->i_index);
  } else {
  
    playlist_Delete( p_playlist, p_playlist->i_index);

    for( i = 1 ; i <= p_cdda->i_nb_tracks ; i++ )
      {
	CDDACreatePlayListItem(p_input, p_cdda, p_playlist, i, psz_mrl, 
			       psz_mrl_max, psz_source, PLAYLIST_APPEND, 
			       PLAYLIST_END);

      }

    playlist_Command( p_playlist, PLAYLIST_GOTO, 0 );

  }
    
  vlc_object_release( p_playlist );
  free(psz_mrl);
  return 0;
}
