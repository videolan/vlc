/*****************************************************************************
 * intf_ctrl.c: interface commands access to control functions
 * Library of functions common to all interfaces, allowing access to various
 * structures and settings. Interfaces should only use those functions
 * to read or write informations from other threads.
 * A control function must be declared in the `local prototypes' section (it's
 * type is fixed), and copied into the control_command array. Functions should
 * be listed in alphabetical order, so when `help' is called they are also
 * displayed in this order.
 * A control function can use any function of the program, but should respect
 * two points: first, it should not block, since if it does so, the whole
 * interface thread will hang and in particular miscelannous interface events
 * won't be handled. Secondly, it should send it's output messages exclusively
 * with intf_IntfMsg() function, except particularly critical messages which
 * can use over intf_*Msg() functions.
 * Control functions should return 0 (INTF_NO_ERROR) on success, or one of the
 * error codes defined in command.h. Custom error codes are allowed, but should
 * be positive.
 * More informations about parameters stand in `list of commands' section.
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors:
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
#include "defs.h"

#include <sys/types.h>                        /* on BSD, uio.h needs types.h */
#include <sys/stat.h>                        /* on BSD, fstat() needs stat.h */
#include <sys/uio.h>                                            /* "input.h" */
#include <stdio.h>                                              /* fprintf() */
#include <stdlib.h>                                      /* malloc(), free() */
#include <unistd.h>                                       /* close(), read() */
#include <fcntl.h>                                                 /* open() */

/* Common headers */
#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "debug.h"
#include "plugins.h"
#include "intf_msg.h"

#include "stream_control.h"
#include "input_ext-intf.h"
#include "audio_output.h"
#include "intf_cmd.h"
#include "interface.h"
#include "main.h"

/*
 * Local prototypes
 */
static int Demo                 ( int i_argc, intf_arg_t *p_argv );
static int DisplayImage         ( int i_argc, intf_arg_t *p_argv );
static int Exec                 ( int i_argc, intf_arg_t *p_argv );
static int Help                 ( int i_argc, intf_arg_t *p_argv );
static int PlayAudio            ( int i_argc, intf_arg_t *p_argv );
static int PlayVideo            ( int i_argc, intf_arg_t *p_argv );
static int Quit                 ( int i_argc, intf_arg_t *p_argv );
static int SelectPID            ( int i_argc, intf_arg_t *p_argv );
static int SpawnInput           ( int i_argc, intf_arg_t *p_argv );
#ifdef DEBUG
static int Test                 ( int i_argc, intf_arg_t *p_argv );
#endif
static int Vlan                 ( int i_argc, intf_arg_t *p_argv );
static int Psi                  ( int i_argc, intf_arg_t *p_argv );

/*
 * List of commands.
 * This list is used by intf_ExecCommand function to find functions to
 * execute and prepare its arguments. It is terminated by an element  which name
 * is a null pointer. intf_command_t is defined in command.h.
 *
 * Here is a description of a command description elements:
 *  name is the name of the function, as it should be typed on command line,
 *  function is a pointer to the control function,
 *  format is an argument descriptor (see below),
 *  summary is a text string displayed in regard of the command name when `help'
 *      is called without parameters, and whith usage on syntax error,
 *  usage is a short syntax indicator displayed with summary when the command
 *      causes a syntax error,
 *  help is a complete help about the command, displayed when `help' is called with
 *      the command name as parameter.
 *
 * Format string is a list of ' ' separated strings, which have following
 * meanings:
 *  s       string argument
 *  i       integer argument
 *  f       float argument
 *  ?       optionnal argument
 *  *       argument can be repeated
 *  name=   named argument
 * Example: "channel=i? s*? i " means that any number of string arguments,
 * followed by a single mandatory integer argument are waited. A named argument,
 * which name is `channel' and must have an integer value can be optionnaly
 * specified at beginning. The last space is mandatory if there is at least one
 * element, since it acts as an item terminator.
 * Named arguments MUST be at the beginning of the format string, and in
 * alphabetic order, but their order on command line has no importance.
 * The format string can't have more than INTF_MAX_ARGS elements.
 */
const intf_command_t control_command[] =
{
  { "demo", Demo,                                                    /* demo */
    /* format: */   "",
    /* summary: */  "program demonstration",
    /* usage: */    "demo",
    /* help: */     "Start program capabilities demonstration." },
  { "display", DisplayImage,                                      /* display */
    /* format: */   "s ",
    /* summary: */  "load and display an image",
    /* usage: */    "display <file>",
    /* help: */     "Load and display an image. Image format is automatically "\
    "identified from file name extension." },
  { "exec", Exec,                                                    /* exec */
    /* format: */   "s ",
    /* summary: */  "execute a script file",
    /* usage: */    "exec <file>",
    /* help: */     "Load an execute a script." },
  { "exit", Quit,                                       /* exit (quit alias) */
    /* format: */   "",
    /* summary: */  "quit program",
    /* usage: */    "exit",
    /* help: */     "see `quit'." },
  { "help", Help,                                                    /* help */
    /* format: */   "s? ",
    /* summary: */  "list all functions or print help about a specific function",
    /* usage: */    "help [command]",
    /* help: */     "If called without argument, list all available " \
    " functions.\nIf a command name is provided as argument, displays a short "\
    "inline help about the command.\n" },
  { "play-audio", PlayAudio,                                   /* play-audio */
    /* format: */   "channels=i? rate=i? s ",
    /* summary: */  "play an audio file",
    /* usage: */    "play-audio [channels=1/2] [rate=r] <file>",
    /* help: */     "Load and play an audio file." },
  { "play-video", PlayVideo,                                      /* play-video */
    /* format: */   "s ",
    /* summary: */  "play a video (.vlp) file",
    /* usage: */    "play-video <file>",
    /* help: */     "Load and play a video file." },
  { "quit", Quit,                                                    /* quit */
    /* format: */   "",
    /* summary: */  "quit program",
    /* usage: */    "quit",
    /* help: */     "Terminates the program execution... There is not much to"\
    " say about it !" },
  { "select-pid", SelectPID,                                   /* select-pid */
    /* format: */   "i i ",
    /* summary: */  "spawn a decoder thread for a specified PID",
    /* summary: */  "select-pid <input> <pid>",
    /* help: */     "Spawn a decoder thread for <pid>. The stream will be" \
    " received by <input>." },
  { "spawn-input", SpawnInput,                                /* spawn-input */
    /* format: */   "method=i? filename=s? hostname=s? ip=s? port=i? vlan=i?",
    /* summary: */  "spawn an input thread",
    /* summary: */  "spawn-input [method=<method>]\n" \
    "[filename=<file>|hostname=<hostname>|ip=<ip>]\n" \
    "[port=<port>] [vlan=<vlan>]",
    /* help: */     "Spawn an input thread. Method is 10, 20, 21, 22, 32, "\
    "hostname is the fully-qualified domain name, ip is a dotted-decimal address." },
#ifdef DEBUG
  { "test", Test,                                                    /* test */
    /* format: */   "i? ",
    /* summary: */  "crazy developper's test",
    /* usage: */    "depends on the last coder :-)",
    /* help: */     "`test' works only in DEBUG mode, and is provide for " \
    "developpers as an easy way to test part of their code. If you don't know "\
    "what it should do, just try !" },
#endif
  { "vlan", Vlan,
    /* format: */   "intf=s? s i? ",
    /* summary: */  "vlan operations",
    /* usage: */    "vlan synchro\n" \
    "vlan [intf=<interface>] request\n" \
    "vlan [intf=<interface>] join <vlan>\n" \
    "vlan [intf=<interface>] leave",
    /* help: */     "Perform various operations on vlans. 'synchro' resynchronize " \
    "with the server. 'request' ask which is the current vlan (for the default "\
    "interface or for a given one). 'join' and 'leave' try to change vlan." },
  { "psi", Psi,
    /* format: */   "i ",
    /* summary: */  "Dump PSI tables",
    /* usage: */    "psi <input thread index>",
    /* help: */     "Display the PSI tables on the console. Warning: this is debug" \
    "command, it can leads to pb since locks are not taken yet" },
  { 0, 0, 0, 0, 0 }                                      /* array terminator */
};

/* following functions are local */

/*****************************************************************************
 * Demo: demo
 *****************************************************************************
 * This function is provided to display a demo of program possibilities.
 *****************************************************************************/
static int Demo( int i_argc, intf_arg_t *p_argv )
{
    intf_IntfMsg( COPYRIGHT_MESSAGE );

    return( INTF_NO_ERROR );
}

/*****************************************************************************
 * Exec: execute a script
 *****************************************************************************
 * This function load and execute a script.
 *****************************************************************************/
static int Exec( int i_argc, intf_arg_t *p_argv )
{
    int i_err;                                                 /* error code */

    i_err = intf_ExecScript( p_argv[1].psz_str );
    return( i_err ? INTF_OTHER_ERROR : INTF_NO_ERROR );
}

/*****************************************************************************
 * DisplayImage: load and display an image                               (ok ?)
 *****************************************************************************
 * Try to load an image identified by it's filename and displays it as a still
 * image using interface video heap.
 *****************************************************************************/
static int DisplayImage( int i_argc, intf_arg_t *p_argv )
{
    /* XXX?? */
    return( INTF_NO_ERROR );
}

/*****************************************************************************
 * Help: list all available commands                                     (ok ?)
 *****************************************************************************
 * This function print a list of available commands
 *****************************************************************************/
static int Help( int i_argc, intf_arg_t *p_argv )
{
    int     i_index;                                        /* command index */

   /* If called with an argument: look for the command and display it's help */
    if( i_argc == 2 )
    {
fprintf( stderr, "maxx debug: coin\n" );
        for( i_index = 0; control_command[i_index].psz_name
                 && strcmp( control_command[i_index].psz_name, p_argv[1].psz_str );
             i_index++ )
        {
            ;
        }
fprintf( stderr, "maxx debug: meuh\n" );
        /* Command has been found in list */
        if( control_command[i_index].psz_name )
        {
fprintf( stderr, "maxx debug: meow\n" );
            intf_IntfMsg( control_command[i_index].psz_usage );
fprintf( stderr, "maxx debug: blah\n" );
            intf_IntfMsg( control_command[i_index].psz_help );
fprintf( stderr, "maxx debug: blih\n" );
        }
        /* Command is unknown */
        else
        {
            intf_IntfMsg("help: don't know command `%s'", p_argv[1].psz_str);
            return( INTF_OTHER_ERROR );
        }
    }
    /* If called without argument: print all commands help field */
    else
    {
        for( i_index = 0; control_command[i_index].psz_name; i_index++ )
        {
            intf_IntfMsg( "%s: %s",  control_command[i_index].psz_name,
                          control_command[i_index].psz_summary );
        }
    }

    return( INTF_NO_ERROR );
}

/*****************************************************************************
 * PlayAudio: play an audio file                                         (ok ?)
 *****************************************************************************
 * Play a raw audio file from a file, at a given rate.
 *****************************************************************************/
static int PlayAudio( int i_argc, intf_arg_t *p_argv )
{
    char * psz_file = NULL;              /* name of the audio raw file (s16) */
    int i_fd;      /* file descriptor of the audio file that is to be loaded */
    aout_fifo_t fifo;         /* fifo stores the informations about the file */
    struct stat stat_buffer;      /* needed to find out the size of psz_file */
    int i_arg;                                             /* argument index */

    if ( !p_main->b_audio )                  /* audio is disabled */
    {
        intf_IntfMsg("play-audio error: audio is disabled");
        return( INTF_NO_ERROR );
    }

    /* Set default configuration */
    fifo.i_channels = 1 + ( fifo.b_stereo = AOUT_STEREO_DEFAULT );
    fifo.l_rate = AOUT_RATE_DEFAULT;

    /* The channels and rate parameters are essential ! */
    /* Parse parameters - see command list above */
    for ( i_arg = 1; i_arg < i_argc; i_arg++ )
    {
        switch( p_argv[i_arg].i_index )
        {
        case 0:                                                  /* channels */
            fifo.i_channels = p_argv[i_arg].i_num;
            break;
        case 1:                                                      /* rate */
            fifo.l_rate = p_argv[i_arg].i_num;
            break;
        case 2:                                                  /* filename */
            psz_file = p_argv[i_arg].psz_str;
            break;
        }
    }

    /* Setting up the type of the fifo */
    switch ( fifo.i_channels )
    {
        case 1:
            fifo.i_type = AOUT_INTF_MONO_FIFO;
            break;

        case 2:
            fifo.i_type = AOUT_INTF_STEREO_FIFO;
            break;

        default:
            intf_IntfMsg("play-audio error: channels must be 1 or 2");
            return( INTF_OTHER_ERROR );
    }

    /* Open file */
    i_fd =  open( psz_file, O_RDONLY );
    if ( i_fd < 0 )                                                 /* error */
    {
        intf_IntfMsg("play-audio error: can't open `%s'", psz_file);
        return( INTF_OTHER_ERROR );
    }

    /* Get file size to calculate number of audio units */
    fstat( i_fd, &stat_buffer );
    fifo.l_units = ( long )( stat_buffer.st_size / (sizeof(s16) << fifo.b_stereo) );

    /* Allocate memory, read file and close it */
    if ( (fifo.buffer = malloc(sizeof(s16)*(fifo.l_units << fifo.b_stereo))) == NULL ) /* !! */
    {
        intf_IntfMsg("play-audio error: not enough memory to read `%s'", psz_file );
        close( i_fd );                                         /* close file */
        return( INTF_OTHER_ERROR );
    }
    if ( read(i_fd, fifo.buffer, sizeof(s16)*(fifo.l_units << fifo.b_stereo))
        != sizeof(s16)*(fifo.l_units << fifo.b_stereo) )
    {
        intf_IntfMsg("play-audio error: can't read %s", psz_file);
        free( fifo.buffer );
        close( i_fd );
        return( INTF_OTHER_ERROR );
    }
    close( i_fd );

   /* Now we can work out how many output units we can compute with the fifo */
    fifo.l_units = (long)(((s64)fifo.l_units*(s64)p_main->p_aout->l_rate)/(s64)fifo.l_rate);

    /* Create the fifo */
    if ( aout_CreateFifo(p_main->p_aout, &fifo) == NULL )
    {
        intf_IntfMsg("play-audio error: can't create audio fifo");
        free( fifo.buffer );
        return( INTF_OTHER_ERROR );
    }

    return( INTF_NO_ERROR );
}

/*****************************************************************************
 * PlayVideo: play a video sequence from a file
 *****************************************************************************
 * XXX??
 *****************************************************************************/
static int PlayVideo( int i_argc, intf_arg_t *p_argv )
{
    /* XXX?? */
    return( INTF_NO_ERROR );
}

/*****************************************************************************
 * Quit: quit program                                                    (ok ?)
 *****************************************************************************
 * This function set `die' flag of interface, asking the program to terminate.
 *****************************************************************************/
static int Quit( int i_argc, intf_arg_t *p_argv )
{
    p_main->p_intf->b_die = 1;
    return( INTF_NO_ERROR );
}


/*****************************************************************************
 *
 *****************************************************************************
 *
 *****************************************************************************/
static int SelectPID( int i_argc, intf_arg_t *p_argv )
{
    int i_input = -1, i_pid = -1;
    int i_arg;

    /* Parse parameters - see command list above */
    for ( i_arg = 1; i_arg < i_argc; i_arg++ )
    {
      switch( p_argv[i_arg].i_index )
      {
      case 0:
          /* FIXME: useless ?? */
          i_input = p_argv[i_arg].i_num;
          break;
      case 1:
         i_pid = p_argv[i_arg].i_num;
        break;
      }
    }


    /* Find to which input this command is destinated */
    intf_IntfMsg( "Adding PID %d to input %d", i_pid, i_input );
    //XXX?? input_AddPgrmElem( p_main->p_intf->p_x11->p_input,
    //XXX??                    i_pid );
    return( INTF_NO_ERROR );
}


/*****************************************************************************
 * SpawnInput: spawn an input thread                                     (ok ?)
 *****************************************************************************
 * Spawn an input thread
 *****************************************************************************/
static int SpawnInput( int i_argc, intf_arg_t *p_argv )
{
    /* FIXME */
#if 0

    int                 i_arg;
    int                 i_method = 0;                    /* method parameter */
    char *              p_source = NULL;                 /* source parameter */
    int                 i_port = 0;                        /* port parameter */
    int                 i_vlan_id = 0;                  /* vlan id parameter */

    /* Parse parameters - see command list above */
    for ( i_arg = 1; i_arg < i_argc; i_arg++ )
    {
        switch( p_argv[i_arg].i_index )
        {
        case 0:                                                    /* method */
            i_method = p_argv[i_arg].i_num;
            break;
        case 1:                                    /* filename, hostname, ip */
        case 2:
        case 3:
            p_source = p_argv[i_arg].psz_str;
            break;
        case 4:                                                      /* port */
            i_port = p_argv[i_arg].i_num;
            break;
        case 5:                                                   /* VLAN id */
            i_vlan_id = p_argv[i_arg].i_num;
            break;
        }
    }

    /* Destroy current input, if any */
    if( p_main->p_intf->p_input != NULL )
    {
        input_DestroyThread( p_main->p_intf->p_input, NULL );
    }

    p_main->p_intf->p_input = input_CreateThread( i_method, p_source, i_port, i_vlan_id,
                                                  p_main->p_intf->p_vout, p_main->p_aout,
                                                  NULL );
    return( INTF_NO_ERROR );
#endif
}

/*****************************************************************************
 * Test: test function
 *****************************************************************************
 * This function is provided to test new functions in the program. Fell free
 * to modify !
 * This function is only defined in DEBUG mode.
 *****************************************************************************/
#ifdef DEBUG
static int Test( int i_argc, intf_arg_t *p_argv )
{
    int i_thread;

/*XXX??    if( i_argc == 1 )
    {
        i_thread = intf_CreateVoutThread( &p_main->intf_thread, NULL, -1, -1);
        intf_IntfMsg("return value: %d", i_thread );
    }
    else*/
    {
        i_thread = p_argv[1].i_num;
    //XXX??    intf_DestroyVoutThread( &p_main->intf_thread, i_thread );
    }

    return( INTF_NO_ERROR );
}
#endif

/*****************************************************************************
 * Vlan: vlan operations
 *****************************************************************************
 * This function performs various vlan operations.
 *****************************************************************************/
static int Vlan( int i_argc, intf_arg_t *p_argv  )
{
    int i_command;                                /* command argument number */

    /* Do not try anything if vlans are deactivated */
    if( !p_main->b_vlans )
    {
        intf_IntfMsg("vlans are deactivated");
        return( INTF_OTHER_ERROR );
    }

    /* Look for command in list of arguments - this argument is mandatory and
     * imposed by the calling function */
    for( i_command = 1; p_argv[i_command].i_index == 1; i_command++ )
    {
        ;
    }

    /* Command is 'join' */
    if( !strcmp(p_argv[i_command].psz_str, "join") )
    {
        /* XXX?? */
    }
    /* Command is 'leave' */
    else if( !strcmp(p_argv[i_command].psz_str, "leave") )
    {
        /* XXX?? */
    }
    /* Command is unknown */
    else
    {
        intf_IntfMsg("vlan error: unknown command %s", p_argv[i_command].psz_str );
        return( INTF_USAGE_ERROR );
    }

    return( INTF_NO_ERROR );
}


/*****************************************************************************
 * Psi
 *****************************************************************************
 * This function is provided to display PSI tables.
 *****************************************************************************/
static int Psi( int i_argc, intf_arg_t *p_argv )
{
    int i_index = p_argv[1].i_num;

    intf_IntfMsg("Reading PSI table for input %d", i_index);
    //XXX?? input_PsiRead(p_main->p_intf->p_x11->p_input );
    return( INTF_NO_ERROR );
}
