/*****************************************************************************
 * cddax.c : CD digital audio input module for vlc using libcdio
 *****************************************************************************
 * Copyright (C) 2000 VideoLAN
 * $Id: cddax.c,v 1.7 2003/11/24 03:28:27 rocky Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *          Rocky Bernstein <rocky@panix.com> 
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
#include <vlc/input.h>
#include <sys/types.h>
#include <cdio/cdio.h>
#include <cdio/cd_types.h>

#include "codecs.h"
#include "vlc_keys.h"

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include <string.h>

#include "vcdx/cdrom.h"

/* how many blocks CDDAOpen will read in each loop */
#define CDDA_BLOCKS_ONCE 1
#define CDDA_DATA_ONCE   (CDDA_BLOCKS_ONCE * CDIO_CD_FRAMESIZE_RAW)

/*****************************************************************************
 * cdda_data_t: CD audio information
 *****************************************************************************/
typedef struct cdda_data_s
{
    cddev_t     *p_cddev;                           /* CD device descriptor */
    int         i_nb_tracks;                        /* Nb of tracks (titles) */
    int         i_track;                                    /* Current track */
    lsn_t       i_sector;                                  /* Current Sector */
    lsn_t *     p_sectors;                                  /* Track sectors */
    vlc_bool_t  b_end_of_track;           /* If the end of track was reached */
    int         i_debug;                  /* Debugging mask */
    intf_thread_t *p_intf;

} cdda_data_t;

struct demux_sys_t
{
    es_descriptor_t *p_es;
    mtime_t         i_pts;
};

/*****************************************************************************
 * Debugging 
 *****************************************************************************/
#define INPUT_DBG_MRL         1 
#define INPUT_DBG_EVENT       2 /* Trace keyboard events */
#define INPUT_DBG_EXT         4 /* Calls from external routines */
#define INPUT_DBG_CALL        8 /* all calls */
#define INPUT_DBG_LSN        16 /* LSN changes */
#define INPUT_DBG_CDIO       32 /* Debugging from CDIO */
#define INPUT_DBG_SEEK       64 /* Seeks to set location */

#define DEBUG_TEXT N_("set debug mask for additional debugging.")
#define DEBUG_LONGTEXT N_( \
    "This integer when viewed in binary is a debugging mask\n" \
    "MRL             1\n" \
    "events          2\n" \
    "external call   4\n" \
    "all calls       8\n" \
    "LSN      (10)  16\n" \
    "libcdio  (20)  32\n" \
    "seeks    (40)  64\n" )

#define DEV_TEXT N_("CD-ROM device name")
#define DEV_LONGTEXT N_( \
    "Specify the name of the CD-ROM device that will be used by default. " \
    "If you don't specify anything, we'll scan for a suitable CD-ROM device.")

#define INPUT_DEBUG 1
#if INPUT_DEBUG
#define dbg_print(mask, s, args...) \
   if (p_cdda->i_debug & mask) \
     msg_Dbg(p_input, "%s: "s, __func__ , ##args)
#else
#define dbg_print(mask, s, args...) 
#endif

/*****************************************************************************
 * intf_sys_t: description and status of interface
 *****************************************************************************/
struct intf_sys_t
{
    input_thread_t    * p_input;
    cdda_data_t       * p_cdda;
    vlc_bool_t          b_click, b_move, b_key_pressed;
};

/* FIXME: This variable is a hack. Would be nice to eliminate. */
static input_thread_t *p_cdda_input = NULL;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  CDDAOpen         ( vlc_object_t * );
static void CDDAClose        ( vlc_object_t * );
static int  CDDARead         ( input_thread_t *, byte_t *, size_t );
static void CDDASeek         ( input_thread_t *, off_t );
static int  CDDASetArea      ( input_thread_t *, input_area_t * );
static int  CDDAPlay         ( input_thread_t *, int );
static int  CDDASetProgram   ( input_thread_t *, pgrm_descriptor_t * );

static int  CDDAOpenDemux    ( vlc_object_t * );
static void CDDACloseDemux   ( vlc_object_t * );
static int  CDDADemux        ( input_thread_t * p_input );

static int  CDDAOpenIntf     ( vlc_object_t * );
static void CDDACloseIntf    ( vlc_object_t * );


static int  InitThread     ( intf_thread_t *p_intf );
static int  MouseEvent     ( vlc_object_t *, char const *,
                             vlc_value_t, vlc_value_t, void * );
static int  KeyEvent       ( vlc_object_t *, char const *,
                             vlc_value_t, vlc_value_t, void * );

static void RunIntf          ( intf_thread_t *p_intf );

static int debug_callback   ( vlc_object_t *p_this, const char *psz_name,
			      vlc_value_t oldval, vlc_value_t val, 
			      void *p_data );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for cdda streams. This " \
    "value should be set in miliseconds units." )

vlc_module_begin();
    set_description( _("CD Audio input") );
    set_capability( "access", 75 /* slightly higher than cdda */ );
    add_integer( "cddax-caching", DEFAULT_PTS_DELAY / 1000, NULL, CACHING_TEXT, CACHING_LONGTEXT, VLC_TRUE );
    set_callbacks( CDDAOpen, CDDAClose );
    add_shortcut( "cdda" );

    /* Configuration options */
    add_category_hint( N_("CDX"), NULL, VLC_TRUE );
    add_integer ( MODULE_STRING "-debug", 0, debug_callback, DEBUG_TEXT, 
                  DEBUG_LONGTEXT, VLC_TRUE );
    add_string( MODULE_STRING "-device", "", NULL, DEV_TEXT, 
		DEV_LONGTEXT, VLC_TRUE );

    add_submodule();
        set_description( _("CD Audio demux") );
        set_capability( "demux", 0 );
        set_callbacks( CDDAOpenDemux, CDDACloseDemux );
        add_shortcut( "cdda" );

    add_submodule();
        set_capability( "interface", 0 );
        set_callbacks( E_(CDDAOpenIntf), E_(CDDACloseIntf) );

vlc_module_end();

/****************************************************************************
 * Private functions
 ****************************************************************************/

static int
debug_callback   ( vlc_object_t *p_this, const char *psz_name,
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
 * CDDAOpen: open cdda
 *****************************************************************************/
static int CDDAOpen( vlc_object_t *p_this )
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
 * CDDAPlay: Arrange things play track
 *****************************************************************************/
static vlc_bool_t
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
static void 
CDDAClose( vlc_object_t *p_this )
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

/****************************************************************************
 * Demux Part
 ****************************************************************************/
static int  CDDAOpenDemux    ( vlc_object_t * p_this)
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t    *p_demux;
    WAVEFORMATEX   *p_wf;

    if( p_input->stream.i_method != INPUT_METHOD_CDDA )
    {
        return VLC_EGENERIC;
    }

    p_demux = malloc( sizeof( es_descriptor_t ) );
    p_demux->i_pts = 0;
    p_demux->p_es = NULL;

    p_input->pf_demux  = CDDADemux;
    p_input->pf_demux_control = demux_vaControlDefault;
    p_input->pf_rewind = NULL;
    p_input->p_demux_data = p_demux;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_AddProgram( p_input, 0, 0) == NULL )
    {
        msg_Err( p_input, "cannot add program" );
        free( p_input->p_demux_data );
        return( -1 );
    }
    p_input->stream.pp_programs[0]->b_is_ok = 0;
    p_input->stream.p_selected_program = p_input->stream.pp_programs[0];

    /* create our ES */ 
    p_demux->p_es = input_AddES( p_input, 
                                 p_input->stream.p_selected_program, 
                                 1 /* id */, AUDIO_ES, NULL, 0 );
    if( !p_demux->p_es )
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "out of memory" );
        free( p_input->p_demux_data );
        return( -1 );
    }
    p_demux->p_es->i_stream_id = 1;
    p_demux->p_es->i_fourcc = VLC_FOURCC('a','r','a','w');

    p_demux->p_es->p_waveformatex = p_wf = malloc( sizeof( WAVEFORMATEX ) );
    p_wf->wFormatTag = WAVE_FORMAT_PCM;
    p_wf->nChannels = 2;
    p_wf->nSamplesPerSec = 44100;
    p_wf->nAvgBytesPerSec = 2 * 44100 * 2;
    p_wf->nBlockAlign = 4;
    p_wf->wBitsPerSample = 16;
    p_wf->cbSize = 0;

    input_SelectES( p_input, p_demux->p_es );

    p_input->stream.p_selected_program->b_is_ok = 1;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return VLC_SUCCESS;
}

static void CDDACloseDemux( vlc_object_t * p_this)
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    demux_sys_t    *p_demux = (demux_sys_t*)p_input->p_demux_data;

    free( p_demux );
    p_input->p_demux_data = NULL;
    return;
}

static int  CDDADemux( input_thread_t * p_input )
{
    demux_sys_t    *p_demux = (demux_sys_t*)p_input->p_demux_data;
    ssize_t         i_read;
    data_packet_t * p_data;
    pes_packet_t *  p_pes;

    input_ClockManageRef( p_input,
                          p_input->stream.p_selected_program,
                          p_demux->i_pts );

    i_read = input_SplitBuffer( p_input, &p_data, CDIO_CD_FRAMESIZE_RAW );
    if( i_read <= 0 )
    {
        return 0; // EOF
    }

    p_pes = input_NewPES( p_input->p_method_data );

    if( p_pes == NULL )
    {
        msg_Err( p_input, "out of memory" );
        input_DeletePacket( p_input->p_method_data, p_data );
        return -1;
    }

    p_pes->i_rate = p_input->stream.control.i_rate;
    p_pes->p_first = p_pes->p_last = p_data;
    p_pes->i_nb_data = 1;
    p_pes->i_pes_size = i_read;

    p_pes->i_dts =
        p_pes->i_pts = input_ClockGetTS( p_input,
                                         p_input->stream.p_selected_program,
                                         p_demux->i_pts );

    if( p_demux->p_es->p_dec )
    {
        input_DecodePES( p_demux->p_es->p_dec, p_pes );
    }
    else
    {
        input_DeletePES( p_input->p_method_data, p_pes );
    }

    p_demux->i_pts += ((mtime_t)90000) * i_read
                      / (mtime_t)44100 / 4 /* stereo 16 bits */;
    return 1;
}


/*****************************************************************************
 * OpenIntf: initialize dummy interface
 *****************************************************************************/
int CDDAOpenIntf ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    /* Allocate instance and initialize some members */
    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        return( 1 );
    };

    p_intf->pf_run = RunIntf;

    var_AddCallback( p_intf->p_vlc, "key-pressed", KeyEvent, p_intf );

    return( 0 );
}

/*****************************************************************************
 * CloseIntf: destroy dummy interface
 *****************************************************************************/
void CDDACloseIntf ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    /* Destroy structure */
    free( p_intf->p_sys );
}


/*****************************************************************************
 * RunIntf: main loop
 *****************************************************************************/
static void RunIntf( intf_thread_t *p_intf )
{
    vlc_object_t      * p_vout = NULL;
    cdda_data_t       * p_cdda;
    input_thread_t    * p_input;
    
    /* What you add to the last input number entry. It accumulates all of
       the 10_ADD keypresses */
    int number_addend = 0; 
    
    if( InitThread( p_intf ) < 0 )
    {
        msg_Err( p_intf, "can't initialize intf" );
        return;
    }

    p_input = p_intf->p_sys->p_input;
    p_cdda   = p_intf->p_sys->p_cdda = 
      (cdda_data_t *) p_input->p_access_data;

    dbg_print( INPUT_DBG_CALL, "intf initialized" );

    /* Main loop */
    while( !p_intf->b_die )
    {
      vlc_mutex_lock( &p_intf->change_lock );

      /*
       * keyboard event
       */
      if( p_vout && p_intf->p_sys->b_key_pressed )
        {
	  vlc_value_t val;
	  int i, i_action = -1;
	  struct hotkey *p_hotkeys = p_intf->p_vlc->p_hotkeys;

	  p_intf->p_sys->b_key_pressed = VLC_FALSE;
          
	  /* Find action triggered by hotkey (if any) */
	  var_Get( p_intf->p_vlc, "key-pressed", &val );

	  dbg_print( INPUT_DBG_EVENT, "Key pressed %d", val.i_int );

	  for( i = 0; p_hotkeys[i].psz_action != NULL; i++ )
            {
	      if( p_hotkeys[i].i_key == val.i_int )
                {
		  i_action = p_hotkeys[i].i_action;
                }
            }
	  
	  if( i_action != -1) {
	    switch (i_action) {
	      
	    case ACTIONID_NAV_LEFT: 
	      dbg_print( INPUT_DBG_EVENT, "ACTIONID_NAV_LEFT (%d)", 
			 number_addend );
	      do {
		if ( CDDAPlay( p_input, p_cdda->i_track-1 ) ) {
		  p_cdda->i_track--;
		} else {
		  break;
		}
	      }	while (number_addend-- > 0);
	      break;

	    case ACTIONID_NAV_RIGHT:
	      dbg_print( INPUT_DBG_EVENT, "ACTIONID_NAV_RIGHT (%d)",
			 number_addend );
	      do {
		if ( CDDAPlay( p_input, p_cdda->i_track+1 ) ) {
		  p_cdda->i_track++;
		} else {
		  break;
		}
	      } while (number_addend-- > 0);
	      break;

	    case ACTIONID_NAV_UP:
	      dbg_print( INPUT_DBG_EVENT, "ACTIONID_NAV_UP" );
	      do {
		;
	      } while (number_addend-- > 0);
	      break;

	    case ACTIONID_NAV_DOWN:
	      dbg_print( INPUT_DBG_EVENT, "ACTIONID_NAV_DOWN"  );
	      break;

	    case ACTIONID_NAV_ACTIVATE: 
	      {
		dbg_print( INPUT_DBG_EVENT, "ACTIONID_NAV_ACTIVATE" );
		if ( CDDAPlay( p_input, number_addend ) ) {
		  p_cdda->i_track = number_addend;
		} else {
		  break;
		}
		break;
	      }
	    }
	    number_addend = 0;
	  } else {
	    unsigned int digit_entered=0;

	    switch (val.i_int) {
	    case '9':
	      digit_entered++;
	    case '8':
	      digit_entered++;
	    case '7':
	      digit_entered++;
	    case '6':
	      digit_entered++;
	    case '5':
	      digit_entered++;
	    case '4':
	      digit_entered++;
	    case '3':
	      digit_entered++;
	    case '2':
	      digit_entered++;
	    case '1':
	      digit_entered++;
	    case '0':
	      {
		number_addend *= 10;
		number_addend += digit_entered;
		dbg_print( INPUT_DBG_EVENT, 
			   "Added %d. Number is now: %d\n", 
			   digit_entered, number_addend);
		break;
	      }
	    }
	  }
        }

      
      vlc_mutex_unlock( &p_intf->change_lock );
      
      if( p_vout == NULL )
        {
	  p_vout = vlc_object_find( p_intf->p_sys->p_input,
				    VLC_OBJECT_VOUT, FIND_ANYWHERE );
	  if( p_vout )
            {
	      var_AddCallback( p_vout, "mouse-moved", MouseEvent, p_intf );
	      var_AddCallback( p_vout, "mouse-clicked", MouseEvent, p_intf );
	      var_AddCallback( p_vout, "key-pressed", KeyEvent, p_intf );
            }
        }
      
      
      /* Wait a bit */
      msleep( INTF_IDLE_SLEEP );
    }

    if( p_vout )
    {
        var_DelCallback( p_vout, "mouse-moved", MouseEvent, p_intf );
        var_DelCallback( p_vout, "mouse-clicked", MouseEvent, p_intf );
        vlc_object_release( p_vout );
    }

    vlc_object_release( p_intf->p_sys->p_input );
}

/*****************************************************************************
 * InitThread:
 *****************************************************************************/
static int InitThread( intf_thread_t * p_intf )
{
    /* We might need some locking here */
    if( !p_intf->b_die )
    {
        input_thread_t * p_input;

        p_input = vlc_object_find( p_intf, VLC_OBJECT_INPUT, FIND_PARENT );

        /* Maybe the input just died */
        if( p_input == NULL )
        {
            return VLC_EGENERIC;
        }

        vlc_mutex_lock( &p_intf->change_lock );

        p_intf->p_sys->p_input = p_input;

        p_intf->p_sys->b_move = VLC_FALSE;
        p_intf->p_sys->b_click = VLC_FALSE;
        p_intf->p_sys->b_key_pressed = VLC_FALSE;

        vlc_mutex_unlock( &p_intf->change_lock );

        return VLC_SUCCESS;
    }
    else
    {
        return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * MouseEvent: callback for mouse events
 *****************************************************************************/
static int MouseEvent( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_data;

    vlc_mutex_lock( &p_intf->change_lock );

    if( psz_var[6] == 'c' ) /* "mouse-clicked" */
    {
        p_intf->p_sys->b_click = VLC_TRUE;
    }
    else if( psz_var[6] == 'm' ) /* "mouse-moved" */
    {
        p_intf->p_sys->b_move = VLC_TRUE;
    }

    vlc_mutex_unlock( &p_intf->change_lock );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * KeyEvent: callback for keyboard events
 *****************************************************************************/
static int KeyEvent( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_data;
    vlc_mutex_lock( &p_intf->change_lock );

    p_intf->p_sys->b_key_pressed = VLC_TRUE;
    
    vlc_mutex_unlock( &p_intf->change_lock );

    return VLC_SUCCESS;
}
