/*******************************************************************************
 * main.c: main vlc source
 * (c)1998 VideoLAN
 *******************************************************************************
 * Includes the main() function for vlc. Parses command line, start interface
 * and spawn threads.
 *******************************************************************************/

/*******************************************************************************
 * Preamble
 *******************************************************************************/
#include <errno.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/soundcard.h>
#include <sys/uio.h>
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>

#include "config.h"
#include "common.h"
#include "mtime.h"
#include "netutils.h"

#include "input.h"
#include "input_vlan.h"
#include "decoder_fifo.h"

#include "audio_output.h"
#include "audio_decoder.h"

#include "video.h"
#include "video_output.h"
#include "video_decoder.h"

#include "xconsole.h"
#include "interface.h"
#include "intf_msg.h"

#include "pgm_data.h"



/*
 * Command line options constants. If something is changed here, be sure that
 * GetConfiguration and Usage are also changed.
 */

/* Long options return values - note that values corresponding to short options
 * chars, and in general any regular char, should be avoided */
#define OPT_DISPLAY             130

#define OPT_CONSOLE_DISPLAY     140
#define OPT_CONSOLE_GEOMETRY    141

#define OPT_NOAUDIO             150
#define OPT_STEREO              151
#define OPT_MONO                152
#define OPT_RATE                153

#define OPT_NOVIDEO             160
#define OPT_XSHM                161
#define OPT_NOXSHM              162

#define OPT_NOVLANS             170
#define OPT_VLAN_SERVER         171
 
/* Long options */
static const struct option longopts[] =
{   
    /*  name,               has_arg,    flag,   val */     

    /* General/common options */
    {   "help",             0,          0,      'h' },          
    {   "display",          1,          0,      OPT_DISPLAY },

    /* Interface options */
    {   "console-display",  1,          0,      OPT_CONSOLE_DISPLAY }, 
    {   "console-geometry", 1,          0,      OPT_CONSOLE_GEOMETRY },

    /* Audio options */
    {   "noaudio",          0,          0,      OPT_NOAUDIO },       
    {   "stereo",           0,          0,      OPT_STEREO },
    {   "mono",             0,          0,      OPT_MONO },      
    {   "rate",             0,          0,      OPT_RATE },

    /* Video options */
    {   "novideo",          0,          0,      OPT_NOVIDEO },           
    {   "xshm",             0,          0,      OPT_XSHM },
    {   "noxshm",           0,          0,      OPT_NOXSHM },    

    /* VLAN management options */
    {   "novlans",          0,          0,      OPT_NOVLANS },
    {   "vlanserver",       1,          0,      OPT_VLAN_SERVER },                    

    {   0,                  0,          0,      0 }
};

/* Short options */
static const char *psz_shortopts = "h";

/*
 * Global variable program_data
 */
program_data_t *p_program_data;                              /* see pgm_data.h */

/*
 * Local prototypes
 */
static void SetDefaultConfiguration ( program_data_t *p_data );
static int  GetConfiguration        ( program_data_t *p_config, int i_argc, char *ppsz_argv[],
                                      char *ppsz_env[] );
static void Usage                   ( void );
static long GetIntParam             ( const char *psz_param, long i_min, long i_max, int *pi_err );
static void InitSignalHandler       ( void );
static void SignalHandler           ( int i_signal );

/*******************************************************************************
 * main: parse command line, start interface and spawn threads
 *******************************************************************************
 * Steps during program execution are:
 *      -configuration parsing and messages interface initialization
 *      -openning of audio output device
 *      -execution of interface, which exit on error or on user request
 *      -closing of audio output device
 * On error, the spawned threads are cancelled, and the openned devices closed.
 *******************************************************************************
 * ?? Signal handlers should be restored to default once the interface has
 * been closed, since they will cause a crash if the message interface is no
 * more active.
 *******************************************************************************/
int main( int i_argc, char *ppsz_argv[], char *ppsz_env[] )
{
    program_data_t  program_data;         /* root of all data - see pgm_data.h */
    
    /*
     * Read configuration, initialize messages interface and set up program
     */
    p_program_data = &program_data;              /* set up the global variable */
    if( intf_InitMsg( &program_data.intf_msg ) )   /* start messages interface */
    {
        fprintf(stderr, "intf critical: can't initialize messages interface (%s)\n",
                strerror(errno));
        return(errno);
    }
    if( GetConfiguration( &program_data,                 /* parse command line */
                          i_argc, ppsz_argv, ppsz_env ) )
    {
        intf_TerminateMsg( &program_data.intf_msg );
        return(errno);
    }
    intf_MsgImm( COPYRIGHT_MESSAGE );                 /* print welcome message */
    InitSignalHandler();                   /* prepare signals for interception */    

    /*
     * Initialize shared resources and libraries
     */
    if( program_data.cfg.b_vlans 
         && input_VlanMethodInit( &program_data.input_vlan_method, 
                                  program_data.cfg.psz_input_vlan_server,
                                  program_data.cfg.i_input_vlan_server_port) )
    {
        /* On error during vlans initialization, switch of vlans */
        intf_Msg("intf: switching off vlans\n");
        program_data.cfg.b_vlans = 0;
    }
    
    /*
     * Open audio device
     */
    if( program_data.cfg.b_audio )
    {                                                
        if( aout_Open( &program_data.aout_thread ) )
        {
            /* On error during audio initialization, switch of audio */
            intf_Msg("intf: switching off audio\n");
            program_data.cfg.b_audio = 0;
        }
        else if( aout_SpawnThread( &program_data.aout_thread ) )
        {
            aout_Close( &program_data.aout_thread );
            input_VlanMethodFree( &program_data.input_vlan_method );            
            intf_TerminateMsg( &program_data.intf_msg );
            return( -1 );
        }
        program_data.intf_thread.p_aout = &program_data.aout_thread;
    }
    
    /*
     * Run interface
     */
    intf_DbgMsg("intf debug: starting interface\n");
    intf_Run( &program_data.intf_thread );
    intf_DbgMsg("intf debug: interface terminated\n");

    /* 
     * Close audio device
     */
    if( program_data.cfg.b_audio )                 
    {
        aout_CancelThread( &program_data.aout_thread );
        aout_Close( &program_data.aout_thread );
    }    

    /*
     * Free shared resources and libraries
     */
    if( program_data.cfg.b_vlans )
    {        
        input_VlanMethodFree( &program_data.input_vlan_method );    
    }    

    /*
     * Terminate messages interface and program
     */
    intf_Msg( "program terminated.\n" );
    intf_TerminateMsg( &program_data.intf_msg );
    return( 0 );
}

/* following functions are local */

/*******************************************************************************
 * SetDefaultConfiguration: set default options
 *******************************************************************************
 * This function is called by GetConfiguration before command line is parsed.
 * It sets all the default values required later by the program. Note that
 * all properties must be initialized, wether they are command-line dependant
 * or not.
 *******************************************************************************/
static void SetDefaultConfiguration( program_data_t *p_data )
{
    /*
     * Audio output thread configuration 
     */
    p_data->cfg.b_audio = 1;                 /* audio is activated by default */
    p_data->aout_thread.dsp.psz_device = AOUT_DEFAULT_DEVICE;
    /* je rajouterai la détection des formats supportés quand le reste
     * marchera */
    p_data->aout_thread.dsp.i_format = AOUT_DEFAULT_FORMAT;
    p_data->aout_thread.dsp.b_stereo = AOUT_DEFAULT_STEREO;
    p_data->aout_thread.dsp.l_rate = AOUT_DEFAULT_RATE;

    /*
     * Interface thread configuration
     */
    /* X11 console */
    p_data->intf_thread.xconsole.psz_display =	NULL;
    p_data->intf_thread.xconsole.psz_geometry =	INTF_XCONSOLE_GEOMETRY;

    /* --- ?? following are ok */

    /*
     * Video output thread configuration
     */
    p_data->cfg.b_video =                   1;    
    p_data->vout_cfg.i_properties =         0;

    /* VLAN management */
    p_data->cfg.b_vlans =                   1;    
    p_data->cfg.psz_input_vlan_server =     VLAN_DEFAULT_SERVER;
    p_data->cfg.i_input_vlan_server_port =  VLAN_DEFAULT_SERVER_PORT;    
}

/*******************************************************************************
 * GetConfiguration: parse command line
 *******************************************************************************
 * Parse command line and configuration file for configuration. If the inline
 * help is requested, the function Usage() is called and the function returns
 * -1 (causing main() to exit). Note than messages interface is initialized at
 * this stage.
 *******************************************************************************/
static int GetConfiguration( program_data_t *p_data, int i_argc, 
                             char *ppsz_argv[], char *ppsz_env[] )
{
    int c, i_err;

    /* Set default configuration and copy arguments */
    p_data->i_argc = i_argc;
    p_data->ppsz_argv = ppsz_argv;
    p_data->ppsz_env = ppsz_env;    
    SetDefaultConfiguration( p_data );

    /* Parse command line */
    opterr = 0;
    while( ( c = getopt_long( i_argc, ppsz_argv, psz_shortopts, longopts, 0 ) ) != EOF )
    {
        switch( c )
        {
        /* General/common options */   
        case 'h':                                                /* -h, --help */
            Usage();
            return( -1 );
            break;
        case OPT_DISPLAY:                                         /* --display */
            p_data->vout_cfg.psz_display = optarg;
            p_data->vout_cfg.i_properties |= VIDEO_CFG_DISPLAY;
            p_data->intf_thread.xconsole.psz_display = optarg;
            break;

        /* Interface options */
        case OPT_CONSOLE_DISPLAY:                         /* --console-display */
            p_data->intf_thread.xconsole.psz_display = optarg;
            break;
        case OPT_CONSOLE_GEOMETRY:                       /* --console-geometry */
            p_data->intf_thread.xconsole.psz_geometry = optarg;
            break;

        /* Audio options */
        case OPT_NOAUDIO:                                        /* --noaudio */
	    p_data->cfg.b_audio = 0;
            break;
        case OPT_STEREO:                                          /* --stereo */
	    p_data->aout_thread.dsp.b_stereo = 1;
            break;
        case OPT_MONO:                                              /* --mono */
	    p_data->aout_thread.dsp.b_stereo = 0;
            break;
        case OPT_RATE:                                              /* --rate */
	    p_data->aout_thread.dsp.l_rate = GetIntParam(optarg, AOUT_MIN_RATE, AOUT_MAX_RATE, &i_err );
            if( i_err )
            {
                return( EINVAL );
	    }
            break;

        /* Video options */
        case OPT_NOVIDEO:                                         /* --novideo */
            p_data->cfg.b_video = 0;
            break;       
        case OPT_XSHM:                                               /* --xshm */
            p_data->vout_cfg.b_shm_ext = 1;
            p_data->vout_cfg.i_properties |= VIDEO_CFG_SHM_EXT;
            break;
        case OPT_NOXSHM:                                           /* --noxshm */
            p_data->vout_cfg.b_shm_ext = 0;
            p_data->vout_cfg.i_properties |= VIDEO_CFG_SHM_EXT;
            break;

        /* VLAN management options */
        case OPT_NOVLANS:                                         /* --novlans */
            p_data->cfg.b_vlans = 0;
            break;
        case  OPT_VLAN_SERVER:                                 /* --vlanserver */
            p_data->cfg.i_input_vlan_server_port = ServerPort( optarg );
            p_data->cfg.psz_input_vlan_server = optarg;
            break;
	    
        /* Internal error: unknown option */
        case '?':                          
        default:
            intf_ErrMsg("intf error: unknown option '%s'\n", ppsz_argv[optind - 1]);
            return( EINVAL );
            break;
        }
    }

    return( 0 );
}

/*******************************************************************************
 * Usage: print program usage
 *******************************************************************************
 * Print a short inline help. Message interface is initialized at this stage.
 *******************************************************************************/
static void Usage( void )
{
    intf_Msg(COPYRIGHT_MESSAGE);
    /* General options */
    intf_Msg("usage: vlc [options...]\n" \
             "  -h, --help                      print usage\n" \
             );
    /* Audio options */
    intf_Msg("  --noaudio                       disable audio\n" \
             "  --stereo                        enable stereo\n" \
             "  --mono                          disable stereo\n"
             "  --rate <rate>                   audio output rate (kHz)\n" \
             );
    /* Video options */
    intf_Msg("  --novideo                       disable video\n" \
             "  --xshm, --noxshm                enable/disable use of XShm extension\n" \
             "  -d, --display <display>         set display name\n" \
             );

    /* VLAN management options */
    intf_Msg("  --novlans		                disable vlans\n" \
             "  --vlanserver <server[:port]>	set vlan server address\n" \
             );
}

/*******************************************************************************
 * GetIntParam: convert a string to an integer
 *******************************************************************************
 * This function convert a string to an integer, check range of the value
 * and that there is no error during convertion. pi_err is a pointer to an
 * error flag, which contains non 0 on error. This function prints its own
 * error messages.
 *******************************************************************************/
static long GetIntParam( const char *psz_param, long i_min, long i_max, int *pi_err )
{
    char *psz_endptr;
    long int i_value;

    i_value = strtol( psz_param, &psz_endptr, 0 );
    if( (psz_param[0] == '\0') && (*psz_endptr != '\0') )  /* conversion error */
    {
        intf_ErrMsg("intf error: conversion error ('%s' should be an integer between %ld and %ld)\n",
                    psz_param, i_min, i_max );
        *pi_err = EINVAL;
        return( 0 );
    }
    if( (i_value < i_min) || (i_value > i_max) )                /* range error */
    {
        intf_ErrMsg("intf error: range error ('%s' should be an integer between %ld and %ld)\n",
                    psz_param, i_min, i_max );
        *pi_err = EINVAL;
        return( 0 );
    }
    *pi_err = 0;
    return( i_value );
}
/*******************************************************************************
 * InitSignalHandler: system signal handler initialization
 *******************************************************************************
 * Set the signal handlers. SIGTERM is not intercepted, because we need at
 * at least a method to kill the program when all other methods failed, and 
 * when we don't want to use SIGKILL.
 *******************************************************************************/
static void InitSignalHandler( void )
{
  /* Termination signals */
  signal( SIGHUP, SignalHandler );
  signal( SIGINT, SignalHandler );
  signal( SIGQUIT, SignalHandler );
}

/*******************************************************************************
 * SignalHandler: system signal handler
 *******************************************************************************
 * This function is called when a signal is received by the program. It ignores
 * it or tries to terminate cleanly.
 *******************************************************************************/
static void SignalHandler( int i_signal )
{
  /* Once a signal has been trapped, the signal handler needs to be re-armed on
   * linux systems */
  signal( i_signal, SignalHandler );

  /* Acknowledge the signal received */
  intf_ErrMsgImm("intf: signal %d received\n", i_signal );

  /* Try to terminate everything */
  /* ?? this probably needs to be changed */
 /*  p_program_data->intf_thread.b_die = 1; */
}
