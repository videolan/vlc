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
#include "callback.h"      /* FIXME - reorganize callback.h, cdda.h better */
#include "cdda.h"          /* private structures. Also #includes vlc things */
#include "info.h"          /* headers for meta info retrieval */
#include <vlc_playlist.h>  /* Has to come *after* cdda.h */
#include "vlc_keys.h"

#include <cdio/cdio.h>
#include <cdio/logging.h>
#include <cdio/cd_types.h>

#include <stdio.h>

/* #ifdef variables below are defined via config.h via #include vlc above. */
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

/* FIXME: This variable is a hack. Would be nice to eliminate. */
access_t *p_cdda_input = NULL;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static block_t *CDDAReadBlocks( access_t * p_access );
static int      CDDASeek( access_t * p_access, int64_t i_pos );
static int      CDDAControl( access_t *p_access, int i_query,
                             va_list args );

static int      CDDAInit( access_t *p_access, cdda_data_t *p_cdda ) ;


/****************************************************************************
 * Private functions
 ****************************************************************************/

/* process messages that originate from libcdio. */
static void
cdio_log_handler (cdio_log_level_t level, const char message[])
{
  cdda_data_t *p_cdda = (cdda_data_t *)p_cdda_input->p_sys;

  if( p_cdda == NULL )
      return;

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
            "The above message had unknown cdio log level",
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
    switch (level)
    {
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

     switch (level)
     {
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
            fprintf(stderr, "UNKNOWN ERROR: %s\n%s %d\n", message,
                            "The above message had unknown cdio log level",
                            level);
    }

    /* gl_default_cdio_log_handler (level, message); */
}

/*****************************************************************************
 * CDDAReadBlocks: reads a group of blocks from the CD-DA and returns
 * an allocated pointer to the data. NULL is returned if no data
 * read. It is also possible if we haven't read a RIFF header in which
 * case one that we creaded during Open/Initialization is returned.
 *****************************************************************************/
static block_t * CDDAReadBlocks( access_t * p_access )
{
    block_t     *p_block;
    cdda_data_t *p_cdda   = (cdda_data_t *) p_access->p_sys;
    int          i_blocks = p_cdda->i_blocks_per_read;

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
    while( p_cdda->i_lsn >= p_cdda->lsn[p_cdda->i_track+1] )
    {
        if( p_cdda->i_track >= p_cdda->i_first_track + p_cdda->i_titles - 1 )
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

    /* Possibly adjust i_blocks so we don't read past the end of a track. */
    if( p_cdda->i_lsn + i_blocks >= p_cdda->lsn[p_cdda->i_track+1] )
    {
        i_blocks = p_cdda->lsn[p_cdda->i_track+1 ] - p_cdda->i_lsn;
    }

    /* Do the actual reading */
    p_block = block_New( p_access, i_blocks * CDIO_CD_FRAMESIZE_RAW );
    if( !p_block)
    {
      msg_Err( p_access, "Cannot get a new block of size: %i",
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

    p_cdda->i_lsn        += i_blocks;
    p_access->info.i_pos += p_block->i_buffer;

    return p_block;
}

/****************************************************************************
 * CDDASeek - change position for subsequent reads. For example, this
 * can happen if the user moves a position slider bar in a GUI.
 ****************************************************************************/
static int CDDASeek( access_t * p_access, int64_t i_pos )
{
    cdda_data_t *p_cdda = (cdda_data_t *) p_access->p_sys;

    p_cdda->i_lsn = p_cdda->lsn[p_cdda->i_track]
                  + (i_pos / CDIO_CD_FRAMESIZE_RAW);
    p_access->info.i_pos = i_pos;

    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_EXT|INPUT_DBG_SEEK),
               "lsn %lu, offset: %lld",
               (long unsigned int) p_cdda->i_lsn, i_pos );
    return VLC_SUCCESS;
}

/****************************************************************************
 * Public functions
 ****************************************************************************/

/*****************************************************************************
 * Open: open cdda device or image file and initialize structures
 *       for subsequent operations.
 *****************************************************************************/
int E_(CDDAOpen)( vlc_object_t *p_this )
{
    access_t    *p_access = (access_t*)p_this;
    char *      psz_source = NULL;
    cdda_data_t *p_cdda    = NULL;
    CdIo        *p_cdio;
    track_t     i_track = 1;
    vlc_bool_t  b_single_track = false;
    int         i_rc = VLC_EGENERIC;

    p_access->p_sys = NULL;

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
    }

    if (!psz_source || !*psz_source)
    {
        /* No device/track given. Continue only when this plugin was
           selected */
        if( !p_this->b_force ) return VLC_EGENERIC;

        psz_source = var_CreateGetString( p_this, "cd-audio" );

        if( !psz_source || !*psz_source )
        {
            /* Scan for a CD-ROM drive with a CD-DA in it. */
            char **cd_drives =
              cdio_get_devices_with_cap(NULL,  CDIO_FS_AUDIO, false);

            if (NULL == cd_drives || NULL == cd_drives[0] )
            {
                msg_Err( p_access,
                     "libcdio couldn't find something with a CD-DA in it" );
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
	if (psz_source) free( psz_source );
	return VLC_EGENERIC;
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
    p_cdda->b_cddb_enabled =
      config_GetInt( p_access, MODULE_STRING "-cddb-enabled" );
#endif

    p_cdda->b_cdtext_enabled =
      config_GetInt( p_access, MODULE_STRING "-cdtext-enabled" );

    p_cdda->b_cdtext_prefer =
      config_GetInt( p_access, MODULE_STRING "-cdtext-prefer" );

    p_cdda->b_header = VLC_FALSE;
    p_cdda->p_cdio   = p_cdio;
    p_cdda->i_titles = 0;
    p_cdda->i_track  = i_track;
    p_cdda->i_debug  = config_GetInt(p_this, MODULE_STRING "-debug");
    p_cdda->i_blocks_per_read
                     = config_GetInt(p_this, MODULE_STRING "-blocks-per-read");

    p_cdda->p_input  = vlc_object_find( p_access, VLC_OBJECT_INPUT,
                                        FIND_PARENT );

    if (0 == p_cdda->i_blocks_per_read)
        p_cdda->i_blocks_per_read = DEFAULT_BLOCKS_PER_READ;

    if ( p_cdda->i_blocks_per_read < MIN_BLOCKS_PER_READ
         || p_cdda->i_blocks_per_read > MAX_BLOCKS_PER_READ )
    {
        msg_Warn( p_cdda_input,
                "Number of blocks (%d) has to be between %d and %d. "
                "Using %d.",
                p_cdda->i_blocks_per_read,
                MIN_BLOCKS_PER_READ, MAX_BLOCKS_PER_READ,
                DEFAULT_BLOCKS_PER_READ );
        p_cdda->i_blocks_per_read = DEFAULT_BLOCKS_PER_READ;
    }

    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_EXT), "%s", psz_source );

    /* Set up p_access */
    p_access->pf_read    = NULL;
    p_access->pf_block   = CDDAReadBlocks;
    p_access->pf_control = CDDAControl;
    p_access->pf_seek    = CDDASeek;

    p_access->info.i_size      = 0;

    p_access->info.i_update    = 0;
    p_access->info.b_eof       = VLC_FALSE;
    p_access->info.i_title     = 0;
    p_access->info.i_seekpoint = 0;

    p_access->p_sys     = (access_sys_t *) p_cdda;

    /* We read the Table Of Content information */
    i_rc = CDDAInit( p_access, p_cdda );
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
    vlc_object_release( p_cdda->p_input );
    return VLC_SUCCESS;

 error:
    cdio_destroy( p_cdda->p_cdio );
    if( psz_source) free( psz_source );
    if( p_cdda ) {
      if ( p_cdda->p_input )
	vlc_object_release( p_cdda->p_input );
      free(p_cdda);
    }
    
    return i_rc;

}

/*****************************************************************************
 * CDDAClose: closes cdda and frees any resources associded with it.
 *****************************************************************************/
void E_(CDDAClose)( vlc_object_t *p_this )
{
    access_t    *p_access = (access_t *) p_this;
    cdda_data_t *p_cdda   = (cdda_data_t *) p_access->p_sys;
    track_t      i;

    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_EXT), "" );

    /* Remove playlist titles */
    for( i = 0; i < p_cdda->i_titles; i++ )
    {
        vlc_input_title_Delete( p_cdda->p_title[i] );
    }

    cdio_destroy( p_cdda->p_cdio );

    cdio_log_set_handler (uninit_log_handler);

#ifdef HAVE_LIBCDDB
    cddb_log_set_handler ((cddb_log_handler_t) uninit_log_handler);
    if (p_cdda->b_cddb_enabled)
      cddb_disc_destroy(p_cdda->cddb.disc);
#endif

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
            if ( p_cdda->p_meta )
            {
                *pp_meta = vlc_meta_Duplicate( p_cdda->p_meta );
                dbg_print( INPUT_DBG_META, "%s", "Meta copied" );
		return VLC_SUCCESS;
            }
            else {
	        msg_Warn( p_access, "tried to copy NULL meta info" );
		return VLC_EGENERIC;
	    }
        }

        case ACCESS_CAN_SEEK:
        case ACCESS_CAN_FASTSEEK:
        case ACCESS_CAN_PAUSE:
        case ACCESS_CAN_CONTROL_PACE:
        {
            vlc_bool_t *pb_bool = (vlc_bool_t*)va_arg( args, vlc_bool_t* );
            *pb_bool = VLC_TRUE;
            return VLC_SUCCESS;;
        }

        /* */
        case ACCESS_GET_MTU:
        {
            pi_int = (int*)va_arg( args, int * );
            *pi_int = p_cdda-> i_blocks_per_read * CDIO_CD_FRAMESIZE_RAW;
            break;
        }

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
        {
            input_title_t ***ppp_title;
            ppp_title = (input_title_t***)va_arg( args, input_title_t*** );
            pi_int    = (int*)va_arg( args, int* );
            *((int*)va_arg( args, int* )) = 1; /* Title offset */

            /* Duplicate title info */
            dbg_print ( INPUT_DBG_EVENT,
                        "GET TITLE: i_tracks %d, i_titles %d",
                        p_cdda->i_tracks, p_cdda->i_titles );
            if( p_cdda->i_titles == 0 )
            {
                *pi_int = 0; ppp_title = NULL;
                return VLC_SUCCESS;
            }
            *pi_int = p_cdda->i_titles;
            *ppp_title = malloc(sizeof( input_title_t **) * p_cdda->i_titles );

            if (!*ppp_title) return VLC_ENOMEM;

            for( i = 0; i < p_cdda->i_titles; i++ )
            {
                if ( p_cdda->p_title[i] )
                   (*ppp_title)[i] =
                          vlc_input_title_Duplicate( p_cdda->p_title[i] );
            }
            break;
        }

        case ACCESS_SET_TITLE:
        {
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
                p_cdda->i_lsn = p_cdda->lsn[p_cdda->i_first_track+i];
            }
            break;
        }

        case ACCESS_SET_SEEKPOINT:
        case ACCESS_SET_PRIVATE_ID_STATE:
            return VLC_EGENERIC;

        default:
            msg_Warn( p_access, "unimplemented query in control" );
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}

/*****************************************************************************
  CDDAInit:

 Initialize information pertaining to the CD: the number of tracks,
 first track number, LSNs for each track and the leadout. The leadout
 information is stored after the last track. The LSN array is
 0-origin, same as p_access->info.  Add first_track to get what track
 number this is on the CD. Note: libcdio uses the real track number.

 On input we assume p_cdda->p_cdio and p_cdda->i_track have been set.

 We return the VLC-type status, e.g. VLC_SUCCESS, VLC_ENOMEM, etc.
 *****************************************************************************/
static int CDDAInit( access_t *p_access, cdda_data_t *p_cdda )
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
               "Disc seems not to be CD-DA. libcdio reports it is %s",
               discmode2str[discmode]
               );
        return VLC_EGENERIC;
    }

    /* Fill the lsn array with the track/sector matches.
       Note cdio_get_track_lsn when given num_tracks + 1 will return
       the leadout LSN.
     */
    for( i = 0 ; i <= p_cdda->i_tracks ; i++ )
    {
        track_t i_track = p_cdda->i_first_track + i;
        (p_cdda->lsn)[ i_track ] = cdio_get_track_lsn(p_cdda->p_cdio, i_track);
    }

    /* Set reading start LSN. */
    p_cdda->i_lsn = p_cdda->lsn[p_cdda->i_track];

    return VLC_SUCCESS;
}
