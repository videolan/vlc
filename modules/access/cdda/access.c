/*****************************************************************************
 * cddax.c : CD digital audio input module for vlc using libcdio
 *****************************************************************************
 * Copyright (C) 2000,2003 VideoLAN
 * $Id: access.c,v 1.3 2003/11/30 02:41:00 rocky Exp $
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

/* FIXME: This variable is a hack. Would be nice to eliminate. */
static input_thread_t *p_cdda_input = NULL;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  CDDARead         ( input_thread_t *, byte_t *, size_t );
static void CDDASeek         ( input_thread_t *, off_t );
static int  CDDASetArea      ( input_thread_t *, input_area_t * );
static int  CDDASetProgram   ( input_thread_t *, pgrm_descriptor_t * );

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
    int                     i_title = 1;
    cddev_t                 *p_cddev;

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
          
        i_title = (int)strtol( psz_parser, NULL, 10 );
        i_title = i_title ? i_title : 1;
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

    if( !(p_cddev = ioctl_Open( p_this, psz_source )) )
    {
        msg_Warn( p_input, "could not open %s", psz_source );
        free( psz_source );
        return -1;
    }
    free( psz_source );

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
        return -1;
    }

    if( i_title >= p_cdda->i_nb_tracks || i_title < 1 )
        i_title = 1;

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

    CDDAPlay( p_input, i_title);

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
