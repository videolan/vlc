/*****************************************************************************
 * intf_cmd.c: interface commands parsing and executions functions
 * This file implements the interface commands execution functions. It is used
 * by command-line oriented interfaces and scripts. The commands themselves are
 * implemented in intf_ctrl.
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <errno.h>                                                  /* errno */
#include <stdio.h>                                                   /* FILE */
#include <stdlib.h>                                    /* strtod(), strtol() */
#include <string.h>                                            /* strerror() */

#include "threads.h"
#include "config.h"
#include "common.h"
#include "mtime.h"
#include "plugins.h"

#include "interface.h"
#include "intf_msg.h"
#include "intf_cmd.h"
#include "intf_ctrl.h"
#include "main.h"

/*
 * Local prototypes
 */
static int  ParseCommandArguments   ( char *psz_argv[INTF_MAX_ARGS], char *psz_cmd );
static int  CheckCommandArguments   ( intf_arg_t argv[INTF_MAX_ARGS], int i_argc,
                                      char *psz_argv[INTF_MAX_ARGS], char *psz_format );
static void ParseFormatString       ( intf_arg_t format[INTF_MAX_ARGS], char *psz_format );
static int  ConvertArgument         ( intf_arg_t *p_arg, int i_flags, char *psz_str );

/*****************************************************************************
 * intf_ExecCommand: parse and execute a command
 *****************************************************************************
 * This function is called when a command needs to be executed. It parse the
 * command line, build an argument array, find the command in control commands
 * array and run the command. It returns the return value of the command, or
 * EINVAL if no command could be executed. Command line is modified by this
 * function.
 * Note that this function may terminate abruptly the program or signify it's
 * end to the interface thread.
 *****************************************************************************/
int intf_ExecCommand( char *psz_cmd )
{
    char *          psz_argv[INTF_MAX_ARGS];           /* arguments pointers */
    intf_arg_t      argv[INTF_MAX_ARGS];              /* converted arguments */
    int             i_argc;                           /* number of arguments */
    int             i_index;                         /* multi-purposes index */
    int             i_return;                        /* command return value */

    intf_DbgMsg("command `%s'\n", psz_cmd);

    /* Parse command line (separate arguments). If nothing has been found,
     * the function returns without error */
    i_argc = ParseCommandArguments( psz_argv, psz_cmd );
    if( !i_argc )
    {
        return( 0 );
    }

    /* Find command. Command is always the first token on the line */
    for( i_index = 0;
         control_command[i_index].psz_name && strcmp( psz_argv[0], control_command[i_index].psz_name );
         i_index++ )
    {
        ;
    }
    if( !control_command[i_index].psz_name )              /* unknown command */
    {
        /* Print error message */
        intf_IntfMsg( "error: unknown command `%s'. Try `help'", psz_argv[0] );
        return( INTF_USAGE_ERROR );
    }

    /* Check arguments validity */
    if( CheckCommandArguments( argv, i_argc, psz_argv, control_command[i_index].psz_format ) )
    {
        /* The given arguments does not match the format string. An error message has
         * already been displayed, so only the usage string is printed */
        intf_IntfMsg( "usage: %s", control_command[i_index].psz_usage );
        return( INTF_USAGE_ERROR );
    }

    /* Execute command */
    i_return = control_command[i_index].function( i_argc, argv );

    /* Manage special error codes */
    switch( i_return )
    {
    case INTF_FATAL_ERROR:                                    /* fatal error */
        /* Print message and terminates the interface thread */
        intf_ErrMsg( "fatal error in command `%s'\n", psz_argv[0] );
        p_main->p_intf->b_die = 1;
        break;

    case INTF_CRITICAL_ERROR:                              /* critical error */
        /* Print message, flush messages queue and exit. Note that this
         * error should be very rare since it does not even try to cancel other
         * threads... */
        intf_ErrMsg("critical error in command `%s'. Please report this error !\n", psz_argv[0] );
        intf_FlushMsg();
        exit( INTF_CRITICAL_ERROR );
        break;

    case INTF_USAGE_ERROR:                                    /* usage error */
        /* Print error message and usage */
        intf_IntfMsg( "usage: %s", control_command[i_index].psz_usage );
        break;
    }

    /* Return error code */
    return( i_return );
}

/*****************************************************************************
 * intf_ExecScript: parse and execute a command script
 *****************************************************************************
 * This function, based on ExecCommand read a file and tries to execute each
 * of its line as a command. It returns 0 if everything succeeded, a negative
 * number if the script could not be executed and a positive one if an error
 * occured during execution.
 *****************************************************************************/
int intf_ExecScript( char *psz_filename )
{
    FILE *  p_file;                                                  /* file */
    char    psz_line[INTF_MAX_CMD_SIZE];                             /* line */
    char *  psz_index;                                    /* index in string */
    int     i_err;                                        /* error indicator */

    /* Open file */
    i_err = 0;
    p_file = fopen( psz_filename, "r" );
    if( p_file == NULL )
    {
        intf_ErrMsg("warning: %s: %s\n", psz_filename, strerror(errno));
        return( -1 );
    }

    /* For each line: read and execute */
    while( fgets( psz_line, INTF_MAX_CMD_SIZE, p_file ) != NULL )
    {
        /* If line begins with a '#', it is a comment and shoule be ignored,
         * else, execute it */
        if( psz_line[0] != '#' )
        {
            /* The final '\n' needs to be removed before execution */
            for( psz_index = psz_line; *psz_index && (*psz_index != '\n'); psz_index++ )
            {
                ;
            }
            if( *psz_index == '\n' )
            {
                *psz_index = '\0';
            }

            /* Execute command */
            i_err |= intf_ExecCommand( psz_line );
        }
    }
    if( !feof( p_file ) )
    {
        intf_ErrMsg("error: %s: %s\n", psz_filename, strerror(errno));
        return( -1 );
    }

    /* Close file */
    fclose( p_file );
    return( i_err != 0 );
}

/* following functions are local */

/*****************************************************************************
 * ParseCommandArguments: isolate arguments in a command line
 *****************************************************************************
 * This function modify the original command line, adding '\0' and completes
 * an array of pointers to beginning of arguments. It return the number of
 * arguments.
 *****************************************************************************/
static int ParseCommandArguments( char *psz_argv[INTF_MAX_ARGS], char *psz_cmd )
{
    int         i_argc;                               /* number of arguments */
    char *      psz_index;                                          /* index */
    boolean_t   b_block;                       /* block (argument) indicator */

    /* Initialize parser state */
    b_block = 0;   /* we start outside a block to remove spaces at beginning */
    i_argc = 0;

    /* Go through command until end has been reached or maximal number of
     * arguments has been reached */
    for( psz_index = psz_cmd; *psz_index && (i_argc < INTF_MAX_ARGS); psz_index++ )
    {
        /* Inside a block, end of blocks are marked by spaces */
        if( b_block )
        {
            if( *psz_index == ' ' )
            {
                *psz_index = '\0';               /* mark the end of argument */
                b_block = 0;                               /* exit the block */
            }

        }
        /* Outside a block, beginning of blocks are marked by any character
         * different from space */
        else
        {
            if( *psz_index != ' ' )
            {
                psz_argv[i_argc++] = psz_index;            /* store argument */
                b_block = 1;                              /* enter the block */
            }
        }
    }

    /* Return number of arguments found */
    return( i_argc );
}

/*****************************************************************************
 * CheckCommandArguments: check arguments agains format
 *****************************************************************************
 * This function parse each argument and tries to find a match in the format
 * string. It fills the argv array.
 * If all arguments have been sucessfuly identified and converted, it returns
 * 0, else, an error message is issued and non 0 is returned.
 * Note that no memory is allocated by this function, but that the arguments
 * can be modified.
 *****************************************************************************/
static int CheckCommandArguments( intf_arg_t argv[INTF_MAX_ARGS], int i_argc,
                                  char *psz_argv[INTF_MAX_ARGS], char *psz_format )
{
    intf_arg_t  format[INTF_MAX_ARGS];           /* parsed format indicators */
    int         i_arg;                                     /* argument index */
    int         i_format;                                    /* format index */
    char *      psz_index;                                   /* string index */
    char *      psz_cmp_index;                   /* string comparaison index */
    int         i_index;                                    /* generic index */
    boolean_t   b_found;                            /* `argument found' flag */


    /* Build format array */
    ParseFormatString( format, psz_format );

    /* Initialize parser: i_format must be the first non named formatter */
    for( i_format = 0; ( i_format < INTF_MAX_ARGS )
             && (format[i_format].i_flags & INTF_NAMED_ARG);
         i_format++ )
    {
        ;
    }

    /* Scan all arguments */
    for( i_arg = 1; i_arg < i_argc; i_arg++ )
    {
        b_found = 0;

        /* Test if argument can be taken as a named argument: try to find a
         * '=' in the string */
        for( psz_index = psz_argv[i_arg]; *psz_index && ( *psz_index != '=' ); psz_index++ )
        {
            ;
        }
        if( *psz_index == '=' )                                 /* '=' found */
        {
            /* Browse all named arguments to check if there is one matching */
            for( i_index = 0; (i_index < INTF_MAX_ARGS)
                     && ( format[i_index].i_flags & INTF_NAMED_ARG )
                     && !b_found;
                 i_index++ )
            {
                /* Current format string is named... compare start of two
                 * names. A local inline ntation of a strcmp is used since
                 * string isn't ended by '\0' but by '=' */
                for( psz_index = psz_argv[i_arg], psz_cmp_index = format[i_index].ps_name;
                     (*psz_index == *psz_cmp_index) && (*psz_index != '=') && (*psz_cmp_index != '=');
                     psz_index++, psz_cmp_index++ )
                {
                    ;
                }
                if( *psz_index == *psz_cmp_index )        /* the names match */
                {
                    /* The argument is a named argument which name match the
                     * named argument i_index. To be valid, the argument should
                     * not have been already encountered and the type must
                     * match. Before going further, the '=' is replaced by
                     * a '\0'. */
                    *psz_index = '\0';

                    /* Check unicity. If the argument has already been encountered,
                     * print an error message and return. */
                    if( format[i_index].i_flags & INTF_PRESENT_ARG )/* present */
                    {
                        intf_IntfMsg("error: `%s' has already been encountered", psz_argv[i_arg] );
                        return( 1 );
                    }

                     /* Register argument and prepare exit */
                    b_found = 1;
                    format[i_index].i_flags |= INTF_PRESENT_ARG;
                    argv[i_arg].i_flags = INTF_NAMED_ARG;
                    argv[i_arg].i_index = i_index;
                    argv[i_arg].ps_name = psz_argv[i_arg];

                    /* Check type and store value */
                    psz_index++;
                    if( ConvertArgument( &argv[i_arg], format[i_index].i_flags, psz_index ) )
                    {
                        /* An error occured during conversion */
                        intf_IntfMsg( "error: invalid type for `%s'", psz_index );
                    }
                }
            }
        }

        /* If argument is not a named argument, the format string will
         * be browsed starting from last position until the argument is
         * found or an error occurs. */
        if( !b_found )
        {
            /* Reset type indicator */
            argv[i_arg].i_flags = 0;

            /* If argument is not a named argument, the format string will
             * be browsed starting from last position until the argument is
             * found, an error occurs or the last format argument is
             * reached */
            while( !b_found && (i_format < INTF_MAX_ARGS) && format[i_format].i_flags )
            {
                /* Try to convert argument */
                if( !ConvertArgument( &argv[i_arg], format[i_format].i_flags, psz_argv[i_arg] ) )
                {
                    /* Matching format has been found */
                    b_found = 1;
                    format[i_format].i_flags |= INTF_PRESENT_ARG;
                    argv[i_arg].i_index = i_format;

                    /* If argument is repeatable, dot not increase format counter */
                    if( !(format[i_format].i_flags & INTF_REP_ARG) )
                    {
                        i_format++;
                    }
                }
                else
                {
                    /* Argument does not match format. This can be an error, or
                     * just a missing optionnal parameter, or the end of a
                     * repeated argument */
                    if( (format[i_format].i_flags & INTF_OPT_ARG)
                        || (format[i_format].i_flags & INTF_PRESENT_ARG) )
                    {
                        /* This is not an error */
                        i_format++;
                    }
                    else
                    {
                        /* The present format argument is mandatory and does
                         * not match the argument */
                        intf_IntfMsg("error: missing argument before `%s'", psz_argv[i_arg] );
                        return( 1 );
                    }
                }
            }
        }

        /* If argument is not a named argument and hasn't been found in
         * format string, then it is an usage error and the function can
         * return */
        if( !b_found )
        {
            intf_IntfMsg("error: `%s' does not match any argument", psz_argv[i_arg] );
            return( 1 );
        }

        intf_DbgMsg("argument flags=0x%x (index=%d) name=%s str=%s int=%d float=%f\n",
                    argv[i_arg].i_flags,
                    argv[i_arg].i_index,
                    (argv[i_arg].i_flags & INTF_NAMED_ARG) ? argv[i_arg].ps_name : "NA",
                    (argv[i_arg].i_flags & INTF_STR_ARG) ? argv[i_arg].psz_str : "NA",
                    (argv[i_arg].i_flags & INTF_INT_ARG) ? argv[i_arg].i_num : 0,
                    (argv[i_arg].i_flags & INTF_FLOAT_ARG) ? argv[i_arg].f_num : 0);
    }

    /* Parse all remaining format specifier to verify they are all optionnal */
    for( ;  (i_format < INTF_MAX_ARGS) && format[i_format].i_flags ; i_format++ )
    {
        if( !(( format[i_format].i_flags & INTF_OPT_ARG)
              || ( format[i_format].i_flags & INTF_PRESENT_ARG)) )
        {
            /* Format has not been used and is neither optionnal nor multiple
             * and present */
            intf_IntfMsg("error: missing argument(s)\n");
            return( 1 );
        }
    }

    /* If an error occured, the function already exited, so if this point is
     * reached, everything is fine */
    return( 0 );
}

/*****************************************************************************
 * ConvertArgument: try to convert an argument to a given type
 *****************************************************************************
 * This function tries to convert the string argument given in psz_str to
 * a type specified in i_flags. It updates p_arg and returns O on success,
 * or 1 on error. No error message is issued.
 *****************************************************************************/
static int ConvertArgument( intf_arg_t *p_arg, int i_flags, char *psz_str )
{
    char *psz_end;                   /* end pointer for conversion functions */

    if( i_flags & INTF_STR_ARG )                                   /* string */
    {
        /* A conversion from a string to a string will always succeed... */
        p_arg->psz_str = psz_str;
        p_arg->i_flags |= INTF_STR_ARG;
    }
    else if( i_flags & INTF_INT_ARG )                             /* integer */
    {
        p_arg->i_num = strtol( psz_str, &psz_end, 0 );     /* convert string */
        /* If the conversion failed, return 1 and do not modify argument
         * flags. Else, add 'int' flag and continue. */
        if( !*psz_str || *psz_end )
        {
            return( 1 );
        }
        p_arg->i_flags |= INTF_INT_ARG;
    }
    else if( i_flags & INTF_FLOAT_ARG )                             /* float */
    {
        p_arg->f_num = strtod( psz_str, &psz_end );        /* convert string */
        /* If the conversion failed, return 1 and do not modify argument
         * flags. Else, add 'float' flag and continue. */
        if( !*psz_str || *psz_end )
        {
            return( 1 );
        }
        p_arg->i_flags |= INTF_FLOAT_ARG;
    }
#ifdef DEBUG
    else                                    /* error: missing type specifier */
    {
        intf_ErrMsg("error: missing type specifier for `%s' (0x%x)\n", psz_str, i_flags);
        return( 1 );
    }
#endif

    return( 0 );
}

/*****************************************************************************
 * ParseFormatString: parse a format string                              (ok ?)
 *****************************************************************************
 * This function read a format string, as specified in the control_command
 * array, and fill a format array, to allow easier argument identification.
 * Note that no memory is allocated by this function, but that, in a named
 * argument, the name field does not end with a '\0' but with an '='.
 * See command.h for format string specifications.
 * Note that this function is designed to be efficient, not to check everything
 * in a format string, which should be entered by a developper and therefore
 * should be correct (TRUST !).
 *****************************************************************************/
static void ParseFormatString( intf_arg_t format[INTF_MAX_ARGS], char *psz_format )
{
    char *  psz_index;                                /* format string index */
    char *  psz_start;                              /* argument format start */
    char *  psz_item;                                          /* item index */
    int     i_index;                                         /* format index */

    /* Initialize parser */
    i_index = 0;
    psz_start = psz_format;

    /* Reset first format indicator */
    format[ 0 ].i_flags = 0;

    /* Parse format string */
    for( psz_index = psz_format; *psz_index && (i_index < INTF_MAX_ARGS) ; psz_index++ )
    {
        /* A space is always an item terminator */
        if( *psz_index == ' ' )
        {
            /* Parse format item. Items are parsed from end to beginning or to
             * first '=' */
            for( psz_item = psz_index - 1;
                 (psz_item >= psz_start) && !( format[i_index].i_flags & INTF_NAMED_ARG);
                 psz_item-- )
            {
                switch( *psz_item )
                {
                case 's':                                          /* string */
                    format[i_index].i_flags |= INTF_STR_ARG;
                    break;
                case 'i':                                         /* integer */
                    format[i_index].i_flags |= INTF_INT_ARG;
                    break;
                case 'f':                                           /* float */
                    format[i_index].i_flags |= INTF_FLOAT_ARG;
                    break;
                case '*':                                 /* can be repeated */
                    format[i_index].i_flags |= INTF_REP_ARG;
                    break;
                case '?':                              /* optionnal argument */
                    format[i_index].i_flags |= INTF_OPT_ARG;
                    break;
                case '=':                                   /* name argument */
                    format[i_index].i_flags |= INTF_NAMED_ARG;
                    format[i_index].ps_name = psz_start;
                    break;
#ifdef DEBUG
                default:/* error which should never happen: incorrect format */
                    intf_DbgMsg("error: incorrect format string `%s'\n", psz_format);
                    break;
#endif
                }
            }

            /* Mark next item start, increase items counter and reset next
             * format indicator, if it wasn't the last one. */
            i_index++;
            psz_start = psz_index + 1;
            if( i_index != INTF_MAX_ARGS )       /* end of array not reached */
            {
                format[ i_index ].i_flags = 0;
            }
        }
    }
}
