/*****************************************************************************
 * access.c: DVB card input v4l2 only
 *****************************************************************************
 * Copyright (C) 1998-2004 VideoLAN
 *
 * Authors: Johan Bilien <jobi@via.ecp.fr>
 *          Jean-Paul Saman <jpsaman@wxs.nl>
 *          Christophe Massiot <massiot@via.ecp.fr>
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

#include "../../demux/mpeg/system.h"

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include <fcntl.h>
#include <sys/types.h>

#ifdef HAVE_ERRNO_H
#    include <string.h>
#    include <errno.h>
#endif

#ifdef STRNCASECMP_IN_STRINGS_H
#   include <strings.h>
#endif

#include "dvb.h"

#define SATELLITE_READ_ONCE 3

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int     Open( vlc_object_t *p_this );
static void    Close( vlc_object_t *p_this );
static ssize_t Read( input_thread_t * p_input, byte_t * p_buffer,
                              size_t i_len);
static int     SetArea    ( input_thread_t *, input_area_t * );
static int     SetProgram ( input_thread_t *, pgrm_descriptor_t * );
static void    Seek       ( input_thread_t *, off_t );
static void    AllocateDemux( input_thread_t * p_input, int i_pid,
                              int i_type );
static void    CloseProgram( input_thread_t * p_input );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define CACHING_TEXT N_("Caching value in ms")
#define CACHING_LONGTEXT N_( \
    "Allows you to modify the default caching value for dvb streams. This " \
    "value should be set in millisecond units." )

#define PROGRAM_TEXT N_("Program to decode")
#define PROGRAM_LONGTEXT N_("This is a workaround for a bug in the input")

#define ADAPTER_TEXT N_("Adapter card to tune")
#define ADAPTER_LONGTEXT N_("Adapter cards have a device file in directory named /dev/dvb/adapter[n] with n>=0.")

#define DEVICE_TEXT N_("Device number to use on adapter")
#define DEVICE_LONGTEXT ""

#define FREQ_TEXT N_("Transponder/multiplex frequency")
#define FREQ_LONGTEXT N_("In kHz for DVB-S or Hz for DVB-C/T")

#define INVERSION_TEXT N_("Inversion mode")
#define INVERSION_LONGTEXT N_("Inversion mode [0=off, 1=on, 2=auto]")

#define PROBE_TEXT N_("Probe DVB card for capabilities")
#define PROBE_LONGTEXT N_("Some DVB cards do not like to be probed for their capabilities.")

#define LNB_LOF1_TEXT N_("Antenna lnb_lof1 (kHz)")
#define LNB_LOF1_LONGTEXT ""

#define LNB_LOF2_TEXT N_("Antenna lnb_lof2 (kHz)")
#define LNB_LOF2_LONGTEXT ""

#define LNB_SLOF_TEXT N_("Antenna lnb_slof (kHz)")
#define LNB_SLOF_LONGTEXT ""

/* Satellite */
#define BUDGET_TEXT N_("Budget mode")
#define BUDGET_LONGTEXT N_("This allows you to stream an entire transponder with a budget card. Budget mode is compatible with the ts2 demux.")

#define SATNO_TEXT N_("Satellite number in the Diseqc system")
#define SATNO_LONGTEXT N_("[0=no diseqc, 1-4=normal diseqc, -1=A, -2=B simple diseqc")

#define VOLTAGE_TEXT N_("LNB voltage")
#define VOLTAGE_LONGTEXT N_("In Volts [0, 13=vertical, 18=horizontal]")

#define TONE_TEXT N_("22 kHz tone")
#define TONE_LONGTEXT N_("[0=off, 1=on, -1=auto]")

#define FEC_TEXT N_("Transponder FEC")
#define FEC_LONGTEXT N_("FEC=Forward Error Correction mode [9=auto]")

#define SRATE_TEXT N_("Transponder symbol rate in kHz")
#define SRATE_LONGTEXT ""

/* Cable */
#define MODULATION_TEXT N_("Modulation type")
#define MODULATION_LONGTEXT N_("Modulation type for front-end device.")

/* Terrestrial */
#define CODE_RATE_HP_TEXT N_("Terrestrial high priority stream code rate (FEC)")
#define CODE_RATE_HP_LONGTEXT ""

#define CODE_RATE_LP_TEXT N_("Terrestrial low priority stream code rate (FEC)")
#define CODE_RATE_LP_LONGTEXT ""

#define BANDWIDTH_TEXT N_("Terrestrial bandwidth")
#define BANDWIDTH_LONGTEXT N_("Terrestrial bandwidth [0=auto,6,7,8 in MHz]")

#define GUARD_TEXT N_("Terrestrial guard interval")
#define GUARD_LONGTEXT ""

#define TRANSMISSION_TEXT N_("Terrestrial transmission mode")
#define TRANSMISSION_LONGTEXT ""

#define HIERARCHY_TEXT N_("Terrestrial hierarchy mode")
#define HIERARCHY_LONGTEXT ""

vlc_module_begin();
    set_shortname( _("DVB") );
    set_description( N_("DVB input with v4l2 support") );

    add_integer( "dvb-caching", DEFAULT_PTS_DELAY / 1000, NULL, CACHING_TEXT,
                 CACHING_LONGTEXT, VLC_TRUE );
    add_integer( "dvb-adapter", 0, NULL, ADAPTER_TEXT, ADAPTER_LONGTEXT,
                 VLC_FALSE );
    add_integer( "dvb-device", 0, NULL, DEVICE_TEXT, DEVICE_LONGTEXT,
                 VLC_TRUE );
    add_integer( "dvb-frequency", 11954000, NULL, FREQ_TEXT, FREQ_LONGTEXT,
                 VLC_FALSE );
    add_integer( "dvb-inversion", 2, NULL, INVERSION_TEXT, INVERSION_LONGTEXT,
                 VLC_TRUE );
    add_bool( "dvb-probe", 1, NULL, PROBE_TEXT, PROBE_LONGTEXT, VLC_TRUE );
    add_integer( "dvb-lnb-lof1", 9750000, NULL, LNB_LOF1_TEXT,
                 LNB_LOF1_LONGTEXT, VLC_TRUE );
    add_integer( "dvb-lnb-lof2", 10600000, NULL, LNB_LOF2_TEXT,
                 LNB_LOF2_LONGTEXT, VLC_TRUE );
    add_integer( "dvb-lnb-slof", 11700000, NULL, LNB_SLOF_TEXT,
                 LNB_SLOF_LONGTEXT, VLC_TRUE );
    /* DVB-S (satellite) */
    add_bool( "dvb-budget-mode", 0, NULL, BUDGET_TEXT, BUDGET_LONGTEXT,
              VLC_TRUE );
    add_integer( "dvb-satno", 0, NULL, SATNO_TEXT, SATNO_LONGTEXT,
                 VLC_TRUE );
    add_integer( "dvb-voltage", 13, NULL, VOLTAGE_TEXT, VOLTAGE_LONGTEXT,
                 VLC_TRUE );
    add_integer( "dvb-tone", -1, NULL, TONE_TEXT, TONE_LONGTEXT,
                 VLC_TRUE );
    add_integer( "dvb-fec", 9, NULL, FEC_TEXT, FEC_LONGTEXT, VLC_TRUE );
    add_integer( "dvb-srate", 27500000, NULL, SRATE_TEXT, SRATE_LONGTEXT,
                 VLC_FALSE );
    /* DVB-T (terrestrial) */
    add_integer( "dvb-modulation", 0, NULL, MODULATION_TEXT,
                 MODULATION_LONGTEXT, VLC_TRUE );
    /* DVB-T (terrestrial) */
    add_integer( "dvb-code-rate-hp", 9, NULL, CODE_RATE_HP_TEXT,
                 CODE_RATE_HP_LONGTEXT, VLC_TRUE );
    add_integer( "dvb-code-rate-lp", 9, NULL, CODE_RATE_LP_TEXT,
                 CODE_RATE_LP_LONGTEXT, VLC_TRUE );
    add_integer( "dvb-bandwidth", 0, NULL, BANDWIDTH_TEXT, BANDWIDTH_LONGTEXT,
                 VLC_TRUE );
    add_integer( "dvb-guard", 0, NULL, GUARD_TEXT, GUARD_LONGTEXT, VLC_TRUE );
    add_integer( "dvb-transmission", 0, NULL, TRANSMISSION_TEXT,
                 TRANSMISSION_LONGTEXT, VLC_TRUE );
    add_integer( "dvb-hierarchy", 0, NULL, HIERARCHY_TEXT, HIERARCHY_LONGTEXT,
                 VLC_TRUE );

    set_capability( "access", 0 );
    add_shortcut( "dvb" );
    add_shortcut( "dvb-s" );
    add_shortcut( "qpsk" );
    add_shortcut( "dvb-c" );
    add_shortcut( "cable" );
    add_shortcut( "dvb-t" );
    add_shortcut( "terrestrial" );
    add_shortcut( "satellite" );    /* compatibility with the interface. */
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: open the frontend device
 *****************************************************************************/
#define GET_OPTION_INT( option )                                            \
    if ( !strncmp( psz_parser, option "=", strlen(option "=") ) )           \
    {                                                                       \
        val.i_int = strtol( psz_parser + strlen(option "="), &psz_parser,   \
                            0 );                                            \
        var_Set( p_input, "dvb-" option, val );                             \
    }

#define GET_OPTION_BOOL( option )                                           \
    if ( !strncmp( psz_parser, option "=", strlen(option "=") ) )           \
    {                                                                       \
        val.b_bool = strtol( psz_parser + strlen(option "="), &psz_parser,  \
                             0 );                                           \
        var_Set( p_input, "dvb-" option, val );                             \
    }

static int Open( vlc_object_t *p_this )
{
    input_thread_t *    p_input = (input_thread_t *)p_this;
    thread_dvb_data_t * p_dvb;
    char *              psz_parser;
    char *              psz_next;
    vlc_value_t         val;
    int                 i_test;

    /* Initialize structure */
    p_dvb = (thread_dvb_data_t *)malloc( sizeof( thread_dvb_data_t ) );
    if( p_dvb == NULL )
    {
        msg_Err( p_input, "out of memory" );
        return -1;
    }
    memset( p_dvb, 0, sizeof(thread_dvb_data_t) );
    p_input->p_access_data = (void *)p_dvb;

    /* Register callback functions */
    p_input->pf_read = Read;
    p_input->pf_set_program = SetProgram;
    p_input->pf_set_area = SetArea;
    p_input->pf_seek = Seek;

    /* Parse the options passed in command line */

    psz_parser = strdup( p_input->psz_name );
    if ( !psz_parser )
    {
        free( p_dvb );
        return( -1 );
    }

    var_Create( p_input, "dvb-caching", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Get( p_input, "dvb-caching", &val );
    p_input->i_pts_delay = val.i_int * 1000;

    var_Create( p_input, "dvb-adapter", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_input, "dvb-device", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_input, "dvb-frequency", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_input, "dvb-inversion", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_input, "dvb-probe", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_input, "dvb-lnb-lof1", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_input, "dvb-lnb-lof2", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_input, "dvb-lnb-slof", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    var_Create( p_input, "dvb-budget-mode", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_input, "dvb-satno", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_input, "dvb-voltage", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_input, "dvb-tone", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_input, "dvb-fec", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_input, "dvb-srate", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    var_Create( p_input, "dvb-modulation", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    var_Create( p_input, "dvb-code-rate-hp", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_input, "dvb-code-rate-lp", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_input, "dvb-bandwidth", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_input, "dvb-transmission", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_input, "dvb-guard", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    var_Create( p_input, "dvb-hierarchy", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    i_test = strtol( psz_parser, &psz_next, 10 );
    if ( psz_next == psz_parser )
    {
        for ( ; ; )
        {
            GET_OPTION_INT("adapter")
            else GET_OPTION_INT("device")
            else GET_OPTION_INT("frequency")
            else GET_OPTION_INT("inversion")
            else GET_OPTION_BOOL("probe")
            else GET_OPTION_INT("lnb-lof1")
            else GET_OPTION_INT("lnb-lof2")
            else GET_OPTION_INT("lnb-slof")

            else GET_OPTION_BOOL("budget-mode")
            else GET_OPTION_INT("voltage")
            else GET_OPTION_INT("tone")
            else GET_OPTION_INT("fec")
            else GET_OPTION_INT("srate")

            else GET_OPTION_INT("modulation")

            else GET_OPTION_INT("code-rate-hp")
            else GET_OPTION_INT("code-rate-lp")
            else GET_OPTION_INT("bandwidth")
            else GET_OPTION_INT("transmission")
            else GET_OPTION_INT("guard")
            else GET_OPTION_INT("hierarchy")

            else if( !strncmp( psz_parser, "satno=",
                               strlen( "satno=" ) ) )
            {
                psz_parser += strlen( "satno=" );
                if ( *psz_parser == 'A' || *psz_parser == 'a' )
                    val.i_int = -1;
                else if ( *psz_parser == 'B' || *psz_parser == 'b' )
                    val.i_int = -2;
                else
                    val.i_int = strtol( psz_parser, &psz_parser, 0 );
                var_Set( p_input, "dvb-satno", val );
            }
            /* Redundant with voltage but much easier to use */
            else if( !strncmp( psz_parser, "polarization=",
                               strlen( "polarization=" ) ) )
            {
                psz_parser += strlen( "polarization=" );
                if ( *psz_parser == 'V' || *psz_parser == 'v' )
                    val.i_int = 13;
                else if ( *psz_parser == 'H' || *psz_parser == 'h' )
                    val.i_int = 18;
                else
                {
                    msg_Err( p_input, "illegal polarization %c", *psz_parser );
                    free( p_dvb );
                    return -1;
                }
                var_Set( p_input, "dvb-voltage", val );
            }
            if ( *psz_parser )
                psz_parser++;
            else
                break;
        }
    }
    else
    {
        msg_Err( p_input, "the DVB input old syntax is deprecated, use vlc " \
                 "-p dvb to see an explanation of the new syntax" );
        free( p_dvb );
        return -1;
    }

    /* Getting frontend info */
    if ( E_(FrontendOpen)( p_input ) < 0 )
    {
        free( p_dvb );
        return -1;
    }

    /* Setting frontend parameters for tuning the hardware */      
    msg_Dbg( p_input, "trying to tune the frontend...");
    if ( E_(FrontendSet)( p_input ) < 0 )
    {
        E_(FrontendClose)( p_input );
        free( p_dvb );
        return -1;
    }

    /* Opening DVR device */      
    if ( E_(DVROpen)( p_input ) < 0 )
    {
        E_(FrontendClose)( p_input );
        free( p_dvb );
        return -1;
    }

    var_Get( p_input, "dvb-budget-mode", &val );
    p_dvb->b_budget_mode = val.b_bool;
    if ( val.b_bool )
    {
        msg_Dbg( p_input, "setting filter on all PIDs" );
        AllocateDemux( p_input, 0x2000, OTHER_TYPE );
    }
    else
    {
        msg_Dbg( p_input, "setting filter on PAT" );
        AllocateDemux( p_input, 0x0, OTHER_TYPE );
    }

    if( input_InitStream( p_input, sizeof( stream_ts_data_t ) ) == -1 )
    {
        msg_Err( p_input, "could not initialize stream structure" );
	E_(FrontendClose)( p_input );
        close( p_dvb->i_handle );
        free( p_dvb );
        return( -1 );
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );

    p_input->stream.b_pace_control = 0;
    p_input->stream.b_seekable = 0;
    p_input->stream.p_selected_area->i_tell = 0;

    vlc_mutex_unlock( &p_input->stream.stream_lock );

    p_input->i_mtu = SATELLITE_READ_ONCE * TS_PACKET_SIZE;
    p_input->stream.i_method = INPUT_METHOD_SATELLITE;

    return 0;
}

/*****************************************************************************
 * Close : Close the device
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    input_thread_t *    p_input = (input_thread_t *)p_this;
    thread_dvb_data_t * p_dvb = (thread_dvb_data_t *)p_input->p_access_data;

    if ( !p_dvb->b_budget_mode )
    {
        CloseProgram( p_input );
    }
    if ( p_dvb->p_demux_handles[0].i_type )
    {
        E_(DMXUnsetFilter)( p_input, p_dvb->p_demux_handles[0].i_handle );
        p_dvb->p_demux_handles[0].i_type = 0;
    }
    E_(DVRClose)( p_input );
    E_(FrontendClose)( p_input );
    free( p_dvb );
}

/*****************************************************************************
 * Read: reads data from the satellite card
 *****************************************************************************/
static ssize_t Read( input_thread_t * p_input, byte_t * p_buffer,
                     size_t i_len )
{
    thread_dvb_data_t * p_dvb = (thread_dvb_data_t *)p_input->p_access_data;
    ssize_t i_ret;
    vlc_value_t val;
    struct timeval timeout;
    fd_set fds;

    if ( !p_dvb->b_budget_mode && !p_dvb->p_demux_handles[1].i_type )
    {
        int i_program;
        unsigned int i;
        var_Get( p_input, "program", &val );
        i_program = val.i_int;

        /* FIXME : this is not demux2-compatible */
        for ( i = 0; i < p_input->stream.i_pgrm_number; i++ )
        {
            /* Only set a filter on the selected program : some boards
             * (read: Dreambox) only have 8 filters, so you don't want to
             * spend them on unwanted PMTs. --Meuuh */
            if ( !i_program
                   || p_input->stream.pp_programs[i]->i_number == i_program )
            {
                msg_Dbg( p_input, "setting filter on PMT pid %d",
                         p_input->stream.pp_programs[i]->pp_es[0]->i_id );
                AllocateDemux( p_input,
                        p_input->stream.pp_programs[i]->pp_es[0]->i_id,
                        OTHER_TYPE );
            }
        }
    }

    /* Find if some data is available. This won't work under Windows. */

    /* Initialize file descriptor set */
    FD_ZERO( &fds );
    FD_SET( p_dvb->i_handle, &fds );

    /* We'll wait 0.5 second if nothing happens */
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;

    /* Find if some data is available */
    while ( (i_ret = select( p_dvb->i_handle + 1, &fds,
                             NULL, NULL, &timeout )) == 0
             || (i_ret < 0 && errno == EINTR) )
    {
        FD_ZERO( &fds );
        FD_SET( p_dvb->i_handle, &fds );
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;

        if ( p_input->b_die || p_input->b_error )
        {
            return 0;
        }
    }

    if ( i_ret < 0 )
    {
#ifdef HAVE_ERRNO_H
        msg_Err( p_input, "select error (%s)", strerror(errno) );
#else
        msg_Err( p_input, "select error" );
#endif
        return -1;
    }

    i_ret = read( p_dvb->i_handle, p_buffer, i_len );
 
    if( i_ret < 0 )
    {
#ifdef HAVE_ERRNO_H
        msg_Err( p_input, "read failed (%s)", strerror(errno) );
#else
        msg_Err( p_input, "read failed" );
#endif
    }

    return i_ret;
}

/*****************************************************************************
 * SetArea : Does nothing
 *****************************************************************************/
static int SetArea( input_thread_t * p_input, input_area_t * p_area )
{
    return -1;
}

/*****************************************************************************
 * SetProgram : Sets the card filters according to the selected program,
 *              and makes the appropriate changes to stream structure.
 *****************************************************************************/
static int SetProgram( input_thread_t    * p_input,
                       pgrm_descriptor_t * p_new_prg )
{
    thread_dvb_data_t * p_dvb = (thread_dvb_data_t *)p_input->p_access_data;
    unsigned int i_es_index;
    vlc_value_t val;
    int i_video_type = VIDEO0_TYPE;
    int i_audio_type = AUDIO0_TYPE;

    if ( p_input->stream.p_selected_program )
    {
        for ( i_es_index = 0; /* 0 should be the PMT */
                i_es_index < p_input->stream.p_selected_program->i_es_number;
                i_es_index ++ )
        {
#define p_es p_input->stream.p_selected_program->pp_es[i_es_index]
            if ( p_es->p_dec )
            {
                input_UnselectES( p_input , p_es );
            }
#undef p_es
        }
    }

    if ( !p_dvb->b_budget_mode )
    {
        msg_Dbg( p_input, "unsetting filters on all pids" );
        CloseProgram( p_input );
        msg_Dbg( p_input, "setting filter on PMT pid %d",
                 p_new_prg->pp_es[0]->i_id );
        AllocateDemux( p_input, p_new_prg->pp_es[0]->i_id, OTHER_TYPE );
    }

    for ( i_es_index = 1; i_es_index < p_new_prg->i_es_number;
          i_es_index++ )
    {
#define p_es p_new_prg->pp_es[i_es_index]
        switch( p_es->i_cat )
        {
        case VIDEO_ES:
            if ( !p_dvb->b_budget_mode )
            {
                msg_Dbg(p_input, "setting filter on video ES 0x%x",
                        p_es->i_id);
                /* Always set the filter. This may seem a little odd, but
                 * it allows you to stream the video with demuxstream
                 * without having a decoder or a stream output behind.
                 * The result is you'll sometimes filter a PID which you
                 * don't really want, but in the most common cases it
                 * should be OK. --Meuuh */
                AllocateDemux( p_input, p_es->i_id, i_video_type );
                i_video_type += TYPE_INTERVAL;
            }
            input_SelectES( p_input, p_es );
            break;

        case AUDIO_ES:
            if ( !p_dvb->b_budget_mode )
            {
                msg_Dbg(p_input, "setting filter on audio ES 0x%x",
                        p_es->i_id);
                AllocateDemux( p_input, p_es->i_id, i_audio_type );
                i_audio_type += TYPE_INTERVAL;
            }
            input_SelectES( p_input, p_es );
            break;
        default:
            if ( !p_dvb->b_budget_mode )
            {
                msg_Dbg(p_input, "setting filter on other ES 0x%x",
                        p_es->i_id);
                AllocateDemux( p_input, p_es->i_id, OTHER_TYPE );
            }
            input_SelectES( p_input, p_es );
            break;
        }
#undef p_es
    }

    p_input->stream.p_selected_program = p_new_prg;

    /* Update the navigation variables without triggering a callback */
    val.i_int = p_new_prg->i_number;
    var_Change( p_input, "program", VLC_VAR_SETVALUE, &val, NULL );

    return 0;
}

/*****************************************************************************
 * Seek: does nothing (not a seekable stream
 *****************************************************************************/
static void Seek( input_thread_t * p_input, off_t i_off )
{
    ;
}

/*****************************************************************************
 * AllocateDemux:
 *****************************************************************************/
static void AllocateDemux( input_thread_t * p_input, int i_pid,
                           int i_type )
{
    thread_dvb_data_t * p_dvb = (thread_dvb_data_t *)p_input->p_access_data;
    int                 i;

    /* Find first free slot */
    for ( i = 0; i < MAX_DEMUX; i++ )
    {
        if ( !p_dvb->p_demux_handles[i].i_type )
        {
            if ( E_(DMXSetFilter)( p_input, i_pid,
                                   &p_dvb->p_demux_handles[i].i_handle,
                                   i_type ) < 0 )
            {
                break;
            }
            p_dvb->p_demux_handles[i].i_type = i_type;
            p_dvb->p_demux_handles[i].i_pid = i_pid;
            break;
        }
    }
}

/*****************************************************************************
 * CloseProgram:
 *****************************************************************************/
static void CloseProgram( input_thread_t * p_input )
{
    thread_dvb_data_t * p_dvb = (thread_dvb_data_t *)p_input->p_access_data;
    int                 i;

    for ( i = 1; i < MAX_DEMUX; i++ )
    {
        if ( p_dvb->p_demux_handles[i].i_type )
        {
            E_(DMXUnsetFilter)( p_input, p_dvb->p_demux_handles[i].i_handle );
            p_dvb->p_demux_handles[i].i_type = 0;
        }
    }
}


