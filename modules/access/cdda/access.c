/*****************************************************************************
 * access.c : CD digital audio input module for vlc using libcdio
 *****************************************************************************
 * Copyright (C) 2000, 2003, 2004, 2005 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "callback.h"      /* FIXME - reorganize callback.h, cdda.h better */
#include "cdda.h"          /* private structures. Also #includes vlc things */
#include "info.h"          /* headers for meta info retrieval */
#include "access.h"
#include "vlc_keys.h"
#include <vlc_interface.h>

#include <cdio/cdio.h>
#include <cdio/logging.h>
#include <cdio/cd_types.h>


/* #ifdef variables below are defined via config.h via #include vlc above. */

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

/* FIXME: This variable is a hack. Would be nice to eliminate. */
access_t *p_cdda_input = NULL;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static ssize_t  CDDARead( access_t *, uint8_t *, size_t );
static block_t *CDDAReadBlocks( access_t * p_access );
static int      CDDASeek( access_t * p_access, int64_t i_pos );
static int      CDDAControl( access_t *p_access, int i_query,
                             va_list args );

static int      CDDAInit( access_t *p_access, cdda_data_t *p_cdda ) ;


/****************************************************************************
 * Private functions
 ****************************************************************************/

/* process messages that originate from libcdio.
   called by CDDAOpen
*/
static void
cdio_log_handler( cdio_log_level_t level, const char message[] )
{
    cdda_data_t *p_cdda = (cdda_data_t *)p_cdda_input->p_sys;

    if( p_cdda == NULL )
        return;

    switch( level )
    {
        case CDIO_LOG_DEBUG:
        case CDIO_LOG_INFO:
            if (p_cdda->i_debug & INPUT_DBG_CDIO)
            msg_Dbg( p_cdda_input, "%s", message);
            break;
        case CDIO_LOG_WARN:
            msg_Warn( p_cdda_input, "%s", message);
            break;
        case CDIO_LOG_ERROR:
        case CDIO_LOG_ASSERT:
            msg_Err( p_cdda_input, "%s", message);
            break;
        default:
            msg_Warn( p_cdda_input, "%s\n%s %d", message,
                    "the above message had unknown cdio log level",
                    level);
            break;
    }
}

#ifdef HAVE_LIBCDDB
/*! This routine is called by libcddb routines on error.
   called by CDDAOpen
*/
static void
cddb_log_handler( cddb_log_level_t level, const char message[] )
{
    cdda_data_t *p_cdda = (cdda_data_t *)p_cdda_input->p_sys;
    switch( level )
    {
        case CDDB_LOG_DEBUG:
        case CDDB_LOG_INFO:
            if( !(p_cdda->i_debug & INPUT_DBG_CDDB) )
                return;
        /* Fall through if to warn case */
        default:
            cdio_log_handler( level, message );
            break;
    }
}
#endif /*HAVE_LIBCDDB*/


/*! This routine is when vlc is not fully set up (before full initialization)
  or is not around (before finalization).
*/
static void
uninit_log_handler( cdio_log_level_t level, const char message[] )
{
    cdda_data_t *p_cdda = NULL;

    if( p_cdda_input )
        p_cdda = (cdda_data_t *)p_cdda_input->p_sys;

     switch( level )
     {
        case CDIO_LOG_DEBUG:
        case CDIO_LOG_INFO:
            if( !p_cdda || !(p_cdda->i_debug & (INPUT_DBG_CDIO|INPUT_DBG_CDDB)) )
                return;
        /* Fall through if to warn case */
        case CDIO_LOG_WARN:
            fprintf( stderr, "WARN: %s\n", message );
            break;
        case CDIO_LOG_ERROR:
            fprintf( stderr, "ERROR: %s\n", message );
            break;
        case CDIO_LOG_ASSERT:
            fprintf( stderr, "ASSERT ERROR: %s\n", message );
            break;
        default:
            fprintf( stderr, "UNKNOWN ERROR: %s\n%s %d\n", message,
                            "The above message had unknown cdio log level",
                            level );
        break;
    }
    /* gl_default_cdio_log_handler (level, message); */
}

/* Only used in audio control mode. Gets the current LSN from the
   CD-ROM drive. */
static int64_t get_audio_position ( access_t *p_access )
{
    cdda_data_t *p_cdda   = (cdda_data_t *) p_access->p_sys;
    lsn_t i_offset;

#if LIBCDIO_VERSION_NUM >= 73
    if( p_cdda->b_audio_ctl )
    {
        cdio_subchannel_t sub;
        CdIo_t *p_cdio = p_cdda->p_cdio;

        if( DRIVER_OP_SUCCESS == cdio_audio_read_subchannel(p_cdio, &sub) )
        {
            if( (sub.audio_status != CDIO_MMC_READ_SUB_ST_PAUSED) &&
                (sub.audio_status != CDIO_MMC_READ_SUB_ST_PLAY) )
                return CDIO_INVALID_LSN;

            if( ! p_cdda->b_nav_mode )
            {
                i_offset = cdio_msf_to_lba( (&sub.abs_addr) );
            }
            else
            {
                i_offset = cdio_msf_to_lba( (&sub.rel_addr) );
            }
        }
        else
        {
            i_offset = p_cdda->i_lsn;
        }
    }
    else
    {
        i_offset = p_cdda->i_lsn;
    }
#else
        i_offset = p_cdda->i_lsn;
#endif
    return i_offset;
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

    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_EXT|INPUT_DBG_LSN),
                "called i_lsn: %d i_pos: %lld, size: %lld",
                p_cdda->i_lsn, p_access->info.i_pos, p_access->info.i_size );

    /* Check end of file */
    if( p_access->info.b_eof )
        return NULL;

    if( !p_cdda->b_header )
      {
        /* Return only the dummy RIFF header we created in Open/Init */
        p_block = block_New( p_access, sizeof( WAVEHEADER ) );
        memcpy( p_block->p_buffer, &p_cdda->waveheader, sizeof(WAVEHEADER) );
        p_cdda->b_header = true;
        return p_block;
    }

    /* Check end of track */
    while( p_cdda->i_lsn > cdio_get_track_last_lsn(p_cdda->p_cdio,
           p_cdda->i_track) )
    {
        bool go_on;

        if( p_cdda->b_nav_mode )
            go_on = p_cdda->i_lsn > p_cdda->last_disc_frame;
        else
            go_on = p_cdda->i_track >= p_cdda->i_first_track+p_cdda->i_titles-1 ;

        if( go_on )
        {
            dbg_print( (INPUT_DBG_LSN), "EOF");
                        p_access->info.b_eof = true;
            return NULL;
        }

        p_access->info.i_update |= INPUT_UPDATE_TITLE | INPUT_UPDATE_META;
        p_access->info.i_title++;
        p_cdda->i_track++;

        if( p_cdda-> b_nav_mode )
        {
            char *psz_title = CDDAFormatTitle( p_access, p_cdda->i_track );
            input_Control( p_cdda->p_input, INPUT_SET_NAME, psz_title );
            free(psz_title);
        }
        else
        {
            p_access->info.i_size =
                    p_cdda->p_title[p_access->info.i_title]->i_size;
            p_access->info.i_pos = 0;
            p_access->info.i_update |= INPUT_UPDATE_SIZE;
        }
    }

    /* Possibly adjust i_blocks so we don't read past the end of a track. */
    if( p_cdda->i_lsn + i_blocks >=
        cdio_get_track_lsn(p_cdda->p_cdio, p_cdda->i_track+1) )
    {
        i_blocks = cdio_get_track_lsn( p_cdda->p_cdio, p_cdda->i_track+1 )
                    - p_cdda->i_lsn;
    }

    /* Do the actual reading */
    p_block = block_New( p_access, i_blocks * CDIO_CD_FRAMESIZE_RAW );
    if( !p_block)
    {
        msg_Err( p_access, "cannot get a new block of size: %i",
                i_blocks * CDIO_CD_FRAMESIZE_RAW );
        intf_UserFatal( p_access, false, _("CD reading failed"),
                        _("VLC could not get a new block of size: %i."),
                        i_blocks * CDIO_CD_FRAMESIZE_RAW );
        return NULL;
    }

    {
#if LIBCDIO_VERSION_NUM >= 72
        driver_return_code_t rc = DRIVER_OP_SUCCESS;

        if( p_cdda->e_paranoia && p_cdda->paranoia )
        {
            int i;
            for( i = 0; i < i_blocks; i++ )
            {
                int16_t *p_readbuf = cdio_paranoia_read( p_cdda->paranoia, NULL );
                char *psz_err = cdio_cddap_errors( p_cdda->paranoia_cd );
                char *psz_mes = cdio_cddap_messages( p_cdda->paranoia_cd );

                if( psz_mes || psz_err )
                    msg_Err( p_access, "%s%s\n", psz_mes ? psz_mes: "",
                             psz_err ? psz_err: "" );

                free( psz_err );
                free( psz_mes );
                if( !p_readbuf )
                {
                    msg_Err( p_access, "paranoia read error on frame %i\n",
                    p_cdda->i_lsn+i );
                }
                else
                    memcpy( p_block->p_buffer + i * CDIO_CD_FRAMESIZE_RAW,
                            p_readbuf, CDIO_CD_FRAMESIZE_RAW );
            }
        }
        else
        {
            rc = cdio_read_audio_sectors( p_cdda->p_cdio, p_block->p_buffer,
                                          p_cdda->i_lsn, i_blocks );
#else
#define DRIVER_OP_SUCCESS 0
            int rc;
            rc = cdio_read_audio_sectors( p_cdda->p_cdio, p_block->p_buffer,
                                          p_cdda->i_lsn, i_blocks);
#endif
        }
        if( rc != DRIVER_OP_SUCCESS )
        {
            msg_Err( p_access, "could not read %d sectors starting from %lu",
                     i_blocks, (long unsigned int) p_cdda->i_lsn );
            block_Release( p_block );

            /* If we had problems above, assume the problem is with
                the first sector of the read and set to skip it.  In
                the future libcdio may have cdparanoia support.
            */
            p_cdda->i_lsn++;
            p_access->info.i_pos += CDIO_CD_FRAMESIZE_RAW;
            return NULL;
        }
    }

    p_cdda->i_lsn        += i_blocks;
    p_access->info.i_pos += i_blocks * CDIO_CD_FRAMESIZE_RAW;

    return p_block;
}

/*****************************************************************************
 * CDDARead: Handler for audio control reads the CD-DA.
 *****************************************************************************/
static ssize_t
CDDARead( access_t * p_access, uint8_t *p_buffer, size_t i_len )
{
    cdda_data_t *p_cdda   = (cdda_data_t *) p_access->p_sys;

    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_EXT|INPUT_DBG_LSN),
               "called lsn: %d pos: %lld, size: %lld",
                p_cdda->i_lsn, p_access->info.i_pos, p_access->info.i_size);

    /* Check end of file */
    if( p_access->info.b_eof )
        return 0;

    {
        lsn_t i_lsn = get_audio_position(p_access);
        if( CDIO_INVALID_LSN == i_lsn )
        {
            dbg_print( (INPUT_DBG_LSN), "invalid lsn" );
            memset( p_buffer, 0, i_len );
            return i_len;
        }

        p_cdda->i_lsn = i_lsn;
        p_access->info.i_pos = p_cdda->i_lsn * CDIO_CD_FRAMESIZE_RAW;
    }

    dbg_print( (INPUT_DBG_LSN), "updated lsn: %d", p_cdda->i_lsn );

    /* Check end of track */
    while( p_cdda->i_lsn > cdio_get_track_last_lsn( p_cdda->p_cdio,
                                                    p_cdda->i_track) )
    {
        if( p_cdda->i_track >= p_cdda->i_first_track + p_cdda->i_titles - 1 )
        {
            dbg_print( (INPUT_DBG_LSN), "EOF");
            p_access->info.b_eof = true;
            return 0;
        }
        p_access->info.i_update |= INPUT_UPDATE_TITLE;
        p_access->info.i_title++;
        p_cdda->i_track++;

        if( p_cdda-> b_nav_mode )
        {
            char *psz_title = CDDAFormatTitle( p_access, p_cdda->i_track );
            input_Control( p_cdda->p_input, INPUT_SET_NAME, psz_title );
            free( psz_title );
        }
        else
        {
            p_access->info.i_size =
                p_cdda->p_title[p_access->info.i_title]->i_size;
            p_access->info.i_pos = 0;
            p_access->info.i_update |= INPUT_UPDATE_SIZE;
        }
    }
    memset( p_buffer, 0, i_len );
    return i_len;
}

/*! Pause CD playing via audio control */
static bool cdda_audio_pause( CdIo_t *p_cdio )
{
    bool b_ok = true;
#if LIBCDIO_VERSION_NUM >= 73
    cdio_subchannel_t sub;

    if( DRIVER_OP_SUCCESS == cdio_audio_read_subchannel( p_cdio, &sub ) )
    {
        if( sub.audio_status == CDIO_MMC_READ_SUB_ST_PLAY )
        {
            b_ok = DRIVER_OP_SUCCESS == cdio_audio_pause(p_cdio);
        }
    }
    else
        b_ok = false;
#endif
    return b_ok;
}

#if LIBCDIO_VERSION_NUM >= 73
/*! play CD using audio controls */
static driver_return_code_t
cdda_audio_play( CdIo_t *p_cdio, lsn_t start_lsn, lsn_t end_lsn )
{
    msf_t start_msf;
    msf_t last_msf;
    cdio_lsn_to_msf( start_lsn, &start_msf );
    cdio_lsn_to_msf( end_lsn, &last_msf );
    cdda_audio_pause( p_cdio );
    return cdio_audio_play_msf( p_cdio, &start_msf, &last_msf );
}
#endif

/****************************************************************************
 * CDDASeek - change position for subsequent reads. For example, this
 * can happen if the user moves a position slider bar in a GUI.
 ****************************************************************************/
static int CDDASeek( access_t * p_access, int64_t i_pos )
{
    cdda_data_t *p_cdda = (cdda_data_t *) p_access->p_sys;

    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_EXT|INPUT_DBG_SEEK),
               "lsn %lu, offset: %lld",
               (long unsigned int) p_cdda->i_lsn, i_pos );

    p_cdda->i_lsn = (i_pos / CDIO_CD_FRAMESIZE_RAW);

#if LIBCDIO_VERSION_NUM >= 72
    if( p_cdda->e_paranoia && p_cdda->paranoia )
         cdio_paranoia_seek( p_cdda->paranoia, p_cdda->i_lsn, SEEK_SET );
#endif

#if LIBCDIO_VERSION_NUM >= 73
    if( p_cdda->b_audio_ctl )
    {
        track_t i_track = cdio_get_track( p_cdda->p_cdio, p_cdda->i_lsn );
        lsn_t i_last_lsn;

        if( p_cdda->b_nav_mode )
            i_last_lsn = p_cdda->last_disc_frame;
        else
            i_last_lsn = cdio_get_track_last_lsn( p_cdda->p_cdio, i_track );

        cdda_audio_play( p_cdda->p_cdio, p_cdda->i_lsn, i_last_lsn );
    }
#endif

    if( ! p_cdda->b_nav_mode )
        p_cdda->i_lsn += cdio_get_track_lsn( p_cdda->p_cdio, p_cdda->i_track );

    /* Seeked backwards and we are doing disc mode. */
    if( p_cdda->b_nav_mode && p_access->info.i_pos > i_pos )
    {
        track_t i_track;
        char *psz_title;

        for( i_track = p_cdda->i_track; i_track > 1 &&
             p_cdda->i_lsn < cdio_get_track_lsn( p_cdda->p_cdio, i_track );
             i_track--, p_access->info.i_title-- )
            ;

        p_cdda->i_track = i_track;
        p_access->info.i_update |= INPUT_UPDATE_TITLE | INPUT_UPDATE_META ;
        psz_title  = CDDAFormatTitle( p_access, p_cdda->i_track );
        input_Control( p_cdda->p_input, INPUT_SET_NAME,
                        psz_title );
        free( psz_title );
    }

    p_access->info.i_pos = i_pos;
    p_access->info.b_eof = false;
    return VLC_SUCCESS;
}

/*
  Set up internal state so that we play a given track.
  If we are using audio-ctl mode we also activate CD-ROM
  to play.
 */
static bool cdda_play_track( access_t *p_access, track_t i_track )
{
    cdda_data_t *p_cdda = (cdda_data_t *) p_access->p_sys;

    dbg_print( (INPUT_DBG_CALL), "called track: %d\n", i_track );

    if( i_track > p_cdda->i_tracks )
    {
        msg_Err( p_access, "CD has %d tracks, and you requested track %d",
                 p_cdda->i_tracks, i_track );
        return false;
    }
    p_cdda->i_track = i_track;

    /* set up the frame boundaries for this particular track */
    p_cdda->first_frame = p_cdda->i_lsn =
    cdio_get_track_lsn( p_cdda->p_cdio, i_track );

    p_cdda->last_frame  = cdio_get_track_lsn( p_cdda->p_cdio, i_track+1 ) - 1;

#if LIBCDIO_VERSION_NUM >= 73
    if( p_cdda->b_audio_ctl )
    {
        lsn_t i_last_lsn;
        if( p_cdda->b_nav_mode )
            i_last_lsn = p_cdda->last_disc_frame;
        else
            i_last_lsn = cdio_get_track_last_lsn( p_cdda->p_cdio, i_track );
        cdda_audio_play( p_cdda->p_cdio, p_cdda->i_lsn, i_last_lsn );
    }
#endif
    return true;
}

/****************************************************************************
 * Public functions
 ****************************************************************************/

/*****************************************************************************
 * Open: open cdda device or image file and initialize structures
 *       for subsequent operations.
 *****************************************************************************/
int CDDAOpen( vlc_object_t *p_this )
{
    access_t    *p_access = (access_t*)p_this;
    char *      psz_source = NULL;
    cdda_data_t *p_cdda    = NULL;
    CdIo_t      *p_cdio;
    track_t     i_track = 1;
    bool  b_single_track = false;
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

    if( !psz_source || !*psz_source )
    {
        free( psz_source );
        /* No device/track given. Continue only when this plugin was
           selected */
        if( !p_this->b_force )
            return VLC_EGENERIC;

        psz_source = var_CreateGetString( p_this, "cd-audio" );
        if( !psz_source || !*psz_source )
        {
            free( psz_source );
            /* Scan for a CD-ROM drive with a CD-DA in it. */
            char **ppsz_drives =
                    cdio_get_devices_with_cap( NULL,  CDIO_FS_AUDIO, false );

            if( (NULL == ppsz_drives) || (NULL == ppsz_drives[0]) )
            {
                msg_Err( p_access,
                         "libcdio couldn't find something with a CD-DA in it" );
                if( ppsz_drives )
                    cdio_free_device_list( ppsz_drives );
                return VLC_EGENERIC;
            }
            psz_source = strdup( ppsz_drives[0] );
            cdio_free_device_list( ppsz_drives );
        }
    }
    cdio_log_set_handler( cdio_log_handler );

    /* Open CDDA */
    if( !(p_cdio = cdio_open( psz_source, DRIVER_UNKNOWN )) )
    {
        msg_Warn( p_access, "could not open %s", psz_source );
        free( psz_source );
        return VLC_EGENERIC;
    }

    p_cdda = calloc( 1, sizeof(cdda_data_t) );
    if( p_cdda == NULL )
    {
        free( psz_source );
        return VLC_ENOMEM;
    }

#ifdef HAVE_LIBCDDB
    cddb_log_set_handler ( cddb_log_handler );
    p_cdda->cddb.disc = NULL;
    p_cdda->b_cddb_enabled =
        config_GetInt( p_access, MODULE_STRING "-cddb-enabled" );
#endif
    p_cdda->b_cdtext =
        config_GetInt( p_access, MODULE_STRING "-cdtext-enabled" );
    p_cdda->b_cdtext_prefer =
        config_GetInt( p_access, MODULE_STRING "-cdtext-prefer" );
#if LIBCDIO_VERSION_NUM >= 73
    p_cdda->b_audio_ctl =
        config_GetInt( p_access, MODULE_STRING "-analog-output" );
#endif

    p_cdda->psz_source = strdup( psz_source );
    p_cdda->b_header   = false;
    p_cdda->p_cdio     = p_cdio;
    p_cdda->i_tracks   = 0;
    p_cdda->i_titles   = 0;
    p_cdda->i_debug    = config_GetInt( p_this, MODULE_STRING "-debug" );
    p_cdda->b_nav_mode = config_GetInt(p_this, MODULE_STRING "-navigation-mode" );
    p_cdda->i_blocks_per_read =
            config_GetInt( p_this, MODULE_STRING "-blocks-per-read" );
    p_cdda->last_disc_frame =
            cdio_get_track_lsn( p_cdio, CDIO_CDROM_LEADOUT_TRACK );
    p_cdda->p_input = vlc_object_find( p_access, VLC_OBJECT_INPUT,
                                       FIND_PARENT );

    if( 0 == p_cdda->i_blocks_per_read )
        p_cdda->i_blocks_per_read = DEFAULT_BLOCKS_PER_READ;

    if( (p_cdda->i_blocks_per_read < MIN_BLOCKS_PER_READ)
         || (p_cdda->i_blocks_per_read > MAX_BLOCKS_PER_READ) )
    {
        msg_Warn( p_cdda_input,
                  "number of blocks (%d) has to be between %d and %d. "
                  "Using %d.",
                  p_cdda->i_blocks_per_read,
                  MIN_BLOCKS_PER_READ, MAX_BLOCKS_PER_READ,
                  DEFAULT_BLOCKS_PER_READ );
        p_cdda->i_blocks_per_read = DEFAULT_BLOCKS_PER_READ;
    }

    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_EXT), "%s", psz_source );

    /* Set up p_access */
    if( p_cdda->b_audio_ctl )
    {
        p_access->pf_read  = CDDARead;
        p_access->pf_block = NULL;
    }
    else
    {
        p_access->pf_read  = NULL;
        p_access->pf_block = CDDAReadBlocks;
    }

    p_access->pf_control = CDDAControl;
    p_access->pf_seek    = CDDASeek;

    {
        lsn_t i_last_lsn;

        if( p_cdda->b_nav_mode )
            i_last_lsn = p_cdda->last_disc_frame;
        else
            i_last_lsn = cdio_get_track_last_lsn( p_cdio, i_track );

        if( CDIO_INVALID_LSN != i_last_lsn )
            p_access->info.i_size = i_last_lsn * (uint64_t) CDIO_CD_FRAMESIZE_RAW;
        else
            p_access->info.i_size = 0;
    }

    p_access->info.i_update    = 0;
    p_access->info.b_eof       = false;
    p_access->info.i_title     = 0;
    p_access->info.i_seekpoint = 0;

    p_access->p_sys     = (access_sys_t *) p_cdda;

    /* We read the Table Of Content information */
    i_rc = CDDAInit( p_access, p_cdda );
    if( VLC_SUCCESS != i_rc )
        goto error;

    cdda_play_track( p_access, i_track );
    CDDAFixupPlaylist( p_access, p_cdda, b_single_track );

#if LIBCDIO_VERSION_NUM >= 72
    {
        char *psz_paranoia = config_GetPsz( p_access,
                                MODULE_STRING "-paranoia" );

        p_cdda->e_paranoia = PARANOIA_MODE_DISABLE;
        if( psz_paranoia && *psz_paranoia )
        {
            if( !strncmp( psz_paranoia, "full", strlen("full") ) )
                p_cdda->e_paranoia = PARANOIA_MODE_FULL;
            else if( !strncmp(psz_paranoia, "overlap", strlen("overlap")) )
                p_cdda->e_paranoia = PARANOIA_MODE_OVERLAP;

            /* Use CD Paranoia? */
            if( p_cdda->e_paranoia )
            {
                p_cdda->paranoia_cd =
                            cdio_cddap_identify_cdio( p_cdio, 1, NULL );
                /* We'll set for verbose paranoia messages. */
                cdio_cddap_verbose_set( p_cdda->paranoia_cd,
                                        CDDA_MESSAGE_PRINTIT,
                                        CDDA_MESSAGE_PRINTIT );
                if ( 0 != cdio_cddap_open(p_cdda->paranoia_cd) )
                {
                    msg_Warn( p_cdda_input, "unable to get paranoia support - "
                                "continuing without it." );
                    p_cdda->e_paranoia = PARANOIA_MODE_DISABLE;
                }
                else
                {
                    p_cdda->paranoia = cdio_paranoia_init(p_cdda->paranoia_cd);
                    cdio_paranoia_seek( p_cdda->paranoia, p_cdda->i_lsn,
                                        SEEK_SET);

                    /* Set reading mode for full or overlap paranoia,
                     * but allow skipping sectors. */
                    cdio_paranoia_modeset( p_cdda->paranoia,
                            PARANOIA_MODE_FULL == p_cdda->e_paranoia ?
                            PARANOIA_MODE_FULL^PARANOIA_MODE_NEVERSKIP :
                            PARANOIA_MODE_OVERLAP^PARANOIA_MODE_NEVERSKIP );
                }
            }
        }
        free( psz_paranoia );
    }
#endif

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
    free( psz_source );
    return VLC_SUCCESS;

 error:
    cdio_destroy( p_cdda->p_cdio );
    free( psz_source );
    if( p_cdda )
    {
        if ( p_cdda->p_input )
            vlc_object_release( p_cdda->p_input );
        free(p_cdda);
    }
    return i_rc;
}

/*****************************************************************************
 * CDDAClose: closes cdda and frees any resources associded with it.
 *****************************************************************************/
void CDDAClose (vlc_object_t *p_this )
{
    access_t    *p_access = (access_t *) p_this;
    cdda_data_t *p_cdda   = (cdda_data_t *) p_access->p_sys;
    track_t      i;

#if LIBCDIO_VERSION_NUM >= 73
    if( p_cdda->b_audio_ctl )
        cdio_audio_stop(p_cdda->p_cdio);
#endif

    dbg_print( (INPUT_DBG_CALL|INPUT_DBG_EXT), "" );

    /* Remove playlist titles */
    for( i = 0; i < p_cdda->i_titles; i++ )
    {
        vlc_input_title_Delete( p_cdda->p_title[i] );
    }

#ifdef HAVE_LIBCDDB
    cddb_log_set_handler( (cddb_log_handler_t) uninit_log_handler );
    if( p_cdda->b_cddb_enabled )
        cddb_disc_destroy( p_cdda->cddb.disc );
#endif

    cdio_destroy( p_cdda->p_cdio );
    cdio_log_set_handler( uninit_log_handler );

#if LIBCDIO_VERSION_NUM >= 72
    if( p_cdda->paranoia )
        cdio_paranoia_free(p_cdda->paranoia);
    if( p_cdda->paranoia_cd )
        cdio_cddap_close_no_free_cdio( p_cdda->paranoia_cd );
#endif

    free( p_cdda->psz_mcn );
    free( p_cdda->psz_source );

#if LIBCDDB_VERSION_NUM >= 1
    libcddb_shutdown();
#endif
    free( p_cdda );
    p_cdda = NULL;
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
#if 0
            vlc_meta_t **pp_meta = (vlc_meta_t**)va_arg( args, vlc_meta_t** );
            if( p_cdda->p_meta )
            {
                *pp_meta = vlc_meta_Duplicate( p_cdda->p_meta );
                dbg_print( INPUT_DBG_META, "%s", "Meta copied" );
                return VLC_SUCCESS;
            }
            else
#endif
            {
                msg_Warn( p_access, "tried to copy NULL meta info" );
                return VLC_EGENERIC;
            }
        }

        case ACCESS_CAN_CONTROL_PACE:
        {
            bool *pb_bool = (bool*)va_arg( args, bool* );
            *pb_bool = p_cdda->b_audio_ctl ? false : true;
            dbg_print( INPUT_DBG_META, "can control pace? %d", *pb_bool);
            return VLC_SUCCESS;
        }

        case ACCESS_CAN_FASTSEEK:
            dbg_print( INPUT_DBG_META, "can fast seek?");
            goto common;
        case ACCESS_CAN_SEEK:
            dbg_print( INPUT_DBG_META, "can seek?");
            goto common;
        case ACCESS_CAN_PAUSE:
            dbg_print( INPUT_DBG_META, "can pause?");
 common:
            {
                bool *pb_bool = (bool*)va_arg( args, bool* );
                *pb_bool = true;
                return VLC_SUCCESS;
            }

        /* */
        case ACCESS_GET_MTU:
        {
            pi_int = (int*)va_arg( args, int * );
            *pi_int = p_cdda-> i_blocks_per_read * CDIO_CD_FRAMESIZE_RAW;
            dbg_print( INPUT_DBG_META, "Get MTU %d", *pi_int);
            break;
        }

        case ACCESS_GET_PTS_DELAY:
        {
            int64_t *pi_64 = (int64_t*)va_arg( args, int64_t * );
            *pi_64 = var_GetInteger( p_access, MODULE_STRING "-caching" )
              * MILLISECONDS_PER_SEC;
            break;
        }

        case ACCESS_GET_TITLE_INFO:
        {
            input_title_t ***ppp_title =
             (input_title_t***)va_arg( args, input_title_t*** );

            pi_int    = (int*)va_arg( args, int* );
            *((int*)va_arg( args, int* )) = 1; /* Title offset */

            dbg_print ( INPUT_DBG_EVENT,
                        "GET TITLE: i_tracks %d, i_tracks %d",
                        p_cdda->i_tracks, p_cdda->i_tracks );

            CDDAMetaInfo( p_access, CDIO_INVALID_TRACK );

            if( p_cdda->b_nav_mode)
            {
                char *psz_title = CDDAFormatTitle( p_access, p_cdda->i_track );
                input_Control( p_cdda->p_input, INPUT_SET_NAME, psz_title );
                free(psz_title);
            }

            /* Duplicate title info */
            if( p_cdda->i_titles == 0 )
            {
                *pi_int = 0; ppp_title = NULL;
                return VLC_SUCCESS;
            }
            *pi_int = p_cdda->i_titles;
            *ppp_title = calloc(1, sizeof( input_title_t **)
                                           * p_cdda->i_titles );

            if (!*ppp_title)
                return VLC_ENOMEM;

            for( i = 0; i < p_cdda->i_titles; i++ )
            {
                if ( p_cdda->p_title[i] )
                {
                    (*ppp_title)[i] =
                        vlc_input_title_Duplicate( p_cdda->p_title[i] );
                }
            }
            break;
        }

        case ACCESS_SET_TITLE:
        {
            i = (int)va_arg( args, int );

            dbg_print( INPUT_DBG_EVENT, "set title %d", i );
            if( i != p_access->info.i_title )
            {
                const track_t i_track = p_cdda->i_first_track + i;
                /* Update info */
                p_access->info.i_title = i;
                if( p_cdda->b_nav_mode)
                {
                    lsn_t i_last_lsn;
                    char *psz_title = CDDAFormatTitle( p_access, i_track );
                    input_Control( p_cdda->p_input, INPUT_SET_NAME,
                                   psz_title );
                    free( psz_title );
                    p_cdda->i_track = i_track;
                    i_last_lsn = cdio_get_track_lsn( p_cdda->p_cdio,
                                                CDIO_CDROM_LEADOUT_TRACK );
                    if( CDIO_INVALID_LSN != i_last_lsn )
                        p_access->info.i_size = (int64_t) CDIO_CD_FRAMESIZE_RAW
                                                            * i_last_lsn ;
                    p_access->info.i_pos = (int64_t)
                                    cdio_get_track_lsn( p_cdda->p_cdio, i_track )
                                                        * CDIO_CD_FRAMESIZE_RAW;
                }
                else
                {
                    p_access->info.i_size = p_cdda->p_title[i]->i_size;
                    p_access->info.i_pos  = 0;
                }
                p_access->info.i_update = INPUT_UPDATE_TITLE|INPUT_UPDATE_SIZE;

                /* Next sector to read */
                p_cdda->i_lsn = cdio_get_track_lsn( p_cdda->p_cdio, i_track );
            }
            break;
        }

        case ACCESS_SET_PAUSE_STATE:
            dbg_print( INPUT_DBG_META, "Pause");
            if( p_cdda->b_audio_ctl )
                cdda_audio_pause( p_cdda->p_cdio );
            break;

        case ACCESS_SET_SEEKPOINT:
            dbg_print( INPUT_DBG_META, "set seekpoint");
            return VLC_EGENERIC;

        case ACCESS_SET_PRIVATE_ID_STATE:
            dbg_print( INPUT_DBG_META, "set private id state");
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
    discmode_t  discmode = CDIO_DISC_MODE_NO_INFO;

    p_cdda->i_tracks       = cdio_get_num_tracks( p_cdda->p_cdio );
    p_cdda->i_first_track  = cdio_get_first_track_num( p_cdda->p_cdio );

    discmode = cdio_get_discmode( p_cdda->p_cdio );
    switch( discmode )
    {
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

    /* Set reading start LSN. */
    p_cdda->i_lsn = cdio_get_track_lsn(p_cdda->p_cdio, p_cdda->i_track);

    return VLC_SUCCESS;
}

/*
 * Local variables:
 *  mode: C
 *  style: gnu
 * End:
 */
